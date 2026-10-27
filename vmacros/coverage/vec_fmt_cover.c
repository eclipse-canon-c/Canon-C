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

/* vmacros/coverage/vec_fmt_cover.c                                [MCDC-018]
 * ============================================================================
 * MC/DC cover translation unit for data/vec/vec_fmt.h.
 *
 * WHY THIS FILE EXISTS: vec_fmt.h's conditions live in macro bodies and are
 * stamped to the instantiation site; test/data/vec_fmt_test.c's site is
 * removed by the coverage job's filter on test paths (VERIFY-041's lesson,
 * MCDC-017). This TU instantiates DEFINE_VEC_FMT(int) outside test/.
 *
 * NOT A CTEST. Built only under -DENABLE_COVERAGE_TUS=ON and run directly by
 * the coverage job. Flags as the WP run: -DCANON_NO_REQUIRE -DNDEBUG.
 *
 * PREDICTION (MCDC-018, VERIFY-042, before this file was written): 26 / 26.
 * The format form has 7 conditions (the 3-way guard, n == 0, !sb->data,
 * the loop, the append's failure); the callback form 6 (no buffer test).
 * The helpers below carry no condition, so they add none.
 * ============================================================================ */

#define CANON_CONTRACT_IMPL

#include "data/vec/vec_defn.h"
#include "data/vec/vec_fmt.h"

DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_FMT(static inline, int)

static volatile int g_sink;

static bool cb_print(StringBuf* sb, int x)
{
    return stringbuf_printf(sb, "[%d]", x);
}

static bool cb_false(StringBuf* sb, int x)
{
    (void)sb;
    (void)x;
    return false;
}

int main(void)
{
    int buf[3] = { 1, 1000000, 2 };
    vec_int v = { .items = buf, .len = 3, .capacity = 3 };
    vec_int e = { .items = buf, .len = 0, .capacity = 3 };
    char big[64];
    char small[8];
    StringBuf sb;
    StringBuf z = { 0 };      /* the zero state: no buffer */

    /* ── to_stringbuf ───────────────────────────────────────────────────── */
    stringbuf_init_buffer(&sb, big, sizeof big);
    g_sink = (int)vec_int_to_stringbuf(NULL, &sb, "%d");   /* !v true      */
    g_sink = (int)vec_int_to_stringbuf(&v, NULL, "%d");    /* !sb true     */
    g_sink = (int)vec_int_to_stringbuf(&v, &sb, NULL);     /* !fmt true    */
    g_sink = (int)vec_int_to_stringbuf(&e, &sb, "%d");     /* n == 0 true  */
    g_sink = (int)vec_int_to_stringbuf(&v, &z, "%d");      /* !data true   */
    g_sink = (int)vec_int_to_stringbuf(&v, &sb, "%d ");    /* all appended */
    stringbuf_init_buffer(&sb, small, sizeof small);
    g_sink = (int)vec_int_to_stringbuf(&v, &sb, "%d ");    /* append fails */

    /* ── to_stringbuf_cb ────────────────────────────────────────────────── */
    stringbuf_init_buffer(&sb, big, sizeof big);
    g_sink = (int)vec_int_to_stringbuf_cb(NULL, &sb, cb_print);  /* !v   */
    g_sink = (int)vec_int_to_stringbuf_cb(&v, NULL, cb_print);   /* !sb  */
    g_sink = (int)vec_int_to_stringbuf_cb(&v, &sb, NULL);        /* !cb  */
    g_sink = (int)vec_int_to_stringbuf_cb(&e, &sb, cb_print);    /* n==0 */
    g_sink = (int)vec_int_to_stringbuf_cb(&v, &sb, cb_print);    /* all  */
    g_sink = (int)vec_int_to_stringbuf_cb(&v, &sb, cb_false);    /* false */

    (void)vec_int_lifetime_open_;
    (void)vec_int_lifetime_close_;
    return 0;
}
