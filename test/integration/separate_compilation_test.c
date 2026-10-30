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

/**
 * @file separate_compilation_test.c
 * @brief The separate-compilation path of vec, stack, deque and queue
 *
 * VERIFY-049 S1: DECLARE_VEC and DECLARE_DEQUE did not compile at all
 * (`borrowed((type)*)` expands to `(type)*`), and no test used any DECLARE_*
 * container macro, so the path every container header documents for
 * multi-file programs had never been built. This test is that path: the
 * declarations come from separate_compilation_api.h, the definitions from
 * separate_compilation_defs.c, a second translation unit linked in, and each
 * container is used through the declarations alone.
 */
#define CANON_CONTRACT_IMPL
#include "separate_compilation_api.h"

#include <stdio.h>

static int g_failures = 0;
#define EXPECT(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_failures++; } } while (0)

static void test_vec(void) {
    SepTask buf[3];
    vec_SepTask v = vec_SepTask_init(buf, 3);
    SepTask a = {1};
    SepTask b = {2};
    EXPECT(result__Bool_Error_is_ok(vec_SepTask_push(&v, a)));
    EXPECT(vec_SepTask_try_push(&v, b));
    EXPECT(vec_SepTask_len(&v) == 2);
    option_SepTask o = vec_SepTask_pop_option(&v);
    EXPECT(option_SepTask_is_some(o) && o.value.id == 2);   /* option's own definition, linked */
}

static void test_stack(void) {
    SepTask buf[2];
    stack_SepTask s;
    stack_SepTask_init(&s, buf, 2);
    SepTask a = {10};
    SepTask b = {20};
    SepTask c = {30};
    EXPECT(result__Bool_Error_is_ok(stack_SepTask_push(&s, a)));
    EXPECT(stack_SepTask_try_push(&s, b));
    EXPECT(!stack_SepTask_try_push(&s, c));   /* full */
    option_SepTask top = stack_SepTask_peek_option(&s);
    EXPECT(top.has_value && top.value.id == 20);
    SepTask out = {0};
    EXPECT(result__Bool_Error_is_ok(stack_SepTask_pop(&s, &out)) && out.id == 20);
    EXPECT(stack_SepTask_len(&s) == 1);
}

static void test_queue(void) {
    SepTask buf[2];
    queue_SepTask q;
    queue_SepTask_init(&q, buf, 2);
    SepTask a = {7};
    SepTask b = {8};
    EXPECT(result__Bool_Error_is_ok(queue_SepTask_enqueue(&q, a)));
    EXPECT(result__Bool_Error_is_ok(queue_SepTask_enqueue(&q, b)));
    option_SepTask first = queue_SepTask_dequeue_option(&q);
    EXPECT(first.has_value && first.value.id == 7);   /* FIFO */
    EXPECT(queue_SepTask_len(&q) == 1);
}

int main(void) {
    test_vec();
    test_stack();
    test_queue();
    if (g_failures) { printf("FAIL  separate_compilation_test  (%d failure(s))\n", g_failures); return 1; }
    printf("OK  separate_compilation_test  (all assertions passed)\n");
    return 0;
}
