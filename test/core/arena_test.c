/**
 * @file arena_test.c
 * @brief Tests for arena.h — linear bump allocator
 *
 * Covers:
 *   - arena_init: basic setup, NULL/zero-capacity contracts
 *   - arena_alloc: normal allocation, alignment, exhaustion, zero-size
 *   - arena_alloc_aligned: custom power-of-2 alignments
 *   - arena_alloc_zero / arena_alloc_aligned_zero: memory is zeroed,
 *     exhaustion returns NULL (closes `if (p)` FALSE branch)
 *   - arena_try_alloc / arena_try_alloc_aligned: bool return variants,
 *     NULL out parameter handling (closes `if (out)` FALSE branch)
 *   - arena_alloc_type / arena_alloc_array macros
 *   - arena_reset: fast reset, reuse after reset
 *   - arena_reset_secure: wipes consumed memory, empty-arena early return
 *   - arena_mark / arena_reset_to: checkpoint / rollback
 *   - arena_capacity / arena_remaining / arena_used / arena_is_empty / arena_is_full
 *   - arena_as_bytes / arena_buffer_bytes / arena_free_bytes
 *     (including exhausted-arena case)
 *   - CANON_ARENA_DEBUG stats (alloc_count, peak) when enabled
 *   - CANON_LIFETIME_DEBUG: lifetime ID stamping, restamping on reset,
 *     and rollback-preserves-id contract
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random (size, alignment) pairs into arena_alloc /
 *     arena_alloc_aligned on a fixed buffer. Verifies no crash, no
 *     overflow, and that returned pointers satisfy alignment.
 */

/* Must be defined in exactly one TU before including contract.h (via arena.h) */
#define CANON_CONTRACT_IMPL
#define CANON_ARENA_DEBUG
#include "core/arena.h"

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

/* EXPECT_NOT_NULL — records failure and returns from the calling function
 * if ptr is NULL. Prevents cppcheck nullPointerRedundantCheck false positives
 * that arise when EXPECT(p != NULL) is followed by a dereference of p. */
#define EXPECT_NOT_NULL(ptr)                                     \
    do {                                                         \
        if ((ptr) == NULL) {                                     \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",         \
                    __FILE__, __LINE__, #ptr);                   \
            g_failed++;                                          \
            return;                                              \
        }                                                        \
    } while (0)

/* ── Helpers ─────────────────────────────────────────────────────────────── */

#define BUF_SIZE ((usize)512)

static u8    g_buf[BUF_SIZE];
static Arena g_arena;

static void setup(void)
{
    memset(g_buf, 0xAB, BUF_SIZE);
    arena_init(&g_arena, g_buf, BUF_SIZE);
}

/* Drain an arena to exactly remaining == 0.
 *
 * The naive loop `while (arena_alloc(&a, 1)) {}` does NOT reach
 * remaining == 0 because each 1-byte alloc consumes CANON_DEFAULT_ALIGN
 * bytes of offset (alignment padding). Once offset gets within
 * CANON_DEFAULT_ALIGN - 1 of capacity, the next 1-byte alloc fails the
 * capacity check and the loop terminates with non-zero remaining.
 *
 * The pattern used by test_alloc_exact_fit is reliable: allocate a
 * full CANON_DEFAULT_ALIGN chunk to force the offset onto a clean
 * boundary, then allocate the entire remaining capacity in one call.
 * After that, remaining == 0 deterministically. */
static void drain_arena_to_zero(Arena* a)
{
    /* Force offset onto an aligned boundary */
    (void)arena_alloc(a, CANON_DEFAULT_ALIGN);
    usize rem = arena_remaining(a);
    if (rem > 0) {
        (void)arena_alloc(a, rem);
    }
}

/* ── arena_init ──────────────────────────────────────────────────────────── */

static void test_init_state(void)
{
    setup();
    EXPECT(g_arena.buffer   == g_buf);
    EXPECT(g_arena.capacity == BUF_SIZE);
    EXPECT(g_arena.offset   == 0);
    EXPECT(arena_is_empty(&g_arena));
    EXPECT(!arena_is_full(&g_arena));
    EXPECT(arena_used(&g_arena)      == 0);
    EXPECT(arena_remaining(&g_arena) == BUF_SIZE);
    EXPECT(arena_capacity(&g_arena)  == BUF_SIZE);
}

/* ── arena_alloc — basic ─────────────────────────────────────────────────── */

static void test_alloc_returns_non_null(void)
{
    setup();
    void* p = arena_alloc(&g_arena, 16);
    EXPECT(p != NULL);
}

static void test_alloc_zero_size_returns_null(void)
{
    setup();
    void* p = arena_alloc(&g_arena, 0);
    EXPECT(p == NULL);
}

static void test_alloc_advances_offset(void)
{
    setup();
    usize before = arena_used(&g_arena);
    arena_alloc(&g_arena, 32);
    EXPECT(arena_used(&g_arena) >= before + 32);
}

static void test_alloc_default_alignment(void)
{
    setup();
    void* p = arena_alloc(&g_arena, 1);
    EXPECT_NOT_NULL(p);
    EXPECT(mem_is_aligned(p, CANON_DEFAULT_ALIGN));
}

static void test_alloc_sequential_non_overlapping(void)
{
    setup();
    u8* a = (u8*)arena_alloc(&g_arena, 8);
    u8* b = (u8*)arena_alloc(&g_arena, 8);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);
    EXPECT(b >= a + 8);
}

static void test_alloc_exhaustion_returns_null(void)
{
    setup();
    void* p = arena_alloc(&g_arena, BUF_SIZE);
    if (p == NULL) {
        /* already full due to alignment pad — expected */
    } else {
        EXPECT(arena_alloc(&g_arena, 1) == NULL);
    }
}

static void test_alloc_exact_fit(void)
{
    u8    small[128];
    Arena a;
    void* p;
    usize rem;

    arena_init(&a, small, sizeof(small));

    /* Allocate CANON_DEFAULT_ALIGN bytes — offset is now exactly
     * CANON_DEFAULT_ALIGN, which is aligned, so remaining is a
     * clean multiple of CANON_DEFAULT_ALIGN with no padding needed. */
    p = arena_alloc(&a, CANON_DEFAULT_ALIGN);
    EXPECT_NOT_NULL(p);

    rem = arena_remaining(&a);
    EXPECT(rem > 0);

    p = arena_alloc(&a, rem);
    EXPECT_NOT_NULL(p);
    EXPECT(arena_remaining(&a) == 0);
    EXPECT(arena_alloc(&a, 1) == NULL);
}

/* ── arena_alloc_aligned ─────────────────────────────────────────────────── */

static void test_alloc_aligned_16(void)
{
    setup();
    void* p = arena_alloc_aligned(&g_arena, 32, 16);
    EXPECT_NOT_NULL(p);
    EXPECT(mem_is_aligned(p, 16));
}

static void test_alloc_aligned_64(void)
{
    setup();
    void* p = arena_alloc_aligned(&g_arena, 8, 64);
    EXPECT_NOT_NULL(p);
    EXPECT(mem_is_aligned(p, 64));
}

static void test_alloc_aligned_1(void)
{
    setup();
    void* p = arena_alloc_aligned(&g_arena, 4, 1);
    EXPECT(p != NULL);
}

static void test_alloc_aligned_zero_size_returns_null(void)
{
    setup();
    void* p = arena_alloc_aligned(&g_arena, 0, 8);
    EXPECT(p == NULL);
}

/* ── arena_alloc_zero ────────────────────────────────────────────────────── */

static void test_alloc_zero_is_zeroed(void)
{
    setup();
    u8*   p = (u8*)arena_alloc_zero(&g_arena, 32);
    usize i;
    EXPECT_NOT_NULL(p);
    for (i = 0; i < 32; i++) EXPECT(p[i] == 0);
}

static void test_alloc_aligned_zero_is_zeroed(void)
{
    setup();
    u8*   p = (u8*)arena_alloc_aligned_zero(&g_arena, 16, 16);
    usize i;
    EXPECT_NOT_NULL(p);
    EXPECT(mem_is_aligned(p, 16));
    for (i = 0; i < 16; i++) EXPECT(p[i] == 0);
}

/* MC/DC: closes the `if (p)` FALSE branch in arena_alloc_zero — when
 * the underlying alloc returns NULL, mem_zero must not be called. */
static void test_alloc_zero_exhaustion_returns_null(void)
{
    setup();
    void* p = arena_alloc_zero(&g_arena, BUF_SIZE * 2);
    EXPECT(p == NULL);
}

/* MC/DC: closes the `if (p)` FALSE branch in arena_alloc_aligned_zero. */
static void test_alloc_aligned_zero_exhaustion_returns_null(void)
{
    setup();
    void* p = arena_alloc_aligned_zero(&g_arena, BUF_SIZE * 2, 16);
    EXPECT(p == NULL);
}

/* ── arena_try_alloc ─────────────────────────────────────────────────────── */

static void test_try_alloc_success(void)
{
    setup();
    void* p  = NULL;
    bool  ok = arena_try_alloc(&g_arena, 16, &p);
    EXPECT(ok);
    EXPECT(p != NULL);
}

static void test_try_alloc_failure(void)
{
    setup();
    void* p  = NULL;
    bool  ok = arena_try_alloc(&g_arena, BUF_SIZE * 2, &p);
    EXPECT(!ok);
    EXPECT(p == NULL);
}

static void test_try_alloc_aligned_success(void)
{
    setup();
    void* p  = NULL;
    bool  ok = arena_try_alloc_aligned(&g_arena, 16, 16, &p);
    EXPECT(ok);
    EXPECT_NOT_NULL(p);
    EXPECT(mem_is_aligned(p, 16));
}

/* MC/DC: closes the `if (out)` FALSE branch in arena_try_alloc.
 *
 * REQUIRES the arena.h fix: the C code must match its ACSL contract
 *     ensures \result <==> (out != \null && *out != \null);
 * which means the return must be:
 *     return out != NULL && p != NULL;
 * not:
 *     return p != NULL;
 *
 * Without the fix, this test fails on the path where out == NULL but
 * the internal allocation succeeds. */
static void test_try_alloc_null_out(void)
{
    setup();
    bool ok = arena_try_alloc(&g_arena, 16, NULL);
    EXPECT(ok == false);
}

/* MC/DC: same `if (out)` FALSE branch closure for arena_try_alloc_aligned. */
static void test_try_alloc_aligned_null_out(void)
{
    setup();
    bool ok = arena_try_alloc_aligned(&g_arena, 16, 16, NULL);
    EXPECT(ok == false);
}

/* ── arena_reset ─────────────────────────────────────────────────────────── */

static void test_reset_clears_offset(void)
{
    setup();
    arena_alloc(&g_arena, 64);
    arena_reset(&g_arena);
    EXPECT(arena_used(&g_arena)      == 0);
    EXPECT(arena_remaining(&g_arena) == BUF_SIZE);
    EXPECT(arena_is_empty(&g_arena));
}

static void test_reset_allows_reuse(void)
{
    setup();
    void* a = arena_alloc(&g_arena, 128);
    EXPECT_NOT_NULL(a);
    arena_reset(&g_arena);
    void* b = arena_alloc(&g_arena, 128);
    EXPECT_NOT_NULL(b);
    EXPECT(b == a);
}

static void test_reset_null_safe(void)
{
    arena_reset(NULL);
    EXPECT(1);
}

/* ── arena_reset_secure ──────────────────────────────────────────────────── */

static void test_reset_secure_wipes_memory(void)
{
    setup();
    u8*   p = (u8*)arena_alloc(&g_arena, 32);
    usize i;
    EXPECT_NOT_NULL(p);
    for (i = 0; i < 32; i++) p[i] = (u8)(i + 1);
    usize used = arena_used(&g_arena);
    arena_reset_secure(&g_arena);
    EXPECT(arena_used(&g_arena) == 0);
    for (i = 0; i < used; i++) EXPECT(g_buf[i] == 0);
}

static void test_reset_secure_null_safe(void)
{
    arena_reset_secure(NULL);
    EXPECT(1);
}

/* MC/DC: closes the offset==0 TRUE side of (!arena || offset==0).
 * test_reset_secure_wipes_memory hits both subconditions FALSE;
 * test_reset_secure_null_safe hits the left-TRUE; this hits
 * left-FALSE && right-TRUE (initialized but empty arena). */
static void test_reset_secure_empty_arena(void)
{
    setup();
    /* Don't allocate — offset is 0 */
    arena_reset_secure(&g_arena);
    EXPECT(arena_used(&g_arena) == 0);
    EXPECT(arena_is_empty(&g_arena));
}

/* ── arena_mark / arena_reset_to ─────────────────────────────────────────── */

static void test_mark_and_rollback(void)
{
    setup();
    arena_alloc(&g_arena, 16);
    ArenaMark mark = arena_mark(&g_arena);
    usize     used = arena_used(&g_arena);
    void*     tmp  = arena_alloc(&g_arena, 64);
    EXPECT(tmp != NULL);
    EXPECT(arena_used(&g_arena) > used);
    arena_reset_to(&g_arena, mark);
    EXPECT(arena_used(&g_arena) == used);
}

static void test_mark_at_zero(void)
{
    setup();
    ArenaMark mark = arena_mark(&g_arena);
    arena_alloc(&g_arena, 64);
    arena_reset_to(&g_arena, mark);
    EXPECT(arena_used(&g_arena) == 0);
    EXPECT(arena_is_empty(&g_arena));
}

static void test_mark_null_safe(void)
{
    ArenaMark m = arena_mark(NULL);
    EXPECT(m == 0);
    arena_reset_to(NULL, 0);
    EXPECT(1);
}

static void test_nested_marks(void)
{
    setup();
    arena_alloc(&g_arena, 8);
    ArenaMark m1 = arena_mark(&g_arena);
    arena_alloc(&g_arena, 8);
    ArenaMark m2 = arena_mark(&g_arena);
    arena_alloc(&g_arena, 8);
    arena_reset_to(&g_arena, m2);
    EXPECT(arena_used(&g_arena) == (usize)m2);
    arena_reset_to(&g_arena, m1);
    EXPECT(arena_used(&g_arena) == (usize)m1);
}

/* ── query functions ─────────────────────────────────────────────────────── */

static void test_query_null_safe(void)
{
    EXPECT(arena_capacity(NULL)  == 0);
    EXPECT(arena_remaining(NULL) == 0);
    EXPECT(arena_used(NULL)      == 0);
    EXPECT(arena_is_empty(NULL)  == true);
    EXPECT(arena_is_full(NULL)   == true);
}

static void test_remaining_decreases_on_alloc(void)
{
    setup();
    usize before = arena_remaining(&g_arena);
    arena_alloc(&g_arena, 16);
    EXPECT(arena_remaining(&g_arena) < before);
    EXPECT(arena_used(&g_arena) + arena_remaining(&g_arena) == BUF_SIZE);
}

/* MC/DC: closes the offset==0 FALSE outcome on a non-null arena.
 * test_init_state covers TRUE; test_query_null_safe covers !arena TRUE.
 * This is the missing combination: arena non-null AND offset > 0. */
static void test_is_empty_after_alloc(void)
{
    setup();
    EXPECT(arena_is_empty(&g_arena));      /* before alloc */
    arena_alloc(&g_arena, 16);
    EXPECT(!arena_is_empty(&g_arena));     /* after alloc — closes the gap */
}

/* MC/DC: closes the offset >= capacity TRUE outcome on a non-null arena.
 * Uses drain_arena_to_zero() to reach exact exhaustion deterministically. */
static void test_is_full_when_exhausted(void)
{
    u8    small[128];
    Arena a;
    arena_init(&a, small, sizeof(small));

    drain_arena_to_zero(&a);
    EXPECT(arena_remaining(&a) == 0);
    EXPECT(arena_is_full(&a));
}

/* ── bytes views ─────────────────────────────────────────────────────────── */

static void test_arena_as_bytes(void)
{
    setup();
    arena_alloc(&g_arena, 32);
    bytes_t b = arena_as_bytes(&g_arena);
    EXPECT(b.ptr == g_buf);
    EXPECT(b.len == arena_used(&g_arena));
}

static void test_arena_buffer_bytes(void)
{
    setup();
    arena_alloc(&g_arena, 32);
    bytes_t b = arena_buffer_bytes(&g_arena);
    EXPECT(b.ptr == g_buf);
    EXPECT(b.len == BUF_SIZE);
}

static void test_arena_free_bytes(void)
{
    setup();
    arena_alloc(&g_arena, 32);
    bytes_t f = arena_free_bytes(&g_arena);
    EXPECT(f.len == arena_remaining(&g_arena));
    EXPECT(f.ptr == g_buf + arena_used(&g_arena));
}

static void test_arena_free_bytes_when_full(void)
{
    setup();
    arena_alloc(&g_arena, BUF_SIZE);
    if (arena_remaining(&g_arena) == 0) {
        bytes_t f = arena_free_bytes(&g_arena);
        EXPECT(f.ptr == NULL);
        EXPECT(f.len == 0);
    }
    EXPECT(1);
}

/* MC/DC: closes the offset >= capacity TRUE branch in arena_free_bytes
 * unconditionally. Uses drain_arena_to_zero() for deterministic exhaustion. */
static void test_free_bytes_when_exhausted(void)
{
    u8    small[128];
    Arena a;
    arena_init(&a, small, sizeof(small));

    drain_arena_to_zero(&a);
    EXPECT(arena_remaining(&a) == 0);

    bytes_t f = arena_free_bytes(&a);
    EXPECT(f.ptr == NULL);
    EXPECT(f.len == 0);
}

/* ── typed macros ────────────────────────────────────────────────────────── */

static void test_alloc_type_macro(void)
{
    setup();
    i32* p = arena_alloc_type(&g_arena, i32);
    EXPECT_NOT_NULL(p);
    *p = 42;
    EXPECT(*p == 42);
}

static void test_alloc_array_macro(void)
{
    setup();
    i32*  arr = arena_alloc_array(&g_arena, i32, 8);
    usize i;
    EXPECT_NOT_NULL(arr);
    for (i = 0; i < 8; i++) arr[i] = (i32)i;
    for (i = 0; i < 8; i++) EXPECT(arr[i] == (i32)i);
}

static void test_alloc_type_zero_macro(void)
{
    setup();
    i32* p = arena_alloc_type_zero(&g_arena, i32);
    EXPECT_NOT_NULL(p);
    EXPECT(*p == 0);
}

static void test_alloc_array_zero_macro(void)
{
    setup();
    i32*  arr = arena_alloc_array_zero(&g_arena, i32, 8);
    usize i;
    EXPECT_NOT_NULL(arr);
    for (i = 0; i < 8; i++) EXPECT(arr[i] == 0);
}

/* ── CANON_ARENA_DEBUG stats ─────────────────────────────────────────────── */

static void test_debug_alloc_count(void)
{
    setup();
    EXPECT(g_arena.alloc_count == 0);
    arena_alloc(&g_arena, 8);
    EXPECT(g_arena.alloc_count == 1);
    arena_alloc(&g_arena, 8);
    EXPECT(g_arena.alloc_count == 2);
}

static void test_debug_peak(void)
{
    setup();
    arena_alloc(&g_arena, 32);
    usize after_first = g_arena.peak;
    EXPECT(after_first >= 32);
    ArenaMark m = arena_mark(&g_arena);
    arena_alloc(&g_arena, 64);
    usize after_second = g_arena.peak;
    EXPECT(after_second >= after_first + 64);
    arena_reset_to(&g_arena, m);
    EXPECT(g_arena.peak == after_second);
}

static void test_debug_reset_clears_stats(void)
{
    setup();
    arena_alloc(&g_arena, 16);
    arena_reset(&g_arena);
    EXPECT(g_arena.alloc_count == 0);
    EXPECT(g_arena.peak        == 0);
}

static void test_debug_stats_function(void)
{
    setup();
    arena_alloc(&g_arena, 16);
    ArenaStats s = arena_stats(&g_arena);
    EXPECT(s.used        == arena_used(&g_arena));
    EXPECT(s.remaining   == arena_remaining(&g_arena));
    EXPECT(s.capacity    == BUF_SIZE);
    EXPECT(s.alloc_count == 1);
    EXPECT(s.peak        >= 16);
}

static void test_debug_stats_null_returns_zero(void)
{
    ArenaStats s = arena_stats(NULL);
    EXPECT(s.used        == 0);
    EXPECT(s.remaining   == 0);
    EXPECT(s.capacity    == 0);
    EXPECT(s.peak        == 0);
    EXPECT(s.alloc_count == 0);
}

/* ── CANON_LIFETIME_DEBUG: lifetime token semantics ──────────────────────── *
 *
 * These tests exercise the (id, open) lifetime token embedded on the Arena
 * when CANON_LIFETIME_DEBUG is defined. They verify state-level invariants
 * directly on a.lt — they do NOT construct borrows or trigger
 * lifetime_assert_valid. The borrow-side validation path (fires require_msg
 * after reset) is covered in test/semantics/borrow_test.c, alongside the
 * borrow construction logic and the contract-trap helpers it needs.
 *
 * The contract under test is:
 *   - arena_init opens the lifetime: lt.open == true, lt.id is set.
 *   - arena_reset re-stamps lt.id: new ID differs from old ID.
 *   - arena_reset_secure also re-stamps lt.id.
 *   - arena_reset_to does NOT touch lt.id — borrows allocated before the
 *     mark must remain valid through rollback (rollback contract).
 */

#ifdef CANON_LIFETIME_DEBUG

static void test_lifetime_init_opens_token(void)
{
    setup();
    EXPECT(g_arena.lt.open == true);
    /* ID is address-derived, so for a non-NULL arena it is non-zero on
     * any reasonable platform. We don't assert the exact value — just
     * that the token is in the "open" state with some ID assigned. */
    EXPECT(g_arena.lt.id != REGION_ID_STATIC);
}

static void test_lifetime_reset_restamps_id(void)
{
    setup();
    region_id_t before = g_arena.lt.id;
    arena_reset(&g_arena);
    EXPECT(g_arena.lt.id != before);
    EXPECT(g_arena.lt.open == true); /* reset recycles, does not close */
}

static void test_lifetime_reset_secure_restamps_id(void)
{
    setup();
    /* Allocate something so reset_secure has bytes to wipe — exercises
     * the same restamp path as the empty-arena case. */
    arena_alloc(&g_arena, 32);
    region_id_t before = g_arena.lt.id;
    arena_reset_secure(&g_arena);
    EXPECT(g_arena.lt.id != before);
    EXPECT(g_arena.lt.open == true);
}

static void test_lifetime_reset_to_preserves_id(void)
{
    /* Rollback contract: borrows allocated before the mark must remain
     * valid after arena_reset_to. The lifetime ID must NOT change. */
    setup();
    arena_alloc(&g_arena, 16);
    ArenaMark   mark   = arena_mark(&g_arena);
    region_id_t before = g_arena.lt.id;
    arena_alloc(&g_arena, 64);
    arena_reset_to(&g_arena, mark);
    EXPECT(g_arena.lt.id == before);
    EXPECT(g_arena.lt.open == true);
}

#endif /* CANON_LIFETIME_DEBUG */

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    test_init_state();

    test_alloc_returns_non_null();
    test_alloc_zero_size_returns_null();
    test_alloc_advances_offset();
    test_alloc_default_alignment();
    test_alloc_sequential_non_overlapping();
    test_alloc_exhaustion_returns_null();
    test_alloc_exact_fit();

    test_alloc_aligned_16();
    test_alloc_aligned_64();
    test_alloc_aligned_1();
    test_alloc_aligned_zero_size_returns_null();

    test_alloc_zero_is_zeroed();
    test_alloc_aligned_zero_is_zeroed();
    test_alloc_zero_exhaustion_returns_null();
    test_alloc_aligned_zero_exhaustion_returns_null();

    test_try_alloc_success();
    test_try_alloc_failure();
    test_try_alloc_aligned_success();
    test_try_alloc_null_out();
    test_try_alloc_aligned_null_out();

    test_reset_clears_offset();
    test_reset_allows_reuse();
    test_reset_null_safe();

    test_reset_secure_wipes_memory();
    test_reset_secure_null_safe();
    test_reset_secure_empty_arena();

    test_mark_and_rollback();
    test_mark_at_zero();
    test_mark_null_safe();
    test_nested_marks();

    test_query_null_safe();
    test_remaining_decreases_on_alloc();
    test_is_empty_after_alloc();
    test_is_full_when_exhausted();

    test_arena_as_bytes();
    test_arena_buffer_bytes();
    test_arena_free_bytes();
    test_arena_free_bytes_when_full();
    test_free_bytes_when_exhausted();

    test_alloc_type_macro();
    test_alloc_array_macro();
    test_alloc_type_zero_macro();
    test_alloc_array_zero_macro();

    test_debug_alloc_count();
    test_debug_peak();
    test_debug_reset_clears_stats();
    test_debug_stats_function();
    test_debug_stats_null_returns_zero();

#ifdef CANON_LIFETIME_DEBUG
    test_lifetime_init_opens_token();
    test_lifetime_reset_restamps_id();
    test_lifetime_reset_secure_restamps_id();
    test_lifetime_reset_to_preserves_id();
#endif

    if (g_failed == 0) {
        printf("OK  arena_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  arena_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */

#define FUZZ_BUF_SIZE ((usize)1024)

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static u8    fuzz_buf[FUZZ_BUF_SIZE];
    Arena        a;
    u8           raw[8];
    u16          size1;
    u16          size2;
    u16          size3;
    u8           log2_align;
    usize        alignment;
    bool         do_reset;
    void*        p;

    memset(raw, 0, sizeof(raw));
    if (size > sizeof(raw)) size = sizeof(raw);
    memcpy(raw, data, size);

    size1      = (u16)((u16)raw[0] | ((u16)raw[1] << 8));
    log2_align = raw[2] & 0x07u;
    size2      = (u16)((u16)raw[3] | ((u16)raw[4] << 8));
    do_reset   = raw[5] != 0;
    size3      = (u16)((u16)raw[6] | ((u16)raw[7] << 8));

    alignment  = (usize)1 << log2_align;

    arena_init(&a, fuzz_buf, FUZZ_BUF_SIZE);

    p = arena_alloc(&a, (usize)size1);
    if (p != NULL) {
        if ((u8*)p < fuzz_buf || (u8*)p + size1 > fuzz_buf + FUZZ_BUF_SIZE)
            __builtin_trap();
        if (!mem_is_aligned(p, CANON_DEFAULT_ALIGN))
            __builtin_trap();
    }

    if (do_reset) arena_reset(&a);

    p = arena_alloc_aligned(&a, (usize)size2, alignment);
    if (p != NULL) {
        if ((u8*)p < fuzz_buf || (u8*)p + size2 > fuzz_buf + FUZZ_BUF_SIZE)
            __builtin_trap();
        if (!mem_is_aligned(p, alignment))
            __builtin_trap();
    }

    p = arena_alloc_zero(&a, (usize)size3);
    if (p != NULL && size3 > 0) {
        if (!mem_is_zero(p, (usize)size3))
            __builtin_trap();
    }

    {
        ArenaMark mark = arena_mark(&a);
        usize     used = arena_used(&a);
        void*     tmp  = arena_alloc(&a, 32);
        (void)tmp;
        arena_reset_to(&a, mark);
        if (arena_used(&a) != used) __builtin_trap();
    }

    arena_reset_secure(&a);
    if (arena_used(&a) != 0) __builtin_trap();

    return 0;
}

#endif /* CANON_FUZZING */
