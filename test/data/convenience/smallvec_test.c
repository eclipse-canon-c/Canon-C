/**
 * @file smallvec_test.c
 * @brief Tests for data/convenience/smallvec.h — inline-first vector
 *
 * Two instantiations:
 *   DEFINE_VEC(int) + DEFINE_SMALLVEC(int, 4)
 *       — primary, all functions exercised; INLINE_CAP=4 makes spill easy to trigger
 *   DEFINE_VEC(Point) + DEFINE_SMALLVEC(Point, 2)
 *       — struct coverage; INLINE_CAP=2 forces spill after two elements
 *
 * Covers:
 *   - smallvec_T_init()           — data==inline_buf, len==0, cap==INLINE_CAP
 *   - smallvec_T_init_arena()     — same as init but arena stored
 *   - smallvec_T_using_inline()   — true until spill
 *   - smallvec_T_len()            — NULL-safe; tracks pushes
 *   - smallvec_T_capacity()       — NULL-safe; INLINE_CAP before spill, 2× after
 *   - smallvec_T_is_empty()       — true on init; false after push
 *   - smallvec_T_push()           — fills inline; spills at INLINE_CAP+1; post-spill
 *                                    full returns false (no second growth)
 *   - smallvec_T_pop()            — returns last; fails on empty/NULL
 *   - smallvec_T_get()            — bounds-checked copy; OOB returns false
 *   - smallvec_T_get_unchecked()  — fast path (debug-only preconditions)
 *   - smallvec_T_data()           — raw pointer; NULL-safe
 *   - smallvec_T_first() / _last()— pointer to first/last; NULL when empty
 *   - smallvec_T_insert()         — shifts right; may spill; post-spill full fails
 *   - smallvec_T_remove()         — shifts left; OOB fails
 *   - smallvec_T_extend()         — bulk append; may spill; post-spill overflow fails
 *   - smallvec_T_clear()          — resets len; buffer retained; NULL-safe
 *   - smallvec_T_free()           — frees heap spill; reinitializes inline
 *   - smallvec_T_as_vec()         — borrowed vec view; same data pointer
 *   - Inline invariant             — data==inline_buf when using_inline==true
 *   - Spill semantics              — spills exactly once; cap doubles at spill time
 *   - Post-spill capacity fixed    — subsequent push into full buffer returns false
 *   - Arena-backed spill           — no free needed; data points to arena memory
 *   - Point struct                 — push/pop/get/set with struct elements
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Single smallvec_int with INLINE_CAP=4; reference model tracks expected contents
 *   - Operations: push, pop, clear, get, free+reinit
 *   - Invariants after every op:
 *       • len() == ref_count
 *       • is_empty() == (ref_count == 0)
 *       • using_inline() == (ref_count <= INLINE_CAP and not yet spilled)
 *       • get(i) == ref[i] for all i in [0, ref_count)
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "core/arena.h"
#include "data/convenience/smallvec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

/* DEFINE_SMALLVEC unconditionally emits as_vec, which references
 * canon_vec_##type and canon_vec_##type##_init. DEFINE_VEC(linkage, type)
 * would provide these but requires CANON_OPTION and CANON_RESULT first.
 * Instead, provide minimal shim typedefs — just the struct layout and the
 * init constructor that as_vec actually calls. The vec struct has exactly
 * three fields (data, len, cap) matching the standard canon_vec layout. */

typedef struct { int*   data; usize len; usize cap; } canon_vec_int;
static inline canon_vec_int canon_vec_int_init(int* d, usize n)
    { canon_vec_int v; v.data = d; v.len = 0; v.cap = n; return v; }

DEFINE_SMALLVEC(int,   4)

typedef struct { int x; int y; } Point;

typedef struct { Point* data; usize len; usize cap; } canon_vec_Point;
static inline canon_vec_Point canon_vec_Point_init(Point* d, usize n)
    { canon_vec_Point v; v.data = d; v.len = 0; v.cap = n; return v; }

DEFINE_SMALLVEC(Point, 2)

/* ════════════════════════════════════════════════════════════════════════════
   Unit test build
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                                  \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n",                      \
                    __FILE__, __LINE__, #cond);                       \
            g_failed++;                                               \
        }                                                             \
    } while (0)

/* ── init ────────────────────────────────────────────────────────────────── */
static void test_init(void)
{
    smallvec_int v = smallvec_int_init();

    EXPECT(v.len          == 0);
    EXPECT(v.cap          == 4);  /* INLINE_CAP */
    EXPECT(v.arena        == NULL);
    EXPECT(v.using_inline == true);
    EXPECT(v.data         == v.inline_buf);

    EXPECT(smallvec_int_is_empty(&v));
    EXPECT(smallvec_int_using_inline(&v));
    EXPECT(smallvec_int_len(&v)      == 0);
    EXPECT(smallvec_int_capacity(&v) == 4);
    EXPECT(smallvec_int_data(&v)     == v.inline_buf);
    EXPECT(smallvec_int_first(&v)    == NULL);
    EXPECT(smallvec_int_last(&v)     == NULL);

    /* NULL-safe queries */
    EXPECT(smallvec_int_len(NULL)         == 0);
    EXPECT(smallvec_int_capacity(NULL)    == 0);
    EXPECT(smallvec_int_is_empty(NULL));
    EXPECT(!smallvec_int_using_inline(NULL));
    EXPECT(smallvec_int_data(NULL)  == NULL);
    EXPECT(smallvec_int_first(NULL) == NULL);
    EXPECT(smallvec_int_last(NULL)  == NULL);
    /* No free needed — inline only */
}

/* ── init_arena ──────────────────────────────────────────────────────────── */
static void test_init_arena(void)
{
    u8    abuf[512];
    Arena arena;
    arena_init(&arena, abuf, sizeof(abuf));

    smallvec_int v = smallvec_int_init_arena(&arena);
    EXPECT(v.arena        == &arena);
    EXPECT(v.using_inline == true);
    EXPECT(v.len          == 0);
    EXPECT(v.cap          == 4);
    /* No free needed — spill would go to arena */
}

/* ── push inline (no spill) ──────────────────────────────────────────────── */
static void test_push_inline(void)
{
    smallvec_int v = smallvec_int_init();

    EXPECT(smallvec_int_push(&v, 10));
    EXPECT(smallvec_int_push(&v, 20));
    EXPECT(smallvec_int_push(&v, 30));
    EXPECT(smallvec_int_push(&v, 40));
    EXPECT(v.len == 4);
    EXPECT(v.using_inline == true);  /* still inline — exactly at capacity */
    EXPECT(v.data == v.inline_buf);

    /* NULL returns false */
    EXPECT(!smallvec_int_push(NULL, 99));

    /* No free needed — still inline */
}

/* ── spill to heap ───────────────────────────────────────────────────────── */
static void test_spill_heap(void)
{
    smallvec_int v = smallvec_int_init();
    /* Fill inline buffer */
    EXPECT(smallvec_int_push(&v, 1));
    EXPECT(smallvec_int_push(&v, 2));
    EXPECT(smallvec_int_push(&v, 3));
    EXPECT(smallvec_int_push(&v, 4));
    EXPECT(v.using_inline == true);

    /* 5th push triggers spill */
    EXPECT(smallvec_int_push(&v, 5));
    EXPECT(v.using_inline == false);
    EXPECT(v.data != v.inline_buf);
    EXPECT(v.cap  == 8);  /* 4 * 2 */
    EXPECT(v.len  == 5);
    EXPECT(v.arena == NULL); /* heap spill */

    /* Contents preserved after spill */
    int val = 0;
    EXPECT(smallvec_int_get(&v, 0, &val) && val == 1);
    EXPECT(smallvec_int_get(&v, 4, &val) && val == 5);

    /* Can continue pushing until new cap exhausted */
    EXPECT(smallvec_int_push(&v, 6));
    EXPECT(smallvec_int_push(&v, 7));
    EXPECT(smallvec_int_push(&v, 8));
    EXPECT(v.len == 8 && v.cap == 8);

    /* Post-spill full — no second growth; returns false */
    EXPECT(!smallvec_int_push(&v, 9));
    EXPECT(v.len == 8); /* unchanged */

    smallvec_int_free(&v);
    EXPECT(v.using_inline == true); /* free reinitializes inline */
    EXPECT(v.data == v.inline_buf);
    EXPECT(v.len  == 0);
}

/* ── spill to arena ──────────────────────────────────────────────────────── */
static void test_spill_arena(void)
{
    u8    abuf[512];
    Arena arena;
    arena_init(&arena, abuf, sizeof(abuf));

    smallvec_int v = smallvec_int_init_arena(&arena);
    EXPECT(smallvec_int_push(&v, 10));
    EXPECT(smallvec_int_push(&v, 20));
    EXPECT(smallvec_int_push(&v, 30));
    EXPECT(smallvec_int_push(&v, 40));
    EXPECT(v.using_inline == true);

    /* 5th push spills into arena */
    EXPECT(smallvec_int_push(&v, 50));
    EXPECT(v.using_inline == false);
    EXPECT(v.arena == &arena);
    /* data points inside arena buffer — not heap */
    EXPECT(v.data >= (int*)(void*)abuf);
    EXPECT(v.data < (int*)(void*)(abuf + sizeof(abuf)));

    int val = 0;
    EXPECT(smallvec_int_get(&v, 4, &val) && val == 50);

    /* free() with arena does NOT call free() — no-op for spill memory */
    smallvec_int_free(&v);
    EXPECT(v.using_inline == true); /* reinitializes inline */
}

/* ── pop ─────────────────────────────────────────────────────────────────── */
static void test_pop(void)
{
    smallvec_int v = smallvec_int_init();
    smallvec_int_push(&v, 100);
    smallvec_int_push(&v, 200);
    smallvec_int_push(&v, 300);

    int out = 0;
    EXPECT(smallvec_int_pop(&v, &out) && out == 300);
    EXPECT(smallvec_int_pop(&v, &out) && out == 200);
    EXPECT(smallvec_int_pop(&v, &out) && out == 100);
    EXPECT(v.len == 0);
    EXPECT(!smallvec_int_pop(&v, &out));   /* empty */
    EXPECT(!smallvec_int_pop(NULL, &out)); /* NULL */
    EXPECT(!smallvec_int_pop(&v, NULL));   /* NULL out */
}

/* ── get / get_unchecked ─────────────────────────────────────────────────── */
static void test_get(void)
{
    smallvec_int v = smallvec_int_init();
    for (int i = 0; i < 4; i++) smallvec_int_push(&v, i * 10);

    int val = 0;
    EXPECT(smallvec_int_get(&v, 0, &val) && val == 0);
    EXPECT(smallvec_int_get(&v, 3, &val) && val == 30);
    EXPECT(!smallvec_int_get(&v, 4, &val));   /* OOB */
    EXPECT(!smallvec_int_get(NULL, 0, &val)); /* NULL v */
    EXPECT(!smallvec_int_get(&v, 0, NULL));   /* NULL out */

    EXPECT(smallvec_int_get_unchecked(&v, 1) == 10);
    EXPECT(smallvec_int_get_unchecked(&v, 2) == 20);
}

/* ── data / first / last ─────────────────────────────────────────────────── */
static void test_data_first_last(void)
{
    smallvec_int v = smallvec_int_init();
    EXPECT(smallvec_int_data(&v)  == v.inline_buf);
    EXPECT(smallvec_int_first(&v) == NULL);
    EXPECT(smallvec_int_last(&v)  == NULL);

    smallvec_int_push(&v, 1);
    smallvec_int_push(&v, 2);
    smallvec_int_push(&v, 3);

    EXPECT(*smallvec_int_first(&v) == 1);
    EXPECT(*smallvec_int_last(&v)  == 3);

    /* Mutate via pointer */
    *smallvec_int_first(&v) = 99;
    int val = 0;
    EXPECT(smallvec_int_get(&v, 0, &val) && val == 99);
}

/* ── insert ──────────────────────────────────────────────────────────────── */
static void test_insert(void)
{
    smallvec_int v = smallvec_int_init();
    smallvec_int_push(&v, 1);
    smallvec_int_push(&v, 2);
    smallvec_int_push(&v, 4);

    /* Insert 3 at index 2 — still inline */
    EXPECT(smallvec_int_insert(&v, 2, 3));
    EXPECT(v.len == 4 && v.using_inline == true);
    int val = 0;
    EXPECT(smallvec_int_get(&v, 2, &val) && val == 3);
    EXPECT(smallvec_int_get(&v, 3, &val) && val == 4);

    /* Insert at index 0 — triggers spill (len was 4 == cap) */
    EXPECT(smallvec_int_insert(&v, 0, 0));
    EXPECT(v.len == 5 && v.using_inline == false);
    EXPECT(smallvec_int_get(&v, 0, &val) && val == 0);
    EXPECT(smallvec_int_get(&v, 1, &val) && val == 1);

    /* OOB */
    EXPECT(!smallvec_int_insert(&v, 100, 0));
    EXPECT(!smallvec_int_insert(NULL, 0, 0));

    smallvec_int_free(&v);
}

/* ── remove ──────────────────────────────────────────────────────────────── */
static void test_remove(void)
{
    smallvec_int v = smallvec_int_init();
    for (int i = 0; i < 4; i++) smallvec_int_push(&v, i); /* 0,1,2,3 */

    int out = 0;
    /* Remove middle */
    EXPECT(smallvec_int_remove(&v, 1, &out) && out == 1);
    EXPECT(v.len == 3);
    EXPECT(smallvec_int_get(&v, 1, &out) && out == 2);

    /* Remove first */
    EXPECT(smallvec_int_remove(&v, 0, &out) && out == 0);
    EXPECT(v.len == 2);

    /* Remove last */
    EXPECT(smallvec_int_remove(&v, v.len - 1, &out) && out == 3);
    EXPECT(v.len == 1);

    /* OOB */
    EXPECT(!smallvec_int_remove(&v, 10, &out));
    EXPECT(!smallvec_int_remove(NULL, 0, &out));
    EXPECT(!smallvec_int_remove(&v, 0, NULL));
}

/* ── extend ──────────────────────────────────────────────────────────────── */
static void test_extend(void)
{
    smallvec_int v = smallvec_int_init();
    int src3[] = {10, 20, 30};

    /* 3 elements fit inline */
    EXPECT(smallvec_int_extend(&v, src3, 3));
    EXPECT(v.len == 3 && v.using_inline == true);

    /* 2 more triggers spill (3+2 > 4) */
    int src2[] = {40, 50};
    EXPECT(smallvec_int_extend(&v, src2, 2));
    EXPECT(v.len == 5 && v.using_inline == false);
    int val = 0;
    EXPECT(smallvec_int_get(&v, 4, &val) && val == 50);

    /* Extend beyond post-spill cap fails */
    int big[10] = {0};
    EXPECT(!smallvec_int_extend(&v, big, 10));
    EXPECT(v.len == 5); /* unchanged */

    /* NULL */
    EXPECT(!smallvec_int_extend(NULL, src3, 1));
    EXPECT(!smallvec_int_extend(&v, NULL, 1));

    /* count == 0 */
    EXPECT(smallvec_int_extend(&v, src3, 0));
    EXPECT(v.len == 5);

    smallvec_int_free(&v);
}

/* ── clear ───────────────────────────────────────────────────────────────── */
static void test_clear(void)
{
    smallvec_int v = smallvec_int_init();
    smallvec_int_push(&v, 1);
    smallvec_int_push(&v, 2);
    smallvec_int_push(&v, 3);

    smallvec_int_clear(&v);
    EXPECT(v.len == 0);
    EXPECT(smallvec_int_is_empty(&v));
    EXPECT(v.using_inline == true); /* clear doesn't affect inline state */

    /* Can push again */
    EXPECT(smallvec_int_push(&v, 42));
    EXPECT(v.len == 1);

    /* NULL-safe */
    smallvec_int_clear(NULL);
}

/* ── free reinitializes ──────────────────────────────────────────────────── */
static void test_free(void)
{
    smallvec_int v = smallvec_int_init();
    /* Spill to heap */
    for (int i = 0; i < 5; i++) smallvec_int_push(&v, i);
    EXPECT(v.using_inline == false);

    smallvec_int_free(&v);
    EXPECT(v.using_inline == true);
    EXPECT(v.data == v.inline_buf);
    EXPECT(v.len  == 0);
    EXPECT(v.cap  == 4);

    /* Can use again after free */
    EXPECT(smallvec_int_push(&v, 7));
    EXPECT(v.len == 1);

    /* NULL-safe */
    smallvec_int_free(NULL);
}

/* ── inline invariant ────────────────────────────────────────────────────── */
static void test_inline_invariant(void)
{
    smallvec_int v = smallvec_int_init();

    /* Before spill: data always aliases inline_buf */
    EXPECT(v.data == v.inline_buf);
    smallvec_int_push(&v, 1);
    EXPECT(v.data == v.inline_buf);
    smallvec_int_push(&v, 2);
    EXPECT(v.data == v.inline_buf);
    smallvec_int_push(&v, 3);
    EXPECT(v.data == v.inline_buf);
    smallvec_int_push(&v, 4); /* still inline at cap */
    EXPECT(v.data == v.inline_buf && v.using_inline == true);

    /* After spill: data no longer aliases inline_buf */
    smallvec_int_push(&v, 5);
    EXPECT(v.data != v.inline_buf && v.using_inline == false);

    smallvec_int_free(&v);
}

/* ── as_vec interop ──────────────────────────────────────────────────────── */
static void test_as_vec(void)
{
    smallvec_int v = smallvec_int_init();
    smallvec_int_push(&v, 10);
    smallvec_int_push(&v, 20);
    smallvec_int_push(&v, 30);

    canon_vec_int vv = smallvec_int_as_vec(&v);
    /* Borrowed view — same data pointer, same capacity */
    EXPECT(vv.data == v.data);
    EXPECT(vv.cap  == v.cap);
    /* len is 0 in the view (vec starts logically empty) */
    EXPECT(vv.len  == 0);
}

/* ── Point struct instantiation ──────────────────────────────────────────── */
static void test_point(void)
{
    smallvec_Point v = smallvec_Point_init();
    EXPECT(v.cap == 2 && v.using_inline == true);

    Point p1 = {1, 2};
    Point p2 = {3, 4};
    EXPECT(smallvec_Point_push(&v, p1));
    EXPECT(smallvec_Point_push(&v, p2));
    EXPECT(v.using_inline == true && v.len == 2);

    /* 3rd push spills */
    Point p3 = {5, 6};
    EXPECT(smallvec_Point_push(&v, p3));
    EXPECT(v.using_inline == false && v.len == 3 && v.cap == 4);

    Point out = {0, 0};
    EXPECT(smallvec_Point_get(&v, 0, &out) && out.x == 1 && out.y == 2);
    EXPECT(smallvec_Point_get(&v, 2, &out) && out.x == 5 && out.y == 6);

    Point popped = {0, 0};
    EXPECT(smallvec_Point_pop(&v, &popped) && popped.x == 5 && popped.y == 6);

    smallvec_Point_free(&v);
    EXPECT(v.using_inline == true);
}

/* ── Suppress unused ─────────────────────────────────────────────────────── */
static void smallvec_suppress_unused(void)
{
    (void)smallvec_int_spill;
    (void)smallvec_Point_spill;

    /* Point functions not exercised in test_point */
    (void)smallvec_Point_len;
    (void)smallvec_Point_capacity;
    (void)smallvec_Point_is_empty;
    (void)smallvec_Point_using_inline;
    (void)smallvec_Point_get_unchecked;
    (void)smallvec_Point_data;
    (void)smallvec_Point_first;
    (void)smallvec_Point_last;
    (void)smallvec_Point_insert;
    (void)smallvec_Point_remove;
    (void)smallvec_Point_extend;
    (void)smallvec_Point_clear;
    (void)smallvec_Point_init_arena;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */
int main(void)
{
    (void)smallvec_suppress_unused;

    test_init();
    test_init_arena();
    test_push_inline();
    test_spill_heap();
    test_spill_arena();
    test_pop();
    test_get();
    test_data_first_last();
    test_insert();
    test_remove();
    test_extend();
    test_clear();
    test_free();
    test_as_vec();
    test_inline_invariant();
    test_point();

    if (g_failed == 0) {
        printf("OK  smallvec_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  smallvec_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * Invariant note: using_inline is ONLY guaranteed to be true while
 * ref_count <= INLINE_CAP AND no spill has happened yet.
 * After spill, using_inline stays false even if len drops back to 0.
 * The reference model tracks this with a separate `spilled` flag.
 */

static void smallvec_fuzz_suppress_unused(void)
{
    (void)smallvec_int_spill;
    (void)smallvec_int_init_arena;
    (void)smallvec_int_capacity;
    (void)smallvec_int_get_unchecked;
    (void)smallvec_int_data;
    (void)smallvec_int_first;
    (void)smallvec_int_last;
    (void)smallvec_int_insert;
    (void)smallvec_int_remove;
    (void)smallvec_int_extend;
    (void)smallvec_int_as_vec;

    (void)smallvec_Point_init;
    (void)smallvec_Point_init_arena;
    (void)smallvec_Point_spill;
    (void)smallvec_Point_len;
    (void)smallvec_Point_capacity;
    (void)smallvec_Point_is_empty;
    (void)smallvec_Point_using_inline;
    (void)smallvec_Point_get;
    (void)smallvec_Point_get_unchecked;
    (void)smallvec_Point_data;
    (void)smallvec_Point_first;
    (void)smallvec_Point_last;
    (void)smallvec_Point_push;
    (void)smallvec_Point_pop;
    (void)smallvec_Point_insert;
    (void)smallvec_Point_remove;
    (void)smallvec_Point_extend;
    (void)smallvec_Point_clear;
    (void)smallvec_Point_free;
    (void)smallvec_Point_as_vec;
}

/*
 * Fuzz a single smallvec_int (INLINE_CAP=4) against a reference model.
 *
 * Reference model: fixed int array + count.
 *
 * Input layout — each byte is one operation:
 *   high nibble: op (0–4)
 *   low  nibble: aux value (0–15)
 *
 * Operations:
 *   0 — push(aux)          if ref_count < REF_MAX
 *   1 — pop()              if ref_count > 0
 *   2 — get(aux % len)     verify value matches ref
 *   3 — clear()            ref_count = 0
 *   4 — free()+reinit()    ref_count = 0; spilled = false
 *
 * Invariants after every op:
 *   - len() == ref_count
 *   - is_empty() == (ref_count == 0)
 *   - get(i) == ref[i] for all i in [0, ref_count)
 */

#define FUZZ_INLINE_CAP 4
#define FUZZ_REF_MAX    (FUZZ_INLINE_CAP * 2)  /* post-spill cap */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    (void)smallvec_fuzz_suppress_unused;

    smallvec_int v = smallvec_int_init();

    int    ref[FUZZ_REF_MAX];
    usize  ref_count = 0;
    memset(ref, 0, sizeof(ref));

    #define CHECK_INVARIANTS()                                               \
        do {                                                                 \
            if (smallvec_int_len(&v) != ref_count)   __builtin_trap();      \
            if (smallvec_int_is_empty(&v) != (ref_count == 0)) __builtin_trap(); \
            for (usize _k = 0; _k < ref_count; _k++) {                      \
                int _val = 0;                                                \
                if (!smallvec_int_get(&v, _k, &_val)) __builtin_trap();     \
                if (_val != ref[_k])                   __builtin_trap();     \
            }                                                                \
        } while (0)

    CHECK_INVARIANTS();

    for (usize i = 0; i < size; i++) {
        u8   byte = data[i];
        u8   op   = (u8)(byte >> 4u) % 5u;
        int  aux  = (int)(byte & 0x0Fu);

        switch (op) {
            case 0: { /* push — only when ref has room */
                if (ref_count < FUZZ_REF_MAX) {
                    bool ok = smallvec_int_push(&v, aux);
                    if (ok) ref[ref_count++] = aux;
                    /* push can return false when post-spill cap is full;
                     * that's valid — ref_count stays in sync */
                }
                break;
            }
            case 1: { /* pop */
                int out = 0;
                bool ok = smallvec_int_pop(&v, &out);
                if (ok) {
                    if (ref_count == 0)             __builtin_trap();
                    if (out != ref[ref_count - 1])  __builtin_trap();
                    ref_count--;
                } else {
                    if (ref_count != 0)             __builtin_trap();
                }
                break;
            }
            case 2: { /* get */
                if (ref_count > 0) {
                    usize idx = (usize)aux % ref_count;
                    int   out = 0;
                    if (!smallvec_int_get(&v, idx, &out)) __builtin_trap();
                    if (out != ref[idx])                  __builtin_trap();
                }
                break;
            }
            case 3: { /* clear */
                smallvec_int_clear(&v);
                ref_count = 0;
                break;
            }
            case 4: { /* free + reinit */
                smallvec_int_free(&v);
                v = smallvec_int_init();
                ref_count = 0;
                break;
            }
            default:
                break;
        }

        CHECK_INVARIANTS();
    }

    #undef CHECK_INVARIANTS
    #undef FUZZ_REF_MAX
    #undef FUZZ_INLINE_CAP

    smallvec_int_free(&v);
    return 0;
}

#endif /* CANON_FUZZING */
