# Architectures

Which processor architectures Canon-C is checked on, and how strongly. There
are three kinds of evidence, from running code to proof:

- **Tested on**: the test suite is built for the architecture and run there,
  natively or under an emulator.
- **Compiled for**: the headers, and where stated the tests, are compiled
  for the architecture with the project's warning flags; nothing runs.
- **Proved at**: Frama-C/WP proves the units at a machine model of the
  architecture (its type sizes, alignments and byte order), and Eva runs the
  programs of `tools/eva/` there.

Each entry names the CI job that shows it. An entry marked *planned* is on
the project's list and shows nothing yet; it moves into its table when its
job runs. An architecture that waits only for a compiler CI can use is
planned too, and listed under "Waiting for a compiler" with what holds it
back. This file is the one place these facts are kept; the README points
here for them.

## Tested on

| architecture | system, C library | compilers | job |
|--------------|-------------------|-----------|-----|
| x86-64 | Linux, glibc | GCC, Clang | `build` (`ci.yml`), with sanitizers, Valgrind and fuzzing in their own jobs |
| x86-64 | Linux, musl (Alpine) | GCC | `musl` (`platforms.yml`) |
| x86-64 | Windows | MSVC; MinGW-w64 GCC | `build`, `mingw` (`ci.yml`) |
| x86-64 | Windows under Cygwin: its C library, newlib's, in `cygwin1.dll` | Cygwin's GCC | `cygwin` (`ci.yml`), since VERIFY-126: LP64, where Windows' other compilers are LLP64, with Windows' 16-bit `wchar_t` and x64 calling convention; no `_WIN32`, so that the tests take their POSIX branches; the baseline of CompCert's `x86_64-cygwin` |
| x86-64 | FreeBSD 15.1, its own C library | FreeBSD's Clang | `freebsd` (`ci.yml`), since VERIFY-133: FreeBSD's own VM image under QEMU and KVM on the Ubuntu runner, GitHub having no BSD runner; LP64, as Linux; the baseline of CompCert's `x86_64-bsd` |
| x86-64 | Linux, glibc | CompCert 3.17 | `compcert` (`analysis.yml`), informational: its result blocks nothing |
| 32-bit Arm, AArch64, PowerPC 32, x86 and RISC-V 64 | Linux, glibc | CompCert 3.17, built for each with Ubuntu's cross toolchain | `compcert-targets` (`analysis.yml`), the tests under qemu-user, natively at x86: 61 of 61 at each, at 32-bit Arm with `-U__REDIRECT -U__REDIRECT_NTH` (below), each target held to that (VERIFY-115); informational, as `compcert` |
| 32-bit Arm, bare metal: ARMv7-A (Cortex-A9; the hard-float ABI, and the base ABI), ARMv7-R (Cortex-R5F), ARMv7-M (Cortex-M7, double-precision FPU), ARMv6 (ARM1176) | bare metal: picolibc 1.8.6 | CompCert 3.17 (`arm-eabihf`, `arm-eabi`, `armv7r-eabihf`, `armv7m-eabihf`, `armv6-eabihf`), Ubuntu's `arm-none-eabi` GCC preprocessing, assembling and linking for it | `compcert-bare` (`analysis.yml`), on QEMU's models of the cores through semihosting: 55 of the 61 tests at each, held to that (VERIFY-117); not `lifetime_token_test`, `time_test`, `impl_defined_test` and `file_test`, for what picolibc on bare metal does not give, nor `compare_test` and `parse_test`, whose `<math.h>` CompCert does not take (below); informational, as `compcert` |
| PowerPC 32, big-endian, bare metal: the embedded ABI, with the FPU | bare metal: newlib 4.4 | CompCert 3.17 (`ppc-eabi`), the `powerpc` job's toolchain preprocessing, assembling and linking for it | `compcert-ppc` (`analysis.yml`), under psim, GDB 15.1's PowerPC simulator, each test linked as `powerpc` links its tests at `ppc` (below): 56 of 61 run, held to that (VERIFY-120): not built, `lifetime_token_test` and `time_test`, which use what newlib gives only with `_POSIX_BARRIERS` and `_POSIX_TIMERS`, and `impl_defined_test` and `file_test`, psim's system calls having no `link`, which `rename()` needs; `stringbuf_test` failing, its `%ls` on big-endian data, as at `powerpc`; newlib's `__func__` undone for ccomp (below); informational, as `compcert` |
| 32-bit Arm, big-endian (Armv7-A, hard float) | Linux, glibc 2.39 | CompCert 3.17 (`armeb-linux`), the `armeb` job's toolchain preprocessing, assembling and linking for it | `compcert-armeb` (`analysis.yml`), under qemu-user in that toolchain's sysroot, each binary checked BE8: 61 of 61 run, held to that (VERIFY-122); configure's `__REDIRECT` left as it is, this GCC not defining `_TIME_BITS=64` (below); informational, as `compcert` |
| AArch64 (Apple silicon) and x86-64 | macOS | CompCert 3.17 (`aarch64-macos`, `x86_64-macos`), built on each runner, Apple's cc preprocessing, assembling and linking for it | `compcert-macos` (`analysis.yml`), on `macos-latest` and `macos-15-intel`, the tests run natively, Apple Clang's on the same runners their baseline: 60 of 61 run at each, held to that (VERIFY-125): `lifetime_token_test` needs POSIX barriers, which macOS does not have, so that no compiler builds it there; each file given first a header of the job's own, for two things Apple's headers give a compiler without GNU C (below); informational, as `compcert` |
| x86-64 | Windows under Cygwin: newlib's C library, in `cygwin1.dll` | CompCert 3.17 (`x86_64-cygwin`), built under Cygwin on the runner, Cygwin's GCC preprocessing, assembling and linking for it | `compcert-cygwin` (`analysis.yml`), on `windows-latest`, the tests run natively, Cygwin's GCC's on the same runner their baseline (`cygwin`, `ci.yml`): 61 of 61 run, held to that (VERIFY-132); each file given first a header of the job's own, with the feature macro first, newlib's `<stdint.h>` read whole and its `__func__` undone (below); informational, as `compcert` |
| x86-64 | FreeBSD 15.1, its own C library | CompCert 3.17 (`x86_64-bsd`), built in FreeBSD on the runner, FreeBSD's cc preprocessing, assembling and linking for it | `compcert-bsd` (`analysis.yml`), since VERIFY-134, in FreeBSD's own image under QEMU and KVM on the Ubuntu runner, as `freebsd` (`ci.yml`) boots it, whose run of FreeBSD's Clang is the baseline; the tests run in FreeBSD: 61 of 61 run, held to that (VERIFY-137); each file given first a header of the job's own, FreeBSD's nullability qualifiers and lock annotations undone and `INFINITY` and `NAN` given glibc's forms (below), and a test that includes `<pthread.h>` linked with `-lpthread`; informational, as `compcert` |
| x86, 32-bit | FreeBSD 15.1, its own C library: 64-bit FreeBSD's 32-bit libraries | CompCert 3.17 (`x86_32-bsd`), built in FreeBSD on the runner, FreeBSD's cc preprocessing, assembling and linking for it at `-m32` | `compcert-bsd` (`analysis.yml`), since VERIFY-138, in the image of its `x86_64-bsd` leg, whose run of FreeBSD's Clang at `-m32`, `freebsd`'s `-m32` legs, is the baseline; the tests run in FreeBSD; no pin yet: run 1 measures; each file given first the job's header, which at i386 gives `size_t` as FreeBSD's headers do (below), and a test that includes `<pthread.h>` linked with `-lpthread`; informational, as `compcert` |
| x86, 32-bit | Linux, glibc | GCC (`-m32`) | `build-32bit` (`ci.yml`): `size_t` exactly 32 bits |
| x86, 32-bit | FreeBSD 15.1, its own C library: 64-bit FreeBSD's 32-bit libraries | FreeBSD's Clang (`-m32`) | `freebsd` (`ci.yml`), its `-m32` legs, since VERIFY-137, in the image of its x86-64 legs: FreeBSD 15 has no i386 release, and its amd64 image holds the 32-bit libraries, their development files and the 32-bit runtime linker; each test checked a 32-bit program for the i386; the baseline of CompCert's `x86_32-bsd` |
| AArch64 | Linux, glibc | GCC, Clang | `build-arm64` (`platforms.yml`), on GitHub's Arm runner, with sanitizers; `char` is unsigned there |
| AArch64 | macOS | Apple Clang | `build` and `lifetime-debug` (`ci.yml`), `macos-latest` is Apple silicon |
| x86-64 | macOS | Apple Clang | `build` and `lifetime-debug` (`ci.yml`), on `macos-15-intel`, GitHub's last Intel image, until it retires with macOS 15 in fall 2027 (VERIFY-123) |
| s390x, big-endian | Linux, glibc | GCC | `big-endian` (`platforms.yml`), under qemu-user |
| PowerPC 32, big-endian | Linux, glibc | GCC | `big-endian-32` (`platforms.yml`), under qemu-user, also with `CANON_LIFETIME=debug`; `char` is unsigned there, and the 64-bit atomic of `lifetime.h` is libatomic's |
| 32-bit Arm (Armv7-A, Thumb-2, hard float) | Linux, glibc | GCC 13.3 (Ubuntu's `arm-linux-gnueabihf`) | `armhf` (`platforms.yml`), under qemu-user, also with `CANON_LIFETIME=debug`; `char` is unsigned there and a `uint64_t` in a struct aligned to 8, and the 64-bit atomic of `lifetime.h` is lock-free (LDREXD and STREXD), no test linked with libatomic (VERIFY-112) |
| 32-bit Arm, big-endian (Armv7-A, Thumb-2, hard float; BE8) | Linux, glibc 2.39 | GCC 13.3.0, with binutils 2.42 and glibc 2.39, built from Ubuntu's source packages (`tools/armeb/build.sh`) | `armeb` (`platforms.yml`, `tools/armeb/`), under qemu-user (`qemu-armeb`) in the toolchain's sysroot, also with `CANON_LIFETIME=debug`; its data big-endian and its instructions little-endian (BE8), each binary checked; as at `armhf`, `char` unsigned, a `uint64_t` in a struct aligned to 8, and the 64-bit atomic of `lifetime.h` lock-free, no test linked with libatomic; `time_t` and `off_t` of 32 bits, glibc's default, where Ubuntu's GCC for armhf makes them 64 (VERIFY-121) |
| Cortex-M0+ (Armv6-M, soft float) | bare metal: newlib; picolibc | GCC 13.2 (Ubuntu's `arm-none-eabi`), with newlib and picolibc; GCC 15.3 (the Arm GNU Toolchain 15.3.Rel2), with its newlib | `cortex-m` (`platforms.yml`, `tools/cortexm/`), under QEMU's micro:bit, whose Cortex-M0 runs Armv6-M code, its memory enlarged; at `-O0`, `-O2`, `-O3` and `-Os`; the tests that do not pass pinned with the reason |
| Cortex-M4 (Armv7E-M, hard float) | bare metal: newlib; picolibc | the same | `cortex-m`, under QEMU's mps2-an386 |
| MSP430X, 16-bit `int` | bare metal: newlib 2.4 (TI's) | GCC 9.3.1 (TI's MSP430-GCC-OPENSOURCE 9.3.1.2, Mitto Systems' build) | `msp430` (`platforms.yml`, `tools/msp430/`), under the GDB simulator the toolchain ships; the small memory model (16-bit `size_t`) and the large (20-bit); at `-O0`, `-O2`, `-O3` and `-Os`; the tests below Tier 1 only, which refuses both `size_t`s; the tests that do not pass pinned with the reason (VERIFY-097) |
| Cortex-R5F (Armv7-R) and Cortex-R52 (Armv8-R AArch32), hard float | bare metal: picolibc 1.8.6 | GCC 13.2 (Ubuntu's `arm-none-eabi`, with picolibc's specs) | `cortex-r` (`platforms.yml`, `tools/cortexr/`), on QEMU 8.2's models of both cores, on its empty machine, the MPU off; the R5F's tests in Supervisor mode, the R52's at EL2, where it starts; at `-O0`, `-O2`, `-O3` and `-Os`; the tests that do not pass pinned with the reason, GCC 13.2's wrong code among them (VERIFY-113) |
| AArch64, bare metal: Cortex-A78AE (Armv8.2-A) and Cortex-R82 (Armv8-R) | bare metal: picolibc 1.8.6 | GCC 13.3 (Ubuntu's `aarch64-linux-gnu`, with picolibc's specs) | `aarch64` (`platforms.yml`, `tools/aarch64/`), under QEMU 8.2's virt board; QEMU models neither core, so each is compiled for with its `-mcpu` and run on QEMU's `max` CPU, which has the instructions of both, at EL1 with the MMU off; at `-O0`, `-O2`, `-O3` and `-Os`; the tests that do not pass pinned with the reason (VERIFY-099) |
| Renesas RH850 (V850E3V5), with its FPU and without | bare metal: newlib 4.4 | GCC 13.3.0, built from Ubuntu's source packages (`tools/renesas/build.sh`) | `renesas` (`platforms.yml`, `tools/renesas/`), under GDB 15.1's V850 simulator, built from the same packages; Renesas' RH850 ABI, where an 8-byte object is aligned to 4; at `-O0`, `-O2`, `-O3` and `-Os`; each test linked with the `_open` newlib's V850 port gets wrong (`open.c`); the tests that do not pass pinned with the reason (VERIFY-101) |
| Renesas RX (RX600), with its FPU, without it, and without it with big-endian data | bare metal: newlib 4.4 | GCC 13.3.0, built from Ubuntu's source packages (`tools/renesas/build.sh`), with `-m64bit-doubles` (GCC's `double` for RX has 32 bits otherwise, which `types.h` refuses) | `renesas`, under GDB 15.1's RX simulator, built from the same packages; plain `char` unsigned, an 8-byte object aligned to 4; at `-O0` (without `-g`, at which GCC 13.3's RX port stops with an internal error), `-O2`, `-O3` and `-Os`; each test linked with the MSP430 job's `tmpfile()` (`tools/msp430/tmpfile.c`), since the simulator has no `lseek`, and the big-endian ones with crt files whose arrays are data; with the FPU, GCC compiles with finite math, and the checks of NaN and infinity fail; without it, linked with the libraries built without it, which GCC's driver does not pick, each binary checked to hold none of its instructions (VERIFY-111); the tests that do not pass pinned with the reason (VERIFY-102) |
| PowerPC 32, big-endian, bare metal, with its FPU and without | bare metal: newlib 4.4 | GCC 13.3.0, built from Ubuntu's source packages (`tools/renesas/build.sh`) | `powerpc` (`platforms.yml`, `tools/renesas/`), under psim, GDB 15.1's PowerPC simulator, built from the same packages, which models a 604; plain `char` unsigned, an 8-byte object aligned to 8; at `-O0`, `-O2`, `-O3` and `-Os`; each test linked with the MSP430 job's `tmpfile()`, psim's `lseek` failing, with a start file of its own (`psim_start.S`), libgloss's defining an `__atexit` that newlib 4.4 defines too, and with the linker's script ending each section of data on a word, as psim maps a program section by section (below); the tests that do not pass pinned with the reason (VERIFY-119) |

On Cortex-M, and on Cortex-R5F and Cortex-R52, GCC 13.2 compiles one
check of `option_test` wrong at `-O2` and `-Os`, and GCC 15.3 compiles it
right on Cortex-M (the README's bare-metal section); Ubuntu's newlib has no C99 printf formats, which the Arm GNU
Toolchain's has, and which the library's own format strings do not use
since VERIFY-091.
On RX, GCC compiles with `-ffinite-math-only` wherever it uses the FPU,
its default for the RX600, so that NaN and infinity are assumed away, and
`-fno-finite-math-only` does not undo it; with `-nofpu` those checks pass
(the README's bare-metal section, and `tools/renesas/README.md`). Since
VERIFY-110 `compare.h` and `random.h` refuse finite math unless
`CANON_ALLOW_FINITE_MATH` is defined, which the `rx` variant defines.
GCC 13.3 links the libraries built for the FPU, with finite math and the
FPU's instructions, whatever the flags; since VERIFY-111 the variants
without the FPU link those built without it.
On 32-bit PowerPC under psim, three things are the simulator's
(`tools/renesas/README.md`): libgloss's start file for it defines an
`__atexit` that newlib 4.4 defines too, so that no program links; psim
maps a program section by section, each its exact size, and stops a load
that passes a section's end, which newlib's `strlen` makes when a string
ends a section within a word; and its system calls have no `link`, which
newlib's `rename()` needs, and their `lseek` fails.
With CompCert at 32-bit Arm Linux, where the compiler defines
`_TIME_BITS=64` (Ubuntu 24.04's for `arm-linux-gnueabihf` does), add
`-U__REDIRECT -U__REDIRECT_NTH` to ccomp's options. CompCert's configure
defines both macros at ARM without the alias by which glibc gives a
64-bit `time_t` its functions, so that `clock_gettime` and the other
functions of time are the 32-bit ones, and `util/time.h` reads a clock
that goes back (VERIFY-114); `compcert-targets` adds them (VERIFY-115).
Undefined, they leave glibc 2.39 naming the 64-bit functions by macro,
`#define clock_gettime __clock_gettime64`, with no prototype for them:
ccomp compiles each call as an implicit declaration, as it does at
PowerPC 32, where configure defines neither. That is right for
`clock_gettime` and `nanosleep`, the only functions of time the library
and its tests call, which return `int`; for `time`, `mktime` and
`difftime`, whose `time_t` and `double` an implicit declaration takes for
`int`, it would not be (VERIFY-119, read in glibc's `<time.h>`). At
big-endian 32-bit Arm, `armeb-linux`, the `armeb` job's GCC, as released,
does not define `_TIME_BITS=64`: `time_t` has 32 bits, glibc declares the
functions of time by their own names, and nothing is added (VERIFY-121).
The CompCert jobs put `core/primitives` on the quote path only
(`-iquote`): with `-I`, its `limits.h` stands for the system's, which
picolibc's `<stdint.h>` includes where `__GNUC__` is undefined, and two
tests did not compile at RISC-V 32 (the trap `core/primitives/limits.h`
records). The README's way, the repository's root on the path, does not
meet it.
CompCert's ARM back end does every floating-point operation on the VFP,
double precision included, and has no software floating point: it
targets Arm cores with a double-precision FPU, ARMv6 with VFPv2 and ARMv7
with VFPv3-D16 (Cortex-A, Cortex-R4F and R5F, Cortex-M7 with FPv5-D16),
and not Cortex-M0+, M3 or M4, which `cortex-m` tests with GCC. With
picolibc, whose `errno` is thread-local, CompCert, which has no
`__thread`, reaches `errno` through a function: define
`__PICOLIBC_ERRNO_FUNCTION` to one that returns `&errno`, compiled by
GCC as picolibc is, through the library's thread pointer (`-mtp=soft`).
At ARMv6, picolibc's start file enables the FPU without opening its
coprocessors (CPACR), which the core requires: open them before it.
`compcert-bare` does both (VERIFY-116). And picolibc's `<math.h>` for ARM
does not compile under CompCert: it gives CompCert `INFINITY` and `NAN` as
GCC's `__builtin_inff` and `__builtin_nanf`, which CompCert 3.17 refuses,
and its `machine/math.h` defines `sqrt` and `fabs` as GNU inline
functions of `asm`, whose VFP operands CompCert's extended `asm` does not
take. No header of the library includes `<math.h>`; two of its tests do.
With newlib, whose `<sys/cdefs.h>` defines `__func__` as `NULL` where
`__GNUC__` is not defined (its lines 410 and 411), as CompCert's
preprocessing leaves it, `contract.h` and `diag.h` are given a null
function name under CompCert, and `contract.h`'s default handler prints
it with `%s` (VERIFY-119). Give ccomp, with `-include`, a header that
includes `<sys/cdefs.h>` and then undefines `__func__`: its later
inclusions change nothing, and ccomp's own `__func__`, C99's, names the
function. `compcert-ppc` does so (VERIFY-120); picolibc excepts CompCert
from that definition. Under Cygwin, whose C library is newlib's too, the
same header would come before a file's own feature macro, which newlib,
fixing what it declares at its first header, would then not see:
`time_test` and `lifetime_token_test` define `_POSIX_C_SOURCE`, for
`clock_gettime` and the barriers. So the header defines it first, at
200809, the widest any test asks for. `compcert-cygwin` does so
(VERIFY-128). Under Cygwin, newlib's `<stdint.h>` reads
`<machine/_default_types.h>` and `<sys/_intsup.h>` before it declares
anything, and each, where `__GNUC__` is not defined, includes
`<limits.h>`. GCC's `<limits.h>` reaches the system's through its
`syslimits.h`, found beside it by `#include "syslimits.h"`; GCC resumes
an `#include_next` in a file so found at the start of the quote path
(its `libcpp/files.cc`), where the CompCert jobs' `-iquote
core/primitives` holds the library's own `limits.h`, the trap that
file's comment records. That header comes back, by `types.h`, to
`<stdint.h>`, then half read, and every test stopped at its `intmax_t`,
not yet declared (VERIFY-130). So the header reads `<sys/features.h>`
and then `<stdint.h>`, whole, with newlib's `__GNUC_PREREQ` defined as
true for it alone: newlib then takes GCC's own `__LONG_MAX__` and the
rest, which GCC defines with or without `__GNUC__`, reads no
`<limits.h>`, and declares the same types; `__GNUC_PREREQ` is then 0
again, as newlib defines it there. `compcert-cygwin` does so
(VERIFY-131), and every test then compiles and passes.
On macOS, Apple's headers give a compiler that does not define
`__GNUC__`, as CompCert's preprocessing leaves it, two things CompCert
cannot use (VERIFY-124). `NAN` is a call, `__nan()`, which libSystem
exports at x86-64 and not at Apple silicon, where a program that uses it
does not link. And `<stdio.h>` declares `va_list` as `void *`, before
CompCert's `<stdarg.h>`, which finds `_VA_LIST_T` defined and keeps it:
harmless at Apple silicon, whose `va_list` is a pointer, but at x86-64
CompCert's is three words, which its builtins reach by the address the
array decays to: given the pointer, they take its value, never set, for
that address, and a function that passes its arguments on to
`vsnprintf` stops by a signal.
Give ccomp, with `-include`, a header that includes `<stdarg.h>`, which
is CompCert's, its directory first on ccomp's path, and `<math.h>`, and
then undefines `NAN` and defines it as glibc's `<math.h>` does without
GNU C, `(0.0f / 0.0f)`. `compcert-macos` does so (VERIFY-125). It is not
a recipe for glibc, which fixes what it declares at the first system
header it reads: a feature macro that a file defines after it, as
`util/time.h` defines `_POSIX_C_SOURCE`, comes too late (here, under
GCC, `time_test` and `lifetime_token_test` do not compile so). Apple's
headers set their level at the first one too, but in full where no such
macro is defined, and the full level declares `clock_gettime`
(VERIFY-125, read in Apple's sources).
On FreeBSD, CompCert's configure has FreeBSD's cc, Clang, preprocess,
with `-U__GNUC__` and nothing else of the kind, so that `__clang__`
stays defined, and FreeBSD's `<sys/cdefs.h>` keeps for Clang two things
CompCert does not take (VERIFY-134, read in FreeBSD 15.1's sources and
emulated here). Its nullability qualifiers, `_Nonnull`, `_Nullable` and
`_Null_unspecified`, which `<stdio.h>` and `<stdlib.h>` carry, `FILE`
among them: every test reads `<stdio.h>`, and at its line 135,
`int (* _Nullable _close)(void *);`, CompCert sees a declarator named
`_Nullable` and then `_close`. And its lock annotations, `__attribute__`s
in `<pthread.h>` whose arguments name a function's parameters, which
CompCert, elaborating them, finds undeclared. FreeBSD's `<math.h>`, for
every compiler, gives `INFINITY` and `NAN` as `__builtin_inff()` and
`__builtin_nanf("")`, which CompCert 3.17 refuses as builtins it does
not know. Give ccomp, with `-include`, a header that includes
`<sys/cdefs.h>`, then defines the three qualifiers and the two pragmas
that go with them empty, as `<sys/cdefs.h>` defines them for a compiler
without nullability, and `__lock_annotate` empty, as it defines it for
one without thread-safety attributes; then includes `<math.h>` and
defines `INFINITY` and `NAN` as glibc's `<math.h>` defines them without
GNU C, `1e10000f` and `(0.0f / 0.0f)`. `<sys/cdefs.h>`, read first,
sets FreeBSD's level, in full where no feature macro is defined: a
file's own, defined after it, asks for less than it gets. And FreeBSD
keeps `pthread_create` and the barriers in libthr, not in libc: a test
that includes `<pthread.h>` is linked with `-lpthread`. `compcert-bsd`
does both (VERIFY-134), and every test then compiles and passes
(VERIFY-136). At `x86_32-bsd`, on 64-bit FreeBSD's 32-bit libraries,
one thing more: CompCert's own `<stddef.h>` makes `size_t` `unsigned
long`, where FreeBSD's headers make it `__size_t`, at i386 an `unsigned
int`, of the same size but another type, and CompCert refuses a
typedef given again with another type. So at i386 the header also gives
`size_t` as FreeBSD's headers give it, once, and defines `_SIZE_T`, by
which CompCert's `<stddef.h>` leaves it be (VERIFY-138).

*Planned:*

- AArch64 on Windows, MSVC's ARM64 compiler on GitHub's Windows Arm runner.
- PowerPC 64, big-endian, Linux, under qemu-user.
- SPARC LEON3 and LEON4 first, the cores of European spacecraft, with
  GCC on QEMU's `leon3_generic`; then MIPS (PIC32) and Xtensa (ESP32)
  under QEMU. And Cortex-M7 with GCC in `cortex-m`, on QEMU's
  `mps2-an500`, which `compcert-bare` runs already.
- TriCore (AURIX TC2xx and TC3xx) with the community port of GCC 13.3 and
  newlib (NoMore201/tricore-gcc-toolchain), built from its sources, their
  checksums pinned, the tests on QEMU's TriCore models: the author's
  decision of 2026-11-23, in place of VERIFY-105's. A vendor's compiler
  joins it when one comes with a licence that does not expire (below).
- The targets compiled only, run: RISC-V 32 and 64 on bare metal under
  QEMU, AVR under simavr, and ARC where a simulator CI can run turns up.
- WebAssembly under a runtime.
- CompCert 3.17 at `ppc-eabi-diab`, which waits for a compiler (below).

## Compiled for

| architecture | C library | compiler | what compiles | job |
|--------------|-----------|----------|---------------|-----|
| Cortex-M0+ (Armv6-M) | newlib; picolibc | GCC (`arm-none-eabi`) | every header, as is and with `CANON_NO_GNU_EXTENSIONS`, and every test, at `-O0`, `-Os` and `-O2` | `embedded` (`platforms.yml`, `tools/embedded_compile_check.sh`) |
| Cortex-M4 (Armv7E-M, hard float) | newlib; picolibc | GCC (`arm-none-eabi`) | the same | `embedded` |
| RISC-V 32 (RV32IMAC), bare metal | picolibc | GCC (`riscv64-unknown-elf`) | the same | `embedded` |
| RISC-V 64 (RV64IMAC), bare metal | picolibc | GCC (`riscv64-unknown-elf`) | the same | `embedded` |
| RISC-V 32 (RV32GC), no Linux C library in Ubuntu | picolibc's headers | CompCert 3.17 (`rv32-linux`), `riscv64-unknown-elf-gcc -march=rv32gc` preprocessing and assembling | every test, to an object, nothing linked or run: 55 of 61, held to that (VERIFY-115); not `lifetime_token_test` and `time_test`, for what picolibc does not have (`pthread.h`; `CLOCK_MONOTONIC`, without `_POSIX_TIMERS`), nor four whose picolibc headers CompCert's C does not take (a thread-local `errno`; statement expressions in RISC-V's `machine/math.h`); informational | `compcert-targets` (`analysis.yml`) |
| AVR (ATmega328P), 16-bit `size_t` | avr-libc | avr-gcc 7 | Tier 0 only (the README's platform tiers): the refusal of a 32-bit `double`, and with that check removed, Tier 0 compiled and Tier 1 refused | `embedded` (`tools/avr_tier_check.sh`) |
| dsPIC33 and PIC24's core (XC-DSC's virtual devices `generic-16dsp` and `generic-16bit`), 16-bit `size_t` | Microchip's | Microchip's XC-DSC 4.00, fetched from Microchip, its SHA-256 pinned | Tier 0 only, as on AVR: the refusal of XC-DSC's own 32-bit `double`, and with `-fno-short-double`, Tier 0 compiled and Tier 1 refused; of the 80 headers, 30 compile, 49 are refused by Tier 1, and `util/time.h` did not compile (VERIFY-110: the check's `-I util` hid the C library's `<time.h>`). Measured at VERIFY-107's run 1, enforced since VERIFY-108, the compiler's figures as pinned (`tools/dspic/probe.pinned`) | `dspic` (`platforms.yml`, `tools/tier_check.sh`, `tools/dspic/`) |
| ARC: ARCv2 EM (`-mcpu=em4`), ARCv2 HS (`-mcpu=hs38`), ARCompact ARC700 (`-mcpu=arc700`) | newlib 4.4 | GCC 13.3.0 (`arc-elf32`), built from Ubuntu's source packages (`tools/renesas/build.sh`) | the same as Cortex-M's; nothing runs, since neither GDB nor QEMU has an ARC simulator (VERIFY-106) | `arc` (`platforms.yml`, `tools/embedded_compile_check.sh`) |

*Planned*, each compiled as the Cortex-M entries are, with code size
measured where the list says so:

- Armv8-M (Cortex-M33, Cortex-M55). Cortex-M7 is planned tested (above).
- PowerPC e200 with VLE (NXP MPC5xxx), with NXP's GCC 4.9.4 and its VLE
  patches, built from their sources once NXP's own are found and the
  patches checked against them; compiled only, since no emulator runs
  VLE code.
- Xtensa (ESP32).
- SPARC LEON3 and LEON4.
- MIPS (PIC32).
- WebAssembly (wasm32).
- 8051 and STM8, with SDCC: Tier 0, once `f64` is optional. SDCC has no
  64-bit `double` (it reads `double` as `float`), so `types.h` refuses it
  today.
- TI C29x (C2000's newer core), if its compiler's type sizes give 8-bit
  bytes; not yet checked.
- dsPIC and PIC24, run: compiled for with XC-DSC (above, VERIFY-107);
  run if Microchip's simulator (MPLAB X's, through its command-line
  debugger) runs in CI. QEMU has no model of them. XC16, Microchip's
  compiler for PIC24's devices, is not used: an installer, and its
  optimizations above `-O1` want a key from a Microchip account.

## Proved at

| machine model | data model, byte order | what is proved | job |
|---------------|------------------------|----------------|-----|
| x86-64 (Frama-C's default) | LP64, little | all 26 units | `proofs-core.yml`, `proofs-semantics.yml`, `proofs-data.yml` |
| Cortex-M4, short enums | ILP32, little | the ten `core/` units | `proofs-machdeps.yml` |
| RISC-V 32, bare metal | ILP32, little | the ten `core/` units | `proofs-machdeps.yml` |
| RISC-V 64, bare metal | LP64, little | the ten `core/` units | `proofs-machdeps.yml` |
| 32-bit Arm, Linux | ILP32, little | the ten `core/` units | `proofs-machdeps.yml` |
| RISC-V 64, Linux | LP64, little | the ten `core/` units | `proofs-machdeps.yml` |
| AArch64, Linux | LP64, little | the ten `core/` units | `proofs-machdeps.yml` |
| PowerPC 32, Linux | ILP32, big | the ten `core/` units | `proofs-machdeps.yml` |
| PowerPC 32 (Frama-C's `ppc_32`) | ILP32, big | the ten `core/` units | `proofs-machdeps.yml` |
| x86-64, Windows (Frama-C's `msvc_x86_64`) | LLP64, little | the ten `core/` units | `proofs-machdeps.yml`, and `memory.h` and `bits.h` again in `frama-c-win64` |
| x86, 32-bit (Frama-C's `gcc_x86_32`) | ILP32, little | `core/arena.h` only | `frama-c-arena-32` (`proofs-core.yml`) |

The seven models of `tools/machdeps/` are generated from the targets'
compilers and checked against them for drift (`machdeps.yml`). At x86-64
and the nine models of `proofs-machdeps.yml`, Coq also proves `bits.h`'s
twelve goals the provers leave (`coq.yml`), and Eva runs the 26 programs of
`tools/eva/` (`eva.yml`), each result pinned.

*Planned:* proofs by data model, as the author chose on 2026-11-23. A
16-bit model, `int` and `size_t` of 16 bits as MSP430's, where `size_t`
arithmetic differs most from x86-64's: the ten `core/` units first, then
every unit; and every unit at one ILP32 model. Those proofs run weekly,
or when the layer they prove changes, not at each push. And models for
bare-metal AArch64, Cortex-R52, SPARC LEON3, TriCore and PowerPC e200,
generated from their compilers as the seven are: TriCore's from the
community GCC, e200's from NXP's.

## Waiting for a compiler

Some architectures can be checked only with a compiler that is
commercial, or free to use but downloaded behind a vendor's sign-in, which
a CI job cannot do. What holds them back is the toolchain, not Canon-C:
each is planned, and listed here with what holds it back and what would
let it in. This is the rule for every such architecture, those found so
far and those found later: one whose only C compilers are of this kind
goes in this table, not under "Not supported", which is for architectures
whose C implementation Canon-C cannot work with. When a compiler CI can
use appears, or the project has a licence and a runner for one, the entry
moves to the table its job then fills. The licence must not expire: a
trial's job would stop with it, and every later change to the library go
untested on that compiler.

A compiler is usable when CI can download it itself, its checksum pinned,
as the `msp430` job fetches TI's, `cortex-m` Arm's and `dspic` Microchip's, or build it from
sources in Ubuntu's archive, as `renesas` builds GCC for RH850 and RX,
and `powerpc` for PowerPC; a
free compiler with no emulator for its target is usable too, and its
target compiled only, as AVR is.

| architecture | its C compilers | what holds it back | what would let it in, and what it would check |
|--------------|-----------------|--------------------|-----------------------------------------------|
| NXP S12, S12X and S12Z (16-bit, MagniV) | NXP's CodeWarrior and Cosmic's, both commercial and for Windows | no free C compiler: GCC's 68HC11 and 68HC12 port, which covered HCS12, was declared obsolete in GCC 4.6 and removed; S12X and S12Z were never in GCC, and GNU binutils assembles S12Z with no compiler behind it | a licence for one of them, and a Windows runner. A 16-bit `int`; where `size_t` has 16 bits, Tier 0 only, as on AVR |
| Analog Devices SHARC+ addressing bytes (`-char-size-8`: ADSP-SC58x, ADSP-2158x) | Analog Devices' CrossCore Embedded Studio, commercial | no free C compiler: GCC has no SHARC port | a licence for it, and a runner it installs on. Its type sizes are not yet checked; the SHARC+ built for words is not supported (below) |

PowerPC e200 and TriCore left this table on 2026-11-23 (VERIFY-117).
NXP's `powerpc-eabivle-gcc` is GCC, under the GPL, and the patches it
carries for VLE are public, repackaged on GitHub: built from them, once
NXP's own source is found and they are checked against it, e200 is
planned under "Compiled for".
For TriCore the author took the community port of GCC, planned under
"Tested on"; HighTec's and TASKING's compilers join it when one comes
with a licence that does not expire.

CompCert's `ppc-eabi-diab` waits for a compiler too, though its
architecture does not: PowerPC is tested with GCC, at Linux and on bare
metal, and with CompCert's `ppc-eabi` (above). It is CompCert configured to use Wind
River's Diab tools in place of GNU's: `dcc` to preprocess and to link,
`das` to assemble, `dar` to archive (CompCert 3.17's `configure`). Diab
is Wind River's commercial compiler, which CI cannot fetch or install
without a licence. What would let it in: a licence for Diab and a runner it
installs on. What it would check beyond `ppc-eabi`: Diab's preprocessor,
its C library's headers, and its assembler and linker with CompCert's
code; CompCert's code for PowerPC EABI is the same.

## Not supported

None of Canon-C compiles on these: every header includes
`core/primitives/types.h`, which refuses them. A target where only part of
the library works is listed with its tier in the tables above. This is the
current design's limit, not a final one:
[issue #14](https://github.com/eclipse-canon-c/Canon-C/issues/14) records
what a subset for each kind of target would take (no 8-bit bytes, no
64-bit `double`, a 16-bit `size_t`), and it is revisited, as the README's
platform tiers say, "if and when a concrete user with a concrete 16-bit
target materializes".

| architecture | why |
|--------------|-----|
| TI C28x (C2000's classic core) and other processors with 16-bit bytes | the smallest addressable unit is 16 bits, so `uint8_t` cannot exist, and the C standard does not allow it to; Canon-C's `u8`, its byte views and its byte counts assume 8-bit bytes. Not supported by the current design: the semantic layer (contracts, scope, ownership, Error, Option, Result, Diag) compiles without `u8` and `i8`, the byte-based layers (arena, slice, memory, the 8-bit helpers) do not, so a subset is possible (issue #14). |
| 8051 and STM8, for now | SDCC, their free compiler, has no 64-bit `double`; planned above, Tier 0 once `f64` is optional (issue #14) |
| Renesas RL78, for now | GCC 13.3's RL78 port has a 32-bit `double` and no option to change it: `rl78.h` fixes `DOUBLE_TYPE_SIZE` at 32, and the `-m64bit-doubles` that GCC's manual documents for RL78 is "unrecognized" (VERIFY-104). So `types.h` refuses it at its `f64` check, as it refuses 8051 and STM8. The toolchain builds from Ubuntu's source packages (`tools/renesas/build.sh rl78-elf`), and GDB's RL78 simulator carries a program's output and exit status; but in a scratch copy with only the `f64` check removed, nine tests do not link (newlib's `printf` alone is 64 KB of code, and RL78's 16-bit function pointers reach no further: "relocation truncated to fit: R_RL78_DIR16U"), files do not open under the simulator, and `log_test` does not end. Tier 0 once `f64` is optional, with that work to do (issue #14) |
| Analog Devices SHARC before SHARC+ (ADSP-2106x to ADSP-2148x), and SHARC+ built with `-char-size-32` | word-addressed: `char` has 32 bits, as `short`, `int` and `long` do, each of `sizeof` 1, so `uint8_t` cannot exist, as on the C28x, nor `uint16_t`, so a subset would lose `u16` and `i16` too (issue #14). SHARC+ processors that address bytes (ADSP-SC58x and ADSP-2158x among them) do so by default, with `-char-size-8`, their `char` 8 bits: this row does not exclude them; they wait for a compiler (above). |

Sources for the compilers named here: [SDCC's
`double`](https://sourceforge.net/p/sdcc/feature-requests/351), [byte
accesses on the C28x](https://software-dl.ti.com/ccs/esd/documents/c2000_byte-accesses-with-the-c28x-cpu.html),
[TI's c29clang manual](https://software-dl.ti.com/codegen/docs/c29clang/compiler_tools_user_guide/compiler_manual/indexC29.html),
the SHARC compiler's sizes of types before SHARC+ ([its manual, Rev
1.5](https://www-cce.cldnet.analog.com/media/en/dsp-documentation/software-manuals/50_21k_cc_mn_rev_1.5.pdf),
"Data Type and Data Type Sizes") and its byte-addressing on SHARC+
([CrossCore Embedded Studio 2.9.0's
manual](https://www.analog.com/media/en/dsp-documentation/software-manuals/cces-sharccompiler-manual.pdf),
"Using Byte-Addressing"); for TriCore, [HighTec's free entry
toolchain](https://free-entry-toolchain.hightec-rt.com/), [TASKING's free
VX-toolset](https://tasking.com/node/485) and [the community GCC
port](https://nomore201.github.io/tricore-gcc-toolchain); for PowerPC
e200, [the answer on NXP's forum that its VLE compiler is
GCC](https://community.nxp.com/t5/S32-Design-Studio/S32-Design-Studio-for-Power-Architecture-GCC-Compiler-license/td-p/1414546)
and [its VLE patches for GCC 4.9.4,
repackaged](https://github.com/fpoussin/gcc-powerpc-eabivle).
