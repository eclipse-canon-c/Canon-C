# The tests on bare-metal AArch64 under QEMU: `aarch64`

`2 · Platforms`' job `build-arm64` runs the tests on AArch64 Linux, with
glibc. The job `aarch64` runs them with no operating system (VERIFY-099):
each test of `CMakeLists.txt` is built with Ubuntu 24.04's
`aarch64-linux-gnu-gcc` and picolibc in place of glibc
(`--specs=picolibc.specs`), and run under `qemu-system-aarch64`'s `virt`
board through semihosting, which carries the output and the exit status
to the host. A test passes when it exits with status 0 within 120 s.

| core | flag | architecture |
|------|------|--------------|
| `a78ae` | `-mcpu=cortex-a78ae` | Armv8.2-A, A-profile: the Cortex-A78's automotive variant, in application processors for cars |
| `r82` | `-mcpu=cortex-r82` | Armv8-R AArch64, R-profile: Arm's 64-bit real-time core |

QEMU 8.2 models neither core. Each is compiled for with its flag and run
on QEMU's `max` CPU, which has the instructions of both: what the job
holds is the code GCC generates for each core, run. It does not hold the
core's timing, nor its memory system: the tests run on QEMU's A-profile
CPU, its MMU off (below). picolibc's library is the package's one build,
the same at both cores and built without LSE: the core's flag reaches the
tests and Canon-C's code, not picolibc's.

Each core is built at the four build types of CMake, Debug (`-O0`),
Release (`-O3`), RelWithDebInfo (`-O2`) and MinSizeRel (`-Os`). Nothing
is defined for `lifetime.h`: both cores have Armv8.1's atomics (LSE), to
which its token counter's 64-bit atomic compiles (`ldadd`). Before the
tests, a probe checks that QEMU carries a program's output and its exit
status, and prints the core's macros as GCC has them (architecture,
profile, atomics), the type sizes, whether `char` is signed, the
exception level and its `SCTLR`'s M and A bits (the MMU and the alignment
check), and an unaligned load's result. The script checks that the
binaries are 64-bit little-endian AArch64 and that each test was compiled
with the core's flag, prints each test's seconds, and keeps the output a
stopped test had written.

**The memory.** picolibc's linker script, its memory set inside the
board's RAM (0x40000000, 128 MB): 16 MB of code from 0x40000000, then
16 MB of data, a 256 KB stack among it, the stack the Cortex-M job's
picolibc legs give the tests. picolibc 1.8.6's semihosting start file
(`--crt0=semihost`) enables the floating point, sets the thread pointer
and leaves the MMU off: the tests run at EL1, the MMU and the alignment
check off. With its MMU off, an A-profile core treats data memory as
Device memory, where an unaligned access faults. QEMU 8.2 does not fault
it (the probe's unaligned load reads its four bytes), so the tests run as
on a core whose MMU maps memory as normal, where AArch64 allows an
unaligned access; an unaligned access that would fault with the MMU off
is not caught here.

**The toolchain.** Ubuntu 24.04's packages, signed by its archive:
`gcc-aarch64-linux-gnu` (GCC 13.3), `picolibc-aarch64-linux-gnu`
(picolibc 1.8.6) and `qemu-system-arm` (QEMU 8.2.2, which holds
`qemu-system-aarch64`), installed without the packages apt recommends,
from the Actions cache first (`.github/actions/apt-install`). The compiler
is the one Ubuntu builds for AArch64 Linux: picolibc's specs put its
headers first, and the compiler still searches the system's after them,
and pre-includes glibc's `stdc-predef.h`. Without glibc's AArch64 headers,
which apt only recommends, that file is the host's; its macros
(`__STDC_IEC_559__`, `__STDC_ISO_10646__`) are glibc's claims, and nothing
in Canon-C reads them. No other system header reaches a test:
VERIFY-099's measure found every other header picolibc's or GCC's, and
each of its 464 links reading picolibc's start file, `libc.a` and
`libsemihost.a`, and GCC's `libgcc.a`, nothing else.

**The pins**, `pins.txt`: the tests that do not pass, each with the cores
and build types it holds for, what the test does there (`not-built`;
`fails N`, N the lines of its output that begin with `FAIL`; or
`times-out`, stopped at 60 s rather than 120), and why. A test the pins do
not name must pass. An outcome other than its pin's is a difference; with
`ENFORCE` (in the script) a difference fails the job, unless the run is
`--report-only`. Three tests are not built, at both cores and every build
type, as with picolibc on Cortex-M: `time_test` (picolibc declares
`clock_gettime` and `nanosleep` only where `_POSIX_TIMERS` is defined,
which it is not on bare metal), and `file_test` and `impl_defined_test`
(picolibc's semihosting has no `rename`).

    python3 tools/aarch64/aarch64.py run CORE [BUILD...] [--report-only]
    python3 tools/aarch64/aarch64.py check

`run` builds into `aarch64_out/`; `check` is `8 · Records'`. The steps the
script shares with `tools/msp430/msp430.py` (a test's run, and why a test
is not built) and with `tools/cortexm/cortexm.py` (CTest's list of the
tests and their `FAIL` lines) are those scripts'.

**Runs.** Enforced from VERIFY-099's run 1: its pins were measured before
it, at both cores and each build type, with the packages the job installs
and without glibc's AArch64 headers, as the runner has them. The
predictions and their scores are in `docs/deviations.md`.
