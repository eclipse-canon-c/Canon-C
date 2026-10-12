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
row's function (as declared, or without the leading, trailing and repeated
underscores the kernel drops from most goal names), a fact cited but not
stated, or counts that differ from the block's closing line. A name
misspelled after its function's part is not caught: the script has the pinned
names, not WP's full list of goals.

It also reads the record's `**Filed by name**: VERIFY-nnn, <heading>` lines
(VERIFY-039, the main pass), each with a goal table that files residuals by
name under a block of another section, cited by its record and the start of
its heading up to the colon or count (`VERIFY-008, Category 1`). The table
starts on the next line that is not blank: a header row, then rows numbered
in the first cell, each with one goal in backticks in the Goal column (the
second, if no column is so named) and, if the header has a Units column, the
units that pin it, in backticks. Each goal so filed counts for the cited
block as its own table's goals do: it must be pinned somewhere, it is
reported stale if not, and it leaves the "not listed in any goal table"
count. A block's own table is the goal table between its heading and its
argument, the filing tables there left out. A block's heading count is
checked against the distinct goals of its own table, or, for a block with
none, against the distinct goals filed by name under it. A retired block
covers nothing: its rows stay for goals that closed, and a goal still pinned
in its table counts as in no goal table. Filing problems: a line that starts
`**Filed by name` but does not read as above; a citation that matches no
block, several, or a retired one; a line with no table after it, or a table
none of whose rows is read; a row not read (not numbered, its cells not the
header's number, or a goal cell that is not one goal in backticks); a Units
cell that differs from the units that pin the goal (`frama-c-arena-32`,
whose embedded copy of frama-c-arena's set the script does not read, may
stand beside `frama-c-arena`); a goal filed twice, by name or in a block's
own table; and a goal still pinned in a retired block's table. A filing line
starts in the first column; one that is indented is a problem too. Lines
inside a fenced block, ``` or ~~~, are not read as filings.

Advisory: prints a report and exits 0, unless --strict, in which case any
stale listed goal, empty block, pinned filed part, problem reading the
proved-goal block or filing problem exits 1, or --complete, in which case a
pinned name in no goal table exits 1.

Sources:
  - .github/workflows/cmake-multi-platform.yml   CHECKS=( / INHERITED=( / PIN_NAMES=( arrays,
                                                  EXPECTED_GOAL_n names (frama-c-checked), and
                                                  frama-c-memory's CHECKS for
                                                  frama-c-priority-queue, which pins
                                                  them at run time
  - vmacros/vdrivers/cc_pins/*.txt               the -CC roll-calls
  - docs/deviations.md                            goal tables in argument blocks;
                                                  the filed-by-name tables;
                                                  the proved-goal block's rows

Limits, stated: four blocks (option, result, borrow, diag) have no goal
table: option's and result's list their residuals by function and count, and
borrow's and diag's name theirs in prose. Until goals are filed by name
under them they are reported as not joined. bits.h's roll-call is a
documented 6-of-15 sample, and bitset's pins all 15; two of the sample's
names, bits_next_power_of_two_normal_ensures and
bits_rotl_nonzero_shift_ensures, are whole goals that bitset pins split,
as _part3 and _part2, so they count as names of their own: over the main
CI's units the script's union is the paper's (§6.3) and these two. Goal
names are compared modulo the typed_ / typed_cast_ prefix, as in the paper.
"""
import re, sys, glob, argparse
from collections import defaultdict

WF = '.github/workflows/cmake-multi-platform.yml'
REC = 'docs/deviations.md'
CC = 'vmacros/vdrivers/cc_pins/*.txt'

def norm(n): return re.sub(r'^typed_cast_', 'typed_', n)

def pinned_names():
    """{normalised name: set(units that pin it)}: each frama-c job's CHECKS,
    INHERITED and PIN_NAMES arrays and EXPECTED_GOAL_n names (frama-c-checked
    pins its two so); frama-c-priority-queue, which pins frama-c-memory's
    first CHECKS array by set equality at run time, for each name in it; and
    the -CC pin files. frama-c-arena-32's embedded list is not read: it is a
    copy of frama-c-arena's set, which that job's gate compares with the
    32-bit run and an advisory check with frama-c-arena's CHECKS (paper
    §6.5). A job header that is not a frama-c job's ends the reading."""
    out = defaultdict(set)
    cur = None; grab = False; mem_checks = set(); mem_state = 0
    for l in open(WF, encoding='utf-8'):
        m = re.match(r'^  ([A-Za-z0-9_-]+):\s*$', l)
        if m:
            cur = m.group(1) if m.group(1).startswith('frama-c-') else None
            grab = False; continue
        if cur is None or cur == 'frama-c-arena-32': continue
        e = re.match(r'\s*EXPECTED_GOAL_\d+="(typed[A-Za-z0-9_]*)"', l)
        if e: out[norm(e.group(1))].add(cur); continue
        a = re.search(r'(CHECKS|INHERITED|PIN_NAMES)=\(', l)
        if a:
            grab = True
            if cur == 'frama-c-memory' and a.group(1) == 'CHECKS' and mem_state == 0: mem_state = 1
            continue
        if grab and l.strip() == ')':
            grab = False
            if mem_state == 1: mem_state = 2
            continue
        if grab:
            m = re.match(r'\s*"(typed[A-Za-z0-9_]*)"', l)
            if m:
                out[norm(m.group(1))].add(cur)
                if mem_state == 1: mem_checks.add(norm(m.group(1)))
    for n in mem_checks: out[n].add('frama-c-priority-queue')
    for f in glob.glob(CC):
        unit = 'cc-' + f.split('/')[-1][:-4]
        for l in open(f, encoding='utf-8'):
            n = l.strip()
            if n.startswith('typed'): out[norm(n)].add(unit)
    return out

FILED = re.compile(r'^\*\*Filed by name\*\*: (VERIFY-\d+), (.+?)\s*$')
FENCE = re.compile(r'^\s*(`{3,}|~{3,})')

def filing_tables(lines):
    """[(index of the line, record cited, heading cited, [(index, row)])]
    for each line outside a fenced block that starts `**Filed by name`,
    after any indentation, and the table that starts on the next line that
    is not blank (no rows if none does); record and heading are None if the
    line does not read as `**Filed by name**: VERIFY-nnn, <heading>` from its
    first column."""
    out = []; fence = None
    for i, l in enumerate(lines):
        f = FENCE.match(l)
        if f:
            c = f.group(1)[0]
            if fence is None: fence = c
            elif fence == c: fence = None
            continue
        if fence is not None or not l.lstrip().startswith('**Filed by name'): continue
        m = FILED.match(l)
        j = i + 1
        while j < len(lines) and not lines[j].strip(): j += 1
        rows = []
        while j < len(lines) and lines[j].startswith('|'):
            rows.append((j, lines[j])); j += 1
        out.append((i, m.group(1) if m else None, m.group(2) if m else None, rows))
    return out

def record_blocks():
    """[(block heading, record id, [goal names listed in its table])]; the
    table is read from the heading to the argument, the filing tables there
    left out."""
    lines = open(REC, encoding='utf-8').read().split('\n')
    skip = {j for i, _, _, rows in filing_tables(lines) for j in [i] + [k for k, _ in rows]}
    rec = None; blocks = []
    for i, l in enumerate(lines):
        m = re.match(r'^## (VERIFY-\d+)', l)
        if m: rec = m.group(1)
        if l.startswith('**Manual proof argument**:'):
            h = next(j for j in range(i, max(i - 200, 0), -1)
                     if lines[j].startswith('#### ') or lines[j].startswith('### '))
            goals = []
            for j in range(h, i):
                if j in skip: continue
                t = lines[j]
                g = re.match(r'^\|\s*\d+\s*\|\s*`(typed[A-Za-z0-9_]*)`', t)
                if g: goals.append(g.group(1))
                if t.startswith('**Functions affected**'): break
            blocks.append((lines[h].strip(), rec, goals))
    return blocks

def filed_by_name_tables():
    """[(line, record cited, heading cited, [(line, goal, units or None)],
    [problems])] for each `**Filed by name**: VERIFY-nnn, <heading>` line;
    units are the backticked unit names of the row's Units cell, None if the
    header has no Units column. Line numbers count from 1."""
    lines = open(REC, encoding='utf-8').read().split('\n')
    out = []
    for i, rec, cited, rows in filing_tables(lines):
        goals = []; problems = []
        if rec is None:
            problems.append(f"line {i + 1}: not read as `**Filed by name**: VERIFY-nnn, <heading>`; "
                            f"its table not read")
            out.append((i + 1, rec, cited, goals, problems)); continue
        where = f"line {i + 1} ('{rec}, {cited}')"
        if not rows:
            problems.append(f"{where}: no table on the next line that is not blank")
            out.append((i + 1, rec, cited, goals, problems)); continue
        head = cells(rows[0][1])
        if re.fullmatch(r'\d+', head[0]):
            problems.append(f"{where}: the table's first row is numbered; a header row comes first")
        gcol = head.index('Goal') if 'Goal' in head else 1
        ucol = head.index('Units') if 'Units' in head else None
        for j, r in rows[1:]:
            c = cells(r)
            if all(re.fullmatch(r':?-+:?', x) for x in c): continue
            if len(c) != len(head):
                problems.append(f"line {j + 1}: {len(c)} cells, the header has {len(head)}; not read"); continue
            if not re.fullmatch(r'\d+', c[0]):
                problems.append(f"line {j + 1}: row not numbered; not read"); continue
            g = re.fullmatch(r'`(typed[A-Za-z0-9_]*)`', c[gcol])
            if not g:
                problems.append(f"line {j + 1}: the goal cell is not one goal in backticks; not read"); continue
            units = (set(re.findall(r'`((?:frama-c|cc)-[A-Za-z0-9_-]+)`', c[ucol]))
                     if ucol is not None else None)
            goals.append((j + 1, g.group(1), units))
        if not goals:
            problems.append(f"{where}: no goal read in the table")
        out.append((i + 1, rec, cited, goals, problems))
    return out

def resolve_filings(blocks, pins):
    """({block index: [goals filed by name under it]}, number of filing
    lines, [problems])"""
    filed = defaultdict(list); problems = []; tables = 0
    for line, rec, cited, goals, probs in filed_by_name_tables():
        tables += 1
        problems.extend(probs)
        if rec is None: continue
        def cites(b):
            text = b[0].lstrip('#').strip()
            rest = text[len(cited):]
            return (b[1] == rec and text.startswith(cited)
                    and (rest == '' or rest[0] == ':' or rest.lstrip().startswith('(')))
        hits = [k for k, b in enumerate(blocks) if cites(b)]
        if len(hits) != 1:
            problems.append(f"line {line}: '{rec}, {cited}' matches {len(hits)} blocks, not one")
            continue
        if 'RETIRED' in blocks[hits[0]][0]:
            problems.append(f"line {line}: '{rec}, {cited}' is retired; goals are filed under a block in force")
            continue
        for n, g, units in goals:
            want = pins.get(norm(g))
            if units is None or not want: continue
            have = units - {'frama-c-arena-32'} if 'frama-c-arena' in units else set(units)
            if have != want:
                problems.append(f"line {n}: {g}: the Units cell names {', '.join(sorted(units)) or 'none'}; "
                                f"pinned in {', '.join(sorted(want))}")
        if goals: filed[hits[0]].extend(g for _, g, _ in goals)
    seen = defaultdict(int)
    for k, b in enumerate(blocks):
        for g in b[2]: seen[norm(g)] += 1
    for k, gs in filed.items():
        for g in gs: seen[norm(g)] += 1
    for g in sorted(n for n, c in seen.items() if c > 1):
        problems.append(f"filed twice: {g}")
    return filed, tables, problems

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
    property whose name does not contain "_<the row's Function cell>_", the
    cell as declared or as the kernel's sanitizer writes it, are problems too.
    The block's last closing line, "Filed: N properties at M write sites,
    kept off by K facts", is checked against what was read."""
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
            # WP writes a function's name as declared in a call-site goal's
            # name, and in the others through the kernel's sanitizer, which
            # drops leading, trailing and repeated underscores:
            # _stringbuf_append_bytes's postconditions are
            # typed_cast_stringbuf_append_bytes_ensures_...
            forms = {fn.group(1), re.sub(r'_+', '_', fn.group(1)).strip('_')} if fn else set()
            for q, _ in props:
                if fn and not any(f"_{f}_" in q for f in forms):
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
                    help='exit 1 on any stale listed goal, empty block, pinned filed part, '
                         'problem reading the proved-goal block, or filing problem')
    ap.add_argument('--complete', action='store_true',
                    help='exit 1 while any pinned name is in no goal table '
                         '(VERIFY-039\'s main pass ends when none is)')
    a = ap.parse_args()
    pins = pinned_names()
    blocks = record_blocks()
    filed, tables, filing_problems = resolve_filings(blocks, pins)
    listed_all = set()
    stale_total = 0; empty = []; count_drift = []
    print(f"pinned distinct names (mod prefix): {len(pins)}")
    print(f"argument blocks: {len(blocks)}\n")
    for k, (heading, rec, goals) in enumerate(blocks):
        m = re.search(r'\((\d+)(?:\s*—[^)]*)?\)\s*$', heading)
        declared = int(m.group(1)) if m else None
        more = filed.get(k, [])
        if not goals and not more:
            print(f"[{rec}] {heading[:80]}\n    no goal table, and no goal filed by name under it — not joined")
            continue
        live = [g for g in goals if norm(g) in pins]
        stale = [g for g in goals if norm(g) not in pins]
        more_live = [g for g in more if norm(g) in pins]
        more_stale = [g for g in more if norm(g) not in pins]
        flag = ''
        if 'RETIRED' in heading:
            # A retired block covers nothing: its rows are kept for legibility,
            # for goals that closed. One still pinned is in no goal table.
            flag = '  (retired, acknowledged in the heading; rows kept for legibility)'
            stale = []  # acknowledged retirement is not drift
            if live:
                flag += f'  <-- {len(live)} still pinned'
                filing_problems.extend(f"pinned, in the table of a retired block ({rec}): {g}" for g in live)
        else:
            listed_all |= {norm(g) for g in goals}
            if not live and not more_live:
                flag = '  <-- EMPTY: covers nothing; retired in the §3 sense'; empty.append(heading)
            elif stale or more_stale:
                flag = f'  <-- {len(stale) + len(more_stale)} listed goal(s) no longer pinned'
        listed_all |= {norm(g) for g in more}
        counted = len({norm(g) for g in live}) if goals else len({norm(g) for g in more_live})
        if declared is not None and declared != counted:
            count_drift.append((heading, declared, counted))
            flag += f'  <-- heading says {declared}, live {counted}'
        extra = (f"  filed by name elsewhere {len(more)}: live {len(more_live)}, stale {len(more_stale)}"
                 if more else '')
        print(f"[{rec}] {heading[:80]}\n    listed {len(goals)}  live {len(live)}  stale {len(stale)}{extra}{flag}")
        for g in stale + more_stale: print(f"      stale: {g}")
        stale_total += len(stale) + len(more_stale)
    if tables or filing_problems:
        print(f"\nfiled-by-name tables: {tables}, {sum(len(v) for v in filed.values())} goals "
              f"under {len(filed)} block(s)")
        for pr in filing_problems: print(f"      {pr}")
    unlisted = sorted(n for n in pins if n not in listed_all)
    by_unit = defaultdict(int)
    for n in unlisted:
        for u in pins[n]: by_unit[u] += 1
    print(f"\npinned names not listed in any goal table: {len(unlisted)} "
          f"(VERIFY-039's main pass files them by name; --complete fails until this is 0)")
    print("  per unit: " + ", ".join(f"{u}:{c}" for u, c in sorted(by_unit.items())))
    filed_pinned, problems = check_proved_goals(pins)
    print(f"\nSUMMARY  stale listed goals: {stale_total}   empty blocks: {len(empty)}   heading/live count drift: {len(count_drift)}"
          f"   filed parts pinned: {filed_pinned}   block problems: {problems}   filing problems: {len(filing_problems)}")
    if a.strict and (stale_total or empty or filed_pinned or problems or filing_problems):
        sys.exit(1)
    if a.complete and unlisted:
        sys.exit(1)

if __name__ == '__main__':
    main()
