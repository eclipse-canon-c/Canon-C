#!/usr/bin/env python3
"""
aorai_wp_probe.py -- VERIFY-085's probe: WP on the programs Aorai
instruments with tools/aorai/'s automata, measured before anything is
decided of it. 14 · Eva's job aorai checks the same cases with Eva
(tools/aorai/aorai.py); this asks what WP makes of them.

Aorai instruments a program with an automaton: ghost state, the ghost
functions that update it at each call and return, and contracts on every
function that name the states it is called and returns in. For a program
to keep the rule, every one of those contracts must hold: main's
obligations, among them each call's precondition on the state, and each
other function's, that it moves the state as its contract says. The parts,
each a job of the probe's matrix in probes.yml:

  main            each case of aorai.py's CASES: Aorai, then WP on main
                  alone (-wp-fct main), the callees' contracts taken as
                  given, with WP's smoke tests (since run 1b); Report's
                  table then gives each of main's properties its status.
                  Then hashmap_stale_checks.c, here: hashmap_stale.c with
                  ACSL checks, of \false at four points and of iter == 0
                  before the refused call (run 1b)
  lib-hashmap     hashmap_ok.c with hashmap_iteration.ya, WP on every
  lib-stringbuf   function; stringbuf_ok.c with stringbuf_views.ya;
  lib-bitset      bitset_ok.c with bitset_views.ya

WP runs with the options of the unit whose program the case is
preprocessed as (tools/agreement/agreement.py's UNITS and EXTRA): its
model, -wp-rte, -wp-split, Alt-Ergo, Z3 and CVC5, 120 s. Aorai's
properties are those that name its state (aorai_), as for aorai.py. Each
part posts notices: WP's summary, Aorai's properties by status, and for a
case that breaks its rule, the status of Aorai's precondition at the call
marked refused. Its log lists Aorai's properties neither valid nor
ignored (never tried, as a behavior's assumption or an assigns clause's
\from), WP's lines on smoke tests, and for a case that breaks its rule
every property of main at the marked call and Aorai's of main; for the
checks, their statuses. It never fails on what it measures: a part fails
only if a WP run gives no summary or no table, or is stopped at its limit
(30 minutes a case of main, 230 a lib part).

The file steps, next to this script, names the parts that run; a part it
does not name posts that it did not run. Without the file, every part
runs. "named PART" prints run=true or run=false, for the workflow to skip
Frama-C's setup for a part not named. Each part's files go to
aorai_wp_out/PART/, run from the repository's root.

    python3 tools/probes/verify-085/aorai_wp_probe.py PART
    python3 tools/probes/verify-085/aorai_wp_probe.py named PART
"""
import csv
import importlib.util
import json
import os
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
STEPS = os.path.join(HERE, 'steps')
OUT = os.path.abspath('aorai_wp_out')
LIBS = {'lib-hashmap': 'hashmap_ok', 'lib-stringbuf': 'stringbuf_ok', 'lib-bitset': 'bitset_ok'}
# Run 1b: hashmap_stale.c with checks of \false and of iter == 0 before the
# refused call, a case of the part main after aorai.py's.
CHECKS = 'tools/probes/verify-085/hashmap_stale_checks.c'
PARTS = ['main'] + list(LIBS)
LIMITS = {'main': 30 * 60, 'lib': 230 * 60}
CUT = 4000   # the runner cuts an annotation at 4096 characters


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


def enabled():
    if not os.path.exists(STEPS):
        return list(PARTS)
    with open(STEPS) as f:
        return [w for line in f for w in line.split('#', 1)[0].split()]


def command(tools, case, scope, csv_path):
    aorai, ev, agreement = tools
    _, source, program, auto, _ = case
    name = ev.PROGRAMS[program]
    model = agreement.UNITS[name][1]
    # The unit's job's options (proofs-*.yml), at its model, x86_64.
    cmd = (['frama-c', f'-cpp-extra-args={ev.cpp_of(program)}', source,
            '-aorai-automata', aorai.automaton(auto), '-then-last',
            '-wp', '-wp-rte', '-wp-model', model, '-wp-prover', 'alt-ergo,z3,cvc5',
            '-wp-timeout', '120', '-wp-split'] + agreement.EXTRA.get(name, []))
    if scope == 'main':
        # Run 1b: smoke tests, which try to prove that a call's contract or
        # a precondition leaves the code after it unreachable (VERIFY-085's
        # run 1: WP proved the precondition of the call hashmap_stale.c's
        # rule refuses, which Eva finds invalid).
        cmd += ['-wp-fct', 'main', '-wp-smoke-tests']
    # -report-untried: a property WP did not try is in the table too.
    return cmd + ['-then-last', '-report-untried', '-report-csv', csv_path], name


def run(cmd, d, limit):
    os.makedirs(d, exist_ok=True)
    txt = os.path.join(d, 'frama-c.log')
    print('\n=== ' + ' '.join(cmd), flush=True)
    start = time.time()
    with open(txt, 'w') as f:
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


def table(csv_path):
    if not os.path.exists(csv_path):
        return None
    with open(csv_path, encoding='utf-8', errors='replace') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def statuses(rows):
    out = {}
    for r in rows:
        out[r['status']] = out.get(r['status'], 0) + 1
    return '; '.join(f'{k} {v}' for k, v in sorted(out.items())) or 'none'


def one(tools, case, scope, limit):
    """WP on one case: (line for the notice, summary dict), or raises."""
    aorai, ev, agreement = tools
    name, source = case[0], case[1]
    d = os.path.join(OUT, scope if scope != 'main' else 'main', name)
    csv_path = os.path.join(d, 'report.csv')
    if os.path.exists(csv_path):
        os.remove(csv_path)
    cmd, unit = command(tools, case, scope, csv_path)
    code, text, minutes = run(cmd, d, limit)
    s = agreement.summary(text)
    rows = table(csv_path)
    if code is None or s is None or rows is None:
        shown('\n'.join(text.splitlines()[-40:]))
        why = (f'stopped after {limit // 60} minutes' if code is None
               else f'frama-c exit {code}, no summary' if s is None else 'no table')
        raise RuntimeError(f'{name}: {why}')
    mine = [r for r in rows if 'aorai_' in (r.get('property') or '')]
    if scope == 'main':
        mine = [r for r in mine if r.get('function') == 'main']
    cats = '; '.join(f'{k} {v}' for k, v in s[2].items())
    line = (f'{name}: Proved goals {s[0]} / {s[1]} ({cats}); Aorai\'s properties'
            f'{" of main" if scope == "main" else ""}: {statuses(mine)}')
    marks = aorai.marked_lines(source)
    if marks:
        base = os.path.basename(source)
        at = [r['status'] for r in mine if r['file'] == base and int(r['line']) == marks[0]
              and r['property kind'].startswith('precondition of')]
        line += f'; at the call marked refused (line {marks[0]}): {", ".join(at) or "no property"}'
    # Ignored: never tried, as WP does not prove a behavior's assumption or
    # an assigns clause's \from; the rest not valid are listed.
    unproved = [f'{r["status"]}: {r["function"]}:{r["line"]} {r["property kind"]}: {r["property"][:160]}'
                for r in mine if not r['status'].startswith('Valid') and r['status'] != 'Ignored']
    smoke = [ln for ln in text.splitlines() if 'moke' in ln or 'oomed' in ln]
    if scope == 'main':
        line += f'; smoke-test lines {len(smoke)}, doomed {sum("oomed" in ln for ln in smoke)}'
    line += f'; {minutes:.1f} minutes'
    print(line)
    if unproved:
        print(f'--- {name}: Aorai\'s properties neither valid nor ignored ({len(unproved)}) ---')
        shown('\n'.join(unproved[:200]))
    if smoke:
        print(f'--- {name}: WP\'s lines on smoke tests ({len(smoke)}) ---')
        shown('\n'.join(smoke[:80]))
    checks = [r for r in rows if r.get('function') == 'main'
              and ('check' in r['property kind'] or 'assert' in r['property kind'])
              and os.path.basename(source) == r['file']]
    if source == CHECKS:
        line += '; checks: ' + ', '.join(f'{r["property"][:60]} {r["status"]}' for r in checks)
    if marks:
        base = os.path.basename(source)
        print(f'--- {name}: every property of main at line {marks[0]}, and Aorai\'s of main ---')
        shown('\n'.join(f'{r["status"]}: {r["line"]} {r["property kind"]}: {r["property"][:200]}'
                         for r in rows if r.get('function') == 'main' and r['file'] == base
                         and (int(r['line']) == marks[0] or 'aorai_' in (r.get('property') or ''))))
    summary = {'case': name, 'unit': unit, 'summary': s[:2], 'cats': s[2],
               'aorai': statuses(mine), 'unproved': unproved, 'minutes': round(minutes, 1)}
    with open(os.path.join(d, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=1)
    return line


def main(argv):
    if len(argv) == 2 and argv[0] == 'named':
        want = argv[1]
        print(f'run={"true" if want in enabled() else "false"}')
        return 0
    if len(argv) != 1 or argv[0] not in PARTS:
        print(__doc__)
        return 2
    part = argv[0]
    if part not in enabled():
        notice(f'aorai wp {part}', f'{part}: not run; tools/probes/verify-085/steps names '
               + (' '.join(enabled()) or 'no part'))
        return 0
    os.chdir(ROOT)
    tools = (load('aorai', 'tools/aorai/aorai.py'), load('eva', 'tools/eva/eva.py'),
             load('agreement', 'tools/agreement/agreement.py'))
    cases = {c[0]: c for c in tools[0].CASES}
    failed = []
    if part == 'main':
        lines = {}
        extra = ('hashmap_stale_checks', CHECKS, 'hashmap', 'hashmap_iteration', 'breaks')
        for case in tools[0].CASES + [extra]:
            try:
                lines.setdefault(case[3], []).append(one(tools, case, 'main', LIMITS['main']))
            except RuntimeError as e:
                failed.append(str(e))
                lines.setdefault(case[3], []).append(f'{case[0]}: no data ({e})')
        for auto, ls in lines.items():
            notice(f'aorai wp main {auto}', f'WP on main, {auto}: ' + '; '.join(ls))
    else:
        case = cases[LIBS[part]]
        try:
            notice(f'aorai wp {part}', f'WP on every function, {case[3]}: '
                   + one(tools, case, part, LIMITS['lib']))
        except RuntimeError as e:
            failed.append(str(e))
    for f in failed:
        print(f'::error title=aorai wp {part}::{f}')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
