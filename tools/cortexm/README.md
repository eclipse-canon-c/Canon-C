# The tests on Cortex-M under QEMU: `cortex-m`

`2 · Platforms`' job `embedded` compiles the tests for Cortex-M0+ and
Cortex-M4 (`tools/embedded_compile_check.sh`); the job `cortex-m` runs
them there (VERIFY-089). Each test of `CMakeLists.txt` is built with
`arm-none-eabi-gcc`, with newlib or picolibc, and run under
`qemu-system-arm` through semihosting, which carries its output and its
exit status to the host. A test passes when it exits with status 0.

| CPU | flags | QEMU board |
|-----|-------|------------|
| `m0plus` | `-mcpu=cortex-m0plus -mthumb -mfloat-abi=soft` | `microbit`, whose Cortex-M0 runs the same Armv6-M code; its nRF51's SRAM and flash given 4 MB each |
| `m4` | `-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16` | `mps2-an386` |

On both, enums are short and `int32_t` is `long`, which no hosted target
has. The Cortex-M0+ has no FPU, and Armv6-M faults on an unaligned
access, which QEMU models; the Cortex-M4's FPU is single precision, so
that a `double` is computed in software there too. Each is built at the
four build types of CMake: Debug (`-O0`), Release (`-O3`),
RelWithDebInfo (`-O2`) and MinSizeRel (`-Os`), with
`CANON_LIFETIME_NO_ATOMICS` defined: neither CPU has the lock-free 64-bit
atomic that `CANON_LIFETIME_DEBUG`'s counter needs otherwise (the README's
bare-metal section). The script checks that the binaries are the CPU's,
from their ELF attributes, and names the C library and whether its printf
has C99's formats.

**The toolchains.** `ubuntu`: Ubuntu 24.04's packages, GCC 13.2 with
newlib and picolibc. `arm-15.3.rel2`: the Arm GNU Toolchain 15.3.Rel2,
its GCC and its newlib, on the `PATH` (`--toolchain`); the job fetches it
from Arm and checks its SHA-256 against the one pinned in the workflow.

picolibc's semihosting start file and linker script are used as they
are. newlib is linked with rdimon, its semihosting library, and with
`startup.c` and `cortexm.ld` in place of its start file, so that the
stack comes from the vector table, as at a Cortex-M's reset.

**The pins**, `pins.txt`: the tests that do not pass, each with the
toolchains, CPUs, C libraries and build types it holds for, what the test
does there (`not-built`, or `fails N`, N the lines of its output that
begin with `FAIL`), and why. A test the pins do not name must pass. An
outcome other than its pin's is a difference; with `ENFORCE` (in the
script) a difference fails the job, unless the run is `--report-only`.

    python3 tools/cortexm/cortexm.py run CPU LIBC [BUILD...] [--toolchain T] [--report-only]
    python3 tools/cortexm/cortexm.py check

`run` builds into `cortexm_out/`; `check` is `8 · Records`'. Locally,
on Ubuntu 24.04: `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`,
`picolibc-arm-none-eabi` and `qemu-system-arm`.

**Runs.** VERIFY-089's: run 1 report-only, its pins those it measured
before it ran; run 2 enforced for Ubuntu's toolchain, with
`CANON_LIFETIME_NO_ATOMICS`, and the Arm GNU Toolchain's first measure,
report-only. The predictions and their scores are in
`docs/deviations.md`.
