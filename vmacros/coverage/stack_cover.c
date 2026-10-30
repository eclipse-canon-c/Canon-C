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

/* vmacros/coverage/stack_cover.c                                 [MCDC-019]
 *
 * MC/DC for stack.h's own conditions. They are macro-body conditions, which
 * GCC stamps to the instantiation line, so they are measured here and not in
 * test/data/stack_test.c (filtered). Vec is declared here and defined in
 * test/support/stack_cover_vec.c, so that this file's row holds stack.h's
 * conditions only: peek's two decisions and peek_option's one, six
 * outcomes, of which peek's `!last` true is dead (a non-empty stack has a
 * last element). Flags as
 * frama-c-stack's: CANON_NO_REQUIRE, NDEBUG.
 */
#define CANON_CONTRACT_IMPL

#include "semantics/option/option_decl.h"
#include "data/vec/vec_decl.h"
#include "data/stack.h"

DECLARE_OPTION_ALL(extern, int)
DECLARE_VEC(int)
DEFINE_STACK(static inline, int)

static volatile int g_sink;

int main(void)
{
    int buf[2];
    int out = 0;
    stack_int s;
    stack_int_init(&s, buf, 2);

    g_sink = (int)stack_int_peek(&s, &out);           /* empty: is_empty true   */
    g_sink = stack_int_peek_option(&s).has_value;     /* empty: peek false      */
    g_sink = (int)stack_int_try_push(&s, 5);
    g_sink = (int)stack_int_peek(&s, &out);           /* top: both false        */
    g_sink = stack_int_peek_option(&s).has_value;     /* top: peek true         */
    g_sink = (int)result__Bool_Error_is_ok(stack_int_push(&s, 6));
    stack_int_clear(&s);
    stack_int_push_unchecked(&s, 7);
    g_sink = (int)result__Bool_Error_is_ok(stack_int_pop(&s, &out));
    g_sink = stack_int_pop_option(&s).has_value;
    g_sink = (int)(stack_int_len(&s) + stack_int_capacity(&s) + stack_int_remaining(&s));
    g_sink = (int)(stack_int_is_empty(&s) + stack_int_is_full(&s));
    return 0;
}
