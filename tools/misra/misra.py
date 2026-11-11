#!/usr/bin/env python3
"""
misra.py -- the misra job's findings against their pins (VERIFY-068).

The misra job (3 · Static analysis & coverage, analysis.yml) runs
cppcheck's MISRA addon on the library's headers and counts the real
findings: those outside canary/, whose three intentional violations test
the instrument. Since VERIFY-068 each real finding is argued in
docs/deviations.md: a deviation (MISRA-DEV-nnn), or a finding of
cppcheck's that is not a violation (MISRA-TOOL-nnn). tools/misra/pins.txt
holds them by file and rule: how many, and the record that argues them.

    python3 tools/misra/misra.py report REPORT   the job's report against the pins
    python3 tools/misra/misra.py pin REPORT      write pins.txt from a report
    python3 tools/misra/misra.py check           pins.txt is well formed, and each
                                                 line names a record that exists

report prints each file and rule whose count is not its pin's, a finding
with no pin, and a pin with no finding, then one notice line; it exits 1
on a difference, which fails the misra job (enforced since VERIFY-068's
run 3), 0 otherwise, and 2 when the report cannot be read. pin keeps each pair's record and writes `?`
for a pair new to the file, which check refuses until a record is named.
8 · Records runs check (records.yml). A count changes only with a record
that says why.

Pinned by file and rule, not by line: an edit that moves a finding keeps
its pin, and a finding that comes where one of the same rule went, in the
same file, is not seen. cppcheck reports at most one MISRA finding per
line, so a fix can bring up another at the same line (docs/misra-campaign.md,
"Counts are lower bounds"): the comparison shows it as a new pair or count.
"""
import os
import re
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PINS = os.path.join(HERE, 'pins.txt')
DEVIATIONS = os.path.join(ROOT, 'docs', 'deviations.md')
ENFORCE = True    # VERIFY-068 run 3: report-only before it

FINDING = re.compile(
    r'^(?P<path>[^\s:][^:]*):(?P<line>\d+):(?P<col>\d+): \w+: .*'
    r'\[misra-(?:c2012-)?(?P<rule>[^\]]+)\]\s*$')
RECORD = re.compile(r'^(MISRA-(?:DEV|TOOL|CFG|SCAN)-\d{3})$')


def rel(path):
    """A report's path, from the repository's root."""
    full = os.path.normpath(path if os.path.isabs(path) else os.path.join(ROOT, path))
    r = os.path.relpath(full, ROOT)
    return r.replace(os.sep, '/')


def rule_key(rule):
    """Sort rules as numbers: 2.3 before 10.3; words after."""
    parts = rule.split('.')
    if all(p.isdigit() for p in parts):
        return (0, [int(p) for p in parts], rule)
    return (1, [], rule)


def read_report(path):
    """The real findings of a report, {(file, rule): count}, and the canary's count."""
    try:
        with open(path, errors='replace') as f:
            lines = f.read().split('\n')
    except OSError as e:
        print(f'misra.py: cannot read {path}: {e}')
        sys.exit(2)
    found, canary = {}, 0
    for line in lines:
        m = FINDING.match(line.strip())
        if not m:
            continue
        f = rel(m.group('path'))
        if f.startswith('canary/'):
            canary += 1
            continue
        key = (f, m.group('rule'))
        found[key] = found.get(key, 0) + 1
    return found, canary


def read_pins():
    """{(file, rule): (count, record)}, and the problems reading the file."""
    pins, problems = {}, []
    if not os.path.exists(PINS):
        return pins, ['tools/misra/pins.txt: missing']
    with open(PINS) as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            cells = text.split()
            if len(cells) != 4 or not cells[2].isdigit() or int(cells[2]) < 1:
                problems.append(f'pins.txt:{n}: not "file rule count record": {line.strip()}')
                continue
            key = (cells[0], cells[1])
            if key in pins:
                problems.append(f'pins.txt:{n}: {cells[0]} {cells[1]} pinned twice')
                continue
            pins[key] = (int(cells[2]), cells[3])
    return pins, problems


def write_pins(found, records):
    width = max([len(f) for f, _ in found] + [4])
    with open(PINS, 'w') as out:
        out.write('# The misra job\'s real findings, by file and rule (VERIFY-068): how\n'
                  '# many, and the record in docs/deviations.md that argues them. Written\n'
                  '# by `misra.py pin`; a count changes only with a record that says why.\n'
                  '#\n')
        out.write(f'# {"file":<{width - 2}}  {"rule":<7}  {"count":>5}  record\n')
        for key in sorted(found, key=lambda k: (k[0], rule_key(k[1]))):
            out.write(f'{key[0]:<{width}}  {key[1]:<7}  {found[key]:>5}  {records.get(key, "?")}\n')


def report(path):
    found, canary = read_report(path)
    pins, problems = read_pins()
    for p in problems:
        print(p)
    diffs = []
    for key in sorted(set(found) | set(pins), key=lambda k: (k[0], rule_key(k[1]))):
        n = found.get(key, 0)
        if key not in pins:
            diffs.append(f'{key[0]} {key[1]}: {n} finding(s), no pin')
        elif n != pins[key][0]:
            diffs.append(f'{key[0]} {key[1]}: {n} finding(s), pinned {pins[key][0]} ({pins[key][1]})')
    total = sum(found.values())
    pinned = sum(c for c, _ in pins.values())
    for key in sorted(found, key=lambda k: (k[0], rule_key(k[1]))):
        record = pins[key][1] if key in pins else 'no pin'
        print(f'{key[0]} {key[1]}: {found[key]}  {record}')
    if diffs or problems:
        print(f'Not as pinned: {len(diffs)} difference(s)')
        for d in diffs:
            print('  ' + d)
        head = f'not as pinned: {len(diffs)} difference(s): ' + '; '.join(diffs)
    else:
        head = f'as pinned: {total} real finding(s) in {len(found)} file and rule pair(s)'
    print(f'Real findings {total} (pinned {pinned}); canary {canary}')
    print(f'::notice title=misra pins::{head}')
    return 1 if (ENFORCE and (diffs or problems)) else 0


def pin(path):
    found, _ = read_report(path)
    pins, _ = read_pins()
    records = {k: r for k, (_, r) in pins.items()}
    write_pins(found, records)
    new = [k for k in found if k not in records]
    print(f'wrote tools/misra/pins.txt: {sum(found.values())} finding(s), {len(found)} pair(s)'
          + (f'; {len(new)} new pair(s) need a record' if new else ''))
    return 0


def check():
    pins, problems = read_pins()
    with open(DEVIATIONS) as f:
        headings = set(re.findall(r'^## (MISRA-[A-Z]+-\d{3}):', f.read(), re.M))
    for (f, rule), (count, record) in sorted(pins.items()):
        if not RECORD.match(record):
            problems.append(f'{f} {rule}: no record named ({record})')
        elif record not in headings:
            problems.append(f'{f} {rule}: {record} is not a heading of docs/deviations.md')
        if not os.path.exists(os.path.join(ROOT, f)):
            problems.append(f'{f} {rule}: no such file')
    for p in problems:
        print(p)
    print(f'pins: {len(pins)} pair(s), {sum(c for c, _ in pins.values())} finding(s); '
          f'{len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'report':
        return report(argv[2])
    if len(argv) == 3 and argv[1] == 'pin':
        return pin(argv[2])
    if len(argv) == 2 and argv[1] == 'check':
        return check()
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
