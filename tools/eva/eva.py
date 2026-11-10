#!/usr/bin/env python3
"""
eva.py -- Eva, Frama-C's abstract interpreter, on small programs that use
Canon-C (VERIFY-066).

WP proves each function against its contract: given its preconditions, its
body has no runtime error and ensures its postconditions. Whether a program
that calls the library meets those preconditions is the program's question,
and WP does not answer it for a program it is not run on. Eva does: from
main, it computes the values the program can take, and reports each place
where a runtime error may happen (an alarm) and each call whose precondition
may not hold. A precondition Eva finds invalid is one the call breaks on every
path Eva considered; unknown means Eva could not decide.

tools/eva/programs/ holds one program per verified unit (the units of
tools/agreement/agreement.py). Each includes its unit's file, which it is
compiled with the unit's own flags, as the unit's proof job compiles it, and
calls the unit's functions as a user would: with values the contracts allow,
and with the values the functions are written to refuse. It checks what each
call returns (CHECK) and marks its end (END, eva_program.h).

    eva.py list
        The JSON list of the programs, for the workflow's matrix.

    eva.py native [PROGRAM...]
        Compiles each program with the C compiler (CC, default gcc: -std=c11
        -Wall -Wextra -Werror, AddressSanitizer and UndefinedBehaviorSanitizer)
        and runs it. Each must compile and return 0.

    eva.py run MODEL [PROGRAM...]
        Eva on each program at the machine model MODEL (MODELS: the ten the
        machine-model proofs use), into eva_out/MODEL/: Eva's output (.log),
        the property statuses (.csv, Frama-C's Report plugin) and a summary
        (.json). EVA_SECONDS limits each run (default 1200). EVA_FRAMAC_ARGS
        adds options (Frama-C 25 needs -c11, which 29 does not).

    eva.py report [DIR]
        Reads the summaries under DIR (default eva_out) and writes the
        notices: one line per program and model, and the totals.

    eva.py check
        For 8 · Records: every unit of agreement.py has a program here and
        every program a unit; each program includes its unit's file; the
        models are proofs-machdeps.yml's and x86_64, and eva.yml's matrix
        lists them.

Report-only until ENFORCE is set (VERIFY-066, run 3).
"""
import csv
import glob
import json
import os
import re
import shlex
import subprocess
import sys
import time

ENFORCE = False   # VERIFY-066: report-only until its third run

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PROGRAMS_DIR = os.path.join(HERE, 'programs')
sys.path.insert(0, os.path.join(ROOT, 'tools', 'agreement'))
sys.dont_write_bytecode = True
import agreement  # noqa: E402  (the units, their files and their flags)

# Each program and the unit whose file and flags it uses.
PROGRAMS = {
    'checked': 'frama-c-checked',
    'bits': 'frama-c-bits',
    'compare': 'frama-c-compare',
    'ptr': 'frama-c-ptr',
    'slice': 'frama-c-slice',
    'memory': 'frama-c-memory',
    'arena': 'frama-c-arena',
    'pool': 'frama-c-pool',
    'region': 'frama-c-region',
    'lifetime': 'frama-c-lifetime',
    'error': 'frama-c-error',
    'option': 'frama-c-option',
    'result': 'frama-c-result',
    'borrow': 'frama-c-borrow',
    'diag': 'frama-c-diag',
    'vec': 'frama-c-vec',
    'deque': 'frama-c-deque',
    'bitset': 'frama-c-bitset',
    'priority_queue': 'frama-c-priority-queue',
    'range': 'frama-c-range',
    'vec_range': 'frama-c-vec-range',
    'vec_fmt': 'frama-c-vec-fmt',
    'stringbuf': 'frama-c-stringbuf',
    'stack': 'frama-c-stack',
    'array': 'frama-c-array',
    'hashmap': 'frama-c-hashmap',
}

# The machine models: x86_64, Frama-C's default, at which the proof jobs
# prove every unit, and the nine at which proofs-machdeps.yml proves core/.
MODELS = {
    'x86_64': ['-machdep', 'x86_64'],
    'cortex_m4': ['-machdep', 'tools/machdeps/machdep_cortex_m4.yaml', '-enums', 'gcc-short-enums'],
    'rv32_ilp32': ['-machdep', 'tools/machdeps/machdep_rv32_ilp32.yaml'],
    'rv64_lp64': ['-machdep', 'tools/machdeps/machdep_rv64_lp64.yaml'],
    'arm_linux_gnueabihf': ['-machdep', 'tools/machdeps/machdep_arm_linux_gnueabihf.yaml'],
    'rv64_linux_gnu': ['-machdep', 'tools/machdeps/machdep_rv64_linux_gnu.yaml'],
    'aarch64_linux_gnu': ['-machdep', 'tools/machdeps/machdep_aarch64_linux_gnu.yaml'],
    'ppc32_linux_gnu': ['-machdep', 'tools/machdeps/machdep_ppc32_linux_gnu.yaml'],
    'ppc_32': ['-machdep', 'ppc_32'],
    'msvc_x86_64': ['-machdep', 'msvc_x86_64'],
}

# Eva's options for every program. -eva-precision sets how many states Eva
# keeps apart and how far it unrolls loops; -eva-split-return full keeps the
# states a function returns apart by its return value, so that a program's
# test of a result separates what the function did on each path.
COMMON = ['-eva', '-eva-precision', '3', '-eva-split-return', 'full']

# ptr_align_padding computes an address modulo the alignment, which Eva
# cannot follow: it does not know where an object lies in memory. A program
# that includes ptr.h is analysed with the function's contract, which WP
# proves (\result < align), in place of its body.
USE_SPEC = {'core/primitives/ptr.h': ['-eva-use-spec', 'ptr_align_padding']}

# compare.c sorts a NaN, which the float comparators order last: Eva is told
# that a NaN is a value here, not an error (-warn-special-float none).
OPTIONS = {'compare': ['-warn-special-float', 'none']}

# The property kinds of contracts and annotations in Frama-C's Report: every
# other kind is an alarm, a runtime error Eva could not exclude.
CONTRACT_KINDS = ('precondition', 'postcondition', 'assigns clause', 'from clause',
                  'loop invariant', 'loop assigns', 'loop variant', 'assertion', 'check',
                  'behavior', 'complete behaviors', 'disjoint behaviors', 'terminates',
                  'exits', 'breaks', 'continues', 'returns', 'requires', 'ensures',
                  'reachability', 'code annotation', 'axiom', 'lemma', 'instance',
                  'decreases', 'allocates', 'frees', 'extended')
# The statuses Eva leaves open: 'Unknown', 'Invalid', 'Invalid or
# unreachable', and their '(under hypotheses)' forms.
OPEN = ('Unknown', 'Invalid')

# How much of Frama-C's output a run that does not finish prints.
TAIL = 80


def unit_of(program):
    return agreement.UNITS[PROGRAMS[program]]


def cpp_of(program, native=False):
    cpp = unit_of(program)[2]
    if native:
        cpp = ' '.join(w for w in shlex.split(cpp) if w != '-CC')
    return cpp + ' -I tools/eva/programs'


def source(program):
    return os.path.join('tools', 'eva', 'programs', program + '.c')


def includes(program):
    """The repository files the program includes, with Frama-C's macros."""
    cmd = (['gcc', '-x', 'c', '-std=c11', '-M', '-MG', '-D__FRAMAC__', '-D__FC_MACHDEP_X86_64']
           + shlex.split(cpp_of(program, native=True)) + [source(program)])
    out = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT).stdout
    files = set()
    for w in out.replace('\\\n', ' ').split()[1:]:
        p = os.path.normpath(w)
        if not os.path.isabs(p) and os.path.exists(os.path.join(ROOT, p)):
            files.add(p)
    return files


def eva_command(program, model, csv_path):
    extra = []
    inc = includes(program)
    for f, opts in USE_SPEC.items():
        if f in inc:
            extra += opts
    extra += OPTIONS.get(program, [])
    extra += shlex.split(os.environ.get('EVA_FRAMAC_ARGS', ''))
    return (['frama-c'] + COMMON + MODELS[model] + extra
            + [f'-cpp-extra-args={cpp_of(program)}', source(program),
               '-then', '-report-csv', csv_path])


def parse_log(text):
    """Eva's summary, and the lines the programs' markers print."""
    s = {}
    m = re.search(r'(\d+) functions? analyzed \(out of (\d+)\)', text)
    s['functions'] = [int(m.group(1)), int(m.group(2))] if m else None
    m = re.search(r'(\d+|No) alarms? generated by the analysis', text)
    s['alarms'] = (0 if m.group(1) == 'No' else int(m.group(1))) if m else None
    for kind in ('Assertions', 'Preconditions'):
        m = re.search(kind + r'\s+(\d+) valid\s+(\d+) unknown\s+(\d+) invalid', text)
        s[kind.lower()] = [int(x) for x in m.groups()] if m else None
    m = re.search(r'by the Eva analyzer:\s+(\d+) errors?\s+(\d+) warnings?', text)
    s['eva_messages'] = [int(x) for x in m.groups()] if m else None
    m = re.search(r'by the Frama-C kernel:\s+(\d+) errors?\s+(\d+) warnings?', text)
    s['kernel_messages'] = [int(x) for x in m.groups()] if m else None
    ends = re.findall(r'Frama_C_show_each_reached_end:\s*(.+)', text)
    values = set()
    for e in ends:
        e = e.strip()
        m = re.fullmatch(r'\{([^}]*)\}', e)
        if m:
            values |= {int(x) for x in m.group(1).replace(' ', '').split(';') if x}
        else:
            values.add(e)          # an interval: Eva printed a range
    s['reached_end'] = bool(ends)
    s['fails_at_end'] = sorted(values, key=str)
    s['unconfirmed_checks'] = sorted({int(x) for x in
                                      re.findall(r'Frama_C_show_each_check_failed:\s*\{(\d+)\}', text)})
    s['aborted'] = bool(re.search(r'User Error|Frama-C aborted|Plug-in \w+ aborted', text))
    return s


def parse_csv(path):
    """The properties Eva left open: alarms and preconditions at calls."""
    rows = []
    if not os.path.exists(path):
        return None
    with open(path, newline='', errors='replace') as f:
        for r in csv.DictReader(f, delimiter='\t'):
            rows.append(r)
    open_rows = []
    for r in rows:
        status = (r.get('status') or '').strip()
        kind = (r.get('property kind') or '').strip()
        fn = (r.get('function') or '').strip()
        if not status.startswith(OPEN) or fn.startswith('Frama_C_'):
            continue
        where = os.path.join(r.get('directory') or '', r.get('file') or '')
        item = {'file': os.path.normpath(where), 'line': int(r.get('line') or 0),
                'function': fn, 'kind': kind, 'status': status,
                'property': ' '.join((r.get('property') or '').split())}
        if kind.startswith('precondition of'):
            item['class'] = 'call'
        elif kind.split(' ')[0] in ('precondition', 'postcondition', 'assigns', 'from', 'loop',
                                    'assertion', 'check', 'behavior', 'complete', 'disjoint'):
            item['class'] = 'contract'
        elif kind in CONTRACT_KINDS:
            item['class'] = 'contract'
        else:
            item['class'] = 'alarm'
        open_rows.append(item)
    return open_rows


def summarize(program, model, log_text, rows, seconds, code):
    s = parse_log(log_text)
    s.update({'program': program, 'model': model, 'seconds': round(seconds, 1), 'exit': code})
    if rows is None:
        s['csv'] = False
        rows = []
    else:
        s['csv'] = True
    in_program = lambda r: r['file'].startswith('tools/eva/programs')
    s['alarm_list'] = [r for r in rows if r['class'] == 'alarm']
    s['program_calls'] = [r for r in rows if r['class'] == 'call' and in_program(r)]
    s['library_calls'] = [r for r in rows if r['class'] == 'call' and not in_program(r)]
    s['library_contracts'] = len([r for r in rows if r['class'] == 'contract'])
    return s


def line(s):
    """One line for a program at a model."""
    if s.get('aborted') or s.get('functions') is None:
        return f"{s['program']} @ {s['model']}: Eva did not finish (exit {s['exit']}, {s['seconds']} s)"
    inv = [r for r in s['program_calls'] if r['status'].startswith('Invalid')]
    unk = [r for r in s['program_calls'] if r['status'].startswith('Unknown')]
    linv = [r for r in s['library_calls'] if r['status'].startswith('Invalid')]
    lunk = [r for r in s['library_calls'] if r['status'].startswith('Unknown')]
    end = ('end reached, fails ' + ('{' + '; '.join(str(v) for v in s['fails_at_end']) + '}')
           if s['reached_end'] else 'END NOT REACHED')
    parts = [end,
             f"alarms {s['alarms']}",
             f"program calls: {len(inv)} invalid, {len(unk)} unknown",
             f"library calls: {len(linv)} invalid, {len(lunk)} unknown"]
    if s['unconfirmed_checks']:
        parts.append('checks not confirmed at lines ' + ','.join(str(x) for x in s['unconfirmed_checks']))
    if s.get('preconditions'):
        # Eva's own count of every precondition it evaluated, at every call:
        # valid / unknown / invalid.
        parts.append('preconditions ' + '/'.join(str(x) for x in s['preconditions']))
    parts.append(f"functions {s['functions'][0]}/{s['functions'][1]}")
    parts.append(f"{s['seconds']} s")
    return f"{s['program']} @ {s['model']}: " + '; '.join(parts)


def details(s):
    """The open properties, one per line, for the log."""
    out = []
    for key, title in (('alarm_list', 'alarm'), ('program_calls', 'call'), ('library_calls', 'library call')):
        for r in s.get(key, []):
            out.append(f"  {title}: {r['status']}: {r['file']}:{r['line']} {r['function']}: "
                       f"{r['kind']}: {r['property'][:300]}")
    return out


def run(model, programs):
    out_dir = os.path.join('eva_out', model)
    os.makedirs(out_dir, exist_ok=True)
    limit = float(os.environ.get('EVA_SECONDS') or 1200)
    results = []
    for program in programs:
        stem = os.path.join(out_dir, program)
        csv_path = os.path.abspath(stem + '.csv')
        if os.path.exists(csv_path):
            os.remove(csv_path)
        cmd = eva_command(program, model, csv_path)
        print('Running:', ' '.join(shlex.quote(c) for c in cmd), flush=True)
        start = time.time()
        try:
            # Frama-C reads the directory from PWD, which cwd= does not set.
            # OCAMLRUNPARAM=b: an internal error prints OCaml's backtrace.
            p = subprocess.run(cmd, cwd=ROOT, env=dict(os.environ, PWD=ROOT, OCAMLRUNPARAM='b'),
                               capture_output=True,
                               text=True, errors='replace', timeout=limit)
            text, code = p.stdout + p.stderr, p.returncode
        except subprocess.TimeoutExpired as e:
            text = (e.stdout or '') if isinstance(e.stdout, str) else ''
            text += f'\n[eva.py] stopped after {limit:.0f} s\n'
            code = None
        seconds = time.time() - start
        with open(stem + '.log', 'w') as f:
            f.write(text)
        s = summarize(program, model, text, parse_csv(csv_path), seconds, code)
        with open(stem + '.json', 'w') as f:
            json.dump(s, f, indent=1)
        print(line(s), flush=True)
        if s.get('aborted') or s.get('functions') is None:
            # A run that did not finish: the end of Frama-C's own output,
            # which says why (VERIFY-066 run 1: exit 125 on three programs).
            print(f'  -- the last {TAIL} lines of Frama-C\'s output:')
            for l in text.rstrip().split('\n')[-TAIL:]:
                print('  | ' + l)
        for d in details(s):
            print(d)
        results.append(s)
    write_notices([line(s) for s in results], f'eva {model}')
    bad = [s for s in results if s.get('aborted') or s.get('functions') is None
           or not s['reached_end'] or s['exit'] != 0]
    return 1 if (ENFORCE and bad) else 0


def write_notices(lines, title):
    """Ten notices to a file: GitHub keeps ten notices of a step."""
    for old in glob.glob('notices_*.txt'):
        os.remove(old)
    for i in range(0, len(lines), 10):
        with open(f'notices_{i // 10 + 1}.txt', 'w') as f:
            for l in lines[i:i + 10]:
                f.write(f'::notice title={title}::{l}\n')


def native(programs):
    cc = os.environ.get('CC', 'gcc')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-g',
             '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
    os.makedirs('eva_native', exist_ok=True)
    failed = []
    lines = []
    for program in programs:
        exe = os.path.abspath(os.path.join('eva_native', program))
        cmd = [cc] + flags + shlex.split(cpp_of(program, native=True)) + [source(program), '-o', exe]
        print('Running:', ' '.join(cmd), flush=True)
        c = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        if c.returncode != 0:
            print(c.stdout + c.stderr)
            failed.append(program)
            lines.append(f'{program}: does not compile with {cc}')
            continue
        r = subprocess.run([exe], cwd=ROOT, capture_output=True, text=True)
        print(r.stdout[-2000:] + r.stderr[-4000:])
        if r.returncode != 0:
            failed.append(program)
        lines.append(f'{program}: returns {r.returncode} ({cc}, ASan and UBSan)')
    summary = (f'{len(programs) - len(failed)} of {len(programs)} programs compile with {cc} '
               f'and return 0 under ASan and UBSan' + (f'; not: {", ".join(failed)}' if failed else ''))
    print(summary)
    with open('notices_1.txt', 'w') as f:
        f.write(f'::notice title=native {cc}::{summary}\n')
    return 1 if (ENFORCE and failed) else 0


def report(where):
    found = sorted(glob.glob(os.path.join(where, '**', '*.json'), recursive=True))
    results = []
    for p in found:
        with open(p) as f:
            s = json.load(f)
        if isinstance(s, dict) and 'program' in s:
            results.append(s)
    by_model = {}
    for s in results:
        by_model.setdefault(s['model'], []).append(s)
    lines = []
    for model in MODELS:
        ss = by_model.get(model, [])
        if not ss:
            lines.append(f'{model}: no results')
            continue
        reached = sum(1 for s in ss if s.get('reached_end'))
        zero = sum(1 for s in ss if s.get('reached_end') and s.get('fails_at_end') == [0])
        alarms = sum(s.get('alarms') or 0 for s in ss)
        inv = sum(1 for s in ss for r in s.get('program_calls', []) if r['status'].startswith('Invalid'))
        unk = sum(1 for s in ss for r in s.get('program_calls', []) if r['status'].startswith('Unknown'))
        linv = sum(1 for s in ss for r in s.get('library_calls', []) if r['status'].startswith('Invalid'))
        lunk = sum(1 for s in ss for r in s.get('library_calls', []) if r['status'].startswith('Unknown'))
        unfinished = [s['program'] for s in ss if s.get('aborted') or s.get('functions') is None]
        lines.append(f'{model}: {len(ss)} programs; {reached} reach their end, {zero} with every '
                     f'check confirmed; alarms {alarms}; program calls {inv} invalid, {unk} unknown; '
                     f'library calls {linv} invalid, {lunk} unknown'
                     + (f'; Eva did not finish: {", ".join(unfinished)}' if unfinished else ''))
    # The functions the programs reach, over every model: Eva's own count.
    print('\n'.join(lines))
    with open('notices_1.txt', 'w') as f:
        for l in lines:
            f.write(f'::notice title=eva total::{l}\n')
    with open('eva_summary.tsv', 'w') as f:
        f.write('program\tmodel\treached_end\tfails_at_end\talarms\tprogram_invalid\tprogram_unknown'
                '\tlibrary_invalid\tlibrary_unknown\tfunctions\tseconds\n')
        for s in sorted(results, key=lambda s: (s['program'], s['model'])):
            pc = s.get('program_calls', [])
            lc = s.get('library_calls', [])
            f.write('\t'.join(str(x) for x in (
                s['program'], s['model'], s.get('reached_end'), s.get('fails_at_end'), s.get('alarms'),
                sum(1 for r in pc if r['status'].startswith('Invalid')),
                sum(1 for r in pc if r['status'].startswith('Unknown')),
                sum(1 for r in lc if r['status'].startswith('Invalid')),
                sum(1 for r in lc if r['status'].startswith('Unknown')),
                s.get('functions'), s.get('seconds'))) + '\n')
    return 0


def check():
    problems = []
    units = set(agreement.UNITS)
    have = set(PROGRAMS.values())
    for u in sorted(units - have):
        problems.append(f'unit {u} has no program')
    for u in sorted(have - units):
        problems.append(f'program unit {u} is not a unit of agreement.py')
    files = {os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(PROGRAMS_DIR, '*.c'))}
    for p in sorted(files - set(PROGRAMS)):
        problems.append(f'tools/eva/programs/{p}.c is not in PROGRAMS')
    for p in sorted(set(PROGRAMS) - files):
        problems.append(f'program {p} has no tools/eva/programs/{p}.c')
    for p in sorted(set(PROGRAMS) & files):
        path = unit_of(p)[0]
        with open(os.path.join(ROOT, source(p))) as f:
            text = f.read()
        if f'#include "{path}"' not in text:
            problems.append(f'{source(p)} does not include its unit\'s file {path}')
    wf = os.path.join(ROOT, '.github', 'workflows', 'proofs-machdeps.yml')
    with open(wf) as f:
        text = f.read()
    theirs = {}
    for m in re.finditer(r'-\s*\{\s*name:\s*(\w+),\s*class:\s*\w+,\s*args:\s*"([^"]*)"\s*\}', text):
        theirs[m.group(1)] = m.group(2).split()
    mine = {k: v for k, v in MODELS.items() if k != 'x86_64'}
    if theirs != mine:
        problems.append(f'the models differ from proofs-machdeps.yml\'s: {sorted(theirs)} / {sorted(mine)}')
    with open(os.path.join(ROOT, '.github', 'workflows', 'eva.yml')) as f:
        text = f.read()
    m = re.search(r'model:\s*\[([^\]]*)\]', text)
    listed = [x.strip() for x in m.group(1).split(',')] if m else []
    if listed != list(MODELS):
        problems.append(f'eva.yml\'s matrix is not MODELS: {listed} / {list(MODELS)}')
    for p in problems:
        print('::error title=eva check::' + p)
    print(f'eva check: {len(PROGRAMS)} programs, {len(MODELS)} models, {len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd, args = argv[1], argv[2:]
    if cmd == 'list':
        print(json.dumps(list(PROGRAMS)))
        return 0
    if cmd == 'native':
        return native(args or list(PROGRAMS))
    if cmd == 'run':
        if not args or args[0] not in MODELS:
            print('eva.py run MODEL [PROGRAM...]; models:', ' '.join(MODELS))
            return 2
        return run(args[0], args[1:] or list(PROGRAMS))
    if cmd == 'report':
        return report(args[0] if args else 'eva_out')
    if cmd == 'check':
        return check()
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
