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
 * @file compare_test.c
 * @brief Unit tests (and optional fuzz harness) for compare.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput exercises the same logic with
 *              arbitrary byte input.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   algo_cmp_u8   / algo_cmp_u8_desc
 *   algo_cmp_u16  / algo_cmp_u16_desc
 *   algo_cmp_u32  / algo_cmp_u32_desc
 *   algo_cmp_u64  / algo_cmp_u64_desc
 *   algo_cmp_i8   / algo_cmp_i8_desc
 *   algo_cmp_i16  / algo_cmp_i16_desc
 *   algo_cmp_i32  / algo_cmp_i32_desc
 *   algo_cmp_i64  / algo_cmp_i64_desc
 *   algo_cmp_usize / algo_cmp_usize_desc
 *   algo_cmp_isize / algo_cmp_isize_desc
 *   algo_cmp_f32  / algo_cmp_f32_desc  (including NaN, INFINITY)
 *   algo_cmp_f64  / algo_cmp_f64_desc  (including NaN, INFINITY)
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   No 0b binary literals. All variables declared before use (C99).
 *   The TEST() macro uses CANON_MAYBE_UNUSED to suppress -Wunused-function
 *   in the fuzz twin build.
 */

#include "compare.h"
#include <math.h>     /* NAN, INFINITY */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Portability: unused-function suppression
 * ====================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_MAYBE_UNUSED __attribute__((unused))
#else
    #define CANON_MAYBE_UNUSED
#endif

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) static CANON_MAYBE_UNUSED void test_##name(void)
#define RUN(name)  do { test_##name(); } while (0)

#define EXPECT(expr)                                                \
    do {                                                            \
        if (expr) {                                                 \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr, "FAIL  %s:%d  %s\n",                   \
                    __FILE__, __LINE__, #expr);                     \
        }                                                           \
    } while (0)

/* Helpers */
static int sign_of(int v) { return (v > 0) - (v < 0); }

/* =========================================================================
 * Helper macros
 *
 * CONTRACT3(fn, lo_val, hi_val, mid_val)
 *   Requires lo_val < mid_val < hi_val in the ascending ordering.
 *   Verifies: equal, less, greater, and antisymmetry for all three pairs.
 *
 * DESC_REVERSES(asc_fn, desc_fn, val_a, val_b)
 *   Verifies desc returns -sign(asc) for unequal inputs, 0 for equal.
 *   IMPORTANT: val_a and val_b must be the exact type expected by the
 *   comparator. For f32 comparators use (f32) casts; for f64 use plain
 *   double literals. Passing the wrong size causes a stack buffer overflow
 *   because __typeof__ allocates a variable of the literal's type and the
 *   comparator dereferences it as a wider type.
 * ====================================================================== */

#define CONTRACT3(fn, lo_val, hi_val, mid_val)                              \
    do {                                                                    \
        __typeof__(lo_val) _lo  = (lo_val);                                 \
        __typeof__(lo_val) _hi  = (hi_val);                                 \
        __typeof__(lo_val) _mid = (mid_val);                                \
        EXPECT(fn(&_lo,  &_lo,  NULL) == 0);                                \
        EXPECT(fn(&_hi,  &_hi,  NULL) == 0);                                \
        EXPECT(fn(&_mid, &_mid, NULL) == 0);                                \
        EXPECT(fn(&_lo,  &_hi,  NULL) < 0);                                 \
        EXPECT(fn(&_hi,  &_lo,  NULL) > 0);                                 \
        EXPECT(fn(&_lo,  &_mid, NULL) < 0);                                 \
        EXPECT(fn(&_mid, &_lo,  NULL) > 0);                                 \
        EXPECT(fn(&_mid, &_hi,  NULL) < 0);                                 \
        EXPECT(fn(&_hi,  &_mid, NULL) > 0);                                 \
        EXPECT(sign_of(fn(&_lo,  &_hi,  NULL)) ==                           \
              -sign_of(fn(&_hi,  &_lo,  NULL)));                            \
        EXPECT(sign_of(fn(&_lo,  &_mid, NULL)) ==                           \
              -sign_of(fn(&_mid, &_lo,  NULL)));                            \
        EXPECT(sign_of(fn(&_mid, &_hi,  NULL)) ==                           \
              -sign_of(fn(&_hi,  &_mid, NULL)));                            \
    } while (0)

#define DESC_REVERSES(asc_fn, desc_fn, val_a, val_b)                        \
    do {                                                                    \
        __typeof__(val_a) _a = (val_a);                                     \
        __typeof__(val_b) _b = (val_b);                                     \
        int _asc  = asc_fn(&_a,  &_b,  NULL);                              \
        int _desc = desc_fn(&_a, &_b,  NULL);                              \
        if (_asc != 0) {                                                    \
            EXPECT(sign_of(_asc) == -sign_of(_desc));                      \
        } else {                                                            \
            EXPECT(_desc == 0);                                             \
        }                                                                   \
    } while (0)

/* =========================================================================
 * Unsigned integer comparators
 * ====================================================================== */

TEST(cmp_u8) {
    /* CONTRACT3 requires lo < mid < hi */
    CONTRACT3(algo_cmp_u8, (u8)0, (u8)255, (u8)128);

    /* Boundary spot checks */
    {
        u8 zero = 0, one = 1, max_val = 255;
        EXPECT(algo_cmp_u8(&zero, &max_val, NULL) < 0);
        EXPECT(algo_cmp_u8(&max_val, &zero, NULL) > 0);
        EXPECT(algo_cmp_u8(&one, &one, NULL) == 0);
    }

    DESC_REVERSES(algo_cmp_u8, algo_cmp_u8_desc, (u8)10, (u8)20);
    DESC_REVERSES(algo_cmp_u8, algo_cmp_u8_desc, (u8)5,  (u8)5);
}

TEST(cmp_u16) {
    CONTRACT3(algo_cmp_u16, (u16)0, (u16)65535, (u16)1000);
    {
        u16 lo = 999, hi = 1000;
        EXPECT(algo_cmp_u16(&lo, &hi, NULL) < 0);
        EXPECT(algo_cmp_u16(&hi, &lo, NULL) > 0);
    }
    DESC_REVERSES(algo_cmp_u16, algo_cmp_u16_desc, (u16)100, (u16)200);
    DESC_REVERSES(algo_cmp_u16, algo_cmp_u16_desc, (u16)7,   (u16)7);
}

TEST(cmp_u32) {
    CONTRACT3(algo_cmp_u32, (u32)0, (u32)0xFFFFFFFFU, (u32)0x80000000U);
    {
        u32 lo = 0x7FFFFFFFU, hi = 0x80000000U;
        EXPECT(algo_cmp_u32(&lo, &hi, NULL) < 0);
        EXPECT(algo_cmp_u32(&hi, &lo, NULL) > 0);
    }
    DESC_REVERSES(algo_cmp_u32, algo_cmp_u32_desc, (u32)1, (u32)2);
    DESC_REVERSES(algo_cmp_u32, algo_cmp_u32_desc, (u32)0, (u32)0);
}

TEST(cmp_u64) {
    CONTRACT3(algo_cmp_u64, (u64)0, (u64)~0ULL, (u64)1);
    {
        u64 lo = 0x100000000ULL, hi = 0x200000000ULL;
        EXPECT(algo_cmp_u64(&lo, &hi, NULL) < 0);
        EXPECT(algo_cmp_u64(&hi, &lo, NULL) > 0);
    }
    DESC_REVERSES(algo_cmp_u64, algo_cmp_u64_desc, (u64)0xDEADBEEFULL, (u64)0xCAFEBABEULL);
    DESC_REVERSES(algo_cmp_u64, algo_cmp_u64_desc, (u64)42, (u64)42);
}

/* =========================================================================
 * Signed integer comparators
 * ====================================================================== */

TEST(cmp_i8) {
    CONTRACT3(algo_cmp_i8, (i8)-128, (i8)127, (i8)0);
    {
        i8 neg = -1, pos = 1, zero = 0;
        EXPECT(algo_cmp_i8(&neg,  &pos,  NULL) < 0);
        EXPECT(algo_cmp_i8(&pos,  &neg,  NULL) > 0);
        EXPECT(algo_cmp_i8(&zero, &zero, NULL) == 0);
        EXPECT(algo_cmp_i8(&neg,  &zero, NULL) < 0);
    }
    DESC_REVERSES(algo_cmp_i8, algo_cmp_i8_desc, (i8)-1, (i8)1);
    DESC_REVERSES(algo_cmp_i8, algo_cmp_i8_desc, (i8)0,  (i8)0);
}

TEST(cmp_i16) {
    CONTRACT3(algo_cmp_i16, (i16)-32768, (i16)32767, (i16)0);
    {
        i16 lo = -1, hi = 0;
        EXPECT(algo_cmp_i16(&lo, &hi, NULL) < 0);
        EXPECT(algo_cmp_i16(&hi, &lo, NULL) > 0);
    }
    DESC_REVERSES(algo_cmp_i16, algo_cmp_i16_desc, (i16)-100, (i16)100);
    DESC_REVERSES(algo_cmp_i16, algo_cmp_i16_desc, (i16)5,    (i16)5);
}

TEST(cmp_i32) {
    CONTRACT3(algo_cmp_i32, (i32)(-2147483647 - 1), (i32)2147483647, (i32)0);
    {
        i32 min_val = (-2147483647 - 1);
        i32 max_val = 2147483647;
        EXPECT(algo_cmp_i32(&min_val, &max_val, NULL) < 0);
        EXPECT(algo_cmp_i32(&max_val, &min_val, NULL) > 0);
    }
    DESC_REVERSES(algo_cmp_i32, algo_cmp_i32_desc, (i32)-1, (i32)1);
    DESC_REVERSES(algo_cmp_i32, algo_cmp_i32_desc, (i32)0,  (i32)0);
}

TEST(cmp_i64) {
    CONTRACT3(algo_cmp_i64,
              (i64)(-9223372036854775807LL - 1),
              (i64)9223372036854775807LL,
              (i64)0LL);
    {
        i64 lo = -1LL, hi = 1LL;
        EXPECT(algo_cmp_i64(&lo, &hi, NULL) < 0);
        EXPECT(algo_cmp_i64(&hi, &lo, NULL) > 0);
    }
    DESC_REVERSES(algo_cmp_i64, algo_cmp_i64_desc, (i64)-1LL, (i64)1LL);
    DESC_REVERSES(algo_cmp_i64, algo_cmp_i64_desc, (i64)42LL, (i64)42LL);
}

/* =========================================================================
 * usize / isize comparators
 * ====================================================================== */

TEST(cmp_usize) {
    CONTRACT3(algo_cmp_usize, (usize)0, (usize)~(usize)0, (usize)42);
    {
        usize lo = 100, hi = 200;
        EXPECT(algo_cmp_usize(&lo, &hi, NULL) < 0);
        EXPECT(algo_cmp_usize(&hi, &lo, NULL) > 0);
    }
    DESC_REVERSES(algo_cmp_usize, algo_cmp_usize_desc, (usize)1, (usize)2);
    DESC_REVERSES(algo_cmp_usize, algo_cmp_usize_desc, (usize)0, (usize)0);
}

TEST(cmp_isize) {
    /* isize_min without overflow */
    {
        isize min_val = (isize)(((usize)1) << (sizeof(isize) * 8 - 1));
        isize max_val = (isize)(((usize)~(usize)0) >> 1);
        isize zero    = 0;
        CONTRACT3(algo_cmp_isize, min_val, max_val, zero);
    }
    DESC_REVERSES(algo_cmp_isize, algo_cmp_isize_desc, (isize)-1, (isize)1);
    DESC_REVERSES(algo_cmp_isize, algo_cmp_isize_desc, (isize)0,  (isize)0);
}

/* =========================================================================
 * f32 comparator — including NaN and INFINITY
 *
 * NOTE: All literals passed to DESC_REVERSES for f32 comparators must have
 * the F suffix so __typeof__ allocates a float (4 bytes). Passing a plain
 * double literal (8 bytes) to an f32 comparator causes a stack buffer
 * overflow because algo_cmp_f32 dereferences the pointer as float*.
 * ====================================================================== */

TEST(cmp_f32) {
    f32 neg     = -1.0F;
    f32 zero    =  0.0F;
    f32 pos     =  1.0F;
    f32 inf     =  (f32)INFINITY;
    f32 neginf  = -(f32)INFINITY;
    f32 nan1    =  (f32)NAN;
    f32 nan2    =  (f32)NAN;
    f32 big     =  1e38F;
    f32 neg_big = -1e38F;

    /* Normal total order */
    EXPECT(algo_cmp_f32(&neg,  &zero, NULL) < 0);
    EXPECT(algo_cmp_f32(&zero, &neg,  NULL) > 0);
    EXPECT(algo_cmp_f32(&zero, &pos,  NULL) < 0);
    EXPECT(algo_cmp_f32(&pos,  &zero, NULL) > 0);
    EXPECT(algo_cmp_f32(&neg,  &pos,  NULL) < 0);
    EXPECT(algo_cmp_f32(&pos,  &neg,  NULL) > 0);

    /* Equality */
    EXPECT(algo_cmp_f32(&zero, &zero, NULL) == 0);
    EXPECT(algo_cmp_f32(&pos,  &pos,  NULL) == 0);
    EXPECT(algo_cmp_f32(&neg,  &neg,  NULL) == 0);

    /* INFINITY */
    EXPECT(algo_cmp_f32(&inf,    &pos,    NULL) > 0);
    EXPECT(algo_cmp_f32(&pos,    &inf,    NULL) < 0);
    EXPECT(algo_cmp_f32(&neginf, &neg,    NULL) < 0);
    EXPECT(algo_cmp_f32(&neg,    &neginf, NULL) > 0);
    EXPECT(algo_cmp_f32(&neginf, &inf,    NULL) < 0);
    EXPECT(algo_cmp_f32(&inf,    &inf,    NULL) == 0);
    EXPECT(algo_cmp_f32(&neginf, &neginf, NULL) == 0);

    /* NaN sorted last */
    EXPECT(algo_cmp_f32(&nan1, &pos,  NULL) > 0);
    EXPECT(algo_cmp_f32(&pos,  &nan1, NULL) < 0);
    EXPECT(algo_cmp_f32(&nan1, &inf,  NULL) > 0);
    EXPECT(algo_cmp_f32(&inf,  &nan1, NULL) < 0);
    EXPECT(algo_cmp_f32(&nan1, &nan2, NULL) == 0);
    EXPECT(algo_cmp_f32(&nan1, &nan1, NULL) == 0);

    /* Wide range */
    EXPECT(algo_cmp_f32(&neg_big, &big, NULL) < 0);
    EXPECT(algo_cmp_f32(&big, &neg_big, NULL) > 0);

    /* desc reverses — use F-suffix literals so __typeof__ gives float */
    DESC_REVERSES(algo_cmp_f32, algo_cmp_f32_desc, -1.0F, 1.0F);
    DESC_REVERSES(algo_cmp_f32, algo_cmp_f32_desc,  0.0F, 0.0F);
    DESC_REVERSES(algo_cmp_f32, algo_cmp_f32_desc, (f32)INFINITY, 1.0F);

    /* desc: NaN sorted first */
    EXPECT(algo_cmp_f32_desc(&nan1, &pos, NULL) < 0);
    EXPECT(algo_cmp_f32_desc(&pos, &nan1, NULL) > 0);

    /* ctx is ignored */
    {
        int dummy = 42;
        EXPECT(algo_cmp_f32(&neg, &pos, &dummy) < 0);
    }
}

/* =========================================================================
 * f64 comparator — including NaN and INFINITY
 *
 * NOTE: All literals passed to DESC_REVERSES for f64 comparators must be
 * plain double literals (no F suffix). Using (f32)INFINITY here would
 * allocate a float (4 bytes) but algo_cmp_f64 reads 8 bytes — overflow.
 * ====================================================================== */

TEST(cmp_f64) {
    f64 neg     = -1.0;
    f64 zero    =  0.0;
    f64 pos     =  1.0;
    f64 inf     =  INFINITY;
    f64 neginf  = -INFINITY;
    f64 nan1    =  NAN;
    f64 nan2    =  NAN;
    f64 big     =  1e308;
    f64 neg_big = -1e308;

    /* Normal total order */
    EXPECT(algo_cmp_f64(&neg,  &zero, NULL) < 0);
    EXPECT(algo_cmp_f64(&zero, &neg,  NULL) > 0);
    EXPECT(algo_cmp_f64(&zero, &pos,  NULL) < 0);
    EXPECT(algo_cmp_f64(&pos,  &zero, NULL) > 0);
    EXPECT(algo_cmp_f64(&neg,  &pos,  NULL) < 0);
    EXPECT(algo_cmp_f64(&pos,  &neg,  NULL) > 0);

    /* Equality */
    EXPECT(algo_cmp_f64(&zero, &zero, NULL) == 0);
    EXPECT(algo_cmp_f64(&pos,  &pos,  NULL) == 0);
    EXPECT(algo_cmp_f64(&neg,  &neg,  NULL) == 0);

    /* INFINITY */
    EXPECT(algo_cmp_f64(&inf,    &pos,    NULL) > 0);
    EXPECT(algo_cmp_f64(&pos,    &inf,    NULL) < 0);
    EXPECT(algo_cmp_f64(&neginf, &neg,    NULL) < 0);
    EXPECT(algo_cmp_f64(&neg,    &neginf, NULL) > 0);
    EXPECT(algo_cmp_f64(&neginf, &inf,    NULL) < 0);
    EXPECT(algo_cmp_f64(&inf,    &inf,    NULL) == 0);
    EXPECT(algo_cmp_f64(&neginf, &neginf, NULL) == 0);

    /* NaN sorted last */
    EXPECT(algo_cmp_f64(&nan1, &pos,  NULL) > 0);
    EXPECT(algo_cmp_f64(&pos,  &nan1, NULL) < 0);
    EXPECT(algo_cmp_f64(&nan1, &inf,  NULL) > 0);
    EXPECT(algo_cmp_f64(&inf,  &nan1, NULL) < 0);
    EXPECT(algo_cmp_f64(&nan1, &nan2, NULL) == 0);
    EXPECT(algo_cmp_f64(&nan1, &nan1, NULL) == 0);

    /* Wide range */
    EXPECT(algo_cmp_f64(&neg_big, &big, NULL) < 0);
    EXPECT(algo_cmp_f64(&big, &neg_big, NULL) > 0);

    /* desc reverses — use plain double literals (no F suffix) */
    DESC_REVERSES(algo_cmp_f64, algo_cmp_f64_desc, -1.0, 1.0);
    DESC_REVERSES(algo_cmp_f64, algo_cmp_f64_desc,  0.0, 0.0);
    DESC_REVERSES(algo_cmp_f64, algo_cmp_f64_desc, (f64)INFINITY, 1.0);

    /* desc: NaN sorted first */
    EXPECT(algo_cmp_f64_desc(&nan1, &pos, NULL) < 0);
    EXPECT(algo_cmp_f64_desc(&pos, &nan1, NULL) > 0);

    /* ctx is ignored */
    {
        int dummy = 99;
        EXPECT(algo_cmp_f64(&neg, &pos, &dummy) < 0);
    }
}

/* =========================================================================
 * Total order properties — antisymmetry and transitivity
 * ====================================================================== */

TEST(total_order_properties) {
    /* Antisymmetry */
    {
        u32 lo = 1, hi = 2;
        EXPECT(sign_of(algo_cmp_u32(&lo, &hi, NULL)) ==
              -sign_of(algo_cmp_u32(&hi, &lo, NULL)));
        EXPECT(sign_of(algo_cmp_u32(&lo, &lo, NULL)) == 0);
    }
    {
        i32 lo = -5, hi = 5;
        EXPECT(sign_of(algo_cmp_i32(&lo, &hi, NULL)) ==
              -sign_of(algo_cmp_i32(&hi, &lo, NULL)));
    }

    /* Transitivity: sorted sequence, each adjacent pair and first/last */
    {
        u64 vals[4];
        int idx;
        vals[0] = 0;
        vals[1] = 100;
        vals[2] = 1000;
        vals[3] = 0xFFFFFFFFFFFFFFFFULL;
        for (idx = 0; idx < 3; idx++) {
            EXPECT(algo_cmp_u64(&vals[idx], &vals[idx + 1], NULL) < 0);
        }
        EXPECT(algo_cmp_u64(&vals[0], &vals[3], NULL) < 0);
    }
    {
        i64 vals[4];
        int idx;
        vals[0] = -9223372036854775807LL - 1;
        vals[1] = -1;
        vals[2] = 0;
        vals[3] = 9223372036854775807LL;
        for (idx = 0; idx < 3; idx++) {
            EXPECT(algo_cmp_i64(&vals[idx], &vals[idx + 1], NULL) < 0);
        }
        EXPECT(algo_cmp_i64(&vals[0], &vals[3], NULL) < 0);
    }
}

/* =========================================================================
 * desc == reverse of asc for all types
 * ====================================================================== */

TEST(desc_reverses_asc) {
    DESC_REVERSES(algo_cmp_u8,    algo_cmp_u8_desc,    (u8)0,    (u8)255);
    DESC_REVERSES(algo_cmp_u16,   algo_cmp_u16_desc,   (u16)0,   (u16)65535);
    DESC_REVERSES(algo_cmp_u32,   algo_cmp_u32_desc,   (u32)0,   (u32)0xFFFFFFFFU);
    DESC_REVERSES(algo_cmp_u64,   algo_cmp_u64_desc,   (u64)0,   (u64)~0ULL);
    DESC_REVERSES(algo_cmp_i8,    algo_cmp_i8_desc,    (i8)-128, (i8)127);
    DESC_REVERSES(algo_cmp_i16,   algo_cmp_i16_desc,   (i16)-32768, (i16)32767);
    DESC_REVERSES(algo_cmp_i32,   algo_cmp_i32_desc,
                  (i32)(-2147483647 - 1), (i32)2147483647);
    DESC_REVERSES(algo_cmp_i64,   algo_cmp_i64_desc,
                  (i64)(-9223372036854775807LL - 1),
                  (i64)9223372036854775807LL);
    DESC_REVERSES(algo_cmp_usize, algo_cmp_usize_desc, (usize)0,  (usize)~(usize)0);
    DESC_REVERSES(algo_cmp_isize, algo_cmp_isize_desc, (isize)-1, (isize)1);
    /* f32: must use F-suffix literals */
    DESC_REVERSES(algo_cmp_f32,   algo_cmp_f32_desc,   -1.0F, 1.0F);
    /* f64: must use plain double literals */
    DESC_REVERSES(algo_cmp_f64,   algo_cmp_f64_desc,   -1.0,  1.0);

    /* Equal values */
    DESC_REVERSES(algo_cmp_u32,   algo_cmp_u32_desc,   (u32)42,  (u32)42);
    DESC_REVERSES(algo_cmp_i64,   algo_cmp_i64_desc,   (i64)-1,  (i64)-1);
    DESC_REVERSES(algo_cmp_f64,   algo_cmp_f64_desc,   3.14,     3.14);
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    u64 raw_a = 0, raw_b = 0;

    if (size < 16) return 0;

    memcpy(&raw_a, data,     8);
    memcpy(&raw_b, data + 8, 8);

    {
        u8   a8   = (u8)raw_a,   b8   = (u8)raw_b;
        u16  a16  = (u16)raw_a,  b16  = (u16)raw_b;
        u32  a32  = (u32)raw_a,  b32  = (u32)raw_b;
        u64  a64  = raw_a,       b64  = raw_b;
        i8   ai8  = (i8)raw_a,   bi8  = (i8)raw_b;
        i16  ai16 = (i16)raw_a,  bi16 = (i16)raw_b;
        i32  ai32 = (i32)raw_a,  bi32 = (i32)raw_b;
        i64  ai64 = (i64)raw_a,  bi64 = (i64)raw_b;
        usize aus = (usize)raw_a, bus = (usize)raw_b;
        isize ais = (isize)raw_a, bis = (isize)raw_b;

#define CHECK_ANTISYM(fn, pa, pb) \
        if (sign_of(fn(pa, pb, NULL)) != -sign_of(fn(pb, pa, NULL))) \
            __builtin_trap()

        CHECK_ANTISYM(algo_cmp_u8,    &a8,   &b8);
        CHECK_ANTISYM(algo_cmp_u16,   &a16,  &b16);
        CHECK_ANTISYM(algo_cmp_u32,   &a32,  &b32);
        CHECK_ANTISYM(algo_cmp_u64,   &a64,  &b64);
        CHECK_ANTISYM(algo_cmp_i8,    &ai8,  &bi8);
        CHECK_ANTISYM(algo_cmp_i16,   &ai16, &bi16);
        CHECK_ANTISYM(algo_cmp_i32,   &ai32, &bi32);
        CHECK_ANTISYM(algo_cmp_i64,   &ai64, &bi64);
        CHECK_ANTISYM(algo_cmp_usize, &aus,  &bus);
        CHECK_ANTISYM(algo_cmp_isize, &ais,  &bis);

#undef CHECK_ANTISYM

#define CHECK_DESC(asc, desc, pa, pb)                                   \
        {                                                               \
            int _a = asc(pa, pb, NULL);                                 \
            int _d = desc(pa, pb, NULL);                                \
            if (_a != 0 && sign_of(_a) != -sign_of(_d)) __builtin_trap(); \
            if (_a == 0 && _d != 0)                     __builtin_trap(); \
        }

        CHECK_DESC(algo_cmp_u8,    algo_cmp_u8_desc,    &a8,   &b8);
        CHECK_DESC(algo_cmp_u16,   algo_cmp_u16_desc,   &a16,  &b16);
        CHECK_DESC(algo_cmp_u32,   algo_cmp_u32_desc,   &a32,  &b32);
        CHECK_DESC(algo_cmp_u64,   algo_cmp_u64_desc,   &a64,  &b64);
        CHECK_DESC(algo_cmp_i8,    algo_cmp_i8_desc,    &ai8,  &bi8);
        CHECK_DESC(algo_cmp_i16,   algo_cmp_i16_desc,   &ai16, &bi16);
        CHECK_DESC(algo_cmp_i32,   algo_cmp_i32_desc,   &ai32, &bi32);
        CHECK_DESC(algo_cmp_i64,   algo_cmp_i64_desc,   &ai64, &bi64);
        CHECK_DESC(algo_cmp_usize, algo_cmp_usize_desc, &aus,  &bus);
        CHECK_DESC(algo_cmp_isize, algo_cmp_isize_desc, &ais,  &bis);

#undef CHECK_DESC
    }

    /* Float comparators: reinterpret raw bytes as f32/f64 bit patterns */
    {
        f32 af32, bf32;
        f64 af64, bf64;
        u32 raw32a = (u32)raw_a, raw32b = (u32)raw_b;

        memcpy(&af32, &raw32a, sizeof(f32));
        memcpy(&bf32, &raw32b, sizeof(f32));
        memcpy(&af64, &raw_a,  sizeof(f64));
        memcpy(&bf64, &raw_b,  sizeof(f64));

        /* Reflexivity */
        if (algo_cmp_f32(&af32, &af32, NULL) != 0) __builtin_trap();
        if (algo_cmp_f64(&af64, &af64, NULL) != 0) __builtin_trap();

        /* Antisymmetry */
        {
            int r32 = algo_cmp_f32(&af32, &bf32, NULL);
            int s32 = algo_cmp_f32(&bf32, &af32, NULL);
            if (sign_of(r32) != -sign_of(s32)) __builtin_trap();
        }
        {
            int r64 = algo_cmp_f64(&af64, &bf64, NULL);
            int s64 = algo_cmp_f64(&bf64, &af64, NULL);
            if (sign_of(r64) != -sign_of(s64)) __builtin_trap();
        }

        /* desc reverses asc */
        {
            int asc32  = algo_cmp_f32(&af32, &bf32, NULL);
            int desc32 = algo_cmp_f32_desc(&af32, &bf32, NULL);
            if (asc32 != 0 && sign_of(asc32) != -sign_of(desc32)) __builtin_trap();
            if (asc32 == 0 && desc32 != 0)                        __builtin_trap();
        }
        {
            int asc64  = algo_cmp_f64(&af64, &bf64, NULL);
            int desc64 = algo_cmp_f64_desc(&af64, &bf64, NULL);
            if (asc64 != 0 && sign_of(asc64) != -sign_of(desc64)) __builtin_trap();
            if (asc64 == 0 && desc64 != 0)                        __builtin_trap();
        }
    }

    return 0;
}

#else

int main(void) {
    RUN(cmp_u8);
    RUN(cmp_u16);
    RUN(cmp_u32);
    RUN(cmp_u64);
    RUN(cmp_i8);
    RUN(cmp_i16);
    RUN(cmp_i32);
    RUN(cmp_i64);
    RUN(cmp_usize);
    RUN(cmp_isize);
    RUN(cmp_f32);
    RUN(cmp_f64);
    RUN(total_order_properties);
    RUN(desc_reverses_asc);

    printf("\ncompare_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
