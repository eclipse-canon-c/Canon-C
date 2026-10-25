#!/usr/bin/env python3
"""
overlap_probe.py: the WP runs of .github/workflows/overlap-probe.yml and
their reading against the committed prediction in docs/deviations.md,
VERIFY-008, "Committed prediction (2026-10-25, the overlap probe)".

QUESTION. mem_regions_overlap compares two pointers with <, which C leaves
undefined unless both point into one object (C11 6.5.8p5). The fix in
tools/probes/overlap/overlap.patch compares the two addresses as uintptr_t
values instead, without wrapping, and states that comparison in the
postcondition. Do WP 29 and the CI's provers prove the patched function,
and how many goals does it have, so that the fix push can set every pin
that inherits memory.h?

    python3 tools/probes/overlap/overlap_probe.py run
        WP as frama-c-memory runs it (Typed+Cast, -wp-rte, -wp-split,
        Alt-Ergo, Z3 and CVC5 at 120 s, its preprocessing), restricted to
        mem_regions_overlap with -wp-fct, on core/memory.h as committed
        (tree C) and as patched in a copy (tree P), each under the default
        machine and under -machdep gcc_x86_32, as frama-c-arena-32 runs;
        and, to count goals only, the whole of core/memory.h on both trees
        with Qed alone. Output: wp_<run>.txt for each run. Exits 1, posting
        the run's last lines as an error, if the patch does not apply or a
        run writes no summary.

    python3 tools/probes/overlap/overlap_probe.py report
        reads that output and prints MATCH or MISS, with its confidence,
        for each predicted part, or NO DATA where a part has nothing to
        read. The log needs a sign-in, so it also posts the verdicts, each
        run's counts and its goals not proved as notices on the run page,
        and everything to the step summary. Exits 1 on a MISS, else 2 on
        NO DATA.

Run from the repository root, with frama-c on PATH.
"""
import os
import re
import shutil
import subprocess
import sys

PATCH = "tools/probes/overlap/overlap.patch"
FCT = "mem_regions_overlap"
CPP = "-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG"
PROVERS = "alt-ergo,z3,cvc5"
PATCHED = os.path.join(os.environ.get("RUNNER_TEMP", "/tmp"), "overlap-patched")
SUMMARY = os.environ.get("GITHUB_STEP_SUMMARY")
LIMIT = 3800        # the runner cuts an annotation's message at 4096

# run name -> (tree, extra frama-c options, function only?, provers)
RUNS = {
    "C64": ("C", [], True, PROVERS),
    "P64": ("P", [], True, PROVERS),
    "C32": ("C", ["-machdep", "gcc_x86_32"], True, PROVERS),
    "P32": ("P", ["-machdep", "gcc_x86_32"], True, PROVERS),
    "Cunit": ("C", [], False, "qed"),
    "Punit": ("P", [], False, "qed"),
}

# The committed prediction (docs/deviations.md, VERIFY-008): the function's
# goals, proved / total, on each tree, as WP 25 counted them locally.
PRED_FN = {"C": (35, 35), "P": (29, 29)}


def esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def patch_tree():
    if os.path.isdir(PATCHED):
        shutil.rmtree(PATCHED)
    shutil.copytree(".", PATCHED, ignore=shutil.ignore_patterns(".git", "wp_*"))
    r = subprocess.run(["git", "apply", os.path.abspath(PATCH)],
                       cwd=PATCHED, capture_output=True, text=True)
    if r.returncode != 0:
        print("::error title=overlap probe::%s" % esc("patch does not apply: " + r.stderr[-LIMIT:]))
        sys.exit(1)


def run_all():
    patch_tree()
    failed = []
    for name, (tree, extra, fn_only, provers) in RUNS.items():
        cmd = ["frama-c"] + extra + ["-wp", "-wp-rte", "-wp-model", "Typed+Cast",
                                     "-wp-prover", provers, "-wp-timeout", "120",
                                     "-wp-split", "-cpp-extra-args=" + CPP]
        if fn_only:
            cmd += ["-wp-fct", FCT]
        cmd += ["core/memory.h"]
        cwd = "." if tree == "C" else PATCHED
        print("== run %s: %s (in %s)" % (name, " ".join(cmd), cwd), flush=True)
        r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, errors="replace")
        out = r.stdout + r.stderr
        with open("wp_%s.txt" % name, "w", encoding="utf-8") as f:
            f.write(out)
            f.write("\n[probe] frama-c exit status: %d\n" % r.returncode)
        print(out[-2000:], flush=True)
        if not re.search(r"^\[wp\] Proved goals:\s+\d+\s*/\s*\d+", out, re.M):
            failed.append(name)
            print("::error title=overlap probe run %s::%s" % (name, esc(out[-LIMIT:])))
    if failed:
        sys.exit(1)


def read_run(name):
    path = "wp_%s.txt" % name
    if not os.path.exists(path):
        return None
    out = open(path, encoding="utf-8", errors="replace").read()
    m = re.search(r"^\[wp\] Proved goals:\s+(\d+)\s*/\s*(\d+)", out, re.M)
    if not m:
        return None
    # WP 29 prints "[wp] [Timeout] typed_...", WP 25 "[wp] [Timeout] Goal typed_..."
    bad = re.findall(r"^\[wp\] \[(Timeout|Unknown|Failed|Failure|Invalid|Stepout|NoResult)\] (?:Goal )?(\S+)", out, re.M)
    return {"proved": int(m.group(1)), "total": int(m.group(2)), "bad": bad}


verdicts = []


def check(conf, label, ok, detail=""):
    v = "NO DATA" if ok is None else ("MATCH" if ok else "MISS")
    verdicts.append((v, conf, label, detail))


def predictions(D):
    for mach in ("64", "32"):
        for tree in ("C", "P"):
            d = D["%s%s" % (tree, mach)]
            p, t = PRED_FN[tree]
            what = "committed" if tree == "C" else "patched"
            machine = "default machine" if mach == "64" else "gcc_x86_32"
            check("high", "%s, %s: every goal of %s proved" % (what, machine, FCT),
                  None if d is None else (d["proved"] == d["total"] and not d["bad"]),
                  "" if d is None else "%d / %d" % (d["proved"], d["total"]))
            check("medium", "%s, %s: %d goals (WP 25's count)" % (what, machine, t),
                  None if d is None else d["total"] == t,
                  "" if d is None else "%d" % d["total"])
    c, p, c64, p64 = D["Cunit"], D["Punit"], D["C64"], D["P64"]
    ok = None
    if None not in (c, p, c64, p64):
        ok = (p["total"] - c["total"]) == (p64["total"] - c64["total"])
    check("high", "memory.h's unit: goals change only in %s (unit delta = the function's delta)" % FCT,
          ok, "" if ok is None else "unit %+d, function %+d" % (p["total"] - c["total"], p64["total"] - c64["total"]))
    check("medium", "memory.h's unit: 6 goals fewer",
          None if None in (c, p) else (p["total"] - c["total"]) == -6,
          "" if None in (c, p) else "%+d" % (p["total"] - c["total"]))


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("run", "report"):
        print(__doc__)
        sys.exit(2)
    if sys.argv[1] == "run":
        run_all()
        return
    D = {name: read_run(name) for name in RUNS}
    predictions(D)
    lines = ["## Overlap probe (VERIFY-008): the prediction against the runs", ""]
    vlines = []
    for v, conf, label, detail in verdicts:
        line = "%s (%s) %s%s" % (v, conf, label, (": " + detail) if detail else "")
        print(line)
        vlines.append(line)
        lines.append("- " + line)
    rlines = []
    for name, d in D.items():
        if d is None:
            line = "%s: no summary" % name
        elif RUNS[name][3] == "qed":
            # Qed alone leaves most goals open; the run counts goals only.
            line = "%s: %d goals (Qed alone, counted only)" % (name, d["total"])
        else:
            line = "%s: %d / %d proved; not proved: %s" % (
                name, d["proved"], d["total"],
                ", ".join("%s %s" % (k, g) for k, g in d["bad"]) or "none")
        print(line)
        rlines.append(line)
    lines += ["", "## Runs", ""] + ["- " + l for l in rlines]
    # The runner shows at most ten annotations of a kind per step: post the
    # verdicts and the runs as one notice each, and each MISS or NO DATA as
    # an error.
    print("::notice title=overlap probe verdicts::%s" % esc("\n".join(vlines)[:LIMIT]))
    print("::notice title=overlap probe runs::%s" % esc("\n".join(rlines)[:LIMIT]))
    for line in vlines:
        if not line.startswith("MATCH"):
            print("::error title=overlap probe::%s" % esc(line[:LIMIT]))
    if SUMMARY:
        with open(SUMMARY, "a", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    vs = [v for v, _, _, _ in verdicts]
    if "MISS" in vs:
        sys.exit(1)
    if "NO DATA" in vs:
        sys.exit(2)


if __name__ == "__main__":
    main()
