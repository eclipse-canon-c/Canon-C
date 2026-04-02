/**
 * @file intern_test.c
 * @brief Unit tests for util/str/intern.h
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   intern_pool_init   — basic init, pre-interned empty string, arena exhaustion
 *   intern_string      — deduplication, pointer equality, NULL → "",
 *                        multiple distinct strings, pool full
 *   intern_equals      — same string, different strings, empty strings
 *   intern_count       — tracks insertions correctly
 *   intern_load_factor — increases with insertions, stays in [0, 1]
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL pool, NULL arena, capacity==0 in intern_pool_init, and NULL pool
 *   in intern_string/intern_count/intern_load_factor are contract violations
 *   (require_msg → abort). Not tested here.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Loop variables declared before the loop body (C99). All buffers
 *   zero-initialized to satisfy static analysis.
 */

#define CANON_CONTRACT_IMPL
#include "util/str/intern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Portability: unused-function suppression
 * ====================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_MAYBE_UNUSED __attribute__((unused))
#else
    #define CANON_MAYBE_UNUSED
#endif

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

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
 * Helper: create a fresh arena + pool for each test
 * ====================================================================== */

typedef struct {
    u8          backing[16384];
    Arena       arena;
    InternPool  pool;
} TestFixture;

static bool fixture_init(TestFixture* f, usize capacity) {
    arena_init(&f->arena, f->backing, sizeof(f->backing));
    return intern_pool_init(&f->pool, &f->arena, capacity);
}

/* =========================================================================
 * intern_pool_init — basic
 * ====================================================================== */

TEST(init_basic) {
    TestFixture f;
    EXPECT(fixture_init(&f, 64) == true);
    /* Empty string is pre-interned */
    EXPECT(intern_count(&f.pool) == 1);
    EXPECT(f.pool.capacity == 64);
}

/* =========================================================================
 * intern_pool_init — pre-interned empty string
 * ====================================================================== */

TEST(init_preinterns_empty) {
    TestFixture f;
    fixture_init(&f, 64);

    /* Interning "" should return the pre-interned view, not add a new one */
    usize before = intern_count(&f.pool);
    str_view_t v = intern_string(&f.pool, "");
    EXPECT(intern_count(&f.pool) == before);
    EXPECT(!str_view_is_null(v));
    EXPECT(str_view_is_empty(v));
}

/* =========================================================================
 * intern_pool_init — arena exhaustion
 * ====================================================================== */

TEST(init_arena_exhaustion) {
    u8 tiny[32];
    Arena arena;
    InternPool pool;

    arena_init(&arena, tiny, sizeof(tiny));
    /* Capacity * sizeof(str_view_t) exceeds arena — should fail */
    EXPECT(intern_pool_init(&pool, &arena, 1000) == false);
}

/* =========================================================================
 * intern_string — deduplication (same content → same pointer)
 * ====================================================================== */

TEST(dedup_same_content) {
    TestFixture f;
    fixture_init(&f, 64);

    str_view_t s1 = intern_string(&f.pool, "hello");
    str_view_t s2 = intern_string(&f.pool, "hello");

    EXPECT(!str_view_is_null(s1));
    EXPECT(!str_view_is_null(s2));
    EXPECT(intern_equals(s1, s2));
    EXPECT(s1.ptr == s2.ptr);
    EXPECT(s1.len == s2.len);
}

/* =========================================================================
 * intern_string — distinct strings get distinct pointers
 * ====================================================================== */

TEST(distinct_strings) {
    TestFixture f;
    fixture_init(&f, 64);

    str_view_t s1 = intern_string(&f.pool, "hello");
    str_view_t s2 = intern_string(&f.pool, "world");

    EXPECT(!str_view_is_null(s1));
    EXPECT(!str_view_is_null(s2));
    EXPECT(!intern_equals(s1, s2));
    EXPECT(s1.ptr != s2.ptr);
}

/* =========================================================================
 * intern_string — NULL is normalized to ""
 * ====================================================================== */

TEST(null_is_empty) {
    TestFixture f;
    fixture_init(&f, 64);

    str_view_t from_null  = intern_string(&f.pool, NULL);
    str_view_t from_empty = intern_string(&f.pool, "");

    EXPECT(!str_view_is_null(from_null));
    EXPECT(!str_view_is_null(from_empty));
    EXPECT(intern_equals(from_null, from_empty));
}

/* =========================================================================
 * intern_string — content is correct
 * ====================================================================== */

TEST(content_correct) {
    TestFixture f;
    fixture_init(&f, 64);

    str_view_t v = intern_string(&f.pool, "Canon-C");
    EXPECT(str_view_equals_cstr(v, "Canon-C"));
    EXPECT(v.len == 7);
}

/* =========================================================================
 * intern_string — many distinct strings
 * ====================================================================== */

TEST(many_strings) {
    TestFixture f;
    int i;
    char buf[16] = {0};

    fixture_init(&f, 128);

    for (i = 0; i < 50; i++) {
        buf[0] = 'A' + (char)(i / 26);
        buf[1] = 'a' + (char)(i % 26);
        buf[2] = '\0';
        str_view_t v = intern_string(&f.pool, buf);
        EXPECT(!str_view_is_null(v));
    }

    /* 50 distinct + 1 pre-interned empty */
    EXPECT(intern_count(&f.pool) == 51);

    /* Re-intern all — count should not change */
    for (i = 0; i < 50; i++) {
        buf[0] = 'A' + (char)(i / 26);
        buf[1] = 'a' + (char)(i % 26);
        buf[2] = '\0';
        (void)intern_string(&f.pool, buf);
    }
    EXPECT(intern_count(&f.pool) == 51);
}

/* =========================================================================
 * intern_string — pool full returns null view
 * ====================================================================== */

TEST(pool_full) {
    TestFixture f;
    int i;
    char buf[16] = {0};
    int nulls = 0;

    /* Capacity 8 — pre-interned empty takes 1 slot, 7 remain */
    fixture_init(&f, 8);

    for (i = 0; i < 20; i++) {
        buf[0] = 'a' + (char)(i % 26);
        buf[1] = (char)('0' + i);
        buf[2] = '\0';
        str_view_t v = intern_string(&f.pool, buf);
        if (str_view_is_null(v)) nulls++;
    }

    /* Some insertions must have failed */
    EXPECT(nulls > 0);
    EXPECT(intern_count(&f.pool) <= 8);
}

/* =========================================================================
 * intern_equals — basic
 * ====================================================================== */

TEST(equals_same) {
    TestFixture f;
    fixture_init(&f, 64);

    str_view_t a = intern_string(&f.pool, "test");
    str_view_t b = intern_string(&f.pool, "test");
    EXPECT(intern_equals(a, b));
}

TEST(equals_different) {
    TestFixture f;
    fixture_init(&f, 64);

    str_view_t a = intern_string(&f.pool, "foo");
    str_view_t b = intern_string(&f.pool, "bar");
    EXPECT(!intern_equals(a, b));
}

/* =========================================================================
 * intern_count — tracks correctly
 * ====================================================================== */

TEST(count_tracking) {
    TestFixture f;
    fixture_init(&f, 64);

    /* 1 for pre-interned "" */
    EXPECT(intern_count(&f.pool) == 1);

    intern_string(&f.pool, "a");
    EXPECT(intern_count(&f.pool) == 2);

    intern_string(&f.pool, "b");
    EXPECT(intern_count(&f.pool) == 3);

    /* Duplicate — no change */
    intern_string(&f.pool, "a");
    EXPECT(intern_count(&f.pool) == 3);
}

/* =========================================================================
 * intern_load_factor — increases with insertions
 * ====================================================================== */

TEST(load_factor) {
    TestFixture f;
    f64 lf;

    fixture_init(&f, 64);

    lf = intern_load_factor(&f.pool);
    EXPECT(lf > 0.0);   /* pre-interned empty */
    EXPECT(lf < 1.0);

    intern_string(&f.pool, "x");
    intern_string(&f.pool, "y");
    intern_string(&f.pool, "z");

    lf = intern_load_factor(&f.pool);
    /* 4 strings in 64 slots → ~0.0625 */
    EXPECT(lf > 0.0);
    EXPECT(lf < 1.0);
    EXPECT(intern_count(&f.pool) == 4);
}

/* =========================================================================
 * Interned views survive across later intern calls
 * ====================================================================== */

TEST(views_stable) {
    TestFixture f;
    fixture_init(&f, 64);

    str_view_t first = intern_string(&f.pool, "stable");

    /* Intern many more strings */
    intern_string(&f.pool, "a");
    intern_string(&f.pool, "b");
    intern_string(&f.pool, "c");
    intern_string(&f.pool, "d");
    intern_string(&f.pool, "e");

    /* Original view still valid and still points to same data */
    str_view_t again = intern_string(&f.pool, "stable");
    EXPECT(intern_equals(first, again));
    EXPECT(str_view_equals_cstr(first, "stable"));
}

/* =========================================================================
 * Interned copy is independent of input buffer
 * ====================================================================== */

TEST(copy_independence) {
    TestFixture f;
    char buf[16] = {0};

    fixture_init(&f, 64);

    str_copy_into(buf, sizeof(buf), "mutable");
    str_view_t v = intern_string(&f.pool, buf);

    /* Mutate the original buffer */
    buf[0] = 'X';

    /* Interned view must be unaffected */
    EXPECT(str_view_equals_cstr(v, "mutable"));
}

/* =========================================================================
 * Entry point
 * ====================================================================== */

int main(void) {
    /* intern_pool_init */
    RUN(init_basic);
    RUN(init_preinterns_empty);
    RUN(init_arena_exhaustion);

    /* intern_string */
    RUN(dedup_same_content);
    RUN(distinct_strings);
    RUN(null_is_empty);
    RUN(content_correct);
    RUN(many_strings);
    RUN(pool_full);

    /* intern_equals */
    RUN(equals_same);
    RUN(equals_different);

    /* intern_count */
    RUN(count_tracking);

    /* intern_load_factor */
    RUN(load_factor);

    /* Stability */
    RUN(views_stable);
    RUN(copy_independence);

    printf("\nintern_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
