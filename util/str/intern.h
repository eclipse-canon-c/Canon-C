#ifndef CANON_UTIL_INTERN_H
#define CANON_UTIL_INTERN_H

#include <stdbool.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "core/memory.h"
#include "core/arena.h"
#include "util/str/str_view.h"

/**
 * @file util/str/intern.h
 * @brief Minimal string interning — returns shared immutable str_view_t
 *
 * Interning maps unique strings to a single shared copy.
 * All returned views point into the arena — lifetime = arena lifetime.
 *
 * Core properties:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero-copy views — returns str_view_t (ptr + len)
 * - Arena-backed — no heap unless you use a heap arena
 * - Deterministic — fixed capacity, no resizing
 * - Collision-safe — uses FNV-1a + linear probing
 * - Empty string & NULL handled efficiently
 * - Thread-safe if arena access is synchronized
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * - pool must not be NULL — violated → contract abort
 * - arena must not be NULL — violated → contract abort
 * - capacity must be > 0 — violated → contract abort
 * - Pool full or arena exhausted are data errors → returns str_view_null()
 * - NULL input string in intern_string is valid → normalized to ""
 *
 * Load factor:
 * ────────────────────────────────────────────────────────────────────────────
 * Linear probing degrades significantly above 70% load. For best performance
 * size capacity to at least ceil(expected_strings / 0.7). Power-of-2
 * capacity is recommended for future hash masking compatibility.
 *
 * Lifetime:
 * ────────────────────────────────────────────────────────────────────────────
 * All interned string data lives in the arena. Returned str_view_t values
 * are valid only as long as the arena is alive and has not been reset.
 * Do NOT free individual views — they are not heap-allocated.
 *
 * Usage pattern:
 * ────────────────────────────────────────────────────────────────────────────
 *   InternPool pool;
 *   intern_pool_init(&pool, &arena, 1024);
 *   str_view_t s1 = intern_string(&pool, "hello");
 *   str_view_t s2 = intern_string(&pool, "hello"); // same pointer as s1
 */

/**
 * @brief Fixed-size string interning pool backed by an arena
 */
typedef struct {
    Arena*      arena;    ///< all interned string data lives here
    str_view_t* entries;  ///< flat hash table of (ptr, len) pairs
    usize       capacity; ///< total number of slots
    usize       count;    ///< number of unique strings currently interned
} InternPool;

/* ────────────────────────────────────────────────────────────────────────────
   Internal: FNV-1a hash over a byte sequence
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Computes FNV-1a hash of a byte sequence
 *
 * Returns a non-zero hash. The value 0 is remapped to 1 to avoid
 * collision with the empty-slot sentinel.
 *
 * @remark Internal — do not call directly.
 */
static inline u64 intern_hash_(borrowed(const char*) s, usize len) {
    u64 h = 14695981039346656037ULL;
    usize i;
    for (i = 0; i < len; i++) {
        h ^= (u64)(unsigned char)s[i];
        h *= 1099511628211ULL;
    }
    return h ? h : 1;
}

/* ────────────────────────────────────────────────────────────────────────────
   Core interning function — declared before intern_pool_init
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Interns a null-terminated string — returns shared view
 *
 * If the string already exists → returns the existing view (pointer equality).
 * If new → allocates a null-terminated copy in the arena and stores the view.
 * If pool is full or arena allocation fails → returns str_view_null().
 *
 * NULL input is normalized to empty string "".
 *
 * @param pool Valid initialized InternPool (must not be NULL — contract)
 * @param s    Null-terminated string to intern (NULL → treated as "")
 * @return Shared str_view_t on success, str_view_null() on pool full / arena exhausted
 *
 * @remark Returned view valid only while backing arena is alive
 * @remark Pointer equality (intern_equals) valid only within same pool
 */
static inline str_view_t intern_string(
    borrowed(InternPool*)  pool,
    borrowed(const char*)  s)
{
    usize len, probes, index;
    u64 hash;

    require_msg(pool          != NULL, "intern_string: pool is NULL");
    require_msg(pool->arena   != NULL, "intern_string: pool->arena is NULL");
    require_msg(pool->entries != NULL, "intern_string: pool->entries is NULL");

    if (!s) s = "";

    len    = str_len(s);
    hash   = intern_hash_(s, len);
    index  = (usize)(hash % pool->capacity);
    probes = 0;

    while (probes < pool->capacity) {
        str_view_t* entry = &pool->entries[index];

        if (entry->ptr == NULL) {
            /* Empty slot — insert new entry */
            char* copy;

            if (pool->count >= pool->capacity) return str_view_null();

            copy = (char*)arena_alloc(pool->arena, len + 1);
            if (!copy) return str_view_null();

            mem_copy(copy, s, len);
            copy[len] = '\0';

            entry->ptr = copy;
            entry->len = len;
            pool->count++;
            return *entry;
        }

        /* Occupied slot — check for match */
        if (entry->len == len && mem_compare(entry->ptr, s, len) == 0) {
            return *entry;
        }

        index = (index + 1) % pool->capacity;
        probes++;
    }

    /* Table exhausted */
    return str_view_null();
}

/* ────────────────────────────────────────────────────────────────────────────
   Initialization
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Initializes a string interning pool backed by an arena
 *
 * Allocates the hash table from the arena and zero-initializes all slots.
 * Pre-interns the empty string so intern_string(pool, "") always returns
 * a consistent non-null view.
 *
 * @param pool     Pool struct to initialize (must not be NULL — contract)
 * @param arena    Arena for all interned data (must not be NULL — contract)
 * @param capacity Number of unique string slots (must be > 0 — contract)
 * @return true on success, false if arena allocation fails
 *
 * @remark Pool valid only while arena is alive and not reset
 */
static inline bool intern_pool_init(
    borrowed(InternPool*)  pool,
    borrowed(Arena*)       arena,
    usize                  capacity)
{
    str_view_t* entries;

    require_msg(pool     != NULL, "intern_pool_init: pool is NULL");
    require_msg(arena    != NULL, "intern_pool_init: arena is NULL");
    require_msg(capacity  > 0,    "intern_pool_init: capacity is 0");

    entries = (str_view_t*)arena_alloc(arena, capacity * sizeof(str_view_t));
    if (!entries) return false;

    mem_set(entries, 0, capacity * sizeof(str_view_t));

    pool->arena    = arena;
    pool->entries  = entries;
    pool->capacity = capacity;
    pool->count    = 0;

    /* Pre-intern empty string — intern_string is defined above */
    intern_string(pool, "");

    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if two interned views refer to the exact same string
 *
 * Since interned strings are deduplicated, pointer equality is sufficient
 * and O(1). Only valid for views obtained from the same pool.
 */
static inline bool intern_equals(str_view_t a, str_view_t b) {
    return a.ptr == b.ptr;
}

/**
 * @brief Returns the number of unique strings currently interned
 *
 * @param pool Pool to query (must not be NULL — contract)
 */
static inline usize intern_count(borrowed(const InternPool*) pool) {
    require_msg(pool != NULL, "intern_count: pool is NULL");
    return pool->count;
}

/**
 * @brief Returns the current load factor of the pool (count / capacity)
 *
 * Values above 0.7 indicate probe chains are likely growing.
 *
 * @param pool Pool to query (must not be NULL — contract)
 * @return Load factor in [0.0, 1.0]
 */
static inline f64 intern_load_factor(borrowed(const InternPool*) pool) {
    require_msg(pool != NULL, "intern_load_factor: pool is NULL");
    if (pool->capacity == 0) return 0.0;
    return (f64)pool->count / (f64)pool->capacity;
}

#endif /* CANON_UTIL_INTERN_H */
