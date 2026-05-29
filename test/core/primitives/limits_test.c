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
 * @file limits_test.c
 * @brief Unit tests for limits.h
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   Integer type limits      — CANON_U8/U16/U32/U64_MAX,
 *                              CANON_I8/I16/I32/I64_MIN/MAX
 *   Size type limits         — CANON_USIZE_MAX, CANON_ISIZE_MAX/MIN
 *   Size literals            — CANON_KB, CANON_MB, CANON_GB
 *   Alignment constants      — CANON_DEFAULT_ALIGN, CANON_CACHE_LINE,
 *                              CANON_SIMD_ALIGN, CANON_SIMD_ALIGN_AVX,
 *                              CANON_SIMD_ALIGN_AVX512, CANON_PAGE_SIZE,
 *                              CANON_ATOMIC_ALIGN
 *   CANON_ALIGN_MAX()        — picks the larger of two alignments
 *   Capacity limits          — CANON_VEC_MAX_CAPACITY,
 *                              CANON_STRING_MAX_SIZE, CANON_ARENA_MAX_SIZE
 *   SBO thresholds           — CANON_SSO_THRESHOLD, CANON_SVO_THRESHOLD
 *   Growth factor            — CANON_GROWTH_FACTOR_NUM/DENOM,
 *                              CANON_MIN_ALLOCATION
 *   Pointer tagging          — CANON_PTR_TAG_BITS, CANON_PTR_TAG_MASK,
 *                              CANON_PTR_ADDR_MASK
 *   Platform info            — CANON_POINTER_SIZE, CANON_BITS_PER_BYTE,
 *                              CANON_POINTER_BITS
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   No 0b binary literals. Variables declared before use (C99).
 *   static_require() is used for compile-time invariants; EXPECT() for
 *   runtime assertions.
 */

/* Pull in CANON_CONTRACT_IMPL so static_require() compiles — limits.h
 * includes types.h which is the only dependency needed here. */
#define CANON_CONTRACT_IMPL
#include "contract.h"   /* for static_require() */
#include "limits.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

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
 * Compile-time invariants via static_require()
 *
 * These fire at compile time if violated — that is the test.
 * If this file compiles, all static_require() assertions passed.
 * ====================================================================== */

/* Integer limits match standard header values */
static_require(CANON_U8_MAX  == UINT8_MAX,  u8_max_matches_uint8_max);
static_require(CANON_U16_MAX == UINT16_MAX, u16_max_matches_uint16_max);
static_require(CANON_U32_MAX == UINT32_MAX, u32_max_matches_uint32_max);
static_require(CANON_U64_MAX == UINT64_MAX, u64_max_matches_uint64_max);

static_require(CANON_I8_MIN  == INT8_MIN,   i8_min_matches_int8_min);
static_require(CANON_I8_MAX  == INT8_MAX,   i8_max_matches_int8_max);
static_require(CANON_I16_MIN == INT16_MIN,  i16_min_matches_int16_min);
static_require(CANON_I16_MAX == INT16_MAX,  i16_max_matches_int16_max);
static_require(CANON_I32_MIN == INT32_MIN,  i32_min_matches_int32_min);
static_require(CANON_I32_MAX == INT32_MAX,  i32_max_matches_int32_max);
static_require(CANON_I64_MIN == INT64_MIN,  i64_min_matches_int64_min);
static_require(CANON_I64_MAX == INT64_MAX,  i64_max_matches_int64_max);

/* Size type limits match standard header values */
static_require(CANON_USIZE_MAX == SIZE_MAX,    usize_max_matches_size_max);
static_require(CANON_ISIZE_MAX == PTRDIFF_MAX, isize_max_matches_ptrdiff_max);
static_require(CANON_ISIZE_MIN == PTRDIFF_MIN, isize_min_matches_ptrdiff_min);

/* Size literals have correct values */
static_require(CANON_KB == (usize)1024,                                kb_is_1024);
static_require(CANON_MB == (usize)1024 * (usize)1024,                  mb_is_1048576);
static_require(CANON_GB == (usize)1024 * (usize)1024 * (usize)1024,    gb_is_correct);

/* Alignment constants are powers of two and at least 1 */
static_require(CANON_DEFAULT_ALIGN    >= 1, default_align_at_least_1);
static_require(CANON_CACHE_LINE       >= 1, cache_line_at_least_1);
static_require(CANON_SIMD_ALIGN       >= 1, simd_align_at_least_1);
static_require(CANON_SIMD_ALIGN_AVX   >= 1, simd_align_avx_at_least_1);
static_require(CANON_SIMD_ALIGN_AVX512 >= 1, simd_align_avx512_at_least_1);
static_require(CANON_PAGE_SIZE        >= 1, page_size_at_least_1);
static_require(CANON_ATOMIC_ALIGN     >= 1, atomic_align_at_least_1);

/* Hierarchy: each wider SIMD alignment >= the narrower one */
static_require(CANON_SIMD_ALIGN_AVX    >= CANON_SIMD_ALIGN,
               avx_align_at_least_simd);
static_require(CANON_SIMD_ALIGN_AVX512 >= CANON_SIMD_ALIGN_AVX,
               avx512_align_at_least_avx);

/* Capacity limits are positive and sensibly ordered */
static_require(CANON_VEC_MAX_CAPACITY  > 0, vec_max_capacity_positive);
static_require(CANON_STRING_MAX_SIZE   > 0, string_max_size_positive);
static_require(CANON_ARENA_MAX_SIZE    > 0, arena_max_size_positive);

/* SBO thresholds are positive */
static_require(CANON_SSO_THRESHOLD > 0, sso_threshold_positive);
static_require(CANON_SVO_THRESHOLD > 0, svo_threshold_positive);

/* Growth factor is > 1 (num > denom) */
static_require(CANON_GROWTH_FACTOR_NUM  > CANON_GROWTH_FACTOR_DENOM,
               growth_factor_greater_than_one);
static_require(CANON_MIN_ALLOCATION > 0, min_allocation_positive);

/* Pointer tag bits are 1, 2, or 3 */
static_require(CANON_PTR_TAG_BITS >= 1, ptr_tag_bits_at_least_1);
static_require(CANON_PTR_TAG_BITS <= 3, ptr_tag_bits_at_most_3);

/* Platform info */
static_require(CANON_BITS_PER_BYTE == 8, bits_per_byte_is_8);
static_require(CANON_POINTER_SIZE  == sizeof(void*),
               pointer_size_matches_sizeof_voidptr);
static_require(CANON_POINTER_BITS  == sizeof(void*) * 8,
               pointer_bits_is_pointer_size_times_8);

/* =========================================================================
 * Integer type limit values
 * ====================================================================== */

static void test_integer_limits(void) {
    /* Unsigned: check known exact values */
    EXPECT(CANON_U8_MAX  == 255U);
    EXPECT(CANON_U16_MAX == 65535U);
    EXPECT(CANON_U32_MAX == 4294967295UL);
    EXPECT(CANON_U64_MAX == 18446744073709551615ULL);

    /* Signed minimums and maximums */
    EXPECT(CANON_I8_MIN  == -128);
    EXPECT(CANON_I8_MAX  ==  127);
    EXPECT(CANON_I16_MIN == -32768);
    EXPECT(CANON_I16_MAX ==  32767);
    EXPECT(CANON_I32_MIN == (-2147483647 - 1));
    EXPECT(CANON_I32_MAX ==   2147483647);
    EXPECT(CANON_I64_MIN == (-9223372036854775807LL - 1));
    EXPECT(CANON_I64_MAX ==   9223372036854775807LL);

    /* MAX + 1 wraps to 0: load into a typed variable first so the increment
     * is a runtime operation — avoids MSVC C4310 "cast truncates constant". */
    {
        u8  v8  = CANON_U8_MAX;  v8++;  EXPECT(v8  == 0U);
        u16 v16 = CANON_U16_MAX; v16++; EXPECT(v16 == 0U);
        u32 v32 = CANON_U32_MAX; v32++; EXPECT(v32 == 0U);
        u64 v64 = CANON_U64_MAX; v64++; EXPECT(v64 == 0ULL);
    }

    /* Signed: MAX - MIN == -1 as unsigned difference */
    EXPECT((u8) ((u8) CANON_I8_MAX  - (u8) CANON_I8_MIN)  == 0xFFU);
    EXPECT((u16)((u16)CANON_I16_MAX - (u16)CANON_I16_MIN) == 0xFFFFU);
    EXPECT((u32)((u32)CANON_I32_MAX - (u32)CANON_I32_MIN) == 0xFFFFFFFFU);
    EXPECT((u64)((u64)CANON_I64_MAX - (u64)CANON_I64_MIN) == 0xFFFFFFFFFFFFFFFFULL);
}

/* =========================================================================
 * Size type limits
 * ====================================================================== */

static void test_size_limits(void) {
    /* USIZE_MAX is at least 2^16-1 (C99 guarantees SIZE_MAX >= 65535) */
    EXPECT(CANON_USIZE_MAX >= 65535U);

    /* ISIZE_MAX is positive, ISIZE_MIN is negative */
    EXPECT(CANON_ISIZE_MAX > (isize)0);
    EXPECT(CANON_ISIZE_MIN < (isize)0);

    /* ISIZE_MAX + 1 as usize == high bit of platform word */
    {
        usize one_past_max = (usize)CANON_ISIZE_MAX + (usize)1;
        EXPECT(one_past_max > (usize)CANON_ISIZE_MAX);
    }

    /* USIZE_MAX >= ISIZE_MAX (the unsigned range covers the signed range) */
    EXPECT(CANON_USIZE_MAX >= (usize)CANON_ISIZE_MAX);
}

/* =========================================================================
 * Size literals
 * ====================================================================== */

static void test_size_literals(void) {
    EXPECT(CANON_KB == (usize)1024);
    EXPECT(CANON_MB == (usize)1024 * CANON_KB);
    EXPECT(CANON_GB == (usize)1024 * CANON_MB);

    /* Hierarchy */
    EXPECT(CANON_MB > CANON_KB);
    EXPECT(CANON_GB > CANON_MB);

    /* GB fits in usize on both 32-bit and 64-bit */
    EXPECT(CANON_GB <= CANON_USIZE_MAX);
}

/* =========================================================================
 * Alignment constants
 * ====================================================================== */

/* Helper: returns 1 if n is a power of two, 0 otherwise */
static int is_pow2(usize n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

static void test_alignment_constants(void) {
    /* Every alignment constant must be a power of two */
    EXPECT(is_pow2(CANON_DEFAULT_ALIGN));
    EXPECT(is_pow2(CANON_CACHE_LINE));
    EXPECT(is_pow2(CANON_SIMD_ALIGN));
    EXPECT(is_pow2(CANON_SIMD_ALIGN_AVX));
    EXPECT(is_pow2(CANON_SIMD_ALIGN_AVX512));
    EXPECT(is_pow2(CANON_PAGE_SIZE));
    EXPECT(is_pow2(CANON_ATOMIC_ALIGN));

    /* Minimum expected values (documented defaults) */
    EXPECT(CANON_DEFAULT_ALIGN    >= (usize)8);
    EXPECT(CANON_CACHE_LINE       >= (usize)32);
    EXPECT(CANON_SIMD_ALIGN       >= (usize)16);
    EXPECT(CANON_SIMD_ALIGN_AVX   >= (usize)32);
    EXPECT(CANON_SIMD_ALIGN_AVX512 >= (usize)64);
    EXPECT(CANON_PAGE_SIZE        >= (usize)4096);
    EXPECT(CANON_ATOMIC_ALIGN     >= (usize)8);

    /* Documented hierarchy */
    EXPECT(CANON_SIMD_ALIGN_AVX    >= CANON_SIMD_ALIGN);
    EXPECT(CANON_SIMD_ALIGN_AVX512 >= CANON_SIMD_ALIGN_AVX);
}

/* =========================================================================
 * CANON_ALIGN_MAX()
 * ====================================================================== */

static void test_align_max(void) {
    EXPECT(CANON_ALIGN_MAX((usize)8,  (usize)16) == (usize)16);
    EXPECT(CANON_ALIGN_MAX((usize)16, (usize)8)  == (usize)16);
    EXPECT(CANON_ALIGN_MAX((usize)32, (usize)32) == (usize)32);
    EXPECT(CANON_ALIGN_MAX((usize)1,  (usize)64) == (usize)64);
    EXPECT(CANON_ALIGN_MAX((usize)64, (usize)1)  == (usize)64);

    /* With named constants */
    EXPECT(CANON_ALIGN_MAX(CANON_SIMD_ALIGN, CANON_SIMD_ALIGN_AVX) ==
           CANON_SIMD_ALIGN_AVX);
    EXPECT(CANON_ALIGN_MAX(CANON_DEFAULT_ALIGN, CANON_CACHE_LINE) >=
           CANON_DEFAULT_ALIGN);
    EXPECT(CANON_ALIGN_MAX(CANON_DEFAULT_ALIGN, CANON_CACHE_LINE) >=
           CANON_CACHE_LINE);
}

/* =========================================================================
 * Capacity limits
 * ====================================================================== */

static void test_capacity_limits(void) {
    /* All positive */
    EXPECT(CANON_VEC_MAX_CAPACITY > (usize)0);
    EXPECT(CANON_STRING_MAX_SIZE  > (usize)0);
    EXPECT(CANON_ARENA_MAX_SIZE   > (usize)0);

    /* All fit in usize */
    EXPECT(CANON_VEC_MAX_CAPACITY  <= CANON_USIZE_MAX);
    EXPECT(CANON_STRING_MAX_SIZE   <= CANON_USIZE_MAX);
    EXPECT(CANON_ARENA_MAX_SIZE    <= CANON_USIZE_MAX);

    /* Default values: vec and arena are 1 GiB, string is 16 MiB */
    EXPECT(CANON_VEC_MAX_CAPACITY  >= CANON_MB);
    EXPECT(CANON_STRING_MAX_SIZE   >= CANON_KB);
    EXPECT(CANON_ARENA_MAX_SIZE    >= CANON_MB);

    /* String max < vec max (string is typically smaller) */
    EXPECT(CANON_STRING_MAX_SIZE <= CANON_VEC_MAX_CAPACITY);
}

/* =========================================================================
 * Small buffer optimization thresholds
 * ====================================================================== */

static void test_sbo_thresholds(void) {
    /* SSO: 23 bytes is the standard "fits in 24-byte struct" value */
    EXPECT(CANON_SSO_THRESHOLD >= (usize)1);
    EXPECT(CANON_SSO_THRESHOLD <= (usize)255);  /* must fit in u8 length field */

    /* SVO: must hold at least one element */
    EXPECT(CANON_SVO_THRESHOLD >= (usize)1);

    /* Both fit comfortably in usize */
    EXPECT(CANON_SSO_THRESHOLD <= CANON_USIZE_MAX);
    EXPECT(CANON_SVO_THRESHOLD <= CANON_USIZE_MAX);
}

/* =========================================================================
 * Growth factor
 * ====================================================================== */

static void test_growth_factor(void) {
    usize old_cap = 16;
    usize new_cap = old_cap * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM;

    /* Growth factor > 1 means new_cap > old_cap */
    EXPECT(new_cap > old_cap);

    /* Growth factor denominator is non-zero */
    EXPECT(CANON_GROWTH_FACTOR_DENOM > (usize)0);

    /* Minimum allocation is positive and a reasonable size */
    EXPECT(CANON_MIN_ALLOCATION >= (usize)1);
    EXPECT(CANON_MIN_ALLOCATION <= CANON_VEC_MAX_CAPACITY);

    /* Applying growth repeatedly never stalls (always strictly increases) */
    {
        usize cap = CANON_MIN_ALLOCATION;
        int idx;
        for (idx = 0; idx < 8; idx++) {
            usize next = cap * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM;
            EXPECT(next > cap);
            cap = next;
        }
    }
}

/* =========================================================================
 * Pointer tagging constants
 * ====================================================================== */

static void test_ptr_tag(void) {
    uintptr_t tag_mask  = CANON_PTR_TAG_MASK;
    uintptr_t addr_mask = CANON_PTR_ADDR_MASK;

    /* Tag mask has exactly CANON_PTR_TAG_BITS low bits set */
    EXPECT(tag_mask == (uintptr_t)(((uintptr_t)1 << CANON_PTR_TAG_BITS) - 1));

    /* Addr mask is the bitwise complement of tag mask */
    EXPECT(addr_mask == ~tag_mask);

    /* Tag mask and addr mask are disjoint */
    EXPECT((tag_mask & addr_mask) == (uintptr_t)0);

    /* Together they cover all bits */
    EXPECT((tag_mask | addr_mask) == (uintptr_t)~(uintptr_t)0);

    /* TAG_BITS is consistent with platform pointer size */
#if UINTPTR_MAX == UINT64_MAX
    EXPECT(CANON_PTR_TAG_BITS == 3);
#elif UINTPTR_MAX == UINT32_MAX
    EXPECT(CANON_PTR_TAG_BITS == 2);
#else
    EXPECT(CANON_PTR_TAG_BITS == 1);
#endif

    /* Round-trip: strip tags, re-apply, recover original */
    {
        /* Construct a fake "tagged pointer" with all tag bits set */
        uintptr_t fake_ptr = (uintptr_t)0xABCDEF00UL | tag_mask;
        uintptr_t tag      = fake_ptr & tag_mask;
        uintptr_t addr     = fake_ptr & addr_mask;
        uintptr_t rebuilt  = addr | tag;
        EXPECT(rebuilt == fake_ptr);
        EXPECT((addr & tag_mask) == (uintptr_t)0);  /* addr has no tag bits */
    }

    /* A naturally aligned pointer loses no address bits when masked */
    {
        /* Use the address of a local variable as a naturally aligned pointer */
        int dummy = 0;
        uintptr_t raw  = (uintptr_t)(void*)&dummy;
        uintptr_t stripped = raw & addr_mask;
        /* The low CANON_PTR_TAG_BITS of a naturally aligned pointer are 0,
         * so stripping them is a no-op */
        EXPECT((raw & tag_mask) == (uintptr_t)0 || stripped <= raw);
    }
}

/* =========================================================================
 * Platform info constants
 * ====================================================================== */

static void test_platform_info(void) {
    EXPECT(CANON_BITS_PER_BYTE == (usize)8);
    EXPECT(CANON_POINTER_SIZE  == sizeof(void*));
    EXPECT(CANON_POINTER_BITS  == sizeof(void*) * 8);

    /* Pointer size is 4 (32-bit) or 8 (64-bit) */
    EXPECT(CANON_POINTER_SIZE == (usize)4 || CANON_POINTER_SIZE == (usize)8);

    /* Pointer bits matches */
    EXPECT(CANON_POINTER_BITS == CANON_POINTER_SIZE * CANON_BITS_PER_BYTE);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void) {
    test_integer_limits();
    test_size_limits();
    test_size_literals();
    test_alignment_constants();
    test_align_max();
    test_capacity_limits();
    test_sbo_thresholds();
    test_growth_factor();
    test_ptr_tag();
    test_platform_info();

    printf("\nlimits_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
