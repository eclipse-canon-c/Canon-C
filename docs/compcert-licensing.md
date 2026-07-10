# CompCert Usage — Licensing Record

## Summary

Canon-C uses the **non-commercial release of CompCert** solely as a
**CI-only evaluation tool**: the library's headers are compiled across
CompCert's supported targets to produce portability evidence
("compiles cleanly under a formally verified compiler").

CompCert is **not** shipped, linked, redistributed, vendored, or made a
dependency of any Canon-C artifact. No CompCert output is distributed.
The CI job is observational only.

## Permission

- **License basis:** INRIA Non-Commercial License (CompCert
  non-commercial release), evaluation/research use.
- **Original inquiry sent:** 2026-06-28, by the project lead
  (Fikret Güney Ersezer), describing exactly the scope above.
- **Written confirmation received:** 2026-07-10, by email from
  **Dr.-Ing. Christian Ferdinand**, Managing Director
  (Geschäftsführer), AbsInt Angewandte Informatik GmbH
  (ferdinand@absint.com), the commercial steward of CompCert.
- **Content of confirmation:** Dr. Ferdinand confirmed — noting
  agreement from Xavier Leroy — that the CI-only, non-shipped use
  described above falls under the INRIA Non-Commercial License, and
  approved proceeding on that basis.
- **Archive:** the full email exchange is retained by the project lead
  and can be provided to Eclipse Foundation IP review on request.

## Scope and boundary of the permission

The confirmation covers **this project's own non-commercial, CI-only,
evaluation use**. It does **not** extend to third parties.

In particular:

- **Forks and downstream users:** if you fork Canon-C or reuse its CI
  configuration in a **commercial context**, running the CompCert job
  constitutes *your own* use of CompCert and is **not** covered by this
  permission. You must either **disable the CompCert CI job** or obtain
  an appropriate CompCert license from AbsInt
  (https://www.absint.com/).
- The CompCert CI job is therefore configured as **optional /
  informational** and clearly labeled in the pipeline configuration.
  Disabling it does not affect any other evidence stream (Frama-C/WP
  proofs, MC/DC coverage, fuzzing, sanitizers).

## What the CompCert stream does and does not claim

- **Does claim:** the Canon-C sources are accepted and compiled without
  error by CompCert across the targets exercised in CI, providing
  portability evidence under a formally verified compiler.
- **Does not claim:** that Canon-C binaries are built with CompCert,
  that CompCert's correctness theorem extends to any shipped artifact,
  or that CompCert is part of Canon-C's trusted base. CompCert is an
  evaluation instrument here, not a build dependency.

## Contact

Questions about this record: project lead, Eclipse Canon-C.
Questions about CompCert licensing: AbsInt Angewandte Informatik GmbH,
info@absint.com.
