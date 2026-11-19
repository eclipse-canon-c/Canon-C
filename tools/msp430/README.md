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

A `size_t` below 32 bits is Tier 1's refusal (`limits.h`, the README's
platform tiers): a test that includes a Tier 1 header is not built, and
the tests that build are those of the headers below Tier 1. Each model is
built at the four build types of CMake, Debug (`-O0`), Release (`-O3`),
RelWithDebInfo (`-O2`) and MinSizeRel (`-Os`), with
`CANON_LIFETIME_NO_ATOMICS` defined: MSP430 has no 64-bit atomic. Before
the tests, a probe checks that the simulator carries a program's output
and its exit status, and prints the model's type sizes as the target has
them; the script checks that the binaries are MSP430X at the model, from
their MSPABI attributes, and names newlib's version and whether its printf
has C99's formats.

**The toolchain.** TI's MSP430-GCC-OPENSOURCE 9.3.1.2: the Mitto Systems
GCC 9.3.1.11 with binutils 2.34, newlib 2.4.0 and GDB 9.1, its simulator
among them, for x86-64 Linux (`msp430-gcc-9.3.1.11_linux64.tar.bz2`). The
job fetches it from TI and checks its SHA-256 against the one pinned in
the workflow, which three recipes that fetch the same file pin too.

**The pins**, `pins.txt`: the tests that do not pass, each with the
models and build types it holds for, what the test does there
(`not-built`, or `fails N`, N the lines of its output that begin with
`FAIL`), and why. A test the pins do not name must pass. An outcome other
than its pin's is a difference; with `ENFORCE` (in the script) a
difference fails the job, unless the run is `--report-only`.

    python3 tools/msp430/msp430.py run MODEL [BUILD...] [--report-only]
    python3 tools/msp430/msp430.py check

`run` builds into `msp430_out/`, with the toolchain's `bin` on the
`PATH`; `check` is `8 · Records'`. The steps the script shares with
`tools/cortexm/cortexm.py` (CTest's list of the tests, a test's run, its
`FAIL` lines) are that script's.

**Runs.** VERIFY-096's run 1 is report-only: the toolchain had not run
before it, and its measure is what the pins will hold. The predictions
and their scores are in `docs/deviations.md`.
