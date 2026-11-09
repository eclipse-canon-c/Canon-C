#!/usr/bin/env python3
"""
agreement.py -- how many of the three provers prove each goal, each alone
(VERIFY-064, made a gate by VERIFY-065).

Every proof job gives Alt-Ergo, Z3 and CVC5 together each goal that WP's
simplifier, Qed, does not close, and counts the goal proved when the first
of them succeeds: WP then stops the other two (ProverTask, WP 29). Even with
-wp-run-all-provers, WP's per-goal report keeps only the best prover's
result (Stats.results), so a job cannot say whether the other two could
have proved a goal. This script runs a unit three times, once with each
prover alone and otherwise as its job runs it, and reads each goal's result
from WP's JSON report (-wp-report-json). It sorts the unit's goals:

  - WP itself: proved with no prover, by Qed or by WP's own passes;
  - three, two or one: proved by that many of the three provers, each alone;
  - none: proved by no prover alone; these must be the job's pinned names.

WP's summary also counts "Terminating" and "Unreachable" goals, which are
not in its report; they are read from the summary and added back.

A proof that takes a prover more than SLOW seconds could fall either side of
the 120 s limit on another run: VERIFY-065 saw proofs that took over 60 s on
one run time out on the next. Such a goal is listed as slow, with its time,
and left out of the comparison with the pins, as is every goal the pins list
as slow.

    agreement.py run UNIT PROVER
        WP on UNIT with PROVER alone (alt-ergo, z3 or cvc5) at 120 s, into
        agree_UNIT_PROVER.txt and .json, and a notice of its summary. If
        AGREE_SECONDS is set, WP is stopped after that many seconds. Exits 1
        if WP is stopped, or writes no summary or no report.

    agreement.py unit UNIT [DIR]
        Sorts UNIT's goals from its three reports under DIR (default .),
        checks that the goals no prover proves alone are the names its job
        pins, and compares the goals one or two provers prove with
        tools/agreement/pins/UNIT.txt. Writes its notices to notices_1.txt,
        notices_2.txt, ... (ten to a file: GitHub keeps ten notices of a
        step). Exits 1 on a difference when ENFORCE is set, and without data.

    agreement.py total [DIR]
        The totals over every report under DIR, and agreement.tsv.

    agreement.py deps [UNIT]
        The repository files each unit includes, as the compiler lists them
        with the unit's own flags, with and without Frama-C's macros.

    agreement.py affected --all | FILE...
        The JSON list of the units a change to FILEs can affect, longest
        first: those that include a changed file; every unit if the change
        is to this script, the workflow, the Frama-C setup, or a header
        under the watched folders that no unit includes; a unit whose pins
        changed. Used by the workflow's first job.

    agreement.py check
        For 8 · Records: each unit's file, memory model, preprocessing and
        extra options here are those of its proof job's command; every proof
        job but those at other machine models has a unit here; every file a
        unit includes is under the workflow's push paths; the run order lists
        every unit once; and, once pins exist, every unit has them.

The units are the pointer probe's 26 (tools/probes/verify-057), with
frama-c-diag's -variadic-no-translation. frama-c-arena-32, frama-c-win64,
the -CC families and the machine-model jobs are left out.
"""
import glob
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import time

ENFORCE = True    # since VERIFY-065's third run: a difference fails the unit's job

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PINS = os.path.join(HERE, 'pins')
WORKFLOW = '.github/workflows/prover-agreement.yml'
PROOF_WORKFLOWS = ['.github/workflows/proofs-core.yml', '.github/workflows/proofs-semantics.yml',
                   '.github/workflows/proofs-data.yml']
WATCHED = ('core/', 'semantics/', 'data/', 'vmacros/vdrivers/')
FORCING = ('tools/agreement/agreement.py', WORKFLOW, '.github/actions/framac-setup/')

CORE = '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'
SEM = '-I core/primitives -I core -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'
DATA = '-I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'
UNITS = {
    'frama-c-checked': ('core/primitives/checked.h', 'Typed', CORE),
    'frama-c-bits': ('core/primitives/bits.h', 'Typed', CORE),
    'frama-c-compare': ('core/primitives/compare.h', 'Typed+Cast', CORE),
    'frama-c-ptr': ('core/primitives/ptr.h', 'Typed+Cast', CORE),
    'frama-c-slice': ('core/slice.h', 'Typed+Cast', CORE),
    'frama-c-memory': ('core/memory.h', 'Typed+Cast', CORE),
    'frama-c-arena': ('core/arena.h', 'Typed+Cast', CORE),
    'frama-c-pool': ('core/pool.h', 'Typed+Cast', CORE),
    'frama-c-region': ('core/region.h', 'Typed+Cast', CORE),
    'frama-c-lifetime': ('core/primitives/lifetime.h', 'Typed', '-I core/primitives -I core -I . -DCANON_LIFETIME_DEBUG -DCANON_LIFETIME_NO_ATOMICS -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-error': ('semantics/error.h', 'Typed', SEM),
    'frama-c-option': ('vmacros/vdrivers/option_verify.h', 'Typed', SEM),
    'frama-c-result': ('vmacros/vdrivers/result_verify.h', 'Typed', SEM),
    'frama-c-borrow': ('semantics/borrow.h', 'Typed+Cast', CORE),
    'frama-c-diag': ('semantics/diag.h', 'Typed+Cast', SEM),
    'frama-c-vec': ('vmacros/vdrivers/vec_verify.h', 'Typed+Cast', DATA),
    'frama-c-deque': ('vmacros/vdrivers/deque_verify.h', 'Typed', DATA),
    'frama-c-bitset': ('vmacros/vdrivers/bitset_verify.h', 'Typed+Cast', DATA),
    'frama-c-priority-queue': ('data/priority_queue.h', 'Typed+Cast', '-I data -I core -I core/primitives -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-range': ('vmacros/vdrivers/range_verify.h', 'Typed', DATA),
    'frama-c-vec-range': ('vmacros/vdrivers/vec_range_verify.h', 'Typed+Cast', '-CC ' + DATA),
    'frama-c-vec-fmt': ('vmacros/vdrivers/vec_fmt_verify.h', 'Typed+Cast', '-CC ' + DATA),
    'frama-c-stringbuf': ('data/stringbuf.h', 'Typed+Cast', SEM),
    'frama-c-stack': ('vmacros/vdrivers/stack_verify.h', 'Typed+Cast', '-CC ' + DATA),
    'frama-c-array': ('vmacros/vdrivers/array_verify.h', 'Typed+Cast', '-CC ' + DATA),
    'frama-c-hashmap': ('vmacros/vdrivers/hashmap_verify.h', 'Typed+Cast', '-CC ' + DATA),
}
EXTRA = {'frama-c-diag': ['-variadic-no-translation']}
# Proof jobs that prove units of these 26 at another machine model: not run here.
OTHER_MODELS = {'frama-c-arena-32', 'frama-c-win64'}
# The run order: longest first, by the units' runs of VERIFY-064.
ORDER = ['frama-c-vec', 'frama-c-bitset', 'frama-c-stack', 'frama-c-vec-fmt', 'frama-c-hashmap',
         'frama-c-vec-range', 'frama-c-region', 'frama-c-stringbuf', 'frama-c-pool',
         'frama-c-priority-queue', 'frama-c-arena', 'frama-c-memory', 'frama-c-array',
         'frama-c-borrow', 'frama-c-diag', 'frama-c-range', 'frama-c-bits', 'frama-c-deque',
         'frama-c-slice', 'frama-c-ptr', 'frama-c-option', 'frama-c-result', 'frama-c-checked',
         'frama-c-compare', 'frama-c-lifetime', 'frama-c-error']
# frama-c-stack's job pins its unproved names as -CC's vec family's (cc_pins/vec.txt).
RESIDUALS_OF = {'frama-c-stack': 'cc-vec'}
PROVERS = ('alt-ergo', 'z3', 'cvc5')
NAMES = {'alt-ergo': 'Alt-Ergo', 'z3': 'Z3', 'cvc5': 'CVC5'}
SLOW = 30.0       # seconds: a quarter of the 120 s limit (VERIFY-065, after run 1 and #3)
CUT = 4000        # the runner cuts an annotation at 4096 characters
PER_FILE = 10     # GitHub keeps ten notices of a step
MAX_FILES = 4     # and fifty annotations of a job


def stem(unit, prover):
    return f'agree_{unit}_{prover}'


def summary(text):
    """(proved, total, {category: count}) from WP's last summary, or None."""
    lines = text.splitlines()
    at = [i for i, line in enumerate(lines) if line.startswith('[wp] Proved goals:')]
    if not at:
        return None
    m = re.search(r'(\d+)\s*/\s*(\d+)', lines[at[-1]])
    if not m:
        return None
    cats = {}
    for line in lines[at[-1] + 1:]:
        c = re.match(r'^\s+(\S.*?):\s+(\d+)\b', line)
        if not c:
            break
        cats[c.group(1)] = int(c.group(2))
    return int(m.group(1)), int(m.group(2)), cats


def canonical(name):
    n = str(name).lower()
    for key in PROVERS:
        if n.startswith(key):
            return NAMES[key]
    return 'Qed' if n.startswith('qed') else str(name)


def load(path):
    """goal -> (passed, {prover with a success: its time}), and WP's first two records.

    WP writes a float with no digit after its point ("0."), which JSON does
    not allow; the point gets a 0 before the text is read."""
    with open(path) as f:
        text = re.sub(r'(\d)\.(?=\s*[,}\]])', r'\1.0', f.read())
    data = json.loads(text)
    if not isinstance(data, list):
        raise ValueError('the report is not a list')
    goals = {}
    for rec in data:
        if not isinstance(rec, dict) or 'goal' not in rec:
            continue
        succ = {}
        for p in rec.get('provers') or []:
            if isinstance(p, dict) and (p.get('success') or 0) > 0:
                succ[canonical(p.get('prover', '?'))] = float(p.get('time') or 0.0)
        passed = rec.get('passed') is True or str(rec.get('verdict', '')).lower() == 'valid'
        goals[str(rec['goal'])] = (passed, succ)
    return goals, data[:2]


def run(unit, prover):
    path, model, cpp = UNITS[unit]
    out, rep = stem(unit, prover) + '.txt', stem(unit, prover) + '.json'
    for f in (out, rep):
        if os.path.exists(f):
            os.remove(f)
    cmd = (['frama-c', '-wp', '-wp-rte',
            '-wp-model', model,
            '-wp-prover', prover,
            '-wp-timeout', '120',
            '-wp-split',
            '-wp-report-json', rep]
           + EXTRA.get(unit, [])
           + [f'-cpp-extra-args={cpp}', path])
    limit = float(os.environ.get('AGREE_SECONDS') or 0)
    print('Running:', ' '.join(cmd), flush=True)
    if limit > 0:
        print(f'WP is stopped after {limit / 60:.0f} minutes', flush=True)
    title = f'agree {unit} {prover}'
    start = time.time()
    with open(out, 'w') as f:
        # A session of its own, so that a stop takes the provers with it.
        p = subprocess.Popen(cmd, stdout=f, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = p.wait(timeout=limit if limit > 0 else None)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()
            code = None
    minutes = (time.time() - start) / 60
    with open(out, errors='replace') as f:
        text = f.read()
    if code is None:
        print(f'::error title={title}::stopped after {minutes:.0f} minutes, at the run\'s limit: no summary, no report')
        return 1
    s = summary(text)
    print(f'frama-c exit {code} after {minutes:.0f} minutes')
    if s is None or not os.path.exists(rep):
        tail = ' / '.join(text.strip().splitlines()[-5:])
        print(f'::error title={title}::frama-c exit {code}: no summary or no report; last lines: {tail[:CUT - 100]}')
        return 1
    try:
        goals, _ = load(rep)
    except (OSError, ValueError) as e:
        print(f'::error title={title}::the report cannot be read: {e}')
        return 1
    passed = sum(1 for ok, _ in goals.values() if ok)
    cats = '; '.join(f'{k} {v}' for k, v in s[2].items())
    msg = (f'{unit}, {NAMES[prover]} alone: proved {s[0]} / {s[1]} ({cats}); '
           f'WP\'s report: {len(goals)} goals, {passed} proved; {minutes:.0f} minutes')
    print(msg)
    print(f'::notice title={title}::{msg[:CUT]}')
    return 0


def collect(where, only=None):
    """unit -> prover -> (goals, summary, first records), from the files under where."""
    runs = {}
    for path in sorted(glob.glob(os.path.join(where, '**', 'agree_*.json'), recursive=True)):
        unit, prover = os.path.basename(path)[len('agree_'):-len('.json')].rsplit('_', 1)
        if unit not in UNITS or prover not in PROVERS or (only and unit != only):
            continue
        try:
            goals, first = load(path)
        except (OSError, ValueError) as e:
            print(f'::warning title=agreement {unit} {prover}::the report cannot be read: {e}')
            continue
        s = None
        txt = path[:-len('.json')] + '.txt'
        if os.path.exists(txt):
            with open(txt, errors='replace') as f:
                s = summary(f.read())
        runs.setdefault(unit, {})[prover] = (goals, s, first)
    return runs


def sort_goals(have):
    """A unit's goals, by the provers that prove each alone, with its counts."""
    ids = sorted(set().union(*[set(g) for g, _, _ in have.values()]))
    smt_names = set(NAMES.values())
    o = {'ids': ids, 'wp': [], 'qed': 0, 'three': [], 'two': {},
         'one': {NAMES[p]: [] for p in PROVERS}, 'none': [], 'absent': 0, 'wp_differ': 0,
         'slow': {}, 'cls': {}}
    for g in ids:
        smt, present, by_wp, other = {}, 0, 0, set()
        for p, (goals, _, _) in have.items():
            if g not in goals:
                o['absent'] += 1
                continue
            present += 1
            ok, succ = goals[g]
            if ok and NAMES[p] in succ:
                smt[NAMES[p]] = succ[NAMES[p]]
            elif ok:
                by_wp += 1
            other |= set(succ) - smt_names
        slow = {n: t for n, t in smt.items() if t > SLOW}
        if slow:
            o['slow'][g] = slow
        names = '+'.join(sorted(smt))
        if len(smt) == 3:
            o['three'].append(g)
        elif len(smt) == 2:
            o['two'].setdefault(names, []).append(g)
            o['cls'][g] = ('two', names)
        elif len(smt) == 1:
            o['one'][names].append(g)
            o['cls'][g] = ('one', names)
        elif by_wp:
            o['wp'].append(g)
            o['qed'] += 'Qed' in other
            o['wp_differ'] += by_wp != present
        else:
            o['none'].append(g)
    sums = {p: s for p, (_, s, _) in have.items() if s}
    term = {s[2].get('Terminating', 0) for s in sums.values()}
    unre = {s[2].get('Unreachable', 0) for s in sums.values()}
    o['totals'] = sorted({s[1] for s in sums.values()})
    o['sums'] = sums
    o['t'] = max(term) if term else 0
    o['u'] = max(unre) if unre else 0
    o['summaries_differ'] = len(term) > 1 or len(unre) > 1 or len(o['totals']) > 1
    o['n_two'] = sum(len(v) for v in o['two'].values())
    o['n_one'] = sum(len(v) for v in o['one'].values())
    o['all'] = len(ids) + o['t'] + o['u']
    o['proved'] = (o['t'] + o['u'] + len(o['wp']) + len(o['three'])
                   + o['n_two'] + o['n_one'])
    return o


def unit_line(unit, have):
    """The unit's counts in one line, and its sorted goals."""
    o = sort_goals(have)
    sums = o['sums']
    msg = (f'{unit}: {len(o["ids"])} goals in WP\'s report, with Terminating {o["t"]} and Unreachable '
           f'{o["u"]}: {o["all"]} (the summaries\' total: '
           f'{"/".join(str(x) for x in o["totals"]) or "none"}). '
           f'Proved by WP itself {len(o["wp"])} (Qed {o["qed"]}); by all three provers alone '
           f'{len(o["three"])}; by two {o["n_two"]} ('
           + ', '.join(f'{k} {len(v)}' for k, v in sorted(o['two'].items())) + '); '
           f'by one {o["n_one"]} (' + ', '.join(f'{k} {len(v)}' for k, v in o['one'].items()) + '); '
           f'by none {len(o["none"])}. Proved, with Terminating and Unreachable: '
           f'{o["proved"]} / {o["all"]}. Each prover alone: ' + ', '.join(
               f'{NAMES[p]} {sums[p][0]} / {sums[p][1]}' if p in sums else f'{NAMES[p]} no summary'
               for p in PROVERS if p in have))
    missing = [NAMES[p] for p in PROVERS if p not in have]
    if missing:
        msg += f'. NO REPORT from {", ".join(missing)}: the counts are of the other runs'
    if o['summaries_differ']:
        msg += '. The summaries differ between runs'
    if o['absent']:
        msg += f'. The reports list different goals ({o["absent"]} absences)'
    if o['wp_differ']:
        msg += f'. WP itself proves {o["wp_differ"]} goals in some runs only'
    return msg, o


def chunks(head, names):
    """Notices of at most CUT characters listing names after head."""
    out, cur, part = [], [], 1
    for name in names:
        if cur and len(head) + 12 + sum(len(x) + 1 for x in cur) + len(name) > CUT:
            out.append((part, cur))
            cur, part = [], part + 1
        cur.append(name)
    if cur:
        out.append((part, cur))
    many = len(out) > 1
    return [f'{head}{f", part {k}" if many else ""}: ' + ' '.join(c) for k, c in out]


def norm(name):
    return re.sub(r'^typed_cast_', 'typed_', name)


def pinned_residuals(unit):
    """The names the unit's proof job pins as unproved, as 8 · Records reads them."""
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    import record_coverage   # noqa: E402, the records' own reader of the pins
    here = os.getcwd()
    try:
        os.chdir(ROOT)
        pins = record_coverage.pinned_names()
    finally:
        os.chdir(here)
    key = RESIDUALS_OF.get(unit, unit)
    return {n for n, units in pins.items() if key in units}


def read_pins(unit):
    """{name: (kind, provers)} and the names listed slow, or None without a pin file."""
    path = os.path.join(PINS, unit + '.txt')
    if not os.path.exists(path):
        return None
    cls, slow = {}, set()
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) != 3 or parts[0] not in ('one', 'two', 'slow'):
                raise ValueError(f'{path}:{n}: not "one|two|slow PROVERS NAME": {line}')
            if parts[0] == 'slow':
                slow.add(parts[2])
            else:
                cls[parts[2]] = (parts[0], parts[1])
    return cls, slow


def state(o, g):
    """How the run proves goal g, in the words of the pin file."""
    if g in o['cls']:
        return ' '.join(o['cls'][g])
    if g in o['three']:
        return 'three provers'
    if g in o['wp']:
        return 'WP itself'
    if g in o['none']:
        return 'no prover'
    return 'absent'


def compare(o, pins):
    """The differences between a run's classes and its pins, slow goals left out."""
    cls, slow = pins
    skip = set(o['slow']) | slow
    now = {g: c for g, c in o['cls'].items() if g not in skip}
    was = {g: c for g, c in cls.items() if g not in skip}
    diffs = []
    for g in sorted(set(now) | set(was)):
        if was.get(g) == now.get(g):
            continue
        old = ' '.join(was[g]) if g in was else 'not listed'
        diffs.append(f'{g}: pinned {old}, now {state(o, g)}')
    return diffs


def unit_report(unit, where):
    runs = collect(where, unit)
    for f in glob.glob('notices_*.txt'):
        os.remove(f)
    if unit not in runs:
        print(f'::error title=agreement {unit}::NO DATA: no report of {unit} under {where}')
        return 1
    have = runs[unit]
    p0 = sorted(have)[0]
    print(f'The first two records of {unit} at {p0}, as read:')
    print(json.dumps(have[p0][2], indent=1)[:3000])
    msg, o = unit_line(unit, have)
    fail = []
    if len(have) < 3:
        fail.append('not all three runs reported')
    # The goals no prover proves alone must be the names the unit's job pins.
    want = pinned_residuals(unit)
    got = {norm(g) for g in o['none']}
    if got == want:
        msg += f'. Unproved: the {len(want)} names its job pins'
    else:
        msg += (f'. Unproved: NOT the names its job pins: {len(got - want)} not pinned, '
                f'{len(want - got)} pinned but proved here')
        fail.append('the unproved goals are not the pinned names')
    if o['slow']:
        msg += f'. Slow (over {SLOW:.0f} s), left out of the comparison: {len(o["slow"])}'
    pins = None
    try:
        pins = read_pins(unit)
    except ValueError as e:
        fail.append(str(e))
    diffs = []
    if pins is None:
        msg += '. No pins yet: the names below are this run\'s'
        if ENFORCE:
            fail.append('no pin file')
    else:
        diffs = compare(o, pins)
        msg += f'. Against its pins: {len(diffs)} difference(s)'
        if diffs:
            fail.append(f'{len(diffs)} difference(s) from the pins')
    if not ENFORCE:
        msg += '. Report-only (VERIFY-065)'
    print(msg)
    notices = [f'::notice title=agreement {unit}::{msg[:CUT]}']
    lists = []
    for k, v in o['one'].items():
        lists.append((f'{unit} one {k} ({len(v)})', v))
    for k, v in sorted(o['two'].items()):
        lists.append((f'{unit} two {k} ({len(v)})', v))
    for g, ts in sorted(o['slow'].items()):
        times = ', '.join(f'{n} {t:.0f} s' for n, t in sorted(ts.items()))
        lists.append((f'{unit} slow {"+".join(sorted(ts))} (1, {times})', [g]))
    for head, names in lists:
        if names:
            for text in chunks(head, names):
                notices.append(f'::notice title=agreement {unit} names::{text[:CUT]}')
    cur = []
    for d in diffs + [None]:
        if d is None or (cur and len('; '.join(cur + [d])) > CUT - 60):
            if cur:
                notices.append(f'::notice title=agreement {unit} differences::{unit}, differences from '
                               f'its pins: ' + '; '.join(cur))
            cur = []
        if d is not None:
            cur.append(d)
    room = PER_FILE * MAX_FILES
    if len(notices) > room:
        left = len(notices) - room + 1
        notices = notices[:room - 1] + [f'::notice title=agreement {unit} names::{left} more notices of '
                                        'names are not posted; the names are in the log and in agreement.tsv']
    for i in range(0, len(notices), PER_FILE):
        with open(f'notices_{i // PER_FILE + 1}.txt', 'w') as f:
            f.write('\n'.join(notices[i:i + PER_FILE]) + '\n')
    for head, names in lists + [(f'{unit} none ({len(o["none"])})', o['none'])]:
        print(f'{head}:')
        for name in names:
            print(f'  {name}')
    for d in diffs:
        print(f'DIFFERENCE {d}')
    if fail and ENFORCE:
        print(f'::error title=agreement {unit}::' + '; '.join(fail))
        return 1
    return 0


def total(where):
    runs = collect(where)
    if not runs:
        print(f'::error title=agreement::NO DATA: no agree_*.json under {where}')
        return 1
    keys = ('all', 'tu', 'wp', 'three', 'two', 'one', 'none', 'proved')
    sums = dict.fromkeys(keys, 0)
    rows, classes, incomplete = [], {}, []
    for unit in UNITS:
        if unit not in runs:
            continue
        have = runs[unit]
        if len(have) < 3:
            incomplete.append(f'{unit} ({len(have)} of 3)')
        msg, o = unit_line(unit, have)
        print(msg)
        for k, v in (('all', o['all']), ('tu', o['t'] + o['u']), ('wp', len(o['wp'])),
                     ('three', len(o['three'])), ('two', o['n_two']), ('one', o['n_one']),
                     ('none', len(o['none'])), ('proved', o['proved'])):
            sums[k] += v
        rows += [(unit, g, 3, 'Alt-Ergo,CVC5,Z3') for g in o['three']]
        for k, v in sorted(o['two'].items()):
            rows += [(unit, g, 2, k.replace('+', ',')) for g in v]
        for k, v in o['one'].items():
            rows += [(unit, g, 1, k) for g in v]
        rows += [(unit, g, 0, '') for g in o['none']]
    for unit, g, n, names in rows:
        classes.setdefault(g, set()).add((n, names))
    one = {g for g, c in classes.items() if any(n == 1 for n, _ in c)}
    none = {g for g, c in classes.items() if any(n == 0 for n, _ in c)}
    split = sorted(g for g, c in classes.items() if len(c) > 1)
    by = {}
    for g in one:
        for n, names in classes[g]:
            if n == 1:
                by[names] = by.get(names, 0) + 1
    with open('agreement.tsv', 'w') as f:
        f.write('unit\tgoal\tprovers_alone\tnames\n')
        for row in rows:
            f.write('\t'.join(str(x) for x in row) + '\n')
    ran = [u for u in UNITS if u in runs]
    msg = (f'{len(ran)} of {len(UNITS)} units: {sums["all"]} goals, Terminating and Unreachable '
           f'{sums["tu"]} of them; proved by WP itself {sums["wp"]}; by all three provers alone '
           f'{sums["three"]}; by two {sums["two"]}; by one {sums["one"]}; by none {sums["none"]}; '
           f'proved, with Terminating and Unreachable, {sums["proved"]} / {sums["all"]}. Distinct '
           f'names, across units, that one prover alone proves: {len(one)} ('
           + ', '.join(f'{k} {v}' for k, v in sorted(by.items())) + f'); that none proves alone: '
           f'{len(none)}; sorted differently in different units: {len(split)}')
    if incomplete:
        msg += '. INCOMPLETE: ' + ', '.join(incomplete)
    print(msg)
    print(f'::notice title=agreement total::{msg[:CUT]}')
    for text in chunks('names sorted differently in different units', split)[:8]:
        print(f'::notice title=agreement total names::{text[:CUT]}')
    for g in split:
        print(f'  {g}: ' + '; '.join(f'{n} {names}' for n, names in sorted(classes[g])))
    return 0


def unit_deps(unit):
    """The repository files the unit includes, with and without Frama-C's macros."""
    path, _, cpp = UNITS[unit]
    args = [a for a in shlex.split(cpp) if a not in ('-C', '-CC')]
    found = set()
    for extra in ([], ['-D__FRAMAC__', '-D__FC_MACHDEP_X86_64']):
        cmd = ['gcc', '-x', 'c', '-std=c11', '-M', '-MG'] + extra + args + [path]
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
        if r.returncode:
            raise RuntimeError(f'{unit}: {" ".join(cmd)}: {r.stderr.strip()[:500]}')
        for f in r.stdout.replace('\\\n', ' ').split(':', 1)[1].split():
            a = os.path.normpath(f if os.path.isabs(f) else os.path.join(ROOT, f))
            if a.startswith(ROOT + os.sep):
                found.add(os.path.relpath(a, ROOT))
    return found


def affected(files):
    """The units a change to files can affect, in the run order."""
    deps = {u: unit_deps(u) for u in UNITS}
    hit = set()
    for f in files:
        f = f.strip()
        if not f:
            continue
        if f.startswith(FORCING):
            return list(ORDER)
        pin = re.match(r'^tools/agreement/pins/(frama-c-[a-z0-9-]+)\.txt$', f)
        if pin:
            if pin.group(1) in UNITS:
                hit.add(pin.group(1))
            continue
        users = {u for u, d in deps.items() if f in d}
        if users:
            hit |= users
        elif f.endswith('.h') and f.startswith(WATCHED):
            return list(ORDER)   # a header no unit includes today: run every unit
    return [u for u in ORDER if u in hit]


def push_paths():
    """The push paths of the workflow, read as written (one '- ...' line each)."""
    paths, inside = [], False
    with open(os.path.join(ROOT, WORKFLOW)) as f:
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


def matches(path, pattern):
    rx, i = '', 0
    while i < len(pattern):
        if pattern.startswith('**', i):
            rx, i = rx + '.*', i + 2
        elif pattern[i] == '*':
            rx, i = rx + '[^/]*', i + 1
        else:
            rx, i = rx + re.escape(pattern[i]), i + 1
    return re.fullmatch(rx, path) is not None


def proof_jobs():
    """job name -> its lines, for the frama-c-* jobs of the proof workflows."""
    out = {}
    for wf in PROOF_WORKFLOWS:
        cur = None
        with open(os.path.join(ROOT, wf), encoding='utf-8') as f:
            for line in f.read().split('\n'):
                m = re.match(r'^  ([A-Za-z0-9_-]+):\s*$', line)
                if m:
                    cur = m.group(1)
                    out.setdefault(cur, [])
                    continue
                if cur:
                    out[cur].append(line)
    return {k: v for k, v in out.items() if k.startswith('frama-c-')}


def job_command(lines):
    """(file, memory model, preprocessing, extra options) of the job's WP run with the three provers."""
    text = '\n'.join(lines)
    model = re.search(r'^\s*WP_MODEL="?([A-Za-z+]+)"?', text, re.M)
    model = model.group(1) if model else None
    for line in re.sub(r'\\\n', ' ', text).split('\n'):
        s = line.strip()
        if not (re.match(r'^frama-c\s', s) and '-wp-prover alt-ergo,z3,cvc5' in s):
            continue
        if model:
            s = s.replace('"${WP_MODEL}"', model).replace('${WP_MODEL}', model)
        s = re.split(r'\s(?:2>&1|>|\|)', s)[0]
        toks = shlex.split(s)
        got_model, cpp, extra, path = 'Typed', None, [], None   # WP's default model is Typed
        i = 1
        while i < len(toks):
            t = toks[i]
            if t == '-wp-model' and i + 1 < len(toks):
                got_model = toks[i + 1]
                i += 2
                continue
            if t.startswith('-cpp-extra-args='):
                cpp = ' '.join(t[len('-cpp-extra-args='):].split())
            elif t == '-variadic-no-translation':
                extra.append(t)
            elif not t.startswith('-') and t.endswith('.h'):
                path = t
            i += 1
        return path, got_model, cpp, extra
    return None


def check():
    problems = []
    jobs = proof_jobs()
    for unit, (path, model, cpp) in UNITS.items():
        if unit not in jobs:
            problems.append(f'{unit}: no such job in the proof workflows')
            continue
        got = job_command(jobs[unit])
        want = (path, model, ' '.join(cpp.split()), EXTRA.get(unit, []))
        if got is None:
            problems.append(f'{unit}: no WP command with the three provers in its job')
        elif got != want:
            problems.append(f'{unit}: its job runs {got}, this script {want}')
    for job in sorted(set(jobs) - set(UNITS) - OTHER_MODELS):
        problems.append(f'{job}: a proof job with no entry here; a new unit needs its entry in UNITS and ORDER')
    if sorted(ORDER) != sorted(UNITS):
        problems.append('ORDER does not list every unit exactly once')
    paths = push_paths()
    if not paths:
        problems.append(f'{WORKFLOW}: no push paths read')
    seen = set()
    for unit in UNITS:
        try:
            for f in sorted(unit_deps(unit)):
                if f not in seen and not any(matches(f, p) for p in paths):
                    problems.append(f'{unit} includes {f}, which is not under {WORKFLOW}\'s push paths')
                seen.add(f)
        except RuntimeError as e:
            problems.append(str(e))
    for f in seen:
        if not f.startswith(WATCHED):
            problems.append(f'{f} is included but not under {", ".join(WATCHED)}: affected() would miss a new header there')
    if os.path.isdir(PINS):
        for unit in UNITS:
            try:
                if read_pins(unit) is None:
                    problems.append(f'{unit}: no pin file in {os.path.relpath(PINS, ROOT)}')
            except ValueError as e:
                problems.append(str(e))
    for p in problems:
        print(f'::error title=agreement check::{p}')
    print(f'agreement check: {len(UNITS)} units against their jobs, {len(seen)} included files against '
          f'{len(paths)} push paths: {len(problems)} problem(s)')
    return 1 if problems else 0


def main():
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == 'run' and a[1] in UNITS and a[2] in PROVERS:
        return run(a[1], a[2])
    if len(a) in (2, 3) and a[0] == 'unit' and a[1] in UNITS:
        return unit_report(a[1], a[2] if len(a) == 3 else '.')
    if len(a) in (1, 2) and a[0] == 'total':
        return total(a[1] if len(a) == 2 else '.')
    if a and a[0] == 'deps' and len(a) <= 2 and (len(a) == 1 or a[1] in UNITS):
        for u in ([a[1]] if len(a) == 2 else UNITS):
            print(u + ': ' + ' '.join(sorted(unit_deps(u))))
        return 0
    if a and a[0] == 'affected':
        print(json.dumps(list(ORDER) if a[1:] == ['--all'] else affected(a[1:])))
        return 0
    if a == ['check']:
        return check()
    print(__doc__)
    print('units:', ' '.join(UNITS))
    return 2


if __name__ == '__main__':
    sys.exit(main())
