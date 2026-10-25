# Verification Record

## Overview

This document records the formal verification status of Canon-C library
headers. Verification is performed using Frama-C WP (Weakest Precondition)
with ACSL contracts, enforced in CI on every push to master.

Combined verification status across all annotated headers:

| Metric               | Value                                                                          |
|----------------------|--------------------------------------------------------------------------------|
| **Headers verified** | 20 (checked.h, bits.h, compare.h, ptr.h, slice.h, memory.h, arena.h, pool.h, region.h, error.h, option, result, borrow.h, diag.h, vec, deque, bitset, priority_queue.h, range.h, stringbuf.h) — lifetime.h is a 21st unit of narrower scope, see VERIFY-021 |
| **Functions**        | 456 annotated and verified (371 + priority_queue.h's 41 + range.h's 15 + stringbuf.h's 29; lifetime.h's 1 counted separately) |
| **Total obligations**| 49762 (summed over the 20 verification units; substrate goals are re-emitted in each downstream unit, so this counts goal-instances, not distinct obligations) |
| **Proved automatic** | 49301 (99.07%)                                                                 |
| **Unproved**         | 461 (each filed by name in `docs/deviations.md`; see per-header sections)      |

*Recomputed 2026-10-25 (VERIFY-008, the overlap fix, predicted).*
`mem_regions_overlap` compares the two addresses as `uintptr_t` values, not
the pointers with `<`, which C leaves undefined across objects. Its goals go
from 37 to 31 under WP 29, all proved, as the overlap probe measured, in each
of the eight units that verify memory.h's functions (memory.h, arena.h,
pool.h, region.h, vec, bitset, priority_queue.h, stringbuf.h) and in
arena-32 and the two vec `-CC` units: 48 obligations and 48 proved fewer
over the twenty units, no unproved goal added or removed. Dated notes below
give the pins of their dates.

*Recomputed 2026-10-21 (VERIFY-040 round 2's score).* Summed again from the
twenty pins as CI #1375 holds them, vec's at the line that run measured: +15
obligations, +21 proved and −6 unproved, from CI #1374's 49795, 49328 and 467.
memory.h's `regions_overlap`, stated by `\separated` since 2d36ca1,
`pq_push_result`'s assertion, vec's frames and `bitset_assign`'s early return
closed six of the 467 (VERIFY-040 round 2), the queue's three copies between a
caller's memory and its buffer among them, so that no unproved clause is now
one whose value C leaves undefined at some admitted calls: vec's no-overlap
goal at `append_array`'s copy stays, the byte view, filed under diag.h's
block. The vec, bitset and priority_queue.h rows of the data/ table below, a
note under each of the three summaries, and `docs/traceability.md`'s proof
table are brought to the same pins in the same commit.

*Recomputed 2026-10-20 (VERIFY-040 round 1's score).* Summed again from the
twenty pins as CI #1374 holds them: −6 obligations, +2 proved and −8 unproved,
from the 49801, 49326 and 475 of CI #1373 that the note below gives. vec's
`swap`, deque's `swap` and `bitset_init` closed eight of the 475 (VERIFY-040
round 1), the clause on a struct exchange whose operands may overlap among
them, so that the clauses whose value C leaves undefined at some admitted
calls are the four stated by pointer order across objects. The vec, deque and
bitset rows of the data/ table below, a note under each of the three
summaries, and `docs/traceability.md`'s proof table are brought to the same
pins in the same commit.

*Recomputed 2026-10-20 (VERIFY-039, the main pass's docs-only sweep).* This
card previously read 20 / 456 / 49680 / 48604 / 1076, as recomputed at
VERIFY-035. It is summed again from the twenty enforced CI pins, as CI #1373
held them: +121 obligations, +722 proved, −601 unproved. The movements are
VERIFY-036's and VERIFY-039's, each step in its job's pin comment in
`.github/workflows/cmake-multi-platform.yml` and scored in
`docs/deviations.md`. The per-unit figures are in the roadmap tables below and
in `docs/traceability.md`'s proof table, brought to the same pins in the same
commit; the headers and functions rows do not move. Each of the 475 is pinned
in its job, by count and by name, but for `frama-c-bits`, which pins its
twelve by count and a sample of six names, all twelve named in
`frama-c-bitset`. Each is listed in `docs/deviations.md` under a block that
argues it true for every call the contracts admit, but for five clauses whose
value C leaves undefined at some of those calls, four stated by pointer order
across objects and one on a struct exchange whose operands may overlap: their
blocks argue them wherever C defines them, and the four, across objects, for
the property the order is meant to state. Since 140a6ac the record coverage
job fails a push that leaves a pinned name in no block. The per-header
sections keep the figures and readings of their own rounds; where VERIFY-039
found a reading wrong, a dated note beside it says so, and under the summary
of each unit whose pin has moved a note gives its pin at CI #1373.

*Recomputed 2026-10-03 (VERIFY-035).* This card previously read 19 / 427 /
44580 / 43593 / 987. It is now **summed directly from the twenty enforced CI
pins** rather than carried forward by deltas: the delta chain had missed the
VERIFY-026/027 movements in ptr, memory, arena, pool, region, vec, bitset and
priority_queue.h, and VERIFY-033/034's +20 in arena, pool, region and vec.
The per-unit figures are in `docs/traceability.md`'s proof table, brought to
the same pins in the same commit. **(7) stringbuf.h** (VERIFY-031..035;
enforced from the commit carrying VERIFY-035, pinned from #1322 and #1324)
adds a twentieth unit: +29 functions, +4848 obligations, +4767 proved, +81
unproved — all 81 inherited, **none own**.

*Corrected 2026-08-21.* This card previously read 15 / 315 / 30599 / 29899 /
700 — the state before deque was verified and before the vec F4 confirming
fix landed. Two independent movements are folded in here.

(1) **deque** (VERIFY-019; baseline CI #1234 / 5ae441d, set-identical
#1237 / 08bdf42, enforced #1238 / db0a864, re-confirmed #1239 / 06e4cad
and #1240 / dbf31c3) adds a sixteenth unit: +24 functions, +1668
obligations, +1601 proved, +67 unproved.

(2) The **vec F4 closure** contracted `get_ok`/`get_err` in
`vec_verify.h` at CI #1246 (c427548, 2026-08-17) — a deliberately red
run, the pin still reading 5231 / 5429 and 198 — and the acknowledged
ratchet followed at CI #1247 (43a46b1, 2026-08-18), moving the pin to
5271 / 5467 and EXPECTED_UNPROVED to 196. The two new contracts add 38
goals of their own, all 38 prove, and the two F4 goals flip: +38 total
against +40 proved. See VERIFY-018's DEMONSTRATED note in
`docs/deviations.md`.

Arithmetic: 30599 + 1668 + 38 = 32305; 29899 + 1601 + 40 = 31540;
700 + 67 − 2 = 765.

*Updated 2026-09-29.* This card previously read 18 / 412 / 41933 / 40982 /
951 — the state before range.h was verified. **(6) range.h** (VERIFY-028/029/030;
enforced CI #1317 / 9f7a9d0, name-identical to #1315 and #1316) adds a
nineteenth unit: +15 functions, +2647 obligations, +2611 proved, +36
unproved — all 36 inherited, **none own**. Arithmetic: 41933 + 2647 = 44580;
40982 + 2611 = 43593; 951 + 36 = 987.

*Corrected 2026-09-07.* This card previously read 17 / 371 / 37307 / 36379 —
the state before VERIFY-023 (+6 goals in each of the 7 units including ptr.h,
and 24 residuals closed) and before priority_queue.h was verified (+4584 / +4513).
Before 2026-08-27 it read 16 / 339 / 32305 / 31540 / 765.

(3) **bitset** (VERIFY-020; pinned CI #1260 / 6cb78da at 158, ratcheted
CI #1265 / 4241a10 to 163 after the F2/F5 fixes, re-confirmed CI #1266 /
16d0f0b; **the job's gate was not wired until CI #1285** — see VERIFY-020's
reading note) adds a seventeenth unit: +32 functions, +5008 obligations,
+4845 proved, +163 unproved (5002/4839 before VERIFY-023).

(4) **priority_queue.h** (VERIFY-022; enforced CI #1290 / 63d6705,
name-identical to #1289) adds an eighteenth unit: +41 functions, +4584
obligations, +4513 proved, +71 unproved. The first in-place data/-layer
module; its core operation calls a caller-supplied function pointer from
inside its loops, and the comparator is proved as a verified configuration
over compare.h's 24 built-ins.

(5) **VERIFY-023** (CI #1285) moved seven existing units without adding one:
ptr.h's four address helpers gained an `ensures` each, every TU including
ptr.h grew by 6 goals, and 24 residuals in arena, pool, region and vec that had
been recorded as arithmetic or call-site limits closed. Net: +42 obligations,
+90 proved, −48 unproved.

Arithmetic: 32305 + 5002 = 37307; 31540 + 4839 = 36379;
765 + 163 = 928; then 37307 + 42 + 4584 = 41933; 36379 + 90 + 4513 =
40982; 928 − 48 + 71 = 951. Figures above are the enforced CI pins at
HEAD (63d6705, CI #1290), and match the master table in
`docs/traceability.md` (whose totals run 4 higher on obligations and
proved because that table also carries lifetime.h's 4/4).

The slice.h baseline (367 / 390) carries a higher residual fraction
than the four primitives headers because it is the first Canon-C header
that crosses the C standard library boundary — it calls `memcmp` and
`strlen`, whose ACSL contracts require initialization and danglingness
reasoning that Frama-C 29 has not yet implemented. The 15 slice.h
residuals (3.8% of slice.h's obligations) are therefore not specific
to slice.h's design but are the cost of crossing into libc with the
current verifier; future headers using these functions will incur the
same residuals. See VERIFY-007 in `docs/deviations.md` for the full
classification.

The memory.h baseline (2805 / 2862) is the first Canon-C verification
round where inherited residuals from the substrate exceed memory.h-own
residuals: 23 of memory.h's 43 unproved goals are byte-identical to
goals already documented in VERIFY-002 (checked.h), VERIFY-006 (ptr.h),
and VERIFY-007 (slice.h), re-emitted in memory.h's run because
memory.h includes these headers transitively. The 20 memory.h-own
residuals fall into three categories: 5 \fresh/\freeable allocation
reasoning goals (mem_alloc/free/array_checked), 9 bitwise-alignment
integer theory goals (mem_align/is_aligned/get_alignment), and 6
memcmp call-site danglingness goals (mem_compare/equal/equal_bytes).
The initialization sub-class formerly counted here (6 goals) was closed
in VERIFY-012 by an explicit `\initialized` precondition; the remaining
20 are rooted in the `\dangling`/`\fresh`/`\freeable` feature gaps that
Frama-C 29 has not implemented. See VERIFY-008 in
`docs/deviations.md` for the full classification.

The 23:20 inherited:own ratio is the first quantitative data point
for the composable-verification thesis: substrate residuals propagate
without amplification across composition layers, and a layer that
builds on multiple already-verified headers inherits their residual
fingerprints unchanged. The 23 inherited residuals are byte-identical
between memory.h round 2 (before contract-shape fixes) and memory.h
round 3 (after fixes) — the round 3 fix removed 10 contract-shape
residuals and zero inherited ones, confirming that contract-level
edits do not perturb the substrate's residual surface.

The arena.h baseline (3369 / 3472) is the third Canon-C verification
data point and the first to demonstrate propagation through two
composition layers. arena.h includes memory.h, which transitively
includes ptr.h, slice.h, checked.h, and contract.h. arena.h's 43
inherited residuals are byte-identical to memory.h's full 43-goal
residual surface (memory.h's 23 inherited + memory.h's 20 own), and
arena.h adds 46 own residuals of its own across four categories:
8 ptr_span call-site residuals at arena_alloc / arena_alloc_aligned
(downstream of VERIFY-006's empty `nonnull` behaviors), 26
arithmetic-chain residuals at arena_alloc / arena_alloc_aligned (the
deepest residual class, arising from the deliberate readable form of
the `arena_can_fit` predicate), 10 wrapper-delegation residuals on
the zero/try variants, and 2 arena_free_bytes helper call-site
residuals. The 43:46 inherited:own ratio confirms the composable
verification claim at a third composition layer — substrate residuals
propagate without amplification, and arena.h is the first header to
observe propagation through two layers of composition (substrate →
memory.h → arena.h). See VERIFY-009 in `docs/deviations.md` for the
full classification.

The pool.h baseline (3775 / 3902) is the fourth Canon-C verification data
point and the first to observe residual propagation across a two-hop
transitive include. pool.h includes arena.h, which transitively includes
memory.h, ptr.h, slice.h, checked.h, and contract.h. pool.h's 89 inherited
residuals are byte-identical to arena.h's full 89-goal residual surface
(arena.h's 43 inherited + arena.h's 46 own), and pool.h adds 24 own residuals
across four categories: 5 pool_invariant-establishment arithmetic residuals at
pool_init (3 of them LIMITATION-SUSPECTED pending a manual invariant review),
6 ptr_elem-cascade residuals at pool_alloc / pool_get / pool_get_const
(downstream of VERIFY-006's empty `nonnull` behaviors), 5 bytes_from / mem_zero
call-site residuals on pool_alloc_zero / pool_as_bytes / pool_reserved_bytes,
and 8 arena-delegation + reset-wrapper residuals on pool_alloc_zero /
pool_reset / pool_reset_secure. The 89:24 inherited:own ratio is the most
lopsided yet — a direct consequence of pool.h sitting atop the deepest
substrate stack verified to date while adding the thinnest own surface: pool.h
reserves its region once at pool_init and computes slots by fixed-stride
ptr_elem, so it has no per-allocation alignment-pad arithmetic and arena.h's
26-goal cat 2b arithmetic-chain residual class does not recur in pool.h's own
surface. See VERIFY-010 in `docs/deviations.md` for the full classification.

> **Correction (2026-10-20, VERIFY-039).** The four baseline paragraphs above
> are as first written, each at its header's baseline. Their readings of the
> residuals as the cost of the verifier's feature gaps do not all hold. Of
> slice.h's fifteen, seven were false for calls the contracts admitted,
> memcmp's four `valid_*` preconditions at a zero length and strlen's string
> requirement (G17) and contract.h's handler pair (G14), and all seven closed
> at CI #1368. slice.h's eight danglingness goals, and memory.h's six, were
> false for admitted calls too until G13's repair (7624d8c), since which each
> caller requires that no byte it compares hold a dangling address; they hold
> for every admitted call and wait on WP's `\dangling`. Of memory.h's twenty
> own, the two allocation frames were false, `assigns \nothing` where libc
> lets malloc and free change the heap's state, and closed at G18 (806bdeb);
> `mem_is_aligned`'s two need a clause that `ptr_is_aligned`'s contract omits;
> and one integer-theory goal, an overflow at an address with only its top bit
> set, was false and closed at VERIFY-036 F1. arena.h's own categories, and
> pool.h's 2b, 2c and 2d, were read again and closed by VERIFY-023, VERIFY-036
> and VERIFY-039; pool.h's 2a keeps one goal, `pool_init`'s division bound, as
> their sections' notes say. The ratios stand as measured.

A note on totals: the 44580 obligation count is the row-sum of
each header's own WP-relevant goals — checked.h's 1755, bits.h's
757, compare.h's 208, ptr.h's 1959, slice.h's 394, memory.h's 2872,
arena.h's 3527, pool.h's 4009, region.h's 3698, error.h's 65,
option's 223, result's 215, borrow.h's 2458, diag.h's 3060, vec's
5473, deque's 1668, bitset's 5008, priority_queue.h's 4584 and range.h's 2647 (each
counted in full because each header was
verified atop its full substrate, with no separate substrate-free
measurement available for downstream headers). The CI WP steps for
downstream headers report larger numbers because their runs include
substrate via `#include` — for instance, ptr.h's CI step reports
1949/1959 (1943/1953 before VERIFY-023) due to checked.h growth at c3df659. Those +214 obligations
are checked.h's, not ptr.h's, and are counted in checked.h's row
above; double-counting them would inflate the aggregate.

> **Correction (2026-10-20, VERIFY-039).** This note describes the card as it
> stood at 44580, and reads each row as its header's own goals. Since
> VERIFY-035 the card sums the enforced pins, each its unit's full-run figure
> with the substrate goals the unit re-emits, as the card's obligations row
> says. At CI #1373 the twenty pins count checked.h's 1755, bits.h's 757,
> compare.h's 208, ptr.h's 1958, slice.h's 398, memory.h's 2879, arena.h's
> 3556, pool.h's 4076, region.h's 3730, error.h's 65, option's 228, result's
> 213, borrow.h's 2465, diag.h's 3058, vec's 5569, deque's 1673, bitset's
> 5041, priority_queue.h's 4688, range.h's 2652 and stringbuf.h's 4832
> goals: 49801.

> **Note (2026-10-20, VERIFY-040 round 1).** At CI #1374 deque's pin counts
> 1665 goals and bitset's 5043, the others as above: 49795.

> **Note (2026-10-21, VERIFY-040 round 2).** At CI #1375 vec's pin counts
> 5574 goals, bitset's 5052 and priority_queue.h's 4689, the others as at
> CI #1374: 49810.

> **Note (2026-10-25, VERIFY-008, the overlap fix, predicted).** memory.h's
> pin counts 2873 goals, arena.h's 3550, pool.h's 4070, region.h's 3724,
> vec's 5568, bitset's 5046, priority_queue.h's 4683 and stringbuf.h's 4826,
> the others as at CI #1375: 49762.

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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


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

> **Note (2026-10-20, VERIFY-039, batch 6).** The proof holds with N the width
> of the type: 64, or 32 for `checked_add`'s `usize` in `frama-c-arena-32`.
> The two goals are filed by name in VERIFY-039's block "Fixed-width
> arithmetic stated over unbounded integers", whose argument states this
> proof, with the fifteen other units that pin them. The pin is unchanged:
> 1753 / 1755 at CI #1373.

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
| **Proof obligations**  | 742 / 757 discharged automatically (98.02%)     |
| **Timeouts**           | 15 (all WP model limitations, see below)        |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120`         |
| **CI enforcement**     | Yes — exactly 15 named unproved goals expected  |
| **MC/DC coverage**     | 100% (52/52 condition outcomes)                 |
| **CI artifact**        | `wp-proof-bits` (full per-goal breakdown)       |

> **Correction (2026-10-20, VERIFY-039 batch 6 and G15).** The table gives the
> baseline's figures. Since 767a151 the job pins 745 / 757 with 12 unproved
> (first run CI #1368, held at CI #1373). Four of the fifteen were not WP
> model limitations but false postconditions: `bits_insert`'s `full_width` and
> `normal` ones and `bits_rotl`'s and `bits_rotr`'s `nonzero_shift` ones
> stated their results with shifts that ACSL reads over unbounded integers,
> where C's `<<` on a `u64` keeps the low 64 bits, so that
> `bits_rotl(0x8000000000000001, 1)`, the header's example, returns 3 where
> the postcondition asked for 2^64 + 3. G15 (9a6e1aa) casts each shift to
> `u64`, as C computes it. Three closed; `normal`'s holds for every admitted
> call since the cast and stays unproved. The twelve are true and filed by
> name in VERIFY-039's blocks, with manual proof arguments: fixed-width
> arithmetic stated over unbounded integers (seven: `bits_clear`'s complement,
> `bits_insert`'s `normal`, the two rotations' shift checks and the three byte
> swaps), the SWAR population count's range (one) and bit smearing to the next
> power of two (four). The job's roll-call is a sample of six names.

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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


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

> **Correction (2026-10-20, VERIFY-039 batch 6 and G15).** The grouping is the
> baseline's. Of the complement group's three, `bits_clear`'s is true;
> `bits_insert`'s `full_width` postcondition has no complement in it and was
> false; and its `normal` one, whose complement is sound, was false through
> its shifts and is true since G15's cast. Of the rotation group's four, the
> two shift checks are true and the two postconditions were false.
> `full_width`'s and the two rotation postconditions closed at CI #1368. The
> four "minimality" goals are all four postconditions of
> `bits_next_power_of_two`'s `normal` behaviour on its smearing path, the
> minimality being the fourth.

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

Expected output: `Proved goals: 745 / 757` with 12 unproved goals, the pin
since VERIFY-039 G15 (`742 / 757` with 15 before it).

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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


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
| **CI enforcement**     | Yes — 1729/1739 ptr.h-own goals with 10 named residuals; full WP run reports 1949/1959 (incl. checked.h substrate; 1943/1953 before VERIFY-023's four `ensures`) |
| **MC/DC coverage**     | 100% (42/42 condition outcomes)                 |
| **CI artifact**        | `wp-proof-ptr` (full per-goal breakdown)        |

> **Correction (2026-10-20, VERIFY-039 batch 6, G14 and G16).** The table and
> the sections below give the baseline's figures and readings. Since 767a151
> the job pins 1952 / 1958 with 6 unproved, all true (first run CI #1368, held
> at CI #1373). Before G14 to G17 it pinned 1959 / 1975 with 16: the ten
> below, and six pointer postconditions given to `ptr_align_up`'s and
> `ptr_align_down`'s `nonnull` behaviours at VERIFY-026. Ten of the sixteen
> did not hold for every admitted call, and all ten are gone: the handler's
> two (G14), category 3's two calls and the six pointer postconditions (G16),
> as the notes below say. The six left are checked.h's two and `align_up`'s
> and `align_down`'s formulas, filed by name in VERIFY-039's block on
> fixed-width arithmetic, and `align_padding`'s formula and
> `ptr_align_padding`'s `\result < align`, filed by name under VERIFY-008's
> Category 2. The Baseline note's "ptr.h's source has not materially changed"
> no longer holds: VERIFY-023 and VERIFY-026 gave five `nonnull` behaviours
> their results, and G16 restated `align_padding` and the two rounding
> functions. The ptr.h-own figure of 1729 / 1739 was not re-measured after
> VERIFY-023.

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
**1949 / 1959** (1943 / 1953 before VERIFY-023) instead of 1729 / 1739. The +214 difference is the
12 division and modulo functions added to checked.h at commit
`c3df659` (CI #804, Apr 27 2026) — see VERIFY-002 and the checked.h
section. Those 214 obligations belong to checked.h, not to ptr.h;
they are counted separately in checked.h's own row of the per-header
table. The CI wrapper uses the 1949/1959 figure for enforcement (1943/1953 before VERIFY-023)
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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


| Prover         | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Qed (internal) | 1563             | 1ms–10ms          |
| Alt-Ergo 2.6.3 | 83               | 16ms–45ms         |
| CVC5 1.2.1     | 3                | 81ms–255ms        |
| Z3 4.15.2      | 11               | 20ms–69ms         |
| Timeout        | 10               | >120s (see below) |
| **Total**      | **1729 / 1739**  |                    |

The CI WP step reports 1949/1959 (1943/1953 before VERIFY-023) because its run includes checked.h
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

> **Correction (2026-10-20, VERIFY-039 batch 6, G14 and G16).** Only the first
> two categories stand, and both are true. Category 2's obstacle is not a
> bridge to multiples of `align`, which the three formulas do not state:
> `align_up`'s and `align_down`'s is C's complement and wrap in the width, and
> `align_padding`'s the bound that rounding by a mask keeps, an identity
> VERIFY-008's Category 2 names, under which it is filed. Category 3's two
> calls were false for an address within `align - 1` bytes of the top of the
> address space, which nothing in the contracts excluded: on a 32-bit build
> `ptr_align_up` returned NULL for a non-null pointer. No bound was lost in
> the round trip; nothing bounded the address. Since f83ad1c `align_padding`
> computes `(0 - n) & (align - 1)` and requires no bound on `n`, and
> `ptr_align_up` requires that `p`'s address fit; both calls' goals are gone
> or proved, and the category's third goal, `ptr_align_padding`'s bound, is
> true and filed with `align_padding`'s. Category 4's two were false for every
> call, not unprovable by construction: the invariant was `\false`, which
> holds in no state, and the handler's contract stated no `terminates` clause,
> so WP 29 checked the default, that it terminates, for a loop that never
> ends. Since c9ac146 the handler states `terminates \false;` and its loop
> carries no invariant: the invariant's goals are gone and the termination is
> proved, in every unit that includes contract.h.

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

> **Correction (2026-10-20, VERIFY-039).** Two bullets above no longer hold.
> The `nonnull` behaviours they call empty state their results: VERIFY-023
> gave `ptr_offset`'s and `ptr_offset_const`'s, VERIFY-026 `ptr_align_up`'s,
> `ptr_align_down`'s and `ptr_retreat`'s, and G16 restated the two rounding
> functions' as the result's address, which need not lie in `p`'s object.
> `ptr_is_aligned`'s states none, the clause `mem_is_aligned`'s two goals
> want. And the handler's contract did not state `terminates \false` until G14
> (c9ac146), which also removed its `loop invariant \false`: the two goals
> were false, not correct by construction, and are gone.

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

Expected output: `Proved goals: 1952 / 1958` with 6 unproved goals, the pin
since VERIFY-039 G14 and G16 (`1959 / 1975` with 16 before them;
`1949 / 1959` with 10 at VERIFY-023, `1943 / 1953` before it). (Each total
reflects the full WP run on ptr.h's translation unit, including
checked.h. ptr.h's own contribution is 1729 / 1739; the +214 are
checked.h's div/mod goals — see Baseline note above.)

---

## core/slice.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | 1e0d0fe (Canon-C CI #821)                       |
| **Functions**          | 22 of 22 non-macro functions annotated          |
| **Proof obligations**  | 379 / 394 discharged automatically (96.19%)     |
| **Timeouts**           | 15 (all documented under VERIFY-007)            |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 379/394 with 15 named goals expected      |
| **MC/DC coverage**     | 93.1% (54/58 condition outcomes since VERIFY-039 G17; 92.6%, 50/54, before — see MCDC-002) |
| **CI artifact**        | `wp-proof-slice` (full per-goal breakdown)      |

> **Correction (2026-10-20, VERIFY-039 batch 6, G13, G14 and G17).** The
> table's proof figures are the baseline's. Since 767a151 the job pins
> 390 / 398 with 8 unproved (first run CI #1368, held at CI #1373). Seven of
> the fifteen below were false for calls the contracts admitted, and closed at
> CI #1368: the handler's two (G14, as in ptr.h's section); memcmp's validity
> of each pointer at `bytes_equal` and `str_equal`, which their contracts did
> not establish at a zero length; and strlen's string requirement, which
> `str_from_cstr` did not state (G17). Since 64f4bd1 the two equality
> functions return true at a zero length before memcmp, and `str_from_cstr`
> and `borrowed_str_from_cstr` require
> `cstr == \null || valid_read_string(cstr)`. The eight left are memcmp's
> danglingness preconditions at the four functions that call it. They were
> false for admitted calls too, the bytes of a pointer to a freed block
> compared, until G13's repair (7624d8c): each function now requires that no
> byte it compares hold a dangling address. They hold for every admitted call,
> wait on WP's `\dangling`, and are filed by name under VERIFY-008's
> Category 3. The zero-length returns add four MC/DC outcomes, all covered.

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
WP-verified in this baseline. Contract specifications were retained in
the macro body as human-readable comments — superseded by VERIFY-025,
which turns them into live ACSL contracts under `-CC` (115/115 proved,
zero residuals, and one latent defect found: F1). These functions are
validated by unit testing (90 tests in `test/core/slice_test.c`),
fuzzing, and 93.1% MC/DC coverage on the i32 instantiation (92.6% before
VERIFY-039 G17). Full
WP verification of the macro family will require a separate
`slice_verify.h` driver instantiating `DEFINE_SLICE(i32)` outside
the macro context — planned for a future commit if the certification
target requires it. (This is the same driver pattern option now uses —
see the semantics/option section below and VERIFY-014.)

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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 12               | (structural)      |
| Unreachable    | 15               | (structural)      |
| Qed (internal) | 292              | 0.6ms–11ms        |
| Alt-Ergo 2.6.3 | 54               | 16ms–73ms         |
| Z3 4.15.2      | 2                | 65ms              |
| Timeout        | 15               | >120s (see below) |
| **Total**      | **375 / 390**    |                    |

The 15 unproved goals belong to three documented categories (see
VERIFY-007); WP's discharge of the four MCDC-002 functions as
unreachable is verified separately by the wrapper's
`MCDC-002 functions with WP residuals: 0/4` diagnostic, which
confirms none of `bytes_slice`, `bytes_skip`, `str_slice`, or
`str_skip` appear in the unproved list.

CVC5 1.2.1 is invoked as a tertiary prover but closes none of the 15
unproved goals — they are demonstrated triple-prover-resistant.

### Timeout goals (15)

All 15 are documented as triple-prover-resistant. They fall into three
categories:

**memcmp call-site preconditions (12):** Four per `bytes_equal` and
`str_equal` (two `valid_*` on the bytes_t variant, two `danglingness_*`
on each), two per `str_starts_with` and `str_ends_with`.
Goal-name pattern:
`typed_cast_<func>_call_memcmp_requires_<aspect>` where `<aspect>`
ranges over `valid_s1`, `valid_s2`, `danglingness_s1`,
`danglingness_s2`. The `valid_*` obligations appear only on the
bytes_t variants because the str_t variants close them through
`str_valid` predicate reasoning; the `danglingness_*` obligations
appear on all four equality functions. The eight `initialization_*`
obligations formerly in this category were closed in VERIFY-012 by
stating `\initialized` as an explicit precondition on each equality
function — WP discharges initialization once the contract asserts it,
even though `\dangling` remains unimplemented.

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
yet implemented in Frama-C 29 for `\dangling`. (Initialization, by
contrast, WP *can* discharge once stated — see VERIFY-012; that is how
the eight `initialization_*` goals were closed.)

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

> **Correction (2026-10-20, VERIFY-039 batch 6, G13, G14 and G17).** The three
> categories' readings above do not hold as written. The `valid_*` goals were
> pinned at `str_equal` as at `bytes_equal`, and at both they were false at a
> zero length, where libc's `valid_read_or_empty` asks each pointer for a
> block and the contracts asked nothing (G17). The danglingness goals were not
> the verifier's alone: `\dangling` is a property of the bytes compared, which
> no contract required free of dangling addresses (G13). The strlen trade
> weighed a dependency every unit that verifies slice.h already takes, libc's
> contracts being checked at each call, and kept a call that the contract
> admitted and C does not (G17). The handler's pair was false, not unprovable
> by construction (G14). See the note under the summary for the repairs.

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
reads 54/58 (93.1%) since VERIFY-039 G17, whose zero-length returns in
`bytes_equal` and `str_equal` add four outcomes, all covered (50/54,
92.6%, on the post-Commit-9 surface before it; see the 2026-07-24
MCDC-002 note in docs/deviations.md), because gcov instruments the source
rather than the proof; the two evidence streams complement rather than
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

Expected output: `Proved goals: 390 / 398` with 8 unproved goals, the pin
since VERIFY-039 G14 and G17 (`379 / 394` with 15 before them).

---

## core/memory.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | b3e668b (Canon-C CI #841)                       |
| **Functions**          | 27 of 27 non-macro functions annotated          |
| **Proof obligations**  | 2829 / 2872 discharged automatically (98.50%) — 2823 / 2866 before VERIFY-023 |
| **Timeouts**           | 43 (all documented under VERIFY-008)            |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 2829/2872 with 43 named goals expected (2823/2866 before VERIFY-023; set unchanged) |
| **MC/DC coverage**     | 88.3% (113/128 condition outcomes)              |
| **CI artifact**        | `wp-proof-memory` (full per-goal breakdown)     |

> **Correction (2026-10-20, VERIFY-039).** The table and the sections below
> give the baseline's figures, brought up to date at VERIFY-023. Since a79b69d
> the job pins 2848 / 2879 with 31 unproved (first run CI #1369, held at
> CI #1373), each true for every admitted call. They are 14 inherited,
> checked.h's 2, ptr.h's 4 and slice.h's 8, and 17 own: three \fresh /
> \freeable goals, VERIFY-008's Category 1; six of integer theory,
> `mem_align`'s and `mem_align_to`'s four and `mem_get_alignment`'s two, its
> Category 2; six memcmp danglingness goals, its Category 3; and
> `mem_is_aligned`'s two, filed under VERIFY-039's block on a clause the code
> establishes and the specification omits. The inherited part moved with the
> substrate's repairs, G14's, G16's and G17's among them, and the own part as
> the notes on categories 2a, 2b and 2c below say; each step is in the job's
> pin comment.

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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 60               | (structural)      |
| Unreachable    | 63               | (structural)      |
| Qed (internal) | 2427             | 0.88ms–11ms       |
| Alt-Ergo 2.6.3 | 248              | 12ms–180ms        |
| CVC5 1.2.1     | 5                | 62ms–81ms         |
| Z3 4.15.2      | 16               | 14ms–45ms         |
| Timeout        | 40               | >120s             |
| Unknown        | 3                | (solver gave up)  |
| **Total**      | **2819 / 2862**  |                    |

The 43 unproved goals (40 `[Timeout]` + 3 `[Unknown]` + 0 `[Failed]`)
belong to seven categories — three memory.h-own and four inherited
from substrate headers. CVC5 contributes 5 closures; Z3 contributes
16; Alt-Ergo carries the bulk of the SMT load (248 goals) at typical
times in the tens of milliseconds.

The CI wrapper sums Timeout + Unknown + Failed because WP may
reclassify the same goal between Timeout and Unknown across runs
depending on solver heuristics — what matters is that the goal is not
proved, not which category it lands in.

### Timeout goals (43)

All 43 are documented as triple-prover-resistant. They fall into
seven categories: 4 inherited from already-verified substrate (23
goals total) and 3 memory.h-own (20 goals total).

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

**Inherited from VERIFY-007 categories 1–2 (13):**
- 12 memcmp call-site preconditions on slice.h's
  `bytes_equal`/`str_equal`/`str_starts_with`/`str_ends_with` —
  re-emerge here because memory.h includes slice.h transitively
  through bytes_t/cbytes_t. (Down from 20: the 8 `initialization_*`
  goals were closed upstream in VERIFY-012.)
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

> **Correction (2026-10-20, VERIFY-039 G18).** Two of the five were not a
> feature gap. `mem_alloc_assigns_normal_part2` and `mem_free_assigns_normal`
> are `mem_alloc`'s and `mem_free`'s frames, `assigns \nothing`, and libc's
> contracts let malloc and free change the heap's state, the ghost
> `__fc_heap_status`: each frame was false for a call that allocates or frees.
> WP implements frames, and the inclusion it checked was false. Since 806bdeb
> the wrappers state `allocates \result;` or `frees ptr;` and
> `assigns __fc_heap_status;`, as libc states them, and both frames are proved
> in every unit that includes memory.h. The other three are true and wait on
> WP's `\fresh` and `\freeable`, as above; VERIFY-008's Category 1 lists them.

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

> **Correction (2026-10-20, VERIFY-036 F1 and VERIFY-039).** Of the nine, one
> was false and closed at VERIFY-036 F1:
> `mem_get_alignment_assert_rte_signed_overflow`, an overflow of
> `-(intptr_t)addr` at an address with only its top bit set, `0x80000000` on a
> 32-bit target; the negation is unsigned since f1ba2de. `mem_is_aligned`'s
> two do not belong to integer theory: `ptr_is_aligned`'s `nonnull` behaviour
> states no result, so WP has no fact about the value `mem_is_aligned`
> returns. They are filed under VERIFY-039's block on a clause the code
> establishes and the specification omits, whose retirement is that clause.
> The six left are VERIFY-008's Category 2, whose argument states the mask
> identity and the lowest set bit's.

**memory.h-own category 2c — memcmp call-site danglingness (6):**
2 each on `mem_compare`, `mem_equal`, `mem_equal_bytes` —
`requires_danglingness_s1/s2`. (Down from 12: the
`requires_initialization_s1/s2` pair on each function was closed in
VERIFY-012 by an explicit `\initialized` precondition. WP discharges
initialization once the contract states it; `\dangling` remains
unimplemented.)

Identical root cause to VERIFY-007's danglingness residual. memory.h
directly calls `memcmp` from these three functions, so the same
residual class re-emerges at memory.h's call sites.

> **Correction (2026-10-20, VERIFY-039 G13).** These six, and slice.h's eight,
> were false for calls the contracts admitted: `\dangling` is a property of
> the bytes compared, not of the pointer passed, and comparing the bytes of a
> pointer to a freed block met every precondition of the three functions and
> made memcmp's false. Since 7624d8c each function requires, on its path to
> memcmp, that no byte it compares hold a dangling address. The goals hold for
> every admitted call and wait on WP's `\dangling`, which WP 29 does not
> implement. VERIFY-008's Category 3 lists the six and files slice.h's eight
> and borrow.h's four by name.

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
new ones. Final count (at the round-3 baseline): 57 residuals,
byte-identical inherited count (31) between rounds 2 and 3. (VERIFY-012
later lowered this to 43 = 23 inherited + 20 own, by closing the
initialization sub-class; that closure is a contract addition, not a
contract-shape fix, and likewise left the substrate fingerprint intact.)

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

Expected output: `Proved goals: 2842 / 2873` with 31 unproved goals, the pin
since the VERIFY-008 overlap fix (predicted) (`2848 / 2879` since VERIFY-039
G18, `2829 / 2872` with 43 at VERIFY-023, `2823 / 2866` before it).
(The Timeout/Unknown split varies run to run; only the union is pinned.)

---

## core/arena.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | f53bddb (Canon-C CI #962)                       |
| **Functions**          | 22 of 22 non-macro functions annotated          |
| **Proof obligations**  | 3444 / 3527 discharged automatically (97.65%) — 3430 / 3521 before VERIFY-023 |
| **Timeouts**           | 89 (all documented under VERIFY-009)            |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 3444/3527 with 83 named goals expected (3430/3521 and 91 before VERIFY-023 closed 8 misattributed residuals; see the reading note in VERIFY-009) |
| **MC/DC coverage**     | 89.4% (59/66 condition outcomes — see MCDC-003, 2026-07-30 note) |
| **Line coverage**      | 100% (97/97)                                    |
| **CI artifact**        | `wp-proof-arena` (full per-goal breakdown)      |

> **Note (2026-10-20, VERIFY-039).** The table and the sections below give the
> baseline's figures and categories, brought up to date at VERIFY-023. Since
> a79b69d the job pins 3523 / 3556 with 33 unproved (first run CI #1369, held
> at CI #1373): memory.h's 31, inherited, and 2 own, `arena_alloc_zero`'s and
> `arena_alloc_aligned_zero`'s zero-content postconditions. Those two are
> true, filed by name under VERIFY-017's byte-view block: `mem_zero` states
> the bytes it zeroes through `char *`, the postconditions read them through
> `u8 *`, and WP's typed model keeps the two in separate arrays. The four own
> categories below are retired, 2d at VERIFY-023, 2a and 2b at VERIFY-036 F3
> and F4, and 2c at VERIFY-039's batch 5; VERIFY-009's notes say how each
> closed, and the job's pin comment gives each step.

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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 69               | (structural)      |
| Unreachable    | 72               | (structural)      |
| Qed (internal) | 2889             | 0.88ms–14ms       |
| Alt-Ergo 2.6.3 | 327              | 12ms–220ms        |
| CVC5 1.2.1     | 3                | 65ms–110ms        |
| Z3 4.15.2      | 23               | 14ms–62ms         |
| Timeout        | 83               | >120s             |
| Unknown        | 6                | (solver gave up)  |
| **Total**      | **3383 / 3472**  |                    |

The 89 unproved goals (83 `[Timeout]` + 6 `[Unknown]` + 0 `[Failed]`)
belong to eight categories — four arena.h-own (46 goals total) and
four inherited groups (43 goals total). The exact category and prover
breakdowns above reflect the typical distribution across CI runs; WP
may reclassify Timeout/Unknown goals between runs depending on solver
heuristics.

The CI wrapper sums Timeout + Unknown + Failed because what matters
is that the goal is not proved, not which category it lands in. The
named-goal enforcement (per-pattern check in the wrapper) catches
both count regressions and silent renames.

### Timeout goals (89)

All 89 are documented as triple-prover-resistant. The 43 inherited
goals are byte-identical to memory.h's full residual surface
(VERIFY-008's 23 inherited from substrate + 20 memory.h-own).
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
plus 2 release-build macro no-op artifacts on `arena_debug_update_`
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

Expected output: `Proved goals: 3517 / 3550` with 33 unproved goals, the pin
since the VERIFY-008 overlap fix (predicted) (`3523 / 3556` since VERIFY-039
G18, `3444 / 3527` with 83 at VERIFY-023, `3430 / 3521` and 91 before it).
(The Timeout/Unknown split varies run to run; only the union is pinned.)
The 43 inherited goals are byte-identical
to memory.h's full residual surface (see VERIFY-009 inherited
residuals table); the 48 own goals split across cats 2a (8) / 2b (26)
/ 2c (10) / 2d (2).

---

## core/pool.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | b2644ba (Canon-C CI #972)                       |
| **Functions**          | 19 of 19 non-macro functions annotated          |
| **Proof obligations**  | 3914 / 4009 discharged automatically (97.63%) — 3884 / 4003 before VERIFY-023 |
| **Timeouts**           | 113 (all documented under VERIFY-010)           |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 3914/4009 with 95 named goals expected (3884/4003 and 119 before VERIFY-023 closed 24 misattributed residuals; see the reading note in VERIFY-010) |
| **MC/DC coverage**     | 87.2% (68/78 condition outcomes — see MCDC-004, 2026-07-30 note) |
| **Line coverage**      | 100% (74/74)                                    |
| **CI artifact**        | `wp-proof-pool` (full per-goal breakdown)       |

> **Correction (2026-10-20, VERIFY-039).** The table and the sections below
> give the baseline's figures and categories, brought up to date at
> VERIFY-023. Since a79b69d the job pins 4042 / 4076 with 34 unproved (first
> run CI #1369, held at CI #1373): arena.h's 33, inherited, and 1 own,
> `pool_init_ensures_part4`, VERIFY-010's Category 2a, whose argument covers
> every conjunct of `pool_invariant`, the division bound
> `capacity <= CANON_USIZE_MAX / object_size` among them. Four of 2a's five
> closed at G5 (528bfa1), which separated a `Pool` from its `Arena`: they
> asked that a field survive stores into the other struct, not arithmetic.
> Categories 2b, 2c and 2d are retired, at VERIFY-036 F2 and F6 and at
> VERIFY-039 G7; three of 2d's four closed with G5's separation, and its last,
> false for an admitted call, at G7. Since G18 (806bdeb) `pool_alloc_zero`,
> `pool_try_alloc` and `pool_try_alloc_zero` state `pool_alloc`'s behaviours,
> the zero variants the slot's bytes, and the 42 goals they add are proved.
> The job's pin comment gives each step.

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
  ptr_elem uintptr_t round-trip). The runtime `require_msg` region check is
  NOT a backstop for them: it is compiled out by `-DCANON_NO_REQUIRE`, the
  configuration WP runs under, and it sits in `pool_get` rather than in the
  function carrying the goal. See the corrected argument in VERIFY-010
  cat 2b.

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

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 77               | (structural)      |
| Unreachable    | 80               | (structural)      |
| Qed (internal) | 3201             | 0.67ms–63ms       |
| Alt-Ergo 2.6.3 | 395              | 16ms–908ms        |
| CVC5 1.2.1     | 10               | 81ms              |
| Z3 4.15.2      | 26               | 13ms–1s           |
| Timeout        | 110              | >120s             |
| Unknown        | 3                | (solver gave up)  |
| **Total**      | **3789 / 3902**  |                   |

The proved total breaks down as 77 Terminating + 80 Unreachable (structural)
+ 3201 Qed + 395 Alt-Ergo + 10 CVC5 + 26 Z3 = 3789. CVC5's 10 closures confirm
the triple-prover claim holds for pool.h: those goals were closed by neither
Alt-Ergo nor Z3. The full per-goal breakdown is recorded in the `wp-proof-pool`
CI artifact.

The CI wrapper sums Timeout + Unknown + Failed (110 + 3 + 0 = 113) because WP
may reclassify the same goal between Timeout and Unknown across runs depending
on solver heuristics; what matters is that the goal is not proved.

### Timeout goals (113)

All 113 are documented as triple-prover-resistant. They split into 89
inherited from substrate and 24 pool.h-own.

The **89 inherited** goals are byte-identical to arena.h's full residual
surface (VERIFY-009's 43 inherited from substrate + 46 arena.h-own). pool.h is
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

Expected output: `Proved goals: 4036 / 4070` with 34 unproved goals, the pin
since the VERIFY-008 overlap fix (predicted) (`4042 / 4076` since VERIFY-039
G18, `3914 / 4009` with 95 at VERIFY-023, `3884 / 4003` and 119 before it).
(The Timeout/Unknown split varies run to run; only the union is pinned.)
pool.h is the first header whose WP run processes a
two-hop transitive include (pool.h → arena.h → memory.h → ptr/slice/checked/
contract); the 4003 reflects the full translation unit. The 91 inherited
goals are byte-identical to arena.h's full residual surface (see VERIFY-010's
inherited-residuals table); the 24 own goals split across cats 2a (5) / 2b (6)
/ 2c (5) / 2d (8).

---

## core/region.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented timeouts)             |
| **Baseline commit**    | c9172fc (Canon-C CI #992)                       |
| **Proof obligations**  | 3592 / 3698 discharged automatically (97.13%) — 3578 / 3692 before VERIFY-023 |
| **Unproved**           | 112 (all documented under VERIFY-011)           |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-model Typed+Cast -wp-split -wp-timeout 120` |
| **CI enforcement**     | Yes — 3592/3698 with 106 named goals expected (3578/3692 and 114 before VERIFY-023; the 8 closed are arena's, inherited) |
| **MC/DC coverage**     | 95.5% (21/22 condition outcomes — see MCDC-005) |
| **Line coverage**      | 100% (45/45)                                    |
| **CI artifact**        | `wp-proof-region` (full per-goal breakdown)     |

> **Correction (2026-10-20, VERIFY-039 G19 and G20).** The table and the
> sections below give the baseline's figures and readings, brought up to date
> at VERIFY-023. Since 9d35881 the job pins 3691 / 3730 with 39 unproved, each
> named in its roll-call (first run CI #1372, held at CI #1373, the job in
> 36m 34s): arena.h's 33, inherited, and 6 own, each true for every admitted
> call. Of the 23 own below, 17 closed at G19's and G20's repair (642ad8a),
> and fourteen of those had not held for every admitted call.
>
> - Category 1: `region_end`'s frame omitted the `Arena` it resets, and its
>   loop's frame the store to `h`, each false for every call of a kind; and
>   eleven goals claimed, for every hook, what only some hooks give:
>   termination, no exit, the hook's writes within the frame, a valid function
>   pointer, and facts the code needs after a hook that one which loops,
>   exits, writes another object or frees the `Region`'s storage breaks.
> - Category 2: nothing separated a `Region` from its `Arena`, so three
>   mutators' stores could reach the `Arena` and falsify the invariant.
>
> Since 642ad8a `region_invariant` separates the two, and
> `region_attach_arena` requires it. `region_end` claims termination, no exit
> and its frame, which names the saved `Arena`, only where no hook is called,
> and requires each hook below the count to be a valid function and to keep
> `region_hook_rule`, a predicate with no definition whose meaning is its
> comment in `core/region.h`: a hook leaves the `Region`'s storage in place
> and the attached `Arena` meeting `arena_invariant`, leaves the slots below
> its own as they were or with a null `fn`, and does not call `region_end` on
> its `Region`. The six left are filed by name: the check that a hook is a
> valid function and the slot's two stores after a hook, which no verifier
> proves without the rule, under VERIFY-039's block on what a hook keeps,
> specification-bound; the loop's first invariant and its variant after a
> hook, which read `i`, which WP loses across the call, VERIFY-011's
> Category 1; and `region_begin`'s invariant after its store of a compound
> literal, its Category 2. Ten goals WP proves read, after a hook, storage a
> hook could reach, and are proved on WP's assumption that the hook's writes
> and frees do not reach it; the rule and the separation are what keep them
> off, as VERIFY-039's proved-goal block on the goals a hook's writes could
> reach files them.

### Enforcement status

region.h's WP run is **enforced** (as of CI #1022): the `frama-c-region`
CI step fails the build on any deviation from 3592/3698 proved or 106 (3578/3692 and 114 before VERIFY-023)
unproved, and additionally roll-calls all 23 own goals plus a
representative inherited sample by name. It was promoted from report-only
once the residual set proved name-stable across the VERIFY-012
initialization closures: those 14 goals left the inherited surface (126 →
112) without disturbing the 23 region-own goals, and the 19 region_end
`-wp-split` fragments near the 120s boundary all reproduce by name. The
job runs ~1h40m in its own isolated runner with `timeout-minutes: 350`
headroom. See VERIFY-011 for the full rationale.

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

> **Correction (2026-10-20, VERIFY-039 G19 and G20).** Since 642ad8a
> `region_end` carries five of the six own residuals and `region_begin` one.
> `region_attach_arena`, `region_register` and `region_set_parent` are proved,
> their invariants from G20's separation: ten of the twelve functions carry
> none.

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

> **Correction (2026-10-20, VERIFY-039 G19 and G20).** The category 2 goals
> were not the weight of composing `arena_invariant`: nothing separated a
> `Region` from its `Arena`, so a mutator's store could reach the `Arena` and
> the invariant be false after it. `region_end`'s `assigns *r` was false for
> every call with an `Arena` attached that does not lie wholly within the
> `Region`, as `arena_reset` writes it, and the writes after the loop
> re-establish the postconditions only for a hook that leaves the `Region`'s
> storage in place, which no clause required. See the note under the summary
> for what `region_end` states since 642ad8a; its run-time checks are proved
> but the check that a hook is a valid function and the slot's two stores
> after a hook.

### Prover breakdown

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 80               | (structural)      |
| Unreachable    | 83               | (structural)      |
| Qed (internal) | 2988             | 0.62ms–35ms       |
| Alt-Ergo 2.6.3 | 352              | 16ms–295ms        |
| CVC5 1.2.1     | 9                | 74ms–88ms         |
| Z3 4.15.2      | 19               | 13ms–61ms         |
| Timeout        | 107              | >120s             |
| Unknown        | 5                | (solver gave up)  |
| **Total**      | **3531 / 3643**  |                   |

CVC5's 9 closures confirm the triple-prover claim holds for region.h.
The CI wrapper sums Timeout + Unknown + Failed (107 + 5 + 0 = 112).

### Unproved goals (112)

89 inherited from substrate (byte-identical to arena.h's full residual
surface) + 23 region.h-own in two categories: 19 region_end
opaque-hook-dispatch residuals (category 1) and 4 region_invariant
re-establishment residuals on the trivial mutators (category 2). Full
goal list, inheritance table, manual proof arguments, and the WP warning
text quoted as evidence: see VERIFY-011 in `docs/deviations.md`. The
architectural decision behind category 1 is OWN-003.

> **Correction (2026-10-20, VERIFY-039 G19 and G20).** 39 since 642ad8a:
> arena.h's 33 and 6 own, two of them in category 1, one in category 2 and
> three under VERIFY-039's block on what a hook keeps. See the note under the
> summary.

### MCDC-005 cross-reference

region.h's single uncovered condition outcome is the `if (h->fn != NULL)` FALSE
branch in region_end (line 496), API-unreachable because
region_register enforces `fn != NULL` and the dispatch loop visits only
filled slots. 95.5% (21/22) is the achievable ceiling. See MCDC-005.

> **Correction (2026-10-20, VERIFY-039 G20).** The false side is not
> unreachable through the API. No clause states that every slot below the
> count holds a hook, and a hook may leave a slot below its own with a null
> `fn`, as one that re-opens its own `Region` with `region_begin` does; calls
> that each meet their contracts reach the guard's false side by either route
> (`tools/probes/verify-039/p_region_arena.c`, its fifth and sixth cases), and
> both stay within the contracts since 642ad8a. No goal states the outcome
> dead. 21/22 is the tests' ceiling, not the API's.

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

Expected output: `Proved goals: 3685 / 3724` with 39 unproved goals, the pin
since the VERIFY-008 overlap fix (predicted) (`3691 / 3730` since VERIFY-039
G19 and G20, `3592 / 3698` with 106 at VERIFY-023, `3578 / 3692` and 114
before it).
(The Timeout/Unknown split varies run to run; only the union is pinned.)
region.h's WP run processes a two-hop
transitive include (region.h → arena.h → memory.h → ...); the 91
inherited goals are byte-identical to arena.h's full residual surface;
the 23 own goals split across category 1 (19) / category 2 (4).

---


## semantics/error.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Fully verified — 100% automatic                |
| **Baseline commit**    | ca9732d (Canon-C CI #1036)                      |
| **Functions**          | 4 of 4 annotated and proved                    |
| **Proof obligations**  | 65 / 65 discharged automatically (100%)         |
| **Residuals**          | 0                                               |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120` (default Typed model) |
| **CI enforcement**     | Yes — 65/65 required, 0 unproved allowed        |
| **MC/DC coverage**     | 100% (trivial — no condition outcomes to miss)  |
| **CI artifact**        | `wp-proof-error` (full per-goal breakdown)      |

error.h is the first verified header of the semantics/ layer and the
first header since compare.h (VERIFY-005) to prove fully automatically
with zero residuals. It is also the first verified header to use the
default `Typed` memory model rather than `Typed+Cast`: error.h contains
no `void*` casts, so the cast-aware model is unnecessary.

### Function inventory

error.h provides a flat `Error` enum plus four pure functions:

- `error_message` — switch-based lookup returning a static string
  literal; never NULL.
- `error_is_ok` — true iff `e == ERR_OK`.
- `error_in_range` — true iff `(int)e` is in `[ERR_OK, ERR_COUNT)`.
- `error_code` — returns `(int)e`.

All four are `static inline`, take an `Error` by value, and carry
`assigns \nothing`. There are no macro-templated surfaces, so the whole
public API is directly WP-verified (no documentation-only macro family).

### What is proved

- **Never-NULL message lookup**: `error_message` carries
  `ensures \result != \null;`. The `switch` is total — every named case
  plus the `default:` arm returns a string literal — so WP proves the
  postcondition for every input including gap values and out-of-range
  casts (e.g. `(Error)(-1)`, `(Error)9999`).

- **Exact-value predicate postconditions**: `error_is_ok` proves
  `\result <==> (e == ERR_OK)`; `error_in_range` proves
  `\result <==> ((int)e >= (int)ERR_OK && (int)e < (int)ERR_COUNT)`;
  `error_code` proves `\result == (int)e`. The casts in the ACSL mirror
  the casts in the C bodies, so WP reasons about the int-backed enum
  directly without an enum/representation bridge.

- **Absence of runtime errors** (`-wp-rte`): proved for all four
  functions.

- **Side-effect bounding**: all four specify `assigns \nothing;`.

### Spec-strength note (error_message)

`error_message`'s postcondition is the weak `\result != \null`, which is
exactly what the public contract guarantees ("Never returns NULL"). It
does **not** assert `valid_read_string(\result)`. Asserting full string
validity would cross the same libc `strlen`/`valid_string` reasoning
boundary documented in VERIFY-007 category 2 (`str_from_cstr`'s omitted
`valid_string` precondition). Keeping the spec weak holds error.h at 0
residuals while still proving the property callers depend on.

### Prover breakdown

*Prover attribution below is a snapshot of the CI #1154 run and is not
ratcheted: it moves run to run even when the residual set does not
(VERIFY-009-W records Alt-Ergo 325 -> 347 and Z3 29 -> 12 across three
runs with an identical 91-name set). The pinned quantities are the
proved-goal line and the residual set, given above.*


| Category       | Goals discharged | Typical time      |
|----------------|------------------|--------------------|
| Terminating    | 4                | (structural)      |
| Unreachable    | 4                | (structural)      |
| Qed (internal) | 35               | 0.5ms-4ms         |
| Alt-Ergo 2.6.3 | 22               | 25ms-43ms         |
| **Total**      | **65 / 65**      |                    |

Z3 4.15.2 and CVC5 1.2.1 are available in the prover chain but are not
reached — every goal closes at Qed or Alt-Ergo. This is expected for a
header this simple and is not a triple-prover degradation; the error.h
CI wrapper omits the CVC5-presence check that the `Typed+Cast` headers
carry, precisely because error.h's goals close before the heavier
provers are invoked. The four `Terminating` and four `Unreachable`
goals are WP's structural proofs (one termination proof per function;
the dead-code-after-`return` obligations the `switch` generates,
including the `default:` and explicit `case ERR_COUNT:` arms).

### Advisory — pedantic-assigns

WP emits one advisory on `error_message`:

```
[wp:pedantic-assigns] semantics/error.h: Warning:
  No 'assigns \result \from ...' specification for function
  'error_message' returning pointer type.
```

This is advisory, **not a residual** — the header proves 65/65 with the
warning present. It is the same WP pedantry about pointer-returning
functions already noted for ptr.h and slice.h (`bytes_at`, the align
functions): `assigns \nothing` does not specify the provenance (`\from`)
of the returned pointer. Consistent with that precedent, the advisory is
accepted as-is and not silenced.

### MC/DC note

error.h's MC/DC is trivially 100%: the three predicate functions are
single-expression, and `error_message`'s switch is fully exercised by
`test/semantics/error_test.c` (every named case, ERR_COUNT, gap values,
and out-of-range casts). There are no defensive or
type-invariant-unreachable branches, so no MCDC-NNN entry is warranted —
error.h is the header that demonstrates the MC/DC machinery correctly
produces "nothing to document" when nothing is unreachable.

### Composable-verification note

error.h includes only `core/primitives/types.h` (type definitions only,
no proof obligations of its own), so it inherits zero residuals. This is
the trivial base case of the composability thesis at the bottom of a new
layer — there is nothing upstream in semantics/ yet to inherit from. It
establishes the layer's zero baseline against which the next semantics/
headers (option, then Result, borrow, diag) are measured for
inherited-vs-own residuals. option (the next module, VERIFY-014) inherits
exactly 2 residuals from contract.h and adds 32 of its own — the first
non-trivial data point in the semantics/ layer. (So when enforced; since
VERIFY-039 G11 and G14 option inherits none and pins 6 of its own, the
run-time checks on its callbacks' pointers.)

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: consistent with the other verified headers.
  error.h itself contains no `require_msg` calls, so the flag is inert
  here, but it is passed for CI uniformity.
- **`-DNDEBUG`**: standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I semantics \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  semantics/error.h
```

Expected output: `Proved goals: 65 / 65` with 0 unproved goals.


---

## semantics/option (driver-verified)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals) — driver-verified |
| **Baseline commit**    | e2d908d (Canon-C CI #1067, enforced); report-only first at 83cfb16 (CI #1065) |
| **Functions**          | 16 of 16 generated functions annotated and proved |
| **Proof obligations**  | 189 / 223 discharged automatically (84.75%)     |
| **Unproved**           | 34 (2 inherited + 32 own; all documented under VERIFY-014) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120` (default Typed model) |
| **CI enforcement**     | Yes — 189/223 with 34 named goals expected (enforced as of CI #1067) |
| **MC/DC coverage**     | 96.7% (29/30 condition outcomes — see MCDC-006) |
| **CI artifact**        | `wp-proof-option` (full per-goal breakdown)     |

> **Correction (2026-10-20, VERIFY-039 G11 and G14).** The table and the
> sections below give the figures and readings of enforcement. Since 767a151
> the job pins 222 / 228 with 6 unproved (first run CI #1368, held at
> CI #1373); from G11's repair of the driver (a4f8f5f, CI #1361) it pinned
> 222 / 230 with 8. The 32 own residuals were not a verifier feature gap. The
> combinators' contracts required nothing of the callback's pointer, and
> claimed, by an empty frame and by WP 29's default clauses, that every call
> terminates, does not exit and writes nothing; a callback that is not a valid
> function, loops, exits or writes falsifies each claim
> (`tools/probes/verify-039/p_combinator_callback.c`), and no verifier proves
> a claim that an admitted call falsifies. Since a4f8f5f each combinator
> requires a valid function where it calls one, and claims termination, no
> exit and an empty frame only where it calls none: 26 goals closed, and 7
> were added, all proved. The six left are the run-time checks that the
> pointer is a valid function, true for every admitted call and unproved
> because WP 29 does not implement `\valid_function`, filed by name under
> VERIFY-014's block. The inherited handler pair was false as written and
> closed at G14 (c9ac146).

option is the **first driver-verified** Canon-C module and the first
**Shape-B** (expression-macro-bodied) module verified. Its functions
exist only as `IMPL_OPTION_*` macro bodies until instantiated, so WP is
pointed not at a shipped header but at
`vmacros/vdrivers/option_verify.h`, which instantiates the real shipped
macros at `CANON_OPTION(int)` and attaches ACSL contracts to the concrete
generated prototypes (see `docs/vmacros.md` for the driver mechanism). It
is the second semantics/ module verified (after error.h, VERIFY-013) and
the first semantics/ module to carry residuals — error.h proved 65/65
clean.

Like error.h, option uses the default `Typed` memory model rather than
`Typed+Cast`: the by-value tagged struct `{ bool has_value; T value; }`
performs no `void*` casts, so the cast-aware model is unnecessary. The
option CI wrapper omits the CVC5-presence check the `Typed+Cast` headers
carry, for the same reason error.h's does.

### Function inventory

`CANON_OPTION(int)` generates 16 `option_int_*` functions, all annotated
and verified:

**Non-combinator (10 functions, all 100% proved):** `some`, `none`,
`is_some`, `is_none`, `get`, `unwrap`, `unwrap_or`, `expect`, `replace`,
`take`. These carry no residuals — their structural postconditions (the
result's `has_value` / `value` relationship to the inputs) discharge
fully.

**Combinators (6 functions, carry all 32 own residuals):** `map`,
`and_then`, `filter`, `combine_with` (6 residuals each = 24); `or_else`,
`eq` (4 residuals each = 8). Each dispatches a caller-supplied function
pointer.

### What is proved

- **Structural correctness**: every function's result `has_value` /
  `value` relationship to its inputs is proved for all inputs consistent
  with the preconditions, including for the combinators (the structural
  postconditions discharge; only the opaque-callee obligations remain —
  see below).
- **Absence of runtime errors** (`-wp-rte`): proved for all
  non-combinator functions and for the non-opaque-call surface of the
  combinators.
- **Side-effect bounding**: `assigns` clauses discharge fully except on
  the combinators, where the opaque callee's frame cannot be bounded
  (the function-pointer-dispatch `assigns` residuals below).

WP discharges 189 of the 223 obligations automatically — the majority at
Qed, the remainder at Alt-Ergo. As with error.h, this is a small
default-`Typed` run and Z3/CVC5 are available but not reached; the 34
residuals are feature-gap goals (see below), not goals the heavier
provers would close. The full per-goal breakdown is in the
`wp-proof-option` CI artifact.

### Residual goals (34)

All 34 are documented under VERIFY-014. They split into 2 inherited and
32 own.

**Inherited from contract.h (2):**
`typed_contract_default_handler_terminates`,
`typed_contract_default_handler_loop_invariant_established` — option is
the first semantics/ module to inherit the contract.h handler
non-termination pair (VERIFY-006 cat 4), reached through `expect`'s
`CANON_INVOKE_HANDLER_` path. Note the goal-name prefix is
`typed_contract_default_handler_*` (the non-`_cast` form), because option
runs under the default `Typed` model — the same two goals as VERIFY-006
category 4, re-emitted in the option run under the Typed prefix.

**option-own — combinator function-pointer dispatch (32):** each of the
six combinators dispatches a caller-supplied function pointer (`f`,
`pred`, `combine`, `fallback`, `eq`) for which no `calls` clause can
exist — the callee is arbitrary by design. WP reports the boundary
during the run:

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
opaque-hook dispatch (VERIFY-011 category 1 / OWN-003) — different
surface (six combinators vs one teardown loop), identical limitation: no
`calls` clause for an arbitrary callee, plus `\valid_function`
unimplemented in Frama-C 29. It is a verifier feature gap, not a
prover-strength residual — additional solver power would not close it.

> **Correction (2026-10-20, VERIFY-039 G11, G14 and G19).** The reading above
> does not hold. What failed was the contracts' claims, not the verifier: a
> contract that claims termination, no exit and an empty frame for every
> callback is false for a callback that loops, exits or writes, and its
> `\valid_function` check for a pointer that is not a valid function, whatever
> a verifier implements. `region_end`'s hooks were the same class, and G19
> repaired them alike. The handler's pair was false too, its invariant
> `\false` and its termination left to the default (G14). The note under the
> summary gives the repair and the six goals left.

Full goal list and per-combinator manual-proof arguments: see VERIFY-014
in `docs/deviations.md`.

### MCDC-006 cross-reference

option's single uncovered MC/DC outcome — `option_int_expect`'s
`has_value` FALSE (panic-on-absent) branch — is cross-confirmed by this
run: WP reports `Missing decreases clause on recursive function
option_int_expect, call must be unreachable`, modelling the panic
handler's call as a non-terminating recursive call it treats as
unreachable. gcov measures the same branch as not-executed under
`-DCANON_NO_REQUIRE`. 96.7% (29/30) is the achievable ceiling; the
condition set is measured through `vmacros/coverage/option_cover.c` (the
paired cover TU, see MCDC-006), which re-instantiates the same
`CANON_OPTION(int)` outside `test/` so the generated conditions survive
the coverage filter. WP and gcov point at the same branch from opposite
directions — proof unreachability and source non-execution.

### Driver mechanism note

option is the first module where WP runs over a *driver* rather than a
shipped header. The driver `vmacros/vdrivers/option_verify.h` does three
things: (1) `#include`s the shipped `semantics/option/option.h`; (2)
instantiates `CANON_OPTION(int)` so the `IMPL_OPTION_*` macro bodies
expand into concrete `option_int_*` function definitions with real source
locations; (3) attaches ACSL contracts to those concrete prototypes. WP
then verifies the instantiated functions exactly as it would
hand-written ones. This is the same pattern slice.h's section anticipates
for a future `slice_verify.h` (DEFINE_SLICE(i32) outside macro context),
now realized for the first time — and since followed by result (see the
semantics/result section). The remaining Shape-B modules (vec, deque,
fold) will each get their own `*_verify.h` driver following this
template; see `docs/vmacros.md`.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: compiles `require_msg` runtime checks away;
  ACSL `requires` clauses provide the static guarantees. (The `expect`
  panic path, removed by this flag, is the MCDC-006 uncovered branch.)
- **`-DNDEBUG`**: standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I semantics \
    -I vmacros \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  vmacros/vdrivers/option_verify.h
```

Expected output: `Proved goals: 222 / 228` with 6 unproved goals, the pin
since VERIFY-039 G14: the run-time checks that a callback is a valid
function (`189 / 223` with 34 when enforced: the 2 contract.h handler
goals + 32 combinator function-pointer-dispatch goals).

---

## semantics/result (driver-verified)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals) — driver-verified |
| **Baseline commit**    | 6516ae5 (Canon-C CI #1090, enforced); report-only first at b528515 (CI #1089) |
| **Functions**          | 17 of 17 generated functions annotated and proved |
| **Proof obligations**  | 185 / 215 discharged automatically (86.05%)     |
| **Unproved**           | 30 (2 inherited + 28 own; all documented under VERIFY-015) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120` (default Typed model) |
| **CI enforcement**     | Yes — 185/215 with 30 named goals expected (enforced as of CI #1090) |
| **MC/DC coverage**     | 100% (28/28 condition outcomes — see MCDC-007) |
| **CI artifact**        | `wp-proof-result` (full per-goal breakdown)     |

> **Correction (2026-10-20, VERIFY-039 G11 and G14).** The table and the
> sections below give the figures and readings of enforcement. Since 767a151
> the job pins 207 / 213 with 6 unproved (first run CI #1368, held at
> CI #1373); from G11's repair of the driver (068ae6a, CI #1361) it pinned
> 207 / 215 with 8. The 28 own residuals were option's case: the dispatching
> combinators claimed termination, no exit and an empty frame for every
> callback, and required nothing of the pointers, so a callback that loops,
> exits or writes, or a pointer that is not a valid function, falsified them.
> Since 068ae6a each requires a valid function where it calls one and makes
> the three claims only where it calls none: 22 goals closed. The six left are
> the run-time checks on the pointers, `eq`'s two among them, true for every
> admitted call and unproved because WP 29 does not implement
> `\valid_function`, filed by name under VERIFY-015's block. The handler's
> pair closed at G14 (c9ac146).

result is the **second driver-verified** Canon-C module and the second
Shape-B module verified. WP is pointed at
`vmacros/vdrivers/result_verify.h`, which instantiates the real shipped
macros at `CANON_RESULT(int, VErr)` — `VErr` a driver-local enum,
deliberately a distinct type from `int` so the payload union genuinely
carries two member types — and attaches ACSL contracts to the concrete
generated prototypes (see `docs/vmacros.md`). It is also the **first
union-typed module** verified: result's payload is
`union { T ok; E err; } val`, where option's is a plain struct field.
All union-member postconditions proved, subject to the union-model
standing hypothesis documented in VERIFY-015 (WP flags its union-field
reasoning as potentially unsound under type punning; the generated code
never puns — every union read occurs under the `is_ok` value that
selected the write).

Like error.h and option, result uses the default `Typed` memory model
rather than `Typed+Cast`: the by-value tagged struct performs no `void*`
casts. The result CI wrapper omits the CVC5-presence check the
`Typed+Cast` headers carry, for the same reason.

### Function inventory

`CANON_RESULT(int, VErr)` generates 17 `result_int_VErr_*` functions,
all annotated and verified:

**Non-dispatching (12 functions, all 100% proved):** `ok`, `err`,
`is_ok`, `is_err`, `get_ok`, `get_err`, `unwrap_or`, `unwrap`,
`unwrap_err`, `expect`, `and`, `or`. These carry no residuals — their
structural postconditions discharge fully, including `and`/`or`'s
cross-value union-member implications and the eager `and`/`or`
combinators (no function pointers).

**Dispatching combinators (5 functions, carry all 28 own residuals):**
`map`, `map_err` (6 residuals each = 12); `and_then`, `or_else` (4 each
= 8); `eq` (8 — two function pointers, `eq_ok` and `eq_err`, across two
calling branches). Each dispatches at least one caller-supplied function
pointer.

### What is proved

- **Structural correctness**: every function's result `is_ok` /
  active-member relationship to its inputs is proved for all inputs
  consistent with the preconditions — including the union-member
  postconditions on the constructors and passthrough branches, and
  `and`/`or`'s cross-value member equalities. On the dispatching
  combinators the non-calling branch is fully proved; only the
  opaque-callee obligations remain (below).
- **Absence of runtime errors** (`-wp-rte`): proved for all
  non-dispatching functions and for the non-opaque-call surface of the
  combinators.
- **Side-effect bounding**: `assigns` clauses discharge fully except on
  the dispatching combinators, where the opaque callee's frame cannot
  be bounded.
- **Panic-surface note**: `unwrap`, `unwrap_err`, and `expect` prove as
  straight-line functions under `-DCANON_NO_REQUIRE` — result's panic
  surface is `require_msg`-only, so unlike option's `expect` no result
  function contains a handler call in the verified configuration; the
  2 inherited handler goals are definition-presence inheritance only
  (VERIFY-015).

WP discharges 185 of the 215 obligations automatically — 138 at Qed, 15
at Alt-Ergo, plus 13 Terminating and 19 Unreachable. As with error.h and
option, this is a small default-`Typed` run; the 30 residuals are
feature-gap goals, not goals the heavier provers would close (each
resolves at Qed in 2–6ms then buckets). The full per-goal breakdown is
in the `wp-proof-result` CI artifact.

### Residual goals (30)

All 30 are documented under VERIFY-015. They split into 2 inherited and
28 own.

**Inherited from contract.h (2):**
`typed_contract_default_handler_terminates`,
`typed_contract_default_handler_loop_invariant_established` — the
VERIFY-006 category 4 pair, re-emitted under the Typed prefix. Unlike
option (which reaches the handler through `expect`'s
`CANON_INVOKE_HANDLER_`), result's `require_msg`-only panic surface is
fully compiled out under `-DCANON_NO_REQUIRE`, so these 2 goals arise
purely from the handler's definition being present in the TU; no result
function calls it.

**result-own — combinator function-pointer dispatch (28):** the same
verifier feature gap as option's 32 (VERIFY-014) and region_end's
opaque-hook dispatch (VERIFY-011 category 1 / OWN-003): no `calls`
clause for an arbitrary callee, `\valid_function` unimplemented in
Frama-C 29. Cluster sizes are structural — `map`/`map_err` 6 each
(calling branch rewraps through the known constructor), `and_then`/
`or_else` 4 each (return the callee's Result directly), `eq` 8 (one
`assert_rte_function_pointer` goal per pointer across its two calling
branches). Full goal list, split-numbering notes, and per-combinator
manual-proof arguments: see VERIFY-015 in `docs/deviations.md`.

> **Correction (2026-10-20, VERIFY-039 G11 and G14).** Not a verifier feature
> gap: the contracts' claims for every callback were false for some callbacks
> they admitted, as in option's section, and the handler's pair was false as
> written. See the note under the summary.

### MCDC-007 cross-reference

result's MC/DC audit is **clean**: 28/28 condition outcomes covered,
measured through `vmacros/coverage/result_cover.c` — no MCDC-006-style
ceiling exists because result's `require_msg`-only panic surface
vanishes under `-DCANON_NO_REQUIRE`, leaving no panic branch in the
measured set (the same fact that leaves result with no handler-call WP
goals: the two evidence streams agree on the surface's absence). Of the
28 outcomes, 22 are the generated `result_int_VErr_*` conditions —
attributed to `vmacros/vdrivers/result_verify.h`, the file containing
the `DEFINE_RESULT_FUNCTIONS` expansion site — and 6 are cover-driver
scaffolding in `result_cover.c`. See MCDC-007 for the attribution
finding and its forward implications for vec/deque/fold.

### Driver mechanism note

result follows option's driver template exactly (see the option
section's driver mechanism note and `docs/vmacros.md`), with two
module-specific choices worth recording: the representative
instantiation uses **distinct T and E types** (`int`, enum `VErr`) so
the tagged union is genuinely two-typed, and the driver verifies the
**default named-`val`-member layout** (`CANON_RESULT_ANON_UNION` is not
exercised — it changes field-access syntax, not logic, and keeping the
proof and coverage streams on one layout keeps them comparable). The
cover TU takes its instantiation from the driver include — the literal
one-instantiation-two-consumers form — which is why the generated
conditions attribute to the driver header (MCDC-007, finding 2).

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: compiles `require_msg` runtime checks away;
  ACSL `requires` clauses provide the static guarantees. For result
  this removes the *entire* panic surface — see the panic-surface note
  above.
- **`-DNDEBUG`**: standard release-mode flag.

### Reproduction

```bash
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I semantics \
    -I vmacros \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  vmacros/vdrivers/result_verify.h
```

Expected output: `Proved goals: 207 / 213` with 6 unproved goals, the pin
since VERIFY-039 G14: the run-time checks that a callback is a valid
function (`185 / 215` with 30 when enforced: the 2 contract.h handler
goals + 28 combinator function-pointer-dispatch goals), and 14+ expected
`[wp:union]` warnings (VERIFY-015's union-model hypothesis — not
failures).

---

## semantics/borrow.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals) — in place |
| **Baseline commit**    | 262a503 (Canon-C CI #1110); report-only first at 383bf9f (CI #1109); enforced as of #1111 |
| **Functions**          | 24 of 24 non-macro functions annotated and proved |
| **Proof obligations**  | 2439 / 2458 discharged automatically (99.23%)   |
| **Unproved**           | 19 (17 inherited + 2 own; all documented under VERIFY-016) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed+Cast` |
| **CI enforcement**     | Yes — 2439/2458 with 19 named goals expected    |
| **MC/DC coverage**     | 94.7% (36/38 condition outcomes — see MCDC-008) |
| **CI artifact**        | `wp-proof-borrow` (full per-goal breakdown)     |

> **Correction (2026-10-20, VERIFY-039 G13, G14 and G17).** The table and the
> sections below give the figures and readings of enforcement. Since 767a151
> the job pins 2451 / 2465 with 14 unproved (first run CI #1368, held at
> CI #1373); G13 had moved it to 2439 / 2460 with 21 (7624d8c, CI #1367). The
> 14 are slice.h's eight memcmp danglingness goals and checked.h's two,
> inherited, and 4 own: `borrowed_bytes_eq`'s two at memcmp, and two at
> `borrowed_str_eq`'s call to `str_equal`. The own pair below was not a
> feature gap but false for calls the contract admitted: `\dangling` is a
> property of the bytes compared, and comparing the bytes of a pointer to a
> freed block met every precondition. Since 7624d8c `borrowed_bytes_eq`, and
> `str_equal`, require that no compared byte hold a dangling address, and
> `borrowed_str_eq` requires the same of its views: the four hold for every
> admitted call, wait on WP's `\dangling`, and are filed by name under
> VERIFY-008's Category 3, and VERIFY-016's own block is retired. Of the 17
> inherited below, the seven of slice.h's that G14 and G17 closed had not held
> for every admitted call (see slice.h's section).

borrow.h is the fourth semantics/ module verified and the second
semantics/ header verified **in place** (after error.h): its 24
non-macro functions are annotated in place, slice.h-style, while
`DEFINE_BORROWED_SLICE` remains outside the verified surface — parked
on the single-file macro-family list in `docs/vmacros.md`, per the
DEFINE_SLICE disposition. That pairing (in-place-verified non-macro
surface + parked macro family) is slice.h's structure (VERIFY-007);
borrow.h is the first semantics/ header to carry it. The single-file
families are a distinct category from the multi-file Shape-B driver
modules per `docs/vmacros.md` — the split here is in-place-verified
vs. parked, not Shape A vs. Shape B. It uses `Typed+Cast` — forced
transitively by slice.h's casts plus borrow.h's own `(const u8 *)`
cast in `borrowed_bytes_slice` — so, unlike error.h/option/result,
its CI wrapper retains the CVC5-presence check the `Typed+Cast`
headers carry.

The verified configuration is the **default build** (`CANON_LIFETIME`
off, `-DCANON_NO_REQUIRE -DNDEBUG`): the `BORROW_LT_*` macros expand
to nothing, so WP sees the exact shipped bodies of the v1.2.x ABI
guarantee. The lifetime-debug path is runtime-only by construction
(OWN-001 §7), validated by `test/semantics/borrow_test.c` across all
16 CI configs.

### Function inventory

All 24 non-macro functions annotated and proved (residuals are
call-site/feature-gap goals, not function failures):

- **borrowed_ptr (6)**: `from`, `from_lifetime`, `null`, `get`,
  `is_valid`, `eq`
- **borrowed_str (9)**: `from`, `from_lifetime`, `from_cstr`, `empty`,
  `get`, `is_valid`, `len`, `eq`, `slice`
- **borrowed_bytes (9)**: `from`, `from_lifetime`, `from_cbytes`,
  `empty`, `get`, `len`, `is_valid`, `eq`, `slice`

The two `_from_lifetime` variants are contract-identical to their
`_from` counterparts in the verified configuration (the lifetime
parameters are consumed by no-op macros).

### What is proved

- **No specification vocabulary of its own**: every contract uses
  slice.h's exported predicates (`cbytes_invariant`, `str_invariant`,
  `str_valid`) directly — borrow.h defines zero ACSL predicates.
- **Defensive NULL semantics as behaviors**: the three `_get`
  functions (and `is_valid`/`len`) carry complete + disjoint
  `null_b`/`non_null` behavior splits matching the shipped
  release-mode contract ("NULL → safe empty") — the same branches the
  CANON_NO_REQUIRE-gated tests cover in the measurement stream.
- **Full functional spec where the body is self-contained**:
  `borrowed_bytes_slice` proves a complete two-behavior clamping spec
  (`returns_empty`/`returns_sub`, complete + disjoint), and
  `borrowed_bytes_eq` proves the **full content-equality
  postcondition** through memcmp's libc contract.
- **Partial spec by delegation where the callee's is partial**:
  `borrowed_str_eq` carries `str_equal`'s partial ensures trio
  (boolean result, length-mismatch ⇒ false, same-view ⇒ true) with an
  in-source comment recording why full content equality cannot be
  stated (str_equal's spec is deliberately partial — VERIFY-007).
- **Day-one initialization/validity clauses**: guarded `\valid_read`
  and `\initialized` preconditions in implication form (the
  VERIFY-012 lesson applied at authoring time) closed the memcmp
  `valid_s1/s2` and all initialization goals at
  `borrowed_bytes_eq`'s call site — only the two danglingness goals
  remain (the `\dangling` feature gap).
- **Dead-guard discharge as a named goal**: the assertion
  `dead_by_invariant: \false` inside `borrowed_bytes_eq`'s one-NULL
  guard **proved**, formally establishing the guard unreachable under
  `cbytes_invariant` — the cross-stream half of MCDC-008.
- **Source-tag inertness**: the `source` debug field appears in no
  validity clause; the header never dereferences it.

### Contract-alignment note (round 2)

The first landing (CI #1109) mirrored slice.h's callee contracts by
shape and carried 27 unproved — the final 19 plus 8 call-site
chaining goals (`cbytes_from`'s two requires clauses at both `_from`
constructors, `str_valid` requires at `str_slice`/`str_equal` call
sites, and an unprovable full-equality ensures on `borrowed_str_eq`).
Aligning the four affected contracts to slice.h's real clauses
(262a503, CI #1110) closed all 8 with zero executable change and
removed two duplicate borrow-local predicates. The 19-goal residual
set is the identical named subset of #1109's 27 — name-stable across
both runs by construction.

### Residual goals (19)

All 19 are documented under VERIFY-016. All are Timeouts (0 Unknown,
0 Failed). They split into 17 inherited and 2 own.

**Inherited (17)**: slice.h's full 15-goal surface (VERIFY-007/-012 —
12 memcmp valid/danglingness + 1 strlen valid_string + the 2
contract.h handler goals), re-emitted byte-identically, plus
checked.h's 2 (VERIFY-002), prefix-flipped `typed_` → `typed_cast_`
under this TU's model. The sixth composability confirmation, and the
smallest inherited surface since memory.h — borrow.h's TU pulls in
checked.h + slice.h only (no ptr.h, no memory.h).

**borrow-own (2)**:
`typed_cast_borrowed_bytes_eq_call_memcmp_requires_danglingness_s1/s2`
— the known memcmp `\dangling` feature-gap class (VERIFY-007/-008)
at the header's only direct libc call. **No new residual class**: the
first downstream header since error.h to add none (no combinators, no
hooks, no function-pointer dispatch).

### MCDC-008 cross-reference

borrow.h's gcov-measured MC/DC is 36/38 = 94.7%, its documented
ceiling. The three `_get(NULL)` defensive outcomes were closed at
CI #1106 by `CANON_NO_REQUIRE`-gated tests (reachable by design in
the coverage build); the two remaining outcomes are the one-NULL
guard this section's `dead_by_invariant` goal proves dead — the
seventh cross-stream closure and the first carried by a named
in-body assertion, giving the closure a regression detector in
**both** evidence streams (the coverage job's per-line debug step and
the WP wrapper's proved-set check). See MCDC-008.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: compiles `require_msg` runtime checks
  away; ACSL `requires` clauses provide the static guarantees. Unlike
  memory.h's panic surface, borrow.h's `_get` functions pair the
  assertion with a defensive branch that survives the flag — specced
  as the `null_b` behaviors, covered by the gated tests.
- **`-DNDEBUG`**: standard release-mode flag.
- **`CANON_LIFETIME` off (default)**: the verified configuration; see
  the summary note and OWN-001 §7.

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
  semantics/borrow.h
```

Expected output: `Proved goals: 2451 / 2465` with the 14 documented
unproved goals, the pin since VERIFY-039 G14 and G17 (`2439 / 2458` with
19 when enforced, VERIFY-016).


---

## semantics/diag.h

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals) — in place |
| **Baseline commit**    | bb269f9 (Canon-C CI #1134, actions run 28967245082); report-only first at d8566d5 (CI #1132, 300s), re-measured name-stable at 1597a51 (CI #1133, 120s); enforced as of 1965b23 (#1135) |
| **Functions**          | 13 of 13 non-macro functions annotated and proved |
| **Proof obligations**  | 3050 / 3060 discharged automatically (99.67%)   |
| **Unproved**           | 10 (2 inherited + 8 own; all documented under VERIFY-017) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-variadic-no-translation -wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed+Cast` |
| **CI enforcement**     | Yes — 3050/3060 with 10 named goals expected    |
| **MC/DC coverage**     | 97.67% (84/86 condition outcomes — see MCDC-009) |
| **CI artifact**        | `wp-proof-diag` (full per-goal breakdown)       |

> **Correction (2026-10-20, VERIFY-039 G14).** Since 767a151 the job pins
> 3050 / 3058 with 8 unproved, all diag.h's own (first run CI #1368, held at
> CI #1373). The inherited pair below, contract.h's handler goals, was false
> as written, not unprovable by construction: the invariant was `\false`, and
> the handler's contract stated no `terminates` clause, so WP 29 checked the
> default, that it terminates. Since c9ac146 the handler states
> `terminates \false;` and its loop carries no invariant: the invariant's two
> goals are gone and the termination is proved. diag.h inherits no residual
> now; its eight are VERIFY-017's block, unchanged.

diag.h is the fifth semantics/ module verified and the third verified
**in place** (after error.h and borrow.h) — completing the semantics/
layer. Its 13 `static inline` functions are annotated in place; the
four call-site expansion macros (`DIAG_PUSH`, `DIAG_PUSH_FMT`,
`DIAG_RETURN_IF`, `DIAG_PROPAGATE`) are outside the verified surface,
parked on the single-file macro-family list in `docs/vmacros.md` and
exercised by `test/semantics/diag_test.c`. It uses `Typed+Cast` —
uniquely, forced by diag.h's **own** byte-view casts (the `(char *)`
views of `Diag`/`DiagFrame` at the memset/memmove sites and in
`push_no_alias`'s `\separated` clause), making it the first
semantics/ header to originate the model requirement rather than
inherit it transitively — so its CI wrapper retains the CVC5-presence
check the `Typed+Cast` headers carry.

The verified configuration is the default shipped build
(`-DCANON_NO_REQUIRE -DNDEBUG`): `require_msg` in diag_push and
`DIAG_PUSH_FMT` compiles out, so contract.h contributes only the
definition-presence handler pair.

### Function inventory

All 13 non-macro functions annotated and proved (residuals are
call-site/feature-gap goals, not function failures):

- **Construction & reset (2)**: `diag_init`, `diag_clear`
- **Pushing (1)**: `diag_push`
- **Inspection (8)**: `diag_depth`, `diag_has_error`, `diag_frame_at`,
  `diag_root`, `diag_latest`, `diag_root_code`, `diag_latest_code`
  (+ the shared `frame_strings_ok`/`diag_invariant` predicate
  vocabulary they consume)
- **Output (3)**: `diag_print`, `diag_render_frame`, `diag_render`

### What is proved

- **Complete + disjoint behavior splits everywhere the API is
  NULL-safe**: `diag_clear` and `diag_push` carry
  `null_diag`/`valid_diag` splits; all three rendering functions carry
  three-way `no_buffer`/`null-or-empty`/`rendered` splits matching the
  documented "NULL-safe: returns 0 / no-op" contracts.
- **Full push semantics through a byte-level libc call**: on the
  overflow path, `push_depth_bounds`, `push_depth_step`, and
  `push_overflow_flag` all **prove through the memmove** — WP frames
  `d->depth` correctly past the `(char *)` write. `push_shift_semantics`
  deliberately pins only `.code` across the shift (full-frame fidelity
  is a documented weak spec in the `error_message` sense,
  runtime-verified by diag_test.c's overflow tests; the byte-level
  memmove spec cannot support field-level equalities — the residual
  class).
- **Message termination**: `push_msg_terminated` proves via the
  bounded scan loop's invariants (`copy_len_bounds`, `copy_scanned`)
  plus the explicit terminator write.
- **Buffer termination through trusted stdio axioms**: `rf_terminated`
  and `r_terminated` prove against the snprintf axiom's conditional
  `term` ensures; diag_render carries a `render_terminated` loop
  invariant that survives both the writing (`rem > 0`, fresh witness
  from the axiom) and measure-only (`rem == 0`, empty assigns range)
  iterations.
- **Dead-guard discharge as a named goal**: the assertion
  `dead_by_invariant_clamp: \false` inside diag_push's overflow clamp
  **proved** — under `diag_invariant`, both paths reaching the clamp
  give `depth < DIAG_MAX_FRAMES`, so the path is infeasible. Notable
  for what the proof traverses: on the overflow path the contradiction
  depends on the decrement surviving the memmove, so the proof
  demonstrates precisely where the byte-view gap ends (depth framing
  holds) even while frame-content goals at the same call site resist.
  The cross-stream half of MCDC-009 outcome 1.

### Trusted stdio axioms

diag.h carries two `__FRAMAC__`-gated redeclarations of `snprintf` and
`fprintf` — **assumed specifications** (extern, no body; WP axioms)
that exist because Frama-C 29's stock variadic handling emits an open
`assigns *(buf+(0..))` range the Typed+Cast model rejects ('Invalid
infinite range', aborting the run). They require
`-variadic-no-translation` so the specs bind to the untranslated
calls. Two clauses are deliberate judgment calls, recorded normatively
in VERIFY-017: `format != \null` instead of `valid_read_string`
(the validity goal is triple-prover-resistant even for literals — 32
caller obligations, all timeouts, at CI #1132/#1133 — and every call
site passes a literal), and an unconditional termination ensures whose
encoding-error side rests on the same hosted-libc environmental
assumption as MCDC-009's permanent outcome-2 measurement residual —
one assumption, cited by both evidence streams.

### Spec-alignment chronology (round 1 → pinning)

The report-only first run (d8566d5, CI #1132, 300s) returned 2968/3027
with 59 unproved. 49 of the 59 traced to the round-1 stdio specs, not
the code or the in-place contracts: 32 `valid_read_string(format)`
caller obligations plus 17 `rf_/r_terminated` casualties of the
ensures-free round-1 snprintf axiom. An intermediate run (1597a51,
CI #1133, 120s) re-measured the same header goal-for-goal identically
to the 300s run — the residual set is name-stable across both
timeouts, refuting time-starvation directly. The trusted-axiom
alignment closed all 49 with zero executable change (bb269f9,
CI #1134: 3050/3060, exactly 10 unproved), the toolchain byte-identical across
all three runs. The 10 are the identical named subset of the round-1
59 — name-stable across three runs by construction.

### Residual goals (10)

All 10 are documented under VERIFY-017. All Timeouts at the pinning
run; the diag_init memset goal oscillates Timeout/Unknown across runs
(the wrapper counts the classes together). They split 2 inherited +
8 own.

**Inherited (2)**: `typed_cast_contract_default_handler_terminates` /
`_loop_invariant_established` — VERIFY-006 cat 4, definition-presence
only under `CANON_NO_REQUIRE` (no diag function contains a handler
call), prefix `typed_cast_` per this TU's model (the VERIFY-016 flip).
The **smallest inherited surface of any residual-carrying header**:
diag.h's TU pulls in types.h, contract.h, and error.h only, and
error.h is VERIFY-013 clean. Seventh composability confirmation.

**diag-own (8)**: the libc byte-view class (VERIFY-007/-008 pattern at
two new sites) — diag_init's memset byte-view validity (1), diag_push's
memmove call-site validity pair (2), the `.code`-equality
`push_shift_semantics` parts (3), and the assigns-framing pair against
the byte-level write (2). **No new residual class**: round-1's
predicted stdio-intrinsic class did not materialize — with sound
axioms the stdio surface contributes zero residuals.

### MCDC-009 cross-reference

diag.h's gcov-measured MC/DC is 84/86 = 97.67%, its documented ceiling
(reached at CI #1120, when the render-pair gap closure grew the
denominator 46 → 86 and surfaced two pre-verification defects, both
fixed). The two open outcomes have **distinct dispositions** — a first:
the diag_push overflow clamp is invariant-dead and WP-closed via the
named `dead_by_invariant_clamp` assertion (the eighth cross-stream
closure, second named-assert, detectors in both streams), while
diag_render's `n < 0` encoding-error skip is **libc-environmental** —
permanently open by design, with no corresponding WP goal, sharing its
assumption with the snprintf axiom's `term` ensures. See MCDC-009.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: compiles `require_msg` runtime checks
  away; ACSL `requires` clauses provide the static guarantees.
- **`-DNDEBUG`**: standard release-mode flag.
- **`-variadic-no-translation`** (WP invocation, not a preprocessor
  define): required for the trusted stdio axioms to bind — see the
  axioms section above.

### Reproduction

```bash
frama-c \
  -variadic-no-translation \
  -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I semantics \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  semantics/diag.h
```

Expected output: `Proved goals: 3050 / 3058` with the 8 documented
unproved goals, the pin since VERIFY-039 G14 (`3050 / 3060` with 10 when
enforced, VERIFY-017).


---

## data/vec (driver)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals) — driver   |
| **Baseline commit**    | 96dd41d (Canon-C CI #1152, run 3); report-only chronology 1eeb58c (CI #1150, run 1) → 8a3bb1e (CI #1151, run 2); enforced as of e663e2c (CI #1154); ratcheted 1c54f0c (CI #1202, API-001) and 43a46b1 (CI #1247, F4 closure — measured at c427548 / CI #1246) |
| **Functions**          | 37 generated `vec_int_*` functions contracted and proved |
| **Proof obligations**  | 5285 / 5473 discharged automatically (96.56%) — 5271 / 5467 before VERIFY-023 |
| **Unproved**           | 196 (123 inherited + 73 subject-side: 53 own + 20 fresh result(Bool, Error) instantiation; VERIFY-018 incl. Correction note 2026-07-16 and DEMONSTRATED note 2026-08-17; 0 Failed) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed+Cast` |
| **CI enforcement**     | Yes — pinned `5285 / 5473` (5271 / 5467 before VERIFY-023) + zero-Failed + exact count 188 + by-name roll-call (set equality); ratcheted at CI #1247 (43a46b1) |
| **MC/DC coverage**     | 98.10% (155/158 condition outcomes — see MCDC-010) |
| **CI artifact**        | `wp-proof-vec` (full per-goal breakdown)        |

> **Correction (2026-10-20, VERIFY-039).** The table and the sections below
> give the figures of enforcement, brought up to date at VERIFY-023. Since
> a79b69d the job pins 5474 / 5569 with 95 unproved (first run CI #1369, held
> at CI #1373): memory.h's 31 and arena.h's 2, inherited; option's six and
> result's six at (`Bool`, `Error`), the run-time checks on the callbacks'
> pointers since G11; and 50 own, each filed by name (VERIFY-039's batch 1):
> 24 under the block on a loop without annotations in a macro body, 21 under
> VERIFY-017's byte-view block, 2 under VERIFY-008's Category 1, and one each
> under the blocks on byte ranges compared by pointer order, on a call framed
> by the callee's default `assigns`, and on a struct exchange whose operands
> may overlap. G11 closed the option arm's 26 termination, exit and frame
> goals and the result arm's 14, which did not hold for every callback the
> contracts admitted (a4f8f5f, b52bba6), and G18 made vec's alloc and free
> state the heap's state, so that their frames, proved from the allocators',
> rest on true ones (806bdeb). The job's pin comment gives each step.

> **Note (2026-10-20, VERIFY-040 round 1).** Since 62ce2a0, the round's
> ratchet, the job pins 5475 / 5569 with 94 unproved (first run CI #1374):
> `swap` requires its operands the same vec or separated (49d38d7), and its
> first postcondition, the struct exchange's, is proved, so the own residuals
> are 49 and the block on a struct exchange whose operands may overlap is
> retired.

> **Note (2026-10-21, VERIFY-040 round 2).** Since the score's ratchet the
> job pins 5481 / 5574 with 93 unproved, the line CI #1375 measured, two
> goals more than round 2 predicted, both proved. `extend` states its frame
> as postconditions and `append_array` its frame above the new elements
> (ec88871), so `extend`'s `ok` frame and its goal are gone and the block on
> a call framed by the callee's default `assigns` is retired; memory.h
> states `regions_overlap` by `\separated` (2d36ca1), and `append_array`'s
> no-overlap goal stays, the byte view, filed under VERIFY-017's block, so
> that the block on byte ranges compared by pointer order is retired too.
> The own residuals are 48.

vec is the **third driver-verified Shape-B module** (after option and
result) and the **first data/-layer module verified** — verification's
entry into data/. WP runs over `vmacros/vdrivers/vec_verify.h`, which
instantiates the real shipped macros at `int` via the
`DEFINE_VEC_STRUCTS` / `DEFINE_VEC_FUNCTIONS` split (landed upstream
as VERIFY-018 finding F3 before the driver was drafted — the
split-patch-first ordering is now the standing checklist item for
deque). It is the **first driver on the `Typed+Cast` model**: unlike
option/result's by-value structs, vec crosses `void*` boundaries —
`(int*)mem_alloc(bytes)`, and `int*` buffers through
`mem_copy`/`mem_move`'s `void*` parameters — so its CI wrapper carries
the CVC5-presence check of the Typed+Cast family. The TU is the
largest verified to date: the facade pulls slice.h, ptr.h, arena.h →
memory.h and the full substrate, plus the option and result
instantiations the driver composes (5467 goals at HEAD).

The verified configuration is the default shipped build
(`-DCANON_NO_REQUIRE -DNDEBUG`, `CANON_LIFETIME` off): vec's entire
panic surface (3 `require_msg`, 10 `ensure_msg`, zero direct handler
calls) compiles away, so the driver's `requires` clauses are
load-bearing and contract.h contributes only the definition-presence
handler pair.

### Function inventory

All 37 generated functions contracted and proved (residuals are
call-site/feature-gap/model goals, not function failures):

- **Constructors & teardown (5)**: `init`, `empty`, `alloc`,
  `arena_alloc`, `free`
- **Push family (3)**: `push`, `try_push`, `push_unchecked`
- **Element access (8)**: `get`, `get_option`, `get_unchecked`, `at`,
  `set`, `first`, `last`, `data`
- **Pop family (2)**: `pop`, `pop_option`
- **Queries (5)**: `len`, `capacity`, `remaining`, `is_empty`,
  `is_full`
- **Structural mutation (4)**: `clear`, `insert`, `remove`,
  `remove_option`
- **Bulk (3)**: `append_array`, `extend`, `fill`
- **Misc (2)**: `swap`; iterator pair `iter_init`, `iter_next`
- **Slices (2)**: `slice_init`, `slice_get`
- **Lifetime plumbing (2)**: `lifetime_open_`, `lifetime_close_`
  (no-ops under the verified `CANON_LIFETIME`-off configuration)

The three `DEFINE_VEC_SLICE` facade views
(`as_slice`/`as_slice_full`/`as_bytes`) are **measured** in the cover
TU (MCDC-010) but not yet WP-driven — the facade-widening pass is a
recorded follow-up. The optional `_range`/`_fmt` extension files are
deferred per `docs/vmacros.md`; the core decl/impl/defn surface is
what this entry verifies.

### What is proved

- **Complete + disjoint behavior splits across the NULL-safe API**:
  every fallible function carries explicit
  null/empty/full/out-of-range/ok behavior splits matching the
  documented "NULL → error/no-op" contracts, proved complete and
  disjoint.
- **Capacity discipline**: push/insert/append preserve
  `len <= capacity`; the too-big constructor legs
  (`capacity > CANON_VEC_MAX_CAPACITY / sizeof(int)`) prove degenerate
  results; `alloc`/`arena_alloc` prove **branch-complete** — the
  `!checked_mul` overflow branches are formally dead under the
  preceding capacity guard (the WP half of MCDC-010's U1/U2
  infeasibility rows).
- **Length/index semantics through the transfer calls**: insert/remove
  length steps, index bounds, and the untouched-prefix framing prove
  through `mem_move`'s byte-level contract; the element-content
  equalities across the transfer are the documented category-(d)
  residuals (frame-only memcopy/memmove specs — the VERIFY-017
  `push_shift_semantics` shape).
- **Composed option/result plumbing**: `get_option`, `pop_option`,
  `remove_option`, and the `result__Bool_Error` returns prove against
  the composed driver contracts (the run-1 → run-2 composition fix;
  see VERIFY-018's method lesson R1). The `[wp:union]` warnings on the
  `.val.err` postconditions are the expected VERIFY-015
  union-model standing hypothesis, re-cited here.

### Run chronology (report-only → enforced)

Run 1 (1eeb58c, CI #1150, 5h21m): 4988/5350, 362 unproved — spec-less
composed callees drowning provable goals (lesson R1). Run 2 (8a3bb1e,
CI #1151): 5208/5413, 205 — driver composition landed; inherited
surface stabilized at 143 names. Run 3, the baseline (96dd41d, CI
#1152): 5184/5380, 196 (193 Timeout + 3 Unknown + 0 Failed) —
delegate narrowing closed 9 own goals (62 → 53); the two
retry-variance candidates failed a third consecutive run and were
pinned. Enforcement went green on its first run (e663e2c, CI #1154)
with the set name-identical for the third consecutive run, while
prover attribution, source line numbers, and roll-call order all
shifted beneath it — the empirical validation of name-only,
order-independent matching.

### Residual goals (196)

All 196 are documented under VERIFY-018 (incl. Correction note
2026-07-16) — 121 inherited **byte-identically** (arena.h 46, option
combinators 32, memory.h 20, slice.h libc 13, ptr.h 6, checked.h 2,
contract.h handler pair 2; the core arm equal to arena.h's pinned list
verbatim — fragment indices and Timeout/Unknown sub-verdicts
reproduced — and the option arm equal modulo the typed_→typed_cast_
prefix; pinned name-stable across three runs), 22 on the **fresh
result(Bool, Error) instantiation** (a type pair not verified in
VERIFY-015's home unit: 20 goals matching the family's home profile
family/count/split-exactly, 8 home assigns goals never emitted under
the driver's lighter contracts, 2 new union get_* mem-access goals —
attribution open, VERIFY-018 F4), and 53 vec-own in four categories:
**(e)** 2 allocation-model plumbing goals (`\fresh`/`\freeable`
feature gap at alloc/free delegation), **(d)** 5 element-transfer
ensures (frame-only memcopy/memmove specs), **(g)** 24 fill
macro-body-loop goals (ACSL loop annotations cannot survive macro
definition — unprovable by construction; MC/DC is the primary
evidence, all three fill legs covered; deque's shift loops
pre-classified here), and **(h)** 22 Typed+Cast int↔char bridging
goals (assigns-framing and transfer call-requires at the typed/void*
boundary). Zero `rte_function_pointer` goals on vec functions — the
first module without the OWN-003 dispatch class of its own.

> **Correction (2026-10-20, VERIFY-039 batch 1).** Category (g) is not
> unprovable by construction: the loop carries no annotation, which under
> `-C`, the preprocessing `frama-c-vec` uses, could not attach, and under
> `-CC` could (VERIFY-025). Its 24 goals are true, filed by name under
> VERIFY-039's block on a loop without annotations in a macro body,
> specification-bound.

> **Note (2026-10-22, VERIFY-040 round 3).** Since a95276e the loop carries
> its invariants, frame and variant, written in `IMPL_VEC_FILL`. Under `-C`
> they are stripped, as above, and the 24 stay in `frama-c-vec`, under the
> same block; under `-CC` they attach, and `cc-vec` and `cc-vec_struct` prove
> fill's goals (CC #58).

### MCDC-010 cross-reference

vec's cover TU measures 155/158 = 98.10%, its documented ceiling from
the first run (CI #1146), byte-stable through enforcement. The three
open outcomes carry two dispositions: U1/U2 (the `!checked_mul` true
outcomes in alloc/arena_alloc) are **guard-redundancy-infeasible and
WP-corroborated** — this entry's branch-complete constructor proofs
are the proof-stream half — and U3 (heap `!buf`) is
**environmental**, with the arena sibling's covered exhaustion leg
demonstrating the exclusion is heap-specific. The Shape-B attribution
check confirmed vec as the third variant: generated conditions
attribute to `vec_cover.c` itself (direct `DEFINE_VEC`
instantiation), with `vec_impl.h`'s functions-but-no-conditions
fingerprint standing as the Shape-A-drift tripwire. See MCDC-010.

### Preprocessing flags

- **`-DCANON_NO_REQUIRE`**: compiles `require_msg`/`ensure_msg`
  runtime checks away; the driver's ACSL `requires` clauses provide
  the static guarantees.
- **`-DNDEBUG`**: standard release-mode flag.
- **`CANON_LIFETIME` off** (default): the lifetime plumbing pair are
  no-ops in the verified configuration (OWN-001 §7 precedent).

### Reproduction

```bash
frama-c \
  -wp -wp-rte \
  -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" \
    -I core/primitives \
    -I core \
    -I semantics \
    -I data \
    -I . \
    -DCANON_NO_REQUIRE \
    -DNDEBUG" \
  vmacros/vdrivers/vec_verify.h
```

Expected output: `Proved goals: 5475 / 5568` with the 93 documented unproved
goals, the pin since the VERIFY-008 overlap fix (predicted) (`5481 / 5574`
since VERIFY-040 round 2's score, `5475 / 5569` with 94 since round 1,
`5474 / 5569` with 95 since VERIFY-039 G18, `5285 / 5473` with 188 at
VERIFY-023, 196 before it;
VERIFY-018), 0 Failed. Wall time ~3h05m (CI #1247;
~2h50m before the F4 contracts added their 38 goals)
(timeout-dominated; see VERIFY-018's runtime record).


---

## data/deque (driver)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals) — driver   |
| **Baseline commit**    | 5ae441d (Canon-C CI #1234, run 1) → 08bdf42 (CI #1237, run 2, set-identical); enforced as of db0a864 (CI #1238); re-confirmed 06e4cad (CI #1239) and dbf31c3 (CI #1240). Superseded pre-fix measurement: 0738eb5 (CI #1231) |
| **Functions**          | 24 generated `deque_int_*` functions contracted and proved |
| **Proof obligations**  | 1601 / 1668 discharged automatically (95.98%)   |
| **Unproved**           | 67 (2 inherited handler + 32 option arm + 28 fresh result(Bool, Error) + 5 deque-own; VERIFY-019; **0 Failed, 0 Invalid, 0 Stepout**) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed` |
| **CI enforcement**     | Yes — pinned `1601 / 1668` + zero Failed/Invalid/Stepout + exact count 67 + by-name roll-call over all 67 (set equality) |
| **MC/DC coverage**     | 100.00% (82/82 condition outcomes — see MCDC-011) |
| **CI artifact**        | `wp-proof-deque` (full per-goal breakdown)      |
| **Job runtime**        | ~52 min (vs vec's ~2h50m; deque is not the workflow's long pole) |

> **Correction (2026-10-20, VERIFY-039 batch 2, G11 and G14).** The table and
> the sections below give the figures and readings of enforcement. Since
> 767a151 the job pins 1656 / 1673 with 17 unproved (first run CI #1368, held
> at CI #1373): option's six and result's six at (`Bool`, `Error`), the
> run-time checks on the callbacks' pointers, and the 5 own, `swap`'s. G11
> closed the option arm's 26 and the result arm's 22 termination, exit and
> frame goals, which did not hold for every callback the contracts admitted
> (a4f8f5f, b52bba6), and G14 the handler's pair, false as written (c9ac146).
> The swap cluster is not vec's class widened: its five clauses about `*b` are
> true, and WP generates the fact that proves them, then drops it in its own
> hypothesis filter, because `*b` is stored from a local whose value came from
> its initializer and each clause names one field. `tmp`, whose address is
> never taken, is a value no store reaches, so the account of the obstacle as
> `tmp`'s preservation across the second struct write does not fit the model.
> The five are filed by name under VERIFY-039's block on a struct stored from
> an initialised local, stated field by field.

> **Note (2026-10-20, VERIFY-040 round 1).** Since 62ce2a0, the round's
> ratchet, the job pins 1653 / 1665 with 12 unproved (first run CI #1374):
> `swap` admits `a == b`, the call `test_swap_self` makes, and states each
> side of the exchange as one conjunction (d0dea31). Both are proved, deque
> has no own residual, and the five's block is retired.

deque is the **fourth driver-verified Shape-B module** (after option,
result and vec) and the **second data/-layer module**. WP runs over
`vmacros/vdrivers/deque_verify.h`, which instantiates the real shipped
macros at `int` via the `DEFINE_DEQUE_STRUCTS` /
`DEFINE_DEQUE_FUNCTIONS` split — VERIFY-018 finding F3's standing
checklist item, executed at CI #1225 with the expansion verified
byte-identical to the pre-split monolithic macro across four
linkage/type combinations *before* the driver was drafted.

### Model: Typed, not Typed+Cast

Unlike vec — the first driver on `Typed+Cast`, which crosses `void*`
boundaries through `mem_alloc` and `mem_copy`/`mem_move` — deque's own
implementation contains **zero casts**, and its include closure is
types/limits/contract/ownership + error + option + result. None of the
cast-originating headers (memory.h, ptr.h, slice.h, arena.h,
checked.h) appear. deque is therefore the first data/-layer driver on
plain **Typed**. The flag is load-bearing and the job banner says so:
it is what made the VERIFY-018 F4 control possible.

### The load-bearing fact: `size <= capacity`

deque is a **ring**, not a shifting buffer. Every index update is
modular — `(tail + 1) % capacity`, `(head + 1) % capacity`, and the
two `(x == 0) ? capacity - 1 : x - 1` decrements. Two obligations
follow, and **both are discharged by the invariant rather than by any
runtime guard**:

1. **Division by zero.** `% d->capacity` is UB at capacity == 0, and
   no body tests capacity directly. push_*'s `size >= capacity` and
   pop_*'s `size == 0` early returns make the modulus unreachable at
   capacity == 0 *only* under `size <= capacity`, which at capacity 0
   forces size 0 and trips both guards.
2. **Unsigned wrap.** `deque_int_remaining`'s `capacity - size` is
   computed unguarded on the non-NULL leg — the module's canary, and
   the structural analogue of vec's `fill()`.

Both proved on every run. The driver carries the ring relation
`tail == (head + size) % capacity` as a **separate** predicate from
the validity view, so that a functional-correctness failure could
never be mistaken for a failure to prove absence of RTE. (The relation
is guarded by `capacity > 0`, since at capacity 0 it would itself
divide by zero.)

### Residual goals (67)

| Arm | Goals | Nature |
|-----|-------|--------|
| contract.h handler pair | 2 | Inherited, definition-presence only — deque has zero direct `CANON_INVOKE_HANDLER_` calls |
| `typed_option_int_*` | 32 | Inheritance: `option_verify.h`'s verified instance at `int`, re-included. Zero `typed_cast_` prefixed — same model as option's home unit, so no prefix transform applies |
| `typed_result_Bool_Error_*` | 28 | Fresh instantiation, full home contract set attached (20 family profile + 8 `assigns`) |
| `typed_deque_int_swap_ensures_6..10` | 5 | deque-own: the swap cluster, an inherited class widened beyond Typed+Cast |
| **core substrate** | **0** | deque's closure contains none of the headers vec inherited its 91 core goals from |

> **Note (2026-10-20, VERIFY-040 round 1).** The table is the set at
> enforcement, 67. The handler pair closed at VERIFY-039 G14, the option arm's
> 26 and the result arm's 22 at G11, and the swap cluster's five since d0dea31
> (CI #1374), each side of the exchange stated as one conjunction: the job
> pins 12, the option arm's six and the result arm's six.

### Zero core-substrate inheritance

vec inherited 91 core goals byte-identically from arena.h's roll-call.
deque inherits none. Every prior composability confirmation showed
residuals propagating *downward* without amplification; deque
establishes the complementary half — **a module does not inherit what
it does not include** — and is the first module in the arc whose
inherited surface is smaller than its predecessor's. Predicted before
the first run and confirmed on all five enforced runs.

### Memory-model invariance

The driver proves identically under `Typed` and `Typed+Cast`: same
1601/1668, same Qed/solver/terminating/unreachable partition, same 67
residual names modulo the `typed_` → `typed_cast_` prefix, across
seven runs (five Typed, two Typed+Cast controls with the positive
control established). Recorded as **VERIFY-019-M**; it is the
model-axis analogue of arena-32's width-axis result.

### Stability

Five enforced-configuration runs report one distinct decomposition:
proved 1601/1668, Qed 1239, solver-discharged 277, terminating 38,
unreachable 47, residual 67. Solver *attribution* moved across that
span (Alt-Ergo 228–244, CVC5 16–30, Z3 17–24) and the
Timeout/Unknown split oscillated on `swap_ensures_10`, while the
67-name set did not move. **Which goals need a solver at all is
invariant; which solver reaches them first is scheduling** — the
clearest justification available for the house rule that pools Timeout
with Unknown.

### MCDC-011 cross-reference

`vmacros/coverage/deque_cover.c` measures **82/82 (100.00%)** with
zero justification rows and confirms Shape B by attribution
(`deque_impl.h`: functions and lines, "No conditions", under both
TUs). See MCDC-011.

### Preprocessing flags

`-DCANON_NO_REQUIRE -DNDEBUG`, `CANON_LIFETIME` off (OWN-001 §7).
Under this configuration `init`'s four guards, both `*_unchecked`
triples, `swap`'s pair **and `peek_front`/`peek_back`'s d/out guards**
all compile away, so the driver's `requires` clauses are load-bearing.
deque has no `ensure_msg` sites at all. The peek/pop NULL-guard
asymmetry this exposes is VERIFY-019 F1.

### Reproduction

```
frama-c -wp -wp-rte \
  -wp-model Typed \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args=" -I core/primitives -I core -I semantics -I data -I . \
                    -DCANON_NO_REQUIRE -DNDEBUG" \
  vmacros/vdrivers/deque_verify.h
```

The `Typed+Cast` control is the same command with the model changed;
it is wired as `.github/workflows/f4-control.yml`
(`workflow_dispatch` only, gates nothing).

## data/bitset.h (in-place contracts, interposition driver)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals)            |
| **Baseline commit**    | 95526a9 (Canon-C CI #1259, surface complete) → enforced 6cb78da (CI #1260) at 158 → ratcheted 4241a10 (CI #1265) to 163 after the F2/F5 fixes → re-confirmed 16d0f0b (CI #1266). Report-only chronology: #1249 (221), #1256 (218), #1257 (172), #1258 (163 own-66), #1259 (158) |
| **Functions**          | 32 contracted and proved, all in place in `data/bitset.h` |
| **Proof obligations**  | 4845 / 5008 discharged automatically (96.75%) — 4839 / 5002 before VERIFY-023 |
| **Unproved**           | 163 (2 handler + 32 option_usize inheritance + 58 core substrate + 71 bitset-own; VERIFY-020; **0 Failed, 0 Invalid, 0 Stepout**) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed+Cast` |
| **CI enforcement**     | Yes — pinned `4845 / 5008` + zero Failed/Invalid/Stepout + exact count 163 + by-name roll-call over all 163 (set equality, both directions). **Gate wired at CI #1285**: until then the step computed the verdict but could not fail the build (VERIFY-020 reading note). |
| **MC/DC coverage**     | 97.01% (130/134 condition outcomes — see MCDC-012) |
| **CI artifact**        | `wp-proof-bitset` (full per-goal breakdown)     |
| **Job runtime**        | ~2h50m                                          |

> **Correction (2026-10-20, VERIFY-039).** The table and the sections below
> give the figures of enforcement. Since a79b69d the job pins 4963 / 5041 with
> 78 unproved (first run CI #1369, held at CI #1373): memory.h's 31, bits.h's
> 12, borrow.h's 4, and option's six at `usize`, the run-time checks on the
> callbacks' pointers since G11 (ec6a30b), inherited; and 25 own. Of the 71
> own below, 30 closed with VERIFY-039's G4 entry 3, G6, the bounds of two
> loops by `word_count`, and G8, which found `bitset_not`'s loop invariant and
> postcondition false as written, the postcondition proved from the invariant,
> and 16 at G10 (ec56091): the null guards read `bs->words` where sixteen
> contracts did not require `*bs` readable. The 25 are filed by name: 15 under
> VERIFY-017's byte-view block, 2 under the block on a call framed by the
> callee's default `assigns`, and 8 under three blocks of VERIFY-039's batch
> 3, on the word count through a wrap at the capacity bound, on bit-level
> identities on the padding word, and on two clauses `bitset_find_next`'s code
> establishes and its specification omits. The job's pin comment gives each
> step.

> **Note (2026-10-20, VERIFY-040 round 1).** Since 62ce2a0, the round's
> ratchet, the job pins 4967 / 5043 with 76 unproved (first run CI #1374):
> `bitset_init` asserts its word count's step below the capacity bound and at
> it (e4e2635), two goals added and proved, and its `ensures_3` and
> `ensures_5` are proved, so the own residuals are 23 and the block on the
> word count through a wrap is retired.

> **Note (2026-10-21, VERIFY-040 round 2).** Since 73a7b86, the round's
> ratchet, the job pins 4978 / 5052 with 74 unproved (first run CI #1375):
> `bitset_assign` returns before its calls when `bs` or its words are null
> (ebe7b05), nine goals added and proved, and its `null` frame's two parts
> are proved, so the own residuals are 21 and the block on a call framed by
> the callee's default `assigns`, under which the two were filed, is
> retired. The new decision's four condition outcomes are covered: MC/DC,
> which the table gives as at enforcement, is 134/138 at CI #1375 (MCDC-012).

bitset is the **fifth driver-verified module** and the **third
data/-layer module**, and it is the first that is not Shape B.

### Not a Shape-B driver: interposition

`data/bitset.h` is a plain non-macro header. Its 32 functions carry
their ACSL **in place**, like `borrow.h` and `diag.h`, and its
conditions attribute to `data/bitset.h` itself in the MC/DC table — so
it has no cover TU and needs none.

`vmacros/vdrivers/bitset_verify.h` exists for one reason:
`bitset.h` self-instantiates `option_usize` (finding F3) so frama-c can
be handed the header directly, but that instantiation is
**uncontracted**, and `bitset_find_*_option` call into it. The driver
`#include`s a *contracted* `option_usize` first so those call sites have
specifications to discharge against. It contracts none of bitset's
functions. The 32-goal option arm is therefore **inheritance**, as in
deque — not a fresh instantiation as in vec. See `docs/vmacros.md`.

### Model: Typed+Cast

Forced by `bitset_as_bytes` / `_as_cbytes` / `_as_borrowed_bytes`,
which hand `bs->words` (a `u64*`) to byte-view constructors. Goal names
carry the `typed_cast_` prefix throughout. An earlier draft of this arc
predicted plain Typed; corrected before the first run.

### The 71 own residuals

Derived by the job rather than hardcoded, and summing exactly:

| n | class |
|---:|---|
| 26 | array framing on and / or / xor / not — writing `words[w]` leaves `words[0..w-1]` alone, which WP does not frame for free |
| 22 | Group 1 single-bit family + init |
| 9 | clear_all / set_all frames over symbolic mem_zero / mem_set ranges |
| 6 | P4 Typed+Cast bridge at the three as_bytes sites |
| 8 | queries and finds |
| **71** | smallest own-residual set of any driver-verified module (vec 196, pool 119, region 114, arena 91, deque 67) |

> **Correction (2026-10-20, VERIFY-039).** The comparison in the last row is
> not one: 71 is bitset's own set, and the figures beside it are other units'
> totals of unproved goals, three of them units verified in place, not through
> a driver. Among the driver-verified modules at this baseline every other own
> set was smaller, option's 32, result's 28, vec's 53 and deque's 5: bitset's
> 71 was the largest. At CI #1373 bitset's own set is 25, against vec's 50,
> deque's 5, option's and result's 6 and range.h's 0.

> **Note (2026-10-20, VERIFY-040 round 1).** At CI #1374 bitset's own set is
> 23, vec's 49 and deque's 0.

> **Note (2026-10-21, VERIFY-040 round 2).** At CI #1375 bitset's own set is
> 21 and vec's 48.

The job prints an `UNCLASSIFIED` line naming any stray if the class list
ever stops partitioning — added after the first derived version summed
to 77 (prefix collision: `bitset_set_` also matches `bitset_set_all_`)
and the second to 68 (`test_*` and `xor_call_*` matched no class). Both
errors were invisible to reading and obvious to arithmetic.

### The specification-strength cap (F4)

`bits_popcount`, `bits_ctz` and `bits_clz` are specified at RANGE
strength only — `0 <= \result <= 64` — by a written decision in bits.h.
`bitset_count` therefore cannot claim `\result <= bs->capacity` (only
`<= 64 * word_count`); `bitset_is_full` delegates and is capped
transitively; `find_*` cannot claim minimality, and the `_option`
wrappers inherit the cap a second time.

The cap is exactly one word wide. `\result == BITSET_NPOS || \result <
capacity` IS proved, straight from the explicit guard. This is a new
residual shape for the campaign — see `docs/decision-families.md`
Shape 5 — because every prior ceiling was prover weakness, and this one
is upstream specification weakness.

### DO NOT SIMPLIFY: `bitset_pad`

```
capacity % 64 == 0 || (words[wc-1] >> (capacity % 64)) == 0
```

The disjunct is redundant for the LOGIC and load-bearing for the
PROVER. `% 64` bounds the shift exponent syntactically in [0,63];
replacing it with `bitset_rem(bs)` requires the solver to derive the
bound from `bitset_sized` first, and until it does the term is
`x / 2^k` with k free — memory blow-up, which `-wp-timeout` cannot
bound because a timeout bounds time, not resident memory. Experiment
E1 tried exactly this and took the CI runner down twice (CI #1254).

The same lesson recurred on a **precondition** at F5: deleting
`requires bs == \null || bs->words != \null`, verified redundant
beforehand, cost five `assert_rte_mem_access` goals. Full account in
VERIFY-020.

> **Correction (2026-10-20, VERIFY-039 batch 3 and G10).** E1's lesson holds;
> F5 is not a second instance. At `bitset_not`, F5's control, the same commit
> (61e1312) widened the first precondition to admit `bs->words == \null` as
> well as deleting the clause, and after the widening the clause was no longer
> redundant: it had excluded a non-null `bs` that points to no readable
> `Bitset` and whose `words` reads null, the state in which each function's
> null guard reads `bs->words` undefined. The five goals were that read's
> checks, false in a state the contracts then admitted, not timeouts of a
> correct specification. G10 (ec56091) requires `*bs` readable, and they
> closed at CI #1358.

### Reproduction

```
frama-c -c11 -cpp-extra-args="-I. -DCANON_NO_REQUIRE -DCANON_BITS_FORCE_FALLBACK \
                             -DCANON_CHECKED_FORCE_FALLBACK" \
  vmacros/vdrivers/bitset_verify.h \
  -wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed+Cast \
  -wp-prover alt-ergo,z3,cvc5
```

See `docs/deviations.md` (VERIFY-020) for the full residual
classification and the five findings F1–F5, and MCDC-012 for the
coverage-stream record.

---

## data/priority_queue.h (in-place contracts, calls-clause comparator)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals)            |
| **Baseline commit**    | bfd1202 (Canon-C CI #1289, run 7, first measurement at the final contract state) → enforced 63d6705 (CI #1290), name-identical |
| **Functions**          | 41 contracted and proved in place, including `pq_cmp_`; DEFINE_PRIORITY_QUEUE's 17 typed wrappers parked |
| **Proof obligations**  | 4513 / 4584 discharged automatically (98.45%)   |
| **Unproved**           | 71 (43 memory.h substrate + 22 result(bool, Error) + 6 own; VERIFY-022; **0 Failed, 0 Invalid, 0 Stepout**) |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed+Cast` |
| **CI enforcement**     | Yes — pinned `4513 / 4584` + zero Failed/Invalid/Stepout + exact count 71 + by-name roll-call (set equality); the 43 memory.h names are read from the memory job's own list at run time |
| **MC/DC coverage**     | 98.78% (81/82 condition outcomes, 1 justified — see MCDC-014) |
| **CI artifact**        | `wp-proof-priority-queue`                       |
| **Job runtime**        | ~2h45m                                          |

> **Correction (2026-10-20, VERIFY-039).** The table and the paragraphs below
> give the figures and readings of enforcement. Since de5a8d1 the job pins
> 4647 / 4688 with 41 unproved (first run CI #1370, held at CI #1373):
> memory.h's 31, inherited; result's six at (`Bool`, `Error`), a fresh
> instantiation (VERIFY-022); and 4 own. G12 (77cc69c) gave the queue's result
> instantiation `get_ok`'s and `get_err`'s contracts, removing the pair
> VERIFY-018's F4 called removable; G11 (b52bba6) closed the result arm's 14
> termination, exit and frame goals, which did not hold for every callback the
> contracts admitted, leaving the arm its six run-time checks; and G21
> (da2fce6) closed two of the six own, as below. The job's pin comment gives
> each step.

> **Note (2026-10-21, VERIFY-040 round 2).** Since 73a7b86, the round's
> ratchet, the job pins 4651 / 4689 with 38 unproved (first run CI #1375):
> memory.h states `regions_overlap` by `\separated` (2d36ca1), and the three
> copies between a caller's memory and the buffer are proved,
> `pq_push_result`'s with an assertion of its slot's step (2ca5475), one goal
> added and proved. The queue's own residual is the comparator's goal, and
> the block on byte ranges compared by pointer order across objects is
> retired.

priority_queue.h is the **first in-place (Shape A) data/-layer module**. Its
central operation calls a **caller-supplied function pointer** from inside
its loops — region.h and the option/result combinators call function
pointers too, but as their last act; here the rest of the function depends on
what the call might have changed. That call was, until commit 5, an
information horizon: WP has no contract for an unknown callee, so everything
after `pq->cmp(...)` began with the queue's fields unknown, and ~99 of the
run-4 residuals were downstream of it.

> **Correction (2026-10-20, VERIFY-039 G19).** region.h does not call its
> hooks as its last act: `region_end` calls them in a loop, clears each slot
> after its call, and after the loop resets the arena and closes the region,
> all in state a hook can change. G19 found its goals after the call false for
> some admitted hooks, and its repair requires each hook to keep
> `region_hook_rule`, a predicate with no definition whose meaning is its
> comment, as no clause can state it of code behind a pointer.

The comparator is proved as a **verified configuration**, not by a trusted
axiom. `pq_cmp_` is the single call point; it carries a `calls` clause naming
compare.h's 24 built-in comparators — all proved 208/208 with `assigns
\nothing` — and `pq_wf` requires `pq->cmp` to be one of them with an element
wide enough for it. The wrapper's frame and termination then follow from their
contracts. A caller-supplied comparator compiles and runs exactly as before;
it is outside the proof. This is lifetime.h's "verified at level 4 only"
shape, and it is stronger than diag.h's trusted-axiom shape because nothing
is assumed.

**What is claimed** is the structural invariant `pq_wf` — validity, positive
`elem_size` and `capacity`, no product wrap, child indices that cannot wrap
(F-WRAP), `len <= capacity`, a buffer valid and disjoint from the struct, a
built-in comparator of fitting width — preserved by every mutator and required
by every query. **Heap order is not claimed**: the built-ins ensure only
`-1 <= \result <= 1`. The runtime suite is the evidence for ordering.

**The six own residuals**: one `\valid_function(pq->cmp)`, unimplemented in
Frama-C 29; five `!regions_overlap` obligations at four `mem_copy` sites and
one `mem_swap` site, which memory.h states by pointer ordering and which
`\separated` therefore cannot discharge across bases — a memory.h
contract-shape finding and the **VERIFY-024 candidate**.

> **Correction (2026-10-20, VERIFY-039 batch 2 and G21).** Two of the five are
> not memory.h's pointer order. `pq_swap_`'s call to `mem_swap` and
> `pq_remove_at_result`'s call to `mem_copy` compare two slots of one buffer,
> where WP's order is defined and compares offsets; what the provers did not
> find was the step `a·x + x <= b·x` for slots `a < b` and the element size
> `x`. Since da2fce6 the queue states it as assertions before the two calls,
> proved, and both goals are proved (CI #1370). The four own left are the
> comparator's `\valid_function` check, under VERIFY-039's block on a call
> through a pointer to one of the listed comparators, and three copies between
> a caller's memory and the buffer, under its block on byte ranges compared by
> pointer order across objects.

The arc took eight commits and refuted three consecutive count predictions
before switching to named residual classes; the full history, the F-WRAP
finding, and the method notes are in VERIFY-022. The coverage arc that
preceded it, and the PQ-A finding that made six heap helpers internal, are
MCDC-014.

## data/range.h (in-place contracts, interposition driver)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals)            |
| **Baseline commit**    | c3c4e64 (Canon-C CI #1315, run 3, first measurement at the final code state) → name-identical fa05034 (#1316) → enforced 9f7a9d0 (CI #1317), name-identical |
| **Functions**          | 15 contracted and proved in place; `RANGE_FOR` (call-site macro) parked per `docs/vmacros.md` |
| **Proof obligations**  | 2611 / 2647 discharged automatically (98.64%)   |
| **Unproved**           | 36 — all inherited (32 option_isize + 2 contract handler + 2 checked.h), **0 own**; VERIFY-030; **0 Failed, 0 Invalid, 0 Stepout** |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed` |
| **CI enforcement**     | Yes — pinned `2611 / 2647` + zero Failed/Invalid + exact count 36 + by-name roll-call (set equality) |
| **MC/DC coverage**     | 98.39% (61/62 condition outcomes, 1 justified — see MCDC-015) |
| **CI artifact**        | `wp-proof-range`                                |
| **Job runtime**        | ~30 min                                          |

> **Correction (2026-10-20, VERIFY-039 G11 and G14).** Since 767a151 the job
> pins 2644 / 2652 with 8 unproved, all inherited (first run CI #1368, held at
> CI #1373): option's six at `isize`, the run-time checks on the callbacks'
> pointers, and checked.h's two. G11 (ec6a30b) gave `range_verify.h`'s copy of
> option's combinators the repaired contracts, closing 26 termination, exit
> and frame goals that did not hold for every callback the contracts admitted
> (2644 / 2654 with 10), and G14 (c9ac146) the handler's pair, false as
> written. range.h still has no own residual.

range.h is **module 18** and the **first module verified after the
argument-base saturation claim was written down** — the first prospective
test of it. VERIFY-028 pre-registered the predictions (bd1da17) before any
contract existed; VERIFY-029 scored runs 1–2 and pre-registered run 3
(9c1cdfc) before any run-3 code existed; VERIFY-030 closed it.

**What is claimed.** `range_wf` is exactly the header's two documented
invariants (`step != 0`, `step != ISIZE_MIN`). `range_count` gives the exact
number of values still to be yielded over mathematical integers, and every
function is proved against it over the **whole** `isize` domain: `range_len`
returns it exactly; `range_next` decrements it by one; `range_skip(n)` leaves
`max(0, count − n)`, landing on `current + n·step` when inside the range and
on `end` otherwise. **Not claimed**: the value sequence as a whole (no ghost
model of iteration), and anything about `RANGE_FOR`.

**Defects found before the prover ran** (VERIFY-028, by reading and probing):
F1, `range_len` undefined for spans wider than `ISIZE_MAX` (observed returning
0); F2a/F2b, `range_skip` reversing direction for `n > ISIZE_MAX` and
exhausting wide ranges early; F3, an unguarded `CANON_OPTION(isize)`; F4, a
caller precondition stated with `ensure_msg`. Tracing F1's consumers found
**F5**, a buffer overflow in `vec_extend_from_range` (`data/vec/vec_range.h`)
reachable through the public API. The existing suite passed on all of it.

**The arc.** Run 1, on the unfixed bodies, failed on exactly the goals the
defects falsify and on no others. Run 2 fixed them and falsified VERIFY-028
P6: 11 own residuals, from two causes — a MISRA-motivated wraparound form of
the F1 fix that was correct but opaque to the provers, and the nonlinear
bound `n·|step| ≤ span − 1`. Run 3 — declared in advance as the last
provability rewrite — removed the wraparound and added two lemmas
(`range_mul_le`, `range_div_shift`) that WP proves as goals of their own.
Own residuals: 12 → 11 → 0. **Zero new argument blocks** (P1 held), but not
unaided; VERIFY-029/030 say so and Table 5 carries the footnote.

## data/stringbuf.h (in-place contracts, no driver)

### Summary

| Property               | Value                                          |
|------------------------|-------------------------------------------------|
| **Status**             | Verified (with documented residuals)            |
| **Baseline commit**    | b785d52 (Canon-C CI #1322, first run at the final code state) → name-identical 50045d2 (#1324) → enforced by the commit carrying VERIFY-035 |
| **Functions**          | 29 contracted and proved in place (`stringbuf_printf` is a name alias) |
| **Proof obligations**  | 4767 / 4848 discharged automatically (98.33%)   |
| **Unproved**           | 81 — all inherited (arena.h's 79 + borrow.h's 2 memcmp-danglingness), **0 own**; VERIFY-035; **0 Failed, 0 Invalid, 0 Stepout** |
| **Prover setup**       | Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1        |
| **Frama-C version**    | 29.0 (Copper)                                   |
| **WP flags**           | `-wp -wp-rte -wp-split -wp-timeout 120 -wp-model Typed+Cast` |
| **CI enforcement**     | Yes — pinned `4767 / 4848` + zero Failed/Invalid/Stepout + exact count 81 + by-name roll-call (set equality) |
| **Trusted axiom**      | `vsnprintf` (`__FRAMAC__`-only): `term`, ISO C's guarantee only, true on glibc and musl; `sep`, the `restrict` pair for the format's first byte. The project's third. |
| **MC/DC coverage**     | 98.55% (136/138 condition outcomes, 2 justified — see MCDC-016) |
| **CI artifact**        | `wp-proof-stringbuf`                            |
| **Job runtime**        | ~70 min                                          |

> **Note (2026-10-20, VERIFY-039).** Since a79b69d the job pins 4795 / 4832
> with 37 unproved, all inherited (first run CI #1369, held at CI #1373):
> arena.h's 33 and borrow.h's 4, `borrowed_bytes_eq`'s two memcmp danglingness
> goals and the two at `borrowed_str_eq`'s call to `str_equal` that G13 added
> (7624d8c), true for every admitted call since that repair. The movements
> since VERIFY-035 are the substrate's, each step in the job's pin comment;
> stringbuf.h still has no own residual.

stringbuf.h is **module 19** and the **second prospective test of the
argument-base saturation claim**. VERIFY-031 pre-registered the predictions
(9268da3) before any contract existed. VERIFY-032 scored them after run 2,
VERIFY-033/034 closed the last three goals with a callee fix, and VERIFY-035
enforced.

**What is claimed.** `stringbuf_wf` has five clauses: `data != NULL`,
`len < capacity`, `data[0 .. capacity-1]` writable, the struct is not inside
its own buffer, and `data[len] == '\0'`. The zero state (a `StringBuf z =
{0}`) is a behaviour of every NULL-safe function. Every append claims:

- `failure_unchanged`: on failure the string `data[0 .. len]` is unchanged;
- on success, `wf`, an unchanged prefix, and the exact length for byte
  appends;
- for `append_char`, the stored character.

**Not claimed**:

- byte content of byte or formatted appends (memory.h's `mem_move` assigns
  without a content ensures);
- `strlen(data) == len` (embedded NULs are legal);
- arena provenance;
- that a formatted append succeeds;
- the `%s`-argument half of the `restrict` precondition.

**Defects found before the prover ran** (VERIFY-031, by probe):

- **G1:** a documented-valid stale view, appended back, reached `memcpy`
  with overlapping regions. That is a contract abort in ordinary builds and
  undefined behaviour in the verified configuration.
- **G2:** `vsnprintf` was given the whole remaining capacity, so on musl
  every formatted append fails once more than 2 GiB is free.
- **G3:** the write pass's failure path overwrote the terminator.
- **G5:** an unstated `restrict` precondition.

Writing the contracts found **G6**: the helper's own `@pre` was violated by
its own caller. The suite passed on all of it under ASan and UBSan.

**The arc.** Own residuals ran 22 → 5 → 3 → 0:

- **Run 1**, on the unfixed bodies, reported every prover-visible defect at
  the seam the record named.
- **Run 2** fixed them.
- **Run 3** worked around WP's call-site frame rule with an early return
  and gave `append_n` a range invariant.
- **VERIFY-033** gave `arena_alloc` the frame clauses the header already
  stated for its three reset functions. That closed `init_arena`'s three
  goals and moved seven pins by +20, every new goal proved.

**Zero new argument blocks** (P1 held). G2, a defect only a second libc
reveals, stayed invisible to WP as pre-registered (P6): the axiom is true on
musl because musl's −1 satisfies it vacuously, and the contract cannot
promise success.

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
  checked.h, 3 goals on ptr.h, 5 goals on memory.h, 3 goals on
  arena.h, 10 goals on pool.h, 9 on region.h discharged by CVC5 that neither Alt-Ergo nor Z3 could close.

The practical consequence: **every remaining unproved goal on the
in-place headers (2 on checked.h, 15 on bits.h, 10 on ptr.h, 15 on
slice.h, 43 on memory.h, 89 on arena.h, 113 on pool.h, 112 on region.h)
is demonstrated triple-prover-resistant**. This is a stronger
certification-evidence statement than dual-prover resistance — the goals
are genuinely limited by WP's encoding, integer theory, or stdlib
feature gaps, not by prover strength.

> **Correction (2026-10-20, VERIFY-039).** The counts above are the
> baselines'; at CI #1373 those units pin 2, 12, 6, 8, 31, 33, 34 and 39. And
> not every goal among them was limited by WP's encoding, theory or feature
> gaps: VERIFY-039 found some false for calls the contracts admitted, bits.h's
> four shifted postconditions (G15), ptr.h's two calls and six pointer
> postconditions (G16), slice.h's five libc preconditions (G17), memcmp's
> danglingness at its callers (G13), the handler's pair (G14), the allocators'
> two frames (G18) and seventeen of region.h's own (G19, G20). Each was
> repaired, and every goal pinned unproved now is argued true for every
> admitted call under a block that names its obstacle, the verifier's or a
> clause the specification does not or cannot state; in vec's and the queue's
> units, five clauses whose value C leaves undefined at some admitted calls
> are argued wherever C defines them (see the note under the summary card;
> four since VERIFY-040 round 1 and none since round 2, CI #1375).
> Resistance is also a per-run observation: at CI #1369's first attempt a
> prover proved within the limit a pinned goal of the queue's that the re-run
> did not (G21).

option's 34 residuals are a distinct class and are deliberately *not*
lumped into the triple-prover-resistant set above:
function-pointer-dispatch feature-gap goals (no `calls` clause for an
arbitrary callee; `\valid_function` unimplemented in Frama-C 29), the
same class as region_end's opaque-hook dispatch (VERIFY-011 category 1).
These are also not prover-strength residuals — additional solver power
would not close them — but the limiting factor is WP's missing
indirect-call support rather than its integer or stdlib theory, so they
are characterized as a verifier feature gap rather than as
triple-prover-resistant. error.h carries no residuals at all.

> **Correction (2026-10-20, VERIFY-039 G11, G14 and G19).** Option's own 32,
> like result's 28 and `region_end`'s hook goals, were not a verifier feature
> gap; the other two of option's 34 were the handler's pair, false as written
> (G14). The contracts claimed, for every callback or hook they admitted,
> termination, no exit, writes within their frames and a valid function
> pointer, and a callback that loops, exits, writes outside the frame or is
> not a valid function falsifies each claim, whatever a verifier implements.
> Since G11's and G19's repairs the contracts require a valid function where
> they call one, and make the other claims only where nothing is called. What
> is left of the combinators' is the run-time check that a pointer is a valid
> function, true for every admitted call and unproved because WP 29 does not
> implement `\valid_function`; what is left of `region_end`'s is three goals
> that need the hook rule, which no clause can state of code behind a pointer,
> and two that read the loop counter, which WP loses across the call.

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
| bits.h       | ✅ Verified       | 745/757   | 12 documented residuals at CI #1373, all true (VERIFY-003, VERIFY-039 batch 6); 15 before VERIFY-039 G15 closed three false postconditions |
| compare.h    | ✅ Fully verified | 208/208   | 100% automatic, 0 timeouts         |
| ptr.h        | ✅ Verified       | 1952/1958 | CI #1373's pin, the full run with checked.h's goals; 6 documented residuals, all true: checked.h's 2 and `align_up`'s and `align_down`'s formulas (VERIFY-039's fixed-width block), and `align_padding`'s formula and `ptr_align_padding`'s bound (VERIFY-008 Category 2); 16 before VERIFY-039 G14 and G16 closed the handler's pair, two calls and six pointer postconditions, none of which held for every admitted call; own baseline 1729/1739 with 10 (VERIFY-006), not re-measured after VERIFY-023 |
| types.h      | N/A              |           | Type definitions only              |
| limits.h     | N/A              |           | Constant definitions only          |
| lifetime.h   | ✅ Verified at level 4 only | 4/4 | **Enforced at CI #1275** (VERIFY-021; name-identical #1271/#1273/#1274, pinned 4/4 with 0 unproved). Layout convention plus one `CANON_LIFETIME_DEBUG`-gated function, `canon_lifetime_next_id_`, the single token generator consolidated from eleven private copies at CI #1243–#1245 (5b45aeb → ca909c7). The former N/A disposition — "one function, excluded from every verified configuration" — was retired on 2026-09-02 not by weakening the argument but by satisfying it: rather than annotate an untranslated block, a configuration that TRANSLATES it was added (`frama-c-lifetime`, `CANON_LIFETIME_DEBUG` + `CANON_LIFETIME_NO_ATOMICS`, ladder level 4). **Levels 1–3 remain excluded permanently** — WP has no concurrency model for `atomic_fetch_add_explicit`, `__atomic_fetch_add` or `_InterlockedIncrement64` — and the contract is `#if`-gated on level 4 so it cannot acquire authority over them. Note the asymmetry: the verified path is the only one that is NOT race-free; verification followed the tool's reach, not the risk. Claim is `ensures \result != REGION_ID_STATIC` only. **No `assigns` clause: it cannot be written.** `counter_` is a function-local static and so is not in scope where a function contract is parsed; `assigns \nothing` would be FALSE (the historical defect, two of the eleven superseded copies) and hoisting the variable would verify a different program, so no frame is claimed and WP assumes `\everything` (VERIFY-021 F1, a specification-strength ceiling in the VERIFY-020 F4 family). Instrument integrity only — says nothing about whether callers obey the borrow discipline (issue #7). Zero inherited residuals (closure is `<stdbool.h>` + types.h), so nothing here bears on the composition result. Atomic-ladder deviation: MISRA-DEV-018; concurrency contract: docs/thread-safety.md. MCDC-013 (1/2 outcomes, one platform-dead justification row). Exercised through `lifetime_test.c`, `lifetime_token_test.c`, borrow_test.c and per-container tests. |
| contract.h   | ✅ Annotated      |           | Handler contract used by ptr.h; since VERIFY-039 G14 it states `terminates \false;` and its loop carries no invariant, so no unit pins a handler goal |

### core/ (complete)

| Header       | Status           | Proved    | Notes                                                                  |
|--------------|------------------|-----------|------------------------------------------------------------------------|
| slice.h      | ✅ Verified       | 390/398   | 8 documented residuals at CI #1373, memcmp's danglingness at the four functions that call it, true since VERIFY-039 G13 (VERIFY-008 Category 3); 15 before VERIFY-039 G14 and G17 closed seven that did not hold for every admitted call (VERIFY-007/-012); MCDC-002 closed, MC/DC 54/58 |
| memory.h     | ✅ Verified       | 2842/2873 (predicted) | 31 documented residuals at CI #1373: 14 inherited (checked.h 2, ptr.h 4, slice.h 8) + 17 own (VERIFY-008 Categories 1 to 3, and `mem_is_aligned`'s two under VERIFY-039's block on a clause the specification omits); 43 at VERIFY-023 (VERIFY-008/-012), each step since in the job's pin comment |
| arena.h      | ✅ Verified       | 3517/3550 (predicted) | 33 documented residuals at CI #1373: memory.h's 31, inherited, + 2 own, the zero-content `ensures` of `arena_alloc_zero` and `arena_alloc_aligned_zero`, filed under VERIFY-017's byte-view block since VERIFY-039's batch 5 retired VERIFY-009 Cat 2c; formerly 50 (CI #1350), 79 (VERIFY-033/034), 83 (VERIFY-009/-012/-023); VERIFY-036 and VERIFY-039 closed the rest, each step in the job's pin comment; MCDC-003 |
| pool.h       | ✅ Verified       | 4036/4070 (predicted) | 34 documented residuals at CI #1373: arena.h's 33, inherited, + 1 own, VERIFY-010 Cat 2a's division bound; formerly 51 (CI #1350), 91 (VERIFY-033/034), 95 (VERIFY-010/-012/-023); VERIFY-036 and VERIFY-039 closed the rest, each step in the job's pin comment; MCDC-004 |
| region.h     | ✅ Verified       | 3685/3724 (predicted) | 39 documented residuals at CI #1373: arena.h's 33, inherited, + 6 own, each true for every admitted call (VERIFY-011 Categories 1 and 2, and VERIFY-039's block on what a hook keeps); formerly 73 (CI #1350) with 23 own, 17 of which VERIFY-039 G19 and G20 closed, fourteen of them not holding for every admitted call; 102 (VERIFY-033/034), 106 (VERIFY-011/-012/-023); MCDC-005, whose outcome is reachable through the API (VERIFY-039 G20) |
| scope.h      | N/A              |           | Macro-only header; DEFER expands at call sites, no static inline functions to verify. scope_test.c locks the exit-method table to regression tests. |
| ownership.h  | N/A              |           | Annotation macros expand to T (no behavior); DEFINE_OWNED(T)/DEFINE_BORROWED(T) generate verifiable functions per instantiation but follow the DEFINE_SLICE(T) disposition (VERIFY-007 macro-verification rationale). ownership_test.c covers Widget and Complex instantiations. |

*Every row of the four tables brought to the pins CI #1373 confirmed
(2026-10-20, VERIFY-039, the main pass's docs-only sweep), and bitset given
its row in the data/ table. This note read, from 2026-10-10, that the rows for
arena.h, pool.h and region.h were at CI #1350's pins and the rows for ptr.h,
memory.h, vec, priority_queue.h and stringbuf.h stale; since then every row's
pin has moved but checked.h's, compare.h's, error.h's and lifetime.h's. The
summary card at the top is recomputed from the same pins. The per-header
Summary cards keep their baselines' figures; under the card of each unit whose
pin has moved, a dated note gives its pin at CI #1373. The pins in
`.github/workflows/cmake-multi-platform.yml` are the authority.*

### semantics/ (complete)

| Header       | Status           | Proved    | Notes                                                                  |
|--------------|------------------|-----------|------------------------------------------------------------------------|
| error.h          | ✅ Verified  | 65/65   | 100% automatic, 0 residuals; default Typed model; calibration baseline for the layer |
| option (driver)  | ✅ Verified  | 222/228 | First driver-verified Shape-B module (VERIFY-014); 6 residuals at CI #1373, the run-time checks that a callback is a valid function, unproved because WP 29 does not implement `\valid_function`; 189/223 with 34 when enforced (2 inherited + 32 own), of which VERIFY-039 G11 and G14 closed 28 that did not hold for every admitted callback or were false as written; default Typed model; MCDC-006; cover TU `option_cover.c` |
| result (driver)  | ✅ Verified  | 207/213 | Second driver-verified Shape-B module and first union-typed module (VERIFY-015); 6 residuals at CI #1373, the run-time checks that a callback is a valid function; 185/215 with 30 when enforced (2 inherited + 28 own), of which VERIFY-039 G11 and G14 closed 24 that did not hold for every admitted callback or were false as written; union-model standing hypothesis (all union goals proved); default Typed model; MCDC-007 (clean 28/28); cover TU `result_cover.c` |
| borrow.h         | ✅ Verified  | 2451/2465 | Fourth semantics/ module, second verified in place (VERIFY-016): 24 non-macro functions annotated in place, `DEFINE_BORROWED_SLICE` parked per `docs/vmacros.md` (the slice.h/DEFINE_SLICE structure, first in semantics/); 14 residuals at CI #1373: 10 inherited (slice.h's 8 memcmp-danglingness goals and checked.h's 2) + 4 own (memcmp danglingness at `borrowed_bytes_eq` and at `borrowed_str_eq`'s call to `str_equal`); the 12 danglingness goals true since VERIFY-039 G13's repair and filed under VERIFY-008 Category 3, checked.h's 2 under VERIFY-039's block on fixed-width arithmetic; 19 when enforced (17 inherited + 2 own memcmp-danglingness residuals) — no new residual class; Typed+Cast; verified config `CANON_LIFETIME` off (OWN-001 §7); MCDC-008 (36/38 ceiling post-Commit-9 surface, first named-assert closure) |
| diag.h           | ✅ Verified  | 3050/3058 | Fifth semantics/ module, third verified in place — layer complete (VERIFY-017): 13 functions annotated in place, the four `DIAG_*` call-site macros parked per `docs/vmacros.md`; 8 own libc byte-view residuals at CI #1373 and none inherited, the contract.h handler pair, false as written, having closed at VERIFY-039 G14; 10 when enforced (2 inherited, the handler pair — smallest inherited surface of any residual-carrying header — + 8 own) — no new residual class; Typed+Cast originated by diag.h's own casts; two documented trusted stdio axioms (`-variadic-no-translation`); MCDC-009 (84/86 ceiling, second named-assert closure + first libc-environmental permanent outcome) |

**The semantics/ layer is complete.** diag.h shipped as the fifth and
final semantics/ module (see its section above, VERIFY-017): 13
functions verified in place under the default configuration, with the
four `DIAG_*` call-site macros parked per `docs/vmacros.md` — the
slice.h/borrow.h in-place-surface + parked-family structure. Its run
originated (rather than inherited) the `Typed+Cast` model, carried the
smallest inherited surface of any residual-carrying header (the
contract.h handler pair alone — composability confirmation #7), added
no new residual class, and introduced the project's first **trusted
stdio axioms** — two documented `__FRAMAC__`-gated assumed
specifications whose environmental assumption is cited by both
evidence streams (VERIFY-017 / MCDC-009 outcome 2). Its pinning
chronology (d8566d5/#1132 → 1597a51/#1133 → bb269f9/#1134) also produced the cleanest
methodology validation to date: the 10-goal residual set and both
roll-calls were predicted from goal-by-goal classification before the
pinning run and confirmed 10-for-10 with zero goals outside the
documented set — the residual classes are now demonstrably predictive
across new headers, not merely descriptive. borrow.h preceded it as
the fourth module (VERIFY-016, composability confirmation #6); option
and result shipped as the first two driver-verified Shape-B modules
(VERIFY-014/-015), confirming the driver + cover-TU template end to
end, with result additionally establishing the first union-typed proof
and the first clean Shape-B MC/DC audit (MCDC-007). vec has since
shipped as the third driver-verified module and the first in data/
(VERIFY-018 / MCDC-010 — see its section below); the remaining
Shape-B modules (deque, fold — see `docs/vmacros.md`) follow the same
driver + cover-TU template, with MCDC-007's forward note governing
their audit expectations and VERIFY-018's method lessons (split patch
first; composition before contract-weakening; three-run pinning
discipline) recorded for deque.

> **Correction (2026-10-20).** The 10-goal set was not predicted by name:
> `docs/traceability.md`'s correction of 2026-08-09 records that the committed
> pre-run artifact predicted its shape, and that of the 10 only the 2
> inherited were named in advance. Those 2 were contract.h's handler pair,
> which VERIFY-039 G14 found false as written; they closed at CI #1368.

### data/ (in progress — vec, deque, bitset, priority_queue.h, range.h, stringbuf.h enforced)

| Header       | Status           | Proved    | Notes                                                                  |
|--------------|------------------|-----------|------------------------------------------------------------------------|
| vec (driver) | ✅ Verified  | 5475/5568 (predicted) | Third driver-verified Shape-B module, first data/-layer module, first driver on Typed+Cast (VERIFY-018, enforced CI #1154; ratcheted CI #1202 and CI #1247/43a46b1 for the F4 closure; baseline CI #1152; report-only #1150–#1151): 37 generated functions via the DEFINE_VEC_STRUCTS/FUNCTIONS split (F3); 93 residuals at CI #1375: memory.h's 31 and arena.h's 2, inherited, option's 6 and result's 6 at (Bool, Error), the run-time checks on the callbacks' pointers since VERIFY-039 G11, and 48 own, each filed by name (VERIFY-039 batch 1), `swap`'s proved since VERIFY-040 round 1 and `extend`'s frame stated as postconditions since round 2; 94 at CI #1374; 95 at CI #1373; 184 at VERIFY-035 (5337/5521), 196 at CI #1247, each step since in the job's pin comment: 123 inherited byte-identically (largest TU to date; 91 core = arena.h's set verbatim, 32 option mod prefix) + 73 subject-side (53 own across 4 categories incl. the new macro-body-loop class (g) forward-flagged for deque, + 20 fresh result(Bool, Error) instantiation, the F4 pair having been removed by contract at CI #1247 — VERIFY-018 Correction note 2026-07-16 and DEMONSTRATED note 2026-08-17); zero own fn-pointer-dispatch goals; MCDC-010 (155/158 ceiling, U1/U2 WP-corroborated infeasible + U3 heap-environmental; third attribution variant); facade views measured but not yet WP-driven (follow-up); `_range`/`_fmt` extensions deferred |
| deque (driver) | ✅ Verified  | 1653/1665 | Fourth driver-verified Shape-B module, second data/-layer module, first data/-layer driver on plain **Typed** (VERIFY-019, enforced CI #1238; baseline #1234; name-stable #1237; re-confirmed #1239–#1240): 24 generated functions via the DEFINE_DEQUE_STRUCTS/FUNCTIONS split (VERIFY-018 F3's checklist item, landed CI #1225 with byte-identical expansion verified first); **zero core-substrate inheritance** — the first module whose inherited surface is SMALLER than its predecessor's, composability tested in the converse direction; 12 residuals at CI #1374: option's 6 and result's 6 at (Bool, Error), the run-time checks on the callbacks' pointers since VERIFY-039 G11, and no own, `swap`'s five proved since VERIFY-040 round 1; 17 at CI #1373, 5 of them own, `swap`'s, true and filed under VERIFY-039's block on a struct stored from an initialised local; 67 when enforced: 2 handler + 32 option (inheritance) + 28 fresh result(Bool, Error) + 5 own (swap cluster only); class (g) macro-body-loop **withdrawn** before the run (a ring shifts nothing); **memory-model invariant** (VERIFY-019-M); closed VERIFY-018 F4; MCDC-011 (82/82, 100%, zero justification rows) |
| bitset       | ✅ Verified | 4972/5046 (predicted) | Fifth driver-verified module, third data/-layer module (VERIFY-020; enforced CI #1260, ratcheted CI #1265, gate wired at CI #1285): 32 functions contracted in place behind an interposition driver; 74 residuals at CI #1375: memory.h's 31, bits.h's 12, borrow.h's 4 and option's 6 at `usize`, inherited, + 21 own, each filed by name (VERIFY-039 batch 3), `bitset_init`'s two proved since VERIFY-040 round 1 and `bitset_assign`'s two since round 2; 76 at CI #1374; 78 at CI #1373; 163 when enforced, 71 own, the largest own set among the driver-verified modules then; F4 specification-strength inheritance; MCDC-012 (134/138 since VERIFY-040 round 2, 130/134 before) |
| priority_queue.h | ✅ Verified | 4645/4683 (predicted) | First in-place (Shape A) data/-layer module; its core operation calls a caller-supplied function pointer from inside its loops. Comparator proved as a **verified configuration**: a `calls` clause over compare.h's 24 built-ins makes `pq_cmp_`'s frame a theorem, not an axiom; caller-supplied comparators are outside the proof. Heap order NOT claimed (built-ins ensure only -1..1). 38 residuals at CI #1375: memory.h's 31 + result's 6, the run-time checks on the callbacks' pointers, + 1 own, the comparator's `\valid_function` check, the three copies between a caller's memory and the buffer proved since VERIFY-040 round 2, `regions_overlap` stated by `\separated`; 41 at CI #1373; 77 at VERIFY-035 (4521/4598); 71 when enforced (43 inherited + 22 result + 6 own), two of the six own being same-buffer slot goals, not pointer order, which VERIFY-039 G21 closed. Enforced at CI #1290, name-identical to #1289 (VERIFY-022). MC/DC 81/82, graduated to the per-line allowlist (MCDC-014). |
| range.h      | ✅ Verified | 2644/2652 | Module 18 and the **first prospective test of the saturation claim** (VERIFY-028 pre-registered, VERIFY-029 scored runs 1–2 and pre-registered run 3, VERIFY-030 enforced at CI #1317, name-identical to #1315/#1316). Shape A in place; thin driver interposes a contracted `option_isize` (F3 guard). Exact `range_count` proved over the whole `isize` domain. 8 residuals at CI #1373, **all inherited** (option's 6 run-time checks + checked.h's 2), **0 own**; 36 when enforced (32 option + 2 handler + 2 checked), VERIFY-039 G11 and G14 closing 28. F1/F2a/F2b/F3/F4 found by reading before the prover ran; **F5** a buffer overflow in `vec_range.h` downstream of F1. P1 (zero new arguments) held; P6 failed at run 2 — closed by a no-wraparound rewrite + two proved lemmas. MC/DC 61/62, 1 justified (MCDC-015). |
| stringbuf.h  | ✅ Verified | 4789/4826 (predicted) | Module 19 and the **second prospective test of the saturation claim** (VERIFY-031 pre-registered; VERIFY-032 scored run 2; VERIFY-033/034 closed A2 through arena.h's frame; VERIFY-035 enforced). 29 functions in place, Typed+Cast, no driver, first module with variadic definitions; 37 inherited at CI #1373 (arena.h 33 + borrow.h 4), **0 own**; 81 when enforced (arena.h 79 + borrow.h 2); zero new argument blocks; third trusted axiom (`vsnprintf`). Defects G1/G2/G3/G5/G6 fixed. MCDC-016 (136/138, two environmental rows). |
| hashmap      | Planned          |           | Shape A (confirmed) — in-place surface via `hashmap_impl.h`, no cover TU needed |

### algo/ (longer term)

Algorithms — planned after data/; fold is the one Shape-B entry
(cover TU required), the rest are Shape A via their `_impl.h` bodies.
