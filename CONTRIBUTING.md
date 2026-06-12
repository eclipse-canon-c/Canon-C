# Contributing Guidelines

This document defines **rules for community contributions** to ensure
clarity, consistency, and maintainability.

---

## Module Acceptance Criteria

A module may be accepted if it can justify itself as:

1. **Foundational**
   - Clearly required in most non-trivial C programs.
   - Reduces boilerplate or mechanical complexity.
   - Composes with other modules cleanly.

2. **Optional but Isolated**
   - Provides convenience or utility.
   - Has no hidden dependencies or side effects.
   - Does not violate the dependency hierarchy.

---

## Coding Rules

- No hidden allocation.
- No global state.
- No implicit ownership.
- No clever tricks or macros that obscure intent.
- All function behavior must be explicit in headers.
- Header-only by default; ".c" files require explicit justification.
- No system-`<limits.h>` symbols (`CHAR_BIT`, `INT_MAX`, `UCHAR_MAX`, ...) —
  see Platform Requirement Rules.

---

## Dependency Rules

Modules must obey the dependency hierarchy:

core → semantics → data → algo → util


- No upward dependencies.
- No circular dependencies.
- Every module must be independently usable.

---

## Platform Requirement Rules

Platform requirements mirror the dependency hierarchy: a header inherits
the contracts of everything it includes, and must guard any **new**
requirement it introduces — at its origin, once.

- Tier 0 (8-bit bytes, C99 exact-width types) is enforced in `types.h`.
  Every header inherits it by inclusion.
- Tier 1 (`size_t` >= 32 bits) is enforced in `limits.h`, beside the size
  literals that need it.
- Tier 2 (hosted libc: working malloc, retargeted stdio, OS clocks —
  memory.h, data/convenience, file.h, log, parse.h, time.h) is enforced
  by documentation at origin, not guards: hosted-ness is a runtime/link
  property the preprocessor cannot test (`__STDC_HOSTED__` tracks a
  compilation mode — newlib bare-metal reports 1 — not whether malloc
  works). The linker enforces the rest.
- Never duplicate a guard downstream. Never centralize guards in a shared
  platform header. One requirement, one home, inherited by inclusion.
- Guards testing a macro's **value** must gate on `defined()` first:
  `#if defined(SIZE_MAX) && (SIZE_MAX < 0xFFFFFFFF)`. Static analyzers
  that do not expand system headers treat unknown identifiers as 0 and
  would fire the error on healthy targets. Guards testing **existence**
  (`!defined(UINT8_MAX)`) already branch correctly.
- Tier guards are not overridable. Tuning constants use the
  `#ifndef CANON_*` override pattern; contracts do not.

### The limits.h name shadow

`core/primitives/limits.h` shadows the system `<limits.h>` under the
build's `-I core/primitives` flag (angle includes search `-I` before
system directories; MSVC `/I` likewise). Therefore:

- No Canon-C header, test, or doc example may use symbols only the system
  `<limits.h>` provides: `CHAR_BIT`, `INT_MAX`, `INT_MIN`, `UCHAR_MAX`, etc.
- All limits come from `<stdint.h>` (`INT32_MAX`, `SIZE_MAX`, ...), which
  is sufficient and never shadowed.
- `types.h` enforces 8-bit bytes by implication from `uint8_t`'s existence
  (C99 7.18.1.1) for exactly this reason.
- Renaming the file would remove the shadow but is a breaking include-path
  change — recorded as a known constraint instead.

Re-audit (run from the repo root; both should return only comments and
documentation):

```bash
grep -rn '#include <limits.h>' --include='*.h' --include='*.c' .
grep -rnE '\b(CHAR_BIT|SCHAR_MIN|SCHAR_MAX|UCHAR_MAX|CHAR_MIN|CHAR_MAX|MB_LEN_MAX|SHRT_MIN|SHRT_MAX|USHRT_MAX|INT_MIN|INT_MAX|UINT_MAX|LONG_MIN|LONG_MAX|ULONG_MAX|LLONG_MIN|LLONG_MAX|ULLONG_MAX)\b' \
  --include='*.h' --include='*.c' .
```

---

## Submission Process

1. Fork the repository.
2. Create your module in the appropriate layer.
3. Write clear headers with:
   - Module purpose
   - Dependencies
   - Memory/failure guarantees
4. Submit a pull request with explanation of:
   - Why the module is foundational or optional
   - How it fits into the hierarchy
   - Any guarantees or limitations

PRs violating these rules will be returned with feedback.

---

## Frozen Modules (v1.2.0)

All modules included in v1.2.0 are frozen.

Allowed changes:
- Comments
- Documentation
- Typo fixes
- Bug fixes that do not alter semantics or signatures

Disallowed changes:
- Signature changes
- Ownership changes
- Lifetime changes
- Hidden behavior
- New dependencies

Breaking changes require a new major release.
