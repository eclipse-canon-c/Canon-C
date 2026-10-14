#!/usr/bin/env python3
"""
g11_probe.py — the WP runs of .github/workflows/g11-probe.yml (VERIFY-039 G11)
and their reading against the committed prediction in docs/deviations.md,
VERIFY-039, "G11 — committed prediction for the probe".

    python3 tools/probes/verify-039/g11_probe.py run
        A1 and A2 prove the recommended contracts of p_combinator_contracts.c,
        its sixteen combinators and two clients, under Typed and Typed+Cast.
        B1 to B7 generate, without proving (-wp-prover none), the goals of
        today's combinators in the files the units verify, to count them
        function by function. Each run takes its unit's model, -wp-rte,
        -wp-split, Alt-Ergo, Z3 and CVC5 at 120 s, and -cpp-extra-args, and
        is restricted to those functions with -wp-fct. Output: wp_<run>.txt;
        wp_<run>.json, the statistics per function and property (WP 29's
        -wp-deprecated-report-json, WP 25's -wp-report-json); and for A, on
        WP 29, wp_<run>_goals.json, the per-goal report of -wp-report-json.

    python3 tools/probes/verify-039/g11_probe.py report A|B|compare
        reads that output and prints MATCH or MISS, with its confidence, for
        each predicted part. The log needs a sign-in, so it also posts the
        verdicts, and each run's goals function by function, as notices on
        the run page (each under the runner's 4096-character cut), an error
        annotation for each MISS (up to nine, and a count of the rest), and
        everything to the step summary. Exits 1 on a MISS. A client's goal
        predicted proved must have been generated: the per-goal report, or
        without it the statistics, must show one. B's counts are measured,
        not predicted.

Run from the repository root, with frama-c on PATH.
"""
import json
import os
import re
import subprocess
import sys
import time

OPT = ["map", "and_then", "or_else", "filter", "combine_with", "eq"]
RES = ["map", "map_err", "and_then", "or_else", "eq"]
PROBE = "tools/probes/verify-039/p_combinator_contracts.c"
# (name in goal names, name in C, combinators). The kernel drops the
# repeated underscore of result__Bool_Error from goal names.
OPT_INT = ("option_int", "option_int", OPT)
RES_INT = ("result_int_VErr", "result_int_VErr", RES)
RES_BOOL = ("result_Bool_Error", "result__Bool_Error", RES)
BASE = "-I core/primitives -I core -I semantics -I ."
DATA = "-I core/primitives -I core -I semantics -I data -I ."
PQ = "-I data -I core -I core/primitives -I semantics -I ."
# run: (file, model, instantiations, has the clients, include flags), each
# with its unit's model and flags: frama-c-option, -result, -deque, -vec,
# -bitset, -range and -priority-queue.
RUNS = {
    "A1": (PROBE, "Typed", [OPT_INT, RES_INT, RES_BOOL], True, BASE),
    "A2": (PROBE, "Typed+Cast", [OPT_INT, RES_INT, RES_BOOL], True, BASE),
    "B1": ("vmacros/vdrivers/option_verify.h", "Typed", [OPT_INT], False, BASE),
    "B2": ("vmacros/vdrivers/result_verify.h", "Typed", [RES_INT], False, BASE),
    "B3": ("vmacros/vdrivers/deque_verify.h", "Typed",
           [OPT_INT, RES_BOOL], False, DATA),
    "B4": ("vmacros/vdrivers/vec_verify.h", "Typed+Cast",
           [OPT_INT, RES_BOOL], False, DATA),
    "B5": ("vmacros/vdrivers/bitset_verify.h", "Typed+Cast",
           [("option_usize", "option_usize", OPT)], False, DATA),
    "B6": ("vmacros/vdrivers/range_verify.h", "Typed",
           [("option_isize", "option_isize", OPT)], False, DATA),
    "B7": ("data/priority_queue.h", "Typed+Cast", [RES_BOOL], False, PQ),
}
# WP 29's tags for a goal not proved; the CI jobs pool Timeout and Unknown.
NOT_PROVED = "Timeout|Unknown|Failed|Failure|Invalid|Stepout|NoResult"
BAD = {"Failed", "Failure", "Invalid", "Stepout", "NoResult"}
LIMIT = 3800        # the runner cuts an annotation's message at 4096
SUMMARY = os.environ.get("GITHUB_STEP_SUMMARY")
verdicts = []       # (MATCH or MISS, confidence, label, detail)
notes = []
pending = []        # annotations, printed at the end, the verdicts first


def prefix(model):
    return "typed_cast_" if model == "Typed+Cast" else "typed_"


def functions(run):
    """[(name in goal names, name in C)] of the functions a run selects."""
    types, clients = RUNS[run][2], RUNS[run][3]
    fs = [(g + "_" + c, cn + "_" + c) for g, cn, cs in types for c in cs]
    if clients:
        fs += [("client_some", "client_some"), ("client_none", "client_none")]
    return fs


def owner(run, goal):
    """The selected function a goal belongs to (goal-name form), or None."""
    p = prefix(RUNS[run][1])
    if not goal.startswith(p):
        return None
    rest = goal[len(p):]
    best = None
    for gname, _ in functions(run):
        if rest.startswith(gname + "_") and (best is None or len(gname) > len(best)):
            best = gname
    return best


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
    for run in sorted(RUNS):
        f, model, _, _, inc = RUNS[run]
        provers = "alt-ergo,z3,cvc5" if run.startswith("A") else "none"
        cmd = ["frama-c", "-wp", "-wp-rte", "-wp-model", model,
               "-wp-prover", provers, "-wp-timeout", "120", "-wp-split",
               "-wp-fct", ",".join(c for _, c in functions(run))]
        if stats_opt:
            cmd += [stats_opt, "wp_%s.json" % run]
        if goals_opt and run.startswith("A"):
            cmd += [goals_opt, "wp_%s_goals.json" % run]
        cmd += ["-cpp-extra-args=%s -DCANON_NO_REQUIRE -DNDEBUG" % inc, f]
        print("=== %s: %s, %s, provers %s" % (run, f, model, provers))
        print("    " + " ".join(cmd))
        sys.stdout.flush()
        start = time.time()
        with open("wp_%s.txt" % run, "w") as out:
            status = subprocess.run(cmd, stdout=out, stderr=subprocess.STDOUT).returncode
        print("    exit status %d, %d s" % (status, time.time() - start))
        with open("wp_%s.txt" % run, errors="replace") as fh:
            for line in fh:
                if re.match(r"^\[wp\] (Proved goals:|\d+ goals scheduled|\[(%s)\])"
                            % NOT_PROVED, line):
                    print("    " + line.rstrip())
        for js in ("wp_%s.json" % run, "wp_%s_goals.json" % run):
            print("    %s: %s" % (js, "written" if os.path.exists(js) else "absent"))
        sys.stdout.flush()


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
    out = {"proved": None, "total": None, "scheduled": None,
           "not_proved": {}, "stats": None, "goals": None}
    log = "wp_%s.txt" % run
    if os.path.exists(log):
        with open(log, errors="replace") as fh:
            for line in fh:
                m = re.match(r"^\[wp\] Proved goals:\s*(\d+)\s*/\s*(\d+)", line)
                if m:
                    out["proved"], out["total"] = int(m.group(1)), int(m.group(2))
                m = re.match(r"^\[wp\] (\d+) goals scheduled", line)
                if m:
                    out["scheduled"] = int(m.group(1))
                m = re.match(r"^\[wp\] \[(%s)\] (?:Goal )?(typed\S*)" % NOT_PROVED, line)
                if m and m.group(2) not in out["not_proved"]:
                    out["not_proved"][m.group(2)] = m.group(1)
    else:
        notes.append("%s: no output file" % run)
    out["stats"] = load_json("wp_%s.json" % run, run)
    out["goals"] = load_json("wp_%s_goals.json" % run, run)
    return out


def counts(entry):
    """(valid, total) of one property or section of the statistics report."""
    if not isinstance(entry, dict):
        return None
    m = entry.get("wp:main", entry)
    if not isinstance(m, dict) or "total" not in m:
        return None
    return (int(m.get("valid", 0)), int(m["total"]))


def per_function(run, data):
    """{function: (valid, total, {property, less the function's name: (v, t)})}."""
    res = {}
    js = data["stats"]
    if js is None:
        return res
    fns = js.get("wp:functions") if isinstance(js, dict) else None
    if not isinstance(fns, dict):
        notes.append("%s: the statistics report has no 'wp:functions' (%s)"
                     % (run, type(js).__name__))
        return res
    for gname, cname in functions(run):
        f = fns.get(cname, fns.get(gname))
        if not isinstance(f, dict):
            continue
        props = {}
        for k, v in f.items():
            if k == "wp:section":
                continue
            c = counts(v)
            if c is None:
                continue
            for n in (cname, gname):
                if k.startswith(n + "_"):
                    k = k[len(n) + 1:]
                    break
            props[k] = c
        sec = counts(f.get("wp:section"))
        if sec is None:
            sec = (sum(v for v, _ in props.values()), sum(t for _, t in props.values()))
        res[gname] = (sec[0], sec[1], props)
    return res


def goals_line(run, data):
    """The per-goal report against the log, when WP 29 wrote one."""
    gl = data["goals"]
    if gl is None:
        return None
    if not isinstance(gl, list):
        return "per-goal report: not a list (%s)" % type(gl).__name__
    names = {str(g.get("goal")): str(g.get("verdict", "")).lower()
             for g in gl if isinstance(g, dict) and "goal" in g}
    valid = sum(1 for v in names.values() if v == "valid")
    other = {n for n, v in names.items() if v != "valid"}
    agree = "agrees" if other == set(data["not_proved"]) else "DIFFERS"
    return ("per-goal report: %d goals, %d valid; its set not valid %s with the log's"
            % (len(names), valid, agree))


# ── output ───────────────────────────────────────────────────────────────────

def check(conf, label, ok, detail=""):
    v = "MATCH" if ok else "MISS"
    print("%s (%s) %s%s" % (v, conf, label, (": " + detail) if detail and not ok else ""))
    verdicts.append((v, conf, label, detail if not ok else ""))


def esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def annotate(kind, title, lines):
    """One annotation per chunk of at most LIMIT characters."""
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
        t = title if len(chunks) == 1 else "%s (%d/%d)" % (title, i + 1, len(chunks))
        pending.append("::%s title=%s::%s" % (kind, esc(t).replace(":", "%3A")
                                              .replace(",", "%2C"), esc(c)))


def flush(first):
    """Print the annotations, those of `first` first: the runner keeps ten
    of each kind per step, so the verdicts must not come last."""
    global pending
    head, rest = pending[first:], pending[:first]
    for a in head + rest:
        print(a)
    pending = []


def summary(lines):
    if SUMMARY:
        with open(SUMMARY, "a") as fh:
            fh.write("\n".join(lines) + "\n\n")


def run_lines(run, data, pf, proved):
    p = prefix(RUNS[run][1])
    lines = []
    if not proved:
        lines.append("goals per function, generated with -wp-prover none")
    elif data["total"] is None:
        lines.append("no summary line: WP did not run to the end")
    else:
        lines.append("Proved goals: %d / %d; scheduled %s; not proved %d"
                     % (data["proved"], data["total"], data["scheduled"],
                        len(data["not_proved"])))
        np = ["%s [%s]" % (n[len(p):] if n.startswith(p) else n, s)
              for n, s in sorted(data["not_proved"].items())]
        if not np:
            lines.append("not proved: none")
        for i in range(0, len(np), 8):
            lines.append(("not proved: " if i == 0 else "  ") + ", ".join(np[i:i + 8]))
        gline = goals_line(run, data)
        if gline:
            lines.append(gline)
    for gname, _ in functions(run):
        if gname not in pf:
            lines.append("%s: not counted (no entry in the statistics report)" % gname)
            continue
        v, t, props = pf[gname]
        if proved:
            body = ", ".join("%s %d/%d" % (k, pv, pt) for k, (pv, pt) in sorted(props.items()))
            lines.append("%s %d/%d: %s" % (gname, v, t, body))
        else:
            body = ", ".join("%s %d" % (k, pt) for k, (_, pt) in sorted(props.items()))
            lines.append("%s %d: %s" % (gname, t, body))
    return lines


def post(run, data, pf, proved):
    lines = run_lines(run, data, pf, proved)
    print("=== %s: %s, %s" % (run, RUNS[run][0], RUNS[run][1]))
    for line in lines:
        print("  " + line)
    annotate("notice", "G11 %s (%s, %s)" % (run, os.path.basename(RUNS[run][0]),
                                           RUNS[run][1]), lines)
    summary(["### %s: %s, %s" % (run, RUNS[run][0], RUNS[run][1])]
            + ["- " + line for line in lines])


# ── A: the recommended contracts ─────────────────────────────────────────────

def expected_A(run):
    """The run-time check on the pointer at each combinator, named as today."""
    p = prefix(RUNS[run][1])
    exp = set()
    for gname, _, cs in RUNS[run][2]:
        for c in cs:
            if gname.startswith("option") and c == "eq":
                exp.add(p + gname + "_eq_assert_rte_function_pointer_part2")
            else:
                exp.add(p + gname + "_" + c + "_assert_rte_function_pointer")
            if gname.startswith("result") and c == "eq":
                exp.add(p + gname + "_eq_assert_rte_function_pointer_2")
    return exp


CLASSES = [
    ("precondition", r"_call_option_int_map_requires(_\d+)?(_part\d+)?$"),
    ("termination", r"_terminates(_part\d+)?$"),
    ("exit", r"_exits(_part\d+)?$"),
    ("frame on the normal path", r"_assigns_normal(_part\d+)?$"),
    ("frame on the exit path", r"_assigns_exit(_part\d+)?$"),
    ("ensures g == 0", r"_ensures(_part\d+)?$"),
]
# The statistics name a client's properties without the part: the class
# whose goals a property holds, for when there is no per-goal report.
STAT_CLASS = [("precondition", "requires"), ("termination", "terminates"),
              ("exit", "exits"), ("frame on the exit path", "assigns"),
              ("frame on the normal path", "assigns"), ("ensures g == 0", "ensures")]
# (client, class): (predicted not proved, confidence)
CLIENTS = {
    "client_some": {c: (True, "H") for c, _ in CLASSES},
    "client_none": {"precondition": (False, "H"),
                    "termination": (False, "M"),
                    "exit": (False, "M"),
                    "frame on the normal path": (True, "H"),
                    "frame on the exit path": (False, "H"),
                    "ensures g == 0": (True, "H")},
}


def generated(run, data, pf, client):
    """{class: True or False} for whether WP generated a goal of that class
    for the client, from the per-goal report or, without one, the
    statistics; None for a class neither can tell."""
    gl = data["goals"]
    if isinstance(gl, list):
        names = [str(g.get("goal")) for g in gl if isinstance(g, dict)]
        names = [n for n in names if owner(run, n) == client]
        return {c: any(re.search(rx, n) for n in names) for c, rx in CLASSES}
    props = pf.get(client, (0, 0, {}))[2]
    return {c: (any(key in k and t > 0 for k, (_, t) in props.items()) if props else None)
            for c, key in STAT_CLASS}


def report_A():
    for run in ("A1", "A2"):
        data = read_run(run)
        pf = per_function(run, data)
        post(run, data, pf, True)
        np = data["not_proved"]
        bad = sorted(n for n, s in np.items() if s in BAD)
        check("H", run + ": no goal Failed, Failure, Invalid, Stepout or NoResult",
              not bad, ", ".join(bad))
        others = sorted(n for n in np if owner(run, n) is None)
        if data["total"] is None:
            check("H", run + ": the goals listed as not proved account for the summary "
                  "line, all of the selected functions", False, "no summary line")
        else:
            left = data["total"] - data["proved"]
            check("H", run + ": the goals listed as not proved account for the summary "
                  "line, all of the selected functions",
                  left == len(np) and not others,
                  "%d listed, %d / %d; of other functions: %s"
                  % (len(np), data["proved"], data["total"], others))
        comb = {n for n in np if owner(run, n) and not owner(run, n).startswith("client_")}
        exp = expected_A(run)
        check("H", run + ": the 18 run-time checks on the pointer are not proved",
              exp <= comb, "proved: %s" % sorted(exp - comb))
        rest = comb - exp
        check("H", run + ": every other goal of the sixteen combinators is proved",
              not rest, ", ".join(sorted(rest)))
        for cl in ("client_some", "client_none"):
            names = [n for n in np if owner(run, n) == cl]
            found, stray = {}, []
            for n in names:
                k = [c for c, rx in CLASSES if re.search(rx, n)]
                if len(k) == 1:
                    found.setdefault(k[0], []).append(n)
                else:
                    stray.append(n)
            gen = generated(run, data, pf, cl)
            for c, _ in CLASSES:
                want, conf = CLIENTS[cl][c]
                got = c in found
                label = "%s %s(): its %s is %s" % (run, cl, c, "not proved" if want else "proved")
                if want:
                    check(conf, label, got, "not listed")
                else:
                    exists = gen.get(c)
                    check(conf, label, not got and exists is True,
                          ", ".join(found.get(c, [])) if got else
                          "no such goal generated" if exists is False else
                          "neither report shows such a goal")
            check("H", "%s %s(): every goal it leaves unproved is of a class above"
                  % (run, cl), not stray, ", ".join(stray))


# ── B: today's goals, counted ────────────────────────────────────────────────

def report_B():
    for run in ["B%d" % i for i in range(1, 8)]:
        data = read_run(run)
        pf = per_function(run, data)
        post(run, data, pf, False)
        missing = [g for g, _ in functions(run) if g not in pf]
        check("H", run + ": every selected function has its goals counted", not missing,
              ", ".join(missing))


# ── compare: today against the recommended contracts ─────────────────────────

KINDS = ("assert_rte_", "terminates", "exits")
# The behaviours that call no pointer: one per combinator, two at option's eq.
NONCALLING = {("option", "eq"): 2}
# (today, recommended, instantiation today, in the probe, today's contract
# written). A written contract keeps every clause but the frame.
PAIRS = [("B1", "A1", "option_int", "option_int", True),
         ("B3", "A1", "option_int", "option_int", True),
         ("B4", "A2", "option_int", "option_int", True),
         ("B5", "A2", "option_usize", "option_int", True),
         ("B6", "A1", "option_isize", "option_int", True),
         ("B2", "A1", "result_int_VErr", "result_int_VErr", True),
         ("B3", "A1", "result_Bool_Error", "result_Bool_Error", True),
         ("B4", "A2", "result_Bool_Error", "result_Bool_Error", False),
         ("B7", "A2", "result_Bool_Error", "result_Bool_Error", False)]


def is_frame(k):
    return k == "assigns" or k.endswith("_assigns")


def report_compare():
    pf = {r: per_function(r, read_run(r)) for r in RUNS}
    lines, kinds, frames = [], [], []
    for b, a, tb, ta, written in PAIRS:
        family = "option" if tb.startswith("option") else "result"
        row, dsum = [], 0
        for c in (OPT if family == "option" else RES):
            fb, fa = pf[b].get(tb + "_" + c), pf[a].get(ta + "_" + c)
            if fb is None or fa is None:
                row.append("%s ?" % c)
                kinds.append("%s/%s %s: not counted" % (b, a, c))
                if written:
                    frames.append("%s/%s %s: not counted" % (b, a, c))
                continue
            dsum += fa[1] - fb[1]
            row.append("%s %d->%d" % (c, fb[1], fa[1]))
            for k in sorted(set(fb[2]) | set(fa[2])):
                nb, na = fb[2].get(k, (0, 0))[1], fa[2].get(k, (0, 0))[1]
                if k.startswith(KINDS):
                    if nb != na:
                        kinds.append("%s/%s %s %s %d->%d" % (b, a, c, k, nb, na))
                elif written and not is_frame(k) and nb != na:
                    frames.append("%s/%s %s %s %d->%d" % (b, a, c, k, nb, na))
            if written:
                n = NONCALLING.get((family, c), 1)
                f_b = sum(t for k, (_, t) in fb[2].items() if is_frame(k))
                f_a = sum(t for k, (_, t) in fa[2].items() if is_frame(k))
                if f_a != n * f_b:
                    frames.append("%s/%s %s frame %d->%d, predicted %d"
                                  % (b, a, c, f_b, f_a, n * f_b))
        lines.append("%s %s -> %s %s: %s; total %+d" % (b, tb, a, ta, ", ".join(row), dsum))
    print("=== goals per combinator, today (B) -> recommended (A)")
    for line in lines:
        print("  " + line)
    annotate("notice", "G11 goals per combinator, today (B) -> recommended (A)", lines)
    summary(["### Goals per combinator, today (B) -> recommended (A)"]
            + ["- " + line for line in lines])
    check("M", "run-time checks, termination goals and exit goals: as many under the "
          "recommended contract as today, at every combinator", not kinds, "; ".join(kinds))
    check("M", "where today's contract is written, the frame's goals move into each "
          "behaviour that calls no pointer and every other clause keeps its goals",
          not frames, "; ".join(frames))


def main():
    if sys.argv[1:] == ["run"]:
        run_all()
        return
    if len(sys.argv) != 3 or sys.argv[1] != "report" or sys.argv[2] not in ("A", "B", "compare"):
        print(__doc__)
        sys.exit(2)
    group = sys.argv[2]
    {"A": report_A, "B": report_B, "compare": report_compare}[group]()
    for n in notes:
        print("NOTE: " + n)
    vlines = ["%s (%s) %s" % (v, c, l) for v, c, l, _ in verdicts] + ["NOTE: " + n for n in notes]
    first = len(pending)
    annotate("notice", "G11 report %s: verdicts" % group, vlines)
    missed = [(c, l, d) for v, c, l, d in verdicts if v == "MISS"]
    summary(["### Report %s: verdicts" % group]
            + ["- %s (%s) %s%s" % (v, c, l, (": " + d) if d else "") for v, c, l, d in verdicts]
            + ["- NOTE: " + n for n in notes])
    for i, (c, l, d) in enumerate(missed[:9]):
        annotate("error", "G11 report %s: MISS (%s)" % (group, c), [l + (": " + d if d else "")])
    if len(missed) > 9:
        annotate("error", "G11 report %s: MISS" % group,
                 ["and %d more MISS; see the verdicts notice" % (len(missed) - 9)])
    flush(first)
    if missed:
        sys.exit(1)
    print("Every part of report %s MATCHES." % group)


main()
