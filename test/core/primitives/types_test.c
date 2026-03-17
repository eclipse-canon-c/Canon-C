/**
 * @file types_test.c
 * @brief Tests for core/primitives/types.h
 *
 * Verifies that every typedef has the correct size, signedness, and
 * (where applicable) the correct value range via compile-time and
 * runtime checks.
 *
 * Covers:
 *   - u8  / u16 / u32 / u64   — unsigned integer widths
 *   - i8  / i16 / i32 / i64   — signed integer widths
 *   - usize / isize            — platform-width types
 *   - f32 / f64                — floating-point widths
 *   - bool / true / false      — boolean via stdbool.h
 *
 * Exit code: 0 on all pass, 1 on any failure.
 */

#include <stdio.h>
#include <stddef.h>
#include <limits.h>   /* CHAR_BIT */
#include <stdint.h>   /* SIZE_MAX, PTRDIFF_MIN/MAX */
#include <float.h>    /* FLT_MANT_DIG, DBL_MANT_DIG */

#include "types.h"

/* ── Minimal test harness ─────────────────────────────────────────────────── */

static int s_pass = 0;
static int s_fail = 0;

#define CHECK(expr)                                                             \
    do {                                                                        \
        if (expr) {                                                             \
            ++s_pass;                                                           \
        } else {                                                                \
            ++s_fail;                                                           \
            fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr);   \
        }                                                                       \
    } while (0)

static int report(void) {
    printf("%d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}

/* ── Compile-time size assertions ─────────────────────────────────────────── */
/*
 * These fire at compile time and are the primary correctness guarantee.
 * If any typedef maps to the wrong underlying type on this platform, the
 * build fails immediately with a clear diagnostic — no need to run the
 * binary at all.
 */
typedef char check_u8_size   [ sizeof(u8)    == 1 ? 1 : -1 ];
typedef char check_u16_size  [ sizeof(u16)   == 2 ? 1 : -1 ];
typedef char check_u32_size  [ sizeof(u32)   == 4 ? 1 : -1 ];
typedef char check_u64_size  [ sizeof(u64)   == 8 ? 1 : -1 ];

typedef char check_i8_size   [ sizeof(i8)    == 1 ? 1 : -1 ];
typedef char check_i16_size  [ sizeof(i16)   == 2 ? 1 : -1 ];
typedef char check_i32_size  [ sizeof(i32)   == 4 ? 1 : -1 ];
typedef char check_i64_size  [ sizeof(i64)   == 8 ? 1 : -1 ];

typedef char check_f32_size  [ sizeof(f32)   == 4 ? 1 : -1 ];
typedef char check_f64_size  [ sizeof(f64)   == 8 ? 1 : -1 ];

typedef char check_usize_size[ sizeof(usize) == sizeof(size_t)    ? 1 : -1 ];
typedef char check_isize_size[ sizeof(isize) == sizeof(ptrdiff_t) ? 1 : -1 ];

typedef char check_bool_size [ sizeof(bool)  >= 1 ? 1 : -1 ];

/* ══════════════════════════════════════════════════════════════════════════ */
/*  1. Unsigned integer sizes                                                  */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_unsigned_sizes(void) {
    CHECK(sizeof(u8)  == 1);
    CHECK(sizeof(u16) == 2);
    CHECK(sizeof(u32) == 4);
    CHECK(sizeof(u64) == 8);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  2. Signed integer sizes                                                    */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_signed_sizes(void) {
    CHECK(sizeof(i8)  == 1);
    CHECK(sizeof(i16) == 2);
    CHECK(sizeof(i32) == 4);
    CHECK(sizeof(i64) == 8);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  3. Unsigned integer ranges                                                 */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_unsigned_ranges(void) {
    /* minimum: 0 */
    CHECK((u8)  0 == 0);
    CHECK((u16) 0 == 0);
    CHECK((u32) 0 == 0);
    CHECK((u64) 0 == 0);

    /* maximum: 2^width - 1 */
    CHECK((u8)  0xFF               == 255U);
    CHECK((u16) 0xFFFF             == 65535U);
    CHECK((u32) 0xFFFFFFFFU        == 4294967295UL);
    CHECK((u64) 0xFFFFFFFFFFFFFFFFULL == (u64)-1);

    /* Wrap-around at max + 1 rolls back to 0 */
    u8  u8_max  = 0xFF;
    u16 u16_max = 0xFFFF;
    u32 u32_max = 0xFFFFFFFFU;
    u64 u64_max = 0xFFFFFFFFFFFFFFFFULL;
    CHECK((u8) (u8_max  + 1) == 0);
    CHECK((u16)(u16_max + 1) == 0);
    CHECK((u32)(u32_max + 1) == 0);
    CHECK((u64)(u64_max + 1) == 0);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  4. Signed integer ranges                                                   */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_signed_ranges(void) {
    /* maximum: 2^(width-1) - 1 */
    CHECK((i8)  0x7F             ==  127);
    CHECK((i16) 0x7FFF           ==  32767);
    CHECK((i32) 0x7FFFFFFF       ==  2147483647L);
    CHECK((i64) 0x7FFFFFFFFFFFFFFFLL == (i64)9223372036854775807LL);

    /* minimum: -2^(width-1) */
    CHECK((i8)  (-127 - 1)       == -128);
    CHECK((i16) (-32767 - 1)     == -32768);
    CHECK((i32) (-2147483647L-1) == -2147483648L);
    CHECK((i64) (-9223372036854775807LL - 1) == (i64)(-9223372036854775807LL - 1));

    /* Signedness: negative values compare less than zero */
    CHECK((i8)  -1 < 0);
    CHECK((i16) -1 < 0);
    CHECK((i32) -1 < 0);
    CHECK((i64) -1 < 0);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  5. Signedness: unsigned types never go negative                           */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_signedness(void) {
    /* Casting -1 to an unsigned type yields the max value, not a negative. */
    CHECK((u8)  -1 > 0);
    CHECK((u16) -1 > 0);
    CHECK((u32) -1 > 0);
    CHECK((u64) -1 > 0);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  6. Platform-width types                                                    */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_platform_width_types(void) {
    /* usize matches size_t in both size and behaviour */
    CHECK(sizeof(usize) == sizeof(size_t));
    CHECK((usize)sizeof(int) == sizeof(int));

    /* isize matches ptrdiff_t */
    CHECK(sizeof(isize) == sizeof(ptrdiff_t));

    /* usize can hold the size of the largest object */
    usize big = (usize)-1;   /* max value */
    CHECK(big > 0);

    /* isize can represent a negative pointer difference */
    isize neg = -1;
    CHECK(neg < 0);

    /* Pointer arithmetic round-trip through isize */
    int arr[4] = {0, 1, 2, 3};
    isize diff = (isize)(&arr[3] - &arr[0]);
    CHECK(diff == 3);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  7. Floating-point types                                                    */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_float_types(void) {
    /* Size */
    CHECK(sizeof(f32) == 4);
    CHECK(sizeof(f64) == 8);

    /* f32 has 24 bits of mantissa (IEEE 754 single) */
    CHECK(FLT_MANT_DIG == 24);

    /* f64 has 53 bits of mantissa (IEEE 754 double) */
    CHECK(DBL_MANT_DIG == 53);

    /* Basic arithmetic */
    f32 a = 1.0f;
    f32 b = 2.0f;
    CHECK(a + b == 3.0f);

    f64 x = 1.0;
    f64 y = 3.0;
    /* 1/3 in double is representable and nonzero */
    CHECK(x / y > 0.0);
    CHECK(x / y < 1.0);

    /* f64 is strictly more precise than f32 for this value */
    f32 f32_third = 1.0f / 3.0f;
    f64 f64_third = 1.0  / 3.0;
    CHECK((f64)f32_third != f64_third);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  8. Boolean type                                                            */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_bool_type(void) {
    CHECK(sizeof(bool) >= 1);

    CHECK(true  != false);
    CHECK(true  == 1);
    CHECK(false == 0);

    /* Logical operations */
    CHECK((true  && true)  == true);
    CHECK((true  && false) == false);
    CHECK((false || true)  == true);
    CHECK((false || false) == false);
    CHECK(!true  == false);
    CHECK(!false == true);

    /* Integers convert to bool correctly */
    CHECK((bool)1    == true);
    CHECK((bool)0    == false);
    CHECK((bool)(-1) == true);
    CHECK((bool)255  == true);

    /* bool result of comparison */
    bool lt = (1 < 2);
    bool gt = (2 < 1);
    CHECK(lt == true);
    CHECK(gt == false);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  9. Interoperability between types                                          */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_interop(void) {
    /* u8 promotes to u32 without sign extension */
    u8  byte = 0xFF;
    u32 word = byte;
    CHECK(word == 255U);

    /* i8 sign-extends to i32 */
    i8  sbyte = -1;
    i32 sword = sbyte;
    CHECK(sword == -1);

    /* usize round-trips through u64 on 64-bit targets (or u32 on 32-bit) */
    usize sz = 1024;
    u64   as_u64 = (u64)sz;
    CHECK((usize)as_u64 == sz);

    /* isize round-trips through i64 */
    isize isz = -512;
    i64   as_i64 = (i64)isz;
    CHECK((isize)as_i64 == isz);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  main                                                                       */
/* ══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    test_unsigned_sizes();
    test_signed_sizes();
    test_unsigned_ranges();
    test_signed_ranges();
    test_signedness();
    test_platform_width_types();
    test_float_types();
    test_bool_type();
    test_interop();

    return report();
}
