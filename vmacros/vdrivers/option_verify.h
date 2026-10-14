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

/* vmacros/vdrivers/option_verify.h
 * ============================================================================
 * WP verification driver for the Option<T> macro module (Shape B).
 *
 * READ BY `frama-c -wp` ONLY. NEVER COMPILED OR LINKED.
 *   - Its `static inline` definitions would collide at link time with the
 *     same functions instantiated elsewhere. Keep it out of every build
 *     target and out of any globbed source root (see vmacros.md, build
 *     isolation).
 *
 * Integrity rule (vmacros.md): this driver instantiates the REAL shipped
 * macros. It is a second *caller* of the generated functions, never a
 * hand-written verified copy. There is exactly one source of truth — the
 * macros in option_impl.h / option_defn.h — and WP proves the code users
 * actually compile.
 *
 * Memory model: default `Typed` (NOT `Typed+Cast`). option_T is a by-value
 * tagged struct { bool has_value; T value; } and every function uses it by
 * value or through *typed* pointers. There are no void* casts, so the
 * cast-aware model is unnecessary — same situation as semantics/error.h.
 *
 * Linkage: the contracted prototypes below are `static inline` to MATCH the
 * linkage of DEFINE_OPTION_FUNCTIONS(static inline, int). A bare
 * (external-linkage) prototype ahead of a static-inline definition is a
 * linkage conflict (C99 6.2.2p7) and is rejected by GCC/Clang. WP merges a
 * contract on a same-linkage declaration onto the macro-generated definition.
 *
 * Representative instantiation: `int`. option_int's layout and value-equality
 * are trivial for a scalar, so one primitive instantiation exercises every
 * generated body. A struct instantiation (e.g. Point) can be added later if
 * by-value struct semantics or designated-initializer None need exercising;
 * it is not required for the pattern baseline.
 *
 * Expected residual classes (to be CONFIRMED by the report-only CI run, not
 * asserted here — this driver lands report-only first, per the region.h ->
 * enforced pattern):
 *
 *   (a) Contract-handler non-termination (likely 2, INHERITED).
 *       option_impl.h includes contract.h; under __FRAMAC__ the default
 *       handler carries the non-returning idiom (ensures \false / terminates
 *       \false). These are the same goals as VERIFY-006 cat 4, re-emitted —
 *       NOT option-own. error.h avoided them by including only types.h;
 *       option cannot, because expect()/get()/unwrap()/replace()/take() route
 *       through contract.h. option is the first semantics/ header to inherit
 *       this pair.
 *
 *   (b) Function-pointer dispatch (OWN). map / and_then / or_else / filter /
 *       combine_with / eq each call a caller-supplied function pointer.
 *       Since VERIFY-039 G11 each contract claims only what holds for every
 *       callback it admits (see the combinators below), and the class is one
 *       goal per combinator: the run-time check that the pointer is a valid
 *       function, true for every admitted call and unproved because WP 29
 *       does not implement `\valid_function`. Before G11 the class also held
 *       the termination, exit and frame goals at the call through the
 *       pointer, which the default `terminates \true` and `exits \false` and
 *       the stated `assigns \nothing` made false for some admitted callbacks.
 *
 * The non-combinator surface (some, none, is_some, is_none, get, unwrap_or,
 * unwrap, expect-under-precondition, replace, take) is expected to prove
 * fully — replace/take call the known constructors option_int_some/none
 * (not pointers), so WP reasons through them.
 * ============================================================================ */

#ifndef CANON_VDRIVER_OPTION_VERIFY_H
#define CANON_VDRIVER_OPTION_VERIFY_H

/* Real, unmodified module headers. option_defn.h pulls in option_mangle.h and
 * option_impl.h (which pulls core/primitives/types.h + contract.h). Resolves
 * with `-I .` at the repo root; the nested includes self-resolve relative to
 * their own directories. */
#include "semantics/option/option_defn.h"

/* ── Type + struct from the real macro ─────────────────────────────────────── */

DEFINE_OPTION_STRUCT(int)   /* typedef struct option_int_s { bool has_value; int value; } option_int; */

/* ════════════════════════════════════════════════════════════════════════════
   CONTRACTED PROTOTYPES
   static inline to match DEFINE_OPTION_FUNCTIONS(static inline, int) below.
   ════════════════════════════════════════════════════════════════════════════ */

/* ── Constructors ──────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result.has_value == \true;
    ensures \result.value == v;
*/
static inline option_int option_int_some(int v);

/*@ assigns \nothing;
    ensures \result.has_value == \false;
    ensures \result.value == 0;
*/
static inline option_int option_int_none(void);

/* ── Queries ───────────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result <==> o.has_value;
*/
static inline bool option_int_is_some(option_int o);

/*@ assigns \nothing;
    ensures \result <==> !o.has_value;
*/
static inline bool option_int_is_none(option_int o);

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
static inline bool option_int_get(option_int o, int *out);

/*@ assigns \nothing;
    ensures \result == (o.has_value ? o.value : fallback);
*/
static inline int option_int_unwrap_or(option_int o, int fallback);

/* ── Unsafe extraction ─────────────────────────────────────────────────────── */

/* Under -DCANON_NO_REQUIRE the require_msg guard is a no-op; the precondition
 * is what makes the body's read well-specified. */
/*@ requires o.has_value;
    assigns \nothing;
    ensures \result == o.value;
*/
static inline int option_int_unwrap(option_int o);

/* expect() calls the contract handler on None even under CANON_NO_REQUIRE
 * (CANON_INVOKE_HANDLER_ is not suppressed). Under `requires o.has_value` the
 * None path is dead, so the handler CALL discharges; the handler's OWN
 * non-termination goals are the inherited residual (a), counted separately. */
/*@ requires o.has_value;
    assigns \nothing;
    ensures \result == o.value;
*/
static inline int option_int_expect(option_int o, const char *msg);

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
static inline option_int option_int_map(option_int o, int (*f)(int));

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
static inline option_int option_int_and_then(option_int o, option_int (*f)(int));

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
static inline option_int option_int_or_else(option_int o, option_int (*fallback)(void));

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
static inline option_int option_int_filter(option_int o, bool (*pred)(int));

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
static inline option_int option_int_combine_with(option_int o1, option_int o2,
                                                  int (*combine)(int, int));

/* ── Mutation (known constructors, no fn pointers — fully provable) ─────────── */

/*@ requires \valid(o);
    assigns *o;
    ensures \result.has_value == \old(o->has_value);
    ensures \result.value == \old(o->value);
    ensures o->has_value == \true;
    ensures o->value == new_value;
*/
static inline option_int option_int_replace(option_int *o, int new_value);

/*@ requires \valid(o);
    assigns *o;
    ensures \result.has_value == \old(o->has_value);
    ensures \result.value == \old(o->value);
    ensures o->has_value == \false;
    ensures o->value == 0;
*/
static inline option_int option_int_take(option_int *o);

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
static inline bool option_int_eq(option_int o1, option_int o2,
                                 bool (*eq)(int, int));

/* ════════════════════════════════════════════════════════════════════════════
   REAL MACRO-GENERATED BODIES
   WP merges each contract above onto the matching definition emitted here.
   ════════════════════════════════════════════════════════════════════════════ */

DEFINE_OPTION_FUNCTIONS(static inline, int)

#endif /* CANON_VDRIVER_OPTION_VERIFY_H */
