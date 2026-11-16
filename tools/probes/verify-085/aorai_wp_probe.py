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

  main            each case of MAIN: Aorai, then WP on main alone
                  (-wp-fct main), the callees' contracts taken as given,
                  with WP's smoke tests. Since run 2b, MAIN is
                  hashmap_stale_checks.c with hashmap_iteration.ya, and
                  hashmap_stale_bytes.c and hashmap_ok_bytes.c, the same
                  programs with their buffer declared as bytes; WP runs
                  without -wp-rte and with a 10 s timeout. Run 2 ran the
                  first also without an automaton; runs 1 and 1b ran
                  aorai.py's CASES (1b, the checks file after them)
  lib-hashmap     hashmap_ok.c with hashmap_iteration.ya, WP on every
  lib-stringbuf   function; stringbuf_ok.c with stringbuf_views.ya;
  lib-bitset      bitset_ok.c with bitset_views.ya

WP runs with the options of the unit whose program the case is
preprocessed as (tools/agreement/agreement.py's UNITS and EXTRA): its
model, -wp-rte, -wp-split, Alt-Ergo, Z3 and CVC5, 120 s; in the part
main, since run 2b, without -wp-rte and with 10 s (MAIN_TIMEOUT). Aorai's
properties are those that name its state (aorai_), as for aorai.py. Each
part posts notices: WP's summary, Aorai's properties by status, and for a
case that breaks its rule, the status of Aorai's precondition at the call
marked refused. In main, a notice a case, which also gives the lines WP's
smoke tests find unreachable (its warnings "Failed smoke-test"), the
status of each check the program names, and main's other properties
neither valid nor ignored. The log lists them all: main's properties
neither valid nor ignored, its assertions and checks, and for a case that
breaks its rule every property of main at the marked call and Aorai's of
main; for a lib part, Aorai's properties neither valid nor ignored (never
tried, as a behavior's assumption or an assigns clause's \\from). It never
fails on what it measures: a part fails only if a WP run gives no summary
or no table, or is stopped at its limit (90 minutes a case of main, since
run 2b, 30 before; 230 a lib part).

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
import re
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
PROBE = 'tools/probes/verify-085/'
# Run 1b: hashmap_stale.c with checks of \false after the init, the loop,
# the first hashmap_iter_next and the insert, and of iter == 0 before the
# refused call.
CHECKS = PROBE + 'hashmap_stale_checks.c'
# The part main's cases, as aorai.py's CASES: (case, source, the Eva
# program whose preprocessing it takes, automaton or None, verdict). Run 2
# ran the checks file with and without Aorai, and the two programs whose
# buffer is declared as bytes, which were stopped at 30 minutes. Run 2b:
# the checks file with Aorai, as a control at the new settings, and the
# two programs of bytes.
MAIN = [
    ('hashmap_stale_checks', CHECKS, 'hashmap', 'hashmap_iteration', 'breaks'),
    ('hashmap_stale_bytes', PROBE + 'hashmap_stale_bytes.c', 'hashmap', 'hashmap_iteration', 'breaks'),
    ('hashmap_ok_bytes', PROBE + 'hashmap_ok_bytes.c', 'hashmap', 'hashmap_iteration', 'follows'),
]
# Run 2b: in the part main, WP without -wp-rte and with a 10 s timeout a
# prover. In run 2, without the contradiction, every goal WP did not prove,
# and every smoke test that passed, took the unit's 120 s a prover, and
# -wp-rte's overflow checks were among them.
MAIN_TIMEOUT = '10'
PARTS = ['main'] + list(LIBS)
LIMITS = {'main': 90 * 60, 'lib': 230 * 60}
CUT = 4000   # the runner cuts an annotation at 4096 characters
SHOWN = 10   # main's properties not valid, in a notice
NOT_VALID = ('Valid', 'Ignored', 'Considered valid')


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
    cmd = ['frama-c', f'-cpp-extra-args={ev.cpp_of(program)}', source]
    if auto:   # run 2: a case without an automaton is WP on the program alone
        cmd += ['-aorai-automata', aorai.automaton(auto), '-then-last']
    # The unit's job's options (proofs-*.yml), at its model, x86_64; in the
    # part main, since run 2b, without -wp-rte and with MAIN_TIMEOUT.
    rte, timeout = ([], MAIN_TIMEOUT) if scope == 'main' else (['-wp-rte'], '120')
    cmd += (['-wp'] + rte + ['-wp-model', model, '-wp-prover', 'alt-ergo,z3,cvc5',
                             '-wp-timeout', timeout, '-wp-split'] + agreement.EXTRA.get(name, []))
    if scope == 'main':
        # Since run 1b: smoke tests, which try to prove that a call's
        # contract or a precondition leaves the code after it unreachable
        # (VERIFY-085's run 1: WP proved the precondition of the call
        # hashmap_stale.c's rule refuses, which Eva finds invalid).
        cmd += ['-wp-fct', 'main', '-wp-smoke-tests']
    # -report-untried: a property WP did not try is in the table too. Without
    # Aorai no project was made, and -then-last would find none.
    return cmd + ['-then-last' if auto else '-then', '-report-untried', '-report-csv', csv_path], name


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


def doomed(text, source):
    """The lines of source at which WP's smoke tests find the code
    unreachable: its warnings "Failed smoke-test", which name the line."""
    pattern = re.escape(source) + r':(\d+): Warning:\s*Failed smoke-test'
    return sorted({int(m.group(1)) for m in re.finditer(pattern, text)})


def named_checks(source):
    """{line: name} of the ACSL checks source names (check NAME: ...)."""
    out = {}
    with open(os.path.join(ROOT, source), encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            m = re.search(r'/\*@\s*check\s+(\w+)\s*:', line)
            if m:
                out[n] = m.group(1)
    return out


def asserted(r):
    return 'check' in r['property kind'] or 'assert' in r['property kind']


def one(tools, case, scope, limit):
    """WP on one case: the line for its notice, or raises."""
    aorai, ev, agreement = tools
    name, source = case[0], case[1]
    d = os.path.join(OUT, scope, name)
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
    base = os.path.basename(source)
    mine = [r for r in rows if 'aorai_' in (r.get('property') or '')]
    if scope == 'main':
        mine = [r for r in mine if r.get('function') == 'main']
    cats = '; '.join(f'{k} {v}' for k, v in s[2].items())
    line = (f'{name}: Proved goals {s[0]} / {s[1]} ({cats}); Aorai\'s properties'
            f'{" of main" if scope == "main" else ""}: {statuses(mine)}')
    marks = aorai.marked_lines(source)
    if marks:
        at = [r['status'] for r in mine if r['file'] == base and int(r['line']) == marks[0]
              and r['property kind'].startswith('precondition of')]
        line += f'; at the call marked refused (line {marks[0]}): {", ".join(at) or "no property"}'
    summary = {'case': name, 'unit': unit, 'automaton': case[3], 'summary': s[:2],
               'cats': s[2], 'aorai': statuses(mine)}
    if scope == 'main':
        ofmain = [r for r in rows if r.get('function') == 'main' and r['file'] == base]
        dl = doomed(text, source)
        line += '; doomed at lines ' + (', '.join(map(str, dl)) or 'none')
        checks = named_checks(source)
        got = []
        for n, nm in sorted(checks.items()):
            st = [r['status'] for r in ofmain if int(r['line']) == n and asserted(r)]
            got.append(f'{nm} ({n}) {", ".join(st) or "no property"}')
        if got:
            line += '; checks: ' + ', '.join(got)
        other = [r for r in ofmain if not asserted(r) and r['status'] not in NOT_VALID]
        line += f'; main\'s other properties not valid: {len(other)}'
        if other:
            line += ' (' + ', '.join(f'{r["line"]} {r["property kind"]} {r["status"]}'
                                     for r in other[:SHOWN])
            line += f', and {len(other) - SHOWN} more)' if len(other) > SHOWN else ')'
        print(f'--- {name}: main\'s properties neither valid nor ignored, but its assertions '
              f'and checks ({len(other)}) ---')
        shown('\n'.join(f'{r["status"]}: {r["line"]} {r["property kind"]}: {r["property"][:200]}'
                        for r in other))
        print(f'--- {name}: main\'s assertions and checks, smoke tests among them ---')
        shown('\n'.join(f'{r["line"]} {r["property kind"]}: {r["status"]}: {r["property"][:120]}'
                        for r in ofmain if asserted(r)))
        summary.update({'doomed': dl, 'checks': got,
                        'not_valid': [f'{r["status"]}: {r["line"]} {r["property kind"]}: '
                                      f'{r["property"]}' for r in other]})
    else:
        # Ignored: never tried, as WP does not prove a behavior's assumption
        # or an assigns clause's \from; the rest not valid are listed.
        unproved = [f'{r["status"]}: {r["function"]}:{r["line"]} {r["property kind"]}: '
                    f'{r["property"][:160]}' for r in mine if r['status'] not in NOT_VALID]
        if unproved:
            print(f'--- {name}: Aorai\'s properties neither valid nor ignored ({len(unproved)}) ---')
            shown('\n'.join(unproved[:200]))
        summary['unproved'] = unproved
    if marks:
        print(f'--- {name}: every property of main at line {marks[0]}, and Aorai\'s of main ---')
        shown('\n'.join(f'{r["status"]}: {r["line"]} {r["property kind"]}: {r["property"][:200]}'
                        for r in rows if r.get('function') == 'main' and r['file'] == base
                        and (int(r['line']) == marks[0] or 'aorai_' in (r.get('property') or ''))))
    line += f'; {minutes:.1f} minutes'
    print(line)
    summary['minutes'] = round(minutes, 1)
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
        for case in MAIN:
            what = f'WP on main, {case[3] or "no automaton"}: '
            try:
                notice(f'aorai wp main {case[0]}', what + one(tools, case, 'main', LIMITS['main']))
            except RuntimeError as e:
                failed.append(str(e))
                notice(f'aorai wp main {case[0]}', what + f'{case[0]}: no data ({e})')
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
