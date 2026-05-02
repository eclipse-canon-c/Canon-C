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
`Typed+Cast`. The flag is applied only to compare.h, ptr.h (see
VERIFY-006), and slice.h (see VERIFY-007) — checked.h and bits.h use
the default `Typed` model. The difference is documented in the CI YAML
and in this deviations record. All 28 comparators have 100% MC/DC
coverage (8/8 condition outcomes) and pass fuzzing.

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

## VERIFY-007: WP Limitations on libc Boundary (slice.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-007 |
| **Date**       | 2026-05-02 |
| **Scope**      | slice.h — 23 goals across 3 categories |
| **Category**   | Formal verification completeness |

**Description**: 23 of 390 proof obligations (5.9%) are not discharged
by any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3
4.15.2 + CVC5 1.2.1) with a 120-second timeout and `-wp-model
Typed+Cast`. All 23 are triple-prover-resistant. The goals fall into
three categories:

1. **memcmp call-site preconditions** (20): Six per `bytes_equal`
   and `str_equal`, four per `str_starts_with` and `str_ends_with`.
   Goal-name pattern: `typed_cast_<func>_call_memcmp_requires_<X>`
   where `<X>` ranges over `valid_s1`, `valid_s2`, `initialization_s1`,
   `initialization_s2`, `danglingness_s1`, `danglingness_s2`. The
   `valid_*` obligations appear only on the bytes_t variants because
   the str_t variants close them through `str_valid` predicate
   reasoning; the `initialization_*` and `danglingness_*` obligations
   appear on all four equality functions.

2. **strlen valid_string precondition** (1):
   `typed_cast_str_from_cstr_call_strlen_requires_valid_string_s` —
   ACSL's `strlen` logic function requires `valid_string(s)` (a
   null-terminated string with valid memory through the terminator).
   `str_from_cstr`'s contract deliberately omits this precondition;
   see "Spec scope" below.

3. **Transitive contract.h handler non-termination** (2):
   `typed_cast_contract_default_handler_terminates`,
   `typed_cast_contract_default_handler_loop_invariant_established`
   — same two goals as VERIFY-006 category 4. slice.h includes
   contract.h transitively through the require_msg calls in its
   constructor functions, so the unprovable-by-construction goals
   are re-emitted in the slice.h proof run. These goals are not new
   slice.h residuals; they are the same goals counted for the second
   time.

Full goal list, per-goal Qed-and-prover timing, and the WP warning
text quoted as evidence: see docs/verification.md, slice.h section.

**Rationale**: Category 1 (memcmp) is a Frama-C WP feature gap that
WP itself reports during the proof run:

> [wp] FRAMAC_SHARE/libc/string.h:38: Warning:
>   Allocation, initialization and danglingness not yet implemented
>   (\dangling{L}((char *)s + i))

ACSL's `memcmp` contract requires the caller to establish that both
buffer ranges are fully valid, fully initialized, and non-dangling.
slice.h's `bytes_valid_write` and `str_valid` predicates establish
validity, but WP cannot discharge the `initialization` and
`danglingness` obligations because the underlying logic is, per the
WP warning, not yet implemented in Frama-C 29. Strengthening the
slice.h predicates would not close these goals; the verifier itself
cannot process the obligations.

Category 2 (strlen) is a deliberate spec-strength tradeoff. Adding
`requires valid_read_string(cstr)` would close the residual but
introduces a soundness dependency on Frama-C's `-frama-c-stdlib`
configuration that no other Canon-C header requires. The web research
record (see commit history of slice.h's annotation) documents that
`valid_read_string` and the `strlen` logic function have known
historical interaction issues under Typed+Cast. The cost of one
documented residual is much smaller than the cost of a project-wide
stdlib dependency for one function.

Category 3 is the deliberate ACSL idiom for non-returning functions
(see VERIFY-006 category 4) — the goals are the mathematical
statement of the handler's contract and are correct by construction.

**Spec scope**: Equality functions (`bytes_equal`, `str_equal`,
`str_starts_with`, `str_ends_with`) carry partial functional
specifications. The contracts prove range (`\result == \true ||
\result == \false`), structural properties (length-mismatch returns
false, same-pointer returns true, zero-length-prefix/suffix returns
true), and absence of runtime errors. Full equality semantics (the
"`memcmp == 0`" postcondition) are deferred to testing because the
memcmp axiomatic block needed to prove them is the same feature gap
documented under category 1. This follows the pattern set by
VERIFY-004 for bits.h's CLZ/CTZ/popcount range-only specs.

`str_from_cstr` carries a partial spec for the same reason — pointer
and length-pairing properties are proved, but `\result.len ==
strlen(cstr)` is not, because asserting it requires the strlen logic
function which is the same residual as category 2.

**Mitigation**: CI enforces exactly 23 unproved goals with the named
goal pattern. Any additional unproved goal or missing expected goal
is a regression and fails the build. slice.h achieves 93.1% MC/DC
coverage (54/58 condition outcomes) — the achievable ceiling under
the public-API constraint documented in MCDC-002. Equality functions
are validated by 90 unit tests in `test/core/slice_test.c` covering
identical content, distinct content, length mismatch, same-pointer
fast paths, and the symmetric branch-isolation cases for each `||`
expression. Fuzzing exercises every public function through randomly
constructed slice values via the `CANON_FUZZING` build of
`slice_test.c`.

**MCDC-002 closure (cross-reference)**: The four `!ptr` defensive
branches documented in MCDC-002 (in `bytes_slice`, `bytes_skip`,
`str_slice`, `str_skip`) are discharged by WP as unreachable under
the type invariant predicates. WP confirms this in the slice.h proof
run — none of the four functions appear in the unproved goal list,
which means WP successfully proved the `!ptr` branch unreachable
when the caller satisfies `bytes_invariant` / `str_invariant`. See
the MCDC-002 status update below for the formal closure.

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

## MCDC-002: API-Unreachable Defensive Branches (slice.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-002 |
| **Date**       | 2026-04-27 |
| **Status update** | 2026-05-02 — WP-discharged unreachable (see below) |
| **Scope**      | slice.h — `!ptr` left-side of OR in 4 functions |
| **Category**   | Coverage measurement methodology |

**Description**: 4 of 58 condition outcomes in `core/slice.h` cannot
be exercised in MC/DC isolation through the public API. All four are
the same structural pattern — the `!ptr` left-side of an OR-form
early-return:

```c
if (!s.ptr || cond) return ...;
```

The right-hand subcondition (`start >= b.len`, `n >= b.len`,
`start >= s.len`, `n >= s.len`) can be independently exercised with a
non-NULL ptr through `bytes_from(buf, 0)` or `str_from(buf, 0)`. The
left-hand `!s.ptr` subcondition can only be reached through
`bytes_empty()` or `str_empty()`, which produce `{NULL, 0}` — making
the right-hand subcondition also true. The two subconditions cannot
be flipped independently through the public API.

**Affected outcomes** (line numbers from `core/slice.h`, baseline
commit 2aba25e):

| # | Function           | Line | Branch                                |
|---|--------------------|------|---------------------------------------|
| 1 | `bytes_slice`      | 117  | `!b.ptr` true side of `\|\|`          |
| 2 | `bytes_skip`       | 130  | `!b.ptr` true side of `\|\|`          |
| 3 | `str_slice`        | 194  | `!s.ptr` true side of `\|\|`          |
| 4 | `str_skip`         | 207  | `!s.ptr` true side of `\|\|`          |

**Notable absences**: The same structural pattern appears in
`bytes_at`, `bytes_equal` (branch C), `str_starts_with`, `str_ends_with`,
`str_equal` (branch C), and the AND-form `slice_##T##_first` /
`slice_##T##_last`. Those are *not* listed in this deviation because
gcov-14's outcome-level measurement credits them as covered:

- For `bytes_at` and similar: the `cond` true case (with non-NULL ptr)
  and the `!ptr` true case (via `_empty()`) together cover both
  outcomes of the OR expression at the outcome level.
- For `bytes_equal` and `str_equal` branch C: tests
  `test_bytes_equal_one_null_distinct_ptrs` and
  `test_str_equal_one_null` reach this branch with one NULL and one
  non-NULL ptr at equal length.
- For `str_starts_with` / `str_ends_with`:
  `test_str_starts_with_zero_len_nonnull_prefix` and the symmetric
  `_ends_with_` test isolate the `len == 0` right-side from the
  `!ptr` left-side.
- For AND-form `_first`/`_last`:
  `test_slice_i32_first_zero_len_nonnull` /
  `test_slice_i32_last_zero_len_nonnull` flip the right-side
  independently of the left.

The 4 outcomes listed above are what remain after every reachable
gap has been closed.

**Rationale**: The `bytes_t`, `cbytes_t`, `str_t`, and
`slice_##type##_*` types have public struct fields (`{ptr, len}`).
This is a deliberate design choice — these types are the canonical
"borrow" primitive and must be cheap to construct and pattern-match
in caller code without going through opaque APIs. The cost of public
fields is that callers can construct malformed values like
`bytes_t b = {NULL, 5};` directly, bypassing the `bytes_from` contract.

The `!ptr` checks in `bytes_slice`, `bytes_skip`, `str_slice`, and
`str_skip` are defensive code against this case. They are correct,
necessary, and intentionally not exercisable from the public API in
MC/DC isolation — exercising them in isolation would require
constructing the very malformed values the API is designed to
prevent. Removing the checks to satisfy MC/DC would weaken the
library's robustness.

**Mitigation**:
1. The reachable subconditions of every compound check are tested
   independently. See `test/core/slice_test.c` — for each
   `if (!ptr || cond)` early return, there is a test exercising
   `cond` with a non-NULL ptr (e.g., `test_str_skip_all`,
   `test_str_starts_with_zero_len_nonnull_prefix`,
   `test_slice_i32_first_zero_len_nonnull`).

2. Fuzz testing exercises the defensive branches indirectly — the
   fuzz harness in `test/core/slice_test.c` constructs slices through
   the public API and feeds random inputs, so any code path reachable
   through the API is exercised by random values.

3. The condition outcomes covered by this deviation are NOT counted
   as a coverage regression. The achievable MC/DC ceiling for slice.h
   under the public-API constraint is `(58 - 4) / 58 ≈ 93.1%`.
   Reaching that ceiling — 54/58 — represents 100% of API-reachable
   coverage.

**Pattern note**: The same pattern (public `{ptr, len}` types with
`_empty()` constructors producing `{NULL, 0}`) recurs in other
Canon-C headers in the 70-80% MC/DC range — `arena.h` (73.2%),
`pool.h` (73.5%), `stringbuf.h` (74.2%), and others. Each will need
its own per-line audit (per the procedure validated here) before
opening analogous deviations. Numbers will differ — slice.h's
4-of-58 ratio (6.9% unreachable defensive) is not directly
transferable.

### Status update — 2026-05-02: WP-discharged unreachable

When slice.h was annotated with ACSL contracts and verified by WP
(see VERIFY-007), the four `!ptr` defensive branches listed above
were formally proved unreachable under the type invariant predicates
`bytes_invariant` and `str_invariant` (defined in slice.h):

```c
predicate bytes_invariant(bytes_t b) =
    b.ptr != \null || b.len == 0;

predicate str_invariant(str_t s) =
    s.ptr != \null || s.len == 0;
```

WP confirmed this on the first verification run (CI #821): none of
the four functions (`bytes_slice`, `bytes_skip`, `str_slice`,
`str_skip`) appear in the unproved goal list, which means WP
successfully discharged the `!ptr` branch as unreachable when the
caller satisfies the precondition `bytes_invariant(b)` or
`str_invariant(s)`. The CI wrapper for the slice.h WP step explicitly
checks this — see the `MCDC-002 functions with WP residuals: 0/4`
diagnostic line.

**Evidence stream alignment**:

- gcov MC/DC measurement remains 54/58 (93.1%). gcov instruments
  the source, not the proof — it counts the four branches as
  uncovered because no test reaches them. This does not change.

- WP verification proves the branches are unreachable for any
  caller satisfying the type invariant. This is a stronger
  statement than "API-unreachable in MC/DC isolation" — it says
  the branches are formally provably unexecutable under the
  documented preconditions, not merely difficult to reach in
  testing.

The two streams complement each other rather than converge. A
certification auditor reading both should conclude:

1. The 54/58 measurement is the ceiling reachable through testing
   under the public-API constraint, with the 4 missing outcomes
   documented and explained.
2. The 4 missing outcomes are not coverage gaps but provably
   unreachable code paths, with WP serving as the formal
   substitution for runtime exercise.

This satisfies DO-178C's intent for "deactivated code" or
"defensive code unreachable in normal operation" without requiring
the code to be removed: the verification framework provides the
unreachability evidence the testing framework cannot.

**Forward implication for arena.h, pool.h, stringbuf.h**: The same
pattern will recur. Each of those headers has analogous `_empty()`
constructors and analogous defensive branches. When they are
annotated, WP should discharge their analogous branches under
analogous type invariants. Until then, those headers' MC/DC ceilings
remain "API-unreachable" — the WP-discharged upgrade is per-header
and follows annotation, not preceding it.

**Verification status (cross-reference)**:
- Public-API reachable branches: covered (54/54)
- API-unreachable defensive branches: documented (4/4)
- Total proved or documented: 58/58 (100%)
- WP-discharged unreachable: 4/4 (since 2026-05-02)

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
