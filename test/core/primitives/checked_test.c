/**
 * @file checked_test.c
 * @brief Unit tests (and optional fuzz harness) for checked.h
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
 *   checked_add  / checked_add_u8  / checked_add_u16  / checked_add_u32  / checked_add_u64
 *   checked_sub  / checked_sub_u8  / checked_sub_u16  / checked_sub_u32  / checked_sub_u64
 *   checked_mul  / checked_mul_u8  / checked_mul_u16  / checked_mul_u32  / checked_mul_u64
 *   checked_add_isize / checked_sub_isize / checked_mul_isize
 *   checked_min  / checked_max     / checked_clamp
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   No 0b binary literals (C23/GNU extension). Loop variables declared
 *   before the loop body (C99). The TEST() macro uses CANON_MAYBE_UNUSED
 *   to suppress -Wunused-function in the fuzz twin build.
 */

#include "checked.h"
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

/* =========================================================================
 * checked_add (usize)
 * ====================================================================== */

TEST(checked_add_op) {
    usize result = 0;

    /* Normal cases */
    EXPECT(checked_add(0, 0, &result) == true  && result == 0);
    EXPECT(checked_add(1, 1, &result) == true  && result == 2);
    EXPECT(checked_add(100, 200, &result) == true && result == 300);

    /* Boundary: max - 1 + 1 == max */
    EXPECT(checked_add(CANON_USIZE_MAX - 1, 1, &result) == true &&
           result == CANON_USIZE_MAX);

    /* Overflow: max + 1 */
    EXPECT(checked_add(CANON_USIZE_MAX, 1, &result) == false);

    /* Overflow: max + max */
    EXPECT(checked_add(CANON_USIZE_MAX, CANON_USIZE_MAX, &result) == false);

    /* Zero identity */
    EXPECT(checked_add(42, 0, &result) == true && result == 42);
    EXPECT(checked_add(0, 42, &result) == true && result == 42);
}

/* =========================================================================
 * checked_add_u8
 * ====================================================================== */

TEST(checked_add_u8_op) {
    u8 result = 0;

    EXPECT(checked_add_u8(0, 0, &result) == true  && result == 0);
    EXPECT(checked_add_u8(1, 1, &result) == true  && result == 2);
    EXPECT(checked_add_u8(100, 55, &result) == true && result == 155);
    EXPECT(checked_add_u8(254, 1, &result) == true  && result == 255);
    EXPECT(checked_add_u8(255, 1, &result) == false);
    EXPECT(checked_add_u8(255, 255, &result) == false);
    EXPECT(checked_add_u8(128, 128, &result) == false);
    EXPECT(checked_add_u8(0, 255, &result) == true && result == 255);
}

/* =========================================================================
 * checked_add_u16
 * ====================================================================== */

TEST(checked_add_u16_op) {
    u16 result = 0;

    EXPECT(checked_add_u16(0, 0, &result) == true && result == 0);
    EXPECT(checked_add_u16(1000, 1000, &result) == true && result == 2000);
    EXPECT(checked_add_u16(65534, 1, &result) == true  && result == 65535);
    EXPECT(checked_add_u16(65535, 1, &result) == false);
    EXPECT(checked_add_u16(65535, 65535, &result) == false);
    EXPECT(checked_add_u16(32768, 32767, &result) == true && result == 65535);
    EXPECT(checked_add_u16(32768, 32768, &result) == false);
}

/* =========================================================================
 * checked_add_u32
 * ====================================================================== */

TEST(checked_add_u32_op) {
    u32 result = 0;

    EXPECT(checked_add_u32(0, 0, &result) == true && result == 0);
    EXPECT(checked_add_u32(0xFFFFFFFEU, 1, &result) == true  && result == 0xFFFFFFFFU);
    EXPECT(checked_add_u32(0xFFFFFFFFU, 1, &result) == false);
    EXPECT(checked_add_u32(0xFFFFFFFFU, 0xFFFFFFFFU, &result) == false);
    EXPECT(checked_add_u32(1000000, 2000000, &result) == true && result == 3000000);
}

/* =========================================================================
 * checked_add_u64
 * ====================================================================== */

TEST(checked_add_u64_op) {
    u64 result = 0;

    EXPECT(checked_add_u64(0, 0, &result) == true && result == 0);
    EXPECT(checked_add_u64(0xFFFFFFFFFFFFFFFEULL, 1, &result) == true &&
           result == 0xFFFFFFFFFFFFFFFFULL);
    EXPECT(checked_add_u64(0xFFFFFFFFFFFFFFFFULL, 1, &result) == false);
    EXPECT(checked_add_u64(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, &result) == false);
    EXPECT(checked_add_u64(1000000000ULL, 2000000000ULL, &result) == true &&
           result == 3000000000ULL);
}

/* =========================================================================
 * checked_sub (usize)
 * ====================================================================== */

TEST(checked_sub_op) {
    usize result = 0;

    EXPECT(checked_sub(0, 0, &result) == true  && result == 0);
    EXPECT(checked_sub(10, 3, &result) == true  && result == 7);
    EXPECT(checked_sub(100, 100, &result) == true && result == 0);
    EXPECT(checked_sub(CANON_USIZE_MAX, CANON_USIZE_MAX, &result) == true && result == 0);
    EXPECT(checked_sub(CANON_USIZE_MAX, 0, &result) == true && result == CANON_USIZE_MAX);

    /* Underflow */
    EXPECT(checked_sub(0, 1, &result) == false);
    EXPECT(checked_sub(5, 6, &result) == false);
    EXPECT(checked_sub(0, CANON_USIZE_MAX, &result) == false);
}

/* =========================================================================
 * checked_sub_u8 / u16 / u32 / u64
 * ====================================================================== */

TEST(checked_sub_typed_op) {
    u8  r8  = 0;
    u16 r16 = 0;
    u32 r32 = 0;
    u64 r64 = 0;

    /* u8 */
    EXPECT(checked_sub_u8(10, 3, &r8) == true  && r8 == 7);
    EXPECT(checked_sub_u8(0, 0, &r8)  == true  && r8 == 0);
    EXPECT(checked_sub_u8(255, 255, &r8) == true && r8 == 0);
    EXPECT(checked_sub_u8(0, 1, &r8)  == false);
    EXPECT(checked_sub_u8(5, 6, &r8)  == false);

    /* u16 */
    EXPECT(checked_sub_u16(1000, 500, &r16) == true  && r16 == 500);
    EXPECT(checked_sub_u16(0, 1, &r16)      == false);
    EXPECT(checked_sub_u16(65535, 65535, &r16) == true && r16 == 0);

    /* u32 */
    EXPECT(checked_sub_u32(0xFFFFFFFFU, 1, &r32) == true && r32 == 0xFFFFFFFEU);
    EXPECT(checked_sub_u32(0, 1, &r32)           == false);

    /* u64 */
    EXPECT(checked_sub_u64(0xFFFFFFFFFFFFFFFFULL, 1, &r64) == true &&
           r64 == 0xFFFFFFFFFFFFFFFEULL);
    EXPECT(checked_sub_u64(0, 1, &r64) == false);
}

/* =========================================================================
 * checked_mul (usize)
 * ====================================================================== */

TEST(checked_mul_op) {
    usize result = 0;

    EXPECT(checked_mul(0, 0, &result) == true && result == 0);
    EXPECT(checked_mul(0, CANON_USIZE_MAX, &result) == true && result == 0);
    EXPECT(checked_mul(CANON_USIZE_MAX, 0, &result) == true && result == 0);
    EXPECT(checked_mul(1, 1, &result) == true && result == 1);
    EXPECT(checked_mul(100, 200, &result) == true && result == 20000);
    EXPECT(checked_mul(CANON_USIZE_MAX, 1, &result) == true && result == CANON_USIZE_MAX);
    EXPECT(checked_mul(1, CANON_USIZE_MAX, &result) == true && result == CANON_USIZE_MAX);

    /* Overflow */
    EXPECT(checked_mul(CANON_USIZE_MAX, 2, &result) == false);
    EXPECT(checked_mul(2, CANON_USIZE_MAX, &result) == false);
    EXPECT(checked_mul(CANON_USIZE_MAX, CANON_USIZE_MAX, &result) == false);
}

/* =========================================================================
 * checked_mul_u8 / u16 / u32 / u64
 * ====================================================================== */

TEST(checked_mul_typed_op) {
    u8  r8  = 0;
    u16 r16 = 0;
    u32 r32 = 0;
    u64 r64 = 0;

    /* u8 */
    EXPECT(checked_mul_u8(0, 255, &r8) == true  && r8 == 0);
    EXPECT(checked_mul_u8(1, 255, &r8) == true  && r8 == 255);
    EXPECT(checked_mul_u8(15, 17, &r8) == true  && r8 == 255);
    EXPECT(checked_mul_u8(16, 16, &r8) == true  && r8 == 0);  /* 256 overflows to 0 */
    EXPECT(checked_mul_u8(16, 16, &r8) == false);
    EXPECT(checked_mul_u8(2, 128, &r8) == false);
    EXPECT(checked_mul_u8(255, 2,  &r8) == false);

    /* u16 */
    EXPECT(checked_mul_u16(256, 256, &r16) == true  && r16 == 0);    /* 65536 overflows */
    EXPECT(checked_mul_u16(256, 256, &r16) == false);
    EXPECT(checked_mul_u16(255, 257, &r16) == true  && r16 == 65535);
    EXPECT(checked_mul_u16(0, 65535, &r16) == true  && r16 == 0);

    /* u32 */
    EXPECT(checked_mul_u32(0x10000U, 0x10000U, &r32) == false);
    EXPECT(checked_mul_u32(0xFFFFU, 0x10001U, &r32)  == true && r32 == 0xFFFFFFFFU);
    EXPECT(checked_mul_u32(1000, 1000, &r32) == true && r32 == 1000000);

    /* u64 */
    EXPECT(checked_mul_u64(0xFFFFFFFFULL, 0xFFFFFFFFULL, &r64) == true &&
           r64 == 0xFFFFFFFE00000001ULL);
    EXPECT(checked_mul_u64(0x100000000ULL, 0x100000000ULL, &r64) == false);
    EXPECT(checked_mul_u64(0, 0xFFFFFFFFFFFFFFFFULL, &r64) == true && r64 == 0);
}

/* =========================================================================
 * checked_add_isize
 * ====================================================================== */

TEST(checked_add_isize_op) {
    isize result = 0;

    /* Normal */
    EXPECT(checked_add_isize(0, 0, &result)   == true && result == 0);
    EXPECT(checked_add_isize(1, 1, &result)   == true && result == 2);
    EXPECT(checked_add_isize(-1, -1, &result) == true && result == -2);
    EXPECT(checked_add_isize(100, -50, &result) == true && result == 50);
    EXPECT(checked_add_isize(-100, 50, &result) == true && result == -50);

    /* Boundary */
    EXPECT(checked_add_isize(CANON_ISIZE_MAX, 0, &result) == true &&
           result == CANON_ISIZE_MAX);
    EXPECT(checked_add_isize(CANON_ISIZE_MIN, 0, &result) == true &&
           result == CANON_ISIZE_MIN);
    EXPECT(checked_add_isize(CANON_ISIZE_MAX - 1, 1, &result) == true &&
           result == CANON_ISIZE_MAX);
    EXPECT(checked_add_isize(CANON_ISIZE_MIN + 1, -1, &result) == true &&
           result == CANON_ISIZE_MIN);

    /* Overflow */
    EXPECT(checked_add_isize(CANON_ISIZE_MAX, 1, &result)  == false);
    EXPECT(checked_add_isize(CANON_ISIZE_MIN, -1, &result) == false);
    EXPECT(checked_add_isize(CANON_ISIZE_MAX, CANON_ISIZE_MAX, &result) == false);
    EXPECT(checked_add_isize(CANON_ISIZE_MIN, CANON_ISIZE_MIN, &result) == false);

    /* Mixed signs never overflow */
    EXPECT(checked_add_isize(CANON_ISIZE_MAX, CANON_ISIZE_MIN, &result) == true &&
           result == -1);
    EXPECT(checked_add_isize(CANON_ISIZE_MIN, CANON_ISIZE_MAX, &result) == true &&
           result == -1);
}

/* =========================================================================
 * checked_sub_isize
 * ====================================================================== */

TEST(checked_sub_isize_op) {
    isize result = 0;

    /* Normal */
    EXPECT(checked_sub_isize(0, 0, &result)   == true && result == 0);
    EXPECT(checked_sub_isize(5, 3, &result)   == true && result == 2);
    EXPECT(checked_sub_isize(-5, -3, &result) == true && result == -2);
    EXPECT(checked_sub_isize(0, 1, &result)   == true && result == -1);
    EXPECT(checked_sub_isize(1, -1, &result)  == true && result == 2);

    /* Boundary */
    EXPECT(checked_sub_isize(CANON_ISIZE_MIN, 0, &result) == true &&
           result == CANON_ISIZE_MIN);
    EXPECT(checked_sub_isize(CANON_ISIZE_MAX, 0, &result) == true &&
           result == CANON_ISIZE_MAX);
    EXPECT(checked_sub_isize(CANON_ISIZE_MIN + 1, 1, &result) == true &&
           result == CANON_ISIZE_MIN);
    EXPECT(checked_sub_isize(CANON_ISIZE_MAX - 1, -1, &result) == true &&
           result == CANON_ISIZE_MAX);

    /* Overflow */
    EXPECT(checked_sub_isize(CANON_ISIZE_MIN, 1, &result)  == false);
    EXPECT(checked_sub_isize(CANON_ISIZE_MAX, -1, &result) == false);
    EXPECT(checked_sub_isize(CANON_ISIZE_MIN, CANON_ISIZE_MAX, &result) == false);
    EXPECT(checked_sub_isize(CANON_ISIZE_MAX, CANON_ISIZE_MIN, &result) == false);
}

/* =========================================================================
 * checked_mul_isize
 * ====================================================================== */

TEST(checked_mul_isize_op) {
    isize result = 0;

    /* Zero */
    EXPECT(checked_mul_isize(0, 0, &result)            == true && result == 0);
    EXPECT(checked_mul_isize(0, CANON_ISIZE_MAX, &result) == true && result == 0);
    EXPECT(checked_mul_isize(CANON_ISIZE_MAX, 0, &result) == true && result == 0);
    EXPECT(checked_mul_isize(0, CANON_ISIZE_MIN, &result) == true && result == 0);
    EXPECT(checked_mul_isize(CANON_ISIZE_MIN, 0, &result) == true && result == 0);

    /* Identity */
    EXPECT(checked_mul_isize(1, 1, &result)   == true && result == 1);
    EXPECT(checked_mul_isize(-1, 1, &result)  == true && result == -1);
    EXPECT(checked_mul_isize(1, -1, &result)  == true && result == -1);
    EXPECT(checked_mul_isize(-1, -1, &result) == true && result == 1);

    /* ISIZE_MAX * 1 and ISIZE_MIN * 1 */
    EXPECT(checked_mul_isize(CANON_ISIZE_MAX, 1, &result) == true &&
           result == CANON_ISIZE_MAX);
    EXPECT(checked_mul_isize(1, CANON_ISIZE_MAX, &result) == true &&
           result == CANON_ISIZE_MAX);
    EXPECT(checked_mul_isize(CANON_ISIZE_MIN, 1, &result) == true &&
           result == CANON_ISIZE_MIN);
    EXPECT(checked_mul_isize(1, CANON_ISIZE_MIN, &result) == true &&
           result == CANON_ISIZE_MIN);

    /* ISIZE_MIN special cases */
    EXPECT(checked_mul_isize(CANON_ISIZE_MIN, -1, &result) == false);
    EXPECT(checked_mul_isize(-1, CANON_ISIZE_MIN, &result) == false);
    EXPECT(checked_mul_isize(CANON_ISIZE_MIN, 2, &result)  == false);
    EXPECT(checked_mul_isize(2, CANON_ISIZE_MIN, &result)  == false);

    /* Normal overflow */
    EXPECT(checked_mul_isize(CANON_ISIZE_MAX, 2, &result)  == false);
    EXPECT(checked_mul_isize(2, CANON_ISIZE_MAX, &result)  == false);
    EXPECT(checked_mul_isize(CANON_ISIZE_MAX, CANON_ISIZE_MAX, &result) == false);
    EXPECT(checked_mul_isize(CANON_ISIZE_MIN, CANON_ISIZE_MIN, &result) == false);

    /* Normal values */
    EXPECT(checked_mul_isize(100, 200, &result) == true && result == 20000);
    EXPECT(checked_mul_isize(-100, 200, &result) == true && result == -20000);
    EXPECT(checked_mul_isize(100, -200, &result) == true && result == -20000);
    EXPECT(checked_mul_isize(-100, -200, &result) == true && result == 20000);
}

/* =========================================================================
 * checked_min / checked_max / checked_clamp
 * ====================================================================== */

TEST(checked_min_op) {
    EXPECT(checked_min(0, 0) == 0);
    EXPECT(checked_min(1, 2) == 1);
    EXPECT(checked_min(2, 1) == 1);
    EXPECT(checked_min(-1, 1) == -1);
    EXPECT(checked_min(1, -1) == -1);
    EXPECT(checked_min(CANON_ISIZE_MIN, CANON_ISIZE_MAX) == CANON_ISIZE_MIN);
    EXPECT(checked_min(CANON_ISIZE_MAX, CANON_ISIZE_MIN) == CANON_ISIZE_MIN);

    /* Equal inputs */
    EXPECT(checked_min(42, 42) == 42);
    EXPECT(checked_min(-7, -7) == -7);
}

TEST(checked_max_op) {
    EXPECT(checked_max(0, 0) == 0);
    EXPECT(checked_max(1, 2) == 2);
    EXPECT(checked_max(2, 1) == 2);
    EXPECT(checked_max(-1, 1) == 1);
    EXPECT(checked_max(1, -1) == 1);
    EXPECT(checked_max(CANON_ISIZE_MIN, CANON_ISIZE_MAX) == CANON_ISIZE_MAX);
    EXPECT(checked_max(CANON_ISIZE_MAX, CANON_ISIZE_MIN) == CANON_ISIZE_MAX);

    /* Equal inputs */
    EXPECT(checked_max(42, 42) == 42);
    EXPECT(checked_max(-7, -7) == -7);
}

TEST(checked_clamp_op) {
    /* Normal clamping */
    EXPECT(checked_clamp(5, 0, 10) == 5);
    EXPECT(checked_clamp(0, 0, 10) == 0);   /* at lo */
    EXPECT(checked_clamp(10, 0, 10) == 10); /* at hi */
    EXPECT(checked_clamp(-1, 0, 10) == 0);  /* below lo */
    EXPECT(checked_clamp(11, 0, 10) == 10); /* above hi */

    /* Degenerate: lo == hi */
    EXPECT(checked_clamp(5, 7, 7) == 7);
    EXPECT(checked_clamp(7, 7, 7) == 7);
    EXPECT(checked_clamp(9, 7, 7) == 7);

    /* Negative ranges */
    EXPECT(checked_clamp(-5, -10, -1) == -5);
    EXPECT(checked_clamp(-15, -10, -1) == -10);
    EXPECT(checked_clamp(0, -10, -1) == -1);

    /* Boundary values */
    EXPECT(checked_clamp(CANON_ISIZE_MIN, CANON_ISIZE_MIN, CANON_ISIZE_MAX) ==
           CANON_ISIZE_MIN);
    EXPECT(checked_clamp(CANON_ISIZE_MAX, CANON_ISIZE_MIN, CANON_ISIZE_MAX) ==
           CANON_ISIZE_MAX);
    EXPECT(checked_clamp(0, CANON_ISIZE_MIN, CANON_ISIZE_MAX) == 0);
}

/* =========================================================================
 * Cross-checks: add/sub inverse, mul identity
 * ====================================================================== */

TEST(add_sub_inverse) {
    usize result_add = 0;
    usize result_sub = 0;
    usize values[6];
    int idx;

    values[0] = 0;
    values[1] = 1;
    values[2] = 100;
    values[3] = CANON_USIZE_MAX / 2;
    values[4] = CANON_USIZE_MAX - 1;
    values[5] = CANON_USIZE_MAX;

    for (idx = 0; idx < 6; idx++) {
        usize val = values[idx];
        /* add then sub recovers original when no overflow */
        if (checked_add(val, 1, &result_add)) {
            EXPECT(checked_sub(result_add, 1, &result_sub) == true &&
                   result_sub == val);
        }
        if (checked_sub(val, 1, &result_sub)) {
            EXPECT(checked_add(result_sub, 1, &result_add) == true &&
                   result_add == val);
        }
    }
}

TEST(mul_identity) {
    usize result = 0;
    usize values[5];
    int idx;

    values[0] = 0;
    values[1] = 1;
    values[2] = 42;
    values[3] = CANON_USIZE_MAX / 2;
    values[4] = CANON_USIZE_MAX;

    for (idx = 0; idx < 5; idx++) {
        usize val = values[idx];
        EXPECT(checked_mul(val, 1, &result) == true && result == val);
        EXPECT(checked_mul(1, val, &result) == true && result == val);
        EXPECT(checked_mul(val, 0, &result) == true && result == 0);
        EXPECT(checked_mul(0, val, &result) == true && result == 0);
    }
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    usize  ua = 0, ub = 0;
    isize  ia = 0, ib = 0;
    usize  ur = 0;
    isize  ir = 0;
    u8     r8  = 0;
    u16    r16 = 0;
    u32    r32 = 0;
    u64    r64 = 0;

    if (size < 16) return 0;

    memcpy(&ua, data,     sizeof(usize));
    memcpy(&ub, data + 8, sizeof(usize));
    ia = (isize)ua;
    ib = (isize)ub;

    /* Unsigned add/sub/mul — result not checked for value, only consistency */
    (void)checked_add(ua, ub, &ur);
    (void)checked_sub(ua, ub, &ur);
    (void)checked_mul(ua, ub, &ur);

    /* add then sub inverse: if add succeeds, sub must also succeed and recover ua */
    if (checked_add(ua, ub, &ur)) {
        usize recovered = 0;
        if (!checked_sub(ur, ub, &recovered)) __builtin_trap();
        if (recovered != ua) __builtin_trap();
    }

    /* mul by zero always succeeds */
    if (!checked_mul(ua, 0, &ur) || ur != 0) __builtin_trap();
    if (!checked_mul(0, ua, &ur) || ur != 0) __builtin_trap();

    /* mul by one preserves value */
    if (!checked_mul(ua, 1, &ur) || ur != ua) __builtin_trap();

    /* Typed unsigned */
    {
        u8 a8 = (u8)ua, b8 = (u8)ub;
        (void)checked_add_u8(a8, b8, &r8);
        (void)checked_sub_u8(a8, b8, &r8);
        (void)checked_mul_u8(a8, b8, &r8);
    }
    {
        u16 a16 = (u16)ua, b16 = (u16)ub;
        (void)checked_add_u16(a16, b16, &r16);
        (void)checked_sub_u16(a16, b16, &r16);
        (void)checked_mul_u16(a16, b16, &r16);
    }
    {
        u32 a32 = (u32)ua, b32 = (u32)ub;
        (void)checked_add_u32(a32, b32, &r32);
        (void)checked_sub_u32(a32, b32, &r32);
        (void)checked_mul_u32(a32, b32, &r32);
    }
    {
        u64 a64 = (u64)ua, b64 = (u64)ub;
        (void)checked_add_u64(a64, b64, &r64);
        (void)checked_sub_u64(a64, b64, &r64);
        (void)checked_mul_u64(a64, b64, &r64);
    }

    /* Signed — add then sub inverse */
    if (checked_add_isize(ia, ib, &ir)) {
        isize recovered = 0;
        if (!checked_sub_isize(ir, ib, &recovered)) __builtin_trap();
        if (recovered != ia) __builtin_trap();
    }

    /* Signed mul: zero always succeeds */
    if (!checked_mul_isize(ia, 0, &ir) || ir != 0) __builtin_trap();
    if (!checked_mul_isize(0, ia, &ir) || ir != 0) __builtin_trap();

    /* Signed mul: one preserves value */
    if (!checked_mul_isize(ia, 1, &ir) || ir != ia) __builtin_trap();
    if (!checked_mul_isize(1, ia, &ir) || ir != ia) __builtin_trap();

    /* min/max/clamp sanity */
    {
        isize lo = checked_min(ia, ib);
        isize hi = checked_max(ia, ib);
        /* clamp(ia, lo, hi) == ia when ia is already in [lo, hi] */
        if (checked_clamp(ia, lo, hi) != ia) __builtin_trap();
        if (checked_clamp(ib, lo, hi) != ib) __builtin_trap();
        /* clamp result is always in [lo, hi] */
        isize clamped = checked_clamp(ia, lo, hi);
        if (clamped < lo || clamped > hi) __builtin_trap();
    }

    return 0;
}

#else

int main(void) {
    RUN(checked_add_op);
    RUN(checked_add_u8_op);
    RUN(checked_add_u16_op);
    RUN(checked_add_u32_op);
    RUN(checked_add_u64_op);

    RUN(checked_sub_op);
    RUN(checked_sub_typed_op);

    RUN(checked_mul_op);
    RUN(checked_mul_typed_op);

    RUN(checked_add_isize_op);
    RUN(checked_sub_isize_op);
    RUN(checked_mul_isize_op);

    RUN(checked_min_op);
    RUN(checked_max_op);
    RUN(checked_clamp_op);

    RUN(add_sub_inverse);
    RUN(mul_identity);

    printf("\nchecked_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
