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
| **Date**           | 2026-05-09                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | b3e668b                                                      |
| **CI run**         | Canon-C CI #840 (coverage at 4baf5c6) / #841 (frama-c memory.h at b3e668b) |
| **CI job**         | coverage + frama-c                                           |
| **Branch**         | master                                                       |
| **Compiler**       | GCC 14.2.0                                                   |
| **Build type**     | Debug                                                        |
| **Flags**          | `--coverage -fcondition-coverage -DCANON_CHECKED_FORCE_FALLBACK -DCANON_BITS_FORCE_FALLBACK -DCANON_NO_REQUIRE` |
| **Tool**           | gcov-14 --conditions (MC/DC) + lcov (branch)                |
| **Runner**         | ubuntu-latest (GitHub Actions)                               |
| **Scope**          | Library headers only — test files excluded                   |
| **Test binaries**  | 51 (contract_test excluded from coverage build)              |

### Results

| Metric     | Percentage | Covered    | Total      |
|------------|------------|------------|------------|
| Lines      | 95.7%      | 2163       | 2261       |
| Functions  | 99.6%      | 512        | 514        |
| Branches   | 85.5%      | 1418       | 1658       |
| MC/DC      | 84.8%      | 1381       | 1628       |

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

| Header     | Functions | Proof obligations | Proved (auto)     | Unproved | Deviation    |
|------------|-----------|-------------------|-------------------|----------|--------------|
| checked.h  | 30        | 1755              | 1753 (99.89%)     | 2        | VERIFY-002   |
| bits.h     | 18        |  761              |  746 (98.03%)     | 15       | VERIFY-003/4 |
| compare.h  | 28        |  208              |  208 (100%)       | 0        | VERIFY-005   |
| ptr.h      | 21        | 1739              | 1729 (99.43%)     | 10       | VERIFY-006   |
| slice.h    | 22        |  390              |  367 (94.10%)     | 23       | VERIFY-007   |
| memory.h   | 27        | 2862              | 2805 (98.01%)     | 57       | VERIFY-008   |
| **Total**  | **146**   | **7715**          | **7608 (98.61%)** | **107**  |              |

**Prover setup**: Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1 (triple-prover,
`-wp-timeout 120`). All 107 unproved goals are demonstrated
triple-prover-resistant and carry written manual-proof arguments or
documented WP feature-gap rationale in `docs/deviations.md`.

**Note on totals**: the 7715 figure is the row-sum of independent
per-header WP runs, where each row reports each header's own
obligations (substrate goals are counted under their owning header,
not duplicated into downstream rows). The CI WP step for ptr.h
actually reports 1943/1953 because it processes ptr.h's translation
unit including checked.h, but those +214 obligations belong to
checked.h's row, not ptr.h's. Similarly, memory.h's CI step processes
memory.h with ptr.h, slice.h, checked.h, and contract.h all
transitively included; the 2862 figure for memory.h reflects the
full run, but is shown here as memory.h's row because memory.h is
the verification target. 31 of memory.h's 57 unproved goals are
re-emerged residuals already documented under VERIFY-002/006/007
(see VERIFY-008).

The slice.h baseline (367 / 390) carries a higher residual fraction
(5.9%) than the previously verified primitives headers because slice.h
is the first Canon-C header that crosses the C standard library
boundary — its equality functions call `memcmp` and `str_from_cstr`
calls `strlen`, both of which have ACSL contracts requiring
initialization and danglingness reasoning that Frama-C 29 has not yet
implemented (see VERIFY-007 for the full WP warning text). The 23
slice.h residuals are not specific to slice.h's design; future headers
that use these libc functions will incur the same residual category.

The memory.h baseline (2805 / 2862) is the first Canon-C verification
round where inherited residuals from substrate headers exceed
memory.h-own residuals — 31 of 57 are byte-identical to goals already
documented in VERIFY-002/006/007, re-emerging because memory.h
includes those headers transitively. The 26 memory.h-own residuals
fall into three categories that are each rooted in a Frama-C feature
gap (\fresh/\freeable allocation reasoning, bitwise-alignment integer
theory, memcmp call-site initialization/danglingness). All 26 are
documented in VERIFY-008 with manual-proof arguments. The 31:26
inherited:own ratio is the first quantitative data point for the
composable-verification thesis: substrate residuals propagate without
amplification across composition layers.

The checked.h baseline grew by 214 proof obligations (1541 → 1755) when
the division and modulo functions were added. All 214 new obligations
were discharged automatically — most by Qed, with smaller contributions
from Alt-Ergo and from WP's structural categories (Terminating /
Unreachable). The two manually-discharged goals are unchanged from
the previous baseline.

### Cross-stream evidence: MCDC-002 closure

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

memory.h does not extend the MCDC-002 list because its bytes_t/cbytes_t
variants inherit slice.h's `bytes_invariant` — the analogous `!ptr`
defensive branches are discharged by the inherited invariant rather
than introducing new public-API-unreachable code paths. Future
headers that introduce their own public {ptr, len} types (arena.h,
pool.h, stringbuf.h are candidates) will need their own per-header
MCDC analyses; memory.h does not.

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

---
