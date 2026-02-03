#ifndef CANON_CORE_POOL_H
#define CANON_CORE_POOL_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include "arena.h"
#include "memory.h"

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
 * - Thread-safety: Not thread-safe (caller must synchronize if needed)
 *
 * Usage pattern:
 * - Initialize pool with a pre-allocated Arena and desired object count
 * - Repeatedly call pool_alloc() until full
 * - Reset the whole pool when done with the batch
 *
 * Portability:
 * - Requires C99 or later (inline, stdbool.h, stddef.h)
 * - Depends on arena.h and memory.h from this library
 * - No platform-specific code
 *
 * Performance characteristics:
 * - pool_alloc(): O(1) - single pointer bump
 * - pool_reset(): O(1) - single mark rollback
 * - All queries: O(1)
 *
 * Example:
 * uint8_t buffer[4096];
 * Arena arena;
 * arena_init(&arena, buffer, sizeof(buffer));
 *
 * Pool pool;
 * pool_init(&pool, &arena, sizeof(MyStruct), 100);
 *
 * MyStruct* obj = pool_alloc(&pool);
 * if (obj) {
 * // Use obj...
 * }
 *
 * pool_reset(&pool); // All objects invalid, ready for reuse
 */

/**
 * @brief Fixed-size object pool backed by an Arena
 *
 * Internal state tracking for pool allocator. Do not access fields directly;
 * use the provided functions instead.
 */
typedef struct {
    Arena* arena;         ///< Backing arena (caller-owned, must remain valid)
    size_t object_size;   ///< Aligned size of each individual object
    size_t capacity;      ///< Maximum number of objects that can be allocated
    size_t used;          ///< Current number of allocated objects
    ArenaMark base_mark;  ///< Arena position when pool was initialized
} Pool;

/**
 * @brief Initializes the pool using an existing Arena
 *
 * Reserves space in the arena for max_objects * object_size bytes.
 * The reservation is made implicitly — actual allocation happens per-object.
 *
 * @param pool Pointer to uninitialized Pool struct
 * @param arena Valid, initialized Arena with sufficient remaining space
 * @param object_size Size of each object in bytes (> 0)
 * @param max_objects Maximum number of objects the pool should support (> 0)
 * @return true on success, false if:
 * - any parameter is NULL/invalid
 * - object_size or max_objects is 0
 * - insufficient space in arena
 * - overflow would occur (object_size * max_objects > SIZE_MAX)
 *
 * Preconditions:
 * - pool != NULL
 * - arena != NULL and initialized
 * - object_size > 0
 * - max_objects > 0
 * - arena has enough remaining space
 *
 * Postconditions on success:
 * - pool->object_size is naturally aligned (via mem_align)
 * - pool->used == 0
 * - pool->base_mark captures current arena position
 * - All future allocations happen via arena_alloc()
 *
 * Postconditions on failure:
 * - pool state is undefined (do not use)
 * - arena state is unchanged
 *
 * Example:
 * Pool node_pool;
 * if (!pool_init(&node_pool, &arena, sizeof(Node), 1000)) {
 *     // Handle error - insufficient space
 * }
 */
static inline bool pool_init(Pool* pool, Arena* arena, size_t object_size, size_t max_objects) {
    assert(pool != NULL && "pool_init: pool parameter cannot be NULL");
    assert(arena != NULL && "pool_init: arena parameter cannot be NULL");
    assert(object_size > 0 && "pool_init: object_size must be greater than 0");
    assert(max_objects > 0 && "pool_init: max_objects must be greater than 0");

    if (!pool || !arena || object_size == 0 || max_objects == 0) {
        return false;
    }

    size_t aligned_size = mem_align(object_size);
    if (aligned_size == 0 || max_objects > SIZE_MAX / aligned_size) {
        return false;
    }

    size_t needed = aligned_size * max_objects;
    if (needed > arena_remaining(arena)) {
        return false;
    }

    pool->arena = arena;
    pool->object_size = aligned_size;
    pool->capacity = max_objects;
    pool->used = 0;
    pool->base_mark = arena_mark(arena);
    return true;
}

/**
 * @brief Allocates one object from the pool
 *
 * Returns a pointer to uninitialized memory. The caller is responsible for
 * initialization if needed (use pool_alloc_zero() for zero-initialized memory).
 *
 * @param pool Valid initialized Pool
 * @return Aligned pointer to new object, or NULL if:
 * - pool is invalid/NULL
 * - pool is full (used >= capacity)
 * - underlying arena allocation failed
 *
 * Preconditions:
 * - pool was successfully initialized via pool_init()
 * - pool is not full
 *
 * Postconditions on success:
 * - pool->used is incremented by 1
 * - returned pointer is aligned to natural boundary
 * - memory contents are undefined (not zeroed)
 *
 * Note: All objects are contiguous in arena memory for cache efficiency.
 *
 * Example:
 * Node* node = pool_alloc(&pool);
 * if (!node) {
 *     // Pool is full
 *     return;
 * }
 * // Initialize node fields...
 */
static inline void* pool_alloc(Pool* pool) {
    assert(pool != NULL && "pool_alloc: pool parameter cannot be NULL");

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
 * Convenience function that combines pool_alloc() with zero-initialization.
 * Slightly slower than pool_alloc() due to memset, but safer for complex types.
 *
 * @param pool Valid initialized Pool
 * @return Aligned pointer to zero-initialized object, or NULL if allocation fails
 *
 * Example:
 * struct Data { int x; char* ptr; } *data;
 * data = pool_alloc_zero(&pool); // All fields zeroed (ptr is NULL, x is 0)
 */
static inline void* pool_alloc_zero(Pool* pool) {
    void* ptr = pool_alloc(pool);
    if (ptr) {
        mem_zero(ptr, pool->object_size);
    }
    return ptr;
}

/**
 * @brief Attempts to allocate one object, returns success + pointer
 *
 * @param pool Valid pool
 * @param out Pointer to store result (set to NULL on failure)
 * @return true on success, false on failure (full or invalid)
 */
static inline bool pool_try_alloc(Pool* pool, void** out) {
    void* p = pool_alloc(pool);
    if (out) *out = p;
    return p != NULL;
}

/**
 * @brief Attempts to allocate and zero one object, returns success + pointer
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
 *
 * @param pool Pool to query (NULL returns 0)
 * @return Number of objects currently allocated (0 to capacity)
 */
static inline size_t pool_used(const Pool* pool) {
    return pool ? pool->used : 0;
}

/**
 * @brief Returns the maximum capacity (total objects possible)
 *
 * @param pool Pool to query (NULL returns 0)
 * @return Maximum number of objects this pool can hold
 */
static inline size_t pool_capacity(const Pool* pool) {
    return pool ? pool->capacity : 0;
}

/**
 * @brief Returns the number of objects that can still be allocated
 *
 * @param pool Pool to query (NULL returns 0)
 * @return Number of free slots remaining (capacity - used)
 */
static inline size_t pool_remaining(const Pool* pool) {
    return pool ? (pool->capacity - pool->used) : 0;
}

/**
 * @brief Checks if the pool has reached its maximum capacity
 *
 * @param pool Pool to check (NULL returns true)
 * @return true if no more objects can be allocated, false otherwise
 */
static inline bool pool_is_full(const Pool* pool) {
    return !pool || pool->used >= pool->capacity;
}

/**
 * @brief Checks if the pool is empty (no allocations)
 *
 * @param pool Pool to check (NULL returns true)
 * @return true if no objects are allocated, false otherwise
 */
static inline bool pool_is_empty(const Pool* pool) {
    return !pool || pool->used == 0;
}

/**
 * @brief Returns the size of each object (aligned)
 *
 * @param pool Pool to query (NULL returns 0)
 * @return Aligned object size in bytes
 */
static inline size_t pool_object_size(const Pool* pool) {
    return pool ? pool->object_size : 0;
}

/**
 * @brief Returns the total memory used by allocated objects
 *
 * @param pool Pool to query (NULL returns 0)
 * @return Number of bytes occupied by allocated objects
 */
static inline size_t pool_memory_used(const Pool* pool) {
    return pool ? (pool->object_size * pool->used) : 0;
}

/**
 * @brief Returns the total memory reserved for all objects
 *
 * @param pool Pool to query (NULL returns 0)
 * @return Number of bytes reserved (capacity * object_size)
 */
static inline size_t pool_memory_reserved(const Pool* pool) {
    return pool ? (pool->object_size * pool->capacity) : 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Reset functions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Resets the pool to empty state (invalidates all objects)
 *
 * Rolls back the arena to the state before any pool allocations were made.
 * Does **not** zero the memory — only moves the arena cursor back.
 *
 * After reset:
 * - pool->used == 0
 * - All previous pointers become invalid (dangling)
 * - Pool can be reused immediately
 * - Arena state is restored to pool initialization point
 *
 * @param pool Pool to reset (NULL-safe, does nothing)
 *
 * Warning: All pointers returned by pool_alloc() become invalid after reset.
 * Accessing them results in undefined behavior.
 *
 * Example:
 * for (int i = 0; i < 100; i++) {
 *     Node* n = pool_alloc(&pool);
 *     process(n);
 * }
 * pool_reset(&pool); // All nodes invalid, pool ready for reuse
 */
static inline void pool_reset(Pool* pool) {
    if (!pool || !pool->arena) {
        return;
    }
    // Roll back arena to the position when pool was initialized
    arena_reset_to(pool->arena, pool->base_mark);
    pool->used = 0;
}

/**
 * @brief Resets the pool and zeros all memory used by objects
 *
 * Same as pool_reset(), but also zeros the memory that was used.
 * Use this for security-sensitive data (passwords, keys, etc.)
 *
 * @param pool Pool to reset (NULL-safe, does nothing)
 *
 * Performance: O(used * object_size) due to memset
 *
 * Example:
 * Pool key_pool;
 * pool_init(&key_pool, &arena, sizeof(CryptoKey), 10);
 * // ... use keys ...
 * pool_reset_secure(&key_pool); // Zero memory before reset
 */
static inline void pool_reset_secure(Pool* pool) {
    if (!pool || !pool->arena || pool->used == 0) {
        return;
    }
    // Zero the memory used by allocated objects
    void* start = pool->arena->buffer + pool->base_mark;
    size_t bytes_used = pool->object_size * pool->used;
    mem_secure_zero(start, bytes_used);
    // Then reset normally
    pool_reset(pool);
}

/* ────────────────────────────────────────────────────────────────────────────
   Type-safe pool allocation macros
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe wrapper for pool allocation
 *
 * Example: Node* node = pool_alloc_type(&pool, Node);
 */
#define pool_alloc_type(pool, Type) \
    ((Type*)pool_alloc(pool))

/**
 * @brief Type-safe wrapper for zero-initialized pool allocation
 *
 * Example: Node* node = pool_alloc_type_zero(&pool, Node);
 */
#define pool_alloc_type_zero(pool, Type) \
    ((Type*)pool_alloc_zero(pool))

/* ────────────────────────────────────────────────────────────────────────────
   Usage Examples (not compiled, for documentation only)
   ────────────────────────────────────────────────────────────────────────────
    #include "pool.h"

    typedef struct Node {
        int data;
        struct Node* next;
    } Node;

    void example(void) {
        // Setup arena and pool
        uint8_t buffer[8192];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));

        Pool node_pool;
        if (!pool_init(&node_pool, &arena, sizeof(Node), 100)) {
            // Handle insufficient space
            return;
        }

        // Allocate objects
        Node* head = pool_alloc_type(&node_pool, Node);
        Node* n2 = pool_alloc_type_zero(&node_pool, Node); // Zero-initialized

        // Check capacity
        printf("Used: %zu / %zu\n", pool_used(&node_pool),
                                     pool_capacity(&node_pool));

        if (pool_is_full(&node_pool)) {
            printf("Pool exhausted!\n");
        }

        // Reset for reuse
        pool_reset(&node_pool);

        // Or secure reset for sensitive data
        pool_reset_secure(&node_pool);
    }

    // Batch processing pattern
    void batch_process(void) {
        Pool temp_pool;
        pool_init(&temp_pool, &arena, sizeof(TempData), 1000);

        while (has_work()) {
            // Allocate batch of temporary objects
            for (int i = 0; i < 100; i++) {
                TempData* temp = pool_alloc(&temp_pool);
                process(temp);
            }

            // Reset entire batch at once
            pool_reset(&temp_pool);
        }
    }
   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_POOL_H */
