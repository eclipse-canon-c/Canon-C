#!/usr/bin/env python3
"""
round3_probe.py — the WP runs of .github/workflows/round3-probe.yml
(VERIFY-040 round 3) and their reading against the committed predictions in
docs/deviations.md, VERIFY-040 round 3: "the probe's committed prediction",
for run #1, and "the fix push: committed prediction", for the runs since.

    python3 tools/probes/verify-040/round3_probe.py run
        Block 7's trial, with the CI's provers, Alt-Ergo, Z3 and CVC5 at
        120 s, as frama-c-bitset runs WP (Typed+Cast, -wp-rte, -wp-split, its
        preprocessing), on tools/probes/verify-040/p_padding_lemmas.h, which
        includes bitset's driver and states three lemmas:
          L  the three lemmas, each a goal of its own (-wp-prop);
          F  bitset_set, bitset_clear, bitset_toggle and bitset_clear_padding,
             whose goals have the lemmas as hypotheses (-wp-fct).
        Run #1 also measured block 1's change, an invariant, a frame and a
        variant for IMPL_VEC_FILL's loop, written in a copy of the tree; the
        fix push wrote it into data/vec/vec_impl.h, where the -CC units prove
        it, and its six runs were retired with it.
        Output, for each run X: wp_X.txt, the log; wp_X.json, the statistics
        per function and property (WP 29's -wp-deprecated-report-json, WP
        25's -wp-report-json); on WP 29 wp_X_goals.json, the per-goal report.
        Exits 1, with each failed run's last lines posted as an error
        annotation, if a run fails or writes no statistics.

    python3 tools/probes/verify-040/round3_probe.py report
        reads that output and prints MATCH or MISS, with its confidence, for
        each predicted part, or NO DATA where a part has nothing to read. The
        log needs a sign-in, so it also posts the verdicts and each run's
        counts and goals not proved as notices on the run page, and
        everything to the step summary. Exits 1 on a MISS, else 2 on NO DATA.

Run from the repository root, with frama-c on PATH.
"""
import json
import os
import re
import subprocess
import sys
import time

LEMMA_HDR = "tools/probes/verify-040/p_padding_lemmas.h"
# Run #1 found none of the three proved; the fix push predicts the same.
LEMMAS = [("bitset_shr_zero_bits", "H"), ("bitset_bits_shr_zero", "H"),
          ("bitset_mask_bits", "H")]
BITSET_FUNCS = ["bitset_set", "bitset_clear", "bitset_toggle", "bitset_clear_padding"]
BLOCK7 = ["typed_cast_bitset_set_live_ensures_3_part3",
          "typed_cast_bitset_clear_live_ensures_3_part3",
          "typed_cast_bitset_toggle_live_ensures_3_part3",
          "typed_cast_bitset_clear_padding_ensures_part1"]
CPP = "-I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG"
PROVERS = "alt-ergo,z3,cvc5"
# The verdicts that are neither a proof nor a prover's want of one. WP 29's
# per-goal report writes NoResult, a goal no prover answered, as "none";
# "failure", "noresult" and "computing" are kept for any other spelling.
BAD = {"failed", "failure", "invalid", "stepout", "none", "noresult", "computing"}
P = "typed_cast_"
LIMIT = 3800        # the runner cuts an annotation's message at 4096
SUMMARY = os.environ.get("GITHUB_STEP_SUMMARY")
verdicts = []       # (MATCH, MISS or NO DATA, confidence, label, detail)
notes = []
pending = []        # annotations, printed at the end, the verdicts first

# run: (selection, functions read)
RUNS = {
    "L": (["-wp-prop", ",".join(n for n, _ in LEMMAS)], []),
    "F": (["-wp-fct", ",".join(BITSET_FUNCS)], BITSET_FUNCS),
}
ORDER = ["L", "F"]


def esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


# ── run ──────────────────────────────────────────────────────────────────────

def run_all():
    try:
        helptext = subprocess.run(["frama-c", "-wp-help"], capture_output=True,
                                  text=True).stdout
    except OSError as e:
        print("frama-c did not start: %s" % e)
        sys.exit(1)
    # WP 29 moved the statistics report to -wp-deprecated-report-json and
    # made -wp-report-json a per-goal list; WP 25 has only the statistics.
    if "-wp-deprecated-report-json" in helptext:
        stats_opt, goals_opt = "-wp-deprecated-report-json", "-wp-report-json"
    elif "-wp-report-json" in helptext:
        stats_opt, goals_opt = "-wp-report-json", None
    else:
        stats_opt, goals_opt = None, None
    print("statistics report: %s; per-goal report: %s" % (stats_opt, goals_opt))
    failed = []
    for run in ORDER:
        sel = RUNS[run][0]
        out = {k: "wp_%s%s" % (run, k) for k in (".txt", ".json", "_goals.json")}
        for f in out.values():
            if os.path.exists(f):
                os.remove(f)
        cmd = ["frama-c", "-wp", "-wp-rte", "-wp-model", "Typed+Cast",
               "-wp-prover", PROVERS, "-wp-timeout", "120", "-wp-split"] + sel
        if stats_opt:
            cmd += [stats_opt, out[".json"]]
        if goals_opt:
            cmd += [goals_opt, out["_goals.json"]]
        cmd += ["-cpp-extra-args=%s" % CPP, LEMMA_HDR]
        print("=== %s: %s" % (run, " ".join(cmd)))
        sys.stdout.flush()
        start = time.time()
        with open(out[".txt"], "w") as fh:
            status = subprocess.run(cmd, stdout=fh, stderr=subprocess.STDOUT).returncode
        print("    exit status %d, %d s" % (status, time.time() - start))
        with open(out[".txt"], errors="replace") as fh:
            for line in fh:
                if re.match(r"^\[wp\] (Proved goals:|\d+ goals scheduled)", line):
                    print("    " + line.rstrip())
        if status != 0:
            failed.append((run, "exited %d" % status))
        elif not os.path.exists(out[".json"]):
            failed.append((run, "wrote no statistics"))
        sys.stdout.flush()
    if failed:
        print("FAILED: %s" % "; ".join("%s %s" % f for f in failed))
        # The log needs a sign-in: post why, with each failed run's last lines.
        for run, why in failed:
            try:
                with open("wp_%s.txt" % run, errors="replace") as fh:
                    tail = fh.read().splitlines()[-12:]
            except OSError:
                tail = ["no output file"]
            msg = ("%s %s\n%s" % (run, why, "\n".join(tail)))[-LIMIT:]
            print("::error title=round 3 probe WP run %s::%s" % (run, esc(msg)))
        sys.exit(1)


# ── reading ──────────────────────────────────────────────────────────────────

def load_json(path, run):
    if not os.path.exists(path):
        return None
    try:
        with open(path) as fh:
            text = fh.read()
        # WP 29 prints a float such as 0.0 as "0.", which json rejects.
        return json.loads(re.sub(r"(\d)\.(?=\s*[,}\]])", r"\1.0", text))
    except (OSError, ValueError) as e:
        notes.append("%s: %s did not parse (%s)" % (run, path, e))
        return None


def read_run(run):
    """{proved, total: the summary line; stats; goals, the per-goal report;
    verdicts {goal: valid?} and src, where they come from}."""
    out = {"proved": None, "total": None, "stats": None, "goals": None,
           "verdicts": {}, "src": None}
    log = "wp_%s.txt" % run
    if not os.path.exists(log):
        notes.append("%s: no output file" % run)
        return out
    with open(log, errors="replace") as fh:
        lines = fh.read().split("\n")
    logged = {}
    for line in lines:
        m = re.match(r"^\[wp\] Proved goals:\s*(\d+)\s*/\s*(\d+)", line)
        if m:
            out["proved"], out["total"] = int(m.group(1)), int(m.group(2))
        # A goal's result: WP 25 prints each goal so with a prover, "[wp]
        # [Z3 4.8.12] Goal <name> : Valid" or "[wp] [Failed] Goal <name>";
        # WP 29 prints only the goals not proved, "[wp] [Timeout] <name>".
        m = re.match(r"^\[wp\] \[([^\]]+)\] (?:Goal )?(typed_\S+)(?: : (\S+))?", line)
        if m:
            logged[m.group(2)] = (m.group(3) or m.group(1)).lower() == "valid"
    goals = out["goals"] = load_json("wp_%s_goals.json" % run, run)
    if isinstance(goals, list):
        out["verdicts"] = {str(g.get("goal")): str(g.get("verdict", "")).lower() == "valid"
                           for g in goals if isinstance(g, dict) and "goal" in g}
        out["src"] = "per-goal report"
    elif logged:
        out["verdicts"], out["src"] = logged, "log"
    out["stats"] = load_json("wp_%s.json" % run, run)
    return out


def stats_total(data, fn):
    """fn's goals in the statistics report, or None."""
    js = data["stats"]
    fns = js.get("wp:functions") if isinstance(js, dict) else None
    e = fns.get(fn) if isinstance(fns, dict) else None
    sec = e.get("wp:section") if isinstance(e, dict) else None
    m = sec.get("wp:main", sec) if isinstance(sec, dict) else None
    return int(m["total"]) if isinstance(m, dict) and "total" in m else None


def of_funcs(vs, funcs):
    """The goals of funcs, by name: typed_cast_<function>_..."""
    return {g: v for g, v in vs.items() if any(g.startswith(P + f + "_") for f in funcs)}


def lemma_goals(name, vs):
    """A lemma's goals: WP names them typed_cast_lemma_<name>, or, without
    the model's prefix, lemma_<name>."""
    return {g: v for g, v in vs.items()
            if re.search(r"(?:^|_)lemma_%s(?:_part\d+)?$" % re.escape(name), g)}


def short(g):
    return g[len(P):] if g.startswith(P) else g


def not_proved(vs):
    return sorted(short(g) for g, v in vs.items() if not v)


# ── reporting ────────────────────────────────────────────────────────────────

def check(conf, label, ok, detail=""):
    v = "NO DATA" if ok is None else "MATCH" if ok else "MISS"
    print("%s (%s) %s%s" % (v, conf, label, (": " + detail) if detail else ""))
    verdicts.append((v, conf, label, detail))


def annotate(kind, title, lines):
    chunks, cur = [], ""
    for line in lines:
        line = line if len(line) <= LIMIT else line[:LIMIT - 20] + " [cut]"
        if cur and len(cur) + 1 + len(line) > LIMIT:
            chunks.append(cur)
            cur = line
        else:
            cur = cur + "\n" + line if cur else line
    if cur:
        chunks.append(cur)
    for i, c in enumerate(chunks):
        t = title if len(chunks) == 1 else "%s (%d of %d)" % (title, i + 1, len(chunks))
        pending.append("::%s title=%s::%s" % (
            kind, esc(t).replace(":", "%3A").replace(",", "%2C"), esc(c)))


def flush(first):
    """Print the annotations, those from index first on before the rest: the
    runner shows only so many of each kind per step, so the verdicts must not
    come last."""
    global pending
    for a in pending[first:] + pending[:first]:
        print(a)
    pending = []


def summary(lines):
    if SUMMARY:
        with open(SUMMARY, "a") as fh:
            fh.write("\n".join(lines) + "\n\n")


def describe(run, d):
    funcs = RUNS[run][1]
    stats = "; statistics: %s" % ", ".join("%s %s" % (f, stats_total(d, f))
                                           for f in funcs) if funcs else ""
    return "%s: summary %s / %s%s; %d goals with a verdict (%s); not proved: %s" % (
        run, d["proved"], d["total"], stats, len(d["verdicts"]), d["src"],
        ", ".join(not_proved(d["verdicts"])) or "none")


def predictions(D):
    for name, conf in LEMMAS:
        g = lemma_goals(name, D["L"]["verdicts"])
        check(conf, "block 7: lemma %s not proved" % name,
              None if not g else not any(g.values()),
              ", ".join("%s %s" % (short(k), "valid" if v else "not proved")
                        for k, v in sorted(g.items())) or "no goal of it named")
    fv = of_funcs(D["F"]["verdicts"], BITSET_FUNCS)
    check("M", "block 7: with the three lemmas as hypotheses, every goal of the four "
          "functions proves, block 7's four among them",
          None if not fv or not all(b in fv for b in BLOCK7) else all(fv.values()),
          "%d goals, not proved: %s" % (len(fv), ", ".join(not_proved(fv)) or "none"))
    # Every run.
    wp29 = any(D[run]["src"] == "per-goal report" for run in ORDER)
    gaps = []
    for run in ORDER:
        d = D[run]
        if d["total"] is None:
            gaps.append("%s printed no summary line" % run)
        if d["stats"] is None:
            gaps.append("%s wrote no statistics" % run)
        if wp29 and d["src"] != "per-goal report":
            gaps.append("%s wrote no per-goal report" % run)
        if d["total"] is not None:
            left, listed = d["total"] - d["proved"], len(not_proved(d["verdicts"]))
            if left != listed:
                gaps.append("%s: %d not proved by its summary line, %d listed (%s)"
                            % (run, left, listed, d["src"]))
    check("H", "every run printed its summary line and wrote its statistics%s, and in "
          "each the goals listed as not proved account for its summary line"
          % (" and its per-goal report" if wp29 else ""), not gaps, "; ".join(gaps))
    bad, seen = [], False
    for run in ORDER:
        goals = D[run]["goals"]
        if isinstance(goals, list):
            seen = True
            bad += ["%s %s %s" % (run, short(str(g.get("goal"))), g.get("verdict"))
                    for g in goals if isinstance(g, dict)
                    and str(g.get("verdict", "")).lower() in BAD]
    check("H", "no goal Failed, Failure, Invalid, Stepout or NoResult, by WP 29's "
          "per-goal reports", None if not seen else not bad,
          ", ".join(bad) or ("none" if seen else "no per-goal report (WP 25)"))


def main():
    if sys.argv[1:] == ["run"]:
        run_all()
        return
    if sys.argv[1:] != ["report"]:
        print(__doc__)
        sys.exit(2)
    D = {run: read_run(run) for run in ORDER}
    runs = [describe(run, D[run]) for run in ORDER]
    for line in runs:
        print(line)
    annotate("notice", "round 3 probe: the runs", runs)
    summary(["### Runs"] + ["- " + x for x in runs])
    predictions(D)
    for n in notes:
        print("NOTE: " + n)
    vlines = ["%s (%s) %s%s" % (v, c, l, (": " + d) if d else "")
              for v, c, l, d in verdicts] + ["NOTE: " + n for n in notes]
    first = len(pending)
    annotate("notice", "round 3 probe: verdicts", vlines)
    summary(["### Verdicts"] + ["- " + x for x in vlines])
    bad = [(v, c, l, d) for v, c, l, d in verdicts if v != "MATCH"]
    for v, c, l, d in bad[:9]:
        annotate("error", "round 3 probe: %s (%s)" % (v, c), [l + (": " + d if d else "")])
    flush(first)
    if any(v == "MISS" for v, _, _, _ in bad):
        sys.exit(1)
    if bad:
        sys.exit(2)
    print("Every part of the round 3 probe MATCHES.")


main()
