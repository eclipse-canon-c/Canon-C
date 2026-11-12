# Code size and build cost

What each of Canon-C's headers costs: in flash, on three bare-metal
targets, and in the work it gives the compiler. Measured by
`tools/size/size.py` in `2 · Platforms`' embedded job, on every change to
the library (VERIFY-070).

## What is measured

**Code size: each header's own functions, all of them.** Canon-C is
header-only, and almost every function is `static inline`: a program pays
for what it calls, and inlining spreads that over its callers. So the
figure is an upper bound that stays stable and comparable: the size of
every function a header defines, as if a program called each one.

- Each header is compiled alone with GCC's `-fkeep-inline-functions`, which
  keeps a body of every inline function, even one it inlines everywhere,
  and `-ffunction-sections`. The sizes are read with `nm -S -l`.
- A function counts for the header that defines it, as the debug
  information says: the header itself, or its module's fragments
  (`_impl.h`, `_mangle.h`, `_decl.h`, `_defn.h`). Functions of the headers
  it includes count for those headers, and appear in their own rows.
- One type is shared: `Result<_Bool, Error>`, which eight headers define,
  each guarded so that a program gets it once (MISRA-TOOL-006). It has a
  row of its own.
- A generic module has no code until it is instantiated. It is instantiated
  for `u32` (the hashmap for a `u64` key and an `int` value), and its
  figure is every function the instantiation adds. A container built on
  another (stack on vec, queue on deque, smallvec on vec) counts what it
  adds to an instantiated base. Each `algo/` module counts its functions
  over `void *` and its typed functions for `u32`.
- The separate-compilation forms (`_decl.h`, `_defn.h`) give the same
  functions as the header-only form, and are not measured apart.
  `util/time.h` needs an operating system's clock and is not measured.

**Targets**: Cortex-M0+ and Cortex-M4 (hard float), with newlib, by
arm-none-eabi-gcc; RISC-V RV32IMAC with picolibc, by riscv64-unknown-elf-gcc.
The versions are those of the pinned `ubuntu-24.04` runner's packages.

**Levels**: `-Os` and `-O2`.

**Contract settings**: as shipped for release, `NDEBUG` (`require()` on,
`ensure()` off), and with the checks off, `CANON_NO_REQUIRE` with `NDEBUG`
(both off), the setting for code whose contracts are proved
(`core/primitives/contract.h`). The difference between the two is what the
run-time checks cost.

**Build cost**, with the host's gcc: the lines of Canon-C code the compiler
reads for each header alone, after preprocessing, which is exact; and the
time to compile it at `-O2`, which varies from run to run on a shared
machine, and is reported only.

## Reading the figures

A figure is what a header's functions take if a program calls all of them,
at one target, level and setting. A program that calls fewer pays less,
and at `-Os` and `-O2` the compiler may inline a small function into each
caller, which can cost more or less than one copy. For a program's own
cost, measure the program: its map file, or `size` on its image.

## Where the figures are

The job prints a table for each target, level and setting, and the build
cost of each header, in its log, and posts one notice per target and one
for the build. It uploads every function's size as an artifact,
`code-size`: `size_functions.csv`, `size_headers.csv`, `build_headers.csv`.

The tables on this page come from CI's first run: VERIFY-070's second run
adds them, and pins each header's size, so that a change in it is seen.
