#!/usr/bin/env python3
"""
size.py -- Canon-C's code size and build cost, header by header (VERIFY-070).

Canon-C is header-only, and almost every function is `static inline`: what
a program pays in flash depends on what it calls, and inlining blurs it.
size.py measures each header's own functions, all of them, as an upper
bound that is stable and comparable:

- each header is compiled alone, for a bare-metal target, with
  -fkeep-inline-functions (GCC keeps a body of every inline function, even
  one it inlines everywhere) and -ffunction-sections, and its symbols are
  read with nm -S -l;
- a function counts for the header that defines it: the header itself, or
  its module's fragments (_impl.h, _mangle.h, _decl.h, _defn.h), read from
  the debug information. One type is shared: Result<_Bool, Error>, which
  eight headers define, each guarded so that a program gets it once
  (MISRA-TOOL-006); it is a row of its own;
- a generic module (vec, deque, stack, queue, array, option, result, the
  hashmap, dynvec, smallvec, and each algo/ module's typed functions) has
  no code until it is instantiated: it is instantiated for u32 (the
  hashmap for a u64 key and an int value), and its figure adds every
  function the instantiation adds, found as the difference from the same
  unit without it. A container built on another (stack on vec, queue on
  deque, smallvec on vec) counts what it adds to an instantiated base;
- the separate-compilation forms (_decl.h, _defn.h) give the same functions
  as the header-only form, and are not measured apart.

Three targets (Cortex-M0+ and Cortex-M4 with newlib, RISC-V RV32IMAC with
picolibc), two levels (-Os, -O2) and two contract settings: as shipped for
release (NDEBUG: require() on, ensure() off) and checks off
(CANON_NO_REQUIRE with NDEBUG: both off, the setting for code whose
contracts are proved). The difference between the two is what the
run-time checks cost.

Build cost, with the host's gcc: the lines of Canon-C code the compiler
reads for each header alone, after preprocessing (exact), and the time to
compile it at -O2 (measured, so it varies from run to run: reported, not
pinned).

    python3 tools/size/size.py run [OUT]   measure; tables to stdout, CSV files to OUT
                                           (default size_out), one notice per target

Report-only (VERIFY-070's run 1). A target whose compiler is not installed
is skipped, and said so.
"""
import collections
import concurrent.futures
import csv
import glob
import os
import re
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
INC = ['-I', '.', '-I', 'core/primitives', '-I', 'core', '-I', 'semantics',
       '-I', 'data', '-I', 'algo', '-I', 'util']
FLAGS = ['-std=c99', '-g', '-fkeep-inline-functions', '-ffunction-sections',
         '-fdata-sections', '-c']

TARGETS = [
    ('cortex-m0plus', 'arm-none-eabi-', ['-mcpu=cortex-m0plus', '-mthumb']),
    ('cortex-m4', 'arm-none-eabi-',
     ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=hard', '-mfpu=fpv4-sp-d16']),
    ('rv32imac', 'riscv64-unknown-elf-',
     ['--specs=picolibc.specs', '-march=rv32imac', '-mabi=ilp32']),
]
LEVELS = ['-Os', '-O2']
CHECKS = [('release', ['-DNDEBUG']), ('checks-off', ['-DCANON_NO_REQUIRE', '-DNDEBUG'])]

HASHMAP_PRELUDE = '''#include "core/primitives/types.h"
#include <stdbool.h>
u64 canon_tu_hash(const u64* key, void* ctx);
bool canon_tu_eq(const u64* a, const u64* b, void* ctx);
#define HASHMAP_KEY_TYPE u64
#define HASHMAP_VAL_TYPE int
#define HASHMAP_HASH_FN canon_tu_hash
#define HASHMAP_EQ_FN canon_tu_eq
'''
VEC_U32 = '''#include "semantics/option/option.h"
#include "data/vec/vec.h"
CANON_OPTION(u32)
DEFINE_VEC(static inline, u32)
'''

# Generic modules: (base, instantiation). The unit measured is base +
# instantiation; the module's functions are those the instantiation adds.
GENERIC = {
    'semantics/option/option.h': (
        '#include "semantics/option/option.h"\n', 'CANON_OPTION(u32)\n'),
    'semantics/result/result.h': (
        '#include "semantics/error.h"\n#include "semantics/result/result.h"\n',
        'CANON_RESULT(u32, Error)\n'),
    'data/vec/vec.h': (
        '#include "semantics/option/option.h"\n#include "data/vec/vec.h"\nCANON_OPTION(u32)\n',
        'DEFINE_VEC(static inline, u32)\n'),
    'data/vec/vec_range.h': (
        VEC_U32 + '#include "data/vec/vec_range.h"\n',
        'DEFINE_VEC_RANGE(static inline, u32)\n'),
    'data/vec/vec_fmt.h': (
        VEC_U32 + '#include "data/vec/vec_fmt.h"\n',
        'DEFINE_VEC_FMT(static inline, u32)\n'),
    'data/deque/deque.h': (
        '#include "semantics/option/option.h"\n#include "data/deque/deque.h"\nCANON_OPTION(u32)\n',
        'DEFINE_DEQUE(static inline, u32)\n'),
    'data/stack.h': (
        VEC_U32 + '#include "data/stack.h"\n', 'DEFINE_STACK(static inline, u32)\n'),
    'data/queue.h': (
        '#include "semantics/option/option.h"\n#include "data/deque/deque.h"\n'
        '#include "data/queue.h"\nCANON_OPTION(u32)\nDEFINE_DEQUE(static inline, u32)\n',
        'DEFINE_QUEUE(static inline, u32)\n'),
    'data/array.h': (
        '#include "core/slice.h"\n#include "semantics/option/option.h"\n'
        '#include "data/array.h"\nCANON_OPTION(u32)\nDEFINE_SLICE(u32)\n',
        'DEFINE_ARRAY(u32, 8)\n'),
    'data/convenience/dynvec.h': (
        '#include "data/convenience/dynvec.h"\n', 'DEFINE_DYNVEC(u32)\n'),
    'data/convenience/smallvec.h': (
        VEC_U32 + '#include "core/arena.h"\n#include "data/convenience/smallvec.h"\n',
        'DEFINE_SMALLVEC(u32, 4)\n'),
    'data/hashmap/hashmap.h': (
        HASHMAP_PRELUDE, '#include "data/hashmap/hashmap.h"\n'),
    'data/hashmap/hashmap_range.h': (
        HASHMAP_PRELUDE + '#include "data/hashmap/hashmap.h"\n'
        '#include "test/support/hashmap_vecs.h"\n',
        '#include "data/hashmap/hashmap_range.h"\n'),
    'data/hashmap/hashmap_fmt.h': (
        HASHMAP_PRELUDE + '#include "data/hashmap/hashmap.h"\n#include "data/stringbuf.h"\n'
        'bool canon_tu_fmt_key(StringBuf* sb, const u64* k);\n'
        'bool canon_tu_fmt_val(StringBuf* sb, const int* v);\n'
        '#define HASHMAP_FMT_KEY_FN canon_tu_fmt_key\n#define HASHMAP_FMT_VAL_FN canon_tu_fmt_val\n',
        '#include "data/hashmap/hashmap_fmt.h"\n'),
}
SLICE_U32 = ('#include "core/slice.h"\n#include "semantics/option/option.h"\n'
             'CANON_OPTION(u32)\nDEFINE_SLICE(u32)\n')
for _m in ('any_all', 'filter', 'find', 'fold', 'reverse', 'search', 'sort', 'unique'):
    GENERIC[f'algo/{_m}/{_m}.h'] = (SLICE_U32 + f'#include "algo/{_m}/{_m}.h"\n',
                                    f'DEFINE_ALGO_{_m.upper()}(u32)\n')
GENERIC['algo/map/map.h'] = (SLICE_U32 + '#include "algo/map/map.h"\n',
                             'DEFINE_ALGO_MAP(u32, u32)\n')
SHARED = 'result__Bool_Error'      # its functions' prefix: a row of its own
FRAGMENTS = ('_impl', '_mangle', '_decl', '_defn')
TAIL = 'typedef int canon_tu_not_empty_;\n'


def headers():
    """The headers measured: each module's header-only form, and the
    generic modules' units."""
    found = []
    for d in ('core', 'semantics', 'data', 'algo', 'util'):
        for h in glob.glob(os.path.join(ROOT, d, '**', '*.h'), recursive=True):
            r = os.path.relpath(h, ROOT).replace(os.sep, '/')
            stem = os.path.basename(r)[:-2]
            if any(stem.endswith(f) for f in FRAGMENTS):
                continue
            if r == 'util/time.h':        # needs an OS clock (README, Tier 2)
                continue
            found.append(r)
    return sorted(set(found) | set(GENERIC))


def owns(header, path):
    """Whether a function defined in path belongs to header's module."""
    if path == header:
        return True
    d, b = os.path.split(header)
    stem = b[:-2]
    pd, pb = os.path.split(path)
    return pd == d and any(pb == stem + f + '.h' for f in FRAGMENTS)


def units(header):
    """(unit, base) source texts; base is None for a header that is not generic."""
    if header in GENERIC:
        base, inst = GENERIC[header]
        return base + inst + TAIL, base + TAIL
    return f'#include "{header}"\n' + TAIL, None


def compile_unit(cc, flags, text, tmp, name):
    src = os.path.join(tmp, name + '.c')
    obj = os.path.join(tmp, name + '.o')
    with open(src, 'w') as f:
        f.write(text)
    p = subprocess.run([cc] + flags + INC + [src, '-o', obj], cwd=ROOT,
                       capture_output=True, text=True)
    return (obj if p.returncode == 0 else None), p.stderr, src


def functions(nm, obj, src):
    """{name: (bytes, file)} for the function symbols of obj."""
    out = subprocess.run([nm, '-S', '-l', '--defined-only', obj],
                         capture_output=True, text=True).stdout
    fns = {}
    for line in out.splitlines():
        head, _, where = line.partition('\t')
        parts = head.split()
        if len(parts) != 4 or parts[2] not in ('t', 'T'):
            continue
        path = where.rsplit(':', 1)[0] if where else ''
        if path == src:
            path = '<unit>'
        elif path:
            path = os.path.relpath(os.path.normpath(os.path.join(ROOT, path)), ROOT)
            path = path.replace(os.sep, '/')
        fns[parts[3]] = (int(parts[1], 16), path)
    return fns


def own_functions(header, fns, base_fns):
    """The functions of fns that are header's: defined in its module's
    files, or written into the unit by its instantiation (the unit's
    functions that its base does not have); the shared type's apart."""
    own, shared = {}, {}
    for name, (size, path) in fns.items():
        if name.startswith(SHARED + '_'):
            shared[name] = size
        elif owns(header, path) or (path == '<unit>' and base_fns is not None
                                    and name not in base_fns):
            own[name] = size
    return own, shared


def measure_header(cc, nm, flags, h, tmp, k):
    """(own, shared) for header h; k names its files in tmp."""
    unit, base = units(h)
    obj, err, src = compile_unit(cc, flags, unit, tmp, f'unit{k}')
    if obj is None:
        raise SystemExit(f'FAIL size {cc} {" ".join(flags)} {h}:\n{err[:2000]}')
    base_fns = None
    if base is not None:
        bobj, err, bsrc = compile_unit(cc, flags, base, tmp, f'base{k}')
        if bobj is None:
            raise SystemExit(f'FAIL size {cc} {" ".join(flags)} {h} (base):\n{err[:2000]}')
        base_fns = functions(nm, bobj, bsrc)
    return own_functions(h, functions(nm, obj, src), base_fns)


def measure_size(out_dir, tmp):
    rows, summary, notes, skipped = [], [], [], []
    hs = headers()
    for tname, prefix, tflags in TARGETS:
        cc, nm = prefix + 'gcc', prefix + 'nm'
        if subprocess.run(['which', cc], capture_output=True).returncode:
            skipped.append(tname)
            print(f'size: {tname}: {cc} not installed, skipped')
            continue
        totals = {}
        fn_per_header = {}
        for level in LEVELS:
            for cname, cflags in CHECKS:
                flags = tflags + [level] + FLAGS + cflags
                total = 0
                shared_row = None
                with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 2) as pool:
                    results = list(pool.map(
                        lambda kh: measure_header(cc, nm, flags, kh[1], tmp, kh[0]),
                        enumerate(hs)))
                first = (level, cname) == (LEVELS[0], CHECKS[0][0])
                for h, (own, shared) in zip(hs, results):
                    if shared_row is None and shared:
                        shared_row = shared
                    size = sum(own.values())
                    total += size
                    summary.append((tname, level, cname, h, len(own), size))
                    for name, b in sorted(own.items()):
                        rows.append((tname, level, cname, h, name, b))
                    if first:
                        fn_per_header[h] = len(own)
                row = '(shared) ' + SHARED
                shared_row = shared_row or {}
                total += sum(shared_row.values())
                summary.append((tname, level, cname, row, len(shared_row), sum(shared_row.values())))
                for name, b in sorted(shared_row.items()):
                    rows.append((tname, level, cname, row, name, b))
                if first:
                    fn_per_header[row] = len(shared_row)
                totals[(level, cname)] = total
        with_code = sum(1 for n in fn_per_header.values() if n)
        nfun = sum(fn_per_header.values())
        notes.append(f'{tname}: {with_code} headers with code, {nfun} functions at '
                     f'{LEVELS[0]} as shipped; '
                     + '; '.join(f'{lv} {totals[(lv, "release")]} bytes as shipped, '
                                 f'{totals[(lv, "checks-off")]} with checks off' for lv in LEVELS))
    with open(os.path.join(out_dir, 'size_functions.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['target', 'level', 'checks', 'header', 'function', 'bytes'])
        w.writerows(rows)
    with open(os.path.join(out_dir, 'size_headers.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['target', 'level', 'checks', 'header', 'functions', 'bytes'])
        w.writerows(summary)
    return summary, notes, skipped


def preprocessed_lines(text, tmp):
    """Lines of Canon-C code in the unit after preprocessing, with the host's gcc."""
    src = os.path.join(tmp, 'pp.c')
    with open(src, 'w') as f:
        f.write(text)
    p = subprocess.run(['gcc', '-std=c99', '-E'] + INC + [src], cwd=ROOT,
                       capture_output=True, text=True)
    if p.returncode:
        return None
    current, n = '', 0
    for line in p.stdout.split('\n'):
        m = re.match(r'# \d+ "([^"]*)"', line)
        if m:
            current = m.group(1)
            continue
        if not line.strip():
            continue
        if current == src:
            n += 1
            continue
        path = os.path.normpath(os.path.join(ROOT, current))
        if path.startswith(ROOT + os.sep):
            n += 1
    return n


def measure_build(out_dir, tmp):
    rows = []
    for h in headers():
        unit, _ = units(h)
        lines = preprocessed_lines(unit, tmp)
        src = os.path.join(tmp, 'bt.c')
        with open(src, 'w') as f:
            f.write(unit)
        start = time.time()
        p = subprocess.run(['gcc', '-std=c99', '-O2', '-c'] + INC + [src, '-o', os.path.join(tmp, 'bt.o')],
                           cwd=ROOT, capture_output=True, text=True)
        secs = time.time() - start
        if lines is None or p.returncode:
            print(f'FAIL build {h}:\n{p.stderr[:2000]}')
            sys.exit(1)
        rows.append((h, lines, secs))
    with open(os.path.join(out_dir, 'build_headers.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['header', 'canon_c_lines_after_preprocessing', 'seconds_at_O2'])
        w.writerows((h, n, f'{s:.3f}') for h, n, s in rows)
    return rows


def run(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='canon_size_') as tmp:
        summary, notes, skipped = measure_size(out_dir, tmp)
        build = measure_build(out_dir, tmp)
    # Tables: one row per header, one column per target, level and setting.
    cols = sorted({(t, lv, cn) for t, lv, cn, *_ in summary},
                  key=lambda k: ([x[0] for x in TARGETS].index(k[0]), LEVELS.index(k[1]),
                                 [c[0] for c in CHECKS].index(k[2])))
    table = collections.defaultdict(dict)
    nfun = {}
    for t, lv, cn, h, n, s in summary:
        table[h][(t, lv, cn)] = s
        nfun[h] = n
    print('Code size: each header\'s own functions, bytes (functions at the first column\'s setting)')
    print('header'.ljust(36) + ' fns ' + ' '.join(f'{t[:6]}{lv}{"r" if cn == "release" else "x"}'.rjust(11)
                                                 for t, lv, cn in cols))
    for h in sorted(table):
        print(h.ljust(36) + f'{nfun[h]:4d} ' + ' '.join(f'{table[h].get(c, 0):11d}' for c in cols))
    print('(r: as shipped, NDEBUG; x: checks off, CANON_NO_REQUIRE and NDEBUG)')
    print()
    print('Build cost: each header alone, with the host gcc')
    for h, n, s in build:
        print(f'{h.ljust(36)} {n:7d} lines  {s:6.2f} s')
    total_lines = sum(n for _, n, _ in build)
    total_secs = sum(s for _, _, s in build)
    biggest = max(build, key=lambda r: r[1])
    slowest = max(build, key=lambda r: r[2])
    for note in notes:
        print(f'::notice title=size::{note}')
    if skipped:
        print(f'::notice title=size::not measured, compiler not installed: {", ".join(skipped)}')
    print(f'::notice title=build cost::{len(build)} headers alone: {total_lines} lines of '
          f'Canon-C after preprocessing, the most {biggest[1]} ({biggest[0]}); compiled at -O2 in '
          f'{total_secs:.1f} s in all, the slowest {slowest[2]:.2f} s ({slowest[0]}), times reported only')
    return 0


def main(argv):
    if len(argv) in (2, 3) and argv[1] == 'run':
        return run(argv[2] if len(argv) == 3 else 'size_out')
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
