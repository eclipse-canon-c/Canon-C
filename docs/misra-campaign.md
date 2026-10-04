# MISRA C:2012 — Campaign Record

This document exists because the per-record files (`deviations.md`,
`verification.md`, `traceability.md`) answer *"what is the status of finding
X?"* but not *"what happened here, and what is it worth?"* Sixteen commits
moved the advisory count from 1672 to 53, and a reader arriving cold cannot
reconstruct from the records alone what that number means. (The count has
been **54** since VERIFY-031 added one verifier-only declaration; see the
dated note at the end. Every "53" below describes the campaign's close.)

It is written to be read by someone deciding whether to trust this work.

---

## The headline, stated honestly

**The campaign's value was two real defects and a complete classification of
everything else. The count going down is mostly the classification becoming
explicit, not code becoming correct.**

That is a less impressive sentence than "we eliminated 1619 violations," and
it is the accurate one. A qualified assessor will ask exactly this question,
and the answer should not have to be extracted.

---

## Why the raw number cannot be read as a defect count

Four separate effects sit between "1672" and "53", and they do not decompose
into a clean arithmetic.

### 1. The two figures were measured with different instruments

On 2026-07-25 (Commit 12, `MISRA-SCAN-001`) the scan surface changed from
114 headers to 86. `*_impl.h` and `*_mangle.h` are internal template
fragments: they are never compiled alone and depend on macros their including
entry point supplies. Handed to cppcheck as standalone translation units,
they produced findings that described the scan rather than the code.

Excluding them cost 10 findings and **zero analysis coverage** — every
fragment is still reached through an entry point that is scanned, verified by
include-graph reachability (28 fragments, 0 unreached). But it means the
pre- and post-2026-07-25 numbers are **not comparable**, and the ledger is
kept in two segments for that reason:

```
pre-2026-07-25 surface (114 headers):  1672 → 108
corrected surface       (86 headers):    98 →  53 → 54
```

The last step is not part of the campaign: it is VERIFY-031's `__FRAMAC__`-only
`vsnprintf` declaration, the third rule-21.2 site of the same shape as
diag.h's two (see "2026-10-02 — 53 → 54" below).

Anyone quoting "1672 → 53" as a single reduction is quoting across an
instrument change.

### 2. Counts are lower bounds, and can rise after genuine cleanup

cppcheck reports at most **one MISRA violation per source line**. A line
violating three rules shows one until that one is fixed or suppressed, then
the next surfaces. This is measured, not theoretical: suppressing the 892
findings covered by the initial deviation records took the count from 1672 to
1004 — a drop of 668, not 892, because **224 previously shadowed findings
appeared on the same lines**.

A count that *rises* after cleanup is therefore surfacing, not regression.
Every count in this project is a count of visible findings under the
then-active suppression set.

### 3. A large share of the reduction is classification, not change

The current suppression set is 12 project-wide command-line rules
(`MISRA-DEV-001..010`) plus **153 inline per-site suppressions**:

| rule | sites | record | what it is |
|---|---|---|---|
| 20.7 | 117 | MISRA-DEV-012 | macro parameter parenthesisation in template machinery |
| 19.2 | 13 | MISRA-DEV-014 | unions — the tagged union *is* the Result type |
| 21.3 | 9 | MISRA-DEV-015 | `mem_alloc`/`mem_free` wrappers |
| 21.6 | 6 | MISRA-DEV-016 | `<stdio.h>` at the two documented entry points |
| 2.3 | 5 | MISRA-DEV-017 | the C99 static-assertion idiom — being declared *is* the assertion |
| 21.1 | 3 | — | reserved-identifier idiom |

None of these changed code. They record *why the code is the way it is*.
That has real value — it is the difference between an unknown and a known,
justified decision — but it is not defect removal, and this record does not
present it as such.

### 4. The rest were mechanical transforms of modest value

Rules 10.4, 15.6, 12.1, 14.4, 17.7, 20.7, 17.8, 10.7, 13.3, 12.3, 14.2 and
10.8 were driven to zero by changing code. Honestly assessed:

- **Some genuinely improved clarity.** Rule 10.8 ("name the composite, then
  cast the name") made several pointer-arithmetic expressions easier to read
  and reason about, and one of its sites exposed a contract that had been
  silently weakened by an earlier edit — caught by assembly diff.
- **Some were close to neutral.** Splitting `T x = A, y = B;` into two
  declarations (12.3) satisfies the rule and changes nothing a reader cares
  about. Several transforms were verified assembly-identical.
- **All of them cost verification effort.** Each sweep required a five-gate
  check: ctest, assembly equivalence, WP token proof, MC/DC A/B, and a local
  rescan. That effort was worth spending on the ones that found something and
  was overhead on the ones that did not.

---

## What the campaign actually found

Three defects, none of which the test suite caught by passing.

### Rule 5.7 → eleven public header pairs that could not be compiled together

Analysing rule 5.7 revealed that `fold_decl.h`, `fold_impl.h` and
`hashmap_decl.h` each instantiated `CANON_RESULT(bool, Error)` without
joining the `CANON_RESULT_BOOL_ERROR_DEFINED` guard protocol, and that
`log.h` carried a private variant aliased to the same tag — mutually
exclusive with any other instantiating header. A pairwise compile matrix
found **53 failing header pairs, 42 after the fix** — eleven pairs of public
headers that a user combining them could not compile.

Fixed in Commit 15, guarded by `test/integration/header_combination_test.c`,
which fails to build on the parent commit.

### Rule 11.8 → a const hole in a formally verified library

Ten accessors across `arena`, `pool`, `bitset`, `priority_queue` and
`stringbuf` accepted a `const` pointer and returned mutable access to the
object's storage. A caller holding only a `const Arena*` could obtain a
writable `bytes_t` and mutate the arena. Demonstrated under
`-Wall -Wextra -Wpedantic`, compiling cleanly:

```
before: 0x11
after : 0xEE      ← written through a const parameter
```

`pool_get` was the starkest case, returning a raw `void*` from a `const
Pool*` although `pool_get_const` already existed. The `const` was decoration.

Fixed in Commit 16 by following the convention `slice.h` and `array.h`
already used, which removed the qualifier cast entirely — so **rule 11.8
cleared by fix, with zero suppressions**. Recorded as API-001 in
`design-decisions.md`, guarded by
`test/integration/const_correctness_test.c`.

### CompCert → an alignment assumption that would have disabled an MC/DC closure

Not a MISRA finding, but from the same programme of adding instruments. The
CompCert portability job's first run rejected one translation unit and failed
one test. The second was `pool_test`'s `test_init_arena_alloc_fails_after_guard`,
which assumed a `static u8[65]` would be 16-byte aligned. C99 guarantees only
`alignof(unsigned char) == 1`; GCC and Clang over-align, CompCert does not.

The consequence was worse than a failed assertion: without the over-alignment
the setup allocation lands at `pad + 1` instead of `1`, `pool_init` returns
false at the *early guard* rather than at the line the test exists to reach,
and `EXPECT(!ok)` still passes. **The MC/DC gap-closure would have silently
stopped closing anything.** The test's own `EXPECT(arena_used == 1)` caught
it — that assertion is the canary and is documented as such.

---

## Two measurement-surface corrections

Twice the campaign found that the *instruments* were misreporting, and both
are recorded because a project whose product is evidence has to treat wrong
evidence as a defect.

**MCDC-001 — min/max ternaries are branchless.** GCC gimplifies a
min/max-shaped ternary to `MIN_EXPR` at every optimisation level including
`-O0`, emitting neither a branch nor a condition record. Commit 9's clamp
folds therefore moved the MC/DC aggregate from 1679/1900 to 1671/1892 with
**zero coverage lost** — every removed outcome was fully covered before the
fold and every missed-set was byte-identical. Found by a docs-versus-run
audit after the documented figures stopped matching the job output. The
standing rule: a min/max-shaped fold is a measurement-surface change, not a
coverage change, and must be verified with a pre/post per-line condition
diff.

**The max-per-file aggregation artifact.** The MC/DC report takes the maximum
condition total per file across translation units, so a broad-but-shallow
integration test displaces a narrow-but-thorough one as the maximum and
lowers the reported percentage for files nobody touched. Commit 15's
`header_combination_test` triggered this and it went unnoticed because the
coverage job is report-only. Both `test/integration/` compile-time guards are
now excluded from the coverage *run*, and untouched files stop moving.

---

## What this campaign does *not* claim

- **Not MISRA compliance.** cppcheck's MISRA addon covers roughly 60–70% of
  the rules and is not a qualified checker. For certification, a qualified
  tool (Polyspace, LDRA, PC-lint, Parasoft) is required. The workflow says
  this at the job that produces the number.
- **Not a defect-free library.** It claims that 54 findings remain (53 at
  the campaign's close, plus VERIFY-031's axiom declaration), each one
  classified, and that the instrument producing that number is itself tested
  — the `canary/misra_canary.h` carries three intentional violations and the
  job fails if they are not detected, because a silently-broken addon
  otherwise reports a fake-green zero.
- **Not that the remaining 54 are a backlog.** See below.

---

## The remaining 53, and why they are not a to-do list

*Current count 54 (2026-10-02, VERIFY-031): the 53 described here plus one
more 21.2 — the `21.2 x2` block below is `x3` today. Nothing else in this
section changes.*

The largest blocks, from the CI #1202 report:

```
5.7  x10    8.7  x5    15.1 x5    11.9 x4
17.2 x2    21.16 x2    22.8 x2    21.2 x2    5.5 x2    15.4 x2
```

That is the top of the distribution — 36 of the 53 — not the whole of it.
The remaining 17 are spread across rules with one to three sites each. The
complete per-rule, per-site breakdown is the `misra-report` artifact attached
to every CI run; it is deliberately not transcribed here, because a hand-copied
list is a second copy that will drift out of step with the one the tool
produces.


**The largest block, rule 5.7 ×10, has nothing left in it.** The real defect
5.7 pointed at — the eleven uncompilable header pairs — was found and fixed
in Commit 15. What remains is the *guard protocol itself* being visible to
the tool: `result_bool_Error` is textually declared across eighteen headers,
each guarded so that exactly one definition is active per translation unit,
with a test that fails on the parent commit if the protocol is broken.
cppcheck scanning each header standalone sees a repeated tag and reports 5.7.
It is the mechanism working, not a problem.

Two things could move that number, and neither is required:

1. **Write a deviation record.** Honest bookkeeping — the mechanism is now
   provably sound and worth documenting. Costs an afternoon, changes no code,
   moves 10 findings from "unexplained" to "explained."
2. **Refactor to a single canonical instantiation header.** Arguably better
   architecture — one source of truth for the shared type. But it is an
   API and layering change with real risk, and it should be judged as a
   design decision on its own merits, **not adopted because a rule counts
   the current arrangement.**

The same test applies to the rest. `11.9` is a null-pointer-constant
convention. `15.1` is `goto` in `file.h`'s cleanup paths — which the README
explicitly recommends as the right tool for error-handled functions. `8.7`
is linkage visible only to a standalone-TU scan. **These are findings, not
faults.**

The governing rule for this project: **a finding is worth acting on when
acting on it makes the code better, not when it makes the count smaller.**
Two of the seventeen rules driven to zero were worth it because they exposed
defects. Most of the others were worth it because the transform was cheap and
the result marginally clearer. Some were neither, and were classified rather
than changed — which is the correct outcome and should not be read as
avoidance.

---

## Instrument integrity

Three properties make the number above meaningful rather than decorative, and
each was established after a specific failure:

- **The canary.** `canary/misra_canary.h` carries three intentional
  violations and the job fails if they are not all detected. They are
  observed as rules 10.1, 14.4 and 21.6; per-line masking means the second
  can also surface as 15.1, which is why the workflow records it as
  "14.4/15.1" rather than pinning one spelling. This exists because the job once reported "0 violations" while
  cppcheck was silently discarding the addon's style-severity findings.
  Rule 21.6 appears in the canary, which is why the `MISRA-DEV-016` stdio
  deviation is applied per-site and never globally — a global suppression
  would blind the self-test.
- **The runner is pinned.** `ubuntu-24.04`, not `ubuntu-latest`, because the
  finding set is a function of the cppcheck version that comes from the
  runner image. During an image migration the count flipped between 780 and
  1004 across pushes. Pinning the image pins the tool pins the baseline.
- **The advisory count is pinned and echoed.** The expected real count is
  written into the job. It is advisory — real violations do not fail the
  build — but a change in it is visible in the log rather than silent.

  **This earned its keep at CI #1262.** The bitset arc's VERIFY-020 F1
  guard, `require_msg(capacity <= CANON_USIZE_MAX - 63u, ...)`, tripped
  rule 12.1 (explicit operator precedence) and moved the count 53 → 54.
  Nothing failed — advisory findings do not fail the build — and the
  commit that introduced it had claimed to be pin-neutral, having
  measured MC/DC and reasoned about WP while never considering MISRA at
  all. The echoed count is the only reason it was noticed. Parenthesised
  at fad155a to match `core/memory.h:343`/`:381`, which already wrote the
  bracketed form; back to 53 at CI #1263 and unmoved since.

  The general lesson is not about parentheses. A claim that a change is
  "pin-neutral" has to **enumerate** the pins — this project has four
  gating analysis surfaces (WP, MC/DC, MISRA, clang-tidy) across 21 of
  its 29 CI jobs, and naming a number from memory instead of reading the
  workflow got it wrong twice in consecutive commits.

---

## Where the records live

| question | file |
|---|---|
| Why is finding X acceptable? | `deviations.md` — MISRA-DEV-001..017, MISRA-SCAN-001, MISRA-CFG-001 |
| What does WP prove, and what does it not? | `verification.md`, `deviations.md` VERIFY-001..018 |
| What is the coverage, and when did it move? | `traceability.md` — including the History table |
| Why is the API shaped this way? | `design-decisions.md` — API-001, OWN-001..003 |
| What happened across the campaign? | this file |

There is no CHANGELOG. The change record is the CI history: every landed
commit has an Actions run carrying its verification, coverage and MISRA
results, and the records above cite those runs by number.

---

## 2026-10-02 — 53 → 54 (VERIFY-031)

One finding added, rule 21.2, at `data/stringbuf.h`'s `__FRAMAC__`-only
extern redeclaration of `vsnprintf`. That redeclaration is the trusted
axiom VERIFY-031 design item 5 specifies, and it has the same shape as
`semantics/diag.h`'s two (`snprintf`, `fprintf`), already among the 53.
Cppcheck analyses every configuration, including `__FRAMAC__`, so it
cannot be avoided without moving the axiom out of the header it specifies.
First seen at CI #1319 (f6e54ca), reproduced locally with Cppcheck 2.13.0,
and pinned in the job's ledger from the next commit. `21.2 x2` in the table
above is now `x3`.
