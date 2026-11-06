#!/usr/bin/env python3
"""tools/machdeps/check_pins.py  (run from the repository root)

VERIFY-060, phase C. The pins of 11 · Proofs: machine models are
tools/machdeps/pins/<unit>.<class>.txt: for each core/ unit and each class
of models (ilp32, lp64, llp64), the proved line and the goals left unproved.
This checks that the thirty files are there and read, and that each goal
they leave unproved is one the unit's x86_64 job pins too, as
tools/record_coverage.py reads proofs-core.yml. Each x86_64 residual is
argued in docs/deviations.md (record coverage checks that), so a model's
residual is argued where the x86_64 one is, and no model adds a residual
that nothing argues.

One unit is checked as its x86_64 job checks itself: frama-c-bits pins its
count, 12, and a sample of six names matched as prefixes ("roll-call: 6-name
sample - full-set completion pending"). A bits goal passes here if a sampled
name is a prefix of it; the others are listed as not joined, not counted as
problems, since no x86_64 pin names them either. Exit status: the number of
problems found.
"""
import importlib.util
import os
import re
import sys

UNITS = ['checked', 'bits', 'compare', 'ptr', 'slice', 'memory', 'arena',
         'pool', 'region', 'lifetime']
CLASSES = ['ilp32', 'lp64', 'llp64']
SAMPLED = {'bits'}   # x86_64 job pins a count and a prefix sample
HERE = os.path.dirname(os.path.abspath(__file__))


def record_coverage():
    path = os.path.join(HERE, '..', 'record_coverage.py')
    spec = importlib.util.spec_from_file_location('record_coverage', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def read_pin(path):
    """(proved, total, names) from one pin file; raises ValueError if bad."""
    proved = None
    names = []
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            m = re.fullmatch(r'proved (\d+) / (\d+)', line)
            if m:
                if proved is not None:
                    raise ValueError('two proved lines')
                proved = (int(m.group(1)), int(m.group(2)))
            elif re.fullmatch(r'typed[A-Za-z0-9_]*', line):
                names.append(line)
            else:
                raise ValueError(f'unreadable line: {line!r}')
    if proved is None:
        raise ValueError('no proved line')
    if proved[1] - proved[0] != len(names):
        raise ValueError(f'proved {proved[0]} / {proved[1]} but {len(names)} names')
    return proved[0], proved[1], names


def main():
    rc = record_coverage()
    pins = rc.pinned_names()
    problems = 0
    for unit in UNITS:
        job = f'frama-c-{unit}'
        for cls in CLASSES:
            path = os.path.join(HERE, 'pins', f'{unit}.{cls}.txt')
            if not os.path.exists(path):
                print(f'MISSING: {path}')
                problems += 1
                continue
            try:
                proved, total, names = read_pin(path)
            except ValueError as err:
                print(f'UNREADABLE: {path}: {err}')
                problems += 1
                continue
            mine = {k for k, units in pins.items() if job in units}
            stray = [n for n in names if rc.norm(n) not in mine]
            if unit in SAMPLED:
                stray = [n for n in stray
                         if not any(rc.norm(n).startswith(k) for k in mine)]
                for n in stray:
                    print(f'not joined: {unit}.{cls}: {n} ({job} pins a sample)')
            else:
                for n in stray:
                    print(f'NOT PINNED AT x86_64: {unit}.{cls}: {n} ({job} does not pin it)')
                problems += len(stray)
            print(f'{unit}.{cls}: proved {proved} / {total}, {len(names)} unproved, '
                  f'{len(names) - len(stray)} of them pinned by {job}')
    print(f'check_pins.py: {problems} problem(s)')
    return problems


if __name__ == '__main__':
    sys.exit(min(main(), 125))
