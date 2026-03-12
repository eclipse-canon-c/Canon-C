#include <stdio.h>
#include <math.h>    /* NAN, INFINITY */
#include "core/primitives/compare.h"

/* ============================================================================
 * Test Helpers
 * ========================================================================= */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(label, expected, actual)                                      \
    do {                                                                        \
        tests_run++;                                                            \
        if ((expected) == (actual)) {                                           \
            tests_passed++;                                                     \
        } else {                                                                \
            tests_failed++;                                                     \
            fprintf(stderr, "FAIL: %s — expected %d, got %d  (%s:%d)\n",       \
                    label, (int)(expected), (int)(actual), __FILE__, __LINE__); \
        }                                                                       \
    } while (0)

#define ASSERT_TRUE(label, expr)  ASSERT_EQ(label, 1, !!(expr))
#define ASSERT_FALSE(label, expr) ASSERT_EQ(label, 0, !!(expr))

/* Sign-normalise a comparator result to {-1, 0, 1} for readable assertions. */
static inline int sign(int v) { return (v > 0) - (v < 0); }

/* Wrappers that normalise so ASSERT_EQ(…, -1/0/1, …) is meaningful. */
#define CMP_LT(cmp, a, b) ASSERT_EQ(#cmp ": " #a " < " #b, -1, sign(cmp(&(a), &(b), NULL)))
#define CMP_EQ(cmp, a, b) ASSERT_EQ(#cmp ": " #a " == " #b,  0, sign(cmp(&(a), &(b), NULL)))
#define CMP_GT(cmp, a, b) ASSERT_EQ(#cmp ": " #a " > " #b,  1, sign(cmp(&(a), &(b), NULL)))

/* Antisymmetry: if cmp(a,b) < 0 then cmp(b,a) > 0, and vice-versa. */
#define CMP_ANTISYM(cmp, a, b)                                                  \
    do {                                                                        \
        int fwd = sign(cmp(&(a), &(b), NULL));                                  \
        int rev = sign(cmp(&(b), &(a), NULL));                                  \
        ASSERT_EQ(#cmp " antisymmetric " #a "/" #b, -fwd, rev);                 \
    } while (0)

/* ============================================================================
 * Helpers: three-way contract
 *
 *   less(a,b)    → cmp < 0, reflexive (a==a → 0), antisymmetric
 *   equal(a,b)   → cmp == 0
 *   greater(a,b) → cmp > 0, antisymmetric
 * ========================================================================= */

/* Verify the full contract for a single (asc, desc) pair. */
#define CONTRACT3(base, lo_val, hi_val, eq_val)                                 \
    do {                                                                        \
        __typeof__(lo_val) _lo = (lo_val);                                      \
        __typeof__(hi_val) _hi = (hi_val);                                      \
        __typeof__(eq_val) _eq1 = (eq_val), _eq2 = (eq_val);                   \
        /* ascending */                                                         \
        CMP_LT(base,       _lo,  _hi);                                          \
        CMP_GT(base,       _hi,  _lo);                                          \
        CMP_EQ(base,       _eq1, _eq2);                                         \
        CMP_ANTISYM(base,  _lo,  _hi);                                          \
        /* descending reverses numeric order */                                 \
        CMP_GT(base##_desc, _lo, _hi);                                          \
        CMP_LT(base##_desc, _hi, _lo);                                          \
        CMP_EQ(base##_desc, _eq1, _eq2);                                        \
        CMP_ANTISYM(base##_desc, _lo, _hi);                                     \
    } while (0)

/* ============================================================================
 * Unsigned integer comparators
 * ========================================================================= */

static void test_cmp_u8(void) {
    CONTRACT3(algo_cmp_u8, (u8)0, (u8)255, (u8)128);
    CONTRACT3(algo_cmp_u8, (u8)1, (u8)2,   (u8)0);

    /* Explicit boundary values */
    u8 zero = 0, one = 1, max = 255;
    CMP_LT(algo_cmp_u8, zero, one);
    CMP_LT(algo_cmp_u8, one,  max);
    CMP_EQ(algo_cmp_u8, zero, zero);
    CMP_EQ(algo_cmp_u8, max,  max);
}

static void test_cmp_u16(void) {
    CONTRACT3(algo_cmp_u16, (u16)0, (u16)65535, (u16)1000);
    u16 a = 999, b = 1000;
    CMP_LT(algo_cmp_u16, a, b);
}

static void test_cmp_u32(void) {
    CONTRACT3(algo_cmp_u32, (u32)0, (u32)0xFFFFFFFFU, (u32)0x80000000U);
    u32 a = 0x7FFFFFFFU, b = 0x80000000U;
    CMP_LT(algo_cmp_u32, a, b);
}

static void test_cmp_u64(void) {
    CONTRACT3(algo_cmp_u64, (u64)0, (u64)~0ULL, (u64)1);
    /* Values that would alias if truncated to 32-bit */
    u64 a = 0x100000000ULL, b = 0x200000000ULL;
    CMP_LT(algo_cmp_u64, a, b);
    CMP_ANTISYM(algo_cmp_u64, a, b);
}

/* ============================================================================
 * Signed integer comparators
 * ========================================================================= */

static void test_cmp_i8(void) {
    CONTRACT3(algo_cmp_i8, (i8)-128, (i8)127, (i8)0);

    /* Negative vs positive */
    i8 neg = -1, pos = 1, zero = 0;
    CMP_LT(algo_cmp_i8, neg,  pos);
    CMP_LT(algo_cmp_i8, neg,  zero);
    CMP_GT(algo_cmp_i8, pos,  zero);
    CMP_EQ(algo_cmp_i8, zero, zero);

    /* Descending reverses */
    CMP_GT(algo_cmp_i8_desc, neg, pos);
}

static void test_cmp_i16(void) {
    CONTRACT3(algo_cmp_i16, (i16)-32768, (i16)32767, (i16)0);
    i16 a = -1, b = 0;
    CMP_LT(algo_cmp_i16, a, b);
    CMP_ANTISYM(algo_cmp_i16, a, b);
}

static void test_cmp_i32(void) {
    CONTRACT3(algo_cmp_i32, (i32)(-2147483647 - 1), (i32)2147483647, (i32)0);

    /* Classic subtraction-based comparator trap: INT_MIN vs INT_MAX */
    i32 min = (-2147483647 - 1), max = 2147483647;
    CMP_LT(algo_cmp_i32, min, max);
    CMP_GT(algo_cmp_i32, max, min);
    CMP_ANTISYM(algo_cmp_i32, min, max);
}

static void test_cmp_i64(void) {
    CONTRACT3(algo_cmp_i64, (i64)(-9223372036854775807LL - 1), (i64)9223372036854775807LL, (i64)0LL);

    i64 a = -1LL, b = 1LL;
    CMP_LT(algo_cmp_i64, a, b);
    CMP_ANTISYM(algo_cmp_i64, a, b);
}

/* ============================================================================
 * usize / isize comparators
 * ========================================================================= */

static void test_cmp_usize(void) {
    CONTRACT3(algo_cmp_usize, (usize)0, (usize)~(usize)0, (usize)42);
    usize a = 100, b = 200;
    CMP_LT(algo_cmp_usize, a, b);
    CMP_ANTISYM(algo_cmp_usize, a, b);
}

static void test_cmp_isize(void) {
    isize min_val = (isize)(((usize)1) << (sizeof(isize) * 8 - 1));  /* ISIZE_MIN */
    isize max_val = min_val - 1;  /* ISIZE_MAX via two's complement */
    isize zero = 0;
    CMP_LT(algo_cmp_isize, min_val, zero);
    CMP_LT(algo_cmp_isize, zero,    max_val);
    CMP_LT(algo_cmp_isize, min_val, max_val);
    CMP_EQ(algo_cmp_isize, zero,    zero);
    CMP_ANTISYM(algo_cmp_isize, min_val, max_val);

    /* Descending reversal */
    CMP_GT(algo_cmp_isize_desc, min_val, max_val);
}

/* ============================================================================
 * f32 comparators — including NaN/Inf edge cases
 * ========================================================================= */

static void test_cmp_f32(void) {
    f32 neg  = -1.0f, zero = 0.0f, pos = 1.0f;
    f32 inf  = (f32)INFINITY, neginf = -(f32)INFINITY;
    f32 nan1 = (f32)NAN, nan2 = (f32)NAN;
    f32 big  = 1e38f, small = -1e38f;

    /* Basic ordering */
    CMP_LT(algo_cmp_f32, neg,    zero);
    CMP_LT(algo_cmp_f32, zero,   pos);
    CMP_LT(algo_cmp_f32, neg,    pos);
    CMP_EQ(algo_cmp_f32, zero,   zero);
    CMP_EQ(algo_cmp_f32, pos,    pos);

    /* Infinities */
    CMP_LT(algo_cmp_f32, neginf, neg);
    CMP_GT(algo_cmp_f32, inf,    pos);
    CMP_LT(algo_cmp_f32, neginf, inf);
    CMP_EQ(algo_cmp_f32, inf,    inf);

    /* Extremes */
    CMP_LT(algo_cmp_f32, small,  big);
    CMP_ANTISYM(algo_cmp_f32, small, big);

    /* NaN: sorted last in ascending order (after every non-NaN value) */
    CMP_GT(algo_cmp_f32, nan1, zero);
    CMP_GT(algo_cmp_f32, nan1, pos);
    CMP_GT(algo_cmp_f32, nan1, inf);
    CMP_GT(algo_cmp_f32, nan1, neginf);
    CMP_EQ(algo_cmp_f32, nan1, nan2);  /* NaN == NaN by convention */

    /* NaN antisymmetry: non-NaN < NaN means NaN > non-NaN */
    CMP_LT(algo_cmp_f32, zero, nan1);

    /* Descending: NaN sorted first, numerics reversed */
    CMP_LT(algo_cmp_f32_desc, nan1, zero);  /* NaN comes first (smallest) in desc */
    CMP_LT(algo_cmp_f32_desc, nan1, inf);
    CMP_EQ(algo_cmp_f32_desc, nan1, nan2);
    CMP_GT(algo_cmp_f32_desc, neg,  pos);   /* numeric order reversed */
    CMP_LT(algo_cmp_f32_desc, pos,  neg);
}

/* ============================================================================
 * f64 comparators — including NaN/Inf edge cases
 * ========================================================================= */

static void test_cmp_f64(void) {
    f64 neg  = -1.0, zero = 0.0, pos = 1.0;
    f64 inf  = INFINITY, neginf = -INFINITY;
    f64 nan1 = NAN, nan2 = NAN;
    f64 big  = 1e308, small = -1e308;

    /* Basic ordering */
    CMP_LT(algo_cmp_f64, neg,    zero);
    CMP_LT(algo_cmp_f64, zero,   pos);
    CMP_EQ(algo_cmp_f64, zero,   zero);
    CMP_EQ(algo_cmp_f64, pos,    pos);

    /* Infinities */
    CMP_LT(algo_cmp_f64, neginf, neg);
    CMP_GT(algo_cmp_f64, inf,    pos);
    CMP_LT(algo_cmp_f64, neginf, inf);
    CMP_EQ(algo_cmp_f64, inf,    inf);

    /* Extremes */
    CMP_LT(algo_cmp_f64, small,  big);
    CMP_ANTISYM(algo_cmp_f64, small, big);

    /* NaN: sorted last in ascending order */
    CMP_GT(algo_cmp_f64, nan1, zero);
    CMP_GT(algo_cmp_f64, nan1, pos);
    CMP_GT(algo_cmp_f64, nan1, inf);
    CMP_GT(algo_cmp_f64, nan1, neginf);
    CMP_EQ(algo_cmp_f64, nan1, nan2);
    CMP_LT(algo_cmp_f64, zero, nan1);

    /* Descending: NaN sorted first */
    CMP_LT(algo_cmp_f64_desc, nan1, zero);
    CMP_LT(algo_cmp_f64_desc, nan1, inf);
    CMP_EQ(algo_cmp_f64_desc, nan1, nan2);
    CMP_GT(algo_cmp_f64_desc, neg,  pos);
    CMP_LT(algo_cmp_f64_desc, pos,  neg);
}

/* ============================================================================
 * ctx parameter is ignored (passed as NULL, result must not change)
 * ========================================================================= */

static void test_ctx_ignored(void) {
    int sentinel = 0xDEAD;

    u32 a = 1, b = 2;
    int r_null = algo_cmp_u32(&a, &b, NULL);
    int r_ctx  = algo_cmp_u32(&a, &b, &sentinel);
    ASSERT_EQ("ctx ignored: u32 asc result is same", sign(r_null), sign(r_ctx));

    i32 x = -1, y = 1;
    r_null = algo_cmp_i32(&x, &y, NULL);
    r_ctx  = algo_cmp_i32(&x, &y, &sentinel);
    ASSERT_EQ("ctx ignored: i32 asc result is same", sign(r_null), sign(r_ctx));

    f64 fx = 1.0, fy = 2.0;
    r_null = algo_cmp_f64(&fx, &fy, NULL);
    r_ctx  = algo_cmp_f64(&fx, &fy, &sentinel);
    ASSERT_EQ("ctx ignored: f64 asc result is same", sign(r_null), sign(r_ctx));
}

/* ============================================================================
 * algo_cmp_fn function-pointer assignability
 *
 * Verifies every comparator has the correct signature to be stored as
 * algo_cmp_fn. This is a compile-time check — if it compiles, it passes.
 * ========================================================================= */

static void test_function_pointer_assignability(void) {
    algo_cmp_fn fns[] = {
        algo_cmp_u8,     algo_cmp_u8_desc,
        algo_cmp_u16,    algo_cmp_u16_desc,
        algo_cmp_u32,    algo_cmp_u32_desc,
        algo_cmp_u64,    algo_cmp_u64_desc,
        algo_cmp_i8,     algo_cmp_i8_desc,
        algo_cmp_i16,    algo_cmp_i16_desc,
        algo_cmp_i32,    algo_cmp_i32_desc,
        algo_cmp_i64,    algo_cmp_i64_desc,
        algo_cmp_usize,  algo_cmp_usize_desc,
        algo_cmp_isize,  algo_cmp_isize_desc,
        algo_cmp_f32,    algo_cmp_f32_desc,
        algo_cmp_f64,    algo_cmp_f64_desc,
    };
    usize count = sizeof(fns) / sizeof(fns[0]);

    tests_run++;
    if (count == 24) {
        tests_passed++;
    } else {
        tests_failed++;
        fprintf(stderr, "FAIL: function pointer table — expected 24 entries, got %zu\n", count);
    }
}

/* ============================================================================
 * Main
 * ========================================================================= */

int main(void) {
    test_cmp_u8();
    test_cmp_u16();
    test_cmp_u32();
    test_cmp_u64();

    test_cmp_i8();
    test_cmp_i16();
    test_cmp_i32();
    test_cmp_i64();

    test_cmp_usize();
    test_cmp_isize();

    test_cmp_f32();
    test_cmp_f64();

    test_ctx_ignored();
    test_function_pointer_assignability();

    printf("\nResults: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED\n", tests_failed);
        return 1;
    }
    printf(" — all tests passed!\n");
    return 0;
}
