#!/usr/bin/env bash
# tools/coq/phase_a.sh OUTDIR
#
# VERIFY-061, phase A: bits.h's twelve residual goals as Coq is given them.
# Run from the repository root, with the Frama-C opam switch active, after
# Coq and Why3's Coq library (why3-coq) are installed and Why3 has detected
# Coq. It proves nothing, and prints what the proofs will be written
# against, so that the log carries it (the job's artifact cannot be
# downloaded where the proofs are written):
#   1. Why3's configuration for Coq, the coqc command among it;
#   2. a WP run of bits.h: frama-c-bits's command with Coq added to the
#      provers, in -wp-interactive update mode, which writes a Coq script
#      for the goals it gives Coq into the session's interactive directory;
#   3. every file the session holds, by name, and the twelve goals' scripts
#      in full;
#   4. the Coq sources the scripts can require: Why3's Coq library and
#      WP's own.
# Exit status: the number of the twelve goals with no script found.

set -u
OUT="${1:?usage: phase_a.sh OUTDIR}"
mkdir -p "$OUT"

GOALS=(
  typed_bits_bswap16_assert_rte_signed_overflow
  typed_bits_bswap32_ensures
  typed_bits_bswap64_ensures
  typed_bits_clear_ensures
  typed_bits_insert_normal_ensures_part3
  typed_bits_next_power_of_two_normal_ensures_2_part3
  typed_bits_next_power_of_two_normal_ensures_3_part3
  typed_bits_next_power_of_two_normal_ensures_4_part3
  typed_bits_next_power_of_two_normal_ensures_part3
  typed_bits_popcount_ensures
  typed_bits_rotl_assert_rte_shift_2
  typed_bits_rotr_assert_rte_shift_2
)
FUNCTIONS=bits_bswap16,bits_bswap32,bits_bswap64,bits_clear,bits_insert,bits_next_power_of_two,bits_popcount,bits_rotl,bits_rotr

echo "=== 1. Why3's configuration for Coq"
why3 --version
coqc --version
why3 config list-provers | sort
conf="${HOME}/.why3.conf"
awk '/^\[prover\]/{p=0} /^\[/{s=$0} /^name = "Coq"/{p=1; print s} p' "$conf" | sed 's/^/   /'

echo "=== 2. WP: bits.h, frama-c-bits's command with Coq added (update mode)"
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5,coq \
  -wp-interactive update \
  -wp-session "$OUT/session" \
  -wp-fct "$FUNCTIONS" \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" -I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG" \
  core/primitives/bits.h > "$OUT/wp_output.txt" 2>&1
echo "WP exit status: $?"
grep -E "^\[wp\] (Proved goals|\[(Timeout|Unknown|Failed|Failure|Invalid|Stepout)\])|Coq|coq" \
  "$OUT/wp_output.txt" | head -80

echo "=== 3. The session's files"
find "$OUT/session" -type f | sort | sed 's/^/   /'
MISSING=0
for g in "${GOALS[@]}"; do
  files=$(find "$OUT/session" -type f -name "*${g}*" | sort)
  if [ -z "$files" ]; then
    echo "-- ${g}: no file in the session"
    MISSING=$((MISSING + 1))
    continue
  fi
  for f in $files; do
    echo "-- FILE ${f#"$OUT"/}"
    cat "$f"
    echo "-- END ${f#"$OUT"/}"
  done
done

echo "=== 4. The Coq sources the scripts can require"
why3lib=$(why3 --print-libdir)
wpshare="$(frama-c -print-share-path)/wp"
for d in "${why3lib}/coq" "${wpshare}"; do
  echo "-- directory ${d}:"
  find "$d" -type f \( -name '*.v' -o -name '_CoqProject' -o -name '*.drv' \) 2>/dev/null \
    | sort | sed 's/^/   /'
done
find "${why3lib}/coq" "${wpshare}" -type f -name '*.v' 2>/dev/null | sort | while read -r f; do
  echo "-- FILE ${f}"
  cat "$f"
  echo "-- END ${f}"
done

echo "phase_a.sh: ${#GOALS[@]} goals, ${MISSING} with no script found"
exit "$MISSING"
