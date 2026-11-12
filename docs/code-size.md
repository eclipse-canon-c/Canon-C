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
  and `-ffunction-sections`. The sizes are read with `nm -S`.
- A function counts for the header that defines it, as its entry in the
  debug information says (`DW_AT_decl_file`): the header itself, or its
  module's fragments (`_impl.h`, `_mangle.h`, `_decl.h`, `_defn.h`).
  Functions of the headers it includes count for those headers, and appear
  in their own rows.
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

**Build cost**, with the host's gcc: the tokens of Canon-C code the
compiler reads for each header alone, after preprocessing, which is exact
(tokens, not lines: a macro's expansion is one line however long, and the
generic modules are macros); and the time to compile it at `-O2`, which
varies from run to run on a shared machine, and is reported only.

## Reading the figures

A figure is what a header's functions take if a program calls all of them,
at one target, level and setting. A program that calls fewer pays less,
and at `-Os` and `-O2` the compiler may inline a small function into each
caller, which can cost more or less than one copy. For a program's own
cost, measure the program: its map file, or `size` on its image.

## The figures

From `tools/size/pins.txt`, which holds every figure, measured here with
the runner's toolchains. They are CI's first run (`2 · Platforms` #67,
VERIFY-070), figure for figure, but for what VERIFY-070's run 2 changed in
the measure: two of `core/memory.h`'s figures, whose function the first
run had counted for no header, and the tokens, which it counted as lines.
`util/random.h`'s row is VERIFY-072's, whose fixes gave its range
functions the full `i32` range, spans that overflow, infinite bounds, and a
draw again where rounding reached `max`. `semantics/diag.h`'s is
VERIFY-073's, whose shift of a full chain, a loop of frame copies where it
was a `memmove`, costs nothing at `-Os` and 2 to 12 bytes at `-O2`.
In bytes, at `-Os` as shipped unless the column says otherwise.

**All headers together**, every function kept:

| target | -Os, as shipped | -Os, checks off | -O2, as shipped | -O2, checks off |
|--------|----------------:|----------------:|----------------:|----------------:|
| cortex-m0plus | 44,696 | 28,706 | 75,320 | 41,986 |
| cortex-m4 | 44,588 | 29,080 | 71,208 | 40,556 |
| rv32imac | 54,684 | 33,126 | 90,034 | 48,076 |

The run-time checks are about a third of the code at `-Os`: on Cortex-M4,
15,508 of 44,588 bytes. `require()` stays on in release builds by design
(`core/primitives/contract.h`); a program whose contracts are proved can
turn it off with `CANON_NO_REQUIRE`.

**Each header** (its own functions; "tokens", the build cost):

| header | functions | Cortex-M0+ | Cortex-M4 | RV32 | Cortex-M4, checks off | Cortex-M4, -O2 | tokens |
|--------|----------:|-----------:|----------:|-----:|---------------------:|---------------:|------:|
| `algo/any_all/any_all.h` | 4 | 472 | 456 | 656 | 188 | 736 | 8,582 |
| `algo/filter/filter.h` | 2 | 408 | 412 | 584 | 180 | 736 | 10,524 |
| `algo/find/find.h` | 3 | 440 | 444 | 604 | 176 | 584 | 8,581 |
| `algo/fold/fold.h` | 2 | 320 | 332 | 446 | 114 | 372 | 4,655 |
| `algo/map/map.h` | 4 | 536 | 532 | 764 | 188 | 916 | 7,484 |
| `algo/reverse/reverse.h` | 4 | 568 | 548 | 794 | 210 | 1,500 | 10,664 |
| `algo/search/search.h` | 11 | 1,712 | 1,740 | 2,526 | 496 | 3,118 | 10,167 |
| `algo/sort/sort.h` | 8 | 1,192 | 1,204 | 1,736 | 776 | 4,924 | 11,366 |
| `algo/unique/unique.h` | 2 | 324 | 320 | 464 | 130 | 704 | 10,394 |
| `core/arena.h` | 22 | 1,142 | 1,078 | 1,444 | 428 | 1,982 | 8,984 |
| `core/memory.h` | 27 | 1,406 | 1,314 | 1,682 | 654 | 1,650 | 7,297 |
| `core/ownership.h` | 0 | 0 | 0 | 0 | 0 | 0 | 309 |
| `core/pool.h` | 21 | 1,132 | 1,010 | 1,294 | 512 | 2,800 | 10,628 |
| `core/primitives/bits.h` | 18 | 548 | 724 | 860 | 724 | 780 | 719 |
| `core/primitives/checked.h` | 30 | 1,056 | 612 | 742 | 612 | 634 | 2,110 |
| `core/primitives/compare.h` | 24 | 752 | 690 | 752 | 690 | 856 | 1,295 |
| `core/primitives/contract.h` | 2 | 192 | 196 | 246 | 196 | 212 | 309 |
| `core/primitives/lifetime.h` | 0 | 0 | 0 | 0 | 0 | 0 | 106 |
| `core/primitives/limits.h` | 0 | 0 | 0 | 0 | 0 | 0 | 90 |
| `core/primitives/ptr.h` | 21 | 992 | 948 | 1,282 | 218 | 1,204 | 4,069 |
| `core/primitives/types.h` | 0 | 0 | 0 | 0 | 0 | 0 | 90 |
| `core/region.h` | 12 | 780 | 758 | 1,082 | 162 | 802 | 10,278 |
| `core/scope.h` | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| `core/slice.h` | 22 | 692 | 782 | 704 | 612 | 958 | 1,561 |
| `data/array.h` | 20 | 684 | 680 | 878 | 342 | 878 | 6,517 |
| `data/bitset.h` | 48 | 2,218 | 2,444 | 2,738 | 1,806 | 3,328 | 12,300 |
| `data/convenience/dynstring.h` | 23 | 834 | 798 | 1,162 | 800 | 1,902 | 9,117 |
| `data/convenience/dynvec.h` | 21 | 478 | 442 | 578 | 442 | 528 | 8,929 |
| `data/convenience/smallvec.h` | 24 | 918 | 856 | 1,094 | 756 | 1,254 | 14,987 |
| `data/deque/deque.h` | 24 | 1,202 | 1,182 | 1,380 | 828 | 1,506 | 4,145 |
| `data/hashmap/hashmap.h` | 52 | 3,296 | 3,418 | 3,640 | 2,232 | 3,792 | 9,701 |
| `data/hashmap/hashmap_fmt.h` | 1 | 264 | 256 | 362 | 188 | 488 | 18,437 |
| `data/hashmap/hashmap_range.h` | 2 | 288 | 288 | 298 | 198 | 476 | 25,271 |
| `data/priority_queue.h` | 26 | 1,454 | 1,422 | 1,878 | 842 | 5,274 | 11,642 |
| `data/queue.h` | 14 | 176 | 144 | 210 | 94 | 822 | 4,469 |
| `data/range.h` | 31 | 1,164 | 1,200 | 1,378 | 858 | 1,492 | 4,285 |
| `data/stack.h` | 14 | 306 | 282 | 406 | 140 | 684 | 13,937 |
| `data/stringbuf.h` | 29 | 1,140 | 1,052 | 1,344 | 756 | 1,342 | 12,179 |
| `data/vec/vec.h` | 37 | 1,334 | 1,302 | 1,488 | 1,170 | 1,820 | 13,476 |
| `data/vec/vec_fmt.h` | 2 | 122 | 108 | 184 | 108 | 136 | 16,898 |
| `data/vec/vec_range.h` | 1 | 156 | 162 | 166 | 166 | 396 | 15,496 |
| `semantics/borrow.h` | 24 | 564 | 620 | 672 | 462 | 896 | 5,090 |
| `semantics/diag.h` | 13 | 790 | 758 | 936 | 708 | 832 | 2,020 |
| `semantics/error.h` | 4 | 286 | 306 | 376 | 306 | 302 | 365 |
| `semantics/option/option.h` | 16 | 574 | 640 | 638 | 446 | 682 | 1,099 |
| `semantics/result/result.h` | 17 | 738 | 788 | 608 | 566 | 832 | 1,466 |
| `util/file.h` | 23 | 1,786 | 1,796 | 1,988 | 1,166 | 2,648 | 13,565 |
| `util/log/log.h` | 5 | 282 | 298 | 346 | 298 | 572 | 1,878 |
| `util/log/log_macros.h` | 0 | 0 | 0 | 0 | 0 | 0 | 1,878 |
| `util/parse.h` | 56 | 2,996 | 3,090 | 3,466 | 2,312 | 3,128 | 3,926 |
| `util/random.h` | 8 | 932 | 920 | 1,354 | 656 | 1,908 | 1,185 |
| `util/str/intern.h` | 6 | 658 | 698 | 924 | 406 | 1,148 | 12,960 |
| `util/str/str.h` | 28 | 1,674 | 1,726 | 2,218 | 932 | 2,928 | 9,539 |
| `util/str/str_join.h` | 4 | 912 | 896 | 1,268 | 442 | 1,324 | 10,750 |
| `util/str/str_split.h` | 5 | 564 | 548 | 726 | 292 | 710 | 10,347 |
| `util/str/str_view.h` | 21 | 606 | 732 | 646 | 686 | 1,044 | 10,450 |
| Result<_Bool, Error>, shared | 17 | 636 | 636 | 672 | 410 | 668 | – |

Headers with 0 functions are types and macros only, or, for
`core/primitives/lifetime.h`, code under `CANON_LIFETIME_DEBUG` alone. The
twelve settings, Cortex-M0+ and RV32 at `-O2` and with checks off among
them, are in the pins.

**Build cost.** Each header alone, with what it needs, brings at most
25,271 tokens of Canon-C (`data/hashmap/hashmap_range.h`, with the hashmap,
the vectors it needs and Option), and each compiles at `-O2` in a few
hundredths of a second.

## Where the figures come from

The embedded job (`2 · Platforms`) runs `size.py run` on every change that
can move them. It prints a table for each target, level and setting, and
the build cost of each header, in its log; posts one notice per target, one
for the build and one for the comparison with the pins; and uploads every
function's size as an artifact, `code-size`: `size_functions.csv`,
`size_headers.csv`, `build_headers.csv`.

Every figure is pinned (VERIFY-070's run 2): a change that moves one is
seen, and is pinned again with the change, from that change's run
(`size.py pin`), with its reason in the commit. Since VERIFY-070's run 3,
a figure not as pinned fails the job. `size.py table` prints this page's
tables from the pins.
