#!/usr/bin/env python3
"""
size.py -- Canon-C's code size and build cost, header by header (VERIFY-070).

Canon-C is header-only, and almost every function is `static inline`: what
a program pays in flash depends on what it calls, and inlining blurs it.
size.py measures each header's own functions, all of them, as an upper
bound that is stable and comparable:

- each header is compiled alone, for a bare-metal target, with
  -fkeep-inline-functions (GCC keeps a body of every inline function, even
  one it inlines everywhere) and -ffunction-sections; each function's size
  is read from the symbol table (nm -S), and the file that defines it from
  the debug information (readelf: DW_AT_decl_file);
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

Build cost, with the host's gcc: the tokens of Canon-C code the compiler
reads for each header alone, after preprocessing (exact; tokens, not
lines, since a macro's expansion is one line however long), and the time
to compile it at -O2 (measured, so it varies from run to run: reported,
not pinned).

    python3 tools/size/size.py run [OUT]   measure, and compare with the pins; tables
                                           to stdout, CSV files to OUT (default size_out)
    python3 tools/size/size.py pin DIR     write pins.txt from a run's CSV files in DIR
    python3 tools/size/size.py check       pins.txt is well formed, its headers exist
    python3 tools/size/size.py table       docs/code-size.md's tables, from the pins

tools/size/pins.txt holds, for each header, its functions at the first
setting (Cortex-M0+, -Os, as shipped), its bytes at each of the twelve
settings, and its tokens after preprocessing. run compares what it
measures with them, figure by figure: report-only in VERIFY-070's run 2
(ENFORCE); its run 3 makes a difference fail the job. A figure changes
only with the change that moves it, pinned again with `pin` from that
change's run. A target whose compiler is not installed is skipped, and
said so; its figures are then not compared.
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
SHARED_ROW = 'shared:' + SHARED
PINS = os.path.join(HERE, 'pins.txt')
ENFORCE = False   # VERIFY-070 run 2: report-only; run 3 sets it
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


def decl_paths(readelf, obj):
    """{function: path} from the debug information: each function's
    DW_AT_decl_file, the file its definition is written in (for a function
    out of line, through DW_AT_abstract_origin), read with readelf. A
    function's first instruction may come from another header's inlined
    code, so the line table, which nm -l reads, can name the wrong file
    (VERIFY-070 run 2)."""
    out = subprocess.run([readelf, '--debug-dump=info,line', obj],
                         capture_output=True, text=True).stdout
    dirs, files, mode = {0: ROOT}, {}, None
    dies, cur = {}, None
    for line in out.split('\n'):
        s = line.strip()
        if s.startswith('The Directory Table'):
            mode = 'dir'
            continue
        if s.startswith('The File Name Table'):
            mode = 'file'
            continue
        if mode and (not s or s.startswith('Line Number Statements')):
            if not s and mode == 'file' and files:
                mode = None
            elif s.startswith('Line Number Statements'):
                mode = None
            continue
        if mode in ('dir', 'file'):
            cells = s.split('\t')
            if not cells[0].isdigit():
                continue
            value = cells[-1].split('): ', 1)[-1] if '): ' in cells[-1] else cells[-1]
            if mode == 'dir':
                dirs[int(cells[0])] = value
            elif len(cells) >= 3 and cells[1].isdigit():
                files[int(cells[0])] = (int(cells[1]), value)
            continue
        m = re.match(r'<(\d+)><([0-9a-f]+)>: Abbrev Number: \d+ \((DW_TAG_\w+)\)', s)
        if m:
            cur = {'tag': m.group(3)}
            dies[int(m.group(2), 16)] = cur
            continue
        m = re.match(r'<[0-9a-f]+>\s+(DW_AT_\w+)\s*: (.*)$', s)
        if m and cur is not None:
            cur[m.group(1)] = m.group(2)
    paths = {}
    for die in dies.values():
        # A function's name and file are on its entry, or, for a body out of
        # line, on the entry its DW_AT_abstract_origin names; a static
        # function's name is one in a translation unit.
        if die['tag'] != 'DW_TAG_subprogram':
            continue
        src = die
        if 'DW_AT_abstract_origin' in die:
            o = re.search(r'<0x([0-9a-f]+)>', die['DW_AT_abstract_origin'])
            src = dies.get(int(o.group(1), 16), {}) if o else {}
        name = src.get('DW_AT_name', '').split('): ')[-1].strip()
        fno = src.get('DW_AT_decl_file', '').split()[0] if src.get('DW_AT_decl_file') else ''
        if not name or not fno.isdigit() or int(fno) not in files:
            continue
        d, f = files[int(fno)]
        base = dirs.get(d, ROOT)
        path = f if os.path.isabs(f) else os.path.join(base if os.path.isabs(base)
                                                      else os.path.join(ROOT, base), f)
        paths[name] = os.path.normpath(path)
    return paths


def functions(nm, obj, src):
    """{name: (bytes, file)} for the function symbols of obj: the size from
    the symbol table, the file from the debug information."""
    out = subprocess.run([nm, '-S', '--defined-only', obj],
                         capture_output=True, text=True).stdout
    decl = decl_paths(nm[:-2] + 'readelf', obj)
    fns = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) != 4 or parts[2] not in ('t', 'T'):
            continue
        # A clone GCC makes (name.part.0, name.isra.0, ...) is its function's.
        path = decl.get(parts[3]) or decl.get(parts[3].split('.')[0], '')
        if path == os.path.normpath(src):
            path = '<unit>'
        elif path:
            path = os.path.relpath(path, ROOT).replace(os.sep, '/')
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
                row = SHARED_ROW
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


# C99's tokens (6.4): identifiers and keywords, numbers, character and
# string literals, and punctuators, the longest first.
TOKEN = re.compile(r'[A-Za-z_]\w*|\.?\d(?:[eEpP][+-]|[\w.])*|"(?:\\.|[^"\\])*"'
                   r"|'(?:\\.|[^'\\])*'"
                   r'|\.\.\.|<<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||##'
                   r'|[-+*/%&|^]=|\S')


def preprocessed_tokens(text, tmp):
    """Tokens of Canon-C code in the unit after preprocessing, with the host's gcc."""
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
            n += len(TOKEN.findall(line))
            continue
        path = os.path.normpath(os.path.join(ROOT, current))
        if path.startswith(ROOT + os.sep):
            n += len(TOKEN.findall(line))
    return n


def measure_build(out_dir, tmp):
    rows = []
    for h in headers():
        unit, _ = units(h)
        lines = preprocessed_tokens(unit, tmp)
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
        w.writerow(['header', 'canon_c_tokens_after_preprocessing', 'seconds_at_O2'])
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
        if (t, lv, cn) == cols[0]:
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
        print(f'{h.ljust(36)} {n:7d} tokens  {s:6.2f} s')
    total_lines = sum(n for _, n, _ in build)
    total_secs = sum(s for _, _, s in build)
    biggest = max(build, key=lambda r: r[1])
    slowest = max(build, key=lambda r: r[2])
    for note in notes:
        print(f'::notice title=size::{note}')
    if skipped:
        print(f'::notice title=size::not measured, compiler not installed: {", ".join(skipped)}')
    print(f'::notice title=build cost::{len(build)} headers alone: {total_lines} tokens of '
          f'Canon-C after preprocessing, the most {biggest[1]} ({biggest[0]}); compiled at -O2 in '
          f'{total_secs:.1f} s in all, the slowest {slowest[2]:.2f} s ({slowest[0]}), times reported only')
    cur = figures([(t, lv, cn, h, n, s) for t, lv, cn, h, n, s in summary],
                  {h: n for h, n, _ in build})
    return compare(cur, skipped)


# ---- the pins --------------------------------------------------------------

def columns():
    return [f'{t}/{lv}/{cn}' for t, _, _ in TARGETS for lv in LEVELS for cn, _ in CHECKS]


def figures(summary, lines):
    """{header: {column: value}}: functions at the first setting, bytes at
    each setting, tokens after preprocessing ('-' for the shared row)."""
    first = columns()[0]
    cur = collections.defaultdict(dict)
    for t, lv, cn, h, n, s in summary:
        col = f'{t}/{lv}/{cn}'
        cur[h][col] = int(s)
        if col == first:
            cur[h]['functions'] = int(n)
    for h in cur:
        cur[h]['tokens'] = lines.get(h, '-')
    return cur


def read_pins():
    """({header: {column: value}}, problems)."""
    pins, problems, cols = {}, [], None
    if not os.path.exists(PINS):
        return pins, ['tools/size/pins.txt: missing']
    with open(PINS) as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            cells = text.split()
            if cells[0] == 'header':
                cols = cells[1:]
                if cols != ['functions'] + columns() + ['tokens']:
                    problems.append(f'pins.txt:{n}: the columns are not size.py\'s')
                continue
            if cols is None or len(cells) != len(cols) + 1:
                problems.append(f'pins.txt:{n}: not a row of the columns: {line.strip()}')
                continue
            row = {}
            for c, v in zip(cols, cells[1:]):
                if v.isdigit():
                    row[c] = int(v)
                elif v == '-' and c == 'tokens':
                    row[c] = '-'
                else:
                    problems.append(f'pins.txt:{n}: {c} is not a number: {v}')
            if cells[0] in pins:
                problems.append(f'pins.txt:{n}: {cells[0]} pinned twice')
            pins[cells[0]] = row
    return pins, problems


def write_pins(cur):
    cols = ['functions'] + columns() + ['tokens']
    width = max(len(h) for h in cur)
    with open(PINS, 'w') as out:
        out.write('# Canon-C\'s code size and build cost, header by header (VERIFY-070):\n'
                  '# each header\'s own functions at the first setting, its bytes at each\n'
                  '# target/level/setting (release: NDEBUG, as shipped; checks-off:\n'
                  '# CANON_NO_REQUIRE and NDEBUG), and the tokens of Canon-C it brings after\n'
                  '# preprocessing. Written by `size.py pin` from a run\'s CSV files; compared\n'
                  '# by `size.py run` in the embedded job. Toolchains: the ubuntu-24.04\n'
                  '# runner\'s packages (docs/code-size.md).\n')
        out.write(' '.join(['header'.ljust(width)] + cols) + '\n')
        for h in sorted(cur):
            out.write(' '.join([h.ljust(width)] + [str(cur[h].get(c, '-')).rjust(len(c)) for c in cols])
                      + '\n')


def compare(cur, skipped):
    pins, problems = read_pins()
    measured = [c for c in columns() if c.split('/')[0] not in skipped]
    diffs = []
    for h in sorted(set(cur) | set(pins)):
        if h not in pins:
            diffs.append(f'{h}: measured, no pin')
            continue
        if h not in cur:
            diffs.append(f'{h}: pinned, not measured')
            continue
        for c in ['functions'] + measured + ['tokens']:
            if c == 'functions' and columns()[0] not in measured:
                continue
            if cur[h].get(c) != pins[h].get(c):
                diffs.append(f'{h} {c}: {cur[h].get(c)}, pinned {pins[h].get(c)}')
    for p in problems:
        print(p)
    n = len(cur) * (len(measured) + 2)
    if diffs or problems:
        print(f'Not as pinned: {len(diffs)} difference(s)')
        for d in diffs:
            print('  ' + d)
        head = (f'not as pinned: {len(diffs)} difference(s)'
                + (f', {len(problems)} problem(s) in pins.txt' if problems else '')
                + ': ' + '; '.join(diffs[:12]) + (' …' if len(diffs) > 12 else ''))
    else:
        head = (f'as pinned: {len(cur)} rows, {n} figures'
                + (f' ({", ".join(skipped)} not measured)' if skipped else ''))
    print(f'::notice title=size pins::{head}')
    return 1 if (ENFORCE and (diffs or problems)) else 0


def pin(src):
    summary = []
    with open(os.path.join(src, 'size_headers.csv')) as f:
        for r in csv.DictReader(f):
            summary.append((r['target'], r['level'], r['checks'], r['header'],
                            r['functions'], r['bytes']))
    with open(os.path.join(src, 'build_headers.csv')) as f:
        lines = {r['header']: int(r['canon_c_tokens_after_preprocessing']) for r in csv.DictReader(f)}
    cur = figures(summary, lines)
    missing = [c for c in columns() if not any(c in row for row in cur.values())]
    if missing:
        print(f'size.py pin: no figures for {", ".join(missing)}; pin from a run that measures them')
        return 1
    write_pins(cur)
    print(f'wrote tools/size/pins.txt: {len(cur)} rows')
    return 0


def check():
    pins, problems = read_pins()
    for h in pins:
        if h != SHARED_ROW and not os.path.exists(os.path.join(ROOT, h)):
            problems.append(f'{h}: no such header')
    for p in problems:
        print(p)
    print(f'size pins: {len(pins)} rows; {len(problems)} problem(s)')
    return 1 if problems else 0


def table():
    """docs/code-size.md's tables, as Markdown, from the pins."""
    pins, problems = read_pins()
    if problems:
        for p in problems:
            print(p)
        return 1
    tot = collections.Counter()
    for row in pins.values():
        for c in columns():
            tot[c] += row[c]
    print('| target | -Os, as shipped | -Os, checks off | -O2, as shipped | -O2, checks off |')
    print('|--------|----------------:|----------------:|----------------:|----------------:|')
    for t, _, _ in TARGETS:
        print(f'| {t} | ' + ' | '.join(f'{tot[f"{t}/{lv}/{cn}"]:,}' for lv in LEVELS
                                        for cn, _ in CHECKS) + ' |')
    print()
    print('| header | functions | Cortex-M0+ | Cortex-M4 | RV32 | Cortex-M4, checks off '
          '| Cortex-M4, -O2 | tokens |')
    print('|--------|----------:|-----------:|----------:|-----:|---------------------:'
          '|---------------:|------:|')
    for h in sorted(pins, key=lambda k: (k == SHARED_ROW, k)):
        r = pins[h]
        name = 'Result<_Bool, Error>, shared' if h == SHARED_ROW else f'`{h}`'
        print(f'| {name} | {r["functions"]} | {r["cortex-m0plus/-Os/release"]:,} | '
              f'{r["cortex-m4/-Os/release"]:,} | {r["rv32imac/-Os/release"]:,} | '
              f'{r["cortex-m4/-Os/checks-off"]:,} | {r["cortex-m4/-O2/release"]:,} | '
              + (f'{r["tokens"]:,}' if r['tokens'] != '-' else '–') + ' |')
    return 0


def main(argv):
    if len(argv) in (2, 3) and argv[1] == 'run':
        return run(argv[2] if len(argv) == 3 else 'size_out')
    if len(argv) == 3 and argv[1] == 'pin':
        return pin(argv[2])
    if len(argv) == 2 and argv[1] == 'check':
        return check()
    if len(argv) == 2 and argv[1] == 'table':
        return table()
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
