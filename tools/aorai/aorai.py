#!/usr/bin/env python3
"""tools/aorai/aorai.py  (run from the repository root)

VERIFY-085. Three call-order rules that the library's documentation states
and that no contract states, each written as an automaton in Ya, the
language of Frama-C's Aorai plug-in (tools/aorai/README.md):

  hashmap_iteration.ya  hashmap.h: an iteration does not go on after the
                        map changed ("Do not mutate the map during
                        iteration")
  stringbuf_views.ya    stringbuf.h: a view is not read after stringbuf_close,
                        until the buffer is initialised again and a view
                        taken from it
  bitset_views.ya       bitset.h: the same of bitset_close

Aorai instruments a program with an automaton: ghost state that each call
updates, and contracts that name the calls the state allows. Eva then runs
the instrumented program as 14 · Eva runs the program it comes from, with
tools/eva/eva.py's options, at x86_64, and Report's table gives each
property its status. Aorai's properties are those that name its state
(aorai_). Each pair of a program and an automaton is a case of CASES, with
the verdict the rule gives it:

  follows  no property of Aorai's invalid or unknown, and the program
           reaches its END;
  breaks   a call in the program that violates one of Aorai's
           preconditions, and only the call marked /* refused */ in it.

Anything else is "undecided". The programs are tools/eva/programs/'s three
that use the rules' functions, and tools/aorai/programs/, two for each
rule's ways of being kept and broken; each is preprocessed as the Eva
program it names. A case not as CASES says fails the run only if ENFORCE.

    python3 tools/aorai/aorai.py check           CASES against the files
    python3 tools/aorai/aorai.py run [CASE...]   Aorai and Eva on each case
    python3 tools/aorai/aorai.py native          each program compiled and
                                                 run, and with
                                                 CANON_LIFETIME_DEBUG

`run` writes aorai_out/CASE/ (Frama-C's log, Report's table, a summary)
and notices_aorai.txt, in the directory it is run from; `native` writes
aorai_out/native/ and notices_aorai_native.txt. A program's run-time
checks (CANON_LIFETIME_DEBUG) see the views rules: built with them, a
program that breaks a views rule stops at the read, a contract violation
(abort). They do not see the iteration rule.
EVA_FRAMAC_ARGS, as for eva.py, adds kernel options (here, with Frama-C 25:
'-c11 -kernel-warn-key annot-error=active').
"""
import csv
import importlib.util
import json
import os
import re
import shlex
import subprocess
import sys
import time

sys.dont_write_bytecode = True

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
HERE = os.path.join('tools', 'aorai')
START = os.getcwd()   # the outputs go here; Frama-C runs from ROOT
OUT = os.path.join(START, 'aorai_out')
MODEL = 'x86_64'
SECONDS = 600
ENFORCE = True    # VERIFY-085 run 2 (report-only in runs 1 and 1b)

# (case, source, the Eva program whose preprocessing and options it takes,
#  automaton, verdict)
CASES = [
    ('hashmap', 'tools/eva/programs/hashmap.c', 'hashmap', 'hashmap_iteration', 'follows'),
    ('hashmap_ok', 'tools/aorai/programs/hashmap_ok.c', 'hashmap', 'hashmap_iteration', 'follows'),
    ('hashmap_stale', 'tools/aorai/programs/hashmap_stale.c', 'hashmap', 'hashmap_iteration', 'breaks'),
    ('stringbuf', 'tools/eva/programs/stringbuf.c', 'stringbuf', 'stringbuf_views', 'follows'),
    ('stringbuf_ok', 'tools/aorai/programs/stringbuf_ok.c', 'stringbuf', 'stringbuf_views', 'follows'),
    ('stringbuf_closed', 'tools/aorai/programs/stringbuf_closed.c', 'stringbuf', 'stringbuf_views', 'breaks'),
    ('stringbuf_reopened', 'tools/aorai/programs/stringbuf_reopened.c', 'stringbuf', 'stringbuf_views', 'breaks'),
    ('bitset', 'tools/eva/programs/bitset.c', 'bitset', 'bitset_views', 'follows'),
    ('bitset_ok', 'tools/aorai/programs/bitset_ok.c', 'bitset', 'bitset_views', 'follows'),
    ('bitset_closed', 'tools/aorai/programs/bitset_closed.c', 'bitset', 'bitset_views', 'breaks'),
    ('bitset_reopened', 'tools/aorai/programs/bitset_reopened.c', 'bitset', 'bitset_views', 'breaks'),
]
AUTOMATA = ['hashmap_iteration', 'stringbuf_views', 'bitset_views']
MARKER = re.compile(r'/\*\s*refused\s*\*/')

# What a native build with CANON_LIFETIME_DEBUG gives each program of
# tools/aorai/programs/: 0, or "aborts", the lifetime check's contract
# violation. A normal build returns 0 for each.
LIFETIME_DEBUG = {
    'hashmap_ok': '0', 'hashmap_stale': '0',
    'stringbuf_ok': '0', 'stringbuf_closed': 'aborts', 'stringbuf_reopened': 'aborts',
    'bitset_ok': '0', 'bitset_closed': 'aborts', 'bitset_reopened': 'aborts',
}


def eva():
    path = os.path.join(ROOT, 'tools', 'eva', 'eva.py')
    spec = importlib.util.spec_from_file_location('eva', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def automaton(name):
    return os.path.join(HERE, name + '.ya')


def marked_lines(source):
    with open(os.path.join(ROOT, source), encoding='utf-8') as f:
        return [n for n, line in enumerate(f, 1) if MARKER.search(line)]


def check():
    """CASES against the files: each source, automaton and Eva program
    there; each automaton in a case; one marked call in a case that breaks
    its rule, none in one that follows it; each of tools/aorai/programs/ in
    a case and in LIFETIME_DEBUG."""
    ev = eva()
    problems = []
    names = [c[0] for c in CASES]
    if len(set(names)) != len(names):
        problems.append('a case named twice')
    for case, source, program, auto, verdict in CASES:
        if not os.path.exists(os.path.join(ROOT, source)):
            problems.append(f'{case}: no file {source}')
            continue
        if program not in ev.PROGRAMS:
            problems.append(f'{case}: {program} is not a program of tools/eva/eva.py')
        if auto not in AUTOMATA or not os.path.exists(os.path.join(ROOT, automaton(auto))):
            problems.append(f'{case}: no automaton {auto}')
        marks = marked_lines(source)
        if verdict == 'breaks' and len(marks) != 1:
            problems.append(f'{case}: breaks, but {len(marks)} call(s) marked refused')
        if verdict == 'follows' and marks:
            problems.append(f'{case}: follows, but a call marked refused at line {marks}')
        if verdict not in ('follows', 'breaks'):
            problems.append(f'{case}: verdict {verdict!r}')
    used = {c[3] for c in CASES}
    for auto in AUTOMATA:
        if auto not in used:
            problems.append(f'{auto}: in no case')
    own = sorted(f[:-2] for f in os.listdir(os.path.join(ROOT, HERE, 'programs')) if f.endswith('.c'))
    for p in own:
        if p not in names:
            problems.append(f'tools/aorai/programs/{p}.c: in no case')
        if p not in LIFETIME_DEBUG:
            problems.append(f'tools/aorai/programs/{p}.c: not in LIFETIME_DEBUG')
    for p in LIFETIME_DEBUG:
        if p not in own:
            problems.append(f'LIFETIME_DEBUG names {p}, not a file of tools/aorai/programs/')
    for p in problems:
        print('PROBLEM:', p)
    print(f'aorai check: {len(CASES)} cases, {len(AUTOMATA)} automata, '
          f'{len(own)} programs of its own; {len(problems)} problem(s)')
    return 1 if problems else 0


def command(ev, source, program, auto, csv_path):
    # eva.py's options for the program, but not USE_SPEC: with the
    # specification of ptr_align_padding in place of its body, Eva leaves
    # Aorai's postcondition on it unknown, the ghost code that keeps the
    # state being in the body.
    extra = list(ev.OPTIONS.get(program, []))
    kernel = shlex.split(os.environ.get('EVA_FRAMAC_ARGS', ''))
    return (['frama-c'] + ev.MODELS[MODEL] + kernel
            + [f'-cpp-extra-args={ev.cpp_of(program)}', source,
               '-aorai-automata', automaton(auto), '-then-last']
            + ev.COMMON + extra + ['-then-last', '-report-csv', csv_path])


def read_table(csv_path):
    if not os.path.exists(csv_path):
        return None
    with open(csv_path, encoding='utf-8', errors='replace') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def judge(source, rows, log, rc):
    """The verdict, and what it rests on."""
    s = {'rc': rc}
    ends = set()
    for m in re.findall(r'Frama_C_show_each_reached_end: \{([^}]*)\}', log):
        ends |= {int(x) for x in re.findall(r'-?\d+', m)}
    s['end'] = ('{' + '; '.join(str(x) for x in sorted(ends)) + '}') if ends else None
    s['aborted'] = bool(re.search(r'aborted|User Error|Fatal error', log))
    if rows is None:
        s['statuses'] = None
        return 'undecided', s
    mine = [r for r in rows if 'aorai_' in (r.get('property') or '')]
    st = {}
    for r in mine:
        st[r['status']] = st.get(r['status'], 0) + 1
    s['statuses'] = st
    name = os.path.basename(source)
    sites = sorted({(r['file'], int(r['line'])) for r in mine
                    if r['status'].startswith('Invalid')
                    and r['property kind'].startswith('precondition of')})
    s['invalid_calls'] = [f'{f}:{n}' for f, n in sites]
    s['unknown'] = sum(v for k, v in st.items() if k.startswith('Unknown'))
    if rc != 0 or s['aborted']:
        return 'undecided', s
    if not sites and s['unknown'] == 0 and s['end'] is not None:
        return 'follows', s
    marks = marked_lines(source)
    if sites and all(f == name for f, _ in sites) and {n for _, n in sites} == set(marks):
        s['at'] = marks[0]
        return 'breaks', s
    return 'undecided', s


def run(selected):
    ev = eva()
    os.makedirs(OUT, exist_ok=True)
    results = []
    for case, source, program, auto, want in CASES:
        if selected and case not in selected:
            continue
        d = os.path.join(OUT, case)
        os.makedirs(d, exist_ok=True)
        csv_path = os.path.join(d, 'report.csv')
        if os.path.exists(csv_path):
            os.remove(csv_path)
        cmd = command(ev, source, program, auto, csv_path)
        print('Running:', ' '.join(shlex.quote(c) for c in cmd), flush=True)
        t0 = time.time()
        try:
            # Frama-C reads relative paths against $PWD, not the process's
            # directory: both are the repository's root here.
            p = subprocess.run(cmd, cwd=ROOT, env=dict(os.environ, PWD=ROOT),
                               capture_output=True, text=True, timeout=SECONDS)
            log, rc = p.stdout + p.stderr, p.returncode
        except subprocess.TimeoutExpired as e:
            out = e.stdout or ''
            log = out if isinstance(out, str) else out.decode(errors='replace')
            log, rc = log + f'\n[aorai.py] stopped after {SECONDS} s', -1
        secs = time.time() - t0
        with open(os.path.join(d, 'frama-c.log'), 'w', encoding='utf-8') as f:
            f.write(log)
        rows = read_table(csv_path)
        verdict, s = judge(source, rows, log, rc)
        s.update({'case': case, 'automaton': auto, 'verdict': verdict, 'want': want,
                  'seconds': round(secs, 1)})
        with open(os.path.join(d, 'summary.json'), 'w') as f:
            json.dump(s, f, indent=1)
        print(f'{case} ({auto}): {describe(s)}', flush=True)
        # Aorai's properties neither valid nor dead, and, for a case not as
        # CASES says, the end of Frama-C's log, so that the job's log tells.
        odd = [r for r in rows or [] if 'aorai_' in (r.get('property') or '')
               and r['status'] not in ('Valid', 'Dead')]
        if odd:
            shown('\n'.join(f'  {r["status"]}: {r["file"]}:{r["line"]} {r["function"]} '
                             f'{r["property kind"]}: {r["property"][:200]}' for r in odd[:40]))
        if verdict != want:
            print(f'--- {case}: the end of Frama-C\'s log ---')
            shown('\n'.join(log.splitlines()[-25:]))
        results.append(s)
    return notices(results)


def shown(text):
    """A tool's output in the log, none of its lines a workflow command."""
    for line in text.splitlines():
        print('| ' + line if line.lstrip().startswith(('::', '##[')) else line)
    sys.stdout.flush()


def describe(s):
    v = s['verdict']
    if v == 'breaks':
        v += f' at line {s["at"]}'
    st = s.get('statuses')
    st = ', '.join(f'{k} {n}' for k, n in sorted(st.items())) if st else 'no table'
    end = f'end {s["end"]}' if s['end'] else 'end not reached'
    extra = f'; invalid calls {" ".join(s["invalid_calls"])}' if s.get('invalid_calls') and s['verdict'] != 'breaks' else ''
    tail = '' if s['verdict'] == s['want'] else f'; NOT AS CASES SAYS ({s["want"]})'
    return f'{v}; Aorai\'s properties: {st}; {end}{extra}; {s["seconds"]} s{tail}'


def notices(results):
    lines = []
    for auto in AUTOMATA:
        mine = [s for s in results if s['automaton'] == auto]
        if mine:
            lines.append(f'::notice title=aorai {auto}::' + '; '.join(
                f'{s["case"]}: {describe(s)}' for s in mine))
    bad = [s['case'] for s in results if s['verdict'] != s['want']]
    counts = {}
    for s in results:
        counts[s['verdict']] = counts.get(s['verdict'], 0) + 1
    tally = ', '.join(f'{k} {n}' for k, n in sorted(counts.items()))
    if bad:
        lines.append(f'::notice title=aorai::{len(results)} cases: {tally}; '
                     f'not as CASES says: {" ".join(bad)}')
    else:
        lines.append(f'::notice title=aorai::{len(results)} cases as CASES says: {tally}')
    with open(os.path.join(START, 'notices_aorai.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    if not results:
        return 1
    return 1 if (bad and ENFORCE) else 0


def native():
    """Each program of tools/aorai/programs/ compiled with gcc as
    eva.py native compiles the Eva programs, and run; then compiled with
    CANON_LIFETIME_DEBUG and the contracts' checks on (without
    CANON_NO_REQUIRE and NDEBUG), and run."""
    ev = eva()
    cc = os.environ.get('CC', 'gcc')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-g',
             '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
    os.makedirs(os.path.join(OUT, 'native'), exist_ok=True)
    parts, bad = [], []
    for case, source, program, _auto, _want in CASES:
        if not source.startswith(HERE):
            continue
        base = shlex.split(ev.cpp_of(program, native=True))
        debug = [w for w in base if w not in ('-DCANON_NO_REQUIRE', '-DNDEBUG')] + ['-DCANON_LIFETIME_DEBUG']
        got = []
        for kind, cpp in (('normal', base), ('lifetime', debug)):
            exe = os.path.abspath(os.path.join(OUT, 'native', f'{case}.{kind}'))
            cmd = [cc] + flags + cpp + [source, '-o', exe]
            print('Running:', ' '.join(cmd), flush=True)
            c = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
            if c.returncode != 0:
                print(c.stdout + c.stderr)
                got.append('does not compile')
                continue
            r = subprocess.run([exe], cwd=ROOT, capture_output=True, text=True, timeout=60)
            print(r.stdout + r.stderr)
            got.append('aborts' if r.returncode in (-6, 134) else str(r.returncode))
        want = ['0', LIFETIME_DEBUG.get(case, '?')]
        if got != want:
            bad.append(case)
        parts.append(f'{case} {got[0]}/{got[-1]}' + ('' if got == want else f' (wanted {want[0]}/{want[1]})'))
    line = (f'::notice title=aorai native::{len(parts)} programs, normal build / CANON_LIFETIME_DEBUG: '
            + '; '.join(parts) + ('' if not bad else f'; not as LIFETIME_DEBUG says: {" ".join(bad)}'))
    with open(os.path.join(START, 'notices_aorai_native.txt'), 'w') as f:
        f.write(line + '\n')
    print(line)
    return 1 if (bad and ENFORCE) else 0


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    os.chdir(ROOT)
    if argv[0] == 'check':
        return check()
    if argv[0] == 'run':
        if check():
            return 1
        return run(argv[1:])
    if argv[0] == 'native':
        return native()
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
