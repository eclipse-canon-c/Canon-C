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

/* vmacros/vdrivers/range_verify.h                                 [VERIFY-028]
 * ============================================================================
 * WP driver for data/range.h — interposition only.
 *
 * range.h's own 15 functions are contracted IN PLACE in range.h. This driver
 * exists for one reason: to interpose a CONTRACTED option_isize before
 * range.h instantiates a bare one (vec run-1 lesson R1: a bare CANON_OPTION
 * leaves the combinators spec-less and drowns the caller).
 *
 * This is bitset_verify.h transcribed usize -> isize, and nothing else. That
 * sameness is load-bearing: VERIFY-028 P2 predicts option's 32 inherited
 * residuals re-emit byte-identical modulo the type prefix. If this file
 * diverges from bitset_verify.h beyond the rename, P2 is no longer testing
 * what it says. VERIFY-039 G11 repaired the combinators of both files
 * together, from option_verify.h's repaired contracts, so the sameness
 * holds; option's residuals here are its six run-time checks since.
 *
 * It depends on VERIFY-028 F3: range.h's CANON_OPTION(isize) is now behind
 * CANON_OPTION_ISIZE_DEFINED. Before F3, interposition was impossible — the
 * bare instantiation would redefine everything declared below.
 * ============================================================================ */

#ifndef CANON_VDRIVER_RANGE_VERIFY_H
#define CANON_VDRIVER_RANGE_VERIFY_H

/* ── INTERPOSITION ──────────────────────────────────────────────────────────
 * Claim the guard BEFORE range.h is read, so range.h skips its own bare
 * emission and the contracted instantiation below is the one that stands.
 * Exactly deque_verify.h's move with CANON_RESULT_BOOL_ERROR_DEFINED. If this
 * define is ever removed, the build still succeeds and the proof silently
 * degrades to spec-less option — which is why the job's roll-call checks for
 * option's arm by name rather than trusting the count (32 goals until
 * VERIFY-039 G11, the six run-time checks on the pointers since). */
#define CANON_OPTION_ISIZE_DEFINED

/* Real, unmodified module header. option_defn.h pulls in option_mangle.h and
 * option_impl.h (which pull core/primitives/types.h + contract.h). Resolves
 * with `-I .` at the repo root. */
#include "semantics/option/option_defn.h"

/* ── Type + struct from the real macro ─────────────────────────────────────── */

DEFINE_OPTION_STRUCT(isize)

/* ════════════════════════════════════════════════════════════════════════════
   CONTRACTED PROTOTYPES — transcribed from option_verify.h, int -> isize (bitset_verify.h, usize -> isize)
   static inline to match DEFINE_OPTION_FUNCTIONS(static inline, isize) below.
   ════════════════════════════════════════════════════════════════════════════ */

/* ── Constructors ──────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result.has_value == \true;
    ensures \result.value == v;
*/
static inline option_isize option_isize_some(isize v);

/*@ assigns \nothing;
    ensures \result.has_value == \false;
    ensures \result.value == 0;
*/
static inline option_isize option_isize_none(void);

/* ── Queries ───────────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result <==> o.has_value;
*/
static inline bool option_isize_is_some(option_isize o);

/*@ assigns \nothing;
    ensures \result <==> !o.has_value;
*/
static inline bool option_isize_is_none(option_isize o);

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
static inline bool option_isize_get(option_isize o, isize *out);

/*@ assigns \nothing;
    ensures \result == (o.has_value ? o.value : fallback);
*/
static inline isize option_isize_unwrap_or(option_isize o, isize fallback);

/* ── Unsafe extraction ─────────────────────────────────────────────────────── */

/* Under -DCANON_NO_REQUIRE the require_msg guard is a no-op; the precondition
 * is what makes the body's read well-specified. */
/*@ requires o.has_value;
    assigns \nothing;
    ensures \result == o.value;
*/
static inline isize option_isize_unwrap(option_isize o);

/* expect() calls the contract handler on None even under CANON_NO_REQUIRE
 * (CANON_INVOKE_HANDLER_ is not suppressed). Under `requires o.has_value` the
 * None path is dead, so the handler CALL discharges; the handler's OWN
 * non-termination goals are the inherited residual (a), counted separately. */
/*@ requires o.has_value;
    assigns \nothing;
    ensures \result == o.value;
*/
static inline isize option_isize_expect(option_isize o, const char *msg);

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
static inline option_isize option_isize_map(option_isize o, isize (*f)(isize));

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
static inline option_isize option_isize_and_then(option_isize o,
                                                 option_isize (*f)(isize));

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
static inline option_isize option_isize_or_else(option_isize o,
                                                option_isize (*fallback)(void));

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
static inline option_isize option_isize_filter(option_isize o,
                                               bool (*pred)(isize));

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
static inline option_isize option_isize_combine_with(option_isize o1,
                                                     option_isize o2,
                                                     isize (*combine)(isize, isize));

/* ── Mutation (known constructors, no fn pointers — fully provable) ─────────── */

/*@ requires \valid(o);
    assigns *o;
    ensures \result.has_value == \old(o->has_value);
    ensures \result.value == \old(o->value);
    ensures o->has_value == \true;
    ensures o->value == new_value;
*/
static inline option_isize option_isize_replace(option_isize *o, isize new_value);

/*@ requires \valid(o);
    assigns *o;
    ensures \result.has_value == \old(o->has_value);
    ensures \result.value == \old(o->value);
    ensures o->has_value == \false;
    ensures o->value == 0;
*/
static inline option_isize option_isize_take(option_isize *o);

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
static inline bool option_isize_eq(option_isize o1, option_isize o2,
                                   bool (*eq)(isize, isize));

/* ════════════════════════════════════════════════════════════════════════════
   REAL MACRO-GENERATED BODIES
   WP merges each contract above onto the matching definition emitted here.
   ════════════════════════════════════════════════════════════════════════════ */

DEFINE_OPTION_FUNCTIONS(static inline, isize)

/* ════════════════════════════════════════════════════════════════════════════
   THE SUBJECT MODULE
   Included LAST and UNMODIFIED. Its own contracts live in the header itself
   (in-place), not here — this driver adds nothing to range's own surface.
   ════════════════════════════════════════════════════════════════════════════ */

#include "data/range.h"

#endif /* CANON_VDRIVER_RANGE_VERIFY_H */
