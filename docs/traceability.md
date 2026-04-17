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
| **Date**           | 2026-04-17                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | 7efd1c7                                                      |
| **CI run**         | Canon-C CI #752                                              |
| **CI job**         | coverage                                                     |
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
| Lines      | 95.6%      | 2109       | 2207       |
| Functions  | 99.6%      | 499        | 501        |
| Branches   | 84.2%      | 1364       | 1620       |
| MC/DC      | 83.3%      | 1325       | 1590       |

### Methodology changes since baseline

Two coverage flags were added after the baseline measurement:

**CANON_CHECKED_FORCE_FALLBACK** (added 2026-04-14): Forces the portable
fallback path in checked.h instead of compiler builtins. Without this,
GCC expands `__builtin_*_overflow` to single instructions with no
branches for gcov to see. The fallback path is the same code Frama-C
proves — the coverage and verification evidence streams stay aligned.

**CANON_BITS_FORCE_FALLBACK** (added 2026-04-17): Same rationale for
bits.h. Forces the portable fallback paths for popcount, clz, ctz,
and bswap instead of compiler builtins. Without this flag, GCC builtins
expand to single instructions and the fallback branches are dead code.

**CANON_NO_REQUIRE** (added 2026-04-14): Compiles `require_msg()` to
`((void)0)`, removing NULL-check branches that the ACSL
`requires \valid(result)` clause has already proved unreachable under
Frama-C. Without this flag, every `require_msg` in every header adds
a branch whose false-side is structurally unreachable in correct code,
inflating the coverage denominator with conditions no test can
meaningfully exercise. contract_test is excluded from the coverage
build for the same reason — it tests the assertion infrastructure
itself, which is compiled out when CANON_NO_REQUIRE is defined.
contract_test still runs in all other CI jobs (build, valgrind, fuzzing).

These changes reduced the branch/MC/DC denominator (from 2074/1858 to
1620/1590) by removing proved-unreachable branches. The numerator also
decreased slightly because some of those removed branches were being
hit. The net effect is a higher percentage that more accurately
reflects coverage of real, reachable logic.

### Notable per-header results

Headers at 100% MC/DC (no coverage-driven tests — correctness tests only):

`bits.h`, `compare.h`, `error.h`, `str_view.h`, `time.h`,
`map_impl.h`, `filter_impl.h`, `find_impl.h`, `reverse_impl.h`,
`search_impl.h`, `any_all_impl.h`, `unique_impl.h`, `checked.h`

checked.h reached 100% MC/DC (64/64) after the checked_mul_isize
refactor on 2026-04-17 (commit 972eb6c, CI #743), which lifted identity
cases before the ISIZE_MIN guard to eliminate a structurally unreachable
branch.

bits.h has maintained 100% MC/DC (52/52) since initial measurement.

### History

| Date       | Commit  | CI Run | Version | Lines  | Functions | Branches | MC/DC  | Notes                                                                    |
|------------|---------|--------|---------|--------|-----------|----------|--------|--------------------------------------------------------------------------|
| 2026-04-07 | 669d6a7 | #668   | v1.3.0  | 95.9%  | 99.6%     | 74.3%    | —      | Baseline — branch coverage only                                         |
| 2026-04-07 | 0a22f76 | #677   | v1.3.0  | 95.9%  | 99.6%     | 74.3%    | 74.3%  | First MC/DC measurement (GCC 14.2.0)                                    |
| 2026-04-14 | 24c69cc | #739   | v1.3.0  | 95.5%  | 99.6%     | 83.9%    | 83.1%  | CANON_NO_REQUIRE + CANON_CHECKED_FORCE_FALLBACK flags added             |
| 2026-04-17 | 972eb6c | #743   | v1.3.0  | 95.5%  | 99.6%     | 84.0%    | 83.1%  | checked_mul_isize refactor; checked.h → 100% MC/DC; WP artifact upload  |
| 2026-04-17 | 7efd1c7 | #752   | v1.3.0  | 95.6%  | 99.6%     | 84.2%    | 83.3%  | bits.h ACSL contracts; WP 746/761; CANON_BITS_FORCE_FALLBACK added       |

---
