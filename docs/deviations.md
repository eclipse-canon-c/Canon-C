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
| **Date**       | 2026-04-17 |
| **Scope**      | checked.h, bits.h |
| **Category**   | Formal verification scope |

**Description**: When `__GNUC__` or `__clang__` is defined, checked.h
uses `__builtin_*_overflow` intrinsics and bits.h uses
`__builtin_popcountll`, `__builtin_clzll`, `__builtin_ctzll`, and
`__builtin_bswap*`. These paths are not verified by Frama-C WP because
WP has no semantics for compiler builtins. The `__FRAMAC__` preprocessor
guard forces the fallback path during verification.

**Rationale**: The builtin path is semantically equivalent to the
fallback path — both implement the same mathematical operation. GCC and
Clang's builtins are extensively tested by compiler test suites and
used in millions of production codebases. The fallback path is the one
that needs verification because it contains hand-written arithmetic
that could have subtle bugs.

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
perform. The goals are discharged by a manual proof recorded in
docs/verification.md.

**Mitigation**: Manual proof by modular-arithmetic argument (see
verification.md, "Manually discharged goals"). CI enforces that
exactly these two goals time out and no others — any additional
timeout is a regression.

---

## VERIFY-003: WP Timeout Goals (bits.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-003 |
| **Date**       | 2026-04-17 |
| **Scope**      | bits.h — 15 goals across 9 functions |
| **Category**   | Formal verification completeness |

**Description**: 15 of 761 proof obligations (2.0%) time out under
WP with Alt-Ergo 2.6.2 + Z3 4.15.2 at 120-second timeout. All are
WP model limitations on bitwise reasoning, not code defects. The
timeout goals fall into five categories:

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

Full goal list: see docs/verification.md, bits.h section.

**Rationale**: These are fundamental limitations of WP's integer
theory and current SMT solvers' bitwise reasoning capabilities, not
weaknesses in the code. The same algorithms are standard in
production codebases (glibc, LLVM compiler-rt, Linux kernel).

**Mitigation**: CI enforces exactly 15 timeouts on the named goals.
Any additional timeout is a regression and fails the build. All 18
functions have 100% MC/DC coverage (52/52 condition outcomes) and
pass fuzzing. The full WP output is uploaded as the `wp-proof-bits`
CI artifact for auditor inspection.

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

The strong specs generate 64+ sub-goals per ensures clause through
the binary search (CLZ/CTZ) and SWAR (popcount) algorithms. None
close within 120 seconds on any available prover.

**Rationale**: The binary search CLZ/CTZ generates one sub-goal per
possible return value (0–63), each requiring WP to trace the
cascading mask-and-shift logic through 6 conditional branches. The
SWAR popcount uses magic constants and multiplication that have no
axiomatic representation in WP's theory. Weakening the specs to
provable properties is the standard approach in Frama-C practice
for complex bitwise algorithms.

**Mitigation**: The weak specs still prove absence of runtime errors
and correct range bounds — useful safety properties. Full functional
correctness is verified by testing (100% MC/DC, 100% line coverage)
and fuzzing. The `bits_ffs` and `bits_fls` functions, which delegate
to `bits_ctz` and `bits_clz` respectively, inherit the same range-only
specs and the same mitigation applies.

**Future work**: Replace the binary search CLZ/CTZ fallbacks with
simple loop implementations that WP can reason about with loop
invariants. This would enable strong functional specs at the cost
of slower fallback performance. The builtin path (used in production)
is unaffected.

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
structurally unreachable branches (builtin paths under GCC,
proved-unreachable NULL checks).

**Rationale**: The flags align the coverage measurement with the
formal verification scope. WP proves the fallback path; coverage
measures the fallback path. Without the flags, GCC builtins expand
to single instructions with no branches, making the fallback blocks
appear as dead code. The NULL-check branches are proved unreachable
by ACSL `requires \valid(result)` clauses.

**Mitigation**: The flags are documented in the CI YAML, in
traceability.md (methodology changes section), and here. The
`contract_test` binary is excluded from the coverage build but runs
in all other CI jobs. Both the builtin and fallback paths are tested
by the same test suite in the regular build jobs.

---

## MISRA-CFG-001: Cppcheck MISRA Configuration Limitation

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-CFG-001 |
| **Date**       | 2026-04-07 |
| **Scope**      | MISRA CI job — `*_impl.h` headers |
| **Category**   | MISRA analysis tool limitation |

**Description**: Cppcheck's MISRA addon emits `[misra-config]` errors
on macro-templated implementation headers (`hashmap_impl.h`,
`deque_impl.h`, etc.) because it cannot resolve macro-instantiated
type names (e.g. `HASHMAP_SLOT_NAME`) without an instantiation context.

**Rationale**: This is a tool limitation, not a code defect. Qualified
MISRA checkers (Polyspace, LDRA, PC-lint, Parasoft) handle
macro-instantiated types correctly via full preprocessor-aware analysis.
Cppcheck's free MISRA addon covers approximately 60–70% of MISRA C:2012
rules and is used as a preliminary compliance indicator, not as a
qualified checker.

**Mitigation**: The `--suppress=misra-config:*_impl.h` flag suppresses
these false positives. For certification, replace Cppcheck's MISRA addon
with a qualified tool. The MISRA CI job is advisory — it does not fail
the build.
