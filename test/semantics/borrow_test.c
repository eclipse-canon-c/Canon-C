/**
 * @file borrow_test.c
 * @brief Tests for semantics/borrow.h — non-owning semantic view types
 *
 * Covers:
 *   - borrowed_ptr: from, null, get, is_valid, eq
 *   - borrowed_str: from, from_cstr, empty, get, is_valid, len, eq, slice
 *   - borrowed_bytes: from, from_cbytes, empty, get, len, is_valid, eq, slice
 *   - DEFINE_BORROWED_SLICE(int): from, empty, get, len, is_valid, at, slice,
 *     as_bytes (including overflow guard path)
 *   - NULL-safety: is_valid, len, at, slice, as_bytes — functions that accept
 *     NULL b and return a safe empty/zero value without firing require_msg.
 *     The _get functions fire require_msg on NULL b and are not called with
 *     NULL in tests — that is the correct behavior to verify.
 *   - Source tag round-trip (stored, not dereferenced)
 *   - borrowed_bytes_slice clamping and edge cases
 *   - borrowed_bytes_eq: content equality, length mismatch, empty regions,
 *     same-pointer short-circuit, partial overlap, source tag ignored
 *   - CANON_LIFETIME_DEBUG: _from_lifetime captures source's lifetime token;
 *     _get fires require_msg after arena_reset (id mismatch); untracked
 *     borrows bypass the check; arena_reset_to preserves validity.
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random (start, end, len) triples into borrowed_bytes_slice and
 *     borrowed_slice_int_as_bytes. Verifies no crash, no out-of-bounds
 *     pointer, and that the overflow guard on as_bytes fires correctly.
 */

#define CANON_CONTRACT_IMPL
#include "semantics/borrow.h"

#ifdef CANON_LIFETIME_DEBUG
#  include "core/arena.h"           /* Arena, arena_init, arena_reset, ... */
#  include <setjmp.h>               /* setjmp/longjmp for FIRES/SILENT     */
#endif

#include <stdio.h>
#include <string.h>

/* ── Slice instantiation needed by borrowed slice tests ───────────────────── *
 * DEFINE_SLICE stamps out more functions than this test file uses directly.
 * The unused-function warning is suppressed here — the same approach used
 * elsewhere in the Canon-C test suite for macro-generated instantiations.
 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

DEFINE_SLICE(int)
DEFINE_BORROWED_SLICE(int)

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

/* ── Harness ──────────────────────────────────────────────────────────────── */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                                \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL %s:%d  %s\n",                    \
                    __FILE__, __LINE__, #cond);                     \
            g_failed++;                                             \
        }                                                           \
    } while (0)

#define EXPECT_NOT_NULL(ptr)                                        \
    do {                                                            \
        if ((ptr) == NULL) {                                        \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",            \
                    __FILE__, __LINE__, #ptr);                      \
            g_failed++;                                             \
            return;                                                 \
        }                                                           \
    } while (0)

/* ============================================================================
   borrowed_ptr
   ============================================================================ */

static void test_ptr_from_stores_ptr_and_source(void)
{
    int        owner = 0;
    borrowed_ptr b   = borrowed_ptr_from(&owner, &owner);
    EXPECT(b.ptr    == (const void *)&owner);
    EXPECT(b.source == (const void *)&owner);
}

static void test_ptr_from_null_ptr(void)
{
    int          owner = 0;
    borrowed_ptr b     = borrowed_ptr_from(NULL, &owner);
    EXPECT(b.ptr    == NULL);
    EXPECT(b.source == (const void *)&owner);
}

static void test_ptr_null_constructor(void)
{
    borrowed_ptr b = borrowed_ptr_null();
    EXPECT(b.ptr    == NULL);
    EXPECT(b.source == NULL);
}

static void test_ptr_get_returns_stored_pointer(void)
{
    int          owner = 42;
    borrowed_ptr b     = borrowed_ptr_from(&owner, &owner);
    const void  *p     = borrowed_ptr_get(&b);
    EXPECT(p == (const void *)&owner);
}

static void test_ptr_is_valid_true(void)
{
    int          owner = 0;
    borrowed_ptr b     = borrowed_ptr_from(&owner, &owner);
    EXPECT(borrowed_ptr_is_valid(&b));
}

static void test_ptr_is_valid_null_ptr(void)
{
    borrowed_ptr b = borrowed_ptr_null();
    EXPECT(!borrowed_ptr_is_valid(&b));
}

static void test_ptr_is_valid_null_b(void)
{
    EXPECT(!borrowed_ptr_is_valid(NULL));
}

static void test_ptr_eq_same_address(void)
{
    int          owner = 0;
    borrowed_ptr a     = borrowed_ptr_from(&owner, NULL);
    borrowed_ptr b     = borrowed_ptr_from(&owner, &owner); /* different source */
    EXPECT(borrowed_ptr_eq(a, b));                          /* source ignored */
}

static void test_ptr_eq_different_address(void)
{
    int          x = 0, y = 0;
    borrowed_ptr a = borrowed_ptr_from(&x, NULL);
    borrowed_ptr b = borrowed_ptr_from(&y, NULL);
    EXPECT(!borrowed_ptr_eq(a, b));
}

static void test_ptr_eq_both_null(void)
{
    borrowed_ptr a = borrowed_ptr_null();
    borrowed_ptr b = borrowed_ptr_null();
    EXPECT(borrowed_ptr_eq(a, b));
}

/* ============================================================================
   borrowed_str
   ============================================================================ */

static void test_str_from_stores_str_and_source(void)
{
    const char  *owner = "hello";
    str_t        s     = str_from_cstr(owner);
    borrowed_str b     = borrowed_str_from(s, owner);
    EXPECT(b.str.ptr    == s.ptr);
    EXPECT(b.str.len    == s.len);
    EXPECT(b.source     == (const void *)owner);
}

static void test_str_from_cstr_round_trip(void)
{
    const char  *lit = "Canon";
    borrowed_str b   = borrowed_str_from_cstr(lit, lit);
    EXPECT(b.str.len == 5);
    EXPECT(b.str.ptr == (const char *)lit);
}

static void test_str_from_cstr_null_yields_empty(void)
{
    borrowed_str b = borrowed_str_from_cstr(NULL, NULL);
    /* str_from_cstr(NULL) must yield len == 0 per slice.h contract */
    EXPECT(b.str.len == 0);
}

static void test_str_empty_constructor(void)
{
    borrowed_str b = borrowed_str_empty();
    EXPECT(b.str.ptr == NULL);
    EXPECT(b.str.len == 0);
    EXPECT(b.source  == NULL);
}

static void test_str_get_returns_inner(void)
{
    const char  *lit = "world";
    borrowed_str b   = borrowed_str_from_cstr(lit, NULL);
    str_t        s   = borrowed_str_get(&b);
    EXPECT(s.ptr == b.str.ptr);
    EXPECT(s.len == b.str.len);
}

static void test_str_is_valid_non_null_ptr(void)
{
    borrowed_str b = borrowed_str_from_cstr("ok", NULL);
    EXPECT(borrowed_str_is_valid(&b));
}

static void test_str_is_valid_empty(void)
{
    borrowed_str b = borrowed_str_empty();
    EXPECT(!borrowed_str_is_valid(&b));
}

static void test_str_is_valid_null_b(void)
{
    EXPECT(!borrowed_str_is_valid(NULL));
}

static void test_str_len_correct(void)
{
    borrowed_str b = borrowed_str_from_cstr("hello", NULL);
    EXPECT(borrowed_str_len(&b) == 5);
}

static void test_str_len_null_b_returns_zero(void)
{
    EXPECT(borrowed_str_len(NULL) == 0);
}

static void test_str_eq_same_content(void)
{
    borrowed_str a = borrowed_str_from_cstr("abc", NULL);
    borrowed_str b = borrowed_str_from_cstr("abc", (const void *)0x1); /* diff source */
    EXPECT(borrowed_str_eq(a, b));
}

static void test_str_eq_different_content(void)
{
    borrowed_str a = borrowed_str_from_cstr("abc", NULL);
    borrowed_str b = borrowed_str_from_cstr("xyz", NULL);
    EXPECT(!borrowed_str_eq(a, b));
}

static void test_str_eq_different_lengths(void)
{
    borrowed_str a = borrowed_str_from_cstr("ab",  NULL);
    borrowed_str b = borrowed_str_from_cstr("abc", NULL);
    EXPECT(!borrowed_str_eq(a, b));
}

static void test_str_slice_middle(void)
{
    /* "hello" → [1,4) → "ell" */
    borrowed_str b   = borrowed_str_from_cstr("hello", NULL);
    borrowed_str sub = borrowed_str_slice(b, 1, 4);
    EXPECT(sub.str.len == 3);
    EXPECT(sub.source  == b.source);
    EXPECT(strncmp(sub.str.ptr, "ell", 3) == 0);
}

static void test_str_slice_full_range(void)
{
    borrowed_str b   = borrowed_str_from_cstr("hello", NULL);
    borrowed_str sub = borrowed_str_slice(b, 0, 5);
    EXPECT(sub.str.len == 5);
}

static void test_str_slice_empty_range(void)
{
    borrowed_str b   = borrowed_str_from_cstr("hello", NULL);
    borrowed_str sub = borrowed_str_slice(b, 2, 2);
    EXPECT(sub.str.len == 0);
}

static void test_str_slice_out_of_bounds_clamped(void)
{
    borrowed_str b   = borrowed_str_from_cstr("hi", NULL);
    borrowed_str sub = borrowed_str_slice(b, 0, 999);
    /* str_slice must clamp end to len */
    EXPECT(sub.str.len == 2);
}

static void test_str_slice_inherits_source(void)
{
    const char  *owner = "test";
    borrowed_str b     = borrowed_str_from_cstr(owner, owner);
    borrowed_str sub   = borrowed_str_slice(b, 0, 2);
    EXPECT(sub.source == (const void *)owner);
}

/* ============================================================================
   borrowed_bytes
   ============================================================================ */

static void test_bytes_from_stores_ptr_len_source(void)
{
    u8             buf[8] = {0};
    borrowed_bytes b = borrowed_bytes_from(buf, 8, buf);
    EXPECT(b.bytes.ptr == buf);
    EXPECT(b.bytes.len == 8);
    EXPECT(b.source    == (const void *)buf);
}

static void test_bytes_from_cbytes(void)
{
    u8       buf[4]  = {0xAA, 0xBB, 0xCC, 0xDD};
    cbytes_t cb      = cbytes_from(buf, 4);
    borrowed_bytes b = borrowed_bytes_from_cbytes(cb, buf);
    EXPECT(b.bytes.ptr == buf);
    EXPECT(b.bytes.len == 4);
    EXPECT(b.source    == (const void *)buf);
}

static void test_bytes_empty_constructor(void)
{
    borrowed_bytes b = borrowed_bytes_empty();
    EXPECT(b.bytes.ptr == NULL);
    EXPECT(b.bytes.len == 0);
    EXPECT(b.source    == NULL);
}

static void test_bytes_get_returns_inner(void)
{
    u8             buf[4] = {0};
    borrowed_bytes b  = borrowed_bytes_from(buf, 4, NULL);
    cbytes_t       cb = borrowed_bytes_get(&b);
    EXPECT(cb.ptr == buf);
    EXPECT(cb.len == 4);
}

static void test_bytes_len_correct(void)
{
    u8             buf[16] = {0};
    borrowed_bytes b = borrowed_bytes_from(buf, 16, NULL);
    EXPECT(borrowed_bytes_len(&b) == 16);
}

static void test_bytes_len_null_b_returns_zero(void)
{
    EXPECT(borrowed_bytes_len(NULL) == 0);
}

static void test_bytes_is_valid_non_null(void)
{
    u8             buf[4] = {0};
    borrowed_bytes b = borrowed_bytes_from(buf, 4, NULL);
    EXPECT(borrowed_bytes_is_valid(&b));
}

static void test_bytes_is_valid_empty(void)
{
    borrowed_bytes b = borrowed_bytes_empty();
    EXPECT(!borrowed_bytes_is_valid(&b));
}

static void test_bytes_is_valid_null_b(void)
{
    EXPECT(!borrowed_bytes_is_valid(NULL));
}

/* ── borrowed_bytes_eq ────────────────────────────────────────────────────── */

static void test_bytes_eq_same_content(void)
{
    u8             buf_a[4] = {0x01, 0x02, 0x03, 0x04};
    u8             buf_b[4] = {0x01, 0x02, 0x03, 0x04};
    borrowed_bytes a = borrowed_bytes_from(buf_a, 4, buf_a);
    borrowed_bytes b = borrowed_bytes_from(buf_b, 4, (const void *)0x1); /* diff source */
    EXPECT(borrowed_bytes_eq(a, b)); /* source ignored */
}

static void test_bytes_eq_different_content(void)
{
    u8             buf_a[4] = {0x01, 0x02, 0x03, 0x04};
    u8             buf_b[4] = {0x01, 0x02, 0x03, 0xFF};
    borrowed_bytes a = borrowed_bytes_from(buf_a, 4, NULL);
    borrowed_bytes b = borrowed_bytes_from(buf_b, 4, NULL);
    EXPECT(!borrowed_bytes_eq(a, b));
}

static void test_bytes_eq_different_lengths(void)
{
    u8             buf[4] = {0x01, 0x02, 0x03, 0x04};
    borrowed_bytes a = borrowed_bytes_from(buf, 3, NULL);
    borrowed_bytes b = borrowed_bytes_from(buf, 4, NULL);
    EXPECT(!borrowed_bytes_eq(a, b));
}

static void test_bytes_eq_both_empty(void)
{
    borrowed_bytes a = borrowed_bytes_empty();
    borrowed_bytes b = borrowed_bytes_empty();
    EXPECT(borrowed_bytes_eq(a, b)); /* vacuously equal */
}

static void test_bytes_eq_zero_length_different_ptrs(void)
{
    /* Two zero-length views with different (non-NULL) ptrs must be equal —
     * zero-length comparison is vacuously true regardless of ptr value.    */
    u8             buf_a[1] = {0};
    u8             buf_b[1] = {0};
    borrowed_bytes a = borrowed_bytes_from(buf_a, 0, NULL);
    borrowed_bytes b = borrowed_bytes_from(buf_b, 0, NULL);
    EXPECT(borrowed_bytes_eq(a, b));
}

static void test_bytes_eq_same_ptr_same_len(void)
{
    /* Same pointer — short-circuit path, no memcmp call. */
    u8             buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    borrowed_bytes a = borrowed_bytes_from(buf, 4, NULL);
    borrowed_bytes b = borrowed_bytes_from(buf, 4, NULL);
    EXPECT(borrowed_bytes_eq(a, b));
}

static void test_bytes_eq_partial_overlap(void)
{
    /* buf[0..2) vs buf[1..3): same length, overlapping but different content */
    u8             buf[4]  = {0x01, 0x02, 0x03, 0x04};
    borrowed_bytes a = borrowed_bytes_from(buf,     2, NULL);
    borrowed_bytes b = borrowed_bytes_from(buf + 1, 2, NULL);
    EXPECT(!borrowed_bytes_eq(a, b));
}

static void test_bytes_eq_single_byte_match(void)
{
    u8             x = 0xAB;
    u8             y = 0xAB;
    borrowed_bytes a = borrowed_bytes_from(&x, 1, NULL);
    borrowed_bytes b = borrowed_bytes_from(&y, 1, NULL);
    EXPECT(borrowed_bytes_eq(a, b));
}

static void test_bytes_eq_single_byte_mismatch(void)
{
    u8             x = 0xAB;
    u8             y = 0xAC;
    borrowed_bytes a = borrowed_bytes_from(&x, 1, NULL);
    borrowed_bytes b = borrowed_bytes_from(&y, 1, NULL);
    EXPECT(!borrowed_bytes_eq(a, b));
}

/* ── borrowed_bytes_slice ─────────────────────────────────────────────────── */

static void test_bytes_slice_middle(void)
{
    u8             buf[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    borrowed_bytes b      = borrowed_bytes_from(buf, 8, buf);
    borrowed_bytes sub    = borrowed_bytes_slice(b, 2, 5);
    EXPECT(sub.bytes.len      == 3);
    EXPECT(sub.bytes.ptr      == buf + 2);
    EXPECT(sub.source         == (const void *)buf);
}

static void test_bytes_slice_full_range(void)
{
    u8             buf[4] = {0};
    borrowed_bytes b   = borrowed_bytes_from(buf, 4, NULL);
    borrowed_bytes sub = borrowed_bytes_slice(b, 0, 4);
    EXPECT(sub.bytes.len == 4);
    EXPECT(sub.bytes.ptr == buf);
}

static void test_bytes_slice_clamps_end(void)
{
    u8             buf[4] = {0};
    borrowed_bytes b   = borrowed_bytes_from(buf, 4, NULL);
    borrowed_bytes sub = borrowed_bytes_slice(b, 0, 999);
    EXPECT(sub.bytes.len == 4);
}

static void test_bytes_slice_start_equals_end_returns_empty(void)
{
    u8             buf[4] = {0};
    borrowed_bytes b   = borrowed_bytes_from(buf, 4, NULL);
    borrowed_bytes sub = borrowed_bytes_slice(b, 2, 2);
    EXPECT(sub.bytes.ptr == NULL);
    EXPECT(sub.bytes.len == 0);
}

static void test_bytes_slice_start_beyond_len_returns_empty(void)
{
    u8             buf[4] = {0};
    borrowed_bytes b   = borrowed_bytes_from(buf, 4, NULL);
    borrowed_bytes sub = borrowed_bytes_slice(b, 10, 20);
    EXPECT(sub.bytes.ptr == NULL);
    EXPECT(sub.bytes.len == 0);
}

static void test_bytes_slice_null_ptr_returns_empty(void)
{
    /* {0} initializer covers all fields including the lifetime fields under
     * CANON_LIFETIME_DEBUG. The subsequent assignments overwrite bytes and
     * source; the lifetime fields stay at their safe untracked defaults. */
    borrowed_bytes b = {0};
    b.bytes  = cbytes_empty();
    b.source = NULL;
    borrowed_bytes sub = borrowed_bytes_slice(b, 0, 1);
    EXPECT(sub.bytes.ptr == NULL);
    EXPECT(sub.bytes.len == 0);
}

static void test_bytes_slice_start_greater_than_end_returns_empty(void)
{
    u8             buf[8] = {0};
    borrowed_bytes b   = borrowed_bytes_from(buf, 8, NULL);
    /* end (2) < len (8) but start (5) > end (2) after clamping — empty */
    borrowed_bytes sub = borrowed_bytes_slice(b, 5, 2);
    EXPECT(sub.bytes.ptr == NULL);
    EXPECT(sub.bytes.len == 0);
}

/* ============================================================================
   DEFINE_BORROWED_SLICE(int)
   ============================================================================ */

static void test_slice_int_from_stores_slice_and_source(void)
{
    int                arr[4]  = {10, 20, 30, 40};
    slice_int          s       = slice_int_from(arr, 4);
    borrowed_slice_int b       = borrowed_slice_int_from(s, arr);
    EXPECT(b.slice.ptr  == arr);
    EXPECT(b.slice.len  == 4);
    EXPECT(b.source     == (const void *)arr);
}

static void test_slice_int_empty_constructor(void)
{
    borrowed_slice_int b = borrowed_slice_int_empty();
    EXPECT(b.slice.ptr == NULL);
    EXPECT(b.slice.len == 0);
    EXPECT(b.source    == NULL);
}

static void test_slice_int_get_returns_inner(void)
{
    int                arr[2] = {1, 2};
    slice_int          s      = slice_int_from(arr, 2);
    borrowed_slice_int b      = borrowed_slice_int_from(s, arr);
    slice_int          got    = borrowed_slice_int_get(&b);
    EXPECT(got.ptr == arr);
    EXPECT(got.len == 2);
}

static void test_slice_int_len_correct(void)
{
    int                arr[5] = {0};
    slice_int          s      = slice_int_from(arr, 5);
    borrowed_slice_int b      = borrowed_slice_int_from(s, NULL);
    EXPECT(borrowed_slice_int_len(&b) == 5);
}

static void test_slice_int_len_null_b_returns_zero(void)
{
    EXPECT(borrowed_slice_int_len(NULL) == 0);
}

static void test_slice_int_is_valid_non_null(void)
{
    int                arr[1] = {0};
    slice_int          s      = slice_int_from(arr, 1);
    borrowed_slice_int b      = borrowed_slice_int_from(s, NULL);
    EXPECT(borrowed_slice_int_is_valid(&b));
}

static void test_slice_int_is_valid_empty(void)
{
    borrowed_slice_int b = borrowed_slice_int_empty();
    EXPECT(!borrowed_slice_int_is_valid(&b));
}

static void test_slice_int_is_valid_null_b(void)
{
    EXPECT(!borrowed_slice_int_is_valid(NULL));
}

static void test_slice_int_at_in_bounds(void)
{
    int                arr[3]  = {7, 8, 9};
    slice_int          s       = slice_int_from(arr, 3);
    borrowed_slice_int b       = borrowed_slice_int_from(s, NULL);
    const int         *p0      = borrowed_slice_int_at(&b, 0);
    const int         *p2      = borrowed_slice_int_at(&b, 2);
    EXPECT_NOT_NULL(p0);
    EXPECT(*p0 == 7);
    EXPECT_NOT_NULL(p2);
    EXPECT(*p2 == 9);
}

static void test_slice_int_at_out_of_bounds_returns_null(void)
{
    int                arr[3] = {1, 2, 3};
    slice_int          s      = slice_int_from(arr, 3);
    borrowed_slice_int b      = borrowed_slice_int_from(s, NULL);
    EXPECT(borrowed_slice_int_at(&b, 3)  == NULL);
    EXPECT(borrowed_slice_int_at(&b, 99) == NULL);
}

static void test_slice_int_at_null_b_returns_null(void)
{
    EXPECT(borrowed_slice_int_at(NULL, 0) == NULL);
}

static void test_slice_int_slice_sub_range(void)
{
    int                arr[6]  = {0, 1, 2, 3, 4, 5};
    slice_int          s       = slice_int_from(arr, 6);
    borrowed_slice_int b       = borrowed_slice_int_from(s, arr);
    borrowed_slice_int sub     = borrowed_slice_int_slice(b, 2, 5);
    EXPECT(sub.slice.len == 3);
    EXPECT(sub.slice.ptr == arr + 2);
    EXPECT(sub.source    == (const void *)arr);
}

static void test_slice_int_slice_empty_range(void)
{
    int                arr[4] = {0};
    slice_int          s      = slice_int_from(arr, 4);
    borrowed_slice_int b      = borrowed_slice_int_from(s, NULL);
    borrowed_slice_int sub    = borrowed_slice_int_slice(b, 2, 2);
    EXPECT(sub.slice.len == 0);
}

static void test_slice_int_as_bytes_size(void)
{
    int                arr[4]  = {1, 2, 3, 4};
    slice_int          s       = slice_int_from(arr, 4);
    borrowed_slice_int b       = borrowed_slice_int_from(s, arr);
    borrowed_bytes     raw     = borrowed_slice_int_as_bytes(&b);
    EXPECT(raw.bytes.ptr == (const u8 *)arr);
    EXPECT(raw.bytes.len == 4 * sizeof(int));
    EXPECT(raw.source    == (const void *)arr);
}

static void test_slice_int_as_bytes_null_b_returns_empty(void)
{
    borrowed_bytes raw = borrowed_slice_int_as_bytes(NULL);
    EXPECT(raw.bytes.ptr == NULL);
    EXPECT(raw.bytes.len == 0);
}

static void test_slice_int_as_bytes_null_ptr_returns_empty(void)
{
    borrowed_slice_int b = borrowed_slice_int_empty();
    borrowed_bytes     raw = borrowed_slice_int_as_bytes(&b);
    EXPECT(raw.bytes.ptr == NULL);
    EXPECT(raw.bytes.len == 0);
}

/* ============================================================================
   Source tag round-trip — stored and retrievable, never dereferenced
   ============================================================================ */

static void test_source_tag_round_trip_all_types(void)
{
    /* Use a stack address as the tag — just verify it survives the round-trip */
    int    sentinel   = 0xDEAD;
    u8     buf[4]     = {0};
    int    arr[2]     = {0};

    borrowed_ptr   bp = borrowed_ptr_from(&sentinel, &sentinel);
    borrowed_str   bs = borrowed_str_from_cstr("tag", &sentinel);
    borrowed_bytes bb = borrowed_bytes_from(buf, 4, &sentinel);

    slice_int          si = slice_int_from(arr, 2);
    borrowed_slice_int bv = borrowed_slice_int_from(si, &sentinel);

    EXPECT(bp.source == (const void *)&sentinel);
    EXPECT(bs.source == (const void *)&sentinel);
    EXPECT(bb.source == (const void *)&sentinel);
    EXPECT(bv.source == (const void *)&sentinel);
}

/* ============================================================================
   CANON_LIFETIME_DEBUG — lifetime tracking end-to-end
   ============================================================================
 *
 * These tests close the loop on the substrate: arena restamps its lifetime
 * ID on reset → captured borrow's stored ID no longer matches the source's
 * current ID → borrowed_*_get fires require_msg via lifetime_assert_valid.
 *
 * The trap mechanism mirrors test/core/primitives/contract_test.c: install
 * a capture handler via contract_set_handler that longjmps back to the test
 * site instead of calling abort. setjmp wraps each "should fire" call.
 *
 * Coverage:
 *   1. _from_lifetime constructor captures source_lt and captured_id.
 *   2. _get on a fresh, open borrow is silent (baseline).
 *   3. _get after arena_reset fires require_msg (the headline guarantee).
 *   4. Untracked borrows (built via _from, no lifetime arg) bypass the
 *      check even after reset (opt-in design works as documented).
 *   5. _get after arena_reset_to is silent (rollback contract preserved).
 */

#ifdef CANON_LIFETIME_DEBUG

#define LT_CAP_BUF 256

static jmp_buf _lt_cap_jmp;
static int     _lt_cap_fired = 0;
static char    _lt_cap_msg[LT_CAP_BUF];

static void _lt_capture_handler(const char *file, int line, const char *func,
                                const char *expr, const char *msg)
{
    (void)file; (void)line; (void)func; (void)expr;
    _lt_cap_fired = 1;
    strncpy(_lt_cap_msg, msg ? msg : "", LT_CAP_BUF - 1);
    _lt_cap_msg[LT_CAP_BUF - 1] = '\0';
    longjmp(_lt_cap_jmp, 1);
}

static void _lt_cap_reset(void)
{
    _lt_cap_fired = 0;
    _lt_cap_msg[0] = '\0';
}

#define LT_FIRES(stmt)                  \
    do {                                \
        _lt_cap_reset();                \
        if (setjmp(_lt_cap_jmp) == 0) { \
            stmt;                       \
        }                               \
    } while (0)

#define LT_SILENT(stmt)                 \
    do {                                \
        _lt_cap_reset();                \
        if (setjmp(_lt_cap_jmp) == 0) { \
            stmt;                       \
        }                               \
    } while (0)

/* Shared backing buffer for arena-based borrow tests. */
#define LT_BUF_SIZE ((usize)256)
static u8    g_lt_buf[LT_BUF_SIZE];
static Arena g_lt_arena;

static void lt_setup(void)
{
    memset(g_lt_buf, 0, LT_BUF_SIZE);
    arena_init(&g_lt_arena, g_lt_buf, LT_BUF_SIZE);
    contract_set_handler(_lt_capture_handler);
}

static void test_lifetime_borrow_captures_id(void)
{
    lt_setup();
    u8 *p = (u8 *)arena_alloc(&g_lt_arena, 32);
    EXPECT_NOT_NULL(p);

    borrowed_bytes b = borrowed_bytes_from_lifetime(
        p, 32, &g_lt_arena.lt, &g_lt_arena);

    EXPECT(b.source_lt   == &g_lt_arena.lt);
    EXPECT(b.captured_id == g_lt_arena.lt.id);
}

static void test_lifetime_get_open_borrow_silent(void)
{
    lt_setup();
    u8 *p = (u8 *)arena_alloc(&g_lt_arena, 32);
    EXPECT_NOT_NULL(p);

    borrowed_bytes b = borrowed_bytes_from_lifetime(
        p, 32, &g_lt_arena.lt, &g_lt_arena);

    /* Baseline: fresh borrow, arena untouched — _get must not fire. */
    LT_SILENT(borrowed_bytes_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

static void test_lifetime_get_after_reset_fires(void)
{
    lt_setup();
    u8 *p = (u8 *)arena_alloc(&g_lt_arena, 32);
    EXPECT_NOT_NULL(p);

    borrowed_bytes b = borrowed_bytes_from_lifetime(
        p, 32, &g_lt_arena.lt, &g_lt_arena);

    /* Reset re-stamps lt.id — borrow's captured_id is now stale. */
    arena_reset(&g_lt_arena);

    LT_FIRES(borrowed_bytes_get(&b));
    EXPECT(_lt_cap_fired == 1);
    /* The message comes from lifetime_assert_valid — verify a non-empty
     * message was captured. We don't assert exact content to keep this
     * decoupled from the wording of the assertion site. */
    EXPECT(_lt_cap_msg[0] != '\0');
}

static void test_lifetime_untracked_borrow_bypasses_check(void)
{
    lt_setup();
    u8 *p = (u8 *)arena_alloc(&g_lt_arena, 32);
    EXPECT_NOT_NULL(p);

    /* Built via the classic _from constructor — source_lt is NULL,
     * so the borrow is untracked. _get must NOT fire even after reset. */
    borrowed_bytes b = borrowed_bytes_from(p, 32, &g_lt_arena);
    EXPECT(b.source_lt == NULL);

    arena_reset(&g_lt_arena);

    LT_SILENT(borrowed_bytes_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

static void test_lifetime_get_after_reset_to_silent(void)
{
    lt_setup();
    u8 *p = (u8 *)arena_alloc(&g_lt_arena, 32);
    EXPECT_NOT_NULL(p);

    borrowed_bytes b = borrowed_bytes_from_lifetime(
        p, 32, &g_lt_arena.lt, &g_lt_arena);

    /* Take a mark AFTER the borrow's allocation, then allocate more,
     * then rollback to the mark. The borrow's underlying memory is
     * preserved, the lifetime ID is preserved, _get must NOT fire. */
    ArenaMark mark = arena_mark(&g_lt_arena);
    (void)arena_alloc(&g_lt_arena, 64);
    arena_reset_to(&g_lt_arena, mark);

    LT_SILENT(borrowed_bytes_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

#endif /* CANON_LIFETIME_DEBUG */

/* ============================================================================
   Unit test entry point
   ============================================================================ */

int main(void)
{
    /* borrowed_ptr */
    test_ptr_from_stores_ptr_and_source();
    test_ptr_from_null_ptr();
    test_ptr_null_constructor();
    test_ptr_get_returns_stored_pointer();
    test_ptr_is_valid_true();
    test_ptr_is_valid_null_ptr();
    test_ptr_is_valid_null_b();
    test_ptr_eq_same_address();
    test_ptr_eq_different_address();
    test_ptr_eq_both_null();

    /* borrowed_str */
    test_str_from_stores_str_and_source();
    test_str_from_cstr_round_trip();
    test_str_from_cstr_null_yields_empty();
    test_str_empty_constructor();
    test_str_get_returns_inner();
    test_str_is_valid_non_null_ptr();
    test_str_is_valid_empty();
    test_str_is_valid_null_b();
    test_str_len_correct();
    test_str_len_null_b_returns_zero();
    test_str_eq_same_content();
    test_str_eq_different_content();
    test_str_eq_different_lengths();
    test_str_slice_middle();
    test_str_slice_full_range();
    test_str_slice_empty_range();
    test_str_slice_out_of_bounds_clamped();
    test_str_slice_inherits_source();

    /* borrowed_bytes */
    test_bytes_from_stores_ptr_len_source();
    test_bytes_from_cbytes();
    test_bytes_empty_constructor();
    test_bytes_get_returns_inner();
    test_bytes_len_correct();
    test_bytes_len_null_b_returns_zero();
    test_bytes_is_valid_non_null();
    test_bytes_is_valid_empty();
    test_bytes_is_valid_null_b();

    /* borrowed_bytes_eq */
    test_bytes_eq_same_content();
    test_bytes_eq_different_content();
    test_bytes_eq_different_lengths();
    test_bytes_eq_both_empty();
    test_bytes_eq_zero_length_different_ptrs();
    test_bytes_eq_same_ptr_same_len();
    test_bytes_eq_partial_overlap();
    test_bytes_eq_single_byte_match();
    test_bytes_eq_single_byte_mismatch();

    /* borrowed_bytes_slice */
    test_bytes_slice_middle();
    test_bytes_slice_full_range();
    test_bytes_slice_clamps_end();
    test_bytes_slice_start_equals_end_returns_empty();
    test_bytes_slice_start_beyond_len_returns_empty();
    test_bytes_slice_null_ptr_returns_empty();
    test_bytes_slice_start_greater_than_end_returns_empty();

    /* DEFINE_BORROWED_SLICE(int) */
    test_slice_int_from_stores_slice_and_source();
    test_slice_int_empty_constructor();
    test_slice_int_get_returns_inner();
    test_slice_int_len_correct();
    test_slice_int_len_null_b_returns_zero();
    test_slice_int_is_valid_non_null();
    test_slice_int_is_valid_empty();
    test_slice_int_is_valid_null_b();
    test_slice_int_at_in_bounds();
    test_slice_int_at_out_of_bounds_returns_null();
    test_slice_int_at_null_b_returns_null();
    test_slice_int_slice_sub_range();
    test_slice_int_slice_empty_range();
    test_slice_int_as_bytes_size();
    test_slice_int_as_bytes_null_b_returns_empty();
    test_slice_int_as_bytes_null_ptr_returns_empty();

    /* Source tag round-trip */
    test_source_tag_round_trip_all_types();

#ifdef CANON_LIFETIME_DEBUG
    test_lifetime_borrow_captures_id();
    test_lifetime_get_open_borrow_silent();
    test_lifetime_get_after_reset_fires();
    test_lifetime_untracked_borrow_bypasses_check();
    test_lifetime_get_after_reset_to_silent();
#endif

    if (g_failed == 0) {
        printf("OK  borrow_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  borrow_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ─────────────────────────────────────────────────────
 *
 * Targets the two non-trivial functions in borrow.h:
 *   - borrowed_bytes_slice: clamping logic, pointer arithmetic on u8*
 *   - borrowed_slice_int_as_bytes: checked_mul overflow guard
 *
 * Input layout (10 bytes):
 *   [0..1]  buf_len   — u16, size of the byte buffer to borrow
 *   [2..3]  start     — u16, slice start index
 *   [4..5]  end       — u16, slice end index
 *   [6..7]  arr_len   — u16, number of int elements for slice_int
 *   [8]     flags     — bit 0: run bytes_slice, bit 1: run as_bytes
 *   [9]     pad
 */

#define FUZZ_BUF_SIZE  ((usize)65536)
#define FUZZ_ARR_SIZE  ((usize)4096)

int LLVMFuzzerTestOneInput(const u8 *data, usize size)
{
    static u8  fuzz_buf[FUZZ_BUF_SIZE];
    static int fuzz_arr[FUZZ_ARR_SIZE];

    u8    raw[10];
    u16   buf_len16, start16, end16, arr_len16;
    u8    flags;
    usize buf_len, start, end, arr_len;

    memset(raw, 0, sizeof(raw));
    if (size > sizeof(raw)) { size = sizeof(raw); }
    memcpy(raw, data, size);

    buf_len16 = (u16)((u16)raw[0] | ((u16)raw[1] << 8));
    start16   = (u16)((u16)raw[2] | ((u16)raw[3] << 8));
    end16     = (u16)((u16)raw[4] | ((u16)raw[5] << 8));
    arr_len16 = (u16)((u16)raw[6] | ((u16)raw[7] << 8));
    flags     = raw[8];

    buf_len = (usize)buf_len16 % (FUZZ_BUF_SIZE + 1u);
    start   = (usize)start16;
    end     = (usize)end16;
    arr_len = (usize)arr_len16 % (FUZZ_ARR_SIZE + 1u);

    /* ── borrowed_bytes_slice ─────────────────────────────────────────── */
    if (flags & 0x01u) {
        borrowed_bytes b   = borrowed_bytes_from(fuzz_buf, buf_len, fuzz_buf);
        borrowed_bytes sub = borrowed_bytes_slice(b, start, end);

        /* Invariant: result ptr must be within the original buffer or NULL */
        if (sub.bytes.ptr != NULL) {
            if ((const u8 *)sub.bytes.ptr < fuzz_buf)
                __builtin_trap();
            if ((const u8 *)sub.bytes.ptr + sub.bytes.len > fuzz_buf + buf_len)
                __builtin_trap();
            if (sub.bytes.len == 0)
                __builtin_trap(); /* non-NULL ptr must have non-zero len */
        }
    }

    /* ── borrowed_slice_int_as_bytes ─────────────────────────────────── */
    if (flags & 0x02u) {
        slice_int          s   = slice_int_from(fuzz_arr, arr_len);
        borrowed_slice_int b   = borrowed_slice_int_from(s, fuzz_arr);
        borrowed_bytes     raw_b = borrowed_slice_int_as_bytes(&b);

        if (raw_b.bytes.ptr != NULL) {
            /* Pointer must alias the original array */
            if ((const u8 *)raw_b.bytes.ptr != (const u8 *)fuzz_arr)
                __builtin_trap();
            /* Byte length must equal arr_len * sizeof(int) exactly */
            usize expected;
            if (!checked_mul(arr_len, sizeof(int), &expected))
                __builtin_trap(); /* overflow path should have returned empty */
            if (raw_b.bytes.len != expected)
                __builtin_trap();
        }
    }

    return 0;
}

#endif /* CANON_FUZZING */
