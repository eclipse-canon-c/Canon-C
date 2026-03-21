/**
 * @file region_test.c
 * @brief Tests for region.h — explicit lifetime region tokens
 *
 * Covers:
 *   - region_begin: initialization, ID assignment, open state
 *   - region_end: closes region, calls hooks LIFO, resets arena
 *   - region_attach_arena: arena reset on region_end
 *   - region_register: hook registration, LIFO call order, table full
 *   - region_set_parent / region_has_parent
 *   - region_id / region_is_open / region_hook_count
 *   - region_assert_open / region_assert_borrow_valid (debug builds)
 *   - REGION_ID_STATIC: static lifetime never expires
 *   - Nested regions
 *   - CANON_NO_REGION_PARENT: region_has_parent always false
 */

/* Must be defined in exactly one TU before including contract.h (via region.h) */
#define CANON_CONTRACT_IMPL
#include "core/region.h"

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

/* ── region_begin ────────────────────────────────────────────────────────── */

static void test_begin_sets_open(void)
{
    Region r;
    region_begin(&r);
    EXPECT(region_is_open(&r));
    region_end(&r);
}

static void test_begin_id_is_address(void)
{
    Region r;
    region_begin(&r);
    EXPECT(r.id == (region_id_t)(uintptr_t)&r);
    EXPECT(r.id != REGION_ID_STATIC);
    region_end(&r);
}

static void test_begin_clears_arena(void)
{
    Region r;
    region_begin(&r);
    EXPECT(r.arena == NULL);
    region_end(&r);
}

static void test_begin_clears_hooks(void)
{
    Region r;
    region_begin(&r);
    EXPECT(r.num_hooks == 0);
    region_end(&r);
}

static void test_begin_two_regions_distinct_ids(void)
{
    Region a;
    Region b;
    region_begin(&a);
    region_begin(&b);
    EXPECT(a.id != b.id);
    region_end(&b);
    region_end(&a);
}

/* ── region_end ──────────────────────────────────────────────────────────── */

static void test_end_sets_closed(void)
{
    Region r;
    region_begin(&r);
    region_end(&r);
    EXPECT(!region_is_open(&r));
}

static void test_end_preserves_id(void)
{
    Region      r;
    region_id_t id;
    region_begin(&r);
    id = r.id;
    region_end(&r);
    EXPECT(r.id == id);
}

static void test_end_clears_hooks(void)
{
    Region r;
    region_begin(&r);
    region_end(&r);
    EXPECT(region_hook_count(&r) == 0);
}

/* ── region_attach_arena ─────────────────────────────────────────────────── */

static void test_attach_arena_reset_on_end(void)
{
    u8    buf[256];
    Arena arena;
    Region r;

    arena_init(&arena, buf, sizeof(buf));
    arena_alloc(&arena, 64);
    EXPECT(arena_used(&arena) > 0);

    region_begin(&r);
    region_attach_arena(&r, &arena);
    region_end(&r);

    EXPECT(arena_used(&arena) == 0);
    EXPECT(r.arena == NULL);
}

static void test_attach_arena_null_after_end(void)
{
    u8    buf[64];
    Arena arena;
    Region r;

    arena_init(&arena, buf, sizeof(buf));
    region_begin(&r);
    region_attach_arena(&r, &arena);
    region_end(&r);
    EXPECT(r.arena == NULL);
}

static void test_attach_arena_replace(void)
{
    u8    buf1[64];
    u8    buf2[64];
    Arena a1;
    Arena a2;
    Region r;

    arena_init(&a1, buf1, sizeof(buf1));
    arena_init(&a2, buf2, sizeof(buf2));
    arena_alloc(&a2, 32);

    region_begin(&r);
    region_attach_arena(&r, &a1);
    region_attach_arena(&r, &a2);   /* replace — a1 NOT reset */
    region_end(&r);

    /* a2 was the final attachment — it gets reset */
    EXPECT(arena_used(&a2) == 0);
}

/* ── region_register / cleanup hooks ────────────────────────────────────── */

static int g_hook_order[REGION_MAX_CLEANUP + 1];
static int g_hook_count = 0;

static void hook_record(void* ctx)
{
    int id = (int)(uintptr_t)ctx;
    if (g_hook_count < REGION_MAX_CLEANUP) {
        g_hook_order[g_hook_count++] = id;
    }
}

static void test_hooks_called_on_end(void)
{
    Region r;
    g_hook_count = 0;

    region_begin(&r);
    region_register(&r, hook_record, (void*)(uintptr_t)1);
    region_end(&r);

    EXPECT(g_hook_count == 1);
    EXPECT(g_hook_order[0] == 1);
}

static void test_hooks_called_lifo(void)
{
    Region r;
    g_hook_count = 0;

    region_begin(&r);
    region_register(&r, hook_record, (void*)(uintptr_t)1);
    region_register(&r, hook_record, (void*)(uintptr_t)2);
    region_register(&r, hook_record, (void*)(uintptr_t)3);
    region_end(&r);

    EXPECT(g_hook_count    == 3);
    EXPECT(g_hook_order[0] == 3);
    EXPECT(g_hook_order[1] == 2);
    EXPECT(g_hook_order[2] == 1);
}

static void test_hooks_cleared_after_end(void)
{
    Region r;
    region_begin(&r);
    region_register(&r, hook_record, (void*)(uintptr_t)1);
    region_end(&r);
    EXPECT(region_hook_count(&r) == 0);
}

static void test_register_returns_false_when_full(void)
{
    Region r;
    usize  i;
    bool   ok;

    region_begin(&r);
    for (i = 0; i < (usize)REGION_MAX_CLEANUP; i++) {
        ok = region_register(&r, hook_record, NULL);
        EXPECT(ok);
    }
    /* One more — must fail */
    ok = region_register(&r, hook_record, NULL);
    EXPECT(!ok);
    EXPECT(region_hook_count(&r) == (usize)REGION_MAX_CLEANUP);
    region_end(&r);
}

static void test_hook_ctx_null_ok(void)
{
    /* Hook with NULL ctx must not crash */
    static int called = 0;
    void hook_null_ctx(void* ctx) { (void)ctx; called = 1; }
    Region r;
    called = 0;
    region_begin(&r);
    region_register(&r, hook_null_ctx, NULL);
    region_end(&r);
    EXPECT(called == 1);
}

/* ── hooks run before arena reset ────────────────────────────────────────── */

static u8    g_hook_arena_buf[256];
static Arena g_hook_arena;
static int   g_hook_saw_alloc = 0;

static void hook_check_arena_live(void* ctx)
{
    Arena* a = (Arena*)ctx;
    /* arena must still be valid inside a hook */
    g_hook_saw_alloc = (arena_remaining(a) > 0) ? 1 : 0;
}

static void test_hooks_run_before_arena_reset(void)
{
    Region r;
    arena_init(&g_hook_arena, g_hook_arena_buf, sizeof(g_hook_arena_buf));
    arena_alloc(&g_hook_arena, 32);
    g_hook_saw_alloc = 0;

    region_begin(&r);
    region_attach_arena(&r, &g_hook_arena);
    region_register(&r, hook_check_arena_live, &g_hook_arena);
    region_end(&r);

    EXPECT(g_hook_saw_alloc == 1);
    EXPECT(arena_used(&g_hook_arena) == 0);
}

/* ── region_id / region_is_open / region_hook_count — NULL safety ────────── */

static void test_query_null_safe(void)
{
    EXPECT(region_id(NULL)         == REGION_ID_STATIC);
    EXPECT(region_is_open(NULL)    == false);
    EXPECT(region_hook_count(NULL) == 0);
    EXPECT(region_has_parent(NULL) == false);
}

static void test_region_is_open_false_before_begin(void)
{
    Region r;
    memset(&r, 0, sizeof(r));
    EXPECT(!region_is_open(&r));
}

/* ── region_assert_open / region_assert_borrow_valid ─────────────────────── */

static void test_assert_open_passes_when_open(void)
{
    Region r;
    region_begin(&r);
    region_assert_open(&r);   /* must not abort */
    EXPECT(1);
    region_end(&r);
}

static void test_assert_borrow_valid_static_lifetime(void)
{
    Region r;
    region_begin(&r);
    /* REGION_ID_STATIC always valid — no assertion */
    region_assert_borrow_valid(&r, REGION_ID_STATIC);
    EXPECT(1);
    region_end(&r);
}

static void test_assert_borrow_valid_matching_id(void)
{
    Region      r;
    region_id_t rid;
    region_begin(&r);
    rid = region_id(&r);
    region_assert_borrow_valid(&r, rid);   /* must not abort */
    EXPECT(1);
    region_end(&r);
}

/* ── parent tracking ─────────────────────────────────────────────────────── */

static void test_parent_tracking(void)
{
#ifdef CANON_NO_REGION_PARENT
    EXPECT(1);   /* parent tracking compiled out — skip */
#else
    Region outer;
    Region inner;

    region_begin(&outer);
    region_begin(&inner);

    EXPECT(!region_has_parent(&inner));
    region_set_parent(&inner, &outer);
    EXPECT(region_has_parent(&inner));
    EXPECT(inner.parent == &outer);

    region_end(&inner);
    region_end(&outer);
#endif
}

static void test_has_parent_false_without_set(void)
{
    Region r;
    region_begin(&r);
    EXPECT(!region_has_parent(&r));
    region_end(&r);
}

/* ── nested regions ──────────────────────────────────────────────────────── */

static void test_nested_regions_independent(void)
{
    u8    buf_outer[256];
    u8    buf_inner[128];
    Arena outer_arena;
    Arena inner_arena;
    Region outer;
    Region inner;

    arena_init(&outer_arena, buf_outer, sizeof(buf_outer));
    arena_init(&inner_arena, buf_inner, sizeof(buf_inner));
    arena_alloc(&outer_arena, 64);
    arena_alloc(&inner_arena, 32);

    region_begin(&outer);
    region_attach_arena(&outer, &outer_arena);

    region_begin(&inner);
    region_attach_arena(&inner, &inner_arena);

    /* close inner — only inner arena reset */
    region_end(&inner);
    EXPECT(arena_used(&inner_arena) == 0);
    EXPECT(arena_used(&outer_arena) > 0);
    EXPECT(region_is_open(&outer));

    /* close outer — outer arena reset */
    region_end(&outer);
    EXPECT(arena_used(&outer_arena) == 0);
}

static void test_nested_ids_distinct(void)
{
    Region outer;
    Region inner;
    region_begin(&outer);
    region_begin(&inner);
    EXPECT(outer.id != inner.id);
    EXPECT(outer.id != REGION_ID_STATIC);
    EXPECT(inner.id != REGION_ID_STATIC);
    region_end(&inner);
    region_end(&outer);
}

/* ── reuse after end ─────────────────────────────────────────────────────── */

static void test_reuse_after_end(void)
{
    Region r;
    region_begin(&r);
    region_end(&r);
    EXPECT(!region_is_open(&r));

    /* Re-open the same Region variable */
    region_begin(&r);
    EXPECT(region_is_open(&r));
    EXPECT(r.id == (region_id_t)(uintptr_t)&r);
    region_end(&r);
    EXPECT(!region_is_open(&r));
}

/* ── REGION_ID_STATIC ────────────────────────────────────────────────────── */

static void test_region_id_static_is_zero(void)
{
    EXPECT(REGION_ID_STATIC == (region_id_t)0);
}

static void test_live_region_id_not_static(void)
{
    Region r;
    region_begin(&r);
    EXPECT(region_id(&r) != REGION_ID_STATIC);
    region_end(&r);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    /* region_begin */
    test_begin_sets_open();
    test_begin_id_is_address();
    test_begin_clears_arena();
    test_begin_clears_hooks();
    test_begin_two_regions_distinct_ids();

    /* region_end */
    test_end_sets_closed();
    test_end_preserves_id();
    test_end_clears_hooks();

    /* region_attach_arena */
    test_attach_arena_reset_on_end();
    test_attach_arena_null_after_end();
    test_attach_arena_replace();

    /* region_register / hooks */
    test_hooks_called_on_end();
    test_hooks_called_lifo();
    test_hooks_cleared_after_end();
    test_register_returns_false_when_full();
    test_hook_ctx_null_ok();
    test_hooks_run_before_arena_reset();

    /* query — NULL safety */
    test_query_null_safe();
    test_region_is_open_false_before_begin();

    /* assert_open / assert_borrow_valid */
    test_assert_open_passes_when_open();
    test_assert_borrow_valid_static_lifetime();
    test_assert_borrow_valid_matching_id();

    /* parent tracking */
    test_parent_tracking();
    test_has_parent_false_without_set();

    /* nested regions */
    test_nested_regions_independent();
    test_nested_ids_distinct();

    /* reuse */
    test_reuse_after_end();

    /* REGION_ID_STATIC */
    test_region_id_static_is_zero();
    test_live_region_id_not_static();

    if (g_failed == 0) {
        printf("OK  region_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  region_test  (%d assertion(s) failed)\n",
            g_failed);
    return 1;
}
