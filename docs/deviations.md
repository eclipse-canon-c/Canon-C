# Canon-C Deviations

This document catalogs deliberate deviations from automatic verification
and coverage targets. Each entry is a concrete, justified departure from
"tool says valid" — recorded so that certification reviewers, downstream
adopters, and future maintainers can trace the reasoning.

Entries fall into three categories:

1. **VERIFY-nnn** — Formal verification deviations (Frama-C WP goals
   that require manual justification).
2. **MCDC-nnn** — MC/DC coverage deviations (conditions that cannot be
   exercised at runtime despite being reachable in the ACSL-proven
   contract model).
3. **MISRA-CFG-nnn** — MISRA C:2012 configuration deviations (project-
   wide decisions that systematically affect how MISRA rules apply).

Every entry documents: what is deviated, why it is deviated, what
compensating evidence exists, and how a reviewer can verify the
compensating evidence without re-running the full tool chain.

---

## VERIFY-001 — checked.h: 2 manually discharged 64-bit overflow goals

**Scope.** `core/primitives/checked.h`
**Tool.** Frama-C 29.0 + WP, Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1
**Goals.**
- `typed_checked_add_overflow_ensures`
- `typed_checked_add_u64_overflow_ensures`

**Status.** 1539 of 1541 goals discharged automatically (99.87%). The
2 remaining goals are manually discharged.

**Nature of the deviation.** Both goals are ensures clauses on the
overflow branch of 64-bit unsigned addition fallback paths. They
assert that when `a + b` wraps around, the function returns `false`.
The property holds mathematically — unsigned wraparound is well-defined
in C99 §6.2.5¶9 — but WP's integer memory model requires the SMT
solver to reason about modular arithmetic across the fallback code's
comparison-after-addition pattern (`*result = a + b; return *result >= a;`).
None of Alt-Ergo 2.6.3, Z3 4.15.2, or CVC5 1.2.1 close these two
goals within the 120-second timeout.

**Manual proof argument.** For unsigned 64-bit `a, b`:

Let `S = a + b mod 2^64`. If `a + b` fits in u64 (no overflow), then
`S = a + b >= a` (since `b >= 0`), so the function returns `true` with
`*result = a + b`. If `a + b >= 2^64` (overflow), then `S = a + b - 2^64`.
Because `b < 2^64`, we have `a + b - 2^64 < a`, so `S < a`, so the
function returns `false`. The ensures clause on the overflow branch
therefore holds by the definition of unsigned wraparound.

**Compensating evidence.**
- Exhaustive test: `test/core/primitives/checked_test.c` covers
  boundary cases including `UINT64_MAX + 1`, `UINT64_MAX + UINT64_MAX`,
  zero-boundary, and random sampling.
- MC/DC coverage: 100% (64/64) on checked.h — every condition in the
  fallback path is exercised with independence.
- Fuzz testing: `test/fuzz/checked_fuzz.c` runs 4 hours nightly with
  libFuzzer; no divergence between `checked_*` and `__builtin_*_overflow`
  results to date.

**How to verify.** Run the CI `frama-c` job; look for the
`[wp] [Timeout]` lines naming the two goals above. Run the CI
`coverage` job; verify `checked.h: 100.0% (64/64)` in the MC/DC
summary table. Re-read the manual argument above against the fallback
source at `core/primitives/checked.h:205-213` and `:300-308`.

---

## VERIFY-002 — checked.h: MC/DC NULL-pointer branches

**Scope.** `core/primitives/checked.h`
**Goals.** Structurally unreachable NULL-pointer branches in
`CHECKED_ASSERT_RESULT(result)`.

**Nature of the deviation.** Each `checked_*` function begins with
`CHECKED_ASSERT_RESULT(result)`, which in debug builds expands to a
runtime NULL check on the output pointer. Under the ACSL contract,
every caller has already proven `\valid(result)` statically — the
NULL branch is therefore structurally unreachable in any call site
that passes WP verification.

**Resolution.** The coverage build defines `CANON_NO_REQUIRE`, which
compiles `CHECKED_ASSERT_RESULT` to `((void)0)`. This removes the
unreachable branches from the preprocessed source so that MC/DC
measurement reflects real branches only. The same flag is passed to
the frama-c job to keep WP and coverage evidence streams aligned.

**Compensating evidence.**
- `test/core/primitives/contract_test.c` verifies that
  `require_msg()` (and therefore `CHECKED_ASSERT_RESULT` under
  `!NDEBUG`) actually fires when called with NULL.
- The contract_test is excluded from the coverage job (`CANON_NO_REQUIRE`
  disables it), but runs in every other CI job where assertions are live.

**How to verify.** Check that `.github/workflows/ci.yml` defines
`CANON_NO_REQUIRE` in both `coverage` and `frama-c` jobs. Run
`contract_test` locally with assertions enabled; observe it fires.
Review `CHECKED_ASSERT_RESULT` definition at `core/primitives/checked.h:141-155`.

---

## VERIFY-003 — bits.h: Bitwise complement and SWAR ensures clauses

**Scope.** `core/primitives/bits.h`
**Tool.** Frama-C 29.0 + WP, Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1
**Goals (4 of 15 in VERIFY-003/004 combined).**
- `typed_bits_clear_ensures`
- `typed_bits_insert_full_width_ensures_part2`
- `typed_bits_insert_normal_ensures_part3`
- `typed_bits_popcount_ensures`

**Nature of the deviation.** These four goals require WP's integer
theory to reason across bitwise complement (`~`) or SWAR popcount
magic constants (`0x5555...`, `0x3333...`, `0x0f0f...`). WP represents
bitwise operators as uninterpreted functions by default; relating
their results to integer-theory arithmetic (required by the ensures
formula) needs specialized decision procedures that none of the three
configured SMT provers handle in 120 seconds.

**Manual proof argument.** All four properties are textbook. For
example, `bits_popcount(x)` equals the number of set bits in x — a
property that follows by induction over the SWAR sequence (see
Hacker's Delight Chapter 5). Each step halves the bit-group width
while preserving the total count. The final reduction to 8 bits
followed by `(x * 0x0101...) >> 56` extracts the sum.

**Compensating evidence.**
- 100% MC/DC (52/52) on bits.h, measured with
  `-DCANON_BITS_FORCE_FALLBACK`.
- Exhaustive test vectors for each function in
  `test/core/primitives/bits_test.c` covering zero, all-ones, single-bit,
  powers of two, and boundary values.
- Cross-check: each fallback result is compared against the GCC builtin
  (`__builtin_popcountll`, etc.) in the test harness.

**How to verify.** Review the four fallback implementations against
standard references. MC/DC evidence in CI `coverage` artifact.

---

## VERIFY-004 — bits.h: Rotation, power-of-two, and byte-swap ensures

**Scope.** `core/primitives/bits.h`
**Tool.** Frama-C 29.0 + WP, Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1
**Goals (11 of 15 in VERIFY-003/004 combined).**
- `typed_bits_rotl_assert_rte_shift_2`
- `typed_bits_rotl_nonzero_shift_ensures_part2`
- `typed_bits_rotr_assert_rte_shift_2`
- `typed_bits_rotr_nonzero_shift_ensures_part2`
- `typed_bits_next_power_of_two_normal_ensures_part3`
- `typed_bits_next_power_of_two_normal_ensures_2_part3`
- `typed_bits_next_power_of_two_normal_ensures_3_part3`
- `typed_bits_next_power_of_two_normal_ensures_4_part3`
- `typed_bits_bswap16_assert_rte_signed_overflow`
- `typed_bits_bswap32_ensures`
- `typed_bits_bswap64_ensures`

**Nature of the deviation.** Rotation goals combine a shift-count RTE
check with a bitwise OR in the ensures clause. Next-power-of-two goals
specify minimality through a bit-smearing algorithm. Byte-swap goals
relate a sequence of shifts and masks to the reversed-byte value.
All share the same root cause as VERIFY-003: WP's integer theory
cannot bridge bitwise operations and arithmetic equality within the
configured solver budgets.

**Manual proof argument.** Each follows a textbook construction
(Hacker's Delight Chapter 3 for rotation, Chapter 7 for power-of-two
and byte-swap). The ACSL contracts carry the formula; the manual
argument establishes that the C source implements that formula
exactly.

**Compensating evidence.** Same as VERIFY-003: 100% MC/DC on bits.h,
exhaustive test vectors, cross-check against builtins.

---

## VERIFY-005 — compare.h: Typed+Cast memory model required

**Scope.** `core/primitives/compare.h`
**Tool.** Frama-C 29.0 + WP, Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1
**Goals.** 208 of 208 proved (100%) — this entry documents a
**model configuration** deviation, not an unproved-goal deviation.

**Nature of the deviation.** compare.h's 28 comparator functions take
`const void*` parameters and cast them to typed pointers inside the
function body (e.g. `const u32* pa = (const u32*)a;`). WP's default
`Typed` memory model treats `void*` as `sint8*` (`char*`) and emits
warnings — and in some cases refuses to prove memory-access RTE
goals — when casting to `uint32*` or other incompatible pointer types.

**Resolution.** The compare.h WP step runs with `-wp-model Typed+Cast`,
which allows pointer casts between types of compatible size. This is
sound for compare.h because: (1) every cast target type has the same
size as the object the caller actually supplied (the caller contract
is `p points to a valid T`), and (2) the function reads bytes in
native alignment, not with unaligned accesses.

**Soundness argument.** ISO C99 §6.3.2.3¶7 permits pointer conversions
between object types provided the result is correctly aligned for the
target type. Canon-C comparator callers are responsible for passing
correctly-aligned pointers — this is an explicit precondition in every
comparator's @pre clause. WP's Typed+Cast model exactly captures this
discipline: casts are allowed but alignment is not assumed, so misuse
would still generate a memory-access RTE at the call site.

**Compensating evidence.**
- 208/208 goals proved with Typed+Cast (no deviations beyond the model
  choice itself).
- 100% MC/DC (8/8) on compare.h.
- Cross-check testing in `test/core/primitives/compare_test.c`.
- No callers known to pass misaligned pointers in the Canon-C codebase;
  slice.h, vec.h, and pool.h allocators all produce aligned storage.

**How to verify.** The CI `frama-c` job logs show `[wp] Proved goals:
208 / 208` on compare.h. Review the `-wp-model Typed+Cast` argument
in `.github/workflows/ci.yml`. Read the alignment preconditions on
each comparator's `@pre` clause in `core/primitives/compare.h`.

---

## VERIFY-006 — ptr.h: 10 manually discharged pointer/align/handler goals

**Scope.** `core/primitives/ptr.h` (with `core/primitives/contract.h`
contract handler annotations)
**Tool.** Frama-C 29.0 + WP with `-wp-model Typed+Cast`, Alt-Ergo 2.6.3
+ Z3 4.15.2 + CVC5 1.2.1
**Status.** 1729 of 1739 goals discharged automatically (99.43%). The
10 remaining goals are manually discharged.

**Nature of the deviation.** The 10 unproved goals are demonstrated
**triple-prover-resistant**: no combination of Alt-Ergo 2.6.3,
Z3 4.15.2, and CVC5 1.2.1 closes them within 120 seconds. This is a
stronger certification statement than dual-prover resistance; the
goals are genuinely limited by WP's encoding and integer theory, not
by prover strength. They fall into four categories:

### Category 1: Transitive checked.h overflow (2 goals)

- `typed_cast_checked_add_overflow_ensures`
- `typed_cast_checked_add_u64_overflow_ensures`

These are the same 2 goals documented in VERIFY-001, re-emitted in the
ptr.h proof run because ptr.h includes checked.h. The manual argument
from VERIFY-001 applies verbatim; no new analysis is needed here.

### Category 2: Align formula ensures clauses (3 goals)

- `typed_cast_align_up_ensures`
- `typed_cast_align_down_ensures`
- `typed_cast_align_padding_ensures`

The ensures clauses specify the exact formula: for example,
`align_up(n, a) == ((n + a - 1) & ~(a - 1))`. WP's integer theory
cannot reason across the bitwise AND with complement (`~(a - 1)`)
relative to arithmetic multiples-of-a equality. The manual argument:
when `a` is a power of two, `~(a - 1)` has exactly the bits at
positions >= log2(a) set, so ANDing with it clears the low log2(a)
bits — which is the arithmetic definition of rounding down to a
multiple of a. Hacker's Delight §3-1 records the construction
formally.

### Category 3: ptr_align_* call-chain preconditions (3 goals)

- `typed_cast_ptr_align_up_call_align_up_requires_3`
- `typed_cast_ptr_align_padding_call_align_padding_requires_3`
- `typed_cast_ptr_align_padding_nonnull_ensures_part2`

These arise when ptr.h call sites reconstruct align_up / align_padding
preconditions through pointer-to-integer casts. Requires clause 3 on
align_up is `n <= CANON_USIZE_MAX - (align - 1)`, and the caller has
cast `(uintptr_t)p` to `usize`. WP under Typed+Cast cannot prove that
the uintptr_t round-trip preserves the bound — a model-limitation
consequence of allowing pointer casts at all. The manual argument:
ISO C99 §6.3.2.3 guarantees that `(uintptr_t)p` produces a value
representable in usize (since `sizeof(uintptr_t) == sizeof(usize)` on
every platform Canon-C supports), and the bound follows from the
flat-address-space assumption documented in ptr.h's header comment.

The `ptr_align_padding_nonnull_ensures_part2` goal is a transitive
consequence: it asks that the non-null return value is < align, which
again requires reasoning across the pointer-integer round-trip.

### Category 4: Contract handler non-termination (2 goals)

- `typed_cast_contract_default_handler_loop_invariant_established`
- `typed_cast_contract_default_handler_terminates`

Under the `__FRAMAC__` preprocessor guard, `contract.h` replaces the
body of `contract_default_handler` with `while(1) {}` — the standard
ACSL idiom for a non-returning function. The ACSL contract declares
`ensures \false` and `exits \false`. WP correctly recognizes that the
function does not terminate (the `terminates` goal is unprovable, by
construction) and cannot establish a loop invariant at entry to a
loop that executes zero or more iterations toward `\false` (the
`loop_invariant_established` goal is also unprovable, by construction).

This is **not a code defect**. The two unproved goals are the
mathematical statement of the handler's non-returning contract. Any
caller that invokes the handler relies on this exact non-termination
semantics (the handler is expected to abort, panic, or enter an
infinite diagnostic loop). The ACSL contract is intentional and
correct.

**Compensating evidence.**
- 100% MC/DC (42/42) on ptr.h.
- Exhaustive alignment test vectors in `test/core/primitives/ptr_test.c`
  covering all `align_*` and `ptr_align_*` functions with representative
  alignment values (1, 2, 4, 8, 16, 4096) and boundary inputs.
- Call-chain test coverage: every ptr.h function that transitively
  calls align_up, align_padding, or checked_* is exercised in tests.
- Contract handler behavior is tested by the `contract_test` suite
  under `!NDEBUG`; under `__FRAMAC__` the `while(1)` body is unreachable
  at runtime (testing happens in non-Frama-C builds).

**How to verify.**
- CI `frama-c` job: look for `[wp] Proved goals: 1729 / 1739` and the
  10 `[Timeout]` lines naming the goals above.
- CI `coverage` job: verify `ptr.h: 100.0% (42/42)` in the MC/DC
  summary.
- Read the manual proof arguments above against the source at:
  - `core/primitives/ptr.h:322-408` (align_up, align_down, align_padding)
  - `core/primitives/ptr.h:445-585` (ptr_align_*)
  - `core/primitives/contract.h` (contract_default_handler, under
    `__FRAMAC__` guard)

---

## MCDC-001 — contract.h: Unreachable assertion branches

**Scope.** `core/primitives/contract.h`
**Status.** 0 of 2 conditions hit (0% MC/DC) in the coverage build.

**Nature of the deviation.** contract.h contains the NULL-check branch
of `require_msg()`. Under `CANON_NO_REQUIRE` (which the coverage build
defines), the macro expands to `((void)0)` and the branch is removed
from the preprocessed source entirely. gcov still reports the 2
conditions as "missed" because the source file contains them, even
though the preprocessed translation unit does not.

**Resolution.** This is a cosmetic artifact of gcov's source-file
view vs preprocessed-translation-unit view. The same code is exercised
100% by `contract_test` in other CI jobs (where `CANON_NO_REQUIRE` is
not defined). No action required beyond documenting it here.

**How to verify.** Compare `contract.h` coverage in CI `coverage`
artifact against `contract_test` execution in the `build` job.

---

## MISRA-CFG-001 — Cppcheck MISRA addon scope

**Scope.** `.github/workflows/ci.yml`, `misra` job
**Status.** Advisory reporting only.

**Nature of the deviation.** The CI `misra` job uses Cppcheck's MISRA
C:2012 addon, which covers approximately 60-70% of MISRA rules. The
job is configured as advisory (does not fail the build) because:

1. Qualified MISRA compliance requires a qualified tool (Polyspace,
   LDRA, PC-lint, Parasoft C/C++test). Cppcheck's addon is not
   qualified for certification.
2. Macro-templated implementation headers (e.g. `hashmap_impl.h`,
   `deque_impl.h`) emit `misra-config` errors because Cppcheck cannot
   resolve macro-instantiated type names without instantiation
   context. These are tool limitations, not code defects.

**Resolution plan.** When Canon-C moves toward formal certification,
the MISRA job will be replaced with a qualified tool run. The
deviations catalog will be restructured at that point to track
category-by-category compliance (Mandatory / Required / Advisory)
against the qualified tool's output.

**How to verify.** Review the `misra` job configuration in
`.github/workflows/ci.yml`. Check the suppressions list against the
`*_impl.h` pattern.

---

*Last updated: 2026-04-23, CI #795 (commit debb202).*
*Triple-prover baseline: Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1.*
*Combined proof statistics: 4222/4249 automatic (99.36%), 27 documented deviations.*
