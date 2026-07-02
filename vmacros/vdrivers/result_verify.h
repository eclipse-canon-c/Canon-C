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

/* vmacros/vdrivers/result_verify.h
 * ============================================================================
 * WP verification driver for the Result<T, E> macro module (Shape B).
 *
 * READ BY `frama-c -wp` ONLY. NEVER COMPILED OR LINKED.
 *   - Its `static inline` definitions would collide at link time with the
 *     same functions instantiated elsewhere. Keep it out of every build
 *     target and out of any globbed source root (see vmacros.md, build
 *     isolation). The paired cover TU (vmacros/coverage/result_cover.c)
 *     #includes this file and is the ONLY compiler of it, in the coverage
 *     job only.
 *
 * Integrity rule (vmacros.md): this driver instantiates the REAL shipped
 * macros. It is a second *caller* of the generated functions, never a
 * hand-written verified copy. There is exactly one source of truth — the
 * macros in result_impl.h / result_defn.h — and WP proves the code users
 * actually compile.
 *
 * Memory model: default `Typed` (NOT `Typed+Cast`). result_T_E is a by-value
 * tagged struct { bool is_ok; union { T ok; E err; } val; } and every
 * function uses it by value or through *typed* pointers. There are no void*
 * casts — same situation as error.h and option.
 *
 * ── FIRST UNION-TYPED MODULE — open question flagged up front ─────────────
 * result is the first verified Canon-C module whose payload is a UNION
 * (option's payload is a plain struct field). WP's Typed model has known
 * limitations around union member reasoning; the contracts below read the
 * member consistent with is_ok (i.e. the last-written member on every
 * reachable path), which is the well-behaved case, but whether WP discharges
 * union-member ensures like `\result.val.ok == v` on a compound-literal
 * construction is NOT asserted here — it is exactly what the report-only
 * run must answer. If a union-model residual class emerges, it is a NEW
 * class with no option analogue: classify it in VERIFY-015 before any
 * enforcement numbers are pinned. Do not silently absorb it into the
 * fn-pointer class.
 *
 * Union layout: this driver verifies the DEFAULT (strict-C99, named `.val`
 * member) layout. CANON_RESULT_ANON_UNION is deliberately not exercised —
 * it changes field access syntax, not logic, and the coverage build measures
 * the default layout too, keeping the two evidence streams on one code path.
 *
 * Linkage: the contracted prototypes below are `static inline` to MATCH the
 * linkage of DEFINE_RESULT_FUNCTIONS(static inline, int, VErr). A bare
 * (external-linkage) prototype ahead of a static-inline definition is a
 * linkage conflict (C99 6.2.2p7). WP merges a contract on a same-linkage
 * declaration onto the macro-generated definition.
 *
 * Representative instantiation: (int, VErr) where VErr is a small enum
 * defined below. T and E are deliberately DISTINCT types so the union
 * carries two members of different type — the structural feature that
 * distinguishes result from option. A single (int, VErr) instantiation
 * exercises every generated body; the (Point, MyError) instantiation in
 * result_test.c exists for by-value-struct API exercise, not for distinct
 * conditions (the macro bodies are type-generic).
 *
 * Expected residual classes (to be CONFIRMED by the report-only CI run, not
 * asserted here — this driver lands report-only first, per the option
 * pattern at CI #1065 -> #1067):
 *
 *   (a) Contract-handler non-termination (likely 2, INHERITED).
 *       result_impl.h includes contract.h, so contract_default_handler's
 *       definition is present in the TU and WP emits its goals regardless
 *       of call sites — the same mechanism by which every core/ header and
 *       option inherit the VERIFY-006 cat-4 pair. NOTE a difference from
 *       option worth recording in VERIFY-015: result's panic surface
 *       (unwrap / unwrap_err / expect / get_ok / get_err NULL-guard) routes
 *       EXCLUSIVELY through require_msg, which -DCANON_NO_REQUIRE compiles
 *       to ((void)0). Unlike option's expect (which invoked the handler
 *       via _CANON_INVOKE_HANDLER even under NO_REQUIRE), result's
 *       generated bodies contain NO handler call at all in the verified
 *       configuration — the 2 goals are pure definition-presence
 *       inheritance, and result's own functions should show no
 *       handler-call or unreachable-recursion goals of their own.
 *
 *   (b) Function-pointer dispatch (OWN). map / map_err / and_then /
 *       or_else / eq each call caller-supplied function pointers. WP has no
 *       `calls` clause for an arbitrary pointer and `\valid_function` is
 *       unimplemented in Frama-C 29 — the same limitation as option's six
 *       combinators (VERIFY-014) and region_end (OWN-003 / VERIFY-011
 *       cat 1). For each, the branch that does NOT call the pointer is
 *       specified and provable; the calling branch is left to the
 *       documented residual (rte_function_pointer + assigns-frame +
 *       termination cluster). eq dispatches TWO pointers across two
 *       calling branches (both_ok -> eq_ok, both_err -> eq_err), so its
 *       residual cluster is expected to be the largest — larger than
 *       option_int_eq's 4. Confirm the split on the run.
 *
 *   (c) Possible union-model residuals (OWN, count unknown — see the open
 *       question above). Candidate sites: the constructors' nested
 *       designated-initializer compound literals, and the cross-value
 *       union-member ensures on and()/or() (which relate \result's member
 *       to `other`'s member). If these appear, they are result's first
 *       genuinely new residual class.
 *
 * The non-fn-pointer surface (ok, err, is_ok, is_err, get_ok, get_err,
 * unwrap_or, unwrap, unwrap_err, expect, and, or) is expected to prove
 * fully, modulo class (c).
 * ============================================================================ */

#ifndef CANON_VDRIVER_RESULT_VERIFY_H
#define CANON_VDRIVER_RESULT_VERIFY_H

/* Real, unmodified module headers. result_defn.h pulls in result_mangle.h and
 * result_impl.h (which pulls core/primitives/types.h + contract.h). Resolves
 * with `-I .` at the repo root; the nested includes self-resolve relative to
 * their own directories. */
#include "semantics/result/result_defn.h"

/* ── Representative error type ─────────────────────────────────────────────
 * Single-token identifier (## constraint), distinct from the value type so
 * the union genuinely carries two member types. Three members give the eq /
 * map_err exercises distinct values without any semantic weight. */
typedef enum { VERR_A = 0, VERR_B = 1, VERR_C = 2 } VErr;

/* ── Type + struct from the real macros ────────────────────────────────────
 * TYPEDEF then STRUCT, individually — NOT DEFINE_RESULT_ALL, which would
 * also emit the function bodies before the contracted prototypes below. */

DEFINE_RESULT_TYPEDEF(int, VErr)
DEFINE_RESULT_STRUCT(int, VErr)
/* struct result_int_VErr_s { bool is_ok; union { int ok; VErr err; } val; }; */

/* ════════════════════════════════════════════════════════════════════════════
   CONTRACTED PROTOTYPES
   static inline to match DEFINE_RESULT_FUNCTIONS(static inline, int, VErr)
   below. Union access in the specs uses the default named-member form
   (r.val.ok / r.val.err), matching the layout this driver instantiates.
   ════════════════════════════════════════════════════════════════════════════ */

/* ── Constructors ──────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result.is_ok == \true;
    ensures \result.val.ok == v;
*/
static inline result_int_VErr result_int_VErr_ok(int v);

/*@ assigns \nothing;
    ensures \result.is_ok == \false;
    ensures \result.val.err == err;
*/
static inline result_int_VErr result_int_VErr_err(VErr err);

/* ── Queries ───────────────────────────────────────────────────────────────── */

/*@ assigns \nothing;
    ensures \result <==> r.is_ok;
*/
static inline bool result_int_VErr_is_ok(result_int_VErr r);

/*@ assigns \nothing;
    ensures \result <==> !r.is_ok;
*/
static inline bool result_int_VErr_is_err(result_int_VErr r);

/* ── Safe extraction ───────────────────────────────────────────────────────── */

/* out is dereferenced only on the Ok path, but the public contract requires
 * non-NULL unconditionally (the runtime require_msg checks it on every call,
 * per the get_ok/get_err NULL contract in result.h); the top-level requires
 * states that static guarantee. Same shape as option_int_get. */
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
static inline bool result_int_VErr_get_ok(result_int_VErr r, int *out);

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
static inline bool result_int_VErr_get_err(result_int_VErr r, VErr *out);

/*@ assigns \nothing;
    ensures \result == (r.is_ok ? r.val.ok : fallback);
*/
static inline int result_int_VErr_unwrap_or(result_int_VErr r, int fallback);

/* ── Unsafe extraction ─────────────────────────────────────────────────────────
   Under -DCANON_NO_REQUIRE the require_msg guards are ((void)0); the
   preconditions are what make the bodies' union reads well-specified.
   Note vs option: these bodies contain NO handler invocation in the verified
   configuration (see class (a) in the header comment) — they should prove
   as straight-line functions. */

/*@ requires r.is_ok;
    assigns \nothing;
    ensures \result == r.val.ok;
*/
static inline int result_int_VErr_unwrap(result_int_VErr r);

/*@ requires !r.is_ok;
    assigns \nothing;
    ensures \result == r.val.err;
*/
static inline VErr result_int_VErr_unwrap_err(result_int_VErr r);

/*@ requires r.is_ok;
    assigns \nothing;
    ensures \result == r.val.ok;
*/
static inline int result_int_VErr_expect(result_int_VErr r, const char *msg);

/* ── Combinators (function-pointer dispatch) ───────────────────────────────────
   Structural specs: the non-calling branch is proved; the calling branch is
   the documented fn-pointer residual (class (b)). No `requires
   \valid_function(f)` — it is unimplemented in Frama-C 29 and would not help.
   On the calling branches, the ensures kept are the ones option's run proved
   through the unknown callee (the is_ok shape of a result rebuilt via the
   known ok/err constructor); ensures that would need the callee's VALUE are
   left as comments. */

/* map: Err passes through untouched (provable, including the union member);
 * Ok rewraps f's return via the known ok constructor. */
/*@ assigns \nothing;
    behavior err:
      assumes !r.is_ok;
      ensures \result.is_ok == \false;
      ensures \result.val.err == r.val.err;
    behavior ok:
      assumes r.is_ok;
      ensures \result.is_ok == \true;
      // \result.val.ok == f(r.val.ok) — fn-pointer residual
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_map(result_int_VErr r,
                                                  int (*f)(int));

/* map_err: mirror image — Ok passes through, Err rewraps via err ctor. */
/*@ assigns \nothing;
    behavior ok:
      assumes r.is_ok;
      ensures \result.is_ok == \true;
      ensures \result.val.ok == r.val.ok;
    behavior err:
      assumes !r.is_ok;
      ensures \result.is_ok == \false;
      // \result.val.err == f(r.val.err) — fn-pointer residual
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_map_err(result_int_VErr r,
                                                      VErr (*f)(VErr));

/* and_then: Err passes through (provable); Ok branch returns f's whole
 * Result — nothing structural survives the unknown callee, so no ensures. */
/*@ assigns \nothing;
    behavior err:
      assumes !r.is_ok;
      ensures \result.is_ok == \false;
      ensures \result.val.err == r.val.err;
    behavior ok:
      assumes r.is_ok;
      // \result == f(r.val.ok) — fn-pointer residual
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_and_then(
    result_int_VErr r, result_int_VErr (*f)(int));

/* or_else: Ok passes through (provable); Err branch returns f's Result. */
/*@ assigns \nothing;
    behavior ok:
      assumes r.is_ok;
      ensures \result.is_ok == \true;
      ensures \result.val.ok == r.val.ok;
    behavior err:
      assumes !r.is_ok;
      // \result == f(r.val.err) — fn-pointer residual
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_or_else(
    result_int_VErr r, result_int_VErr (*f)(VErr));

/* ── Eager combinators (no fn pointers — expected fully provable) ─────────────
   These relate \result's union member to ANOTHER value's union member
   (`other`), guarded by the corresponding is_ok — the class-(c) candidate
   sites. The guarded implications only assert the member that is active. */

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
static inline result_int_VErr result_int_VErr_and(result_int_VErr r,
                                                  result_int_VErr other);

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
static inline result_int_VErr result_int_VErr_or(result_int_VErr r,
                                                 result_int_VErr other);

/* ── Comparison (TWO function pointers, TWO calling branches) ─────────────────
   The mismatch branch calls neither pointer and is provable. both_ok calls
   eq_ok; both_err calls eq_err — each is its own fn-pointer residual
   cluster, which is why eq is expected to carry result's largest own
   residual count (cf. option_int_eq's 4, with ONE calling branch). */
/*@ assigns \nothing;
    behavior mismatch:
      assumes r1.is_ok != r2.is_ok;
      ensures \result == \false;
    behavior both_ok:
      assumes r1.is_ok && r2.is_ok;
      // \result == eq_ok(r1.val.ok, r2.val.ok) — fn-pointer residual
    behavior both_err:
      assumes !r1.is_ok && !r2.is_ok;
      // \result == eq_err(r1.val.err, r2.val.err) — fn-pointer residual
    complete behaviors;
    disjoint behaviors;
*/
static inline bool result_int_VErr_eq(result_int_VErr r1,
                                      result_int_VErr r2,
                                      bool (*eq_ok)(int, int),
                                      bool (*eq_err)(VErr, VErr));

/* ════════════════════════════════════════════════════════════════════════════
   REAL MACRO-GENERATED BODIES
   WP merges each contract above onto the matching definition emitted here.
   DEFINE_RESULT_FUNCTIONS (not _ALL): typedef and struct already emitted.
   ════════════════════════════════════════════════════════════════════════════ */

DEFINE_RESULT_FUNCTIONS(static inline, int, VErr)

#endif /* CANON_VDRIVER_RESULT_VERIFY_H */
