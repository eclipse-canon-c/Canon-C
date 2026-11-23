# The tests on bare-metal Cortex-R5F and Cortex-R52 on QEMU: `cortex-r`

`2 · Platforms`' job `armhf` runs the tests on 32-bit Arm Linux, an
A-profile core. The job `cortex-r` runs them on Arm's 32-bit real-time
cores, the R-profile, with no operating system (VERIFY-113): each test of
`CMakeLists.txt` is built with Ubuntu 24.04's `arm-none-eabi-gcc` and
picolibc (`--specs=picolibc.specs`), and run on `qemu-system-arm`'s model
of the core through semihosting, which carries the output and the exit
status to the host. A test passes when it exits with status 0 within
120 s.

| core | flags | QEMU's CPU | architecture |
|------|-------|------------|--------------|
| `r5` | `-mcpu=cortex-r5 -mfpu=vfpv3-d16` | `cortex-r5f` | Armv7-R, with the R5F's FPU, of single and double precision |
| `r52` | `-mcpu=cortex-r52 -mfpu=neon-fp-armv8` | `cortex-r52` | Armv8-R AArch32, with its FPU and Advanced SIMD, and a hypervisor level (EL2) |

Both are compiled to Thumb-2 with the hard-float ABI (`-mthumb
-mfloat-abi=hard`), and built at the four build types of CMake, Debug
(`-O0`), Release (`-O3`), RelWithDebInfo (`-O2`) and MinSizeRel (`-Os`).
GCC links picolibc's `thumb/v7+fp/hard` multilib at both. Nothing is
defined for `lifetime.h`: both cores have LDREXD and STREXD, to which its
token counter's 64-bit atomic compiles, and nothing links libatomic.

**QEMU.** QEMU 8.2 models both cores. `qemu.sh` runs a test on QEMU's
empty machine (`-M none`), with the core and 64 MB of RAM from address
0, and no device: the generic loader puts the ELF into the RAM and the
core at its entry, since that machine takes no `-kernel`. The tests need
nothing but the core, its memory and semihosting. Before the tests, a probe checks that QEMU carries
a program's output and its exit status, and prints the core's macros as
GCC has them (architecture, profile, floating point, Advanced SIMD,
LDREXD), the type sizes, whether `char` is signed, the mode the tests run
in, its system control register's M and A bits (the MPU and the alignment
check), and an unaligned load's result. The script checks that the
binaries are 32-bit little-endian Arm, of the core's architecture
(`Tag_CPU_arch` v7 or v8-R), of the real-time profile, with the
hard-float ABI, and that each test was compiled with the core's flags; it
prints each test's seconds, and keeps the output a stopped test had
written.

**The memory, and the modes.** picolibc's linker script, its memory set
inside the RAM: 16 MB of code from 0, then 16 MB of data, a 256 KB stack
among it, the stack the Cortex-M job's picolibc legs give the tests.
picolibc 1.8.6's semihosting start file (`--crt0=semihost`) enables the
floating point and sets the stack, and leaves the MPU off. The R5F runs
the tests in Supervisor mode. The R52 starts in Hyp mode, at EL2, and the
start file leaves it there: the tests run at EL2, under its own control
register, HSCTLR. With the MPU off, a core gives memory the types of its
default memory map, which QEMU 8.2 does not hold to: it faults no
unaligned access (the probe's unaligned load reads its four bytes), so
the tests run as on a core whose MPU maps memory as normal. What the job
holds is the code GCC generates for each core, run on QEMU's model of it:
not the core's timing, nor its caches, nor its memory system.

**The toolchain.** Ubuntu 24.04's packages, signed by its archive:
`gcc-arm-none-eabi` (GCC 13.2.1, Arm's 13.2.rel1), `picolibc-arm-none-eabi`
(picolibc 1.8.6) and `qemu-system-arm` (QEMU 8.2.2), installed without the
packages apt recommends, newlib among them, from the Actions cache first
(`.github/actions/apt-install`). VERIFY-113's measure found every header
the tests read picolibc's, GCC's or the library's, and each link reading
picolibc's start file, `libc.a` and `libsemihost.a`, and GCC's `libgcc.a`,
nothing else.

**The pins**, `pins.txt`: the tests that do not pass, each with the cores
and build types it holds for, what the test does there (`not-built`;
`fails N`, N the lines of its output that begin with `FAIL`; or
`times-out`, stopped at 60 s rather than 120), and why. A test the pins do
not name must pass. An outcome other than its pin's is a difference; with
`ENFORCE` (in the script) a difference fails the job, unless the run is
`--report-only`. Three tests are not built, at both cores and every build
type, as with picolibc on Cortex-M and on bare-metal AArch64: `time_test`
(picolibc declares `clock_gettime` and `nanosleep` only where
`_POSIX_TIMERS` is defined, which it is not on bare metal), and
`file_test` and `impl_defined_test` (picolibc's semihosting has no
`rename`). `option_test` fails one check at `-O2` and `-Os`, at both
cores: GCC 13.2's wrong code, which VERIFY-089 found on Cortex-M (the
README's bare-metal section).

    python3 tools/cortexr/cortexr.py run CORE [BUILD...] [--report-only]
    python3 tools/cortexr/cortexr.py check

`run` builds into `cortexr_out/`; `check` is `8 · Records'`. The steps
the script shares with `tools/msp430/msp430.py` (a test's run, and why a
test is not built) and with `tools/cortexm/cortexm.py` (CTest's list of
the tests and their `FAIL` lines) are those scripts'.

**Runs.** Enforced from VERIFY-113's run 1: its pins were measured before
it, at both cores and each build type, with the packages the job installs.
The predictions and their scores are in `docs/deviations.md`.
