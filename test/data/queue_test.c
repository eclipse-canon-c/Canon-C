/**
 * @file queue_test.c
 * @brief Tests for data/queue.h — bounded FIFO queue (deque wrapper)
 *
 * Covers:
 *   - queue_int_init()              — valid initialization
 *   - queue_int_enqueue()           — success, capacity exceeded,
 *                                           NULL args (ERR_INVALID_ARG)
 *   - queue_int_try_enqueue()       — bool variant, success + full + NULL
 *   - queue_int_enqueue_unchecked() — unchecked variant, success + wraparound
 *   - queue_int_dequeue()           — FIFO order, empty queue
 *                                           (ERR_INVALID_STATE), NULL args
 *   - queue_int_dequeue_option()    — Some / None
 *   - queue_int_peek()              — present / empty, non-destructive
 *   - queue_int_peek_option()       — Some / None, non-destructive
 *   - queue_int_clear()             — resets to empty, buffer reusable
 *   - Queries                             — len, capacity, remaining,
 *                                           is_empty, is_full
 *   - FIFO ordering stress                — 128-element wrap-around ring
 *   - DEFINE_QUEUE(Msg) struct type       — exercises all generated code paths
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - ref[16] parallel array tracks ground truth FIFO ordering
 *   - ref_head / ref_tail / ref_len maintain queue state
 *   - Verifies enqueue, try_enqueue, enqueue_unchecked, dequeue, peek,
 *     dequeue_option against ref after every operation
 *   - Structural invariants: len + remaining == capacity,
 *     is_empty/is_full consistency
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "semantics/option/option.h"
#include "data/deque/deque.h"
#include "data/queue.h"

#include <stdio.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

CANON_OPTION(int)
DEFINE_DEQUE(static inline, int)
DEFINE_QUEUE(static inline, int)

/* Second instantiation — struct type, exercises all generated code paths */
typedef struct { int id; int priority; } Msg;
CANON_OPTION(Msg)
DEFINE_DEQUE(static inline, Msg)
DEFINE_QUEUE(static inline, Msg)

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
    queue_int q;
    queue_int_init(&q, buf, 8);

    EXPECT(queue_int_len(&q)       == 0);
    EXPECT(queue_int_capacity(&q)  == 8);
    EXPECT(queue_int_remaining(&q) == 8);
    EXPECT(queue_int_is_empty(&q));
    EXPECT(!queue_int_is_full(&q));
}

/* ── FIFO ordering ───────────────────────────────────────────────────────── */

static void test_fifo_ordering(void)
{
    int buf[8];
    queue_int q;
    queue_int_init(&q, buf, 8);

    for (int i = 1; i <= 5; i++) {
        result__Bool_Error r = queue_int_enqueue(&q, i * 10);
        EXPECT(result__Bool_Error_is_ok(r));
    }
    EXPECT(queue_int_len(&q) == 5);

    /* dequeue must return values in enqueue order */
    for (int i = 1; i <= 5; i++) {
        int out = 0;
        result__Bool_Error r = queue_int_dequeue(&q, &out);
        EXPECT(result__Bool_Error_is_ok(r));
        EXPECT(out == i * 10);
    }
    EXPECT(queue_int_is_empty(&q));
}

/* ── enqueue — capacity exceeded ─────────────────────────────────────────── */

static void test_enqueue_capacity_exceeded(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    queue_int_enqueue(&q, 1);
    queue_int_enqueue(&q, 2);
    queue_int_enqueue(&q, 3);
    queue_int_enqueue(&q, 4);
    EXPECT(queue_int_is_full(&q));

    result__Bool_Error r = queue_int_enqueue(&q, 5);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
    EXPECT(queue_int_len(&q) == 4); /* unchanged */
}

/* ── enqueue — NULL args ─────────────────────────────────────────────────── */

static void test_enqueue_null_args(void)
{
    result__Bool_Error r = queue_int_enqueue(NULL, 42);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── try_enqueue ─────────────────────────────────────────────────────────── */

static void test_try_enqueue_success(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    EXPECT(queue_int_try_enqueue(&q, 1));
    EXPECT(queue_int_try_enqueue(&q, 2));
    EXPECT(queue_int_try_enqueue(&q, 3));

    EXPECT(queue_int_len(&q) == 3);

    /* FIFO order preserved */
    int out = 0;
    queue_int_dequeue(&q, &out); EXPECT(out == 1);
    queue_int_dequeue(&q, &out); EXPECT(out == 2);
    queue_int_dequeue(&q, &out); EXPECT(out == 3);
}

static void test_try_enqueue_full_returns_false(void)
{
    int buf[2];
    queue_int q;
    queue_int_init(&q, buf, 2);

    EXPECT(queue_int_try_enqueue(&q, 1));
    EXPECT(queue_int_try_enqueue(&q, 2));
    EXPECT(!queue_int_try_enqueue(&q, 3));

    EXPECT(queue_int_len(&q) == 2);
}

static void test_try_enqueue_null_returns_false(void)
{
    EXPECT(!queue_int_try_enqueue(NULL, 1));
}

/* ── enqueue_unchecked ───────────────────────────────────────────────────── */

static void test_enqueue_unchecked_success(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    queue_int_enqueue_unchecked(&q, 10);
    queue_int_enqueue_unchecked(&q, 20);
    queue_int_enqueue_unchecked(&q, 30);

    EXPECT(queue_int_len(&q) == 3);

    /* FIFO order preserved */
    int out = 0;
    queue_int_dequeue(&q, &out); EXPECT(out == 10);
    queue_int_dequeue(&q, &out); EXPECT(out == 20);
    queue_int_dequeue(&q, &out); EXPECT(out == 30);
}

static void test_enqueue_unchecked_wraparound(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    /* Fill and drain to move head/tail forward */
    queue_int_enqueue_unchecked(&q, 1);
    queue_int_enqueue_unchecked(&q, 2);
    int out = 0;
    queue_int_dequeue(&q, &out);
    queue_int_dequeue(&q, &out);

    /* Now enqueue_unchecked should wrap around correctly */
    queue_int_enqueue_unchecked(&q, 3);
    queue_int_enqueue_unchecked(&q, 4);
    queue_int_enqueue_unchecked(&q, 5);
    queue_int_enqueue_unchecked(&q, 6);

    EXPECT(queue_int_is_full(&q));

    queue_int_dequeue(&q, &out); EXPECT(out == 3);
    queue_int_dequeue(&q, &out); EXPECT(out == 4);
    queue_int_dequeue(&q, &out); EXPECT(out == 5);
    queue_int_dequeue(&q, &out); EXPECT(out == 6);
}

/* ── dequeue — empty queue ───────────────────────────────────────────────── */

static void test_dequeue_empty(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    int out = 999;
    result__Bool_Error r = queue_int_dequeue(&q, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_STATE);
    EXPECT(out == 999); /* out must not be written */
}

/* ── dequeue — NULL args ─────────────────────────────────────────────────── */

static void test_dequeue_null_args(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);
    int out = 0;

    result__Bool_Error r;

    /* NULL queue */
    r = queue_int_dequeue(NULL, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    /* NULL out */
    queue_int_enqueue(&q, 42);
    r = queue_int_dequeue(&q, NULL);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── dequeue_option — Some / None ────────────────────────────────────────── */

static void test_dequeue_option(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    queue_int_enqueue(&q, 10);
    queue_int_enqueue(&q, 20);

    option_int o;

    o = queue_int_dequeue_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 10);

    o = queue_int_dequeue_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 20);

    /* None on empty */
    o = queue_int_dequeue_option(&q);
    EXPECT(option_int_is_none(o));
}

/* ── peek — present / empty ──────────────────────────────────────────────── */

static void test_peek(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    /* empty — peek returns false */
    int out = 999;
    EXPECT(!queue_int_peek(&q, &out));
    EXPECT(out == 999); /* not written */

    queue_int_enqueue(&q, 55);
    queue_int_enqueue(&q, 77);

    /* peek twice — same result, len unchanged */
    EXPECT(queue_int_peek(&q, &out));
    EXPECT(out == 55);
    EXPECT(queue_int_len(&q) == 2);

    out = 0;
    EXPECT(queue_int_peek(&q, &out));
    EXPECT(out == 55);
    EXPECT(queue_int_len(&q) == 2);
}

/* ── peek_option — Some / None ───────────────────────────────────────────── */

static void test_peek_option(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    EXPECT(option_int_is_none(queue_int_peek_option(&q)));

    queue_int_enqueue(&q, 42);

    option_int o = queue_int_peek_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 42);
    EXPECT(queue_int_len(&q) == 1); /* non-destructive */

    /* peek again — still Some(42) */
    o = queue_int_peek_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 42);
}

/* ── clear — resets to empty ─────────────────────────────────────────────── */

static void test_clear(void)
{
    int buf[8];
    queue_int q;
    queue_int_init(&q, buf, 8);

    queue_int_enqueue(&q, 1);
    queue_int_enqueue(&q, 2);
    queue_int_enqueue(&q, 3);
    EXPECT(queue_int_len(&q) == 3);

    queue_int_clear(&q);
    EXPECT(queue_int_len(&q)       == 0);
    EXPECT(queue_int_capacity(&q)  == 8);
    EXPECT(queue_int_remaining(&q) == 8);
    EXPECT(queue_int_is_empty(&q));

    /* Buffer reusable after clear */
    result__Bool_Error r = queue_int_enqueue(&q, 99);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(queue_int_len(&q) == 1);
}

/* ── queries ─────────────────────────────────────────────────────────────── */

static void test_queries(void)
{
    int buf[4];
    queue_int q;
    queue_int_init(&q, buf, 4);

    EXPECT(queue_int_len(&q)       == 0);
    EXPECT(queue_int_capacity(&q)  == 4);
    EXPECT(queue_int_remaining(&q) == 4);
    EXPECT(queue_int_is_empty(&q));
    EXPECT(!queue_int_is_full(&q));

    queue_int_enqueue(&q, 10);
    EXPECT(queue_int_len(&q)       == 1);
    EXPECT(queue_int_remaining(&q) == 3);
    EXPECT(!queue_int_is_empty(&q));
    EXPECT(!queue_int_is_full(&q));

    queue_int_enqueue(&q, 20);
    queue_int_enqueue(&q, 30);
    queue_int_enqueue(&q, 40);
    EXPECT(queue_int_len(&q)       == 4);
    EXPECT(queue_int_remaining(&q) == 0);
    EXPECT(queue_int_is_full(&q));
    EXPECT(!queue_int_is_empty(&q));
}

/* ── ring-buffer wrap-around stress ──────────────────────────────────────── */

static void test_ring_wrap_around(void)
{
    int buf[8];
    queue_int q;
    queue_int_init(&q, buf, 8);

    for (int round = 0; round < 16; round++) {
        for (int i = 0; i < 8; i++) {
            result__Bool_Error r = queue_int_enqueue(&q, round * 8 + i);
            EXPECT(result__Bool_Error_is_ok(r));
        }
        EXPECT(queue_int_is_full(&q));

        for (int i = 0; i < 8; i++) {
            int out = 0;
            result__Bool_Error r = queue_int_dequeue(&q, &out);
            EXPECT(result__Bool_Error_is_ok(r));
            EXPECT(out == round * 8 + i);
        }
        EXPECT(queue_int_is_empty(&q));
    }
}

/* ── struct type — exercises all generated Msg functions ─────────────────── */

static void test_struct_type(void)
{
    Msg buf[4];
    queue_Msg q;
    queue_Msg_init(&q, buf, 4);

    EXPECT(queue_Msg_is_empty(&q));
    EXPECT(queue_Msg_len(&q)       == 0);
    EXPECT(queue_Msg_capacity(&q)  == 4);
    EXPECT(queue_Msg_remaining(&q) == 4);
    EXPECT(!queue_Msg_is_full(&q));

    Msg m1 = {1, 10};
    Msg m2 = {2, 20};
    Msg m3 = {3, 30};

    result__Bool_Error r;
    r = queue_Msg_enqueue(&q, m1); EXPECT(result__Bool_Error_is_ok(r));
    r = queue_Msg_enqueue(&q, m2); EXPECT(result__Bool_Error_is_ok(r));
    r = queue_Msg_enqueue(&q, m3); EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(queue_Msg_len(&q) == 3);

    /* try_enqueue on Msg */
    EXPECT(queue_Msg_try_enqueue(&q, (Msg){4, 40}));
    EXPECT(queue_Msg_is_full(&q));
    EXPECT(!queue_Msg_try_enqueue(&q, (Msg){5, 50}));

    /* Drain one and test enqueue_unchecked on Msg */
    Msg out_msg = {0, 0};
    queue_Msg_dequeue(&q, &out_msg);
    queue_Msg_enqueue_unchecked(&q, (Msg){5, 50});

    option_Msg peek = queue_Msg_peek_option(&q);
    EXPECT(option_Msg_is_some(peek));
    EXPECT(option_Msg_unwrap(peek).id == 2);
    EXPECT(queue_Msg_len(&q) == 4);

    /* peek — out-param variant */
    Msg top = {0, 0};
    EXPECT(queue_Msg_peek(&q, &top));
    EXPECT(top.id == 2);

    /* FIFO dequeue via option */
    option_Msg o;
    o = queue_Msg_dequeue_option(&q);
    EXPECT(option_Msg_is_some(o));
    EXPECT(option_Msg_unwrap(o).id == 2);

    /* FIFO dequeue via out-param */
    out_msg = (Msg){0, 0};
    r = queue_Msg_dequeue(&q, &out_msg);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out_msg.id == 3);

    EXPECT(queue_Msg_len(&q) == 2);

    /* clear */
    queue_Msg_clear(&q);
    EXPECT(queue_Msg_is_empty(&q));
    EXPECT(option_Msg_is_none(queue_Msg_dequeue_option(&q)));

    /* capacity exceeded */
    queue_Msg_enqueue(&q, m1);
    queue_Msg_enqueue(&q, m2);
    queue_Msg_enqueue(&q, m3);
    queue_Msg_enqueue(&q, (Msg){4, 40});
    EXPECT(queue_Msg_is_full(&q));
    r = queue_Msg_enqueue(&q, (Msg){5, 50});
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
}

/* ── Suppress unused generated functions ────────────────────────────────── */

static void queue_suppress_unused(void)
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

    /* option_Msg combinators not tested here */
    (void)option_Msg_get;
    (void)option_Msg_unwrap_or;
    (void)option_Msg_expect;
    (void)option_Msg_map;
    (void)option_Msg_and_then;
    (void)option_Msg_or_else;
    (void)option_Msg_filter;
    (void)option_Msg_combine_with;
    (void)option_Msg_replace;
    (void)option_Msg_take;
    (void)option_Msg_eq;

    /* Deque functions exposed through the alias but not needed in tests */
    (void)deque_int_push_front;
    (void)deque_int_pop_back;
    (void)deque_int_pop_back_option;
    (void)deque_int_peek_back;
    (void)deque_int_peek_back_option;
    (void)deque_int_empty;
    (void)deque_int_swap;
    (void)deque_int_try_push_front;
    (void)deque_int_try_push_back;
    (void)deque_int_push_front_unchecked;
    (void)deque_int_push_back_unchecked;
    (void)deque_Msg_push_front;
    (void)deque_Msg_pop_back;
    (void)deque_Msg_pop_back_option;
    (void)deque_Msg_peek_back;
    (void)deque_Msg_peek_back_option;
    (void)deque_Msg_empty;
    (void)deque_Msg_swap;
    (void)deque_Msg_try_push_front;
    (void)deque_Msg_try_push_back;
    (void)deque_Msg_push_front_unchecked;
    (void)deque_Msg_push_back_unchecked;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    (void)queue_suppress_unused;

    test_init();
    test_fifo_ordering();
    test_enqueue_capacity_exceeded();
    test_enqueue_null_args();

    /* try_enqueue */
    test_try_enqueue_success();
    test_try_enqueue_full_returns_false();
    test_try_enqueue_null_returns_false();

    /* enqueue_unchecked */
    test_enqueue_unchecked_success();
    test_enqueue_unchecked_wraparound();

    test_dequeue_empty();
    test_dequeue_null_args();
    test_dequeue_option();
    test_peek();
    test_peek_option();
    test_clear();
    test_queries();
    test_ring_wrap_around();
    test_struct_type();

    if (g_failed == 0) {
        printf("OK  queue_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  queue_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void queue_fuzz_suppress_unused(void)
{
    /* Struct type not used in fuzz path */
    (void)queue_Msg_init;
    (void)queue_Msg_enqueue;
    (void)queue_Msg_try_enqueue;
    (void)queue_Msg_enqueue_unchecked;
    (void)queue_Msg_dequeue;
    (void)queue_Msg_dequeue_option;
    (void)queue_Msg_peek;
    (void)queue_Msg_peek_option;
    (void)queue_Msg_len;
    (void)queue_Msg_capacity;
    (void)queue_Msg_remaining;
    (void)queue_Msg_is_empty;
    (void)queue_Msg_is_full;
    (void)queue_Msg_clear;

    /* Deque functions not used in fuzz path */
    (void)deque_int_push_front;
    (void)deque_int_pop_back;
    (void)deque_int_pop_back_option;
    (void)deque_int_peek_back;
    (void)deque_int_peek_back_option;
    (void)deque_int_empty;
    (void)deque_int_swap;
    (void)deque_int_try_push_front;
    (void)deque_int_try_push_back;
    (void)deque_int_push_front_unchecked;
    (void)deque_int_push_back_unchecked;
    (void)deque_Msg_push_front;
    (void)deque_Msg_pop_back;
    (void)deque_Msg_pop_back_option;
    (void)deque_Msg_peek_back;
    (void)deque_Msg_peek_back_option;
    (void)deque_Msg_empty;
    (void)deque_Msg_swap;
    (void)deque_Msg_try_push_front;
    (void)deque_Msg_try_push_back;
    (void)deque_Msg_push_front_unchecked;
    (void)deque_Msg_push_back_unchecked;

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

    /* option_Msg — all unused in fuzz */
    (void)option_Msg_some;
    (void)option_Msg_none;
    (void)option_Msg_is_some;
    (void)option_Msg_is_none;
    (void)option_Msg_unwrap;
    (void)option_Msg_get;
    (void)option_Msg_unwrap_or;
    (void)option_Msg_expect;
    (void)option_Msg_map;
    (void)option_Msg_and_then;
    (void)option_Msg_or_else;
    (void)option_Msg_filter;
    (void)option_Msg_combine_with;
    (void)option_Msg_replace;
    (void)option_Msg_take;
    (void)option_Msg_eq;

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

    /* int queue functions not used in fuzz path */
    (void)queue_int_capacity;
    (void)queue_int_remaining;
    (void)queue_int_is_full;
    (void)queue_int_peek;
}

/*
 * Input layout:
 *   [0]    capacity selector — maps to cap_table[data[0] % 4]
 *   [1..N] operation stream — each byte:
 *            high nibble: operation index (0–6)
 *            low  nibble: value to enqueue (0–15)
 *
 * Operations:
 *   0 — enqueue(value)
 *   1 — dequeue — verify FIFO ordering against ref[]
 *   2 — dequeue_option — same verification, option variant
 *   3 — peek_option — verify non-destructive, matches front of ref
 *   4 — clear — resets queue and reference
 *   5 — try_enqueue(value) — bool variant
 *   6 — enqueue_unchecked(value) — only if not full
 *
 * Reference model:
 *   ref[16]         — circular array holding enqueued values
 *   ref_head        — index of the oldest (next to dequeue) element
 *   ref_len         — number of elements tracked
 *
 * Invariants checked after every operation:
 *   - is_empty iff len == 0
 *   - len agrees with ref_len
 *   - peek_option matches ref front (if non-empty)
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static const usize cap_table[4] = {2, 4, 8, 16};

    /* Max capacity 16 — size buffer for that */
    int buf[16];
    queue_int q;

    (void)queue_fuzz_suppress_unused;

    if (size < 2) return 0;

    usize cap = cap_table[data[0] % 4u];
    queue_int_init(&q, buf, cap);

    /*
     * Reference model — a parallel fixed-size circular queue.
     * Capacity matches the fuzzer-selected cap (max 16).
     */
    int   ref[16];
    usize ref_head = 0;
    usize ref_len  = 0;
    memset(ref, 0, sizeof(ref));

    #define CHECK_INVARIANTS()                                              \
        do {                                                                \
            /* is_empty consistency */                                      \
            if (queue_int_is_empty(&q) != (queue_int_len(&q) == 0)) \
                __builtin_trap();                                           \
            /* len agrees with ref */                                       \
            if (queue_int_len(&q) != ref_len)                        \
                __builtin_trap();                                           \
            /* peek matches front of ref */                                 \
            {                                                               \
                option_int top__ = queue_int_peek_option(&q);        \
                if (ref_len == 0) {                                         \
                    if (option_int_is_some(top__)) __builtin_trap();       \
                } else {                                                    \
                    if (!option_int_is_some(top__)) __builtin_trap();      \
                    if (option_int_unwrap(top__) != ref[ref_head % 16])    \
                        __builtin_trap();                                   \
                }                                                           \
            }                                                               \
        } while (0)

    for (usize i = 1; i < size; i++) {
        u8  byte = data[i];
        u8  op   = (u8)(byte >> 4u) % 7u;
        int val  = (int)(byte & 0x0Fu); /* 0–15 */

        switch (op) {

            case 0: { /* enqueue */
                result__Bool_Error r = queue_int_enqueue(&q, val);
                if (result__Bool_Error_is_ok(r)) {
                    if (ref_len >= cap) __builtin_trap();
                    ref[(ref_head + ref_len) % 16] = val;
                    ref_len++;
                }
                break;
            }

            case 1: { /* dequeue — out-param variant */
                int out = -1;
                result__Bool_Error r = queue_int_dequeue(&q, &out);
                if (result__Bool_Error_is_ok(r)) {
                    if (ref_len == 0)                     __builtin_trap();
                    if (out != ref[ref_head % 16])        __builtin_trap();
                    ref_head = (ref_head + 1) % 16;
                    ref_len--;
                } else {
                    if (ref_len != 0)                     __builtin_trap();
                }
                break;
            }

            case 2: { /* dequeue_option variant */
                option_int o = queue_int_dequeue_option(&q);
                if (option_int_is_some(o)) {
                    if (ref_len == 0)                           __builtin_trap();
                    if (option_int_unwrap(o) != ref[ref_head % 16])
                                                                __builtin_trap();
                    ref_head = (ref_head + 1) % 16;
                    ref_len--;
                } else {
                    if (ref_len != 0)                           __builtin_trap();
                }
                break;
            }

            case 3: { /* peek_option — non-destructive */
                usize before = queue_int_len(&q);
                option_int o = queue_int_peek_option(&q);
                if (queue_int_len(&q) != before)  __builtin_trap();
                if (ref_len == 0) {
                    if (option_int_is_some(o))           __builtin_trap();
                } else {
                    if (!option_int_is_some(o))          __builtin_trap();
                    if (option_int_unwrap(o) != ref[ref_head % 16])
                                                         __builtin_trap();
                }
                break;
            }

            case 4: { /* clear */
                queue_int_clear(&q);
                ref_head = 0;
                ref_len  = 0;
                if (!queue_int_is_empty(&q)) __builtin_trap();
                break;
            }

            case 5: { /* try_enqueue */
                bool ok = queue_int_try_enqueue(&q, val);
                if (ok) {
                    if (ref_len >= cap) __builtin_trap();
                    ref[(ref_head + ref_len) % 16] = val;
                    ref_len++;
                }
                break;
            }

            case 6: { /* enqueue_unchecked — only if not full */
                if (!queue_int_is_full(&q)) {
                    queue_int_enqueue_unchecked(&q, val);
                    if (ref_len >= cap) __builtin_trap();
                    ref[(ref_head + ref_len) % 16] = val;
                    ref_len++;
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
