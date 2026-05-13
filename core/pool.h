#ifndef CANON_CORE_POOL_H
#define CANON_CORE_POOL_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/arena.h"
#include "core/memory.h"
#include "core/slice.h"

#ifdef CANON_LIFETIME_DEBUG
    #include "core/primitives/lifetime.h"  /* region_id_t, lifetime_t */
#endif

/**
 * @file pool.h
 * @brief Fixed-size object pool allocator backed by an Arena
 *
 * Provides fast, deterministic O(1) allocation for many objects of the same size.
 * Objects are allocated sequentially from a contiguous region reserved upfront
 * from the arena at pool_init() time.
 *
 * Key properties:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed maximum capacity (no growth)
 * - No individual deallocation — only full pool reset
 * - O(1) alloc, get, reset, query
 * - Objects are contiguous — index access via pool_get() / pool_as_bytes()
 * - All allocated objects live until pool_reset() or arena teardown
 *
 * Ownership and arena usage:
 * ────────────────────────────────────────────────────────────────────────────
 * pool_init() reserves capacity * object_size bytes from the arena immediately
 * and advances the arena offset by that amount. The pool owns this region
 * exclusively for its lifetime.
 *
 * You MAY make other arena allocations after pool_init() — they will land
 * beyond the reserved region and will not interfere with the pool.
 *
 * pool_reset() rolls the arena back to base_mark, releasing both the pool
 * objects and everything allocated from the arena after pool_init(). If you
 * need to keep post-pool allocations alive across a pool reset, use a
 * separate arena for the pool.
 *
 * pool_get() always verifies that the object being accessed lies within the
 * reserved region — this detects use-after-reset bugs at all build levels.
 *
 * Safe patterns:
 * ✓ Dedicated arena exclusively for the pool
 * ✓ Shared arena — allocate non-pool data after pool_init()
 * ✓ Multiple pools from the same arena (each reserves its own region)
 *
 * Unsafe patterns:
 * ✗ Calling pool_reset() when you need post-init arena allocations to survive
 * ✗ Calling arena_reset() / arena_reset_secure() on the pool's backing arena
 *   while the pool is still live — see "Lifetime tracking" below.
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 * ────────────────────────────────────────────────────────────────────────────
 *   - Embeds a lifetime_t lt field (id + open) on the Pool.
 *   - pool_init() opens a fresh lifetime; the ID is derived from &pool.
 *   - pool_reset() and pool_reset_secure() RE-STAMP the pool's lifetime ID,
 *     invalidating every borrow that captured the previous ID. Borrow
 *     reads stamped with the old ID will fire require_msg via
 *     lifetime_assert_valid() in semantics/borrow.h.
 *   - Pool's lifetime is independent of the arena's. The pool's field
 *     catches pool_reset-class bugs (using a pool pointer after the
 *     pool was reset). It does NOT catch arena_reset-class bugs:
 *     if the caller resets the backing arena directly while the pool
 *     is still live, the pool's reserved region is gone but the pool's
 *     lt.id is unchanged. This is caller error and out of scope for
 *     pool's lifetime tracking — same way it would be caller error to
 *     free the arena's backing buffer while the pool is live. Use a
 *     dedicated arena for the pool if this matters.
 *   - There is no pool_destroy(); pools are reset, not destroyed.
 *   Zero cost in release builds — struct layout is identical without the flag.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pool_init()         O(1)
 * - pool_alloc()        O(1)
 * - pool_get()          O(1)
 * - pool_reset()        O(1)
 * - pool_as_bytes()     O(1)
 * - All queries         O(1)
 *
 * @sa pool_init(), pool_alloc(), pool_get(), pool_reset()
 * @sa core/primitives/ptr.h      — ptr_offset, ptr_elem for index/offset calculation
 * @sa core/slice.h               — bytes_t / bytes_from for contiguous object views
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
 * @sa core/region.h              — lifetime_assert_valid runtime check
 */

/* ════════════════════════════════════════════════════════════════════════════
   Pool struct
   ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    Arena*    arena;        /**< Backing arena (caller-owned, must outlive pool)    */
    usize     object_size;  /**< Aligned size of each object in bytes               */
    usize     capacity;     /**< Maximum number of objects                          */
    usize     used;         /**< Number of allocated objects                        */
    ArenaMark base_mark;    /**< Arena offset at pool_init() — start of reserved region */
    ArenaMark end_mark;     /**< Arena offset after reservation — base_mark + capacity * object_size */
#ifdef CANON_LIFETIME_DEBUG
    lifetime_t lt;          /**< [debug] Lifetime token: id + open                  */
#endif
} Pool;

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime helpers (compiled away in release)
   ════════════════════════════════════════════════════════════════════════════
   When CANON_LIFETIME_DEBUG is enabled, a Pool exposes a lifetime_t
   that borrows can capture and validate against. The helpers below
   manage the (id, open) pair across the pool lifecycle:

     pool_lifetime_open_(p)
       Called by pool_init. Sets id to the pool's address-derived
       value and marks the lifetime open.

     pool_lifetime_restamp_(p)
       Called by pool_reset and pool_reset_secure. XORs the id
       with a 64-bit golden-ratio constant so the new id is
       guaranteed to differ from the old, invalidating every
       previously-captured borrow without needing a separate
       generation counter. Open flag stays true; pools are not
       "closed" by reset — they are recycled.

     pool_lifetime_close_(p)
       Reserved for future Pool destruction semantics. Not called
       by any current API. Marks the lifetime closed, which makes
       any subsequent borrow read fire lifetime_assert_valid.

   In release builds (CANON_LIFETIME_DEBUG undefined) all three
   helpers are no-ops — the Pool struct does not have an lt field
   and no code touches it.
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    #define pool_lifetime_open_(p)                                        \
        do {                                                              \
            (p)->lt.id   = (region_id_t)(uintptr_t)(p);                  \
            (p)->lt.open = true;                                          \
        } while (0)
    /* XOR with golden-ratio constant guarantees the new id differs from
       the old one even when the pool address is reused. */
    #define pool_lifetime_restamp_(p)                                     \
        do {                                                              \
            (p)->lt.id ^= (region_id_t)0x9E3779B97F4A7C15ULL;             \
        } while (0)
    #define pool_lifetime_close_(p)                                       \
        do { (p)->lt.open = false; } while (0)
#else
    #define pool_lifetime_open_(p)     ((void)0)
    #define pool_lifetime_restamp_(p)  ((void)0)
    #define pool_lifetime_close_(p)    ((void)0)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Initialization
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a pool and reserves its entire region from the arena
 *
 * Immediately advances the arena offset by capacity * aligned(object_size).
 * Other arena allocations made after this call are safe — they land beyond
 * the reserved region and do not interfere with pool address calculations.
 *
 * @param pool        Valid pointer to an uninitialized Pool
 * @param arena       Valid initialized Arena
 * @param object_size Size of each object (aligned up internally)
 * @param max_objects Maximum number of objects the pool can hold
 * @return true on success, false if the arena has insufficient space or
 *         if the required size overflows usize
 *
 * @pre  pool != NULL && arena != NULL
 * @pre  object_size > 0 && max_objects > 0
 * @post On success: pool is ready; arena offset advanced by capacity * object_size
 * @post On failure: pool and arena are unchanged
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token derived
 * from &pool. Borrows captured after this call carry this lifetime ID.
 * Lifetime is only opened on successful init.
 */
static inline bool pool_init(
    Pool* pool, Arena* arena, usize object_size, usize max_objects)
{
    usize  aligned_size;
    usize  needed;
    void*  region;

    require_msg(pool        != NULL, "pool_init: pool cannot be NULL");
    require_msg(arena       != NULL, "pool_init: arena cannot be NULL");
    require_msg(object_size  > 0,    "pool_init: object_size must be > 0");
    require_msg(max_objects  > 0,    "pool_init: max_objects must be > 0");

    aligned_size = mem_align(object_size);
    if (aligned_size == 0 || max_objects > CANON_USIZE_MAX / aligned_size) {
        return false;
    }

    needed = aligned_size * max_objects;
    if (needed > arena_remaining(arena)) {
        return false;
    }

    pool->arena       = arena;
    pool->object_size = aligned_size;
    pool->capacity    = max_objects;
    pool->used        = 0;
    pool->base_mark   = arena_mark(arena);

    /* Reserve the entire region upfront. arena_alloc advances the offset,
       so subsequent non-pool allocations land safely beyond this region. */
    region = arena_alloc(arena, needed);
    if (!region) return false;

    pool->end_mark = arena_mark(arena);
    pool_lifetime_open_(pool);
    return true;
}

/** @brief Type-safe pool initialization */
#define pool_init_type(pool, arena, Type, max_objects) \
    pool_init((pool), (arena), sizeof(Type), (max_objects))

/* ════════════════════════════════════════════════════════════════════════════
   Allocation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocates the next object slot from the pool
 *
 * Returns a pointer into the pre-reserved region. Does not advance the
 * arena offset (the region was fully committed at pool_init()).
 *
 * @return Pointer to the object slot, or NULL if the pool is full
 */
static inline void* pool_alloc(Pool* pool) {
    void* base;
    void* slot;

    require_msg(pool != NULL, "pool_alloc: pool cannot be NULL");
    if (pool->used >= pool->capacity) return NULL;

    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    slot = ptr_elem(base, pool->used, pool->object_size);
    pool->used++;
    return slot;
}

/** @brief Allocates and zeroes the next object slot */
static inline void* pool_alloc_zero(Pool* pool) {
    void* p;
    p = pool_alloc(pool);
    if (p) mem_zero(p, pool->object_size);
    return p;
}

/** @brief Allocates and writes result to *out; returns false if full */
static inline bool pool_try_alloc(Pool* pool, void** out) {
    void* p;
    require_msg(pool != NULL, "pool_try_alloc: pool cannot be NULL");
    p = pool_alloc(pool);
    if (out) *out = p;
    return p != NULL;
}

/** @brief Allocates, zeroes, and writes result to *out; returns false if full */
static inline bool pool_try_alloc_zero(Pool* pool, void** out) {
    void* p;
    require_msg(pool != NULL, "pool_try_alloc_zero: pool cannot be NULL");
    p = pool_alloc_zero(pool);
    if (out) *out = p;
    return p != NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
   Index access
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a pointer to the object at index i (bounds-checked)
 *
 * Address is calculated as: arena->buffer + base_mark + i * object_size.
 * Always verifies the returned pointer falls within the reserved region —
 * detects use-after-reset in all build configurations.
 *
 * @param pool  Valid initialized Pool
 * @param i     Index (must be < pool->used)
 * @return Pointer to object at index i, or NULL if out of bounds
 */
static inline void* pool_get(const Pool* pool, usize i) {
    void* base;
    void* p;

    if (!pool || !pool->arena || i >= pool->used) return NULL;

    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    p    = ptr_elem(base, i, pool->object_size);

    require_msg(
        (usize)((u8*)p - pool->arena->buffer) < pool->end_mark,
        "pool_get: address outside reserved region — pool may have been reset"
    );

    return p;
}

static inline const void* pool_get_const(const Pool* pool, usize i) {
    const void* base;
    const void* p;

    if (!pool || !pool->arena || i >= pool->used) return NULL;

    base = ptr_offset_const(pool->arena->buffer, pool->base_mark);
    p    = ptr_elem_const(base, i, pool->object_size);

    require_msg(
        (usize)((const u8*)p - pool->arena->buffer) < pool->end_mark,
        "pool_get_const: address outside reserved region — pool may have been reset"
    );

    return p;
}

/* ════════════════════════════════════════════════════════════════════════════
   Query
   ════════════════════════════════════════════════════════════════════════════ */

static inline usize pool_used(const Pool* pool)            { return pool ? pool->used     : 0; }
static inline usize pool_capacity(const Pool* pool)        { return pool ? pool->capacity : 0; }
static inline usize pool_remaining(const Pool* pool)       { return pool ? pool->capacity - pool->used : 0; }
static inline bool  pool_is_full(const Pool* pool)         { return !pool || pool->used >= pool->capacity; }
static inline bool  pool_is_empty(const Pool* pool)        { return !pool || pool->used == 0; }
static inline usize pool_object_size(const Pool* pool)     { return pool ? pool->object_size : 0; }
static inline usize pool_memory_used(const Pool* pool)     { return pool ? pool->object_size * pool->used     : 0; }
static inline usize pool_memory_reserved(const Pool* pool) { return pool ? pool->object_size * pool->capacity : 0; }

/* ════════════════════════════════════════════════════════════════════════════
   Byte views — slice.h integration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a bytes_t view over all currently allocated objects
 *
 * Covers [base_mark, base_mark + used * object_size).
 * Non-owning — becomes invalid after pool_reset().
 */
static inline bytes_t pool_as_bytes(const Pool* pool) {
    void* base;
    if (!pool || !pool->arena || pool->used == 0) return bytes_empty();
    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    return bytes_from(base, pool->object_size * pool->used);
}

/**
 * @brief Returns a bytes_t view over the entire reserved pool region
 *
 * Covers [base_mark, end_mark). Includes unallocated slots. Non-owning.
 */
static inline bytes_t pool_reserved_bytes(const Pool* pool) {
    void* base;
    if (!pool || !pool->arena) return bytes_empty();
    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    return bytes_from(base, pool->object_size * pool->capacity);
}

/* ════════════════════════════════════════════════════════════════════════════
   Reset
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resets the pool to empty — O(1), no zeroing
 *
 * Rolls the arena back to base_mark. This releases both pool objects and
 * any arena allocations made after pool_init(). If post-init arena
 * allocations must survive, use a separate arena for the pool.
 *
 * @post pool->used == 0
 * @post All pointers returned by pool_alloc() / pool_get() are invalid
 *
 * Lifetime (CANON_LIFETIME_DEBUG): re-stamps the pool's lifetime ID,
 * invalidating every borrow captured before this call. Subsequent
 * reads of those borrows will fire require_msg.
 */
static inline void pool_reset(Pool* pool) {
    void* region;
    usize needed;

    if (!pool || !pool->arena) return;
    arena_reset_to(pool->arena, pool->base_mark);
    pool->used = 0;

    /* Re-reserve the region so subsequent allocs remain within bounds.
     * The pointer value is not used directly — the side effect of advancing
     * the arena offset is what matters. (void) suppresses unused-variable
     * warnings on strict compilers without affecting correctness. */
    needed = pool->object_size * pool->capacity;
    region = arena_alloc(pool->arena, needed);
    ensure_msg(region != NULL,
               "pool_reset: failed to re-reserve pool region after rollback");
    (void)region;

    pool_lifetime_restamp_(pool);
}

/**
 * @brief Resets the pool and securely zeroes all allocated object memory
 *
 * @post pool->used == 0
 * @post All previously allocated object memory is zeroed
 *
 * Lifetime (CANON_LIFETIME_DEBUG): re-stamps the pool's lifetime ID, same
 * as pool_reset(). Borrows captured before this call are invalidated.
 */
static inline void pool_reset_secure(Pool* pool) {
    void* base;

    if (!pool || !pool->arena || pool->used == 0) {
        pool_reset(pool);
        return;
    }
    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    mem_secure_zero(base, pool->object_size * pool->used);
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
