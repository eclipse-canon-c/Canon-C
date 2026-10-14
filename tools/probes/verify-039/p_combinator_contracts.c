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

/* VERIFY-039 G11 -- docs/deviations.md: the combinators' contracts claim,
 * for every callback, what only some callbacks give. G11's decision
 * recommends contracts that claim only what holds for every callback their
 * preconditions admit. WP 29 has not run on that shape and WP 25 cannot
 * stand in for it, so this probe carries the shape, for
 * .github/workflows/g11-probe.yml to run before any surface is repaired.
 *
 * The real shipped macro bodies, at three instantiations: option<int> as
 * vmacros/vdrivers/option_verify.h instantiates it, result<int, VErr> as
 * result_verify.h does, and result<bool, Error> as deque_verify.h,
 * vec_verify.h and data/priority_queue.h do. The constructors' contracts
 * are copied from those files. The eleven combinators' contracts follow the
 * decision, with C the condition on which the body calls the pointer:
 *
 *   requires C ==> \valid_function(f);    (result's eq: one per pointer)
 *   terminates !C;
 *   exits C;
 *   no decreases clause;
 *   `assigns \nothing` in each behaviour whose assumption excludes C, and
 *   no frame in the behaviours that call the pointer.
 *
 * Their other clauses are the drivers' (option_verify.h, result_verify.h,
 * deque_verify.h), unchanged.
 *
 * Two clients call option_int_map and claim the global g unchanged:
 * client_some(), with Some and a callback that writes g, as
 * p_combinator_callback.c's client() does; and client_none(), with None and
 * a null pointer, a call the new precondition admits.
 *
 * At run time main() calls each combinator on its non-calling branch with
 * null pointers, which the new preconditions admit, and checks the
 * behaviour's postconditions and that g, which every callback here writes,
 * is unchanged; and on each calling branch with a callback that writes g,
 * and checks the behaviour's postconditions and that the callback ran,
 * which is what C says. A null pointer on a calling branch is the undefined
 * call itself and is not made; result<bool, Error> is run on its
 * non-calling branches only. Then each client runs once. Exit status 0 when
 * all 32 checks hold, which is the expected outcome.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG \
 *       -I core/primitives -I core -I semantics -I . \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_combinator_contracts.c -o p && ./p
 * The WP runs, with the functions they select and their options, are in
 * tools/probes/verify-039/g11_probe.py.
 */

#include <stdio.h>

#include "semantics/error.h"
#include "semantics/option/option_defn.h"
#include "semantics/result/result_defn.h"

int g;
static int failures;

/* ════════════════════════════════════════════════════════════════════════════
   option<int>
   ════════════════════════════════════════════════════════════════════════════ */

DEFINE_OPTION_STRUCT(int)

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
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int option_int_and_then(option_int o,
                                             option_int (*f)(int));

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
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int option_int_or_else(option_int o,
                                            option_int (*fallback)(void));

/*@ requires o.has_value ==> \valid_function(pred);
    terminates !o.has_value;
    exits o.has_value;
    behavior none:
      assumes !o.has_value;
      assigns \nothing;
      ensures !\result.has_value;
    behavior some:
      assumes o.has_value;
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
    complete behaviors;
    disjoint behaviors;
*/
static inline option_int option_int_combine_with(option_int o1, option_int o2,
                                                 int (*combine)(int, int));

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
    complete behaviors;
    disjoint behaviors;
*/
static inline bool option_int_eq(option_int o1, option_int o2,
                                 bool (*eq)(int, int));

DEFINE_OPTION_FUNCTIONS(static inline, int)

/* ════════════════════════════════════════════════════════════════════════════
   result<int, VErr>
   ════════════════════════════════════════════════════════════════════════════ */

typedef enum { VERR_A = 0, VERR_B = 1, VERR_C = 2 } VErr;

DEFINE_RESULT_TYPEDEF(int, VErr)
DEFINE_RESULT_STRUCT(int, VErr)

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
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_map(result_int_VErr r,
                                                  int (*f)(int));

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
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_map_err(result_int_VErr r,
                                                      VErr (*f)(VErr));

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
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_and_then(
    result_int_VErr r, result_int_VErr (*f)(int));

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
    complete behaviors;
    disjoint behaviors;
*/
static inline result_int_VErr result_int_VErr_or_else(
    result_int_VErr r, result_int_VErr (*f)(VErr));

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
    behavior both_err:
      assumes !r1.is_ok && !r2.is_ok;
    complete behaviors;
    disjoint behaviors;
*/
static inline bool result_int_VErr_eq(result_int_VErr r1, result_int_VErr r2,
                                      bool (*eq_ok)(int, int),
                                      bool (*eq_err)(VErr, VErr));

DEFINE_RESULT_FUNCTIONS(static inline, int, VErr)

/* ════════════════════════════════════════════════════════════════════════════
   result<bool, Error>: result__Bool_Error, as bool expands to _Bool before
   the token paste (data/priority_queue.h)
   ════════════════════════════════════════════════════════════════════════════ */

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
    complete behaviors;
    disjoint behaviors;
*/
static inline result__Bool_Error result__Bool_Error_or_else(
    result__Bool_Error r, result__Bool_Error (*f)(Error));

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
    behavior both_err:
      assumes !r1.is_ok && !r2.is_ok;
    complete behaviors;
    disjoint behaviors;
*/
static inline bool result__Bool_Error_eq(result__Bool_Error r1,
                                         result__Bool_Error r2,
                                         bool (*eq_ok)(bool, bool),
                                         bool (*eq_err)(Error, Error));

DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)

/* ════════════════════════════════════════════════════════════════════════════
   Clients
   ════════════════════════════════════════════════════════════════════════════ */

static int bump(int x) { g = g + 1; return x; }

/*@ requires g == 0;
    assigns g;
    ensures g == 0;
*/
void client_some(void) {
    option_int r = option_int_map(option_int_some(1), bump);
    (void)r;
}

/*@ requires g == 0;
    assigns \nothing;
    ensures g == 0;
*/
void client_none(void) {
    option_int r = option_int_map(option_int_none(), (int (*)(int))0);
    (void)r;
}

/* ════════════════════════════════════════════════════════════════════════════
   Run time
   ════════════════════════════════════════════════════════════════════════════ */

static int inc(int x) { g = g + 1; return x + 1; }
static option_int some_inc(int x) { g = g + 1; return option_int_some(x + 1); }
static option_int some_seven(void) { g = g + 1; return option_int_some(7); }
static bool positive(int x) { g = g + 1; return x > 0; }
static int add(int a, int b) { g = g + 1; return a + b; }
static bool int_eq(int a, int b) { g = g + 1; return a == b; }
static VErr next_err(VErr e) { g = g + 1; return e == VERR_A ? VERR_B : VERR_C; }
static result_int_VErr ok_inc(int x) { g = g + 1; return result_int_VErr_ok(x + 1); }
static result_int_VErr recover(VErr e) { g = g + 1; (void)e; return result_int_VErr_ok(0); }
static bool err_eq(VErr a, VErr b) { g = g + 1; return a == b; }

static void check(const char *claim, int holds) {
    printf("  %-70s %s\n", claim, holds ? "holds" : "VIOLATED");
    if (!holds) { failures++; }
}

int main(void) {
    printf("G11: the recommended contracts' claims, checked at run time\n");

    /* option<int>, the branch that calls no pointer, with null pointers */
    option_int o;
    bool b;
    g = 0;
    o = option_int_map(option_int_none(), (int (*)(int))0);
    check("option_int_map(None, NULL): None, g unchanged", !o.has_value && g == 0);
    o = option_int_and_then(option_int_none(), (option_int (*)(int))0);
    check("option_int_and_then(None, NULL): None, g unchanged", !o.has_value && g == 0);
    o = option_int_or_else(option_int_some(5), (option_int (*)(void))0);
    check("option_int_or_else(Some 5, NULL): Some 5, g unchanged",
          o.has_value && o.value == 5 && g == 0);
    o = option_int_filter(option_int_none(), (bool (*)(int))0);
    check("option_int_filter(None, NULL): None, g unchanged", !o.has_value && g == 0);
    o = option_int_combine_with(option_int_none(), option_int_some(2), (int (*)(int, int))0);
    check("option_int_combine_with(None, Some, NULL): None, g unchanged",
          !o.has_value && g == 0);
    o = option_int_combine_with(option_int_some(1), option_int_none(), (int (*)(int, int))0);
    check("option_int_combine_with(Some, None, NULL): None, g unchanged",
          !o.has_value && g == 0);
    b = option_int_eq(option_int_none(), option_int_none(), (bool (*)(int, int))0);
    check("option_int_eq(None, None, NULL): true, g unchanged", b && g == 0);
    b = option_int_eq(option_int_some(1), option_int_none(), (bool (*)(int, int))0);
    check("option_int_eq(Some, None, NULL): false, g unchanged", !b && g == 0);

    /* option<int>, the calling branch, with callbacks that write g */
    g = 0;
    o = option_int_map(option_int_some(1), inc);
    check("option_int_map(Some 1, inc): Some, the callback ran", o.has_value && g == 1);
    g = 0;
    o = option_int_and_then(option_int_some(1), some_inc);
    check("option_int_and_then(Some 1, f): the callback ran", g == 1);
    g = 0;
    o = option_int_or_else(option_int_none(), some_seven);
    check("option_int_or_else(None, f): the callback ran", g == 1);
    g = 0;
    o = option_int_filter(option_int_some(1), positive);
    check("option_int_filter(Some 1, pred): the callback ran", g == 1);
    g = 0;
    o = option_int_combine_with(option_int_some(1), option_int_some(2), add);
    check("option_int_combine_with(Some, Some, f): Some, the callback ran",
          o.has_value && g == 1);
    g = 0;
    b = option_int_eq(option_int_some(1), option_int_some(1), int_eq);
    check("option_int_eq(Some, Some, eq): the callback ran", g == 1);

    /* result<int, VErr>, the branch that calls no pointer, with null pointers */
    result_int_VErr r;
    g = 0;
    r = result_int_VErr_map(result_int_VErr_err(VERR_B), (int (*)(int))0);
    check("result_int_VErr_map(Err B, NULL): Err B, g unchanged",
          !r.is_ok && r.val.err == VERR_B && g == 0);
    r = result_int_VErr_map_err(result_int_VErr_ok(4), (VErr (*)(VErr))0);
    check("result_int_VErr_map_err(Ok 4, NULL): Ok 4, g unchanged",
          r.is_ok && r.val.ok == 4 && g == 0);
    r = result_int_VErr_and_then(result_int_VErr_err(VERR_C),
                                 (result_int_VErr (*)(int))0);
    check("result_int_VErr_and_then(Err C, NULL): Err C, g unchanged",
          !r.is_ok && r.val.err == VERR_C && g == 0);
    r = result_int_VErr_or_else(result_int_VErr_ok(6), (result_int_VErr (*)(VErr))0);
    check("result_int_VErr_or_else(Ok 6, NULL): Ok 6, g unchanged",
          r.is_ok && r.val.ok == 6 && g == 0);
    b = result_int_VErr_eq(result_int_VErr_ok(1), result_int_VErr_err(VERR_A),
                           (bool (*)(int, int))0, (bool (*)(VErr, VErr))0);
    check("result_int_VErr_eq(Ok, Err, NULL, NULL): false, g unchanged",
          !b && g == 0);
    g = 0;
    b = result_int_VErr_eq(result_int_VErr_ok(1), result_int_VErr_ok(1),
                           int_eq, (bool (*)(VErr, VErr))0);
    check("result_int_VErr_eq(Ok, Ok, eq_ok, NULL): eq_ok ran", g == 1);
    g = 0;
    b = result_int_VErr_eq(result_int_VErr_err(VERR_A), result_int_VErr_err(VERR_A),
                           (bool (*)(int, int))0, err_eq);
    check("result_int_VErr_eq(Err, Err, NULL, eq_err): eq_err ran", g == 1);

    /* result<int, VErr>, the calling branch, with callbacks that write g */
    g = 0;
    r = result_int_VErr_map(result_int_VErr_ok(1), inc);
    check("result_int_VErr_map(Ok 1, f): Ok, the callback ran", r.is_ok && g == 1);
    g = 0;
    r = result_int_VErr_map_err(result_int_VErr_err(VERR_A), next_err);
    check("result_int_VErr_map_err(Err A, f): Err, the callback ran", !r.is_ok && g == 1);
    g = 0;
    r = result_int_VErr_and_then(result_int_VErr_ok(1), ok_inc);
    check("result_int_VErr_and_then(Ok 1, f): the callback ran", g == 1);
    g = 0;
    r = result_int_VErr_or_else(result_int_VErr_err(VERR_A), recover);
    check("result_int_VErr_or_else(Err A, f): the callback ran", g == 1);

    /* result<bool, Error>, the branch that calls no pointer, null pointers */
    result__Bool_Error q;
    g = 0;
    q = result__Bool_Error_map(result__Bool_Error_err(ERR_INVALID_ARG), (bool (*)(bool))0);
    check("result__Bool_Error_map(Err, NULL): the Err, g unchanged",
          !q.is_ok && q.val.err == ERR_INVALID_ARG && g == 0);
    q = result__Bool_Error_map_err(result__Bool_Error_ok(true), (Error (*)(Error))0);
    check("result__Bool_Error_map_err(Ok true, NULL): Ok true, g unchanged",
          q.is_ok && q.val.ok && g == 0);
    q = result__Bool_Error_and_then(result__Bool_Error_err(ERR_OUT_OF_RANGE),
                                    (result__Bool_Error (*)(bool))0);
    check("result__Bool_Error_and_then(Err, NULL): the Err, g unchanged",
          !q.is_ok && q.val.err == ERR_OUT_OF_RANGE && g == 0);
    q = result__Bool_Error_or_else(result__Bool_Error_ok(false),
                                   (result__Bool_Error (*)(Error))0);
    check("result__Bool_Error_or_else(Ok false, NULL): Ok false, g unchanged",
          q.is_ok && !q.val.ok && g == 0);
    b = result__Bool_Error_eq(result__Bool_Error_ok(true),
                              result__Bool_Error_err(ERR_INVALID_ARG),
                              (bool (*)(bool, bool))0, (bool (*)(Error, Error))0);
    check("result__Bool_Error_eq(Ok, Err, NULL, NULL): false, g unchanged",
          !b && g == 0);

    /* the two clients */
    g = 0;
    client_none();
    check("client_none(): g unchanged", g == 0);
    g = 0;
    client_some();
    check("client_some(): the callback ran (its ensures g == 0 is false)", g == 1);

    return failures ? 1 : 0;
}
