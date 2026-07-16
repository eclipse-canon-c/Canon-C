# Requirements Traceability Matrix

## Coverage Record

### Baseline (Branch Coverage)

| Field              | Value                                                        |
|--------------------|--------------------------------------------------------------|
| **Date**           | 2026-04-07                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | 669d6a7                                                      |
| **CI run**         | Canon-C CI #668                                              |
| **CI job**         | coverage                                                     |
| **Branch**         | master                                                       |
| **Compiler**       | GCC 13.3.0                                                   |
| **Build type**     | Debug                                                        |
| **Flags**          | `--coverage -fprofile-arcs -ftest-coverage`                  |
| **Tool**           | gcov (GCC 13.3.0) + lcov                                    |
| **Runner**         | ubuntu-latest (GitHub Actions)                               |
| **Scope**          | Library headers only — test files excluded                   |
| **Test binaries**  | 51                                                           |

### First MC/DC Measurement

| Field              | Value                                                        |
|--------------------|--------------------------------------------------------------|
| **Date**           | 2026-04-07                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | 0a22f76                                                      |
| **CI run**         | Canon-C CI #677                                              |
| **CI job**         | coverage                                                     |
| **Branch**         | master                                                       |
| **Compiler**       | GCC 14.2.0                                                   |
| **Build type**     | Debug                                                        |
| **Flags**          | `--coverage -fcondition-coverage`                            |
| **Tool**           | gcov-14 --conditions (MC/DC) + lcov (branch)                |
| **Runner**         | ubuntu-latest (GitHub Actions)                               |
| **Scope**          | Library headers only — test files excluded                   |
| **Test binaries**  | 51                                                           |

### Current Measurement

| Field              | Value                                                        |
|--------------------|--------------------------------------------------------------|
| **Date**           | 2026-07-12                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | e663e2c                                                      |
| **CI run**         | Canon-C CI #1154                                             |
| **CI job**         | coverage + frama-c                                           |
| **Branch**         | master                                                       |
| **Compiler**       | GCC 14.2.0                                                   |
| **Build type**     | Debug                                                        |
| **Flags**          | `--coverage -fcondition-coverage -DCANON_CHECKED_FORCE_FALLBACK -DCANON_BITS_FORCE_FALLBACK -DCANON_NO_REQUIRE` |
| **Tool**           | gcov-14 --conditions (MC/DC) + lcov (branch)                |
| **Runner**         | ubuntu-latest (GitHub Actions)                               |
| **Scope**          | Library headers + Shape-B cover TUs — test files excluded    |
| **Test binaries**  | 53 (50 test binaries + 3 Shape-B cover TUs `option_cover`, `result_cover`, `vec_cover`; contract_test excluded from the coverage build) |

> Note: this baseline advances from CI #1072 to CI #1089, where the
> second Shape-B cover TU (`vmacros/coverage/result_cover.c`) entered the
> coverage build. The cover TU takes its `CANON_RESULT(int, VErr)`
> instantiation from the WP driver it `#include`s
> (`vmacros/vdrivers/result_verify.h`), so the module's 22 generated
> condition outcomes are attributed to the **driver header** — the file
> containing the `DEFINE_RESULT_FUNCTIONS` expansion site — and the TU's
> own 6 scaffolding outcomes to `result_cover.c`; both paths survive the
> `*/test/*` filter (see MCDC-007, finding 2, including the flagged
> re-audit of option's attribution wording). result's per-module
> attribution check confirmed the Shape-B pattern (result_test.c owns all
> 256 test-measured outcomes; result_impl.h none) before these numbers
> were baselined. All 28 new outcomes are covered — result has no
> MCDC-006-style ceiling (its `require_msg`-only panic surface vanishes
> under `-DCANON_NO_REQUIRE`; MCDC-007 is a clean audit). The aggregate
> moved accordingly: MC/DC 86.2% → 86.4% (1471/1702), branches 86.7%
> (1469/1694), lines 95.8% (2323/2426), functions 99.5% (572/575) — the
> cover TU's scaffolding is fully executed, so unlike option_cover there
> is no never-executed helper and the function percentage does not dip.
>
> This baseline advances again from CI #1089 to CI #1106, where three
> `#ifdef CANON_NO_REQUIRE`-gated tests entered `borrow_test.c` to
> exercise the `_get(NULL)` defensive branches — real, documented
> release-mode behavior ("NULL → safe empty") that survives
> `-DCANON_NO_REQUIRE` and is therefore reachable by design in exactly
> the coverage build. borrow.h's file-level MC/DC moved 35/40 → 38/40
> (87.5% → 95.0%), its documented ceiling under MCDC-008 (the two
> remaining outcomes are the one-NULL guard in `borrowed_bytes_eq`,
> type-invariant-unreachable and WP-discharged dead at CI #1110). The
> aggregate moved accordingly: MC/DC 86.4% → 86.6% (1474/1702),
> branches 86.7% → 86.9% (1472/1694); lines and functions unchanged.
> A per-line MC/DC debug step for borrow.h entered the coverage job in
> the same change as the measurement stream's regression detector.
>
> This baseline advances again from CI #1106 to CI #1122 (197ef8e),
> capturing diag.h's render gap-closure arc (CI #1118–#1120) and the
> per-line diag.h debug step that entered the coverage job alongside
> the `frama-c-diag` WP job at #1122. diag.h's file-level MC/DC moved
> 73.91% (34/46) → 97.67% (84/86): the denominator grew 46 → 86
> because `diag_render` and `diag_render_frame` were entirely uncalled
> before the pass — uncalled `static inline` functions are invisible
> to gcov, so the render pair had been silently excluded rather than
> counted as missed. The gap-closure pass surfaced two
> pre-verification defects on the newly exercised path (a
> contract-violating NULL guard permitting a `snprintf(NULL, >0, …)`
> UB call, and a `-Werror=format-truncation` break on the documented
> truncation path), both fixed before the ceiling was pinned at
> CI #1120 (93fa22c). The two remaining outcomes are diag.h's
> documented ceiling under MCDC-009 — one invariant-dead (the
> diag_push overflow clamp, later WP-discharged via the named
> `dead_by_invariant_clamp` assertion) and one libc-environmental
> (diag_render's `n < 0` encoding-error skip, permanently open by
> design). The aggregate moved accordingly: MC/DC 86.6% → 87.5%
> (1524/1742), branches 86.9% → 87.7% (1520/1734), lines 95.8% →
> 96.1% (2366/2461), functions 572/575 → 574/577 (the render pair
> entering both function denominator and covered set).
>
> This baseline advances again from CI #1122 to CI #1154 (e663e2c),
> capturing the third Shape-B cover TU
> (`vmacros/coverage/vec_cover.c`, landed at 5bfde5b/CI #1146) and
> vec's driver verification arc through enforcement (VERIFY-018,
> enforced at #1154). Unlike result_cover, vec_cover instantiates
> `DEFINE_VEC(static inline, int)` **directly** rather than including
> its WP driver, so the module's generated condition outcomes are
> attributed to **the cover TU itself** — the third attribution
> variant (see MCDC-010). vec_cover.c measures 155/158 (98.10%) from
> its first run, its documented ceiling: the three open outcomes (two
> guard-redundancy-infeasible `!checked_mul` true legs, WP-corroborated
> by branch-complete constructor proofs, plus one heap-environmental
> `!buf` leg) were written down with dispositions before the first
> measurement and confirmed exactly at block level. vec's Shape-B
> attribution check confirmed the pattern (vec_test.c owns all 640
> test-measured outcomes; vec_impl.h shows the
> functions-but-no-conditions fingerprint). The aggregate moved
> accordingly: MC/DC 87.5% → 88.4% (1679/1900), branches 87.7% →
> 87.8% (1542/1756), lines 96.1% → 96.4% (2515/2610), functions
> 574/577 → 621/624 — vec_cover's 47 scaffolding-and-generated
> functions are all executed, so the function percentage holds at
> 99.5%.

### Results

| Metric     | Percentage | Covered    | Total      |
|------------|------------|------------|------------|
| Lines      | 96.4%      | 2515       | 2610       |
| Functions  | 99.5%      | 621        | 624        |
| Branches   | 87.8%      | 1542       | 1756       |
| MC/DC      | 88.4%      | 1679       | 1900       |

arena.h's MC/DC contribution at 90.6% (58/64) is the achievable
ceiling under the documented MCDC-003 unreachability — see the
"Headers at their documented MC/DC ceiling" prose below. The
contribution increased from 57/64 (89.1%) to 58/64 (90.6%) at the
arena.h baseline because CI #962 closed the previously-missing
`arena_try_alloc_aligned` line 510 outcome (the compound-return
`out != NULL && p != NULL` false branch) by adding
`test_try_alloc_aligned_failure` to `test/core/arena_test.c`,
mirroring the non-aligned `arena_try_alloc` test that already
covered line 485. The 6 remaining missed outcomes
on arena.h are documented in MCDC-003: 4 overflow-guard subconditions
that are structurally unreachable under `arena_invariant +
CANON_ARENA_MAX_SIZE = CANON_GB`, plus 2 release-build macro no-op
artifacts on `_arena_debug_update` calls (the same gcov-14
instrumentation behavior documented in MCDC-001's `contract.h 0/2`
pattern).

memory.h's MC/DC contribution rose from 82.0% (105/128) at the
pre-Phase-1 baseline to 88.3% (113/128) at the Phase-1+ACSL baseline.
The 8-outcome improvement comes from the `mem_alloc_array_checked`
refactor — extracting the previously-inlined arithmetic into a
verified primitive function exposes the overflow-detection branches
(via `checked_mul`) directly to the test suite, where 3 dedicated
overflow tests in `test_mem_alloc_array_*` exercise the
fallback-and-error paths that were previously dead-coded inside the
macro expansion. The 15 remaining missed outcomes are defensive
`require_msg` checks under `-DCANON_NO_REQUIRE`, the same coverage
methodology pattern documented in MCDC-001.

pool.h's MC/DC contribution reached 62/68 (91.2%) at its baseline, the
achievable ceiling under MCDC-004. The four wave-1 gap-closure tests
(b2644ba / CI #972) moved pool.h from 55/68 to 61/68, and
`test_init_arena_alloc_fails_after_guard` (98de378 / CI #974) closed the
line-309 reachable gap to reach 62/68 — the gcov dump confirms line 309 at
2/2 condition outcomes. The 6 remaining missed outcomes on pool.h are the
`!pool->arena` defensive subconditions documented in MCDC-004, unreachable
under `pool_invariant`.

option_cover.c's MC/DC contribution is 96.7% (29/30) at this baseline, the
achievable ceiling under MCDC-006 and the first Shape-B (macro-templated)
module to enter the per-file table. option's combinator conditions live in
`IMPL_OPTION_*` macro bodies; measured through `option_test.c` they are
attributed to a `/test/` path and filtered out, so
`vmacros/coverage/option_cover.c` re-instantiates `CANON_OPTION(int)` outside
`test/` to give them a surviving home (see `docs/vmacros.md`). Of
option_cover.c's 30 condition outcomes, 26 are the generated `option_int_*`
combinator conditions and 4 are the cover driver's own scaffolding
(`half_if_even`'s even-check and `observe_opt`'s `get`-result branch, both
covered); the single miss is the FALSE side of `option_int_expect`'s
`has_value` guard — the panic-on-absent contract-violation branch, not
exercised under `-DCANON_NO_REQUIRE` and cross-confirmed unreachable by WP
(VERIFY-014). Adding the cover TU to the coverage build also brings its
scaffolding functions and lines into the project denominators: function
coverage moves to 99.5% (546/549) — the one new uncovered function is the
`is_positive` helper, reachable only via the `filter` F&&_ short-circuit so
its body never runs — and the project MC/DC aggregate to 86.2% (1443/1674).

The result module's MC/DC contribution is 100% (28/28) at this
baseline — the first Shape-B module with a **clean audit** (MCDC-007;
no unreachable outcome exists, because result's `require_msg`-only
panic surface vanishes under `-DCANON_NO_REQUIRE`, unlike option's
handler-invoking `expect`). The 28 outcomes split across two surviving
files: 22 generated `result_int_VErr_*` conditions attributed to
`vmacros/vdrivers/result_verify.h` (the driver header containing the
`DEFINE_RESULT_FUNCTIONS` expansion site the cover TU includes) and 6
cover-driver scaffolding conditions in
`vmacros/coverage/result_cover.c` (`checked_double`'s threshold and
`observe_res`'s two `get_*`-result branches), all covered. result's
per-module attribution check re-confirmed the Shape-B pattern before
baselining: `result_test.c` owns all 256 of the module's test-measured
outcomes, `result_impl.h` none. Unlike option_cover, result_cover adds
no never-executed function to the denominator, so the function
percentage holds.

vec_cover.c's MC/DC contribution is 98.1% (155/158) at this baseline,
the achievable ceiling under MCDC-010 and the third Shape-B module in
the per-file table. vec's conditions live in `IMPL_VEC_*` macro
bodies; measured through `vec_test.c` they are attributed to a
`/test/` path and filtered out, so `vmacros/coverage/vec_cover.c`
re-instantiates `DEFINE_VEC(static inline, int)` outside `test/` —
directly, rather than by including the WP driver, so the generated
conditions attribute to the cover TU itself (the third attribution
variant; see MCDC-010). Of vec_cover.c's 158 condition outcomes, 152
are generated (140 `DEFINE_VEC` conditions + 12 `DEFINE_VEC_SLICE`
facade-view conditions — as_slice/as_slice_full/as_bytes, measured
here rather than lost to the test filter) and 6 are cover-driver
scaffolding (three fill-to-capacity/iterate `while` loops), all
scaffolding covered. The three misses are U1/U2 (the `!checked_mul`
true outcomes in alloc/arena_alloc — guard-redundancy-infeasible,
WP-corroborated by branch-complete constructor proofs, VERIFY-018)
and U3 (heap `!buf` — environmental; the arena analogue IS covered
via deterministic exhaustion, demonstrating the exclusion is
heap-specific). All three were predicted with dispositions before
the first run. vec_cover adds no never-executed function, so the
function percentage holds.

borrow.h's MC/DC contribution reached 38/40 (95.0%) at this baseline,
its achievable ceiling under MCDC-008. The three-outcome improvement
from 35/40 comes from the `CANON_NO_REQUIRE`-gated `_get(NULL)` tests
(the defensive branches are the shipped release-mode contract and are
reachable only where `require_msg` compiles away — exactly the
coverage build, keeping the measured set aligned with the `null_b`
behaviors the ACSL contracts verify). The 2 remaining missed outcomes
are the NULL-true sides of the one-NULL guard in `borrowed_bytes_eq`
(line 758 at this baseline), unreachable under `cbytes_invariant` and
formally discharged dead by the named WP assertion `dead_by_invariant`
at CI #1110 — the first closure carried as a single named proof goal
(MCDC-008, VERIFY-016).

diag.h's MC/DC contribution reached 84/86 (97.67%) at this baseline,
its achievable ceiling under MCDC-009. The 50-outcome improvement from
34/46 (73.91%) comes from the render gap-closure pass (CI #1118–#1120):
`diag_render` and `diag_render_frame` were entirely uncalled before it
and therefore invisible to gcov, so the denominator grew 46 → 86 as
the pair entered measurement, and the pass surfaced (and fixed) two
pre-verification defects on the newly exercised path. The 2 remaining
missed outcomes have **distinct dispositions** — a first for the
ceiling family: the true side of diag_push's overflow clamp
(invariant-dead under `diag_invariant`, later formally discharged by
the named WP assertion `dead_by_invariant_clamp` at CI #1132) and the
true side of diag_render's `n < 0` encoding-error skip
(libc-environmental: unreachable because these formats with valid
arguments cannot provoke a snprintf encoding error on a hosted libc —
permanently open by design, with no WP goal to retire it, sharing its
assumption with VERIFY-017's trusted snprintf axiom). The sibling
encoding-error ternary in `diag_render_frame` is gimplified as value
selection and emits no condition row — confirmed absent from the
denominator, matching prediction (MCDC-009).


### Methodology changes since baseline

**CANON_CHECKED_FORCE_FALLBACK** (added 2026-04-14): Forces the portable
fallback path in checked.h instead of compiler builtins.

**CANON_BITS_FORCE_FALLBACK** (added 2026-04-17): Same rationale for
bits.h — forces the portable fallback paths for popcount, clz, ctz,
and bswap instead of compiler builtins.

**CANON_NO_REQUIRE** (added 2026-04-14): Compiles `require_msg()` to
`((void)0)`, removing NULL-check branches that ACSL `requires \valid(result)`
has already proved unreachable under Frama-C. contract_test is excluded
from the coverage build for the same reason.

**Shape-B cover TUs** (added 2026-06-27; second TU 2026-07-03): The
coverage build compiles the Shape-B macro-module cover translation units
under `vmacros/coverage/` (`option_cover.c`, `result_cover.c`) so the
macro-generated condition outcomes — otherwise attributed to `/test/`
paths and filtered out — are measured against surviving non-test files.
When the cover TU takes its instantiation from the driver `#include`
(result's form), gcov attributes the generated conditions to the
**driver header** (`vmacros/vdrivers/<module>_verify.h`) and only the
scaffolding to the `.c`; both survive the filter (MCDC-007, finding 2).
The cover TUs are compiled with the same `-DCANON_NO_REQUIRE -DNDEBUG`
as the WP run so the measured condition set equals the proof's goal set.
See `docs/vmacros.md`, MCDC-006, and MCDC-007.

### Notes on per-test column attribution

Headers in this table report MC/DC measured at their own source-line
attribution after gcov merges across translation units. When a header
is included transitively — `checked.h` reaches every test that
includes `ptr.h`, `memory.h`, or `slice.h` — gcov attributes
inlined-expanded conditions to each call site in the including TU.
This produces a "50% of N" pattern visible in many per-test columns
of the raw gcov output: at each inlined call site, the success
branch is reachable from typical test inputs but the
overflow/underflow branches require pathological inputs (arrays of
2^62 elements, capacity calculations near `USIZE_MAX`) that unit
tests don't construct.

A notable instance: `priority_queue_test` reports 33.93% of 56 for
`core/primitives/checked.h`. checked.h itself has 96 condition
outcomes total (visible in `checked_test`'s column as 100% of 96).
priority_queue.h's transitive include graph pulls in a subset of
checked.h's functions, contributing 56 of those condition outcomes
to pq_test's TU; pq_test inputs reach 19 of them (the success
branches at the inlined call sites), leaving 37 uncovered (the
overflow/underflow/zero-input branches that require pathological
inputs unit tests don't construct). The aggregate measurement at
the top of the table correctly merges across TUs (checked.h itself
is at 100% via `checked_test`); per-call-site uncovered branches at
transitive call sites are call-site-unreachable defensive code,
analogous to MCDC-002's disposition for slice.h's `!ptr` branches
at the per-function level.

The aggregate MC/DC figures in the "Results" table reflect the
post-merge coverage and are the authoritative numbers. Per-test
column figures in the raw gcov output reflect what each test
reaches in isolation, including transitive inlined-expanded
conditions, and should not be read as per-header coverage gaps.

### Notable per-header results

Headers at 100% MC/DC:

`bits.h`, `checked.h`, `compare.h`, `ptr.h`, `error.h`, `str_view.h`,
`time.h`, `map_impl.h`, `filter_impl.h`, `find_impl.h`, `reverse_impl.h`,
`search_impl.h`, `any_all_impl.h`, `unique_impl.h`

The `checked.h` 100% MC/DC result holds at the new 96/96 condition-outcome
denominator (up from 64/64) following the addition of 12 division and
modulo functions. Each new function adds exactly one zero-check branch
plus, for the signed `_isize` variants, an additional `&&` short-circuit
pair for the `ISIZE_MIN / -1` overflow guard. All new conditions are
exercised by the dedicated `checked_div_*_op`, `checked_mod_*_op`,
`checked_div_isize_mcdc`, and `checked_mod_isize_mcdc` test groups in
`test/core/primitives/checked_test.c`.

Headers at their documented MC/DC ceiling (below 100% by design or
by methodology):

- **slice.h: 93.1% (54/58)** — the achievable ceiling under the
  public-API constraint documented in MCDC-002. The 4 remaining
  outcomes are the `!ptr` left-side of `||` in `bytes_slice` (line
  117), `bytes_skip` (line 130), `str_slice` (line 194), and
  `str_skip` (line 207). All four are unreachable in MC/DC isolation
  through the public API and are formally proved unreachable by
  Frama-C WP under the `bytes_invariant` / `str_invariant` type
  predicates (see VERIFY-007 and the MCDC-002 status update). The
  gcov measurement remains 54/58 because gcov instruments the source
  rather than the proof — the two evidence streams complement each
  other rather than converge.

- **memory.h: 88.3% (113/128)** — the ceiling under the
  `-DCANON_NO_REQUIRE` methodology documented in MCDC-001. The 15
  missed outcomes are defensive `require_msg` checks at the entry of
  alignment, allocation, and copy functions; ACSL `requires` clauses
  provide the static guarantee these checks enforce at runtime, so
  the coverage build removes them and the WP proof discharges the
  same precondition obligations statically. memory.h does not add a
  new MCDC-002-style deviation — its bytes_t/cbytes_t variants
  inherit slice.h's `bytes_invariant`, so the analogous `!ptr`
  branches are discharged by the inherited type invariant rather
  than re-introducing public-API-unreachable code.

- **arena.h: 90.6% (58/64)** — the ceiling under MCDC-003. The 6
  missed outcomes split into two categories: 4 structurally
  unreachable overflow-guard subconditions in `arena_alloc` (line
  346) and `arena_alloc_aligned` (line 401), and 2 release-build
  macro no-op artifacts on `_arena_debug_update` calls (lines 356
  and 411). The overflow-guard cond 0 (`offset > CANON_USIZE_MAX -
  pad`) is unreachable because `arena_invariant` bounds `capacity`
  by `CANON_ARENA_MAX_SIZE = CANON_GB = 2^30` (from limits.h) and
  `pad <= 15`; cond 1 (`offset + pad > CANON_USIZE_MAX - size`) is
  mathematically unreachable while cond 2 (the only reachable
  subcondition) is false, regardless of the constant. WP discharges
  arena_invariant preservation across all alloc paths, providing the
  substrate fact that establishes cond 0's unreachability — the
  cross-stream evidence pattern follows MCDC-002. The 2 macro-artifact
  outcomes are gcov-14 instrumentation behavior on no-op
  preprocessor expansion (same pattern as the `contract.h 0/2`
  entry visible in every test row); not real conditions, not closeable
  by tests. The new `test_try_alloc_aligned_failure` test added at
  CI #962 closed the previous 7th outcome — a real test gap on
  `arena_try_alloc_aligned` line 510 — by exercising the compound
  return when `p == NULL` but `out != NULL`. arena.h's MC/DC
  contribution moved from 89.1% (57/64) to 90.6% (58/64) at this
  baseline; the 6 remaining outcomes are the documented ceiling and
  not counted as a coverage regression.

- **pool.h: 91.2% (62/68)** — the ceiling under MCDC-004. The 6
  missed outcomes are all the same shape: the `!pool->arena` middle
  subcondition of the defensive `if (!pool || !pool->arena || ...)`
  early return in `pool_get` (line 435), `pool_get_const` (464),
  `pool_as_bytes` (541), `pool_reserved_bytes` (557), `pool_reset`
  (597), and `pool_reset_secure` (644). Each is unreachable under
  `pool_invariant`, which entails `arena_invariant(pool->arena)` — a
  valid pool always has a valid arena, and no public path constructs a
  pool with `pool != \null && pool->arena == \null`, so the
  `!pool->arena`-true outcome cannot fire. WP discharges these
  branches as unreachable (placing them in its `Unreachable` count,
  not the residual list); gcov measures 62/68 because it instruments
  the source rather than the proof — the two evidence streams
  complement each other, the same cross-stream pattern as MCDC-002 and
  MCDC-003. pool.h's contribution rose from 61/68 (89.7%) at the
  four-test b2644ba baseline to 62/68 (91.2%) at CI #974 (98de378),
  which added `test_init_arena_alloc_fails_after_guard` to close the
  line-309 reachable gap — a genuine test gap (arena_alloc can fail
  after pool_init's coarse `arena_remaining` guard passes, because
  arena_remaining returns raw `capacity - offset` without the
  alignment pad), not an unreachability. The 6 remaining outcomes are
  the documented ceiling and not counted as a coverage regression.

- **region.h: 95.5% (21/22)** — the ceiling under MCDC-005. The single
  missed outcome is the `!h->fn` FALSE branch of the hook guard in
  `region_end` (line 496), unreachable because `region_register`
  enforces `fn != NULL` and the dispatch loop visits only filled slots,
  so every dispatched hook has a non-NULL `fn`. WP discharges the
  no-NULL-hook property; gcov measures 21/22 because it instruments the
  source rather than the proof — the same cross-stream pattern as
  MCDC-002/003/004. region.h's WP run (VERIFY-011, enforced) carries
  112 residuals, 19 of them on `region_end`'s opaque-hook dispatch
  (the verification boundary documented in OWN-003), distinct from this
  guard branch. The 1 missed outcome is the documented ceiling and not
  counted as a coverage regression.

- **borrow.h: 95.0% (38/40)** — the ceiling under MCDC-008. The 2
  missed outcomes are the NULL-true sides of the one-NULL guard in
  `borrowed_bytes_eq` (`a.bytes.ptr == NULL || b.bytes.ptr == NULL`),
  each requiring a `{NULL, len>0}` cbytes_t — the exact malformed
  value `cbytes_invariant` forbids and `cbytes_from` refuses to
  construct; the guard sits behind three earlier exits (length
  mismatch, zero length, same pointer), so no API path reaches it. WP
  discharges the named in-body assertion `dead_by_invariant` under the
  invariant preconditions (VERIFY-016), the first cross-stream closure
  stated as a single named goal — checked by name in the proof stream
  (frama-c-borrow wrapper) while the coverage stream's per-line debug
  step watches the gcov side; gcov measures 38/40 because it
  instruments the source rather than the proof, the same
  complementary-evidence pattern as MCDC-002 through MCDC-005. The
  three `_get(NULL)` defensive outcomes closed at CI #1106 by
  `CANON_NO_REQUIRE`-gated tests were reachable by design in the
  coverage build (the inverse disposition — a test closure, not a
  deviation; see MCDC-008's description).

- **diag.h: 97.7% (84/86)** — the ceiling under MCDC-009, and the
  first ceiling entry whose two outcomes carry **different
  dispositions**. The clamp outcome (diag_push's
  `d->depth >= DIAG_MAX_FRAMES` true side) is the familiar
  invariant-dead shape — unreachable because the overflow branch above
  it already holds depth at `DIAG_MAX_FRAMES - 1` under
  `diag_invariant` — and is WP-discharged via the named in-body
  assertion `dead_by_invariant_clamp` (proved at CI #1132; the second
  named-assert closure after MCDC-008, with detectors in both streams:
  the coverage job's per-line diag.h debug step and the frama-c-diag
  wrapper's proved-set check, enforced as of #1135). The skip outcome
  (diag_render's `n < 0` true side) is **libc-environmental** — a new
  disposition: unreachable because of the stdio implementation's
  behavior, not any diag invariant, so no WP goal can retire it and it
  is a permanent documented residual rather than a deviation that
  later closes. It is the measurement-stream citation of the same
  hosted-libc assumption VERIFY-017's trusted snprintf axiom makes in
  the proof stream. gcov measures 84/86 because it instruments the
  source rather than the proof or the platform — the
  complementary-evidence pattern, now spanning three kinds of
  unreachability argument.

- **option_cover.c: 96.7% (29/30)** — the ceiling under MCDC-006, and
  the first Shape-B (macro-templated) module to appear in the per-file
  table. option's combinator conditions live in `IMPL_OPTION_*` macro
  bodies; measured through `option_test.c` they are attributed to a
  `/test/` path and filtered out, so `vmacros/coverage/option_cover.c`
  re-instantiates `CANON_OPTION(int)` outside `test/` to give them a
  surviving home (see `docs/vmacros.md`). The 30 outcomes are 26
  generated `option_int_*` combinator conditions plus 4 cover-driver
  scaffolding conditions (`half_if_even`'s even-check and `observe_opt`'s
  `get`-result branch, both covered); the single miss is the FALSE side
  of `option_int_expect`'s `has_value` guard — the panic-on-absent
  contract-violation branch, not exercised under `-DCANON_NO_REQUIRE`
  and cross-confirmed unreachable by WP, which reports `expect`'s handler
  call as a non-terminating recursive call that "must be unreachable"
  (VERIFY-014). Unlike MCDC-002/003/004/005, the unreachability source
  is the panic handler's non-termination, not a type or construction
  invariant — the same family as the `contract.h 0/2` artifact
  (MCDC-001). The 1 missed outcome is the documented ceiling and not
  counted as a coverage regression.

- **result (via `result_cover`): 100% (28/28)** — the first Shape-B
  module with a clean audit (MCDC-007): no unreachable outcome exists.
  result's panic surface (`unwrap`/`unwrap_err`/`expect`/`get_*` NULL
  guards) is `require_msg`-only and vanishes under
  `-DCANON_NO_REQUIRE`, so there is no analogue of option's
  handler-invoking `expect` branch — the same fact that leaves result's
  WP run with no handler-call goals of its own (VERIFY-015): the two
  evidence streams agree the surface is absent from the
  measured/verified configuration. The 28 outcomes are attributed 22 to
  `vmacros/vdrivers/result_verify.h` (the generated
  `result_int_VErr_*` conditions — the expansion site is the driver
  header the cover TU includes) and 6 to
  `vmacros/coverage/result_cover.c` (scaffolding), all covered. Not a
  ceiling claim: 28/28 is full coverage of the full measured set.

- **vec (via `vec_cover`): 98.1% (155/158)** — the ceiling under
  MCDC-010, the third Shape-B entry, and a new disposition mix: no
  panic branch survives into the measured set (vec's
  `require_msg`/`ensure_msg` surface vanishes under
  `-DCANON_NO_REQUIRE`, like result's), yet the audit is not clean —
  the three open outcomes are infeasibility and environment. U1/U2
  (the `!checked_mul` true outcomes in `vec_int_alloc` /
  `vec_int_arena_alloc`) are dead because the preceding
  `capacity <= CANON_VEC_MAX_CAPACITY / sizeof(int)` guard already
  bounds the multiplication; WP corroborates from the proof side —
  both constructors prove branch-complete in the VERIFY-018 baseline,
  so the justification rows carry formal discharges. U3 (heap `!buf`)
  is environmental: heap OOM is not deterministically forcible from a
  portable cover TU, while the arena sibling's `!buf` TRUE leg **is**
  covered (deterministic 64-byte-arena exhaustion), demonstrating the
  exclusion is heap-specific rather than structural. All three were
  written down with dispositions before the first measurement (CI
  #1146) and confirmed exactly at block level. The 158 outcomes are
  attributed 152 to `vmacros/coverage/vec_cover.c` itself (140
  generated `DEFINE_VEC` conditions + 12 `DEFINE_VEC_SLICE`
  facade-view conditions — vec_cover instantiates the macro directly,
  the third attribution variant) and 6 scaffolding loops, all
  scaffolding covered. gcov measures 155/158 because it instruments
  the source rather than the proof or the heap — the
  complementary-evidence pattern. The known F2 exclusion
  (`slice_init` on an items==NULL vec with `[0,0)`, pedantic UB) is
  deliberately unexercised pending the upstream guard fix; its
  condition legs are covered through other inputs, and the
  denominator is expected to move when the F2 PR lands (a planned
  closure, not a regression). The 3 missed outcomes are the
  documented ceiling and not counted as a coverage regression.

Headers absent from the MC/DC table:

`core/primitives/types.h`, `core/primitives/limits.h`,
`core/primitives/lifetime.h`, `core/scope.h`, and `core/ownership.h`
do not appear in the per-header MC/DC table because they have no
measurable condition outcomes under the coverage build. Their content
is exclusively typedefs, constants, and macros that expand at call
sites — no `static inline` functions with their own branches for gcov
to attribute. Runtime evidence comes from `types_test.c`,
`limits_test.c`, `scope_test.c`, and `ownership_test.c` respectively,
which lock the headers' documented behavior to regression tests. The
condition outcomes measured by those tests are attributed to the
calling TU's expansion sites, not to the headers themselves.

`lifetime.h` has no dedicated test file by the same logic — its
content (two typedefs and one constant) makes no claim that could
fail on any conforming C99 target beyond what `types_test.c` already
covers. Lifetime substrate behavior is exercised through
`borrow_test.c` and the per-container tests across all 16 CI configs
under both `CANON_LIFETIME` modes. See verification.md's roadmap
table for the matching N/A disposition.

`core/primitives/contract.h` reports 0.0% MC/DC (0/2 condition
outcomes) consistently across every test column in the per-TU gcov
output. The two condition outcomes are the
`handler ? handler : contract_default_handler` ternary inside
`contract_set_handler`, which selects between a caller-supplied
handler and the default. `contract_set_handler` is exercised only by
`contract_test.c`, which is excluded from the coverage build because
its assertions deliberately violate contracts to test the handler —
and under the coverage build's project-wide `-DCANON_NO_REQUIRE`,
those assertions compile to `((void)0)` and the test cannot fire
(see MCDC-001 for the methodology). The 0/2 is therefore unreachable
under the coverage build's flag set, not a coverage gap.

## Formal verification status

Per-header formal verification state (see `docs/verification.md` for
full per-header detail):

As of 2026-06-13 (CI #1022), VERIFY-012 closed 50 enforced goals across
five verified headers by ACSL precondition alone — no executable change —
by stating `\initialized` as an input precondition where WP can discharge
it. This lowered the slice/memory/arena/pool/region residual counts to the
values below (formerly 23/57/103/127/126). As of 2026-06-27 (CI #1067),
option became the first driver-verified Shape-B module (VERIFY-014); its
row is the driver's WP run over `vmacros/vdrivers/option_verify.h`. As
of 2026-07-03 (CI #1090), result followed as the second driver-verified
module and the first union-typed one (VERIFY-015); its row is the WP run
over `vmacros/vdrivers/result_verify.h`. As of 2026-07-05 (CI #1110,
clean run at 262a503; report-only first at CI #1109; enforced as of
#1111), borrow.h became the fourth semantics/ module verified and the
second verified in place — 24 non-macro functions annotated in place
under the default `CANON_LIFETIME`-off configuration (VERIFY-016),
with `DEFINE_BORROWED_SLICE` parked per `docs/vmacros.md` (the
slice.h/DEFINE_SLICE in-place-surface + parked-family structure,
first in semantics/). As of 2026-07-08 (pinning measurement at
bb269f9, CI #1134, actions run 28967245082; enforced as of 1965b23,
CI #1135),
diag.h became the fifth semantics/ module verified and the third
verified in place, completing the semantics/ layer (VERIFY-017) — its
run originated the `Typed+Cast` model from its own byte-view casts,
carried the smallest inherited surface of any residual-carrying header
(the contract.h handler pair alone), and introduced the project's
first documented trusted stdio axioms; its 10-goal residual set was
predicted by name before the pinning run and confirmed with zero goals
outside the documented set (report-only chronology: d8566d5/#1132 at
300s, 1597a51/#1133 name-stable at 120s). As of 2026-07-12 (baseline at
96dd41d, CI #1152, run 3; report-only chronology 1eeb58c/#1150 →
8a3bb1e/#1151; enforced as of e663e2c, CI #1154), vec became the
third driver-verified Shape-B module and the **first data/-layer
module verified** (VERIFY-018) — the first driver on the `Typed+Cast`
model (vec crosses `void*` boundaries through mem_alloc and
mem_copy/mem_move), instantiated via the `DEFINE_VEC_STRUCTS` /
`DEFINE_VEC_FUNCTIONS` split that landed upstream before the driver
was drafted (finding F3); its 196-goal residual set — the largest to
date, on the largest TU to date — was name-stable across three
consecutive runs before enforcement and decomposes exactly into 121
inherited re-emissions (byte-identical to the substrate's pinned
lists — core arm verbatim vs. arena.h, option arm modulo the
typed_→typed_cast_ prefix) plus 75 subject-side goals: 53 vec-own
across four categories, including the new macro-body-loop class
forward-flagged for deque, and 22 on the fresh result(Bool, Error)
instantiation (VERIFY-018 incl. Correction note 2026-07-16).

| Header     | Functions | Proof obligations | Proved (auto)     | Unproved | Deviation    |
|------------|-----------|-------------------|-------------------|----------|--------------|
| checked.h  | 30        | 1755              | 1753 (99.89%)     | 2        | VERIFY-002   |
| bits.h     | 18        |  761              |  746 (98.03%)     | 15       | VERIFY-003/4 |
| compare.h  | 28        |  208              |  208 (100%)       | 0        | VERIFY-005   |
| ptr.h      | 21        | 1739              | 1729 (99.43%)     | 10       | VERIFY-006   |
| slice.h    | 22        |  390              |  375 (96.15%)     | 15       | VERIFY-007/12|
| memory.h   | 27        | 2862              | 2819 (98.50%)     | 43       | VERIFY-008/12|
| arena.h    | 22        | 3472              | 3383 (97.44%)     | 89       | VERIFY-009/12|
| pool.h     | 19        | 3902              | 3789 (97.10%)     | 113      | VERIFY-010/12|
| region.h   | 12        | 3643              | 3531 (96.92%)     | 112      | VERIFY-011/12|
| error.h    | 4         |   65              |   65 (100%)       | 0        | VERIFY-013   |
| option     | 16        |  223              |  189 (84.75%)     | 34       | VERIFY-014   |
| result     | 17        |  215              |  185 (86.05%)     | 30       | VERIFY-015   |
| borrow.h   | 24        | 2452              | 2433 (99.23%)     | 19       | VERIFY-016   |
| diag.h     | 13        | 3060              | 3050 (99.67%)     | 10       | VERIFY-017   |
| vec        | 37        | 5380              | 5184 (96.36%)     | 196      | VERIFY-018   |
| **Total**  | **310**   | **30127**         | **29439 (97.72%)**| **688**  |              |

The option row is the driver-verified Shape-B module: WP runs over
`vmacros/vdrivers/option_verify.h`, which instantiates the real shipped
`IMPL_OPTION_*` macros at `CANON_OPTION(int)`. Its 223 obligations are
option's own generated-function goals plus the 2 contract.h handler goals
it reaches transitively through `expect`; option does not pull in the
core/ substrate stack. See VERIFY-014. The result row is likewise
driver-verified: WP runs over `vmacros/vdrivers/result_verify.h`,
instantiating `CANON_RESULT(int, VErr)`. Its 215 obligations are
result's own generated-function goals plus the 2 contract.h handler
goals present via the transitive include (definition-presence only —
result's `require_msg`-only panic surface contains no handler call
under `-DCANON_NO_REQUIRE`); result does not pull in the core/
substrate stack. See VERIFY-015. The vec row is the third
driver-verified module: WP runs over `vmacros/vdrivers/vec_verify.h`,
instantiating `DEFINE_VEC(static inline, int)` via the
STRUCTS/FUNCTIONS split. Unlike option/result, vec's TU **does** pull
in the substrate stack — the facade includes slice.h, ptr.h, and
arena.h → memory.h, plus the option and result instantiations the
driver composes — so its 5380 obligations are the largest full-run
figure in the table. Its 121 inherited goals re-emerge
**byte-identically** from seven already-documented families (arena.h
46, option 32, memory.h 20, slice.h-libc 13, ptr.h 6, checked.h 2,
contract.h 2) — the core arm equal to arena.h's pinned list verbatim,
the option arm equal modulo the typed_→typed_cast_ prefix — and its
remaining 22 non-own goals belong to the fresh result(Bool, Error)
instantiation, a new verification subject (VERIFY-015 verifies
(int, VErr)). See VERIFY-018 incl. Correction note 2026-07-16.

**Prover setup**: Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1 (triple-prover,
`-wp-timeout 120`). Of the 688 unproved goals, the 428 in-place-header
goals (including borrow.h's 19 — 17 inherited re-emissions plus 2
memcmp-danglingness goals, VERIFY-016 — and diag.h's 10 — the
contract.h handler pair plus 8 libc byte-view goals at its
memset/memmove sites, VERIFY-017) are demonstrated
triple-prover-resistant and carry written
manual-proof arguments or documented WP feature-gap rationale in
`docs/deviations.md`. The 64 option/result driver-module goals
(option's 34 + result's 30) are a distinct class —
function-pointer-dispatch feature-gap residuals (no `calls` clause
for an arbitrary callee; `\valid_function` unimplemented in Frama-C
29), the same class as region_end's opaque-hook dispatch (VERIFY-011
cat 1), plus each driver's 2 inherited contract.h handler goals —
documented in VERIFY-014 and VERIFY-015. vec's 196 driver-module
goals (VERIFY-018 incl. Correction note 2026-07-16) are 121
byte-identical re-emissions of already-documented families (its
mega-TU pulls the substrate stack *and* the verified option
instantiation; the core arm matches arena.h's pinned list verbatim,
the option arm modulo the typed_→typed_cast_ prefix), 22 goals on the
fresh result(Bool, Error) instantiation (the family's home profile
reproduced 20/20 at a new type pair; 8 assigns goals never emitted
under the driver's lighter contracts; 2 new union get_* mem-access
goals, attribution open — F4), plus 53 vec-own across four categories — 2
allocation-model, 5 element-transfer (frame-only memcopy/memmove
specs), 24 fill macro-body-loop goals (unprovable by construction —
ACSL loop annotations cannot survive macro definition; the first
class where MC/DC is the primary evidence, MCDC-010), and 22
Typed+Cast int↔char bridging goals; notably **zero** vec-own
function-pointer-dispatch goals — the first module without the
OWN-003 class of its own. result additionally carries the union-model
standing hypothesis (no goals; all union-member postconditions proved
under a WP union model the tool itself flags — see VERIFY-015).

**Note on totals**: each per-header row reports that header's own
obligations (substrate goals are counted under their owning header,
not duplicated into downstream rows). The CI WP step for ptr.h
actually reports 1943/1953 because it processes ptr.h's translation
unit including checked.h, but those +214 obligations belong to
checked.h's row, not ptr.h's. Similarly, memory.h's CI step processes
memory.h with ptr.h, slice.h, checked.h, and contract.h all
transitively included; the 2862 figure for memory.h reflects the
full run, but is shown here as memory.h's row because memory.h is
the verification target. arena.h's CI step processes arena.h with
memory.h and the full transitive substrate; the 3472 figure for
arena.h likewise reflects the full run as arena.h's row. pool.h's CI
step processes pool.h with arena.h and the full two-hop transitive
substrate; the 3902 figure for pool.h likewise reflects the full run
as pool.h's row. region.h's CI step processes region.h with arena.h
and the full two-hop transitive substrate; the 3643 figure for
region.h likewise reflects the full run as region.h's row. 89 of
region.h's 112 unproved goals are re-emerged residuals already
documented under VERIFY-002/006/007/008/009 — byte-identical to
arena.h's full 89-goal residual surface (see VERIFY-011 for the
inheritance table). 89 of pool.h's 113 unproved goals are re-emerged
residuals already documented under VERIFY-002/006/007/008/009 —
byte-identical to arena.h's full 89-goal residual surface (see
VERIFY-010 for the inheritance table). 43 of arena.h's 89 unproved
goals are re-emerged residuals already documented under
VERIFY-002/006/007/008 — byte-identical to memory.h's full 43-goal
residual surface (see VERIFY-009 for the inheritance table). 23 of
memory.h's 43 unproved goals are likewise re-emerged residuals already
documented under VERIFY-002/006/007 (see VERIFY-008). option's 34
unproved goals are 2 inherited from contract.h (VERIFY-006 cat 4) plus
32 option-own combinator function-pointer-dispatch residuals (see
VERIFY-014). borrow.h's CI step
processes borrow.h with slice.h and checked.h transitively included;
the 2452 figure for borrow.h likewise reflects the full run as
borrow.h's row. 17 of borrow.h's 19 unproved goals are re-emerged
residuals already documented under VERIFY-002/006/007 — slice.h's
full 15-goal surface plus checked.h's pair, byte-identical (see
VERIFY-016 for the inheritance table). diag.h's CI step processes
diag.h with types.h, contract.h, and error.h transitively included
(error.h is VERIFY-013 clean and contributes nothing); the 3060 figure
for diag.h likewise reflects the full run as diag.h's row. 2 of
diag.h's 10 unproved goals are the re-emerged contract.h handler pair
(VERIFY-006 cat 4) — the smallest inherited surface of any
residual-carrying header; the 8 diag-own goals are the libc byte-view
class (see VERIFY-017). vec's CI step processes the driver TU with the
full substrate (slice.h, ptr.h, arena.h → memory.h and below) plus
the composed option/result instantiations; the 5380 figure for vec
reflects the full run as vec's row because the driver is the
verification target. Its 121 inherited goals propagate **byte-identically**, exactly as in
the in-place stack: the 89-goal core arm is equal to arena.h's pinned
residual list verbatim (no prefix map applies — both units run
Typed+Cast; `-wp-split` fragment indices and the Timeout/Unknown
sub-verdicts reproduced) and the 32-goal option arm is equal modulo
the `typed_` → `typed_cast_` prefix, all fragment indices included.
The 22 result(Bool, Error) goals are not inheritance but a fresh
instantiation — a new verification subject whose residual profile
matches the family's home profile at clause-family granularity (see
VERIFY-018's byte-identity note and Correction note 2026-07-16).

The slice.h baseline (367 / 390) carries a higher residual fraction
(5.9%) than the previously verified primitives headers because slice.h
is the first Canon-C header that crosses the C standard library
boundary — its equality functions call `memcmp` and `str_from_cstr`
calls `strlen`, both of which have ACSL contracts requiring
initialization and danglingness reasoning that Frama-C 29 has not yet
implemented (see VERIFY-007 for the full WP warning text). The 15
slice.h residuals are not specific to slice.h's design; future headers
that use these libc functions will incur the same residual category.

The memory.h baseline (2805 / 2862) is the first Canon-C verification
round where inherited residuals from substrate headers exceed
memory.h-own residuals — 23 of 43 are byte-identical to goals already
documented in VERIFY-002/006/007, re-emerging because memory.h
includes those headers transitively. The 20 memory.h-own residuals
fall into three categories that are each rooted in a Frama-C feature
gap (\fresh/\freeable allocation reasoning, bitwise-alignment integer
theory, memcmp call-site danglingness — the initialization sub-class was
closed in VERIFY-012). All 20 are
documented in VERIFY-008 with manual-proof arguments. The 23:20
inherited:own ratio is the first quantitative data point for the
composable-verification thesis: substrate residuals propagate without
amplification across composition layers.

The arena.h baseline (3369 / 3472) extends the composable-verification
thesis to a third composition layer. arena.h includes memory.h, which
transitively includes ptr.h, slice.h, checked.h, and contract.h —
making arena.h the first downstream header to observe propagation
through *two* layers of composition. arena.h's 43 inherited residuals
are byte-identical to memory.h's full residual surface (memory.h's 23
inherited + memory.h's 20 own = 43 total) — meaning memory.h's own
residuals propagate as a unit just like the headers below it. The 46
arena.h-own residuals fall into four categories: 8 ptr_span call-site
preconditions at arena_alloc / arena_alloc_aligned (downstream of
VERIFY-006's empty `nonnull` behaviors), 26 arithmetic-chain residuals
at the same allocators (the deepest residual class, arising from the
deliberate readable form of the `arena_can_fit` predicate), 10
wrapper-delegation residuals on the zero/try variants, and 2
arena_free_bytes helper call-site residuals. All 46 are documented
in VERIFY-009 with manual-proof arguments. The 43:46 inherited:own
ratio confirms the composable-verification claim at the third
composition layer.

The pool.h baseline (3775 / 3902) extends the composable-verification
thesis to a fourth composition layer and is the first Canon-C header to
observe residual propagation across a two-hop transitive include.
pool.h includes arena.h, which transitively includes memory.h, ptr.h,
slice.h, checked.h, and contract.h. pool.h's 89 inherited residuals
are byte-identical to arena.h's full residual surface (arena.h's 43
inherited + arena.h's 46 own = 89) — meaning arena.h's own residuals
propagate as a unit just like the headers below it, now across two
transitive hops. The 24 pool.h-own residuals fall into four categories:
5 pool_invariant-establishment arithmetic residuals at pool_init (3
LIMITATION-SUSPECTED pending a manual invariant review), 6
ptr_elem-cascade residuals at pool_alloc / pool_get / pool_get_const
(downstream of VERIFY-006's empty `nonnull` behaviors), 5 bytes_from /
mem_zero call-site residuals on pool_alloc_zero / pool_as_bytes /
pool_reserved_bytes, and 8 arena-delegation + reset-wrapper residuals
on pool_alloc_zero / pool_reset / pool_reset_secure. All 24 are
documented in VERIFY-010 with manual-proof arguments. The 89:24
inherited:own ratio is the most lopsided in the core/ stack — pool.h
sits atop the deepest substrate verified to date while adding the
thinnest own surface, because it reserves its region once at pool_init
and computes slots by fixed-stride ptr_elem, so arena.h's 26-goal
per-allocation arithmetic-chain residual class does not recur.

The region.h baseline (3517 / 3643) extends the composable-verification
thesis to a fifth composition layer. region.h includes arena.h, which
transitively includes memory.h, ptr.h, slice.h, checked.h, and
contract.h — a two-hop transitive include, the same depth as pool.h.
region.h is a *sibling* of pool.h at the arena.h layer (both include
arena.h; neither includes the other), so its inherited surface is
arena.h's *total* — VERIFY-009's 43 inherited + 46 arena.h-own = 89 —
re-emitted byte-identically, with zero new substrate residuals
introduced at the region.h boundary. The 23 region.h-own residuals
fall into two categories: 19 region_end opaque-hook-dispatch residuals
(WP cannot reason about the indirect call through the caller-supplied
`h->fn` pointer — no `calls` clause is possible for an arbitrary hook,
and `\valid_function` is unimplemented in Frama-C 29; this is the
documented verification boundary, see OWN-003) and 4 region_invariant
re-establishment residuals on the trivial mutators (region_begin,
region_attach_arena, region_register, region_set_parent), which carry
arena_invariant's composition weight at the postcondition. All 23 are
documented in VERIFY-011 with manual-proof arguments. region.h's WP run
is **enforced** at this baseline (3531 / 3643 proved, exactly 112
unproved): the residual set is name-stable across the VERIFY-012
initialization closures — those 14 goals left the inherited surface
(126 → 112) without disturbing the 23 region-own goals — so the
exact-count + by-name CI gate was promoted from report-only as of
CI #1022 (the 19 region_end `-wp-split` fragments near the 120s
boundary all reproduce by name). The 89:23 inherited:own
ratio confirms the composable-verification claim at the fifth
composition layer, and 19 of the 23 own residuals concentrate on the
single function (region_end) that region.h's header comment names as
the verification boundary — the WP boundary landed exactly where the
design predicted it. With region.h verified, the core/ layer is
complete (every core/ header is verified or justified-N/A).

The option baseline (189 / 223) is the first **driver-verified** and
first **Shape-B** module, and the first semantics/ header to carry
residuals (error.h, VERIFY-013, proved 65/65 clean). WP runs over the
driver `vmacros/vdrivers/option_verify.h` rather than a shipped header,
because option's functions exist only as `IMPL_OPTION_*` macro bodies
until `CANON_OPTION(int)` instantiates them. The 34 residuals are 2
inherited from contract.h (the handler non-termination pair, VERIFY-006
cat 4 — option reaches contract.h through `expect`) plus 32 option-own
across the six combinators that dispatch a caller-supplied function
pointer (`map`, `and_then`, `filter`, `combine_with` — 6 each;
`or_else`, `eq` — 4 each). The own residuals are a function-pointer-
dispatch feature gap — no `calls` clause for an arbitrary callee,
`\valid_function` unimplemented in Frama-C 29 — the same limitation as
region_end's opaque-hook dispatch (VERIFY-011 cat 1), not the SMT
integer/libc residual classes of the core/ stack. Every non-combinator
function proved fully. option uses the default `Typed` model (no void*
casts), like error.h. All 34 are documented in VERIFY-014 with manual-
proof arguments. option's WP run is enforced (exact-count + by-name
roll-call) as of CI #1067.

The vec baseline (5184 / 5380) is the first **data/-layer**
verification and the composable-verification thesis's first test on a
**mega-TU**: the driver composes the full core/ substrate (through
arena.h), the verified option instantiation (option_verify.h
re-included), a fresh result(Bool, Error) instantiation, and vec's
own 37 generated functions in one translation unit — the largest
verified to date. The decomposition held exactly and was name-stable
across three consecutive runs before enforcement, and the inherited
surface propagates **byte-identically**, exactly as the in-place
stack exhibits (VERIFY-009/-010/-011): the 89-goal core arm equals
arena.h's pinned list verbatim (both units Typed+Cast — no prefix map
applies; fragment indices and Timeout/Unknown sub-verdicts
reproduced) and the 32-goal option arm equals its home list modulo
the `typed_` → `typed_cast_` prefix. The checked/align goals are the
pinned home residuals of checked.h and ptr.h re-emitted unchanged —
no goal in the unit flips verdict relative to its home (this corrects
the record's original "model-variant subset" reading; VERIFY-018
Correction note 2026-07-16). The 22 result(Bool, Error) goals are a
fresh instantiation — a new verification subject reproducing the
family's home profile at clause-family granularity (inherited:own
therefore reads 121:75; originally recorded 143:53). The 53 vec-own residuals introduced one genuinely new class — the
24-goal fill macro-body-loop cluster, unprovable by construction
because ACSL loop annotations cannot survive macro definition — for
which the coverage stream carries the primary evidence (all three
fill legs exercised, MCDC-010), inverting the usual
cross-stream direction. vec's WP run is enforced (pinned Proved line
+ zero-Failed + exact count + by-name roll-call = set equality) as of
CI #1154, green on the first enforced run while prover attribution,
source line numbers, and roll-call order all varied beneath the
name-only gates.

The checked.h baseline grew by 214 proof obligations (1541 → 1755) when
the division and modulo functions were added. All 214 new obligations
were discharged automatically — most by Qed, with smaller contributions
from Alt-Ergo and from WP's structural categories (Terminating /
Unreachable). The two manually-discharged goals are unchanged from
the previous baseline.

### Cross-stream evidence: MCDC-002 through MCDC-008 closures

slice.h's MC/DC ceiling at 93.1% (54/58) and slice.h's WP discharge
of the four MCDC-002 functions form complementary certification
evidence. gcov reports the four `!ptr` defensive branches as uncovered
because no test reaches them through the public API. WP proves the
same four branches are unreachable for any caller satisfying the
type invariant. The gcov measurement is honest about test coverage;
the WP discharge is honest about formal reachability. Together they
satisfy DO-178C's intent for "deactivated code" or "defensive code
unreachable in normal operation" without removing the defensive
checks: the verification framework provides the unreachability
evidence the testing framework cannot. See MCDC-002 in
`docs/deviations.md` for the formal closure record.

arena.h's MC/DC ceiling at 90.6% (58/64) and the MCDC-003 closure
follow the same cross-stream pattern. gcov reports 4 overflow-guard
subconditions and 2 release-build macro-artifact outcomes as
uncovered. The 2 macro-artifact outcomes are gcov-14 instrumentation
behavior on no-op preprocessor expansion (same pattern as `contract.h
0/2`), not real conditions. The 4 overflow-guard outcomes are
structurally unreachable: WP verifies that `arena_invariant` is
preserved by every public function (substrate fact discharged via
VERIFY-009), and `arena_invariant` combined with `CANON_ARENA_MAX_SIZE
= CANON_GB = 2^30` (from `core/primitives/limits.h`) yields the
unreachability of cond 0 at lines 346 and 401. Cond 1's unreachability
is purely mathematical — independent of any constant — and falls out
of usize arithmetic alone, so WP does not need a specific discharge
for it. The same DO-178C disposition applies: the source-level
defense-in-depth code is preserved (the overflow guards document the
contract's safety boundary and keep the implementation robust against
future invariant relaxations) while the verification framework
provides the unreachability evidence the testing framework cannot.
See MCDC-003 in `docs/deviations.md` for the formal closure record.

pool.h's MC/DC ceiling at 91.2% (62/68) and the MCDC-004 closure are
the fourth instance of the same cross-stream pattern. gcov reports 6
`!pool->arena` defensive subconditions as uncovered; WP discharges them
as unreachable under `pool_invariant` (which entails
`arena_invariant(pool->arena)`), placing them in its `Unreachable`
count rather than the residual list. The line-309 reachable gap pool.h's
audit uncovered — `arena_alloc` returning NULL after pool_init's coarse
`arena_remaining` guard passes, because `arena_remaining` returns raw
`capacity - offset` without the alignment pad — was a genuine test gap,
not an unreachability, and was closed by a targeted boundary test
(`test_init_arena_alloc_fails_after_guard`, CI #974) rather than
documented as a deviation. See MCDC-004 in `docs/deviations.md` for the
formal closure record and the full reachable-gap closure history.

region.h's MC/DC ceiling at 95.5% (21/22) and the MCDC-005 closure are
the fifth instance of the same cross-stream pattern, with one wrinkle
worth stating explicitly. gcov reports the single `!h->fn` FALSE branch
of region_end's hook guard (line 496) as uncovered. Its unreachability
rests not on an ACSL *type invariant* — as MCDC-002/003/004 do via
`bytes_invariant` / `arena_invariant` / `pool_invariant` — but on the
register/dispatch *runtime invariant*: `region_register` enforces
`fn != NULL` as a precondition and increments `num_hooks` only after
storing a non-NULL `fn`, and region_end's loop visits only filled slots
`[0, num_hooks)`, so every dispatched slot has a non-NULL `fn`. The
evidence shape is identical (gcov measures source-level uncoverage; the
property establishes the branch dead), but the source of the guarantee
is the API's construction discipline rather than a named predicate — so
an auditor should not expect a "region_invariant discharges this branch"
claim, because the guarantee is structural, not predicate-borne.
Separately, region_end *does* carry WP residuals (VERIFY-011 category 1,
the 19 opaque-hook-dispatch goals), but those are on the indirect-call
obligations, a distinct concern from this guard branch — the guard
branch is a structural defensive outcome, not a WP residual. See
MCDC-005 in `docs/deviations.md` for the formal closure record.

option_cover.c's MC/DC ceiling at 96.7% (29/30) and the MCDC-006 closure
are the sixth instance of the cross-stream pattern, and the first on a
Shape-B (macro-templated) module measured through a cover TU. gcov
reports the single `!o.has_value` FALSE branch of `option_int_expect` as
uncovered. Its unreachability rests not on a *type invariant* — option
has no `option_invariant`, and the by-value `{has_value, value}` tagged
struct admits no malformed state to guard against — but on the
**contract-violation discipline**: `expect`'s absent-path is a panic,
deliberately unexercised under `-DCANON_NO_REQUIRE`. WP cross-confirms
from the proof side, reporting `expect`'s handler call as a
non-terminating recursive call that "must be unreachable" (VERIFY-014).
The evidence shape is identical to MCDC-002 through MCDC-005 (gcov
measures source-level uncoverage; a separate stream establishes the
branch dead), but the source of the guarantee is the panic handler's
non-termination, not a named predicate or a construction invariant — so
an auditor should not expect an invariant-discharge claim. This is the
same family as MCDC-001's `contract.h 0/2` artifact, here surfaced
inside a generated combinator function. See MCDC-006 in
`docs/deviations.md` for the formal closure record.

memory.h does not extend the MCDC-002 list because its bytes_t/cbytes_t
variants inherit slice.h's `bytes_invariant` — the analogous `!ptr`
defensive branches are discharged by the inherited invariant rather
than introducing new public-API-unreachable code paths. arena.h's
MCDC-003, pool.h's MCDC-004, and region.h's MCDC-005 are each a
*different* unreachability shape than slice.h's MCDC-002 (overflow-guard
subconditions; a `!pool->arena` arena-validity disjunct; and a `!h->fn`
hook-slot guard, respectively, vs. slice.h's `!ptr` defensive branches),
but all share the same cross-stream evidence pattern (gcov measures
source, the proof establishes unreachability). With the core/ layer
complete, option's MCDC-006 is the first semantics/-layer and first
Shape-B entry, carrying a sixth unreachability shape (a generated
panic/contract-violation branch). The remaining headers that introduce
their own public {ptr, len} types or analogous unreachable defensive
code live in data/ and util/ — stringbuf.h remains the next candidate
(MCDC-002 lists it provisionally at 74.2%). result's audit (MCDC-007)
followed as the second Shape-B entry and the first **clean** one: no
unreachable outcome exists, because result's `require_msg`-only panic
surface vanishes under `-DCANON_NO_REQUIRE` — so unlike MCDC-002
through MCDC-006 there is no branch for a second stream to establish
dead. Its cross-stream content is instead the alignment of the measured
set with the proof set (28 measured outcomes, no panic branches; WP's
run has no handler-call goals of result's own), plus the runtime
execution of every union read under its matching `is_ok` guard as
operational support for VERIFY-015's union-model hypothesis. The
remaining Shape-B cover TUs (deque, fold — vec's landed as MCDC-010)
follow the same pattern; MCDC-007's forward note sets their audit
expectations — a module's panic-surface routing determines whether it
lands as a ceiling entry (MCDC-006's shape) or a clean audit
(MCDC-007's shape), and each still gets its own per-module MCDC
analysis. vec's MCDC-010 refined the taxonomy: a require_msg-only
panic surface guarantees no *panic-branch* ceiling, but infeasible
and environmental outcomes can still produce one.

borrow.h's MCDC-008 is the seventh closure entry and the first in
semantics/ on an in-place-annotated header. Its unreachability shape is
MCDC-002's nearest relative — a `ptr == NULL` disjunct dead under a
slice.h type invariant (`cbytes_invariant`, here guarding
`borrowed_bytes_eq`'s compare path rather than a slicing entry) — but
its closure mechanics are new in two ways. First, the proof-side
evidence is a **named in-body assertion** (`dead_by_invariant:
\false`) that WP discharges directly, rather than unreachability
inferred from invariant-preservation proofs: the closure is a single
goal the proof stream checks by name every run. Second, the closure
therefore has a regression detector in **both** streams — the coverage
job's per-line debug step (measurement) and the frama-c-borrow
wrapper's proved-set check (proof) — where prior entries had only the
gcov-side detector. gcov still measures 38/40 because it instruments
the source rather than the proof; the complementary-evidence pattern
is unchanged. The same commit-pair also demonstrates the inverse
disposition cleanly: borrow.h's three `_get(NULL)` defensive outcomes
were *closed by tests* (CANON_NO_REQUIRE-gated, CI #1106) because the
coverage build is precisely the configuration where those branches are
the shipped behavior — reachable-by-design branches get tests,
invariant-dead branches get proofs. See MCDC-008 in
`docs/deviations.md` for the formal closure record.

diag.h's MCDC-009 is the eighth closure entry, the second carried by a
named in-body assertion, and the first whose two open outcomes carry
**different dispositions**. Outcome 1 (the diag_push overflow clamp)
extends MCDC-008's mechanics — `dead_by_invariant_clamp: \false`
discharged directly by WP, detectors in both streams (the coverage
job's per-line diag.h debug step and the frama-c-diag wrapper's
proved-set check, enforced as of #1135) — with a proof that is itself
informative: on the overflow path the contradiction depends on
`d->depth` surviving the byte-level memmove, so the discharged goal
demonstrates exactly where the Typed+Cast byte-view gap ends (depth
framing holds) while the frame-content goals at the same call site
remain VERIFY-017 residuals. Outcome 2 (diag_render's `n < 0`
encoding-error skip) opens a **new disposition flavor**:
libc-environmental unreachability, resting on the stdio
implementation's behavior rather than any diag invariant — no WP goal
can ever retire it, so it is a permanent documented residual, and it
is the measurement-stream citation of the same hosted-libc assumption
VERIFY-017's trusted snprintf axiom makes in the proof stream (one
assumption, two records, each pointing at the other). Future
ceiling-family entries should classify against three shapes, not two:
test-closable, invariant-dead (proof-closable), and environmental
(permanent). See MCDC-009 in `docs/deviations.md` for the formal
record, including the denominator-growth history and the two defects
the gap closure surfaced.

vec's MCDC-010 is the ninth closure entry, the third on a Shape-B
cover TU, and the first whose proof-side corroboration is a
**branch-complete function proof** rather than a named assertion or
an invariant-preservation inference: `vec_int_alloc` and
`vec_int_arena_alloc` carry zero unproved branch goals in the
VERIFY-018 baseline, formally establishing the `!checked_mul` true
outcomes (U1/U2) dead under the preceding capacity guard — the same
branches gcov measures uncovered. The disposition is
guard-redundancy infeasibility (proof-closable in MCDC-009's
three-shape classification), with an upstream option recorded: drop
one of the two redundant guards and the rows convert to ordinary
covered outcomes. U3 (heap `!buf`) joins MCDC-009 outcome 2's
environmental family in a heap-OOM flavor, with in-measurement
contrast evidence: the arena analogue's `!buf` TRUE leg is covered
deterministically, so the exclusion is demonstrably about heap
non-determinism, not condition shape. MCDC-010 also completes the
Shape-B attribution-variant set (option: cover TU direct; result:
driver header; vec: cover TU direct with the facade views measured
alongside) and cuts the reverse cross-stream link for the first time:
VERIFY-018's category (g) — fill's 24 macro-body-loop goals,
unprovable by construction — cites the coverage stream as its
*primary* evidence, all three fill legs exercised. See MCDC-010 in
`docs/deviations.md` for the formal record.

### History

| Date       | Commit  | CI Run | Version | Lines  | Functions | Branches | MC/DC  | Notes                                                                    |
|------------|---------|--------|---------|--------|-----------|----------|--------|--------------------------------------------------------------------------|
| 2026-04-07 | 669d6a7 | #668   | v1.3.0  | 95.9%  | 99.6%     | 74.3%    | —      | Baseline — branch coverage only                                         |
| 2026-04-07 | 0a22f76 | #677   | v1.3.0  | 95.9%  | 99.6%     | 74.3%    | 74.3%  | First MC/DC measurement (GCC 14.2.0)                                    |
| 2026-04-14 | 24c69cc | #739   | v1.3.0  | 95.5%  | 99.6%     | 83.9%    | 83.1%  | CANON_NO_REQUIRE + CANON_CHECKED_FORCE_FALLBACK flags added             |
| 2026-04-17 | 972eb6c | #743   | v1.3.0  | 95.5%  | 99.6%     | 84.0%    | 83.1%  | checked_mul_isize refactor; checked.h → 100% MC/DC; WP artifact upload  |
| 2026-04-17 | 7efd1c7 | #752   | v1.3.0  | 95.6%  | 99.6%     | 84.2%    | 83.3%  | bits.h ACSL contracts; WP 746/761; CANON_BITS_FORCE_FALLBACK added       |
| 2026-04-18 | 2f33389 | #761   | v1.3.0  | 95.6%  | 99.6%     | 84.2%    | 83.3%  | compare.h ACSL contracts; WP 208/208 (100%); Typed+Cast model            |
| 2026-04-23 | debb202 | #795   | v1.3.0  | 96.1%  | 99.6%     | 84.3%    | 83.5%  | ptr.h ACSL contracts; WP 1729/1739; triple-prover with CVC5 1.2.1        |
| 2026-04-27 | c3df659 | #804   | v1.3.0  | 95.7%  | 99.6%     | 84.6%    | 83.8%  | checked.h div/mod added (12 functions); WP 1753/1755; 100% MC/DC held. ptr.h CI step now reports 1943/1953 (full-run figure including checked.h substrate); ptr.h's own baseline 1729/1739 unchanged. |
| 2026-05-02 | 1e0d0fe | #821   | v1.3.0  | 95.7%  | 99.6%     | 85.2%    | 84.5%  | slice.h: ACSL contracts + new branch-isolation tests for MCDC-002 closure (+9 branches, +11 MCDC outcomes covered with no denominator growth — same code, more tests); MC/DC 93.1% ceiling (MCDC-002); WP 367/390 (VERIFY-007); MCDC-002 WP-discharged |
| 2026-05-09 | b3e668b | #841   | v1.3.0  | 95.7%  | 99.6%     | 85.5%    | 84.8%  | memory.h: Phase 0 tests + Phase 1 refactor (mem_alloc_array_checked) at 4baf5c6 (CI #840), then ACSL contracts + WP enforcement YAML at b3e668b (CI #841); WP 2805/2862 (VERIFY-008); 88.3% MC/DC; 31 inherited + 26 own residuals. |
| 2026-05-24 | f53bddb | #962   | v1.3.0  | 95.6%  | 99.6%     | 85.9%    | 85.3%  | arena.h: ACSL contracts + WP enforcement YAML; WP 3369/3472 (VERIFY-009); MC/DC 90.6% ceiling (MCDC-003); 57 inherited (memory.h's full residual surface) + 46 own (8 ptr_span call-sites + 26 fits/does_not_fit arithmetic chain + 10 wrapper delegation + 2 free_bytes helpers). test_try_alloc_aligned_failure closed the line 510 compound-return gap (+1 MC/DC outcome, +1 branch covered). Composable-verification thesis confirmed at third composition layer: arena.h inherits memory.h's full 57-goal residual surface byte-identically. |
| 2026-05-29 | 98de378 | #974   | v1.3.0  | 95.6%  | 99.6%     | 86.6%    | 86.1%  | pool.h: ACSL contracts + WP enforcement (WP 3775/3902, VERIFY-010; 103 inherited = arena.h's full residual surface + 24 own across cats 2a/2b/2c/2d), first two-hop transitive include; MC/DC 91.2% ceiling (MCDC-004, 6 `!pool->arena` outcomes unreachable under pool_invariant). Gap-closure: four wave-1 tests (b2644ba/#972) 55→61/68; test_init_arena_alloc_fails_after_guard (98de378/#974) closed the line-309 reachable gap 61→62/68 (gcov confirms L309 2/2). Project MC/DC 1401→1413 of 1642, branches 1437→1448 of 1672. Composable-verification thesis confirmed at fourth composition layer; 103:24 inherited:own ratio. |
| 2026-06-06 | c9172fc | #992   | v1.3.0  | 95.6%  | 99.6%     | 86.6%    | 86.1%  | region.h: ACSL contracts + WP (report-only) at 120s/split; WP 3517/3643 (VERIFY-011; 103 inherited = arena.h's full residual surface + 23 own: 19 region_end opaque-hook-dispatch + 4 region_invariant composition), first 120s/split completion, enforcement deferred pending count stability. MC/DC 95.5% (21/22) ceiling (MCDC-005, the `!h->fn` guard branch unreachable). region.h's coverage was already in the aggregate (Phase 1 substrate); this records its documented MCDC entry. core/ layer verification complete. Composable-verification thesis confirmed at fifth composition layer. |
| 2026-06-13 | b78768e | #1022  | v1.3.0  | 95.6%  | 99.6%     | 86.6%    | 86.1%  | VERIFY-012: contract-strengthening pass closing 50 enforced unproved goals across five verified headers by ACSL precondition alone — zero executable change. Stated `\initialized` as an input precondition where WP can discharge it (6 requires in memory.h, 4 in slice.h), closing 8 slice + 6 memory own goals and their downstream inherited copies: slice 23→15, memory 57→43, arena 103→89, pool 127→113, region 126→112; project 18269→18333 proved, 463→399 unproved. Falsifies VERIFY-008's earlier "strengthening memory.h's predicates would produce no improvement" (corrected in place). `\dangling`/`\fresh`/`\freeable` remain WP-blocked in Frama-C 29 (Blanchard). All five exact-count CI wrappers re-enforced; region.h promoted from report-only to enforced (residual set name-stable across the closure). No coverage/MC/DC change (closures touched no branches). |
| 2026-06-20 | ca9732d | #1036  | v1.3.0  | 95.6%  | 99.6%     | 86.6%    | 86.1%  | error.h: first semantics/ header verified. ACSL contracts + WP enforcement (65/65, VERIFY-013); default Typed model (no void* casts); 0 residuals — first clean header since compare.h. Calibration baseline for the semantics/ layer. No coverage change (error.h trivially 100% MC/DC, already in aggregate). |
| 2026-06-27 | 93bb107 | #1072  | v1.3.0  | 95.7%  | 99.5%     | 86.6%    | 86.2%  | option: first Shape-B module covered via the vmacros cover-TU pattern (`option_cover.c`, `CANON_OPTION(int)` instantiated outside `test/`). MC/DC 96.7% (29/30) ceiling (MCDC-006, the `expect` panic-on-absent guard unreachable, cross-confirmed by WP). WP driver enforced 189/223 (VERIFY-014, #1067; report-only first at #1065). The cover TU enters the aggregate: project MC/DC 86.1% → 86.2% (1443/1674), branches 86.6% (1459/1684), functions 99.6% → 99.5% (546/549, the one new uncovered function being the `is_positive` cover-driver helper), lines 95.7% (2268/2371). option_cover.c contributes 29/30 at the file level (26 generated `option_int_*` conditions + 4 driver-scaffolding conditions). First vmacros driver + cover TU to land; Shape-B attribution pattern confirmed. |
| 2026-07-03 | b528515 | #1089  | v1.3.0  | 95.8%  | 99.5%     | 86.7%    | 86.4%  | result: second Shape-B module via the vmacros pattern (`result_verify.h` driver + `result_cover.c` cover TU; `CANON_RESULT(int, VErr)`, first union-typed module). WP driver enforced 185/215 (VERIFY-015, #1090 at 6516ae5; report-only first at #1089): 2 inherited (contract.h handler, definition-presence only — require_msg panic surface fully compiled out) + 28 own fn-pointer-dispatch (map/map_err 6 each, and_then/or_else 4 each, eq 8 with one rte_function_pointer goal per pointer); all union-member postconditions proved under the VERIFY-015 union-model standing hypothesis ([wp:union] warnings expected). MC/DC 100% (28/28) — first clean Shape-B audit (MCDC-007), no MCDC-006-style ceiling; generated conditions attributed to the driver header result_verify.h (22/22) with scaffolding in result_cover.c (6/6) — attribution finding recorded in MCDC-007 with option's wording flagged for re-audit. Per-module attribution check confirmed (result_test.c owns all 256 test-measured outcomes). Aggregate MC/DC 86.2% → 86.4% (1471/1702). |
| 2026-07-05 | a76202d → 262a503 | #1106 → #1110 | v1.3.0  | 95.8%  | 99.5%     | 86.9%    | 86.6%  | borrow.h — MC/DC ceiling + WP verification (MCDC-008, VERIFY-016), both streams in one arc. **Measurement (a76202d/#1106)**: three CANON_NO_REQUIRE-gated `_get(NULL)` tests close the defensive-branch outcomes (35/40 → 38/40, 95.0%); the 2 remaining outcomes are `borrowed_bytes_eq`'s one-NULL guard (MCDC-008 measurement half; gcov dump confirms line 758 conds 0/1 true, guard body `#####`); per-line MC/DC debug step for borrow.h added to the coverage job as the measurement-stream regression detector; aggregate MC/DC 86.4% → 86.6% (1474/1702), branches 86.7% → 86.9% (1472/1694). **Proof (262a503/#1110)**: ACSL contracts + WP (VERIFY-016) — fourth semantics/ module, second verified in place; 24 non-macro functions in place, DEFINE_BORROWED_SLICE parked per docs/vmacros.md (the slice.h/DEFINE_SLICE structure, first in semantics/), Typed+Cast, verified config CANON_LIFETIME off (OWN-001 §7). WP 2433/2452, exactly 19 unproved: 17 inherited (slice.h's full 15 + checked.h's 2, byte-identical; composability confirmation #6, smallest inherited surface since memory.h) + 2 own memcmp danglingness at the header's only libc call — no new residual class (first since error.h). Round 1 report-only at 383bf9f/#1109 (27 unproved; 8 call-site chaining goals closed by aligning four contracts to slice.h's real clauses, zero executable change). `dead_by_invariant` named assert proved — MCDC-008 cross-stream closure complete (first named-assert closure; detectors in both streams). Enforced as of #1111. No coverage change from the proof half (ACSL comments touch no branches). |
| 2026-07-08 | 1965b23 | #1135  | v1.3.0  | 96.1%  | 99.5%     | 87.7%    | 87.5%  | diag.h: MC/DC ceiling + ACSL contracts + WP enforced (VERIFY-017, MCDC-009) — fifth semantics/ module, third verified in place; **semantics/ layer complete**. Measurement stream (baselined at 197ef8e/#1122): render gap-closure arc #1118–#1120 grew the denominator 46 → 86 (diag_render/diag_render_frame were uncalled and invisible to gcov) and surfaced two pre-verification defects, both fixed (NULL-guard snprintf UB; -Werror=format-truncation on the documented truncation path); ceiling 84/86 (97.67%) pinned at #1120 (93fa22c); per-line diag.h debug step added as the measurement-stream detector; aggregate MC/DC 86.6% → 87.5% (1524/1742), branches 86.9% → 87.7% (1520/1734), lines 95.8% → 96.1% (2366/2461), functions 574/577. Proof stream: WP 3050/3060, exactly 10 unproved — 2 inherited (contract.h handler pair; smallest inherited surface of any residual-carrying header, composability confirmation #7) + 8 own libc byte-view goals at the memset/memmove sites — no new residual class. Typed+Cast originated by diag.h's own casts; first documented trusted stdio axioms (`-variadic-no-translation`; format-nonnull + conditional termination ensures sharing MCDC-009 outcome 2's hosted-libc assumption). Chronology: report-only d8566d5/#1132 (300s, 59), 1597a51/#1133 (120s, 59 name-stable — refuting time-starvation), pinning bb269f9/#1134 (actions run 28967245082: 49 closed by trusted-axiom alignment, zero executable change, toolchain byte-identical); residual set and both roll-calls predicted by name before the pinning run, confirmed 10/10 with 0 outside the set. `dead_by_invariant_clamp` named assert proved — MCDC-009 outcome-1 cross-stream closure (second named-assert closure; detectors in both streams). Enforced as of #1135. |
| 2026-07-12 | 5bfde5b → e663e2c | #1146 → #1154 | v1.3.0  | 96.4%  | 99.5%     | 87.8%    | 88.4%  | vec — MC/DC cover TU + WP driver through enforcement (MCDC-010, VERIFY-018), both streams in one arc; third Shape-B module, **first data/-layer module verified**, first driver on Typed+Cast. **Measurement (5bfde5b/#1146)**: `vmacros/coverage/vec_cover.c` lands (direct `DEFINE_VEC(static inline, int)` instantiation — third attribution variant: generated conditions attribute to the cover TU itself; 152 generated incl. 12 DEFINE_VEC_SLICE facade-view conditions + 6 scaffolding); 155/158 (98.10%) from the first run, the documented ceiling — U1/U2 (`!checked_mul` true in alloc/arena_alloc) guard-redundancy-infeasible and U3 (heap `!buf`) environmental, all three written down with dispositions before the run and confirmed exactly at block level; the arena `!buf` TRUE leg covered via deterministic exhaustion (the heap-vs-arena contrast evidence); Shape-B attribution check confirms the pattern (vec_test.c owns all 640 test-measured outcomes; vec_impl.h functions-but-no-conditions fingerprint = the Shape-A-drift tripwire); per-line + attribution debug steps added. Aggregate MC/DC 87.5% → 88.4% (1679/1900), branches 87.7% → 87.8% (1542/1756), lines 96.1% → 96.4% (2515/2610), functions 621/624. **Proof (driver arc #1144–#1152, enforced e663e2c/#1154)**: WP over `vmacros/vdrivers/vec_verify.h` via the DEFINE_VEC_STRUCTS/FUNCTIONS split (F3, landed before the driver — the split-patch-first checklist item). Chronology: run 1 1eeb58c/#1150 (4988/5350, 362 — spec-less composed callees, lesson R1: composition, not weaker specs), run 2 8a3bb1e/#1151 (5208/5413, 205 — inherited stabilized at 143 names), run 3 baseline 96dd41d/#1152 (5184/5380, exactly 196: 193 Timeout + 3 Unknown + 0 Failed; delegate narrowing closed 9 own goals 62 → 53; two retry-variance candidates failed a third consecutive run and pinned). 196 = 143 inherited model-variant re-emissions (arena 46, option 32, result 22, memory 20, slice-libc 13, ptr 3, checked/align 5, contract 2) + 53 own: (e) 2 allocation-model, (d) 5 element-transfer (frame-only memcopy/memmove), (g) 24 fill macro-body-loop (unprovable by construction; MC/DC primary evidence; deque's shift loops pre-classified), (h) 22 Typed+Cast bridging; zero own fn-pointer goals (first module without OWN-003). U1/U2 WP-corroborated: alloc/arena_alloc prove branch-complete. Enforced (pinned `5184 / 5380` + zero-Failed + exact 196 + by-name roll-call = set equality) green first try at #1154 while prover attribution, +14 source lines, and roll-call order all shifted — name-only matching validated empirically; set name-identical three consecutive runs. Runtime ~2h50m, timeout-dominated (every proving goal ≤332 ms); -wp-cache evaluated and rejected; 120 s timeout retained campaign-wide; job `timeout-minutes: 240`. Findings: F3 landed; F1 (append_array overlap doc) and F2 (slice_init NULL+0 one-token guard; cover-TU exclusion in force) owed upstream. **Correction (2026-07-16, docs-only)**: byte-level diffs of the frozen #1154 artifacts (wp-proof-arena/-option/-result/-vec) falsified two interpretive claims in this entry as first written — (1) the "model-variant re-emissions" reading: the checked/align five are the pinned home residuals of checked.h/ptr.h, the 89-goal core arm is **byte-identical** to arena.h's list (fragment indices and Timeout/Unknown sub-verdicts reproduced), the option arm byte-identical mod the typed_→typed_cast_ prefix (34/34) — **no verdict flip anywhere in the unit**; (2) the 143:53 inherited:own split: result(Bool, Error) is a **fresh instantiation** (VERIFY-015 verifies (int, VErr)) — reclassified 121 inherited + 75 subject-side; the fresh instance reproduces the family's home profile 20/20 at clause-family granularity, drops 8 never-emitted assigns goals, adds 2 union get_* mem-access goals (attribution open, F4). Pinned 196-name baseline, gates, and counts unchanged; superseded text retained verbatim in VERIFY-018's Correction note; instantiation-identity rule added to docs/vmacros.md (deque drivers to pre-register inheritance vs. fresh arms). No re-run required — only the artifacts the enforcement discipline preserves. |

---
