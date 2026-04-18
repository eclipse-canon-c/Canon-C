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
| **Date**           | 2026-04-18                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | 2f33389                                                      |
| **CI run**         | Canon-C CI #761                                              |
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

**CANON_CHECKED_FORCE_FALLBACK** (added 2026-04-14): Forces the portable
fallback path in checked.h instead of compiler builtins.

**CANON_BITS_FORCE_FALLBACK** (added 2026-04-17): Same rationale for
bits.h — forces the portable fallback paths for popcount, clz, ctz,
and bswap instead of compiler builtins.

**CANON_NO_REQUIRE** (added 2026-04-14): Compiles `require_msg()` to
`((void)0)`, removing NULL-check branches that ACSL `requires \valid(result)`
has already proved unreachable under Frama-C. contract_test is excluded
from the coverage build for the same reason.

### Notable per-header results

Headers at 100% MC/DC:

`bits.h`, `compare.h`, `error.h`, `str_view.h`, `time.h`,
`map_impl.h`, `filter_impl.h`, `find_impl.h`, `reverse_impl.h`,
`search_impl.h`, `any_all_impl.h`, `unique_impl.h`, `checked.h`

### History

| Date       | Commit  | CI Run | Version | Lines  | Functions | Branches | MC/DC  | Notes                                                                    |
|------------|---------|--------|---------|--------|-----------|----------|--------|--------------------------------------------------------------------------|
| 2026-04-07 | 669d6a7 | #668   | v1.3.0  | 95.9%  | 99.6%     | 74.3%    | —      | Baseline — branch coverage only                                         |
| 2026-04-07 | 0a22f76 | #677   | v1.3.0  | 95.9%  | 99.6%     | 74.3%    | 74.3%  | First MC/DC measurement (GCC 14.2.0)                                    |
| 2026-04-14 | 24c69cc | #739   | v1.3.0  | 95.5%  | 99.6%     | 83.9%    | 83.1%  | CANON_NO_REQUIRE + CANON_CHECKED_FORCE_FALLBACK flags added             |
| 2026-04-17 | 972eb6c | #743   | v1.3.0  | 95.5%  | 99.6%     | 84.0%    | 83.1%  | checked_mul_isize refactor; checked.h → 100% MC/DC; WP artifact upload  |
| 2026-04-17 | 7efd1c7 | #752   | v1.3.0  | 95.6%  | 99.6%     | 84.2%    | 83.3%  | bits.h ACSL contracts; WP 746/761; CANON_BITS_FORCE_FALLBACK added       |
| 2026-04-18 | 2f33389 | #761   | v1.3.0  | 95.6%  | 99.6%     | 84.2%    | 83.3%  | compare.h ACSL contracts; WP 208/208 (100%); Typed+Cast model            |

---
