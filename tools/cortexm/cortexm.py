#!/usr/bin/env python3
"""
cortexm.py -- the test suite on Cortex-M0+ and Cortex-M4 under QEMU
(VERIFY-089).

`2 · Platforms`' job `embedded` compiles the tests for Cortex-M; this runs
them there. Each test of CMakeLists.txt is built with arm-none-eabi-gcc,
with newlib or picolibc, and run under qemu-system-arm through
semihosting, which carries its output and its exit status to the host.
The toolchain is Ubuntu 24.04's packages ("ubuntu", GCC 13.2, newlib and
picolibc), or the Arm GNU Toolchain on the PATH ("arm-15.3.rel2", its own
GCC and newlib), named with --toolchain:

    m0plus   -mcpu=cortex-m0plus, soft float, on QEMU's micro:bit, whose
             Cortex-M0 runs the same Armv6-M code; its nRF51's SRAM and
             flash are given 4 MB each. Armv6-M faults on an unaligned
             access, and QEMU models it.
    m4       -mcpu=cortex-m4, hard float (FPv4-SP: doubles in software),
             on mps2-an386.

On both, enums are short and int32_t is long. Each is built at the four
build types of CMake: Debug (-O0), Release (-O3), RelWithDebInfo (-O2) and
MinSizeRel (-Os), with CANON_LIFETIME_NO_ATOMICS defined: a Cortex-M has
no lock-free 64-bit atomic, which CANON_LIFETIME_DEBUG's counter needs
otherwise (the README's bare-metal section). A test passes when it exits
with status 0.

The tests that do not pass are pinned, with the reason, in pins.txt: each
line a test, the toolchains, CPUs, C libraries and build types it holds
for, and what it does there, "not-built" or "fails N" (N the lines its
output begins with FAIL). A test the pins do not name must pass. An
outcome other than its pin's is a difference; with ENFORCE, a difference
fails the run, unless the run is --report-only. A run that cannot
configure, finds no test, or builds a binary that is not the CPU's (its
ELF attributes) always fails.

    python3 tools/cortexm/cortexm.py run CPU LIBC [BUILD...] [--toolchain T] [--report-only]
    python3 tools/cortexm/cortexm.py check

"run" writes its builds and its summaries to cortexm_out/, run from the
repository's root. Its log names the compiler, QEMU and the C library,
with whether its printf has C99's formats. "check" is 8 · Records': each
pin well formed, naming a test of CMakeLists.txt, no two pins for the same
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
PINS = os.path.join(HERE, 'pins.txt')
OUT = 'cortexm_out'
ENFORCE = True        # VERIFY-089 run 2: a difference fails the run (run 1 report-only)
TIMEOUT = 60          # seconds a test, under QEMU
SHOWN = 6             # differences named in a notice

SEMIHOSTING = ['-nographic', '-monitor', 'none', '-serial', 'none',
               '-semihosting-config', 'enable=on,target=native', '-kernel']
CPUS = {
    'm0plus': {
        'flags': '-mcpu=cortex-m0plus -mthumb -mfloat-abi=soft',
        'qemu': ['qemu-system-arm', '-M', 'microbit',
                 '-global', 'nrf51-soc.sram-size=0x400000',
                 '-global', 'nrf51-soc.flash-size=0x400000'],
        'board': 'microbit, a Cortex-M0, 4 MB of SRAM and of flash',
        'attrs': {'Tag_CPU_arch': 'v6S-M', 'Tag_ABI_enum_size': 'small'},
        'absent': ('Tag_ABI_VFP_args', 'Tag_FP_arch'),
    },
    'm4': {
        'flags': '-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16',
        'qemu': ['qemu-system-arm', '-M', 'mps2-an386'],
        'board': 'mps2-an386, a Cortex-M4F',
        'attrs': {'Tag_CPU_arch': 'v7E-M', 'Tag_ABI_enum_size': 'small',
                  'Tag_FP_arch': 'VFPv4-D16', 'Tag_ABI_VFP_args': 'VFP registers'},
        'absent': (),
    },
}
LIBCS = ('newlib', 'picolibc')
# The toolchains, and the C libraries each has.
TOOLCHAINS = {
    'ubuntu': ('newlib', 'picolibc'),
    'arm-15.3.rel2': ('newlib',),
}
# Defined for every test: lifetime.h's debug counter without atomics.
DEFINES = '-DCANON_LIFETIME_NO_ATOMICS'

BUILDS = ('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')
SUPPORT = ('toolchain.cmake', 'startup.c', 'cortexm.ld', 'pins.txt')
FAIL_LINE = re.compile(r'^FAIL\b')
PIN = re.compile(r'^(\w+)\s+(\S+)\s+(all|m0plus|m4)\s+(all|newlib|picolibc)\s+(\S+)\s+'
                 r'(not-built|fails)(?:\s+(\d+))?$')


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def rel(text):
    return text.replace(ROOT + os.sep, '')


def shown(text):
    for line in text.splitlines():
        print('    ' + line)


def read_pins():
    """[{line, test, toolchain, cpu, libc, builds, outcome, fails}], and the problems."""
    pins, problems = [], []
    with open(PINS, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            text = line.split('#', 1)[0].strip()
            if not text:
                continue
            m = PIN.match(text)
            if not m:
                problems.append(f'pins.txt:{n}: not "TEST TOOLCHAIN CPU LIBC BUILDS OUTCOME '
                                f'[FAILS]"')
                continue
            test, toolchain, cpu, libc, builds, outcome, fails = m.groups()
            if toolchain != 'all' and toolchain not in TOOLCHAINS:
                problems.append(f'pins.txt:{n}: toolchain "all" or of {", ".join(TOOLCHAINS)}')
                continue
            builds = BUILDS if builds == 'all' else tuple(builds.split(','))
            if any(b not in BUILDS for b in builds):
                problems.append(f'pins.txt:{n}: build types "all" or of {", ".join(BUILDS)}')
                continue
            if (outcome == 'fails') != (fails is not None):
                problems.append(f'pins.txt:{n}: "fails" takes a count of FAIL lines, '
                                f'"not-built" none')
                continue
            pins.append({'line': n, 'test': test, 'toolchain': toolchain, 'cpu': cpu,
                         'libc': libc,
                         'builds': builds, 'outcome': outcome,
                         'fails': int(fails) if fails is not None else None})
    return pins, problems


def matching(pins, test, toolchain, cpu, libc, build):
    return [p for p in pins if p['test'] == test and p['toolchain'] in ('all', toolchain)
            and p['cpu'] in ('all', cpu) and p['libc'] in ('all', libc)
            and build in p['builds']]


def expected(pins, test, toolchain, cpu, libc, build):
    m = matching(pins, test, toolchain, cpu, libc, build)
    return (m[0]['outcome'], m[0]['fails']) if m else ('passes', None)


def described(outcome, fails):
    return f'fails {fails}' if outcome == 'fails' else outcome


def cmake_tests():
    """The names of CMakeLists.txt's tests (canon_c_test)."""
    with open(os.path.join(ROOT, 'CMakeLists.txt'), encoding='utf-8') as f:
        text = f.read()
    names = []
    for m in re.finditer(r'^canon_c_test\((.*?)\)', text, re.S | re.M):
        toks = m.group(1).split()
        if 'NAME' in toks:
            names.append(toks[toks.index('NAME') + 1])
    return names


def versions(toolchain):
    out = []
    for cmd in (['arm-none-eabi-gcc', '--version'], ['qemu-system-arm', '--version']):
        r = sh(cmd)
        out.append(r.stdout.splitlines()[0] if r.returncode == 0 and r.stdout
                   else f'{cmd[0]}: not found')
    r = sh(['sh', '-c', 'command -v arm-none-eabi-gcc'])
    out.append('arm-none-eabi-gcc: ' + (r.stdout.strip() or 'not on the PATH'))
    if toolchain == 'ubuntu' and shutil.which('dpkg-query'):
        for pkg in ('libnewlib-arm-none-eabi', 'picolibc-arm-none-eabi'):
            r = sh(['dpkg-query', '-W', '-f=${Package} ${Version}', pkg])
            out.append(r.stdout if r.returncode == 0 else f'{pkg}: not installed')
    return out


def libc_facts(cpu, libc):
    """The C library's version, and whether its printf has C99's formats (%zu)."""
    specs = ['--specs=picolibc.specs'] if libc == 'picolibc' else []
    r = sh(['arm-none-eabi-gcc', *CPUS[cpu]['flags'].split(), *specs, '-E', '-dM', '-x', 'c',
            '-'], input='#include <stdio.h>\n')
    macros = dict(re.findall(r'^#define (\w+)(?: (.*))?$', r.stdout, re.M))
    if libc == 'picolibc':
        version = macros.get('__PICOLIBC_VERSION__', '?').strip('"')
    else:
        version = macros.get('_NEWLIB_VERSION', '?').strip('"')
    c99 = 'yes' if '_WANT_IO_C99_FORMATS' in macros else 'no'
    return f'{libc} {version}; C99 printf formats (_WANT_IO_C99_FORMATS): {c99}'


def support(cpu, libc, out):
    """The directory of newlib's start-up object and linker script."""
    d = os.path.abspath(os.path.join(out, 'support', cpu))
    if libc == 'newlib':
        os.makedirs(d, exist_ok=True)
        r = sh(['arm-none-eabi-gcc', *CPUS[cpu]['flags'].split(), '-std=c99', '-O2',
                '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-c',
                os.path.join(HERE, 'startup.c'), '-o', os.path.join(d, 'startup.o')])
        if r.returncode != 0:
            shown(r.stdout + r.stderr)
            raise RuntimeError(f'{cpu}: tools/cortexm/startup.c does not compile')
        shutil.copy(os.path.join(HERE, 'cortexm.ld'), d)
    return d


def environment(cpu, libc, sup):
    env = dict(os.environ)
    env['CANON_CM_FLAGS'] = CPUS[cpu]['flags'] + ' ' + DEFINES
    env['CANON_CM_LIBC'] = libc
    env['CANON_CM_SUPPORT'] = sup
    env['CANON_CM_QEMU'] = ';'.join(CPUS[cpu]['qemu'] + SEMIHOSTING)
    return env


def configure_and_build(toolchain, cpu, libc, build, out, env):
    b = os.path.abspath(os.path.join(out, f'{toolchain}-{cpu}-{libc}-{build}'))
    shutil.rmtree(b, ignore_errors=True)
    r = sh(['cmake', '-S', ROOT, '-B', b, '-G', 'Unix Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE=' + os.path.join(HERE, 'toolchain.cmake'),
            '-DCMAKE_BUILD_TYPE=' + build], env=env)
    if r.returncode != 0:
        shown('\n'.join((r.stdout + r.stderr).splitlines()[-30:]))
        raise RuntimeError(f'{label(toolchain, cpu, libc, build)}: CMake does not configure')
    # -k: the tests that build are built, whatever does not.
    r = sh(['cmake', '--build', b, '--parallel', str(os.cpu_count() or 2), '--', '-k'], env=env)
    with open(b + '.build.log', 'w') as f:
        f.write(r.stdout + r.stderr)
    return b


def listed(b):
    r = sh(['ctest', '--show-only=json-v1'], cwd=b)
    try:
        data = json.loads(r.stdout)
    except ValueError:
        return []
    out = []
    for t in data.get('tests', []):
        cwd = next((p['value'] for p in t.get('properties', [])
                    if p.get('name') == 'WORKING_DIRECTORY'), b)
        out.append({'name': t['name'], 'command': t['command'], 'cwd': cwd})
    return out


def why_not_built(b, name, env):
    """The first errors of the test's own build, which -k ran with the others."""
    r = sh(['cmake', '--build', b, '--target', name], env=env)
    out = []
    for line in (r.stdout + r.stderr).splitlines():
        if (re.search(r'error:|undefined reference', line)
                and not re.search(r'collect2|ld returned|\*\*\*', line)):
            line = re.sub(r'^\S*/ld: ', '', rel(line.strip()))
            if line not in out:
                out.append(line)
    return out[:3] or ['(no error line found)']


def run_test(t):
    start = time.time()
    try:
        r = subprocess.run(t['command'], cwd=t['cwd'], capture_output=True, text=True,
                           errors='replace', timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return {'outcome': 'timeout', 'rc': None, 'fails': None, 'fail_lines': [],
                'tail': [], 'seconds': TIMEOUT}
    text = rel((r.stdout or '') + (r.stderr or ''))
    fails = [line.rstrip() for line in text.splitlines() if FAIL_LINE.match(line)]
    rest = [line.rstrip() for line in text.splitlines() if not FAIL_LINE.match(line)]
    return {'outcome': 'passes' if r.returncode == 0 else 'fails', 'rc': r.returncode,
            'fails': len(fails) if r.returncode != 0 else None, 'fail_lines': fails,
            'tail': rest[-4:], 'seconds': round(time.time() - start, 2)}


def attributes(binary, cpu):
    """The ELF attributes that say the binary is the CPU's, and their problems."""
    r = sh(['arm-none-eabi-readelf', '-A', binary])
    got = dict(re.findall(r'^\s*(Tag_\w+): (.*?)\s*$', r.stdout, re.M))
    spec = CPUS[cpu]
    problems = [f'{k} is {got.get(k, "absent")}, not {v}'
                for k, v in spec['attrs'].items() if got.get(k) != v]
    problems += [f'{k} is {got[k]}, where soft float has none' for k in spec['absent'] if k in got]
    shown_tags = ('Tag_CPU_arch', 'Tag_FP_arch', 'Tag_ABI_VFP_args', 'Tag_ABI_enum_size',
                  'Tag_CPU_unaligned_access')
    return ', '.join(f'{k} {got[k]}' for k in shown_tags if k in got), problems


def label(toolchain, cpu, libc, build=None):
    """A leg's name: the toolchain named only when it is not Ubuntu's."""
    words = ([] if toolchain == 'ubuntu' else [toolchain]) + [cpu, libc] + ([build] if build else [])
    return ' '.join(words)


def leg(toolchain, cpu, libc, build, out, env, pins):
    """One build type: (notice line, differences, data), or raises."""
    t0 = time.time()
    b = configure_and_build(toolchain, cpu, libc, build, out, env)
    tests = listed(b)
    if not tests:
        raise RuntimeError(f'{label(toolchain, cpu, libc, build)}: CTest lists no test')
    rows = []
    for t in tests:
        if os.path.exists(t['command'][-1]):
            got = run_test(t)
        else:
            got = {'outcome': 'not-built', 'rc': None, 'fails': None, 'fail_lines': [],
                   'why': why_not_built(b, t['name'], env)}
        want = expected(pins, t['name'], toolchain, cpu, libc, build)
        rows.append({'name': t['name'], 'got': got, 'want': want})
    built = [t['command'][-1] for t in tests if os.path.exists(t['command'][-1])]
    if not built:
        raise RuntimeError(f'{label(toolchain, cpu, libc, build)}: no test built')
    attrs, attr_problems = attributes(built[0], cpu)
    if attr_problems:
        raise RuntimeError(f'{label(toolchain, cpu, libc, build)}: {os.path.basename(built[0])} is not the '
                           f'CPU\'s: ' + '; '.join(attr_problems))
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
    line = (f'cortex-m {label(toolchain, cpu, libc, build)}: {len(rows)} tests, '
            f'{len(passed)} passed, {len(failed)} failed')
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
    print(f'--- {label(toolchain, cpu, libc, build)}: the binaries\' ELF attributes: {attrs}')
    for r in failed:
        g = r['got']
        print(f'--- {r["name"]}: {g["outcome"]}, exit {g["rc"]}, '
              f'{len(g["fail_lines"])} FAIL line(s) ---')
        shown('\n'.join(g['fail_lines'][:25] + (['...'] if len(g['fail_lines']) > 25 else [])
                        + ['(the last other lines)'] + g['tail']))
    for r in unbuilt:
        print(f'--- {r["name"]}: not built ---')
        shown('\n'.join(r['got']['why']))
    if diffs:
        print(f'--- {label(toolchain, cpu, libc, build)}: differences from the pins ({len(diffs)}) ---')
        shown('\n'.join(diffs))
    data = {'toolchain': toolchain, 'cpu': cpu, 'libc': libc, 'build': build,
            'minutes': round(minutes, 1),
            'tests': len(rows), 'passed': [r['name'] for r in passed],
            'failed': {r['name']: r['got']['fails'] for r in failed},
            'not_built': [r['name'] for r in unbuilt], 'differences': diffs,
            'attributes': attrs, 'line': line}
    with open(b + '.json', 'w') as f:
        json.dump(data, f, indent=1)
    return line, diffs, data


def check():
    pins, problems = read_pins()
    tests = set(cmake_tests())
    for p in pins:
        if p['test'] not in tests:
            problems.append(f'pins.txt:{p["line"]}: {p["test"]} is not a test of CMakeLists.txt')
    for toolchain, libcs in TOOLCHAINS.items():
        for cpu in CPUS:
            for libc in libcs:
                for build in BUILDS:
                    for t in sorted({p['test'] for p in pins}):
                        m = matching(pins, t, toolchain, cpu, libc, build)
                        if len(m) > 1:
                            problems.append(f'pins.txt:{m[0]["line"]},{m[1]["line"]}: {t} pinned '
                                            f'twice for {label(toolchain, cpu, libc, build)}')
    for f in SUPPORT:
        if not os.path.exists(os.path.join(HERE, f)):
            problems.append(f'tools/cortexm/{f}: no such file')
    for p in problems:
        print('  ' + p)
    print(f'cortexm check: {len(pins)} pins, {len(tests)} tests in CMakeLists.txt; '
          f'{len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if argv[:1] == ['check'] and len(argv) == 1:
        return check()
    args, toolchain, report_only = [], 'ubuntu', False
    rest = list(argv)
    while rest:
        a = rest.pop(0)
        if a == '--toolchain' and rest:
            toolchain = rest.pop(0)
        elif a == '--report-only':
            report_only = True
        else:
            args.append(a)
    if len(args) < 3 or args[0] != 'run' or args[1] not in CPUS \
            or toolchain not in TOOLCHAINS or args[2] not in TOOLCHAINS[toolchain] \
            or any(b not in BUILDS for b in args[3:]):
        print(__doc__)
        return 2
    cpu, libc, builds = args[1], args[2], args[3:] or list(BUILDS)
    for v in versions(toolchain):
        print(v)
    print(f'C library: {libc_facts(cpu, libc)}')
    print(f'QEMU board: {CPUS[cpu]["board"]}; flags: {CPUS[cpu]["flags"]} {DEFINES}')
    name = label(toolchain, cpu, libc)
    pins, problems = read_pins()
    os.makedirs(OUT, exist_ok=True)
    all_diffs = list(problems)
    try:
        sup = support(cpu, libc, OUT)
        env = environment(cpu, libc, sup)
        for build in builds:
            line, diffs, _ = leg(toolchain, cpu, libc, build, OUT, env, pins)
            print(f'::notice title=cortex-m {label(toolchain, cpu, libc, build)}::{line}')
            all_diffs += diffs
    except RuntimeError as e:
        print(f'::error title=cortex-m {name}::{e}')
        return 1
    if all_diffs and ENFORCE and not report_only:
        print(f'::error title=cortex-m {name}::{len(all_diffs)} difference(s) from '
              f'tools/cortexm/pins.txt')
        return 1
    if all_diffs:
        print(f'REPORT-ONLY: {len(all_diffs)} difference(s) from the pins, not failing '
              f'({"--report-only" if report_only else "ENFORCE is off"})')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
