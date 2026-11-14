#!/usr/bin/env python3
"""
vecrange_probe.py -- VERIFY-079's probe: the two slow goals of
vec_int_extend_from_range's loop, measured with changes to its annotations
before data/vec/vec_range.h changes.

frama-c-vec-range proves every goal of vec_int_extend_from_range, but two of
them only with Alt-Ergo: parts 3 and 4 of the preservation of loop invariant
5, which took Alt-Ergo alone 89 s and 38 s in VERIFY-065's runs and up to
76 s and 38 s in 13 · Prover agreement's runs #6 to #11, against the 120 s
limit. A slower runner could lose them, and the proof job would fail its pin.
Invariant 5 states the values the loop has stored so far:

    \\forall integer k; 0 <= k < i
      ==> v->items[v->len + k] == \\at(r.current, Pre) + k * r.step

WP splits each goal of the loop's body on the paths through the round-trip
check (-wp-split): parts 1 and 2 are the two that return, which WP proves
itself, and parts 3 and 4 the two that reach the store. The probe measures
the function with versions of the header, each applied to a copy of the
checkout:

  P0  the header as it is, the control;
  A   three assertions in the loop's body (variants/a_asserts.patch): after
      the round-trip check, that value is the range's i-th value (ith_value)
      and that elem equals it (fits); after the store, that the slot holds it
      (ith_stored);
  B   invariant 5 over values no store changes (variants/b_anchored.patch):
      v->items[\\at(v->len, Pre) + k] == \\at(r.current, Pre) + k * \\at(r.step, Pre),
      and a new invariant after it, len_kept: v->len == \\at(v->len, Pre);
  C   both (variants/c_both.patch);
  D   after the store, kept: each earlier slot holds what it held at the
      iteration's start, \\at(v->items[v->len + k], LoopCurrent)
      (variants/d_kept.patch);
  E   D, and the same after the call to range_next, kept_call
      (variants/e_kept_call.patch): the frame across the call and the frame
      across the store, each its own goal;
  R   the header as it is, under WP's by-reference model, Typed+Cast+ref, in
      which the range, passed to range_next by its address, may stay out of
      memory.

VERIFY-079's run 1 measured P0 and A to C; its run 1b, P0, D, E and R.

Each version's step runs WP on vec_int_extend_from_range alone (-wp-fct),
with frama-c-vec-range's options as tools/agreement/agreement.py lists them
(R's model aside): once with Alt-Ergo, Z3 and CVC5 together, as the proof
job runs them, and once with each alone, as 13 · Prover agreement runs them.
It reports the goals proved and not, each goal's prover and time, the goals
one or two provers prove alone as tools/agreement/pins/ lists them, and the
pin file's lines for the function that would go and come. WP names a goal
after its model, so the probe names each by frama-c-vec-range's, typed_cast.
The other steps:

  goals   P0's invariants, as WP gives them to the provers: WP with Alt-Ergo,
          -wp-prop @invariant and -wp-print, its print in the log; and,
          from -wp-out, the provers' files for invariant 5's preservation,
          parts 3 and 4, in the log;
  report  the versions the file steps names, compared from what their steps
          wrote: counts, the slowest proof, and the watched goals,
          invariant 5's preservation, kept and kept_call.

The file steps, next to this script, names the steps that run; a step it
does not name posts that it did not run. Without the file, every step runs.

It writes notices for each step and prints each goal's results in the log.
GitHub reads a workflow command at the start of a line even behind spaces,
so the probe prints each line of WP's output that would be one behind "| ".
It never fails on what it measures. It fails only if P0's WP gives no
summary or no report, or is stopped at LIMIT: P0 is the checkout as it is,
so that is the probe or its setup failing. Another version whose WP does
so, or whose patch neither applies nor is found applied, is reported and
not measured; so is the goals step's print.

Each step's files go to vecrange_out/ in the directory it is run from:
VERSION/all.txt and .json, WP's output and report with the three provers
together; VERSION/alt-ergo.txt, z3.txt, cvc5.txt and their .json, each
alone; VERSION/summary.json, what the step report reads; goals/, the goals
step's output and WP's -wp-out.

    python3 tools/probes/verify-079/vecrange_probe.py [P0|A|...|R|goals|report]...
    python3 tools/probes/verify-079/vecrange_probe.py --nth N

With no argument it runs every step the file steps names. With --nth N it
runs the N-th step the file names, or nothing past the file's last: the
job in probes.yml runs N = 1 to 8, each in a step of its own, so the file
alone decides what runs.
"""
import glob
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
PREFIX = f'typed_cast_{FN}_'   # frama-c-vec-range's model, Typed+Cast, names its goals so
PROVERS = ['alt-ergo', 'z3', 'cvc5']
SHOWN_PROVERS = ['Alt-Ergo', 'Z3', 'CVC5']
RUNS = ['all'] + PROVERS   # all: the three together, as the proof job runs them
VERSIONS = [
    # name, patch, what, WP's model when not frama-c-vec-range's
    ('P0', None, 'the header as it is', None),
    ('A', 'variants/a_asserts.patch', 'three assertions in the loop', None),
    ('B', 'variants/b_anchored.patch', 'invariant 5 over values at Pre, and len_kept', None),
    ('C', 'variants/c_both.patch', 'A and B', None),
    ('D', 'variants/d_kept.patch', 'kept, the earlier slots as at the iteration\'s start, '
                                   'after the store', None),
    ('E', 'variants/e_kept_call.patch', 'D, and kept_call, the same after the call to '
                                        'range_next', None),
    ('R', None, 'the header as it is, under WP\'s by-reference model', 'Typed+Cast+ref'),
]
CONTROL = 'P0'
ALL = [v[0] for v in VERSIONS] + ['goals', 'report']
WATCH = re.compile(r'^(loop_invariant_5_preserved|assert_kept|assert_kept_call)(_part\d+)?$')
INV5 = 'loop_invariant_5_preserved'
CUT = 4000        # the runner cuts an annotation at 4096 characters
STEP = 10         # GitHub keeps ten notices of a step
SHOWN = 10.0      # seconds: a goal that took a run longer is named in the notices
SLOW = 30.0       # seconds: 13 · Prover agreement's slow, tools/agreement/agreement.py's SLOW
LIMIT = 60 * 60   # seconds: a WP run still going after this is stopped, and gives no data
FILE_LINES = 4000  # lines of a prover's file printed in the log


def notice(title, msg):
    print(f'::notice title={title}::{msg[:CUT]}', flush=True)


def shown(text):
    """A tool's output in the log, none of its lines a workflow command."""
    for l in text.splitlines():
        print('| ' + l if l.lstrip().startswith(('::', '##[')) else l)
    sys.stdout.flush()


def short(goal):
    """A goal's name without WP's model and the function's name."""
    i = goal.find(f'_{FN}_')
    return goal[i + len(FN) + 2:] if i >= 0 else goal


def canon(goal):
    """A goal's name as frama-c-vec-range's model gives it."""
    return PREFIX + short(goal) if f'_{FN}_' in goal else goal


def secs(t):
    """A time in seconds as the notices give it: tenths under 10 s."""
    return f'{t:.1f} s' if t < 10 else f'{t:.0f} s'


def shown_result(r):
    """A run's result for a goal: a prover's time, WP, or - for not proved."""
    if r is None:
        return '-'
    if r == 'WP':
        return 'WP'
    return secs(r)


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


def wp_run(tree, agreement, where, run, model=None, extra=()):
    """WP on FN in tree, as frama-c-vec-range runs it, with the provers of run
    and, if given, another model and more options. Returns WP's summary
    (proved, total, {category: count}), the report's goals by their names
    under frama-c-vec-range's model (goal -> (passed, {prover: time})) and
    the minutes it took; or, if WP gave no summary or no report or was
    stopped, a text saying so."""
    path, unit_model, cpp = agreement.UNITS[UNIT]
    d = os.path.join(OUT, where)
    os.makedirs(d, exist_ok=True)
    txt, rep = os.path.join(d, f'{run}.txt'), os.path.join(d, f'{run}.json')
    for f in (txt, rep):
        if os.path.exists(f):
            os.remove(f)
    provers = ','.join(PROVERS) if run == 'all' else run
    cmd = (['frama-c', '-wp', '-wp-rte',
            '-wp-model', model or unit_model,
            '-wp-prover', provers,
            '-wp-timeout', '120',
            '-wp-split',
            '-wp-fct', FN,
            '-wp-report-json', rep]
           + list(extra)
           + agreement.EXTRA.get(UNIT, [])
           + [f'-cpp-extra-args={cpp}', path])
    print(f'\n=== {where}, {run}: ' + ' '.join(cmd), flush=True)
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
        return (f'{run}: stopped after {LIMIT // 60} minutes' if code is None
                else f'{run}: frama-c exit {code}, no summary or no report')
    try:
        goals, _ = agreement.load(rep)
    except (OSError, ValueError) as e:
        return f'{run}: the report cannot be read: {e}'
    goals = {canon(g): r for g, r in goals.items()}
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


def timed_result(r):
    """Whether a run's result for a goal is a prover's time."""
    return isinstance(r, (int, float)) and not isinstance(r, bool)


def alone_result(goals, g, name):
    """A prover's run alone on goal g: its time, 'WP' if WP proved it itself,
    or None."""
    if g not in goals or not goals[g][0]:
        return None
    t = goals[g][1].get(name)
    return t if t is not None else 'WP'


def best(goals, g):
    """(prover, time) of the run's proof of g, ('WP', 0) without a prover, or None."""
    if g not in goals or not goals[g][0]:
        return None
    succ = goals[g][1]
    smt = {n: t for n, t in succ.items() if n in SHOWN_PROVERS}
    if smt:
        n = min(smt, key=smt.get)
        return n, smt[n]
    return 'WP', 0.0


def step_version(version, patch, what, model):
    tree, how = make_tree(patch)
    if model:
        how += f', WP\'s model {model}'
    print(f'{version}, {what}: {how}' + (f': {tree}' if tree else ''), flush=True)
    if tree is None:
        notice(f'vecrange {version}', f'{version}, {what}: {how}; not measured')
        return
    agreement = load_tool(tree, 'tools', 'agreement', 'agreement.py')
    res = {}
    for run in RUNS:
        r = wp_run(tree, agreement, version, run, model=model)
        if isinstance(r, str):
            if version == CONTROL:
                sys.exit(f'::error title=vecrange {version}::{r}')
            notice(f'vecrange {version}', f'{version}, {what} ({how}): not measured; {r}')
            return
        res[run] = r
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
            r = alone_result(res[p][1], g, names[p])
            alone.append(f'{names[p]} ' + (f'{r:.1f} s' if timed_result(r) else
                                           shown_result(r)))
        rows.append(f'{short(g)} | together {together} | alone {", ".join(alone)} | '
                    f'{agreement.state(o, g)}')
        times = [b[1] if b else 0.0] + [r for p in PROVERS
                                        for r in [alone_result(res[p][1], g, names[p])]
                                        if timed_result(r)]
        if max(times) > SHOWN:
            timed.append((max(times), f'{short(g)}: together {together}; alone '
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
                             'alone': {names[p]: alone_result(res[p][1], g, names[p])
                                       for p in PROVERS}}
                  for g in ids},
    }
    with open(os.path.join(OUT, version, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=1, sort_keys=True)

    cats = '; '.join(f'{k} {v}' for k, v in s_all[2].items())
    low = (f'slowest proof {secs(slowest[0])} ({slowest[1]}, {short(slowest[2])})'
           if slowest else 'no proof by a prover')
    notice(f'vecrange {version}',
           f'{version}, {what} ({how}). Alt-Ergo, Z3 and CVC5 together, as frama-c-vec-range '
           f'runs them: {FN} {s_all[0]} / {s_all[1]} ({cats}); not proved {len(unproved)}; '
           f'{low}; proofs over {SLOW:.0f} s {over}; {m_all:.0f} min')
    alone = ', '.join(f'{names[p]} {res[p][0][0]} / {res[p][0][1]} ({res[p][2]:.0f} min)'
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


def step_goals():
    """P0's invariants as WP gives them to the provers, in the log."""
    tree, how = make_tree(None)
    agreement = load_tool(tree, 'tools', 'agreement', 'agreement.py')
    d = os.path.join(OUT, 'goals')
    wp_out = os.path.join(d, 'wp')
    shutil.rmtree(wp_out, ignore_errors=True)
    os.makedirs(wp_out, exist_ok=True)
    picked = '-wp-prop @invariant'
    r = wp_run(tree, agreement, 'goals', 'alt-ergo',
               extra=['-wp-prop', '@invariant', '-wp-print', '-wp-out', wp_out])
    if not isinstance(r, str) and r[0][1] == 0:
        print('goals: -wp-prop @invariant selected no goal; again with every goal', flush=True)
        picked = 'every goal, -wp-prop @invariant having selected none'
        shutil.rmtree(wp_out, ignore_errors=True)
        os.makedirs(wp_out, exist_ok=True)
        r = wp_run(tree, agreement, 'goals', 'alt-ergo', extra=['-wp-print', '-wp-out', wp_out])
    if isinstance(r, str):
        notice('vecrange goals', f'goals: not printed; {r}')
        return
    s, goals, minutes = r
    with open(os.path.join(d, 'alt-ergo.txt'), errors='replace') as f:
        text = f.read()
    print(f'\n=== goals: WP\'s print, P0, Alt-Ergo, {picked}: begin ===')
    shown(text)
    print('=== goals: WP\'s print: end ===')
    files = sorted(p for p in glob.glob(os.path.join(wp_out, '**', '*'), recursive=True)
                   if os.path.isfile(p))
    mine = [p for p in files if INV5 in os.path.basename(p)]
    printed = [p for p in mine if re.search(r'part[34]\b|part[34][^0-9]', os.path.basename(p))]
    print(f'\n=== goals: -wp-out holds {len(files)} file(s); invariant 5\'s preservation\'s: '
          f'{len(mine)} ===')
    for p in files[:200]:
        print(os.path.relpath(p, wp_out))
    for p in printed:
        with open(p, errors='replace') as f:
            lines = f.read().splitlines()
        print(f'\n=== goals: {os.path.relpath(p, wp_out)} ({len(lines)} lines): begin ===')
        shown('\n'.join(lines[:FILE_LINES]))
        if len(lines) > FILE_LINES:
            print(f'... {len(lines) - FILE_LINES} more lines, in vecrange_out/goals/wp/')
        print('=== goals: file: end ===')
    inv5 = sorted(short(g) for g in goals if short(g).startswith(INV5))
    notice('vecrange goals',
           f'goals: WP\'s print of P0 with Alt-Ergo, {picked}: {s[0]} / {s[1]}, '
           f'{len(text.splitlines())} lines, in the log between "=== goals: WP\'s print" lines; '
           f'invariant 5\'s preservation in the report: {" ".join(inv5) or "no goal"}. '
           f'-wp-out: {len(files)} file(s), {len(mine)} of invariant 5\'s preservation, '
           f'{len(printed)} printed in the log (parts 3 and 4): '
           + (' '.join(os.path.relpath(p, wp_out) for p in printed) or 'none'))


def step_report():
    on = enabled()
    wanted = [v for v in VERSIONS if v[0] in on]
    done, missing = [], []
    for version, _, _, _ in wanted:
        path = os.path.join(OUT, version, 'summary.json')
        if os.path.exists(path):
            with open(path) as f:
                done.append(json.load(f))
        else:
            missing.append(version)
    if not done:
        notice('vecrange report',
               f'no version measured: {" ".join(missing) or "none named"} have no summary')
        return
    base = next((s for s in done if s['version'] == CONTROL), None)
    for s in done:
        t, c = s['together'], s['classes']
        low = (f'slowest {secs(t["slowest"][0])} ({t["slowest"][1]}, {t["slowest"][2]})'
               if t['slowest'] else 'no proof by a prover')
        watched = []
        for g in sorted(s['goals']):
            if not WATCH.match(g):
                continue
            r = s['goals'][g]
            tg = r['together']
            tg = ('not proved' if tg is None else 'WP' if tg[0] == 'WP'
                  else f'{tg[0]} {secs(tg[1])}')
            al = ', '.join(f'{n} {shown_result(r["alone"].get(n))}' for n in SHOWN_PROVERS)
            watched.append(f'{g}: together {tg}, alone {al}')
        new = ''
        if base is not None and s is not base:
            added = sorted(set(s['goals']) - set(base['goals']))
            gone = sorted(set(base['goals']) - set(s['goals']))
            new = f'; goals new against {CONTROL} {len(added)}, gone {len(gone)}'
        notice(f'vecrange report {s["version"]}',
               f'{s["version"]}, {s["what"]}: together {t["proved"]} / {t["total"]}, not proved '
               f'{len(t["unproved"])}, {low}, over {SLOW:.0f} s {t["over"]}; alone: none '
               f'{c["none"]}, one {c["one"]}, two {c["two"]}, over {SLOW:.0f} s '
               f'{len(s["slow_alone"])}; the pin file\'s lines for {FN}: {len(s["pins"]["gone"])} '
               f'would go, {len(s["pins"]["came"])} come{new}. Watched: '
               + ('; '.join(watched) or 'no goal'))
    if missing:
        notice('vecrange report', f'not measured: {" ".join(missing)}')


def main(argv):
    if len(argv) == 3 and argv[1] == '--nth':
        # probes.yml's slots: the n-th step the file steps names, each in a
        # step of its own, since GitHub keeps ten notices of a step.
        on, n = enabled(), int(argv[2])
        if n > len(on):
            print(f'vecrange: {os.path.relpath(STEPS, ROOT)} names {len(on)} step(s); '
                  f'no step {n}', flush=True)
            return 0
        argv = [argv[0], on[n - 1]]
        print(f'vecrange: step {n} of {os.path.relpath(STEPS, ROOT)}: {on[n - 1]}', flush=True)
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
        elif s == 'goals':
            step_goals()
        else:
            _, patch, what, model = next(v for v in VERSIONS if v[0] == s)
            step_version(s, patch, what, model)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
