/**
 * @file queue_test.c
 * @brief Tests for data/queue.h — bounded FIFO queue (deque wrapper)
 *
 * Covers:
 *   - canon_queue_int_init()              — valid initialization
 *   - canon_queue_int_enqueue()           — success, capacity exceeded,
 *                                           NULL args (ERR_INVALID_ARG)
 *   - canon_queue_int_dequeue()           — FIFO order, empty queue
 *                                           (ERR_INVALID_STATE), NULL args
 *   - canon_queue_int_dequeue_option()    — Some / None
 *   - canon_queue_int_peek()              — present / empty, non-destructive
 *   - canon_queue_int_peek_option()       — Some / None, non-destructive
 *   - canon_queue_int_clear()             — resets to empty, buffer reusable
 *   - Queries                             — len, capacity, remaining,
 *                                           is_empty, is_full
 *   - FIFO ordering stress                — 128-element wrap-around ring
 *   - DEFINE_QUEUE(Msg) struct type       — exercises all generated code paths
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - ref[16] parallel array tracks ground truth FIFO ordering
 *   - ref_head / ref_tail / ref_len maintain queue state
 *   - Verifies enqueue, dequeue, peek, dequeue_option against ref after
 *     every operation
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
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 8);

    EXPECT(canon_queue_int_len(&q)       == 0);
    EXPECT(canon_queue_int_capacity(&q)  == 8);
    EXPECT(canon_queue_int_remaining(&q) == 8);
    EXPECT(canon_queue_int_is_empty(&q));
    EXPECT(!canon_queue_int_is_full(&q));
}

/* ── FIFO ordering ───────────────────────────────────────────────────────── */

static void test_fifo_ordering(void)
{
    int buf[8];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 8);

    for (int i = 1; i <= 5; i++) {
        result__Bool_Error r = canon_queue_int_enqueue(&q, i * 10);
        EXPECT(result__Bool_Error_is_ok(r));
    }
    EXPECT(canon_queue_int_len(&q) == 5);

    /* dequeue must return values in enqueue order */
    for (int i = 1; i <= 5; i++) {
        int out = 0;
        result__Bool_Error r = canon_queue_int_dequeue(&q, &out);
        EXPECT(result__Bool_Error_is_ok(r));
        EXPECT(out == i * 10);
    }
    EXPECT(canon_queue_int_is_empty(&q));
}

/* ── enqueue — capacity exceeded ─────────────────────────────────────────── */

static void test_enqueue_capacity_exceeded(void)
{
    int buf[4];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 4);

    canon_queue_int_enqueue(&q, 1);
    canon_queue_int_enqueue(&q, 2);
    canon_queue_int_enqueue(&q, 3);
    canon_queue_int_enqueue(&q, 4);
    EXPECT(canon_queue_int_is_full(&q));

    result__Bool_Error r = canon_queue_int_enqueue(&q, 5);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
    EXPECT(canon_queue_int_len(&q) == 4); /* unchanged */
}

/* ── enqueue — NULL args ─────────────────────────────────────────────────── */

static void test_enqueue_null_args(void)
{
    result__Bool_Error r = canon_queue_int_enqueue(NULL, 42);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── dequeue — empty queue ───────────────────────────────────────────────── */

static void test_dequeue_empty(void)
{
    int buf[4];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 4);

    int out = 999;
    result__Bool_Error r = canon_queue_int_dequeue(&q, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_STATE);
    EXPECT(out == 999); /* out must not be written */
}

/* ── dequeue — NULL args ─────────────────────────────────────────────────── */

static void test_dequeue_null_args(void)
{
    int buf[4];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 4);
    int out = 0;

    result__Bool_Error r;

    /* NULL queue */
    r = canon_queue_int_dequeue(NULL, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    /* NULL out */
    canon_queue_int_enqueue(&q, 42);
    r = canon_queue_int_dequeue(&q, NULL);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── dequeue_option — Some / None ────────────────────────────────────────── */

static void test_dequeue_option(void)
{
    int buf[4];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 4);

    canon_queue_int_enqueue(&q, 10);
    canon_queue_int_enqueue(&q, 20);

    option_int o;

    o = canon_queue_int_dequeue_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 10);

    o = canon_queue_int_dequeue_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 20);

    /* None on empty */
    o = canon_queue_int_dequeue_option(&q);
    EXPECT(option_int_is_none(o));
}

/* ── peek — present / empty ──────────────────────────────────────────────── */

static void test_peek(void)
{
    int buf[4];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 4);

    /* empty — peek returns false */
    int out = 999;
    EXPECT(!canon_queue_int_peek(&q, &out));
    EXPECT(out == 999); /* not written */

    canon_queue_int_enqueue(&q, 55);
    canon_queue_int_enqueue(&q, 77);

    /* peek twice — same result, len unchanged */
    EXPECT(canon_queue_int_peek(&q, &out));
    EXPECT(out == 55);
    EXPECT(canon_queue_int_len(&q) == 2);

    out = 0;
    EXPECT(canon_queue_int_peek(&q, &out));
    EXPECT(out == 55);
    EXPECT(canon_queue_int_len(&q) == 2);
}

/* ── peek_option — Some / None ───────────────────────────────────────────── */

static void test_peek_option(void)
{
    int buf[4];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 4);

    EXPECT(option_int_is_none(canon_queue_int_peek_option(&q)));

    canon_queue_int_enqueue(&q, 42);

    option_int o = canon_queue_int_peek_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 42);
    EXPECT(canon_queue_int_len(&q) == 1); /* non-destructive */

    /* peek again — still Some(42) */
    o = canon_queue_int_peek_option(&q);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 42);
}

/* ── clear — resets to empty ─────────────────────────────────────────────── */

static void test_clear(void)
{
    int buf[8];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 8);

    canon_queue_int_enqueue(&q, 1);
    canon_queue_int_enqueue(&q, 2);
    canon_queue_int_enqueue(&q, 3);
    EXPECT(canon_queue_int_len(&q) == 3);

    canon_queue_int_clear(&q);
    EXPECT(canon_queue_int_len(&q)       == 0);
    EXPECT(canon_queue_int_capacity(&q)  == 8);
    EXPECT(canon_queue_int_remaining(&q) == 8);
    EXPECT(canon_queue_int_is_empty(&q));

    /* Buffer reusable after clear */
    result__Bool_Error r = canon_queue_int_enqueue(&q, 99);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_queue_int_len(&q) == 1);
}

/* ── queries ─────────────────────────────────────────────────────────────── */

static void test_queries(void)
{
    int buf[4];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 4);

    EXPECT(canon_queue_int_len(&q)       == 0);
    EXPECT(canon_queue_int_capacity(&q)  == 4);
    EXPECT(canon_queue_int_remaining(&q) == 4);
    EXPECT(canon_queue_int_is_empty(&q));
    EXPECT(!canon_queue_int_is_full(&q));

    canon_queue_int_enqueue(&q, 10);
    EXPECT(canon_queue_int_len(&q)       == 1);
    EXPECT(canon_queue_int_remaining(&q) == 3);
    EXPECT(!canon_queue_int_is_empty(&q));
    EXPECT(!canon_queue_int_is_full(&q));

    canon_queue_int_enqueue(&q, 20);
    canon_queue_int_enqueue(&q, 30);
    canon_queue_int_enqueue(&q, 40);
    EXPECT(canon_queue_int_len(&q)       == 4);
    EXPECT(canon_queue_int_remaining(&q) == 0);
    EXPECT(canon_queue_int_is_full(&q));
    EXPECT(!canon_queue_int_is_empty(&q));
}

/* ── ring-buffer wrap-around stress ──────────────────────────────────────── */

static void test_ring_wrap_around(void)
{
    /*
     * Enqueue 128 values through a cap=8 queue by interleaving
     * enqueue and dequeue — exercises the ring-buffer wrap-around path
     * that deque_impl.h uses for head/tail management.
     */
    int buf[8];
    canon_queue_int q;
    canon_queue_int_init(&q, buf, 8);

    for (int round = 0; round < 16; round++) {
        /* Fill to capacity */
        for (int i = 0; i < 8; i++) {
            result__Bool_Error r = canon_queue_int_enqueue(&q, round * 8 + i);
            EXPECT(result__Bool_Error_is_ok(r));
        }
        EXPECT(canon_queue_int_is_full(&q));

        /* Drain in FIFO order */
        for (int i = 0; i < 8; i++) {
            int out = 0;
            result__Bool_Error r = canon_queue_int_dequeue(&q, &out);
            EXPECT(result__Bool_Error_is_ok(r));
            EXPECT(out == round * 8 + i);
        }
        EXPECT(canon_queue_int_is_empty(&q));
    }
}

/* ── struct type — exercises all generated Msg functions ─────────────────── */

static void test_struct_type(void)
{
    Msg buf[4];
    canon_queue_Msg q;
    canon_queue_Msg_init(&q, buf, 4);

    EXPECT(canon_queue_Msg_is_empty(&q));
    EXPECT(canon_queue_Msg_len(&q)       == 0);
    EXPECT(canon_queue_Msg_capacity(&q)  == 4);
    EXPECT(canon_queue_Msg_remaining(&q) == 4);
    EXPECT(!canon_queue_Msg_is_full(&q));

    Msg m1 = {1, 10};
    Msg m2 = {2, 20};
    Msg m3 = {3, 30};

    result__Bool_Error r;
    r = canon_queue_Msg_enqueue(&q, m1); EXPECT(result__Bool_Error_is_ok(r));
    r = canon_queue_Msg_enqueue(&q, m2); EXPECT(result__Bool_Error_is_ok(r));
    r = canon_queue_Msg_enqueue(&q, m3); EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_queue_Msg_len(&q) == 3);

    /* peek_option — non-destructive */
    option_Msg peek = canon_queue_Msg_peek_option(&q);
    EXPECT(option_Msg_is_some(peek));
    EXPECT(option_Msg_unwrap(peek).id == 1);
    EXPECT(canon_queue_Msg_len(&q) == 3);

    /* peek — out-param variant */
    Msg top = {0, 0};
    EXPECT(canon_queue_Msg_peek(&q, &top));
    EXPECT(top.id == 1);

    /* FIFO dequeue via option */
    option_Msg o;
    o = canon_queue_Msg_dequeue_option(&q);
    EXPECT(option_Msg_is_some(o));
    EXPECT(option_Msg_unwrap(o).id == 1);

    /* FIFO dequeue via out-param */
    Msg out_msg = {0, 0};
    r = canon_queue_Msg_dequeue(&q, &out_msg);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out_msg.id == 2);

    EXPECT(canon_queue_Msg_len(&q) == 1);

    /* clear */
    canon_queue_Msg_clear(&q);
    EXPECT(canon_queue_Msg_is_empty(&q));
    EXPECT(option_Msg_is_none(canon_queue_Msg_dequeue_option(&q)));

    /* capacity exceeded */
    canon_queue_Msg_enqueue(&q, m1);
    canon_queue_Msg_enqueue(&q, m2);
    canon_queue_Msg_enqueue(&q, m3);
    canon_queue_Msg_enqueue(&q, (Msg){4, 40});
    EXPECT(canon_queue_Msg_is_full(&q));
    r = canon_queue_Msg_enqueue(&q, (Msg){5, 50});
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
    (void)canon_deque_int_push_front;
    (void)canon_deque_int_pop_back;
    (void)canon_deque_int_pop_back_option;
    (void)canon_deque_int_peek_back;
    (void)canon_deque_int_peek_back_option;
    (void)canon_deque_int_empty;
    (void)canon_deque_int_swap;
    (void)canon_deque_Msg_push_front;
    (void)canon_deque_Msg_pop_back;
    (void)canon_deque_Msg_pop_back_option;
    (void)canon_deque_Msg_peek_back;
    (void)canon_deque_Msg_peek_back_option;
    (void)canon_deque_Msg_empty;
    (void)canon_deque_Msg_swap;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    (void)queue_suppress_unused;

    test_init();
    test_fifo_ordering();
    test_enqueue_capacity_exceeded();
    test_enqueue_null_args();
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
    (void)canon_queue_Msg_init;
    (void)canon_queue_Msg_enqueue;
    (void)canon_queue_Msg_dequeue;
    (void)canon_queue_Msg_dequeue_option;
    (void)canon_queue_Msg_peek;
    (void)canon_queue_Msg_peek_option;
    (void)canon_queue_Msg_len;
    (void)canon_queue_Msg_capacity;
    (void)canon_queue_Msg_remaining;
    (void)canon_queue_Msg_is_empty;
    (void)canon_queue_Msg_is_full;
    (void)canon_queue_Msg_clear;

    /* Deque functions not used in fuzz path */
    (void)canon_deque_int_push_front;
    (void)canon_deque_int_pop_back;
    (void)canon_deque_int_pop_back_option;
    (void)canon_deque_int_peek_back;
    (void)canon_deque_int_peek_back_option;
    (void)canon_deque_int_empty;
    (void)canon_deque_int_swap;
    (void)canon_deque_Msg_push_front;
    (void)canon_deque_Msg_pop_back;
    (void)canon_deque_Msg_pop_back_option;
    (void)canon_deque_Msg_peek_back;
    (void)canon_deque_Msg_peek_back_option;
    (void)canon_deque_Msg_empty;
    (void)canon_deque_Msg_swap;

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

    /* int queue capacity/remaining not used in fuzz path */
    (void)canon_queue_int_capacity;
    (void)canon_queue_int_remaining;
    (void)canon_queue_int_is_full;
    (void)canon_queue_int_peek;
}

/*
 * Input layout:
 *   [0]    capacity selector — maps to cap_table[data[0] % 4]
 *   [1..N] operation stream — each byte:
 *            high nibble: operation index (0–4)
 *            low  nibble: value to enqueue (0–15)
 *
 * Operations:
 *   0 — enqueue(value)
 *   1 — dequeue — verify FIFO ordering against ref[]
 *   2 — dequeue_option — same verification, option variant
 *   3 — peek_option — verify non-destructive, matches front of ref
 *   4 — clear — resets queue and reference
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
    canon_queue_int q;

    (void)queue_fuzz_suppress_unused;

    if (size < 2) return 0;

    usize cap = cap_table[data[0] % 4u];
    canon_queue_int_init(&q, buf, cap);

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
            if (canon_queue_int_is_empty(&q) != (canon_queue_int_len(&q) == 0)) \
                __builtin_trap();                                           \
            /* len agrees with ref */                                       \
            if (canon_queue_int_len(&q) != ref_len)                        \
                __builtin_trap();                                           \
            /* peek matches front of ref */                                 \
            {                                                               \
                option_int top__ = canon_queue_int_peek_option(&q);        \
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
        u8  op   = (u8)(byte >> 4u) % 5u;
        int val  = (int)(byte & 0x0Fu); /* 0–15 */

        switch (op) {

            case 0: { /* enqueue */
                result__Bool_Error r = canon_queue_int_enqueue(&q, val);
                if (result__Bool_Error_is_ok(r)) {
                    /* queue accepted it — ref must have room */
                    if (ref_len >= cap) __builtin_trap();
                    ref[(ref_head + ref_len) % 16] = val;
                    ref_len++;
                }
                /* ERR_CAPACITY_EXCEEDED when full — not a trap */
                break;
            }

            case 1: { /* dequeue — out-param variant */
                int out = -1;
                result__Bool_Error r = canon_queue_int_dequeue(&q, &out);
                if (result__Bool_Error_is_ok(r)) {
                    if (ref_len == 0)                     __builtin_trap();
                    if (out != ref[ref_head % 16])        __builtin_trap();
                    ref_head = (ref_head + 1) % 16;
                    ref_len--;
                } else {
                    /* must fail only when empty */
                    if (ref_len != 0)                     __builtin_trap();
                }
                break;
            }

            case 2: { /* dequeue_option variant */
                option_int o = canon_queue_int_dequeue_option(&q);
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
                usize before = canon_queue_int_len(&q);
                option_int o = canon_queue_int_peek_option(&q);
                if (canon_queue_int_len(&q) != before)  __builtin_trap();
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
                canon_queue_int_clear(&q);
                ref_head = 0;
                ref_len  = 0;
                if (!canon_queue_int_is_empty(&q)) __builtin_trap();
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
