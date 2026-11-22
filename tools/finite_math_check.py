#!/usr/bin/env python3
"""finite_math_check.py: finite math, refused (VERIFY-110). Run from the
repository root; 2 · Platforms' job finite-math runs it.

compare.h's float comparators order NaN, and random.h's float ranges refuse
an infinite bound, by tests that finite math lets GCC and Clang fold away
(VERIFY-102). Each of the two refuses it with an #error, unless
CANON_ALLOW_FINITE_MATH is defined. For each compiler:

  Part 1  the two headers, under -ffinite-math-only, -ffast-math and
          -Ofast: refused, each by its own #error.
  Part 2  the two, under -ffinite-math-only with CANON_ALLOW_FINITE_MATH,
          and without finite math: compiling.
  Part 3  every public header, under -ffinite-math-only: compiling, or
          refused by one of the two #errors; anything else fails.
  Part 4  every public header, under -ffinite-math-only with
          CANON_ALLOW_FINITE_MATH: compiling.

Each unit is one header and nothing else, at the project's warning flags.
The public headers are those tools/tier_check.sh classifies.

    CANON_FM_CC  the compilers, by name (gcc clang)
"""
import os
import re
import subprocess
import sys
import tempfile

WARN = ['-std=c99', '-Wall', '-Wextra', '-Wpedantic', '-Werror']
# Not -I util: under it, util/time.h's <time.h> is util/time.h itself, and
# the system's is never read (CMakeLists.txt's time_test has no util).
INC = []
for d in ('.', 'core/primitives', 'core', 'semantics', 'data', 'algo'):
    INC += ['-I', d]
GUARDED = {'core/primitives/compare.h': "compare.h's float comparators",
           'util/random.h': "random.h's float ranges"}
FINITE = ['-ffinite-math-only', '-ffast-math', '-Ofast']
ALLOW = '-DCANON_ALLOW_FINITE_MATH'


def public_headers():
    out = []
    for top in ('core', 'semantics', 'data', 'algo', 'util'):
        for d, _, files in os.walk(top):
            for f in files:
                p = os.path.join(d, f).replace(os.sep, '/')
                if not p.endswith('.h') or re.search(r'_(impl|mangle)\.h$', p):
                    continue
                if re.fullmatch(r'data/hashmap/hashmap(_decl|_defn|_range|_fmt)?\.h', p):
                    continue
                out.append(p)
    return sorted(out)


def compile_one(cc, header, flags, tmp):
    src = os.path.join(tmp, 'h.c')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(f'#include "{header}"\ntypedef int canon_tu_not_empty_;\n')
    p = subprocess.run([cc] + flags + WARN + INC + ['-c', src, '-o', src + '.o'],
                       capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def refused_by(out):
    """Which of the two #errors refused the unit, or None."""
    for h, what in GUARDED.items():
        if f'Canon-C: {what}' in out and 'CANON_ALLOW_FINITE_MATH' in out:
            return h
    return None


def first_error(out):
    m = re.search(r'error: .*', out)
    return (m.group(0) if m else out.strip().split('\n')[0] if out.strip() else '?')[:120]


def check(cc, headers, tmp):
    fails = 0
    st, out = 0, ''
    p = subprocess.run([cc, '--version'], capture_output=True, text=True)
    print(f'{cc}: {(p.stdout or p.stderr).splitlines()[0].strip()}')

    print(f'=== {cc}, Part 1: the two headers under finite math, refused ===')
    for h in GUARDED:
        for flag in FINITE:
            st, out = compile_one(cc, h, [flag], tmp)
            by = refused_by(out)
            if st != 0 and by == h:
                print(f'  refused, by its #error: {h} {flag}')
            else:
                print(f'FAIL: {h} {flag}: status {st}, {first_error(out)}'); fails += 1

    print(f'=== {cc}, Part 2: the two with {ALLOW}, and without finite math, compiling ===')
    for h in GUARDED:
        for flags in ([FINITE[0], ALLOW], []):
            st, out = compile_one(cc, h, flags, tmp)
            label = ' '.join(flags) or 'no finite math'
            if st == 0:
                print(f'  compiles: {h} ({label})')
            else:
                print(f'FAIL: {h} ({label}): {first_error(out)}'); fails += 1

    print(f'=== {cc}, Part 3: every public header under {FINITE[0]} ===')
    ok = 0
    refused = {h: 0 for h in GUARDED}
    for h in headers:
        st, out = compile_one(cc, h, [FINITE[0]], tmp)
        by = refused_by(out)
        if st == 0:
            ok += 1
        elif by is not None:
            refused[by] += 1
            print(f'  refused, by {by}: {h}')
        else:
            print(f'FAIL: {h}: neither compiles nor is refused by the #error: {first_error(out)}')
            fails += 1
    n_ref = sum(refused.values())
    print(f'  {len(headers)} headers: {ok} compile, {n_ref} refused '
          f'({", ".join(f"{os.path.basename(k)} {v}" for k, v in refused.items())})')

    print(f'=== {cc}, Part 4: every public header under {FINITE[0]} {ALLOW} ===')
    ok4 = 0
    for h in headers:
        st, out = compile_one(cc, h, [FINITE[0], ALLOW], tmp)
        if st == 0:
            ok4 += 1
        else:
            print(f'FAIL: {h} with {ALLOW}: {first_error(out)}'); fails += 1
    print(f'  {len(headers)} headers: {ok4} compile')
    line = (f'{len(headers)} headers under {FINITE[0]}: {ok} compile, {n_ref} refused by the '
            f'finite-math #error; with {ALLOW[2:]}: {ok4} compile; {fails} check(s) failed')
    return fails, line


def main():
    ccs = os.environ.get('CANON_FM_CC', 'gcc clang').split()
    headers = public_headers()
    total = 0
    with tempfile.TemporaryDirectory(prefix='canon-fm-') as tmp:
        for cc in ccs:
            fails, line = check(cc, headers, tmp)
            total += fails
            print(f'{cc}: {line}')
            if os.environ.get('GITHUB_ACTIONS'):
                print(f'::notice title=finite-math {cc}::{line}')   # VERIFY-050
    if total:
        print(f'FAIL: {total} check(s) failed')
        return 1
    print('OK')
    return 0


if __name__ == '__main__':
    sys.exit(main())
