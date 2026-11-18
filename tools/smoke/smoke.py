#!/usr/bin/env python3
"""
smoke.py -- WP's smoke tests on the proof units, a check on each push that
can change them (VERIFY-088; measured first by VERIFY-087's probe,
tools/probes/verify-087/).

A proof is of nothing where WP's hypotheses contradict each other: from
there on WP proves every goal, the false with the true. With
-wp-smoke-tests WP adds goals of \\false: at each function's entry, under
its requires and each behavior's assumes; after each call; at each loop;
and in code its own analysis does not find reachable. A smoke test whose
\\false a prover proves has failed: WP's hypotheses at that point
contradict each other. One that no prover proves has passed; that is a
test, not a proof that the point is reachable. VERIFY-087 found two kinds
of failure: branches no execution reaches, where WP proves of nothing as
it should, and calls whose precondition is false in WP's model, past which
it proved what follows of nothing. It fixed three of the second kind and
documented the nine that remain, past memcmp's call
(docs/verification.md, "Past memcmp's call").

This script runs WP on one unit of tools/agreement/agreement.py's UNITS,
with the unit's options as its proof job runs it (its model, -wp-rte,
-wp-split, Alt-Ergo, Z3 and CVC5, its extra options), every goal at 1 s a
prover and the smoke tests at WP's 2 s. It does not read the goals'
results, which the proof jobs check. It reads the smoke tests from WP's
JSON report, and compares those that failed with the unit's pins,
tools/smoke/pins/UNIT.txt: each line a goal's name as WP gives it, but its
statement number, and how many smoke tests of that name fail. WP names a
call's smoke test after the function called. A count other than the
pin's, a name the pins do not hold, or a smoke test with no result is a
difference; with ENFORCE, a difference fails the job. A run stopped at its
limit (120 minutes), an error of Frama-C, or a run without WP's summary or
JSON report always fails it. Each run's files go to smoke_out/UNIT/, run
from the repository's root.

    python3 tools/smoke/smoke.py UNIT
    python3 tools/smoke/smoke.py affected --all | FILE...
    python3 tools/smoke/smoke.py check
    python3 tools/smoke/smoke.py pin UNIT

"affected" prints, as JSON, the units a change to the files can affect,
as 13 · Prover agreement chooses them (agreement.py's unit_deps): those
that include a changed file, every unit for a change to this script,
agreement.py, the workflow, the Frama-C setup or a header no unit
includes, and a unit whose pins changed. "check" is 8 · Records': one pin
file a unit, each line well formed, and every file a unit includes under
the workflow's push paths. "pin" writes the unit's pins from its last run.
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
ROOT = os.path.dirname(os.path.dirname(HERE))
PINS = os.path.join(HERE, 'pins')
OUT = 'smoke_out'
WORKFLOW = '.github/workflows/smoke.yml'
FORCING = ('tools/smoke/smoke.py', 'tools/agreement/agreement.py', WORKFLOW,
           '.github/actions/framac-setup/')
ENFORCE = False       # VERIFY-088 run 1: report-only
GOAL_TIMEOUT = '1'    # seconds a prover, the unit's own goals, whose results are not read
SMOKE_TIMEOUT = '2'   # seconds a prover, a smoke test: WP's default
LIMIT = 120 * 60      # seconds, the run
CUT = 4000            # the runner cuts an annotation at 4096 characters
SHOWN = 12            # names in a notice
SMOKE = '_wp_smoke_'
PREFIXES = ('typed_cast_', 'typed_ref_', 'typed_')
KINDS = ['requires', 'assumes', 'call', 'loop', 'dead code', 'unreachable', 'other']
PIN_LINE = re.compile(r'^(typed\w*' + SMOKE + r'\w+)\s+(\d+)$')


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


def units(ag):
    return [u for u in ag.ORDER if u in ag.UNITS] + sorted(set(ag.UNITS) - set(ag.ORDER))


def command(ag, unit, rep):
    """The unit's proof command, as agreement.py and the proof jobs run it,
    with the smoke tests and a 1 s timeout for the unit's own goals."""
    path, model, cpp = ag.UNITS[unit]
    return (['frama-c', '-wp', '-wp-rte',
             '-wp-model', model,
             '-wp-prover', 'alt-ergo,z3,cvc5',
             '-wp-timeout', GOAL_TIMEOUT,
             '-wp-split',
             '-wp-smoke-tests', '-wp-smoke-timeout', SMOKE_TIMEOUT,
             '-wp-report-json', rep]
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


def doomed(text):
    """The places WP's warnings "Failed smoke-test" name, file:line."""
    pat = re.compile(r'^\[wp\] (\S+?):(\d+): Warning:\s*Failed smoke-test', re.M)
    seen = []
    for m in pat.finditer(text):
        at = f'{m.group(1)}:{m.group(2)}'
        if at not in seen:
            seen.append(at)
    return seen


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


def named(goal):
    """The goal's name but its statement number: a pin's name."""
    return re.sub(r'_s\d+$', '', goal)


def function_of(goal):
    name = goal.split(SMOKE, 1)[0]
    for p in PREFIXES:
        if name.startswith(p):
            return name[len(p):]
    return name


def report(path):
    """[{goal, status, function, at, seconds, prover}] of the smoke tests in
    WP's JSON report, status "failed" (a prover proved \\false), "passed",
    or WP's verdict; seconds and prover the fastest proof of a failed one.
    None without a report. WP writes a float with no digit after its point,
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
        best = None
        if status == 'failed':
            for p in rec.get('provers') or []:
                if isinstance(p, dict) and p.get('success') and isinstance(p.get('time'), (int, float)):
                    if best is None or p['time'] < best[0]:
                        best = (float(p['time']), str(p.get('prover', '?')))
        f = os.path.relpath(str(rec['file']), ROOT) if rec.get('file') else '?'
        out.append({'goal': str(rec['goal']), 'status': status,
                    'function': str(rec.get('function', '?')),
                    'at': f'{f}:{rec.get("line", "?")}',
                    'seconds': best[0] if best else None,
                    'prover': best[1] if best else None})
    return out


def counted(items):
    out = {}
    for k in items:
        out[k] = out.get(k, 0) + 1
    return out


def by_kind(goals):
    c = counted(kind_of(g) for g in goals)
    return ', '.join(f'{k} {c[k]}' for k in KINDS if k in c)


def names(items):
    items = list(items)
    text = ', '.join(items[:SHOWN])
    return text + (f', and {len(items) - SHOWN} more' if len(items) > SHOWN else '')


def pin_path(unit):
    return os.path.join(PINS, unit + '.txt')


def read_pins(unit):
    """{name: count}, and the file's problems."""
    out, problems = {}, []
    path = pin_path(unit)
    if not os.path.exists(path):
        return None, [f'{os.path.relpath(path, ROOT)}: no such file']
    with open(path, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            m = PIN_LINE.match(text)
            if not m or int(m.group(2)) < 1 or re.search(r'_s\d+$', m.group(1)):
                problems.append(f'{os.path.relpath(path, ROOT)}:{n}: not "NAME COUNT", a smoke '
                                f'test\'s name without its statement number and a count of 1 or more')
                continue
            if m.group(1) in out:
                problems.append(f'{os.path.relpath(path, ROOT)}:{n}: {m.group(1)} pinned twice')
                continue
            out[m.group(1)] = int(m.group(2))
    return out, problems


def differences(want, got):
    out = []
    for k in sorted(set(want) | set(got)):
        a, b = want.get(k, 0), got.get(k, 0)
        if a != b:
            out.append(f'{k} (pinned {a}, fails {b})')
    return out


def one(ag, unit):
    """WP with smoke tests on the unit: (notice line, differences, data), or raises."""
    d = os.path.join(OUT, unit)
    rep = os.path.join(d, 'wp-report.json')
    if os.path.exists(rep):
        os.remove(rep)
    code, text, minutes = run(command(ag, unit, rep), d, LIMIT)
    s = ag.summary(text)
    rows = report(rep)
    if code is None or code != 0 or s is None or rows is None:
        shown('\n'.join(text.splitlines()[-40:]))
        why = (f'stopped after {LIMIT // 60} minutes' if code is None
               else f'frama-c exit {code}' if code != 0
               else 'no summary' if s is None else 'no JSON report')
        raise RuntimeError(f'{unit}: {why}')
    failed = [r for r in rows if r['status'] == 'failed']
    other = [r for r in rows if r['status'] not in ('failed', 'passed')]
    got = counted(named(r['goal']) for r in failed)
    want, problems = read_pins(unit)
    diffs = problems + differences(want or {}, got)
    diffs += [f'{r["goal"]} (no result: {r["status"]})' for r in other]
    places = doomed(text)
    line = (f'{unit}: smoke tests {len(rows)}, passed {len(rows) - len(failed) - len(other)}, '
            f'failed {len(failed)}')
    if failed:
        line += f' ({by_kind(r["goal"] for r in failed)})'
    if other:
        line += f'; no result {len(other)}'
    line += '; as pinned' if not diffs else f'; {len(diffs)} difference(s): {names(diffs)}'
    timed = [r for r in failed if r['seconds'] is not None]
    if timed:
        slow = max(timed, key=lambda r: r['seconds'])
        line += f'; the slowest failure {slow["seconds"]:.2f} s ({slow["prover"]}) of {SMOKE_TIMEOUT}'
    line += f'; {minutes:.1f} minutes'
    print(line)
    print(f'--- {unit}: the smoke tests that failed ({len(failed)}): goal, function, place, '
          f'the fastest proof of \\false ---')
    shown('\n'.join(f'{r["goal"]}  {r["function"]}  {r["at"]}  '
                    + (f'{r["seconds"]:.2f} s {r["prover"]}' if r['seconds'] is not None else '?')
                    for r in failed) or 'none')
    if other:
        print(f'--- {unit}: smoke tests with no result ({len(other)}) ---')
        shown('\n'.join(f'{r["goal"]}: {r["status"]}' for r in other))
    if places:
        print(f'--- {unit}: lines WP warns of, "Failed smoke-test" ({len(places)}) ---')
        shown('\n'.join(places))
    kinds = {}
    for r in rows:
        kinds.setdefault(kind_of(r['goal']), {}).setdefault(r['status'], 0)
        kinds[kind_of(r['goal'])][r['status']] += 1
    print(f'--- {unit}: smoke tests in WP\'s JSON report, {len(rows)}, by kind and status ---')
    shown('\n'.join(f'{k}: ' + ', '.join(f'{st} {n}' for st, n in sorted(kinds[k].items()))
                    for k in KINDS if k in kinds) or 'none')
    print(f'--- {unit}: the pins as measured (tools/smoke/smoke.py pin {unit}) ---')
    shown('\n'.join(f'{k} {got[k]}' for k in sorted(got)) or 'none')
    if diffs:
        print(f'--- {unit}: differences from the pins ({len(diffs)}) ---')
        shown('\n'.join(diffs))
    data = {'unit': unit, 'minutes': round(minutes, 1), 'summary': list(s[:2]),
            'tests': len(rows), 'failed': [r['goal'] for r in failed],
            'other': [r['goal'] for r in other], 'pins': got, 'differences': diffs,
            'doomed': places, 'line': line}
    with open(os.path.join(d, 'summary.json'), 'w') as f:
        json.dump(data, f, indent=1)
    return line, diffs, data


def affected(ag, files):
    """The units a change to files can affect, in the run order."""
    order = units(ag)
    deps = {u: ag.unit_deps(u) for u in ag.UNITS}
    hit = set()
    for f in files:
        f = f.strip()
        if not f:
            continue
        if f.startswith(FORCING):
            return order
        pin = re.match(r'^tools/smoke/pins/(frama-c-[a-z0-9-]+)\.txt$', f)
        if pin:
            if pin.group(1) in ag.UNITS:
                hit.add(pin.group(1))
            continue
        users = {u for u, d in deps.items() if f in d}
        if users:
            hit |= users
        elif f.endswith('.h') and f.startswith(ag.WATCHED):
            return order   # a header no unit includes today: run every unit
    return [u for u in order if u in hit]


def push_paths():
    """The workflow's push paths, read as written (one '- ...' line each)."""
    paths, inside = [], False
    with open(os.path.join(ROOT, WORKFLOW), encoding='utf-8') as f:
        for line in f:
            if re.match(r'^    paths:\s*$', line):
                inside = True
                continue
            if inside:
                m = re.match(r"^      - ['\"]?([^'\"]+)['\"]?\s*$", line)
                if not m:
                    break
                paths.append(m.group(1))
    return paths


def check(ag):
    problems = []
    us = set(ag.UNITS)
    files = {f[:-4] for f in os.listdir(PINS) if f.endswith('.txt')} if os.path.isdir(PINS) else set()
    for u in sorted(us - files):
        problems.append(f'{u}: no pin file tools/smoke/pins/{u}.txt')
    for u in sorted(files - us):
        problems.append(f'tools/smoke/pins/{u}.txt: no such unit in agreement.py')
    pinned = 0
    for u in sorted(us & files):
        got, ps = read_pins(u)
        problems += ps
        pinned += sum((got or {}).values())
    paths = push_paths()
    if not paths:
        problems.append(f'{WORKFLOW}: no push paths read')
    else:
        seen = set()
        for u in units(ag):
            try:
                for f in sorted(ag.unit_deps(u)):
                    if f not in seen and not any(ag.matches(f, p) for p in paths):
                        problems.append(f'{u} includes {f}, which is not under {WORKFLOW}\'s push paths')
                    seen.add(f)
            except Exception as e:   # the compiler's error, as agreement.py's check reports it
                problems.append(str(e))
    for f in FORCING:
        if not os.path.exists(os.path.join(ROOT, f)):
            problems.append(f'{f}: named as forcing every unit, and not there')
    print(f'smoke check: {len(us)} units, {len(files & us)} pin files, {pinned} smoke tests pinned '
          f'to fail; {len(problems)} problem(s)')
    for p in problems:
        print('  ' + p)
    return 1 if problems else 0


def pin(unit):
    path = os.path.join(ROOT, OUT, unit, 'summary.json')
    with open(path) as f:
        got = json.load(f)['pins']
    head = [f'# {unit}',
            '# The smoke tests WP fails in this unit (tools/smoke/smoke.py, VERIFY-088):',
            '# each line a goal\'s name as WP gives it, but its statement number, and how',
            '# many smoke tests of that name fail; a call\'s smoke test is named after the',
            '# function called. Written by "smoke.py pin".']
    with open(pin_path(unit), 'w', encoding='utf-8') as f:
        f.write('\n'.join(head + ([f'{k} {got[k]}' for k in sorted(got)] or ['# none'])) + '\n')
    print(f'{unit}: {sum(got.values())} smoke test(s) pinned, {len(got)} name(s)')
    return 0


def main(argv):
    ag = load('agreement', 'tools/agreement/agreement.py')
    if argv == ['check']:
        return check(ag)
    if argv and argv[0] == 'affected':
        print(json.dumps(units(ag) if argv[1:] == ['--all'] else affected(ag, argv[1:])))
        return 0
    if len(argv) == 2 and argv[0] == 'pin' and argv[1] in ag.UNITS:
        return pin(argv[1])
    if len(argv) != 1 or argv[0] not in ag.UNITS:
        print(__doc__)
        return 2
    unit = argv[0]
    os.chdir(ROOT)
    try:
        line, diffs, _ = one(ag, unit)
    except RuntimeError as e:
        notice(f'smoke {unit}', f'{unit}: no data ({e})')
        print(f'::error title=smoke {unit}::{e}')
        return 1
    notice(f'smoke {unit}', line)
    if diffs and ENFORCE:
        print(f'::error title=smoke {unit}::{len(diffs)} difference(s) from tools/smoke/pins/{unit}.txt')
        return 1
    if diffs:
        print(f'REPORT-ONLY: {len(diffs)} difference(s) from the pins, not failing (ENFORCE is off)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
