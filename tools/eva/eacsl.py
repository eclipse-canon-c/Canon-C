#!/usr/bin/env python3
"""
eacsl.py -- E-ACSL on 14 · Eva's programs (VERIFY-083).

E-ACSL, Frama-C's runtime assertion checker, turns a program's ACSL
annotations -- the contracts of the functions it calls, their assertions
and loop invariants -- into C that checks them while the program runs.
Each program of tools/eva/programs is instrumented and compiled by
e-acsl-gcc.sh, as Frama-C reads it for Eva (eva.py's preprocessing, C11,
and Frama-C's own C library, -L), at the host's machine model, x86_64.
Without -L the script reads the system's headers, where Frama-C's
library's logic is not: core/slice.h's contract of str_from_cstr names
valid_read_string, and 16 programs stopped there (VERIFY-083 run 1b). tools/eva/eacsl_markers.c defines the
markers it calls (CHECK and END, eva_program.h). Then it runs.

E-ACSL's segment memory model wants the program's memory where it expects
it, so each run is without address randomization (setarch -R): with it,
E-ACSL's runtime stops at the start, "Unsupported TLS area in the middle
of heap area", three programs of four here (Frama-C 25, VERIFY-083).

With --keep-going, a failed annotation is reported and the run goes on,
each as "FILE:LINE: Error: KIND failed:" on stderr. A program is as
E-ACSL runs it when it is instrumented and compiled, and runs to its END
with no CHECK failed, no annotation failed, and no verdict E-ACSL calls
unsound ("no sound verdict"). The run's exit status does not tell a failed
annotation, with --keep-going.

    python3 tools/eva/eacsl.py run [PROGRAM...]
        Into eacsl_out/: each program's instrumented code (.frama.c),
        E-ACSL's output (.log), the run's (.out, .err) and a summary
        (.json). Prints a table, and compares each program's result with
        its pin; the notices go to notices_eacsl.txt. EACSL_GCC names the
        script (default e-acsl-gcc.sh), EACSL_SECONDS limits each
        instrumentation (default 300), EACSL_PINS names the pins (default
        tools/eva/eacsl_pins.txt).
    python3 tools/eva/eacsl.py pin [DIR] [FILE]
        Writes the pins from a run's summaries (DIR, default eacsl_out).
    python3 tools/eva/eacsl.py check
        The pins are well formed: one result of three for each program.

Counted besides, not part of the verdict: the annotations E-ACSL does not
translate ("E-ACSL construct ... is not yet supported"), by construct, and
the checks it inserts: the calls of __e_acsl_assert in the instrumented
code, each a check of an annotation, or of part of one.

Each program's result, as tools/eva/eacsl_pins.txt pins it:

    runs                     as E-ACSL runs it, above
    stops: mkBinOp           E-ACSL's translation stops on a C integer
                             beside a GMP one ("mkBinOp: unsupported
                             operator for such operands")
    stops: does not finish   E-ACSL's stack overflows, or it is not done in
                             EACSL_SECONDS: with no limit on the stack,
                             option.c's instrumentation was not done in 600 s
                             of CPU (VERIFY-083), so the two are one result

A program compiled and not as E-ACSL runs it is never pinned. With
ENFORCE (since VERIFY-083's run 2), a result not as pinned, or pins not
well formed, fail the job.
"""
import collections
import json
import os
import platform
import re
import shlex
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import eva  # noqa: E402  (the programs and their preprocessing)

ENFORCE = True    # since VERIFY-083's run 2; runs 1, 1b and 1c reported

OUT = 'eacsl_out'
NOTICES = 'notices_eacsl.txt'
CUT = 4000        # the runner cuts an annotation at 4096 characters
SCRIPT = os.environ.get('EACSL_GCC', 'e-acsl-gcc.sh')
SECONDS = int(os.environ.get('EACSL_SECONDS', '300'))
PINS = os.environ.get('EACSL_PINS', os.path.join(HERE, 'eacsl_pins.txt'))
RESULTS = ('runs', 'stops: mkBinOp', 'stops: does not finish')
RUN_SECONDS = 120
MARKERS = os.path.join(HERE, 'eacsl_markers.c')

NOT_SUPPORTED = re.compile(r"E-ACSL construct\s+`([^']+)'\s+is not yet supported")
FAILED = re.compile(r'^(\S+):(\d+): Error: (.+) failed:\s*$')
UNSOUND = re.compile(r'^(\S+):(\d+): Warning: no sound verdict for (.+) \(guess: (\w+)\)\.\s*$')
END = re.compile(r'^END reached, fails (-?\d+)\s*$', re.M)
CHECK_FAILED = re.compile(r'^CHECK failed at line (\d+)\s*$', re.M)
STOPPED = re.compile(r'(User Error:.*|Failure:.*|Frama-C aborted:.*|Unexpected error.*|'
                     r'fail to compile.*|aborted by Frama-C.*|error:.*)')


# e-acsl-gcc.sh reads its arguments word by word, unquoted ("for i in $@"),
# so a word of an argument that is one of its options is taken for that
# option: VERIFY-083's run 1 gave "-std=c11 -I core ..." as one -E, whose
# "-I" took the next word for Frama-C's path, and no program kept its file
# ("no input files"). Each flag goes in a -E of its own, one word.
SCRIPT_OPTIONS = {'-h', '-q', '-s', '-D', '-V', '-F', '-c', '-d', '-v', '-o', '-O', '-E', '-l',
                  '-e', '-C', '-f', '-X', '-L', '-M', '-g', '-I', '-G', '-a', '-A', '-T', '-m',
                  '-k'}


def cpp_words(program):
    """Eva's preprocessing flags for PROGRAM, in C11, each one word: -I and
    its directory joined, as -D and -U with theirs."""
    toks = shlex.split('-std=c11 ' + eva.cpp_of(program))
    words, i = [], 0
    while i < len(toks):
        t = toks[i]
        if t in ('-I', '-D', '-U') and i + 1 < len(toks):
            t, i = t + toks[i + 1], i + 1
        if t in SCRIPT_OPTIONS or t.startswith('--') or any(c.isspace() for c in t):
            raise ValueError(f"{t!r} would be read as e-acsl-gcc.sh's own")
        words.append(t)
        i += 1
    return words


def command(program, stem, markers):
    """e-acsl-gcc.sh's command: instrument with Frama-C's C library (-L),
    then compile the instrumented code alone (-c -X), with the segment
    model and --keep-going."""
    for path in (stem, markers):
        if any(c.isspace() for c in path):
            raise ValueError(f'{path!r} has a space, which e-acsl-gcc.sh would split')
    return ([SCRIPT, '-c', '-X', '-L', '-m', 'segment', '-k']
            + [w for flag in cpp_words(program) for w in ('-E', flag)]
            + ['-l', markers, '-o', stem + '.frama.c', '-O', stem, eva.source(program)])


def text_of(x):
    """A process's output as text, which a timeout leaves as bytes or None."""
    if isinstance(x, bytes):
        return x.decode('utf-8', errors='replace')
    return x or ''


def stopped(text):
    """The first line that says why an instrumentation or a compilation
    stopped."""
    lines = text.splitlines()
    for i, line in enumerate(lines):
        m = STOPPED.search(line)
        if m:
            said = m.group(1)
            if said.rstrip().endswith(':'):
                said += ' ' + ' '.join(x.strip() for x in lines[i + 1:i + 3] if x.strip())
            return ' '.join(said.split())[:200]
    return ''


def failures(err):
    """The failed annotations of a run's stderr: file, line, kind and the
    predicate's first lines."""
    lines = err.splitlines()
    out = []
    for i, line in enumerate(lines):
        m = FAILED.match(line)
        if m:
            pred = ' '.join(x.strip() for x in lines[i + 2:i + 4]
                            if x.startswith('\t') and not x.strip().startswith('With values'))
            out.append({'file': m.group(1), 'line': int(m.group(2)), 'kind': m.group(3),
                        'predicate': ' '.join(pred.split())[:300]})
    return out


def one(program, markers):
    stem = os.path.join(os.path.abspath(OUT), program)
    for ext in ('.frama.c', '.e-acsl', '.log', '.out', '.err', '.json'):
        if os.path.exists(stem + ext):
            os.remove(stem + ext)
    s = {'program': program, 'instrumented': False, 'compiled': False, 'reached_end': False,
         'fails': None, 'checks_failed': [], 'failed': [], 'unsound': [], 'run_exit': None,
         'reason': '', 'not_supported': {}, 'checks': 0}
    try:
        cmd = command(program, stem, markers)
    except ValueError as e:
        s.update(reason=str(e), ok=False, exit=None, seconds=0.0)
        with open(stem + '.json', 'w', encoding='utf-8') as f:
            json.dump(s, f, indent=1, ensure_ascii=False)
        return s
    start = time.time()
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=SECONDS)
        log, s['exit'] = p.stdout + p.stderr, p.returncode
    except subprocess.TimeoutExpired as e:
        log, s['exit'] = text_of(e.stdout) + text_of(e.stderr), None
        s['reason'] = f'not done in {SECONDS} s'
    s['seconds'] = round(time.time() - start, 1)
    with open(stem + '.log', 'w', encoding='utf-8') as f:
        f.write(' '.join(shlex.quote(c) for c in cmd) + '\n' + log)
    s['not_supported'] = dict(collections.Counter(m.group(1) for m in NOT_SUPPORTED.finditer(log)))
    if os.path.exists(stem + '.frama.c') and os.path.getsize(stem + '.frama.c') > 0:
        s['instrumented'] = True
        with open(stem + '.frama.c', encoding='utf-8', errors='replace') as f:
            s['checks'] = f.read().count('__e_acsl_assert(')
    s['compiled'] = os.path.exists(stem + '.e-acsl')
    if not s['compiled']:
        s['reason'] = s['reason'] or stopped(log) or f'exit {s["exit"]}'
    else:
        run = ['setarch', platform.machine(), '-R', stem + '.e-acsl']
        try:
            r = subprocess.run(run, cwd=ROOT, capture_output=True, text=True, timeout=RUN_SECONDS)
            out, err, s['run_exit'] = r.stdout, r.stderr, r.returncode
        except subprocess.TimeoutExpired as e:
            out, err = text_of(e.stdout), text_of(e.stderr)
            s['reason'] = f'run not done in {RUN_SECONDS} s'
        with open(stem + '.out', 'w', encoding='utf-8') as f:
            f.write(out)
        with open(stem + '.err', 'w', encoding='utf-8') as f:
            f.write(err)
        m = END.search(out)
        s['reached_end'] = m is not None
        s['fails'] = int(m.group(1)) if m else None
        s['checks_failed'] = [int(x) for x in CHECK_FAILED.findall(out)]
        s['failed'] = failures(err)
        s['unsound'] = [{'file': u.group(1), 'line': int(u.group(2)), 'kind': u.group(3),
                         'guess': u.group(4)} for u in map(UNSOUND.match, err.splitlines()) if u]
        if not s['reached_end'] and not s['reason']:
            s['reason'] = (f'run exits {s["run_exit"]} before END'
                           + (': ' + ' '.join(err.split())[:160] if err.strip() else ''))
    s['ok'] = bool(s['compiled'] and s['run_exit'] == 0 and s['reached_end'] and s['fails'] == 0
                   and not s['checks_failed'] and not s['failed'] and not s['unsound'])
    with open(stem + '.json', 'w', encoding='utf-8') as f:
        json.dump(s, f, indent=1, ensure_ascii=False)
    return s


def result_of(s):
    """A program's result, as the pins hold it."""
    if s['ok']:
        return 'runs'
    if s['compiled']:
        return 'fails'
    r = s['reason']
    if 'mkBinOp' in r:
        return 'stops: mkBinOp'
    if 'Stack overflow' in r or r.startswith('not done in'):
        return 'stops: does not finish'
    return 'stops: ' + (r or 'unknown')


def read_pins(path=None):
    """The pins, and what is wrong with them."""
    path = path or PINS
    pins, problems = {}, []
    try:
        with open(path, encoding='utf-8') as f:
            lines = f.read().splitlines()
    except OSError as e:
        return {}, [f'no pins: {e}']
    for n, raw in enumerate(lines, 1):
        line = raw.split('#', 1)[0].strip()
        if not line or line.split()[0] == 'program':
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            problems.append(f'line {n}: not "program result"')
            continue
        prog, res = parts[0], ' '.join(parts[1].split())
        if prog in pins:
            problems.append(f'line {n}: {prog} pinned twice')
        if res not in RESULTS:
            problems.append(f'line {n}: {prog}: "{res}" is not a result the pins hold')
        pins[prog] = res
    problems += [f'{p}: no pin' for p in sorted(eva.PROGRAMS) if p not in pins]
    problems += [f'{p}: pinned, and not a program' for p in sorted(pins) if p not in eva.PROGRAMS]
    return pins, problems


HEADER = """\
# E-ACSL on the programs of tools/eva/programs, at x86_64: each one's result,
# as tools/eva/eacsl.py gives it with Frama-C 29.0 (Copper) (VERIFY-083).
#
#   runs                     instrumented and compiled, and run to its END with
#                            no CHECK, no annotation and no verdict failing
#   stops: mkBinOp           E-ACSL's translation stops on a C integer beside a
#                            GMP one ("mkBinOp: unsupported operator")
#   stops: does not finish   E-ACSL's stack overflows, or it is not done in
#                            EACSL_SECONDS (300)
#
# A result not as pinned fails the e-acsl job of 14 · Eva. A program compiled
# and not as E-ACSL runs it is never pinned. eacsl.py pin DIR writes the pins
# from a run's summaries, under a record.
"""


def pin(where_dir, path):
    rows = []
    for p in sorted(eva.PROGRAMS):
        try:
            with open(os.path.join(where_dir, p + '.json'), encoding='utf-8') as f:
                s = json.load(f)
        except OSError:
            print(f'{p}: no summary in {where_dir}')
            return 1
        r = result_of(s)
        if r not in RESULTS:
            print(f'{p}: "{r}" is not a result the pins hold')
            return 1
        rows.append((p, r))
    with open(path, 'w', encoding='utf-8') as f:
        f.write(HEADER + f'{"program":<16}result\n')
        for p, r in rows:
            f.write(f'{p:<16}{r}\n')
    print(f'{path}: {len(rows)} programs')
    return 0


def check():
    pins, problems = read_pins()
    counts = collections.Counter(pins.values())
    print(f'e-acsl pins: {len(pins)} programs, ' + ', '.join(f'{r} {counts[r]}' for r in RESULTS)
          + f'; {len(problems)} problem(s)')
    for p in problems:
        print('  ' + p)
    return 1 if problems else 0


def run(programs):
    os.makedirs(OUT, exist_ok=True)
    markers = os.path.abspath(os.path.join(OUT, 'eacsl_markers.o'))
    c = subprocess.run(['gcc', '-std=c11', '-O2', '-c', MARKERS, '-o', markers],
                       capture_output=True, text=True)
    if c.returncode != 0:
        print(c.stdout + c.stderr)
        return 1
    start = time.time()
    with ThreadPoolExecutor(max_workers=max(1, os.cpu_count() or 1)) as pool:
        results = list(pool.map(lambda p: one(p, markers), programs))
    seconds = round(time.time() - start)

    print(f'{"program":<15} {"instr.":>6} {"comp.":>5} {"end":>4} {"fails":>5} {"failed":>6} '
          f'{"unsound":>7} {"checks":>6} {"not tr.":>7} {"s":>6}  reason')
    for s in results:
        print(f'{s["program"]:<15} {"yes" if s["instrumented"] else "no":>6} '
              f'{"yes" if s["compiled"] else "no":>5} {"yes" if s["reached_end"] else "no":>4} '
              f'{"-" if s["fails"] is None else s["fails"]:>5} {len(s["failed"]):>6} '
              f'{len(s["unsound"]):>7} {s["checks"]:>6} {sum(s["not_supported"].values()):>7} '
              f'{s["seconds"]:>6}  {s["reason"]}')
    for s in results:
        for f in s['failed']:
            print(f'failed: {s["program"]}: {f["file"]}:{f["line"]} {f["kind"]}: {f["predicate"]}')
        for u in s['unsound']:
            print(f'unsound: {s["program"]}: {u["file"]}:{u["line"]} {u["kind"]} (guess {u["guess"]})')

    n = len(results)
    ok = [s for s in results if s['ok']]
    inst = [s for s in results if s['instrumented']]
    comp = [s for s in results if s['compiled']]
    end = [s for s in results if s['reached_end']]
    nfail = sum(len(s['failed']) for s in results)
    nuns = sum(len(s['unsound']) for s in results)
    ncheck = sum(len(s['checks_failed']) for s in results)
    nys = collections.Counter()
    for s in results:
        nys.update(s['not_supported'])
    lines = [
        ('e-acsl', f'e-acsl: {len(ok)} of {n} programs as E-ACSL runs them'
                   + ('' if ENFORCE else ', report-only')
                   + f': instrumented {len(inst)}, compiled {len(comp)}, run to their end '
                     f'{len(end)}; annotations failed {nfail}, verdicts unsound {nuns}, CHECKs '
                     f'failed {ncheck}; {seconds} s'),
        ('e-acsl checks', f'e-acsl checks: {sum(s["checks"] for s in inst)} inserted in the '
                          f'{len(inst)} instrumented programs; annotations not translated '
                          f'{sum(nys.values())}: '
                          + ', '.join(f'{k} {v}' for k, v in nys.most_common())),
    ]
    stopped_at = [s for s in results if not s['compiled'] or not s['reached_end']]
    if stopped_at:
        lines.append(('e-acsl stopped', 'e-acsl stopped: ' + '; '.join(
            f'{s["program"]}: {"not instrumented" if not s["instrumented"] else "not compiled" if not s["compiled"] else "no END"}'
            f': {s["reason"]}' for s in stopped_at)))
    if nfail or nuns or ncheck:
        lines.append(('e-acsl failed', 'e-acsl failed: ' + '; '.join(
            [f'{s["program"]}: {f["file"]}:{f["line"]} {f["kind"]}: {f["predicate"][:120]}'
             for s in results for f in s['failed']]
            + [f'{s["program"]}: unsound at {u["file"]}:{u["line"]} {u["kind"]}'
               for s in results for u in s['unsound']]
            + [f'{s["program"]}: CHECK at line {x}' for s in results for x in s['checks_failed']])))
    pins, pin_problems = read_pins()
    diffs = [f'{s["program"]}: pinned "{pins[s["program"]]}", gives "{result_of(s)}"'
             for s in results if s['program'] in pins and result_of(s) != pins[s['program']]]
    counts = collections.Counter(result_of(s) for s in results)
    if not diffs and not pin_problems:
        pinned = (f'e-acsl pins: as pinned, {n} programs: '
                  + ', '.join(f'{r} {counts[r]}' for r in RESULTS if counts[r]))
    else:
        pinned = (f'e-acsl pins: {len(diffs)} difference(s), {len(pin_problems)} problem(s)'
                  + ('' if ENFORCE else ', report-only') + ': ' + '; '.join(diffs + pin_problems))
    lines.insert(1, ('e-acsl pins', pinned))
    with open(NOTICES, 'w', encoding='utf-8') as f:
        for title, msg in lines:
            f.write(f'::notice title={title}::{msg[:CUT]}\n')
    for _, msg in lines:
        print(msg)
    return 1 if (ENFORCE and (diffs or pin_problems)) else 0


def main(argv):
    if len(argv) >= 2 and argv[1] == 'run':
        programs = argv[2:] or sorted(eva.PROGRAMS)
        unknown = [p for p in programs if p not in eva.PROGRAMS]
        if unknown:
            print('no such program: ' + ', '.join(unknown))
            return 2
        return run(programs)
    if len(argv) >= 2 and argv[1] == 'pin' and len(argv) <= 4:
        return pin(argv[2] if len(argv) > 2 else OUT, argv[3] if len(argv) > 3 else PINS)
    if len(argv) == 2 and argv[1] == 'check':
        return check()
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
