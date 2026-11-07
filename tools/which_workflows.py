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
"""
import glob
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
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
