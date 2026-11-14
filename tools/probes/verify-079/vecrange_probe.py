#!/usr/bin/env python3
"""
vecrange_probe.py -- VERIFY-079's probe: the two slow goals of
vec_int_extend_from_range's loop, measured with three changes to its
annotations before data/vec/vec_range.h changes.

frama-c-vec-range proves every goal of vec_int_extend_from_range, but two of
them only with Alt-Ergo: parts 3 and 4 of the preservation of loop invariant
5, which took Alt-Ergo alone 89 s and 38 s in VERIFY-065's runs and up to
76 s and 38 s in 13 · Prover agreement's runs #6 to #11, against the 120 s
limit. A slower runner could lose them, and the proof job would fail its pin.
Invariant 5 states the values the loop has stored so far:

    \\forall integer k; 0 <= k < i
      ==> v->items[v->len + k] == \\at(r.current, Pre) + k * r.step

Under WP's typed model the items, v->len and the range's fields are all
integers in one memory, the range being there because its address is taken.
After the store, each read of v->len and of r.step needs the provers to show
that the store did not write it, once for every k. The probe measures the
function with four versions of the header, each applied to a copy of the
checkout:

  P0  the header as it is;
  A   three assertions in the loop's body (variants/a_asserts.patch): after
      the round-trip check, that value is the range's i-th value (ith_value)
      and that elem equals it (fits); after the store, that the slot holds it
      (ith_stored);
  B   invariant 5 over values no store changes (variants/b_anchored.patch):
      v->items[\\at(v->len, Pre) + k] == \\at(r.current, Pre) + k * \\at(r.step, Pre),
      and a new invariant after it, len_kept: v->len == \\at(v->len, Pre);
  C   both (variants/c_both.patch).

Each version's step runs WP on vec_int_extend_from_range alone (-wp-fct),
with frama-c-vec-range's options as tools/agreement/agreement.py lists them:
once with Alt-Ergo, Z3 and CVC5 together, as the proof job runs them, and
once with each alone, as 13 · Prover agreement runs them. It reports the
goals proved and not, each goal's prover and time, the goals one or two
provers prove alone as tools/agreement/pins/ lists them, and the pin file's
lines for the function that would go and come. The step report compares the
four from what the other steps wrote.

The file steps, next to this script, names the steps that run; a step it
does not name posts that it did not run. Without the file, every step runs.

It writes notices for each step and prints each goal's results in the log.
GitHub reads a workflow command at the start of a line even behind spaces,
so the probe prints each line of WP's output that would be one behind "| ".
It never fails on what it measures: it fails only if a WP run gives no
summary or no report, or is stopped at LIMIT. A version whose patch neither
applies nor is found applied is reported and not measured.

Each step's files go to vecrange_out/ in the directory it is run from:
VERSION/all.txt and .json, WP's output and report with the three provers
together; VERSION/alt-ergo.txt, z3.txt, cvc5.txt and their .json, each
alone; VERSION/summary.json, what the step report reads.

    python3 tools/probes/verify-079/vecrange_probe.py [P0|A|B|C|report]...

With no argument it runs every step the file steps names.
"""
import importlib.util
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
STEPS = os.path.join(HERE, 'steps')
OUT = os.path.abspath('vecrange_out')
UNIT = 'frama-c-vec-range'
FN = 'vec_int_extend_from_range'
PREFIX = f'typed_cast_{FN}_'
PROVERS = ['alt-ergo', 'z3', 'cvc5']
RUNS = ['all'] + PROVERS   # all: the three together, as the proof job runs them
VERSIONS = [
    ('P0', None, 'the header as it is'),
    ('A', 'variants/a_asserts.patch', 'three assertions in the loop'),
    ('B', 'variants/b_anchored.patch', 'invariant 5 over values at Pre, and len_kept'),
    ('C', 'variants/c_both.patch', 'A and B'),
]
ALL = [v for v, _, _ in VERSIONS] + ['report']
INV5 = 'loop_invariant_5_preserved'
CUT = 4000        # the runner cuts an annotation at 4096 characters
STEP = 10         # GitHub keeps ten notices of a step
SHOWN = 10.0      # seconds: a goal that took a run longer is named in the notices
SLOW = 30.0       # seconds: 13 · Prover agreement's slow, tools/agreement/agreement.py's SLOW
LIMIT = 60 * 60   # seconds: a WP run still going after this is stopped, and gives no data


def notice(title, msg):
    print(f'::notice title={title}::{msg[:CUT]}', flush=True)


def shown(text):
    """A tool's output in the log, none of its lines a workflow command."""
    for l in text.splitlines():
        print('| ' + l if l.lstrip().startswith(('::', '##[')) else l)
    sys.stdout.flush()


def short(goal):
    return goal[len(PREFIX):] if goal.startswith(PREFIX) else goal


def secs(t):
    """A time in seconds as the notices give it: tenths under 10 s."""
    return f'{t:.1f} s' if t < 10 else f'{t:.0f} s'


def grouped(head, items, room, sep='; '):
    """Texts "head: item; item ..." of at most CUT characters, at most room of
    them; the last says how many more there were."""
    texts, cur = [], []
    for n in items:
        if cur and len(f'{head}: ' + sep.join(cur + [n])) > CUT:
            texts.append(f'{head}: ' + sep.join(cur))
            cur = []
        cur.append(n)
    if cur:
        texts.append(f'{head}: ' + sep.join(cur))
    if room < 1:
        return []
    if len(texts) > room:
        left = len(texts) - room + 1
        texts = texts[:room - 1] + [f'{head}: {left} more notice(s) of these, in the log '
                                    'and in vecrange_out/']
    return texts


def enabled():
    """The steps the file steps names, or every step without it."""
    if not os.path.exists(STEPS):
        return list(ALL)
    with open(STEPS) as f:
        return [w for line in f for w in line.split('#', 1)[0].split()]


def load_tool(tree, *path):
    spec = importlib.util.spec_from_file_location(path[-1][:-3], os.path.join(tree, *path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def make_tree(patch):
    """A copy of the checkout with the patch applied, and how it was found;
    no copy if the patch neither applies nor is in the checkout."""
    tmp = tempfile.mkdtemp(prefix='vecrange_')
    shutil.copytree(ROOT, tmp, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('.git', 'vecrange_out', 'eva_out', 'diagpush_*'))
    if patch is None:
        return tmp, 'the checkout as it is'
    path = os.path.join(HERE, patch)
    fwd = subprocess.run(['patch', '-p1', '--forward', '--dry-run', '-i', path],
                         cwd=tmp, capture_output=True, text=True)
    if fwd.returncode == 0:
        subprocess.run(['patch', '-p1', '--forward', '-i', path], cwd=tmp, check=True,
                       stdout=subprocess.DEVNULL)
        return tmp, 'the patch applied to a copy of the checkout'
    rev = subprocess.run(['patch', '-p1', '-R', '--dry-run', '-i', path],
                         cwd=tmp, capture_output=True, text=True)
    if rev.returncode == 0:
        return tmp, 'the patch already in the checkout'
    shown(fwd.stdout + fwd.stderr)
    shutil.rmtree(tmp, ignore_errors=True)
    return None, 'the patch neither applies nor is in the checkout'


def wp_run(tree, agreement, version, run):
    """WP on FN in tree, as frama-c-vec-range runs it, with the provers of run.
    Returns WP's summary (proved, total, {category: count}), the report's
    goals (goal -> (passed, {prover: time})) and the minutes it took."""
    path, model, cpp = agreement.UNITS[UNIT]
    d = os.path.join(OUT, version)
    os.makedirs(d, exist_ok=True)
    txt, rep = os.path.join(d, f'{run}.txt'), os.path.join(d, f'{run}.json')
    for f in (txt, rep):
        if os.path.exists(f):
            os.remove(f)
    provers = ','.join(PROVERS) if run == 'all' else run
    cmd = (['frama-c', '-wp', '-wp-rte',
            '-wp-model', model,
            '-wp-prover', provers,
            '-wp-timeout', '120',
            '-wp-split',
            '-wp-fct', FN,
            '-wp-report-json', rep]
           + agreement.EXTRA.get(UNIT, [])
           + [f'-cpp-extra-args={cpp}', path])
    print(f'\n=== {version}, {run}: ' + ' '.join(cmd), flush=True)
    start = time.time()
    with open(txt, 'w') as f:
        # A session of its own, so that a stop takes the provers with it.
        p = subprocess.Popen(cmd, cwd=tree, stdout=f, stderr=subprocess.STDOUT,
                             start_new_session=True)
        try:
            code = p.wait(timeout=LIMIT)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()
            code = None
    minutes = (time.time() - start) / 60
    with open(txt, errors='replace') as f:
        text = f.read()
    s = agreement.summary(text)
    if code is None or s is None or not os.path.exists(rep):
        shown('\n'.join(text.splitlines()[-30:]))
        why = (f'stopped after {LIMIT // 60} minutes' if code is None
               else f'frama-c exit {code}: no summary or no report')
        sys.exit(f'::error title=vecrange {version}::{run}: {why}')
    try:
        goals, _ = agreement.load(rep)
    except (OSError, ValueError) as e:
        sys.exit(f'::error title=vecrange {version}::{run}: the report cannot be read: {e}')
    cats = '; '.join(f'{k} {v}' for k, v in s[2].items())
    print(f'frama-c exit {code} after {minutes:.1f} minutes: Proved goals {s[0]} / {s[1]} '
          f'({cats}); the report: {len(goals)} goals')
    return s, goals, minutes


def pin_lines(agreement):
    """The pin file's lines for FN, as "kind provers name"."""
    path = os.path.join(agreement.PINS, UNIT + '.txt')
    lines = set()
    with open(path) as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if line and len(line.split()) == 3 and line.split()[2].startswith(PREFIX):
                lines.add(' '.join(line.split()))
    return lines


def took(goals, g, name):
    """The time the run's report gives name on goal g, or None."""
    if g not in goals:
        return None
    ok, succ = goals[g]
    return succ.get(name) if ok else None


def best(goals, g):
    """(prover, time) of the run's proof of g, ('WP', 0) without a prover, or None."""
    if g not in goals or not goals[g][0]:
        return None
    succ = goals[g][1]
    smt = {n: t for n, t in succ.items() if n in ('Alt-Ergo', 'Z3', 'CVC5')}
    if smt:
        n = min(smt, key=smt.get)
        return n, smt[n]
    return 'WP', 0.0


def step_version(version, patch, what):
    tree, how = make_tree(patch)
    print(f'{version}, {what}: {how}' + (f': {tree}' if tree else ''), flush=True)
    if tree is None:
        notice(f'vecrange {version}', f'{version}, {what}: {how}; not measured')
        return
    agreement = load_tool(tree, 'tools', 'agreement', 'agreement.py')
    res = {run: wp_run(tree, agreement, version, run) for run in RUNS}
    s_all, g_all, m_all = res['all']
    have = {p: (res[p][1], res[p][0], None) for p in PROVERS}
    o = agreement.sort_goals(have)
    ids = sorted(set(o['ids']) | set(g_all))
    names = agreement.NAMES

    # Each goal: the three together, then each alone, in the log.
    rows, timed = [], []
    for g in ids:
        b = best(g_all, g)
        together = f'{b[0]} {b[1]:.1f} s' if b and b[0] != 'WP' else ('WP' if b else 'NOT PROVED')
        alone = []
        for p in PROVERS:
            t = took(res[p][1], g, names[p])
            if t is not None:
                alone.append(f'{names[p]} {t:.1f} s')
            elif g in res[p][1] and res[p][1][g][0]:
                alone.append(f'{names[p]} WP')
            else:
                alone.append(f'{names[p]} -')
        rows.append(f'{short(g)} | together {together} | alone {", ".join(alone)} | '
                    f'{agreement.state(o, g)}')
        longest = max([b[1] if b else 0.0] + [took(res[p][1], g, names[p]) or 0.0
                                               for p in PROVERS])
        if longest > SHOWN:
            timed.append((longest, f'{short(g)}: together {together}; alone '
                                   + ', '.join(alone)))
    print(f'\n--- {version}: each goal of {FN} ({len(ids)}), with the three provers '
          'together and each alone ---')
    print('\n'.join(rows))
    print('--- end ---')

    unproved = [g for g in ids if g in g_all and not g_all[g][0]]
    proofs = [(b[1], b[0], g) for g in ids for b in [best(g_all, g)] if b and b[0] != 'WP']
    slowest = max(proofs) if proofs else None
    over = sum(1 for t, _, _ in proofs if t > SLOW)

    # The pin file's lines for FN, as this run would write them.
    pinned = pin_lines(agreement)
    now = {f'{k} {p} {g}' for g, (k, p) in o['cls'].items() if g.startswith(PREFIX)}
    now |= {f'slow {"+".join(sorted(ts))} {g}' for g, ts in o['slow'].items()
            if g.startswith(PREFIX)}
    gone, came = sorted(pinned - now), sorted(now - pinned)
    pins = agreement.read_pins(UNIT)
    diffs = [d for d in agreement.compare(o, pins) if d.startswith(PREFIX)] if pins else []
    print(f'--- {version}: the pin file\'s lines for {FN} that would go ({len(gone)}) ---')
    print('\n'.join(gone) or '(none)')
    print(f'--- {version}: and that would come ({len(came)}) ---')
    print('\n'.join(came) or '(none)')
    print(f'--- {version}: differences as 13 · Prover agreement reads them, slow goals left '
          f'out ({len(diffs)}) ---')
    print('\n'.join(diffs) or '(none)')

    inv5 = [g for g in ids if short(g).startswith(INV5)]
    summary = {
        'version': version, 'what': what, 'how': how,
        'together': {'proved': s_all[0], 'total': s_all[1], 'cats': s_all[2],
                     'unproved': [short(g) for g in unproved],
                     'slowest': [slowest[0], slowest[1], short(slowest[2])] if slowest else None,
                     'over': over, 'minutes': round(m_all, 1)},
        'alone': {names[p]: [res[p][0][0], res[p][0][1], round(res[p][2], 1)] for p in PROVERS},
        'classes': {'wp': len(o['wp']), 'three': len(o['three']), 'two': o['n_two'],
                    'one': o['n_one'], 'none': len(o['none']),
                    'two_by': {k: len(v) for k, v in o['two'].items()},
                    'one_by': {k: len(v) for k, v in o['one'].items() if v}},
        'slow_alone': {short(g): ts for g, ts in o['slow'].items()},
        'pins': {'gone': gone, 'came': came, 'diffs': diffs},
        'goals': {short(g): {'together': list(best(g_all, g)) if best(g_all, g) else None,
                             'alone': {names[p]: took(res[p][1], g, names[p]) for p in PROVERS}}
                  for g in ids},
        'inv5': [short(g) for g in inv5],
    }
    with open(os.path.join(OUT, version, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=1, sort_keys=True)

    cats = '; '.join(f'{k} {v}' for k, v in s_all[2].items())
    low = (f'slowest proof {secs(slowest[0])} ({slowest[1]}, {short(slowest[2])})'
           if slowest else 'no proof by a prover')
    notice(f'vecrange {version}',
           f'{version}, {what} ({how}). Alt-Ergo, Z3 and CVC5 together, as frama-c-vec-range '
           f'runs them: {FN} {s_all[0]} / {s_all[1]} ({cats}); not proved {len(unproved)}; '
           f'{low}; proofs over {SLOW:.0f} s {over}; {m_all:.0f} minutes')
    alone = ', '.join(f'{names[p]} {res[p][0][0]} / {res[p][0][1]} ({res[p][2]:.0f} minutes)'
                      for p in PROVERS)
    two = ', '.join(f'{k} {len(v)}' for k, v in sorted(o['two'].items())) or 'none'
    one = ', '.join(f'{k} {len(v)}' for k, v in o['one'].items() if v) or 'none'
    notice(f'vecrange {version} alone',
           f'{version}, each prover alone: {alone}. Of the reports\' {len(o["ids"])} goals: by WP '
           f'itself {len(o["wp"])}, three provers {len(o["three"])}, two {o["n_two"]} ({two}), '
           f'one {o["n_one"]} ({one}), none {len(o["none"])}; over {SLOW:.0f} s with a prover '
           f'alone {len(o["slow"])}')
    notice(f'vecrange {version} pins',
           f'{version} against tools/agreement/pins/{UNIT}.txt, its {len(pinned)} lines for {FN}: '
           f'{len(diffs)} difference(s) as 13 reads them, slow goals left out. Lines that would '
           f'go ({len(gone)}): {"; ".join(gone) or "none"}. Lines that would come '
           f'({len(came)}): {"; ".join(came) or "none"}')
    used = 3
    if unproved or o['none']:
        notice(f'vecrange {version} unproved',
               f'{version} not proved together ({len(unproved)}): '
               f'{" ".join(short(g) for g in unproved) or "none"}; by no prover alone '
               f'({len(o["none"])}): {" ".join(short(g) for g in o["none"]) or "none"}')
        used += 1
    timed = [t for _, t in sorted(timed, reverse=True)]
    for text in grouped(f'{version} over {SHOWN:.0f} s ({len(timed)})', timed, STEP - used):
        notice(f'vecrange {version} times', text)


def step_report():
    done, missing = [], []
    for version, _, what in VERSIONS:
        path = os.path.join(OUT, version, 'summary.json')
        if os.path.exists(path):
            with open(path) as f:
                done.append(json.load(f))
        else:
            missing.append(version)
    if not done:
        notice('vecrange report', f'no version measured: {" ".join(missing)} have no summary')
        return
    base = next((s for s in done if s['version'] == 'P0'), None)
    for s in done:
        t, c = s['together'], s['classes']
        low = (f'slowest {secs(t["slowest"][0])} ({t["slowest"][1]}, {t["slowest"][2]})'
               if t['slowest'] else 'no proof by a prover')
        inv = []
        for g in s['inv5']:
            r = s['goals'][g]
            tg = (f'{r["together"][0]} {secs(r["together"][1])}' if r['together']
                  else 'not proved')
            al = ', '.join(f'{n} {secs(r["alone"][n])}' if r['alone'].get(n) is not None
                           else f'{n} -' for n in ('Alt-Ergo', 'Z3', 'CVC5'))
            inv.append(f'{g[len(INV5) + 1:] or "whole"}: together {tg}, alone {al}')
        new = ''
        if base is not None and s is not base:
            added = sorted(set(s['goals']) - set(base['goals']))
            gone = sorted(set(base['goals']) - set(s['goals']))
            new = f'; goals new against P0 {len(added)}, gone {len(gone)}'
            if added:
                worst = max(added, key=lambda g: (s['goals'][g]['together'] or ['', 999])[1])
                w = s['goals'][worst]['together']
                new += (f', the new goals\' slowest proof together {secs(w[1])} ({worst})'
                        if w else f', not proved together: {worst}')
        notice(f'vecrange report {s["version"]}',
               f'{s["version"]}, {s["what"]}: together {t["proved"]} / {t["total"]}, not proved '
               f'{len(t["unproved"])}, {low}, over {SLOW:.0f} s {t["over"]}; alone: none '
               f'{c["none"]}, one {c["one"]}, two {c["two"]}, over {SLOW:.0f} s '
               f'{len(s["slow_alone"])}; the pin file\'s lines for {FN}: {len(s["pins"]["gone"])} '
               f'would go, {len(s["pins"]["came"])} come{new}. Invariant 5 preserved '
               f'({len(s["inv5"])} part(s)): ' + ('; '.join(inv) or 'no goal'))
    if missing:
        notice('vecrange report', f'not measured: {" ".join(missing)}')


def main(argv):
    steps = argv[1:] or enabled()
    bad = [s for s in steps if s not in ALL]
    if bad:
        sys.exit(f'unknown step(s): {" ".join(bad)}; the steps are {", ".join(ALL)}')
    os.makedirs(OUT, exist_ok=True)
    on = enabled()
    for s in steps:
        if s not in on:
            print(f'vecrange: {s} is not in {os.path.relpath(STEPS, ROOT)}; not run', flush=True)
            notice('vecrange', f'{s}: not run; {os.path.relpath(STEPS, ROOT)} names '
                               f'{" ".join(on) or "no step"}')
            continue
        if s == 'report':
            step_report()
            continue
        _, patch, what = next(v for v in VERSIONS if v[0] == s)
        step_version(s, patch, what)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
