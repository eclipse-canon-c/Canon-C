# Verification Record

## Overview

This document records the formal verification status of Canon-C library
headers. Verification is performed using Frama-C WP (Weakest Precondition)
with ACSL contracts, enforced in CI on every push to master.

Combined verification status across all annotated headers:

| Metric               | Value                                            |
|----------------------|---------------------------------------------------|
| **Headers verified** | 5 (checked.h, bits.h, compare.h, ptr.h, slice.h) |
| **Functions**        | 119 annotated and verified                       |
| **Total obligations**| 4853                                              |
| **Proved automatic** | 4803 (98.97%)                                     |
| **Unproved**         | 50 (all documented, see per-header sections)      |
| **Prover setup**     | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1 (triple)  |

The slice.h baseline (367 / 390) carries a higher residual fraction
than the other four headers because it is the first Canon-C header
that crosses the C standard library boundary — it calls `memcmp` and
`strlen`, whose ACSL contracts require initialization and danglingness
reasoning that Frama-C 29 has not yet implemented. The 23 slice.h
residuals (5.9% of slice.h's obligations) are therefore not specific
to slice.h's design but are the cost of crossing into libc with the
current verifier; future headers using these functions will incur the
same residuals. See VERIFY-007 in `docs/deviations.md` for the full
classification.

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

This flag is applied to compare.h, ptr.h, and slice.h (all use void* →
typed pointer casts). checked.h and bits.h do not use void* parameters
and work correctly with the default `Typed` model.

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
| **Functions**          | 26 of 26 annotated                             |
| **Proof obligations**  | 1729 / 1739 discharged automatically (99.43%)   |
| **Timeouts**           | 10 (all documented under VERIFY-006)            |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 1729/1739 with 10 named goals expected    |
| **MC/DC coverage**     | 100% (42/42 condition outcomes)                 |
| **CI artifact**        | `wp-proof-ptr` (full per-goal breakdown)        |

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

- All functions specify `assigns \nothing;` (26 of 26). The
  `wp:pedantic-assigns` warnings observed in the CI output are
  advisory — they note that pointer-returning functions would benefit
  from `assigns \result \from ...` clauses for tighter caller
  assumptions. This is a precision-refinement opportunity for future
  work, not a soundness concern.

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

Expected output: `Proved goals: 1729 / 1739` with 10 timeouts.

---

## core/slice.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | (Canon-C CI #821)                               |
| **Functions**          | 17 of 17 non-macro functions annotated          |
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

**bytes_t (mutable byte view) — 8 functions, all annotated and proved:**
`bytes_from`, `bytes_empty`, `bytes_as_const`, `bytes_is_empty`,
`bytes_at`, `bytes_equal`, `bytes_slice`, `bytes_take`, `bytes_skip`.

**cbytes_t (read-only byte view) — 2 constructors, annotated and proved:**
`cbytes_from`, `cbytes_empty`.

**str_t (read-only character view) — 9 functions, annotated and proved:**
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

- **Functional correctness for non-libc functions** (12 of 17): Full
  behavioral specs with `complete` and `disjoint` behaviors on
  multi-case functions (`bytes_at`, `bytes_slice`, `bytes_skip`,
  `str_slice`, `str_skip`, `str_from_cstr`).

- **Partial functional correctness for libc-bridging functions** (5 of 17):
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

The 15 `Unreachable` goals include WP's discharge of the four
MCDC-002 `!ptr` defensive branches as unreachable under the type
invariant — the formal closure of the coverage gap that gcov cannot
exercise through the public API.

CVC5 1.2.1 is invoked as a tertiary prover but closes none of the 23
unproved goals — they are demonstrated triple-prover-resistant.

### Timeout goals (23)

All 23 are documented as triple-prover-resistant. They fall into three
categories:

**memcmp call-site preconditions (18):** Six per `bytes_equal` and
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

- All functions specify `assigns \nothing;` (17 of 17). The single
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
  checked.h and 3 goals on ptr.h discharged by CVC5 that neither
  Alt-Ergo nor Z3 could close.

The practical consequence: **every remaining unproved goal (2 on
checked.h, 15 on bits.h, 10 on ptr.h, 23 on slice.h) is demonstrated
triple-prover-resistant**. This is a stronger certification-evidence
statement than dual-prover resistance — the goals are genuinely
limited by WP's encoding, integer theory, or stdlib feature gaps,
not by prover strength.

### CVC5 installation note

The opam `cvc5` package (currently 1.3.0) ships only the OCaml library
bindings used by Frama-C plugins, not the standalone CLI binary that
Why3 needs for `-wp-prover cvc5`. The Canon-C CI installs the prebuilt
CVC5 1.2.1 binary directly from the official cvc5 GitHub release and
registers it with Why3 using `why3 config add-prover` (Why3 1.7.2's
auto-detection regex only recognizes CVC5 1.0.x, so manual registration
is required). See the `frama-c` job in `.github/workflows/ci.yml` for
the complete installation and registration procedure.

---

## Verification roadmap

### core/primitives/ (current)

| Header       | Status           | Proved    | Notes                              |
|--------------|------------------|-----------|------------------------------------|
| checked.h    | ✅ Verified       | 1753/1755 | 2 manual discharges                |
| bits.h       | ✅ Verified       | 746/761   | 15 documented timeouts             |
| compare.h    | ✅ Fully verified | 208/208   | 100% automatic, 0 timeouts         |
| ptr.h        | ✅ Verified       | 1729/1739 | 10 documented timeouts (VERIFY-006)|
| types.h      | N/A              |           | Type definitions only              |
| limits.h     | N/A              |           | Constant definitions only          |
| contract.h   | ✅ Annotated      |           | Handler contract used by ptr.h     |

### core/ (in progress)

| Header       | Status           | Proved    | Notes                                    |
|--------------|------------------|-----------|------------------------------------------|
| slice.h      | ✅ Verified       | 367/390   | 23 documented timeouts (VERIFY-007); MCDC-002 closed |
| arena.h      | Planned          |           | Memory region management                  |
| pool.h       | Planned          |           | Fixed-size block allocator                |
| memory.h     | Planned          |           | Allocation wrappers                       |
| region.h     | Planned          |           | Lifetime management                       |
| scope.h      | Planned          |           | Cleanup pairing                           |
| ownership.h  | N/A              |           | Annotation macros only, no logic          |

### semantics/ (after core/ complete)

Result, Option, borrow, diag — planned after core/ layer is verified.

### data/ and algo/ (longer term)

Collections and algorithms — planned after semantics/ layer is verified.
