#!/usr/bin/env bash
# VERIFY-046, VERIFY-048: compile Canon-C for bare-metal Arm (newlib and
# picolibc) and RISC-V RV32 and RV64 (picolibc).
#
# Compile only: nothing is linked or run on the target. Every public header
# is compiled as its own translation unit, and every test file, under the
# project's warning flags (-Wall -Wextra -Wpedantic -Werror, C99), for each
# CPU and optimisation level below. Fails if any compile fails, or if a list
# is empty.
#
# Left out, each for a stated reason:
#   util/time.h, test/util/time_test.c  — need an OS clock (README, Tier 2)
#   test/concurrency/*                  — need pthreads
#   *_impl.h, *_mangle.h                — template fragments, reached through
#                                         the headers that include them
#   data/hashmap/{hashmap,hashmap_decl,hashmap_defn,hashmap_range,
#   hashmap_fmt}.h                      — need HASHMAP_KEY_TYPE (and a key
#                                         formatter); compiled through
#                                         test/data/hashmap_test.c
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
INC="-I . -I core/primitives -I core -I semantics -I data -I algo -I util"
WARN="-std=c99 -Wall -Wextra -Wpedantic -Werror"

mapfile -t HEADERS < <(find core semantics data algo util -name '*.h' \
  | grep -vE '_(impl|mangle)\.h$' \
  | grep -vE '^util/time\.h$' \
  | grep -vE '^data/hashmap/hashmap(_decl|_defn|_range|_fmt)?\.h$' | sort)
mapfile -t TESTS < <(find test -name '*_test.c' \
  | grep -vE '^test/concurrency/|^test/util/time_test\.c$' | sort)

if [ "${#HEADERS[@]}" -eq 0 ] || [ "${#TESTS[@]}" -eq 0 ]; then
  echo "FAIL: empty header or test list"; exit 1
fi
for cc in arm-none-eabi-gcc riscv64-unknown-elf-gcc; do "$cc" --version | head -1; done
echo "headers: ${#HEADERS[@]}  tests: ${#TESTS[@]}"

FAILS=0
for cpu in "${TARGETS[@]}"; do
  for opt in "${OPTS[@]}"; do
    n=0
    for h in "${HEADERS[@]}"; do
      printf '#include "%s"\ntypedef int canon_tu_not_empty_;\n' "$h" > "$OUT/h.c"
      # shellcheck disable=SC2086
      if ! $cpu $opt $WARN $INC -c "$OUT/h.c" -o "$OUT/h.o" 2> "$OUT/err"; then
        echo "FAIL [$cpu $opt] header $h"; sed 's/^/    /' "$OUT/err" | head -20
        FAILS=$((FAILS + 1))
      else n=$((n + 1)); fi
    done
    for t in "${TESTS[@]}"; do
      # shellcheck disable=SC2086
      if ! $cpu $opt $WARN $INC -c "$t" -o "$OUT/t.o" 2> "$OUT/err"; then
        echo "FAIL [$cpu $opt] test $t"; sed 's/^/    /' "$OUT/err" | head -20
        FAILS=$((FAILS + 1))
      else n=$((n + 1)); fi
    done
    echo "[$cpu $opt] $n / $(( ${#HEADERS[@]} + ${#TESTS[@]} )) compiled"
  done
done
SUMMARY="${#TARGETS[@]} targets x ${#OPTS[@]} levels, $(( ${#HEADERS[@]} + ${#TESTS[@]} )) units each, ${FAILS} failed"
[ -n "${GITHUB_ACTIONS:-}" ] && echo "::notice title=embedded::${SUMMARY}"   # VERIFY-050
if [ "$FAILS" -ne 0 ]; then echo "FAIL: $FAILS compile(s) failed"; exit 1; fi
echo "OK"
