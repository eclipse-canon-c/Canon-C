#!/usr/bin/env python3
"""
order_probe.py -- VERIFY-084's probe: compare.h's 24 comparators as total
orders, proved by WP through wrapper functions whose calls are inlined,
measured in CI before a proof job pins them.

vmacros/vdrivers/compare_order_verify.h, written by gen_order.py, has for
each comparator three functions whose assertions state that it is
reflexive, antisymmetric and transitive. Run 1 measured it here, in this
directory; since run 2 it is there, and the job frama-c-compare-order
(proofs-core.yml) proves it as the probe's together part did. The probe is
closed: its file steps names no part. WP runs on it as frama-c-compare
runs on compare.h (tools/agreement/agreement.py's UNITS: Typed+Cast,
-wp-rte, -wp-split, 120 s, the same preprocessing), and with -inline-calls
and -remove-inlined, each given the 24 comparators: each call is replaced by
the comparator's body, and the comparators leave the unit.

The parts, each a job of the probe's matrix in probes.yml and one WP run:

  together     the files first (below); then the driver with Alt-Ergo, Z3
               and CVC5 together, as a proof job runs them
  alt-ergo, z3, cvc5
               the driver with each prover alone, as 13 · Prover agreement
               runs them
  contract     the control without inlining: the driver's four kinds of
               assertion only (-wp-prop), each call proved from the
               comparator's contract, which states -1 <= result <= 1 and
               nothing that relates two calls
  false        the control of false assertions: compare_order_false.h with
               the inlining, its assertions symmetric and self_equal only
  cortex_m4, rv64_lp64, msvc_x86_64
               the driver with the three provers together at one model of
               each class of 11 · Proofs: machine models (ilp32, lp64,
               llp64), with the model's arguments as proofs-machdeps.yml
               gives them

and report, which reads what the others wrote (their artifacts, downloaded)
and compares them: the goals one or two provers prove alone, as a pin file
of 13 · Prover agreement would list them; each model against x86_64; the
two controls.

The together part first checks the files: gen_order.py --check; and, with
Frama-C but not WP, each driver as the inlining leaves it (-print): its
functions, its assertions, and that no comparator's name is left in it;
and whether -inline-calls @all leaves the driver's functions the same.

It writes notices and prints each goal's results in the log. GitHub reads
a workflow command at the start of a line even behind spaces, so the probe
prints each line of a tool's output that would be one behind "| ". It never
fails on what it measures. A part fails only if its WP gives no summary or
no report, or is stopped at LIMIT; report fails only if no part wrote a
summary.

The file steps, next to this script, names the parts that run; a part it
does not name posts that it did not run, and report that no part ran.
Without the file, every part runs. "named PART" prints run=true or
run=false, for the workflow to skip Frama-C's setup for a part not named,
and "named any" whether it names a part, for the report's download.

Each part's files go to order_out/PART/ in the directory it is run from,
which must be the repository's root: wp.txt and wp.json, WP's output and
report, and summary.json, what report reads; the together part's files
check, to order_out/files/.

    python3 tools/probes/verify-084/order_probe.py PART
    python3 tools/probes/verify-084/order_probe.py report [DIR]
    python3 tools/probes/verify-084/order_probe.py named PART|any
"""
import importlib.util
import json
import os
import re
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True   # no __pycache__ beside agreement.py or gen_order.py

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
STEPS = os.path.join(HERE, 'steps')
OUT = os.path.abspath('order_out')
UNIT = 'frama-c-compare'   # the driver is proved with its options
DRIVER = 'vmacros/vdrivers/compare_order_verify.h'
FALSE = 'tools/probes/verify-084/compare_order_false.h'
MACHDEPS = '.github/workflows/proofs-machdeps.yml'
PROVERS = ['alt-ergo', 'z3', 'cvc5']
PROPS = ['reflexive', 'antisymmetric', 'transitive_lt', 'transitive_eq']
FALSE_PROPS = ['symmetric', 'self_equal']
MODELS = ['cortex_m4', 'rv64_lp64', 'msvc_x86_64']   # one of each class: ilp32, lp64, llp64
PARTS = ['together'] + PROVERS + ['contract', 'false'] + MODELS
FLOATS = ('f32', 'f64')
GOAL = re.compile(r'^typed(?:_cast)?_order_(?:false_)?([a-z0-9]+?)(_desc)?_'
                  r'(reflexive|antisymmetric|transitive|symmetric|self_equal)_(.*)$')
CUT = 4000         # the runner cuts an annotation at 4096 characters
STEP = 10          # GitHub keeps ten notices of a step
SHOWN = 10.0       # seconds: a proof that took longer is named in the notices
SLOW = 30.0        # seconds: 13 · Prover agreement's slow (agreement.py's SLOW)
LIMIT = 230 * 60   # seconds: a WP run still going is stopped, and gives no data


def notice(title, msg):
    print(f'::notice title={title}::{msg[:CUT]}', flush=True)


def shown(text):
    """A tool's output in the log, none of its lines a workflow command."""
    for line in text.splitlines():
        print('| ' + line if line.lstrip().startswith(('::', '##[')) else line)
    sys.stdout.flush()


def secs(t):
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
        texts = texts[:room - 1] + [f'{head}: {left} more notice(s) of these, in the log']
    return texts


def enabled():
    """The parts the file steps names, or every part without it."""
    if not os.path.exists(STEPS):
        return list(PARTS)
    with open(STEPS) as f:
        return [w for line in f for w in line.split('#', 1)[0].split()]


def agreement_tool():
    spec = importlib.util.spec_from_file_location(
        'agreement', os.path.join(ROOT, 'tools', 'agreement', 'agreement.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def comparators():
    sys.path.insert(0, HERE)
    import gen_order   # noqa: E402, the drivers' own list
    return gen_order.NAMES


def model_args(name):
    """The model's arguments, as proofs-machdeps.yml's matrix gives them."""
    with open(os.path.join(ROOT, MACHDEPS)) as f:
        for line in f:
            m = re.match(r'^\s*- \{ name: (\S+),\s*class: (\S+),\s*args: "([^"]*)" \}', line)
            if m and m.group(1) == name:
                return m.group(2), m.group(3).split()
    sys.exit(f'::error title=order {name}::{name}: no such model in {MACHDEPS}')


def short(goal):
    """A goal's name without WP's model."""
    return re.sub(r'^typed(_cast)?_', '', goal)


def kind(goal):
    """What a goal is: one of the properties, or what else WP proves."""
    m = GOAL.match(goal)
    rest = m.group(4) if m else short(goal)
    p = re.match(r'assert_(' + '|'.join(PROPS + FALSE_PROPS) + r')(_part\d+)?$', rest)
    if p:
        return p.group(1)
    for k in ('rte', 'requires', 'ensures', 'assigns', 'terminates', 'exits'):
        if k in rest:
            return k
    return 'other'


def family(goal):
    """'float' or 'integer', after the comparator's type; None if not a wrapper's."""
    m = GOAL.match(goal)
    if not m:
        return None
    return 'float' if m.group(1) in FLOATS else 'integer'


def wrapper(goal):
    m = GOAL.match(goal)
    if not m:
        return None
    return f'{m.group(1)}{m.group(2) or ""}_{m.group(3)}'


def command(agreement, driver, provers, report, inline=True, props=None, extra_model=()):
    _, model, cpp = agreement.UNITS[UNIT]
    names = ','.join(comparators())
    cmd = ['frama-c', '-wp', '-wp-rte'] + list(extra_model) + [
        '-wp-model', model,
        '-wp-prover', provers,
        '-wp-timeout', '120',
        '-wp-split',
        '-wp-report-json', report]
    if inline:
        cmd += ['-inline-calls', names, '-remove-inlined', names]
    if props:
        cmd += ['-wp-prop', ','.join(props)]
    return cmd + agreement.EXTRA.get(UNIT, []) + [f'-cpp-extra-args={cpp}', driver]


def counts(text):
    """WP's verdict lines that are not Valid, by verdict."""
    out = {}
    for v in ('Timeout', 'Unknown', 'Failed', 'Failure', 'Invalid', 'Stepout'):
        out[v] = len(re.findall(rf'^\[wp\] \[{v}\]', text, re.M))
    return out


def wp(agreement, part, cmd):
    """Runs cmd from the repository's root, its output and report under
    order_out/part/. Returns (summary, goals, minutes, verdicts), or a text
    saying why there is no data."""
    d = os.path.join(OUT, part)
    os.makedirs(d, exist_ok=True)
    txt, rep = os.path.join(d, 'wp.txt'), os.path.join(d, 'wp.json')
    for f in (txt, rep):
        if os.path.exists(f):
            os.remove(f)
    print(f'\n=== {part}: ' + ' '.join(cmd), flush=True)
    start = time.time()
    with open(txt, 'w') as f:
        # A session of its own, so that a stop takes the provers with it.
        p = subprocess.Popen(cmd, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT,
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
        shown('\n'.join(text.splitlines()[-40:]))
        return (f'stopped after {LIMIT // 60} minutes' if code is None
                else f'frama-c exit {code}, no summary or no report')
    try:
        goals, _ = agreement.load(rep)
    except (OSError, ValueError) as e:
        return f'the report cannot be read: {e}'
    cats = '; '.join(f'{k} {v}' for k, v in s[2].items())
    print(f'frama-c exit {code} after {minutes:.1f} minutes: Proved goals {s[0]} / {s[1]} '
          f'({cats}); the report: {len(goals)} goals')
    warnings = [line for line in text.splitlines() if 'warning' in line.lower()]
    if warnings:
        print(f'--- {part}: WP\'s warnings ({len(warnings)}) ---')
        shown('\n'.join(warnings[:200]))
    return s, goals, minutes, counts(text)


def best(goals, g):
    """(prover, time) of the run's proof of g, ('WP', 0) without a prover, or None."""
    if g not in goals or not goals[g][0]:
        return None
    smt = {n: t for n, t in goals[g][1].items() if n in ('Alt-Ergo', 'Z3', 'CVC5')}
    if smt:
        n = min(smt, key=smt.get)
        return n, smt[n]
    return 'WP', 0.0


def by_property(goals, props):
    """{(property, family): [proved, total]} over the goals of those properties."""
    out = {}
    for g, (ok, _) in goals.items():
        k = kind(g)
        if k in props:
            c = out.setdefault(f'{k} {family(g)}', [0, 0])
            c[0] += ok
            c[1] += 1
    return out


def by_kind(goals):
    out = {}
    for g, (ok, _) in goals.items():
        c = out.setdefault(kind(g), [0, 0])
        c[0] += ok
        c[1] += 1
    return out


def files_check(agreement):
    """The together part's first step: the files as gen_order.py writes them,
    and the drivers as the inlining leaves them."""
    d = os.path.join(OUT, 'files')
    os.makedirs(d, exist_ok=True)
    r = subprocess.run([sys.executable, os.path.join(HERE, 'gen_order.py'), '--check'],
                       cwd=ROOT, capture_output=True, text=True)
    shown(r.stdout + r.stderr)
    gen = 'as gen_order.py writes them' if r.returncode == 0 else 'NOT as gen_order.py writes them'
    _, _, cpp = agreement.UNITS[UNIT]
    names = ','.join(comparators())
    texts, results = {}, []
    runs = [('verify', DRIVER, ['-inline-calls', names, '-remove-inlined', names]),
            ('false', FALSE, ['-inline-calls', names, '-remove-inlined', names]),
            ('verify-kept', DRIVER, ['-inline-calls', names]),
            ('verify-all', DRIVER, ['-inline-calls', '@all'])]
    for key, driver, opts in runs:
        out = os.path.join(d, f'{key}.c')
        if os.path.exists(out):
            os.remove(out)
        cmd = ['frama-c'] + opts + [f'-cpp-extra-args={cpp}', driver, '-print', '-ocode', out]
        print(f'\n=== files, {key}: ' + ' '.join(cmd), flush=True)
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        shown(r.stdout + r.stderr)
        if r.returncode or not os.path.exists(out):
            texts[key] = None
            results.append(f'{key}: frama-c exit {r.returncode}, no code printed')
            continue
        with open(out, errors='replace') as f:
            texts[key] = f.read()
    for key in ('verify', 'false'):
        t = texts.get(key)
        if t is None:
            continue
        funcs = re.findall(r'^void (order_\w+)\(', t, re.M)
        asserts = re.findall(r'assert (' + '|'.join(PROPS + FALSE_PROPS) + r'):', t)
        left = len(re.findall(r'algo_cmp_', t))
        # Each inlined body keeps its comparator's contract as a statement
        # contract; the wrappers' own contracts have no ensures.
        bodies = len(re.findall(r'^\s*ensures\b', t, re.M))
        results.append(f'{key}: {len(funcs)} functions, {len(asserts)} assertions, '
                       f'{bodies} inlined bodies with a statement contract, '
                       f'{left} comparator name(s) left')
        if key == 'verify':
            for fn in ('order_u8_desc_antisymmetric', 'order_f32_desc_transitive'):
                m = re.search(r'^void ' + fn + r'\(.*?^}\n', t, re.M | re.S)
                print(f'\n--- files: {fn} as the inlining leaves it ---')
                shown(m.group(0) if m else '(not found)')

    def wrappers(t):
        return {m.group(1): m.group(0) for m in
                re.finditer(r'^void (order_\w+)\(.*?^}\n', t, re.M | re.S)} if t else None
    kept, every = wrappers(texts.get('verify-kept')), wrappers(texts.get('verify-all'))
    if kept is None or every is None:
        same = '@all: not compared'
    else:
        diff = sorted(k for k in set(kept) | set(every) if kept.get(k) != every.get(k))
        same = (f'@all leaves the {len(kept)} functions as the 24 names do' if not diff else
                f'@all leaves {len(diff)} of the functions otherwise: {" ".join(diff[:20])}')
    msg = f'files: {gen}. ' + '; '.join(results) + f'. {same}'
    notice('order files', msg)
    return msg


def run_part(part):
    agreement = agreement_tool()
    title = f'order {part}'
    if part == 'together':
        files_check(agreement)
    if part in MODELS:
        cls, args = model_args(part)
        what = f'the driver, the three provers together, at {part} ({cls}: {" ".join(args)})'
        cmd = command(agreement, DRIVER, ','.join(PROVERS), os.path.join(OUT, part, 'wp.json'),
                      extra_model=args)
    elif part == 'contract':
        what = 'the driver without inlining, its four kinds of assertion only'
        cmd = command(agreement, DRIVER, ','.join(PROVERS), os.path.join(OUT, part, 'wp.json'),
                      inline=False, props=PROPS)
    elif part == 'false':
        what = 'compare_order_false.h with the inlining, symmetric and self_equal only'
        cmd = command(agreement, FALSE, ','.join(PROVERS), os.path.join(OUT, part, 'wp.json'),
                      props=FALSE_PROPS)
    elif part == 'together':
        what = 'the driver, Alt-Ergo, Z3 and CVC5 together'
        cmd = command(agreement, DRIVER, ','.join(PROVERS), os.path.join(OUT, part, 'wp.json'))
    else:
        what = f'the driver, {agreement.NAMES[part]} alone'
        cmd = command(agreement, DRIVER, part, os.path.join(OUT, part, 'wp.json'))
    r = wp(agreement, part, cmd)
    if isinstance(r, str):
        sys.exit(f'::error title={title}::{part}, {what}: {r}')
    s, goals, minutes, verdicts = r

    # Each goal, in the log.
    rows = []
    for g in sorted(goals):
        b = best(goals, g)
        res = f'{b[0]} {b[1]:.2f} s' if b and b[0] != 'WP' else ('WP' if b else 'NOT PROVED')
        rows.append(f'{short(g)} | {kind(g)} | {res}')
    print(f'\n--- {part}: each goal ({len(goals)}) | kind | proved by ---')
    print('\n'.join(rows))
    print('--- end ---')

    unproved = sorted(short(g) for g, (ok, _) in goals.items() if not ok)
    proofs = [(b[1], b[0], g) for g in goals for b in [best(goals, g)] if b and b[0] != 'WP']
    slowest = max(proofs) if proofs else None
    over = sum(1 for t, _, _ in proofs if t > SLOW)
    props = PROPS if part != 'false' else FALSE_PROPS
    prop = by_property(goals, props)
    kinds = by_kind(goals)
    summary = {
        'part': part, 'what': what, 'cmd': cmd,
        'summary': [s[0], s[1], s[2]], 'minutes': round(minutes, 1), 'verdicts': verdicts,
        'unproved': unproved, 'over': over,
        'slowest': [slowest[0], slowest[1], short(slowest[2])] if slowest else None,
        'properties': prop, 'kinds': kinds,
        'goals': {g: [ok, succ] for g, (ok, succ) in goals.items()},
    }
    with open(os.path.join(OUT, part, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=1, sort_keys=True)

    cats = '; '.join(f'{k} {v}' for k, v in s[2].items())
    bad = ', '.join(f'{k} {v}' for k, v in verdicts.items() if v) or 'none'
    low = (f'slowest proof {secs(slowest[0])} ({slowest[1]}, {short(slowest[2])})'
           if slowest else 'no proof by a prover')
    notice(title, f'{part}, {what}: Proved goals {s[0]} / {s[1]} ({cats}); the report: '
                  f'{len(goals)} goals, not proved {len(unproved)}; verdicts other than Valid: '
                  f'{bad}; {low}; proofs over {SLOW:.0f} s {over}; {minutes:.0f} min')
    notice(f'{title} properties',
           f'{part}, the assertions\' goals, proved / goals, by property and by the '
           f'comparators\' type: ' + '; '.join(f'{k} {v[0]} / {v[1]}' for k, v in sorted(prop.items()))
           + '. Every goal by kind: ' + '; '.join(f'{k} {v[0]} / {v[1]}' for k, v in sorted(kinds.items())))
    used = 3 if part == 'together' else 2   # the together part posted the files' notice too
    if part == 'false':
        wr = {}
        for g, (ok, _) in goals.items():
            w = wrapper(g)
            c = wr.setdefault(w, [0, 0])
            c[0] += ok
            c[1] += 1
        whole = sorted(w for w, c in wr.items() if c[0] == c[1])
        notice(f'{title} wrappers',
               f'false: {len(wr)} functions; with every part of their assertion proved '
               f'{len(whole)}: {" ".join(whole) or "none"}. Each, proved / parts: '
               + '; '.join(f'{w} {c[0]} / {c[1]}' for w, c in sorted(wr.items())))
        used += 1
    if unproved:
        for text in grouped(f'{part} not proved ({len(unproved)})', unproved, 3, sep=' '):
            notice(f'{title} unproved', text)
            used += 1
    timed = [f'{short(g)} {n} {secs(t)}' for t, n, g in sorted(proofs, reverse=True) if t > SHOWN]
    for text in grouped(f'{part} proofs over {SHOWN:.0f} s ({len(timed)})', timed, STEP - used):
        notice(f'{title} times', text)
    return 0


def report(where):
    agreement = agreement_tool()
    got, missing = {}, []
    for p in PARTS:
        path = os.path.join(where, p, 'summary.json')
        if os.path.exists(path):
            with open(path) as f:
                got[p] = json.load(f)
        else:
            missing.append(p)
    if not got:
        if not enabled():
            notice('order report', f'no part ran: {os.path.relpath(STEPS, ROOT)} names none')
            return 0
        sys.exit(f'::error title=order report::no part wrote a summary under {where}')
    notes = []
    t = got.get('together')

    # Each prover alone: the goals one or two prove, as a pin file of 13 lists them.
    alone = [p for p in PROVERS if p in got]
    if alone:
        have = {}
        for p in alone:
            goals = {g: (ok, succ) for g, (ok, succ) in got[p]['goals'].items()}
            have[p] = (goals, tuple(got[p]['summary']), None)
        o = agreement.sort_goals(have)
        lines = sorted(f'{k} {pv} {g}' for g, (k, pv) in o['cls'].items())
        slow = sorted(f'slow {"+".join(sorted(ts))} {g}' for g, ts in o['slow'].items())
        print(f'\n--- the pin file\'s lines, as 13 · Prover agreement would write them '
              f'({len(lines)} and slow {len(slow)}) ---')
        print('\n'.join(lines + slow) or '(none)')
        print('--- end ---')
        print(f'--- goals no prover proves alone ({len(o["none"])}) ---')
        print('\n'.join(o['none']) or '(none)')
        print('--- end ---')
        two = ', '.join(f'{k} {len(v)}' for k, v in sorted(o['two'].items())) or 'none'
        one = ', '.join(f'{k} {len(v)}' for k, v in o['one'].items() if v) or 'none'
        each = ', '.join(f'{agreement.NAMES[p]} {got[p]["summary"][0]} / {got[p]["summary"][1]} '
                         f'({got[p]["minutes"]:.0f} min)' for p in alone)
        none_t = ''
        if t is not None:
            tu = {g for g, (ok, _) in t['goals'].items() if not ok}
            none_t = (f'; the same as those not proved together: '
                      f'{"yes" if tu == set(o["none"]) else "NO"}')
        notes.append(('order report alone',
                      f'each prover alone{"" if len(alone) == 3 else " (NOT ALL THREE)"}: {each}. '
                      f'Of the reports\' {len(o["ids"])} goals: by WP itself {len(o["wp"])}, three '
                      f'provers {len(o["three"])}, two {o["n_two"]} ({two}), one {o["n_one"]} '
                      f'({one}), none {len(o["none"])}{none_t}; over {SLOW:.0f} s with a prover '
                      f'alone {len(o["slow"])}. A pin file would hold {len(lines)} lines and '
                      f'{len(slow)} slow ones, in the log'))
        byp = {}
        for g, (k, pv) in o['cls'].items():
            key = f'{k} {pv}, {kind(g)} {family(g)}'
            byp[key] = byp.get(key, 0) + 1
        if byp:
            notes.append(('order report alone kinds',
                          'the goals one or two provers prove alone, by provers, kind and the '
                          'comparators\' type: ' + '; '.join(f'{k} {v}' for k, v in sorted(byp.items()))))

    # Each model against x86_64.
    if t is not None:
        tg = t['goals']
        for m in MODELS:
            if m not in got:
                continue
            mg = got[m]['goals']
            only_t, only_m = sorted(set(tg) - set(mg)), sorted(set(mg) - set(tg))
            un_t = {g for g, (ok, _) in tg.items() if not ok}
            un_m = {g for g, (ok, _) in mg.items() if not ok}
            notes.append((f'order report {m}',
                          f'{m}: Proved goals {got[m]["summary"][0]} / {got[m]["summary"][1]}, '
                          f'x86_64 {t["summary"][0]} / {t["summary"][1]}; goals at {m} only '
                          f'{len(only_m)}, at x86_64 only {len(only_t)}; not proved at {m} '
                          f'{len(un_m)}, at x86_64 {len(un_t)}, the same: '
                          f'{"yes" if un_m == un_t else "NO"}'
                          + (f'. At {m} only: {" ".join(short(g) for g in only_m[:30])}' if only_m else '')
                          + (f'. At x86_64 only: {" ".join(short(g) for g in only_t[:30])}' if only_t else '')))

    # The controls.
    if 'contract' in got:
        c = got['contract']
        notes.append(('order report contract',
                      f'contract, without inlining: Proved goals {c["summary"][0]} / '
                      f'{c["summary"][1]}; the assertions\' goals: '
                      + '; '.join(f'{k} {v[0]} / {v[1]}' for k, v in sorted(c['properties'].items()))))
    if 'false' in got:
        f_ = got['false']
        wr = {}
        for g, (ok, _) in f_['goals'].items():
            w = wrapper(g)
            cnt = wr.setdefault(w, [0, 0])
            cnt[0] += ok
            cnt[1] += 1
        whole = sorted(w for w, cnt in wr.items() if cnt[0] == cnt[1])
        notes.append(('order report false',
                      f'false: Proved goals {f_["summary"][0]} / {f_["summary"][1]}; of its '
                      f'{len(wr)} functions, with every part of their assertion proved: '
                      f'{len(whole)} {" ".join(whole)}'))
    if t is not None:
        notes.insert(0, ('order report',
                         f'together: Proved goals {t["summary"][0]} / {t["summary"][1]}, not proved '
                         f'{len(t["unproved"])}; the assertions\' goals: '
                         + '; '.join(f'{k} {v[0]} / {v[1]}' for k, v in sorted(t['properties'].items()))
                         + f'. Parts that wrote no summary: {" ".join(missing) or "none"}'))
    elif missing:
        notes.insert(0, ('order report', f'parts that wrote no summary: {" ".join(missing)}'))
    for title, msg in notes[:STEP]:
        notice(title, msg)
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'named' and argv[2] in PARTS + ['any']:
        named = enabled() if argv[2] == 'any' else [p for p in enabled() if p == argv[2]]
        print(f'run={"true" if named else "false"}')
        return 0
    if len(argv) in (2, 3) and argv[1] == 'report':
        return report(argv[2] if len(argv) == 3 else OUT)
    if len(argv) != 2 or argv[1] not in PARTS:
        print(__doc__)
        return 2
    part = argv[1]
    if part not in enabled():
        notice(f'order {part}', f'{part}: not run; {os.path.relpath(STEPS, ROOT)} names '
                                f'{" ".join(enabled()) or "no part"}')
        return 0
    os.makedirs(OUT, exist_ok=True)
    return run_part(part)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
