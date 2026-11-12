#!/usr/bin/env python3
"""
diagpush_probe.py -- VERIFY-073's probe: semantics/diag.h with diag_push's new
contract and frame-by-frame shift, measured before the header changes.

diag_push's contract kept only the code of the frames before the one it
pushes, so a program proved with WP alone could not call diag_render after two
pushes (VERIFY-066, "Found while reading"). diag_push.patch states the frames
kept whole, asks that file and func be NULL or strings outside *d, promises the
new frame renderable, and shifts a full chain one frame at a time instead of
with memmove, whose byte copy WP cannot follow.

WP runs only in CI. This probe applies the patch to a copy of the checkout and
measures there what the change's run will be compared with, while the enforced
jobs keep reading the header as it is:

  wp     frama-c-diag's command (5 · Proofs: semantics/): its summary, and
         every goal it leaves unproved, by name;
  agree  13 · Prover agreement's unit frama-c-diag: WP with each prover alone,
         the goals one or two provers prove, as tools/agreement/pins/ lists
         them, and the comparison with the pins as they are;
  eva    14 · Eva's program diag.c at the ten machine models, each compared
         with its pin.

It writes notices for each step and prints the details in the log: the
unproved names, the would-be pin file of frama-c-diag, and each Eva block's
difference from its pin. The tools it runs are enforced in their own jobs, so
their workflow commands are shown here as plain text: a difference from a pin
is what the probe measures, not an error of its job. It never fails on what it
measures; it fails only if the patch neither applies nor is found applied, or
a step gives no data.

Each step's files go to diagpush_out/ in the directory it is run from:
wp_diag_output.txt; agree_frama-c-diag_PROVER.txt and .json, and
frama-c-diag.txt, the would-be pin file; eva_out/MODEL/diag.log, .json, .csv.

If the patch is already in the checkout (VERIFY-073's change applied), the
probe measures the checkout as it is.

    python3 tools/probes/verify-073/diagpush_probe.py [wp|agree|eva]...

With no argument it runs the three steps.
"""
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
PATCH = os.path.join(HERE, 'diag_push.patch')
OUT = os.path.abspath('diagpush_out')
UNIT = 'frama-c-diag'
PROVERS = ['alt-ergo', 'z3', 'cvc5']
CUT = 1000       # a notice's length, as tools/agreement/agreement.py keeps it
STEP = 10        # GitHub keeps ten notices of a step
UNPROVED = r'(Timeout|Unknown|Failed|Failure|Invalid|Stepout)'

# frama-c-diag's command, as .github/workflows/proofs-semantics.yml runs it.
WP_CMD = ['frama-c', '-variadic-no-translation', '-wp', '-wp-rte',
          '-wp-model', 'Typed+Cast', '-wp-prover', 'alt-ergo,z3,cvc5',
          '-wp-timeout', '120', '-wp-split',
          '-cpp-extra-args= -I core/primitives -I core -I semantics -I . '
          '-DCANON_NO_REQUIRE -DNDEBUG',
          'semantics/diag.h']


def notice(title, msg):
    print(f'::notice title={title}::{msg[:CUT]}', flush=True)


def shown(text):
    """A tool's output in the log, its workflow commands as plain text."""
    for l in text.splitlines():
        print('  ' + l if l.startswith('::') else l)
    sys.stdout.flush()


def grouped(groups, room, sep=' '):
    """[(head, items)] as texts "head: item item ..." of at most CUT characters,
    at most room of them; the last says how many more there were."""
    texts = []
    for head, items in groups:
        cur = []
        for n in items:
            if cur and len(f'{head}: ' + sep.join(cur + [n])) > CUT:
                texts.append(f'{head}: ' + sep.join(cur))
                cur = []
            cur.append(n)
        if cur:
            texts.append(f'{head}: ' + sep.join(cur))
    if len(texts) > room:
        left = len(texts) - room + 1
        texts = texts[:room - 1] + [f'{left} more notice(s) of these, in the log and in diagpush_out/']
    return texts


def keep(src, *where):
    """A copy of src under diagpush_out/, if src exists."""
    if os.path.exists(src):
        dst = os.path.join(OUT, *where)
        os.makedirs(dst, exist_ok=True)
        shutil.copy(src, dst)


def make_tree():
    """A copy of the checkout with the patch applied, and how it was found."""
    tmp = tempfile.mkdtemp(prefix='diagpush_')
    shutil.copytree(ROOT, tmp, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('.git', 'eva_out', 'diagpush_*'))
    fwd = subprocess.run(['patch', '-p1', '--forward', '--dry-run', '-i', PATCH],
                         cwd=tmp, capture_output=True, text=True)
    if fwd.returncode == 0:
        subprocess.run(['patch', '-p1', '--forward', '-i', PATCH], cwd=tmp, check=True,
                       stdout=subprocess.DEVNULL)
        return tmp, 'the patch applied to a copy of the checkout'
    rev = subprocess.run(['patch', '-p1', '-R', '--dry-run', '-i', PATCH],
                         cwd=tmp, capture_output=True, text=True)
    if rev.returncode == 0:
        return tmp, 'the patch already in the checkout'
    print(fwd.stdout + fwd.stderr)
    sys.exit('::error title=diagpush::the patch neither applies nor is in the checkout')


def step_wp(tree):
    print(f'\n=== wp: {UNIT}\'s command\nRunning: {" ".join(WP_CMD)}', flush=True)
    p = subprocess.run(WP_CMD, cwd=tree, capture_output=True, text=True, errors='replace')
    text = p.stdout + p.stderr
    out = os.path.join(tree, 'wp_diag_output.txt')
    with open(out, 'w') as f:
        f.write(text)
    keep(out)
    proved = re.search(r'^\[wp\] Proved goals:\s*(\d+ / \d+)', text, re.M)
    if not proved:
        print('\n'.join(text.splitlines()[-30:]))
        sys.exit('::error title=diagpush wp::frama-c gave no summary')
    lines = [l for l in text.splitlines() if re.match(rf'^\[wp\] \[{UNPROVED}\]', l)]
    names = [re.match(rf'^\[wp\] \[{UNPROVED}\] (?:Goal )?(\S+)', l).group(2) for l in lines]
    counts = {}
    for l in lines:
        k = re.match(rf'^\[wp\] \[{UNPROVED}\]', l).group(1)
        counts[k] = counts.get(k, 0) + 1
    print(f'frama-c exit {p.returncode}')
    print('\n'.join(l for l in text.splitlines() if l.startswith('[wp] Proved goals')
                    or re.match(r'^\s+(Terminating|Unreachable|Qed|Alt-Ergo|Z3|CVC5|'
                                r'Timeout|Unknown|Failed|Missing)', l)))
    print('--- every goal not proved ---')
    print('\n'.join(lines) or '(none)')
    tally = ', '.join(f'{k} {v}' for k, v in counts.items())
    notice('diagpush wp', f'{UNIT} with the patch: Proved goals: {proved.group(1)}; '
                          f'not proved {len(names)} ({tally or "none"})')
    for text in grouped([(f'not proved ({len(names)})', names)], STEP - 2):
        notice('diagpush wp names', text)


def load_tool(tree, *path):
    spec = importlib.util.spec_from_file_location(path[-1][:-3], os.path.join(tree, *path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def step_agree(tree):
    script = os.path.join(tree, 'tools', 'agreement', 'agreement.py')
    for prover in PROVERS:
        print(f'\n=== agree: {UNIT}, {prover} alone', flush=True)
        p = subprocess.run([sys.executable, script, 'run', UNIT, prover], cwd=tree,
                           capture_output=True, text=True, errors='replace')
        shown(p.stdout + p.stderr)
        for ext in ('txt', 'json'):
            keep(os.path.join(tree, f'agree_{UNIT}_{prover}.{ext}'))
        said = re.search(rf'^{re.escape(UNIT)}, .+ alone: .+$', p.stdout, re.M)
        if p.returncode != 0 or not said:
            sys.exit(f'::error title=diagpush agree::{prover}: WP gave no summary or no report')
        notice(f'diagpush agree {prover}', said.group(0))
    print(f'\n=== agree: {UNIT} against its pins as they are', flush=True)
    p = subprocess.run([sys.executable, script, 'unit', UNIT, tree], cwd=tree,
                       capture_output=True, text=True, errors='replace')
    shown(p.stdout + p.stderr)
    agreement = load_tool(tree, 'tools', 'agreement', 'agreement.py')
    runs = agreement.collect(tree, UNIT)
    if UNIT not in runs or len(runs[UNIT]) != len(PROVERS):
        sys.exit('::error title=diagpush agree::not all three runs reported')
    o = agreement.sort_goals(runs[UNIT])
    order = {'one': 0, 'two': 1}
    lines = sorted((f'{kind} {provers} {g}' for g, (kind, provers) in o['cls'].items()),
                   key=lambda l: (order[l.split()[0]], l.split()[1], l.split()[2]))
    slow = sorted(f'slow {"+".join(sorted(ts))} {g}' for g, ts in o['slow'].items())
    with open(os.path.join(OUT, f'{UNIT}.txt'), 'w') as f:
        f.write('\n'.join(lines + slow) + '\n')
    print(f'--- the would-be tools/agreement/pins/{UNIT}.txt (without its header) ---')
    print('\n'.join(lines + slow))
    print('--- end ---')
    print(f'--- proved by no prover alone ({len(o["none"])}) ---')
    print('\n'.join(o['none']) or '(none)')
    # Every goal of diag_push, the function the patch changes, as each prover
    # alone left it: which are new, which are gone, which moved class.
    push = [g for g in o['ids'] if '_diag_push_' in g]
    print(f'--- diag_push\'s goals in WP\'s report ({len(push)}), and how they are proved ---')
    for g in push:
        took = []
        for p in PROVERS:
            goals, name = runs[UNIT][p][0], agreement.NAMES[p]
            if g in goals and name in goals[g][1]:
                took.append(f'{name} {goals[g][1][name]:.1f} s')
        print(f'{g}: {agreement.state(o, g)}' + (f' ({", ".join(took)})' if took else ''))
    notice('diagpush agree', f'{UNIT} with the patch: {len(o["ids"])} goals in WP\'s report; '
                             f'by WP itself {len(o["wp"])}, three provers {len(o["three"])}, '
                             f'two {o["n_two"]}, one {o["n_one"]}, none {len(o["none"])}; '
                             f'slow (over {agreement.SLOW:.0f} s) {len(o["slow"])}')
    groups = []
    for kind in ('one', 'two'):
        by = {}
        for g, (k, provers) in o['cls'].items():
            if k == kind:
                by.setdefault(provers, []).append(g)
        groups += [(f'{kind} {p} ({len(by[p])})', sorted(by[p])) for p in sorted(by)]
    by = {}
    for g, ts in o['slow'].items():
        by.setdefault('+'.join(sorted(ts)), []).append(g)
    groups += [(f'slow {p} ({len(by[p])})', sorted(by[p])) for p in sorted(by)]
    groups.append((f'none ({len(o["none"])})', sorted(o['none'])))
    # Of a step's ten: one for the tree, three for the provers, one for the counts.
    for text in grouped(groups, STEP - 5):
        notice('diagpush agree names', text)


def step_eva(tree):
    script = os.path.join(tree, 'tools', 'eva', 'eva.py')
    eva = load_tool(tree, 'tools', 'eva', 'eva.py')
    summary = []
    for model in eva.MODELS:
        print(f'\n=== eva: diag.c at {model}', flush=True)
        p = subprocess.run([sys.executable, script, 'run', model, 'diag'], cwd=tree,
                           capture_output=True, text=True, errors='replace')
        shown(p.stdout + p.stderr)
        for ext in ('log', 'json', 'csv'):
            keep(os.path.join(tree, 'eva_out', model, f'diag.{ext}'), 'eva_out', model)
        said = re.search(r'^diag @ .*$', p.stdout, re.M)
        if not said or not re.search(r'^pins: ', p.stdout, re.M):
            sys.exit(f'::error title=diagpush eva::{model}: eva.py gave no summary')
        pre = re.search(r'preconditions (\d+/\d+/\d+)', said.group(0))
        gone = len(re.findall(r'^  pin \| -(?!-- pinned)', p.stdout, re.M))
        came = len(re.findall(r'^  pin \| \+(?!\+\+ this run)', p.stdout, re.M))
        pin = ('as pinned' if said.group(0).endswith('; as pinned')
               else f'not as pinned, {gone} line(s) of the pin out and {came} in')
        summary.append(f'{model}: {pin}, preconditions '
                       f'{pre.group(1) if pre else "none: Eva did not finish"}')
    for text in grouped([('diag.c', summary)], STEP - 1, '; '):
        notice('diagpush eva', text)


def main(argv):
    steps = argv[1:] or ['wp', 'agree', 'eva']
    bad = [s for s in steps if s not in ('wp', 'agree', 'eva')]
    if bad:
        sys.exit(f'unknown step(s): {" ".join(bad)}; the steps are wp, agree and eva')
    os.makedirs(OUT, exist_ok=True)
    tree, how = make_tree()
    print(f'diagpush: {how}: {tree}', flush=True)
    notice('diagpush', f'{how}; steps: {" ".join(steps)}')
    for s in steps:
        {'wp': step_wp, 'agree': step_agree, 'eva': step_eva}[s](tree)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
