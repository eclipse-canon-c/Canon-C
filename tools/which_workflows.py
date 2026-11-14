#!/usr/bin/env python3
"""which_workflows.py BASE HEAD | --files FILE...: the workflows a push to
master starts, read from each workflow's push filters, as GitHub reads
them. Run from the repository root; the workflow files are read from the
working tree.

A workflow with `paths` runs if a changed file matches one of them, a later
`!pattern` excluding it again. A workflow with `paths-ignore` runs unless
every changed file matches one of them. A workflow with neither runs on
every push. In a pattern, `*` matches within a path segment, `**` across
segments and `?` one character. A record's pre-registration names the
workflows its push will start from this script (VERIFY-061, phase C's run 2,
C7). Needs PyYAML.

After the workflows it lists what reads the changed files inside them
(VERIFY-080): the units 13 · Prover agreement proves
(tools/agreement/agreement.py affected), the probes 9 · Probes starts (its
changes job's filters), and the Eva programs, 14 · Eva's, that include a
changed file. A program whose unit is preprocessed with -CC reads the
annotations inside macro bodies, so an annotation added to a macro of a
changed header can change its Eva pins too, as VERIFY-079's run 2 found:
the list marks those programs, and a record pre-registering such a change
predicts their pins.
"""
import fnmatch
import glob
import importlib.util
import json
import os
import re
import subprocess
import sys

import yaml


def pattern(glob_pattern):
    out, i = '', 0
    while i < len(glob_pattern):
        if glob_pattern.startswith('**', i):
            out, i = out + '.*', i + 2
        elif glob_pattern[i] == '*':
            out, i = out + '[^/]*', i + 1
        elif glob_pattern[i] == '?':
            out, i = out + '[^/]', i + 1
        else:
            out, i = out + re.escape(glob_pattern[i]), i + 1
    return re.compile(out + r'\Z')


def matches(path, patterns):
    hit = False
    for p in patterns:
        if p.startswith('!'):
            if pattern(p[1:]).match(path):
                hit = False
        elif pattern(p).match(path):
            hit = True
    return hit


def main(argv):
    if len(argv) >= 2 and argv[0] == '--files':
        files = argv[1:]
    elif len(argv) == 2:
        files = subprocess.run(['git', 'diff', '--name-only', argv[0], argv[1]],
                               capture_output=True, text=True,
                               check=True).stdout.split()
    else:
        print(__doc__)
        return 2
    print('changed:', ' '.join(files))
    for wf in sorted(glob.glob('.github/workflows/*.yml')):
        with open(wf) as f:
            d = yaml.safe_load(f)
        on = d.get('on', d.get(True))    # YAML 1.1 reads a bare `on` as true
        if not isinstance(on, dict) or 'push' not in on:
            continue
        push = on['push'] or {}
        if 'paths' in push:
            runs = any(matches(p, push['paths']) for p in files)
        elif 'paths-ignore' in push:
            runs = not all(matches(p, push['paths-ignore']) for p in files)
        else:
            runs = True
        print(('runs   ' if runs else 'skips  ') + d.get('name', wf))
    readers(files)
    return 0


def load(path):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location(os.path.basename(path)[:-3], path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def readers(files):
    """What reads the changed files inside the workflows: 13's units, 9's
    probes, and 14's programs, those reading macro bodies marked."""
    agreement = load(os.path.join('tools', 'agreement', 'agreement.py'))
    print('13 units: ' + (' '.join(agreement.affected(files)) or 'none'))
    with open(os.path.join('.github', 'workflows', 'probes.yml')) as f:
        d = yaml.safe_load(f)
    step = next(s for s in d['jobs']['changes']['steps'] if s.get('id') == 'f')
    filters = json.loads(step['env']['FILTERS'])
    hit = [k for k, pats in filters.items()
           if any(fnmatch.fnmatch(c, p) for c in files for p in pats)]
    print('9 probes: ' + (' '.join(hit) or 'none'))
    eva = load(os.path.join('tools', 'eva', 'eva.py'))
    changed = {os.path.normpath(f) for f in files}
    macro, plain = [], []
    for program in eva.PROGRAMS:
        if eva.includes(program) & changed or eva.source(program) in changed:
            cc = '-CC' in eva.unit_of(program)[2].split()
            (macro if cc else plain).append(program)
    print('14 programs including a changed file: ' + (' '.join(plain + macro) or 'none'))
    if macro:
        print('  of them, reading the annotations inside macro bodies (-CC): '
              + ' '.join(macro) + '; an annotation added to a macro of a changed file '
              'can change their Eva pins')


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
