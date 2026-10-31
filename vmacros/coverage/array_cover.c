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

/* vmacros/coverage/array_cover.c                                 [MCDC-020]
 *
 * MC/DC for array.h's own conditions. They are macro-body conditions, which
 * GCC stamps to the instantiation line, so they are measured here and not in
 * test/data/array_test.c (filtered). Option is declared here and defined in
 * test/support/array_cover_option.c, so that this file's row holds array.h's
 * conditions only (DEFINE_SLICE's one function array.h calls, slice_int_from,
 * has none under CANON_NO_REQUIRE). Each call below is commented with the
 * outcomes it takes. The four outcomes not taken are checked_mul's failure at
 * from_ptr, size_bytes, as_bytes and as_cbytes, dead: N * sizeof(int) is the
 * size of an object that exists. Flags as frama-c-array's: CANON_NO_REQUIRE,
 * NDEBUG.
 */
#define CANON_CONTRACT_IMPL

#include "semantics/option/option_decl.h"
#include "core/slice.h"
#include "data/array.h"

DECLARE_OPTION_ALL(extern, int)
DEFINE_SLICE(int)
DEFINE_ARRAY(int, 8)

static volatile int g_sink;

int main(void)
{
    static const int src[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    array_int_8 a = array_int_8_zero();
    array_int_8 b = array_int_8_fill(3);              /* loop: both             */
    int out = 0;

    /* from_ptr: !src / count == 0 each true and false; count < N both. */
    b = array_int_8_from_ptr(NULL, 4);
    b = array_int_8_from_ptr(src, 0);
    b = array_int_8_from_ptr(src, 4);                 /* count < N true         */
    b = array_int_8_from_ptr(src, 10);                /* count < N false        */
    g_sink = (int)array_int_8_size_bytes() + (int)array_int_8_len();

    /* get: each of !a, !out, i >= N decides once; then all false. */
    g_sink = (int)array_int_8_get(NULL, 0, &out);
    g_sink = (int)array_int_8_get(&a, 0, NULL);
    g_sink = (int)array_int_8_get(&a, 8, &out);
    g_sink = (int)array_int_8_get(&a, 0, &out);
    g_sink = array_int_8_get_option(NULL, 0).has_value;
    g_sink = array_int_8_get_option(&a, 8).has_value;
    g_sink = array_int_8_get_option(&a, 1).has_value;
    g_sink = array_int_8_get_unchecked(&a, 2);

    g_sink = (int)array_int_8_set(NULL, 0, 1);
    g_sink = (int)array_int_8_set(&a, 8, 1);
    g_sink = (int)array_int_8_set(&a, 0, 1);
    g_sink = (array_int_8_at(NULL, 0) != NULL);
    g_sink = (array_int_8_at(&a, 8) != NULL);
    g_sink = (array_int_8_at(&a, 0) != NULL);
    g_sink = *array_int_8_first(&a) + *array_int_8_last(&a);
    g_sink = array_int_8_first_option(NULL).has_value + array_int_8_first_option(&a).has_value;
    g_sink = array_int_8_last_option(NULL).has_value + array_int_8_last_option(&a).has_value;

    array_int_8_fill_all(NULL, 2);
    array_int_8_fill_all(&b, 2);                      /* !a false; loop: both   */
    array_int_8_copy_from(&a, &b);
    array_int_8_copy_from(&a, &a);                    /* A1: dst == src         */

    /* equal: !a, !b each decides once; a == b both. */
    g_sink = (int)array_int_8_equal(NULL, &b);
    g_sink = (int)array_int_8_equal(&a, NULL);
    g_sink = (int)array_int_8_equal(&a, &a);
    g_sink = (int)array_int_8_equal(&a, &b);

    g_sink = (int)array_int_8_as_slice(&a).len;
    g_sink = (int)array_int_8_as_bytes(&a).len + (int)array_int_8_as_cbytes(&a).len;
    return 0;
}
