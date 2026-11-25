# Canon-C at a glance

What is checked, on which targets, and how strongly, on one page. Every
line here sums up a fact kept elsewhere:
[`architectures.md`](architectures.md) for the targets,
[`verification.md`](verification.md) for the proofs and the other checks,
[`deviations.md`](deviations.md) for each change's record. Where this page
and those files differ, they are right and this page is wrong; please say
so in an issue. A record that changes a fact stated here updates this
page with it.

*Written for VERIFY-118, 2026-11-24; brought up to date by VERIFY-119 to VERIFY-124.*

## The words used

- **Runs**: the test suite is built for the target and run on it, natively
  or on an emulator, by CI.
- **Builds**: the code compiles for the target; nothing runs there.
- **Proved**: Frama-C/WP proves that the library's functions meet their
  contracts and have no run-time error, with the target's type sizes.
- **Pinned**: a test that does not pass on a target is named, with the
  reason, and any other outcome fails the job. No failure is let through
  silently.
- **Informational**: the job's result blocks nothing; CompCert's jobs are
  so, by the terms of the project's permission to use CompCert
  ([`compcert-licensing.md`](compcert-licensing.md)).

## Targets

With GCC, Clang, MSVC or a vendor's compiler:

| family | target | compilers | the tests | proved at its type sizes |
|--------|--------|-----------|-----------|--------------------------|
| x86-64 | Linux (glibc), Linux (musl), Windows, macOS (Intel) | GCC, Clang, MSVC, MinGW, Apple Clang | run, all pass; also with sanitizers, Valgrind and fuzzing | every unit; at Windows' type sizes, `core/` |
| x86 | Linux, 32-bit | GCC | run, all pass | `core/arena.h` |
| AArch64 | Linux; macOS (Apple silicon) | GCC, Clang; Apple Clang | run, all pass | `core/` (Linux) |
| AArch64, bare metal | Cortex-A78AE, Cortex-R82 | GCC, picolibc | run, exceptions pinned | planned |
| 32-bit Arm | Linux (Armv7-A) | GCC | run, all pass | `core/` |
| 32-bit Arm, big-endian | Linux (Armv7-A, BE8) | GCC 13.3, glibc 2.39, built from Ubuntu's source packages | run, all pass | `core/` |
| Arm Cortex-R, bare metal | Cortex-R5F, Cortex-R52 | GCC, picolibc | run, exceptions pinned | planned (R52) |
| Arm Cortex-M, bare metal | Cortex-M0+, Cortex-M4 | GCC 13.2 and 15.3, newlib, picolibc | run, exceptions pinned | `core/` (Cortex-M4) |
| PowerPC 32, big-endian | Linux | GCC | run, all pass | `core/` |
| PowerPC 32, big-endian, bare metal | with its FPU and without | GCC 13.3, newlib | run, exceptions pinned | — |
| s390x, big-endian | Linux | GCC | run, all pass | — |
| RISC-V 32 and 64, bare metal | — | GCC, picolibc | build | `core/` |
| MSP430X, 16-bit `int` | bare metal | TI's GCC, newlib | run where a 16-bit `size_t` allows (below Tier 1), every one that builds passes | planned (a 16-bit model) |
| Renesas RH850 | bare metal, with and without its FPU | GCC 13.3, newlib | run, exceptions pinned | — |
| Renesas RX | bare metal, with its FPU, without it, big-endian data | GCC 13.3, newlib | run, exceptions pinned | — |
| AVR | bare metal, 16-bit `size_t` | avr-gcc | build (Tier 0) | — |
| dsPIC33, PIC24 | bare metal, 16-bit `size_t` | Microchip's XC-DSC | build (Tier 0) | — |
| ARC | bare metal: EM, HS, ARC700 | GCC 13.3, newlib | build | — |

With CompCert 3.17, the formally verified compiler (informational):

| target | the tests |
|--------|-----------|
| x86-64, x86, 32-bit Arm, AArch64, PowerPC 32 and RISC-V 64 Linux | run, 61 of 61 at each (at 32-bit Arm with two options, `architectures.md` says which and why) |
| bare-metal Arm: Cortex-A9 (both floating-point ABIs), Cortex-R5F, Cortex-M7, ARM1176, with picolibc | run, 55 of 61 at each, pinned: the six use what picolibc on bare metal does not have, or its `<math.h>`, which CompCert cannot compile and which no header of the library includes |
| RISC-V 32, picolibc's headers | build, 55 of 61, pinned |
| bare-metal PowerPC 32 (`ppc-eabi`), with newlib | run, 56 of 61, pinned: the five use what newlib on bare metal or GDB's simulator does not have, or print `%ls` on big-endian data |
| big-endian 32-bit Arm Linux (`armeb-linux`), with glibc | run, 61 of 61 |
| macOS on Apple silicon and Intel (`aarch64-macos`, `x86_64-macos`) | run, 59 and 57 of 61, pinned: one test needs POSIX barriers, which macOS does not have; one does not link at Apple silicon, and three stop by a signal at Intel, under diagnosis |

In the GCC jobs on bare metal, each test runs at four levels of
optimization (`-O0`, `-O2`, `-O3`, `-Os`). Each pinned exception, and
why, is in the job's pins file or matrix, and in its record.

## The other checks

| check | how | where |
|-------|-----|-------|
| Proofs | Frama-C/WP with three provers (Alt-Ergo, Z3, CVC5), and Coq for the goals of `bits.h` they leave: over 99% of the proof obligations proved automatically, each one left named | `verification.md`, its first table |
| No run-time error, by abstract interpretation | Frama-C's Eva, 26 programs, at x86-64 and nine other machine models | `verification.md`, `using-eva.md` |
| Rules across functions | Frama-C's Aorai (the order of calls) and MetAcsl (who may call what) | `verification.md` |
| Coverage | MC/DC of the proved paths, GCC 14 | `traceability.md` |
| Guidelines | MISRA C:2012 with Cppcheck's addon, every finding pinned and argued; CERT C and CWE with clang-tidy | `misra-campaign.md`, `cert-cwe.md` |
| Memory and undefined behaviour at run time | AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer, Valgrind, fuzzing | `verification.md` |
| C standards | written to C99; tested at C11, C17 and C23 too | `verification.md` |
| The README's examples | built and run as shown | `.github/workflows/README.md` |

## How it stays honest

Every change is a record in `deviations.md`, written and committed
before CI runs it: what changes, what was measured beforehand, and what
CI is predicted to show, each prediction with what would prove it wrong.
After the run, each is scored as it fell, a miss as a miss, and the
record is closed. A target's exceptions are pinned, so that a new failure,
or a test that starts passing, is seen at once.

## What is missing

- **Automotive**: Infineon TriCore (AURIX), to be tested with the
  community port of GCC on QEMU, and NXP's PowerPC e200 with VLE, to be
  built with NXP's GCC. Planned, in `architectures.md`.
- **Aerospace**: SPARC LEON, to be tested on QEMU first among the targets
  planned next.
- **Proofs beyond `core/`** at a 32-bit model, and at a 16-bit model,
  which there is none of yet.
- **Commercial compilers** (TriCore's, Wind River's Diab, S12's, SHARC+'s):
  only with a licence that does not expire, since the library keeps
  growing and every change has to be re-checked.
- **Not supported**: targets whose bytes are not 8 bits (TI C28x, SHARC
  addressing words); and those with no 64-bit `double` (8051, STM8,
  RL78) until `f64` is optional. `architectures.md` says why.
