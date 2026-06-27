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
| **Date**           | 2026-06-27                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | 93bb107                                                      |
| **CI run**         | Canon-C CI #1072                                             |
| **CI job**         | coverage + frama-c                                           |
| **Branch**         | master                                                       |
| **Compiler**       | GCC 14.2.0                                                   |
| **Build type**     | Debug                                                        |
| **Flags**          | `--coverage -fcondition-coverage -DCANON_CHECKED_FORCE_FALLBACK -DCANON_BITS_FORCE_FALLBACK -DCANON_NO_REQUIRE` |
| **Tool**           | gcov-14 --conditions (MC/DC) + lcov (branch)                |
| **Runner**         | ubuntu-latest (GitHub Actions)                               |
| **Scope**          | Library headers + Shape-B cover TUs — test files excluded    |
| **Test binaries**  | 51 (50 test binaries + 1 Shape-B cover TU `option_cover`; contract_test excluded from the coverage build) |

> Note: this baseline advances from CI #974/#992 to CI #1072, where the
> first Shape-B cover TU (`vmacros/coverage/option_cover.c`) entered the
> coverage build. The cover TU re-instantiates `CANON_OPTION(int)` outside
> `test/` so the option module's macro-generated condition outcomes — which
> are filtered out when measured through `option_test.c` (a `/test/` path) —
> are attributed to a surviving file and counted (see `docs/vmacros.md` and
> MCDC-006). Adding the cover TU brings option's 30 condition outcomes into
> the MC/DC denominator and also brings the TU's own scaffolding functions
> and lines into the project line/function denominators. The aggregate moved
> accordingly: MC/DC 86.1% → 86.2% (1443/1674), function coverage 99.6% →
> 99.5% (546/549, the one new uncovered function being the `is_positive`
> cover-driver helper, reachable only via the `filter` F&&_ short-circuit so
> its body never runs). region.h's 21/22 MC/DC contribution was already in the
> aggregate from Phase 1 of the lifetime substrate; CI #992 added only its
> *documented* MCDC entry (MCDC-005), not a new measurement.

### Results

| Metric     | Percentage | Covered    | Total      |
|------------|------------|------------|------------|
| Lines      | 95.7%      | 2268       | 2371       |
| Functions  | 99.5%      | 546        | 549        |
| Branches   | 86.6%      | 1459       | 1684       |
| MC/DC      | 86.2%      | 1443       | 1674       |

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

**Shape-B cover TUs** (added 2026-06-27): The coverage build now compiles
the Shape-B macro-module cover translation units under
`vmacros/coverage/` (starting with `option_cover.c`) so the macro-generated
condition outcomes — otherwise attributed to `/test/` paths and filtered
out — are measured against a surviving non-test file. The cover TUs are
compiled with the same `-DCANON_NO_REQUIRE -DNDEBUG` as the WP run so the
measured condition set equals the proof's goal set. See `docs/vmacros.md`
and MCDC-006.

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
row is the driver's WP run over `vmacros/vdrivers/option_verify.h`.

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
| **Total**  | **219**   | **19020**         | **18587 (97.72%)**| **433**  |              |

The option row is the driver-verified Shape-B module: WP runs over
`vmacros/vdrivers/option_verify.h`, which instantiates the real shipped
`IMPL_OPTION_*` macros at `CANON_OPTION(int)`. Its 223 obligations are
option's own generated-function goals plus the 2 contract.h handler goals
it reaches transitively through `expect`; option does not pull in the
core/ substrate stack. See VERIFY-014.

**Prover setup**: Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1 (triple-prover,
`-wp-timeout 120`). Of the 433 unproved goals, the 399 in-place-header
goals are demonstrated triple-prover-resistant and carry written
manual-proof arguments or documented WP feature-gap rationale in
`docs/deviations.md`. option's 34 are a distinct class — function-pointer-
dispatch feature-gap residuals (no `calls` clause for an arbitrary callee;
`\valid_function` unimplemented in Frama-C 29), the same class as
region_end's opaque-hook dispatch (VERIFY-011 cat 1), documented in
VERIFY-014.

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
VERIFY-014).

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

The checked.h baseline grew by 214 proof obligations (1541 → 1755) when
the division and modulo functions were added. All 214 new obligations
were discharged automatically — most by Qed, with smaller contributions
from Alt-Ergo and from WP's structural categories (Terminating /
Unreachable). The two manually-discharged goals are unchanged from
the previous baseline.

### Cross-stream evidence: MCDC-002 through MCDC-006 closures

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
(MCDC-002 lists it provisionally at 74.2%) — and the remaining Shape-B
cover TUs (result, vec, deque, fold) will follow option's pattern. Each
will need its own per-header / per-module MCDC analysis; each specific
unreachability shape will be documented separately.

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

---
