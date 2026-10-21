#!/usr/bin/env python3
"""
round3_probe.py — the WP runs of .github/workflows/round3-probe.yml
(VERIFY-040 round 3) and their reading against the committed prediction in
docs/deviations.md, VERIFY-040 round 3, "the probe's committed prediction".

    python3 tools/probes/verify-040/round3_probe.py run
        Block 1's measurement, on fill's loop with the invariants, frame and
        variant below (ANNOTATION) written into IMPL_VEC_FILL in a copy of
        the tree, r3_annotated/, and on the tree as committed:
          CB, SB  cc-vec's vec_int_fill and cc-vec_struct's vec_Rec_fill as
                  committed, under -CC, as the -CC units run them, with Qed,
                  WP's simplifier, as the only prover;
          CP, SP  the same, annotated, with the CI's provers, Alt-Ergo, Z3
                  and CVC5 at 120 s, and -wp-print;
          VB, VH  frama-c-vec's vec_int_fill, under the home unit's
                  preprocessing, which keeps no comment written in a macro
                  body: as committed and annotated, with Qed alone.
        Block 7's trial, with the CI's provers, as frama-c-bitset runs WP, on
        tools/probes/verify-040/p_padding_lemmas.h, which includes bitset's
        driver and states three lemmas:
          L  the three lemmas, each a goal of its own (-wp-prop);
          F  bitset_set, bitset_clear, bitset_toggle and bitset_clear_padding,
             whose goals have the lemmas as hypotheses (-wp-fct).
        Every run is Typed+Cast, with -wp-rte and -wp-split, the units' model
        and options. Output, for each run X: wp_X.txt, the log; wp_X.json, the
        statistics per function and property (WP 29's
        -wp-deprecated-report-json, WP 25's -wp-report-json); on WP 29
        wp_X_goals.json, the per-goal report, which WP 29 writes for the
        goals it schedules for a prover, Qed among them. Exits 1, with each
        failed run's last lines posted as an error annotation, if a run fails
        or writes no statistics.

    python3 tools/probes/verify-040/round3_probe.py report
        reads that output and prints MATCH or MISS, with its confidence, for
        each predicted part, or NO DATA where a part has nothing to read. The
        log needs a sign-in, so it also posts the verdicts, each run's counts
        and the goals with a prover that did not prove, and, for CP and SP,
        the goals by property and the run-time checks as printed, each with
        the definitions its goal reads, as notices on the run page, and
        everything to the step summary. WP 29 prints no goal's name: its
        printed checks pair with the per-goal report's names in print order,
        property by property and part by part. Exits 1 on a MISS, else 2 on
        NO DATA.

Run from the repository root, with frama-c on PATH.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time

COPY = "r3_annotated"
IMPL = "data/vec/vec_impl.h"
ANCHOR = ("    usize to_fill   = (count < remaining) ? count : remaining; \\\n"
          "    for (usize i = 0; i < to_fill; i++) { \\\n")
# Each line ends in a continuation at column 82, as IMPL_VEC_FILL's contract.
ANNOTATION = "".join(line.ljust(81) + "\\\n" for line in [
    "    /*@ loop invariant 0 <= i <= to_fill;",
    "        loop invariant \\forall integer k; v->len <= k < v->len + i",
    "        ==> v->items[k] == value;",
    "        loop assigns i, v->items[v->len .. v->len + to_fill - 1];",
    "        loop variant to_fill - i; */",
])
LEMMA_HDR = "tools/probes/verify-040/p_padding_lemmas.h"
LEMMAS = [("bitset_shr_zero_bits", "M"), ("bitset_bits_shr_zero", "H"),
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
WP25_ADDED = 27     # goals the annotation adds to each fill, as WP 25 makes them
LIMIT = 3800        # the runner cuts an annotation's message at 4096
SUMMARY = os.environ.get("GITHUB_STEP_SUMMARY")
verdicts = []       # (MATCH, MISS or NO DATA, confidence, label, detail)
notes = []
pending = []        # annotations, printed at the end, the verdicts first

INT, REC = "vec_int_fill", "vec_Rec_fill"
CC_VEC = "vmacros/vdrivers/vec_cc_experiment.h"
CC_REC = "vmacros/vdrivers/vec_struct_cc_experiment.h"
HOME = "vmacros/vdrivers/vec_verify.h"
# run: (tree, file, -CC?, provers, selection, -wp-print?, functions read)
RUNS = {
    "CB": (".", CC_VEC, True, "qed", ["-wp-fct", INT], False, [INT]),
    "SB": (".", CC_REC, True, "qed", ["-wp-fct", REC], False, [REC]),
    "VB": (".", HOME, False, "qed", ["-wp-fct", INT], False, [INT]),
    "VH": (COPY, HOME, False, "qed", ["-wp-fct", INT], False, [INT]),
    "CP": (COPY, CC_VEC, True, PROVERS, ["-wp-fct", INT], True, [INT]),
    "SP": (COPY, CC_REC, True, PROVERS, ["-wp-fct", REC], True, [REC]),
    "L": (".", LEMMA_HDR, False, PROVERS, ["-wp-prop", ",".join(n for n, _ in LEMMAS)],
          False, []),
    "F": (".", LEMMA_HDR, False, PROVERS, ["-wp-fct", ",".join(BITSET_FUNCS)], False,
          BITSET_FUNCS),
}
ORDER = ["CB", "SB", "VB", "VH", "CP", "SP", "L", "F"]


def esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


# ── run ──────────────────────────────────────────────────────────────────────

def make_copy():
    """r3_annotated/: the tree with ANNOTATION written into IMPL_VEC_FILL."""
    if os.path.isdir(COPY):
        shutil.rmtree(COPY)
    shutil.copytree(".", COPY, ignore=shutil.ignore_patterns(".git", COPY, "wp_*"))
    path = os.path.join(COPY, IMPL)
    with open(path) as fh:
        text = fh.read()
    if text.count(ANCHOR) != 1:
        print("FAILED: IMPL_VEC_FILL's loop found %d times in %s, not once"
              % (text.count(ANCHOR), IMPL))
        print("::error title=round 3 probe::the loop to annotate was found %d times in %s"
              % (text.count(ANCHOR), IMPL))
        sys.exit(1)
    first, second = ANCHOR.split("\n")[:2]
    with open(path, "w") as fh:
        fh.write(text.replace(ANCHOR, first + "\n" + ANNOTATION + second + "\n"))
    print("%s/%s: IMPL_VEC_FILL's loop annotated" % (COPY, IMPL))


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
    make_copy()
    here = os.getcwd()
    failed = []
    for run in ORDER:
        tree, src, cc, provers, sel, prt, _ = RUNS[run]
        out = {k: os.path.join(here, "wp_%s%s" % (run, k))
               for k in (".txt", ".json", "_goals.json")}
        for f in out.values():
            if os.path.exists(f):
                os.remove(f)
        cmd = ["frama-c", "-wp", "-wp-rte", "-wp-model", "Typed+Cast",
               "-wp-prover", provers]
        if provers != "qed":
            cmd += ["-wp-timeout", "120"]
        cmd += ["-wp-split"] + (["-wp-print"] if prt else []) + sel
        if stats_opt:
            cmd += [stats_opt, out[".json"]]
        if goals_opt:
            cmd += [goals_opt, out["_goals.json"]]
        cmd += ["-cpp-extra-args=%s%s" % ("-CC " if cc else "", CPP), src]
        print("=== %s, in %s: %s" % (run, tree, " ".join(cmd)))
        sys.stdout.flush()
        start = time.time()
        with open(out[".txt"], "w") as fh:
            status = subprocess.run(cmd, cwd=os.path.join(here, tree), stdout=fh,
                                    stderr=subprocess.STDOUT).returncode
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
    verdicts {goal: valid?} and src, where they come from; printed [{gid,
    title, body}]}."""
    out = {"proved": None, "total": None, "stats": None, "goals": None,
           "verdicts": {}, "src": None, "printed": []}
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
    # The printed goals: "Goal <title>:", the title wrapped onto the next
    # line where it is long, then the goal up to a line of dashes. A local
    # WP 25 built to name each goal prints "GID <name>" on the line before;
    # WP 29 names none.
    cur, gid, title = None, None, None
    for line in lines:
        if title is not None:
            title += " " + line.strip()
            if title.endswith(":"):
                cur = {"title": title[:-1].strip(), "body": [], "gid": gid}
                out["printed"].append(cur)
                title, gid = None, None
            continue
        m = re.match(r"^GID (\S+)$", line)
        if m:
            gid = m.group(1)
            continue
        m = re.match(r"^Goal (.*)$", line)
        if m:
            title = m.group(1).rstrip()
            if title.endswith(":"):
                cur = {"title": title[:-1].strip(), "body": [], "gid": gid}
                out["printed"].append(cur)
                title, gid = None, None
            continue
        if cur is not None:
            if re.match(r"^-{20,}$", line):
                cur = None
            else:
                cur["body"].append(line)
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
    funcs = RUNS[run][6]
    stats = "; statistics: %s" % ", ".join("%s %s" % (f, stats_total(d, f))
                                           for f in funcs) if funcs else ""
    line = "%s: summary %s / %s%s; %d goals with a verdict (%s)" % (
        run, d["proved"], d["total"], stats, len(d["verdicts"]), d["src"])
    if RUNS[run][3] != "qed":
        line += "; not proved: %s" % (", ".join(not_proved(d["verdicts"])) or "none")
    return line


def by_property(run, d, fn):
    """fn's goals, property by property: each goal's name less the model, the
    function and its part, with how many of its goals proved."""
    groups = {}
    for g, v in sorted(of_funcs(d["verdicts"], [fn]).items()):
        prop = re.sub(r"_part\d+$", "", g[len(P) + len(fn) + 1:])
        n, ok = groups.get(prop, (0, 0))
        groups[prop] = (n + 1, ok + (1 if v else 0))
    return ["%s %s: %s" % (run, fn, ", ".join("%s %d of %d" % (p, ok, n)
                                             for p, (n, ok) in sorted(groups.items()))
                           or "no goal with a verdict")]


def logical_lines(body):
    """A printed goal's lines, each with the indented lines after it, which
    continue it, joined on: "Let a = ...", "Assume { ... }", "Prove: ..."."""
    out = []
    for line in body:
        if not line.strip():
            continue
        if out and line[0].isspace():
            out[-1] += " " + line.strip()
        else:
            out.append(line.strip())
    return out


def rte_printed(run, d):
    """The run-time checks as WP printed them, in order: title, goal, and the
    definitions the goal reads (WP's "Let" lines), followed through, since
    the goal alone, valid_rw(Malloc_0, a, 1), does not say which access it
    is."""
    out = []
    for g in d["printed"]:
        if "rte" not in g["title"]:
            continue
        lets, prove = {}, ""
        for line in logical_lines(g["body"]):
            m = re.match(r"^Let (\w+) = (.*?)\.?$", line)
            if m:
                lets[m.group(1)] = m.group(2)
            elif line.startswith("Prove:") and not prove:
                prove = line
        names, todo = [], re.findall(r"[A-Za-z_]\w*", prove)
        while todo:
            v = todo.pop(0)
            if v in lets and v not in names:
                names.append(v)
                todo += re.findall(r"[A-Za-z_]\w*", lets[v])
        where = ", ".join("%s = %s" % (v, lets[v]) for v in names)
        out.append("%s%s | %s%s" % ("%s | " % g["gid"] if g["gid"] else "", g["title"],
                                    prove[:300] or "no Prove line",
                                    " | where " + where[:300] if where else ""))
    return out or ["%s: no run-time check printed" % run]


def predictions(D):
    # Block 1.
    for run, fn in (("CP", INT), ("SP", REC)):
        vs = of_funcs(D[run]["verdicts"], [fn])
        check("M", "block 1: under -CC, with the annotation, every goal of %s proves" % fn,
              None if not vs else all(vs.values()),
              "%d goals, not proved: %s" % (len(vs), ", ".join(not_proved(vs)) or "none"))
    for base, ann, fn in (("CB", "CP", INT), ("SB", "SP", REC)):
        b, h = D[base]["total"], D[ann]["total"]
        sb, sh = stats_total(D[base], fn), stats_total(D[ann], fn)
        check("L", "block 1: under -CC the annotation adds %d goals to %s, as WP 25 makes them"
              % (WP25_ADDED, fn), None if b is None or h is None else h - b == WP25_ADDED,
              "summary lines %s as committed and %s annotated, %s; statistics %s and %s" % (
                  b, h, "%+d" % (h - b) if b is not None and h is not None else "?", sb, sh))
    vb, vh = D["VB"], D["VH"]
    nb, nh = set(of_funcs(vb["verdicts"], [INT])), set(of_funcs(vh["verdicts"], [INT]))
    same = None
    if vb["total"] is not None and vh["total"] is not None:
        same = (vb["total"], vb["proved"], stats_total(vb, INT)) == \
               (vh["total"], vh["proved"], stats_total(vh, INT)) and nb == nh
    check("H", "block 1: frama-c-vec's preprocessing keeps no annotation in IMPL_VEC_FILL: "
          "vec_int_fill's goals the same, annotated or not",
          same, "summary lines %s / %s and %s / %s, statistics %s and %s, names %s" % (
              vb["proved"], vb["total"], vh["proved"], vh["total"], stats_total(vb, INT),
              stats_total(vh, INT), "equal" if nb == nh else "differ"))
    # Block 7.
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
    # A refutation, or a prover that gave no answer, in the runs with provers;
    # with Qed alone a goal Qed does not prove has no prover's answer.
    bad, seen = [], False
    for run in ORDER:
        if RUNS[run][3] == "qed":
            continue
        goals = D[run]["goals"]
        if isinstance(goals, list):
            seen = True
            bad += ["%s %s %s" % (run, short(str(g.get("goal"))), g.get("verdict"))
                    for g in goals if isinstance(g, dict)
                    and str(g.get("verdict", "")).lower() in BAD]
    check("H", "no goal Failed, Failure, Invalid, Stepout or NoResult in the runs with "
          "provers, by WP 29's per-goal reports", None if not seen else not bad,
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
    for run, fn in (("CP", INT), ("SP", REC)):
        props = by_property(run, D[run], fn)
        rte = rte_printed(run, D[run])
        print("--- %s: %s's goals by property" % (run, fn))
        for line in props:
            print("  " + line)
        print("--- %s: the run-time checks as printed" % run)
        for line in rte:
            print("  " + line)
        annotate("notice", "round 3 probe %s: goals by property" % run, props)
        annotate("notice", "round 3 probe %s: run-time checks as printed" % run, rte)
        summary(["### %s: %s's goals by property" % (run, fn)] + ["- " + x for x in props]
                + ["", "### %s: the run-time checks as printed" % run] + ["- " + x for x in rte])
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
    if len(bad) > 9:
        annotate("error", "round 3 probe", ["and %d more; see the verdicts notice"
                                            % (len(bad) - 9)])
    flush(first)
    if any(v == "MISS" for v, _, _, _ in bad):
        sys.exit(1)
    if bad:
        sys.exit(2)
    print("Every part of the round 3 probe MATCHES.")


main()
