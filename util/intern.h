#ifndef CANON_UTIL_INTERN_H
#define CANON_UTIL_INTERN_H

#include "core/primitives/types.h"       // usize, bool, uint64_t
#include "core/memory.h"                 // str_len, str_ncompare, mem_copy, mem_equal
#include "core/arena.h"                  // Arena, arena_alloc
#include "util/str_view.h"               // str_view_t

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
 *   InternPool pool;
 *   intern_pool_init(&pool, arena, 1024);
 *   str_view_t s1 = intern_string(&pool, "hello");
 *   str_view_t s2 = intern_string(&pool, "hello"); // same pointer as s1
 *
 * When full: returns str_view_null() — caller must handle
 */

/**
 * @brief Simple fixed-size string interning pool
 */
typedef struct {
    Arena*      arena;          // all strings live here
    str_view_t* entries;        // hash table: pointer + length pairs
    usize       capacity;       // number of slots
    usize       count;          // number of unique strings
} InternPool;

/* ────────────────────────────────────────────────────────────────────────────
   Initialization & destruction
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Initializes an interning pool backed by given arena
 *
 * @param pool      Pool to initialize
 * @param arena     Arena that will own all interned strings
 * @param capacity  Number of unique strings expected (power of 2 recommended)
 * @return true on success, false if allocation failed
 */
static inline bool intern_pool_init(InternPool* pool, Arena* arena, usize capacity) {
    if (!pool || !arena || capacity == 0) return false;

    str_view_t* entries = (str_view_t*)arena_alloc(arena, capacity * sizeof(str_view_t));
    if (!entries) return false;

    // Zero-init (important for mem_equal checks)
    mem_zero(entries, capacity * sizeof(str_view_t));

    *pool = (InternPool){
        .arena     = arena,
        .entries   = entries,
        .capacity  = capacity,
        .count     = 0
    };

    // Optional: pre-intern empty string
    intern_string(pool, "");

    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Core interning function
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Interns a null-terminated string — returns shared view
 *
 * If string already exists → returns existing view (same ptr)
 * If new → allocates copy in arena, returns new view
 * If pool full or allocation fails → returns str_view_null()
 *
 * @param pool Pool to use
 * @param s    Null-terminated string to intern (NULL → empty string)
 * @return Shared str_view_t pointing into arena
 *         str_view_null() on failure/full
 */
static inline str_view_t intern_string(InternPool* pool, const char* s) {
    if (!pool || !pool->arena || !pool->entries) {
        return str_view_null();
    }

    // Normalize NULL → empty string
    if (!s) s = "";

    usize len = str_len(s);
    uint64_t hash = 0;

    // FNV-1a hash (fast, good distribution)
    {
        uint64_t h = 14695981039346656037ULL; // FNV offset basis
        for (usize i = 0; i < len; i++) {
            h ^= (uint64_t)(unsigned char)s[i];
            h *= 1099511628211ULL; // FNV prime
        }
        hash = h;
    }

    // Linear probing
    usize index = hash % pool->capacity;
    usize probes = 0;

    while (probes < pool->capacity) {
        str_view_t* entry = &pool->entries[index];

        if (entry->ptr == NULL) {
            // Empty slot — insert new
            if (pool->count >= pool->capacity) {
                return str_view_null(); // full
            }

            char* copy = (char*)arena_alloc(pool->arena, len + 1);
            if (!copy) return str_view_null();

            mem_copy(copy, s, len);
            copy[len] = '\0';

            *entry = (str_view_t){ copy, len };
            pool->count++;
            return *entry;
        }

        // Existing entry — compare
        if (entry->len == len && mem_equal(entry->ptr, s, len)) {
            return *entry; // hit
        }

        // Probe next
        index = (index + 1) % pool->capacity;
        probes++;
    }

    // Table full (very unlikely with good capacity)
    return str_view_null();
}

/* ────────────────────────────────────────────────────────────────────────────
   Quick helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if two interned views are the exact same string (pointer equality)
 */
static inline bool intern_equals(str_view_t a, str_view_t b) {
    return a.ptr == b.ptr;  // interned → same pointer means same string
}

/**
 * @brief Returns number of unique strings currently interned
 */
static inline usize intern_count(const InternPool* pool) {
    return pool ? pool->count : 0;
}

/**
 * @brief Returns current load factor (count / capacity)
 */
static inline double intern_load_factor(const InternPool* pool) {
    if (!pool || pool->capacity == 0) return 0.0;
    return (double)pool->count / (double)pool->capacity;
}

#endif /* CANON_UTIL_INTERN_H */
