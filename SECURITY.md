# Security Policy

This Eclipse Foundation Project adheres to the
[Eclipse Foundation Vulnerability Reporting Policy](https://www.eclipse.org/security/policy/).

## How To Report a Vulnerability

If you think you have found a vulnerability in this repository, please report
it to us through coordinated disclosure.

**Please do not report security vulnerabilities through public issues,
discussions, or change requests.**

Instead, report it using one of the following ways:

- Contact the
  [Eclipse Foundation Security Team](mailto:security@eclipse-foundation.org)
  via email
- Create a
  [confidential issue](https://gitlab.eclipse.org/security/vulnerability-reports/-/issues/new?issuable_template=new_vulnerability)
  in the Eclipse Foundation Vulnerability Reporting Tracker
- Report a
  [vulnerability](https://github.com/eclipse-canon-c/Canon-C/security/advisories/new)
  directly via private vulnerability reporting on GitHub

You can find more information about reporting and disclosure at the
[Eclipse Foundation Security page](https://www.eclipse.org/security/).

Please include as much of the information listed below as you can to help us
better understand and resolve the issue:

- The type of issue (e.g. buffer overflow, integer overflow, missed bounds
  check, unsound contract)
- Affected version(s)
- Impact of the issue, including how an attacker might exploit it
- Step-by-step instructions to reproduce the issue
- The location of the affected source code (tag/branch/commit or direct URL)
- Full paths of source file(s) related to the manifestation of the issue
- Configuration required to reproduce the issue — for Canon-C this means the
  **compiler, target width, and the `CANON_*` macros in effect**, because
  several behaviours are configuration-dependent (see below)
- Log files that are related to this issue (if possible)
- Proof-of-concept or exploit code (if possible)

This information will help us triage your report more quickly.

## Supported Versions

Canon-C is header-only: there is no binary artifact and no runtime to patch.
"Supported" therefore means *the version against which a fix will be issued*.

| Version           | Supported |
| ----------------- | --------- |
| `master`          | Yes       |
| >= v1.2.0         | Yes       |
| < v1.2.0          | No        |

Releases before v1.2.0 are historical snapshots documenting the project's
evolution and are not semantically frozen; see `RELEASES.md`. Fixes are made
on `master` and carried into the next release rather than backported to
pre-v1.2.0 tags.

---

## What counts as a vulnerability in Canon-C

Canon-C is a header-only library with no runtime, no daemon, no network
surface, and no global state beyond a single replaceable contract handler. It
does not process untrusted input on its own — it is linked into programs that
do. The useful question is therefore not "can Canon-C be attacked" but
**"can a Canon-C safety mechanism fail to hold while appearing to hold."**

Two classes matter.

### Class 1 — a check that does not check

Canon-C's value to a calling program is that certain classes of error are
caught at the boundary. A defect in that mechanism is security-relevant even
when the library itself is not the attack surface, because callers have
delegated the check. Examples:

- A bounds check that admits an out-of-range index or length.
- An overflow-checked arithmetic function that reports success on an operation
  that in fact wrapped, or is wrong on a supported target width.
- An arena or pool that returns overlapping, misaligned, or out-of-capacity
  regions, or whose reset leaves reachable stale pointers it claims to have
  invalidated.
- A borrow-validity check (`region_assert_borrow_valid`) that reports a borrow
  live after its region has closed.
- A precondition compiled out under a configuration where the documentation
  says it remains active.

The modules most likely to touch attacker-influenced bytes in a calling
program are `util/parse.h`, the `util/str/*` family, `util/file.h`, and the
formatting paths in `semantics/diag.h` and `util/log.h`. Reports about those
are especially welcome, as are reports about `core/slice.h`,
`core/primitives/checked.h`, `core/arena.h`, and `core/pool.h`, which
everything else is built on.

### Class 2 — evidence that does not mean what it says

This class is specific to this project and is treated as security-relevant.

Canon-C publishes formal-verification results, coverage measurements, and
deviation records in `docs/`, and downstream users are invited to reuse them
as certification evidence. If a published claim is wrong, someone's safety
argument rests on a false premise — which is a defect in the product, because
the evidence *is* part of the product. Examples:

- An ACSL contract too weak to mean what its prose says, so the corresponding
  WP proof is vacuous or narrower than the documented guarantee.
- A residual classified as unreachable in `docs/deviations.md` that is in fact
  reachable on a supported platform or configuration.
- A deviation argument that holds at one supported target width but not
  another, while being stated unconditionally.
- A coverage or proof claim that measures a different code path than the one
  shipped — for instance if the verified configuration and the default build
  diverged without the divergence being documented.

This is not hypothetical. A CI job compiling the test suite under CompCert
found a test whose unstated 16-byte alignment assumption held on GCC and
Clang but not on a conforming compiler that does not over-align; the
consequence was that an MC/DC gap-closure test would silently stop exercising
the branch it existed for while still reporting success. That defect was in a
test rather than in shipped code, and it was found by tooling rather than
reported — but it is exactly the shape this class describes, and a report of
the same shape against a published claim would be handled as a vulnerability.

If you believe a published claim is wrong, please report it through the
channels above rather than opening a public issue, even though the underlying
records are public. Whether it warrants an advisory is a triage decision, and
we would rather make it in private first.

---

## What is not a vulnerability

The following are working as designed. Reporting them is welcome as ordinary
issues, but they will not be handled as vulnerabilities:

- **A contract firing.** `require_msg` and `ensure_msg` panic on precondition
  and postcondition violations by design — that is the mechanism stopping a
  bug before damage spreads, not a bug in itself. See the README's discussion
  of contracts versus error propagation.
- **A documented residual.** Every unproved proof obligation is published by
  ID in `docs/deviations.md` with its category and rationale, and CI fails if
  the set changes. These are disclosed, not hidden. A report that one is
  *misclassified* is Class 2 above and is very much in scope.
- **Misuse contrary to a documented contract.** Passing a NULL where the
  contract forbids it, exceeding a documented capacity, or using a borrow past
  the lifetime the API defines is caller error. If the documentation is
  ambiguous about which is which, that is worth reporting as an issue.
- **A platform-tier guard refusing to compile.** The `#error` guards in
  `types.h` and `sizes.h` are the intended behaviour on unsupported targets;
  they exist so that unsupported configurations fail loudly rather than
  silently producing wrong constants.
- **Absence of a hardening measure Canon-C does not claim.** Canon-C is not a
  cryptographic library, does not claim constant-time behaviour, and does not
  defend against an attacker with arbitrary write access to the calling
  program's memory.

---

## Security-relevant properties, for calibration

These may help you judge whether something you have found is in scope:

- **No hidden allocation.** `core/arena.h` and `core/pool.h` operate entirely
  on caller-supplied buffers and never call `malloc`. Heap use is reached only
  through `mem_alloc()` and `data/convenience/` (dynvec, smallvec, dynstring).
- **No global mutable state** except the contract handler pointer, which is
  installed once via `contract_set_handler()`.
- **Configuration changes behaviour.** `CANON_NO_REQUIRE`, `NDEBUG`,
  `CANON_STRICT`, `CANON_LIFETIME=debug`, and the `*_FORCE_FALLBACK` macros
  each change which checks are present at runtime. A report is much easier to
  act on if it names the configuration; a finding that only appears under one
  of them is still a valid finding.
- **Target width matters.** Tier 1 requires `size_t >= 32` bits. Behaviour on
  16-bit targets is out of scope for the Tier 1 layers by design, and the
  guard in `sizes.h` enforces that.
