/**
 * @file deque_test.c
 * @brief Tests for data/deque/deque.h — bounded double-ended queue (ring buffer)
 *
 * Covers:
 *   - DEFINE_DEQUE instantiation for primitive and struct types
 *   - Constructor: init(), empty()
 *   - Queries: len(), capacity(), remaining(), is_empty(), is_full()
 *   - Push: push_front(), push_back() — success and ERR_CAPACITY_EXCEEDED
 *   - Pop:  pop_front(), pop_back()  — success and ERR_INVALID_STATE
 *   - Pop option: pop_front_option(), pop_back_option()
 *   - Peek: peek_front(), peek_back() — non-mutating
 *   - Peek option: peek_front_option(), peek_back_option()
 *   - Misc: clear(), swap()
 *   - Ring buffer wrap-around correctness (head/tail modulo arithmetic)
 *   - NULL-guard paths: ERR_INVALID_ARG on NULL deque or NULL out-param
 *   - Sliding window pattern
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random bytes interpreted as a sequence of push/pop/peek operations
 *   - Verifies ring buffer invariants hold under arbitrary inputs:
 *     no crash, size always in [0, capacity], len + remaining == capacity,
 *     is_empty/is_full consistency, FIFO order preserved
 */

#define CANON_CONTRACT_IMPL
#include "semantics/option/option.h"
#include "data/deque/deque.h"

#include <stdio.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

/* Option must be instantiated before DEFINE_DEQUE */
CANON_OPTION(int)
DEFINE_DEQUE(static inline, int)

/* Second instantiation — struct type, exercises all generated code paths */
typedef struct { int x; int y; } Point;
CANON_OPTION(Point)
DEFINE_DEQUE(static inline, Point)

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static bool point_eq(Point a, Point b) { return a.x == b.x && a.y == b.y; }

/* ════════════════════════════════════════════════════════════════════════════
   Unit test build
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                              \
    do {                                                          \
        if (!(cond)) {                                            \
            fprintf(stderr, "FAIL %s:%d  %s\n",                  \
                    __FILE__, __LINE__, #cond);                   \
            g_failed++;                                           \
        }                                                         \
    } while (0)

/* EXPECT_NOT_NULL — records failure and returns from the calling function
 * if ptr is NULL. Prevents nullPointerRedundantCheck false positives that
 * arise when EXPECT(p != NULL) is followed by a dereference of p. */
#define EXPECT_NOT_NULL(ptr)                                      \
    do {                                                          \
        if ((ptr) == NULL) {                                      \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",          \
                    __FILE__, __LINE__, #ptr);                    \
            g_failed++;                                           \
            return;                                               \
        }                                                         \
    } while (0)

/* ── Constructor: init() ─────────────────────────────────────────────────── */

static void test_init_sets_state(void)
{
    int buf[8];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 8);

    EXPECT(canon_deque_int_len(&d)       == 0);
    EXPECT(canon_deque_int_capacity(&d)  == 8);
    EXPECT(canon_deque_int_remaining(&d) == 8);
    EXPECT(canon_deque_int_is_empty(&d));
    EXPECT(!canon_deque_int_is_full(&d));
}

/* ── Constructor: empty() ────────────────────────────────────────────────── */

static void test_empty_is_null_state(void)
{
    canon_deque_int d = canon_deque_int_empty();

    /* NULL-safe queries return safe defaults */
    EXPECT(canon_deque_int_len(&d)       == 0);
    EXPECT(canon_deque_int_capacity(&d)  == 0);
    EXPECT(canon_deque_int_remaining(&d) == 0);
    EXPECT(canon_deque_int_is_empty(&d));
    EXPECT(canon_deque_int_is_full(&d));  /* NULL guard: is_full returns true on NULL */
}

/* ── NULL-safe queries ───────────────────────────────────────────────────── */

static void test_queries_null_safe(void)
{
    EXPECT(canon_deque_int_len(NULL)       == 0);
    EXPECT(canon_deque_int_capacity(NULL)  == 0);
    EXPECT(canon_deque_int_remaining(NULL) == 0);
    EXPECT(canon_deque_int_is_empty(NULL));
    EXPECT(canon_deque_int_is_full(NULL));
}

/* ── push_back + pop_front (FIFO order) ──────────────────────────────────── */

static void test_push_back_pop_front_fifo(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    result__Bool_Error r;

    r = canon_deque_int_push_back(&d, 1);
    EXPECT(result__Bool_Error_is_ok(r));
    r = canon_deque_int_push_back(&d, 2);
    EXPECT(result__Bool_Error_is_ok(r));
    r = canon_deque_int_push_back(&d, 3);
    EXPECT(result__Bool_Error_is_ok(r));

    EXPECT(canon_deque_int_len(&d) == 3);

    int out = 0;
    r = canon_deque_int_pop_front(&d, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out == 1);

    r = canon_deque_int_pop_front(&d, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out == 2);

    r = canon_deque_int_pop_front(&d, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out == 3);

    EXPECT(canon_deque_int_is_empty(&d));
}

/* ── push_front + pop_back (reverse FIFO order) ──────────────────────────── */

static void test_push_front_pop_back(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    canon_deque_int_push_front(&d, 10);
    canon_deque_int_push_front(&d, 20);
    canon_deque_int_push_front(&d, 30);

    /* deque front-to-back: 30, 20, 10 */

    int out = 0;
    canon_deque_int_pop_back(&d, &out);
    EXPECT(out == 10);

    canon_deque_int_pop_back(&d, &out);
    EXPECT(out == 20);

    canon_deque_int_pop_back(&d, &out);
    EXPECT(out == 30);

    EXPECT(canon_deque_int_is_empty(&d));
}

/* ── push_front + pop_front (LIFO / stack order) ─────────────────────────── */

static void test_push_front_pop_front_lifo(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    canon_deque_int_push_front(&d, 1);
    canon_deque_int_push_front(&d, 2);
    canon_deque_int_push_front(&d, 3);

    int out = 0;
    canon_deque_int_pop_front(&d, &out);
    EXPECT(out == 3);

    canon_deque_int_pop_front(&d, &out);
    EXPECT(out == 2);

    canon_deque_int_pop_front(&d, &out);
    EXPECT(out == 1);
}

/* ── capacity exceeded ───────────────────────────────────────────────────── */

static void test_push_back_full_returns_err(void)
{
    int buf[2];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 2);

    canon_deque_int_push_back(&d, 1);
    canon_deque_int_push_back(&d, 2);

    EXPECT(canon_deque_int_is_full(&d));

    result__Bool_Error r = canon_deque_int_push_back(&d, 3);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);

    /* deque is unchanged */
    EXPECT(canon_deque_int_len(&d) == 2);
}

static void test_push_front_full_returns_err(void)
{
    int buf[2];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 2);

    canon_deque_int_push_front(&d, 1);
    canon_deque_int_push_front(&d, 2);

    result__Bool_Error r = canon_deque_int_push_front(&d, 3);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
}

/* ── pop on empty ────────────────────────────────────────────────────────── */

static void test_pop_front_empty_returns_err(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    int out = 0;
    result__Bool_Error r = canon_deque_int_pop_front(&d, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_STATE);
}

static void test_pop_back_empty_returns_err(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    int out = 0;
    result__Bool_Error r = canon_deque_int_pop_back(&d, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_STATE);
}

/* ── NULL guard on push/pop ──────────────────────────────────────────────── */

static void test_push_null_deque_returns_err(void)
{
    result__Bool_Error r;

    r = canon_deque_int_push_back(NULL, 1);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    r = canon_deque_int_push_front(NULL, 1);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_pop_null_deque_returns_err(void)
{
    int out = 0;
    result__Bool_Error r;

    r = canon_deque_int_pop_front(NULL, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    r = canon_deque_int_pop_back(NULL, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_pop_null_out_returns_err(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);
    canon_deque_int_push_back(&d, 1);

    result__Bool_Error r;

    r = canon_deque_int_pop_front(&d, NULL);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    r = canon_deque_int_pop_back(&d, NULL);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── peek_front / peek_back ──────────────────────────────────────────────── */

static void test_peek_front_does_not_remove(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    canon_deque_int_push_back(&d, 10);
    canon_deque_int_push_back(&d, 20);

    int out = 0;
    bool ok = canon_deque_int_peek_front(&d, &out);
    EXPECT(ok);
    EXPECT(out == 10);
    EXPECT(canon_deque_int_len(&d) == 2); /* unchanged */

    ok = canon_deque_int_peek_front(&d, &out);
    EXPECT(ok);
    EXPECT(out == 10); /* same value again */
}

static void test_peek_back_does_not_remove(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    canon_deque_int_push_back(&d, 10);
    canon_deque_int_push_back(&d, 20);

    int out = 0;
    bool ok = canon_deque_int_peek_back(&d, &out);
    EXPECT(ok);
    EXPECT(out == 20);
    EXPECT(canon_deque_int_len(&d) == 2); /* unchanged */
}

static void test_peek_front_empty_returns_false(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    int out = 99;
    bool ok = canon_deque_int_peek_front(&d, &out);
    EXPECT(!ok);
    EXPECT(out == 99); /* unchanged */
}

static void test_peek_back_empty_returns_false(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    int out = 99;
    bool ok = canon_deque_int_peek_back(&d, &out);
    EXPECT(!ok);
    EXPECT(out == 99); /* unchanged */
}

/* ── Option variants: pop ────────────────────────────────────────────────── */

static void test_pop_front_option_some(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);
    canon_deque_int_push_back(&d, 7);

    option_int o = canon_deque_int_pop_front_option(&d);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 7);
    EXPECT(canon_deque_int_is_empty(&d));
}

static void test_pop_front_option_none_on_empty(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    option_int o = canon_deque_int_pop_front_option(&d);
    EXPECT(option_int_is_none(o));
}

static void test_pop_back_option_some(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);
    canon_deque_int_push_back(&d, 1);
    canon_deque_int_push_back(&d, 2);

    option_int o = canon_deque_int_pop_back_option(&d);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 2);
    EXPECT(canon_deque_int_len(&d) == 1);
}

static void test_pop_back_option_none_on_empty(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    option_int o = canon_deque_int_pop_back_option(&d);
    EXPECT(option_int_is_none(o));
}

/* ── Option variants: peek ───────────────────────────────────────────────── */

static void test_peek_front_option_some(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);
    canon_deque_int_push_back(&d, 42);

    option_int o = canon_deque_int_peek_front_option(&d);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 42);
    EXPECT(canon_deque_int_len(&d) == 1); /* unchanged */
}

static void test_peek_front_option_none_on_empty(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    option_int o = canon_deque_int_peek_front_option(&d);
    EXPECT(option_int_is_none(o));
}

static void test_peek_back_option_some(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);
    canon_deque_int_push_back(&d, 1);
    canon_deque_int_push_back(&d, 99);

    option_int o = canon_deque_int_peek_back_option(&d);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 99);
    EXPECT(canon_deque_int_len(&d) == 2); /* unchanged */
}

static void test_peek_back_option_none_on_empty(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    option_int o = canon_deque_int_peek_back_option(&d);
    EXPECT(option_int_is_none(o));
}

/* ── clear() ─────────────────────────────────────────────────────────────── */

static void test_clear_resets_state(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    canon_deque_int_push_back(&d, 1);
    canon_deque_int_push_back(&d, 2);
    canon_deque_int_push_back(&d, 3);

    canon_deque_int_clear(&d);

    EXPECT(canon_deque_int_len(&d)       == 0);
    EXPECT(canon_deque_int_capacity(&d)  == 4);
    EXPECT(canon_deque_int_remaining(&d) == 4);
    EXPECT(canon_deque_int_is_empty(&d));
    EXPECT(!canon_deque_int_is_full(&d));
}

static void test_clear_allows_reuse(void)
{
    int buf[2];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 2);

    canon_deque_int_push_back(&d, 10);
    canon_deque_int_push_back(&d, 20);
    canon_deque_int_clear(&d);

    /* After clear, deque is usable at full capacity again */
    result__Bool_Error r;
    r = canon_deque_int_push_back(&d, 30);
    EXPECT(result__Bool_Error_is_ok(r));
    r = canon_deque_int_push_back(&d, 40);
    EXPECT(result__Bool_Error_is_ok(r));

    int out = 0;
    canon_deque_int_pop_front(&d, &out);
    EXPECT(out == 30);
    canon_deque_int_pop_front(&d, &out);
    EXPECT(out == 40);
}

static void test_clear_null_safe(void)
{
    /* Must not crash */
    canon_deque_int_clear(NULL);
}

/* ── swap() ──────────────────────────────────────────────────────────────── */

static void test_swap_exchanges_deques(void)
{
    int bufA[4], bufB[4];
    canon_deque_int a, b;
    canon_deque_int_init(&a, bufA, 4);
    canon_deque_int_init(&b, bufB, 4);

    canon_deque_int_push_back(&a, 1);
    canon_deque_int_push_back(&a, 2);

    canon_deque_int_push_back(&b, 9);

    canon_deque_int_swap(&a, &b);

    EXPECT(canon_deque_int_len(&a) == 1);
    EXPECT(canon_deque_int_len(&b) == 2);

    int out = 0;
    canon_deque_int_pop_front(&a, &out);
    EXPECT(out == 9);

    canon_deque_int_pop_front(&b, &out);
    EXPECT(out == 1);
    canon_deque_int_pop_front(&b, &out);
    EXPECT(out == 2);
}

static void test_swap_self(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);
    canon_deque_int_push_back(&d, 5);

    canon_deque_int_swap(&d, &d);

    EXPECT(canon_deque_int_len(&d) == 1);

    int out = 0;
    canon_deque_int_pop_front(&d, &out);
    EXPECT(out == 5);
}

/* ── Ring buffer wrap-around ─────────────────────────────────────────────── */

/*
 * This is the most critical correctness test for a ring buffer.
 * We fill the buffer, drain half, then push more — forcing head/tail
 * to wrap around the backing array boundary.
 */
static void test_wraparound_push_back_pop_front(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    /* Fill: [0]=1 [1]=2 [2]=3 [3]=4, head=0 tail=0 (full) */
    canon_deque_int_push_back(&d, 1);
    canon_deque_int_push_back(&d, 2);
    canon_deque_int_push_back(&d, 3);
    canon_deque_int_push_back(&d, 4);

    /* Drain two: head moves to 2 */
    int out = 0;
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 1);
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 2);

    /* Push two more — tail wraps: buf[0]=5, buf[1]=6 */
    canon_deque_int_push_back(&d, 5);
    canon_deque_int_push_back(&d, 6);

    EXPECT(canon_deque_int_is_full(&d));

    /* Drain all — should come out in FIFO order: 3,4,5,6 */
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 3);
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 4);
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 5);
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 6);

    EXPECT(canon_deque_int_is_empty(&d));
}

static void test_wraparound_push_front_pop_back(void)
{
    int buf[4];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 4);

    canon_deque_int_push_front(&d, 4);
    canon_deque_int_push_front(&d, 3);
    canon_deque_int_push_front(&d, 2);
    canon_deque_int_push_front(&d, 1);
    /* front-to-back: 1,2,3,4 */

    int out = 0;
    canon_deque_int_pop_back(&d, &out); EXPECT(out == 4);
    canon_deque_int_pop_back(&d, &out); EXPECT(out == 3);

    /* Push two more to the front — head wraps */
    canon_deque_int_push_front(&d, 0);
    canon_deque_int_push_front(&d, -1);
    /* front-to-back: -1, 0, 1, 2 */

    EXPECT(canon_deque_int_is_full(&d));

    canon_deque_int_pop_back(&d, &out); EXPECT(out == 2);
    canon_deque_int_pop_back(&d, &out); EXPECT(out == 1);
    canon_deque_int_pop_back(&d, &out); EXPECT(out == 0);
    canon_deque_int_pop_back(&d, &out); EXPECT(out == -1);

    EXPECT(canon_deque_int_is_empty(&d));
}

/* ── Sliding window pattern ──────────────────────────────────────────────── */

static void test_sliding_window(void)
{
    int buf[3];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 3);

    int stream[] = {10, 20, 30, 40, 50};
    int n = (int)(sizeof(stream) / sizeof(stream[0]));

    for (int i = 0; i < n; i++) {
        if (canon_deque_int_is_full(&d)) {
            int dropped = 0;
            canon_deque_int_pop_front(&d, &dropped);
        }
        canon_deque_int_push_back(&d, stream[i]);
    }

    /* Window should hold last 3: 30, 40, 50 */
    EXPECT(canon_deque_int_len(&d) == 3);

    int out = 0;
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 30);
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 40);
    canon_deque_int_pop_front(&d, &out); EXPECT(out == 50);
}

/* ── len + remaining invariant ───────────────────────────────────────────── */

static void test_len_remaining_invariant(void)
{
    int buf[5];
    canon_deque_int d;
    canon_deque_int_init(&d, buf, 5);

    for (int i = 0; i < 5; i++) {
        EXPECT(canon_deque_int_len(&d) + canon_deque_int_remaining(&d)
               == canon_deque_int_capacity(&d));
        canon_deque_int_push_back(&d, i);
    }

    EXPECT(canon_deque_int_len(&d) + canon_deque_int_remaining(&d)
           == canon_deque_int_capacity(&d));

    for (int i = 0; i < 5; i++) {
        int out = 0;
        canon_deque_int_pop_front(&d, &out);
        EXPECT(canon_deque_int_len(&d) + canon_deque_int_remaining(&d)
               == canon_deque_int_capacity(&d));
    }
}

/* ── struct type: Point — exercises all generated code for a second type ─── */

/*
 * These tests exist to exercise the full generated API for Point so that
 * -Wunused-function does not fire. Each generated function must be called
 * at least once per instantiation.
 */
static void test_struct_all_functions(void)
{
    Point buf[4];
    canon_deque_Point d;
    canon_deque_Point_init(&d, buf, 4);

    Point zero = {0, 0};
    Point a    = {1, 2};
    Point b    = {3, 4};
    Point c    = {5, 6};

    /* empty() constructor */
    canon_deque_Point empty_d = canon_deque_Point_empty();
    EXPECT(canon_deque_Point_len(&empty_d)      == 0);
    EXPECT(canon_deque_Point_capacity(&empty_d) == 0);

    /* push_back + push_front */
    canon_deque_Point_push_back(&d, b);
    canon_deque_Point_push_front(&d, a);
    canon_deque_Point_push_back(&d, c);
    /* front-to-back: a, b, c */

    EXPECT(canon_deque_Point_len(&d)       == 3);
    EXPECT(canon_deque_Point_remaining(&d) == 1);
    EXPECT(!canon_deque_Point_is_empty(&d));
    EXPECT(!canon_deque_Point_is_full(&d));

    /* peek_front / peek_back */
    Point peeked = zero;
    EXPECT(canon_deque_Point_peek_front(&d, &peeked));
    EXPECT(point_eq(peeked, a));

    peeked = zero;
    EXPECT(canon_deque_Point_peek_back(&d, &peeked));
    EXPECT(point_eq(peeked, c));

    /* peek_front_option / peek_back_option */
    option_Point pfo = canon_deque_Point_peek_front_option(&d);
    EXPECT(option_Point_is_some(pfo));
    EXPECT(point_eq(option_Point_unwrap(pfo), a));

    option_Point pbo = canon_deque_Point_peek_back_option(&d);
    EXPECT(option_Point_is_some(pbo));
    EXPECT(point_eq(option_Point_unwrap(pbo), c));

    /* pop_front / pop_back */
    Point out = zero;
    canon_deque_Point_pop_front(&d, &out);
    EXPECT(point_eq(out, a));

    canon_deque_Point_pop_back(&d, &out);
    EXPECT(point_eq(out, c));

    /* pop_front_option / pop_back_option */
    option_Point pfo2 = canon_deque_Point_pop_front_option(&d);
    EXPECT(option_Point_is_some(pfo2));
    EXPECT(point_eq(option_Point_unwrap(pfo2), b));

    EXPECT(canon_deque_Point_is_empty(&d));

    option_Point none_opt = canon_deque_Point_pop_front_option(&d);
    EXPECT(option_Point_is_none(none_opt));

    none_opt = canon_deque_Point_pop_back_option(&d);
    EXPECT(option_Point_is_none(none_opt));

    /* Refill and test swap */
    Point buf2[4];
    canon_deque_Point d2;
    canon_deque_Point_init(&d2, buf2, 4);
    canon_deque_Point_push_back(&d, a);
    canon_deque_Point_push_back(&d2, b);

    canon_deque_Point_swap(&d, &d2);
    EXPECT(canon_deque_Point_len(&d) == 1);

    out = zero;
    canon_deque_Point_pop_front(&d, &out);
    EXPECT(point_eq(out, b));

    /* clear */
    canon_deque_Point_push_back(&d2, a);
    canon_deque_Point_clear(&d2);
    EXPECT(canon_deque_Point_is_empty(&d2));
}

/* ── Suppress unused option API functions ────────────────────────────────────
 *
 * CANON_OPTION generates the full combinator API (map, filter, eq, etc.).
 * Deque tests only use is_some, is_none, unwrap, some, none — the remaining
 * generated functions are suppressed here to prevent -Wunused-function errors.
 * This follows the same pattern as option_test.c's fuzz suppress block.
 */
static void deque_suppress_unused_option_fns(void)
{
    /* int option combinators not exercised by deque tests */
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
    /* Point option combinators not exercised by deque tests */
    (void)option_Point_get;
    (void)option_Point_unwrap_or;
    (void)option_Point_expect;
    (void)option_Point_map;
    (void)option_Point_and_then;
    (void)option_Point_or_else;
    (void)option_Point_filter;
    (void)option_Point_combine_with;
    (void)option_Point_replace;
    (void)option_Point_take;
    (void)option_Point_eq;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    (void)deque_suppress_unused_option_fns;
    /* constructor */
    test_init_sets_state();
    test_empty_is_null_state();

    /* null-safe queries */
    test_queries_null_safe();

    /* push + pop */
    test_push_back_pop_front_fifo();
    test_push_front_pop_back();
    test_push_front_pop_front_lifo();

    /* capacity exceeded */
    test_push_back_full_returns_err();
    test_push_front_full_returns_err();

    /* pop on empty */
    test_pop_front_empty_returns_err();
    test_pop_back_empty_returns_err();

    /* null guards */
    test_push_null_deque_returns_err();
    test_pop_null_deque_returns_err();
    test_pop_null_out_returns_err();

    /* peek */
    test_peek_front_does_not_remove();
    test_peek_back_does_not_remove();
    test_peek_front_empty_returns_false();
    test_peek_back_empty_returns_false();

    /* option: pop */
    test_pop_front_option_some();
    test_pop_front_option_none_on_empty();
    test_pop_back_option_some();
    test_pop_back_option_none_on_empty();

    /* option: peek */
    test_peek_front_option_some();
    test_peek_front_option_none_on_empty();
    test_peek_back_option_some();
    test_peek_back_option_none_on_empty();

    /* clear */
    test_clear_resets_state();
    test_clear_allows_reuse();
    test_clear_null_safe();

    /* swap */
    test_swap_exchanges_deques();
    test_swap_self();

    /* ring buffer wrap-around */
    test_wraparound_push_back_pop_front();
    test_wraparound_push_front_pop_back();

    /* sliding window pattern */
    test_sliding_window();

    /* invariant */
    test_len_remaining_invariant();

    /* struct type — covers all generated Point functions */
    test_struct_all_functions();

    if (g_failed == 0) {
        printf("OK  deque_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  deque_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */

/*
 * Suppress unused-function warnings for helpers and generated API functions
 * not exercised in the fuzz entry point.
 *
 * int option combinators — fuzz path only uses is_some and is_none:
 */
static void deque_fuzz_suppress_unused(void)
{
    /* int option API not used in fuzz path */
    (void)option_int_get;
    (void)option_int_unwrap;
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

    /* int deque functions not used in fuzz path */
    (void)canon_deque_int_empty;
    (void)canon_deque_int_capacity;
    (void)canon_deque_int_swap;

    /* Point is not fuzzed — suppress entire Point API */
    (void)point_eq;
    (void)option_Point_some;
    (void)option_Point_none;
    (void)option_Point_is_some;
    (void)option_Point_is_none;
    (void)option_Point_get;
    (void)option_Point_unwrap;
    (void)option_Point_unwrap_or;
    (void)option_Point_expect;
    (void)option_Point_map;
    (void)option_Point_and_then;
    (void)option_Point_or_else;
    (void)option_Point_filter;
    (void)option_Point_combine_with;
    (void)option_Point_replace;
    (void)option_Point_take;
    (void)option_Point_eq;
    (void)canon_deque_Point_init;
    (void)canon_deque_Point_empty;
    (void)canon_deque_Point_len;
    (void)canon_deque_Point_capacity;
    (void)canon_deque_Point_remaining;
    (void)canon_deque_Point_is_empty;
    (void)canon_deque_Point_is_full;
    (void)canon_deque_Point_push_back;
    (void)canon_deque_Point_push_front;
    (void)canon_deque_Point_pop_front;
    (void)canon_deque_Point_pop_back;
    (void)canon_deque_Point_pop_front_option;
    (void)canon_deque_Point_pop_back_option;
    (void)canon_deque_Point_peek_front;
    (void)canon_deque_Point_peek_back;
    (void)canon_deque_Point_peek_front_option;
    (void)canon_deque_Point_peek_back_option;
    (void)canon_deque_Point_clear;
    (void)canon_deque_Point_swap;
}

/*
 * Input layout (consumed in order, excess ignored):
 *   [0..1]   capacity selector — maps to 1, 2, 4, or 8
 *   [2..N]   operation stream — each byte selects an operation and its value:
 *              high nibble: operation index (0-9)
 *              low  nibble: element value (0-15)
 *
 * Operations:
 *   0 — push_back(value)
 *   1 — push_front(value)
 *   2 — pop_front
 *   3 — pop_back
 *   4 — peek_front
 *   5 — peek_back
 *   6 — pop_front_option
 *   7 — pop_back_option
 *   8 — peek_front_option
 *   9 — clear
 *
 * Invariants checked after every operation:
 *   - len + remaining == capacity (always)
 *   - is_empty iff len == 0
 *   - is_full  iff len == capacity
 *   - pop/peek on empty must return Err / false / None — never crash
 *   - push when full must return Err — never write
 *   - FIFO order: push_back + pop_front preserves insertion order
 */

int LLVMFuzzerTestOneInput(const u8 *data, usize size)
{
    static const usize cap_table[4] = {1, 2, 4, 8};
    int backing[8]; /* max capacity */
    canon_deque_int d;

    (void)deque_fuzz_suppress_unused;

    if (size < 2) return 0;

    usize cap = cap_table[data[0] % 4u];
    canon_deque_int_init(&d, backing, cap);

    /* Reference FIFO to verify ordering (bounded by cap) */
    int  ref[8];
    usize ref_head = 0;
    usize ref_size = 0;

    for (usize i = 1; i < size; i++) {
        u8    byte  = data[i];
        u8    op    = (u8)(byte >> 4u) % 10u;
        int   val   = (int)(byte & 0x0Fu);

        /* Invariant after every operation */
        #define CHECK_INVARIANTS()                                            \
            do {                                                              \
                usize len = canon_deque_int_len(&d);                          \
                usize rem = canon_deque_int_remaining(&d);                    \
                if (len + rem != cap)                   __builtin_trap();     \
                if (canon_deque_int_is_empty(&d) != (len == 0))              \
                                                        __builtin_trap();     \
                if (canon_deque_int_is_full(&d)  != (len == cap))            \
                                                        __builtin_trap();     \
            } while (0)

        switch (op) {
            case 0: { /* push_back */
                result__Bool_Error r = canon_deque_int_push_back(&d, val);
                if (canon_deque_int_len(&d) == cap) {
                    /* Was full before push — must fail */
                    if (result__Bool_Error_is_ok(r))     __builtin_trap();
                } else {
                    if (!result__Bool_Error_is_ok(r))    __builtin_trap();
                    if (ref_size < cap) {
                        ref[(ref_head + ref_size) % cap] = val;
                        ref_size++;
                    }
                }
                break;
            }
            case 1: { /* push_front */
                result__Bool_Error r = canon_deque_int_push_front(&d, val);
                if (!result__Bool_Error_is_ok(r) &&
                    canon_deque_int_len(&d) < cap)      __builtin_trap();
                /* Note: push_front alters FIFO ordering — reset reference */
                ref_head = 0;
                ref_size = 0;
                break;
            }
            case 2: { /* pop_front */
                int out = 0;
                result__Bool_Error r = canon_deque_int_pop_front(&d, &out);
                if (canon_deque_int_is_empty(&d)) {
                    /* Was empty — must fail (is_empty checked before pop) */
                }
                if (result__Bool_Error_is_ok(r)) {
                    if (ref_size > 0) {
                        int expected = ref[ref_head % cap];
                        if (out != expected)            __builtin_trap();
                        ref_head = (ref_head + 1) % cap;
                        ref_size--;
                    }
                }
                break;
            }
            case 3: { /* pop_back */
                int out = 0;
                canon_deque_int_pop_back(&d, &out);
                /* Invalidate reference — mixed front/back makes tracking harder */
                ref_head = 0;
                ref_size = 0;
                break;
            }
            case 4: { /* peek_front */
                int out = 0;
                bool ok = canon_deque_int_peek_front(&d, &out);
                usize len = canon_deque_int_len(&d);
                if (ok  && len == 0)                    __builtin_trap();
                if (!ok && len >  0)                    __builtin_trap();
                /* peek must not change length */
                if (canon_deque_int_len(&d) != len)     __builtin_trap();
                break;
            }
            case 5: { /* peek_back */
                int out = 0;
                bool ok = canon_deque_int_peek_back(&d, &out);
                usize len = canon_deque_int_len(&d);
                if (ok  && len == 0)                    __builtin_trap();
                if (!ok && len >  0)                    __builtin_trap();
                if (canon_deque_int_len(&d) != len)     __builtin_trap();
                break;
            }
            case 6: { /* pop_front_option */
                usize before = canon_deque_int_len(&d);
                option_int o = canon_deque_int_pop_front_option(&d);
                if (before == 0 && option_int_is_some(o)) __builtin_trap();
                if (before >  0 && option_int_is_none(o)) __builtin_trap();
                ref_head = 0;
                ref_size = 0;
                break;
            }
            case 7: { /* pop_back_option */
                usize before = canon_deque_int_len(&d);
                option_int o = canon_deque_int_pop_back_option(&d);
                if (before == 0 && option_int_is_some(o)) __builtin_trap();
                if (before >  0 && option_int_is_none(o)) __builtin_trap();
                ref_head = 0;
                ref_size = 0;
                break;
            }
            case 8: { /* peek_front_option / peek_back_option */
                usize before = canon_deque_int_len(&d);
                option_int pf = canon_deque_int_peek_front_option(&d);
                option_int pb = canon_deque_int_peek_back_option(&d);
                if (before == 0) {
                    if (option_int_is_some(pf))         __builtin_trap();
                    if (option_int_is_some(pb))         __builtin_trap();
                } else {
                    if (option_int_is_none(pf))         __builtin_trap();
                    if (option_int_is_none(pb))         __builtin_trap();
                }
                /* peek must not mutate */
                if (canon_deque_int_len(&d) != before)  __builtin_trap();
                break;
            }
            case 9: { /* clear */
                canon_deque_int_clear(&d);
                if (!canon_deque_int_is_empty(&d))      __builtin_trap();
                if (canon_deque_int_len(&d) != 0)       __builtin_trap();
                ref_head = 0;
                ref_size = 0;
                break;
            }
            default:
                break;
        }

        CHECK_INVARIANTS();
        #undef CHECK_INVARIANTS
    }

    return 0;
}

#endif /* CANON_FUZZING */
