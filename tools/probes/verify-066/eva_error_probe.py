#!/usr/bin/env python3
"""
eva_error_probe.py -- VERIFY-066's probe: Frama-C 29's error in Eva.

Frama-C 29's Eva stops on entering core/arena.h's arena_alloc, in every
program that calls it, with the uncaught exception
Eva__Eval_terms.Reduce_to_bottom (exit 125), raised while it decides which
of arena_alloc's behaviours is active (docs/deviations.md, VERIFY-066, the
fix's run). The behaviours `fits` and `does_not_fit` assume arena_can_fit,
which binds with \\let the address a->buffer + a->offset converted to an
integer, and a pad computed from it. Frama-C 25, which runs every program
to its end, does not evaluate \\let; Frama-C 28 began to ("Support
evaluation of simple \\let bindings in ACSL terms and predicates", its
changelog).

The probe runs Eva

- on six small programs, each run as `frama-c -eva FILE` and nothing else:
  r0_full.c, arena_alloc standalone, and r1_min.c, its smallest form, then
  r1_min.c with one construct taken out: \\let (r2_nolet.c), the address
  converted to an integer (r3_nocast.c), the second behaviour (r4_onebhv.c),
  the body (r5_nobody.c);
- on tools/eva/programs/arena.c, with 14 · Eva's command at x86_64
  (tools/eva/eva.py): as it is (a0_control); with arena_can_fit's three
  \\let written out, in a copy of core/arena.h (a1_inline); and with
  arena_alloc read by its contract, -eva-use-spec (a2_usespec).

    python3 tools/probes/verify-066/eva_error_probe.py

For each variant it prints the outcome, and for one that stops, the end of
Frama-C's output (its backtrace) and the last call Eva names. One notice
per variant goes to notices_evaerror.txt; each variant's whole output, and
the CSV of each arena.c run, go to evaerror_out/ in the directory it is run
from. Report-only: it exits 0, or 1 if a variant cannot be set up (a file
missing, arena_can_fit not found as written). Frama-C 25 needs
EVA_FRAMAC_ARGS=-c11 for arena.c, as tools/eva/README.md says; 29 does
not. The scoring is VERIFY-066's, in docs/deviations.md.
"""
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'eva'))
import eva  # noqa: E402  (tools/eva/eva.py: 14 · Eva's command and reading)

SMALL = [
    ('r0_full', 'arena_alloc standalone'),
    ('r1_min', 'its smallest form'),
    ('r2_nolet', 'r1_min without \\let'),
    ('r3_nocast', 'r1_min with no address converted'),
    ('r4_onebhv', 'r1_min with one behaviour'),
    ('r5_nobody', 'r1_min with no body'),
]

# arena_can_fit as core/arena.h writes it (VERIFY-036 F4), and the same
# predicate with its three \let written out.
CAN_FIT = '''\
  predicate arena_can_fit{L}(Arena *a, integer size, integer alignment) =
      \\let cur  = a->offset;
      \\let addr = (usize)(uintptr_t)(a->buffer + cur);
      \\let pad  = ((addr + alignment - 1) & ~(alignment - 1)) - addr;
      cur <= CANON_USIZE_MAX - pad &&
      cur + pad <= CANON_USIZE_MAX - size &&
      cur + pad + size <= a->capacity;
'''
ADDR = '(usize)(uintptr_t)(a->buffer + a->offset)'
PAD = f'((({ADDR} + alignment - 1) & ~(alignment - 1)) - {ADDR})'
CAN_FIT_INLINE = f'''\
  predicate arena_can_fit{{L}}(Arena *a, integer size, integer alignment) =
      a->offset <= CANON_USIZE_MAX - {PAD} &&
      a->offset + {PAD} <= CANON_USIZE_MAX - size &&
      a->offset + {PAD} + size <= a->capacity;
'''

TAIL = 40
LIMIT = 900
OUT = os.path.abspath('evaerror_out')   # each variant's output and CSV
OUTCOMES = []


def frama_c(cmd, cwd):
    """Frama-C's exit status and output. It reads its directory from PWD;
    OCAMLRUNPARAM=b makes an internal error print OCaml's backtrace."""
    print('Running:', ' '.join(shlex.quote(c) for c in cmd), f'(in {cwd})', flush=True)
    start = time.time()
    try:
        p = subprocess.run(cmd, cwd=cwd, env=dict(os.environ, PWD=cwd, OCAMLRUNPARAM='b'),
                           capture_output=True, text=True, errors='replace', timeout=LIMIT)
        return p.returncode, p.stdout + p.stderr, time.time() - start
    except subprocess.TimeoutExpired:
        return None, f'[eva_error_probe.py] stopped after {LIMIT} s\n', time.time() - start


def last_call(cmd, cwd):
    """The last call Eva entered: tools/eva/eva.py's last_call, in cwd."""
    i = cmd.index('-then') if '-then' in cmd else len(cmd)
    again = cmd[:1] + ['-eva-show-progress'] + cmd[1:i]
    code, out, _ = frama_c(again, cwd)
    calls = re.findall(r'computing for function (.+?)\.\s*\n\s*Called from ([^\n]+?)\.\s*\n', out)
    if not calls:
        return 'not found'
    callee, site = calls[-1]
    return f'{callee}, called from {site}'


def outcome(name, what, cmd, cwd, code, text, seconds, summary=None):
    """One line for the variant, and for a run that stopped, why and where."""
    with open(os.path.join(OUT, name + '.log'), 'w') as f:
        f.write(text)
    if code is not None and 'Reduce_to_bottom' in text:
        head = f'stops: Reduce_to_bottom (exit {code})'
    elif code == 0:
        if summary is not None:
            head = 'finishes: ' + eva.line(summary).split(': ', 1)[1]
        else:
            s = eva.parse_log(text)
            pre = s.get('preconditions')
            fn = s.get('functions')
            head = (f"finishes: alarms {s['alarms']}"
                    + (f"; preconditions {'/'.join(map(str, pre))}" if pre else '')
                    + (f"; functions {fn[0]}/{fn[1]}" if fn else ''))
    else:
        head = f'stops otherwise (exit {code})'
    # eva.line gives the seconds itself.
    result = f'{name} ({what}): {head}' + ('' if summary is not None else f'; {seconds:.1f} s')
    print(result, flush=True)
    if not head.startswith('finishes'):
        print(f"  -- the last {TAIL} lines of Frama-C's output:")
        for l in text.rstrip().split('\n')[-TAIL:]:
            print('  | ' + l)
        where = last_call(cmd, cwd)
        print('  -- the call Eva was analysing when it stopped: ' + where)
        result += f'; last call {where}'
    OUTCOMES.append(result)


def small(name, what):
    path = os.path.join('tools', 'probes', 'verify-066', name + '.c')
    if not os.path.exists(os.path.join(ROOT, path)):
        raise SystemExit(f'missing: {path}')
    cmd = ['frama-c', '-eva', path]
    code, text, seconds = frama_c(cmd, ROOT)
    outcome(name, what, cmd, ROOT, code, text, seconds)


def arena(name, what, root, extra_use_spec=None):
    csv_path = os.path.join(OUT, f'{name}.csv')
    cmd = eva.eva_command('arena', 'x86_64', csv_path)
    if extra_use_spec:
        if '-eva-use-spec' in cmd:
            i = cmd.index('-eva-use-spec') + 1
            cmd[i] = cmd[i] + ',' + extra_use_spec
        else:
            cmd[1:1] = ['-eva-use-spec', extra_use_spec]
    code, text, seconds = frama_c(cmd, root)
    s = eva.summarize('arena', 'x86_64', text, eva.parse_csv(csv_path), seconds, code)
    outcome(name, what, cmd, root, code, text, seconds,
            summary=None if (s.get('aborted') or s.get('functions') is None) else s)
    for d in eva.details(s):
        print(d)


def inline_copy():
    """A copy of what arena.c reads, with arena_can_fit's \\let written out."""
    files = eva.includes('arena') | {eva.source('arena')}
    outside = sorted(f for f in files if not f.startswith(('core/', 'tools/eva/programs/')))
    if outside:
        raise SystemExit(f'arena.c reads files outside core/ and tools/eva/programs/: {outside}')
    root = tempfile.mkdtemp(prefix='eva_error_probe_')
    for d in ('core', os.path.join('tools', 'eva', 'programs')):
        shutil.copytree(os.path.join(ROOT, d), os.path.join(root, d))
    header = os.path.join(root, 'core', 'arena.h')
    with open(header) as f:
        text = f.read()
    if text.count(CAN_FIT) != 1:
        raise SystemExit('core/arena.h: arena_can_fit is not written as this probe expects')
    with open(header, 'w') as f:
        f.write(text.replace(CAN_FIT, CAN_FIT_INLINE))
    print('a1_inline: arena_can_fit in the copy of core/arena.h:')
    print(CAN_FIT_INLINE)
    return root


def main():
    os.makedirs(OUT, exist_ok=True)
    print(subprocess.run(['frama-c', '-version'], capture_output=True, text=True).stdout.strip())
    for name, what in SMALL:
        small(name, what)
    arena('a0_control', 'arena.c as it is', ROOT)
    arena('a1_inline', 'arena.c, the \\let of arena_can_fit written out', inline_copy())
    arena('a2_usespec', 'arena.c, arena_alloc read by its contract', ROOT,
          extra_use_spec='arena_alloc')
    with open('notices_evaerror.txt', 'w') as f:
        for l in OUTCOMES:
            f.write(f'::notice title=Eva error probe::{l}\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
