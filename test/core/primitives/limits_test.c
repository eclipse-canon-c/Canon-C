/**
 * @file limits_test.c
 * @brief Tests for limits.h
 *
 * Tests are organized by section matching the header:
 *   1. Integer type limits       — values match stdint.h constants
 *   2. Size type limits          — usize/isize bounds
 *   3. Size literals             — KB/MB/GB arithmetic
 *   4. Alignment constants       — power-of-two, relative ordering
 *   5. Capacity limits           — practical upper bounds
 *   6. SSO/SVO thresholds        — small buffer values
 *   7. Growth factor constants   — ratio and minimum allocation
 *   8. Pointer tagging           — tag/addr masks are complementary,
 *                                  tag bits match platform width
 *   9. Platform read-only info   — pointer size, bits per byte
 *  10. Override mechanism        — #define before include takes effect
 *
 * No dynamic allocation. No external dependencies beyond limits.h itself.
 * All tests are O(1) compile-time or single-expression runtime checks.
 */

#include <limits.h>
#include "limits.h"

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>

/* ============================================================================
 * Test Helpers
 * ========================================================================= */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(label, expected, actual)                                       \
    do {                                                                         \
        tests_run++;                                                             \
        if ((u64)(expected) == (u64)(actual)) {                                  \
            tests_passed++;                                                      \
        } else {                                                                 \
            tests_failed++;                                                      \
            fprintf(stderr, "FAIL: %s — expected %llu, got %llu  (%s:%d)\n",    \
                    label, (unsigned long long)(expected),                       \
                    (unsigned long long)(actual), __FILE__, __LINE__);           \
        }                                                                        \
    } while (0)

#define ASSERT_TRUE(label, expr) \
    do { \
        tests_run++; \
        if (expr) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", label, __FILE__, __LINE__); \
        } \
    } while (0)

/* Helper: check that a value is a power of two */
static int is_power_of_two(usize v) {
    return v > 0 && (v & (v - 1)) == 0;
}

/* ============================================================================
 * 1. Integer Type Limits
 * ========================================================================= */

static void test_integer_limits(void) {
    /* Unsigned: match stdint.h exactly */
    ASSERT_EQ("U8_MAX  == UINT8_MAX",  UINT8_MAX,  CANON_U8_MAX);
    ASSERT_EQ("U16_MAX == UINT16_MAX", UINT16_MAX, CANON_U16_MAX);
    ASSERT_EQ("U32_MAX == UINT32_MAX", UINT32_MAX, CANON_U32_MAX);
    ASSERT_EQ("U64_MAX == UINT64_MAX", UINT64_MAX, CANON_U64_MAX);

    /* Signed min: cast to u64 for macro comparison — bit patterns must match */
    ASSERT_EQ("I8_MIN  == INT8_MIN",  (u64)(i64)INT8_MIN,  (u64)(i64)CANON_I8_MIN);
    ASSERT_EQ("I16_MIN == INT16_MIN", (u64)(i64)INT16_MIN, (u64)(i64)CANON_I16_MIN);
    ASSERT_EQ("I32_MIN == INT32_MIN", (u64)(i64)INT32_MIN, (u64)(i64)CANON_I32_MIN);
    ASSERT_EQ("I64_MIN == INT64_MIN", (u64)(i64)INT64_MIN, (u64)(i64)CANON_I64_MIN);

    /* Signed max */
    ASSERT_EQ("I8_MAX  == INT8_MAX",  (u64)INT8_MAX,  (u64)CANON_I8_MAX);
    ASSERT_EQ("I16_MAX == INT16_MAX", (u64)INT16_MAX, (u64)CANON_I16_MAX);
    ASSERT_EQ("I32_MAX == INT32_MAX", (u64)INT32_MAX, (u64)CANON_I32_MAX);
    ASSERT_EQ("I64_MAX == INT64_MAX", (u64)INT64_MAX, (u64)CANON_I64_MAX);

    /* Sanity: unsigned max == signed max * 2 + 1 */
    ASSERT_EQ("U8_MAX  == 2*I8_MAX+1",  (u64)CANON_U8_MAX,  (u64)(2 * CANON_I8_MAX  + 1));
    ASSERT_EQ("U16_MAX == 2*I16_MAX+1", (u64)CANON_U16_MAX, (u64)(2 * CANON_I16_MAX + 1));
    ASSERT_EQ("U32_MAX == 2*I32_MAX+1", (u64)CANON_U32_MAX, (u64)(2u * CANON_I32_MAX + 1u));

    /* Widths increase monotonically */
    ASSERT_TRUE("U8_MAX < U16_MAX",  CANON_U8_MAX  < CANON_U16_MAX);
    ASSERT_TRUE("U16_MAX < U32_MAX", CANON_U16_MAX < CANON_U32_MAX);
    ASSERT_TRUE("U32_MAX < U64_MAX", CANON_U32_MAX < CANON_U64_MAX);
    ASSERT_TRUE("I8_MIN > I16_MIN",  CANON_I8_MIN  > CANON_I16_MIN);
    ASSERT_TRUE("I8_MAX < I16_MAX",  CANON_I8_MAX  < CANON_I16_MAX);
}

/* ============================================================================
 * 2. Size Type Limits
 * ========================================================================= */

static void test_size_limits(void) {
    ASSERT_EQ("USIZE_MAX == SIZE_MAX",    (u64)SIZE_MAX,    (u64)CANON_USIZE_MAX);
    ASSERT_EQ("ISIZE_MAX == PTRDIFF_MAX", (u64)PTRDIFF_MAX, (u64)CANON_ISIZE_MAX);
    ASSERT_EQ("ISIZE_MIN == PTRDIFF_MIN", (u64)(i64)PTRDIFF_MIN, (u64)(i64)CANON_ISIZE_MIN);

    /* ISIZE_MAX must be exactly (USIZE_MAX >> 1) on two's-complement platforms */
    ASSERT_EQ("ISIZE_MAX == USIZE_MAX >> 1",
        (u64)(CANON_USIZE_MAX >> 1), (u64)CANON_ISIZE_MAX);

    /* ISIZE_MIN must be negative */
    ASSERT_TRUE("ISIZE_MIN < 0", CANON_ISIZE_MIN < 0);

    /* USIZE_MAX must be at least 32-bit */
    ASSERT_TRUE("USIZE_MAX >= U32_MAX", CANON_USIZE_MAX >= CANON_U32_MAX);
}

/* ============================================================================
 * 3. Size Literals
 * ========================================================================= */

static void test_size_literals(void) {
    ASSERT_EQ("KB == 1024",           (usize)1024,                 CANON_KB);
    ASSERT_EQ("MB == 1024*KB",        (usize)1024 * CANON_KB,      CANON_MB);
    ASSERT_EQ("GB == 1024*MB",        (usize)1024 * CANON_MB,      CANON_GB);

    ASSERT_EQ("MB == 1048576",        (usize)1048576,              CANON_MB);
    ASSERT_EQ("GB == 1073741824",     (usize)1073741824,           CANON_GB);

    /* Ordering */
    ASSERT_TRUE("KB < MB", CANON_KB < CANON_MB);
    ASSERT_TRUE("MB < GB", CANON_MB < CANON_GB);

    /* All fit in usize (no truncation) */
    ASSERT_TRUE("KB fits in usize", CANON_KB <= CANON_USIZE_MAX);
    ASSERT_TRUE("MB fits in usize", CANON_MB <= CANON_USIZE_MAX);
    ASSERT_TRUE("GB fits in usize", CANON_GB <= CANON_USIZE_MAX);
}

/* ============================================================================
 * 4. Alignment Constants
 * ========================================================================= */

static void test_alignment_constants(void) {
    /* All alignment constants must be powers of two */
    ASSERT_TRUE("DEFAULT_ALIGN is power of two",     is_power_of_two(CANON_DEFAULT_ALIGN));
    ASSERT_TRUE("CACHE_LINE is power of two",        is_power_of_two(CANON_CACHE_LINE));
    ASSERT_TRUE("SIMD_ALIGN is power of two",        is_power_of_two(CANON_SIMD_ALIGN));
    ASSERT_TRUE("SIMD_ALIGN_AVX is power of two",    is_power_of_two(CANON_SIMD_ALIGN_AVX));
    ASSERT_TRUE("SIMD_ALIGN_AVX512 is power of two", is_power_of_two(CANON_SIMD_ALIGN_AVX512));
    ASSERT_TRUE("PAGE_SIZE is power of two",         is_power_of_two(CANON_PAGE_SIZE));
    ASSERT_TRUE("ATOMIC_ALIGN is power of two",      is_power_of_two(CANON_ATOMIC_ALIGN));

    /* Minimum expected values */
    ASSERT_TRUE("DEFAULT_ALIGN >= 8",      CANON_DEFAULT_ALIGN      >= 8);
    ASSERT_TRUE("CACHE_LINE >= 64",        CANON_CACHE_LINE         >= 64);
    ASSERT_TRUE("SIMD_ALIGN >= 16",        CANON_SIMD_ALIGN         >= 16);
    ASSERT_TRUE("SIMD_ALIGN_AVX >= 32",    CANON_SIMD_ALIGN_AVX     >= 32);
    ASSERT_TRUE("SIMD_ALIGN_AVX512 >= 64", CANON_SIMD_ALIGN_AVX512  >= 64);
    ASSERT_TRUE("PAGE_SIZE >= 4096",       CANON_PAGE_SIZE          >= 4096);
    ASSERT_TRUE("ATOMIC_ALIGN >= 8",       CANON_ATOMIC_ALIGN       >= 8);

    /* Relative ordering */
    ASSERT_TRUE("SIMD_ALIGN <= SIMD_ALIGN_AVX",     CANON_SIMD_ALIGN     <= CANON_SIMD_ALIGN_AVX);
    ASSERT_TRUE("SIMD_ALIGN_AVX <= SIMD_ALIGN_AVX512", CANON_SIMD_ALIGN_AVX <= CANON_SIMD_ALIGN_AVX512);

    /* ALIGN_MAX returns the larger of two values */
    ASSERT_EQ("ALIGN_MAX(16, 32) == 32", (usize)32, CANON_ALIGN_MAX(16, 32));
    ASSERT_EQ("ALIGN_MAX(32, 16) == 32", (usize)32, CANON_ALIGN_MAX(32, 16));
    ASSERT_EQ("ALIGN_MAX(16, 16) == 16", (usize)16, CANON_ALIGN_MAX(16, 16));
    ASSERT_EQ("ALIGN_MAX(DEFAULT, SIMD)",
        CANON_DEFAULT_ALIGN >= CANON_SIMD_ALIGN ? CANON_DEFAULT_ALIGN : CANON_SIMD_ALIGN,
        CANON_ALIGN_MAX(CANON_DEFAULT_ALIGN, CANON_SIMD_ALIGN));
}

/* ============================================================================
 * 5. Capacity Limits
 * ========================================================================= */

static void test_capacity_limits(void) {
    /* VEC_MAX_CAPACITY default is 1 GiB */
    ASSERT_EQ("VEC_MAX_CAPACITY == GB", CANON_GB, CANON_VEC_MAX_CAPACITY);

    /* STRING_MAX_SIZE default is 16 MiB */
    ASSERT_EQ("STRING_MAX_SIZE == 16*MB", (usize)16 * CANON_MB, CANON_STRING_MAX_SIZE);

    /* ARENA_MAX_SIZE default is 1 GiB */
    ASSERT_EQ("ARENA_MAX_SIZE == GB", CANON_GB, CANON_ARENA_MAX_SIZE);

    /* All fit in usize */
    ASSERT_TRUE("VEC_MAX_CAPACITY fits in usize",   CANON_VEC_MAX_CAPACITY  <= CANON_USIZE_MAX);
    ASSERT_TRUE("STRING_MAX_SIZE fits in usize",    CANON_STRING_MAX_SIZE   <= CANON_USIZE_MAX);
    ASSERT_TRUE("ARENA_MAX_SIZE fits in usize",     CANON_ARENA_MAX_SIZE    <= CANON_USIZE_MAX);

    /* Ordering: string limit is well below vec and arena limits */
    ASSERT_TRUE("STRING_MAX_SIZE < VEC_MAX_CAPACITY", CANON_STRING_MAX_SIZE < CANON_VEC_MAX_CAPACITY);
}

/* ============================================================================
 * 6. SSO / SVO Thresholds
 * ========================================================================= */

static void test_sso_svo_thresholds(void) {
    /* SSO: 23 bytes fits the classic 24-byte small-string struct */
    ASSERT_EQ("SSO_THRESHOLD == 23", (usize)23, CANON_SSO_THRESHOLD);

    /* The SSO struct is: 23 bytes data + 1 byte length/tag = 24 bytes total */
    ASSERT_TRUE("SSO_THRESHOLD + 1 == 24", CANON_SSO_THRESHOLD + 1 == 24);

    /* SVO: default 8 elements */
    ASSERT_EQ("SVO_THRESHOLD == 8", (usize)8, CANON_SVO_THRESHOLD);

    /* Both thresholds must be positive */
    ASSERT_TRUE("SSO_THRESHOLD > 0", CANON_SSO_THRESHOLD > 0);
    ASSERT_TRUE("SVO_THRESHOLD > 0", CANON_SVO_THRESHOLD > 0);
}

/* ============================================================================
 * 7. Growth Factor Constants
 * ========================================================================= */

static void test_growth_factor(void) {
    /* Default 1.5x: NUM=3, DENOM=2 */
    ASSERT_EQ("GROWTH_FACTOR_NUM == 3",   (usize)3, CANON_GROWTH_FACTOR_NUM);
    ASSERT_EQ("GROWTH_FACTOR_DENOM == 2", (usize)2, CANON_GROWTH_FACTOR_DENOM);

    /* The ratio must be > 1 to guarantee growth */
    ASSERT_TRUE("GROWTH_FACTOR_NUM > GROWTH_FACTOR_DENOM",
        CANON_GROWTH_FACTOR_NUM > CANON_GROWTH_FACTOR_DENOM);

    /* Applied to a concrete capacity: 100 * 3 / 2 == 150 */
    ASSERT_EQ("growth: 100 -> 150",
        (usize)150,
        (usize)100 * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM);

    /* Applied to capacity 1: must produce at least 1 (no zero-capacity loop) */
    ASSERT_TRUE("growth: 1 -> >= 1",
        (usize)1 * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM >= 1);

    /* MIN_ALLOCATION: default 32 bytes, positive, power of two */
    ASSERT_EQ("MIN_ALLOCATION == 32", (usize)32, CANON_MIN_ALLOCATION);
    ASSERT_TRUE("MIN_ALLOCATION > 0",            CANON_MIN_ALLOCATION > 0);
    ASSERT_TRUE("MIN_ALLOCATION is power of two", is_power_of_two(CANON_MIN_ALLOCATION));
}

/* ============================================================================
 * 8. Pointer Tagging
 * ========================================================================= */

static void test_pointer_tagging(void) {
    /* TAG_BITS must be 1, 2, or 3 depending on platform width */
#if UINTPTR_MAX == UINT64_MAX
    ASSERT_EQ("PTR_TAG_BITS == 3 on 64-bit", (usize)3, (usize)CANON_PTR_TAG_BITS);
#elif UINTPTR_MAX == UINT32_MAX
    ASSERT_EQ("PTR_TAG_BITS == 2 on 32-bit", (usize)2, (usize)CANON_PTR_TAG_BITS);
#else
    ASSERT_EQ("PTR_TAG_BITS == 1 on other",  (usize)1, (usize)CANON_PTR_TAG_BITS);
#endif

    /* TAG_MASK must have exactly PTR_TAG_BITS low bits set */
    uintptr_t expected_tag_mask = ((uintptr_t)1 << CANON_PTR_TAG_BITS) - (uintptr_t)1;
    ASSERT_EQ("PTR_TAG_MASK has correct low bits set",
        (u64)expected_tag_mask, (u64)CANON_PTR_TAG_MASK);

    /* ADDR_MASK must be the bitwise complement of TAG_MASK */
    ASSERT_EQ("PTR_ADDR_MASK == ~PTR_TAG_MASK",
        (u64)(uintptr_t)(~CANON_PTR_TAG_MASK), (u64)CANON_PTR_ADDR_MASK);

    /* TAG_MASK | ADDR_MASK must be all-ones — together they cover every bit */
    ASSERT_EQ("TAG_MASK | ADDR_MASK == all ones",
        (u64)(uintptr_t)(~(uintptr_t)0),
        (u64)(CANON_PTR_TAG_MASK | CANON_PTR_ADDR_MASK));

    /* TAG_MASK & ADDR_MASK must be zero — no bit belongs to both */
    ASSERT_EQ("TAG_MASK & ADDR_MASK == 0",
        (u64)0,
        (u64)(CANON_PTR_TAG_MASK & CANON_PTR_ADDR_MASK));

    /* TAG_MASK must be a sequence of low bits — is_power_of_two(mask + 1) */
    ASSERT_TRUE("PTR_TAG_MASK is a low-bit mask",
        is_power_of_two(CANON_PTR_TAG_MASK + 1));

    /* Round-trip: tag + untag recovers original pointer */
    uintptr_t original = (uintptr_t)0xDEADBEEF & CANON_PTR_ADDR_MASK;
    uintptr_t tag      = (uintptr_t)0x1;
    uintptr_t tagged   = (original & CANON_PTR_ADDR_MASK) | tag;
    uintptr_t untagged = tagged & CANON_PTR_ADDR_MASK;
    ASSERT_EQ("PTR round-trip: untag recovers original", (u64)original, (u64)untagged);
    ASSERT_EQ("PTR round-trip: tag extracted correctly",  (u64)tag,     (u64)(tagged & CANON_PTR_TAG_MASK));
}

/* ============================================================================
 * 9. Platform Read-Only Info
 * ========================================================================= */

static void test_platform_info(void) {
    ASSERT_EQ("POINTER_SIZE == sizeof(void*)", (usize)sizeof(void*), CANON_POINTER_SIZE);
    ASSERT_EQ("BITS_PER_BYTE == 8",            (usize)8,             CANON_BITS_PER_BYTE);

    ASSERT_EQ("POINTER_BITS == POINTER_SIZE * BITS_PER_BYTE",
        CANON_POINTER_SIZE * CANON_BITS_PER_BYTE, CANON_POINTER_BITS);

    /* Pointer size must be 4 or 8 bytes on all supported platforms */
    ASSERT_TRUE("POINTER_SIZE is 4 or 8",
        CANON_POINTER_SIZE == 4 || CANON_POINTER_SIZE == 8);

    /* POINTER_BITS must be 32 or 64 */
    ASSERT_TRUE("POINTER_BITS is 32 or 64",
        CANON_POINTER_BITS == 32 || CANON_POINTER_BITS == 64);

    /* PTR_TAG_BITS must fit within a pointer */
    ASSERT_TRUE("PTR_TAG_BITS < POINTER_BITS",
        (usize)CANON_PTR_TAG_BITS < CANON_POINTER_BITS);
}

/* ============================================================================
 * 10. Compile-Time Override Mechanism
 *
 * The override mechanism is verified by a separate translation unit:
 * limits_test_override.c, compiled with custom #defines set via
 * -D flags. It checks that each overridden constant reflects the
 * user-supplied value rather than the header default.
 *
 * Here we just verify that all constants are defined (compilation proof)
 * and have the expected default values on a system where nothing was
 * overridden.
 * ========================================================================= */

static void test_override_defaults(void) {
    /* Confirm defaults have not been silently zeroed or corrupted */
    ASSERT_TRUE("PAGE_SIZE > 0",          CANON_PAGE_SIZE          > 0);
    ASSERT_TRUE("CACHE_LINE > 0",         CANON_CACHE_LINE         > 0);
    ASSERT_TRUE("VEC_MAX_CAPACITY > 0",   CANON_VEC_MAX_CAPACITY   > 0);
    ASSERT_TRUE("STRING_MAX_SIZE > 0",    CANON_STRING_MAX_SIZE    > 0);
    ASSERT_TRUE("ARENA_MAX_SIZE > 0",     CANON_ARENA_MAX_SIZE     > 0);
    ASSERT_TRUE("MIN_ALLOCATION > 0",     CANON_MIN_ALLOCATION     > 0);
    ASSERT_TRUE("SSO_THRESHOLD > 0",      CANON_SSO_THRESHOLD      > 0);
    ASSERT_TRUE("SVO_THRESHOLD > 0",      CANON_SVO_THRESHOLD      > 0);
}

/* ============================================================================
 * Compile-Time Assertions (static_require equivalents via _Static_assert)
 *
 * These fire at compile time if any fundamental invariant is violated.
 * They complement the runtime tests above.
 * ========================================================================= */

/* KB/MB/GB are representable as usize */
typedef char _check_kb[(usize)CANON_KB  <= (usize)-1 ? 1 : -1];
typedef char _check_mb[(usize)CANON_MB  <= (usize)-1 ? 1 : -1];
typedef char _check_gb[(usize)CANON_GB  <= (usize)-1 ? 1 : -1];

/* Alignment constants are non-zero */
typedef char _check_default_align[CANON_DEFAULT_ALIGN > 0 ? 1 : -1];
typedef char _check_cache_line   [CANON_CACHE_LINE    > 0 ? 1 : -1];
typedef char _check_page_size    [CANON_PAGE_SIZE     > 0 ? 1 : -1];

/* ============================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("limits_test\n");
    printf("───────────\n");

    test_integer_limits();
    test_size_limits();
    test_size_literals();
    test_alignment_constants();
    test_capacity_limits();
    test_sso_svo_thresholds();
    test_growth_factor();
    test_pointer_tagging();
    test_platform_info();
    test_override_defaults();

    printf("\nResults: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED\n", tests_failed);
        return 1;
    }
    printf(" — all tests passed!\n");
    return 0;
}
