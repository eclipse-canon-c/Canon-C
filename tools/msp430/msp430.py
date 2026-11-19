#!/usr/bin/env python3
"""
msp430.py -- the test suite on MSP430X under the GDB simulator
(VERIFY-096).

MSP430's int has 16 bits, which no other target that runs the tests has.
Each test of CMakeLists.txt is built with TI's msp430-elf-gcc (the Mitto
Systems GCC 9.3.1.11, with its newlib) for the MSP430X CPU, and run under
msp430-elf-run, the GDB simulator that toolchain ships, through newlib's
simulator runtime (-msim), whose system calls carry the output and the
exit status to the host. Two memory models:

    small   -mcpu=msp430x -msmall: 16-bit pointers and size_t; code, data
            and stack below 64 KB (the simulator's msp430-sim.ld).
    large   -mcpu=msp430x -mlarge -mcode-region=either -mdata-region=either:
            20-bit pointers and size_t (__int20, stored in 4 bytes, its
            maximum 0xFFFFF); code and data above 64 KB too
            (msp430xl-sim.ld).

The run prints the memory regions of the model's linker script, as the
toolchain has it.

A size_t below 32 bits is Tier 1's refusal (limits.h): a test that
includes a Tier 1 header is not built, and those that build are the tests
of the headers below it. Each model is built at the four build types of
CMake: Debug (-O0), Release (-O3), RelWithDebInfo (-O2) and MinSizeRel
(-Os), with CANON_LIFETIME_NO_ATOMICS defined: MSP430 has no 64-bit
atomic. A test passes when it exits with status 0. Before the tests, a
probe checks that the simulator carries a program's output and its exit
status, and prints the model's type sizes as the target has them.

The tests that do not pass are pinned, with the reason, in pins.txt: each
line a test, the models and build types it holds for, and what the test
does there, "not-built" or "fails N" (N the lines its output begins with
FAIL). A test the pins do not name must pass. An outcome other than its
pin's is a difference; with ENFORCE, a difference fails the run, unless
the run is --report-only. A run that cannot configure, whose probe fails,
that finds no test, builds none, or builds a binary that is not MSP430X
at the model (its MSPABI attributes), always fails.

    python3 tools/msp430/msp430.py run MODEL [BUILD...] [--report-only]
    python3 tools/msp430/msp430.py check

"run" writes its builds and its summaries to msp430_out/, run from the
repository's root, with the toolchain's bin directory on the PATH. The
steps it shares with tools/cortexm/cortexm.py (CTest's list, a test's run
and its FAIL lines) are that script's. "check" is 8 · Records': each pin
well formed, naming a test of CMakeLists.txt, no two pins for the same
test and leg, and the support files there.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'cortexm'))
import cortexm  # noqa: E402  (the steps both runners share)

PINS = os.path.join(HERE, 'pins.txt')
OUT = 'msp430_out'
ENFORCE = False       # VERIFY-096 run 1: report-only, nothing measured before it
TIMEOUT = 180         # seconds a test, under the simulator
SHOWN = 6             # differences named in a notice

GCC = 'msp430-elf-gcc'
RUN = 'msp430-elf-run'
READELF = 'msp430-elf-readelf'
MODELS = {
    'small': {
        'flags': '-mcpu=msp430x -msmall',
        'attrs': {'Tag_ISA': 'MSP430X', 'Tag_Code_Model': 'Small',
                  'Tag_Data_Model': 'Small'},
        'script': 'msp430-sim.ld',
    },
    'large': {
        'flags': '-mcpu=msp430x -mlarge -mcode-region=either -mdata-region=either',
        'attrs': {'Tag_ISA': 'MSP430X', 'Tag_Code_Model': 'Large',
                  'Tag_Data_Model': 'Large'},
        'script': 'msp430xl-sim.ld',
    },
}
# Defined for every test: lifetime.h's debug counter without atomics.
DEFINES = '-DCANON_LIFETIME_NO_ATOMICS'
BUILDS = cortexm.BUILDS
SUPPORT = ('toolchain.cmake', 'pins.txt', 'README.md')
PIN = re.compile(r'^(\w+)\s+(all|small|large)\s+(\S+)\s+(not-built|fails)(?:\s+(\d+))?$')
PROBE = r'''#include <limits.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    printf("msp430 probe: int %u bytes (INT_MAX %ld), long %u, "
           "size_t %u (SIZE_MAX %lu), pointer %u, double %u, char %s\n",
           (unsigned)sizeof(int), (long)INT_MAX, (unsigned)sizeof(long),
           (unsigned)sizeof(size_t), (unsigned long)SIZE_MAX,
           (unsigned)sizeof(void *), (unsigned)sizeof(double),
           (CHAR_MIN == 0) ? "unsigned" : "signed");
    return 3;
}
'''
PROBE_STATUS = 3

sh, rel, shown, described = cortexm.sh, cortexm.rel, cortexm.shown, cortexm.described


def read_pins():
    """[{line, test, model, builds, outcome, fails}], and the problems."""
    pins, problems = [], []
    with open(PINS, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            m = PIN.match(text)
            if not m:
                problems.append(f'pins.txt:{n}: not "TEST MODELS BUILDS OUTCOME [FAILS]"')
                continue
            test, model, builds, outcome, fails = m.groups()
            builds = BUILDS if builds == 'all' else tuple(builds.split(','))
            if any(b not in BUILDS for b in builds):
                problems.append(f'pins.txt:{n}: build types "all" or of {", ".join(BUILDS)}')
                continue
            if (outcome == 'fails') != (fails is not None):
                problems.append(f'pins.txt:{n}: "fails" takes a count of FAIL lines, '
                                f'"not-built" none')
                continue
            pins.append({'line': n, 'test': test, 'model': model, 'builds': builds,
                         'outcome': outcome,
                         'fails': int(fails) if fails is not None else None})
    return pins, problems


def matching(pins, test, model, build):
    return [p for p in pins if p['test'] == test and p['model'] in ('all', model)
            and build in p['builds']]


def expected(pins, test, model, build):
    m = matching(pins, test, model, build)
    return (m[0]['outcome'], m[0]['fails']) if m else ('passes', None)


def first_line(cmd):
    r = sh(cmd)
    text = (r.stdout or r.stderr).strip()
    return text.splitlines()[0] if r.returncode == 0 and text else f'{cmd[0]}: no version'


def versions():
    out = [first_line([GCC, '--version']), first_line([RUN, '--version'])]
    for tool in (GCC, RUN):
        out.append(f'{tool}: ' + (shutil.which(tool) or 'not on the PATH'))
    return out


def facts(model):
    """newlib's version and its printf's formats, and the model's sizes,
    from GCC's macros."""
    r = sh([GCC, *MODELS[model]['flags'].split(), '-E', '-dM', '-x', 'c', '-'],
           input='#include <stdio.h>\n#include <stdint.h>\n')
    macros = dict(re.findall(r'^#define (\w+)(?: (.*))?$', r.stdout, re.M))
    c99 = 'yes' if '_WANT_IO_C99_FORMATS' in macros else 'no'
    ll = 'yes' if '_WANT_IO_LONG_LONG' in macros else 'no'
    sizes = ', '.join(f'{k} {macros.get(k, "?")}' for k in
                      ('__SIZEOF_INT__', '__SIZEOF_LONG__', '__SIZEOF_POINTER__',
                       '__SIZEOF_SIZE_T__', '__SIZE_MAX__', '__SIZEOF_DOUBLE__',
                       '__SIZEOF_LONG_DOUBLE__'))
    char = 'unsigned' if '__CHAR_UNSIGNED__' in macros else 'signed'
    version = macros.get('_NEWLIB_VERSION', '?').strip('"')
    return (f'newlib {version}; C99 printf formats (_WANT_IO_C99_FORMATS): {c99}; '
            f'long long formats (_WANT_IO_LONG_LONG): {ll}; {sizes}; char {char}')


def memory(model):
    """The MEMORY regions of the model's simulator linker script, as found
    where the toolchain's GCC finds it."""
    script = MODELS[model]['script']
    r = sh([GCC, *MODELS[model]['flags'].split(), f'-print-file-name={script}'])
    path = r.stdout.strip()
    if r.returncode != 0 or not os.path.isabs(path) or not os.path.exists(path):
        return f'{script}: not found'
    with open(path, encoding='utf-8', errors='replace') as f:
        m = re.search(r'\bMEMORY\s*\{(.*?)\}', f.read(), re.S)
    regions = re.findall(r'^\s*(\w+)\s*(?:\([^)]*\))?\s*:\s*ORIGIN\s*=\s*(\w+)\s*,'
                         r'\s*LENGTH\s*=\s*(\w+)', m.group(1) if m else '', re.M)
    return f'{script}: ' + (', '.join(f'{n} {o} +{n2}' for n, o, n2 in regions)
                            or 'no MEMORY regions read')


def probe(model, out):
    """Build and run a program that prints the model's sizes and exits with
    PROBE_STATUS: the simulator must carry both, or no test's result means
    anything. Returns its line of output."""
    d = os.path.abspath(os.path.join(out, 'probe-' + model))
    os.makedirs(d, exist_ok=True)
    src, elf = os.path.join(d, 'probe.c'), os.path.join(d, 'probe')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(PROBE)
    r = sh([GCC, *MODELS[model]['flags'].split(), '-msim', '-std=c99', '-O1', src, '-o', elf])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{model}: the probe does not build')
    try:
        r = subprocess.run([RUN, elf], capture_output=True, text=True, errors='replace',
                           timeout=60)
    except subprocess.TimeoutExpired:
        raise RuntimeError(f'{model}: the probe does not end under {RUN}')
    lines = [line for line in (r.stdout + r.stderr).splitlines() if line.startswith('msp430 probe:')]
    if r.returncode != PROBE_STATUS or not lines:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{model}: under {RUN} the probe exits with {r.returncode}, not '
                           f'{PROBE_STATUS}, or prints no line: the simulator does not carry '
                           f'a program\'s output and exit status')
    return lines[0]


def environment(model):
    env = dict(os.environ)
    env['CANON_MSP_FLAGS'] = MODELS[model]['flags'] + ' ' + DEFINES
    return env


def configure_and_build(model, build, out, env):
    b = os.path.abspath(os.path.join(out, f'{model}-{build}'))
    shutil.rmtree(b, ignore_errors=True)
    r = sh(['cmake', '-S', ROOT, '-B', b, '-G', 'Unix Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE=' + os.path.join(HERE, 'toolchain.cmake'),
            '-DCMAKE_BUILD_TYPE=' + build], env=env)
    if r.returncode != 0:
        shown('\n'.join((r.stdout + r.stderr).splitlines()[-30:]))
        raise RuntimeError(f'{model} {build}: CMake does not configure')
    # -k: the tests that build are built, whatever does not.
    r = sh(['cmake', '--build', b, '--parallel', str(os.cpu_count() or 2), '--', '-k'], env=env)
    with open(b + '.build.log', 'w') as f:
        f.write(r.stdout + r.stderr)
    return b


def why_not_built(b, name, env):
    """The first errors of the test's own build: the compiler's, or the
    linker's, an overflowed memory region among them."""
    r = sh(['cmake', '--build', b, '--target', name], env=env)
    out = []
    for line in (r.stdout + r.stderr).splitlines():
        if (re.search(r'error:|undefined reference|will not fit in region|overflowed by', line)
                and not re.search(r'collect2|ld returned|\*\*\*', line)):
            line = re.sub(r'^\S*/ld: ', '', rel(line.strip()))
            if line not in out:
                out.append(line)
    return out[:3] or ['(no error line found)']


def attributes(b, name, binary, model):
    """The binary's machine and MSPABI attributes, those of its first object
    where the binary has none, and their problems."""
    r = sh([READELF, '-h', binary])
    machine = (re.findall(r'^\s*Machine:\s*(.*?)\s*$', r.stdout, re.M) or ['?'])[0]
    problems = [] if 'msp430' in machine.lower() else [f'its machine is {machine}']
    source = binary
    got = dict(re.findall(r'^\s*(Tag_\w+): (.*?)\s*$', sh([READELF, '-A', binary]).stdout, re.M))
    if 'Tag_ISA' not in got:
        objs = sorted(os.path.join(dp, f) for dp, _, fs in
                      os.walk(os.path.join(b, 'CMakeFiles', name + '.dir'))
                      for f in fs if f.endswith(('.obj', '.o')))
        if objs:
            source = objs[0]
            got = dict(re.findall(r'^\s*(Tag_\w+): (.*?)\s*$',
                                  sh([READELF, '-A', source]).stdout, re.M))
    problems += [f'{k} is {got.get(k, "absent")}, not {v}'
                 for k, v in MODELS[model]['attrs'].items() if got.get(k) != v]
    tags = ', '.join(f'{k} {got[k]}' for k in ('Tag_ISA', 'Tag_Code_Model', 'Tag_Data_Model')
                     if k in got)
    where = '' if source == binary else ' (from its first object)'
    return f'machine {machine}; {tags}{where}', problems


def leg(model, build, out, env, pins):
    """One build type: (notice line, differences, data), or raises."""
    t0 = time.time()
    b = configure_and_build(model, build, out, env)
    tests = cortexm.listed(b)
    if not tests:
        raise RuntimeError(f'{model} {build}: CTest lists no test')
    rows = []
    for t in tests:
        if os.path.exists(t['command'][-1]):
            got = cortexm.run_test(t)
        else:
            got = {'outcome': 'not-built', 'rc': None, 'fails': None, 'fail_lines': [],
                   'why': why_not_built(b, t['name'], env)}
        want = expected(pins, t['name'], model, build)
        rows.append({'name': t['name'], 'got': got, 'want': want})
    built = [t for t in tests if os.path.exists(t['command'][-1])]
    if not built:
        raise RuntimeError(f'{model} {build}: no test built')
    attrs, attr_problems = attributes(b, built[0]['name'], built[0]['command'][-1], model)
    if attr_problems:
        raise RuntimeError(f'{model} {build}: {built[0]["name"]} is not MSP430X at the '
                           f'{model} model: ' + '; '.join(attr_problems))
    names = {t['name'] for t in tests}
    diffs = []
    for r in rows:
        got = (r['got']['outcome'], r['got']['fails'])
        if got != r['want']:
            diffs.append(f'{r["name"]} (pinned {described(*r["want"])}, {described(*got)})')
    for p in pins:
        if p['test'] not in names:
            diffs.append(f'{p["test"]} (pinned at pins.txt:{p["line"]}, not a test)')
    passed = [r for r in rows if r['got']['outcome'] == 'passes']
    failed = [r for r in rows if r['got']['outcome'] in ('fails', 'timeout')]
    unbuilt = [r for r in rows if r['got']['outcome'] == 'not-built']
    minutes = (time.time() - t0) / 60
    line = (f'msp430 {model} {build}: {len(rows)} tests, {len(passed)} passed, '
            f'{len(failed)} failed')
    if failed:
        line += ' (' + ', '.join(f'{r["name"]} {described(r["got"]["outcome"], r["got"]["fails"])}'
                                 .replace(' fails ', ' ') for r in failed) + ')'
    line += f', {len(unbuilt)} not built'
    line += '; as pinned' if not diffs else (
        f'; {len(diffs)} difference(s): ' + ', '.join(diffs[:SHOWN])
        + (f', and {len(diffs) - SHOWN} more' if len(diffs) > SHOWN else ''))
    line += f'; {minutes:.1f} minutes'
    print(line)
    print(f'--- {model} {build}: {built[0]["name"]}: {attrs}')
    print(f'--- {model} {build}: passed: ' + (', '.join(r['name'] for r in passed) or 'none'))
    for r in failed:
        g = r['got']
        print(f'--- {r["name"]}: {g["outcome"]}, exit {g["rc"]}, '
              f'{len(g["fail_lines"])} FAIL line(s) ---')
        shown('\n'.join(g['fail_lines'][:25] + (['...'] if len(g['fail_lines']) > 25 else [])
                        + ['(the last other lines)'] + g['tail']))
    reasons = {}
    for r in unbuilt:
        reasons.setdefault(r['got']['why'][0], []).append(r['name'])
    for why, which in sorted(reasons.items(), key=lambda kv: -len(kv[1])):
        print(f'--- not built ({len(which)}): {", ".join(which)}')
        shown(why)
    if diffs:
        print(f'--- {model} {build}: differences from the pins ({len(diffs)}) ---')
        shown('\n'.join(diffs))
    data = {'model': model, 'build': build, 'minutes': round(minutes, 1),
            'tests': len(rows), 'passed': [r['name'] for r in passed],
            'failed': {r['name']: r['got']['fails'] for r in failed},
            'not_built': {r['name']: r['got']['why'] for r in unbuilt},
            'differences': diffs, 'attributes': attrs, 'line': line}
    with open(b + '.json', 'w') as f:
        json.dump(data, f, indent=1)
    return line, diffs, data


def check():
    pins, problems = read_pins()
    tests = set(cortexm.cmake_tests())
    for p in pins:
        if p['test'] not in tests:
            problems.append(f'pins.txt:{p["line"]}: {p["test"]} is not a test of CMakeLists.txt')
    for model in MODELS:
        for build in BUILDS:
            for t in sorted({p['test'] for p in pins}):
                m = matching(pins, t, model, build)
                if len(m) > 1:
                    problems.append(f'pins.txt:{m[0]["line"]},{m[1]["line"]}: {t} pinned '
                                    f'twice for {model} {build}')
    for f in SUPPORT:
        if not os.path.exists(os.path.join(HERE, f)):
            problems.append(f'tools/msp430/{f}: no such file')
    for p in problems:
        print('  ' + p)
    print(f'msp430 check: {len(pins)} pins, {len(tests)} tests in CMakeLists.txt; '
          f'{len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if argv[:1] == ['check'] and len(argv) == 1:
        return check()
    report_only = '--report-only' in argv
    args = [a for a in argv if a != '--report-only']
    if len(args) < 2 or args[0] != 'run' or args[1] not in MODELS \
            or any(b not in BUILDS for b in args[2:]):
        print(__doc__)
        return 2
    model, builds = args[1], args[2:] or list(BUILDS)
    cortexm.TIMEOUT = TIMEOUT   # cortexm.run_test's limit; the simulator is slower than QEMU
    for v in versions():
        print(v)
    print(f'C library and model: {facts(model)}')
    print(f'Model: {model}; flags: {MODELS[model]["flags"]} {DEFINES}')
    print(f'Memory: {memory(model)}')
    pins, problems = read_pins()
    os.makedirs(OUT, exist_ok=True)
    all_diffs = list(problems)
    try:
        if not shutil.which(GCC) or not shutil.which(RUN):
            raise RuntimeError(f'{GCC} and {RUN} must be on the PATH')
        line = probe(model, OUT)
        print(line)
        print(f'::notice title=msp430 {model} probe::{line}')
        env = environment(model)
        for build in builds:
            line, diffs, _ = leg(model, build, OUT, env, pins)
            print(f'::notice title=msp430 {model} {build}::{line}')
            all_diffs += diffs
    except RuntimeError as e:
        print(f'::error title=msp430 {model}::{e}')
        return 1
    if all_diffs and ENFORCE and not report_only:
        print(f'::error title=msp430 {model}::{len(all_diffs)} difference(s) from '
              f'tools/msp430/pins.txt')
        return 1
    if all_diffs:
        print(f'REPORT-ONLY: {len(all_diffs)} difference(s) from the pins, not failing '
              f'({"--report-only" if report_only else "ENFORCE is off"})')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
