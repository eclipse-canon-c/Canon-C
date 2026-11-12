#!/usr/bin/env python3
"""
diagpush_probe.py -- VERIFY-073's probe: semantics/diag.h with diag_push's new
contract and frame-by-frame shift, measured before the header changes.

diag_push's contract kept only the code of the frames before the one it
pushes, so a program proved with WP alone could not call diag_render after two
pushes (VERIFY-066, "Found while reading"). Run 1's patch (variants/
p0_run1.patch, P0) stated the frames kept whole, asked that file and func be
NULL or strings outside *d, promised the new frame renderable, and shifted a
full chain one frame at a time instead of with memmove, whose byte copy WP
cannot follow. WP proved all of it but the promise (runs 1 and 1b), so
diag_push.patch, the change run 2 makes, keeps the rest: the frames kept whole
and the shift (run 1c).

WP runs only in CI. This probe applies a patch to a copy of the checkout and
measures there what the change's run will be compared with, while the enforced
jobs keep reading the header as it is. Its steps:

  variants  diag_push's goals, and its lemma if it has one, with each of
            VERIFY-073's variants (run 1b): P0 and A to D, under variants/.
            And for P0, WP's text of the goals of push_frame_strings, the
            postcondition run 1 left 6 parts of.
  caller    p_caller.c, a program proved with WP alone that pushes two frames
            and renders the chain, with each variant: with -wp-literals, and
            for P0 also without.
  wp        frama-c-diag's command (5 · Proofs: semantics/) with
            diag_push.patch: its summary, and every goal it leaves unproved;
  agree     13 · Prover agreement's unit frama-c-diag with diag_push.patch:
            WP with each prover alone, the goals one or two provers prove, as
            tools/agreement/pins/ lists them, and the comparison with the pins
            as they are;
  eva       14 · Eva's program diag.c at the ten machine models with
            diag_push.patch, each compared with its pin.

The file steps, next to this script, names the steps that run; a step it does
not name posts that it did not run. Without the file, every step runs.

It writes notices for each step and prints the details in the log. The tools it
runs are enforced in their own jobs, where a difference from a pin is an error;
here it is what the probe measures. GitHub reads a workflow command at the
start of a line even behind spaces, so the probe prints each line of theirs
that would be one behind "| ", and posts its own notices. It never fails on
what it measures; it fails only if a patch neither applies nor is found
applied, or a step gives no data.

Each step's files go to diagpush_out/ in the directory it is run from:
variants/ and caller/, each run's output; wp_diag_output.txt;
agree_frama-c-diag_PROVER.txt and .json, and frama-c-diag.txt, the would-be pin
file; eva_out/MODEL/diag.log, .json, .csv.

If a patch is already in the checkout (VERIFY-073's change applied), the probe
measures the checkout as it is.

    python3 tools/probes/verify-073/diagpush_probe.py [variants|caller|wp|agree|eva]...

With no argument it runs every step the file steps names.
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
STEPS = os.path.join(HERE, 'steps')
CALLER = os.path.relpath(os.path.join(HERE, 'p_caller.c'), ROOT)
OUT = os.path.abspath('diagpush_out')
UNIT = 'frama-c-diag'
PROVERS = ['alt-ergo', 'z3', 'cvc5']
ALL = ['variants', 'caller', 'wp', 'agree', 'eva']
CUT = 1000       # a notice's length, as tools/agreement/agreement.py keeps it
STEP = 10        # GitHub keeps ten notices of a step
UNPROVED = r'(Timeout|Unknown|Failed|Failure|Invalid|Stepout)'
PRINTED = 8000   # lines of WP's text of the goals the log shows

# VERIFY-073's variants (run 1b): name, patch, its lemma, what it is.
VARIANTS = [
    ('P0', 'variants/p0_run1.patch', None, "run 1's patch"),
    ('A', 'variants/a_lemma.patch', 'strlen_kept', 'P0 and the lemma strlen_kept'),
    ('B', 'variants/b_nocast.patch', None,
     "P0, with file's and func's separation from *d written without casts"),
    ('C', 'variants/c_lemma_kept.patch', 'strlen_kept',
     'A, and assertions that the bytes of file and func are kept'),
    ('D', 'variants/d_all.patch', 'strlen_kept', 'C and B'),
]

# frama-c-diag's options, as .github/workflows/proofs-semantics.yml runs them.
OPTS = ['-variadic-no-translation', '-wp', '-wp-rte',
        '-wp-model', 'Typed+Cast', '-wp-prover', 'alt-ergo,z3,cvc5',
        '-wp-timeout', '120', '-wp-split',
        '-cpp-extra-args= -I core/primitives -I core -I semantics -I . '
        '-DCANON_NO_REQUIRE -DNDEBUG']
WP_CMD = ['frama-c'] + OPTS + ['semantics/diag.h']


def notice(title, msg):
    print(f'::notice title={title}::{msg[:CUT]}', flush=True)


def shown(text):
    """A tool's output in the log, none of its lines a workflow command."""
    for l in text.splitlines():
        print('| ' + l if l.lstrip().startswith(('::', '##[')) else l)
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


def enabled():
    """The steps the file steps names, or every step without it."""
    if not os.path.exists(STEPS):
        return list(ALL)
    with open(STEPS) as f:
        return [w for line in f for w in line.split('#', 1)[0].split()]


def make_tree(patch=PATCH):
    """A copy of the checkout with the patch applied, and how it was found."""
    tmp = tempfile.mkdtemp(prefix='diagpush_')
    shutil.copytree(ROOT, tmp, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('.git', 'eva_out', 'diagpush_*'))
    fwd = subprocess.run(['patch', '-p1', '--forward', '--dry-run', '-i', patch],
                         cwd=tmp, capture_output=True, text=True)
    if fwd.returncode == 0:
        subprocess.run(['patch', '-p1', '--forward', '-i', patch], cwd=tmp, check=True,
                       stdout=subprocess.DEVNULL)
        return tmp, 'the patch applied to a copy of the checkout'
    rev = subprocess.run(['patch', '-p1', '-R', '--dry-run', '-i', patch],
                         cwd=tmp, capture_output=True, text=True)
    if rev.returncode == 0:
        return tmp, 'the patch already in the checkout'
    print(fwd.stdout + fwd.stderr)
    sys.exit(f'::error title=diagpush::{os.path.basename(patch)} neither applies '
             'nor is in the checkout')


def wp_run(tree, extra, target, out, opts=OPTS):
    """WP on target with frama-c-diag's options and extra; its output kept as
    out under diagpush_out/. Returns the summary ("X / Y", or None), the names
    of the goals not proved, and the output."""
    cmd = ['frama-c'] + opts + extra + [target]
    print(f'Running: {" ".join(cmd)}', flush=True)
    p = subprocess.run(cmd, cwd=tree, capture_output=True, text=True, errors='replace')
    text = p.stdout + p.stderr
    path = os.path.join(OUT, out)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write(text)
    proved = re.search(r'^\[wp\] Proved goals:\s*(\d+ / \d+)', text, re.M)
    lines = [l for l in text.splitlines() if re.match(rf'^\[wp\] \[{UNPROVED}\]', l)]
    names = [re.match(rf'^\[wp\] \[{UNPROVED}\] (?:Goal )?(\S+)', l).group(2) for l in lines]
    print(f'frama-c exit {p.returncode}')
    shown('\n'.join(l for l in text.splitlines() if l.startswith('[wp] Proved goals')
                    or re.match(r'^\s+(Terminating|Unreachable|Qed|Alt-Ergo|Z3|CVC5|'
                                r'Timeout|Unknown|Failed|Missing)', l)))
    shown('\n'.join(lines) or '(every goal proved)')
    if not proved:
        shown('\n'.join(text.splitlines()[-30:]))
    return (proved.group(1) if proved else None), names, text


def step_variants():
    """diag_push's goals with each variant, and its lemma's."""
    summary, unproved = [], []
    for name, rel, lemma, what in VARIANTS:
        print(f'\n=== variants: {name}, {what} ({rel})', flush=True)
        tree, how = make_tree(os.path.join(HERE, rel))
        print(f'{name}: {how}: {tree}', flush=True)
        proved, names, _ = wp_run(tree, ['-wp-fct', 'diag_push'], 'semantics/diag.h',
                                  f'variants/{name}_diag_push.txt')
        if not proved:
            sys.exit(f'::error title=diagpush variants::{name}: frama-c gave no summary')
        said = f'{name}: diag_push {proved}, not proved {len(names)}'
        if lemma:
            print(f'--- {name}: the lemma {lemma}', flush=True)
            lp, ln, _ = wp_run(tree, ['-wp-prop', lemma], 'semantics/diag.h',
                               f'variants/{name}_lemma.txt')
            said += (f'; lemma {lemma} {lp}' if lp else f'; lemma {lemma}: no goal')
            names += ln
        if name == 'P0':
            # WP's text of push_frame_strings' goals, for what the provers are
            # asked; no prover runs.
            opts = [o if o != 'alt-ergo,z3,cvc5' else 'none' for o in OPTS]
            print(f'--- {name}: WP\'s text of push_frame_strings, first {PRINTED} lines',
                  flush=True)
            _, _, text = wp_run(tree, ['-wp-fct', 'diag_push', '-wp-prop', 'push_frame_strings',
                                       '-wp-print'], 'semantics/diag.h',
                                f'variants/{name}_push_frame_strings_print.txt', opts)
            shown('\n'.join(text.splitlines()[:PRINTED]))
        summary.append(said)
        unproved.append((f'{name} not proved ({len(names)})', names))
    for s in summary:
        notice('diagpush variants', s)
    for text in grouped(unproved, STEP - len(summary)):
        notice('diagpush variants names', text)


def step_caller():
    """p_caller.c with each variant, with -wp-literals; P0 also without."""
    runs = [(n, rel, ['-wp-literals']) for n, rel, _, _ in VARIANTS]
    runs.insert(1, ('P0', 'variants/p0_run1.patch', []))
    summary, unproved = [], []
    for name, rel, lit in runs:
        how_lit = 'with -wp-literals' if lit else 'without -wp-literals'
        print(f'\n=== caller: {name}, {how_lit}', flush=True)
        tree, how = make_tree(os.path.join(HERE, rel))
        print(f'{name}: {how}: {tree}', flush=True)
        tag = f'{name}{"" if lit else "_noliterals"}'
        proved, names, _ = wp_run(tree, ['-wp-fct', 'caller'] + lit, CALLER,
                                  f'caller/{tag}.txt')
        if not proved:
            sys.exit(f'::error title=diagpush caller::{tag}: frama-c gave no summary')
        summary.append(f'{name} {how_lit}: caller {proved}, not proved {len(names)}')
        unproved.append((f'{tag} not proved ({len(names)})', names))
    for s in summary:
        notice('diagpush caller', s)
    for text in grouped(unproved, STEP - len(summary)):
        notice('diagpush caller names', text)


def step_wp(tree):
    print(f'\n=== wp: {UNIT}\'s command', flush=True)
    proved, names, text = wp_run(tree, [], 'semantics/diag.h', 'wp_diag_output.txt')
    if not proved:
        sys.exit('::error title=diagpush wp::frama-c gave no summary')
    counts = {}
    for l in text.splitlines():
        m = re.match(rf'^\[wp\] \[{UNPROVED}\]', l)
        if m:
            counts[m.group(1)] = counts.get(m.group(1), 0) + 1
    tally = ', '.join(f'{k} {v}' for k, v in counts.items())
    notice('diagpush wp', f'{UNIT} with the patch: Proved goals: {proved}; '
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
    steps = argv[1:] or enabled()
    bad = [s for s in steps if s not in ALL]
    if bad:
        sys.exit(f'unknown step(s): {" ".join(bad)}; the steps are {", ".join(ALL)}')
    os.makedirs(OUT, exist_ok=True)
    on = enabled()
    tree = None
    for s in steps:
        if s not in on:
            print(f'diagpush: {s} is not in {os.path.relpath(STEPS, ROOT)}; not run', flush=True)
            notice('diagpush', f'{s}: not run; {os.path.relpath(STEPS, ROOT)} names '
                               f'{" ".join(on) or "no step"}')
            continue
        if s in ('variants', 'caller'):
            {'variants': step_variants, 'caller': step_caller}[s]()
            continue
        if tree is None:
            tree, how = make_tree()
            print(f'diagpush: {how}: {tree}', flush=True)
            notice('diagpush', f'{how}; steps: {" ".join(x for x in steps if x in on)}')
        {'wp': step_wp, 'agree': step_agree, 'eva': step_eva}[s](tree)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
