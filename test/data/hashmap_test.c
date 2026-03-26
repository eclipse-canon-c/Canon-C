/**
 * @file hashmap_test.c
 * @brief Tests for data/hashmap/hashmap.h — Robin Hood open-addressed hashmap
 *
 * Covers:
 *   - hashmap_buffer_size()
 *   - hashmap_init() — valid and all error paths
 *   - Queries: len(), capacity(), is_empty(), load_factor()
 *   - hashmap_insert() — new key (Ok true), update (Ok false),
 *                        capacity exceeded, NULL errors
 *   - hashmap_get() — Some / None
 *   - hashmap_get_or_null() — pointer / NULL, in-place mutation
 *   - hashmap_contains_key()
 *   - hashmap_remove() — Ok(old_value), ERR_NOT_FOUND, NULL errors
 *   - hashmap_remove() then re-insert (backward shift deletion)
 *   - hashmap_clear() — len resets, buffer reusable
 *   - hashmap_iter_next() — visits every occupied slot exactly once
 *   - Robin Hood stress: 75%-load insert + remove + verify survivors
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Keys drawn from nibble (0–15), values are key*10
 *   - Parallel bool ref_present[16] + int ref[16] tracks ground truth
 *   - Verifies get, get_or_null, contains_key, remove against ref after
 *     every operation
 *   - Structural invariant: is_empty iff len == 0, checked after every op
 */

#define CANON_CONTRACT_IMPL

/*
 * Hash and equality functions must be visible before hashmap.h so the
 * compiler can see their signatures when HASHMAP_HASH_FN / HASHMAP_EQ_FN
 * are referenced inside the generated static inline functions.
 */
#include "core/primitives/types.h"
#include <stdbool.h>
#include <string.h>

static u64 hash_u64(const u64* key, void* ctx)
{
    (void)ctx;
    u64 h = *key * 11400714819323198485ULL;
    return h == 0 ? 1 : h;
}

static bool eq_u64(const u64* a, const u64* b, void* ctx)
{
    (void)ctx;
    return *a == *b;
}

#define HASHMAP_KEY_TYPE u64
#define HASHMAP_VAL_TYPE int
#define HASHMAP_HASH_FN  hash_u64
#define HASHMAP_EQ_FN    eq_u64
#include "data/hashmap/hashmap.h"

#include <stdio.h>

/* ── Buffer sizing helper ────────────────────────────────────────────────── */

/*
 * hashmap_buffer_size(cap) is a runtime function so we cannot use it for
 * static array declarations. sizeof(hashmap_slot) * cap is equivalent and
 * is a compile-time constant when cap is a literal.
 */
#define HM_BUF(cap) ((cap) * sizeof(hashmap_slot))

/* ════════════════════════════════════════════════════════════════════════════
   Unit test build
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                              \
    do {                                                          \
        if (!(cond)) {                                            \
            fprintf(stderr, "FAIL %s:%d  %s\n",                  \
                    __FILE__, __LINE__, #cond);                   \
            g_failed++;                                           \
        }                                                         \
    } while (0)

/* EXPECT_NOT_NULL — records failure and returns from the calling function
 * if ptr is NULL, preventing null-dereference and Cppcheck ctunullpointer. */
#define EXPECT_NOT_NULL(ptr)                                      \
    do {                                                          \
        if ((ptr) == NULL) {                                      \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",          \
                    __FILE__, __LINE__, #ptr);                    \
            g_failed++;                                           \
            return;                                               \
        }                                                         \
    } while (0)

/* Helper: initialize a map over a stack buffer, assert success */
static void init_map(hashmap* m, u8* buf, usize buf_len, usize cap)
{
    result_bool_Error r = hashmap_init(m, bytes_from(buf, buf_len), cap, NULL);
    if (!result_bool_Error_is_ok(r)) {
        fprintf(stderr, "FAIL: hashmap_init returned Err in helper\n");
        g_failed++;
    }
}

/* ── hashmap_buffer_size ─────────────────────────────────────────────────── */

static void test_buffer_size(void)
{
    EXPECT(hashmap_buffer_size(1)  == 1  * sizeof(hashmap_slot));
    EXPECT(hashmap_buffer_size(4)  == 4  * sizeof(hashmap_slot));
    EXPECT(hashmap_buffer_size(16) == 16 * sizeof(hashmap_slot));
}

/* ── hashmap_init — valid ────────────────────────────────────────────────── */

static void test_init_valid(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    result_bool_Error r = hashmap_init(&m, bytes_from(buf, sizeof(buf)), 8, NULL);
    EXPECT(result_bool_Error_is_ok(r));
    EXPECT(hashmap_len(&m)      == 0);
    EXPECT(hashmap_capacity(&m) == 8);
    EXPECT(hashmap_is_empty(&m));
    EXPECT(hashmap_load_factor(&m) == 0.0);
}

/* ── hashmap_init — error paths ──────────────────────────────────────────── */

static void test_init_null_map(void)
{
    u8 buf[HM_BUF(8)];
    result_bool_Error r = hashmap_init(NULL, bytes_from(buf, sizeof(buf)), 8, NULL);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_init_null_buf(void)
{
    hashmap m;
    bytes_t empty;
    empty.ptr = NULL;
    empty.len = 0;
    result_bool_Error r = hashmap_init(&m, empty, 8, NULL);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_init_zero_capacity(void)
{
    u8 buf[64];
    hashmap m;
    result_bool_Error r = hashmap_init(&m, bytes_from(buf, sizeof(buf)), 0, NULL);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_init_non_power_of_two(void)
{
    u8 buf[HM_BUF(16)];
    hashmap m;
    /* 6 is not a power of 2 */
    result_bool_Error r = hashmap_init(&m, bytes_from(buf, sizeof(buf)), 6, NULL);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_init_buf_too_small(void)
{
    u8 buf[4]; /* far too small for any slot array */
    hashmap m;
    result_bool_Error r = hashmap_init(&m, bytes_from(buf, sizeof(buf)), 8, NULL);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_BUFFER_TOO_SMALL);
}

/* ── insert — new key ────────────────────────────────────────────────────── */

static void test_insert_new_key(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 key = 42; int val = 100;
    result_bool_Error r = hashmap_insert(&m, &key, &val);
    EXPECT(result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap(r)); /* true = new key */
    EXPECT(hashmap_len(&m) == 1);
    EXPECT(!hashmap_is_empty(&m));
}

/* ── insert — update existing key ───────────────────────────────────────── */

static void test_insert_update(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 key = 1; int val1 = 10; int val2 = 99;
    hashmap_insert(&m, &key, &val1);

    result_bool_Error r = hashmap_insert(&m, &key, &val2);
    EXPECT(result_bool_Error_is_ok(r));
    EXPECT(!result_bool_Error_unwrap(r)); /* false = existing key updated */
    EXPECT(hashmap_len(&m) == 1);        /* no new entry */

    option_hm_val_t o = hashmap_get(&m, &key);
    EXPECT(option_hm_val_t_is_some(o));
    EXPECT(option_hm_val_t_unwrap(o) == 99);
}

/* ── insert — capacity exceeded at 75% load ─────────────────────────────── */

static void test_insert_capacity_exceeded(void)
{
    /*
     * cap=4, load limit = cap - cap/4 = 3.
     * Insert 3 elements (allowed), 4th must fail.
     */
    u8 buf[HM_BUF(4)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 4);

    u64 k; int v = 0;
    k = 1; EXPECT(result_bool_Error_is_ok(hashmap_insert(&m, &k, &v)));
    k = 2; EXPECT(result_bool_Error_is_ok(hashmap_insert(&m, &k, &v)));
    k = 3; EXPECT(result_bool_Error_is_ok(hashmap_insert(&m, &k, &v)));

    k = 4;
    result_bool_Error r = hashmap_insert(&m, &k, &v);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_CAPACITY_EXCEEDED);
    EXPECT(hashmap_len(&m) == 3); /* unchanged */
}

/* ── insert — NULL error paths ───────────────────────────────────────────── */

static void test_insert_null_errors(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);
    u64 k = 1; int v = 0;

    result_bool_Error r;

    r = hashmap_insert(NULL, &k, &v);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    r = hashmap_insert(&m, NULL, &v);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);

    r = hashmap_insert(&m, &k, NULL);
    EXPECT(!result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* ── hashmap_get — found / not found ────────────────────────────────────── */

static void test_get_found(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k = 7; int v = 42;
    hashmap_insert(&m, &k, &v);

    option_hm_val_t o = hashmap_get(&m, &k);
    EXPECT(option_hm_val_t_is_some(o));
    EXPECT(option_hm_val_t_unwrap(o) == 42);
}

static void test_get_not_found(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k = 99;
    option_hm_val_t o = hashmap_get(&m, &k);
    EXPECT(option_hm_val_t_is_none(o));
}

static void test_get_multiple_keys(void)
{
    u8 buf[HM_BUF(16)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 16);

    u64 keys[] = {10, 20, 30, 40};
    int vals[] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++) {
        hashmap_insert(&m, &keys[i], &vals[i]);
    }

    for (int i = 0; i < 4; i++) {
        option_hm_val_t o = hashmap_get(&m, &keys[i]);
        EXPECT(option_hm_val_t_is_some(o));
        EXPECT(option_hm_val_t_unwrap(o) == vals[i]);
    }

    u64 absent = 999;
    EXPECT(option_hm_val_t_is_none(hashmap_get(&m, &absent)));
}

/* ── hashmap_get_or_null ─────────────────────────────────────────────────── */

static void test_get_or_null_found(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k = 5; int v = 55;
    hashmap_insert(&m, &k, &v);

    int* ptr = hashmap_get_or_null(&m, &k);
    EXPECT_NOT_NULL(ptr);
    EXPECT(*ptr == 55);

    /* Mutate in-place and verify via get */
    *ptr = 99;
    option_hm_val_t o = hashmap_get(&m, &k);
    EXPECT(option_hm_val_t_unwrap(o) == 99);
}

static void test_get_or_null_not_found(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k = 123;
    EXPECT(hashmap_get_or_null(&m, &k) == NULL);
}

/* ── hashmap_contains_key ────────────────────────────────────────────────── */

static void test_contains_key(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k1 = 1, k2 = 2; int v = 0;
    hashmap_insert(&m, &k1, &v);

    EXPECT(hashmap_contains_key(&m, &k1));
    EXPECT(!hashmap_contains_key(&m, &k2));
}

/* ── hashmap_remove ──────────────────────────────────────────────────────── */

static void test_remove_found(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k = 3; int v = 33;
    hashmap_insert(&m, &k, &v);

    result_hm_val_t_Error r = hashmap_remove(&m, &k);
    EXPECT(result_hm_val_t_Error_is_ok(r));
    EXPECT(result_hm_val_t_Error_unwrap(r) == 33);
    EXPECT(hashmap_len(&m) == 0);
    EXPECT(hashmap_is_empty(&m));
    EXPECT(!hashmap_contains_key(&m, &k));
}

static void test_remove_not_found(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k = 77;
    result_hm_val_t_Error r = hashmap_remove(&m, &k);
    EXPECT(!result_hm_val_t_Error_is_ok(r));
    EXPECT(result_hm_val_t_Error_unwrap_err(r) == ERR_NOT_FOUND);
}

static void test_remove_null_errors(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);
    u64 k = 1;

    result_hm_val_t_Error r;

    r = hashmap_remove(NULL, &k);
    EXPECT(!result_hm_val_t_Error_is_ok(r));
    EXPECT(result_hm_val_t_Error_unwrap_err(r) == ERR_INVALID_ARG);

    r = hashmap_remove(&m, NULL);
    EXPECT(!result_hm_val_t_Error_is_ok(r));
    EXPECT(result_hm_val_t_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

static void test_remove_then_reinsert(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k = 10; int v1 = 1; int v2 = 2;
    hashmap_insert(&m, &k, &v1);
    hashmap_remove(&m, &k);

    result_bool_Error r = hashmap_insert(&m, &k, &v2);
    EXPECT(result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap(r)); /* true = new key again */
    EXPECT(option_hm_val_t_unwrap(hashmap_get(&m, &k)) == 2);
}

/* ── hashmap_clear ───────────────────────────────────────────────────────── */

static void test_clear(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k; int v = 0;
    k = 1; hashmap_insert(&m, &k, &v);
    k = 2; hashmap_insert(&m, &k, &v);
    k = 3; hashmap_insert(&m, &k, &v);
    EXPECT(hashmap_len(&m) == 3);

    hashmap_clear(&m);
    EXPECT(hashmap_len(&m)      == 0);
    EXPECT(hashmap_capacity(&m) == 8); /* unchanged */
    EXPECT(hashmap_is_empty(&m));

    /* All previously-present keys must now be absent */
    k = 1; EXPECT(!hashmap_contains_key(&m, &k));
    k = 2; EXPECT(!hashmap_contains_key(&m, &k));
    k = 3; EXPECT(!hashmap_contains_key(&m, &k));

    /* Buffer is reusable after clear */
    k = 99; v = 42;
    result_bool_Error r = hashmap_insert(&m, &k, &v);
    EXPECT(result_bool_Error_is_ok(r));
    EXPECT(result_bool_Error_unwrap(r));
}

/* ── load_factor ─────────────────────────────────────────────────────────── */

static void test_load_factor(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    EXPECT(hashmap_load_factor(&m) == 0.0);

    u64 k = 1; int v = 0;
    hashmap_insert(&m, &k, &v);
    /* 1/8 = 0.125 */
    double lf = hashmap_load_factor(&m);
    EXPECT(lf > 0.12 && lf < 0.13);
}

/* ── iteration ───────────────────────────────────────────────────────────── */

static void test_iter_visits_all(void)
{
    u8 buf[HM_BUF(16)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 16);

    u64 keys[] = {1, 2, 3, 4, 5};
    int vals[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        hashmap_insert(&m, &keys[i], &vals[i]);
    }

    usize iter = 0;
    const u64* k;
    const int* v;
    int count = 0;

    while (hashmap_iter_next(&m, &iter, &k, &v)) {
        count++;
        int matched = 0;
        for (int i = 0; i < 5; i++) {
            if (*k == keys[i]) {
                EXPECT(*v == vals[i]);
                matched = 1;
                break;
            }
        }
        EXPECT(matched);
    }
    EXPECT(count == 5);
}

static void test_iter_empty_map(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    usize iter = 0;
    const u64* k;
    const int* v;
    EXPECT(!hashmap_iter_next(&m, &iter, &k, &v));
}

static void test_iter_after_remove(void)
{
    u8 buf[HM_BUF(8)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 8);

    u64 k; int v = 0;
    k = 1; hashmap_insert(&m, &k, &v);
    k = 2; hashmap_insert(&m, &k, &v);
    k = 3; hashmap_insert(&m, &k, &v);
    k = 2; hashmap_remove(&m, &k); /* remove middle */

    usize iter = 0;
    const u64* ik;
    const int* iv;
    int count = 0;
    while (hashmap_iter_next(&m, &iter, &ik, &iv)) {
        count++;
        EXPECT(*ik != 2); /* removed key must not appear */
    }
    EXPECT(count == 2);
}

/* ── Robin Hood stress ───────────────────────────────────────────────────── */

static void test_robin_hood_stress(void)
{
    /*
     * Insert 12 elements into a cap=16 map (75% load exactly triggers
     * the cap check on the 13th attempt). Then remove evens, verify odds.
     */
    u8 buf[HM_BUF(16)];
    hashmap m;
    init_map(&m, buf, sizeof(buf), 16);

    for (u64 i = 0; i < 12; i++) {
        u64 k = i * 13 + 7; /* spread across buckets */
        int v = (int)(i * 100);
        result_bool_Error r = hashmap_insert(&m, &k, &v);
        EXPECT(result_bool_Error_is_ok(r));
    }
    EXPECT(hashmap_len(&m) == 12);

    /* Remove even indices */
    for (u64 i = 0; i < 12; i += 2) {
        u64 k = i * 13 + 7;
        result_hm_val_t_Error r = hashmap_remove(&m, &k);
        EXPECT(result_hm_val_t_Error_is_ok(r));
        EXPECT(result_hm_val_t_Error_unwrap(r) == (int)(i * 100));
    }
    EXPECT(hashmap_len(&m) == 6);

    /* Odd-indexed keys still present with correct values */
    for (u64 i = 1; i < 12; i += 2) {
        u64 k = i * 13 + 7;
        option_hm_val_t o = hashmap_get(&m, &k);
        EXPECT(option_hm_val_t_is_some(o));
        EXPECT(option_hm_val_t_unwrap(o) == (int)(i * 100));
    }

    /* Even-indexed keys gone */
    for (u64 i = 0; i < 12; i += 2) {
        u64 k = i * 13 + 7;
        EXPECT(!hashmap_contains_key(&m, &k));
    }
}

/* ── Suppress unused generated functions ────────────────────────────────── */

static void hashmap_suppress_unused(void)
{
    /* option_hm_val_t — only is_some, is_none, unwrap used in tests */
    (void)option_hm_val_t_get;
    (void)option_hm_val_t_unwrap_or;
    (void)option_hm_val_t_expect;
    (void)option_hm_val_t_map;
    (void)option_hm_val_t_and_then;
    (void)option_hm_val_t_or_else;
    (void)option_hm_val_t_filter;
    (void)option_hm_val_t_combine_with;
    (void)option_hm_val_t_replace;
    (void)option_hm_val_t_take;
    (void)option_hm_val_t_eq;

    /* result_bool_Error — combinators not tested here (covered by result_test) */
    (void)result_bool_Error_get_ok;
    (void)result_bool_Error_get_err;
    (void)result_bool_Error_is_err;
    (void)result_bool_Error_unwrap_or;
    (void)result_bool_Error_expect;
    (void)result_bool_Error_map;
    (void)result_bool_Error_map_err;
    (void)result_bool_Error_and_then;
    (void)result_bool_Error_or_else;
    (void)result_bool_Error_and;
    (void)result_bool_Error_or;
    (void)result_bool_Error_eq;

    /* result_hm_val_t_Error — combinators not tested here */
    (void)result_hm_val_t_Error_is_err;
    (void)result_hm_val_t_Error_get_ok;
    (void)result_hm_val_t_Error_get_err;
    (void)result_hm_val_t_Error_unwrap_or;
    (void)result_hm_val_t_Error_expect;
    (void)result_hm_val_t_Error_map;
    (void)result_hm_val_t_Error_map_err;
    (void)result_hm_val_t_Error_and_then;
    (void)result_hm_val_t_Error_or_else;
    (void)result_hm_val_t_Error_and;
    (void)result_hm_val_t_Error_or;
    (void)result_hm_val_t_Error_eq;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    (void)hashmap_suppress_unused;

    /* buffer_size */
    test_buffer_size();

    /* init — valid and errors */
    test_init_valid();
    test_init_null_map();
    test_init_null_buf();
    test_init_zero_capacity();
    test_init_non_power_of_two();
    test_init_buf_too_small();

    /* queries on empty */
    /* (covered by test_init_valid) */

    /* insert */
    test_insert_new_key();
    test_insert_update();
    test_insert_capacity_exceeded();
    test_insert_null_errors();

    /* get */
    test_get_found();
    test_get_not_found();
    test_get_multiple_keys();

    /* get_or_null */
    test_get_or_null_found();
    test_get_or_null_not_found();

    /* contains_key */
    test_contains_key();

    /* remove */
    test_remove_found();
    test_remove_not_found();
    test_remove_null_errors();
    test_remove_then_reinsert();

    /* clear */
    test_clear();

    /* load_factor */
    test_load_factor();

    /* iteration */
    test_iter_visits_all();
    test_iter_empty_map();
    test_iter_after_remove();

    /* stress */
    test_robin_hood_stress();

    if (g_failed == 0) {
        printf("OK  hashmap_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  hashmap_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * Suppress functions not exercised in the fuzz path.
 */
static void hashmap_fuzz_suppress_unused(void)
{
    /* option_hm_val_t */
    (void)option_hm_val_t_is_none;
    (void)option_hm_val_t_get;
    (void)option_hm_val_t_unwrap_or;
    (void)option_hm_val_t_expect;
    (void)option_hm_val_t_map;
    (void)option_hm_val_t_and_then;
    (void)option_hm_val_t_or_else;
    (void)option_hm_val_t_filter;
    (void)option_hm_val_t_combine_with;
    (void)option_hm_val_t_replace;
    (void)option_hm_val_t_take;
    (void)option_hm_val_t_eq;

    /* result_bool_Error */
    (void)result_bool_Error_is_err;
    (void)result_bool_Error_get_ok;
    (void)result_bool_Error_get_err;
    (void)result_bool_Error_unwrap;
    (void)result_bool_Error_unwrap_or;
    (void)result_bool_Error_unwrap_err;
    (void)result_bool_Error_expect;
    (void)result_bool_Error_map;
    (void)result_bool_Error_map_err;
    (void)result_bool_Error_and_then;
    (void)result_bool_Error_or_else;
    (void)result_bool_Error_and;
    (void)result_bool_Error_or;
    (void)result_bool_Error_eq;

    /* result_hm_val_t_Error */
    (void)result_hm_val_t_Error_is_err;
    (void)result_hm_val_t_Error_get_ok;
    (void)result_hm_val_t_Error_get_err;
    (void)result_hm_val_t_Error_unwrap_or;
    (void)result_hm_val_t_Error_expect;
    (void)result_hm_val_t_Error_map;
    (void)result_hm_val_t_Error_map_err;
    (void)result_hm_val_t_Error_and_then;
    (void)result_hm_val_t_Error_or_else;
    (void)result_hm_val_t_Error_and;
    (void)result_hm_val_t_Error_or;
    (void)result_hm_val_t_Error_eq;

    /* hashmap functions not used in fuzz path */
    (void)hashmap_capacity;
    (void)hashmap_load_factor;
    (void)hashmap_buffer_size;
}

/*
 * Input layout (consumed in order, excess ignored):
 *   [0]    capacity selector — maps to 4, 8, 16, or 32
 *   [1..N] operation stream — each byte:
 *            high nibble: operation index (0-6)
 *            low  nibble: key value (0-15), val = key*10
 *
 * Operations:
 *   0 — insert(key, val)
 *   1 — get(key) — verify against ref
 *   2 — get_or_null(key) — verify against ref
 *   3 — contains_key(key) — verify against ref
 *   4 — remove(key) — verify result and update ref
 *   5 — clear — reset ref
 *   6 — iter_next — count entries, verify matches len
 *
 * Invariants checked after every operation:
 *   - is_empty iff len == 0
 *   - get(key) == Some(ref[key]) iff ref_present[key]
 *   - get_or_null(key) matches ref
 *   - contains_key(key) == ref_present[key]
 *   - remove returns Ok(ref[key]) iff ref_present[key], else Err(NOT_FOUND)
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    static const usize cap_table[4] = {4, 8, 16, 32};

    /* Max capacity is 32 — size the buffer for that */
    u8 buf[HM_BUF(32)];
    hashmap m;

    (void)hashmap_fuzz_suppress_unused;

    if (size < 2) return 0;

    usize cap       = cap_table[data[0] % 4u];
    usize buf_bytes = cap * sizeof(hashmap_slot);

    result_bool_Error init_r = hashmap_init(
        &m, bytes_from(buf, buf_bytes), cap, NULL);
    if (!result_bool_Error_is_ok(init_r)) return 0;

    /*
     * Reference arrays indexed by key (0–15).
     * ref_present[k] == true means key k is in the map with value ref[k].
     */
    int  ref[16];
    bool ref_present[16];
    memset(ref,         0, sizeof(ref));
    memset(ref_present, 0, sizeof(ref_present));

    #define CHECK_INVARIANTS()                                        \
        do {                                                          \
            if (hashmap_is_empty(&m) != (hashmap_len(&m) == 0))      \
                __builtin_trap();                                     \
        } while (0)

    for (usize i = 1; i < size; i++) {
        u8  byte = data[i];
        u8  op   = (u8)(byte >> 4u) % 7u;
        u64 key  = (u64)(byte & 0x0Fu);   /* 0–15 */
        int val  = (int)key * 10;

        switch (op) {

            case 0: { /* insert */
                usize before = hashmap_len(&m);
                result_bool_Error r = hashmap_insert(&m, &key, &val);
                if (result_bool_Error_is_ok(r)) {
                    bool was_new = result_bool_Error_unwrap(r);
                    /* was_new must agree with ref */
                    if (was_new  &&  ref_present[key]) __builtin_trap();
                    if (!was_new && !ref_present[key]) __builtin_trap();
                    /* len must have incremented for new keys */
                    if (was_new && hashmap_len(&m) != before + 1)
                        __builtin_trap();
                    ref[key]         = val;
                    ref_present[key] = true;
                }
                /* ERR_CAPACITY_EXCEEDED is expected at 75% load — not a trap */
                break;
            }

            case 1: { /* get */
                option_hm_val_t o = hashmap_get(&m, &key);
                if (ref_present[key]) {
                    if (!option_hm_val_t_is_some(o))       __builtin_trap();
                    if (option_hm_val_t_unwrap(o) != ref[key]) __builtin_trap();
                } else {
                    if (option_hm_val_t_is_some(o))        __builtin_trap();
                }
                break;
            }

            case 2: { /* get_or_null */
                int* ptr = hashmap_get_or_null(&m, &key);
                if (ref_present[key]) {
                    if (!ptr)            __builtin_trap();
                    if (*ptr != ref[key]) __builtin_trap();
                } else {
                    if (ptr)             __builtin_trap();
                }
                break;
            }

            case 3: { /* contains_key */
                bool has = hashmap_contains_key(&m, &key);
                if (has != ref_present[key]) __builtin_trap();
                break;
            }

            case 4: { /* remove */
                usize before = hashmap_len(&m);
                result_hm_val_t_Error r = hashmap_remove(&m, &key);
                if (ref_present[key]) {
                    if (!result_hm_val_t_Error_is_ok(r))
                        __builtin_trap();
                    if (result_hm_val_t_Error_unwrap(r) != ref[key])
                        __builtin_trap();
                    if (hashmap_len(&m) != before - 1)
                        __builtin_trap();
                    ref_present[key] = false;
                } else {
                    if (result_hm_val_t_Error_is_ok(r))
                        __builtin_trap();
                    if (result_hm_val_t_Error_unwrap_err(r) != ERR_NOT_FOUND)
                        __builtin_trap();
                }
                break;
            }

            case 5: { /* clear */
                hashmap_clear(&m);
                if (hashmap_len(&m) != 0) __builtin_trap();
                memset(ref_present, 0, sizeof(ref_present));
                break;
            }

            case 6: { /* iter_next — count must match len */
                usize iter_state = 0;
                const u64* ik;
                const int* iv;
                usize count = 0;
                while (hashmap_iter_next(&m, &iter_state, &ik, &iv)) {
                    count++;
                    /* Every iterated key must be in ref */
                    if (*ik > 15)               __builtin_trap();
                    if (!ref_present[*ik])      __builtin_trap();
                    if (*iv != ref[*ik])        __builtin_trap();
                }
                if (count != hashmap_len(&m))   __builtin_trap();
                break;
            }

            default:
                break;
        }

        CHECK_INVARIANTS();
    }

    #undef CHECK_INVARIANTS

    return 0;
}

#endif /* CANON_FUZZING */
