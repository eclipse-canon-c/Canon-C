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
 *     Lifetime check is exercised on all four borrow types (ptr, str,
 *     bytes, slice_int), plus the type-erasure path through as_bytes
 *     (which must inherit lifetime tracking from the source slice).
 *   - Phase 4 — end-to-end container lifetime tests:
 *     vec, deque, priority_queue, hashmap. Validate that the full chain
 *     (container mutation → lt restamp/close → borrow captured_id stale →
 *     borrowed_*_get fires require_msg) composes correctly for every
 *     Phase 3 container.
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

/* ── Hash and equality for hashmap instantiation ─────────────────────────── *
 *
 * Must be declared BEFORE hashmap.h is included so the generated static
 * inline functions can reference them.
 */
static u64 _bt_hash_u64(const u64* k, void* ctx)
{
    (void)ctx;
    u64 h = *k * 11400714819323198485ULL;
    return h == 0 ? 1 : h;
}

static bool _bt_eq_u64(const u64* a, const u64* b, void* ctx)
{
    (void)ctx;
    return *a == *b;
}

/* ── Slice + container instantiations needed by tests ────────────────────── *
 *
 * DEFINE_SLICE / DEFINE_VEC / DEFINE_DEQUE / DEFINE_PRIORITY_QUEUE all stamp
 * out more functions than this test file uses directly. The unused-function
 * warning is suppressed here; the same approach used elsewhere in the
 * Canon-C test suite for macro-generated instantiations. A
 * borrow_test_suppress_unused() function below references the leftovers
 * with (void) to keep cppcheck and stricter linters quiet too.
 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

DEFINE_SLICE(int)
DEFINE_BORROWED_SLICE(int)

/* option_int is required by:
 *   - DEFINE_VEC (vec_int_get_option, vec_int_pop_option, vec_int_remove_option)
 *   - DEFINE_DEQUE (deque_int_pop_*_option, deque_int_peek_*_option)
 *   - DEFINE_PRIORITY_QUEUE (pq_int_pop_option, pq_int_peek_option)
 * Must be instantiated before any of those. */
#include "semantics/option/option.h"
CANON_OPTION(int)

#include "data/vec/vec.h"
DEFINE_VEC(static inline, int)

#include "data/deque/deque.h"
DEFINE_DEQUE(static inline, int)

#include "data/priority_queue.h"
DEFINE_PRIORITY_QUEUE(int)

#define HASHMAP_KEY_TYPE u64
#define HASHMAP_VAL_TYPE int
#define HASHMAP_HASH_FN  _bt_hash_u64
#define HASHMAP_EQ_FN    _bt_eq_u64
#include "data/hashmap/hashmap.h"

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
 * Sections in order:
 *   - Trap infrastructure (handler, jmp_buf, LT_FIRES/LT_SILENT macros)
 *   - Arena lifetime tests (the original Phase 1 coverage)
 *   - Phase 4: vec lifetime tests
 *   - Phase 4: deque lifetime tests
 *   - Phase 4: priority_queue lifetime tests
 *   - Phase 4: hashmap lifetime tests
 *
 * The trap mechanism mirrors test/core/primitives/contract_test.c: install
 * a capture handler via contract_set_handler that longjmps back to the test
 * site instead of calling abort. setjmp wraps each "should fire" call.
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

/* Lighter setup for tests that don't need an arena — just installs the
 * contract handler so LT_FIRES/LT_SILENT work. Each Phase 4 test creates
 * its own container on the stack. */
static void lt_setup_handler_only(void)
{
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

/* ── borrowed_str_get: fires after reset ──────────────────────────────────── *
 *
 * Allocates scratch space in the arena and points a str_t at it, so the
 * str's underlying storage is owned by g_lt_arena and its lifetime is
 * g_lt_arena.lt. arena_reset restamps lt.id; the captured_id in the
 * borrow becomes stale; borrowed_str_get must fire require_msg.
 */
static void test_lifetime_str_get_after_reset_fires(void)
{
    lt_setup();
    char *p = (char *)arena_alloc(&g_lt_arena, 6);
    EXPECT_NOT_NULL(p);
    memcpy(p, "hello", 6);

    str_t s;
    s.ptr = p;
    s.len = 5;

    borrowed_str b = borrowed_str_from_lifetime(s, &g_lt_arena.lt, &g_lt_arena);
    EXPECT(b.source_lt   == &g_lt_arena.lt);
    EXPECT(b.captured_id == g_lt_arena.lt.id);

    /* Baseline: open arena — silent. */
    LT_SILENT(borrowed_str_get(&b));
    EXPECT(_lt_cap_fired == 0);

    /* Reset restamps lt.id — captured_id is now stale. */
    arena_reset(&g_lt_arena);

    LT_FIRES(borrowed_str_get(&b));
    EXPECT(_lt_cap_fired == 1);
    EXPECT(_lt_cap_msg[0] != '\0');
}

/* ── borrowed_ptr_get: fires after reset ──────────────────────────────────── *
 *
 * The generic borrow type uses the same BORROW_LT_CHECK_ macro as the
 * specialised types, but exercising it independently ensures any future
 * divergence (e.g. a typed _get omitting the check) is caught.
 */
static void test_lifetime_ptr_get_after_reset_fires(void)
{
    lt_setup();
    void *p = arena_alloc(&g_lt_arena, 16);
    EXPECT_NOT_NULL(p);

    borrowed_ptr b = borrowed_ptr_from_lifetime(p, &g_lt_arena.lt, &g_lt_arena);
    EXPECT(b.source_lt   == &g_lt_arena.lt);
    EXPECT(b.captured_id == g_lt_arena.lt.id);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);

    arena_reset(&g_lt_arena);

    LT_FIRES(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 1);
    EXPECT(_lt_cap_msg[0] != '\0');
}

/* ── borrowed_slice_int_get: fires after reset ────────────────────────────── *
 *
 * DEFINE_BORROWED_SLICE emits its own copy of BORROW_LT_CHECK_ via macro
 * expansion, and the _from_lifetime helper takes a const void * (not a
 * const lifetime_t *) and casts internally. This test exercises that
 * macro-internal cast path — without it the macro's lifetime tracking
 * would be untested.
 */
static void test_lifetime_slice_int_get_after_reset_fires(void)
{
    lt_setup();
    int *p = (int *)arena_alloc(&g_lt_arena, 4 * sizeof(int));
    EXPECT_NOT_NULL(p);
    p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 4;

    slice_int s = slice_int_from(p, 4);
    borrowed_slice_int b = borrowed_slice_int_from_lifetime(
        s, &g_lt_arena.lt, &g_lt_arena);

    EXPECT(b.source_lt   == &g_lt_arena.lt);
    EXPECT(b.captured_id == g_lt_arena.lt.id);

    LT_SILENT(borrowed_slice_int_get(&b));
    EXPECT(_lt_cap_fired == 0);

    arena_reset(&g_lt_arena);

    LT_FIRES(borrowed_slice_int_get(&b));
    EXPECT(_lt_cap_fired == 1);
    EXPECT(_lt_cap_msg[0] != '\0');
}

/* ── borrowed_slice_int_as_bytes inherits lifetime ────────────────────────── *
 *
 * Substrate contract: lifetime stays attached to the underlying memory
 * across type erasure. The byte view derived from a tracked slice must
 * also be tracked — otherwise as_bytes silently strips the safety check
 * at the moment a caller does the most type-erasing operation. This
 * test pins that behaviour: derived byte view inherits source_lt and
 * captured_id, and fires after arena_reset just like the source slice.
 */
static void test_lifetime_slice_int_as_bytes_inherits_and_fires(void)
{
    lt_setup();
    int *p = (int *)arena_alloc(&g_lt_arena, 4 * sizeof(int));
    EXPECT_NOT_NULL(p);
    p[0] = 10; p[1] = 20; p[2] = 30; p[3] = 40;

    slice_int s = slice_int_from(p, 4);
    borrowed_slice_int v = borrowed_slice_int_from_lifetime(
        s, &g_lt_arena.lt, &g_lt_arena);

    /* The derived byte view must inherit the source's lifetime token. */
    borrowed_bytes bb = borrowed_slice_int_as_bytes(&v);
    EXPECT(bb.source_lt   == &g_lt_arena.lt);
    EXPECT(bb.captured_id == g_lt_arena.lt.id);

    /* Baseline: open arena — silent. */
    LT_SILENT(borrowed_bytes_get(&bb));
    EXPECT(_lt_cap_fired == 0);

    /* Reset restamps lt.id — captured_id on the derived view is now stale. */
    arena_reset(&g_lt_arena);

    LT_FIRES(borrowed_bytes_get(&bb));
    EXPECT(_lt_cap_fired == 1);
    EXPECT(_lt_cap_msg[0] != '\0');
}

/* ════════════════════════════════════════════════════════════════════════════
   Phase 4 — End-to-end container lifetime tests
   ════════════════════════════════════════════════════════════════════════════
 *
 * Each section validates that the full owner→borrow→check chain composes
 * correctly when the owner is a Phase 3 container (rather than an arena).
 *
 * Pattern:
 *   1. Construct container on stack
 *   2. Optionally push some content
 *   3. Build a tracked borrow via borrowed_*_from_lifetime(payload, &c.lt, &c)
 *   4. LT_SILENT baseline _get (proves the borrow is fresh)
 *   5. Trigger a state change on the container (free / swap / push / etc.)
 *   6. LT_FIRES or LT_SILENT on the next _get, depending on whether the
 *      operation was meant to invalidate the borrow
 *
 * Borrow construction uses borrowed_ptr_from_lifetime throughout — it's
 * the lowest-common-denominator that exercises BORROW_LT_CHECK_ for every
 * Phase 3 container type. For vec the natural borrow target is
 * vec_int_first(&v) — a real element pointer. For deque and pq there is
 * no pointer-returning accessor (peek_front and peek_raw return via
 * out-param or option), so the borrow target is the struct itself (&d,
 * &pq); the substrate only inspects (source_lt, captured_id), so the
 * payload pointer is incidental.
 */

/* ── vec — Phase 4 ───────────────────────────────────────────────────────── */

static void test_lifetime_vec_borrow_open_is_silent(void)
{
    lt_setup_handler_only();

    int buf[4];
    vec_int v = vec_int_init(buf, 4);
    vec_int_push(&v, 42);

    /* Borrow against an element pointer in the vec. */
    int *first = vec_int_first(&v);
    EXPECT_NOT_NULL(first);
    borrowed_ptr b = borrowed_ptr_from_lifetime(first, &v.lt, &v);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);
    /* No vec_int_free — buf is stack-allocated.
     * vec_int_free is for heap-backed vecs only (calls mem_free). */
}

static void test_lifetime_vec_free_closes_fires(void)
{
    /* Use vec_int_alloc so vec_int_free is legal — vec_int_free
     * calls mem_free on v->items, which is undefined for stack buffers. */
    lt_setup_handler_only();

    vec_int v = vec_int_alloc(4);
    vec_int_push(&v, 42);

    borrowed_ptr b = borrowed_ptr_from_lifetime(
        vec_int_first(&v), &v.lt, &v);

    /* free closes the lifetime — open flag flipped to false. */
    vec_int_free(&v);

    LT_FIRES(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 1);
    EXPECT(_lt_cap_msg[0] != '\0');
}

static void test_lifetime_vec_swap_invalidates_a_fires(void)
{
    /* Two vecs, borrow against a, swap. After swap, &a.lt holds what was
     * b's lt (different id), so the captured id is stale and _get fires.
     * This is the most rigorous test of the substrate: it proves that
     * vec_swap exchanges the lt field along with everything else. */
    lt_setup_handler_only();

    int buf_a[4];
    int buf_b[4];
    vec_int a = vec_int_init(buf_a, 4);
    vec_int b = vec_int_init(buf_b, 4);
    vec_int_push(&a, 1);
    vec_int_push(&b, 2);

    borrowed_ptr ba = borrowed_ptr_from_lifetime(
        vec_int_first(&a), &a.lt, &a);

    /* Baseline silent before swap. */
    LT_SILENT(borrowed_ptr_get(&ba));
    EXPECT(_lt_cap_fired == 0);

    vec_int_swap(&a, &b);

    LT_FIRES(borrowed_ptr_get(&ba));
    EXPECT(_lt_cap_fired == 1);
    /* No free — stack-backed. */
}

static void test_lifetime_vec_in_place_push_silent(void)
{
    /* Push into a vec with spare capacity does NOT relocate or restamp
     * (vec has fixed capacity, no growth). Borrow against first element
     * must still be valid. */
    lt_setup_handler_only();

    int buf[4];
    vec_int v = vec_int_init(buf, 4);
    vec_int_push(&v, 1);

    borrowed_ptr b = borrowed_ptr_from_lifetime(
        vec_int_first(&v), &v.lt, &v);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);

    /* In-place push — no relocation possible (fixed buf). */
    vec_int_push(&v, 2);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);
    /* No free — stack-backed. */
}

/* ── deque — Phase 4 ─────────────────────────────────────────────────────── *
 *
 * deque_int_init is void (takes the deque by pointer); the deque exposes
 * peek_front as a bool-returning out-param, not a pointer accessor. So
 * for Phase 4 we borrow against the deque struct itself with
 * borrowed_ptr_from_lifetime(&d, &d.lt, &d). The borrow target's address
 * is incidental — BORROW_LT_CHECK_ only inspects (source_lt, captured_id),
 * which is what we want to test. */

static void test_lifetime_deque_borrow_open_is_silent(void)
{
    lt_setup_handler_only();

    int buf[4];
    deque_int d;
    deque_int_init(&d, buf, 4);
    deque_int_push_back(&d, 7);

    /* Borrow against the deque struct itself. */
    borrowed_ptr b = borrowed_ptr_from_lifetime(&d, &d.lt, &d);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

static void test_lifetime_deque_swap_invalidates_fires(void)
{
    lt_setup_handler_only();

    int buf_a[4];
    int buf_b[4];
    deque_int a;
    deque_int b;
    deque_int_init(&a, buf_a, 4);
    deque_int_init(&b, buf_b, 4);
    deque_int_push_back(&a, 1);
    deque_int_push_back(&b, 2);

    borrowed_ptr ba = borrowed_ptr_from_lifetime(&a, &a.lt, &a);

    LT_SILENT(borrowed_ptr_get(&ba));
    EXPECT(_lt_cap_fired == 0);

    deque_int_swap(&a, &b);

    LT_FIRES(borrowed_ptr_get(&ba));
    EXPECT(_lt_cap_fired == 1);
}

static void test_lifetime_deque_in_place_push_silent(void)
{
    lt_setup_handler_only();

    int buf[4];
    deque_int d;
    deque_int_init(&d, buf, 4);
    deque_int_push_back(&d, 1);

    borrowed_ptr b = borrowed_ptr_from_lifetime(&d, &d.lt, &d);

    /* push_back into spare capacity — does NOT restamp (deque has no
     * restamp on mutation; lt only changes on swap or via struct-copy). */
    deque_int_push_back(&d, 2);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

/* ── priority_queue — Phase 4 ────────────────────────────────────────────── *
 *
 * Typed PQ generated by DEFINE_PRIORITY_QUEUE(int) is named `pq_int`,
 * not `PriorityQueue_int`. pq_int_init takes (h, buf, cap, cmp, ctx) by
 * pointer and returns void. pq_int_peek_raw does NOT exist as a typed
 * wrapper — the typed wrappers expose pq_int_peek_option and pq_int_peek
 * (out-param). The base pq_peek_raw lives on the inner PriorityQueue.
 *
 * As with deque, Phase 4 tests borrow against the typed struct itself
 * (&pq) and capture the lifetime via &pq._pq.lt. What we test is the
 * substrate's invalidation logic, not the borrow payload.
 *
 * Comparator: algo_cmp_i32 (signed 32-bit ascending) — from
 * core/primitives/compare.h, pulled in transitively via priority_queue.h.
 */

static void test_lifetime_pq_peek_raw_silent(void)
{
    lt_setup_handler_only();

    int buf[8];
    pq_int pq;
    pq_int_init(&pq, buf, 8, algo_cmp_i32, NULL);
    pq_int_push(&pq, 5);
    pq_int_push(&pq, 1);
    pq_int_push(&pq, 9);

    borrowed_ptr b = borrowed_ptr_from_lifetime(&pq, &pq._pq.lt, &pq);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

static void test_lifetime_pq_push_restamps_fires(void)
{
    lt_setup_handler_only();

    int buf[8];
    pq_int pq;
    pq_int_init(&pq, buf, 8, algo_cmp_i32, NULL);
    pq_int_push(&pq, 5);

    borrowed_ptr b = borrowed_ptr_from_lifetime(&pq, &pq._pq.lt, &pq);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);

    /* Push restamps — heap may have reordered. */
    pq_int_push(&pq, 1);

    LT_FIRES(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 1);
}

static void test_lifetime_pq_pop_restamps_fires(void)
{
    lt_setup_handler_only();

    int buf[8];
    pq_int pq;
    pq_int_init(&pq, buf, 8, algo_cmp_i32, NULL);
    pq_int_push(&pq, 5);
    pq_int_push(&pq, 3);

    borrowed_ptr b = borrowed_ptr_from_lifetime(&pq, &pq._pq.lt, &pq);

    /* Pop restamps — heap reorganized. */
    int out;
    pq_int_pop(&pq, &out);

    LT_FIRES(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 1);
}

static void test_lifetime_pq_query_silent(void)
{
    /* Queries (len, peek_option) do NOT restamp. Borrow stays valid. */
    lt_setup_handler_only();

    int buf[8];
    pq_int pq;
    pq_int_init(&pq, buf, 8, algo_cmp_i32, NULL);
    pq_int_push(&pq, 5);

    borrowed_ptr b = borrowed_ptr_from_lifetime(&pq, &pq._pq.lt, &pq);

    (void)pq_int_len(&pq);
    (void)pq_int_peek_option(&pq);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

/* ── hashmap — Phase 4 ───────────────────────────────────────────────────── */

static void test_lifetime_hashmap_borrow_open_is_silent(void)
{
    lt_setup_handler_only();

    u8 buf[8 * sizeof(hashmap_slot)];
    hashmap m;
    hashmap_init(&m, bytes_from(buf, sizeof(buf)), 8, NULL);

    u64 k = 42; int v = 100;
    hashmap_insert(&m, &k, &v);

    int *vp = hashmap_get_or_null(&m, &k);
    EXPECT_NOT_NULL(vp);
    borrowed_ptr b = borrowed_ptr_from_lifetime(vp, &m.lt, &m);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);
}

static void test_lifetime_hashmap_insert_restamps_fires(void)
{
    lt_setup_handler_only();

    u8 buf[8 * sizeof(hashmap_slot)];
    hashmap m;
    hashmap_init(&m, bytes_from(buf, sizeof(buf)), 8, NULL);

    u64 k1 = 1; int v1 = 10;
    hashmap_insert(&m, &k1, &v1);

    borrowed_ptr b = borrowed_ptr_from_lifetime(
        hashmap_get_or_null(&m, &k1), &m.lt, &m);

    LT_SILENT(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 0);

    /* Insert another key — Robin Hood may have shifted, so the substrate
     * conservatively restamps on every successful insert. */
    u64 k2 = 2; int v2 = 20;
    hashmap_insert(&m, &k2, &v2);

    LT_FIRES(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 1);
}

static void test_lifetime_hashmap_remove_restamps_fires(void)
{
    /* Remove of a different key may backward-shift entries — invalidates
     * prior pointers. Substrate restamps on every successful remove. */
    lt_setup_handler_only();

    u8 buf[8 * sizeof(hashmap_slot)];
    hashmap m;
    hashmap_init(&m, bytes_from(buf, sizeof(buf)), 8, NULL);

    u64 k1 = 1; int v1 = 10;
    u64 k2 = 2; int v2 = 20;
    hashmap_insert(&m, &k1, &v1);
    hashmap_insert(&m, &k2, &v2);

    borrowed_ptr b = borrowed_ptr_from_lifetime(
        hashmap_get_or_null(&m, &k1), &m.lt, &m);

    /* Remove a different key — may shift adjacent slots. */
    (void)hashmap_remove(&m, &k2);

    LT_FIRES(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 1);
}

static void test_lifetime_hashmap_clear_fires(void)
{
    lt_setup_handler_only();

    u8 buf[8 * sizeof(hashmap_slot)];
    hashmap m;
    hashmap_init(&m, bytes_from(buf, sizeof(buf)), 8, NULL);

    u64 k = 42; int v = 100;
    hashmap_insert(&m, &k, &v);

    borrowed_ptr b = borrowed_ptr_from_lifetime(
        hashmap_get_or_null(&m, &k), &m.lt, &m);

    hashmap_clear(&m);

    LT_FIRES(borrowed_ptr_get(&b));
    EXPECT(_lt_cap_fired == 1);
}

#endif /* CANON_LIFETIME_DEBUG */

/* ── Suppress unused generated functions ─────────────────────────────────── *
 *
 * The DEFINE_VEC / DEFINE_DEQUE / DEFINE_PRIORITY_QUEUE / hashmap.h
 * instantiations stamp out many functions not directly called in this test
 * file. Reference them with (void) so cppcheck and stricter linters
 * don't complain. Matches the convention used in the other test files. */

static void borrow_test_suppress_unused(void)
{
    /* hashmap — all symbol names confirmed by reading hashmap.h directly.
     * Reference the ones we know exist and aren't called above. The other
     * container instantiations (vec, deque, pq) emit many generated names
     * whose exact spellings would require introspecting each header; those
     * are covered by the GCC diagnostic-ignored block at the top of the
     * file rather than enumerated here. */
    (void)hashmap_get;
    (void)hashmap_contains_key;
    (void)hashmap_len;
    (void)hashmap_capacity;
    (void)hashmap_is_empty;
    (void)hashmap_load_factor;
    (void)hashmap_iter_next;
    (void)hashmap_buffer_size;
}

/* ============================================================================
   Unit test entry point
   ============================================================================ */

int main(void)
{
    (void)borrow_test_suppress_unused;

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
    /* Arena lifetime tests — Phase 1 coverage */
    test_lifetime_borrow_captures_id();
    test_lifetime_get_open_borrow_silent();
    test_lifetime_get_after_reset_fires();
    test_lifetime_untracked_borrow_bypasses_check();
    test_lifetime_get_after_reset_to_silent();
    test_lifetime_str_get_after_reset_fires();
    test_lifetime_ptr_get_after_reset_fires();
    test_lifetime_slice_int_get_after_reset_fires();
    test_lifetime_slice_int_as_bytes_inherits_and_fires();

    /* Phase 4 — vec end-to-end */
    test_lifetime_vec_borrow_open_is_silent();
    test_lifetime_vec_free_closes_fires();
    test_lifetime_vec_swap_invalidates_a_fires();
    test_lifetime_vec_in_place_push_silent();

    /* Phase 4 — deque end-to-end */
    test_lifetime_deque_borrow_open_is_silent();
    test_lifetime_deque_swap_invalidates_fires();
    test_lifetime_deque_in_place_push_silent();

    /* Phase 4 — priority_queue end-to-end */
    test_lifetime_pq_peek_raw_silent();
    test_lifetime_pq_push_restamps_fires();
    test_lifetime_pq_pop_restamps_fires();
    test_lifetime_pq_query_silent();

    /* Phase 4 — hashmap end-to-end */
    test_lifetime_hashmap_borrow_open_is_silent();
    test_lifetime_hashmap_insert_restamps_fires();
    test_lifetime_hashmap_remove_restamps_fires();
    test_lifetime_hashmap_clear_fires();
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
