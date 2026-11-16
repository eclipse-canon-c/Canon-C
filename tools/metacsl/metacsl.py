#!/usr/bin/env python3
"""
metacsl.py -- the library's cross-cutting rules, written as meta-properties
for Frama-C's MetAcsl and proved by WP on every proof unit (VERIFY-086).

A contract speaks of one function. Some of the library's rules speak of
all of them: core/primitives/contract.h's handler, "the library's ONLY
global mutable object", is to be written "through contract_set_handler()
only"; and only documented functions reach the heap. MetAcsl takes such a
rule as a meta-property: a predicate, the functions it applies to, and a
context, the points of each function where it must hold (each write, each
call, each return). It writes the predicate as a check at each such point,
and WP proves the checks. The rules, tools/metacsl/rules/:

  handler_written   at each write of every function but contract_set_handler:
                    the location written is not the handler
  handler_kept      when every function but contract_set_handler returns:
                    the handler is the one it was called with
  heap_raw          at each call of every function but mem_alloc and
                    mem_free: the function called is not malloc, calloc,
                    realloc or free
  heap_wrappers     at each call of every function but
                    mem_alloc_array_checked, vec_int_alloc and vec_int_free:
                    the function called is not mem_alloc, mem_free or
                    mem_alloc_array_checked

Each unit of tools/agreement/agreement.py's UNITS is run three times: the
two heap rules together, handler_kept, and handler_written, which takes the
handler's address and so runs alone. A handler rule runs only where the
unit includes contract.h. Each run is a C file of its own, written to
metacsl_out/UNIT/RUN/unit.c, that includes the unit's file and then the
rules, preprocessed with the unit's options. MetAcsl writes checks
(-meta-checks); WP, at the unit's model and with the unit's extra options
(and META_EXTRA's), proves the checks alone (-wp-prop=@check: no unit has
checks of its own), with Alt-Ergo, Z3 and CVC5 at 120 s, the proof jobs'
timeout (since run 1b; 10 s in run 1); Report's table then gives each
check its status.

A check is proved when Report gives it "Valid", or "Partially proven":
proved, with properties of the unit that the run does not prove (its loop
invariants, its callees' contracts), which the unit's proof job does. The
functions whose checks are not proved are pinned, unit by unit and rule by
rule, in tools/metacsl/pins.txt, with why.

Each part posts a notice for each rule, its checks by unit and how many
are proved, and one for the part, with the differences from the pins; the
log lists each check not proved, and WP's lines on its goals not proved.
Report-only in runs 1 and 1b (ENFORCE): a part fails only when a run gives
no table, or is stopped at its limit; enforced, a difference from the pins
fails it too.

    python3 tools/metacsl/metacsl.py check        the units, the rules, the pins
    python3 tools/metacsl/metacsl.py run PART     core, semantics or data
    python3 tools/metacsl/metacsl.py run PART UNIT...

Frama-C runs from the repository's root, with PWD set to it; the outputs
go to metacsl_out/ and notices_metacsl_PART.txt in the directory it is run
from.
"""
import csv
import importlib.util
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
START = os.getcwd()
OUT = os.path.join(START, 'metacsl_out')
PINS = os.path.join(HERE, 'pins.txt')
ENFORCE = False   # VERIFY-086 runs 1 and 1b: report-only
LIMIT = 60 * 60   # seconds, a run
BUDGET = 240 * 60  # seconds, a part: past it no run starts, so that the
                   # notices are written before the job's limit
TIMEOUT = '120'   # seconds, a prover: the proof jobs' (run 1: 10)
CUT = 4000        # the runner cuts an annotation at 4096 characters
PROVED = ('Valid', 'Partially proven')

# The rules: name, file, and how a row of Report's table is known to be one
# of its checks, by the property's kind and text. No unit has checks of its
# own; handler_kept's checks are postconditions, of a text no unit has.
RULES = {
    'handler_written': ('tools/metacsl/rules/handler_written.h',
                        lambda k, t: 'check' in k and 'canon_contract_handler' in t),
    'handler_kept': ('tools/metacsl/rules/handler_kept.h',
                     lambda k, t: 'canon_contract_handler' in t and '\\old' in t),
    'heap_raw': ('tools/metacsl/rules/heap_raw.h',
                 lambda k, t: 'check' in k and re.search(r'\b(malloc|calloc|realloc|free)\b', t) is not None),
    'heap_wrappers': ('tools/metacsl/rules/heap_wrappers.h',
                      lambda k, t: 'check' in k and re.search(
                          r'\b(mem_alloc|mem_free|mem_alloc_array_checked)\b', t) is not None),
}
# The runs of a unit, in order. A handler rule needs the handler declared.
RUNS = [('heap', ['heap_raw', 'heap_wrappers'], False),
        ('kept', ['handler_kept'], True),
        ('written', ['handler_written'], True)]
HANDLER = 'canon_contract_handler'

# MetAcsl's options for a unit, besides the runs' (run 1b). diag's proof
# needs -variadic-no-translation, so its calls of the trusted snprintf and
# fprintf keep their variable arguments; at a call of a function without a
# body MetAcsl's \writing context binds the formals to the arguments, and
# stops on more arguments than formals (run 1: Invalid_argument
# "List.iter2"). -meta-no-check-ext leaves those calls' writes unchecked:
# diag's own writes are checked; its stdio calls write the buffer they are
# given, by the trusted axioms (VERIFY-017's "Trusted stdio axioms").
META_EXTRA = {'frama-c-diag': ['-meta-no-check-ext']}

# The parts, a job each: the units of 4 · Proofs: core/, 5 · Proofs:
# semantics/ and 6 · Proofs: data/.
PART_OF = {
    'frama-c-arena': 'core', 'frama-c-bits': 'core', 'frama-c-checked': 'core',
    'frama-c-compare': 'core', 'frama-c-compare-order': 'core', 'frama-c-lifetime': 'core',
    'frama-c-memory': 'core', 'frama-c-pool': 'core', 'frama-c-ptr': 'core',
    'frama-c-region': 'core', 'frama-c-slice': 'core',
    'frama-c-borrow': 'semantics', 'frama-c-diag': 'semantics', 'frama-c-error': 'semantics',
    'frama-c-option': 'semantics', 'frama-c-result': 'semantics',
    'frama-c-array': 'data', 'frama-c-bitset': 'data', 'frama-c-deque': 'data',
    'frama-c-hashmap': 'data', 'frama-c-priority-queue': 'data', 'frama-c-range': 'data',
    'frama-c-stack': 'data', 'frama-c-stringbuf': 'data', 'frama-c-vec': 'data',
    'frama-c-vec-fmt': 'data', 'frama-c-vec-range': 'data',
}
PARTS = ['core', 'semantics', 'data']


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def agreement():
    return load('agreement', 'tools/agreement/agreement.py')


def shown(text):
    """A tool's output in the log, none of its lines a workflow command."""
    for line in text.splitlines():
        print('| ' + line if line.lstrip().startswith(('::', '##[')) else line)
    sys.stdout.flush()


def pins():
    """{(unit, rule): set of functions} from pins.txt, and its problems."""
    out, problems = {}, []
    with open(PINS, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            words = line.split('#', 1)[0].split()
            if not words:
                continue
            if len(words) < 3:
                problems.append(f'pins.txt:{n}: UNIT RULE FUNCTION... wanted')
                continue
            unit, rule, functions = words[0], words[1], words[2:]
            if unit not in PART_OF:
                problems.append(f'pins.txt:{n}: {unit} is not a unit')
            if rule not in RULES:
                problems.append(f'pins.txt:{n}: {rule} is not a rule')
            if (unit, rule) in out:
                problems.append(f'pins.txt:{n}: {unit} {rule} pinned twice')
            out[(unit, rule)] = set(functions)
    return out, problems


def check():
    ag = agreement()
    problems = []
    units, mine = set(ag.UNITS), set(PART_OF)
    for u in sorted(units - mine):
        problems.append(f'{u}: a unit of agreement.py that PART_OF does not place')
    for u in sorted(mine - units):
        problems.append(f'{u}: in PART_OF, not a unit of agreement.py')
    for u, p in PART_OF.items():
        if p not in PARTS:
            problems.append(f'{u}: part {p} is not one of {PARTS}')
    for u in META_EXTRA:
        if u not in PART_OF:
            problems.append(f'META_EXTRA: {u} is not a unit')
    for rule, (path, _) in RULES.items():
        if not os.path.exists(os.path.join(ROOT, path)):
            problems.append(f'{rule}: {path} missing')
    named = {r for _, rules, _ in RUNS for r in rules}
    if named != set(RULES):
        problems.append(f'the runs name {sorted(named)}, the rules are {sorted(RULES)}')
    with open(os.path.join(ROOT, 'core/primitives/contract.h'), encoding='utf-8') as f:
        text = f.read()
    for word in ('#define CANON_CORE_PRIMITIVES_CONTRACT_H', HANDLER, 'contract_set_handler'):
        if word not in text:
            problems.append(f'contract.h: {word} not found, which the handler rules name')
    pinned, bad = pins()
    problems += bad
    for p in problems:
        print('problem:', p)
    print(f'metacsl check: {len(units)} units in {len(PARTS)} parts, {len(RULES)} rules, '
          f'{sum(len(v) for v in pinned.values())} pinned functions; {len(problems)} problem(s)')
    return 1 if problems else 0


def wrapper(unit, run, rules, source):
    d = os.path.join(OUT, unit, run)
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, 'unit.c')
    with open(path, 'w') as f:
        f.write(f'/* VERIFY-086: {unit}, then the rules of the run {run}. */\n')
        f.write(f'#include "{source}"\n')
        for r in rules:
            f.write(f'#include "{RULES[r][0]}"\n')
    return d, path


def declares_handler(cpp, path):
    """Whether the unit, preprocessed, declares the handler (contract.h)."""
    cmd = ['gcc', '-E', '-x', 'c', '-D__FRAMAC__'] + [w for w in shlex.split(cpp) if w != '-CC'] + [path]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    return r.returncode == 0 and re.search(r'\b' + HANDLER + r'\b', r.stdout) is not None


def meta_options():
    """MetAcsl's options, from its help: whether -meta-checks is there."""
    r = subprocess.run(['frama-c', '-meta-h'], cwd=ROOT, capture_output=True, text=True,
                       env=dict(os.environ, PWD=ROOT))
    text = r.stdout + r.stderr
    print('--- frama-c -meta-h ---')
    shown(text)
    return r.returncode == 0, '-meta-checks' in text


def command(ag, unit, path, csv_path):
    source, model, cpp = ag.UNITS[unit]
    meta = (['-meta', '-meta-checks', '-meta-list-targets', '-meta-warn-key', 'unknown-func=inactive']
            + META_EXTRA.get(unit, []))
    return (['frama-c', f'-cpp-extra-args={cpp}', path] + ag.EXTRA.get(unit, [])
            + meta + ['-then-last', '-wp', '-wp-model', model, '-wp-prop=@check',
                      '-wp-prover', 'alt-ergo,z3,cvc5', '-wp-timeout', TIMEOUT]
            + ['-then-last', '-report-untried', '-report-csv', csv_path])


def execute(cmd, d, limit):
    txt = os.path.join(d, 'frama-c.log')
    print('\n=== ' + ' '.join(shlex.quote(w) for w in cmd), flush=True)
    start = time.time()
    with open(txt, 'w') as f:
        p = subprocess.Popen(cmd, cwd=ROOT, env=dict(os.environ, PWD=ROOT), stdout=f,
                             stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = p.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()
            code = None
    with open(txt, errors='replace') as f:
        return code, f.read(), (time.time() - start) / 60


def table(csv_path):
    if not os.path.exists(csv_path):
        return None
    with open(csv_path, encoding='utf-8', errors='replace') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def one(ag, unit, run, rules, needs_handler, left):
    """A run of a unit: {rule: summary}, or {rule: {'error': ...}}."""
    source, _, cpp = ag.UNITS[unit]
    d, path = wrapper(unit, run, rules, source)
    if needs_handler and not declares_handler(cpp, path):
        return {r: {'stated': False} for r in rules}
    if left < 60:
        return {r: {'stated': True, 'error': f'not run, past the part\'s {BUDGET // 60} minutes'}
                for r in rules}
    limit = int(min(LIMIT, left))
    csv_path = os.path.join(d, 'report.csv')
    if os.path.exists(csv_path):
        os.remove(csv_path)
    code, text, minutes = execute(command(ag, unit, path, csv_path), d, limit)
    rows = table(csv_path)
    if code is None or rows is None:
        shown('\n'.join(text.splitlines()[-30:]))
        why = f'stopped after {limit // 60} minutes' if code is None else f'frama-c exit {code}, no table'
        return {r: {'stated': True, 'error': why, 'minutes': round(minutes, 1)} for r in rules}
    # The rules' rows alone are kept: the whole table, every property of the
    # unit, is the size of the unit.
    keep = [x for x in rows if any(RULES[r][1](x.get('property kind') or '', x.get('property') or '')
                                   for r in rules)]
    with open(os.path.join(d, 'checks.csv'), 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else ['status'], delimiter='\t')
        w.writeheader()
        w.writerows(keep)
    os.remove(csv_path)
    s = ag.summary(text)
    # WP's own lines on the goals it did not prove: a timeout, an unknown.
    wp_lines = [ln for ln in text.splitlines()
                if re.match(r'\[wp\] \[(Timeout|Unknown|Failed|Stepout|Error)\]', ln)]
    out = {}
    for r in rules:
        mine = [x for x in rows if RULES[r][1](x.get('property kind') or '', x.get('property') or '')]
        st = {}
        for x in mine:
            st[x['status']] = st.get(x['status'], 0) + 1
        bad = [x for x in mine if x['status'] not in PROVED]
        out[r] = {'stated': True, 'checks': len(mine), 'statuses': st,
                  'proved': sum(st.get(p, 0) for p in PROVED),
                  'hypotheses': st.get('Partially proven', 0),
                  'not_proved': [f'{x["status"]}: {x["function"]}:{x["line"]} {x["property kind"]}: '
                                 f'{x["property"][:140]}' for x in bad],
                  'functions': sorted({x['function'] for x in bad}),
                  'wp': s[:2] if s else None, 'minutes': round(minutes, 1)}
        wp = f'WP {s[0]} / {s[1]}' if s else 'no WP summary'
        print(f'--- {unit}, {r}: {len(mine)} checks, {st or "none"}; {wp}; {minutes:.1f} minutes')
        if bad:
            shown('\n'.join(out[r]['not_proved'][:300]))
    if wp_lines:
        print(f'--- {unit}, run {run}: WP\'s lines on goals not proved ({len(wp_lines)}) ---')
        shown('\n'.join(wp_lines[:200]))
    return out


def describe(unit, s):
    name = unit.replace('frama-c-', '')
    if not s.get('stated'):
        return None
    if 'error' in s:
        return f'{name} no data ({s["error"]})'
    if not s['checks']:
        return f'{name} no check'
    line = f'{name} {s["proved"]}/{s["checks"]}'
    if s['functions']:
        line += f' (not proved in {", ".join(s["functions"][:8])}' + (
            f', and {len(s["functions"]) - 8} more)' if len(s['functions']) > 8 else ')')
    return line


def differences(results):
    """The functions not proved that the pins do not name, and the pinned
    ones proved or without a check, for the units of results."""
    pinned, _ = pins()
    out = []
    for unit in results:
        for rule, s in results[unit].items():
            if not s.get('stated') or 'error' in s:
                continue
            got, want = set(s['functions']), pinned.get((unit, rule), set())
            out += [f'{unit} {rule} {f}: not proved, not pinned' for f in sorted(got - want)]
            out += [f'{unit} {rule} {f}: pinned, proved or without a check' for f in sorted(want - got)]
    return out


def notices(part, results, diffs, minutes):
    lines = []
    errors = 0
    for rule in RULES:
        parts, unstated = [], []
        total = proved = hyp = 0
        for unit in results:
            s = results[unit][rule]
            d = describe(unit, s)
            if d is None:
                unstated.append(unit.replace('frama-c-', ''))
                continue
            parts.append(d)
            if 'error' in s:
                errors += 1
            else:
                total += s['checks']
                proved += s['proved']
                hyp += s['hypotheses']
        msg = f'{rule}, {part}: checks {total}, proved {proved} ({hyp} under hypotheses); ' + '; '.join(parts)
        if unstated:
            msg += f'; not stated (no contract.h): {" ".join(unstated)}'
        lines.append(f'::notice title=metacsl {rule} {part}::{msg[:CUT]}')
    msg = (f'{part}: {len(results)} units, {len(RULES)} rules; runs without data {errors}; '
           f'differences from the pins {len(diffs)}')
    if diffs:
        msg += ': ' + '; '.join(diffs[:20]) + (f'; and {len(diffs) - 20} more' if len(diffs) > 20 else '')
    lines.append(f'::notice title=metacsl {part}::{msg[:CUT - 40]}; {minutes:.1f} minutes')
    with open(os.path.join(START, f'notices_metacsl_{part}.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    # The workflow's next step posts the file; here, the same lines, not
    # posted twice.
    print('\n'.join(line.split('::', 2)[-1] for line in lines))
    return errors


def run(part, only):
    os.chdir(ROOT)
    ag = agreement()
    units = [u for u in ag.ORDER if PART_OF.get(u) == part]
    units += sorted(u for u in PART_OF if PART_OF[u] == part and u not in units)
    if only:
        units = [u for u in units if u in only or u.replace('frama-c-', '') in only]
    ok, checks = meta_options()
    if not ok or not checks:
        print(f'::error title=metacsl {part}::'
              + ('frama-c -meta-h failed: no MetAcsl' if not ok else 'MetAcsl has no -meta-checks'))
        return 1
    start = time.time()
    results = {}
    for unit in units:
        results[unit] = {}
        for run_name, rules, needs_handler in RUNS:
            left = BUDGET - (time.time() - start)
            results[unit].update(one(ag, unit, run_name, rules, needs_handler, left))
    with open(os.path.join(OUT, f'summary_{part}.json'), 'w') as f:
        json.dump(results, f, indent=1)
    diffs = differences(results)
    if diffs:
        print(f'--- {part}: differences from the pins ({len(diffs)}) ---')
        shown('\n'.join(diffs))
    errors = notices(part, results, diffs, (time.time() - start) / 60)
    if errors:
        print(f'::error title=metacsl {part}::{errors} run(s) of a rule without data')
    if ENFORCE and diffs:
        print(f'::error title=metacsl {part}::{len(diffs)} difference(s) from tools/metacsl/pins.txt')
    return 1 if errors or (ENFORCE and diffs) else 0


def main(argv):
    if argv == ['check']:
        return check()
    if len(argv) >= 2 and argv[0] == 'run' and argv[1] in PARTS:
        return run(argv[1], set(argv[2:]))
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
