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

/* vmacros/vdrivers/bitset_verify.h                                [VERIFY-020]
 * ============================================================================
 * WP driver for data/bitset.h.
 *
 * ── WHY THIS FILE EXISTS ────────────────────────────────────────────────────
 *
 * NOT because bitset.h fails to parse. It parses fine. That was true for
 * about one CI run and is worth recording, because the first attempt at this
 * arc pointed frama-c straight at data/bitset.h and got:
 *
 *     data/bitset.h:971: syntax error, before or at token:
 *     bitset_find_first_option
 *     [kernel] Frama-C aborted: invalid user input.
 *
 * bitset.h used option_usize in its three _option wrappers without
 * instantiating it, and required every includer to do so first —
 * test/data/bitset_test.c carried the instruction in a comment. bitset.h was
 * the ONLY header in the tree that made a caller pre-instantiate a generic;
 * vec_impl.h had solved the identical problem years earlier with a
 * type-specific guard.
 *
 * That was fixed AT THE SOURCE (finding F3), not worked around here:
 * bitset.h now instantiates option_usize itself behind
 * CANON_OPTION_USIZE_DEFINED, and is self-contained. The proof arc surfaced
 * a real library wart, and the wart got fixed rather than absorbed into a
 * driver — which is the outcome the campaign is supposed to produce.
 *
 * So bitset.h REMAINS an in-place-contracted header. Its 32 functions are
 * contracted in the header itself and stay there. This driver exists for one
 * narrower reason: bitset.h's self-instantiation is a bare CANON_OPTION,
 * which emits all sixteen option functions SPEC-LESS.
 *
 * ── A NEW SHAPE: THIN INSTANTIATION DRIVER ──────────────────────────────────
 *
 * This driver is NOT a Shape-B driver in the option/result/vec/deque sense.
 * Those transcribe contracts for macro-generated functions that have no
 * other home. bitset's own 32 functions are contracted IN PLACE, in
 * bitset.h, and stay there. This file exists for exactly one reason: to
 * interpose a CONTRACTED option_usize before bitset.h is read.
 *
 * Call it what it is — a thin interposition driver. It is the smallest
 * driver in the project and it contracts none of the subject module's
 * functions. The distinction matters for the composability record: bitset's
 * own goals are attributed to bitset.h, exactly as slice/borrow/diag's are,
 * and only the option arm comes from here. bitset's classification in the
 * status table stays IN-PLACE, with a footnote for the interposition.
 *
 * ── WHY SPEC-LESS IS NOT GOOD ENOUGH ────────────────────────────────────────
 *
 * bitset calls only four of the sixteen — some, none, is_some, unwrap — and
 * never touches a combinator, so vec's run-1 lesson R1 (spec-less
 * combinators make WP assume every option call assigns everything, drowning
 * the caller) does not bite bitset at its call sites the way it bit vec.
 *
 * The problem is inheritance, not drowning. DEFINE_OPTION_FUNCTIONS defines
 * all sixteen in the TU whether or not bitset calls them, and WP generates
 * goals for defined functions regardless. Spec-less, those goals are NOT
 * option's goals: different contracts, different goal names, different
 * count. bitset would carry an option arm that is INCOMPARABLE to option's
 * home unit, and every module before it has claimed its inherited residuals
 * re-emit BYTE-IDENTICALLY from an already-documented set — diag's 10-for-10,
 * vec's 123 machine-diffed against arena's roll-call, deque's 32 equal modulo
 * the prefix. Losing that for bitset would cost more than this file does.
 *
 * option_verify.h cannot be reused: it instantiates option_int, not
 * option_usize, for the same reason deque could not reuse result_verify.h
 * for result__Bool_Error.
 *
 * The resolution is deque's ARM B pattern: transcribe the FULL home contract
 * set — all 16 functions — retyped int -> usize. That is the
 * instantiation-identity rule's first option, so this instance is comparable
 * to option's home unit clause-for-clause and needs no written exemption for
 * a reduced surface. Shapes below are verbatim from
 * vmacros/vdrivers/option_verify.h with int -> usize; no clause is invented,
 * weakened, or dropped. The combinators' shapes were copied again at
 * VERIFY-039 G11, after option_verify.h's repair.
 *
 * ── MODEL ───────────────────────────────────────────────────────────────────
 *
 * Typed+Cast, forced by bitset_as_bytes / bitset_as_cbytes handing
 * bs->words (u64*) to bytes_from / cbytes_from. Goal names carry the
 * typed_cast_ prefix, per diag.h and vec.
 *
 * ── PREDICTIONS (revised; see the job banner for the full set) ──────────────
 *
 * P2 is REVISED by this file's existence. The original patch-1 prediction
 * was a 43-goal inherited SUBSET, on the reasoning that option's 32 and
 * bits.h's 15 would only enter once the _option and count/find_* families
 * were contracted. That reasoning was wrong twice over:
 *
 *   - option's 32 enter NOW, because this driver contracts option_usize and
 *     WP verifies every contracted function in the TU.
 *   - bits.h's 15 enter NOW too, and for a reason that has nothing to do
 *     with bitset contracting its callers: bits.h's own functions carry
 *     their own contracts, so WP verifies them wherever they are in scope.
 *
 * REVISED P2: the FULL 92-goal arm appears in patch 1 —
 *     contract 2 + bits 15 + memory 20 + slice-libc 13
 *     + ptr 6 + checked 2 + borrow 2 + option 32 = 92.
 *
 * This is strictly better than the original prediction: it makes P3
 * (bits.h's 15 re-emitting byte-identically, the first such test on a
 * timeout-class ARITHMETIC surface) testable on the very first run instead
 * of deferred to patch 2.
 * ============================================================================
 */

#ifndef CANON_VDRIVER_BITSET_VERIFY_H
#define CANON_VDRIVER_BITSET_VERIFY_H

/* ── INTERPOSITION ──────────────────────────────────────────────────────────
 * Claim the guard BEFORE bitset.h is read, so bitset.h skips its own bare
 * emission and the contracted instantiation below is the one that stands.
 * Exactly deque_verify.h's move with CANON_RESULT_BOOL_ERROR_DEFINED. If this
 * define is ever removed, the build still succeeds and the proof silently
 * degrades to spec-less option — which is why the job's roll-call checks for
 * option's arm by name rather than trusting the count (32 goals until
 * VERIFY-039 G11, the six run-time checks on the pointers since). */
#define CANON_OPTION_USIZE_DEFINED

/* Real, unmodified module header. option_defn.h pulls in option_mangle.h and
 * option_impl.h (which pull core/primitives/types.h + contract.h). Resolves
 * with `-I .` at the repo root. */
#include "semantics/option/option_defn.h"

/* ── Type + struct from the real macro ─────────────────────────────────────── */

DEFINE_OPTION_STRUCT(usize)

/* ════════════════════════════════════════════════════════════════════════════
   CONTRACTED PROTOTYPES — transcribed from option_verify.h, int -> usize
   static inline to match DEFINE_OPTION_FUNCTIONS(static inline, usize) below.
   ════════════════════════════════════════════════════════════════════════════ */

/* ── Constructors ──────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result.has_value == \true;
    ensures \result.value == v;
*/
static inline option_usize option_usize_some(usize v);

/*@ assigns \nothing;
    ensures \result.has_value == \false;
    ensures \result.value == 0;
*/
static inline option_usize option_usize_none(void);

/* ── Queries ───────────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result <==> o.has_value;
*/
static inline bool option_usize_is_some(option_usize o);

/*@ assigns \nothing;
    ensures \result <==> !o.has_value;
*/
static inline bool option_usize_is_none(option_usize o);

/* ── Safe extraction ───────────────────────────────────────────────────────── */

/* out is dereferenced only on the Some path, but the public contract requires
 * non-NULL unconditionally (the runtime require_msg checks it on every call);
 * the top-level requires states that static guarantee. */
/*@ requires \valid(out);
    assigns *out;
    behavior some:
      assumes o.has_value;
      assigns *out;
      ensures \result == \true;
      ensures *out == o.value;
    behavior none:
      assumes !o.has_value;
      assigns \nothing;
      ensures \result == \false;
      ensures *out == \old(*out);
    complete behaviors;
    disjoint behaviors;
*/
static inline bool option_usize_get(option_usize o, usize *out);

/*@ assigns \nothing;
    ensures \result == (o.has_value ? o.value : fallback);
*/
static inline usize option_usize_unwrap_or(option_usize o, usize fallback);

/* ── Unsafe extraction ─────────────────────────────────────────────────────── */

/* Under -DCANON_NO_REQUIRE the require_msg guard is a no-op; the precondition
 * is what makes the body's read well-specified. */
/*@ requires o.has_value;
    assigns \nothing;
    ensures \result == o.value;
*/
static inline usize option_usize_unwrap(option_usize o);

/* expect() calls the contract handler on None even under CANON_NO_REQUIRE
 * (CANON_INVOKE_HANDLER_ is not suppressed). Under `requires o.has_value` the
 * None path is dead, so the handler CALL discharges; the handler's OWN
 * non-termination goals are the inherited residual (a), counted separately. */
/*@ requires o.has_value;
    assigns \nothing;
    ensures \result == o.value;
*/
static inline usize option_usize_expect(option_usize o, const char *msg);

/* ── Combinators (function-pointer dispatch) ───────────────────────────────────
   VERIFY-039 G11. Each contract claims only what holds for every callback
   its preconditions admit. With C the condition on which the body calls the
   pointer: the pointer must be a valid function where it is called
   (`requires C ==> \valid_function(f)`); termination and no exit are
   claimed only where nothing is called (`terminates !C; exits C;`, and no
   `decreases` clause); and the empty frame is stated only in the behaviours
   that call nothing. The defaults claimed termination and no exit for every
   call, and `assigns \nothing` claimed the frame, which a callback that
   loops, exits or writes a global falsifies
   (tools/probes/verify-039/p_combinator_callback.c).

   WP 29 does not implement `\valid_function`: the precondition gives the
   proof no hypothesis, and the run-time check on the pointer stays
   unproved, true for every admitted call. A caller proves the precondition
   where its arguments make C false, and gets no frame from any call: WP
   frames a call by a default `assigns`, an unguarded behaviour's or a
   complete set's, and the calling behaviour states none (VERIFY-039 G11,
   the probe: tools/probes/verify-039/p_combinator_contracts.c).
   ────────────────────────────────────────────────────────────────────────────*/

/*@ requires o.has_value ==> \valid_function(f);
    terminates !o.has_value;
    exits o.has_value;
    behavior none:
      assumes !o.has_value;
      assigns \nothing;
      ensures !\result.has_value;
    behavior some:
      assumes o.has_value;
      ensures \result.has_value;
    complete behaviors;
    disjoint behaviors;
*/
static inline option_usize option_usize_map(option_usize o, usize (*f)(usize));

/*@ requires o.has_value ==> \valid_function(f);
    terminates !o.has_value;
    exits o.has_value;
    behavior none:
      assumes !o.has_value;
      assigns \nothing;
      ensures !\result.has_value;
    behavior some:
      assumes o.has_value;
      // result == f(o.value): not stated, it is the callback's value
    complete behaviors;
    disjoint behaviors;
*/
static inline option_usize option_usize_and_then(option_usize o,
                                                 option_usize (*f)(usize));

/*@ requires !o.has_value ==> \valid_function(fallback);
    terminates o.has_value;
    exits !o.has_value;
    behavior some:
      assumes o.has_value;
      assigns \nothing;
      ensures \result.has_value == o.has_value;
      ensures \result.value == o.value;
    behavior none:
      assumes !o.has_value;
      // result == fallback(): not stated, it is the callback's value
    complete behaviors;
    disjoint behaviors;
*/
static inline option_usize option_usize_or_else(option_usize o,
                                                option_usize (*fallback)(void));

/* filter's guard is `(o.has_value && pred(o.value))`. The has_value==false
 * branch short-circuits before calling pred and returns None; the
 * has_value==true branch calls pred. */
/*@ requires o.has_value ==> \valid_function(pred);
    terminates !o.has_value;
    exits o.has_value;
    behavior none:
      assumes !o.has_value;
      assigns \nothing;
      ensures !\result.has_value;
    behavior some:
      assumes o.has_value;
      // result is o or None depending on pred(o.value): not stated
    complete behaviors;
    disjoint behaviors;
*/
static inline option_usize option_usize_filter(option_usize o,
                                               bool (*pred)(usize));

/*@ requires (o1.has_value && o2.has_value) ==> \valid_function(combine);
    terminates !(o1.has_value && o2.has_value);
    exits o1.has_value && o2.has_value;
    behavior any_none:
      assumes !o1.has_value || !o2.has_value;
      assigns \nothing;
      ensures !\result.has_value;
    behavior both_some:
      assumes o1.has_value && o2.has_value;
      ensures \result.has_value;
      // result.value == combine(o1.value, o2.value): not stated
    complete behaviors;
    disjoint behaviors;
*/
static inline option_usize option_usize_combine_with(option_usize o1,
                                                     option_usize o2,
                                                     usize (*combine)(usize, usize));

/* ── Mutation (known constructors, no fn pointers — fully provable) ─────────── */

/*@ requires \valid(o);
    assigns *o;
    ensures \result.has_value == \old(o->has_value);
    ensures \result.value == \old(o->value);
    ensures o->has_value == \true;
    ensures o->value == new_value;
*/
static inline option_usize option_usize_replace(option_usize *o, usize new_value);

/*@ requires \valid(o);
    assigns *o;
    ensures \result.has_value == \old(o->has_value);
    ensures \result.value == \old(o->value);
    ensures o->has_value == \false;
    ensures o->value == 0;
*/
static inline option_usize option_usize_take(option_usize *o);

/* ── Comparison (function-pointer dispatch on the Some/Some branch) ───────────
   The combinators' shape (VERIFY-039 G11), with two behaviours that call
   nothing, each stating the empty frame. */

/*@ requires (o1.has_value && o2.has_value) ==> \valid_function(eq);
    terminates !(o1.has_value && o2.has_value);
    exits o1.has_value && o2.has_value;
    behavior both_none:
      assumes !o1.has_value && !o2.has_value;
      assigns \nothing;
      ensures \result == \true;
    behavior mismatch:
      assumes o1.has_value != o2.has_value;
      assigns \nothing;
      ensures \result == \false;
    behavior both_some:
      assumes o1.has_value && o2.has_value;
      // result == eq(o1.value, o2.value): not stated, the callback's value
    complete behaviors;
    disjoint behaviors;
*/
static inline bool option_usize_eq(option_usize o1, option_usize o2,
                                   bool (*eq)(usize, usize));

/* ════════════════════════════════════════════════════════════════════════════
   REAL MACRO-GENERATED BODIES
   WP merges each contract above onto the matching definition emitted here.
   ════════════════════════════════════════════════════════════════════════════ */

DEFINE_OPTION_FUNCTIONS(static inline, usize)

/* ════════════════════════════════════════════════════════════════════════════
   THE SUBJECT MODULE
   Included LAST and UNMODIFIED. Its own contracts live in the header itself
   (in-place), not here — this driver adds nothing to bitset's own surface.
   ════════════════════════════════════════════════════════════════════════════ */

#include "data/bitset.h"

#endif /* CANON_VDRIVER_BITSET_VERIFY_H */
