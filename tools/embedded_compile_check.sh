#!/usr/bin/env bash
# VERIFY-046, VERIFY-048: compile Canon-C for bare-metal Arm (newlib and
# picolibc) and RISC-V RV32 and RV64 (picolibc).
#
# Compile only: nothing is linked or run on the target. Every public header
# is compiled as its own translation unit, and every test file, under the
# project's warning flags (-Wall -Wextra -Wpedantic -Werror, C99), for each
# CPU and optimisation level below. Each header twice (VERIFY-069): as GCC
# sees it, and with CANON_NO_GNU_EXTENSIONS, the portable path a compiler
# that is not GCC's takes (CompCert compiles the tests that way in
# `3 · Static analysis & coverage`). Fails if any compile fails, or if a
# list is empty.
#
# Left out, each for a stated reason:
#   util/time.h, test/util/time_test.c  — need an OS clock (README, Tier 2)
#   test/concurrency/*                  — need pthreads
#   *_impl.h, *_mangle.h                — template fragments, reached through
#                                         the headers that include them
#
# The AUTOSAR adapter, integrations/autosar/canon_autosar.h, is compiled
# through its test, test/integrations/autosar/autosar_test.c, with the
# stand-in AUTOSAR headers beside the test on the include path (VERIFY-075):
# it needs a ModuleId and the project's AUTOSAR headers, which the test gives
# it.
#
# data/hashmap/'s five headers need a key and value type first, and each
# what its documentation asks before it; each is compiled after just that
# (hashmap_tu below, VERIFY-069), and through the tests that use them
# (hashmap_test.c, hashmap_multi_test.c, hashmap_separate_test.c).
#
# Targets (VERIFY-046; picolibc and RISC-V added by VERIFY-048). Each entry
# is a compiler and its flags; every one is run at -O0, -Os and -O2.
set -u
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

TARGETS=(
  "arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb"
  "arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16"
  "arm-none-eabi-gcc --specs=picolibc.specs -mcpu=cortex-m0plus -mthumb"
  "arm-none-eabi-gcc --specs=picolibc.specs -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16"
  "riscv64-unknown-elf-gcc --specs=picolibc.specs -march=rv32imac -mabi=ilp32"
  "riscv64-unknown-elf-gcc --specs=picolibc.specs -march=rv64imac -mabi=lp64 -mcmodel=medany"
)
OPTS=(-O0 -Os -O2)
INC="-I . -I core/primitives -I core -I semantics -I data -I algo -I util -I integrations/autosar -I test/integrations/autosar/stubs"
WARN="-std=c99 -Wall -Wextra -Wpedantic -Werror"

mapfile -t HEADERS < <(find core semantics data algo util -name '*.h' \
  | grep -vE '_(impl|mangle)\.h$' \
  | grep -vE '^util/time\.h$' \
  | grep -vE '^data/hashmap/hashmap(_decl|_defn|_range|_fmt)?\.h$' | sort)
HASHMAP=(hashmap hashmap_decl hashmap_defn hashmap_range hashmap_fmt)
CONFIGS=("" "-DCANON_NO_GNU_EXTENSIONS")

# A translation unit for one of data/hashmap/'s headers: the key and value
# types, the hash and equality functions (declared: nothing is linked), and
# for hashmap_range.h and hashmap_fmt.h, hashmap.h with what each needs
# besides: Vec<K> and Vec<V> (test/support/hashmap_vecs.h, as the tests
# instantiate them), and StringBuf with the two formatting functions.
hashmap_tu() {
  printf '%s\n' '#include "core/primitives/types.h"' '#include <stdbool.h>' \
    'u64 canon_tu_hash(const u64* key, void* ctx);' \
    'bool canon_tu_eq(const u64* a, const u64* b, void* ctx);' \
    '#define HASHMAP_KEY_TYPE u64' '#define HASHMAP_VAL_TYPE int' \
    '#define HASHMAP_HASH_FN canon_tu_hash' '#define HASHMAP_EQ_FN canon_tu_eq'
  case "$1" in
    hashmap_range)
      printf '%s\n' '#include "data/hashmap/hashmap.h"' \
        '#include "test/support/hashmap_vecs.h"' ;;
    hashmap_fmt)
      printf '%s\n' '#include "data/hashmap/hashmap.h"' '#include "data/stringbuf.h"' \
        'bool canon_tu_fmt_key(StringBuf* sb, const u64* k);' \
        'bool canon_tu_fmt_val(StringBuf* sb, const int* v);' \
        '#define HASHMAP_FMT_KEY_FN canon_tu_fmt_key' \
        '#define HASHMAP_FMT_VAL_FN canon_tu_fmt_val' ;;
  esac
  printf '#include "data/hashmap/%s.h"\ntypedef int canon_tu_not_empty_;\n' "$1"
}
mapfile -t TESTS < <(find test -name '*_test.c' \
  | grep -vE '^test/concurrency/|^test/util/time_test\.c$' | sort)

if [ "${#HEADERS[@]}" -eq 0 ] || [ "${#TESTS[@]}" -eq 0 ]; then
  echo "FAIL: empty header or test list"; exit 1
fi
for cc in arm-none-eabi-gcc riscv64-unknown-elf-gcc; do "$cc" --version | head -1; done
echo "headers: ${#HEADERS[@]} + ${#HASHMAP[@]} hashmap, each twice  tests: ${#TESTS[@]}"
UNITS=$(( ${#CONFIGS[@]} * (${#HEADERS[@]} + ${#HASHMAP[@]}) + ${#TESTS[@]} ))

FAILS=0
for cpu in "${TARGETS[@]}"; do
  for opt in "${OPTS[@]}"; do
    n=0
    for cfg in "${CONFIGS[@]}"; do
      for h in "${HEADERS[@]}" "${HASHMAP[@]}"; do
        case "$h" in
          hashmap*) hashmap_tu "$h" > "$OUT/h.c"; h="data/hashmap/$h.h" ;;
          *) printf '#include "%s"\ntypedef int canon_tu_not_empty_;\n' "$h" > "$OUT/h.c" ;;
        esac
        # shellcheck disable=SC2086
        if ! $cpu $opt $WARN $cfg $INC -c "$OUT/h.c" -o "$OUT/h.o" 2> "$OUT/err"; then
          echo "FAIL [$cpu $opt${cfg:+ $cfg}] header $h"; sed 's/^/    /' "$OUT/err" | head -20
          FAILS=$((FAILS + 1))
        else n=$((n + 1)); fi
      done
    done
    for t in "${TESTS[@]}"; do
      # shellcheck disable=SC2086
      if ! $cpu $opt $WARN $INC -c "$t" -o "$OUT/t.o" 2> "$OUT/err"; then
        echo "FAIL [$cpu $opt] test $t"; sed 's/^/    /' "$OUT/err" | head -20
        FAILS=$((FAILS + 1))
      else n=$((n + 1)); fi
    done
    echo "[$cpu $opt] $n / $UNITS compiled"
  done
done
SUMMARY="${#TARGETS[@]} targets x ${#OPTS[@]} levels, ${UNITS} units each ($(( ${#HEADERS[@]} + ${#HASHMAP[@]} )) headers, with and without CANON_NO_GNU_EXTENSIONS, and ${#TESTS[@]} tests), ${FAILS} failed"
[ -n "${GITHUB_ACTIONS:-}" ] && echo "::notice title=embedded::${SUMMARY}"   # VERIFY-050
if [ "$FAILS" -ne 0 ]; then echo "FAIL: $FAILS compile(s) failed"; exit 1; fi
echo "OK"
