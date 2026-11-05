#!/usr/bin/env bash
# tools/machdeps/generate.sh OUTDIR
#
# VERIFY-060, phases A to A3: generate the Frama-C machine models
# ("machdeps") of the seven targets the core/ proofs are to run at. Frama-C
# ships none for Arm or RISC-V; since Frama-C 28 its make-machdep script
# derives one from a C compiler by compiling small test files, without
# running anything on the target
# (https://frama-c.com/2024/01/29/new-machdep.html).
#
# The three bare-metal targets take the embedded job's compilers and flags
# (tools/embedded_compile_check.sh), and the four Linux targets the default
# configuration of Ubuntu's cross toolchains. make-machdep takes one
# compiler command; each target's compiler and flags go in a small wrapper
# named gcc, in a directory of its own put first on PATH. The flags are then
# not read as make-machdep's options, and the model's `compiler` field is
# "gcc", which Frama-C reads as GCC mode (phase A, VERIFY-060: a wrapper
# named otherwise gave its path there, and Frama-C's GCC mode is
# `compiler = "gcc"` exactly).
#
# The PowerPC model takes its predefined macros in ISO C mode, as the
# project's builds compile (CMAKE_C_EXTENSIONS OFF): its wrapper adds
# -std=c11 when make-machdep asks for the macros (gcc -c -dM -E -), and only
# then. In GNU mode the compiler predefines bool, vector, pixel and _Bool as
# themselves, keywords it reads only in AltiVec declarations; Frama-C reads
# them as plain macros, and bool then stops being a type in a unit that
# includes <stdbool.h> first (VERIFY-060, phase A2). The model's other
# fields stay in GNU mode, as at the other models: in full ISO mode glibc's
# headers also leave out NSIG, PATH_MAX, HOST_NAME_MAX and TTY_NAME_MAX,
# which make-machdep reads into the model's library constants.
# ppc32_linux_gnu_gnumode, the same compiler wholly in GNU mode, is generated
# in phase A3 only, uncommitted, to test that diagnosis.
#
# Each model is written to OUTDIR/machdep_<name>.yaml and printed, so that the
# job's log carries it. Exit status: the number of targets that failed.

set -u
OUT="${1:?usage: generate.sh OUTDIR}"
mkdir -p "$OUT"

# name | compiler | flags | flags added when the macros are read (-dM)
TARGETS=(
  "cortex_m4|arm-none-eabi-gcc|-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16"
  "rv32_ilp32|riscv64-unknown-elf-gcc|--specs=picolibc.specs -march=rv32imac -mabi=ilp32"
  "rv64_lp64|riscv64-unknown-elf-gcc|--specs=picolibc.specs -march=rv64imac -mabi=lp64 -mcmodel=medany"
  "arm_linux_gnueabihf|arm-linux-gnueabihf-gcc|"
  "rv64_linux_gnu|riscv64-linux-gnu-gcc|"
  "aarch64_linux_gnu|aarch64-linux-gnu-gcc|"
  "ppc32_linux_gnu|powerpc-linux-gnu-gcc||-std=c11"
  "ppc32_linux_gnu_gnumode|powerpc-linux-gnu-gcc|"
)

FAILS=0
for t in "${TARGETS[@]}"; do
  IFS='|' read -r name cc flags dmflags <<< "$t"
  bin="$OUT/bin/${name}"
  mkdir -p "$bin"
  wrap="$bin/gcc"
  {
    echo '#!/bin/sh'
    if [ -n "$dmflags" ]; then
      echo 'case " $* " in'
      printf '  *" -dM "*) exec %s %s %s "$@" ;;\n' "$cc" "$flags" "$dmflags"
      echo 'esac'
    fi
    printf 'exec %s %s "$@"\n' "$cc" "$flags"
  } > "$wrap"
  chmod +x "$wrap"
  echo "=================================================================="
  echo "== ${name}: ${cc} ${flags}${dmflags:+ (macros: ${dmflags})}"
  echo "=================================================================="
  "$wrap" --version | head -1
  [ -z "$dmflags" ] || sed 's/^/   wrapper: /' "$wrap"
  if PATH="$bin:$PATH" frama-c-script make-machdep -v --compiler gcc --check \
       -o "$OUT/machdep_${name}.yaml"; then
    echo "-- machdep_${name}.yaml:"
    cat "$OUT/machdep_${name}.yaml"
  else
    echo "FAIL: make-machdep for ${name}"
    FAILS=$((FAILS + 1))
  fi
done
echo "generate.sh: ${#TARGETS[@]} targets, ${FAILS} failed"
exit "$FAILS"
