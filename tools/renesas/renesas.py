#!/usr/bin/env python3
"""
renesas.py -- the test suite on Renesas cores under GDB's simulators, with
GCC and newlib built from Ubuntu's source packages (VERIFY-101).

tools/renesas/build.sh builds the toolchain of a target from the release
tarballs of Ubuntu's binutils-source, gcc-13-source, newlib-source and
gdb-source: binutils 2.42, GCC 13.3.0, newlib 4.4.0, and the simulator of
GDB 15.1, TARGET-run. Each test of CMakeLists.txt is built with that GCC
and newlib and run under the simulator, which carries its output and its
exit status through newlib's system calls. The variants:

    rh850       v850-elf, -mv850e3v5: RH850's V850E3V5 instruction set,
                Renesas' RH850 ABI (GCC's default), its FPU.
    rh850-soft  the same with -msoft-float: no FPU, float and double in
                libgcc's software.

Each is built at the four build types of CMake: Debug (-O0), Release
(-O3), RelWithDebInfo (-O2) and MinSizeRel (-Os), with
CANON_LIFETIME_NO_ATOMICS defined: the V850 has no 64-bit atomic, and its
libgcc no __atomic_fetch_add_8. A test passes when it exits with status 0,
within TIMEOUT seconds. Before the tests, a probe checks that the
simulator carries a program's output and its exit status, and prints the
type sizes and alignments as the target has them.

The tests that do not pass are pinned, with the reason, in pins.txt: each
line a test, the variants and build types it holds for, and what the test
does there: "not-built", "fails N" (N the lines its output begins with
FAIL), or "times-out", stopped at PINNED_TIMEOUT seconds. A test the pins
do not name must pass. An outcome other than its pin's is a difference;
with ENFORCE, a difference fails the run, unless the run is --report-only.
A run that cannot configure, whose probe fails, that finds no test, builds
none, or builds a binary not of the variant's machine and flags, always
fails.

    python3 tools/renesas/renesas.py run VARIANT [BUILD...] [--report-only]
    python3 tools/renesas/renesas.py check

"run" needs the variant's toolchain on the PATH (build.sh's PREFIX/bin),
and writes its builds and summaries to renesas_out/. The steps it shares
with tools/msp430/msp430.py (a test's run, its output kept when it is
stopped, why a test is not built) and tools/cortexm/cortexm.py (CTest's
list, FAIL lines) are those scripts'. "check" is 8 · Records': each pin
well formed, naming a test of CMakeLists.txt, no two pins for the same
test and leg, and the support files there.
"""
import json
import os
import re
import shutil
import sys
import time

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'cortexm'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'msp430'))
import cortexm  # noqa: E402  (the steps the runners share)
import msp430   # noqa: E402

PINS = os.path.join(HERE, 'pins.txt')
OUT = 'renesas_out'
ENFORCE = True        # from VERIFY-101's run 1: its pins measured before it
TIMEOUT = 300         # seconds a test, under the simulator
PINNED_TIMEOUT = 60   # seconds a test pinned to time out
SHOWN = 6             # differences named in a notice

VARIANTS = {
    'rh850': {'target': 'v850-elf', 'flags': '-mv850e3v5', 'fpu': 'FPU-3.0'},
    'rh850-soft': {'target': 'v850-elf', 'flags': '-mv850e3v5 -msoft-float', 'fpu': 'not set'},
}
DEFINES = '-DCANON_LIFETIME_NO_ATOMICS'
BUILDS = cortexm.BUILDS
SUPPORT = ('build.sh', 'toolchain.cmake', 'open.c', 'pins.txt', 'README.md')
PIN = re.compile(r'^(\w+)\s+(all|' + '|'.join(re.escape(v) for v in VARIANTS) +
                 r')\s+(\S+)\s+(not-built|fails|times-out)(?:\s+(\d+))?$')
PROBE = r'''#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALIGN(T) ((unsigned)offsetof(struct { char c; T x; }, x))

int main(void)
{
    printf("renesas probe: int %u, long %u, long long %u, pointer %u, size_t %u "
           "(SIZE_MAX %lu), double %u, long double %u; aligned: long long %u, "
           "double %u, long double %u, pointer %u; char %s\n",
           (unsigned)sizeof(int), (unsigned)sizeof(long), (unsigned)sizeof(long long),
           (unsigned)sizeof(void *), (unsigned)sizeof(size_t), (unsigned long)SIZE_MAX,
           (unsigned)sizeof(double), (unsigned)sizeof(long double),
           ALIGN(long long), ALIGN(double), ALIGN(long double), ALIGN(void *),
           (CHAR_MIN < 0) ? "signed" : "unsigned");
    return 3;
}
'''
PROBE_STATUS = 3
FILE_PROBE = r'''#include <stdio.h>
#include <string.h>

int main(void)
{
    char buf[16] = {0};
    FILE *f = fopen("renesas-file-probe.txt", "w");
    fprintf(stderr, "file probe: fopen w %s\n", f ? "a stream" : "NULL");
    if (f == NULL)
        return 1;
    fputs("canon", f);
    fprintf(stderr, "file probe: fclose %d\n", fclose(f));
    f = fopen("renesas-file-probe.txt", "r");
    fprintf(stderr, "file probe: fopen r %s\n", f ? "a stream" : "NULL");
    if (f == NULL)
        return 2;
    (void)fgets(buf, (int)sizeof buf, f);
    (void)fclose(f);
    fprintf(stderr, "file probe: read \"%s\"\n", buf);
    (void)remove("renesas-file-probe.txt");
    return strcmp(buf, "canon") == 0 ? 0 : 3;
}
'''
FILE_PROBE_LIMIT = 30   # seconds the file probe runs

sh, rel, shown, described = cortexm.sh, cortexm.rel, cortexm.shown, cortexm.described
run_test, why_not_built = msp430.run_test, msp430.why_not_built


def tool(variant, name):
    return VARIANTS[variant]['target'] + '-' + name


def read_pins():
    """[{line, test, variant, builds, outcome, fails}], and the problems."""
    pins, problems = [], []
    with open(PINS, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            m = PIN.match(text)
            if not m:
                problems.append(f'pins.txt:{n}: not "TEST VARIANTS BUILDS OUTCOME [FAILS]"')
                continue
            test, variant, builds, outcome, fails = m.groups()
            builds = BUILDS if builds == 'all' else tuple(builds.split(','))
            if any(b not in BUILDS for b in builds):
                problems.append(f'pins.txt:{n}: build types "all" or of {", ".join(BUILDS)}')
                continue
            if (outcome == 'fails') != (fails is not None):
                problems.append(f'pins.txt:{n}: "fails" takes a count of FAIL lines, '
                                f'"not-built" and "times-out" none')
                continue
            pins.append({'line': n, 'test': test, 'variant': variant, 'builds': builds,
                         'outcome': outcome,
                         'fails': int(fails) if fails is not None else None})
    return pins, problems


def matching(pins, test, variant, build):
    return [p for p in pins if p['test'] == test and p['variant'] in ('all', variant)
            and build in p['builds']]


def expected(pins, test, variant, build):
    m = matching(pins, test, variant, build)
    return (m[0]['outcome'], m[0]['fails']) if m else ('passes', None)


def versions(variant):
    out = []
    for name in ('gcc', 'run'):
        r = sh([tool(variant, name), '--version'])
        text = (r.stdout or r.stderr).strip()
        out.append(text.splitlines()[0] if text else f'{tool(variant, name)}: not found')
    r = sh(['sh', '-c', 'command -v ' + tool(variant, 'gcc')])
    out.append(tool(variant, 'gcc') + ': ' + (r.stdout.strip() or 'not on the PATH'))
    return out


def facts(variant):
    """newlib's version and the compiler's macros of the variant."""
    r = sh([tool(variant, 'gcc'), *VARIANTS[variant]['flags'].split(), '-E', '-dM', '-x', 'c',
            '-'], input='#include <stdio.h>\n')
    macros = dict(re.findall(r'^#define (\w+)(?: (.*))?$', r.stdout, re.M))
    version = macros.get('_NEWLIB_VERSION', '?').strip('"')
    keys = [k for k in sorted(macros) if re.match(r'__(v850|V850|RH850)\w*__$', k)]
    return f'newlib {version}; ' + ', '.join(keys)


def probe(variant, out):
    """Build and run a program that prints the target's sizes and alignments
    and exits with PROBE_STATUS: the simulator must carry both, or no
    test's result means anything. Returns its line of output."""
    d = os.path.abspath(os.path.join(out, 'probe-' + variant))
    os.makedirs(d, exist_ok=True)
    src, elf = os.path.join(d, 'probe.c'), os.path.join(d, 'probe')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(PROBE)
    r = sh([tool(variant, 'gcc'), *VARIANTS[variant]['flags'].split(), '-std=c99', '-O1', src,
            '-o', elf])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: the probe does not build')
    got = run_test({'command': [tool(variant, 'run'), elf], 'cwd': d}, 60)
    lines = [line for line in got['tail'] if line.startswith('renesas probe:')]
    if got['outcome'] == 'times-out' or got['rc'] != PROBE_STATUS or not lines:
        shown('\n'.join(got['tail']))
        raise RuntimeError(f'{variant}: under the simulator the probe exits with '
                           f'{got["rc"]}, not {PROBE_STATUS}, or prints no line: the '
                           f'simulator does not carry a program\'s output and exit status')
    return lines[0]


def support(variant, out):
    """open.c compiled for the variant: the object each test is linked with."""
    d = os.path.abspath(os.path.join(out, 'support-' + variant))
    os.makedirs(d, exist_ok=True)
    obj = os.path.join(d, 'open.o')
    r = sh([tool(variant, 'gcc'), *VARIANTS[variant]['flags'].split(), '-std=c99', '-O2',
            '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-c', os.path.join(HERE, 'open.c'),
            '-o', obj])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: tools/renesas/open.c does not compile')
    return obj


def file_probe(variant, out, obj):
    """A program that writes a file and reads it back, its steps on stderr,
    linked with newlib's _open and with open.c's: what each does under the
    simulator. It fails nothing. Returns one line per link."""
    d = os.path.abspath(os.path.join(out, 'file-probe-' + variant))
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, 'file_probe.c')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(FILE_PROBE)
    lines = []
    for label, extra in (("newlib's _open", []), ("open.c's _open", [obj])):
        elf = os.path.join(d, 'file_probe' + ('_open_c' if extra else ''))
        r = sh([tool(variant, 'gcc'), *VARIANTS[variant]['flags'].split(), '-std=c99', '-O1',
                src, *extra, '-o', elf])
        if r.returncode != 0:
            shown(r.stdout + r.stderr)
            lines.append(f'file probe, {label}: does not build')
            continue
        got = run_test({'command': [tool(variant, 'run'), elf], 'cwd': d}, FILE_PROBE_LIMIT,
                       keep=20)
        steps = [x[len('file probe: '):] for x in got['tail'] if x.startswith('file probe:')]
        end = (f'stopped at {FILE_PROBE_LIMIT} s' if got['outcome'] == 'times-out'
               else f'exit {got["rc"]}')
        lines.append(f'file probe, {label}: ' + '; '.join(steps + [end]))
    return lines


def environment(variant, obj):
    env = dict(os.environ)
    env['CANON_RS_TARGET'] = VARIANTS[variant]['target']
    env['CANON_RS_FLAGS'] = (VARIANTS[variant]['flags'] + ' ' + DEFINES).strip()
    env['CANON_RS_LINK'] = obj
    env['CANON_RS_RUN'] = tool(variant, 'run')
    return env


def configure_and_build(variant, build, out, env):
    b = os.path.abspath(os.path.join(out, f'{variant}-{build}'))
    shutil.rmtree(b, ignore_errors=True)
    r = sh(['cmake', '-S', ROOT, '-B', b, '-G', 'Unix Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE=' + os.path.join(HERE, 'toolchain.cmake'),
            '-DCMAKE_BUILD_TYPE=' + build], env=env)
    if r.returncode != 0:
        shown('\n'.join((r.stdout + r.stderr).splitlines()[-30:]))
        raise RuntimeError(f'{variant} {build}: CMake does not configure')
    # -k: the tests that build are built, whatever does not.
    r = sh(['cmake', '--build', b, '--parallel', str(os.cpu_count() or 2), '--', '-k'], env=env)
    with open(b + '.build.log', 'w') as f:
        f.write(r.stdout + r.stderr)
    return b


def attributes(b, name, binary, variant):
    """The binary's machine and flags, the notes the RH850 ABI's binaries
    carry (the alignment of 8-byte objects, the FPU the code needs), whether
    its test was compiled with the variant's flags (CMake's flags.make), and
    their problems."""
    r = sh([tool(variant, 'readelf'), '-h', binary])
    head = dict(re.findall(r'^\s*(Class|Data|Machine|Flags):\s*(.*?)\s*$', r.stdout, re.M))
    notes = dict(re.findall(r'^\s*(Alignment of 8-byte objects|Type of FPU support needed):'
                            r'\s*(.*?)\s*$', sh([tool(variant, 'readelf'), '-n', binary]).stdout,
                            re.M))
    problems = []
    if head.get('Class') != 'ELF32' or 'V850' not in head.get('Machine', '').upper():
        problems.append(f'it is {head.get("Class")}, {head.get("Machine")}')
    if notes.get('Type of FPU support needed') != VARIANTS[variant]['fpu']:
        problems.append(f'its FPU note is "{notes.get("Type of FPU support needed")}", not '
                        f'"{VARIANTS[variant]["fpu"]}"')
    flags = os.path.join(b, 'CMakeFiles', name + '.dir', 'flags.make')
    text = open(flags, encoding='utf-8').read() if os.path.exists(flags) else ''
    if VARIANTS[variant]['flags'] not in text:
        problems.append(f'its flags.make has no {VARIANTS[variant]["flags"]}')
    return (f'{head.get("Class")}, {head.get("Data")}, {head.get("Machine")}; notes: '
            f'8-byte objects aligned {notes.get("Alignment of 8-byte objects")}, FPU '
            f'{notes.get("Type of FPU support needed")}; compiled with '
            f'{VARIANTS[variant]["flags"]}'), problems


def leg(variant, build, out, env, pins):
    """One build type: (notice line, differences, data), or raises."""
    t0 = time.time()
    b = configure_and_build(variant, build, out, env)
    tests = cortexm.listed(b)
    if not tests:
        raise RuntimeError(f'{variant} {build}: CTest lists no test')
    rows = []
    for t in tests:
        want = expected(pins, t['name'], variant, build)
        if os.path.exists(t['command'][-1]):
            got = run_test(t, PINNED_TIMEOUT if want[0] == 'times-out' else TIMEOUT)
        else:
            got = {'outcome': 'not-built', 'rc': None, 'fails': None, 'fail_lines': [],
                   'why': why_not_built(b, t['name'], env), 'seconds': None}
        rows.append({'name': t['name'], 'got': got, 'want': want})
    built = [t for t in tests if os.path.exists(t['command'][-1])]
    if not built:
        raise RuntimeError(f'{variant} {build}: no test built')
    attrs, attr_problems = attributes(b, built[0]['name'], built[0]['command'][-1], variant)
    if attr_problems:
        raise RuntimeError(f'{variant} {build}: {built[0]["name"]} is not the variant\'s: '
                           + '; '.join(attr_problems))
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
    failed = [r for r in rows if r['got']['outcome'] in ('fails', 'times-out')]
    unbuilt = [r for r in rows if r['got']['outcome'] == 'not-built']
    minutes = (time.time() - t0) / 60
    line = (f'renesas {variant} {build}: {len(rows)} tests, {len(passed)} passed, '
            f'{len(failed)} failed')
    if failed:
        line += ' (' + ', '.join(f'{r["name"]} {described(r["got"]["outcome"], r["got"]["fails"])}'
                                 .replace(' fails ', ' ') for r in failed) + ')'
    line += f', {len(unbuilt)} not built'
    if unbuilt:
        line += ' (' + ', '.join(r['name'] for r in unbuilt) + ')'
    line += '; as pinned' if not diffs else (
        f'; {len(diffs)} difference(s): ' + ', '.join(diffs[:SHOWN])
        + (f', and {len(diffs) - SHOWN} more' if len(diffs) > SHOWN else ''))
    line += f'; {minutes:.1f} minutes'
    print(line)
    print(f'--- {variant} {build}: {built[0]["name"]}: {attrs}')
    ran = sorted((r for r in rows if r['got']['seconds'] is not None),
                 key=lambda r: -r['got']['seconds'])
    print(f'--- {variant} {build}: seconds: '
          + ', '.join(f'{r["name"]} {r["got"]["seconds"]}' for r in ran[:10]))
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
        print(f'--- {variant} {build}: differences from the pins ({len(diffs)}) ---')
        shown('\n'.join(diffs))
    data = {'variant': variant, 'build': build, 'minutes': round(minutes, 1),
            'tests': len(rows), 'passed': [r['name'] for r in passed],
            'failed': {r['name']: described(r['got']['outcome'], r['got']['fails'])
                       for r in failed},
            'seconds': {r['name']: r['got']['seconds'] for r in rows
                        if r['got']['seconds'] is not None},
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
    for variant in VARIANTS:
        for build in BUILDS:
            for t in sorted({p['test'] for p in pins}):
                m = matching(pins, t, variant, build)
                if len(m) > 1:
                    problems.append(f'pins.txt:{m[0]["line"]},{m[1]["line"]}: {t} pinned '
                                    f'twice for {variant} {build}')
    for f in SUPPORT:
        if not os.path.exists(os.path.join(HERE, f)):
            problems.append(f'tools/renesas/{f}: no such file')
    for p in problems:
        print('  ' + p)
    print(f'renesas check: {len(pins)} pins, {len(tests)} tests in CMakeLists.txt; '
          f'{len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if argv[:1] == ['check'] and len(argv) == 1:
        return check()
    report_only = '--report-only' in argv
    args = [a for a in argv if a != '--report-only']
    if len(args) < 2 or args[0] != 'run' or args[1] not in VARIANTS \
            or any(b not in BUILDS for b in args[2:]):
        print(__doc__)
        return 2
    variant, builds = args[1], args[2:] or list(BUILDS)
    for v in versions(variant):
        print(v)
    print(f'C library and target: {facts(variant)}')
    print(f'Variant: {variant}; flags: {VARIANTS[variant]["flags"]} {DEFINES}'.rstrip())
    pins, problems = read_pins()
    os.makedirs(OUT, exist_ok=True)
    all_diffs = list(problems)
    try:
        for name in ('gcc', 'run', 'readelf'):
            if not shutil.which(tool(variant, name)):
                raise RuntimeError(f'{tool(variant, name)} must be on the PATH: '
                                   f'tools/renesas/build.sh builds it')
        line = probe(variant, OUT)
        print(line)
        print(f'::notice title=renesas {variant} probe::{line}')
        obj = support(variant, OUT)
        for line in file_probe(variant, OUT, obj):
            print(line)
            print(f'::notice title=renesas {variant} file probe::{line}')
        env = environment(variant, obj)
        for build in builds:
            line, diffs, _ = leg(variant, build, OUT, env, pins)
            print(f'::notice title=renesas {variant} {build}::{line}')
            all_diffs += diffs
    except RuntimeError as e:
        print(f'::error title=renesas {variant}::{e}')
        return 1
    if all_diffs and ENFORCE and not report_only:
        print(f'::error title=renesas {variant}::{len(all_diffs)} difference(s) from '
              f'tools/renesas/pins.txt')
        return 1
    if all_diffs:
        print(f'REPORT-ONLY: {len(all_diffs)} difference(s) from the pins, not failing '
              f'({"--report-only" if report_only else "ENFORCE is off"})')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
