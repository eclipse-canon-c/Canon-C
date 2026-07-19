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

/* vmacros/coverage/option_cover.c
 * ============================================================================
 * MC/DC cover translation unit for the Option<T> macro module (Shape B).
 *
 * WHY THIS FILE EXISTS
 * ----------------------------------------------------------------------------
 * option is Shape B: the condition sites live in IMPL_OPTION_* macro BODIES
 * (e.g. filter's `has_value && pred(...)`, combine_with's `o1.has_value &&
 * o2.has_value`, eq's two guards). A macro body has no source location until
 * it is expanded. In test/semantics/option_test.c the expansion happens at
 * the CANON_OPTION(int) call site IN THAT TEST FILE, so gcov stamps every
 * option_int_* condition to option_test.c — which the coverage job's
 * test-path removal filter (lcov --remove on the test directory glob) then
 * deletes. The conditions are measured and thrown away.
 *
 * This file re-instantiates CANON_OPTION(int) OUTSIDE test/, so the identical
 * conditions are stamped to vmacros/coverage/option_cover.c and survive the
 * filter. It is the MC/DC analogue of vmacros/vdrivers/option_verify.h (the
 * WP driver): same module, same instantiation, different evidence stream.
 *
 * NOT A CTEST. This TU is built only when -DENABLE_COVERAGE_TUS=ON (set by the
 * coverage job alone) and is invoked directly by that job to emit its .gcda —
 * never registered with add_test(), never fuzzed, never built by the default
 * build / valgrind / lifetime jobs. Build isolation per vmacros.md: explicit
 * target, never file(GLOB).
 *
 * COMPILE FLAGS must match the WP run and the coverage build:
 *   -DCANON_NO_REQUIRE -DNDEBUG   (so the require_msg branches vanish and the
 *                                  measured condition set equals WP's goal set)
 *   --coverage -fcondition-coverage
 *
 * WHAT IT MUST DO
 * ----------------------------------------------------------------------------
 * Call every generated option_int_* function with enough input variety to
 * drive both outcomes of every condition in its macro body:
 *   - get / unwrap_or / map / and_then / or_else / filter : Some AND None
 *   - filter : additionally pred-true AND pred-false (the && right operand)
 *   - combine_with : (Some,Some), (Some,None), (None,Some), (None,None)
 *                    AND combine actually called (both-Some) — covers the &&
 *   - eq : (None,None), (Some,Some-equal), (Some,Some-unequal),
 *          (Some,None) AND (None,Some) — covers both guards and the eq call
 *   - replace / take : from Some AND from None
 *   - some / none / is_some / is_none / unwrap / expect-on-Some : trivially
 *
 * The helper functions and the value choices mirror option_test.c on purpose:
 * the goal is to reproduce the same branch exercise, only stamped to a file
 * the coverage filter keeps.
 *
 * Representative instantiation: int. Same rationale as the WP driver — the
 * macro bodies are type-generic, so the conditions are identical for any T;
 * a second instantiation would re-measure byte-identical conditions. (The
 * Point instantiation in option_test.c exists for -Wunused-function reasons,
 * not for distinct conditions.)
 * ============================================================================ */

/* expect() routes through CANON_INVOKE_HANDLER_, which needs the handler
 * symbol defined in exactly one TU of the link. This is a standalone binary,
 * so define it here — same as option_test.c does. */
#define CANON_CONTRACT_IMPL

#include "semantics/option/option.h"

CANON_OPTION(int)

/* ── Helpers (mirror option_test.c) ──────────────────────────────────────── */

static int  double_it(int x)     { return x * 2; }
static int  add(int a, int b)    { return a + b; }
static bool is_even(int x)       { return (x % 2) == 0; }
static bool is_positive(int x)   { return x > 0; }
static bool int_eq(int a, int b) { return a == b; }

static option_int half_if_even(int x)
{
    if (x % 2 == 0) return option_int_some(x / 2);
    return option_int_none();
}

static option_int always_42(void)   { return option_int_some(42); }
static option_int always_none(void) { return option_int_none(); }

/* Sink so the compiler cannot dead-strip the calls under -O / NDEBUG.
 * volatile guarantees every result is observed, keeping all branches live. */
static volatile int g_sink;

static void observe_opt(option_int o)
{
    g_sink = (int)option_int_is_some(o);
    g_sink = (int)option_int_is_none(o);
    int out = 0;
    if (option_int_get(o, &out)) {
        g_sink = out;
    }
    g_sink = option_int_unwrap_or(o, -1);
}

/* ── Exercise: every condition, both outcomes ────────────────────────────── */

int main(void)
{
    option_int some = option_int_some(21);
    option_int none = option_int_none();

    /* constructors / queries / get / unwrap_or — Some and None */
    observe_opt(some);
    observe_opt(none);

    /* unwrap / expect on Some (None path is a panic; not exercised — it is
     * the contract-violation branch, not an MC/DC condition under NO_REQUIRE) */
    g_sink = option_int_unwrap(some);
    g_sink = option_int_expect(some, "cover: must be present");

    /* map — Some (calls f) and None (skips f): covers has_value ? : */
    g_sink = (int)option_int_is_some(option_int_map(some, double_it));
    g_sink = (int)option_int_is_some(option_int_map(none, double_it));

    /* and_then — Some-returns-Some, Some-returns-None, None */
    g_sink = (int)option_int_is_some(option_int_and_then(option_int_some(42),
                                                         half_if_even));
    g_sink = (int)option_int_is_some(option_int_and_then(option_int_some(7),
                                                         half_if_even));
    g_sink = (int)option_int_is_some(option_int_and_then(none, half_if_even));

    /* or_else — Some (no call) and None (calls fallback): covers has_value ? : */
    g_sink = (int)option_int_is_some(option_int_or_else(some, always_42));
    g_sink = (int)option_int_is_some(option_int_or_else(none, always_42));
    g_sink = (int)option_int_is_some(option_int_or_else(none, always_none));

    /* filter — drives BOTH operands of (has_value && pred(value)):
     *   Some + pred true  -> (T && T)
     *   Some + pred false -> (T && F)
     *   None              -> (F && _) short-circuit                       */
    g_sink = (int)option_int_is_some(option_int_filter(option_int_some(4),
                                                       is_even));      /* T&&T */
    g_sink = (int)option_int_is_some(option_int_filter(option_int_some(3),
                                                       is_even));      /* T&&F */
    g_sink = (int)option_int_is_some(option_int_filter(none,
                                                       is_positive));  /* F&&_ */

    /* combine_with — drives BOTH operands of (o1.has_value && o2.has_value)
     * and the both-Some branch that actually calls combine */
    g_sink = (int)option_int_is_some(
        option_int_combine_with(option_int_some(10),
                                option_int_some(32), add));            /* T&&T */
    g_sink = (int)option_int_is_some(
        option_int_combine_with(option_int_some(10), none, add));      /* T&&F */
    g_sink = (int)option_int_is_some(
        option_int_combine_with(none, option_int_some(32), add));      /* F&&_ */
    g_sink = (int)option_int_is_some(
        option_int_combine_with(none, none, add));                     /* F&&_ */

    /* replace — from Some and from None */
    {
        option_int o = option_int_some(10);
        g_sink = (int)option_int_is_some(option_int_replace(&o, 20));
        option_int n = option_int_none();
        g_sink = (int)option_int_is_some(option_int_replace(&n, 42));
    }

    /* take — from Some and from None (no panic on None) */
    {
        option_int o = option_int_some(7);
        g_sink = (int)option_int_is_some(option_int_take(&o));
        g_sink = (int)option_int_is_some(o);           /* now None */
        option_int n = option_int_none();
        g_sink = (int)option_int_is_some(option_int_take(&n));
    }

    /* eq — drives both guards (!a&&!b ; a!=b) and the eq() call:
     *   (None,None)        -> first guard true
     *   (Some,None)        -> second guard true
     *   (None,Some)        -> second guard true (other direction)
     *   (Some=,Some=)      -> falls through to eq(), true
     *   (Some,Some!=)      -> falls through to eq(), false                 */
    g_sink = (int)option_int_eq(none, none, int_eq);
    g_sink = (int)option_int_eq(option_int_some(1), none, int_eq);
    g_sink = (int)option_int_eq(none, option_int_some(1), int_eq);
    g_sink = (int)option_int_eq(option_int_some(42), option_int_some(42), int_eq);
    g_sink = (int)option_int_eq(option_int_some(1), option_int_some(2), int_eq);

    (void)g_sink;
    return 0;
}
