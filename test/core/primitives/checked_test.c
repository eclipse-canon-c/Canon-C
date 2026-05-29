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
 *   checked_div  / checked_div_u8  / checked_div_u16  / checked_div_u32  / checked_div_u64
 *   checked_mod  / checked_mod_u8  / checked_mod_u16  / checked_mod_u32  / checked_mod_u64
 *   checked_add_isize / checked_sub_isize / checked_mul_isize
 *   checked_div_isize / checked_mod_isize
 *   checked_min  / checked_max     / checked_clamp
 *
 * MC/DC coverage note
 * ───────────────────────────────────────────────────────────────────────────
 *   The CI coverage job compiles this file with -DCANON_CHECKED_FORCE_FALLBACK,
 *   which forces checked.h to use its portable fallback branches instead of
 *   __builtin_*_overflow. The fallback branches in checked_mul_isize contain
 *   short-circuit conditions that need near-miss vs overflow pairs to reach
 *   full MC/DC — see checked_mul_isize_quadrants_op below.
 *
 *   Division and modulo have no compiler builtin equivalent, so their code
 *   paths are the same in all builds — CANON_CHECKED_FORCE_FALLBACK has no
 *   effect on them. The div/mod tests below exercise the full MC/DC pairs
 *   (zero/non-zero denominator; for signed: ISIZE_MIN/-1 vs near-misses).
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
    EXPECT(checked_mul_u8(16, 16, &r8) == false);   /* 256 overflows u8 */
    EXPECT(checked_mul_u8(2, 128, &r8) == false);
    EXPECT(checked_mul_u8(255, 2,  &r8) == false);

    /* u16 */
    EXPECT(checked_mul_u16(256, 256, &r16) == false);  /* 65536 overflows u16 */
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
 * checked_add_isize — MC/DC short-circuit coverage for fallback path
 *
 * The fallback in checked_add_isize has two short-circuit conditions:
 *   if (a > 0 && b > 0 && b > (CANON_ISIZE_MAX - a)) return false;
 *   if (a < 0 && b < 0 && b < (CANON_ISIZE_MIN - a)) return false;
 *
 * MC/DC requires each condition to independently affect the outcome.
 * These tests target the "near-miss" case (all three true → overflow)
 * versus "just fits" (third condition false → no overflow).
 * ====================================================================== */

TEST(checked_add_isize_mcdc) {
    isize result = 0;

    /* (+, +) short-circuit — first condition false */
    EXPECT(checked_add_isize(0, CANON_ISIZE_MAX, &result) == true &&
           result == CANON_ISIZE_MAX);

    /* (+, +) short-circuit — second condition false */
    EXPECT(checked_add_isize(CANON_ISIZE_MAX, 0, &result) == true &&
           result == CANON_ISIZE_MAX);

    /* (+, +) — third condition false (exact fit, b == MAX - a) */
    EXPECT(checked_add_isize(CANON_ISIZE_MAX / 2, CANON_ISIZE_MAX - (CANON_ISIZE_MAX / 2), &result) == true &&
           result == CANON_ISIZE_MAX);

    /* (+, +) — all three true (overflow by 1) */
    EXPECT(checked_add_isize(CANON_ISIZE_MAX / 2 + 1, CANON_ISIZE_MAX - (CANON_ISIZE_MAX / 2), &result) == false);

    /* (-, -) short-circuit — first condition false (already covered, but pair for symmetry) */
    EXPECT(checked_add_isize(0, CANON_ISIZE_MIN, &result) == true &&
           result == CANON_ISIZE_MIN);

    /* (-, -) short-circuit — second condition false */
    EXPECT(checked_add_isize(CANON_ISIZE_MIN, 0, &result) == true &&
           result == CANON_ISIZE_MIN);

    /* (-, -) — third condition false (exact fit, b == MIN - a) */
    EXPECT(checked_add_isize(CANON_ISIZE_MIN / 2, CANON_ISIZE_MIN - (CANON_ISIZE_MIN / 2), &result) == true &&
           result == CANON_ISIZE_MIN);

    /* (-, -) — all three true (underflow by 1) */
    EXPECT(checked_add_isize(CANON_ISIZE_MIN / 2 - 1, CANON_ISIZE_MIN - (CANON_ISIZE_MIN / 2), &result) == false);
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
 * checked_sub_isize — MC/DC short-circuit coverage for fallback path
 * ====================================================================== */

TEST(checked_sub_isize_mcdc) {
    isize result = 0;

    /* b > 0 branch — first condition false (b == 0) */
    EXPECT(checked_sub_isize(CANON_ISIZE_MIN, 0, &result) == true &&
           result == CANON_ISIZE_MIN);

    /* b > 0 branch — second condition false (exact fit, a == MIN + b) */
    EXPECT(checked_sub_isize(CANON_ISIZE_MIN + 5, 5, &result) == true &&
           result == CANON_ISIZE_MIN);

    /* b > 0 branch — both true (underflow by 1) */
    EXPECT(checked_sub_isize(CANON_ISIZE_MIN + 4, 5, &result) == false);

    /* b < 0 branch — second condition false (exact fit, a == MAX + b) */
    EXPECT(checked_sub_isize(CANON_ISIZE_MAX - 5, -5, &result) == true &&
           result == CANON_ISIZE_MAX);

    /* b < 0 branch — both true (overflow by 1) */
    EXPECT(checked_sub_isize(CANON_ISIZE_MAX - 4, -5, &result) == false);
}

/* =========================================================================
 * checked_mul_isize
 * ====================================================================== */

TEST(checked_mul_isize_op) {
    isize result = 0;

    /* Zero */
    EXPECT(checked_mul_isize(0, 0, &result)               == true && result == 0);
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
    EXPECT(checked_mul_isize(CANON_ISIZE_MAX, 2, &result)            == false);
    EXPECT(checked_mul_isize(2, CANON_ISIZE_MAX, &result)            == false);
    EXPECT(checked_mul_isize(CANON_ISIZE_MAX, CANON_ISIZE_MAX, &result) == false);
    EXPECT(checked_mul_isize(CANON_ISIZE_MIN, CANON_ISIZE_MIN, &result) == false);

    /* Normal values */
    EXPECT(checked_mul_isize(100, 200, &result)   == true && result == 20000);
    EXPECT(checked_mul_isize(-100, 200, &result)  == true && result == -20000);
    EXPECT(checked_mul_isize(100, -200, &result)  == true && result == -20000);
    EXPECT(checked_mul_isize(-100, -200, &result) == true && result == 20000);
}

/* =========================================================================
 * checked_mul_isize — MC/DC four-quadrant coverage for fallback path
 * ====================================================================== */

TEST(checked_mul_isize_quadrants) {
    isize result = 0;
    isize half_max = CANON_ISIZE_MAX / 2;
    isize half_min = CANON_ISIZE_MIN / 2;

    /* (+, +) — just fits: a == MAX/b */
    EXPECT(checked_mul_isize(half_max, 2, &result) == true &&
           result == half_max * 2);

    /* (+, +) — one past: a == MAX/b + 1 */
    EXPECT(checked_mul_isize(half_max + 2, 2, &result) == false);

    /* (-, -) — just fits */
    EXPECT(checked_mul_isize(half_min + 1, -2, &result) == true);

    /* (-, -) — one past (|a| > MAX/|b|) */
    EXPECT(checked_mul_isize(half_min, -3, &result) == false);

    /* (+, -) — just fits: b == MIN/a */
    EXPECT(checked_mul_isize(2, half_min, &result) == true &&
           result == 2 * half_min);

    /* (+, -) — one past: b < MIN/a */
    EXPECT(checked_mul_isize(2, half_min - 1, &result) == false);

    /* (-, +) — just fits: a == MIN/b */
    EXPECT(checked_mul_isize(half_min, 2, &result) == true &&
           result == half_min * 2);

    /* (-, +) — one past: a < MIN/b */
    EXPECT(checked_mul_isize(half_min - 1, 2, &result) == false);

    /* Additional sanity: small magnitudes never overflow regardless of signs */
    EXPECT(checked_mul_isize(3, -4, &result)  == true && result == -12);
    EXPECT(checked_mul_isize(-3, 4, &result)  == true && result == -12);
    EXPECT(checked_mul_isize(-3, -4, &result) == true && result == 12);
    EXPECT(checked_mul_isize(3, 4, &result)   == true && result == 12);
}

/* =========================================================================
 * checked_mul_isize — MC/DC gap closer for the (a == 1 && b == ISIZE_MIN)
 * special-case guard
 * ====================================================================== */

TEST(checked_mul_isize_one_min_guard) {
    isize result = 0;

    /* a == 1, b == ISIZE_MIN: the special case itself (exact path) */
    EXPECT(checked_mul_isize(1, CANON_ISIZE_MIN, &result) == true &&
           result == CANON_ISIZE_MIN);

    /* a == 1, b != ISIZE_MIN: first subcondition true, second false. */
    EXPECT(checked_mul_isize(1, 0, &result)                == true && result == 0);
    EXPECT(checked_mul_isize(1, 1, &result)                == true && result == 1);
    EXPECT(checked_mul_isize(1, -1, &result)               == true && result == -1);
    EXPECT(checked_mul_isize(1, CANON_ISIZE_MAX, &result)  == true && result == CANON_ISIZE_MAX);
    EXPECT(checked_mul_isize(1, CANON_ISIZE_MIN + 1, &result) == true &&
           result == CANON_ISIZE_MIN + 1);

    /* a != 1, b == ISIZE_MIN: first subcondition false with the same b. */
    EXPECT(checked_mul_isize(0, CANON_ISIZE_MIN, &result)  == true && result == 0);
    EXPECT(checked_mul_isize(-1, CANON_ISIZE_MIN, &result) == false);
}

/* =========================================================================
 * checked_mul_u64 — MC/DC gap closer for the (a == 0 || b == 0) shortcut
 * ====================================================================== */

TEST(checked_mul_u64_mcdc) {
    u64 result = 0;

    EXPECT(checked_mul_u64(0, 0, &result) == true && result == 0);
    EXPECT(checked_mul_u64(0, 42, &result) == true && result == 0);
    EXPECT(checked_mul_u64(0, 0xFFFFFFFFFFFFFFFFULL, &result) == true && result == 0);
    EXPECT(checked_mul_u64(42, 0, &result) == true && result == 0);
    EXPECT(checked_mul_u64(0xFFFFFFFFFFFFFFFFULL, 0, &result) == true && result == 0);
    EXPECT(checked_mul_u64(2, 3, &result) == true && result == 6);
    EXPECT(checked_mul_u64(0xFFFFFFFFFFFFFFFFULL / 2, 2, &result) == true);
    EXPECT(checked_mul_u64(0xFFFFFFFFFFFFFFFFULL / 2 + 1, 3, &result) == false);
}

/* =========================================================================
 * checked_mul (usize) — MC/DC short-circuit for fallback path
 * ====================================================================== */

TEST(checked_mul_mcdc) {
    usize result = 0;

    EXPECT(checked_mul(0, 42, &result) == true && result == 0);
    EXPECT(checked_mul(42, 0, &result) == true && result == 0);
    EXPECT(checked_mul(2, 3, &result) == true && result == 6);
    EXPECT(checked_mul(CANON_USIZE_MAX / 2, 2, &result) == true);
    EXPECT(checked_mul(CANON_USIZE_MAX / 2 + 1, 3, &result) == false);
}

/* =========================================================================
 * checked_div (usize)
 *
 * Single failure mode: division by zero. Tests cover MC/DC pairs:
 *   - b == 0 (failure) vs b != 0 (success)
 *   - a == 0 with non-zero b (the "zero numerator, non-zero denominator"
 *     path that's distinct from the zero-denominator short-circuit)
 * ====================================================================== */

TEST(checked_div_op) {
    usize result = 0;

    /* Normal cases */
    EXPECT(checked_div(10, 2, &result) == true && result == 5);
    EXPECT(checked_div(100, 10, &result) == true && result == 10);
    EXPECT(checked_div(7, 3, &result) == true && result == 2);  /* truncation */
    EXPECT(checked_div(0, 1, &result) == true && result == 0);
    EXPECT(checked_div(0, CANON_USIZE_MAX, &result) == true && result == 0);

    /* Boundary */
    EXPECT(checked_div(CANON_USIZE_MAX, 1, &result) == true &&
           result == CANON_USIZE_MAX);
    EXPECT(checked_div(CANON_USIZE_MAX, CANON_USIZE_MAX, &result) == true &&
           result == 1);
    EXPECT(checked_div(1, CANON_USIZE_MAX, &result) == true && result == 0);

    /* Division by zero */
    EXPECT(checked_div(0, 0, &result) == false);
    EXPECT(checked_div(1, 0, &result) == false);
    EXPECT(checked_div(CANON_USIZE_MAX, 0, &result) == false);
}

TEST(checked_div_typed_op) {
    u8  r8  = 0;
    u16 r16 = 0;
    u32 r32 = 0;
    u64 r64 = 0;

    /* u8 */
    EXPECT(checked_div_u8(255, 5, &r8) == true && r8 == 51);
    EXPECT(checked_div_u8(0, 1, &r8)   == true && r8 == 0);
    EXPECT(checked_div_u8(0, 255, &r8) == true && r8 == 0);
    EXPECT(checked_div_u8(0, 0, &r8)   == false);
    EXPECT(checked_div_u8(255, 0, &r8) == false);

    /* u16 */
    EXPECT(checked_div_u16(65535, 256, &r16) == true && r16 == 255);
    EXPECT(checked_div_u16(0, 1, &r16)       == true && r16 == 0);
    EXPECT(checked_div_u16(0, 0, &r16)       == false);
    EXPECT(checked_div_u16(1, 0, &r16)       == false);

    /* u32 */
    EXPECT(checked_div_u32(0xFFFFFFFFU, 2, &r32) == true && r32 == 0x7FFFFFFFU);
    EXPECT(checked_div_u32(0, 1, &r32)            == true && r32 == 0);
    EXPECT(checked_div_u32(0, 0, &r32)            == false);
    EXPECT(checked_div_u32(0xFFFFFFFFU, 0, &r32)  == false);

    /* u64 */
    EXPECT(checked_div_u64(0xFFFFFFFFFFFFFFFFULL, 2, &r64) == true &&
           r64 == 0x7FFFFFFFFFFFFFFFULL);
    EXPECT(checked_div_u64(0, 1, &r64) == true && r64 == 0);
    EXPECT(checked_div_u64(0, 0, &r64) == false);
    EXPECT(checked_div_u64(1, 0, &r64) == false);
}

/* =========================================================================
 * checked_mod (usize)
 *
 * Same failure mode as checked_div. Tests verify:
 *   - C99 modulo semantics: a % b is in [0, b-1] for unsigned
 *   - Zero-numerator success: 0 % b == 0
 *   - The (a / b) * b + (a % b) == a identity for spot-checked values
 * ====================================================================== */

TEST(checked_mod_op) {
    usize result = 0;

    /* Normal cases */
    EXPECT(checked_mod(10, 3, &result) == true && result == 1);
    EXPECT(checked_mod(100, 10, &result) == true && result == 0);
    EXPECT(checked_mod(7, 3, &result) == true && result == 1);
    EXPECT(checked_mod(0, 1, &result) == true && result == 0);
    EXPECT(checked_mod(0, CANON_USIZE_MAX, &result) == true && result == 0);

    /* a < b: result == a */
    EXPECT(checked_mod(3, 10, &result) == true && result == 3);
    EXPECT(checked_mod(1, CANON_USIZE_MAX, &result) == true && result == 1);

    /* a == b: result == 0 */
    EXPECT(checked_mod(42, 42, &result) == true && result == 0);
    EXPECT(checked_mod(CANON_USIZE_MAX, CANON_USIZE_MAX, &result) == true &&
           result == 0);

    /* Boundary: result is always strictly less than b */
    EXPECT(checked_mod(CANON_USIZE_MAX, 2, &result) == true && result < 2);
    EXPECT(checked_mod(CANON_USIZE_MAX, 256, &result) == true && result < 256);

    /* Division by zero */
    EXPECT(checked_mod(0, 0, &result) == false);
    EXPECT(checked_mod(1, 0, &result) == false);
    EXPECT(checked_mod(CANON_USIZE_MAX, 0, &result) == false);
}

TEST(checked_mod_typed_op) {
    u8  r8  = 0;
    u16 r16 = 0;
    u32 r32 = 0;
    u64 r64 = 0;

    /* u8 */
    EXPECT(checked_mod_u8(255, 7, &r8) == true && r8 == 3);   /* 255 = 36*7 + 3 */
    EXPECT(checked_mod_u8(0, 1, &r8)   == true && r8 == 0);
    EXPECT(checked_mod_u8(0, 0, &r8)   == false);
    EXPECT(checked_mod_u8(255, 0, &r8) == false);

    /* u16 */
    EXPECT(checked_mod_u16(65535, 256, &r16) == true && r16 == 255);
    EXPECT(checked_mod_u16(0, 1, &r16)       == true && r16 == 0);
    EXPECT(checked_mod_u16(0, 0, &r16)       == false);
    EXPECT(checked_mod_u16(1, 0, &r16)       == false);

    /* u32 */
    EXPECT(checked_mod_u32(0xFFFFFFFFU, 7, &r32) == true && r32 == 3);
    EXPECT(checked_mod_u32(0, 1, &r32)            == true && r32 == 0);
    EXPECT(checked_mod_u32(0, 0, &r32)            == false);
    EXPECT(checked_mod_u32(0xFFFFFFFFU, 0, &r32)  == false);

    /* u64 */
    EXPECT(checked_mod_u64(0xFFFFFFFFFFFFFFFFULL, 7, &r64) == true && r64 == 1);
    EXPECT(checked_mod_u64(0, 1, &r64) == true && r64 == 0);
    EXPECT(checked_mod_u64(0, 0, &r64) == false);
    EXPECT(checked_mod_u64(1, 0, &r64) == false);
}

/* =========================================================================
 * checked_div_isize
 *
 * Two failure modes — both must be tested separately for MC/DC:
 *   1. b == 0 (division by zero)
 *   2. a == ISIZE_MIN && b == -1 (overflow, mathematical -ISIZE_MIN
 *      is unrepresentable)
 *
 * MC/DC pairs for the && in the second guard:
 *   - a == ISIZE_MIN, b == -1   → both true   (failure)
 *   - a == ISIZE_MIN, b != -1   → first true, second false (success)
 *   - a != ISIZE_MIN, b == -1   → first false, second true (success)
 *   - a != ISIZE_MIN, b != -1   → both false  (success, unrelated path)
 *
 * Also tests C99 truncation-toward-zero semantics for negative operands
 * (-7 / 2 == -3 in C99, not -4 as Euclidean modulo would give).
 * ====================================================================== */

TEST(checked_div_isize_op) {
    isize result = 0;

    /* Normal positive */
    EXPECT(checked_div_isize(10, 2, &result) == true && result == 5);
    EXPECT(checked_div_isize(7, 3, &result)  == true && result == 2);

    /* Normal negative — C99 truncation toward zero */
    EXPECT(checked_div_isize(-10, 2, &result)  == true && result == -5);
    EXPECT(checked_div_isize(10, -2, &result)  == true && result == -5);
    EXPECT(checked_div_isize(-10, -2, &result) == true && result == 5);
    EXPECT(checked_div_isize(-7, 2, &result)   == true && result == -3);  /* not -4 */
    EXPECT(checked_div_isize(7, -2, &result)   == true && result == -3);
    EXPECT(checked_div_isize(-7, -2, &result)  == true && result == 3);

    /* Zero numerator paths (non-zero denominator, positive AND negative) */
    EXPECT(checked_div_isize(0, 1, &result)  == true && result == 0);
    EXPECT(checked_div_isize(0, -1, &result) == true && result == 0);
    EXPECT(checked_div_isize(0, CANON_ISIZE_MAX, &result) == true && result == 0);
    EXPECT(checked_div_isize(0, CANON_ISIZE_MIN, &result) == true && result == 0);
    EXPECT(checked_div_isize(0, 42, &result)  == true && result == 0);
    EXPECT(checked_div_isize(0, -42, &result) == true && result == 0);

    /* Identity */
    EXPECT(checked_div_isize(42, 1, &result)   == true && result == 42);
    EXPECT(checked_div_isize(-42, 1, &result)  == true && result == -42);
    EXPECT(checked_div_isize(42, -1, &result)  == true && result == -42);
    EXPECT(checked_div_isize(-42, -1, &result) == true && result == 42);

    /* Boundary — ISIZE_MAX */
    EXPECT(checked_div_isize(CANON_ISIZE_MAX, 1, &result) == true &&
           result == CANON_ISIZE_MAX);
    EXPECT(checked_div_isize(CANON_ISIZE_MAX, -1, &result) == true &&
           result == -CANON_ISIZE_MAX);
    EXPECT(checked_div_isize(CANON_ISIZE_MAX, CANON_ISIZE_MAX, &result) == true &&
           result == 1);

    /* Boundary — ISIZE_MIN with safe denominators */
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, 1, &result) == true &&
           result == CANON_ISIZE_MIN);
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, 2, &result) == true);
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, CANON_ISIZE_MIN, &result) == true &&
           result == 1);

    /* Failure — division by zero (a varies) */
    EXPECT(checked_div_isize(0, 0, &result)               == false);
    EXPECT(checked_div_isize(1, 0, &result)               == false);
    EXPECT(checked_div_isize(-1, 0, &result)              == false);
    EXPECT(checked_div_isize(CANON_ISIZE_MAX, 0, &result) == false);
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, 0, &result) == false);

    /* Failure — ISIZE_MIN / -1 overflow (the only signed-div overflow case) */
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, -1, &result) == false);
}

TEST(checked_div_isize_mcdc) {
    isize result = 0;

    /*
     * MC/DC for the (a == CANON_ISIZE_MIN && b == -1) guard.
     * Each subcondition must independently determine the outcome.
     */

    /* Both true → failure */
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, -1, &result) == false);

    /* First true, second false → success (ISIZE_MIN as numerator, b != -1) */
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, 1, &result) == true &&
           result == CANON_ISIZE_MIN);
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, 2, &result) == true);
    EXPECT(checked_div_isize(CANON_ISIZE_MIN, CANON_ISIZE_MAX, &result) == true);

    /* First false, second true → success (b == -1, a != ISIZE_MIN) */
    EXPECT(checked_div_isize(0, -1, &result)               == true && result == 0);
    EXPECT(checked_div_isize(1, -1, &result)               == true && result == -1);
    EXPECT(checked_div_isize(-1, -1, &result)              == true && result == 1);
    EXPECT(checked_div_isize(CANON_ISIZE_MAX, -1, &result) == true &&
           result == -CANON_ISIZE_MAX);
    EXPECT(checked_div_isize(CANON_ISIZE_MIN + 1, -1, &result) == true);
}

/* =========================================================================
 * checked_mod_isize
 *
 * Same two failure modes as checked_div_isize. Tests verify:
 *   - C99 truncation-toward-zero semantics: -7 % 2 == -1 (not 1)
 *   - The standard's identity (a/b)*b + (a%b) == a holds for representable
 *     pairs (verified in cross-check test below)
 *   - ISIZE_MIN % -1 fails even though some compilers return 0 — Canon-C
 *     rejects on standard's terms, not de-facto behavior
 *   - MC/DC pairs for the (a == ISIZE_MIN && b == -1) guard
 * ====================================================================== */

TEST(checked_mod_isize_op) {
    isize result = 0;

    /* Normal positive */
    EXPECT(checked_mod_isize(10, 3, &result) == true && result == 1);
    EXPECT(checked_mod_isize(7, 3, &result)  == true && result == 1);
    EXPECT(checked_mod_isize(100, 10, &result) == true && result == 0);

    /* C99 truncation toward zero — sign of result matches sign of dividend */
    EXPECT(checked_mod_isize(-7, 2, &result)  == true && result == -1);  /* not 1 */
    EXPECT(checked_mod_isize(7, -2, &result)  == true && result == 1);
    EXPECT(checked_mod_isize(-7, -2, &result) == true && result == -1);
    EXPECT(checked_mod_isize(-10, 3, &result) == true && result == -1);
    EXPECT(checked_mod_isize(-1, 2, &result)  == true && result == -1);

    /* Zero numerator paths (non-zero denominator, positive AND negative) */
    EXPECT(checked_mod_isize(0, 1, &result)  == true && result == 0);
    EXPECT(checked_mod_isize(0, -1, &result) == true && result == 0);
    EXPECT(checked_mod_isize(0, CANON_ISIZE_MAX, &result) == true && result == 0);
    EXPECT(checked_mod_isize(0, CANON_ISIZE_MIN, &result) == true && result == 0);
    EXPECT(checked_mod_isize(0, 42, &result)  == true && result == 0);
    EXPECT(checked_mod_isize(0, -42, &result) == true && result == 0);

    /* a == b → 0 */
    EXPECT(checked_mod_isize(42, 42, &result) == true && result == 0);
    EXPECT(checked_mod_isize(-42, -42, &result) == true && result == 0);

    /* Identity-by-divisor: a % 1 == 0, a % -1 == 0 */
    EXPECT(checked_mod_isize(42, 1, &result)   == true && result == 0);
    EXPECT(checked_mod_isize(-42, 1, &result)  == true && result == 0);
    EXPECT(checked_mod_isize(42, -1, &result)  == true && result == 0);
    EXPECT(checked_mod_isize(-42, -1, &result) == true && result == 0);

    /* Boundary */
    EXPECT(checked_mod_isize(CANON_ISIZE_MAX, 2, &result) == true);
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, 2, &result) == true);
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, 1, &result) == true && result == 0);

    /* Failure — division by zero */
    EXPECT(checked_mod_isize(0, 0, &result)               == false);
    EXPECT(checked_mod_isize(1, 0, &result)               == false);
    EXPECT(checked_mod_isize(-1, 0, &result)              == false);
    EXPECT(checked_mod_isize(CANON_ISIZE_MAX, 0, &result) == false);
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, 0, &result) == false);

    /* Failure — ISIZE_MIN % -1.
     * Canon-C rejects this on standard's terms (the intermediate
     * ISIZE_MIN / -1 is UB), even though the mathematical answer is 0
     * and some compilers (notably MSVC) return 0 as a QoI choice. */
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, -1, &result) == false);
}

TEST(checked_mod_isize_mcdc) {
    isize result = 0;

    /* Both true → failure */
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, -1, &result) == false);

    /* First true, second false → success (ISIZE_MIN, b != -1) */
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, 1, &result) == true && result == 0);
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, 2, &result) == true);
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN, CANON_ISIZE_MAX, &result) == true);

    /* First false, second true → success (b == -1, a != ISIZE_MIN) */
    EXPECT(checked_mod_isize(0, -1, &result)               == true && result == 0);
    EXPECT(checked_mod_isize(1, -1, &result)               == true && result == 0);
    EXPECT(checked_mod_isize(-1, -1, &result)              == true && result == 0);
    EXPECT(checked_mod_isize(CANON_ISIZE_MAX, -1, &result) == true && result == 0);
    EXPECT(checked_mod_isize(CANON_ISIZE_MIN + 1, -1, &result) == true && result == 0);
}

/* =========================================================================
 * Cross-check: C99 division-modulo identity (a/b)*b + (a%b) == a
 *
 * Verifies for representable inputs that the C99 §6.5.5¶6 guarantee
 * holds. Skips inputs that would trigger overflow during the
 * reconstruction multiplication or addition, since those would not be
 * meaningful failures of the identity.
 * ====================================================================== */

TEST(div_mod_isize_identity) {
    isize values[12];
    int i, j;

    values[0]  = 0;
    values[1]  = 1;
    values[2]  = -1;
    values[3]  = 7;
    values[4]  = -7;
    values[5]  = 100;
    values[6]  = -100;
    values[7]  = CANON_ISIZE_MAX;
    values[8]  = CANON_ISIZE_MIN;
    values[9]  = CANON_ISIZE_MAX / 2;
    values[10] = CANON_ISIZE_MIN / 2;
    values[11] = 42;

    for (i = 0; i < 12; i++) {
        for (j = 0; j < 12; j++) {
            isize a = values[i];
            isize b = values[j];
            isize q = 0, r = 0;

            if (!checked_div_isize(a, b, &q)) continue;
            if (!checked_mod_isize(a, b, &r)) continue;

            /* Reconstruct a from q and r using checked ops to avoid
             * triggering UB on adversarial values. Skip if any step
             * would overflow — those inputs simply can't be checked
             * by this identity. */
            isize qb = 0, sum = 0;
            if (!checked_mul_isize(q, b, &qb)) continue;
            if (!checked_add_isize(qb, r, &sum)) continue;

            EXPECT(sum == a);
        }
    }
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
    EXPECT(checked_max(42, 42) == 42);
    EXPECT(checked_max(-7, -7) == -7);
}

TEST(checked_clamp_op) {
    /* Normal clamping */
    EXPECT(checked_clamp(5, 0, 10)  == 5);
    EXPECT(checked_clamp(0, 0, 10)  == 0);
    EXPECT(checked_clamp(10, 0, 10) == 10);
    EXPECT(checked_clamp(-1, 0, 10) == 0);
    EXPECT(checked_clamp(11, 0, 10) == 10);

    /* Degenerate: lo == hi */
    EXPECT(checked_clamp(5, 7, 7) == 7);
    EXPECT(checked_clamp(7, 7, 7) == 7);
    EXPECT(checked_clamp(9, 7, 7) == 7);

    /* Negative ranges */
    EXPECT(checked_clamp(-5,  -10, -1) == -5);
    EXPECT(checked_clamp(-15, -10, -1) == -10);
    EXPECT(checked_clamp(0,   -10, -1) == -1);

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
    usize ua = 0, ub = 0;
    isize ia = 0, ib = 0;
    usize ur = 0;
    isize ir = 0;
    u8    r8  = 0;
    u16   r16 = 0;
    u32   r32 = 0;
    u64   r64 = 0;

    if (size < 16) return 0;

    memcpy(&ua, data,     sizeof(usize));
    memcpy(&ub, data + 8, sizeof(usize));
    ia = (isize)ua;
    ib = (isize)ub;

    /* Unsigned add/sub/mul */
    (void)checked_add(ua, ub, &ur);
    (void)checked_sub(ua, ub, &ur);
    (void)checked_mul(ua, ub, &ur);

    /* add then sub inverse */
    if (checked_add(ua, ub, &ur)) {
        usize recovered = 0;
        if (!checked_sub(ur, ub, &recovered)) __builtin_trap();
        if (recovered != ua)                  __builtin_trap();
    }

    /* mul by zero always succeeds */
    if (!checked_mul(ua, 0, &ur) || ur != 0) __builtin_trap();
    if (!checked_mul(0, ua, &ur) || ur != 0) __builtin_trap();

    /* mul by one preserves value */
    if (!checked_mul(ua, 1, &ur) || ur != ua) __builtin_trap();

    /* Unsigned div/mod */
    (void)checked_div(ua, ub, &ur);
    (void)checked_mod(ua, ub, &ur);

    /* Division by zero must fail */
    if (checked_div(ua, 0, &ur)) __builtin_trap();
    if (checked_mod(ua, 0, &ur)) __builtin_trap();

    /* Division by one is identity; modulo by one is zero */
    if (!checked_div(ua, 1, &ur) || ur != ua) __builtin_trap();
    if (!checked_mod(ua, 1, &ur) || ur != 0)  __builtin_trap();

    /* Unsigned div/mod identity: (a/b)*b + (a%b) == a, gated on success.
     * The reconstruction uses checked_mul + checked_add and is skipped
     * if either step would overflow — adversarial inputs can construct
     * (a, b) pairs where the identity holds mathematically but the
     * reconstruction overflows usize. */
    {
        usize q = 0, r = 0;
        if (checked_div(ua, ub, &q) && checked_mod(ua, ub, &r)) {
            usize qb = 0, sum = 0;
            if (checked_mul(q, ub, &qb) && checked_add(qb, r, &sum)) {
                if (sum != ua) __builtin_trap();
            }
        }
    }

    /* Typed unsigned */
    {
        u8 a8 = (u8)ua, b8 = (u8)ub;
        (void)checked_add_u8(a8, b8, &r8);
        (void)checked_sub_u8(a8, b8, &r8);
        (void)checked_mul_u8(a8, b8, &r8);
        (void)checked_div_u8(a8, b8, &r8);
        (void)checked_mod_u8(a8, b8, &r8);
        if (b8 == 0) {
            if (checked_div_u8(a8, b8, &r8)) __builtin_trap();
            if (checked_mod_u8(a8, b8, &r8)) __builtin_trap();
        }
    }
    {
        u16 a16 = (u16)ua, b16 = (u16)ub;
        (void)checked_add_u16(a16, b16, &r16);
        (void)checked_sub_u16(a16, b16, &r16);
        (void)checked_mul_u16(a16, b16, &r16);
        (void)checked_div_u16(a16, b16, &r16);
        (void)checked_mod_u16(a16, b16, &r16);
    }
    {
        u32 a32 = (u32)ua, b32 = (u32)ub;
        (void)checked_add_u32(a32, b32, &r32);
        (void)checked_sub_u32(a32, b32, &r32);
        (void)checked_mul_u32(a32, b32, &r32);
        (void)checked_div_u32(a32, b32, &r32);
        (void)checked_mod_u32(a32, b32, &r32);
    }
    {
        u64 a64 = (u64)ua, b64 = (u64)ub;
        (void)checked_add_u64(a64, b64, &r64);
        (void)checked_sub_u64(a64, b64, &r64);
        (void)checked_mul_u64(a64, b64, &r64);
        (void)checked_div_u64(a64, b64, &r64);
        (void)checked_mod_u64(a64, b64, &r64);
    }

    /* Signed add then sub inverse */
    if (checked_add_isize(ia, ib, &ir)) {
        isize recovered = 0;
        if (!checked_sub_isize(ir, ib, &recovered)) __builtin_trap();
        if (recovered != ia)                        __builtin_trap();
    }

    /* Signed mul: zero always succeeds */
    if (!checked_mul_isize(ia, 0, &ir) || ir != 0) __builtin_trap();
    if (!checked_mul_isize(0, ia, &ir) || ir != 0) __builtin_trap();

    /* Signed mul: one preserves value */
    if (!checked_mul_isize(ia, 1, &ir) || ir != ia) __builtin_trap();
    if (!checked_mul_isize(1, ia, &ir) || ir != ia) __builtin_trap();

    /* Signed div/mod */
    (void)checked_div_isize(ia, ib, &ir);
    (void)checked_mod_isize(ia, ib, &ir);

    /* Division by zero must fail */
    if (checked_div_isize(ia, 0, &ir)) __builtin_trap();
    if (checked_mod_isize(ia, 0, &ir)) __builtin_trap();

    /* ISIZE_MIN / -1 and ISIZE_MIN % -1 must fail */
    if (checked_div_isize(CANON_ISIZE_MIN, -1, &ir)) __builtin_trap();
    if (checked_mod_isize(CANON_ISIZE_MIN, -1, &ir)) __builtin_trap();

    /* Signed div/mod identity: (a/b)*b + (a%b) == a.
     * Use checked_mul_isize + checked_add_isize for the reconstruction
     * so adversarial inputs can't trigger UB during the invariant
     * check itself. Skip if either reconstruction step overflows —
     * that's not a violation of the C99 identity, just an input
     * pattern where the round-trip exceeds isize range. */
    {
        isize q = 0, r = 0;
        if (checked_div_isize(ia, ib, &q) && checked_mod_isize(ia, ib, &r)) {
            isize qb = 0, sum = 0;
            if (checked_mul_isize(q, ib, &qb) && checked_add_isize(qb, r, &sum)) {
                if (sum != ia) __builtin_trap();
            }
        }
    }

    /* min/max/clamp sanity */
    {
        isize lo = checked_min(ia, ib);
        isize hi = checked_max(ia, ib);
        if (checked_clamp(ia, lo, hi) != ia) __builtin_trap();
        if (checked_clamp(ib, lo, hi) != ib) __builtin_trap();
        {
            isize clamped = checked_clamp(ia, lo, hi);
            if (clamped < lo || clamped > hi) __builtin_trap();
        }
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
    RUN(checked_mul_mcdc);
    RUN(checked_mul_u64_mcdc);

    RUN(checked_div_op);
    RUN(checked_div_typed_op);
    RUN(checked_mod_op);
    RUN(checked_mod_typed_op);

    RUN(checked_add_isize_op);
    RUN(checked_add_isize_mcdc);
    RUN(checked_sub_isize_op);
    RUN(checked_sub_isize_mcdc);
    RUN(checked_mul_isize_op);
    RUN(checked_mul_isize_quadrants);
    RUN(checked_mul_isize_one_min_guard);

    RUN(checked_div_isize_op);
    RUN(checked_div_isize_mcdc);
    RUN(checked_mod_isize_op);
    RUN(checked_mod_isize_mcdc);

    RUN(div_mod_isize_identity);

    RUN(checked_min_op);
    RUN(checked_max_op);
    RUN(checked_clamp_op);

    RUN(add_sub_inverse);
    RUN(mul_identity);

    printf("\nchecked_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
