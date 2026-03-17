#include <stdio.h>
#include <stdlib.h>
#include "checked.h"

/* ============================================================================
 * Test Helpers
 * ========================================================================= */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(label, expected, actual)                                      \
    do {                                                                        \
        tests_run++;                                                            \
        if ((u64)(expected) == (u64)(actual)) {                                 \
            tests_passed++;                                                     \
        } else {                                                                \
            tests_failed++;                                                     \
            fprintf(stderr, "FAIL: %s — expected %llu, got %llu  (%s:%d)\n",   \
                    label, (unsigned long long)(expected),                      \
                    (unsigned long long)(actual), __FILE__, __LINE__);          \
        }                                                                       \
    } while (0)

#define ASSERT_TRUE(label, expr)  ASSERT_EQ(label, 1, !!(expr))
#define ASSERT_FALSE(label, expr) ASSERT_EQ(label, 0, !!(expr))

/* Helper: assert the operation succeeds AND produces the expected value */
#define ASSERT_OK(label, expected, ok, actual)                                  \
    do {                                                                        \
        ASSERT_TRUE(label " (no overflow)", ok);                                \
        ASSERT_EQ  (label " (value)",       expected, actual);                  \
    } while (0)

/* Helper: assert the operation reports overflow (return value = false) */
#define ASSERT_OVERFLOW(label, ok) \
    ASSERT_FALSE(label " (overflow detected)", ok)

/* ============================================================================
 * checked_add (usize)
 * ========================================================================= */

static void test_checked_add(void) {
    usize r;

    /* Normal cases */
    ASSERT_OK("add: 0+0",       0ULL,  checked_add(0, 0, &r),              r);
    ASSERT_OK("add: 1+1",       2ULL,  checked_add(1, 1, &r),              r);
    ASSERT_OK("add: large+0",   100ULL,checked_add(100, 0, &r),            r);
    ASSERT_OK("add: near-max",  (usize)(~0ULL - 1),
                                       checked_add((usize)(~0ULL - 1), 0, &r), r);

    /* Boundary: USIZE_MAX exactly */
    ASSERT_OK("add: USIZE_MAX",  (usize)~0ULL,
                                        checked_add((usize)~0ULL - 1, 1, &r), r);

    /* Overflow: USIZE_MAX + 1 */
    ASSERT_OVERFLOW("add: USIZE_MAX+1", checked_add((usize)~0ULL, 1, &r));

    /* Overflow: USIZE_MAX + USIZE_MAX */
    ASSERT_OVERFLOW("add: MAX+MAX",     checked_add((usize)~0ULL, (usize)~0ULL, &r));
}

/* ============================================================================
 * checked_add_u8 / u16 / u32 / u64
 * ========================================================================= */

static void test_checked_add_sized(void) {
    u8  r8;
    u16 r16;
    u32 r32;
    u64 r64;

    /* u8 */
    ASSERT_OK      ("add_u8: 100+50",   150U,  checked_add_u8(100, 50,  &r8),  r8);
    ASSERT_OK      ("add_u8: 255+0",    255U,  checked_add_u8(255,  0,  &r8),  r8);
    ASSERT_OVERFLOW("add_u8: 255+1",           checked_add_u8(255,  1,  &r8));
    ASSERT_OVERFLOW("add_u8: 200+100",         checked_add_u8(200, 100, &r8));

    /* u16 */
    ASSERT_OK      ("add_u16: 60000+5534",  65534U, checked_add_u16(60000, 5534, &r16), r16);
    ASSERT_OK      ("add_u16: 65535+0",     65535U, checked_add_u16(65535,    0, &r16), r16);
    ASSERT_OVERFLOW("add_u16: 65535+1",             checked_add_u16(65535,    1, &r16));

    /* u32 */
    ASSERT_OK      ("add_u32: UINT32_MAX+0",  0xFFFFFFFFU, checked_add_u32(0xFFFFFFFFU, 0, &r32), r32);
    ASSERT_OVERFLOW("add_u32: UINT32_MAX+1",              checked_add_u32(0xFFFFFFFFU, 1, &r32));

    /* u64 */
    ASSERT_OK      ("add_u64: 1+1",  2ULL,  checked_add_u64(1, 1, &r64), r64);
    ASSERT_OVERFLOW("add_u64: MAX+1",       checked_add_u64(~0ULL, 1, &r64));
}

/* ============================================================================
 * checked_sub (usize)
 * ========================================================================= */

static void test_checked_sub(void) {
    usize r;

    /* Normal cases */
    ASSERT_OK("sub: 10-3",   7ULL, checked_sub(10, 3, &r),  r);
    ASSERT_OK("sub: 5-5",    0ULL, checked_sub(5,  5, &r),  r);
    ASSERT_OK("sub: 0-0",    0ULL, checked_sub(0,  0, &r),  r);
    ASSERT_OK("sub: MAX-MAX", 0ULL, checked_sub((usize)~0ULL, (usize)~0ULL, &r), r);

    /* Underflow */
    ASSERT_OVERFLOW("sub: 0-1",   checked_sub(0, 1, &r));
    ASSERT_OVERFLOW("sub: 5-6",   checked_sub(5, 6, &r));
    ASSERT_OVERFLOW("sub: 0-MAX", checked_sub(0, (usize)~0ULL, &r));
}

/* ============================================================================
 * checked_sub_u8 / u16 / u32 / u64
 * ========================================================================= */

static void test_checked_sub_sized(void) {
    u8  r8;
    u16 r16;
    u32 r32;
    u64 r64;

    /* u8 */
    ASSERT_OK      ("sub_u8: 10-3",   7U, checked_sub_u8(10, 3, &r8), r8);
    ASSERT_OK      ("sub_u8: 0-0",    0U, checked_sub_u8(0,  0, &r8), r8);
    ASSERT_OVERFLOW("sub_u8: 0-1",       checked_sub_u8(0,  1, &r8));
    ASSERT_OVERFLOW("sub_u8: 5-6",       checked_sub_u8(5,  6, &r8));

    /* u16 */
    ASSERT_OK      ("sub_u16: 1000-999",  1U, checked_sub_u16(1000, 999, &r16), r16);
    ASSERT_OVERFLOW("sub_u16: 0-1",           checked_sub_u16(0, 1, &r16));

    /* u32 */
    ASSERT_OK      ("sub_u32: MAX-1",  0xFFFFFFFEU, checked_sub_u32(0xFFFFFFFFU, 1, &r32), r32);
    ASSERT_OVERFLOW("sub_u32: 0-1",                 checked_sub_u32(0, 1, &r32));

    /* u64 */
    ASSERT_OK      ("sub_u64: MAX-MAX",  0ULL, checked_sub_u64(~0ULL, ~0ULL, &r64), r64);
    ASSERT_OVERFLOW("sub_u64: 0-1",           checked_sub_u64(0, 1, &r64));
}

/* ============================================================================
 * checked_mul (usize)
 * ========================================================================= */

static void test_checked_mul(void) {
    usize r;

    /* Normal cases */
    ASSERT_OK("mul: 0*anything",  0ULL, checked_mul(0, (usize)~0ULL, &r), r);
    ASSERT_OK("mul: anything*0",  0ULL, checked_mul((usize)~0ULL, 0, &r), r);
    ASSERT_OK("mul: 1*1",         1ULL, checked_mul(1, 1, &r),            r);
    ASSERT_OK("mul: 100*100",  10000ULL,checked_mul(100, 100, &r),        r);

    /* Boundary: large but valid */
    ASSERT_OK("mul: 2*(MAX/2)", (usize)(~0ULL & ~1ULL),
                                       checked_mul(2, (usize)~0ULL / 2, &r), r);

    /* Overflow */
    ASSERT_OVERFLOW("mul: MAX*2",       checked_mul((usize)~0ULL, 2, &r));
    ASSERT_OVERFLOW("mul: MAX*MAX",     checked_mul((usize)~0ULL, (usize)~0ULL, &r));
    ASSERT_OVERFLOW("mul: large overflow", checked_mul((usize)~0ULL / 2 + 2, (usize)2, &r));
}

/* ============================================================================
 * checked_mul_u8 / u16 / u32 / u64
 * ========================================================================= */

static void test_checked_mul_sized(void) {
    u8  r8;
    u16 r16;
    u32 r32;
    u64 r64;

    /* u8 */
    ASSERT_OK      ("mul_u8: 10*10",  100U, checked_mul_u8(10, 10, &r8), r8);
    ASSERT_OK      ("mul_u8: 0*255",    0U, checked_mul_u8( 0,255, &r8), r8);
    ASSERT_OK      ("mul_u8: 255*1",  255U, checked_mul_u8(255,  1, &r8), r8);
    ASSERT_OVERFLOW("mul_u8: 16*16",       checked_mul_u8(16,  16, &r8)); /* 256 > 255 */
    ASSERT_OVERFLOW("mul_u8: 255*2",       checked_mul_u8(255,   2, &r8));

    /* u16 */
    ASSERT_OK      ("mul_u16: 256*255",  65280U, checked_mul_u16(256, 255, &r16), r16);
    ASSERT_OVERFLOW("mul_u16: 256*256",          checked_mul_u16(256, 256, &r16)); /* 65536 > 65535 */

    /* u32 */
    ASSERT_OK      ("mul_u32: 65535*65535", 0xFFFE0001UL, checked_mul_u32(65535, 65535, &r32), r32);
    ASSERT_OVERFLOW("mul_u32: 65536*65536",              checked_mul_u32(65536, 65536, &r32));

    /* u64 */
    ASSERT_OK      ("mul_u64: 1000*1000", 1000000ULL, checked_mul_u64(1000, 1000, &r64), r64);
    ASSERT_OVERFLOW("mul_u64: MAX*2",                  checked_mul_u64(~0ULL, 2, &r64));
}

/* ============================================================================
 * checked_add_isize (signed)
 * ========================================================================= */

static void test_checked_add_isize(void) {
    isize r;

    /* Normal cases */
    ASSERT_OK("iadd: 1+1",       2,    checked_add_isize(1,  1,  &r), r);
    ASSERT_OK("iadd: -1+(-1)",  -2,    checked_add_isize(-1, -1, &r), r);
    ASSERT_OK("iadd: -5+5",      0,    checked_add_isize(-5,  5, &r), r);
    ASSERT_OK("iadd: 0+0",       0,    checked_add_isize(0,   0, &r), r);

    /* Boundary: exact max/min */
    ASSERT_OK("iadd: MAX+0",   CANON_ISIZE_MAX, checked_add_isize(CANON_ISIZE_MAX, 0,  &r), r);
    ASSERT_OK("iadd: MIN+0",   CANON_ISIZE_MIN, checked_add_isize(CANON_ISIZE_MIN, 0,  &r), r);
    ASSERT_OK("iadd: MAX+MIN",  -1,             checked_add_isize(CANON_ISIZE_MAX, CANON_ISIZE_MIN, &r), r);

    /* Positive overflow */
    ASSERT_OVERFLOW("iadd: MAX+1",   checked_add_isize(CANON_ISIZE_MAX, 1, &r));
    ASSERT_OVERFLOW("iadd: MAX+MAX", checked_add_isize(CANON_ISIZE_MAX, CANON_ISIZE_MAX, &r));

    /* Negative overflow (underflow) */
    ASSERT_OVERFLOW("iadd: MIN+(-1)",   checked_add_isize(CANON_ISIZE_MIN, -1, &r));
    ASSERT_OVERFLOW("iadd: MIN+MIN",    checked_add_isize(CANON_ISIZE_MIN, CANON_ISIZE_MIN, &r));

    /* Mixed signs never overflow */
    ASSERT_TRUE("iadd: no overflow (mixed signs)", checked_add_isize(CANON_ISIZE_MAX, CANON_ISIZE_MIN, &r));
}

/* ============================================================================
 * checked_sub_isize (signed)
 * ========================================================================= */

static void test_checked_sub_isize(void) {
    isize r;

    /* Normal cases */
    ASSERT_OK("isub: 5-3",      2,  checked_sub_isize(5,   3,  &r), r);
    ASSERT_OK("isub: -5-(-3)", -2,  checked_sub_isize(-5, -3,  &r), r);
    ASSERT_OK("isub: 0-0",      0,  checked_sub_isize(0,   0,  &r), r);
    ASSERT_OK("isub: 0-1",     -1,  checked_sub_isize(0,   1,  &r), r);
    ASSERT_OK("isub: MIN-0",  CANON_ISIZE_MIN, checked_sub_isize(CANON_ISIZE_MIN, 0, &r), r);
    ASSERT_OK("isub: MAX-0",  CANON_ISIZE_MAX, checked_sub_isize(CANON_ISIZE_MAX, 0, &r), r);

    /* Overflow: subtracting a negative from MAX → too positive */
    ASSERT_OVERFLOW("isub: MAX-(-1)",  checked_sub_isize(CANON_ISIZE_MAX, -1,  &r));
    ASSERT_OVERFLOW("isub: MAX-MIN",   checked_sub_isize(CANON_ISIZE_MAX, CANON_ISIZE_MIN, &r));

    /* Underflow: subtracting a positive from MIN → too negative */
    ASSERT_OVERFLOW("isub: MIN-1",     checked_sub_isize(CANON_ISIZE_MIN, 1,   &r));
    ASSERT_OVERFLOW("isub: MIN-MAX",   checked_sub_isize(CANON_ISIZE_MIN, CANON_ISIZE_MAX, &r));
}

/* ============================================================================
 * checked_mul_isize (signed)
 * ========================================================================= */

static void test_checked_mul_isize(void) {
    isize r;

    /* Zero short-circuits */
    ASSERT_OK("imul: 0*MAX",   0, checked_mul_isize(0, CANON_ISIZE_MAX, &r), r);
    ASSERT_OK("imul: MAX*0",   0, checked_mul_isize(CANON_ISIZE_MAX, 0, &r), r);
    ASSERT_OK("imul: 0*MIN",   0, checked_mul_isize(0, CANON_ISIZE_MIN, &r), r);

    /* Normal cases */
    ASSERT_OK("imul: 3*4",    12, checked_mul_isize(3,   4, &r), r);
    ASSERT_OK("imul: -3*4",  -12, checked_mul_isize(-3,  4, &r), r);
    ASSERT_OK("imul: 3*(-4)",-12, checked_mul_isize(3,  -4, &r), r);
    ASSERT_OK("imul: -3*(-4)", 12,checked_mul_isize(-3, -4, &r), r);
    ASSERT_OK("imul: 1*MAX",  CANON_ISIZE_MAX, checked_mul_isize(1, CANON_ISIZE_MAX, &r), r);
    ASSERT_OK("imul: -1*MAX", -CANON_ISIZE_MAX, checked_mul_isize(-1, CANON_ISIZE_MAX, &r), r);

    /* (+,+) overflow */
    ASSERT_OVERFLOW("imul: MAX*2",   checked_mul_isize(CANON_ISIZE_MAX, 2, &r));
    ASSERT_OVERFLOW("imul: MAX*MAX", checked_mul_isize(CANON_ISIZE_MAX, CANON_ISIZE_MAX, &r));

    /* (-,-) overflow: result would exceed MAX */
    ASSERT_OVERFLOW("imul: MIN*(-1)", checked_mul_isize(CANON_ISIZE_MIN, -1, &r));
    ASSERT_OVERFLOW("imul: MIN*MIN",  checked_mul_isize(CANON_ISIZE_MIN, CANON_ISIZE_MIN, &r));

    /* (+,-) and (-,+) underflow */
    ASSERT_OVERFLOW("imul: MAX*(-2)",  checked_mul_isize(CANON_ISIZE_MAX, -2, &r));
    ASSERT_OVERFLOW("imul: MIN*2",     checked_mul_isize(CANON_ISIZE_MIN,  2, &r));
}

/* ============================================================================
 * checked_min / checked_max / checked_clamp (macros)
 * ========================================================================= */

static void test_checked_min_max_clamp(void) {
    /* min */
    ASSERT_EQ("min: 3,5",        3,   checked_min(3, 5));
    ASSERT_EQ("min: 5,3",        3,   checked_min(5, 3));
    ASSERT_EQ("min: equal",      4,   checked_min(4, 4));
    ASSERT_EQ("min: negative",  -5,   checked_min(-5, -3));
    ASSERT_EQ("min: 0,MAX",      0,   checked_min(0, (usize)~0ULL));

    /* max */
    ASSERT_EQ("max: 3,5",        5,   checked_max(3, 5));
    ASSERT_EQ("max: 5,3",        5,   checked_max(5, 3));
    ASSERT_EQ("max: equal",      4,   checked_max(4, 4));
    ASSERT_EQ("max: negative",  -3,   checked_max(-5, -3));

    /* clamp */
    ASSERT_EQ("clamp: in range",    5,   checked_clamp(5, 1, 10));
    ASSERT_EQ("clamp: below lo",    1,   checked_clamp(0, 1, 10));
    ASSERT_EQ("clamp: above hi",   10,   checked_clamp(15, 1, 10));
    ASSERT_EQ("clamp: at lo",       1,   checked_clamp(1, 1, 10));
    ASSERT_EQ("clamp: at hi",      10,   checked_clamp(10, 1, 10));
    ASSERT_EQ("clamp: lo==hi",      7,   checked_clamp(5,  7, 7));
    ASSERT_EQ("clamp: negative",   -3,   checked_clamp(-5, -3, 0));
}

/* ============================================================================
 * Main
 * ========================================================================= */

int main(void) {
    test_checked_add();
    test_checked_add_sized();

    test_checked_sub();
    test_checked_sub_sized();

    test_checked_mul();
    test_checked_mul_sized();

    test_checked_add_isize();
    test_checked_sub_isize();
    test_checked_mul_isize();

    test_checked_min_max_clamp();

    printf("\nResults: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED\n", tests_failed);
        return 1;
    }
    printf(" — all tests passed!\n");
    return 0;
}
