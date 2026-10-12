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

It also reads the `**Proved-goal argument**` block (VERIFY-039 G4): proved
goals whose evidence is an argument, because WP's typed model takes a location
to be unchanged across a write it cannot see reach it (through a character
type; of another type, read back through a character type; or through the
signed or unsigned counterpart), and the block's rows say why it is. A filed
property must stay proved. For each one, named in a row's Properties cell, the
script reports any pinned residual whose name is the property's, or the
property's followed by _partk, unless the row excludes that part with
"(not `_partk`)": such a goal has stopped proving and needs a residual
argument instead. It reports the block's numbers of write sites (rows),
properties and facts (the **Fk** labels), a property filed twice, a fact a
row cites in its "Kept off by" cell that the block does not state or one no
row cites, an excluded part that is no longer pinned, and any problem reading
the block: a row it cannot read, a property whose name does not contain its
row's function, a fact cited but not stated, or counts that differ from the
block's closing line. A name misspelled after its function's part is not
caught: the script has the pinned names, not WP's full list of goals.

Advisory: prints a report and exits 0, unless --strict, in which case any
stale listed goal, empty block, pinned filed part or problem reading the
proved-goal block exits 1.

Sources:
  - .github/workflows/cmake-multi-platform.yml   CHECKS=( / INHERITED=( / PIN_NAMES=( arrays
  - vmacros/vdrivers/cc_pins/*.txt               the -CC roll-calls
  - docs/deviations.md                            goal tables in argument blocks;
                                                  the proved-goal block's rows

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

def cells(row):
    """A Markdown table row's cells, split at the pipes not escaped."""
    return [c.strip() for c in re.split(r'(?<!\\)\|', row.strip().strip('|'))]

def proved_goal_blocks():
    """[(heading, record id, rows, facts stated, problems)] for each
    **Proved-goal argument** block; rows = [(row number, [(property, excluded
    parts)], facts cited)]. A block runs from its first line to the next
    heading. Properties are read from the Properties cell and facts from the
    "Kept off by" cell, as the table's header row places them; a row is not
    read, and a problem is reported, if its cells differ in number from the
    header's, it is not numbered, it comes before any header, or it names no
    property or no fact. A name in the Properties cell outside backticks, and a
    property whose name does not contain "_<the row's Function cell>_", are
    problems too. The block's last closing line, "Filed:
    N properties at M write sites, kept off by K facts", is checked against
    what was read."""
    lines = open(REC, encoding='utf-8').read().split('\n')
    rec = None; blocks = []
    for i, l in enumerate(lines):
        m = re.match(r'^## (VERIFY-\d+)', l)
        if m: rec = m.group(1)
        if not l.startswith('**Proved-goal argument**:'): continue
        h = next(j for j in range(i, -1, -1)
                 if lines[j].startswith('#### ') or lines[j].startswith('### '))
        rows = []; facts = set(); problems = []; col = None; prose = []
        for t in lines[i + 1:]:
            if re.match(r'#{1,6} ', t): break
            if not t.startswith('|'):
                col = None                     # a table ends at its first other line
                prose.append(t)
                f = re.match(r'^- \*\*(F\d+)\*\*', t)
                if f: facts.add(f.group(1))
                continue
            c = cells(t)
            if 'Properties' in c and 'Kept off by' in c:
                col = (c.index('Properties'), c.index('Kept off by'), len(c),
                       c.index('Function') if 'Function' in c else None); continue
            if all(re.fullmatch(r':?-+:?', x) for x in c): continue
            if col is None:
                if re.fullmatch(r'\d+', c[0]):
                    problems.append(f"row {c[0]}: before any header with Properties and Kept off by")
                continue                       # a table that is not the block's
            if not re.fullmatch(r'\d+', c[0]):
                problems.append(f"row not numbered: {t[:60]}"); continue
            if len(c) != col[2]:
                problems.append(f"row {c[0]}: {len(c)} cells, the header has {col[2]}"); continue
            props = [(p.group(1), set(re.findall(r'`(_part\d+)`', p.group(2) or '')))
                     for p in re.finditer(r'`(typed[A-Za-z0-9_]*)`(\s*\(not\s[^)]*\))?', c[col[0]])]
            cited = set(re.findall(r'\b(F\d+)\b', c[col[1]]))
            if not props or not cited:
                problems.append(f"row {c[0]}: no {'property' if not props else 'fact'} read"); continue
            for q in re.findall(r'\btyped\w*', re.sub(r'`[^`]*`', '', c[col[0]])):
                problems.append(f"row {c[0]}: {q} is not in backticks, so not read")
            fn = re.fullmatch(r'`(\w+)`', c[col[3]]) if col[3] is not None else None
            for q, _ in props:
                if fn and f"_{fn.group(1)}_" not in q:
                    problems.append(f"row {c[0]}: {q} does not name the row's function {fn.group(1)}")
            rows.append((int(c[0]), props, cited))
        n_props = sum(len(ps) for _, ps, _ in rows)
        closes = list(re.finditer(r'Filed: (\d+) propert(?:y|ies) at (\d+) write sites?, '
                                  r'kept off by (\d+) facts?', ' '.join(prose)))
        close = closes[-1] if closes else None
        cited_all = set().union(*(c for _, _, c in rows)) if rows else set()
        for c in sorted(cited_all - facts):
            problems.append(f"fact cited, not stated: {c}")
        if not close:
            problems.append("no closing line (Filed: N properties at M write sites, kept off by K facts)")
        elif tuple(map(int, close.groups())) != (n_props, len(rows), len(facts)):
            problems.append(f"closing line says {close.group(0)!r}; read {n_props} properties, "
                            f"{len(rows)} write sites, {len(facts)} facts")
        blocks.append((lines[h].strip(), rec, rows, facts, problems))
    return blocks

def check_proved_goals(pins):
    """Report the proved-goal blocks; return the numbers of filed parts pinned
    and of problems reading the blocks."""
    blocks = proved_goal_blocks()
    print(f"\nproved-goal argument blocks: {len(blocks)}")
    pinned_total = 0; problem_total = 0
    for heading, rec, rows, facts, problems in blocks:
        props = [(p, ex) for _, ps, _ in rows for p, ex in ps]
        cited = set().union(*(c for _, _, c in rows)) if rows else set()
        names = [norm(p) for p, _ in props]
        print(f"[{rec}] {heading[:80]}\n    write sites {len(rows)}  properties {len(props)}  "
              f"facts {len(facts)}")
        for pr in problems: print(f"      {pr}")
        problem_total += len(problems)
        for n in sorted({n for n in names if names.count(n) > 1}):
            print(f"      filed twice: {n}")
        for c in sorted(facts - cited): print(f"      stated, not cited: {c}")
        bad = 0
        for p, ex in props:
            n = norm(p)
            for m in sorted(pins):
                if m != n and not re.fullmatch(re.escape(n) + r'_part\d+', m): continue
                part = m[len(n):]
                if part and part in ex: continue
                bad += 1
                print(f"      FILED BUT PINNED: {m} ({', '.join(sorted(pins[m]))})")
            for part in sorted(ex):
                if n + part not in pins:
                    print(f"      excluded part no longer pinned: {p}{part}")
        print(f"    filed parts pinned: {bad}")
        pinned_total += bad
    return pinned_total, problem_total

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--strict', action='store_true',
                    help='exit 1 on any stale goal, empty block, pinned filed part or block problem')
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
    filed_pinned, problems = check_proved_goals(pins)
    print(f"\nSUMMARY  stale listed goals: {stale_total}   empty blocks: {len(empty)}   heading/live count drift: {len(count_drift)}"
          f"   filed parts pinned: {filed_pinned}   block problems: {problems}")
    if a.strict and (stale_total or empty or filed_pinned or problems):
        sys.exit(1)

if __name__ == '__main__':
    main()
