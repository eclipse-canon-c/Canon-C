#!/usr/bin/env bash
# VERIFY-107, VERIFY-108: what Microchip's XC-DSC is, where the `dspic` job
# runs it: its version and devices, whether an empty unit compiles at each
# device, the type sizes there, with and without -fno-short-double, and
# whether each optimization level and the project's warning flags pass
# without a message. Compile only; nothing is linked. VERIFY-107's run 1
# printed it; with PINS, its output must be that file's, line for line
# (tools/dspic/probe.pinned, run 1's, since VERIFY-108).
#
#   CC    the compiler (xc-dsc-gcc)
#   CPUS  the devices, XC-DSC's virtual ones (generic-16dsp generic-16bit):
#         a core with the DSP instructions, dsPIC's, and one without, PIC24's
#   PINS  the file the output must match (none: printed only)
set -u
CC=${CC:-xc-dsc-gcc}
read -r -a CPUS <<< "${CPUS:-generic-16dsp generic-16bit}"
WARN="-std=c99 -Wall -Wextra -Wpedantic -Werror"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
command -v "$CC" > /dev/null || { echo "FAIL: no $CC on the PATH"; exit 1; }
BIN=$(dirname "$(command -v "$CC")")
# The compiler's directory is written <bin>, and no line ends in a blank,
# so that the output does not depend on where the archive is unpacked.
esc=$(printf '%s' "$BIN" | sed 's/[][\\.*^$#]/\\&/g')

first() {  # file -> its first line, as <bin> and cut, or "no message"
  if [ -s "$1" ]; then head -1 "$1" | sed "s#$esc#<bin>#g" | cut -c1-160; else echo "no message"; fi
}

macro() {  # file name -> its value, or ?
  local v
  v=$(sed -n "s/^#define $2 //p" "$1" | head -1)
  echo "${v:-?}"
}

probe() {  # -> the report; sets SUMMARY, LEVELS and WST for the notice
  echo "$CC: $("$CC" --version 2>&1 | head -1)"

  echo "=== Devices ==="
  "$CC" -mprint-devices > "$OUT/devices" 2>&1
  local st=$?
  local listed="" cpu flag o msg ch end types names
  for cpu in "${CPUS[@]}"; do listed+=", $cpu listed $(grep -cw -- "$cpu" "$OUT/devices")"; done
  echo "  -mprint-devices: status $st, $(wc -l < "$OUT/devices") lines$listed"

  printf 'int canon_probe_(int x) { return x + 1; }\n' > "$OUT/p.c"
  echo "=== An empty unit, compiled ==="
  for flag in "" "${CPUS[@]/#/-mcpu=}"; do
    # shellcheck disable=SC2086
    "$CC" $flag -c "$OUT/p.c" -o "$OUT/p.o" 2> "$OUT/err"
    echo "  ${flag:-(no -mcpu)}: status $?, $(first "$OUT/err")"
  done

  echo "=== The types, in bytes, at each device ==="
  SUMMARY=""
  for cpu in "${CPUS[@]}"; do
    if ! "$CC" -mcpu="$cpu" -dM -E -x c /dev/null > "$OUT/m" 2> "$OUT/err" ||
       ! "$CC" -mcpu="$cpu" -fno-short-double -dM -E -x c /dev/null > "$OUT/m64" 2>> "$OUT/err"; then
      echo "  $cpu: the preprocessor failed: $(first "$OUT/err")"
      SUMMARY+="; $cpu: no types"
      continue
    fi
    if grep -q '^#define __CHAR_UNSIGNED__ ' "$OUT/m"; then ch=unsigned; else ch=signed; fi
    case "$(macro "$OUT/m" __BYTE_ORDER__)" in
      __ORDER_LITTLE_ENDIAN__) end=little-endian ;;
      __ORDER_BIG_ENDIAN__) end=big-endian ;;
      *) end="byte order ?" ;;
    esac
    types="char $ch, short $(macro "$OUT/m" __SIZEOF_SHORT__), int $(macro "$OUT/m" __SIZEOF_INT__)"
    types+=", long $(macro "$OUT/m" __SIZEOF_LONG__), long long $(macro "$OUT/m" __SIZEOF_LONG_LONG__)"
    types+=", pointer $(macro "$OUT/m" __SIZEOF_POINTER__), size_t $(macro "$OUT/m" __SIZEOF_SIZE_T__)"
    types+=", float $(macro "$OUT/m" __SIZEOF_FLOAT__), double $(macro "$OUT/m" __SIZEOF_DOUBLE__)"
    types+=" ($(macro "$OUT/m64" __SIZEOF_DOUBLE__) with -fno-short-double)"
    types+=", long double $(macro "$OUT/m" __SIZEOF_LONG_DOUBLE__), $end"
    echo "  $cpu: $types"
    echo "  $cpu: GCC $(macro "$OUT/m" __GNUC__).$(macro "$OUT/m" __GNUC_MINOR__).$(macro "$OUT/m" __GNUC_PATCHLEVEL__), __XC_DSC_VERSION__ $(macro "$OUT/m" __XC_DSC_VERSION__)"
    names=$(grep -oE '^#define __(XC|C30|dsPIC|PIC24|HAS_|GENERIC|generic)[A-Za-z0-9_]*' "$OUT/m" | sed 's/^#define //' | sort -u | tr '\n' ' ')
    echo "  $cpu: Microchip's macros: ${names:-none}"
    SUMMARY+="; $cpu: $types"
  done

  echo "=== The optimization levels and the project's warning flags, at ${CPUS[0]} ==="
  LEVELS=""
  for o in -O0 -O1 -O2 -O3 -Os; do
    "$CC" -mcpu="${CPUS[0]}" "$o" -c "$OUT/p.c" -o "$OUT/p.o" 2> "$OUT/err"
    st=$?
    msg=$(first "$OUT/err")
    echo "  $o: status $st, $msg"
    LEVELS+=", $o $st $msg"
  done
  # shellcheck disable=SC2086
  "$CC" -mcpu="${CPUS[0]}" $WARN -c "$OUT/p.c" -o "$OUT/p.o" 2> "$OUT/err"
  WST=$?
  echo "  $WARN: status $WST, $(first "$OUT/err")"
}

probe > "$OUT/raw"
sed -e "s#$esc#<bin>#g" -e 's/[[:space:]]*$//' "$OUT/raw" > "$OUT/probe"
cat "$OUT/probe"
if [ -z "${PINS:-}" ]; then
  RESULT="not pinned"
elif diff -u "$PINS" "$OUT/probe"; then
  RESULT="as pinned ($PINS, $(wc -l < "$PINS") lines)"
else
  RESULT="FAIL: not as pinned ($PINS)"
fi
echo "$RESULT"
VERSION=$("$CC" --version 2>&1 | head -1 | sed -e 's/ *Build date:.*//' -e 's/[[:space:]]*$//')
[ -n "${GITHUB_ACTIONS:-}" ] && echo "::notice title=dspic probe::${VERSION}${SUMMARY}; levels${LEVELS#,}; warning flags status ${WST}; ${RESULT}"   # VERIFY-050
case "$RESULT" in FAIL*) exit 1 ;; esac
exit 0
