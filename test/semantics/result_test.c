/**
 * @file result_test.c
 * @brief Tests for semantics/result/result.h — Result<T, E> type
 *
 * Covers:
 *   - CANON_RESULT instantiation for (int, MyError) and (Point, MyError)
 *   - Constructors: ok(), err()
 *   - Queries: is_ok(), is_err()
 *   - Safe extraction: get_ok(), get_err(), unwrap_or()
 *   - Unsafe extraction: unwrap(), unwrap_err(), expect()
 *   - Combinators: map(), map_err(), and_then(), or_else(), and(), or()
 *   - Comparison: eq()
 *   - TRY macro — strict C99 early-return propagation
 *   - TRY_REMAP macro — early-return with error substitution
 *   - Union layout: only ok or err is valid at any time
 *   - NULL-safety contracts on get_ok(), get_err() are present (not invoked)
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random bytes as (is_ok, value, error_code, op) tuples
 *   - Exercises all constructors, combinators, and mutation ops
 *   - Verifies is_ok/is_err are always complementary
 *   - Verifies get_ok/get_err write correct values
 *   - Verifies and() / or() structural invariants
 */

#define CANON_CONTRACT_IMPL
#include "semantics/result/result.h"

#include <stdio.h>
#include <string.h>

/* ── Type definitions ────────────────────────────────────────────────────── */

typedef enum {
    ERR_NONE = 0,
    ERR_INVALID,
    ERR_NOT_FOUND,
    ERR_OVERFLOW
} MyError;

typedef struct { int x; int y; } Point;

CANON_RESULT(int,   MyError)
CANON_RESULT(Point, MyError)

/* ── Helper functions (visible to both unit test and fuzz builds) ─────────── */

static int      int_double(int x)          { return x * 2; }
static int      int_negate(int x)          { return -x; }
static bool     int_eq(int a, int b)       { return a == b; }
static bool     err_eq(MyError a, MyError b) { return a == b; }
static MyError  err_enrich(MyError e)      { (void)e; return ERR_OVERFLOW; }
static result_int_MyError checked_double(int x)
{
    if (x > 1000) return result_int_MyError_err(ERR_OVERFLOW);
    return result_int_MyError_ok(x * 2);
}

static result_int_MyError fallback_zero(MyError e)
{
    (void)e;
    return result_int_MyError_ok(0);
}

static result_int_MyError always_err(MyError e)
{
    (void)e;
    return result_int_MyError_err(ERR_NOT_FOUND);
}

/* ── Fuzz-only helpers — not used in unit test build ─────────────────────── */
#ifdef CANON_FUZZING
static bool int_positive(int x) { return x > 0; }
#endif

/* ── TRY helper — must be a function so TRY can early-return ─────────────── */

static result_int_MyError try_chain(result_int_MyError first,
                                    result_int_MyError second)
{
    TRY(int, MyError, first);
    TRY(int, MyError, second);
    return result_int_MyError_ok(
        result_int_MyError_unwrap(first) +
        result_int_MyError_unwrap(second));
}

static result_int_MyError try_remap_fn(result_int_MyError r)
{
    TRY_REMAP(int, MyError, r, ERR_NOT_FOUND);
    return result_int_MyError_ok(result_int_MyError_unwrap(r));
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

static void test_ok_is_ok(void)
{
    result_int_MyError r = result_int_MyError_ok(42);
    EXPECT(result_int_MyError_is_ok(r));
    EXPECT(!result_int_MyError_is_err(r));
}

static void test_err_is_err(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    EXPECT(result_int_MyError_is_err(r));
    EXPECT(!result_int_MyError_is_ok(r));
}

static void test_ok_zero_value(void)
{
    result_int_MyError r = result_int_MyError_ok(0);
    EXPECT(result_int_MyError_is_ok(r));
    EXPECT(result_int_MyError_unwrap(r) == 0);
}

static void test_err_none(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_NONE);
    EXPECT(result_int_MyError_is_err(r));
    EXPECT(result_int_MyError_unwrap_err(r) == ERR_NONE);
}

/* ── get_ok / get_err ────────────────────────────────────────────────────── */

static void test_get_ok_writes_value(void)
{
    result_int_MyError r = result_int_MyError_ok(7);
    int out = 0;
    bool ok = result_int_MyError_get_ok(r, &out);
    EXPECT(ok);
    EXPECT(out == 7);
}

static void test_get_ok_returns_false_on_err(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    int out = 99;
    bool ok = result_int_MyError_get_ok(r, &out);
    EXPECT(!ok);
    EXPECT(out == 99); /* unchanged */
}

static void test_get_err_writes_error(void)
{
    result_int_MyError r   = result_int_MyError_err(ERR_NOT_FOUND);
    MyError            out = ERR_NONE;
    bool               ok  = result_int_MyError_get_err(r, &out);
    EXPECT(ok);
    EXPECT(out == ERR_NOT_FOUND);
}

static void test_get_err_returns_false_on_ok(void)
{
    result_int_MyError r   = result_int_MyError_ok(5);
    MyError            out = ERR_OVERFLOW;
    bool               ok  = result_int_MyError_get_err(r, &out);
    EXPECT(!ok);
    EXPECT(out == ERR_OVERFLOW); /* unchanged */
}

/* ── unwrap_or ───────────────────────────────────────────────────────────── */

static void test_unwrap_or_ok(void)
{
    result_int_MyError r = result_int_MyError_ok(10);
    EXPECT(result_int_MyError_unwrap_or(r, 99) == 10);
}

static void test_unwrap_or_err(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    EXPECT(result_int_MyError_unwrap_or(r, 99) == 99);
}

/* ── unwrap / unwrap_err / expect ────────────────────────────────────────── */

static void test_unwrap_ok(void)
{
    result_int_MyError r = result_int_MyError_ok(42);
    EXPECT(result_int_MyError_unwrap(r) == 42);
}

static void test_unwrap_err_value(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_OVERFLOW);
    EXPECT(result_int_MyError_unwrap_err(r) == ERR_OVERFLOW);
}

static void test_expect_ok(void)
{
    result_int_MyError r = result_int_MyError_ok(55);
    EXPECT(result_int_MyError_expect(r, "must be present") == 55);
}

/* ── map ─────────────────────────────────────────────────────────────────── */

static void test_map_ok(void)
{
    result_int_MyError r = result_int_MyError_ok(21);
    result_int_MyError m = result_int_MyError_map(r, int_double);
    EXPECT(result_int_MyError_is_ok(m));
    EXPECT(result_int_MyError_unwrap(m) == 42);
}

static void test_map_err_passthrough(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    result_int_MyError m = result_int_MyError_map(r, int_double);
    EXPECT(result_int_MyError_is_err(m));
    EXPECT(result_int_MyError_unwrap_err(m) == ERR_INVALID);
}

static void test_map_chain(void)
{
    result_int_MyError r = result_int_MyError_ok(3);
    result_int_MyError m = result_int_MyError_map(
                               result_int_MyError_map(r, int_double),
                               int_negate);
    EXPECT(result_int_MyError_unwrap(m) == -6);
}

/* ── map_err ─────────────────────────────────────────────────────────────── */

static void test_map_err_transforms_error(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    result_int_MyError m = result_int_MyError_map_err(r, err_enrich);
    EXPECT(result_int_MyError_is_err(m));
    EXPECT(result_int_MyError_unwrap_err(m) == ERR_OVERFLOW);
}

static void test_map_err_ok_passthrough(void)
{
    result_int_MyError r = result_int_MyError_ok(7);
    result_int_MyError m = result_int_MyError_map_err(r, err_enrich);
    EXPECT(result_int_MyError_is_ok(m));
    EXPECT(result_int_MyError_unwrap(m) == 7);
}

/* ── and_then ────────────────────────────────────────────────────────────── */

static void test_and_then_ok_returns_ok(void)
{
    result_int_MyError r = result_int_MyError_ok(5);
    result_int_MyError m = result_int_MyError_and_then(r, checked_double);
    EXPECT(result_int_MyError_is_ok(m));
    EXPECT(result_int_MyError_unwrap(m) == 10);
}

static void test_and_then_ok_returns_err(void)
{
    result_int_MyError r = result_int_MyError_ok(2000); /* triggers overflow */
    result_int_MyError m = result_int_MyError_and_then(r, checked_double);
    EXPECT(result_int_MyError_is_err(m));
    EXPECT(result_int_MyError_unwrap_err(m) == ERR_OVERFLOW);
}

static void test_and_then_err_skips_fn(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_NOT_FOUND);
    result_int_MyError m = result_int_MyError_and_then(r, checked_double);
    EXPECT(result_int_MyError_is_err(m));
    EXPECT(result_int_MyError_unwrap_err(m) == ERR_NOT_FOUND);
}

/* ── or_else ─────────────────────────────────────────────────────────────── */

static void test_or_else_ok_unchanged(void)
{
    result_int_MyError r = result_int_MyError_ok(3);
    result_int_MyError m = result_int_MyError_or_else(r, fallback_zero);
    EXPECT(result_int_MyError_is_ok(m));
    EXPECT(result_int_MyError_unwrap(m) == 3);
}

static void test_or_else_err_calls_fn(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    result_int_MyError m = result_int_MyError_or_else(r, fallback_zero);
    EXPECT(result_int_MyError_is_ok(m));
    EXPECT(result_int_MyError_unwrap(m) == 0);
}

static void test_or_else_err_fn_returns_err(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    result_int_MyError m = result_int_MyError_or_else(r, always_err);
    EXPECT(result_int_MyError_is_err(m));
    EXPECT(result_int_MyError_unwrap_err(m) == ERR_NOT_FOUND);
}

/* ── and() / or() ────────────────────────────────────────────────────────── */

static void test_and_ok_returns_other(void)
{
    result_int_MyError a = result_int_MyError_ok(1);
    result_int_MyError b = result_int_MyError_ok(99);
    result_int_MyError r = result_int_MyError_and(a, b);
    EXPECT(result_int_MyError_is_ok(r));
    EXPECT(result_int_MyError_unwrap(r) == 99);
}

static void test_and_err_propagates(void)
{
    result_int_MyError a = result_int_MyError_err(ERR_INVALID);
    result_int_MyError b = result_int_MyError_ok(99);
    result_int_MyError r = result_int_MyError_and(a, b);
    EXPECT(result_int_MyError_is_err(r));
    EXPECT(result_int_MyError_unwrap_err(r) == ERR_INVALID);
}

static void test_or_ok_returns_self(void)
{
    result_int_MyError a = result_int_MyError_ok(5);
    result_int_MyError b = result_int_MyError_ok(99);
    result_int_MyError r = result_int_MyError_or(a, b);
    EXPECT(result_int_MyError_is_ok(r));
    EXPECT(result_int_MyError_unwrap(r) == 5);
}

static void test_or_err_returns_other(void)
{
    result_int_MyError a = result_int_MyError_err(ERR_INVALID);
    result_int_MyError b = result_int_MyError_ok(99);
    result_int_MyError r = result_int_MyError_or(a, b);
    EXPECT(result_int_MyError_is_ok(r));
    EXPECT(result_int_MyError_unwrap(r) == 99);
}

/* ── eq ──────────────────────────────────────────────────────────────────── */

static void test_eq_both_ok_equal(void)
{
    result_int_MyError a = result_int_MyError_ok(7);
    result_int_MyError b = result_int_MyError_ok(7);
    EXPECT(result_int_MyError_eq(a, b, int_eq, err_eq));
}

static void test_eq_both_ok_unequal(void)
{
    result_int_MyError a = result_int_MyError_ok(7);
    result_int_MyError b = result_int_MyError_ok(8);
    EXPECT(!result_int_MyError_eq(a, b, int_eq, err_eq));
}

static void test_eq_both_err_equal(void)
{
    result_int_MyError a = result_int_MyError_err(ERR_INVALID);
    result_int_MyError b = result_int_MyError_err(ERR_INVALID);
    EXPECT(result_int_MyError_eq(a, b, int_eq, err_eq));
}

static void test_eq_both_err_unequal(void)
{
    result_int_MyError a = result_int_MyError_err(ERR_INVALID);
    result_int_MyError b = result_int_MyError_err(ERR_NOT_FOUND);
    EXPECT(!result_int_MyError_eq(a, b, int_eq, err_eq));
}

static void test_eq_ok_vs_err(void)
{
    result_int_MyError a = result_int_MyError_ok(1);
    result_int_MyError b = result_int_MyError_err(ERR_NONE);
    EXPECT(!result_int_MyError_eq(a, b, int_eq, err_eq));
    EXPECT(!result_int_MyError_eq(b, a, int_eq, err_eq));
}

static void test_eq_reflexive(void)
{
    result_int_MyError a = result_int_MyError_ok(42);
    EXPECT(result_int_MyError_eq(a, a, int_eq, err_eq));
}

/* ── TRY macro ───────────────────────────────────────────────────────────── */

static void test_try_both_ok(void)
{
    result_int_MyError a = result_int_MyError_ok(3);
    result_int_MyError b = result_int_MyError_ok(4);
    result_int_MyError r = try_chain(a, b);
    /* try_chain returns ok(unwrap(a) + unwrap(b)) = ok(7) */
    EXPECT(result_int_MyError_is_ok(r));
    EXPECT(result_int_MyError_unwrap(r) == 7);
}

static void test_try_first_err_propagates(void)
{
    result_int_MyError a = result_int_MyError_err(ERR_INVALID);
    result_int_MyError b = result_int_MyError_ok(4);
    result_int_MyError r = try_chain(a, b);
    EXPECT(result_int_MyError_is_err(r));
    EXPECT(result_int_MyError_unwrap_err(r) == ERR_INVALID);
}

static void test_try_second_err_propagates(void)
{
    result_int_MyError a = result_int_MyError_ok(3);
    result_int_MyError b = result_int_MyError_err(ERR_NOT_FOUND);
    result_int_MyError r = try_chain(a, b);
    EXPECT(result_int_MyError_is_err(r));
    EXPECT(result_int_MyError_unwrap_err(r) == ERR_NOT_FOUND);
}

/* ── TRY_REMAP macro ─────────────────────────────────────────────────────── */

static void test_try_remap_ok_passes_through(void)
{
    result_int_MyError r = result_int_MyError_ok(5);
    result_int_MyError m = try_remap_fn(r);
    EXPECT(result_int_MyError_is_ok(m));
    EXPECT(result_int_MyError_unwrap(m) == 5);
}

static void test_try_remap_err_substitutes(void)
{
    result_int_MyError r = result_int_MyError_err(ERR_INVALID);
    result_int_MyError m = try_remap_fn(r);
    EXPECT(result_int_MyError_is_err(m));
    /* ERR_INVALID should be replaced with ERR_NOT_FOUND by try_remap_fn */
    EXPECT(result_int_MyError_unwrap_err(m) == ERR_NOT_FOUND);
}

/* ── Point struct round-trip and full API coverage ───────────────────────── */

static bool point_eq(Point a, Point b) { return a.x == b.x && a.y == b.y; }
static Point point_double(Point p) { Point r = {p.x*2, p.y*2}; return r; }
static MyError err_to_overflow(MyError e) { (void)e; return ERR_OVERFLOW; }
static result_Point_MyError point_ok_if_positive(Point p) {
    if (p.x > 0 && p.y > 0) return result_Point_MyError_ok(p);
    return result_Point_MyError_err(ERR_INVALID);
}
static result_Point_MyError point_fallback(MyError e) {
    Point z = {-1, -1};
    (void)e;
    return result_Point_MyError_ok(z);
}

static void test_point_round_trip(void)
{
    Point p = {3, 4};
    result_Point_MyError r = result_Point_MyError_ok(p);
    Point out = {0, 0};
    EXPECT(result_Point_MyError_get_ok(r, &out));
    EXPECT(out.x == 3 && out.y == 4);
}

static void test_point_all_functions(void)
{
    Point p   = {2, 5};
    Point neg = {-1, -1};
    result_Point_MyError ok_r  = result_Point_MyError_ok(p);
    result_Point_MyError err_r = result_Point_MyError_err(ERR_INVALID);
    Point                out;
    MyError              eout;

    /* is_ok / is_err */
    EXPECT(result_Point_MyError_is_ok(ok_r));
    EXPECT(result_Point_MyError_is_err(err_r));

    /* get_ok / get_err */
    EXPECT(result_Point_MyError_get_ok(ok_r, &out));
    EXPECT(out.x == 2 && out.y == 5);
    EXPECT(!result_Point_MyError_get_ok(err_r, &out));
    EXPECT(result_Point_MyError_get_err(err_r, &eout));
    EXPECT(eout == ERR_INVALID);

    /* unwrap_or */
    out = result_Point_MyError_unwrap_or(err_r, neg);
    EXPECT(out.x == -1 && out.y == -1);

    /* unwrap / expect */
    out = result_Point_MyError_unwrap(ok_r);
    EXPECT(out.x == 2 && out.y == 5);
    out = result_Point_MyError_expect(ok_r, "must be present");
    EXPECT(out.x == 2 && out.y == 5);

    /* unwrap_err */
    eout = result_Point_MyError_unwrap_err(err_r);
    EXPECT(eout == ERR_INVALID);

    /* map */
    result_Point_MyError m = result_Point_MyError_map(ok_r, point_double);
    out = result_Point_MyError_unwrap(m);
    EXPECT(out.x == 4 && out.y == 10);

    /* map_err */
    result_Point_MyError me = result_Point_MyError_map_err(err_r, err_to_overflow);
    EXPECT(result_Point_MyError_unwrap_err(me) == ERR_OVERFLOW);

    /* and_then */
    result_Point_MyError at = result_Point_MyError_and_then(ok_r, point_ok_if_positive);
    EXPECT(result_Point_MyError_is_ok(at));

    /* or_else */
    result_Point_MyError oe = result_Point_MyError_or_else(err_r, point_fallback);
    out = result_Point_MyError_unwrap(oe);
    EXPECT(out.x == -1 && out.y == -1);

    /* and / or */
    result_Point_MyError second = result_Point_MyError_ok(neg);
    result_Point_MyError ra = result_Point_MyError_and(ok_r, second);
    out = result_Point_MyError_unwrap(ra);
    EXPECT(out.x == -1 && out.y == -1);

    result_Point_MyError ro = result_Point_MyError_or(err_r, second);
    out = result_Point_MyError_unwrap(ro);
    EXPECT(out.x == -1 && out.y == -1);

    /* eq */
    EXPECT(result_Point_MyError_eq(ok_r, ok_r, point_eq, err_eq));
    EXPECT(!result_Point_MyError_eq(ok_r, err_r, point_eq, err_eq));
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    /* constructors */
    test_ok_is_ok();
    test_err_is_err();
    test_ok_zero_value();
    test_err_none();

    /* get_ok / get_err */
    test_get_ok_writes_value();
    test_get_ok_returns_false_on_err();
    test_get_err_writes_error();
    test_get_err_returns_false_on_ok();

    /* unwrap_or */
    test_unwrap_or_ok();
    test_unwrap_or_err();

    /* unwrap / unwrap_err / expect */
    test_unwrap_ok();
    test_unwrap_err_value();
    test_expect_ok();

    /* map */
    test_map_ok();
    test_map_err_passthrough();
    test_map_chain();

    /* map_err */
    test_map_err_transforms_error();
    test_map_err_ok_passthrough();

    /* and_then */
    test_and_then_ok_returns_ok();
    test_and_then_ok_returns_err();
    test_and_then_err_skips_fn();

    /* or_else */
    test_or_else_ok_unchanged();
    test_or_else_err_calls_fn();
    test_or_else_err_fn_returns_err();

    /* and / or */
    test_and_ok_returns_other();
    test_and_err_propagates();
    test_or_ok_returns_self();
    test_or_err_returns_other();

    /* eq */
    test_eq_both_ok_equal();
    test_eq_both_ok_unequal();
    test_eq_both_err_equal();
    test_eq_both_err_unequal();
    test_eq_ok_vs_err();
    test_eq_reflexive();

    /* TRY */
    test_try_both_ok();
    test_try_first_err_propagates();
    test_try_second_err_propagates();

    /* TRY_REMAP */
    test_try_remap_ok_passes_through();
    test_try_remap_err_substitutes();

    /* Point struct */
    test_point_round_trip();
    test_point_all_functions();

    if (g_failed == 0) {
        printf("OK  result_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  result_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */

/*
 * Suppress unused-function warnings for helpers and generated API functions
 * not exercised in the fuzz entry point. The fuzz build is intentionally
 * minimal — only int operations are stressed.
 */
static void result_fuzz_suppress_unused(void)
{
    (void)int_positive;
    (void)int_negate;
    (void)always_err;
    (void)try_chain;
    (void)try_remap_fn;
    (void)point_eq;
    (void)point_double;
    (void)err_to_overflow;
    (void)point_ok_if_positive;
    (void)point_fallback;
    /* Point generated API */
    (void)result_Point_MyError_ok;
    (void)result_Point_MyError_err;
    (void)result_Point_MyError_is_ok;
    (void)result_Point_MyError_is_err;
    (void)result_Point_MyError_get_ok;
    (void)result_Point_MyError_get_err;
    (void)result_Point_MyError_unwrap_or;
    (void)result_Point_MyError_unwrap;
    (void)result_Point_MyError_unwrap_err;
    (void)result_Point_MyError_expect;
    (void)result_Point_MyError_map;
    (void)result_Point_MyError_map_err;
    (void)result_Point_MyError_and_then;
    (void)result_Point_MyError_or_else;
    (void)result_Point_MyError_and;
    (void)result_Point_MyError_or;
    (void)result_Point_MyError_eq;
    /* int API not used in fuzz path */
    (void)result_int_MyError_expect;
}

/*
 * Input layout (8 bytes, excess ignored):
 *   [0]    is_ok flag    (nonzero -> Ok)
 *   [1..4] int value     (little-endian i32)
 *   [5]    error code    (clamped to [0, ERR_OVERFLOW])
 *   [6]    operation     (selects combinator / extraction)
 *   [7]    second is_ok  (for and() / or() / eq())
 *
 * Invariants checked after every operation:
 *   - is_ok and is_err are always complementary
 *   - get_ok writes the correct value when Ok
 *   - get_err writes the correct error when Err
 *   - and() result is Ok iff both inputs are Ok
 *   - or()  result is Ok iff at least one input is Ok
 */
int LLVMFuzzerTestOneInput(const u8 *data, usize size)
{
    u8      raw[8];
    int     val;
    MyError e;
    int     out_val;
    MyError out_err;

    (void)result_fuzz_suppress_unused;

    memset(raw, 0, sizeof(raw));
    if (size > sizeof(raw)) size = sizeof(raw);
    memcpy(raw, data, size);

    val = (int)((u32)raw[1] |
                ((u32)raw[2] << 8) |
                ((u32)raw[3] << 16) |
                ((u32)raw[4] << 24));
    e   = (MyError)(raw[5] % ((u8)ERR_OVERFLOW + 1u));

    result_int_MyError r1 = raw[0] ? result_int_MyError_ok(val)
                                   : result_int_MyError_err(e);
    result_int_MyError r2 = raw[7] ? result_int_MyError_ok(val + 1)
                                   : result_int_MyError_err(e);

    /* Invariant: is_ok and is_err are always complementary */
    if (result_int_MyError_is_ok(r1) == result_int_MyError_is_err(r1))
        __builtin_trap();

    /* get_ok invariant */
    out_val = ~val;
    if (result_int_MyError_get_ok(r1, &out_val)) {
        if (!result_int_MyError_is_ok(r1)) __builtin_trap();
        if (out_val != val)                 __builtin_trap();
    }

    /* get_err invariant */
    out_err = ERR_NONE;
    if (result_int_MyError_get_err(r1, &out_err)) {
        if (!result_int_MyError_is_err(r1)) __builtin_trap();
        if (out_err != e)                   __builtin_trap();
    }

    switch (raw[6] % 9u) {
        case 0: {
            /* map */
            result_int_MyError m = result_int_MyError_map(r1, int_double);
            if (result_int_MyError_is_ok(r1) != result_int_MyError_is_ok(m))
                __builtin_trap();
            if (result_int_MyError_is_ok(m) &&
                result_int_MyError_unwrap(m) != val * 2)
                __builtin_trap();
            break;
        }
        case 1: {
            /* map_err */
            result_int_MyError m = result_int_MyError_map_err(r1, err_enrich);
            if (result_int_MyError_is_ok(r1) != result_int_MyError_is_ok(m))
                __builtin_trap();
            break;
        }
        case 2: {
            /* and_then */
            result_int_MyError m = result_int_MyError_and_then(r1, checked_double);
            if (result_int_MyError_is_err(r1) && result_int_MyError_is_ok(m))
                __builtin_trap();
            break;
        }
        case 3: {
            /* or_else */
            result_int_MyError m = result_int_MyError_or_else(r1, fallback_zero);
            /* fallback_zero always returns Ok, so result must be Ok */
            if (!result_int_MyError_is_ok(m)) __builtin_trap();
            break;
        }
        case 4: {
            /* and() — Ok iff both are Ok */
            result_int_MyError m = result_int_MyError_and(r1, r2);
            bool both_ok = result_int_MyError_is_ok(r1) &&
                           result_int_MyError_is_ok(r2);
            if (result_int_MyError_is_ok(m) != both_ok) __builtin_trap();
            break;
        }
        case 5: {
            /* or() — Ok iff at least one is Ok */
            result_int_MyError m   = result_int_MyError_or(r1, r2);
            bool               any = result_int_MyError_is_ok(r1) ||
                                     result_int_MyError_is_ok(r2);
            if (result_int_MyError_is_ok(m) != any) __builtin_trap();
            break;
        }
        case 6: {
            /* unwrap_or */
            int def = 0;
            int v   = result_int_MyError_unwrap_or(r1, def);
            if (result_int_MyError_is_ok(r1) && v != val)  __builtin_trap();
            if (result_int_MyError_is_err(r1) && v != def) __builtin_trap();
            break;
        }
        case 7: {
            /* eq — reflexive */
            if (!result_int_MyError_eq(r1, r1, int_eq, err_eq))
                __builtin_trap();
            break;
        }
        case 8: {
            /* is_ok / is_err symmetry on r2 */
            if (result_int_MyError_is_ok(r2) == result_int_MyError_is_err(r2))
                __builtin_trap();
            break;
        }
        default:
            break;
    }

    return 0;
}

#endif /* CANON_FUZZING */
