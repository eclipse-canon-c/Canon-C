#!/usr/bin/env bash
# VERIFY-048: the README's platform tiers, checked on AVR (avr-gcc, avr-libc).
# AVR has a 16-bit size_t, the case Tier 0 and Tier 1 are drawn around.
#
# Part 1, stock toolchain. Ubuntu's avr-gcc 7 makes double 4 bytes (DBL_DIG
#   6, below C's 10). types.h requires sizeof(f64) == 8, so every header
#   that includes types.h must be REFUSED there, by that check. The README
#   says AVR needs a 64-bit double (-mdouble=64, GCC 10 and later).
# Part 2, the tiers. In a scratch copy with only that one check removed:
#   - the README's Tier 0 headers must compile;
#   - the Tier 1 headers must be refused by limits.h's Tier 1 #error.
#   The classification of every public header is printed.
# Compile only. Nothing is linked or run.
set -u
CC=${CC:-avr-gcc}
MCU=${MCU:-atmega328p}
WARN="-std=c99 -Wall -Wextra -Wpedantic -Werror"
INC="-I . -I core/primitives -I core -I semantics -I data -I algo -I util"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
"$CC" --version | head -1
FAILS=0

TIER0=(core/primitives/types.h core/primitives/bits.h core/primitives/compare.h
       core/scope.h core/primitives/contract.h)
TIER1=(core/primitives/limits.h core/primitives/checked.h core/primitives/ptr.h
       core/memory.h core/arena.h core/pool.h core/region.h data/stringbuf.h)

compile() {  # dir header -> sets $OUT/err; returns the compiler's status
  printf '#include "%s"\ntypedef int canon_tu_not_empty_;\n' "$2" > "$OUT/h.c"
  # shellcheck disable=SC2086
  (cd "$1" && "$CC" -mmcu="$MCU" $WARN $INC -c "$OUT/h.c" -o "$OUT/h.o") 2> "$OUT/err"
}

echo "=== Part 1: stock $CC refuses at types.h's f64 check ==="
for h in "${TIER0[@]}"; do
  [ "$h" = core/scope.h ] && continue   # includes nothing, so no f64 check
  if compile . "$h"; then
    echo "FAIL: $h compiled with a 4-byte double"; FAILS=$((FAILS + 1))
  elif grep -q canon_types_assert_f64_is_8_bytes_ "$OUT/err"; then
    echo "  refused, by the f64 check: $h"
  else
    echo "FAIL: $h refused, but not by the f64 check:"; head -5 "$OUT/err"; FAILS=$((FAILS + 1))
  fi
done
if compile . core/scope.h; then echo "  compiles: core/scope.h (no includes)"
else echo "FAIL: core/scope.h"; head -5 "$OUT/err"; FAILS=$((FAILS + 1)); fi

echo "=== Part 2: the tiers, with only the f64 check removed ==="
SCRATCH="$OUT/src"; mkdir -p "$SCRATCH"
cp -r core semantics data algo util "$SCRATCH/"
T="$SCRATCH/core/primitives/types.h"
before=$(grep -c 'canon_types_assert_f64_is_8_bytes_' "$T")
sed -i '/^typedef char canon_types_assert_f64_is_8_bytes_/d' "$T"
after=$(grep -c 'canon_types_assert_f64_is_8_bytes_' "$T")
if [ "$before" -ne 1 ] || [ "$after" -ne 0 ]; then
  echo "FAIL: expected to remove exactly one f64 check line (before $before, after $after)"; exit 1
fi
for h in "${TIER0[@]}"; do
  if compile "$SCRATCH" "$h"; then echo "  Tier 0 compiles: $h"
  else echo "FAIL: Tier 0 header refused: $h"; head -5 "$OUT/err"; FAILS=$((FAILS + 1)); fi
done
for h in "${TIER1[@]}"; do
  if compile "$SCRATCH" "$h"; then
    echo "FAIL: Tier 1 header compiled at 16-bit size_t: $h"; FAILS=$((FAILS + 1))
  elif grep -q 'Canon-C platform contract (Tier 1)' "$OUT/err"; then
    echo "  Tier 1 refused, by its #error: $h"
  else
    echo "FAIL: $h refused, but not by the Tier 1 #error:"; head -5 "$OUT/err"; FAILS=$((FAILS + 1))
  fi
done

echo "=== Every public header (report only) ==="
mapfile -t ALL < <(find core semantics data algo util -name '*.h' \
  | grep -vE '_(impl|mangle)\.h$' \
  | grep -vE '^data/hashmap/hashmap(_decl|_defn|_range|_fmt)?\.h$' | sort)
ok=0; t1=0; other=0
for h in "${ALL[@]}"; do
  if compile "$SCRATCH" "$h"; then ok=$((ok + 1)); echo "  compiles   $h"
  elif grep -q 'Canon-C platform contract (Tier 1)' "$OUT/err"; then t1=$((t1 + 1))
  else other=$((other + 1)); echo "  other      $h: $(grep -m1 -oE 'error:.*' "$OUT/err" | cut -c1-120)"; fi
done
echo "  ${#ALL[@]} headers: $ok compile, $t1 refused by Tier 1, $other other"

if [ "$FAILS" -ne 0 ]; then echo "FAIL: $FAILS check(s) failed"; exit 1; fi
echo "OK"
