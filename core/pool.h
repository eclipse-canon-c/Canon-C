// core/pool.h
#ifndef CANON_CORE_POOL_H
#define CANON_CORE_POOL_H

#include <stddef.h>
#include <stdbool.h>
#include "arena.h"
#include "memory.h"  // for mem_align

/**
 * @file pool.h
 * @brief Fixed-size object pool allocator backed by an Arena
 *
 * Provides fast, deterministic O(1) allocation for many objects of the same size.
 * All objects are allocated sequentially from an Arena (linear bump style).
 *
 * Key properties:
 *   - Fixed maximum number of objects (no growth)
 *   - No individual deallocation — only full reset of the entire pool
 *   - Extremely low overhead (just an Arena bump + counter)
 *   - Perfect for high-churn objects: nodes, particles, temporary structs, etc.
 *   - All allocations are aligned and live until pool/arena reset
 *
 * Usage pattern:
 *   - Initialize pool with a pre-allocated Arena and desired object count
 *   - Repeatedly call pool_alloc() until full
 *   - Reset the whole pool when done with the batch
 *
 * Example:
 *   Pool pool;
 *   pool_init(&pool, &my_arena, sizeof(MyStruct), 1000);
 *   MyStruct* obj = pool_alloc(&pool);
 *   if (obj) { ... }
 *   pool_reset(&pool);  // all objects invalid, ready for reuse
 */

/**
 * @brief Fixed-size object pool backed by an Arena
 */
typedef struct {
    Arena* arena;        ///< Backing arena (caller-owned, must remain valid)
    size_t object_size;  ///< Aligned size of each individual object
    size_t capacity;     ///< Maximum number of objects that can be allocated
    size_t used;         ///< Current number of allocated objects
} Pool;

/**
 * @brief Initializes the pool using an existing Arena
 *
 * @param pool         Pointer to uninitialized Pool struct
 * @param arena        Valid, initialized Arena with sufficient remaining space
 * @param object_size  Size of each object in bytes (> 0)
 * @param max_objects  Maximum number of objects the pool should support (> 0)
 * @return             true on success, false if:
 *                     - invalid parameters
 *                     - insufficient space in arena
 *
 * Postconditions:
 *   - pool->object_size is naturally aligned (via mem_align)
 *   - pool->used == 0
 *   - All future allocations happen via arena_alloc()
 */
static inline bool pool_init(Pool* pool, Arena* arena, size_t object_size, size_t max_objects) {
    if (!pool || !arena || object_size == 0 || max_objects == 0) {
        return false;
    }

    size_t aligned_size = mem_align(object_size);
    size_t needed = aligned_size * max_objects;

    if (needed > arena_remaining(arena)) {
        return false;
    }

    pool->arena       = arena;
    pool->object_size = aligned_size;
    pool->capacity    = max_objects;
    pool->used        = 0;

    return true;
}

/**
 * @brief Allocates one object from the pool
 *
 * @param pool Valid initialized Pool
 * @return     Aligned pointer to new object, or NULL if:
 *             - pool is invalid
 *             - pool is full (used >= capacity)
 *             - underlying arena allocation failed
 *
 * Note: The returned pointer is aligned and zero-initialized only if you choose to do so.
 * All objects are contiguous in arena memory.
 */
static inline void* pool_alloc(Pool* pool) {
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
 * @brief Returns the current number of allocated objects
 */
static inline size_t pool_used(const Pool* pool) {
    return pool ? pool->used : 0;
}

/**
 * @brief Returns the maximum capacity (total objects possible)
 */
static inline size_t pool_capacity(const Pool* pool) {
    return pool ? pool->capacity : 0;
}

/**
 * @brief Checks if the pool has reached its maximum capacity
 */
static inline bool pool_is_full(const Pool* pool) {
    return pool && pool->used >= pool->capacity;
}

/**
 * @brief Resets the pool to empty state (invalidates all objects)
 *
 * Rolls back the arena to the state before any pool allocations were made.
 * Does **not** zero the memory — only moves the arena cursor back.
 *
 * After reset:
 *   - pool->used == 0
 *   - All previous pointers become invalid
 *   - Pool can be reused immediately
 */
static inline void pool_reset(Pool* pool) {
    if (!pool || !pool->arena) {
        return;
    }

    // Calculate how much to roll back (used objects × object_size)
    size_t rollback_bytes = pool->object_size * pool->used;

    ArenaMark current_mark = arena_mark(pool->arena);
    arena_reset_to(pool->arena, current_mark - rollback_bytes);

    pool->used = 0;
}

#endif /* CANON_CORE_POOL_H */
