# Machine models for the core/ proofs (VERIFY-060)

Frama-C ships no machine model ("machdep") for Arm or RISC-V. The models here
are generated with Frama-C 29's `make-machdep` from the cross-compilers named
in `generate.sh`, which compiles small test files with each and runs nothing
on the target. They state what the proofs at each target assume: type sizes
and alignments, the signedness of `char`, the byte order, the C library's
constants and the compiler's predefined macros. They are part of the
proofs' trusted base: a wrong size here would make a proof at that target
wrong.

| file | target | compiler and flags |
|------|--------|--------------------|
| `machdep_cortex_m4.yaml` | Cortex-M4, bare metal | `arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16` |
| `machdep_rv32_ilp32.yaml` | RV32, bare metal | `riscv64-unknown-elf-gcc --specs=picolibc.specs -march=rv32imac -mabi=ilp32` |
| `machdep_rv64_lp64.yaml` | RV64, bare metal | `riscv64-unknown-elf-gcc --specs=picolibc.specs -march=rv64imac -mabi=lp64 -mcmodel=medany` |
| `machdep_arm_linux_gnueabihf.yaml` | 32-bit Arm Linux | `arm-linux-gnueabihf-gcc` |
| `machdep_rv64_linux_gnu.yaml` | RV64 Linux | `riscv64-linux-gnu-gcc` |
| `machdep_aarch64_linux_gnu.yaml` | AArch64 Linux | `aarch64-linux-gnu-gcc` |
| `machdep_ppc32_linux_gnu.yaml` | 32-bit PowerPC Linux, big-endian | `powerpc-linux-gnu-gcc`; its macros with `-std=c11` |

The PowerPC model's predefined macros are taken in ISO C mode. In GNU mode
that compiler predefines `bool`, `vector`, `pixel` and `_Bool` as
themselves, keywords it reads only in AltiVec declarations, and Frama-C,
reading them as plain macros, cannot parse a unit that includes
`<stdbool.h>` first (VERIFY-060, phases A2 and A3). Its wrapper adds
`-std=c11` when the macros are read, and only then: in full ISO mode glibc's
headers also leave out `NSIG`, `PATH_MAX` and other POSIX constants that the
model's library fields read. The proofs also run at Frama-C's own `ppc_32`,
which states the same ABI on every fact the core units use, and at its
`msvc_x86_64` (LLP64).

The proofs at Cortex-M4 also pass `-enums gcc-short-enums`, the enum layout
of `arm-none-eabi`, which no field of a model states.

**How these files were made.** The first five were generated in CI by
`10 · Machine models` #1 (VERIFY-060, phase A) and copied from its log, which
prints each in full; the job's artifact cannot be downloaded in the session
that committed them. One field was changed: `compiler`, which read the path
of the wrapper `generate.sh` then used and now reads `gcc`. The AArch64
model was copied, unchanged, from `10 · Machine models` #2's log (phase A2),
and the PowerPC one from #3's (phase A3). A model is in Frama-C's GCC mode
when that field is `gcc`, as Frama-C's `gcc_x86_32` and `gcc_x86_64` are;
its default `x86_64`, at which most proofs run, reads `generic` (Frama-C
29), and its `ppc_32` reads `clang`. `generate.sh` names each wrapper `gcc`,
so that it generates that field itself, and `check.sh` checks that the mode
changes no unit's code. The job compares each committed model with a fresh
generation (`compare.py`), and fails if one differs (since phase C): a
model that no longer regenerates the same is regenerated, reviewed and
committed again, as these were.

`check.c` and `check.sh` read what Frama-C sees at each model, parse the ten
core/ units at each, and compare each unit's code outside and in GCC mode.
`packages.txt` lists the Ubuntu packages of the compilers and of the Linux
targets' C library headers, installed without the packages apt recommends.

The proofs at these models are `.github/workflows/proofs-machdeps.yml`,
enforced on `pins/<unit>.<class>.txt`: for each core/ unit and class of
models (ilp32, lp64, llp64), the proved line and the unproved goals, which
phase B's three runs gave alike at every model of the class.
`check_pins.py` checks that each of those goals is one the unit's x86_64 job
pins. The char probe (`.github/workflows/probes.yml`) proves arena at copies
made by `flip_char.py`: a model with only the signedness of `char`
changed.
