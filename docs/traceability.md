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
| **Date**           | 2026-04-27                                                   |
| **Version**        | v1.3.0                                                       |
| **Commit**         | c3df659                                                      |
| **CI run**         | Canon-C CI #804                                              |
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
| Lines      | 95.7%      | 2159       | 2257       |
| Functions  | 99.6%      | 511        | 513        |
| Branches   | 84.6%      | 1398       | 1652       |
| MC/DC      | 83.8%      | 1359       | 1622       |

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

## Formal verification status

Per-header formal verification state (see `docs/verification.md` for
full per-header detail):

| Header     | Functions | Proof obligations | Proved (auto) | Unproved | Deviation    |
|------------|-----------|-------------------|---------------|----------|--------------|
| checked.h  | 30        | 1755              | 1753 (99.89%) | 2        | VERIFY-002   |
| bits.h     | 18        |  761              |  746 (98.03%) | 15       | VERIFY-003/4 |
| compare.h  | 28        |  208              |  208 (100%)   | 0        | VERIFY-005   |
| ptr.h      | 26        | 1739              | 1729 (99.43%) | 10       | VERIFY-006   |
| **Total**  | **102**   | **4463**          | **4436 (99.40%)** | **27** |            |

**Prover setup**: Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1 (triple-prover,
`-wp-timeout 120`). All 27 unproved goals are demonstrated
triple-prover-resistant and carry written manual-proof arguments in
`docs/deviations.md`.

The checked.h baseline grew by 214 proof obligations (1541 → 1755) when
the division and modulo functions were added. All 214 new obligations
were discharged automatically — most by Qed, with smaller contributions
from Alt-Ergo and from WP's structural categories (Terminating /
Unreachable). The two manually-discharged goals are unchanged from
the previous baseline.

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
| 2026-04-27 | c3df659 | #804   | v1.3.0  | 95.7%  | 99.6%     | 84.6%    | 83.8%  | checked.h div/mod added (12 functions); WP 1753/1755; 100% MC/DC held    |

---
