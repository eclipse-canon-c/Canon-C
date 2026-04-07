# Requirements Traceability Matrix

## Coverage Record

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

### Results

| Metric     | Percentage | Covered    | Total      |
|------------|------------|------------|------------|
| Lines      | 95.9%      | 2289       | 2388       |
| Functions  | 99.6%      | 499        | 501        |
| Branches   | 74.3%      | 1542       | 2074       |

Baseline measurement — no coverage-driven tests. All tests written
for correctness only. Coverage is a side effect of functional testing.

### History

| Date       | Commit  | CI Run | Version | Lines  | Functions | Branches | Notes                              |
|------------|---------|--------|---------|--------|-----------|----------|------------------------------------|
| 2026-04-07 | 669d6a7 | #668   | v1.3.0  | 95.9%  | 99.6%     | 74.3%    | Baseline — no coverage-driven tests |

---
