#!/usr/bin/env python3
"""
aarch64.py -- the test suite on bare-metal AArch64 under QEMU, at two
cores Arm makes for cars and real-time systems (VERIFY-099).

`2 · Platforms`' build-arm64 runs the tests on AArch64 Linux, with glibc;
this runs them on bare metal. Each test of CMakeLists.txt is built with
Ubuntu 24.04's aarch64-linux-gnu-gcc and picolibc (its picolibc.specs in
place of glibc), and run under qemu-system-aarch64's virt board through
semihosting, which carries its output and its exit status to the host.
QEMU 8.2 models neither core: each is compiled for with -mcpu and run on
QEMU's "max" CPU, which has the instructions of both:

    a78ae   -mcpu=cortex-a78ae: Armv8.2-A, A-profile, the Cortex-A78's
            automotive variant.
    r82     -mcpu=cortex-r82: Armv8-R AArch64, R-profile, Arm's 64-bit
            real-time core.

Each is built at the four build types of CMake: Debug (-O0), Release
(-O3), RelWithDebInfo (-O2) and MinSizeRel (-Os). Nothing is defined for
lifetime.h: both cores have Armv8.1's atomics (LSE), to which its token
counter's 64-bit atomic compiles. A test passes when it exits with status
0, within TIMEOUT seconds. Before the tests, a probe checks that QEMU
carries a program's output and its exit status, and prints the core's
macros as the compiler has them (architecture, profile, atomics), the
type sizes, the exception level the tests run at and whether its MMU is
on (picolibc's start file leaves it off), and an unaligned load's result.

The tests that do not pass are pinned, with the reason, in pins.txt: each
line a test, the cores and build types it holds for, and what the test
does there: "not-built", "fails N" (N the lines its output begins with
FAIL), or "times-out", stopped at PINNED_TIMEOUT seconds. A test the pins
do not name must pass. An outcome other than its pin's is a difference;
with ENFORCE, a difference fails the run, unless the run is --report-only.
A run that cannot configure, whose probe fails, that finds no test, builds
none, or builds a binary that is not AArch64, or not with the core's flag,
always fails.

    python3 tools/aarch64/aarch64.py run CORE [BUILD...] [--report-only]
    python3 tools/aarch64/aarch64.py check

"run" writes its builds and its summaries to aarch64_out/, run from the
repository's root. The steps it shares with tools/msp430/msp430.py (a
test's run, its output kept when it is stopped, and why a test is not
built) and with tools/cortexm/cortexm.py (CTest's list, FAIL lines) are
those scripts'. "check" is 8 · Records': each pin well formed, naming a
test of CMakeLists.txt, no two pins for the same test and leg, and the
support files there.
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
OUT = 'aarch64_out'
ENFORCE = True        # from VERIFY-099's run 1: its pins measured before it
TIMEOUT = 120         # seconds a test, under QEMU
PINNED_TIMEOUT = 60   # seconds a test pinned to time out
SHOWN = 6             # differences named in a notice

GCC = 'aarch64-linux-gnu-gcc'
# -nic none: no network card, whose ROM (efi-virtio.rom) the packages
# installed without apt's recommends do not hold, and QEMU refuses to
# start without it.
QEMU = ['qemu-system-aarch64', '-M', 'virt', '-cpu', 'max', '-nic', 'none', '-nographic',
        '-monitor', 'none', '-serial', 'none',
        '-semihosting-config', 'enable=on,target=native', '-kernel']
CORES = {
    'a78ae': {'flags': '-mcpu=cortex-a78ae', 'profile': 'A'},
    'r82': {'flags': '-mcpu=cortex-r82', 'profile': 'R'},
}
DEFINES = ''
SPECS = ['--specs=picolibc.specs']
# The probe's link: toolchain.cmake's CMAKE_EXE_LINKER_FLAGS_INIT, which
# links the tests; the two must stay the same.
LINK = ['--oslib=semihost', '--crt0=semihost', '-Wl,--defsym=__flash=0x40000000',
        '-Wl,--defsym=__flash_size=0x1000000', '-Wl,--defsym=__ram=0x41000000',
        '-Wl,--defsym=__ram_size=0x1000000', '-Wl,--defsym=__stack_size=0x40000']
BUILDS = cortexm.BUILDS
SUPPORT = ('toolchain.cmake', 'pins.txt', 'README.md')
PIN = re.compile(r'^(\w+)\s+(all|a78ae|r82)\s+(\S+)\s+(not-built|fails|times-out)'
                 r'(?:\s+(\d+))?$')
PROBE = r'''#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static unsigned char bytes[16];
    uint32_t word;
    volatile uint32_t *at;
    unsigned long el, sctlr = 0;

    /* The exception level, and its system control register: M is the MMU
       (or the R-profile's MPU), A the alignment check. */
    __asm__ volatile ("mrs %0, CurrentEL" : "=r"(el));
    el = (el >> 2) & 3u;
    if (el == 1u) {
        __asm__ volatile ("mrs %0, sctlr_el1" : "=r"(sctlr));
    } else if (el == 2u) {
        __asm__ volatile ("mrs %0, sctlr_el2" : "=r"(sctlr));
    }
    memcpy(bytes + 1, "\x01\x02\x03\x04", 4);
    at = (volatile uint32_t *)(void *)(bytes + 1);   /* an unaligned load */
    word = *at;
    printf("aarch64 probe: __ARM_ARCH %d, profile %c, LSE atomics %s, long %u, "
           "pointer %u, long double %u, char %s, EL%lu, SCTLR M %lu A %lu, "
           "an unaligned load 0x%08lx\n",
           (int)__ARM_ARCH, (char)__ARM_ARCH_PROFILE,
#if defined(__ARM_FEATURE_ATOMICS)
           "yes",
#else
           "no",
#endif
           (unsigned)sizeof(long), (unsigned)sizeof(void *), (unsigned)sizeof(long double),
#if defined(__CHAR_UNSIGNED__)
           "unsigned",
#else
           "signed",
#endif
           el, sctlr & 1u, (sctlr >> 1) & 1u, (unsigned long)word);
    return 3;
}
'''
PROBE_STATUS = 3

sh, rel, shown, described = cortexm.sh, cortexm.rel, cortexm.shown, cortexm.described
run_test, why_not_built = msp430.run_test, msp430.why_not_built


def read_pins():
    """[{line, test, core, builds, outcome, fails}], and the problems."""
    pins, problems = [], []
    with open(PINS, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            m = PIN.match(text)
            if not m:
                problems.append(f'pins.txt:{n}: not "TEST CORES BUILDS OUTCOME [FAILS]"')
                continue
            test, core, builds, outcome, fails = m.groups()
            builds = BUILDS if builds == 'all' else tuple(builds.split(','))
            if any(b not in BUILDS for b in builds):
                problems.append(f'pins.txt:{n}: build types "all" or of {", ".join(BUILDS)}')
                continue
            if (outcome == 'fails') != (fails is not None):
                problems.append(f'pins.txt:{n}: "fails" takes a count of FAIL lines, '
                                f'"not-built" and "times-out" none')
                continue
            pins.append({'line': n, 'test': test, 'core': core, 'builds': builds,
                         'outcome': outcome,
                         'fails': int(fails) if fails is not None else None})
    return pins, problems


def matching(pins, test, core, build):
    return [p for p in pins if p['test'] == test and p['core'] in ('all', core)
            and build in p['builds']]


def expected(pins, test, core, build):
    m = matching(pins, test, core, build)
    return (m[0]['outcome'], m[0]['fails']) if m else ('passes', None)


def versions():
    out = []
    for cmd in ([GCC, '--version'], [QEMU[0], '--version']):
        r = sh(cmd)
        out.append(r.stdout.splitlines()[0] if r.returncode == 0 and r.stdout
                   else f'{cmd[0]}: not found')
    r = sh(['dpkg-query', '-W', '-f=${Package} ${Version}', 'picolibc-aarch64-linux-gnu'])
    out.append(r.stdout.strip() if r.returncode == 0 else 'picolibc-aarch64-linux-gnu: ?')
    return out


def facts(core):
    """picolibc's version and the core's macros, from GCC's."""
    r = sh([GCC, *CORES[core]['flags'].split(), *SPECS, '-E', '-dM', '-x', 'c', '-'],
           input='#include <stdio.h>\n')
    macros = dict(re.findall(r'^#define (\w+)(?: (.*))?$', r.stdout, re.M))
    keys = ('__ARM_ARCH', '__ARM_ARCH_PROFILE', '__ARM_FEATURE_ATOMICS',
            '__ARM_FEATURE_UNALIGNED', '__SIZEOF_LONG_DOUBLE__')
    sizes = ', '.join(f'{k} {macros.get(k, "undefined")}' for k in keys)
    version = macros.get('__PICOLIBC_VERSION__', '?').strip('"')
    return f'picolibc {version}; {sizes}'


def probe(core, out):
    """Build and run a program that prints the core's macros and an unaligned
    load, and exits with PROBE_STATUS: QEMU must carry both, or no test's
    result means anything. Returns its line of output."""
    d = os.path.abspath(os.path.join(out, 'probe-' + core))
    os.makedirs(d, exist_ok=True)
    src, elf = os.path.join(d, 'probe.c'), os.path.join(d, 'probe')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(PROBE)
    r = sh([GCC, *CORES[core]['flags'].split(), *SPECS, '-std=c99', '-O1', src, *LINK,
            '-o', elf])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{core}: the probe does not build')
    got = run_test({'command': QEMU + [elf], 'cwd': d}, 60)
    lines = [line for line in got['tail'] if line.startswith('aarch64 probe:')]
    if got['outcome'] == 'times-out' or got['rc'] != PROBE_STATUS or not lines:
        shown('\n'.join(got['tail']))
        raise RuntimeError(f'{core}: under QEMU the probe exits with {got["rc"]}, not '
                           f'{PROBE_STATUS}, or prints no line: QEMU does not carry a '
                           f'program\'s output and exit status')
    want = f'profile {CORES[core]["profile"]}'
    if want not in lines[0]:
        raise RuntimeError(f'{core}: the probe was not compiled for the core\'s '
                           f'profile ({want}): {lines[0]}')
    return lines[0]


def environment(core):
    env = dict(os.environ)
    env['CANON_A64_FLAGS'] = (CORES[core]['flags'] + ' ' + DEFINES).strip()
    env['CANON_A64_QEMU'] = ';'.join(QEMU)
    return env


def configure_and_build(core, build, out, env):
    b = os.path.abspath(os.path.join(out, f'{core}-{build}'))
    shutil.rmtree(b, ignore_errors=True)
    r = sh(['cmake', '-S', ROOT, '-B', b, '-G', 'Unix Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE=' + os.path.join(HERE, 'toolchain.cmake'),
            '-DCMAKE_BUILD_TYPE=' + build], env=env)
    if r.returncode != 0:
        shown('\n'.join((r.stdout + r.stderr).splitlines()[-30:]))
        raise RuntimeError(f'{core} {build}: CMake does not configure')
    # -k: the tests that build are built, whatever does not.
    r = sh(['cmake', '--build', b, '--parallel', str(os.cpu_count() or 2), '--', '-k'], env=env)
    with open(b + '.build.log', 'w') as f:
        f.write(r.stdout + r.stderr)
    return b


def attributes(b, name, binary, core):
    """The binary's class, byte order and machine, and whether its test was
    compiled with the core's flag (CMake's flags.make), and their problems."""
    r = sh(['readelf', '-h', binary])
    head = dict(re.findall(r'^\s*(Class|Data|Machine):\s*(.*?)\s*$', r.stdout, re.M))
    problems = []
    if head.get('Class') != 'ELF64' or 'little endian' not in head.get('Data', '') \
            or head.get('Machine') != 'AArch64':
        problems.append(f'it is {head.get("Class")}, {head.get("Data")}, {head.get("Machine")}')
    flags = os.path.join(b, 'CMakeFiles', name + '.dir', 'flags.make')
    text = open(flags, encoding='utf-8').read() if os.path.exists(flags) else ''
    if CORES[core]['flags'] not in text:
        problems.append(f'its flags.make has no {CORES[core]["flags"]}')
    return (f'{head.get("Class")}, {head.get("Data")}, {head.get("Machine")}; '
            f'compiled with {CORES[core]["flags"]}'), problems


def leg(core, build, out, env, pins):
    """One build type: (notice line, differences, data), or raises."""
    t0 = time.time()
    b = configure_and_build(core, build, out, env)
    tests = cortexm.listed(b)
    if not tests:
        raise RuntimeError(f'{core} {build}: CTest lists no test')
    rows = []
    for t in tests:
        want = expected(pins, t['name'], core, build)
        if os.path.exists(t['command'][-1]):
            got = run_test(t, PINNED_TIMEOUT if want[0] == 'times-out' else TIMEOUT)
        else:
            got = {'outcome': 'not-built', 'rc': None, 'fails': None, 'fail_lines': [],
                   'why': why_not_built(b, t['name'], env), 'seconds': None}
        rows.append({'name': t['name'], 'got': got, 'want': want})
    built = [t for t in tests if os.path.exists(t['command'][-1])]
    if not built:
        raise RuntimeError(f'{core} {build}: no test built')
    attrs, attr_problems = attributes(b, built[0]['name'], built[0]['command'][-1], core)
    if attr_problems:
        raise RuntimeError(f'{core} {build}: {built[0]["name"]} is not AArch64 for the '
                           f'{core}: ' + '; '.join(attr_problems))
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
    line = (f'aarch64 {core} {build}: {len(rows)} tests, {len(passed)} passed, '
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
    print(f'--- {core} {build}: {built[0]["name"]}: {attrs}')
    ran = sorted((r for r in rows if r['got']['seconds'] is not None),
                 key=lambda r: -r['got']['seconds'])
    print(f'--- {core} {build}: seconds: '
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
        print(f'--- {core} {build}: differences from the pins ({len(diffs)}) ---')
        shown('\n'.join(diffs))
    data = {'core': core, 'build': build, 'minutes': round(minutes, 1),
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
    for core in CORES:
        for build in BUILDS:
            for t in sorted({p['test'] for p in pins}):
                m = matching(pins, t, core, build)
                if len(m) > 1:
                    problems.append(f'pins.txt:{m[0]["line"]},{m[1]["line"]}: {t} pinned '
                                    f'twice for {core} {build}')
    for f in SUPPORT:
        if not os.path.exists(os.path.join(HERE, f)):
            problems.append(f'tools/aarch64/{f}: no such file')
    for p in problems:
        print('  ' + p)
    print(f'aarch64 check: {len(pins)} pins, {len(tests)} tests in CMakeLists.txt; '
          f'{len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if argv[:1] == ['check'] and len(argv) == 1:
        return check()
    report_only = '--report-only' in argv
    args = [a for a in argv if a != '--report-only']
    if len(args) < 2 or args[0] != 'run' or args[1] not in CORES \
            or any(b not in BUILDS for b in args[2:]):
        print(__doc__)
        return 2
    core, builds = args[1], args[2:] or list(BUILDS)
    for v in versions():
        print(v)
    print(f'C library and core: {facts(core)}')
    print(f'Core: {core}; flags: {CORES[core]["flags"]} {DEFINES}'.rstrip())
    print('QEMU: ' + ' '.join(QEMU))
    pins, problems = read_pins()
    os.makedirs(OUT, exist_ok=True)
    all_diffs = list(problems)
    try:
        if not shutil.which(GCC) or not shutil.which(QEMU[0]):
            raise RuntimeError(f'{GCC} and {QEMU[0]} must be on the PATH')
        line = probe(core, OUT)
        print(line)
        print(f'::notice title=aarch64 {core} probe::{line}')
        env = environment(core)
        for build in builds:
            line, diffs, _ = leg(core, build, OUT, env, pins)
            print(f'::notice title=aarch64 {core} {build}::{line}')
            all_diffs += diffs
    except RuntimeError as e:
        print(f'::error title=aarch64 {core}::{e}')
        return 1
    if all_diffs and ENFORCE and not report_only:
        print(f'::error title=aarch64 {core}::{len(all_diffs)} difference(s) from '
              f'tools/aarch64/pins.txt')
        return 1
    if all_diffs:
        print(f'REPORT-ONLY: {len(all_diffs)} difference(s) from the pins, not failing '
              f'({"--report-only" if report_only else "ENFORCE is off"})')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
