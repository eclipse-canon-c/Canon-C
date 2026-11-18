#!/usr/bin/env python3
"""
smoke_probe.py -- VERIFY-087's probe: WP's smoke tests on each of the 27
proof units, measured before anything is decided of them.

A proof is of nothing where WP's hypotheses contradict each other: from
there on WP proves every goal, the false with the true. VERIFY-085 met
one: a call's precondition, false in WP's typed model, which WP takes as
a hypothesis after the call, and every goal after it proved. WP's smoke
tests look for such points. With -wp-smoke-tests WP adds goals of \\false:
at each function's entry, under its requires and each behavior's assumes;
after each call; at each loop; and in code its own analysis does not find
reachable. It tries each with a short timeout (-wp-smoke-timeout). A smoke
test whose \\false a prover proves has failed: WP's hypotheses at that
point contradict each other, and what it proves past it, it proves of
nothing. One that no prover proves has passed; that is a test, not a
proof that the point is reachable.

Each part is a unit of tools/agreement/agreement.py's UNITS, a job of the
probe's matrix in probes.yml, and runs WP twice:

  smoke-only   -wp-prop=smoke: the smoke tests alone, as WP selects them
               by the name it gives them, "smoke"
  full         every goal of the unit, and the smoke tests: the smoke
               tests WP makes for the unit's proof. The unit's own goals
               have a 1 s timeout a prover, and their results are not read

WP runs with the unit's options (its model, -wp-rte, -wp-split, Alt-Ergo,
Z3 and CVC5, the unit's extra options), the smoke tests at WP's default
timeout, 2 s. Each run posts a notice: its smoke tests, how many passed,
those that failed, by kind (a behavior's requires or assumes, a call, a
loop, dead code) and function, the lines WP's warnings "Failed smoke-test"
name, and WP's summary; a third notice compares the two runs. The log
lists the smoke tests that failed and gave no result, and, from WP's JSON
report, every smoke test by kind and status. It never fails on what it
measures: a part fails only if a run is stopped at its limit (120
minutes), Frama-C exits with an error, or the full run gives no summary.

The file steps, next to this script, names the parts that run, or "all";
a part it does not name posts that it did not run. Without the file,
every part runs. "named PART" prints run=true or run=false, for the
workflow to skip Frama-C's setup for a part not named; "check" compares
the workflow's matrix with agreement.py's units. Each run's files go to
smoke_probe_out/UNIT/MODE/, run from the repository's root.

    python3 tools/probes/verify-087/smoke_probe.py UNIT
    python3 tools/probes/verify-087/smoke_probe.py named UNIT
    python3 tools/probes/verify-087/smoke_probe.py check
"""
import importlib.util
import json
import os
import re
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
STEPS = os.path.join(HERE, 'steps')
OUT = os.path.abspath('smoke_probe_out')
WORKFLOW = os.path.join(ROOT, '.github', 'workflows', 'probes.yml')
MODES = [('smoke-only', ['-wp-prop=smoke']), ('full', [])]
GOAL_TIMEOUT = '1'    # seconds a prover, the unit's own goals in the full run
SMOKE_TIMEOUT = '2'   # seconds a prover, a smoke test: WP's default
LIMIT = 120 * 60      # seconds, a run
CUT = 4000            # the runner cuts an annotation at 4096 characters
SHOWN = 12            # functions or lines named in a notice
LISTED = 300          # smoke tests listed in the log, a run
SMOKE = '_wp_smoke_'
PREFIXES = ('typed_cast_', 'typed_ref_', 'typed_')
KINDS = ['requires', 'assumes', 'call', 'loop', 'dead code', 'unreachable', 'other']


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def notice(title, msg):
    print(f'::notice title={title}::{msg[:CUT]}', flush=True)


def shown(text):
    """A tool's output in the log, none of its lines a workflow command."""
    for line in text.splitlines():
        print('| ' + line if line.lstrip().startswith(('::', '##[')) else line)
    sys.stdout.flush()


def enabled(parts):
    if not os.path.exists(STEPS):
        return list(parts)
    with open(STEPS) as f:
        words = [w for line in f for w in line.split('#', 1)[0].split()]
    return list(parts) if 'all' in words else [w for w in words if w in parts]


def command(ag, unit, opts, rep):
    """The unit's proof command, as agreement.py and the proof jobs run it,
    with the smoke tests, the run's options, and a 1 s timeout."""
    path, model, cpp = ag.UNITS[unit]
    return (['frama-c', '-wp', '-wp-rte',
             '-wp-model', model,
             '-wp-prover', 'alt-ergo,z3,cvc5',
             '-wp-timeout', GOAL_TIMEOUT,
             '-wp-split',
             '-wp-smoke-tests', '-wp-smoke-timeout', SMOKE_TIMEOUT,
             '-wp-report-json', rep]
            + opts
            + ag.EXTRA.get(unit, [])
            + [f'-cpp-extra-args={cpp}', path])


def run(cmd, d, limit):
    os.makedirs(d, exist_ok=True)
    txt = os.path.join(d, 'frama-c.log')
    print('\n=== ' + ' '.join(cmd), flush=True)
    start = time.time()
    with open(txt, 'w') as f:
        # A session of its own, so that a stop takes the provers with it.
        p = subprocess.Popen(cmd, cwd=ROOT, env=dict(os.environ, PWD=ROOT), stdout=f,
                             stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = p.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()
            code = None
    with open(txt, errors='replace') as f:
        text = f.read()
    return code, text, (time.time() - start) / 60


def smoke_count(text):
    """(passed, total) from the "Smoke Tests" line of WP's last summary,
    or None: WP prints it only when it ran smoke tests."""
    lines = text.splitlines()
    at = [i for i, line in enumerate(lines) if line.startswith('[wp] Proved goals:')]
    if not at:
        return None
    for line in lines[at[-1] + 1:]:
        m = re.match(r'^\s+Smoke Tests:\s+(\d+)\s*/\s*(\d+)', line)
        if m:
            return int(m.group(1)), int(m.group(2))
        if not re.match(r'^\s+\S', line):
            break
    return None


def smoke_results(text):
    """[(status, detail, goal)] of WP's lines on smoke tests: "[Failed]
    (Doomed)" a smoke test a prover proved, "[Passed] (...)" one none did
    (WP prints these only when asked), "[NoResult]" and "[Failure]" one
    without a result."""
    pat = re.compile(r'^\[wp\] \[(\w+)\] \(([^)]*)\) (\S*' + SMOKE + r'\S*)', re.M)
    return [(m.group(1), m.group(2), m.group(3)) for m in pat.finditer(text)]


def doomed(text):
    """The places WP's warnings "Failed smoke-test" name, file:line."""
    pat = re.compile(r'^\[wp\] (\S+?):(\d+): Warning:\s*Failed smoke-test', re.M)
    seen = []
    for m in pat.finditer(text):
        at = f'{m.group(1)}:{m.group(2)}'
        if at not in seen:
            seen.append(at)
    return seen


def function_of(goal):
    name = goal.split(SMOKE, 1)[0]
    for p in PREFIXES:
        if name.startswith(p):
            return name[len(p):]
    return name


def kind_of(goal):
    tag = goal.split(SMOKE, 1)[1] if SMOKE in goal else goal
    if tag.startswith('dead_call'):
        return 'call'
    if tag.startswith('dead_loop'):
        return 'loop'
    if tag.startswith('dead_code'):
        return 'dead code'
    if tag.startswith('unreachable'):
        return 'unreachable'
    if re.search(r'_requires(_\d+)?(_part\d+)?$', tag):
        return 'requires'
    if re.search(r'_assumes(_\d+)?(_part\d+)?$', tag):
        return 'assumes'
    return 'other'


def report(path):
    """[(goal, status)] of the smoke tests in WP's JSON report, status
    "failed" (a prover proved \\false), "passed", or WP's verdict; None
    without a report. WP writes a float with no digit after its point,
    which JSON does not allow: the point gets a 0 first, as agreement.py
    reads it."""
    if not os.path.exists(path):
        return None
    try:
        with open(path) as f:
            data = json.loads(re.sub(r'(\d)\.(?=\s*[,}\]])', r'\1.0', f.read()))
    except (OSError, ValueError):
        return None
    out = []
    for rec in data if isinstance(data, list) else []:
        if not isinstance(rec, dict) or SMOKE not in str(rec.get('goal', '')):
            continue
        verdict = str(rec.get('verdict', '')).lower()
        status = ('failed' if verdict == 'valid' else 'passed' if rec.get('passed') is True
                  else verdict or 'no verdict')
        out.append((str(rec['goal']), status))
    return out


def counted(pairs):
    out = {}
    for k in pairs:
        out[k] = out.get(k, 0) + 1
    return out


def by_kind(goals):
    c = counted(kind_of(g) for g in goals)
    return ', '.join(f'{k} {c[k]}' for k in KINDS if k in c)


def names(items):
    items = list(items)
    text = ', '.join(items[:SHOWN])
    return text + (f', and {len(items) - SHOWN} more' if len(items) > SHOWN else '')


def one(ag, unit, mode, opts):
    """WP with smoke tests on one unit in one mode: its data, or raises."""
    d = os.path.join(OUT, unit, mode)
    rep = os.path.join(d, 'wp-report.json')
    if os.path.exists(rep):
        os.remove(rep)
    code, text, minutes = run(command(ag, unit, opts, rep), d, LIMIT)
    s = ag.summary(text)
    if code is None or code != 0 or (s is None and mode == 'full'):
        shown('\n'.join(text.splitlines()[-40:]))
        why = (f'stopped after {LIMIT // 60} minutes' if code is None
               else f'frama-c exit {code}' if code != 0 else 'no summary')
        raise RuntimeError(f'{unit}, {mode}: {why}')
    count = smoke_count(text)
    lines = smoke_results(text)
    # WP prints a failed smoke test's line once for each prover that proves
    # it, and its summary counts it as often (run 1): each test once here.
    failed = list(dict.fromkeys(g for st, _, g in lines if st == 'Failed'))
    noresult = list(dict.fromkeys(g for st, _, g in lines
                                  if st in ('NoResult', 'Failure') and g not in failed))
    places = doomed(text)
    rows = report(rep)
    data = {'unit': unit, 'mode': mode, 'minutes': round(minutes, 1),
            'summary': list(s[:2]) if s else None, 'cats': s[2] if s else None,
            'smoke': list(count) if count else None, 'failed': failed,
            'noresult': noresult, 'doomed': places,
            'tests': len(rows) if rows is not None else None}
    if s is None:
        line = f'{unit}, {mode}: no goal (WP printed no summary)'
    else:
        # The smoke tests, each once: WP's JSON report's, or else its
        # summary's, less what it counts twice.
        if rows is not None:
            total = len(rows)
        else:
            passed_, total_ = count if count else (0, 0)
            total = passed_ + len(failed) + len(noresult)
        line = (f'{unit}, {mode}: smoke tests {total}, passed {total - len(failed) - len(noresult)}, '
                f'failed {len(failed)}')
        if failed:
            line += f' ({by_kind(failed)}): {names(sorted(set(function_of(g) for g in failed)))}'
        if noresult:
            line += f'; no result {len(noresult)}'
        if places:
            line += f'; doomed at {names(places)}'
        line += f'; WP {s[0]} / {s[1]}'
    line += f'; {minutes:.1f} minutes'
    print(line)
    if count:
        print(f'--- {unit}, {mode}: WP\'s summary, "Smoke Tests: {count[0]} / {count[1]}"')
    print(f'--- {unit}, {mode}: smoke tests that failed ({len(failed)}) and gave no result '
          f'({len(noresult)}) ---')
    shown('\n'.join([f'Failed: {g}' for g in failed[:LISTED]]
                    + [f'No result: {g}' for g in noresult[:LISTED]]))
    if places:
        print(f'--- {unit}, {mode}: lines WP warns of, "Failed smoke-test" ({len(places)}) ---')
        shown('\n'.join(places[:LISTED]))
    if rows is None:
        print(f'--- {unit}, {mode}: no JSON report')
    else:
        kinds = {}
        for g, st in rows:
            kinds.setdefault(kind_of(g), {}).setdefault(st, 0)
            kinds[kind_of(g)][st] += 1
        data['report'] = {k: kinds[k] for k in KINDS if k in kinds}
        print(f'--- {unit}, {mode}: smoke tests in WP\'s JSON report, {len(rows)}, by kind and status ---')
        shown('\n'.join(f'{k}: ' + ', '.join(f'{st} {n}' for st, n in sorted(kinds[k].items()))
                        for k in KINDS if k in kinds) or 'none')
        other = [f'{st}: {g}' for g, st in rows if st not in ('failed', 'passed')]
        if other:
            print(f'--- {unit}, {mode}: smoke tests neither failed nor passed in the report ({len(other)}) ---')
            shown('\n'.join(other[:LISTED]))
    data['line'] = line
    with open(os.path.join(d, 'summary.json'), 'w') as f:
        json.dump(data, f, indent=1)
    return data


def compared(unit, a, b):
    """The two runs' smoke tests side by side: a smoke-only, b full."""
    def total(x):
        if x.get('tests') is not None:
            return x['tests']
        return x['smoke'][1] if x.get('smoke') else 0
    fa, fb = set(a['failed']), set(b['failed'])
    line = (f'{unit}, the two runs: smoke-only {total(a)} smoke tests, failed {len(fa)}; '
            f'full {total(b)}, failed {len(fb)}; failed in both {len(fa & fb)}')
    if fb - fa:
        line += f'; failed only in full: {names(sorted(fb - fa))}'
    if fa - fb:
        line += f'; failed only in smoke-only: {names(sorted(fa - fb))}'
    return line


def matrix():
    """The units of the job smoke-probe's matrix in probes.yml, in order."""
    with open(WORKFLOW, encoding='utf-8') as f:
        text = f.read()
    at = text.find('\n  smoke-probe:')
    if at < 0:
        return None
    m = re.search(r'\n\s+unit:\s*\[([^\]]*)\]', text[at:])
    return [u.strip() for u in m.group(1).split(',') if u.strip()] if m else None


def check(ag):
    problems = []
    units = matrix()
    if units is None:
        problems.append('probes.yml: no job smoke-probe with a matrix of units')
    else:
        if sorted(units) != sorted(ag.UNITS):
            problems.append(f'probes.yml: the matrix lists {sorted(set(units) - set(ag.UNITS))} '
                            f'that agreement.py does not, and not {sorted(set(ag.UNITS) - set(units))}')
        elif units != [u for u in ag.ORDER if u in units]:
            problems.append('probes.yml: the matrix is not in agreement.py\'s ORDER')
    print(f'smoke probe check: {len(ag.UNITS)} units, the matrix {len(units or [])}; '
          f'{len(problems)} problem(s)')
    for p in problems:
        print('  ' + p)
    return 1 if problems else 0


def main(argv):
    ag = load('agreement', 'tools/agreement/agreement.py')
    parts = [u for u in ag.ORDER if u in ag.UNITS] + sorted(set(ag.UNITS) - set(ag.ORDER))
    if argv == ['check']:
        return check(ag)
    if len(argv) == 2 and argv[0] == 'named':
        print(f'run={"true" if argv[1] in enabled(parts) else "false"}')
        return 0
    if len(argv) != 1 or argv[0] not in parts:
        print(__doc__)
        return 2
    unit = argv[0]
    if unit not in enabled(parts):
        on = enabled(parts)
        notice(f'smoke probe {unit}', f'{unit}: not run; tools/probes/verify-087/steps names '
               + ('every unit' if on == parts else ' '.join(on) or 'no unit'))
        return 0
    os.chdir(ROOT)
    failed, got = [], {}
    for mode, opts in MODES:
        try:
            got[mode] = one(ag, unit, mode, opts)
            notice(f'smoke probe {unit} {mode}', got[mode]['line'])
        except RuntimeError as e:
            failed.append(str(e))
            notice(f'smoke probe {unit} {mode}', f'{unit}, {mode}: no data ({e})')
    if len(got) == len(MODES):
        notice(f'smoke probe {unit}', compared(unit, got['smoke-only'], got['full']))
    for f in failed:
        print(f'::error title=smoke probe {unit}::{f}')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
