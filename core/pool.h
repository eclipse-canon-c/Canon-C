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
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * pool.h is core/. It depends only on primitives/, core/arena.h,
 * core/memory.h, and core/slice.h.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pool_alloc():     O(1)
 * - pool_get():       O(1) — ptr_elem index calculation
 * - pool_reset():     O(1)
 * - pool_as_bytes():  O(1)
 * - All queries:      O(1)
 *
 * @sa pool_init(), pool_alloc(), pool_get(), pool_reset()
 * @sa core/primitives/ptr.h — ptr_elem used for index access
 * @sa core/slice.h          — bytes_t used by pool_as_bytes()
 */

/* ════════════════════════════════════════════════════════════════════════════
   Pool struct
   ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    Arena*    arena;       ///< Backing arena (caller-owned)
    usize     object_size; ///< Aligned size of each individual object in bytes
    usize     capacity;    ///< Maximum number of objects that can be allocated
    usize     used;        ///< Current number of allocated objects
    ArenaMark base_mark;   ///< Arena position when pool was initialized
} Pool;

/* ════════════════════════════════════════════════════════════════════════════
   Initialization
   ════════════════════════════════════════════════════════════════════════════ */

static inline bool pool_init(
    Pool* pool, Arena* arena, usize object_size, usize max_objects) {

    require_msg(pool != NULL,    "pool_init: pool cannot be NULL");
    require_msg(arena != NULL,   "pool_init: arena cannot be NULL");
    require_msg(object_size > 0, "pool_init: object_size must be > 0");
    require_msg(max_objects > 0, "pool_init: max_objects must be > 0");

    if (!pool || !arena || object_size == 0 || max_objects == 0) return false;

    usize aligned_size = mem_align(object_size);
    if (aligned_size == 0 || max_objects > CANON_USIZE_MAX / aligned_size) return false;

    usize needed = aligned_size * max_objects;
    if (needed > arena_remaining(arena)) return false;

    pool->arena       = arena;
    pool->object_size = aligned_size;
    pool->capacity    = max_objects;
    pool->used        = 0;
    pool->base_mark   = arena_mark(arena);
    return true;
}

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
 * Objects are stored contiguously starting at base_mark.
 * Uses ptr_elem() from core/primitives/ptr.h for the offset calculation —
 * replaces the raw (buffer + base_mark + i * object_size) arithmetic.
 *
 * @param pool Valid initialized Pool
 * @param i    Object index (must be < pool->used)
 * @return Pointer to object at index i, or NULL if out of bounds
 *
 * Performance: O(1)
 */
static inline void* pool_get(const Pool* pool, usize i) {
    if (!pool || !pool->arena || i >= pool->used) return NULL;
    void* base = ptr_offset(pool->arena->buffer, pool->base_mark);
    return ptr_elem(base, i, pool->object_size);
}

/**
 * @brief Const variant of pool_get() for read-only access
 *
 * Performance: O(1)
 */
static inline const void* pool_get_const(const Pool* pool, usize i) {
    if (!pool || !pool->arena || i >= pool->used) return NULL;
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
 *
 * Performance: O(1)
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
 * Includes unallocated slots.
 *
 * Performance: O(1)
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
 * @post pool->used == 0
 * @post All pool_alloc() / pool_get() pointers are now invalid
 *
 * Performance: O(1)
 */
static inline void pool_reset(Pool* pool) {
    if (!pool || !pool->arena) return;
    arena_reset_to(pool->arena, pool->base_mark);
    pool->used = 0;
}

/**
 * @brief Resets the pool and securely zeros all allocated object memory
 *
 * Uses ptr_offset() to locate the pool base in the arena buffer —
 * replaces the raw (pool->arena->buffer + pool->base_mark) arithmetic.
 *
 * @post pool->used == 0
 * @post All allocated memory is zeroed
 *
 * Performance: O(used * object_size)
 */
static inline void pool_reset_secure(Pool* pool) {
    if (!pool || !pool->arena || pool->used == 0) return;
    void*  base       = ptr_offset(pool->arena->buffer, pool->base_mark);
    usize  bytes_used = pool->object_size * pool->used;
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

/** @brief Type-safe index access: pool_get_type(&pool, 3, Node) */
#define pool_get_type(pool, i, Type) \
    ((Type*)pool_get((pool), (i)))

/** @brief Type-safe const index access: pool_get_type_const(&pool, 3, Node) */
#define pool_get_type_const(pool, i, Type) \
    ((const Type*)pool_get_const((pool), (i)))

#endif /* CANON_CORE_POOL_H */
