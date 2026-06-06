# Verification Record

## Overview

This document records the formal verification status of Canon-C library
headers. Verification is performed using Frama-C WP (Weakest Precondition)
with ACSL contracts, enforced in CI on every push to master.

Combined verification status across all annotated headers:

| Metric               | Value                                                                          |
|----------------------|--------------------------------------------------------------------------------|
| **Headers verified** | 9 (checked.h, bits.h, compare.h, ptr.h, slice.h, memory.h, arena.h, pool.h, region.h)    |
| **Functions**        | 199 annotated and verified                                                     |
| **Total obligations**| 18732 (per-header own goals; CI WP runs include substrate)                     |
| **Proved automatic** | 18269 (97.53%)                                                                 |
| **Unproved**         | 463 (all documented; see per-header sections)                                  |
| **Prover setup**     | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1 (triple)                               |

The slice.h baseline (367 / 390) carries a higher residual fraction
than the four primitives headers because it is the first Canon-C header
that crosses the C standard library boundary — it calls `memcmp` and
`strlen`, whose ACSL contracts require initialization and danglingness
reasoning that Frama-C 29 has not yet implemented. The 23 slice.h
residuals (5.9% of slice.h's obligations) are therefore not specific
to slice.h's design but are the cost of crossing into libc with the
current verifier; future headers using these functions will incur the
same residuals. See VERIFY-007 in `docs/deviations.md` for the full
classification.

The memory.h baseline (2805 / 2862) is the first Canon-C verification
round where inherited residuals from the substrate exceed memory.h-own
residuals: 31 of memory.h's 57 unproved goals are byte-identical to
goals already documented in VERIFY-002 (checked.h), VERIFY-006 (ptr.h),
and VERIFY-007 (slice.h), re-emitted in memory.h's run because
memory.h includes these headers transitively. The 26 memory.h-own
residuals fall into three categories: 5 \fresh/\freeable allocation
reasoning goals (mem_alloc/free/array_checked), 9 bitwise-alignment
integer theory goals (mem_align/is_aligned/get_alignment), and 12
memcmp call-site initialization/danglingness goals (mem_compare/equal/
equal_bytes). All 26 are rooted in the same Frama-C 29 feature gaps
that produced VERIFY-006 and VERIFY-007's residuals; none are fixable
by strengthening memory.h's contracts. See VERIFY-008 in
`docs/deviations.md` for the full classification.

The 31:26 inherited:own ratio is the first quantitative data point
for the composable-verification thesis: substrate residuals propagate
without amplification across composition layers, and a layer that
builds on multiple already-verified headers inherits their residual
fingerprints unchanged. The 31 inherited residuals are byte-identical
between memory.h round 2 (before contract-shape fixes) and memory.h
round 3 (after fixes) — the round 3 fix removed 10 contract-shape
residuals and zero inherited ones, confirming that contract-level
edits do not perturb the substrate's residual surface.

The arena.h baseline (3369 / 3472) is the third Canon-C verification
data point and the first to demonstrate propagation through two
composition layers. arena.h includes memory.h, which transitively
includes ptr.h, slice.h, checked.h, and contract.h. arena.h's 57
inherited residuals are byte-identical to memory.h's full 57-goal
residual surface (memory.h's 31 inherited + memory.h's 26 own), and
arena.h adds 46 own residuals of its own across four categories:
8 ptr_span call-site residuals at arena_alloc / arena_alloc_aligned
(downstream of VERIFY-006's empty `nonnull` behaviors), 26
arithmetic-chain residuals at arena_alloc / arena_alloc_aligned (the
deepest residual class, arising from the deliberate readable form of
the `arena_can_fit` predicate), 10 wrapper-delegation residuals on
the zero/try variants, and 2 arena_free_bytes helper call-site
residuals. The 57:46 inherited:own ratio confirms the composable
verification claim at a third composition layer — substrate residuals
propagate without amplification, and arena.h is the first header to
observe propagation through two layers of composition (substrate →
memory.h → arena.h). See VERIFY-009 in `docs/deviations.md` for the
full classification.

The pool.h baseline (3775 / 3902) is the fourth Canon-C verification data
point and the first to observe residual propagation across a two-hop
transitive include. pool.h includes arena.h, which transitively includes
memory.h, ptr.h, slice.h, checked.h, and contract.h. pool.h's 103 inherited
residuals are byte-identical to arena.h's full 103-goal residual surface
(arena.h's 57 inherited + arena.h's 46 own), and pool.h adds 24 own residuals
across four categories: 5 pool_invariant-establishment arithmetic residuals at
pool_init (3 of them LIMITATION-SUSPECTED pending a manual invariant review),
6 ptr_elem-cascade residuals at pool_alloc / pool_get / pool_get_const
(downstream of VERIFY-006's empty `nonnull` behaviors), 5 bytes_from / mem_zero
call-site residuals on pool_alloc_zero / pool_as_bytes / pool_reserved_bytes,
and 8 arena-delegation + reset-wrapper residuals on pool_alloc_zero /
pool_reset / pool_reset_secure. The 103:24 inherited:own ratio is the most
lopsided yet — a direct consequence of pool.h sitting atop the deepest
substrate stack verified to date while adding the thinnest own surface: pool.h
reserves its region once at pool_init and computes slots by fixed-stride
ptr_elem, so it has no per-allocation alignment-pad arithmetic and arena.h's
26-goal cat 2b arithmetic-chain residual class does not recur in pool.h's own
surface. See VERIFY-010 in `docs/deviations.md` for the full classification.

A note on totals: the 18732 obligation count is the row-sum of
each header's own WP-relevant goals — checked.h's 1755, bits.h's
761, compare.h's 208, ptr.h's 1739, slice.h's 390, memory.h's 2862,
arena.h's 3472 and pool.h's 3902 (each counted in full because each header was
verified atop its full substrate, with no separate substrate-free
measurement available for downstream headers). The CI WP steps for
downstream headers report larger numbers because their runs include
substrate via `#include` — for instance, ptr.h's CI step reports
1943/1953 due to checked.h growth at c3df659. Those +214 obligations
are checked.h's, not ptr.h's, and are counted in checked.h's row
above; double-counting them would inflate the aggregate.

---

## core/primitives/checked.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified                                        |
| **Baseline commit**    | c3df659 (Canon-C CI #804)                       |
| **Functions**          | 30 of 30 annotated and proved                  |
| **Proof obligations**  | 1753 / 1755 discharged automatically            |
| **Manually discharged**| 2 (see below)                                   |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120`         |
| **CI enforcement**     | Yes — exactly 2 named timeouts allowed          |
| **MC/DC coverage**     | 100% (96/96 condition outcomes)                 |
| **Line coverage**      | 100% (131/131)                                  |
| **Function coverage**  | 100% (30/30)                                    |
| **CI artifact**        | `wp-proof-checked` (full per-goal breakdown)    |

### Function inventory

The 30 functions split into two groups by verification approach:

**Overflow-detecting arithmetic (18 functions):** `checked_add`,
`checked_add_u8/u16/u32/u64`, `checked_sub`, `checked_sub_u8/u16/u32/u64`,
`checked_mul`, `checked_mul_u8/u16/u32/u64`, `checked_add_isize`,
`checked_sub_isize`, `checked_mul_isize`. These functions use compiler
builtins (`__builtin_*_overflow`) on GCC/Clang with portable fallback
paths for verification under `__FRAMAC__` (see VERIFY-001).

**UB-detecting division and modulo (12 functions):** `checked_div`,
`checked_div_u8/u16/u32/u64`, `checked_div_isize`, `checked_mod`,
`checked_mod_u8/u16/u32/u64`, `checked_mod_isize`. These functions
have no compiler builtin equivalent — `__builtin_div_overflow` does
not exist. They are implemented directly in C under all build
configurations and verified directly without the `__FRAMAC__`
workaround. The verified code is the executed code.

### What is proved

- **Functional correctness**: Every function's ACSL contract (requires,
  ensures, assigns, behaviors) is verified for all possible inputs
  consistent with the preconditions. Behaviors are marked `complete`
  and `disjoint`, ensuring the specification is exhaustive and
  non-overlapping.

  For overflow-detecting functions, behaviors are
  `no_overflow` / `overflow` (with an additional `zero` behavior on
  multiplication for `a == 0 || b == 0`).

  For division and modulo, behaviors are `ok` / `div_by_zero` for
  unsigned variants, and `ok` / `div_by_zero` / `overflow` for the
  signed isize variants (where `overflow` captures the
  `ISIZE_MIN / -1` and `ISIZE_MIN % -1` UB cases).

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

| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 30               | (structural)      |
| Unreachable    | 30               | (structural)      |
| Qed (internal) | 1587             | 1ms–10ms          |
| Alt-Ergo 2.6.3 | 92               | 14ms–47ms         |
| CVC5 1.2.1     | 3                | 79ms–268ms        |
| Z3 4.15.2      | 11               | 18ms–72ms         |
| Timeout        | 2                | >120s (see below) |
| **Total**      | **1753 / 1755**  |                    |

`Terminating` and `Unreachable` are WP's structural categories for
goals that are discharged without invoking an SMT solver — termination
proofs and unreachable-by-construction proof obligations. They count
toward the proved total alongside the prover-discharged goals.

The division and modulo functions added 214 new proof obligations
over the previous baseline (1541 → 1755). All 214 were discharged
automatically — most by Qed (`+174`) plus contributions from Alt-Ergo
and the structural categories. Z3 and CVC5 were not invoked on any
new goal, confirming that the div/mod contracts require no nonlinear
or modular reasoning. The two manually-discharged goals are unchanged
from the previous baseline (both belong to the original add-overflow
functions).

### Manually discharged goals

Two proof obligations are not discharged by any automated prover
(demonstrated **triple-prover-resistant** under Alt-Ergo + Z3 + CVC5):

1. **`typed_checked_add_overflow_ensures`**
2. **`typed_checked_add_u64_overflow_ensures`**

Both assert that when `a > USIZE_MAX - b` (or `a > 0xFFFFFFFFFFFFFFFF - b`),
the function returns `false`. The fallback implementation computes
`*result = a + b` (unsigned wraparound, well-defined in C) and returns
`*result >= a`, which is `false` when overflow occurred.

**Why provers fail**: WP formulates this in its integer memory model where
unsigned wraparound requires modular-arithmetic reasoning. None of
Alt-Ergo 2.6.3, Z3 4.15.2, or CVC5 1.2.1 close this goal because the
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
`wp-proof-checked` CI artifact for auditor inspection.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: Compiles `CHECKED_ASSERT_RESULT(result)` to
  `((void)0)`. The ACSL `requires \valid(result)` clause provides the
  corresponding static guarantee.

- **`-DNDEBUG`**: Standard release-mode flag. Combined with
  `CANON_NO_REQUIRE`, ensures the preprocessed source contains only the
  arithmetic logic, not the assertion infrastructure.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5 \
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

Expected output: `Proved goals: 1753 / 1755` with exactly 2 timeouts.

---

## core/primitives/bits.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | debb202 (Canon-C CI #795)                       |
| **Functions**          | 18 of 18 annotated                             |
| **Proof obligations**  | 746 / 761 discharged automatically (98.03%)     |
| **Timeouts**           | 15 (all WP model limitations, see below)        |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120`         |
| **CI enforcement**     | Yes — exactly 15 named unproved goals expected  |
| **MC/DC coverage**     | 100% (52/52 condition outcomes)                 |
| **CI artifact**        | `wp-proof-bits` (full per-goal breakdown)       |

### What is proved

- **Functional correctness** for simple bit operations: `bits_test`,
  `bits_set`, `bits_toggle`, `bits_extract`, `bits_is_power_of_two`,
  `bits_ffs`, `bits_fls` — full behavioral specs with `complete` and
  `disjoint` behaviors.

- **Range and zero properties** for complex algorithms: `bits_clz`,
  `bits_ctz`, `bits_popcount` — range bounds proved, zero behavior
  proved exactly.

- **Absence of runtime errors** (`-wp-rte`) for most functions.

- **Side-effect bounding**: Every function specifies `assigns \nothing;`.

### Prover breakdown

| Prover         | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Qed (internal) | 701              | 1ms–12ms          |
| Alt-Ergo 2.6.3 | 12               | 20ms–44ms         |
| Z3 4.15.2      | 1                | 157ms             |
| Timeout/Unknown| 15               | >120s (see below) |
| **Total**      | **746 / 761**    |                    |

Note: CVC5 1.2.1 is invoked as a tertiary prover on this header but
closes none of the 15 unproved goals — they are demonstrated
triple-prover-resistant.

### Timeout goals (15)

All 15 are WP model limitations on bitwise reasoning. Grouped by root cause:

**Bitwise complement (3):** `bits_clear`, `bits_insert` — WP cannot
connect C's `~x` to XOR-based ACSL spec.

**SWAR popcount (1):** `bits_popcount` — magic constants + multiplication
beyond SMT reasoning.

**Rotation RTE + spec (4):** `bits_rotl`, `bits_rotr` — RTE shift check
on `64 - shift` and bitwise OR in spec.

**Next-power-of-two minimality (4):** `bits_next_power_of_two` — the
`result / 2 < value` property through bit-smearing.

**Byte swap (3):** `bits_bswap16` signed overflow RTE (u16→int promotion),
`bits_bswap32`/`bits_bswap64` multi-term OR spec timeout.

Note: Some goals may appear as `[Unknown]` instead of `[Timeout]` across
runs — WP solver heuristics are nondeterministic. The CI enforcement
counts both combined.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/primitives/bits.h
```

Expected output: `Proved goals: 746 / 761` with 15 unproved goals.

---

## core/primitives/compare.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Fully verified — 100% automatic                |
| **Baseline commit**    | debb202 (Canon-C CI #795)                       |
| **Functions**          | 28 of 28 annotated and proved                  |
| **Proof obligations**  | 208 / 208 discharged automatically (100%)       |
| **Timeouts**           | 0                                               |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 208/208 required, 0 timeouts allowed      |
| **MC/DC coverage**     | 100% (8/8 condition outcomes)                   |
| **CI artifact**        | `wp-proof-compare` (full per-goal breakdown)    |

### What is proved

- **Functional correctness**: Every comparator's result is bounded to
  {-1, 0, +1}. The `(x > y) - (x < y)` branchless pattern is proved
  to always produce exactly these three values for all possible inputs.

- **Absence of runtime errors** (`-wp-rte`): WP proves no execution
  can trigger invalid memory access, signed overflow, division by zero,
  or null dereference — including through the void* → typed pointer
  casts in the function bodies.

- **Side-effect bounding**: Every function specifies `assigns \nothing;`,
  proving all comparators are pure functions.

- **Memory safety through void* casts**: The `-wp-model Typed+Cast`
  memory model allows WP to reason about the void* → typed pointer
  casts that every comparator performs. With the default `Typed` model,
  WP treats void* as char* and cannot connect char-level validity to
  typed pointer dereferences. `Typed+Cast` resolves this cleanly.

### Prover breakdown

| Prover         | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Qed (internal) | 148              | 1ms–4ms           |
| Alt-Ergo 2.6.3 | 36               | 16ms–56ms         |
| Timeout        | 0                |                    |
| **Total**      | **208 / 208**    |                    |

Note: Z3 4.15.2 and CVC5 1.2.1 are invoked as secondary and tertiary
provers but all 60 non-Qed goals are closed by Alt-Ergo alone on this
header.

### WP memory model note

compare.h uses `-wp-model Typed+Cast` instead of the default `Typed`
model. This is necessary because every comparator takes `const void*`
parameters and casts them to typed pointers inside the function body
(e.g. `*(const u32*)a`). Frama-C's WP treats `void*` as `char*`
(sint8*) internally. With the default `Typed` model, the cast from
sint8* to uint32* is flagged as incompatible and all RTE mem_access
goals become unprovable (`Stronger` status). `Typed+Cast` relaxes
the memory model to allow these casts, which is sound because the
callers always pass correctly-typed pointers — the void* is a C
generics mechanism, not a type-punning operation.

This flag is applied to compare.h, ptr.h, slice.h, memory.h, and
arena.h (all use void* → typed pointer casts). checked.h and bits.h
do not use void* parameters and work correctly with the default
`Typed` model.

### ACSL contract conventions

- All contracts use byte-level validity: `\valid_read((char *)a + (0 .. sizeof(T) - 1))`
  instead of typed-pointer validity. This avoids the incompatible-cast
  warnings in the ACSL preconditions (the `Typed+Cast` model handles
  the cast inside the function body, not in the contract).

- All functions specify `assigns \nothing;` (28 of 28).

- Range ensures: `ensures -1 <= \result <= 1` on all 28 functions.

- Floating-point comparators (4 functions) have behavioral specs with
  `complete` and `disjoint` behaviors covering NaN × NaN, NaN × normal,
  normal × NaN, and normal × normal cases using `\is_NaN`.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/primitives/compare.h
```

Expected output: `Proved goals: 208 / 208` with 0 timeouts.

---

## core/primitives/ptr.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | debb202 (Canon-C CI #795)                       |
| **Functions**          | 21 of 21 annotated                             |
| **Proof obligations**  | 1729 / 1739 discharged automatically (99.43%)   |
| **Timeouts**           | 10 (all documented under VERIFY-006)            |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 1729/1739 ptr.h-own goals with 10 named residuals; full WP run reports 1943/1953 (incl. checked.h substrate) |
| **MC/DC coverage**     | 100% (42/42 condition outcomes)                 |
| **CI artifact**        | `wp-proof-ptr` (full per-goal breakdown)        |

### Baseline note

ptr.h's own verification baseline is **1729 / 1739** — the proof
obligations belonging to ptr.h's own functions, with 10 documented
residuals (VERIFY-006). This number has not changed since the
original v1.3.0 verification at commit `debb202` (Canon-C CI #795,
Apr 22 2026); ptr.h's source has not materially changed since.

The CI step, however, runs Frama-C on ptr.h *with all `#include`s
expanded into the translation unit*, so its output reports the
combined obligation count for ptr.h plus everything ptr.h pulls in.
On master, ptr.h `#include`s checked.h, so the CI step today reports
**1943 / 1953** instead of 1729 / 1739. The +214 difference is the
12 division and modulo functions added to checked.h at commit
`c3df659` (CI #804, Apr 27 2026) — see VERIFY-002 and the checked.h
section. Those 214 obligations belong to checked.h, not to ptr.h;
they are counted separately in checked.h's own row of the per-header
table. The CI wrapper uses the 1943/1953 figure for enforcement
because that's what WP reports, but the substantive ptr.h baseline
is 1729/1739.

The 10 named residuals (VERIFY-006) are the same goals before and
after c3df659 — none of the 214 inherited obligations entered the
unproved list.

### What is proved

- **Functional correctness**: Every function's ACSL contract is
  verified — alignment predicates (`is_power_of_two`, `is_aligned`),
  alignment arithmetic (`align_up`, `align_down`, `align_padding`),
  pointer arithmetic (`ptr_offset`, `ptr_retreat`, `ptr_diff`,
  `ptr_span`), bounds checking (`ptr_in_range`, `ptr_range_in_range`,
  `ptr_is_valid`, `ptr_is_aligned`), and element indexing
  (`ptr_elem`, `ptr_elem_const`, `ptr_or`, `ptr_or_const`). Behaviors
  are marked `complete` and `disjoint` where applicable.

- **Absence of runtime errors** (`-wp-rte`): WP proves no execution
  can trigger signed overflow, division by zero, invalid shifts, null
  dereference, or out-of-bounds access — including through the
  pointer-to-integer casts used in alignment computations.

- **Side-effect bounding**: Every function specifies `assigns \nothing;`.
  All functions are pure with respect to memory.

### Prover breakdown

| Prover         | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Qed (internal) | 1563             | 1ms–10ms          |
| Alt-Ergo 2.6.3 | 83               | 16ms–45ms         |
| CVC5 1.2.1     | 3                | 81ms–255ms        |
| Z3 4.15.2      | 11               | 20ms–69ms         |
| Timeout        | 10               | >120s (see below) |
| **Total**      | **1729 / 1739**  |                    |

The CI WP step reports 1943/1953 because its run includes checked.h
(via `#include`). The breakdown above is for ptr.h's own goals; the
inherited substrate goals are counted under checked.h in the per-
header table.

### Timeout goals (10)

All 10 are documented as **triple-prover-resistant** under Alt-Ergo
2.6.3 + Z3 4.15.2 + CVC5 1.2.1. They fall into four categories:

**Transitive checked.h overflow (2):**
`typed_cast_checked_add_overflow_ensures`,
`typed_cast_checked_add_u64_overflow_ensures` — the same two goals
documented in VERIFY-002 (checked.h), re-emitted when checked.h is
included in the ptr.h proof run.

**Align formula ensures (3):** `typed_cast_align_up_ensures`,
`typed_cast_align_down_ensures`, `typed_cast_align_padding_ensures` —
WP integer theory cannot bridge bitwise AND with complement
(`~(a - 1)`) and arithmetic multiples-of-a equality.

**Ptr_align call-chain preconditions (3):**
`typed_cast_ptr_align_up_call_align_up_requires_3`,
`typed_cast_ptr_align_padding_call_align_padding_requires_3`,
`typed_cast_ptr_align_padding_nonnull_ensures_part2` — arise when
ptr.h call sites reconstruct align_up / align_padding preconditions
through pointer-to-integer (`uintptr_t`) casts. The Typed+Cast model
allows the casts but cannot carry the numeric bounds across the
round-trip.

**Contract handler non-termination (2):**
`typed_cast_contract_default_handler_loop_invariant_established`,
`typed_cast_contract_default_handler_terminates` — under `__FRAMAC__`,
contract.h replaces `contract_default_handler`'s body with `while(1) {}`,
the standard ACSL idiom for a non-returning function. WP correctly
identifies the function as non-terminating; the `terminates` goal and
the loop-invariant establishment goal are unprovable by construction,
which is the intended semantics of a panic handler.

Full goal list and manual proof arguments: see VERIFY-006 in
docs/deviations.md.

### WP memory model note

ptr.h uses `-wp-model Typed+Cast` for the same reason as compare.h:
byte-level pointer arithmetic in `ptr_offset`, `ptr_retreat`,
`ptr_elem` and related functions performs `void*` → `u8*` casts to
compute byte offsets. The soundness argument (ISO C99 §6.3.2.3¶7) is
identical: callers supply correctly-aligned pointers, and the casts
preserve the referenced byte range.

### ACSL contract conventions

- Pointer validity uses byte-level ranges: `\valid((char *)p + (0 .. size - 1))`
  to allow Typed+Cast to bridge the void* → u8* conversion.

- All functions specify `assigns \nothing;` (21 of 21). The
  `wp:pedantic-assigns` warnings observed in the CI output are
  advisory — they note that pointer-returning functions would benefit
  from `assigns \result \from ...` clauses for tighter caller
  assumptions. This is a precision-refinement opportunity for future
  work, not a soundness concern.

- The `nonnull` behaviors on `ptr_align_up`, `ptr_align_down`,
  `ptr_offset`, `ptr_offset_const`, and `ptr_retreat` carry no
  `ensures` clause. The empty body is deliberate (see VERIFY-006
  forward-implication note); strengthening it would require WP to
  discharge the uintptr_t round-trip on ptr.h's own bodies, which is
  the VERIFY-006 cat 3 limitation. Downstream headers inherit
  call-chain residuals as a consequence — see VERIFY-009 cats 2a
  and 2d for arena.h's instances.

- Contract handler (`core/primitives/contract.h`) under `__FRAMAC__`
  uses `ensures \false` + `exits \false` + `terminates \false` +
  `loop invariant \false` to specify non-returning semantics. The two
  resulting unproved goals (see Category 4 above) are the ACSL
  manifestation of the non-termination contract and are correct by
  construction.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: Same as checked.h — compiles runtime NULL
  checks away; ACSL `requires` clauses provide static guarantees.

- **`-DNDEBUG`**: Standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/primitives/ptr.h
```

Expected output: `Proved goals: 1943 / 1953` with 10 timeouts. (The
1953 reflects the full WP run on ptr.h's translation unit, including
checked.h. ptr.h's own contribution is 1729 / 1739; the +214 are
checked.h's div/mod goals — see Baseline note above.)

---

## core/slice.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | (Canon-C CI #821)                               |
| **Functions**          | 22 of 22 non-macro functions annotated          |
| **Proof obligations**  | 367 / 390 discharged automatically (94.10%)     |
| **Timeouts**           | 23 (all documented under VERIFY-007)            |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 367/390 with 23 named goals expected      |
| **MC/DC coverage**     | 93.1% (54/58 condition outcomes — see MCDC-002) |
| **CI artifact**        | `wp-proof-slice` (full per-goal breakdown)      |

### Function inventory

slice.h provides four type families with different verification scope:

**bytes_t (mutable byte view) — 9 functions, all annotated and proved:**
`bytes_from`, `bytes_empty`, `bytes_as_const`, `bytes_is_empty`,
`bytes_at`, `bytes_equal`, `bytes_slice`, `bytes_take`, `bytes_skip`.

**cbytes_t (read-only byte view) — 2 constructors, annotated and proved:**
`cbytes_from`, `cbytes_empty`.

**str_t (read-only character view) — 11 functions, annotated and proved:**
`str_from`, `str_from_cstr`, `str_empty`, `str_is_empty`, `str_equal`,
`str_starts_with`, `str_ends_with`, `str_slice`, `str_take`, `str_skip`,
`str_as_bytes`. (str_from_cstr and the four equality/match functions
carry partial functional specs — see "What is proved" below.)

**DEFINE_SLICE(T) macro — 14 generated functions per instantiation,
documentation only:** The C preprocessor strips ACSL annotations inside
`#define` bodies before macro expansion, so the macro-generated
functions (`slice_T_from`, `slice_T_at`, `slice_T_first`, etc.) are not
WP-verified in this baseline. Contract specifications are retained in
the macro body as human-readable comments. These functions are
validated by unit testing (90 tests in `test/core/slice_test.c`),
fuzzing, and 93.1% MC/DC coverage on the i32 instantiation. Full
WP verification of the macro family will require a separate
`slice_verify.h` driver instantiating `DEFINE_SLICE(i32)` outside
the macro context — planned for a future commit if the certification
target requires it.

### What is proved

- **Type invariants**: slice.h defines three named ACSL predicates
  (`bytes_invariant`, `cbytes_invariant`, `str_invariant`) and four
  validity predicates with `{L}` memory-state labels (`bytes_valid_read`,
  `bytes_valid_write`, `cbytes_valid`, `str_valid`). Every function
  carries the appropriate predicate as a precondition. WP uses these
  predicates to discharge the four `!ptr` defensive branches in
  `bytes_slice`, `bytes_skip`, `str_slice`, `str_skip` as unreachable
  — the cross-stream closure of MCDC-002.

- **Functional correctness for non-libc functions** (17 of 22): Full
  behavioral specs with `complete` and `disjoint` behaviors on
  multi-case functions (`bytes_at`, `bytes_slice`, `bytes_skip`,
  `str_slice`, `str_skip`, `str_from_cstr`).

- **Partial functional correctness for libc-bridging functions** (5 of 22):
  `bytes_equal`, `str_equal`, `str_starts_with`, `str_ends_with`, and
  `str_from_cstr` carry range and structural specs but defer full
  functional semantics to testing — see "Timeout goals" below for the
  reasoning.

- **Absence of runtime errors** (`-wp-rte`): WP proves no execution
  of any annotated function can trigger signed overflow, invalid
  memory access, null dereference, or out-of-bounds access through
  the void* → u8* casts in the byte-construction functions.

- **Side-effect bounding**: Every function specifies `assigns \nothing;`.
  All slice operations are pure with respect to memory. The
  `wp:pedantic-assigns` warnings observed in the CI output are
  advisory — they note that pointer-returning functions
  (`bytes_at` specifically) would benefit from `assigns \result \from ...`
  clauses for tighter caller assumptions. This is a
  precision-refinement opportunity for future work, not a soundness
  concern.

### Prover breakdown

| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 12               | (structural)      |
| Unreachable    | 15               | (structural)      |
| Qed (internal) | 292              | 0.6ms–11ms        |
| Alt-Ergo 2.6.3 | 47               | 16ms–73ms         |
| Z3 4.15.2      | 1                | 65ms              |
| Timeout        | 23               | >120s (see below) |
| **Total**      | **367 / 390**    |                    |

The 23 unproved goals belong to three documented categories (see
VERIFY-007); WP's discharge of the four MCDC-002 functions as
unreachable is verified separately by the wrapper's
`MCDC-002 functions with WP residuals: 0/4` diagnostic, which
confirms none of `bytes_slice`, `bytes_skip`, `str_slice`, or
`str_skip` appear in the unproved list.

CVC5 1.2.1 is invoked as a tertiary prover but closes none of the 23
unproved goals — they are demonstrated triple-prover-resistant.

### Timeout goals (23)

All 23 are documented as triple-prover-resistant. They fall into three
categories:

**memcmp call-site preconditions (20):** Six per `bytes_equal` and
`str_equal`, four per `str_starts_with` and `str_ends_with`.
Goal-name pattern:
`typed_cast_<func>_call_memcmp_requires_<aspect>` where `<aspect>`
ranges over `valid_s1`, `valid_s2`, `initialization_s1`,
`initialization_s2`, `danglingness_s1`, `danglingness_s2`. The
`valid_*` obligations appear only on the bytes_t variants because
the str_t variants close them through `str_valid` predicate
reasoning; the `initialization_*` and `danglingness_*` obligations
appear on all four equality functions.

These are not solver-strength residuals. WP itself reports the
limitation during the proof run:

```
[wp] FRAMAC_SHARE/libc/string.h:38: Warning:
  Allocation, initialization and danglingness not yet implemented
  (\dangling{L}((char *)s + i))
```

ACSL's `memcmp` standard-library contract requires the caller to
establish that both buffer ranges are fully valid, fully initialized,
and non-dangling. slice.h's `bytes_valid_write` and `str_valid`
predicates establish validity, but the initialization and danglingness
obligations cannot be discharged because the underlying logic is not
yet implemented in Frama-C 29. Strengthening the slice.h predicates
would not close these goals.

**strlen valid_string precondition (1):**
`typed_cast_str_from_cstr_call_strlen_requires_valid_string_s` —
ACSL's `strlen` logic function requires the caller to establish
`valid_string(s)` (a null-terminated string with valid memory through
the terminator). slice.h's `str_from_cstr` deliberately omits this
precondition because adding `requires valid_read_string(cstr)` would
introduce a soundness dependency on Frama-C's `-frama-c-stdlib`
configuration that no other Canon-C header requires. The cost (one
documented residual) is much smaller than the cost (project-wide
stdlib dependency for one function).

**Transitive contract.h handler non-termination (2):**
`typed_cast_contract_default_handler_terminates`,
`typed_cast_contract_default_handler_loop_invariant_established`
— same two goals as VERIFY-006 category 4. slice.h includes
contract.h transitively through the `require_msg` calls in its
constructor functions, so the unprovable-by-construction goals are
re-emitted in the slice.h proof run. These are not new slice.h
residuals; they are the same goals counted in VERIFY-006 reappearing.

Full goal list and per-goal Qed-and-prover timing: see VERIFY-007 in
`docs/deviations.md`. The CI artifact `wp-proof-slice` contains the
verbatim WP output for auditor inspection.

### MCDC-002 closure

The MCDC-002 deviation documents four `!ptr` defensive branches in
`bytes_slice`, `bytes_skip`, `str_slice`, and `str_skip` that are
unreachable in MC/DC isolation through the public API. WP discharges
these branches statically under the type invariant predicates — none
of the four functions appears in the unproved goal list. The slice.h
CI wrapper explicitly verifies this with the diagnostic line `MCDC-002
functions with WP residuals: 0/4`. See the MCDC-002 status update in
`docs/deviations.md` for the formal closure record. gcov measurement
remains at 54/58 (93.1%) because gcov instruments the source rather
than the proof; the two evidence streams complement rather than
converge.

### WP memory model note

slice.h uses `-wp-model Typed+Cast` for the same reason as compare.h
and ptr.h: the constructors `bytes_from`, `cbytes_from`, and the
macro-generated `slice_T_as_bytes`/`slice_T_as_cbytes` perform
`void*` → `u8*` casts. The default `Typed` model rejects these casts
and every RTE mem_access goal becomes unprovable. Soundness argument
identical to VERIFY-005 / VERIFY-006: callers supply correctly-typed
pointers, and the casts preserve the referenced byte range.

### ACSL contract conventions

- Type invariants are encoded as named predicates rather than inline
  conditions, so the same predicate text is reused across function
  contracts. This makes the predicates available as lemmas for WP
  and keeps the contracts readable.

- Validity predicates carry `{L}` memory-state labels because they
  reference memory; bare invariant predicates do not because they
  test only the {ptr, len} structural relationship.

- Contracts use byte-level validity for memory ranges:
  `\valid((u8 *)ptr + (0 .. len - 1))` to allow Typed+Cast to bridge
  the void* → u8* conversion at the contract level.

- Equality functions (`bytes_equal`, `str_equal`, `str_starts_with`,
  `str_ends_with`) carry partial functional specs — range
  (`\result == \true || \result == \false`), structural (length-
  mismatch returns false, same-pointer returns true,
  zero-length-prefix/suffix returns true), and absence of runtime
  errors. Full equality semantics (the "memcmp == 0" postcondition)
  are deferred to testing because the memcmp axiomatic block needed
  to prove them is the same feature gap documented under timeout
  category 1. This follows the pattern set by VERIFY-004 for bits.h's
  CLZ/CTZ/popcount range-only specs.

- `str_from_cstr` carries a partial spec for the same reason —
  pointer and length-pairing properties are proved, but
  `\result.len == strlen(cstr)` is not, because asserting it requires
  the strlen logic function which is the same residual as timeout
  category 2.

- All functions specify `assigns \nothing;` (22 of 22). The single
  `wp:pedantic-assigns` warning on `bytes_at` is precision-refinement
  opportunity, not a soundness concern.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: Same as the other verified headers —
  compiles `require_msg` runtime NULL checks away; ACSL `requires`
  clauses provide static guarantees.

- **`-DNDEBUG`**: Standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/slice.h
```

Expected output: `Proved goals: 367 / 390` with 23 timeouts.

---

## core/memory.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | b3e668b (Canon-C CI #841)                       |
| **Functions**          | 27 of 27 non-macro functions annotated          |
| **Proof obligations**  | 2805 / 2862 discharged automatically (98.01%)   |
| **Timeouts**           | 57 (all documented under VERIFY-008)            |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 2805/2862 with 57 named goals expected    |
| **MC/DC coverage**     | 88.3% (113/128 condition outcomes)              |
| **CI artifact**        | `wp-proof-memory` (full per-goal breakdown)     |

### Function inventory

memory.h provides safe wrappers around `memcpy`, `memmove`, `memset`,
`memcmp`, `malloc`, and `free`, plus alignment helpers and
bytes_t/cbytes_t slice variants. The 27 user-facing functions split
into three groups by verification surface:

**Group 1 — alignment and allocation (9 functions):**
`mem_regions_overlap`, `mem_alloc`, `mem_free`,
`mem_alloc_array_checked`, `mem_align`, `mem_align_to`,
`mem_is_aligned`, `mem_get_alignment`, `mem_is_power_of_two`.

**Group 2 — raw memory operations (11 functions):**
`mem_copy`, `mem_move`, `mem_zero`, `mem_secure_zero`, `mem_set`,
`mem_compare`, `mem_equal`, `mem_is_all`, `mem_is_zero`, `mem_swap`,
`mem_swap_buf`.

**Group 3 — bytes_t/cbytes_t variants (7 functions):**
`mem_copy_bytes`, `mem_move_bytes`, `mem_zero_bytes`,
`mem_set_bytes`, `mem_equal_bytes`, `mem_is_zero_bytes`,
`mem_secure_zero_bytes`.

**Type-safe macros — documentation only:** `mem_alloc_type`,
`mem_alloc_array`, `mem_zero_type`, `mem_zero_array`,
`mem_secure_zero_type`, `mem_copy_type`, `mem_equal_type`. Same
verification rationale as VERIFY-007's DEFINE_SLICE: ACSL inside
`#define` bodies is preprocessor-stripped before macro expansion, so
macro-generated code is documented but not directly WP-verified.
`mem_alloc_array` is the most security-critical macro because it
performs `sizeof(Type) * count` arithmetic; its overflow safety is
provided transitively through `mem_alloc_array_checked`, which the
macro routes through. Validation is by unit testing on a representative
type (i32) plus the dedicated overflow-test cases in
`test/core/memory_test.c`.

### What is proved

- **ACSL predicates**: memory.h defines four named predicates —
  `regions_overlap` (two byte regions overlap iff each starts within
  the other's range), `is_aligned_addr` (a pointer is aligned to a
  power-of-2 boundary iff its low bits are zero), `mem_valid_read`
  (size-zero is trivially valid; otherwise every byte is readable),
  and `mem_valid_write` (size-zero is trivially valid; otherwise every
  byte is writable). Every function carries the appropriate predicate
  as a precondition.

- **Functional correctness with complete behavior splits** (16 of 27
  functions): Full behavioral specs with `complete` and `disjoint`
  behaviors covering NULL handling, zero-size handling, and the
  primary functional case. Functions in Group 1 (`mem_regions_overlap`,
  `mem_alloc`, `mem_free`, `mem_alloc_array_checked`, `mem_align`,
  `mem_align_to`, `mem_is_aligned`, `mem_get_alignment`,
  `mem_is_power_of_two`) and most of Group 2 carry these full specs.

- **Functional correctness with partial behavior splits, same-pointer
  fast paths** (3 of 27 functions): `mem_compare`, `mem_equal`, and
  `mem_equal_bytes` add a `same_pointer` behavior covering both-NULL
  and same-non-NULL cases, plus separate behaviors for length
  mismatch, NULL handling, and the primary `memcmp` case. The `memcmp`
  behavior is partial — full equality semantics are deferred to
  testing for the same reason as slice.h's `bytes_equal`/`str_equal`
  (see VERIFY-008 category 2c).

- **Non-overlap preconditions** (3 of 27 functions): `mem_copy`,
  `mem_copy_bytes`, `mem_swap`, and `mem_swap_buf` carry top-level
  `requires !regions_overlap(...)` clauses. (`mem_swap_buf` carries
  three pairwise non-overlap clauses for a/b, a/scratch, b/scratch.)
  These preconditions guarantee `memcpy` semantics; callers needing
  to copy overlapping regions must use `mem_move` instead.

- **Loop invariants on byte-scanning functions** (2 of 27 functions):
  `mem_secure_zero` and `mem_is_all` carry ACSL `loop invariant`,
  `loop assigns`, and `loop variant` annotations on their byte-by-byte
  loops. WP discharges all loop-related goals automatically.

- **Absence of runtime errors** (`-wp-rte`): WP proves that no
  execution of any function can trigger signed overflow, division by
  zero, invalid shifts, null dereference, or out-of-bounds access
  through the void* → u8*/char* casts in the byte-region functions.

- **Side-effect bounding**: Functions that modify memory specify
  `assigns ((char *)dest)[0 .. size - 1];` (or the equivalent
  `bytes_t.ptr` form). Functions that do not modify memory specify
  `assigns \nothing;` — applies to all alignment helpers, predicates,
  and read-only operations.

### Prover breakdown

| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 60               | (structural)      |
| Unreachable    | 63               | (structural)      |
| Qed (internal) | 2427             | 0.88ms–11ms       |
| Alt-Ergo 2.6.3 | 241              | 12ms–180ms        |
| CVC5 1.2.1     | 2                | 62ms–81ms         |
| Z3 4.15.2      | 12               | 14ms–45ms         |
| Timeout        | 53               | >120s             |
| Unknown        | 4                | (solver gave up)  |
| **Total**      | **2805 / 2862**  |                    |

The 57 unproved goals (53 `[Timeout]` + 4 `[Unknown]` + 0 `[Failed]`)
belong to seven categories — three memory.h-own and four inherited
from substrate headers. CVC5 contributes 2 closures; Z3 contributes
12; Alt-Ergo carries the bulk of the SMT load (241 goals) at typical
times in the tens of milliseconds.

The CI wrapper sums Timeout + Unknown + Failed because WP may
reclassify the same goal between Timeout and Unknown across runs
depending on solver heuristics — what matters is that the goal is not
proved, not which category it lands in.

### Timeout goals (57)

All 57 are documented as triple-prover-resistant. They fall into
seven categories: 4 inherited from already-verified substrate (31
goals total) and 3 memory.h-own (26 goals total).

**Inherited from VERIFY-002 (2):**
`typed_cast_checked_add_overflow_ensures`,
`typed_cast_checked_add_u64_overflow_ensures` — re-emerge in the
memory.h proof run because memory.h includes checked.h transitively
through `mem_alloc_array_checked`'s call to `checked_mul`. Same goals
as in VERIFY-002 and VERIFY-006.

**Inherited from VERIFY-006 categories 2–4 (8):**
- 3 `align_*_ensures` (cat 2): `typed_cast_align_up_ensures`,
  `typed_cast_align_down_ensures`, `typed_cast_align_padding_ensures`.
- 3 `ptr_align_*` call-chain preconditions (cat 3):
  `typed_cast_ptr_align_up_call_align_up_requires_3`, related.
- 2 contract handler non-termination (cat 4):
  `typed_cast_contract_default_handler_terminates`, related.

**Inherited from VERIFY-007 categories 1–2 (21):**
- 20 memcmp call-site preconditions on slice.h's
  `bytes_equal`/`str_equal`/`str_starts_with`/`str_ends_with` —
  re-emerge here because memory.h includes slice.h transitively
  through bytes_t/cbytes_t.
- 1 strlen valid_string precondition on
  `typed_cast_str_from_cstr_call_strlen_requires_valid_string_s`.

**memory.h-own category 2a — \fresh / \freeable allocation
reasoning (5):** `mem_alloc_assigns_normal_part2`,
`mem_alloc_nonzero_size_ensures_part2`, `mem_free_assigns_normal`,
`mem_free_call_free_requires_freeable`,
`mem_alloc_array_checked_nonoverflow_ensures_part3`.

These cannot be discharged because Frama-C 29's libc spec for
`malloc` and `free` uses ACSL clauses that the verifier has not yet
implemented. WP itself reports the limitation:

```
[wp] FRAMAC_SHARE/libc/stdlib.h:427: Warning:
  Allocation, initialization and danglingness not yet implemented
  (allocation: \fresh{Old, Here}(\at(\result,wp:post),\at(size,wp:pre)))
```

This is the same root cause as VERIFY-007 category 1
(`\dangling{L}` and `\initialization`) — different ACSL primitives,
same verifier-side feature gap.

**memory.h-own category 2b — WP integer theory / bitwise alignment
(9):** `mem_align_normal_ensures_part3`,
`mem_align_normal_ensures_2_part3`, `mem_align_to_normal_ensures_part3`,
`mem_align_to_normal_ensures_2_part3`,
`mem_is_aligned_nonnull_aligned_ensures`,
`mem_is_aligned_nonnull_unaligned_ensures`,
`mem_get_alignment_assert_rte_signed_overflow`,
`mem_get_alignment_nonnull_ensures_part2`,
`mem_get_alignment_nonnull_ensures_2_part2`.

WP's integer theory cannot bridge bitwise alignment formulas
(`(addr & (alignment - 1)) == 0`) with the modular formulation
(`addr % alignment == 0`). Same root cause as VERIFY-006 category 2
(ptr.h's `align_up`, `align_down`, `align_padding`); memory.h
re-emits the limitation at its own call sites because it specifies
alignment ensures clauses in the modular form (the natural
mathematical formulation in an `ensures` clause).

**memory.h-own category 2c — memcmp call-site inheritance (12):**
4 each on `mem_compare`, `mem_equal`, `mem_equal_bytes` —
`requires_initialization_s1/s2` and `requires_danglingness_s1/s2`.

Identical root cause to VERIFY-007 category 1. memory.h directly
calls `memcmp` from these three functions, so the same residual
class re-emerges at memory.h's call sites.

Full goal list and per-category manual proof arguments: see
VERIFY-008 in `docs/deviations.md`.

### Round 3 contract-shape fix

memory.h's verification took three rounds to reach the stable
2805/2862 baseline. Round 1 annotated 9 functions and produced a
larger residual count. Round 2 extended annotation to all 27
functions and reduced toward the final shape — but 10 of the
remaining residuals were contract-shape errors, where non-overlap
preconditions sat inside a behavior's `assumes` rather than as
top-level `requires`. Round 3 (commit `b3e668b`, Canon-C CI #841)
hoisted the non-overlap preconditions to top-level requires on
`mem_copy`, `mem_copy_bytes`, `mem_swap`, and `mem_swap_buf`,
removing those 10 contract-shape residuals without introducing any
new ones. Final count: 57 residuals, byte-identical inherited count
(31) between rounds 2 and 3.

The round 3 fix is the empirical confirmation of the composable-
verification thesis: contract-level edits within memory.h removed
exactly memory.h's own contract-shape residuals and produced zero
ripple in the substrate's residual fingerprint.

### MCDC reference

memory.h's bytes_t/cbytes_t variants (`mem_copy_bytes`,
`mem_move_bytes`, `mem_zero_bytes`, `mem_set_bytes`,
`mem_equal_bytes`, `mem_is_zero_bytes`, `mem_secure_zero_bytes`)
inherit slice.h's `bytes_t` invariant rather than introducing new
public {ptr, len} types. The defensive `!ptr` checks in these
functions are guarded by `bytes_invariant`, which slice.h already
discharged under WP. memory.h does not introduce a new MCDC-002-style
deviation; the analogous defensive branches are discharged by the
inherited type invariant. memory.h's MC/DC ceiling (88.3%) reflects
the `-DCANON_NO_REQUIRE` infrastructure removal (same as in
checked.h, ptr.h, and slice.h), not API-unreachable defensive code.

### WP memory model note

memory.h uses `-wp-model Typed+Cast` for the same reason as compare.h,
ptr.h, and slice.h: the byte-region functions perform `void*` →
`char*`/`u8*` casts (e.g. `(char *)dest + (0 .. size - 1)` in the
ACSL ranges, `(const u8 *)src` in the bodies). The default `Typed`
model rejects these casts and every RTE mem_access goal becomes
unprovable. Soundness argument identical to VERIFY-005, VERIFY-006,
VERIFY-007: callers supply correctly-typed pointers, and the casts
preserve the referenced byte range.

### ACSL contract conventions

- Predicates carry `{L}` memory-state labels when they reference
  memory (`mem_valid_read`, `mem_valid_write`, `regions_overlap`),
  and bare labels otherwise (`is_aligned_addr` operates on integer
  arithmetic only).

- Validity preconditions use the disjunctive form
  `dest == \null || src == \null || size == 0 || mem_valid_write(...)`,
  which encodes the NULL/zero-safe contract uniformly across all
  byte-region functions. The behavior split below the requires
  enumerates the four cases.

- Non-overlap preconditions on `mem_copy`, `mem_copy_bytes`,
  `mem_swap`, and `mem_swap_buf` are top-level `requires` clauses.
  An earlier round 2 attempt placed them inside a behavior's
  `assumes`, which produced contract-shape residuals because WP's
  "complete behaviors" check needed every input case covered (the
  `assumes` left the overlapping-regions case in no behavior) and
  WP's call-site discharge of `memcpy`'s separation precondition
  needed the non-overlap fact available at the call site (which is
  outside any behavior's lexical scope). Top-level `requires`
  resolves both issues simultaneously — see the round 3 fix above.

- `mem_compare`/`mem_equal`/`mem_equal_bytes` have a `same_pointer`
  behavior matching `a == b` (covers both-NULL and same-non-NULL),
  followed by NULL-handling, zero-size, and the primary `memcmp`
  behavior. This pattern matches the C source's branching and lets
  WP discharge the structural cases automatically.

- `mem_secure_zero` and `mem_is_all` carry loop annotations — `loop
  invariant 0 <= i <= size`, `loop invariant \forall integer k; ...`,
  `loop assigns i, p[0 .. size - 1]`, `loop variant size - i`. WP
  discharges all loop-related goals automatically.

- All 27 functions specify `assigns` correctly: `\nothing` for
  predicates and pure functions, the modified byte range for
  destructive operations.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: Same as the other verified headers —
  compiles `require_msg` runtime NULL checks away; ACSL `requires`
  clauses provide static guarantees.

- **`-DNDEBUG`**: Standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/memory.h
```

Expected output: `Proved goals: 2805 / 2862` with 57 unproved goals
(53 timeouts + 4 unknown).

---

## core/arena.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | f53bddb (Canon-C CI #962)                       |
| **Functions**          | 22 of 22 non-macro functions annotated          |
| **Proof obligations**  | 3369 / 3472 discharged automatically (97.03%)   |
| **Timeouts**           | 103 (all documented under VERIFY-009)           |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 3369/3472 with 103 named goals expected   |
| **MC/DC coverage**     | 90.6% (58/64 condition outcomes — see MCDC-003) |
| **Line coverage**      | 100% (97/97)                                    |
| **CI artifact**        | `wp-proof-arena` (full per-goal breakdown)      |

### Function inventory

arena.h provides a linear bump allocator with explicit lifetime
control — 22 user-facing functions across init/reset, allocation,
zero variants, try variants, query, checkpoint/rollback, and byte
views.

**Group 1 — init / reset (3 functions):** `arena_init`, `arena_reset`,
`arena_reset_secure`. All three are 100% proved (no residuals).

**Group 2 — allocation (2 functions):** `arena_alloc`,
`arena_alloc_aligned`. These carry the bulk of arena.h's own
residuals (cats 2a and 2b in VERIFY-009).

**Group 3 — zero variants (2 functions):** `arena_alloc_zero`,
`arena_alloc_aligned_zero`. Wrapper-delegation residuals inherited
from Group 2 (cat 2c in VERIFY-009).

**Group 4 — try variants (2 functions):** `arena_try_alloc`,
`arena_try_alloc_aligned`. Wrapper-delegation residuals (cat 2c
continued).

**Group 5 — query (5 functions):** `arena_capacity`, `arena_remaining`,
`arena_used`, `arena_is_empty`, `arena_is_full`. All five are 100%
proved.

**Group 6 — checkpoint / rollback (2 functions):** `arena_mark`,
`arena_reset_to`. Both are 100% proved.

**Group 7 — byte views (3 functions):** `arena_as_bytes`,
`arena_buffer_bytes`, `arena_free_bytes`. The first two are 100%
proved; `arena_free_bytes` carries cat 2d's 2 residuals.

**Debug-only inline (1 function under `CANON_ARENA_DEBUG`):**
`arena_stats`. Conditionally compiled; verified when the flag is set,
absent otherwise.

**Type-safe allocation macros — documentation only:**
`arena_alloc_type`, `arena_alloc_array`, `arena_alloc_type_zero`,
`arena_alloc_array_zero`. Same verification rationale as VERIFY-007's
DEFINE_SLICE and VERIFY-008's mem_alloc_type family: ACSL inside
`#define` bodies is preprocessor-stripped before macro expansion, so
macro-generated code is documented but not directly WP-verified.
Validation is by unit testing on representative types in
`test/core/arena_test.c`.

Of the 22 user-facing functions, **10 are 100% proved** with no
residuals: `arena_init`, `arena_reset`, `arena_reset_secure`,
`arena_reset_to`, `arena_mark`, `arena_capacity`, `arena_remaining`,
`arena_used`, `arena_is_empty`, `arena_is_full`. The remaining 12
functions carry the 46 own residuals across cats 2a/2b/2c/2d.

### What is proved

- **ACSL predicates**: arena.h defines three named ACSL predicates:
  `is_power_of_two_logic(n)` (mirrors ptr.h's `is_power_of_two` C
  function inside the ACSL identifier namespace),
  `arena_invariant(a)` (validity of arena struct, capacity bound by
  `CANON_ARENA_MAX_SIZE`, offset bounded by capacity, buffer fully
  valid), and `arena_can_fit{L}(a, size, alignment)` (the predicate
  that captures whether a `size`-byte allocation at `alignment`
  alignment will fit). The `mark_in_arena` predicate composes
  `arena_invariant` with the constraint `mark <= a->offset`.

- **Functional correctness with complete behavior splits** (the
  bulk of the surface): `arena_alloc` and `arena_alloc_aligned` carry
  three-way splits (`size_zero` / `fits` / `does_not_fit`) with
  `complete` and `disjoint` behaviors. The init / reset / query /
  checkpoint groups carry `null_arena` / `non_null` or analogous
  two-way splits.

- **Invariant preservation**: WP proves `arena_invariant` is preserved
  by every public function that modifies the arena. This is the
  substrate fact MCDC-003 leans on to establish the structural
  unreachability of `arena_alloc`'s and `arena_alloc_aligned`'s line
  346/401 overflow-guard subconditions (cond 0 and cond 1).

- **Pointer validity on allocation results**: When `arena_alloc`
  succeeds, the returned pointer satisfies
  `\valid((u8*)\result + (0 .. size - 1))`. This is one of the four
  `fits_ensures` parts that fully discharge for non-residual cases.

- **Absence of runtime errors** (`-wp-rte`): WP proves no execution
  of any annotated function can trigger signed overflow, division by
  zero, invalid shifts, null dereference, or out-of-bounds access
  through the void* → u8* casts in `arena_alloc`'s pointer
  arithmetic.

- **Side-effect bounding**: Mutating functions specify
  `assigns *arena;` (and `assigns *arena, *out;` for the try
  variants). Read-only functions specify `assigns \nothing;`. WP
  discharges assigns clauses fully except for the 10 cat 2c
  wrapper-delegation residuals where the parent's assigns clause is
  itself partially unproved.

### Prover breakdown

| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 73               | (structural)      |
| Unreachable    | 78               | (structural)      |
| Qed (internal) | 2891             | 0.88ms–14ms       |
| Alt-Ergo 2.6.3 | 313              | 12ms–220ms        |
| CVC5 1.2.1     | 3                | 65ms–110ms        |
| Z3 4.15.2      | 11               | 14ms–62ms         |
| Timeout        | 96               | >120s             |
| Unknown        | 7                | (solver gave up)  |
| **Total**      | **3369 / 3472**  |                    |

The 103 unproved goals (96 `[Timeout]` + 7 `[Unknown]` + 0 `[Failed]`)
belong to eight categories — four arena.h-own (46 goals total) and
four inherited groups (57 goals total). The exact category and prover
breakdowns above reflect the typical distribution across CI runs; WP
may reclassify Timeout/Unknown goals between runs depending on solver
heuristics.

The CI wrapper sums Timeout + Unknown + Failed because what matters
is that the goal is not proved, not which category it lands in. The
named-goal enforcement (per-pattern check in the wrapper) catches
both count regressions and silent renames.

### Timeout goals (103)

All 103 are documented as triple-prover-resistant. The 57 inherited
goals are byte-identical to memory.h's full residual surface
(VERIFY-008's 31 inherited from substrate + 26 memory.h-own).
arena.h is the first downstream header to include memory.h in WP
scope, so memory.h's own residuals re-emerge here unchanged — every
inherited goal name matches an already-documented goal from VERIFY-002,
VERIFY-006, VERIFY-007, or VERIFY-008.

The 46 arena.h-own residuals split into four categories:

**Category 2a — arena_alloc / arena_alloc_aligned ptr_span call-site
preconditions (8):** Four per function, matching the goal-name
pattern `typed_cast_arena_alloc{_aligned}_call_ptr_span_requires{,_2,_3,_4}`.
arena_alloc's body computes the alignment pad through ptr_offset →
ptr_align_up → ptr_span; WP cannot reconstruct ptr_span's
preconditions through ptr_align_up's empty `nonnull` behavior. Same
structural pattern as VERIFY-006 cat 3 (ptr.h ptr_align_up
call-chain). See VERIFY-006's forward-implication note for the
substrate-side root cause.

**Category 2b — arena_alloc / arena_alloc_aligned fits / does_not_fit
ensures (26):** The deepest residual class. The behavioral contracts
on arena_alloc and arena_alloc_aligned relate the C bump-pointer
update to `arena_can_fit`'s let-bindings (`\let pad = (alignment -
(cur % alignment)) % alignment`); WP cannot prove the C pad
(computed through ptr_align_up's uintptr_t round-trip) equals the
ACSL pad (modular arithmetic). 13 residuals per function: 4 ×
`fits_ensures_part{2,3,4,5}` + 4 × `fits_ensures_2_part{2,3,4,5}` +
3 × `fits_ensures_3_part{2,3,4}` + 1 × `does_not_fit_ensures_part5` +
1 × `does_not_fit_ensures_2_part5`. The category exists because
arena.h made a deliberate spec choice: keep `arena_can_fit` in
readable mathematical form rather than rewriting it as bitwise-
axiomatic form matching ptr_align_up's body. The readable form
preserves auditability; the proof cost is the 26 residuals.

**Category 2c — zero / try wrappers (10):** Wrapper-delegation
residuals inherited from cat 2b through the four wrappers
`arena_alloc_zero`, `arena_alloc_aligned_zero`, `arena_try_alloc`,
`arena_try_alloc_aligned`. Each wrapper's 2–3-line body delegates to
arena_alloc / arena_alloc_aligned, picking up the parent's
fits-ensures and assigns residuals at the wrapper's own ensures
clauses. If cat 2b closes (via spec restructuring), cat 2c closes
with it.

**Category 2d — arena_free_bytes helper calls (2):**
`arena_free_bytes` returns either `bytes_empty()` or
`bytes_from(ptr_offset(buffer, offset), capacity - offset)`. WP
cannot reconstruct bytes_from's two `requires` clauses through
ptr_offset's empty `nonnull` behavior. Same root cause as cat 2a.

Full goal list, per-category manual proof arguments, and the
forward-implication trace from VERIFY-006's empty `nonnull` behaviors
to arena.h cats 2a/2c/2d: see VERIFY-009 in `docs/deviations.md`.

### MCDC-003 cross-reference (no closure check in this wrapper)

MCDC-003 documents 6 of arena.h's 64 condition outcomes as
unreachable: 4 overflow-guard subconditions in arena_alloc /
arena_alloc_aligned at lines 346 and 401 (structurally unreachable
under `arena_invariant` + `CANON_ARENA_MAX_SIZE = CANON_GB = 2^30`),
plus 2 release-build macro no-op artifacts on `_arena_debug_update`
calls at lines 356 and 411 (gcov-14 instrumentation artifact, not
real conditions).

Unlike slice.h's MCDC-002 closure block (which the CI wrapper verifies
with a `MCDC-002 functions with WP residuals: 0/4` diagnostic),
arena.h's WP step does *not* include an analogous MCDC-003 closure
check. The reason: slice.h's MCDC-002 functions (`bytes_slice`,
`bytes_skip`, `str_slice`, `str_skip`) are 100% proved — verifying
they don't appear in residuals is meaningful. For arena.h, MCDC-003's
affected functions (arena_alloc, arena_alloc_aligned) DO appear in
the residual list under cats 2a and 2b — they carry 13 WP-resistant
goals each. A "function not in residuals" check would always fail
and is not the right shape for arena.h's situation.

Cross-stream MCDC-003 evidence is provided differently: the WP
verification of `arena_invariant`-preservation across all alloc paths
establishes the substrate fact that combined with
`CANON_ARENA_MAX_SIZE`'s value yields the line 346/401 cond 0
unreachability. The line 346/401 cond 1 unreachability is purely
mathematical (independent of any constant). gcov's per-line debug
step surfaces the specific unreachable subconditions for human review
against `docs/deviations.md` MCDC-003.

### WP memory model note

arena.h uses `-wp-model Typed+Cast` for the same reason as compare.h,
ptr.h, slice.h, and memory.h: arena.h performs void* → u8* casts in
`arena_init` (the user-supplied buffer is `void*`, stored as `u8*`),
in the byte views (`arena_as_bytes`, `arena_buffer_bytes`,
`arena_free_bytes`), and indirectly through `ptr_offset` /
`ptr_align_up` in the allocators. The default `Typed` model rejects
these casts and every RTE mem_access goal becomes unprovable.
Soundness argument identical to VERIFY-005 through VERIFY-008:
callers supply correctly-typed pointers, and the casts preserve the
referenced byte range.

### ACSL contract conventions

- `arena_invariant` is the central named predicate. Every public
  function that takes a non-NULL Arena pointer carries
  `requires arena == \null || arena_invariant(arena);` (or just
  `requires arena_invariant(arena);` for functions that don't
  tolerate NULL, like arena_alloc). The predicate is preserved by
  every mutating function — WP discharges this fully.

- `arena_can_fit{L}(a, size, alignment)` uses ACSL let-bindings for
  readability. The trade-off is that WP must compose the let-bound
  expressions with the C body's bitwise alignment formula, producing
  cat 2b's 26 residuals. An alternative formulation using a function
  call (e.g. `pad_for(cur, alignment)`) instead of let-bindings was
  considered and rejected — see VERIFY-009 for the rationale.

- `arena_alloc` and `arena_alloc_aligned` carry three-way behavior
  splits: `size_zero` (assumes `size == 0`, ensures `\result ==
  \null`), `fits` (assumes `size > 0 && arena_can_fit(...)`, ensures
  valid pointer and bumped offset), `does_not_fit` (assumes
  `size > 0 && !arena_can_fit(...)`, ensures `\result == \null` and
  unchanged offset). All three are `complete` and `disjoint`.

- The try variants (`arena_try_alloc`, `arena_try_alloc_aligned`)
  carry a `null_out` / `non_null_out` split based on the `out`
  parameter's nullness. The contract pins the alignment between the
  bool result and the dereferenced out pointer:
  `\result <==> (*out != \null)`. This is what made closing line 510
  (the compound-return `out != NULL && p != NULL` aligned-variant
  test gap) the actionable improvement at CI #962 — see MCDC-003's
  history section.

- The `assigns` clauses are tight: mutating functions specify
  `assigns *arena;` (and `assigns *arena, *out;` for the try
  variants under `non_null_out`). Read-only functions specify
  `assigns \nothing;`. WP discharges all assigns clauses fully
  except for the 10 cat 2c wrapper-delegation residuals.

- All 22 functions are annotated; macro-generated typed allocators
  (`arena_alloc_type`, etc.) are documented but not directly
  verified, matching the VERIFY-007/008 macro-verification rationale.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: Same as the other verified headers —
  compiles `require_msg` runtime NULL checks away; ACSL `requires`
  clauses provide static guarantees.

- **`-DNDEBUG`**: Standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/arena.h
```

Expected output: `Proved goals: 3369 / 3472` with 103 unproved goals
(96 timeouts + 7 unknown). The 57 inherited goals are byte-identical
to memory.h's full residual surface (see VERIFY-009 inherited
residuals table); the 46 own goals split across cats 2a (8) / 2b (26)
/ 2c (10) / 2d (2).

---

## core/pool.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | b2644ba (Canon-C CI #972)                       |
| **Functions**          | 19 of 19 non-macro functions annotated          |
| **Proof obligations**  | 3775 / 3902 discharged automatically (96.74%)   |
| **Timeouts**           | 127 (all documented under VERIFY-010)           |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 3775/3902 with 127 named goals expected   |
| **MC/DC coverage**     | 91.2% (62/68 condition outcomes — see MCDC-004) |
| **Line coverage**      | 100% (74/74)                                    |
| **CI artifact**        | `wp-proof-pool` (full per-goal breakdown)       |

### Function inventory

pool.h provides a fixed-size object pool carved from a caller-supplied arena —
19 user-facing functions across init, allocation, try variants, indexed
access, reset, query, and byte views. It is the first Canon-C header whose
WP translation unit includes arena.h, so its run is the first to process the
arena → memory → ptr/slice/checked/contract substrate at two transitive hops.

**Group 1 — init (1 function):** `pool_init`. Carries cat 2a's 5
pool_invariant-establishment residuals.

**Group 2 — allocation (2 functions):** `pool_alloc`, `pool_alloc_zero`.
`pool_alloc` carries cat 2b ptr_elem residuals; `pool_alloc_zero` carries
cat 2c (mem_zero call-site) and cat 2d (through-effect) residuals.

**Group 3 — try variants (2 functions):** `pool_try_alloc`,
`pool_try_alloc_zero`. Both 100% proved (no residuals).

**Group 4 — indexed access (2 functions):** `pool_get`, `pool_get_const`.
Both carry cat 2b ptr_elem-cascade residuals.

**Group 5 — reset (2 functions):** `pool_reset`, `pool_reset_secure`. Both
carry cat 2d arena-delegation / reset-wrapper residuals.

**Group 6 — query (8 functions):** `pool_used`, `pool_capacity`,
`pool_remaining`, `pool_is_full`, `pool_is_empty`, `pool_object_size`,
`pool_memory_used`, `pool_memory_reserved`. All eight 100% proved.

**Group 7 — byte views (2 functions):** `pool_as_bytes`,
`pool_reserved_bytes`. Both carry cat 2c bytes_from call-site residuals.

**Type-safe macros — documentation only:** `pool_init_type`,
`pool_alloc_type`, `pool_alloc_type_zero`, `pool_get_type`,
`pool_get_type_const`. Same verification rationale as VERIFY-007's
DEFINE_SLICE and VERIFY-008/009's typed-macro families: ACSL inside `#define`
bodies is preprocessor-stripped before macro expansion, so macro-generated
code is documented but not directly WP-verified. Validation is by unit testing
on a representative type (Vec2 / i32) in `test/core/pool_test.c`.

Of the 19 user-facing functions, **10 are 100% proved** with no residuals:
the 8 query functions plus `pool_try_alloc` and `pool_try_alloc_zero`. The
remaining 9 (`pool_init`, `pool_alloc`, `pool_alloc_zero`, `pool_get`,
`pool_get_const`, `pool_as_bytes`, `pool_reserved_bytes`, `pool_reset`,
`pool_reset_secure`) carry the 24 own residuals across cats 2a/2b/2c/2d.

### What is proved

- **ACSL predicate**: pool.h defines `pool_invariant`, whose load-bearing
  conjunct is `end_mark - base_mark == capacity * object_size` together with
  the arena-validity entailment `arena_invariant(pool->arena)`. Every public
  function that takes a non-NULL Pool carries the invariant (or the
  null-tolerant `pool == \null || pool_invariant(pool)` form). WP proves the
  invariant is established by `pool_init` on success and preserved by
  `pool_reset` / `pool_reset_secure` — the `ensures_part4` arithmetic conjuncts
  are the cat 2a residuals, but the structural invariant (validity, the arena
  entailment) discharges fully, which is the substrate fact MCDC-004 leans on
  to establish the `!pool->arena` unreachability.

- **Functional correctness with complete behavior splits**: `pool_init`,
  `pool_alloc`, `pool_get`, `pool_get_const`, and the reset functions carry
  `complete` and `disjoint` behavior splits over their NULL / full / OOB /
  success cases. The query functions carry `null_pool` / `non_null` two-way
  splits.

- **Indexed-access bounds**: when `pool_get` / `pool_get_const` succeed, the
  returned slot satisfies `\valid` over the object's byte range. The
  `in_bounds_ensures_part4` validity conjuncts are the cat 2b residuals (the
  ptr_elem uintptr_t round-trip), but the runtime `require_msg` region check
  is the backstop exercised at all build levels.

- **Absence of runtime errors** (`-wp-rte`): WP proves no execution of any
  annotated function can trigger signed overflow, division by zero, invalid
  shifts, null dereference, or out-of-bounds access through the void* → u8*
  casts in the slot computation and byte views.

- **Side-effect bounding**: mutating functions specify `assigns *pool;` (and
  the arena region for the reset/secure wrappers); read-only functions specify
  `assigns \nothing;`. WP discharges the assigns clauses fully except for the
  cat 2c/2d wrapper-delegation residuals where the parent's clause is itself
  partially unproved.

### Prover breakdown

| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 77               | (structural)      |
| Unreachable    | 80               | (structural)      |
| Qed (internal) | 3201             | 0.67ms–63ms       |
| Alt-Ergo 2.6.3 | 396              | 16ms–908ms        |
| CVC5 1.2.1     | 2                | 81ms              |
| Z3 4.15.2      | 19               | 13ms–1s           |
| Timeout        | 123              | >120s             |
| Unknown        | 4                | (solver gave up)  |
| **Total**      | **3775 / 3902**  |                   |

The proved total breaks down as 77 Terminating + 80 Unreachable (structural)
+ 3201 Qed + 396 Alt-Ergo + 2 CVC5 + 19 Z3 = 3775. CVC5's 2 closures confirm
the triple-prover claim holds for pool.h: those goals were closed by neither
Alt-Ergo nor Z3. The full per-goal breakdown is recorded in the `wp-proof-pool`
CI artifact.

The CI wrapper sums Timeout + Unknown + Failed (123 + 4 + 0 = 127) because WP
may reclassify the same goal between Timeout and Unknown across runs depending
on solver heuristics; what matters is that the goal is not proved.

### Timeout goals (127)

All 127 are documented as triple-prover-resistant. They split into 103
inherited from substrate and 24 pool.h-own.

The **103 inherited** goals are byte-identical to arena.h's full residual
surface (VERIFY-009's 57 inherited from substrate + 46 arena.h-own). pool.h is
the first header to include arena.h in WP scope, so arena.h's own residuals
re-emerge here unchanged alongside the deeper substrate — every inherited goal
name matches an already-documented goal from VERIFY-002, VERIFY-006,
VERIFY-007, VERIFY-008, or VERIFY-009.

The **24 pool.h-own** residuals split into four categories:

**Category 2a — pool_invariant postcondition arithmetic at pool_init (5):**
`pool_init_ensures{,_2,_3}_part4`, `pool_init_ensures_4_part3`, and
`pool_init_call_arena_alloc_requires`. To discharge `pool_invariant`'s
`end_mark - base_mark == capacity * object_size` conjunct, WP must carry the
offset arithmetic across `arena_alloc`'s empty `nonnull` boundary and discharge
the nonlinear `capacity * object_size` product. Same class as ptr.h's
`align_up_ensures` (VERIFY-006 cat 2) and arena.h's `fits_ensures`
(VERIFY-009 cat 2b). The three `ensures_part4` goals are flagged
LIMITATION-SUSPECTED — their classification as pure WP-limitation versus an
over-strong invariant conjunct is pending a manual review (see VERIFY-010).

**Category 2b — ptr_elem cascade in pool_alloc / pool_get / pool_get_const
(6):** `pool_alloc_assert_rte_mem_access`, `pool_alloc_call_ptr_elem_requires`,
`pool_get_call_ptr_elem_requires`, `pool_get_in_bounds_ensures_part4`,
`pool_get_const_call_ptr_elem_const_requires`,
`pool_get_const_in_bounds_ensures_part4`. Each computes a slot via
`ptr_offset` → `ptr_elem`; WP cannot reconstruct ptr_elem's preconditions or
the `\result != \null` ensures through ptr.h's empty `nonnull` behaviors. The
VERIFY-006 empty-`nonnull` cascade re-emerging at pool.h's call sites.

**Category 2c — bytes_from / mem_zero call-sites (5):**
`pool_alloc_zero_call_mem_zero_requires`,
`pool_as_bytes_call_bytes_from_requires{,_2}`,
`pool_reserved_bytes_call_bytes_from_requires{,_2}`. Same root cause as
VERIFY-009 cat 2d (arena_free_bytes → bytes_from) and VERIFY-008: the
`ptr_offset` empty `nonnull` boundary leaves WP without the substrate facts to
reconstruct bytes_from's / mem_zero's preconditions.

**Category 2d — arena delegation + reset wrapper assigns/ensures (8):**
`pool_alloc_zero_assigns_normal_part3`,
`pool_reset_call_arena_reset_to_requires_2`,
`pool_reset_call_arena_alloc_requires`, `pool_reset_reset_ensures{,_2}_part3`,
`pool_reset_secure_assigns_{exit,normal}_part6`,
`pool_reset_secure_call_mem_secure_zero_requires`. All eight chain through
cats 2a/2b/2c via wrapper delegation, structurally identical to arena.h's
cat 2c. If cats 2a–2c close, cat 2d closes with them.

Full goal list, per-category manual proof arguments, and the two-hop
inheritance trace: see VERIFY-010 in `docs/deviations.md`. The CI artifact
`wp-proof-pool` contains the verbatim WP output for auditor inspection.

### MCDC-004 cross-reference (no closure check in this wrapper)

MCDC-004 documents 6 of pool.h's 68 condition outcomes as unreachable: the
`!pool->arena` middle subcondition of the defensive `if (!pool || !pool->arena
|| ...)` guard in `pool_get` (435), `pool_get_const` (464), `pool_as_bytes`
(541), `pool_reserved_bytes` (557), `pool_reset` (597), and `pool_reset_secure`
(644). A valid pool always has a valid arena (`pool_invariant` entails
`arena_invariant(pool->arena)`), and no public path constructs a pool with a
NULL arena, so the outcome cannot fire.

Like arena.h's MCDC-003, pool.h's WP step does *not* include an MCDC-closure
diagnostic of the slice.h MCDC-002 "0/N functions in residuals" shape. The
reason: the residual-bearing functions (`pool_init`, `pool_alloc`, `pool_get`,
`pool_get_const`) overlap with the unreachable-branch functions, so "function
absent from residuals" is the wrong predicate — `pool_get` / `pool_get_const`
appear in the residual list under cat 2b (the ptr_elem cascade on the success
path) while their `!pool->arena` defensive branch is separately discharged as
unreachable. Cross-stream MCDC evidence is provided by the per-line gcov debug
step in the coverage job, cross-referenced against WP's `Unreachable` count.

pool.h's gcov-measured MC/DC was 61/68 (89.7%) at the b2644ba baseline (four
gap-closure tests) and reached the documented 62/68 (91.2%) ceiling at CI #974
(98de378), which added `test_init_arena_alloc_fails_after_guard` to close the
line-309 reachable gap. See MCDC-004 for the closure history.

### WP memory model note

pool.h uses `-wp-model Typed+Cast` for the same reason as compare.h, ptr.h,
slice.h, memory.h, and arena.h: pool.h performs void* → u8* casts in the slot
computation (`ptr_offset` / `ptr_elem` over the reserved region) and in the
byte views (`pool_as_bytes`, `pool_reserved_bytes`). The default `Typed` model
rejects these casts and every RTE mem_access goal becomes unprovable.
Soundness argument identical to VERIFY-005 through VERIFY-009: callers supply
correctly-typed pointers, and the casts preserve the referenced byte range.

### ACSL contract conventions

- `pool_invariant` is the central named predicate. Every public function that
  takes a non-NULL Pool carries `requires pool_invariant(pool);` (or the
  null-tolerant disjunctive form for the query/byte-view functions). The
  invariant entails `arena_invariant(pool->arena)`, which is what discharges
  the `!pool->arena` defensive branches as unreachable (MCDC-004).

- The reservation size is computed through `checked_mul` (checked.h's verified
  primitive) rather than inline `object_size * capacity`, so pool_init inherits
  the VERIFY-002 overflow residual rather than minting a pool-specific one
  (counted in the inherited table, via the mem_alloc_array_checked class).

- `pool_init` carries a three-stage failure ladder: object-size alignment
  overflow, reservation-size `checked_mul` overflow, and the post-`arena_alloc`
  NULL guard (line 309). The third is reachable from an unaligned arena offset
  — see MCDC-004's line-309 closure — and is exercised by
  `test_init_arena_alloc_fails_after_guard`.

- All 19 functions specify `assigns` correctly: `*pool` (plus the arena region
  for the reset/secure wrappers) for mutating functions, `\nothing` for the
  query and byte-view functions. Macro-generated typed allocators are
  documented but not directly verified, matching the VERIFY-007/008/009
  macro-verification rationale.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: Same as the other verified headers — compiles
  `require_msg` runtime NULL checks away; ACSL `requires` clauses provide the
  static guarantees.

- **`-DNDEBUG`**: Standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/pool.h
```

Expected output: `Proved goals: 3775 / 3902` with 127 unproved goals (123
timeouts + 4 unknown). pool.h is the first header whose WP run processes a
two-hop transitive include (pool.h → arena.h → memory.h → ptr/slice/checked/
contract); the 3902 reflects the full translation unit. The 103 inherited
goals are byte-identical to arena.h's full residual surface (see VERIFY-010's
inherited-residuals table); the 24 own goals split across cats 2a (5) / 2b (6)
/ 2c (5) / 2d (8).

---

## core/region.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (report-only; enforcement deferred)    |
| **Baseline commit**    | c9172fc (Canon-C CI #992)                       |
| **Functions**          | 12 of 12 non-macro functions annotated          |
| **Proof obligations**  | 3517 / 3643 discharged automatically (96.54%)   |
| **Unproved**           | 126 (all documented under VERIFY-011)           |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | REPORT-ONLY — deferred pending count stability (see VERIFY-011) |
| **MC/DC coverage**     | 95.5% (21/22 condition outcomes — see MCDC-005) |
| **Line coverage**      | 100% (45/45)                                    |
| **CI artifact**        | `wp-proof-region` (full per-goal breakdown)     |

### Enforcement status

region.h's WP run is enforced **report-only**: the `frama-c-region` CI
step measures and prints the full residual list but does not fail the
build. This baseline is from the first observed completion of the
120s/`-wp-split` run. The step flips to exact-count enforcement once the
126 count and the 23 own goal names hold across 2-3 further runs — 19 of
the 23 own residuals are `-wp-split` fragments of region_end near the
120s boundary, the class most prone to Timeout/Proved flicker under
runner load. See VERIFY-011 for the full rationale.

### Function inventory

region.h provides stack-allocated lifetime tokens — 12 user-facing
non-macro functions across lifecycle, arena attachment, hook
registration, parent tracking, inspection, and lifetime assertions.

**Group 1 — lifecycle (2 functions):** `region_begin`, `region_end`.
`region_end` carries category 1's 19 opaque-hook-dispatch residuals;
`region_begin` carries one category 2 invariant-composition residual.

**Group 2 — arena attachment (1 function):** `region_attach_arena`.
One category 2 residual.

**Group 3 — hook registration (1 function):** `region_register`. One
category 2 residual.

**Group 4 — parent tracking (1 function, under
`!CANON_NO_REGION_PARENT`):** `region_set_parent`. One category 2
residual.

**Group 5 — inspection (4 functions):** `region_id`, `region_is_open`,
`region_has_parent`, `region_hook_count`. All 100% proved.

**Group 6 — lifetime assertions (3 functions):** `region_assert_open`,
`region_assert_borrow_valid`, `lifetime_assert_valid`. All 100% proved.

Of the 12 functions, **7 are 100% proved** (the 4 inspection functions
plus the 3 assertion functions). The remaining 5 (`region_begin`,
`region_end`, `region_attach_arena`, `region_register`,
`region_set_parent`) carry the 23 own residuals.

### What is proved

- **ACSL predicate**: region.h defines `region_invariant(r)` —
  `\valid(r) && 0 <= num_hooks <= REGION_MAX_CLEANUP && (arena == \null
  || arena_invariant(arena))` — composing arena.h's invariant for the
  optional attached arena. Every function taking a live Region carries
  it as precondition; mutators re-establish it (the category 2
  residuals are the arena_invariant-composition weight on this
  re-establishment).
- **Functional correctness with complete behavior splits**:
  `region_register` (full/has_space), `region_id`, `region_hook_count`
  (null/nonnull) carry `complete` and `disjoint` behavior splits, fully
  proved.
- **Absence of runtime errors** (`-wp-rte`): proved for all functions
  except the region_end opaque-call obligations (VERIFY-011 cat 1,
  including the `\valid_function` RTE check WP reports as
  not-yet-implemented).
- **Side-effect bounding**: region_end declares `assigns *r`; the
  structural postconditions are re-established by unconditional writes
  after the hook loop (see OWN-003). Read-only functions specify
  `assigns \nothing`.

### Prover breakdown

| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 80               | (structural)      |
| Unreachable    | 83               | (structural)      |
| Qed (internal) | 2988             | 0.62ms–35ms       |
| Alt-Ergo 2.6.3 | 350              | 17ms–342ms        |
| CVC5 1.2.1     | 4                | 81ms–278ms        |
| Z3 4.15.2      | 12               | 15ms–76ms         |
| Timeout        | 123              | >120s             |
| Unknown        | 3                | (solver gave up)  |
| **Total**      | **3517 / 3643**  |                   |

CVC5's 4 closures confirm the triple-prover claim holds for region.h.
The CI wrapper sums Timeout + Unknown + Failed (123 + 3 + 0 = 126).

### Unproved goals (126)

103 inherited from substrate (byte-identical to arena.h's full residual
surface) + 23 region.h-own in two categories: 19 region_end
opaque-hook-dispatch residuals (category 1) and 4 region_invariant
re-establishment residuals on the trivial mutators (category 2). Full
goal list, inheritance table, manual proof arguments, and the WP warning
text quoted as evidence: see VERIFY-011 in `docs/deviations.md`. The
architectural decision behind category 1 is OWN-003.

### MCDC-005 cross-reference

region.h's single uncovered condition outcome is the `if (h->fn)` FALSE
branch in region_end (line 496), API-unreachable because
region_register enforces `fn != NULL` and the dispatch loop visits only
filled slots. 95.5% (21/22) is the achievable ceiling. See MCDC-005.

### WP memory model note

region.h uses `-wp-model Typed+Cast` for the same reason as the rest of
the core/ stack: it composes arena.h (and transitively memory.h, ptr.h,
slice.h), whose void* → u8* casts require the model. Soundness argument
identical to VERIFY-005 through VERIFY-010.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: compiles `require_msg` runtime checks away;
  ACSL `requires` clauses provide the static guarantees.
- **`-DNDEBUG`**: standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  core/region.h
```

Expected output: `Proved goals: 3517 / 3643` with 126 unproved goals
(123 timeouts + 3 unknown). region.h's WP run processes a two-hop
transitive include (region.h → arena.h → memory.h → ...); the 103
inherited goals are byte-identical to arena.h's full residual surface;
the 23 own goals split across category 1 (19) / category 2 (4).

---

## Triple-prover rationale

Canon-C's verification baseline uses three SMT provers in sequence:
**Alt-Ergo 2.6.3**, **Z3 4.15.2**, and **CVC5 1.2.1**. WP invokes them
in that order, falling through on timeout. Each prover handles a
different class of goal well:

- **Alt-Ergo 2.6.3** — fastest on linear integer arithmetic and
  first-order ACSL contracts. Discharges the majority of non-Qed
  goals in milliseconds.
- **Z3 4.15.2** — stronger on nonlinear integer reasoning (e.g. the
  division-based overflow checks in `checked_mul_isize`) and on goals
  involving arrays and quantifiers.
- **CVC5 1.2.1** — contributes additional proofs on goals where
  Alt-Ergo and Z3 time out. Observed contributions: 3 goals on
  checked.h, 3 goals on ptr.h, 2 goals on memory.h, 3 goals on
  arena.h, 2 goals on pool.h, 4 on region.h discharged by CVC5 that neither Alt-Ergo nor Z3 could close.

The practical consequence: **every remaining unproved goal (2 on checked.h, 
15 on bits.h, 10 on ptr.h, 23 on slice.h, 57 on memory.h, 103 on arena.h, 
127 on pool.h, 126 on region.h) is demonstrated triple-prover-resistant**. This is a
stronger certification-evidence statement than dual-prover
resistance — the goals are genuinely limited by WP's encoding,
integer theory, or stdlib feature gaps, not by prover strength.

### CVC5 installation note

The opam `cvc5` package (currently 1.3.0) ships only the OCaml library
bindings used by Frama-C plugins, not the standalone CLI binary that
Why3 needs for `-wp-prover cvc5`. The Canon-C CI installs the prebuilt
CVC5 1.2.1 binary directly from the official cvc5 GitHub release and
registers it with Why3 using `why3 config add-prover` (Why3 1.7.2's
auto-detection regex only recognizes CVC5 1.0.x, so manual registration
is required). See the `frama-c` job in `.github/workflows/cmake-multi-platform.yml` for
the complete installation and registration procedure.

---

## Verification roadmap

### core/primitives/ (complete)

| Header       | Status           | Proved    | Notes                              |
|--------------|------------------|-----------|------------------------------------|
| checked.h    | ✅ Verified       | 1753/1755 | 2 manual discharges                |
| bits.h       | ✅ Verified       | 746/761   | 15 documented timeouts             |
| compare.h    | ✅ Fully verified | 208/208   | 100% automatic, 0 timeouts         |
| ptr.h        | ✅ Verified       | 1729/1739 | 10 documented timeouts (VERIFY-006); CI run reports 1943/1953 due to checked.h #include |
| types.h      | N/A              |           | Type definitions only              |
| limits.h     | N/A              |           | Constant definitions only          |
| lifetime.h   | N/A              |           | Layout convention only — three typedefs and one constant; no functions to verify. Exercised through borrow_test.c and per-container tests. |
| contract.h   | ✅ Annotated      |           | Handler contract used by ptr.h     |

### core/ (complete)

| Header       | Status           | Proved    | Notes                                                                  |
|--------------|------------------|-----------|------------------------------------------------------------------------|
| slice.h      | ✅ Verified       | 367/390   | 23 documented timeouts (VERIFY-007); MCDC-002 closed                   |
| memory.h     | ✅ Verified       | 2805/2862 | 57 documented timeouts (VERIFY-008); 31 inherited + 26 own             |
| arena.h      | ✅ Verified       | 3369/3472 | 103 documented timeouts (VERIFY-009); 57 inherited + 46 own; MCDC-003 |
| pool.h       | ✅ Verified       | 3775/3902 | 127 documented timeouts (VERIFY-010); 103 inherited + 24 own; MCDC-004 |
| region.h     | ✅ Verified       | 3517/3643 | 126 documented timeouts (VERIFY-011, report-only); 103 inherited + 23 own; MCDC-005 |
| scope.h      | N/A              |           | Macro-only header; DEFER expands at call sites, no static inline functions to verify. scope_test.c locks the exit-method table to regression tests. |
| ownership.h  | N/A              |           | Annotation macros expand to T (no behavior); DEFINE_OWNED(T)/DEFINE_BORROWED(T) generate verifiable functions per instantiation but follow the DEFINE_SLICE(T) disposition (VERIFY-007 macro-verification rationale). ownership_test.c covers Widget and Complex instantiations. |

### semantics/ (after core/ complete)

Result, Option, borrow, diag — planned after core/ layer is verified.

### data/ and algo/ (longer term)

Collections and algorithms — planned after semantics/ layer is verified.
