/**
 * @file slice_test.c
 * @brief Tests for slice.h — non-owning views into contiguous memory
 *
 * Covers:
 *   - bytes_t: from, empty, is_empty, at, equal, slice, take, skip
 *   - cbytes_t: from, empty, bytes_as_const
 *   - str_t: from, from_cstr, empty, is_empty, equal,
 *            starts_with, ends_with, slice, take, skip, as_bytes
 *   - DEFINE_SLICE(T): from, empty, len, is_empty, get, get_unchecked,
 *                      at, first, last, slice, take, skip,
 *                      as_bytes, as_cbytes
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random offsets and lengths into bytes_t / str_t / slice_i32
 *     operations. Verifies no crash and returned pointers stay within
 *     backing buffers.
 */

#define CANON_CONTRACT_IMPL
#include "core/slice.h"

#include <stdio.h>
#include <string.h>

/* ── Generate typed slice for i32 (used by both unit and fuzz builds) ────── */

// cppcheck-suppress returnDanglingLifetime
DEFINE_SLICE(i32)

/* ══════════════════════════════════════════════════════════════════════════
   Unit test build
   ══════════════════════════════════════════════════════════════════════════ */

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

#define EXPECT_NOT_NULL(ptr)                                     \
    do {                                                         \
        if ((ptr) == NULL) {                                     \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",         \
                    __FILE__, __LINE__, #ptr);                   \
            g_failed++;                                          \
            return;                                              \
        }                                                        \
    } while (0)

/* ── bytes_t — construction ──────────────────────────────────────────────── */

static void test_bytes_from_basic(void)
{
    u8      buf[8] = {1,2,3,4,5,6,7,8};
    bytes_t b      = bytes_from(buf, 8);
    EXPECT(b.ptr == buf);
    EXPECT(b.len == 8);
}

static void test_bytes_from_zero_len(void)
{
    u8      buf[4] = {0};
    bytes_t b      = bytes_from(buf, 0);
    EXPECT(b.ptr == buf);
    EXPECT(b.len == 0);
}

static void test_bytes_from_null_zero_len(void)
{
    bytes_t b = bytes_from(NULL, 0);
    EXPECT(b.ptr == NULL);
    EXPECT(b.len == 0);
}

static void test_bytes_empty(void)
{
    bytes_t b = bytes_empty();
    EXPECT(b.ptr == NULL);
    EXPECT(b.len == 0);
}

/* ── bytes_t — queries ───────────────────────────────────────────────────── */

static void test_bytes_is_empty_true(void)
{
    bytes_t b = bytes_empty();
    EXPECT(bytes_is_empty(b));
}

static void test_bytes_is_empty_false(void)
{
    u8      buf[4] = {0};
    bytes_t b      = bytes_from(buf, 4);
    EXPECT(!bytes_is_empty(b));
}

static void test_bytes_at_valid(void)
{
    u8      buf[4] = {10,20,30,40};
    bytes_t b      = bytes_from(buf, 4);
    u8*     p      = bytes_at(b, 2);
    EXPECT_NOT_NULL(p);
    EXPECT(*p == 30);
}

static void test_bytes_at_oob(void)
{
    u8      buf[4] = {0};
    bytes_t b      = bytes_from(buf, 4);
    EXPECT(bytes_at(b, 4)  == NULL);
    EXPECT(bytes_at(b, 99) == NULL);
}

static void test_bytes_at_null_ptr(void)
{
    bytes_t b = bytes_empty();
    EXPECT(bytes_at(b, 0) == NULL);
}

static void test_bytes_equal_identical(void)
{
    u8      a[4] = {1,2,3,4};
    u8      b[4] = {1,2,3,4};
    bytes_t ba   = bytes_from(a, 4);
    bytes_t bb   = bytes_from(b, 4);
    EXPECT(bytes_equal(ba, bb));
}

static void test_bytes_equal_different(void)
{
    u8      a[4] = {1,2,3,4};
    u8      b[4] = {1,2,3,5};
    bytes_t ba   = bytes_from(a, 4);
    bytes_t bb   = bytes_from(b, 4);
    EXPECT(!bytes_equal(ba, bb));
}

static void test_bytes_equal_different_lengths(void)
{
    u8      a[4] = {1,2,3,4};
    u8      b[3] = {1,2,3};
    bytes_t ba   = bytes_from(a, 4);
    bytes_t bb   = bytes_from(b, 3);
    EXPECT(!bytes_equal(ba, bb));
}

static void test_bytes_equal_same_pointer(void)
{
    u8      buf[4] = {1,2,3,4};
    bytes_t b      = bytes_from(buf, 4);
    EXPECT(bytes_equal(b, b));
}

static void test_bytes_equal_both_empty(void)
{
    bytes_t a = bytes_empty();
    bytes_t b = bytes_empty();
    EXPECT(bytes_equal(a, b));
}

/* ── bytes_t — slicing ───────────────────────────────────────────────────── */

static void test_bytes_slice_basic(void)
{
    u8      buf[8] = {0,1,2,3,4,5,6,7};
    bytes_t b      = bytes_from(buf, 8);
    bytes_t s      = bytes_slice(b, 2, 5);
    EXPECT(s.len == 3);
    EXPECT(s.ptr == buf + 2);
}

static void test_bytes_slice_clamp_end(void)
{
    u8      buf[4] = {0,1,2,3};
    bytes_t b      = bytes_from(buf, 4);
    bytes_t s      = bytes_slice(b, 1, 100);
    EXPECT(s.len == 3);
}

static void test_bytes_slice_empty_range(void)
{
    u8      buf[4] = {0};
    bytes_t b      = bytes_from(buf, 4);
    bytes_t s      = bytes_slice(b, 2, 2);
    EXPECT(bytes_is_empty(s));
}

static void test_bytes_slice_oob_start(void)
{
    u8      buf[4] = {0};
    bytes_t b      = bytes_from(buf, 4);
    bytes_t s      = bytes_slice(b, 10, 20);
    EXPECT(bytes_is_empty(s));
}

static void test_bytes_take(void)
{
    u8      buf[8] = {0,1,2,3,4,5,6,7};
    bytes_t b      = bytes_from(buf, 8);
    bytes_t t      = bytes_take(b, 3);
    EXPECT(t.len == 3);
    EXPECT(t.ptr == buf);
}

static void test_bytes_skip(void)
{
    u8      buf[8] = {0,1,2,3,4,5,6,7};
    bytes_t b      = bytes_from(buf, 8);
    bytes_t s      = bytes_skip(b, 3);
    EXPECT(s.len == 5);
    EXPECT(s.ptr == buf + 3);
}

static void test_bytes_skip_all(void)
{
    u8      buf[4] = {0};
    bytes_t b      = bytes_from(buf, 4);
    bytes_t s      = bytes_skip(b, 4);
    EXPECT(bytes_is_empty(s));
}

/* ── cbytes_t ────────────────────────────────────────────────────────────── */

static void test_cbytes_from_basic(void)
{
    u8       buf[4] = {1,2,3,4};
    cbytes_t c      = cbytes_from(buf, 4);
    EXPECT(c.ptr == buf);
    EXPECT(c.len == 4);
}

static void test_cbytes_empty(void)
{
    cbytes_t c = cbytes_empty();
    EXPECT(c.ptr == NULL);
    EXPECT(c.len == 0);
}

static void test_bytes_as_const(void)
{
    u8       buf[4] = {1,2,3,4};
    bytes_t  b      = bytes_from(buf, 4);
    cbytes_t c      = bytes_as_const(b);
    EXPECT(c.ptr == buf);
    EXPECT(c.len == 4);
}

/* ── str_t — construction ────────────────────────────────────────────────── */

static void test_str_from_basic(void)
{
    str_t s = str_from("hello", 5);
    EXPECT(s.len == 5);
    EXPECT(s.ptr[0] == 'h');
}

static void test_str_from_cstr(void)
{
    str_t s = str_from_cstr("hello");
    EXPECT(s.len == 5);
    EXPECT(s.ptr != NULL);
}

static void test_str_from_cstr_null(void)
{
    str_t s = str_from_cstr(NULL);
    EXPECT(s.ptr == NULL);
    EXPECT(s.len == 0);
}

static void test_str_empty(void)
{
    str_t s = str_empty();
    EXPECT(s.ptr == NULL);
    EXPECT(s.len == 0);
}

/* ── str_t — queries ─────────────────────────────────────────────────────── */

static void test_str_is_empty_true(void)
{
    str_t s = str_empty();
    EXPECT(str_is_empty(s));
}

static void test_str_is_empty_false(void)
{
    str_t s = str_from_cstr("hi");
    EXPECT(!str_is_empty(s));
}

static void test_str_equal_identical(void)
{
    str_t a = str_from_cstr("hello");
    str_t b = str_from_cstr("hello");
    EXPECT(str_equal(a, b));
}

static void test_str_equal_different(void)
{
    str_t a = str_from_cstr("hello");
    str_t b = str_from_cstr("world");
    EXPECT(!str_equal(a, b));
}

static void test_str_equal_different_lengths(void)
{
    str_t a = str_from_cstr("hi");
    str_t b = str_from_cstr("hi!");
    EXPECT(!str_equal(a, b));
}

static void test_str_equal_both_empty(void)
{
    str_t a = str_empty();
    str_t b = str_empty();
    EXPECT(str_equal(a, b));
}

static void test_str_starts_with_true(void)
{
    str_t s      = str_from_cstr("hello world");
    str_t prefix = str_from_cstr("hello");
    EXPECT(str_starts_with(s, prefix));
}

static void test_str_starts_with_false(void)
{
    str_t s      = str_from_cstr("hello world");
    str_t prefix = str_from_cstr("world");
    EXPECT(!str_starts_with(s, prefix));
}

static void test_str_starts_with_empty_prefix(void)
{
    str_t s      = str_from_cstr("hello");
    str_t prefix = str_empty();
    EXPECT(str_starts_with(s, prefix));
}

static void test_str_starts_with_too_long(void)
{
    str_t s      = str_from_cstr("hi");
    str_t prefix = str_from_cstr("hello");
    EXPECT(!str_starts_with(s, prefix));
}

static void test_str_ends_with_true(void)
{
    str_t s      = str_from_cstr("hello world");
    str_t suffix = str_from_cstr("world");
    EXPECT(str_ends_with(s, suffix));
}

static void test_str_ends_with_false(void)
{
    str_t s      = str_from_cstr("hello world");
    str_t suffix = str_from_cstr("hello");
    EXPECT(!str_ends_with(s, suffix));
}

static void test_str_ends_with_empty_suffix(void)
{
    str_t s      = str_from_cstr("hello");
    str_t suffix = str_empty();
    EXPECT(str_ends_with(s, suffix));
}

/* ── str_t — slicing ─────────────────────────────────────────────────────── */

static void test_str_slice_basic(void)
{
    str_t s = str_from_cstr("hello");
    str_t r = str_slice(s, 1, 4);
    EXPECT(r.len == 3);
    /* Guard ptr before dereferencing — clang-tidy NullDereference */
    EXPECT_NOT_NULL(r.ptr);
    EXPECT(r.ptr[0] == 'e');
}

static void test_str_take(void)
{
    str_t s = str_from_cstr("hello");
    str_t t = str_take(s, 3);
    EXPECT(t.len == 3);
    EXPECT_NOT_NULL(t.ptr);
    EXPECT(t.ptr[0] == 'h');
}

static void test_str_skip(void)
{
    str_t s = str_from_cstr("hello");
    str_t r = str_skip(s, 2);
    EXPECT(r.len == 3);
    EXPECT_NOT_NULL(r.ptr);
    EXPECT(r.ptr[0] == 'l');
}

static void test_str_as_bytes(void)
{
    str_t    s = str_from_cstr("hi");
    cbytes_t c = str_as_bytes(s);
    EXPECT(c.len    == 2);
    EXPECT(c.ptr[0] == 'h');
}

/* ── DEFINE_SLICE(i32) ───────────────────────────────────────────────────── */

static void test_slice_i32_from(void)
{
    i32       arr[4] = {1,2,3,4};
    slice_i32 s      = slice_i32_from(arr, 4);
    EXPECT(s.ptr == arr);
    EXPECT(s.len == 4);
}

static void test_slice_i32_empty(void)
{
    slice_i32 s = slice_i32_empty();
    EXPECT(s.ptr == NULL);
    EXPECT(s.len == 0);
}

static void test_slice_i32_len(void)
{
    i32       arr[3] = {1,2,3};
    slice_i32 s      = slice_i32_from(arr, 3);
    EXPECT(slice_i32_len(s) == 3);
}

static void test_slice_i32_is_empty_true(void)
{
    slice_i32 s = slice_i32_empty();
    EXPECT(slice_i32_is_empty(s));
}

static void test_slice_i32_is_empty_false(void)
{
    i32       arr[2] = {1,2};
    slice_i32 s      = slice_i32_from(arr, 2);
    EXPECT(!slice_i32_is_empty(s));
}

static void test_slice_i32_get_valid(void)
{
    i32       arr[4] = {10,20,30,40};
    slice_i32 s      = slice_i32_from(arr, 4);
    i32       val    = 0;
    bool      ok     = slice_i32_get(s, 2, &val);
    EXPECT(ok);
    EXPECT(val == 30);
}

static void test_slice_i32_get_oob(void)
{
    i32       arr[2] = {1,2};
    slice_i32 s      = slice_i32_from(arr, 2);
    i32       val    = 0;
    bool      ok     = slice_i32_get(s, 5, &val);
    EXPECT(!ok);
}

static void test_slice_i32_get_null_out(void)
{
    i32       arr[2] = {1,2};
    slice_i32 s      = slice_i32_from(arr, 2);
    bool      ok     = slice_i32_get(s, 0, NULL);
    EXPECT(!ok);
}

static void test_slice_i32_get_unchecked_valid(void)
{
    i32       arr[4] = {10,20,30,40};
    slice_i32 s      = slice_i32_from(arr, 4);
    i32       val    = slice_i32_get_unchecked(s, 2);
    EXPECT(val == 30);
}

static void test_slice_i32_at_valid(void)
{
    i32       arr[4] = {10,20,30,40};
    slice_i32 s      = slice_i32_from(arr, 4);
    i32*      p      = slice_i32_at(s, 1);
    EXPECT_NOT_NULL(p);
    EXPECT(*p == 20);
}

static void test_slice_i32_at_oob(void)
{
    i32       arr[2] = {1,2};
    slice_i32 s      = slice_i32_from(arr, 2);
    EXPECT(slice_i32_at(s, 2)  == NULL);
    EXPECT(slice_i32_at(s, 99) == NULL);
}

static void test_slice_i32_first(void)
{
    i32       arr[3] = {10,20,30};
    slice_i32 s      = slice_i32_from(arr, 3);
    i32*      p      = slice_i32_first(s);
    EXPECT_NOT_NULL(p);
    EXPECT(*p == 10);
}

static void test_slice_i32_first_empty(void)
{
    slice_i32 s = slice_i32_empty();
    EXPECT(slice_i32_first(s) == NULL);
}

static void test_slice_i32_last(void)
{
    i32       arr[3] = {10,20,30};
    slice_i32 s      = slice_i32_from(arr, 3);
    i32*      p      = slice_i32_last(s);
    EXPECT_NOT_NULL(p);
    EXPECT(*p == 30);
}

static void test_slice_i32_last_empty(void)
{
    slice_i32 s = slice_i32_empty();
    EXPECT(slice_i32_last(s) == NULL);
}

static void test_slice_i32_slice_basic(void)
{
    i32       arr[6] = {0,1,2,3,4,5};
    slice_i32 s      = slice_i32_from(arr, 6);
    slice_i32 r      = slice_i32_slice(s, 2, 5);
    EXPECT(r.len    == 3);
    EXPECT(r.ptr[0] == 2);
    EXPECT(r.ptr[2] == 4);
}

static void test_slice_i32_slice_clamp(void)
{
    i32       arr[4] = {0,1,2,3};
    slice_i32 s      = slice_i32_from(arr, 4);
    slice_i32 r      = slice_i32_slice(s, 1, 100);
    EXPECT(r.len == 3);
}

static void test_slice_i32_slice_empty_range(void)
{
    i32       arr[4] = {0};
    slice_i32 s      = slice_i32_from(arr, 4);
    slice_i32 r      = slice_i32_slice(s, 2, 2);
    EXPECT(slice_i32_is_empty(r));
}

static void test_slice_i32_take(void)
{
    i32       arr[5] = {0,1,2,3,4};
    slice_i32 s      = slice_i32_from(arr, 5);
    slice_i32 t      = slice_i32_take(s, 3);
    EXPECT(t.len == 3);
    EXPECT(t.ptr == arr);
}

static void test_slice_i32_skip(void)
{
    i32       arr[5] = {0,1,2,3,4};
    slice_i32 s      = slice_i32_from(arr, 5);
    slice_i32 r      = slice_i32_skip(s, 2);
    EXPECT(r.len    == 3);
    EXPECT(r.ptr[0] == 2);
}

static void test_slice_i32_skip_all(void)
{
    i32       arr[3] = {0,1,2};
    slice_i32 s      = slice_i32_from(arr, 3);
    slice_i32 r      = slice_i32_skip(s, 3);
    EXPECT(slice_i32_is_empty(r));
}

static void test_slice_i32_as_bytes(void)
{
    i32       arr[2] = {1,2};
    slice_i32 s      = slice_i32_from(arr, 2);
    bytes_t   b      = slice_i32_as_bytes(s);
    EXPECT(b.len == 2 * sizeof(i32));
    EXPECT(b.ptr == (u8*)arr);
}

static void test_slice_i32_as_cbytes(void)
{
    i32       arr[2] = {1,2};
    slice_i32 s      = slice_i32_from(arr, 2);
    cbytes_t  c      = slice_i32_as_cbytes(s);
    EXPECT(c.len == 2 * sizeof(i32));
    EXPECT(c.ptr == (const u8*)arr);
}

static void test_slice_i32_as_bytes_empty(void)
{
    slice_i32 s = slice_i32_empty();
    bytes_t   b = slice_i32_as_bytes(s);
    EXPECT(b.ptr == NULL);
    EXPECT(b.len == 0);
}

static void test_slice_i32_write_through_at(void)
{
    i32       arr[4] = {0,0,0,0};
    slice_i32 s      = slice_i32_from(arr, 4);
    i32*      p      = slice_i32_at(s, 2);
    EXPECT_NOT_NULL(p);
    *p = 99;
    EXPECT(arr[2] == 99);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    /* bytes_t — construction */
    test_bytes_from_basic();
    test_bytes_from_zero_len();
    test_bytes_from_null_zero_len();
    test_bytes_empty();

    /* bytes_t — queries */
    test_bytes_is_empty_true();
    test_bytes_is_empty_false();
    test_bytes_at_valid();
    test_bytes_at_oob();
    test_bytes_at_null_ptr();
    test_bytes_equal_identical();
    test_bytes_equal_different();
    test_bytes_equal_different_lengths();
    test_bytes_equal_same_pointer();
    test_bytes_equal_both_empty();

    /* bytes_t — slicing */
    test_bytes_slice_basic();
    test_bytes_slice_clamp_end();
    test_bytes_slice_empty_range();
    test_bytes_slice_oob_start();
    test_bytes_take();
    test_bytes_skip();
    test_bytes_skip_all();

    /* cbytes_t */
    test_cbytes_from_basic();
    test_cbytes_empty();
    test_bytes_as_const();

    /* str_t — construction */
    test_str_from_basic();
    test_str_from_cstr();
    test_str_from_cstr_null();
    test_str_empty();

    /* str_t — queries */
    test_str_is_empty_true();
    test_str_is_empty_false();
    test_str_equal_identical();
    test_str_equal_different();
    test_str_equal_different_lengths();
    test_str_equal_both_empty();
    test_str_starts_with_true();
    test_str_starts_with_false();
    test_str_starts_with_empty_prefix();
    test_str_starts_with_too_long();
    test_str_ends_with_true();
    test_str_ends_with_false();
    test_str_ends_with_empty_suffix();

    /* str_t — slicing */
    test_str_slice_basic();
    test_str_take();
    test_str_skip();
    test_str_as_bytes();

    /* DEFINE_SLICE(i32) */
    test_slice_i32_from();
    test_slice_i32_empty();
    test_slice_i32_len();
    test_slice_i32_is_empty_true();
    test_slice_i32_is_empty_false();
    test_slice_i32_get_valid();
    test_slice_i32_get_oob();
    test_slice_i32_get_null_out();
    test_slice_i32_get_unchecked_valid();
    test_slice_i32_at_valid();
    test_slice_i32_at_oob();
    test_slice_i32_first();
    test_slice_i32_first_empty();
    test_slice_i32_last();
    test_slice_i32_last_empty();
    test_slice_i32_slice_basic();
    test_slice_i32_slice_clamp();
    test_slice_i32_slice_empty_range();
    test_slice_i32_take();
    test_slice_i32_skip();
    test_slice_i32_skip_all();
    test_slice_i32_as_bytes();
    test_slice_i32_as_cbytes();
    test_slice_i32_as_bytes_empty();
    test_slice_i32_write_through_at();

    if (g_failed == 0) {
        printf("OK  slice_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  slice_test  (%d assertion(s) failed)\n",
            g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */
/*
 * Input layout:
 *   [0]  u8  buf_fill  — byte value to fill backing buffers
 *   [1]  u8  start_raw — start index (modulo buffer length + 1)
 *   [2]  u8  end_raw   — end index   (modulo buffer length + 1)
 *   [3]  u8  op        — operation selector (modulo 10)
 *
 * All generated slice_i32_* functions are exercised here so the fuzz
 * build does not produce unused-function errors.
 */

#define FUZZ_BUF_LEN  ((usize)32)
#define FUZZ_I32_LEN  ((usize)8)

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static u8  byte_buf[FUZZ_BUF_LEN];
    static i32 i32_buf[FUZZ_I32_LEN];

    u8    raw[4];
    usize start;
    usize end;
    u8    op;
    usize i;

    memset(raw, 0, sizeof(raw));
    if (size > sizeof(raw)) size = sizeof(raw);
    memcpy(raw, data, size);

    memset(byte_buf, raw[0], FUZZ_BUF_LEN);
    for (i = 0; i < FUZZ_I32_LEN; i++) i32_buf[i] = (i32)i;

    start = (usize)(raw[1] % (u8)(FUZZ_BUF_LEN + 1));
    end   = (usize)(raw[2] % (u8)(FUZZ_BUF_LEN + 1));
    op    = raw[3] % 10u;

    switch (op) {
        /* ── bytes_t operations ─────────────────────────────────────────── */
        case 0: {
            bytes_t b = bytes_from(byte_buf, FUZZ_BUF_LEN);
            bytes_t s = bytes_slice(b, start, end);
            if (s.ptr != NULL) {
                if (s.ptr < byte_buf)                          __builtin_trap();
                if (s.ptr + s.len > byte_buf + FUZZ_BUF_LEN)  __builtin_trap();
            }
            break;
        }
        case 1: {
            bytes_t b = bytes_from(byte_buf, FUZZ_BUF_LEN);
            bytes_t t = bytes_take(b, start);
            bytes_t k = bytes_skip(b, start);
            if (t.ptr != NULL && t.ptr + t.len > byte_buf + FUZZ_BUF_LEN)
                __builtin_trap();
            if (k.ptr != NULL && k.ptr + k.len > byte_buf + FUZZ_BUF_LEN)
                __builtin_trap();
            break;
        }
        case 2: {
            bytes_t b = bytes_from(byte_buf, FUZZ_BUF_LEN);
            u8*     p = bytes_at(b, start);
            if (p != NULL) {
                if (p < byte_buf || p >= byte_buf + FUZZ_BUF_LEN)
                    __builtin_trap();
                (void)*p;
            }
            break;
        }
        /* ── str_t operations ───────────────────────────────────────────── */
        case 3: {
            str_t s = str_from((const char*)byte_buf, FUZZ_BUF_LEN);
            str_t r = str_slice(s, start, end);
            str_t t = str_take(s, start);
            str_t k = str_skip(s, start);
            if (r.ptr != NULL &&
                (const u8*)r.ptr + r.len > byte_buf + FUZZ_BUF_LEN)
                __builtin_trap();
            if (t.ptr != NULL &&
                (const u8*)t.ptr + t.len > byte_buf + FUZZ_BUF_LEN)
                __builtin_trap();
            if (k.ptr != NULL &&
                (const u8*)k.ptr + k.len > byte_buf + FUZZ_BUF_LEN)
                __builtin_trap();
            break;
        }
        /* ── slice_i32 — slice, take, skip ──────────────────────────────── */
        case 4: {
            usize     s_start = start % (FUZZ_I32_LEN + 1);
            usize     s_end   = end   % (FUZZ_I32_LEN + 1);
            slice_i32 s       = slice_i32_from(i32_buf, FUZZ_I32_LEN);
            slice_i32 r       = slice_i32_slice(s, s_start, s_end);
            slice_i32 t       = slice_i32_take(s, s_start);
            slice_i32 k       = slice_i32_skip(s, s_start);
            if (r.ptr != NULL && r.ptr + r.len > i32_buf + FUZZ_I32_LEN)
                __builtin_trap();
            if (t.ptr != NULL && t.ptr + t.len > i32_buf + FUZZ_I32_LEN)
                __builtin_trap();
            if (k.ptr != NULL && k.ptr + k.len > i32_buf + FUZZ_I32_LEN)
                __builtin_trap();
            break;
        }
        /* ── slice_i32 — at, get, get_unchecked ─────────────────────────── */
        case 5: {
            usize     idx = start % (FUZZ_I32_LEN + 1);
            slice_i32 s   = slice_i32_from(i32_buf, FUZZ_I32_LEN);
            i32*      p   = slice_i32_at(s, idx);
            if (p != NULL) {
                if (p < i32_buf || p >= i32_buf + FUZZ_I32_LEN)
                    __builtin_trap();
                (void)*p;
            }
            break;
        }
        case 6: {
            usize     idx = start % (FUZZ_I32_LEN + 1);
            slice_i32 s   = slice_i32_from(i32_buf, FUZZ_I32_LEN);
            i32       val = 0;
            bool      ok  = slice_i32_get(s, idx, &val);
            if ( ok && idx >= FUZZ_I32_LEN) __builtin_trap();
            if (!ok && idx <  FUZZ_I32_LEN) __builtin_trap();
            break;
        }
        case 7: {
            /* get_unchecked — only call with valid index */
            usize     idx = start % FUZZ_I32_LEN;
            slice_i32 s   = slice_i32_from(i32_buf, FUZZ_I32_LEN);
            i32       val = slice_i32_get_unchecked(s, idx);
            if (val != (i32)idx) __builtin_trap();
            break;
        }
        /* ── slice_i32 — len, is_empty, first, last, as_bytes, as_cbytes ── */
        case 8: {
            slice_i32 s = slice_i32_from(i32_buf, FUZZ_I32_LEN);
            if (slice_i32_len(s)      != FUZZ_I32_LEN) __builtin_trap();
            if (slice_i32_is_empty(s) != false)         __builtin_trap();
            {
                i32* f = slice_i32_first(s);
                i32* l = slice_i32_last(s);
                if (f == NULL || l == NULL)              __builtin_trap();
                if (f != &i32_buf[0])                    __builtin_trap();
                if (l != &i32_buf[FUZZ_I32_LEN - 1])    __builtin_trap();
            }
            break;
        }
        case 9: {
            slice_i32 s = slice_i32_from(i32_buf, FUZZ_I32_LEN);
            bytes_t   b = slice_i32_as_bytes(s);
            cbytes_t  c = slice_i32_as_cbytes(s);
            if (b.len != FUZZ_I32_LEN * sizeof(i32)) __builtin_trap();
            if (c.len != FUZZ_I32_LEN * sizeof(i32)) __builtin_trap();
            if (b.ptr != (u8*)i32_buf)               __builtin_trap();
            if (c.ptr != (const u8*)i32_buf)         __builtin_trap();
            break;
        }
        default:
            break;
    }

    return 0;
}

#endif /* CANON_FUZZING */
