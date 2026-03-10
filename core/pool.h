#ifndef CANON_CORE_POOL_H
#define CANON_CORE_POOL_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/bits.h"
#include "core/arena.h"
#include "core/memory.h"
#include "core/slice.h"

/**
 * @file pool.h
 * @brief Fixed-size object pool allocator backed by an Arena
 *
 * Provides fast, deterministic O(1) allocation for many objects of the same size.
 * All objects are allocated sequentially from an Arena (linear bump style).
 *
 * Key properties:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed maximum number of objects (no growth)
 * - No individual deallocation — only full reset of the entire pool
 * - Extremely low overhead (just an Arena bump + counter)
 * - Objects are contiguous — index access via pool_get() / pool_as_bytes()
 * - All allocations are aligned and live until pool/arena reset
 *
 * IMPORTANT: Do not interleave arena allocations with pool allocations
 * ────────────────────────────────────────────────────────────────────────────
 * The pool assumes exclusive ownership of a contiguous region of the arena
 * starting at base_mark. pool_get() calculates object addresses as:
 *
 *   base_mark + i * object_size
 *
 * If any other allocation is made from the same arena between pool_init()
 * and pool_reset(), this calculation produces wrong addresses — pointing
 * into foreign memory with no warning.
 *
 * pool_get() and pool_get_const() detect this in debug builds via ensure_msg:
 * they verify that the arena's current offset matches the expected layout.
 * In release builds this check is compiled away — do not rely on it.
 *
 * Safe patterns:
 * ✓ Use a dedicated arena exclusively for the pool
 * ✓ Use arena_mark() before pool_init() and arena_reset_to() before any
 *   other allocation if you must share the arena
 * ✓ Allocate all non-pool data before calling pool_init()
 *
 * Unsafe patterns:
 * ✗ Calling arena_alloc() on the same arena between pool_alloc() calls
 * ✗ Using the pool's arena for other purposes during the pool's lifetime
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * pool.h is core/. It depends only on primitives/, core/arena.h,
 * core/memory.h, and core/slice.h.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pool_alloc(): O(1)
 * - pool_get(): O(1) — ptr_elem index calculation
 * - pool_reset(): O(1)
 * - pool_as_bytes(): O(1)
 * - All queries: O(1)
 *
 * @sa pool_init(), pool_alloc(), pool_get(), pool_reset()
 * @sa core/primitives/ptr.h — ptr_offset, ptr_elem for safe index/offset calculation
 * @sa core/slice.h — bytes_t / bytes_from used for contiguous object views
 */

/* ════════════════════════════════════════════════════════════════════════════
   Internal helper — expected arena offset given current pool state
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the arena offset the pool expects after `used` allocations
 *
 * The pool reserves base_mark + capacity * object_size bytes at init time.
 * After `used` allocations, the arena offset should be exactly
 * base_mark + used * object_size (pool always advances by object_size per alloc).
 *
 * Used by pool_get() / pool_get_const() to detect interleaved allocations.
 *
 * Internal — do not call directly.
 */
static inline usize _pool_expected_offset(const Pool* pool) {
    return pool->base_mark + pool->used * pool->object_size;
}

/* ════════════════════════════════════════════════════════════════════════════
   Pool struct
   ════════════════════════════════════════════════════════════════════════════ */
typedef struct {
    Arena* arena;           ///< Backing arena (caller-owned, must outlive pool)
    usize object_size;      ///< Aligned size of each individual object in bytes
    usize capacity;         ///< Maximum number of objects that can be allocated
    usize used;             ///< Current number of allocated objects
    ArenaMark base_mark;    ///< Arena position when pool was initialized
} Pool;

/* ════════════════════════════════════════════════════════════════════════════
   Initialization
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a fixed-size object pool backed by an arena
 *
 * Reserves capacity * aligned(object_size) bytes from the arena starting
 * at the current arena offset. This region must not be touched by other
 * arena allocations for the lifetime of the pool.
 *
 * @param pool        Valid pointer to uninitialized Pool struct
 * @param arena       Valid initialized Arena to allocate from
 * @param object_size Size of each object (will be aligned up)
 * @param max_objects Maximum number of objects the pool can hold
 * @return true on success, false on failure (insufficient space, overflow, etc.)
 *
 * @pre object_size > 0 && max_objects > 0
 * @post On success: pool is ready, objects can be allocated until capacity
 * @post arena offset is advanced by capacity * aligned(object_size) bytes
 */
static inline bool pool_init(
    Pool* pool, Arena* arena, usize object_size, usize max_objects) {
    require_msg(pool != NULL,        "pool_init: pool cannot be NULL");
    require_msg(arena != NULL,       "pool_init: arena cannot be NULL");
    require_msg(object_size > 0,     "pool_init: object_size must be > 0");
    require_msg(max_objects > 0,     "pool_init: max_objects must be > 0");

    if (!pool || !arena || object_size == 0 || max_objects == 0) {
        return false;
    }

    usize aligned_size = mem_align(object_size);
    if (aligned_size == 0 || max_objects > CANON_USIZE_MAX / aligned_size) {
        return false;  /* prevent usize overflow in needed = aligned_size * max_objects */
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

/** @brief Type-safe pool initialization for a fixed object type */
#define pool_init_type(pool, arena, Type, max_objects) \
    pool_init((pool), (arena), sizeof(Type), (max_objects))

/* ════════════════════════════════════════════════════════════════════════════
   Allocation
   ════════════════════════════════════════════════════════════════════════════ */

static inline void* pool_alloc(Pool* pool) {
    require_msg(pool != NULL, "pool_alloc: pool cannot be NULL");
    if (!pool || pool->used >= pool->capacity) return NULL;

    void* p = arena_alloc(pool->arena, pool->object_size);
    if (p) pool->used++;
    return p;
}

static inline void* pool_alloc_zero(Pool* pool) {
    void* p = pool_alloc(pool);
    if (p) mem_zero(p, pool->object_size);
    return p;
}

static inline bool pool_try_alloc(Pool* pool, void** out) {
    void* p = pool_alloc(pool);
    if (out) *out = p;
    return p != NULL;
}

static inline bool pool_try_alloc_zero(Pool* pool, void** out) {
    void* p = pool_alloc_zero(pool);
    if (out) *out = p;
    return p != NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
   Index access — ptr.h integration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a pointer to the object at index i (bounds-checked)
 *
 * Objects are stored contiguously starting at base_mark. Address is
 * calculated as: arena->buffer + base_mark + i * object_size.
 *
 * In debug builds, verifies that the arena's current offset matches the
 * expected layout — detects interleaved allocations that would corrupt
 * the pool's address calculation. This check is compiled away in release.
 *
 * @param pool  Valid initialized Pool
 * @param i     Object index (must be < pool->used)
 * @return Pointer to object at index i, or NULL if out of bounds
 *
 * @pre No arena allocations have been made from pool->arena since pool_init()
 *      other than through pool_alloc() — verified by ensure_msg in debug builds
 */
static inline void* pool_get(const Pool* pool, usize i) {
    if (!pool || !pool->arena || i >= pool->used) return NULL;

    ensure_msg(
        pool->arena->offset == _pool_expected_offset(pool),
        "pool_get: arena offset mismatch — interleaved allocations have corrupted pool layout"
    );

    void* base = ptr_offset(pool->arena->buffer, pool->base_mark);
    return ptr_elem(base, i, pool->object_size);
}

static inline const void* pool_get_const(const Pool* pool, usize i) {
    if (!pool || !pool->arena || i >= pool->used) return NULL;

    ensure_msg(
        pool->arena->offset == _pool_expected_offset(pool),
        "pool_get_const: arena offset mismatch — interleaved allocations have corrupted pool layout"
    );

    const void* base = ptr_offset_const(pool->arena->buffer, pool->base_mark);
    return ptr_elem_const(base, i, pool->object_size);
}

/* ════════════════════════════════════════════════════════════════════════════
   Query
   ════════════════════════════════════════════════════════════════════════════ */

static inline usize pool_used(const Pool* pool) {
    return pool ? pool->used : 0;
}

static inline usize pool_capacity(const Pool* pool) {
    return pool ? pool->capacity : 0;
}

static inline usize pool_remaining(const Pool* pool) {
    return pool ? (pool->capacity - pool->used) : 0;
}

static inline bool pool_is_full(const Pool* pool) {
    return !pool || pool->used >= pool->capacity;
}

static inline bool pool_is_empty(const Pool* pool) {
    return !pool || pool->used == 0;
}

static inline usize pool_object_size(const Pool* pool) {
    return pool ? pool->object_size : 0;
}

static inline usize pool_memory_used(const Pool* pool) {
    return pool ? (pool->object_size * pool->used) : 0;
}

static inline usize pool_memory_reserved(const Pool* pool) {
    return pool ? (pool->object_size * pool->capacity) : 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   Byte views — slice.h integration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a bytes_t view over all currently allocated objects
 *
 * Covers [base_mark, base_mark + used * object_size) in the arena buffer.
 * Non-owning — becomes invalid after pool_reset().
 */
static inline bytes_t pool_as_bytes(const Pool* pool) {
    if (!pool || !pool->arena || pool->used == 0) return bytes_empty();
    void* base = ptr_offset(pool->arena->buffer, pool->base_mark);
    return bytes_from(base, pool->object_size * pool->used);
}

/**
 * @brief Returns a bytes_t view over the entire reserved pool region
 *
 * Covers [base_mark, base_mark + capacity * object_size).
 * Includes unallocated slots. Non-owning.
 */
static inline bytes_t pool_reserved_bytes(const Pool* pool) {
    if (!pool || !pool->arena) return bytes_empty();
    void* base = ptr_offset(pool->arena->buffer, pool->base_mark);
    return bytes_from(base, pool->object_size * pool->capacity);
}

/* ════════════════════════════════════════════════════════════════════════════
   Reset
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resets the pool to empty — fast O(1) path, no zeroing
 *
 * Rolls the arena back to base_mark, discarding all pool allocations.
 *
 * @post pool->used == 0
 * @post All pool_alloc() / pool_get() pointers are now invalid
 */
static inline void pool_reset(Pool* pool) {
    if (!pool || !pool->arena) return;
    arena_reset_to(pool->arena, pool->base_mark);
    pool->used = 0;
}

/**
 * @brief Resets the pool and securely zeros all allocated object memory
 *
 * @post pool->used == 0
 * @post All allocated memory is zeroed
 */
static inline void pool_reset_secure(Pool* pool) {
    if (!pool || !pool->arena || pool->used == 0) return;
    void* base = ptr_offset(pool->arena->buffer, pool->base_mark);
    usize bytes_used = pool->object_size * pool->used;
    mem_secure_zero(base, bytes_used);
    pool_reset(pool);
}

/* ════════════════════════════════════════════════════════════════════════════
   Type-safe macros
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Type-safe pool allocation */
#define pool_alloc_type(pool, Type) \
    ((Type*)pool_alloc(pool))

/** @brief Type-safe zero-initialized pool allocation */
#define pool_alloc_type_zero(pool, Type) \
    ((Type*)pool_alloc_zero(pool))

/** @brief Type-safe index access */
#define pool_get_type(pool, i, Type) \
    ((Type*)pool_get((pool), (i)))

/** @brief Type-safe const index access */
#define pool_get_type_const(pool, i, Type) \
    ((const Type*)pool_get_const((pool), (i)))

#endif /* CANON_CORE_POOL_H */
