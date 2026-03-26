/**
 * @file vec_test.c
 * @brief Tests for data/vec/vec.h — bounded typed vector
 *
 * Covers:
 *   - DEFINE_VEC instantiation for primitive and struct types
 *   - Constructors: init(), empty(), alloc(), free()
 *   - Queries: len(), capacity(), remaining(), is_empty(), is_full()
 *   - Element access: get(), get_option(), get_unchecked(), at(), set(),
 *                     first(), last(), data()
 *   - Push: push() — Result, try_push() — bool, push_unchecked()
 *   - Pop: pop() — Result, pop_option() — Option
 *   - Insert/remove: insert(), remove(), remove_option() — shift semantics
 *   - Bulk: append_array(), extend(), fill()
 *   - Misc: clear(), swap()
 *   - Iterator: iter_init(), iter_next()
 *   - Slice: slice_init(), slice_get()
 *   - NULL-guard paths: ERR_INVALID_ARG on NULL vec or NULL out-param
 *   - Error codes: ERR_CAPACITY_EXCEEDED, ERR_INVALID_STATE, ERR_OUT_OF_RANGE
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Exercises push, pop, insert, remove, clear under arbitrary inputs
 *   - Verifies structural invariants after every operation:
 *     len + remaining == capacity, is_empty/is_full consistency,
 *     LIFO order of push/pop preserved
 */

#define CANON_CONTRACT_IMPL
#include "semantics/option/option.h"
#include "data/vec/vec.h"

#include <stdio.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

/* Option must be instantiated before DEFINE_VEC */
CANON_OPTION(int)
DEFINE_VEC(static inline, int)

/* Second instantiation — struct type, exercises all generated code paths */
typedef struct { int x; int y; } Point;
CANON_OPTION(Point)
DEFINE_VEC(static inline, Point)

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
 * if ptr is NULL. Prevents nullPointerRedundantCheck false positives. */
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
    canon_vec_int v = canon_vec_int_init(buf, 8);

    EXPECT(canon_vec_int_len(&v)       == 0);
    EXPECT(canon_vec_int_capacity(&v)  == 8);
    EXPECT(canon_vec_int_remaining(&v) == 8);
    EXPECT(canon_vec_int_is_empty(&v));
    EXPECT(!canon_vec_int_is_full(&v));
}

/* ── Constructor: empty() ────────────────────────────────────────────────── */

static void test_empty_is_null_state(void)
{
    canon_vec_int v = canon_vec_int_empty();

    EXPECT(canon_vec_int_len(&v)       == 0);
    EXPECT(canon_vec_int_capacity(&v)  == 0);
    EXPECT(canon_vec_int_remaining(&v) == 0);
    EXPECT(canon_vec_int_is_empty(&v));
    EXPECT(canon_vec_int_is_full(&v)); /* NULL-guard: is_full returns true */
}

/* ── Constructor: alloc() / free() ──────────────────────────────────────── */

static void test_alloc_and_free(void)
{
    canon_vec_int v = canon_vec_int_alloc(4);

    EXPECT(v.items != NULL);
    EXPECT(canon_vec_int_capacity(&v) == 4);
    EXPECT(canon_vec_int_len(&v)      == 0);

    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);
    EXPECT(canon_vec_int_len(&v) == 2);

    canon_vec_int_free(&v);

    EXPECT(v.items    == NULL);
    EXPECT(v.len      == 0);
    EXPECT(v.capacity == 0);
}

static void test_alloc_zero_returns_empty(void)
{
    canon_vec_int v = canon_vec_int_alloc(0);
    EXPECT(canon_vec_int_capacity(&v) == 0);
    /* items may be NULL — do not call free on a zero-alloc */
}

/* ── NULL-safe queries ───────────────────────────────────────────────────── */

static void test_queries_null_safe(void)
{
    EXPECT(canon_vec_int_len(NULL)       == 0);
    EXPECT(canon_vec_int_capacity(NULL)  == 0);
    EXPECT(canon_vec_int_remaining(NULL) == 0);
    EXPECT(canon_vec_int_is_empty(NULL));
    EXPECT(canon_vec_int_is_full(NULL));
}

/* ── push() ──────────────────────────────────────────────────────────────── */

static void test_push_appends_in_order(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);

    result__Bool_Error r;

    r = canon_vec_int_push(&v, 10);
    EXPECT(result__Bool_Error_is_ok(r));
    r = canon_vec_int_push(&v, 20);
    EXPECT(result__Bool_Error_is_ok(r));
    r = canon_vec_int_push(&v, 30);
    EXPECT(result__Bool_Error_is_ok(r));

    EXPECT(canon_vec_int_len(&v) == 3);
    EXPECT(buf[0] == 10);
    EXPECT(buf[1] == 20);
    EXPECT(buf[2] == 30);
}

static void test_push_full_returns_err(void)
{
    int buf[2];
    canon_vec_int v = canon_vec_int_init(buf, 2);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);

    EXPECT(canon_vec_int_is_full(&v));

    result__Bool_Error r = canon_vec_int_push(&v, 3);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
    EXPECT(canon_vec_int_len(&v) == 2); /* unchanged */
}

static void test_push_null_vec_returns_err(void)
{
    result__Bool_Error r = canon_vec_int_push(NULL, 1);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── try_push() ──────────────────────────────────────────────────────────── */

static void test_try_push_returns_bool(void)
{
    int buf[2];
    canon_vec_int v = canon_vec_int_init(buf, 2);

    EXPECT(canon_vec_int_try_push(&v, 1));
    EXPECT(canon_vec_int_try_push(&v, 2));
    EXPECT(!canon_vec_int_try_push(&v, 3)); /* full */
    EXPECT(canon_vec_int_len(&v) == 2);
}

/* ── pop() ───────────────────────────────────────────────────────────────── */

static void test_pop_removes_last(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);
    canon_vec_int_push(&v, 3);

    int out = 0;
    result__Bool_Error r = canon_vec_int_pop(&v, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out == 3);
    EXPECT(canon_vec_int_len(&v) == 2);

    r = canon_vec_int_pop(&v, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out == 2);
}

static void test_pop_empty_returns_err(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);

    int out = 0;
    result__Bool_Error r = canon_vec_int_pop(&v, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_STATE);
}

static void test_pop_null_returns_err(void)
{
    int out = 0;
    result__Bool_Error r = canon_vec_int_pop(NULL, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_pop_null_out_returns_err(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);

    result__Bool_Error r = canon_vec_int_pop(&v, NULL);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── pop_option() ────────────────────────────────────────────────────────── */

static void test_pop_option_some(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 42);

    option_int o = canon_vec_int_pop_option(&v);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 42);
    EXPECT(canon_vec_int_is_empty(&v));
}

static void test_pop_option_none_on_empty(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);

    option_int o = canon_vec_int_pop_option(&v);
    EXPECT(option_int_is_none(o));
}

/* ── get() / get_option() ────────────────────────────────────────────────── */

static void test_get_reads_element(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 10);
    canon_vec_int_push(&v, 20);

    int out = 0;
    EXPECT(canon_vec_int_get(&v, 0, &out));
    EXPECT(out == 10);

    EXPECT(canon_vec_int_get(&v, 1, &out));
    EXPECT(out == 20);

    EXPECT(!canon_vec_int_get(&v, 2, &out)); /* out of range */
}

static void test_get_option_some_and_none(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 7);

    option_int o = canon_vec_int_get_option(&v, 0);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 7);

    option_int n = canon_vec_int_get_option(&v, 1);
    EXPECT(option_int_is_none(n));
}

/* ── set() ───────────────────────────────────────────────────────────────── */

static void test_set_overwrites_element(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);

    EXPECT(canon_vec_int_set(&v, 0, 99));
    int out = 0;
    canon_vec_int_get(&v, 0, &out);
    EXPECT(out == 99);

    EXPECT(!canon_vec_int_set(&v, 2, 0)); /* out of range — len==2, so index 2 is invalid */
}

/* ── first() / last() / data() ───────────────────────────────────────────── */

static void test_first_last_data(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);

    EXPECT(canon_vec_int_first(&v) == NULL); /* empty */
    EXPECT(canon_vec_int_last(&v)  == NULL);

    canon_vec_int_push(&v, 10);
    canon_vec_int_push(&v, 20);
    canon_vec_int_push(&v, 30);

    EXPECT_NOT_NULL(canon_vec_int_first(&v));
    EXPECT(*canon_vec_int_first(&v) == 10);

    EXPECT_NOT_NULL(canon_vec_int_last(&v));
    EXPECT(*canon_vec_int_last(&v) == 30);

    EXPECT(canon_vec_int_data(&v) == buf);
}

/* ── insert() ────────────────────────────────────────────────────────────── */

static void test_insert_shifts_right(void)
{
    int buf[5];
    canon_vec_int v = canon_vec_int_init(buf, 5);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);
    canon_vec_int_push(&v, 3);

    /* Insert at index 1: [1, 99, 2, 3] */
    result__Bool_Error r = canon_vec_int_insert(&v, 1, 99);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_vec_int_len(&v) == 4);

    int out = 0;
    canon_vec_int_get(&v, 0, &out); EXPECT(out == 1);
    canon_vec_int_get(&v, 1, &out); EXPECT(out == 99);
    canon_vec_int_get(&v, 2, &out); EXPECT(out == 2);
    canon_vec_int_get(&v, 3, &out); EXPECT(out == 3);
}

static void test_insert_at_end_appends(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);

    result__Bool_Error r = canon_vec_int_insert(&v, 2, 3);
    EXPECT(result__Bool_Error_is_ok(r));

    int out = 0;
    canon_vec_int_get(&v, 2, &out);
    EXPECT(out == 3);
}

static void test_insert_out_of_range_returns_err(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);

    result__Bool_Error r = canon_vec_int_insert(&v, 5, 99);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_OUT_OF_RANGE);
}

static void test_insert_full_returns_err(void)
{
    int buf[2];
    canon_vec_int v = canon_vec_int_init(buf, 2);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);

    result__Bool_Error r = canon_vec_int_insert(&v, 0, 99);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
}

/* ── remove() ────────────────────────────────────────────────────────────── */

static void test_remove_shifts_left(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);
    canon_vec_int_push(&v, 3);

    int out = 0;
    result__Bool_Error r = canon_vec_int_remove(&v, 1, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(out == 2);
    EXPECT(canon_vec_int_len(&v) == 2);

    canon_vec_int_get(&v, 0, &out); EXPECT(out == 1);
    canon_vec_int_get(&v, 1, &out); EXPECT(out == 3);
}

static void test_remove_last_element(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);

    int out = 0;
    canon_vec_int_remove(&v, 1, &out);
    EXPECT(out == 2);
    EXPECT(canon_vec_int_len(&v) == 1);
}

static void test_remove_out_of_range_returns_err(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);

    int out = 0;
    result__Bool_Error r = canon_vec_int_remove(&v, 5, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_OUT_OF_RANGE);
}

static void test_remove_empty_returns_err(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);

    int out = 0;
    result__Bool_Error r = canon_vec_int_remove(&v, 0, &out);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_STATE);
}

/* ── remove_option() ─────────────────────────────────────────────────────── */

static void test_remove_option_some(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 10);
    canon_vec_int_push(&v, 20);

    option_int o = canon_vec_int_remove_option(&v, 0);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 10);
    EXPECT(canon_vec_int_len(&v) == 1);
}

static void test_remove_option_none_out_of_range(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);

    option_int o = canon_vec_int_remove_option(&v, 5);
    EXPECT(option_int_is_none(o));
}

/* ── append_array() / extend() ───────────────────────────────────────────── */

static void test_append_array_copies_elements(void)
{
    int buf[8];
    canon_vec_int v = canon_vec_int_init(buf, 8);

    int src[] = {1, 2, 3, 4};
    result__Bool_Error r = canon_vec_int_append_array(&v, src, 4);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_vec_int_len(&v) == 4);

    int out = 0;
    canon_vec_int_get(&v, 0, &out); EXPECT(out == 1);
    canon_vec_int_get(&v, 3, &out); EXPECT(out == 4);
}

static void test_append_array_capacity_exceeded(void)
{
    int buf[2];
    canon_vec_int v = canon_vec_int_init(buf, 2);

    int src[] = {1, 2, 3};
    result__Bool_Error r = canon_vec_int_append_array(&v, src, 3);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
}

static void test_extend_is_alias_for_append_array(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);

    int src[] = {5, 6};
    result__Bool_Error r = canon_vec_int_extend(&v, src, 2);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_vec_int_len(&v) == 2);
}

/* ── fill() ──────────────────────────────────────────────────────────────── */

static void test_fill_repeats_value(void)
{
    int buf[6];
    canon_vec_int v = canon_vec_int_init(buf, 6);
    canon_vec_int_push(&v, 0); /* pre-existing element */

    canon_vec_int_fill(&v, 7, 3);

    EXPECT(canon_vec_int_len(&v) == 4);
    int out = 0;
    canon_vec_int_get(&v, 1, &out); EXPECT(out == 7);
    canon_vec_int_get(&v, 2, &out); EXPECT(out == 7);
    canon_vec_int_get(&v, 3, &out); EXPECT(out == 7);
}

static void test_fill_truncates_at_capacity(void)
{
    int buf[3];
    canon_vec_int v = canon_vec_int_init(buf, 3);

    /* Ask for 10, only 3 slots available */
    canon_vec_int_fill(&v, 1, 10);
    EXPECT(canon_vec_int_len(&v) == 3);
}

/* ── clear() ─────────────────────────────────────────────────────────────── */

static void test_clear_resets_len(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);

    canon_vec_int_clear(&v);

    EXPECT(canon_vec_int_len(&v)      == 0);
    EXPECT(canon_vec_int_capacity(&v) == 4);
    EXPECT(canon_vec_int_is_empty(&v));
}

static void test_clear_allows_reuse(void)
{
    int buf[2];
    canon_vec_int v = canon_vec_int_init(buf, 2);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);
    canon_vec_int_clear(&v);

    result__Bool_Error r = canon_vec_int_push(&v, 99);
    EXPECT(result__Bool_Error_is_ok(r));
}

static void test_clear_null_safe(void)
{
    canon_vec_int_clear(NULL); /* must not crash */
}

/* ── swap() ──────────────────────────────────────────────────────────────── */

static void test_swap_exchanges_vecs(void)
{
    int bufA[4], bufB[4];
    canon_vec_int a = canon_vec_int_init(bufA, 4);
    canon_vec_int b = canon_vec_int_init(bufB, 4);

    canon_vec_int_push(&a, 1);
    canon_vec_int_push(&a, 2);
    canon_vec_int_push(&b, 9);

    canon_vec_int_swap(&a, &b);

    EXPECT(canon_vec_int_len(&a) == 1);
    EXPECT(canon_vec_int_len(&b) == 2);

    int out = 0;
    canon_vec_int_get(&a, 0, &out); EXPECT(out == 9);
    canon_vec_int_get(&b, 0, &out); EXPECT(out == 1);
    canon_vec_int_get(&b, 1, &out); EXPECT(out == 2);
}

/* ── len + remaining invariant ───────────────────────────────────────────── */

static void test_len_remaining_invariant(void)
{
    int buf[5];
    canon_vec_int v = canon_vec_int_init(buf, 5);

    for (int i = 0; i < 5; i++) {
        EXPECT(canon_vec_int_len(&v) + canon_vec_int_remaining(&v)
               == canon_vec_int_capacity(&v));
        canon_vec_int_push(&v, i);
    }
    EXPECT(canon_vec_int_len(&v) + canon_vec_int_remaining(&v)
           == canon_vec_int_capacity(&v));

    for (int i = 0; i < 5; i++) {
        int out = 0;
        canon_vec_int_pop(&v, &out);
        EXPECT(canon_vec_int_len(&v) + canon_vec_int_remaining(&v)
               == canon_vec_int_capacity(&v));
    }
}

/* ── Iterator ────────────────────────────────────────────────────────────── */

static void test_iter_visits_all_elements(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 10);
    canon_vec_int_push(&v, 20);
    canon_vec_int_push(&v, 30);

    canon_vec_int_iter it = canon_vec_int_iter_init(&v);
    int val = 0;
    int count = 0;
    int expected[] = {10, 20, 30};

    while (canon_vec_int_iter_next(&it, &val)) {
        EXPECT(val == expected[count]);
        count++;
    }
    EXPECT(count == 3);
}

static void test_iter_empty_vec(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_iter it = canon_vec_int_iter_init(&v);

    int val = 0;
    EXPECT(!canon_vec_int_iter_next(&it, &val));
}

/* ── Slice ───────────────────────────────────────────────────────────────── */

static void test_slice_init_sub_range(void)
{
    int buf[6];
    canon_vec_int v = canon_vec_int_init(buf, 6);
    for (int i = 0; i < 6; i++) canon_vec_int_push(&v, i * 10);

    /* Slice of elements [1, 3) → values 10, 20 */
    canon_vec_int_slice s = canon_vec_int_slice_init(&v, 1, 3);
    EXPECT(s.len == 2);
    EXPECT_NOT_NULL(s.items);
    EXPECT(*canon_vec_int_slice_get(&s, 0) == 10);
    EXPECT(*canon_vec_int_slice_get(&s, 1) == 20);
}

static void test_slice_invalid_range_returns_empty(void)
{
    int buf[4];
    canon_vec_int v = canon_vec_int_init(buf, 4);
    canon_vec_int_push(&v, 1);

    canon_vec_int_slice s = canon_vec_int_slice_init(&v, 2, 1); /* start > end */
    EXPECT(s.len == 0);
}

static void test_slice_full_vec(void)
{
    int buf[3];
    canon_vec_int v = canon_vec_int_init(buf, 3);
    canon_vec_int_push(&v, 1);
    canon_vec_int_push(&v, 2);
    canon_vec_int_push(&v, 3);

    canon_vec_int_slice s = canon_vec_int_slice_init(&v, 0, 3);
    EXPECT(s.len == 3);
    EXPECT(*canon_vec_int_slice_get(&s, 2) == 3);
}

/* ── Point type — exercises all generated code for the second instantiation ── */

/*
 * These tests ensure the full generated API for Point compiles and runs
 * correctly, and prevent -Wunused-function warnings.
 */
static void test_struct_all_functions(void)
{
    Point buf[4];
    canon_vec_Point v = canon_vec_Point_init(buf, 4);

    Point zero = {0, 0};
    Point a    = {1, 2};
    Point b    = {3, 4};
    Point c    = {5, 6};

    /* empty() */
    canon_vec_Point empty_v = canon_vec_Point_empty();
    EXPECT(canon_vec_Point_len(&empty_v)      == 0);
    EXPECT(canon_vec_Point_capacity(&empty_v) == 0);

    /* push / len / capacity / remaining / is_empty / is_full */
    canon_vec_Point_push(&v, a);
    canon_vec_Point_push(&v, b);
    canon_vec_Point_push(&v, c);
    EXPECT(canon_vec_Point_len(&v)       == 3);
    EXPECT(canon_vec_Point_remaining(&v) == 1);
    EXPECT(!canon_vec_Point_is_empty(&v));
    EXPECT(!canon_vec_Point_is_full(&v));

    /* get */
    Point out = zero;
    EXPECT(canon_vec_Point_get(&v, 0, &out));
    EXPECT(point_eq(out, a));

    /* get_option */
    option_Point go = canon_vec_Point_get_option(&v, 1);
    EXPECT(option_Point_is_some(go));
    EXPECT(point_eq(option_Point_unwrap(go), b));
    EXPECT(option_Point_is_none(canon_vec_Point_get_option(&v, 99)));

    /* get_unchecked */
    EXPECT(point_eq(canon_vec_Point_get_unchecked(&v, 2), c));

    /* at / set */
    Point* ptr = canon_vec_Point_at(&v, 0);
    EXPECT_NOT_NULL(ptr);
    EXPECT(point_eq(*ptr, a));
    EXPECT(canon_vec_Point_set(&v, 0, b));
    canon_vec_Point_get(&v, 0, &out);
    EXPECT(point_eq(out, b));
    canon_vec_Point_set(&v, 0, a); /* restore */

    /* first / last / data */
    EXPECT_NOT_NULL(canon_vec_Point_first(&v));
    EXPECT(point_eq(*canon_vec_Point_first(&v), a));
    EXPECT_NOT_NULL(canon_vec_Point_last(&v));
    EXPECT(point_eq(*canon_vec_Point_last(&v), c));
    EXPECT(canon_vec_Point_data(&v) == buf);

    /* try_push */
    EXPECT(canon_vec_Point_try_push(&v, zero)); /* fills slot 4 */
    EXPECT(canon_vec_Point_is_full(&v));
    EXPECT(!canon_vec_Point_try_push(&v, zero)); /* full */

    /* pop_option */
    option_Point po = canon_vec_Point_pop_option(&v);
    EXPECT(option_Point_is_some(po));
    EXPECT(point_eq(option_Point_unwrap(po), zero));

    /* pop (Result) */
    result__Bool_Error r = canon_vec_Point_pop(&v, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(point_eq(out, c));
    EXPECT(canon_vec_Point_len(&v) == 2); /* a, b remain */

    /* insert */
    r = canon_vec_Point_insert(&v, 1, c);
    EXPECT(result__Bool_Error_is_ok(r));
    canon_vec_Point_get(&v, 1, &out);
    EXPECT(point_eq(out, c));

    /* remove */
    r = canon_vec_Point_remove(&v, 1, &out);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(point_eq(out, c));

    /* remove_option */
    option_Point ro = canon_vec_Point_remove_option(&v, 0);
    EXPECT(option_Point_is_some(ro));
    EXPECT(point_eq(option_Point_unwrap(ro), a));

    /* append_array */
    Point src[2] = {{10, 20}, {30, 40}};
    r = canon_vec_Point_append_array(&v, src, 2);
    EXPECT(result__Bool_Error_is_ok(r));

    /* extend */
    canon_vec_Point_clear(&v);
    r = canon_vec_Point_extend(&v, src, 2);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(canon_vec_Point_len(&v) == 2);

    /* fill */
    canon_vec_Point_clear(&v);
    canon_vec_Point_fill(&v, zero, 3);
    EXPECT(canon_vec_Point_len(&v) == 3);

    /* swap */
    Point buf2[4];
    canon_vec_Point v2 = canon_vec_Point_init(buf2, 4);
    canon_vec_Point_push(&v2, a);
    canon_vec_Point_swap(&v, &v2);
    EXPECT(canon_vec_Point_len(&v) == 1);
    canon_vec_Point_get(&v, 0, &out);
    EXPECT(point_eq(out, a));

    /* iterator */
    canon_vec_Point_clear(&v);
    canon_vec_Point_push(&v, a);
    canon_vec_Point_push(&v, b);
    canon_vec_Point_iter it = canon_vec_Point_iter_init(&v);
    Point itval = zero;
    int itcount = 0;
    while (canon_vec_Point_iter_next(&it, &itval)) { itcount++; }
    EXPECT(itcount == 2);

    /* slice */
    canon_vec_Point_slice s = canon_vec_Point_slice_init(&v, 0, 2);
    EXPECT(s.len == 2);
    EXPECT_NOT_NULL(canon_vec_Point_slice_get(&s, 0));

    /* push_unchecked */
    canon_vec_Point_clear(&v);
    canon_vec_Point_push_unchecked(&v, a);
    EXPECT(canon_vec_Point_len(&v) == 1);

    /* free (heap-allocated) */
    canon_vec_Point heap_v = canon_vec_Point_alloc(2);
    EXPECT(heap_v.items != NULL);
    canon_vec_Point_push(&heap_v, a);
    canon_vec_Point_free(&heap_v);
    EXPECT(heap_v.items == NULL);
}

/* ── Suppress unused option API functions ────────────────────────────────── */

static void vec_suppress_unused_option_fns(void)
{
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

    /* int vec functions only exercised via the Point path in test_struct_all_functions */
    (void)canon_vec_int_arena_alloc;
    (void)canon_vec_int_get_unchecked;
    (void)canon_vec_int_at;
    (void)canon_vec_int_push_unchecked;

    /* Point arena_alloc — no Arena available in test context */
    (void)canon_vec_Point_arena_alloc;

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
    (void)vec_suppress_unused_option_fns;

    /* constructor */
    test_init_sets_state();
    test_empty_is_null_state();
    test_alloc_and_free();
    test_alloc_zero_returns_empty();

    /* null-safe queries */
    test_queries_null_safe();

    /* push */
    test_push_appends_in_order();
    test_push_full_returns_err();
    test_push_null_vec_returns_err();

    /* try_push */
    test_try_push_returns_bool();

    /* pop */
    test_pop_removes_last();
    test_pop_empty_returns_err();
    test_pop_null_returns_err();
    test_pop_null_out_returns_err();

    /* pop_option */
    test_pop_option_some();
    test_pop_option_none_on_empty();

    /* get / get_option */
    test_get_reads_element();
    test_get_option_some_and_none();

    /* set */
    test_set_overwrites_element();

    /* first / last / data */
    test_first_last_data();

    /* insert */
    test_insert_shifts_right();
    test_insert_at_end_appends();
    test_insert_out_of_range_returns_err();
    test_insert_full_returns_err();

    /* remove */
    test_remove_shifts_left();
    test_remove_last_element();
    test_remove_out_of_range_returns_err();
    test_remove_empty_returns_err();

    /* remove_option */
    test_remove_option_some();
    test_remove_option_none_out_of_range();

    /* append_array / extend */
    test_append_array_copies_elements();
    test_append_array_capacity_exceeded();
    test_extend_is_alias_for_append_array();

    /* fill */
    test_fill_repeats_value();
    test_fill_truncates_at_capacity();

    /* clear */
    test_clear_resets_len();
    test_clear_allows_reuse();
    test_clear_null_safe();

    /* swap */
    test_swap_exchanges_vecs();

    /* invariant */
    test_len_remaining_invariant();

    /* iterator */
    test_iter_visits_all_elements();
    test_iter_empty_vec();

    /* slice */
    test_slice_init_sub_range();
    test_slice_invalid_range_returns_empty();
    test_slice_full_vec();

    /* struct type — covers all generated Point functions */
    test_struct_all_functions();

    if (g_failed == 0) {
        printf("OK  vec_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  vec_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */

/*
 * Suppress unused functions in the fuzz build.
 * Only push, pop, insert, remove, clear are exercised in the fuzz path.
 */
static void vec_fuzz_suppress_unused(void)
{
    /* int option API */
    (void)option_int_is_some;
    (void)option_int_is_none;
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

    /* int vec functions not used in fuzz path */
    (void)canon_vec_int_empty;
    (void)canon_vec_int_alloc;
    (void)canon_vec_int_arena_alloc;
    (void)canon_vec_int_free;
    (void)canon_vec_int_capacity;
    (void)canon_vec_int_get_option;
    (void)canon_vec_int_get_unchecked;
    (void)canon_vec_int_at;
    (void)canon_vec_int_set;
    (void)canon_vec_int_first;
    (void)canon_vec_int_last;
    (void)canon_vec_int_data;
    (void)canon_vec_int_try_push;
    (void)canon_vec_int_push_unchecked;
    (void)canon_vec_int_pop_option;
    (void)canon_vec_int_remove_option;
    (void)canon_vec_int_append_array;
    (void)canon_vec_int_extend;
    (void)canon_vec_int_fill;
    (void)canon_vec_int_swap;
    (void)canon_vec_int_iter_init;
    (void)canon_vec_int_iter_next;
    (void)canon_vec_int_slice_init;
    (void)canon_vec_int_slice_get;

    /* Point type — not fuzzed at all */
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
    (void)canon_vec_Point_init;
    (void)canon_vec_Point_empty;
    (void)canon_vec_Point_alloc;
    (void)canon_vec_Point_arena_alloc;
    (void)canon_vec_Point_free;
    (void)canon_vec_Point_len;
    (void)canon_vec_Point_capacity;
    (void)canon_vec_Point_remaining;
    (void)canon_vec_Point_is_empty;
    (void)canon_vec_Point_is_full;
    (void)canon_vec_Point_get;
    (void)canon_vec_Point_get_option;
    (void)canon_vec_Point_get_unchecked;
    (void)canon_vec_Point_at;
    (void)canon_vec_Point_set;
    (void)canon_vec_Point_first;
    (void)canon_vec_Point_last;
    (void)canon_vec_Point_data;
    (void)canon_vec_Point_push;
    (void)canon_vec_Point_try_push;
    (void)canon_vec_Point_push_unchecked;
    (void)canon_vec_Point_pop;
    (void)canon_vec_Point_pop_option;
    (void)canon_vec_Point_clear;
    (void)canon_vec_Point_insert;
    (void)canon_vec_Point_remove;
    (void)canon_vec_Point_remove_option;
    (void)canon_vec_Point_append_array;
    (void)canon_vec_Point_extend;
    (void)canon_vec_Point_fill;
    (void)canon_vec_Point_swap;
    (void)canon_vec_Point_iter_init;
    (void)canon_vec_Point_iter_next;
    (void)canon_vec_Point_slice_init;
    (void)canon_vec_Point_slice_get;
}

/*
 * Input layout (consumed in order, excess ignored):
 *   [0]    capacity selector — maps to 1, 2, 4, or 8
 *   [1..N] operation stream — each byte:
 *            high nibble: operation index (0-6)
 *            low  nibble: element value (0-15)
 *
 * Operations:
 *   0 — push(value)
 *   1 — pop
 *   2 — insert(index=value%len, value) — only when non-empty
 *   3 — remove(index=value%len)        — only when non-empty
 *   4 — get(index=value%len)           — only when non-empty
 *   5 — set(index=value%len, value+1)  — only when non-empty
 *   6 — clear
 *
 * Invariants checked after every operation:
 *   - len + remaining == capacity (always)
 *   - is_empty iff len == 0
 *   - is_full  iff len == capacity
 *   - push when full must fail
 *   - pop/remove on empty must fail
 *   - get/set within bounds must succeed
 *   - LIFO order: values pushed are popped in reverse order
 */

int LLVMFuzzerTestOneInput(const u8 *data, usize size)
{
    static const usize cap_table[4] = {1, 2, 4, 8};
    int backing[8];
    canon_vec_int v;

    (void)vec_fuzz_suppress_unused;

    if (size < 2) return 0;

    usize cap = cap_table[data[0] % 4u];
    v = canon_vec_int_init(backing, cap);

    /* LIFO reference stack to verify push/pop ordering */
    int  ref[8];
    usize ref_top = 0; /* number of elements in ref */

    for (usize i = 1; i < size; i++) {
        u8  byte = data[i];
        u8  op   = (u8)(byte >> 4u) % 7u;
        int val  = (int)(byte & 0x0Fu);

        #define CHECK_INVARIANTS()                                            \
            do {                                                              \
                usize len = canon_vec_int_len(&v);                            \
                usize rem = canon_vec_int_remaining(&v);                      \
                if (len + rem != cap)                   __builtin_trap();     \
                if (canon_vec_int_is_empty(&v) != (len == 0))                \
                                                        __builtin_trap();     \
                if (canon_vec_int_is_full(&v)  != (len == cap))              \
                                                        __builtin_trap();     \
            } while (0)

        switch (op) {
            case 0: { /* push */
                usize before = canon_vec_int_len(&v);
                result__Bool_Error r = canon_vec_int_push(&v, val);
                if (before >= cap) {
                    if (result__Bool_Error_is_ok(r))    __builtin_trap();
                } else {
                    if (!result__Bool_Error_is_ok(r))   __builtin_trap();
                    if (ref_top < cap) ref[ref_top++] = val;
                }
                break;
            }
            case 1: { /* pop */
                int out = 0;
                result__Bool_Error r = canon_vec_int_pop(&v, &out);
                if (result__Bool_Error_is_ok(r)) {
                    if (canon_vec_int_is_empty(&v) &&
                        canon_vec_int_len(&v) != 0)     __builtin_trap();
                    /* verify LIFO ordering against ref */
                    if (ref_top > 0) {
                        ref_top--;
                        if (out != ref[ref_top])        __builtin_trap();
                    }
                } else {
                    if (canon_vec_int_len(&v) != 0)     __builtin_trap();
                }
                break;
            }
            case 2: { /* insert at position */
                usize len = canon_vec_int_len(&v);
                if (len == 0) break;
                usize idx = (usize)val % (len + 1);
                result__Bool_Error r = canon_vec_int_insert(&v, idx, val);
                if (len >= cap) {
                    if (result__Bool_Error_is_ok(r))    __builtin_trap();
                } else {
                    if (!result__Bool_Error_is_ok(r))   __builtin_trap();
                }
                /* ref is now stale — any insert breaks LIFO ordering */
                ref_top = 0;
                break;
            }
            case 3: { /* remove */
                usize len = canon_vec_int_len(&v);
                if (len == 0) break;
                usize idx = (usize)val % len;
                int out = 0;
                result__Bool_Error r = canon_vec_int_remove(&v, idx, &out);
                if (!result__Bool_Error_is_ok(r))       __builtin_trap();
                ref_top = 0; /* stale after remove */
                break;
            }
            case 4: { /* get */
                usize len = canon_vec_int_len(&v);
                if (len == 0) break;
                usize idx = (usize)val % len;
                int out = 0;
                if (!canon_vec_int_get(&v, idx, &out))  __builtin_trap();
                /* get must not mutate len */
                if (canon_vec_int_len(&v) != len)       __builtin_trap();
                break;
            }
            case 5: { /* set */
                usize len = canon_vec_int_len(&v);
                if (len == 0) break;
                usize idx = (usize)val % len;
                if (!canon_vec_int_set(&v, idx, val + 1)) __builtin_trap();
                if (canon_vec_int_len(&v) != len)         __builtin_trap();
                ref_top = 0; /* set changed a value, ref is stale */
                break;
            }
            case 6: { /* clear */
                canon_vec_int_clear(&v);
                if (!canon_vec_int_is_empty(&v))        __builtin_trap();
                if (canon_vec_int_len(&v) != 0)         __builtin_trap();
                ref_top = 0;
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
