/**
 * @file pool_test.c
 * @brief Tests for pool.h — fixed-size object pool allocator
 *
 * Covers:
 *   - pool_init: basic setup, arena reservation, failure cases
 *   - pool_init from an UNALIGNED arena offset (regression: base_mark must be
 *     the post-padding data start, not the pre-padding mark)
 *   - pool_alloc: sequential allocation, exhaustion, NULL pool
 *   - pool_alloc_zero: memory is zeroed
 *   - pool_try_alloc / pool_try_alloc_zero: bool return variants
 *   - pool_get / pool_get_const: bounds-checked index access
 *   - pool_reset: fast reset, reuse after reset
 *   - pool_reset_secure: wipes allocated object memory
 *   - Query: pool_used, pool_capacity, pool_remaining,
 *            pool_is_full, pool_is_empty, pool_object_size,
 *            pool_memory_used, pool_memory_reserved
 *   - Byte views: pool_as_bytes, pool_reserved_bytes
 *   - Type-safe macros: pool_init_type, pool_alloc_type,
 *     pool_alloc_type_zero, pool_get_type, pool_get_type_const
 *   - Multiple pools from the same arena
 *   - CANON_LIFETIME_DEBUG: lifetime ID stamping and restamping on reset
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random allocation counts and object sizes into the pool,
 *     plus a random pre-pad of the backing arena so the pool's base_mark
 *     is exercised at every alignment, verifying no crash, no out-of-bounds
 *     access, and correct state.
 */

/* Must be defined in exactly one TU before including contract.h (via pool.h) */
#define CANON_CONTRACT_IMPL
#include "core/pool.h"

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

#define ARENA_SIZE ((usize)4096)

static u8    g_buf[ARENA_SIZE];
static Arena g_arena;
static Pool  g_pool;

static void setup(void)
{
    memset(g_buf, 0xAB, ARENA_SIZE);
    arena_init(&g_arena, g_buf, ARENA_SIZE);
}

/* Simple test struct */
typedef struct {
    i32 x;
    i32 y;
} Vec2;

/* ── pool_init ───────────────────────────────────────────────────────────── */

static void test_init_basic(void)
{
    bool ok;
    setup();
    ok = pool_init(&g_pool, &g_arena, sizeof(Vec2), 16);
    EXPECT(ok);
    EXPECT(pool_capacity(&g_pool)  == 16);
    EXPECT(pool_used(&g_pool)      == 0);
    EXPECT(pool_remaining(&g_pool) == 16);
    EXPECT(pool_is_empty(&g_pool));
    EXPECT(!pool_is_full(&g_pool));
    EXPECT(pool_object_size(&g_pool) >= sizeof(Vec2));
}

static void test_init_reserves_arena_space(void)
{
    usize before;
    usize after;
    bool  ok;
    setup();
    before = arena_used(&g_arena);
    ok     = pool_init(&g_pool, &g_arena, sizeof(Vec2), 8);
    EXPECT(ok);
    after = arena_used(&g_arena);
    EXPECT(after > before);
    EXPECT(after - before >= sizeof(Vec2) * 8);
}

static void test_init_arena_too_small(void)
{
    bool ok;
    setup();
    /* Request more than the arena can hold */
    ok = pool_init(&g_pool, &g_arena, sizeof(Vec2), ARENA_SIZE);
    EXPECT(!ok);
    /* Arena must be unchanged on failure */
    EXPECT(arena_used(&g_arena) == 0);
}

static void test_init_type_macro(void)
{
    bool ok;
    setup();
    ok = pool_init_type(&g_pool, &g_arena, Vec2, 8);
    EXPECT(ok);
    EXPECT(pool_object_size(&g_pool) >= sizeof(Vec2));
}

static void test_init_post_arena_alloc_safe(void)
{
    /* Allocations after pool_init must not collide with pool region */
    void* extra;
    bool  ok;
    setup();
    ok = pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    EXPECT(ok);
    extra = arena_alloc(&g_arena, 64);
    EXPECT(extra != NULL);
    /* extra must be outside the pool's reserved region */
    EXPECT((u8*)extra >= g_buf + (usize)g_pool.end_mark);
}

/* ── pool_init from an unaligned arena offset (REGRESSION) ───────────────────
 *
 * arena_alloc inserts alignment padding BEFORE the pointer it returns. If
 * pool_init captured base_mark from arena_mark() *before* the reservation
 * (the pre-pad offset) instead of from arena_alloc's returned region (the
 * post-pad data start), then with a non-aligned starting offset every pool
 * slot would sit `pad` bytes below the reserved region and the last object
 * would overrun end_mark. This test forces a non-aligned offset by doing a
 * 1-byte arena_alloc first, then verifies the last object stays within the
 * reserved region. It fails on the pre-fix code and passes on the fixed code.
 */
static void test_init_unaligned_base_no_overrun(void)
{
    void* throwaway;
    Vec2* last;
    u8*   last_end;
    u8*   region_end;
    usize i;
    bool  ok;

    setup();
    /* Push the arena offset to a deliberately odd value. */
    throwaway = arena_alloc(&g_arena, 1);
    EXPECT_NOT_NULL(throwaway);

    ok = pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    EXPECT(ok);

    /* base_mark must be the post-pad data start: the byte at base_mark must
     * be at or after the throwaway allocation's end, never before it. */
    EXPECT((usize)g_pool.base_mark >=
           (usize)((u8*)throwaway - g_buf) + 1);

    /* Fill the pool and check the last object's end is within [base, end). */
    for (i = 0; i < 4; i++) {
        Vec2* v = pool_alloc_type(&g_pool, Vec2);
        EXPECT_NOT_NULL(v);
    }
    last = pool_get_type(&g_pool, 3, Vec2);
    EXPECT_NOT_NULL(last);

    last_end   = (u8*)last + pool_object_size(&g_pool);
    region_end = g_buf + (usize)g_pool.end_mark;
    /* The crux: no overrun of the reserved region. */
    EXPECT(last_end <= region_end);
    /* And the first slot starts exactly at base_mark. */
    EXPECT((u8*)pool_get_type(&g_pool, 0, Vec2) == g_buf + (usize)g_pool.base_mark);
}

/* ── pool_init overflow guards (MC/DC gap closure) ───────────────────────────
 *
 * These two tests exercise the overflow branches in pool_init that the
 * baseline suite never reached (gcov L283 alignment-overflow, L291
 * checked_mul-overflow — both reported "condition 0 not covered (true)").
 * Both pass valid-but-pathological sizes that satisfy the > 0 preconditions
 * yet trip the internal overflow guards, so pool_init must return false and
 * leave the arena untouched.
 */
static void test_init_object_size_alignment_overflow(void)
{
    bool ok;
    setup();
    /* mem_align(CANON_USIZE_MAX) returns CANON_USIZE_MAX (overflow sentinel),
     * so pool_init must reject at the aligned_size == CANON_USIZE_MAX guard.
     * object_size > 0 holds, so the require_msg precondition is satisfied. */
    ok = pool_init(&g_pool, &g_arena, CANON_USIZE_MAX, 1);
    EXPECT(!ok);
    EXPECT(arena_used(&g_arena) == 0);
}

static void test_init_reservation_mul_overflow(void)
{
    bool ok;
    setup();
    /* A large object_size times a large max_objects overflows usize in the
     * reservation-size computation, so checked_mul fails and pool_init must
     * return false. Both args are > 0, satisfying the preconditions. The
     * exact product overflows on both 32- and 64-bit usize: half of
     * CANON_USIZE_MAX times 4 wraps. */
    usize big = (CANON_USIZE_MAX / 2u) + 1u;
    ok = pool_init(&g_pool, &g_arena, big, 4);
    EXPECT(!ok);
    EXPECT(arena_used(&g_arena) == 0);
}

/* ── pool_alloc ──────────────────────────────────────────────────────────── */

static void test_alloc_returns_non_null(void)
{
    void* p;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    p = pool_alloc(&g_pool);
    EXPECT(p != NULL);
    EXPECT(pool_used(&g_pool) == 1);
}

static void test_alloc_sequential_non_overlapping(void)
{
    u8* a;
    u8* b;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    a = (u8*)pool_alloc(&g_pool);
    b = (u8*)pool_alloc(&g_pool);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);
    EXPECT(b >= a + pool_object_size(&g_pool));
}

static void test_alloc_exhaustion_returns_null(void)
{
    usize i;
    void* p;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    for (i = 0; i < 4; i++) {
        p = pool_alloc(&g_pool);
        EXPECT(p != NULL);
    }
    EXPECT(pool_is_full(&g_pool));
    EXPECT(pool_alloc(&g_pool) == NULL);
}

static void test_alloc_fills_used(void)
{
    usize i;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 8);
    for (i = 0; i < 8; i++) pool_alloc(&g_pool);
    EXPECT(pool_used(&g_pool)      == 8);
    EXPECT(pool_remaining(&g_pool) == 0);
}

/* ── pool_alloc_zero ─────────────────────────────────────────────────────── */

static void test_alloc_zero_is_zeroed(void)
{
    Vec2* v;
    setup();
    /* poison then re-init so the pool points at poisoned memory */
    memset(g_buf, 0xAB, ARENA_SIZE);
    arena_init(&g_arena, g_buf, ARENA_SIZE);
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    v = (Vec2*)pool_alloc_zero(&g_pool);
    EXPECT_NOT_NULL(v);
    EXPECT(v->x == 0 && v->y == 0);
}

/* Exercises pool_alloc_zero's `if (p)` false branch (gcov L372): when the
 * pool is full, pool_alloc returns NULL and alloc_zero must NOT call mem_zero
 * — it returns NULL without dereferencing. */
static void test_alloc_zero_full_returns_null(void)
{
    usize i;
    void* p;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 2);
    for (i = 0; i < 2; i++) {
        p = pool_alloc_zero(&g_pool);
        EXPECT(p != NULL);
    }
    /* Pool is now full — alloc_zero must return NULL via the !p path. */
    EXPECT(pool_alloc_zero(&g_pool) == NULL);
    EXPECT(pool_used(&g_pool) == 2);
}

/* ── pool_try_alloc ──────────────────────────────────────────────────────── */

static void test_try_alloc_success(void)
{
    void* p  = NULL;
    bool  ok;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    ok = pool_try_alloc(&g_pool, &p);
    EXPECT(ok);
    EXPECT(p != NULL);
}

static void test_try_alloc_failure(void)
{
    void* p  = NULL;
    bool  ok;
    usize i;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 2);
    for (i = 0; i < 2; i++) pool_alloc(&g_pool);
    ok = pool_try_alloc(&g_pool, &p);
    EXPECT(!ok);
    EXPECT(p == NULL);
}

static void test_try_alloc_zero_success(void)
{
    void* p  = NULL;
    bool  ok;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    ok = pool_try_alloc_zero(&g_pool, &p);
    EXPECT(ok);
    EXPECT(p != NULL);
}

/* Exercises the `if (out)` false branch in pool_try_alloc (gcov L386) and
 * pool_try_alloc_zero (gcov L400): passing out == NULL must still allocate
 * and return the success bool, just without writing through the null out. */
static void test_try_alloc_null_out(void)
{
    bool ok;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    ok = pool_try_alloc(&g_pool, NULL);
    EXPECT(ok);
    EXPECT(pool_used(&g_pool) == 1);
}

static void test_try_alloc_zero_null_out(void)
{
    bool ok;
    setup();
    pool_init(&g_pool, &g_arena, sizeof(Vec2), 4);
    ok = pool_try_alloc_zero(&g_pool, NULL);
    EXPECT(ok);
    EXPECT(pool_used(&g_pool) == 1);
}

/* ── pool_get ────────────────────────────────────────────────────────────── */

static void test_get_returns_correct_slot(void)
{
    Vec2* a;
    Vec2* b;
    Vec2* got;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    a = pool_alloc_type(&g_pool, Vec2);
    b = pool_alloc_type(&g_pool, Vec2);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);
    a->x = 10; a->y = 20;
    b->x = 30; b->y = 40;
    got = pool_get_type(&g_pool, 0, Vec2);
    EXPECT_NOT_NULL(got);
    EXPECT(got->x == 10 && got->y == 20);
    got = pool_get_type(&g_pool, 1, Vec2);
    EXPECT_NOT_NULL(got);
    EXPECT(got->x == 30 && got->y == 40);
}

static void test_get_out_of_bounds_returns_null(void)
{
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    pool_alloc(&g_pool);
    EXPECT(pool_get(&g_pool, 1)  == NULL);   /* used == 1, index 1 OOB */
    EXPECT(pool_get(&g_pool, 99) == NULL);
}

static void test_get_null_pool_returns_null(void)
{
    EXPECT(pool_get(NULL, 0) == NULL);
}

static void test_get_const_returns_correct_slot(void)
{
    Vec2*       v;
    const Vec2* got;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    v = pool_alloc_type(&g_pool, Vec2);
    EXPECT_NOT_NULL(v);
    v->x = 7; v->y = 8;
    got = pool_get_type_const(&g_pool, 0, Vec2);
    EXPECT_NOT_NULL(got);
    EXPECT(got->x == 7 && got->y == 8);
}

/* Exercises pool_get_const's null-pool (gcov L464 cond 0) and OOB (cond 2)
 * outcomes, mirroring test_get_null_pool_returns_null and
 * test_get_out_of_bounds_returns_null, which previously only covered the
 * non-const pool_get. The cond 1 (!pool->arena) outcome remains uncovered —
 * it is unreachable under pool_invariant (a valid pool has a valid arena)
 * and is documented as a deviation, not a test gap. */
static void test_get_const_null_and_oob_return_null(void)
{
    setup();
    EXPECT(pool_get_const(NULL, 0) == NULL);      /* null pool */
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    pool_alloc(&g_pool);
    EXPECT(pool_get_const(&g_pool, 1)  == NULL);  /* used == 1, index 1 OOB */
    EXPECT(pool_get_const(&g_pool, 99) == NULL);
}

/* ── pool_reset ──────────────────────────────────────────────────────────── */

static void test_reset_clears_used(void)
{
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    pool_alloc(&g_pool);
    pool_alloc(&g_pool);
    pool_reset(&g_pool);
    EXPECT(pool_used(&g_pool)      == 0);
    EXPECT(pool_remaining(&g_pool) == 4);
    EXPECT(pool_is_empty(&g_pool));
}

static void test_reset_allows_reuse(void)
{
    Vec2* a;
    Vec2* b;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    a = pool_alloc_type(&g_pool, Vec2);
    EXPECT_NOT_NULL(a);
    pool_reset(&g_pool);
    b = pool_alloc_type(&g_pool, Vec2);
    EXPECT_NOT_NULL(b);
    /* After reset the first slot is reused — same address */
    EXPECT(b == a);
}

static void test_reset_null_safe(void)
{
    pool_reset(NULL);
    EXPECT(1);
}

/* Reset must preserve base_mark/end_mark even when the pool was created at an
 * unaligned offset — the re-reservation re-pads from an already-aligned
 * base_mark (pad == 0), so the region lands at the same place. */
static void test_reset_unaligned_base_stable(void)
{
    void*     throwaway;
    ArenaMark base_before;
    ArenaMark end_before;
    Vec2*     a;
    Vec2*     b;
    setup();
    throwaway = arena_alloc(&g_arena, 1);
    EXPECT_NOT_NULL(throwaway);
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    base_before = g_pool.base_mark;
    end_before  = g_pool.end_mark;
    a = pool_alloc_type(&g_pool, Vec2);
    EXPECT_NOT_NULL(a);
    pool_reset(&g_pool);
    EXPECT(g_pool.base_mark == base_before);
    EXPECT(g_pool.end_mark  == end_before);
    b = pool_alloc_type(&g_pool, Vec2);
    EXPECT_NOT_NULL(b);
    EXPECT(b == a);  /* same slot reused at same address */
}

/* ── pool_reset_secure ───────────────────────────────────────────────────── */

static void test_reset_secure_wipes_memory(void)
{
    Vec2* v;
    u8*   raw;
    usize i;
    usize obj_size;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    v = pool_alloc_type(&g_pool, Vec2);
    EXPECT_NOT_NULL(v);
    v->x = 0x12345678;
    v->y = 0x9ABCDEF0;
    raw      = (u8*)v;
    obj_size = pool_object_size(&g_pool);
    pool_reset_secure(&g_pool);
    EXPECT(pool_used(&g_pool) == 0);
    for (i = 0; i < obj_size; i++) EXPECT(raw[i] == 0);
}

static void test_reset_secure_null_safe(void)
{
    pool_reset_secure(NULL);
    EXPECT(1);
}

/* Exercises pool_reset_secure's empty-pool path (gcov L644 cond 2): a
 * non-null pool with used == 0 must route through the early pool_reset
 * branch without calling mem_secure_zero. The baseline only covered the
 * used > 0 secure-wipe path (test_reset_secure_wipes_memory). */
static void test_reset_secure_empty_pool(void)
{
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    pool_reset_secure(&g_pool);                   /* used == 0 → early path */
    EXPECT(pool_used(&g_pool) == 0);
    EXPECT(pool_is_empty(&g_pool));
    EXPECT(pool_alloc(&g_pool) != NULL);          /* still usable after */
}

/* ── query functions ─────────────────────────────────────────────────────── */

static void test_query_null_safe(void)
{
    EXPECT(pool_used(NULL)            == 0);
    EXPECT(pool_capacity(NULL)        == 0);
    EXPECT(pool_remaining(NULL)       == 0);
    EXPECT(pool_is_full(NULL)         == true);
    EXPECT(pool_is_empty(NULL)        == true);
    EXPECT(pool_object_size(NULL)     == 0);
    EXPECT(pool_memory_used(NULL)     == 0);
    EXPECT(pool_memory_reserved(NULL) == 0);
}

static void test_query_memory_used(void)
{
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 8);
    pool_alloc(&g_pool);
    pool_alloc(&g_pool);
    EXPECT(pool_memory_used(&g_pool)     == pool_object_size(&g_pool) * 2);
    EXPECT(pool_memory_reserved(&g_pool) == pool_object_size(&g_pool) * 8);
}

static void test_query_remaining_decreases(void)
{
    usize before;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 8);
    before = pool_remaining(&g_pool);
    pool_alloc(&g_pool);
    EXPECT(pool_remaining(&g_pool) == before - 1);
    EXPECT(pool_used(&g_pool) + pool_remaining(&g_pool) == pool_capacity(&g_pool));
}

/* Exercises pool_is_empty's false outcomes (gcov L510): the baseline only
 * called pool_is_empty on empty pools (post-init / post-reset), leaving the
 * (non-null, used != 0) vector unexercised — the single vector needed for
 * BOTH conditions' independence under masking MC/DC. One call closes both. */
static void test_is_empty_false_when_used(void)
{
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    pool_alloc(&g_pool);
    EXPECT(!pool_is_empty(&g_pool));
    EXPECT(pool_used(&g_pool) == 1);
}

/* ── byte views ──────────────────────────────────────────────────────────── */

static void test_as_bytes_empty(void)
{
    bytes_t b;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    b = pool_as_bytes(&g_pool);
    EXPECT(b.ptr == NULL && b.len == 0);
}

static void test_as_bytes_after_alloc(void)
{
    bytes_t b;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    pool_alloc(&g_pool);
    pool_alloc(&g_pool);
    b = pool_as_bytes(&g_pool);
    EXPECT(b.ptr != NULL);
    EXPECT(b.len == pool_object_size(&g_pool) * 2);
}

static void test_reserved_bytes(void)
{
    bytes_t b;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    b = pool_reserved_bytes(&g_pool);
    EXPECT(b.ptr != NULL);
    EXPECT(b.len == pool_object_size(&g_pool) * 4);
}

static void test_as_bytes_null_safe(void)
{
    bytes_t b = pool_as_bytes(NULL);
    EXPECT(b.ptr == NULL && b.len == 0);
}

/* Exercises pool_reserved_bytes's null-pool outcome (gcov L557 cond 0),
 * mirroring test_as_bytes_null_safe for pool_as_bytes. The cond 1
 * (!pool->arena) outcome stays uncovered — unreachable under pool_invariant,
 * same as pool_as_bytes. */
static void test_reserved_bytes_null_safe(void)
{
    bytes_t b = pool_reserved_bytes(NULL);
    EXPECT(b.ptr == NULL && b.len == 0);
}

/* ── multiple pools from same arena ─────────────────────────────────────── */

static void test_two_pools_same_arena(void)
{
    Pool  pool_a;
    Pool  pool_b;
    Vec2* va;
    i32*  vb;
    bool  ok_a;
    bool  ok_b;
    setup();
    ok_a = pool_init_type(&pool_a, &g_arena, Vec2, 4);
    ok_b = pool_init_type(&pool_b, &g_arena, i32,  8);
    EXPECT(ok_a && ok_b);
    va = pool_alloc_type(&pool_a, Vec2);
    vb = pool_alloc_type(&pool_b, i32);
    EXPECT_NOT_NULL(va);
    EXPECT_NOT_NULL(vb);
    va->x = 1; va->y = 2;
    *vb   = 99;
    EXPECT(va->x == 1 && va->y == 2);
    EXPECT(*vb == 99);
    /* regions must not overlap */
    EXPECT((u8*)vb >= (u8*)va + pool_object_size(&pool_a) * 4 ||
           (u8*)va >= (u8*)vb + pool_object_size(&pool_b) * 8);
}

/* ── type-safe macro round-trip ─────────────────────────────────────────── */

static void test_type_macro_round_trip(void)
{
    Vec2* w;
    Vec2* got;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 8);
    w = pool_alloc_type_zero(&g_pool, Vec2);
    EXPECT_NOT_NULL(w);
    EXPECT(w->x == 0 && w->y == 0);
    w->x = 42; w->y = 99;
    got = pool_get_type(&g_pool, 0, Vec2);
    EXPECT_NOT_NULL(got);
    EXPECT(got->x == 42 && got->y == 99);
}

/* ── CANON_LIFETIME_DEBUG: lifetime token semantics ──────────────────────── *
 *
 * These tests exercise the (id, open) lifetime token embedded on the Pool
 * when CANON_LIFETIME_DEBUG is defined. They verify state-level invariants
 * directly on g_pool.lt — they do NOT construct borrows or trigger
 * lifetime_assert_valid. The borrow-side validation path (fires require_msg
 * after reset) is covered in test/semantics/borrow_test.c.
 *
 *   - pool_init opens the lifetime: lt.open == true, lt.id is set.
 *   - pool_reset re-stamps lt.id: new ID differs from old ID.
 *   - pool_reset_secure also re-stamps lt.id.
 *
 * There is no pool_reset_to equivalent (pool has no rollback API), so no
 * preserve-on-rollback test is needed — unlike arena.
 */

#ifdef CANON_LIFETIME_DEBUG

static void test_lifetime_init_opens_token(void)
{
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    EXPECT(g_pool.lt.open == true);
    EXPECT(g_pool.lt.id != REGION_ID_STATIC);
}

static void test_lifetime_reset_restamps_id(void)
{
    region_id_t before;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    before = g_pool.lt.id;
    pool_reset(&g_pool);
    EXPECT(g_pool.lt.id != before);
    EXPECT(g_pool.lt.open == true); /* reset recycles, does not close */
}

static void test_lifetime_reset_secure_restamps_id(void)
{
    region_id_t before;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    pool_alloc(&g_pool);
    before = g_pool.lt.id;
    pool_reset_secure(&g_pool);
    EXPECT(g_pool.lt.id != before);
    EXPECT(g_pool.lt.open == true);
}

/* OWN-002 no-cycle property: two consecutive resets must produce three
 * distinct IDs (no XOR-with-constant A -> A^K -> A cycle). Mirrors the
 * arena/pool regression intent from OWN-002. */
static void test_lifetime_two_resets_no_cycle(void)
{
    region_id_t id0;
    region_id_t id1;
    region_id_t id2;
    setup();
    pool_init_type(&g_pool, &g_arena, Vec2, 4);
    id0 = g_pool.lt.id;
    pool_reset(&g_pool);
    id1 = g_pool.lt.id;
    pool_reset(&g_pool);
    id2 = g_pool.lt.id;
    EXPECT(id0 != id1);
    EXPECT(id1 != id2);
    EXPECT(id0 != id2);  /* the property the XOR-with-constant scheme violated */
}

#endif /* CANON_LIFETIME_DEBUG */

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    /* pool_init */
    test_init_basic();
    test_init_reserves_arena_space();
    test_init_arena_too_small();
    test_init_type_macro();
    test_init_post_arena_alloc_safe();
    test_init_unaligned_base_no_overrun();   /* regression */
    test_init_object_size_alignment_overflow();  /* MC/DC gap */
    test_init_reservation_mul_overflow();        /* MC/DC gap */

    /* pool_alloc */
    test_alloc_returns_non_null();
    test_alloc_sequential_non_overlapping();
    test_alloc_exhaustion_returns_null();
    test_alloc_fills_used();

    /* pool_alloc_zero */
    test_alloc_zero_is_zeroed();
    test_alloc_zero_full_returns_null();     /* MC/DC gap */

    /* pool_try_alloc */
    test_try_alloc_success();
    test_try_alloc_failure();
    test_try_alloc_zero_success();
    test_try_alloc_null_out();               /* MC/DC gap */
    test_try_alloc_zero_null_out();          /* MC/DC gap */

    /* pool_get */
    test_get_returns_correct_slot();
    test_get_out_of_bounds_returns_null();
    test_get_null_pool_returns_null();
    test_get_const_returns_correct_slot();
    test_get_const_null_and_oob_return_null();   /* MC/DC gap */

    /* pool_reset */
    test_reset_clears_used();
    test_reset_allows_reuse();
    test_reset_null_safe();
    test_reset_unaligned_base_stable();      /* regression companion */

    /* pool_reset_secure */
    test_reset_secure_wipes_memory();
    test_reset_secure_null_safe();
    test_reset_secure_empty_pool();          /* MC/DC gap */

    /* query */
    test_query_null_safe();
    test_query_memory_used();
    test_query_remaining_decreases();
    test_is_empty_false_when_used();         /* MC/DC gap */

    /* byte views */
    test_as_bytes_empty();
    test_as_bytes_after_alloc();
    test_reserved_bytes();
    test_as_bytes_null_safe();
    test_reserved_bytes_null_safe();         /* MC/DC gap */

    /* multiple pools */
    test_two_pools_same_arena();

    /* type-safe macros */
    test_type_macro_round_trip();

#ifdef CANON_LIFETIME_DEBUG
    test_lifetime_init_opens_token();
    test_lifetime_reset_restamps_id();
    test_lifetime_reset_secure_restamps_id();
    test_lifetime_two_resets_no_cycle();
#endif

    if (g_failed == 0) {
        printf("OK  pool_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  pool_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ────────────────────────────────────────────────────── */
/*
 * Input layout:
 *   [0]    u8   obj_size_raw  — object size 1..64 (raw % 63 + 1)
 *   [1]    u8   capacity_raw  — pool capacity 1..32 (raw % 31 + 1)
 *   [2]    u8   n_allocs_raw  — number of allocs to attempt (raw % 33)
 *   [3]    u8   do_reset      — if non-zero, reset after allocs
 *   [4]    u8   n_allocs2_raw — allocs after reset (raw % 33)
 *   [5]    u8   prepad_raw    — pre-pad the arena 0..15 bytes (raw % 16) so
 *                              pool base_mark is exercised at every alignment
 */

#define FUZZ_ARENA_SIZE ((usize)8192)

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static u8 fuzz_buf[FUZZ_ARENA_SIZE];
    Arena     arena;
    Pool      pool;
    u8        raw[6];
    usize     obj_size;
    usize     capacity;
    usize     n_allocs;
    usize     n_allocs2;
    usize     prepad;
    bool      do_reset;
    usize     i;
    bool      ok;

    memset(raw, 0, sizeof(raw));
    if (size > sizeof(raw)) size = sizeof(raw);
    memcpy(raw, data, size);

    obj_size  = (usize)(raw[0] % 63u) + 1u;
    capacity  = (usize)(raw[1] % 31u) + 1u;
    n_allocs  = (usize)(raw[2] % 33u);
    do_reset  = raw[3] != 0;
    n_allocs2 = (usize)(raw[4] % 33u);
    prepad    = (usize)(raw[5] % 16u);

    arena_init(&arena, fuzz_buf, FUZZ_ARENA_SIZE);

    /* Push the arena to an arbitrary offset before pool_init so base_mark
     * is exercised at every alignment, not just the 16-aligned start. */
    if (prepad > 0) {
        if (arena_alloc(&arena, prepad) == NULL) return 0;
    }

    ok = pool_init(&pool, &arena, obj_size, capacity);
    if (!ok) return 0;

    /* First wave of allocations */
    for (i = 0; i < n_allocs; i++) {
        void* p = pool_alloc(&pool);
        if (p == NULL) {
            if (pool_used(&pool) < pool_capacity(&pool)) __builtin_trap();
            break;
        }
        /* Verify pointer is inside the reserved region */
        if ((u8*)p < fuzz_buf + pool.base_mark)   __builtin_trap();
        if ((u8*)p >= fuzz_buf + pool.end_mark)   __builtin_trap();
        /* Verify the object's END also stays within the reserved region —
         * this is the assertion that the base_mark fix protects. */
        if ((u8*)p + pool_object_size(&pool) > fuzz_buf + pool.end_mark)
            __builtin_trap();
        /* Verify used counter is consistent */
        if (pool_used(&pool) > pool_capacity(&pool)) __builtin_trap();
    }

    if (do_reset) {
        pool_reset(&pool);
        if (pool_used(&pool) != 0) __builtin_trap();

        /* Second wave after reset */
        for (i = 0; i < n_allocs2; i++) {
            void* p = pool_alloc_zero(&pool);
            if (p == NULL) break;
            if (!mem_is_zero(p, pool_object_size(&pool))) __builtin_trap();
            if ((u8*)p + pool_object_size(&pool) > fuzz_buf + pool.end_mark)
                __builtin_trap();
            if (pool_used(&pool) > pool_capacity(&pool))  __builtin_trap();
        }
    }

    /* pool_get consistency */
    for (i = 0; i < pool_used(&pool); i++) {
        void* p = pool_get(&pool, i);
        if (p == NULL)                           __builtin_trap();
        if ((u8*)p < fuzz_buf + pool.base_mark)  __builtin_trap();
        if ((u8*)p >= fuzz_buf + pool.end_mark)  __builtin_trap();
    }

    /* OOB get must return NULL */
    if (pool_get(&pool, pool_used(&pool)) != NULL) __builtin_trap();

    return 0;
}

#endif /* CANON_FUZZING */
