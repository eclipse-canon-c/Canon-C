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

/* vmacros/coverage/vec_range_cover.c                              [MCDC-017]
 * ============================================================================
 * MC/DC cover translation unit for data/vec/vec_range.h.
 *
 * WHY THIS FILE EXISTS: vec_range.h's conditions live in the macro body of
 * IMPL_VEC_EXTEND_FROM_RANGE and are stamped to the instantiation site. The
 * only instantiations were in test/data/vec_test.c, which the coverage job's
 * filter on test paths removes, so vec_range.h had no row (VERIFY-041,
 * "Run 2, scored"). This TU instantiates it outside test/, as vec_cover.c
 * does for vec.
 *
 * NOT A CTEST. Built only under -DENABLE_COVERAGE_TUS=ON and run directly
 * by the coverage job. Flags as the WP run: -DCANON_NO_REQUIRE -DNDEBUG.
 *
 * Two instantiations, each stamped to its own line:
 *   int  -- the WP driver's type;
 *   u64  -- unsigned and as wide as isize, the one class of type where the
 *           representability check's sign clause can be true.
 *
 * OUTCOMES DEAD AT A TYPE (MCDC-017, written down before the first run), in
 * the check  A || (B && C), A = (isize)elem != value, B = value < 0,
 * C = !(elem < (type)1):
 *   D1  int: C true  -- B true means a negative value that fits, so elem < 1.
 *   D2  u64: A true  -- every isize value round-trips through u64.
 *   D3  u64: C false -- a negative value maps to value + 2^64 >= 1.
 * Every other outcome is driven below.
 * ============================================================================ */

/* Handler symbol for the link, as in the other cover TUs. No path reaches it
 * under CANON_NO_REQUIRE. */
#define CANON_CONTRACT_IMPL

#include "semantics/error.h"
#include "semantics/result/result.h"
#include "data/vec/vec_defn.h"
#include "data/vec/vec_range.h"
#include "core/primitives/limits.h"

DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_RANGE(static inline, int)

DEFINE_VEC_STRUCTS(u64)
DEFINE_VEC_RANGE(static inline, u64)

static volatile int g_sink;

static void observe(result__Bool_Error r)
{
    g_sink = (int)r.is_ok;
    if (!r.is_ok) {
        g_sink = (int)r.val.err;
    }
}

int main(void)
{
    /* int's bounds, computed: <limits.h> is shadowed on this include path. */
    const isize imax = (isize)(int)(~0u >> 1);

    int buf[8] = { 0 };
    vec_int v      = { .items = buf,  .len = 0, .capacity = 8 };
    vec_int vnoitm = { .items = NULL, .len = 0, .capacity = 0 };
    /* Fabricated: a len near USIZE_MAX with the capacity to match. The
     * function returns at the overflow guard, before any memory access
     * (MCDC-016's policy). */
    vec_int vhuge  = { .items = buf, .len = CANON_USIZE_MAX - 1u,
                       .capacity = CANON_USIZE_MAX };

    /* !v || !v->items: v NULL; items NULL; both non-NULL (below). */
    observe(vec_int_extend_from_range(NULL, range_make(0, 1, 1)));
    observe(vec_int_extend_from_range(&vnoitm, range_make(0, 1, 1)));

    /* step == ISIZE_MIN: true (a range built by hand), false (every other). */
    {
        range bad = { 0, -10, CANON_ISIZE_MIN };
        observe(vec_int_extend_from_range(&v, bad));
    }
    /* step == 0: true (built by hand; Ok, nothing appended), false. */
    {
        range zero = { 0, 10, 0 };
        observe(vec_int_extend_from_range(&v, zero));
    }
    /* !checked_add: true (vhuge, count 3), false (every other). */
    observe(vec_int_extend_from_range(&vhuge, range_make(0, 3, 1)));
    /* total > capacity: true, false. */
    observe(vec_int_extend_from_range(&v, range_make(0, 9, 1)));
    /* the loop and the check at int: values that fit (A false, B false and
     * B true, C false), and a value past INT_MAX (A true) where isize is
     * wider than int. */
    observe(vec_int_extend_from_range(&v, range_make(0, 2, 1)));
    observe(vec_int_extend_from_range(&v, range_make(-2, 0, 1)));
#if CANON_ISIZE_MAX > 0x7FFFFFFF
    observe(vec_int_extend_from_range(&v, range_make(imax, imax + 2, 1)));
#else
    (void)imax;
#endif
    /* an empty range: the loop's condition false at once */
    observe(vec_int_extend_from_range(&v, range_make(5, 5, 1)));

    /* u64: the same guards, and the check's live outcomes there: A false,
     * B false (a non-negative value), B true with C true (a negative one). */
    {
        u64 ub[4] = { 0 };
        vec_u64 u      = { .items = ub,   .len = 0, .capacity = 4 };
        vec_u64 unoitm = { .items = NULL, .len = 0, .capacity = 0 };
        vec_u64 uhuge  = { .items = ub, .len = CANON_USIZE_MAX - 1u,
                           .capacity = CANON_USIZE_MAX };
        range bad  = { 0, -10, CANON_ISIZE_MIN };
        range zero = { 0, 10, 0 };
        observe(vec_u64_extend_from_range(NULL, range_make(0, 1, 1)));
        observe(vec_u64_extend_from_range(&unoitm, range_make(0, 1, 1)));
        observe(vec_u64_extend_from_range(&u, bad));
        observe(vec_u64_extend_from_range(&u, zero));
        observe(vec_u64_extend_from_range(&uhuge, range_make(0, 3, 1)));
        observe(vec_u64_extend_from_range(&u, range_make(0, 5, 1)));
        observe(vec_u64_extend_from_range(&u, range_make(0, 2, 1)));
        observe(vec_u64_extend_from_range(&u, range_make(-1, 1, 1)));
        observe(vec_u64_extend_from_range(&u, range_make(5, 5, 1)));
    }

    /* The lifetime helpers have no conditions in this configuration; they
     * are referenced so the build does not warn. */
    (void)vec_int_lifetime_open_;
    (void)vec_int_lifetime_close_;
    (void)vec_u64_lifetime_open_;
    (void)vec_u64_lifetime_close_;
    return 0;
}
