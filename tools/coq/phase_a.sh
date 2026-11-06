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
#      in full: WP names each after its goal without the "typed_" prefix
#      (session/interactive/bits_clear_ensures.v; phase A's run #1);
#   4. what the scripts can require: the Coq prover's entry in Why3's
#      detection data (its command, `-R %l/coq Why3`), Why3's Coq drivers,
#      and the source of each module of Why3's Coq library. The scripts
#      require that library alone, WP's theories being written into each
#      script (run #2). why3-coq installs it compiled only, in Why3's
#      libdir/coq (run #2), so the sources come from why3-coq's source
#      archive: for each compiled module there, the .v of the same path.
# Exit status: the number of the twelve goals with no script found, plus
# the number of the library's compiled modules with no source found.

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
  files=$(find "$OUT/session" -type f -name "${g#typed_}.v" | sort)
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

echo "=== 4. What the scripts can require"
why3data=$(why3 --print-datadir)
echo "why3 libdir: $(why3 --print-libdir); datadir: ${why3data}"
echo "-- Coq in ${why3data}/provers-detection-data.conf:"
awk '/^\[/{p=0} /^\[ATP coq\]|^\[ITP coq\]/{p=1} p' "${why3data}/provers-detection-data.conf" | sed 's/^/   /'
echo "-- Why3's Coq drivers:"
for f in $(find "${why3data}/drivers" -type f \( -name 'coq*.drv' -o -name 'coq*.gen' \) | sort); do
  echo "-- FILE ${f}"
  cat "$f"
  echo "-- END ${f}"
done
why3coq="$(why3 --print-libdir)/coq"
mods=$(cd "$why3coq" && find . -type f -name '*.vo' | sed 's#^\./##; s#\.vo$##' | sort)
echo "-- Why3's Coq library: $(echo "$mods" | grep -c .) compiled modules in ${why3coq}:"
for m in $mods; do echo "   ${m}"; done
src=""
if ! find "$why3coq" -type f -name '*.v' | grep -q .; then
  echo "-- installed compiled only: the sources from why3-coq 1.7.2's source archive"
  opam source why3-coq.1.7.2 --dir "$OUT/why3-src" > "$OUT/opam-source.log" 2>&1 \
    || { echo "   opam source failed:"; tail -5 "$OUT/opam-source.log"; }
  src=$(find "$OUT/why3-src" -type d -path '*/lib/coq' 2>/dev/null | head -1)
  echo "   the archive's lib/coq: ${src:-none}"
fi
NOSRC=0
for m in $mods; do
  f="${why3coq}/${m}.v"
  [ -f "$f" ] || f="${src:-/nonexistent}/${m}.v"
  if [ ! -f "$f" ]; then
    echo "-- ${m}: no source found"
    NOSRC=$((NOSRC + 1))
    continue
  fi
  echo "-- FILE ${f}"
  cat "$f"
  echo "-- END ${f}"
done

echo "phase_a.sh: ${#GOALS[@]} goals, ${MISSING} with no script found;" \
  "$(echo "$mods" | grep -c .) library modules, ${NOSRC} with no source found"
exit $((MISSING + NOSRC))
