#ifndef CANON_CORE_POOL_H
#define CANON_CORE_POOL_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/arena.h"
#include "core/memory.h"

/**
 * @file pool.h
 * @brief Fixed-size object pool allocator backed by an Arena
 *
 * Provides fast, deterministic O(1) allocation for many objects of the same size.
 * All objects are allocated sequentially from an Arena (linear bump style).
 *
 * Key properties:
 * - Fixed maximum number of objects (no growth)
 * - No individual deallocation — only full reset of the entire pool
 * - Extremely low overhead (just an Arena bump + counter)
 * - Perfect for high-churn objects: nodes, particles, temporary structs, etc.
 * - All allocations are aligned and live until pool/arena reset
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe. Caller must synchronize if pool is shared across threads.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends on arena.h and memory.h from this library
 * - No platform-specific code
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * pool.h is core/. It depends only on primitives/, core/arena.h, core/memory.h.
 * No data/, semantics/, algo/, or util/ headers may be included here.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pool_alloc():  O(1) — single pointer bump
 * - pool_reset():  O(1) — single mark rollback
 * - All queries:   O(1)
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Objects of varying sizes (use arena directly)
 * - Individual deallocation (use a freelist allocator)
 * - Multi-threaded allocation without external synchronization
 *
 * @sa pool_init(), pool_alloc(), pool_reset()
 */

/* ────────────────────────────────────────────────────────────────────────────
   Pool struct
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Fixed-size object pool backed by an Arena
 *
 * Do not access or modify fields directly.
 * Use the provided functions to interact with the pool.
 */
typedef struct {
    Arena*    arena;       ///< Backing arena (caller-owned, must remain valid)
    usize     object_size; ///< Aligned size of each individual object
    usize     capacity;    ///< Maximum number of objects that can be allocated
    usize     used;        ///< Current number of allocated objects
    ArenaMark base_mark;   ///< Arena position when pool was initialized
} Pool;

/* ────────────────────────────────────────────────────────────────────────────
   Initialization
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Initializes the pool using an existing Arena
 *
 * Verifies that the arena has sufficient remaining space for
 * max_objects * aligned(object_size) bytes. Does not pre-allocate —
 * actual memory is bumped per object_alloc() call.
 *
 * @param pool        Pointer to uninitialized Pool struct
 * @param arena       Valid, initialized Arena with sufficient remaining space
 * @param object_size Size of each object in bytes (> 0)
 * @param max_objects Maximum number of objects (> 0)
 *
 * @return true on success, false if:
 *         - any parameter is NULL or invalid
 *         - object_size or max_objects is 0
 *         - overflow: aligned_size * max_objects > CANON_USIZE_MAX
 *         - insufficient space in arena
 *
 * @pre pool != NULL
 * @pre arena != NULL and initialized via arena_init()
 * @pre object_size > 0
 * @pre max_objects > 0
 *
 * @post On success: pool->used == 0, pool->base_mark captures arena position
 * @post On failure: pool state undefined — do not use
 */
static inline bool pool_init(Pool* pool, Arena* arena, usize object_size, usize max_objects) {
    require_msg(pool != NULL,         "pool_init: pool cannot be NULL");
    require_msg(arena != NULL,        "pool_init: arena cannot be NULL");
    require_msg(object_size > 0,      "pool_init: object_size must be > 0");
    require_msg(max_objects > 0,      "pool_init: max_objects must be > 0");

    if (!pool || !arena || object_size == 0 || max_objects == 0) {
        return false;
    }

    usize aligned_size = mem_align(object_size);
    if (aligned_size == 0 || max_objects > CANON_USIZE_MAX / aligned_size) {
        return false;
    }

    usize needed = aligned_size * max_objects;
    if (needed > arena_remaining(arena)) {
        return false;
    }

    pool->arena       = arena;
    pool->object_size = aligned_size;
    pool->capacity    = max_objects;
    pool->used        = 0;
    pool->base_mark   = arena_mark(arena);
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Allocation
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates one object from the pool
 *
 * Returns a pointer to uninitialized memory. Caller is responsible for
 * initialization. Use pool_alloc_zero() for zero-initialized memory.
 *
 * @param pool Valid initialized Pool
 * @return Aligned pointer to new object, or NULL if:
 *         - pool is NULL or invalid
 *         - pool is full (used >= capacity)
 *         - underlying arena allocation failed
 *
 * @pre pool was successfully initialized via pool_init()
 *
 * @post On success: pool->used incremented by 1
 * @post Memory contents are undefined (not zeroed)
 */
static inline void* pool_alloc(Pool* pool) {
    require_msg(pool != NULL, "pool_alloc: pool cannot be NULL");

    if (!pool || pool->used >= pool->capacity) {
        return NULL;
    }

    void* ptr = arena_alloc(pool->arena, pool->object_size);
    if (ptr) {
        pool->used++;
    }
    return ptr;
}

/**
 * @brief Allocates and zero-initializes one object from the pool
 *
 * @param pool Valid initialized Pool
 * @return Aligned pointer to zeroed object, or NULL on failure
 */
static inline void* pool_alloc_zero(Pool* pool) {
    void* ptr = pool_alloc(pool);
    if (ptr) {
        mem_zero(ptr, pool->object_size);
    }
    return ptr;
}

/**
 * @brief Attempts allocation, returns success flag and pointer via out param
 *
 * @param pool Valid pool
 * @param out  Receives allocated pointer (NULL on failure)
 * @return true on success, false on failure
 */
static inline bool pool_try_alloc(Pool* pool, void** out) {
    void* p = pool_alloc(pool);
    if (out) *out = p;
    return p != NULL;
}

/**
 * @brief Attempts zero-initialized allocation, returns success flag and pointer
 *
 * @param pool Valid pool
 * @param out  Receives zeroed pointer (NULL on failure)
 * @return true on success, false on failure
 */
static inline bool pool_try_alloc_zero(Pool* pool, void** out) {
    void* p = pool_alloc_zero(pool);
    if (out) *out = p;
    return p != NULL;
}

/* ────────────────────────────────────────────────────────────────────────────
   Query functions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns the current number of allocated objects
 * @param pool Pool to query (NULL → 0)
 */
static inline usize pool_used(const Pool* pool) {
    return pool ? pool->used : 0;
}

/**
 * @brief Returns the maximum number of objects this pool can hold
 * @param pool Pool to query (NULL → 0)
 */
static inline usize pool_capacity(const Pool* pool) {
    return pool ? pool->capacity : 0;
}

/**
 * @brief Returns the number of free slots remaining (capacity - used)
 * @param pool Pool to query (NULL → 0)
 */
static inline usize pool_remaining(const Pool* pool) {
    return pool ? (pool->capacity - pool->used) : 0;
}

/**
 * @brief Returns true if no more objects can be allocated
 * @param pool Pool to check (NULL → true)
 */
static inline bool pool_is_full(const Pool* pool) {
    return !pool || pool->used >= pool->capacity;
}

/**
 * @brief Returns true if no objects are currently allocated
 * @param pool Pool to check (NULL → true)
 */
static inline bool pool_is_empty(const Pool* pool) {
    return !pool || pool->used == 0;
}

/**
 * @brief Returns the aligned size of each object in bytes
 * @param pool Pool to query (NULL → 0)
 */
static inline usize pool_object_size(const Pool* pool) {
    return pool ? pool->object_size : 0;
}

/**
 * @brief Returns total bytes occupied by allocated objects
 * @param pool Pool to query (NULL → 0)
 */
static inline usize pool_memory_used(const Pool* pool) {
    return pool ? (pool->object_size * pool->used) : 0;
}

/**
 * @brief Returns total bytes reserved for all objects (capacity * object_size)
 * @param pool Pool to query (NULL → 0)
 */
static inline usize pool_memory_reserved(const Pool* pool) {
    return pool ? (pool->object_size * pool->capacity) : 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Reset functions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Resets the pool to empty state (fast path)
 *
 * Rolls back the arena to the state before any pool allocations.
 * Does NOT zero memory — only moves the arena cursor back.
 *
 * @param pool Pool to reset (NULL-safe)
 *
 * @post pool->used == 0
 * @post All pointers returned by pool_alloc() are now invalid (dangling)
 * @post Arena state restored to pool initialization point
 *
 * WARNING: Accessing any pointer returned by pool_alloc() after reset
 * is undefined behavior.
 */
static inline void pool_reset(Pool* pool) {
    if (!pool || !pool->arena) return;
    arena_reset_to(pool->arena, pool->base_mark);
    pool->used = 0;
}

/**
 * @brief Resets the pool and securely zeros all allocated object memory
 *
 * Use for security-sensitive data (keys, passwords, tokens).
 * Slower than pool_reset() — O(used * object_size).
 *
 * @param pool Pool to reset (NULL-safe)
 *
 * @post pool->used == 0
 * @post All previously allocated memory is zeroed
 * @post All pointers returned by pool_alloc() are invalid
 */
static inline void pool_reset_secure(Pool* pool) {
    if (!pool || !pool->arena || pool->used == 0) return;
    void* start = pool->arena->buffer + pool->base_mark;
    usize bytes_used = pool->object_size * pool->used;
    mem_secure_zero(start, bytes_used);
    pool_reset(pool);
}

/* ────────────────────────────────────────────────────────────────────────────
   Type-safe allocation macros
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe pool allocation
 *
 * Example:
 * ```c
 * Node* node = pool_alloc_type(&pool, Node);
 * ```
 */
#define pool_alloc_type(pool, Type) \
    ((Type*)pool_alloc(pool))

/**
 * @brief Type-safe zero-initialized pool allocation
 *
 * Example:
 * ```c
 * Node* node = pool_alloc_type_zero(&pool, Node);
 * ```
 */
#define pool_alloc_type_zero(pool, Type) \
    ((Type*)pool_alloc_zero(pool))

#endif /* CANON_CORE_POOL_H */
