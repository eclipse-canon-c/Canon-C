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
| **Scope**      | slice.h — 15 goals across 3 categories |
| **Category**   | Formal verification completeness |

**Description**: 15 of 390 proof obligations (3.8%) are not discharged
by any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3
4.15.2 + CVC5 1.2.1) with a 120-second timeout and `-wp-model
Typed+Cast`. All 15 are triple-prover-resistant. The goals fall into
three categories:

1. **memcmp call-site preconditions** (12): Four per `bytes_equal`
   and `str_equal`, two per `str_starts_with` and `str_ends_with`.
   Goal-name pattern: `typed_cast_<func>_call_memcmp_requires_<X>`
   where `<X>` ranges over `valid_s1`, `valid_s2`, `danglingness_s1`,
   `danglingness_s2`. The `valid_*` obligations appear only on the
   bytes_t variants because the str_t variants close them through
   `str_valid` predicate reasoning; the `danglingness_*` obligations
   appear on all four equality functions. The eight `initialization_*`
   obligations formerly in this category were closed in VERIFY-012 by
   stating `\initialized` as an explicit precondition on each equality
   function — WP discharges initialization once the contract asserts
   it, even though `\dangling` remains unimplemented in Frama-C 29.

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
`danglingness` obligation because the underlying `\dangling` logic is,
per the WP warning, not yet implemented in Frama-C 29. Strengthening
the slice.h predicates does not close the danglingness goals; the
verifier itself cannot process them. (The `initialization` obligation,
by contrast, WP *can* discharge once the contract states `\initialized`
as a precondition — that is precisely how VERIFY-012 closed the eight
former `initialization_*` goals.)

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

**Mitigation**: CI enforces exactly 15 unproved goals with the named
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
| **Scope**      | memory.h — 43 goals across 7 categories (3 own + 4 inherited) |
| **Category**   | Formal verification completeness |

**Description**: 43 of 2862 proof obligations (1.5%) are not discharged
by any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3
4.15.2 + CVC5 1.2.1) with a 120-second timeout and `-wp-model
Typed+Cast`. All 43 are triple-prover-resistant. The goals split
cleanly into two top-level groups: **23 inherited from already-verified
substrate headers** (re-emerging because memory.h includes those
headers transitively) and **20 memory.h-own** residuals in three
categories.

This is the first Canon-C verification round where inherited
residuals exceed own residuals. The 23:20 ratio is a quantitative
expression of the composable-verification thesis: substrate residuals
propagate without amplification across composition layers, so a layer
that builds on five already-verified headers inherits their residual
fingerprints. memory.h does not introduce 31 new defects — it is the
first place where every previously-documented residual category
becomes simultaneously visible.

### Inherited residuals (23)

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
| 5 | VERIFY-007 cat 1 | 12    | `typed_cast_bytes_equal_call_memcmp_requires_<aspect>` (4 goals),  |
|   |                  |       | `str_equal_call_memcmp_requires_<aspect>` (4),                     |
|   |                  |       | `str_starts_with_call_memcmp_requires_<aspect>` (2),               |
|   |                  |       | `str_ends_with_call_memcmp_requires_<aspect>` (2); the 8           |
|   |                  |       | `initialization_*` goals were closed in VERIFY-012                 |
| 6 | VERIFY-007 cat 2 | 1     | `typed_cast_str_from_cstr_call_strlen_requires_valid_string_s`     |

**Inherited subtotal: 23 goals.** These are the same goals counted in
the originating deviations; they are not new memory.h residuals. The
composable-verification claim is the empirical observation that this
count is byte-identical between memory.h round 2 (before
contract-shape fixes) and memory.h round 3 (after fixes) — the round
3 fix removed 10 contract-shape residuals and zero inherited ones,
confirming that hoisting non-overlap preconditions does not perturb
the substrate's residual surface.

### memory.h-own residuals (20)

Three categories, each rooted in a documented WP feature gap. The
initialization sub-class of category 3 was closed in VERIFY-012 by
stating `\initialized` as an explicit precondition (WP discharges
initialization once the contract asserts it); the remaining 20 are
limited by the `\dangling`/`\fresh`/`\freeable` feature gaps the
verifier itself cannot process.

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

#### Category 3: memcmp call-site danglingness (6)

| #  | Goal                                                                       |
|----|-----------------------------------------------------------------------------|
| 1  | `typed_cast_mem_compare_call_memcmp_requires_danglingness_s1`               |
| 2  | `typed_cast_mem_compare_call_memcmp_requires_danglingness_s2`               |
| 3  | `typed_cast_mem_equal_call_memcmp_requires_danglingness_s1`                 |
| 4  | `typed_cast_mem_equal_call_memcmp_requires_danglingness_s2`                 |
| 5  | `typed_cast_mem_equal_bytes_call_memcmp_requires_danglingness_s1`           |
| 6  | `typed_cast_mem_equal_bytes_call_memcmp_requires_danglingness_s2`           |

The six `initialization_*` goals formerly in this category (one
`_s1`/`_s2` pair per function) were closed in VERIFY-012 by stating
`\initialized` as an explicit precondition on `mem_compare`,
`mem_equal`, and `mem_equal_bytes`.

**Functions affected**: `mem_compare`, `mem_equal`, `mem_equal_bytes`.

**Root cause**: Identical to VERIFY-007 category 1. ACSL's `memcmp`
contract requires the caller to establish that both buffer ranges
are fully valid, fully initialized, and non-dangling. memory.h's
`mem_valid_read` predicate establishes validity, but the
`initialization` and `danglingness` obligations cannot be discharged
because the underlying `\dangling` logic — quoted from WP's own
warning during the slice.h proof run — is "not yet implemented" in
Frama-C 29:

```
[wp] FRAMAC_SHARE/libc/string.h:38: Warning:
  Allocation, initialization and danglingness not yet implemented
  (\dangling{L}((char *)s + i))
```

memory.h directly calls `memcmp` from `mem_compare`, `mem_equal`,
and `mem_equal_bytes`, so the same residual class re-emerges at
memory.h's call sites. Note the count: 2 residuals per function ×
3 functions = 6, danglingness only. The `initialization_*` pair on
each function was closed in VERIFY-012 (WP discharges initialization
once `\initialized` is stated as a precondition); the `valid_*` aspect
discharges through `mem_valid_read`, leaving only the `\dangling`
aspect, which Frama-C 29 cannot process.

**Manual proof argument**: For `mem_compare_call_memcmp_requires_danglingness_s1`,
the obligation is "before calling `memcmp(a, b, size)`, the pointer
`a` is non-dangling over `[a, a + size)`". memory.h's contract
requires `mem_valid_read((void *)a, (integer)size)`, which establishes
the byte range is readable in the program memory state — a property
that entails non-danglingness for any caller that can legally form the
range. WP cannot discharge the `\dangling` obligation because the
`\dangling` logic is, per the WP warning, not yet implemented in
Frama-C 29; the unproved obligation reflects that verifier gap, not a
real soundness gap. (The companion `initialization_s1/s2` obligations
were closed in VERIFY-012 by stating `\initialized` explicitly — WP
*can* discharge initialization once the contract asserts it.)

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
| 2c       | 6     | mem_compare/equal/equal_bytes | `\dangling` (initialization closed, VERIFY-012) |
| **Total**| **20**|                              |                                |

All 20 residuals would be discharged by improvements to Frama-C's
`\dangling`/`\fresh`/`\freeable` theory or its integer theory.
**Correction (VERIFY-012, supersedes this entry's original claim):** an
earlier version of this section stated that "strengthening memory.h's
predicates would produce no improvement." That is now falsified.
Stating `\initialized` as an explicit precondition closed 6 memory.h
goals here (and 8 in slice.h) with no executable change — WP discharges
initialization once the contract asserts it. What remains genuinely
WP-blocked is `\dangling`/`\fresh`/`\freeable`, which Frama-C 29 does
not implement (confirmed by A. Blanchard, CEA). Those are not closable
by contract strengthening; initialization was.

### Mitigation

CI enforces exactly 43 unproved goals with the named goal patterns
covering all 23 inherited and all 20 memory.h-own residuals. Any
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
verification thesis quantitatively. The 23 inherited residuals
propagated unchanged from the substrate (ptr.h's 8, checked.h's 2,
slice.h's 13 — all visible in memory.h's 2862-goal run). The 20
memory.h-own residuals fall into three categories that are each
rooted in a Frama-C feature gap, not in memory.h's design, and
each category has a known-good mitigation strategy (allocation
testing, alignment vectors, libc compatibility under valgrind).

The composable-verification claim — that substrate residuals
propagate without amplification — is now empirically supported
by two data points: ptr.h → slice.h (where slice.h inherited
ptr.h's 2 contract-handler residuals unchanged) and ptr.h/checked.h/
slice.h → memory.h (where memory.h inherited 23 substrate residuals
unchanged across two proof rounds; 31 before VERIFY-012's upstream
closures). The arena.h verification (see
VERIFY-009, shipped at CI #962) provides the third data point:
arena.h inherits memory.h's full 43-goal residual surface
byte-identically (zero new substrate residuals introduced), confirming
that the propagation-without-amplification property extends through
one more composition layer.

### Cross-references

- Inherited residuals: VERIFY-002 (checked.h), VERIFY-006 (ptr.h),
  VERIFY-007 (slice.h).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag).
- Downstream confirmation: VERIFY-009 (arena.h inherits all 43
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
| **Scope**      | arena.h — 89 goals across 8 categories (4 own + 4 inherited groups) |
| **Category**   | Formal verification completeness |

**Description**: 89 of 3472 proof obligations (2.56%) are not
discharged by any prover in the triple-prover configuration (Alt-Ergo
2.6.3 + Z3 4.15.2 + CVC5 1.2.1) with a 120-second timeout and
`-wp-model Typed+Cast`. All 89 are triple-prover-resistant. The goals
split cleanly into two top-level groups: **43 inherited from
already-verified substrate headers** (re-emerging because arena.h
includes memory.h, which transitively pulls in ptr.h, slice.h,
checked.h, and contract.h) and **46 arena.h-own** residuals in four
categories.

arena.h extends the composable-verification thesis to a third data
point. memory.h established it (23 inherited + 20 own); arena.h
re-confirms it (43 inherited + 46 own). The full 43-goal residual
surface from VERIFY-008 re-emerges byte-identically in the arena.h
proof run — zero new substrate residuals are introduced at the arena.h
boundary. Every inherited residual matches an already-documented goal
by name; the wrapper enforces this with per-goal pattern checks.

### Inherited residuals (43)

These goals are not arena.h defects. They re-emerge in the arena.h
proof run because arena.h includes memory.h, which transitively
includes ptr.h, slice.h, checked.h, and contract.h. WP re-emits the
relevant obligations at the new call sites. The full memory.h residual
set (VERIFY-008's 23 inherited + 20 own = 43 total) is re-emitted as
arena.h's inherited surface — arena.h, the first header to include
memory.h transitively in WP scope, demonstrates that memory.h's
residuals propagate as a unit just as the headers below it do.

| # | Source            | Goals | Notes                                                            |
|---|-------------------|-------|-------------------------------------------------------------------|
| 1 | VERIFY-002        | 2     | checked.h u64 add overflow — via mem_alloc_array_checked          |
| 2 | VERIFY-006 cat 2  | 3     | ptr.h align_up/down/padding integer theory                        |
| 3 | VERIFY-006 cat 3  | 3     | ptr.h ptr_align_* call-chain                                      |
| 4 | VERIFY-006 cat 4  | 2     | contract.h handler non-termination                                |
| 5 | VERIFY-007 cat 1  | 12    | slice.h memcmp valid/danglingness (init closed, VERIFY-012)       |
| 6 | VERIFY-007 cat 2  | 1     | slice.h str_from_cstr strlen valid_string                         |
| 7 | VERIFY-008 cat 1  | 5     | memory.h \fresh / \freeable (mem_alloc, mem_free, array_checked)  |
| 8 | VERIFY-008 cat 2  | 9     | memory.h integer theory (mem_align, mem_is_aligned, etc.)         |
| 9 | VERIFY-008 cat 3  | 6     | memory.h memcmp danglingness (mem_compare/equal/equal_bytes; init closed, VERIFY-012) |

**Inherited subtotal: 43 goals.** Byte-identical to memory.h's full
residual list. The composable-verification claim, now empirically
supported at three composition layers (ptr.h → slice.h with 2
inherited; ptr.h/checked.h/slice.h → memory.h with 23 inherited;
memory.h+substrate → arena.h with 43 inherited), is that substrate
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

CI enforces exactly 89 unproved goals with the named goal patterns
covering all 43 inherited and all 46 arena.h-own residuals. Any
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
23 substrate residuals; 31 before VERIFY-012's upstream closures);
memory.h+substrate → arena.h (arena.h inherited memory.h's full
43-goal residual list byte-identically). Each downstream header's
inherited count equals the upstream total, not greater. arena.h is the
first header to demonstrate propagation-through-two-composition-layers
— memory.h's own 20 residuals re-emerge as part of arena.h's inherited
43, along with memory.h's own 23 inherited (= memory.h's full residual
surface).
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
| **Scope**      | pool.h — 113 goals across 6 categories (4 own + 2 inherited groups) |
| **Category**   | Formal verification completeness |

**Description**: 113 of 3902 proof obligations (2.90%) are not discharged by
any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3 4.15.2 +
CVC5 1.2.1) with a 120-second timeout and `-wp-model Typed+Cast`. All 113 are
triple-prover-resistant. The goals split cleanly into two top-level groups:
**89 inherited from already-verified substrate headers** (re-emerging because
pool.h includes arena.h, which transitively pulls in memory.h, ptr.h, slice.h,
checked.h, and contract.h) and **24 pool.h-own** residuals in four categories.

pool.h extends the composable-verification thesis to a fourth data point and
the first observation across a **two-hop transitive include**. memory.h
established the thesis (23 inherited + 20 own); arena.h re-confirmed it (43
inherited + 46 own); pool.h re-confirms it again (89 inherited + 24 own).
arena.h's entire VERIFY-009 residual surface (43 substrate + 46 arena.h-own =
89) re-emerges byte-identically in the pool.h proof run — zero new substrate
residuals are introduced at the pool.h boundary. pool.h is a **sibling** of
arena.h in the include graph sense at the memory.h layer, but a **descendant**
of arena.h itself (`pool.h` includes `arena.h`), so its inherited surface is
arena.h's *total*, not memory.h's. Every inherited residual matches an
already-documented goal by name; the CI wrapper enforces this with per-goal
pattern checks.

### Inherited residuals (89)

These goals are not pool.h defects. They re-emerge in the pool.h proof run
because pool.h includes arena.h, which transitively includes memory.h, ptr.h,
slice.h, checked.h, and contract.h. WP re-emits the relevant obligations at
the new call sites. The full arena.h residual set (VERIFY-009's 43 inherited +
46 own = 89 total) is re-emitted as pool.h's inherited surface.

| # | Source            | Goals | Notes                                                              |
|---|-------------------|-------|--------------------------------------------------------------------|
| 1 | VERIFY-002        | 2     | checked.h u64 add overflow — via mem_alloc_array_checked / checked_mul |
| 2 | VERIFY-006 cat 2  | 3     | ptr.h align_up/down/padding integer theory                         |
| 3 | VERIFY-006 cat 3  | 3     | ptr.h ptr_align_* call-chain                                       |
| 4 | VERIFY-006 cat 4  | 2     | contract.h handler non-termination                                 |
| 5 | VERIFY-007 cat 1  | 12    | slice.h memcmp valid/danglingness (init closed, VERIFY-012)        |
| 6 | VERIFY-007 cat 2  | 1     | slice.h str_from_cstr strlen valid_string                          |
| 7 | VERIFY-008 cat 1  | 5     | memory.h \fresh / \freeable (mem_alloc, mem_free, array_checked)   |
| 8 | VERIFY-008 cat 2  | 9     | memory.h integer theory (mem_align, mem_is_aligned, etc.)          |
| 9 | VERIFY-008 cat 3  | 6     | memory.h memcmp danglingness (mem_compare/equal/equal_bytes; init closed) |
| 10| VERIFY-009 cat 2a | 8     | arena.h ptr_span call-site (arena_alloc, arena_alloc_aligned)      |
| 11| VERIFY-009 cat 2b | 26    | arena.h fits/does_not_fit arithmetic chain                         |
| 12| VERIFY-009 cat 2c | 10    | arena.h zero/try wrapper delegation                                |
| 13| VERIFY-009 cat 2d | 2     | arena.h arena_free_bytes helper call-site                          |

**Inherited subtotal: 89 goals.** Byte-identical to arena.h's full residual
list (VERIFY-009's 43 inherited + 46 own). The composable-verification claim,
now empirically supported at four composition layers (ptr.h → slice.h with 2
inherited; ptr.h/checked.h/slice.h → memory.h with 23 inherited;
memory.h+substrate → arena.h with 43 inherited; arena.h+substrate → pool.h
with 89 inherited), is that substrate residuals propagate without
amplification: a downstream header's inherited count equals the upstream's
total, not greater. pool.h is the first header to observe propagation through
two transitive include hops (pool → arena → memory → ...), and the 89 count
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

CI enforces exactly 113 unproved goals with the named goal patterns covering
all 89 inherited and all 24 pool.h-own residuals. Any additional unproved
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
89-goal residual surface byte-identically (zero new substrate residuals
introduced), and adds 24 own residuals concentrated at the ptr_elem /
bytes_from / pool_invariant call sites that pool.h's header comment predicted.
The 89:24 inherited:own ratio is the most lopsided yet — a direct consequence
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



## VERIFY-011: WP Limitations Inherited from Substrate Plus region_end Opaque-Hook-Dispatch Residuals (region.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-011 |
| **Date**       | 2026-06-06 |
| **Baseline commit** | c9172fc (Canon-C CI #992) |
| **Scope**      | region.h — 112 goals across 4 categories (2 own + 2 inherited groups) |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced (exact-count + by-name roll-call) as of CI #1022 |

**Description**: 112 of 3643 proof obligations (3.07%) are not discharged
by any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3
4.15.2 + CVC5 1.2.1) with a 120-second timeout, `-wp-split`, and
`-wp-model Typed+Cast`. The goals split into two top-level groups:
**89 inherited from already-verified substrate headers** (re-emerging
because region.h includes arena.h, which transitively pulls in memory.h,
ptr.h, slice.h, checked.h, and contract.h) and **23 region.h-own**
residuals in two categories.

region.h extends the composable-verification thesis to a fifth
composition layer. pool.h established the deepest prior point (89
inherited + 24 own across a two-hop include); region.h is a **sibling**
of pool.h at the arena.h layer (both include arena.h; neither includes
the other). Its inherited surface is arena.h's *total* — VERIFY-009's
43 inherited + 46 arena.h-own = 89 — re-emitted byte-identically. Zero
new substrate residuals are introduced at the region.h boundary. Every
inherited residual matches an already-documented goal by name.

**Enforcement status — ENFORCED (as of CI #1022).** region.h's
`frama-c-region` job now fails the build on any deviation from 3531/3643
proved or 112 unproved, and additionally roll-calls all 23 own goals
plus a representative inherited sample by name. It was promoted from its
original report-only baseline (the first observed 120s/`-wp-split`
completion, which carried 126 residuals) once the residual set proved
name-stable: VERIFY-012's 14 initialization closures left the inherited
surface (126 → 112) without disturbing the 23 region-own goals, and the
19 region_end `-wp-split` `_partN` fragments near the 120s boundary all
reproduce by name across runs. The 112 baseline supersedes the original
126. The job runs ~1h40m in an isolated runner with `timeout-minutes:
350` headroom.

### Inherited residuals (89)

These goals are not region.h defects. They re-emerge in the region.h
proof run because region.h includes arena.h, which transitively
includes memory.h, ptr.h, slice.h, checked.h, and contract.h. The full
arena.h residual set (VERIFY-009's 43 inherited + 46 own = 89 total)
is re-emitted as region.h's inherited surface.

| # | Source            | Goals | Notes                                                              |
|---|-------------------|-------|--------------------------------------------------------------------|
| 1 | VERIFY-002        | 2     | checked.h u64 add overflow — via mem_alloc_array_checked           |
| 2 | VERIFY-006 cat 2  | 3     | ptr.h align_up/down/padding integer theory                         |
| 3 | VERIFY-006 cat 3  | 3     | ptr.h ptr_align_* call-chain                                       |
| 4 | VERIFY-006 cat 4  | 2     | contract.h handler non-termination                                 |
| 5 | VERIFY-007 cat 1  | 12    | slice.h memcmp valid/danglingness (init closed, VERIFY-012)        |
| 6 | VERIFY-007 cat 2  | 1     | slice.h str_from_cstr strlen valid_string                          |
| 7 | VERIFY-008 cat 1  | 5     | memory.h \fresh / \freeable                                        |
| 8 | VERIFY-008 cat 2  | 9     | memory.h integer theory (mem_align, mem_is_aligned, etc.)          |
| 9 | VERIFY-008 cat 3  | 6     | memory.h memcmp danglingness (init closed, VERIFY-012)             |
| 10| VERIFY-009 cat 2a | 8     | arena.h ptr_span call-site                                         |
| 11| VERIFY-009 cat 2b | 26    | arena.h fits/does_not_fit arithmetic chain                        |
| 12| VERIFY-009 cat 2c | 10    | arena.h zero/try wrapper delegation                               |
| 13| VERIFY-009 cat 2d | 2     | arena.h arena_free_bytes helper call-site                         |

**Inherited subtotal: 89 goals.** Byte-identical to arena.h's full
residual list. The composable-verification claim, now supported at five
composition layers, holds: region.h's inherited count equals arena.h's
total, not greater.

### region.h-own residuals (23)

Two categories. Category 1 (the dominant class) is the region_end
opaque-hook-dispatch family; category 2 is region_invariant
re-establishment arithmetic on the trivial mutators.

#### Category 1: region_end opaque-hook-dispatch family (19)

| #  | Goal                                                          |
|----|----------------------------------------------------------------|
| 1  | `typed_cast_region_end_terminates_part2`                       |
| 2  | `typed_cast_region_end_terminates_part3`                       |
| 3  | `typed_cast_region_end_ensures_2_part1`                        |
| 4  | `typed_cast_region_end_ensures_4_part1`                        |
| 5  | `typed_cast_region_end_exits_part1`                            |
| 6  | `typed_cast_region_end_loop_invariant_preserved_part1`         |
| 7  | `typed_cast_region_end_assert_rte_function_pointer`            |
| 8  | `typed_cast_region_end_assert_rte_mem_access_6`                |
| 9  | `typed_cast_region_end_assert_rte_mem_access_7`                |
| 10 | `typed_cast_region_end_loop_assigns_part2`                     |
| 11 | `typed_cast_region_end_loop_assigns_part4`                     |
| 12 | `typed_cast_region_end_loop_assigns_part5`                     |
| 13 | `typed_cast_region_end_loop_assigns_part6`                     |
| 14 | `typed_cast_region_end_loop_assigns_part7`                     |
| 15 | `typed_cast_region_end_loop_assigns_part8`                     |
| 16 | `typed_cast_region_end_assigns_exit_part5`                     |
| 17 | `typed_cast_region_end_assigns_normal_part5`                   |
| 18 | `typed_cast_region_end_loop_variant_decrease_part1`            |
| 19 | `typed_cast_region_end_call_arena_reset_requires`             |

**Functions affected**: `region_end`.

**Root cause**: region_end dispatches caller-supplied cleanup hooks
through an opaque function pointer `h->fn(h->ctx)` (line 509) for which
no `calls` clause can exist — the hook is arbitrary by design. WP
reports the boundary directly during the proof run:

```
[wp] core/region.h:509: Warning: Missing 'calls' for default behavior
[wp] core/region.h:509: Warning:
  Unknown callee, considering non-terminating call
[wp] core/region.h:509: Warning:
  Missing decreases clause on recursive function region_end, call must be unreachable
[wp] core/region.h:509: Warning:
  \valid_function not yet implemented
  (rte: function_pointer: \valid_function(h->fn))
```

Because WP cannot rule out that an opaque hook calls region_end itself,
it models region_end as potentially recursive (the `terminates_*` and
`loop_variant_decrease_*` goals), and because the hook could — as far
as WP can prove — write through a pointer aliasing `r`, it cannot
discharge the function's `assigns *r` frame against the indirect call
(the `assigns_*`, `loop_assigns_*`, `ensures_*`, `exits_*` goals). The
`assert_rte_function_pointer` goal is the `\valid_function(h->fn)` RTE
check, which WP reports as not-yet-implemented. This is the documented
region.h verification boundary — see OWN-003 for the architectural
decision to accept it rather than redesign the hook mechanism, and the
region.h header comment (the region_end verification note) which names
this entry.

**Manual proof argument**: region_end's structural postconditions
(`r->open == false`, `r->num_hooks == 0`, `r->arena == null`,
`region_invariant(r)`) are re-established by unconditional writes
*after* the hook loop (lines 514, 520, 522), and the attached arena is
captured into a loop-immune local (`saved_arena`, line 476) before the
loop so arena_reset discharges against the local rather than a
hook-havoc'd field. The hook contract — a hook must not call region_end
on this region or repoint r's fields — makes the modelled recursion
vacuous. The C is correct by construction under that contract; the
obstacle is that WP has no `calls` clause to encode it, and
`\valid_function` is unimplemented in Frama-C 29. This is a verifier
feature gap plus a deliberate API-generality choice, not a code defect.

#### Category 2: region_invariant re-establishment on trivial mutators (4)

| # | Goal                                              |
|---|----------------------------------------------------|
| 1 | `typed_cast_region_begin_ensures_5`               |
| 2 | `typed_cast_region_attach_arena_ensures_2`        |
| 3 | `typed_cast_region_register_ensures_part2`        |
| 4 | `typed_cast_region_set_parent_ensures_2`          |

**Functions affected**: `region_begin`, `region_attach_arena`,
`region_register`, `region_set_parent`.

**Root cause**: each goal is the `ensures region_invariant(r)`
postcondition on a trivial mutator. region_invariant composes
arena_invariant (`r->arena == \null || arena_invariant(r->arena)`), so
re-establishing it carries arena.h's invariant-composition weight into
region.h's run. The residual is the cost of composing the substrate
invariant, the same class as the inherited arithmetic-chain goals, not
a region.h logic gap.

**Manual proof argument**: each mutator either leaves r->arena NULL
(region_begin sets the whole struct to `{0}`) or stores a pointer the
caller already proved satisfies arena_invariant (region_attach_arena's
`requires arena_invariant(arena)`), so the `r->arena == \null ||
arena_invariant(r->arena)` disjunct holds at the postcondition by
construction. The hook-count bound (`0 <= num_hooks <=
REGION_MAX_CLEANUP`) is maintained by the explicit guard in
region_register. The C matches the predicate; the obstacle is WP
carrying arena_invariant's composed body through the postcondition.

### Summary of region.h-own residuals

| Category | Goals | Functions affected                                            | WP feature gap                                   |
|----------|-------|---------------------------------------------------------------|--------------------------------------------------|
| 1        | 19    | region_end                                                    | opaque function-pointer dispatch: no `calls` clause, `\valid_function` unimplemented, modelled recursion |
| 2        | 4     | region_begin, region_attach_arena, region_register, region_set_parent | region_invariant / arena_invariant composition weight |
| **Total**| **23**|                                                               |                                                  |

### Mitigation

The `frama-c-region` CI step enforces exactly 112 unproved goals (and
3531/3643 proved) with named patterns covering all 89 inherited and all
23 own residuals, matching the other eight headers, and prints the full
residual list on every run (artifact `wp-proof-region`). Any additional
unproved goal, missing expected goal, or count change is a regression
and fails the build.

region.h achieves 100% line coverage (45/45) and 95.5% MC/DC (21/22) —
the achievable ceiling under MCDC-005 (the single uncovered outcome is
the `if (h->fn)` FALSE branch, API-unreachable; see MCDC-005). Region
behavior is tested by the unit suite in `test/core/region_test.c`
covering begin/end, arena attachment and auto-reset, LIFO hook
dispatch, the hook-table-full path, parent tracking, ID/open/hook-count
inspection, the static-lifetime fast path, and lifetime_assert_valid.
ASan + UBSan across all 16 CI configs verify absence of UB. The runtime
lifetime substrate (OWN-001) covers region under CANON_LIFETIME_DEBUG.

The composable-verification claim is confirmed at a fifth composition
layer: region.h inherits arena.h's full 89-goal residual surface
byte-identically (zero new substrate residuals introduced) and adds 23
own residuals, 19 of which concentrate on the single function
(region_end) that region.h's header comment names as the verification
boundary. The WP boundary landed exactly where the design predicted it.

### Cross-references

- Inherited residuals: VERIFY-002 (checked.h), VERIFY-006 (ptr.h),
  VERIFY-007 (slice.h), VERIFY-008 (memory.h), VERIFY-009 (arena.h).
- Architectural decision for the region_end boundary: OWN-003.
- MC/DC coverage: MCDC-005 (region.h MC/DC ceiling 95.5%, the
  `if (h->fn)` FALSE branch unreachable).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag).
- Substrate runtime tracking: OWN-001, OWN-002.
- Composable verification thesis: see README, "Composable
  verification" section.
- Per-goal CI artifact: `wp-proof-region` (full WP output).
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: core/region.h".

---

## VERIFY-012: Contract-Strengthening Closure of Initialization Preconditions (slice.h, memory.h, and downstream)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-012 |
| **Date**       | 2026-06-13 |
| **Baseline commit** | b78768e (Canon-C CI #1022) |
| **Scope**      | 50 enforced goals closed across five verified headers (64 including region report-only at the time of authoring); zero executable change |
| **Category**   | Specification correction |

**Description**: A targeted contract-strengthening pass closed 50 enforced
unproved goals — 8 in slice.h and 6 in memory.h directly, plus their
re-emitted inherited copies in arena.h, pool.h, and region.h — by adding
`\initialized` as an explicit input precondition where WP is able to discharge
it. No function body changed; the closures are ACSL-only, and the preprocessed
C is byte-identical before and after.

**Trigger**: A correspondence from Allan Blanchard (CEA, a core Frama-C
developer) pointed out that WP's warning text —

> Allocation, initialization and danglingness not yet implemented

— conflates three distinct features. Of the three, **`\initialized` is
verifiable** in Frama-C 29 (and has been since roughly Frama-C 22) *provided
the contract states it as an input precondition*: WP cannot synthesize
initialization out of nothing, but once a `requires \initialized(...)` clause
asserts it at the call boundary, WP propagates and discharges it. The other
two features named in the warning — `\dangling` (and the related `\fresh` /
`\freeable` / `\allocable` allocation predicates) — are genuinely unimplemented
in Frama-C 29. The original VERIFY-007 and VERIFY-008 records treated all three
as a single immovable feature gap; that was an over-broad reading of the
warning.

**What changed**:

- **slice.h** — 4 guarded, implication-form `requires` added to the equality
  functions `bytes_equal`, `str_equal`, `str_starts_with`, `str_ends_with`
  (the `str_ends_with` clause uses the window range `(s.len - suffix.len ..
  s.len - 1)`). These close the 8 `initialization_*` memcmp call-site goals
  (two per function) that VERIFY-007 category 1 formerly carried. slice.h
  residuals: 23 → 15.
- **memory.h** — 6 behavior-local `\initialized` requires added to
  `mem_compare`, `mem_equal`, `mem_equal_bytes` (two per function). These close
  the 6 `initialization_*` goals that VERIFY-008 category 3 formerly carried.
  memory.h residuals: 57 → 43 (23 inherited + 20 own).
- **Downstream inheritance closures** (no edits to these headers — their
  inherited surface simply shrank as the upstream goals vanished):
  arena.h 103 → 89, pool.h 127 → 113, region.h 126 → 112.

Project totals moved from 18269 → 18333 proved and 463 → 399 unproved; total
obligations (18732) unchanged.

**Enforced totals** (the five exact-count CI wrappers, all re-baselined and
green at CI #1022): slice 375/390, memory 2819/2862, arena 3383/3472, pool
3789/3902, region 3531/3643. region.h was additionally promoted from
report-only to enforced in the same pass, the residual set having proved
name-stable across the closure (the 14 initialization goals left the inherited
surface without disturbing region's 23 own goals).

**Freeze compliance**: All modules are frozen at v1.2.0. ACSL annotations are
C comments — they are stripped by the preprocessor before compilation, so the
preprocessed translation unit is byte-identical before and after this pass.
The runtime behavior the new preconditions describe was already enforced by
the existing Valgrind job (which flags uninitialized reads at execution time);
VERIFY-012 only states in ACSL what the dynamic analysis already checked. The
change is therefore freeze-compliant.

**Consumer caveat**: The new `requires \initialized(...)` clauses are input
preconditions. A verified caller of `mem_compare` / `mem_equal` /
`mem_equal_bytes` or slice.h's equality functions now inherits the obligation
to establish initialization of the compared ranges at its own call site — which
is the correct, sound contract (comparing partially-uninitialized buffers was
never well-defined). Callers that were already passing fully-initialized
buffers are unaffected.

**Correction to prior records**: VERIFY-008's summary previously asserted that
"strengthening memory.h's predicates would produce no improvement." That claim
is **falsified** and has been corrected in place: stating `\initialized`
explicitly closed 6 memory.h goals (and 8 slice.h goals) with no executable
change. What remains genuinely WP-blocked in Frama-C 29 is `\dangling`,
`\fresh`, `\freeable`, and `\allocable` — confirmed by A. Blanchard. The
danglingness, allocation, and freeability residuals documented in VERIFY-007
through VERIFY-011 are unaffected by this pass and remain open on the verifier
feature gap.

**Cross-references**: VERIFY-007 (slice.h, 23 → 15), VERIFY-008 (memory.h,
57 → 43, "no improvement" claim corrected), VERIFY-009 (arena.h, 103 → 89),
VERIFY-010 (pool.h, 127 → 113), VERIFY-011 (region.h, 126 → 112, promoted to
enforced). Per-goal CI artifacts: `wp-proof-{slice,memory,arena,pool,region}`
at CI #1022. Wrapper enforcement: `.github/workflows/cmake-multi-platform.yml`,
the five `frama-c-{slice,memory,arena,pool,region}` steps.

---

## VERIFY-014: Function-Pointer-Dispatch and Inherited Residuals (option, first driver-verified module)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-014 |
| **Date**       | 2026-06-27 |
| **Baseline commit** | e2d908d (Canon-C CI #1067, enforced); report-only first at 83cfb16 (CI #1065) |
| **Scope**      | semantics/option/ via `vmacros/vdrivers/option_verify.h` — 223 obligations, 34 unproved across 2 categories (1 own + 1 inherited group) |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced (exact-count + by-name roll-call) as of CI #1067 |

**Description**: 34 of 223 proof obligations (15.2%) on the option driver
are not discharged by any prover in the triple-prover configuration
(Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1) with a 120-second timeout and
`-wp-split`. option is the **first driver-verified module** — WP is
pointed at `vmacros/vdrivers/option_verify.h`, which instantiates the
real shipped `IMPL_OPTION_*` macros at `CANON_OPTION(int)` and attaches
ACSL contracts to the concrete generated prototypes (see
`docs/vmacros.md` for the mechanism). It is also the **first Shape-B
module verified** and the first semantics/ header to carry residuals at
all (error.h, VERIFY-013, proved 65/65 clean).

option uses the default `Typed` memory model — the by-value tagged
struct `{ bool has_value; T value; }` performs no `void*` casts, so the
cast-aware model is unnecessary. As with error.h, the option CI wrapper
omits the CVC5-presence check the `Typed+Cast` headers carry.

The 34 split into **2 inherited from contract.h** and **32 option-own**
function-pointer-dispatch residuals.

### Inherited residuals (2)

| # | Source           | Goals | Pattern                                                          |
|---|------------------|-------|------------------------------------------------------------------|
| 1 | VERIFY-006 cat 4 | 2     | `typed_contract_default_handler_terminates`, `typed_contract_default_handler_loop_invariant_established` |

option is the first **semantics/** header to inherit the contract.h
handler non-termination pair (it reaches contract.h through `expect`'s
`_CANON_INVOKE_HANDLER` path). These are the same two goals documented
in VERIFY-006 category 4 — the deliberate ACSL idiom for a non-returning
panic handler, unprovable by construction.

### option-own residuals (32)

**Category 1 — combinator function-pointer dispatch (32)**

**Functions affected**: `option_int_map`, `option_int_and_then`,
`option_int_filter`, `option_int_combine_with` (6 goals each = 24);
`option_int_or_else`, `option_int_eq` (4 goals each = 8).

**Root cause**: each of the six combinators dispatches a caller-supplied
function pointer (`f`, `pred`, `combine`, `fallback`, `eq`) for which no
`calls` clause can exist — the callee is arbitrary by design. WP reports
the boundary directly during the proof run:

~~~
[wp] vmacros/vdrivers/option_verify.h:283: Warning:
  Unknown callee, considering non-terminating call
[wp] vmacros/vdrivers/option_verify.h:283: Warning:
  \valid_function not yet implemented
  (rte: function_pointer: \valid_function(f))
~~~

Because WP cannot rule out that an opaque callee fails to terminate or
writes through an aliasing pointer, it cannot discharge the combinators'
`terminates`, `exits`, `assigns`, and `assert_rte_function_pointer`
goals. The per-function residuals are the `_terminates_partN`,
`_exits_partN`, `_assigns_{normal,exit}_partN`, and
`_assert_rte_function_pointer` fragments visible in the WP log. This is
the same root cause and the same class of residual as region_end's
opaque-hook dispatch (OWN-003 / VERIFY-011 category 1) — different
surface (six combinators vs one teardown loop), identical limitation:
no `calls` clause for an arbitrary callee, plus `\valid_function`
unimplemented in Frama-C 29.

**Manual proof argument**: each combinator's structural postconditions
(the result's `has_value` and `value` relationship to the inputs) hold
by construction under the contract that the supplied callback is a valid
function that terminates and does not repoint the option. WP has no ACSL
mechanism to encode that callback contract, so the obligations remain
residual. The functional shape is exercised exhaustively by
`test/semantics/option_test.c` and by the cover TU (MCDC-006); ASan +
UBSan across all 16 CI configs verify absence of UB on every combinator
path.

Every non-combinator function proved fully: `some`, `none`, `is_some`,
`is_none`, `get`, `unwrap`, `unwrap_or`, `replace`, `take`, and
`expect`'s reachable surface carry no residuals.

### Mitigation

The `frama-c-option` CI step enforces exactly 34 unproved goals (and
189/223 proved) with named patterns covering the 2 inherited and all 32
own residuals, and prints the full residual list on every run (artifact
`wp-proof-option`). Any additional unproved goal, missing expected goal,
or count change is a regression and fails the build.

option achieves 96.7% MC/DC (29/30 condition outcomes — see MCDC-006).
Functional behavior is tested by `test/semantics/option_test.c` and
exercised for coverage by `vmacros/coverage/option_cover.c`. The single
uncovered MC/DC outcome — `option_int_expect`'s panic-on-absent guard —
is cross-confirmed by this entry's residual analysis: WP reports
`Missing decreases clause on recursive function option_int_expect, call
must be unreachable`, the same branch gcov measures as not-executed (see
MCDC-006 for the cross-stream record).

### Cross-references

- Inherited residuals: VERIFY-006 (contract.h handler non-termination).
- Architectural analogue: OWN-003 / VERIFY-011 category 1 (region_end
  opaque-hook dispatch — same function-pointer limitation).
- MC/DC coverage: MCDC-006 (option_cover.c ceiling 96.7%, the `expect`
  panic guard unreachable).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag).
- Driver mechanism: `docs/vmacros.md` (Shape-B verification drivers).
- Per-goal CI artifact: `wp-proof-option` (full WP output).
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: semantics/option (driver)".


---

## VERIFY-015: Function-Pointer-Dispatch and Inherited Residuals, Plus Union-Model Hypothesis (result, second driver-verified module)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-015 |
| **Date**       | 2026-07-03 |
| **Baseline commit** | 6516ae5 (Canon-C CI #1090, enforced); report-only first at b528515 (CI #1089) |
| **Scope**      | semantics/result/ via `vmacros/vdrivers/result_verify.h` — 215 obligations, 30 unproved across 2 categories (1 own + 1 inherited group), plus a union-model standing hypothesis carrying no goals |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced (exact-count + by-name roll-call) as of CI #1090 |

**Description**: 30 of 215 proof obligations (13.95%) on the result
driver are not discharged by any prover in the triple-prover
configuration (Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1) with a
120-second timeout and `-wp-split`. result is the **second
driver-verified module** (after option, VERIFY-014) — WP is pointed at
`vmacros/vdrivers/result_verify.h`, which instantiates the real shipped
`IMPL_RESULT_*` macros at `CANON_RESULT(int, VErr)` (`VErr` a
driver-local enum, chosen so T and E are distinct types and the payload
union genuinely carries two member types) and attaches ACSL contracts to
the concrete generated prototypes (see `docs/vmacros.md`). It is the
second Shape-B module verified and the **first union-typed module** —
result's payload is `union { T ok; E err; } val`, where option's is a
plain struct field. All 30 unproved goals are Timeouts; 0 Unknown, 0
Failed.

result uses the default `Typed` memory model — the by-value tagged
struct `{ bool is_ok; union { T ok; E err; } val; }` performs no `void*`
casts, so the cast-aware model is unnecessary. As with error.h and
option, the result CI wrapper omits the CVC5-presence check the
`Typed+Cast` headers carry.

The 30 split into **2 inherited from contract.h** and **28 result-own**
function-pointer-dispatch residuals. There are **no union-related
unproved goals** — see the union-model hypothesis below, which is the
entry's third section precisely because it carries no goal count.

### Inherited residuals (2)

| # | Source           | Goals | Pattern                                                          |
|---|------------------|-------|------------------------------------------------------------------|
| 1 | VERIFY-006 cat 4 | 2     | `typed_contract_default_handler_terminates`, `typed_contract_default_handler_loop_invariant_established` |

The same contract.h handler non-termination pair as VERIFY-006 category
4 and VERIFY-014 — with one difference from option worth recording.
option reaches the handler through `expect`'s `_CANON_INVOKE_HANDLER`
path, which survives `-DCANON_NO_REQUIRE`; result's entire panic surface
(`unwrap`, `unwrap_err`, `expect`, and the `get_ok`/`get_err` NULL
out-pointer guards) routes exclusively through `require_msg`, which the
flag compiles to `((void)0)`. In the verified configuration **no result
function contains a handler call at all**: the 2 goals are pure
definition-presence inheritance (the handler's definition is in the TU
via the transitive contract.h include, and WP emits goals for every
defined function), and — unlike option's `expect` — no result function
carries handler-call or unreachable-recursion goals of its own. All
fifteen non-handler-related, non-combinator obligations surfaces prove
fully (see the own-residual section for the twelve fully-proved
functions).

### result-own residuals (28)

**Category 1 — combinator function-pointer dispatch (28)**

**Functions affected**: `result_int_VErr_map`,
`result_int_VErr_map_err` (6 goals each = 12);
`result_int_VErr_and_then`, `result_int_VErr_or_else` (4 goals each =
8); `result_int_VErr_eq` (8 goals).

**Root cause**: each of the five dispatching combinators calls a
caller-supplied function pointer (`f`, `eq_ok`, `eq_err`) for which no
`calls` clause can exist — the callee is arbitrary by design. WP reports
the boundary directly during the proof run:

~~~
[wp] vmacros/vdrivers/result_verify.h:380: Warning:
  Unknown callee, considering non-terminating call
[wp] vmacros/vdrivers/result_verify.h:380: Warning:
  \valid_function not yet implemented
  (rte: function_pointer: \valid_function(f))
~~~

Because WP cannot rule out that an opaque callee fails to terminate or
writes through an aliasing pointer, it cannot discharge the combinators'
`terminates`, `exits`, `assigns`, and `assert_rte_function_pointer`
goals. This is the same root cause and the same class as option's 32
combinator residuals (VERIFY-014) and region_end's opaque-hook dispatch
(OWN-003 / VERIFY-011 category 1): no `calls` clause for an arbitrary
callee, plus `\valid_function` unimplemented in Frama-C 29. It is a
verifier feature gap, not a prover-strength residual — every category-1
goal resolves at Qed in 2–6ms and then buckets as unprovable, the same
deterministic name-stable signature as option's.

The per-function cluster sizes are structural, not incidental:
`map`/`map_err` carry 6 each because the calling branch *rewraps* the
callee's return through the known `ok`/`err` constructor (extra
`assigns` split fragments); `and_then`/`or_else` carry 4 because they
return the callee's Result directly; `eq` carries 8 because it
dispatches **two** pointers across two calling branches (`both_ok` →
`eq_ok`, `both_err` → `eq_err`), producing one
`assert_rte_function_pointer` goal per pointer — the largest own
cluster, against option_int_eq's 4 with one calling branch. The
asymmetric `-wp-split` fragment numbering
(`map…assigns_normal_part2` vs `map_err…assigns_normal_part3`;
`and_then…assigns_part1` vs `or_else…assigns_part2`) is a split
artifact, pinned by name in the CI roll-call as observed.

**Manual proof argument**: each combinator's structural postconditions
(the result's `is_ok` and active-member relationship to the inputs)
prove on the non-calling branch, and on the calling branch hold by
construction under the contract that the supplied callback is a valid
function that terminates and does not repoint the result. WP has no ACSL
mechanism to encode that callback contract, so the obligations remain
residual. The functional shape is exercised exhaustively by
`test/semantics/result_test.c` and by the cover TU (MCDC-007); ASan +
UBSan across all 16 CI configs verify absence of UB on every combinator
path.

Every non-dispatching function proved fully: `ok`, `err`, `is_ok`,
`is_err`, `get_ok`, `get_err`, `unwrap_or`, `unwrap`, `unwrap_err`,
`expect`, `and`, and `or` carry no residuals — including `and`/`or`'s
cross-value union-member postconditions
(`other.is_ok ==> \result.val.ok == other.val.ok`, etc.).

### Union-model hypothesis (no goals)

result is the first verified module whose payload is a **union**, and
its run introduces a documentation obligation that is not a goal count.
All union-member postconditions **proved** — the constructors'
compound-literal union writes, the passthrough branches' member
equalities, and `and`/`or`'s cross-value guarded implications. However,
WP emitted 14+ `[wp:union]` warnings during the run:

~~~
[wp:union] vmacros/vdrivers/result_verify.h:183: Warning:
  Accessing union fields with WP might be unsound.
  Please refer to WP manual.
~~~

WP's Typed model reasons about union members in a way that is unsound in
the presence of **type punning** — reading a member other than the
last-written one. The manual argument for validity here is that the
generated result code never type-puns: every union write is paired with
the `is_ok` value that selected the member, and every read — in the
generated bodies (guarded by the `is_ok` tests/ternaries), in the
driver's preconditions (`requires r.is_ok` before any spec reads
`.val.ok`, and symmetrically for `.val.err`), and in the guarded
`ensures` implications — occurs only under the matching `is_ok`. The
punning scenario WP warns about is unreachable in the verified code.
This discipline is cross-checked by the two other evidence streams,
which execute the same read-after-matching-write pattern:
`result_test.c` (functional assertions) and the MC/DC cover stream
(MCDC-007).

This is a **standing hypothesis** in the spirit of the
LIMITATION-SUSPECTED tagging (VERIFY-010): the union-member proofs are
valid conditional on the no-punning discipline, which WP cannot itself
establish. The `[wp:union]` warnings are expected output of an
enforced-green run. Triggers to revisit: a change in the warnings'
character, any union goal turning unprovable, or any future result
variant that reads an inactive member (none exists in the shipped
macros). Future union-typed modules inherit this hypothesis pattern and
must restate the discipline argument for their own read sites.

### Mitigation

The `frama-c-result` CI step enforces exactly 30 unproved goals (and
185/215 proved) with named patterns covering the 2 inherited and all 28
own residuals, and prints the full residual list on every run (artifact
`wp-proof-result`). Any additional unproved goal, missing expected goal,
or count change is a regression and fails the build.

result achieves 100% MC/DC (28/28 condition outcomes — see MCDC-007,
the first Shape-B module with no unreachable outcome; its
require_msg-only panic surface vanishes entirely under
`-DCANON_NO_REQUIRE`, so no analogue of option's MCDC-006 ceiling
exists). Functional behavior is tested by
`test/semantics/result_test.c` (two instantiations, including a
by-value-struct payload) and exercised for coverage by
`vmacros/coverage/result_cover.c` through the same `(int, VErr)`
instantiation the proof uses.

### Cross-references

- Inherited residuals: VERIFY-006 (contract.h handler non-termination);
  VERIFY-014 (option's inherited pair — note the
  `_CANON_INVOKE_HANDLER` vs `require_msg` reachability difference
  recorded above).
- Architectural analogue: VERIFY-014 (option's 32 combinator
  residuals); OWN-003 / VERIFY-011 category 1 (region_end opaque-hook
  dispatch — same function-pointer limitation).
- Union-model hypothesis precedent: VERIFY-010 (LIMITATION-SUSPECTED
  tagging for goals resting on a manual review).
- MC/DC coverage: MCDC-007 (result's clean 28/28 audit; attribution of
  generated conditions to the driver header).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag).
- Driver mechanism: `docs/vmacros.md` (Shape-B verification drivers).
- Per-goal CI artifact: `wp-proof-result` (full WP output).
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: vmacros/vdrivers/result_verify.h".

---

## VERIFY-016: Inherited-Substrate and memcmp-Danglingness Residuals (borrow.h, fourth semantics/ module, verified in place)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-016 |
| **Date**       | 2026-07-05 |
| **Baseline commit** | 262a503 (Canon-C CI #1110, clean run); report-only first at 383bf9f (CI #1109) |
| **Scope**      | semantics/borrow.h non-macro surface — 2452 obligations, 19 unproved across 2 categories (1 own + 1 inherited group) |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced (exact-count + by-name roll-call) as of #1111 |

**Description**: 19 of 2452 proof obligations (0.77%) on borrow.h's
translation unit are not discharged by any prover in the triple-prover
configuration (Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1) with a
120-second timeout and `-wp-split`, under the `Typed+Cast` memory model
(forced transitively by slice.h's casts, plus borrow.h's own
`(const u8 *)` cast in `borrowed_bytes_slice`). All 19 are Timeouts;
0 Unknown, 0 Failed.

borrow.h is the fourth semantics/ module verified (after error.h,
VERIFY-013, and the option/result driver modules) and the second
semantics/ header verified **in place** (after error.h). Its 24
non-macro functions (`borrowed_ptr` ×6, `borrowed_str` ×9,
`borrowed_bytes` ×9, counting the two `_from_lifetime` variants) are
annotated in place, while `DEFINE_BORROWED_SLICE` is outside the
verified surface — it sits on the single-file macro-family list in
`docs/vmacros.md`, whose disposition is deliberately open. This
in-place-verified-surface + parked-macro-family structure is not new:
slice.h established it with DEFINE_SLICE (VERIFY-007), and borrow.h is
the first semantics/ header to carry it. (Note the single-file macro
families are a category distinct from the multi-file Shape-B driver
modules — see `docs/vmacros.md`'s classification — so the split here
is in-place-verified vs. parked, not Shape A vs. Shape B.)

**Verified configuration.** The verified build is the default
configuration (`CANON_LIFETIME` off, `-DCANON_NO_REQUIRE -DNDEBUG`):
the `BORROW_LT_FIELDS_` / `BORROW_LT_CHECK_` / `BORROW_LT_INHERIT_`
macros expand to nothing, so WP verifies the exact shipped bodies the
v1.2.x ABI guarantee describes. The lifetime-debug path is
runtime-only by construction (OWN-001 §7) and remains validated by
`test/semantics/borrow_test.c` across all 16 CI configs, including
the OWN-002 double-reset regression guards. The `source` debug tag
appears in no validity clause anywhere in the header's contracts:
borrow.h never dereferences it, so to the specs it is inert data —
stored and propagated, never read through.

**Specification vocabulary.** borrow.h defines **no ACSL predicates of
its own**: every contract uses slice.h's exported predicates verbatim
(`cbytes_invariant`, `str_invariant`, `str_valid`). The first landing
(CI #1109) carried two borrow-local predicates and 8 additional
residuals — all call-site precondition (or delegated-ensures) goals
into slice.h functions whose contracts had been mirrored by shape
rather than by sight (`cbytes_from`'s two requires clauses,
`str_slice`'s and `str_equal`'s `str_valid` requires, and a full
content-equality postcondition on `borrowed_str_eq` that `str_equal`'s
deliberately partial spec cannot support). Aligning the four affected
contracts to slice.h's real clauses (262a503) closed all 8 and removed
the duplicate predicates; `borrowed_str_eq` now carries `str_equal`'s
partial ensures trio with an in-source comment recording why —
contrast `borrowed_bytes_eq`, whose self-contained body carries (and
proves) the full content-equality postcondition.

### Inherited residuals (17)

| # | Source                          | Goals | Pattern                                                          |
|---|---------------------------------|-------|------------------------------------------------------------------|
| 1 | VERIFY-007/-012 (slice.h)       | 13    | memcmp call-site valid/danglingness at `bytes_equal`, `str_equal`, `str_starts_with`, `str_ends_with` (12) + `str_from_cstr` strlen `valid_string` (1) |
| 2 | VERIFY-006 cat 4 (via slice.h)  | 2     | `typed_cast_contract_default_handler_terminates`, `typed_cast_contract_default_handler_loop_invariant_established` |
| 3 | VERIFY-002 (checked.h)          | 2     | `typed_cast_checked_add_overflow_ensures`, `typed_cast_checked_add_u64_overflow_ensures` — prefix flipped `typed_` → `typed_cast_` under this TU's model, the same flip the memory.h/region.h runs exhibit |

This is the **sixth composability data point**, and the smallest
inherited surface since memory.h: borrow.h's TU pulls in checked.h +
slice.h only (slice.h includes neither ptr.h nor memory.h), so no
alignment, allocation, or arena-chain residuals exist to inherit. All
17 re-emitted **byte-identically** (names verified one-for-one by the
CI wrapper's inherited roll-call on both #1109 and #1110) — the
substrate's residual fingerprint propagated unchanged through a new
downstream header, again.

### borrow-own residuals (2)

**Category 1 — memcmp call-site danglingness (2)**

**Function affected**: `borrowed_bytes_eq` — the header's only direct
libc call.

**Goals**: `typed_cast_borrowed_bytes_eq_call_memcmp_requires_danglingness_s1`,
`typed_cast_borrowed_bytes_eq_call_memcmp_requires_danglingness_s2`.

**Root cause**: the `\dangling` feature gap in Frama-C 29 (Blanchard)
— the same class as VERIFY-007's memcmp sites and VERIFY-008's
`mem_compare`/`mem_equal` goals, at one new call site. Notably, the
`valid_s1/s2` and `initialization` goals at the same call site
**closed**: borrow.h is the first header to carry guarded
`\valid_read` and `\initialized` preconditions (in implication form
mirroring the short-circuit structure) from day one rather than as a
VERIFY-012-style retrofit, and they discharged exactly as VERIFY-012
predicted for authored-at-annotation-time clauses.

**Manual proof argument**: on the path reaching memcmp, the two views
have equal non-zero lengths, distinct non-NULL pointers, and
`\valid_read`/`\initialized` regions established by precondition; a
pointer that is `\valid_read` over the compared range in the call
state is not dangling. ASan and Valgrind execute the same call on
every `_eq` test path across the CI matrix.

**Zero new residual classes.** borrow.h is the first downstream header
since error.h to add **no new residual class**: its own residuals are
strictly the known memcmp class at one new site. It has no combinators
or hooks, so — unlike option, result, and region_end — no
function-pointer-dispatch residuals exist.

The named in-body assertion `dead_by_invariant` (inside
`borrowed_bytes_eq`'s one-NULL guard) **proved**, formally discharging
the guard as dead code under `cbytes_invariant` — the cross-stream
half of MCDC-008 (see that entry).

### Mitigation

The `frama-c-borrow` CI step enforces exactly 19 unproved goals (and
2433/2452 proved) with a by-name roll-call of all 17 inherited and
both own residuals, plus an inverted check that `dead_by_invariant`
remains proved; the full residual list prints on every run (artifact
`wp-proof-borrow`). Landed report-only at CI #1109 per the
option/result promotion pattern; the residual set was name-stable
across #1109/#1110 (the 19 are the identical named subset of #1109's
27, the 8 delta being the contract-alignment closures described
above).

borrow.h's gcov-measured MC/DC is 38/40 = 95.0%, its documented
ceiling — see MCDC-008. Functional behavior, including the
lifetime-debug configuration this entry does not verify, is tested by
`test/semantics/borrow_test.c` (all 16 configs; ASan + UBSan on
Linux/macOS debug builds) and fuzzed via its `CANON_FUZZING` entry
point (`borrowed_bytes_slice` clamping and the
`borrowed_slice_int_as_bytes` overflow guard).

### Cross-references

- Inherited residuals: VERIFY-007/-012 (slice.h memcmp/strlen
  surface); VERIFY-006 category 4 (contract.h handler pair);
  VERIFY-002 (checked.h overflow pair; prefix flip).
- Own-residual class precedent: VERIFY-007 (memcmp danglingness),
  VERIFY-008 (mem_compare/equal sites); `\dangling` feature-gap
  rationale in VERIFY-012's closing note.
- MC/DC coverage: MCDC-008 (the 38/40 ceiling; the `dead_by_invariant`
  named-assert closure; the CANON_NO_REQUIRE-gated `_get(NULL)` tests
  that closed the three reachable outcomes at CI #1106).
- Substrate decision record: OWN-001 §7 (macro-templated vs non-macro
  verification posture — this entry is the cross-reference §7
  promised); OWN-002 (double-reset regression guards in the runtime
  evidence stream).
- Macro-family disposition: `docs/vmacros.md` single-file list
  (`DEFINE_BORROWED_SLICE` remains parked).
- Per-goal CI artifact: `wp-proof-borrow` (full WP output).
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: semantics/borrow.h".

---

## VERIFY-017: libc Byte-View and Inherited Residuals, Plus Trusted Stdio Axioms (diag.h, fifth semantics/ module, verified in place)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-017 |
| **Date**       | 2026-07-08 |
| **Baseline commit** | bb269f9 (Canon-C CI #1134, actions run 28967245082); report-only first at d8566d5 (CI #1132, 59 unproved, 300s), re-measured name-stable at 1597a51 (CI #1133, 59 unproved, 120s) |
| **Scope**      | semantics/diag.h — 3060 obligations, 10 unproved across 2 categories (1 own + 1 inherited pair), plus 2 documented trusted axioms |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced (exact-count + by-name roll-call) as of 1965b23 (#1135) |

**Description**: 10 of 3060 proof obligations (0.33%) on diag.h's
translation unit are not discharged by any prover in the triple-prover configuration (Alt-Ergo 2.6.3 + Z3 4.15.2
+ CVC5 1.2.1) with a 120-second timeout and `-wp-split`, under the
`Typed+Cast` memory model. The model is forced by diag.h's **own**
byte-view casts — the `(char *)` views of `Diag` and `DiagFrame` at
the memset/memmove sites and in `push_no_alias`'s `\separated` clause
(WP cast warnings at the diag_init memset, the diag_push memmove pair,
and the `(char *)d` separation term) — making diag.h the first
semantics/ header to *originate* the Typed+Cast requirement rather
than inherit it transitively. All 10 are Timeouts at the pinning run;
the diag_init memset goal oscillates between Unknown and Timeout
across runs (Unknown at #1132, Timeout at #1133 and the pinning run) —
the wrapper deliberately counts the two classes together, so the
oscillation is pin-safe.

diag.h is the fifth semantics/ module verified (after error.h,
VERIFY-013; the option/result driver modules, VERIFY-014/-015; and
borrow.h, VERIFY-016) and the third verified **in place**. Its 13
`static inline` functions are annotated in place. The four call-site
expansion macros (`DIAG_PUSH`, `DIAG_PUSH_FMT`, `DIAG_RETURN_IF`,
`DIAG_PROPAGATE`) are outside the verified surface — they sit on the
single-file macro-family list in `docs/vmacros.md` (the
DEFINE_SLICE/DEFINE_BORROWED_SLICE disposition), exercised by
`test/semantics/diag_test.c`.

**Verified configuration.** The verified build is the default shipped
configuration (`-DCANON_NO_REQUIRE -DNDEBUG`): `require_msg` in
diag_push and `DIAG_PUSH_FMT` compiles out, so contract.h contributes
only the definition-presence handler pair (see inherited residuals).
The stdio surface is verified against the two `__FRAMAC__`-gated
redeclarations described under **Trusted stdio axioms** below; the
real `<stdio.h>` never sees them.

**Round-1 chronology and spec alignment.** The report-only first run
(CI #1132) returned 2968/3027 with 59 unproved. 49 of the 59 traced to
two clauses in the round-1 trusted stdio specs, not to the code or the
in-place contracts: (a) a `valid_read_string(format)` requires on the
snprintf/fprintf redeclarations minted 32 caller obligations that time
out even on string literals under this model — 8 at diag_print's
fprintf, 8 at diag_render_frame's snprintf, 16 at diag_render's
snprintf, the 8/8/16 fan-out being `-wp-split` over the three
field-defaulting ternaries in each call's argument list (2³ = 8; ×2
for diag_render's `total < buf_size` branch); and (b) the round-1
snprintf axiom havocked `buf[0 .. size-1]` with no termination
ensures, leaving WP unable to re-establish the `rf_terminated` /
`r_terminated` postconditions after the call (17 goals). An
intermediate run (CI #1133, workflow-only change, 120s) re-measured
the round-1 header goal-for-goal identically to #1132's 300s run,
establishing the residual set as name-stable across both timeouts and
refuting the time-starvation hypothesis directly. The pinning run
replaces (a) with `format != \null` (Qed-instant at every call site —
all pass literals) and closes (b) with a conditional termination
ensures on the axiom plus a termination loop invariant in diag_render:
59 → 10, with the total growing 3027 → 3060 (the render_terminated
loop-invariant goals and the re-labeled `fmt_nonnull` obligations, all
proved). The 10 remaining goals are the identical named subset of
#1132's/#1133's 59 — name-stable across three runs. The toolchain was
byte-identical throughout (same opam cache key, same Why3 1.7.x with
its "version not recognized" warnings for all three provers — the same
warnings every enforced baseline in the project was measured under):
the 49-goal collapse was produced by specification alignment alone,
which empirically closes the question of whether the residuals were a
prover-recognition artifact.

### Trusted stdio axioms (documented, deliberate)

The `__FRAMAC__`-gated redeclarations of `snprintf` and `fprintf` are
**assumed specifications**: extern prototypes with no body, which WP
takes as axioms and never proves. They exist because Frama-C 29's
stock variadic handling emits `assigns *(buf+(0..))` — an open range
the Typed+Cast model rejects with 'Invalid infinite range', aborting
the run — and they require `-variadic-no-translation` in the WP job so
the spec binds to the untranslated call. Two clauses are deliberate
judgment calls and are recorded here as part of the trusted base:

1. **`format != \null` only** (no `valid_read_string`): the string
   validity of the format literal is not asserted, because the goal is
   triple-prover-resistant under this model and every call site passes
   a literal whose validity is a compiler guarantee, not a proof
   target.

2. **Unconditional termination ensures** (`size > 0 ==> ∃k < size:
   buf[k] == '\0'`): ISO C99 guarantees null-termination on the
   success path but is silent on the encoding-error path (`\result <
   0`). Asserting termination unconditionally rests on the
   environmental assumption that these format strings with valid
   arguments do not provoke encoding errors on any hosted libc — **the
   same assumption, cited by both evidence streams**, that MCDC-009
   records for the permanently-uncovered `n < 0` true side in
   diag_render (line 968 at d8566d5). One assumption, two records,
   both pointing here. The variadic arguments themselves are invisible
   to the axiom (untranslated variadics carry no argument spec), so no
   read-validity of the rendered fields is assumed or checked at the
   axiom level; those are covered by `frame_strings_ok` preconditions
   on the callers.

Round-1 clauses removed at pinning: the `\from` dependency lists
(ignored by WP; carried an open `format[0 ..]` range as a parse
hazard) and `ensures \result >= -1` (stronger than ISO C, which
permits any negative value on error; no code path depends on it — both
render functions branch only on `n < 0`).

### Inherited residuals (2)

| # | Source                          | Goals | Pattern |
|---|---------------------------------|-------|---------|
| 1 | VERIFY-006 cat 4 (contract.h)   | 2     | `typed_cast_contract_default_handler_terminates`, `typed_cast_contract_default_handler_loop_invariant_established` |

Definition-presence inheritance only, as with option/result
(VERIFY-014/-015): under `-DCANON_NO_REQUIRE` no diag function
contains a handler call, so the pair is emitted for the handler's own
definition. The prefix is `typed_cast_` (not `typed_` as in the
option/result drivers) because this TU's model is Typed+Cast — the
same prefix flip VERIFY-016 recorded for borrow.h's checked.h pair.
This is the **smallest inherited surface of any residual-carrying
header** (2 goals): diag.h's TU pulls in types.h, contract.h, and
error.h only — error.h is VERIFY-013 clean and contributes nothing —
exactly as the round-1 prediction anticipated. Seventh composability
data point.

### diag-own residuals (8)

**Category 1 — libc byte-view through Typed+Cast (8)**

**Functions affected**: `diag_init` (1), `diag_push` (7).

**Goals**:

- `typed_cast_diag_init_call_memset_requires_valid_s` (Unknown) —
  `\valid` of the `(char *)` byte view over a local `Diag` at the
  memset call.
- `typed_cast_diag_push_call_memmove_requires_valid_dest`,
  `typed_cast_diag_push_call_memmove_requires_valid_src` — byte-view
  validity over `DiagFrame` subarrays at the overflow-shift memmove.
- `typed_cast_diag_push_valid_diag_ensures_push_shift_semantics_part05`
  / `_part06` / `_part07` — field-level `.code` equality across the
  shift; the memmove axiom moves bytes, and reconstructing typed field
  equalities from a byte-level copy is the known byte-spec-vs-Typed
  reasoning gap (VERIFY-007/-008 class) at a new site. The spec
  deliberately pins **only** `.code` across the shift
  (`push_shift_semantics`) — full-frame fidelity
  (file/func/line/message) is a deliberately weak spec in the
  error_message sense, runtime-verified by diag_test.c's overflow
  tests rather than formally specified, precisely because the byte-
  level memmove spec cannot support it.
- `typed_cast_diag_push_valid_diag_assigns_exit_part02`,
  `typed_cast_diag_push_valid_diag_assigns_normal_part03` — framing of
  the byte-level memmove write against the typed
  `assigns d->frames[0 .. DIAG_MAX_FRAMES - 1]` clause; same root
  cause.

**Root cause**: all 8 are the libc byte-view class — Frama-C's
memset/memmove specs are stated over `(char *)` views, and relating
byte-range validity/writes to typed struct locations is the documented
Typed+Cast reasoning gap (VERIFY-007 memcmp sites, VERIFY-008
mem_copy/mem_move surface) at two new sites. This matches round-1
predicted class (a) exactly; predicted class (b) (stdio-intrinsic
residuals) **vanished at pinning** — with sound axioms, the stdio
surface contributes zero residuals, so no new residual class enters
the project ledger. Notably, the depth-tracking goals *through* the
memmove all proved (`push_depth_bounds`, `push_depth_step`,
`push_overflow_flag`, and the `dead_by_invariant_clamp` assertion —
see MCDC-009): WP frames `d->depth` correctly past the byte-level
write; only the frame-content and byte-range-validity goals resist.

**Manual proof argument**: on the overflow path, `d` is `\valid` by
precondition, so its full object — including the `(char *)` byte view
of `frames[0 .. DIAG_MAX_FRAMES-1]` and any subarray — is valid for
read and write; the memmove copies `(DIAG_MAX_FRAMES-1) ×
sizeof(DiagFrame)` bytes from `&frames[1]` to `&frames[0]`, which is
precisely a one-slot field-preserving shift, so `frames[j].code ==
\old(frames[j+1].code)` for all shifted `j`, and every byte written
lies inside the typed assigns footprint. For diag_init, `d` is a local
of complete type `Diag`, valid over its full `sizeof` by construction.
ASan and Valgrind execute the overflow shift on every
`test_push_overflow*` path across the CI matrix.

### Mitigation

The `frama-c-diag` CI step enforces exactly 10 unproved goals
(and 3050/3060 proved) with a by-name roll-call of both inherited
and all 8 own residuals, plus an inverted check that
`dead_by_invariant_clamp` remains in the proved set (absence from the
unproved list is the proof — WP prints only unproved goals by name);
the full residual list prints on every run (artifact `wp-proof-diag`).
Landed report-only at CI #1132 per the option/result/borrow promotion
pattern; the pinned set is the identical named subset of #1132's/
#1133's 59, the 49-goal delta being the trusted-spec alignment
described above.
`-wp-timeout` is 120s as for every other enforced header (the round-1
runs used 300s while the spec noise was being diagnosed; with 59
unproved goals at 300s × 3 sequential provers the job ran 3h20m —
reverted at pinning).

diag.h's gcov-measured MC/DC is 84/86 = 97.67%, its documented ceiling
— see MCDC-009. Functional behavior, including the full-frame overflow
shift this entry's weak spec does not pin and the truncation paths, is
tested by `test/semantics/diag_test.c` (all CI configs; ASan + UBSan
on Linux/macOS debug builds) and fuzzed via its `CANON_FUZZING` entry
point.

### Cross-references

- Inherited residuals: VERIFY-006 category 4 (contract.h handler
  pair); definition-presence-only precedent VERIFY-014/-015; prefix
  flip precedent VERIFY-016.
- Own-residual class precedent: VERIFY-007/-008 (libc byte-spec vs
  Typed reasoning); VERIFY-003 (Unknown-class residuals).
- Trusted axioms: this entry's "Trusted stdio axioms" section is the
  normative record; the shared environmental assumption is cited by
  MCDC-009 (the `n < 0` permanent measurement residual).
- MC/DC coverage: MCDC-009 (84/86 ceiling; `dead_by_invariant_clamp`
  named-assert closure; denominator history).
- Macro-family disposition: `docs/vmacros.md` single-file list (the
  four DIAG_* macros).
- Per-goal CI artifact: `wp-proof-diag` (full WP output).
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: semantics/diag.h".

---

---

## VERIFY-018: Inherited Mega-TU Surface Plus Macro-Body-Loop, Element-Transfer, Bridging, and Allocation-Model Residuals (vec, third driver-verified module, first data/-layer module)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-018 |
| **Date**       | 2026-07-12 |
| **Baseline commit** | 96dd41d (Canon-C CI #1152, run 3); report-only chronology 1eeb58c (CI #1150, run 1) → 8a3bb1e (CI #1151, run 2); enforced as of e663e2c (CI #1154) |
| **Scope**      | data/vec/ via `vmacros/vdrivers/vec_verify.h` — 5380 obligations, 196 unproved (143 inherited across 8 source families + 53 vec-own across 4 categories) |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced (pinned Proved line + zero-Failed + exact count + by-name roll-call) as of CI #1154 |

**Description**: 196 of 5380 proof obligations (3.64%) on the vec driver
are not discharged by any prover in the triple-prover configuration
(Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1) with a 120-second timeout and
`-wp-split`; 193 are Timeouts, 3 Unknowns, and **0 Failed** — no
contract is falsified. vec is the **third driver-verified module**
(after option, VERIFY-014, and result, VERIFY-015), the **first
data/-layer module verified**, and the **first driver on the
`Typed+Cast` model**: unlike option/result (by-value structs, plain
Typed), vec crosses `void*` boundaries — `(int*)mem_alloc(bytes)` and
`int*` buffers through `mem_copy`/`mem_move`'s `void*` parameters. WP
runs over `vmacros/vdrivers/vec_verify.h`, which instantiates the real
shipped macros at `int` via the `DEFINE_VEC_STRUCTS` /
`DEFINE_VEC_FUNCTIONS` split (finding F3 below; `docs/vmacros.md`).
The verified configuration is the default shipped build
(`-DCANON_NO_REQUIRE -DNDEBUG`, `CANON_LIFETIME` off): vec's entire
panic surface (3 `require_msg`, 10 `ensure_msg`, zero direct handler
calls) compiles away, so the driver's `requires` clauses are
load-bearing and contract.h contributes only the definition-presence
handler pair.

vec's TU is the **largest verified to date** (5380 goals): the facade
pulls slice.h, ptr.h, arena.h → memory.h and the full substrate, plus
the option and result instantiations the driver composes. Its
196-goal residual set is correspondingly the largest, but decomposes
exactly and was name-stable across three consecutive runs before
enforcement (runs 2, 3, and the enforced run 4).

### Run chronology and method lessons (recorded forward for deque/hashmap)

**Run 1** (1eeb58c, CI #1150, 5h21m): 4988/5350, 362 unproved. Root
cause **R1 — spec-less callees**: the driver's uncontracted lifetime
helpers and bare option/result instantiations made WP treat every
constructor call as potentially assigning the world, drowning goals
that were individually provable. The cure was **driver composition**
(contracting the composed instantiations), not weaker vec specs.
**Lesson**: in a mega-TU, an unproved-goal explosion is more likely a
spec-less-callee cascade than a contract-quality problem — check the
`Missing 'calls'` / `assigns everything` warnings before touching
ensures clauses.

**Run 2** (8a3bb1e, CI #1151, 2h52m): 5208/5413, 205 unproved. The
composition fix collapsed the noise; the inherited surface stabilized
at 143 names. vec-own stood at 62.

**Run 3 — baseline** (96dd41d, CI #1152, 2h52m): 5184/5380, 196
unproved (193 Timeout + 3 Unknown + 0 Failed). A delegate-narrowing
relaxation closed 9 vec-own goals (62 → 53) as predicted; the two
retry-variance candidates failed a **third consecutive run** and were
pinned rather than excused. Inherited 143 identical name-for-name with
run 2 — the stability evidence enforcement requires.

**Run 4 — enforced** (e663e2c, CI #1154, 2h50m): green on the first
enforced run, with the 196 set name-identical for the third
consecutive run. The run also validated the gate design empirically:
prover attribution shifted (Alt-Ergo 524 → 534, CVC5 8 → 4, Z3
43 → 37) without any name change; the driver's diagnostics moved 14
lines (a comment-only commit) without any name change; and two
roll-call lines emitted in swapped order — all three variances are
absorbed by name-only, order-independent matching and would have made
line-, count-per-prover-, or order-sensitive pinning flaky.

### Inherited residuals (143)

Re-emissions of already-documented residual classes under this TU's
`Typed+Cast` model (names carry the `typed_cast_` prefix), by source
family:

| # | Source family | Goals | Documented under |
|---|---------------|-------|------------------|
| 1 | arena.h own surface (ptr_span call-sites, fits/does_not_fit arithmetic chains, zero/try wrappers, free_bytes helpers) | 46 | VERIFY-009 |
| 2 | option combinator function-pointer dispatch | 32 | VERIFY-014 |
| 3 | result combinator function-pointer dispatch + union `get_*` mem-access pair | 22 | VERIFY-015 |
| 4 | memory.h own surface (allocation-model, alignment, memcmp danglingness) | 20 | VERIFY-008 |
| 5 | slice.h libc boundary (str_* / bytes_equal memcmp + strlen) | 13 | VERIFY-007 |
| 6 | ptr.h align call-sites | 3 | VERIFY-006 |
| 7 | checked.h / align model-variance re-emissions | 5 | VERIFY-002/-008 (see note) |
| 8 | contract.h handler pair (definition-presence only) | 2 | VERIFY-006 cat 4 |

**Model-variance note (family 7)**: `checked_add`/`checked_add_u64`
overflow-ensures and the three `align_*` ensures prove in their home
TUs but time out under this mega-TU's Typed+Cast context. They failed
three consecutive runs (including both retry-variance candidacies) and
are pinned as model-variance re-emissions, not reclassifications of
their home entries. Note the inherited surface is a **model-variant
subset**, not the byte-identical propagation of the in-place stack
(VERIFY-009/-010/-011 pattern): goal names re-derive under
`Typed+Cast` with `-wp-split` fragment numbering, so identity is
per-name within this TU across runs, not across TUs.

The **12 `rte_function_pointer` goals all sit on inherited
option/result combinators — zero on vec functions**, confirming
prediction (f): vec is the first module whose own surface contains no
OWN-003-class dispatch at all.

### vec-own residuals (53)

**Category (e) — allocation-model plumbing (2)**:
`vec_int_alloc_call_vec_int_init_requires_2` and
`vec_int_free_call_mem_free_requires` — the `\fresh`/`\freeable`
feature gap (VERIFY-008's allocation category) surfacing at vec's
heap constructor/destructor delegation, the precise analogs of
memory.h's own mem_alloc/mem_free pair. Falsifies the round-0
prediction of zero allocation-model residuals.

**Category (d) — element-transfer ensures (5)**: the `ok`-behavior
element equalities on `pop`, `insert`, `remove` (three fragments),
and `append_array`. Root cause: `mem_copy`/`mem_move` carry
**frame-only** byte-level contracts (VERIFY-008), so typed
`\old(...) == ...` element postconditions across the transfer cannot
be established — the same weak-spec shape as diag.h's
`push_shift_semantics` (VERIFY-017). Runtime-verified by
`test/data/vec_test.c`'s insert/remove/append content assertions and
the cover TU's shift legs.

**Category (g) — fill macro-body loop (24)**: `vec_int_fill`'s entire
goal cluster (terminates ×2, rte_mem_access ×8, assigns ×4,
live-ensures ×6, live-assigns ×4). Root cause: the `for` loop lives
inside the `IMPL_VEC_FILL` macro body, and **ACSL loop annotations
cannot survive macro definition** — the goals are unprovable by
construction regardless of contract quality, a new residual class
introduced by Shape-B modules whose bodies contain loops.
**Forward-flag**: deque's shift loops are pre-classified into this
class. This is the first class where MC/DC is the **primary**
evidence rather than corroboration: all three fill legs (truncating,
non-truncating, zero-count) are exercised in the cover TU (MCDC-010).

**Category (h) — Typed+Cast int↔char bridging (22)**: assigns-framing
and `mem_copy`/`mem_move` call-requires goals on `insert`, `remove`,
`append_array`, `extend`, and `swap` — the `(char*)`↔`(int*)`
byte-view bridging obligations the Typed+Cast model emits at vec's
typed-buffer/`void*` boundary (the `Cast with incompatible pointers
types (sint32*)↔(sint8*)` warnings in the WP log). Pinned after
failing three consecutive runs; the recorded shrink levers (wp-cache
replay, predicate-shape restatement) may be tried later without
unpinning.

### Findings for back-propagation

- **F1 (open)**: the driver's `\separated(src, v->items)` requires on
  `append_array`/`extend` makes self-append a contract violation, but
  `IMPL_VEC_APPEND_ARRAY`'s shipped doc comment carries no overlap
  prohibition — the shipped doc and the verified contract disagree.
  Doc patch owed upstream (comment-only, no verification impact).
- **F2 (open)**: `slice_init` on an `items==NULL` vec with `[0,0)`
  computes `&(NULL)[0]` — pedantic UB. The guard fix is one token
  (`!v->items` in the early return), semantics-preserving. Until it
  lands, the driver carries an exclusion `requires` and the cover TU
  deliberately does not exercise `slice_init(&e, 0, 0)` (MCDC-010's
  known exclusion). The F2 PR should land guard + cover-TU call +
  driver-exclusion removal together; expect the MCDC-010 denominator
  and possibly one RTE goal to move.
- **F3 (landed)**: the `DEFINE_VEC_STRUCTS` / `DEFINE_VEC_FUNCTIONS`
  split plus the corrected one-instantiation-per-TU rule shipped in
  `vec_defn.h` before the driver was drafted — the split-patch-first
  ordering is now the standing checklist item for deque.

### Prediction scorecard (honest record)

Confirmed: (a) exactly 2 inherited handler goals, definition-presence
only; (f) zero `rte_function_pointer` on vec functions; (d) the
element-transfer class (predicted 4, actual 5); the
`!checked_mul` dead-branch claim, discharged indirectly — `vec_int_alloc`
and `vec_int_arena_alloc` carry no unproved branch goals, so the
branch-dead facts proved within vec's own goals (the WP half of
MCDC-010's U1/U2 infeasibility rows). Falsified: "no ptr.h goals"
(11 enter through the arena → ptr_span edge both the scoping analysis
and the review missed: families 1-partial and 6 above) and "no
allocation-model residuals" (category (e) = 2). Newly discovered:
categories (g) and (h) — neither predicted, both now recorded forward.

### Enforcement and runtime

The `frama-c-vec` CI step enforces **set equality** through four
gates: (0) the pinned Proved line `5184 / 5380` (catches silent
goal-surface drift — a function dropping out of the TU leaves the
other gates satisfiable); (1) zero Failed verdicts ever (Failed means
a falsified contract, unlike Timeout/Unknown); (2) exact count 196;
(3) by-name roll-call of all 196 `CHECKS` names inline in the
workflow (option/result house style — with gate 2's exact count and
unique names, full presence equals set equality, so an invisible swap
fails loudly). Any change in either direction is a red run resolved
by the acknowledged ratchet — update the pin, `EXPECTED_UNPROVED`,
and `CHECKS` together in one commit. A goal flipping to Proved on a
fast prover day is a red run carrying good news: ratchet down, do not
widen. The wrapper retains the CVC5-presence warning (Typed+Cast
family) and runs under `timeout-minutes: 240`.

**Runtime record**: steady state is ~2h50m, and it is
timeout-dominated by construction — every proving goal completes in
≤332 ms (run-3/4 prover stats), so the wall time is essentially
193 × 120 s of scheduled timeout burn. `-wp-cache` was evaluated and
rejected (timeouts are re-attempted every run; the cache can only
replay the sub-second proving goals). Lowering `-wp-timeout` was
evaluated and deferred: 120 s is the campaign-wide constant every
baseline is measured at, and cross-module comparability was judged
worth the wall time as further modules land.

### Cross-references

- Inherited families: VERIFY-002/-006/-007/-008/-009 (substrate),
  VERIFY-014/-015 (combinators), VERIFY-006 cat 4 (handler pair).
- Element-transfer weak-spec analogue: VERIFY-017
  (`push_shift_semantics`).
- MC/DC: MCDC-010 (155/158 ceiling; U1/U2 WP-corroborated infeasible,
  U3 heap-environmental; third attribution variant).
- Driver mechanism and split patch: `docs/vmacros.md` (F3).
- Coverage methodology: MCDC-001 (`-DCANON_NO_REQUIRE`).
- Per-goal CI artifact: `wp-proof-vec` (full WP output).
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: vmacros/vdrivers/vec_verify.h (VERIFY-018, enforced)".


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

## MCDC-005: API-Unreachable Hook-Guard Branch (region.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-005 |
| **Date**       | 2026-06-06 |
| **Baseline commit** | c9172fc (Canon-C CI #992) |
| **Scope**      | region.h — 1 of 22 condition outcomes (1 unreachable) |
| **Category**   | Coverage measurement methodology |

**Description**: 1 of 22 condition outcomes in `core/region.h` is not
exercisable by tests. It is the FALSE side of the `if (h->fn)` hook
guard inside region_end's LIFO cleanup loop:

| # | Function     | Line | Subcondition not covered          |
|---|--------------|------|------------------------------------|
| 1 | `region_end` | 496  | cond 0 false (`!h->fn`)           |

region.h's gcov-measured MC/DC is 21/22 = 95.5%. This is the achievable
ceiling — the single uncovered outcome is unreachable by construction.

### Rationale

The `if (h->fn)` guard checks each cleanup slot before dispatching it.
The FALSE branch (a registered slot with a NULL `fn`) is unreachable
through the public API: `region_register` enforces `fn != NULL` as a
precondition (`require_msg(fn != NULL, ...)`) and only increments
`num_hooks` after storing a non-NULL `fn`, and region_end's loop visits
only filled slots `[0, num_hooks)`. Every slot the loop reaches
therefore has a non-NULL `fn`, so the guard's FALSE branch cannot fire.

The guard is defensive code preserved deliberately — it documents the
slot-validity contract at the dispatch site and protects against a
malformed Region whose hook array was corrupted or hand-constructed.
Removing it to satisfy MC/DC would weaken robustness against exactly
the caller-error class region.h exists to surface. The 1 uncovered
outcome is the gcov-measurement cost of keeping it.

This is the same disposition established by MCDC-002 (slice.h `!ptr`
branches), MCDC-003 (arena.h overflow guards), and MCDC-004 (pool.h
`!pool->arena` disjunct): the source-level shape differs (a
`!h->fn` hook-slot guard), but the cross-stream evidence pattern is
identical — gcov measures source-level uncoverage; the unreachability
follows from the no-NULL-hook property the register/dispatch pair
maintains. VERIFY-011 records that region_end carries WP residuals on
the *opaque-call* obligations (category 1) — those are a distinct
concern from this guard branch, which is a structural defensive
outcome, not a residual.

### Mitigation

1. **Reachability argument via the register/dispatch invariant**:
   region_register's `fn != NULL` precondition plus the
   `[0, num_hooks)` loop bound establish that every dispatched slot
   has a non-NULL `fn`. The FALSE branch is provably dead under normal
   operation.

2. **CI regression detector**: the coverage job's "Debug: per-line
   MC/DC detail for region.h" step prints the gcov dump every run. If
   a future change makes the branch reachable (e.g. a public API that
   can register a NULL `fn`), the per-line detail surfaces it.

3. **The achievable MC/DC ceiling is 21/22 = 95.5%**. Reaching it
   represents 100% of API-reachable coverage. The 1 missing outcome is
   documented here and not counted as a coverage regression.

4. **Region behavior is otherwise exhaustively tested**.
   `test/core/region_test.c` exercises the TRUE branch (hooks with
   non-NULL `fn` are dispatched LIFO), the empty-hook path, the
   hook-table-full path, arena auto-reset, and parent tracking.

### Cross-references

- VERIFY-011 — region.h's WP residual analysis; the region_end
  opaque-hook-dispatch residuals (category 1) are distinct from this
  guard branch.
- MCDC-001 — `-DCANON_NO_REQUIRE` coverage methodology.
- MCDC-002 / MCDC-003 / MCDC-004 — the same API-unreachable-defensive
  disposition in slice.h / arena.h / pool.h; different source-level
  shapes, same cross-stream pattern.
- Per-line gcov dump: CI artifact via the "Debug: per-line MC/DC
  detail for region.h" step in
  `.github/workflows/cmake-multi-platform.yml`.

---

## MCDC-006: Contract-Violation Panic-Branch Unreachable (option, first Shape-B cover TU)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-006 |
| **Date**       | 2026-06-27 |
| **Baseline commit** | 93bb107 (Canon-C CI #1072) |
| **Scope**      | `vmacros/coverage/option_cover.c` — 1 of 30 condition outcomes (1 unreachable) |
| **Category**   | Coverage measurement methodology |

**Description**: 1 of 30 condition outcomes measured through the option
cover TU is not exercisable by the cover driver. option is the **first
Shape-B module** to appear in the MC/DC record at all — its conditions
live in `IMPL_OPTION_*` macro bodies that have no source location until
`CANON_OPTION(int)` expands, so in `option_test.c` they are stamped to a
`/test/` path and deleted by the coverage filter. `option_cover.c`
re-instantiates the identical macros outside `test/`, stamping the same
conditions to `vmacros/coverage/option_cover.c`, where they survive (see
`docs/vmacros.md`). The single uncovered outcome is the FALSE side of
`option_int_expect`'s `has_value` guard:

| # | Function            | Subcondition not covered                  |
|---|---------------------|--------------------------------------------|
| 1 | `option_int_expect` | cond 0 true (`!o.has_value`) — panic path |

gcov reports `condition 0 not covered (true)` with the panic block
marked `%%%%%` (never executed). option_cover.c's measured MC/DC is
29/30 = 96.7% — the achievable ceiling.

Of the 30 outcomes, 26 are the generated option_int_* combinator conditions
and 4 are the cover driver's own scaffolding (half_if_even's even-check and
observe_opt's get-result branch), all 4 covered. The single miss is in the
generated set (expect). Every generated combinator decision point is fully covered: `eq` 4/4 + 2/2,
`combine_with` 4/4, `filter` 4/4 (T&&T / T&&F / F&&_), and
`or_else`/`and_then`/`map`/`get`/`unwrap_or` 2/2 each.

### Rationale

`expect`'s `has_value` FALSE outcome is the **contract-violation branch**:
when the option is absent, `expect` routes through the contract handler
and panics. Under the coverage build's project-wide `-DCANON_NO_REQUIRE`
this branch is the panic-on-absent path, deliberately not exercised —
exercising it means triggering the very contract violation the build
removes. The cover TU calls `option_int_expect` on a present option only
(line 126), with the rationale stated inline (lines 123-124): "the None
path is a panic; not exercised — it is the contract-violation branch,
not an MC/DC condition under NO_REQUIRE."

This is **not** a test gap and **not** a type-invariant-unreachable
branch. Its disposition differs from MCDC-002 through MCDC-005, and the
distinction is worth stating: those four are defensive branches proved
dead by a *type invariant* (`bytes_invariant` / `arena_invariant` /
`pool_invariant`) or a *runtime construction invariant* (region's
no-NULL-hook discipline). option's miss has no such predicate — it is a
panic branch, the same family as the `contract.h 0/2` artifact that
appears on every test row (MCDC-001's methodology, here surfaced inside
a generated function). An auditor should therefore not expect an
"option_invariant discharges this" claim; the unreachability is the
panic-handler's non-termination, not a predicate.

### Cross-stream evidence via VERIFY-014

WP and gcov point at the same branch from opposite directions. gcov
measures `expect`'s `!has_value` outcome as not-executed. WP, verifying
the same instantiation through the option driver, reports during its run:

~~~
[wp] vmacros/vdrivers/option_verify.h:283: Warning:
  Missing decreases clause on recursive function option_int_expect, call must be unreachable
~~~

WP models the handler call as a potentially-recursive call it cannot
prove terminating, and therefore treats it as unreachable. The two
streams complement rather than converge: gcov instruments the source and
finds the branch unexecuted; WP analyses the proof and finds the call
unreachable. Together they establish the outcome as a provably
unexecutable contract-violation path under the documented build flags,
not a coverage gap — the same cross-stream pattern as MCDC-002 through
MCDC-005, with the source of unreachability being the panic handler
rather than a type invariant.

### Mitigation

1. **Cross-stream evidence via VERIFY-014**: gcov reports source-level
   uncoverage; WP reports the `expect` handler call unreachable. Together
   they establish the 1 outcome as provably unexecutable, not a gap.

2. **CI regression detector**: the coverage job's "Debug: per-line MC/DC
   detail for option (cover TU)" step prints the gcov dump on every run.
   If a future change makes the panic branch reachable, or opens a new
   uncovered outcome, the per-line detail surfaces it for human review.

3. **The achievable MC/DC ceiling is 29/30 = 96.7%**. Reaching it
   represents 100% of cover-reachable coverage. The 1 missing outcome is
   documented here and not counted as a coverage regression.

4. **Option behavior is otherwise exhaustively tested**.
   `test/semantics/option_test.c` exercises every public function
   including both expect-on-present and the panic-on-absent path (the
   latter through the contract-handler test harness, which is excluded
   from the coverage build). The cover TU drives both outcomes of every
   reachable condition in every generated combinator.

### Forward note (result, vec, deque, fold)

option is the first of five Shape-B modules slated for cover TUs
(`docs/vmacros.md`). It confirms the cover-TU attribution pattern works
and establishes the disposition for a generated panic/contract-violation
branch. The remaining Shape-B modules inherit the pattern but not the
specific outcome count or shape; `result`'s `expect`/`unwrap` family is
the nearest analogue and will need its own audit and its own MCDC-NNN
entry. (Fulfilled by MCDC-007, which landed **clean** — result's panic
surface is `require_msg`-only and vanishes entirely under
`-DCANON_NO_REQUIRE`, so no expect-style branch survives into its
measured set; the analogy held for the audit obligation, not the
outcome.)

### Cross-references

- VERIFY-014 — option's WP residual analysis; the `expect` "must be
  unreachable" warning that cross-confirms this outcome, and the 32
  combinator function-pointer-dispatch residuals.
- MCDC-001 — `-DCANON_NO_REQUIRE` coverage methodology; the
  contract-violation-branch family this entry belongs to.
- MCDC-002 — slice.h's per-header forward note, which anticipated each
  new module needing its own MC/DC entry; option is the first Shape-B
  fulfillment.
- MCDC-003 / MCDC-004 / MCDC-005 — the same cross-stream evidence
  pattern in arena.h / pool.h / region.h; different unreachability
  source (type / construction invariant vs option's panic handler).
- Driver and cover-TU mechanism: `docs/vmacros.md`.
- Per-line gcov dump: CI artifact via the "Debug: per-line MC/DC detail
  for option (cover TU)" step in
  `.github/workflows/cmake-multi-platform.yml`.

---

## MCDC-007: Clean Shape-B Audit — No Unreachable Outcomes; Generated Conditions Attributed to the Driver Header (result)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-007 |
| **Date**       | 2026-07-03 |
| **Baseline commit** | b528515 (Canon-C CI #1089) |
| **Scope**      | result module via `result_cover` — 28 of 28 condition outcomes covered (0 unreachable) |
| **Category**   | Coverage measurement methodology |

**Description**: the result module's MC/DC audit is **clean** — all 28
condition outcomes measured through the result cover TU are covered,
with no unreachable outcome to document. This entry exists because
MCDC-006's forward note commits each Shape-B module to its own audit
record, and because result's audit produced two methodology findings
that future Shape-B modules (vec, deque, fold) must inherit correctly.

result is the second Shape-B module measured through the cover-TU
pattern. Its per-module attribution check (the report-only "Debug:
Shape-B attribution check for result" CI step) re-confirmed the pattern
before baselining: `result_test`'s gcov output attributes all 256 of the
module's measured condition outcomes to `result_test.c` (a `/test/`
path, deleted by the coverage filter), with `result_impl.h` owning none
— so the cover TU is the correct fix, exactly as on option.

**Finding 1 — no unreachable outcomes (contrast with MCDC-006).**
result's entire panic surface (`unwrap` on Err, `unwrap_err` on Ok,
`expect` on Err, the `get_ok`/`get_err` NULL out-pointer guards) routes
through plain `require_msg`, which the coverage build's
`-DCANON_NO_REQUIRE` compiles to `((void)0)`. Unlike option's `expect`
— whose `_CANON_INVOKE_HANDLER` invocation survives the flag and is
MCDC-006's single uncovered outcome — result's generated bodies under
the coverage flags contain **no panic branch at all**: `unwrap`,
`unwrap_err`, and `expect` are straight-line functions with zero
conditions. There is consequently no MCDC-006-style ceiling; 28/28 =
100% is both the measurement and the maximum. The cover TU calls
`unwrap`/`expect` on Ok values only and `unwrap_err` on Err only — with
the guards compiled out, a wrong-variant call would read the inactive
union member; those paths are contract violations, not conditions.

**Finding 2 — generated conditions are attributed to the driver
header.** The 28 outcomes split across two files:

| File                                  | Outcomes | Content |
|---------------------------------------|----------|---------|
| `vmacros/vdrivers/result_verify.h`    | 22/22    | the generated `result_int_VErr_*` conditions (get_ok, get_err, unwrap_or, map, map_err, and_then, or_else, and, or ×1 each; eq ×2) |
| `vmacros/coverage/result_cover.c`     | 6/6      | cover-driver scaffolding (`checked_double`'s threshold and `observe_res`'s two `get_*`-result branches) |

gcov attributes a macro expansion to the file containing the expansion
site, and result_cover.c takes its instantiation from the **driver
include** — `DEFINE_RESULT_FUNCTIONS(static inline, int, VErr)` sits at
`result_verify.h:380` — so the generated conditions are stamped to the
driver header, not to the including `.c`. Both paths live under
`vmacros/` and survive the `*/test/*` filter, so the measurement is
unaffected; but the per-file coverage table's row for the module's
generated conditions is **`result_verify.h`**, and regression diagnosis
must read `result_verify.h.gcov`, not `result_cover.c.gcov` (the CI
per-line debug step dumps both). This is the literal form of
`docs/vmacros.md`'s one-instantiation-two-consumers rule: the cover TU
adds only call sites; even the expansion site belongs to the driver.

The `contract.h 0/2` rows in the cover binary are the pre-existing
MCDC-001 artifact, unchanged.

### Cross-stream evidence via VERIFY-015

With no uncovered outcome there is no unreachability to close — the
cross-stream relationship here is **alignment of the measured set with
the proof set**. Both streams run under `-DCANON_NO_REQUIRE -DNDEBUG`,
so the condition set gcov measures (28 outcomes, no panic branches) is
the same body surface WP proves (185/215, no handler-call goals of
result's own): the two evidence streams agree that the require_msg
surface is absent from the verified/measured configuration, and the
runtime execution of every union read under its matching `is_ok` guard
is part of the operational evidence for VERIFY-015's union-model
hypothesis.

### Mitigation

1. **CI regression detector**: the coverage job's "Debug: per-line
   MC/DC detail for result (cover TU)" step prints the gcov dumps on
   every run; a future change that introduces an uncovered outcome (or
   re-introduces a panic branch into the measured set) surfaces there
   for human review, at which point this entry is amended from clean
   audit to gap record.
2. **Attribution check retained**: the "Debug: Shape-B attribution
   check for result" step remains report-only in the workflow as the
   per-module confirmation record.
3. **28/28 is not a ceiling claim** — it is full coverage of the full
   measured set. No outcome is excluded from the denominator.

### Forward note (vec, deque, fold)

The remaining three Shape-B modules inherit both findings: (a) audit
the panic surface's routing — a `require_msg`-only surface yields a
clean audit like result's, a handler-invocation path yields an
MCDC-006-style ceiling; do not assume either. (b) Expect the generated
conditions under the module's `*_verify.h` driver header, with only
scaffolding under `*_cover.c`, whenever the cover TU takes its
instantiation from the driver include. option's records predate this
finding and describe its 30 outcomes as option_cover.c's; re-audit
option's per-line artifact to determine whether its 26 generated
outcomes are likewise attributed to `option_verify.h`, and align
MCDC-006's wording if so.

### Cross-references

- VERIFY-015 — result's WP residual analysis and the union-model
  hypothesis this audit's runtime evidence supports.
- MCDC-006 — option's ceiling entry and the forward note this entry
  fulfills; the attribution wording flagged for re-audit above.
- MCDC-001 — `-DCANON_NO_REQUIRE` methodology; the `contract.h 0/2`
  artifact family.
- Attribution mechanism and one-instantiation-two-consumers rule:
  `docs/vmacros.md`.
- Per-line gcov dump: CI artifact via the "Debug: per-line MC/DC
  detail for result (cover TU)" step in
  `.github/workflows/cmake-multi-platform.yml`.

---

## MCDC-008: Type-Invariant-Unreachable One-NULL Guard, First Named-Assert Cross-Stream Closure (borrow.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-008 |
| **Date**       | 2026-07-05 |
| **Baseline commit** | a76202d (Canon-C CI #1106, measurement); cross-stream closure at 262a503 (CI #1110) |
| **Scope**      | semantics/borrow.h — 2 of 40 condition outcomes (2 unreachable) |
| **Category**   | Coverage measurement methodology |

**Description**: 2 of 40 condition outcomes in `semantics/borrow.h`
are not exercisable by tests. They are the two NULL-true sides of the
one-NULL guard inside `borrowed_bytes_eq`:

| # | Function            | Line (at baseline) | Subcondition not covered        |
|---|---------------------|--------------------|----------------------------------|
| 1 | `borrowed_bytes_eq` | 758                | cond 0 true (`a.bytes.ptr == NULL`) |
| 2 | `borrowed_bytes_eq` | 758                | cond 1 true (`b.bytes.ptr == NULL`) |

The guard body (line 759, `return false`) shows `#####` — never
executed across the entire coverage run. (Line numbers cite the
measurement baseline a76202d; the ACSL pass at 262a503 shifted the
guard to line 996 with the named assert at 1005. The coverage debug
step identifies the guard by function and condition, not by line, so
the drift is cosmetic.)

borrow.h's gcov-measured MC/DC is 38/40 = 95.0%. This is the
achievable ceiling — the two uncovered outcomes are unreachable by
construction. The ceiling was reached at CI #1106, when three
`#ifdef CANON_NO_REQUIRE`-gated tests closed the three `_get(NULL)`
defensive-branch outcomes (35/40 → 38/40): those branches survive
`-DCANON_NO_REQUIRE` and are the shipped, documented release-mode
behavior ("NULL → safe empty"), so in the coverage build they are
reachable by design and were closed by tests, not documented as a
deviation — the inverse disposition of the two outcomes this entry
records.

### Rationale

The guard sits behind three earlier exits: lengths equal, length
non-zero, pointers distinct. Reaching either NULL-true outcome
therefore requires a `cbytes_t` with a NULL pointer and non-zero
length — exactly the malformed value `cbytes_invariant` forbids
(`ptr != \null || len == 0`) and `cbytes_from` refuses to construct
(it is a stated precondition there). No public path produces such a
value. Manufacturing one field-by-field in a test would exercise the
invariant violation the API exists to prevent — the MCDC-002
rationale, verbatim; the guard is defensive code preserved
deliberately, documenting the view-validity contract at the compare
site.

This is the **seventh cross-stream instance** (after MCDC-002/-003/
-004/-005/-006's differing source-level shapes) and the **first in
semantics/** on an in-place-annotated header. Its distinguishing
feature: the closure is carried by a **named in-body assertion** —
`/*@ assert dead_by_invariant: \false; */` placed inside the guard
body — which WP **proved** at CI #1110 under the `cbytes_invariant`
preconditions on `borrowed_bytes_eq` (a `\false` assertion is
provable exactly when the path condition is contradictory, i.e. the
path is infeasible). Prior closures established unreachability as a
consequence of invariant-preservation proofs; this one states it as a
single named goal the CI wrapper checks by name on every run.

### Mitigation

1. **Reachability argument via the type invariant**: `cbytes_invariant`
   plus the guard's position behind the length/pointer exits make the
   NULL-true outcomes contradictory; WP discharges the named
   `dead_by_invariant` assertion, formally establishing the path
   infeasible (VERIFY-016).

2. **Two regression detectors, one per evidence stream** — a first:
   the coverage job's "Debug: per-line MC/DC detail for borrow.h" step
   prints the gcov dump and a `not covered` quick-filter every run
   (measurement stream); the `frama-c-borrow` wrapper fails if
   `dead_by_invariant` ever leaves the proved set (proof stream). A
   future change making the branch reachable, or weakening the
   invariant, surfaces in both.

3. **The achievable MC/DC ceiling is 38/40 = 95.0%**, reached at
   CI #1106 and representing 100% of API-reachable coverage. The 2
   missing outcomes are documented here and not counted as a coverage
   regression.

4. **borrowed_bytes_eq is otherwise exhaustively tested**:
   `test/semantics/borrow_test.c` isolates every reachable link of the
   compare chain — length mismatch, both-empty, zero-length with
   distinct pointers, same-pointer short-circuit, partial overlap,
   single-byte match/mismatch — and the guard's FALSE outcomes (both
   conditions) are covered on every memcmp-reaching call.

### Cross-references

- VERIFY-016 — borrow.h's WP residual analysis; the `dead_by_invariant`
  proof; note the two memcmp danglingness residuals at the same
  function are a distinct concern (libc feature-gap goals, not this
  guard).
- MCDC-001 — `-DCANON_NO_REQUIRE` coverage methodology (also the
  mechanism that makes the three `_get(NULL)` defensive branches
  testable in the coverage build).
- MCDC-002 through MCDC-006 — the same API-unreachable-defensive
  disposition across slice.h/arena.h/pool.h/region.h/option; different
  source-level shapes, same cross-stream pattern.
- Per-line gcov dump: CI artifact via the "Debug: per-line MC/DC
  detail for borrow.h" step in
  `.github/workflows/cmake-multi-platform.yml`.

---

## MCDC-009: Invariant-Dead Overflow Clamp and Libc-Environmental Encoding-Error Skip — Two Dispositions in One Header, Second Named-Assert Closure (diag.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-009 |
| **Date**       | 2026-07-08 |
| **Baseline commit** | 93fa22c (Canon-C CI #1120, ceiling measurement); cross-stream closure at d8566d5 (CI #1132), re-confirmed on every WP run since; proof-stream detector enforced as of 1965b23 (CI #1135) |
| **Scope**      | semantics/diag.h — 2 of 86 condition outcomes (2 unreachable, distinct dispositions) |
| **Category**   | Coverage measurement methodology |

**Description**: 2 of 86 condition outcomes in `semantics/diag.h` are
not exercisable by tests. Unlike every prior entry in this family,
the two outcomes have **different dispositions** — one is
invariant-dead (the MCDC-002 family, WP-closable), the other is
libc-environmental (a new disposition, permanently open by design):

| # | Function      | Line (at d8566d5) | Subcondition not covered | Disposition |
|---|---------------|-------------------|--------------------------|-------------|
| 1 | `diag_push`   | 427 (body 429)    | cond 0 true (`d->depth >= DIAG_MAX_FRAMES`) | Invariant-dead; WP-closed |
| 2 | `diag_render` | 968               | cond 0 true (`n < 0`)    | Libc-environmental; permanent |

Both guard bodies show `#####` / `%%%%%` — never executed across the
coverage run. (Line numbers cite d8566d5; earlier CI comments cite the
pre-annotation lines 293 and 668 — the ACSL pass shifted the header.
The coverage debug step identifies both outcomes by function and
condition, so the drift is cosmetic — the trusted-axiom pass at the
VERIFY-017 pinning commit shifted the file again, without changing
either outcome.)

**Denominator history.** diag.h's measured ceiling is 84/86 = 97.67%,
reached at CI #1120. The denominator grew 46 → 86 in the same pass:
`diag_render` and `diag_render_frame` were entirely uncalled before
it, and uncalled `static inline` functions are invisible to gcov, so
the render pair had been silently excluded from the old 73.91% (34/46)
baseline rather than counted as missed. The gap-closure pass also
surfaced two pre-verification defects on the newly exercised path — a
contract-violating NULL guard in diag_render that permitted a
`snprintf(NULL, >0, …)` UB call, and a `-Werror=format-truncation`
break on the documented truncation path — both fixed before the
ceiling was pinned (CI #1120). The ceiling is unchanged at d8566d5
(84/86, same two outcomes).

### Outcome 1 — the overflow clamp (invariant-dead, WP-closed)

The clamp in diag_push:

```c
if (d->depth >= DIAG_MAX_FRAMES) {
    /*@ assert dead_by_invariant_clamp: \false; */
    d->depth = DIAG_MAX_FRAMES - 1u;
}
```

exists to make the `depth < DIAG_MAX_FRAMES` bound visible to the
optimizer under `-DNDEBUG` (GCC 16 at `-O3` otherwise fires a spurious
`-Wstringop-overflow` on the message write). Its true side is
unreachable on every correct execution: under `diag_invariant`
(`depth <= DIAG_MAX_FRAMES`, a precondition of the valid_diag
behavior), either `depth == DIAG_MAX_FRAMES` and the overflow branch
immediately above has already decremented it to `DIAG_MAX_FRAMES - 1`,
or `depth < DIAG_MAX_FRAMES` unchanged — both contradict the clamp
condition.

This is the **eighth cross-stream instance** and the **second carried
by a named in-body assertion** (after MCDC-008's
`dead_by_invariant`): WP **proved** `dead_by_invariant_clamp` at CI
#1132 (a `\false` assertion is provable exactly when the path
condition is contradictory, i.e. the path is infeasible). The proof is
notable for *what it traverses*: on the `depth == DIAG_MAX_FRAMES`
path, the contradiction depends on the decrement surviving the
byte-level memmove between the branch and the clamp — WP frames
`d->depth` correctly past the `(char *)` write (the memmove's assigns
footprint covers `frames` bytes only), even while the frame-content
goals at the same call site remain residuals (VERIFY-017 category 1).
The clamp is deliberately preserved: it is toolchain-defensive code
whose absence breaks the build on a supported compiler.

### Outcome 2 — the encoding-error skip (libc-environmental, permanent)

The skip in diag_render's loop:

```c
if (n < 0) { continue; } /* encoding error — skip frame */
```

is unreachable **not** because any diag invariant forbids it, but
because these format strings (`%zu`, `%s`, `%d` over
invariant-satisfying fields) with valid arguments cannot provoke a
`snprintf` encoding error on any hosted libc. This is a **new
disposition flavor** for the traceability record — do not file it
under the invariant-dead family:

- No WP goal can retire it: the unreachability depends on the stdio
  implementation's behavior, not on diag's contracts, so it is a
  **permanent documented residual**, never a deviation that later
  closes.
- It is the measurement-stream citation of the **same environmental
  assumption** VERIFY-017's trusted snprintf axiom makes in the proof
  stream (the unconditional termination ensures on the
  encoding-error-free path). One assumption, two records, each
  pointing at the other.
- The skip is deliberately preserved: it is the documented
  defensive handling of a return value ISO C permits, and removing it
  would convert a hypothetical negative return into unsigned
  wraparound of `total`.

The sibling encoding-error handling in `diag_render_frame`
(`return (n < 0) ? 0u : (usize)n;`) is confirmed **absent from the
condition denominator**: gcov emits no condition row for it (the
ternary is gimplified as value selection, not a branch), matching the
round-1 prediction. If a future toolchain surfaces it as a condition,
it joins this outcome's disposition, not a test gap.

### Mitigation

1. **Outcome 1, reachability argument via the invariant**:
   `diag_invariant` plus the overflow branch make the clamp's true
   side contradictory; WP discharges the named
   `dead_by_invariant_clamp` assertion, formally establishing the path
   infeasible (VERIFY-017).

2. **Outcome 2, environmental argument**: hosted-libc encoding-error
   freedom for these formats, recorded as a trusted assumption shared
   with VERIFY-017's snprintf axiom; exercised as the FALSE outcome on
   every render call across the matrix, including the MinGW-UCRT job
   (the CRT most likely to differ).

3. **Two regression detectors, one per evidence stream** (outcome 1):
   the coverage job's "Debug: per-line MC/DC detail for diag.h" step
   prints the gcov dump and a `not covered` quick-filter every run
   (measurement stream); the `frama-c-diag` wrapper fails if
   `dead_by_invariant_clamp` ever leaves the proved set (proof
   stream). Outcome 2 has one detector by nature — the coverage dump —
   plus the axiom's presence in VERIFY-017's trusted base.

4. **The achievable MC/DC ceiling is 84/86 = 97.67%**, reached at CI
   #1120 and representing 100% of API-reachable coverage under a
   hosted libc. The 2 missing outcomes are documented here and not
   counted as a coverage regression.

5. **The surrounding paths are exhaustively tested**:
   `test/semantics/diag_test.c` exercises the overflow shift (both
   sides of the `depth == DIAG_MAX_FRAMES` branch, 8 overflow pushes
   observed in the run), the render truncation path (both sides of
   `total < buf_size`, including the measure-only `rem = 0` re-entry),
   NULL/empty guards on all three rendering functions (6/6 and 8/8
   compound outcomes), and the message-copy loop at all four
   outcomes.

### Cross-references

- VERIFY-017 — diag.h's WP residual analysis; the
  `dead_by_invariant_clamp` proof; the trusted stdio axioms sharing
  outcome 2's environmental assumption.
- MCDC-008 — the first named-assert closure (borrow.h); this entry's
  outcome 1 is the same mechanism, second instance.
- MCDC-002 through MCDC-006 — the invariant-dead/API-unreachable
  disposition family that outcome 1 joins and outcome 2 deliberately
  does not.
- MCDC-001 — `-DCANON_NO_REQUIRE` coverage methodology (why
  diag_push's `require_msg` contributes no outcomes here).
- Per-line gcov dump: CI artifact via the "Debug: per-line MC/DC
  detail for diag.h" step in
  `.github/workflows/cmake-multi-platform.yml`.

---

---

## MCDC-010: Two Guard-Redundancy-Infeasible Overflow Checks (WP-Corroborated) and One Heap-Environmental Allocation Failure — Third Attribution Variant (vec)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-010 |
| **Date**       | 2026-07-12 |
| **Baseline commit** | 5bfde5b (Canon-C CI #1146, cover TU landed and ceiling measured); stable through the VERIFY-018 enforcement run (e663e2c, CI #1154); WP corroboration at 96dd41d (CI #1152) |
| **Scope**      | data/vec/ via `vmacros/coverage/vec_cover.c` — 3 of 158 condition outcomes (2 infeasible + 1 environmental; all three written down before the first run) |
| **Category**   | Coverage measurement methodology |

**Description**: 3 of 158 condition outcomes measured on the vec cover
TU are not exercisable by tests, giving a ceiling of **155/158 =
98.10%**. All three were predicted, with dispositions, in the cover
TU's header comment **before the first measurement run**, and the
first run confirmed exactly those three at block level and nothing
else:

| # | Function             | Subcondition not covered | Disposition |
|---|----------------------|--------------------------|-------------|
| U1 | `vec_int_alloc`       | `!checked_mul(...)` true | Infeasible (guard redundancy); WP-corroborated |
| U2 | `vec_int_arena_alloc` | `!checked_mul(...)` true | Infeasible (guard redundancy); WP-corroborated |
| U3 | `vec_int_alloc`       | `!buf` true (heap OOM)   | Environmental (heap); permanent unless a failure-injection hook lands |

vec is the **third Shape-B cover TU** (after option, MCDC-006, and
result, MCDC-007) and lands as a **ceiling entry with a new
disposition mix**: no panic branch survives into the measured set
(vec's `require_msg`/`ensure_msg` surface vanishes under
`-DCANON_NO_REQUIRE`, like result's), yet the audit is not clean —
the three open outcomes are infeasibility and environment, not
contract-violation unreachability. Per MCDC-009's forward
classification, U1/U2 are the proof-closable shape and U3 the
environmental shape; no test-closable gap exists.

### U1/U2 — the redundant-guard infeasibility (WP-corroborated)

In both constructors the `checked_mul(capacity, sizeof(type), &bytes)`
call is preceded by the guard
`capacity <= CANON_VEC_MAX_CAPACITY / sizeof(type)`, which already
bounds `capacity * sizeof(type) <= CANON_VEC_MAX_CAPACITY` — the
multiplication cannot overflow, so `!checked_mul`'s true outcome is
infeasible on every path. The corroboration is cross-stream and
unusually direct: in the VERIFY-018 baseline run, `vec_int_alloc` and
`vec_int_arena_alloc` carry **zero unproved branch goals** (their only
residuals are the category-(e) allocation-model pair), meaning WP
discharged the branch-dead facts formally — the same branches gcov
records as uncovered, the proof stream records as dead. These are
justification rows with a proof behind them, the strongest form a
justified-infeasible row can take. **Upstream option recorded**: if a
future change drops one of the two redundant guards, U1/U2 convert
from permanent justification rows to ordinary covered outcomes; until
then they stand.

### U3 — the heap-environmental allocation failure

`vec_int_alloc`'s `!buf` true outcome requires `malloc` to fail, which
is not deterministically forcible from a portable cover TU. The
disposition is environmental (MCDC-009 outcome-2's family, heap-OOM
flavored). The evidence that this is genuinely about heap
non-determinism rather than condition shape is **in the same
measurement**: the arena analogue is covered — the cover TU exhausts a
64-byte arena with a 4096-element request, forcing
`vec_int_arena_alloc`'s `!buf` TRUE deterministically (gcov shows that
condition 2/2). U3 remains open unless memory.h grows a
failure-injection hook; it is a permanent documented residual, not a
deviation that later closes.

### Third attribution variant (Shape-B mechanics)

The Shape-B attribution check ran in the same coverage job:
`vec_test.c` owns **all 640** of the module's test-measured condition
outcomes (56.25% covered there — a non-gating number mixing generated
and harness conditions), while `data/vec/vec_impl.h` shows the pure
Shape-B fingerprint — 1 line, 4 functions, **zero conditions**. The
conditions are genuinely lost to the `/test/` exclusion and the cover
TU is the correct fix — Shape B **confirmed** for vec (third
independent confirmation).

The surviving attribution differs from both predecessors, completing
the variant set: option's conditions attribute to `option_cover.c`
(the TU instantiates `CANON_OPTION` directly), result's to the
**driver header** `result_verify.h` (its TU `#include`s the driver and
takes the instantiation from it), and vec's to **`vec_cover.c`
itself** — the TU instantiates `DEFINE_VEC(static inline, int)`
directly rather than including `vec_verify.h`, so the expansion site
is the cover TU. The 158 outcomes split **152 generated (140
`DEFINE_VEC` conditions + 12 `DEFINE_VEC_SLICE` facade-view conditions
— `as_slice`/`as_slice_full`/`as_bytes`, measured here rather than
lost to the test filter) + 6 cover-driver scaffolding conditions**
(three fill-to-capacity/iterate `while` loops). The attribution
fingerprint (`vec_impl.h`: functions but no conditions) is the
standing tripwire for Shape-A drift: if vec_impl.h is ever
restructured so conditions land on it directly, the fingerprint
breaks and the cover TU no longer measures what it claims — the
CI attribution step exists to make that visible.

### Known exclusion (F2)

`slice_init` on an `items==NULL` vec with `[0,0)` is pedantic UB
(`NULL + 0`; VERIFY-018 finding F2) and is deliberately **not
exercised** until the upstream one-token guard fix lands; the `!v` and
range-check legs of slice_init's condition are covered through other
inputs, so no outcome is currently missing on its account. When the
F2 PR lands (guard + the excluded `slice_init(&e, 0, 0)` call + the
driver's exclusion-requires removal, together), expect the 158
denominator to move — that shift is the planned closure of this
exclusion, not a regression.

### Mitigation

1. **U1/U2, infeasibility argument**: the preceding capacity guard
   bounds the multiplication; WP corroborates by proving
   `vec_int_alloc`/`vec_int_arena_alloc` branch-complete (VERIFY-018).
2. **U3, environmental argument**: heap OOM non-forcible; the arena
   sibling's covered `!buf` TRUE leg demonstrates the exclusion is
   heap-specific, not structural.
3. **Two regression detectors**: the coverage job's "Debug: per-line
   MC/DC detail for vec (cover TU)" step prints the gcov dump with an
   uncovered-outcome quick-filter every run, and the "Debug: Shape-B
   attribution check for vec (vec_test)" step re-prints the
   attribution fingerprint (the Shape-A-drift tripwire). The WP side
   of U1/U2 is watched by the VERIFY-018 enforcement gates.
4. **The achievable ceiling is 155/158 = 98.10%**, reached at CI
   #1146 and byte-stable through CI #1154; the 3 missing outcomes are
   documented here and not counted as a coverage regression.
5. **The surrounding paths are exhaustively driven**: every other
   condition in the measured set is exercised to both outcomes,
   including all three `fill` legs (the MC/DC-primary evidence for
   VERIFY-018 category (g), whose WP goals are unprovable by
   construction), both `free` legs, the full NULL/empty/full guard
   matrix, the iterator exhaustion path, and the facade views'
   NULL/empty/live triples.

### Cross-references

- VERIFY-018 — vec's WP residual analysis; the alloc/arena_alloc
  branch-complete proofs corroborating U1/U2; finding F2; category
  (g), for which this entry's fill legs are the primary evidence.
- MCDC-006/-007 — the prior Shape-B entries; this entry completes the
  attribution-variant set (cover-TU / driver-header / cover-TU-direct)
  and follows MCDC-007's forward note (panic-surface routing:
  require_msg-only, hence no panic branch — but a ceiling nonetheless,
  via dispositions MCDC-007's clean audit did not encounter).
- MCDC-009 — the three-shape classification (test-closable /
  proof-closable / environmental) this entry applies; U3 joins the
  environmental family.
- MCDC-001 — `-DCANON_NO_REQUIRE` coverage methodology (why vec's
  require/ensure surface contributes no outcomes).
- Per-line gcov dump and attribution check: CI steps in
  `.github/workflows/cmake-multi-platform.yml` (coverage job).

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
