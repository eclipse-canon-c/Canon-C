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

/* vmacros/vdrivers/vec_verify.h                                  [VERIFY-018]
 * ============================================================================
 * WP verification driver for the vec macro module (Shape B, confirmed).
 *
 * READ BY `frama-c -wp` ONLY. NEVER COMPILED OR LINKED.
 *   - Its `static inline` definitions would collide at link time with the
 *     same functions instantiated elsewhere. Keep it out of every build
 *     target and out of any globbed source root (see vmacros.md, build
 *     isolation).
 *
 * Integrity rule (vmacros.md): this driver instantiates the REAL shipped
 * macros. DEFINE_VEC_STRUCTS / DEFINE_VEC_FUNCTIONS are the two halves of
 * DEFINE_VEC (split added for exactly this purpose — same pattern as
 * option's DEFINE_OPTION_STRUCT / DEFINE_OPTION_FUNCTIONS); the expansion
 * is byte-identical to what users compile. This file is a second *caller*
 * of the generated functions, never a hand-written verified copy.
 *
 * Verified configuration: -DCANON_NO_REQUIRE -DNDEBUG, CANON_LIFETIME_DEBUG
 * undefined (per OWN-001 §7: the lt field and helpers expand to nothing;
 * WP proves the shipped ABI bodies). Consequences:
 *   - require_msg  -> ((void)0): init's compound guard and swap's two NULL
 *     guards VANISH. The `requires` clauses below carry those obligations —
 *     under this config the preconditions are load-bearing, not redundant.
 *   - ensure_msg   -> ((void)0): get_unchecked / at / push_unchecked /
 *     slice_get become raw unguarded accesses. Their safety comes ENTIRELY
 *     from the contracts here. This is the config CANON_NO_REQUIRE was
 *     designed for ("only when formal proof covers every call site").
 *
 * Memory model: `Typed+Cast` — corroborated by memory.h itself: its
 * contracts bake (char*) casts into the ACSL (regions_overlap over char*,
 * byte-range assigns ((char*)dest)[0 .. size-1]), and vec adds
 * (type*)mem_alloc plus int*->void* copy calls on top. Final confirmation
 * is the flag on the memory.h/arena.h CI jobs; record the choice in this
 * job's banner.
 *
 * Linkage: contracted prototypes are `static inline` to MATCH
 * DEFINE_VEC_FUNCTIONS(static inline, int) below (C99 6.2.2p7).
 *
 * Representative instantiation: `int` (sizeof 4 — exercises the
 * capacity-bound division and the byte-count multiplications non-trivially,
 * unlike a sizeof-1 type).
 *
 * ── Module invariant (the load-bearing fact) ────────────────────────────────
 * Every unchecked multiplication downstream — insert/remove's mem_move byte
 * counts, append_array's copy size — is safe ONLY because construction
 * clamps capacity <= CANON_VEC_MAX_CAPACITY / sizeof(type). vec_int_view /
 * vec_int_mut below carry that clamp plus len <= capacity plus buffer
 * validity. fill() is the canary: its `capacity - len` is computed
 * unguarded, so if the invariant is mis-threaded, fill's subtraction goal
 * is the first to go orange.
 *
 * ── Expected residual classes (to be CONFIRMED by the report-only CI run,
 *    not asserted here — report-only lands first, per the region.h ->
 *    enforced pattern) ─────────────────────────────────────────────────────
 *
 *   (a) Contract-handler pair (predict 2, INHERITED). contract.h is in the
 *       include graph, so contract_default_handler's `ensures \false` /
 *       `exits \false` goals re-emit — same as VERIFY-006 cat 4 and the
 *       option driver's class (a). NOTE the mechanism differs from option:
 *       under CANON_NO_REQUIRE + NDEBUG *no vec function ever reaches the
 *       handler* (vec has zero direct _CANON_INVOKE_HANDLER calls — the
 *       clean-audit shape, like result). The goals appear only because the
 *       handler is DEFINED in the TU.
 *
 *   (b) memory.h / arena.h inherited surface (counts per their in-place
 *       runs). mem_alloc/mem_free/mem_copy/mem_move/arena_alloc goals
 *       re-emit here. Largest inherited surface of any driver to date.
 *
 *   (c) checked.h inherited — fully ACSL'd (complete+disjoint behaviors,
 *       `*result == a + b` on no_overflow); predict these RE-PROVE, not
 *       residual, and that alloc/arena_alloc's `!checked_mul` branches are
 *       proved DEAD (the division pre-check makes overflow impossible —
 *       see the cover TU for the MC/DC side of this fact).
 *
 *   (d) insert/remove/append_array/extend element-transfer ensures —
 *       CONFIRMED-EXPECTED residuals. memory.h's mem_copy/mem_move carry
 *       frame-only contracts (assigns ((char*)dest)[0 .. size-1], no
 *       functional ensures relating dest to src), so the \forall shift /
 *       copy postconditions on the four ok-behaviors below are unprovable
 *       by construction. They are kept deliberately: they document the
 *       intended semantics, name the residual precisely, and become
 *       provable the day memory.h gains functional ensures. Predicted
 *       count: 4 goals (one per ok-behavior). The prefix-unchanged
 *       \forall clauses may also land here pending the Typed+Cast
 *       byte-range/element-range framing question.
 *
 *   (e) ANSWERED pre-run: mem_free's own contract carries NO frees
 *       clause (requires \freeable; assigns \nothing; ensures \true), so
 *       this driver follows precedent — free() keeps the \freeable
 *       requires (needed to discharge mem_free's) and states no frees.
 *       Expect only \freeable plumbing, no allocation-model residuals.
 *
 *   (f) NO function-pointer residual. vec core has zero fn-pointer
 *       parameters — the first macro module without option's class (b).
 *       If any rte_function_pointer goal appears, something is wrong.
 *
 * ── RUN 1 RESULTS (report-only, 4988/5350 proved, 350 T / 12 U / 0 F) ──────
 * Prediction scorecard:
 *   (a) CONFIRMED exactly: 2 goals (contract_default_handler loop_invariant
 *       _established + terminates), the only handler goals in the run.
 *   (f) CONFIRMED in substance: all 13 rte_function_pointer goals sit on
 *       result__Bool_Error_* / option_int_* combinators (inherited);
 *       ZERO on any vec_int_* function.
 *   ptr.h prediction FALSIFIED: 11 ptr.h goals present. arena.h calls
 *       ptr_span (and pulls slice.h -> str.h), so ptr.h enters through
 *       arena, not the facade — the include-graph read missed arena's own
 *       dependencies. Classification unchanged: inherited, proven in place.
 *   (c) checked/align/arena re-emissions TIMED OUT despite proving in
 *       their own runs — mega-TU context bloat; expect flip-back once the
 *       spec-less-callee noise (below) is fixed.
 *   [wp:union]: 18 warnings, expected, non-failures (VERIFY-015 precedent).
 *
 * Root causes fixed in driver v2 (this file):
 *   R1. Spec-less callees assign-everything: the lifetime helpers and the
 *       bare option/result instantiations had no contracts, so WP treated
 *       every call as clobbering the world — every constructor, every
 *       result-returning function drowned. Fixed by driver composition
 *       (option_verify.h include; contracted result__Bool_Error _ok/_err/
 *       _is_ok via the split macros + guard define) and tag-forward-decl
 *       contracts on the lifetime helpers.
 *   R2. free lacked a global assigns (kernel unioned behaviors — benign
 *       but now explicit).
 *
 * NEW residual class (g) — macro-body loops (first instance in campaign):
 *   fill()'s for-loop lives in IMPL_VEC_FILL's macro body; ACSL loop
 *   annotations (loop invariant / assigns / variant) can only be attached
 *   adjacent to the loop statement and cannot survive macro definition
 *   (comments are stripped). fill's cluster — terminates ×2, rte_mem_access
 *   ×8 (4 pairs), assigns ×4(+4 live), live ensures ×6 — is therefore
 *   unprovable BY CONSTRUCTION under WP, independent of contract quality.
 *   Permanent named residual; runtime confidence comes from MCDC-010
 *   (fill's three legs measured) and the fuzz invariants. FORWARD-FLAG:
 *   deque's shift loops will hit the same class.
 *
 * Class (e) concretized: the allocation-model residuals are
 *   vec_int_alloc_call_vec_int_init_requires_2 (\fresh cannot establish
 *   buffer validity — WP: "not yet implemented") and
 *   vec_int_free_call_mem_free_requires (\freeable likewise) + free's
 *   live ensures/assigns parts — the exact analogs of memory.h's own
 *   mem_free residual pair, visible in this run's inherited list.
 *
 * WATCH-ITEM for run 2: insert/remove/append's call_mem_move/mem_copy
 *   _requires goals (int-element \separated bridging to char-level
 *   regions_overlap). May close once R1's noise is gone; if not, restate
 *   the driver requires in memory.h's own predicate shape.
 *
 * ── RUN 2 RESULTS (report-only, 5208/5413 proved, 195 T / 10 U / 0 F) ──────
 * R1 VALIDATED: 362 -> 205 unproved, 5h19m -> 2h51m. Fully proved after the
 * composition fix: empty, init, arena_alloc, all queries, push, try_push,
 * push_unchecked, get*, set, first/last/data, clear, iter_*, slice_init,
 * slice_get. checked/align re-emissions still time out (see model-variance
 * note below).
 *
 * VEC-OWN CLASSIFICATION (62 goals):
 *   (e) allocation-model, CONFIRMED at exactly the 2 predicted goals:
 *       vec_int_alloc_call_vec_int_init_requires_2   (\fresh unimplemented)
 *       vec_int_free_call_mem_free_requires          (\freeable likewise)
 *   (g) fill macro-body-loop cluster, CONFIRMED: 24 goals (terminates x2,
 *       rte_mem_access x8, assigns x4, live_ensures x6, live_assigns x4).
 *   (d) element-transfer ensures: insert_ok_ensures_5 (shift forall),
 *       remove_ok_ensures_4/_5 (prefix + shift foralls), append_ok_
 *       ensures_4 (copy forall) — per the frame-only mem_copy/mem_move
 *       contracts, plus the char-frame bridging below.
 *   (h) NEW CLASS — Typed+Cast int<->char bridging at memory.h call sites:
 *       the watch-item did NOT close. call_mem_move/mem_copy_requires (7)
 *       and the insert/remove/append assigns parts that must relate
 *       ((char*)dest)[0..bytes-1] frames to int-element windows. Candidate
 *       shrink levers before accepting as permanent: -wp-timeout bump on
 *       retry, -wp-cache, or restating driver requires in memory.h's own
 *       regions_overlap/mem_valid_* predicate shapes.
 *   RETRY-VARIANCE candidates (may flip on any rerun, do not pin yet):
 *       vec_int_pop_ok_ensures_4_part5 (prefix forall with NO mem call —
 *       should follow from the assigns frame), vec_int_swap_ensures
 *       (whole-struct copies under the cast model).
 *
 * DRIVER v3 (this file): delegate-narrowing lesson. WP uses a callee's
 * contract WHOLE at call sites — a delegating caller's per-behavior
 * `assigns \nothing` cannot be discharged when the callee's GLOBAL assigns
 * is wider, even though the code path assigns nothing. Affected: the three
 * pure delegates only (extend -> append_array, pop_option -> pop,
 * remove_option -> remove); insert/remove's direct-code error legs proved
 * their narrow assigns fine. v3 relaxes exactly those six behavior-assigns
 * (~10 goals). Contract meaning: delegate effects are bounded by the
 * callee's contract, which is the honest statement anyway.
 *
 * INHERITED-DELTA nuance for the docs: option/result/checked goals here are
 * generated under Typed+Cast, while their home runs used plain Typed —
 * goal sets and prover difficulty differ BY MODEL, so this run's inherited
 * unproved list is not expected to equal the home runs' residual lists
 * name-for-name. Classification is by module ownership, with the model
 * variance recorded once.
 *
 * ── RUN 3 = ENFORCED BASELINE (5184/5380, 193 T / 3 U / 0 F = 196) ─────────
 * v3 validated: all 9 delegate-narrowing goals gone; inherited 143
 * IDENTICAL name-for-name to run 2 (the stability subset-enforcement
 * relies on). Retry candidates failed a third consecutive run — PINNED
 * into class (h): pop_ok_ensures_4_part5, swap_ensures.
 * Final vec-own classification (53): (e)=2, (g)=24, (d)=5, (h)=22.
 * Enforcement (option/result house style): the 196 names live inline in
 * the frama-c-vec job's CHECKS array. Gates: 0 Failed verdicts, exact
 * unproved count, by-name roll-call. ANY change to the residual set —
 * including a timeout flipping to Proved on a fast prover day — fails the
 * job; the fix is the acknowledged ratchet (update CHECKS +
 * EXPECTED_UNPROVED). Full classification: docs/verification.md
 * VERIFY-018.
 *
 * ── Findings already on record from the pre-run read (docs follow-ups) ─────
 *   F1. append_array/extend: shipped doc does not forbid src overlapping
 *       the vec's own buffer; mem_copy (memcpy semantics) makes self-append
 *       UB. The \separated requires below is therefore a CONTRACT ADDITION
 *       — back-propagate to the IMPL_VEC_APPEND_ARRAY doc comment.
 *   F2. slice_init on an items==NULL vec (e.g. from empty()) with
 *       start == end == 0 passes the guard and computes &v->items[0] ==
 *       NULL + 0 — pedantic UB (C99 6.5.6), harmless at runtime, but WP's
 *       RTE will flag the pointer arithmetic. Upstream fix: add `!v->items`
 *       to the early-return guard (semantics preserved: empty slice).
 *       Until patched, the `ok` behavior below assumes v->items != \null;
 *       the report-only run should show exactly this goal orange if the
 *       assumption is removed.
 *   F3. vec_defn.h's old claim that double instantiation per TU is safe
 *       was wrong (no guard is possible inside a macro; C99 rejects
 *       repeated typedefs) — fixed in the same patch that adds the split.
 *
 * FIELD NAMES (resolved against result_verify.h / option_verify.h):
 * result__Bool_Error is { bool is_ok; union { bool ok; Error err; } val; }
 * — contracts use `.is_ok` / `.val.err`. option_int uses `.has_value` /
 * `.value`. NOTE: the `.val.err` ensures are union-member postconditions
 * and therefore sit downstream of VERIFY-015's open class (c) (union-model
 * residuals, first flagged by the result driver). If class (c) materializes
 * there, vec's error-code ensures join it as INHERITED — do not classify
 * them as a new vec-own class.
 * ============================================================================ */

#ifndef CANON_VDRIVER_VEC_VERIFY_H
#define CANON_VDRIVER_VEC_VERIFY_H

/* Real, unmodified module headers. vec_defn.h pulls vec_mangle.h and
 * vec_impl.h (which pulls types/limits/contract/checked/memory/arena/
 * ownership + result + error, and emits the guarded CANON_RESULT(bool,
 * Error) instantiation — those result__Bool_Error goals are INHERITED,
 * attributed to the result module in the split). option_##type must exist
 * before the vec functions, so option is instantiated here too — its goals
 * are likewise inherited (proved under the option driver's own run). */
/* ── Driver composition: option ────────────────────────────────────────────
 * option_verify.h instantiates option_int through the real split macros
 * WITH contracts on every function. Run 1 proved that bare CANON_OPTION(int)
 * poisons every vec caller of option_int_some/none: spec-less callees make
 * WP assume they assign everything. Composing the drivers gives vec
 * contracted callees and collapses the option-combinator noise to option's
 * own documented residual set (VERIFY-014). */
#include "vmacros/vdrivers/option_verify.h"

/* ── Driver composition: result__Bool_Error ────────────────────────────────
 * result_verify.h instantiates result_int_VErr, not the (bool, Error) pair
 * vec uses, so it cannot be included; instead the same interposition is
 * done here with the real split macros. Contracts cover the three functions
 * vec bodies call (_ok / _err / _is_ok) — shapes copied verbatim from
 * result_verify.h, retyped. The guard define below makes vec_impl.h skip
 * its own file-scope emission (same guard convention, one instantiation).
 * The remaining result functions (combinators) are emitted uncontracted;
 * their fn-pointer/termination cluster re-emits with the same goal names
 * as run 1 and stays classified INHERITED (result class (b) analog). */
#include "semantics/error.h"
#include "semantics/result/result_defn.h"

DEFINE_RESULT_TYPEDEF(bool, Error)
DEFINE_RESULT_STRUCT(bool, Error)

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

/*@ assigns \nothing;
    ensures \result <==> r.is_ok;
*/
static inline bool result__Bool_Error_is_ok(result__Bool_Error r);

DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)

#define CANON_RESULT_BOOL_ERROR_DEFINED
#include "data/vec/vec_defn.h"

/* ── Types + structs from the real macro (first half of DEFINE_VEC) ──────────
 * The per-instantiation lifetime helpers are emitted by DEFINE_VEC_STRUCTS
 * with no ACSL (annotations cannot live inside a macro body — comments are
 * stripped before macro definition). Run 1: the spec-less helpers made WP
 * assume every constructor/destructor call could assign anything, drowning
 * init/empty/alloc/free goals (vec_int_empty_ensures went Unknown on a
 * function that sets three fields). Tag-only forward declaration +
 * contracted prototypes merge `assigns \nothing` onto the macro-generated
 * definitions (empty bodies in the verified config, CANON_LIFETIME off). */
struct vec_int_s;

/*@ assigns \nothing; */
static inline void vec_int_lifetime_open_(struct vec_int_s* v);

/*@ assigns \nothing; */
static inline void vec_int_lifetime_close_(struct vec_int_s* v);

DEFINE_VEC_STRUCTS(int)

/* ════════════════════════════════════════════════════════════════════════════
   VALIDITY PREDICATES (driver-local)
   The capacity clamp is part of validity: it is established by every
   constructor and is what discharges the unchecked byte-count arithmetic
   in insert / remove / append_array (and as_bytes in the facade pass).
   ════════════════════════════════════════════════════════════════════════════ */

/*@
  predicate vec_int_view(vec_int * v) =
    \valid_read(v)
    && v->len <= v->capacity
    && v->capacity <= CANON_VEC_MAX_CAPACITY / sizeof(int)
    && (v->items == \null ==> v->capacity == 0)
    && (v->capacity > 0 ==> \valid_read(v->items + (0 .. v->capacity - 1)));

  predicate vec_int_mut(vec_int * v) =
    vec_int_view(v)
    && \valid(v)
    && (v->capacity > 0 ==> \valid(v->items + (0 .. v->capacity - 1)));

  predicate vec_int_slice_view(vec_int_slice * s) =
    \valid_read(s)
    && (s->len > 0 ==> \valid_read(s->items + (0 .. s->len - 1)));
*/

/* ════════════════════════════════════════════════════════════════════════════
   CONTRACTED PROTOTYPES
   static inline to match DEFINE_VEC_FUNCTIONS(static inline, int) below.
   NULL-tolerant functions carry two-legged contracts (null / live behaviors)
   because NULL tolerance is shipped, documented semantics — not a
   precondition to assume away.
   ════════════════════════════════════════════════════════════════════════════ */

/* ── Constructors / destructor ─────────────────────────────────────────────── */

/* The compound require_msg (buffer || capacity==0) is compiled out under
 * CANON_NO_REQUIRE — the first `requires` below is what carries it. */
/*@ requires buffer == \null ==> capacity == 0;
    requires (capacity > 0 && capacity <= CANON_VEC_MAX_CAPACITY / sizeof(int))
               ==> \valid(buffer + (0 .. capacity - 1));
    assigns \nothing;
    behavior too_big:
      assumes capacity > CANON_VEC_MAX_CAPACITY / sizeof(int);
      ensures \result.items == \null && \result.len == 0 && \result.capacity == 0;
    behavior ok:
      assumes capacity <= CANON_VEC_MAX_CAPACITY / sizeof(int);
      ensures \result.items == buffer;
      ensures \result.len == 0;
      ensures \result.capacity == capacity;
    complete behaviors;
    disjoint behaviors;
*/
static inline vec_int vec_int_init(borrowed(int*) buffer, usize capacity);

/*@ assigns \nothing;
    ensures \result.items == \null && \result.len == 0 && \result.capacity == 0;
*/
static inline vec_int vec_int_empty(void);

/* Allocation effects live in mem_alloc's own contract (allocates clause);
 * at this level nothing pre-existing is written. The OOM and success
 * outcomes are folded into one attempt behavior because WP cannot case-
 * split on mem_alloc's result from outside. */
/*@ assigns \nothing;
    behavior degenerate:
      assumes capacity == 0 || capacity > CANON_VEC_MAX_CAPACITY / sizeof(int);
      ensures \result.items == \null && \result.len == 0 && \result.capacity == 0;
    behavior attempt:
      assumes 0 < capacity <= CANON_VEC_MAX_CAPACITY / sizeof(int);
      ensures \result.len == 0;
      ensures \result.items == \null ==> \result.capacity == 0;
      ensures \result.items != \null ==>
                \result.capacity == capacity
             && \valid(\result.items + (0 .. capacity - 1));
    complete behaviors;
    disjoint behaviors;
*/
static inline vec_int vec_int_alloc(usize capacity);

/*@ requires arena == \null || arena_invariant(arena);
    assigns *arena;
    behavior degenerate:
      assumes arena == \null || capacity == 0
           || capacity > CANON_VEC_MAX_CAPACITY / sizeof(int);
      assigns \nothing;
      ensures \result.items == \null && \result.len == 0 && \result.capacity == 0;
    behavior attempt:
      assumes arena != \null && 0 < capacity <= CANON_VEC_MAX_CAPACITY / sizeof(int);
      assigns *arena;
      ensures \result.len == 0;
      ensures \result.items == \null ==> \result.capacity == 0;
      ensures \result.items != \null ==>
                \result.capacity == capacity
             && \valid(\result.items + (0 .. capacity - 1));
    complete behaviors;
    disjoint behaviors;
*/
static inline vec_int vec_int_arena_alloc(borrowed(Arena*) arena, usize capacity);

/*@ requires v == \null || vec_int_mut(v);
    requires (v != \null && v->items != \null) ==> \freeable(v->items);
    assigns *v;
    behavior null:
      assumes v == \null;
      assigns \nothing;
    behavior live:
      assumes v != \null;
      assigns *v;
      ensures v->items == \null && v->len == 0 && v->capacity == 0;
    complete behaviors;
    disjoint behaviors;
*/
static inline void vec_int_free(dropped(vec_int*) v);

/* ── Queries (all NULL-safe by shipped contract) ───────────────────────────── */

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures \result == (v == \null ? 0 : v->len);
*/
static inline usize vec_int_len(borrowed(const vec_int*) v);

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures \result == (v == \null ? 0 : v->capacity);
*/
static inline usize vec_int_capacity(borrowed(const vec_int*) v);

/* len <= capacity (invariant) makes the subtraction wrap-free. */
/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures v == \null ==> \result == 0;
    ensures v != \null ==> \result == v->capacity - v->len;
*/
static inline usize vec_int_remaining(borrowed(const vec_int*) v);

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures \result <==> (v == \null || v->len == 0);
*/
static inline bool vec_int_is_empty(borrowed(const vec_int*) v);

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures \result <==> (v == \null || v->len >= v->capacity);
*/
static inline bool vec_int_is_full(borrowed(const vec_int*) v);

/* ── Element access ────────────────────────────────────────────────────────── */

/*@ requires v == \null || vec_int_view(v);
    requires out == \null || \valid(out);
    assigns *out;
    behavior hit:
      assumes v != \null && out != \null && i < v->len;
      assigns *out;
      ensures \result == \true;
      ensures *out == v->items[i];
    behavior miss:
      assumes v == \null || out == \null || (v != \null && i >= v->len);
      assigns \nothing;
      ensures \result == \false;
    complete behaviors;
    disjoint behaviors;
*/
static inline bool vec_int_get(borrowed(const vec_int*) v, usize i, borrowed(int*) out);

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    behavior hit:
      assumes v != \null && i < v->len;
      ensures \result.has_value == \true;
      ensures \result.value == v->items[i];
    behavior miss:
      assumes v == \null || (v != \null && i >= v->len);
      ensures \result.has_value == \false;
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int vec_int_get_option(borrowed(const vec_int*) v, usize i);

/* ensure_msg guards are compiled out under NDEBUG: the body is a raw read
 * and these preconditions are its entire safety argument. items != NULL is
 * derivable: i < len <= capacity forces capacity > 0, and the invariant's
 * (items == NULL ==> capacity == 0) contrapositive gives items != NULL. */
/*@ requires vec_int_view(v);
    requires i < v->len;
    assigns \nothing;
    ensures \result == v->items[i];
*/
static inline int vec_int_get_unchecked(borrowed(const vec_int*) v, usize i);

/*@ requires vec_int_view(v);
    requires i < v->len;
    assigns \nothing;
    ensures \result == v->items + i;
*/
static inline borrowed(int*) vec_int_at(borrowed(const vec_int*) v, usize i);

/*@ requires v == \null || vec_int_mut(v);
    assigns v->items[0 .. v->capacity - 1];
    behavior hit:
      assumes v != \null && i < v->len;
      assigns v->items[i];
      ensures \result == \true;
      ensures v->items[i] == val;
      ensures \forall integer k; 0 <= k < v->len && k != i
                ==> v->items[k] == \old(v->items[k]);
    behavior miss:
      assumes v == \null || (v != \null && i >= v->len);
      assigns \nothing;
      ensures \result == \false;
    complete behaviors;
    disjoint behaviors;
*/
static inline bool vec_int_set(borrowed(vec_int*) v, usize i, int val);

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures (v != \null && v->len > 0) ==> \result == v->items;
    ensures (v == \null || v->len == 0) ==> \result == \null;
*/
static inline borrowed(int*) vec_int_first(borrowed(const vec_int*) v);

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures (v != \null && v->len > 0) ==> \result == v->items + (v->len - 1);
    ensures (v == \null || v->len == 0) ==> \result == \null;
*/
static inline borrowed(int*) vec_int_last(borrowed(const vec_int*) v);

/*@ requires v == \null || vec_int_view(v);
    assigns \nothing;
    ensures v == \null ==> \result == \null;
    ensures v != \null ==> \result == v->items;
*/
static inline borrowed(int*) vec_int_data(borrowed(const vec_int*) v);

/* ── Modification — push ───────────────────────────────────────────────────── */

/* Result field names: see FIELD-NAME ASSUMPTION in the header. */
/*@ requires v == \null || vec_int_mut(v);
    assigns v->len, v->items[0 .. v->capacity - 1];
    behavior invalid:
      assumes v == \null || (v != \null && v->items == \null);
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_ARG;
    behavior full:
      assumes v != \null && v->items != \null && v->len >= v->capacity;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_CAPACITY_EXCEEDED;
    behavior ok:
      assumes v != \null && v->items != \null && v->len < v->capacity;
      assigns v->len, v->items[v->len];
      ensures \result.is_ok == \true;
      ensures v->len == \old(v->len) + 1;
      ensures v->items[\old(v->len)] == item;
      ensures \forall integer k; 0 <= k < \old(v->len)
                ==> v->items[k] == \old(v->items[k]);
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error vec_int_push(borrowed(vec_int*) v, int item);

/*@ requires v == \null || vec_int_mut(v);
    assigns v->len, v->items[0 .. v->capacity - 1];
    behavior refuse:
      assumes v == \null || (v != \null && (v->items == \null || v->len >= v->capacity));
      assigns \nothing;
      ensures \result == \false;
    behavior ok:
      assumes v != \null && v->items != \null && v->len < v->capacity;
      assigns v->len, v->items[v->len];
      ensures \result == \true;
      ensures v->len == \old(v->len) + 1;
      ensures v->items[\old(v->len)] == item;
      ensures \forall integer k; 0 <= k < \old(v->len)
                ==> v->items[k] == \old(v->items[k]);
    complete behaviors;
    disjoint behaviors;
*/
static inline bool vec_int_try_push(borrowed(vec_int*) v, int item);

/* Raw under NDEBUG — preconditions are the whole safety argument. */
/*@ requires vec_int_mut(v);
    requires v->items != \null;
    requires v->len < v->capacity;
    assigns v->len, v->items[v->len];
    ensures v->len == \old(v->len) + 1;
    ensures v->items[\old(v->len)] == item;
    ensures \forall integer k; 0 <= k < \old(v->len)
              ==> v->items[k] == \old(v->items[k]);
*/
static inline void vec_int_push_unchecked(borrowed(vec_int*) v, int item);

/* ── Modification — pop ────────────────────────────────────────────────────── */

/*@ requires v == \null || vec_int_mut(v);
    requires out == \null || \valid(out);
    assigns v->len, *out;
    behavior invalid:
      assumes v == \null || out == \null || (v != \null && v->items == \null);
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_ARG;
    behavior empty:
      assumes v != \null && out != \null && v->items != \null && v->len == 0;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_STATE;
    behavior ok:
      assumes v != \null && out != \null && v->items != \null && v->len > 0;
      assigns v->len, *out;
      ensures \result.is_ok == \true;
      ensures v->len == \old(v->len) - 1;
      ensures *out == \old(v->items[v->len - 1]);
      ensures \forall integer k; 0 <= k < v->len
                ==> v->items[k] == \old(v->items[k]);
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error vec_int_pop(borrowed(vec_int*) v, borrowed(int*) out);

/*@ requires v == \null || vec_int_mut(v);
    assigns v->len;
    behavior some:
      assumes v != \null && v->items != \null && v->len > 0;
      assigns v->len;
      ensures \result.has_value == \true;
      ensures \result.value == \old(v->items[v->len - 1]);
      ensures v->len == \old(v->len) - 1;
    behavior none:
      assumes v == \null || (v != \null && (v->items == \null || v->len == 0));
      ensures \result.has_value == \false;
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int vec_int_pop_option(borrowed(vec_int*) v);

/*@ requires v == \null || vec_int_mut(v);
    assigns v->len;
    behavior null:
      assumes v == \null;
      assigns \nothing;
    behavior live:
      assumes v != \null;
      assigns v->len;
      ensures v->len == 0;
    complete behaviors;
    disjoint behaviors;
*/
static inline void vec_int_clear(borrowed(vec_int*) v);

/* ── Modification — insert / remove ────────────────────────────────────────── */

/* Element-shift ensures depend on mem_move's ACSL spec — residual class (d). */
/*@ requires v == \null || vec_int_mut(v);
    assigns v->len, v->items[0 .. v->capacity - 1];
    behavior invalid:
      assumes v == \null || (v != \null && v->items == \null);
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_ARG;
    behavior out_of_range:
      assumes v != \null && v->items != \null && i > v->len;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_OUT_OF_RANGE;
    behavior full:
      assumes v != \null && v->items != \null && i <= v->len && v->len >= v->capacity;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_CAPACITY_EXCEEDED;
    behavior ok:
      assumes v != \null && v->items != \null && i <= v->len && v->len < v->capacity;
      assigns v->len, v->items[i .. v->len];
      ensures \result.is_ok == \true;
      ensures v->len == \old(v->len) + 1;
      ensures v->items[i] == item;
      ensures \forall integer k; 0 <= k < i
                ==> v->items[k] == \old(v->items[k]);
      ensures \forall integer k; i < k <= \old(v->len)
                ==> v->items[k] == \old(v->items[k - 1]);
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error vec_int_insert(borrowed(vec_int*) v, usize i, int item);

/*@ requires v == \null || vec_int_mut(v);
    requires out == \null || \valid(out);
    assigns v->len, *out, v->items[0 .. v->capacity - 1];
    behavior invalid:
      assumes v == \null || out == \null || (v != \null && v->items == \null);
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_ARG;
    behavior empty:
      assumes v != \null && out != \null && v->items != \null && v->len == 0;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_STATE;
    behavior out_of_range:
      assumes v != \null && out != \null && v->items != \null
           && v->len > 0 && i >= v->len;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_OUT_OF_RANGE;
    behavior ok:
      assumes v != \null && out != \null && v->items != \null && i < v->len;
      assigns v->len, *out, v->items[i .. v->len - 1];
      ensures \result.is_ok == \true;
      ensures *out == \old(v->items[i]);
      ensures v->len == \old(v->len) - 1;
      ensures \forall integer k; 0 <= k < i
                ==> v->items[k] == \old(v->items[k]);
      ensures \forall integer k; i <= k < v->len
                ==> v->items[k] == \old(v->items[k + 1]);
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error vec_int_remove(borrowed(vec_int*) v, usize i, borrowed(int*) out);

/*@ requires v == \null || vec_int_mut(v);
    assigns v->len, v->items[0 .. v->capacity - 1];
    behavior some:
      assumes v != \null && v->items != \null && i < v->len;
      assigns v->len, v->items[0 .. v->capacity - 1];
      ensures \result.has_value == \true;
      ensures \result.value == \old(v->items[i]);
      ensures v->len == \old(v->len) - 1;
    behavior none:
      assumes v == \null || (v != \null && (v->items == \null || i >= v->len));
      ensures \result.has_value == \false;
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int vec_int_remove_option(borrowed(vec_int*) v, usize i);

/* ── Bulk operations ───────────────────────────────────────────────────────── */

/* The \separated requires is finding F1 — a contract ADDITION over the
 * shipped doc, mandated by mem_copy's memcpy semantics. Back-propagate. */
/*@ requires v == \null || vec_int_mut(v);
    requires (v != \null && v->items != \null && src != \null
              && v->len + count <= v->capacity) ==>
               \valid_read(src + (0 .. count - 1));
    requires (v != \null && v->items != \null && src != \null
              && v->len + count <= v->capacity) ==>
               \separated(src + (0 .. count - 1),
                          v->items + (0 .. v->capacity - 1));
    assigns v->len, v->items[0 .. v->capacity - 1];
    behavior invalid:
      assumes v == \null || src == \null || (v != \null && v->items == \null);
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_ARG;
    behavior overflow:
      assumes v != \null && v->items != \null && src != \null
           && v->len + count > CANON_USIZE_MAX;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_OVERFLOW;
    behavior too_big:
      assumes v != \null && v->items != \null && src != \null
           && v->len + count <= CANON_USIZE_MAX
           && v->len + count > v->capacity;
      assigns \nothing;
      ensures \result.is_ok == \false && \result.val.err == ERR_CAPACITY_EXCEEDED;
    behavior ok:
      assumes v != \null && v->items != \null && src != \null
           && v->len + count <= v->capacity;
      assigns v->len, v->items[v->len .. v->len + count - 1];
      ensures \result.is_ok == \true;
      ensures v->len == \old(v->len) + count;
      ensures \forall integer k; 0 <= k < \old(v->len)
                ==> v->items[k] == \old(v->items[k]);
      ensures \forall integer k; 0 <= k < count
                ==> v->items[\old(v->len) + k] == src[k];
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error vec_int_append_array(borrowed(vec_int*) v,
                                                      borrowed(const int*) src,
                                                      usize count);

/* Pure delegate to append_array — identical contract (incl. F1). */
/*@ requires v == \null || vec_int_mut(v);
    requires (v != \null && v->items != \null && src != \null
              && v->len + count <= v->capacity) ==>
               \valid_read(src + (0 .. count - 1));
    requires (v != \null && v->items != \null && src != \null
              && v->len + count <= v->capacity) ==>
               \separated(src + (0 .. count - 1),
                          v->items + (0 .. v->capacity - 1));
    assigns v->len, v->items[0 .. v->capacity - 1];
    behavior invalid:
      assumes v == \null || src == \null || (v != \null && v->items == \null);
      ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_ARG;
    behavior overflow:
      assumes v != \null && v->items != \null && src != \null
           && v->len + count > CANON_USIZE_MAX;
      ensures \result.is_ok == \false && \result.val.err == ERR_OVERFLOW;
    behavior too_big:
      assumes v != \null && v->items != \null && src != \null
           && v->len + count <= CANON_USIZE_MAX
           && v->len + count > v->capacity;
      ensures \result.is_ok == \false && \result.val.err == ERR_CAPACITY_EXCEEDED;
    behavior ok:
      assumes v != \null && v->items != \null && src != \null
           && v->len + count <= v->capacity;
      assigns v->len, v->items[v->len .. v->len + count - 1];
      ensures \result.is_ok == \true;
      ensures v->len == \old(v->len) + count;
      ensures \forall integer k; 0 <= k < count
                ==> v->items[\old(v->len) + k] == src[k];
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error vec_int_extend(borrowed(vec_int*) v,
                                                borrowed(const int*) src,
                                                usize count);

/* fill is the invariant canary: `capacity - len` computed unguarded. */
/*@ requires v == \null || vec_int_mut(v);
    assigns v->len, v->items[0 .. v->capacity - 1];
    behavior skip:
      assumes v == \null || (v != \null && v->items == \null);
      assigns \nothing;
    behavior live:
      assumes v != \null && v->items != \null;
      assigns v->len, v->items[v->len .. v->capacity - 1];
      ensures v->len == \old(v->len)
                      + \min(count, (usize)(\old(v->capacity) - \old(v->len)));
      ensures \forall integer k; \old(v->len) <= k < v->len
                ==> v->items[k] == value;
      ensures \forall integer k; 0 <= k < \old(v->len)
                ==> v->items[k] == \old(v->items[k]);
    complete behaviors;
    disjoint behaviors;
*/
static inline void vec_int_fill(borrowed(vec_int*) v, int value, usize count);

/* Both require_msg guards vanish under CANON_NO_REQUIRE — the requires
 * carry them. a == b aliasing is legal and the ensures still hold. */
/*@ requires \valid(a);
    requires \valid(b);
    assigns *a, *b;
    ensures a->items == \old(b->items) && a->len == \old(b->len)
         && a->capacity == \old(b->capacity);
    ensures b->items == \old(a->items) && b->len == \old(a->len)
         && b->capacity == \old(a->capacity);
*/
static inline void vec_int_swap(borrowed(vec_int*) a, borrowed(vec_int*) b);

/* ── Iterator ──────────────────────────────────────────────────────────────── */

/* No NULL check in the body (iter_next tolerates a NULL vec later). */
/*@ assigns \nothing;
    ensures \result.vec == v && \result.index == 0;
*/
static inline vec_int_iter vec_int_iter_init(borrowed(vec_int*) v);

/*@ requires it == \null || \valid(it);
    requires (it != \null && it->vec != \null) ==> vec_int_view(it->vec);
    requires out == \null || \valid(out);
    assigns it->index, *out;
    behavior exhausted_or_invalid:
      assumes it == \null || out == \null
           || (it != \null && it->vec == \null)
           || (it != \null && it->vec != \null && it->index >= it->vec->len);
      assigns \nothing;
      ensures \result == \false;
    behavior yield:
      assumes it != \null && out != \null && it->vec != \null
           && it->index < it->vec->len;
      assigns it->index, *out;
      ensures \result == \true;
      ensures it->index == \old(it->index) + 1;
      ensures *out == it->vec->items[\old(it->index)];
    complete behaviors;
    disjoint behaviors;
*/
static inline bool vec_int_iter_next(borrowed(vec_int_iter*) it, borrowed(int*) out);

/* ── Slice ─────────────────────────────────────────────────────────────────── */

/* Finding F2: the `ok` behavior assumes v->items != \null. The shipped
 * guard does NOT — an empty()-constructed vec sliced [0,0) reaches
 * &v->items[0] == NULL + 0 (pedantic UB, C99 6.5.6). Driver precondition
 * excludes the case until the upstream one-token guard fix lands; the
 * report-only run should confirm the RTE goal by removing the exclusion. */
/*@ requires v == \null || vec_int_view(v);
    requires (v != \null && v->items == \null) ==> !(start == 0 && end == 0);
    assigns \nothing;
    behavior invalid:
      assumes v == \null || start > end || (v != \null && end > v->len);
      ensures \result.items == \null && \result.len == 0;
    behavior ok:
      assumes v != \null && start <= end && end <= v->len;
      ensures \result.items == v->items + start;
      ensures \result.len == end - start;
    complete behaviors;
    disjoint behaviors;
*/
static inline vec_int_slice vec_int_slice_init(borrowed(vec_int*) v, usize start, usize end);

/* Raw under NDEBUG (ensure_msg compiled out) — preconditions carry it. */
/*@ requires vec_int_slice_view(s);
    requires i < s->len;
    assigns \nothing;
    ensures \result == s->items + i;
*/
static inline borrowed(int*) vec_int_slice_get(borrowed(const vec_int_slice*) s, usize i);

/* ════════════════════════════════════════════════════════════════════════════
   REAL MACRO-GENERATED BODIES (second half of DEFINE_VEC)
   WP merges each contract above onto the matching definition emitted here.
   ════════════════════════════════════════════════════════════════════════════ */

DEFINE_VEC_FUNCTIONS(static inline, int)

#endif /* CANON_VDRIVER_VEC_VERIFY_H */
