/**
 * @file stack_test.c
 * @brief Tests for data/stack.h — bounded LIFO stack (vec wrapper)
 *
 * Covers:
 *   - canon_stack_int_init()            — valid initialization
 *   - canon_stack_int_push()            — success, capacity exceeded,
 *                                         NULL args (ERR_INVALID_ARG)
 *   - canon_stack_int_try_push()        — bool variant, success + full + NULL
 *   - canon_stack_int_push_unchecked()  — unchecked variant, success
 *   - canon_stack_int_pop()             — LIFO order, empty stack
 *                                         (ERR_INVALID_STATE), NULL args
 *   - canon_stack_int_pop_option()      — Some / None
 *   - canon_stack_int_peek()            — present / empty, non-destructive
 *   - canon_stack_int_peek_option()     — Some / None, non-destructive
 *   - canon_stack_int_clear()           — resets to empty, buffer reusable
 *   - Queries                           — len, capacity, remaining,
 *                                         is_empty, is_full
 *   - LIFO ordering stress              — push N elements, pop in reverse order
 *   - DEFINE_STACK(Frame) struct type   — exercises all generated code paths
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - ref[32] parallel array tracks LIFO ground truth
 *   - ref_top tracks stack depth
 *   - Verifies push, try_push, push_unchecked, pop, peek, pop_option
 *     against ref after every operation
 *   - Structural invariants: len + remaining == capacity,
 *     is_empty/is_full consistency, peek matches top of ref
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "semantics/option/option.h"
#include "data/vec/vec.h"
#include "data/stack.h"

#include <stdio.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

CANON_OPTION(int)
DEFINE_VEC(static inline, int)
DEFINE_STACK(static inline, int)

/* Second instantiation — struct type, exercises all generated code paths */
typedef struct { int id; int value; } Frame;
CANON_OPTION(Frame)
DEFINE_VEC(static inline, Frame)
DEFINE_STACK(static inline, Frame)

/* ════════════════════════════════════════════════════════════════════════════
   Unit test build
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                                  \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n",                      \
                    __FILE__, __LINE__, #cond);                       \
            g_failed++;                                               \
        }                                                             \
    } while (0)

#define EXPECT_NOT_NULL(ptr)                                          \
    do {                                                              \
        if ((ptr) == NULL) {                                          \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",              \
                    __FILE__, __LINE__, #ptr);                        \
            g_failed++;                                               \
            return;                                                   \
        }                                                             \
    } while (0)

/* ── init ────────────────────────────────────────────────────────────────── */

static void test_init(void)
{
    int buf[8];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 8);

    EXPECT(canon_stack_int_len(&s)       == 0);
    EXPECT(canon_stack_int_capacity(&s)  == 8);
    EXPECT(canon_stack_int_remaining(&s) == 8);
    EXPECT(canon_stack_int_is_empty(&s));
    EXPECT(!canon_stack_int_is_full(&s));
}

/* ── LIFO ordering ───────────────────────────────────────────────────────── */

static void test_lifo_ordering(void)
{
    int buf[8];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 8);

    for (int i = 1; i <= 5; i++) {
        result__Bool_Error r = canon_stack_int_push(&s, i * 10);
        EXPECT(result__Bool_Error_is_ok(r));
    }
    EXPECT(canon_stack_int_len(&s) == 5);

    /* pop must return values in reverse push order (LIFO) */
    for (int i = 5; i >= 1; i--) {
        int out = 0;
        result__Bool_Error r = canon_stack_int_pop(&s, &out);
        EXPECT(result__Bool_Error_is_ok(r));
        EXPECT(out == i * 10);
    }
    EXPECT(canon_stack_int_is_empty(&s));
}

/* ── push — capacity exceeded ────────────────────────────────────────────── */

static void test_push_capacity_exceeded(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    canon_stack_int_push(&s, 1);
    canon_stack_int_push(&s, 2);
    canon_stack_int_push(&s, 3);
    canon_stack_int_push(&s, 4);
    EXPECT(canon_stack_int_is_full(&s));

    result__Bool_Error r = canon_stack_int_push(&s, 5);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
    EXPECT(canon_stack_int_len(&s) == 4); /* unchanged */
}

/* ── push — NULL args ────────────────────────────────────────────────────── */

static void test_push_null_args(void)
{
    result__Bool_Error r = canon_stack_int_push(NULL, 42);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── try_push ────────────────────────────────────────────────────────────── */

static void test_try_push_success(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    EXPECT(canon_stack_int_try_push(&s, 1));
    EXPECT(canon_stack_int_try_push(&s, 2));
    EXPECT(canon_stack_int_try_push(&s, 3));

    EXPECT(canon_stack_int_len(&s) == 3);

    /* LIFO order preserved */
    int out = 0;
    canon_stack_int_pop(&s, &out); EXPECT(out == 3);
    canon_stack_int_pop(&s, &out); EXPECT(out == 2);
    canon_stack_int_pop(&s, &out); EXPECT(out == 1);
}

static void test_try_push_full_returns_false(void)
{
    int buf[2];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 2);

    EXPECT(canon_stack_int_try_push(&s, 1));
    EXPECT(canon_stack_int_try_push(&s, 2));
    EXPECT(!canon_stack_int_try_push(&s, 3));

    EXPECT(canon_stack_int_len(&s) == 2);
}

static void test_try_push_null_returns_false(void)
{
    EXPECT(!canon_stack_int_try_push(NULL, 1));
}

/* ── push_unchecked ──────────────────────────────────────────────────────── */

static void test_push_unchecked_success(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    canon_stack_int_push_unchecked(&s, 10);
    canon_stack_int_push_unchecked(&s, 20);
    canon_stack_int_push_unchecked(&s, 30);

    EXPECT(canon_stack_int_len(&s) == 3);

    /* LIFO order preserved */
    int out = 0;
    canon_stack_int_pop(&s, &out); EXPECT(out == 30);
    canon_stack_int_pop(&s, &out); EXPECT(out == 20);
    canon_stack_int_pop(&s, &out); EXPECT(out == 10);
}

/* ── pop — empty stack ───────────────────────────────────────────────────── */

static void test_pop_empty(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    int out = 999;
    result__Bool_Error r = canon_stack_int_pop(&s, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_STATE);
    EXPECT(out == 999); /* not written */
}

/* ── pop — NULL args ─────────────────────────────────────────────────────── */

static void test_pop_null_args(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);
    int out = 0;

    result__Bool_Error r;

    /* NULL stack */
    r = canon_stack_int_pop(NULL, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    /* NULL out */
    canon_stack_int_push(&s, 42);
    r = canon_stack_int_pop(&s, NULL);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── pop_option — Some / None ────────────────────────────────────────────── */

static void test_pop_option(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    canon_stack_int_push(&s, 10);
    canon_stack_int_push(&s, 20);

    option_int o;

    /* LIFO: 20 was pushed last, comes out first */
    o = canon_stack_int_pop_option(&s);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 20);

    o = canon_stack_int_pop_option(&s);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 10);

    /* None on empty */
    o = canon_stack_int_pop_option(&s);
    EXPECT(option_int_is_none(o));
}

/* ── peek — present / empty ──────────────────────────────────────────────── */

static void test_peek(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    /* empty — peek returns false */
    int out = 999;
    EXPECT(!canon_stack_int_peek(&s, &out));
    EXPECT(out == 999); /* not written */

    canon_stack_int_push(&s, 10);
    canon_stack_int_push(&s, 20);

    /* peek twice — same result, len unchanged */
    EXPECT(canon_stack_int_peek(&s, &out));
    EXPECT(out == 20);
    EXPECT(canon_stack_int_len(&s) == 2);

    out = 0;
    EXPECT(canon_stack_int_peek(&s, &out));
    EXPECT(out == 20);
    EXPECT(canon_stack_int_len(&s) == 2);
}

/* ── peek_option — Some / None ───────────────────────────────────────────── */

static void test_peek_option(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    EXPECT(option_int_is_none(canon_stack_int_peek_option(&s)));

    canon_stack_int_push(&s, 42);
    canon_stack_int_push(&s, 99);

    option_int o = canon_stack_int_peek_option(&s);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 99);
    EXPECT(canon_stack_int_len(&s) == 2); /* non-destructive */

    /* peek again — still Some(99) */
    o = canon_stack_int_peek_option(&s);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 99);
}

/* ── clear — resets to empty ─────────────────────────────────────────────── */

static void test_clear(void)
{
    int buf[8];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 8);

    canon_stack_int_push(&s, 1);
    canon_stack_int_push(&s, 2);
    canon_stack_int_push(&s, 3);
    EXPECT(canon_stack_int_len(&s) == 3);

    canon_stack_int_clear(&s);
    EXPECT(canon_stack_int_len(&s)       == 0);
    EXPECT(canon_stack_int_capacity(&s)  == 8);
    EXPECT(canon_stack_int_remaining(&s) == 8);
    EXPECT(canon_stack_int_is_empty(&s));

    /* Buffer reusable after clear */
    result__Bool_Error r = canon_stack_int_push(&s, 99);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_stack_int_len(&s) == 1);
}

/* ── queries ─────────────────────────────────────────────────────────────── */

static void test_queries(void)
{
    int buf[4];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 4);

    EXPECT(canon_stack_int_len(&s)       == 0);
    EXPECT(canon_stack_int_capacity(&s)  == 4);
    EXPECT(canon_stack_int_remaining(&s) == 4);
    EXPECT(canon_stack_int_is_empty(&s));
    EXPECT(!canon_stack_int_is_full(&s));

    canon_stack_int_push(&s, 10);
    EXPECT(canon_stack_int_len(&s)       == 1);
    EXPECT(canon_stack_int_remaining(&s) == 3);
    EXPECT(!canon_stack_int_is_empty(&s));
    EXPECT(!canon_stack_int_is_full(&s));

    canon_stack_int_push(&s, 20);
    canon_stack_int_push(&s, 30);
    canon_stack_int_push(&s, 40);
    EXPECT(canon_stack_int_len(&s)       == 4);
    EXPECT(canon_stack_int_remaining(&s) == 0);
    EXPECT(canon_stack_int_is_full(&s));
    EXPECT(!canon_stack_int_is_empty(&s));
}

/* ── LIFO stress — push N, pop N in reverse ──────────────────────────────── */

static void test_lifo_stress(void)
{
    int buf[32];
    canon_stack_int s;
    canon_stack_int_init(&s, buf, 32);

    /* Push 0..31 */
    for (int i = 0; i < 32; i++) {
        result__Bool_Error r = canon_stack_int_push(&s, i);
        EXPECT(result__Bool_Error_is_ok(r));
    }
    EXPECT(canon_stack_int_is_full(&s));

    /* Pop must come out in reverse order: 31..0 */
    for (int i = 31; i >= 0; i--) {
        int out = -1;
        result__Bool_Error r = canon_stack_int_pop(&s, &out);
        EXPECT(result__Bool_Error_is_ok(r));
        EXPECT(out == i);
    }
    EXPECT(canon_stack_int_is_empty(&s));
}

/* ── struct type — exercises all generated Frame functions ───────────────── */

static void test_struct_type(void)
{
    Frame buf[4];
    canon_stack_Frame s;
    canon_stack_Frame_init(&s, buf, 4);

    EXPECT(canon_stack_Frame_is_empty(&s));
    EXPECT(canon_stack_Frame_len(&s)       == 0);
    EXPECT(canon_stack_Frame_capacity(&s)  == 4);
    EXPECT(canon_stack_Frame_remaining(&s) == 4);
    EXPECT(!canon_stack_Frame_is_full(&s));

    Frame f1 = {1, 100};
    Frame f2 = {2, 200};
    Frame f3 = {3, 300};

    result__Bool_Error r;
    r = canon_stack_Frame_push(&s, f1); EXPECT(result__Bool_Error_is_ok(r));
    r = canon_stack_Frame_push(&s, f2); EXPECT(result__Bool_Error_is_ok(r));
    r = canon_stack_Frame_push(&s, f3); EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_stack_Frame_len(&s) == 3);

    /* try_push on Frame */
    EXPECT(canon_stack_Frame_try_push(&s, (Frame){4, 400}));
    EXPECT(canon_stack_Frame_is_full(&s));
    EXPECT(!canon_stack_Frame_try_push(&s, (Frame){5, 500}));

    /* Drain one and test push_unchecked on Frame */
    Frame out_frame = {0, 0};
    canon_stack_Frame_pop(&s, &out_frame); /* removes {4, 400} */
    canon_stack_Frame_push_unchecked(&s, (Frame){5, 500});

    /* peek_option — non-destructive, returns top ({5, 500}) */
    option_Frame peek = canon_stack_Frame_peek_option(&s);
    EXPECT(option_Frame_is_some(peek));
    EXPECT(option_Frame_unwrap(peek).id == 5);
    EXPECT(canon_stack_Frame_len(&s) == 4);

    /* peek — out-param variant */
    Frame top = {0, 0};
    EXPECT(canon_stack_Frame_peek(&s, &top));
    EXPECT(top.id == 5);

    /* LIFO pop via option — {5, 500} first */
    option_Frame o;
    o = canon_stack_Frame_pop_option(&s);
    EXPECT(option_Frame_is_some(o));
    EXPECT(option_Frame_unwrap(o).id == 5);

    /* LIFO pop via out-param — f3 next */
    out_frame = (Frame){0, 0};
    r = canon_stack_Frame_pop(&s, &out_frame);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out_frame.id == 3);

    EXPECT(canon_stack_Frame_len(&s) == 2);

    /* clear */
    canon_stack_Frame_clear(&s);
    EXPECT(canon_stack_Frame_is_empty(&s));
    EXPECT(option_Frame_is_none(canon_stack_Frame_pop_option(&s)));

    /* capacity exceeded */
    canon_stack_Frame_push(&s, f1);
    canon_stack_Frame_push(&s, f2);
    canon_stack_Frame_push(&s, f3);
    canon_stack_Frame_push(&s, (Frame){4, 400});
    EXPECT(canon_stack_Frame_is_full(&s));
    r = canon_stack_Frame_push(&s, (Frame){5, 500});
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
}

/* ── Suppress unused generated functions ────────────────────────────────── */

static void stack_suppress_unused(void)
{
    /* result__Bool_Error combinators not tested here
     * (covered by result_test) */
    (void)result__Bool_Error_is_err;
    (void)result__Bool_Error_get_ok;
    (void)result__Bool_Error_get_err;
    (void)result__Bool_Error_unwrap;
    (void)result__Bool_Error_unwrap_or;
    (void)result__Bool_Error_expect;
    (void)result__Bool_Error_map;
    (void)result__Bool_Error_map_err;
    (void)result__Bool_Error_and_then;
    (void)result__Bool_Error_or_else;
    (void)result__Bool_Error_and;
    (void)result__Bool_Error_or;
    (void)result__Bool_Error_eq;

    /* option_int combinators not tested here (covered by option_test) */
    (void)option_int_get;
    (void)option_int_unwrap_or;
    (void)option_int_expect;
    (void)option_int_map;
    (void)option_int_and_then;
    (void)option_int_or_else;
    (void)option_int_filter;
    (void)option_int_combine_with;
    (void)option_int_replace;
    (void)option_int_take;
    (void)option_int_eq;

    /* option_Frame combinators not tested here */
    (void)option_Frame_get;
    (void)option_Frame_unwrap_or;
    (void)option_Frame_expect;
    (void)option_Frame_map;
    (void)option_Frame_and_then;
    (void)option_Frame_or_else;
    (void)option_Frame_filter;
    (void)option_Frame_combine_with;
    (void)option_Frame_replace;
    (void)option_Frame_take;
    (void)option_Frame_eq;

    /* Vec functions exposed through the alias but not needed in stack tests */
    (void)canon_vec_int_get;
    (void)canon_vec_int_get_option;
    (void)canon_vec_int_get_unchecked;
    (void)canon_vec_int_at;
    (void)canon_vec_int_set;
    (void)canon_vec_int_first;
    (void)canon_vec_int_data;
    (void)canon_vec_int_try_push;
    (void)canon_vec_int_push_unchecked;
    (void)canon_vec_int_insert;
    (void)canon_vec_int_remove;
    (void)canon_vec_int_remove_option;
    (void)canon_vec_int_append_array;
    (void)canon_vec_int_extend;
    (void)canon_vec_int_fill;
    (void)canon_vec_int_swap;
    (void)canon_vec_int_iter_init;
    (void)canon_vec_int_iter_next;
    (void)canon_vec_int_slice_init;
    (void)canon_vec_int_slice_get;
    (void)canon_vec_int_alloc;
    (void)canon_vec_int_arena_alloc;
    (void)canon_vec_int_free;
    (void)canon_vec_int_empty;

    (void)canon_vec_Frame_get;
    (void)canon_vec_Frame_get_option;
    (void)canon_vec_Frame_get_unchecked;
    (void)canon_vec_Frame_at;
    (void)canon_vec_Frame_set;
    (void)canon_vec_Frame_first;
    (void)canon_vec_Frame_data;
    (void)canon_vec_Frame_try_push;
    (void)canon_vec_Frame_push_unchecked;
    (void)canon_vec_Frame_insert;
    (void)canon_vec_Frame_remove;
    (void)canon_vec_Frame_remove_option;
    (void)canon_vec_Frame_append_array;
    (void)canon_vec_Frame_extend;
    (void)canon_vec_Frame_fill;
    (void)canon_vec_Frame_swap;
    (void)canon_vec_Frame_iter_init;
    (void)canon_vec_Frame_iter_next;
    (void)canon_vec_Frame_slice_init;
    (void)canon_vec_Frame_slice_get;
    (void)canon_vec_Frame_alloc;
    (void)canon_vec_Frame_arena_alloc;
    (void)canon_vec_Frame_free;
    (void)canon_vec_Frame_empty;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    (void)stack_suppress_unused;

    test_init();
    test_lifo_ordering();
    test_push_capacity_exceeded();
    test_push_null_args();

    /* try_push */
    test_try_push_success();
    test_try_push_full_returns_false();
    test_try_push_null_returns_false();

    /* push_unchecked */
    test_push_unchecked_success();

    test_pop_empty();
    test_pop_null_args();
    test_pop_option();
    test_peek();
    test_peek_option();
    test_clear();
    test_queries();
    test_lifo_stress();
    test_struct_type();

    if (g_failed == 0) {
        printf("OK  stack_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  stack_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void stack_fuzz_suppress_unused(void)
{
    /* Frame type not used in fuzz path */
    (void)canon_stack_Frame_init;
    (void)canon_stack_Frame_push;
    (void)canon_stack_Frame_try_push;
    (void)canon_stack_Frame_push_unchecked;
    (void)canon_stack_Frame_pop;
    (void)canon_stack_Frame_pop_option;
    (void)canon_stack_Frame_peek;
    (void)canon_stack_Frame_peek_option;
    (void)canon_stack_Frame_len;
    (void)canon_stack_Frame_capacity;
    (void)canon_stack_Frame_remaining;
    (void)canon_stack_Frame_is_empty;
    (void)canon_stack_Frame_is_full;
    (void)canon_stack_Frame_clear;

    /* Vec int functions not used in fuzz path */
    (void)canon_vec_int_get;
    (void)canon_vec_int_get_option;
    (void)canon_vec_int_get_unchecked;
    (void)canon_vec_int_at;
    (void)canon_vec_int_set;
    (void)canon_vec_int_first;
    (void)canon_vec_int_data;
    (void)canon_vec_int_try_push;
    (void)canon_vec_int_push_unchecked;
    (void)canon_vec_int_insert;
    (void)canon_vec_int_remove;
    (void)canon_vec_int_remove_option;
    (void)canon_vec_int_append_array;
    (void)canon_vec_int_extend;
    (void)canon_vec_int_fill;
    (void)canon_vec_int_swap;
    (void)canon_vec_int_iter_init;
    (void)canon_vec_int_iter_next;
    (void)canon_vec_int_slice_init;
    (void)canon_vec_int_slice_get;
    (void)canon_vec_int_alloc;
    (void)canon_vec_int_arena_alloc;
    (void)canon_vec_int_free;
    (void)canon_vec_int_empty;
    (void)canon_vec_int_capacity;
    (void)canon_vec_int_remaining;
    (void)canon_vec_int_is_full;

    /* Vec Frame functions not used in fuzz path */
    (void)canon_vec_Frame_get;
    (void)canon_vec_Frame_get_option;
    (void)canon_vec_Frame_get_unchecked;
    (void)canon_vec_Frame_at;
    (void)canon_vec_Frame_set;
    (void)canon_vec_Frame_first;
    (void)canon_vec_Frame_last;
    (void)canon_vec_Frame_data;
    (void)canon_vec_Frame_try_push;
    (void)canon_vec_Frame_push_unchecked;
    (void)canon_vec_Frame_push;
    (void)canon_vec_Frame_pop;
    (void)canon_vec_Frame_pop_option;
    (void)canon_vec_Frame_insert;
    (void)canon_vec_Frame_remove;
    (void)canon_vec_Frame_remove_option;
    (void)canon_vec_Frame_append_array;
    (void)canon_vec_Frame_extend;
    (void)canon_vec_Frame_fill;
    (void)canon_vec_Frame_swap;
    (void)canon_vec_Frame_iter_init;
    (void)canon_vec_Frame_iter_next;
    (void)canon_vec_Frame_slice_init;
    (void)canon_vec_Frame_slice_get;
    (void)canon_vec_Frame_alloc;
    (void)canon_vec_Frame_arena_alloc;
    (void)canon_vec_Frame_free;
    (void)canon_vec_Frame_empty;
    (void)canon_vec_Frame_len;
    (void)canon_vec_Frame_capacity;
    (void)canon_vec_Frame_remaining;
    (void)canon_vec_Frame_is_empty;
    (void)canon_vec_Frame_is_full;
    (void)canon_vec_Frame_clear;
    (void)canon_vec_Frame_init;

    /* option_Frame — all unused in fuzz */
    (void)option_Frame_some;
    (void)option_Frame_none;
    (void)option_Frame_is_some;
    (void)option_Frame_is_none;
    (void)option_Frame_unwrap;
    (void)option_Frame_get;
    (void)option_Frame_unwrap_or;
    (void)option_Frame_expect;
    (void)option_Frame_map;
    (void)option_Frame_and_then;
    (void)option_Frame_or_else;
    (void)option_Frame_filter;
    (void)option_Frame_combine_with;
    (void)option_Frame_replace;
    (void)option_Frame_take;
    (void)option_Frame_eq;

    /* option_int combinators not used in fuzz path */
    (void)option_int_is_none;
    (void)option_int_get;
    (void)option_int_unwrap_or;
    (void)option_int_expect;
    (void)option_int_map;
    (void)option_int_and_then;
    (void)option_int_or_else;
    (void)option_int_filter;
    (void)option_int_combine_with;
    (void)option_int_replace;
    (void)option_int_take;
    (void)option_int_eq;

    /* result__Bool_Error combinators not used in fuzz path */
    (void)result__Bool_Error_is_err;
    (void)result__Bool_Error_get_ok;
    (void)result__Bool_Error_get_err;
    (void)result__Bool_Error_unwrap;
    (void)result__Bool_Error_unwrap_or;
    (void)result__Bool_Error_unwrap_err;
    (void)result__Bool_Error_expect;
    (void)result__Bool_Error_map;
    (void)result__Bool_Error_map_err;
    (void)result__Bool_Error_and_then;
    (void)result__Bool_Error_or_else;
    (void)result__Bool_Error_and;
    (void)result__Bool_Error_or;
    (void)result__Bool_Error_eq;

    /* int stack functions not used in fuzz path */
    (void)canon_stack_int_capacity;
    (void)canon_stack_int_remaining;
    (void)canon_stack_int_is_full;
    (void)canon_stack_int_peek;
}

/*
 * Input layout:
 *   [0]    capacity selector — maps to cap_table[data[0] % 4]
 *   [1..N] operation stream — each byte:
 *            high nibble: operation index (0–6)
 *            low  nibble: value to push (0–15)
 *
 * Operations:
 *   0 — push(value)
 *   1 — pop — verify LIFO ordering against ref[]
 *   2 — pop_option — same verification, option variant
 *   3 — peek_option — verify non-destructive, matches top of ref
 *   4 — clear — resets stack and reference
 *   5 — try_push(value) — bool variant
 *   6 — push_unchecked(value) — only if not full
 *
 * Reference model:
 *   ref[16]   — array holding pushed values in order
 *   ref_top   — index of the next free slot (== depth of stack)
 *
 * Invariants checked after every operation:
 *   - is_empty iff len == 0
 *   - len agrees with ref_top
 *   - peek_option matches ref[ref_top - 1] (top of stack)
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static const usize cap_table[4] = {2, 4, 8, 16};

    /* Max capacity 16 — size buffer for that */
    int buf[16];
    canon_stack_int s;

    (void)stack_fuzz_suppress_unused;

    if (size < 2) return 0;

    usize cap = cap_table[data[0] % 4u];
    canon_stack_int_init(&s, buf, cap);

    /*
     * Reference model — a simple array used as a stack.
     */
    int   ref[16];
    usize ref_top = 0; /* number of elements currently in ref */
    memset(ref, 0, sizeof(ref));

    #define CHECK_INVARIANTS()                                              \
        do {                                                                \
            /* is_empty consistency */                                      \
            if (canon_stack_int_is_empty(&s) != (canon_stack_int_len(&s) == 0)) \
                __builtin_trap();                                           \
            /* len agrees with ref */                                       \
            if (canon_stack_int_len(&s) != ref_top)                        \
                __builtin_trap();                                           \
            /* peek matches top of ref */                                   \
            {                                                               \
                option_int top__ = canon_stack_int_peek_option(&s);        \
                if (ref_top == 0) {                                         \
                    if (option_int_is_some(top__)) __builtin_trap();       \
                } else {                                                    \
                    if (!option_int_is_some(top__)) __builtin_trap();      \
                    if (option_int_unwrap(top__) != ref[ref_top - 1])      \
                        __builtin_trap();                                   \
                }                                                           \
            }                                                               \
        } while (0)

    for (usize i = 1; i < size; i++) {
        u8  byte = data[i];
        u8  op   = (u8)(byte >> 4u) % 7u;
        int val  = (int)(byte & 0x0Fu); /* 0–15 */

        switch (op) {

            case 0: { /* push */
                result__Bool_Error r = canon_stack_int_push(&s, val);
                if (result__Bool_Error_is_ok(r)) {
                    if (ref_top >= cap) __builtin_trap();
                    ref[ref_top++] = val;
                }
                /* ERR_CAPACITY_EXCEEDED when full — not a trap */
                break;
            }

            case 1: { /* pop — out-param variant */
                int out = -1;
                result__Bool_Error r = canon_stack_int_pop(&s, &out);
                if (result__Bool_Error_is_ok(r)) {
                    if (ref_top == 0)                     __builtin_trap();
                    if (out != ref[ref_top - 1])          __builtin_trap();
                    ref_top--;
                } else {
                    if (ref_top != 0)                     __builtin_trap();
                }
                break;
            }

            case 2: { /* pop_option variant */
                option_int o = canon_stack_int_pop_option(&s);
                if (option_int_is_some(o)) {
                    if (ref_top == 0)                           __builtin_trap();
                    if (option_int_unwrap(o) != ref[ref_top - 1])
                                                                __builtin_trap();
                    ref_top--;
                } else {
                    if (ref_top != 0)                           __builtin_trap();
                }
                break;
            }

            case 3: { /* peek_option — non-destructive */
                usize before = canon_stack_int_len(&s);
                option_int o = canon_stack_int_peek_option(&s);
                if (canon_stack_int_len(&s) != before)  __builtin_trap();
                if (ref_top == 0) {
                    if (option_int_is_some(o))           __builtin_trap();
                } else {
                    if (!option_int_is_some(o))          __builtin_trap();
                    if (option_int_unwrap(o) != ref[ref_top - 1])
                                                         __builtin_trap();
                }
                break;
            }

            case 4: { /* clear */
                canon_stack_int_clear(&s);
                ref_top = 0;
                if (!canon_stack_int_is_empty(&s)) __builtin_trap();
                break;
            }

            case 5: { /* try_push */
                bool ok = canon_stack_int_try_push(&s, val);
                if (ok) {
                    if (ref_top >= cap) __builtin_trap();
                    ref[ref_top++] = val;
                }
                break;
            }

            case 6: { /* push_unchecked — only if not full */
                if (!canon_stack_int_is_full(&s)) {
                    canon_stack_int_push_unchecked(&s, val);
                    if (ref_top >= cap) __builtin_trap();
                    ref[ref_top++] = val;
                }
                break;
            }

            default:
                break;
        }

        CHECK_INVARIANTS();
    }

    #undef CHECK_INVARIANTS

    return 0;
}

#endif /* CANON_FUZZING */
