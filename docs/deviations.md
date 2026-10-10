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


**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 742/757 (was 746/761; -4 goals). Cause: the Commit-9 rule-17.8 reshape replaced the two `shift &= 63;` parameter compound-assignments in bits_rotl/bits_rotr with hoisted `const u64 sh` locals, removing two obligations per rotation function (the compound-assignment RTE/typing pair). The
unproved set is UNCHANGED — same count, same goal names (CI #1187
transcript is the name-stability record); no residual entered or left
the categories above, so the classification tables in this record
remain valid as written. Ratcheted with the acknowledged commit the
enforcement gate prescribes.

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

**Update (2026-10-09, VERIFY-039 G3) — the proofs use a broader hypothesis
than the one stated.** The WP manual lists the selector `Typed+Cast` adds,
`cast`, as "Typed memory model with unlimited casts (unsound)" (29.0, §2.4.3).
The typed model keeps one array per atomic type, integers by exact kind, so a
write through `char *` changes only the `sint8` array. A proof that relates a
location of another type across a call whose `assigns` are stated through
`char *` (memory.h's byte-level functions; libc's `memset`, `memcpy` and
`memmove`) rests on the hypothesis that a location is accessed at one type,
which those calls break by design whenever they write a typed buffer — not
only on callers passing correctly-typed pointers. In the model, content
stated through `char *` does not reach a read at another type (shown for
`uint8` at VERIFY-039 G3), and, as WP 29 frames such calls, a frame across one
at another type proves without its range being checked (VERIFY-039 G4, to be
tested by probe).

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
list. The CI wrapper emitted a `WARNING: proved count changed from
expected 1729 / 1739` when the rebaseline first appeared and PASSed
because all 10 expected named ptr.h goals were still present. **Update
(2026-07-16)**: the gate retrofit replaced the warning-only pin with a
hard gate on the full-run figure (1943/1953); the own-baseline
1729/1739 remains the substantive ptr.h number in this record and in
the per-header table.

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

**Reading note (2026-09-07, VERIFY-023).** The full-run figure this record
quotes as `1943/1953` is now **1949/1959**: VERIFY-023 gave `ptr_offset`,
`ptr_offset_const`, `ptr_elem` and `ptr_elem_const` an `ensures` stating the
address they return, +6 goals in every TU that includes ptr.h, all proved. The
10 residuals are unchanged by name. The ptr.h-own figure of 1729/1739 was not
re-measured; the change is +6 goals, all proved, so its residual set is
unaffected.

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
is a regression and fails the build. slice.h achieves 92.6% MC/DC
coverage (50/54 condition outcomes; surface per the 2026-07-24
MCDC-002 note) — the achievable ceiling under
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


**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 379/394 (was 375/390; +4 goals, all proved). Cause: the Commit-9 clamp folds in bytes_slice/str_slice (`const usize e = (end > len) ? len : end;`) introduce a guarded-initializer obligation pair per function that Qed/Alt-Ergo discharge. The
unproved set is UNCHANGED — same count, same goal names (CI #1187
transcript is the name-stability record); no residual entered or left
the categories above, so the classification tables in this record
remain valid as written. Ratcheted with the acknowledged commit the
enforcement gate prescribes.

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

#### Category 2: WP integer theory / bitwise alignment (8 — was 9)

| # | Goal                                                          |
|---|----------------------------------------------------------------|
| 1 | `typed_cast_mem_align_normal_ensures_part3`                    |
| 2 | `typed_cast_mem_align_normal_ensures_2_part3`                  |
| 3 | `typed_cast_mem_align_to_normal_ensures_part3`                 |
| 4 | `typed_cast_mem_align_to_normal_ensures_2_part3`               |
| 5 | `typed_cast_mem_is_aligned_nonnull_aligned_ensures`            |
| 6 | `typed_cast_mem_is_aligned_nonnull_unaligned_ensures`          |
| 7 | `typed_cast_mem_get_alignment_nonnull_ensures_part2`           |
| 8 | `typed_cast_mem_get_alignment_nonnull_ensures_2_part2`         |

**Coverage update (2026-10-05, VERIFY-036 F1) — one goal was false; closed.**
The former goal 7, `typed_cast_mem_get_alignment_assert_rte_signed_overflow`,
was not a cast-round-trip limitation: `-(intptr_t)addr` overflowed when `addr`
had only its top bit set, which on 32-bit targets is `0x80000000`, an ordinary
user-space address (probe `tools/probes/verify-036/p_getalign.c`). F1
(f1ba2de) negates in unsigned arithmetic; the goal is no longer generated (CI
#1326) and its row is removed from the table above. The root-cause paragraph
below still describes it as a cast-round-trip limitation, which was wrong; the
argument never addressed it.

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
| 2b       | 8     | mem_align*, mem_is_aligned, mem_get_alignment | bitwise-alignment integer theory |
| 2c       | 6     | mem_compare/equal/equal_bytes | `\dangling` (initialization closed, VERIFY-012) |
| **Total**| **19**|                              |                                |

All 19 remaining residuals would be discharged by improvements to Frama-C's
`\dangling`/`\fresh`/`\freeable` theory or its integer theory. (A twentieth,
closed by VERIFY-036 F1, was a real signed overflow, not a tool limit.)
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


**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 2823/2866 (was 2819/2862; +4 goals, all proved). Cause: inherited verbatim from slice.h's bytes_slice/str_slice clamp folds (VERIFY-007 note of the same date); memory.h has no own-goal change. The
unproved set is UNCHANGED — same count, same goal names (CI #1187
transcript is the name-stability record); no residual entered or left
the categories above, so the classification tables in this record
remain valid as written. Ratcheted with the acknowledged commit the
enforcement gate prescribes.

**Reading note (2026-09-07, VERIFY-023).** The pin is now **2829 / 2872**;
the 43 residuals are unchanged by name. The +6 goals are ptr.h's new
`ensures` fragments, all proved here as in every other TU.

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

### arena.h-own residuals (8 — was 46)

Four categories. Cats 2a and 2d are call-chain residuals at ptr.h
boundary functions whose `nonnull` behavior carries no `ensures` clause
(see VERIFY-006 forward-implication note). Cat 2b is the
arithmetic-chain residual at arena_alloc / arena_alloc_aligned. Cat 2c
inherits from cat 2b through wrapper delegation.

#### Category 2a: ptr_span call-site preconditions in arena_alloc / arena_alloc_aligned (0 — RETIRED at VERIFY-036 F3; was 4, originally 8)

| # | Goal                                                      |
|---|------------------------------------------------------------|
| 1  | `typed_cast_arena_alloc_call_ptr_span_requires`            |
| 2  | `typed_cast_arena_alloc_call_ptr_span_requires_2`          |
| 3  | `typed_cast_arena_alloc_aligned_call_ptr_span_requires`    |
| 4  | `typed_cast_arena_alloc_aligned_call_ptr_span_requires_2`  |
**Coverage update (2026-09-21, VERIFY-026 row 1 / VERIFY-027).** The four
`_requires_3` / `_requires_4` goals (same `\base_addr`, `to >= from`) closed
at CI #1299 when `ptr_align_up` stated its result. The four that remain are the
two `\valid_read` requires per variant, which are genuinely false when
`arena->offset == capacity` (the aligned pointer is one-past-the-end and
`ptr_span` only subtracts): a `ptr_span` contract question, tracked in
VERIFY-006. This block covers 4 goals; the argument below is unchanged.

**Coverage update (2026-10-05, VERIFY-036 F3) — argument defective.** The four
goals are false on a wider region than the note above says. `ptr_span` runs
before the capacity guard, so whenever `offset + pad > capacity` (a failing
allocation on a nearly full arena) the aligned pointer lies beyond
one-past-the-end of the buffer. There the subtraction inside `ptr_span` is
itself outside C99 6.5.6p9, so weakening `ptr_span`'s requires would not cover
it. The argument below says both pointers lie within the buffer; it is wrong
for exactly the goals it still covers. Probe:
`tools/probes/verify-036/p_arena_span.c`.

**Coverage update (2026-10-07, VERIFY-036 F3) — this argument is RETIRED.** F3
(baf8ee9) computes the pad on the address with `ptr_align_padding` and no
longer calls `ptr_align_up` or `ptr_span`; the four goals vanished at CI #1334
in every unit that pinned them. They were removed, not proved: the call that
generated them is gone. Rows kept for legibility.

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

#### Category 2b: arena_alloc / arena_alloc_aligned fits / does_not_fit ensures (0 — RETIRED at VERIFY-036 F4; was 16, originally 26)

| #  | Goal                                                                     |
|----|--------------------------------------------------------------------------|
| 1  | `typed_cast_arena_alloc_fits_ensures_part4`                              |
| 2  | `typed_cast_arena_alloc_fits_ensures_2_part4`                            |
| 3  | `typed_cast_arena_alloc_fits_ensures_3_part4`                            |
| 4  | `typed_cast_arena_alloc_does_not_fit_ensures_part5`                      |
| 5  | `typed_cast_arena_alloc_does_not_fit_ensures_2_part5`                    |
| 6  | `typed_cast_arena_alloc_aligned_fits_ensures_part3`                      |
| 7  | `typed_cast_arena_alloc_aligned_fits_ensures_part4`                      |
| 8  | `typed_cast_arena_alloc_aligned_fits_ensures_2_part3`                    |
| 9  | `typed_cast_arena_alloc_aligned_fits_ensures_2_part4`                    |
| 10 | `typed_cast_arena_alloc_aligned_fits_ensures_3_part3`                    |
| 11 | `typed_cast_arena_alloc_aligned_fits_ensures_3_part4`                    |
| 12 | `typed_cast_arena_alloc_aligned_does_not_fit_ensures_part5`              |
| 13 | `typed_cast_arena_alloc_aligned_does_not_fit_ensures_2_part5`            |
| 14 | `typed_cast_arena_alloc_fits_ensures_address_part4`                      |
| 15 | `typed_cast_arena_alloc_aligned_fits_ensures_address_part3`              |
| 16 | `typed_cast_arena_alloc_aligned_fits_ensures_address_part4`              |

**Coverage update (2026-09-21).** Twenty-six became sixteen in three steps.
VERIFY-023 (#1285) closed the four `fits_ensures*_part5` when `ptr_offset`
stated its result — those four were never this category, and VERIFY-023
records them as misattributed. VERIFY-026 row 1 (#1299) closed nine more —
every `part2`, and `arena_alloc`'s three `part3` — when `ptr_align_up` stated
its result; VERIFY-027 records those nine as misattributed to this chain as
well. VERIFY-026 row 2 added the three `address` parts, which fail on exactly
this chain. What survives is sixteen goals: the `part4` of every `fits` ensures on both
variants (6), the aligned variant's `part3` (3, user-supplied `alignment`),
the `address` parts (3), and the four `does_not_fit` `part5` — the goals whose
difficulty is the readable `arena_can_fit` form and nothing else. **Class:
(c)** — the paragraph above is the written cost comparison §4.2 of the paper
requires for that class; the solver-theory limit it also names (VERIFY-006
cat 2) is the mechanism, the readable-predicate choice is the reason the
mechanism is reached.

**Coverage update (2026-10-05, VERIFY-036 F4) — argument defective; the 16
goals are false.** The argument's central step, that the C pad equals the ACSL
pad, holds only for buffers aligned to the requested alignment.
`arena_can_fit` pads from the offset, the code pads from the address, and
neither `arena_init` nor `arena_invariant` requires alignment. For any other
buffer both `fits` and `does_not_fit` are violated, including
`arena_alloc_aligned(.., 64)` on a malloc-style 16-aligned buffer (probe
`tools/probes/verify-036/p_arena_pad.c`). The readability trade described
below is real, but it is not why these goals fail.

**Coverage update (2026-10-07, VERIFY-036 F4 scored) — this argument is
RETIRED.** F4 (e953219) made `arena_can_fit` pad from the address, as the code
does, and gave `ptr_align_padding` its exact result. All 16 goals closed at CI
#1336, in every unit that pinned them. They were false before F4; they became
true and then proved. The block covers nothing. Rows kept for legibility.

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

#### Category 2c: zero / try wrappers (2 — was 8)

| #  | Goal                                                            |
|----|------------------------------------------------------------------|
| 1  | `typed_cast_arena_alloc_zero_ensures_3_part1`                   |
| 2  | `typed_cast_arena_alloc_aligned_zero_ensures_3_part1`           |

**Coverage update (2026-10-05, VERIFY-036 F4).** This argument assumes the
parent's postconditions per Cat 2b's argument, which VERIFY-036 found
defective. Its status follows F4.

**Coverage update (2026-10-05, VERIFY-036 F5) — two goals were false;
closed.** The former goals 2 and 4, `arena_alloc_zero_assigns_normal_part3`
and `arena_alloc_aligned_zero_assigns_normal_part3`, failed because the
assigns clause named only `*arena` while the functions zero bytes of the
buffer, not by inheritance from Cat 2b. F5 (c5281dc) added the free tail to
both clauses and both goals closed at CI #1330, in every unit that pinned
them; their rows are removed from the table above. Probe:
`tools/probes/verify-036/p_zero_assigns.c`.

**Coverage update (2026-10-07, VERIFY-036 F4) — attribution refuted.** This
block filed its eight remaining goals as inheritance from Cat 2b. F4 made
every Cat 2b goal prove and all eight stayed, unchanged, at CI #1336: whatever
blocks them, it is not the fits chain. They are misattributed obligations; the
argument needs rewriting before it can cover them.

**Coverage update (2026-10-08, VERIFY-039 G2) — the eight read against the
code.** Goals 3 and 6 are false: the default `assigns *arena` omits `*out`,
which both try wrappers write. Goals 4, 5, 7 and 8 are true but unprovable as
stated: `arena_invariant` after a store through an `out` that nothing
separates from `*arena`. Goals 1 and 2 fail because `mem_zero`'s contract does
not say that it writes zeros. None is inheritance from Cat 2b. See VERIFY-039
G2.

**Coverage update (2026-10-08, VERIFY-039 G2) — six goals closed.** The former
goals 3 to 8, the try wrappers', closed at CI #1343 and CC #30 in all eight
units that pinned them, after G2 (f4a846a) added `*out` to both default
`assigns` clauses and separated `out` from `*arena`; their rows are removed
from the table above. The block now covers two goals, true but unprovable
because `mem_zero`'s contract states no values. The argument below does not
cover them as written: it relies on a `mem_zero` postcondition that does not
exist. Probe: `tools/probes/verify-039/p_try_alloc_assigns.c`.

**Coverage update (2026-10-09, VERIFY-039 G3) — the premise exists; the
bridge remains.** G3 (76a1f94) gave `mem_zero` the postcondition the argument
below cites — the bytes at `ptr` are zero — stated through `char *`, as libc's
`memset` states its own. It proved in every unit, and both goals stayed at CI
#1344 and CC #31: WP's typed model keeps `char` (`sint8`) and `u8` (`uint8`)
in separate arrays, so a fact about the bytes through `char *` does not reach
a read through `u8 *`, and, as WP 29 frames the call, the `uint8` array keeps
its old values. Both goals are true in C. Their mechanism is the byte-type bridge,
as G3 pre-registered. The argument now has its premise; it covers both goals
once it states the one step the model cannot take — a byte written as zero
through `char *` reads 0 through `u8 *` — and drops the inheritance from Cat
2b it claims. Whether it is rewritten that way or the two goals move to an
existing byte-view block is the filing pass's decision; until then this block
lists them, and its argument as written does not cover them. See VERIFY-039
G3.

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

#### Category 2d: arena_free_bytes ptr_offset / bytes_from call-site preconditions (0 — RETIRED at VERIFY-023; was 2)

| # | Goal                                                       |
|---|-------------------------------------------------------------|
| 1 | `typed_cast_arena_free_bytes_call_bytes_from_requires`      |
| 2 | `typed_cast_arena_free_bytes_call_bytes_from_requires_2`    |

**Coverage update (2026-09-21) — this argument is RETIRED.** Both goals
closed at CI #1285 when `ptr_offset` stated its result (VERIFY-023, which
lists `arena_free_{,c}bytes_call_bytes_from_requires` among its 24). The
category has covered no obligation since, which nobody noticed for fifteen
days: the argument below reads exactly as it did while it was live, and no
gate checks coverage. In the paper's terms (§3) a block covering nothing is
retired whether or not anyone has said so; this is the campaign's first
retirement, produced by a specification improvement, and it means the number
of arguments *in force* has been 16 — not 17 — since 2026-09-06. The two rows
are kept so the retirement is legible.

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
| 2a       | 0     | (retired at VERIFY-036 F3)                                    | ptr.h empty nonnull behavior (VERIFY-006)   |
| 2b       | 0     | (retired at VERIFY-036 F4)                                    | arithmetic chain through ptr_align_up       |
| 2c       | 8     | arena_alloc_zero, arena_alloc_aligned_zero, arena_try_alloc{,_aligned} | Wrapper delegation through cat 2b          |
| 2d       | 0     | (retired at VERIFY-023)                                       | ptr.h empty nonnull behavior (VERIFY-006)   |
| **Total**| **8** |                                                               | counts as of VERIFY-036 F4 (was 8/26/10/2, total 46) |

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

arena.h achieves 100% line coverage and 89.4% MC/DC coverage
(59/66 on the post-API-001 surface; see the 2026-07-30 note below —
90.6% (58/64) before it) — the latter is the achievable ceiling under MCDC-003's
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


**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 3387/3476 (was 3383/3472; +4 goals, all proved). Cause: inherited verbatim from slice.h via the substrate chain (VERIFY-007 note of the same date); arena.h has no own-goal change. The
unproved set is UNCHANGED — same count, same goal names (CI #1187
transcript is the name-stability record); no residual entered or left
the categories above, so the classification tables in this record
remain valid as written. Ratcheted with the acknowledged commit the
enforcement gate prescribes.

**Goal-surface reclassification (2026-07-30, CI #1202, Commit 16/16b):**
pinned proved-goal summary ratcheted to 3430/3521 (was 3387/3476; +45 goals, +43 proved), EXPECTED_UNPROVED to 91 (was 89).
Cause: the three `arena_*_cbytes` accessors added by API-001, each carrying the same ACSL contract as its mutable twin.

**Reading note (2026-08-09).** The body of this record above was written against the pre-API-001 figures and still says **46 arena.h-own** residuals and a 89-goal total. Those numbers are historically correct for the baseline they describe and are left as written; the enforced state since CI #1202 is **48 arena.h-own** and 91 (43 inherited + 48 own), the two added goals being the `arena_free_cbytes` const twins. Where the two disagree, the CI pin governs.

**The unproved set GREW — this is not the usual scalar-only drift.** Two goals entered: `typed_cast_arena_free_cbytes_call_cbytes_from_requires` and `..._requires_2`.
Each new residual is the const twin of a residual this record ALREADY
documents: `arena_free_cbytes_call_cbytes_from_requires` times out on exactly the obligation its mutable
counterpart `arena_free_bytes_call_bytes_from_requires` times out on, for the same reason — WP cannot discharge
the `bytes_from`/`cbytes_from` validity precondition when the pointer
argument is computed rather than a plain member read. No new CATEGORY of
residual appeared; the categories in the tables above absorb them
unchanged, and every pre-existing residual is still present by name
(roll-calls extended, not replaced). Zero Failed goals.

### Width-axis evidence (2026-08-02, CI #1209/#1210) — VERIFY-009-W

Every WP job in this project runs at Frama-C's default machine model, which
is 64-bit. Tier 1, however, promises `size_t >= 32 bits`, and this record's
own cond-0 unreachability argument reasons explicitly about both widths
(`SIZE_MAX = 2^32 - 1` on 32-bit, `2^64 - 1` on 64-bit). The proof stream
therefore evidenced ONE of the two widths the library claims to support —
the same gap the `build-32bit` job was added to close for the TEST stream,
which the proofs had not caught up with.

**Measured result: the arena proof surface is IDENTICAL at 32-bit and
64-bit.**

| run | machdep | proved | Timeout | Unknown | T+U | Alt-Ergo | CVC5 | Z3 |
|---|---|---|---|---|---|---|---|---|
| #1202 | default (64-bit) | 3430 / 3521 | 88 | 3 | 91 | 325 | 11 | 29 |
| #1209 | `gcc_x86_32` | 3430 / 3521 | 88 | 3 | 91 | 326 | 9 | 30 |
| #1210 | `gcc_x86_32` | 3430 / 3521 | 85 | 6 | 91 | 347 | 6 | 12 |

Same proved count, same residual count, and — checked by name, not by
count — **the same 91 residuals, with zero delta in either direction**. No
goal proves at one width and resists at the other.

**Why the null result is evidence rather than an absent measurement.** A
result identical to the baseline is exactly what a silently-ignored
`-machdep` flag would produce, so the run establishes a positive control
before reporting: under `gcc_x86_32` it requires `sizeof(size_t) == 4`,
`sizeof(void*) == 4` AND `SIZE_MAX == 0xFFFFFFFF`, and it requires the same
probe to be REJECTED at `gcc_x86_64`. Both halves matter — `-machdep` sets
Frama-C's internal type model, but this header's width-dependent behaviour
flows through `CANON_USIZE_MAX = SIZE_MAX`, fixed at preprocessing time by
whichever `stdint.h` is resolved; had the host's headers been used instead
of Frama-C's, `sizeof(size_t)` could have been 4 while `SIZE_MAX` stayed
`2^64 - 1`, an incoherent model whose signature is precisely an unchanged
result. The control passed both directions at #1210.

**Why the two 32-bit runs are the strongest part of the evidence.** They
differ from EACH OTHER more than either differs from 64-bit: the
Timeout/Unknown split moved 88/3 to 85/6 and Z3's share went from 30 goals
to 12. The 91-name set did not move. That rules out a deterministic-replay
artifact — the solvers demonstrably behaved differently and the outcome set
held regardless. It is also independent justification for pooling Timeout
with Unknown, the house rule adopted in the 2026-07-16 retrofit: the split
is run-to-run noise, the union is the signal.

**Cond 0 specifically.** The unreachability argument recorded above holds at
32-bit, and needed no CI job to establish — it is arithmetic on two
compile-time constants. `CANON_ARENA_MAX_SIZE + (CANON_DEFAULT_ALIGN - 1)`
is `2^30 + 15 = 1073741839`, against `CANON_USIZE_MAX = 4294967295`. The
margin is **4x**, where at 64-bit it is 1.7e10. It is a designed
relationship — the arena cap is exactly a quarter of the 32-bit address
space — but it is worth stating that the argument at 32-bit rests on a
factor of four rather than on the astronomical headroom the 64-bit reading
suggests. Cond 1 is unaffected: its unreachability is purely mathematical
and independent of any project constant, as recorded above.

**Enforcement.** Promoted from report-only to enforced at CI #1210, after
two name-stable runs — the `diag` precedent (#1132 report-only, #1133
name-stable, then pinned). The gate is deliberately a different shape from
every other WP job: rather than pinning its own numbers, `frama-c-arena-32`
pins **set equality with the 64-bit residuals**, failing if the symmetric
difference is non-empty in either direction. A goal newly proved at 32-bit
is as much a surface change as one newly resistant, and a swap that leaves
the count at 91 while changing the set would pass a count-only gate. There
is deliberately no second copy of the 91 names to drift out of step, that
being the failure mode this project has already been bitten by.

**Coupling — read before ratcheting this record's pin.** When arena's
64-bit pin moves, `frama-c-arena-32`'s embedded baseline list and its
`EXPECTED_PROVED` must move in the SAME commit. The job carries a
best-effort cross-check that parses the 64-bit job's `CHECKS` array out of
the workflow and warns on divergence, but the warning is advisory and does
not substitute for doing it.

**Scope.** This is arena only. pool, region and vec inherit arena's surface
transitively and are the natural next candidates; slice, memory and ptr sit
below it. Nothing here evidences those units at 32-bit, and nothing here
says anything about 16-bit, which the Tier 1 guard in `limits.h` refuses
outright.


**Reading note (2026-09-07, VERIFY-023).** Eight of the residuals this record
classifies were misattributed. `arena_alloc_fits_ensures_part5`,
`arena_alloc_fits_ensures_2_part5` and their `_aligned_` twins were recorded
as "fits/does_not_fit arithmetic chain"; the four
`arena_free_bytes` / `arena_free_cbytes` `call_bytes_from_requires` goals as
"free_bytes helpers". All eight closed at CI #1284 when `ptr_offset` and
`ptr_elem` gained an `ensures` stating their result (VERIFY-023). They were
never about arithmetic or about the helpers; they were the callee's opaque
return address. The pin is now **3444 / 3527, 83 residuals** (43 inherited +
40 own). The text above is left as written so the error is visible.

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

#### Category 2a: pool_invariant postcondition arithmetic at pool_init (1 — was 5)

| # | Goal                                                |
|---|------------------------------------------------------|
| 1 | `typed_cast_pool_init_ensures_part4`                |

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

**Coverage update (2026-10-09, VERIFY-039 G4, arena repair).** Goals 1 and 5,
`pool_init_ensures_part4` and `pool_init_call_arena_alloc_requires`, now also
state the conjunct `arena_invariant` gained: the `Arena` lies outside its
buffer. The conjunct depends only on the `Arena`'s address, `buffer` and
`capacity`. The argument takes `arena_invariant(arena)` at the call from
`pool_init`'s precondition, so it already presumes that nothing before the
call changes those; after the call, `arena_invariant(arena)`, conjunct
included, is `arena_alloc`'s own postcondition, which is what goal 1's
`arena_invariant(p->arena)` part rests on. The argument covers the conjunct
as written.

**Coverage update (2026-10-09, VERIFY-039 G4, pool repair).** Goal 1,
`pool_init_ensures_part4`, now also states the conjunct `pool_invariant`
gained: the `Pool` lies outside its slots,
`buffer + (base_mark .. end_mark - 1)`. By `arena_alloc`'s postconditions
(`frame_buffer`, `address`, `offset >= \old(offset) + size`,
`offset <= capacity`), the slots lie in the free tail at the call,
`buffer + (\old(offset) .. capacity - 1)`, which `pool_init`'s new
precondition separates from `*pool`. That holds only if `*pool` and
`*arena` do not overlap, so that `pool_init`'s stores into the one and
`arena_alloc`'s into the other leave each other's fields alone. The argument
presumes this without stating it, `pool_init`'s contract does not state it,
and without it the goal is not true for an admitted call; G5 records that.

**Coverage update (2026-10-10, VERIFY-039 G5).** Goals 2 to 5
(`pool_init_ensures_2_part4`, `_3_part4`, `_4_part3` and
`pool_init_call_arena_alloc_requires`) closed at CI #1349 once `pool_init`
required `\separated(pool, arena)` (fix 528bfa1); their rows are removed.
They were never arithmetic: each asked only that a field stored or kept, or
`arena_invariant`, survive the stores into the other struct or
`arena_alloc`'s `assigns *arena` (VERIFY-033 F4's hypothesis, G5). The
presumption the two G4 coverage updates above name is now `pool_init`'s
precondition. Goal 1 stayed, as predicted. Every conjunct of
`pool_invariant` but one follows from `pool_init`'s preconditions, the two
separations among them, the values it stores and its callees'
postconditions; the product equality needs only its factors swapped
(`needed` is `aligned_size * max_objects`, the conjunct's product
`capacity * object_size`). The one that needs more is
`capacity <= CANON_USIZE_MAX / object_size`: on success both factors are
nonzero and `checked_mul` returned true, so its `no_overflow` behaviour
applies and gives `aligned_size <= CANON_USIZE_MAX / max_objects`, so
`aligned_size * max_objects <= CANON_USIZE_MAX`, so
`max_objects <= CANON_USIZE_MAX / aligned_size`; `pool_init` stores
`max_objects` as `capacity` and `aligned_size` as `object_size`. The
LIMITATION-SUSPECTED review of the note above is thereby done: two of the
three `ensures_part4` goals were the separation; the third stays, and the
argument, which had spelled out only the product equality,
`arena_invariant(p->arena)` and the `Pool`'s separation from its slots, now
covers every conjunct, this bound included. That the bound is what WP fails
on is the reason given, not observed.

#### Category 2b: ptr_elem cascade in pool_alloc / pool_get / pool_get_const (0 — RETIRED at VERIFY-036 F2; was 1, originally 6)

| # | Goal                                                       |
|---|-------------------------------------------------------------|
| 1  | `typed_cast_pool_alloc_assert_rte_mem_access`              |
**Coverage update (2026-09-21).** Five of six closed at CI #1285 when
`ptr_elem` / `ptr_elem_const` stated their result (VERIFY-023). The one that
remains, `pool_alloc_assert_rte_mem_access`, is the pool allocator's write
into the slot — the same address chain, one dereference deeper — and is the
goal the VERIFY-08 argument audit found cited against a compiled-out runtime
check. This block covers 1 goal.

**Coverage update (2026-10-05, VERIFY-036 F2) — the argument does not cover
this goal.** `pool_alloc` never dereferences the slot. Its contract admits
`pool == \null`, and its only null check is `require_msg`, which every WP job
compiles out, so the first memory access, `pool->used`, is unguarded. An
unsuffixed `assert_rte_mem_access` names that first access (to confirm from
the goal's source line in `frama-c-pool`'s JSON report). `pool_alloc(NULL)`
segfaults under `-DCANON_NO_REQUIRE` (probe
`tools/probes/verify-036/p_pool_null.c`). The slot-bounds argument below is
sound about the slot; it is not about this goal. This is the block's second
defect; the first was corrected on 2026-08-09.

**Coverage update (2026-10-05, VERIFY-036 F2) — this argument is RETIRED.**
The last goal closed at CI #1328 (8163f06) when `pool_alloc`'s contract
required a live pool. F2 added nothing to the proof but `pool != \null`, since
`pool_invariant` was already a hypothesis on every non-null path, so the goal
can only have failed on the NULL path: the slot-address attribution above is
refuted by measurement. The block covers nothing; its argument never applied
to the goal it was last filed against. Row kept for legibility.

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
(VERIFY-006 cat 3), not a real bounds gap.

**Configuration of the compensating control (corrected 2026-08-09).** An
earlier revision of this argument cited `pool_get`'s runtime `require_msg`
region check (`(u8*)p - buffer < end_mark`) as "the runtime backstop, exercised
at all build levels". That was wrong twice over and is withdrawn. First, every
WP job runs `-DCANON_NO_REQUIRE`, under which `contract.h` expands
`require_msg` to `((void)0)`, so the check does not exist in the configuration
in which this obligation is unproved. Second, the check lives in `pool_get`,
not in the function carrying the goal, so it would not discharge it even where
it is compiled in. What actually carries this obligation is the manual argument
above, from `pool_invariant`, and nothing else; the runtime check is a
defence-in-depth measure at default and debug build levels only. Per the
evidence standard, every class-(a) and class-(c) argument must now name the
build configuration in which any cited control exists.

#### Category 2c: bytes_from / mem_zero / mem_secure_zero call-sites (0 — RETIRED at VERIFY-036 F6; was 1, originally 5)

| # | Goal                                                          |
|---|----------------------------------------------------------------|
| 1  | `typed_cast_pool_alloc_zero_call_mem_zero_requires`           |
**Coverage update (2026-09-21).** The four `bytes_from` call-site goals
closed at CI #1285 (VERIFY-023). `pool_alloc_zero_call_mem_zero_requires`
remains. This block covers 1 goal.

**Coverage update (2026-10-06, VERIFY-036 F6) — this argument is RETIRED.**
The last goal closed at CI #1332 (c069881) when `pool_alloc` stated its result
address. The root cause below names the `ptr_offset` boundary, which
VERIFY-023 removed; after that the goal stayed only because `pool_alloc` did
not pass the address on, so the attribution had been stale since #1285. The
block covers nothing. Row kept for legibility.

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

#### Category 2d: arena delegation + reset wrapper assigns/ensures (0 — RETIRED at VERIFY-039 G7; was 1, originally 8)

| # | Goal                                                          |
|---|----------------------------------------------------------------|
| 1  | `typed_cast_pool_reset_call_arena_reset_to_requires_2`       |
**Coverage update (2026-09-21).** `pool_reset_secure`'s three closed at
CI #1285 (VERIFY-023). Five remain. This block covers 5 goals.

**Coverage update (2026-10-05, VERIFY-036 F5) — goal 1 is false.**
`pool_alloc_zero` zeroes the slot, which is not `pool->used`, the whole of its
assigns clause. The goal fails because the clause is too narrow;
`pool_alloc`'s assigns goals all prove, so the "through-effect" below does not
exist. Probe: `tools/probes/verify-036/p_zero_assigns.c`.

**Coverage update (2026-10-05, VERIFY-036 F5 scored).** F5 (c5281dc) added the
reserved window to the clause, so goal 1 is now true; it stays residual, as
predicted, because `pool_alloc` states nothing about where its result lies and
WP cannot place the zeroed slot in the window. The argument below still
misattributes it; it is rewritten with F6, which states `pool_alloc`'s result
address.

**Coverage update (2026-10-06, VERIFY-036 F6).** The former goal 1,
`pool_alloc_zero_assigns_normal_part3`, closed at CI #1332 once `pool_alloc`
stated its result address; its row is removed. False until F5 and opaque until
F6, it was never the delegation effect this block argues. The four
`pool_reset` goals remain.

**Functions affected**: `pool_alloc_zero`, `pool_reset`, `pool_reset_secure`.

**Root cause**: `pool_reset` delegates to `arena_reset_to` then re-reserves via
`arena_alloc`; `pool_reset_secure` adds a `mem_secure_zero` over the allocated
region before delegating to `pool_reset`. The wrapper-specific obligations
chain through the parent allocators' contracts:

- `pool_reset`'s `reset_ensures_part3` (re-establish `pool_invariant` after
  rollback-and-re-reserve) and the `arena_reset_to` / `arena_alloc` call-site
  preconditions reduce to the same arithmetic chain cat 2a leaves unproved,
  inherited through the delegation to `arena_reset_to` / `arena_alloc`.

  **Coverage update (2026-10-09, VERIFY-039 G4, arena repair).** Goals 2 and
  4, `pool_reset_call_arena_alloc_requires` and
  `pool_reset_reset_ensures_2_part3`, now also state `arena_invariant`'s
  conjunct, that the `Arena` lies outside its buffer. `arena_reset_to` keeps
  `buffer` and `capacity` (its frames) and re-establishes `arena_invariant`,
  conjunct included, as its own postcondition, and so does `arena_alloc`; the
  conjunct depends on nothing else but the `Arena`'s address. The argument
  covers it as written.

  **Coverage update (2026-10-09, VERIFY-039 G4, pool repair).** Goal 4,
  `pool_reset_reset_ensures_2_part3`, now also states the conjunct
  `pool_invariant` gained: the `Pool` lies outside its slots. `pool_reset`
  stores only `pool->used` into the `Pool`, and the two arena calls keep
  `buffer` (their frames), so the `Pool`'s address, `arena`, `base_mark` and
  `end_mark` and the `Arena`'s `buffer` are those of the precondition, whose
  `pool_invariant(pool)` states the conjunct. The argument covers it under
  the presumption cat 2a's goal 1 rests on: that `*pool` and `*pool->arena`
  do not overlap, which `pool_invariant` does not state (G5).
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

**Coverage update (2026-10-10, VERIFY-039 G5).** Goals 2, 3 and 4
(`pool_reset_call_arena_alloc_requires`, `pool_reset_reset_ensures_part3`
and `pool_reset_reset_ensures_2_part3`) closed at CI #1349 once
`pool_invariant` separated `*p` from `*p->arena` (fix 528bfa1); their rows
are removed. They were the missing separation, not delegation (VERIFY-033
F4, G5). Goal 1 stays, and it is not covered by this block's argument: an
admitted call sequence falsifies it (G7).

**Coverage update (2026-10-10, VERIFY-039 G7) — this argument is RETIRED.**
The last goal, `pool_reset_call_arena_reset_to_requires_2`, closed at
CI #1350 (f5c616c) once `pool_invariant` stated
`base_mark <= arena->offset`. It was never delegation: it was not true for an
admitted call (G7). All eight goals the block ever held have closed, and its
delegation reading covered none of the last four. The block covers nothing.
Row kept for legibility.

### Summary of pool.h-own residuals

| Category | Goals | Functions affected                                   | WP feature gap                              |
|----------|-------|------------------------------------------------------|---------------------------------------------|
| 2a       | 1     | pool_init                                            | the division bound `capacity <= CANON_USIZE_MAX / object_size` (VERIFY-039 G5) |
| 2b       | 0     | (retired at VERIFY-036 F2)                          | ptr_elem cascade (VERIFY-006 empty nonnull) |
| 2c       | 0     | (retired at VERIFY-036 F6)                          | bytes_from / mem_zero call-site (VERIFY-006/007/008) |
| 2d       | 0     | (retired at VERIFY-039 G7)                          | arena delegation + wrapper assigns/ensures; its last goal, `base_mark <= arena->offset`, closed once `pool_invariant` stated it |
| **Total**| **1** |                                                      | counts as of VERIFY-039 G7 (2 at G5, 9 before it; 24 before VERIFY-023) |

Cats 2b, 2c, and 2d are downstream consequences of VERIFY-006's
forward-implication note: ptr.h's `nonnull` behaviors carry no `ensures`
clause because strengthening them would itself require WP to discharge the
uintptr_t round-trip. pool.h inherits the cascade through up to four call
layers (pool.h → arena.h → ptr.h → uintptr_t). Cat 2a is pool.h's own
invariant-establishment arithmetic at pool_init; three of its five goals are
LIMITATION-SUSPECTED pending the manual review noted above.

**Update (2026-10-10, VERIFY-039 G5).** The paragraph above no longer
holds for cats 2a and 2d. Four of cat 2a's five goals and three of cat 2d's
four were the missing separation of the `Pool` from its `Arena`, not
arithmetic or VERIFY-006's cascade, and closed with it at CI #1349; the
LIMITATION-SUSPECTED review is done (cat 2a's coverage update). Cat 2a
keeps the division bound; cat 2d keeps one goal, which G7 records as false
for an admitted call.

**Update (2026-10-10, VERIFY-039 G7).** Cat 2d's last goal closed at
CI #1350 once `pool_invariant` stated `base_mark <= arena->offset`, and the
block is retired. Pool's own residuals: one, cat 2a's division bound.

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




**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 3793/3906 (was 3789/3902; +4 goals, all proved). Cause: inherited verbatim from slice.h via the substrate chain (VERIFY-007 note of the same date); pool.h has no own-goal change. The
unproved set is UNCHANGED — same count, same goal names (CI #1187
transcript is the name-stability record); no residual entered or left
the categories above, so the classification tables in this record
remain valid as written. Ratcheted with the acknowledged commit the
enforcement gate prescribes.

**Goal-surface reclassification (2026-07-30, CI #1202, Commit 16/16b):**
pinned proved-goal summary ratcheted to 3884/4003 (was 3793/3906; +97 goals, +91 proved), EXPECTED_UNPROVED to 119 (was 113).
Cause: the two `pool_*_cbytes` accessors added by API-001 plus the three `arena_*_cbytes` inherited through the two-hop substrate. The inherited/own split moves 89+24 to 91+28.

**Reading note (2026-08-09).** The body of this record above was written against the pre-API-001 figures and still says 89 inherited + 24 pool.h-own = 113. Those numbers are historically correct for the baseline they describe and are left as written; the enforced state since CI #1202 is 91 inherited + 28 pool.h-own = 119. Where the two disagree, the CI pin governs.

**The unproved set GREW — this is not the usual scalar-only drift.** Six goals entered: the two inherited `arena_free_cbytes` residuals, plus `typed_cast_pool_as_cbytes_call_cbytes_from_requires{,_2}` and `typed_cast_pool_reserved_cbytes_call_cbytes_from_requires{,_2}`.
Each new residual is the const twin of a residual this record ALREADY
documents: `pool_as_cbytes_call_cbytes_from_requires` times out on exactly the obligation its mutable
counterpart `pool_as_bytes_call_bytes_from_requires` times out on, for the same reason — WP cannot discharge
the `bytes_from`/`cbytes_from` validity precondition when the pointer
argument is computed rather than a plain member read. No new CATEGORY of
residual appeared; the categories in the tables above absorb them
unchanged, and every pre-existing residual is still present by name
(roll-calls extended, not replaced). Zero Failed goals.


**Reading note (2026-09-07, VERIFY-023).** Twenty-four of the residuals this
record classifies were misattributed — the eight inherited from arena (see
VERIFY-009's note) and sixteen of pool's own: every `*_call_ptr_elem*_requires`,
both `pool_get*_in_bounds_ensures_part4`, every `as_bytes` / `as_cbytes` /
`reserved_bytes` / `reserved_cbytes` `call_*bytes_from_requires` pair, and
`pool_reset_secure`'s three. They were recorded as call-site obligations and
frame goals. They were the callee's opaque return address, and closed the
moment `ptr_elem` stated it. The pin is now **3914 / 4009, 95 residuals** (83
inherited + 12 own). The text above is left as written.

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

**Coverage update (2026-10-09, VERIFY-039 G4, arena repair).** Goals 4 and
19, `region_end_ensures_4_part1` and `region_end_call_arena_reset_requires`,
now also state `arena_invariant`'s conjunct, that the `Arena` lies outside
its buffer. Goal 4's arena disjunct is trivial once `r->arena` is null. For
goal 19, the hook contract the argument relies on lets a hook allocate from
the arena; every arena function preserves `arena_invariant`, conjunct
included, and none moves the `Arena` or changes its `buffer` or `capacity`,
which is all the conjunct depends on. The argument covers it as written.

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

**Coverage update (2026-10-09, VERIFY-039 G4, arena repair).** All four goals
now also state `arena_invariant`'s conjunct, that the `Arena` lies outside
its buffer, through `region_invariant`. The argument covers it as written:
each mutator leaves `r->arena` null, stores a pointer of which the caller has
proved `arena_invariant` (now with the conjunct), or leaves it alone, and none
writes an `Arena`.

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
the `if (h->fn != NULL)` FALSE branch, API-unreachable; see MCDC-005). Region
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
  `if (h->fn != NULL)` FALSE branch unreachable).
- Coverage methodology: MCDC-001 (CANON_NO_REQUIRE flag).
- Substrate runtime tracking: OWN-001, OWN-002.
- Composable verification thesis: see README, "Composable
  verification" section.
- Per-goal CI artifact: `wp-proof-region` (full WP output).
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: core/region.h".

---


**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 3535/3647 (was 3531/3643; +4 goals, all proved). Cause: inherited verbatim from slice.h via the substrate chain (VERIFY-007 note of the same date); region.h has no own-goal change. The
unproved set is UNCHANGED — same count, same goal names (CI #1187
transcript is the name-stability record); no residual entered or left
the categories above, so the classification tables in this record
remain valid as written. Ratcheted with the acknowledged commit the
enforcement gate prescribes.

**Goal-surface reclassification (2026-07-30, CI #1202, Commit 16/16b):**
pinned proved-goal summary ratcheted to 3578/3692 (was 3535/3647; +45 goals, +43 proved), EXPECTED_UNPROVED to 114 (was 112).
Cause: inherited verbatim from arena.h's three `_cbytes` accessors (VERIFY-009 note of the same date); region.h has no own-goal change. The inherited/own split moves 89+23 to 91+23.

**Reading note (2026-08-09).** The body of this record above was written against the pre-API-001 figures and still says 89 inherited + 23 region.h-own = 112. Those numbers are historically correct for the baseline they describe and are left as written; the enforced state since CI #1202 is 91 inherited + 23 region.h-own = 114. Where the two disagree, the CI pin governs.

**The unproved set GREW — this is not the usual scalar-only drift.** Two goals entered, both inherited: `typed_cast_arena_free_cbytes_call_cbytes_from_requires{,_2}`.
Each new residual is the const twin of a residual this record ALREADY
documents: `arena_free_cbytes_call_cbytes_from_requires` times out on exactly the obligation its mutable
counterpart `arena_free_bytes_call_bytes_from_requires` times out on, for the same reason — WP cannot discharge
the `bytes_from`/`cbytes_from` validity precondition when the pointer
argument is computed rather than a plain member read. No new CATEGORY of
residual appeared; the categories in the tables above absorb them
unchanged, and every pre-existing residual is still present by name
(roll-calls extended, not replaced). Zero Failed goals.


**Reading note (2026-09-07, VERIFY-023).** The eight arena residuals reclassified
in VERIFY-009's note were inherited here verbatim and closed with them. The pin
is now **3592 / 3698, 106 residuals** (83 inherited + 23 own, region's own
surface unchanged by name).

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

## VERIFY-013: No Deviation to Record (error.h, and why this entry exists at all)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-013 |
| **Date**       | 2026-09-02 (id allocated at the time of the error.h run; entry written later, see below) |
| **Baseline commit** | error.h has been 65/65 with zero residuals since its verification run |
| **Scope**      | `semantics/error.h` — 4 functions, 65 obligations, **65 proved, 0 unproved, 0 residuals** |
| **Category**   | Formal verification completeness |
| **Enforcement**| `frama-c-error` pins 65/65 |

**Description**: there is no deviation here. That is the entire content, and it
is the reason this entry did not exist for as long as it didn't.

This file records DEVIATIONS. Every other entry names something that had to be
conceded: timeouts (VERIFY-003), manually discharged goals (VERIFY-002, -006),
a memory-model override (VERIFY-005), libc boundary limits (VERIFY-007, -008),
inherited substrate (VERIFY-009 through -011), function-pointer dispatch
(VERIFY-014, -015). `error.h` conceded nothing. 65 obligations, 65 proved,
nothing weakened, nothing inherited, nothing pending. It is the only module in
the project in that position.

So the id was allocated in sequence and no entry was written, because by this
file's own standard there was nothing to write. **That judgement was right and
is not being reversed.** What was wrong was the encoding: with no entry, twelve
references across `verification.md`, `traceability.md` and
`decision-families.md` — including a Reference-column cell in the WP accounting
table, whose one job is to point somewhere — cited a document that did not
exist. A dangling link in a traceability chain is a defect independent of
whether the target deserved to be written, and it survived every release since
error.h was verified.

Two fixes were available: rewrite the twelve citations to read "n/a — clean, no
deviation", or write the entry the citations already claim exists. The second
is taken, because the id is in circulation in shipped releases, because
"error.h is VERIFY-013 clean" is the natural way the prose already reads, and
because "this module deviated in no way" turns out to be worth one paragraph in
a file where every other module needed several.

**Found by** a one-off sweep for referenced-but-undefined ids, run on
2026-09-02 alongside the VERIFY-021 work, which also surfaced a two-week-old
staleness in `design-decisions.md` (lifetime.h described as "typedefs only"
after the token generator landed in that header at CI #1243–#1245). Neither
defect was in code or in a proof; both were in the assurance argument ABOUT
them. Nothing checks that argument mechanically today, and the sweep was not
retained as tooling — noted here so the next person knows this class of defect
is currently found by looking, and how long one of them survived.

**What this entry does not claim**: that error.h is trivially correct or that
its 65/65 required no work. Only that once proved, it left nothing behind.

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
`CANON_INVOKE_HANDLER_` path). These are the same two goals documented
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
`test/semantics/option_test.c` and by the cover TU (MCDC-006) across all
16 CI configs, with ASan + UBSan on the Linux and macOS debug builds
(CMakeLists.txt enables sanitizers only for Debug, and skips ASan under
MSVC), verifying absence of UB on every combinator path.

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
option reaches the handler through `expect`'s `CANON_INVOKE_HANDLER_`
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
`test/semantics/result_test.c` and by the cover TU (MCDC-007) across all
16 CI configs, with ASan + UBSan on the Linux and macOS debug builds
(CMakeLists.txt enables sanitizers only for Debug, and skips ASan under
MSVC), verifying absence of UB on every combinator path.

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
  `CANON_INVOKE_HANDLER_` vs `require_msg` reachability difference
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

borrow.h's gcov-measured MC/DC is 36/38 = 94.7% (surface per the
2026-07-24 MCDC-008 note), its documented
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

**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 2439/2458 (was 2433/2452;
+6 goals, all proved). Cause: +4 inherited verbatim from slice.h's
bytes_slice/str_slice clamp folds (VERIFY-007 note of the same date)
plus +2 from borrow.h's own borrowed_bytes_slice clamp fold — the same
guarded-initializer obligation pair, discharged by Qed/Alt-Ergo. The
unproved set is UNCHANGED: CI #1187's enforcement transcript records
the inherited roll-call at 17/17, the own-residual (borrowed_bytes_eq
memcmp danglingness) roll-call at 2/2, ZERO unproved outside the
documented set, and MCDC-008's dead_by_invariant cross-stream goal
still PROVED. Ratcheted with the acknowledged commit the enforcement
gate prescribes.

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
| **Scope**      | data/vec/ via `vmacros/vdrivers/vec_verify.h` — 5380 obligations, 196 unproved (121 inherited across 7 source families + 75 subject-side: 53 vec-own across 4 categories + 22 on the fresh result(Bool, Error) instantiation; originally recorded 143 inherited + 53 own — see Correction note 2026-07-16) |
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
the option instantiation (option_verify.h re-included verbatim — the
same verified instance as VERIFY-014) and a **fresh** result
instantiation at (Bool, Error) — a type pair not verified in
VERIFY-015's home unit (int, VErr); see the fresh-instantiation
section and Correction note below. Its 196-goal residual set is
correspondingly the largest, but decomposes exactly and was
name-stable across three consecutive runs before enforcement (runs 2,
3, and the enforced run 4).

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

### Inherited residuals (121)

Re-emissions of the substrate's pinned residual sets. The core arm
(families 1, 3–7 below) is **byte-identical** to arena.h's frozen
CI #1154 residual list — machine-diffed, zero differences, no prefix
map needed (both units run `Typed+Cast`), every `-wp-split` fragment
index reproduced (including the zero-padded `part03` quirk in the
try_alloc family), and the three Unknown sub-verdicts fall on the same
three memory goals in both units. The option arm (family 2) is
byte-identical modulo the documented `typed_` → `typed_cast_` prefix,
all 32 names including fragment indices, machine-diffed against
option's frozen CI #1154 log. By source family:

| # | Source family | Goals | Documented under |
|---|---------------|-------|------------------|
| 1 | arena.h own surface (ptr_span call-sites, fits/does_not_fit arithmetic chains, zero/try wrappers, free_bytes helpers) | 46 | VERIFY-009 |
| 2 | option combinator function-pointer dispatch (verified instance; driver re-included) | 32 | VERIFY-014 |
| 3 | memory.h own surface (allocation-model, alignment, memcmp danglingness) | 20 | VERIFY-008 |
| 4 | slice.h libc boundary (str_* / bytes_equal memcmp + strlen) | 13 | VERIFY-007 |
| 5 | ptr.h own surface (align formula ensures ×3 + align call-chain ×3) | 6 | VERIFY-006 cats 2–3 |
| 6 | checked.h manual-discharge overflow pair | 2 | VERIFY-002 |
| 7 | contract.h handler pair (definition-presence only) | 2 | VERIFY-006 cat 4 |

(The original table's family 3 — result, 22 goals — is reclassified to
the fresh-instantiation section below; its family 7 — "checked/align
model-variance", 5 goals — dissolves into rows 5–6 above, where those
goals were documented all along.)

**Byte-identity note (supersedes the original model-variance note —
see Correction note 2026-07-16)**: the inherited surface propagates
byte-identically, exactly as in the in-place stack
(VERIFY-009/-010/-011 pattern). The `checked_add`/`checked_add_u64`
overflow-ensures pair and the three `align_*` ensures are the pinned
**home residuals** of checked.h (VERIFY-002, manual-discharge) and
ptr.h (VERIFY-006 cat 2, solver-theory) respectively — they do not
prove in any home unit, and no goal in this TU flips verdict relative
to its home. Their three-consecutive-run stability observed here
stands; only the original interpretation was wrong.

The **12 `rte_function_pointer` goals all sit on option (inherited)
and result (fresh-instantiation) combinators — zero on vec
functions**, confirming prediction (f): vec is the first module whose
own surface contains no OWN-003-class dispatch at all.

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

**Coverage update (2026-10-08, VERIFY-039 G1) — three goals were false;
closed.** `pop_ok_ensures_4_part5`, `remove_ok_ensures_4_part6` and
`remove_ok_ensures_4_part7` were not element-transfer residuals: `pop` calls
neither `mem_copy` nor `mem_move`, and a frame-only contract yields exactly a
frame. All three failed on an `out` the contracts let point into the buffer,
and each was false for such a call (`tools/probes/verify-039/`). G1 (40cccd9)
requires `out` to be separated from the buffer; the three closed at CI #1341
and CC #28 in every unit that pinned them. The category now covers three
goals, each a content postcondition across `mem_copy` or `mem_move`, the
mechanism it states: `remove_ok_ensures_5_part6`, `insert_ok_ensures_5_part5`
and `append_array_ok_ensures_4_part5`.

**Coverage update (2026-10-09, VERIFY-039 G3) — frame-only is not the whole
obstacle.** The stated root cause, frame-only contracts on `mem_copy` and
`mem_move`, is one obstacle, not the whole: G3 gave `mem_zero` a content
postcondition through `char *`, and the two arena goals that read the same
bytes through `u8 *` stayed, because WP's typed model keeps one array per
integer kind. The three goals here read `int` cells (`Rec` fields in
`vec_struct`) across writes stated through `char *`, so content
postconditions on `mem_copy` and `mem_move` would not reach them either
(deduced, not tested). By the same mechanism the frames across the same
calls, `insert`'s and `remove`'s elements below `i`, would prove without the
call's range being checked; VERIFY-039 G4 tests that.

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

### Fresh-instantiation residuals (22) — result(Bool, Error)

vec's fallible API returns result(Bool, Error); VERIFY-015 verifies
the result family at (int, VErr). The driver therefore declares a new
instantiation inside this TU, with a lighter contract set than
result_verify.h attaches (no assigns clauses on any
`result__Bool_Error_*` function — per this run's own
`pedantic-assigns` warnings). Under the instance-level reading of
"previously verified", these 22 goals are a **new verification
subject**, not inheritance: this TU is their first verification.
Reconciled against the family's home profile (result_verify.h,
frozen CI #1154):

- **20 goals match the home profile** family-for-family,
  count-for-count, and split-for-split (map 4, map_err 4, and_then 3,
  or_else 3, eq 6) — the VERIFY-015 combinator-dispatch class
  reproducing at a new type pair under Typed+Cast.
- **8 home goals are never emitted** — exactly the seven assigns
  clause families (map/map_err assigns_exit + assigns_normal,
  and_then, or_else, eq ×2); a specification-surface difference from
  the driver's lighter contracts, not prover behavior.
- **2 goals are new**: `get_ok`/`get_err` `assert_rte_mem_access` —
  union-member accesses at the sites where WP emits its `wp:union`
  soundness caution. Home get_* carries no residuals and every
  union-member postcondition proved (VERIFY-015). Attribution is
  confounded across three simultaneously-changed variables (type
  pair × memory model × contract surface) — see finding F4.

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
- **F4 (CLOSED 2026-08-15 — contract-surface effect)**: attribute the
  fresh instantiation's `get_ok`/`get_err` `rte_mem_access` pair.
  These were, until this closure, the only two goals in the
  30,127-goal dataset whose cause was not pinned to a documented
  class.

  *The experiment as first run was confounded, and that is part of the
  record.* deque's driver (VERIFY-019) instantiates the same fresh
  (Bool, Error) pair and observed the goals ABSENT, and the run-1 CI
  log printed "model-emission effect attributable to Typed+Cast" on
  that basis. The inference did not hold: **two** variables differ
  between vec's run and deque's, not one — vec runs Typed+Cast with a
  REDUCED result contract surface, deque runs Typed with the FULL
  home contract set, which adds `requires \valid(out)` to
  `get_ok`/`get_err`. Either could explain the absence. The verdict
  was downgraded to an OBSERVATION in the deque job and F4 left open
  with the confound named.

  *The control that isolates the model.* `.github/workflows/f4-control.yml`
  (`workflow_dispatch` only, gates nothing) runs
  `vmacros/vdrivers/deque_verify.h` **unmodified** — contracts held
  fixed — under `-wp-model Typed+Cast`, moving only the model. It
  carries arena-32's positive control: if no `typed_cast_`-prefixed
  goals appear the flag was inert and a null reading would be
  meaningless, so the job refuses a verdict in that case.

  | run | model | positive control | `get_ok`/`get_err` pair |
  |-----|-------|------------------|--------------------------|
  | #1234/#1237/#1238/#1239/#1240 | Typed | n/a | ABSENT |
  | f4-control #1 (2026-08-15) | Typed+Cast | 67 cast / 0 plain — established | ABSENT |
  | f4-control #2 (2026-08-15) | Typed+Cast | 67 cast / 0 plain — established | ABSENT |

  **The memory model is ELIMINATED.** With contracts byte-identical
  across models and the instrument verified to have changed, the pair
  is not a Typed+Cast emission artifact. The surviving explanation is
  the contract surface: the full home set's `requires \valid(out)`
  discharges the union read that vec's lighter contracts leave open.

  Stated precisely, that was **elimination, not demonstration** — and
  the demonstration has since been run.

  **DEMONSTRATED 2026-08-17 (measured c427548 / CI #1246; ratcheted
  43a46b1 / CI #1247, 2026-08-18).** `vec_verify.h` contracted 3 of the 17
  emitted `result__Bool_Error_*` functions; `get_ok` and `get_err` were
  not among them, so nothing established that `out` was valid and
  `-wp-rte`'s memory-access obligation for each union read had nothing
  to discharge it. Contracting the two — shapes copied from
  `deque_verify.h`, no clause invented — removed **exactly** those two
  goals and nothing else.

  | | before | after | delta |
  |---|---|---|---|
  | total goals | 5429 | 5467 | +38 |
  | proved | 5231 | 5271 | +40 |
  | unproved | 198 | **196** | **−2** |

  The accounting closes with nothing unexplained: the two contracts add
  38 goals of their own (each carries a `requires`, a default `assigns`,
  two behaviors, and `complete`/`disjoint`), **all 38 prove**, and the
  two F4 goals flip — hence +40 proved against +38 total. Zero Failed,
  Invalid or Stepout, so neither new contract is falsified by the
  implementation it describes. The remaining 196 are the previous 198
  minus the pair, machine-diffed against the pinned roll-call.

  So the pair was never a property of `result(Bool, Error)`, never a WP
  union-model artifact, and not a memory-model emission: it was **a
  precondition vec's driver could have stated and did not**. Removable,
  not residual. The last two goals in the 30,127-goal dataset without a
  pinned class are now pinned to a cause and eliminated by fixing it.

  Ratcheted DOWN in one acknowledged commit — `EXPECTED_UNPROVED`
  198→196, the proved-line pin `5231 / 5429`→`5271 / 5467`, and the two
  `CHECKS` entries removed — per this job's standing rule that a goal
  flipping to Proved is a red run carrying good news.

  What the closure changes about the pair's status: it was never a
  property of `result(Bool, Error)`, and never a WP union-model
  artifact. It is a precondition vec's driver could have stated and
  did not — **removable**, not residual.

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

*(Figures below describe the CI #1154 baseline. The enforced values at
HEAD are `5271 / 5467` and an exact count of 196, pinned at 43a46b1 /
CI #1247; the CI #1202 state they superseded was `5231 / 5429` and 198.
See the Goal-surface reclassification notes and the DEMONSTRATED note
above. The gate DESIGN is unchanged — gates (0) and (2) below quote the
#1154 numbers, not the current pins.)*

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

### Correction note (2026-07-16)

Byte-level diffs of the frozen CI #1154 log artifacts (wp-proof-arena,
wp-proof-option, wp-proof-result, wp-proof-vec) falsified two
interpretive claims in this record as originally written. The pinned
196-name baseline, all four gates, and all counts are unaffected.

1. **The family-7 "model-variance" claim.** Superseded text, verbatim:
   *"`checked_add`/`checked_add_u64` overflow-ensures and the three
   `align_*` ensures prove in their home TUs but time out under this
   mega-TU's Typed+Cast context"* and *"the inherited surface is a
   model-variant subset, not the byte-identical propagation of the
   in-place stack […] identity is per-name within this TU across runs,
   not across TUs."* Both false: those five goals are the pinned home
   residuals of checked.h (VERIFY-002) and ptr.h (VERIFY-006 cat 2) —
   consistent with those records all along — and the full 89-goal core
   arm is byte-identical to arena.h's list (zero diffs; fragment
   indices and Timeout/Unknown sub-verdicts reproduced), with the
   option arm byte-identical modulo the `typed_` → `typed_cast_`
   prefix. No goal in the unit flips verdict relative to its home.
2. **The 143/53 inherited/own split.** Superseded accounting counted
   result(Bool, Error) as inheritance; VERIFY-015 verifies (int,
   VErr), so the 22 goals are a fresh instantiation — corrected to
   **121 inherited + 75 subject-side** (53 vec-own + 22
   fresh-instantiation).

These are the second and third instances (after VERIFY-012) of a
classification narrative corrected by evidence the enforcement
discipline itself preserved — the diffs required no re-run, only the
frozen artifacts the gates force every run to keep.

### Cross-references

- Inherited families: VERIFY-002/-006/-007/-008/-009 (substrate),
  VERIFY-014 (option combinators; VERIFY-015 supplies the *profile*
  for the fresh result instantiation), VERIFY-006 cat 4 (handler
  pair).
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

**Goal-surface reclassification (2026-07-23, CI #1187, Commit 9/9b):**
pinned proved-goal summary ratcheted to 5188/5384 (was 5184/5380;
+4 goals, all proved). Cause: inherited verbatim from slice.h's
bytes_slice/str_slice clamp folds via the memory-chain substrate
(VERIFY-007 note of the same date); vec's own goal surface is
unchanged — the Commit-9 sweep touched no vec/option/result source,
and the Commit-8b IMPL_VEC braces were already absorbed in the prior
baseline. The unproved set is UNCHANGED: 196 goals, same names (CI
#1187 transcript is the name-stability record; 193 Timeout + 3
Unknown, MODE: ENFORCED exact-match transcript). Pin provenance
updated from CI #1152 to CI #1187 in the workflow comments. Ratcheted
with the acknowledged commit the enforcement gate prescribes.

**Goal-surface reclassification (2026-07-30, CI #1202, Commit 16/16b):**
pinned proved-goal summary ratcheted to 5231/5429 (was 5188/5384; +45 goals, +43 proved), EXPECTED_UNPROVED to 198 (was 196).
Cause: inherited verbatim from arena.h's three `_cbytes` accessors via the memory-chain substrate (VERIFY-009 note of the same date); vec's own goal surface is unchanged. Pin provenance updated CI #1187 to CI #1202.

**Reading note (2026-08-09).** The body of this record above was written against the pre-API-001 figures and still says 196 residuals, a 121-goal inherited arm and an 89-goal core arm. Those numbers are historically correct for the baseline they describe and are left as written; the enforced state since CI #1202 is 198 residuals, a 123-goal inherited arm and a 91-goal core arm (the 91 machine-diffed set-identical to arena's roll-call on 2026-08-09). Where the two disagree, the CI pin governs.

**Extended 2026-08-21.** The F4 closure supersedes the CI #1202 state named immediately above. The enforced values at HEAD (43a46b1, CI #1247) are `5271 / 5467` and **196** residuals, decomposing as a 123-goal inherited arm, a 91-goal core arm within it, and a **20**-goal fresh-result arm — was 22, the `get_ok`/`get_err` pair having been *removed* by contract rather than reclassified. The 123 and the 91 are unchanged by the closure, which touched the subject side only. Note the coincidence of digits: the pre-API-001 body and the current HEAD both say 196, but they are different sets — the older 196 has a 121-goal inherited arm and a 22-goal fresh-result arm, the current one 123 and 20. Where any two of these figures disagree, the CI pin governs.

**The unproved set GREW — this is not the usual scalar-only drift.** Two goals entered, both inherited: `typed_cast_arena_free_cbytes_call_cbytes_from_requires{,_2}`.
Each new residual is the const twin of a residual this record ALREADY
documents: `arena_free_cbytes_call_cbytes_from_requires` times out on exactly the obligation its mutable
counterpart `arena_free_bytes_call_bytes_from_requires` times out on, for the same reason — WP cannot discharge
the `bytes_from`/`cbytes_from` validity precondition when the pointer
argument is computed rather than a plain member read. No new CATEGORY of
residual appeared; the categories in the tables above absorb them
unchanged, and every pre-existing residual is still present by name
(roll-calls extended, not replaced). Zero Failed goals.


**Reading note (2026-09-07, VERIFY-023).** The eight arena residuals reclassified
in VERIFY-009's note were inherited here verbatim and closed with them. The pin
is now **5285 / 5473, 188 residuals**; vec's own 53 are unchanged by name.

## VERIFY-019: Zero Core-Substrate Inheritance and a Memory-Model-Invariant Proof (deque, fourth driver-verified module, second data/-layer module)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-019 |
| **Date**       | 2026-08-15 |
| **Baseline commit** | Canon-C CI #1234 (run 1, surface established) → #1237 (run 2, set-identical) → enforced at CI #1238; re-confirmed #1239, #1240. Superseded pre-fix measurement: CI #1231 |
| **Scope**      | data/deque/ via `vmacros/vdrivers/deque_verify.h` — 1668 obligations, 67 unproved (2 inherited handler + 32 option arm + 28 fresh result(Bool, Error) instantiation + 5 deque-own) |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced (pinned Proved line + zero Failed/Invalid/Stepout + exact count + by-name roll-call over all 67) as of CI #1238 |

**Description**: 67 of 1668 proof obligations (4.02%) on the deque
driver are not discharged by the triple-prover configuration
(Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1) at a 120-second timeout with
`-wp-split`, and **0 are Failed, Invalid or Stepout** — no contract is
falsified. deque is the **fourth driver-verified module** (after
option, result and vec) and the **second data/-layer module**.

Two results distinguish this entry from its predecessors, and neither
is about deque's own code being hard to prove:

1. **The inherited surface is SMALLER than its predecessor's** — the
   first time in the arc. Every prior composability confirmation
   showed residuals propagating downward without amplification; deque
   tests the converse and finds **zero core-substrate inheritance**.
2. **The proof is memory-model invariant** — identical under Typed and
   Typed+Cast. Recorded separately as VERIFY-019-M below, because it
   is a claim about the verification method rather than about deque.

### Model: Typed, not Typed+Cast

`deque_impl.h` contains zero casts and its include closure is
types/limits/contract/ownership + error + option + result — none of
the cast-originating headers (no memory.h, ptr.h, slice.h, arena.h,
checked.h). deque is therefore the first data/-layer driver on plain
**Typed**, where vec was the first on Typed+Cast. The flag is
load-bearing for VERIFY-018 F4 and is called out in the job banner.

### Residual decomposition (67)

| Arm | Goals | Classification |
|-----|-------|----------------|
| contract.h handler | 2 | Inherited, definition-presence only. deque has **zero** direct `CANON_INVOKE_HANDLER_` calls (grep-confirmed), the clean-audit shape shared with vec and result. The VERIFY-006 cat-4 pair. |
| option_int_* | 32 | **Inheritance.** `option_verify.h` instantiates option at `int` — exactly deque's type parameter — so the verified instance is imported. These 32 are option's own roll-call (VERIFY-014) MINUS the 2 handler goals counted once above under ARM C. |
| result_Bool_Error_* | 28 | **Fresh instantiation** (the (Bool, Error) pair is not VERIFY-015's home (int, VErr)). 20 family-profile + 8 `assigns` goals. |
| deque_int_swap_ensures_6..10 | 5 | deque-own; the swap cluster. See below. |
| core substrate | **0** | See below. |

### Zero core-substrate inheritance — composability tested in the other direction

vec inherited **91** core goals byte-identically from arena.h's roll-call.
deque inherits **none**, because its closure contains none of the
headers those goals come from. Every previous confirmation
(VERIFY-008/-009/-010/-011/-016/-017) established that a downstream
module re-emits its substrate's residuals without adding to them.
deque establishes the complementary half: **a module does not inherit
what it does not include**, and the inherited surface can therefore
shrink as the arc proceeds. Predicted as ARM D's companion claim
before the first run and confirmed on every run since.

### The result(Bool, Error) arm: 28, and why not 30

The pre-registered arithmetic was 20 (family profile) + 8 (home
`assigns` clauses this driver attaches and vec's reduced surface
omitted) + 2 (the F4 `get_ok`/`get_err` pair) = 30. The observed value
is **28**: the F4 pair is absent under this driver's contract surface.
Per the instantiation-identity rule (`docs/vmacros.md`), this driver
takes the rule's FIRST option — attach the family's full home contract
set — where vec took the second (reduced surface, recorded). The 8
`assigns` goals returning is the *predicted* outcome of that choice,
not a regression; their absence would have meant the
int→bool / VErr→Error retyping had dropped a clause.

### The swap cluster (5) — an inherited class, widened

`deque_int_swap`'s ten field-wise `ensures` split cleanly: the five
a-side clauses (a receives b) prove; the five b-side clauses (b
receives `\old(a)` through the local temporary) do not. The obstacle
is therefore **preservation of `tmp` across the second struct write**,
not the struct copy itself.

This is **not a new residual class**. vec pins the same one as
`typed_cast_vec_int_swap_ensures` and VERIFY-018 classifies it as
"whole-struct copies under the cast model". deque reproduces it under
plain Typed **and** under Typed+Cast (VERIFY-019-M), so the
cast-model qualifier comes off: the class is memory-model independent
and VERIFY-018's wording should be read as widened accordingly.

The clause split is deliberately retained rather than collapsed to
vec's two combined `ensures`. Combining would drop the residual count
5 → 1 and look like progress while deleting the only evidence that
localises the failure to one direction of the exchange.

### Prediction scorecard (honest record)

Predictions were written into the driver header **before the first
run**, per the instantiation-identity rule's pre-registration
requirement.

| Prediction | Outcome |
|------------|---------|
| ARM C handler pair = 2 | **CONFIRMED** |
| ARM A option arm = 32, inheritance, **no** `typed_cast_` prefix | **CONFIRMED** (0 cast-prefixed observed) |
| ARM B result arm, fresh, full home contract set, 8 `assigns` present | **CONFIRMED** on classification and on the 8; count ADJUSTED 30 → 28 (the F4 pair) |
| ARM D deque-own = **0** | **REFUTED at run 0** (9 observed), then diagnosed — see below |
| core substrate = 0 | **CONFIRMED** |
| Class (g) macro-body-loop applies to deque | **WITHDRAWN before the run** — see below |
| Runtime well under vec's ~2h50m | **CONFIRMED** (~52 min) |

**Class (g) withdrawn.** VERIFY-018 forward-flagged "deque's shift
loops" into the macro-body-loop class. deque is a ring buffer: it
shifts nothing, and `deque_impl.h` contains exactly one `while` — the
`do {...} while (0)` of the contract idiom. The pre-classification was
carried over from vec's `insert`/`remove` memmove and does not apply.
Withdrawn in the driver banner before the first run rather than
quietly omitted, and the enforced job carries a gate that fails if any
loop goal ever appears on a deque function.

**ARM D refuted, then diagnosed.** Run 0 (CI #1231) observed 9 own
residuals against a predicted 0. They split along the at-risk
candidates named in advance:

- **4 goals — the driver's error, not the module's.** WP printed "No
  default assigns clause, using complete behaviors assigns" four
  times, once per *called* function. `pop_front`, `pop_back`,
  `peek_front` and `peek_back` carried `assigns` only inside their
  behaviors, so WP framed their CALL SITES with the union of the
  behavior footprints, which the `*_option` wrappers' none-branch
  `assigns \nothing` could not discharge. Fixed by adopting
  `vec_int_pop` / `vec_int_pop_option`'s shape verbatim — default
  `assigns` at contract top, none-branch inheriting it. vec has no
  pop_option assigns residual under that shape. The four goals have
  not returned in any run since, and the enforced job reports the
  `*_option` frame count separately so a return would be visible
  rather than absorbed.
- **5 goals — the swap cluster**, as above.
- **0 goals — the modular-arithmetic candidate did not fire.** No
  division-by-zero, no unsigned wrap; `deque_int_remaining`'s
  unguarded `capacity - size` (the canary) proved. This is the
  load-bearing claim of the module and the one worth having tested:
  the ring's `% capacity` sites are safe by `size <= capacity`
  threading through the early-return guards, with no runtime guard on
  capacity anywhere.

So ARM D's revised statement after diagnosis — **5, the swap cluster
only** — has held on every run from #1234 onward, and the driver's
prediction of zero was right about deque and wrong about the driver.

**A bookkeeping error, recorded.** The commit that set the revised
ARM D prediction updated `PREDICT_TOTAL` to 67 (correctly subtracting
the absent F4 pair) but left `PREDICT_RESULT` at its pre-run value of
30. The arms summed to 69 against a total of 67 — two equal and
opposite errors — so CI #1234's "TOTAL 67/67" matched by cancellation
rather than by being right. An internally inconsistent prediction can
be neither confirmed nor refuted; it is not a prediction. Corrected at
CI #1237, and the job now computes the arm sum and **exits before
invoking WP** if it does not equal the total.

### Findings for back-propagation

- **F1 (deque, open — doc patch owed)**: `pop_front`/`pop_back` test
  `!d || !out || !d->buffer` at RUNTIME and stay NULL-safe in every
  configuration. `peek_front`/`peek_back` guard the same arguments
  with `require_msg`, so under `-DCANON_NO_REQUIRE` they become raw
  dereferences while their same-named `pop_` siblings do not. Both
  families are the "safe" variants by name — the unchecked variants
  are separately spelled `*_unchecked`. The driver's `requires`
  clauses carry the obligation, so this is not a soundness problem
  for the proof, but the shipped doc comment does not flag the
  asymmetry. Comment-only; no code change proposed. The cover TU
  deliberately does not exercise the peek NULL legs (MCDC-011).
- **F2 (deque, open — doc patch owed)**: a NULL deque answers true to
  both `is_empty` and `is_full`. Defensible as fail-closed on both
  sides, and the driver's contracts state it explicitly rather than
  smoothing it over, but it deserves a sentence in the module doc.
- **F3 (deque, cosmetic)**: `int out = {0};` in the four `*_option`
  wrappers — brace-initialising a scalar is legal C99 but reads as a
  struct initialiser. Recorded only so the pre-run read is complete.
- **F4 (VERIFY-018) — CLOSED by this module's driver.** See
  VERIFY-018 F4. deque supplied both the confounded first observation
  and, via `f4-control.yml`, the control that resolved it.

### Cross-references

- Inherited families: VERIFY-006 cat 4 (handler pair), VERIFY-014
  (option arm, 32 goals), VERIFY-015 (result family profile for the
  fresh instantiation).
- Memory-model invariance: **VERIFY-019-M** below.
- Swap class: VERIFY-018 (`vec_int_swap_ensures`), widened here.
- MC/DC: **MCDC-011** (82/82, 100%; first cover TU with zero
  justification rows; Shape B confirmed).
- Split patch and the one-instantiation-per-TU rule:
  `docs/vmacros.md`; VERIFY-018 F3's checklist item, executed here at
  CI #1225 with byte-identical expansion verified across four
  linkage/type combinations before the driver was drafted.
- Per-goal CI artifact: `wp-proof-deque`.
- Wrapper: `.github/workflows/cmake-multi-platform.yml`, step
  "WP: vmacros/vdrivers/deque_verify.h (VERIFY-019, report-only)"
  (step name retained from the report-only arc; MODE now prints
  ENFORCED).

---

## VERIFY-019-M: Memory-Model Invariance of the deque Proof (Typed vs Typed+Cast)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-019-M |
| **Date**       | 2026-08-15 |
| **Baseline commit** | Typed: CI #1234/#1237/#1238/#1239/#1240 (enforced). Typed+Cast: `f4-control.yml` runs #1 and #2, both 2026-08-15 |
| **Scope**      | data/deque/ via `vmacros/vdrivers/deque_verify.h`, unmodified, under both WP memory models |
| **Category**   | Formal verification methodology |
| **Enforcement**| Not gated. The Typed side is enforced by the deque job; the Typed+Cast side is a manually dispatched control (`f4-control.yml`) |

**Description**: the deque driver proves **identically** under WP's
`Typed` and `Typed+Cast` memory models. This was not the question the
control was run to answer — it fell out of the F4 experiment — but it
is a stronger and more portable result than the F4 closure itself.

| run | model | proved | Qed | Alt-Ergo | CVC5 | Z3 | solver Σ | terminating | unreachable | T+U |
|-----|-------|--------|-----|----------|------|----|----------|-------------|-------------|-----|
| #1234 | Typed | 1601/1668 | 1239 | 230 | 27 | 20 | 277 | 38 | 47 | 67 |
| #1237 | Typed | 1601/1668 | 1239 | 235 | 24 | 18 | 277 | 38 | 47 | 67 |
| #1238 | Typed | 1601/1668 | 1239 | 228 | 30 | 19 | 277 | 38 | 47 | 67 |
| #1239 | Typed | 1601/1668 | 1239 | 235 | 25 | 17 | 277 | 38 | 47 | 67 |
| #1240 | Typed | 1601/1668 | 1239 | 233 | 20 | 24 | 277 | 38 | 47 | 67 |
| ctl #1 | Typed+Cast | 1601/1668 | 1239 | 244 | 16 | 17 | 277 | 38 | 47 | 67 |
| ctl #2 | Typed+Cast | 1601/1668 | 1239 | 230 | 27 | 20 | 277 | 38 | 47 | 67 |

Seven runs, two models, **one distinct tuple**. The 67 residual names
are set-identical across models modulo the `typed_` → `typed_cast_`
prefix (machine-diffed, zero symmetric difference, both controls).

**The instrument is known to have changed.** Per arena-32's rule, a
null result is evidence only when the flag demonstrably took effect:
both control runs report 67 `typed_cast_`-prefixed unproved goals and
0 plain `typed_`, and the job refuses to print a verdict otherwise.

**The invariant is sharper than "the numbers are stable."** What holds
across all seven runs is the whole decomposition: goal surface (1668),
Qed-discharged (1239), solver-discharged (277), terminating (38),
unreachable (47), residual (67). What moves is only *which* solver
reaches a goal first — Alt-Ergo 228–244, CVC5 16–30, Z3 17–24. So:
**which goals need a solver at all is invariant; which solver gets
them is scheduling.** That is the cleanest available justification for
the house rule that pools Timeout with Unknown — the partition is
noise, the union is the property. It joins arena-32's width-axis
result (same 91 residuals at 32-bit and 64-bit) as the second
invariance of this shape, on a different axis.

**Scope of the claim.** This is one TU. It says deque's proof does not
depend on the memory model; it does not say memory models never
matter — vec's Typed+Cast requirement is real and originates in
`void*` boundaries deque does not have. What deque shows is that when
a module's own code needs no casts, the model is not silently doing
work for it.

### Cross-references

- F4 closure this control produced: VERIFY-018 F4.
- Width-axis analogue: VERIFY-009-W / the `frama-c-arena-32` job.
- Swap-class widening this run enabled: VERIFY-019.
- Control workflow: `.github/workflows/f4-control.yml`
  (`workflow_dispatch` only; gates nothing).
- Artifact: `wp-f4-control-typedcast`.

---

## VERIFY-020: Specification-Strength Inheritance, and a Prover-Load-Bearing Redundancy Twice (bitset, fifth driver-verified module, third data/-layer module)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-020 |
| **Date**       | 2026-08-27 |
| **Baseline commit** | Canon-C CI #1266 (16d0f0b). Enforced at #1260 with 158; ratcheted to 163 at #1265 after F2/F5; re-confirmed at #1266. Superseded measurements: #1251 (221), #1256 (218), #1257 (172), #1258 (163 own-66), #1259–#1263 (158) |
| **Scope**      | `data/bitset.h` via `vmacros/vdrivers/bitset_verify.h` — 5002 obligations, 163 unproved (2 handler + 32 option_usize + 58 core substrate + 71 bitset-own) |
| **Category**   | Formal verification completeness |
| **Enforcement**| Enforced: pinned Proved line, zero Failed/Invalid/Stepout, exact count, and a by-name roll-call over all 163 in **both directions** (set equality) |

**Description**: 163 of 5002 proof obligations (3.26%) on the bitset
driver are not discharged by the triple-prover configuration
(Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1) at a 120-second timeout with
`-wp-split`, and **0 are Failed, Invalid or Stepout** — no contract is
falsified. bitset is the **fifth driver-verified module** and the
**third data/-layer module**.

**71 own residuals is the smallest own-residual set of any
driver-verified module in the project** — vec 196, pool 119, region
114, arena 91, deque 67. The class breakdown is derived by the job
rather than hardcoded, and sums exactly:

| n | class |
|---:|---|
| 26 | array framing on and / or / xor / not |
| 22 | Group 1 single-bit family + init |
| 9 | clear_all / set_all frames over symbolic mem_zero / mem_set |
| 6 | P4 Typed+Cast bridge at the three as_bytes sites |
| 8 | queries and finds |
| **71** | |

Three results distinguish this entry.

1. **SPECIFICATION-STRENGTH INHERITANCE** (F4) — a residual class new to
   the campaign. Every prior ceiling was PROVER weakness: a true
   property the solver could not reach. Here the ceiling is UPSTREAM
   SPECIFICATION weakness — a property that cannot be stated at all,
   because the callee's contract is deliberately too weak to support it.
2. **A REDUNDANCY LOAD-BEARING FOR THE PROVER, FOUND TWICE.** Once by
   deleting a predicate disjunct (E1, two dead CI runners). Once by
   deleting a redundant precondition (F5, five goals). The second time
   happened with the first already documented 300 lines above the edit.
3. **REAL UNDEFINED BEHAVIOUR** (F2), demonstrated under ASan/UBSan, in
   a family whose asymmetry had been recorded as a comment for two
   weeks before it was fixed.

### Specification-strength inheritance (F4)

`bits_popcount`, `bits_ctz` and `bits_clz` are specified at RANGE
strength only — `0 <= \result <= 64` — by a written decision in bits.h
that a functional definition needs an axiomatisation beyond current SMT
capability. Three consequences propagate into bitset, none of them a
gap in bitset:

- `bitset_count` cannot claim `\result <= bs->capacity`. That needs a
  functional popcount. The strongest honest claim is
  `\result <= 64 * word_count`, and that is what is written.
- `bitset_is_full` delegates to `bitset_count` and is capped
  TRANSITIVELY.
- `bitset_find_first` / `_next` / `_last` cannot claim MINIMALITY.
  `\result == BITSET_NPOS || \result < capacity` IS provable, straight
  from the explicit guard. That the result is the FIRST or LAST set bit
  is not. The `_option` wrappers inherit the same cap a second time.

The cap is exactly one word wide — "minimality" — and everything else
about these functions is specified. Naming its width precisely is the
point: an unqualified "capped by bits.h" would understate what the
contracts do deliver.

### The redundancy that was load-bearing — twice

**E1 (CI #1254).** `bitset_pad` reads

```
capacity % 64 == 0 || (words[wc-1] >> (capacity % 64)) == 0
```

E1 replaced this with `(words[wc-1] >> bitset_rem(bs)) == 0`. The two
are mathematically equivalent — verified exhaustively over capacity in
[1,600] plus the 64/128/192/1000/4096/65535 boundaries.

**It took the CI runner down twice on the same commit.** Once as "the
hosted runner lost communication with the server", once at 1h 0m 2s
with the log cut mid-results, against a 360-minute default timeout.
Every sibling WP job was green on the same runner pool and commit.

The mechanism: `capacity % 64` is SYNTACTICALLY bounded — a solver sees
`% 64` and knows the exponent lies in [0,63] without deriving anything.
`bitset_rem(bs)` is an arithmetic expression whose bound in [1,64]
follows only from `bitset_sized`, which the solver must derive FIRST.
Until it does, the term is `x >> k` with k an unbounded symbolic
expression — `x / 2^k`, exponential in a free variable. Memory
blow-up, which `-wp-timeout` cannot bound because a timeout bounds
time, not resident memory. `bitset_pad` now carries a DO-NOT-SIMPLIFY
note at the predicate.

**F5 (CI #1264), the same lesson on a precondition.** Five functions
carried `requires bs == \null || bs->words != \null`. That clause is
LOGICALLY REDUNDANT: `bitset_mut` contains
`\valid(bs->words + (0 .. word_count-1))`, which for word_count >= 1
already implies `words != \null`. That was checked before deleting it.

WP used it anyway. With the precondition, the memory-access RTE goal
discharged from the caller obligation directly. Without it, WP must
derive validity from the runtime guard instead, and times out. Five new
`assert_rte_mem_access` residuals, one per function.

`bitset_not` is the clean control: its code did not change, its
behaviors did not change, and the ONLY edit was deleting that one
redundant line. It cost exactly one goal.

So: a clause redundant for the LOGIC can be load-bearing for the
PROVER, on predicates and on preconditions alike. The second discovery
was made with the first already written down, in the same file, a few
hundred lines above the edit. Documenting a lesson is not the same as
having learned it.

### Three refuted hypotheses, recorded because they cost four runs

| # | Hypothesis | Outcome |
|---|---|---|
| 1 | The predicates are too big to re-establish | Refuted by `init_ensures_3`, whose code line is literally its own assignment |
| 2 | A missing arithmetic LEMMA (this was P1) | Never needed; E1's division-free form made the same fact subtraction |
| 3 | Integer division in PROVEN positions | Refuted by E1a (#1255): all 18 unchanged, same goal indices |

The error was not any single hypothesis but the METHOD — diagnosing by
archaeology on straight-line goals instead of measuring the thing in
question. The correction was to contract ONE loop (`bitset_not`, the
simplest in the header) and read the result. That probe cost the same
one run each wrong guess had cost, and could not be wrong about what it
measured, because it WAS the thing rather than a proxy. It came back
6 → 3 with both postconditions proved, which authorised the remaining
nine loops.

A second-order correction belongs here too: after E1a refuted division
as the cost of the Group-1 POSTCONDITIONS, that was over-generalised
into "division is expensive everywhere in this TU", producing a written
prediction that `find_*` would improve LEAST of its batch. They improved
MOST — 14 → 4. Division inside an RTE index obligation is cheap when
`bitset_view` supplies the sizing equation as an ASSUMPTION. That is the
free-vs-expensive POSITIONAL asymmetry E1 originally identified and that
was wrongly abandoned along with E1's other half.

### Two contracts deliberately NOT written

**The as_bytes bridge (6 goals).** `bytes_from` requires
`\valid((u8*)ptr + (0 .. len-1))` while `bitset_view` supplies
`\valid_read(bs->words + (0 .. word_count-1))` as a `u64*` range. Same
memory; WP's Typed+Cast model does not transport range validity across
a pointer-type change. Adding `requires \valid((u8*)bs->words + ...)`
to each of the three would clear all six by pushing a byte-level
obligation onto every caller, about memory the function already knows
is valid. Rejected: it buys a cleaner number by making the API worse.

**Self-application of and/or/xor.** Those three read `other->words[w]`
at the same index they write `bs->words[w]`, so a pointwise
postcondition is FALSE under aliasing. They therefore require
`\separated`, which puts `bitset_and(&a, &a)` outside the specification
even though it is a harmless no-op at runtime. The alternative —
dropping the postcondition so self-application stays admissible — was
rejected: a contract that says nothing about the result is worth less
than one that says something under a stated precondition.
`bitset_is_disjoint` is read-only, needs no `\separated`, and keeps
self-application inside its spec; the asymmetry is deliberate and noted
at both sites.

### Enforcement, and why set equality in both directions

The count gate is necessary but NOT sufficient: a residual could start
proving while a new one appeared, leaving the total unchanged, and a
count-only gate would pass in silence. The job checks that every one of
the 163 pinned names IS unproved, and separately that no unpinned goal
is unproved.

Both name arrays were GENERATED by script from CI output, never
transcribed. Hand-copying 158 identifiers is the mechanical-error class
this arc tripped on repeatedly: an inverted roll-call at #1252, a
prefix collision introduced in the same commit that fixed it, a caller
updated where the tree needed grepping, and — at #1265 — a
string-indexed edit that matched frama-c-DEQUE's CHECKS array, the
first of fourteen in the file, and would have replaced deque's 67
pinned names with bitset's 163. That was caught only because the
verification counted entries afterwards instead of trusting the
script's own success message.

The gate is NEGATIVE-tested before each pin change: one fixture with a
residual removed, and one SWAP fixture where one proves and one new
appears with the total unchanged. The swap case is the one a count-only
gate cannot see.

**Stability**: #1264 and #1266 produced identical 163-name sets across
a source change in between. What DOES move run to run is goal ORDER and
prover assignment — `checked_add` has been answered by both Z3 and
Alt-Ergo on different runs. An ordered diff would go red on a run where
nothing changed; set equality is the correct instrument. Same result
VERIFY-019-M reached on the model axis: *which goals need a solver is
invariant; which solver reaches them first is scheduling.*

### Predictions, scored

| | Verdict |
|---|---|
| **P1** | REFUTED, then SUBSUMED. Called for a LEMMA; none was written, and the goals it named are residuals anyway. Right that the obstacle was arithmetic, wrong about mechanism and fix. |
| **P2** | EXACT. 92, family by family, on every run #1251–#1266. The only prediction right first time and never adjusted. |
| **P3** | CONFIRMED. bits.h's 15 re-emit byte-identically. bitset is the first verified module whose closure contains bits.h, so this was byte-identity's first test on a timeout-class ARITHMETIC surface rather than libc or a WP feature gap. |
| **P4** | CONFIRMED, OVER-COUNTED. The Typed+Cast bridging class is real and sits exactly where predicted, at 6 goals rather than 9. Lower confidence had been recorded before the run rather than after. |

### Model: Typed+Cast

Forced by `bitset_as_bytes` / `_as_cbytes` / `_as_borrowed_bytes`,
which hand `bs->words` (a `u64*`) to byte-view constructors. Goal names
carry the `typed_cast_` prefix throughout. An earlier draft of this arc
predicted plain Typed; corrected before the first run.

### Classification: in-place contracts, thin interposition driver

Contracts live in `data/bitset.h` itself.
`vmacros/vdrivers/bitset_verify.h` exists solely to interpose a
CONTRACTED `option_usize` ahead of the header's own instantiation — it
contracts none of bitset's functions and is not a Shape-B driver.

### Findings

- **F1 — FIXED** (f18e72c). `BITSET_WORD_COUNT(n)` is `(n+63)/64` in
  usize and WRAPS above `CANON_USIZE_MAX - 63`. Measured on the
  unguarded header: `capacity == USIZE_MAX` yields `word_count == 0`,
  `mem_zero` writes zero bytes, and **`bitset_init` succeeds silently**
  — after which every `i < bs->capacity` guard passes for any `i`,
  making `bs->words[i / 64]` an out-of-bounds write at a
  caller-controlled offset. The realistic trigger is not an absurd
  literal but an ordinary upstream underflow,
  `bitset_init(&bs, buf, n - 1)` with `n == 0`. The initial disposition
  was "live design question"; that was wrong, and the measurement is
  what changed it.

  **Residual exposure, stated rather than implied**: the guard is a
  `require_msg`, which is `((void)0)` under `-DCANON_NO_REQUIRE`, so
  the release configuration is still exposed. That is the library's
  model for every constructor guard, but it means the fix hardens
  contract-checked builds only.

- **F2 — FIXED** (61e1312). `set`/`clear`/`toggle`/`test` checked only
  `!bs`, while eleven siblings checked `!bs || !bs->words`. On a
  zero-initialised `Bitset b = {0}` the index guard compiled out under
  `-DCANON_NO_REQUIRE` and the four dereferenced NULL. Demonstrated by
  reverting only those guards and running under ASan/UBSan:
  `runtime error: load of null pointer of type 'u64'`, then
  `AddressSanitizer: SEGV on unknown address 0x000000000000`.

  Scope was wrong twice in the fixing: FIVE functions, not four
  (`bitset_assign` delegates and was exposed transitively), and five
  lines per contract, not the "exactly one line" the old comment
  predicted. The count was caught by an assertion, not by reading.

- **F3 — FIXED** (19febec). bitset.h was the only header in the tree
  requiring callers to pre-instantiate a generic. It now
  self-instantiates `option_usize` behind `CANON_OPTION_USIZE_DEFINED`,
  vec_impl.h's convention verbatim. Found when frama-c, handed the
  header directly, aborted in the PARSER.

- **F4 — CLASSIFICATION**, the specification-strength cap above. Not a
  defect; a documented ceiling.

- **F5 — FIXED** (61e1312), found while scoping F2. `bitset_not`
  carried BOTH `requires bs == \null || bs->words != \null` AND
  `behavior null: assumes bs == \null || bs->words == \null`. The
  precondition forbade exactly what the behavior claimed to handle and
  what the code has always guarded at runtime, so the null behavior was
  VACUOUS — WP could never enter it. A leftover from the Group-1
  contract template, carried in when `bitset_not` was contracted as the
  Group-2 probe.

  Found by scanning all 32 contracts for the signature "requires
  words != null AND a null behavior assuming words == null".
  `bitset_not` was the only hit, which is the useful part: the scan is
  what makes "only one" a measurement rather than an impression.

  **Proof cost**: fixing it, together with F2, added the five
  `assert_rte_mem_access` residuals described above. Correct
  specification, five timeouts. Recorded as a price, not absorbed into
  the count.

### Cross-references

- Coverage-stream record for the same module: MCDC-012 (130/134, four
  invariant-dead justification rows J1–J4).
- The `bitset_pad ==> bitset_pad_meaning` MANUAL-PROOF obligation: the
  masking equation the code establishes is not the sentence a reader
  wants, and the equivalence is bit-level reasoning the SMT backend is
  not expected to discharge. Recorded rather than asserted by an
  annotation nothing checks — the lifetime.h lesson applied ahead of
  time, where two of eleven superseded copies of
  `canon_lifetime_next_id_` carried a false `assigns \nothing` that
  survived precisely because no verified configuration translated it.
  MCDC-012's J1–J4 are the SAME gap seen from the coverage side.
- MISRA: `data/bitset.h` contributes zero real violations; the pinned
  total is 53, unchanged across the arc. F1's guard briefly raised it
  to 54 under rule 12.1 (explicit precedence) and was parenthesised at
  fad155a.

---


**Reading note (2026-09-07, VERIFY-023).** The `frama-c-bitset` job could not
fail the build. Every other accumulator-style WP job ends its step with
`if ENFORCE && FAILURES then exit 1`; this one did not, and had printed
"ENFORCED" since CI #1259 without ever being able to turn red. Found at CI
#1284, when the step printed `VERIFY-020 ENFORCED FAIL: 1 check(s)` and the
job went green; an audit of all 28 gated steps found it to be the only one.
The name-stability evidence from #1259 onward is real — every roll-call printed
`missing 0 / unpinned 0` — but **enforcement of this module begins at CI
#1285**, when the gate was wired, not at #1259 as the table above says. The pin
is now **4845 / 5008, 163 residuals**, set unchanged by name; the +6 is
VERIFY-023's goal delta.

**Reading note (2026-10-10, VERIFY-039 G8).** `bitset_not`'s pointwise
postcondition and its loop invariant 2 are false as written: in ACSL `~` is
the integer complement, negative for a `u64` word. The invariant's unproved
preservation, counted above under array framing, is unproved because it is
false, and the postcondition counted among the probe's results ("both
postconditions proved") is proved from it. See VERIFY-039 G8.

## VERIFY-021: An Instrument Verified, and a Frame Clause That Cannot Be Written (lifetime.h, ladder level 4 only)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-021 |
| **Date**       | 2026-09-02 |
| **Baseline commit** | Canon-C CI #1275. The `frama-c-lifetime` JOB reported an identical `Proved goals: 4 / 4` (Terminating 1 / Unreachable 1 / Qed 2) at #1271 (307307c), #1273 and #1274. Note #1273 and #1274 were cancelled at the WORKFLOW level after this job had already completed — unrelated long-running WP jobs were stopped to save runner minutes — so the three-run evidence is job-level, not whole-run. #1275 is the first uncancelled run under enforcement and is the citable baseline. Prior attempts: #1269 (build), #1270 (build), #1271-a (ACSL rejected, ARM B) |
| **Scope**      | `core/primitives/lifetime.h`, `canon_lifetime_next_id_` only, under `CANON_LIFETIME_DEBUG` + `CANON_LIFETIME_NO_ATOMICS` (ladder level 4) — 4 obligations, 0 unproved |
| **Category**   | Formal verification completeness |
| **Enforcement**| **ENFORCED at CI #1275.** Pinned total 4 and unproved 0; Failed/Invalid/Stepout gate unconditionally. Promoted after three name-identical job results (see baseline note on their cancelled parent runs) and after the blocking open item below was closed. Enforcement was NOT set in the commit that first turned the job green — that was #1271, five commits earlier |

**Description**: this is an **instrument-integrity** entry, not a module arc,
and it must not be read as one. It verifies the lifetime token GENERATOR. It
says nothing about whether client code obeys the borrow discipline — that is a
typestate property over all callers, WP proves contracts on functions, and it
remains issue #7's territory. `lifetime.h` was the last unverified instrument
in a project whose entire argument rests on measurement discipline.

4 of 4 obligations discharged, 0 unproved, 0 Failed/Invalid/Stepout.

### Why a configuration, not a contract

Until 2026-09 the header carried no ACSL, deliberately, on written grounds:
every WP job ran with `CANON_LIFETIME` off, so a contract would have been
unchecked decoration. Two of the eleven superseded private copies had carried
`assigns \nothing` — FALSE, the function writes the counter — and it survived
unnoticed for exactly that reason.

That argument was correct and is **not overturned here. It is satisfied.** The
fix for an unchecked annotation is a configuration that checks it. Job
`frama-c-lifetime` is that configuration.

Levels 1–3 are excluded **permanently**: WP has no concurrency model for
`atomic_fetch_add_explicit`, `__atomic_fetch_add` or `_InterlockedIncrement64`.
The contract is `#if`-gated on `CANON_LIFETIME_ATOMIC_LEVEL_ == 4` so it cannot
silently acquire authority over the other three. The ladder's correctness
argument remains the prose one in `docs/thread-safety.md`.

**The roadmap row reads "verified at level 4 only" and must never be shortened
to "verified".** Three of the four ladder paths — including every path a
threaded build actually takes — carry no machine-checked claim whatsoever.

### F1 — a frame clause that cannot be written (ARM B, refuted on its reason)

The prereg predicted run 0 would break on `assigns counter_`, and that the
cause would be NAMING: Frama-C hoists function-local statics, so the mangled
name would fix it. **Confirmed on where it broke. Refuted on why.**

Frama-C reported `unbound logic variable counter_` and rejected the entire
specification. The cause is not a name to look up. `counter_` is a
function-local static and therefore does not exist in the file scope where a
FUNCTION CONTRACT is parsed. There is no spelling that works. Three responses
were available:

1. `assigns \nothing` — FALSE, and precisely the historical defect this arc
   exists to correct. Rejected outright.
2. Hoist `counter_` to file scope under `__FRAMAC__` — makes the clause
   writable by verifying a DIFFERENT program than the one that ships.
   Rejected: the project verifies shipped source, never a copy. This is the
   same rule that forbids a driver reimplementing its subject.
3. Write no frame clause. WP assumes `\everything`, which is sound and claims
   nothing false.

(3) is taken. WP's own `[wp:pedantic-assigns]` warning — "No 'assigns'
specification ... Callers assumptions might be imprecise" — is retained in the
uploaded artifact rather than suppressed. It is the tool's record of exactly
the position documented here, and suppressing it would hide the one honest
signal that a frame is missing.

This is a **specification-strength ceiling**, the same family as VERIFY-020 F4:
a limit on what can be STATED, not on what a prover can DISCHARGE. F4's ceiling
came from a callee's deliberately weak contract; F1's comes from ACSL's scoping
rules meeting C's storage classes. Same family, different source. The effect is
witnessed at runtime instead, by `test_counter_advances`.

### What is claimed

`ensures \result != REGION_ID_STATIC` — the property with teeth. Handing out 0
marks the owner as static-lifetime, so every borrow over it stops expiring: the
instrument fails **OPEN**, the worst failure mode an instrument has.

**Distinctness is NOT claimed and is not claimable.** It is a property over a
SEQUENCE of calls; a WP contract speaks about one. See L2 below — it is not
merely unproved but false.

### Prediction scorecard (registered before run 0)

| Arm | Prediction | Outcome |
|---|---|---|
| A | `\result != REGION_ID_STATIC` proves under Typed AND Typed+Cast | **CONFIRMED under Typed, and more precisely than predicted.** `-wp-split` decomposes it into exactly the two guard legs (Then / Else), both Qed-valid with no prover call — see below. The guard is total, so the postcondition holds independently of how the model interprets the pointer-to-integer cast. Typed+Cast control still not run |
| B | assigns clause breaks run 0, on naming | **Confirmed on where, REFUTED on why.** See F1 |
| C | 0 inherited residuals | **Weakly confirmed only.** With 0 TOTAL residuals the run cannot discriminate "inherited nothing" from "inherited goals that all proved." Not refuted; not discriminating either. Recorded as weak on purpose |
| D | 2–10 total goals, 0 Failed/Invalid/Stepout | **CONFIRMED (4).** Note the contract shrank after ARM B, so the range was set against a two-clause contract and scored against a one-clause one — a caveat on the confirmation, not a defence of it |
| E | No new residual class | **Vacuously held** (0 residuals). No information |

Three of five arms carry caveats. That is the honest reading of a run this
small, and the entry is written to prevent "5/5 goals, all arms confirmed"
being quoted from it.

### The `Unreachable` goal — closed twice, and the second question was malformed

**First question: is it the `REGION_ID_STATIC` guard?** If it were, WP would be
calling unreachable the same branch MCDC-013 disposes of as
reachable-but-undriveable, and one entry would be wrong.

It is not. `-wp-print` at CI #1274 shows `-wp-split` decomposing the
postcondition ALONG THAT GUARD:

```
Goal Post-condition (lifetime.h, line 324) (1/2):  Tags: Then.   Qed Valid
Goal Post-condition (lifetime.h, line 324) (2/2):  Tags: Else.   Qed Valid
```

Both legs are scheduled and both discharged. WP **reasons about the case where
the guard fires and proves the postcondition through it**, so it holds the
branch reachable in the model. That is not merely the absence of a
contradiction but positive cross-stream agreement: coverage says the branch
cannot be DRIVEN on a hosted x86-64 runner, WP says it can be REACHED in the
model, and both are true because they are different claims. The MCDC-013
justification row stands, corroborated rather than asserted.

**Second question: then which program point IS it?** This was recorded as an
open curiosity and then closed at CI #1275, where the answer turned out to be
that the question does not parse.

`-wp-prover none` generates every goal and proves none — nothing can be
absorbed by Qed before it is listed. It lists **two** goals: the same
Then/Else postcondition pair. So the `Terminating: 1` and `Unreachable: 1`
lines in the summary do **not** correspond to generated obligations over
program points. There is no branch for them to name. They are structural
entries in the WP property tally, discharged without an obligation ever being
produced, and they appear in the `Proved goals: 4 / 4` count because that
count tallies PROPERTIES rather than obligations.

Three flags were spent before this was understood, and the sequence is worth
recording because each failure was informative rather than wasted:

| flag | outcome | what it established |
|---|---|---|
| `-wp-verbose 2` | repeated the category counts | verbosity does not enumerate goals |
| `-wp-out` | wrote **no files** | it writes only for goals sent to a PROVER, and Qed closes every goal here first |
| `-wp-prover none` | listed exactly 2 goals | the two entries are not obligations at all |

The `-wp-out` result is the substantive one: an empty obligation directory on a
unit that reports 4 proved goals is not a tooling failure, it is evidence that
nothing reached a prover.

The decomposition also sharpens ARM A beyond what was registered: the
postcondition does not merely prove, it proves as exactly two Qed obligations
corresponding to the two guard legs, with no prover call on either.

The diagnostic step is retained under enforcement as a regression detector
rather than deleted: if a future change makes WP emit a third goal here, it
surfaces in the log before the pinned count fails the build.

### L1 — a rationale that reasoned about the wrong quantity

The header justified "a valid owner will never have ID 0" from the fact that no
object has address 0. But the id is `counter ^ address`, not the address. It is
0 exactly when the two coincide, and the counter walks 1, 2, 3, …

| target | counter value needed | verdict |
|---|---|---|
| hosted x86-64 (stack ~`0x7ffc…`, heap ~`0x5653…`) | ~1e14 calls | infeasible |
| low-RAM embedded, object at `0x0800` | ~2e3 calls | ordinary |

Canon-C targets the second. Measured: over 4096 calls where the address equals
the counter, the guard's TRUE leg fires 4096/4096. **The code was always
correct** — the guard catches it. Only the justification was wrong, and it
justified a property of `owner_` while the value at risk was `c_ ^ owner_`.
Found by attempting the proof, not by reading the file.

### L2 — the generator is not injective, by construction

The guard maps the xor-cancelling case onto 1, and 1 is also produced directly
when `counter ^ address == 1`. Demonstrated: owner `0x7` at call 7 and owner
`0x8` at call 9 both receive id 1. The rate is negligible and **this is not a
defect**. It is recorded because a reader may otherwise assume uniqueness, and
because it fixes the boundary of what the contract can say.

### Relation to the composition result

**None, and the entry should not be cited in that argument.** The include
closure is `<stdbool.h>` + `types.h`, which pulls only `<stddef.h>`,
`<stdint.h>`, `<stdbool.h>`. No `contract.h`, so not even the 2-goal handler
pair every residual-carrying header has inherited. There is nothing composed
here and nothing to measure about composition.

## VERIFY-022: A Heap Verified in Place, an Information Horizon Closed by a `calls` Clause, and Three Count Predictions Refuted in a Row (priority_queue.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-022 |
| **Date**       | 2026-09-07 |
| **Baseline commit** | Canon-C CI #1289 (`bfd1202`, run 7, first measurement at the final contract state); **enforced at CI #1290** (`63d6705`), which reproduced all 71 names with no header change. Superseded measurements: #1281 (cancelled at 90 min), #1282 (264), #1283 (235), #1284 (208), #1285 (209), #1287 (83), #1288 (72). |
| **Scope**      | `data/priority_queue.h`, verified in place (Shape A, no driver), all 41 non-macro functions contracted including the new `pq_cmp_`. `DEFINE_PRIORITY_QUEUE`'s typed wrappers PARKED, per the borrow.h/diag.h precedent. Model Typed+Cast, `-wp-split`, 120 s, Alt-Ergo/Z3/CVC5. |
| **Category**   | Formal verification completeness |
| **Enforcement**| **ENFORCED at CI #1290.** Pinned `4513 / 4584`; 71 unproved by set equality: 43 inherited from memory.h (read from the memory job's own list at run time so the two pins cannot drift apart silently), 22 result(bool, Error), 6 own. Failed never pinnable. Terminal `exit 1` present — the bitset lesson. |

**Description**: `priority_queue.h`'s central operation calls a
**caller-supplied function pointer** — the comparator — from inside its loops.
Other verified modules call function pointers too (region.h's cleanup hooks,
option's and result's combinators), but there the call is the last thing the
function does; here everything after it depends on state the call could have
changed. That single fact shaped the whole arc. WP has no contract
for an unknown callee, so every statement after `pq->cmp(...)` began with
`pq->data`, `pq->len` and `pq->elem_size` unknown, and every later obligation
in `pq_sift_up_` and `pq_sift_down_` failed whatever its own merit. Until
commit 5 the comparator call was an **information horizon**, and it hid
everything behind it.

What is claimed is the **structural invariant** `pq_wf`: the struct is valid,
`elem_size` and `capacity` are positive and their product does not wrap,
`capacity` is bounded so child indices cannot wrap (F-WRAP), `len <=
capacity`, the buffer is valid and disjoint from the struct, and the
comparator is one of compare.h's 24 built-ins with an element wide enough for
it. Every mutator preserves it; every query requires it. **Heap order is not
claimed** and cannot be: the built-in comparators ensure only `-1 <= \result <=
1`, nothing about which way. The runtime suite remains the evidence for
ordering. This is the VERIFY-020 F4 shape — a specification-strength ceiling
declared before the run.

4513 of 4584 obligations discharged. Six own residuals: one is Frama-C's
unimplemented `\valid_function`; five belong to memory.h (see "What remains").

### The run history, and what each run taught

| Run | CI | Own | Predicted | Verdict |
|-----|----|-----|-----------|---------|
| 0 | #1281 | — | — | cancelled at 90 min: budget sized from lifetime.h, not the memory.h substrate |
| 1 | #1282 | 199 | > 71 | confirmed on count, refuted on reason (annotations, not heap complexity) |
| 2 | #1283 | 170 | < 60 | **refuted** |
| 3 | #1284 | 143 | < 90 | **refuted** |
| 4 | #1285 | 144 | < 130 | **refuted**; heapify regressed a second time |
| 5 | #1287 | 18 | classes, not a count | 12 outside the classes, each traced |
| 6 | #1288 | 7 | classes | 1 outside, traced |
| 7 | #1289 | 6 | classes | **confirmed**: comparator 1, separation 5, else 0 |
| 8 | #1290 | 6 | set equality | **enforced**, name-identical to #1289 |

Three consecutive count predictions were refuted, and each for the same
reason: a cause was inferred from a symptom and not traced in the log.

- Run 2 attributed ~85 residuals to `ptr_elem` returning an opaque pointer.
  That was partly right — it produced VERIFY-023 — but most of the count was
  the comparator cascade.
- Run 3 counted the "cmp ceiling" at 41 by grepping for goals with
  function-pointer-ish *names*. The true cascade — everything after the first
  `pq->cmp` call — was ~99.
- Run 4 added `\separated(pq, buffer)` to `pq_wf`, predicting it would close
  push_result's postconditions. It closed nothing. The real cause was the
  **uncontracted result constructors** at push_result's return statements,
  which WP took as assigning everything.

Run 5 stopped predicting counts. The job now sorts every own residual into
named classes and prints a per-function tally; the only prediction that can
fail is "everything outside the named classes is 0". It failed once more (run
6, one goal, `pq_pop` lacking the readability requires its callee had gained),
then held.

### The comparator: two closures considered, the verified one chosen

**(a) A trusted axiom** — wrap the call, contract the wrapper `assigns
\nothing`, leave its own goal unprovable as the permanent marker. This is
diag.h's stdio-axiom shape (VERIFY-017). It covers any comparator and proves
nothing about it.

**(b) A verified configuration** — a `calls` clause naming compare.h's 24
built-in comparators, and a requires that `pq->cmp` is one of them. All 24
are proved (VERIFY-005, 208/208, zero residuals) with `assigns \nothing`,
byte-form validity requires and a bounded result, so the wrapper's frame and
termination follow from **their** contracts. This is lifetime.h's "verified at
level 4 only" shape: the claim narrows to the built-in family, and nothing is
trusted. A caller-supplied comparator compiles and runs exactly as before; it
is outside the verified configuration.

(b) was chosen, on the observation that the built-ins' requires are already in
byte form, so no Typed+Cast bridging is needed at the wrapper. The wrapper
`pq_cmp_` is the single point at which the queue calls the comparator; the
three call sites become `pq_cmp_(pq, a, b)`, identical machine code, MC/DC
denominator unchanged at 82. The one goal it carries is
`\valid_function(pq->cmp)`, unimplemented in Frama-C 29 — the same single goal
every function-pointer call in the project carries. **This closed ~99
residuals at once**, and the `calls` clause was confirmed to take by the
absence of `assigns` and `terminates` goals on the wrapper at #1287.

### Findings

**F-WRAP** — `pq_left_child_` and `pq_right_child_` compute `2*i+1` and `2*i+2`
in `usize` with no guard. For `elem_size == 1` a queue with `capacity > 2^63 - 1`
makes `2*idx+1` wrap, and `left < pq->len` then reads the wrong slot silently.
No such buffer exists, so this is not a runtime bug; but the code is unguarded
and WP found it as two unprovable child-index requires at CI #1287. `pq_wf_buf`
and `pq_init` now bound `capacity <= (USIZE_MAX - 2) / 2` — the MCDC-003
arena-cap shape. The proof states the bound rather than assuming it.

**Result constructors assign everything.** `result__Bool_Error_ok` / `_err`,
macro-instantiated at the top of the header, had no `assigns`. Every
postcondition of a function that returns one of them died at the `return`.
Fixed by interposition under `__FRAMAC__` using vec_verify.h's exact shapes:
type first, contracted prototypes, then the definitions. Shipped path
byte-identical. Six goals, misattributed in run 4 to struct/buffer aliasing.

**Call-site frames are the callee's union.** A wrapper cannot claim `assigns
\nothing` on a reject path if it delegates to a function whose default frame
is the union over its behaviours; WP checks the call against that union. Seven
goals across `pq_push`, `pq_pop`, `pq_remove_at`. The wrappers now carry the
union as their function-level frame — true but weaker, deque's run-1 shape,
recorded as such.

**`pq_wf` grew and heapify lagged it, twice.** Commits 3 and 4 each added a
conjunct to `pq_wf`; `pq_heapify` — which sets `len` and so cannot require
`len <= capacity` — carried a hand-copied subset of the invariant in its
requires, and that copy was not updated either time. Regressed 0 → 4 → 5.
Commit 5 split `pq_wf_buf` out so heapify requires the shared predicate. One
definition, two names.

**Three small omissions in the contracts**, each caught by the class tally
within a run of being made: `pop_raw` read `pq->len` before requiring readability;
`pq_peek`'s `none` branch with `out == NULL` still ran `peek_raw`'s nonempty
behaviour without `pq_wf`; `pq_pop` did not carry the readability requires
its callee had gained.

### What remains: the six, and whose they are

| # | Goal | Cause | Owner |
|---|------|-------|-------|
| 1 | `pq_cmp_assert_rte_function_pointer` | `\valid_function` unimplemented in Frama-C 29 | tool |
| 5 | `mem_copy_requires_3` ×4 (push_result, pop_raw, remove_at_result, peek), `mem_swap_requires_2` ×1 | memory.h states `regions_overlap` by **pointer ordering** — `a < b + size && b < a + size`. Under WP, ordering two pointers into different bases is meaningless, so `\separated(elem, buffer)`, which `pq_wf` and the contracts give, cannot discharge it. | **memory.h** — a contract-shape finding of the VERIFY-023 kind. State the predicate with `\separated`. **VERIFY-024 candidate**, its own arc, its own ratchet. |

`remove_at_result`'s instance is the same-base case and additionally needs
multiplication (slot `i` vs slot `len`, lifted through `elem_size`); it is
listed with the other four because the predicate shape is the blocker first.

### Prediction scorecard

| | Registered | Outcome |
|---|---|---|
| ARM A: Typed+Cast takes effect | 0 plain `typed_` goals | confirmed, every run |
| ARM B: memory.h's 43 inherited verbatim, none extra | 43/43, 0 extra | confirmed, every run; the regex bug of run 1 (`result__Bool_Error` vs `result_Bool_Error`) was in the job, not the proof |
| ARM C: fresh result arm | 22, get_ok/get_err 1/1 | confirmed, every run; 22 = vec's pre-F4 count, second reproduction |
| ARM D runs 2–4: own count | < 60, < 90, < 130 | **refuted ×3** — see above |
| ARM D runs 5–7: named classes | comparator 1, separation 5, else 0 | refuted at 5 and 6 by traced omissions; **confirmed at 7** |
| ARM E: arms sum to total | equality | confirmed, every run |
| Comparator `calls` clause takes | 1 goal on `pq_cmp_`, no `assigns`/`terminates` | confirmed — the job's bin said 0 because its regex wanted a double underscore where WP emits one; third instance of the prefix/underscore class in this workflow, fixed |
| VERIFY-023 timeout-flip risk | "unlikely" | did not occur |

### Method notes recorded for the next arc

Every wrong count in this arc came from naming a cause that fit the symptom
without tracing it in the log — the same error VERIFY-023 found in the
VERIFY-009/-010 records, made here in real time. Every right call — the `calls` clause, the
constructor interposition, F-WRAP — came from reading a contract or a goal
name and checking what it actually said. Three additional rules now apply to
this header and should apply to the next: ACSL predicates must be defined
before use and that order is checked in the preprocessed unit before commit
(CI #1286 aborted on it); a `pq_wf` conjunct is never added without checking
every function that carries a subset of `pq_wf` by hand (there are now none);
and a residual bin's regex is dry-run against a collision-prone name list
before it ships.

### Cross-references

- VERIFY-023 — the ptr.h ensures this arc caused, and the 24 misattributions it
  exposed.
- MCDC-014 — the coverage arc that preceded verification: 62/78 → 81/82, and
  the PQ-A finding.
- VERIFY-005 — the 24 comparators the `calls` clause names; this is the first
  module to compose over a zero-residual header by enumeration.
- VERIFY-017 — the trusted-axiom shape considered and not chosen.
- VERIFY-021 — the "verified configuration" shape that was chosen.
- VERIFY-024 (candidate) — `regions_overlap` restated with `\separated`.

## VERIFY-025: Contracts Move Into the Macro Bodies — a Two-Letter Preprocessor Flag Removes a Premise That Three Documents Called a Property of the Language (slice, result, option, borrow, vec, deque)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-025 |
| **Date**       | 2026-09-16 |
| **Baseline commit** | Branch `exp/cc-slice`, seven commits `eeb387f`..`b01211a` on top of `e973e13`. Local runs only at the time of writing; no CI pin yet (see *Enforcement*). |
| **Scope**      | Every macro-generated function in six families: `DEFINE_SLICE` (14), `DEFINE_RESULT_*` (17), `DEFINE_OPTION_*` (16), `DEFINE_BORROWED_SLICE` (9), `IMPL_VEC_*` + lifetime helpers (37), `IMPL_DEQUE_*` + lifetime helper (24). 117 functions. |
| **Category**   | Verification mechanism; design-decision correction; contract relocation (driver prototypes → macro bodies) |
| **Enforcement**| **Not yet.** Reproduced locally on Frama-C 29.0 / Alt-Ergo 2.6.3 / Why3 1.7.2 only. The existing driver-based CI jobs are unaffected (they run under `-C`, where macro-body annotations are invisible, so the prototype contracts in `vmacros/vdrivers/*_verify.h` still apply alone). A `-CC` job per family over the bare `*_cc_experiment.h` drivers is the follow-up. |

**Description.** Three documents stated that ACSL annotations cannot live
inside a `#define`: `design-decisions.md` §7 ("the C99 preprocessor strips
ACSL annotations inside `#define` before expansion ... would need either a
code generator or a switch to a different annotation toolchain"),
`vmacros.md` ("phase 3, before it expands macros in phase 4 ... a property
of the C standard's translation order, not a Frama-C limitation"), and
`verification.md`'s DEFINE_SLICE note ("contract specifications are
retained in the macro body as human-readable comments"). Every driver in
`vmacros/vdrivers/` was built on that premise: contracts written on
prototypes of the to-be-generated functions, then the macro instantiated
beneath them.

The premise is true of `cpp -C` — Frama-C's default first-pass
preprocessor command — and false of `cpp -CC`, which GCC documents as
preserving comments *including during macro expansion*. Demonstrated
before anything else was touched:

```
$ gcc -E -C  m.c | grep -A1 'vec_int {'   # contract gone
static inline int pop_int(struct vec_int *v) { ... }
$ gcc -E -CC m.c | grep -A1 'vec_int {'   # contract present, collapsed to one line
/*@ requires \valid(v); requires v->len > 0; ... */ static inline int pop_int(...)
```

Frama-C's `-cpp-extra-args` applies to that first pass, so
`-cpp-extra-args="-CC ..."` is the whole toolchain change. Frama-C 29
parses the collapsed comment, attaches it to the instantiated function,
and WP proves it. The rest of this entry is what happened when that was
done to every family that already had contracts somewhere.

### What moved

Method for families with a driver: run the existing driver unchanged
(local baseline), move the prototype contracts into the corresponding
`DEFINE_*`/`IMPL_*` macro, instantiate through a *bare* driver — struct +
functions, no prototypes, no contracts — under `-CC`, and diff the
residual sets by goal name. For `slice` (prose `Spec:` comments) and
`borrow` (nothing), the contracts were written; there is no baseline.

All runs: Frama-C 29.0, Alt-Ergo 2.6.3 only, `-wp-rte -wp-model
Typed+Cast`, `-DCANON_NO_REQUIRE -DNDEBUG`.

| Family | Fns | Flags | Baseline (driver, `-C`) | Migrated (bare, `-CC`) | Residual diff by name |
|--------|-----|-------|------------------------|------------------------|-----------------------|
| slice  | 14 | `-wp-timeout 30 -wp-fct <14>` | — (prose) | **115/115** — Qed 70, AE 25, term 10, unreach 10 | zero residuals |
| result | 17 | `-wp-timeout 20 -wp-split` | 185/215 — Qed 138, AE 15, TO 6, Unk 24 | **185/215**, identical breakdown | ∅ / ∅ |
| option | 16 | `-wp-timeout 20 -wp-split` | 189/223 — Qed 147, AE 20, TO 2, Unk 32 | **189/223**, identical breakdown | ∅ / ∅ |
| borrow | 9  | `-wp-timeout 30 -wp-split -wp-fct <9>` | — (none) | **145/145** — Qed 120, AE 15, term 5, unreach 5 | zero residuals |
| vec    | 37 | `-wp-timeout 3 -wp-split -wp-par 4` | 5269/5473, TO 204 | **5326/5538**, TO 212 | only-in-baseline ∅; only-in-migrated: 8 (see below) |
| deque  | 24 | `-wp-timeout 3 -wp-split -wp-par 4` | 1589/1668, TO 79 | **1592/1668**, TO 76 | only-in-migrated ∅; only-in-baseline: 3 (see below) |

**vec's 8.** All `result__Bool_Error_{map,map_err,and_then,or_else,eq}_assigns_*`
— the VERIFY-015 function-pointer class. The old driver contracted only 5 of
the 17 `result__Bool_Error` prototypes, so those five functions generated no
`assigns` goals in the baseline. The macro carries all 17, so the goals now
travel with the instantiation. No `vec_int_*` goal changed; the goal set
grew by 65 and the proved set by 57.

**deque's 3.** `push_back_ok_ensures_7_part4`, `push_back_unchecked_ensures_5`,
`try_push_back_ok_ensures_5_part4` timed out under the driver and discharge
under the macro — by Qed (1239 → 1242), not by the prover. With the
`deque_int_view`/`mut` predicates inlined (see F3), the simplifier sees the
conjuncts directly instead of behind a predicate name. Same goal set (1668),
same flags; the improvement is the contract's shape, not the timeout.

The local 3 s / single-prover baselines sit slightly above the CI pins
(vec 204 vs 196 at 120 s with three provers); the difference is the
`checked_mul_isize` and `arena_alloc` parts Z3/CVC5 pick up. Both sides of
each comparison used the same local flags, so the diffs are like-for-like.

### Findings

**F1 — a prose spec referenced a symbol its header cannot see.** The
retained `Spec:` for `slice_T_as_bytes` bounded `s.len * sizeof(type)` by
`CANON_USIZE_MAX`, defined in `core/primitives/limits.h`, which `slice.h`
does not include (`arena.h` does). Frama-C: *unbound logic variable
CANON_USIZE_MAX*. Unfindable while the spec was a comment; found the moment
it became a contract. Replaced by `SIZE_MAX` (in scope via `types.h` →
`<stdint.h>`). First concrete return on the conversion beyond the proof
itself.

**F2 — `//` inside an ACSL block is fatal under `-CC`.** The drivers annotate
residuals inline (`// ... fn-pointer residual`). Collapsed to one line, a
`//` comments out the remainder of the contract. All such notes were dropped
from the migrated contracts; the residuals are still residuals, just not
annotated at the clause.

**F3 — no token pasting, no macro-parameter substitution, inside a comment.**
This is the real constraint, and it is a style constraint, not a blocker.
Consequences, each hit at least once:
- Per-type predicates (`vec_int_view`, `vec_int_mut`, `vec_int_slice_view`,
  `deque_int_view`, `_mut`, `_ring`; `bytes_invariant` for slice) cannot be
  named as `vec_##type##_view` inside the comment. Inlined at every use site
  (35 in vec, all in deque). Verbose; provably equivalent; and, per deque's
  3, sometimes better for Qed.
- `sizeof(type)` does not substitute. `sizeof(*v->items)`, `sizeof(*s.ptr)`,
  `sizeof(*buffer)`, `sizeof(*d->buffer)` — a typed expression — does.
- Struct fields must be reached through a typed parameter (`v->len`,
  `s.ptr`), never through the generated type name.

**F4 — a pre-state bound with no typed lvalue.** `vec_T_alloc(usize
capacity)` has nothing typed in its pre-state, so the driver's
`assumes capacity > CANON_VEC_MAX_CAPACITY / sizeof(int)` cannot be written
in the macro. `sizeof(*\result.items)` is a compile-time constant regardless
of `\result`'s value, so the bound moves to the post-state as a flat
`ensures ... ==> \result.items == \null`. Same precision, no behaviours.
`arena_alloc` likewise.

**F5 — old drivers and new contracts must not meet under `-CC`.** A driver
with prototype contracts, instantiated under `-CC`, gives every function a
contract on its declaration *and* its definition. The bare
`*_cc_experiment.h` drivers exist so the migrated contracts are the only
ones present. Under `-C` the existing drivers are unaffected, which is why
the existing CI jobs did not move.

**F6 — composition through callee contracts holds across the macro
boundary.** `borrowed_slice_T_as_bytes` composes over `checked_mul`
(`core/primitives/checked.h`) and `borrowed_bytes_empty`; its `empty`/`view`
behaviour split discharged on the first run with nothing added to either
callee. This is the first macro-body contract proved through the substrate's
contracts rather than alongside them.

### What this changes in the record

`design-decisions.md` §7's "runtime-only by construction" paragraph is
corrected in place (dated note beneath it, original retained);
`vmacros.md`'s phase-3/phase-4 paragraph likewise; `decision-families.md`'s
verification-posture bullet no longer splits macro bodies from the roadmap;
`verification.md`'s DEFINE_SLICE note points here. The driver pattern is not
retired: it remains the right tool for per-instantiation proofs with richer
contracts and for the MC/DC pairing `vmacros.md` describes. What it no
longer is, is the *only* way to verify a macro family.

### What this does not claim

- No CI enforcement yet. The numbers above are local, single-prover, short
  timeout. The follow-up is one `-CC` job per family over the bare drivers,
  pinned at the CI flags (three provers, 120 s), and only then does the
  master table move.
- Nothing about the remaining macro families (`stack`, `queue`, the `hashmap`
  and `priority_queue` typed wrappers, `stringbuf`, `dynvec`, `smallvec`).
  They have no driver and no prose specs; each is a fresh contract-writing
  arc with its own entry.
- Nothing about functional composition for clients beyond what the
  contracts already state. The contracts that moved are the same contracts;
  they now travel with every `DEFINE_X(T)` instead of one `T` in one driver.
  That is the compositional gain, and it is exactly as large as the
  contracts are.

### CI addendum — enforcement (2026-09-21)

| Field | Value |
|-------|-------|
| **Workflow** | `.github/workflows/frama-c-cc.yml` — one job per family, bare `*_cc_experiment.h` driver, driver-job flags plus `-CC`, family's home memory model |
| **Report-only runs** | CC #1 (41697e7), CC #2 (ae8ddce), CC #3 (manual dispatch) — set-identical by goal name on all six families |
| **Enforced from** | the run after CC #3; gates (0)–(3) as every other enforced unit |
| **Positive control** | each job counts ACSL blocks in the `gcc -E -CC` output of the bare driver before running WP (31 / 8 / 8 / 92 / 169 / 37) and stops if zero — the instrument is checked before the measurement, on the arena-32 principle |
| **Baseline** | the driver job's roll-call, read from the main workflow at run time (union of every `*CHECKS=(` / `INHERITED=(` array in the job); nothing copied |

**What three CI runs found, against the local runs in the table above.**

| family | goals (driver → CC) | residuals | vs driver pin | vs local |
|--------|---------------------|-----------|---------------|----------|
| slice  | 394 → 587 | 15 | identical by name | as reported (the 15 are substrate/libc; the 14 slice functions themselves have none) |
| result | 215 → 215 | 30 | identical | as reported |
| option | 223 → 223 | 34 | identical | as reported |
| borrow | 2458 → 2796 | 19 | identical (17 inherited + 2 own) | as reported |
| vec    | 5501 → 5566 | 192 | +8, 0 closed | as reported — the eight named below |
| deque  | 1668 → 1668 | 67 | identical | **not** as reported: see below |

vec's eight, by name: `result_Bool_Error_and_then_assigns_part1`,
`eq_assigns_part2`, `eq_assigns_part4`, `map_assigns_exit_part2`,
`map_assigns_normal_part2`, `map_err_assigns_exit_part2`,
`map_err_assigns_normal_part3`, `or_else_assigns_part2`. All are the
VERIFY-015 function-pointer class on functions the old driver never
contracted; the macro carries every contract, so the goals now travel with
the instantiation. Nothing moved for any reason but contract surface.

**Correction 1 — deque's "three closures" were a timeout artifact.** The
table above reports deque 1589 → 1592 proved under the macro contracts,
attributed to the inlined `deque_int_view`/`mut` predicates letting Qed see
the conjuncts. That was at `-wp-timeout 3`, Alt-Ergo only. At CI's 120 s
with three provers, the enforced *driver* job already proves 1601/1668, and
the `-CC` job proves the same 1601/1668 with the same 67 names. The three
goals the local run credited to the contract's shape were goals the driver
also proves given time. The F3 observation about predicate inlining may still
be true of Qed's behaviour; it is not evidenced by these three goals, and the
sentence claiming so is withdrawn.

**Correction 2 — the 14 `loop` goals did not move, and could not have.**
This record established that ACSL inside macro bodies survives `-CC`; it
migrated *function contracts*. No `loop invariant`, `loop assigns` or
`loop variant` was written into any macro body (`grep 'loop invariant'
data/vec/vec_impl.h` is empty). So `vec_int_fill`'s loop goals and their
siblings are exactly where they were: residual, now with no construction
argument covering them. The class-(d) argument for macro-body loops is
retired as an *argument* — its premise is false — but the goals it covered
are not retired; they are ordinary specification debt (class (c): an
annotation nobody has written yet) until someone writes the invariants under
`-CC` and they either prove or join a different class. The paper's v4.2–v4.4
text saying enforcement makes "the 14 loop obligations leave the residue" is
wrong and is corrected in the next paper pass. Retiring the argument and
retiring the goals are two different events; this record produced the first.

**Decision — alongside, not instead.** The `-CC` jobs run beside the driver
jobs; neither replaces the other yet. The driver jobs remain the enforced home
units of Table 1 (numbers stay at CI #1304 for the paper), and the `-CC` jobs
are a seventh perturbation row: same code, same flags, same model, different
contract surface, residual sets identical on five families and +8 on the
sixth. Switching the home units to the macro-body contracts (and deleting the
prototype contracts from `*_verify.h`) is a Table-1-wide re-base, deferred
until after the paper is submitted.

**One finding about the record's own method.** Local runs at 3 s / one prover
were used to characterise the migration. Two of the six families produced
numbers that CI at 120 s / three provers did not reproduce (deque's three
"closures"; vec's local TO 204 vs CI 184). The residual *names* that matter
were right in both cases; the counts were the artifact. Next time: the local
baseline should be run at the CI configuration, or the record should say it
is a fast approximation and defer the numbers to CI.

### Follow-ups

- CI: `-CC` jobs for `slice`, `result`, `option`, `borrow`, `vec`, `deque`
  over `vmacros/vdrivers/*_cc_experiment.h` (rename to `*_macro_verify.h`
  when promoted). Pin, then ratchet.
- Retire the prototype contracts from `vec_verify.h`, `deque_verify.h`,
  `option_verify.h`, `result_verify.h` once the `-CC` jobs are enforced, or
  keep them as the MC/DC cover TUs with contracts removed.
- Decide whether `-CC` becomes the project-wide default in the WP command
  (it is a superset of `-C`; the only cost is F2).

## VERIFY-023: Four Address Helpers State Their Result, and Twenty-Four Residuals in Five Modules Turn Out to Have Been Misattributed (ptr.h)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-023 |
| **Date**       | 2026-09-06 |
| **Baseline commit** | Canon-C CI #1284 (measurement), CI #1285 (ratchet, `b79a51f`). |
| **Scope**      | `core/primitives/ptr.h`: `ptr_offset`, `ptr_offset_const`, `ptr_elem`, `ptr_elem_const`. Each gains one `ensures` in its nonnull behaviour stating the address it returns. Nothing else in the header changes; the shipped token stream is byte-identical after comment stripping. |
| **Category**   | Substrate contract strengthening; residual reclassification |
| **Enforcement**| Eight enforced jobs ratcheted in one commit (`b79a51f`): ptr, memory, arena, arena-32, pool, region, vec, bitset. Residual lists rewritten by name; arena-32's embedded baseline rewritten to the new 83. |

**Description**: the four helpers that every container uses to address an
element returned a pointer about which their contracts said nothing. WP
therefore treated the result of every `ptr_elem` call as an unconstrained
pointer, and every downstream obligation about that address — validity for
`mem_copy`, separation for `mem_swap`, the frame of a write through it —
failed regardless of its own merit. The gap was found from the outside:
`priority_queue.h`'s second WP run (VERIFY-022, CI #1283) left 170 own
residuals, and reading them showed ~85 landing on element addresses.

The fix is four lines, one per helper: `ensures (u8*)\result == (u8*)base +
index * elem_size;` and its three siblings. The pre-registration predicted +4
goals per translation unit and an unchanged residual set. Both were wrong in
the good direction.

### What moved, and what was predicted

| Job | Pin before | Pin after | Residuals | Predicted |
|-----|-----------|-----------|-----------|-----------|
| ptr | 1943/1953 | 1949/1959 | 10 → 10 | unchanged ✓ |
| memory | 2823/2866 | 2829/2872 | 43 → 43 | unchanged ✓ |
| arena | 3430/3521 | 3444/3527 | 91 → **83** | unchanged ✗ |
| arena-32 | = arena | 3444/3527 | 83 = 83 | set equality ✓ |
| pool | 3884/4003 | 3914/4009 | 119 → **95** | 2 named ✗ (24 closed) |
| region | 3578/3692 | 3592/3698 | 114 → **106** | unchanged ✗ |
| vec | 5271/5467 | 5285/5473 | 196 → **188** | unchanged ✗ |
| bitset | 4839/5002 | 4845/5008 | 163 → 163 | unchanged ✓ |

The goal delta was **+6**, not +4: `ptr_offset` and `ptr_offset_const` have a
null/nonnull branch, so `-wp-split` fragments their new `ensures` in two;
`ptr_elem` and `ptr_elem_const` have none. Right mechanism, wrong count.

Twenty-four distinct goals closed, and every one had been pinned as a residual
with a written explanation. Arena's eight: four `arena_alloc*_fits_ensures_part5`
fragments and all four `arena_free_bytes` / `arena_free_cbytes`
`call_bytes_from_requires` goals — inherited verbatim by arena-32, pool,
region and vec. Pool's own sixteen: every `*_call_ptr_elem*_requires`, every
`as_bytes` / `as_cbytes` / `reserved_bytes` / `reserved_cbytes` call-site pair,
and `pool_reset_secure`'s three. Of the 24 the pre-registration named 2
(pool's `in_bounds_ensures_part4` pair); it also named 8 (arena's
`ptr_span_requires`) that did **not** close.

**Zero goals moved in the bad direction.** The named risk — that a new
callee `ensures` is a hypothesis in every caller and could push a 90-second
proof over the 120-second timeout — did not materialise. Two arena
`ptr_span_requires` goals flipped Timeout→Unknown, which the house rule pools.

### The finding: the classifications were wrong

VERIFY-009 recorded the `fits_ensures` fragments as "fits/does_not_fit
arithmetic chain" and the `free_bytes` four as "free_bytes helpers". VERIFY-010
recorded pool's `ptr_elem_requires` goals as call-site obligations. Those
entries described a **symptom** and attributed it to the wrong **cause**. The
goals were never about arithmetic or about the call sites; they were the
callee's opaque return value, and they closed the moment the callee stated it.

Nothing in the pipeline could have told anyone. A misattributed residual and a
correctly attributed one pin identically, print identically, and are
name-stable identically. The record was wrong for months and stayed wrong
because a wrong explanation is as stable as a right one. It surfaced only
because a fourth module hit the same wall hard enough that someone read
`ptr_elem`'s contract. Reading notes dated 2026-09-07 are appended to
VERIFY-009, -010, -011 and -018 above (and, for the +6 with no set change, to
VERIFY-006 and -008; for the gate, to -020); the original text is left
standing so the error is visible.

### Two collateral findings, both about the workflow rather than the code

**frama-c-bitset could not fail the build.** Every other accumulator-style WP
job ends with `if ENFORCE && FAILURES then exit 1`; the bitset job did not.
It had printed "ENFORCED" since CI #1259 and had never been able to turn red.
Found at CI #1284, when its step printed `VERIFY-020 ENFORCED FAIL: 1 check(s)`
and the job went green. An audit of all 28 gated steps in the workflow found it
to be the only one. The name-stability evidence from #1259 onward is real —
every roll-call printed `missing 0 / unpinned 0`, and #1284 holds set equality
at 163 — but **enforcement of bitset begins at CI #1285**, not #1259. VERIFY-020
carries a reading note.

**arena-32's embedded baseline was a second copy of arena's pin.** It reported
"8 newly proved at 32-bit, 0 width-specific" against its own stale 91-name
list while the 64-bit job had also moved to 83. Width invariance held; the
job's coupling-check comment had warned about exactly this drift, and the
baseline is now rewritten with the pin.

### Two mistakes caught before the commit

A rationale comment contained `sint8*/uint8*`; the `*/` closed the comment
and would have broken all 32 build-matrix cells. And the pin ratchet blindly
rewrote `3430 / 3521` everywhere it appeared — including arena-32's historical
measurement table for CI #1202/#1209/#1210. Restored; the paragraph is now
annotated "do not rewrite history when ratcheting."

### What this says about the pedantic-assigns list

Every WP run prints `No 'assigns \result \from ...' specification for function
X returning pointer type`. Before this arc that list was noise. It is now
known to be a residual generator: `ptr_align_up`, `ptr_align_down`,
`ptr_retreat`, `bytes_at`, `mem_alloc`, `arena_alloc*`, `pool_get`,
`borrowed_ptr_get` are all still on it, and the `ptr_align_*` trio is
inherited by every job in the workflow. No prediction is made about whether
any of them close; it is a lead of the shape that just paid 24 goals, and it
costs one read of each contract to check.

## VERIFY-026: Pre-registration — Which of the Eight Pedantic-Assigns Functions Close Downstream Goals When Their Result Is Stated

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-026 |
| **Date**       | 2026-09-18 |
| **Status**     | PRE-REGISTERED — no contract touched; predictions committed before any change |
| **Baseline**   | CI #1298 (986d5ad) — pins identical to #1290 |
| **Scope**      | ptr.h, slice.h, memory.h, arena.h, pool.h, borrow.h |
| **Category**   | Prospective test of the VERIFY-023 mechanism (§5.7 of the paper: "a lead, not a prediction") |

**Why this record exists.** VERIFY-023 closed 24 pinned residuals by stating the
return value of four `ptr.h` address helpers, and found afterwards that all 24
had been misattributed for months. Its closing note listed eight more functions
on WP's `pedantic-assigns` list as "the same shape" and made no prediction. This
record makes the prediction, per function, before any contract is edited. The
commit hash of this record is the evidence that the predictions predate the
changes. The scoring record (VERIFY-027) compares, goal by goal.

**Method (read the contract, then the goal names — VERIFY-022's method note).**
For each function: (1) what its contract currently says about `\result`;
(2) which verified translation units call it; (3) which pinned residual names
at #1298 could depend on its result; (4) the mechanism by which an `ensures`
on the result would or would not discharge them. The prediction column is the
author's, signed by the commit.

### Evidence

| # | function | contract says about `\result` | called from verified TUs | pinned goals that could depend on its result | mechanism |
|---|----------|-------------------------------|--------------------------|----------------------------------------------|-----------|
| 1 | `ptr_align_up` | `null` behavior: `== \null`. `nonnull` behavior: **nothing** — the VERIFY-023 shape exactly | `arena.h` (`arena_alloc_aligned`) | `arena_alloc_aligned_fits_ensures{,_2,_3}_part{2,3,4}` (9) — the `\valid((u8*)\result + (0..size-1))` and offset ensures of the aligned allocator, whose result is the aligned address ptr_align_up returned | With `ensures \result == (void*)align_up((uintptr_t)p, align)` in the nonnull behavior, WP can relate the returned pointer to the arena base. Same mechanism as ptr_elem. |
| 2 | `ptr_align_down` | as above, nonnull behavior empty | **none** — no call site in any verified TU | none | Adding an ensures changes ptr_align_down's own goal count (+1 or +2 with split) and nothing downstream, because nothing downstream exists. |
| 3 | `ptr_retreat` | as above, nonnull behavior empty | **none** | none | Same as 2. |
| 4 | `bytes_at` | **already stated**: `in_bounds: \result == b.ptr + i`; `out_of_bounds: \result == \null` | (data layer via drivers) | none named at #1298 | Nothing to add on the result. It is on the pedantic list only because it lacks `assigns \result \from ...`, which is a frame clause, not a postcondition. Not the VERIFY-023 shape. |
| 5 | `mem_alloc` | `nonzero_size: \result == \null \|\| \fresh(\result, size)` | `vec_impl.h` | `mem_alloc_nonzero_size_ensures_part2`, `mem_alloc_assigns_normal_part2`, `mem_alloc_array_checked_nonoverflow_ensures_part3` (3) | These are class-(b) `\fresh` residuals (VERIFY-008 cat 1). `malloc`'s result has no statable address. No ensures on the address is possible; the three goals are a verifier feature gap, not an opaque return value. Not the VERIFY-023 shape. |
| 6 | `arena_alloc`, `arena_alloc_aligned`, `arena_alloc_zero`, `arena_alloc_aligned_zero` | `fits: \result != \null; \valid((u8*)\result + (0..size-1)); offset relations` — non-null and valid, but **not the address** | `pool.h` (`pool_init`, `pool_reset`), `region.h`, `vec` | own: `arena_alloc{,_aligned}_fits_ensures{,_2,_3}_part{2,3,4}` (18), `*_does_not_fit_ensures*_part5` (4), `*_zero_ensures_3_part1` (2), `*_zero_assigns_normal_part3` (2); downstream: `pool_init_call_arena_alloc_requires`, `pool_reset_call_arena_alloc_requires` (2) | An `ensures \result == (void*)(arena->base + \old(aligned_offset))` names the address. The own `fits_ensures_part*` goals are about `\valid` of that address and the offset arithmetic; whether they close depends on whether the difficulty was the opaque address or the `arena_can_fit` let-binding chain (VERIFY-009 block 5 — the block VERIFY-023 already showed was partly misattributed). The two pool call-site goals are `requires` at the call, i.e. `arena_can_fit` at pool's call site — those depend on pool's invariant, not on arena_alloc's result, and are **not** expected to move. |
| 7 | `pool_get` | `in_bounds: \result != \null` — non-null, **not the address** | **none** in verified TUs (API only; tests) | none at #1298 — pool_get has no pinned residual | Adding `ensures \result == pool_slot_address(pool, i)` adds own goals; nothing downstream to close. If the new ensures does not prove, it becomes a *new* residual — the one outcome in this table that would move a pin upward. |
| 8 | `borrowed_ptr_get` | **already stated**: `stored_result: \result == b->ptr` | none | none | Same as 4: pedantic list only. Nothing to add. |

### What the evidence already says, before any prediction

- Two of the eight (`bytes_at`, `borrowed_ptr_get`) are not the VERIFY-023
  shape at all: their result is stated. The pedantic-assigns list is a list of
  missing `\from` clauses, and VERIFY-023's closing note over-read it as a list
  of unstated results. That over-reading is itself a small instance of §5.7's
  mechanism (a symptom — the warning — taken for the cause).
- One (`mem_alloc`) is a class-(b) feature gap whose result *cannot* be stated.
- Three (`ptr_align_down`, `ptr_retreat`, `pool_get`) have no downstream
  dependents in any verified unit. Stating their result can add goals and
  cannot close any.
- Two (`ptr_align_up`, `arena_alloc*`) are the only candidates, with 9 and
  26 own goals respectively in range, plus the 2 pool call-site goals that
  the mechanism predicts will *not* move.

So the prediction this record can make is mostly "no", and that is what makes
it a test: a lead that VERIFY-023 called "the same shape that just paid 24
goals" resolves, on reading, to at most two functions and at most 35 goals,
with a mechanism-level reason each of the other six will not pay.

### Predictions (author — fill before committing)

| function | prediction | goals expected to close (by name or family) | goals expected to be added | confidence |
|----------|------------|---------------------------------------------|----------------------------|------------|
| `ptr_align_up` | close | the 8 `arena_alloc{,_aligned}_call_ptr_span_requires{,_2,_3,_4}` (VERIFY-009 block 4, class e) — the direct dependents; the 18 `fits_ensures*_part{2,3,4}` and 4 `does_not_fit_*_part5` predicted NOT to close (fits chain + cast bridging, VERIFY-009 block 5, class a) | +1 own (nonnull ensures; +2 if split), same +1/+2 in every including TU | M — if the 8 close, S-block 4 covers nothing and retires |
| `ptr_align_down` | no downstream change | — | +1 own | H |
| `ptr_retreat` | no downstream change | — | +1 own | H |
| `bytes_at` | not applicable — result already stated | — | 0 (no edit) | H |
| `mem_alloc` | not applicable — feature gap | — | 0 (no edit) | H |
| `arena_alloc*` | no downstream change; own: new ensures prove | none of the 26 existing own goals — they are the fits chain, not the address; `pool_init/pool_reset_call_arena_alloc_requires` are `arena_can_fit` at pool's call site and do not depend on the result | +4 own (one per variant), each predicted to prove *only if* row 1 lands first, since the address is `buffer + old offset + pad` and pad needs ptr_align_up stated | M |
| `pool_get` | no downstream change; own ensures proves | — | +1 own, predicted to prove: the address is `ptr_elem(base, i, object_size)` and ptr_elem's result has been stated since VERIFY-023 | M |
| `borrowed_ptr_get` | not applicable — result already stated | — | 0 (no edit) | H |

**Addendum, 2026-09-18, before any contract change (commit follows ac854e9).**
Reading ptr_span's contract refines row 1. Its four requires, in WP's goal
order, are \valid_read(to), \valid_read(from), same \base_addr, to >= from.
Stating ptr_align_up's result can discharge the third and fourth; it cannot
discharge the first, because when arena->offset == capacity the aligned
pointer is at or past one-past-the-end and \valid_read of it is genuinely
false — ptr_span only subtracts and its requires is stronger than its body
needs (a ptr_span contract question, out of scope here). Refined prediction:
of the 8 `*_call_ptr_span_requires*` goals, the four `_3`/`_4` close and the
four `_requires`/`_requires_2` do not. The 22 fits/does_not_fit goals:
unchanged prediction (do not close). The new ensures on ptr_align_up itself
may be residual under the cast model (int→pointer round-trip), which would be
a class-(c) trade: own goals opened to close call-site goals.

**Committed order of edits.** `ptr_align_up` first (one header, one commit,
cross-checked against the 9 arena goals over two hops); `arena_alloc*` second
(one commit, four variants); `pool_get`, `ptr_align_down`, `ptr_retreat` third
(one commit, expected to add goals and close none — the negative control).
`bytes_at`, `mem_alloc`, `borrowed_ptr_get` are not edited.

**Scoring.** VERIFY-027 reports, per row: predicted vs observed, by goal name,
with the CI run number; and states whether the "at most two functions" reading
of the lead held. Any goal that closes and is *not* in this table is a
misattribution by this record and is reported as one.

## VERIFY-027: Scoring VERIFY-026 Row 1 — ptr_align_up States Its Result, Thirteen Arena Goals Close, and the Pre-registration Misattributed Nine of Them

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-027 |
| **Date**       | 2026-09-19 |
| **Status**     | SCORED — pins ratcheted at CI #1300 (4ef4270), green |
| **Prediction** | VERIFY-026, commit ac854e9 (2026-09-18 11:33) and addendum e90ee71 (11:35) |
| **Change**     | fb3dc58 (11:36): three `ensures` in `ptr_align_up`'s `nonnull` behavior; contract only, token stream unchanged |
| **Measured at**| CI #1299 (fb3dc58); ratcheted and re-confirmed at CI #1300 |
| **Scope**      | Row 1 of eight. Rows 2–3 (arena_alloc*, pool_get) pending; rows for ptr_align_down, ptr_retreat pending as negative controls; bytes_at, mem_alloc, borrowed_ptr_get not edited |

**Why this record matters more than its 13 goals.** It is the campaign's
first *prospective* misattribution measurement. VERIFY-023 found 24
misattributed obligations after the fact; VERIFY-026 committed a prediction
by goal name before a contract was touched, and this record scores it. The
prediction was right about the mechanism it named and wrong about the one
it ruled out. Both halves are reported.

### The change, exactly

```
    behavior nonnull:
        assumes p != \null;
        ensures  same_base: \base_addr((char*)\result) == \base_addr((char*)p);
        ensures  not_below: (char*)\result >= (char*)p;
        ensures  bounded:   (char*)\result - (char*)p <= (align - 1);
```

Pointer-level, phrased as what `ptr_span` needs at the call site rather than
as the integer alignment formula, whose bitwise form is memory.h's class-(a)
shape and would prove nothing downstream.

### Scoring, clause by clause

| # | prediction (VERIFY-026 + addendum) | observed at #1299 | verdict |
|---|------------------------------------|-------------------|---------|
| 1 | `arena_alloc{,_aligned}_call_ptr_span_requires_{3,4}` close (same base, ordering) | all four closed | **CONFIRMED** |
| 2 | `_requires`, `_requires_2` (the two `\valid_read`s) do not close — genuinely false at capacity | all four still residual | **CONFIRMED** |
| 3 | the 18 `fits_ensures*_part{2,3,4}` do not close — "fits chain + cast bridging, VERIFY-009 block 5, class (a)" | **9 of 18 closed**: all six `part2` (both variants × three ensures) and `arena_alloc`'s three `part3`. The nine `part4` and `arena_alloc_aligned`'s three `part3` remain | **REFUTED, partially** |
| 4 | the 4 `does_not_fit_*_part5` do not close | all four residual | CONFIRMED |
| 5 | +1 own goal on ptr_align_up (+2 if split), per including TU | +6 goals per TU (3 ensures × 2 parts); the three `part1` prove, the three `part2` are residual | CONFIRMED, count under-estimated |
| 6 | the new ensures may themselves be residual under the cast model (addendum) | they are: `same_base_part2`, `not_below_part2`, `bounded_part2` time out in every TU | CONFIRMED |
| 7 | pool's `pool_init/pool_reset_call_arena_alloc_requires` do not move (they are `arena_can_fit` at pool's call site) | both still residual; pool's own 12 unchanged | CONFIRMED |
| 8 | (implicit, §3 of the paper) closures and additions propagate at unit gain over the include DAG | see propagation table | CONFIRMED |

Net: **13 closed for 3 opened**, one hop deeper than the change.

### The misattribution — by this record's predecessor

VERIFY-026 wrote, of the 18 `fits_ensures` goals: "predicted NOT to close
(fits chain + cast bridging, VERIFY-009 block 5, class a)". Nine of them
closed the moment `ptr_align_up`'s result was stated. Those nine were
therefore never about the `arena_can_fit` let-binding chain; they were the
opaque aligned pointer feeding `pad`, i.e. the same VERIFY-023 shape the
record had correctly identified for the `ptr_span` goals and then explicitly
excluded for these. In §3's terms: nine obligations were covered by an
argument (block 5, arithmetic) that did not apply to them; the applicable
argument was block 4 (cascade from an unstated result).

This is the third misattribution finding in the campaign and the first
made prospectively: the prediction is in `ac854e9`, the refutation in
CI #1299, and nothing in between could have been adjusted. It also refines
VERIFY-023's lesson. The cause there was "inferred from a symptom, not traced
to the goal". Here the cause *was* traced — to the body of `arena_alloc`,
where `aligned_ptr` is visibly not the result — and the trace still stopped
one step short: `pad` is computed *from* `aligned_ptr`, and `offset += pad`
is what the `fits` postconditions constrain. A trace that stops at the first
plausible cause is the same error as no trace, with better paperwork.

What the split says. The `part2` halves closed on **both** variants and the
`part3` halves only on plain `arena_alloc`: the default-alignment case is
now determined end to end, the user-supplied `alignment` case is not, and
`part4` on both is the chain proper. So VERIFY-009 block 5 covers, after
this record, exactly the twelve goals that survive — nine `part4` and three
aligned `part3` — and the block's own text should say so. [Do this edit in
VERIFY-009, not here; this record reports, it does not re-argue.]

### Propagation — the eleventh edge measurement, and the third at unit gain

| unit | includes arena.h | goals | residuals | closed | new | own moved |
|------|------------------|-------|-----------|--------|-----|-----------|
| ptr.h | — | 1959 → 1965 | 10 → 13 | 0 | 3 (own) | — |
| memory.h | no | 2872 → 2878 | 43 → 46 | 0 | 3 | 0 |
| arena.h | own | 3527 → 3533 | 83 → 73 | 13 (own) | 3 | −13 |
| arena.h @ 32-bit | own | 3527 → 3533 | 83 → 73 | 13 | 3 | set-identical to 64-bit |
| pool.h | yes | 4009 → 4015 | 95 → 85 | 13 | 3 | 0 (12 unchanged) |
| region.h | yes | 3698 → 3704 | 106 → 96 | 13 | 3 | 0 (23 unchanged) |
| vec | yes | 5473 → 5479 | 188 → 178 | 13 | 3 | 0 (53 own, 20 fresh unchanged) |
| bitset | no | 5008 → 5014 | 163 → 166 | 0 | 3 | 0 |
| priority_queue.h | no | 4584 → 4590 | 71 → 74 | 0 | 3 | 0 |

Every unit whose closure contains `ptr.h` gained exactly the three new
names; every unit whose closure contains `arena.h` lost exactly arena's
thirteen; six units that include neither (slice, borrow, deque, diag,
option, result, compare, error, checked, bits, lifetime) did not run red.
The six goals added to each TU are the three ensures split in two. Width
invariance held: the 32-bit set is byte-identical to the 64-bit set after
the change, as before it.

Two gate observations for §7 of the paper. priority_queue's inherited
baseline is derived from memory's `CHECKS` at run time, so the ratchet
touched only memory's array and pq's parse-count guard (43 → 46); the
job moved by itself. arena-32's embedded duplicate had to be edited by hand
(−13, +3), which is the gap §7 already reports and this ratchet exercised
again.

### A finding about the lead itself

WP still prints `[wp:pedantic-assigns] No 'assigns \result \from ...'
specification for function 'ptr_align_up'` after the three `ensures` were
added. Stating the result does not silence the warning, because the warning
is about a `\from` frame clause, not about a postcondition. VERIFY-023's
closing note read the pedantic list as "eight functions whose result is
unstated"; two of the eight (`bytes_at`, `borrowed_ptr_get`) already stated
their result, and this run confirms the two things are independent. The
list was a lead for a different property than the one it was taken for.
It still led to 13 goals; the mechanism that closed them was reading the
contract and the body, which is VERIFY-022's method note, again.

### The trade, stated

Three own goals opened on `ptr_align_up` (class (c): a specification whose
truth WP cannot establish through the int→pointer round-trip under
Typed+Cast) to close thirteen call-site and postcondition goals in arena and
its dependents. The three are true — `align_up` returns the smallest
multiple of `align` at or above its argument, and the pointer round-trip
preserves the object — and are argued under the existing class-(a)/(c)
arithmetic argument in VERIFY-009, not a new one. Δ*A* = 0 for this change.

**Coverage** (no new manual proof argument) for the three: They are instances of
memory.h category 2 (bitwise alignment through casts, VERIFY-008) at a new
site; that block's argument covers them. Coverage recorded here so the next
audit can check it.

### What this does to the paper

- §5.7 gains its first prospective row: 9 of 18 predicted-not-to-close
  closed; predicted-to-close closed 4/4; predicted-not-to-close on the
  `\valid_read` pair confirmed 4/4.
- Table 3 gains P10: "Upstream addition + closure, pre-registered — +3 in
  every TU including ptr.h, −13 in every TU including arena.h, unit gain,
  up to four hops, width-invariant."
- The S-block 4 (ptr_span cascade) now covers 4 obligations, not 8; block 5
  covers 12, not the 21 it was written for. Neither retires yet.
- Numbers are at #1300 until row 2 lands.

### Row 2 — arena_alloc* state their result (scored 2026-09-19)

| Field | Value |
|-------|-------|
| **Change** | one `address:` ensures in the `fits` behavior of `arena_alloc` and `arena_alloc_aligned`; a guarded form on `arena_alloc_zero` and `arena_alloc_aligned_zero`. Post-state phrasing: `(u8*)\result == (u8*)arena->buffer + (arena->offset - size)`, which needs no `pad` term. Contract only. |
| **Measured at** | the run after 2d308ae (VERIFY-027 commit); ratcheted at the run after |
| **Baseline** | CI #1300, arena 73 |

**Observed.** arena.h 3533 → 3547 goals (+14: the two allocator ensures split
four ways each, the two wrapper ensures three ways each). Residuals 73 → 76:
**0 closed, 3 new** — `arena_alloc_fits_ensures_address_part4`,
`arena_alloc_aligned_fits_ensures_address_part3`,
`arena_alloc_aligned_fits_ensures_address_part4`. The other eleven parts
proved, including all six on the zero wrappers.

| prediction (VERIFY-026 row 2) | observed | verdict |
|---|---|---|
| no downstream change | pool, region, vec: 0 closed; pool's own 12 unchanged; `pool_init/pool_reset_call_arena_alloc_requires` still residual | **CONFIRMED** |
| +4 own, "each predicted to prove *only if* row 1 lands first" | +14 goals; 11 prove; 3 residual on the allocators' `address` clause; row 1 had landed | **REFUTED on own** |
| wrappers inherit via delegation | 6/6 prove | confirmed |
| width invariance | arena-32 set-identical at 76 | confirmed |

**Propagation.** +3 in every TU whose closure contains arena.h (pool 85→88,
region 96→99, vec 178→181); no change in any TU that does not. Unit gain,
addition only.

**What the split says, again.** The three residual parts are `part4` on both
variants and `part3` on the user-alignment variant — the identical fingerprint
VERIFY-027 read off the `fits_ensures` family after row 1. The address clause
is therefore blocked by the same obstacle as the existing `fits` postconditions,
the `arena_can_fit` chain (VERIFY-009 block 5, class a), and is covered by that
block. Δ*A* = 0. No new argument; coverage recorded here.

**Why nothing closed, stated so the next lead is read correctly.** VERIFY-023
closed 24 and row 1 closed 13 because a callee's *unstated result* was feeding
a *caller's call-site precondition* — the caller could not establish
`requires` it needed at the call. arena_alloc's callers (pool_init,
pool_reset, vec_int_arena_alloc) have no residual of that shape: their
residuals are `arena_can_fit` at the call site, which depends on the caller's
invariant, not on where the result points. VERIFY-026's own evidence table
said exactly this, and then predicted the new ensures would prove anyway,
which was a prediction about the *own* side that the evidence did not
support. Stating a result pays only where a dependent call site is waiting
for it; row 2 had none, and the record should have predicted zero on both
sides.

**Net for the pre-registration so far.** Two rows scored: row 1 closed 13 for
3 opened with one misattribution against the record; row 2 closed 0 for 3
opened with one direction wrong. Both predicted "no downstream change" on
pool's call-site goals correctly. The ptr_span `\valid_read` pair and the
`fits` chain are now the two named residual mechanisms in arena, and neither
is an unstated result.

**Next.** Row 3 (`pool_get`, `ptr_align_down`, `ptr_retreat`) is the negative
control: no dependents, predicted to add goals and close none. Given row 2,
the honest prediction for its own goals is "may be residual on any part that
touches the cast model", not "prove". That refinement is committed here,
before the edit.

### Row 3 — ptr_align_down, ptr_retreat, pool_get: the negative control (scored 2026-09-19)

| Field | Value |
|-------|-------|
| **Change** | b28cab4: three `ensures` in `ptr_align_down`'s `nonnull` behavior (mirror of row 1), one `address:` ensures on `ptr_retreat`, one `address:` ensures in `pool_get`'s `in_bounds` behavior. Contract only. |
| **Baseline** | the run after 5ab66ba (row 2 ratchet), ptr.h 13 / arena 76 / pool 88 |
| **Predicted** | VERIFY-026: no dependents in any verified unit, so 0 close anywhere; own goals added. Refined in the row-2 section: any part touching the cast model may be residual. |

**Observed.**

| function | goals added | proved | residual |
|---|---|---|---|
| `ptr_align_down` | 6 (3 ensures × 2 parts) | 3 (`part1`) | 3 (`part2`): `same_base`, `not_above`, `bounded` — the exact mirror of row 1 |
| `ptr_retreat` | 2 | 2 | 0 — the body is pointer arithmetic; same shape as ptr_elem in VERIFY-023 |
| `pool_get` | 4 | 4 | 0 — the address `buffer + base_mark + i·object_size` is derived through `ptr_offset` and `ptr_elem`, both of which have stated their result since VERIFY-023 |

**0 closed anywhere.** The three `ptr_align_down` residuals propagated +3 to
every TU whose closure contains ptr.h — ptr 13→16, memory 46→49, arena
76→79, arena-32 76→79 (set-identical), pool 88→91, region 99→102, vec
181→184, bitset 166→169, priority_queue 74→77. Pool's own residue stayed at
12 while its own four new goals all proved.

| prediction | verdict |
|---|---|
| no downstream closure (negative control) | **CONFIRMED**, nine of nine units |
| own goals: cast-model parts may be residual | **CONFIRMED** for align_down (3 of 6), and the proving half is the same `part1` half as align_up |
| pool_get's ensures proves via ptr_elem's stated result | **CONFIRMED** — a two-hop chain of stated results discharged with no lemma |
| ptr_retreat proves | confirmed |

The negative control is what makes rows 1 and 2 readable: the same kind of
edit, on functions with no dependents, moved nothing downstream. Row 1's 13
closures were therefore caused by the dependency, not by the edit.

**A prover-noise instance, logged.** The vec job's summary listed no CVC5
discharges in this run (Alt-Ergo 549, Z3 68, CVC5 absent) and the advisory
check printed "CVC5 does not appear in WP output". vec's residual set moved
by exactly the +3 every other unit moved by; which solver reached each proved
goal first is scheduling. This is P5 of the paper, observed on the largest
unit, and the reason open item 20 keeps that check advisory.

**Δ*A* = 0.** The three `ptr_align_down` residuals are covered by the same
argument as row 1's three (memory.h category 2, bitwise alignment through
casts, VERIFY-008). Coverage recorded here.

---

### VERIFY-026 closed: what the eight-function pre-registration measured

Three contract commits (fb3dc58, the row-2 commit, b28cab4), three scoring
sections, all predictions on record before the corresponding edit.

| row | functions | closed | opened | own predicted right? | downstream predicted right? |
|---|---|---|---|---|---|
| 1 | ptr_align_up | 13 (4 predicted, 9 not) | 3 | no — 9 `fits_ensures` misattributed to the chain | partly — the 4 `ptr_span` and the 4 `\valid_read` exactly as the addendum said |
| 2 | arena_alloc ×4 | 0 | 3 | no — "prove" predicted, 3 residual | yes — 0 |
| 3 | ptr_align_down, ptr_retreat, pool_get | 0 | 3 | yes | yes — 0 |
| — | bytes_at, mem_alloc, borrowed_ptr_get | not edited | — | (evidence table: not the VERIFY-023 shape) | — |

**Net across the campaign's residue:** 13 closed, 9 opened, all 9 opened
under existing arguments; Δ*A* = 0 over three ratchets.

**What the lead was worth.** VERIFY-023's closing note named eight functions
as "the same shape that just paid 24 goals". Reading them before editing
resolved that to two candidates and one of those paid. The pedantic-assigns
list, which the note took as a list of unstated results, is a list of missing
`\from` clauses; the warning still prints for every function whose result is
now stated. A lead is not a prediction, and this record's evidence table is
what turning one into the other looks like: six of eight rows predicted "no"
with a mechanism each, and all six held.

**What the record got wrong, in its own words.** Row 1 under-predicted (9
goals closed that it said would not — a misattribution of those goals to the
`arena_can_fit` chain). Row 2 over-predicted its own side (the address
ensures were said to prove; three parts did not, for the same chain). The
two errors are the same error from opposite directions: the boundary of
VERIFY-009 block 5 — which goals are the chain and which are the opaque
pointer — was drawn wrong in both predictions, and the runs redrew it. Block
5 now covers exactly: the nine `fits_ensures*_part4`, the three
aligned-variant `part3`, and the three `address` parts of row 2 — sixteen
goals (the nine are six `part4` and three `part3`; the four `does_not_fit`
`part5` were omitted from the earlier count), none of them about a callee's
result.

**Retirement check, requested by VERIFY-027 §5.5 and answered here.**
S-block 4 (ptr_span cascade, VERIFY-009 cat 2a, originally 8 goals) now
covers 4: `arena_alloc{,_aligned}_call_ptr_span_requires{,_2}`, the
`\valid_read` pair on each variant. It is not empty and is not retired; the
remaining four are a ptr_span contract question (its requires is stronger
than its body needs), which is out of scope for this record and belongs in
VERIFY-006.

**Three CI facts, for §7.** pq's inherited baseline moved by itself all
three times, because it is derived from memory's array at run time. arena-32's
duplicate was hand-edited all three times. And every one of the three runs
was red on exactly the units the include DAG says it should be, and green on
the rest — the DAG was checked nine times by accident.

**Numbers are now at the run that ratchets row 3.** The paper re-bases there.

### Next

Loop annotations in the macro bodies under `-CC` (the second half of
VERIFY-025), as a pre-registration: the 14 `loop` goals of the paper's §4.3,
predicted per goal before the annotations are written.

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

**Instrumentation-surface addendum (2026-07-24, Commit 9c):** GCC's
gimplifier converts ternaries of min/max shape — `(a > b) ? b : a`
and congeners — to branchless `MIN_EXPR`/`MAX_EXPR` at every
optimization level, including the -O0 coverage build. Such
expressions emit no branch and no condition records, so a
source-level refactor from `if`-clamp to min-shaped ternary silently
removes fully-covered outcomes from the MC/DC denominator (a
constant-arm ternary, by contrast, stays a COND_EXPR and remains
instrumented — the shape, not the operator, decides). Commit 9's
17.8 sweep decounted exactly three such sites (slice.h ×2, borrow.h
×1, priority_queue.h ×1 = −8 outcomes, −8 branches), each verified
fully covered pre-fold with missed-sets byte-identical across the
change. Standing rule: any future fold of this shape is a
measurement-surface change, not a coverage change; verify with a
pre/post per-line condition diff (gcov --conditions --json-format)
that only covered conditions left and the missed-set is stable, then
update the aggregate with a dated note.

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
`pool.h` (87.2% post-API-001), `stringbuf.h` (74.3%), and others. Each will need
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

- gcov MC/DC measurement now reads 50/54 (92.6%) on the post-Commit-9
  surface (see the 2026-07-24 note below). gcov instruments
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

**Measurement-surface reclassification (2026-07-24, Commit 9c):**
slice.h's condition total moved 58 → 54 (93.1% → 92.6%): the
Commit-9 clamp folds in bytes_slice (L470) and str_slice (L721) are
min-shaped and left instrumentation as branchless MIN_EXPR (see the
MCDC-001 addendum). Both removed conditions were fully covered (2/2).
The four documented outcomes of this record are byte-identical
pre/post (L469/519/720/760, each 3-of-4, per-line JSON diff): this
record's residual analysis stands as written, with the ceiling
arithmetic transposing to (54 − 4) / 54 ≈ 92.6% — all four
API-unreachable outcomes and their dispositions carry over unchanged
to the 50/54 surface.

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
| 5 | `arena_alloc`         | 356  | cond 0 false (`arena_debug_update_(arena)`)                 | Release-build macro no-op artifact |
| 6 | `arena_alloc_aligned` | 411  | cond 0 false (`arena_debug_update_(arena)`)                 | Release-build macro no-op artifact |

arena.h's gcov-measured MC/DC is 59/66 = 89.4% on the post-API-001
surface (2026-07-30 note below); it was 58/64 = 90.6% when the analysis
below was written. The closure of the
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
calls to `arena_debug_update_(arena)`. The macro is defined on line
176:

```c
#define arena_debug_update_(a) ((void)0)
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
   constants is 58/64 = 90.6%** on the surface analysed here (59/66 =
   89.4% post-API-001 — see the 2026-07-30 note below; the six outcomes
   below are unchanged). Reaching 90.6% represents 100% of
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

**Measurement-surface reclassification (2026-07-30, CI #1202, Commit 16/16b):**
arena.h's MC/DC surface moved **58/64 (90.6%) → 59/66 (89.4%)**. Cause:
API-001 added three `arena_*_cbytes` accessors. Two of the three guard only
with `require_msg()`, which the coverage build compiles out under
`CANON_NO_REQUIRE` and which therefore contributes no condition (the
MCDC-001 rule); `arena_free_cbytes` carries a real
`arena->offset >= arena->capacity` branch, contributing the +2 outcomes.
One of the two is covered (+1 hit).

**The uncovered outcome is a genuine gap, not a ceiling item, and it is NOT
counted against the six structurally-unreachable outcomes documented above.**
The `offset >= capacity` true branch is reachable in principle — a perfectly
sized allocation can land `offset == capacity` — but filling an arena with a
loop of equal-sized `arena_alloc` calls generally stops with
`offset < capacity` because of alignment padding, which is why the
accompanying test asserts agreement with `arena_free_bytes` rather than a
zero length. Closing it needs an allocation sequence chosen to land exactly
on the capacity boundary. Recorded here rather than folded into the ceiling
so that the ceiling keeps meaning "provably unexecutable".


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
maximum at the time, since the 6 outcomes above are provably unexecutable.
(Post-API-001 the surface is 68/78 = 87.2%; see the 2026-07-30 note below.)

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

3. **The achievable MC/DC ceiling under `pool_invariant` is 62/68 = 91.2%**
   on the surface analysed here (68/78 = 87.2% post-API-001 — see the
   2026-07-30 note below; the six outcomes below are unchanged).
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
74.3% post-API-001) will need its own audit; its number and unreachability shape are not
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

**Measurement-surface reclassification (2026-07-30, CI #1202, Commit 16/16b):**
pool.h's MC/DC surface moved **62/68 (91.2%) → 68/78 (87.2%)**. Cause:
API-001 added `pool_as_cbytes` (guarding `!pool || !pool->arena ||
pool->used == 0`) and `pool_reserved_cbytes` (guarding `!pool ||
!pool->arena`) — ten new outcomes, six of them covered by the tests added
alongside, four not.

**The six outcomes documented above are unchanged** — this record's
unreachability analysis stands as written; only the denominator moved. The
four new uncovered outcomes are the `pool->arena == NULL` halves of the two
new guards, which `pool_invariant` makes unconstructible for any pool that
`pool_init` has accepted — i.e. the same class as the six already documented,
arising in new code rather than a new phenomenon.

**Caveat, stated rather than glossed:** that classification is inferred from
the guard structure and the +6/+10 split, not from a per-outcome gcov audit
of the kind that produced the table above. The 87.2% figure is measured and
exact; the claim that exactly four of the ten are unreachable is provisional
until each is confirmed the same way the original six were. Until then the
"achievable ceiling" for pool.h should be read as the 62/68 analysis plus an
un-audited remainder, not as a new pinned ceiling.


## MCDC-005: API-Unreachable Hook-Guard Branch (region.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-005 |
| **Date**       | 2026-06-06 |
| **Baseline commit** | c9172fc (Canon-C CI #992) |
| **Scope**      | region.h — 1 of 22 condition outcomes (1 unreachable) |
| **Category**   | Coverage measurement methodology |

**Description**: 1 of 22 condition outcomes in `core/region.h` is not
exercisable by tests. It is the FALSE side of the `if (h->fn != NULL)` hook
guard inside region_end's LIFO cleanup loop:

| # | Function     | Line | Subcondition not covered          |
|---|--------------|------|------------------------------------|
| 1 | `region_end` | 496  | cond 0 false (`!h->fn`)           |

region.h's gcov-measured MC/DC is 21/22 = 95.5%. This is the achievable
ceiling — the single uncovered outcome is unreachable by construction.

### Rationale

The `if (h->fn != NULL)` guard checks each cleanup slot before dispatching it.
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
— whose `CANON_INVOKE_HANDLER_` invocation survives the flag and is
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

borrow.h's gcov-measured MC/DC is 36/38 = 94.7% on the post-Commit-9
surface (see the 2026-07-24 note below). This is the
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

**Measurement-surface reclassification (2026-07-24, Commit 9c):**
borrow.h's condition total moved 40 → 38 (95.0% → 94.7%): the
Commit-9 clamp fold at L1059 is min-shaped and left instrumentation
as branchless MIN_EXPR (see the MCDC-001 addendum). The removed
condition was fully covered (2/2). The two documented outcomes of
this record (borrowed_bytes_eq's one-NULL guard) are unchanged: this
record's residual analysis and the dead_by_invariant cross-stream
closure stand as written, with the ceiling arithmetic transposing to
(38 − 2) / 38 ≈ 94.7% — both documented outcomes and their
dispositions carry over unchanged to the 36/38 surface.

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

## MCDC-011: A Clean Cover TU — Zero Uncoverable Conditions, Zero Justification Rows (deque)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-011 |
| **Date**       | 2026-08-15 |
| **Baseline commit** | Canon-C CI #1234 (100% first reached); re-confirmed #1237, #1238, #1239, #1240. Superseded pre-fix measurement: CI #1231 (79/82) |
| **Scope**      | data/deque/ via `vmacros/coverage/deque_cover.c` — 82 of 82 condition outcomes exercised |
| **Category**   | Coverage measurement methodology |

**Description**: all **82 of 82** condition outcomes measured on the
deque cover TU are exercisable, giving **100.00%** with **zero
justification rows**. deque is the **fourth Shape-B cover TU** (after
option MCDC-006, result MCDC-007 and vec MCDC-010) and the first
*large* one to close clean: result_cover reached 100% over 6 outcomes,
deque does it over 82.

### The prediction, written down before the first run

The cover TU's header stated, before any measurement:

> **NONE. Predicted 100%, zero justification rows.**

with the reasoning recorded alongside it: vec's three uncoverables
(MCDC-010) were U1/U2 `!checked_mul` guard-redundancy-infeasible and
U3 `!buf` heap-environmental — **all three arising from allocation**.
deque allocates nothing. The buffer is caller-owned, there is no
alloc/free family, and checked.h is not in the include closure. Every
remaining condition is a NULL test, a size/capacity comparison, or a
ring-index ternary.

The header also fixed the **disposition in advance**: if a condition
came back uncovered, the response was *not* to open a justification
row but to find the driving input, because the argument above asserts
one exists. Only a genuine infeasibility argument could turn one into
a justified row.

### The denominator, also pre-counted

**39 generated conditions / 78 outcomes**, plus 2 scaffolding
conditions (4 outcomes) in the TU's own fill and wrap-around loops =
**41 conditions / 82 outcomes**. These were counted statically off the
preprocessed expansion under the exact measured flag pair
(`-DCANON_NO_REQUIRE -DNDEBUG`), per function, not estimated. The
measured denominator matched on the first run and every run since.

The flag pair matters more here than in most modules: `init`, `swap`
and both `*_unchecked` pushes contribute **1** condition between them
under `-DCANON_NO_REQUIRE`, against 10 `require_msg` guards with the
flag absent. A different generated total would mean `require_msg` had
not compiled out, and the header says so.

### Run 0 (CI #1231): 79/82 — wrong on the measurement, right on the disposition

Three outcomes came back uncovered. All three were **state-tracking
errors in the cover TU**, not properties of deque, and each was closed
by finding the driving input exactly as the pre-registered disposition
prescribed. **Zero justification rows were added.**

| # | Site | Cause | Fix |
|---|------|-------|-----|
| 1–2 | `peek_back` / `pop_back` `tail == 0` FALSE leg | Every `tail == 0` evaluation in the file was TRUE. With capacity 4 and tail at 3, the `push_back` commented "tail 0 → 1" wraps tail straight back to 0, so the two lines claiming to drive the FALSE leg drove TRUE a second time. | Driven in the `b3` block, where the state is controlled: push through the BACK to move tail off zero first. |
| 3 | `while (!deque_int_is_full(&d))` guard TRUE outcome | `d` was already 4/4 at that point, so the loop body never executed. | One `pop_front` ahead of the loop. |

None of the three fixes adds a condition site, so the denominator
stayed at 41/82 and the 100% prediction remained testable unchanged.
The stale comments were **corrected in place rather than deleted** —
the wrong claim is the interesting part of the record.

### Deliberate exclusions (not gaps)

Under `-DCANON_NO_REQUIRE` the `require_msg` guards on
`peek_front`/`peek_back`, `swap` and both `*_unchecked` pushes compile
to `((void)0)`, so calling those functions with NULL is undefined
behaviour — see VERIFY-019 F1 for the peek/pop asymmetry. The cover TU
therefore does **not** exercise the NULL legs of `peek_front`,
`peek_back`, `peek_*_option`, `swap`, or the unchecked pushes. Those
conditions do not exist under the measured flags, so they are absent
from the denominator rather than missing from the numerator. The TU
runs clean under ASan+UBSan, which is the check that confirms the
discipline held.

A second deliberate difference from `vec_cover.c`: vec calls
`vec_int_init(NULL, 0)` as a legal spec'd input. deque's `init`
requires `buffer != NULL` and `capacity > 0`, so the analogous call
would violate its (compiled-out) precondition. The buffer==NULL /
capacity==0 state is reached the spec'd way instead, via
`deque_int_empty()`.

### Shape B — confirmed

The coverage job's attribution check reports `deque_impl.h` with
**functions and lines but "No conditions"** under *both*
`deque_test.c` and `deque_cover.c`. The Shape-A-drift tripwire (vec's
precedent: conditions appearing in `_impl.h` would mean a body was
written out as real source lines) did not fire on any run. deque's
generated conditions attribute to `deque_cover.c` itself — vec's third
attribution variant, direct instantiation.

**This is the coverage stream's evidence alone.** The WP run neither
tests nor supports the shape claim, and the driver header says so
explicitly to prevent the upgrade being cited from the wrong stream.
`docs/vmacros.md`'s status tables move from "B (provisional)" to
"B (confirmed)" on this basis.

### Cross-references

- Predecessor ceiling entry and the allocation-derived uncoverables
  this entry contrasts with: MCDC-010 (vec, 155/158).
- Clean-audit predecessors: MCDC-007 (result, 28/28).
- Coverage methodology and the `-DCANON_NO_REQUIRE` measured surface:
  MCDC-001.
- WP-side record for the same module: VERIFY-019; the peek/pop
  NULL-guard asymmetry is VERIFY-019 F1.
- Cover TU: `vmacros/coverage/deque_cover.c`; CMake target
  `deque_cover` under `-DENABLE_COVERAGE_TUS=ON`; invoked directly by
  the coverage job (never an `add_test`, never globbed).

---

## MCDC-012: An Uninitialised-Bitset Battery That Inverted, and Four Outcomes Dead by Invariant (bitset)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-012 |
| **Date**       | 2026-08-27 |
| **Baseline commit** | Canon-C CI #1266 (16d0f0b). Enumerated at #1252; 122/126 measured at #1253; ratcheted to 130/134 at #1265 when VERIFY-020 F2 widened the guards |
| **Scope**      | `data/bitset.h` — 134 condition outcomes, 130 covered (97.01%), 4 justified |
| **Category**   | Structural coverage completeness |
| **Enforcement**| Measured in the aggregate MC/DC table; four justification rows below |

**Description**: `data/bitset.h` reaches **130/134 (97.01%)** MC/DC
condition-outcome coverage under GCC 14 `-fcondition-coverage`, with
**100.00% of 139 lines**. The four uncovered outcomes are dead by
invariant and take justification rows, not tests. Aggregate MC/DC is
1795/2012 (89.2%).

### No cover TU, deliberately

bitset is not a macro module: conditions attribute to `data/bitset.h`
itself. The ownership tripwire in the coverage job confirms this
POSITIVELY rather than by assumption — `bitset.h` reports its outcomes
while `bitset_test.c` separately reports its own, so the glob removes
only the latter. Adding a cover TU would build under two flag sets
against the same measured header: a merge hazard with no gain.

### Enumeration, then disposition

CI #1252 enumerated seventeen uncovered outcomes by line. Thirteen were
drivable and are driven; four are not.

**`test_uninitialised_bitset` — eleven outcomes, then sixteen.** The
`!bs->words` TRUE leg, once each in `clear_all`, `set_all`, `not`,
`count`, `is_empty`, `is_full`, `find_first`, `find_next`, `find_last`,
`as_bytes`, `as_cbytes`. All eleven need the same input: a non-NULL
Bitset whose words pointer is NULL. `Bitset b = {0}` is exactly that,
and it is reachable by ordinary C — a declared-but-never-initialised
Bitset — not a synthetic poke at private state.

**The battery inverted, and that is the interesting part.** As written
at #1253 it carried:

> *Do not "complete" this battery by adding the four. That would be UB
> in the configuration being measured.*

`set`/`clear`/`toggle`/`test` checked only `!bs`, then guarded the index
with `require_msg`, which is `((void)0)` under `-DCANON_NO_REQUIRE` —
the flag both the coverage and WP jobs use. On this very object those
four dereferenced NULL. Their ABSENCE was the finding, and the comment
said so.

VERIFY-020 F2 fixed it. All fifteen functions now guard
`!bs || !bs->words` uniformly, and the four are present — plus
`bitset_assign`, which delegates to set/clear and was exposed
transitively, a fifth site the original F2 scoping missed. The test is
now the evidence that the UB is GONE rather than the evidence that it
exists, and the comment records the inversion instead of being quietly
deleted.

**`test_mcdc_residual_legs` — two outcomes.** `find_next`'s
`prev >= bs->capacity` TRUE leg, and `bitset_and`'s tail loop
`for (w = n; w < bs->word_count; w++)`, entered only when `bs` has more
words than `other`. Nothing in the suite ANDed unequal capacities, so
that loop was never entered — it was also the file's only uncovered
LINE.

### The four justified outcomes: J1–J4

| ID | Line | Condition |
|----|------|-----------|
| J1 | 1323 | `return (bit < bs->capacity) ? bit : BITSET_NPOS;` — FALSE leg, `bitset_find_first` |
| J2 | 1371 | same shape, `bitset_find_next` first-word probe |
| J3 | 1381 | same shape, `bitset_find_next` scan loop |
| J4 | 1420 | same shape, `bitset_find_last` |

(Lines were 924 / 960 / 966 / 989 before F1 and F2 added code above
them. The four outcomes are unchanged in identity; only their line
numbers moved, and the move was verified by re-reading the source at
the new positions rather than assumed.)

All four fire only if a set bit is found at an index >= capacity, which
the PADDING INVARIANT forbids: bits at `[capacity, word_count*64)` are
always zero, so `bits_ctz` / `bits_clz` can never report one. They are
defensive branches, dead by invariant.

Reaching them would require writing `bs.words[]` directly behind the
API to assert behaviour the library does not promise. `diag.h` line 293
is the existing invariant-dead precedent.

**Cross-stream result, and the reason these four matter beyond the
number.** The SAME gap appears in the proof stream as the
`bitset_pad ==> bitset_pad_meaning` manual-proof obligation recorded in
VERIFY-020. Coverage cannot reach these branches because the invariant
forbids it; WP cannot prove them redundant because `pad_meaning` is not
machine-checked. One fact, two instruments, neither able to close it
alone.

### The ratchet, and what it did not disturb

F2 moved the denominator 126 → 134 (+8 outcomes: four `!bs->words`
TRUE legs and their FALSE partners) and the numerator 122 → 130. Lines
stayed at 139, because `if (!bs || !bs->words)` occupies the same line
as `if (!bs)`. The four justified outcomes stayed four: nothing new
became uncoverable.

Measured locally before the commit and confirmed at #1265/#1266.

### Fallout: fixing UB moved an analyzer finding into view

`bitset_as_bytes` returns `bytes_empty()` — `ptr == NULL` — when
`words == NULL`. `test_as_bytes` indexed `bv.ptr[0]` with nothing but a
comment for protection: *"bv.ptr is always non-NULL here, bs.words is a
stack array."*

On the `words == NULL` path the OLD `bitset_set` dereferenced words
unconditionally, so clang-analyzer's null-deref report landed inside
`data/bitset.h` — non-user code, suppressed by `-header-filter`. With
the F2 guard, `bitset_set` returns early instead, the path survives
into the test file, and the deref is reported where it is visible.

**The finding was always reachable.** Fixing the UB moved the report
from a suppressed location to a reported one — the same shape as this
project's MISRA per-line masking note: a count that rises after cleanup
work can be surfacing rather than regression, so diff before
concluding. Fixed by asserting `bv.ptr != NULL` before indexing, which
`str_view_test`, `stringbuf_test` and `slice_test` already do;
`bitset_test` was the outlier, substituting a comment for a check.

### Measurement discipline

Local GCC was 13, so `-fcondition-coverage` was unavailable at #1253
and the change was validated on branch-taken as a proxy before commit,
predicting exactly four remaining untaken branches at the four lines
that became J1–J4. CI then measured the real metric and matched. The
prediction was written before the run rather than after.

The F2 ratchet was measured directly, before and after, with the same
command and flags.

### Cross-references

- Proof-stream record for the same module: VERIFY-020 (4845/5008, 163
  pinned residuals, 71 own).
- F2's WP-side statement and its five-goal proof cost: VERIFY-020 F2
  and F5.
- Prior invariant-dead precedent: `semantics/diag.h` line 293.

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

---

## MISRA-DEV-001: Multiple Points of Exit (Rule 15.5)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-001 |
| **Date**       | 2026-07-18 |
| **Scope**      | Project-wide — 325 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Advisory) |

**Description**: Rule 15.5 (Advisory) recommends a single point of exit
per function. Canon-C functions use early-return guard clauses
throughout: `require_msg` precondition blocks at function entry, early
`return` on `Result`/`Option` construction, and the `goto done` cleanup
idiom for resource-holding functions.

**Rationale**: Early return on precondition failure and error paths is
the project's documented style (README, "Contracts" and "Cleanup"
sections). It is what makes preconditions visible at function entry and
keeps error paths flat. Rewriting to single-exit form would nest the
happy path inside accumulated condition state and obscure the exact
structure the library exists to make visible, with no behavioral
benefit. The rule is Advisory; MISRA C:2012 permits documented
project-wide deviation of advisory rules.

**Mitigation**: Control-flow correctness is covered by MC/DC condition
coverage measurement (coverage job) and, for verified headers, by
Frama-C WP proofs over all exit paths (frama-c-* jobs). Suppressed
project-wide via `--suppress=misra-c2012-15.5` in the misra CI job.

---

## MISRA-DEV-002: Unused Macros Under Standalone-Header Analysis (Rule 2.5)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-002 |
| **Date**       | 2026-07-18 |
| **Scope**      | Project-wide — 161 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Advisory) |

**Description**: Rule 2.5 (Advisory) recommends that a project contain
no unused macros. The misra CI job analyzes each header as a standalone
translation unit, so every public API macro is by definition "unused"
within its own TU.

**Rationale**: The flagged macros are the library's API surface —
usage occurs in downstream user code and in the test suite, neither of
which is part of the analyzed TU. This is an artifact of the
standalone-header analysis configuration, not dead code.

**Mitigation**: The test suite instantiates the public macro surface
across the CI matrix; genuinely dead macros would surface in review.
Suppressed project-wide via `--suppress=misra-c2012-2.5` in the misra
CI job.

---

## MISRA-DEV-003: Token-Pasting as the Type-Instantiation Mechanism (Rule 20.10)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-003 |
| **Date**       | 2026-07-18 |
| **Scope**      | Project-wide (concentrated in `*_mangle.h` and `DEFINE_*` machinery) — 118 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Advisory) |

**Description**: Rule 20.10 (Advisory) recommends avoiding the `#` and
`##` preprocessor operators. Canon-C's name-mangling machinery
(`*_mangle.h`) and `DEFINE_*` / `DECLARE_*` instantiation macros use
`##` to stamp per-type function families, and `#` for stringized
contract messages.

**Rationale**: Token pasting is the library's type-instantiation
mechanism — the documented architectural substitute for the templates
C99 lacks (README, `algo/` and `data/` sections; docs/vmacros.md).
Removing it removes the library's central design.

**Mitigation**: The instantiation machinery follows a fixed 5-file
architecture; every expansion is compiled under three compiler
families and multiple flag configurations in CI, so mis-pastes fail
loudly at build time. Instantiated modules are additionally verified
through the Shape-B driver mechanism (docs/vmacros.md; VERIFY-014/-015/
-018). Suppressed project-wide via `--suppress=misra-c2012-20.10` in
the misra CI job.

---

## MISRA-DEV-004: Comment Markers Inside Documentation Comments (Rule 3.1)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-004 |
| **Date**       | 2026-07-18 |
| **Scope**      | Project-wide — 105 sites at baseline (CI #1169), all in documentation comment blocks |
| **Category**   | MISRA rule deviation (Required) |

**Description**: Rule 3.1 (Required) forbids the character sequences
`/*` and `//` within a comment. The flagged sites are Doxygen-style
documentation blocks containing URLs (`https://...` contains `//`) and
inline code examples.

**Rationale**: The rule targets accidentally nested comment markers
that can silently swallow code. The flagged occurrences are inside
deliberate documentation text; no executable code is adjacent or
affected. Rewording URLs and code examples to avoid the sequences
would degrade documentation accuracy for no safety benefit.

**Mitigation**: None required — documentation-only. Suppressed
project-wide via `--suppress=misra-c2012-3.1` in the misra CI job.
Any future occurrence of a genuinely mis-nested comment is caught by
`-Wcomment` under `-Wall` in the build matrix.

---

## MISRA-DEV-005: Identifier Uniqueness Under Standalone Analysis of Instantiation Headers (Rules 5.6, 5.8, 5.9)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-005 |
| **Date**       | 2026-07-18 |
| **Scope**      | Template-machinery headers (`algo/*_impl.h`, `data/hashmap/*`, `algo/fold/fold.h`, `algo/map/*`, `core/slice.h`, `util/log/log.h`) — 47 sites at baseline (CI #1169): 5.6 x10, 5.8 x30, 5.9 x7 |
| **Category**   | MISRA rule deviation (5.6/5.8 Required, 5.9 Advisory) |

**Description**: Rules 5.6/5.8/5.9 require uniqueness of typedef names
and identifiers across the project. The flagged identifiers are in
per-type instantiation headers (`*_impl.h` and companions), which the
misra CI job analyzes standalone across multiple preprocessor
configurations of the same file.

**Rationale**: In real translation units, generated identifiers are
mangled per instantiated type via the `*_mangle.h` machinery and are
unique. The analyzer sees the un-instantiated template names
"collide" across configurations of the same source — an artifact of
standalone-header analysis without an instantiation context, the same
tool-limitation family as MISRA-CFG-001.

**Mitigation**: Multi-instantiation link tests in the test suite fail
on genuine identifier collisions (duplicate-symbol link errors) across
the CI matrix. Suppressed via `--suppress=misra-c2012-5.6`, `-5.8`,
`-5.9` in the misra CI job.

---

## MISRA-DEV-006: void* Conversions in the Generic Interface Level (Rule 11.5)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-006 |
| **Date**       | 2026-07-18 |
| **Scope**      | Generic (`void*`) interfaces: `core/memory.h`, `core/arena.h`, `core/slice.h`, `core/primitives/{ptr,compare}.h`, `algo/` generic level, string/collection internals — 36 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Advisory) |

**Description**: Rule 11.5 (Advisory) recommends against converting
`void*` to object pointers. The flagged conversions are allocator
returns (`mem_alloc`, `arena_alloc`), the generic Level-1 `void*` +
function-pointer interfaces of `algo/`, and `{ptr, len}` byte-view
plumbing.

**Rationale**: Type-erased operation over arbitrary element types is
one of three documented API levels; `void*` conversion at the
boundary is inherent to generic C library design and to any allocator
interface. The typed macro level and `DEFINE_ALGO_X` instantiation
level are the documented alternatives for callers requiring full type
visibility (README, `algo/` section) — the strict path exists and is
the recommended one for verification-grade use.

**Mitigation**: Element size and count are carried explicitly
alongside every `void*`; slice bounds are contract-checked; the
allocator conversions sit inside WP-verified functions (VERIFY-008/
-009). Suppressed via `--suppress=misra-c2012-11.5` in the misra CI
job.

---

## MISRA-DEV-007: #undef as Template-Parameter Cleanup (Rule 20.5)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-007 |
| **Date**       | 2026-07-18 |
| **Scope**      | Instantiation machinery (`algo/*` and `data/hashmap/*` template headers) and internal helper-macro cleanup (`bits.h`, `checked.h`, `util/time.h`) — 30 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Advisory) |

**Description**: Rule 20.5 (Advisory) recommends against `#undef`. The
flagged sites undefine template parameters (`HASHMAP_KEY_TYPE`, algo
linkage/type parameters) after instantiation, and clean up
file-internal helper macros before header exit.

**Rationale**: Undefining template parameters after instantiation is
what permits multiple instantiations per translation unit — it is the
mechanism, not an accident. Internal helper-macro cleanup prevents
namespace leakage from headers, which is a hygiene improvement in a
header-only library, not a hazard.

**Mitigation**: Undefs are confined to the parameter and helper names
of the enclosing header; the guarded-emission pattern is compiled
under every CI configuration. Suppressed via
`--suppress=misra-c2012-20.5` in the misra CI job.

---

## MISRA-DEV-008: Pointer Arithmetic in Slice and String Processing (Rule 18.4)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-008 |
| **Date**       | 2026-07-18 |
| **Scope**      | `{ptr, len}`-view processing: `core/slice.h`, `semantics/borrow.h`, `semantics/diag.h`, `data/stringbuf.h`, `data/convenience/dynstring.h`, `util/str/*`, `util/file.h` — 27 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Advisory) |

**Description**: Rule 18.4 (Advisory) recommends against applying
`+`, `-`, `+=`, `-=` to pointer operands. The flagged sites advance
cursors and compute sub-views over `{ptr, len}` slices and string
buffers.

**Rationale**: Slice and string processing over pointer+length views
is the purpose of these modules; the arithmetic is bounded by the
carried `len` at every site, which is exactly the mitigation the rule
exists to encourage. Replacing pointer arithmetic with index
arithmetic re-derives the same addresses with additional operations
and no additional checking.

**Mitigation**: Every flagged operation is bounds-guarded by the
slice's carried length; `slice.h` and `borrow.h` are WP-verified with
RTE checking (VERIFY-007/-016), which discharges in-bounds pointer
validity for the verified subset. Suppressed via
`--suppress=misra-c2012-18.4` in the misra CI job.

---

## MISRA-DEV-009: stdarg in Formatting and Logging Entry Points (Rule 17.1)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-009 |
| **Date**       | 2026-07-18 |
| **Scope**      | printf-style entry points: `util/log/log.h`, `data/stringbuf.h`, `data/convenience/dynstring.h` — 24 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Required) |

**Description**: Rule 17.1 (Required) forbids the features of
`<stdarg.h>`. The flagged sites are the `..._fmt` / logging entry
points that forward variadic arguments to `vsnprintf`.

**Rationale**: printf-style formatting is the accepted interface for
logging and string building; per-type alternatives multiply the API
surface without removing the underlying varargs inside `vsnprintf`
itself. The sites are confined to rendering/formatting functions that
are documented hosted-tier (Tier 2) functionality, excluded from the
bare-metal subset (README, "Bare-metal and embedded use").

**Mitigation**: Format strings are compile-time literals at library
call sites; the GNU `format` attribute is applied where available so
GCC/Clang type-check the argument lists under `-Wall -Werror` across
the CI matrix. Suppressed via `--suppress=misra-c2012-17.1` in the
misra CI job.

---

## MISRA-DEV-010: Pointer/Integer Round-Trips for Alignment Computation (Rule 11.6)

| Field          | Value |
|----------------|-------|
| **ID**         | MISRA-DEV-010 |
| **Date**       | 2026-07-18 |
| **Scope**      | Alignment machinery: `core/primitives/ptr.h`, `core/memory.h`, `core/arena.h`, `core/pool.h`, and the collection headers that align backing storage (`bitset.h`, `priority_queue.h`, `stringbuf.h`, `deque/hashmap/vec` impls, convenience vectors) — 19 sites at baseline (CI #1169) |
| **Category**   | MISRA rule deviation (Required) |

**Description**: Rule 11.6 (Required) forbids casts between `void*`
and arithmetic types. The flagged sites are `uintptr_t` round-trips
used to compute and apply alignment (`align_up`/`align_down` over
addresses) in the allocators and in collection headers that align
their backing storage.

**Rationale**: Alignment computation requires treating an address as
an integer; C99 provides `uintptr_t` for exactly this conversion.
This pattern is already a named verification residual category in
this document — the `uintptr_t` round-trip goals of VERIFY-006 and
the alignment-formula goals of VERIFY-008 — with manual arguments
recorded there. The MISRA deviation covers the same sites for the
same underlying reason: the pattern is deliberate, centralized, and
characterized.

**Mitigation**: The round-trip validity and alignment-formula
correctness arguments are recorded in VERIFY-006/-008 and enforced by
the frama-c-ptr / frama-c-memory pinned baselines; the computations
are centralized in `ptr.h` rather than repeated ad hoc. Suppressed
via `--suppress=misra-c2012-11.6` in the misra CI job.
## MISRA-DEV-011: Rule 21.1 — _POSIX_C_SOURCE feature-test macro protocol (util/time.h)

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-011 | 2026-07-19 | util/time.h lines 23-29 (3 directives) | Per-site deviation (inline suppressions) |

**Description**: Rule 21.1 forbids `#define` or `#undef` of reserved identifiers. util/time.h defines (and, when a caller has already set a lower value, `#undef`s and redefines) `_POSIX_C_SOURCE 199309L` before any system-header inclusion.

**Rationale**: This is the protocol POSIX itself mandates for the identifier: `_POSIX_C_SOURCE` is a feature-test macro whose entire documented interface is that *application code* defines it before including system headers (POSIX.1-2017 §2.2.1). Under strict `-std=c99`, glibc hides `clock_gettime` and `CLOCK_MONOTONIC` unless the macro is set to at least 199309L; without it, util/time.h's monotonic clock cannot be declared at all on POSIX hosts. The `#undef`/redefine arm exists only to raise a caller's insufficient value and preserves any caller value ≥ 199309L. Renaming is impossible by definition — the identifier's reservedness is precisely what makes it the libc control knob. The 2026-07-19 sweep removed every *project-owned* reserved-identifier macro (rule 21.1 count → 0 outside this file); these three directives are the sole principled remainder.

**Mitigation**: The block is `#ifndef _WIN32`-guarded and confined to a Tier 2 utility header outside the verified core. Each of the three directives carries an individual inline `cppcheck-suppress` citing this record, so any *new* 21.1 finding anywhere in the tree surfaces in CI rather than being absorbed by a rule-wide suppression. util/time.h's behavior is exercised by the test suite on Linux, macOS (both POSIX arm) and MSVC/MinGW (the `_WIN32` arm) across the CI matrix.

## MISRA-DEV-012: Rule 20.7 — macro parameters in type-name, declarator, and token-composition positions (template machinery)

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-012 | 2026-07-22 | 117 macro definitions across 26 headers (each carries an inline suppression) | Per-site deviation (inline suppressions) |

**Description**: Rule 20.7 requires macro-parameter expansions to be parenthesized. Canon-C's type-instantiation machinery (`DEFINE_*` / `DECLARE_*` / `IMPL_*` / `MANGLE_*` families and the container `*_defn.h` / `*_decl.h` / `*_impl.h` headers) necessarily uses parameters as type names (`_t v`, `type* ptr`, `typedef struct DequeTag`), as declaration specifiers (`_linkage`), and as operands of `##` token composition — positions where C grammar forbids parenthesization outright (`(_t) v` is a cast or a syntax error, never a declaration).

**Rationale**: The parameter-as-type/identifier usage *is* the library's template mechanism, the same design identity already recorded for the `##` operator itself (MISRA-DEV-003) and for instantiation-header identifier artifacts (MISRA-DEV-005). Additionally, the reference checker (cppcheck 2.13.0 `misra.py`) implements 20.7 as neighbor-character text analysis that (a) cannot distinguish type positions from expression positions and (b) reaches into string literals, flagging parameter names appearing in `require_msg` diagnostic text (probe-verified against the shipped addon). A rule-wide suppression would therefore silence a check whose expression-position findings ARE actionable; the per-site form keeps exactly those live.

**Prerequisite sweep (2026-07-22, same commit)**: before any suppression was added, every parenthesizable occurrence in the 126 then-flagged macros was fixed — 73 occurrences: 60 expression/callee positions (`(_f)((_o).value)`-style, each validated by full-suite compilation and byte-level assembly equivalence) and 13 parameter names inside `require_msg` string literals (parenthesized in-string, which the checker honors; diagnostic text change only). 9 macros thereby cleared entirely (ALGO_FOLD family, CANON_DROP/_IF, VEC_ASSERT_TYPE, IMPL_OPTION_MAP/AND_THEN/FILTER) and carry no suppression. The 117 suppressed macros each retain only occurrences in unparenthesizable positions.

**Mitigation**: Each of the 117 definitions carries an individual inline `cppcheck-suppress` citing this record — rule 20.7 stays live for every other macro in the tree and for all future code, so a new expression-position 20.7 finding surfaces in CI rather than being absorbed. The macro-generated code paths are exercised by the instantiation test suite (51 test TUs) and, for the option/result/vec families, verified by the WP driver TUs under pinned proved-goal baselines; a qualified MISRA checker (Polyspace/LDRA/PC-lint, per the project's certification note) performs real type-aware 20.7 analysis and remains the certification path.

## MISRA-DEV-013: Rule 14.2 — template `_impl.h` loops unanalyzable without the includer's linkage macro

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-013 | 2026-07-24 | 13 `for` loops across 7 template headers (each carries an inline suppression) | Per-site deviation (inline suppressions) |

**Description**: Rule 14.2 requires a `for` loop to be well-formed. The loops
cited here are textbook-well-formed — `for (usize i = 0; i < len; i++)` with a
single counter, a pure comparison, a lone increment, and no modification of the
counter in the body. They are reported only because the reference checker
(cppcheck 2.13.0 `misra.py`) analyses each `_impl.h` template header standalone,
without the `*_LINKAGE` macro that the including umbrella header supplies. With
the macro undefined the function signature does not parse, the loop counter never
resolves to a `Variable` object, and the addon takes its "if it is not possible
to identify a loop counter, all three clauses must be empty" branch (misra.py
`misra_14_2`, lines 2866-2875) and reports a violation.

**Evidence (probe-verified against the shipped addon, 2026-07-24)**:
- Substituting a concrete linkage specifier (`ALGO_ANY_ALL_LINKAGE` → `static`,
  `HASHMAP_LINKAGE` → `static`) drops the findings from 2 → 0 and 5 → 0
  respectively, with no other edit.
- Scanning the umbrella headers (which define the linkage macro before including
  the template) reports zero rule-14.2 findings for the same loops.
- Rewriting the loops does *not* clear the finding: hoisted declaration,
  pre-increment, removing the counter from the body, and `!=` for `<` were each
  measured and all still report. The finding is not addressable at the site.
- Isolating the includes shows the reports appear only once `core/ownership.h`
  supplies `borrowed(T) → T`; without it the signature is unparseable and the
  loop is not analysed at all. The finding is thus an artifact of *partial*
  configuration, not of the loop.

**Rationale**: This is the same root cause the misra job already acknowledges
with its command-line `--suppress=misra-config:*_impl.h` — template headers are
not standalone translation units and cannot be fully configured as such. The
genuine rule-14.2 violations found in the same sweep were fixed rather than
deviated (`data/bitset.h` first clause `w++` → `w = w + 1u`; the empty
controlling expression in `util/str/str_split.h`), so this record covers only
the unaddressable class.

**Mitigation**: Each of the 13 loops carries an individual inline
`cppcheck-suppress` citing this record — rule 14.2 stays live for every other
loop in the tree and for all future code, so a genuine malformed `for` surfaces
in CI rather than being absorbed. Twelve of the thirteen correspond to findings
visible in CI #1190; the thirteenth (`hashmap_impl.h`, the Phase-1 probe loop) is
currently masked at CI by a rule-15.4 finding on the same line and is suppressed
here for absolute-state consistency, so that clearing that 15.4 in a later commit
cannot surface a latent 14.2. A qualified MISRA checker analysing the
instantiated umbrella headers (Polyspace/LDRA/PC-lint, per the project's
certification note) performs configuration-complete 14.2 analysis and remains the
certification path.

**Root-cause alternative considered**: defining the `*_LINKAGE` macros for the
misra job, or scanning umbrella headers instead of templates, removes this class
at its source. It was deliberately *not* taken in this commit because it changes
the scan surface for the entire tree and would re-baseline every count in the
campaign ledger; it is recorded here as a candidate for a dedicated
instrument-change commit with its own dated re-baseline.

**SUPERSEDED (2026-07-25, Commit 12) — record retired.** MISRA-SCAN-001 removed
the condition this deviation existed to accommodate: `*_impl.h` fragments are no
longer scanned as standalone translation units, so the rule-14.2 findings this
record covered do not arise. All 13 inline suppressions were deleted in the same
commit, verified dead beforehand (86-header scan against a tree with every
suppression removed: 101 findings, zero rule-14.2 — identical to the same scan
with them present). Rule 14.2 remains live and unsuppressed across the entire
tree. This record is retained for provenance: it documents why the findings were
believed unaddressable at the site (they were — at the site), and its
"root-cause alternative considered" paragraph is what became MISRA-SCAN-001.


## MISRA-SCAN-001: Analysis-surface re-baseline — template fragments are not translation units (2026-07-25, Commit 12)

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-SCAN-001 | 2026-07-25 | misra job header discovery (`*_impl.h`, `*_mangle.h` excluded) | Instrument change + re-baseline |

**What changed**: the misra job previously handed every `*.h` under
`core/ semantics/ data/ algo/ util/ canary/` to cppcheck as a standalone
translation unit — 114 headers. 28 of those are internal template fragments
(`*_impl.h`, `*_mangle.h`) that are never compiled alone: they depend on macros
(`*_LINKAGE`, type parameters such as `HASHMAP_TYPE_NAME`) that their including
entry point supplies. Analysed without that configuration the function
signatures do not parse, and the checker emits findings that describe the scan
rather than the code. The job now excludes those 28 and scans the 86
configuration-complete headers.

**Why this is not a coverage reduction**: the fragments are still analysed —
through the entry points that include them, with their macros defined — and
findings inside them are still reported at their own file and line (confirmed:
the corrected scan reports 19.2, 15.4 and 11.3 inside `hashmap_impl.h`, reached
via `hashmap.h`). Include-graph reachability was computed over all 114 headers
before the change: **28 fragments, 0 unreached** from the retained set.

**Rejected alternative (recorded because it looks attractive and is not safe)**:
also excluding `*_decl.h` / `*_defn.h` yields a lower count (87 rather than 98).
It was rejected: nothing in the tree includes those files — they are user-facing
entry points — so excluding them leaves **20 fragments with no scanned includer
at all**, and their contents (e.g. the `CANON_RESULT(bool, Error)` instantiation
at `algo/fold/fold_decl.h:54`) would go entirely unanalysed. A lower number
obtained by not looking is not an improvement. The related option of adding
includes to the umbrellas so those files become reachable was also rejected:
that is a source change made to satisfy a scanner.

**Re-baseline arithmetic (measured locally at e974663 before the change)**:

| | count |
|---|---|
| previous surface (114 headers), CI #1191 | 108 real |
| corrected surface (86 headers) | **98 real** |
| net | **-10** |

Disappearing: rule 17.3 x9 (the template-linkage class — `pred(...)` calls read
as implicit declarations once the signature fails to parse) and rule 2.2 x1.
**Appearing: none.** No finding was hidden by the previous surface.

**Consequence for MISRA-DEV-013**: superseded and retired in this commit. Its
13 inline rule-14.2 suppressions were verified dead before removal — the 86-header
scan run against a tree with all 13 deleted returns **101 findings including zero
rule-14.2**, identical to the same scan with them in place.

**Consequence for the ledger**: every count in the campaign ledger from 1672
down to 108 was measured on the previous surface. They remain valid as a record
of what that instrument reported, and the reductions they describe were real
(each was verified by shift-aware set-diff against CI at the time). They are
NOT directly comparable to counts from 98 onward. The ledger is therefore read
as two segments joined at this commit: 1672 -> 108 on the pre-2026-07-25
surface, 98 -> ... on the corrected one, with -10 of the step attributable to
the instrument rather than to the code.

**Corrected classification note**: rules 8.7 (x5) and 5.7 (x4) were provisionally
classified as template-configuration artifacts during the 2026-07-25 census.
They survive configuration-complete scanning and are therefore **real findings**
arising from the `*_decl.h` entry points, not scan artifacts. The census
classification is corrected accordingly.

## MISRA-DEV-014: Rule 19.2 — the `union` keyword in Result/Option

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-014 | 2026-07-26 | 13 `CANON_RESULT` / `CANON_OPTION` instantiations (inline suppressions) | Design-identity deviation |

**Description**: Rule 19.2 advises against `union`. Every cited site is an
instantiation of the library's discriminated-union types — `CANON_RESULT(T, E)`
and `CANON_OPTION(T)` — whose payload is a tagged union selected by an
explicit discriminant.

**Rationale**: the tagged union IS the type. Result and Option exist to make
"a value or an error, never both" representable without sentinel values,
out-parameters, or `errno`. Removing the union does not fix a defect; it
deletes the abstraction the library is built on and pushes callers back onto
the error-handling idioms this project exists to replace. The declaration is
deliberately C99-strict — a NAMED union member (`.val`) rather than a C11
anonymous union, precisely so the type is portable to the strict-C99 toolchains
used in certified contexts (CompCert, MSVC /Za, Polyspace, LDRA); see the
C99-compliance note in `semantics/result/result_decl.h`.

**Why the rule's hazard does not apply**: 19.2's concern is reading a member
other than the one last written. Access is never raw here: `_is_ok()` /
`_is_err()` / `_unwrap()` / `_get_ok()` gate every read on the discriminant,
and those accessors are formally verified — the result and option WP jobs prove
their contracts (VERIFY-013/014, pins 185/215 and 189/223), which is stronger
evidence of correct member selection than the rule's syntactic prohibition
provides.

**Mitigation**: per-site inline suppressions; rule 19.2 stays live for every
other union in the tree.

## MISRA-DEV-015: Rule 21.3 — `malloc` / `free` / `realloc` in the allocation layer

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-015 | 2026-07-26 | 9 sites: `core/memory.h` x2, `data/convenience/dynstring.h` x7 | Design-identity deviation |

**Description**: Rule 21.3 prohibits the `<stdlib.h>` allocation functions.
Two sites are the library's allocation boundary itself (`mem_alloc` wrapping
`malloc`, `mem_free` wrapping `free`); seven are `dynstring`'s growth policy
(`realloc`).

**Rationale**: the rule's target is *unmanaged* dynamic allocation scattered
through application code. This library's answer to that hazard is structural —
`Arena`, `Pool` and `Region` provide bounded, resettable, allocation-free-by-
construction storage, and they are the recommended path. `mem_alloc`/`mem_free`
exist as the single audited boundary where heap allocation is admitted, so that
a project which must use the heap has exactly one place to review, and a project
which must not can exclude these two functions and use the arena family without
losing the rest of the library. Deleting the boundary does not remove
allocation from user programs; it removes the audited chokepoint.

**Note on `dynstring`**: this is an explicitly heap-backed convenience container
under `data/convenience/`. Its arena-backed counterparts (`StringBuf` on an
`Arena`, `str_join` with an arena allocator) are available for
allocation-restricted builds.

**Residual risk**: allocation failure is a *checked* condition throughout —
`mem_alloc` returns NULL on failure, every caller in the tree tests it, and
the WP jobs prove the NULL-return contracts (`typed_cast_mem_alloc_*`, VERIFY-008).

**Mitigation**: per-site inline suppressions; rule 21.3 stays live tree-wide.

## MISRA-DEV-016: Rule 21.6 — `<stdio.h>` inclusion for formatted output

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-016 | 2026-07-26 | 6 `#include <stdio.h>` sites | Design-identity deviation |

**Description**: Rule 21.6 prohibits the standard I/O functions. All six cited
sites are the `#include <stdio.h>` line itself, in the six headers that need
formatted output: `contract.h` (diagnostic handler), `diag.h`, `log.h`,
`file.h`, `dynstring.h` and `stringbuf.h` (`vsnprintf` for `append_fmt`).

**Rationale**: these are the library's diagnostic and I/O surfaces. A contract
handler that cannot report which contract failed, a logger that cannot log, and
a file module that cannot read files are not improvements. The bounded forms
are used deliberately — `vsnprintf`/`snprintf` with an explicit size argument,
never `sprintf`, `gets`, or the unbounded `scanf` family.

**Deliberately NOT deviated — the canary depends on this rule.** The misra job's
three-violation canary (`canary/misra_canary.h`) uses rule 21.6 as one of its
three tripwires. A command-line or blanket suppression of 21.6 would silence the
canary and blind the job to its own failure, so this deviation is implemented
*exclusively* as six per-site inline suppressions. The canary's 21.6 continues
to fire and the job continues to require exactly 3 canary findings. This
constraint is the reason no rule in this campaign is ever deviated globally.

**Mitigation**: per-site inline suppressions. Callers needing a freestanding
build can define the library's I/O-free subset; `core/` requires stdio only in
`contract.h`, and only for the default handler, which is replaceable via
`contract_set_handler()`.

## MISRA-DEV-018: Rule 1.2 — compiler atomic intrinsics for lifetime-token generation

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-018 | 2026-08 | 2 sites in `core/primitives/lifetime.h`, both inside `CANON_LIFETIME_DEBUG` | Correctness-required deviation |

**Description**: Rule 1.2 ("language extensions should not be used", advisory)
is deviated at two sites in `canon_lifetime_next_id_()`: GCC/Clang's
`__atomic_fetch_add(..., __ATOMIC_RELAXED)` and MSVC's
`_InterlockedIncrement64()`. Both are compiler intrinsics rather than ISO C99.

**Rationale**: this is the only deviation in the campaign taken because the
conforming alternative is *incorrect*, not because it is inconvenient.

Token generation is a read-modify-write of a counter shared between the
caller's threads. Performed non-atomically, two concurrent constructions can
read the same counter value; because the token is `counter ^ owner-address`,
two owners can then receive the same token — and the token is precisely the
value the borrow checks compare. A stale borrow can validate against a
different live owner, so **the lifetime check passes when it should fail**.
The mode exists to catch use-after-invalidate; the race defeats it silently
and in the unsafe direction.

C11's `<stdatomic.h>` is the conforming fix and is preferred where available
(level 1 of the ladder in `lifetime.h`). It is not sufficient on its own:
Canon-C is C99 (`CMAKE_C_STANDARD 99`, `REQUIRED ON`), so a C11-only fix
would be dead code in every CI job and for every caller following the
project's own language stance — correct, and never compiled. The intrinsics
are what make the fix reach the configuration the project actually ships.

**Measured, on a single-core runner, 32 threads, 1.6M tokens, fixed owner
address so the address term cannot mask a counter collision:**

| path | `CANON_LIFETIME_ATOMIC_IDS` | duplicate tokens | TSan races |
|------|------|------|------|
| GCC/Clang intrinsic (C99, this deviation) | 1 | 0 | 0 |
| C11 `<stdatomic.h>` | 1 | 0 | 0 |
| plain increment, `-O2` | 0 | 150,000 of 1,600,000 | 2 |
| plain increment, `-O0` | 0 | 0 | 2 |

The `-O0` row is why the deviation is taken rather than the caveat merely
documented: the race does not reproduce without optimisation, because the
optimiser is what keeps the counter in a register across the loop. A
debug-only safety mechanism that is intact in a debug build and 9% corrupt in
an optimised one is the worst available failure profile.

**Mitigation**: two per-site inline suppressions, never a command-line or
blanket suppression — the campaign's standing constraint, so the misra job's
canary continues to fire. Both sites are inside `#ifdef CANON_LIFETIME_DEBUG`
and are therefore absent from every default build and from every WP job (all
run with `CANON_LIFETIME` off, so no proof baseline is affected). Both are
additionally guarded by `CANON_NO_GNU_EXTENSIONS`, so the CompCert job and any
strict-C99 build fall through to the conforming path.

**Correction (same day this record was written).** The paragraph above
originally also claimed the sites were absent "from the misra scan itself,
which does not define the macro". **That was false**, and the first run
disproved it: cppcheck enumerates preprocessor configurations rather than
scanning a single one, and its log shows it checking
`CANON_LIFETIME_DEBUG;CANON_NO_GNU_EXTENSIONS;__GNUC__` and neighbours
explicitly. The block IS scanned. The prediction that the pinned advisory
count would stay at 53 therefore failed — it read 56.

The three extra findings were **not** the intrinsics: the rule 1.2
suppressions worked as written. They were rule **20.9** at
`lifetime.h:234/238/243`, on the ladder's own selection tests. On the two
paths that select level 4, `CANON_LIFETIME_ATOMIC_LEVEL_` was never defined,
so `#if CANON_LIFETIME_ATOMIC_IDS && (CANON_LIFETIME_ATOMIC_LEVEL_ == 1)`
relied on C evaluating an unrecognised identifier as 0. Legal, and precisely
the fragility 20.9 exists to catch. Fixed by defining the macro to 4 on those
paths rather than suppressing the findings — a real defect the scan found,
not noise, and the count returns to 53. Verified with `-Wundef` clean across
all four configurations.

**Residual risk, stated rather than mitigated**: on a toolchain that is
neither C11 nor GCC/Clang/MSVC, or with `CANON_NO_GNU_EXTENSIONS` or
`CANON_LIFETIME_NO_ATOMICS` defined, generation falls back to the plain
increment and **concurrent construction under `CANON_LIFETIME_DEBUG` remains
a data race**. `CANON_LIFETIME_ATOMIC_IDS` reports which path is in effect;
`docs/thread-safety.md` §3 states the caller's obligation on that path. The
`lifetime-token-concurrency` CI job exercises both directions.

**Evidence**: `test/concurrency/lifetime_token_test.c`, run by the
`lifetime-token-concurrency` job. Note what that job gates and what it does
not: zero duplicates on an atomic path is deterministic and is gated; the
*presence* of duplicates on the fallback is not, because a data race is
undefined behaviour rather than a scheduled event — the same counter produced
50000, 50000, 0, 50000, 50000 duplicates across five consecutive runs. TSan
instruments accesses instead of sampling outcomes and gives a deterministic
two-way signal (0 races atomic, 2 races fallback), so it is what the job
gates the fallback direction on.

## MISRA-DEV-017: Rule 2.3 — the C99 static-assertion idiom

| ID | Date | Scope | Category |
|----|------|-------|----------|
| MISRA-DEV-017 | 2026-07-26 | 5 sites: `types.h` x2, `diag.h` x2, `ptr.h` x1 | Tool-idiom deviation |

**Description**: Rule 2.3 forbids unused type declarations. Every cited site is
a compile-time assertion. Under C11 `static_require(cond, msg)` expands to
`_Static_assert`; under strict C99 it expands to the portable negative-size-array
idiom `typedef char static_assert_##msg[(cond) ? 1 : -1];` (see
`core/primitives/contract.h`). Two sites in `types.h` spell the idiom out
directly for float-width checks.

**Rationale**: the typedef is not unused — **being declared is its entire
function**. If the condition is false the array size is negative and the
translation unit fails to compile, which is the assertion firing. Removing the
"unused" type removes the check. The findings are an artifact of a C99 toolchain
configuration: compiled as C11 these sites produce `_Static_assert` and the rule
does not apply at all.

**What is actually being asserted**: `sizeof(f32) == 4`, `sizeof(f64) == 8`,
`CANON_USIZE_MAX == SIZE_MAX`, and the `diag.h` frame/message-length floors —
i.e. the platform assumptions the rest of the library's proofs rest on. These
are exactly the checks a MISRA project should want to keep.

**Mitigation**: per-site inline suppressions; rule 2.3 stays live for genuinely
unused typedefs.

## MCDC-013: One Outcome Platform-Dead, and a Denominator That Grew (lifetime.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-013 |
| **Date**       | 2026-09-02 |
| **Baseline commit** | Canon-C CI #1271 (307307c) |
| **Scope**      | `core/primitives/lifetime.h` — 2 condition outcomes, 1 covered (50.00%), 1 justified; 5/5 executable lines (100.00%) |
| **Category**   | Structural coverage completeness |
| **Enforcement**| Measured in the aggregate MC/DC table; one justification row below |

**Description**: `canon_lifetime_next_id_` contains exactly one condition,
`if (id_ == REGION_ID_STATIC)`. Its FALSE leg is covered 100079 times by
`lifetime_test.c`; its TRUE leg is not covered and takes a justification row.

### The aggregate goes DOWN, and that is correct

`lifetime_test.c` defines `CANON_LIFETIME_DEBUG` itself, so the generator now
compiles in the coverage build, which does not set `CANON_LIFETIME`. Code that
previously contributed NOTHING to the denominator now contributes 2 outcomes
with 1 covered:

| | outcomes | covered | % |
|---|---:|---:|---:|
| before | 2012 | 1795 | 89.215 |
| after  | 2014 | 1796 | 89.176 |

**The headline number falls by 0.04 points because coverage got more honest,
not less complete.** The branch was always there and always uncovered; it was
merely invisible, compiled out of every measured TU. Recorded explicitly so
nobody later "fixes" the regression by removing the test.

The enforced gate is BRANCH coverage at 80%; MC/DC is report-only. Verified
that this does not approach the threshold.

### Justification row — the `REGION_ID_STATIC` guard, TRUE leg

Fires only when `c_ == (uintptr_t)owner_`: the call counter equal to the
owner's address. **Not dead by invariant** — dead by ADDRESS MAP, which makes
it a different kind of justification from MCDC-012's J1–J4, and the difference
is the reason this row is written out rather than cross-referenced.

On the hosted x86-64 runner it needs order 1e14 calls and is unreachable within
any feasible budget. On a low-address embedded target it needs a few thousand
and is ordinary. **The branch is live code on the deployment target and dead
only on the machine that measures it.**

Driving it would require an injectable counter, i.e. reimplementing the
generator in the test — verifying a copy, which the project does not do. The
one-source-of-truth rule in `docs/vmacros.md` governs.

**Cross-stream, and now corroborated rather than assumed.** The proof stream
needs this same branch to be REACHABLE: it is what discharges
`\result != REGION_ID_STATIC` in VERIFY-021. This row was first written with
that consistency FLAGGED as unverified, because WP reported an unnamed
`Unreachable` goal and, had it been this branch, the two streams would have
contradicted each other.

It is not. `-wp-print` at CI #1274 shows `-wp-split` decomposing the
postcondition along this exact guard into goal (1/2) tagged `Then` and (2/2)
tagged `Else`, both Qed-valid. WP proves the postcondition THROUGH the branch
and holds it reachable in the model. CI #1275 closed the follow-up question
too: `-wp-prover none` lists only those two goals, so the summary's
`Unreachable: 1` is not an obligation over any program point and there is no
branch it could have named.

So the two instruments agree, and say different things because they are asking
different questions: **undriveable on the measuring host, reachable in the
model.** That is what makes a justification row the right disposition here
rather than a test — and it is now evidence, not an argument.

This inverts VERIFY-020 E1/F5, where a clause redundant for the LOGIC was
load-bearing for the PROVER. Here a branch dead in EXECUTION on the measuring
host is load-bearing for the PROOF.


## MCDC-014: Twelve Test Gaps, a Zero-Execution Swap Path Misread as a Justification, and Six Public Symbols That Should Never Have Been (priority_queue.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-014 |
| **Date**       | 2026-09-05 |
| **Baseline commit** | Canon-C CI #1280 (`d0b6f2c`). |
| **Scope**      | `data/priority_queue.h` MC/DC: 62/78 (79.5%) → 82/82 (100%) by tests, then → 81/82 (98.8%) after PQ-A. Aggregate 1796/2014 → 1816/2018 → 1815/2018. |
| **Category**   | Coverage completeness; API finding |

**Description**: `priority_queue.h` entered this arc at 79.5% MC/DC with no
enumeration of what the missing 16 outcomes were. The enumeration — a per-line
`gcov-14 --conditions` dump run **locally**, because at the time no CI step
printed it for this file (the coverage job's per-line steps covered fourteen
other files; this one was added with this entry) — turned "79.5%" into twelve
test gaps, one zero-execution code path, and one API finding. Nothing was
justified away.

### Enumeration, then disposition

Twelve of the misses were ordinary test gaps — outcomes a straightforward
test could reach and no test did. Each got a test; each moved. The per-line
dump, not this entry, is the record of which lines they were.

One miss was a whole path: **the large-element swap loop had never
executed.** `pq_swap_` uses `mem_swap` for elements up to `CANON_MEM_SWAP_MAX`
bytes and falls back to a byte loop above it. Every existing test used small
elements. The first reading of this miss was that it was a justification row —
"no realistic element is that large." That reading was **wrong** and was
corrected before it reached the docs: the path is reachable by any caller with
a large element, it is the only code that runs for them, and it had zero
executions. `test_large_elements` now drives it with a 516-byte element (`PqBig`: 512
bytes of padding plus an `int` key, asserted larger than `CANON_MEM_SWAP_MAX`
so the test cannot become vacuous), and the correction is recorded here
because the misreading is exactly the kind the justification mechanism
invites.

The aggregate moved 1796/2014 → 1816/2018: +20 outcomes covered, and the
denominator grew by 4 (78 → 82 for this file) — gcov's condition set is
measured over what the test binary instantiates, and the new tests reach
functions the old suite never called.

### The justification row that IS real — and why coverage then went DOWN

`pq_swap_`'s `if (a == b) { return; }` had been covered by
`test_self_swap_is_a_noop`, which called the then-public `pq_swap` directly
with `a == b`. PQ-A (below) renamed the six heap helpers internal, and with
that the self-swap leg became **dead by construction**: `pq_sift_down_` only
swaps when `smallest != idx`, and `pq_sift_up_` swaps a parent with a child at
`idx > 0`, so a self-swap cannot arise from the heap's own operations.

The test was deleted rather than adapted. Reaching that leg now would mean
calling an internal symbol from a test purely to move a number — the
contrived-driver pattern the project rejects, and the same reasoning that kept
lifetime.h's guard as a justification row in MCDC-013 rather than an injectable
counter. So coverage went **82/82 → 81/82** and aggregate **1816 → 1815**, and
that is the correct outcome. A reader seeing 90.0% then 89.9% in consecutive
runs deserves the reason without digging through commits: the rename made a
branch unreachable, and the honest measurement follows the code.

| Justification | File:line | Outcome | Reason |
|---|---|---|---|
| J1 | `data/priority_queue.h`, `pq_swap_` `a == b` | TRUE leg | Dead by construction after PQ-A: no internal caller passes equal indices. Kept as a guard against future internal callers, not as reachable behaviour. |

### PQ-A: the finding coverage work produced

`pq_swap`, `pq_sift_up`, `pq_sift_down`, `pq_parent`, `pq_left_child` and
`pq_right_child` were plain public symbols. The same header already marked
`pq_lifetime_next_id_`, `pq_lifetime_open_` and `pq_lifetime_restamp_`
internal with a trailing underscore; the convention existed and had not been
applied to the heap internals.

The exposure was not cosmetic. Demonstrated on the pre-rename header: a client
calling `pq_swap(&q, 0, 4)` on a queue holding 1..5 leaves `pq_peek` returning
3 where the minimum is 1. The heap invariant breaks silently — no error, no
diagnostic. And because `pq_swap` deliberately does not restamp the lifetime
token (so a single push or pop produces exactly one id bump), a client call
mutates contents **without** bumping the id: an outstanding borrow keeps
validating against a queue that changed underneath it. The instrument fails
OPEN — the mode VERIFY-021 was written to prevent in the token generator.

Blast radius was checked before renaming, not assumed: no callers outside
the header except one in test/ — `test_self_swap_is_a_noop`, which called
`pq_swap` directly and was deleted rather than adapted (J1 above). No
deprecation shim, because nothing else to deprecate was found. The shipped
change is the rename plus comments.

### Graduation — what exists and what does not

`data/priority_queue.h` is the second file, after `core/primitives/lifetime.h`
(MCDC-013), whose coverage arc is **settled**: every outcome is either covered
or carries a justification row above. Both now have a `Debug: per-line MC/DC
detail` step in the coverage job (added with this entry — neither had one
before, although fourteen other files did), so the exact missed outcome is
printed on every run and can be compared against the J-table by eye.

What does **not** exist, and must not be read into the word "graduated": an
automated allowlist. The per-line steps are report-only, like the fourteen
before them. The intended mechanism — one `file:line` justification register
per settled file, with the coverage job failing on any miss not in it — is
recorded here as future work, to be checked by hand against the per-line dump
before each release cut until it is built. An unexplained miss on a settled
file is a regression in the sense that a human reading the dump should treat
it as one, not in the sense that CI turns red.

### Cross-references

- VERIFY-022 — the verification arc that followed; the same six renamed helpers
  are the ones whose comparator calls became the information horizon.
- MCDC-013 — the justification-row reasoning this entry reuses.
- VERIFY-021 — the failure-open mode PQ-A's exposure shares.

---

## VERIFY-028: Pre-registration — range.h, the Eighteenth Unit: Two Defects Demonstrated Before Any Contract, and a Prediction of Zero New Arguments

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-028 *(renumber if 028 was taken by the loop-annotation pre-registration)* |
| **Date**       | 2026-09-26 |
| **Status**     | PRE-REGISTERED — no contract written, no source changed; findings demonstrated by probe only |
| **Baseline**   | 4e2eb2b (CI #____ — author fills) |
| **Scope**      | `data/range.h` (15 functions, Shape A in place) + a thin interposition driver for `option_isize` |
| **Category**   | Prospective test of the Table 5 saturation claim (module 18); prospective test of prover detection on known defects |

**Why this record exists.** Table 5 reports that the written-argument base
saturated at module 12 (verifier-bound) and module 7 (spec-bound) and held
through every unit after. Every one of those units was verified *before* the
saturation claim was written down. range.h is the first unit verified *after*
it, so it is the first unit that can falsify it. This record commits the
prediction, and the commit hash is the evidence that it predates the contracts.

A second, independent test comes with it. Two defects in range.h were found by
reading and demonstrated by probe (below) before any ACSL was written. This
record predicts **which goals** WP will report for each on the unfixed bodies.
If WP finds exactly those, the tool is corroborated on defects it had no hand
in finding; if it finds others, those are new findings; if it misses one, that
is a gap in the contract, not in the code.

### The unit

15 functions: `range_make`, `range_upto`, `range_from_to`, `range_downfrom`,
`range_downto`, `range_is_empty`, `range_has_next`, `range_is_valid`,
`range_len`, `range_remaining`, `range_peek`, `range_peek_option`,
`range_next`, `range_reset`, `range_skip`. `RANGE_FOR` is a call-site macro:
parked per `docs/vmacros.md`, the `DIAG_*` precedent.

Closure: `types.h`, `limits.h`, `contract.h`, `checked.h`, `option.h`. No
`ptr.h`, no `slice.h`, no `memory.h` — the smallest closure of any data/ unit.
It is **not** zero core-substrate inheritance: checked.h is core substrate in
bitset's own breakdown (58 = bits 15 + memory 20 + slice 13 + ptr 6 +
**checked 2** + borrow 2), and all of checked.h's functions are defined in
the TU, so VERIFY-002's two goals re-emit. range's core-substrate surface is
predicted at 2 — one checked.h pair more than deque's 0.

### Findings demonstrated before any contract

**F1 — `range_len` signed overflow on ranges wider than `ISIZE_MAX`.**
`diff = r->end - r->current` (and its descending mirror) is computed in
`isize`. For any range whose span exceeds `ISIZE_MAX` this is undefined
behaviour. Demonstrated:

    range_make(ISIZE_MIN, ISIZE_MAX, 1)
    UBSan: range.h:337: signed integer overflow:
           9223372036854775807 - -9223372036854775808 cannot be represented
    -O2, no sanitizer: range_len returns 0      (true count: USIZE_MAX)
    range_make(-10, ISIZE_MAX, 1): returns 0    (true count: ISIZE_MAX + 10)

A range the API accepts, reports as non-empty, and iterates correctly is
reported as length zero. The header comment ("diff is a non-negative
isize") states the false premise explicitly, and the `CANON_USIZE_MAX` branch
it guards is dead: `checked_sub_isize(diff, 1)` cannot fail for `diff > 0`.
The exact count *always* fits in `usize` (at most `2·ISIZE_MAX + 1 =
USIZE_MAX`), so the correct fix computes the span as
`(usize)end − (usize)current` (the conversions are value-preserving modulo
2^N and the unsigned subtraction is well defined) and needs no saturation at
all.

**F2 — `range_skip` computes the jump in `isize`, with two failure modes.**

*F2a, reversal.* `(isize)n` is an implementation-defined conversion that
yields a negative value for `n > ISIZE_MAX` on every two's-complement target.
The jump then runs *against* the step. Demonstrated:

    range_make(0, 10, 1); range_skip(&r, SIZE_MAX);
    current = -1, has_next = true, range_next yields -1

A request to skip everything produces a value *before* the start, silently.
Failure-open, the VERIFY-021 / PQ-A mode.

*F2b, premature exhaustion.* No downcast involved: on a range wider than
`ISIZE_MAX`, `n · step` can exceed `ISIZE_MAX` while the target is still
inside the range, and the overflow branch saturates to `end`. Demonstrated:

    range_make(ISIZE_MIN, ISIZE_MAX, 2); range_skip(&r, 2^62 + 1);
    current = ISIZE_MAX (end), has_next = false   (correct: current = 2)

Failure-closed, but wrong: elements the caller did not skip are lost.

Fix: if `n >= range_count`, exhaust. Otherwise the offset `n · |step|` is at
most `span − 1`, so it fits in `usize` but **not necessarily in `isize`** —
comparing against `range_len` first (as an earlier draft of this record
proposed) fixes F2a but not F2b. The target itself lies between `current`
and `end` and is always representable, so apply the `usize` offset in at
most two `checked_add_isize` steps of size ≤ `ISIZE_MAX` each, avoiding any
unsigned-to-signed conversion.

**Test gap.** The existing `range_test.c` passes, including under
ASan/UBSan, on the defective source: no test uses a span wider than
`ISIZE_MAX`. The fix commit adds the three probes above as regression
tests; each fails (or trips UBSan) on the current source.

**F3 — `CANON_OPTION(isize)` is unguarded.** The same wart as bitset's F3,
one step milder: range.h *does* instantiate its own option (bitset once did
not), but with no `CANON_OPTION_ISIZE_DEFINED` guard, so no caller — and no
verification driver — can interpose a contracted instantiation first, and any
second header instantiating `option_isize` in the same TU is a redefinition.
Fix at the source with bitset's guard, not in the driver.

**F4 — `range_next` states its precondition with `ensure_msg`.** Calling
`range_next` on an exhausted range is a caller bug, which contract.h's own
vocabulary assigns to `require_msg`. The difference is not only naming: in
contract.h, `ensure_msg` compiles to nothing under `NDEBUG`, whereas
`require_msg` is removed only by the explicit `CANON_NO_REQUIRE`. So in an
ordinary release build the check vanishes and `range_next` on an exhausted
range returns `r->current` unconditionally — a value outside `[start, end)`
(e.g. `5` from `range_make(5, 3, 1)`). The ACSL contract will say `requires`
regardless; the C should say the same thing.

### Contract design, fixed before the run

So the contracts cannot be tuned to the result afterwards:

1. **Well-formedness** `range_wf(r) := r.step != 0 && r.step != ISIZE_MIN` —
   exactly the header's documented invariants, nothing added. Every function
   that dereferences `r` requires it in its non-null behaviour.
   `range_make` ensures it.
2. **Exact count.** A logic function `range_count(r)` over mathematical
   integers: `0` if empty, else `(span − 1) / |step| + 1`. `range_len` ensures
   `\result == range_count(*r)` over the **full** `isize` domain — that is the
   claim F1 currently makes false.
3. **Consumption, in two named layers.** *Positional* (no division):
   `range_next` ensures `\result == \old(r->current)` and the new `current`
   is `old + step` or `end`; `range_skip` ensures the new `current` is
   `old + n·step` when that is strictly inside the range, else `end`.
   *Count* (division by a variable): `range_next` ensures
   `range_count(*r) == \old(range_count(*r)) − 1`; `range_skip` ensures
   `range_count(*r) == max(0, \old(range_count(*r)) − n)`. Each count claim
   is a separately named `ensures`, so its goal can be isolated.
   **Decision rule, fixed now:** if a count ensures does not prove, it is
   **kept and argued**, not weakened or dropped. P1's outcome must not be
   decided by editing the specification after the run.
4. **Not claimed:** the value sequence as a whole (no ghost model of
   iteration); anything about `RANGE_FOR`.
5. **Dead branch left in place:** `range_is_empty`'s `return true` for
   `step == 0` is unreachable under `range_wf`. Kept (defensive, and the
   struct is public), recorded as a predicted MC/DC justification row.

### Run plan

| Run | Source state | Mode |
|-----|--------------|------|
| 1 | contracts + F3 guard + driver; **F1, F2 unfixed** | report-only |
| 2 | F1, F2, F4 fixed + regression tests; MC/DC measured (range.h has no MCDC record yet — every data/ unit since bitset ran both streams in one arc) | report-only |
| 3–5 | unchanged | report-only, name-stability arc |
| 6 | pinned | enforced |

Run 1 on the unfixed bodies is deliberate: it is the detection test. F3 must
land in run 1 because the driver cannot interpose without it.

### Predictions (author — confirm or amend before committing)

| # | prediction | falsified if | confidence |
|---|------------|--------------|------------|
| P1 | **Zero new `**Manual proof argument**` blocks.** Cumulative *A* stays 17; *V* and *S* both unchanged. Equivalent to: the two count ensures prove (P6). | any range.h residual not covered by an existing block — in practice, an unproved count ensures | M–L (tied to P6) |
| P2 | Inherited residuals = **36**, byte-identical to documented sets: contract.h handler 2 + checked.h 2 (VERIFY-002) + option 32 (modulo the `isize` prefix, bitset's option_usize precedent). | any inherited count ≠ 36, or a name not in the documented sets | H |
| P3 | Run 1 reports **F1** as exactly one unproved `signed_overflow` RTE goal at each of the two span subtractions in `range_len` (2) **and nothing else**: WP inserts each RTE check as an assertion and assumes it in every later goal, so `range_len`'s count ensures is predicted to **prove** in run 1 — F1 is visible only as the two RTE goals. The `-r->step` negation's RTE goal proves under `range_wf`. | either subtraction's goal proves, the count ensures fails, or any other `range_len` goal fails | H |
| P4 | Run 1 reports **F2** as an unproved *positional* ensures on `range_skip` (F2b violates it with no cast involved, so this holds whatever the job's RTE flags). The job passes no `-warn-signed-downcast`; whether a downcast RTE goal also appears for F2a depends on the Frama-C 29 default, **not checked** — its presence or absence is recorded either way, not scored. | `range_skip`'s positional ensures proves on the unfixed body | H (ensures) |
| P5 | Run 1: **no other own goal fails.** The twelve functions other than `range_len`, `range_next`, `range_skip` prove outright, and `range_next`'s positional ensures proves. | any own failure outside `range_len`'s two RTE goals, `range_skip`, and the two count ensures | M |
| P6 | Run 2 (fixed): own residuals = **0**, *or* only the two count ensures (≤ 2 goals, class (a), division by a variable). **No existing block covers them**: VERIFY-002's checked.h pair is unsigned-add wraparound and prose-argued (outside the 17), and none of the 17 blocks argues division. So P6's second branch **falsifies P1**. | own residuals > 2, or of any other class | M–L |
| P7 | `range_wf` requires no strengthening beyond the header's two documented invariants. | any goal that needs a third invariant to prove | M |

**What each outcome means for the paper.** P1 holding is the first
*prospective* data point for saturation — every point in Table 5 so far is
retrospective. P1 failing is equally reportable: it gives the curve its first
post-saturation increment, with a named cause (division by a variable, a
verifier-bound *V* argument). Note what the test actually hinges on: P1 is
decided by two goals, both in a class the argument base has never seen, and
the decision rule above forbids resolving it by weakening the spec. That is
what makes it a real test rather than a formality. P3/P4 holding means the
tool confirmed human-found defects at predicted locations; the paper can say
that without overclaiming that the tool *found* them.

### Scoring

VERIFY-029 scores this record goal by goal after run 2, and again at
enforcement.

---

## VERIFY-029: range.h Runs 1–2 Scored — Inheritance Held Exactly, Own Proofs Did Not; a Buffer Overflow Downstream; Run 3 Pre-Registered

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-029 |
| **Date**       | 2026-09-28 |
| **Status**     | SCORES runs 1–2 of VERIFY-028 (bd1da17); PRE-REGISTERS run 3 |
| **Commits**    | pre-registration bd1da17 · run 1 c9138f4 (CI #1312) · run 2 source 4cf7f90 (CI #1313, build failed — see below) · run 2 evidence 5e778b7 (CI #1314) |
| **Scope**      | `data/range.h` (module 18), plus `data/vec/vec_range.h` (F5) |
| **Category**   | Prospective scoring; new finding; pre-registration of a remediation run |

This record has three parts, and they must not be read out of order. Part 1
scores VERIFY-028 against runs 1 and 2 exactly as it was committed — no
prediction is reinterpreted. Part 2 records a finding VERIFY-028 did not
predict. Part 3 pre-registers run 3, and is committed **before** any run-3
code reaches the repository; its commit hash is the evidence for that.

---

### Part 1 — VERIFY-028 scored

**Run 1** (c9138f4, unfixed bodies, report-only). 2504 goals, 48 unproved:
36 inherited, 12 own.

| # | prediction | result |
|---|------------|--------|
| P2 | inherited = 36 (handler 2 + checked 2 + option 32), documented names | **HELD, exact.** Checked's two are VERIFY-002's `checked_add_overflow_ensures` / `checked_add_u64_overflow_ensures`. |
| P3 | F1 appears as exactly 2 `signed_overflow` RTE goals in `range_len`; `len_count` **proves** | **HELD, both halves.** WP assumes each RTE assertion in later goals, so F1 is visible only as the two RTE goals (`…_signed_overflow_3_part1`, `…_5_part2`). |
| P4 | F2 appears as unproved `skip_inside` (F2b) | **HELD.** Also `skip_exhaust` unproved — see below. |
| P4 | signed-downcast RTE alarm (recorded, not scored) | **Absent.** Frama-C 29 raises none under this job's flags. |
| P5 | no own goal fails outside `range_len` / `range_skip` / count goals | **HELD.** Twelve functions prove outright; `range_next` fully proves. |
| P7 | `range_wf` needs nothing beyond the two documented invariants | **HELD** (runs 1 and 2). |

*F2 split by postcondition.* With no downcast alarm, WP models the
`(isize)n` conversion rather than assuming it away, so F2a (reversal for
`n > ISIZE_MAX`) falsifies `skip_exhaust` and F2b (premature exhaustion)
falsifies `skip_inside`. Each half of the defect surfaced under its own named
ensures. Splitting the positional claim into two named clauses (VERIFY-028
contract design item 3) is what made that visible.

*Wording constraint for the paper.* These are timeouts, not counterexamples.
The supportable claim is: **every own goal WP failed to prove in run 1 is one
that the independently demonstrated defects make false, and none outside
them.** Not "WP detected F1 and F2" — the defects were found by reading and
probing first.

**Run 2.** First pushed at 4cf7f90; the build failed on two test-portability
errors in the new regression tests (a 62-bit shift on the 32-bit job; an unused
`static inline` in the clang fuzz build), so `frama-c-range` (`needs: build`)
did not run. Fixed in 5e778b7, touching only `test/`. `range.h` and
`vec_range.h` are byte-identical between 4cf7f90 and 5e778b7; **run 2's
evidence is from 5e778b7.** 2475 goals, 47 unproved: 36 inherited, 11 own.

| # | prediction | result |
|---|------------|--------|
| P2 | inherited 36 | **HELD, and name-identical to run 1** (two of the three name-stability runs). |
| P5 | nothing own outside `range_len` / `range_skip` / count goals | **HELD.** |
| P6 | run 2 own residuals = 0, or ≤ 2 (the count ensures), class (a) | **FALSIFIED — 11.** |
| P1 | zero new argument blocks | **Scored at enforcement** (VERIFY-028 "Scoring"). After P6's failure it survives only if run 3 closes all own goals. |

Also from run 2: `skip_exhaust` now proves; `next_count` — a
division-by-variable goal — proved unaided in both runs. The class is not
uniformly hard.

**Diagnosis of the 11.** None is a defect: the regression tests pass on
GCC 64-bit, GCC `-m32` (Release and Debug), clang, and the clang fuzz build,
and a differential test (Part 3) agrees with exact 128-bit arithmetic. Two
causes, with the evidence for each:

- **Group A — 3 goals, `range_len`** (`assert_rte_division_by_zero_part4`,
  `nonnull_ensures_len_count_part3`, `…_part6`). Run 2 computed
  `|step| = (usize)0 − (usize)step` and `span = (usize)end − (usize)current`:
  correct, but correct *because of* unsigned wraparound, which WP models as
  reduction modulo 2^N. Evidence: `len_count` **proved in run 1**, where the
  same division was taken over an `isize` difference; only the span
  computation changed between runs. Confidence: high.
- **Group B — 8 goals, `range_skip`** (four `signed_overflow` RTE goals,
  `skip_inside` ×2, `skip_count` ×2). Each needs
  `n < count ⟹ n·|step| ≤ span − 1`, i.e. `k·(a/k) ≤ a` with monotonicity —
  multiplication meeting division. This is the class VERIFY-028 flagged (P6
  confidence M–L). Confidence: high that this is the obstacle; the exact
  split-part mapping is not established.

**Methodological finding.** Group A was introduced by a MISRA-motivated
choice: the run-2 fix wrote `(usize)0 − (usize)step` rather than
`(usize)(−step)` specifically to avoid a Rule 10.8 composite-expression cast. The code became
MISRA-clean and correct, and simultaneously unprovable by the deductive
verifier. Two assurance tools pulled the same line in opposite directions.
Run 3 shows the tension is resolvable (a named temporary satisfies both), but
it had to be noticed first.

**MC/DC** (GCC 14 `-fcondition-coverage`, CI job). `range.h`: 81.7% (49/60)
at run 1 → **92.3% (48/52)** at run 2. The denominator changed because the
branch structure changed; the regression tests reached outcomes nothing
reached before. Four missed outcomes remain for the MCDC record.

**MISRA.** 53 real violations at runs 1 and 2 = the pinned 53.

---

### Part 2 — F5: a buffer overflow downstream of F1 (not predicted)

`vec_extend_from_range` (`data/vec/vec_range.h`) checked capacity against
`range_len(&r)` and then wrote **until the range was exhausted**, not up to
that count. Under F1, `range_len` returned 0 for a range wider than
`ISIZE_MAX`, the capacity check passed, and the loop wrote past the buffer.
Demonstrated before the fix, through the public API only:

    range_make(ISIZE_MIN, ISIZE_MAX, ISIZE_MAX)   /* 3 elements */
    into a vec of capacity 1
    ASan: stack-buffer-overflow, WRITE of size 8

F1 is therefore a memory-safety defect for any consumer that trusts
`range_len` — not a wrong-answer defect. `vec_range.h` had no tests at all.
Fixed at run 2 on both sides: F1 makes the count exact, and the loop is now
bounded by the same `count` the capacity check used, so the check and the
writes agree by construction whatever `range_len` returns. Regression tests
in `vec_test.c` fail (ASan) on the old source and pass on the new.

`vec_range.h` is not in any WP translation unit; F5 is a finding of this arc,
not of the prover.

---

### Part 3 — Run 3, pre-registered

**What changes.** `range_len` and `range_skip` bodies only, plus two lemmas
in the ACSL logic layer. **The 15 function contracts are byte-identical to
runs 1 and 2** (checked mechanically: of the 16 ACSL blocks, only block 0, the
logic layer, differs).

- `|step|`: negate in `isize` (safe under `range_wf`), then widen —
  `const isize neg_step = −r->step; abs_step = (usize)neg_step;`
- span, by endpoint signs, never wrapping: same sign → `hi − lo` exact in
  `isize`; `lo < 0 ≤ hi` → `(usize)hi + (usize)(−(lo+1)) + 1`, each part in
  `[0, ISIZE_MAX]`, sum ≤ `USIZE_MAX`.
- lemmas, each proved by WP as its own goal (no trusted axiom):
  `range_mul_le`: `a ≥ 0 ∧ k > 0 ∧ 0 ≤ n ≤ a/k ⟹ n·k ≤ a`;
  `range_div_shift`: `a ≥ 0 ∧ k > 0 ∧ n ≥ 0 ∧ n·k ≤ a ⟹ (a − n·k)/k = a/k − n`.
- one new test, `test_len_sign_cases`: the all-negative case (`lo < 0`,
  `hi < 0`) is the only way to show `hi < 0` independently flips the new
  decision `(lo >= 0) || (hi < 0)`; no existing test had one.

**Checks already done** (none is a substitute for WP):
- both lemmas valid in Z3 (integer arithmetic; ACSL `/` truncates, which
  equals floor division on the lemmas' non-negative domain);
- differential test: 21,055,064 `range_skip` cases and every `range_len`
  against exact 128-bit reference arithmetic, weighted to the extremes —
  0 mismatches, 0 UBSan reports;
- full local matrix: GCC 64-bit 54/54, GCC `-m32` Release 54/54 and Debug
  54/54, clang fuzz build (40 targets), clang unit tests under ASan/UBSan,
  driver compiles under WP's flags;
- MISRA (Cppcheck 2.13.0, the CI's version): rule-for-rule identical to run 2
  on `range.h`; no Rule 10.8.

**Rules fixed now.**
1. **Run 3 is the last provability rewrite.** Whatever own goal remains
   unproved after run 3 is kept and argued (VERIFY-028 decision rule), not
   rewritten again. This closes the garden of forking paths: without it,
   "zero arguments" could always be reached by iterating until the prover
   agrees.
2. **How lemmas count for P1.** A lemma WP proves adds no trusted
   assumption and is not an argument block. A lemma WP does not prove is a
   residual like any other, and needs one.
3. **What P1 may claim if it holds.** "Zero new argument blocks" — true — but
   never "unaided": the record states that P6 failed and that a body rewrite
   plus two lemmas were required. Table 5's module-18 row carries that
   footnote.

**Predictions (author — confirm or amend before committing).**

| # | prediction | falsified if | confidence |
|---|------------|--------------|------------|
| Q1 | inherited = 36, **name-identical** to runs 1–2 (third stability run) | any change in count or names | H |
| Q2 | both lemmas prove | either `typed_lemma_range_*` unproved | M |
| Q3 | group A closed: 0 unproved goals in `range_len` | any `range_len` goal unproved | M–H |
| Q4 | group B closed: 0 unproved goals in `range_skip` | any `range_skip` goal unproved | M–L |
| Q5 | no own goal unproved outside `range_len` / `range_skip` / lemmas | any other own goal | H |
| Q6 | own total = 0 (so P1 survives to enforcement) | own ≥ 1 | M–L |
| Q7 | MC/DC: `range.h` has no missed outcome *in the rewritten `range_len` decision* | a missed outcome there | M |

Q4 is the weak point, as P6 was: the lemmas give the provers the right facts,
but they must still instantiate them against terms involving the modelled
`usize` multiplication. If Q4 fails, rule 1 applies — the remainder is argued,
and the curve records module 18 as its first post-saturation increment, with
this record as the explanation.

---

## VERIFY-030: range.h Enforced — Zero Own Residuals, 36 Inherited by Name, and P1 Survives Its First Prospective Test (Not Unaided)

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-030 |
| **Date**       | 2026-09-29 |
| **Status**     | ENFORCED — CI #1317 / 9f7a9d0 |
| **Pinned from**| CI #1315 (c3c4e64) and #1316 (fa05034), name-identical; #1317 the third, name-identical |
| **Scope**      | `data/range.h` (module 18), `frama-c-range` job |
| **Scores**     | VERIFY-028 P1 (final); VERIFY-029 Q1–Q7 |
| **Related**    | VERIFY-028 (pre-registration), VERIFY-029 (runs 1–2 scored, run 3 pre-registered), MCDC-015 |

This closes the range.h arc. It scores what VERIFY-028 and VERIFY-029 left
open and records the enforced state. Nothing in either earlier record is
reinterpreted.

### The enforced state

`frama-c-range` pins `2611 / 2647` and the 36 unproved goals **by name, as a
set**, and fails on any Failed or Invalid goal, on a pinned goal that starts
proving (ratchet), or on any new unproved goal (investigate). Each failure
mode was dry-run against a synthetic log before the pin was committed.

| Class | Goals | Owner | Prior record |
|-------|-------|-------|--------------|
| option_isize combinators (function-pointer dispatch: terminates / exits / assigns / `\valid_function`) | 32 | option (inherited) | VERIFY-014; the same 32 bitset pins as `option_usize` (VERIFY-020) |
| `contract_default_handler` (abort path) | 2 | contract.h (inherited) | VERIFY-002 class |
| `checked_add` / `checked_add_u64` `overflow_ensures` | 2 | checked.h (inherited) | VERIFY-002 |
| **range.h own** | **0** | — | — |

`tools/record_coverage.py` counts the 36 among the pinned names that records
list by class rather than goal by goal (as for option, bitset and
priority_queue), and reports 0 stale goals, 0 empty blocks, 0 heading drift,
and **17 argument blocks — unchanged**.

**Name stability.** #1315, #1316 and #1317 report the same 36 names. Two
details that are not instability: the two `contract_default_handler` lines
swap order between runs (the pin compares sorted sets), and which prover
closes a given proved goal shifts run to run (e.g. CVC5 11 → 6). Neither
changes the proved set, which is why the pin records names, not provers.
#1317 changed `range.h` in comments only (every code line and all 16 ACSL
blocks byte-identical to #1316), so it counts as the third unchanged run.

### Scores

| # | prediction (record) | result |
|---|---------------------|--------|
| Q1 | inherited 36, name-identical (029) | **HELD** — three runs |
| Q2 | both lemmas prove (029) | **HELD** — neither appears unproved; inferred from absence, since WP lists only unproved goals, plus a clean parse |
| Q3 | 0 unproved in `range_len` (029) | **HELD** |
| Q4 | 0 unproved in `range_skip` (029, M–L) | **HELD** |
| Q5 | nothing else own (029) | **HELD** |
| Q6 | own total 0 (029) | **HELD** |
| Q7 | no missed MC/DC outcome in the rewritten `range_len` decision (029) | **HELD** — `(lo >= 0) \|\| (hi < 0)` 4/4 (MCDC-015) |
| P1 | zero new argument blocks (028) | **HELD at enforcement — with the VERIFY-029 footnote** |

**How P1 must be reported.** Zero new argument blocks is true; cumulative
*A* stays 17, *V* and *S* unchanged. It is **not** true that module 18 closed
unaided: VERIFY-028 P6 was falsified at run 2 (11 own residuals), and closure
took one body rewrite (no unsigned wraparound in `range_len`/`range_skip`) and
two lemmas that WP proves as goals of their own, under the rule fixed in
VERIFY-029 that run 3 was the last provability rewrite. The Table 5 row for
module 18 carries that footnote; the saturation claim survives its first
prospective test, and the record shows what it cost.

Own residuals across the arc: **12 (run 1) → 11 (run 2) → 0 (run 3)**.

### For Table 5 (module 18)

| Quantity | Value |
|----------|-------|
| Module | `data/range.h` (Shape A, in place; thin interposition driver for `option_isize`) |
| Functions | 15 |
| Goals | 2611 / 2647 proved |
| Residuals | 36 — 36 inherited, 0 own |
| New argument blocks | **0** (cumulative A = 17) |
| First post-saturation module | yes — prospective; P1 held, P6 failed (footnote above) |

### Findings of the arc (for the summary)

| ID | Finding | Kind | Status |
|----|---------|------|--------|
| F1 | `range_len` signed overflow for spans > ISIZE_MAX; observed returning 0 | UB / wrong count | fixed (run 2), proved (run 3) |
| F2a | `range_skip` with `n > ISIZE_MAX` reversed direction | wrong result | fixed, proved |
| F2b | `range_skip` exhausted wide ranges early | wrong result | fixed, proved |
| F3 | unguarded `CANON_OPTION(isize)` blocked interposition | structural | fixed (run 1) |
| F4 | `range_next` precondition stated with `ensure_msg` (off under NDEBUG) | contract vocabulary | fixed (run 2) |
| F5 | `vec_extend_from_range` buffer overflow downstream of F1 (ASan) | **memory safety** | fixed (run 2), regression-tested |
| M1 | a MISRA-motivated wraparound form made correct code unprovable | method | resolved (run 3) |

F1–F4 were found by reading and probing before any contract; F5 by tracing
F1's consumers. The prover found none of them first. What it did was fail
exactly on the goals those defects falsify (run 1), then confirm the fixes.

---

## MCDC-015: A Wide-Range Blind Spot, Two Guard Gaps, and One Outcome Dead by Construction (range.h)

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-015 |
| **Date**       | 2026-09-29 |
| **Baseline commit** | Canon-C CI #1312 (`c9138f4`), before any fix or new test. |
| **Final commit** | Canon-C CI #1317 (`9f7a9d0`). |
| **Scope**      | `data/range.h` MC/DC (from `range_test`): 49/60 (81.7%) → 48/52 (92.3%) → 58/62 (93.5%) → **61/62 (98.4%)**. Aggregate 1815/2018 → **1827/2020 (90.4%)**. |
| **Category**   | Coverage completeness; run alongside VERIFY-028/029/030 |

**Description**: `range.h` had a test suite that passed — under ASan and
UBSan — on source containing F1 and F2 (VERIFY-028): no test used a range
wider than `ISIZE_MAX`. Coverage said 81.7% and could not have said more,
because MC/DC measures which outcomes the tests reach, not which input
regions they sample. The two numbers answer different questions, and both
were needed.

Until this entry, no CI step printed per-line MC/DC for `range.h`; the
coverage job's seventeenth per-line step was added for it (fa05034). The
denominator changed twice because the code did — the F1/F2 fixes (run 2) and
the no-wraparound rewrite (run 3) — so the percentages across stages are not
a single series. The per-stage numerators are.

### Stages

| Stage | Commit | Outcomes | What moved it |
|-------|--------|----------|---------------|
| baseline | c9138f4 | 49/60 | — |
| run 2 | 5e778b7 | 48/52 | F1/F2 removed branches; wide-range regression tests (F1, F2a, F2b) reached new ones |
| run 3 | c3c4e64 → fa05034 | 58/62 | sign-split span added the decision `(lo >= 0) \|\| (hi < 0)`; `test_len_sign_cases` added the all-negative case, the only way to show `hi < 0` independently decides it |
| final | 9f7a9d0 | **61/62** | G1, G2 below |

### Disposition of the four misses at fa05034

| # | Line (source) | Missed | Disposition |
|---|---------------|--------|-------------|
| G1 | 404 `return r && !range_is_empty(r)` | both conditions' TRUE outcome | **test gap** — `range_is_valid` had never been called on a valid, non-empty range. Closed: `test_mcdc015_gaps`. |
| G2 | 544 `!r \|\| !out \|\| range_is_empty(r)` | `!out` TRUE | **test gap** — `range_peek` had never been called with `out == NULL` and a real range. Closed: `test_mcdc015_gaps`. |
| J1 | 363 `if (r->step < 0)` | FALSE | **justified — dead by construction.** FALSE here means `step == 0`, which `range_make` never produces (0 is normalised to 1; `ISIZE_MIN` is rejected). Predicted in VERIFY-028 design item 5 and kept deliberately: the struct is public, and `range_empty` mirrors the arm in ACSL. Reaching it would mean building a `range` by hand in violation of the documented invariant, to move a number. |

**Final**: 61/62, one justification row (J1). Any other miss printed by the
per-line step is a regression.

### Note on the second `range.h` row

`range.h` is also compiled into `vec_test` through `vec_range.h` (F5's
regression tests), which exercises only the functions `extend_from_range`
calls: that TU reports 16/32. The summary table takes the per-file maximum,
and the per-line step reads `range_test` — the unit's own tests — as every
other per-line step does.

---

## VERIFY-031: Pre-registration — stringbuf.h, the Nineteenth Unit: Three Defects Demonstrated Before Any Contract, One the Prover Cannot See

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-031 |
| **Date**       | 2026-10-02 |
| **Status**     | PRE-REGISTERED — no contract written, no source changed; findings demonstrated by probe only |
| **Baseline**   | 21fe8d9 (CI #____ — author fills) |
| **Scope**      | `data/stringbuf.h` (29 functions in the verified configuration, Shape A in place, no driver) + one `__FRAMAC__`-gated trusted `vsnprintf` axiom |
| **Category**   | Second prospective test of the Table 5 saturation claim (module 19); prospective test of prover detection on known defects; first test of a defect class the trusted base is predicted to hide |

**Why this record exists.** range.h (VERIFY-028/029/030) was the first unit
verified after the saturation claim was written down. P1 held there, but not
unaided: run 3 needed two lemmas. One prospective point is not a trend.
stringbuf.h is the second, and it differs from range.h on every axis that
might matter: memory-heavy rather than arithmetic, a deep core closure
(arena.h) rather than the shallowest one, libc string and formatting calls,
a loop, and the project's first variadic *definitions*. If saturation is a
property of the method rather than of range.h, it should hold here too. This
record commits the prediction; its commit hash is the evidence that it
predates the contracts.

The second test is sharper than VERIFY-028's. Of the three defects below, the
record predicts the prover will report two at named goals and will **not**
report the third, and says why in advance.

### The unit

29 functions in the verified configuration (`CANON_LIFETIME_DEBUG` off — the
borrow.h precedent, OWN-001 §7): `stringbuf_lifetime_open_`,
`stringbuf_lifetime_close_` (empty bodies in this configuration),
`stringbuf_init_arena`, `stringbuf_init_buffer`, `stringbuf_close`,
`_stringbuf_append_bytes`, `stringbuf_append`, `stringbuf_append_str`,
`stringbuf_append_char`, `stringbuf_append_fmt`, `stringbuf_append_fmt_va`,
`stringbuf_append_n`, `stringbuf_str`, `stringbuf_as_str`,
`stringbuf_as_bytes`, `stringbuf_as_cbytes`, `stringbuf_buffer_bytes`,
`stringbuf_buffer_cbytes`, `stringbuf_as_borrowed_str`,
`stringbuf_as_borrowed_bytes`, `stringbuf_buffer_as_borrowed_bytes`,
`stringbuf_len`, `stringbuf_capacity`, `stringbuf_remaining`,
`stringbuf_is_empty`, `stringbuf_is_full`, `stringbuf_is_arena_backed`,
`stringbuf_clear`, `stringbuf_truncate`. `stringbuf_lifetime_next_id_`
exists only under `CANON_LIFETIME_DEBUG` and is outside the configuration.
`stringbuf_printf` is a name alias (`#define`), with no body of its own.

Closure: types, limits, contract, ptr, checked, memory, arena, slice,
ownership, borrow. No option or result instance — the first data/ unit since
deque with none. The core closure is arena.h's exactly, plus borrow.h.
Model: **Typed+Cast** (the header casts `char*` ↔ `u8*` itself). Job:
`frama-c -wp` directly on `data/stringbuf.h` with `-I semantics` added to
arena's include set; no driver is needed because nothing must be interposed.

### Findings demonstrated before any contract

All probes: GCC 13.3 and 14.2, glibc 2.39; musl 1.2.4 via `musl-gcc`. The
existing `stringbuf_test.c` passes on all of it, including under ASan/UBSan.

**G1 — a documented-valid view, appended back, reaches `memcpy` with
overlapping regions.** The header says a view captured before
`stringbuf_clear`/`stringbuf_truncate` "still points at valid memory".
Appending such a view after a truncate makes source and destination overlap.
`_stringbuf_append_bytes` passes them to `mem_copy`, whose own precondition
forbids overlap. The only statement of that requirement in stringbuf.h is the
internal helper's `@pre`. No public function states it.

    stringbuf_append(&sb, "report.txt");
    str_t all = stringbuf_as_str(&sb);       /* [0,10) */
    stringbuf_truncate(&sb, 6);
    stringbuf_append_str(&sb, all);          /* src [0,10), dst [6,16) */

    default / NDEBUG build:  contract abort in mem_copy
                             ("mem_copy: regions overlap — use mem_move")
    verified configuration (-DCANON_NO_REQUIRE), ASan:
                             memcpy-param-overlap at stringbuf.h:437

`stringbuf_append` and `stringbuf_append_n` reach the same call with a stale
`const char*`. The program satisfies every precondition the public API
states. In ordinary builds it then aborts inside a library-internal callee.
In the verified configuration it has undefined behaviour. **Fix:** call
`mem_move` — the contract message already says so — and delete the helper's
separation `@pre`. The alternative, adding a separation precondition to
three public functions, would make callers carry an obligation the
documentation tells them they do not have. Both fmt functions write in place
and are not affected; G5 covers them.

**G2 — `vsnprintf`'s size argument is the whole remaining capacity; on a
POSIX-conforming libc every formatted append then fails.** POSIX requires
`vsnprintf` to fail with `EOVERFLOW` when `n > INT_MAX`. glibc does not
enforce this; musl does. Both fmt functions pass `sb->capacity - sb->len`:

    StringBuf over a (usize)INT_MAX + 16 byte buffer
    stringbuf_append(&sb, "x=");  stringbuf_append_fmt(&sb, "%d", 42);
    glibc:  ok=1/1  "x=42"
    musl:   ok=1/0  "x="        (fails; buffer correctly unchanged)

The failure is closed (nothing is written), but it is permanent. Once a
StringBuf has more than 2 GiB free, no formatted append succeeds on musl, a
libc that embedded targets actually use. **Fix:** pass `needed + 1`. The
measure pass has already checked that this fits. It is at most `INT_MAX + 1`
only when the output itself is `INT_MAX` bytes long (recorded, not fixed).
It also shrinks the write frame from `[len, capacity)` to exactly
`[len, end]`.

**G3 — the documented failure guarantee does not hold on the write pass's
failure path.** "On failure the buffer is unchanged and remains
null-terminated." On the pass-2 failure branch
(`written < 0 || written != needed`), `vsnprintf` has already written from
`data[len]` onward. The function returns `false` with `len` unchanged and
`data[len]` overwritten. In defined behaviour on glibc this branch is
unreachable: two passes with identical arguments agree. Before G2's fix it
is reachable on musl, but musl fails before writing anything. The only
demonstration available goes through undefined behaviour (G5):

    format string stored in the buffer's stale tail, then appended
    glibc:  ok=0 len=1 strlen=11 data[len]='1'    (invariant broken)
    musl:   ok=1 len=10 "i1234567AB"              (UB resolves differently)

This is recorded as a **specification gap the code does not close**, not as
a demonstrated defect in defined behaviour. **Fix:** restore
`data[len] = '\0'` inside the existing failure branch (no new condition), and
reword the guarantee to the string value `data[0 .. len]`. Bytes past the
terminator are scratch on this path.

**G4 — structural, not a defect: two copies of one body.**
`stringbuf_append_fmt` re-implements `stringbuf_append_fmt_va` line for line
(18 MC/DC outcomes of duplicated logic). After the fixes, `append_fmt`
becomes `va_start; r = stringbuf_append_fmt_va(sb, fmt, args); va_end;
return r;`. This is portable C99, since `_va` already `va_copy`s both
passes. The result is one body to prove and one to cover, and the variadic
*definition* becomes a branch-free wrapper (see Q1).

**G5 — an unstated `restrict` precondition.** `vsnprintf`'s `s` and
`format` are `restrict`-qualified, and overlapping copies are undefined
(C11 7.21.6.5). A format string, or a `%s` argument, that points into the
region being written is undefined behaviour. G3's probe uses exactly this.
Pointing into the *current* string `[0, len)` is fine, so
`stringbuf_append_fmt(&sb, "%s", stringbuf_str(&sb))` is legal. Pointing
into the stale tail `[len, capacity)` is not. **Fix:** state it in both fmt
functions' documentation and in their contracts. The ACSL can bind `fmt`
only, because untranslated variadic arguments are invisible to the axiom
(the VERIFY-017 limit). The `%s` half stays documented and unverified.

**Also found, not defects** (each becomes a *Not claimed* line below):
`stringbuf_append_char(sb, '\0')` and any `str_t` containing NUL produce
embedded NULs, so `strlen(data) == len` is not an invariant. The header never
claims it, and the record will not either. The measure pass's failure branch
(`needed_i < 0`) is reachable in tests: `"%ls"` with a non-ASCII wide string
in the C locale returns −1 on both libcs, and the buffer is unchanged
(probe-confirmed). This differs from diag.h's MCDC-009 outcome 2. There the
format strings are fixed and the caller cannot inject `%ls`.

### Contract design, fixed before the run

1. **Well-formedness** `stringbuf_wf(sb)`, exactly five clauses:
   `data != \null`; `len < capacity`;
   `\valid(data + (0 .. capacity − 1))`;
   `\separated(sb, data + (0 .. capacity − 1))`; `data[len] == '\0'`.
   The first, second and fifth are the header's documented invariants. The
   third and fourth are its ownership paragraph (the buffer is borrowed and
   must outlive the StringBuf) in memory-model form. The header's third
   invariant ("if `arena != NULL`, data came from that arena") is
   **not encoded**: it is provenance, not a property of the state.
2. **Zero state.** Every function documented as NULL-safe or
   "uninitialised-safe" gets a behaviour for `sb == \null || sb->data ==
   \null` with the documented result. The field readers (`len`, `capacity`,
   `remaining`, `is_empty`, `is_full`, `is_arena_backed`) require only
   `sb == \null || \valid_read(sb)`.
3. **Appends.** The shared clause `failure_unchanged`:
   `!\result ==> len == \old(len)` and `data[0 .. len]` unchanged. On
   success: `wf` preserved, prefix `data[0 .. \old(len) − 1]` unchanged, and
   for the byte appends `len == \old(len) + k`. **Content is not claimed for
   the byte appends.** `mem_copy`/`mem_move` assign without an ensures on
   contents, which is memory.h's specification-strength cap (the VERIFY-020
   F4 family), inherited rather than worked around. `stringbuf_append_char`
   writes directly, so it claims `data[\old(len)] == c`. The fmt appends
   claim length growth and terminator only; formatting is opaque.
4. **Run-1 preconditions are the documentation's, verbatim.**
   `_stringbuf_append_bytes` carries its own `@pre` as
   `\separated(src + (0 .. add_len − 1), data + (0 .. capacity − 1))`. The
   public appends and both fmt functions carry no separation precondition,
   because their documentation states none. G1 and G5 are therefore visible
   to the prover in run 1, if it can see them.
5. **The trusted axiom**, `__FRAMAC__`-only and assumed, never proved
   (diag.h's pattern):

        requires fmt_nonnull: format != \null;
        requires valid_buf:   \valid(s + (0 .. n − 1));        // empty when n == 0
        requires sep:         \separated(s + (0 .. n − 1), format);
        assigns  s[0 .. n − 1];
        ensures  term: 0 <= \result < n ==> s[\result] == '\0';

   `term` is ISO C's guarantee for a complete conversion, and nothing more:
   it says nothing for `\result < 0`, and nothing that rules out failure. It
   is therefore true on glibc *and* musl. That is a stated design rule: the
   trusted base asserts only the intersection of the libcs the project
   claims. It is also **weaker** than diag.h's unconditional `term`, which
   rests on an environmental assumption this one does not need. `sep` binds
   the format's first byte. Separating the whole string would need the
   strlen axiomatics in a trusted clause, which is the diag lesson's noise
   source, and one byte is enough to make G5 visible. Trusted axioms go from
   2 to 3. This is recorded separately from *A*: an assumption is not an
   argument.
6. **Decision rule (VERIFY-028's, restated).** An own goal that does not
   prove is **kept and argued**, never weakened or dropped. P1 must not be
   decided by editing the specification after the run.
7. **Not claimed:** byte content of any append except `append_char`;
   `strlen(data) == len`; arena provenance; anything under
   `CANON_LIFETIME_DEBUG` (runtime-verified by `borrow_test.c` Phase 5, as
   for borrow.h); that a formatted append succeeds; the `%s`-argument half of
   G5.

### Run plan

| Run | Source state | Mode |
|-----|--------------|------|
| 1 | contracts + axiom + `frama-c-stringbuf` job; **G1, G2, G3, G5 unfixed**, G4 not applied | report-only — the detection test |
| 2 | G1–G5 applied; regression tests (G1 probe; the `%ls` measure failure); MC/DC tests (below); MC/DC measured | report-only |
| 3–4 | unchanged | report-only, name-stability |
| 5 | pinned | enforced |

G2 has no glibc regression test: glibc passes before and after the fix. Its
evidence is the musl probe in this record plus the code change. A musl CI leg
is out of scope here and noted as follow-up.

### Predictions (author — confirm or amend before committing)

| # | prediction | falsified if | confidence |
|---|------------|--------------|------------|
| P1 | **Zero new `**Manual proof argument**` blocks.** Cumulative *A* stays 17. Trusted axioms 2 → 3, reported separately. | any own residual at enforcement that no existing block covers | M |
| P2 | Inherited residuals = **81**, by name: `frama-c-arena`'s 79 pinned names verbatim + borrow.h's `typed_cast_borrowed_bytes_eq_call_memcmp_requires_danglingness_s1/_s2`. No option or result instance, so no option/result inheritance. | count ≠ 81, or any inherited name outside that set | H |
| P3 | **Run 1, G1 detected at the seam.** The separation precondition of `_stringbuf_append_bytes` is unproved at exactly its three public call sites (`stringbuf_append`, `_append_str`, `_append_n`). Inside the helper, `mem_copy`'s non-overlap precondition **proves** (it follows from the helper's own `@pre`). | any of the three call-site goals proves, or the `mem_copy` call goal fails | H |
| P4 | **Run 1, G3 detected.** `failure_unchanged`'s terminator conjunct is unproved in both fmt functions (the pass-2 failure path) and proves everywhere else it appears. | it proves in either fmt function, or fails anywhere else | M–H (split granularity may show it as several goals per function; counted as one family) |
| P5 | **Run 1, G5 detected.** The axiom's `sep` precondition is unproved at the pass-2 `vsnprintf` call in both fmt functions and proves at both pass-1 calls (`n == 0`, empty range). | either pass-2 `sep` goal proves, or either pass-1 goal fails | H |
| P6 | **G2 is invisible to WP, in every run.** No goal changes status because of the size argument, before or after the fix. musl's −1 satisfies `term` vacuously, and the contract cannot claim a formatted append succeeds. A safety proof is silent about availability. | any goal's status is attributable to the `n` argument | H |
| P7 | **Run 1: no other own goal fails.** This includes `init_arena`'s `wf` (derived from `arena_alloc`'s `address` ensures, assumed at the call even though it is itself an inherited residual), `append`'s `strlen` use, and `append_n`'s loop. | any own failure outside P3–P5 | M |
| P8 | **Run 2: own residuals = 0**, or only `append_n`'s read-validity goals (≤ 2, from Frama-C's `valid_read_nstring`/`strlen` axiomatics). Under P1 those must fall to an existing libc-string block; if none covers them, **P1 fails**, and the cause is named. | own residuals > 2, or of any other class | M–L |
| P9 | `stringbuf_wf` needs no sixth clause. | any goal needs one to prove | M |

**Open questions** — recorded, not scored:

- **Q1 — the variadic definitions.** This is the first unit in the project
  that *defines* a variadic function and uses `va_start`/`va_copy`. Run 1
  uses Frama-C 29's default Variadic translation; `vsnprintf` itself is not
  variadic, so the axiom binds either way. Rule, fixed now: if the kernel
  rejects the definitions, run 1 is repeated with `-variadic-no-translation`
  (diag.h's flag) as run 1b with identical contracts. If neither mode
  ingests the fmt pair, it is recorded as a **tool-reach boundary** (the
  lifetime.h precedent). Shipped code is never `#ifndef __FRAMAC__`-excluded,
  the other 27 functions are verified, and the pair carries test and MC/DC
  evidence only. P4 and P5 are void in that case, not failed.
- **Q2** — whether Frama-C's bundled libc spec for `vsnprintf` merges with
  the axiom and adds caller obligations. Whatever appears is recorded.

### MC/DC (MCDC-016 at closure)

**Baseline, this source**, measured locally with the coverage job's recipe
(GCC 14.2, `-fcondition-coverage`, `CANON_NO_REQUIRE`, forced fallbacks):
**101 / 136** (74.3%), 35 missed. Author confirms from the next CI log.

**Blind spot.** `stringbuf_close` and the three `_as_borrowed_*` accessors
are never called from `stringbuf_test.c`, so GCC does not emit them in that
TU, and their **14 outcomes are absent from the denominator**. `borrow_test.c`
calls them, but the per-file table takes the largest-denominator TU. This
is MCDC-015's wide-range blind spot in another form: the number looks
complete and is not.

The 35 missed outcomes, classified:

| Class | Outcomes | Disposition |
|-------|----------|-------------|
| Zero state (`sb` non-null, `data == NULL`): never tested | 15 | test with `StringBuf z = {0}` |
| NULL/empty argument sides (`!sb`, `!fmt`, `s.len == 0`, `actual_len == 0`) | 5 | test |
| `checked_add` overflow true sides (7 sites) | 7 | test with a fabricated struct or view — every site returns before touching memory |
| `with_nul > capacity` in `_va` | 1 | test |
| measure failure `needed_i < 0` | 2 | test (`"%ls"`, C locale — demonstrated) |
| pass-2 failure `written < 0`, `written != needed` | 4 | 2 removed with `append_fmt`'s copy (G4); 2 remain, **environmental** (MCDC-009 family) |
| `remaining`'s `!checked_sub` true side | 1 | test with a fabricated struct |

**A policy choice, made now — and where it differs from MCDC-015.** A
guard whose *only* job is to reject inputs outside the documented domain is
**tested** with such an input, provided the function returns before any
memory access. If it is never exercised, there is no evidence it works, and
evidence that it works is the only reason it exists. MCDC-015 J1 declined a
hand-built `range` in explicit terms ("to move a number"). The cases differ:
J1's line, `if (r->step < 0)`, is direction logic that is meaningful under
the invariant, and its FALSE arm degenerates to `step == 0` only for an
invalid struct. The 8 guards here have no meaning except on invalid input.
MCDC-015 is not reopened. If the author prefers J1's rule throughout, the
fabricated-input rows become justification rows instead, and the prediction
below becomes 122 / 132 with 10 rows. That choice is made before committing,
not after measuring.

**Prediction:** denominator 136 − 18 (`append_fmt`'s copy, G4) + 14 (the
blind spot, brought in by calling those four functions from
`stringbuf_test.c`) = **132**; final **130 / 132**, **2 justification rows**,
both pass-2 failure outcomes in `stringbuf_append_fmt_va`. Falsified if the
denominator is not 132 or there are more than 2 rows.

### What each outcome means for the paper

P1 holding twice is a trend of two, on two units that share almost nothing.
P1 failing is the curve's first unaided post-saturation increment, with a
named cause. P3 and P5 holding mean the tool confirmed human-found defects
at the exact seam the record names: the gap between an internal `@pre` and
the public contract. P6 holding is the newest kind of data point: a
**pre-registered blind spot**. A defect visible only on a second libc, which
the trusted base cannot see *because* it was written to be true on that
libc. That bounds what "verified" means here, and the paper should say so in
those words.

### Scoring

VERIFY-032 scores this record goal by goal after run 2, and again at
enforcement. Probe sources: `p_overlap.c` (G1), `p_bigcap.c` (G2),
`p_fmtalias.c` (G3/G5), `p_enc.c` (measure failure). They become regression
tests in run 2, except `p_bigcap.c`, which is musl-only, and `p_fmtalias.c`,
which is undefined behaviour by construction and stays a recorded probe,
never a test.

### Pre-run addendum (before run 1)

Written after 9268da3 while the contracts were being written, and committed
together with them, **before** CI had produced any result. It changes no
prediction above and no scoring rule. P1–P9 are scored exactly as committed
at 9268da3. The addendum records two things that writing the contracts
exposed, so that they also count as predicted rather than discovered.

**A1 — G6, a sixth finding: the helper's own `@pre` is violated by its own
caller.** `_stringbuf_append_bytes` documents `@pre add_len > 0`.
`stringbuf_append` calls it with `strlen(s)`, which is 0 for `""`. The
violation is benign: with `add_len == 0` the helper still behaves correctly
(`mem_copy` is a no-op for size 0, `len` is unchanged, and the terminator is
rewritten in place). So the defect is in the `@pre`, not the code. Design
item 4 carries the helper's `@pre` verbatim, so run 1 is predicted to report
**exactly one** further unproved goal: `add_pos` at `stringbuf_append`'s call
site. It is not reported at `append_str` or `append_n`, which both guard
zero length. Fix in run 2: drop the `@pre`.

**A2 — `init_arena` cannot be proved against arena.h's present contract.**
`arena_alloc` declares `assigns *arena` and gives no frame on
`arena->buffer` or `arena->capacity`. Its `address` ensures places the
result relative to the *post*-state buffer. Two of `init_arena`'s goals need
the *pre*-state buffer:

- the assigns check for the `buf[0] = '\0'` write, whose assigns location is
  necessarily stated in the pre-state;
- `init_ok`'s `\separated(sb, data …)` conjunct (`stringbuf_wf` clause 4),
  which can only come from a pre-state precondition.

Run 1 is predicted to report both as unproved; `-wp-split` may list the
second as a `_part` goal. This **falsifies P7 as committed**, and P7 is
scored as committed. The cause is a specification-strength cap in a
verified callee. It is not a stringbuf.h defect and not a prover limit.
Resolution is decided at run 2 between two options:

- (a) add a frame to `arena_alloc`, i.e.
  `ensures arena->buffer == \old(arena->buffer) && arena->capacity ==
  \old(arena->capacity)`. This adds proved goals to arena.h and to every job
  that inherits it, so their pinned proved-counts move, in one acknowledged
  ratchet.
- (b) keep the two goals and argue them, which bears on P1.

Weakening `init_arena`'s contract is not an option (design item 6).

**Run-1 own residuals, predicted in total:** P3's 3, plus A1's 1, plus P4's
pair, plus P5's pair, plus A2's 2, **= 10** (counting each `_part` family as
one). Nothing else.

**A3 — Q1/Q2, first observation (not scored).** Ubuntu's Frama-C 25.0
package has no WP plugin, but its kernel and Variadic plugin were usable
locally:

- All 29 contracts, the predicates, the loop annotation and the axiom parse
  and type-check (`-c11`).
- The Variadic plugin translates `stringbuf_append_fmt`'s definition:
  `va_start` becomes an assignment from `__va_params`, `va_copy` an
  assignment, `va_end` disappears. The contract survives the translation.
- The kernel reports merging the axiom with the bundled libc spec. Frama-C
  25's libc spec is only `assigns s[0..n-1] \from format[..], arg`.

CI's Frama-C 29 is authoritative for Q1 and Q2. The compiled C is unchanged
by this commit: the preprocessed TU is identical apart from `__LINE__`
values, since every addition is an ACSL comment or `__FRAMAC__`-only.

### Run 1 — observed (CI #1319, f6e54ca)

Report-only. Frama-C 29; Alt-Ergo 2.6.3, CVC5 1.2.1, Z3 4.15.2.
**4810 / 4913**, 103 unproved (97 Timeout, 6 Unknown, 0 Failed, 0 Invalid).
The facts are recorded here. Scoring is VERIFY-032's job, as committed.

**Inherited: 81 of 81, by name.** Every predicted name appears, none is
missing, and nothing outside the set is inherited.

**Own: 22 goals in eight causes.**

| # | Goals | Where | Cause | Predicted? |
|---|-------|-------|-------|------------|
| 1 | 4 | `no_overlap` at `stringbuf_append`, `_append_str`, `_append_n` (two `_part`s) | G1: the helper's separation `@pre`, which no public function states | yes, P3 — exactly the three call sites |
| 2 | 1 | `_stringbuf_append_bytes_call_mem_copy_requires_3` | traced below | **no** — P3 said this proves |
| 3 | 1 | `add_pos` at `stringbuf_append` | G6 | yes, addendum A1 |
| 4 | 4 | `failure_unchanged` `_part06`/`_part07` in both fmt functions | G3: the write pass overwrites `data[len]` | yes, P4 — nowhere else |
| 5 | 2 | `vsnprintf_2_requires_sep` in both fmt functions | G5: the `restrict` precondition. Both pass-1 calls proved. | yes, P5 |
| 6 | 3 | `stringbuf_init_arena`: `ensures_init_ok_part2`, `assigns_exit_part4`, `assigns_normal_part05` | A2: `arena_alloc` has no frame on `buffer`/`capacity` | yes, addendum A2. It predicted 2; the assigns clause yields two goals (normal and exit paths) |
| 7 | 4 | `_stringbuf_append_bytes` `assert_rte_mem_access` ×4 | traced below | **no** |
| 8 | 3 | `stringbuf_append_n`: `assert_rte_mem_access_2` (Unknown), `call…requires_src_read_part1/_part2` | not traced (hypothesis below) | **no** |

**Cause 2, traced.** `mem_copy`'s third `requires` is
`!regions_overlap((char *)dest, (char *)src, size)`. memory.h defines
`regions_overlap` by pointer ordering (`a < b + size && b < a + size`).
VERIFY-022 already recorded that `\separated` cannot discharge it across
bases. priority_queue.h carries four instances, and VERIFY-024 is the
candidate fix. P3's claim that this goal proves overlooked a class the
project had already named. G1's fix removes the call: `mem_move` has no
overlap precondition.

**Cause 7, traced.** The helper required only `stringbuf_wf(sb)`, and
`stringbuf_wf` does not contain `\valid(sb)`. The public functions get it
from `stringbuf_ok`; the helper had nothing, so its four field accesses
could not be proved. This is a contract omission of exactly the kind
VERIFY-022 recorded for `pop_raw`, made again here. It was caught by the
class tally on the first run.

**Cause 8, not traced.** The log names the goals but not their hypotheses.
**Hypothesis H-n:** `valid_read_nstring` is a disjunction ("n bytes
readable" *or* "a valid string"). Its second branch needs the strlen
axiomatics to bound the loop index by `strlen(s)`. Run 2 tests H-n by
stating the precondition as exactly what the loop reads (see below). If
any of the three goals survives run 2, H-n is wrong and the cause is still
open.

**Q1 answered:** Frama-C 29's default Variadic translation ingests both
variadic definitions, so no run 1b was needed. **Q2:** the kernel merged
the axiom with the bundled libc spec (`stdio.h:242`). As for every
bodiless libc function, it generated default `exits`/`terminates` clauses.
No extra caller obligation surfaced, and both pass-1 calls proved.

**Unpredicted, outside WP: MISRA 53 → 54.** The axiom's extern
redeclaration of `vsnprintf` is a rule 21.2 finding, the same shape as
diag.h's two. This was reproduced locally with Cppcheck 2.13.0: the only
stringbuf.h finding is `21.2` at line 75. The job is advisory and stayed
green, but its pinned count went stale. Run 2 updates it.

**MC/DC baseline confirmed:** CI reports `data/stringbuf.h` at
**101 / 136**, the figure this record measured locally.

### Run 2 — the fixes, and what they predict (committed before CI)

**Changes**, all in one commit:

- **G1:** the helper copies with `mem_move`. Its separation `@pre` is
  deleted, and its docs now say a stale view may be appended (memmove
  semantics).
- **G2:** the write pass passes `with_nul - len` (= `needed + 1`) as the
  size.
- **G3:** a failed write pass restores `data[len] = '\0'`. The failure
  guarantee is reworded to "the string `data[0 .. len]` is unchanged".
- **G4:** `stringbuf_append_fmt` is now a wrapper over `_va` (va_start,
  delegate, va_end). `_va` moves above it.
- **G5:** both fmt functions document the separation and carry it as
  `fmt_sep`.
- **G6:** the helper's `add_len > 0` `@pre` is deleted.
- **Cause 7:** the helper requires `\valid(sb)`.
- **H-n:** `stringbuf_append_n` requires a new predicate,
  `stringbuf_nreadable(s, n)`: every byte up to the first NUL, or the
  first n bytes, is readable. This is a *weaker* precondition than
  `valid_read_nstring`, so it admits every caller the documentation admits
  and strengthens the contract rather than tuning it.
- **Tests:** G1 regression (both entry points; fails on the run-1 source
  with ASan `memcpy-param-overlap`), G6, the `%ls` measure failure, the
  zero state, NULL/empty arguments, the 8 out-of-domain guards with
  fabricated inputs, and the four blind-spot functions.
- **MISRA ledger:** 54.

**Expected:**

- **inherited 81, same names;**
- **own exactly 3, the same three `stringbuf_init_arena` names** (A2 is
  not fixed in this run);
- total unproved 84;
- every other run-1 family closes, including H-n's three;
- no goal's status is attributable to G2's size change (P6).

**A2 is deferred, deliberately.** Giving `arena_alloc` a frame adds goals
to every job that includes arena.h: arena, arena-32, pool, region, vec and
stringbuf. Every one of their pinned proved-counts would move, so it is
its own arc with its own ratchet: **VERIFY-033 candidate**, the VERIFY-024
pattern. Stated now, before the run: if A2 is still open, **P8 as
committed is falsified by run 2** (P8 allowed only `append_n` goals), and
at enforcement A2 needs either VERIFY-033 or an argument block, which bears
on P1.

**MC/DC — measured locally before commit, not a fresh prediction.**
Measured with the coverage job's recipe (GCC 14.2): **130 / 132**. The
only misses are the two pass-2 failure outcomes in `_va`
(`written < 0`, `written != needed`), and the denominator is 132. That is
exactly what this record pre-registered at 9268da3. The prediction was
committed before any of this code existed; the local measurement only
reveals its outcome early. CI is authoritative, and MCDC-016 records it.

### Run 2 — observed (CI #1320, 600f238)

**4716 / 4802**, 86 unproved (83 Timeout, 3 Unknown, 0 Failed, 0 Invalid).

- **Inherited: 81 of 81, the same names as run 1.**
- **MC/DC:** CI reports `data/stringbuf.h` at **130 / 132**, the
  pre-registered figure. The two misses are the pass-2 failure outcomes in
  `_va`.
- **MISRA:** 54 real, matching the new pin.
- **Own: 5, against 3 predicted.**

| Goals | Status |
|-------|--------|
| A2's three `stringbuf_init_arena` names | as predicted, unchanged from run 1 |
| G1/G6 seam, `mem_copy`, helper rte, G3 `failure_unchanged`, G2/G5 `vsnprintf` preconditions | **all closed**, as predicted |
| `stringbuf_append_fmt_no_buffer_assigns_normal_part2` | **not predicted** — traced below |
| `stringbuf_append_n_call…requires_src_read_part2` | **H-n partly right:** 2 of run 1's 3 closed (the loop read and `src_read_part1`); this one remains |

**The wrapper goal, traced.** G4 made `stringbuf_append_fmt` a pure
delegation. WP frames a call with the callee's *top-level* `assigns`
(`sb->len, sb->data[len .. capacity-1]`), not with the behaviour that
applies at the call site. So the wrapper's `no_buffer` behaviour,
`assigns \nothing`, cannot be established, even though `_va`'s own
`no_buffer` behaviour says exactly that. The run-2 prediction missed a
consequence of its own structural change. Decision rule 6 forbids dropping
the frame claim, so the code changes instead: the wrapper repeats `_va`'s
guard and returns before the call.

**The `append_n` goal, hypothesis H-n2 (not traced).**
`stringbuf_nreadable` gives *pointwise* readability (`\valid_read(s + i)`
for each qualifying `i`). The helper requires *range* readability,
`\valid_read(s + (0 .. actual_len - 1))`. Turning the first into the
second needs the prover to instantiate the quantifier at both ends of the
range, under a nested quantifier, and Z3 times out on one split part. Run 3
adds the loop invariant `readable: \valid_read(s + (0 .. actual_len - 1))`,
which extends by one element per iteration from the read the loop has just
made. This is a proof annotation; no contract changes.

### Run 3 — changes and expectation (committed before CI)

- **`stringbuf_append_fmt`:** `if (!sb || !fmt || !sb->data) return false;`
  before `va_start`.
- **`stringbuf_append_n`:** the `readable` loop invariant.
- **Tests:** `_va`'s own guard through `fmt_va_helper` (NULL `sb`, NULL
  `fmt`, zero state). The wrapper now answers those cases first, so without
  these tests `_va`'s guard outcomes would drop out of coverage.

**Expected:**

- inherited 81, same names;
- **own exactly 3, A2's names**; total unproved 84.

**MC/DC moves, measured locally before commit:** **136 / 138**. The
wrapper's guard adds 6 outcomes and all are covered. The same two pass-2
justification rows remain. The pre-registered 130 / 132 held at run 2;
this change re-baselines the denominator for a traced WP reason, and
MCDC-016 records both figures.

**Schedule moves by one run.** Runs 4 and 5 are name-stability runs on
unchanged code; run 6 is enforcement. A2 is still deferred to the
VERIFY-033 candidate.

---

## VERIFY-032: Scoring VERIFY-031 After Run 2 — Five Held, Three Failed, One Open; Every Planted Defect Found Where Named, and the Blind Spot Stayed Blind

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-032 |
| **Date**       | 2026-10-03 |
| **Status**     | SCORED at run 2 (CI #1320, 600f238). Re-scored at enforcement for P1 only. |
| **Scores**     | VERIFY-031 (pre-registered at 9268da3, addendum at f6e54ca) |

Each prediction is scored **as committed**. The run-1 and run-2 sections of
VERIFY-031 hold the facts; this record holds only the verdicts.

| # | Verdict | Evidence |
|---|---------|----------|
| P1 | **OPEN** | Decided at enforcement. A2's three goals need either VERIFY-033 (an `arena_alloc` frame) or an argument block. The latter fails P1. |
| P2 | **HELD** | 81 / 81 inherited by name in both runs. None missing, none extra. |
| P3 | **FAILED** | The call-site half held exactly: `no_overlap` was unproved at the three named call sites and nowhere else. The helper half failed: `mem_copy_requires_3` did not prove. The cause is memory.h's pointer-ordering `regions_overlap`, a class VERIFY-022 had already recorded and P3 overlooked. |
| P4 | **HELD** | `failure_unchanged` was unproved in both fmt functions and in no other function. |
| P5 | **HELD** | `sep` was unproved at both pass-2 calls; both pass-1 calls proved. |
| P6 | **HELD** | G2's fix changed the size argument between runs, and no goal changed status because of it. `valid_buf` and `sep` proved in run 2; `term` is assumed. musl's failure mode stayed invisible, as predicted. |
| P7 | **FAILED** | Conceded before the run by the addendum (A1, A2). Eight further own goals came from three causes: the overlooked memory.h class, a missing `\valid(sb)` in the helper's contract, and `append_n`'s read-validity. |
| P8 | **FAILED** | Run 2 had 5 own goals: A2's 3 (stated in advance as falsifying P8), one `append_n` part (within P8's allowance), and the wrapper's `no_buffer` assigns (outside it). |
| P9 | **HELD** | No sixth `stringbuf_wf` clause was needed. The helper's missing `\valid(sb)` was a precondition omission, not an invariant gap: every public function carries it through `stringbuf_ok`. |
| Q1 | answered | Frama-C 29's default Variadic translation ingests both variadic definitions. |
| Q2 | answered | The axiom merged with the bundled libc spec, with no extra caller obligation. |
| A1 | **HELD** | Exactly one `add_pos` goal, at `stringbuf_append`'s call site. |
| A2 | **HELD** (count off by one) | Both predicted goals failed. The assigns clause yielded two goals (normal and exit paths), so 3 goals, not 2. |
| MC/DC | **HELD exactly** | 130 / 132 at run 2. The 2 justification rows are the pass-2 outcomes in `_va`. |

**Totals:** 5 held (P2, P4, P5, P6, P9), 3 failed (P3, P7, P8), 1 open (P1).
Both addendum items held, and the MC/DC figure held exactly.

**The detection test.** Of the five pre-run findings, the prover reported
the four that are visible to it (G1, G3, G5, G6) at the named goals. It did
not report G2, and it was predicted not to. Every report sat at the seam
the record named: the internal-helper/public-contract boundary for G1 and
G6, the failure path for G3, the restrict pair for G5. What the
pre-registration got wrong was never *where* a defect would surface. It was
what else would surface beside it. One cause was a class the project had
already written down (VERIFY-022); one was a precondition omitted the same
way VERIFY-022 recorded for `pop_raw`; one was the tool's call-site frame
rule, missed by the run-2 restructure (G4) itself.

**Method note for the next unit.** Two of run 1's three unpredicted causes
were already documented in this file. Before writing predictions, the
pre-registration for module 20 should grep `deviations.md` for every callee
precondition the new contracts will meet: `regions_overlap`, `\valid` of
the struct pointer, string predicates. That check costs minutes, and here
it would have moved two of three failures into the predicted column.

---

## VERIFY-033: Pre-registration — arena.h's Allocators State Their Frame: One Missing Fact, Three stringbuf Goals, Six Pins

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-033 |
| **Date**       | 2026-10-03 |
| **Status**     | PRE-REGISTERED — no contract touched; predictions committed before any change |
| **Baseline**   | fc924e2 (CI #1321): stringbuf 4744 / 4828, 84 unproved, 3 own |
| **Scope**      | `core/arena.h`: `arena_alloc`, `arena_alloc_aligned` |
| **Category**   | Callee specification-strength fix (the VERIFY-023 / VERIFY-026 shape), cross-job ratchet |

**Why this record exists.** VERIFY-031's addendum A2 predicted, and runs 1–3
confirmed, that `stringbuf_init_arena` keeps three unproved goals:

- `assigns_exit_part4`
- `assigns_normal_part05`
- `ensures_init_ok_part2`

The cause named there: `arena_alloc` declares `assigns *arena` and says
nothing about `arena->buffer` or `arena->capacity`. A caller therefore
cannot connect the post-state buffer, which `arena_alloc`'s `address`
ensures is stated against, to the pre-state buffer, which its own assigns
clause and preconditions are stated against. VERIFY-032 left P1 open on
exactly this. This record predicts what stating the missing fact does, job
by job, before the contract is edited.

**The fact is already stated three times in the same header.**
`arena_reset`, `arena_reset_secure` and `arena_reset_to` each carry:

    ensures arena->buffer == \old(arena->buffer);
    ensures arena->capacity == \old(arena->capacity);

The two allocators are the only `*arena`-mutating primitives that do not.
The bodies write `offset` and `padding_accum` only (plus the debug counters,
outside the verified configuration). The fact is true by inspection; it has
simply never been written down.

### The change, fixed now

Exactly two global, named clauses, appended after the behaviours in
**`arena_alloc` and `arena_alloc_aligned`** and nowhere else:

    ensures frame_buffer:   arena->buffer   == \old(arena->buffer);
    ensures frame_capacity: arena->capacity == \old(arena->capacity);

Named, so that no existing unnamed clause is renumbered and no existing goal
name moves. `assigns *arena` stays: narrowing it to
`arena->offset, arena->padding_accum` would be false under
`CANON_ARENA_DEBUG`, which writes `alloc_count` and `peak`.

**Out of scope, and why.**

- The wrappers (`arena_alloc_zero`, `arena_alloc_aligned_zero`,
  `arena_try_alloc`, `arena_try_alloc_aligned`) are not called by any
  verified module that needs the frame.
- Their open residuals have a different, already-visible shape.
  `try_alloc`'s `non_null_out_ensures_part{1,2}` is `arena_invariant` after
  a `*out` write that nothing separates from `*arena`. `alloc_zero`'s
  `assigns_normal_part3` is a `mem_zero` write into the buffer that
  `assigns *arena` does not cover.
- A frame clause would fix neither. Adding it would only add goals to six
  pins for no closure.

### Predictions (author — confirm or amend before committing)

New goals per translation unit: 2 clauses × 2 functions = **4**. Each is an
atomic equality over a field the body never writes, expected to close in Qed
as one goal each. This is predicted for every TU that compiles arena.h.
Jobs that do not include arena.h (all others) do not move.

| # | Job | Now | Predicted | Residual names |
|---|-----|-----|-----------|----------------|
| R1 | `frama-c-arena` | 3476 / 3555, 79 | **3480 / 3559, 79** | the same 79 |
| R2 | `frama-c-arena-32` | 3476 / 3555, 79 | **3480 / 3559, 79** | set equality with R1 still holds |
| R3 | `frama-c-pool` | 3950 / 4041, 91 | **3954 / 4045, 91** | the same 91 |
| R4 | `frama-c-region` | 3624 / 3726, 102 | **3628 / 3730, 102** | the same 102 |
| R5 | `frama-c-vec` | 5317 / 5501, 184 | **5321 / 5505, 184** | the same 184 |
| R6 | `frama-c-stringbuf` | 4744 / 4828, 84 | **4751 / 4832, 81** | the 81 inherited; **own 3 → 0** |

| # | Prediction | Falsified if | Confidence |
|---|------------|--------------|------------|
| F1 | All four new goals prove in every TU (no new residual anywhere). | any `…_ensures_frame_*` goal unproved | H |
| F2 | Each new clause is exactly one goal (`+4` per TU, not split into parts). | any total in R1–R6 off by other than 4 | M |
| F3 | A2 closes: all three `stringbuf_init_arena` goals prove. | any of the three survives | M–H |
| F4 | No residual outside stringbuf changes status. In particular the pool and try-alloc goals do not close (aliasing, not framing — see below). | any pinned residual in R1–R5 closes or appears | M |
| F5 | Nothing else moves: MC/DC, MISRA (54) and every test are unchanged, since the change is ACSL only. | any of these changes | H |

**F4 is the risky one, and why.** `pool_reset`'s two open goals
(`reset_ensures_part3`, `reset_ensures_2_part3`) have a frame-shaped
reading: `pool_invariant` needs `end_mark <= arena->capacity` after an
`arena_alloc` that, today, forgets the capacity. But `pool_reset`'s first
failing goal is `pool->used == 0`. That one cannot depend on the arena's
frame; it fails because nothing separates `*pool` from the `*arena` the
call havocs. `pool_init`'s four (`ensures{,_2,_3}_part4`, `ensures_4_part3`)
read the same way. So the prediction is that the frame is necessary but not
sufficient there, and those goals stay. If any of them closes, F4 fails in
the good direction, and VERIFY-034 records which goals moved and which
argument blocks retire. The pool/try-alloc aliasing reading is recorded as
a **hypothesis** for a later arc, not as a finding.

**What the run decides.** If F1 and F3 hold, stringbuf has zero own
residuals. Then P1 can be scored HELD at enforcement, *A* stays 17, and
stringbuf proceeds to three name-stability runs and enforcement (the
range.h #1315–#1317 pattern). If F3 fails, the three goals are traced from
the goal text before anything else is changed. If F1 fails anywhere, the
arena pins do not ratchet on a guess: the failing clause is traced first.

### Mechanics

The change commit carries the **predicted** pins, so a correct prediction
runs green and a wrong one fails visibly at the job that missed:

- R1–R5's `PROVED_COUNT` pins and the arena-32 embedded 64-bit baseline
  text;
- the stringbuf job's evidence block (own predicted 0).

A failed pin is a scored outcome, followed by a ratchet commit, as at
VERIFY-027's CI #1299 → #1300. VERIFY-034 scores this record.

### Change (committed before CI)

The two clauses went into both contracts exactly as written above, with one
placement note. ACSL requires default-behaviour clauses before the named
behaviours, so they sit directly after `assigns *arena;`. Placed after the
behaviours, they would have attached to the last behaviour (`does_not_fit`)
and stopped being global. The "appended after the behaviours" wording above
describes intent, not position. Checked with the Frama-C 25 kernel
pretty-printer: both clauses print as global `ensures` ahead of
`behavior size_zero`.

The pins carry the predicted values: arena and arena-32 at 3480 / 3559, pool
at 3954 / 4045, region at 3628 / 3730, vec at 5321 / 5505. EXPECTED_UNPROVED
and every name roll-call are unchanged, which is F4. The compiled C is
untouched: the change is ACSL comments only, so F5 has nothing to move.

---

## VERIFY-034: Scoring VERIFY-033 — The Frame Closes A2; Every Prediction Holds Except the Count, Which Was Off by Exactly ×5

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-034 |
| **Date**       | 2026-10-03 |
| **Status**     | SCORED at CI #1322 (b785d52); pins ratcheted in the commit carrying this record |
| **Scores**     | VERIFY-033 (pre-registered at 8629d10) |

**Observed (CI #1322).** The five enforced jobs failed on their proved-count
pins only. Each failure moved by exactly the same amount, and no residual
count or name moved.

| Job | Predicted | Observed | Unproved | Names |
|-----|-----------|----------|----------|-------|
| arena | 3480 / 3559 | **3496 / 3575** | 79 | same 79 |
| arena-32 | 3480 / 3559 | **3496 / 3575** | 79 | set equality with arena: 0 / 0 / 79 |
| pool | 3954 / 4045 | **3970 / 4061** | 91 | same 91 |
| region | 3628 / 3730 | **3644 / 3746** | 102 | same 102 |
| vec | 5321 / 5505 | **5337 / 5521** | 184 | same 184 |
| stringbuf (report-only) | 4751 / 4832 | **4767 / 4848** | 81 | the 81 inherited; **own 0** |

Every TU gained **+20** goals, not +4, and every new goal proved.
stringbuf's +23 proved = 20 new + A2's 3 closed.

| # | Verdict | Evidence |
|---|---------|----------|
| F1 | **HELD** | No `…frame_*` goal is unproved in any of six TUs. |
| F2 | **FAILED** | +20 per TU, not +4. Each clause is 5 goals, not 1. |
| F3 | **HELD** | All three `stringbuf_init_arena` goals proved; stringbuf own residuals 3 → 0. |
| F4 | **HELD** | Unproved counts 79 / 79 / 91 / 102 / 184 unchanged; arena-32's set equality holds; pool's goals stayed open, as predicted. |
| F5 | **HELD** | MC/DC `data/stringbuf.h` 136 / 138, unchanged; ACSL-only change. |

**Why ×5, traced.** Under `-wp-split`, an ensures over a function body is
split by the paths that reach the return. `arena_alloc` and
`arena_alloc_aligned` each have five:

- `size == 0`;
- the three disjuncts of the overflow/capacity guard;
- success.

The same five-way split was already visible in the pinned names
`arena_alloc_does_not_fit_ensures_part5` and
`arena_alloc_aligned_does_not_fit_ensures_part5`. They prove in Qed one part
at a time, so they count once per part. F2 assumed the simplifier would
collapse a trivially-true frame before splitting; it splits first. **Method
note:** a part-count prediction should be read off the highest `_partN` of
the same function's existing goals, not assumed to be 1. That is the same
lesson as VERIFY-032's: the evidence was already in the file.

**What this decides.**

- P1 of VERIFY-031 can now be scored HELD at stringbuf's enforcement:
  stringbuf has zero own residuals, so no argument block is needed and
  cumulative *A* stays 17.
- The pool/try-alloc aliasing reading (VERIFY-033 F4) stays a hypothesis
  for a later arc.
- **Ratchet:** the five pins move to the observed values in the commit
  carrying this record. That CI run, on stringbuf source unchanged since
  b785d52, is stringbuf's second name-stability run; #1322 is the first.
  One more identical run, then enforcement, following the range.h
  #1315–#1317 pattern.

### Addendum — the seventh pin (Canon-C CC #13, b785d52)

VERIFY-033's job inventory named six jobs. There was a seventh:
`frama-c-cc-vec`, the VERIFY-025 macro-body experiment. It lives in a
separate workflow file (`frama-c-cc.yml`), and the inventory scanned only
`cmake-multi-platform.yml`. Its TU compiles arena.h like the others.

CC #13 measured **5394 / 5586** against the pinned 5374 / 5566, i.e. the
same **+20** as every other arena-including TU. Gates (1)–(3) all passed:

- 0 Failed, Invalid or Stepout;
- 192 unproved, unchanged;
- roll-call: pinned 192, missing 0, unpinned 0.

So F1 and F4 hold for this TU too, and F2's ×5 is confirmed a seventh time.
The other five CC families (slice, result, option, borrow, deque) do not
include arena.h and stayed green, consistent with the rule.

The CC workflow triggers only on source and CC-file changes, so the docs-
and workflow-only ratchet commit (aaa09c7) did not re-run it. The pin moves
to 5394 / 5586 in the commit carrying this addendum, which edits
`frama-c-cc.yml` and therefore re-runs it.

**Method note, extending the one above:** a cross-job ratchet's inventory is
every workflow file under `.github/workflows/` that runs WP on a TU
including the changed header, not the main workflow alone. `grep -l` across
the directory for the header's pinned figures finds them in seconds.

---

## VERIFY-035: stringbuf.h Enforced — Module 19 Closes With Zero Own Residuals and Zero New Argument Blocks

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-035 |
| **Date**       | 2026-10-03 |
| **Status**     | ENFORCED — pinned from CI #1322 (b785d52) and #1324 (50045d2), name-identical; the commit carrying this record is the third run |
| **Scope**      | `data/stringbuf.h`: 29 functions in place, Typed+Cast, no driver |
| **Closes**     | VERIFY-031 (pre-registration), VERIFY-032 (run-2 scoring), VERIFY-033/034 (the arena_alloc frame that closed A2) |

**Pinned.** **4767 / 4848**, exactly **81** unproved, **0 Failed, 0 Invalid,
0 Stepout**. All 81 are inherited:

- `frama-c-arena`'s 79 pinned names, verbatim;
- borrow.h's two own memcmp-danglingness goals.

**Own residuals: 0.** #1322 and #1324 report the same 81 names. Their
Timeout/Unknown split differs (78/3 against 76/5), which is why the gate
pools the two classes. Pooling them is the VERIFY-019-M lesson: which goals
need a solver is invariant, while which solver reaches a goal first is
scheduling.

**Gates** (`frama-c-stringbuf`, enforced from this commit):

0. a summary exists;
1. zero Failed/Invalid/Stepout;
2. proved line == `4767 / 4848`;
3. Timeout + Unknown == 81;
4. roll-call: the unproved names equal the 81 pinned names by set
   equality, so own == 0.

A ratchet edits gates 2 and 3 and the roll-call together.

### The arc

| Step | Commit / CI | Proved | Own | What moved it |
|------|-------------|--------|-----|---------------|
| pre-registration | 9268da3 | — | — | G1, G2, G3, G5 demonstrated by probe; P1–P9 committed |
| run 1 (detection) | f6e54ca / #1319 | 4810 / 4913 | 22 | contracts on the unfixed bodies; addendum A1 (G6) and A2 (init_arena) committed with them |
| run 2 (fixes) | 600f238 / #1320 | 4716 / 4802 | 5 | G1–G6 fixed; helper `\valid(sb)`; `stringbuf_nreadable` |
| run 3 (provability) | fc924e2 / #1321 | 4744 / 4828 | 3 | fmt wrapper guard (the WP call-site frame rule); `append_n` `readable` invariant |
| VERIFY-033 | b785d52 / #1322 | 4767 / 4848 | **0** | `arena_alloc{,_aligned}` state `frame_buffer`/`frame_capacity`; A2 closes; +20 in every arena-including TU |
| ratchets | aaa09c7 / #1323, 50045d2 / #1324 | 4767 / 4848 | 0 | seven pins moved, stringbuf source untouched: stability runs |
| **enforced** | this commit | 4767 / 4848 | 0 | gate switched on |

### Final scoreboard (VERIFY-031)

| # | Verdict | Note |
|---|---------|------|
| P1 | **HELD** | Zero new argument blocks; cumulative *A* stays 17. Not unaided: closing A2 took a callee specification fix (VERIFY-033), the range.h footnote's shape. |
| P2 | **HELD** | 81 inherited by name in every run. |
| P3 | **FAILED** | The call-site half held exactly. The helper half failed on memory.h's known `regions_overlap` class (VERIFY-022). |
| P4 | **HELD** | |
| P5 | **HELD** | |
| P6 | **HELD** | G2 was invisible to WP before and after its fix: a pre-registered blind spot that stayed blind. |
| P7 | **FAILED** | Conceded by addendum A2, plus 8 unpredicted goals. |
| P8 | **FAILED** | A2's 3, the wrapper frame and one `append_n` part at run 2. |
| P9 | **HELD** | |
| A1, A2 | **HELD** | A2 counted 3 goals, not 2. |
| MC/DC | **HELD** at run 2 | 130 / 132; final 136 / 138 after run 3's guard (MCDC-016). |

**Trusted base:** 3 axioms. `vsnprintf`'s `term` (ISO C, true on glibc and
musl) is added to diag.h's two.

**MISRA:** 54, with one 21.2 at the axiom. MISRA-DEV-018 (all three
verifier-only declarations) is recorded as an option, not taken here.

### For Table 5 (module 19)

| Field | Value |
|-------|-------|
| Module | `data/stringbuf.h` |
| Functions | 29 |
| Model | Typed+Cast (originated by the header's own casts) |
| Inherited residuals | 81 (arena.h's 79 + borrow.h's 2), byte-identical by name |
| Own residuals | 22 → 5 → 3 → **0** |
| New argument blocks | **0** (cumulative *A* = 17) |
| Trusted axioms added | 1 (cumulative 3) |
| Defects found and fixed | G1 (overlap reaching `mem_copy`: abort / UB through the public API), G2 (`vsnprintf` size > `INT_MAX`: every formatted append fails on musl), G3 (failure path overwrites the terminator), G5 (unstated `restrict` precondition), G6 (helper `@pre` violated by its own caller); G4 structural |
| Side effect upstream | arena.h: the allocators' missing frame (VERIFY-033), +20 goals in six TUs plus the CC experiment's vec, every one proved |

**What the paper can say.** Saturation held for a second prospective unit,
and this unit shares almost nothing with range.h: memory-heavy rather than
arithmetic, the deepest core closure, libc string and formatting calls, a
loop, and the project's first variadic definitions.

The detection test also held. Every defect visible to the prover was
reported at the seam the pre-registration named. The one defect it was
predicted not to see, G2, it did not see. What the predictions missed was
never *where* a defect surfaces, only what else surfaces beside it. Three of
those misses were already written down in this file (VERIFY-022 twice, and
the highest `_partN` of an existing goal name). VERIFY-032 and VERIFY-034
record the resulting method notes.

---

## MCDC-016: stringbuf.h — 101/136 → 136/138, a Four-Function Blind Spot Closed, Two Outcomes Environmental

| Field          | Value |
|----------------|-------|
| **ID**         | MCDC-016 |
| **Date**       | 2026-10-03 |
| **Baseline commit** | Canon-C CI #1319 (f6e54ca); the coverage recipe on this code was first measured locally at 9268da3 and matched. |
| **Final commit** | Canon-C CI #1321 (fc924e2); unchanged at #1322 and #1324. |
| **Scope**      | `data/stringbuf.h` MC/DC from `stringbuf_test`: 101/136 (74.3%) → 130/132 (98.5%) → **136/138 (98.55%)**. |
| **Category**   | Coverage completeness; run alongside VERIFY-031..035 |

**Description.** At baseline the suite passed under ASan and UBSan on source
containing G1, G2, G3 and G5 (VERIFY-031), and it measured 74.3%. That
number also overstated the real figure, because four functions were never
called from `stringbuf_test.c`: `stringbuf_close` and the three
`_as_borrowed_*` accessors. GCC does not emit an unused `static inline`
function, so their 14 outcomes were not in the denominator at all.
`borrow_test.c` calls them, but the summary table takes the
largest-denominator TU.

### Stages

| Stage | Commit | Outcomes | What moved it |
|-------|--------|----------|---------------|
| baseline | f6e54ca | 101 / 136 | — |
| run 2 | 600f238 | 130 / 132 | −18 (`append_fmt`'s duplicated body removed, G4); +14 (the blind-spot functions called); zero state; NULL/empty arguments; the `%ls` measure failure; six out-of-domain guards driven by fabricated inputs |
| run 3 | fc924e2 | **136 / 138** | +6, all covered: the fmt wrapper's own guard (VERIFY-032). `_va`'s guard is now reached directly by `fmt_va_helper` tests, since the wrapper returns first. |

The run-2 figure, 130 / 132, was pre-registered at 9268da3, before any of
that code existed, and held exactly.

**Policy, as decided in VERIFY-031.** A guard whose only job is to reject
inputs outside the documented domain is tested with such an input, provided
the function returns before any memory access. In the final source that is
six outcomes, all TRUE sides:

- 612 and 613 (helper `checked_add` overflows, via an oversized `str_t`
  view);
- 761 (`append_char`, fabricated `len`);
- 841 and 842 (`_va`, fabricated `len`);
- 1299 (`remaining`, `len + 1 > capacity`).

This diverges from MCDC-015 J1, for the reason VERIFY-031 records: those
guards have no meaning except on invalid input.

### Line by line (`data/stringbuf.h` at fc924e2, `stringbuf_test`)

| Line | Function | Decision | Outcomes |
|------|----------|----------|----------|
| 476 | `stringbuf_init_arena` | `!buf` | 2/2 |
| 561 | `stringbuf_close` | `!sb` | 2/2 |
| 612 | `_stringbuf_append_bytes` | `!checked_add(len, add_len, &end)` | 2/2 |
| 613 | `_stringbuf_append_bytes` | `!checked_add(end, 1, &with_nul)` | 2/2 |
| 614 | `_stringbuf_append_bytes` | `with_nul > capacity` | 2/2 |
| 670 | `stringbuf_append` | `!sb \|\| !sb->data` | 4/4 |
| 671 | `stringbuf_append` | `!s` | 2/2 |
| 717 | `stringbuf_append_str` | `!sb \|\| !sb->data` | 4/4 |
| 718 | `stringbuf_append_str` | `!s.ptr \|\| s.len == 0` | 4/4 |
| 758 | `stringbuf_append_char` | `!sb \|\| !sb->data` | 4/4 |
| 761 | `stringbuf_append_char` | `!checked_add(len, 2, &with_nul)` | 2/2 |
| 762 | `stringbuf_append_char` | `with_nul > capacity` | 2/2 |
| 831 | `stringbuf_append_fmt_va` | `!sb \|\| !fmt \|\| !sb->data` | 6/6 |
| 838 | `stringbuf_append_fmt_va` | `needed_i < 0` | 2/2 |
| 841 | `stringbuf_append_fmt_va` | `!checked_add(len, needed, &end)` | 2/2 |
| 842 | `stringbuf_append_fmt_va` | `!checked_add(end, 1, &with_nul)` | 2/2 |
| 843 | `stringbuf_append_fmt_va` | `with_nul > capacity` | 2/2 |
| **856** | `stringbuf_append_fmt_va` | `(written < 0) \|\| ((usize)written != needed)` | **2/4** — J1, J2 |
| 928 | `stringbuf_append_fmt` | `!sb \|\| !fmt \|\| !sb->data` | 6/6 |
| 980 | `stringbuf_append_n` | `!sb \|\| !sb->data` | 4/4 |
| 981 | `stringbuf_append_n` | `!s \|\| n == 0` | 4/4 |
| 991 | `stringbuf_append_n` | `actual_len < n && s[actual_len] != '\0'` | 4/4 |
| 993 | `stringbuf_append_n` | `actual_len == 0` | 2/2 |
| 1019 | `stringbuf_str` | `sb && sb->data` | 4/4 |
| 1047 | `stringbuf_as_str` | `!sb \|\| !sb->data` | 4/4 |
| 1075 | `stringbuf_as_bytes` | `!sb \|\| !sb->data` | 4/4 |
| 1092 | `stringbuf_as_cbytes` | `!sb \|\| !sb->data` | 4/4 |
| 1117 | `stringbuf_buffer_bytes` | `!sb \|\| !sb->data` | 4/4 |
| 1134 | `stringbuf_buffer_cbytes` | `!sb \|\| !sb->data` | 4/4 |
| 1172 | `stringbuf_as_borrowed_str` | `!sb \|\| !sb->data` | 4/4 |
| 1207 | `stringbuf_as_borrowed_bytes` | `!sb \|\| !sb->data` | 4/4 |
| 1243 | `stringbuf_buffer_as_borrowed_bytes` | `!sb \|\| !sb->data` | 4/4 |
| 1266 | `stringbuf_len` | `sb` | 2/2 |
| 1276 | `stringbuf_capacity` | `sb` | 2/2 |
| 1296 | `stringbuf_remaining` | `!sb \|\| capacity == 0` | 4/4 |
| 1299 | `stringbuf_remaining` | `!checked_sub(capacity, len + 1, &usable)` | 2/2 |
| 1310 | `stringbuf_is_empty` | `!sb \|\| len == 0` | 4/4 |
| 1328 | `stringbuf_is_full` | `!sb \|\| len + 1 >= capacity` | 4/4 |
| 1338 | `stringbuf_is_arena_backed` | `sb && arena != NULL` | 4/4 |
| 1370 | `stringbuf_clear` | `sb && sb->data` | 4/4 |
| 1406 | `stringbuf_truncate` | `sb && sb->data && new_len < len` | 6/6 |
| | **41 decisions** | | **136 / 138** |

The four functions without a decision (`stringbuf_init_buffer`,
`stringbuf_lifetime_open_`, `stringbuf_lifetime_close_` in the verified
configuration, and the `stringbuf_printf` alias) contribute nothing to the
denominator. Line numbers are those of fc924e2; the coverage job's
eighteenth per-line step (added with this entry) prints them on every run.

### Disposition of the two misses

| # | Line | Missed | Disposition |
|---|------|--------|-------------|
| J1 | 856 | `written < 0` TRUE | **justified — environmental.** Pass 2 runs with the same format and arguments as pass 1, which already succeeded, and since G2 its size is exactly `needed + 1`, which is ≤ `INT_MAX + 1`. No libc in the matrix fails the second of two identical conversions. Before G2, musl reached it (n > `INT_MAX`, VERIFY-031); G2 removed the cause. The failure branch is kept, and G3 made it restore the terminator. |
| J2 | 856 | `(usize)written != needed` TRUE | **justified — environmental, same argument.** Two conversions with identical arguments agree in defined behaviour. The only demonstration, VERIFY-031's `p_fmtalias.c`, is undefined behaviour by construction (G5) and stays a recorded probe, never a test. |

The measure-failure outcome (838) is covered on CI's glibc through `%ls`
with a character the C locale cannot encode. A libc whose C locale encodes
that character would leave 838 uncovered; the test asserts the invariant
either way.

**Final**: 136 / 138, two justification rows (J1, J2). Any other miss
printed by the per-line step is a regression.

---

## VERIFY-036: Four Arguments Read Against Their Code — Three More Blocks Defective, One Defective Twice

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-036 |
| **Date**       | 2026-10-05 |
| **Status**     | OPEN — F1 fixed (f1ba2de) and scored exact at CI #1326 / CC #15; F2 fixed (8163f06) and scored at CI #1328 / CC #17 (exact on names, count estimate missed); F5 fixed (c5281dc) and scored exact at CI #1330 / CC #18; F6 fixed (c069881) and scored exact at CI #1332 / CC #20; F3 fixed (baf8ee9) and scored at CI #1334 / CC #21 (exact on names; count and job-set misses recorded); F4 fixed (e953219) and scored exact at CI #1336 / CC #23. All six fixes landed; the arguments of arena Cat 2c and pool Cat 2d still need rewriting. Each fix is preceded by a committed prediction and followed by its scoring, in the VERIFY-026/027 form |
| **Scope**      | argument blocks VERIFY-008 Cat 2; VERIFY-009 Cat 2a and Cat 2b (Cat 2c by dependency); VERIFY-010 Cat 2b |
| **Method**     | each block's central claim read against the body and contract it describes; each doubtful claim tested by an executable probe in `tools/probes/verify-036/`. Reading and probes produced with AI assistance (Claude) and reviewed by the author before commit |

**Summary.** Four in-force arguments cover obligations that are false, not
true-but-unprovable. Each is demonstrated by a probe against master as of
2026-10-04. In the paper's terms (§5) all four are arguments wrong on their own
terms. The gates were right throughout: every one of these goals was unproved
and pinned by name. What was wrong was the explanation attached to it.

Blocks found defective: 3 of 17 before this record (the argument audit of
2026-08-09), 6 of 17 after it. VERIFY-010 Cat 2b is defective for the second
time, for a different reason. F5, recorded after F1 was scored, adds two more:
8 of 17.

### F1 — `mem_get_alignment`: the signed negation can overflow (VERIFY-008 Cat 2, goal 7)

**Record says.** Cat 2's root-cause paragraph attributes
`typed_cast_mem_get_alignment_assert_rte_signed_overflow` to the cast
round-trip losing integer bounds. The block's argument does not address it.

**Code does.** `(uintptr_t)(-(intptr_t)addr)` overflows when `addr` has only
its top bit set, so that `(intptr_t)addr == INTPTR_MIN`. On 32-bit targets that
address is `0x80000000`, an ordinary user-space address, and
`frama-c-arena-32` carries the goal.

**Probe.** `p_getalign.c`. UBSan reports the negation at `core/memory.h:450`.

**Fix.** Negate in unsigned arithmetic:
`return (usize)(addr & (~addr + (uintptr_t)1));`. The RTE plugin emits no
signed-overflow alarm for it.

### F2 — `pool_alloc`: the remaining goal is a compiled-out null check (VERIFY-010 Cat 2b)

**Record says.** The block's one remaining goal,
`typed_cast_pool_alloc_assert_rte_mem_access`, is the slot address chain
through the `uintptr_t` round-trip, one dereference deeper than the five
VERIFY-023 closed.

**Code does.** `pool_alloc` never dereferences the slot. Its contract admits
`pool == \null` (`requires pool == \null || pool_invariant(pool)`; behavior
`null_pool` ensures `\result == \null`), and the body's only null check is
`require_msg`, which `-DCANON_NO_REQUIRE`, the configuration of every WP job,
compiles to `((void)0)`. The first memory access is then `pool->used`. An
unsuffixed `assert_rte_mem_access` is the first mem-access alarm WP emits in
the function, and every later access proves because WP takes that assertion as
a hypothesis. This also explains why VERIFY-023 closed five goals in this block
and left this one: it was never about the slot. **To confirm:** the goal's
source line in `frama-c-pool`'s `-wp-report-json` output should be the
`if (pool->used >= pool->capacity)` line.

**Consequence.** The `null_pool` ensures is proved only relative to that
unproved assertion. In the proof configuration `pool_alloc(NULL)` dereferences
NULL; in the default build it aborts. No configuration returns NULL as the
contract promises.

**Probe.** `p_pool_null.c`. Segmentation fault under
`-DCANON_NO_REQUIRE -DNDEBUG`; contract-violation abort in the default build.

**Fix (author's decision).** (a) Make the code match the contract: replace the
`require_msg` with `if (pool == NULL) { return NULL; }`. Local, but the default
build changes from abort to returning NULL. (b) Make the contract match the
code and its doc comment: `requires pool_invariant(pool)`, dropping the
`== \null` disjunct and `null_pool`. This fits `require_msg`-as-precondition,
but `pool_alloc_zero` and `pool_try_alloc` also admit NULL, so their contracts
tighten with it or their call sites open false goals.

### F3 — `arena_alloc{,_aligned}`: `ptr_span` runs before the capacity guard (VERIFY-009 Cat 2a)

**Record says.** Both pointers lie within the buffer at the `ptr_span` call.
The 2026-09-21 coverage note concedes the four `\valid_read` requires are false
at `offset == capacity`, and the VERIFY-026 addendum of 2026-09-18 defers that
case as a `ptr_span` contract question, since `ptr_span` only subtracts.

**Code does.** `ptr_span(aligned_ptr, current)` is called before the guard.
Whenever `offset + pad > capacity`, not only at `offset == capacity`, the
aligned pointer lies beyond one-past-the-end of the caller's buffer. There a
weaker `ptr_span` requires would not help, because the subtraction itself is
outside C99 6.5.6p9.

**Probe.** `p_arena_span.c`. On a 100-byte `malloc`'d buffer,
`arena_alloc(&a, 97)` then `arena_alloc(&a, 1)` gives ASan
`invalid-pointer-pair` in `ptr_span` (`core/primitives/ptr.h:851`), called from
`arena_alloc` (`core/arena.h:364`), with the aligned pointer 12 bytes past the
object.

**Fix (author's decision).** Do not form the aligned pointer before the guard:
compute `pad` in integer arithmetic on the address, test capacity, then form
the result pointer. Decide together with F4, which constrains how `pad` is
specified; commit separately so each delta stays readable.

### F4 — `arena_can_fit` pads from the offset, the code from the address (VERIFY-009 Cat 2b; Cat 2c by dependency)

**Record says.** When `arena_can_fit` holds, its let-bindings give
`cur + pad + size <= capacity`, so the guard does not fire, and the only
obstacle is proving that the C pad equals the ACSL pad under Typed+Cast. The
block is filed (c), the predicate's readable form being the deliberate trade.

**Code does.** The ACSL pad is `(alignment - (offset % alignment)) % alignment`.
The C pad is computed by `ptr_align_up` on the address `buffer + offset`. They
are equal only when the buffer is aligned to `alignment`, and neither
`arena_init` nor `arena_invariant` requires that. For any other buffer both
`fits` and `does_not_fit` are violated, so the 16 goals are false, not hard.
Cat 2c's argument assumes the parent's postconditions per Cat 2b and rests on
it. Downstream proofs that used `fits` or `does_not_fit` were made against a
contract that is false for unaligned buffers; they are re-checked after F4.

**Probe.** `p_arena_pad.c`, three cases, each meeting the precondition and
violating the selected behaviour: `arena_alloc` with `buffer % 16 == 1`
(`fits` selected, NULL returned); `arena_alloc` with `buffer % 16 == 15` after
one ordinary allocation (`does_not_fit` selected, non-NULL returned);
`arena_alloc_aligned(.., 64)` on a 16-aligned, malloc-style buffer (`fits`
selected, NULL returned). Same results under `-DCANON_NO_REQUIRE -DNDEBUG`.

**Fix (author's decision).** (a) Pad from the address in the predicate, so it
states what the code does. General; whether it proves is the open question.
(b) Require buffer alignment in `arena_init` and `arena_invariant`. Simpler,
but it repairs only `arena_alloc`: `arena_alloc_aligned` with
`alignment > CANON_DEFAULT_ALIGN` still needs (a).

### F5 — the zero wrappers' assigns clauses omit the zeroed bytes (VERIFY-009 Cat 2c; VERIFY-010 Cat 2d)

**Record says.** Arena Cat 2c files `arena_alloc_zero_assigns_normal_part3`
and `arena_alloc_aligned_zero_assigns_normal_part3` as inheritance from Cat 2b
through wrapper delegation. Pool Cat 2d says
`pool_alloc_zero_assigns_normal_part3` chains through `pool_alloc`'s
partially-unproved assigns.

**Code does.** `arena_alloc_zero` and `arena_alloc_aligned_zero` declare
`assigns *arena;` and `pool_alloc_zero` declares `assigns pool->used;`, but
each zeroes the returned bytes with `mem_zero`. Those bytes live in the
caller's buffer, outside `*arena` and `pool->used`, and existed before the
call, so each assigns clause is false whenever they were not already zero. The
three goals are false obligations, not inherited difficulty. Nor is there a
partially-unproved assigns to chain through: every `pool_alloc` assigns goal
proves.

**Probe.** `p_zero_assigns.c`. With the buffer filled with 0xFF,
`arena_alloc_zero(&a, 8)` and `pool_alloc_zero(&p)` each leave a byte reading
0x00 at a location outside the declared assigns set.

**Consequence.** A caller that knows the region's prior contents can derive a
contradiction from the frame together with the zeroing ensures. The zeroed
bytes were unallocated, so no live data changes; the defect is in the
contract, not the behaviour.

**Fix (author's decision).** Add the zeroed range to each assigns clause. For
the arena pair, the free tail of the buffer,
`((u8*)arena->buffer)[arena->offset .. arena->capacity - 1]`, covers every
byte the call can zero; for `pool_alloc_zero`, the free part of the reserved
window. Predicted to close the three goals; the author names the exact
clauses.

### What this changes in the count

No block retires yet, and *A* in force stays 16 until a fix closes a block's
last goal; F2 is predicted to retire VERIFY-010 Cat 2b. Each affected block
carries a 2026-10-05 coverage update naming its finding, with the original
argument kept legible, as for the retired Cat 2d. F2 retired VERIFY-010 Cat 2b
(CI #1328) and F6 retired VERIFY-010 Cat 2c (CI #1332), F3 retired VERIFY-009 Cat 2a (CI #1334), and F4 retired VERIFY-009
Cat 2b (CI #1336): *A* in force is 12.

All four defects sit in modules 5–7, the rising segment of the budget curve,
as VERIFY-023's 24 misattributions did.

### Proposed rule (decide before F1)

Every class (a) and (c) argument lists its hypotheses and, for each, where it
is established at the goal's program point: a precondition, an invariant, or a
guard dominating the point. F3 and F4 are failures of exactly this: the range
hypothesis held only after the guard, and the alignment hypothesis was never
established. F2 is a third: the non-null hypothesis was established by a check
the proof configuration compiles out. That is §3.2's configuration field,
applied to the function's own guard rather than to a cited control.

### Predictions (author — fill and commit before each fix)

Draft rows, AI-assisted: edit, then commit each row before its fix. "Vanishes"
means the goal is no longer generated; "closes" means it is generated and now
proved. Either moves the pinned proved line (gate 2) as well as the roll-call.

| Fix | Change | Own goals | Downstream | Blocks | Conf. |
|-----|--------|-----------|------------|--------|-------|
| F1 | unsigned negation in `mem_get_alignment` | `mem_get_alignment_assert_rte_signed_overflow` vanishes; `_nonnull_ensures_part2` and `_nonnull_ensures_2_part2` stay residual (the lowest-set-bit identity is bitwise reasoning) | the same name vanishes from every TU whose closure contains `core/memory.h`, `frama-c-arena-32` included; nothing else moves | VERIFY-008 Cat 2 covers 8 (was 9); none retires | H vanish; M ensures |
| F2(a) | `require_msg` replaced by `if (pool == NULL) return NULL;` | `pool_alloc_assert_rte_mem_access` closes; other `pool_alloc` fragment names may shift with the new branch | `frama-c-pool` only | VERIFY-010 Cat 2b covers nothing and retires; *A* in force 16 → 15 | H closure; M names |
| F2(b) | `requires pool_invariant(pool)` on `pool_alloc`, `pool_alloc_zero`, `pool_try_alloc` | `pool_alloc_assert_rte_mem_access` closes; `null_pool` goals vanish; no call-site goal opens if all three tighten | `frama-c-pool` only | as F2(a) | H |
| F3 | pad in integer arithmetic before the guard | the four `arena_alloc{,_aligned}_call_ptr_span_requires{,_2}` vanish; new call-site goals on the integer helper (author to name) | every arena-including TU, by the same delta | VERIFY-009 Cat 2a covers nothing and retires | author |
| F4 | address-based `arena_can_fit`, or an alignment precondition | the 16 Cat 2b goals become true; whether they prove is the open question | every arena-including TU, by the same delta | Cat 2b rewritten or retired; Cat 2c follows | author |
| F5 | add the zeroed bytes to the three zero wrappers' assigns | the three `*_zero_assigns_normal_part3` close | the two arena goals in every arena-including TU; pool's in `frama-c-pool` | Cat 2c covers 8 (was 10); Cat 2d covers 4 (was 5) | author |

F3 and F4 are also a fresh test of the inheritance claim: each predicts that
every arena-including unit moves by exactly the own delta.

### F1 — committed prediction (before the fix commit)

**Change.** `core/memory.h`, `mem_get_alignment`: `(uintptr_t)(-(intptr_t)addr)`
becomes `(~addr + (uintptr_t)1)`. No other source change. This supersedes the
F1 draft row above.

**Prediction.** `typed_cast_mem_get_alignment_assert_rte_signed_overflow` is no
longer generated. In each job below the proved count is unchanged, the total
and the unproved count fall by exactly 1, and the roll-call loses exactly that
name. No other job moves, and no other goal in these jobs changes status or
name.

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-memory` | 2837 / 2886, 49 | 2837 / 2885, 48 |
| `frama-c-arena` | 3496 / 3575, 79 | 3496 / 3574, 78 |
| `frama-c-arena-32` | 3496 / 3575, 79 | 3496 / 3574, 78; 64/32 symmetric difference stays empty |
| `frama-c-pool` | 3970 / 4061, 91 | 3970 / 4060, 90 |
| `frama-c-region` | 3644 / 3746, 102 | 3644 / 3745, 101 |
| `frama-c-vec` | 5337 / 5521, 184 | 5337 / 5520, 183 |
| `frama-c-bitset` | 4853 / 5022, 169 | 4853 / 5021, 168 |
| `frama-c-priority-queue` | 4521 / 4598, 77 | 4521 / 4597, 76; memory's CHECKS parses to 48 names |
| `frama-c-stringbuf` | 4767 / 4848, 81 | 4767 / 4847, 80 |
| `cc-vec` (`frama-c-cc.yml`) | 5394 / 5586, 192 | 5394 / 5585, 191 |

**Also predicted.** `mem_get_alignment_nonnull_ensures_part2` and
`_nonnull_ensures_2_part2` stay residual (M). MC/DC, MISRA and every
non-proof job are unchanged (M for MISRA). Confidence for the table: H.

**Scoring rule.** F1 holds if every row's observed proved line and unproved
count equal the prediction exactly and each roll-call differs from its pin by
exactly that one name. The fix commit is expected to turn exactly these ten
jobs red; the ratchet commit then moves exactly these pins, removes goal 7 from
VERIFY-008 Cat 2's table, and records the score here.

### F1 — scored (fix f1ba2de; CI #1326, CC #15)

**Result: the prediction held exactly.** Every row matches to the digit.

| Job | Predicted | Observed |
|-----|-----------|----------|
| `frama-c-memory` | 2837 / 2885, 48 | 2837 / 2885, 48 |
| `frama-c-arena` | 3496 / 3574, 78 | 3496 / 3574, 78 |
| `frama-c-arena-32` | 3496 / 3574, 78 | 3496 / 3574, 78 |
| `frama-c-pool` | 3970 / 4060, 90 | 3970 / 4060, 90 |
| `frama-c-region` | 3644 / 3745, 101 | 3644 / 3745, 101 |
| `frama-c-vec` | 5337 / 5520, 183 | 5337 / 5520, 183 |
| `frama-c-bitset` | 4853 / 5021, 168 | 4853 / 5021, 168 |
| `frama-c-priority-queue` | 4521 / 4597, 76 | 4521 / 4597, 76 |
| `frama-c-stringbuf` | 4767 / 4847, 80 | 4767 / 4847, 80 |
| `cc-vec` (`frama-c-cc.yml`) | 5394 / 5585, 191 | 5394 / 5585, 191 |

**By name.** Where a roll-call ran (arena-32, bitset, priority-queue,
stringbuf, cc-vec), the only difference from the pin was the one predicted
name, with nothing unpinned. The memory, arena, pool, region and vec jobs stop
at the proved-line gate before their roll-calls, so their by-name check is the
ratchet run; the containing units already confirm them: priority-queue finds
48 of memory's 49 names and stringbuf 80 of its 81, each missing only this
one. Both `mem_get_alignment` ensures stayed residual, as predicted (M). No
job outside the ten went red.

**Two readings of the red runs.** arena-32 reported the name as "residual at
64-bit, not at 32-bit": its baseline is an embedded copy of the 64-bit list,
still pre-fix. The 64-bit job lost the same name, so width invariance held (78
= 78); the embedded duplicate made a width-invariant change look like a width
difference until ratcheted, which is the limit §6.2 of the paper records for
that gate. Timeout/Unknown splits moved between units (arena 76/2, arena-32
74/4) while every pooled count was exact, as P5 records.

**What it changes.** VERIFY-008 Cat 2 covers 8 goals; no block retires and *A*
in force stays 16. One code change, made because the obligation was false,
closed one pinned name in every unit whose closure contains `core/memory.h`,
at unit gain, exactly where predicted. `p_getalign.c` now runs clean under
UBSan.

### F2 — committed prediction, option (b) (before the fix commit)

**Change.** `core/pool.h`: `requires pool_invariant(pool);` replaces `requires
pool == \null || pool_invariant(pool);` on `pool_alloc`, `pool_alloc_zero`,
`pool_try_alloc` and `pool_try_alloc_zero`; `pool_alloc`'s `null_pool`
behaviour is deleted; the doc comments state the precondition. No executable
change: each function already rejects NULL with `require_msg`.

**Why (b).** The code, its `require_msg` messages and its doc comment already
treat NULL as a precondition violation, and arena draws the same line:
allocation requires a live arena, while reset and teardown accept NULL. Option
(a) would have turned the default build's abort into a silent NULL return.

**Prediction.** Only `frama-c-pool` moves; no other verified unit includes
`core/pool.h`.

- Exact (H): unproved 90 → 89. The roll-call loses exactly
  `typed_cast_pool_alloc_assert_rte_mem_access`, which closes because the
  precondition now gives `pool != \null` before the first access. No residual
  appears.
- Count: total 4060 → 4060 − g and proved 3970 → 3971 − g, where g is the number
  of goals WP generated for the deleted `null_pool` behaviour (its ensures and
  assigns, split fragments included). g is not predicted to the digit; estimate
  2 to 4 (L).
- Name exposure (M): `pool_alloc_zero_assigns_normal_part3` stays residual (it
  is false, F5), but its fragment index may shift, since the call it splits over
  loses a behaviour. If it moves, one name leaves and one arrives for the same
  obligation.
- No call-site goal opens: the only verified callers of `pool_alloc` are the
  three wrappers tightened with it (H). MC/DC, MISRA, tests and sanitizers do
  not move: the edit is ACSL and comments only (H).

**Blocks.** VERIFY-010 Cat 2b covers nothing and retires; *A* in force 16 → 15
(H). The goal closes because the contract changed, not because the block's
argument was right: it never applied to this goal.

**Scoring rule.** F2 holds if unproved is 89, the roll-call loses exactly that
name with nothing new beyond the stated fragment-index exposure, and no job
other than `frama-c-pool` goes red. The proved and total lines are scored by
the observed g.

### F2 — scored (fix 8163f06; CI #1328, CC #17)

**Result: every exact prediction held; the count estimate missed.**

| Item | Predicted | Observed |
|------|-----------|----------|
| unproved, `frama-c-pool` | 89 | 89 |
| roll-call | loses exactly `pool_alloc_assert_rte_mem_access` | loses exactly that name; nothing new |
| `pool_alloc_zero_assigns_normal_part3` | stays residual; index may shift (M) | stays residual; index unchanged |
| jobs red | `frama-c-pool` only | `frama-c-pool` only |
| total | 4060 − g | 4050 |
| proved | 3971 − g | 3961 |
| g | 2 to 4 (L) | 10 — missed |

**The closure confirms the mechanism.** F2 added one fact to `pool_alloc`'s
proof: `pool != \null`. On every non-null path `pool_invariant` was already a
hypothesis. A goal that closes under F2 can therefore only have failed on the
NULL path, so VERIFY-010 Cat 2b's slot-address attribution is refuted by
measurement, not only by reading; the JSON source-line check F2 asked for is
no longer needed.

**The count.** Total and proved both fell by the same g, net of the one
closure, exactly as the relation predicted; g itself was 10, not 2 to 4. Where
the ten come from is not traced: the job archives no per-goal report. As with
VERIFY-034's ×5, a count for a contract edit should be predicted from a goal
list, not from the contract text.

**By name.** The pool job stops at the proved-line gate before its roll-call;
the printed unproved list shows the memory and arena blocks unchanged and the
pool section short exactly the predicted name. The by-name gate runs in the
ratchet. One inherited goal moved from Timeout to Unknown
(`arena_try_alloc_non_null_out_ensures_part2`) while every pooled count stayed
exact, as P5 records.

**What it changes.** VERIFY-010 Cat 2b covers nothing and retires: *A* in
force 16 → 15. Runtime behaviour, MC/DC and MISRA (54 = 54) did not move.

### F5 — committed prediction (before the fix commit)

**Change.** The zeroed bytes join the assigns clauses. `arena_alloc_zero` and
`arena_alloc_aligned_zero` gain `((char *)arena->buffer)[arena->offset ..
arena->capacity - 1]`, the free tail, which contains every byte the call can
zero. `pool_alloc_zero` and `pool_try_alloc_zero` gain `((char
*)pool->arena->buffer)[pool->base_mark .. pool->end_mark - 1]`, the reserved
window. The `char` typing matches `mem_zero`'s own assigns clause. No
executable change. `pool_try_alloc_zero` is included because it calls
`pool_alloc_zero`: left narrow, its frame would become the next false clause.

**Prediction.**

- (M) The two arena goals close: `arena_alloc_zero_assigns_normal_part3` and
  `arena_alloc_aligned_zero_assigns_normal_part3`. `arena_alloc`'s contract
  already gives the result's address, the offset bound and the frame of `buffer`
  (VERIFY-033), which place the zeroed bytes inside the free tail. They close in
  every unit that pins them. Risk: the inclusion can still fail on location
  typing under Typed+Cast; then the goals stay, true.
- (H) `pool_alloc_zero_assigns_normal_part3` stays residual, now true.
  `pool_alloc` states nothing about where its result lies, so WP cannot place
  the zeroed slot inside the window. That is VERIFY-023's opaque-result pattern
  one level up, and it is also why `pool_alloc_zero_call_mem_zero_requires` (Cat
  2c) does not prove. Stating `pool_alloc`'s result address is a follow-up (F6),
  not part of F5.
- (M) No goal opens, `pool_try_alloc_zero` included, and every total is
  unchanged: an assigns clause gains a location, not a goal.

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-arena` | 3496 / 3574, 78 | 3498 / 3574, 76 |
| `frama-c-arena-32` | 3496 / 3574, 78 | 3498 / 3574, 76; 64/32 set equality holds |
| `frama-c-pool` | 3961 / 4050, 89 | 3963 / 4050, 87 |
| `frama-c-region` | 3644 / 3745, 101 | 3646 / 3745, 99 |
| `frama-c-vec` | 5337 / 5520, 183 | 5339 / 5520, 181 |
| `frama-c-stringbuf` | 4767 / 4847, 80 | 4769 / 4847, 78 |
| `cc-vec` (`frama-c-cc.yml`) | 5394 / 5585, 191 | 5396 / 5585, 189 |

Memory, bitset and priority-queue do not include `core/arena.h` and do not
move.

**Blocks.** Arena Cat 2c covers 8 (was 10). Pool Cat 2d still covers 5, but
its argument for the `pool_alloc_zero` goal is wrong and is rewritten with F6.
No block retires; *A* in force stays 15.

**Scoring rule.** F5 holds if the two arena goals leave every listed roll-call
and nothing else moves; the pool goal staying is part of the prediction.

### F5 — scored (fix c5281dc; CI #1330, CC #18)

**Result: the prediction held exactly**, both medium-confidence closures included.

| Job | Predicted | Observed |
|-----|-----------|----------|
| `frama-c-arena` | 3498 / 3574, 76 | 3498 / 3574, 76 |
| `frama-c-arena-32` | 3498 / 3574, 76 | 3498 / 3574, 76 |
| `frama-c-pool` | 3963 / 4050, 87 | 3963 / 4050, 87 |
| `frama-c-region` | 3646 / 3745, 99 | 3646 / 3745, 99 |
| `frama-c-vec` | 5339 / 5520, 181 | 5339 / 5520, 181 |
| `frama-c-stringbuf` | 4769 / 4847, 78 | 4769 / 4847, 78 |
| `cc-vec` (`frama-c-cc.yml`) | 5396 / 5585, 189 | 5396 / 5585, 189 |

**By name.** Where a roll-call ran (arena-32, stringbuf, cc-vec), exactly the
two arena goals left and nothing new arrived.
`pool_alloc_zero_assigns_normal_part3` stayed in the pool list, as predicted
(H), and no `pool_try_alloc_zero` goal appeared. Every total was unchanged, as
predicted. arena-32 again reported the closures against its embedded pre-fix
baseline; the 64-bit job lost the same two names, so width invariance held. No
job outside the seven went red.

**What the closures show.** The two goals closed the moment their clause
stated what the code writes, with nothing else changed, so they were false
obligations, not inherited difficulty: Cat 2c's attribution to the fits chain
is refuted by measurement for both. Arena Cat 2c covers 8 (was 10). Pool Cat
2d still covers 5, its `pool_alloc_zero` goal now true and still misattributed
until F6. No block retires; *A* in force stays 15.

### F6 — `pool_alloc` states its result address (committed prediction)

**Why.** F5 left `pool_alloc_zero_assigns_normal_part3` true but unprovable,
and `pool_alloc_zero_call_mem_zero_requires` (pool Cat 2c) fails for the same
reason: `pool_alloc`'s contract says nothing about where its result lies, so
WP cannot place the slot `pool_alloc_zero` zeroes. That is VERIFY-023's
opaque-result pattern one level up: VERIFY-023 made `ptr_elem` state its
result; `pool_alloc`, which returns `ptr_elem`'s result, never passed it on.

**Change.** One clause in `pool_alloc`'s `alloc` behaviour: `ensures address:
(u8*)\result == pool->arena->buffer + pool->base_mark + \old(pool->used) *
pool->object_size;` No executable change.

**Prediction.** Only `frama-c-pool` moves; no other verified unit includes `core/pool.h`.

- (H/M) The new clause proves: `ptr_offset` and `ptr_elem` already state their
  results (VERIFY-023), so the address is their composition. No residual
  appears.
- (M) Both `pool_alloc_zero` goals close:
  `pool_alloc_zero_call_mem_zero_requires` and
  `pool_alloc_zero_assigns_normal_part3`. Each needs the slot inside the
  reserved window, which is the nonlinear step `(used + 1) * object_size <=
  capacity * object_size` from `used < capacity`. They rest on the same fact, so
  they should close or stay together; if they stay, they are true solver-limit
  residuals and get a correct class (a) argument.
- (M) The clause adds two goals, one per path of `pool_alloc`'s early return,
  the infeasible one discharged as unreachable.

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-pool` | 3963 / 4050, 87 | 3967 / 4052, 85 |

**Blocks.** If both close: pool Cat 2c covers nothing and retires (*A* in
force 15 → 14), and pool Cat 2d covers 4 (was 5).

**Scoring rule.** F6 holds if no residual appears, both `pool_alloc_zero`
goals leave the roll-call, the total grows by 2, and only `frama-c-pool` goes
red.

### F6 — scored (fix c069881; CI #1332, CC #20)

**Result: the prediction held exactly**, both medium-confidence closures and the goal count
included.

| Item | Predicted | Observed |
|------|-----------|----------|
| `frama-c-pool` | 3967 / 4052, 85 | 3967 / 4052, 85 |
| new `address` clause | proves; no residual | no residual |
| `pool_alloc_zero` goals | both close (M) | both closed |
| goals added | 2 (M) | 2 |

**By name.** The pool section of the unproved list is the five `pool_init`
goals and the four `pool_reset` goals; both `pool_alloc_zero` names are gone
and nothing new appears. The pool job stops at the proved-line gate, so its
by-name gate runs in the ratchet. This run also stood in for the F5 ratchet's
confirmation, which the F6 push cancelled: every job F6 cannot reach was
green.

**What the closures show.** The nonlinear step the prediction flagged as the
risk, the slot inside the reserved window, did not stop the provers. The two
goals closed only when `pool_alloc` passed on the address it already computed,
which is VERIFY-023's opaque-result pattern one level up. Pool Cat 2c's stated
cause, the `ptr_offset` boundary, had been stale since VERIFY-023 removed it:
one more misattributed obligation, at obligation level. Cat 2c retires and Cat
2d covers 4: *A* in force 15 → 14.

### F3 — committed prediction (before the fix commit)

**Design (decided 2026-10-06).** Both allocators compute `pad` with
`ptr_align_padding(current, alignment)`, which works on the address as an
integer, instead of forming `ptr_align_up(current, alignment)` and subtracting
it with `ptr_span`. No pointer past the buffer is formed and no pointer
subtraction remains; the result pointer is still formed by `ptr_offset` after
the capacity guard, as now. Run-time results are unchanged wherever the old
code was defined. F4, the predicate, follows as its own change.

**Prediction.**

- (H) The four `arena_alloc{,_aligned}_call_ptr_span_requires{,_2}` residuals
  vanish from every unit that pins them: arena, arena-32, pool, region, vec,
  stringbuf and cc-vec. No residual appears: `ptr_align_padding`'s two requires
  are the pair `ptr_align_up`'s were, and those proved.
- (M) In each of those units the total falls by 8 and proved by 4: each
  allocator loses four `ptr_span` and two `ptr_align_up` call-site goals and
  gains two `ptr_align_padding` ones.
- (M/H) The 16 Cat 2b goals stay residual under unchanged names. `pad` is still
  known only to lie in `[0, alignment)`, which is what `ptr_align_up` gave; they
  stay false for unaligned buffers until F4.
- `p_arena_span.c` runs clean under ASan pointer-pair checking; `p_arena_pad.c`
  still reports three violations.

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-arena` | 3498 / 3574, 76 | 3494 / 3566, 72 |
| `frama-c-arena-32` | 3498 / 3574, 76 | 3494 / 3566, 72; 64/32 set equality holds |
| `frama-c-pool` | 3967 / 4052, 85 | 3963 / 4044, 81 |
| `frama-c-region` | 3646 / 3745, 99 | 3642 / 3737, 95 |
| `frama-c-vec` | 5339 / 5520, 181 | 5335 / 5512, 177 |
| `frama-c-stringbuf` | 4769 / 4847, 78 | 4765 / 4839, 74 |
| `cc-vec` (`frama-c-cc.yml`) | 5396 / 5585, 189 | 5392 / 5577, 185 |

**Blocks.** VERIFY-009 Cat 2a covers nothing and retires: *A* in force 14 →
13. Its goals vanish rather than close: the call that generated them is gone.

**Scoring rule.** F3 holds if the four names leave every listed roll-call with
nothing new, and only the seven listed jobs go red. The counts are scored as
observed.

### F3 — scored (fix baf8ee9; CI #1334, CC #21)

**Result: exact on every goal name and every unproved count; two misses, both recorded.**

| Job | Predicted | Observed |
|-----|-----------|----------|
| `frama-c-arena` | 3494 / 3566, 72 | 3490 / 3562, 72 |
| `frama-c-arena-32` | 3494 / 3566, 72 | 3490 / 3562, 72 |
| `frama-c-pool` | 3963 / 4044, 81 | 3959 / 4040, 81 |
| `frama-c-region` | 3642 / 3737, 95 | 3638 / 3733, 95 |
| `frama-c-vec` | 5335 / 5512, 177 | 5331 / 5508, 177 |
| `frama-c-stringbuf` | 4765 / 4839, 74 | 4761 / 4835, 74 |
| `cc-vec` (`frama-c-cc.yml`) | 5392 / 5577, 185 | 5388 / 5573, 185 |

**By name.** Where a roll-call ran (arena-32, stringbuf, cc-vec), exactly the
four `ptr_span` names left and nothing new arrived; the 16 Cat 2b names
stayed, unchanged. The 64-bit arena job lost the same four, so width
invariance held.

**Miss 1, the count (M).** Every unit lost 12 goals and 8 proved, not 8 and 4.
The unproved change was exact everywhere, so the four extra goals per unit
were all proved ones, and the prover breakdown places them: in arena, pool and
region alike, Qed's count fell by 4 more than predicted. Four trivially
discharged goals per unit went with the old calls; which ones is not traced,
the jobs keep no per-goal report. As with F2: count from a goal list.

**Miss 2, the job set.** `mingw (Release)` went red, outside the seven. Its
GCC, at -O3, rejected `test/util/intern_test.c` with -Wmaybe-uninitialized:
most tests ignore `fixture_init`'s result, and a failed `intern_pool_init`
leaves `pool.arena` unset. F3 changed what the optimiser could see of
`arena_alloc`, and the latent read became visible. Neither Linux GCC 13 nor 14
reproduces it. The ratchet carries a test-only fix, zeroing the fixture first;
predicted: `mingw (Release)` green, no proof job moves.

**What it changes.** No pointer past the buffer is formed and no pointer
subtraction remains; `p_arena_span.c` runs clean under ASan pointer-pair
checking, and old and new allocators agree on 80,000 random allocations.
VERIFY-009 Cat 2a retires by removal: *A* in force 14 → 13. CI #1333 (the F6
ratchet), attempt 1: `frama-c-lifetime` hit its 15-minute limit; the re-run
passed in about a minute.

### F4 — committed prediction (before the fix commit)

**Design (decided 2026-10-07).** The contract is made to say what the code
does; buffers are not required to be aligned. That would break callers passing
ordinary byte arrays and would still leave `arena_alloc_aligned` wrong above
16. Two clauses, one commit:

- `ptr_align_padding` (ptr.h) states its exact result in its `nonnull`
  behaviour, the expression its body already gets from `align_padding`: `ensures
  exact: \result == (((usize)(uintptr_t)p + align - 1) & ~(align - 1)) -
  (usize)(uintptr_t)p;` It promised only `\result < align`: the opaque-result
  pattern, a third time.
- `arena_can_fit` (arena.h) computes the pad from the address with the same
  expression, `addr = (usize)(uintptr_t)(a->buffer + a->offset)`, instead of
  from the offset.

No executable change. The four units that include `ptr.h` but not `arena.h`
(ptr, memory, bitset, priority-queue) isolate the first clause; the arena
units show both.

**Prediction.**

- (H/M) The `exact` clause proves: it is `align_padding`'s ensures with its
  argument substituted. No residual appears in any of the eleven units that
  verify `ptr.h`.
- (M) It adds two goals per unit, one per path of the null test, the null path
  unreachable.
- (M) The 16 Cat 2b goals close in all seven arena units. With both pads now the
  same expression of the same address, the guard and the predicate say the same
  thing, and what is left is linear. The risk is one step: WP must see that
  `(uintptr_t)current` and `(uintptr_t)(a->buffer + a->offset)` are the same
  integer, from `ptr_offset`'s `(u8*)\result == (u8*)p + n`; pointer-to-integer
  conversion is where WP is weakest. If that step fails, the 16 stay, now true,
  and nothing else moves.
- (M) Cat 2c's eight goals stay; their failures are not the fits chain's. Parts
  1 to 3 of the fits goals, which prove from the capacity bound alone, stay
  proved.

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-ptr` | 1957 / 1973, 16 | 1959 / 1975, 16 |
| `frama-c-memory` | 2837 / 2885, 48 | 2839 / 2887, 48 |
| `frama-c-bitset` | 4853 / 5021, 168 | 4855 / 5023, 168 |
| `frama-c-priority-queue` | 4521 / 4597, 76 | 4523 / 4599, 76 |
| `frama-c-arena` | 3490 / 3562, 72 | 3508 / 3564, 56 |
| `frama-c-arena-32` | 3490 / 3562, 72 | 3508 / 3564, 56; 64/32 set equality holds |
| `frama-c-pool` | 3959 / 4040, 81 | 3977 / 4042, 65 |
| `frama-c-region` | 3638 / 3733, 95 | 3656 / 3735, 79 |
| `frama-c-vec` | 5331 / 5508, 177 | 5349 / 5510, 161 |
| `frama-c-stringbuf` | 4761 / 4835, 74 | 4779 / 4837, 58 |
| `cc-vec` (`frama-c-cc.yml`) | 5388 / 5573, 185 | 5406 / 5575, 169 |

**Blocks.** If the 16 close, VERIFY-009 Cat 2b covers nothing and retires: *A*
in force 13 → 12. It would be the first false block to retire by its goals
becoming true and then proving, rather than by removal or a contract that
stopped claiming them.

**Scoring rule.** Scored by part: the `exact` clause (no residual, two goals,
eleven units), then the 16 (close or stay, all together), then the job set
(only the eleven).

### F4 — scored (fix e953219; CI #1336, CC #23)

**Result: exact in all eleven units**, the medium-confidence closure of the 16 included.

| Job | Predicted | Observed |
|-----|-----------|----------|
| `frama-c-ptr` | 1959 / 1975, 16 | 1959 / 1975, 16 |
| `frama-c-memory` | 2839 / 2887, 48 | 2839 / 2887, 48 |
| `frama-c-bitset` | 4855 / 5023, 168 | 4855 / 5023, 168 |
| `frama-c-priority-queue` | 4523 / 4599, 76 | 4523 / 4599, 76 |
| `frama-c-arena` | 3508 / 3564, 56 | 3508 / 3564, 56 |
| `frama-c-arena-32` | 3508 / 3564, 56 | 3508 / 3564, 56 |
| `frama-c-pool` | 3977 / 4042, 65 | 3977 / 4042, 65 |
| `frama-c-region` | 3656 / 3735, 79 | 3656 / 3735, 79 |
| `frama-c-vec` | 5349 / 5510, 161 | 5349 / 5510, 161 |
| `frama-c-stringbuf` | 4779 / 4837, 58 | 4779 / 4837, 58 |
| `cc-vec` (`frama-c-cc.yml`) | 5406 / 5575, 169 | 5406 / 5575, 169 |

**By part.** The `exact` clause proved and added two goals in all eleven
units; the four units without `arena.h`, which isolate it, moved by exactly
that and nothing else. The 16 Cat 2b names left every roll-call that ran
(arena-32, stringbuf, cc-vec, bitset's and priority-queue's set equality
held), with nothing new; the 64-bit arena job lost the same 16, so width
invariance held. Cat 2c's eight stayed, as predicted. Only the eleven went
red.

**What the closure shows.** The step flagged as the risk, WP identifying
`(uintptr_t)current` with `(uintptr_t)(a->buffer + a->offset)`, held. These
are the first false goals in the campaign retired by becoming true and then
proving, rather than by removal or by a contract that stopped claiming them.
Run-time evidence agrees: `p_arena_pad.c` now reports the pre-F4 contract
violated and the current one satisfied in all three cases, and on 160,000
random allocations over skewed buffers the current predicate disagrees with
the code 0 times, the pre-F4 one 1,529 times.

**What else it shows.** Cat 2c filed its eight goals as inheritance from Cat
2b. Cat 2b now proves and the eight did not move, so that attribution is
refuted by measurement: eight more misattributed obligations. VERIFY-009 Cat
2b retires: *A* in force 13 → 12.

### For the paper

§3.3's worked example is F2's goal, and its explanation changes. §5 gains a row
for this audit (17 blocks read against their code; 4 defects; 6 of 17 blocks
defective). §3.4 should say that some pinned residuals were false obligations
rather than true-but-unprovable ones.

### Scoring

Each fix is scored goal by goal in a later record, as VERIFY-027 scored
VERIFY-026. Probes become regression tests as their fixes land, except
`p_arena_span.c`, which needs ASan's pointer-pair checking and stays a recorded
probe unless a sanitizer job adopts it.

## VERIFY-037: Pre-registration — vec and option at a Struct Type: Extending the Catalogue the §4.3 Test Is Measured Against

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-037 |
| **Date**       | 2026-10-08 |
| **Status**     | PRE-REGISTERED — the driver is not yet in CI; the predicted pin is committed with this entry |
| **Baseline**   | da323da (CI #1337 substrate; `tools/idioms` catalogue at 6290f4a) |
| **Scope**      | a bare `-CC` driver, `vmacros/vdrivers/vec_struct_cc_experiment.h`, instantiating `option(Rec)`, `result(bool, Error)` and `vec(Rec)` with `Rec = struct { u32 id; i64 due; }`; verified as a new `-CC` family `vec_struct` (Typed+Cast, the `cc-vec` flags) |
| **Category**   | Catalogue extension for the §4.3 test; a third prospective test of P1 (zero new argument blocks) |

**Why this record exists.** The committed test of the paper's §4.3 needs a
program *P* with n(*P*) = 0, computed by `tools/idioms` from source text
alone. `vec` and `option` have only ever been verified at integer types, so a
realistic program that stores records in a `vec` measures n(*P*) = 1
(`family:vec@struct`) and leaves the zero case untested. Rather than shape *P*
around the catalogue, this record extends the catalogue: it verifies both
families at a struct type. Under `-CC` the contracts live in the macro bodies
(VERIFY-025), so the struct instantiation is verified against the same
contract text as `cc-vec`'s, with nothing duplicated that could drift.

**What changes at a struct type.** Element equality in the contracts becomes
structural equality on records — `v->items[i] == val` in `set`, the
`v->items[\old(v->len)] == item` postconditions of the push family, and every
frame clause `v->items[k] == \old(v->items[k])`; element assignment becomes a
struct copy; and the element size passed to `mem_move` and `mem_copy` becomes
16 bytes instead of 4. Nothing else in the translation unit differs: the
result instantiation and the substrate are the same, and `gcc -E -CC` exposes
169 ACSL blocks for both drivers.

**Measured now (not predicted).** `tools/idioms` measures the driver at
n = 2 against the da323da catalogue: `family:option@struct` and
`family:vec@struct`, and nothing else.

### Predictions

- **P1 (zero new argument blocks).** Every residual of `vec_struct` is covered
  by an argument already in the record, judged against the code, not the name.
- **P2 (by name; committed as `cc_pins/vec_struct.txt`).** The residual set is
  `cc_pins/vec.txt` with `vec_int_` renamed to `vec_Rec_` and `option_int_` to
  `option_Rec_`: 169 names, of which 85 are renamed. Scored by part:
  - P2a (H): the option arm, 32 names, renamed exactly. Its residuals are
    function-pointer, termination and frame shapes that do not mention the
    element type.
  - P2b (H): the result arm (28) and the substrate arm (56) are byte-identical:
    the same instantiation and the same includes.
  - P2c (M): the vec arm, 53 names, renamed exactly. If it fails, every
    difference is in a function whose contract compares elements — `set`, the
    push family, `insert`, `remove`, `swap`, `fill`, `pop` — the
    structural-equality sites.
- **P3 (counts, L).** 5406 / 5575 proved, 169 unproved — identical to `cc-vec`,
  since the annotations and the runtime-error structure are the same. Committed
  as the matrix entry. Count predictions have missed before (VERIFY-034,
  VERIFY-036 F2 and F3), hence the low confidence.
- **P4 (H).** No Failed, Invalid or Stepout goal.

**Scoring rule.** By part, in the order above. The `-CC` gate scores P2 and P3
mechanically: a missing name is a renamed residual that proved, an unpinned
name is a new one. P1 is scored after classification: an unpinned residual
that no existing argument's mechanism covers, read against the code, refutes
it; its class and shape are then the result.

**Procedure.** This commit holds the prediction and the predicted pin. The
next commit adds the driver and the matrix entry; the `-CC` run scores it. If
the job is green, P1 to P4 hold; if not, the gate output says which part
failed. After scoring, `tools/idioms` is extended to read the `-CC` matrix and
the catalogue is rebuilt, so that `option@struct` and `vec@struct` become
catalogued and the driver itself measures n = 0.

## VERIFY-038: Scoring VERIFY-037 — Unscored, Its Premise Refuted: option's Contracts State Zero With an Integer Literal; Re-registered With an Aggregate Variant

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-038 |
| **Date**       | 2026-10-08 |
| **Status**     | VERIFY-037 SCORED (unscorable, premise refuted); RE-REGISTERED — predictions below precede the fix commit |
| **Evidence**   | CC #26 (99613e4), job `frama-c-cc-vec_struct`: Frama-C aborted in the kernel; WP never ran |
| **Scope**      | `semantics/option/option_defn.h` (new aggregate variants of `none` and `take`), `vmacros/vdrivers/vec_struct_cc_experiment.h`, the `vec_struct` matrix entry |

**What happened.** Frama-C rejected the translation unit before generating a
single goal:

    [kernel:annot-error] vec_struct_cc_experiment.h:19: Warning:
      incompatible types ℤ and Rec. Ignoring logic specification of function option_Rec_none
    [kernel] User Error: warning annot-error treated as fatal error.

`DEFINE_OPTION_NONE` promises `ensures \result.value == 0;` and
`DEFINE_OPTION_TAKE` promises `ensures o->value == 0;`. At an integer element
both type-check; at a struct they compare a record with an integer literal,
and the contract is ill-typed. Frama-C stops at the first such error, so only
`none` was reported; `take` carries the same clause. A scan of every
`Rec`-typed contract after `-CC` expansion finds no third site.

**Score.** P1 to P4 are unscorable: no goal exists. The premise is refuted.
VERIFY-037 said that at a struct type "nothing else in the translation unit
differs" beyond structural equality, struct copies and the element size. That
was wrong: `option`'s contracts are not type-generic. They state the zero
value of an empty option with an integer literal, so `option` at a struct type
was never verifiable as written. No residual and no argument is involved; the
finding is a contract that only type-checks at scalar element types, surfaced
by the catalogue extension before any goal existed. The prediction did not
anticipate it.

**Decision.** Three repairs were considered. Dropping both clauses everywhere
would give one macro for every type but remove a verified guarantee — no
indeterminate memory exposed through `.value` — from every scalar
instantiation, and re-pin about ten jobs. Comparing against a zero-initialized
`static const` of type *T* would keep the guarantee generically, but changes
every scalar contract with uncertain provability. Chosen: **aggregate
variants**. `DEFINE_OPTION_NONE_AGGREGATE` and `DEFINE_OPTION_TAKE_AGGREGATE`
are the same functions with the zero clause removed, assembled by
`DEFINE_OPTION_FUNCTIONS_AGGREGATE` and `DEFINE_OPTION_ALL_AGGREGATE`. Every
existing instantiation keeps its contract exactly; a struct instantiation gets
an honest, weaker one. The C bodies are unchanged, so the value is still
zero-initialized; the contract no longer states it, because ACSL has no
type-generic zero.

### Re-registered predictions (for the fix commit)

- **P0 (M).** Frama-C accepts the corrected translation unit: the two zero
  clauses were the only scalar-only constructs, and the structural equalities in
  `vec`'s contracts are well-typed. A further `annot-error` would be the next
  finding, recorded the same way.
- **P1.** Zero new argument blocks.
- **P2.** The residual set is `cc_pins/vec_struct.txt` as committed with
  VERIFY-037 (38404b9), unchanged: the removed clauses were proved goals at
  `int`, so removing them removes no residual. P2a (option arm, H), P2b (result
  and substrate arms, H) and P2c (vec arm, M) as before.
- **P3 (M).** 5404 / 5573 proved, 169 unproved: two goals fewer than `cc-vec`,
  one per removed clause (`option_*_none`'s second `ensures`, `option_*_take`'s
  fourth), each proved at `int`.
- **P4 (H).** No Failed, Invalid or Stepout goal.
- **P5 (H).** No existing job moves: the new macros are used only by the
  `vec_struct` driver.

**Scoring rule.** As VERIFY-037: by part, P0 first; the `-CC` gate scores P2
and P3 mechanically, P1 after classification against the code.

### VERIFY-038 — scored (CC #27, 98b5283)

| Part | Predicted | Observed | Score |
|------|-----------|----------|-------|
| P0 (M) | Frama-C accepts the unit | accepted; WP scheduled 5335 goals; exit 0 | held |
| P1 | zero new argument blocks | two new residuals that no existing argument covers, read against the code | refuted as stated; resolution pending (VERIFY-039 G1) |
| P2a (H) | option arm, 32 names renamed | 32, exact | held |
| P2b (H) | result (28) and substrate (56) arms byte-identical | exact | held |
| P2c (M) | vec arm, 53 names renamed | all 53 present (one under a mis-transcribed pin name), plus 2 new | missed |
| P3 (M) | 5404 / 5573, 169 unproved | 5402 / 5573, 171 unproved | total exact; proved and unproved off by the 2 new residuals |
| P4 (H) | no Failed, Invalid or Stepout goal | none | held |
| P5 (H) | no existing job moves | pending confirmation | — |

**The two new residuals.** `typed_cast_vec_Rec_get_hit_ensures_2_part4` and
`typed_cast_vec_Rec_iter_next_yield_ensures_3_part5`. Both are the
postcondition of a direct element copy through a caller's pointer — `*out =
v->items[i]` in `get`, `*out = it->vec->items[it->index++]` in `iter_next` —
stating that `*out` equals the source element, and both contracts require of
`out` only `out == \null || \valid(out)`. P2c's localization held, since both
functions compare elements in their contracts, but its list (set, the push
family, insert, remove, swap, fill, pop) omitted the two functions that copy
an element out through a pointer parameter. Neither involves `mem_copy` or
`mem_move`, so VERIFY-018 category (d), whose stated mechanism is those
functions' frame-only contracts, cannot cover them, and no other existing
argument does: P1 is refuted as stated. At `int` both postconditions are
proved. No C probe can show them false: `*out` may coincide with the source
element exactly, which leaves the claim true, and any partial overlap is
undefined behaviour for a struct assignment (C11 6.5.16.1p3). The hypothesis,
to be tested by intervention, is that both fail on the same unconstrained
`out` as VERIFY-039 G1, which a two-field element makes visible to WP where a
one-cell element does not. If requiring `out` to be separated from the buffer
closes them, no argument is needed and P1's conclusion survives by a contract
change, not unaided, as with range.h and stringbuf.h; if it does not, they are
a different mechanism and get an argument of their own.

**An error in the committed pin.** `cc_pins/vec_struct.txt` (38404b9) lists
`typed_cast_vec_Rec_alloc_call_vec_int_init_requires_2`. The renaming replaced
only the name's prefix, but the goal also names its callee, so the correct
name is `typed_cast_vec_Rec_alloc_call_vec_Rec_init_requires_2`, which the run
reports. The goal behaved as predicted; the committed artifact was wrong in
one name. The gate counts it as one missing and one unpinned name, so the
roll-call "missing 1, unpinned 3" is this error plus the two new residuals.

**A broken advisory parse.** The report-only step reads the driver's
`EXPECTED_UNPROVED=` with `grep -o '[0-9]*'` over the whole line. Since the
VERIFY-036 F3 ratchet that line carries a comment with earlier counts, so for
`cc-vec` and `vec_struct` the step has printed a list of numbers and a shell
error instead of one count. No gate reads it; it is fixed with the next
workflow change.

**Disposition.** The pin is not ratcheted yet. The `-CC` gate's rule is to
classify new residuals before ratcheting, and the classification runs through
VERIFY-039 G1's fix; the pin correction goes in with that ratchet.


## VERIFY-039: Making Every Filing Explicit — G1: vec's Out-Parameters Are Not Separated From the Buffer; Two Frame Postconditions Are False and Misfiled

| Field          | Value |
|----------------|-------|
| **ID**         | VERIFY-039 |
| **Date**       | 2026-10-08 |
| **Status**     | OPEN — G1 and G2 closed, both scored exact (G1: fix 40cccd9, CI #1341 / CC #28; G2: fix f4a846a, CI #1343 / CC #30); G3 closed (fix 76a1f94, CI #1344 / CC #31: the new postcondition held, the two Cat 2c closures missed, the mechanism identified; ratchet confirmed at CI #1345 / CC #32); G4: the probe scored (run #1: both false frames proved, the count missed by one), listing entries 1 (arena) and 2 (pool) repaired and scored exact (fixes 7e5757f and f3d9520, CI #1346 / CC #33 and CI #1347 / CC #34: nothing moved, as predicted); G5 opened (VERIFY-033 F4's aliasing hypothesis taken up: a `Pool` may overlap its `Arena`, `pool_init`'s first postcondition is then not true for an admitted call, and four goals filed under arithmetic or delegation state no arithmetic); G4 listing entries 3 to 5 found (a `Bitset` in its own words, `diag_render` into a buffer over the `Diag`, a vec in its own free tail or live elements: twelve more goals proved and false in defined C, one of them in two units); G6 opened (bitset's single-bit residuals false on LP64 for the same `Bitset`, filed as prover weakness, and one proved goal that rests on them); the Linux runner pin scored exact (pin 25133fe, CI #1348 / CC #35, G4 probe run #3: nothing moved); G5 repaired and scored exact (fix 528bfa1, CI #1349 / CC #36: seven goals closed, two stayed, as predicted); G7 opened, repaired and scored exact (`pool_reset` after its arena is reset; fix f5c616c, CI #1350 / CC #37: pool Cat 2d's last goal closed, and the block retired); G8 opened (`bitset_not`'s postcondition and loop invariant are false as written; the invariant's unproved preservation is filed as array framing, and the postcondition is proved from it); the rest of the audit in progress |
| **Baseline**   | 98b5283 (CI #1340, CC #27) |
| **Scope**      | (1) the 343 obligations the record files by function, family convention, category or module prose, each to be filed under one named argument, goal by goal; (2) the 135 container filings (vec, deque, bitset, priority queue) read against the code — the flat-segment audit the paper's §8 says has not been done |

**Why this record exists.** At CI #1337 the record files 59 of the 402
residual obligations under a counted block by goal name. The rest are filed by
function and count (70: option, result, borrow, diag), by family convention
(94: fresh instantiations), by category in the container records (135: vec,
deque, bitset, priority queue), or under the primitives' uncounted
module-level prose (44). The paper's §2 defines a record as a coverage
relation over every obligation, so until each obligation is filed explicitly
the record is not one in that sense. The 135 container filings matter most:
they are where the paper's §8 says stretching would hide, and no audit has
read them against the code. G1 below came from the VERIFY-038 run before the
audit began.

### G1 — vec's out-parameters

**Finding.** `vec_pop`, `vec_remove`, `vec_get` and `vec_iter_next` write an
element through a caller's `out` pointer and require of it only `out == \null
|| \valid(out)`. Nothing keeps `out` out of the vector's own buffer — unlike
`vec_append_array`, which in the same file requires its source to be
`\separated` from the destination. For `pop` and `remove` that makes frame
postconditions false:

- `pop`'s fourth `ensures` (every remaining element unchanged) is false when
  `out` points at a remaining element: `pop(&v, &v.items[0])` on [10, 20, 30]
  leaves [30, 20].
- `remove`'s fourth `ensures` (elements before `i` unchanged) is false when
  `out` points before `i`, and its fifth (elements after `i` shifted left) is
  false when `out` points after `i`: `remove(v, 2, &items[0])` and `remove(v, 1,
  &items[3])` on [10, 20, 30, 40].

Each call is admitted by the contract and is defined C: the probes run clean
under AddressSanitizer and UBSan, in both build configurations
(`tools/probes/verify-039/`).

**What the record said.** VERIFY-018 files these residuals —
`pop_ok_ensures_4_part5` and `remove_ok_ensures_4_part6`, `_4_part7` and
`_5_part6`, pinned in `frama-c-vec` and `cc-vec` and, renamed, in `vec_struct`
— under category (d), "element-transfer ensures", whose stated root cause is
that `mem_copy` and `mem_move` carry frame-only contracts, "the same weak-spec
shape as diag.h's `push_shift_semantics` (VERIFY-017)". `pop` calls neither
function. A frame-only contract yields exactly a frame, so it cannot explain
`remove`'s fourth `ensures` either. For `remove`'s fifth it may be a second,
independent obstacle, but the obligation is false regardless. Four of the
category's six live goals are fragments of these three false postconditions —
which split part carries the aliasing case WP cannot say — and for three of
the four the stated mechanism cannot apply. (The category's heading says five; six are
live: the four above, `insert_ok_ensures_5_part5` and
`append_array_ok_ensures_4_part5`.)

**Why it matters beyond vec.** vec is module 13, the first module of the flat
segment of the paper's Table 1: the argument budget last grew at module 12,
diag.h, and vec's 53 own residuals were filed under existing arguments.
Category (d) filed these by shape, citing diag.h's argument — the last one
written before the curve went flat — rather than by mechanism. They are the
first false obligations, and the first misattribution, found in the flat
segment (VERIFY-025's false toolchain premise concerned flat-segment loop
goals, but those obligations were true). This is the mechanism the paper's §8
calls stretching: an existing argument filed over obligations it does not
cover — here, obligations that are false. It is one instance; the audit will
say how many there are.

**Decision (pending).** Two repairs:

- (a) Require `out` to be separated from the buffer in all four functions —
  `\separated(out, v->items + (0 .. v->capacity - 1))` under the existing null
  guards, and for `iter_next` the iterated vector's buffer. Every postcondition
  keeps its strength; a caller may no longer pass an `out` that points into the
  vector. This matches `append_array`.
- (b) Weaken `pop`'s and `remove`'s frames to the elements `*out` does not
  overlap. Callers stay unrestricted; the guarantees weaken, and `get` and
  `iter_next` would need weaker postconditions too.

Recommended: (a). After the decision, a prediction commit by goal name and
unit precedes the fix, as in VERIFY-036; it covers the four false-obligation
goals, VERIFY-038's two new residuals, and the call-site goals `pop_option`
and `remove_option` gain, since they call `pop` and `remove` with a local.

**Rest of the audit.** File the 343, read the 135 container filings against
the code, and settle the open items — arena Cat 2c (attribution refuted by
VERIFY-036 F4), pool Cat 2d (`pool_reset`), `pool_alloc_zero`'s missing
`ensures`, and the ten blocks never read against their code. Next in line
after G1: the other out-parameter functions in deque and the priority queue,
then the rest of VERIFY-018's categories.

### G1 — committed prediction, option (a) (before the fix commit)

**Decision (2026-10-08).** (a), as recommended above. Every postcondition
keeps its strength; a caller may no longer pass an `out` that points into the
vector's buffer.

**Change.** One `requires`, the last in each contract, added to both contract
surfaces: the macro bodies in `data/vec/vec_impl.h`, which `cc-vec` and
`vec_struct` read under `-CC`, and the prototypes in
`vmacros/vdrivers/vec_verify.h`, which `frama-c-vec` reads under `-C`. In
`get`, `pop` and `remove` it is the third:

```
requires (v != \null && out != \null) ==>
    \separated(out, v->items + (0 .. v->capacity - 1));
```

In `iter_next` it is the fourth, the same over `it->vec` under the guard
`it != \null && it->vec != \null && out != \null`. Being last, it renumbers no
existing clause. Each doc comment gains a `@pre` line. No executable change:
`vec_test` passes in both build configurations. The same commit repairs the
advisory parse VERIFY-038 recorded (`grep -o 'EXPECTED_UNPROVED=[0-9]*'`
instead of every number on the line); no gate reads that step.

No WP run on the changed code precedes this prediction. The only local check
was parsing: Frama-C 25.0, with no prover, accepts all three drivers, and each
normalized AST carries the four new clauses.

**By goal.** Names are short and at `int`. At `Rec` every `vec_int` and
`option_int` in a name becomes `vec_Rec` and `option_Rec`, the callee's
included.

| Goal | `frama-c-vec` | `cc-vec` | `vec_struct` | Conf. |
|------|---------------|----------|--------------|-------|
| `pop_ok_ensures_4_part5` | closes | closes | closes | H at `int`; M at `Rec` |
| `remove_ok_ensures_4_part6`, `remove_ok_ensures_4_part7` | close | close | close | M |
| `remove_ok_ensures_5_part6` | stays | stays | stays | H |
| `get_hit_ensures_2_part4` | not residual | not residual | closes | M |
| `iter_next_yield_ensures_3_part5` | not residual | not residual | closes | M |
| `pop_option_call_vec_int_pop_requires_3` (new) | proved | proved | proved | H generated; H/M proved |
| `remove_option_call_vec_int_remove_requires_3` (new) | proved | proved | proved | H generated; H/M proved |

Why, row by row:

- `pop`'s frame. Apart from `v->len`, the store through `out` is `pop`'s only
  write, and with `out` outside the buffer it reaches no element. At `Rec` the
  proof must also get from separation of the objects to disjointness of their
  fields, a step the `int` units do not need.
- `remove`'s frame below `i`. The store through `out` is excluded the same
  way. The only other write to the buffer, `mem_move`, assigns
  `((char *)dest)[0 .. size - 1]` from `dest = &v->items[i]`, which cannot
  reach below `i`. M, because the frame has to cross that char-typed
  `assigns`, the step the Typed+Cast model handles worst (category (h)).
- `remove`'s shift. It becomes true and stays unprovable: `mem_move`'s contract
  says which bytes it may write, not what it writes, so no contract-level
  argument yields `items[k] == \old(items[k + 1])`. Of the four, this is the
  one goal category (d)'s stated mechanism covers, once it is true.
- `get` and `iter_next` at `Rec`. VERIFY-038's hypothesis, tested here: the
  clause excludes the partial overlap a two-field element lets WP consider. If
  either stays, it is another mechanism and needs an argument of its own.
- The call sites. `pop_option` and `remove_option` pass the address of a
  local, which is separated from any buffer valid at entry; with capacity zero
  the range is empty. A call-site goal that fails to prove is a new residual,
  and moves one goal in the table below from proved to unproved.

**By job.**

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-vec` | 5349 / 5510, 161 | 5354 / 5512, 158 |
| `cc-vec` (`frama-c-cc.yml`) | 5406 / 5575, 169 | 5411 / 5577, 166 |
| `vec_struct` (`frama-c-cc.yml`) | 5404 / 5573, 169 (VERIFY-037's prediction; CC #27 observed 5402 / 5573, 171) | 5409 / 5575, 166 |

- (H) Each unit gains exactly the two call-site goals. Nothing else is
  generated, removed or renumbered: the clause is a hypothesis in the four
  functions' own goals and a new obligation only where they are called, and
  neither call site sits under a branch. A wrong row in the goal table moves
  this table by exactly that goal.
- `vec_struct`'s residual set is `cc-vec`'s predicted set renamed. Against the
  committed pin its gate prints missing 4 (the three closed fragments and the
  mis-transcribed `typed_cast_vec_Rec_alloc_call_vec_int_init_requires_2`)
  and unpinned 1 (that goal's real name,
  `typed_cast_vec_Rec_alloc_call_vec_Rec_init_requires_2`).
- (H) No other goal in the three units changes status or name, including
  `remove`'s six other residuals, category (h)'s `assigns` and `mem_move`
  call goals, which the clause does not touch.
- (H) No other job moves. The macro-body clauses are invisible to every `-C`
  unit, and only `frama-c-vec` reads `vec_verify.h`. The other five `-CC`
  families, every other WP job, MC/DC, MISRA, the builds and the tests stay
  as they are.
- (H) No Failed, Invalid or Stepout goal.
- (H) The advisory step prints one count, `EXPECTED_UNPROVED=161`, for
  `cc-vec` and `vec_struct`, with no warning and no shell error.

**What goes red.** `frama-c-vec` and `cc-vec` turn red, `vec_struct` stays
red, and nothing else is red. The ratchet commit then moves exactly these three
pins, corrects `vec_struct`'s mis-transcribed name, and records the score.

**What it settles.** If the three frame fragments close, the false obligations
leave the record and category (d) keeps three live goals, each a content
postcondition across `mem_copy` or `mem_move`: `remove_ok_ensures_5_part6`,
`insert_ok_ensures_5_part5` and `append_array_ok_ensures_4_part5`. Whether its
mechanism is right for the other two is part of the audit. If `get` and
`iter_next` close as well, VERIFY-038's P1 holds by a contract change, as for
range.h and stringbuf.h, and its P2c miss stands; no argument is added or
retired, and *A* does not move.

**Scoring rule.** By part: (1) the three frame fragments, per unit; (2) the
shift fragment stays; (3) the two `Rec`-only goals; (4) the two call-site
goals; (5) the job set and the counts. VERIFY-038's P5 (no existing job moved
at 98b5283) is still unconfirmed; if CI #1340 is not checked first, this run's
other jobs, green at unchanged pins, settle it.

### G1 — scored (fix 40cccd9; CI #1341, CC #28)

**Result: exact in all three units, every part.**

| Job | Predicted | Observed |
|-----|-----------|----------|
| `frama-c-vec` | 5354 / 5512, 158 | 5354 / 5512, 158 |
| `cc-vec` (`frama-c-cc.yml`) | 5411 / 5577, 166 | 5411 / 5577, 166 |
| `vec_struct` (`frama-c-cc.yml`) | 5409 / 5575, 166 | 5409 / 5575, 166 |

**By part.**

1. The three frame fragments, `pop_ok_ensures_4_part5`, `remove_ok_ensures_4_part6`
   and `remove_ok_ensures_4_part7`, closed in all three units, the
   medium-confidence closures at `Rec` included.
2. `remove_ok_ensures_5_part6` stayed in all three, under the same name.
3. At `Rec`, `get_hit_ensures_2_part4` and `iter_next_yield_ensures_3_part5`
   closed. VERIFY-038's hypothesis held: they failed on the same unconstrained
   `out`.
4. Each unit gained the two call-site goals and proved them. No goal was
   removed or renumbered.
5. `frama-c-vec` and `cc-vec` turned red, `vec_struct` stayed red, and every
   other job of CI #1341 and CC #28 passed. No Failed, Invalid or Stepout goal
   anywhere.

`frama-c-vec` stops at gate 0, so its roll-call was read from the WP log: 154
Timeout and 4 Unknown, exactly its pin less the three names. `vec_struct`'s
gate printed missing 4 and unpinned 1, the names predicted, so its residual
set is `cc-vec`'s renamed. The advisory parse (H) is not scored here: the
pasted output does not include the report step, so it is read from the ratchet
run.

**What it shows.** The three goals VERIFY-018 filed as element-transfer
residuals were false obligations; once the contract was true, they proved with
no argument. They are the first false obligations removed from the flat
segment. Every medium-confidence row held, those at `Rec` included, where the
risk was the step from separation of objects to disjointness of fields.

**VERIFY-038, settled.** P1 holds by a contract change: the two residuals it
found closed with no argument, so vec at a struct type adds no argument block,
as with range.h and stringbuf.h. P2c's miss stands; its list omitted the two
functions. P5 held: CI #1340 concluded success, and in CC #27 only
`vec_struct` failed, as expected (GitHub's run pages, read 2026-10-08).

**Ratchet (this commit).** `frama-c-vec` 5349 / 5510, 161 → 5354 / 5512, 158;
`cc-vec` 5406 / 5575, 169 → 5411 / 5577, 166; `vec_struct` 5404 / 5573, 169 →
5409 / 5575, 166, its roll-call written from the observed set, which corrects
the mis-transcribed name. VERIFY-018 category (d) carries a coverage update.
Expected on the ratchet run: every job green.

### G2 — arena's try wrappers: a write their default `assigns` omits, and an unseparated `out`

**Where it came from.** After G1, the out-parameter functions outside vec,
read against their code:

- deque's `pop_front`, `pop_back`, `peek_front` and `peek_back` separate `out`
  from the deque but not from its buffer, and say nothing about the elements
  that remain, so no clause can be falsified through `out`. The contracts are
  weaker than vec's, not wrong.
- The priority queue's `pq_pop_raw`, `pq_pop` and `pq_peek` already require
  `out` to be separated from the queue's storage.
- `range_peek`, `slice_get`, `pool_try_alloc{,_zero}` and option's and result's
  getters make no claim that a write through `out` could break.
- `arena_try_alloc` and `arena_try_alloc_aligned` do.

**Finding.** Both functions store their allocation into `*out` whenever `out`
is non-null. Their contracts declare `assigns *arena;` for the default
behaviour and list `*out` only in the `non_null_out` behaviour. ACSL's
default-behaviour `assigns` holds on every call, so it is false for the
ordinary call `void* p; arena_try_alloc(&a, 8, &p)`: `p` changes, and it is not
part of `*arena`. `tools/probes/verify-039/p_try_alloc_assigns.c` shows this
for both functions in both build configurations, with the sanitizers clean.
The pinned goals are `arena_try_alloc_assigns_normal_part03` and
`arena_try_alloc_aligned_assigns_normal_part03`; the store through `out` is the
only write that `*arena` does not cover.

The other four, `non_null_out_ensures_part1` and `_part2` of each function, are
`arena_invariant(arena)` after that store, the shape VERIFY-033 noted. Nothing
separates `out` from `*arena`, so WP has to consider a store into
`arena->buffer`, which would break the invariant's validity conjunct. C does
not allow that store: `out` is a `void **`, and a `void *` lvalue may not access
`Arena`'s `u8 *` and `usize` fields (C11 6.5p7). These four are true, and
unprovable as stated.

**What the record said.** VERIFY-009 Category 2c files all eight of its goals
as inheritance from Category 2b, an attribution VERIFY-036 F4 refuted by
measurement. Read against the code, two are false (the default `assigns`),
four are true but unprovable without a separation precondition, and the last
two, `arena_alloc_zero_ensures_3_part1` and
`arena_alloc_aligned_zero_ensures_3_part1`, fail because `mem_zero`'s contract
says which bytes it may write but not that it writes zeros: the frame-only
shape of vec's category (d). The block's stated mechanism covers none of the
eight.

**Where they are pinned.** The six try-wrapper goals are pinned in eight units:
`frama-c-arena`, `frama-c-arena-32`, `frama-c-pool`, `frama-c-region`,
`frama-c-vec`, `frama-c-stringbuf`, `cc-vec` and `vec_struct`. No verified
code calls either wrapper.

**Repair (recommended; the shape of F5 and G1).** In both contracts, add
`*out` to the default `assigns`, and add `requires out == \null ||
\separated(out, arena);`. The first makes the clause true; the second excludes
only calls that C already leaves undefined. The prediction, by goal and unit,
precedes the fix and follows the G1 ratchet run, because `frama-c-vec`,
`cc-vec` and `vec_struct` pin these goals too.

### G2 — committed prediction (before the fix commit)

**Decision (2026-10-08).** The recommended repair, in both contracts: the
default behaviour's `assigns *arena;` becomes `assigns *arena, *out;`, and a
new last precondition `requires out == \null || \separated(out, arena);` is
added. No verified code calls either wrapper, so the precondition adds no
goal. No executable change.

**Recorded from the G1 ratchet run.** CI #1342 and CC #29 (598540e) were
green in every job, as expected. The advisory parse held there: the report
step printed `driver pin (frama-c-vec): 158 names, EXPECTED_UNPROVED=158`,
with no warning and no shell error.

**By goal.** The same in all eight units.

| Goal | Prediction | Conf. |
|------|------------|-------|
| `arena_try_alloc_assigns_normal_part03`, `arena_try_alloc_aligned_assigns_normal_part03` | close: the store through `out` is the only write the default frame missed | H |
| `arena_try_alloc_non_null_out_ensures_part1` and `_part2`, and the same two of `arena_try_alloc_aligned` | close: with `out` separated from `*arena`, the store cannot reach the fields `arena_invariant` reads | M |
| `arena_alloc_zero_ensures_3_part1`, `arena_alloc_aligned_zero_ensures_3_part1` | stay: `mem_zero`'s contract is unchanged | H |
| every other goal | unchanged in status and name; none generated or removed | H |

**By job.** In each unit: proved +6, total unchanged, unproved −6.

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-arena` | 3508 / 3564, 56 | 3514 / 3564, 50 |
| `frama-c-arena-32` | 3508 / 3564, 56 | 3514 / 3564, 50; 64/32 set equality holds |
| `frama-c-pool` | 3977 / 4042, 65 | 3983 / 4042, 59 |
| `frama-c-region` | 3656 / 3735, 79 | 3662 / 3735, 73 |
| `frama-c-vec` | 5354 / 5512, 158 | 5360 / 5512, 152 |
| `frama-c-stringbuf` | 4779 / 4837, 58 | 4785 / 4837, 52 |
| `cc-vec` (`frama-c-cc.yml`) | 5411 / 5577, 166 | 5417 / 5577, 160 |
| `vec_struct` (`frama-c-cc.yml`) | 5409 / 5575, 166 | 5415 / 5575, 160 |

- (H) No other job moves. These eight are exactly the units whose include
  closure contains `core/arena.h`.
- (H) No Failed, Invalid or Stepout goal.

**How it is scored.** The fix commit carries these pins, as VERIFY-037 carried
`vec_struct`'s: each of the eight units' proved line, count and roll-call (the
current roll-call less the six names) is set to the table above in the fix
commit itself. A green run therefore means every gated quantity matched in all
eight units and nothing else moved. A red job is a miss, read from its log,
and a ratchet follows. VERIFY-036 left pins to a ratchet after the fix; with
eight units, predicted pins let the gates score by name.

**What it settles.** If the six close, two false obligations leave the record
and four true ones prove. VERIFY-009 Cat 2c then covers two goals, the
zero-content postconditions, and its stated mechanism, inheritance from Cat
2b, is wrong for both: they wait on `mem_zero`'s contract. No block retires,
and *A* stays at 12.

### G2 — scored (fix f4a846a; CI #1343, CC #30)

**Result: exact in all eight units.** The fix commit carried the predicted
pins (prediction 5178a91), and both runs were green in every job: in each of
the eight units the proved line, the count and the roll-call matched the
prediction, the four medium-confidence closures included, and no other job
moved.

| Job | Predicted and observed |
|-----|------------------------|
| `frama-c-arena` | 3514 / 3564, 50 |
| `frama-c-arena-32` | 3514 / 3564, 50; 64/32 set equality held |
| `frama-c-pool` | 3983 / 4042, 59 |
| `frama-c-region` | 3662 / 3735, 73 |
| `frama-c-vec` | 5360 / 5512, 152 |
| `frama-c-stringbuf` | 4785 / 4837, 52 |
| `cc-vec` (`frama-c-cc.yml`) | 5417 / 5577, 160 |
| `vec_struct` (`frama-c-cc.yml`) | 5415 / 5575, 160 |

**By part.** Both default-`assigns` goals closed: the store through `out` was
the only write the frame missed. The four `arena_invariant` parts closed:
separation was the only missing step. The two zero-content goals stayed. No
Failed, Invalid or Stepout goal anywhere. The gates score by name, so no log
had to be read: a green run is set equality in every unit.

**What it shows.** All six try-wrapper goals that Cat 2c filed as inheritance
from Cat 2b are resolved, two of them false obligations made true and four
true ones proved under a precondition C already implies. Cat 2c's stated
mechanism covered none of them.

**Cat 2c now.** Two goals, `arena_alloc_zero_ensures_3_part1` and
`arena_alloc_aligned_zero_ensures_3_part1`. Both are true: `mem_zero` calls
`memset` with zero over exactly the returned bytes. Both are unprovable for a
reason the block does not state: `mem_zero`'s contract says which bytes it may
write, not what it writes. The block's manual argument cites "mem_zero's
verified postcondition", and `mem_zero` has none, so as written it does not
establish either goal. Which argument they are filed under, this block
rewritten or the frame-only-callee shape VERIFY-017 and VERIFY-018 already
use, is settled in the filing pass, by one rule for every obligation. *A*
stays at 12 until then.

### G3 — `mem_zero` states that it writes zeros (committed prediction)

**Why.** VERIFY-009 Cat 2c's last two goals, `arena_alloc_zero_ensures_3_part1`
and `arena_alloc_aligned_zero_ensures_3_part1`, are true and unprovable
because `mem_zero`'s contract says which bytes it may write but not their
values, and the block's argument cites a `mem_zero` postcondition that does
not exist (G2, scored). G3 tests that reading by intervention: it gives
`mem_zero` the postcondition the argument assumed. If the two goals then
prove, the frame-only contract was the whole obstacle.

**Change.** `core/memory.h`, `mem_zero`'s `zero` behaviour gains
`ensures zeroed: \forall integer i; 0 <= i < size ==> ((char *)ptr)[i] == 0;`
and its doc comment a `@post` line. No executable change. The rest of
memory.h's frame-only family (`mem_zero_bytes`, `mem_copy`, `mem_move`,
`mem_set`) is left as it is: one intervention per prediction.

**By goal.**

| Goal | Prediction | Conf. |
|------|------------|-------|
| `mem_zero_zero_ensures_zeroed`, in every unit whose closure includes `core/memory.h` | generated as three parts, one per path to the return (`!ptr`, `size == 0`, the `memset` call): VERIFY-034's rule. Parts 1 and 2 contradict the behaviour's `assumes` and prove in Qed; part 3 proves from `memset`'s `acsl_c_equiv` postcondition and the libc axiom `memset_def` | H generated; M/H proved |
| `arena_alloc_zero_ensures_3_part1`, `arena_alloc_aligned_zero_ensures_3_part1` | close, on the reading that WP's typed model keeps both 8-bit types in one memory chunk: the goal reads the bytes through `u8 *`, the postcondition states them through `char *` | M |
| the bitset residuals about the zeroed words (`bitset_init_ensures_4`, `_6`; `bitset_clear_all_live_ensures_part3`, `_2_part3`) | stay: on the same reading, `u64` words live in a different chunk from `char`, so a byte-level fact does not reach them | H |
| every other goal | unchanged in status and name; none generated or removed elsewhere | H |

**By job.** Every unit gains three goals. The three without `arena.h` prove
them and nothing else moves; the eight with it also close the two Cat 2c
goals.

| Job | Pinned now | Predicted |
|-----|------------|-----------|
| `frama-c-memory` | 2839 / 2887, 48 | 2842 / 2890, 48 |
| `frama-c-bitset` | 4855 / 5023, 168 | 4858 / 5026, 168 |
| `frama-c-priority-queue` | 4523 / 4599, 76 | 4526 / 4602, 76 |
| `frama-c-arena` | 3514 / 3564, 50 | 3519 / 3567, 48 |
| `frama-c-arena-32` | 3514 / 3564, 50 | 3519 / 3567, 48; 64/32 set equality holds |
| `frama-c-pool` | 3983 / 4042, 59 | 3988 / 4045, 57 |
| `frama-c-region` | 3662 / 3735, 73 | 3667 / 3738, 71 |
| `frama-c-vec` | 5360 / 5512, 152 | 5365 / 5515, 150 |
| `frama-c-stringbuf` | 4785 / 4837, 52 | 4790 / 4840, 50 |
| `cc-vec` (`frama-c-cc.yml`) | 5417 / 5577, 160 | 5422 / 5580, 158 |
| `vec_struct` (`frama-c-cc.yml`) | 5415 / 5575, 160 | 5420 / 5578, 158 |

- (H) No other job moves. These eleven are exactly the units whose include
  closure contains `core/memory.h`.
- (H) No Failed, Invalid or Stepout goal.

**How it is scored.** As G2: the fix commit carries these pins, so a green run
is set equality in all eleven units. A red run reads directly. If the two Cat
2c goals stay, the eight arena units show two fewer proved and those two names
unpinned, and the other three stay green. If part 3 of the new postcondition
does not prove, all eleven show `typed_cast_mem_zero_zero_ensures_zeroed_part3`
unpinned. If the split is not three parts, every unit's total is off by the
same amount.

**What it settles.** If the two close, Cat 2c covers nothing and retires: *A*
in force 12 → 11. It would be the second block to retire by its goals proving,
after Cat 2b at F4, and by the same kind of change: a callee stating what it
does. It also tests by intervention the frame-only reading that VERIFY-018's
category (d) applies to `mem_copy` and `mem_move`. If they stay, the obstacle is the byte-type bridge,
and they are filed under that mechanism.

### G3 — scored (fix 76a1f94; CI #1344, CC #31)

**Result: the new postcondition held in all eleven units; the two Cat 2c
closures missed in all eight arena units.** The fix commit carried the
predicted pins (prediction bcc7343). `frama-c-memory`, `frama-c-bitset` and
`frama-c-priority-queue` were green. The eight arena units were red, each
with two fewer proved than predicted and the same total; wherever the
residual set can be read, it is the predicted set plus the two zero-content
goals.

| Job | Predicted | Observed |
|-----|-----------|----------|
| `frama-c-memory` | 2842 / 2890, 48 | as predicted |
| `frama-c-bitset` | 4858 / 5026, 168 | as predicted |
| `frama-c-priority-queue` | 4526 / 4602, 76 | as predicted |
| `frama-c-arena` | 3519 / 3567, 48 | 3517 / 3567, 50 |
| `frama-c-arena-32` | 3519 / 3567, 48; 64/32 set equality holds | 3517 / 3567, 50; the 32-bit set equals the observed 64-bit set |
| `frama-c-pool` | 3988 / 4045, 57 | 3986 / 4045, 59 |
| `frama-c-region` | 3667 / 3738, 71 | 3665 / 3738, 73 |
| `frama-c-vec` | 5365 / 5515, 150 | 5363 / 5515, 152 |
| `frama-c-stringbuf` | 4790 / 4840, 50 | 4788 / 4840, 52 |
| `cc-vec` (`frama-c-cc.yml`) | 5422 / 5580, 158 | 5420 / 5580, 160 |
| `vec_struct` (`frama-c-cc.yml`) | 5420 / 5578, 158 | 5418 / 5578, 160 |

**By part.**

1. `mem_zero_zero_ensures_zeroed` (H generated; M/H proved): held. Every
   unit's total rose by exactly three, and no `mem_zero` goal is among the
   residuals any gate or log shows; in `frama-c-vec`, whose pasted log is
   partial, the counts (+3 proved, +3 total) are those of three proved goals.
   The logs give prover totals, not provers by goal, so which prover closed
   which part is not scored.
2. `arena_alloc_zero_ensures_3_part1` and
   `arena_alloc_aligned_zero_ensures_3_part1` (M): missed. Both stayed, under
   the same names, in all eight units.
3. The bitset residuals about the zeroed words (H): held. `frama-c-bitset`
   was green, and its gate checks all 168 names in both directions.
4. Every other goal (H): held, as far as the sets can be read. In seven of
   the eight arena units the observed residual set is the G2 set exactly:
   read from the logs for arena, arena-32, pool, region and stringbuf, and
   from the gates for `cc-vec` and `vec_struct` (missing 0, unpinned 2).
   arena-32's gate reported its set "no longer identical to the 64-bit set
   (0 newly proved, 2 width-specific)" because it compares with the baseline
   the fix commit embedded, which lacked the two names; the observed 64-bit
   set is identical to it. `frama-c-vec` stops at gate 0, and its pasted log
   lists 102 of its 152 names, all in the G2 set, the two among them; the
   ratchet run's roll-call checks the rest. memory and the priority queue
   were green, at set equality.
5. The job set (H): held. CI #1344 failed in exactly the six main-workflow
   arena jobs and CC #31 in exactly `cc-vec` and `vec_struct`; every other
   job passed (GitHub's run pages, read 2026-10-09). No Failed, Invalid or
   Stepout goal: in every unit the proved count plus the Timeouts and
   Unknowns equals the total. (The gates' Failed checks grep `[Failed]`,
   which WP 29 prints only in smoke-test mode; it prints a prover failure as
   `[Failure]` and a goal with no result not at all. In these eleven units
   the pinned proved line and count add up to the total, so a green gate
   cannot hide either; the regexes are a separate fix.)

The red jobs are the ones the prediction named for this case: "If the two
Cat 2c goals stay, the eight arena units show two fewer proved and those two
names unpinned, and the other three stay green."

**Why the closures failed.** The prediction rested on the reading that WP's
typed model keeps both 8-bit types in one memory chunk. It does not. The model
stores heap values in "several separated global arrays, one for each atomic
type" (WP manual 29.0, §1.4), and in WP's source the integer arrays are split
by exact kind (`M_int of Ctypes.c_int`). WP takes `void *` as `char *`
(VERIFY-005), and plain `char` is signed under both machdeps, so `mem_zero`'s
new `zeroed`, like the libc `memset` contract it is proved from, speaks of the
`sint8` array. The goal, `((u8*)\result)[i] == 0`, reads the `uint8` array,
of which `zeroed` says nothing. The call's frame does not help either:
`mem_zero` has no default `assigns`, so WP 29 frames the call by the union of
its complete behaviours' clauses, `((char *)ptr)[0 .. size - 1]` (the log
warns "No default assigns clause, using complete behaviors assigns" at the
first such call, `core/arena.h:458`), and havocs only the `sint8` array and
the initialization map. In the model the `uint8` bytes keep whatever values
they had, and no postcondition stated through `char *` reaches the goal. The
run's log shows the type split at the goals themselves: at both goal lines (`core/arena.h:452` and `:469`) WP warns
`Cast with incompatible pointers types (source: sint8*) (target: uint8*)`,
and both goals carry `(Stronger, 3 warnings)`. The same warnings are in CI
#1341's log, read for G1. The evidence was in hand before the prediction, and
the prediction did not consult it.

It is not the call's behaviour selection: the call is under `p != NULL`, and
with `size == 0` the goal is vacuous. (WP's source was read in Frama-C 25.0,
whose source was at hand, and in 29.0 on ocaml.org: 29.0's `MemTyped` declares
the same per-kind arrays. The call frame is 29's: 25.0 takes a missing default
`assigns` as writing everything.)

**Cat 2c: the mechanism, as pre-registered, is the byte-type bridge.** Both
goals are true in C: `memset` writes zero bytes, and a zero byte reads 0
through `char` and `unsigned char` alike. Both are unprovable in this model
as stated, and a `mem_zero` contract can only move the gap: stated through
`u8 *`, its postcondition would let the two goals prove and leave the same
gap in `mem_zero`'s own proof, which starts from `memset`'s `char`-typed
postcondition. This is the libc byte-view class (VERIFY-007, VERIFY-008;
diag.h's Category 1; vec's category (h)), here between two 8-bit kinds. Cat
2c's argument now has the premise it cites, and covers both goals once it
states the bridging step and drops the inheritance from Cat 2b it still
claims. Which block's argument carries them, Cat 2c rewritten that way or an
existing byte-view block, is the filing pass's decision; until then Cat 2c
lists them and its argument as written does not cover them. *A* stays at 12.
`mem_zero` keeps its postcondition: it is true, it proves, and it states what
the function does. One intervention would test the reading directly: state
the two postconditions through `char *`, which in C says the same of a zero
byte; the mechanism predicts both close. The filing pass weighs it, since it
fits a specification to the model's arrays.

**What it shows.**

- **Content across byte-level callees.** A frame-only callee contract was not
  the whole obstacle: stating the content through `char *` did not suffice.
  VERIFY-018's category (d) is in the same position by the same mechanism.
  Its three live goals read `int` cells (`Rec` fields at `vec_struct`) after
  `mem_copy` or `mem_move`, whose writes are stated through `char *`; content
  postconditions on those two would not reach them either. This is deduced
  from the mechanism, not tested.
- **Frames across byte-level callees (from WP 29's handling of the call; not
  yet tested).** The same separation works the other way. A call framed by
  `char`-typed `assigns` havocs only the `sint8` array and the
  initialization map, so every location outside them keeps its value across
  the call, wherever the call writes, and a frame postcondition across such a
  call should prove without the call's range being compared with it. G1's frame in `remove`, the
  elements below `i`, would be one: it holds in C because `mem_move` writes
  from `&items[i]` on, and G1's prediction expected the proof to cross the
  `char`-typed `assigns`; by the source, in the model there is nothing to
  cross. If so, such a proof is evidence only under the model's hypothesis
  that each location is accessed at one type, which these callees break by
  design: they write typed buffers through `char *`. The project runs WP's
  `cast` selector (`Typed+Cast`), which the WP manual lists as "Typed memory
  model with unlimited casts (unsound)" (29.0, §2.4.3); VERIFY-005 calls the
  model sound "under the assumption that callers pass correctly-typed
  pointers", a narrower hypothesis than such proofs use. The record already
  cites one as evidence of a range: VERIFY-017 and MCDC-009 read diag.h's
  `d->depth` surviving `push`'s `memmove` as WP framing the byte-level write
  correctly by its footprint. G4 below opens the item.

**Ratchet (this commit).** The eight arena units return to their G2
roll-calls and counts, with the three new goals in their proved lines:
`frama-c-arena` and `frama-c-arena-32` 3517 / 3567, 50; `frama-c-pool`
3986 / 4045, 59; `frama-c-region` 3665 / 3738, 73; `frama-c-vec` 5363 / 5515,
152; `frama-c-stringbuf` 4788 / 4840, 52; `cc-vec` 5420 / 5580, 160;
`vec_struct` 5418 / 5578, 160. memory, bitset and the priority queue keep the
pins they passed at. VERIFY-009 Cat 2c and VERIFY-018 category (d) carry
coverage updates, and VERIFY-005 an update on its hypothesis. Expected on the
ratchet run: every job green.

### G4 — frames across byte-level callees (opened)

**Finding (from WP 29's handling of these calls; not yet tested by probe).**
Twelve memory.h functions state their writes through `char *` — `mem_copy`,
`mem_move`, `mem_zero`, `mem_set`, `mem_secure_zero`, `mem_swap`,
`mem_swap_buf` and the five `_bytes` variants — inside behaviours, with no
default `assigns`; WP 29 frames a call to one by the union of its complete
behaviours' clauses, and warns "No default assigns clause, using complete
behaviors assigns". libc's `memset`, `memcpy` and `memmove`, which diag.h
calls directly, state theirs at top level. Either way a call havocs only the
`sint8` array and the initialization map, so every location of another type
keeps its value across it, whatever the call's range. A postcondition or
assertion that such a location is unchanged across the call should therefore
prove without the range being compared with it; G1's frame in `remove`
proved at CI #1341, as this predicts. Where the
code's ranges avoid the location, as in `insert`'s and `remove`'s frames
below `i` and diag.h's `d->depth` across `push`'s `memmove`, the property is
true and its proof is not the evidence of it. Were a range to overlap, the
property would be false and would still prove.

**Why it matters.** The record treats a proved goal as needing no argument.
These would need one: the model's hypothesis, and the code's ranges, which no
prover has checked. In vec the ranges are bounded by the `ok` behaviours'
`assigns` clauses, whose goals are category (h) residuals; whether every such
frame has a bound of that kind is what the listing must establish. The record
already reads one such proof as evidence of the range: VERIFY-017 ("WP frames
`d->depth` correctly past the byte-level write") and MCDC-009 ("the memmove's
assigns footprint covers `frames` bytes only"), whose cross-stream instance
rests on that reading.

**Plan.** First a probe, its outcome committed before it runs: a function
that zeroes an `int` through `mem_zero` and claims the `int` unchanged, false
in C and predicted to prove, with controls at `u8` and at `char`. Then the
listing: every proved goal that relates a location of another type across one
of these calls, VERIFY-017's and MCDC-009's included, each read against the
call's range and filed under one stated hypothesis. Until then such proofs
stand relative to the typed model's hypothesis, and VERIFY-005 says so.

### G4 — committed prediction for the probe (before the probe commit)

**Probe.** `tools/probes/verify-039/p_frame_typed.c`, in the next commit.
Three functions each zero one object through `mem_zero` — an `int`, a `u8`
and a `char` — and state two named postconditions of it: `g4_<type>_frame`,
that it is unchanged, and `g4_<type>_zero`, that it is zero. In C every
`_frame` is false whenever the object was non-zero and every `_zero` is true;
the file's `main`, outside the analysed code, checks that at run time. The
`int` pair is the question; the `u8` pair sets G3's two goals beside their
frame, in isolation; the `char` pair is the control, where the write and the
claims share the `sint8` array. `.github/workflows/g4-probe.yml`, in the same
commit, runs the C check and then WP under the units' options (`Typed+Cast`,
`-wp-rte`, `-wp-split`, Alt-Ergo, Z3 and CVC5 at 120 s, frama-c-memory's
`-cpp-extra-args`), restricted to the six postconditions with `-wp-prop`. It
runs when the probe or the workflow changes, and on demand.

No WP run on the probe precedes this prediction. The only local checks were
the C run, in both build configurations and with the sanitizers clean, and a
Frama-C 25.0 parse, which accepts the six clauses and leaves `main` out.

**By goal.** Each name is prefixed `typed_cast_g4_zero_<type>_ensures_`.

| Goal | In C | Predicted | Conf. |
|------|------|-----------|-------|
| `g4_int_frame` | false | proves: WP 29 frames the call by `mem_zero`'s complete behaviours' `assigns`, which write only the `sint8` array, and `*a` lives in the `sint32` array | H |
| `g4_int_zero` | true | does not prove: `zeroed` speaks of the `sint8` array | H |
| `g4_u8_frame` | false | proves, as at `int` | H |
| `g4_u8_zero` | true | does not prove: G3's two goals, in isolation | H |
| `g4_char_frame` | false | does not prove: the write and the claim share the `sint8` array | H |
| `g4_char_zero` | true | proves from `zeroed` | H |

The `_zero` claims test G3's account, one array per integer kind. The
`_frame` claims test the call's frame: WP 29's fallback to the union of the
complete behaviours' `assigns` when a contract has no default clause, which
G1's `remove` frame, proved at CI #1341, already suggests. Only that path is
probed; libc's three functions state their `assigns` at top level, a second
path to the same frame.

- (M) The proved line is `3 / 6`: `-wp-prop` selects the six and nothing
  else, and each lies on one path, so none splits.
- (H) The C step prints every `_frame` VIOLATED and every `_zero` holding,
  and exits 0 with the sanitizers clean.
- (H) No Failed, Invalid or Stepout goal; the workflow also lists
  `[Failure]` and `[NoResult]`, the tags WP 29 prints for a prover failure
  and a goal with no result, and checks that the listed goals account for
  the summary.
- (H) No enforced job moves: the probe adds no header, driver, test or pin.
  The workflow file is outside the main CI's `paths-ignore`, so the push runs
  the full matrix, at the G3 ratchet's pins.

**How it is scored.** The workflow prints MATCH or MISS for the not-proved
set, by name, for the proved line, and for the accounting, and exits 0 only
if all three match. A MISS is read from the log, part by part.

**What it settles.** The probe separates the two steps. If `g4_int_zero` and
`g4_u8_zero` stay unproved while `g4_char_zero` proves, G3's account holds in
isolation. If `g4_int_frame` and `g4_u8_frame` prove, WP proves
postconditions that are false in C across a write stated through `char *`:
G4's reading holds, every proved goal that relates a location of another type
across one of the fifteen functions is evidence only under the typed model's
hypothesis, and the listing proceeds, each such goal read against the call's
range. If a `_frame` claim does not prove, WP frames the call more widely
than its source reads, such frames are checked against the write after all,
and VERIFY-005's update loses its frame clause. If the `char` pair does not
behave as predicted, the probe itself is in question, and nothing is
concluded from the other two.

### G3 — ratchet confirmed (CI #1345, CC #32)

Both runs at 1e6434c — the ratchet, with G4's prediction and probe — were
green in every job (GitHub's run pages, read 2026-10-09; one warning, a
Node.js deprecation notice on `compcert`). `frama-c-vec`'s gate checked its
152 names by set equality, which settles what G3's part 4 left to this run:
its residual set is the G2 set. G3 is closed.

### G4 — probe scored (`g4-probe.yml` run #1, 1e6434c)

**Result: both false frames proved, and the not-proved set is exactly the
predicted one; the proved line missed by one goal outside the six.**

| Goal | In C | Predicted | Observed |
|------|------|-----------|----------|
| `g4_int_frame` | false | proves | proved |
| `g4_int_zero` | true | does not prove | Timeout (Z3), `Stronger` |
| `g4_u8_frame` | false | proves | proved |
| `g4_u8_zero` | true | does not prove | Timeout (Alt-Ergo), `Stronger` |
| `g4_char_frame` | false | does not prove | Timeout (Alt-Ergo) |
| `g4_char_zero` | true | proves | proved |

WP scheduled exactly the six goals. Three timed out, the predicted three, and
three proved: two by Qed alone and one by Alt-Ergo, which fits the two frames
(nothing is left to prove once the call leaves their arrays alone) and
`g4_char_zero` (from `zeroed`). The log shows the step the frames rest on:
"No default assigns clause, using complete behaviors assigns" at the call to
`mem_zero`, line 53, and the casts from `sint32*` and `uint8*` to `sint8*` at
the calls, with no cast at the `char` call.

**By part.**

1. The `_frame` claims at `int` and `u8` (H): held. WP proved two
   postconditions that are false in C.
2. The `_zero` claims at `int` and `u8` (H): held. G3's account, in
   isolation.
3. The `char` control (H): held. The model sees a write in the same array:
   the false frame stayed unproved, the true zero proved.
4. The C step (H): held. It passed — the run's only error annotation is on
   the WP step — so every `_frame` was VIOLATED and every `_zero` held at run
   time, with the sanitizers clean.
5. No Failed, Invalid or Stepout goal (H): held, and the accounting matched:
   3 listed = 7 − 4.
6. The proved line (M): missed. Observed `4 / 7`, predicted `3 / 6`. The
   seventh goal is none of the six: WP counted one goal `Unreachable`, closed
   by its reachability pass, which marks the annotations of statements it
   finds dead as proved without consulting `-wp-prop` (WP 29, `cfgInfos.ml`).
   The log does not say which annotation. The prediction's reason, that
   `-wp-prop` selects the six and nothing else, was wrong for that pass.
7. No enforced job moved (H): held. CI #1345 and CC #32 were green.

The gate exited 1 on part 6 alone. Its frame lines printed "not all accounted
for" rather than PROVED, as designed when the total is not 6; the reading
above rests on the six scheduled goals and the prover counts.

**What it shows.** Under the options every unit uses, WP proves
postconditions that are false in C across a write stated through `char *`.
G4's reading holds, and the listing proceeds.

**Probe ratchet (this commit).** The workflow now expects `4 / 7`, six goals
scheduled, and the same not-proved set, and it also runs when
`core/memory.h` or the toolchain pins in `.github/actions/framac-setup/`
change. It stays as a standing check of this WP behaviour: if a later WP
stops proving the false frames, or frames the call more widely, the run
turns red.

### G4 — scope, corrected

G4 opened on fifteen functions; the gap is wider. WP's typed model keeps one
array per integer kind, so in the model a write through one type is never
seen through another. C sees it in two cases (C11 6.5p6–7). An object may be
read or written through a character type whatever its own type; and through
the signed or unsigned counterpart of its type. Elsewhere, reading an object
through a type other than its own (or a qualified or compatible one, or an
aggregate or union containing it) is undefined; in allocated storage a store
through any type is defined and gives the bytes that type, so a later read
through the old type is the undefined step. The character types fill two of
the model's arrays: `char` and `signed char` share `sint8` on the project's
machdeps, and `unsigned char` (`u8`) has `uint8`. Unions are the other known
gap (VERIFY-015's hypothesis).

The listing therefore covers three kinds of pair: a character-type write —
plain stores into `char` and `u8` buffers included, not only the fifteen
functions — and a postcondition that reads another type; a write through any
type and a postcondition that reads the bytes through a character type; and
signed and unsigned counterparts.

### G4 — listing, entry 1: an `Arena` inside its own buffer

**Finding.** `arena_invariant` does not separate `*arena` from
`arena->buffer`, and no arena contract requires it, so an `Arena` that lives
inside the block it manages is admitted, from `arena_init` on. Three arena
functions write that block through `char *`: `arena_alloc_zero` and
`arena_alloc_aligned_zero` zero the new allocation with `mem_zero`, and
`arena_reset_secure` zeroes the used bytes with `mem_secure_zero`. Two
clients do the same to blocks the arena handed out: `pool_reset_secure`
zeroes the pool's slots, and `stringbuf_init_arena` stores `'\0'` into its
new block, a plain `char` store. When the written range covers the `Arena`,
nine postconditions that WP proves are false:

| Function | Postconditions false when the write covers `*arena` | Units |
|---|---|---|
| `arena_alloc_zero` | `arena_invariant(arena)`; the address `ensures` | the eight that include `arena.h` |
| `arena_alloc_aligned_zero` | `arena_invariant(arena)`; the address `ensures` | the same eight |
| `arena_reset_secure` | `buffer == \old(buffer)`; `capacity == \old(capacity)`; `arena_invariant(arena)` | the same eight |
| `pool_reset_secure` | `pool_invariant(pool)`, through its `arena_invariant(p->arena)` | `frama-c-pool` |
| `stringbuf_init_arena` | `arena_kept: arena_invariant(arena)` | `frama-c-stringbuf` |

No part of any of them is among the residuals: WP proves each across the
character-type write, as G4's probe does. Two probes make the calls with the
`Arena` placed so that the written range covers it; every call meets its
function's contract as it stands. `tools/probes/verify-039/p_arena_self.c`
shows the arena functions' seven and `stringbuf`'s one VIOLATED, in both
build configurations and at ILP32 as at LP64; its placements are computed
from `offsetof` and `CANON_DEFAULT_ALIGN`. In the `stringbuf` case the store
lands on the first byte of `capacity`, which on the project's little-endian
targets is its low byte: 255 becomes 0. `tools/probes/verify-039/p_pool_self.c`,
case A, shows `pool_reset_secure`'s in the proof configuration; in the default
build `pool_reset`'s `ensure_msg` stops the program before the function
returns. The blocks are heap storage, and the writes that clobber the `Arena`
are character-type writes, so every access is defined C. The sanitizers are
clean throughout.

These are the first proved postconditions of the library that the audit
finds false. In G1 a missing separation left false postconditions unproved,
because the store and the element it clobbered shared a memory array in the
model. Here they do not, and the model proves the falsehood.

**Decision (recommended; the shape of G1 and G2).** Make the separation part
of the invariant: `arena_invariant` gains
`\separated(a, a->buffer + (0 .. a->capacity - 1))`, and `arena_init` the
matching precondition. Every arena function then excludes the self-hosted
arena, and every block the arena hands out is separated from the `Arena`,
which a repair at the three arena functions alone would not give `pool` or
`stringbuf`. No code in the repository places an `Arena` in its own buffer.
WP cannot see the difference: the nine postconditions prove with or without
it. What makes them true is an argument — every write lies in the buffer,
which the invariant separates from `*arena` — so they join the goals G4's
listing collects: proved, with an argument as their evidence. How those
enter the record, and whether as a block that counts in *A*, is settled with
the listing.

### G4 — listing, entry 2: a `Pool` inside its own slots

**Finding.** `pool_init` requires `\valid(pool)` and `arena_invariant(arena)`
but does not separate `*pool` from the arena's free tail, where its slots
will come from, and `pool_invariant` does not separate `*pool` from the
slots. A `Pool` that lives inside its own slot region is therefore admitted.
`pool_reset_secure` zeroes the used slots through `char *`, and its proved
`ensures pool_invariant(pool)` (`frama-c-pool`) is false when they cover the
`Pool`: `p_pool_self.c`, case B, puts the `Pool` at the start of the arena's
buffer, so slot 0 is the `Pool` itself, with the `Arena` outside its buffer
as entry 1's repair requires. After `pool_alloc` and `pool_reset_secure` the
`Pool`'s fields are zero, and `pool_invariant` is VIOLATED, in both build
configurations, sanitizers clean. Entry 1's repair does not reach it.
`stringbuf_init_arena` already carries the separation this lacks
(`sep_storage`), and `stringbuf_wf` the invariant's half.

**Repair (recommended, next).** `pool_invariant` gains the separation of
`*p` from its slots, and `pool_init` a precondition separating `*pool` from
the arena's free tail, as `stringbuf_init_arena` has. One intervention per
prediction: it follows entry 1's scoring.

### G4 — listing entry 1, committed prediction for the repair (before the fix commit)

**Decision (2026-10-09).** The recommended repair, in `core/arena.h`.
`arena_invariant` gains the conjunct
`\separated(a, a->buffer + (0 .. a->capacity - 1))`. `arena_init` gains a
last precondition, so no clause is renumbered:
`requires \separated(arena, (u8*)buffer + (0 .. capacity - 1));`. The
file's list of properties says it for callers. No executable change.

No WP run on the changed code precedes this prediction. The only local checks
were the C probes and a Frama-C 25.0 parse of the changed code as each of the
eight units reads it (`arena.h` also at `x86_32`): all accept both clauses,
and each normalized AST carries them.

**Predicted: nothing moves, in any unit.**

- (H) No goal is generated or removed. `arena_init` has no verified caller,
  so the new precondition is a goal nowhere, and a conjunct inside a
  predicate does not split: WP 29 splits by paths and `||` guards
  (VERIFY-034).
- (H) The nine postconditions of entry 1 stay proved. WP proves them across
  the character-type writes whether or not the `Arena` is separated from its
  buffer; that is G4's finding.
- (M) No other goal changes status. Wherever `arena_invariant` is
  established, `buffer` and `capacity` are either left as they were — the
  arena functions write `offset` and `padding_accum`, pool, region,
  `stringbuf` and vec their own state — or set from `arena_init`'s arguments,
  so the new conjunct follows from the old invariant or from the new
  precondition. Where it is assumed it is one more hypothesis, and no
  residual is known to wait on it.
- (H) Every pin stays as it is. The fix commit changes no pin, so a green run
  is set equality in every unit; a red job is a miss, read from its log.
- (H) The G4 probe is green with its count pinned at `4 / 7`: the push that
  carries this commit carries the probe ratchet too, which re-runs it, and
  nothing it reads changes.
- (H) The C probes behave at run time as before. Since the fix,
  `p_arena_self.c`'s calls and `p_pool_self.c`'s case A are outside
  `arena_init`'s contract; case B still meets every contract, because entry 2
  is not repaired here.

**Residuals whose statement changes.** Ten pinned residuals contain
`arena_invariant`, directly or through `pool_invariant` and
`region_invariant`, and now state the conjunct too: pool Cat 2a's
`pool_init_ensures_part4` and `pool_init_call_arena_alloc_requires`; pool
Cat 2d's `pool_reset_call_arena_alloc_requires` and
`pool_reset_reset_ensures_2_part3`; region Cat 2's four
(`region_begin_ensures_5`, `region_attach_arena_ensures_2`,
`region_register_ensures_part2`, `region_set_parent_ensures_2`); and
`region_end_ensures_4_part1` and `region_end_call_arena_reset_requires`.
Their names and pins do not change. Their written arguments predate the
conjunct; the scoring re-reads each for it. None of those functions writes an
`Arena`'s `buffer` or `capacity`, so the step each argument needs is that the
conjunct survives unchanged.

**What it settles.** If nothing moves, the repair is invisible to WP, as G4
says it must be: the nine postconditions prove before it and after it, and
only an argument tells the true version from the false one — every write
lies in the buffer, which the invariant now separates from `*arena`. That
argument is part of their evidence from here on. If a goal moves, the
conjunct reached WP in a way this reading does not foresee, and the log says
where.

### G4 — listing entry 1, scored (fix 7e5757f; CI #1346, CC #33, probe run #2)

**Result: exact. Nothing moved.** The fix commit changed no pin (prediction
e51f1d9), and every job of CI #1346 and CC #33 was green, so every unit's
proved line, count and roll-call are as they were, by set equality. The G4
probe, re-run by the same push at its pinned `4 / 7`, was green (GitHub's run
pages, read 2026-10-09).

**By part.**

1. No goal generated or removed (H): held. Every proved line is unchanged,
   totals included.
2. Entry 1's nine postconditions stay proved (H): held. None is among the
   residuals.
3. No other goal changed status (M): held, by set equality in every unit.
4. Every pin stays (H): held.
5. The G4 probe green at `4 / 7` (H): held. The pinned observation repeated.
6. The C probes (H): unchanged at run time, checked locally. Since the fix,
   `p_arena_self.c`'s calls and `p_pool_self.c`'s case A are outside
   `arena_init`'s contract; case B is not (entry 2).

**What it shows.** The repair is invisible to WP, as G4 said it must be.
Before it, nine proved postconditions were false for calls the contracts
admitted; after it, they are true; WP's output is the same in every unit.
What tells the two apart is the argument the record now carries — every write
lies in the buffer, which the invariant separates from `*arena` — and the
nine are the first goals the record holds as proved with an argument as
their evidence. G4's listing settles how such goals are filed.

**The ten residual arguments, re-read.** Each of the ten residuals that
state the conjunct is covered by its argument as written: the conjunct
depends only on the `Arena`'s address, `buffer` and `capacity`, which no
function involved changes, and wherever an argument re-establishes
`arena_invariant` it does so through an arena function's postcondition,
which now carries the conjunct. Pool Cat 2a and 2d and region Cat 1 and 2
each carry a coverage update saying so.

### G4 — listing entry 2, committed prediction for the repair (before the fix commit)

**Decision (2026-10-09).** The repair recommended with the finding, in
`core/pool.h`. `pool_invariant` gains the conjunct
`\separated(p, p->arena->buffer + (p->base_mark .. p->end_mark - 1))`.
`pool_init` gains a last precondition, so no clause is renumbered:
`requires \separated(pool, ((char *)arena->buffer) + (arena->offset .. arena->capacity - 1));`,
the separation `stringbuf_init_arena` states as `sep_storage`. The header's
list of unsafe patterns says it for callers. No executable change.

No WP run on the changed code precedes this prediction. The only local checks
were the C probe and a Frama-C 25.0 parse of `core/pool.h`, the one unit that
includes it, which accepts both clauses and carries them in the normalized
AST.

**Predicted: nothing moves, in any unit.**

- (H) No goal is generated or removed. `pool_init` has no verified caller,
  and the conjunct does not split.
- (H) `pool_reset_secure`'s `ensures pool_invariant(pool)` stays proved: WP
  proves it across the character-type write whether or not the `Pool` is
  separated from its slots.
- (M) No other goal changes status. Wherever `pool_invariant` is
  established, the `Pool`'s address, `arena`, `base_mark` and `end_mark` and
  the `Arena`'s `buffer` are left as they were — `pool_alloc` writes `used`,
  `pool_reset` writes `used` and calls arena functions whose frames keep
  `buffer` — or, at `pool_init`, set from the region `arena_alloc` returns,
  inside the free tail the new precondition separates from the `Pool`.
- (H) Every pin stays as it is; the fix commit changes none.
- (H) `p_pool_self.c` behaves at run time as before; since the fix its case
  B is outside `pool_init`'s contract, as case A is outside `arena_init`'s.

**Residuals whose statement changes.** Two pinned residuals contain
`pool_invariant` and now state the conjunct too: pool Cat 2a's
`pool_init_ensures_part4` and pool Cat 2d's `pool_reset_reset_ensures_2_part3`.
Their names and pins do not change; the scoring re-reads their arguments for
it.

**What it settles.** As for entry 1: if nothing moves, the repair is
invisible to WP, and `pool_reset_secure`'s postcondition is true from here on
by an argument — the zeroed range lies in the slots, which the invariant now
separates from `*pool`, and in the arena's buffer, which entry 1's conjunct
separates from `*arena`.

### G4 — listing entry 2, scored (fix f3d9520; CI #1347, CC #34)

**Result: exact. Nothing moved.** The fix commit changed no pin (prediction
f75802c), and every job of CI #1347 and CC #34 was green, so every unit's
proved line, count and roll-call are as they were, by set equality. Record
coverage #43 was green. The G4 probe did not run: the push touched none of
its paths. (GitHub's run pages, read 2026-10-09.)

**By part.**

1. No goal generated or removed (H): held. Every proved line is unchanged,
   totals included.
2. `pool_reset_secure`'s `ensures pool_invariant(pool)` stays proved (H):
   held. It is not among the residuals.
3. No other goal changed status (M): held, by set equality in every unit.
4. Every pin stays (H): held.
5. `p_pool_self.c` (H): unchanged at run time, checked locally. Since the
   fix, its case B is outside `pool_init`'s contract, as its case A is
   outside `arena_init`'s.

**What it shows.** As for entry 1, the repair is invisible to WP.
`pool_reset_secure`'s `ensures pool_invariant(pool)` is one of entry 1's
nine; it was false in a second way, with the `Pool` inside its own slots.
Its argument now has both halves: the zeroed range lies in the slots, which
`pool_invariant` separates from `*pool`, inside the arena's buffer, which
`arena_invariant` separates from `*arena`. The count of goals proved with an
argument as their evidence stays nine.

**The two residual arguments, re-read.** Pool Cat 2a's goal 1 and Cat 2d's
goal 4 state the conjunct, and each carries a coverage update. Both
arguments cover it only under a presumption they make and the contracts do
not state: that the `Pool` and its `Arena` do not overlap. Re-reading them
for this conjunct brought back VERIFY-033 F4's hypothesis, which G5 takes
up.

### G5 — the `Pool` is not separated from its `Arena` (opened)

**Origin.** VERIFY-033 F4 recorded this reading as a hypothesis for a later
arc: `pool_reset_reset_ensures_part3` (`pool->used == 0`) "fails because
nothing separates `*pool` from the `*arena` the call havocs", and
`pool_init`'s four `ensures` goals "read the same way". The argument blocks
still file those goals under arithmetic (pool Cat 2a) and delegation (pool
Cat 2d). Re-reading both blocks for entry 2's conjunct brought the
hypothesis back, and G5 takes it up.

**Finding.** `pool_init` requires `\valid(pool)` and `arena_invariant(arena)`
and, since the G4 pool repair, that the `Pool` lie outside the arena's free
tail. `pool_invariant` states `\valid(p)` and `arena_invariant(p->arena)`.
Nothing separates `*pool` from `*arena`, so a `Pool` that overlaps its
`Arena` is admitted.

In C the overlap reaches `pool_init`'s first postcondition. `pool_init`
stores the `Pool`'s first four fields, calls `arena_alloc`, then stores
`base_mark` and `end_mark`. `tools/probes/verify-039/p_pool_arena.c` places
the `Arena` at the `Pool`'s `base_mark` field in one heap block, so that
`base_mark` and `end_mark` are the bytes of the `Arena`'s `buffer` and
`capacity`. The first four stores miss the `Arena`, `arena_alloc` runs on it
intact, and the last two overwrite `buffer` with the region's offset, 0, and
`capacity` with the arena's new offset. `pool_init` returns true, and its
second and third postconditions hold. The `Arena` it was given can no longer
be used: its `buffer` field holds bytes stored as a `usize`, and the next
read of that field through its own type is undefined (C11 6.5p7; G4's scope
calls this the undefined step). Read as bytes, through `memcpy`, the field
is a null pointer, and on that reading the first postcondition,
`pool_invariant(pool)`, is false. The library and the probe make no
undefined access: the block is heap storage, each field the library reads
after a `Pool` store was last stored with a compatible type, and the probe
reads the overwritten fields only through `memcpy` and `memcmp`. The result
is the same in both build configurations, at LP64 and ILP32, with gcc and
clang at `-O0` and `-O2`, and the sanitizers are clean. No placement we
found reaches the fields of the other four goals below without such a read
inside the library: each overlap that reaches them makes the library read an
`Arena` or `Pool` field last stored through a field of another type.

WP leaves the first postcondition unproved. Its goal is pool Cat 2a's
goal 1, `pool_init_ensures_part4`, filed under arithmetic. Cat 2a's argument
derives the arithmetic conjunct "by construction", and the arena repair's
coverage update rests the `arena_invariant(p->arena)` part on
`arena_alloc`'s postcondition, presuming that nothing before the call
changes the `Arena`. Neither covers the two stores after the call, the ones
the probe uses: for goal 1 the argument is defective as written. In WP's
typed model the two structs' `usize` fields share one memory array and
their pointer fields another, so without the separation a store into either
may change the other, and a call that assigns `*arena` may change the
`Pool`. The other four goals VERIFY-033 F4 named state no arithmetic at all:

| Goal | Statement | Filed under |
|---|---|---|
| `pool_init_ensures_2_part4` | `pool->used == 0` | pool Cat 2a, goal 2 (arithmetic) |
| `pool_init_ensures_3_part4` | `pool->capacity == max_objects` | pool Cat 2a, goal 3 (arithmetic) |
| `pool_init_ensures_4_part3` | on failure, `arena->offset == \old(arena->offset)` | pool Cat 2a, goal 4 (arithmetic) |
| `pool_reset_reset_ensures_part3` | `pool->used == 0` | pool Cat 2d, goal 3 (delegation) |

Cat 2d's root-cause bullet describes `reset_ensures_part3` as the
re-establishment of `pool_invariant`; that is `reset_ensures_2_part3`, and
`reset_ensures_part3` is `pool->used == 0`.

Each states a field the function stores, or one it must leave as it found
it, across stores into the other struct or a call that assigns `*arena`:
`pool_init` stores four `Pool` fields before `arena_alloc`, and `pool_reset`
stores `used` before it. The missing separation suffices to explain all
four, and the arithmetic the record names explains none; whether adding it
closes them is the repair's prediction. Cat 2a's own note already doubts the
classification of the `ensures_part4` goals (LIMITATION-SUSPECTED). This is
G1's shape: a separation the contract lacks, goals unproved and misfiled,
and one of them not true for an admitted call.

**Repair (recommended, next).** `pool_init` gains
`requires \separated(pool, arena);` and `pool_invariant` the conjunct
`\separated(p, p->arena)`, so every function that takes the invariant has
it. The prediction will say which of the goals above close, and whether
`pool_init_call_arena_alloc_requires`, `pool_reset_call_arena_alloc_requires`
and `pool_reset_reset_ensures_2_part3`, which state `arena_invariant` or
`pool_invariant` across the same stores, go with them. One intervention per
prediction: it follows the runner pin's scoring.

### G4 — listing, entry 3: a `Bitset` whose fields lie in its own words

**Finding.** `bitset_mut`, and `bitset_view` within it, does not separate
`*bs` from `bs->words`. `bitset_init` requires the separation, but
`bitset_clear_all` and `bitset_set_all` require only `bitset_mut(bs)`, so
they admit a `Bitset` built otherwise, with its fields inside the words it
manages. Both write the words through `char *`: `bitset_clear_all` with
`mem_zero`, and `bitset_set_all` with `mem_set` before it calls
`bitset_clear_padding`, which requires `bitset_mut(bs)`. Three goals that WP
proves in `frama-c-bitset` are false when the write covers the `Bitset`:

| Function | Goals false when the write covers `*bs` |
|---|---|
| `bitset_clear_all` | `ensures bitset_mut(bs)` |
| `bitset_set_all` | at its call to `bitset_clear_padding`, `requires bitset_mut(bs)`; its own `ensures bitset_mut(bs)` |

None is among the residuals. `tools/probes/verify-039/p_bitset_self.c`
places the `Bitset` at the start of a heap block, with its words starting at
its `word_count` field, so that `word_count` lies in `words[0]`. With
capacity 128, two words, `bitset_mut` holds before each call.
`bitset_clear_all` leaves `word_count` at 0 and `bitset_set_all` at all
ones. The capacity is a multiple of 64, so `bitset_clear_padding` writes
nothing, and the state after `bitset_set_all` is the state at that call.
`bitset_mut` is VIOLATED at all three points. The result is the same in
both build configurations, at LP64 and ILP32, with gcc and clang at `-O0`
and `-O2`, and the sanitizers are clean. The block is heap storage, the
clobbering writes are character-type writes, and nothing reads `words[0]`
as a `u64`.

**Decision (recommended).** `bitset_mut` gains
`\separated(bs, bs->words + (0 .. bs->word_count - 1))`, which `bitset_init`
already establishes. The same separation bears on G6.

### G4 — listing, entry 4: `diag_render` into a buffer that covers the `Diag`

**Finding.** `diag_render` requires `d` readable with `diag_invariant(*d)`
and `buf` valid for `buf_size` bytes, and does not separate the two. It
writes through `char *`: `buf[0] = '\0'`, then `snprintf` at `buf + total`,
which on the first iteration is `buf` itself, under the trusted axiom whose
`assigns` is `((char *)buf)[0 .. size - 1]`. In the model those writes
cannot reach `d->depth`, a `usize`, so the loop invariant
`render_i_bounds`, `0 <= i <= d->depth`, proves preserved, and none of
`diag_render`'s goals is among the residuals.
`tools/probes/verify-039/p_diag_render.c` gives a `Diag` two frames and
points `buf` into it at the last frame's message, unused, with `buf_size`
running to the end of `depth`. `buf` is derived from `(char *)&d`, so it
ranges over the `Diag`'s bytes (C11 6.3.2.3p7). A pointer derived from the
`message` member could not go past it, and gcc at `_FORTIFY_SOURCE=3`
aborts one that tries; level 2, and clang, do not. Frame 0's message is
sized so that its rendered line is exactly as long as the distance from
`buf` to `depth`. `snprintf`'s terminating `'\0'` then lands on `depth`'s
first byte, its low byte on the project's little-endian targets, and
`depth` goes from 2 to 0. The loop has run once, so at its next head `i` is
1 and `depth` is 0: the invariant is false there, and the loop exits after
one frame of two. `diag_render`'s own postcondition holds. The result is the
same in both build configurations, at LP64 and ILP32, with gcc and clang at
`-O0`, `-O2` and `-D_FORTIFY_SOURCE=3`, and the sanitizers are clean. The
`Diag` is a declared object, every write into it from `buf` is a
character-type write, `depth` is read back through its declared type, and
the rendered line overlaps none of `snprintf`'s string arguments.

This goal is not a postcondition: it is a step of the proof, false while
the contract's own clauses hold. G4 concerns goals, and this one is proved
and false.

**Decision (recommended).** `diag_render` gains a precondition separating
`buf + (0 .. buf_size - 1)` from `*d`, guarded like `r_buf`.
`diag_render_frame` writes through `char *` with `buf` likewise unseparated
from `*f`, but no goal reads `*f` after its writes, so it is not an entry.

### G4 — listing, entry 5: a vec in its own items

**Finding.** The vec contracts require `*v` readable and writable and
`v->items` valid up to `capacity`, and do not separate the two. That holds
for `frama-c-vec`'s driver predicates, `vec_int_view` and `vec_int_mut`, and
for the inlined first `requires` of the macro-body contracts the `-CC` units
verify. Two placements falsify goals that WP proves.

*In the free tail.* `append_array` copies the new elements with
`mem_copy`, through `char *`, and its ensures 3,
`\forall k < \old(v->len): v->items[k] == \old(v->items[k])`, proves; of its
ensures only the fourth is among the residuals.
`tools/probes/verify-039/p_vec_append_self.c` places a `vec_int` at
`items[len]`, the first free slot, so the copy overwrites its `items` field.
The two "ints" appended are the bytes of a pointer to another array, stored
there as an `int *`. The copy gives the overwritten bytes the effective type
of their source, so after the call `v->items` reads, through its own type,
as a pointer to that array, and ensures 3 compares it with the old
elements: VIOLATED. `append_array` stores `v->len` again after the copy, so
ensures 1 and 2 hold, and it reads no element of `src` as an `int`. Both
build configurations, LP64 and ILP32, gcc and clang at `-O0`, `-O2` and
`-D_FORTIFY_SOURCE=3`; sanitizers clean. This is the `vec_int` of
`frama-c-vec` and `cc-vec`. At this placement `insert` with `i < len`
moves elements up to `items[len]`, the start of the vec's `items` field,
which `v->items[i] = item` then reads: that call is undefined. With
`i == len`, and in `push`, `try_push` and `push_unchecked`, the element
store itself lands on the `items` field: a typed store over a pointer,
G5's case. `remove`'s move, over `items[i .. len - 2]`, never reaches the
vec.

*In its live elements, where `len` is a `Rec`'s `due`.* `cc-vec_struct`
verifies `vec_Rec`, with `Rec` = `struct { u32 id; i64 due; }`. In the
x86_64 machdep, and with glibc on LP64, a `vec_Rec` placed at `items[0]` has
its `len` field exactly where `Rec[0].due` is: a `usize` (`unsigned long`)
over an `i64` (`long`), signed and unsigned counterparts, which the model
keeps in two arrays. Every store the library makes to
`v->len` then changes `Rec[0]`, an element the frames say is unchanged.
`tools/probes/verify-039/p_vec_rec_live.c` makes seven calls, each on a
fresh vec, and reads `Rec[0].due` through a plain `i64` lvalue, the
counterpart of the `usize` stored there, and through `memcpy`; seven goals
are VIOLATED, none among the residuals:

| Call | Goal |
|---|---|
| `insert(v, 2, item)` at `len` 2 | ensures 4, `items[k]` unchanged for `k < i` |
| `remove(v, 2, &out)` at `len` 3 | ensures 4, the same |
| `push`, `try_push` at `len` 2 | ensures 4, `items[k]` unchanged for `k < \old(len)` |
| `push_unchecked` at `len` 2 | ensures 3, the same |
| `pop(v, &out)` at `len` 3 | ensures 4, `items[k]` unchanged for `k < len` |
| `append_array(v, src, 2)` at `len` 2 | ensures 3, `items[k]` unchanged for `k < \old(len)` |

The vec's `items` field lies over `Rec[0].id` and its padding, and its
`capacity` over `Rec[1]`'s; the library reads them through their own types
and stores no element there, so every access is defined. Both build
configurations, gcc and clang at `-O0`, `-O2` and `-D_FORTIFY_SOURCE=3`,
sanitizers clean. On ILP32 `len` is four bytes and covers only half of
`due`, and the probe says so.
At `vec_int` the same placement changes `int` elements through a `usize`
store. Reading them through their own type afterwards is the undefined step
of G4's scope, so G4 does not count them. Read as bytes, as G5 reads
`buffer`, the same frames at `vec_int` (`frama-c-vec`, `cc-vec`), which WP
proves, are not true for an admitted call: that is G5's reading, and the
repair covers it.

**Decision (recommended).** The vec's mutable predicate, and the macro-body
`requires` that inlines it, gains
`\separated(v, v->items + (0 .. v->capacity - 1))`.

### G6 — bitset's single-bit residuals are false for a `Bitset` in its own words (opened)

**Finding.** In the x86_64 machdep WP uses, and with glibc on LP64, `u64`
and `usize` are the same C type, `unsigned long`, and share one memory
array in the model. With entry 3's placement,
`bitset_set`, `bitset_clear` and `bitset_toggle` store a word over
`word_count`: a same-type store, defined C, with no character write.
`bitset_set(bs, 0)` and `bitset_toggle(bs, 0)` leave `word_count` at 3,
and `bitset_clear(bs, 1)` at 0, and each one's `ensures bitset_mut(bs)` is
then false. Those three goals, `bitset_{set,clear,toggle}_live_ensures_4_part3`,
are residuals. VERIFY-020 files them in its single-bit family, under the
class it defines as "a true property the solver could not reach". They are
not
true for an admitted call: G5's shape, unproved and misfiled. The model
keeps the store and `word_count` in one array, so WP could not prove them,
and was right not to.

`bitset_assign` delegates to `bitset_set` and `bitset_clear`, and its live
`ensures bitset_mut(bs)` is not among the residuals: WP proves it from their
contracts. It is false whenever theirs is: proved, and false, because it
rests on residuals that are. `p_bitset_self.c`'s second part shows all
four, with both of `bitset_assign`'s branches, on LP64 Linux, sanitizers
clean. Where `u64` and `usize` are different types (ILP32; macOS, where
`u64` is `unsigned long long`), the compound assignment over `word_count`
reads it through `u64` before storing, which is undefined, and the part is
skipped.

**Repair.** Entry 3's: `bitset_mut` gains the separation. The prediction
for it will say which of the single-bit family's residuals close.

### G4 — listing, the rest of the survey

Every verified unit was read for the three kinds of pair G4's scope lists:
a character-type write (the `mem_*` functions, `memcpy`, `memset`,
`memmove`, `snprintf`, `vsnprintf`, and stores through `char`, `u8` or `i8`
lvalues) followed by a goal that reads another type; a write of any type
followed by a goal that reads the same bytes through a character type; and
writes and reads of signed and unsigned counterparts. For each write, every
goal evaluated after it that names an object it may overlap was checked.
Typed stores over an object of another, non-counterpart type in allocated
storage are G5's case, not G4's: the next read through the old type is
undefined. G5's case was not surveyed beyond `pool` and entry 5's `vec_int`
placements. Beyond entries 1 to 5:

- **Separated by a precondition or an invariant:** `stringbuf`
  (`stringbuf_wf`, and `sep` at `stringbuf_init_buffer`), the priority queue
  (`pq_wf`, and the separations of `out`), and `bitset_init`.
- **No goal after the write names another object:** `memory.h`'s
  primitives, the `u8` checked functions, `pool_alloc_zero`,
  `pool_try_alloc_zero`, `diag_render_frame` and `pq_peek`.
- **Disjoint by layout:** `diag_push`'s copies stay inside a frame's
  `message` array or the `frames` array, and `depth` lies outside both.
- **Counterparts:** the only pair in a verified unit is `vec_Rec`'s `i64`
  member against the vec's `usize` fields (`cc-vec_struct`), reached by the
  vec's own `usize` stores over a `Rec`'s `due` (entry 5).

The typed wrappers that `DEFINE_PRIORITY_QUEUE` generates are instantiated
in no verified unit. The listing's repairs follow G5's, one per prediction.

### Protocol — the Linux runner pinned to `ubuntu-24.04` (committed prediction, before the pin commit)

**Decision (2026-10-09).** Every job that runs on `ubuntu-latest` moves to
`ubuntu-24.04`, in all six workflows. That covers each such `runs-on`, the
`ubuntu-latest` value of the three matrices that list it (`build`,
`static-analysis`, `lifetime-debug`) with their `include` and `exclude`
entries, and the three `matrix.os` conditions in `static-analysis`. The
`misra` job already runs on `ubuntu-24.04`. A note in
`cmake-multi-platform.yml`'s header says why, and each other workflow points
to it. No source, driver, pin or gate changes.

**Why now.** Every `ubuntu-latest` job of CI #1346 and #1347 carries
GitHub's notice that the label moves to Ubuntu 26 from 2026-10-19. The
announcement (actions/runner-images#14748) moves it from Ubuntu 24.04 to
26.04 between 2026-10-19 and 2026-11-19, and names `ubuntu-24.04` as the
way to stay. While a label migrates, the image it names can differ from
push to push and, since each job is given its own runner, between the jobs
of one run. The `misra` job met this, its count flipping between 780 and
1004 across pushes, and was pinned for it (its banner;
`docs/misra-campaign.md`). What the Frama-C jobs measure depends on the
image too. It supplies the gcc that preprocesses each unit, which the jobs
give no `-std`, and the system the cached opam switch is restored onto,
since the cache key names the runner only by `runner.os`. A pin that moved
during the migration could not be told apart from a change of image, and
the audit's next rounds fall inside it.

**Predicted: nothing moves.**

- (H) Every job of the CI and CC runs on the pin commit is green, and every
  proved line, count and roll-call is as it is now. Until 2026-10-19
  `ubuntu-latest` is Ubuntu 24.04, so the pinned label selects the image the
  jobs already run on, and the opam cache key, which names `runner.os`
  (`Linux` on both labels), restores the same switch.
- (H) The G4 probe runs, because its workflow file changes, and is green at
  its pinned `4 / 7`.
- (H) Record coverage runs, because its workflow file and
  `cmake-multi-platform.yml` change, and is green. Of the workflow it reads
  only the roll-call arrays.

`vc-identity` and `f4-control` run only on dispatch, so this push does not
exercise their pin. Job names that carry the matrix's `os` value change
(`build (ubuntu-24.04, …)` for `build (ubuntu-latest, …)`), and nothing in
the repository reads them. Required status checks in the repository's
settings, if any name these jobs, are outside the tree and would need
renaming by hand.

**What it settles.** From the pin commit on, the record's Linux
measurements are made on a fixed Ubuntu release, 24.04, not on a moving
label. The image's updates within 24.04 continue, as they have for every
run so far. Moving to 26.04 is a change of its own, to be predicted like any
other. The Windows and macOS jobs keep their `-latest` labels; none of them
runs Frama-C. The rows of `docs/traceability.md` that say `ubuntu-latest`
describe the runs they were measured on and stay as written.

### Protocol — the runner pin, scored (pin 25133fe; CI #1348, CC #35, G4 probe run #3)

**Result: exact on every scored part.** The pin commit changed no pin
(prediction 400da1d). Every job of CI #1348 and CC #35 was green, so every
unit's proved line, count and roll-call are as they were, by set equality.
The G4 probe ran, because its workflow file changed, and was green at its
pinned `4 / 7`. Record coverage #44 was green. GitHub's notice that
`ubuntu-latest` will migrate, which every `ubuntu-latest` job of CI #1346
and #1347 carried, appears on no job of these runs; the macOS jobs still
carry their notices about queue times. (GitHub's run pages, read
2026-10-09.)

**By part.**

1. Every CI and CC job green, every pin as it was (H): held.
2. The G4 probe re-runs green at `4 / 7` (H): held.
3. Record coverage green (H): held.

**A stated reason, not observed as stated.** The prediction expected the
opam cache to restore the same switch in every job. Three jobs took about
forty minutes longer than before: `frama-c-error` 43m 7s (1m 17s at
CI #1346), `frama-c-bitset` 2h 56m (2h 17m) and the G4 probe run 46m 50s
(4m 29s at run #2). Every other job took within a few minutes of what it
took before. Forty extra minutes is consistent with a cold build of the
switch after a missed cache restore; the job logs, which need a sign-in,
would say. If that is what happened, those three jobs built Frama-C and its
provers from the pinned versions and still met every pin, a harder test
than the prediction set.

### G5 — committed prediction for the repair (before the fix commit)

**Decision (2026-10-09).** The repair recommended with the finding, in
`core/pool.h`. `pool_init` gains a last precondition,
`requires \separated(pool, arena);`, so no clause is renumbered, and
`pool_invariant` gains the conjunct `\separated(p, p->arena)`. The header's
list of unsafe patterns says it for callers. No executable change. No other
verified unit includes `core/pool.h`, so only `frama-c-pool` can move. The
fix commit carries that job's predicted pins, as G2's and G3's did.

No WP run on the changed code precedes this prediction. The only local
check is a Frama-C 25.0 parse of `core/pool.h`.

**Predicted.** With the separation, a store into `*pool` no longer reaches
`*arena` in the model, and a call that assigns `*arena` no longer reaches the
`Pool`. Of the nine pool goals in the roll-call, seven close and two stay:

| # | Goal | Statement | Predicted | Confidence |
|---|---|---|---|---|
| 1 | `pool_init_ensures_2_part4` | `pool->used == 0` | closes | H |
| 2 | `pool_init_ensures_3_part4` | `pool->capacity == max_objects` | closes | H |
| 3 | `pool_init_ensures_4_part3` | on failure, `arena->offset == \old(arena->offset)` | closes | H |
| 4 | `pool_init_call_arena_alloc_requires` | `arena_invariant(arena)` after the four stores | closes | H |
| 5 | `pool_reset_call_arena_alloc_requires` | `arena_invariant(pool->arena)` after the store to `used` | closes | H |
| 6 | `pool_reset_reset_ensures_part3` | `pool->used == 0` | closes | H |
| 7 | `pool_reset_reset_ensures_2_part3` | `pool_invariant(pool)` after the reset | closes | M |
| 8 | `pool_init_ensures_part4` | `pool_invariant(pool)` on success | stays | M |
| 9 | `pool_reset_call_arena_reset_to_requires_2` | `pool->base_mark <= pool->arena->offset` | stays | H |

- Goals 1 to 3 and 6 ask only that a field the function stores, or one it
  must leave as it found it, survive stores into the other struct or a call
  that assigns `*arena`; goals 4 and 5, that `arena_invariant` survive the
  stores into `*pool`. With the separation each follows from hypotheses WP
  already has: the callees' `assigns` and postconditions, and
  `pool_invariant`'s new conjunct at `pool_reset`.
- Goal 7 re-establishes every conjunct of `pool_invariant` from the
  precondition. The `Pool`'s fields other than `used` are kept, the arena
  calls keep `buffer` and `capacity` (their frames) and re-establish
  `arena_invariant`, and the two separations carry over. M because it is
  the largest of the seven.
- Goal 8 also needs the conjunct `capacity <= CANON_USIZE_MAX / object_size`,
  which `pool_init` must derive from `checked_mul`'s `no_overflow`
  assumption, `aligned_size <= CANON_USIZE_MAX / max_objects`: the same bound
  read the other way, which takes nonlinear reasoning about integer division.
  Cat 2a's argument does not cover that conjunct. If goal 8 stays, its
  argument gains it: `aligned_size * max_objects <= CANON_USIZE_MAX`, so
  `max_objects <= CANON_USIZE_MAX / aligned_size`. M: the provers may
  manage it, and then this row misses in the good direction.
- Goal 9 asks for `base_mark <= arena->offset`, which `pool_invariant` does
  not state, and the separation does not bear on it. It is also not true for
  an admitted call: after `arena_reset` on the pool's arena, which the
  header lists as unsafe but no contract excludes, `pool_reset`'s call to
  `arena_reset_to` violates it whenever `base_mark > 0`. It stays, and is a
  finding of its own after this scoring.

Also:

- (H) No goal is generated or removed. `pool_init` has no verified caller,
  and the conjunct does not split. The total stays 4045.
- (M) No other goal changes status. Every function that establishes
  `pool_invariant` keeps `p` and `p->arena`, so the new conjunct carries
  over.
- (H) No other unit moves.

**Pins carried by the fix commit.** `frama-c-pool`: `3993 / 4045`, 52
unproved, the roll-call less goals 1 to 7. A green run means every row held.
A red `frama-c-pool` is a miss, read from its log, and a ratchet follows.
Until the scoring commit retires their rows, record coverage, which is
advisory, will list the seven as stale in pool Cat 2a and Cat 2d.

**What it settles.** If goals 1 to 7 close, VERIFY-033 F4's hypothesis is
confirmed by intervention for four of the five goals it named; the fifth,
goal 8, needs the separation (G5's probe) but not only that. Pool Cat 2a
keeps goal 8, filed under the division bound; Cat 2d keeps goal 9, which an
admitted call falsifies, to be recorded as a finding rather than filed under
an argument.

### G5 — scored (fix 528bfa1; CI #1349, CC #36)

**Result: exact.** The fix commit carried the predicted pins (prediction
f146e51), and every job of CI #1349 and CC #36 was green. `frama-c-pool`
stood at `3993 / 4045` with 52 unproved, its roll-call the old one less the
seven, by set equality, and no other unit moved. Record coverage #45 was
green. (GitHub's run pages, read 2026-10-10.) It is advisory: run on the
tree at 528bfa1, it lists the seven as stale in pool Cat 2a and Cat 2d, as
the prediction said, and the two headings' counts as drifted, which the
prediction did not say. This commit removes the rows and corrects the
counts.

| # | Goal | Predicted | Observed |
|---|---|---|---|
| 1–6 | `pool_init_ensures_2_part4`, `_3_part4`, `_4_part3`, `pool_init_call_arena_alloc_requires`, `pool_reset_call_arena_alloc_requires`, `pool_reset_reset_ensures_part3` | close (H) | closed |
| 7 | `pool_reset_reset_ensures_2_part3` | closes (M) | closed |
| 8 | `pool_init_ensures_part4` | stays (M) | stayed |
| 9 | `pool_reset_call_arena_reset_to_requires_2` | stays (H) | stayed |

Also held: no goal generated or removed, the total still 4045 (H); no other
goal changed status (M), by set equality; no other unit moved (H).

**What it shows.** VERIFY-033 F4's hypothesis is confirmed by intervention
for four of the five goals it named, and the same separation closed three it
did not name: the two call-site `arena_invariant` goals and `pool_reset`'s
`pool_invariant`. Seven goals the record filed under arithmetic or
delegation were the missing separation. The fifth goal F4 named,
`pool_init_ensures_part4`, stayed, as predicted: the separation is necessary
for it to hold (G5's probe) but not sufficient for WP to prove it, and pool
Cat 2a's argument now covers every conjunct, the division bound included.
Pool's own residuals are down from nine to two. No block retires, so *A*
stays at 12.

**Two of the three long run times did not recur.** `frama-c-error` took
49s and `frama-c-bitset` 2h 18m, their usual times; the G4 probe did not
run, the push touching none of its paths. That fits a one-off missed cache
restore at CI #1348; the logs would still say.

### G7 — `pool_reset` after its arena is reset (opened)

**Finding.** `pool_reset` requires `pool_invariant(pool)` and calls
`arena_reset_to(pool->arena, pool->base_mark)`, which requires
`mark <= arena->offset`. `pool_invariant` does not state that relation, and
the arena's own functions can break it while `pool_invariant` stays true:
`arena_reset` sets the offset to 0. The pool header lists resetting the
pool's arena while the pool is live as unsafe; no contract excludes it. The
call-site goal, `pool_reset_call_arena_reset_to_requires_2`, is pool
Cat 2d's last goal, filed under arena delegation. G5's prediction kept it
(H) and already noted that an admitted call falsifies it; it stayed. The
independent review of that prediction found the call sequence.

`tools/probes/verify-039/p_pool_reset_mark.c` takes one byte from the arena
before `pool_init`, so `base_mark` is 16, then resets the arena.
`pool_invariant` still holds, so `pool_reset` is admitted, and its call to
`arena_reset_to` has mark 16 and offset 0: the precondition is false. In the
proof configuration the call goes ahead and moves the offset forward; in the
default build `arena_reset_to`'s `require_msg` stops the program, the
run-time check catching the same false precondition. Both build
configurations, LP64 and ILP32, gcc and clang; the sanitizers are clean.

This is G1's shape again: a goal unproved because it is not true for an
admitted call, filed under an argument (delegation) that presumes the arena
was not reset under the pool.

**Repair (recommended).** `pool_invariant` gains
`p->base_mark <= p->arena->offset`. `pool_init` establishes it, since the
region starts `needed` bytes below the new offset. `pool_alloc` keeps it: it
touches no arena field, and G5 separates the two structs. `pool_reset`
re-establishes it: `arena_reset_to` sets the offset to `base_mark`, and
`arena_alloc` never lowers it. The goal then follows from the precondition.
A stronger conjunct, `end_mark <= arena->offset`, would state the header's
rule in full, the whole region reserved. But `pool_reset` can re-establish
it only if its `arena_alloc` provably succeeds, which needs the alignment of
`buffer + base_mark`, from which `arena_alloc` computes its pad; the
invariant does not state it, so it is left for later. With the weaker
conjunct, a rollback to or into the pool's region still meets the
contracts: `arena_reset` when the region starts at offset 0, or
`arena_reset_to` with a mark inside it. The header's unsafe patterns name
the first and not the second; the repair adds the second.

### G7 — committed prediction for the repair (before the fix commit)

**Decision (2026-10-10).** The repair recommended with the finding, in
`core/pool.h`: `pool_invariant` gains a last conjunct,
`p->base_mark <= p->arena->offset`. The header's unsafe pattern on resetting
the pool's arena says what the contracts now exclude, and a new one names
`arena_reset_to` into the pool's region, which they still admit. No
executable change. Only `frama-c-pool` can move, and the fix commit carries
its predicted pins.

No WP run on the changed code precedes this prediction. The only local
check is a Frama-C 25.0 parse of `core/pool.h`.

**Predicted.**

- (H) `pool_reset_call_arena_reset_to_requires_2` closes. With the conjunct
  in `pool_reset`'s precondition, the call-site precondition is one of its
  hypotheses: at the call, `pool->base_mark` and `pool->arena->offset` are
  those of the precondition.
- (M) Every proved goal that establishes `pool_invariant` still proves.
  `pool_alloc` changes neither `base_mark` nor the offset. In `pool_reset`,
  `arena_reset_to` sets the offset to `base_mark`, the store to `used` does
  not reach the `Arena` (G5), and each of `arena_alloc`'s behaviours leaves
  the offset at or above its old value. `pool_reset_secure` takes its
  postcondition from `pool_reset`'s contract. The goals at the call sites
  that require it still prove too: `pool_try_alloc` and `pool_alloc_zero`
  pass their precondition to `pool_alloc`, `pool_try_alloc_zero` to
  `pool_alloc_zero`, and `pool_reset_secure` to `pool_reset`: directly on
  its first path, and on its second after `mem_secure_zero`, whose `char`
  writes reach neither `base_mark` nor the offset in the model, and in C
  stay inside the slots, which `pool_invariant` keeps the `Pool` off and
  `arena_invariant` the `Arena`. M because
  `pool_reset_reset_ensures_2_part3` closed only at CI #1349 and gains a
  conjunct.
- (M) `pool_init_ensures_part4` stays, as at G5. The new conjunct is linear
  there (the region starts `needed` bytes below the new offset), and nothing
  changes for the division bound, the conjunct its argument names.
- (H) No goal is generated or removed: the conjunct does not split, and the
  total stays 4045.
- (H) No other unit moves.

**Pins carried by the fix commit.** `frama-c-pool`: `3994 / 4045`, 51
unproved, the roll-call less the closing goal. A green run means every row
held; a red `frama-c-pool` is a miss, read from its log, and a ratchet
follows. Until the scoring commit retires the row, record coverage, which is
advisory, will list the goal as stale, pool Cat 2d as empty and its
heading's count as drifted.

**What it settles.** If the goal closes, pool Cat 2d covers nothing and
retires: *A* in force 12 → 11. Then every goal it ever held has closed,
and its delegation reading covered none of the last four. Pool's own
residuals come down to one, the division bound.

### G7 — scored (fix f5c616c; CI #1350, CC #37)

**Result: exact.** The fix commit carried the predicted pins (prediction
8cd3eff), and every job of CI #1350 and CC #37 was green. `frama-c-pool`
stood at `3994 / 4045` with 51 unproved, its roll-call the old one less
`pool_reset_call_arena_reset_to_requires_2`, by set equality, and no other
unit moved. Record coverage #46 was green. (GitHub's run pages, read
2026-10-10.) Run on the tree at f5c616c, it lists the goal as stale, pool
Cat 2d as empty and its heading's count as drifted, as the prediction said.
This commit retires the block.

| # | Goal or claim | Predicted | Observed |
|---|---|---|---|
| 1 | `pool_reset_call_arena_reset_to_requires_2` | closes (H) | closed |
| 2 | every proved goal that establishes `pool_invariant`, and the goals at the call sites that require it | still prove (M) | held, by set equality |
| 3 | `pool_init_ensures_part4` | stays (M) | stayed |
| 4 | no goal generated or removed, the total 4045 | (H) | held |
| 5 | no other unit moves | (H) | held |

**What it shows.** The goal had G1's shape: not true for an admitted call,
and filed under an argument, delegation, that presumed what no contract
stated. One conjunct closed it. Pool Cat 2d covers nothing and retires:
*A* in force 12 → 11. All eight goals the block ever held have closed: three
at VERIFY-023, one at VERIFY-036 F6, three at G5 and this one. Its
delegation reading covered none of the last four. Pool's own residuals are
down to one, the division bound, which pool Cat 2a's argument covers;
`frama-c-pool`'s 51 are arena.h's 50 and that one. No job ran long:
`frama-c-error` took 1m 8s and `frama-c-bitset` 2h 18m.

`docs/verification.md`'s rows for arena.h, pool.h and region.h, which
predated VERIFY-036, now carry the pins CI #1350 confirmed.

### G8 — `bitset_not`'s postcondition and loop invariant are false as written (opened)

**Finding.** `bitset_not`'s live behaviour ensures
`\forall integer k; 0 <= k < bs->word_count ==> bs->words[k] == ~\old(bs->words[k])`,
and its loop carries the same equation over the words already inverted, as
`loop invariant 2`. In ACSL the value of a C integer is promoted to a
mathematical integer, and `~` complements its infinite two's-complement
representation: `~x` is `-x - 1` (ACSL 1.18, §2.2.4 and §2.2.4.6, whose
Example 2.6 gives `~5 == -6`; Frama-C's E-ACSL plugin, run locally,
translates the term with GMP's `mpz_com`). For a `u64` word the right-hand
side is negative and the left-hand side is not. Every live call has at
least one word, so the postcondition is false for every call, and the
invariant at every loop head after the first.

Read as the comment above the loop intends, with the complement taken in
`u64`, the equation holds for every word but, possibly, the last. After the
loop `bitset_not` calls `bitset_clear_padding`, which clears the last word's
bits from `capacity % 64` up, so for a capacity that is not a multiple of 64
the last word is not the complement either, unless every padding bit was set
on entry, which `bitset_pad` excludes and the precondition, `bitset_mut`
alone, does not.
`tools/probes/verify-039/p_bitset_not.c` inverts a fresh `Bitset` of
capacity 100: word 0 is the complement, and word 1 is
`0x0000000fffffffff`, not `0xffffffffffffffff`, `VIOLATED`. At capacity 128
both words hold. Both build configurations, LP64 and ILP32, gcc and clang at
`-O0` and `-O2`; the sanitizers are clean.

**Filing.** The invariant's preservation,
`bitset_not_loop_invariant_2_preserved`, is a residual, and VERIFY-020
files it as array framing ("array framing on and / or / xor / not";
`data/bitset.h`'s comment glosses it "writing words[w] leaves words[0..w-1]
alone"). It is unproved because it is false. The postcondition is not among the
residuals: WP proves it. A false goal can be proved only from a false
hypothesis, and for an ordinary `Bitset` the only one WP has there is the
invariant, which WP assumes at the loop's exit, where it states that a word
equals a negative number. Every goal after the loop has that hypothesis, so
its proof says nothing about whether it is true. VERIFY-020 counts the
postcondition among what contracting the loop delivered ("6 → 3 with both
postconditions proved"); it was never true. The other one,
`ensures bitset_pad(bs)`, also follows from `bitset_clear_padding`'s
postcondition, and nothing here says it needed the contradiction.

This is G1's shape twice over: a residual unproved because it is false,
filed as prover weakness, and a proved goal that rests on it, as at G6.

**Repair (recommended).** Both clauses take the complement in `u64`. The
postcondition states it for every word but the last, and for the last, when
`capacity % 64 != 0`, the complement with the bits from `capacity % 64` up
cleared, and otherwise the complement. That needs `bitset_clear_padding` to
state what it leaves in the last word; today it states only `bitset_pad`.
With entry 3's separation, in the bitset repair.
