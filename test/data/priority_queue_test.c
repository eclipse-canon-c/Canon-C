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
 * @file priority_queue_test.c
 * @brief Tests for data/priority_queue.h — binary min-heap with caller-owned buffer
 *
 * Covers:
 *   - pq_init()               — valid initialization
 *   - pq_push_result()        — success, capacity exceeded, NULL args
 *   - pq_pop_raw()            — min extraction, empty queue
 *   - pq_peek_raw()           — min inspection without removal, empty queue
 *   - pq_remove_at_result()   — valid index, out-of-range, NULL
 *   - pq_heapify()            — Floyd's O(n) in-place heap build
 *   - Queries                 — len, capacity, remaining, is_empty, is_full,
 *                               as_bytes
 *   - Min-heap ordering       — pop always returns minimum element
 *   - Max-heap ordering       — descending comparator, pop returns maximum
 *   - DEFINE_PRIORITY_QUEUE(int)   — typed wrapper (pq_int), full API coverage
 *   - DEFINE_PRIORITY_QUEUE(Score) — struct type for generated-code coverage
 *   - CANON_LIFETIME_DEBUG: init opens, mutating ops restamp, queries
 *     preserve the id. No close test — PQ has no destructor (caller owns
 *     buffer and struct).
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Values drawn from low nibble (0–15); all pushes use that value
 *   - ref_count[16] tracks how many of each value are currently in the heap
 *   - After every operation:
 *       • Structural heap invariant: parent <= every child (min-heap)
 *       • Peek must equal the minimum value present in ref_count
 *       • is_empty must agree with len == 0
 *   - Operations: push, pop, peek, remove_at(0), remove_at(len/2), clear
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "core/primitives/compare.h"
#include "semantics/option/option.h"
#include "data/priority_queue.h"

#include <stdio.h>
#include <string.h>

/* ── Comparators ─────────────────────────────────────────────────────────── */

/* They return int, as algo_cmp_fn does. They returned i32, which is int on
 * the hosted targets but long on arm-none-eabi (newlib's int32_t), where
 * passing them was an incompatible-pointer error (VERIFY-046 A2). */

static int cmp_int_asc(const void* a, const void* b, void* ctx) {
    (void)ctx;
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

static int cmp_int_desc(const void* a, const void* b, void* ctx) {
    (void)ctx;
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ib > ia) - (ib < ia);
}

/* ── Type instantiations ─────────────────────────────────────────────────── */

CANON_OPTION(int)
DEFINE_PRIORITY_QUEUE(int)

/* Second instantiation — struct type, exercises all generated code paths */
typedef struct { int priority; int id; } Score;

static int cmp_score(const void* a, const void* b, void* ctx) {
    (void)ctx;
    int pa = ((const Score*)a)->priority;
    int pb = ((const Score*)b)->priority;
    return (pa > pb) - (pa < pb);
}

CANON_OPTION(Score)
DEFINE_PRIORITY_QUEUE(Score)

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

/* ── pq_init — valid ─────────────────────────────────────────────────────── */

static void test_init_valid(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    EXPECT(pq_int_len(&h)       == 0);
    EXPECT(pq_int_capacity(&h)  == 8);
    EXPECT(pq_int_remaining(&h) == 8);
    EXPECT(pq_int_is_empty(&h));
    EXPECT(!pq_int_is_full(&h));
}

/* ── min-heap ordering ───────────────────────────────────────────────────── */

static void test_min_heap_ordering(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    /* Push out-of-order values */
    pq_int_push_result(&h, 50);
    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 30);
    pq_int_push_result(&h, 5);
    pq_int_push_result(&h, 20);

    EXPECT(pq_int_len(&h) == 5);

    /* Pop must return values in ascending order */
    int prev = -1;
    for (int i = 0; i < 5; i++) {
        option_int o = pq_int_pop_option(&h);
        EXPECT(option_int_is_some(o));
        int v = option_int_unwrap(o);
        EXPECT(v > prev);
        prev = v;
    }
    EXPECT(pq_int_is_empty(&h));
    /* next pop on empty returns None */
    EXPECT(option_int_is_none(pq_int_pop_option(&h)));
}

/* ── max-heap via descending comparator ─────────────────────────────────── */

static void test_max_heap_ordering(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_desc, NULL);

    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 50);
    pq_int_push_result(&h, 30);
    pq_int_push_result(&h, 5);
    pq_int_push_result(&h, 40);

    /* Pop must return values in descending order */
    int prev = 999;
    for (int i = 0; i < 5; i++) {
        option_int o = pq_int_pop_option(&h);
        EXPECT(option_int_is_some(o));
        int v = option_int_unwrap(o);
        EXPECT(v < prev);
        prev = v;
    }
    EXPECT(pq_int_is_empty(&h));
}

/* ── peek does not remove ────────────────────────────────────────────────── */

static void test_peek_no_remove(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    pq_int_push_result(&h, 99);
    pq_int_push_result(&h, 1);
    pq_int_push_result(&h, 50);

    EXPECT(pq_int_len(&h) == 3);

    /* peek_option twice — same result each time, len unchanged */
    option_int o1 = pq_int_peek_option(&h);
    EXPECT(option_int_is_some(o1));
    EXPECT(option_int_unwrap(o1) == 1);
    EXPECT(pq_int_len(&h) == 3);

    option_int o2 = pq_int_peek_option(&h);
    EXPECT(option_int_is_some(o2));
    EXPECT(option_int_unwrap(o2) == 1);
    EXPECT(pq_int_len(&h) == 3);

    /* peek_raw also non-destructive */
    const int* raw = (const int*)pq_peek_raw(&h._pq);
    EXPECT_NOT_NULL(raw);
    EXPECT(*raw == 1);
    EXPECT(pq_int_len(&h) == 3);
}

/* ── push — capacity exceeded ────────────────────────────────────────────── */

static void test_push_capacity_exceeded(void)
{
    /* cap=4, push 4 elements (allowed), 5th must fail */
    int buf[4];
    pq_int h;
    pq_int_init(&h, buf, 4, cmp_int_asc, NULL);

    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 20);
    pq_int_push_result(&h, 30);

    result__Bool_Error r = pq_int_push_result(&h, 40);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(pq_int_is_full(&h));

    r = pq_int_push_result(&h, 50);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
    EXPECT(pq_int_len(&h) == 4); /* unchanged */
}

/* ── push — NULL args ────────────────────────────────────────────────────── */

static void test_push_null_args(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);
    int v = 42;

    /* NULL pq */
    result__Bool_Error r = pq_push_result(NULL, &v);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    /* NULL elem */
    r = pq_push_result(&h._pq, NULL);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── pop_raw — empty queue ───────────────────────────────────────────────── */

static void test_pop_raw_empty(void)
{
    int buf[4];
    pq_int h;
    pq_int_init(&h, buf, 4, cmp_int_asc, NULL);

    int out = 999;
    EXPECT(!pq_pop_raw(&h._pq, &out));
    EXPECT(out == 999); /* out not written */

    /* NULL pq also returns false */
    EXPECT(!pq_pop_raw(NULL, &out));
}

/* ── peek_raw — empty queue ──────────────────────────────────────────────── */

static void test_peek_raw_empty(void)
{
    int buf[4];
    pq_int h;
    pq_int_init(&h, buf, 4, cmp_int_asc, NULL);

    EXPECT(pq_peek_raw(&h._pq) == NULL);
    EXPECT(pq_peek_raw(NULL)   == NULL);
}

/* ── remove_at — valid index ─────────────────────────────────────────────── */

static void test_remove_at_valid(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    pq_int_push_result(&h, 30);
    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 50);
    pq_int_push_result(&h, 20);
    pq_int_push_result(&h, 40);
    EXPECT(pq_int_len(&h) == 5);

    /* Remove element at index 1 (somewhere in the middle of the heap) */
    result__Bool_Error r = pq_int_remove_at_result(&h, 1);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(pq_int_len(&h) == 4);

    /* Remaining 4 elements must still pop in sorted order */
    int prev = -1;
    for (int i = 0; i < 4; i++) {
        option_int o = pq_int_pop_option(&h);
        EXPECT(option_int_is_some(o));
        int v = option_int_unwrap(o);
        EXPECT(v >= prev);
        prev = v;
    }
    EXPECT(pq_int_is_empty(&h));
}

/* ── remove_at — out-of-range ────────────────────────────────────────────── */

static void test_remove_at_out_of_range(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    /* empty heap: any index is out of range */
    result__Bool_Error r = pq_int_remove_at_result(&h, 0);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_OUT_OF_RANGE);

    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 20);

    /* index == len is out of range */
    r = pq_int_remove_at_result(&h, 2);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_OUT_OF_RANGE);
}

/* ── remove_at — NULL pq ─────────────────────────────────────────────────── */

static void test_remove_at_null(void)
{
    result__Bool_Error r = pq_remove_at_result(NULL, 0);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(result__Bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── pq_heapify — build from unsorted array ─────────────────────────────── */

static void test_heapify(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    /* Write values in reverse order directly into the backing buffer.
     * Since h._pq.data == buf, this pre-populates the heap storage. */
    int values[5] = {50, 30, 40, 10, 20};
    memcpy(buf, values, 5 * sizeof(int));

    pq_int_heapify(&h, 5);
    EXPECT(pq_int_len(&h) == 5);

    /* peek must be the minimum */
    option_int top = pq_int_peek_option(&h);
    EXPECT(option_int_is_some(top));
    EXPECT(option_int_unwrap(top) == 10);

    /* pop must yield ascending order */
    int prev = -1;
    for (int i = 0; i < 5; i++) {
        option_int o = pq_int_pop_option(&h);
        EXPECT(option_int_is_some(o));
        int v = option_int_unwrap(o);
        EXPECT(v > prev);
        prev = v;
    }
    EXPECT(pq_int_is_empty(&h));

    /* heapify 0 resets len to 0 */
    pq_int_heapify(&h, 0);
    EXPECT(pq_int_len(&h) == 0);

    /* heapify clamps to capacity */
    memset(buf, 0, sizeof(buf));
    pq_int_heapify(&h, 999);
    EXPECT(pq_int_len(&h) == 8);
}

/* ── queries ─────────────────────────────────────────────────────────────── */

static void test_queries(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    /* empty state */
    EXPECT(pq_int_len(&h)       == 0);
    EXPECT(pq_int_capacity(&h)  == 8);
    EXPECT(pq_int_remaining(&h) == 8);
    EXPECT(pq_int_is_empty(&h));
    EXPECT(!pq_int_is_full(&h));

    bytes_t b = pq_int_as_bytes(&h);
    EXPECT(b.len == 0);

    /* push 3 */
    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 20);
    pq_int_push_result(&h, 30);

    EXPECT(pq_int_len(&h)       == 3);
    EXPECT(pq_int_capacity(&h)  == 8);
    EXPECT(pq_int_remaining(&h) == 5);
    EXPECT(!pq_int_is_empty(&h));
    EXPECT(!pq_int_is_full(&h));

    b = pq_int_as_bytes(&h);
    EXPECT(b.len == 3 * sizeof(int));

    /* fill to capacity */
    pq_int_push_result(&h, 40);
    pq_int_push_result(&h, 50);
    pq_int_push_result(&h, 60);
    pq_int_push_result(&h, 70);
    pq_int_push_result(&h, 80);

    EXPECT(pq_int_is_full(&h));
    EXPECT(pq_int_remaining(&h) == 0);
    EXPECT(pq_int_len(&h) == 8);
}

/* ── struct type — exercises all generated Score functions ───────────────── */

static void test_struct_type(void)
{
    Score buf[8];
    pq_Score h;
    pq_Score_init(&h, buf, 8, cmp_score, NULL);

    EXPECT(pq_Score_is_empty(&h));
    EXPECT(pq_Score_len(&h)       == 0);
    EXPECT(pq_Score_capacity(&h)  == 8);
    EXPECT(pq_Score_remaining(&h) == 8);
    EXPECT(!pq_Score_is_full(&h));

    /* push three scores */
    Score s1 = {30, 1};
    Score s2 = {10, 2};
    Score s3 = {20, 3};

    result__Bool_Error r;
    r = pq_Score_push_result(&h, s1); EXPECT(result__Bool_Error_is_ok(r));
    r = pq_Score_push_result(&h, s2); EXPECT(result__Bool_Error_is_ok(r));
    r = pq_Score_push_result(&h, s3); EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(pq_Score_len(&h) == 3);

    /* peek — must be minimum priority */
    option_Score peek = pq_Score_peek_option(&h);
    EXPECT(option_Score_is_some(peek));
    EXPECT(option_Score_unwrap(peek).priority == 10);
    EXPECT(pq_Score_len(&h) == 3); /* peek is non-destructive */

    /* pop all — ascending priority order */
    int prev_prio = -1;
    for (int i = 0; i < 3; i++) {
        option_Score o = pq_Score_pop_option(&h);
        EXPECT(option_Score_is_some(o));
        Score s = option_Score_unwrap(o);
        EXPECT(s.priority >= prev_prio);
        prev_prio = s.priority;
    }
    EXPECT(pq_Score_is_empty(&h));
    EXPECT(option_Score_is_none(pq_Score_pop_option(&h)));

    /* heapify from pre-filled buffer */
    Score scores[3] = {{50, 0}, {5, 1}, {25, 2}};
    memcpy(buf, scores, 3 * sizeof(Score));
    pq_Score_heapify(&h, 3);
    EXPECT(pq_Score_len(&h) == 3);

    option_Score top = pq_Score_peek_option(&h);
    EXPECT(option_Score_is_some(top));
    EXPECT(option_Score_unwrap(top).priority == 5);

    /* remove_at_result */
    r = pq_Score_remove_at_result(&h, 0);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(pq_Score_len(&h) == 2);

    /* as_bytes */
    bytes_t b = pq_Score_as_bytes(&h);
    EXPECT(b.len == 2 * sizeof(Score));
}

/* ── CANON_LIFETIME_DEBUG: lifetime token semantics ──────────────────────── *
 *
 * State-level assertions on h._pq.lt. Borrow-side end-to-end validation
 * (firing require_msg via lifetime_assert_valid) is covered in Phase 4
 * tests in borrow_test.c.
 *
 * Contract per the file docblock:
 *   - pq_init opens the lifetime: lt.open == true, lt.id derived from
 *     a per-TU counter XOR'd with pq's address.
 *   - PQ has NO destructor — no close test. Lifetime stays open for the
 *     life of the struct.
 *   - RESTAMP on every mutating operation: push, pop, remove_at, heapify.
 *     The id changes; lt.open stays true.
 *   - Peek and queries do NOT restamp.
 */

#ifdef CANON_LIFETIME_DEBUG

static void test_lifetime_init_opens_token(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);
    EXPECT(h._pq.lt.open == true);
    EXPECT(h._pq.lt.id != REGION_ID_STATIC);
}

static void test_lifetime_push_restamps(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);
    region_id_t id_before = h._pq.lt.id;

    pq_int_push_result(&h, 42);
    EXPECT(h._pq.lt.id != id_before);
    EXPECT(h._pq.lt.open == true);

    /* successive pushes each produce a fresh id */
    region_id_t id_after_first = h._pq.lt.id;
    pq_int_push_result(&h, 7);
    EXPECT(h._pq.lt.id != id_after_first);

    /* error path (capacity exceeded) does NOT restamp */
    int small_buf[2];
    pq_int small;
    pq_int_init(&small, small_buf, 2, cmp_int_asc, NULL);
    pq_int_push_result(&small, 1);
    pq_int_push_result(&small, 2);
    region_id_t id_before_overflow = small._pq.lt.id;
    result__Bool_Error r = pq_int_push_result(&small, 3);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(small._pq.lt.id == id_before_overflow); /* error path preserves id */
}

static void test_lifetime_pop_restamps(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);
    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 20);
    pq_int_push_result(&h, 5);

    region_id_t id_before = h._pq.lt.id;
    option_int popped = pq_int_pop_option(&h);
    EXPECT(option_int_is_some(popped));
    EXPECT(h._pq.lt.id != id_before);
    EXPECT(h._pq.lt.open == true);

    /* pop on empty does NOT restamp — nothing changed */
    int empty_buf[4];
    pq_int empty_h;
    pq_int_init(&empty_h, empty_buf, 4, cmp_int_asc, NULL);
    region_id_t empty_id = empty_h._pq.lt.id;
    option_int none = pq_int_pop_option(&empty_h);
    EXPECT(option_int_is_none(none));
    EXPECT(empty_h._pq.lt.id == empty_id); /* preserved on no-op */
}

static void test_lifetime_remove_at_restamps(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);
    pq_int_push_result(&h, 30);
    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 50);
    pq_int_push_result(&h, 20);

    region_id_t id_before = h._pq.lt.id;
    result__Bool_Error r = pq_int_remove_at_result(&h, 1);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(h._pq.lt.id != id_before);
    EXPECT(h._pq.lt.open == true);

    /* removing the last element still restamps (composition changed) */
    region_id_t id_mid = h._pq.lt.id;
    r = pq_int_remove_at_result(&h, pq_int_len(&h) - 1);
    EXPECT(result__Bool_Error_is_ok(r));
    EXPECT(h._pq.lt.id != id_mid);

    /* out-of-range does NOT restamp */
    region_id_t id_stable = h._pq.lt.id;
    r = pq_int_remove_at_result(&h, 999);
    EXPECT(!result__Bool_Error_is_ok(r));
    EXPECT(h._pq.lt.id == id_stable);
}

static void test_lifetime_heapify_restamps(void)
{
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);

    int values[5] = {50, 30, 40, 10, 20};
    memcpy(buf, values, 5 * sizeof(int));

    region_id_t id_before = h._pq.lt.id;
    pq_int_heapify(&h, 5);
    EXPECT(h._pq.lt.id != id_before);
    EXPECT(h._pq.lt.open == true);

    /* heapify(0) — empties the heap; still restamps (state changed) */
    region_id_t id_mid = h._pq.lt.id;
    pq_int_heapify(&h, 0);
    EXPECT(h._pq.lt.id != id_mid);
}

static void test_lifetime_queries_preserve_id(void)
{
    /* Non-mutating operations must not touch lt. This documents the
     * contract — if peek or any query accidentally restamps, this fires. */
    int buf[8];
    pq_int h;
    pq_int_init(&h, buf, 8, cmp_int_asc, NULL);
    pq_int_push_result(&h, 10);
    pq_int_push_result(&h, 20);

    region_id_t id_before = h._pq.lt.id;

    /* peek variants */
    (void)pq_int_peek_option(&h);
    EXPECT(h._pq.lt.id == id_before);
    (void)pq_peek_raw(&h._pq);
    EXPECT(h._pq.lt.id == id_before);

    /* queries */
    (void)pq_int_len(&h);
    (void)pq_int_capacity(&h);
    (void)pq_int_remaining(&h);
    (void)pq_int_is_empty(&h);
    (void)pq_int_is_full(&h);
    (void)pq_int_as_bytes(&h);
    EXPECT(h._pq.lt.id == id_before);
}

#endif /* CANON_LIFETIME_DEBUG */

/* ── Suppress unused generated functions ────────────────────────────────── */

static void pq_suppress_unused(void)
{
    /* Raw untyped legacy bool wrappers — result variants preferred */
    (void)pq_push;
    (void)pq_pop;
    (void)pq_peek;
    (void)pq_remove_at;

    /* pq_int legacy bool wrappers */
    (void)pq_int_push;
    (void)pq_int_pop;
    (void)pq_int_peek;
    (void)pq_int_remove_at;

    /* pq_Score legacy bool wrappers */
    (void)pq_Score_push;
    (void)pq_Score_pop;
    (void)pq_Score_peek;
    (void)pq_Score_remove_at;

    /* result__Bool_Error combinators not tested here
     * (covered by result_test) */
    (void)result__Bool_Error_get_ok;
    (void)result__Bool_Error_get_err;
    (void)result__Bool_Error_is_err;
    (void)result__Bool_Error_unwrap_or;
    (void)result__Bool_Error_expect;
    (void)result__Bool_Error_map;
    (void)result__Bool_Error_map_err;
    (void)result__Bool_Error_and_then;
    (void)result__Bool_Error_or_else;
    (void)result__Bool_Error_and;
    (void)result__Bool_Error_or;
    (void)result__Bool_Error_eq;

    /* option_int combinators not tested here
     * (covered by option_test) */
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

    /* option_Score combinators not tested here */
    (void)option_Score_get;
    (void)option_Score_unwrap_or;
    (void)option_Score_expect;
    (void)option_Score_map;
    (void)option_Score_and_then;
    (void)option_Score_or_else;
    (void)option_Score_filter;
    (void)option_Score_combine_with;
    (void)option_Score_replace;
    (void)option_Score_take;
    (void)option_Score_eq;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

static void test_pq_cbytes_accessor(void)
{
    PriorityQueue q;
    static int qbuf[16];
    pq_init(&q, qbuf, 16u, sizeof(int), cmp_int_asc, NULL);
    const PriorityQueue* cq = &q;
    EXPECT(pq_as_cbytes(cq).len == 0u);            /* empty guard */
    EXPECT(pq_as_cbytes(NULL).len == 0u);          /* NULL guard   */
    { int v = 5; (void)pq_push(&q, &v); }
    EXPECT(pq_as_cbytes(cq).len == pq_as_bytes(&q).len);

    /* Typed wrappers. DEFINE_PRIORITY_QUEUE emits pq_T_as_cbytes for every
       instantiation, and this file carries no -Wunused-function pragma
       (unlike borrow_test.c) because its contract is full API coverage — so
       both instantiations must be exercised in THIS branch. The
       pq_fuzz_suppress_unused() block lives in the #else CANON_FUZZING
       branch and does not cover the ordinary build. */
    {
        int      ibuf[8];
        pq_int   ih;
        pq_int_init(&ih, ibuf, 8, cmp_int_asc, NULL);
        const pq_int* cih = &ih;
        EXPECT(pq_int_as_cbytes(cih).len == pq_int_as_bytes(&ih).len);
    }
    {
        Score     sbuf[8];
        pq_Score  sh;
        pq_Score_init(&sh, sbuf, 8, cmp_score, NULL);
        const pq_Score* csh = &sh;
        EXPECT(pq_Score_as_cbytes(csh).len == pq_Score_as_bytes(&sh).len);
    }
}

typedef struct { unsigned char pad[512]; int key; } PqBig;

static int cmp_pqbig(const void* a, const void* b, void* ctx)
{
    int ka, kb;
    (void)ctx;
    ka = ((const PqBig*)a)->key;
    kb = ((const PqBig*)b)->key;
    return (ka > kb) - (ka < kb);
}

/* L282 false leg + L288 both legs. */
static void test_large_elements(void)
{
    static PqBig  buf[8];
    PriorityQueue q;
    unsigned      i;
    int           prev;

    EXPECT(sizeof(PqBig) > CANON_MEM_SWAP_MAX);   /* else this test is vacuous */

    pq_init(&q, buf, 8u, sizeof(PqBig), cmp_pqbig, NULL);

    /* Descending insertion forces sift_up to swap on every push. */
    for (i = 5u; i >= 1u; i--) {
        PqBig e;
        memset(&e, 0, sizeof e);
        e.key    = (int)i;
        e.pad[0] = (unsigned char)i;
        e.pad[511] = (unsigned char)(i * 3u);      /* far end of the element */
        EXPECT(pq_push(&q, &e));
    }

    prev = -1;
    for (i = 0u; i < 5u; i++) {
        PqBig out;
        /* Poisoned, not zeroed. -O2 raises -Wmaybe-uninitialized here
         * because GCC cannot see that pq_pop writes through the pointer,
         * but zero-init would make the checks below pass vacuously if pop
         * ever stopped writing: out.key == 0 beats prev == -1 on the first
         * iteration. 0xAA fails every assertion instead. */
        memset(&out, 0xAA, sizeof out);
        EXPECT(pq_pop(&q, &out));
        EXPECT(out.key > prev);                    /* heap order held */
        EXPECT(out.pad[0]   == (unsigned char)out.key);
        EXPECT(out.pad[511] == (unsigned char)(out.key * 3)); /* whole element moved */
        prev = out.key;
    }
    EXPECT(pq_is_empty(&q));
}

/* L489 false leg: popping to discard. A real API use -- draining a queue
 * without wanting the values -- that the suite never exercised. */
static void test_pop_discarding_value(void)
{
    static int    buf[8];
    PriorityQueue q;
    int           k = -999;

    pq_init(&q, buf, 8u, sizeof(int), cmp_int_asc, NULL);
    for (k = 3; k >= 1; k--) { EXPECT(pq_push(&q, &k)); }

    EXPECT(pq_pop(&q, NULL));                      /* discard the minimum */
    EXPECT(pq_len(&q) == 2u);
    EXPECT(pq_peek(&q, &k));
    EXPECT(k == 2);                                /* heap re-established */

    EXPECT(pq_pop(&q, NULL));
    EXPECT(pq_pop(&q, NULL));
    EXPECT(pq_is_empty(&q));
    EXPECT(!pq_pop(&q, NULL));                     /* empty, still no crash */
}

/* L545 true leg. Removing the LAST element is the branch that skips the
 * sift entirely; tested on its merits, not for the coverage number. */
static void test_remove_last_element(void)
{
    static int    buf[8];
    PriorityQueue q;
    int           k, i;
    int           seen[4];

    pq_init(&q, buf, 8u, sizeof(int), cmp_int_asc, NULL);
    for (k = 4; k >= 1; k--) { EXPECT(pq_push(&q, &k)); }

    /* Index len-1 is the last slot: the memmove/sift is skipped. */
    EXPECT(pq_remove_at(&q, pq_len(&q) - 1u));
    EXPECT(pq_len(&q) == 3u);

    /* Whatever was removed, the remainder must still be a heap. */
    seen[0] = seen[1] = seen[2] = seen[3] = -999;   /* poisoned, see above */
    for (i = 0; i < 3; i++) { EXPECT(pq_pop(&q, &seen[i])); }
    EXPECT(seen[0] < seen[1]);
    EXPECT(seen[1] < seen[2]);
    EXPECT(pq_is_empty(&q));
}

/* L427 false leg: heapify with n < 2, where the sift loop never runs. */
static void test_heapify_single(void)
{
    static int    buf[4];
    PriorityQueue q;
    int           k = -999;

    buf[0] = 42;
    pq_init(&q, buf, 4u, sizeof(int), cmp_int_asc, NULL);
    pq_heapify(&q, 1u);                            /* n == 1: nothing to sift */
    EXPECT(pq_len(&q) == 1u);
    EXPECT(pq_peek(&q, &k));
    EXPECT(k == 42);

    pq_heapify(&q, 0u);                            /* n == 0: the L424 path */
    EXPECT(pq_is_empty(&q));
}

/* Nine null-argument legs across eight accessors. Grouped because they are
 * one property -- every accessor tolerates NULL and reports the empty
 * answer rather than trapping -- not eight unrelated cases. */
static void test_null_arguments(void)
{
    bytes_t  b;
    cbytes_t cb;
    PriorityQueue q;
    static int buf[4];

    EXPECT(pq_len(NULL)       == 0u);              /* L596 */
    EXPECT(pq_capacity(NULL)  == 0u);              /* L601 */
    EXPECT(pq_remaining(NULL) == 0u);              /* L606 */
    EXPECT(pq_is_empty(NULL));                     /* L611 */
    EXPECT(!pq_is_full(NULL));                     /* L616 */

    pq_heapify(NULL, 4u);                          /* L420: must not trap */

    b = pq_as_bytes(NULL);                         /* L628 leg 1 */
    EXPECT(b.len == 0u);
    cb = pq_as_cbytes(NULL);
    EXPECT(cb.len == 0u);

    /* L628/L636 leg 2: non-NULL queue with NULL data. Reached through a
     * zeroed struct rather than by poking a live queue, so the test does
     * not depend on the layout staying as it is. */
    memset(&q, 0, sizeof q);
    b = pq_as_bytes(&q);
    EXPECT(b.len == 0u);
    cb = pq_as_cbytes(&q);
    EXPECT(cb.len == 0u);

    /* And the non-empty case, so the check is a discrimination and not
     * merely "everything returns empty". */
    pq_init(&q, buf, 4u, sizeof(int), cmp_int_asc, NULL);
    {
        int k = 7;
        EXPECT(pq_push(&q, &k));
    }
    b = pq_as_bytes(&q);
    EXPECT(b.len == sizeof(int));

    /* L580, both legs of pq_peek's guard. !top comes from an empty queue
     * (pq_peek_raw returns NULL); !out from a caller passing no destination.
     * The second is a plausible mistake -- "is there a top?" written as
     * pq_peek(&q, NULL) instead of !pq_is_empty(&q) -- and must return
     * false rather than dereference. */
    {
        PriorityQueue empty_q;
        int           dst = 0;
        static int    ebuf[4];
        pq_init(&empty_q, ebuf, 4u, sizeof(int), cmp_int_asc, NULL);
        EXPECT(!pq_peek(&empty_q, &dst));          /* !top  */
        EXPECT(!pq_peek(&q, NULL));                /* !out  */
        EXPECT(pq_peek(&q, &dst));                 /* both non-null */
        EXPECT(dst == 7);
    }
}

/* test_self_swap_is_a_noop was here and has been REMOVED.
 *
 * It covered L278's `a == b` leg by calling pq_swap(&q, 1, 1) from client
 * code. That is no longer possible: pq_swap became pq_swap_ (internal) in
 * the same change, on the header's own trailing-underscore convention, and
 * the leg is now dead by construction -- pq_sift_down_ only swaps when
 * smallest != idx, pq_sift_up_ swaps a parent with a child at idx > 0.
 *
 * Deleted rather than adapted: reaching it now would mean calling an
 * internal symbol from a test purely to move a coverage number, which is
 * the contrived-driver pattern the project rejects. It takes a
 * justification row instead. Expected MC/DC after this change: 81/82 with
 * one justified outcome, not 82/82.
 */

int main(void)
{
    test_large_elements();
    test_pop_discarding_value();
    test_remove_last_element();
    test_heapify_single();
    test_null_arguments();

    test_pq_cbytes_accessor();
    (void)pq_suppress_unused;

    /* init */
    test_init_valid();

    /* ordering */
    test_min_heap_ordering();
    test_max_heap_ordering();

    /* peek */
    test_peek_no_remove();

    /* push errors */
    test_push_capacity_exceeded();
    test_push_null_args();

    /* pop/peek on empty */
    test_pop_raw_empty();
    test_peek_raw_empty();

    /* remove_at */
    test_remove_at_valid();
    test_remove_at_out_of_range();
    test_remove_at_null();

    /* heapify */
    test_heapify();

    /* queries */
    test_queries();

    /* struct type */
    test_struct_type();

#ifdef CANON_LIFETIME_DEBUG
    test_lifetime_init_opens_token();
    test_lifetime_push_restamps();
    test_lifetime_pop_restamps();
    test_lifetime_remove_at_restamps();
    test_lifetime_heapify_restamps();
    test_lifetime_queries_preserve_id();
#endif

    if (g_failed == 0) {
        printf("OK  priority_queue_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  priority_queue_test  (%d assertion(s) failed)\n",
            g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void pq_fuzz_suppress_unused(void)
{
    /* Comparators unused in fuzz path (only cmp_int_asc is used) */
    (void)cmp_int_desc;
    (void)cmp_score;
    /* Raw untyped legacy bool wrappers */
    (void)pq_push;
    (void)pq_pop;
    (void)pq_peek;
    (void)pq_remove_at;
    (void)pq_heapify; /* not used in fuzz path */

    /* pq_int: legacy wrappers + typed fns not exercised in fuzz */
    (void)pq_int_heapify;
    (void)pq_int_push;
    (void)pq_int_pop;
    (void)pq_int_peek;
    (void)pq_int_remove_at;
    (void)pq_int_capacity;
    (void)pq_int_remaining;
    (void)pq_int_is_full;
    (void)pq_int_as_bytes;
    (void)pq_int_as_cbytes;

    /* All pq_Score functions — Score type not used in fuzz path */
    (void)pq_Score_init;
    (void)pq_Score_heapify;
    (void)pq_Score_push_result;
    (void)pq_Score_pop_option;
    (void)pq_Score_peek_option;
    (void)pq_Score_remove_at_result;
    (void)pq_Score_push;
    (void)pq_Score_pop;
    (void)pq_Score_peek;
    (void)pq_Score_remove_at;
    (void)pq_Score_len;
    (void)pq_Score_capacity;
    (void)pq_Score_remaining;
    (void)pq_Score_is_empty;
    (void)pq_Score_is_full;
    (void)pq_Score_as_bytes;
    (void)pq_Score_as_cbytes;

    /* option_Score — all unused in fuzz */
    (void)option_Score_some;
    (void)option_Score_none;
    (void)option_Score_is_some;
    (void)option_Score_is_none;
    (void)option_Score_unwrap;
    (void)option_Score_get;
    (void)option_Score_unwrap_or;
    (void)option_Score_expect;
    (void)option_Score_map;
    (void)option_Score_and_then;
    (void)option_Score_or_else;
    (void)option_Score_filter;
    (void)option_Score_combine_with;
    (void)option_Score_replace;
    (void)option_Score_take;
    (void)option_Score_eq;

    /* result__Bool_Error combinators not used in fuzz path */
    (void)result__Bool_Error_get_ok;
    (void)result__Bool_Error_get_err;
    (void)result__Bool_Error_is_err;
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
}

/*
 * Input layout:
 *   [0]    capacity selector — maps to cap_table[data[0] % 4]
 *   [1..N] operation stream — each byte:
 *            high nibble: operation index (0–5)
 *            low  nibble: value used for push (0–15)
 *
 * Operations:
 *   0 — push(value)
 *   1 — pop — verify returned value matches expected minimum
 *   2 — peek — verify returned value matches expected minimum
 *   3 — remove_at(0) — removes current root (minimum)
 *   4 — remove_at(len/2) — removes a middle element
 *   5 — clear — resets heap and reference
 *
 * Invariants checked after every operation:
 *   - is_empty iff len == 0
 *   - structural heap property: for every i > 0, data[parent(i)] <= data[i]
 *   - peek == minimum value present in ref_count (if non-empty)
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static const usize cap_table[4] = {4, 8, 16, 32};

    /* Max capacity is 32 — size the buffer for that */
    int buf[32];
    pq_int h;

    (void)pq_fuzz_suppress_unused;

    if (size < 2) return 0;

    usize cap = cap_table[data[0] % 4u];
    pq_int_init(&h, buf, cap, cmp_int_asc, NULL);

    /*
     * ref_count[v] — how many copies of value v (0–15) are in the heap.
     */
    int ref_count[16];
    memset(ref_count, 0, sizeof(ref_count));

    /*
     * CHECK_INVARIANTS — verified after every operation:
     *   1. is_empty consistency
     *   2. structural heap property (parent <= child for all nodes)
     *   3. peek matches the minimum value tracked in ref_count
     */
    #define CHECK_INVARIANTS()                                              \
        do {                                                                \
            usize len__ = pq_int_len(&h);                                  \
                                                                            \
            /* is_empty consistency */                                      \
            if (pq_int_is_empty(&h) != (len__ == 0))                       \
                __builtin_trap();                                           \
                                                                            \
            /* structural heap property */                                  \
            {                                                               \
                const int* hd__ = (const int*)h._pq.data;                  \
                for (usize k__ = 1; k__ < len__; k__++) {                  \
                    usize p__ = (k__ - 1u) / 2u;                           \
                    if (hd__[p__] > hd__[k__]) __builtin_trap();           \
                }                                                           \
            }                                                               \
                                                                            \
            /* peek must match expected minimum */                          \
            {                                                               \
                int exp_min__ = -1;                                         \
                for (int v__ = 0; v__ < 16; v__++) {                       \
                    if (ref_count[v__] > 0) { exp_min__ = v__; break; }    \
                }                                                           \
                if (exp_min__ == -1) {                                      \
                    if (!pq_int_is_empty(&h)) __builtin_trap();             \
                } else {                                                    \
                    option_int top__ = pq_int_peek_option(&h);              \
                    if (!option_int_is_some(top__))  __builtin_trap();      \
                    if (option_int_unwrap(top__) != exp_min__)              \
                        __builtin_trap();                                   \
                }                                                           \
            }                                                               \
        } while (0)

    for (usize i = 1; i < size; i++) {
        u8  byte = data[i];
        u8  op   = (u8)(byte >> 4u) % 6u;
        int val  = (int)(byte & 0x0Fu); /* 0–15 */

        switch (op) {

            case 0: { /* push */
                result__Bool_Error r = pq_int_push_result(&h, val);
                if (result__Bool_Error_is_ok(r)) {
                    ref_count[val]++;
                }
                /* ERR_CAPACITY_EXCEEDED when full — not a trap */
                break;
            }

            case 1: { /* pop — verify value matches expected minimum */
                option_int o = pq_int_pop_option(&h);
                if (option_int_is_some(o)) {
                    int popped = option_int_unwrap(o);
                    /* popped must be in range */
                    if (popped < 0 || popped > 15) __builtin_trap();
                    /* popped must be tracked in ref */
                    if (ref_count[popped] == 0)    __builtin_trap();
                    /* popped must be the minimum tracked value */
                    for (int v = 0; v < popped; v++) {
                        if (ref_count[v] > 0) __builtin_trap();
                    }
                    ref_count[popped]--;
                } else {
                    /* None only valid when heap is empty */
                    if (pq_int_len(&h) != 0) __builtin_trap();
                }
                break;
            }

            case 2: { /* peek — non-destructive, already verified by CHECK_INVARIANTS */
                usize before = pq_int_len(&h);
                (void)pq_int_peek_option(&h);
                if (pq_int_len(&h) != before) __builtin_trap();
                break;
            }

            case 3: { /* remove_at(0) — removes current root */
                if (pq_int_len(&h) == 0) break;
                int removed = ((const int*)h._pq.data)[0];
                result__Bool_Error r = pq_int_remove_at_result(&h, 0);
                if (!result__Bool_Error_is_ok(r)) __builtin_trap();
                if (removed < 0 || removed > 15)  __builtin_trap();
                if (ref_count[removed] == 0)       __builtin_trap();
                ref_count[removed]--;
                break;
            }

            case 4: { /* remove_at(len/2) — removes a middle element */
                usize len = pq_int_len(&h);
                if (len == 0) break;
                usize idx    = len / 2u;
                int   removed = ((const int*)h._pq.data)[idx];
                result__Bool_Error r = pq_int_remove_at_result(&h, idx);
                if (!result__Bool_Error_is_ok(r)) __builtin_trap();
                if (removed < 0 || removed > 15)  __builtin_trap();
                if (ref_count[removed] == 0)       __builtin_trap();
                ref_count[removed]--;
                break;
            }

            case 5: { /* clear */
                /*
                 * pq_int has no clear function — simulate by re-initializing
                 * over the same buffer, which is semantically equivalent.
                 */
                pq_int_init(&h, buf, cap, cmp_int_asc, NULL);
                memset(ref_count, 0, sizeof(ref_count));
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
