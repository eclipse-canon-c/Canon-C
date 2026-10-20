/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

/* vmacros/vdrivers/deque_verify.h                                [VERIFY-019]
 * ============================================================================
 * WP verification driver for the deque macro module.
 *
 * Shape: **B — CONFIRMED at CI #1234**, on the coverage stream's evidence,
 * not this one. deque_impl.h reported "No conditions" under both
 * deque_test.c and deque_cover.c, so the Shape-A-drift tripwire did not
 * fire. The paragraph below is retained because its reasoning still governs:
 * the WP run does not test attribution and must never be cited for the
 * shape claim.
 *
 * (Originally: **B (provisional)** — the status table's classification.)
 * Structurally the call is not in doubt: every deque function body exists
 * only inside an IMPL_DEQUE_* macro and comes into being at the
 * instantiation site, which is the Shape-B definition. But "confirmed" is a
 * term of art here — option, result and vec each earned it from the MC/DC
 * attribution check on their cover TU, and deque_cover.c is not started.
 * Do NOT upgrade this line on the strength of the WP run; the WP run does
 * not test attribution. The upgrade is deque_cover.c's to earn, together
 * with the Shape-A-drift tripwire vec established (deque_impl.h should show
 * functions-but-no-conditions).
 *
 * READ BY `frama-c -wp` ONLY. NEVER COMPILED OR LINKED.
 *   - Its `static inline` definitions would collide at link time with the
 *     same functions instantiated elsewhere. Keep it out of every build
 *     target and out of any globbed source root (see vmacros.md, build
 *     isolation).
 *
 * Integrity rule (vmacros.md): this driver instantiates the REAL shipped
 * macros. DEFINE_DEQUE_STRUCTS / DEFINE_DEQUE_FUNCTIONS are the two halves
 * of DEFINE_DEQUE, split upstream before this driver was written (the
 * split-patch-first checklist item, F3's deque analogue — the split landed
 * at CI #1225 and its expansion was verified byte-identical to the
 * pre-split monolithic macro across four linkage/type combinations). This
 * file is a second *caller* of the generated functions, never a
 * hand-written verified copy.
 *
 * Verified configuration: -DCANON_NO_REQUIRE -DNDEBUG, CANON_LIFETIME_DEBUG
 * undefined (per OWN-001 §7). Consequences, and they are sharper here than
 * for vec:
 *   - require_msg -> ((void)0). init's four guards, both
 *     push_*_unchecked triples, swap's pair, AND **peek_front / peek_back's
 *     d/out NULL guards** all VANISH. The `requires` clauses below carry
 *     those obligations.
 *   - deque has NO ensure_msg sites and NO direct CANON_INVOKE_HANDLER_
 *     calls (grep-confirmed: 0, same clean-audit shape as vec and result).
 *
 * Memory model: **Typed** (NOT vec's Typed+Cast). deque's own impl contains
 * zero casts, and its include closure is types/limits/contract/ownership +
 * error + option + result — no memory.h, ptr.h, slice.h, arena.h or
 * checked.h, i.e. none of the cast-originating headers. This is the first
 * data/-layer driver on plain Typed, and that fact is load-bearing for the
 * F4 experiment below — record the flag in the job banner.
 *
 * Linkage: contracted prototypes are `static inline` to MATCH
 * DEFINE_DEQUE_FUNCTIONS(static inline, int) below (C99 6.2.2p7).
 *
 * Representative instantiation: `int` (sizeof 4 — exercises the
 * capacity-bound division non-trivially, and matches vec's choice so the
 * two data/-layer runs are comparable).
 *
 * ── Module invariant (the load-bearing fact) ────────────────────────────────
 * deque is a RING, not a shifting buffer. Every index update is modular:
 * `(tail + 1) % capacity`, `(head + 1) % capacity`, and the two
 * `(x == 0) ? capacity - 1 : x - 1` decrements. Two obligations follow, and
 * both are discharged by `size <= capacity` rather than by any runtime
 * guard:
 *
 *   1. **Division by zero.** `% d->capacity` is UB at capacity == 0. No
 *      body tests capacity directly. push_*'s `size >= capacity` and pop_*'s
 *      `size == 0` early-returns make the modulus unreachable at
 *      capacity == 0 ONLY under `size <= capacity` — at capacity == 0 that
 *      forces size == 0, which trips both guards. If the invariant is
 *      mis-threaded, the division goals are what go orange.
 *   2. **Unsigned wrap.** `deque_int_remaining`'s `capacity - size` is
 *      computed unguarded on the non-NULL leg. It is deque's canary — the
 *      exact structural analogue of vec's fill(), and the first goal to
 *      inspect if the predicates are wrong.
 *
 * The ring also satisfies `tail == (head + size) % capacity`, preserved by
 * all six mutators and by clear(). It is NOT needed for safety; it is what
 * makes peek_back's `back_idx` and the pop_back postcondition provable.
 * Carried as a SEPARATE predicate (deque_int_ring) so that a failure to
 * prove functional correctness cannot be confused with a failure to prove
 * absence of RTE. Note the guard: at capacity == 0 the relation would
 * itself divide by zero, hence the `capacity > 0 ==>`.
 *
 * ════════════════════════════════════════════════════════════════════════════
 * PRE-REGISTERED ARM CLASSIFICATION
 *   (required by the instantiation-identity rule, docs/vmacros.md §
 *   "Deque note" — written BEFORE the first report-only run, same
 *   write-down-before-the-run discipline the cover TUs use for predicted-
 *   uncoverable conditions. VERIFY-018's Correction note exists because vec
 *   did this reconciliation AFTER its run; deque does it before.)
 * ════════════════════════════════════════════════════════════════════════════
 *
 *   ARM A — option_int: **INHERITANCE**.
 *     option_verify.h instantiates option at `int`, which is exactly the
 *     type parameter deque_int needs. Re-including it imports the verified
 *     instance. PREDICTION: **32 goals**, propagating byte-identically,
 *     `-wp-split` fragment indices included. Note 32, not 34: option's own
 *     job pins 34 = these 32 option_int_* goals PLUS the shared contract
 *     handler pair, and the pair is counted once here, under ARM C. vec's
 *     roll-call shows the same split (32 typed_cast_option_int_* entries
 *     alongside 2 handler entries).
 *     Sharper than vec's edge: vec ran Typed+Cast and so diffed its option
 *     arm only *modulo* the typed_ -> typed_cast_ prefix rename. deque runs
 *     Typed, the same model as option's home unit, so the names should
 *     match with NO prefix transform at all. A prefix difference here means
 *     the model flag is not what this banner claims.
 *
 *   ARM B — result__Bool_Error: **FRESH INSTANTIATION**.
 *     result_verify.h's home unit verifies the family at (int, VErr);
 *     (bool, Error) is a new verification subject, as it was for vec. Per
 *     the rule's explicit fork, this driver takes the FIRST option —
 *     attach the family's **full home contract set** — where vec took the
 *     second (reduced surface, recorded). All 17 home contracts are
 *     transcribed below, retyped int->bool / VErr->Error, shapes otherwise
 *     verbatim. Consequences, all falsifiable:
 *       B1. PREDICTION: **30 goals**, arithmetic rather than estimate.
 *           result's home unit residual is 30 = 2 handler + 28 own. vec's
 *           instance showed 22 = 20 profile-matching + 2 get_* MINUS the 8
 *           home `assigns` goals its lighter contracts never emitted. This
 *           driver attaches those clauses, so they should come back:
 *           20 + 8 + 2 = 30. Zero assigns goals would mean the
 *           int->bool / VErr->Error retyping dropped a clause.
 *       B2. The family profile should reproduce at clause-family
 *           granularity as it did for vec (20/20 there).
 *       B3. The 2 union get_* mem-access goals — the only two goals in the
 *           whole 30,127-goal dataset not pinned to a documented class
 *           (VERIFY-018 F4, attribution open) — should re-appear.
 *
 *   **F4 SEPARATING EXPERIMENT — CONFOUNDED AS DESIGNED. READ THIS BEFORE
 *   CITING RUN 1.** The intent was: vec ran this same fresh (bool, Error)
 *   instance under Typed+Cast, deque runs it under Typed, so diffing the
 *   get_ok/get_err pair isolates the memory model. That is NOT what the two
 *   runs differ by. ARM B's choice — attach the FULL home contract set —
 *   added `requires \valid(out)` to get_ok/get_err, which vec's reduced
 *   surface never had. TWO variables moved between the runs: the model AND
 *   the precondition. Run 1 observed the pair ABSENT, which is consistent
 *   with either cause, so it does not close F4.
 *
 *   The control is one local invocation: re-run THIS driver, contracts
 *   untouched, under `-wp-model Typed+Cast`.
 *     - pair REAPPEARS  => model-emission effect. F4 closes as the run-1
 *       banner claimed, and the claim becomes sound.
 *     - pair STAYS ABSENT => the added `requires \valid(out)` discharges
 *       the union read. F4 closes the other way, and vec's own pair becomes
 *       a contract-surface artefact its driver could have removed.
 *   Until that control runs, VERIFY-019 records F4 as OPEN with a candidate
 *   answer and a named confound. The CI job's F4 verdict lines print a
 *   conclusion the experiment has not yet earned; do not transcribe them.
 *
 *   (Original framing, retained for the record — valid only once the
 *   control above has isolated the model:)
 *       - pair PRESENT here  => not a Typed+Cast artefact; it is a
 *         type/contract-surface effect of the union extraction shape.
 *       - pair ABSENT here   => model-emission effect, attributable to
 *         Typed+Cast, and F4 closes as a memory-model note.
 *   Either outcome resolves F4. Record which, in the VERIFY-019 entry.
 *
 *   ARM C — contract.h handler: **INHERITED, predict exactly 2.**
 *     contract_default_handler's `ensures \false` / `exits \false` goals
 *     re-emit because the handler is DEFINED in the TU, not because deque
 *     reaches it (zero direct CANON_INVOKE_HANDLER_ calls). Same mechanism
 *     as vec's class (a), which confirmed at exactly 2.
 *
 *   ARM D — deque's own subject-side surface: **predict ZERO residuals.**
 *     This is the aggressive prediction and it is meant to be. deque lacks
 *     every structure that produced own residuals in vec:
 *       - no memory.h/arena.h in closure  => no allocation-model class (e)
 *       - no mem_copy/mem_move            => no element-transfer class (d);
 *                                            deque moves single elements by
 *                                            plain assignment, which WP
 *                                            relates exactly
 *       - **no loops of any kind**        => no macro-body-loop class (g).
 *                                            The roadmap pre-classified
 *                                            "deque's shift loops" into (g);
 *                                            that pre-classification is
 *                                            WRONG and is hereby withdrawn.
 *                                            A ring shifts nothing;
 *                                            deque_impl.h contains exactly
 *                                            one `while`, the do/while(0)
 *                                            of the contract idiom.
 *       - no fn-pointer parameters in the
 *         deque core                      => no class (f)/OWN-003, as vec
 *       - Typed, not Typed+Cast           => no bridging class (h)
 *     At-risk candidates, named in advance so a non-zero result is
 *     attributable rather than surprising: (i) the four *_option wrappers,
 *     which compose a result-returning or bool-returning callee with
 *     option_int_some/none across a driver boundary; (ii) the modular
 *     division goals, if the `size <= capacity` threading through the
 *     early-return guards proves harder for WP than the argument above
 *     suggests; (iii) swap's whole-struct copy, if field-wise ensures do
 *     not compose.
 *
 *     RUN-1 RESULT: **REFUTED, 9 observed against 0 predicted** — but the
 *     9 split cleanly along the pre-registered candidates, and NEITHER
 *     cluster is a property of deque:
 *       (i)   FIRED, 4 goals — and they were MY drafting error, not a
 *             module residual. pop_front/pop_back/peek_front/peek_back
 *             carried no default `assigns`, so WP framed their CALL SITES
 *             with the union of the behavior footprints ("No default
 *             assigns clause, using complete behaviors assigns", printed 4
 *             times — once per callee). The wrappers' none-branch could
 *             not then discharge `assigns \nothing`. Fixed for run 2 by
 *             adopting vec_int_pop / vec_int_pop_option's shape verbatim:
 *             default assigns at contract top, none-branch inheriting it.
 *             vec has no pop_option assigns residual under that shape.
 *       (ii)  DID NOT FIRE — zero division-by-zero, zero unsigned-wrap,
 *             `remaining`'s canary proved. The `size <= capacity` argument
 *             carried the whole ring, which is the load-bearing claim of
 *             this module and the one worth having tested.
 *       (iii) FIRED, 5 goals — an inherited class, not a new one. See the
 *             swap contract's note below: vec pins the same residual and
 *             deque widens its classification beyond the cast model.
 *             (Corrected 2026-10-20, VERIFY-040: not vec's class, as
 *             VERIFY-039's batch 2 found and the note on swap below says;
 *             ARM D is predicted 0 since VERIFY-040's round 1.)
 *     REVISED PREDICTION FOR RUN 2: **0 own residuals excluding the swap
 *     cluster**, i.e. ARM D = 5, all five swap ensures. If the four
 *     wrapper goals return, the vec-shaped fix is not the explanation and
 *     the framing problem is deeper than a missing default clause.
 *
 *   TOTAL PREDICTED UNPROVED: **64** = 2 (ARM C) + 32 (ARM A) + 30
 *     (ARM B) + 0 (ARM D) + 0 core substrate. The core-arm zero is its own
 *     claim: vec inherited 89 goals byte-identically from arena.h, and
 *     deque's closure contains none of the headers they come from. This is
 *     the first module whose inherited surface is SMALLER than its
 *     predecessor's — composability tested in the opposite direction from
 *     every prior confirmation, which have all been "propagates downward
 *     without amplification".
 *
 *   RUNTIME PREDICTION: well under vec's ~2h50m. vec's wall clock was
 *   timeout-dominated (193 x 120s) and the bulk of that was the 89-goal
 *   core arithmetic arm plus the 24 fill-loop goals, neither of which deque
 *   has. If deque's job approaches vec's, ARM D is wrong somewhere.
 *
 * ── Findings from the pre-run read (docs follow-ups, not blockers) ─────────
 *
 *   F1 (deque) — **NULL-guard asymmetry between peek_* and pop_*.**
 *     pop_front/pop_back test `!d || !out || !d->buffer` at RUNTIME and stay
 *     NULL-safe under any configuration. peek_front/peek_back instead use
 *     require_msg for d and out — so under -DCANON_NO_REQUIRE they become
 *     raw dereferences, while their same-named pop_ siblings do not. Both
 *     families are "safe" variants by name (the unchecked variants are
 *     separately spelled *_unchecked). The contracts below carry the
 *     obligation, so this is not a soundness problem for the proof, but the
 *     shipped doc comment does not flag the asymmetry. Recommend a doc
 *     patch on deque_decl.h/deque_impl.h; no code change proposed.
 *
 *   F2 (deque) — **a NULL deque is simultaneously empty and full.**
 *     `is_empty` returns `!d || size == 0` and `is_full` returns
 *     `!d || size >= capacity`; both answer true for NULL. Defensible as
 *     fail-closed on both sides (a NULL deque can neither be read nor
 *     written), and the contracts below state it explicitly rather than
 *     smoothing it over, but callers writing `if (!is_full(d)) push(...)`
 *     get the right behaviour while `if (!is_empty(d)) pop(...)` also gets
 *     the right behaviour only by accident of the pop_ NULL check. Worth an
 *     explicit sentence in the module doc.
 *
 *   F3 (deque) — **`int out = {0};` in the four *_option wrappers.**
 *     Brace-initialising a scalar is legal C99 (6.7.8p11 via p16) but is an
 *     unusual spelling and reads as a struct initialiser. Cosmetic;
 *     mentioned only so the pre-run read is complete.
 *
 * ── Run results ────────────────────────────────────────────────────────────
 *   RUN 1 (report-only)  : pending
 *   RUN 2 (report-only)  : pending
 *   RUN 3 (baseline)     : pending
 *   ENFORCED             : pending
 *   Per the region.h -> enforced pattern and VERIFY-018's three-run pinning
 *   discipline, nothing is asserted here until report-only lands. Fill the
 *   scorecard against the ARM A-D predictions above, naming each as
 *   CONFIRMED / REFUTED / ADJUSTED.
 * ============================================================================
 */

#ifndef CANON_VDRIVER_DEQUE_VERIFY_H
#define CANON_VDRIVER_DEQUE_VERIFY_H

/* ── ARM A: option composition (INHERITANCE) ────────────────────────────────
 * option_verify.h instantiates option_int through the real split macros WITH
 * contracts on every function. vec's run 1 established the lesson (R1):
 * composition, not weaker specs — a bare CANON_OPTION(int) leaves the
 * combinators spec-less and WP then assumes every option call assigns
 * everything, drowning the caller's goals. The four *_option wrappers below
 * are deque's only option callers and they need contracted some/none. */
#include "vmacros/vdrivers/option_verify.h"

/* ── ARM B: result__Bool_Error interposition (FRESH INSTANTIATION) ──────────
 * result_verify.h instantiates result_int_VErr, not the (bool, Error) pair
 * deque uses, so it cannot be re-included; the same interposition is done
 * here with the real split macros. Unlike vec, the FULL home contract set is
 * transcribed (all 17 functions) — the instantiation-identity rule's first
 * option — so this instance is comparable to result's home unit
 * clause-for-clause rather than being a reduced surface needing a written
 * exemption. Shapes are verbatim from result_verify.h, retyped int -> bool
 * and VErr -> Error.
 *
 * The guard define below makes deque_impl.h skip its own file-scope
 * emission (deque_impl.h:132, same guard convention as vec_impl.h, one
 * instantiation per TU). */
#include "semantics/error.h"
#include "semantics/result/result_defn.h"

DEFINE_RESULT_TYPEDEF(bool, Error)
DEFINE_RESULT_STRUCT(bool, Error)

/* ── Constructors ─────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result.is_ok == \true;
    ensures \result.val.ok == v;
*/
static inline result__Bool_Error result__Bool_Error_ok(bool v);

/*@ assigns \nothing;
    ensures \result.is_ok == \false;
    ensures \result.val.err == err;
*/
static inline result__Bool_Error result__Bool_Error_err(Error err);

/* ── Queries ──────────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result <==> r.is_ok;
*/
static inline bool result__Bool_Error_is_ok(result__Bool_Error r);

/*@ assigns \nothing;
    ensures \result <==> !r.is_ok;
*/
static inline bool result__Bool_Error_is_err(result__Bool_Error r);

/* ── Safe extraction ──────────────────────────────────────────────────────────
   B3 WATCH: these two are the F4 pair. Under Typed+Cast (vec) they emitted
   2 unattributed rte_mem_access goals on the union read. Same contracts,
   different memory model — the diff is the experiment. */

/*@ requires \valid(out);
    assigns *out;
    behavior ok:
      assumes r.is_ok;
      assigns *out;
      ensures \result == \true;
      ensures *out == r.val.ok;
    behavior err:
      assumes !r.is_ok;
      assigns \nothing;
      ensures \result == \false;
      ensures *out == \old(*out);
    complete behaviors;
    disjoint behaviors;
*/
static inline bool result__Bool_Error_get_ok(result__Bool_Error r, bool *out);

/*@ requires \valid(out);
    assigns *out;
    behavior err:
      assumes !r.is_ok;
      assigns *out;
      ensures \result == \true;
      ensures *out == r.val.err;
    behavior ok:
      assumes r.is_ok;
      assigns \nothing;
      ensures \result == \false;
      ensures *out == \old(*out);
    complete behaviors;
    disjoint behaviors;
*/
static inline bool result__Bool_Error_get_err(result__Bool_Error r, Error *out);

/*@ assigns \nothing;
    ensures \result == (r.is_ok ? r.val.ok : fallback);
*/
static inline bool result__Bool_Error_unwrap_or(result__Bool_Error r,
                                                bool fallback);

/* ── Unsafe extraction ────────────────────────────────────────────────────────
   Under -DCANON_NO_REQUIRE the require_msg guards are ((void)0); the
   preconditions are what make the bodies' union reads well-specified. */

/*@ requires r.is_ok;
    assigns \nothing;
    ensures \result == r.val.ok;
*/
static inline bool result__Bool_Error_unwrap(result__Bool_Error r);

/*@ requires !r.is_ok;
    assigns \nothing;
    ensures \result == r.val.err;
*/
static inline Error result__Bool_Error_unwrap_err(result__Bool_Error r);

/*@ requires r.is_ok;
    assigns \nothing;
    ensures \result == r.val.ok;
*/
static inline bool result__Bool_Error_expect(result__Bool_Error r,
                                             const char *msg);

/* ── Combinators (function-pointer dispatch) ──────────────────────────────────
   VERIFY-039 G11: result_verify.h's repaired contracts, transcribed. With C
   the condition on which the body calls the pointer: `requires C ==>
   \valid_function(f)`; `terminates !C; exits C;` and no `decreases` clause;
   the empty frame only in the behaviours that call nothing. WP 29 does not
   implement `\valid_function`, so the run-time check on each pointer stays
   unproved. deque calls NONE of these — they are emitted because
   DEFINE_RESULT_FUNCTIONS emits the whole family, and contracted because
   the rule's first option requires the full home set. Their goals are
   ARM B, not ARM D. */

/*@ requires r.is_ok ==> \valid_function(f);
    terminates !r.is_ok;
    exits r.is_ok;
    behavior err:
      assumes !r.is_ok;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == r.val.err;
    behavior ok:
      assumes r.is_ok;
      ensures \result.is_ok == \true;
      // \result.val.ok == f(r.val.ok): not stated, the callback's value
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error result__Bool_Error_map(result__Bool_Error r,
                                                        bool (*f)(bool));

/*@ requires !r.is_ok ==> \valid_function(f);
    terminates r.is_ok;
    exits !r.is_ok;
    behavior ok:
      assumes r.is_ok;
      assigns \nothing;
      ensures \result.is_ok == \true;
      ensures \result.val.ok == r.val.ok;
    behavior err:
      assumes !r.is_ok;
      ensures \result.is_ok == \false;
      // \result.val.err == f(r.val.err): not stated, the callback's value
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error result__Bool_Error_map_err(
    result__Bool_Error r, Error (*f)(Error));

/*@ requires r.is_ok ==> \valid_function(f);
    terminates !r.is_ok;
    exits r.is_ok;
    behavior err:
      assumes !r.is_ok;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == r.val.err;
    behavior ok:
      assumes r.is_ok;
      // \result == f(r.val.ok): not stated, the callback's value
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error result__Bool_Error_and_then(
    result__Bool_Error r, result__Bool_Error (*f)(bool));

/*@ requires !r.is_ok ==> \valid_function(f);
    terminates r.is_ok;
    exits !r.is_ok;
    behavior ok:
      assumes r.is_ok;
      assigns \nothing;
      ensures \result.is_ok == \true;
      ensures \result.val.ok == r.val.ok;
    behavior err:
      assumes !r.is_ok;
      // \result == f(r.val.err): not stated, the callback's value
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error result__Bool_Error_or_else(
    result__Bool_Error r, result__Bool_Error (*f)(Error));

/* ── Eager combinators (no fn pointers — expected fully provable) ──────────── */

/*@ assigns \nothing;
    behavior ok:
      assumes r.is_ok;
      ensures \result.is_ok == other.is_ok;
      ensures other.is_ok  ==> \result.val.ok  == other.val.ok;
      ensures !other.is_ok ==> \result.val.err == other.val.err;
    behavior err:
      assumes !r.is_ok;
      ensures \result.is_ok == \false;
      ensures \result.val.err == r.val.err;
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error result__Bool_Error_and(
    result__Bool_Error r, result__Bool_Error other);

/*@ assigns \nothing;
    behavior ok:
      assumes r.is_ok;
      ensures \result.is_ok == \true;
      ensures \result.val.ok == r.val.ok;
    behavior err:
      assumes !r.is_ok;
      ensures \result.is_ok == other.is_ok;
      ensures other.is_ok  ==> \result.val.ok  == other.val.ok;
      ensures !other.is_ok ==> \result.val.err == other.val.err;
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error result__Bool_Error_or(
    result__Bool_Error r, result__Bool_Error other);

/*@ requires (r1.is_ok && r2.is_ok) ==> \valid_function(eq_ok);
    requires (!r1.is_ok && !r2.is_ok) ==> \valid_function(eq_err);
    terminates r1.is_ok != r2.is_ok;
    exits r1.is_ok == r2.is_ok;
    behavior mismatch:
      assumes r1.is_ok != r2.is_ok;
      assigns \nothing;
      ensures \result == \false;
    behavior both_ok:
      assumes r1.is_ok && r2.is_ok;
      // \result == eq_ok(r1.val.ok, r2.val.ok): not stated
    behavior both_err:
      assumes !r1.is_ok && !r2.is_ok;
      // \result == eq_err(r1.val.err, r2.val.err): not stated
    complete behaviors;
    disjoint behaviors;
*/
static inline bool result__Bool_Error_eq(result__Bool_Error r1,
                                         result__Bool_Error r2,
                                         bool (*eq_ok)(bool, bool),
                                         bool (*eq_err)(Error, Error));

DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)

#define CANON_RESULT_BOOL_ERROR_DEFINED
#include "data/deque/deque_defn.h"

/* ── Types + struct from the real macro (first half of DEFINE_DEQUE) ─────────
 * The per-instantiation lifetime helper is emitted by DEFINE_DEQUE_STRUCTS
 * with no ACSL (annotations cannot live inside a macro body — comments are
 * stripped before macro definition). vec's run 1 showed the spec-less helper
 * makes WP assume every constructor call could assign anything, drowning
 * init/empty. Tag-only forward declaration + contracted prototype merges
 * `assigns \nothing` onto the macro-generated definition (empty body in the
 * verified config, CANON_LIFETIME off).
 *
 * deque has ONE helper, not vec's two — there is no close/destructor,
 * because the buffer is caller-owned and deque never frees. */
struct deque_int_s;

/*@ assigns \nothing; */
static inline void deque_int_lifetime_open_(struct deque_int_s* d);

DEFINE_DEQUE_STRUCTS(int)

/* ════════════════════════════════════════════════════════════════════════════
   VALIDITY PREDICATES (driver-local)

   deque_int_view / _mut carry exactly what is needed for ABSENCE OF RTE:
   the size bound, the capacity clamp, buffer validity, and — specific to a
   ring — the index bounds head < capacity and tail < capacity, without
   which the modular arithmetic and peek_back's back_idx are unbounded.

   deque_int_ring is SEPARATE and carries functional correctness only. Keep
   the split: it is what lets the run distinguish "the safety argument
   failed" from "the semantics argument failed".
   ════════════════════════════════════════════════════════════════════════════ */

/*@
  predicate deque_int_view(deque_int * d) =
    \valid_read(d)
    && d->size <= d->capacity
    && d->capacity <= CANON_DEQUE_MAX_CAPACITY / sizeof(int)
    && (d->buffer == \null ==> d->capacity == 0)
    && (d->capacity > 0 ==> \valid_read(d->buffer + (0 .. d->capacity - 1)))
    && (d->capacity > 0 ==> d->head < d->capacity && d->tail < d->capacity)
    && (d->capacity == 0 ==> d->head == 0 && d->tail == 0);

  predicate deque_int_mut(deque_int * d) =
    deque_int_view(d)
    && \valid(d)
    && (d->capacity > 0 ==> \valid(d->buffer + (0 .. d->capacity - 1)));

  predicate deque_int_ring(deque_int * d) =
    d->capacity > 0 ==> d->tail == (d->head + d->size) % d->capacity;
*/

/* ════════════════════════════════════════════════════════════════════════════
   CONTRACTED PROTOTYPES
   static inline to match DEFINE_DEQUE_FUNCTIONS(static inline, int) below.
   NULL-tolerant functions carry two-legged contracts because NULL tolerance
   is shipped, documented semantics — not an accident (see F2).
   ════════════════════════════════════════════════════════════════════════════ */

/* ── Constructors ─────────────────────────────────────────────────────────────
   init's four require_msg guards are compiled out; the requires below are
   load-bearing, not redundant. */

/*@ requires \valid(d);
    requires buffer != \null;
    requires capacity > 0;
    requires capacity <= CANON_DEQUE_MAX_CAPACITY / sizeof(int);
    requires \valid(buffer + (0 .. capacity - 1));
    assigns *d;
    ensures d->buffer == buffer;
    ensures d->capacity == capacity;
    ensures d->head == 0;
    ensures d->tail == 0;
    ensures d->size == 0;
    ensures deque_int_mut(d);
    ensures deque_int_ring(d);
*/
static inline void deque_int_init(deque_int* d, borrowed(int*) buffer,
                                  usize capacity);

/*@ assigns \nothing;
    ensures \result.buffer == \null;
    ensures \result.capacity == 0;
    ensures \result.head == 0;
    ensures \result.tail == 0;
    ensures \result.size == 0;
*/
static inline deque_int deque_int_empty(void);

/* ── Queries (all NULL-safe by shipped contract) ─────────────────────────────── */

/*@ requires d == \null || deque_int_view(d);
    assigns \nothing;
    ensures d == \null ==> \result == 0;
    ensures d != \null ==> \result == d->size;
*/
static inline usize deque_int_len(const deque_int* d);

/*@ requires d == \null || deque_int_view(d);
    assigns \nothing;
    ensures d == \null ==> \result == 0;
    ensures d != \null ==> \result == d->capacity;
*/
static inline usize deque_int_capacity(const deque_int* d);

/* THE CANARY. `capacity - size` is unsigned and unguarded; it is well-defined
 * only under the view predicate's size <= capacity. If deque's arithmetic
 * story is wrong anywhere, this goal is the first to go orange. */
/*@ requires d == \null || deque_int_view(d);
    assigns \nothing;
    ensures d == \null ==> \result == 0;
    ensures d != \null ==> \result == d->capacity - d->size;
    ensures \result <= CANON_DEQUE_MAX_CAPACITY / sizeof(int);
*/
static inline usize deque_int_remaining(const deque_int* d);

/*@ requires d == \null || deque_int_view(d);
    assigns \nothing;
    ensures \result <==> (d == \null || d->size == 0);
*/
static inline bool deque_int_is_empty(const deque_int* d);

/* F2: true for NULL, exactly as is_empty is. Stated, not smoothed over. */
/*@ requires d == \null || deque_int_view(d);
    assigns \nothing;
    ensures \result <==> (d == \null || d->size >= d->capacity);
*/
static inline bool deque_int_is_full(const deque_int* d);

/* ── Push: Result variants (NULL-safe) ───────────────────────────────────────
   The ok behavior's `% d->capacity` is well-defined because the full
   behavior's guard forces size < capacity, hence capacity > 0. */

/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));

    behavior unusable:
      assumes d == \null || d->buffer == \null;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_INVALID_ARG;

    behavior full:
      assumes d != \null && d->buffer != \null && d->size >= d->capacity;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_CAPACITY_EXCEEDED;

    behavior ok:
      assumes d != \null && d->buffer != \null && d->size < d->capacity;
      assigns d->head, d->size, d->buffer[0 .. d->capacity - 1];
      ensures \result.is_ok == \true;
      ensures d->head == (\old(d->head) == 0 ? d->capacity - 1
                                             : \old(d->head) - 1);
      ensures d->buffer[d->head] == item;
      ensures d->size == \old(d->size) + 1;
      ensures d->tail == \old(d->tail);
      ensures d->capacity == \old(d->capacity);
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error deque_int_push_front(deque_int* d, int item);

/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));

    behavior unusable:
      assumes d == \null || d->buffer == \null;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_INVALID_ARG;

    behavior full:
      assumes d != \null && d->buffer != \null && d->size >= d->capacity;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_CAPACITY_EXCEEDED;

    behavior ok:
      assumes d != \null && d->buffer != \null && d->size < d->capacity;
      assigns d->tail, d->size, d->buffer[0 .. d->capacity - 1];
      ensures \result.is_ok == \true;
      ensures d->buffer[\old(d->tail)] == item;
      ensures d->tail == (\old(d->tail) + 1) % d->capacity;
      ensures d->size == \old(d->size) + 1;
      ensures d->head == \old(d->head);
      ensures d->capacity == \old(d->capacity);
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error deque_int_push_back(deque_int* d, int item);

/* ── Push: try variants (NULL-safe, bool-returning) ──────────────────────────── */

/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));

    behavior rejected:
      assumes d == \null || d->buffer == \null || d->size >= d->capacity;
      assigns \nothing;
      ensures \result == \false;

    behavior ok:
      assumes d != \null && d->buffer != \null && d->size < d->capacity;
      assigns d->head, d->size, d->buffer[0 .. d->capacity - 1];
      ensures \result == \true;
      ensures d->head == (\old(d->head) == 0 ? d->capacity - 1
                                             : \old(d->head) - 1);
      ensures d->buffer[d->head] == item;
      ensures d->size == \old(d->size) + 1;
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline bool deque_int_try_push_front(deque_int* d, int item);

/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));

    behavior rejected:
      assumes d == \null || d->buffer == \null || d->size >= d->capacity;
      assigns \nothing;
      ensures \result == \false;

    behavior ok:
      assumes d != \null && d->buffer != \null && d->size < d->capacity;
      assigns d->tail, d->size, d->buffer[0 .. d->capacity - 1];
      ensures \result == \true;
      ensures d->buffer[\old(d->tail)] == item;
      ensures d->tail == (\old(d->tail) + 1) % d->capacity;
      ensures d->size == \old(d->size) + 1;
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline bool deque_int_try_push_back(deque_int* d, int item);

/* ── Push: unchecked variants ─────────────────────────────────────────────────
   All three require_msg guards vanish under the verified config. Safety comes
   ENTIRELY from these preconditions — the config CANON_NO_REQUIRE was designed
   for ("only when formal proof covers every call site"). */

/*@ requires deque_int_mut(d);
    requires deque_int_ring(d);
    requires d->buffer != \null;
    requires d->size < d->capacity;
    assigns d->head, d->size, d->buffer[0 .. d->capacity - 1];
    ensures d->head == (\old(d->head) == 0 ? d->capacity - 1
                                           : \old(d->head) - 1);
    ensures d->buffer[d->head] == item;
    ensures d->size == \old(d->size) + 1;
    ensures d->tail == \old(d->tail);
    ensures deque_int_mut(d);
    ensures deque_int_ring(d);
*/
static inline void deque_int_push_front_unchecked(deque_int* d, int item);

/*@ requires deque_int_mut(d);
    requires deque_int_ring(d);
    requires d->buffer != \null;
    requires d->size < d->capacity;
    assigns d->tail, d->size, d->buffer[0 .. d->capacity - 1];
    ensures d->buffer[\old(d->tail)] == item;
    ensures d->tail == (\old(d->tail) + 1) % d->capacity;
    ensures d->size == \old(d->size) + 1;
    ensures d->head == \old(d->head);
    ensures deque_int_mut(d);
    ensures deque_int_ring(d);
*/
static inline void deque_int_push_back_unchecked(deque_int* d, int item);

/* ── Pop: Result variants (NULL-safe, out-parameter) ─────────────────────────
   pop_front's `% d->capacity` is well-defined because the empty behavior's
   guard forces size > 0, which under size <= capacity forces capacity > 0. */

/* RUN-1 FIX (VERIFY-019): the default `assigns` below is load-bearing and
 * was missing in run 1. Without it WP reported "No default assigns clause,
 * using complete behaviors assigns" and framed CALL SITES with the union of
 * the behavior footprints, which the *_option wrappers' none-branch
 * `assigns \nothing` could not then discharge — 4 of the 9 ARM D residuals.
 * vec_int_pop carries exactly this shape (vec_verify.h) and vec has no
 * pop_option assigns residual; this is the known-good pattern, not a guess. */
/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));
    requires out == \null || \valid(out);
    requires d == \null || out == \null || \separated(d, out);
    assigns *out, d->head, d->size;

    behavior unusable:
      assumes d == \null || out == \null || d->buffer == \null;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_INVALID_ARG;

    behavior empty:
      assumes d != \null && out != \null && d->buffer != \null
              && d->size == 0;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_INVALID_STATE;

    behavior ok:
      assumes d != \null && out != \null && d->buffer != \null
              && d->size > 0;
      assigns *out, d->head, d->size;
      ensures \result.is_ok == \true;
      ensures *out == \old(d->buffer[d->head]);
      ensures d->head == (\old(d->head) + 1) % d->capacity;
      ensures d->size == \old(d->size) - 1;
      ensures d->tail == \old(d->tail);
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error deque_int_pop_front(deque_int* d, int* out);

/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));
    requires out == \null || \valid(out);
    requires d == \null || out == \null || \separated(d, out);
    assigns *out, d->tail, d->size;

    behavior unusable:
      assumes d == \null || out == \null || d->buffer == \null;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_INVALID_ARG;

    behavior empty:
      assumes d != \null && out != \null && d->buffer != \null
              && d->size == 0;
      assigns \nothing;
      ensures \result.is_ok == \false;
      ensures \result.val.err == ERR_INVALID_STATE;

    behavior ok:
      assumes d != \null && out != \null && d->buffer != \null
              && d->size > 0;
      assigns *out, d->tail, d->size;
      ensures \result.is_ok == \true;
      ensures d->tail == (\old(d->tail) == 0 ? d->capacity - 1
                                             : \old(d->tail) - 1);
      ensures *out == d->buffer[d->tail];
      ensures d->size == \old(d->size) - 1;
      ensures d->head == \old(d->head);
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error deque_int_pop_back(deque_int* d, int* out);

/* ── Pop: Option wrappers ────────────────────────────────────────────────────
   ARM D at-risk candidate (i): these compose a deque callee with option_int
   constructors across the driver boundary. The local `int out = {0};` (F3)
   is a scalar, so no separation obligation arises against d. */

/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));

    assigns d->head, d->size;

    behavior none:
      assumes d == \null || d->buffer == \null || d->size == 0;
      ensures \result.has_value == \false;

    behavior some:
      assumes d != \null && d->buffer != \null && d->size > 0;
      assigns d->head, d->size;
      ensures \result.has_value == \true;
      ensures \result.value == \old(d->buffer[d->head]);
      ensures d->size == \old(d->size) - 1;
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline option_int deque_int_pop_front_option(deque_int* d);

/*@ requires d == \null || (deque_int_mut(d) && deque_int_ring(d));

    assigns d->tail, d->size;

    behavior none:
      assumes d == \null || d->buffer == \null || d->size == 0;
      ensures \result.has_value == \false;

    behavior some:
      assumes d != \null && d->buffer != \null && d->size > 0;
      assigns d->tail, d->size;
      ensures \result.has_value == \true;
      ensures d->size == \old(d->size) - 1;
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline option_int deque_int_pop_back_option(deque_int* d);

/* ── Peek (NOT NULL-safe under this config — see F1) ─────────────────────────
   peek_front/peek_back guard d and out with require_msg, which compiles out,
   unlike pop_front/pop_back's runtime checks. The unconditional
   `requires d != \null` / `\valid(out)` below is therefore load-bearing and
   is the formal statement of F1. */

/*@ requires deque_int_view(d);
    requires deque_int_ring(d);
    requires \valid(out);
    requires \separated(d, out);
    assigns *out;

    behavior empty:
      assumes d->size == 0;
      assigns \nothing;
      ensures \result == \false;

    behavior nonempty:
      assumes d->size > 0;
      assigns *out;
      ensures \result == \true;
      ensures *out == d->buffer[d->head];

    complete behaviors;
    disjoint behaviors;
*/
static inline bool deque_int_peek_front(const deque_int* d, int* out);

/* back_idx = (tail == 0) ? capacity - 1 : tail - 1 is in [0, capacity - 1]
 * given the view predicate's tail < capacity; size > 0 forces capacity > 0. */
/*@ requires deque_int_view(d);
    requires deque_int_ring(d);
    requires \valid(out);
    requires \separated(d, out);
    assigns *out;

    behavior empty:
      assumes d->size == 0;
      assigns \nothing;
      ensures \result == \false;

    behavior nonempty:
      assumes d->size > 0;
      assigns *out;
      ensures \result == \true;
      ensures *out == d->buffer[d->tail == 0 ? d->capacity - 1
                                             : d->tail - 1];

    complete behaviors;
    disjoint behaviors;
*/
static inline bool deque_int_peek_back(const deque_int* d, int* out);

/* F1 propagates: these wrappers inherit peek's non-NULL-safety. */

/*@ requires deque_int_view(d);
    requires deque_int_ring(d);
    assigns \nothing;
    behavior none:
      assumes d->size == 0;
      ensures \result.has_value == \false;
    behavior some:
      assumes d->size > 0;
      ensures \result.has_value == \true;
      ensures \result.value == d->buffer[d->head];
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int deque_int_peek_front_option(const deque_int* d);

/*@ requires deque_int_view(d);
    requires deque_int_ring(d);
    assigns \nothing;
    behavior none:
      assumes d->size == 0;
      ensures \result.has_value == \false;
    behavior some:
      assumes d->size > 0;
      ensures \result.has_value == \true;
      ensures \result.value == d->buffer[d->tail == 0 ? d->capacity - 1
                                                      : d->tail - 1];
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int deque_int_peek_back_option(const deque_int* d);

/* ── Bulk / structural ──────────────────────────────────────────────────────── */

/*@ requires d == \null || deque_int_mut(d);

    behavior null:
      assumes d == \null;
      assigns \nothing;

    behavior live:
      assumes d != \null;
      assigns d->size, d->head, d->tail;
      ensures d->size == 0;
      ensures d->head == 0;
      ensures d->tail == 0;
      ensures d->buffer == \old(d->buffer);
      ensures d->capacity == \old(d->capacity);
      ensures deque_int_mut(d);
      ensures deque_int_ring(d);

    complete behaviors;
    disjoint behaviors;
*/
static inline void deque_int_clear(deque_int* d);

/* swap's two require_msg guards vanish. The contract admits a == b: each
 * of the two stores into the deque then writes the value it already holds,
 * which C defines for an exact overlap (C11 6.5.16.1p3), and deque_test.c's
 * test_swap_self makes that call. Two deques that overlap in part are
 * excluded, as for vec's swap.
 * (Until VERIFY-040 this contract required \separated(a, b), and this
 * comment said the body is an exchange only when a and b do not alias.)
 *
 * RUN-1 RESULT (VERIFY-019): ensures 6-10 unproved, ensures 1-5 proved, and
 * the ten field-wise clauses kept, for two reasons VERIFY-039's batch 2
 * found wrong. They were not vec's class: vec's goal failed on *a, whose
 * operands vec's contract let overlap in part. And the obstacle was not
 * tmp's preservation, which no store reaches, tmp being a value in WP's
 * model. The five clauses were true. WP generates the fact that proves
 * them, *b after the last store equal to *a before the first, as tmp's
 * initializer, and its hypothesis filter drops that fact from a goal that
 * names one field, and so one of the two memories the store writes.
 *
 * VERIFY-040: each side of the exchange is one conjunction, as vec states
 * it, so that each goal names both memories and the filter keeps the fact;
 * with a == b admitted, the side about *a rests on that fact too. */
/*@ requires \valid(a) && \valid(b);
    requires a == b || \separated(a, b);
    requires deque_int_mut(a) && deque_int_mut(b);
    assigns *a, *b;
    ensures a->buffer == \old(b->buffer) && a->capacity == \old(b->capacity)
         && a->head == \old(b->head) && a->tail == \old(b->tail)
         && a->size == \old(b->size);
    ensures b->buffer == \old(a->buffer) && b->capacity == \old(a->capacity)
         && b->head == \old(a->head) && b->tail == \old(a->tail)
         && b->size == \old(a->size);
*/
static inline void deque_int_swap(deque_int* a, deque_int* b);

/* ════════════════════════════════════════════════════════════════════════════
   REAL MACRO-GENERATED BODIES
   WP merges each contract above onto the matching definition emitted here.
   DEFINE_DEQUE_FUNCTIONS (not DEFINE_DEQUE): the struct half was emitted
   above, ahead of the lifetime-helper prototype and the predicates.
   ════════════════════════════════════════════════════════════════════════════ */

DEFINE_DEQUE_FUNCTIONS(static inline, int)

#endif /* CANON_VDRIVER_DEQUE_VERIFY_H */
