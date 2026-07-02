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

/* vmacros/coverage/result_cover.c
 * ============================================================================
 * MC/DC cover translation unit for the Result<T, E> macro module (Shape B).
 *
 * WHY THIS FILE EXISTS
 * ----------------------------------------------------------------------------
 * result is Shape B: the condition sites live in IMPL_RESULT_* macro BODIES
 * (get_ok/get_err's `if (r.is_ok)`, the ternaries in unwrap_or / map /
 * map_err / and_then / or_else / and / or, and eq's two guards). A macro body
 * has no source location until it is expanded. In test/semantics/result_test.c
 * the expansion happens at the CANON_RESULT(int, MyError) call site IN THAT
 * TEST FILE, so gcov stamps every result_int_MyError_* condition to
 * result_test.c — which the coverage job's test-path removal filter then
 * deletes. The conditions are measured and thrown away.
 *
 * This file re-instantiates the same macros OUTSIDE test/ (by including the
 * WP driver, which instantiates (int, VErr)), so the identical conditions are
 * stamped to vmacros/coverage/result_cover.c and survive the filter. It is
 * the MC/DC analogue of vmacros/vdrivers/result_verify.h: same module, same
 * instantiation, different evidence stream.
 *
 * PER-MODULE ATTRIBUTION CHECK (vmacros.md checklist): option confirmed the
 * Shape-B pattern, but result gets its own one-run confirmation — the
 * coverage job's report-only "attribution" debug step dumps result_test's
 * raw gcov --conditions output and checks which file owns the
 * result_int_MyError_* conditions BEFORE this TU's numbers are treated as
 * the module baseline.
 *
 * NOT A CTEST. Built only when -DENABLE_COVERAGE_TUS=ON (coverage job alone)
 * and invoked directly by that job to emit its .gcda — never registered with
 * add_test(), never fuzzed, never built by the default build / valgrind /
 * lifetime jobs. Build isolation per vmacros.md: explicit target, never
 * file(GLOB).
 *
 * COMPILE FLAGS must match the WP run and the coverage build:
 *   -DCANON_NO_REQUIRE -DNDEBUG   (so the require_msg branches vanish and the
 *                                  measured condition set equals WP's goal set)
 *   --coverage -fcondition-coverage
 *
 * PANIC SURFACE NOTE — a deliberate difference from option_cover.c:
 * ----------------------------------------------------------------------------
 * ALL of result's panic guards (unwrap on Err, unwrap_err on Ok, expect on
 * Err, get_ok/get_err NULL out-pointer) are plain require_msg calls, which
 * -DCANON_NO_REQUIRE compiles to ((void)0). Unlike option's expect (whose
 * handler invocation survived NO_REQUIRE and produced MCDC-006's single
 * uncovered outcome), result's generated bodies under the coverage flags
 * contain NO panic branch at all — unwrap / unwrap_err / expect become
 * straight-line functions with zero conditions. The expected MCDC outcome
 * for result is therefore "no unreachable outcomes" (the error.h-style
 * disposition) rather than an MCDC-006-style ceiling — to be CONFIRMED by
 * the per-line gcov dump, not asserted.
 *
 * CONSEQUENCE FOR THIS TU: with the guards compiled out, calling unwrap on
 * an Err (or unwrap_err on an Ok) would read the inactive union member —
 * semantically wrong and unguarded. This TU therefore calls unwrap/expect
 * on Ok values only and unwrap_err on Err values only, exactly matching the
 * WP driver's preconditions. The wrong-variant paths are contract
 * violations, not conditions.
 *
 * WHAT IT MUST DO
 * ----------------------------------------------------------------------------
 * Call every generated result_int_VErr_* function with enough input variety
 * to drive both outcomes of every condition in its macro body:
 *   - get_ok / get_err            : Ok AND Err (the `if (r.is_ok)` branch)
 *   - unwrap_or                   : Ok AND Err (ternary)
 *   - map / map_err               : Ok AND Err (ternary; fn called on the
 *                                   active branch)
 *   - and_then                    : Ok-returns-Ok, Ok-returns-Err, Err
 *   - or_else                     : Err-returns-Ok, Err-returns-Err, Ok
 *   - and                         : Ok AND Err first argument (ternary),
 *                                   with both variants of `other` observed
 *   - or                          : Ok AND Err first argument (ternary),
 *                                   with both variants of `other` observed
 *   - eq                          : (Ok,Err), (Err,Ok)   -> guard 1 true
 *                                   (Ok=,Ok=), (Ok,Ok!=) -> guard 2 true, eq_ok both outcomes
 *                                   (Err=,Err=), (Err,Err!=) -> guard 2 false, eq_err both outcomes
 *   - ok / err / is_ok / is_err / unwrap / unwrap_err / expect : trivially
 *
 * The helper functions and value choices mirror result_test.c on purpose:
 * the goal is to reproduce the same branch exercise, only stamped to a file
 * the coverage filter keeps.
 *
 * Representative instantiation: (int, VErr) via the WP driver — one
 * instantiation, two consumers (vmacros.md). The macro bodies are
 * type-generic, so the conditions are identical for any (T, E); the
 * (Point, MyError) instantiation in result_test.c exists for by-value-struct
 * API exercise, not for distinct conditions.
 * ============================================================================ */

/* result's require_msg routes through contract.h, whose handler symbol must
 * be defined in exactly one TU of the link. This is a standalone binary, so
 * define it here — same as result_test.c does. (Under CANON_NO_REQUIRE the
 * guards are no-ops, but the TU still includes contract.h.) */
#define CANON_CONTRACT_IMPL

/* One instantiation, two consumers: the driver emits the real macro bodies
 * (typedef + struct + DEFINE_RESULT_FUNCTIONS(static inline, int, VErr));
 * the ACSL contracts inside it are comments, invisible to gcc. */
#include "vmacros/vdrivers/result_verify.h"

/* ── Helpers (mirror result_test.c) ──────────────────────────────────────── */

static int  int_double(int x)          { return x * 2; }
static bool int_eq(int a, int b)       { return a == b; }
static bool verr_eq(VErr a, VErr b)    { return a == b; }
static VErr verr_bump(VErr e)          { (void)e; return VERR_C; }

static result_int_VErr checked_double(int x)
{
    if (x > 1000) return result_int_VErr_err(VERR_C);
    return result_int_VErr_ok(x * 2);
}

static result_int_VErr fallback_zero(VErr e)
{
    (void)e;
    return result_int_VErr_ok(0);
}

static result_int_VErr always_err(VErr e)
{
    (void)e;
    return result_int_VErr_err(VERR_B);
}

/* Sink so the compiler cannot dead-strip the calls under -O / NDEBUG.
 * volatile guarantees every result is observed, keeping all branches live. */
static volatile int g_sink;

static void observe_res(result_int_VErr r)
{
    g_sink = (int)result_int_VErr_is_ok(r);
    g_sink = (int)result_int_VErr_is_err(r);

    int out = 0;
    if (result_int_VErr_get_ok(r, &out)) {      /* Ok -> true branch  */
        g_sink = out;
    }
    VErr eout = VERR_A;
    if (result_int_VErr_get_err(r, &eout)) {    /* Err -> true branch */
        g_sink = (int)eout;
    }
    g_sink = result_int_VErr_unwrap_or(r, -1);  /* ternary, both via callers */
}

/* ── Exercise: every condition, both outcomes ────────────────────────────── */

int main(void)
{
    result_int_VErr ok_r  = result_int_VErr_ok(21);
    result_int_VErr err_r = result_int_VErr_err(VERR_B);

    /* constructors / queries / get_ok / get_err / unwrap_or — Ok and Err.
     * observe_res drives both outcomes of get_ok's and get_err's is_ok
     * branch and of unwrap_or's ternary across the two calls. */
    observe_res(ok_r);
    observe_res(err_r);

    /* unwrap / expect on Ok only; unwrap_err on Err only — the wrong-variant
     * paths are require_msg contract violations, compiled out under
     * NO_REQUIRE (see PANIC SURFACE NOTE above). */
    g_sink = result_int_VErr_unwrap(ok_r);
    g_sink = result_int_VErr_expect(ok_r, "cover: must be Ok");
    g_sink = (int)result_int_VErr_unwrap_err(err_r);

    /* map — Ok (calls f) and Err (passthrough): covers is_ok ? : */
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_map(ok_r,  int_double));
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_map(err_r, int_double));

    /* map_err — Err (calls f) and Ok (passthrough): covers is_ok ? : */
    g_sink = (int)result_int_VErr_is_err(result_int_VErr_map_err(err_r, verr_bump));
    g_sink = (int)result_int_VErr_is_err(result_int_VErr_map_err(ok_r,  verr_bump));

    /* and_then — Ok-returns-Ok, Ok-returns-Err, Err-skips-fn */
    g_sink = (int)result_int_VErr_is_ok(
        result_int_VErr_and_then(result_int_VErr_ok(5),    checked_double));
    g_sink = (int)result_int_VErr_is_ok(
        result_int_VErr_and_then(result_int_VErr_ok(2000), checked_double));
    g_sink = (int)result_int_VErr_is_ok(
        result_int_VErr_and_then(err_r,                    checked_double));

    /* or_else — Err-returns-Ok, Err-returns-Err, Ok-skips-fn */
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_or_else(err_r, fallback_zero));
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_or_else(err_r, always_err));
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_or_else(ok_r,  fallback_zero));

    /* and — Ok first arg (returns other) and Err first arg (propagates);
     * both variants of `other` observed so the returned value is consumed */
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_and(ok_r,  result_int_VErr_ok(99)));
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_and(ok_r,  err_r));
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_and(err_r, result_int_VErr_ok(99)));

    /* or — Ok first arg (returns self) and Err first arg (returns other) */
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_or(ok_r,  err_r));
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_or(err_r, result_int_VErr_ok(99)));
    g_sink = (int)result_int_VErr_is_ok(result_int_VErr_or(err_r, result_int_VErr_err(VERR_C)));

    /* eq — drives both guards and BOTH fn pointers, each both outcomes:
     *   (Ok,Err) / (Err,Ok)      -> guard 1 (is_ok mismatch) true, both directions
     *   (Ok=,Ok=) / (Ok,Ok!=)    -> guard 2 (r1 is_ok) true; eq_ok true / false
     *   (Err=,Err=) / (Err,Err!=)-> guard 2 false; eq_err true / false        */
    g_sink = (int)result_int_VErr_eq(ok_r, err_r, int_eq, verr_eq);
    g_sink = (int)result_int_VErr_eq(err_r, ok_r, int_eq, verr_eq);
    g_sink = (int)result_int_VErr_eq(result_int_VErr_ok(7),
                                     result_int_VErr_ok(7),  int_eq, verr_eq);
    g_sink = (int)result_int_VErr_eq(result_int_VErr_ok(7),
                                     result_int_VErr_ok(8),  int_eq, verr_eq);
    g_sink = (int)result_int_VErr_eq(result_int_VErr_err(VERR_B),
                                     result_int_VErr_err(VERR_B), int_eq, verr_eq);
    g_sink = (int)result_int_VErr_eq(result_int_VErr_err(VERR_B),
                                     result_int_VErr_err(VERR_C), int_eq, verr_eq);

    (void)g_sink;
    return 0;
}
