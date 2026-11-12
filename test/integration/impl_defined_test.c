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
 * @file impl_defined_test.c
 * @brief The implementation-defined behaviours Canon-C relies on, checked
 *        where the tests run (MISRA C:2012 Directive 1.1, VERIFY-071)
 *
 * docs/implementation-defined.md lists what Canon-C needs of the C
 * implementation beyond what C guarantees. This file checks each of those
 * needs that a running program can check, on every platform CI runs the
 * tests on. A failure means the platform does not do what the page says
 * Canon-C needs: the assumption failed, not the code that relies on it.
 *
 *   flat_addresses      C99 6.3.2.3p6: a pointer converted to uintptr_t is
 *                       its byte address, so an array's elements are at
 *                       addresses that step by the element's size
 *   address_to_pointer  6.3.2.3p5: an address inside an object, converted
 *                       back, points at that byte (ptr_align_up, _down)
 *   alignment_divides   an object of type T is at a multiple of T's
 *                       alignment (ptr_is_aligned, hashmap_init's test)
 *   objects_disjoint    two objects' address ranges do not meet
 *                       (mem_regions_overlap, ptr_in_range)
 *   uintptr_is_usize    uintptr_t and size_t have one width: Canon-C
 *                       converts addresses to usize
 *   signed_conversion   a value out of a signed type's range converts
 *                       without a signal (6.3.1.3p3), here modulo 2^N:
 *                       vec_range's representability check
 *   strtoll_is_i64      strtoll's and strtoull's ranges are i64's and u64's
 *                       (parse_i64, parse_u64)
 *   ieee_floats         float and double are IEEE 754 binary32 and
 *                       binary64, and a NaN is unequal to itself
 *                       (algo_cmp_f32, algo_cmp_f64)
 *   rename_existing     rename onto an existing file (7.19.4.2) replaces it
 *                       on POSIX systems and fails on Windows
 *                       (file_write_all_atomic)
 */

#define CANON_CONTRACT_IMPL
#include "core/primitives/ptr.h"
#include "core/primitives/compare.h"
#include "core/memory.h"
#include "core/arena.h"
#include "util/file.h"
#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_MAYBE_UNUSED __attribute__((unused))
#else
    #define CANON_MAYBE_UNUSED
#endif

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
 * Pointers as flat byte addresses (J.3.7)
 * ====================================================================== */

TEST(flat_addresses) {
    static u8  bytes[64];
    static u64 words[8];
    const uintptr_t b0 = (uintptr_t)&bytes[0];
    const uintptr_t w0 = (uintptr_t)&words[0];
    usize i;
    for (i = 0; i < sizeof bytes; i++) {
        EXPECT((uintptr_t)&bytes[i] == (b0 + (uintptr_t)i));
    }
    for (i = 0; i < (sizeof words / sizeof words[0]); i++) {
        EXPECT((uintptr_t)&words[i] == (w0 + (uintptr_t)(i * sizeof(u64))));
    }
}

TEST(address_to_pointer) {
    static u8 bytes[64];
    const uintptr_t b0 = (uintptr_t)&bytes[0];
    usize k;
    for (k = 0; k < sizeof bytes; k++) {
        EXPECT((u8*)(uintptr_t)(b0 + (uintptr_t)k) == &bytes[k]);
    }
    /* What ptr_align_up and ptr_align_down make of it: a byte of the
       same array, at a multiple of 8, within 8 bytes of the argument. */
    {
        u8* const from_up   = &bytes[1];
        u8* const from_down = &bytes[62];
        const u8* const up   = (const u8*)ptr_align_up(from_up, 8u);
        const u8* const down = (const u8*)ptr_align_down(from_down, 8u);
        EXPECT(((uintptr_t)up % 8u) == 0u);
        EXPECT((up >= from_up) && (up < (from_up + 8)));
        EXPECT(((uintptr_t)down % 8u) == 0u);
        EXPECT((down <= from_down) && (down > (from_down - 8)));
    }
}

/* C99 has no _Alignof: a type's alignment is the offset at which a struct
   places it after one char (a multiple of the alignment, whatever the
   padding). hashmap_init computes its slots' alignment the same way. */
typedef struct { char c; u16   t; } align_u16_;
typedef struct { char c; u32   t; } align_u32_;
typedef struct { char c; u64   t; } align_u64_;
typedef struct { char c; f64   t; } align_f64_;
typedef struct { char c; void* t; } align_ptr_;

TEST(alignment_divides) {
    static u16   h[4];
    static u32   s[4];
    static u64   w[4];
    static f64   d[4];
    static void* p[4];
    const uintptr_t ah = (uintptr_t)offsetof(align_u16_, t);
    const uintptr_t as = (uintptr_t)offsetof(align_u32_, t);
    const uintptr_t aw = (uintptr_t)offsetof(align_u64_, t);
    const uintptr_t ad = (uintptr_t)offsetof(align_f64_, t);
    const uintptr_t ap = (uintptr_t)offsetof(align_ptr_, t);
    usize i;
    for (i = 0; i < 4u; i++) {
        EXPECT(((uintptr_t)&h[i] % ah) == 0u);
        EXPECT(((uintptr_t)&s[i] % as) == 0u);
        EXPECT(((uintptr_t)&w[i] % aw) == 0u);
        EXPECT(((uintptr_t)&d[i] % ad) == 0u);
        EXPECT(((uintptr_t)&p[i] % ap) == 0u);
    }
    /* Canon-C's own test agrees, where the alignment is a power of two. */
    EXPECT(!is_power_of_two((usize)aw) || ptr_is_aligned(&w[1], (usize)aw));
}

TEST(objects_disjoint) {
    static u8 a[32];
    static u8 b[32];
    const uintptr_t ua = (uintptr_t)&a[0];
    const uintptr_t ub = (uintptr_t)&b[0];
    EXPECT(((ua + (uintptr_t)sizeof a) <= ub) || ((ub + (uintptr_t)sizeof b) <= ua));
    EXPECT(!mem_regions_overlap(a, b, sizeof a));
    EXPECT(!ptr_in_range(&b[0], &a[0], &a[0] + sizeof a));
    EXPECT(ptr_in_range(&a[31], &a[0], &a[0] + sizeof a));
}

TEST(uintptr_is_usize) {
    volatile uintptr_t umax = UINTPTR_MAX;
    volatile usize     smax = SIZE_MAX;
    EXPECT(sizeof(uintptr_t) == sizeof(usize));
    EXPECT((u64)umax == (u64)smax);
}

/* =========================================================================
 * Conversion to a signed type, out of its range (J.3.5)
 * ====================================================================== */

TEST(signed_conversion) {
    /* volatile: the values are not constants, so no compiler folds or
       warns about the conversions; each is the one vec_range makes. */
    volatile isize big  = 200;
    volatile u32   high = 0x80000000u;
    volatile u64   all  = UINT64_MAX;
    const i8  narrow = (i8)big;
    const i32 wide   = (i32)high;
    const i64 full   = (i64)all;
    EXPECT(narrow == (i8)-56);
    EXPECT(wide == INT32_MIN);
    EXPECT(full == (i64)-1);
    /* What vec_range tests: the value does not survive the round trip. */
    EXPECT((isize)narrow != big);
}

/* =========================================================================
 * strtoll and strtoull have i64's and u64's ranges (J.3.13)
 * ====================================================================== */

TEST(strtoll_is_i64) {
    char* end = NULL;
    long long v;
    unsigned long long u;
    EXPECT(sizeof(long long) == sizeof(i64));
    errno = 0;
    v = strtoll("9223372036854775807", &end, 10);
    EXPECT((errno == 0) && (v == (long long)INT64_MAX) && (*end == '\0'));
    errno = 0;
    v = strtoll("9223372036854775808", &end, 10);
    EXPECT((errno == ERANGE) && (v == (long long)INT64_MAX));
    errno = 0;
    v = strtoll("-9223372036854775809", &end, 10);
    EXPECT((errno == ERANGE) && (v == (long long)INT64_MIN));
    errno = 0;
    u = strtoull("18446744073709551615", &end, 10);
    EXPECT((errno == 0) && (u == (unsigned long long)UINT64_MAX));
    errno = 0;
    u = strtoull("18446744073709551616", &end, 10);
    EXPECT((errno == ERANGE) && (u == (unsigned long long)UINT64_MAX));
}

/* =========================================================================
 * IEEE 754 floating point (J.3.6)
 * ====================================================================== */

TEST(ieee_floats) {
    /* NaN from 0/0 at run time: IEEE 754 defines it, and no sanitizer
       the project builds with reports floating division by zero. */
    volatile f64 zero = 0.0;
    const f64 nan64 = zero / zero;
    const f32 nan32 = (f32)nan64;
    const f64 one64 = 1.0;
    const f32 one32 = 1.0f;
    EXPECT(FLT_RADIX == 2);
    EXPECT((FLT_MANT_DIG == 24) && (FLT_MAX_EXP == 128));
    EXPECT((DBL_MANT_DIG == 53) && (DBL_MAX_EXP == 1024));
    EXPECT(nan64 != nan64);
    EXPECT(nan32 != nan32);
    EXPECT(algo_cmp_f64(&nan64, &nan64, NULL) == 0);
    EXPECT(algo_cmp_f64(&nan64, &one64, NULL) == 1);
    EXPECT(algo_cmp_f64(&one64, &nan64, NULL) == -1);
    EXPECT(algo_cmp_f32(&nan32, &one32, NULL) == 1);
}

/* =========================================================================
 * rename onto an existing file (J.3.12)
 * ====================================================================== */

#define RENAME_PATH     "canon_test_impl_defined_rename.tmp"
#define RENAME_PATH_TMP "canon_test_impl_defined_rename.tmp.tmp"

TEST(rename_existing) {
    u8 buf[256];
    Arena arena;
    result_usize_Error first;
    result_usize_Error second;
    option_charp got;

    (void)remove(RENAME_PATH);
    (void)remove(RENAME_PATH_TMP);
    first  = file_write_all(RENAME_PATH, "old");
    second = file_write_all_atomic(RENAME_PATH, "new");
    arena_init(&arena, buf, sizeof buf);
    got = file_read_all_arena(RENAME_PATH, &arena);

    EXPECT(result_usize_Error_is_ok(first));
    EXPECT(option_charp_is_some(got));
#if defined(_WIN32)
    /* The Windows C library's rename refuses a name that exists (EACCES):
       the write fails, and the file is as it was. */
    EXPECT(result_usize_Error_is_err(second));
    EXPECT(option_charp_is_some(got) && (strcmp(option_charp_unwrap(got), "old") == 0));
#else
    /* POSIX rename replaces the file that has the name, atomically. */
    EXPECT(result_usize_Error_is_ok(second));
    EXPECT(option_charp_is_some(got) && (strcmp(option_charp_unwrap(got), "new") == 0));
#endif
    EXPECT(!file_exists(RENAME_PATH_TMP));
    (void)remove(RENAME_PATH);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void) {
    RUN(flat_addresses);
    RUN(address_to_pointer);
    RUN(alignment_divides);
    RUN(objects_disjoint);
    RUN(uintptr_is_usize);
    RUN(signed_conversion);
    RUN(strtoll_is_i64);
    RUN(ieee_floats);
    RUN(rename_existing);

    printf("\nimpl_defined_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
