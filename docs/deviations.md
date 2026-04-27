# Deviations Record

## Overview

This document records all deviations from full compliance in Canon-C's
verification, coverage, and MISRA analysis. Each deviation has a unique
ID, rationale, and mitigation strategy.

---

## VERIFY-001: Compiler Intrinsic Path Not Verified

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-001 |
| **Date**       | 2026-04-17 (revised on div/mod addition) |
| **Scope**      | checked.h (add/sub/mul only), bits.h |
| **Category**   | Formal verification scope |

**Description**: When `__GNUC__` or `__clang__` is defined, the
overflow-detecting arithmetic functions in checked.h use
`__builtin_*_overflow` intrinsics, and bits.h uses
`__builtin_popcountll`, `__builtin_clzll`, `__builtin_ctzll`, and
`__builtin_bswap*`. These paths are not verified by Frama-C WP because
WP has no semantics for compiler builtins. The `__FRAMAC__` preprocessor
guard forces the fallback path during verification.

**Scope clarification (added when div/mod functions were introduced):**
This deviation applies only to the addition, subtraction, and
multiplication functions in checked.h. The division and modulo
functions (`checked_div`, `checked_div_u8/u16/u32/u64`,
`checked_div_isize`, `checked_mod`, `checked_mod_u8/u16/u32/u64`,
`checked_mod_isize`) have no compiler builtin equivalent —
`__builtin_div_overflow` does not exist in any compiler — and are
implemented directly in C under all build configurations. The verified
code is the executed code for these functions; no `__FRAMAC__` workaround
is needed. Likewise, compare.h and ptr.h have no compiler builtins and
are verified directly.

**Rationale**: For functions where this deviation does apply, the
builtin path is semantically equivalent to the fallback path — both
implement the same mathematical operation. GCC and Clang's builtins
are extensively tested by compiler test suites and used in millions
of production codebases. The fallback path is the one that needs
verification because it contains hand-written arithmetic that could
have subtle bugs.

**Mitigation**: The fallback path is fully verified by WP. The builtin
path is tested by the same test suite (100% MC/DC on both checked.h
and bits.h) and validated by sanitizers (ASan, UBSan) in Debug builds.
The CI coverage job uses `-DCANON_CHECKED_FORCE_FALLBACK` and
`-DCANON_BITS_FORCE_FALLBACK` to measure the fallback path, keeping
the coverage and verification evidence streams aligned.

---

## VERIFY-002: Manually Discharged Goals (checked.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-002 |
| **Date**       | 2026-04-17 |
| **Scope**      | checked.h — `checked_add`, `checked_add_u64` |
| **Category**   | Formal verification completeness |

**Description**: Two proof obligations are not discharged by any
automated prover:
1. `typed_checked_add_overflow_ensures`
2. `typed_checked_add_u64_overflow_ensures`

Both assert that 64-bit unsigned addition wraparound is detected
correctly by the `*result >= a` check.

**Rationale**: WP's integer memory model requires modular-arithmetic
reasoning (`(a + b) mod 2^64`) that current SMT solvers cannot
perform. The two goals are demonstrated triple-prover-resistant —
Alt-Ergo 2.6.3, Z3 4.15.2, and CVC5 1.2.1 all time out at 120s. They
are discharged by a manual proof recorded in docs/verification.md.

**Mitigation**: Manual proof by modular-arithmetic argument (see
verification.md, "Manually discharged goals"). CI enforces that
exactly these two goals time out and no others — any additional
timeout is a regression. This invariant continues to hold after the
addition of the division and modulo functions: the new functions
introduced 214 proof obligations, all auto-discharged, with no new
timeouts.

---

## VERIFY-003: WP Timeout Goals (bits.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-003 |
| **Date**       | 2026-04-17 |
| **Scope**      | bits.h — 15 goals across 9 functions |
| **Category**   | Formal verification completeness |

**Description**: 15 of 761 proof obligations (2.0%) time out or return
Unknown under WP with Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1 at
120-second timeout. All are WP model limitations on bitwise reasoning,
not code defects. The unproved goals fall into five categories:

1. **Bitwise complement** (3): `bits_clear`, `bits_insert` — WP
   cannot connect C's `~x` operator to the XOR-based ACSL spec.
2. **SWAR popcount** (1): `bits_popcount` — parallel bit-counting
   with magic constants exceeds SMT bitvector reasoning.
3. **Rotation** (4): `bits_rotl`, `bits_rotr` — RTE shift checks
   and bitwise OR in spec.
4. **Minimality** (4): `bits_next_power_of_two` — the
   `result / 2 < value` property through bit-smearing.
5. **Byte swap** (3): `bits_bswap16` signed overflow from u16→int
   promotion, `bits_bswap32`/`bits_bswap64` multi-term OR specs.

Note: Some goals may appear as `[Unknown]` instead of `[Timeout]`
across runs — WP solver heuristics are nondeterministic. The CI
enforcement counts both combined.

Full goal list: see docs/verification.md, bits.h section.

**Rationale**: These are fundamental limitations of WP's integer
theory and current SMT solvers' bitwise reasoning capabilities, not
weaknesses in the code. The triple-prover configuration confirms this
is not a prover-strength issue — CVC5's bitvector reasoning was
specifically expected to help here, yet it closes none of the 15
goals.

**Mitigation**: CI enforces exactly 15 unproved goals on the named
goals. Any additional unproved goal is a regression and fails the
build. All 18 functions have 100% MC/DC coverage (52/52 condition
outcomes) and pass fuzzing.

---

## VERIFY-004: Weakened Specs (bits.h — CLZ, CTZ, popcount)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-004 |
| **Date**       | 2026-04-17 |
| **Scope**      | bits.h — `bits_clz`, `bits_ctz`, `bits_popcount` |
| **Category**   | Formal verification spec strength |

**Description**: Three functions have ACSL contracts weaker than
the full functional specification:

- `bits_clz`: Spec proves range (0–63 for nonzero, 64 for zero)
  but not the mathematical bound
  `value >= 2^(63-result) && value < 2^(64-result)`.
- `bits_ctz`: Spec proves range (0–63 for nonzero, 64 for zero)
  but not `(value >> result) & 1 == 1`.
- `bits_popcount`: Spec proves range (0–64) but not
  `result == number of 1-bits in value`.

**Rationale**: The binary search CLZ/CTZ generates one sub-goal per
possible return value (0–63), each requiring WP to trace the
cascading mask-and-shift logic through 6 conditional branches. The
SWAR popcount uses magic constants and multiplication that have no
axiomatic representation in WP's theory.

**Mitigation**: The weak specs still prove absence of runtime errors
and correct range bounds. Full functional correctness is verified by
testing (100% MC/DC, 100% line coverage) and fuzzing. The `bits_ffs`
and `bits_fls` functions inherit the same range-only specs.

---

## VERIFY-005: WP Memory Model Override (compare.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-005 |
| **Date**       | 2026-04-18 |
| **Scope**      | compare.h — all 28 comparator functions |
| **Category**   | Formal verification configuration |

**Description**: compare.h is verified with `-wp-model Typed+Cast`
instead of WP's default `Typed` memory model. This is required because
every comparator takes `const void*` parameters and casts them to typed
pointers inside the function body (e.g. `*(const u32*)a`). With the
default `Typed` model, WP treats `void*` as `char*` (sint8*) and all
RTE mem_access goals become unprovable due to incompatible pointer cast
warnings.

**Rationale**: `Typed+Cast` is a standard Frama-C WP model designed
for exactly this use case — C generic interfaces that pass data through
`void*`. The model is sound under the assumption that callers pass
correctly-typed pointers, which is guaranteed by the comparator API
contract (each comparator documents the expected pointer type). This
is the same pattern used by `qsort`, `bsearch`, and every C standard
library generic interface.

**Mitigation**: compare.h achieves 208/208 proved goals (100%) with
`Typed+Cast`. The flag is applied only to compare.h and ptr.h (see
VERIFY-006) — checked.h and bits.h use the default `Typed` model.
The difference is documented in the CI YAML and in this deviations
record. All 28 comparators have 100% MC/DC coverage (8/8 condition
outcomes) and pass fuzzing.

---

## VERIFY-006: Manually Discharged Goals (ptr.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-006 |
| **Date**       | 2026-04-23 |
| **Scope**      | ptr.h — 10 goals across 4 categories |
| **Category**   | Formal verification completeness |

**Description**: 10 of 1739 proof obligations (0.57%) are not
discharged by any prover in the triple-prover configuration (Alt-Ergo
2.6.3 + Z3 4.15.2 + CVC5 1.2.1) with a 120-second timeout and
`-wp-model Typed+Cast`. All 10 are triple-prover-resistant. The goals
fall into four categories:

1. **Transitive checked.h overflow** (2): `typed_cast_checked_add_overflow_ensures`,
   `typed_cast_checked_add_u64_overflow_ensures` — same goals as
   VERIFY-002, re-emitted in the ptr.h proof run because ptr.h includes
   checked.h.
2. **Align formula ensures** (3): `typed_cast_align_up_ensures`,
   `typed_cast_align_down_ensures`, `typed_cast_align_padding_ensures`
   — WP integer theory cannot bridge bitwise AND with complement
   (`~(a - 1)`) and arithmetic multiples-of-a equality.
3. **ptr_align_* call-chain preconditions** (3):
   `typed_cast_ptr_align_up_call_align_up_requires_3`,
   `typed_cast_ptr_align_padding_call_align_padding_requires_3`,
   `typed_cast_ptr_align_padding_nonnull_ensures_part2` — call sites
   reconstruct `align_up` / `align_padding` preconditions through
   `(uintptr_t)p` casts; WP under Typed+Cast cannot prove the
   uintptr_t round-trip preserves integer bounds.
4. **Contract handler non-termination** (2):
   `typed_cast_contract_default_handler_loop_invariant_established`,
   `typed_cast_contract_default_handler_terminates` — under `__FRAMAC__`,
   `contract.h` replaces the handler body with `while(1) {}` carrying
   `ensures \false` + `exits \false`. These goals are intended-unprovable:
   they are the mathematical statement of the handler's non-returning
   contract.

Full goal list and per-category manual proof arguments: see
docs/verification.md, ptr.h section.

**Rationale**: Categories 1–3 are WP integer-theory and memory-model
limitations — the same class of limitation as VERIFY-002 (modular
arithmetic) and VERIFY-003 (bitwise/arithmetic bridging). Category 4
is the deliberate ACSL idiom for non-returning functions; the
"unproved" status is the intended expression of the contract. The
triple-prover configuration strengthens the evidence: CVC5's bitvector
and modular-arithmetic reasoning was specifically expected to help on
categories 1–3, yet closes none of them.

**Mitigation**: CI enforces exactly 10 unproved goals with the named
goal list. Any additional unproved goal or missing expected goal is a
regression and fails the build. ptr.h achieves 100% MC/DC coverage
(42/42 condition outcomes). Exhaustive alignment test vectors in
`test/core/primitives/ptr_test.c` cover all `align_*` and `ptr_align_*`
functions with representative alignments (1, 2, 4, 8, 16, 4096) and
boundary inputs. Contract handler behavior is tested by `contract_test`
under `!NDEBUG`.

---

## MCDC-001: Coverage Flags Methodology

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-001 |
| **Date**       | 2026-04-14 |
| **Scope**      | Coverage CI job |
| **Category**   | Coverage measurement methodology |

**Description**: The coverage CI job uses three preprocessor flags
that change the code under measurement:
- `-DCANON_CHECKED_FORCE_FALLBACK`: Forces checked.h fallback path
- `-DCANON_BITS_FORCE_FALLBACK`: Forces bits.h fallback path
- `-DCANON_NO_REQUIRE`: Removes `require_msg()` NULL checks

These flags reduce the branch/MC/DC denominator by removing
structurally unreachable branches.

**Rationale**: The flags align the coverage measurement with the
formal verification scope. WP proves the fallback path; coverage
measures the fallback path. CANON_CHECKED_FORCE_FALLBACK has no effect
on the new checked.h division and modulo functions because those
functions have no builtin path to suppress — they are always-direct
implementations. The flag remains active for the add/sub/mul
functions where the builtin/fallback distinction still applies.

**Mitigation**: The flags are documented in the CI YAML, in
traceability.md, and here. The `contract_test` binary is excluded
from the coverage build but runs in all other CI jobs.

---

## MISRA-CFG-001: Cppcheck MISRA Configuration Limitation

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-CFG-001 |
| **Date**       | 2026-04-07 |
| **Scope**      | MISRA CI job — `*_impl.h` headers |
| **Category**   | MISRA analysis tool limitation |

**Description**: Cppcheck's MISRA addon emits `[misra-config]` errors
on macro-templated implementation headers because it cannot resolve
macro-instantiated type names without an instantiation context.

**Rationale**: This is a tool limitation, not a code defect. Qualified
MISRA checkers handle this correctly.

**Mitigation**: The `--suppress=misra-config:*_impl.h` flag suppresses
these false positives. The MISRA CI job is advisory — it does not fail
the build.
