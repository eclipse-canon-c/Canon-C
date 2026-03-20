/**
 * @file arena_test.c
 * @brief Tests for arena.h — linear bump allocator
 *
 * Covers:
 *   - arena_init: basic setup, NULL/zero-capacity contracts
 *   - arena_alloc: normal allocation, alignment, exhaustion, zero-size
 *   - arena_alloc_aligned: custom power-of-2 alignments
 *   - arena_alloc_zero / arena_alloc_aligned_zero: memory is zeroed
 *   - arena_try_alloc / arena_try_alloc_aligned: bool return variants
 *   - arena_alloc_type / arena_alloc_array macros
 *   - arena_reset: fast reset, reuse after reset
 *   - arena_reset_secure: wipes consumed memory
 *   - arena_mark / arena_reset_to: checkpoint / rollback
 *   - arena_capacity / arena_remaining / arena_used / arena_is_empty / arena_is_full
 *   - arena_as_bytes / arena_buffer_bytes / arena_free_bytes
 *   - CANON_ARENA_DEBUG stats (alloc_count, peak) when enabled
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

static int g_failed = 0;

#define EXPECT(cond)                                             \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL %s:%d  %s\n",                 \
                    __FILE__, __LINE__, #cond);                  \
            g_failed++;                                          \
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
    EXPECT(p != NULL);
    EXPECT(mem_is_aligned(p, CANON_DEFAULT_ALIGN));
}

static void test_alloc_sequential_non_overlapping(void)
{
    setup();
    u8* a = (u8*)arena_alloc(&g_arena, 8);
    u8* b = (u8*)arena_alloc(&g_arena, 8);
    EXPECT(a != NULL && b != NULL);
    EXPECT(b >= a + 8);
}

static void test_alloc_exhaustion_returns_null(void)
{
    setup();
    void* p = arena_alloc(&g_arena, BUF_SIZE);
    if (p == NULL) {
        /* already full due to alignment pad */
    } else {
        EXPECT(arena_alloc(&g_arena, 1) == NULL);
    }
}

static void test_alloc_exact_fit(void)
{
    u8    small[64];
    Arena a;
    arena_init(&a, small, sizeof(small));
    usize rem = arena_remaining(&a);
    void* p   = arena_alloc(&a, rem);
    EXPECT(p != NULL);
    EXPECT(arena_alloc(&a, 1) == NULL);
}

/* ── arena_alloc_aligned ─────────────────────────────────────────────────── */

static void test_alloc_aligned_16(void)
{
    setup();
    void* p = arena_alloc_aligned(&g_arena, 32, 16);
    EXPECT(p != NULL);
    EXPECT(mem_is_aligned(p, 16));
}

static void test_alloc_aligned_64(void)
{
    setup();
    void* p = arena_alloc_aligned(&g_arena, 8, 64);
    EXPECT(p != NULL);
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
    EXPECT(p != NULL);
    for (i = 0; i < 32; i++) EXPECT(p[i] == 0);
}

static void test_alloc_aligned_zero_is_zeroed(void)
{
    setup();
    u8*   p = (u8*)arena_alloc_aligned_zero(&g_arena, 16, 16);
    usize i;
    EXPECT(p != NULL);
    EXPECT(mem_is_aligned(p, 16));
    for (i = 0; i < 16; i++) EXPECT(p[i] == 0);
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
    EXPECT(p != NULL);
    EXPECT(mem_is_aligned(p, 16));
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
    EXPECT(a != NULL);
    arena_reset(&g_arena);
    void* b = arena_alloc(&g_arena, 128);
    EXPECT(b != NULL);
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
    EXPECT(p != NULL);
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

/* ── typed macros ────────────────────────────────────────────────────────── */

static void test_alloc_type_macro(void)
{
    setup();
    i32* p = arena_alloc_type(&g_arena, i32);
    EXPECT(p != NULL);
    *p = 42;
    EXPECT(*p == 42);
}

static void test_alloc_array_macro(void)
{
    setup();
    i32*  arr = arena_alloc_array(&g_arena, i32, 8);
    usize i;
    EXPECT(arr != NULL);
    for (i = 0; i < 8; i++) arr[i] = (i32)i;
    for (i = 0; i < 8; i++) EXPECT(arr[i] == (i32)i);
}

static void test_alloc_type_zero_macro(void)
{
    setup();
    i32* p = arena_alloc_type_zero(&g_arena, i32);
    EXPECT(p != NULL);
    EXPECT(*p == 0);
}

static void test_alloc_array_zero_macro(void)
{
    setup();
    i32*  arr = arena_alloc_array_zero(&g_arena, i32, 8);
    usize i;
    EXPECT(arr != NULL);
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

/* ── Unit test entry point ───────────────────────────────────────────────── */

#ifndef CANON_FUZZING

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

    test_try_alloc_success();
    test_try_alloc_failure();
    test_try_alloc_aligned_success();

    test_reset_clears_offset();
    test_reset_allows_reuse();
    test_reset_null_safe();

    test_reset_secure_wipes_memory();
    test_reset_secure_null_safe();

    test_mark_and_rollback();
    test_mark_at_zero();
    test_mark_null_safe();
    test_nested_marks();

    test_query_null_safe();
    test_remaining_decreases_on_alloc();

    test_arena_as_bytes();
    test_arena_buffer_bytes();
    test_arena_free_bytes();
    test_arena_free_bytes_when_full();

    test_alloc_type_macro();
    test_alloc_array_macro();
    test_alloc_type_zero_macro();
    test_alloc_array_zero_macro();

    test_debug_alloc_count();
    test_debug_peak();
    test_debug_reset_clears_stats();
    test_debug_stats_function();
    test_debug_stats_null_returns_zero();

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
