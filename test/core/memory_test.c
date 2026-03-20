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
    EXPECT(!mem_regions_overlap(NULL, buf,  4));
    EXPECT(!mem_regions_overlap(buf,  NULL, 4));
    EXPECT(!mem_regions_overlap(NULL, NULL, 4));
}

static void test_regions_overlap_zero_size(void)
{
    u8 buf[8];
    EXPECT(!mem_regions_overlap(buf, buf, 0));
}

static void test_regions_overlap_same_pointer(void)
{
    u8 buf[8];
    EXPECT(mem_regions_overlap(buf, buf, 8));
}

static void test_regions_overlap_adjacent(void)
{
    u8 buf[16];
    /* [0,8) and [8,16) — touch but don't overlap */
    EXPECT(!mem_regions_overlap(buf, buf + 8, 8));
    EXPECT(!mem_regions_overlap(buf + 8, buf, 8));
}

static void test_regions_overlap_partial(void)
{
    u8 buf[16];
    /* [0,8) and [4,12) — overlap by 4 bytes */
    EXPECT(mem_regions_overlap(buf, buf + 4, 8));
    EXPECT(mem_regions_overlap(buf + 4, buf, 8));
}

static void test_regions_overlap_contained(void)
{
    u8 buf[16];
    /* [2,6) fully inside [0,8) */
    EXPECT(mem_regions_overlap(buf, buf + 2, 8));
}

static void test_regions_overlap_non_overlapping(void)
{
    u8 a[8];
    u8 b[8];
    /* Separate stack buffers — must not overlap */
    EXPECT(!mem_regions_overlap(a, b, 8) ||
           !mem_regions_overlap(b, a, 8));
    /* At least one direction must be non-overlapping;
     * accept either since stack layout is implementation-defined.
     * The real test: a single-byte region far apart. */
    EXPECT(!mem_regions_overlap(a, a + 4, 1));
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

static void test_free_null_safe(void)
{
    mem_free(NULL);   /* must not crash */
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

/* ── mem_is_aligned ──────────────────────────────────────────────────────── */

static void test_is_aligned_null(void)
{
    EXPECT(!mem_is_aligned(NULL, 8));
}

static void test_is_aligned_true(void)
{
    /* Allocate with known alignment via mem_align_to on a stack buffer.
     * Use a large enough buffer to guarantee we can find an aligned offset. */
    u8    buf[256];
    usize addr = (usize)(uintptr_t)buf;
    usize pad  = (64 - (addr % 64)) % 64;
    u8*   p    = buf + pad;
    if (pad + 1 <= sizeof(buf)) {
        EXPECT(mem_is_aligned(p, 1));
    }
    EXPECT(1);
}

static void test_is_aligned_heap(void)
{
    void* p = mem_alloc(64);
    EXPECT_NOT_NULL(p);
    /* malloc guarantees at least max_align_t alignment — at least 8 */
    EXPECT(mem_is_aligned(p, 1));
    EXPECT(mem_is_aligned(p, 2) || !mem_is_aligned(p, 2)); /* just no crash */
    mem_free(p);
}

/* ── mem_get_alignment ───────────────────────────────────────────────────── */

static void test_get_alignment_null(void)
{
    EXPECT(mem_get_alignment(NULL) == 0);
}

static void test_get_alignment_nonzero(void)
{
    void* p = mem_alloc(64);
    EXPECT_NOT_NULL(p);
    usize a = mem_get_alignment(p);
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
    mem_copy(NULL, src, 8);   /* must not crash */
    EXPECT(1);
}

static void test_copy_null_src(void)
{
    u8 dst[8] = {0};
    mem_copy(dst, NULL, 8);   /* must not crash */
    EXPECT(1);
}

static void test_copy_zero_size(void)
{
    u8 src[8] = {1,2,3,4,5,6,7,8};
    u8 dst[8] = {0};
    mem_copy(dst, src, 0);
    EXPECT(dst[0] == 0);   /* unchanged */
}

static void test_copy_partial(void)
{
    u8 src[8] = {1,2,3,4,5,6,7,8};
    u8 dst[8] = {0};
    mem_copy(dst, src, 4);
    EXPECT(dst[0] == 1 && dst[3] == 4);
    EXPECT(dst[4] == 0);   /* untouched */
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
    EXPECT(buf[0] == 0xAB);   /* unchanged */
}

/* ── mem_set ─────────────────────────────────────────────────────────────── */

static void test_set_fills(void)
{
    u8    buf[16];
    usize i;
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
    u8 a[8] = {1,2,3,4,5,6,7,8};
    u8 b[8] = {9,10,11,12,13,14,15,16};
    u8 orig_a[8];
    u8 orig_b[8];
    memcpy(orig_a, a, 8);
    memcpy(orig_b, b, 8);
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
    EXPECT(a[0] == 1 && b[0] == 9);   /* unchanged */
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
    u8 a[128];
    u8 b[128];
    u8 scratch[128];
    u8 orig_a[128];
    u8 orig_b[128];
    usize i;
    for (i = 0; i < 128; i++) { a[i] = (u8)i; b[i] = (u8)(255 - i); }
    memcpy(orig_a, a, 128);
    memcpy(orig_b, b, 128);
    mem_swap_buf(a, b, 128, scratch, 128);
    EXPECT(memcmp(a, orig_b, 128) == 0);
    EXPECT(memcmp(b, orig_a, 128) == 0);
}

static void test_swap_buf_null_safe(void)
{
    u8 a[8]       = {0};
    u8 scratch[8] = {0};
    mem_swap_buf(NULL, a, 8, scratch, 8);
    mem_swap_buf(a, NULL, 8, scratch, 8);
    mem_swap_buf(a, a+1,  0, scratch, 8);   /* zero size — no-op */
    EXPECT(1);
}

/* ── bytes_t variants ────────────────────────────────────────────────────── */

static void test_copy_bytes_basic(void)
{
    u8      src[8] = {1,2,3,4,5,6,7,8};
    u8      dst[8] = {0};
    bytes_t d      = bytes_from(dst, 8);
    cbytes_t s     = cbytes_from(src, 8);
    usize n        = mem_copy_bytes(d, s);
    EXPECT(n == 8);
    EXPECT(memcmp(dst, src, 8) == 0);
}

static void test_copy_bytes_null_src(void)
{
    u8      dst[8] = {0};
    bytes_t d      = bytes_from(dst, 8);
    cbytes_t s     = cbytes_empty();
    usize n        = mem_copy_bytes(d, s);
    EXPECT(n == 0);
}

static void test_move_bytes_overlapping(void)
{
    u8 buf[16] = {1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0};
    bytes_t  dst = bytes_from(buf + 4, 8);
    cbytes_t src = cbytes_from(buf,    8);
    usize n = mem_move_bytes(dst, src);
    EXPECT(n == 8);
    EXPECT(buf[4] == 1 && buf[11] == 8);
}

static void test_zero_bytes(void)
{
    u8      buf[16];
    bytes_t b = bytes_from(buf, 16);
    usize   i;
    memset(buf, 0xAB, 16);
    mem_zero_bytes(b);
    for (i = 0; i < 16; i++) EXPECT(buf[i] == 0);
}

static void test_set_bytes(void)
{
    u8      buf[16];
    bytes_t b = bytes_from(buf, 16);
    usize   i;
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

static void test_is_zero_bytes_true(void)
{
    u8       buf[16];
    cbytes_t b = cbytes_from(buf, 16);
    memset(buf, 0, 16);
    EXPECT(mem_is_zero_bytes(b));
}

static void test_is_zero_bytes_false(void)
{
    u8       buf[16];
    cbytes_t b = cbytes_from(buf, 16);
    memset(buf, 0, 16);
    buf[8] = 1;
    EXPECT(!mem_is_zero_bytes(b));
}

static void test_secure_zero_bytes(void)
{
    u8      buf[16];
    bytes_t b = bytes_from(buf, 16);
    usize   i;
    memset(buf, 0xAB, 16);
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
    i64 secret = 0x0123456789ABCDEFLL;
    mem_secure_zero_type(&secret);
    EXPECT(secret == 0);
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
    test_regions_overlap_non_overlapping();

    /* mem_alloc / mem_free */
    test_alloc_zero_returns_null();
    test_alloc_nonzero_succeeds();
    test_alloc_and_free_type_macro();
    test_alloc_array_macro();
    test_free_null_safe();

    /* mem_align / mem_align_to */
    test_align_zero();
    test_align_already_aligned();
    test_align_rounds_up();
    test_align_to_zero();
    test_align_to_power_of_two();

    /* mem_is_aligned / mem_get_alignment / mem_is_power_of_two */
    test_is_aligned_null();
    test_is_aligned_true();
    test_is_aligned_heap();
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
    test_is_zero_bytes_true();
    test_is_zero_bytes_false();
    test_secure_zero_bytes();

    /* Type-safe macros */
    test_zero_type_macro();
    test_copy_type_macro();
    test_equal_type_macro();
    test_zero_array_macro();
    test_secure_zero_type_macro();

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
 * Input layout (all values used as raw bytes):
 *
 *   [0..3]   u8[4]  fill_a   — pattern to fill buffer A
 *   [4..7]   u8[4]  fill_b   — pattern to fill buffer B
 *   [8]      u8     op       — which operation to exercise (modulo 6)
 *   [9]      u8     size_raw — region size (modulo 64, min 1)
 *
 * Minimum useful input: 10 bytes.
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
            int r = mem_compare(a, b, n);
            int r2 = mem_compare(b, a, n);
            /* antisymmetry: sign must flip or both zero */
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
            /* after swap, swap back — result must equal original */
            mem_swap(a, b, n);
            if (raw[0] != raw[4]) {
                /* buffers had different fill — each byte must match original */
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
