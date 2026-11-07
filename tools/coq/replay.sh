#!/usr/bin/env bash
# tools/coq/replay.sh OUTDIR
#
# VERIFY-061, phase B: bits.h's twelve residual goals, proved in Coq. Run
# from the repository root, with the Frama-C opam switch active, after Coq
# 8.18.0 and Why3's Coq library (why3-coq 1.7.2) are installed and Why3
# has detected Coq (.github/workflows/coq.yml). The proofs are the twelve
# scripts of tools/coq/session/interactive/, one per goal, each the script
# WP writes for its goal (tools/coq/phase_a.sh) with the proof filled in.
# It prints:
#   1. each script compiled by Coq as Why3 runs Coq (coqtop -batch -R
#      <Why3's libdir>/coq Why3 -l <script>), with the time it took, and
#      the axioms each proof rests on (Print Assumptions);
#   2. a WP run of bits.h: frama-c-bits's command with Coq added to the
#      provers, in -wp-interactive batch mode, on a copy of the committed
#      session: WP gives Coq each script against the goal it generates now;
#   3. WP's summary, with each prover's count, and the unproved goals;
#   4. whether WP changed any script in its copy of the session;
#   5. the same WP run at one model of each other class (cortex_m4,
#      aarch64_linux_gnu, msvc_x86_64; the arguments are those of
#      proofs-machdeps.yml), each on its own copy of the session, with its
#      summary, unproved goals and changed scripts: phase B's run 2
#      measures there whether the scripts prove the twelve at the other
#      machine models too, for phase C.
# Exit status: the number of scripts Coq rejects at 1, plus the number of
# the twelve goals WP leaves unproved at 3. Part 5 is measured only.

set -u
OUT="${1:?usage: replay.sh OUTDIR}"
mkdir -p "$OUT"
SESSION=tools/coq/session
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

echo "=== 1. Each script, compiled by Coq as Why3 runs it"
coqc --version | head -1
why3lib="$(why3 --print-libdir)/coq"
REJECTED=0
mkdir -p "$OUT/coq"
for g in "${GOALS[@]}"; do
  n=${g#typed_}
  src="$SESSION/interactive/$n.v"
  if [ ! -f "$src" ]; then
    echo "-- ${n}: NO SCRIPT"
    REJECTED=$((REJECTED + 1))
    continue
  fi
  cp "$src" "$OUT/coq/$n.v"
  printf '\nPrint Assumptions wp_goal.\n' >> "$OUT/coq/$n.v"
  start=$(date +%s%N)
  if (cd "$OUT/coq" && coqtop -batch -R "$why3lib" Why3 -l "$n.v") > "$OUT/coq/$n.log" 2>&1 \
     && ! grep -q "^Error" "$OUT/coq/$n.log"; then
    verdict=ACCEPTED
  else
    verdict=REJECTED
    REJECTED=$((REJECTED + 1))
  fi
  ms=$(( ($(date +%s%N) - start) / 1000000 ))
  axioms=$(awk '/^Axioms:/{p=1; next} p && /^[^ ]/{print $1}' "$OUT/coq/$n.log" | sort -u | tr '\n' ' ')
  printf -- '-- %s: %s in %d.%d s; rests on: %s\n' "$n" "$verdict" $((ms / 1000)) $((ms % 1000 / 100)) "$axioms"
  [ "$verdict" = REJECTED ] && grep -v "^Warning\|deprecated\|disj-pattern" "$OUT/coq/$n.log" | tail -20
done

echo "=== 2. WP: bits.h, frama-c-bits's command with Coq added (batch mode)"
rm -rf "$OUT/session"
cp -r "$SESSION" "$OUT/session"
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5,coq \
  -wp-interactive batch \
  -wp-session "$OUT/session" \
  -wp-fct "$FUNCTIONS" \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" -I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG" \
  core/primitives/bits.h > "$OUT/wp_output.txt" 2>&1
echo "WP exit status: $?"

echo "=== 3. WP's summary, and the unproved goals"
awk '/^\[wp\] Proved goals:/{p=1; print; next} p && /^  /{print; next} {p=0}' "$OUT/wp_output.txt"
grep -E "^\[wp\] \[(Timeout|Unknown|Failed|Failure|Invalid|Stepout)\]" "$OUT/wp_output.txt" || echo "(none)"
UNPROVED=0
for g in "${GOALS[@]}"; do
  if grep -qE "^\[wp\] \[(Timeout|Unknown|Failed|Failure|Invalid|Stepout)\] (Goal )?${g}( |$)" "$OUT/wp_output.txt"; then
    echo "-- ${g}: UNPROVED"
    UNPROVED=$((UNPROVED + 1))
  fi
done

echo "=== 4. The session after WP"
find "$OUT/session" -type f | sort | sed 's/^/   /'
for f in "$SESSION"/interactive/*.v; do
  cmp -s "$f" "$OUT/session/interactive/$(basename "$f")" \
    || echo "-- $(basename "$f"): changed by WP"
done

echo "=== 5. The same WP run at one model of each other class"
MODELS=(
  "cortex_m4|-machdep tools/machdeps/machdep_cortex_m4.yaml -enums gcc-short-enums"
  "aarch64_linux_gnu|-machdep tools/machdeps/machdep_aarch64_linux_gnu.yaml"
  "msvc_x86_64|-machdep msvc_x86_64"
)
for m in "${MODELS[@]}"; do
  name=${m%%|*}
  args=${m#*|}
  rm -rf "$OUT/session_$name"
  cp -r "$SESSION" "$OUT/session_$name"
  # shellcheck disable=SC2086
  frama-c -wp -wp-rte ${args} \
    -wp-prover alt-ergo,z3,cvc5,coq \
    -wp-interactive batch \
    -wp-session "$OUT/session_$name" \
    -wp-fct "$FUNCTIONS" \
    -wp-timeout 120 \
    -wp-split \
    -cpp-extra-args=" -I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG" \
    core/primitives/bits.h > "$OUT/wp_output_$name.txt" 2>&1
  echo "-- ${name}: WP exit status $?"
  awk '/^\[wp\] Proved goals:/{p=1; print; next} p && /^  /{print; next} {p=0}' "$OUT/wp_output_$name.txt"
  grep -E "^\[wp\] \[(Timeout|Unknown|Failed|Failure|Invalid|Stepout)\]" "$OUT/wp_output_$name.txt" \
    || echo "   no goal unproved"
  for f in "$SESSION"/interactive/*.v; do
    cmp -s "$f" "$OUT/session_$name/interactive/$(basename "$f")" \
      || echo "   $(basename "$f"): changed by WP"
  done
done

echo "replay.sh: ${#GOALS[@]} scripts, ${REJECTED} rejected by Coq; ${UNPROVED} of the twelve unproved by WP"
exit $((REJECTED + UNPROVED))
