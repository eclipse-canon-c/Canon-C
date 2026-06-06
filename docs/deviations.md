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
VERIFY-006), slice.h (see VERIFY-007), memory.h (see VERIFY-008), and
arena.h (see VERIFY-009) — checked.h and bits.h use the default `Typed`
model. The difference is documented in the CI YAML and in this
deviations record. All 28 comparators have 100% MC/DC coverage (8/8
condition outcomes) and pass fuzzing.

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

**CI run note**: ptr.h's own baseline is 1729/1739 (10 residuals out
of 1739 ptr.h-own obligations). The CI WP step reports 1943/1953
because it runs Frama-C on ptr.h's translation unit, which includes
checked.h via `#include`. The +214 difference is checked.h's 12
division and modulo functions added at commit `c3df659` (CI #804,
Apr 27 2026); those obligations belong to checked.h (counted in its
own row) and are not ptr.h's. The CI wrapper enforces 1943/1953 as
the full-run figure but the substantive ptr.h baseline is 1729/1739.
The 10 named residuals are the same ptr.h goals before and after
c3df659 — none of the 214 inherited obligations entered the unproved
list. The CI wrapper emits a `WARNING: proved count changed from
expected 1729 / 1739` when the rebaseline first appeared and PASSes
because all 10 expected named ptr.h goals are still present.

**Forward-implication note on the empty `nonnull` behaviors**: ptr.h's
`ptr_align_up`, `ptr_align_down`, `ptr_offset`, `ptr_offset_const`, and
`ptr_retreat` declare `behavior nonnull: assumes p != \null;` with no
`ensures` clause. The empty body is deliberate — adding postconditions
like `\result != \null` or `\base_addr(\result) == \base_addr(p)` would
itself require WP to discharge the uintptr_t round-trip (the same
category 3 limitation above) on ptr.h's own bodies, which would
introduce new ptr.h residuals rather than close existing ones.
Downstream callers (arena.h in VERIFY-009 categories 2a and 2d, and
memory.h's mem_align variants in VERIFY-008 category 2b) cannot
reconstruct ptr_span's / bytes_from's call-site preconditions through
this empty behavior, producing call-chain residuals at their own
boundaries. The decision to leave the behavior empty is a deliberate
trade — keep ptr.h's residual list at 10 named goals, accept that
downstream headers inherit the cascade — and is recorded as forward
context for any future attempt to strengthen ptr.h's contracts.

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

## VERIFY-008: WP Limitations on Allocation, Alignment, and libc Boundary (memory.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-008 |
| **Date**       | 2026-05-09 |
| **Baseline commit** | b3e668b (Canon-C CI #841) |
| **Scope**      | memory.h — 57 goals across 7 categories (3 own + 4 inherited) |
| **Category**   | Formal verification completeness |

**Description**: 57 of 2862 proof obligations (2.0%) are not discharged
by any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3
4.15.2 + CVC5 1.2.1) with a 120-second timeout and `-wp-model
Typed+Cast`. All 57 are triple-prover-resistant. The goals split
cleanly into two top-level groups: **31 inherited from already-verified
substrate headers** (re-emerging because memory.h includes those
headers transitively) and **26 memory.h-own** residuals in three
categories.

This is the first Canon-C verification round where inherited
residuals exceed own residuals. The 31:26 ratio is a quantitative
expression of the composable-verification thesis: substrate residuals
propagate without amplification across composition layers, so a layer
that builds on five already-verified headers inherits their residual
fingerprints. memory.h does not introduce 31 new defects — it is the
first place where every previously-documented residual category
becomes simultaneously visible.

### Inherited residuals (31)

These goals are not memory.h defects. They re-emerge in the memory.h
proof run because memory.h includes ptr.h, checked.h, contract.h, and
slice.h transitively, and WP re-emits the relevant obligations at the
new call sites. Each is documented under its originating deviation;
the count column shows how many goals appear in the memory.h run.

| # | Source           | Goals | Pattern (within memory.h's run)                                   |
|---|------------------|-------|--------------------------------------------------------------------|
| 1 | VERIFY-002       | 2     | `typed_cast_checked_add_overflow_ensures`,                         |
|   |                  |       | `typed_cast_checked_add_u64_overflow_ensures`                      |
| 2 | VERIFY-006 cat 2 | 3     | `typed_cast_align_up_ensures`, `align_down_ensures`,               |
|   |                  |       | `align_padding_ensures`                                            |
| 3 | VERIFY-006 cat 3 | 3     | `typed_cast_ptr_align_up_call_align_up_requires_3`,                |
|   |                  |       | `ptr_align_padding_call_align_padding_requires_3`,                 |
|   |                  |       | `ptr_align_padding_nonnull_ensures_part2`                          |
| 4 | VERIFY-006 cat 4 | 2     | `typed_cast_contract_default_handler_terminates`,                  |
|   |                  |       | `contract_default_handler_loop_invariant_established`              |
| 5 | VERIFY-007 cat 1 | 20    | `typed_cast_bytes_equal_call_memcmp_requires_<aspect>` (6 goals),  |
|   |                  |       | `str_equal_call_memcmp_requires_<aspect>` (6),                     |
|   |                  |       | `str_starts_with_call_memcmp_requires_<aspect>` (4),               |
|   |                  |       | `str_ends_with_call_memcmp_requires_<aspect>` (4)                  |
| 6 | VERIFY-007 cat 2 | 1     | `typed_cast_str_from_cstr_call_strlen_requires_valid_string_s`     |

**Inherited subtotal: 31 goals.** These are the same goals counted in
the originating deviations; they are not new memory.h residuals. The
composable-verification claim is the empirical observation that this
count is byte-identical between memory.h round 2 (before
contract-shape fixes) and memory.h round 3 (after fixes) — the round
3 fix removed 10 contract-shape residuals and zero inherited ones,
confirming that hoisting non-overlap preconditions does not perturb
the substrate's residual surface.

### memory.h-own residuals (26)

Three categories, each rooted in a documented WP feature gap. None
are fixable by strengthening memory.h's contracts — the verifier
itself cannot process the obligations.

#### Category 1: \fresh / \freeable allocation reasoning (5)

| # | Goal                                                            |
|---|------------------------------------------------------------------|
| 1 | `typed_cast_mem_alloc_assigns_normal_part2`                      |
| 2 | `typed_cast_mem_alloc_nonzero_size_ensures_part2`                |
| 3 | `typed_cast_mem_free_assigns_normal`                             |
| 4 | `typed_cast_mem_free_call_free_requires_freeable`                |
| 5 | `typed_cast_mem_alloc_array_checked_nonoverflow_ensures_part3`   |

**Functions affected**: `mem_alloc`, `mem_free`,
`mem_alloc_array_checked`.

**Root cause**: Frama-C 29's libc spec for `malloc` and `free` uses
ACSL clauses that the verifier has not yet implemented. WP itself
reports the limitation during the proof run:

```
[wp] FRAMAC_SHARE/libc/stdlib.h:427: Warning:
  Allocation, initialization and danglingness not yet implemented
  (allocation: \fresh{Old, Here}(\at(\result,wp:post),\at(size,wp:pre)))

[wp] FRAMAC_SHARE/libc/stdlib.h:438: Warning:
  Allocation, initialization and danglingness not yet implemented
  (\freeable(p))

[wp] FRAMAC_SHARE/libc/stdlib.h:444: Warning:
  Allocation, initialization and danglingness not yet implemented
  (freed: \allocable(\at(p,wp:pre)))

[wp] core/memory.h:197: Warning:
  Allocation, initialization and danglingness not yet implemented
  (\fresh{Old, Here}(\at(\result,wp:post),\at(size,wp:pre)))

[wp] core/memory.h:215: Warning:
  Allocation, initialization and danglingness not yet implemented
  (\freeable(ptr))
```

The `\fresh{L1, L2}(p, n)` and `\freeable(p)` predicates are part of
ACSL's heap-state language. Frama-C 29 parses them but cannot
discharge proof obligations involving them. This is the same root
cause as VERIFY-007 category 1 (memcmp's `\dangling` and
`\initialization`) — different ACSL primitives, same underlying
limitation in Frama-C's allocation-and-danglingness theory.

**Manual proof argument**: For `mem_alloc`'s `nonzero_size_ensures_part2`,
the obligation reads "if `size > 0`, then `\result == \null` or
`\fresh{Old, Here}(\result, size)`". The C source exactly matches:
when `size > 0`, the function calls `malloc(size)` whose return value
is either NULL (on failure) or a freshly-allocated pointer to `size`
bytes. The match between code and contract is direct; only the
verifier's ability to discharge the `\fresh` clause is missing.

For `mem_free`'s `assigns_normal`, the obligation states the function
modifies only the heap region governed by `\freeable(ptr)`. The C
source calls `free(ptr)` directly, which has exactly that effect by
the C standard.

For `mem_alloc_array_checked`'s `nonoverflow_ensures_part3`, the
obligation chains through `mem_alloc`'s contract: when
`element_size * count` does not overflow, the function calls
`mem_alloc(total)` whose `\fresh` postcondition is propagated. The
chain is correct; the residual is the inherited `\fresh` limitation
plus one composition step.

**Verification**: 100% line coverage on all three functions. 113/128
MC/DC condition outcomes (88.3% — the missed branches are the
`require_msg` defensive checks in `mem_alloc_array_checked` that
`-DCANON_NO_REQUIRE` removes from the coverage build). Allocation
behavior tested by `test_mem_alloc_*`, `test_mem_free_*`, and
`test_mem_alloc_array_*` in `test/core/memory_test.c` — overflow
detection, NULL handling, zero-size handling, and round-trip
allocation-and-free are all exercised. ASan and UBSan verify
absence of leaks, double-free, and use-after-free.

#### Category 2: WP integer theory / bitwise alignment (9)

| # | Goal                                                          |
|---|----------------------------------------------------------------|
| 1 | `typed_cast_mem_align_normal_ensures_part3`                    |
| 2 | `typed_cast_mem_align_normal_ensures_2_part3`                  |
| 3 | `typed_cast_mem_align_to_normal_ensures_part3`                 |
| 4 | `typed_cast_mem_align_to_normal_ensures_2_part3`               |
| 5 | `typed_cast_mem_is_aligned_nonnull_aligned_ensures`            |
| 6 | `typed_cast_mem_is_aligned_nonnull_unaligned_ensures`          |
| 7 | `typed_cast_mem_get_alignment_assert_rte_signed_overflow`      |
| 8 | `typed_cast_mem_get_alignment_nonnull_ensures_part2`           |
| 9 | `typed_cast_mem_get_alignment_nonnull_ensures_2_part2`         |

**Functions affected**: `mem_align`, `mem_align_to`, `mem_is_aligned`,
`mem_get_alignment`.

**Root cause**: WP's integer theory cannot bridge bitwise alignment
formulas (e.g. `(addr & (alignment - 1)) == 0` for power-of-2
alignment) with the modular-arithmetic formulation
(`addr % alignment == 0`). This is the same limitation documented in
VERIFY-006 category 2 (ptr.h's `align_up`, `align_down`,
`align_padding`). memory.h's wrappers (`mem_align`, `mem_align_to`,
`mem_is_aligned`, `mem_get_alignment`) re-emit the limitation at the
memory.h call sites because they reformulate the alignment ensures
clauses in terms of `% alignment` (the natural mathematical
formulation in an `ensures` clause) while the implementations use
bitwise operations (the natural C idiom).

The `mem_get_alignment_assert_rte_signed_overflow` goal is a related
RTE check on the `(uintptr_t)(-(intptr_t)addr)` expression that
extracts the lowest set bit. WP cannot prove the negation does not
overflow under the `Typed+Cast` model because the cast round-trip
loses the integer-bound information.

**Manual proof argument**: For `mem_align_normal_ensures_part3`, the
obligation is "if `size > 0` and `size <= USIZE_MAX - (CANON_DEFAULT_ALIGN - 1)`,
then `\result % CANON_DEFAULT_ALIGN == 0`". The implementation calls
`align_up(size, CANON_DEFAULT_ALIGN)` whose definition is
`(size + (a - 1)) & ~(a - 1)` for power-of-2 `a`. By the
power-of-2 mask identity, the result is divisible by `a`. The
implementation is correct; the proof obstacle is WP's inability to
discharge the bitwise→modular bridge, which is exactly VERIFY-006's
documented limitation for `align_up_ensures`.

For `mem_is_aligned_nonnull_aligned_ensures`, the obligation is "if
`ptr != \null` and `is_aligned_addr(ptr, alignment)`, then
`\result == \true`". The implementation calls
`ptr_is_aligned(ptr, alignment)` which checks
`((uintptr_t)ptr & (alignment - 1)) == 0`. The predicate
`is_aligned_addr` was defined in memory.h with this exact body, but
WP's reasoning about pointer-to-integer round-trips under
`Typed+Cast` cannot connect the predicate body to the runtime check.

**Verification**: 88.3% MC/DC on the alignment functions (113/128
across all of memory.h, with most of the alignment-function
condition outcomes covered). Exhaustive alignment test vectors in
`test/core/memory_test.c` cover all four functions with
representative alignments (1, 2, 4, 8, 16, 64, 4096, CANON_DEFAULT_ALIGN)
and boundary inputs (size = 0, size = 1, size near USIZE_MAX). The
underlying primitives (`align_up`, `is_power_of_two`,
`ptr_is_aligned`) are tested independently in
`test/core/primitives/ptr_test.c`.

#### Category 3: memcmp call-site inheritance (12)

| #  | Goal                                                                       |
|----|-----------------------------------------------------------------------------|
| 1  | `typed_cast_mem_compare_call_memcmp_requires_initialization_s1`             |
| 2  | `typed_cast_mem_compare_call_memcmp_requires_initialization_s2`             |
| 3  | `typed_cast_mem_compare_call_memcmp_requires_danglingness_s1`               |
| 4  | `typed_cast_mem_compare_call_memcmp_requires_danglingness_s2`               |
| 5  | `typed_cast_mem_equal_call_memcmp_requires_initialization_s1`               |
| 6  | `typed_cast_mem_equal_call_memcmp_requires_initialization_s2`               |
| 7  | `typed_cast_mem_equal_call_memcmp_requires_danglingness_s1`                 |
| 8  | `typed_cast_mem_equal_call_memcmp_requires_danglingness_s2`                 |
| 9  | `typed_cast_mem_equal_bytes_call_memcmp_requires_initialization_s1`         |
| 10 | `typed_cast_mem_equal_bytes_call_memcmp_requires_initialization_s2`         |
| 11 | `typed_cast_mem_equal_bytes_call_memcmp_requires_danglingness_s1`           |
| 12 | `typed_cast_mem_equal_bytes_call_memcmp_requires_danglingness_s2`           |

**Functions affected**: `mem_compare`, `mem_equal`, `mem_equal_bytes`.

**Root cause**: Identical to VERIFY-007 category 1. ACSL's `memcmp`
contract requires the caller to establish that both buffer ranges
are fully valid, fully initialized, and non-dangling. memory.h's
`mem_valid_read` predicate establishes validity, but the
`initialization` and `danglingness` obligations cannot be discharged
because the underlying logic — quoted from WP's own warning during
the slice.h proof run — is "not yet implemented" in Frama-C 29:

```
[wp] FRAMAC_SHARE/libc/string.h:38: Warning:
  Allocation, initialization and danglingness not yet implemented
  (\dangling{L}((char *)s + i))
```

memory.h directly calls `memcmp` from `mem_compare`, `mem_equal`,
and `mem_equal_bytes`, so the same residual class re-emerges at
memory.h's call sites. Note the count: 4 residuals per function ×
3 functions = 12. This is fewer per function than slice.h's 6
(for `bytes_equal` and `str_equal`) because the `valid_*` aspect
discharges through `mem_valid_read` even when the
`initialization`/`danglingness` aspects do not — memory.h's
predicate is shaped slightly differently than slice.h's,
specifically tuned to the call-site requirements.

**Manual proof argument**: For `mem_compare_call_memcmp_requires_initialization_s1`,
the obligation is "before calling `memcmp(a, b, size)`, every byte
in `[a, a + size)` is initialized". memory.h's contract requires
`mem_valid_read((void *)a, (integer)size)` which establishes that
the byte range is readable in the program memory state. C99's
read-only semantics for `memcmp` accept arbitrary readable bytes —
the function does not require the bytes to be initialized in any
particular sense; uninitialized bytes are read as
implementation-defined values that the function compares
byte-by-byte. The ACSL contract is stricter than C99 requires; the
unproved obligation reflects this strictness, not a real soundness
gap.

**Verification**: 88.3% MC/DC on memory.h overall, with all branches
in `mem_compare`, `mem_equal`, and `mem_equal_bytes` covered. 90+
unit tests in `test/core/memory_test.c` cover identical content,
distinct content, length mismatch, NULL handling, zero-size handling,
and same-pointer fast paths. Fuzzing exercises every public function
through randomly constructed byte buffers. Valgrind verifies absence
of uninitialized-byte reads in real execution.

### Summary of memory.h-own residuals

| Category | Goals | Functions affected | WP feature gap |
|----------|-------|--------------------|--------------------------------|
| 2a       | 5     | mem_alloc/free/array_checked | `\fresh`, `\freeable` |
| 2b       | 9     | mem_align*, mem_is_aligned, mem_get_alignment | bitwise-alignment integer theory |
| 2c       | 12    | mem_compare/equal/equal_bytes | `\dangling`, `\initialization` |
| **Total**| **26**|                              |                                |

All 26 residuals would be discharged by improvements to Frama-C's
allocation-and-danglingness theory or its integer theory. None
require contract changes from memory.h. Strengthening memory.h's
predicates would produce no improvement; the verifier itself cannot
process the obligations.

### Mitigation

CI enforces exactly 57 unproved goals with the named goal patterns
covering all 31 inherited and all 26 memory.h-own residuals. Any
additional unproved goal or missing expected goal is a regression
and fails the build. The exact-count enforcement with named
patterns means a renamed goal (silent regression: a contract was
weakened in a way that produces a new residual under a different
name) is as much a failure as a count change — the wrapper looks
for each named pattern individually.

memory.h achieves 88.3% MC/DC coverage (113/128 condition outcomes)
— an increase from the 82.0% baseline before the Phase 1 refactor
that routed `mem_alloc_array` through the new
`mem_alloc_array_checked` function (2026-05-09 push). The 15
remaining missed outcomes are defensive `require_msg` checks under
`-DCANON_NO_REQUIRE`, the same pattern as in checked.h, ptr.h, and
slice.h — they are the `require_msg` infrastructure that ACSL
preconditions provide statically and which the coverage build
removes. This is the same coverage methodology documented in
MCDC-001.

memory.h is the first Canon-C header to demonstrate the composable-
verification thesis quantitatively. The 31 inherited residuals
propagated unchanged from the substrate (ptr.h's 8, checked.h's 2,
slice.h's 21 — all visible in memory.h's 2862-goal run). The 26
memory.h-own residuals fall into three categories that are each
rooted in a Frama-C feature gap, not in memory.h's design, and
each category has a known-good mitigation strategy (allocation
testing, alignment vectors, libc compatibility under valgrind).

The composable-verification claim — that substrate residuals
propagate without amplification — is now empirically supported
by two data points: ptr.h → slice.h (where slice.h inherited
ptr.h's 2 contract-handler residuals unchanged) and ptr.h/checked.h/
slice.h → memory.h (where memory.h inherited 31 substrate residuals
unchanged across two proof rounds). The arena.h verification (see
VERIFY-009, shipped at CI #962) provides the third data point:
arena.h inherits memory.h's full 57-goal residual surface
byte-identically (zero new substrate residuals introduced), confirming
that the propagation-without-amplification property extends through
one more composition layer.

### Cross-references

- Inherited residuals: VERIFY-002 (checked.h), VERIFY-006 (ptr.h),
  VERIFY-007 (slice.h).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag).
- Downstream confirmation: VERIFY-009 (arena.h inherits all 57
  memory.h residuals unchanged).
- Composable verification thesis: see README, "Composable
  verification" section.
- Per-goal CI artifact: `wp-proof-memory` (full WP output, including
  goal-by-goal classification and Qed-and-prover timing).
- Wrapper enforcement: `.github/workflows/cmake-multi-platform.yml`,
  step "WP: core/memory.h".

---

## VERIFY-009: WP Limitations Inherited from Substrate Plus ptr_span/Arithmetic-Chain Residuals (arena.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-009 |
| **Date**       | 2026-05-24 |
| **Baseline commit** | f53bddb (Canon-C CI #962) |
| **Scope**      | arena.h — 103 goals across 8 categories (4 own + 4 inherited groups) |
| **Category**   | Formal verification completeness |

**Description**: 103 of 3472 proof obligations (2.97%) are not
discharged by any prover in the triple-prover configuration (Alt-Ergo
2.6.3 + Z3 4.15.2 + CVC5 1.2.1) with a 120-second timeout and
`-wp-model Typed+Cast`. All 103 are triple-prover-resistant. The goals
split cleanly into two top-level groups: **57 inherited from
already-verified substrate headers** (re-emerging because arena.h
includes memory.h, which transitively pulls in ptr.h, slice.h,
checked.h, and contract.h) and **46 arena.h-own** residuals in four
categories.

arena.h extends the composable-verification thesis to a third data
point. memory.h established it (31 inherited + 26 own); arena.h
re-confirms it (57 inherited + 46 own). The full 57-goal residual
surface from VERIFY-008 re-emerges byte-identically in the arena.h
proof run — zero new substrate residuals are introduced at the arena.h
boundary. Every inherited residual matches an already-documented goal
by name; the wrapper enforces this with per-goal pattern checks.

### Inherited residuals (57)

These goals are not arena.h defects. They re-emerge in the arena.h
proof run because arena.h includes memory.h, which transitively
includes ptr.h, slice.h, checked.h, and contract.h. WP re-emits the
relevant obligations at the new call sites. The full memory.h residual
set (VERIFY-008's 31 inherited + 26 own = 57 total) is re-emitted as
arena.h's inherited surface — arena.h, the first header to include
memory.h transitively in WP scope, demonstrates that memory.h's
residuals propagate as a unit just as the headers below it do.

| # | Source            | Goals | Notes                                                            |
|---|-------------------|-------|-------------------------------------------------------------------|
| 1 | VERIFY-002        | 2     | checked.h u64 add overflow — via mem_alloc_array_checked          |
| 2 | VERIFY-006 cat 2  | 3     | ptr.h align_up/down/padding integer theory                        |
| 3 | VERIFY-006 cat 3  | 3     | ptr.h ptr_align_* call-chain                                      |
| 4 | VERIFY-006 cat 4  | 2     | contract.h handler non-termination                                |
| 5 | VERIFY-007 cat 1  | 20    | slice.h memcmp call-site preconditions (bytes/str equality, etc.) |
| 6 | VERIFY-007 cat 2  | 1     | slice.h str_from_cstr strlen valid_string                         |
| 7 | VERIFY-008 cat 1  | 5     | memory.h \fresh / \freeable (mem_alloc, mem_free, array_checked)  |
| 8 | VERIFY-008 cat 2  | 9     | memory.h integer theory (mem_align, mem_is_aligned, etc.)         |
| 9 | VERIFY-008 cat 3  | 12    | memory.h memcmp call-site (mem_compare, mem_equal, equal_bytes)   |

**Inherited subtotal: 57 goals.** Byte-identical to memory.h's full
residual list. The composable-verification claim, now empirically
supported at three composition layers (ptr.h → slice.h with 2
inherited; ptr.h/checked.h/slice.h → memory.h with 31 inherited;
memory.h+substrate → arena.h with 57 inherited), is that substrate
residuals propagate without amplification: a downstream header's
inherited count equals the upstream's total, not greater.

### arena.h-own residuals (46)

Four categories. Cats 2a and 2d are call-chain residuals at ptr.h
boundary functions whose `nonnull` behavior carries no `ensures` clause
(see VERIFY-006 forward-implication note). Cat 2b is the
arithmetic-chain residual at arena_alloc / arena_alloc_aligned. Cat 2c
inherits from cat 2b through wrapper delegation.

#### Category 2a: ptr_span call-site preconditions in arena_alloc / arena_alloc_aligned (8)

| # | Goal                                                      |
|---|------------------------------------------------------------|
| 1 | `typed_cast_arena_alloc_call_ptr_span_requires`            |
| 2 | `typed_cast_arena_alloc_call_ptr_span_requires_2`          |
| 3 | `typed_cast_arena_alloc_call_ptr_span_requires_3`          |
| 4 | `typed_cast_arena_alloc_call_ptr_span_requires_4`          |
| 5 | `typed_cast_arena_alloc_aligned_call_ptr_span_requires`    |
| 6 | `typed_cast_arena_alloc_aligned_call_ptr_span_requires_2`  |
| 7 | `typed_cast_arena_alloc_aligned_call_ptr_span_requires_3`  |
| 8 | `typed_cast_arena_alloc_aligned_call_ptr_span_requires_4`  |

**Functions affected**: `arena_alloc`, `arena_alloc_aligned`.

**Root cause**: arena_alloc's body computes the alignment pad through
three ptr.h calls:

```c
current     = ptr_offset(arena->buffer, arena->offset);
aligned_ptr = ptr_align_up(current, CANON_DEFAULT_ALIGN);
pad         = ptr_span(aligned_ptr, current);
```

ptr_span's four `requires` clauses are: `\valid_read((char*)to)`,
`\valid_read((char*)from)`, `\base_addr((char*)to) ==
\base_addr((char*)from)`, and `(char*)to >= (char*)from`. WP has to
discharge each at the call site by tracing back through
`ptr_align_up`'s and `ptr_offset`'s postconditions. Both of those
functions declare `behavior nonnull: assumes p != \null;` with no
`ensures` clause (see VERIFY-006 forward-implication note) — by
design, because adding postconditions like `\base_addr(\result) ==
\base_addr(p)` would require WP to discharge the uintptr_t round-trip
in ptr.h's own bodies, which is the VERIFY-006 cat 3 limitation. The
empty behavior leaves WP without the substrate facts it would need to
reconstruct ptr_span's preconditions in arena.h.

Strengthening arena.h's contracts cannot close these residuals; the
facts needed are at ptr.h's boundary, not arena.h's. Strengthening
ptr.h would shift residuals from arena.h's 8 to ptr.h's
own — same total cost, different attribution — and would do nothing
for the underlying uintptr_t-round-trip limitation. This is the same
trade documented in VERIFY-006's forward-implication note.

**Manual proof argument**: arena_invariant entails
`\valid(arena->buffer + (0 .. arena->capacity - 1))`. Under the
overflow guard `offset + pad + size > capacity → return NULL`, the
arena_alloc bodies reach ptr_span with `current = buffer + offset`
and `aligned_ptr = buffer + offset + pad` (post-alignment), both
within the buffer's valid range and same-base-address as the buffer.
The C semantics match ptr_span's preconditions exactly; the proof
obstacle is WP's inability to track this through the uintptr_t casts
in ptr_align_up's body. Same root cause as VERIFY-006 cat 3.

#### Category 2b: arena_alloc / arena_alloc_aligned fits / does_not_fit ensures (26)

| #  | Goal                                                                     |
|----|--------------------------------------------------------------------------|
| 1  | `typed_cast_arena_alloc_fits_ensures_part2`                              |
| 2  | `typed_cast_arena_alloc_fits_ensures_part3`                              |
| 3  | `typed_cast_arena_alloc_fits_ensures_part4`                              |
| 4  | `typed_cast_arena_alloc_fits_ensures_part5`                              |
| 5  | `typed_cast_arena_alloc_fits_ensures_2_part2`                            |
| 6  | `typed_cast_arena_alloc_fits_ensures_2_part3`                            |
| 7  | `typed_cast_arena_alloc_fits_ensures_2_part4`                            |
| 8  | `typed_cast_arena_alloc_fits_ensures_2_part5`                            |
| 9  | `typed_cast_arena_alloc_fits_ensures_3_part2`                            |
| 10 | `typed_cast_arena_alloc_fits_ensures_3_part3`                            |
| 11 | `typed_cast_arena_alloc_fits_ensures_3_part4`                            |
| 12 | `typed_cast_arena_alloc_does_not_fit_ensures_part5`                      |
| 13 | `typed_cast_arena_alloc_does_not_fit_ensures_2_part5`                    |
| 14 | `typed_cast_arena_alloc_aligned_fits_ensures_part2`                      |
| 15 | `typed_cast_arena_alloc_aligned_fits_ensures_part3`                      |
| 16 | `typed_cast_arena_alloc_aligned_fits_ensures_part4`                      |
| 17 | `typed_cast_arena_alloc_aligned_fits_ensures_part5`                      |
| 18 | `typed_cast_arena_alloc_aligned_fits_ensures_2_part2`                    |
| 19 | `typed_cast_arena_alloc_aligned_fits_ensures_2_part3`                    |
| 20 | `typed_cast_arena_alloc_aligned_fits_ensures_2_part4`                    |
| 21 | `typed_cast_arena_alloc_aligned_fits_ensures_2_part5`                    |
| 22 | `typed_cast_arena_alloc_aligned_fits_ensures_3_part2`                    |
| 23 | `typed_cast_arena_alloc_aligned_fits_ensures_3_part3`                    |
| 24 | `typed_cast_arena_alloc_aligned_fits_ensures_3_part4`                    |
| 25 | `typed_cast_arena_alloc_aligned_does_not_fit_ensures_part5`              |
| 26 | `typed_cast_arena_alloc_aligned_does_not_fit_ensures_2_part5`            |

**Functions affected**: `arena_alloc`, `arena_alloc_aligned`. 13 per
function: 4 × `fits_ensures_part{2,3,4,5}` + 4 ×
`fits_ensures_2_part{2,3,4,5}` + 3 × `fits_ensures_3_part{2,3,4}` +
1 × `does_not_fit_ensures_part5` + 1 ×
`does_not_fit_ensures_2_part5`.

**Root cause**: The behavioral contracts on arena_alloc and
arena_alloc_aligned use the `arena_can_fit` predicate, which is
defined with let-bindings:

```c
predicate arena_can_fit{L}(Arena *a, integer size, integer alignment) =
    \let cur = a->offset;
    \let pad = (alignment - (cur % alignment)) % alignment;
    cur <= CANON_USIZE_MAX - pad &&
    cur + pad <= CANON_USIZE_MAX - size &&
    cur + pad + size <= a->capacity;
```

The `fits` behavior's ensures clauses (`\result != \null`,
`\valid((u8*)\result + (0 .. size - 1))`, `arena->offset >=
\old(arena->offset) + size`, `arena_invariant(arena)`) relate the
post-state offset to the predicate's `\let pad = ...` expression. The
C body computes pad via `pad = ptr_span(aligned_ptr, current)` where
`aligned_ptr = ptr_align_up(current, alignment)`.

To discharge the ensures clauses, WP must prove that the C pad
(computed through ptr_align_up's uintptr_t round-trip) equals the
ACSL pad (computed through the modular-arithmetic expression
`(alignment - (cur % alignment)) % alignment`). This is the same
bitwise-to-modular bridge documented in VERIFY-006 cat 2 (align_up's
ensures clause), applied across two levels of composition (the
predicate's let-binding plus arena.h's call-site bump-pointer update).

The `does_not_fit` behavior's two `part5` residuals are the symmetric
case: when `!arena_can_fit(...)`, the contract states `\result ==
\null` and `arena->offset == \old(arena->offset)`. WP cannot discharge
these because it cannot establish that the C compound-or guard
`offset > USIZE_MAX - pad || offset + pad > USIZE_MAX - size || offset
+ pad + size > capacity` is equivalent to the negation of
`arena_can_fit`. The equivalence holds mathematically but requires the
same pad-equality WP cannot prove for the `fits` direction.

This is the deepest residual class in arena.h. The category exists
because Canon-C made a deliberate spec choice: keep the
`arena_can_fit` predicate readable in the natural mathematical form
(let-bindings + modular arithmetic) rather than rewriting it as a
sequence of axioms that match ptr_align_up's bitwise body. The
readable form preserves auditability for human reviewers; the proof
cost is the 26 residuals.

**Manual proof argument**: arena_invariant entails `offset <= capacity
<= CANON_ARENA_MAX_SIZE`. When `arena_can_fit(arena, size, alignment)`
holds, the predicate's let-bindings establish `cur + pad + size <=
capacity`, so the C compound guard returns false and the function
proceeds to update offset by `pad + size`. The new offset equals
`cur + pad + size`, which is `\old(arena->offset) + pad + size >=
\old(arena->offset) + size` (since `pad >= 0`) — exactly the
`offset >= \old(arena->offset) + size` postcondition. arena_invariant
is preserved because the new offset is bounded by capacity (from the
guard) and capacity is unchanged. The pointer returned, `buffer +
\old(offset) + pad`, is valid for `size` bytes because the validity
range extends through `capacity - 1`. Each step is direct; the
obstacle is WP's inability to prove the C pad equals the ACSL pad
under Typed+Cast.

#### Category 2c: zero / try wrappers (10)

| #  | Goal                                                            |
|----|------------------------------------------------------------------|
| 1  | `typed_cast_arena_alloc_zero_ensures_3_part1`                   |
| 2  | `typed_cast_arena_alloc_zero_assigns_normal_part3`              |
| 3  | `typed_cast_arena_alloc_aligned_zero_ensures_3_part1`           |
| 4  | `typed_cast_arena_alloc_aligned_zero_assigns_normal_part3`      |
| 5  | `typed_cast_arena_try_alloc_assigns_normal_part03`              |
| 6  | `typed_cast_arena_try_alloc_non_null_out_ensures_part1`         |
| 7  | `typed_cast_arena_try_alloc_non_null_out_ensures_part2`         |
| 8  | `typed_cast_arena_try_alloc_aligned_assigns_normal_part03`      |
| 9  | `typed_cast_arena_try_alloc_aligned_non_null_out_ensures_part1` |
| 10 | `typed_cast_arena_try_alloc_aligned_non_null_out_ensures_part2` |

**Functions affected**: `arena_alloc_zero`, `arena_alloc_aligned_zero`,
`arena_try_alloc`, `arena_try_alloc_aligned`.

**Root cause**: These wrappers delegate to arena_alloc /
arena_alloc_aligned. The wrapper-specific obligations chain through
the parent allocator's contract:

- `arena_alloc_zero`'s `ensures_3_part1` is the
  `\result != \null ==> \forall i; ... ((u8*)\result)[i] == 0`
  postcondition. WP can discharge that mem_zero writes zeros, but
  it cannot establish that the pointer returned from arena_alloc
  satisfies `\valid((u8*)p + (0 .. size - 1))` because that fact is
  itself one of cat 2b's unproved ensures.
- `_assigns_normal_part3` residuals state the wrapper modifies the
  same regions as the parent allocator. WP cannot fully discharge
  the assigns clause because the parent's assigns clause is itself
  partially unproved (cat 2b through-effect).
- `arena_try_alloc`'s `non_null_out_ensures_part1` / `part2` cover
  the contract `*out == \null || \valid((u8*)*out + (0 .. size - 1))`
  and `\result <==> (*out != \null)`. Both chain through arena_alloc's
  result, hitting cat 2b's pad-equality residuals.

All 10 are inheritance from cat 2b through wrapper delegation. If
cat 2b closes (via a different `arena_can_fit` formulation or a
stronger ptr_align_up postcondition), cat 2c closes with it.

**Manual proof argument**: Each wrapper's body is a 2–3 line
delegation to the parent allocator. The manual arguments are
mechanical: assume the parent's postconditions hold (per cat 2b's
manual argument), apply mem_zero's verified postcondition or the
boolean compound return's verified shape, conclude.

#### Category 2d: arena_free_bytes ptr_offset / bytes_from call-site preconditions (2)

| # | Goal                                                       |
|---|-------------------------------------------------------------|
| 1 | `typed_cast_arena_free_bytes_call_bytes_from_requires`      |
| 2 | `typed_cast_arena_free_bytes_call_bytes_from_requires_2`    |

**Functions affected**: `arena_free_bytes`.

**Root cause**: arena_free_bytes returns either `bytes_empty()` (when
offset >= capacity) or
`bytes_from(ptr_offset(buffer, offset), capacity - offset)`. WP must
discharge bytes_from's two `requires` clauses (likely validity and
length-bound) at the call site. The pointer argument flows through
ptr_offset, which carries the same empty `nonnull` behavior as
ptr_align_up. Same root cause as cat 2a: ptr.h's deliberate empty
postcondition shape leaves WP without the substrate facts it would
need to reconstruct bytes_from's preconditions.

**Manual proof argument**: arena_invariant gives `\valid(buffer + (0
.. capacity - 1))`. The if-guard `offset >= capacity → return
bytes_empty()` means the bytes_from call is reached only when `offset
< capacity`, so `buffer + offset` is in the valid range and
`capacity - offset > 0`. The validity and length bounds bytes_from
requires are direct consequences; the obstacle is the uintptr_t
round-trip through ptr_offset, identical to cat 2a's situation.

### Summary of arena.h-own residuals

| Category | Goals | Functions affected                                            | WP feature gap                              |
|----------|-------|---------------------------------------------------------------|---------------------------------------------|
| 2a       | 8     | arena_alloc, arena_alloc_aligned                              | ptr.h empty nonnull behavior (VERIFY-006)   |
| 2b       | 26    | arena_alloc, arena_alloc_aligned                              | arithmetic chain through ptr_align_up       |
| 2c       | 10    | arena_alloc_zero, arena_alloc_aligned_zero, arena_try_alloc{,_aligned} | Wrapper delegation through cat 2b          |
| 2d       | 2     | arena_free_bytes                                              | ptr.h empty nonnull behavior (VERIFY-006)   |
| **Total**| **46**|                                                               |                                             |

Cats 2a, 2c, and 2d are downstream consequences of VERIFY-006's
forward-implication note: ptr.h's `nonnull` behaviors carry no
`ensures` clause because strengthening them would itself require
WP to discharge the uintptr_t round-trip. arena.h is the first
header to inherit the cascade through three call layers (arena.h →
ptr.h → uintptr_t). Cat 2b is arena.h's own arithmetic-chain
residual at the bump-pointer update; it could be reduced with
spec-strengthening (alternative `arena_can_fit` formulations using
function-call form, intermediate assertions in the function bodies)
but the spec-complexity cost was deliberately not taken — the
predicate's readable form was preserved.

### Mitigation

CI enforces exactly 103 unproved goals with the named goal patterns
covering all 57 inherited and all 46 arena.h-own residuals. Any
additional unproved goal or missing expected goal is a regression
and fails the build. The exact-count enforcement with named patterns
catches both count regressions (new residual class introduced) and
rename regressions (a contract weakened in a way that produces a new
residual under a different name).

arena.h achieves 100% line coverage (97/97) and 90.6% MC/DC coverage
(58/64) — the latter is the achievable ceiling under MCDC-003's
structural unreachability and the gcov-14 release-build macro artifact
(see MCDC-003). 22 of 22 user-facing functions are annotated and
verified; 10 of them are 100% proved (no residuals at all):
arena_init, arena_reset, arena_reset_secure, arena_reset_to, arena_mark,
arena_capacity, arena_remaining, arena_used, arena_is_empty,
arena_is_full. The remaining 12 functions carry the 46 own residuals
analyzed above.

Allocation behavior is tested by 46 unit tests in
`test/core/arena_test.c` covering init/reset, alloc/alloc_aligned,
zero variants, try variants (including the test_try_alloc_aligned_failure
that closed MCDC-003's line 510 gap at CI #962), mark/reset_to,
nested marks, byte views (including exhausted-arena cases), typed
macros, debug stats under CANON_ARENA_DEBUG, and lifetime tracking
under CANON_LIFETIME_DEBUG. Fuzzing exercises arena_alloc /
arena_alloc_aligned / arena_alloc_zero / mark/reset_to / reset_secure
through the CANON_FUZZING build in the same file. ASan + UBSan
across all 16 CI configs verify absence of out-of-bounds writes,
uninitialized reads, and lifetime violations. The substrate runtime
substrate (OWN-001) tracks lifetime token validity for borrows
captured from the arena.

The composable-verification claim is confirmed at three composition
layers: ptr.h → slice.h (slice.h inherited 2 of ptr.h's residuals
unchanged); ptr.h/checked.h/slice.h → memory.h (memory.h inherited
31 substrate residuals); memory.h+substrate → arena.h (arena.h
inherited memory.h's full 57-goal residual list byte-identically).
Each downstream header's inherited count equals the upstream total,
not greater. arena.h is the first header to demonstrate
propagation-through-two-composition-layers — memory.h's own 26
residuals re-emerge as part of arena.h's inherited 57, along with
memory.h's own 31 inherited (= memory.h's full residual surface).
The thesis is empirically supported at the layer-count that v1.3.0's
core/ stack reaches.

### Cross-references

- Inherited residuals: VERIFY-002 (checked.h), VERIFY-006 (ptr.h),
  VERIFY-007 (slice.h), VERIFY-008 (memory.h).
- ptr.h forward-implication note: VERIFY-006 (the deliberate empty
  `nonnull` behaviors that produce arena.h's cats 2a and 2d).
- MC/DC coverage closure: MCDC-003 (arena.h MC/DC at 90.6% with 6
  structurally unreachable / macro-artifact outcomes).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag, applied
  consistently to arena.h's coverage build).
- Substrate runtime tracking: OWN-001 (lifetime substrate covering
  arena), OWN-002 (Arena/Pool per-TU counter migration).
- Composable verification thesis: see README, "Composable
  verification" section.
- Per-goal CI artifact: `wp-proof-arena` (full WP output, including
  goal-by-goal classification and Qed-and-prover timing).
- Wrapper enforcement: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: core/arena.h".

---

## VERIFY-010: WP Limitations Inherited from Substrate Plus pool_invariant Arithmetic and ptr_elem Cascade Residuals (pool.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-010 |
| **Date**       | 2026-05-29 |
| **Baseline commit** | b2644ba (Canon-C CI #972) |
| **Scope**      | pool.h — 127 goals across 6 categories (4 own + 2 inherited groups) |
| **Category**   | Formal verification completeness |

**Description**: 127 of 3902 proof obligations (3.25%) are not discharged by
any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3 4.15.2 +
CVC5 1.2.1) with a 120-second timeout and `-wp-model Typed+Cast`. All 127 are
triple-prover-resistant. The goals split cleanly into two top-level groups:
**103 inherited from already-verified substrate headers** (re-emerging because
pool.h includes arena.h, which transitively pulls in memory.h, ptr.h, slice.h,
checked.h, and contract.h) and **24 pool.h-own** residuals in four categories.

pool.h extends the composable-verification thesis to a fourth data point and
the first observation across a **two-hop transitive include**. memory.h
established the thesis (31 inherited + 26 own); arena.h re-confirmed it (57
inherited + 46 own); pool.h re-confirms it again (103 inherited + 24 own).
arena.h's entire VERIFY-009 residual surface (57 substrate + 46 arena.h-own =
103) re-emerges byte-identically in the pool.h proof run — zero new substrate
residuals are introduced at the pool.h boundary. pool.h is a **sibling** of
arena.h in the include graph sense at the memory.h layer, but a **descendant**
of arena.h itself (`pool.h` includes `arena.h`), so its inherited surface is
arena.h's *total*, not memory.h's. Every inherited residual matches an
already-documented goal by name; the CI wrapper enforces this with per-goal
pattern checks.

### Inherited residuals (103)

These goals are not pool.h defects. They re-emerge in the pool.h proof run
because pool.h includes arena.h, which transitively includes memory.h, ptr.h,
slice.h, checked.h, and contract.h. WP re-emits the relevant obligations at
the new call sites. The full arena.h residual set (VERIFY-009's 57 inherited +
46 own = 103 total) is re-emitted as pool.h's inherited surface.

| # | Source            | Goals | Notes                                                              |
|---|-------------------|-------|--------------------------------------------------------------------|
| 1 | VERIFY-002        | 2     | checked.h u64 add overflow — via mem_alloc_array_checked / checked_mul |
| 2 | VERIFY-006 cat 2  | 3     | ptr.h align_up/down/padding integer theory                         |
| 3 | VERIFY-006 cat 3  | 3     | ptr.h ptr_align_* call-chain                                       |
| 4 | VERIFY-006 cat 4  | 2     | contract.h handler non-termination                                 |
| 5 | VERIFY-007 cat 1  | 20    | slice.h memcmp call-site preconditions (bytes/str equality, etc.)  |
| 6 | VERIFY-007 cat 2  | 1     | slice.h str_from_cstr strlen valid_string                          |
| 7 | VERIFY-008 cat 1  | 5     | memory.h \fresh / \freeable (mem_alloc, mem_free, array_checked)   |
| 8 | VERIFY-008 cat 2  | 9     | memory.h integer theory (mem_align, mem_is_aligned, etc.)          |
| 9 | VERIFY-008 cat 3  | 12    | memory.h memcmp call-site (mem_compare, mem_equal, equal_bytes)    |
| 10| VERIFY-009 cat 2a | 8     | arena.h ptr_span call-site (arena_alloc, arena_alloc_aligned)      |
| 11| VERIFY-009 cat 2b | 26    | arena.h fits/does_not_fit arithmetic chain                         |
| 12| VERIFY-009 cat 2c | 10    | arena.h zero/try wrapper delegation                                |
| 13| VERIFY-009 cat 2d | 2     | arena.h arena_free_bytes helper call-site                          |

**Inherited subtotal: 103 goals.** Byte-identical to arena.h's full residual
list (VERIFY-009's 57 inherited + 46 own). The composable-verification claim,
now empirically supported at four composition layers (ptr.h → slice.h with 2
inherited; ptr.h/checked.h/slice.h → memory.h with 31 inherited;
memory.h+substrate → arena.h with 57 inherited; arena.h+substrate → pool.h
with 103 inherited), is that substrate residuals propagate without
amplification: a downstream header's inherited count equals the upstream's
total, not greater. pool.h is the first header to observe propagation through
two transitive include hops (pool → arena → memory → ...), and the 103 count
confirms the property holds across the deeper graph.

### pool.h-own residuals (24)

Four categories. Cats 2a, 2c, and 2d are call-chain residuals at ptr.h /
slice.h / arena.h boundary functions whose `nonnull` behavior carries no
`ensures` clause (see VERIFY-006 forward-implication note). Cat 2b is the
`pool_invariant` postcondition arithmetic at pool_init.

As predicted in pool.h's header comment, pool.h-own residuals cluster at the
ptr_offset / ptr_elem and bytes_from call sites (the VERIFY-006 empty `nonnull`
cascade) plus the checked_mul overflow goal (VERIFY-002 class, counted in the
inherited table above via mem_alloc_array_checked). pool.h has **no
per-allocation alignment-pad arithmetic** — the region is reserved once at
pool_init and slots are computed by fixed-stride `ptr_elem`, not re-aligned per
allocation — so arena.h's cat 2b arithmetic-chain residual class does not
recur in pool.h's own surface. pool.h's own cat 2b is a narrower
`pool_invariant`-establishment arithmetic at pool_init, not a per-allocation
chain.

#### Category 2a: pool_invariant postcondition arithmetic at pool_init (5)

| # | Goal                                                |
|---|------------------------------------------------------|
| 1 | `typed_cast_pool_init_ensures_part4`                |
| 2 | `typed_cast_pool_init_ensures_2_part4`              |
| 3 | `typed_cast_pool_init_ensures_3_part4`              |
| 4 | `typed_cast_pool_init_ensures_4_part3`              |
| 5 | `typed_cast_pool_init_call_arena_alloc_requires`    |

**Functions affected**: `pool_init`.

**Root cause**: pool_init's success postcondition is `\result == \true ==>
pool_invariant(pool)`. The load-bearing conjunct of `pool_invariant` is
`end_mark - base_mark == capacity * object_size`. pool_init establishes this
by capturing `base_mark` from `arena_alloc`'s returned pointer and `end_mark`
from `arena_mark(arena)` after the reservation, with `needed = object_size *
capacity` (via `checked_mul`). To discharge the equality, WP must prove the
C arithmetic chain (post-pad data start, plus `needed` bytes, equals
`arena_mark`'s post-state) matches the predicate's `capacity * object_size`
product. This is nonlinear and crosses the `arena_alloc` boundary, where the
returned-pointer-to-offset computation flows through the empty `nonnull`
behavior cascade (VERIFY-006). Same class of limitation as ptr.h's
`align_up_ensures` (VERIFY-006 cat 2) and arena.h's `fits_ensures`
(VERIFY-009 cat 2b), applied to pool_init's invariant establishment.

`pool_init_call_arena_alloc_requires` is the call-site reconstruction of
`arena_alloc`'s `requires arena_invariant(arena)` — unprovable through the
same boundary, identical in shape to arena.h's own cat 2a.

**Note — LIMITATION-SUSPECTED on the three `ensures_part4` goals**: the
`pool_init_ensures{,_2,_3}_part4` goals are enforced as residuals here, but
their classification as pure WP-limitation (versus an over-strong
`pool_invariant` conjunct that could be weakened without losing the
load-bearing region check) is to be confirmed by a manual review, not
asserted. If the review finds the invariant can be restated to discharge them
without weakening `pool_get`'s region check or `pool_as_bytes`'s length proof,
they move from residual to proved in a follow-up. Until then they are enforced
as residuals so a regression cannot slip past the count check.

**Manual proof argument**: pool_init reserves exactly `needed = object_size *
capacity` bytes via `arena_alloc`, captures `base_mark` from the returned
(post-pad) pointer and `end_mark` from the post-reservation `arena_mark`.
Because `arena_alloc` advances the offset by exactly `needed` past the
post-pad start, `end_mark - base_mark == needed == object_size * capacity` by
construction. The C arithmetic is direct; the proof obstacle is WP's inability
to carry the offset arithmetic across `arena_alloc`'s empty `nonnull` boundary
and to discharge the nonlinear `capacity * object_size` product.

#### Category 2b: ptr_elem cascade in pool_alloc / pool_get / pool_get_const (6)

| # | Goal                                                       |
|---|-------------------------------------------------------------|
| 1 | `typed_cast_pool_alloc_assert_rte_mem_access`              |
| 2 | `typed_cast_pool_alloc_call_ptr_elem_requires`            |
| 3 | `typed_cast_pool_get_call_ptr_elem_requires`             |
| 4 | `typed_cast_pool_get_in_bounds_ensures_part4`            |
| 5 | `typed_cast_pool_get_const_call_ptr_elem_const_requires` |
| 6 | `typed_cast_pool_get_const_in_bounds_ensures_part4`      |

**Functions affected**: `pool_alloc`, `pool_get`, `pool_get_const`.

**Root cause**: each computes a slot address via `base = ptr_offset(buffer,
base_mark)` then `slot = ptr_elem(base, i, object_size)`. WP must discharge
`ptr_elem`'s `requires` at the call site and, for the `_get` variants, the
`in_bounds` ensures (`\result != \null`). Both flow through `ptr_offset`'s and
`ptr_elem`'s empty `nonnull` behaviors (VERIFY-006), which leave WP without
the substrate facts to reconstruct the preconditions or the result validity.
`pool_alloc_assert_rte_mem_access` is the RTE mem-access check on the same slot
computation. This is the VERIFY-006 empty-`nonnull` cascade re-emerging at
pool.h's call sites, exactly as pool.h's header comment predicted.

**Manual proof argument**: `pool_invariant` gives `end_mark - base_mark ==
capacity * object_size` and `end_mark <= arena->capacity` with the arena
buffer valid through `capacity - 1`. For `i < used <= capacity`, the slot
`base_mark + i * object_size` lies in `[base_mark, end_mark)`, hence within the
valid buffer range. The C computation matches `ptr_elem`'s contract; the
obstacle is the uintptr_t round-trip through `ptr_offset` / `ptr_elem` bodies
(VERIFY-006 cat 3), not a real bounds gap. `pool_get`'s runtime `require_msg`
region check (`(u8*)p - buffer < end_mark`) is the runtime backstop, exercised
at all build levels.

#### Category 2c: bytes_from / mem_zero / mem_secure_zero call-sites (5)

| # | Goal                                                          |
|---|----------------------------------------------------------------|
| 1 | `typed_cast_pool_alloc_zero_call_mem_zero_requires`           |
| 2 | `typed_cast_pool_as_bytes_call_bytes_from_requires`           |
| 3 | `typed_cast_pool_as_bytes_call_bytes_from_requires_2`         |
| 4 | `typed_cast_pool_reserved_bytes_call_bytes_from_requires`     |
| 5 | `typed_cast_pool_reserved_bytes_call_bytes_from_requires_2`   |

**Functions affected**: `pool_alloc_zero`, `pool_as_bytes`,
`pool_reserved_bytes` (and `pool_reset_secure`'s `mem_secure_zero`, counted in
cat 2d).

**Root cause**: `pool_as_bytes` and `pool_reserved_bytes` each call
`bytes_from(ptr_offset(buffer, base_mark), len)`; `pool_alloc_zero` calls
`mem_zero(p, object_size)`. WP cannot reconstruct `bytes_from`'s two `requires`
(validity, length-bound) or `mem_zero`'s validity precondition through the
`ptr_offset` empty `nonnull` boundary. Same root cause as VERIFY-009 cat 2d
(arena.h's `arena_free_bytes` → `bytes_from`) and VERIFY-008 (memory.h's
call-site preconditions).

**Manual proof argument**: `pool_invariant` establishes the reserved region
`[base_mark, end_mark)` is within the valid buffer, with
`end_mark - base_mark == capacity * object_size`. The `len` passed to
`bytes_from` is `object_size * used` (≤ region span, since `used <= capacity`)
or `object_size * capacity` (= region span); both are within bounds, and the
base pointer is valid. The C matches `bytes_from`'s contract; the obstacle is
the `ptr_offset` round-trip.

#### Category 2d: arena delegation + reset wrapper assigns/ensures (8)

| # | Goal                                                          |
|---|----------------------------------------------------------------|
| 1 | `typed_cast_pool_alloc_zero_assigns_normal_part3`             |
| 2 | `typed_cast_pool_reset_call_arena_reset_to_requires_2`       |
| 3 | `typed_cast_pool_reset_call_arena_alloc_requires`           |
| 4 | `typed_cast_pool_reset_reset_ensures_part3`                 |
| 5 | `typed_cast_pool_reset_reset_ensures_2_part3`               |
| 6 | `typed_cast_pool_reset_secure_assigns_exit_part6`           |
| 7 | `typed_cast_pool_reset_secure_assigns_normal_part6`         |
| 8 | `typed_cast_pool_reset_secure_call_mem_secure_zero_requires`|

**Functions affected**: `pool_alloc_zero`, `pool_reset`, `pool_reset_secure`.

**Root cause**: `pool_reset` delegates to `arena_reset_to` then re-reserves via
`arena_alloc`; `pool_reset_secure` adds a `mem_secure_zero` over the allocated
region before delegating to `pool_reset`. The wrapper-specific obligations
chain through the parent allocators' contracts:

- `pool_reset`'s `reset_ensures_part3` (re-establish `pool_invariant` after
  rollback-and-re-reserve) and the `arena_reset_to` / `arena_alloc` call-site
  preconditions reduce to the same arithmetic chain cat 2a leaves unproved,
  inherited through the delegation to `arena_reset_to` / `arena_alloc`.
- `pool_reset_secure`'s `assigns_{exit,normal}_part6` and the
  `mem_secure_zero` validity precondition chain through the region computation
  (cat 2c shape) plus the `pool_reset` delegation.
- `pool_alloc_zero`'s `assigns_normal_part3` chains through `pool_alloc`'s
  partially-unproved assigns (cat 2b through-effect).

All eight are inheritance from cats 2a/2b/2c through wrapper delegation,
structurally identical to arena.h's cat 2c. If cats 2a–2c close, cat 2d closes
with them.

**Manual proof argument**: each wrapper body is a short delegation. The manual
arguments are mechanical: assume the parent allocator's / region helper's
postconditions (per cats 2a–2c's manual arguments), apply `mem_secure_zero` /
`mem_zero`'s verified zeroing postcondition or `arena_reset_to`'s rollback
contract, conclude `pool_invariant` and the assigns shape.

### Summary of pool.h-own residuals

| Category | Goals | Functions affected                                   | WP feature gap                              |
|----------|-------|------------------------------------------------------|---------------------------------------------|
| 2a       | 5     | pool_init                                            | pool_invariant arithmetic across arena_alloc boundary (3 LIMITATION-SUSPECTED) |
| 2b       | 6     | pool_alloc, pool_get, pool_get_const                | ptr_elem cascade (VERIFY-006 empty nonnull) |
| 2c       | 5     | pool_alloc_zero, pool_as_bytes, pool_reserved_bytes | bytes_from / mem_zero call-site (VERIFY-006/007/008) |
| 2d       | 8     | pool_alloc_zero, pool_reset, pool_reset_secure      | arena delegation + wrapper assigns/ensures  |
| **Total**| **24**|                                                      |                                             |

Cats 2b, 2c, and 2d are downstream consequences of VERIFY-006's
forward-implication note: ptr.h's `nonnull` behaviors carry no `ensures`
clause because strengthening them would itself require WP to discharge the
uintptr_t round-trip. pool.h inherits the cascade through up to four call
layers (pool.h → arena.h → ptr.h → uintptr_t). Cat 2a is pool.h's own
invariant-establishment arithmetic at pool_init; three of its five goals are
LIMITATION-SUSPECTED pending the manual review noted above.

### Verification scope

- **All public `pool_*` functions: ANNOTATED + VERIFIED.** 19 user-facing
  functions; the query functions (`pool_used`, `pool_capacity`,
  `pool_remaining`, `pool_is_full`, `pool_is_empty`, `pool_object_size`,
  `pool_memory_used`, `pool_memory_reserved`) and `pool_try_alloc` /
  `pool_try_alloc_zero` carry no residuals.
- **Type-safe macros (`pool_alloc_type`, `pool_get_type`,
  `pool_alloc_type_zero`, `pool_get_type_const`, `pool_init_type`):
  DOCUMENTED, NOT WP-VERIFIED** — the C preprocessor strips ACSL inside
  `#define` before macro expansion. Same VERIFY-007/008/009
  macro-verification rationale.
- **`mem_alloc_array_checked` routing**: pool_init computes its reservation
  size through `checked_mul` (checked.h's verified primitive), inheriting the
  VERIFY-002 overflow residual rather than minting a pool-specific one (counted
  in the inherited table).

### MCDC note (no closure check in this wrapper)

Like arena.h's MCDC-003, pool.h's defensive `!pool->arena` subconditions are
WP-discharged unreachable under `pool_invariant` (see MCDC-004), but the
arithmetic-residual functions (`pool_init`, `pool_alloc`, `pool_get`,
`pool_get_const`) DO appear in the residual list under cats 2a–2c, so a
"function not in residuals" check (the slice.h MCDC-002 shape) is the wrong
shape for pool.h. The pool.h WP step therefore omits an MCDC closure
diagnostic. Cross-stream MCDC evidence is provided by the per-line gcov debug
step in the coverage job (see MCDC-004).

### Mitigation

CI enforces exactly 127 unproved goals with the named goal patterns covering
all 103 inherited and all 24 pool.h-own residuals. Any additional unproved
goal or missing expected goal is a regression and fails the build. The
exact-count enforcement with named patterns catches both count regressions
(new residual class introduced) and rename regressions (a contract weakened in
a way that produces a new residual under a different name).

pool.h achieves 100% line coverage (74/74) and 89.7% MC/DC coverage (61/68) at the 
baseline commit, rising to the documented ceiling of 91.2% (62/68) once the line-309 
closure (test_init_arena_alloc_fails_after_guard) lands; the residual 6 outcomes are 
type-invariant-unreachable (6 !pool->arena defensive subconditions, see MCDC-004). 
All 19 user-facing functions are annotated and verified; the query and try-variant functions are 100% proved
(no residuals). The remaining functions carry the 24 own residuals analyzed
above.

Allocation, access, and reset behavior is tested by the unit suite in
`test/core/pool_test.c` — init (including the unaligned-base regression and
both overflow guards), alloc / alloc_zero / try variants (including
null-out and full-pool paths), get / get_const (including null and OOB for
both variants), reset / reset_secure (including empty-pool and unaligned-base
stability), queries, byte views, multiple pools per arena, type-safe macros,
and lifetime tracking under `CANON_LIFETIME_DEBUG` (including the OWN-002
no-cycle regression). Fuzzing exercises pool_init / alloc / alloc_zero / get /
reset across random object sizes, capacities, and arena pre-pads through the
`CANON_FUZZING` build in the same file. ASan + UBSan across all 16 CI configs
verify absence of out-of-bounds access, uninitialized reads, and lifetime
violations. The runtime substrate (OWN-001 / OWN-002) tracks lifetime token
validity for borrows captured from the pool.

The composable-verification claim is confirmed at a fourth composition layer
and the first two-hop transitive include: pool.h inherits arena.h's full
103-goal residual surface byte-identically (zero new substrate residuals
introduced), and adds 24 own residuals concentrated at the ptr_elem /
bytes_from / pool_invariant call sites that pool.h's header comment predicted.
The 103:24 inherited:own ratio is the most lopsided yet — a direct consequence
of pool.h sitting atop the deepest substrate stack verified to date while
adding the thinnest own surface (no per-allocation alignment arithmetic).

### Cross-references

- Inherited residuals: VERIFY-002 (checked.h), VERIFY-006 (ptr.h),
  VERIFY-007 (slice.h), VERIFY-008 (memory.h), VERIFY-009 (arena.h).
- ptr.h forward-implication note: VERIFY-006 (the deliberate empty `nonnull`
  behaviors that produce pool.h's cats 2b/2c/2d).
- MC/DC coverage closure: MCDC-004 (pool.h MC/DC ceiling 91.2% (62/68) with 6 type-invariant-unreachable outcomes;
  line 309 closed as a reachable gap).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag, applied consistently
  to pool.h's coverage build).
- Substrate runtime tracking: OWN-001 (lifetime substrate covering pool),
  OWN-002 (Arena/Pool per-TU counter migration).
- Composable verification thesis: see README, "Composable verification"
  section.
- Per-goal CI artifact: `wp-proof-pool` (full WP output, including
  goal-by-goal classification and Qed-and-prover timing).
- Wrapper enforcement: `.github/workflows/cmake-multi-platform.yml`, step "WP: core/pool.h".

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
Canon-C headers in the 70-80% MC/DC range — `arena.h` (90.6%),
`pool.h` (73.5%), `stringbuf.h` (74.2%), and others. Each will need
its own per-line audit (per the procedure validated here) before
opening analogous deviations. Numbers will differ — slice.h's
4-of-58 ratio (6.9% unreachable defensive) is not directly
transferable. arena.h's analogous audit has shipped as MCDC-003,
covering 4 structurally-unreachable overflow-guard subconditions and
2 release-build macro artifacts; arena.h's 90.6% reflects a different
unreachability pattern than slice.h's `!ptr` defensive branches.

memory.h does NOT add to the MCDC-002 list. Its bytes_t/cbytes_t
variants (`mem_copy_bytes`, `mem_move_bytes`, `mem_zero_bytes`,
etc.) inherit slice.h's `bytes_t` invariant rather than introducing
new public {ptr, len} types, so the same `!ptr` checks at memory.h's
boundary are guarded by slice.h's already-discharged invariant. WP
discharges the analogous defensive branches in memory.h's bytes_t
variants under `bytes_invariant`, the same way slice.h's are
discharged. memory.h's MC/DC ceiling (88.3%) reflects the
`-DCANON_NO_REQUIRE` infrastructure missing — see VERIFY-008's
Mitigation section — not API-unreachable defensive code.

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

arena.h's annotation has shipped (VERIFY-009). Its MC/DC analysis
(MCDC-003) found a *different* unreachability pattern than slice.h's:
arena.h's structurally-unreachable outcomes are not `!ptr` defensive
branches but overflow-guard subconditions that arena_invariant
(combined with `CANON_ARENA_MAX_SIZE = CANON_GB`) renders provably
unreachable. The cross-stream evidence pattern is the same (gcov
measures source, WP discharges via invariant) but the source-level
shape differs. The forward implication for pool.h and stringbuf.h
holds: when their annotation lands, each will need its own per-header
MCDC-NNN entry documenting whatever unreachability shape their
invariants produce.

**Verification status (cross-reference)**:
- Public-API reachable branches: covered (54/54)
- API-unreachable defensive branches: documented (4/4)
- Total proved or documented: 58/58 (100%)
- WP-discharged unreachable: 4/4 (since 2026-05-02)

---

## MCDC-003: Structurally Unreachable Overflow Guards and Macro Artifacts (arena.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-003 |
| **Date**       | 2026-05-24 |
| **Baseline commit** | f53bddb (Canon-C CI #962) |
| **Scope**      | arena.h — 6 of 64 condition outcomes (4 unreachable + 2 macro artifact) |
| **Category**   | Coverage measurement methodology |

**Description**: 6 of 64 condition outcomes in `core/arena.h` are not
exercisable by tests. The gap splits cleanly into two categories with
distinct dispositions:

| # | Function            | Line | Subcondition not covered | Category |
|---|---------------------|------|--------------------------|----------|
| 1 | `arena_alloc`         | 346  | cond 0 true (`offset > CANON_USIZE_MAX - pad`)              | Unreachable under arena_invariant |
| 2 | `arena_alloc`         | 346  | cond 1 true (`offset + pad > CANON_USIZE_MAX - size`)       | Mathematically impossible in MC/DC isolation |
| 3 | `arena_alloc_aligned` | 401  | cond 0 true (`offset > CANON_USIZE_MAX - pad`)              | Unreachable under arena_invariant |
| 4 | `arena_alloc_aligned` | 401  | cond 1 true (`offset + pad > CANON_USIZE_MAX - size`)       | Mathematically impossible in MC/DC isolation |
| 5 | `arena_alloc`         | 356  | cond 0 false (`_arena_debug_update(arena)`)                 | Release-build macro no-op artifact |
| 6 | `arena_alloc_aligned` | 411  | cond 0 false (`_arena_debug_update(arena)`)                 | Release-build macro no-op artifact |

arena.h's gcov-measured MC/DC is 58/64 = 90.6%. The closure of the
previously-missing `arena_try_alloc_aligned` line 510 outcome
(`p != NULL` false branch) shipped at CI #962 via the new
`test_try_alloc_aligned_failure` test; without that closure the
baseline would have been 57/64 = 89.1%.

### Category 1: Overflow guard structural unreachability (4 outcomes)

`arena_alloc` and `arena_alloc_aligned` share the same compound-or
overflow guard:

```c
if (arena->offset > CANON_USIZE_MAX - pad ||           // cond 0
    arena->offset + pad > CANON_USIZE_MAX - size ||    // cond 1
    arena->offset + pad + size > arena->capacity) {    // cond 2
    return NULL;
}
```

gcov reports cond 0 and cond 1 as `not covered (true)` in both
functions — four outcomes total. Both subconditions are structurally
unreachable under the `arena_invariant` predicate combined with the
`CANON_ARENA_MAX_SIZE` constant from `core/primitives/limits.h`.

**Cond 0 unreachability**: `arena_invariant` requires `capacity <=
CANON_ARENA_MAX_SIZE`. `core/primitives/limits.h` defines
`CANON_ARENA_MAX_SIZE = CANON_GB = (usize)1024 * (usize)1024 *
(usize)1024 = 2^30`. The alignment pad is bounded by
`CANON_DEFAULT_ALIGN - 1`, which is at most 15 (since
`CANON_DEFAULT_ALIGN = 16` under the Frama-C C99 fallback, or
`_Alignof(max_align_t)` which is 16 on every supported platform).
Therefore `offset + pad <= 2^30 + 15`, which is far below
`CANON_USIZE_MAX` (`SIZE_MAX = 2^32 - 1` on 32-bit, `2^64 - 1` on
64-bit). Cond 0 (`offset > CANON_USIZE_MAX - pad`, equivalent to
`offset + pad > CANON_USIZE_MAX`) cannot fire.

**Cond 1 unreachability**: MC/DC requires each subcondition to flip
the compound outcome independently. For cond 1 to flip the outcome
to true, cond 0 must be false and cond 2 must be false. cond 2 false
means `offset + pad + size <= capacity`. Since `capacity` is a `usize`
(by C type), `capacity <= CANON_USIZE_MAX`. So `offset + pad + size <=
CANON_USIZE_MAX`. But cond 1 true requires `offset + pad + size >
CANON_USIZE_MAX`. Contradiction. Cond 1 cannot fire while cond 2 is
false, regardless of `CANON_ARENA_MAX_SIZE`. This is a purely
mathematical unreachability — independent of any project constant.

### Category 2: Release-build macro no-op artifacts (2 outcomes)

Lines 356 and 411 each show `condition 0 not covered (false)` on
calls to `_arena_debug_update(arena)`. The macro is defined on line
176:

```c
#define _arena_debug_update(a) ((void)0)
```

Under release builds (no `CANON_ARENA_DEBUG`, as in the coverage
build's compile flags), the macro expands to a no-op. gcov-14 still
registers a structural "condition" at the macro-expansion site and
counts it as uncovered. This is not a real condition — there is no
branch at runtime. The same pattern appears in every test row of the
project under `contract.h 0/2` (the disabled `require_msg` site under
`-DCANON_NO_REQUIRE`); it is the gcov-14 instrumentation behavior
documented in MCDC-001.

These 2 outcomes are not coverage gaps. No test could close them
without re-enabling `CANON_ARENA_DEBUG` in the coverage build, which
would change the code under measurement.

### Rationale

Category 1 is structural unreachability under the type invariant.
arena_invariant is preserved by every public function in arena.h (WP
verifies this in VERIFY-009; the cat 2b residuals are about the
arithmetic ensures clauses, not the invariant itself — arena_invariant
is in fact discharged for every alloc path). Combined with
`CANON_ARENA_MAX_SIZE = CANON_GB`, the predicate guarantees that
`offset + pad` cannot exceed `CANON_USIZE_MAX - size` for any callable
`size`, making the overflow guard's first two subconditions
defense-in-depth code that the type discipline already prevents.

The overflow guard is preserved deliberately. Removing it to satisfy
MC/DC would (a) eliminate documentation of the contract's safety
boundary at the source-code site where readers most need to see it,
and (b) couple the implementation to `CANON_ARENA_MAX_SIZE`'s current
value in a way that would silently break if the constant were ever
raised. The guard is correct under the current invariant, correct
under any reasonable extension of the invariant, and zero-cost on
release builds (the compiler optimizes the always-false subconditions
out). The 4 uncovered outcomes are the gcov-measurement cost of
keeping it.

Category 2 is gcov-14 instrumentation behavior on no-op macro
expansion, not a code property at all. It cannot be closed by tests;
it can only be closed by changing the build flags, which would change
the code being measured.

### Mitigation

1. **Cross-stream evidence via VERIFY-009**: arena_invariant is
   verified by WP. Cond 0's unreachability follows from
   arena_invariant + `CANON_ARENA_MAX_SIZE`'s value (a substrate
   constant, not a proof obligation). Cond 1's unreachability is
   mathematical and independent of any constant. The two streams
   align: gcov reports source-level uncoverage; WP proves the
   underlying invariant; together they establish that the 4 outcomes
   are provably unexecutable code paths under the documented
   preconditions, not coverage gaps. This is the same evidence
   pattern MCDC-002 established for slice.h's `!ptr` defensive
   branches.

2. **CI regression detector**: The coverage job's "Debug: per-line
   MC/DC detail for arena.h" step prints the gcov dump on every run.
   If a future change closes one of the 6 outcomes (e.g., by raising
   `CANON_ARENA_MAX_SIZE` close to `CANON_USIZE_MAX`, which would
   make cond 0 reachable), the per-line detail will show it.
   Conversely, if a future change opens a new uncovered outcome, the
   debug step surfaces it for human review.

3. **The achievable MC/DC ceiling under the current invariant +
   constants is 58/64 = 90.6%**. Reaching 90.6% represents 100% of
   API-reachable coverage. The 6 missing outcomes are documented
   here and explained; they are not counted as a coverage regression.

4. **Allocation behavior is otherwise exhaustively tested**.
   `test/core/arena_test.c` covers 46 unit tests across init/reset,
   alloc/alloc_aligned (including the exhaustion-returns-NULL path
   that exercises cond 2 of the overflow guard, the one subcondition
   that *is* reachable), zero variants, try variants (including the
   new `test_try_alloc_aligned_failure` that closed the line 510
   gap at CI #962), mark/reset_to, nested marks, byte views,
   typed macros, debug stats under `CANON_ARENA_DEBUG`, and lifetime
   tracking under `CANON_LIFETIME_DEBUG`. Fuzzing exercises the same
   functions through randomly constructed inputs.

### Line 510 closure (history)

Prior to CI #962, arena.h's MC/DC baseline was 57/64 = 89.1%, with a
7th missing outcome on `arena_try_alloc_aligned` line 510:
`condition 1 not covered (false)` on the compound return
`return out != NULL && p != NULL;`. The non-aligned variant
(`arena_try_alloc` line 485) reported 4/4 coverage because
`test_try_alloc_failure` exercises the case where `out != NULL`
but the underlying allocation returns NULL. The aligned variant had
no equivalent test.

CI #962 added `test_try_alloc_aligned_failure` to
`test/core/arena_test.c`, mirroring the non-aligned variant. Line 510
now reports 4/4 and arena.h's MC/DC baseline moved from 57/64 to
58/64. This was a real test gap, not an unreachability — closing it
is the substantive improvement; the 6 remaining outcomes are the
documented baseline ceiling.

The audit that uncovered the asymmetry between line 485 and line 510
also confirmed that no other reachable gaps exist on arena.h. The
6 outcomes recorded in this entry are the residual after every
reachable outcome has been closed.

### Cross-references

- VERIFY-009 — arena.h's full WP residual analysis, including the
  arena_invariant preservation that establishes cond 0's
  unreachability.
- MCDC-001 — `-DCANON_NO_REQUIRE` coverage methodology; the
  `contract.h 0/2` instrumentation artifact this entry's cat 2
  mirrors.
- MCDC-002 — slice.h's analogous `API-unreachable` → `WP-discharged
  unreachable` pattern. arena.h's structural unreachability is a
  different source-level shape (overflow guards under
  arena_invariant + capacity constant) but the same cross-stream
  evidence pattern.
- Per-line gcov dump: CI artifact via the "Debug: per-line MC/DC
  detail for arena.h" step in `.github/workflows/cmake-multi-platform.yml`.

---

## MCDC-004: Type-Invariant-Unreachable Defensive Arena Subconditions (pool.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-004 |
| **Date**       | 2026-05-29 |
| **Baseline commit** | b2644ba (Canon-C CI #972) |
| **Scope**      | pool.h — 6 of 68 condition outcomes (6 unreachable under pool_invariant) |
| **Category**   | Coverage measurement methodology |

**Description**: 6 of 68 condition outcomes in `core/pool.h` are not
exercisable by tests. All six are the same structural pattern — the
`!pool->arena` middle subcondition of a defensive OR-form early return that
every pool query and reset function shares:

```c
if (!pool || !pool->arena || <op-specific>) return <fail>;
```

The first subcondition (`!pool`) is reachable by passing a NULL pool; the
op-specific third subcondition (out-of-bounds index, empty pool, etc.) is
reachable by ordinary use. The middle subcondition (`!pool->arena`) is
**unreachable under `pool_invariant`**: a pool that satisfies the invariant
has a non-NULL, valid backing arena, and a pool that does not satisfy the
invariant cannot be constructed through `pool_init` (which requires
`arena_invariant(arena)` and stores the validated pointer). There is no public
path that produces a pool with `pool != NULL && pool->arena == NULL`, so the
`!pool->arena`-true outcome cannot fire.

**Affected outcomes** (line numbers from `core/pool.h`, baseline commit
b2644ba):

| # | Function              | Line | Subcondition not covered            |
|---|-----------------------|------|--------------------------------------|
| 1 | `pool_get`            | 435  | cond 1 true (`!pool->arena`)        |
| 2 | `pool_get_const`      | 464  | cond 1 true (`!pool->arena`)        |
| 3 | `pool_as_bytes`       | 541  | cond 1 true (`!pool->arena`)        |
| 4 | `pool_reserved_bytes` | 557  | cond 1 true (`!pool->arena`)        |
| 5 | `pool_reset`          | 597  | cond 1 true (`!pool->arena`)        |
| 6 | `pool_reset_secure`   | 644  | cond 1 true (`!pool->arena`)        |

pool.h's gcov-measured MC/DC at the baseline commit is 61/68 = 89.7%. The
line-309 reachable-gap closure (see "Reachable gap closure" below) lifts the
measured value to the documented ceiling of 62/68 = 91.2% — the achievable
maximum, since the 6 outcomes above are provably unexecutable.

Note that three of the six lines carry *other* outcomes on the same line that
ARE reachable and ARE covered — only the `!pool->arena` (cond 1 true) outcome
is unreachable:

- `pool_get_const` line 464: cond 0 true (null pool) and cond 2 true (OOB
  index) are covered by `test_get_const_null_and_oob_return_null`; only cond 1
  true is unreachable.
- `pool_reserved_bytes` line 557: cond 0 true (null pool) is covered by
  `test_reserved_bytes_null_safe`; only cond 1 true is unreachable.
- `pool_reset_secure` line 644: cond 2 true (empty pool) is covered by
  `test_reset_secure_empty_pool`; only cond 1 true is unreachable.

This per-outcome granularity is why the entry counts 6 outcomes rather than 6
whole lines — gcov-14's `-fcondition-coverage` measures each subcondition's
true/false outcome independently, and the disposition differs within a single
compound guard.

### Rationale

The `!pool->arena` checks are defensive code against a malformed pool — a
`Pool` struct whose `arena` field was zeroed or never initialized. They are
correct, cheap, and intentionally not exercisable from the public API in MC/DC
isolation: exercising the `!pool->arena`-true outcome in isolation would
require constructing the very malformed pool that `pool_init`'s contract is
designed to prevent (a non-NULL pool with a NULL arena). Removing the checks to
satisfy MC/DC would weaken the library's robustness against caller error and
uninitialized-struct bugs, which is exactly the failure mode the borrow/
lifetime substrate (OWN-001) exists to catch at the next layer up.

This is the same disposition established by MCDC-002 for slice.h's `!ptr`
defensive branches and by MCDC-003 for arena.h's overflow-guard subconditions:
the source-level shape differs (pool.h's is a `!pool->arena` arena-validity
disjunct, not slice.h's `!ptr` borrow-validity disjunct or arena.h's
`offset + pad` overflow arithmetic), but the cross-stream evidence pattern is
identical — gcov measures source-level uncoverage; WP discharges the underlying
unreachability via the type invariant.

### Cross-stream evidence via VERIFY-010

`pool_invariant` is verified by WP (see VERIFY-010). The six `!pool->arena`
subconditions are discharged by WP as unreachable: none of the six functions
appears in pool.h's unproved-goal list *for the `!pool->arena` branch* — the
branch is proved dead under `pool_invariant`, which entails
`arena_invariant(pool->arena)` and therefore `\valid(pool->arena)` and
`pool->arena != \null`. WP places these outcomes in its `Unreachable` count
rather than its residual list; they never appear among VERIFY-010's 127
residuals.

Note that `pool_get` and `pool_get_const` *do* appear in VERIFY-010's residual
list under category 2b — but for a different obligation (the `ptr_elem`
cascade on the slot computation), not for the `!pool->arena` branch. The two
are distinct: the arithmetic residual is a WP limitation on the reachable
success path; the `!pool->arena` unreachability is a discharged property of the
defensive path. This is why pool.h's WP wrapper does not carry an
MCDC-closure diagnostic of the MCDC-002 "0/N functions in residuals" shape —
the residual-bearing functions and the unreachable-branch functions overlap,
so "function absent from residuals" is the wrong predicate for pool.h (see
VERIFY-010, "MCDC note"). The unreachability evidence is instead the per-line
gcov dump cross-referenced against WP's `Unreachable` count.

The two streams align: gcov reports the 6 outcomes as source-level uncovered;
WP proves the underlying `!pool->arena` branch unexecutable under
`pool_invariant`. Together they establish that the 6 outcomes are provably
unexecutable code paths under the documented preconditions, not coverage gaps.
This satisfies DO-178C's intent for defensive code unreachable in normal
operation without requiring the code to be removed.

### Reachable gap closure (line 309)

pool.h's `pool_init` carries a post-`arena_alloc` NULL guard at line 309:

```c
needed = aligned_size * max_objects;      /* via checked_mul */
if (needed > arena_remaining(arena)) return false;   /* coarse guard */
region = arena_alloc(arena, needed);
if (!region) return false;                /* line 309 */
```

This guard's true outcome (`!region`) was uncovered in the baseline because
the preceding coarse guard (`needed > arena_remaining`) appears to subsume it.
It does not. `arena_remaining` returns the **raw** `capacity - offset`, which
does not account for the alignment pad `arena_alloc` inserts before the
returned region. From an unaligned arena offset there is a window where
`needed <= arena_remaining(arena)` (coarse guard passes) yet
`offset + pad + needed > capacity` (arena_alloc fails its own check and returns
NULL), so line 309 fires.

This is a **reachable** outcome — a real defensive path, not an
unreachability. It was closed at CI #974 / 98de378, which added 
test_init_arena_alloc_fails_after_guard to test/core/pool_test.c 
and lands the window exactly: a 65-byte arena, a 1-byte throwaway allocation
(offset = 1, so `arena_alloc` needs pad = 15), and a 16x4 = 64-byte
reservation. Raw remaining = 65 - 1 = 64, so the coarse guard passes
(`64 > 64` is false); `arena_alloc` then computes `1 + 15 + 64 = 80 > 65` and
returns NULL, so `pool_init` returns false via line 309. The test also asserts
the arena offset is left at the 1-byte throwaway, confirming the failed
reservation is side-effect-free.

Closing line 309 moved pool.h's measured MC/DC from 61/68 to 62/68 — the
documented ceiling. The 6 outcomes in the table above are the residual after
every reachable outcome has been closed.

### Mitigation

1. **Cross-stream evidence via VERIFY-010**: `pool_invariant` is verified by
   WP. The 6 `!pool->arena` outcomes are discharged as unreachable under the
   invariant (WP `Unreachable` count, not the residual list). gcov reports
   source-level uncoverage; WP proves the underlying branch dead; together
   they establish the 6 outcomes as provably unexecutable, not coverage gaps.
   Same evidence pattern as MCDC-002 (slice.h) and MCDC-003 (arena.h).

2. **CI regression detector**: the coverage job's "Debug: per-line MC/DC
   detail for pool.h" step prints the gcov dump on every run. If a future
   change makes one of the 6 outcomes reachable (e.g. by adding a public
   constructor that can leave `arena` NULL), the per-line detail will show it.
   Conversely, if a future change opens a new uncovered outcome, the debug
   step surfaces it for human review.

3. **The achievable MC/DC ceiling under `pool_invariant` is 62/68 = 91.2%**.
   Reaching 62/68 represents 100% of API-reachable coverage. The 6 missing
   outcomes are documented here and explained; they are not counted as a
   coverage regression.

4. **Pool behavior is otherwise exhaustively tested**. `test/core/pool_test.c`
   covers init (including the unaligned-base regression and all three
   overflow/failure guards: alignment-overflow, checked_mul-overflow, and the
   line-309 arena_alloc-fails-after-guard window), alloc / alloc_zero / try
   variants (including null-out and full-pool paths), get / get_const
   (including null-pool and OOB for both variants), reset / reset_secure
   (including the empty-pool early path and unaligned-base stability), queries
   (including the is_empty-false vector), byte views (including null-safe for
   both variants), multiple pools per arena, type-safe macros, and lifetime
   tracking under `CANON_LIFETIME_DEBUG` (including the OWN-002 no-cycle
   regression). Fuzzing exercises pool_init / alloc / alloc_zero / get / reset
   across random object sizes, capacities, and arena pre-pads through the
   `CANON_FUZZING` build, so every API-reachable path — including the
   defensive guards' reachable outcomes — is exercised by random inputs.

### Reachable-gap closure history

pool.h's MC/DC baseline was 55/68 = 80.9% before the gap-closure audit. The
audit closed every reachable outcome, in two waves:

| Wave | Outcome(s) closed                          | Function(s)            | Line | Closed by                                    | Running total |
|------|---------------------------------------------|------------------------|------|-----------------------------------------------|---------------|
| 1    | cond 0 true (null) + cond 2 true (OOB)     | `pool_get_const`       | 464  | `test_get_const_null_and_oob_return_null`     | 57/68         |
| 1    | cond 2 true (empty pool)                    | `pool_reset_secure`    | 644  | `test_reset_secure_empty_pool`                | 58/68         |
| 1    | both false outcomes (masking-MC/DC vector)  | `pool_is_empty`        | 510  | `test_is_empty_false_when_used`               | 60/68         |
| 1    | cond 0 true (null)                          | `pool_reserved_bytes`  | 557  | `test_reserved_bytes_null_safe`               | 61/68         |
| 2    | cond true (`!region` after arena_alloc)     | `pool_init`            | 309  | `test_init_arena_alloc_fails_after_guard`     | 62/68         |

Wave 1 (four tests, six outcomes) shipped at CI #972 / b2644ba and is the source of the 61/68 measured baseline. 
Wave 2 (the line-309 closure, test_init_arena_alloc_fails_after_guard) shipped at CI #974 / 98de378, reaching the 
62/68 ceiling; this MCDC-004 entry and the accompanying coverage revisions were recorded at CI #975 / 630f68f. The
audit confirmed that after wave 2 no other reachable gaps remain: the 6 outcomes recorded in this entry are the 
residual once every reachable outcome has been closed.

### Forward note (stringbuf.h and other `{ptr, len}` / handle types)

MCDC-002's forward note anticipated that each downstream header would need its
own per-header MCDC entry documenting whatever unreachability shape its
invariant produces. pool.h confirms this: its shape is neither slice.h's `!ptr`
borrow-validity disjunct nor arena.h's overflow-guard arithmetic, but a
`!pool->arena` arena-validity disjunct discharged under `pool_invariant`. The
next header to be annotated (stringbuf.h, MCDC-002 lists it provisionally at
74.2%) will need its own audit; its number and unreachability shape are not
transferable from pool.h's 6-of-68.

### Cross-references

- VERIFY-010 — pool.h's full WP residual analysis, including the
  `pool_invariant` verification that establishes the `!pool->arena`
  unreachability, and the "MCDC note" explaining why pool.h's WP wrapper omits
  an MCDC-closure diagnostic.
- MCDC-001 — `-DCANON_NO_REQUIRE` coverage methodology, applied to pool.h's
  coverage build.
- MCDC-002 — slice.h's `API-unreachable` -> `WP-discharged unreachable`
  pattern; the per-header forward note this entry fulfills. (Note: MCDC-002's
  Pattern note lists pool.h's pre-audit figure of 73.5%; the post-audit ceiling
  is 62/68 = 91.2%.)
- MCDC-003 — arena.h's structurally-unreachable overflow guards; same
  cross-stream evidence pattern, different source-level shape.
- Per-line gcov dump: CI artifact via the "Debug: per-line MC/DC detail for
  pool.h" step in `.github/workflows/cmake-multi-platform.yml`.

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
