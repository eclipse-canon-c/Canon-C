#!/usr/bin/env python3
"""
record_coverage.py — join docs/deviations.md to the CI roll-calls.

The gates pin the residual set by name. Nothing pins the *record's* claim
about which argument covers which residual. This script performs the join
the paper's §5.7 says is missing: for every goal table under a
`**Manual proof argument**` block it checks that each listed goal is still a
pinned residual somewhere, and for every block it reports how many of its
listed goals are still live. It would have flagged VERIFY-009 category 2d as
empty on 2026-09-06, the day VERIFY-023 closed its two goals.

Advisory: prints a report and exits 0, unless --strict, in which case any
stale listed goal or empty block exits 1.

Sources:
  - .github/workflows/cmake-multi-platform.yml   CHECKS=( / INHERITED=( / PIN_NAMES=( arrays
  - vmacros/vdrivers/cc_pins/*.txt               the -CC roll-calls
  - docs/deviations.md                            goal tables in argument blocks

Limits, stated: four records (option, result, borrow, diag) list residuals by
function and count rather than by goal name, so their coverage is reported as
"listed by function" and not joined. bits.h's roll-call is a documented 6-of-15
sample; its names are taken from bitset's full roll-call. Goal names are
compared modulo the typed_ / typed_cast_ prefix, as in the paper.
"""
import re, sys, glob, argparse
from collections import defaultdict

WF = '.github/workflows/cmake-multi-platform.yml'
REC = 'docs/deviations.md'
CC = 'vmacros/vdrivers/cc_pins/*.txt'

def norm(n): return re.sub(r'^typed_cast_', 'typed_', n)

def pinned_names():
    """{normalised name: set(units that pin it)}"""
    out = defaultdict(set)
    cur = None; grab = False
    for l in open(WF, encoding='utf-8'):
        m = re.match(r'^  (frama-c-[a-z0-9-]+):\s*$', l)
        if m: cur = m.group(1); grab = False; continue
        if cur is None or cur == 'frama-c-arena-32': continue
        if re.search(r'(CHECKS|INHERITED|PIN_NAMES)=\(', l): grab = True; continue
        if grab and l.strip() == ')': grab = False; continue
        if grab:
            m = re.match(r'\s*"(typed[A-Za-z0-9_]*)"', l)
            if m: out[norm(m.group(1))].add(cur)
    for f in glob.glob(CC):
        unit = 'cc-' + f.split('/')[-1][:-4]
        for l in open(f, encoding='utf-8'):
            n = l.strip()
            if n.startswith('typed'): out[norm(n)].add(unit)
    return out

def record_blocks():
    """[(block heading, record id, [goal names listed in its table])]"""
    lines = open(REC, encoding='utf-8').read().split('\n')
    rec = None; blocks = []
    for i, l in enumerate(lines):
        m = re.match(r'^## (VERIFY-\d+)', l)
        if m: rec = m.group(1)
        if l.startswith('**Manual proof argument**:'):
            h = next(j for j in range(i, max(i - 200, 0), -1)
                     if lines[j].startswith('#### ') or lines[j].startswith('### '))
            goals = []
            for t in lines[h:i]:
                g = re.match(r'^\|\s*\d+\s*\|\s*`(typed[A-Za-z0-9_]*)`', t)
                if g: goals.append(g.group(1))
                if t.startswith('**Functions affected**'): break
            blocks.append((lines[h].strip(), rec, goals))
    return blocks

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--strict', action='store_true', help='exit 1 on any stale goal or empty block')
    a = ap.parse_args()
    pins = pinned_names()
    blocks = record_blocks()
    listed_all = set()
    stale_total = 0; empty = []; count_drift = []
    print(f"pinned distinct names (mod prefix): {len(pins)}")
    print(f"argument blocks: {len(blocks)}\n")
    for heading, rec, goals in blocks:
        m = re.search(r'\((\d+)(?:\s*—[^)]*)?\)\s*$', heading)
        declared = int(m.group(1)) if m else None
        if not goals:
            print(f"[{rec}] {heading[:80]}\n    listed by function/count, not by goal name — not joined")
            continue
        live = [g for g in goals if norm(g) in pins]
        stale = [g for g in goals if norm(g) not in pins]
        listed_all |= {norm(g) for g in goals}
        flag = ''
        acknowledged = 'RETIRED' in heading
        if not live and acknowledged:
            flag = '  (retired, acknowledged in the heading; rows kept for legibility)'
            stale = []  # acknowledged retirement is not drift
        elif not live: flag = '  <-- EMPTY: covers nothing; retired in the §3 sense'; empty.append(heading)
        elif stale: flag = f'  <-- {len(stale)} listed goal(s) no longer pinned'
        if declared is not None and declared != len(live):
            count_drift.append((heading, declared, len(live)))
            flag += f'  <-- heading says {declared}, live {len(live)}'
        print(f"[{rec}] {heading[:80]}\n    listed {len(goals)}  live {len(live)}  stale {len(stale)}{flag}")
        for g in stale: print(f"      stale: {g}")
        stale_total += len(stale)
    unlisted = sorted(n for n in pins if n not in listed_all)
    by_unit = defaultdict(int)
    for n in unlisted:
        for u in pins[n]: by_unit[u] += 1
    print(f"\npinned names not listed in any goal table: {len(unlisted)} "
          f"(expected for option/result/borrow/diag/vec/deque/bitset/pq own goals, which the records list by function or class)")
    print("  per unit: " + ", ".join(f"{u}:{c}" for u, c in sorted(by_unit.items())))
    print(f"\nSUMMARY  stale listed goals: {stale_total}   empty blocks: {len(empty)}   heading/live count drift: {len(count_drift)}")
    if a.strict and (stale_total or empty):
        sys.exit(1)

if __name__ == '__main__':
    main()
