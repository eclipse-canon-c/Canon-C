#!/usr/bin/env python3
"""
agreement_probe.py -- VERIFY-064's probe: which provers prove each goal alone.

Every proof job gives Alt-Ergo, Z3 and CVC5 together each goal that WP's
simplifier, Qed, does not close, and counts the goal proved when the first
of them succeeds: WP then stops the other two (ProverTask, WP 29). A job's
statistics therefore say which prover proved a goal first, not whether the
other two could. This probe runs each unit three times, once with each
prover alone and otherwise as its job runs it, and reads each goal's result
from WP's JSON report (-wp-report-json). It sorts each unit's goals:

  - WP itself: proved with no prover, by Qed or by WP's own passes;
  - three, two or one: proved by that many of the three provers, each alone;
  - none: proved by no prover alone.

WP's summary line also counts two kinds of goal that are not in its report:
"Terminating" (terminates clauses WP validates without a proof obligation)
and "Unreachable" (goals in code WP finds unreachable). Both are read from
the summary and added back, so that a unit's count can be set beside its pin.

    python3 tools/probes/verify-064/agreement_probe.py run UNIT PROVER
        Runs WP on UNIT's file with UNIT's preprocessing and memory model,
        -wp-rte -wp-split, PROVER alone (alt-ergo, z3 or cvc5) at 120 s,
        into agree_UNIT_PROVER.txt and agree_UNIT_PROVER.json, and posts the
        run's summary as a notice. If AGREE_SECONDS is set, WP is stopped
        after that many seconds. Exits 1 if WP is stopped, or writes no
        summary or no report.

    python3 tools/probes/verify-064/agreement_probe.py unit UNIT [DIR]
        Reads UNIT's three reports and outputs under DIR (default: .) and
        writes its notices to notices_1.txt, notices_2.txt, ... (ten to a
        file: GitHub keeps ten notices of a step, so the workflow prints each
        file in a step of its own): the counts, then by name the goals one
        prover alone proves and those no prover proves alone.

    python3 tools/probes/verify-064/agreement_probe.py total [DIR]
        Reads every report under DIR, posts the totals, and writes
        agreement.tsv: each goal that WP does not prove itself, with its
        unit, the number of provers that prove it alone, and their names.

Report-only: `unit` and `total` exit 0, but 1 when they find no report. The
scoring is VERIFY-064's, in docs/deviations.md.

The units are the pointer probe's 26 (tools/probes/verify-057), each with
its job's file, memory model and preprocessing, and frama-c-diag with the
-variadic-no-translation its job adds. frama-c-arena-32, frama-c-win64,
the -CC families and the machine-model jobs are left out.
"""
import glob
import json
import os
import re
import signal
import subprocess
import sys
import time

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
PROVERS = ('alt-ergo', 'z3', 'cvc5')
NAMES = {'alt-ergo': 'Alt-Ergo', 'z3': 'Z3', 'cvc5': 'CVC5'}
CUT = 4000        # the runner cuts an annotation at 4096 characters
PER_FILE = 10     # GitHub keeps ten notices of a step
MAX_FILES = 4     # and fifty annotations of a job


def stem(unit, prover):
    return f"agree_{unit}_{prover}"


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
    """goal -> (passed, provers with a success), and WP's first two records.

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
        succ = set()
        for p in rec.get('provers') or []:
            if isinstance(p, dict) and (p.get('success') or 0) > 0:
                succ.add(canonical(p.get('prover', '?')))
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
         'one': {NAMES[p]: [] for p in PROVERS}, 'none': [], 'absent': 0, 'wp_differ': 0}
    for g in ids:
        smt, present, by_wp, other = set(), 0, 0, set()
        for p, (goals, _, _) in have.items():
            if g not in goals:
                o['absent'] += 1
                continue
            present += 1
            ok, succ = goals[g]
            if ok and NAMES[p] in succ:
                smt.add(NAMES[p])
            elif ok:
                by_wp += 1
            other |= succ - smt_names
        if len(smt) == 3:
            o['three'].append(g)
        elif len(smt) == 2:
            o['two'].setdefault('+'.join(sorted(smt)), []).append(g)
        elif len(smt) == 1:
            o['one'][next(iter(smt))].append(g)
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
    print(msg)
    notices = [f'::notice title=agreement {unit}::{msg[:CUT]}']
    lists = [(f'{unit}, proved by no prover alone ({len(o["none"])})', o['none'])]
    for k, v in o['one'].items():
        lists.append((f'{unit}, proved by {k} alone and by neither other ({len(v)})', v))
    for head, names in lists:
        for text in chunks(head, names):
            notices.append(f'::notice title=agreement {unit} names::{text[:CUT]}')
    room = PER_FILE * MAX_FILES
    if len(notices) > room:
        left = len(notices) - room + 1
        notices = notices[:room - 1] + [f'::notice title=agreement {unit} names::{left} more notices of names are not posted; '
                                        'the names are in the total job\'s agreement.tsv']
    for i in range(0, len(notices), PER_FILE):
        with open(f'notices_{i // PER_FILE + 1}.txt', 'w') as f:
            f.write('\n'.join(notices[i:i + PER_FILE]) + '\n')
    for head, names in lists:
        print(f'{head}:')
        for name in names:
            print(f'  {name}')
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
            incomplete.append(f'{unit} (no report)')
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
    split = sum(1 for c in classes.values() if len(c) > 1)
    by = {}
    for g in one:
        for n, names in classes[g]:
            if n == 1:
                by[names] = by.get(names, 0) + 1
    with open('agreement.tsv', 'w') as f:
        f.write('unit\tgoal\tprovers_alone\tnames\n')
        for row in rows:
            f.write('\t'.join(str(x) for x in row) + '\n')
    msg = (f'all units: {sums["all"]} goals, Terminating and Unreachable {sums["tu"]} of them; '
           f'proved by WP itself {sums["wp"]}; by all three provers alone {sums["three"]}; '
           f'by two {sums["two"]}; by one {sums["one"]}; by none {sums["none"]}; proved, with '
           f'Terminating and Unreachable, {sums["proved"]} / {sums["all"]}. Distinct names, across '
           f'units, that one prover alone proves: {len(one)} ('
           + ', '.join(f'{k} {v}' for k, v in sorted(by.items())) + f'); that none proves alone: '
           f'{len(none)}; sorted differently in different units: {split}')
    if incomplete:
        msg += '. INCOMPLETE: ' + ', '.join(incomplete)
    print(msg)
    print(f'::notice title=agreement total::{msg[:CUT]}')
    return 0


def main():
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == 'run' and a[1] in UNITS and a[2] in PROVERS:
        return run(a[1], a[2])
    if len(a) in (2, 3) and a[0] == 'unit' and a[1] in UNITS:
        return unit_report(a[1], a[2] if len(a) == 3 else '.')
    if len(a) in (1, 2) and a[0] == 'total':
        return total(a[1] if len(a) == 2 else '.')
    print(__doc__)
    print('units:', ' '.join(UNITS))
    return 2


if __name__ == '__main__':
    sys.exit(main())
