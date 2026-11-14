#!/usr/bin/env python3
"""
metrics.py -- Canon-C's functions as Frama-C's Metrics plug-in measures
them, header by header (VERIFY-081).

Metrics counts, for each function, its statements (Sloc), decision points,
ifs, loops, gotos, assignments, exit points, calls and pointer
dereferences, and its cyclomatic complexity, on the code as Frama-C
normalizes it (the CIL AST). These are syntactic: nothing is proved.

The functions measured are those of tools/size/size.py, read from its
units, so that the two tools measure the same thing: each public header
alone, a generic module instantiated for u32 (the hashmap for a u64 key
and an int value), a function counted for the header whose module defines
it, the instantiation's functions found as the difference from the unit
without it, and Result<_Bool, Error>'s functions in a row of their own.
The setting is as shipped for release: NDEBUG, so require() is on and
ensure() off, at Frama-C's default machine model (x86_64).

tools/metrics/pins.txt holds each function's cyclomatic complexity. run
compares what it measures with them: a function whose complexity changed,
or one measured and not pinned, or pinned and not measured, is a
difference. A difference fails the job once ENFORCE is set; VERIFY-081's
run 1 only reports. The other counts are printed and written to the CSV
file, not pinned. A complexity changes only with the change that moves
it, pinned again with `pin` from that change's run.

    python3 tools/metrics/metrics.py run [OUT]   measure every header, and compare
                                                 with the pins; the table to stdout,
                                                 metrics.csv to OUT (default metrics_out)
    python3 tools/metrics/metrics.py pin DIR     write pins.txt from DIR/metrics.csv
    python3 tools/metrics/metrics.py check       pins.txt is well formed, its headers exist

Needs frama-c on PATH for run.
"""
import concurrent.futures
import csv
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PINS = os.path.join(HERE, 'pins.txt')
ENFORCE = False   # VERIFY-081 run 1: report-only
CUT = 4000        # the runner cuts an annotation at 4096 characters

# Metrics' labels, and the CSV's columns for them.
LABELS = {
    'Sloc': 'sloc',
    'Decision point': 'decisions',
    'If': 'ifs',
    'Loop': 'loops',
    'Goto': 'gotos',
    'Assignment': 'assignments',
    'Exit point': 'exits',
    'Function call': 'calls',
    'Pointer dereferencing': 'derefs',
    'Cyclomatic complexity': 'cyclomatic',
}
FIELDS = list(LABELS.values())
BANDS = ((1, 1, '1'), (2, 5, '2 to 5'), (6, 10, '6 to 10'), (11, 20, '11 to 20'),
         (21, None, 'over 20'))
STATS = re.compile(r'Stats for function <(.+)/([^/<>]+)>')
FIELD = re.compile(r'^\s*([A-Za-z ]+?) = (\d+)\s*$')


def load_size():
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location(
        'size', os.path.join(ROOT, 'tools', 'size', 'size.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def notice(title, msg):
    print(f'::notice title={title}::{msg[:CUT]}', flush=True)


def metrics_of(text, tmp):
    """{function: (file, {field: value})} for the unit text: the file
    relative to the repository, or '<unit>' for the unit itself."""
    os.makedirs(tmp, exist_ok=True)
    unit = os.path.join(tmp, 'canon_tu.c')
    with open(unit, 'w') as f:
        f.write(text)
    # -no-annot: Metrics reads no annotation, and some headers' refer to
    # names only their verification drivers define.
    cmd = ['frama-c', '-c11', '-no-annot', f'-cpp-extra-args=-I{ROOT} -DNDEBUG',
           '-metrics', '-metrics-by-function', unit]
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    out = p.stdout + p.stderr
    if p.returncode != 0:
        raise RuntimeError(f'frama-c exit {p.returncode}: ' + ' | '.join(out.splitlines()[-8:]))
    fns, cur, label = {}, None, None
    for line in out.splitlines():
        # Frama-C wraps a long label onto the lines after it: join them.
        if label is not None:
            label += line.strip()
            if '>' not in line:
                continue
            line, label = label, None
        elif 'Stats for function <' in line and '>' not in line:
            label = line.strip()
            continue
        m = STATS.search(line)
        if m:
            path = os.path.normpath(os.path.join(ROOT, m.group(1)))
            if path == os.path.normpath(unit):
                path = '<unit>'
            else:
                path = os.path.relpath(path, ROOT).replace(os.sep, '/')
            cur = {}
            fns[m.group(2)] = (path, cur)
            continue
        m = FIELD.match(line)
        if m and cur is not None and m.group(1) in LABELS:
            cur[LABELS[m.group(1)]] = int(m.group(2))
            if m.group(1) == 'Cyclomatic complexity':
                # A function's block ends here. Frama-C then prints the whole
                # unit's totals in the same form, which no function owns.
                cur = None
    for name, (_, vals) in fns.items():
        if set(vals) != set(FIELDS):
            raise RuntimeError(f'{name}: Metrics gave {sorted(vals)}, not every field')
    return fns


def measure_header(size, header, tmp):
    """(own, shared): the header's functions, and the shared type's."""
    unit, base = size.units(header)
    fns = metrics_of(unit, os.path.join(tmp, 'unit'))
    base_fns = metrics_of(base, os.path.join(tmp, 'base')) if base is not None else None
    own, shared = {}, {}
    for name, (path, vals) in fns.items():
        if name.startswith(size.SHARED + '_'):
            shared[name] = vals
        elif size.owns(header, path) or (path == '<unit>' and base_fns is not None
                                         and name not in base_fns):
            own[name] = vals
    return own, shared


def band(c):
    return next(label for lo, hi, label in BANDS if c >= lo and (hi is None or c <= hi))


def read_pins():
    """({(row, function): cyclomatic}, problems)."""
    pins, problems = {}, []
    if not os.path.exists(PINS):
        return pins, ['tools/metrics/pins.txt: missing']
    with open(PINS) as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            cells = text.split()
            if cells == ['header', 'function', 'cyclomatic']:
                continue
            if len(cells) != 3 or not cells[2].isdigit():
                problems.append(f'pins.txt:{n}: not "header function cyclomatic": {line.strip()}')
                continue
            key = (cells[0], cells[1])
            if key in pins:
                problems.append(f'pins.txt:{n}: {cells[0]} {cells[1]} pinned twice')
            pins[key] = int(cells[2])
    return pins, problems


def write_pins(cur):
    rows = sorted(cur)
    w0 = max(len(r) for r, _ in rows)
    w1 = max(len(f) for _, f in rows)
    with open(PINS, 'w') as out:
        out.write('# Each function\'s cyclomatic complexity, as Frama-C\'s Metrics gives it for\n'
                  '# the code as shipped (NDEBUG), header by header as tools/size/size.py\n'
                  '# measures them (VERIFY-081). Written by `metrics.py pin` from a run\'s\n'
                  '# metrics.csv; compared by `metrics.py run` in 2 · Platforms\' metrics job.\n')
        out.write(f'{"header".ljust(w0)} {"function".ljust(w1)} cyclomatic\n')
        for r, fn in rows:
            out.write(f'{r.ljust(w0)} {fn.ljust(w1)} {cur[(r, fn)]:>10}\n')


def compare(cur):
    pins, problems = read_pins()
    diffs = list(problems)
    for key in sorted(set(cur) | set(pins)):
        r, fn = key
        if key not in pins:
            diffs.append(f'{r} {fn}: measured {cur[key]}, no pin')
        elif key not in cur:
            diffs.append(f'{r} {fn}: pinned {pins[key]}, not measured')
        elif pins[key] != cur[key]:
            diffs.append(f'{r} {fn}: pinned {pins[key]}, measured {cur[key]}')
    return diffs


def run(out_dir):
    size = load_size()
    os.makedirs(out_dir, exist_ok=True)
    headers = size.headers()
    tmp = tempfile.mkdtemp(prefix='canon_metrics_')
    results, failed = {}, {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as pool:
        jobs = {pool.submit(measure_header, size, h, os.path.join(tmp, str(k))): h
                for k, h in enumerate(headers)}
        for job in concurrent.futures.as_completed(jobs):
            h = jobs[job]
            try:
                results[h] = job.result()
            except RuntimeError as e:
                failed[h] = str(e)
    shutil.rmtree(tmp, ignore_errors=True)

    rows, shared, clash = {}, {}, []
    for h in headers:
        if h not in results:
            continue
        own, sh = results[h]
        for fn, vals in own.items():
            rows[(h, fn)] = vals
        for fn, vals in sh.items():
            if fn in shared and shared[fn] != vals:
                clash.append(fn)
            shared.setdefault(fn, vals)
    for fn, vals in shared.items():
        rows[(size.SHARED_ROW, fn)] = vals

    with open(os.path.join(out_dir, 'metrics.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['header', 'function'] + FIELDS)
        for (h, fn) in sorted(rows):
            w.writerow([h, fn] + [rows[(h, fn)][c] for c in FIELDS])

    # The table: one line a header.
    print(f'{"header":<36} {"functions":>9} {"sloc":>6} {"max cyclomatic":>14}  the most complex')
    for h in sorted({h for h, _ in rows}):
        fns = {fn: v for (r, fn), v in rows.items() if r == h}
        top = max(fns, key=lambda fn: (fns[fn]['cyclomatic'], fn))
        print(f'{h:<36} {len(fns):>9} {sum(v["sloc"] for v in fns.values()):>6} '
              f'{fns[top]["cyclomatic"]:>14}  {top}')
    for h, why in sorted(failed.items()):
        print(f'{h}: not measured: {why}')
    for fn in clash:
        print(f'{fn}: the shared type\'s function measured differently by two headers')

    cyc = {k: v['cyclomatic'] for k, v in rows.items()}
    counts = {label: 0 for _, _, label in BANDS}
    for c in cyc.values():
        counts[band(c)] += 1
    ranked = sorted(cyc, key=lambda k: (-cyc[k], k))
    highest = '; '.join(f'{fn} ({h}) {cyc[(h, fn)]}' for h, fn in ranked[:10])
    notice('metrics',
           f'metrics: {len(cyc)} functions of {len(results)} of {len(headers)} headers, as shipped '
           f'(NDEBUG); cyclomatic complexity ' + '; '.join(f'{b}: {counts[b]}' for _, _, b in BANDS)
           + f'; statements {sum(v["sloc"] for v in rows.values())} in all')
    notice('metrics highest', f'metrics, the ten most complex: {highest}')

    diffs = compare(cyc)
    if failed:
        notice('metrics', 'not measured: ' + '; '.join(f'{h}: {why}' for h, why in sorted(failed.items())))
    if clash:
        notice('metrics', 'the shared type measured differently: ' + ' '.join(clash))
    if diffs:
        print('\n'.join(diffs))
        notice('metrics pins', f'metrics pins: {len(diffs)} difference(s)'
               + ('' if ENFORCE else ', report-only') + ': ' + '; '.join(diffs))
    else:
        notice('metrics pins', f'metrics pins: as pinned, {len(cyc)} functions')
    if failed or clash:
        return 1
    return 1 if diffs and ENFORCE else 0


def pin(src):
    path = os.path.join(src, 'metrics.csv')
    cur = {}
    with open(path) as f:
        for row in csv.DictReader(f):
            cur[(row['header'], row['function'])] = int(row['cyclomatic'])
    write_pins(cur)
    print(f'pins.txt: {len(cur)} functions, from {path}')
    return 0


def check():
    size = load_size()
    pins, problems = read_pins()
    for h in sorted({h for h, _ in pins}):
        if h != size.SHARED_ROW and not os.path.exists(os.path.join(ROOT, h)):
            problems.append(f'{h}: no such header')
    for p in problems:
        print(p)
    print(f'metrics pins: {len(pins)} functions in {len({h for h, _ in pins})} rows; '
          f'{len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if len(argv) >= 1 and argv[0] == 'run':
        return run(argv[1] if len(argv) > 1 else 'metrics_out')
    if len(argv) == 2 and argv[0] == 'pin':
        return pin(argv[1])
    if argv == ['check']:
        return check()
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
