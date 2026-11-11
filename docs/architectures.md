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
job runs. This file is the one place these facts are kept, and the README's
architecture badges follow it.

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
| Cortex-M0+ (Armv6-M) | newlib; picolibc | GCC (`arm-none-eabi`) | every header and test, at `-O0`, `-Os` and `-O2` | `embedded` (`platforms.yml`, `tools/embedded_compile_check.sh`) |
| Cortex-M4 (Armv7E-M, hard float) | newlib; picolibc | GCC (`arm-none-eabi`) | the same | `embedded` |
| RISC-V 32 (RV32IMAC), bare metal | picolibc | GCC (`riscv64-unknown-elf`) | the same | `embedded` |
| RISC-V 64 (RV64IMAC), bare metal | picolibc | GCC (`riscv64-unknown-elf`) | the same | `embedded` |
| AVR (ATmega328P), 16-bit `size_t` | avr-libc | avr-gcc 7 | Tier 0 only (the README's platform tiers): the refusal of a 32-bit `double`, and with that check removed, Tier 0 compiled and Tier 1 refused | `embedded` (`tools/avr_tier_check.sh`) |

*Planned*, each compiled as the Cortex-M entries are, with code size
measured where the list says so:

- Cortex-M7, and Armv8-M (Cortex-M33, Cortex-M55).
- Cortex-R5 and Cortex-R52.
- 32-bit Arm, big-endian.
- AArch64, bare metal (Cortex-A78AE, Cortex-R82).
- TriCore (Infineon AURIX).
- PowerPC e200, big-endian (NXP MPC5xxx).
- Renesas RH850, if a usable GCC exists.
- Renesas RX, big-endian.
- Xtensa (ESP32).
- SPARC LEON3 and LEON4.
- MIPS (PIC32).
- WebAssembly (wasm32).
- MSP430, 16-bit `size_t`: Tier 0, as AVR.
- 8051 and STM8, with SDCC: Tier 0, once `f64` is optional. SDCC has no
  64-bit `double` (it reads `double` as `float`), so `types.h` refuses it
  today.
- TI C29x (C2000's newer core), if its compiler's type sizes give 8-bit
  bytes; not yet checked.

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

## Not supported

| architecture | why |
|--------------|-----|
| TI C28x (C2000's classic core) and other processors with 16-bit bytes | the smallest addressable unit is 16 bits, so `uint8_t` cannot exist, and the C standard does not allow it to; Canon-C's `u8`, its byte views and its byte counts assume 8-bit bytes. Supporting them would mean redesigning the library's foundations. |
| 8051 and STM8, for now | SDCC, their free compiler, has no 64-bit `double`; planned above, Tier 0 once `f64` is optional |

Sources for the compilers named here: [SDCC's
`double`](https://sourceforge.net/p/sdcc/feature-requests/351), [byte
accesses on the C28x](https://software-dl.ti.com/ccs/esd/documents/c2000_byte-accesses-with-the-c28x-cpu.html)
and [TI's c29clang manual](https://software-dl.ti.com/codegen/docs/c29clang/compiler_tools_user_guide/compiler_manual/indexC29.html).
