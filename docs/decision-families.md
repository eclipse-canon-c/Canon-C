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

- **VERIFY-007** records 15 documented unproved goals (memcmp call-site
  valid/danglingness preconditions, strlen valid_string, contract handler
  non-termination); the 8 initialization goals it formerly recorded were
  closed by explicit `\initialized` preconditions (see VERIFY-012).
- **MCDC-002** records four `!ptr` defensive branches measured as
  uncovered at 92.6% MC/DC (50/54 post-Commit-9 surface), with WP discharging them as
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

A second composition is now visible at the `semantics/` layer:
`option` carries **VERIFY-014** (driver WP run, 189/223, 34 documented
residuals) and **MCDC-006** (the `expect` panic-on-absent branch,
unreachable) at once, the first VERIFY-/MCDC- pair on a driver-verified
Shape-B macro module rather than an in-place header.

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

The canonical Category A example is OWN-002 (shipped at CI #944):
migrating `Arena` and `Pool` from XOR-with-constant restamp to the
per-TU counter pattern used by every Phase 3+ container, closing the
two-reset cycle. See `docs/design-decisions.md` for the entry. OWN-002
is also the worked example for what a Category A entry looks like in
practice — short, mechanical, grounded in the originating entry's
Honesty Boundary and pinned by regression tests that fail the build
if the closure is reverted.

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
into the substrate headers, design choices surface at the boundary
between OWN- and VERIFY-. The substrate headers have macro-templated
surfaces that WP cannot reach by construction (OWN-001 §7), and
decisions about how to handle that boundary belong in OWN-, not
VERIFY-:

- How to formalize the OWN-001 §3 contract table as ACSL
  specifications for the non-macro substrate functions.
- Whether to generate per-instantiation ACSL specs for macro-
  templated functions via a code generator (a Category D + Category
  C decision: it changes the build process).
- What to do about substrate-layer residuals that emerge once WP
  reaches a header — Category D records the architectural call;
  VERIFY-NNN records the specific residual class with its manual
  discharge argument.

The canonical Category D example is OWN-003 (shipped at CI #992): the
decision to dispatch region cleanup hooks through an opaque,
caller-supplied function pointer with no ACSL `calls` clause,
accepting the resulting 19 region_end WP residuals as region.h's
documented verification boundary rather than redesigning the hook
mechanism to be WP-tractable. OWN-003 is the worked example of a
Category D decision: the call is *what to prove / how to model the
proof* (keep the opaque hook, accept the residual), while its
proof-side companion VERIFY-011 records the residual class itself
with the manual discharge argument.

Category D entries sit at the seam between the two namespaces. The
rule of thumb: if the decision is *what to prove* or *how to model
the proof*, it's an OWN- entry. If it's *the proof itself* or *the
manual argument for an unproved goal*, it's a VERIFY- entry. (Note
that not every opaque-callee residual produces an OWN- entry:
`option`'s combinators dispatch caller-supplied function pointers and
incur the same WP residual class as OWN-003's hooks — see VERIFY-014 —
but that is intrinsic to what a combinator *is*, with no
path-not-taken to record, so it carries a VERIFY- entry and no OWN-
one. Category D applies when there was an architectural choice about
the boundary, not merely a residual at it.)

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
are shorter, covering one decision with one Honesty Boundary case.
OWN-002 demonstrates the shorter shape: a single closure of a single
bullet from OWN-001 §4, mechanical migration, regression tests
pinning the no-cycle property, three commits across one CI run.
OWN-003 demonstrates the Category D shape: a single architectural
call at the OWN/VERIFY seam, its proof-side consequences recorded
separately in VERIFY-011. But the shape — sourced status, explicit
cross-references, honest about what isn't covered — is the template
that scales from OWN-001's substrate-wide scope down to the
single-decision entries above it.

---

## VERIFY-NNN — Per-header proof obligation accounting

VERIFY-NNN entries live in `docs/verification.md`, with unproved-goal
details cross-referenced into `docs/deviations.md`. They record what
WP proves for each verified header, what it leaves unproved, and the
manual arguments that discharge the residuals.

Entries to date span the full `core/` stack and the first two
`semantics/` modules: VERIFY-001 through VERIFY-012 cover `checked.h`,
`bits.h`, `compare.h`, `ptr.h`, `slice.h`, `memory.h`, `arena.h`,
`pool.h`, and `region.h`, plus the cross-cutting VERIFY-012
contract-strengthening pass that closed the initialization sub-class
across five headers at once; VERIFY-013 (`error.h`) and VERIFY-014
(`option`) extend the namespace into `semantics/`, with VERIFY-014 the
first **driver-verified** entry — a Shape-B macro module proved through
`vmacros/vdrivers/option_verify.h` rather than an in-place header (see
`docs/vmacros.md` for the driver mechanism).

With the `core/` layer complete and the `semantics/` layer underway,
enough VERIFY- entries now exist to describe the namespace's recurring
shapes. The categories below are descriptive of patterns visible
across VERIFY-002 through VERIFY-014.

### Recurring shapes of VERIFY- entries

**Shape 1 — Manually discharged goals.** A small, named set of
obligations that no automated prover closes, with a written
mathematical argument standing in for the proof. VERIFY-002
(checked.h's two u64-add-overflow goals, discharged by a
modular-arithmetic argument) is the canonical instance. The defining
feature is that the goal is *true and provable by hand* but outside
the SMT solvers' default integer theory; CI enforces that exactly the
named goals time out and no others.

**Shape 2 — WP feature-gap residuals.** Obligations WP cannot
discharge because the verifier itself has not implemented the relevant
ACSL primitive. VERIFY-007 (slice.h's `\dangling` memcmp
preconditions) and VERIFY-008 category 1 (memory.h's `\fresh` /
`\freeable` allocation reasoning) are the allocation/danglingness
instances; VERIFY-014 (option's combinator function-pointer dispatch —
no `calls` clause for an arbitrary callee, `\valid_function`
unimplemented in Frama-C 29) is the indirect-call variant of the same
shape, sharing its root cause with region_end's opaque-hook residuals
(VERIFY-011 category 1 / OWN-003). The defining feature is that
strengthening the contract does *not* help — the gap is in Frama-C 29,
confirmed by the WP warning text quoted verbatim in the entry.
VERIFY-012 is the important counter-case: it proved that *one* member
of this shape (`\initialized`) was misclassified — WP *can* discharge
it once stated as a precondition — which is why VERIFY- entries quote
the verifier's own warning rather than asserting a feature gap from
inference.

**Shape 3 — Inherited residuals.** Goals that re-emerge unchanged in a
downstream header's proof run because it includes an already-verified
header transitively. VERIFY-008 (memory.h inherits 23 from the
substrate), VERIFY-009 (arena.h inherits memory.h's full 43),
VERIFY-010 (pool.h inherits arena.h's full 89), and VERIFY-011
(region.h inherits arena.h's full 89) are the substrate-stack
instances; VERIFY-014 carries a minimal version of the same shape —
option inherits exactly the 2 contract.h handler goals it reaches
through `expect`, and nothing deeper, because it does not pull in the
core/ substrate. The defining feature is that the inherited count
equals the upstream's *total* for that include edge, not greater — the
composable-verification thesis, supported across five core/ composition
layers and now at the semantics/ boundary. An inherited-residual
section in a VERIFY- entry is a table mapping each goal back to its
originating VERIFY- entry by name.

**Shape 4 — Deliberate spec-strength tradeoffs.** A residual kept open
on purpose because closing it would cost more than the residual does.
VERIFY-007 category 2 (slice.h's `str_from_cstr` omitting
`valid_string` to avoid a project-wide stdlib dependency) and
VERIFY-006's forward-implication note (ptr.h's empty `nonnull`
behaviors, kept empty so the residual cascade lands downstream rather
than minting new ptr.h goals) are the instances. The defining feature
is an explicit cost comparison: the entry argues why the residual is
cheaper than the closure.

The canonical references for VERIFY- entry *structure* (status box,
per-category goal lists, manual proof arguments, CI wrapper
enforcement) are the entries themselves in `docs/verification.md` and
`docs/deviations.md`, and the CI file's WP wrapper steps, which name
the expected proved/unproved counts per header.

---

## MCDC-NNN — MC/DC coverage gaps and their dispositions

MCDC-NNN entries live in `docs/traceability.md`. They record
MC/DC coverage gaps measured by GCC 14 `-fcondition-coverage` and
classify each gap by its disposition: reachable-and-untested,
API-unreachable, WP-discharged-unreachable, or
defensively-redundant.

Entries to date: MCDC-001 (the coverage-flags methodology) plus
MCDC-002 through MCDC-006. MCDC-002 through MCDC-005 are one per
verified `core/` header that carries an unreachable defensive shape —
slice.h (`!ptr` branches), arena.h (overflow-guard subconditions),
pool.h (`!pool->arena` disjunct), and region.h (`!h->fn` hook guard).
MCDC-006 is the first `semantics/`-layer and first Shape-B entry —
`option`'s `expect` panic-on-absent branch, measured through the cover
TU `vmacros/coverage/option_cover.c`.

With five header- and module-level entries accumulated, the
namespace's recurring shape is now visible:

**The cross-stream complement.** The dominant MCDC- category, present
in all of MCDC-002 through MCDC-006, is the gcov/WP complement: gcov
measures a defensive branch as source-level uncovered, and a separate
evidence stream proves it unreachable. The *source* of unreachability
varies across three variants within this one shape. In
MCDC-002/003/004 the proof is a *type invariant* (`bytes_invariant`,
`arena_invariant`, `pool_invariant`). In MCDC-005 it is a *runtime
construction invariant* (region_register's no-NULL-hook discipline)
rather than a named predicate — a wrinkle worth flagging, because an
auditor should not expect a "region_invariant discharges this" claim
for MCDC-005. In MCDC-006 it is neither: it is a *contract-violation /
panic* branch — `option`'s `expect` dispatches to the panic handler on
the absent path, and WP models that call as non-terminating (it reports
the handler call as a recursive call that "must be unreachable"), while
gcov measures the same branch as not-executed under `CANON_NO_REQUIRE`.
MCDC-006's unreachability source is closest to MCDC-001's `contract.h
0/2` artifact — both rooted in the panic/disabled-contract machinery —
even though its evidence pattern is the MCDC-002-through-005 cross-stream
complement. The two streams complement rather than converge: gcov
instruments the source, the proof establishes reachability, and the
entry documents both. Each header's *shape* differs (the specific
defensive branch and its unreachability source); the *evidence pattern*
is identical. MCDC-006 is also the first MCDC- entry paired with a
VERIFY- entry on a **driver-verified Shape-B module** (VERIFY-014)
rather than an in-place header — the cover TU exists precisely because
the Shape-B conditions are otherwise attributed to the excluded
`_test.c` and filtered out (see `docs/vmacros.md`).

**The methodology artifact.** MCDC-001 is a different shape — not a
per-header gap but the project-wide flags (`CANON_NO_REQUIRE`,
`CANON_CHECKED_FORCE_FALLBACK`, `CANON_BITS_FORCE_FALLBACK`) that
change the code under measurement to align coverage with the verified
path. The recurring `contract.h 0/2` artifact and the
`arena_debug_update_` no-op outcomes (MCDC-003 category 2) are
instances of gcov-14 instrumentation behavior on disabled-macro
expansion, traceable back to MCDC-001's methodology. MCDC-006's single
miss is downstream of the same methodology — `CANON_NO_REQUIRE` is what
removes the `expect` panic path from the measured code, which is why the
branch reads as uncovered rather than as a test gap.

**The reachable gap closed by a test.** A minority shape: an outcome
that *looks* unreachable but is a genuine test gap, closed by a
targeted test rather than documented as a deviation. MCDC-004's
line-309 closure (`arena_alloc` failing after pool_init's coarse
`arena_remaining` guard) and MCDC-003's line-510 closure
(`arena_try_alloc_aligned`'s compound return) are the instances. The
defining feature is that the entry distinguishes this from
unreachability explicitly — it was a missing test, not a provable
dead path, and the fix is a test, not a deviation record.

A future header introducing its own public `{ptr, len}` type or
analogous defensive code (stringbuf.h is the next candidate, listed
provisionally at 74.2% in MCDC-002's pattern note) will need its own
per-header MCDC- entry; its unreachability shape and number are not
transferable from the existing entries. Likewise, the remaining
Shape-B macro modules (`deque`, `fold` — result and vec have since
landed as MCDC-007 and MCDC-010) will each get their own MCDC- entry
paired with a driver, following option/MCDC-006's cover-TU pattern;
their unreachability shapes are not transferable
either.

---

## MISRA-CFG-NNN — MISRA C:2012 deviations

MISRA-CFG-NNN entries live in `docs/deviations.md` alongside the
unproved-goal entries cross-referenced from VERIFY-NNN. They record
MISRA C:2012 rules that Canon-C deliberately deviates from, with
rationale.

Entries to date: MISRA-CFG-001 (the Cppcheck `*_impl.h`
macro-templated-header configuration limitation). The CI file's
`misra` job runs MISRA advisory analysis via Cppcheck's MISRA addon
and is currently non-blocking (*"Advisory only — does not fail the
build"*). The job's output notes that Cppcheck covers approximately
60-70% of MISRA C:2012 rules and that certification-grade analysis
requires a qualified tool (Polyspace, LDRA, PC-lint, Parasoft).

**Pending evidence.** With a single committed entry, the namespace's
categories are not yet describable — MISRA-CFG-001 is a tool-limitation
deviation, but whether that's the dominant category or just the first
one cannot be known until more entries accumulate. This namespace
remains the least settled of the four; the MISRA job stays advisory
and certification-tooling-dependent until a qualified checker produces
categorized output. Categories will be documented here once enough
deviations accumulate to describe their shape.

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
marker (as still used for MISRA-CFG- above) is more honest than a
section that invents categories before the entries demand them. The
OWN-, VERIFY-, and MCDC- sections have each crossed the threshold —
four sourced OWN- categories, four VERIFY- shapes across fourteen
entries, three MCDC- shapes across six entries. MISRA-CFG- has not,
and is marked accordingly.

---

## What this file does not record

- **Entry counts or schedules.** How many OWN- or VERIFY- entries
  will exist at v2.0.0 is unknown; estimating it adds a forecast
  genre that decays. The stable content is the shape, not the count.
  (Where this file names current entries — VERIFY-001 through 014,
  OWN-001 through 003, MCDC-001 through 006 — it is recording the
  present state to ground the category descriptions, not forecasting
  a future total.)
- **Specific future entries.** OWN-002 and OWN-003 were
  forward-referenced from OWN-001 because each was implied by
  OWN-001's Honesty Boundary or §7 verification posture; they earned
  the reference and have since shipped. Speculation about entries not
  implied by an existing entry's Honesty Boundary or equivalent does
  not belong here — forward references earn their place only when the
  originating entry directly implies them.
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
