/**
 * @file dynvec_test.c
 * @brief Tests for data/convenience/dynvec.h — auto-growing typed vector
 *
 * Two instantiations:
 *   DEFINE_DYNVEC(int)     — primary, all functions exercised
 *   DEFINE_DYNVEC(Point)   — struct coverage (get/set/push/pop)
 *
 * Covers:
 *   - dynvec_T_init()            — zero struct, no allocation
 *   - dynvec_T_with_capacity()   — pre-allocates; len==0, cap==N
 *   - dynvec_T_len()             — 0 for NULL; tracks pushes
 *   - dynvec_T_capacity()        — 0 for NULL; at least doubles on overflow
 *   - dynvec_T_is_empty()        — true on init; false after push
 *   - dynvec_T_push()            — grows automatically; NULL returns false
 *   - dynvec_T_pop()             — returns last; fails on empty / NULL
 *   - dynvec_T_get()             — bounds-checked copy; OOB returns false
 *   - dynvec_T_get_unchecked()   — fast path (debug-only preconditions)
 *   - dynvec_T_set()             — bounds-checked write
 *   - dynvec_T_data()            — raw pointer; NULL before first push
 *   - dynvec_T_first() / _last() — pointers to first/last; NULL when empty
 *   - dynvec_T_insert()          — shifts right; grows; OOB fails
 *   - dynvec_T_remove()          — shifts left; OOB fails
 *   - dynvec_T_clear()           — resets len; retains cap
 *   - dynvec_T_extend()          — bulk append; grows
 *   - dynvec_T_reserve()         — pre-allocates; no-op when sufficient
 *   - dynvec_T_shrink_to_fit()   — cap → len; frees when empty
 *   - dynvec_T_free()            — releases heap; resets fields
 *   - Growth semantics           — cap at least doubles on each realloc
 *   - Point struct               — get/set/push/pop with struct elements
 *   - CANON_LIFETIME_DEBUG       — open on init, restamp on grow-with-
 *                                   relocation, preserve on no-realloc
 *                                   operations and on clear, restamp on
 *                                   shrink-to-zero (recycle), close on free
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Single dynvec_int; reference model tracks expected contents
 *   - Operations: push, pop, clear, get
 *   - Invariants after every op:
 *       • len matches reference count
 *       • is_empty() == (len == 0)
 *       • get(i) matches reference[i] for all i in [0, len)
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "data/convenience/dynvec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Type instantiations ─────────────────────────────────────────────────── */

DEFINE_DYNVEC(int)

typedef struct { int x; int y; } Point;
DEFINE_DYNVEC(Point)

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

/* Diagnostic: print test name before/after each call. Used to identify
 * which test crashes when the test binary segfaults without producing
 * an EXPECT failure message. fflush forces the output to disk even if
 * the test SEGFAULTs immediately after RUN prints — without fflush,
 * buffered stderr can be lost on abnormal termination on some platforms
 * (notably Windows MSVC). The wrapper is cheap enough that we leave it
 * on permanently; it adds two fprintf calls per test, which is noise
 * next to the actual test work and immediately useful the next time a
 * platform-specific crash appears. */
#define RUN(test_fn)                                          \
    do {                                                      \
        fprintf(stderr, "RUN %s\n", #test_fn);               \
        fflush(stderr);                                       \
        test_fn();                                            \
        fprintf(stderr, "OK  %s\n", #test_fn);               \
        fflush(stderr);                                       \
    } while (0)

/* Intra-test checkpoint diagnostic — used in test_data_first_last
 * specifically to narrow down a Windows MSVC Release SEGFAULT to a
 * specific line. The last CP number printed before the crash identifies
 * the offending statement. Remove after the bug is fixed. */
#define CP(n)                                                 \
    do {                                                      \
        fprintf(stderr, "  CP%d\n", (n));                    \
        fflush(stderr);                                       \
    } while (0)

/* State dump for diagnosing the SEGFAULT in test_data_first_last on
 * Windows MSVC Release. Prints the three observable fields of a dynvec_int
 * at well-defined points so we can see when (or if) state corrupts.
 * Remove after the bug is fixed. */
#define DUMP_STATE(label, v)                                              \
    do {                                                                  \
        fprintf(stderr,                                                   \
                "    %s: data=%p len=%zu cap=%zu\n",                     \
                (label), (void*)(v).data, (v).len, (v).cap);             \
        fflush(stderr);                                                   \
    } while (0)

/* ── dynvec_int_init ─────────────────────────────────────────────────────── */
static void test_init(void)
{
    dynvec_int v = dynvec_int_init();
    EXPECT(v.data == NULL);
    EXPECT(v.len  == 0);
    EXPECT(v.cap  == 0);
    EXPECT(dynvec_int_is_empty(&v));
    EXPECT(dynvec_int_len(&v)      == 0);
    EXPECT(dynvec_int_capacity(&v) == 0);
    EXPECT(dynvec_int_data(&v)     == NULL);
    EXPECT(dynvec_int_first(&v)    == NULL);
    EXPECT(dynvec_int_last(&v)     == NULL);
    /* NULL-safe queries */
    EXPECT(dynvec_int_len(NULL)      == 0);
    EXPECT(dynvec_int_capacity(NULL) == 0);
    EXPECT(dynvec_int_is_empty(NULL));
    EXPECT(dynvec_int_data(NULL)  == NULL);
    EXPECT(dynvec_int_first(NULL) == NULL);
    EXPECT(dynvec_int_last(NULL)  == NULL);
    /* No free needed — no allocation */
}

/* ── dynvec_int_with_capacity ────────────────────────────────────────────── */
static void test_with_capacity(void)
{
    dynvec_int v = dynvec_int_with_capacity(16);
    EXPECT(v.data != NULL);
    EXPECT(v.len  == 0);
    EXPECT(v.cap  == 16);
    EXPECT(dynvec_int_is_empty(&v));
    dynvec_int_free(&v);
    EXPECT(v.data == NULL);
    EXPECT(v.len  == 0 && v.cap == 0);

    /* capacity == 0 — no alloc */
    dynvec_int z = dynvec_int_with_capacity(0);
    EXPECT(z.data == NULL && z.cap == 0);
}

/* ── push / len / capacity / is_empty ───────────────────────────────────── */
static void test_push_basic(void)
{
    dynvec_int v = dynvec_int_init();

    EXPECT(dynvec_int_push(&v, 10));
    EXPECT(dynvec_int_push(&v, 20));
    EXPECT(dynvec_int_push(&v, 30));
    EXPECT(v.len == 3);
    EXPECT(!dynvec_int_is_empty(&v));
    /* Use the bounds-checked API — avoids clang-analyzer false positive
     * that traces realloc failure leaving data == NULL */
    { int a=0, b=0, c=0;
      EXPECT(dynvec_int_get(&v, 0, &a) && a == 10);
      EXPECT(dynvec_int_get(&v, 1, &b) && b == 20);
      EXPECT(dynvec_int_get(&v, 2, &c) && c == 30); }

    /* NULL returns false */
    EXPECT(!dynvec_int_push(NULL, 99));

    dynvec_int_free(&v);
}

/* ── growth: cap at least doubles ────────────────────────────────────────── */
static void test_growth(void)
{
    dynvec_int v = dynvec_int_with_capacity(2);
    usize old_cap = v.cap;

    /* Force overflow of the initial capacity */
    EXPECT(dynvec_int_push(&v, 1));
    EXPECT(dynvec_int_push(&v, 2));
    EXPECT(dynvec_int_push(&v, 3)); /* triggers realloc */
    EXPECT(v.cap >= old_cap * 2);
    EXPECT(v.len == 3);

    dynvec_int_free(&v);
}

/* ── pop ─────────────────────────────────────────────────────────────────── */
static void test_pop(void)
{
    dynvec_int v = dynvec_int_init();
    dynvec_int_push(&v, 100);
    dynvec_int_push(&v, 200);
    dynvec_int_push(&v, 300);

    int out = 0;
    EXPECT(dynvec_int_pop(&v, &out));
    EXPECT(out == 300);
    EXPECT(v.len == 2);

    EXPECT(dynvec_int_pop(&v, &out));
    EXPECT(out == 200);

    EXPECT(dynvec_int_pop(&v, &out));
    EXPECT(out == 100);
    EXPECT(dynvec_int_is_empty(&v));

    /* Empty — fails */
    EXPECT(!dynvec_int_pop(&v, &out));

    /* NULL */
    EXPECT(!dynvec_int_pop(NULL, &out));
    EXPECT(!dynvec_int_pop(&v, NULL));

    dynvec_int_free(&v);
}

/* ── get / get_unchecked / set ───────────────────────────────────────────── */
static void test_get_set(void)
{
    dynvec_int v = dynvec_int_init();
    for (int i = 0; i < 5; i++) dynvec_int_push(&v, i * 10);

    int val = 0;
    EXPECT(dynvec_int_get(&v, 0, &val) && val == 0);
    EXPECT(dynvec_int_get(&v, 4, &val) && val == 40);

    /* OOB */
    EXPECT(!dynvec_int_get(&v, 5, &val));
    EXPECT(!dynvec_int_get(NULL, 0, &val));
    EXPECT(!dynvec_int_get(&v, 0, NULL));

    /* get_unchecked */
    EXPECT(dynvec_int_get_unchecked(&v, 2) == 20);

    /* set */
    EXPECT(dynvec_int_set(&v, 2, 999));
    EXPECT(dynvec_int_get_unchecked(&v, 2) == 999);
    EXPECT(!dynvec_int_set(&v, 5, 0));  /* OOB */
    EXPECT(!dynvec_int_set(NULL, 0, 0));

    dynvec_int_free(&v);
}

/* ── data / first / last ─────────────────────────────────────────────────── *
 * Instrumented with CP(n), DUMP_STATE(), and a manual step-by-step expansion
 * of the second dynvec_int_push() call to narrow down a Windows MSVC Release
 * SEGFAULT. Previous diagnostic passes confirmed:
 *   - State is valid right before the second push (data is a valid heap ptr,
 *     len=1, cap=8)
 *   - The crash is inside the second dynvec_int_push(&v, 2)
 * This pass manually expands the inline body of push() to find the exact
 * crashing instruction. To be removed once the underlying bug is fixed. */
static void test_data_first_last(void)
{
    CP(1);
    dynvec_int v = dynvec_int_init();
    CP(2);
    DUMP_STATE("after init", v);

    EXPECT(dynvec_int_data(&v)  == NULL);
    CP(3);
    EXPECT(dynvec_int_first(&v) == NULL);
    CP(4);
    EXPECT(dynvec_int_last(&v)  == NULL);
    CP(5);
    DUMP_STATE("after empty queries", v);

    dynvec_int_push(&v, 1);
    CP(6);
    DUMP_STATE("after push(1)", v);

    /* Manually expand dynvec_int_push(&v, 2) to find the crashing instruction.
     * The null_check step from push() is skipped — &v is a stack local and
     * the compiler (correctly) treats &v != NULL as a tautology under
     * -Wtautological-pointer-compare/-Waddress. The inlined push body still
     * does the check; we just don't replicate it here. */
    fprintf(stderr, "    push(2) step A: v=%p\n", (void*)&v);
    fflush(stderr);

    bool need_grow = (v.len >= v.cap);
    fprintf(stderr, "    push(2) step B: need_grow=%d (len=%zu cap=%zu)\n",
            (int)need_grow, v.len, v.cap);
    fflush(stderr);

    fprintf(stderr, "    push(2) step C: v.data=%p v.len=%zu\n",
            (void*)v.data, v.len);
    fflush(stderr);

    int* target = &v.data[v.len];
    fprintf(stderr, "    push(2) step D: target=%p\n", (void*)target);
    fflush(stderr);

    *target = 2;
    fprintf(stderr, "    push(2) step E: store complete\n");
    fflush(stderr);

    v.len++;
    fprintf(stderr, "    push(2) step F: len=%zu\n", v.len);
    fflush(stderr);

    CP(7);
    DUMP_STATE("after manual push(2)", v);

    dynvec_int_push(&v, 3);
    CP(8);

    EXPECT(dynvec_int_data(&v)  == v.data);
    CP(9);
    EXPECT(*dynvec_int_first(&v) == 1);
    CP(10);
    EXPECT(*dynvec_int_last(&v)  == 3);
    CP(11);

    /* Mutate via pointer */
    *dynvec_int_first(&v) = 100;
    CP(12);
    EXPECT(v.data[0] == 100);
    CP(13);

    dynvec_int_free(&v);
    CP(14);
}

/* ── insert ──────────────────────────────────────────────────────────────── */
static void test_insert(void)
{
    dynvec_int v = dynvec_int_init();
    dynvec_int_push(&v, 1);
    dynvec_int_push(&v, 2);
    dynvec_int_push(&v, 4);

    /* Insert 3 at index 2 → {1, 2, 3, 4} */
    EXPECT(dynvec_int_insert(&v, 2, 3));
    EXPECT(v.len == 4);
    EXPECT(v.data[2] == 3 && v.data[3] == 4);

    /* Insert at 0 → {0, 1, 2, 3, 4} */
    EXPECT(dynvec_int_insert(&v, 0, 0));
    EXPECT(v.len == 5 && v.data[0] == 0 && v.data[1] == 1);

    /* Insert at end (== len) */
    EXPECT(dynvec_int_insert(&v, 5, 99));
    EXPECT(v.len == 6 && v.data[5] == 99);

    /* OOB */
    EXPECT(!dynvec_int_insert(&v, 100, 0));
    EXPECT(!dynvec_int_insert(NULL, 0, 0));

    dynvec_int_free(&v);
}

/* ── remove ──────────────────────────────────────────────────────────────── */
static void test_remove(void)
{
    dynvec_int v = dynvec_int_init();
    for (int i = 0; i < 5; i++) dynvec_int_push(&v, i); /* 0,1,2,3,4 */

    int out = 0;
    /* Remove middle */
    EXPECT(dynvec_int_remove(&v, 2, &out));
    EXPECT(out == 2);
    EXPECT(v.len == 4);
    EXPECT(v.data[2] == 3); /* shifted */

    /* Remove first */
    EXPECT(dynvec_int_remove(&v, 0, &out));
    EXPECT(out == 0);
    EXPECT(v.data[0] == 1);

    /* Remove last */
    EXPECT(dynvec_int_remove(&v, v.len - 1, &out));
    EXPECT(out == 4);
    EXPECT(v.len == 2);

    /* OOB */
    EXPECT(!dynvec_int_remove(&v, 10, &out));
    EXPECT(!dynvec_int_remove(NULL, 0, &out));
    EXPECT(!dynvec_int_remove(&v, 0, NULL));

    dynvec_int_free(&v);
}

/* ── clear ───────────────────────────────────────────────────────────────── */
static void test_clear(void)
{
    dynvec_int v = dynvec_int_init();
    for (int i = 0; i < 8; i++) dynvec_int_push(&v, i);
    usize old_cap = v.cap;

    dynvec_int_clear(&v);
    EXPECT(v.len == 0);
    EXPECT(v.cap == old_cap); /* buffer retained */
    EXPECT(dynvec_int_is_empty(&v));

    /* Can push again after clear */
    EXPECT(dynvec_int_push(&v, 42));
    EXPECT(v.len == 1 && v.data[0] == 42);

    /* NULL-safe */
    dynvec_int_clear(NULL);

    dynvec_int_free(&v);
}

/* ── extend ──────────────────────────────────────────────────────────────── */
static void test_extend(void)
{
    dynvec_int v = dynvec_int_init();
    int src[] = {10, 20, 30, 40, 50};

    EXPECT(dynvec_int_extend(&v, src, 5));
    EXPECT(v.len == 5);
    for (usize i = 0; i < 5; i++) EXPECT(v.data[i] == src[i]);

    /* Extend with more, forcing growth */
    int more[] = {60, 70, 80, 90, 100};
    EXPECT(dynvec_int_extend(&v, more, 5));
    EXPECT(v.len == 10);
    EXPECT(v.data[9] == 100);

    /* NULL src/v */
    EXPECT(!dynvec_int_extend(NULL, src, 1));
    EXPECT(!dynvec_int_extend(&v, NULL, 1));

    /* count == 0 — no-op */
    EXPECT(dynvec_int_extend(&v, src, 0));
    EXPECT(v.len == 10);

    dynvec_int_free(&v);
}

/* ── reserve ─────────────────────────────────────────────────────────────── */
static void test_reserve(void)
{
    dynvec_int v = dynvec_int_init();

    EXPECT(dynvec_int_reserve(&v, 64));
    EXPECT(v.cap >= 64);
    EXPECT(v.len == 0);

    /* Already sufficient — no-op */
    usize cap_before = v.cap;
    EXPECT(dynvec_int_reserve(&v, 10));
    EXPECT(v.cap == cap_before);

    /* NULL */
    EXPECT(!dynvec_int_reserve(NULL, 10));

    dynvec_int_free(&v);
}

/* ── shrink_to_fit ───────────────────────────────────────────────────────── */
static void test_shrink_to_fit(void)
{
    dynvec_int v = dynvec_int_with_capacity(128);
    dynvec_int_push(&v, 1);
    dynvec_int_push(&v, 2);
    dynvec_int_push(&v, 3);
    EXPECT(v.cap == 128);

    EXPECT(dynvec_int_shrink_to_fit(&v));
    EXPECT(v.cap == 3);
    EXPECT(v.len == 3);
    { int first=0, last=0;
      EXPECT(dynvec_int_get(&v, 0, &first) && first == 1);
      EXPECT(dynvec_int_get(&v, 2, &last)  && last  == 3); }

    /* Shrink empty — frees buffer */
    dynvec_int_clear(&v);
    EXPECT(dynvec_int_shrink_to_fit(&v));
    EXPECT(v.data == NULL && v.cap == 0);

    /* Already minimal — no-op */
    dynvec_int m = dynvec_int_with_capacity(2);
    dynvec_int_push(&m, 10);
    dynvec_int_push(&m, 20);
    EXPECT(m.cap == 2);
    EXPECT(dynvec_int_shrink_to_fit(&m));
    EXPECT(m.cap == 2);
    dynvec_int_free(&m);

    /* NULL-safe */
    EXPECT(dynvec_int_shrink_to_fit(NULL));
}

/* ── Point struct instantiation ──────────────────────────────────────────── */
static void test_point(void)
{
    dynvec_Point v = dynvec_Point_init();
    EXPECT(dynvec_Point_is_empty(&v));

    Point p1 = {1, 2};
    Point p2 = {3, 4};
    Point p3 = {5, 6};

    EXPECT(dynvec_Point_push(&v, p1));
    EXPECT(dynvec_Point_push(&v, p2));
    EXPECT(dynvec_Point_push(&v, p3));
    EXPECT(v.len == 3);

    Point out = {0, 0};
    EXPECT(dynvec_Point_get(&v, 1, &out));
    EXPECT(out.x == 3 && out.y == 4);

    EXPECT(dynvec_Point_set(&v, 0, (Point){10, 20}));
    { Point check = {0, 0};
      EXPECT(dynvec_Point_get(&v, 0, &check) && check.x == 10 && check.y == 20); }

    Point popped = {0, 0};
    EXPECT(dynvec_Point_pop(&v, &popped));
    EXPECT(popped.x == 5 && popped.y == 6);
    EXPECT(v.len == 2);

    dynvec_Point_free(&v);
    EXPECT(v.data == NULL);
}

/* ── CANON_LIFETIME_DEBUG: lifetime token semantics ──────────────────────── *
 *
 * These tests exercise the (id, open) lifetime token embedded on each
 * dynvec_int instance when CANON_LIFETIME_DEBUG is defined. They verify
 * state-level invariants directly on v.lt — they do NOT construct borrows
 * or trigger lifetime_assert_valid. The borrow-side validation path
 * (fires require_msg) is covered in test/semantics/borrow_test.c.
 *
 * Contract per the file docblock:
 *   - init / with_capacity open the lifetime: lt.open == true, lt.id set.
 *   - grow restamps lt.id ONLY when realloc moves the buffer.
 *   - clear does NOT touch lt — buffer is unchanged, borrows are still
 *     valid addresses (substrate does not catch use-of-removed-element).
 *   - shrink_to_fit(empty) restamps and stays open — recycle semantics.
 *   - free closes lt: lt.open == false.
 */

#ifdef CANON_LIFETIME_DEBUG

static void test_lifetime_init_opens_token(void)
{
    dynvec_int v = dynvec_int_init();
    EXPECT(v.lt.open == true);
    EXPECT(v.lt.id != REGION_ID_STATIC);
    dynvec_int_free(&v);
}

static void test_lifetime_grow_restamps_id_when_relocated(void)
{
    /* Force at least one grow. realloc from a fresh (NULL, 0) state always
     * allocates a new buffer — the old pointer was NULL, so new_data
     * cannot equal old_data. This is the canonical relocation case. */
    dynvec_int v = dynvec_int_init();
    region_id_t before = v.lt.id;

    /* First push triggers grow from cap=0 to DYNVEC_INITIAL_CAPACITY.
     * old_data was NULL, new_data is non-NULL → relocation, restamp. */
    dynvec_int_push(&v, 1);

    EXPECT(v.lt.id != before);
    EXPECT(v.lt.open == true);   /* grow recycles, does not close */
    dynvec_int_free(&v);
}

static void test_lifetime_clear_preserves_id(void)
{
    /* Clear must not touch lt.id. The substrate is honest that it does
     * not catch use-of-removed-element. This test documents that contract
     * — if a future refactor accidentally restamps on clear, this fires. */
    dynvec_int v = dynvec_int_init();
    dynvec_int_push(&v, 1);
    dynvec_int_push(&v, 2);
    region_id_t before = v.lt.id;
    dynvec_int_clear(&v);
    EXPECT(v.lt.id == before);
    EXPECT(v.lt.open == true);
    dynvec_int_free(&v);
}

static void test_lifetime_shrink_to_fit_empty_restamps(void)
{
    /* shrink_to_fit when len == 0 frees the buffer and restamps —
     * the container is recycled to empty state, not destroyed.
     * lt.open stays true; subsequent push will grow from zero. */
    dynvec_int v = dynvec_int_with_capacity(32);
    region_id_t before = v.lt.id;
    EXPECT(dynvec_int_shrink_to_fit(&v));
    EXPECT(v.lt.id != before);
    EXPECT(v.lt.open == true);  /* recycle, not close */
    EXPECT(v.data == NULL);     /* buffer freed */
    dynvec_int_free(&v);
}

static void test_lifetime_free_closes_token(void)
{
    dynvec_int v = dynvec_int_init();
    dynvec_int_push(&v, 1);
    dynvec_int_free(&v);
    EXPECT(v.lt.open == false);
    /* lt.id is preserved for post-mortem inspection but the lifetime
     * is closed. lifetime_assert_valid against the captured id would
     * fail the open check. */
}

#endif /* CANON_LIFETIME_DEBUG */

/* ── Suppress unused ─────────────────────────────────────────────────────── */
static void dynvec_suppress_unused(void)
{
    /* Internal grow functions — called only via push/insert/extend/reserve */
    (void)dynvec_int_grow;
    (void)dynvec_Point_grow;

    /* Point functions not exercised in test_point */
    (void)dynvec_Point_len;
    (void)dynvec_Point_capacity;
    (void)dynvec_Point_get_unchecked;
    (void)dynvec_Point_data;
    (void)dynvec_Point_first;
    (void)dynvec_Point_last;
    (void)dynvec_Point_insert;
    (void)dynvec_Point_remove;
    (void)dynvec_Point_clear;
    (void)dynvec_Point_extend;
    (void)dynvec_Point_reserve;
    (void)dynvec_Point_shrink_to_fit;
    (void)dynvec_Point_with_capacity;

    /* Lifetime helpers — exercised under CANON_LIFETIME_DEBUG only.
     * Reference them unconditionally so release builds don't warn
     * about unused inlines either. */
    (void)dynvec_int_lifetime_open_;
    (void)dynvec_int_lifetime_restamp_;
    (void)dynvec_int_lifetime_close_;
    (void)dynvec_Point_lifetime_open_;
    (void)dynvec_Point_lifetime_restamp_;
    (void)dynvec_Point_lifetime_close_;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */
int main(void)
{
    (void)dynvec_suppress_unused;

    RUN(test_init);
    RUN(test_with_capacity);
    RUN(test_push_basic);
    RUN(test_growth);
    RUN(test_pop);
    RUN(test_get_set);
    RUN(test_data_first_last);
    RUN(test_insert);
    RUN(test_remove);
    RUN(test_clear);
    RUN(test_extend);
    RUN(test_reserve);
    RUN(test_shrink_to_fit);
    RUN(test_point);

#ifdef CANON_LIFETIME_DEBUG
    RUN(test_lifetime_init_opens_token);
    RUN(test_lifetime_grow_restamps_id_when_relocated);
    RUN(test_lifetime_clear_preserves_id);
    RUN(test_lifetime_shrink_to_fit_empty_restamps);
    RUN(test_lifetime_free_closes_token);
#endif

    if (g_failed == 0) {
        printf("OK  dynvec_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  dynvec_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void dynvec_fuzz_suppress_unused(void)
{
    /* dynvec_int — not used in fuzz path */
    (void)dynvec_int_grow;
    (void)dynvec_int_with_capacity;
    (void)dynvec_int_capacity;
    (void)dynvec_int_get_unchecked;
    (void)dynvec_int_set;
    (void)dynvec_int_data;
    (void)dynvec_int_first;
    (void)dynvec_int_last;
    (void)dynvec_int_insert;
    (void)dynvec_int_remove;
    (void)dynvec_int_extend;
    (void)dynvec_int_reserve;
    (void)dynvec_int_shrink_to_fit;
    (void)dynvec_int_lifetime_open_;
    (void)dynvec_int_lifetime_restamp_;
    (void)dynvec_int_lifetime_close_;

    /* Point — entirely unused in fuzz path */
    (void)dynvec_Point_init;
    (void)dynvec_Point_with_capacity;
    (void)dynvec_Point_len;
    (void)dynvec_Point_capacity;
    (void)dynvec_Point_is_empty;
    (void)dynvec_Point_get;
    (void)dynvec_Point_get_unchecked;
    (void)dynvec_Point_set;
    (void)dynvec_Point_data;
    (void)dynvec_Point_first;
    (void)dynvec_Point_last;
    (void)dynvec_Point_push;
    (void)dynvec_Point_pop;
    (void)dynvec_Point_insert;
    (void)dynvec_Point_remove;
    (void)dynvec_Point_clear;
    (void)dynvec_Point_extend;
    (void)dynvec_Point_reserve;
    (void)dynvec_Point_shrink_to_fit;
    (void)dynvec_Point_free;
    (void)dynvec_Point_grow;
    (void)dynvec_Point_lifetime_open_;
    (void)dynvec_Point_lifetime_restamp_;
    (void)dynvec_Point_lifetime_close_;
}

/*
 * Fuzz a single dynvec_int against a reference model.
 *
 * Reference model: a fixed-size int array + count tracking expected state.
 *
 * Input layout — each byte is one operation:
 *   high nibble: op (0–4)
 *   low  nibble: aux value (0–15)
 *
 * Operations:
 *   0 — push(aux)          ref_add(aux)
 *   1 — pop()              ref_remove_last()
 *   2 — get(i)             verify ref[i] matches
 *   3 — clear()            ref_reset()
 *   4 — free() + reinit()  ref_reset(); test free→reinit path
 *
 * Invariants after every op:
 *   - len() == ref_count
 *   - is_empty() == (ref_count == 0)
 *   - get(i) == ref[i] for all i in [0, ref_count)
 */

#define FUZZ_REF_MAX 512

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    (void)dynvec_fuzz_suppress_unused;

    dynvec_int v = dynvec_int_init();

    int    ref[FUZZ_REF_MAX];
    usize  ref_count = 0;
    memset(ref, 0, sizeof(ref));

    #define CHECK_INVARIANTS()                                               \
        do {                                                                 \
            if (dynvec_int_len(&v) != ref_count)   __builtin_trap();        \
            if (dynvec_int_is_empty(&v) != (ref_count == 0)) __builtin_trap(); \
            for (usize _k = 0; _k < ref_count; _k++) {                      \
                int _val = 0;                                                \
                if (!dynvec_int_get(&v, _k, &_val)) __builtin_trap();       \
                if (_val != ref[_k])                 __builtin_trap();       \
            }                                                                \
        } while (0)

    CHECK_INVARIANTS();

    for (usize i = 0; i < size; i++) {
        u8    byte = data[i];
        u8    op   = (u8)(byte >> 4u) % 5u;
        int   aux  = (int)(byte & 0x0Fu);

        switch (op) {
            case 0: { /* push */
                if (ref_count < FUZZ_REF_MAX) {
                    bool ok = dynvec_int_push(&v, aux);
                    if (ok) ref[ref_count++] = aux;
                }
                break;
            }
            case 1: { /* pop */
                int out = 0;
                bool ok = dynvec_int_pop(&v, &out);
                if (ok) {
                    if (ref_count == 0)              __builtin_trap();
                    if (out != ref[ref_count - 1])   __builtin_trap();
                    ref_count--;
                } else {
                    if (ref_count != 0)              __builtin_trap();
                }
                break;
            }
            case 2: { /* get */
                if (ref_count > 0) {
                    usize idx = (usize)aux % ref_count;
                    int   out = 0;
                    if (!dynvec_int_get(&v, idx, &out)) __builtin_trap();
                    if (out != ref[idx])                __builtin_trap();
                }
                break;
            }
            case 3: { /* clear */
                dynvec_int_clear(&v);
                ref_count = 0;
                break;
            }
            case 4: { /* free + reinit */
                dynvec_int_free(&v);
                v = dynvec_int_init();
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

    dynvec_int_free(&v);
    return 0;
}

#endif /* CANON_FUZZING */
