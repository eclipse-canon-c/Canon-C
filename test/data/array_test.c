/**
 * @file array_test.c
 * @brief Tests for data/array.h — fixed-size typed array
 *
 * Covers:
 *   - array_int_8_zero()               — all elements zero
 *   - array_int_8_fill(val)            — all elements = val
 *   - array_int_8_from_ptr(src, count) — partial copy, full copy,
 *                                        NULL src, count > N clamps to N
 *   - array_int_8_len()                — always returns N
 *   - array_int_8_size_bytes()         — N * sizeof(type)
 *   - array_int_8_get(a, i, out)       — success, OOB, NULL a, NULL out
 *   - array_int_8_get_option(a, i)     — Some / None
 *   - array_int_8_get_unchecked(a, i)  — fast path, correct value
 *   - array_int_8_set(a, i, val)       — success, OOB, NULL a
 *   - array_int_8_at(a, i)             — valid pointer / NULL on OOB
 *   - array_int_8_first(a)             — pointer to element 0
 *   - array_int_8_last(a)              — pointer to element N-1
 *   - array_int_8_first_option(a)      — Some / None
 *   - array_int_8_last_option(a)       — Some / None
 *   - array_int_8_fill_all(a, val)     — all elements mutated
 *   - array_int_8_copy_from(dst, src)  — deep element copy
 *   - array_int_8_equal(a, b)          — true / false / self-equal
 *   - array_int_8_as_slice(a)          — non-owning slice view
 *   - array_int_8_as_bytes(a)          — mutable bytes view
 *   - array_int_8_as_cbytes(a)         — read-only cbytes view
 *   - ARRAY_FOR                        — index-based iteration
 *   - ARRAY_FOR_PTR                    — pointer-based iteration
 *   - array_Point_4                    — struct type, exercises all paths
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - ref[8] parallel array tracks ground truth for set/get
 *   - Operations: set, get, get_option, fill_all, copy_from, equal, zero
 *   - Invariants after every op:
 *       • get(i) == ref[i] for all valid i
 *       • equal(a, a) is always true
 *       • get_option returns Some(ref[i]) for valid indices, None for OOB
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "core/slice.h"
#include "semantics/option/option.h"
#include "data/array.h"

#include <stdio.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

CANON_OPTION(int)
DEFINE_SLICE(int)
DEFINE_ARRAY(int, 8)

/* Smaller array for boundary testing */
DEFINE_ARRAY(int, 1)

/* Second instantiation — struct type */
typedef struct { int x; int y; } Point;
CANON_OPTION(Point)
DEFINE_SLICE(Point)
DEFINE_ARRAY(Point, 4)

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

/* ── zero constructor ────────────────────────────────────────────────────── */

static void test_zero(void)
{
    array_int_8 a = array_int_8_zero();

    EXPECT(array_int_8_len() == 8);
    EXPECT(array_int_8_size_bytes() == 8 * sizeof(int));

    for (usize i = 0; i < 8; i++) {
        int val = -1;
        EXPECT(array_int_8_get(&a, i, &val));
        EXPECT(val == 0);
    }
}

/* ── fill constructor ────────────────────────────────────────────────────── */

static void test_fill(void)
{
    array_int_8 a = array_int_8_fill(42);

    for (usize i = 0; i < 8; i++) {
        int val = 0;
        EXPECT(array_int_8_get(&a, i, &val));
        EXPECT(val == 42);
    }
}

/* ── from_ptr constructor ────────────────────────────────────────────────── */

static void test_from_ptr(void)
{
    /* Full copy */
    int src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    array_int_8 a = array_int_8_from_ptr(src, 8);
    for (usize i = 0; i < 8; i++) {
        int val = 0;
        EXPECT(array_int_8_get(&a, i, &val));
        EXPECT(val == (int)(i + 1));
    }

    /* Partial copy — remaining elements zeroed */
    array_int_8 b = array_int_8_from_ptr(src, 3);
    for (usize i = 0; i < 3; i++) {
        int val = -1;
        EXPECT(array_int_8_get(&b, i, &val));
        EXPECT(val == (int)(i + 1));
    }
    for (usize i = 3; i < 8; i++) {
        int val = -1;
        EXPECT(array_int_8_get(&b, i, &val));
        EXPECT(val == 0);
    }

    /* count > N clamps to N */
    array_int_8 c = array_int_8_from_ptr(src, 99);
    for (usize i = 0; i < 8; i++) {
        int val = 0;
        EXPECT(array_int_8_get(&c, i, &val));
        EXPECT(val == (int)(i + 1));
    }

    /* NULL src — returns zeroed array */
    array_int_8 d = array_int_8_from_ptr(NULL, 8);
    for (usize i = 0; i < 8; i++) {
        int val = -1;
        EXPECT(array_int_8_get(&d, i, &val));
        EXPECT(val == 0);
    }

    /* count == 0 — returns zeroed array */
    array_int_8 e = array_int_8_from_ptr(src, 0);
    for (usize i = 0; i < 8; i++) {
        int val = -1;
        EXPECT(array_int_8_get(&e, i, &val));
        EXPECT(val == 0);
    }
}

/* ── get — bounds checking ───────────────────────────────────────────────── */

static void test_get(void)
{
    array_int_8 a = array_int_8_fill(7);

    /* Valid indices */
    int val = 0;
    EXPECT(array_int_8_get(&a, 0, &val)); EXPECT(val == 7);
    EXPECT(array_int_8_get(&a, 7, &val)); EXPECT(val == 7);

    /* OOB */
    EXPECT(!array_int_8_get(&a, 8,  &val));
    EXPECT(!array_int_8_get(&a, 99, &val));

    /* NULL a */
    EXPECT(!array_int_8_get(NULL, 0, &val));

    /* NULL out */
    EXPECT(!array_int_8_get(&a, 0, NULL));
}

/* ── get_option ──────────────────────────────────────────────────────────── */

static void test_get_option(void)
{
    array_int_8 a = array_int_8_zero();
    array_int_8_set(&a, 3, 99);

    option_int o;

    o = array_int_8_get_option(&a, 3);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 99);

    o = array_int_8_get_option(&a, 8);
    EXPECT(option_int_is_none(o));

    o = array_int_8_get_option(NULL, 0);
    EXPECT(option_int_is_none(o));
}

/* ── get_unchecked ───────────────────────────────────────────────────────── */

static void test_get_unchecked(void)
{
    array_int_8 a = array_int_8_zero();
    array_int_8_set(&a, 5, 123);

    EXPECT(array_int_8_get_unchecked(&a, 5) == 123);
    EXPECT(array_int_8_get_unchecked(&a, 0) == 0);
    EXPECT(array_int_8_get_unchecked(&a, 7) == 0);
}

/* ── set ─────────────────────────────────────────────────────────────────── */

static void test_set(void)
{
    array_int_8 a = array_int_8_zero();

    EXPECT(array_int_8_set(&a, 0, 10));
    EXPECT(array_int_8_set(&a, 7, 70));

    /* OOB */
    EXPECT(!array_int_8_set(&a, 8,  99));
    EXPECT(!array_int_8_set(&a, 99, 99));

    /* NULL a */
    EXPECT(!array_int_8_set(NULL, 0, 99));

    /* Verify values were set */
    int val = 0;
    array_int_8_get(&a, 0, &val); EXPECT(val == 10);
    array_int_8_get(&a, 7, &val); EXPECT(val == 70);
}

/* ── at ──────────────────────────────────────────────────────────────────── */

static void test_at(void)
{
    array_int_8 a = array_int_8_zero();
    array_int_8_set(&a, 4, 44);

    int* ptr = array_int_8_at(&a, 4);
    EXPECT_NOT_NULL(ptr);
    EXPECT(*ptr == 44);

    /* In-place mutation via at */
    *ptr = 88;
    int val = 0;
    array_int_8_get(&a, 4, &val);
    EXPECT(val == 88);

    /* OOB and NULL */
    EXPECT(array_int_8_at(&a, 8)    == NULL);
    EXPECT(array_int_8_at(NULL, 0)  == NULL);
}

/* ── first / last ────────────────────────────────────────────────────────── */

static void test_first_last(void)
{
    array_int_8 a = array_int_8_zero();
    array_int_8_set(&a, 0, 11);
    array_int_8_set(&a, 7, 77);

    int* f = array_int_8_first(&a);
    EXPECT_NOT_NULL(f);
    EXPECT(*f == 11);

    int* l = array_int_8_last(&a);
    EXPECT_NOT_NULL(l);
    EXPECT(*l == 77);

    /* first_option / last_option */
    option_int fo = array_int_8_first_option(&a);
    EXPECT(option_int_is_some(fo));
    EXPECT(option_int_unwrap(fo) == 11);

    option_int lo = array_int_8_last_option(&a);
    EXPECT(option_int_is_some(lo));
    EXPECT(option_int_unwrap(lo) == 77);

    /* NULL variants */
    EXPECT(option_int_is_none(array_int_8_first_option(NULL)));
    EXPECT(option_int_is_none(array_int_8_last_option(NULL)));

    /* N=1: first == last */
    array_int_1 s = array_int_1_fill(55);
    int* sf = array_int_1_first(&s);
    int* sl = array_int_1_last(&s);
    EXPECT_NOT_NULL(sf);
    EXPECT_NOT_NULL(sl);
    EXPECT(sf == sl);
    EXPECT(*sf == 55);
}

/* ── fill_all ────────────────────────────────────────────────────────────── */

static void test_fill_all(void)
{
    array_int_8 a = array_int_8_zero();
    array_int_8_fill_all(&a, 33);

    for (usize i = 0; i < 8; i++) {
        int val = 0;
        array_int_8_get(&a, i, &val);
        EXPECT(val == 33);
    }

    /* NULL-safe */
    array_int_8_fill_all(NULL, 99); /* must not crash */
}

/* ── copy_from ───────────────────────────────────────────────────────────── */

static void test_copy_from(void)
{
    int src_data[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    array_int_8 src = array_int_8_from_ptr(src_data, 8);
    array_int_8 dst = array_int_8_zero();

    array_int_8_copy_from(&dst, &src);

    EXPECT(array_int_8_equal(&dst, &src));
    for (usize i = 0; i < 8; i++) {
        int val = 0;
        array_int_8_get(&dst, i, &val);
        EXPECT(val == src_data[i]);
    }

    /* Modifying dst after copy does not affect src */
    array_int_8_set(&dst, 0, 999);
    int src_val = 0;
    array_int_8_get(&src, 0, &src_val);
    EXPECT(src_val == 10);
}

/* ── equal ───────────────────────────────────────────────────────────────── */

static void test_equal(void)
{
    array_int_8 a = array_int_8_fill(5);
    array_int_8 b = array_int_8_fill(5);
    array_int_8 c = array_int_8_fill(6);

    EXPECT(array_int_8_equal(&a, &b));
    EXPECT(!array_int_8_equal(&a, &c));

    /* Self-equal */
    EXPECT(array_int_8_equal(&a, &a));

    /* NULL returns false */
    EXPECT(!array_int_8_equal(NULL, &a));
    EXPECT(!array_int_8_equal(&a, NULL));
    EXPECT(!array_int_8_equal(NULL, NULL));

    /* After one element differs */
    array_int_8_set(&b, 3, 99);
    EXPECT(!array_int_8_equal(&a, &b));
}

/* ── as_slice ────────────────────────────────────────────────────────────── */

static void test_as_slice(void)
{
    int src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    array_int_8 a = array_int_8_from_ptr(src, 8);

    slice_int sv = array_int_8_as_slice(&a);
    EXPECT(sv.len == 8);
    EXPECT_NOT_NULL((void*)sv.ptr);

    for (usize i = 0; i < 8; i++) {
        EXPECT(sv.ptr[i] == (int)(i + 1));
    }

    /* Slice is a live view — mutation via array reflects in slice */
    array_int_8_set(&a, 0, 99);
    EXPECT(sv.ptr[0] == 99);
}

/* ── as_bytes / as_cbytes ────────────────────────────────────────────────── */

static void test_as_bytes(void)
{
    array_int_8 a = array_int_8_fill(1);

    bytes_t  b  = array_int_8_as_bytes(&a);
    cbytes_t cb = array_int_8_as_cbytes(&a);

    EXPECT(b.len  == 8 * sizeof(int));
    EXPECT(cb.len == 8 * sizeof(int));
    EXPECT_NOT_NULL(b.ptr);
    EXPECT_NOT_NULL((void*)cb.ptr);
}

/* ── ARRAY_FOR macro ─────────────────────────────────────────────────────── */

static void test_array_for(void)
{
    array_int_8 a = array_int_8_zero();

    /* Write using ARRAY_FOR */
    ARRAY_FOR(int, 8, &a, i) {
        a.items[i] = (int)i * 3;
    }

    /* Verify */
    for (usize i = 0; i < 8; i++) {
        int val = 0;
        array_int_8_get(&a, i, &val);
        EXPECT(val == (int)i * 3);
    }

    /* NULL arr_ptr: loop body must not execute */
    int count = 0;
    ARRAY_FOR(int, 8, (array_int_8*)NULL, j) {
        count++;
    }
    EXPECT(count == 0);
}

/* ── ARRAY_FOR_PTR macro ─────────────────────────────────────────────────── */

static void test_array_for_ptr(void)
{
    array_int_8 a = array_int_8_fill(1);

    /* Double every element in-place */
    ARRAY_FOR_PTR(int, 8, &a, p) {
        *p *= 2;
    }

    for (usize i = 0; i < 8; i++) {
        int val = 0;
        array_int_8_get(&a, i, &val);
        EXPECT(val == 2);
    }

    /* NULL arr_ptr: loop body must not execute */
    int count = 0;
    ARRAY_FOR_PTR(int, 8, (array_int_8*)NULL, q) {
        count++;
    }
    EXPECT(count == 0);
}

/* ── struct type — exercises all generated Point functions ───────────────── */

static void test_struct_type(void)
{
    array_Point_4 a = array_Point_4_zero();

    EXPECT(array_Point_4_len() == 4);
    EXPECT(array_Point_4_size_bytes() == 4 * sizeof(Point));

    /* set / get */
    Point p1 = {1, 2};
    Point p2 = {3, 4};
    EXPECT(array_Point_4_set(&a, 0, p1));
    EXPECT(array_Point_4_set(&a, 3, p2));

    Point out = {0, 0};
    EXPECT(array_Point_4_get(&a, 0, &out));
    EXPECT(out.x == 1 && out.y == 2);

    /* get_option */
    option_Point o = array_Point_4_get_option(&a, 3);
    EXPECT(option_Point_is_some(o));
    Point v = option_Point_unwrap(o);
    EXPECT(v.x == 3 && v.y == 4);

    EXPECT(option_Point_is_none(array_Point_4_get_option(&a, 4)));
    EXPECT(option_Point_is_none(array_Point_4_get_option(NULL, 0)));

    /* first / last */
    Point* fp = array_Point_4_first(&a);
    EXPECT_NOT_NULL(fp);
    EXPECT(fp->x == 1);

    Point* lp = array_Point_4_last(&a);
    EXPECT_NOT_NULL(lp);
    EXPECT(lp->x == 3);

    /* first_option / last_option */
    option_Point fo = array_Point_4_first_option(&a);
    EXPECT(option_Point_is_some(fo));
    EXPECT(option_Point_unwrap(fo).x == 1);

    option_Point lo = array_Point_4_last_option(&a);
    EXPECT(option_Point_is_some(lo));
    EXPECT(option_Point_unwrap(lo).x == 3);

    /* fill */
    array_Point_4 b = array_Point_4_fill((Point){7, 8});
    for (usize i = 0; i < 4; i++) {
        Point pv = {0, 0};
        array_Point_4_get(&b, i, &pv);
        EXPECT(pv.x == 7 && pv.y == 8);
    }

    /* fill_all */
    array_Point_4_fill_all(&b, (Point){0, 0});
    for (usize i = 0; i < 4; i++) {
        Point pv = {1, 1};
        array_Point_4_get(&b, i, &pv);
        EXPECT(pv.x == 0 && pv.y == 0);
    }

    /* copy_from / equal */
    array_Point_4_copy_from(&b, &a);
    EXPECT(array_Point_4_equal(&a, &b));
    EXPECT(array_Point_4_equal(&a, &a));
    EXPECT(!array_Point_4_equal(NULL, &a));

    /* from_ptr */
    Point src[2] = {{10, 11}, {12, 13}};
    array_Point_4 c = array_Point_4_from_ptr(src, 2);
    Point c0 = {0, 0}, c1 = {0, 0}, c2 = {0, 0};
    array_Point_4_get(&c, 0, &c0);
    array_Point_4_get(&c, 1, &c1);
    array_Point_4_get(&c, 2, &c2);
    EXPECT(c0.x == 10 && c1.x == 12 && c2.x == 0);

    /* as_slice */
    slice_Point sv = array_Point_4_as_slice(&a);
    EXPECT(sv.len == 4);

    /* as_bytes / as_cbytes */
    bytes_t  bv  = array_Point_4_as_bytes(&a);
    cbytes_t cbv = array_Point_4_as_cbytes(&a);
    EXPECT(bv.len == 4 * sizeof(Point));
    EXPECT(cbv.len == 4 * sizeof(Point));

    /* OOB */
    EXPECT(!array_Point_4_set(&a, 4, p1));
    EXPECT(!array_Point_4_get(&a, 4, &out));
    EXPECT(array_Point_4_at(&a, 4) == NULL);
}

/* ── Suppress unused generated functions ────────────────────────────────── */

static void array_suppress_unused(void)
{
    /* option_int combinators not tested here */
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

    /* option_Point combinators not tested here */
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

    /* array_int_1 functions not fully exercised */
    (void)array_int_1_zero;
    (void)array_int_1_fill;
    (void)array_int_1_from_ptr;
    (void)array_int_1_len;
    (void)array_int_1_size_bytes;
    (void)array_int_1_get;
    (void)array_int_1_get_option;
    (void)array_int_1_get_unchecked;
    (void)array_int_1_set;
    (void)array_int_1_at;
    (void)array_int_1_first_option;
    (void)array_int_1_last_option;
    (void)array_int_1_fill_all;
    (void)array_int_1_copy_from;
    (void)array_int_1_equal;
    (void)array_int_1_as_slice;
    (void)array_int_1_as_bytes;
    (void)array_int_1_as_cbytes;

    /* array_int_8 functions not used above */
    (void)array_int_8_get_unchecked; /* used in test_get_unchecked */
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    (void)array_suppress_unused;

    test_zero();
    test_fill();
    test_from_ptr();
    test_get();
    test_get_option();
    test_get_unchecked();
    test_set();
    test_at();
    test_first_last();
    test_fill_all();
    test_copy_from();
    test_equal();
    test_as_slice();
    test_as_bytes();
    test_array_for();
    test_array_for_ptr();
    test_struct_type();

    if (g_failed == 0) {
        printf("OK  array_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  array_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void array_fuzz_suppress_unused(void)
{
    /* array_int_1 — not used in fuzz path */
    (void)array_int_1_zero;
    (void)array_int_1_fill;
    (void)array_int_1_from_ptr;
    (void)array_int_1_len;
    (void)array_int_1_size_bytes;
    (void)array_int_1_get;
    (void)array_int_1_get_option;
    (void)array_int_1_get_unchecked;
    (void)array_int_1_set;
    (void)array_int_1_at;
    (void)array_int_1_first;
    (void)array_int_1_last;
    (void)array_int_1_first_option;
    (void)array_int_1_last_option;
    (void)array_int_1_fill_all;
    (void)array_int_1_copy_from;
    (void)array_int_1_equal;
    (void)array_int_1_as_slice;
    (void)array_int_1_as_bytes;
    (void)array_int_1_as_cbytes;

    /* array_Point_4 — not used in fuzz path */
    (void)array_Point_4_zero;
    (void)array_Point_4_fill;
    (void)array_Point_4_from_ptr;
    (void)array_Point_4_len;
    (void)array_Point_4_size_bytes;
    (void)array_Point_4_get;
    (void)array_Point_4_get_option;
    (void)array_Point_4_get_unchecked;
    (void)array_Point_4_set;
    (void)array_Point_4_at;
    (void)array_Point_4_first;
    (void)array_Point_4_last;
    (void)array_Point_4_first_option;
    (void)array_Point_4_last_option;
    (void)array_Point_4_fill_all;
    (void)array_Point_4_copy_from;
    (void)array_Point_4_equal;
    (void)array_Point_4_as_slice;
    (void)array_Point_4_as_bytes;
    (void)array_Point_4_as_cbytes;

    /* option_Point — not used in fuzz path */
    (void)option_Point_some;
    (void)option_Point_none;
    (void)option_Point_is_some;
    (void)option_Point_is_none;
    (void)option_Point_unwrap;
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

    /* array_int_8 functions not used in fuzz path */
    (void)array_int_8_from_ptr;
    (void)array_int_8_size_bytes;
    (void)array_int_8_get_unchecked;
    (void)array_int_8_at;
    (void)array_int_8_first;
    (void)array_int_8_last;
    (void)array_int_8_first_option;
    (void)array_int_8_last_option;
    (void)array_int_8_as_slice;
    (void)array_int_8_as_bytes;
    (void)array_int_8_as_cbytes;
}

/*
 * Input layout:
 *   [0]    unused (reserved for future capacity selector)
 *   [1..N] operation stream — each byte:
 *            high nibble: operation index (0–5)
 *            low  nibble: value (0–15) used for set/fill operations
 *            bits [7:4] also encode index for set/get: (byte >> 1) & 0x07
 *            (i.e. index = (high_nibble >> 1) % 8, val = low nibble)
 *
 * Operations:
 *   0 — set(i, val)      — set element i to val; update ref[i]
 *   1 — get(i)           — verify get agrees with ref[i]
 *   2 — get_option(i)    — verify option variant agrees with ref[i]
 *   3 — fill_all(val)    — fill all elements; update all ref[i]
 *   4 — copy_from        — copy a into b, verify equal(a,b)
 *   5 — zero             — reset a and ref to zero
 *
 * Invariants after every operation:
 *   - get(i) == ref[i] for all i in [0, 8)
 *   - get_option(i) == Some(ref[i]) for valid i, None for OOB
 *   - equal(a, a) is always true
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    array_int_8 a = array_int_8_zero();
    array_int_8 b = array_int_8_zero();

    (void)array_fuzz_suppress_unused;

    if (size < 2) return 0;

    /* Reference state */
    int ref[8];
    memset(ref, 0, sizeof(ref));

    #define CHECK_INVARIANTS()                                              \
        do {                                                                \
            /* get agrees with ref for all indices */                       \
            for (usize k__ = 0; k__ < 8; k__++) {                          \
                int got__ = -1;                                             \
                if (!array_int_8_get(&a, k__, &got__)) __builtin_trap();   \
                if (got__ != ref[k__])                  __builtin_trap();   \
            }                                                               \
            /* get_option agrees for valid indices */                       \
            for (usize k__ = 0; k__ < 8; k__++) {                          \
                option_int o__ = array_int_8_get_option(&a, k__);          \
                if (!option_int_is_some(o__))           __builtin_trap();   \
                if (option_int_unwrap(o__) != ref[k__]) __builtin_trap();   \
            }                                                               \
            /* OOB get_option returns None */                               \
            if (option_int_is_some(array_int_8_get_option(&a, 8)))         \
                __builtin_trap();                                           \
            /* self-equal */                                                \
            if (!array_int_8_equal(&a, &a))             __builtin_trap();   \
        } while (0)

    for (usize i = 1; i < size; i++) {
        u8  byte = data[i];
        u8  op   = (u8)(byte >> 4u) % 6u;
        int val  = (int)(byte & 0x0Fu);       /* 0–15 */
        usize idx = (usize)((byte >> 1u) % 8u); /* 0–7 */

        switch (op) {

            case 0: { /* set(idx, val) */
                if (!array_int_8_set(&a, idx, val)) __builtin_trap();
                ref[idx] = val;
                break;
            }

            case 1: { /* get(idx) — verify against ref */
                int got = -1;
                if (!array_int_8_get(&a, idx, &got)) __builtin_trap();
                if (got != ref[idx])                  __builtin_trap();
                break;
            }

            case 2: { /* get_option(idx) */
                option_int o = array_int_8_get_option(&a, idx);
                if (!option_int_is_some(o))           __builtin_trap();
                if (option_int_unwrap(o) != ref[idx]) __builtin_trap();
                break;
            }

            case 3: { /* fill_all(val) */
                array_int_8_fill_all(&a, val);
                for (usize k = 0; k < 8; k++) ref[k] = val;
                break;
            }

            case 4: { /* copy_from(b = a), then verify equal */
                array_int_8_copy_from(&b, &a);
                if (!array_int_8_equal(&a, &b)) __builtin_trap();
                break;
            }

            case 5: { /* zero — reset */
                a = array_int_8_zero();
                memset(ref, 0, sizeof(ref));
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
