#!/usr/bin/env bash
# tools/coq/replay.sh OUTDIR
#
# VERIFY-061: bits.h's twelve residual goals, proved in Coq. Run from the
# repository root, with the Frama-C opam switch active, after Coq 8.18.0
# and Why3's Coq library (why3-coq 1.7.2) are installed and Why3 has
# detected Coq (.github/workflows/coq.yml). The proofs are the twelve
# scripts of tools/coq/session/interactive/, one per goal, each the script
# WP writes for its goal (tools/coq/phase_a.sh) with the proof filled in.
# It prints:
#   1. each committed script compiled by Coq as Why3 runs Coq (coqtop
#      -batch -R <Why3's libdir>/coq Why3 -l <script>), with the time it
#      took and the names its proof rests on (Print Assumptions). A script
#      passes if Coq accepts it and each of those names is declared in the
#      script itself, a Parameter or an Axiom of WP's theory;
#   2. at x86_64 and at the nine machine models of proofs-machdeps.yml,
#      with its arguments: frama-c-bits's command with Coq added to the
#      provers, in -wp-interactive batch mode, each on its own copy of the
#      committed session, so that WP gives Coq each script against the goal
#      it generates there. For each model, WP's summary, the unproved goals
#      and any script WP changed. A model passes if WP gives 100 / 100 at
#      the nine functions, credits Coq with twelve goals, leaves no goal
#      unproved and changes no script.
# Exit status: the number of scripts that do not pass at 1, plus the
# number of models that do not pass at 2.

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
# x86_64 is Frama-C's default; the others as proofs-machdeps.yml gives them.
MODELS=(
  "x86_64|"
  "cortex_m4|-machdep tools/machdeps/machdep_cortex_m4.yaml -enums gcc-short-enums"
  "rv32_ilp32|-machdep tools/machdeps/machdep_rv32_ilp32.yaml"
  "rv64_lp64|-machdep tools/machdeps/machdep_rv64_lp64.yaml"
  "arm_linux_gnueabihf|-machdep tools/machdeps/machdep_arm_linux_gnueabihf.yaml"
  "rv64_linux_gnu|-machdep tools/machdeps/machdep_rv64_linux_gnu.yaml"
  "aarch64_linux_gnu|-machdep tools/machdeps/machdep_aarch64_linux_gnu.yaml"
  "ppc32_linux_gnu|-machdep tools/machdeps/machdep_ppc32_linux_gnu.yaml"
  "ppc_32|-machdep ppc_32"
  "msvc_x86_64|-machdep msvc_x86_64"
)

echo "=== 1. Each script, compiled by Coq as Why3 runs it"
coqc --version | head -1
why3lib="$(why3 --print-libdir)/coq"
BADSCRIPTS=0
mkdir -p "$OUT/coq"
for g in "${GOALS[@]}"; do
  n=${g#typed_}
  src="$SESSION/interactive/$n.v"
  if [ ! -f "$src" ]; then
    echo "-- ${n}: NO SCRIPT"
    BADSCRIPTS=$((BADSCRIPTS + 1))
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
  fi
  ms=$(( ($(date +%s%N) - start) / 1000000 ))
  rests=$(awk '/^Axioms:/{p=1; next} p && /^[^ ]/{print $1}' "$OUT/coq/$n.log" | sort -u)
  declared=$(grep -oE "^(Parameter|Axiom) [A-Za-z_][A-Za-z0-9_']*" "$src" | awk '{print $2}' | sort -u)
  foreign=$(comm -13 <(echo "$declared") <(echo "$rests") | grep . | tr '\n' ' ')
  printf -- '-- %s: %s in %d.%d s; rests on %d names: %s\n' \
    "$n" "$verdict" $((ms / 1000)) $((ms % 1000 / 100)) \
    "$(echo "$rests" | grep -c .)" "$(echo "$rests" | tr '\n' ' ')"
  if [ "$verdict" = REJECTED ]; then
    grep -v "^Warning\|deprecated\|disj-pattern" "$OUT/coq/$n.log" | tail -20
    BADSCRIPTS=$((BADSCRIPTS + 1))
  elif [ -n "$foreign" ]; then
    echo "   FAIL: it rests on names not declared in the script: ${foreign}"
    BADSCRIPTS=$((BADSCRIPTS + 1))
  fi
done

echo "=== 2. WP with Coq, at x86_64 and the nine machine models (batch mode)"
BADMODELS=0
for m in "${MODELS[@]}"; do
  name=${m%%|*}
  args=${m#*|}
  out="$OUT/wp_output_$name.txt"
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
    core/primitives/bits.h > "$out" 2>&1
  status=$?
  echo "-- ${name}${args:+ (${args})}: WP exit status ${status}"
  awk '/^\[wp\] Proved goals:/{p=1; print; next} p && /^  /{print; next} {p=0}' "$out"
  fails=""
  grep -qE "^\[wp\] Proved goals: +100 / 100( |$)" "$out" || fails="${fails} not-100/100"
  grep -qE "^  Coq [0-9.]+: +12( |$)" "$out" || fails="${fails} Coq-not-12"
  if grep -E "^\[wp\] \[(Timeout|Unknown|Failed|Failure|Invalid|Stepout)\]" "$out"; then
    fails="${fails} unproved"
  fi
  for f in "$SESSION"/interactive/*.v; do
    if ! cmp -s "$f" "$OUT/session_$name/interactive/$(basename "$f")"; then
      echo "   $(basename "$f"): changed by WP"
      fails="${fails} script-changed"
    fi
  done
  if [ -z "$fails" ]; then
    echo "   PASS"
  else
    echo "   FAIL:${fails}"
    BADMODELS=$((BADMODELS + 1))
  fi
done

echo "replay.sh: ${#GOALS[@]} scripts, ${BADSCRIPTS} not passing;" \
  "${#MODELS[@]} models, ${BADMODELS} not passing"
exit $((BADSCRIPTS + BADMODELS))
