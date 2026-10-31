#!/usr/bin/env python3
"""
g13_probe.py — the runs of the g13-probe job of .github/workflows/probes.yml (VERIFY-039 G13)
and their reading against the committed prediction in docs/deviations.md,
VERIFY-039: "G13 — committed prediction for the probe".

    python3 tools/probes/verify-039/g13_probe.py run
        E: Eva on tools/probes/verify-039/p_dangling_calls.c, once for each
           of its nine entry points (-main), each a call, with the bytes of
           a pointer to a freed block, to a function whose body reaches
           memcmp; and on p_dangling_forms.c from its two entry points,
           which call its recommended(). Output: eva_<entry>.txt.
        W: WP on p_dangling_forms.c's seven functions (-wp-fct), as the
           Typed+Cast units run their headers (-wp-rte, -wp-split, Alt-Ergo,
           Z3 and CVC5 at 120 s). Output: wp_forms.txt, and on WP 29 the
           per-goal report of -wp-report-json, wp_forms_goals.json.

    python3 tools/probes/verify-039/g13_probe.py report E|W
        reads that output and prints MATCH or MISS, with its confidence, for
        each predicted part. The log needs a sign-in, so it also posts the
        verdicts, and what each run found, as notices on the run page (each
        under the runner's 4096-character cut), an error annotation for each
        MISS, and everything to the step summary. Exits 1 on a MISS. W's
        goals outside the prediction are measured, not predicted.

Run from the repository root, with frama-c on PATH.
"""
import json
import os
import re
import subprocess
import sys
import time

CALLS = "tools/probes/verify-039/p_dangling_calls.c"
FORMS = "tools/probes/verify-039/p_dangling_forms.c"
CPP = ("-cpp-extra-args=-I core/primitives -I core -I semantics -I . "
       "-DCANON_NO_REQUIRE -DNDEBUG")
# (file, entry point, expected): ("memcmp", header) for memcmp's
# danglingness:s1 invalid at the call in that header; ("callee", f, name, pair)
# for f's precondition name invalid at the call, pair the clause Eva need not
# reach; ("clean",) for every precondition valid.
#
# Run #1 read the contracts before G13's repair: each of the nine calls met
# every precondition of the function it called, and memcmp's danglingness:s1
# was invalid inside, ("memcmp", header) for each. Since the repair each of
# the nine functions requires the quantifier written out, so Eva is expected
# to reject the call at the function's own first such clause.
ENTRIES = [
    (CALLS, "via_mem_compare",
     ("callee", "mem_compare", "no_dangling_a", "no_dangling_b")),
    (CALLS, "via_mem_equal",
     ("callee", "mem_equal", "no_dangling_a", "no_dangling_b")),
    (CALLS, "via_mem_equal_bytes",
     ("callee", "mem_equal_bytes", "no_dangling_a", "no_dangling_b")),
    (CALLS, "via_bytes_equal",
     ("callee", "bytes_equal", "no_dangling_a", "no_dangling_b")),
    (CALLS, "via_str_equal",
     ("callee", "str_equal", "no_dangling_a", "no_dangling_b")),
    (CALLS, "via_str_starts_with",
     ("callee", "str_starts_with", "no_dangling_s", "no_dangling_prefix")),
    (CALLS, "via_str_ends_with",
     ("callee", "str_ends_with", "no_dangling_s", "no_dangling_suffix")),
    (CALLS, "via_borrowed_bytes_eq",
     ("callee", "borrowed_bytes_eq", "no_dangling_a", "no_dangling_b")),
    (CALLS, "via_borrowed_str_eq",
     ("callee", "borrowed_str_eq", "no_dangling_a", "no_dangling_b")),
    (FORMS, "eva_freed", ("callee", "recommended", "da", "db")),
    (FORMS, "eva_live", ("clean",)),
]
SEVEN = "today,libc_pred,quantified,recommended,caller,other,zero"
P = "typed_cast_"
# W: (goal, predicted proved, label)
W_PRED = [
    (P + "libc_pred_ensures_absurd", True,
     "libc_pred: ensures \\false proved from n > 0 and non_escaping"),
    (P + "other_ensures_other_pointer", True,
     "other: non_escaping of another pointer proved from non_escaping"),
    (P + "zero_ensures_holds", True,
     "zero: non_escaping proved from n == 0"),
    (P + "quantified_ensures_absurd", False,
     "quantified: ensures \\false not proved from the quantifier"),
    (P + "today_call_memcmp_requires_danglingness_s1", False,
     "today: memcmp's danglingness:s1 not proved"),
    (P + "today_call_memcmp_requires_danglingness_s2", False,
     "today: memcmp's danglingness:s2 not proved"),
    (P + "recommended_call_memcmp_requires_danglingness_s1", False,
     "recommended: memcmp's danglingness:s1 not proved"),
    (P + "recommended_call_memcmp_requires_danglingness_s2", False,
     "recommended: memcmp's danglingness:s2 not proved"),
    (P + "caller_call_recommended_requires_da", False,
     "caller: recommended's da not proved at the call"),
    (P + "caller_call_recommended_requires_db", False,
     "caller: recommended's db not proved at the call"),
]
NOT_PROVED = "Timeout|Unknown|Failed|Failure|Invalid|Stepout|NoResult"
LIMIT = 3800        # the runner cuts an annotation's message at 4096
SUMMARY = os.environ.get("GITHUB_STEP_SUMMARY")
verdicts = []       # (MATCH or MISS, confidence, label, detail)
pending = []        # annotations, printed at the end, the verdicts first


# ── run ──────────────────────────────────────────────────────────────────────

def run_cmd(cmd, path):
    print("    " + " ".join(cmd))
    sys.stdout.flush()
    start = time.time()
    with open(path, "w") as out:
        status = subprocess.run(cmd, stdout=out, stderr=subprocess.STDOUT).returncode
    print("    exit status %d, %d s" % (status, time.time() - start))
    sys.stdout.flush()


def run_all():
    try:
        helptext = subprocess.run(["frama-c", "-wp-help"], capture_output=True,
                                  text=True).stdout
    except OSError as e:
        print("frama-c did not start: %s" % e)
        sys.exit(1)
    for f, entry, _ in ENTRIES:
        print("=== E: Eva from %s, %s" % (entry, f))
        run_cmd(["frama-c", "-eva", "-main", entry, CPP, f],
                "eva_%s.txt" % entry)
    # WP 29 made -wp-report-json a per-goal list; WP 25's is statistics.
    goals_opt = ("-wp-report-json" if "-wp-deprecated-report-json" in helptext
                 else None)
    cmd = ["frama-c", "-wp", "-wp-rte", "-wp-model", "Typed+Cast",
           "-wp-prover", "alt-ergo,z3,cvc5", "-wp-timeout", "120", "-wp-split",
           "-wp-fct", SEVEN]
    if goals_opt:
        cmd += [goals_opt, "wp_forms_goals.json"]
    print("=== W: WP on %s" % FORMS)
    run_cmd(cmd + [CPP, FORMS], "wp_forms.txt")
    with open("wp_forms.txt", errors="replace") as fh:
        for line in fh:
            if re.match(r"^\[wp\] (Proved goals:|\d+ goals scheduled|\[(%s)\])"
                        % NOT_PROVED, line):
                print("    " + line.rstrip())
    print("    wp_forms_goals.json: %s"
          % ("written" if os.path.exists("wp_forms_goals.json") else "absent"))


# ── reading ──────────────────────────────────────────────────────────────────

def read_eva(entry):
    """(precondition statuses [(file, line, function, name, status)],
    summary (valid, unknown, invalid) or None, alarms or None, text)."""
    path = "eva_%s.txt" % entry
    if not os.path.exists(path):
        return [], None, None, ""
    with open(path, errors="replace") as fh:
        text = fh.read()
    statuses = []
    where = (None, None)
    for line in text.split("\n"):
        # Frama-C 25 prints the status on the line after the location;
        # read both on every line, in case a later Eva joins them.
        m = re.match(r"^\[eva:alarm\] (\S+?):(\d+): Warning:", line)
        if m:
            where = (m.group(1), int(m.group(2)))
        # "function f: precondition ..." or, for a behaviour's clause,
        # "function f, behavior b: precondition ...".
        m = re.search(r"function (\w+)(?:, behavior \w+)?: precondition '([^']*)' "
                      r"got status (\w+)", line)
        if m:
            # ACSL names a precondition danglingness:s1; Eva 25 prints it
            # danglingness,s1. Read either.
            name = m.group(2).replace(":", ",")
            statuses.append((where[0], where[1], m.group(1), name, m.group(3)))
    m = re.search(r"Preconditions\s+(\d+) valid\s+(\d+) unknown\s+(\d+) invalid", text)
    summary = tuple(int(x) for x in m.groups()) if m else None
    m = re.search(r"(\d+) alarms? generated by the analysis", text)
    alarms = int(m.group(1)) if m else None
    return statuses, summary, alarms, text


def read_wp():
    """{goal: proved?} from the per-goal report, or, on WP 25, from the log."""
    res = {}
    js = "wp_forms_goals.json"
    if os.path.exists(js):
        try:
            with open(js) as fh:
                text = fh.read()
            gl = json.loads(re.sub(r"(\d)\.(?=\s*[,}\]])", r"\1.0", text))
            for g in gl if isinstance(gl, list) else []:
                if isinstance(g, dict) and "goal" in g:
                    res[str(g["goal"])] = str(g.get("verdict", "")).lower() == "valid"
            if res:
                return res, "per-goal report"
        except (OSError, ValueError) as e:
            print("wp_forms_goals.json did not parse (%s)" % e)
    if os.path.exists("wp_forms.txt"):
        with open("wp_forms.txt", errors="replace") as fh:
            for line in fh:
                # WP 25: "[wp] [Qed] Goal typed_cast_x : Valid"
                m = re.match(r"^\[wp\] \[[^\]]+\] Goal (typed\S*) : (\w+)", line)
                if m:
                    res[m.group(1)] = m.group(2) == "Valid"
    return res, "log"


# ── output ───────────────────────────────────────────────────────────────────

def esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def check(conf, label, ok, detail=""):
    v = "MATCH" if ok else "MISS"
    print("%s (%s) %s%s" % (v, conf, label, (": " + detail) if detail and not ok else ""))
    verdicts.append((v, conf, label, detail if not ok else ""))


def annotate(kind, title, lines):
    body, chunk = [], []
    for ln in lines:
        if sum(len(x) + 1 for x in chunk) + len(ln) > LIMIT:
            body.append(chunk)
            chunk = []
        chunk.append(ln)
    if chunk:
        body.append(chunk)
    for i, c in enumerate(body):
        t = title if len(body) == 1 else "%s (%d/%d)" % (title, i + 1, len(body))
        pending.append("::%s title=%s::%s" % (kind, esc(t), esc("\n".join(c))))


def finish(part):
    misses = [v for v in verdicts if v[0] == "MISS"]
    lines = ["%s (%s) %s%s" % (v, c, l, (": " + d) if d else "")
             for v, c, l, d in verdicts]
    annotate("notice", "G13 probe, report %s: %d MATCH, %d MISS"
             % (part, len(verdicts) - len(misses), len(misses)), lines)
    for v, c, l, d in misses[:9]:
        pending.insert(0, "::error title=%s::%s" % (esc("G13 probe MISS (%s)" % c),
                                                    esc(l + (": " + d if d else ""))))
    if len(misses) > 9:
        pending.insert(0, "::error title=G13 probe::%d more MISS" % (len(misses) - 9))
    for p in pending:
        print(p)
    if SUMMARY:
        with open(SUMMARY, "a") as fh:
            fh.write("### G13 probe, report %s\n\n```\n%s\n```\n\n" % (part, "\n".join(lines)))
    print("\n%s: %d MATCH, %d MISS" % (part, len(verdicts) - len(misses), len(misses)))
    sys.exit(1 if misses else 0)


def report_E():
    found = []
    for _, entry, exp in ENTRIES:
        statuses, summary, alarms, text = read_eva(entry)
        if not text:
            check("H", "%s: Eva ran" % entry, False, "no output")
            continue
        if exp[0] == "clean":
            ok = (summary is not None and summary[1] == 0 and summary[2] == 0
                  and not statuses)
            check("H", "%s: every precondition valid" % entry, ok,
                  "summary %s, statuses %s" % (summary, statuses))
        else:
            if exp[0] == "memcmp":
                fn, name, pair, where = "memcmp", "danglingness,s1", "danglingness,s2", exp[1]
            else:
                fn, name, pair, where = exp[1], exp[2], exp[3], None
            hit = [s for s in statuses if s[2] == fn and s[3] == name]
            # the pair is not reached once the first is invalid (Frama-C 25);
            # if a later Eva reaches it, it is false too, and may be invalid.
            two = [s for s in statuses if s[2] == fn and s[3] == pair]
            others = [s for s in statuses if s not in hit and s not in two]
            ok = any(s[4] == "invalid" and (where is None or s[0] == where) for s in hit)
            check("H", "%s: %s's %s invalid%s" % (entry, fn, name,
                                                  ", in %s" % where if where else ""),
                  ok, "statuses %s" % (hit or "none"))
            bad = [s for s in two if s[4] != "invalid"]
            invalid = sum(1 for s in hit + two if s[4] == "invalid")
            ok = (summary is not None and summary[1] == 0 and summary[2] == invalid
                  and not others and not bad)
            check("H", "%s: every other precondition valid, %s aside" % (entry, pair),
                  ok, "summary %s, other statuses %s" % (summary, others + bad))
        check("H", "%s: no alarm" % entry, alarms == 0, "alarms %s" % alarms)
        found.append("%s: %s; preconditions (valid, unknown, invalid) %s; alarms %s"
                     % (entry, "; ".join("%s:%s %s %s %s" % s for s in statuses) or "none",
                        summary, alarms))
    annotate("notice", "G13 probe, E: what Eva reported", found)
    finish("E")


def report_W():
    res, src = read_wp()
    print("goals read from the %s: %d" % (src, len(res)))
    for goal, proved, label in W_PRED:
        if goal not in res:
            check("H", label, False, "goal %s not generated or not read" % goal)
            continue
        check("H", label, res[goal] == proved,
              "%s is %s" % (goal, "proved" if res[goal] else "not proved"))
    predicted = {g for g, _, _ in W_PRED}
    rest = sorted(g for g in res if g not in predicted)
    measured = ["%s %s" % ("proved" if res[g] else "NOT PROVED", g) for g in rest]
    print("\nmeasured, not predicted:")
    for m in measured:
        print("  " + m)
    annotate("notice", "G13 probe, W: the other goals, measured (%s)" % src,
             measured or ["none"])
    finish("W")


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "run":
        run_all()
    elif len(sys.argv) >= 3 and sys.argv[1] == "report" and sys.argv[2] in ("E", "W"):
        {"E": report_E, "W": report_W}[sys.argv[2]]()
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
