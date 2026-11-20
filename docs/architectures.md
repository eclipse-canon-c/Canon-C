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
| x86-64 | Linux, glibc | CompCert 3.17 | `compcert` (`analysis.yml`), informational: its result blocks nothing |
| x86, 32-bit | Linux, glibc | GCC (`-m32`) | `build-32bit` (`ci.yml`): `size_t` exactly 32 bits |
| AArch64 | Linux, glibc | GCC, Clang | `build-arm64` (`platforms.yml`), on GitHub's Arm runner, with sanitizers; `char` is unsigned there |
| AArch64 | macOS | Apple Clang | `build` (`ci.yml`), `macos-latest` is Apple silicon |
| s390x, big-endian | Linux, glibc | GCC | `big-endian` (`platforms.yml`), under qemu-user |
| PowerPC 32, big-endian | Linux, glibc | GCC | `big-endian-32` (`platforms.yml`), under qemu-user, also with `CANON_LIFETIME=debug`; `char` is unsigned there, and the 64-bit atomic of `lifetime.h` is libatomic's |
| Cortex-M0+ (Armv6-M, soft float) | bare metal: newlib; picolibc | GCC 13.2 (Ubuntu's `arm-none-eabi`), with newlib and picolibc; GCC 15.3 (the Arm GNU Toolchain 15.3.Rel2), with its newlib | `cortex-m` (`platforms.yml`, `tools/cortexm/`), under QEMU's micro:bit, whose Cortex-M0 runs Armv6-M code, its memory enlarged; at `-O0`, `-O2`, `-O3` and `-Os`; the tests that do not pass pinned with the reason |
| Cortex-M4 (Armv7E-M, hard float) | bare metal: newlib; picolibc | the same | `cortex-m`, under QEMU's mps2-an386 |
| MSP430X, 16-bit `int` | bare metal: newlib 2.4 (TI's) | GCC 9.3.1 (TI's MSP430-GCC-OPENSOURCE 9.3.1.2, Mitto Systems' build) | `msp430` (`platforms.yml`, `tools/msp430/`), under the GDB simulator the toolchain ships; the small memory model (16-bit `size_t`) and the large (20-bit); at `-O0`, `-O2`, `-O3` and `-Os`; the tests below Tier 1 only, which refuses both `size_t`s; the tests that do not pass pinned with the reason (VERIFY-097) |
| AArch64, bare metal: Cortex-A78AE (Armv8.2-A) and Cortex-R82 (Armv8-R) | bare metal: picolibc 1.8.6 | GCC 13.3 (Ubuntu's `aarch64-linux-gnu`, with picolibc's specs) | `aarch64` (`platforms.yml`, `tools/aarch64/`), under QEMU 8.2's virt board; QEMU models neither core, so each is compiled for with its `-mcpu` and run on QEMU's `max` CPU, which has the instructions of both, at EL1 with the MMU off; at `-O0`, `-O2`, `-O3` and `-Os`; the tests that do not pass pinned with the reason (VERIFY-099) |
| Renesas RH850 (V850E3V5), with its FPU and without | bare metal: newlib 4.4 | GCC 13.3.0, built from Ubuntu's source packages (`tools/renesas/build.sh`) | `renesas` (`platforms.yml`, `tools/renesas/`), under GDB 15.1's V850 simulator, built from the same packages; Renesas' RH850 ABI, where an 8-byte object is aligned to 4; at `-O0`, `-O2`, `-O3` and `-Os`; each test linked with the `_open` newlib's V850 port gets wrong (`open.c`); the tests that do not pass pinned with the reason (VERIFY-101) |

On Cortex-M, GCC 13.2 compiles one check of `option_test` wrong at `-O2`
and `-Os`, and GCC 15.3 compiles it right (the README's bare-metal
section); Ubuntu's newlib has no C99 printf formats, which the Arm GNU
Toolchain's has, and which the library's own format strings do not use
since VERIFY-091.

*Planned:*

- AArch64 on Windows, MSVC's ARM64 compiler on GitHub's Windows Arm runner.
- CompCert at 32-bit Arm, AArch64, RISC-V 32 and 64, PowerPC 32 and x86
  32-bit, Linux, under qemu-user; compiled only where the tests cannot run.
- PowerPC 64, big-endian, Linux, under qemu-user.
- SPARC LEON3 and MIPS under qemu, and WebAssembly under a runtime, where
  the emulators work in CI.

## Compiled for

| architecture | C library | compiler | what compiles | job |
|--------------|-----------|----------|---------------|-----|
| Cortex-M0+ (Armv6-M) | newlib; picolibc | GCC (`arm-none-eabi`) | every header, as is and with `CANON_NO_GNU_EXTENSIONS`, and every test, at `-O0`, `-Os` and `-O2` | `embedded` (`platforms.yml`, `tools/embedded_compile_check.sh`) |
| Cortex-M4 (Armv7E-M, hard float) | newlib; picolibc | GCC (`arm-none-eabi`) | the same | `embedded` |
| RISC-V 32 (RV32IMAC), bare metal | picolibc | GCC (`riscv64-unknown-elf`) | the same | `embedded` |
| RISC-V 64 (RV64IMAC), bare metal | picolibc | GCC (`riscv64-unknown-elf`) | the same | `embedded` |
| AVR (ATmega328P), 16-bit `size_t` | avr-libc | avr-gcc 7 | Tier 0 only (the README's platform tiers): the refusal of a 32-bit `double`, and with that check removed, Tier 0 compiled and Tier 1 refused | `embedded` (`tools/avr_tier_check.sh`) |

*Planned*, each compiled as the Cortex-M entries are, with code size
measured where the list says so:

- Cortex-M7, and Armv8-M (Cortex-M33, Cortex-M55).
- Cortex-R5 and Cortex-R52.
- 32-bit Arm, big-endian.
- TriCore (Infineon AURIX).
- PowerPC e200, big-endian (NXP MPC5xxx): waiting for a compiler (below).
- Renesas RX, big-endian.
- Xtensa (ESP32).
- SPARC LEON3 and LEON4.
- MIPS (PIC32).
- WebAssembly (wasm32).
- 8051 and STM8, with SDCC: Tier 0, once `f64` is optional. SDCC has no
  64-bit `double` (it reads `double` as `float`), so `types.h` refuses it
  today.
- TI C29x (C2000's newer core), if its compiler's type sizes give 8-bit
  bytes; not yet checked.
- dsPIC and PIC24, with Microchip's XC16, a port of GCC, free with its
  optimization limited (`-O0`, `-O1`): `int`, `size_t` and pointers of
  16 bits, so Tier 0 only, with `-fno-short-double`, without which
  `double` has 32 bits and `types.h` refuses it. Compiled first, as AVR
  is; run later, if Microchip's simulator (MPLAB X's, through its
  command-line debugger) runs in CI. QEMU has no model of them.

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

*Planned:* models for bare-metal AArch64, Cortex-R52, SPARC LEON3, TriCore
and PowerPC e200, generated from their compilers as the seven are.

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
moves to the table its job then fills.

A compiler is usable when CI can download it itself, its checksum pinned,
as the `msp430` job fetches TI's and `cortex-m` Arm's, or build it from
sources in Ubuntu's archive, as `renesas` builds GCC for RH850; a free
compiler with no emulator for its target is usable too, and its target
compiled only, as AVR is.

| architecture | its C compilers | what holds it back | what would let it in, and what it would check |
|--------------|-----------------|--------------------|-----------------------------------------------|
| NXP S12, S12X and S12Z (16-bit, MagniV) | NXP's CodeWarrior and Cosmic's, both commercial and for Windows | no free C compiler: GCC's 68HC11 and 68HC12 port, which covered HCS12, was declared obsolete in GCC 4.6 and removed; S12X and S12Z were never in GCC, and GNU binutils assembles S12Z with no compiler behind it | a licence for one of them, and a Windows runner. A 16-bit `int`; where `size_t` has 16 bits, Tier 0 only, as on AVR |
| PowerPC e200 with VLE (NXP MPC5xxx) | NXP's GCC 4.9.4 with its VLE patches (`powerpc-eabivle-gcc`, in S32 Design Studio for Power Architecture), free to use | GCC does not generate VLE code (binutils assembles it), and NXP's compiler is downloaded behind its sign-in. QEMU has no e200 machine: nothing would run, it would be compiled only | a copy of NXP's compiler CI can fetch, or a public release. Big-endian 32-bit PowerPC's data model is tested already (`big-endian-32`); e200 would add VLE code, and the machine model planned under "Proved at" |
| Analog Devices SHARC+ addressing bytes (`-char-size-8`: ADSP-SC58x, ADSP-2158x) | Analog Devices' CrossCore Embedded Studio, commercial | no free C compiler: GCC has no SHARC port | a licence for it, and a runner it installs on. Its type sizes are not yet checked; the SHARC+ built for words is not supported (below) |

## Not supported

| architecture | why |
|--------------|-----|
| TI C28x (C2000's classic core) and other processors with 16-bit bytes | the smallest addressable unit is 16 bits, so `uint8_t` cannot exist, and the C standard does not allow it to; Canon-C's `u8`, its byte views and its byte counts assume 8-bit bytes. Supporting them would mean redesigning the library's foundations. |
| 8051 and STM8, for now | SDCC, their free compiler, has no 64-bit `double`; planned above, Tier 0 once `f64` is optional |
| Analog Devices SHARC before SHARC+ (ADSP-2106x to ADSP-2148x), and SHARC+ built with `-char-size-32` | word-addressed: `char` has 32 bits, as `short`, `int` and `long` do, each of `sizeof` 1, so `uint8_t` cannot exist, as on the C28x. SHARC+ processors that address bytes (ADSP-SC58x and ADSP-2158x among them) do so by default, with `-char-size-8`, their `char` 8 bits: this row does not exclude them; they wait for a compiler (above). |

Sources for the compilers named here: [SDCC's
`double`](https://sourceforge.net/p/sdcc/feature-requests/351), [byte
accesses on the C28x](https://software-dl.ti.com/ccs/esd/documents/c2000_byte-accesses-with-the-c28x-cpu.html),
[TI's c29clang manual](https://software-dl.ti.com/codegen/docs/c29clang/compiler_tools_user_guide/compiler_manual/indexC29.html),
the SHARC compiler's sizes of types before SHARC+ ([its manual, Rev
1.5](https://www-cce.cldnet.analog.com/media/en/dsp-documentation/software-manuals/50_21k_cc_mn_rev_1.5.pdf),
"Data Type and Data Type Sizes") and its byte-addressing on SHARC+
([CrossCore Embedded Studio 2.9.0's
manual](https://www.analog.com/media/en/dsp-documentation/software-manuals/cces-sharccompiler-manual.pdf),
"Using Byte-Addressing").
