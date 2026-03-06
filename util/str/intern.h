#ifndef CANON_UTIL_INTERN_H
#define CANON_UTIL_INTERN_H

#include "core/primitives/types.h"
#include "core/memory.h"
#include "core/arena.h"
#include "util/str/str_view.h"

/**
 * @file util/intern.h
 * @brief Minimal string interning — returns shared immutable str_view_t
 *
 * Interning maps unique strings to a single shared copy.
 * All returned views point into the arena — lifetime = arena lifetime.
 *
 * Core properties:
 * - Zero-copy views — returns str_view_t (ptr + len)
 * - Arena-backed — no heap unless you use a heap arena
 * - Deterministic — fixed capacity, no resizing
 * - Collision-safe — uses FNV-1a + linear probing
 * - Empty string & NULL handled efficiently
 * - Thread-safe if arena access is synchronized
 *
 * Usage pattern:
 * ────────────────────────────────────────────────────────────────────────────
 *   InternPool pool;
 *   intern_pool_init(&pool, arena, 1024);
 *   str_view_t s1 = intern_string(&pool, "hello");
 *   str_view_t s2 = intern_string(&pool, "hello"); // same pointer as s1
 *
 * When full: returns str_view_null() — caller must handle.
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
 * Returns a non-zero hash suitable for use as a slot index seed.
 * The value 0 is remapped to 1 to avoid collision with the empty-slot
 * sentinel (entries with ptr == NULL and hash == 0).
 *
 * @param s   Pointer to byte data
 * @param len Number of bytes
 * @return Non-zero 64-bit hash
 *
 * @remark Internal — do not call directly.
 */
static inline u64 _intern_hash(const char* s, usize len) {
    u64 h = 14695981039346656037ULL;
    for (usize i = 0; i < len; i++) {
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
 * If the string already exists in the pool → returns the existing view
 * (same ptr, guaranteed pointer equality for the same content).
 * If new → allocates a null-terminated copy in the arena and stores the view.
 * If pool is full or arena allocation fails → returns str_view_null().
 *
 * NULL input is normalized to empty string "".
 *
 * @param pool Valid initialized InternPool — must not be NULL
 * @param s    Null-terminated string to intern (NULL → treated as "")
 * @return Shared str_view_t pointing into the arena on success
 *         str_view_null() if pool is full or arena allocation fails
 *
 * @remark Returned view is valid only as long as the backing arena is alive
 * @remark Do NOT free the returned view — it is not heap-allocated
 * @remark Pointer equality (intern_equals) is valid only for views from the
 *         same pool
 */
static inline str_view_t intern_string(InternPool* pool, const char* s) {
    if (!pool || !pool->arena || !pool->entries) return str_view_null();

    if (!s) s = "";

    usize len    = str_len(s);
    u64   hash   = _intern_hash(s, len);
    usize index  = (usize)(hash % pool->capacity);
    usize probes = 0;

    while (probes < pool->capacity) {
        str_view_t* entry = &pool->entries[index];

        if (entry->ptr == NULL) {
            /* Empty slot — insert new entry */
            if (pool->count >= pool->capacity) return str_view_null();

            char* copy = (char*)arena_alloc(pool->arena, len + 1);
            if (!copy) return str_view_null();

            mem_copy(copy, s, len);
            copy[len] = '\0';

            *entry = (str_view_t){ copy, len };
            pool->count++;
            return *entry;
        }

        /* Occupied slot — check for match */
        if (entry->len == len && mem_equal(entry->ptr, s, len)) {
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
 * Optionally pre-interns the empty string so that intern_string(pool, "")
 * always returns a consistent non-null view.
 *
 * @param pool     Pool struct to initialize — must not be NULL
 * @param arena    Arena that will own all interned string data — must not be NULL
 * @param capacity Number of unique string slots (power of 2 recommended)
 * @return true on success, false if arena allocation fails or arguments invalid
 *
 * @remark capacity == 0 is invalid — returns false
 * @remark Pool is valid only as long as arena is alive and has not been reset
 */
static inline bool intern_pool_init(InternPool* pool, Arena* arena, usize capacity) {
    if (!pool || !arena || capacity == 0) return false;

    str_view_t* entries = (str_view_t*)arena_alloc(
        arena, capacity * sizeof(str_view_t)
    );
    if (!entries) return false;

    mem_zero(entries, capacity * sizeof(str_view_t));

    *pool = (InternPool){
        .arena    = arena,
        .entries  = entries,
        .capacity = capacity,
        .count    = 0
    };

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
 *
 * @param a First interned view
 * @param b Second interned view
 * @return true if both views point to the same interned string
 */
static inline bool intern_equals(str_view_t a, str_view_t b) {
    return a.ptr == b.ptr;
}

/**
 * @brief Returns the number of unique strings currently interned
 *
 * @param pool Pool to query (NULL-safe — returns 0)
 * @return Current interned string count
 */
static inline usize intern_count(const InternPool* pool) {
    return pool ? pool->count : 0;
}

/**
 * @brief Returns the current load factor of the pool (count / capacity)
 *
 * Values above 0.7 indicate the pool is approaching capacity and
 * probe chains are likely growing. Consider a larger capacity.
 *
 * @param pool Pool to query (NULL-safe — returns 0.0)
 * @return Load factor in [0.0, 1.0]
 */
static inline f64 intern_load_factor(const InternPool* pool) {
    if (!pool || pool->capacity == 0) return 0.0;
    return (f64)pool->count / (f64)pool->capacity;
}

#endif /* CANON_UTIL_INTERN_H */
