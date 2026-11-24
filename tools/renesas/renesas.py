#!/usr/bin/env python3
"""
renesas.py -- the test suite under GDB's simulators, with GCC and newlib
built from Ubuntu's source packages: on Renesas cores (VERIFY-101,
VERIFY-102), and on 32-bit PowerPC (VERIFY-119), whose job is powerpc.

tools/renesas/build.sh builds the toolchain of a target from the release
tarballs of Ubuntu's binutils-source, gcc-13-source, newlib-source and
gdb-source: binutils 2.42, GCC 13.3.0, newlib 4.4.0, and the simulator of
GDB 15.1, TARGET-run. Each test of CMakeLists.txt is built with that GCC
and newlib and run under the simulator, which carries its output and its
exit status through newlib's system calls. The variants:

    rh850       v850-elf, -mv850e3v5: RH850's V850E3V5 instruction set,
                Renesas' RH850 ABI (GCC's default), its FPU; linked with
                open.c's _open.
    rh850-soft  the same with -msoft-float: no FPU, float and double in
                libgcc's software.
    rx          rx-elf, -mcpu=rx600 -m64bit-doubles: the RX600, double of
                64 bits (GCC's default is 32), little-endian data, and the
                FPU, GCC's default, with which GCC's RX port turns on
                -ffinite-math-only, which compare.h and random.h refuse
                unless CANON_ALLOW_FINITE_MATH is defined, as it is here
                (VERIFY-110); linked with -msim, libgloss's system calls
                for the simulator, and tmpfile.c's tmpfile().
    rx-nofpu    the same with -nofpu: the tests compiled without the FPU,
                and so without finite math; linked with GCC's and newlib's
                no-fpu-libs, which GCC itself does not link whatever the
                flags (facts(), below), by a folder that -B puts first
                (nofpu_libs, below); neither those libraries nor a test's
                binary may hold an instruction of the FPU (nofpu_check,
                fpu_check; VERIFY-111).
    rx-be       rx-nofpu with -mbig-endian-data: data big-endian, the
                instructions as ever; linked also with crtbegin.o and
                crtend.o, no-fpu-libs', whose .init_array and .fini_array
                are marked as data (crt_fixed, below).
    ppc         powerpc-eabi, -mhard-float: 32-bit PowerPC, big-endian,
                with its FPU, as GCC's default is, under psim, GDB's
                simulator of PowerPC, which models a 604; linked with
                -msim, libgloss's system calls for psim, tmpfile.c's
                tmpfile() (psim's lseek fails), the start file of
                psim_start.S in place of libgloss's, which defines an
                __atexit that newlib 4.4 defines too, and the linker's own
                script with the data sections ending on a word (psim,
                below).
    ppc-soft    the same with -msoft-float: no FPU, float and double in
                libgcc's software, with newlib's and libgcc's nof multilib.

Each is built at the four build types of CMake: Debug (-O0), Release
(-O3), RelWithDebInfo (-O2) and MinSizeRel (-Os), with
CANON_LIFETIME_NO_ATOMICS defined: none of the three GCC ports has a 64-bit
atomic, and their libgcc no __atomic_fetch_add_8, which 32-bit PowerPC's
calls (the V850's has no atomic at all). At
RX, Debug's flags are -O0 without -g: GCC 13.3's RX port stops with an
internal compiler error on most tests at -O0 -g (VERIFY-102). A test
passes when it exits with status 0, within TIMEOUT seconds. Before the
tests, a probe checks that the simulator carries a program's output and
its exit status, and prints the type sizes and alignments as the target
has them.

The tests that do not pass are pinned, with the reason, in pins.txt: each
line a test, the variants and build types it holds for, and what the test
does there: "not-built", "fails N" (N the lines its output begins with
FAIL), or "times-out", stopped at PINNED_TIMEOUT seconds. A test the pins
do not name must pass. An outcome other than its pin's is a difference;
with ENFORCE, a difference fails the run, unless the run is --report-only.
A run that cannot configure, whose probe fails, that finds no test, builds
none, or builds a binary not of the variant's machine and flags, or at
PowerPC of another floating-point ABI than the variant's (its GNU
attribute), always fails; so does one, at RX, whose binaries hold an
instruction of the FPU
where the variant has none, or none at all where it has the FPU
(fpu_check), and one, where the variant links no-fpu-libs, whose link
takes a start file or a library from elsewhere, or whose libraries hold
one (nofpu_check; VERIFY-111).

    python3 tools/renesas/renesas.py run VARIANT [BUILD...] [--report-only]
    python3 tools/renesas/renesas.py check
    python3 tools/renesas/renesas.py target VARIANT
    python3 tools/renesas/renesas.py link VARIANT

"run" needs the variant's toolchain on the PATH (build.sh's PREFIX/bin),
and writes its builds and summaries to renesas_out/. The steps it shares
with tools/msp430/msp430.py (a test's run, its output kept when it is
stopped, why a test is not built, and tmpfile.c) and
tools/cortexm/cortexm.py (CTest's list, FAIL lines) are those scripts'.
"check" is 8 · Records': each pin well formed, naming a test of
CMakeLists.txt, no two pins for the same test and leg, and the support
files there. "target" prints the toolchain's target of a variant, for the
workflow. "link" builds the variant's support in renesas_out/ and prints
the arguments a test is linked with, one a line, for the job that links
the tests compiled by CompCert at ppc-eabi (analysis.yml's compcert-ppc,
VERIFY-119).
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

# Each variant: its toolchain's target, the family its notices are titled
# by (family), its flags, what it links besides (link), what its binaries
# say (machine and data from the ELF header, an e_flags word, the RH850
# ABI's FPU note and PowerPC's floating-point ABI attribute where they have
# them), its support: open.c's _open (open_c), tmpfile.c's tmpfile()
# (tmpfile_c), the corrected crt files (crt_fixed), psim_start.S's start
# file and the padded linker script (psim); whether it links the libraries
# built without the FPU
# (nofpu_libs); what its binaries hold of the FPU's instructions, where
# they are counted: 'some' or 'none' (fpu_insns); the flags of CMake's
# Debug, where the variant sets them (debug); and the defines of its tests
# besides DEFINES (defines).
V850 = {'target': 'v850-elf', 'family': 'renesas', 'link': [], 'machine': 'V850',
        'data': 'little endian', 'e_flags': None, 'float_abi': None, 'open_c': True,
        'tmpfile_c': False, 'crt_fixed': False, 'psim': False, 'nofpu_libs': False,
        'fpu_insns': None, 'debug': None, 'defines': ''}
RX = {'target': 'rx-elf', 'family': 'renesas', 'link': ['-msim'], 'machine': 'Renesas RX',
      'fpu': None, 'e_flags': '64-bit doubles', 'float_abi': None, 'open_c': False,
      'tmpfile_c': True, 'psim': False, 'debug': '-O0', 'nofpu_libs': False,
      'fpu_insns': 'some', 'defines': ''}
PPC = {'target': 'powerpc-eabi', 'family': 'powerpc', 'link': ['-msim'], 'machine': 'PowerPC',
       'data': 'big endian', 'fpu': None, 'e_flags': None, 'open_c': False, 'tmpfile_c': True,
       'crt_fixed': False, 'psim': True, 'nofpu_libs': False, 'fpu_insns': None,
       'debug': None, 'defines': ''}
VARIANTS = {
    'rh850': {**V850, 'flags': '-mv850e3v5', 'fpu': 'FPU-3.0'},
    'rh850-soft': {**V850, 'flags': '-mv850e3v5 -msoft-float', 'fpu': 'not set'},
    # GCC's RX FPU turns on finite math, which compare.h and random.h
    # refuse without CANON_ALLOW_FINITE_MATH (VERIFY-110): defined, so that
    # rx goes on testing what a user of GCC's default gets.
    'rx': {**RX, 'flags': '-mcpu=rx600 -m64bit-doubles', 'data': 'little endian',
           'crt_fixed': False, 'defines': '-DCANON_ALLOW_FINITE_MATH'},
    'rx-nofpu': {**RX, 'flags': '-mcpu=rx600 -m64bit-doubles -nofpu',
                 'data': 'little endian', 'crt_fixed': False, 'nofpu_libs': True,
                 'fpu_insns': 'none'},
    'rx-be': {**RX, 'flags': '-mcpu=rx600 -m64bit-doubles -nofpu -mbig-endian-data',
              'data': 'big endian', 'crt_fixed': True, 'nofpu_libs': True,
              'fpu_insns': 'none'},
    'ppc': {**PPC, 'flags': '-mhard-float', 'float_abi': 'hard float'},
    'ppc-soft': {**PPC, 'flags': '-msoft-float', 'float_abi': 'soft float'},
}
DEFINES = '-DCANON_LIFETIME_NO_ATOMICS'


def defines(variant):
    """The defines of the variant's tests: DEFINES, and its own."""
    return (DEFINES + ' ' + VARIANTS[variant]['defines']).strip()

BUILDS = cortexm.BUILDS
SUPPORT = ('build.sh', 'toolchain.cmake', 'open.c', 'psim_start.S', 'pins.txt', 'README.md')
CRT_ARRAYS = ('.init_array', '.fini_array')
# The linker's emulation at -msim (GCC's link_os_sim spec), and the sections
# of its script that psim_script ends on a multiple of 4: those of data
# that can end within a word.
PSIM_EMULATION = 'elf32ppcsim'
PSIM_PADDED = ('.rodata', '.rodata1', '.sdata2', '.sbss2', '.data', '.data1', '.sdata', '.sbss')
# The RX FPU's instructions, as objdump names them, without a size: RXv1's,
# RXv2's (fsqrt, ftou, utof) and RXv3's of the double-precision FPU (dcmp
# with its condition). Not among them: MVTC and MVFC, which move a control
# register, the FPSW among them; libgloss's crt0 writes the FPSW at every
# multilib, no-fpu-libs' too (VERIFY-111).
FPU_INSN = re.compile(r'fadd|fcmp|fdiv|fmul|fsub|ftoi|itof|round|fsqrt|ftou|utof'
                      r'|dabs|dadd|dcmp\w*|ddiv|dmov|dmul|dneg|dpopm|dpushm|dround'
                      r'|dsqrt|dsub|dtof|dtoi|dtou|ftod|itod|utod|mvfdc|mvtdc|mvfdr')
FPSW = re.compile(r'(mvtc|mvfc)\t.*\bfpsw\b')
WRAP_TMPFILE = '-Wl,--wrap=tmpfile'
PIN = re.compile(r'^(\w+)\s+(\S+)\s+(\S+)\s+(not-built|fails|times-out)(?:\s+(\d+))?$')
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
# The tmpfile() part, compiled where the variant links tmpfile.c: what
# newlib's tmpfile() and tmpfile.c's do, written, rewound and read back.
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
    fprintf(stderr, "file probe: remove %d\n", remove("renesas-file-probe.txt"));
#ifdef CANON_PROBE_TMPFILE
    {
        char back[16] = {0};
        FILE *t = tmpfile();
        fprintf(stderr, "file probe: tmpfile %s\n", t ? "a stream" : "NULL");
        if (t != NULL) {
            fputs("canon", t);
            fprintf(stderr, "file probe: fseek %d\n", fseek(t, 0L, SEEK_SET));
            (void)fgets(back, (int)sizeof back, t);
            (void)fclose(t);
            fprintf(stderr, "file probe: tmpfile read back \"%s\"\n", back);
        }
    }
#endif
    return strcmp(buf, "canon") == 0 ? 0 : 3;
}
'''
FILE_PROBE_LIMIT = 30   # seconds the file probe runs

sh, rel, shown, described = cortexm.sh, cortexm.rel, cortexm.shown, cortexm.described
run_test, why_not_built = msp430.run_test, msp430.why_not_built


def tool(variant, name):
    return VARIANTS[variant]['target'] + '-' + name


def family(variant):
    """What the variant's notices are titled by: renesas, or powerpc."""
    return VARIANTS[variant]['family']


def read_pins():
    """[{line, test, variants, builds, outcome, fails}], and the problems."""
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
            test, variants, builds, outcome, fails = m.groups()
            variants = tuple(VARIANTS) if variants == 'all' else tuple(variants.split(','))
            if any(v not in VARIANTS for v in variants):
                problems.append(f'pins.txt:{n}: variants "all" or of {", ".join(VARIANTS)}')
                continue
            builds = BUILDS if builds == 'all' else tuple(builds.split(','))
            if any(b not in BUILDS for b in builds):
                problems.append(f'pins.txt:{n}: build types "all" or of {", ".join(BUILDS)}')
                continue
            if (outcome == 'fails') != (fails is not None):
                problems.append(f'pins.txt:{n}: "fails" takes a count of FAIL lines, '
                                f'"not-built" and "times-out" none')
                continue
            pins.append({'line': n, 'test': test, 'variants': variants, 'builds': builds,
                         'outcome': outcome,
                         'fails': int(fails) if fails is not None else None})
    return pins, problems


def matching(pins, test, variant, build):
    return [p for p in pins if p['test'] == test and variant in p['variants']
            and build in p['builds']]


def expected(pins, test, variant, build):
    m = matching(pins, test, variant, build)
    return (m[0]['outcome'], m[0]['fails']) if m else ('passes', None)


def versions(variant):
    out = []
    for name in ('gcc', 'run'):
        r = sh([tool(variant, name), '--version'])
        text = (r.stdout or r.stderr).strip()
        if r.returncode != 0 and text:
            # RX's simulator has no --version: GDB's, as build.sh built it
            text = f'{tool(variant, name)}: no --version'
        out.append(text.splitlines()[0] if text else f'{tool(variant, name)}: not found')
    r = sh(['sh', '-c', 'command -v ' + tool(variant, 'gcc')])
    out.append(tool(variant, 'gcc') + ': ' + (r.stdout.strip() or 'not on the PATH'))
    return out


def facts(variant):
    """newlib's version; the folder of the multilib GCC links for the
    variant's flags, its newlib and libgcc, and the folder the variant links
    instead (nofpu_libs); whether it compiles with finite math; and the
    compiler's macros of the target. At RX, GCC 13.3 links no folder of
    no-fpu-libs, whatever the flags: t-rx's MULTILIB_MATCHES maps -mnofpu,
    and -mcpu=rx200 and rx100, to "nofpu", which is no multilib option, so
    -nofpu links the libraries built for the FPU."""
    flags = VARIANTS[variant]['flags'].split()
    r = sh([tool(variant, 'gcc'), *flags, '-E', '-dM', '-x', 'c', '-'],
           input='#include <stdio.h>\n')
    macros = dict(re.findall(r'^#define (\w+)(?: (.*))?$', r.stdout, re.M))
    version = macros.get('_NEWLIB_VERSION', '?').strip('"')
    keys = [k for k in sorted(macros) if re.match(r'__(v850|V850|RH850|RX)\w*__$|_ARCH_PPC\w*$'
                                                  r'|_SOFT_FLOAT$|__NO_FPRS__$', k)]
    multi = sh([tool(variant, 'gcc'), *flags, '-print-multi-directory']).stdout.strip()
    libs = multi or '?'
    if VARIANTS[variant]['nofpu_libs']:
        try:
            libs = f'{nofpu_multi(variant)}, for GCC\'s {multi}'
        except RuntimeError:   # support() says why, as an error
            libs = f'?, for GCC\'s {multi}'
    return (f'newlib {version}; libraries {libs}; __FINITE_MATH_ONLY__ '
            f'{macros.get("__FINITE_MATH_ONLY__", "?")}; ' + ', '.join(keys))


def probe(variant, out, link):
    """Build and run a program that prints the target's sizes and alignments
    and exits with PROBE_STATUS, linked as the tests are: the simulator must
    carry both, or no test's result means anything. Returns its line of
    output."""
    d = os.path.abspath(os.path.join(out, 'probe-' + variant))
    os.makedirs(d, exist_ok=True)
    src, elf = os.path.join(d, 'probe.c'), os.path.join(d, 'probe')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(PROBE.replace('renesas probe:', f'{family(variant)} probe:'))
    r = sh([tool(variant, 'gcc'), *VARIANTS[variant]['flags'].split(), '-std=c99', '-O1', src,
            *link, '-o', elf])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: the probe does not build')
    got = run_test({'command': [tool(variant, 'run'), elf], 'cwd': d}, 60)
    lines = [line for line in got['tail'] if line.startswith(f'{family(variant)} probe:')]
    if got['outcome'] == 'times-out' or got['rc'] != PROBE_STATUS or not lines:
        shown('\n'.join(got['tail']))
        raise RuntimeError(f'{variant}: under the simulator the probe exits with '
                           f'{got["rc"]}, not {PROBE_STATUS}, or prints no line: the '
                           f'simulator does not carry a program\'s output and exit status')
    return lines[0]


def compiled(variant, src, d):
    """A support file compiled for the variant, its warnings errors: the
    object."""
    obj = os.path.join(d, os.path.splitext(os.path.basename(src))[0] + '.o')
    r = sh([tool(variant, 'gcc'), *VARIANTS[variant]['flags'].split(), '-std=c99', '-O2',
            '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-c', src, '-o', obj])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: {rel(src)} does not compile')
    return obj


def support_folder(variant, out):
    return os.path.abspath(os.path.join(out, 'support-' + variant))


def support(variant, out):
    """What each test of the variant is linked with besides its own objects:
    the variant's link flags; open.c's object (open_c); tmpfile.c's, with
    -Wl,--wrap=tmpfile, which puts its tmpfile() in place of newlib's
    (tmpfile_c); the folder of the corrected crt files (crt_fixed); and
    that of the libraries built without the FPU (nofpu_libs), after it, so
    that the corrected files come first. Returns the arguments of the
    link."""
    v = VARIANTS[variant]
    d = support_folder(variant, out)
    os.makedirs(d, exist_ok=True)
    link = list(v['link'])
    if v['open_c']:
        link.append(compiled(variant, os.path.join(HERE, 'open.c'), d))
    if v['tmpfile_c']:
        link += [WRAP_TMPFILE, compiled(variant, msp430.TMPFILE, d)]
    nofpu = nofpu_libs(variant, d) if v['nofpu_libs'] else None
    if v['crt_fixed']:
        link.append('-B' + crt_fixed(variant, d, nofpu) + os.sep)
    if nofpu is not None:
        link.append('-B' + nofpu + os.sep)
    if v['psim']:
        link += ['-B' + psim_start(variant, d) + os.sep, '-Wl,-T,' + psim_script(variant, d)]
    return link


def psim_start(variant, d):
    """libgloss's sim-crt0.o, which GCC links at -msim, defines __atexit,
    which newlib 4.4 defines too, and no program links (psim_start.S).
    psim_start.S assembled for the variant as sim-crt0.o, in a folder that
    -B puts before GCC's, under the variant's multilib folder, where the
    driver looks first: returns the folder."""
    flags = VARIANTS[variant]['flags'].split()
    multi = sh([tool(variant, 'gcc'), *flags, '-print-multi-directory']).stdout.strip()
    folder = os.path.join(d, 'start')
    os.makedirs(os.path.join(folder, multi), exist_ok=True)
    obj = os.path.join(folder, multi, 'sim-crt0.o')
    r = sh([tool(variant, 'gcc'), *flags, '-c', os.path.join(HERE, 'psim_start.S'), '-o', obj])
    if r.returncode != 0 or not multi:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: psim_start.S does not assemble')
    return folder


def psim_script(variant, d):
    """psim maps a program section by section, each its exact size
    (map-binary, sim/ppc/hw_init.c), and stops a program with signal 11 on
    a load that passes a section's end, or spans two sections. newlib's
    strlen reads the aligned word that holds a string's end, so a test
    stops whose string ends a section within a word: which test depends on
    the layout (VERIFY-119). The linker's own script for -msim (ld --verbose
    -m elf32ppcsim), with each of PSIM_PADDED ending on a multiple of 4, in
    the support folder: returns its path. attributes() checks that every
    section of a test's binary begins and ends on one."""
    r = sh([tool(variant, 'ld'), '--verbose', '-m', PSIM_EMULATION])
    parts = r.stdout.split('=' * 50)
    if r.returncode != 0 or len(parts) != 3:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: ld gives no script of {PSIM_EMULATION}')
    script = parts[1]
    for name in PSIM_PADDED:
        m = re.search(r'^\s*' + re.escape(name) + r'\s*:', script, re.M)
        start = script.find('{', m.end()) if m else -1
        if start < 0:
            raise RuntimeError(f'{variant}: no {name} in the script of {PSIM_EMULATION}')
        depth, end = 0, None
        for i in range(start, len(script)):
            depth += {'{': 1, '}': -1}.get(script[i], 0)
            if depth == 0:
                end = i
                break
        if end is None:
            raise RuntimeError(f'{variant}: {name} has no end in the script')
        script = script[:end] + ' . = ALIGN(4); ' + script[end:]
    path = os.path.join(d, 'psim.ld')
    with open(path, 'w', encoding='utf-8') as f:
        f.write(script)
    return path


def unaligned_sections(variant, binary):
    """The sections of a binary, loaded into memory and not empty, that
    begin or end off a multiple of 4: psim stops a word's load across
    them (psim_script)."""
    r = sh([tool(variant, 'readelf'), '-S', '-W', binary])
    found = []
    for name, addr, size, flags in re.findall(
            r'^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\S+\s+(\S*)',
            r.stdout, re.M):
        a, n = int(addr, 16), int(size, 16)
        if 'A' in flags and n and (a % 4 or (a + n) % 4):
            found.append(f'{name} at {a:#x}, {n} bytes')
    return found


def nofpu_multi(variant):
    """The multilib folder GCC builds without the FPU for the variant's
    flags: the one -print-multi-lib gives the options of GCC's own choice
    and -mnofpu."""
    flags = VARIANTS[variant]['flags'].split()
    multi = sh([tool(variant, 'gcc'), *flags, '-print-multi-directory']).stdout.strip()
    table = {}
    for line in sh([tool(variant, 'gcc'), '-print-multi-lib']).stdout.split():
        folder, _, opts = line.partition(';')
        table[folder] = frozenset(o for o in opts.split('@') if o)
    if multi not in table:
        raise RuntimeError(f'{variant}: GCC\'s multilib {multi} is not in -print-multi-lib')
    want = table[multi] | {'mnofpu'}
    found = [f for f, o in table.items() if o == want]
    if len(found) != 1:
        raise RuntimeError(f'{variant}: no one multilib of {sorted(want)}: {found}')
    return found[0]


def nofpu_libs(variant, d):
    """GCC's RX port links the FPU's multilib whatever the flags (facts()).
    Copies of every file of the multilib folder without the FPU
    (nofpu_multi), libgcc's (crtbegin.o, crtend.o, libgcc.a) and newlib's
    (crt0.o, libc.a, libm.a, libsim.a and the rest), under the name of
    GCC's own multilib, in a folder that -B puts first: the driver looks
    there before its own folders, for the start files and the libraries
    alike (VERIFY-111). Returns the folder."""
    flags = VARIANTS[variant]['flags'].split()
    multi = sh([tool(variant, 'gcc'), *flags, '-print-multi-directory']).stdout.strip()
    nofpu = nofpu_multi(variant)
    folder = os.path.join(d, 'nofpu')
    into = os.path.join(folder, multi)
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(into)
    for name in ('libgcc.a', 'libc.a'):
        path = sh([tool(variant, 'gcc'), *flags, '-print-file-name=' + name]).stdout.strip()
        suffix = os.path.join(multi, name)
        if not os.path.isabs(path) or not path.endswith(os.sep + suffix):
            raise RuntimeError(f'{variant}: {name} is not under its multilib folder: {path}')
        source = os.path.join(path[:-len(suffix)], nofpu)
        if not os.path.exists(os.path.join(source, name)):
            raise RuntimeError(f'{variant}: no {name} in {source}')
        for f in sorted(os.listdir(source)):
            if not os.path.isfile(os.path.join(source, f)):
                continue
            if os.path.exists(os.path.join(into, f)):
                raise RuntimeError(f'{variant}: {f} is both libgcc\'s and newlib\'s')
            shutil.copy2(os.path.join(source, f), os.path.join(into, f))
    return folder


def crt_fixed(variant, d, nofpu=None):
    """GCC's RX port declares .init_array and .fini_array executable
    ("awx", rx.h's INIT_ARRAY_SECTION_ASM_OP), so crtbegin.o and crtend.o
    carry them as code; the simulator's linker script puts them into .data,
    which so becomes code; and binutils stores the code of a big-endian RX
    executable with its 32-bit words swapped, data and all, which the
    simulator loads as the file holds it. Copies of the two files whose
    arrays are marked as data, in a folder that -B puts before GCC's, under
    the variant's multilib folder; made from those of the folder without the
    FPU, when given (nofpu_libs): returns the folder."""
    flags = VARIANTS[variant]['flags'].split()
    multi = sh([tool(variant, 'gcc'), *flags, '-print-multi-directory']).stdout.strip()
    folder = os.path.join(d, 'crt')
    os.makedirs(os.path.join(folder, multi), exist_ok=True)
    for name in ('crtbegin.o', 'crtend.o'):
        if nofpu is not None:
            src = os.path.join(nofpu, multi, name)
        else:
            src = sh([tool(variant, 'gcc'), *flags, '-print-file-name=' + name]).stdout.strip()
        args = []
        for sec in CRT_ARRAYS:
            args += ['--set-section-flags', sec + '=alloc,load,contents,data']
        r = sh([tool(variant, 'objcopy'), *args, src, os.path.join(folder, multi, name)])
        if r.returncode != 0 or not os.path.isabs(src):
            shown(r.stdout + r.stderr)
            raise RuntimeError(f'{variant}: {name} cannot be copied with its arrays as data')
    return folder


def file_probe(variant, out, link):
    """A program that writes a file, reads it back and removes it, its steps
    on stderr, and, where tmpfile.c is linked, does the same with a
    tmpfile(); linked as the tests are, and, for the support the variant
    links in place of newlib's (open.c's _open, tmpfile.c's tmpfile()),
    also without it: what each does under the simulator. It fails nothing.
    Returns one line per link."""
    v = VARIANTS[variant]
    d = os.path.abspath(os.path.join(out, 'file-probe-' + variant))
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, 'file_probe.c')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(FILE_PROBE)
    defines = ['-DCANON_PROBE_TMPFILE'] if v['tmpfile_c'] else []
    if v['open_c']:
        links = [("newlib's _open", [a for a in link if not a.endswith(os.sep + 'open.o')]),
                 ("open.c's _open", link)]
    elif v['tmpfile_c']:
        links = [("newlib's tmpfile", [a for a in link if a != WRAP_TMPFILE
                                       and not a.endswith(os.sep + 'tmpfile.o')]),
                 ("tmpfile.c's tmpfile", link)]
    else:
        links = [("as the tests link", link)]
    lines = []
    for i, (label, extra) in enumerate(links, 1):
        elf = os.path.join(d, f'file_probe_{i}')
        r = sh([tool(variant, 'gcc'), *v['flags'].split(), '-std=c99', '-O1', *defines, src,
                *extra, '-o', elf])
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


def environment(variant, link):
    env = dict(os.environ)
    env['CANON_RS_TARGET'] = VARIANTS[variant]['target']
    env['CANON_RS_FLAGS'] = (VARIANTS[variant]['flags'] + ' ' + defines(variant)).strip()
    env['CANON_RS_LINK'] = ' '.join(link)
    env['CANON_RS_RUN'] = tool(variant, 'run')
    return env


def configure_and_build(variant, build, out, env):
    b = os.path.abspath(os.path.join(out, f'{variant}-{build}'))
    shutil.rmtree(b, ignore_errors=True)
    args = ['cmake', '-S', ROOT, '-B', b, '-G', 'Unix Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE=' + os.path.join(HERE, 'toolchain.cmake'),
            '-DCMAKE_BUILD_TYPE=' + build]
    if VARIANTS[variant]['debug'] is not None:
        # In the cache before CMake's own Debug flags, -g, which it then
        # does not set.
        args.append('-DCMAKE_C_FLAGS_DEBUG=' + VARIANTS[variant]['debug'])
    r = sh(args, env=env)
    if r.returncode != 0:
        shown('\n'.join((r.stdout + r.stderr).splitlines()[-30:]))
        raise RuntimeError(f'{variant} {build}: CMake does not configure')
    # -k: the tests that build are built, whatever does not.
    r = sh(['cmake', '--build', b, '--parallel', str(os.cpu_count() or 2), '--', '-k'], env=env)
    with open(b + '.build.log', 'w') as f:
        f.write(r.stdout + r.stderr)
    return b


def attributes(b, name, binary, variant, build):
    """The binary's class, byte order, machine and flags; the notes the RH850
    ABI's binaries carry (the alignment of 8-byte objects, the FPU the code
    needs); PowerPC's floating-point ABI, its GNU attribute (hard float or
    soft float); whether its test was compiled with the variant's flags, and at
    Debug with the variant's Debug flags and no -g (CMake's flags.make); and
    their problems."""
    v = VARIANTS[variant]
    r = sh([tool(variant, 'readelf'), '-h', binary])
    head = dict(re.findall(r'^\s*(Class|Data|Machine|Flags):\s*(.*?)\s*$', r.stdout, re.M))
    notes = dict(re.findall(r'^\s*(Alignment of 8-byte objects|Type of FPU support needed):'
                            r'\s*(.*?)\s*$', sh([tool(variant, 'readelf'), '-n', binary]).stdout,
                            re.M))
    m = re.search(r'^\s*Tag_GNU_Power_ABI_FP:\s*(.*?)\s*$',
                  sh([tool(variant, 'readelf'), '-A', binary]).stdout, re.M)
    float_abi = m.group(1) if m else None
    problems = []
    if head.get('Class') != 'ELF32' or v['machine'].upper() not in head.get('Machine', '').upper() \
            or v['data'] not in head.get('Data', ''):
        problems.append(f'it is {head.get("Class")}, {head.get("Data")}, {head.get("Machine")}')
    if v['fpu'] is not None and notes.get('Type of FPU support needed') != v['fpu']:
        problems.append(f'its FPU note is "{notes.get("Type of FPU support needed")}", not '
                        f'"{v["fpu"]}"')
    if v['e_flags'] is not None and v['e_flags'] not in head.get('Flags', ''):
        problems.append(f'its flags are "{head.get("Flags")}", without "{v["e_flags"]}"')
    if v['float_abi'] is not None and not (float_abi or '').startswith(v['float_abi'] + ','):
        problems.append(f'its floating-point ABI is "{float_abi}", not {v["float_abi"]}')
    if v['psim']:
        problems += [f'its section {u} begins or ends off a word: psim stops a load '
                     f'across it' for u in unaligned_sections(variant, binary)]
    flags = os.path.join(b, 'CMakeFiles', name + '.dir', 'flags.make')
    text = open(flags, encoding='utf-8').read() if os.path.exists(flags) else ''
    if v['flags'] not in text:
        problems.append(f'its flags.make has no {v["flags"]}')
    debug = v['debug'] is not None and build == 'Debug'
    if debug:
        m = re.search(r'^C_FLAGS = (.*)$', text, re.M)
        words = m.group(1).split() if m else []
        if '-g' in words or any(w not in words for w in v['debug'].split()):
            problems.append(f'its C_FLAGS are "{m.group(1) if m else ""}", not with '
                            f'{v["debug"]} and without -g')
    what = f'{head.get("Class")}, {head.get("Data")}, {head.get("Machine")}; '
    if v['fpu'] is not None:
        what += (f'notes: 8-byte objects aligned {notes.get("Alignment of 8-byte objects")}, '
                 f'FPU {notes.get("Type of FPU support needed")}; ')
    else:
        what += f'flags {head.get("Flags")}; '
    if v['float_abi'] is not None:
        what += f'floating-point ABI {float_abi}; '
    what += f'compiled with {v["flags"]}'
    return what + (f', and {v["debug"]} without -g' if debug else ''), problems


def fpu_insns(variant, binary):
    """The instructions of the RX FPU in the code of a binary, an object or
    an archive, by objdump: (their number, the FPSW's moves). A section
    that is written is not code, whatever its flags say: GCC's RX port
    marks .init_array and .fini_array executable (crt_fixed), and the
    simulator's linker script puts them into .data, which objdump would
    disassemble."""
    r = sh([tool(variant, 'objdump'), '-h', '-w', binary])
    sections = list(dict.fromkeys(
        name for name, flags in re.findall(r'^\s*\d+\s+(\S+)\s.*\s2\*\*\d+\s+(.*)$',
                                           r.stdout, re.M)
        if 'CODE' in flags and 'READONLY' in flags))
    if r.returncode != 0 or not sections:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: objdump finds no code in {rel(binary)}')
    r = sh([tool(variant, 'objdump'), '-d', *('-j' + name for name in sections), binary])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: objdump does not disassemble {rel(binary)}')
    fpu = fpsw = 0
    for line in r.stdout.splitlines():
        parts = line.split('\t')
        if len(parts) < 3 or not parts[2].strip():
            continue
        if FPU_INSN.fullmatch(parts[2].split()[0].split('.')[0]):
            fpu += 1
        elif FPSW.match('\t'.join(parts[2:])):
            fpsw += 1
    return fpu, fpsw


def nofpu_check(variant, out, link):
    """Where the variant links no-fpu-libs (nofpu_libs): the start files and
    the libraries that a program linked as the tests are takes, by ld's
    trace, each of which must be the support folder's; the FPU's
    instructions in those and in every library and start file of the
    folder without the FPU, which must be none; and in GCC's own files of
    the same names, which must be some, or the count would see nothing
    (VERIFY-111). Returns the notice line, or raises."""
    flags = VARIANTS[variant]['flags'].split()
    d = os.path.realpath(support_folder(variant, out))
    p = os.path.abspath(os.path.join(out, 'probe-' + variant))
    src, obj, elf = (os.path.join(p, name) for name in ('probe.c', 'probe.o', 'probe-traced'))
    r = sh([tool(variant, 'gcc'), *flags, '-std=c99', '-O1', '-c', src, '-o', obj])
    if r.returncode == 0:
        r = sh([tool(variant, 'gcc'), *flags, obj, *link, '-Wl,-t', '-o', elf])
    if r.returncode != 0:
        shown(r.stdout + r.stderr)
        raise RuntimeError(f'{variant}: the probe does not link with ld\'s trace')
    traced = list(dict.fromkeys(os.path.realpath(line.strip())
                                for line in (r.stdout + r.stderr).splitlines()
                                if line.strip().endswith(('.o', '.a'))))
    traced = [t for t in traced if t != os.path.realpath(obj)]
    outside = [t for t in traced if not t.startswith(d + os.sep)]
    if not traced or outside:
        raise RuntimeError(f'{variant}: a program linked as the tests are takes '
                           + (', '.join(outside[:SHOWN]) or 'nothing') + ' from outside '
                           + rel(d))
    multi = sh([tool(variant, 'gcc'), *flags, '-print-multi-directory']).stdout.strip()
    folder = os.path.join(d, 'nofpu', multi)
    files = list(dict.fromkeys(traced + sorted(
        os.path.join(folder, f) for f in os.listdir(folder) if f.endswith(('.a', '.o')))))
    ours = {f: fpu_insns(variant, f)[0] for f in files}
    gccs = {}
    for name in dict.fromkeys(os.path.basename(f) for f in files):
        path = sh([tool(variant, 'gcc'), *flags, '-print-file-name=' + name]).stdout.strip()
        if os.path.isabs(path) and os.path.exists(path):
            gccs[name] = fpu_insns(variant, path)[0]
    held = [f for f, n in ours.items() if n]
    if held:
        raise RuntimeError(f'{variant}: {sum(ours.values())} instruction(s) of the FPU in what '
                           f'the tests link: ' + ', '.join(f'{rel(f)} {ours[f]}' for f in held))
    if not any(gccs.values()):
        raise RuntimeError(f'{variant}: no instruction of the FPU in GCC\'s {multi} libraries '
                           f'either: objdump\'s are not counted')
    return ('linked: ' + ', '.join(os.path.basename(t) for t in traced)
            + f', all from the support folder ({nofpu_multi(variant)}); FPU instructions: 0 in '
            f'those and the folder\'s other files, {len(files)} in all; {sum(gccs.values())} in '
            f'GCC\'s own {multi} files of the same names ('
            + ', '.join(f'{n} {c}' for n, c in gccs.items() if c) + ')')


def fpu_check(variant, build, built):
    """The FPU's instructions in every built test, against the variant's
    fpu_insns: 'none' fails on any, 'some' on none in all (the count would
    then see nothing). Returns the notice's part, or raises."""
    want = VARIANTS[variant]['fpu_insns']
    counts = {t['name']: fpu_insns(variant, t['command'][-1]) for t in built}
    holding = sorted((n for n, (fpu, _) in counts.items() if fpu), key=lambda n: -counts[n][0])
    total = sum(fpu for fpu, _ in counts.values())
    moves = sum(fpsw for _, fpsw in counts.values())
    print(f'--- {variant} {build}: FPU instructions: {total} in {len(holding)} of {len(built)} '
          f'binaries' + (' (' + ', '.join(f'{n} {counts[n][0]}' for n in holding[:10])
                         + (', ...' if len(holding) > 10 else '') + ')' if holding else '')
          + f'; FPSW moves (MVTC, MVFC), not counted: {moves}')
    if want == 'none' and holding:
        raise RuntimeError(f'{variant} {build}: {total} instruction(s) of the FPU, in '
                           f'{len(holding)} of {len(built)} binaries ('
                           + ', '.join(holding[:SHOWN]) + '): the variant has no FPU')
    if want == 'some' and not holding:
        raise RuntimeError(f'{variant} {build}: no instruction of the FPU in {len(built)} '
                           f'binaries, where the variant has the FPU: objdump\'s are not '
                           f'counted')
    return f'FPU instructions in {len(holding)} of {len(built)} binaries'


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
    attrs, attr_problems = attributes(b, built[0]['name'], built[0]['command'][-1], variant,
                                      build)
    if attr_problems:
        raise RuntimeError(f'{variant} {build}: {built[0]["name"]} is not the variant\'s: '
                           + '; '.join(attr_problems))
    fpu = (fpu_check(variant, build, built) if VARIANTS[variant]['fpu_insns'] is not None
           else None)
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
    line = (f'{family(variant)} {variant} {build}: {len(rows)} tests, {len(passed)} passed, '
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
    if fpu is not None:
        line += '; ' + fpu
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
            'differences': diffs, 'attributes': attrs, 'fpu': fpu, 'line': line}
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
    if not os.path.exists(msp430.TMPFILE):
        problems.append(f'{rel(msp430.TMPFILE)}: no such file')
    for p in problems:
        print('  ' + p)
    print(f'renesas check: {len(pins)} pins, {len(tests)} tests in CMakeLists.txt; '
          f'{len(problems)} problem(s)')
    return 1 if problems else 0


def main(argv):
    if argv[:1] == ['check'] and len(argv) == 1:
        return check()
    if argv[:1] == ['target'] and len(argv) == 2 and argv[1] in VARIANTS:
        print(VARIANTS[argv[1]]['target'])
        return 0
    if argv[:1] == ['link'] and len(argv) == 2 and argv[1] in VARIANTS:
        try:
            link = support(argv[1], OUT)
        except RuntimeError as e:
            print(f'::error title={family(argv[1])} {argv[1]}::{e}')
            return 1
        print('\n'.join(link))
        return 0
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
    debug = VARIANTS[variant]['debug']
    print(f'Variant: {variant}; flags: {VARIANTS[variant]["flags"]} {defines(variant)}'.rstrip()
          + (f'; Debug: {debug}, without -g' if debug is not None else ''))
    pins, problems = read_pins()
    os.makedirs(OUT, exist_ok=True)
    all_diffs = list(problems)
    try:
        for name in ('gcc', 'run', 'readelf', 'objcopy', 'objdump', 'ld'):
            if not shutil.which(tool(variant, name)):
                raise RuntimeError(f'{tool(variant, name)} must be on the PATH: '
                                   f'tools/renesas/build.sh builds it')
        link = support(variant, OUT)
        line = probe(variant, OUT, link)
        print(line)
        print(f'::notice title={family(variant)} {variant} probe::{line}')
        if VARIANTS[variant]['nofpu_libs']:
            line = nofpu_check(variant, OUT, link)
            print(line)
            print(f'::notice title={family(variant)} {variant} libraries::{line}')
        for line in file_probe(variant, OUT, link):
            print(line)
            print(f'::notice title={family(variant)} {variant} file probe::{line}')
        env = environment(variant, link)
        for build in builds:
            line, diffs, _ = leg(variant, build, OUT, env, pins)
            print(f'::notice title={family(variant)} {variant} {build}::{line}')
            all_diffs += diffs
    except RuntimeError as e:
        print(f'::error title={family(variant)} {variant}::{e}')
        return 1
    if all_diffs and ENFORCE and not report_only:
        print(f'::error title={family(variant)} {variant}::{len(all_diffs)} difference(s) from '
              f'tools/renesas/pins.txt')
        return 1
    if all_diffs:
        print(f'REPORT-ONLY: {len(all_diffs)} difference(s) from the pins, not failing '
              f'({"--report-only" if report_only else "ENFORCE is off"})')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
