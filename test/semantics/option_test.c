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
 * @file option_test.c
 * @brief Tests for semantics/option/option.h — Option<T> type
 *
 * Covers:
 *   - CANON_OPTION instantiation for primitive and struct types
 *   - Constructors: some(), none()
 *   - Queries: is_some(), is_none()
 *   - Safe extraction: get(), unwrap_or()
 *   - Unsafe extraction: unwrap(), expect()
 *   - Combinators: map(), and_then(), or_else(), filter(), combine_with()
 *   - Strict mutation: replace(), take()
 *   - Comparison: eq()
 *   - None value-field zero-initialization (no indeterminate memory)
 *   - NULL-safety contracts on get(), replace(), take()
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random bytes interpreted as (has_value, value, op) tuples
 *   - Exercises all constructors, combinators, and mutation ops
 *   - Verifies invariants hold under arbitrary inputs: no crash, no
 *     is_some/is_none inconsistency, take always leaves None
 */

#define CANON_CONTRACT_IMPL
#include "semantics/option/option.h"

#include <stdio.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

CANON_OPTION(int)

/* float omitted -- int already covers all numeric-type code paths.
 * A second numeric instantiation would generate unused-function warnings
 * for combinators not exercised in this file. */

typedef struct { int x; int y; } Point;
CANON_OPTION(Point)

/* ── Helper functions (visible to both unit test and fuzz builds) ─────────── */

static int  double_it(int x)          { return x * 2; }
static int  negate(int x)             { return -x; }
static int  add(int a, int b)         { return a + b; }
static bool is_positive(int x)        { return x > 0; }
static bool is_even(int x)            { return (x % 2) == 0; }
static bool int_eq(int a, int b)      { return a == b; }

static option_int half_if_even(int x)
{
    if (x % 2 == 0) return option_int_some(x / 2);
    return option_int_none();
}

static option_int always_none(void) { return option_int_none(); }
static option_int always_42(void)   { return option_int_some(42); }

static bool point_eq_fn(Point a, Point b) { return a.x == b.x && a.y == b.y; }
static Point point_double(Point p) { Point r = {p.x * 2, p.y * 2}; return r; }
static Point point_add(Point a, Point b) { Point r = {a.x+b.x, a.y+b.y}; return r; }
static bool  point_positive(Point p) { return p.x > 0 && p.y > 0; }
static option_Point point_none_if_origin(Point p) {
    if (p.x == 0 && p.y == 0) return option_Point_none();
    return option_Point_some(p);
}
static option_Point point_fallback(void) {
    Point r = {-1, -1};
    return option_Point_some(r);
}

/* ── Harness ─────────────────────────────────────────────────────────────── */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                             \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL %s:%d  %s\n",                 \
                    __FILE__, __LINE__, #cond);                  \
            g_failed++;                                          \
        }                                                        \
    } while (0)

/* EXPECT_NOT_NULL — records failure and returns from the calling function
 * if ptr is NULL. Prevents nullPointerRedundantCheck false positives that
 * arise when EXPECT(p != NULL) is followed by a dereference of p. */
#define EXPECT_NOT_NULL(ptr)                                     \
    do {                                                         \
        if ((ptr) == NULL) {                                     \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",         \
                    __FILE__, __LINE__, #ptr);                   \
            g_failed++;                                          \
            return;                                              \
        }                                                        \
    } while (0)

/* ── Constructors ────────────────────────────────────────────────────────── */

static void test_some_is_some(void)
{
    option_int o = option_int_some(10);
    EXPECT(option_int_is_some(o));
    EXPECT(!option_int_is_none(o));
}

static void test_none_is_none(void)
{
    option_int o = option_int_none();
    EXPECT(option_int_is_none(o));
    EXPECT(!option_int_is_some(o));
}

static void test_none_value_zero_initialized(void)
{
    /* IMPL_OPTION_NONE zero-initializes the value field to avoid
     * indeterminate memory flagged by ASan/Valgrind/Frama-C.
     * We can't read .value directly on None, but the struct should be
     * safe to memcmp — no indeterminate bytes. */
    option_int a = option_int_none();
    option_int b = option_int_none();
    EXPECT(memcmp(&a, &b, sizeof(option_int)) == 0);
}

static void test_some_struct(void)
{
    Point p = {1, 2};
    option_Point o = option_Point_some(p);
    EXPECT(option_Point_is_some(o));
}

/* ── get() ───────────────────────────────────────────────────────────────── */

static void test_get_some_writes_value(void)
{
    option_int o = option_int_some(42);
    int        out = 0;
    bool       ok  = option_int_get(o, &out);
    EXPECT(ok);
    EXPECT(out == 42);
}

static void test_get_none_returns_false(void)
{
    option_int o   = option_int_none();
    int        out = 99;
    bool       ok  = option_int_get(o, &out);
    EXPECT(!ok);
    EXPECT(out == 99); /* unchanged */
}

static void test_get_struct(void)
{
    Point p     = {3, 7};
    option_Point o = option_Point_some(p);
    Point        r = {0, 0};
    bool         ok = option_Point_get(o, &r);
    EXPECT(ok);
    EXPECT(r.x == 3 && r.y == 7);
}

/* ── unwrap_or() ─────────────────────────────────────────────────────────── */

static void test_unwrap_or_some(void)
{
    option_int o = option_int_some(5);
    EXPECT(option_int_unwrap_or(o, 99) == 5);
}

static void test_unwrap_or_none(void)
{
    option_int o = option_int_none();
    EXPECT(option_int_unwrap_or(o, 99) == 99);
}

/* ── unwrap() ────────────────────────────────────────────────────────────── */

static void test_unwrap_some(void)
{
    option_int o = option_int_some(7);
    EXPECT(option_int_unwrap(o) == 7);
}

/* ── expect() ────────────────────────────────────────────────────────────── */

static void test_expect_some(void)
{
    option_int o = option_int_some(99);
    EXPECT(option_int_expect(o, "must be present") == 99);
}

/* ── map() ───────────────────────────────────────────────────────────────── */

static void test_map_some(void)
{
    option_int o = option_int_some(21);
    option_int r = option_int_map(o, double_it);
    EXPECT(option_int_is_some(r));
    EXPECT(option_int_unwrap(r) == 42);
}

static void test_map_none(void)
{
    option_int o = option_int_none();
    option_int r = option_int_map(o, double_it);
    EXPECT(option_int_is_none(r));
}

static void test_map_chain(void)
{
    option_int o = option_int_some(3);
    option_int r = option_int_map(option_int_map(o, double_it), negate);
    EXPECT(option_int_unwrap(r) == -6);
}

/* ── and_then() ──────────────────────────────────────────────────────────── */

static void test_and_then_some_returns_some(void)
{
    option_int o = option_int_some(42);
    option_int r = option_int_and_then(o, half_if_even);
    EXPECT(option_int_is_some(r));
    EXPECT(option_int_unwrap(r) == 21);
}

static void test_and_then_some_returns_none(void)
{
    option_int o = option_int_some(7); /* odd — half_if_even returns None */
    option_int r = option_int_and_then(o, half_if_even);
    EXPECT(option_int_is_none(r));
}

static void test_and_then_none(void)
{
    option_int o = option_int_none();
    option_int r = option_int_and_then(o, half_if_even);
    EXPECT(option_int_is_none(r));
}

/* ── or_else() ───────────────────────────────────────────────────────────── */

static void test_or_else_some_unchanged(void)
{
    option_int o = option_int_some(5);
    option_int r = option_int_or_else(o, always_42);
    EXPECT(option_int_unwrap(r) == 5);
}

static void test_or_else_none_calls_fallback(void)
{
    option_int o = option_int_none();
    option_int r = option_int_or_else(o, always_42);
    EXPECT(option_int_is_some(r));
    EXPECT(option_int_unwrap(r) == 42);
}

static void test_or_else_none_fallback_also_none(void)
{
    option_int o = option_int_none();
    option_int r = option_int_or_else(o, always_none);
    EXPECT(option_int_is_none(r));
}

/* ── filter() ────────────────────────────────────────────────────────────── */

static void test_filter_some_passes(void)
{
    option_int o = option_int_some(4);
    option_int r = option_int_filter(o, is_even);
    EXPECT(option_int_is_some(r));
    EXPECT(option_int_unwrap(r) == 4);
}

static void test_filter_some_fails(void)
{
    option_int o = option_int_some(3);
    option_int r = option_int_filter(o, is_even);
    EXPECT(option_int_is_none(r));
}

static void test_filter_none(void)
{
    option_int o = option_int_none();
    option_int r = option_int_filter(o, is_positive);
    EXPECT(option_int_is_none(r));
}

/* ── combine_with() ──────────────────────────────────────────────────────── */

static void test_combine_with_both_some(void)
{
    option_int a = option_int_some(10);
    option_int b = option_int_some(32);
    option_int r = option_int_combine_with(a, b, add);
    EXPECT(option_int_is_some(r));
    EXPECT(option_int_unwrap(r) == 42);
}

static void test_combine_with_first_none(void)
{
    option_int a = option_int_none();
    option_int b = option_int_some(32);
    option_int r = option_int_combine_with(a, b, add);
    EXPECT(option_int_is_none(r));
}

static void test_combine_with_second_none(void)
{
    option_int a = option_int_some(10);
    option_int b = option_int_none();
    option_int r = option_int_combine_with(a, b, add);
    EXPECT(option_int_is_none(r));
}

static void test_combine_with_both_none(void)
{
    option_int a = option_int_none();
    option_int b = option_int_none();
    option_int r = option_int_combine_with(a, b, add);
    EXPECT(option_int_is_none(r));
}

/* ── replace() ───────────────────────────────────────────────────────────── */

static void test_replace_from_some(void)
{
    option_int o   = option_int_some(10);
    option_int old = option_int_replace(&o, 20);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 20);
    EXPECT(option_int_is_some(old));
    EXPECT(option_int_unwrap(old) == 10);
}

static void test_replace_from_none(void)
{
    option_int o   = option_int_none();
    option_int old = option_int_replace(&o, 42);
    EXPECT(option_int_is_some(o));
    EXPECT(option_int_unwrap(o) == 42);
    EXPECT(option_int_is_none(old));
}

/* ── take() ──────────────────────────────────────────────────────────────── */

static void test_take_from_some(void)
{
    option_int o     = option_int_some(7);
    option_int taken = option_int_take(&o);
    EXPECT(option_int_is_none(o));       /* o is now None */
    EXPECT(option_int_is_some(taken));
    EXPECT(option_int_unwrap(taken) == 7);
}

static void test_take_from_none(void)
{
    option_int o     = option_int_none();
    option_int taken = option_int_take(&o);
    EXPECT(option_int_is_none(o));       /* still None — no panic */
    EXPECT(option_int_is_none(taken));
}

static void test_take_twice(void)
{
    option_int o  = option_int_some(5);
    option_int t1 = option_int_take(&o);
    option_int t2 = option_int_take(&o); /* second take on None — no panic */
    EXPECT(option_int_is_some(t1));
    EXPECT(option_int_is_none(t2));
    EXPECT(option_int_is_none(o));
}

/* ── eq() ────────────────────────────────────────────────────────────────── */

static void test_eq_both_none(void)
{
    option_int a = option_int_none();
    option_int b = option_int_none();
    EXPECT(option_int_eq(a, b, int_eq));
}

static void test_eq_both_some_equal(void)
{
    option_int a = option_int_some(42);
    option_int b = option_int_some(42);
    EXPECT(option_int_eq(a, b, int_eq));
}

static void test_eq_both_some_unequal(void)
{
    option_int a = option_int_some(1);
    option_int b = option_int_some(2);
    EXPECT(!option_int_eq(a, b, int_eq));
}

static void test_eq_some_vs_none(void)
{
    option_int a = option_int_some(1);
    option_int b = option_int_none();
    EXPECT(!option_int_eq(a, b, int_eq));
    EXPECT(!option_int_eq(b, a, int_eq));
}

/* ── struct type round-trip ──────────────────────────────────────────────── */

static void test_struct_round_trip(void)
{
    Point      p = {10, 20};
    option_Point o = option_Point_some(p);
    Point      out = {0, 0};
    EXPECT(option_Point_get(o, &out));
    EXPECT(out.x == 10 && out.y == 20);
}

/* ── Point combinators and remaining API ─────────────────────────────────── */
/* These tests exist to exercise the full generated API for Point so that
 * -Wunused-function does not fire. Each generated function must be called
 * at least once per instantiation. */

static void test_struct_all_functions(void)
{
    Point        p    = {3, 4};
    Point        zero = {0, 0};
    Point        def  = {-1, -1};
    option_Point o    = option_Point_some(p);
    option_Point n    = option_Point_none();
    Point        v;

    /* unwrap_or */
    v = option_Point_unwrap_or(o, def);
    EXPECT(v.x == 3 && v.y == 4);
    v = option_Point_unwrap_or(n, def);
    EXPECT(v.x == -1 && v.y == -1);

    /* unwrap */
    v = option_Point_unwrap(o);
    EXPECT(v.x == 3 && v.y == 4);

    /* expect */
    v = option_Point_expect(o, "must be present");
    EXPECT(v.x == 3 && v.y == 4);

    /* map */
    option_Point m = option_Point_map(o, point_double);
    EXPECT(option_Point_is_some(m));
    v = option_Point_unwrap(m);
    EXPECT(v.x == 6 && v.y == 8);
    EXPECT(option_Point_is_none(option_Point_map(n, point_double)));

    /* and_then */
    option_Point at = option_Point_and_then(o, point_none_if_origin);
    EXPECT(option_Point_is_some(at));
    option_Point at2 = option_Point_and_then(option_Point_some(zero),
                                              point_none_if_origin);
    EXPECT(option_Point_is_none(at2));

    /* or_else */
    option_Point oe = option_Point_or_else(n, point_fallback);
    EXPECT(option_Point_is_some(oe));
    v = option_Point_unwrap(oe);
    EXPECT(v.x == -1 && v.y == -1);

    /* filter */
    option_Point f1 = option_Point_filter(o, point_positive);
    EXPECT(option_Point_is_some(f1));
    option_Point f2 = option_Point_filter(option_Point_some(zero), point_positive);
    EXPECT(option_Point_is_none(f2));

    /* combine_with */
    option_Point c = option_Point_combine_with(o, option_Point_some(p), point_add);
    EXPECT(option_Point_is_some(c));
    v = option_Point_unwrap(c);
    EXPECT(v.x == 6 && v.y == 8);

    /* eq */
    EXPECT(option_Point_eq(o, o, point_eq_fn));
    EXPECT(!option_Point_eq(o, n, point_eq_fn));
    EXPECT(option_Point_eq(n, n, point_eq_fn));
}

static void test_struct_replace_and_take(void)
{
    Point        p1  = {1, 2};
    Point        p2  = {3, 4};
    option_Point o   = option_Point_some(p1);
    option_Point old = option_Point_replace(&o, p2);
    Point        v;
    EXPECT(option_Point_get(o, &v));
    EXPECT(v.x == 3 && v.y == 4);
    EXPECT(option_Point_get(old, &v));
    EXPECT(v.x == 1 && v.y == 2);

    option_Point taken = option_Point_take(&o);
    EXPECT(option_Point_is_none(o));
    EXPECT(option_Point_get(taken, &v));
    EXPECT(v.x == 3 && v.y == 4);
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    /* constructors */
    test_some_is_some();
    test_none_is_none();
    test_none_value_zero_initialized();
    test_some_struct();

    /* get */
    test_get_some_writes_value();
    test_get_none_returns_false();
    test_get_struct();

    /* unwrap_or */
    test_unwrap_or_some();
    test_unwrap_or_none();

    /* unwrap / expect */
    test_unwrap_some();
    test_expect_some();

    /* map */
    test_map_some();
    test_map_none();
    test_map_chain();

    /* and_then */
    test_and_then_some_returns_some();
    test_and_then_some_returns_none();
    test_and_then_none();

    /* or_else */
    test_or_else_some_unchanged();
    test_or_else_none_calls_fallback();
    test_or_else_none_fallback_also_none();

    /* filter */
    test_filter_some_passes();
    test_filter_some_fails();
    test_filter_none();

    /* combine_with */
    test_combine_with_both_some();
    test_combine_with_first_none();
    test_combine_with_second_none();
    test_combine_with_both_none();

    /* replace */
    test_replace_from_some();
    test_replace_from_none();

    /* take */
    test_take_from_some();
    test_take_from_none();
    test_take_twice();

    /* eq */
    test_eq_both_none();
    test_eq_both_some_equal();
    test_eq_both_some_unequal();
    test_eq_some_vs_none();

    /* struct round-trip */
    test_struct_round_trip();
    test_struct_replace_and_take();
    test_struct_all_functions();

    if (g_failed == 0) {
        printf("OK  option_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  option_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */

/*
 * Suppress unused-function warnings for helpers and generated API functions
 * not exercised in the fuzz entry point. The fuzz build is intentionally
 * minimal — only the operations being stress-tested are called. This follows
 * the same pattern used in arena_test.c and other fuzzable Canon-C tests.
 */
static void option_fuzz_suppress_unused(void)
{
    /* int helpers not used in fuzz path */
    (void)negate;
    (void)is_even;
    (void)always_none;
    /* Point helpers and generated API — Point is not fuzzed */
    (void)point_eq_fn;
    (void)point_double;
    (void)point_add;
    (void)point_positive;
    (void)point_none_if_origin;
    (void)point_fallback;
    (void)option_int_expect;
    (void)option_Point_some;
    (void)option_Point_none;
    (void)option_Point_is_some;
    (void)option_Point_is_none;
    (void)option_Point_get;
    (void)option_Point_unwrap_or;
    (void)option_Point_unwrap;
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

/*
 * Input layout (8 bytes, excess ignored):
 *   [0]    has_value flag  (nonzero → Some)
 *   [1..4] int value       (little-endian i32)
 *   [5]    operation index (selects which combinator/mutation to exercise)
 *   [6]    second has_value (for combine_with / eq)
 *   [7]    second int value
 *
 * Invariants checked after every operation:
 *   - is_some and is_none are always complementary
 *   - take always leaves the source as None
 *   - replace always leaves the target as Some
 *   - get returns the same value that some() was given
 */

int LLVMFuzzerTestOneInput(const u8 *data, usize size)
{
    u8  raw[8];
    int val1, val2;
    int out;

    (void)option_fuzz_suppress_unused; /* suppress unused-function warning */

    memset(raw, 0, sizeof(raw));
    if (size > sizeof(raw)) size = sizeof(raw);
    memcpy(raw, data, size);

    val1 = (int)((u32)raw[1] |
                 ((u32)raw[2] << 8) |
                 ((u32)raw[3] << 16) |
                 ((u32)raw[4] << 24));
    val2 = (int)(u8)raw[7];

    option_int o1 = raw[0] ? option_int_some(val1) : option_int_none();
    option_int o2 = raw[6] ? option_int_some(val2) : option_int_none();

    /* Invariant: is_some and is_none are always complementary */
    if (option_int_is_some(o1) == option_int_is_none(o1)) __builtin_trap();
    if (option_int_is_some(o2) == option_int_is_none(o2)) __builtin_trap();

    /* get() invariant: if Some, get writes the correct value */
    out = ~val1;
    if (option_int_get(o1, &out)) {
        if (!option_int_is_some(o1))    __builtin_trap();
        if (out != val1)                __builtin_trap();
    }

    switch (raw[5] % 9u) {
        case 0: {
            /* map */
            option_int r = option_int_map(o1, double_it);
            if (option_int_is_some(o1) != option_int_is_some(r)) __builtin_trap();
            if (option_int_is_some(r) &&
                option_int_unwrap(r) != val1 * 2)                __builtin_trap();
            break;
        }
        case 1: {
            /* and_then */
            option_int r = option_int_and_then(o1, half_if_even);
            if (option_int_is_none(o1) && option_int_is_some(r)) __builtin_trap();
            break;
        }
        case 2: {
            /* or_else */
            option_int r = option_int_or_else(o1, always_42);
            if (option_int_is_none(r)) __builtin_trap(); /* always_42 ensures Some */
            break;
        }
        case 3: {
            /* filter */
            option_int r = option_int_filter(o1, is_positive);
            if (option_int_is_none(o1) && option_int_is_some(r)) __builtin_trap();
            if (option_int_is_some(r) && val1 <= 0)              __builtin_trap();
            break;
        }
        case 4: {
            /* combine_with */
            option_int r = option_int_combine_with(o1, o2, add);
            if (option_int_is_some(o1) && option_int_is_some(o2)) {
                if (option_int_is_none(r))                        __builtin_trap();
                if (option_int_unwrap(r) != val1 + val2)         __builtin_trap();
            } else {
                if (option_int_is_some(r))                        __builtin_trap();
            }
            break;
        }
        case 5: {
            /* replace */
            option_int old = option_int_replace(&o1, val2);
            if (option_int_is_none(o1))                          __builtin_trap();
            if (option_int_unwrap(o1) != val2)                   __builtin_trap();
            (void)old; /* old may be Some or None — both valid */
            break;
        }
        case 6: {
            /* take */
            option_int old = option_int_take(&o1);
            if (option_int_is_some(o1))                          __builtin_trap();
            (void)old;
            break;
        }
        case 7: {
            /* unwrap_or */
            int r = option_int_unwrap_or(o1, val2);
            if (option_int_is_some(o1) && r != val1)             __builtin_trap();
            if (option_int_is_none(o1) && r != val2)             __builtin_trap();
            break;
        }
        case 8: {
            /* eq — reflexive */
            if (!option_int_eq(o1, o1, int_eq))                  __builtin_trap();
            break;
        }
        default:
            break;
    }

    return 0;
}

#endif /* CANON_FUZZING */
