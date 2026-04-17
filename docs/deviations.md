# Deviations Record

## Overview

This document records all deviations from full compliance with
verification, coverage, and coding-standard targets. Each deviation
has a unique identifier, a rationale, a risk assessment, and a
disposition (accepted, mitigated, or planned for resolution).

Deviations are categorized by type:

- **VERIFY-xxx**: Formal verification scope limitations
- **MCDC-xxx**: MC/DC coverage gaps
- **MISRA-CFG-xxx**: MISRA tooling configuration limitations

---

## VERIFY-001: Compiler intrinsic path not verified

| Field          | Value                                                    |
|----------------|----------------------------------------------------------|
| **ID**         | VERIFY-001                                               |
| **File**       | core/primitives/checked.h                                |
| **Scope**      | All 18 functions                                         |
| **Category**   | Verification scope limitation                            |
| **Status**     | Accepted                                                 |

**Description**: When `CANON_HAS_BUILTIN_OVERFLOW` is 1 (GCC/Clang),
checked.h delegates to `__builtin_add_overflow`, `__builtin_sub_overflow`,
and `__builtin_mul_overflow`. These compiler builtins have no ACSL
semantics and cannot be modeled by Frama-C WP. The formal proofs
cover only the `#else` fallback path.

**Rationale**: The builtins are part of the compiler's trusted computing
base. GCC and Clang guarantee their semantics as part of the compiler
specification. Verifying the compiler itself is out of scope for a
library-level verification effort. CompCert (verified compiler) does
not provide these builtins, so CompCert users always take the fallback
path, which is proved.

**Risk assessment**: Low. The builtins are used by millions of programs
and are part of the compiler's own test suite. Any bug in them would
be a compiler bug, not a Canon-C bug. The fallback path is proved
correct and produces identical results.

**Mitigation**: The CI coverage job passes `-DCANON_CHECKED_FORCE_FALLBACK`
to exercise the fallback path even on GCC/Clang, ensuring the proved
code is also tested. CompCert and Frama-C users always take the proved
path via `__FRAMAC__` detection.

---

## VERIFY-002: Two manually discharged proof obligations

| Field          | Value                                                    |
|----------------|----------------------------------------------------------|
| **ID**         | VERIFY-002                                               |
| **File**       | core/primitives/checked.h                                |
| **Functions**  | checked_add (usize), checked_add_u64                     |
| **Category**   | Proof limitation                                         |
| **Status**     | Accepted                                                 |

**Description**: Two proof obligations are not discharged by any
automated SMT prover:
- `typed_checked_add_overflow_ensures`
- `typed_checked_add_u64_overflow_ensures`

Both assert that the overflow behavior returns `false` for 64-bit
unsigned addition.

**Rationale**: WP formulates these goals in its integer memory model
where unsigned wraparound requires modular-arithmetic reasoning
(`(a + b) mod 2^64`). Current SMT solvers cannot close this in WP's
encoding. The goals are discharged by a manual modular-arithmetic
argument recorded in `docs/verification.md`.

**Risk assessment**: Low. The manual argument is a standard
modular-arithmetic proof that any reviewer can verify independently.
The CI wrapper verifies that exactly these two goals timeout and no
others — any additional failure is a regression.

**Mitigation**: CI explicitly checks for exactly 2 timeouts on
exactly the named goals. The manual proof is recorded in
verification.md. The full WP output is uploaded as the
`wp-proof-output` CI artifact for auditor inspection.

**Future work**: Investigate adding an ACSL ghost lemma encoding
the modular-arithmetic relationship to allow Alt-Ergo or Z3 to
close the goals automatically. Estimated effort: 1–3 hours with
no guarantee of success. This would bring the count to 1541/1541.

---

## MCDC-001: contract.h at 0% MC/DC in coverage builds

| Field          | Value                                                    |
|----------------|----------------------------------------------------------|
| **ID**         | MCDC-001                                                 |
| **File**       | core/primitives/contract.h                               |
| **Scope**      | All require_msg / ensure_msg branches                    |
| **Category**   | Coverage measurement artifact                            |
| **Status**     | Accepted — by design                                     |

**Description**: contract.h shows 0% MC/DC across every translation
unit in the coverage build. This is because the coverage build defines
`CANON_NO_REQUIRE`, which compiles `require()` and `require_msg()` to
`((void)0)`. The only remaining condition is in
`contract_default_handler` (the `if (msg)` check), which is never
reached because the handler itself is never called when assertions
are compiled out.

**Rationale**: `CANON_NO_REQUIRE` is defined in the coverage build
specifically to remove proved-unreachable branches from the MC/DC
denominator. Without it, every `require_msg` in every header adds a
NULL branch that is structurally unreachable in correct code but
inflates the coverage denominator. The flag keeps coverage measurement
aligned with the Frama-C proof evidence.

**Risk assessment**: None. contract.h is fully tested in the `build`,
`valgrind`, and `fuzzing` CI jobs where `CANON_NO_REQUIRE` is NOT
defined. The `contract_test` binary specifically tests every macro
in every build configuration. Excluding it from coverage measurement
is honest about what the coverage build measures.

**Mitigation**: contract_test runs in all non-coverage CI jobs and
verifies require, require_msg, ensure, ensure_msg, unreachable,
unreachable_msg, panic, contract_set_handler, and static_require
across all flag combinations (default, NDEBUG, CANON_STRICT).

---

## MISRA-CFG-001: Cppcheck MISRA addon tool limitation

| Field          | Value                                                    |
|----------------|----------------------------------------------------------|
| **ID**         | MISRA-CFG-001                                            |
| **Scope**      | All macro-templated implementation headers (*_impl.h)    |
| **Category**   | Tooling limitation                                       |
| **Status**     | Accepted                                                 |

**Description**: Cppcheck's MISRA addon emits `[misra-config]` errors
on macro-templated headers (hashmap_impl.h, deque_impl.h, vec_impl.h,
etc.) because Cppcheck cannot resolve macro-instantiated type names
(e.g. `HASHMAP_SLOT_NAME`) without an instantiation context.

**Rationale**: This is a tool limitation, not a code defect. Qualified
MISRA checkers (Polyspace, LDRA, PC-lint, Parasoft C/C++test) handle
macro-instantiated code correctly via full preprocessor-aware analysis.
Cppcheck's MISRA addon covers approximately 60–70% of MISRA C:2012
rules and is used for preliminary compliance tracking, not as a
qualified checker.

**Risk assessment**: None for the code. The MISRA CI job is advisory
and does not fail the build.

**Mitigation**: For certification, replace Cppcheck's MISRA addon with
a qualified MISRA checker. The `--suppress=misra-config:*_impl.h`
suppression prevents false positives from obscuring real findings in
non-templated headers.

---

## Deviation history

| Date       | ID           | Action                                    |
|------------|--------------|-------------------------------------------|
| 2026-04-17 | VERIFY-001   | Initial documentation                     |
| 2026-04-17 | VERIFY-002   | Initial documentation                     |
| 2026-04-17 | MCDC-001     | Initial documentation                     |
| 2026-04-17 | MISRA-CFG-001| Initial documentation                     |
