/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_CORE_POOL_H
#define CANON_CORE_POOL_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/checked.h"   /* checked_mul */
#include "core/primitives/ptr.h"
#include "core/arena.h"
#include "core/memory.h"
#include "core/slice.h"

#ifdef CANON_LIFETIME_DEBUG
    #include <stdint.h>                    /* uintptr_t */
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
 * and advances the arena offset by that amount, PLUS any alignment padding the
 * arena inserts before the region. The pool owns this region exclusively for
 * its lifetime.
 *
 * IMPORTANT — base_mark is the post-padding data start:
 * arena_alloc() inserts alignment padding BEFORE the pointer it returns, so the
 * pool's data does not begin at the arena offset that was current before the
 * reservation — it begins at the address arena_alloc() actually returned.
 * base_mark therefore records the offset of that returned region (the post-pad
 * data start), captured from the returned pointer, not from arena_mark() before
 * the call. Capturing the pre-pad mark would place every pool slot `pad` bytes
 * below the reserved region and the last slot would overrun end_mark.
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
 *   - pool_init() opens a fresh lifetime; the ID is derived from a
 *     per-TU counter XOR'd with &pool. See "Internal: lifetime
 *     helpers" below.
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
 *   - Not caught — by design (cross-object boundary): a borrow into an
 *     arena allocation made AFTER pool_init() does not remain valid
 *     memory across pool_reset() — pool_reset rolls the arena back to
 *     base_mark, discarding any post-pool allocations — but the pool's
 *     lt does NOT fire for such a borrow. That borrow captured the
 *     ARENA's lifetime, and pool_reset uses arena_reset_to(), which by
 *     contract does not restamp the arena (borrows allocated before a
 *     mark must survive rollback). The borrow is therefore silently
 *     stale. This is the same Category B cross-object boundary as the
 *     arena-reset case above; the runtime substrate is single-source by
 *     construction. Use a dedicated arena for the pool if post-pool
 *     allocations and their borrows must be tracked across reset.
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
 * Formal verification:
 * ────────────────────────────────────────────────────────────────────────────
 * Every public function carries an ACSL contract suitable for Frama-C WP
 * under the Typed+Cast memory model. pool.h includes memory.h transitively
 * (which pulls in ptr.h, slice.h, checked.h, contract.h), so the full
 * substrate residual surface re-emerges in the pool.h proof run as inherited
 * residuals. pool.h is a SIBLING of arena.h (both include memory.h) rather
 * than a descendant — it provides a second independent observation of the
 * substrate's residual propagation. pool.h-own residuals are expected to
 * cluster at the ptr_offset / ptr_elem call sites (the VERIFY-006 empty
 * `nonnull` behavior cascade) plus the checked_mul overflow goal (VERIFY-002
 * class); pool.h has no per-allocation alignment-pad arithmetic, so arena.h's
 * cat 2b arithmetic-chain residual class does not appear. See the planned
 * VERIFY entry in docs/verification.md for the residual analysis.
 *
 * @sa pool_init(), pool_alloc(), pool_get(), pool_reset()
 * @sa core/primitives/ptr.h      — ptr_offset, ptr_elem for index/offset calculation
 * @sa core/primitives/checked.h  — checked_mul for overflow-safe reservation size
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
    ArenaMark base_mark;    /**< Arena offset of the POST-PADDING data start —
                                 the offset of the pointer arena_alloc() returned,
                                 not the pre-padding offset. See file header.       */
    ArenaMark end_mark;     /**< base_mark + capacity * object_size                 */
#ifdef CANON_LIFETIME_DEBUG
    lifetime_t lt;          /**< [debug] Lifetime token: id + open                  */
#endif
} Pool;

/* ════════════════════════════════════════════════════════════════════════════
   ACSL predicates for verification

   Note: ACSL identifier resolution is separate from C identifier resolution.
   These predicates compose arena_invariant (from arena.h) and add pool-local
   structure. The end_mark - base_mark == capacity * object_size equality is
   the load-bearing conjunct — it is what makes pool_get's region check and
   pool_as_bytes's length provable, and it holds exactly (not >=) only because
   base_mark is captured from arena_alloc's returned pointer (post-pad).
   ════════════════════════════════════════════════════════════════════════════ */

/*@
  predicate pool_invariant(Pool *p) =
      \valid(p) &&
      arena_invariant(p->arena) &&
      p->object_size > 0 &&
      p->used <= p->capacity &&
      p->capacity <= CANON_USIZE_MAX / p->object_size &&
      p->base_mark <= p->end_mark &&
      p->end_mark <= p->arena->capacity &&
      p->end_mark - p->base_mark == p->capacity * p->object_size;
*/

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime helpers (compiled away in release)
   ════════════════════════════════════════════════════════════════════════════
   When CANON_LIFETIME_DEBUG is enabled, a Pool exposes a lifetime_t
   that borrows can capture and validate against. The helpers below
   manage the (id, open) pair across the pool lifecycle:

     pool_lifetime_open_(p)     — called by pool_init; draws a fresh id
     pool_lifetime_restamp_(p)  — called by pool_reset / pool_reset_secure
     pool_lifetime_close_(p)    — reserved; not called by any current API

   In release builds (CANON_LIFETIME_DEBUG undefined) all three helpers
   are no-ops and the Pool struct does not have an lt field.

   History: Phase 1 shipped with bare-address derivation for _open_ and
   XOR-with-`0x9E3779B97F4A7C15ULL` for _restamp_. The XOR-with-constant
   approach cycled after two resets (A -> A^K -> A), silently re-validating
   a borrow captured at the original id. OWN-002 migrated both _open_ and
   _restamp_ to the per-TU counter pattern used by every Phase 3+ container.
   See arena.h's matching helper and docs/design-decisions.md OWN-001 §4 /
   OWN-002.
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    /*@
      assigns \nothing;
      ensures \result != REGION_ID_STATIC;
    */
    static inline region_id_t pool_lifetime_next_id_(void* pp) {
        static region_id_t counter_ = 1;
        region_id_t id = (region_id_t)(counter_++)
                       ^ (region_id_t)(uintptr_t)(pp);
        if (id == REGION_ID_STATIC) { id = (region_id_t)1; }
        return id;
    }

    #define pool_lifetime_open_(p)                                        \
        do {                                                              \
            (p)->lt.id   = pool_lifetime_next_id_((p));                   \
            (p)->lt.open = true;                                          \
        } while (0)
    #define pool_lifetime_restamp_(p)                                     \
        do {                                                              \
            (p)->lt.id = pool_lifetime_next_id_((p));                     \
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
 * Immediately advances the arena offset by capacity * aligned(object_size),
 * plus any alignment padding the arena inserts before the region. Other arena
 * allocations made after this call are safe — they land beyond the reserved
 * region and do not interfere with pool address calculations.
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
 * @post On success: pool is ready; arena offset advanced past the reservation
 * @post On failure: pool is untouched; arena offset unchanged
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token on success.
 *
 * ACSL note: predicted contract, pending Frama-C WP verification. The
 * overflow goal on checked_mul is the inherited VERIFY-002 residual class.
 */
/*@
  requires \valid(pool) && arena_invariant(arena);
  requires object_size > 0;
  requires max_objects > 0;
  assigns  *pool, *arena;
  ensures  \result == \true ==> pool_invariant(pool);
  ensures  \result == \true ==> pool->used == 0;
  ensures  \result == \true ==> pool->capacity == max_objects;
  ensures  \result == \false ==> arena->offset == \old(arena->offset);
*/
static inline bool pool_init(
    Pool* pool, Arena* arena, usize object_size, usize max_objects)
{
    usize  aligned_size;
    usize  needed;
    void*  region;

    require_msg(pool        != NULL, "pool_init: pool cannot be NULL");
    require_msg(arena       != NULL, "pool_init: arena cannot be NULL");
    require_msg(object_size  > 0u,    "pool_init: object_size must be > 0");
    require_msg(max_objects  > 0u,    "pool_init: max_objects must be > 0");

    aligned_size = mem_align(object_size);
    if (aligned_size == CANON_USIZE_MAX) {
        return false;  /* object_size alignment overflowed usize */
    }

    /* Overflow-safe reservation size via the verified primitive (checked_mul,
     * VERIFY-002 residual class) rather than a hand-rolled division guard.
     * Speaks the library's arithmetic vocabulary and inherits the documented
     * overflow residual instead of minting a new pool-specific one. */
    if (!checked_mul(aligned_size, max_objects, &needed)) {
        return false;
    }
    if (needed > arena_remaining(arena)) {
        return false;
    }

    pool->arena       = arena;
    pool->object_size = aligned_size;
    pool->capacity    = max_objects;
    pool->used        = 0;

    /* Reserve the entire region upfront. arena_alloc inserts alignment
     * padding BEFORE the returned pointer, so base_mark is captured from
     * the RETURNED region (the post-pad data start), not from arena_mark
     * before the call. This keeps base_mark, the per-slot ptr_elem
     * calculation in pool_alloc, and end_mark mutually consistent. */
    region = arena_alloc(arena, needed);
    if (!region) { return false; }

    pool->base_mark = (ArenaMark)((u8*)region - arena->buffer);
    pool->end_mark  = arena_mark(arena);
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
/*@
  requires pool == \null || pool_invariant(pool);
  assigns  pool->used;
  behavior null_pool:
    assumes pool == \null;
    assigns \nothing;
    ensures \result == \null;
  behavior full:
    assumes pool != \null && pool->used >= pool->capacity;
    assigns \nothing;
    ensures \result == \null;
  behavior alloc:
    assumes pool != \null && pool->used < pool->capacity;
    ensures pool->used == \old(pool->used) + 1;
    ensures pool_invariant(pool);
  complete behaviors;
  disjoint behaviors;
*/
static inline void* pool_alloc(Pool* pool) {
    void* base;
    void* slot;

    require_msg(pool != NULL, "pool_alloc: pool cannot be NULL");
    if (pool->used >= pool->capacity) { return NULL; }

    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    slot = ptr_elem(base, pool->used, pool->object_size);
    pool->used++;
    return slot;
}

/** @brief Allocates and zeroes the next object slot */
/*@
  requires pool == \null || pool_invariant(pool);
  assigns  pool->used;
*/
static inline void* pool_alloc_zero(Pool* pool) {
    void* p;
    p = pool_alloc(pool);
    if (p) mem_zero(p, pool->object_size);
    return p;
}

/** @brief Allocates and writes result to *out; returns false if full */
/*@
  requires pool == \null || pool_invariant(pool);
  requires out == \null || \valid(out);
  assigns  pool->used, *out;
*/
static inline bool pool_try_alloc(Pool* pool, void** out) {
    void* p;
    require_msg(pool != NULL, "pool_try_alloc: pool cannot be NULL");
    p = pool_alloc(pool);
    if (out) *out = p;
    return p != NULL;
}

/** @brief Allocates, zeroes, and writes result to *out; returns false if full */
/*@
  requires pool == \null || pool_invariant(pool);
  requires out == \null || \valid(out);
  assigns  pool->used, *out;
*/
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
/*@
  requires pool == \null || pool_invariant(pool);
  assigns  \nothing;
  behavior oob:
    assumes pool == \null || pool->arena == \null || i >= pool->used;
    ensures \result == \null;
  behavior in_bounds:
    assumes pool != \null && pool->arena != \null && i < pool->used;
    ensures \result != \null;
  complete behaviors;
  disjoint behaviors;
*/
static inline void* pool_get(const Pool* pool, usize i) {
    void* base;
    void* p;

    if (!pool || !pool->arena || (i >= pool->used)) { return NULL; }

    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    p    = ptr_elem(base, i, pool->object_size);

    require_msg(
        (usize)((u8*)p - pool->arena->buffer) < pool->end_mark,
        "pool_get: address outside reserved region — pool may have been reset"
    );

    return p;
}

/*@
  requires pool == \null || pool_invariant(pool);
  assigns  \nothing;
  behavior oob:
    assumes pool == \null || pool->arena == \null || i >= pool->used;
    ensures \result == \null;
  behavior in_bounds:
    assumes pool != \null && pool->arena != \null && i < pool->used;
    ensures \result != \null;
  complete behaviors;
  disjoint behaviors;
*/
static inline const void* pool_get_const(const Pool* pool, usize i) {
    const void* base;
    const void* p;

    if (!pool || !pool->arena || (i >= pool->used)) { return NULL; }

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

/*@ requires pool == \null || \valid(pool); assigns \nothing;
    behavior null: assumes pool == \null; ensures \result == 0;
    behavior nn:   assumes \valid(pool);   ensures \result == pool->used;
    complete behaviors; disjoint behaviors; */
static inline usize pool_used(const Pool* pool)            { return pool ? pool->used     : 0u; }

/*@ requires pool == \null || \valid(pool); assigns \nothing;
    behavior null: assumes pool == \null; ensures \result == 0;
    behavior nn:   assumes \valid(pool);   ensures \result == pool->capacity;
    complete behaviors; disjoint behaviors; */
static inline usize pool_capacity(const Pool* pool)        { return pool ? pool->capacity : 0u; }

/*@ requires pool == \null || pool_invariant(pool); assigns \nothing;
    behavior null: assumes pool == \null; ensures \result == 0;
    behavior nn:   assumes \valid(pool) && pool->used <= pool->capacity;
                   ensures \result == pool->capacity - pool->used;
    complete behaviors; disjoint behaviors; */
static inline usize pool_remaining(const Pool* pool)       { return pool ? (pool->capacity - pool->used) : 0u; }

/*@ requires pool == \null || \valid(pool); assigns \nothing;
    behavior null: assumes pool == \null; ensures \result == \true;
    behavior nn:   assumes \valid(pool);   ensures \result == (pool->used >= pool->capacity);
    complete behaviors; disjoint behaviors; */
static inline bool  pool_is_full(const Pool* pool)         { return !pool || (pool->used >= pool->capacity); }

/*@ requires pool == \null || \valid(pool); assigns \nothing;
    behavior null: assumes pool == \null; ensures \result == \true;
    behavior nn:   assumes \valid(pool);   ensures \result == (pool->used == 0);
    complete behaviors; disjoint behaviors; */
static inline bool  pool_is_empty(const Pool* pool)        { return !pool || (pool->used == 0u); }

/*@ requires pool == \null || \valid(pool); assigns \nothing;
    behavior null: assumes pool == \null; ensures \result == 0;
    behavior nn:   assumes \valid(pool);   ensures \result == pool->object_size;
    complete behaviors; disjoint behaviors; */
static inline usize pool_object_size(const Pool* pool)     { return pool ? pool->object_size : 0u; }

/*@ requires pool == \null || pool_invariant(pool); assigns \nothing; */
static inline usize pool_memory_used(const Pool* pool)     { return pool ? (pool->object_size * pool->used)     : 0u; }

/*@ requires pool == \null || pool_invariant(pool); assigns \nothing; */
static inline usize pool_memory_reserved(const Pool* pool) { return pool ? (pool->object_size * pool->capacity) : 0u; }

/* ════════════════════════════════════════════════════════════════════════════
   Byte views — slice.h integration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a bytes_t view over all currently allocated objects
 *
 * Covers [base_mark, base_mark + used * object_size).
 * Non-owning — becomes invalid after pool_reset().
 */
/*@
  requires pool == \null || pool_invariant(pool);
  assigns  \nothing;
  ensures  \result.len == 0 ==> \result.ptr == \null;
*/
static inline bytes_t pool_as_bytes(const Pool* pool) {
    void* base;
    if (!pool || !pool->arena || (pool->used == 0u)) { return bytes_empty(); }
    base = ptr_offset(pool->arena->buffer, pool->base_mark);
    return bytes_from(base, pool->object_size * pool->used);
}

/**
 * @brief Returns a bytes_t view over the entire reserved pool region
 *
 * Covers [base_mark, end_mark). Includes unallocated slots. Non-owning.
 */
/*@
  requires pool == \null || pool_invariant(pool);
  assigns  \nothing;
*/
static inline bytes_t pool_reserved_bytes(const Pool* pool) {
    void* base;
    if (!pool || !pool->arena) { return bytes_empty(); }
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
 * Lifetime (CANON_LIFETIME_DEBUG): re-stamps the pool's lifetime ID with a
 * fresh draw from the per-TU counter, invalidating every borrow captured
 * before this call. Multiple resets produce distinct ids — no cycling.
 */
/*@
  requires pool == \null || pool_invariant(pool);
  assigns  pool->used, *(pool->arena);
  behavior null:
    assumes pool == \null || pool->arena == \null;
    assigns \nothing;
  behavior reset:
    assumes pool != \null && pool->arena != \null;
    ensures pool->used == 0;
    ensures pool_invariant(pool);
  complete behaviors;
  disjoint behaviors;
*/
static inline void pool_reset(Pool* pool) {
    void* region;
    usize needed;

    if (!pool || !pool->arena) { return; }
    arena_reset_to(pool->arena, pool->base_mark);
    pool->used = 0;

    /* Re-reserve the region so subsequent allocs remain within bounds.
     *
     * base_mark is alignment-stable across reset: it was produced by a
     * previous arena_alloc and is therefore already CANON_DEFAULT_ALIGN
     * aligned, so arena_alloc inserts zero padding here and the region
     * lands at exactly base_mark again. The pointer value is not used
     * directly — the side effect of advancing the arena offset back to
     * end_mark is what matters. (void) suppresses unused-variable
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
/*@
  requires pool == \null || pool_invariant(pool);
  assigns  pool->used, *(pool->arena),
           (pool->arena->buffer)[pool->base_mark .. pool->end_mark - 1];
  behavior null_or_empty:
    assumes pool == \null || pool->arena == \null || pool->used == 0;
  behavior secure:
    assumes pool != \null && pool->arena != \null && pool->used > 0;
    ensures pool->used == 0;
    ensures pool_invariant(pool);
  complete behaviors;
  disjoint behaviors;
*/
static inline void pool_reset_secure(Pool* pool) {
    void* base;

    if (!pool || !pool->arena || (pool->used == 0u)) {
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
