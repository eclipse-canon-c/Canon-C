#!/usr/bin/env bash
# tools/machdeps/check.sh OUTDIR [MODEL.yaml ...]
#
# VERIFY-060, phases A to A3. For each model given (machdep_<name>.yaml),
# and for Frama-C's own x86_64, gcc_x86_32, ppc_32 and msvc_x86_64:
#   1. Frama-C reads check.c at the model with -constfold, and the probe_
#      lines of the code it prints are shown: sizes, alignments, CHAR_MIN,
#      the libc typedefs, and the compiler macros its preprocessing defines.
#   2. Each of the ten core/ units is parsed and typed at the model, with its
#      proof job's preprocessing flags, without running WP; the warnings
#      Frama-C gives are counted and shown, once each, and the errors of a
#      unit that does not parse are shown.
# The Cortex-M4 model is also read with -enums gcc-short-enums, the enum
# layout of arm-none-eabi.
#   3. The mode control. A model is in Frama-C's GCC mode when its compiler
#      field is "gcc"; Frama-C's x86_64, the proofs' default, is not a GCC
#      model in Frama-C 25 (VERIFY-060). The code Frama-C prints for each
#      unit, the input WP is given, is compared at x86_64 and gcc_x86_64,
#      at the Cortex-M4 model with its compiler field "generic" and "gcc",
#      and at Frama-C's ppc_32 with its field "clang" and "gcc". The
#      printer's #include lines are left out of the comparison.
# Outputs go to OUTDIR. Run from the repository root. Exit status: the
# number of reads that failed; a difference in part 3 is reported, not
# counted.

set -u
D="${1:?usage: check.sh OUTDIR [MODEL.yaml ...]}"
shift
mkdir -p "$D"
HERE=$(cd "$(dirname "$0")" && pwd)
FAILS=0

MODELS=("x86_64|-machdep x86_64" "gcc_x86_32|-machdep gcc_x86_32"
        "ppc_32|-machdep ppc_32" "msvc_x86_64|-machdep msvc_x86_64")
for y in "$@"; do
  n=$(basename "$y" .yaml); n=${n#machdep_}
  MODELS+=("${n}|-machdep ${y}")
  if [ "$n" = cortex_m4 ]; then
    MODELS+=("cortex_m4+short_enums|-machdep ${y} -enums gcc-short-enums")
  fi
done

echo "=== 1. check.c at each model"
for m in "${MODELS[@]}"; do
  IFS='|' read -r name args <<< "$m"
  # shellcheck disable=SC2086
  if frama-c $args "$HERE/check.c" -constfold -print -print-libc -ocode "$D/check_${name}.c" \
       > "$D/check_${name}.log" 2>&1; then
    echo "== ${name}"
    grep -E "probe_[a-z0-9_]+ *=" "$D/check_${name}.c" | sed 's/^/   /'
    grep -E "^typedef [^;]*[ *](__)?(u?int32_t|u?intptr_t|ptrdiff_t|size_t|wchar_t);" \
      "$D/check_${name}.c" | sed 's/^/   /'
  else
    echo "FAIL: ${name}: Frama-C did not read check.c"
    tail -8 "$D/check_${name}.log" | sed 's/^/   /'
    FAILS=$((FAILS + 1))
  fi
done

echo "=== 2. each core/ unit parsed at each model"
BASE="-I core/primitives -I core -I ."
UNITS=(
  "core/primitives/checked.h|"
  "core/primitives/bits.h|"
  "core/primitives/compare.h|"
  "core/primitives/ptr.h|"
  "core/slice.h|"
  "core/memory.h|"
  "core/arena.h|"
  "core/pool.h|"
  "core/region.h|"
  "core/primitives/lifetime.h|-DCANON_LIFETIME_DEBUG -DCANON_LIFETIME_NO_ATOMICS"
)
for m in "${MODELS[@]}"; do
  IFS='|' read -r name args <<< "$m"
  bad=""
  logs=()
  badlogs=()
  for u in "${UNITS[@]}"; do
    IFS='|' read -r file extra <<< "$u"
    log="$D/parse_${name}_$(basename "$file" .h).log"
    logs+=("$log")
    # shellcheck disable=SC2086
    if ! frama-c $args -cpp-extra-args="${BASE} ${extra} -DCANON_NO_REQUIRE -DNDEBUG" \
         "$file" > "$log" 2>&1; then
      bad="${bad} $(basename "$file")"
      badlogs+=("$log")
      FAILS=$((FAILS + 1))
    fi
  done
  if [ -z "$bad" ]; then
    echo "== ${name}: all ${#UNITS[@]} units parsed"
  else
    echo "FAIL: ${name}: not parsed:${bad}"
    for log in "${badlogs[@]}"; do
      echo "-- ${log##*/}: the errors"
      grep -i -E -A6 "error|failure|abort" "$log" | head -24 | sed 's/^/   /'
    done
  fi
  # Each distinct warning once, with the number of units giving it.
  grep -h -i -E "warning" "${logs[@]}" | sed -E 's/^[^ ]*:[0-9]+:[ ]*//' \
    | sort | uniq -c | sort -rn | head -20 | sed 's/^/   /'
done

echo "=== 3. the mode control: each unit's code outside and in GCC mode"
PAIRS=("x86_64|-machdep x86_64|-machdep gcc_x86_64")
share=$(frama-c -print-share-path 2>/dev/null)
if [ -f "${share}/machdeps/machdep_ppc_32.yaml" ]; then
  sed 's/^compiler: .*/compiler: gcc/' "${share}/machdeps/machdep_ppc_32.yaml" > "$D/machdep_ppc_32_gcc.yaml"
  PAIRS+=("ppc_32|-machdep ppc_32|-machdep $D/machdep_ppc_32_gcc.yaml")
fi
for y in "$@"; do
  if [ "$(basename "$y")" = machdep_cortex_m4.yaml ]; then
    sed 's/^compiler: .*/compiler: generic/' "$y" > "$D/machdep_cortex_m4_generic.yaml"
    PAIRS+=("cortex_m4|-machdep $D/machdep_cortex_m4_generic.yaml|-machdep $y")
  fi
done
for p in "${PAIRS[@]}"; do
  IFS='|' read -r name outside gcc <<< "$p"
  differ=""
  for u in "${UNITS[@]}"; do
    IFS='|' read -r file extra <<< "$u"
    b=$(basename "$file" .h)
    ok=1
    for mode in outside gcc; do
      if [ "$mode" = outside ]; then args=$outside; else args=$gcc; fi
      # shellcheck disable=SC2086
      if ! frama-c $args -cpp-extra-args="${BASE} ${extra} -DCANON_NO_REQUIRE -DNDEBUG" \
           "$file" -print -ocode "$D/mode_${name}_${mode}_${b}.c" \
           > "$D/mode_${name}_${mode}_${b}.log" 2>&1; then
        echo "FAIL: ${name}, ${mode}: ${b} not parsed"
        FAILS=$((FAILS + 1))
        ok=0
      fi
    done
    [ "$ok" = 1 ] || continue
    if ! diff <(grep -v '^#include' "$D/mode_${name}_outside_${b}.c") \
              <(grep -v '^#include' "$D/mode_${name}_gcc_${b}.c") > "$D/mode_${name}_${b}.diff"; then
      differ="${differ} ${b}"
      echo "-- ${name}: ${b}'s code differs:"
      head -20 "$D/mode_${name}_${b}.diff" | sed 's/^/   /'
    fi
  done
  if [ -z "$differ" ]; then
    echo "== ${name}: all ${#UNITS[@]} units print the same code outside and in GCC mode"
  else
    echo "== ${name}: the code differs between the modes at:${differ}"
  fi
done
echo "check.sh: ${FAILS} failed"
exit "$FAILS"
