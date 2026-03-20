/**
 * @file types_test.c
 * @brief Tests for types.h — portable integer and size type aliases
 *
 * Verifies:
 *   - Exact byte widths for all aliases (u8..u64, i8..i64, f32, f64)
 *   - usize / isize platform-width correctness
 *   - Signedness: signed types accept negative values, unsigned wrap
 *   - bool / true / false behave correctly
 *
 * MSVC suppressions (declared in CMakeLists.txt):
 *   /wd4146 — unary minus applied to unsigned type (deliberate wrap-around test)
 *   /wd4308 — negative integral constant converted to unsigned (same)
 */

#include "types.h"
#include <stddef.h>
#include <stdio.h>

/* ── Minimal test harness ────────────────────────────────────────────────── */

static int g_failed = 0;

#define EXPECT(cond)                                                \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL %s:%d  %s\n",                    \
                    __FILE__, __LINE__, #cond);                     \
            g_failed++;                                             \
        }                                                           \
    } while (0)

/* ── Size tests ──────────────────────────────────────────────────────────── */

static void test_unsigned_sizes(void)
{
    EXPECT(sizeof(u8)  == 1);
    EXPECT(sizeof(u16) == 2);
    EXPECT(sizeof(u32) == 4);
    EXPECT(sizeof(u64) == 8);
}

static void test_signed_sizes(void)
{
    EXPECT(sizeof(i8)  == 1);
    EXPECT(sizeof(i16) == 2);
    EXPECT(sizeof(i32) == 4);
    EXPECT(sizeof(i64) == 8);
}

static void test_float_sizes(void)
{
    EXPECT(sizeof(f32) == 4);
    EXPECT(sizeof(f64) == 8);
}

static void test_platform_sizes(void)
{
    EXPECT(sizeof(usize) == sizeof(size_t));
    EXPECT(sizeof(isize) == sizeof(ptrdiff_t));
    EXPECT(sizeof(usize) == sizeof(void *));
    EXPECT(sizeof(isize) == sizeof(void *));
}

/* ── Signedness tests ────────────────────────────────────────────────────── */

static void test_signed_accepts_negative(void)
{
    i8  a = -1;
    i16 b = -1;
    i32 c = -1;
    i64 d = -1;

    EXPECT(a < 0);
    EXPECT(b < 0);
    EXPECT(c < 0);
    EXPECT(d < 0);
}

static void test_unsigned_wraps(void)
{
    /* Deliberate unsigned negation — triggers /wd4146, /wd4308 on MSVC */
    u8  a = (u8) -1;
    u16 b = (u16)-1;
    u32 c = (u32)-1;
    u64 d = (u64)-1;

    EXPECT(a == 0xFF);
    EXPECT(b == 0xFFFF);
    EXPECT(c == 0xFFFFFFFFU);
    EXPECT(d == 0xFFFFFFFFFFFFFFFFULL);
}

/* ── Boundary value tests ────────────────────────────────────────────────── */

static void test_unsigned_boundaries(void)
{
    u8  u8_max  = (u8) 0xFF;
    u16 u16_max = (u16)0xFFFF;
    u32 u32_max = (u32)0xFFFFFFFFU;
    u64 u64_max = (u64)0xFFFFFFFFFFFFFFFFULL;

    EXPECT(u8_max  == 255U);
    EXPECT(u16_max == 65535U);
    EXPECT(u32_max == 4294967295U);
    EXPECT(u64_max == 18446744073709551615ULL);

    EXPECT((u8) (u8_max  + 1U) == 0U);
    EXPECT((u16)(u16_max + 1U) == 0U);
    EXPECT((u32)(u32_max + 1U) == 0U);
    EXPECT((u64)(u64_max + 1U) == 0U);
}

static void test_signed_boundaries(void)
{
    /* Avoid implementation-defined or UB forms for MIN constants.      */
    /* Pattern: -(MAX) - 1, kept within the signed type at each step.  */
    i8  i8_max  = (i8) 127;
    i8  i8_min  = (i8) ((i8) -127 - (i8) 1);
    i16 i16_max = (i16) 32767;
    i16 i16_min = (i16)((i16)-32767 - (i16)1);
    i32 i32_max = (i32) 2147483647;
    i32 i32_min = (i32)(-2147483647 - 1);
    i64 i64_max = (i64) 9223372036854775807LL;
    i64 i64_min = (i64)(-9223372036854775807LL - 1LL);

    EXPECT(i8_max  ==  127);
    EXPECT(i8_min  == -128);
    EXPECT(i16_max ==  32767);
    EXPECT(i16_min == -32768);
    EXPECT(i32_max ==  2147483647);
    EXPECT(i32_min == -2147483647 - 1);
    EXPECT(i64_max ==  9223372036854775807LL);
    EXPECT(i64_min == -9223372036854775807LL - 1LL);
}

/* ── Float sanity tests ──────────────────────────────────────────────────── */

static void test_float_arithmetic(void)
{
    f32 a = 1.0f;
    f32 b = 2.0f;
    f32 c = a + b;
    EXPECT(c == 3.0f);

    f64 x = 1.0;
    f64 y = 2.0;
    f64 z = x + y;
    EXPECT(z == 3.0);
}

/* ── Boolean tests ───────────────────────────────────────────────────────── */

static void test_bool(void)
{
    bool t = true;
    bool f = false;

    EXPECT(t  == true);
    EXPECT(f  == false);
    EXPECT(!t == false);
    EXPECT(!f == true);
    EXPECT(sizeof(bool) >= 1);
}

/* ── usize / isize semantic tests ────────────────────────────────────────── */

static void test_usize_isize_semantics(void)
{
    usize zero = 0;
    usize one  = 1;
    EXPECT(one > zero);

    isize neg = -1;
    isize pos =  1;
    EXPECT(neg < pos);
    EXPECT(neg <  0);
    EXPECT(pos >  0);

    /* Round-trip through pointer arithmetic */
    int   arr[4] = {0, 1, 2, 3};
    usize idx    = 2;
    EXPECT(arr[idx] == 2);

    isize diff = (isize)(&arr[3] - &arr[0]);
    EXPECT(diff == 3);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    test_unsigned_sizes();
    test_signed_sizes();
    test_float_sizes();
    test_platform_sizes();

    test_signed_accepts_negative();
    test_unsigned_wraps();

    test_unsigned_boundaries();
    test_signed_boundaries();

    test_float_arithmetic();
    test_bool();
    test_usize_isize_semantics();

    if (g_failed == 0) {
        printf("OK  types_test  (all assertions passed)\n");
        return 0;
    }

    fprintf(stderr, "FAILED  types_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}
