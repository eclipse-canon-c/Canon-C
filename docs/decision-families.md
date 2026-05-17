# Canon-C Decision Families

This file is the namespace guide for Canon-C's ID-namespaced documents:
`docs/design-decisions.md` (OWN-NNN), `docs/verification.md` (VERIFY-NNN),
`docs/traceability.md` (MCDC-NNN), and `docs/deviations.md` (which
collects entries from all three plus MISRA-CFG-NNN). It records what
each namespace is for, what shape its entries take, and what
categories of decision belong in each.

The guide is descriptive, not prescriptive. It documents patterns
visible in the entries that already exist, so future maintainers
landing on a new design call can route it to the right namespace
without inventing a convention. Each section is grounded in the
entries that motivate it; sections for namespaces that don't yet have
enough accumulated evidence to describe their categories are marked
as pending.

This file does not record counts, schedules, or forecasts. Entry
counts vary with the project's actual decision rate; predicting them
gives the file a forecast genre that decays. The stable content is
the *shape* of decisions in each family — what they look like, what
sources ground them, when a new decision earns an entry rather than
being implicit in the codebase.

---

## How the namespaces compose

Canon-C has four ID-namespaced families. Each has a primary home file
and may produce cross-references in other files:

| Namespace      | Primary home                | What it records                                                       | Cross-references                            |
| -------------- | --------------------------- | --------------------------------------------------------------------- | ------------------------------------------- |
| OWN-NNN        | `docs/design-decisions.md`  | Architectural and substrate decisions about ownership and lifetime    | README, CMakeLists.txt, CHANGELOG, VERIFY-* |
| VERIFY-NNN     | `docs/verification.md`      | Per-header proof obligation accounting and manually discharged goals  | `docs/deviations.md` (unproved goals), CI   |
| MCDC-NNN       | `docs/traceability.md`      | MC/DC coverage gaps and their dispositions (reachable vs unreachable) | `docs/deviations.md`, VERIFY-* (when WP discharges a gap) |
| MISRA-CFG-NNN  | `docs/deviations.md`        | MISRA C:2012 deviations with rationale                                | Static analysis tooling configuration       |

The four namespaces compose: a single piece of code can have entries
in multiple files at once. The canonical example, traceable through
the OWN-001 cross-references, is `core/slice.h`:

- **VERIFY-007** records 23 documented unproved goals (memcmp call-site
  preconditions, strlen valid_string, contract handler non-termination).
- **MCDC-002** records four `!ptr` defensive branches measured as
  uncovered at 93.1% MC/DC, with WP discharging them as
  type-invariant-unreachable. The two evidence streams complement
  each other rather than converging.
- **OWN-001 §3** (Contract table) doesn't directly entry on `slice.h`
  but cross-references the `BORROW_LT_INHERIT_` macro in
  `semantics/borrow.h` that propagates lifetime tracking through
  `slice.h`'s typed views.

A future maintainer asking "what's the verification story for
`slice.h`" lands on three entries in three files; each tells one
piece of the story, and the cross-references make the composition
explicit.

---

## OWN-NNN — Ownership and lifetime substrate

OWN-NNN entries live in `docs/design-decisions.md`. They record
*architectural* decisions about how the codebase models ownership,
lifetime, and borrowing — distinct from compile-time annotation
choices, which the README's *"What about compile-time ownership
enforcement?"* section covers in prose, and distinct from per-header
verification accounting, which lives in VERIFY-NNN.

An OWN- entry is appropriate when a decision:

1. Affects multiple owning container types, or sets a contract that
   future container types are expected to follow.
2. Has a real trade-off — a path-not-taken that future readers might
   second-guess without the entry's explanation.
3. Touches ABI, build-time configuration, or substrate composition
   in a way that isn't self-evident from header reading alone.

Decisions that the README's design philosophy already determines (no
hidden allocation, no global state, no clever tricks, C99-strict)
don't need OWN- entries — the philosophy is the record. OWN- exists
for decisions where the philosophy has tensions or doesn't fully
specify the answer.

### Four categories of OWN- decisions

The categories below describe decision *shapes* that already exist in
Canon-C's structure. Each is grounded in evidence visible in the
codebase as of v1.3.0; the source for each is named explicitly.

**Category A — Closing known limitations from earlier entries.**

OWN-001 §4 (Honesty Boundary) documents what the runtime lifetime
substrate deliberately does not catch: two-reset cycles on
`Arena`/`Pool`, arena-reset under a `Pool` or arena-backed
`StringBuf`, use-of-mutated-value across container mutations. Each
documented not-caught case is a potential follow-up entry: a future
OWN-NNN that closes the gap by extending the substrate, with the
earlier entry's Honesty Boundary serving as its motivation.

The canonical Category A example is OWN-002 (forward-referenced in
OWN-001 §8): migrating `Arena` and `Pool` from XOR-with-constant
restamp to the per-TU counter pattern used by every Phase 3+
container, closing the two-reset cycle.

Category A entries are bounded: the substrate has a finite number of
seams, and each seam closes at most once. The pattern propagates
naturally — every OWN- entry that ships with an Honesty Boundary
section produces a known set of candidate follow-ups.

**Category B — Extending the substrate to new domains.**

The lifetime substrate is currently single-threaded by construction.
Every container header (`dynvec.h`, `dynstring.h`, `smallvec.h`,
`bitset.h`, `stringbuf.h`, and the Phase 3 containers) carries the
same caveat: *"No thread-safety guarantee: if a single TU's
constructors or restamps are invoked concurrently, the counter may
race and collide."* Adding atomic-counter support gated by a feature
macro (`CANON_LIFETIME_ATOMIC` or similar) would be a Category B
entry — it extends the substrate to a domain (concurrent
construction) that the current design explicitly excludes.

The same shape applies to any future substrate extension whose
semantics differ from what's in the OWN-001 §3 contract table:

- Region-arena unification (if `Region`'s separate `id`/`open` fields
  are ever folded into `lifetime_t`, that's a Category B entry plus
  an ABI note — overlap with Category C).
- A new container family whose lifetime contract requires events not
  currently in the table (close + restamp + something else).
- Cross-translation-unit lifetime tracking, if that ever becomes a
  goal.

Category B entries are open-ended in count but each one is
high-signal: it adds a new column or row to the substrate's
conceptual model, not just a refinement of existing behavior.

**Category C — ABI or layout decisions affecting every owning type.**

The substrate's `CANON_LIFETIME=off/debug` knob is the canonical
Category C decision, recorded in OWN-001 §2: default builds are
byte-identical to v1.2.x; mixing `off` and `debug` translation units
in one binary is undefined. Adding a second knob (a generation
counter, an atomic mode, a logging hook) would need a Category C
entry explaining what it changes, what the ABI guarantee becomes,
and how it composes with the existing knob.

These are rare but high-stakes: ABI decisions affect every consumer
of Canon-C simultaneously. The README's *"Bare-metal and embedded
use"* section and the CMakeLists `CANON_LIFETIME` comment block are
the user-facing companion to Category C entries; the entry itself
records the rationale, the alternatives considered, and the
composition rules.

**Category D — Boundary decisions with the verification layer.**

As WP coverage walks up the dependency tree from `core/primitives/`
into the substrate headers, design choices will surface at the
boundary between OWN- and VERIFY-. The CI file's *"Candidate order"*
comment lists `region.h`, `arena.h`, `pool.h`, and `borrow.h` as the
next headers to annotate. Each of those will produce VERIFY- entries
recording the proof state. But the substrate headers also have
macro-templated surfaces that WP cannot reach by construction
(OWN-001 §7), and decisions about how to handle that boundary belong
in OWN-, not VERIFY-:

- How to formalize the OWN-001 §3 contract table as ACSL
  specifications for the non-macro substrate functions.
- Whether to generate per-instantiation ACSL specs for macro-
  templated functions via a code generator (a Category D + Category
  C decision: it changes the build process).
- What to do about substrate-layer residuals that emerge once WP
  reaches `arena.h` or `borrow.h` — Category D records the
  architectural call; VERIFY-NNN records the specific residual class
  with its manual discharge argument.

Category D entries sit at the seam between the two namespaces. The
rule of thumb: if the decision is *what to prove* or *how to model
the proof*, it's an OWN- entry. If it's *the proof itself* or *the
manual argument for an unproved goal*, it's a VERIFY- entry.

### Worked example: OWN-001

OWN-001 (Runtime lifetime substrate) is the worked example for the
OWN- namespace. It demonstrates the entry shape future entries
should aim for:

- **Status box** with merge commits, CI run number, and the build
  knob's name.
- **Phase chronology table** with marker commits — every claim
  grounded in a sourced commit SHA.
- **Context section** explaining why the decision exists, with
  explicit cross-reference to the README sections that motivate it.
- **Decision section** stating the chosen approach with type
  signatures and the ABI guarantee.
- **Contract table** as the spec — descriptive, not evaluative.
- **Honesty Boundary** as a separate section listing what is caught
  and what is deliberately not caught, with test names pinning each
  claim.
- **Chronology** for any retrofits or notable design pivots, with
  direct quotations from source headers where the source narrates
  itself better than reconstruction would.
- **Alternatives considered** with the one fully-sourced rejection
  written out; speculative alternatives explicitly removed rather
  than carried as filler.
- **Verification posture** distinguishing what's runtime-checked
  forever (macro-templated bodies) from what's on the verification
  roadmap (non-macro substrate headers).
- **Cross-references** to README anchors, `CMakeLists.txt` blocks,
  the three verification docs, and forward-referenced OWN-NNN
  entries.

Future OWN- entries don't need to be as long as OWN-001 (which
covers a five-phase substrate across twelve container types). Most
will be shorter, covering one decision with one Honesty Boundary
case. But the shape — sourced status, explicit cross-references,
honest about what isn't covered — is the template.

---

## VERIFY-NNN — Per-header proof obligation accounting

VERIFY-NNN entries live in `docs/verification.md`, with unproved-goal
details cross-referenced into `docs/deviations.md`. They record what
WP proves for each verified header, what it leaves unproved, and the
manual arguments that discharge the residuals.

Entries to date: VERIFY-001 through VERIFY-008 cover `checked.h`,
`bits.h`, `compare.h`, `ptr.h`, `slice.h`, and `memory.h`.

Categories of VERIFY- decisions are not yet documented here because
the established entries' shapes are still settling. The CI file's
inline comments on each WP step (the per-header rationale blocks for
the unproved goal counts and their categories) currently serve as the
shape-of-entry documentation. Once the substrate-layer headers
(`arena.h`, `pool.h`, `borrow.h`, etc.) land in VERIFY-, this
section will document the patterns that emerged from accumulating
~10+ entries.

For now, the canonical references for VERIFY- entry shape are the CI
file's WP wrapper steps (each names the expected proved/unproved goal
counts and the categories) and the existing entries themselves in
`docs/verification.md` and `docs/deviations.md`.

---

## MCDC-NNN — MC/DC coverage gaps and their dispositions

MCDC-NNN entries live in `docs/traceability.md`. They record
MC/DC coverage gaps measured by GCC 14 `-fcondition-coverage` and
classify each gap by its disposition: reachable-and-untested,
API-unreachable, WP-discharged-unreachable, or
defensively-redundant.

Entries to date: MCDC-002 is the referenced example, covering four
`!ptr` defensive branches in `bytes_slice`, `bytes_skip`,
`str_slice`, and `str_skip` in `core/slice.h`. These are documented
as API-unreachable in MC/DC isolation and (per VERIFY-007) formally
proved unreachable by WP under the `bytes_invariant` /
`str_invariant` type predicates.

The MCDC- and VERIFY- composition shown in MCDC-002 (gcov measures
the source, WP proves the predicate that makes the source
unreachable, the two streams complement rather than converge) is
likely the canonical category. Other categories — reachable gaps
closed by new tests, defensively-redundant branches kept for
robustness — will be documented here once enough entries exist to
describe their shape.

---

## MISRA-CFG-NNN — MISRA C:2012 deviations

MISRA-CFG-NNN entries live in `docs/deviations.md` alongside the
unproved-goal entries cross-referenced from VERIFY-NNN. They record
MISRA C:2012 rules that Canon-C deliberately deviates from, with
rationale.

Entries to date: the CI file's `misra` job runs MISRA advisory
analysis via Cppcheck's MISRA addon and is currently non-blocking
(*"Advisory only — does not fail the build"*). The job's output
notes that Cppcheck covers approximately 60-70% of MISRA C:2012
rules and that certification-grade analysis requires a qualified
tool (Polyspace, LDRA, PC-lint, Parasoft).

Categories of MISRA-CFG- decisions will be documented here once
enough deviations accumulate to describe their shape. The current
state — MISRA analysis is advisory, certification-tooling-dependent,
and not yet producing committed entries — means this namespace is
the least settled of the four.

---

## When to add a section to this file

A new section earns its place when:

1. The namespace has accumulated enough entries (roughly three or
   more) to make the *shape* of its decisions visible, not just the
   shape of a single example.
2. The patterns in those entries are stable — repeated decision
   shapes, not one-offs.
3. The shape isn't already covered by an existing section or by the
   README's design philosophy.

When in doubt, leave the section pending. A "Pending evidence"
marker (as used for VERIFY-, MCDC-, and MISRA-CFG- above) is more
honest than a section that invents categories before the entries
demand them. The OWN- section above is the standard: four
categories, each grounded in specific entries or sourced caveats
visible in the codebase.

---

## What this file does not record

- **Entry counts or schedules.** How many OWN- or VERIFY- entries
  will exist at v2.0.0 is unknown; estimating it adds a forecast
  genre that decays. The stable content is the shape, not the count.
- **Specific future entries.** OWN-002 is forward-referenced in
  OWN-001 because that specific migration is implied by OWN-001's
  Honesty Boundary; it earns the reference. Speculation about
  OWN-003 or VERIFY-009 does not belong here.
- **Project roadmap.** Roadmap belongs in CHANGELOG (for shipped
  releases) and in issue tracker / project board (for in-flight
  work). This file documents the decision-record system, not the
  project's forward plan.
- **Tooling configuration.** What checks run in CI, which provers
  are used, which suppressions are enabled — those live in
  `CMakeLists.txt`, the CI workflow file, and per-tool configuration
  files. This file references those configurations but does not
  duplicate them.

The principle behind these omissions is the same as OWN-001's:
descriptive, sourced, honest about what isn't known. Forecasts and
plans live in genres better suited to revision; decision records
live here and are not revised in place.
