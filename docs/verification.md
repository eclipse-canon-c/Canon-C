# Verification Record

## Overview

This document records the formal verification status of Canon-C library
headers. Verification is performed using Frama-C WP (Weakest Precondition)
with ACSL contracts, enforced in CI on every push to master.

---

## core/primitives/checked.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified                                        |
| **Functions**          | 18 of 18 annotated and proved                  |
| **Proof obligations**  | 1539 / 1541 discharged automatically            |
| **Manually discharged**| 2 (see below)                                   |
| **Prover setup**       | Alt-Ergo 2.6.2 + Z3 4.15.2                     |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120`         |
| **CI enforcement**     | Yes — any drift from expected results fails CI  |
| **MC/DC coverage**     | 100% (64/64 condition outcomes)                 |
| **Line coverage**      | 100% (81/81)                                    |
| **Function coverage**  | 100% (18/18)                                    |
| **CI artifact**        | `wp-proof-output` (full per-goal breakdown)     |

### What is proved

- **Functional correctness**: Every function's ACSL contract (requires,
  ensures, assigns, behaviors) is verified for all possible inputs
  consistent with the preconditions. Behaviors are marked `complete`
  and `disjoint`, ensuring the specification is exhaustive and
  non-overlapping.

- **Absence of runtime errors** (`-wp-rte`): WP proves that no
  execution of any function can trigger:
  - Signed integer overflow
  - Division by zero
  - Invalid shift operations
  - Null pointer dereference
  - Out-of-bounds memory access

- **Side-effect bounding**: Every function specifies `assigns *result;`,
  proving that no memory other than `*result` is modified.

### Prover breakdown

| Prover         | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Qed (internal) | 1413             | 1ms–13ms          |
| Alt-Ergo 2.6.2 | 76               | 14ms–47ms         |
| Z3 4.15.2      | 14               | 15ms–62ms         |
| Timeout        | 2                | >120s (see below) |
| **Total**      | **1539 / 1541**  |                    |

Alt-Ergo handles linear arithmetic and most contracts. Z3 handles
nonlinear integer reasoning (multiplication contracts in
`checked_mul`, `checked_mul_u64`, `checked_mul_isize`). WP tries
Alt-Ergo first and falls through to Z3 only on goals Alt-Ergo
cannot close.

### Manually discharged goals

Two proof obligations are not discharged by any automated prover:

1. **`typed_checked_add_overflow_ensures`**
2. **`typed_checked_add_u64_overflow_ensures`**

Both assert that when `a > USIZE_MAX - b` (or `a > 0xFFFFFFFFFFFFFFFF - b`),
the function returns `false`. The fallback implementation computes
`*result = a + b` (unsigned wraparound, well-defined in C) and returns
`*result >= a`, which is `false` when overflow occurred.

**Why provers fail**: WP formulates this in its integer memory model where
unsigned wraparound requires modular-arithmetic reasoning. Current SMT
solvers (Alt-Ergo, Z3, CVC5) cannot close this goal because the
relationship between the mathematical sum and the wrapped result requires
reasoning about `(a + b) mod 2^64`, which is outside their default
integer theory.

**Manual proof**: When `a > USIZE_MAX - b`, the mathematical sum
`a + b > USIZE_MAX`. Under C's unsigned wraparound semantics,
`*result = (a + b) mod 2^N` where N is the width. Since the true sum
exceeds `2^N - 1`, the wrapped result satisfies
`*result = (a + b) - 2^N < a` (because `b >= 1` and the wrap subtracts
`2^N`). Therefore `*result >= a` is `false`, and the function correctly
returns `false`. QED.

**Verification**: The CI wrapper explicitly checks that exactly these
two goals time out and no others. Any additional timeout is a regression
and fails the build. The full WP output is uploaded as the
`wp-proof-output` CI artifact for auditor inspection.

**Future work**: Investigate adding an ACSL ghost lemma encoding the
modular-arithmetic relationship to allow Alt-Ergo or Z3 to close the
goals automatically. Estimated effort: 1–3 hours with no guarantee of
success. This would bring the count to 1541/1541.

### Preprocessing flags

Two flags are passed to Frama-C's preprocessor:

- **`-DCANON_NO_REQUIRE`**: Compiles `CHECKED_ASSERT_RESULT(result)` to
  `((void)0)`. The ACSL `requires \valid(result)` clause provides the
  corresponding static guarantee. Without this flag, the `require_msg`
  NULL check would appear in the preprocessed source as dead code that
  WP cannot reason about (it calls an external handler function).

- **`-DNDEBUG`**: Standard release-mode flag. Combined with
  `CANON_NO_REQUIRE`, ensures the preprocessed source contains only the
  arithmetic logic, not the assertion infrastructure.

These same flags are used in the coverage CI job, ensuring the WP proof
and the MC/DC measurement operate on the same preprocessed source.

### ACSL contract conventions

- All contracts use `(integer)` casts for mathematical bounds that could
  exceed the C type's range (17 uses across the file). This prevents
  specification-level overflow.

- All functions have `complete behaviors;` and `disjoint behaviors;`
  markers (18 of 18), ensuring the behavioral specification is exhaustive.

- All functions have `assigns *result;` (18 of 18), bounding side effects.

- 82 individual behaviors across 18 functions (average 4.5 per function).

### Reproduction

To reproduce the proof locally:

```bash
# Install Frama-C 29.0 with Alt-Ergo and Z3
opam install frama-c.29.0 alt-ergo why3 z3

# Configure Why3 provers
why3 config detect

# Run WP on checked.h
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/primitives/checked.h
```

Expected output: `Proved goals: 1539 / 1541` with exactly 2 timeouts
on the goals listed above.

---

## Verification roadmap

The following headers are planned for ACSL annotation and WP proof,
in dependency order (bottom-up within each layer):

### core/primitives/ (next)

| Header       | Status     | Notes                                    |
|--------------|------------|------------------------------------------|
| checked.h    | ✅ Verified | 1539/1541 automatic, 2 manual            |
| bits.h       | Planned    | Bitwise ops, 100% MC/DC already          |
| compare.h    | Planned    | Total ordering, 100% MC/DC already       |
| ptr.h        | Planned    | Alignment and pointer arithmetic          |
| types.h      | N/A        | Type definitions only, no logic to prove  |
| limits.h     | N/A        | Constant definitions only                 |
| contract.h   | N/A        | Assertion infrastructure, not provable    |

### core/ (after core/primitives/ complete)

| Header       | Status     | Notes                                    |
|--------------|------------|------------------------------------------|
| arena.h      | Planned    | Memory region management                  |
| pool.h       | Planned    | Fixed-size block allocator                |
| slice.h      | Planned    | Non-owning views                          |
| memory.h     | Planned    | Allocation wrappers                       |
| region.h     | Planned    | Lifetime management                       |
| scope.h      | Planned    | Cleanup pairing                           |
| ownership.h  | N/A        | Annotation macros only, no logic          |

### semantics/ (after core/ complete)

Result, Option, borrow, diag — planned after core/ layer is verified.

### data/ and algo/ (longer term)

Collections and algorithms — planned after semantics/ layer is verified.
Robin Hood hashmap proof is a specific research-grade target.
