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
 * @file memory_test.c
 * @brief Tests for memory.h — safe low-level memory utilities
 *
 * Covers:
 *   - mem_regions_overlap
 *   - mem_alloc / mem_free
 *   - mem_align / mem_align_to
 *   - mem_is_aligned / mem_get_alignment / mem_is_power_of_two
 *   - mem_copy / mem_move / mem_zero / mem_set / mem_secure_zero
 *   - mem_compare / mem_equal
 *   - mem_is_all / mem_is_zero
 *   - mem_swap / mem_swap_buf
 *   - bytes_t variants: mem_copy_bytes, mem_move_bytes, mem_zero_bytes,
 *     mem_set_bytes, mem_equal_bytes, mem_is_zero_bytes, mem_secure_zero_bytes
 *   - Type-safe macros: mem_zero_type, mem_copy_type, mem_equal_type,
 *     mem_alloc_type, mem_alloc_array
 *   - MC/DC subcondition isolation for compound conditions
 *   - Invariant cross-checks (deterministic counterpart to fuzz invariants)
 *
 * MC/DC coverage note
 * ─────────────────────────────────────────────────────────────────────────
 *   Test groups suffixed with _mcdc target compound conditions where the
 *   surrounding correctness tests already cover the outcome but do not
 *   isolate each subcondition independently. The pattern follows
 *   checked_test.c — see docs/deviations.md for the established convention.
 *
 *   All tests are C99-pure: no _Alignof (we follow limits.h's C99 fallback
 *   constant of 16 for default alignment, asserting only the 8-byte floor
 *   that C99 §7.20.3 guarantees for malloc).
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random byte regions into mem_copy, mem_move, mem_compare,
 *     mem_equal, mem_is_zero, mem_swap on a fixed stack buffer.
 */

/* Must be defined in exactly one TU before including contract.h (via memory.h) */
#define CANON_CONTRACT_IMPL
#include "core/memory.h"

#include <stdio.h>
#include <string.h>

/* ── Harness ─────────────────────────────────────────────────────────────── */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                             \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL %s:%d  %s\n",                 \
                    __FILE__, __LINE__, #cond);                  \
            g_failed++;                                          \
        }                                                        \
    } while (0)

/* EXPECT_NOT_NULL — records failure and returns from calling function
 * if ptr is NULL, preventing cppcheck nullPointerRedundantCheck warnings. */
#define EXPECT_NOT_NULL(ptr)                                     \
    do {                                                         \
        if ((ptr) == NULL) {                                     \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",         \
                    __FILE__, __LINE__, #ptr);                   \
            g_failed++;                                          \
            return;                                              \
        }                                                        \
    } while (0)

/* ── mem_regions_overlap ─────────────────────────────────────────────────── */

static void test_regions_overlap_null(void)
{
    u8 buf[8];
    memset(buf, 0, sizeof(buf));
    EXPECT(!mem_regions_overlap(NULL, buf,  4));
    EXPECT(!mem_regions_overlap(buf,  NULL, 4));
    EXPECT(!mem_regions_overlap(NULL, NULL, 4));
}

static void test_regions_overlap_zero_size(void)
{
    u8 buf[8];
    memset(buf, 0, sizeof(buf));
    EXPECT(!mem_regions_overlap(buf, buf, 0));
}

static void test_regions_overlap_same_pointer(void)
{
    u8 buf[8];
    memset(buf, 0, sizeof(buf));
    EXPECT(mem_regions_overlap(buf, buf, 8));
}

static void test_regions_overlap_adjacent(void)
{
    u8 buf[16];
    memset(buf, 0, sizeof(buf));
    EXPECT(!mem_regions_overlap(buf, buf + 8, 8));
    EXPECT(!mem_regions_overlap(buf + 8, buf, 8));
}

static void test_regions_overlap_partial(void)
{
    u8 buf[16];
    memset(buf, 0, sizeof(buf));
    EXPECT(mem_regions_overlap(buf, buf + 4, 8));
    EXPECT(mem_regions_overlap(buf + 4, buf, 8));
}

static void test_regions_overlap_contained(void)
{
    u8 buf[16];
    memset(buf, 0, sizeof(buf));
    EXPECT(mem_regions_overlap(buf, buf + 2, 8));
}

/* REPLACES test_regions_overlap_non_overlapping. The original had an OR
 * assertion that was nearly tautological. Two distinct stack arrays do
 * not overlap; assert that directly in both directions. */
static void test_regions_overlap_distinct_stack_arrays(void)
{
    u8 a[8];
    u8 b[8];
    u8 c[16];
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
    memset(c, 0, sizeof(c));

    /* Distinct local arrays cannot overlap (the compiler is required to
     * give them disjoint storage). */
    EXPECT(!mem_regions_overlap(a, b, 8));
    EXPECT(!mem_regions_overlap(b, a, 8));

    /* Non-overlapping subregions of the same array */
    EXPECT(!mem_regions_overlap(c,     c + 8, 4));
    EXPECT(!mem_regions_overlap(c + 8, c,     4));
}

/* MC/DC for the final return:
 *   return (pa < pb + size) && (pb < pa + size);
 * Each subcondition must independently affect the outcome. */
static void test_regions_overlap_mcdc(void)
{
    u8 buf[32];
    memset(buf, 0, sizeof(buf));

    /* Both subconditions true → overlap */
    EXPECT(mem_regions_overlap(buf + 4, buf, 8));

    /* First subcondition false: pa(buf+8) >= pb+size(buf+4) */
    EXPECT(!mem_regions_overlap(buf + 8, buf, 4));

    /* Second subcondition false: pb(buf+8) >= pa+size(buf+4) */
    EXPECT(!mem_regions_overlap(buf, buf + 8, 4));
}

/* ── mem_alloc / mem_free ────────────────────────────────────────────────── */

static void test_alloc_zero_returns_null(void)
{
    void* p = mem_alloc(0);
    EXPECT(p == NULL);
}

static void test_alloc_nonzero_succeeds(void)
{
    void* p = mem_alloc(64);
    EXPECT_NOT_NULL(p);
    mem_free(p);
}

static void test_alloc_and_free_type_macro(void)
{
    i32* p = mem_alloc_type(i32);
    EXPECT_NOT_NULL(p);
    *p = 99;
    EXPECT(*p == 99);
    mem_free(p);
}

static void test_alloc_array_macro(void)
{
    i32*  arr = mem_alloc_array(i32, 8);
    usize i;
    EXPECT_NOT_NULL(arr);
    for (i = 0; i < 8; i++) arr[i] = (i32)i;
    for (i = 0; i < 8; i++) EXPECT(arr[i] == (i32)i);
    mem_free(arr);
}

/* sizeof(i32) * 0 → 0 → mem_alloc(0) → NULL. Passes on current code
 * (today's mem_alloc_array macro and the post-refactor version both
 * return NULL on count == 0). */
static void test_alloc_array_zero_count(void)
{
    i32* arr = mem_alloc_array(i32, 0);
    EXPECT(arr == NULL);
    /* mem_free(NULL) is safe; no leak even if the test failed */
    mem_free(arr);
}

/* Overflow detection: sizeof(u64) * ((CANON_USIZE_MAX / 8) + 2) wraps to
 * the small value 8 on both 32-bit and 64-bit platforms. Without overflow
 * checking, the old macro expanded to (u64*)mem_alloc(8), returning a
 * tiny valid buffer — and the caller, believing they had a giant array,
 * would corrupt the heap on first store past offset 0.
 *
 * The post-refactor macro routes through checked_mul, which detects the
 * wraparound and returns NULL. This test is the empirical evidence of
 * the refactor's claim: it FAILS against the unchecked macro (mem_alloc(8)
 * returns a non-NULL 8-byte buffer) and PASSES against the checked one.
 *
 * Why this expression specifically:
 *   - On 64-bit: count = 2^61 + 1, 8 * count = 2^64 + 8 → wraps to 8.
 *   - On 32-bit: count = 2^29 + 1, 8 * count = 2^32 + 8 → wraps to 8.
 *   The wrapped value is small enough that malloc will gladly allocate it,
 *   exposing the bug. Earlier-considered values like CANON_USIZE_MAX / 4
 *   wrap to values close to USIZE_MAX, where malloc rejects the allocation
 *   for system-resource reasons, accidentally producing the correct
 *   answer for the wrong reason.
 *
 * The count itself fits in usize on both platforms (it is < CANON_USIZE_MAX). */
static void test_alloc_array_overflow(void)
{
    usize count = (CANON_USIZE_MAX / 8) + 2;
    u64*  arr   = mem_alloc_array(u64, count);
    EXPECT(arr == NULL);
    /* mem_free(NULL) is safe even if the assertion failed */
    mem_free(arr);
}

/* Test the underlying primitive directly (not through the macro) to
 * exercise its full contract: zero element_size, zero count, overflow,
 * and the success path. */
static void test_alloc_array_checked_direct(void)
{
    void* p;

    /* Zero element_size → NULL */
    EXPECT(mem_alloc_array_checked(0, 100) == NULL);

    /* Zero count → NULL */
    EXPECT(mem_alloc_array_checked(sizeof(i32), 0) == NULL);

    /* Overflow → NULL.
     * (CANON_USIZE_MAX / 8) + 2 elements at 8 bytes each wraps to 8
     * on both 32-bit and 64-bit; see test_alloc_array_overflow for the
     * full rationale. */
    EXPECT(mem_alloc_array_checked(sizeof(u64), (CANON_USIZE_MAX / 8) + 2) == NULL);

    /* Success → non-NULL, freeable */
    p = mem_alloc_array_checked(sizeof(i32), 16);
    EXPECT_NOT_NULL(p);
    mem_free(p);
}

static void test_free_null_safe(void)
{
    mem_free(NULL);
    EXPECT(1);
}

/* ── mem_align / mem_align_to ────────────────────────────────────────────── */

static void test_align_zero(void)
{
    EXPECT(mem_align(0) == 0);
}

static void test_align_already_aligned(void)
{
    usize a = mem_align(CANON_DEFAULT_ALIGN);
    EXPECT(a == CANON_DEFAULT_ALIGN);
}

static void test_align_rounds_up(void)
{
    usize a = mem_align(1);
    EXPECT(a >= 1);
    EXPECT(a % CANON_DEFAULT_ALIGN == 0);
}

static void test_align_to_zero(void)
{
    EXPECT(mem_align_to(0, 16) == 0);
}

static void test_align_to_power_of_two(void)
{
    EXPECT(mem_align_to(1,  16) == 16);
    EXPECT(mem_align_to(16, 16) == 16);
    EXPECT(mem_align_to(17, 16) == 32);
    EXPECT(mem_align_to(1,   8) ==  8);
    EXPECT(mem_align_to(8,   8) ==  8);
    EXPECT(mem_align_to(9,   8) == 16);
}

/* Overflow boundary for mem_align: size > USIZE_MAX - (alignment - 1)
 * triggers the overflow guard. At the boundary itself, the guard does
 * NOT fire (the test is strict >, not >=), and align_up runs normally.
 * Since boundary == USIZE_MAX - (align-1) is already aligned (the low
 * align-1 bits are zero by construction), align_up returns boundary
 * unchanged. One past the boundary triggers the guard. */
static void test_align_overflow_returns_max(void)
{
    usize boundary = CANON_USIZE_MAX - (CANON_DEFAULT_ALIGN - 1);

    /* Just at boundary: guard does not fire, align_up returns boundary
     * (already aligned because low bits are zero by construction). */
    EXPECT(mem_align(boundary) == boundary);

    /* One past boundary: triggers overflow guard, returns USIZE_MAX. */
    EXPECT(mem_align(boundary + 1) == CANON_USIZE_MAX);
}

/* Same boundary structure for mem_align_to with explicit alignment 8. */
static void test_align_to_overflow_returns_max(void)
{
    usize boundary_8 = CANON_USIZE_MAX - 7;     /* USIZE_MAX - (8-1) */

    /* At boundary: guard does not fire, returns boundary unchanged. */
    EXPECT(mem_align_to(boundary_8, 8) == boundary_8);

    /* One past: triggers overflow guard, returns USIZE_MAX. */
    EXPECT(mem_align_to(boundary_8 + 1, 8) == CANON_USIZE_MAX);
}

/* ── mem_is_aligned ──────────────────────────────────────────────────────── */

static void test_is_aligned_null(void)
{
    EXPECT(!mem_is_aligned(NULL, 8));
}

/* REPLACES test_is_aligned_true. The original used a conditional EXPECT
 * inside an unreachable if-block plus a tautological EXPECT(1). This
 * version asserts at a known-aligned address without C11 _Alignof. */
static void test_is_aligned_at_known_address(void)
{
    /* A static u64 array has at least 8-byte natural alignment on every
     * supported platform (where _Alignof(u64) >= 8 — but we don't depend
     * on _Alignof, we depend on the fact). */
    static u64 aligned_buf[8];
    EXPECT(mem_is_aligned(aligned_buf, 1));
    EXPECT(mem_is_aligned(aligned_buf, 2));
    EXPECT(mem_is_aligned(aligned_buf, 4));
    EXPECT(mem_is_aligned(aligned_buf, 8));

    /* Misaligned offset from a u64-aligned base is not 2/4/8-aligned */
    {
        u8* misaligned = (u8*)aligned_buf + 1;
        EXPECT(!mem_is_aligned(misaligned, 2));
        EXPECT(!mem_is_aligned(misaligned, 4));
        EXPECT(!mem_is_aligned(misaligned, 8));
    }
}

/* REPLACES test_is_aligned_heap. The original had a tautological
 * EXPECT (`x || !x`). C99 §7.20.3 guarantees malloc returns a pointer
 * "suitably aligned ... [for] any type of object" — at least 8 bytes
 * on every supported platform. */
static void test_is_aligned_heap_guarantees_natural_align(void)
{
    void* p = mem_alloc(64);
    EXPECT_NOT_NULL(p);
    EXPECT(mem_is_aligned(p, 1));
    EXPECT(mem_is_aligned(p, 2));
    EXPECT(mem_is_aligned(p, 4));
    EXPECT(mem_is_aligned(p, 8));   /* C99 floor for malloc */
    mem_free(p);
}

/* MC/DC for mem_is_aligned: the require_msg checks plus the bitwise AND.
 * Existing tests cover NULL and the success path; this exercises the
 * AND across multiple alignment values relative to a known-aligned base. */
static void test_is_aligned_mcdc(void)
{
    static u64 base[4];
    EXPECT(mem_is_aligned(base, 1));
    EXPECT(mem_is_aligned(base, 8));

    /* Offsets isolate each bit of the alignment mask */
    EXPECT(!mem_is_aligned((u8*)base + 1, 2));
    EXPECT( mem_is_aligned((u8*)base + 2, 2));
    EXPECT(!mem_is_aligned((u8*)base + 2, 4));
    EXPECT( mem_is_aligned((u8*)base + 4, 4));
    EXPECT(!mem_is_aligned((u8*)base + 4, 8));
}

/* ── mem_get_alignment ───────────────────────────────────────────────────── */

static void test_get_alignment_null(void)
{
    EXPECT(mem_get_alignment(NULL) == 0);
}

static void test_get_alignment_nonzero(void)
{
    void* p = mem_alloc(64);
    usize a;
    EXPECT_NOT_NULL(p);
    a = mem_get_alignment(p);
    EXPECT(a >= 1);
    EXPECT(mem_is_power_of_two(a));
    mem_free(p);
}

/* ── mem_is_power_of_two ─────────────────────────────────────────────────── */

static void test_is_power_of_two(void)
{
    EXPECT(!mem_is_power_of_two(0));
    EXPECT( mem_is_power_of_two(1));
    EXPECT( mem_is_power_of_two(2));
    EXPECT(!mem_is_power_of_two(3));
    EXPECT( mem_is_power_of_two(4));
    EXPECT(!mem_is_power_of_two(5));
    EXPECT(!mem_is_power_of_two(6));
    EXPECT(!mem_is_power_of_two(7));
    EXPECT( mem_is_power_of_two(8));
    EXPECT( mem_is_power_of_two(16));
    EXPECT( mem_is_power_of_two(64));
    EXPECT( mem_is_power_of_two(128));
    EXPECT(!mem_is_power_of_two(100));
}

/* ── mem_copy ────────────────────────────────────────────────────────────── */

static void test_copy_basic(void)
{
    u8 src[8] = {1,2,3,4,5,6,7,8};
    u8 dst[8] = {0};
    mem_copy(dst, src, 8);
    EXPECT(memcmp(dst, src, 8) == 0);
}

static void test_copy_null_dest(void)
{
    u8 src[8] = {1,2,3,4,5,6,7,8};
    mem_copy(NULL, src, 8);
    EXPECT(1);
}

static void test_copy_null_src(void)
{
    u8 dst[8] = {0};
    mem_copy(dst, NULL, 8);
    EXPECT(1);
}

static void test_copy_zero_size(void)
{
    u8 src[8] = {1,2,3,4,5,6,7,8};
    u8 dst[8] = {0};
    mem_copy(dst, src, 0);
    EXPECT(dst[0] == 0);
}

static void test_copy_partial(void)
{
    u8 src[8] = {1,2,3,4,5,6,7,8};
    u8 dst[8] = {0};
    mem_copy(dst, src, 4);
    EXPECT(dst[0] == 1 && dst[3] == 4);
    EXPECT(dst[4] == 0);
}

/* ── mem_move ────────────────────────────────────────────────────────────── */

static void test_move_non_overlapping(void)
{
    u8 src[8] = {1,2,3,4,5,6,7,8};
    u8 dst[8] = {0};
    mem_move(dst, src, 8);
    EXPECT(memcmp(dst, src, 8) == 0);
}

static void test_move_overlapping_forward(void)
{
    u8 buf[16] = {1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0};
    mem_move(buf + 4, buf, 8);
    EXPECT(buf[4] == 1 && buf[11] == 8);
}

static void test_move_overlapping_backward(void)
{
    u8 buf[16] = {0,0,0,0,1,2,3,4,5,6,7,8,0,0,0,0};
    mem_move(buf, buf + 4, 8);
    EXPECT(buf[0] == 1 && buf[7] == 8);
}

static void test_move_null_safe(void)
{
    u8 buf[8] = {0};
    mem_move(NULL, buf, 8);
    mem_move(buf, NULL, 8);
    EXPECT(1);
}

/* ── mem_zero ────────────────────────────────────────────────────────────── */

static void test_zero_fills(void)
{
    u8    buf[32];
    usize i;
    memset(buf, 0xAB, sizeof(buf));
    mem_zero(buf, sizeof(buf));
    for (i = 0; i < sizeof(buf); i++) EXPECT(buf[i] == 0);
}

static void test_zero_null_safe(void)
{
    mem_zero(NULL, 8);
    EXPECT(1);
}

static void test_zero_size_zero(void)
{
    u8 buf[8];
    memset(buf, 0xAB, sizeof(buf));
    mem_zero(buf, 0);
    EXPECT(buf[0] == 0xAB);
}

/* ── mem_set ─────────────────────────────────────────────────────────────── */

static void test_set_fills(void)
{
    u8    buf[16];
    usize i;
    memset(buf, 0, sizeof(buf));
    mem_set(buf, 0xFF, sizeof(buf));
    for (i = 0; i < sizeof(buf); i++) EXPECT(buf[i] == 0xFF);
}

static void test_set_null_safe(void)
{
    mem_set(NULL, 0, 8);
    EXPECT(1);
}

/* ── mem_secure_zero ─────────────────────────────────────────────────────── */

static void test_secure_zero_fills(void)
{
    u8    buf[32];
    usize i;
    memset(buf, 0xAB, sizeof(buf));
    mem_secure_zero(buf, sizeof(buf));
    for (i = 0; i < sizeof(buf); i++) EXPECT(buf[i] == 0);
}

static void test_secure_zero_null_safe(void)
{
    mem_secure_zero(NULL, 8);
    EXPECT(1);
}

/* ── mem_compare ─────────────────────────────────────────────────────────── */

static void test_compare_equal(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {1,2,3,4,5,6,7,8};
    EXPECT(mem_compare(a, b, 8) == 0);
}

static void test_compare_less(void)
{
    u8 a[4] = {1,2,3,4};
    u8 b[4] = {1,2,3,5};
    EXPECT(mem_compare(a, b, 4) < 0);
}

static void test_compare_greater(void)
{
    u8 a[4] = {1,2,3,5};
    u8 b[4] = {1,2,3,4};
    EXPECT(mem_compare(a, b, 4) > 0);
}

static void test_compare_same_pointer(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    EXPECT(mem_compare(a, a, 8) == 0);
}

static void test_compare_both_null(void)
{
    EXPECT(mem_compare(NULL, NULL, 8) == 0);
}

static void test_compare_one_null(void)
{
    u8 a[8] = {0};
    EXPECT(mem_compare(NULL, a, 8) != 0);
    EXPECT(mem_compare(a, NULL, 8) != 0);
}

static void test_compare_zero_size(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {9,9,9,9,9,9,9,9};
    EXPECT(mem_compare(a, b, 0) == 0);
}

/* MC/DC asymmetry: the (!a) ? -1 : 1 ternary returns specific signed
 * values. Asserts both directions explicitly to catch regressions where
 * the ternary gets inverted. */
static void test_compare_null_asymmetry_mcdc(void)
{
    u8 a[4] = {1,2,3,4};
    EXPECT(mem_compare(NULL, a,    4) <  0);  /* NULL sorts before non-NULL */
    EXPECT(mem_compare(a,    NULL, 4) >  0);  /* non-NULL sorts after NULL  */
    EXPECT(mem_compare(NULL, NULL, 4) == 0);
}

/* ── mem_equal ───────────────────────────────────────────────────────────── */

static void test_equal_identical(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {1,2,3,4,5,6,7,8};
    EXPECT(mem_equal(a, b, 8));
}

static void test_equal_different(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {1,2,3,4,5,6,7,9};
    EXPECT(!mem_equal(a, b, 8));
}

static void test_equal_same_pointer(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    EXPECT(mem_equal(a, a, 8));
}

static void test_equal_both_null(void)
{
    EXPECT(mem_equal(NULL, NULL, 8));
}

static void test_equal_one_null(void)
{
    u8 a[8] = {0};
    EXPECT(!mem_equal(NULL, a, 8));
    EXPECT(!mem_equal(a, NULL, 8));
}

static void test_equal_zero_size(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {9,9,9,9,9,9,9,9};
    EXPECT(mem_equal(a, b, 0));
}

/* ── mem_is_all / mem_is_zero ────────────────────────────────────────────── */

static void test_is_all_true(void)
{
    u8 buf[16];
    memset(buf, 0xAB, sizeof(buf));
    EXPECT(mem_is_all(buf, 0xAB, sizeof(buf)));
}

static void test_is_all_false(void)
{
    u8 buf[16];
    memset(buf, 0xAB, sizeof(buf));
    buf[8] = 0x00;
    EXPECT(!mem_is_all(buf, 0xAB, sizeof(buf)));
}

static void test_is_all_null(void)
{
    EXPECT(!mem_is_all(NULL, 0, 8));
}

static void test_is_all_zero_size(void)
{
    u8 buf[8];
    memset(buf, 0, sizeof(buf));
    EXPECT(!mem_is_all(buf, 0, 0));
}

static void test_is_zero_true(void)
{
    u8 buf[16];
    memset(buf, 0, sizeof(buf));
    EXPECT(mem_is_zero(buf, sizeof(buf)));
}

static void test_is_zero_false(void)
{
    u8 buf[16];
    memset(buf, 0, sizeof(buf));
    buf[7] = 1;
    EXPECT(!mem_is_zero(buf, sizeof(buf)));
}

static void test_is_zero_null(void)
{
    EXPECT(!mem_is_zero(NULL, 8));
}

/* ── mem_swap ────────────────────────────────────────────────────────────── */

static void test_swap_basic(void)
{
    u8 a[8]      = {1,2,3,4,5,6,7,8};
    u8 b[8]      = {9,10,11,12,13,14,15,16};
    u8 orig_a[8] = {1,2,3,4,5,6,7,8};
    u8 orig_b[8] = {9,10,11,12,13,14,15,16};
    mem_swap(a, b, 8);
    EXPECT(memcmp(a, orig_b, 8) == 0);
    EXPECT(memcmp(b, orig_a, 8) == 0);
}

static void test_swap_null_safe(void)
{
    u8 buf[8] = {1,2,3,4,5,6,7,8};
    mem_swap(NULL, buf, 8);
    mem_swap(buf, NULL, 8);
    EXPECT(1);
}

static void test_swap_zero_size(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {9,10,11,12,13,14,15,16};
    mem_swap(a, b, 0);
    EXPECT(a[0] == 1 && b[0] == 9);
}

static void test_swap_single_byte(void)
{
    u8 a = 0xAA;
    u8 b = 0xBB;
    mem_swap(&a, &b, 1);
    EXPECT(a == 0xBB && b == 0xAA);
}

/* ── mem_swap_buf ────────────────────────────────────────────────────────── */

static void test_swap_buf_large(void)
{
    u8    a[128];
    u8    b[128];
    u8    scratch[128];
    u8    orig_a[128];
    u8    orig_b[128];
    usize i;
    for (i = 0; i < 128; i++) { a[i] = (u8)i; b[i] = (u8)(255 - i); }
    memcpy(orig_a, a, 128);
    memcpy(orig_b, b, 128);
    memset(scratch, 0, sizeof(scratch));
    mem_swap_buf(a, b, 128, scratch, 128);
    EXPECT(memcmp(a, orig_b, 128) == 0);
    EXPECT(memcmp(b, orig_a, 128) == 0);
}

static void test_swap_buf_null_safe(void)
{
    u8 a[8]       = {0};
    u8 scratch[8] = {0};
    mem_swap_buf(NULL, a,   8, scratch, 8);
    mem_swap_buf(a, NULL,   8, scratch, 8);
    mem_swap_buf(a, a + 1,  0, scratch, 8);
    EXPECT(1);
}

/* MC/DC for the four-way early-return:
 *   if (!a || !b || !scratch || size == 0) return;
 * Existing test_swap_buf_null_safe covers !a, !b, and size==0.
 * Missing: !scratch isolated from the other three. */
static void test_swap_buf_null_scratch_mcdc(void)
{
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {9,10,11,12,13,14,15,16};
    u8 orig_a[8];
    u8 orig_b[8];
    memcpy(orig_a, a, 8);
    memcpy(orig_b, b, 8);

    /* !scratch with all other args valid → no swap should occur */
    mem_swap_buf(a, b, 8, NULL, 8);
    EXPECT(memcmp(a, orig_a, 8) == 0);
    EXPECT(memcmp(b, orig_b, 8) == 0);
}

/* ── bytes_t variants ────────────────────────────────────────────────────── */

static void test_copy_bytes_basic(void)
{
    u8       src[8] = {1,2,3,4,5,6,7,8};
    u8       dst[8] = {0};
    bytes_t  d      = bytes_from(dst, 8);
    cbytes_t s      = cbytes_from(src, 8);
    usize    n      = mem_copy_bytes(d, s);
    EXPECT(n == 8);
    EXPECT(memcmp(dst, src, 8) == 0);
}

static void test_copy_bytes_null_src(void)
{
    u8       dst[8] = {0};
    bytes_t  d      = bytes_from(dst, 8);
    cbytes_t s      = cbytes_empty();
    usize    n      = mem_copy_bytes(d, s);
    EXPECT(n == 0);
}

static void test_move_bytes_overlapping(void)
{
    u8       buf[16] = {1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0};
    bytes_t  dst     = bytes_from(buf + 4, 8);
    cbytes_t src     = cbytes_from(buf,    8);
    usize    n       = mem_move_bytes(dst, src);
    EXPECT(n == 8);
    EXPECT(buf[4] == 1 && buf[11] == 8);
}

static void test_zero_bytes(void)
{
    u8      buf[16];
    bytes_t b;
    usize   i;
    memset(buf, 0xAB, 16);
    b = bytes_from(buf, 16);
    mem_zero_bytes(b);
    for (i = 0; i < 16; i++) EXPECT(buf[i] == 0);
}

static void test_set_bytes(void)
{
    u8      buf[16];
    bytes_t b;
    usize   i;
    memset(buf, 0, 16);
    b = bytes_from(buf, 16);
    mem_set_bytes(b, 0x5A);
    for (i = 0; i < 16; i++) EXPECT(buf[i] == 0x5A);
}

static void test_equal_bytes_identical(void)
{
    u8       a[8] = {1,2,3,4,5,6,7,8};
    u8       b[8] = {1,2,3,4,5,6,7,8};
    cbytes_t ca   = cbytes_from(a, 8);
    cbytes_t cb   = cbytes_from(b, 8);
    EXPECT(mem_equal_bytes(ca, cb));
}

static void test_equal_bytes_different(void)
{
    u8       a[8] = {1,2,3,4,5,6,7,8};
    u8       b[8] = {1,2,3,4,5,6,7,9};
    cbytes_t ca   = cbytes_from(a, 8);
    cbytes_t cb   = cbytes_from(b, 8);
    EXPECT(!mem_equal_bytes(ca, cb));
}

static void test_equal_bytes_different_lengths(void)
{
    u8       a[8] = {1,2,3,4,5,6,7,8};
    u8       b[4] = {1,2,3,4};
    cbytes_t ca   = cbytes_from(a, 8);
    cbytes_t cb   = cbytes_from(b, 4);
    EXPECT(!mem_equal_bytes(ca, cb));
}

static void test_equal_bytes_both_empty(void)
{
    cbytes_t a = cbytes_empty();
    cbytes_t b = cbytes_empty();
    EXPECT(mem_equal_bytes(a, b));
}

/* MC/DC for mem_equal_bytes branches:
 *   (1) a.len != b.len → false
 *   (2) a.ptr == b.ptr → true
 *   (3a) !a.ptr && b.ptr non-NULL
 *   (3b) !b.ptr && a.ptr non-NULL
 *   (4) both empty (already covered)
 *   (5) memcmp path
 *
 * The (3a) and (3b) cases construct malformed cbytes_t by direct
 * struct init, deliberately bypassing cbytes_from's require_msg.
 * This is the same pattern slice.h's MCDC-002 documents — testing
 * defensive behavior on values that the public API discourages but
 * cannot prevent (the {ptr, len} layout is public). */
static void test_equal_bytes_mcdc(void)
{
    u8       a[8] = {1,2,3,4,5,6,7,8};
    u8       b[8] = {1,2,3,4,5,6,7,8};
    cbytes_t same_a;
    cbytes_t null_ptr_a;
    cbytes_t real_b;
    cbytes_t real_a;
    cbytes_t null_ptr_b;
    cbytes_t copy_a;
    cbytes_t copy_b;

    /* Branch (2): same non-NULL pointer with non-zero length → true */
    same_a = cbytes_from(a, 8);
    EXPECT(mem_equal_bytes(same_a, same_a));

    /* Branch (3a): a.ptr NULL, b.ptr non-NULL, same length → false.
     * Construct via direct struct init to bypass cbytes_from's contract. */
    null_ptr_a.ptr = NULL;
    null_ptr_a.len = 8;
    real_b         = cbytes_from(b, 8);
    EXPECT(!mem_equal_bytes(null_ptr_a, real_b));

    /* Branch (3b): a.ptr non-NULL, b.ptr NULL, same length → false. */
    real_a         = cbytes_from(a, 8);
    null_ptr_b.ptr = NULL;
    null_ptr_b.len = 8;
    EXPECT(!mem_equal_bytes(real_a, null_ptr_b));

    /* Branch (5): different non-NULL pointers, equal length, equal content */
    copy_a = cbytes_from(a, 8);
    copy_b = cbytes_from(b, 8);
    EXPECT(mem_equal_bytes(copy_a, copy_b));
}

static void test_is_zero_bytes_true(void)
{
    u8       buf[16];
    cbytes_t b;
    memset(buf, 0, sizeof(buf));
    b = cbytes_from(buf, 16);
    EXPECT(mem_is_zero_bytes(b));
}

static void test_is_zero_bytes_false(void)
{
    u8       buf[16];
    cbytes_t b;
    memset(buf, 0, sizeof(buf));
    buf[8] = 1;
    b = cbytes_from(buf, 16);
    EXPECT(!mem_is_zero_bytes(b));
}

static void test_secure_zero_bytes(void)
{
    u8      buf[16];
    bytes_t b;
    usize   i;
    memset(buf, 0xAB, 16);
    b = bytes_from(buf, 16);
    mem_secure_zero_bytes(b);
    for (i = 0; i < 16; i++) EXPECT(buf[i] == 0);
}

/* ── Type-safe macros ────────────────────────────────────────────────────── */

static void test_zero_type_macro(void)
{
    i32 x = 42;
    mem_zero_type(&x);
    EXPECT(x == 0);
}

static void test_copy_type_macro(void)
{
    i32 src = 1234;
    i32 dst = 0;
    mem_copy_type(&dst, &src);
    EXPECT(dst == 1234);
}

static void test_equal_type_macro(void)
{
    i32 a = 42;
    i32 b = 42;
    i32 c = 99;
    EXPECT( mem_equal_type(&a, &b));
    EXPECT(!mem_equal_type(&a, &c));
}

static void test_zero_array_macro(void)
{
    u8    buf[32];
    usize i;
    memset(buf, 0xAB, sizeof(buf));
    mem_zero_array(buf);
    for (i = 0; i < sizeof(buf); i++) EXPECT(buf[i] == 0);
}

static void test_secure_zero_type_macro(void)
{
    i64 secret = (i64)0x0123456789ABCDEFLL;
    mem_secure_zero_type(&secret);
    EXPECT(secret == 0);
}

/* ── Invariant cross-checks ─────────────────────────────────────────────── */
/*
 * Deterministic counterparts to the fuzz harness's invariant checks.
 * These run as unit tests and contribute to MC/DC coverage measurement,
 * which the fuzz harness does not (the coverage CI job runs unit tests
 * only). Pattern matches checked_test.c's add_sub_inverse and mul_identity.
 */

static void test_invariant_copy_then_equal(void)
{
    /* Round-trip: copy src to dst, dst should equal src. */
    u8 src[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    u8 dst[16] = {0};
    mem_copy(dst, src, 16);
    EXPECT(mem_equal(src, dst, 16));
}

static void test_invariant_zero_then_is_zero(void)
{
    /* Composition: zeroing followed by zero-check is true. */
    u8 buf[32];
    memset(buf, 0xAB, sizeof(buf));
    mem_zero(buf, sizeof(buf));
    EXPECT(mem_is_zero(buf, sizeof(buf)));
}

static void test_invariant_set_then_is_all(void)
{
    /* Composition: setting followed by is_all with the same value is true. */
    u8 buf[32];
    memset(buf, 0, sizeof(buf));
    mem_set(buf, 0x5A, sizeof(buf));
    EXPECT(mem_is_all(buf, 0x5A, sizeof(buf)));
}

static void test_invariant_swap_involution(void)
{
    /* Involution: swap twice returns to original. */
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {9,10,11,12,13,14,15,16};
    u8 orig_a[8];
    u8 orig_b[8];
    memcpy(orig_a, a, 8);
    memcpy(orig_b, b, 8);

    mem_swap(a, b, 8);
    mem_swap(a, b, 8);

    EXPECT(memcmp(a, orig_a, 8) == 0);
    EXPECT(memcmp(b, orig_b, 8) == 0);
}

static void test_invariant_secure_zero_then_is_zero(void)
{
    /* Secure zero must produce the same observable result as mem_zero
     * for the is_zero predicate. The "secure" property (compiler can't
     * eliminate it) is not directly testable at this layer — it is a
     * property of how volatile is implemented in the compiler, not of
     * the source code. */
    u8 buf[32];
    memset(buf, 0xAB, sizeof(buf));
    mem_secure_zero(buf, sizeof(buf));
    EXPECT(mem_is_zero(buf, sizeof(buf)));
}

static void test_invariant_copy_bytes_then_equal_bytes(void)
{
    /* bytes_t-level round-trip mirroring the raw-pointer invariant. */
    u8       src[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    u8       dst[16] = {0};
    bytes_t  d       = bytes_from(dst, 16);
    cbytes_t s       = cbytes_from(src, 16);
    cbytes_t d_const;

    mem_copy_bytes(d, s);
    d_const = cbytes_from(dst, 16);
    EXPECT(mem_equal_bytes(d_const, s));
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    /* mem_regions_overlap */
    test_regions_overlap_null();
    test_regions_overlap_zero_size();
    test_regions_overlap_same_pointer();
    test_regions_overlap_adjacent();
    test_regions_overlap_partial();
    test_regions_overlap_contained();
    test_regions_overlap_distinct_stack_arrays();
    test_regions_overlap_mcdc();

    /* mem_alloc / mem_free */
    test_alloc_zero_returns_null();
    test_alloc_nonzero_succeeds();
    test_alloc_and_free_type_macro();
    test_alloc_array_macro();
    test_alloc_array_zero_count();
    test_alloc_array_overflow();
    test_alloc_array_checked_direct();
    test_free_null_safe();

    /* mem_align / mem_align_to */
    test_align_zero();
    test_align_already_aligned();
    test_align_rounds_up();
    test_align_to_zero();
    test_align_to_power_of_two();
    test_align_overflow_returns_max();
    test_align_to_overflow_returns_max();

    /* mem_is_aligned / mem_get_alignment / mem_is_power_of_two */
    test_is_aligned_null();
    test_is_aligned_at_known_address();
    test_is_aligned_heap_guarantees_natural_align();
    test_is_aligned_mcdc();
    test_get_alignment_null();
    test_get_alignment_nonzero();
    test_is_power_of_two();

    /* mem_copy */
    test_copy_basic();
    test_copy_null_dest();
    test_copy_null_src();
    test_copy_zero_size();
    test_copy_partial();

    /* mem_move */
    test_move_non_overlapping();
    test_move_overlapping_forward();
    test_move_overlapping_backward();
    test_move_null_safe();

    /* mem_zero */
    test_zero_fills();
    test_zero_null_safe();
    test_zero_size_zero();

    /* mem_set */
    test_set_fills();
    test_set_null_safe();

    /* mem_secure_zero */
    test_secure_zero_fills();
    test_secure_zero_null_safe();

    /* mem_compare */
    test_compare_equal();
    test_compare_less();
    test_compare_greater();
    test_compare_same_pointer();
    test_compare_both_null();
    test_compare_one_null();
    test_compare_zero_size();
    test_compare_null_asymmetry_mcdc();

    /* mem_equal */
    test_equal_identical();
    test_equal_different();
    test_equal_same_pointer();
    test_equal_both_null();
    test_equal_one_null();
    test_equal_zero_size();

    /* mem_is_all / mem_is_zero */
    test_is_all_true();
    test_is_all_false();
    test_is_all_null();
    test_is_all_zero_size();
    test_is_zero_true();
    test_is_zero_false();
    test_is_zero_null();

    /* mem_swap */
    test_swap_basic();
    test_swap_null_safe();
    test_swap_zero_size();
    test_swap_single_byte();

    /* mem_swap_buf */
    test_swap_buf_large();
    test_swap_buf_null_safe();
    test_swap_buf_null_scratch_mcdc();

    /* bytes_t variants */
    test_copy_bytes_basic();
    test_copy_bytes_null_src();
    test_move_bytes_overlapping();
    test_zero_bytes();
    test_set_bytes();
    test_equal_bytes_identical();
    test_equal_bytes_different();
    test_equal_bytes_different_lengths();
    test_equal_bytes_both_empty();
    test_equal_bytes_mcdc();
    test_is_zero_bytes_true();
    test_is_zero_bytes_false();
    test_secure_zero_bytes();

    /* Type-safe macros */
    test_zero_type_macro();
    test_copy_type_macro();
    test_equal_type_macro();
    test_zero_array_macro();
    test_secure_zero_type_macro();

    /* Invariant cross-checks */
    test_invariant_copy_then_equal();
    test_invariant_zero_then_is_zero();
    test_invariant_set_then_is_all();
    test_invariant_swap_involution();
    test_invariant_secure_zero_then_is_zero();
    test_invariant_copy_bytes_then_equal_bytes();

    if (g_failed == 0) {
        printf("OK  memory_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  memory_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */
/*
 * Input layout:
 *   [0..3]   u8[4]  fill_a   — pattern to fill buffer A
 *   [4..7]   u8[4]  fill_b   — pattern to fill buffer B
 *   [8]      u8     op       — operation to exercise (modulo 6)
 *   [9]      u8     size_raw — region size (modulo 64, min 1)
 */

#define FUZZ_BUF ((usize)128)

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static u8 a[FUZZ_BUF];
    static u8 b[FUZZ_BUF];
    static u8 scratch[FUZZ_BUF];

    u8    raw[10];
    u8    op;
    usize n;

    memset(raw, 0, sizeof(raw));
    if (size > sizeof(raw)) size = sizeof(raw);
    memcpy(raw, data, size);

    op = raw[8] % 6u;
    n  = (usize)(raw[9] % 64u) + 1u;

    memset(a,       raw[0], FUZZ_BUF);
    memset(b,       raw[4], FUZZ_BUF);
    memset(scratch, 0,      FUZZ_BUF);

    switch (op) {
        case 0:
            mem_copy(b, a, n);
            if (!mem_equal(b, a, n)) __builtin_trap();
            break;
        case 1:
            mem_move(b, a, n);
            if (!mem_equal(b, a, n)) __builtin_trap();
            break;
        case 2: {
            int r  = mem_compare(a, b, n);
            int r2 = mem_compare(b, a, n);
            if (r > 0 && r2 >= 0) __builtin_trap();
            if (r < 0 && r2 <= 0) __builtin_trap();
            break;
        }
        case 3:
            mem_zero(a, n);
            if (!mem_is_zero(a, n)) __builtin_trap();
            break;
        case 4:
            mem_swap(a, b, n);
            mem_swap(a, b, n);
            if (raw[0] != raw[4]) {
                usize i;
                for (i = 0; i < n; i++) {
                    if (a[i] != raw[0]) __builtin_trap();
                    if (b[i] != raw[4]) __builtin_trap();
                }
            }
            break;
        case 5:
            mem_secure_zero(a, n);
            if (!mem_is_zero(a, n)) __builtin_trap();
            break;
        default:
            break;
    }

    (void)scratch;
    return 0;
}

#endif /* CANON_FUZZING */
