#!/usr/bin/env python3
"""
g19_probe.py — the WP run of .github/workflows/g19-probe.yml (VERIFY-039
G19) and its reading against the committed prediction in
docs/deviations.md, VERIFY-039: "G19 — committed prediction for the probe",
for the run named F, and "G19 and G20 — the repair: committed prediction".

    python3 tools/probes/verify-039/g19_probe.py run
        Runs WP as frama-c-region runs region.h (Typed+Cast, -wp-rte,
        -wp-split, Alt-Ergo, Z3 and CVC5 at 120 s), with -wp-print, on
        tools/probes/verify-039/p_loop_frame_bhv.c: f(), whose loop's frame
        is stated for one behaviour and whose postcondition is false for the
        other, and g(), the control, its frame stated for the behaviour in
        which the loop writes. Output: wp_F.txt, the log with the printed
        goals; wp_F.json, the statistics per function and property (WP 29's
        -wp-deprecated-report-json, WP 25's -wp-report-json); and on WP 29
        wp_F_goals.json, the per-goal report of -wp-report-json.

    python3 tools/probes/verify-039/g19_probe.py report F
        reads that output and prints MATCH or MISS, with its confidence, for
        each predicted part. The log needs a sign-in, so it also posts the
        verdicts, each function's goals property by property, and each goal
        not proved as WP printed it, its title, its effect and its
        conclusion, as notices on the run page (each under the runner's
        4096-character cut), an error annotation for each MISS (up to nine,
        and a count of the rest), and everything to the step summary. Exits
        1 on a MISS.

The probe's first run, at 8304c61, also ran WP on core/region.h (T) and on
two copies of it with the contracts G19's and G20's decisions recommend, the
loop's frame stated for the executions that call no hook (R) and for every
execution (U); G19's repair wrote R's contracts into core/region.h, and
frama-c-region pins their goals since. F stays: it shows how WP reads a
loop's frame stated for one behaviour, which region_end's loop states, and a
WP that read it otherwise would make F's report MISS.

Run from the repository root, with frama-c on PATH.
"""
import json
import os
import re
import subprocess
import sys
import time

FCT = "tools/probes/verify-039/p_loop_frame_bhv.c"
RUNS = {"F": FCT}
FUNCS = {"F": ["f", "g"]}
LABEL = {"F": "a loop's frame stated for one behaviour"}
CPP = "-I core/primitives -I core -I ."
DEFS = "-DCANON_NO_REQUIRE -DNDEBUG"
P = "typed_cast_"

NOT_PROVED = "Timeout|Unknown|Failed|Failure|Invalid|Stepout|NoResult"
BAD = {"Failed", "Failure", "Invalid", "Stepout", "NoResult"}
LIMIT = 3800        # the runner cuts an annotation's message at 4096
SUMMARY = os.environ.get("GITHUB_STEP_SUMMARY")
verdicts = []       # (MATCH or MISS, confidence, label, detail)
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
    run = "F"
    cmd = ["frama-c", "-wp", "-wp-rte", "-wp-model", "Typed+Cast",
           "-wp-prover", "alt-ergo,z3,cvc5", "-wp-timeout", "120", "-wp-split",
           "-wp-print", "-wp-fct", ",".join(FUNCS[run])]
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


def owner(run, goal):
    """The function of the run's selection a goal belongs to, or None."""
    if not goal.startswith(P):
        return None
    rest = goal[len(P):]
    best = None
    for f in FUNCS[run]:
        if rest.startswith(f + "_") and (best is None or len(f) > len(best)):
            best = f
    return best


def read_run(run):
    out = {"proved": None, "total": None, "scheduled": None, "not_proved": {},
           "printed": [], "stats": None, "goals": None}
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
        m = re.match(r"^\[wp\] \[(%s)\] (?:Goal )?(typed\S*)" % NOT_PROVED, line)
        if m and m.group(2) not in out["not_proved"]:
            out["not_proved"][m.group(2)] = m.group(1)
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
        notes.append("%s: the statistics report has no 'wp:functions'" % run)
        return res
    for f in FUNCS[run]:
        e = fns.get(f)
        if not isinstance(e, dict):
            continue
        props = {}
        for k, v in e.items():
            if k == "wp:section":
                continue
            c = counts(v)
            if c is None:
                continue
            if k.startswith(f + "_"):
                k = k[len(f) + 1:]
            props[k] = c
        sec = counts(e.get("wp:section"))
        if sec is None:
            sec = (sum(v for v, _ in props.values()), sum(t for _, t in props.values()))
        res[f] = (sec[0], sec[1], props)
    return res


def goal_list(data):
    """[(name, verdict, entry)] from the per-goal report, or None."""
    gl = data["goals"]
    if not isinstance(gl, list):
        return None
    return [(str(g.get("goal")), str(g.get("verdict", "")).lower(), g)
            for g in gl if isinstance(g, dict) and "goal" in g]


def names_of(data):
    """Every goal the run named: the per-goal report's, else WP 25's print's."""
    gl = goal_list(data)
    if gl is not None:
        return [(n, e) for n, _, e in gl]
    return [(g["gid"], {}) for g in data["printed"] if g["gid"]]


# ── the printed goals, by name ───────────────────────────────────────────────

KINDS = [  # (pattern on the name after the function's, the title's kind),
    # tried in order: a loop's frame before a function's
    (r"^(?:\w+?_)?loop_assigns(?:_\d+)?(?:_part\d+)?$", "Loop assigns"),
    (r"^(?:\w+?_)?assigns(?:_normal|_exit)?(?:_part\d+)?$", "Assigns"),
    (r"^loop_invariant(?:_\d+)?_preserved(?:_part\d+)?$", "Preservation of Invariant"),
    (r"^loop_invariant(?:_\d+)?_established(?:_part\d+)?$", "Establishment of Invariant"),
    (r"^loop_variant(?:_\d+)?_decrease(?:_part\d+)?$", "Decreasing of Loop variant"),
    (r"^loop_variant(?:_\d+)?_positive(?:_part\d+)?$", "Positivity of Loop variant"),
    (r"^assert_rte_\w+?(?:_\d+)?(?:_part\d+)?$", "Assertion 'rte,"),
    (r"^call_\w+?_requires(?:_\d+)?(?:_part\d+)?$", "at call '"),
    (r"^terminates(?:_part\d+)?$", "Termination-condition"),
    (r"^exits(?:_part\d+)?$", "Exit-condition"),
    (r"^(?:\w+?_)?ensures(?:_\d+)?(?:_part\d+)?$", "Post-condition"),
    (r"^complete_", "Complete behaviors"),
    (r"^disjoint_", "Disjoint behaviors"),
]


def title_key(title):
    """(lines named in the title, part, parts) of a printed goal."""
    lines = [int(x) for x in re.findall(r"line (\d+)", title)]
    m = re.search(r"\((\d+)/(\d+)\)$", title)
    return lines, (int(m.group(1)) if m else None), (int(m.group(2)) if m else None)


def family(name):
    """A split goal's name less its part."""
    return re.sub(r"_part\d+$", "", name)


def candidates(run, name, entry, printed, parts):
    """The printed goals that can be name's, from its kind, its function, its
    behaviour, its line and its part, and for a part, its family's number of
    parts (parts: {family: how many}), which tells a frame's normal path from
    its exit path; WP 25's printer names them."""
    named = [g for g in printed if g["gid"] == name]
    if named:
        return named
    f = owner(run, name)
    if f is None:
        return []
    rest = name[len(P) + len(f) + 1:]
    kind = next((k for rx, k in KINDS if re.match(rx, rest)), None)
    if kind is None:
        return []
    pm = re.search(r"_part(\d+)$", rest)
    part = int(pm.group(1)) if pm else None
    line = entry.get("line") if isinstance(entry, dict) else None
    bhv = entry.get("behavior") if isinstance(entry, dict) else None
    # A behaviour no printed title names is the default one, whatever the
    # report calls it ("default!" is Frama-C's name for it).
    if bhv not in {b for g in printed for b in re.findall(r" for '(\w+)'", g["title"])}:
        bhv = None
    out = []
    for g in printed:
        t = g["title"]
        if kind not in t:
            continue
        if kind == "at call '":
            call = re.match(r"^call_(\w+?)_requires", rest).group(1)
            if ("at call '%s'" % call) not in t:
                continue
        if kind == "Assertion 'rte,":
            rk = re.match(r"^assert_rte_(\w+?)(?:_\d+)?(?:_part\d+)?$", rest).group(1)
            if ("'rte,%s'" % rk) not in t:
                continue
        if kind in ("Complete behaviors", "Disjoint behaviors"):
            # complete_nonzero_zero: "Complete behaviors 'nonzero', 'zero'"
            if "_".join(re.findall(r"'(\w+)'", t)) != rest.split("_", 1)[1]:
                continue
        if " in '" in t and ("in '%s'" % f) not in t:
            continue
        if kind in ("Assigns", "Post-condition"):
            fb = re.search(r" for '(\w+)'", t)
            if (fb.group(1) if fb else None) != bhv:
                continue
        lines, p, n = title_key(t)
        if kind.endswith("Loop variant"):
            # The title names the loop's line; the report, the variant's
            # clause, in the loop's annotation just above it.
            if isinstance(line, int) and lines and not any(0 <= x - line <= 10 for x in lines):
                continue
        elif isinstance(line, int) and lines and line not in lines:
            continue
        if part is not None and (p != part or n != parts.get(family(name))):
            continue
        if part is None and p is not None and n != 1:
            continue
        out.append(g)
    return out


def describe(g):
    """The printed goal in one line: its title, its effect, its conclusion."""
    effect = [b.strip() for b in g["body"]
              if re.match(r"^\s*(Call )?(Effect|Result) at line", b)]
    prove, on = [], False
    for b in g["body"]:
        if b.startswith("Prove:"):
            on = True
        if on:
            prove.append(b.strip())
    text = " ".join(prove)
    if len(text) > 240:
        text = text[:236] + " ..."
    return "%s%s; %s" % (g["title"], (" -- " + effect[0]) if effect else "",
                         text or "no conclusion printed")


def mapping(run, data):
    """{name: printed goal or None} for every goal the run named. Names whose
    candidates are the same printed goals take them in order, as for two
    assertions on one line, unless they are parts of two families, as a
    frame's normal and exit paths with as many parts: those are not told
    apart. Otherwise a name needs a candidate of its own."""
    named = names_of(data)
    parts = {}
    for n, _ in named:
        m = re.search(r"_part(\d+)$", n)
        if m:
            parts[family(n)] = max(parts.get(family(n), 0), int(m.group(1)))
    groups = {}
    for n, e in named:
        c = candidates(run, n, e, data["printed"], parts)
        groups.setdefault(tuple(id(x) for x in c), []).append((n, c))
    def order(n):
        # WP numbers a property's goals in the order it prints them:
        # assert_rte_mem_access, then _2, _3, ...
        m = re.search(r"_(\d+)(?:_part\d+)?$", n)
        return int(m.group(1)) if m else 1
    res = {}
    for members in groups.values():
        cands = members[0][1]
        fams = {family(n) for n, _ in members if re.search(r"_part\d+$", n)}
        if cands and len(cands) == len(members) and len(fams) <= 1:
            # ties across functions, as f()'s and g()'s "Complete behaviors
            # 'nonzero', 'zero'", which name neither, go in -wp-fct's order
            key = lambda x: (FUNCS[run].index(owner(run, x[0])) if owner(run, x[0]) else 0,
                             order(x[0]))
            for (n, _), g in zip(sorted(members, key=key), cands):
                res[n] = g
        else:
            for n, _ in members:
                res[n] = None
    return res


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


def post(run, data, pf, mp):
    lines = []
    if data["total"] is None:
        lines.append("no summary line: WP did not run to the end")
    else:
        lines.append("Proved goals: %d / %d; scheduled %s; not proved %d"
                     % (data["proved"], data["total"], data["scheduled"],
                        len(data["not_proved"])))
    gl = goal_list(data)
    if gl is not None:
        valid = sum(1 for _, v, _ in gl if v == "valid")
        other = {n for n, v, _ in gl if v != "valid"}
        lines.append("per-goal report: %d goals, %d valid; its set not valid %s the log's"
                     % (len(gl), valid, "equals" if other == set(data["not_proved"])
                        else "DIFFERS from"))
    unmapped = sorted(short(n) for n, g in mp.items() if g is None)
    lines.append("printed goals: %d; named goals not identified among them: %d%s"
                 % (len(data["printed"]), len(unmapped),
                    (" (" + ", ".join(unmapped[:12]) + (", ..." if len(unmapped) > 12 else "")
                     + ")") if unmapped else ""))
    for f in FUNCS[run]:
        if f not in pf:
            lines.append("%s: not counted (no entry in the statistics report)" % f)
            continue
        v, t, props = pf[f]
        lines.append("%s %d/%d: %s" % (f, v, t, ", ".join(
            "%s %d/%d" % (k, pv, pt) for k, (pv, pt) in sorted(props.items()))))
    print("=== %s: %s, %s" % (run, RUNS[run], LABEL[run]))
    for line in lines:
        print("  " + line)
    annotate("notice", "G19 %s (%s): goals" % (run, LABEL[run]), lines)
    summary(["### %s: %s, %s" % (run, RUNS[run], LABEL[run])] + ["- " + x for x in lines])
    shown = []
    for n in sorted(data["not_proved"]):
        g = mp.get(n)
        shown.append("%s [%s]: %s" % (short(n), data["not_proved"][n],
                                      describe(g) if g else "printed goal not identified"))
    print("--- %s: the goals not proved, as printed" % run)
    for s in shown:
        print("  " + s)
    annotate("notice", "G19 %s: the goals not proved, as printed" % run, shown or ["none"])
    summary(["### %s: the goals not proved, as printed" % run] + ["- " + x for x in shown])


def common(run, data):
    np = data["not_proved"]
    bad = sorted(n for n, s in np.items() if s in BAD)
    check("H", run + ": no goal Failed, Failure, Invalid, Stepout or NoResult",
          not bad, ", ".join(bad))
    others = sorted(n for n in np if owner(run, n) is None)
    label = (run + ": WP ran to the end, and the goals listed as not proved account for "
             "its summary, all of the functions selected")
    if data["total"] is None:
        check("H", label, False, "no summary line")
    else:
        left = data["total"] - data["proved"]
        check("H", label, left == len(np) and not others,
              "%d listed, %d / %d; of other functions: %s"
              % (len(np), data["proved"], data["total"], others))


# ── the predictions ──────────────────────────────────────────────────────────

def report_F(data):
    names = [n for n, _ in names_of(data)]
    np = data["not_proved"]
    for fn in ("f", "g"):
        ens = [n for n in names if re.match(r"^typed_cast_%s_ensures(_part\d+)?$" % fn, n)]
        bad = [short(n) for n in ens if n in np]
        check("H", "F: %s()'s postcondition, *p unchanged, is proved" % fn,
              bool(ens) and not bad, ("not proved: " + ", ".join(bad)) if ens else "not generated")
    fs = [n for n in names if owner("F", n) == "f"]
    bad = sorted(short(n) for n in fs if n in np)
    check("M", "F: every goal of f() is proved, its loop's frame among them",
          bool(fs) and not bad and any(re.match(r"^typed_cast_f_(\w+_)?loop_assigns", n) for n in fs),
          "not proved: %s" % ", ".join(bad) if bad else "no loop frame goal generated")
    gs = [n for n in names if owner("F", n) == "g"]
    bad = sorted(short(n) for n in gs if n in np)
    check("M", "F: of g()'s goals one is not proved, its loop's frame at the write to *p",
          len(bad) == 1 and re.match(r"^(\w+_)?loop_assigns", bad[0][len("g_"):]) is not None,
          "not proved: %s" % (", ".join(bad) or "none"))


def main():
    if sys.argv[1:] == ["run"]:
        run_all()
        return
    if len(sys.argv) != 3 or sys.argv[1] != "report" or sys.argv[2] not in RUNS:
        print(__doc__)
        sys.exit(2)
    run = sys.argv[2]
    data = read_run(run)
    pf = per_function(run, data)
    mp = mapping(run, data)
    post(run, data, pf, mp)
    common(run, data)
    report_F(data)
    for n in notes:
        print("NOTE: " + n)
    vlines = ["%s (%s) %s" % (v, c, l) for v, c, l, _ in verdicts] + ["NOTE: " + n for n in notes]
    first = len(pending)
    annotate("notice", "G19 report %s: verdicts" % run, vlines)
    missed = [(c, l, d) for v, c, l, d in verdicts if v == "MISS"]
    summary(["### Report %s: verdicts" % run]
            + ["- %s (%s) %s%s" % (v, c, l, (": " + d) if d else "") for v, c, l, d in verdicts]
            + ["- NOTE: " + n for n in notes])
    for c, l, d in missed[:9]:
        annotate("error", "G19 report %s: MISS (%s)" % (run, c), [l + (": " + d if d else "")])
    if len(missed) > 9:
        annotate("error", "G19 report %s: MISS" % run,
                 ["and %d more MISS; see the verdicts notice" % (len(missed) - 9)])
    flush(first)
    if missed:
        sys.exit(1)
    print("Every part of report %s MATCHES." % run)


main()
