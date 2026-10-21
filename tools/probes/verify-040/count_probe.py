#!/usr/bin/env python3
"""
count_probe.py — the WP run of .github/workflows/count-probe.yml (VERIFY-040
round 2) and its reading against the committed prediction in
docs/deviations.md, VERIFY-040: "the count probe: committed prediction".

    python3 tools/probes/verify-040/count_probe.py run
        Runs WP as frama-c-vec runs vmacros/vdrivers/vec_verify.h
        (Typed+Cast, -wp-rte, -wp-split, the same preprocessing), on its two
        functions that round 2 changed, vec_int_append_array and
        vec_int_extend, with Qed, WP's simplifier, as the only prover
        (-wp-prover qed) and with -wp-print, twice: on the driver as
        committed (run H) and on the driver as it was at bd9466e, before
        round 2 (run B), which git writes at the repository root as
        wp_B_vec_verify.h. Only the two functions' contracts differ between
        the runs. Qed alone runs no external prover, and WP still schedules
        the goals, so that it prints its summary line, "Proved goals: P / T",
        as the CI's jobs read it. Output, for each run X: wp_X.txt, the log
        with the printed goals; wp_X.json, the statistics per function and
        property (WP 29's -wp-deprecated-report-json, which WP 29 writes
        whether or not a prover runs; WP 25's -wp-report-json); and on WP 29
        wp_X_goals.json, the per-goal report of -wp-report-json, which WP 29
        writes only when it schedules goals for a prover, as for Qed here.
        Exits 1 if a run fails or writes no statistics, with each run's last
        lines posted as an error annotation.

    python3 tools/probes/verify-040/count_probe.py report
        reads that output and prints MATCH or MISS, with its confidence, for
        each predicted part, from the summary lines, the statistics and the
        print; goal names, where WP writes them, are posted and not scored.
        A part with no data to read is NO DATA, not a MISS. The log needs a
        sign-in, so it also posts each run's goals property by property, the
        names of extend's frames, and the printed goals of extend's frames,
        their titles, their effects and their conclusions, as notices on the
        run page (each under the runner's 4096-character cut), an error
        annotation for each MISS or NO DATA, and everything to the step
        summary. Exits 1 on a MISS, else 2 on NO DATA.

The question: CI #1375 and CC #56 measured five goals more in each vec unit
than at round 1's pins, where round 2's prediction, which counted WP 25's
parts, said three. The CI's logs print no proved goal, so they do not say
which clause WP 29 splits otherwise than WP 25. No external prover is
needed to count goals: WP generates them, and Qed simplifies them, before
any other prover.

Run from the repository root, with frama-c on PATH and the repository's
history (the workflow checks out with fetch-depth 0).
"""
import json
import os
import re
import subprocess
import sys
import time

DRIVER = "vmacros/vdrivers/vec_verify.h"
BASE = "bd9466e"
RUNS = {"H": DRIVER, "B": "wp_B_vec_verify.h"}
LABEL = {"H": "the driver as committed", "B": "the driver at %s, before round 2" % BASE}
FUNCS = ["vec_int_append_array", "vec_int_extend"]
CPP = "-I core/primitives -I core -I semantics -I data -I ."
DEFS = "-DCANON_NO_REQUIRE -DNDEBUG"
P = "typed_cast_"
CI_DELTA = 5        # CI #1375: 5574 goals in frama-c-vec, from 5569 at CI #1374
LIMIT = 3800        # the runner cuts an annotation's message at 4096
SUMMARY = os.environ.get("GITHUB_STEP_SUMMARY")
verdicts = []       # (MATCH, MISS or NO DATA, confidence, label, detail)
notes = []
pending = []        # annotations, printed at the end, the verdicts first


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
    old = subprocess.run(["git", "show", "%s:%s" % (BASE, DRIVER)],
                         capture_output=True, text=True)
    if old.returncode != 0:
        print("git show %s:%s failed: %s" % (BASE, DRIVER, old.stderr.strip()))
        sys.exit(1)
    with open(RUNS["B"], "w") as fh:
        fh.write(old.stdout)
    failed = []
    for run in ("H", "B"):
        for stale in ("wp_%s.txt" % run, "wp_%s.json" % run, "wp_%s_goals.json" % run):
            if os.path.exists(stale):
                os.remove(stale)
        cmd = ["frama-c", "-wp", "-wp-rte", "-wp-model", "Typed+Cast",
               "-wp-prover", "qed", "-wp-split", "-wp-print",
               "-wp-fct", ",".join(FUNCS)]
        if stats_opt:
            cmd += [stats_opt, "wp_%s.json" % run]
        if goals_opt:
            cmd += [goals_opt, "wp_%s_goals.json" % run]
        cmd += ["-cpp-extra-args=%s %s" % (CPP, DEFS), RUNS[run]]
        print("=== %s: %s, %s" % (run, RUNS[run], LABEL[run]))
        print("    " + " ".join(cmd))
        sys.stdout.flush()
        start = time.time()
        with open("wp_%s.txt" % run, "w") as out:
            status = subprocess.run(cmd, stdout=out, stderr=subprocess.STDOUT).returncode
        print("    exit status %d, %d s" % (status, time.time() - start))
        with open("wp_%s.txt" % run, errors="replace") as fh:
            for line in fh:
                if re.match(r"^\[wp\] (Proved goals:|\d+ goals scheduled)", line):
                    print("    " + line.rstrip())
        for js in ("wp_%s.json" % run, "wp_%s_goals.json" % run):
            print("    %s: %s" % (js, "written" if os.path.exists(js) else "absent"))
        if status != 0:
            failed.append("%s exited %d" % (run, status))
        if not os.path.exists("wp_%s.json" % run):
            failed.append("%s wrote no statistics" % run)
        sys.stdout.flush()
    if failed:
        print("FAILED: %s; see wp_H.txt and wp_B.txt" % "; ".join(failed))
        # The log needs a sign-in: post why, with each run's last lines.
        for run in ("H", "B"):
            try:
                with open("wp_%s.txt" % run, errors="replace") as fh:
                    tail = fh.read().splitlines()[-12:]
            except OSError:
                tail = ["no output file"]
            msg = ("; ".join(failed) + "\n" + "\n".join(tail))[-LIMIT:]
            print("::error title=count probe WP run %s::%s" % (run, esc(msg)))
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


def owner(goal):
    """The function of FUNCS a goal belongs to, or None."""
    if not goal.startswith(P):
        return None
    rest = goal[len(P):]
    best = None
    for f in FUNCS:
        if rest.startswith(f + "_") and (best is None or len(f) > len(best)):
            best = f
    return best


def read_run(run):
    out = {"proved": None, "total": None, "scheduled": None, "printed": [],
           "stats": None, "goals": None, "logged": []}
    log = "wp_%s.txt" % run
    if not os.path.exists(log):
        notes.append("%s: no output file" % run)
        return out
    with open(log, errors="replace") as fh:
        lines = fh.read().split("\n")
    for line in lines:
        m = re.match(r"^\[wp\] Proved goals:\s*(\d+)\s*/\s*(\d+)", line)
        if m:
            out["proved"], out["total"] = int(m.group(1)), int(m.group(2))
        m = re.match(r"^\[wp\] (\d+) goals scheduled", line)
        if m:
            out["scheduled"] = int(m.group(1))
        # A goal's result: WP 25 prints each goal so with a prover, "[wp]
        # [Qed] Goal <name> : Valid" or "[wp] [Failed] Goal <name>"; WP 29
        # prints only the goals not proved, "[wp] [Timeout] <name>".
        m = re.match(r"^\[wp\] \[[A-Za-z ]+\] (?:Goal )?(typed_\S+)", line)
        if m:
            out["logged"].append(m.group(1))
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
    out["stats"] = load_json("wp_%s.json" % run, run)
    out["goals"] = load_json("wp_%s_goals.json" % run, run)
    return out


def counts(entry):
    """total of one property or section of the statistics report."""
    if not isinstance(entry, dict):
        return None
    m = entry.get("wp:main", entry)
    if not isinstance(m, dict) or "total" not in m:
        return None
    return int(m["total"])


def per_function(run, data):
    """{function: (total, {property, less the model and the function's name:
    total})}."""
    res = {}
    js = data["stats"]
    fns = js.get("wp:functions") if isinstance(js, dict) else None
    if not isinstance(fns, dict):
        notes.append("%s: no statistics report with 'wp:functions'" % run)
        return res
    for f in FUNCS:
        e = fns.get(f)
        if not isinstance(e, dict):
            notes.append("%s: %s has no entry in the statistics report" % (run, f))
            continue
        props = {}
        for k, v in e.items():
            if k == "wp:section":
                continue
            c = counts(v)
            if c is None:
                continue
            k = re.sub(r"^typed(?:_cast|_ref)?_", "", k)
            k = k[len(f) + 1:] if k.startswith(f + "_") else k
            props[k] = props.get(k, 0) + c
        sec = counts(e.get("wp:section"))
        res[f] = (sec if sec is not None else sum(props.values()), props)
    return res


def total(pf, f, prop):
    """The statistics' goals of f's property prop, a frame's paths and a
    split goal's parts summed, or None where the report has no such key."""
    if f not in pf:
        return None
    rx = re.compile(r"^%s(?:_(?:normal|exit))?(?:_part\d+)?$" % re.escape(prop))
    ks = [k for k in pf[f][1] if rx.match(k)]
    return sum(pf[f][1][k] for k in ks) if ks else None


def names_of(data):
    """(the run's goal names of FUNCS, where they come from): the per-goal
    report's (WP 29), else the log's where it names every scheduled goal
    (WP 25 with a prover), else the print's (a local WP 25 that prints GID
    lines), else the log's names of the goals not proved (WP 29 without its
    per-goal report)."""
    gl = data["goals"]
    if isinstance(gl, list):
        names = [str(g.get("goal")) for g in gl if isinstance(g, dict) and "goal" in g]
        src = "per-goal report"
    elif data["scheduled"] is not None and len(data["logged"]) == data["scheduled"]:
        names, src = data["logged"], "log, every scheduled goal"
    elif any(g["gid"] for g in data["printed"]):
        names, src = [g["gid"] for g in data["printed"] if g["gid"]], "printed names"
    else:
        names, src = data["logged"], "log, the goals not proved only"
    return [n for n in names if owner(n)], src


def parts(names, f, prop):
    """The goals of f's property prop (less the function's name), by name:
    the property itself or its parts, a frame's paths included."""
    rx = re.compile(r"^%s%s_%s(?:_(?:normal|exit))?(?:_part\d+)?$"
                    % (re.escape(P), re.escape(f), re.escape(prop)))
    return sorted(n for n in names if rx.match(n))


def effect_of(g):
    return next((l.strip() for l in g["body"]
                 if re.match(r"^\s*(Call Effect|Call Result|Effect|Stmt|Loop)", l)), "")


def frame_goals(data, f, bhv):
    """The printed goals of f's frame, the default one or a behaviour's, in
    print order: [(title, effect, conclusion)]."""
    head = "Assigns for '%s'" % bhv if bhv else "Assigns "
    out = []
    for g in data["printed"]:
        t = g["title"]
        if not t.startswith(head) or ("in '%s'" % f) not in t:
            continue
        if not bhv and t.startswith("Assigns for"):
            continue
        concl = []
        for i, l in enumerate(g["body"]):
            if l.startswith("Prove:"):
                concl = [x.strip() for x in g["body"][i:i + 3] if x.strip()]
                break
        out.append((t, effect_of(g), " ".join(concl)))
    return out


def by_path(printed):
    """The printed goals of one frame grouped as WP splits them, one group
    per path, normal or exit: a title ending "(k/n)" is part k of a goal in
    n parts, so a group starts at "(1/n)", and a title with no part is a
    goal unsplit, a group of one."""
    groups = []
    for t, effect, concl in printed:
        m = re.search(r"\((\d+)/(\d+)\)$", t)
        k, n = (int(m.group(1)), int(m.group(2))) if m else (1, 1)
        if k == 1 or not groups or groups[-1]["n"] != n or len(groups[-1]["goals"]) >= n:
            groups.append({"n": n, "goals": []})
        groups[-1]["goals"].append((t, effect, concl))
    return groups


# ── reporting ────────────────────────────────────────────────────────────────

def check(conf, label, ok, detail=""):
    """ok: True, False, or None where there is no data to score."""
    v = "NO DATA" if ok is None else "MATCH" if ok else "MISS"
    print("%s (%s) %s%s" % (v, conf, label, (": " + detail) if detail else ""))
    verdicts.append((v, conf, label, detail))


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
    """Print the annotations, those from `first` first: the runner keeps ten
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


def short(n):
    return n[len(P):] if n.startswith(P) else n


def post(run, data, pf, names, src):
    lines = []
    if data["total"] is None:
        lines.append("no 'Proved goals' line")
    else:
        lines.append("Proved goals: %d / %d, by Qed alone; scheduled %s"
                     % (data["proved"], data["total"], data["scheduled"]))
    lines.append("named goals of the two functions: %d (%s)" % (len(names), src))
    for f in FUNCS:
        if f not in pf:
            continue
        t, props = pf[f]
        lines.append("%s %d: %s" % (f, t, ", ".join(
            "%s %d" % (k, n) for k, n in sorted(props.items()))))
    for bhv in (None, "ok"):
        prop = "ok_assigns" if bhv else "assigns"
        ns = parts(names, "vec_int_extend", prop)
        lines.append("vec_int_extend's %s by name: %s"
                     % (prop, ", ".join(short(n) for n in ns) or "none"))
    print("=== %s: %s, %s" % (run, RUNS[run], LABEL[run]))
    for line in lines:
        print("  " + line)
    annotate("notice", "count probe %s (%s): goals" % (run, LABEL[run]), lines)
    summary(["### %s: %s, %s" % (run, RUNS[run], LABEL[run])] + ["- " + x for x in lines])
    shown = []
    for bhv in (None, "ok"):
        for t, effect, concl in frame_goals(data, "vec_int_extend", bhv):
            shown.append("%s | %s | %s" % (t, effect or "(no effect line)", concl or "(no conclusion)"))
    if not shown:
        # No frame matched: post the titles WP printed for extend instead.
        shown = ["no frame goal matched; extend's printed titles:"] + [
            g["title"] for g in data["printed"] if "in 'vec_int_extend'" in g["title"]][:60]
    print("--- %s: vec_int_extend's frames, as printed" % run)
    for s in shown:
        print("  " + s)
    annotate("notice", "count probe %s: vec_int_extend's frames, as printed" % run, shown)
    summary(["### %s: vec_int_extend's frames, as printed" % run] + ["- " + x for x in shown])


def predictions(dH, dB, pH, pB):
    # Part 1 (H): the CI's measure is the summary line, which counts
    # terminating and unreachable entries too; the statistics count goals by
    # property. Scored by the summary lines where both runs print one.
    sH = sum(pH[f][0] for f in FUNCS)
    sB = sum(pB[f][0] for f in FUNCS)
    stat = "statistics: H %d, B %d, %+d" % (sH, sB, sH - sB)
    if dH["total"] is not None and dB["total"] is not None:
        d = dH["total"] - dB["total"]
        check("H", "the two functions' goals, H less B: %d, CI #1375's difference" % CI_DELTA,
              d == CI_DELTA, "summary lines: H %d, B %d, %+d; %s"
              % (dH["total"], dB["total"], d, stat))
        if d != sH - sB:
            notes.append("the summary lines' difference, %+d, is not the statistics', %+d"
                         % (d, sH - sB))
    else:
        check("H", "the two functions' goals, H less B: %d, CI #1375's difference" % CI_DELTA,
              sH - sB == CI_DELTA, "no summary line in both runs; " + stat)
    # Part 2 (M): from the statistics, the parts of each property.
    e = {k: total(pH, "vec_int_append_array", k)
         for k in ("ok_ensures_3", "ok_ensures_4", "ok_ensures_5")}
    x = {k: total(pH, "vec_int_extend", k) for k in ("ok_ensures_4", "ok_ensures_5")}
    vals = list(e.values()) + list(x.values())
    check("M", "H: append_array's ok_ensures_5 in six parts, as its ok_ensures_3 and "
          "ok_ensures_4, and extend's ok_ensures_4 and ok_ensures_5 one goal each",
          None if None in vals else
          (all(v == 6 for v in e.values()) and all(v == 1 for v in x.values())),
          "append_array %s; extend %s" % (
              ", ".join("%s %s" % kv for kv in sorted(e.items())),
              ", ".join("%s %s" % kv for kv in sorted(x.items()))))
    # Part 3 (M): from the statistics.
    okB = total(pB, "vec_int_extend", "ok_assigns")
    check("M", "B: extend's ok frame in three goals, where WP 25 has five",
          None if okB is None else okB == 3, "statistics: %s" % okB)
    # Part 4 (L): from the print, the frame's goals grouped by path, normal or
    # exit.
    printed = frame_goals(dB, "vec_int_extend", "ok")
    groups = by_path(printed)
    eff = [sum(1 for _, ef, _ in g["goals"] if ef.startswith("Call Effect")) for g in groups]
    res = sum(1 for _, ef, _ in printed if ef.startswith("Call Result"))
    check("L", "B: of the ok frame's printed goals, one call effect on each of its normal and "
          "exit paths and one call result, where WP 25 prints two effects on each",
          None if not printed else
          ((okB is None or len(printed) == okB) and len(groups) == 2 and eff == [1, 1]
           and res == 1),
          "%d printed (statistics %s), on %d path(s) of %s goal(s), call effects %s, "
          "call results %d" % (len(printed), okB, len(groups),
                               "/".join(str(len(g["goals"])) for g in groups),
                               "/".join(str(n) for n in eff), res))


def main():
    if sys.argv[1:] == ["run"]:
        run_all()
        return
    if sys.argv[1:] != ["report"]:
        print(__doc__)
        sys.exit(2)
    dH, dB = read_run("H"), read_run("B")
    pH, pB = per_function("H", dH), per_function("B", dB)
    (nH, srcH), (nB, srcB) = names_of(dH), names_of(dB)
    post("H", dH, pH, nH, srcH)
    post("B", dB, pB, nB, srcB)
    if all(f in pH for f in FUNCS) and all(f in pB for f in FUNCS):
        predictions(dH, dB, pH, pB)
    else:
        check("-", "the runs' statistics for both functions, which every part reads",
              None, "H %s, B %s" % (sorted(pH), sorted(pB)))
    for n in notes:
        print("NOTE: " + n)
    vlines = ["%s (%s) %s%s" % (v, c, l, (": " + d) if d else "")
              for v, c, l, d in verdicts] + ["NOTE: " + n for n in notes]
    first = len(pending)
    annotate("notice", "count probe: verdicts", vlines)
    bad = [(v, c, l, d) for v, c, l, d in verdicts if v != "MATCH"]
    summary(["### Verdicts"] + ["- " + x for x in vlines])
    for v, c, l, d in bad[:9]:
        annotate("error", "count probe: %s (%s)" % (v, c), [l + (": " + d if d else "")])
    flush(first)
    if any(v == "MISS" for v, _, _, _ in bad):
        sys.exit(1)
    if bad:
        sys.exit(2)
    print("Every part of the count probe MATCHES.")


main()
