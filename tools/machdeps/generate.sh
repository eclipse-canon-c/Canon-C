#!/usr/bin/env bash
# tools/machdeps/generate.sh OUTDIR
#
# VERIFY-060, phase A: generate the Frama-C machine models ("machdeps") of the
# five targets the core/ proofs are to run at. Frama-C ships none for 32-bit
# Arm or RISC-V; since Frama-C 28 its make-machdep script derives one from a
# C compiler by compiling small test files, without running anything on the
# target (https://frama-c.com/2024/01/29/new-machdep.html).
#
# The three bare-metal targets take the embedded job's compilers and flags
# (tools/embedded_compile_check.sh), and the two Linux targets the default
# configuration of Debian's cross toolchains. make-machdep takes one
# compiler command; each target's flags go in a small wrapper, OUTDIR/<name>-gcc,
# so that the flags are not read as make-machdep's own options.
#
# Each model is written to OUTDIR/machdep_<name>.yaml and printed, so that the
# job's log carries it. Exit status: the number of targets that failed.

set -u
OUT="${1:?usage: generate.sh OUTDIR}"
mkdir -p "$OUT"

# name | compiler | flags
TARGETS=(
  "cortex_m4|arm-none-eabi-gcc|-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16"
  "rv32_ilp32|riscv64-unknown-elf-gcc|--specs=picolibc.specs -march=rv32imac -mabi=ilp32"
  "rv64_lp64|riscv64-unknown-elf-gcc|--specs=picolibc.specs -march=rv64imac -mabi=lp64 -mcmodel=medany"
  "arm_linux_gnueabihf|arm-linux-gnueabihf-gcc|"
  "rv64_linux_gnu|riscv64-linux-gnu-gcc|"
)

FAILS=0
for t in "${TARGETS[@]}"; do
  IFS='|' read -r name cc flags <<< "$t"
  wrap="$OUT/${name}-gcc"
  printf '#!/bin/sh\nexec %s %s "$@"\n' "$cc" "$flags" > "$wrap"
  chmod +x "$wrap"
  echo "=================================================================="
  echo "== ${name}: ${cc} ${flags}"
  echo "=================================================================="
  "$wrap" --version | head -1
  if frama-c-script make-machdep -v --compiler "$wrap" --check \
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
