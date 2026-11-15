#!/usr/bin/env python3
"""
eacsl.py -- E-ACSL on 14 · Eva's programs (VERIFY-083).

E-ACSL, Frama-C's runtime assertion checker, turns a program's ACSL
annotations -- the contracts of the functions it calls, their assertions
and loop invariants -- into C that checks them while the program runs.
Each program of tools/eva/programs is instrumented and compiled by
e-acsl-gcc.sh, as Frama-C reads it for Eva (eva.py's preprocessing, C11),
at the host's machine model, x86_64. tools/eva/eacsl_markers.c defines the
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
        (.json). Prints a table; the notices go to notices_eacsl.txt.
        EACSL_GCC names the script (default e-acsl-gcc.sh), EACSL_SECONDS
        limits each instrumentation (default 900).

Counted besides, not part of the verdict: the annotations E-ACSL does not
translate ("E-ACSL construct ... is not yet supported"), by construct, and
the checks it inserts: the calls of __e_acsl_assert in the instrumented
code, each a check of an annotation, or of part of one.

ENFORCE: a program not as E-ACSL runs it fails the job (VERIFY-083's
run 1 reports).
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

ENFORCE = False   # VERIFY-083 run 1: report-only

OUT = 'eacsl_out'
NOTICES = 'notices_eacsl.txt'
CUT = 4000        # the runner cuts an annotation at 4096 characters
SCRIPT = os.environ.get('EACSL_GCC', 'e-acsl-gcc.sh')
SECONDS = int(os.environ.get('EACSL_SECONDS', '900'))
RUN_SECONDS = 120
MARKERS = os.path.join(HERE, 'eacsl_markers.c')

NOT_SUPPORTED = re.compile(r"E-ACSL construct\s+`([^']+)'\s+is not yet supported")
FAILED = re.compile(r'^(\S+):(\d+): Error: (.+) failed:\s*$')
UNSOUND = re.compile(r'^(\S+):(\d+): Warning: no sound verdict for (.+) \(guess: (\w+)\)\.\s*$')
END = re.compile(r'^END reached, fails (-?\d+)\s*$', re.M)
CHECK_FAILED = re.compile(r'^CHECK failed at line (\d+)\s*$', re.M)
STOPPED = re.compile(r'(User Error:.*|Failure:.*|Frama-C aborted:.*|Unexpected error.*|'
                     r'fail to compile.*|aborted by Frama-C.*|error:.*)')


def command(program, stem, markers):
    """e-acsl-gcc.sh's command: instrument, then compile the instrumented
    code alone (-c -X), with the segment model and --keep-going."""
    return [SCRIPT, '-c', '-X', '-m', 'segment', '-k',
            '-E', '-std=c11 ' + eva.cpp_of(program),
            '-l', markers,
            '-o', stem + '.frama.c', '-O', stem, eva.source(program)]


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
    cmd = command(program, stem, markers)
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
    with open(NOTICES, 'w', encoding='utf-8') as f:
        for title, msg in lines:
            f.write(f'::notice title={title}::{msg[:CUT]}\n')
    for _, msg in lines:
        print(msg)
    return 1 if (ENFORCE and len(ok) < n) else 0


def main(argv):
    if len(argv) >= 2 and argv[1] == 'run':
        programs = argv[2:] or sorted(eva.PROGRAMS)
        unknown = [p for p in programs if p not in eva.PROGRAMS]
        if unknown:
            print('no such program: ' + ', '.join(unknown))
            return 2
        return run(programs)
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
