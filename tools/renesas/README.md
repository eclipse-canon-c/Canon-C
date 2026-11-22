# The tests on Renesas RH850 and RX under GDB's simulators: `renesas`

No Ubuntu package ships a compiler for Renesas' cores, but Ubuntu ships
the sources of one. The job `renesas` of `2 · Platforms` builds a
toolchain for the variant's target, `v850-elf` for RH850 or `rx-elf` for
RX, from the release tarballs of four of Ubuntu's source packages
(`build.sh`), then builds each test of `CMakeLists.txt` with it, and runs
each under GDB's simulator of the target, `v850-elf-run` or `rx-elf-run`,
which carries the output and the exit status through newlib's system
calls (VERIFY-101, VERIFY-102). A test passes when it exits with status 0
within 300 s.

| package | release | what it gives |
|---------|---------|---------------|
| `binutils-source` | binutils 2.42 | the assembler and the linker |
| `gcc-13-source` | GCC 13.3.0 | the C compiler, with GCC's multilibs of libgcc |
| `newlib-source` | newlib 4.4.0 | the C library and its system calls |
| `gdb-source` | GDB 15.1 | the simulator |

Each package is checked against Ubuntu's signed index, as
`.github/actions/apt-install` installs it; nothing is fetched from
anywhere else. Ubuntu's own patches to the sources, for its Linux
builds, are not applied: the tarballs are the projects' releases. Each
target's toolchain is kept in the Actions cache, its key the target, the
four packages' versions and `build.sh`'s hash; a leg that misses it
builds it, in about 20 minutes on two CPUs for `v850-elf`, and 27 for
`rx-elf`, whose GCC builds newlib and libgcc for 32 multilibs.

| variant | target | flags | what it is |
|---------|--------|-------|------------|
| `rh850` | `v850-elf` | `-mv850e3v5` | RH850's V850E3V5 instruction set, Renesas' RH850 ABI (GCC's default), its FPU |
| `rh850-soft` | `v850-elf` | `-mv850e3v5 -msoft-float` | the same with no FPU: `float` and `double` in libgcc's software |
| `rx` | `rx-elf` | `-mcpu=rx600 -m64bit-doubles` | the RX600 with its FPU, GCC's default, and so with finite math (below); little-endian data |
| `rx-nofpu` | `rx-elf` | `-mcpu=rx600 -m64bit-doubles -nofpu` | the same without the FPU, and without finite math; linked with the libraries built without the FPU, which GCC's driver does not pick (below) |
| `rx-be` | `rx-elf` | `-mcpu=rx600 -m64bit-doubles -nofpu -mbig-endian-data` | `rx-nofpu` with big-endian data (the instructions are as ever) |

Under the RH850 ABI, `int`, `long`, pointers and `size_t` have 32 bits,
`double` and `long double` 64, plain `char` is signed, and an 8-byte
object is aligned to 4 bytes, not 8. At RX the same, but plain `char` is
unsigned, and `double` and `long double` have 64 bits only with
`-m64bit-doubles`: GCC's default for RX is 32, which `types.h` refuses.
`rx-be` is `rx-nofpu`, not `rx`, with big-endian data, so that the two
differ by the data's byte order alone, and keep the checks of NaN and
infinity that finite math takes away. Each variant is built at the four
build types of CMake, Debug (`-O0`), Release (`-O3`), RelWithDebInfo
(`-O2`) and MinSizeRel (`-Os`), with `CANON_LIFETIME_NO_ATOMICS`
defined: GCC's V850 port compiles no atomic of its own, and its libgcc
provides none; RX's links a 4-byte `__atomic_fetch_add`, not an 8-byte
one. Before the tests, a probe checks that the simulator carries a
program's output and its exit status, and prints the type sizes and
alignments as the target has them. The script prints newlib's version,
the multilib GCC links (and at `rx-nofpu` and `rx-be` the one linked
instead, below), and the target's macros, `__FINITE_MATH_ONLY__` among
them; it checks that the binaries are the target's, of its byte order:
at RH850, under the RH850 ABI, with the notes that ABI writes into them
(8-byte objects aligned to 4, and the FPU the variant uses, `FPU-3.0` or
none); at RX, with 64-bit doubles in their ELF flags, and with the FPU's
instructions as the variant has the FPU (below). It prints each test's
seconds.

**`open.c`, at RH850.** newlib 4.4 defines the V850's `_open` with two
fixed parameters, and calls it, from `_open_r` and so from `open()`,
`fopen()` and `tmpfile()`, through the prototype it declares for it,
`int _open(const char *, int, ...)`. GCC's V850 port passes a call
through that variadic prototype with the path in r6 and the flags and the
mode on the stack; the fixed `_open` takes its flags from r7, which at
that call still holds the path's address, and the simulator opens the
file with the address's bits for flags. So no file opens as asked: the
file probe's `fopen` for writing gives NULL at one variant, its `fopen`
for reading at the other. And `tmpnam()`, which `tmpfile()` calls, tries
name after name until one does not open, where each try created its
file: `diag_test` and `log_test` ran until stopped, and filled the
host's `/tmp` with empty files (5.7 million in one run here). `open.c`
is an `_open` with the variadic type its callers use, which passes the
path and the flags to the simulator's `open` as newlib's does. The
script compiles it for the variant and links it into each test, where it
is taken before `libc.a`'s; nothing outside this job links it. Before
the tests, a file probe writes a file, reads it back and removes it,
built with newlib's `_open` and with `open.c`'s, and prints what each
does; it fails nothing.

**The FPU and finite math, at RX.** GCC's RX port turns on
`-ffinite-math-only` wherever it uses the FPU, its default for the
RX600, "since the FPU instructions do not support NaNs and infinities"
(`rx.cc`; GCC's manual says `-funsafe-math-optimizations`, which
`-Q --help=optimizers` gives as off). A `-fno-finite-math-only` on the
command line does not undo it (`__FINITE_MATH_ONLY__` stays 1), and
`-nofpu` does. The FPU works on 32-bit values only (GCC's manual): with
`-m64bit-doubles`, `double` is computed in libgcc's software at either
setting, but finite math holds for all of the code. So, at `rx`, the
checks of NaN and infinity are folded away, the library's and the tests'
own: `algo_cmp_f32` and `algo_cmp_f64` order a NaN as a number, the
tests' `isnan` and `isinf` are 0, and `random_f32_range` and
`random_f64_range`, given an infinite bound, take it for a finite one
and, at `-O2`, `-O3` and `-Os`, do not return. Those tests are pinned at
`rx`, and pass at `rx-nofpu`. Since VERIFY-110, `compare.h` and
`random.h` refuse finite math with an `#error` unless
`CANON_ALLOW_FINITE_MATH` is defined; `rx` defines it, so that it goes on
testing what a user of GCC's default gets.

**The libraries, at RX.** GCC's RX port builds newlib and libgcc for 32
multilibs, 16 of them without the FPU (`no-fpu-libs`), but its driver
picks none of those 16: `t-rx`'s `MULTILIB_MATCHES` maps `-mnofpu`,
which `-nofpu` stands for, and `-mcpu=rx200` and `-mcpu=rx100`, to
`nofpu`, which is not one of the multilib options, and the driver takes
an option's first match only. So a program GCC 13.3 links with `-nofpu`
is given the libraries built for the FPU, compiled with finite math, and
their instructions of the FPU: 81 in `libgcc.a`, 12 in `libc.a` and 1987
in `libm.a`, at either byte order (VERIFY-111). newlib's `printf` from
those libraries formats an infinity as `I.nfinitye+9998` and a NaN as
`N.aNe+9998` with `%g`, and much the same with `%e`, and with `%f` writes
past its buffer and stops the program ("Read from unwritten memory at
0x30303038"), with the FPU or `-nofpu`; with the `no-fpu-libs`
libraries, it prints `inf` and `nan`. Since VERIFY-111, as the author
chose, `rx-nofpu` and `rx-be` link those: the script copies every file of
the multilib folder without the FPU, libgcc's and newlib's, into a
folder of its own under the name of the multilib GCC picks, which `-B`
puts before GCC's folders (after the crt files' at `rx-be`), so that the
driver takes the start files and the libraries from there. It prints the
multilib linked, and checks it: a program linked as the tests are must
take every start file and library from that folder (ld's trace); those,
and the folder's other files, must hold no instruction of the FPU; and
GCC's own files of the same names must hold some, or the count would see
nothing. In every test's binary it counts the FPU's instructions too,
with `objdump`, the sections that are written left out (`.data` is code
at `rx` and `rx-nofpu`, below): none at `rx-nofpu` and `rx-be`, and some
at `rx`, for the same reason. `mvtc` and `mvfc`, which
move a control register, are not the FPU's; libgloss's `crt0.S` writes
the FPSW, the FPU's status word, with `mvtc` at every multilib,
`no-fpu-libs` too, and the script prints how many such moves the
binaries hold. No test formats an infinity or a NaN, nor does the
library: its one `%f`, `hashmap_fmt.h`'s load factor, is finite; and the
tests' outcomes, with either set of libraries, are the pins'. Linked as
GCC 13.3 links it, a program for a core without the FPU (RX200, RX100,
which this job does not run) is given the FPU's instructions wherever it
calls a library function that holds them. No test does: with GCC's own
libraries, none of the tests' binaries holds one, which is why the
libraries are checked, and not the binaries alone (VERIFY-111).

**`tmpfile.c`, at RX.** GDB's RX simulator carries eight system calls:
`exit`, `open`, `close`, `read`, `write`, `getpid` (which returns 42),
`gettimeofday` and `kill`. Any other, `lseek`, `link` and `unlink` among
them, returns with its first argument where its result would be, which
newlib takes for success: `remove()` and `rename()` return 0 and do
nothing, and `fseek()` returns 0 and does not move. newlib's `tmpfile()`
so gives a stream that does not rewind, and leaves its file, `t2a.N`, in
the host's `/tmp`. Each RX test is linked, as the author chose, through
`-Wl,--wrap=tmpfile`, with the MSP430 job's `tools/msp430/tmpfile.c`: a
`tmpfile()` that is a stream in memory (newlib's `fmemopen()`), which
`diag_test` and `log_test` write, rewind and read back. The file probe,
at RX, also writes, rewinds and reads back a `tmpfile()`, built with
newlib's and with `tmpfile.c`'s. `file_test` and `impl_defined_test`,
which remove, rename and size named files, fail where the simulator does
not carry them, pinned.

**The crt files, at `rx-be`.** GCC's RX port declares `.init_array` and
`.fini_array` executable (`"awx"`, `rx.h`'s
`INIT_ARRAY_SECTION_ASM_OP`), so `crtbegin.o` and `crtend.o` carry them
as code; the simulator's linker script, `rx-sim.ld`, puts them in
`.data`, which so becomes code; and binutils stores the code of a
big-endian RX executable with its 32-bit words swapped, data and all,
which the simulator loads as the file holds it. Linked as GCC links it,
no big-endian program runs: `nm` gives the data's symbols as code
(newlib's `__impure_ptr` as `T`, not `D`), and the probe stops with
"Read from unwritten memory at 0xdc90000c". As the author chose, the
script copies the two files with their arrays marked as data
(`objcopy --set-section-flags`), into the variant's multilib folder
under a folder that `-B` puts before GCC's own; since VERIFY-111, made
from `no-fpu-libs`' two, and their folder before that of those
libraries. At `rx` and `rx-nofpu`, little-endian, the arrays are code as
GCC marks them, and the programs run: `.data` is code there, which
`objdump` would disassemble.

**Debug, at RX.** GCC 13.3's RX port stops with an internal compiler
error at `-O0 -g` on nearly every file, `int f(int a) { return a; }`
among them: "in dwarf2out_frame_debug_adjust_cfa, at dwarf2cfi.cc:1339",
where the frame pointer is kept. With `-fomit-frame-pointer` added, or
at `-O0` without `-g`, it compiles the tests, as it does at `-O2 -g`
(RelWithDebInfo). It looks like GCC's PR target/89828, the same
assertion with `-fno-omit-frame-pointer`, reported in 2019. So the RX
variants' Debug is `-O0` without `-g`, as the author chose: the script
sets CMake's `CMAKE_C_FLAGS_DEBUG` to `-O0`, and checks that the tests
are compiled with it and without `-g`.

**The pins**, `pins.txt`: the tests that do not pass, each with the
variants and build types it holds for, what the test does there
(`not-built`; `fails N`, N the lines of its output that begin with
`FAIL`; or `times-out`, stopped at 60 s rather than 300), and why. A test
the pins do not name must pass. An outcome other than its pin's is a
difference; with `ENFORCE` (in the script) a difference fails the job,
unless the run is `--report-only`. At every variant and build type,
`time_test` is not built: its `clock_gettime` and `nanosleep` newlib
declares only where `_POSIX_TIMERS` is defined, which it is not here, as
on Cortex-M and MSP430. At `rx`, finite math's: `compare_test`,
`parse_test`, `impl_defined_test`'s NaN lines, and `random_test`, which
runs until stopped (at `-O0` it ends, its two NaN checks failed). At the
three RX variants, the simulator's: `file_test`, and `impl_defined_test`'s
renames. At `rx-be`, `stringbuf_test`'s `%ls`: newlib, built without
multibyte support (`_MB_CAPABLE`, its default), reads the wide string of
`%ls` as a string of `char`, whose first byte, with big-endian data, is
0, so the conversion succeeds and appends nothing, where the test takes a
success to have appended a byte at least; with little-endian data the
first byte is the character's low byte, and the test passes.

    sh tools/renesas/build.sh TARGET PREFIX [SRC]
    python3 tools/renesas/renesas.py run VARIANT [BUILD...] [--report-only]
    python3 tools/renesas/renesas.py check
    python3 tools/renesas/renesas.py target VARIANT

`build.sh` builds in the current directory and installs into PREFIX;
`run` needs `PREFIX/bin` on the `PATH`, and builds into `renesas_out/`;
`check` is `8 · Records'`; `target` prints a variant's target, which the
workflow builds. The steps the script shares with
`tools/msp430/msp430.py` (a test's run, why a test is not built, and
`tmpfile.c`) and `tools/cortexm/cortexm.py` (CTest's list of the tests
and their `FAIL` lines) are those scripts'.

**Runs.** RH850 is enforced from VERIFY-101's run 1: its pins were
measured before it, at both variants and each build type, with the
toolchain `build.sh` builds. The same run found `memory_test` taking a
static `u64` array to be aligned to 8, which the RH850 ABI does not
give; the test was fixed, as the author chose. RX is enforced from
VERIFY-102's run 1, its pins measured before it in the same way, with
`build.sh` unchanged. `rx-nofpu` and `rx-be` link `no-fpu-libs` from
VERIFY-111, the pins unchanged: measured with them before its run 1,
the outcomes were the pins'. The predictions and their scores are in
`docs/deviations.md`.

**ARC.** The job `arc`, in the same workflow, builds `arc-elf32` with
`build.sh`, unchanged, and only compiles with it (VERIFY-106): GDB has no
ARC simulator, so the simulator's step builds none and installs nothing,
and `tools/embedded_compile_check.sh` compiles the headers and the tests,
as it does for RISC-V. Its toolchain has an entry of its own in the
Actions cache.
