# The tests on MSP430X under the GDB simulator: `msp430`

MSP430's `int` has 16 bits, which no other target that runs the tests
has; its `size_t` has 16 bits in the small memory model and 20 in the
large one. `2 · Platforms`' job `msp430` builds each test of
`CMakeLists.txt` with TI's `msp430-elf-gcc` for the MSP430X CPU and runs
it under `msp430-elf-run`, the GDB simulator the toolchain ships, through
newlib's simulator runtime (`-msim`), whose system calls carry the output
and the exit status to the host (VERIFY-096). A test passes when it exits
with status 0.

| model | flags | `size_t`, pointers | the simulator's linker script |
|-------|-------|--------------------|-------------------------------|
| `small` | `-mcpu=msp430x -msmall` | 16 bits | `msp430-sim.ld`: code, data and stack below 64 KB |
| `large` | `-mcpu=msp430x -mlarge -mcode-region=either -mdata-region=either` | 20 bits (`__int20`, in 4 bytes) | `msp430xl-sim.ld`: code and data above 64 KB too |

The run prints the memory regions of the model's linker script, as the
toolchain has it.

A `size_t` below 32 bits is Tier 1's refusal (`sizes.h`, the README's
platform tiers): a test that includes a Tier 1 header is not built, and
the tests that build are those of the headers below Tier 1. Each model is
built at the four build types of CMake, Debug (`-O0`), Release (`-O3`),
RelWithDebInfo (`-O2`) and MinSizeRel (`-Os`), with
`CANON_LIFETIME_NO_ATOMICS` defined: MSP430 has no 64-bit atomic. A test
passes when it exits with status 0 within 300 s. Before the tests, a probe
checks that the simulator carries a program's output and its exit status,
and prints the model's type sizes as the target has them; a file probe
writes a line to a named file, to `tmpnam()`'s name and to a `tmpfile()`,
and reads each back, every step on `stderr`, to show how far a file's I/O
goes under the simulator, failing nothing. There newlib's `tmpfile()` does
not return (VERIFY-097), so each test is linked, through
`-Wl,--wrap=tmpfile`, with `tmpfile.c`: a `tmpfile()` that is a stream in
memory, newlib's `fmemopen()`, which the tests write, rewind and read back
as they would a file; the memory probe checks it before the tests
(VERIFY-098). Outside this job, only the `renesas` job's RX variants link
it, where GDB's RX simulator has no `lseek` (VERIFY-102).
The script checks that the binaries are MSP430X at the model, from their
MSPABI attributes, names newlib's version and whether its printf has C99's
formats and `long long`'s (2.4.0 there has neither), prints each test's
seconds, and keeps the output a stopped test had written.

**The toolchain.** TI's MSP430-GCC-OPENSOURCE 9.3.1.2: the Mitto Systems
GCC 9.3.1.11 with binutils 2.34, newlib 2.4.0 and GDB 9.1, its simulator
among them, for x86-64 Linux (`msp430-gcc-9.3.1.11_linux64.tar.bz2`). The
job fetches it from TI and checks its SHA-256 against the one pinned in
the workflow, which three recipes that fetch the same file pin too.

**The pins**, `pins.txt`: the tests that do not pass, each with the
models and build types it holds for, what the test does there
(`not-built`; `fails N`, N the lines of its output that begin with
`FAIL`; or `times-out`, stopped at 60 s rather than 300), and why. A test
the pins do not name must pass. An outcome other than its pin's is a
difference; with `ENFORCE` (in the script) a difference fails the job,
unless the run is `--report-only`.

    python3 tools/msp430/msp430.py run MODEL [BUILD...] [--report-only]
    python3 tools/msp430/msp430.py check

`run` builds into `msp430_out/`, with the toolchain's `bin` on the
`PATH`; `check` is `8 · Records'`. The steps the script shares with
`tools/cortexm/cortexm.py` (CTest's list of the tests and their `FAIL`
lines) are that script's.

**Runs.** VERIFY-096's run 1 was report-only, the toolchain not run
before it. Its measure is the pins (VERIFY-097), enforced since: the 42
tests Tier 1 refuses, and `time_test` (newlib declares no `clock_gettime`
under `-std=c99`). Three tests whose own arithmetic failed at a 16-bit
`int` were fixed instead, as the author chose. `diag_test` and
`log_test`, which timed out inside newlib's `tmpfile()`, VERIFY-097's
probe found, run with `tmpfile.c`'s since VERIFY-098. The predictions and
their scores are in `docs/deviations.md`.
