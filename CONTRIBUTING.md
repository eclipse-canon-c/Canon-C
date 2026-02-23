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

---

## Dependency Rules

Modules must obey the dependency hierarchy:

core → semantics → data → algo → util


- No upward dependencies.
- No circular dependencies.
- Every module must be independently usable.

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


