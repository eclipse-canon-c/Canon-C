# The tests on Renesas RH850 under GDB's simulator: `renesas`

No Ubuntu package ships a compiler for Renesas' cores, but Ubuntu ships
the sources of one. The job `renesas` of `2 · Platforms` builds a
toolchain for `v850-elf` from the release tarballs of four of Ubuntu's
source packages (`build.sh`), then builds each test of `CMakeLists.txt`
with it, and runs each under GDB's V850 simulator, `v850-elf-run`, which
carries the output and the exit status through newlib's system calls
(VERIFY-101). A test passes when it exits with status 0 within 300 s.

| package | release | what it gives |
|---------|---------|---------------|
| `binutils-source` | binutils 2.42 | the assembler and the linker |
| `gcc-13-source` | GCC 13.3.0 | the C compiler, with GCC's multilibs of libgcc |
| `newlib-source` | newlib 4.4.0 | the C library and its system calls |
| `gdb-source` | GDB 15.1 | the simulator |

Each package is checked against Ubuntu's signed index, as
`.github/actions/apt-install` installs it; nothing is fetched from
anywhere else. Ubuntu's own patches to the sources, for its Linux
builds, are not applied: the tarballs are the projects' releases. The
toolchain is kept in the Actions cache, its key the four packages'
versions and `build.sh`'s hash; a leg that misses it builds it, in about
20 minutes on two CPUs.

| variant | flags | what it is |
|---------|-------|------------|
| `rh850` | `-mv850e3v5` | RH850's V850E3V5 instruction set, Renesas' RH850 ABI (GCC's default), its FPU |
| `rh850-soft` | `-mv850e3v5 -msoft-float` | the same with no FPU: `float` and `double` in libgcc's software |

Under the RH850 ABI, `int`, `long`, pointers and `size_t` have 32 bits,
`double` and `long double` 64, plain `char` is signed, and an 8-byte
object is aligned to 4 bytes, not 8. Each variant is built at the four
build types of CMake, Debug (`-O0`), Release (`-O3`), RelWithDebInfo
(`-O2`) and MinSizeRel (`-Os`), with `CANON_LIFETIME_NO_ATOMICS` defined:
GCC's V850 port compiles no atomic of its own, and its libgcc provides
none, so `__atomic_fetch_add` of 4 or 8 bytes does not link. Before the
tests, a probe checks that the simulator carries a program's output and
its exit status, and prints the type sizes and alignments as the target
has them. The script checks that the binaries are the V850's, under the
RH850 ABI, with the notes that ABI writes into them: 8-byte objects
aligned to 4, and the FPU the variant uses (`FPU-3.0`, or none). It
prints each test's seconds.

**`open.c`.** newlib 4.4 defines the V850's `_open` with two fixed
parameters, and calls it, from `_open_r` and so from `open()`, `fopen()`
and `tmpfile()`, through the prototype it declares for it, `int
_open(const char *, int, ...)`. GCC's V850 port passes a call through
that variadic prototype with the path in r6 and the flags and the mode
on the stack; the fixed `_open` takes its flags from r7, which at that
call still holds the path's address, and the simulator opens the file
with the address's bits for flags. So no file opens as asked: the file
probe's `fopen` for writing gives NULL at one variant, its `fopen` for
reading at the other. And `tmpnam()`, which `tmpfile()` calls, tries
name after name until one does not open, where each try created its
file: `diag_test` and `log_test` ran until stopped, and filled the
host's `/tmp` with empty files (5.7 million in one run here). `open.c`
is an `_open` with the variadic type its callers use, which passes the
path and the flags to the simulator's `open` as newlib's does. The
script compiles it for the variant and links it into each test, where it
is taken before `libc.a`'s; nothing outside this job links it. Before
the tests, a file probe writes a file and reads it back, built with
newlib's `_open` and with `open.c`'s, and prints what each does; it
fails nothing.

**The pins**, `pins.txt`: the tests that do not pass, each with the
variants and build types it holds for, what the test does there
(`not-built`; `fails N`, N the lines of its output that begin with
`FAIL`; or `times-out`, stopped at 60 s rather than 300), and why. A test
the pins do not name must pass. An outcome other than its pin's is a
difference; with `ENFORCE` (in the script) a difference fails the job,
unless the run is `--report-only`. One test is not built, at both
variants and every build type: `time_test`, whose `clock_gettime` and
`nanosleep` newlib declares only where `_POSIX_TIMERS` is defined, which
it is not here, as on Cortex-M and MSP430.

    sh tools/renesas/build.sh v850-elf PREFIX [SRC]
    python3 tools/renesas/renesas.py run VARIANT [BUILD...] [--report-only]
    python3 tools/renesas/renesas.py check

`build.sh` builds in the current directory and installs into PREFIX;
`run` needs `PREFIX/bin` on the `PATH`, and builds into `renesas_out/`;
`check` is `8 · Records'`. The steps the script shares with
`tools/msp430/msp430.py` (a test's run, and why a test is not built) and
`tools/cortexm/cortexm.py` (CTest's list of the tests and their `FAIL`
lines) are those scripts'.

**Runs.** Enforced from VERIFY-101's run 1: its pins were measured before
it, at both variants and each build type, with the toolchain `build.sh`
builds. The same run found `memory_test` taking a static `u64` array to
be aligned to 8, which the RH850 ABI does not give; the test was fixed,
as the author chose. The predictions and their scores are in
`docs/deviations.md`.
