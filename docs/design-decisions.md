<!--
  ============================================================================
  OWN-001 — Runtime lifetime substrate for owning containers
  ----------------------------------------------------------------------------

  Cross-references to confirm before pushing:
    - README anchor "Borrow lifetime — know when a borrowed value is still
      valid"  — verified present in the README example block.
    - README anchor "What about compile-time ownership enforcement?"
      — verified present; covers DEFINE_OWNED / DEFINE_BORROWED.
    - README anchor "Lifetime token types shared across owning modules"
      — verified present in the core/primitives/ getting-started section.
    - README anchor "From shared vocabulary to compositional verification"
      — verified present; positions the substrate as the runtime layer of
      the same compositional bet (§1, §7).
    - CMakeLists.txt comment block on CANON_LIFETIME (off/debug)
      — verified present; documents the ABI guarantee.
    - docs/verification.md, docs/deviations.md, docs/traceability.md
      — referenced as the canonical homes for VERIFY-NNN / MCDC-NNN entries.
      Non-macro substrate entries landed: VERIFY-009/-010/-011 and, closing
      the set, VERIFY-016 + MCDC-008 (borrow.h, CI #1110); §7 and §8 carry
      the ID-specific cross-references OWN-001 promised.
    - CHANGELOG entry for v1.3.0 — TODO: confirm anchor when CHANGELOG is
      updated for the release. Inline as {TODO: CHANGELOG v1.3.0 anchor}.
    - OWN-002 — Arena/Pool restamp migration; shipped post-v1.3.0 at
      CI #944. See OWN-002 entry below.
  ============================================================================
-->

## OWN-001 — Runtime lifetime substrate for owning containers

This entry records the runtime lifetime substrate shipped in v1.3.0:
opt-in runtime tracking of borrow validity across container reset,
realloc, swap, and teardown, enabled by `CANON_LIFETIME=debug` and
byte-identical to v1.2.x in default builds. The substrate covers every
owning container type in `core/` and `data/` and composes with the
formally-verified primitive layer described in the README's
*"From shared vocabulary to compositional verification"* section.

| Field          | Value                                                             |
| -------------- | ----------------------------------------------------------------- |
| Status         | Shipped in v1.3.0                                                 |
| Merged at      | Phase 5 sequence: CI #936 (`55b6aaf`, bitset substrate), CI #937 (`302cf0f`, stringbuf substrate), CI #938 (`da6ed2e`, Phase 5 tests). v1.3.0 = state of master at `da6ed2e`. |
| Last green     | CI #938 across all 16 configs (8 default × `build` + 8 lifetime-debug × `lifetime-debug`, ubuntu/windows/macos × Release/Debug × default/lifetime-debug) at v1.3.0 ship. Subsequent extensions tracked under their own entries (see OWN-002 for the post-v1.3.0 Arena/Pool migration). |
| Scope          | All owning container types in `core/` and `data/`                 |
| Build knob     | `CANON_LIFETIME=off` (default) or `CANON_LIFETIME=debug` — see `CMakeLists.txt` |
| Verification   | Runtime-validated via `test/semantics/borrow_test.c` across all 16 configs. The non-macro substrate is now WP-verified end to end: `arena.h` (VERIFY-009), `pool.h` (VERIFY-010), `region.h` including `lifetime_assert_valid` (VERIFY-011), `lifetime.h` (N/A — typedefs only), and the non-macro surface of `semantics/borrow.h` (VERIFY-016, verified under the default `CANON_LIFETIME`-off configuration — the exact shipped ABI bodies). Macro-templated bodies remain runtime-only by construction (see §7). |
| Cross-refs     | README sections *"Borrow lifetime — know when a borrowed value is still valid"*, *"What about compile-time ownership enforcement?"*, *"From shared vocabulary to compositional verification"*; `CMakeLists.txt` `CANON_LIFETIME` block; `docs/verification.md`, `docs/deviations.md`, `docs/traceability.md` (substrate VERIFY-NNN entries land here as headers get annotated); {TODO: CHANGELOG v1.3.0 anchor}; OWN-002 (Arena/Pool restamp migration, shipped post-v1.3.0). |

### Phase chronology

| Phase | Dates           | Scope                                                                                                                | Marker commits / runs                                         |
| ----- | --------------- | -------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| 1     | May 11–13, 2026 | `lifetime.h` introduced; `arena`, `region`, `pool` instrumented; `lifetime_assert_valid` added to `region.h`         | CI #865 (`59ddf7d`) → CI #887                                 |
| 2     | May 15, 2026    | `dynvec`, `dynstring`, `smallvec` instrumented. Shipped with bare-address ID derivation; retrofitted later (see §5). | CI #888 → CI #903                                             |
| 3a    | May 15–16, 2026 | `vec` and `deque` instrumented. **Value-return ID collision discovered.** Per-TU counter pattern introduced.         | CI #905 → CI #919; key fix at CI #913 (`51f941b`)             |
| 3b    | May 16, 2026    | `priority_queue` and `hashmap` instrumented. Restamp-on-every-mutation contract for reordering containers.           | CI #923 → CI #929                                             |
| 4     | May 15–16, 2026 | End-to-end borrow tests in `test/semantics/borrow_test.c`. Interleaved with Phase 3 — tests landed per container.    | CI #919, #924, #928                                           |
| 5     | May 17, 2026    | `bitset` and `stringbuf` view-tracking. Phase 2 retrofit (XOR-constant → per-TU counter) landed as prep.             | Retrofit at CI #933–#935 (`ab59935`, `7393eb9`, `f5a6dfa`); Phase 5 substrate at CI #936 (`55b6aaf`, bitset) and CI #937 (`302cf0f`, stringbuf); Phase 5 tests at CI #938 (`da6ed2e`) |

Note: The Phase chronology covers Phases 1–5 of the v1.3.0 substrate
shipment. Post-v1.3.0 extensions are recorded under their own OWN-NNN
entries — the Arena/Pool restamp migration at CI #944 lives in OWN-002,
not in this table.

### 1. Context

Ownership annotations in Canon-C (`owned()`, `borrowed()`, `moved()`,
`dropped()` from `core/ownership.h`) are documentation at the source
level. C99 has no flow-sensitive type system that could promote them to
compile-time enforcement project-wide, and the README explicitly rejects
adding one: *"if behavior cannot be understood by reading the header, it
does not belong."* `DEFINE_OWNED(T)` and `DEFINE_BORROWED(T)` give
compile-time enforcement at API boundaries on a per-type opt-in basis;
external static analyzers (Frama-C, Polyspace, LDRA) cover flow-sensitive
properties like use-after-move and double-drop.

That leaves a third class of bug the annotations and the wrappers cannot
catch on their own: a borrow that was valid when constructed but became
stale because the owning container reset, freed, reallocated, or
otherwise invalidated the backing storage. The README's *"Borrow
lifetime — know when a borrowed value is still valid"* example
demonstrates the canonical shape of this bug:

```c
const char* name = get_name(&scratch);   /* borrows from scratch arena */
/* scratch is reset somewhere in between */
printf("%s\n", name);                    /* undefined behavior */
```

In plain C this is undefined behavior, often manifesting as a crash
under load or silent data corruption in production. Static analyzers
can catch some patterns of this, but not all — and the annotation
layer, being doc-only, cannot catch any.

OWN-001 records the runtime substrate added to close that gap: a small
lifetime token embedded in every owning container, captured by every
borrow at construction, and validated on every read. The substrate
fires `require_msg` at the use site if the captured lifetime has been
invalidated. It is opt-in, runs under `CANON_LIFETIME_DEBUG`, and is
byte-identical to v1.2.x when the flag is off — the same ABI guarantee
the `CMakeLists.txt` `CANON_LIFETIME` block documents.

The substrate is the runtime layer of the same compositional bet the
README's *"From shared vocabulary to compositional verification"*
section describes. Where formal verification currently reaches — the
primitives in `core/primitives/`, the borrow types in `core/slice.h`,
and the memory primitives in `core/memory.h` — calling code inherits
proved runtime-safety obligations. The runtime substrate documented
here is the companion piece for the layer above: owning containers
whose macro-templated bodies sit outside WP's reach (§7), validated
at runtime instead while WP coverage walks up the dependency tree
toward them.

### 2. Decision

The substrate is one design across five phases. Every owning container
embeds a `lifetime_t` field; every borrow captures a `region_id_t`
snapshot of that field at construction; every `_get` validates the
snapshot before returning the inner value. The shape is:

```c
typedef struct { region_id_t id; bool open; } lifetime_t;
/* region_id_t = u64; REGION_ID_STATIC = 0 is reserved */
```

`lifetime_assert_valid` (in `core/region.h`) is the runtime check called
by borrow `_get` accessors. It fires `require_msg` on either of two
conditions: the source's `open` flag is `false`, or the captured `id`
no longer matches the source's current `id`. Containers vary in which
events they signal:

- **Close events** — the source flips `open` to `false`. Owners with a
  destructor (`vec`, `dynvec`, `dynstring`, `smallvec`, `bitset`,
  `stringbuf`) use this to invalidate every outstanding borrow at
  teardown.
- **Restamp events** — the source draws a new `id` while leaving `open`
  true. Owners that mutate or reallocate without semantic destruction
  use this; convenience containers restamp when `realloc` relocates
  the buffer, fixed-capacity owners with internal reordering restamp on
  every mutation that could shift element addresses.
- **No-op events** — purely additive or address-stable mutations do not
  restamp. The substrate's *honesty boundary* (§4) records which
  mutations are deliberately untracked.

Borrows that opt out of tracking — the classic `_from` constructors,
distinct from `_from_lifetime` — store `source_lt = NULL` and bypass
the check entirely. This preserves binary compatibility with code that
predates the substrate and gives callers an explicit escape hatch for
performance-critical paths. `borrow_test.c` exercises both:
`test_lifetime_untracked_borrow_bypasses_check` pins that `_from`
borrows remain silent across `arena_reset`, while
`test_lifetime_get_after_reset_fires` pins the symmetric positive case.

The default build (`CANON_LIFETIME=off`) leaves the `lifetime_t` field
out of every owning struct, leaves `source_lt` and `captured_id` out of
every borrow struct, and compiles every `_open_`, `_restamp_`, and
`_close_` helper to nothing. Struct layouts, function bodies, and
emitted machine code are byte-identical to v1.2.x — per `CMakeLists.txt`:
*"same constraint as NDEBUG. Mixing debug and release Canon-C objects in
one binary is undefined."* The two configurations are intentionally
incompatible; the substrate is a build-wide choice, not a per-call-site
one.

### 3. Contract table

Each row records the substrate behavior of one owning container type as
shipped in v1.3.0. The table is descriptive: it reports what each type
does, not whether the behavior is desirable. Evaluative content lives
in §4.

The Arena and Pool rows below reflect the post-OWN-002 state. The
shipped v1.3.0 mechanism for both was XOR-with-constant restamp; the
per-TU counter migration is recorded in OWN-002 and the rows are
updated here so the contract table remains the authoritative
description of the current substrate. The historical XOR-with-constant
mechanism and the cycle issue that motivated the migration are
preserved in §4 and §5.

| Type             | Phase | Field embedded?          | Open on                                       | Restamp on                                                                                                       | Close on                              | ID derivation                                                                                              |
| ---------------- | ----- | ------------------------ | --------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- | ------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| `Arena`          | 1     | Yes                      | `arena_init`                                  | `arena_reset`, `arena_reset_secure` — per-TU counter (see OWN-002)                                               | —                                     | Per-TU counter XOR'd with local address (migrated at CI #944; see OWN-002)                                 |
| `Pool`           | 1     | Yes                      | `pool_init` (on success only)                 | `pool_reset`, `pool_reset_secure` — per-TU counter (see OWN-002)                                                 | —                                     | Per-TU counter XOR'd with local address (migrated at CI #944; see OWN-002)                                 |
| `Region`         | 1     | No — separate `id`, `open` fields | `region_begin`                                | —                                                                                                                | `region_end`                          | Internal; layout-frozen by `region.h` regression tests                                                     |
| `vec_T`          | 3a    | Yes                      | `vec_T_init`, `vec_T_alloc`                   | Struct copy / `vec_T_swap` (swap exchanges the `lt` field along with everything else)                            | `vec_T_free` (heap-backed only)       | Per-TU counter XOR'd with local address                                                                    |
| `deque_T`        | 3a    | Yes                      | `deque_T_init`                                | Struct copy / `deque_T_swap`                                                                                     | —                                     | Per-TU counter XOR'd with local address                                                                    |
| `pq_T`           | 3b    | Yes (via inner PQ)       | `pq_T_init`                                   | Every successful `push` / `pop` / `remove_at` / `heapify`; error paths preserve `id`; internal helpers don't restamp | —                                 | Per-TU counter XOR'd with local address                                                                    |
| `hashmap`        | 3b    | Yes                      | `hashmap_init`                                | Every successful `insert` / `remove` / `clear`; error paths preserve `id` (Robin Hood backward-shift may move adjacent slots) | —                          | TU-wide counter shared across `hashmap` instantiations via the `CANON_HM_LIFETIME_NEXT_ID_DEFINED` guard   |
| `dynvec_T`       | 2     | Yes                      | `dynvec_T_init`, `dynvec_T_with_capacity`     | `realloc` actually relocating the buffer (`new_data != old_data` inside `grow` and `shrink_to_fit`)              | `dynvec_T_free`                       | Per-TU counter XOR'd with local address (retrofitted at CI #933; see §5)                                   |
| `dynstring`      | 2     | Yes                      | `dynstring_init`, `dynstring_with_capacity`, `dynstring_from` | `realloc` actually relocating the buffer (inside `ensure_capacity` and `shrink_to_fit`)         | `dynstring_free`                      | Per-TU counter XOR'd with local address (retrofitted at CI #935; see §5)                                   |
| `smallvec_T`     | 2     | Yes                      | `smallvec_T_init`, `smallvec_T_init_arena`    | Spill from inline to heap or arena — unconditional; happens at most once per instance                            | `smallvec_T_free` (close-then-reinit) | Per-TU counter XOR'd with local address (retrofitted at CI #934; see §5)                                   |
| `Bitset`         | 5     | Yes                      | `bitset_init`                                 | —                                                                                                                | `bitset_close`                        | Per-TU counter XOR'd with local address                                                                    |
| `StringBuf`      | 5     | Yes                      | `stringbuf_init_arena`, `stringbuf_init_buffer` | —                                                                                                              | `stringbuf_close`                     | Per-TU counter XOR'd with local address                                                                    |

**Notes on the table:**

- **`Region` does not participate via `lifetime_t`.** `core/region.h`
  keeps separate `id` and `open` fields whose layout is frozen by
  regression tests. The two are interoperable with `lifetime_assert_valid`
  at the call site only — the substrate does not redefine `Region`'s
  internal shape. This is the most common misreading of OWN-001 at
  review and is called out here to prevent it.
- **`Pool`'s lifetime is independent of its backing arena.** Per
  `pool.h`: *"if the caller resets the backing arena directly while the
  pool is still live, the pool's reserved region is gone but the pool's
  `lt.id` is unchanged. This is caller error and out of scope for
  pool's lifetime tracking."* Use a dedicated arena for the pool if
  this matters.
- **`StringBuf`'s arena variant does not track its arena's lifetime.**
  When initialized via `stringbuf_init_arena`, the StringBuf's `lt`
  catches `stringbuf_close`; the Arena's `lt` catches `arena_reset`.
  Capture both if both bug classes matter.
- **`arena_reset_to(mark)` does not touch the lifetime ID.** Borrows
  allocated before the mark must remain valid through rollback by the
  rollback contract. `borrow_test.c` pins this via
  `test_lifetime_get_after_reset_to_silent`.
- **Borrows that propagate lifetimes do so explicitly.**
  `BORROW_LT_INHERIT_` (in `semantics/borrow.h`) carries `source_lt`
  and `captured_id` from a source borrow into a derived borrow through
  `_slice` and `_as_bytes`. The case
  `test_lifetime_slice_int_as_bytes_inherits_and_fires` in
  `borrow_test.c` pins this: a tracked `borrowed_slice_int` derives a
  tracked `borrowed_bytes` view, and `arena_reset` invalidates both.

### 4. Honesty boundary

The substrate catches a specific, bounded class of bugs. Future
maintainers debugging a *"lifetime tracking should have caught this"*
report should start here. The canonical phrasing comes from `bitset.h`:

> *"lifetime tracking catches use-after-relocation and use-after-close,
> not use-of-mutated-value."*

#### Caught

- **Use-after-close.** Reading a borrow after the owner's destructor or
  `_close` (`vec_free`, `dynvec_free`, `bitset_close`, `stringbuf_close`,
  etc.) fires `require_msg`. Every owner with a destructor in the
  contract table participates. Phase 4/5 tests in `borrow_test.c`
  (e.g. `test_lifetime_vec_free_closes_fires`,
  `test_lifetime_bitset_close_fires`,
  `test_lifetime_stringbuf_close_fires_str`) pin this per container.
- **Use-after-restamp.** Reading a borrow after the owner has restamped
  (Phase 1 reset, Phase 2 reallocating realloc, Phase 3 mutation,
  smallvec spill) fires `require_msg`. Tests:
  `test_lifetime_get_after_reset_fires`,
  `test_lifetime_vec_swap_invalidates_a_fires`,
  `test_lifetime_pq_push_restamps_fires`,
  `test_lifetime_hashmap_insert_restamps_fires`.
- **Type-erased view inheritance.** A `borrowed_bytes` derived from a
  tracked typed slice carries the source's lifetime forward.
  Pinned by `test_lifetime_slice_int_as_bytes_inherits_and_fires`.

#### Not caught — by design

The substrate deliberately stops short of the following bug classes.
Each is documented in the relevant header docblock.

- **Use of mutated value.** A `bitset_set(&bs, 7)` does not invalidate
  a `borrowed_bytes` view over the words array — the address is
  unchanged; the bytes read are simply the new value. Same for
  `stringbuf_append`, `stringbuf_clear`, `stringbuf_truncate`,
  `dynvec_clear`, `dynstring_clear`. Pinned silent by
  `test_lifetime_bitset_mutation_silent`,
  `test_lifetime_stringbuf_mutation_silent`,
  `test_lifetime_vec_in_place_push_silent`,
  `test_lifetime_deque_in_place_push_silent`. Per `dynvec.h`: *"the
  substrate does not catch use-of-removed-element, because the memory
  addresses of removed elements are still valid bytes in the same
  buffer owned by the same dynvec. That bug class is out of scope for
  the substrate."*
- **Arena reset of a Pool's backing arena.** Resetting an Arena
  directly while a Pool drawing from it is still live invalidates the
  pool's reserved region but does not change the pool's `lt.id`. See
  `pool.h` note quoted in §3. The recommendation is a dedicated arena
  per pool if this matters.
- **Arena reset under a `stringbuf_init_arena`-backed StringBuf.** Same
  pattern: the StringBuf's `lt` is independent of the Arena's `lt`.
  Capture both if needed.
- **Two-reset cycles on `Arena` and `Pool`.** *Historical, closed by
  OWN-002.* The original Phase 1 restamp was XOR with the constant
  `0x9E3779B97F4A7C15ULL`. Two consecutive resets cycled the ID back
  to its original value: `A → A ^ K → A`. A borrow captured at the
  original ID would silently re-validate after the second reset. The
  Phase 3a discovery (see §5) introduced the per-TU counter that
  avoids this; Arena and Pool were not retroactively migrated when
  the convenience containers were. The asymmetry was documented here
  as a known limitation and forward-referenced for closure. OWN-002
  (shipped at CI #944, commits 2a7c0ba / a7c0413 / 83fa70b) migrated
  Arena and Pool to the per-TU counter pattern, closing this cycle.
  See OWN-002 for the migration details and regression tests; this
  bullet is preserved for chronological continuity and so future
  readers tracing the substrate's evolution see how the limitation
  was discovered, documented, and resolved.

### 5. Phase 2 retrofit chronology

The Phase 2 convenience containers shipped first (May 15) with a
bare-address ID derivation: `lt.id = (region_id_t)(uintptr_t)&v`. This
was a clean design at the time. The flaw became visible in Phase 3a on
the same day, when `vec_int_init` — a value-returning constructor —
revealed that the compiler reuses the same stack slot for consecutive
constructor calls, so two `vec_int` instances declared back-to-back end
up with identical `lt.id` values. A `vec_swap` test surfaced the
collision: after swapping `a` and `b`, a borrow originally against `a`
re-validated against `b` because both had the same ID.

The fix, introduced at CI #913 (`51f941b`, *"Improve lifetime tracking
with unique IDs"*), was a per-TU monotonic counter XOR'd with the local
address. The counter is a `static region_id_t` inside a `static inline`
helper function, so each translation unit gets its own copy; the XOR
with the local address preserves cross-TU diversity. Phase 3+
containers adopted this pattern from inception. The Phase 2 convenience
containers were retroactively migrated two days later, in Phase 5 prep,
at CI #933 (dynvec, `ab59935`), CI #934 (smallvec, `7393eb9`), and CI
#935 (dynstring, `f5a6dfa`).

`dynstring.h`'s docblock narrates the retrofit motivation directly:

> *"Previously the restamp XOR'd a constant into the id, which could
> cycle (x ^ K ^ K == x) across grow-then-shrink and spuriously
> re-validate a stale borrow. Drawing a fresh value from the counter
> eliminates that issue."*

The same narration appears verbatim in `dynvec.h` and is reflected in
`smallvec.h`'s comments. The header text is the canonical version of
the story; anything else is reinterpretation.

Arena and Pool predate Phase 2 and were not part of the Phase 5 prep
retrofit. `arena.h`'s original restamp comment recorded the intent of
the XOR constant: *"XOR with golden-ratio constant guarantees the new
id differs from the old one even when the arena address is reused."*
That guarantee held for a single reset; the cycle issue noted in §4
was the cost of stopping there. Arena and Pool were subsequently
migrated to the per-TU counter pattern at CI #944 — see OWN-002 for
the migration details. The XOR-with-constant restamp comments in
`arena.h` and `pool.h` were updated at the same time to reflect the
new mechanism.

### 6. Alternatives considered

**Extend `bytes_t` / `str_t` to carry the lifetime token directly.**
Rejected. `bytes_t` and `str_t` are the canonical `{ptr, len}` borrow
types and are designed to be ABI-stable across every Canon-C release.
Adding two fields (`source_lt`, `captured_id`) would have changed their
layout, broken binary compatibility, and forced every consumer of the
existing types — including the verified primitives in `core/slice.h`
whose ACSL contracts depend on the `{ptr, len}` shape — to either
re-verify or be rewritten. The chosen design wraps `bytes_t` / `str_t`
inside `borrowed_bytes` / `borrowed_str` as separate types that carry
the substrate fields. The unmodified canonical types remain available
for the verified path; the tracked variants are opt-in at the call
site.

This is the only alternative that was both seriously considered and
fully sourced against the codebase. Earlier drafts of OWN-001 named two
other options (a separate `tracked_slice.h` header; ACSL temporal
contracts via Frama-C). Those have been removed: the first would have
duplicated the entire slice surface for marginal benefit, and the
second is outside the scope of v1.3.0's runtime-checked substrate. If
formal temporal verification of the substrate becomes a priority in a
later release, it will get its own entry.

### 7. Verification posture: macro-templated vs non-macro substrate

The substrate splits cleanly into two surfaces with different
verification stories. OWN-001 records both so the boundary is not
later mistaken for an omission.

**Macro-templated bodies — runtime-only by construction.** Several
substrate surfaces are macro-templated: `BORROW_LT_FIELDS_`,
`BORROW_LT_INHERIT_`, `BORROW_LT_CHECK_`, and `DEFINE_BORROWED_SLICE`
in `semantics/borrow.h`; `DEFINE_DYNVEC`, `DEFINE_SMALLVEC`, and the
per-instantiation helper bodies in the convenience containers. The
C99 preprocessor strips ACSL annotations inside `#define` before
expansion, so macro-generated function bodies cannot be WP-verified
directly — they exist only after preprocessing, and only at the
instantiation sites. Extending WP coverage to these would require
generating per-instantiation ACSL specs at template expansion time,
which is outside the C99 preprocessor's reach and would need either a
code generator or a switch to a different annotation toolchain.
Neither is in scope for v1.3.0; if it ever becomes a priority, it
will get its own entry.

**Non-macro substrate — WP-verified (roadmap complete).**
`core/primitives/lifetime.h`, `core/region.h`'s `lifetime_assert_valid`,
the lifecycle functions in `core/arena.h` and `core/pool.h`, and the
non-macro surface of `semantics/borrow.h` are all conventional C99
inline functions amenable to WP, and all have now landed:
arena.h under VERIFY-009, pool.h under VERIFY-010, region.h (including
`lifetime_assert_valid`) under VERIFY-011, lifetime.h as N/A (three
typedefs and a constant — nothing to prove), and borrow.h's 24
non-macro functions under **VERIFY-016** (this is the cross-reference
this paragraph promised). borrow.h's entry makes the §-boundary
explicit from the verification side: the verified configuration is the
default build, where the `BORROW_LT_*` macros expand to nothing — WP
proves the exact shipped ABI bodies, and the lifetime-debug
instrumentation above remains runtime-only per this section. As
anticipated, the substrate's behavioral contracts did not change: the
contract table in §3 is the spec; the verification entries added
formal evidence that the implementation meets it. borrow.h's MC/DC
record landed alongside as MCDC-008 (its `borrowed_bytes_eq` one-NULL
guard proved dead under `cbytes_invariant` via a named in-body
assertion).

**Current evidence stream.** While the WP work walks up to the
substrate headers, the substrate is validated at runtime via
`test/semantics/borrow_test.c`. The test covers every phase of the
substrate end-to-end across all four borrow types (`borrowed_ptr`,
`borrowed_str`, `borrowed_bytes`, `borrowed_slice_int`), every Phase 3
container (vec, deque, pq, hashmap), every Phase 5 view container
(bitset, stringbuf), the type-erasure path (`as_bytes` inheritance),
and the opt-out path (untracked `_from` constructors). It runs in all
16 CI configs under both the standard build and
`CANON_LIFETIME=debug`, with ASan + UBSan enabled on Linux and macOS
debug builds. This is the substrate's current verification evidence;
it remains the evidence for macro-templated bodies indefinitely, and
sits alongside formal proof for the non-macro headers once they land.

### 8. Cross-references

- **README**, sections *"Borrow lifetime — know when a borrowed value
  is still valid"* and *"What about compile-time ownership
  enforcement?"* — user-facing introduction to the substrate and the
  complementary compile-time wrappers (`DEFINE_OWNED`,
  `DEFINE_BORROWED`). OWN-001 is the substrate-internal companion.
- **README**, *"1. `core/primitives/` — Foundations"* — lists
  *"Lifetime token types shared across owning modules"* as a core
  primitive.
- **`CMakeLists.txt`**, `CANON_LIFETIME` block — documents the build
  knob, the `off` / `debug` values, and the ABI guarantee.
- **`core/primitives/lifetime.h`** — canonical home of `lifetime_t`,
  `region_id_t`, `REGION_ID_STATIC`.
- **`core/region.h`** — `lifetime_assert_valid`; the CI regression
  story pinned by `test_lifetime_get_after_reset_fires`.
- **`semantics/borrow.h`** — `borrowed_ptr` / `borrowed_str` /
  `borrowed_bytes`; `BORROW_LT_*` macros; `_from_lifetime` constructors
  and the inheriting `_slice` / `_as_bytes` accessors.
- **`test/semantics/borrow_test.c`** — substrate evidence stream.
- **`docs/verification.md`** — canonical home for `VERIFY-NNN` entries.
  The non-macro substrate entries have landed: VERIFY-009 (arena.h),
  VERIFY-010 (pool.h), VERIFY-011 (region.h, incl.
  `lifetime_assert_valid`), and VERIFY-016 (borrow.h's non-macro
  surface, verified under `CANON_LIFETIME` off — see §7).
- **`docs/deviations.md`** — canonical home for documented unproved
  goals with their manual proof arguments. Substrate residuals live
  under the same IDs; borrow.h's are VERIFY-016 (2 memcmp-danglingness
  goals + 17 inherited) and MCDC-008 (the `borrowed_bytes_eq` one-NULL
  guard, proved dead via the named `dead_by_invariant` assertion).
- **`docs/traceability.md`** — MC/DC coverage record. borrow.h sits at
  its documented 38/40 = 95.0% ceiling (MCDC-008); the three
  `_get(NULL)` defensive outcomes are closed by
  `CANON_NO_REQUIRE`-gated tests in `borrow_test.c`.
- **CHANGELOG**, v1.3.0 — `{TODO: CHANGELOG v1.3.0 anchor}`.
- **OWN-002** — Arena/Pool restamp migration. Shipped at CI #944
  (commits 2a7c0ba / a7c0413 / 83fa70b). Closes the two-reset cycle
  documented in §4 by migrating Arena and Pool from XOR-with-constant
  restamp to the per-TU counter pattern used by every Phase 3+
  container.

<!--
  ============================================================================
  OWN-002 — Arena and Pool restamp migration to per-TU counter
  ----------------------------------------------------------------------------
  Paste this entry into docs/design-decisions.md under the OWN- section,
  immediately after OWN-001.

  Cross-references to confirm before pushing:
    - OWN-001 §3 (contract table) — Arena and Pool rows reflect the
      post-migration state; the historical XOR-with-constant mechanism
      is preserved in §4 and §5.
    - OWN-001 §4 (Honesty Boundary) — the "Two-reset cycles" bullet
      forward-references OWN-002 as the closure.
    - OWN-001 §5 (Phase 2 retrofit chronology) — the closing paragraph
      references OWN-002 as the Arena/Pool migration follow-up.
    - docs/decision-families.md — Category A description names OWN-002
      as the canonical example.
    - core/arena.h, core/pool.h — restamp helper comments updated to
      describe the per-TU counter mechanism rather than the original
      XOR-with-constant rationale.
    - test/core/arena_test.c, test/core/pool_test.c — regression tests
      pin the no-cycle property for Arena and Pool respectively.
  ============================================================================
-->

## OWN-002 — Arena and Pool restamp migration to per-TU counter

This entry records the post-v1.3.0 migration of `Arena` and `Pool` from
XOR-with-constant restamp to the per-TU counter pattern used by every
Phase 3+ container. The migration closes the two-reset cycle documented
in OWN-001 §4 and is the first Category A entry per the Decision
Families guide — closing a known limitation from an earlier OWN-NNN
entry's Honesty Boundary.

| Field          | Value                                                             |
| -------------- | ----------------------------------------------------------------- |
| Status         | Shipped post-v1.3.0                                               |
| Merged at      | CI #944 sequence: CI #942 (`2a7c0ba`, *"Refactor lifetime ID generation in arena.h"*), CI #943 (`a7c0413`, *"Refactor lifetime ID generation in pool functions"*), CI #944 (`83fa70b`, *"Implement OWN-002 regression tests for Arena and Pool"*). |
| Last green     | CI #944 across all 16 configs (8 default × `build` + 8 lifetime-debug × `lifetime-debug`, ubuntu/windows/macos × Release/Debug × default/lifetime-debug). All six Frama-C WP wrappers (checked.h, bits.h, compare.h, ptr.h, slice.h, memory.h) passed with their exact expected proved/unproved counts and named residuals — the migration did not perturb the verified substrate. |
| Scope          | `core/arena.h`, `core/pool.h` — restamp helpers and ID derivation |
| Build knob     | Inherits `CANON_LIFETIME=off/debug` from OWN-001; no new knob added |
| Category       | A — Closing a known limitation from an earlier OWN-NNN entry. See `docs/decision-families.md`, "Category A — Closing known limitations from earlier entries." |
| Cross-refs     | OWN-001 §3 (contract table, post-migration state), §4 (Honesty Boundary, closed bullet), §5 (Phase 2 retrofit chronology, closing paragraph); `docs/decision-families.md` Category A; `core/arena.h` and `core/pool.h` restamp helper comments; `test/core/arena_test.c` and `test/core/pool_test.c` regression tests. |

### 1. Context

OWN-001 §4 documented a deliberate limitation in the v1.3.0 substrate:
the Phase 1 restamp mechanism for `Arena` and `Pool` was XOR with the
constant `0x9E3779B97F4A7C15ULL`. Two consecutive resets cycled the ID
back to its original value (`A → A ^ K → A`), so a borrow captured at
the original ID would silently re-validate after the second reset. The
practical impact was bounded — three or more resets cycle further
around the ring, and `arena_reset_to(mark)` does not restamp at all —
but the asymmetry with the per-TU counter pattern used by every
Phase 3+ container was real and acknowledged.

The Phase 3a discovery (OWN-001 §5) introduced the per-TU counter
pattern in response to the value-return ID collision in `vec_int_init`.
The Phase 2 convenience containers were retroactively migrated at
CI #933–#935. Arena and Pool predated Phase 2 and were not part of the
Phase 5 prep retrofit; OWN-001 §4 forward-referenced the migration as
OWN-002, conditional on whether the consistency benefit justified the
churn.

This entry records the decision to ship the migration and the evidence
that it landed cleanly. The trigger was simply that the cycle was the
most legitimate "but what about—" question anyone reviewing the
substrate could raise. Closing it removes the asymmetry and means every
restamping container in Canon-C uses the same ID-generation pattern.

### 2. Decision

Both `arena.h` and `pool.h` migrated their restamp helpers from
XOR-with-constant to the same per-TU monotonic counter pattern that
every Phase 3+ container uses. The pattern is a `static region_id_t`
counter inside a `static inline` helper function, drawn fresh on every
restamp and XOR'd with the local address of the container being
restamped. The counter gives uniqueness across consecutive restamps of
the same container (no cycle); the XOR with the local address
preserves cross-container diversity within a translation unit and
cross-TU diversity overall.

The migration touched:

- `core/arena.h` — `arena_reset` and `arena_reset_secure` restamp
  helpers, and the `arena_init` ID derivation.
- `core/pool.h` — `pool_reset` and `pool_reset_secure` restamp
  helpers, and the `pool_init` ID derivation.
- `test/core/arena_test.c` — regression tests pinning the no-cycle
  property: two consecutive `arena_reset` calls produce three distinct
  IDs, and a borrow captured at the original ID fires on the first
  reset rather than re-validating on the second.
- `test/core/pool_test.c` — symmetric regression tests for `Pool`.

The shipped v1.3.0 ABI is unchanged. The `lifetime_t` field layout in
`Arena` and `Pool` is the same; only the function bodies that draw and
update `lt.id` changed. Default builds (`CANON_LIFETIME=off`) remain
byte-identical to v1.2.x by the same ABI guarantee documented in
OWN-001 §2 — the helper functions compile to nothing under the
default flag.

### 3. Honesty boundary

The migration does not introduce new not-caught cases. The other items
in OWN-001 §4's "Not caught — by design" list (use-of-mutated-value,
arena-reset of a Pool's backing arena, arena-reset under an
arena-backed StringBuf) are unchanged and remain deliberate boundaries
of what the runtime substrate is *for*, not limitations to be closed.

What the migration changes about the Honesty Boundary is bounded and
explicit: the "Two-reset cycles on Arena and Pool" bullet is now a
historical record of a closed limitation rather than an open one. The
bullet is preserved in OWN-001 §4 so future readers tracing the
substrate's evolution see how the limitation was discovered,
documented, and resolved.

The regression tests added in CI #944 are the trust-by-mechanism layer
for the closure. If anyone reverts the change or accidentally
reintroduces the XOR-with-constant pattern, the regression tests fail
the build. This is the same pattern used for the verified substrate
(named unproved-goal lists in the WP wrappers) and the runtime
substrate generally (the lifetime-debug job's ASan + UBSan coverage on
all 16 configs).

### 4. Alternatives considered

There was effectively one alternative considered and one path taken.

**Leave the asymmetry in place.** The cycle issue was bounded in
practice — three or more resets cycle further around the ring, and
`arena_reset_to(mark)` does not restamp at all — so the cost of
shipping the asymmetry was small. The cost of *not* closing it was
that the substrate had a documented limitation with a known fix, and
every reviewer reading OWN-001 §4 would reasonably ask why the fix
wasn't applied. The migration is small (two restamp helpers and two
test files), and the consistency benefit across the substrate
outweighs the churn cost. Closing the cycle was the path taken.

**Other ID derivations** (hash-of-counter, monotonic clock,
per-process counter) were not seriously considered. The per-TU
monotonic counter pattern is already proved out across every Phase 3+
container — it has the right properties (uniqueness across
consecutive restamps, cross-TU diversity via address XOR, no
synchronization required, no observable performance cost) and shipping
a different pattern for Arena and Pool would introduce a second
implementation to maintain.

### 5. Verification posture

OWN-002 is a runtime-substrate change, validated by the same evidence
stream as OWN-001: `test/semantics/borrow_test.c` plus the new
regression tests in `test/core/arena_test.c` and
`test/core/pool_test.c`, running across all 16 CI configs under both
the standard build and `CANON_LIFETIME=debug`, with ASan + UBSan
enabled on Linux and macOS debug builds.

The migration is mechanical (the per-TU counter helper is already
proved out by every Phase 3+ container) and the change surface is
small (four restamp helper bodies plus two test files). The CI #944
green-across-16-configs result is the verification evidence for the
shipped behavior.

The non-macro surface of `arena.h` and `pool.h` remains on the
WP-verification roadmap, as recorded in OWN-001 §7 and in the CI
file's candidate list. The migration does not change what will need
to be proved when WP coverage reaches these headers; the helper
function bodies are amenable to the same ACSL annotation approach as
the rest of the substrate. When those VERIFY-NNN entries land, this
entry will gain cross-references to them.

### 6. Cross-references

- **OWN-001 §3** — contract table rows for Arena and Pool, updated
  post-migration. The historical XOR-with-constant mechanism is
  preserved in §4 and §5.
- **OWN-001 §4** — "Two-reset cycles on Arena and Pool" bullet,
  preserved as a historical record of a closed limitation.
- **OWN-001 §5** — Phase 2 retrofit chronology, closing paragraph
  references OWN-002 as the Arena/Pool migration follow-up.
- **`docs/decision-families.md`** — Category A description ("Closing
  known limitations from earlier entries"). OWN-002 is the canonical
  Category A example.
- **`core/arena.h`** — restamp helper comments updated to describe the
  per-TU counter mechanism. The original XOR-with-constant rationale
  comment was replaced at CI #942.
- **`core/pool.h`** — symmetric update at CI #943.
- **`test/core/arena_test.c`** — regression tests for the no-cycle
  property on Arena.
- **`test/core/pool_test.c`** — regression tests for the no-cycle
  property on Pool.
- **`test/semantics/borrow_test.c`** — substrate evidence stream that
  continues to validate end-to-end borrow behavior across Arena and
  Pool resets under the new restamp mechanism.

  
<!--
  ============================================================================
  OWN-003 — region_end opaque-hook-dispatch verification boundary
  ----------------------------------------------------------------------------
  Cross-references to confirm before pushing:
    - VERIFY-011 (docs/deviations.md) — the proof-side companion; category 1
      records the 19 region_end residuals this decision produces.
    - core/region.h — the region_end verification note (the docblock above
      region_end and the inline comment at the hook call) names VERIFY-011 /
      OWN-003 as the home of this boundary.
    - docs/decision-families.md — Category D (the OWN/VERIFY seam); OWN-003
      is the first shipped Category D example.
  ============================================================================
-->

## OWN-003 — region_end opaque-hook-dispatch verification boundary

This entry records the architectural decision to dispatch region cleanup
hooks through an opaque, caller-supplied function pointer with no ACSL
`calls` clause, accepting the resulting WP residual as region.h's
documented verification boundary rather than redesigning the hook
mechanism to be WP-tractable. It is the first Category D entry per the
Decision Families guide — a decision at the seam between the OWN- and
VERIFY- namespaces, where the call is *what to prove / how to model the
proof* rather than the proof itself.

| Field          | Value                                                             |
| -------------- | ----------------------------------------------------------------- |
| Status         | Shipped (region.h verified report-only at CI #992)               |
| Merged at      | CI #992 (`c9172fc`, region.h 120s/split WP completion)           |
| Scope          | `core/region.h` — region_end hook dispatch                        |
| Category       | D — Boundary decision with the verification layer. See `docs/decision-families.md`, "Category D — Boundary decisions with the verification layer." |
| Cross-refs     | VERIFY-011 (proof-side companion, category 1); `core/region.h` region_end verification note; `docs/decision-families.md` Category D. |

### 1. Context

region_end calls registered cleanup hooks LIFO at region teardown. Each
hook is a caller-supplied `void (*fn)(void* ctx)` dispatched through an
indirect call `h->fn(h->ctx)`. This is the core of region.h's value —
arbitrary cleanup (unlock a mutex, close a file, free a resource) runs
unconditionally at region_end regardless of how the caller reached it.

When region.h was annotated for WP (VERIFY-011), the indirect hook call
became the one obligation WP could not discharge. WP has no `calls`
clause for an arbitrary function pointer, so it must assume the hook
could call region_end itself (modelling region_end as recursive) and
could write through a pointer aliasing the Region (so the call may not
respect region_end's `assigns *r` frame). Frama-C 29 additionally
reports `\valid_function(h->fn)` as not-yet-implemented. The result is
19 residual goals on region_end (VERIFY-011 category 1).

### 2. Decision

Keep the opaque-function-pointer hook design. Accept the 19 region_end
residuals as region.h's documented verification boundary. Structure the
function so that everything *except* the indirect call is provable:

- The attached arena is captured into a loop-immune local (`saved_arena`)
  *before* the hook loop, so arena_reset discharges against the local
  rather than a field the hooks might havoc.
- region_end's structural postconditions (`open == false`,
  `num_hooks == 0`, `arena == null`, `region_invariant`) are
  re-established by unconditional writes *after* the loop, so they
  discharge independently of what the hooks did.
- The loop's own writes (counter, cleanups array) carry a loop
  invariant / loop assigns / loop variant so the array-index RTE and
  loop termination discharge.

The hook contract — documented in the region_register docblock — is the
human-level discharge of the modelled recursion: *a hook must not call
region_end on this region or repoint r's fields*. Under that contract
the modelled recursion is vacuous and the `assigns *r` frame holds; WP
simply has no ACSL mechanism to encode the contract.

### 3. Alternatives considered

**Redesign hooks as a closed enum of cleanup kinds.** Rejected. Replacing
the function pointer with a tagged enum (`REGION_CLEANUP_ARENA_RESET`,
`REGION_CLEANUP_MUTEX_UNLOCK`, ...) plus a WP-visible dispatch `switch`
would let WP reason about every branch and close the 19 residuals. But it
would cripple the hook mechanism's entire purpose: arbitrary
caller-supplied cleanup. Every new cleanup kind any consumer needed would
require a new enum member and a new dispatch arm in region.h itself —
turning an open extension point into a closed, region.h-owned list. The
generality of the hook API is the feature; trading it for proof
convenience on a defensive teardown path is the wrong trade. The honest
boundary (19 documented residuals on one function, with a written manual
argument and a runtime-tested contract) costs less than the API
regression.

**Statement-level `assigns` contract on the hook call.** Rejected — it
does not work. WP reports "Statement specifications not yet supported
(skipped)" for a statement contract at the indirect call, so it provides
no discharge. This is noted inline in region.h at the hook call.

### 4. Verification posture

region.h is verified report-only at CI #992 (VERIFY-011); the 19
region_end residuals are the subject of this decision. The boundary is
backed by three evidence streams: the manual proof argument in
VERIFY-011 category 1, the documented hook contract in region.h, and
runtime testing (`test/core/region_test.c` exercises LIFO dispatch,
arena auto-reset after hooks, and the hook-table-full path) plus ASan +
UBSan across all 16 CI configs. When VERIFY-011's count stabilizes and
its CI step flips to enforced, this decision's residuals are pinned by
the named-goal enforcement.

### 5. Cross-references

- **VERIFY-011** — the proof-side companion. Category 1 enumerates the
  19 region_end residuals this decision produces and gives the manual
  proof argument.
- **`core/region.h`** — the region_end docblock and the inline comment
  at the hook call describe this boundary and name VERIFY-011 / OWN-003.
- **`docs/decision-families.md`** — Category D ("Boundary decisions with
  the verification layer"). OWN-003 is the first shipped Category D
  example.
