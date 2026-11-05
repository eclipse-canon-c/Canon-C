#!/usr/bin/env bash
# tools/machdeps/check.sh DIR
#
# VERIFY-060, phase A. For each model in DIR (machdep_<name>.yaml, written by
# generate.sh), and for Frama-C's own x86_64 and gcc_x86_32 to compare:
#   1. Frama-C reads check.c at the model with -constfold, and the probe_
#      lines of the code it prints are shown: sizes, alignments, CHAR_MIN, and
#      the compiler macros its preprocessing defines.
#   2. Each of the ten core/ units is parsed and typed at the model, with its
#      proof job's preprocessing flags, without running WP.
# The Cortex-M4 model is also read with -enums gcc-short-enums, the enum
# layout of arm-none-eabi. Run from the repository root.
# Exit status: the number of reads that failed.

set -u
D="${1:?usage: check.sh DIR}"
HERE=$(cd "$(dirname "$0")" && pwd)
FAILS=0

MODELS=("x86_64|-machdep x86_64" "gcc_x86_32|-machdep gcc_x86_32")
for y in "$D"/machdep_*.yaml; do
  [ -e "$y" ] || continue
  n=$(basename "$y" .yaml); n=${n#machdep_}
  MODELS+=("${n}|-machdep ${y}")
done
if [ -e "$D/machdep_cortex_m4.yaml" ]; then
  MODELS+=("cortex_m4+short_enums|-machdep $D/machdep_cortex_m4.yaml -enums gcc-short-enums")
fi

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
  for u in "${UNITS[@]}"; do
    IFS='|' read -r file extra <<< "$u"
    log="$D/parse_${name}_$(basename "$file" .h).log"
    # shellcheck disable=SC2086
    if ! frama-c $args -cpp-extra-args="${BASE} ${extra} -DCANON_NO_REQUIRE -DNDEBUG" \
         "$file" > "$log" 2>&1; then
      bad="${bad} $(basename "$file")"
      FAILS=$((FAILS + 1))
    fi
  done
  if [ -z "$bad" ]; then
    echo "== ${name}: all ${#UNITS[@]} units parsed"
  else
    echo "FAIL: ${name}: not parsed:${bad}"
  fi
done
echo "check.sh: ${FAILS} failed"
exit "$FAILS"
