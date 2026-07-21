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

#ifndef CANON_CORE_ARENA_H
#define CANON_CORE_ARENA_H

#include <stdbool.h>             /* bool, true, false (C99) */

#include "core/primitives/types.h"    /* u8, usize */
#include "core/primitives/limits.h"   /* CANON_ARENA_MAX_SIZE, CANON_USIZE_MAX, CANON_DEFAULT_ALIGN */
#include "core/primitives/contract.h" /* require_msg */
#include "core/primitives/ptr.h"      /* ptr_offset, ptr_align_up, ptr_span, is_power_of_two */
#include "core/memory.h"              /* mem_zero, mem_secure_zero */
#include "core/slice.h"               /* bytes_t, bytes_from, bytes_empty */

#ifdef CANON_LIFETIME_DEBUG
    #include <stdint.h>                    /* uintptr_t */
    #include "core/primitives/lifetime.h"  /* region_id_t, lifetime_t */
#endif

/**
 * @file arena.h
 * @brief Linear bump allocator with explicit lifetime control
 *
 * Allocates sequentially from a fixed, caller-owned buffer.
 * No individual deallocation — memory is released only via full reset
 * or rollback to a saved mark.
 *
 * Properties:
 *   - O(1) alloc (pointer bump + alignment padding only)
 *   - Zero hidden allocations or metadata overhead
 *   - Caller owns and controls the backing buffer
 *   - Two reset modes: fast (offset = 0) and secure (memset 0 over full used+pad range)
 *   - Checkpoint/rollback via ArenaMark
 *   - NOT thread-safe — synchronize externally if needed
 *   - Requires C99 or later; no platform-specific code
 *
 * Debug instrumentation (define CANON_ARENA_DEBUG before including):
 *   - alloc_count: total successful arena_alloc() calls since last reset
 *   - peak: high-watermark of bytes used since last reset
 *   Zero cost in release builds — struct layout is identical without the flag.
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 *   - Embeds a lifetime_t lt field (id + open) on the Arena.
 *   - arena_init() opens a fresh lifetime; the ID is derived from a
 *     per-TU counter XOR'd with &arena.
 *   - arena_reset() and arena_reset_secure() RE-STAMP the lifetime ID,
 *     invalidating every borrow that captured the previous ID.
 *   - arena_reset_to(mark) does NOT touch the lifetime — borrows
 *     allocated before the mark must remain valid through rollback.
 *   Zero cost in release builds.
 *
 * Formal verification: every public function carries an ACSL contract
 * suitable for Frama-C WP under the Typed+Cast memory model. See
 * docs/verification.md (VERIFY-009) for the residual category analysis
 * and docs/deviations.md for the manually-discharged goals (including
 * the OWN-002 no-cycle property, which is runtime-validated rather than
 * formally proved — see test/core/arena_test.c).
 *
 * @sa arena_alloc(), arena_alloc_aligned(), arena_mark(), arena_reset_to()
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
 * @sa core/region.h              — Region type and lifetime_assert_valid()
 * @sa semantics/borrow.h         — borrow types that capture and validate IDs
 */

/* ============================================================================
   Arena struct and mark type
   ============================================================================ */

typedef struct Arena {
    u8*   buffer;          /**< Start of caller-owned memory block          */
    usize capacity;        /**< Total usable bytes in buffer                */
    usize offset;          /**< Bump pointer: bytes consumed (used+padding) */
    usize padding_accum;   /**< Cumulative alignment padding since last reset */
#ifdef CANON_ARENA_DEBUG
    usize alloc_count;     /**< [debug] Successful alloc calls since reset  */
    usize peak;            /**< [debug] Max offset ever reached since reset  */
#endif
#ifdef CANON_LIFETIME_DEBUG
    lifetime_t lt;         /**< [debug] Lifetime token: id + open           */
#endif
} Arena;

typedef usize ArenaMark;

/* ============================================================================
   ACSL predicates for verification

   Note: ACSL identifier resolution is separate from C identifier resolution.
   C functions in ptr.h (like is_power_of_two) cannot be called directly from
   inside ACSL annotation blocks. The is_power_of_two_logic predicate below
   mirrors the semantics of the C function and is what contracts use.
   ============================================================================ */

/*@
  predicate is_power_of_two_logic(integer n) =
      n > 0 && (n & (n - 1)) == 0;

  predicate arena_invariant(Arena *a) =
      \valid(a) &&
      a->capacity > 0 &&
      a->capacity <= CANON_ARENA_MAX_SIZE &&
      a->offset <= a->capacity &&
      \valid(a->buffer + (0 .. a->capacity - 1));

  predicate arena_can_fit{L}(Arena *a, integer size, integer alignment) =
      \let cur = a->offset;
      \let pad = (alignment - (cur % alignment)) % alignment;
      cur <= CANON_USIZE_MAX - pad &&
      cur + pad <= CANON_USIZE_MAX - size &&
      cur + pad + size <= a->capacity;

  predicate mark_in_arena(Arena *a, ArenaMark mark) =
      arena_invariant(a) &&
      mark <= a->offset;
*/

/* ============================================================================
   Debug stats (only available with CANON_ARENA_DEBUG)
   ============================================================================ */

#ifdef CANON_ARENA_DEBUG
typedef struct {
    usize used;
    usize remaining;
    usize capacity;
    usize peak;
    usize alloc_count;
} ArenaStats;

/*@
  requires arena == \null || \valid(arena);
  assigns \nothing;
  behavior null_arena:
    assumes arena == \null;
    ensures \result.used == 0;
    ensures \result.remaining == 0;
    ensures \result.capacity == 0;
    ensures \result.peak == 0;
    ensures \result.alloc_count == 0;
  behavior non_null:
    assumes \valid(arena);
    ensures \result.used == arena->offset;
    ensures \result.remaining == arena->capacity - arena->offset;
    ensures \result.capacity == arena->capacity;
    ensures \result.peak == arena->peak;
    ensures \result.alloc_count == arena->alloc_count;
  complete behaviors;
  disjoint behaviors;
*/
static inline ArenaStats arena_stats(const Arena* arena) {
    if (!arena) {
        ArenaStats s = {0};
        return s;
    }
    ArenaStats s;
    s.used        = arena->offset;
    s.remaining   = arena->capacity - arena->offset;
    s.capacity    = arena->capacity;
    s.peak        = arena->peak;
    s.alloc_count = arena->alloc_count;
    return s;
}
#endif /* CANON_ARENA_DEBUG */

/* ============================================================================
   Internal: debug update helpers (compiled away in release)
   ============================================================================ */

#ifdef CANON_ARENA_DEBUG
    #define arena_debug_update_(a) \
        do { \
            (a)->alloc_count += 1u; \
            if ((a)->offset > (a)->peak) { (a)->peak = (a)->offset; } \
        } while (0)
    #define arena_debug_reset_(a) \
        do { (a)->alloc_count = 0; (a)->peak = 0; } while (0)
#else
    #define arena_debug_update_(a) ((void)0)
    #define arena_debug_reset_(a)  ((void)0)
#endif

/* ============================================================================
   Internal: lifetime helpers (compiled away in release)
   ============================================================================ */

#ifdef CANON_LIFETIME_DEBUG
    /*@
      predicate arena_lifetime_open(Arena *a) =
          a->lt.open == \true;

      predicate arena_lifetime_valid(Arena *a) =
          arena_lifetime_open(a) &&
          a->lt.id != REGION_ID_STATIC;
    */

    /*@
      assigns \nothing;
      ensures \result != REGION_ID_STATIC;
    */
    static inline region_id_t arena_lifetime_next_id_(void* ap) {
        static region_id_t counter_ = 1;
        region_id_t id = (region_id_t)(counter_++)
                       ^ (region_id_t)(uintptr_t)(ap);
        if (id == REGION_ID_STATIC) { id = (region_id_t)1; }
        return id;
    }

    #define arena_lifetime_open_(a)                                       \
        do {                                                              \
            (a)->lt.id   = arena_lifetime_next_id_((a));                  \
            (a)->lt.open = true;                                          \
        } while (0)
    #define arena_lifetime_restamp_(a)                                    \
        do {                                                              \
            (a)->lt.id = arena_lifetime_next_id_((a));                    \
        } while (0)
    #define arena_lifetime_close_(a)                                      \
        do { (a)->lt.open = false; } while (0)
#else
    #define arena_lifetime_open_(a)     ((void)0)
    #define arena_lifetime_restamp_(a)  ((void)0)
    #define arena_lifetime_close_(a)    ((void)0)
#endif

/* ============================================================================
   Initialization & reset
   ============================================================================ */

/*@
  requires \valid(arena);
  requires \valid((u8*)buffer + (0 .. capacity - 1));
  requires capacity > 0;
  requires capacity <= CANON_ARENA_MAX_SIZE;
  assigns *arena;
  ensures arena_invariant(arena);
  ensures arena->buffer == (u8*)buffer;
  ensures arena->capacity == capacity;
  ensures arena->offset == 0;
  ensures arena->padding_accum == 0;
*/
static inline void arena_init(Arena* arena, void* buffer, usize capacity) {
    require_msg(arena    != NULL, "arena_init: arena cannot be NULL");
    require_msg(buffer   != NULL, "arena_init: buffer cannot be NULL");
    require_msg(capacity  > 0u,    "arena_init: capacity must be > 0");
    require_msg(capacity <= CANON_ARENA_MAX_SIZE,
                "arena_init: capacity exceeds CANON_ARENA_MAX_SIZE");

    arena->buffer        = (u8*)buffer;
    arena->capacity      = capacity;
    arena->offset        = 0;
    arena->padding_accum = 0;
    arena_debug_reset_(arena);
    arena_lifetime_open_(arena);
}

/*@
  requires arena == \null || arena_invariant(arena);
  assigns *arena;
  behavior null_arena:
    assumes arena == \null;
    assigns \nothing;
  behavior non_null:
    assumes \valid(arena);
    ensures arena->offset == 0;
    ensures arena->padding_accum == 0;
    ensures arena->buffer == \old(arena->buffer);
    ensures arena->capacity == \old(arena->capacity);
    ensures arena_invariant(arena);
  complete behaviors;
  disjoint behaviors;
*/
static inline void arena_reset(Arena* arena) {
    if (!arena) { return; }
    arena->offset        = 0;
    arena->padding_accum = 0;
    arena_debug_reset_(arena);
    arena_lifetime_restamp_(arena);
}

/*@
  requires arena == \null || arena_invariant(arena);
  assigns *arena;
  assigns arena->buffer[0 .. arena->offset - 1];
  behavior null_or_empty:
    assumes arena == \null || arena->offset == 0;
    assigns \nothing;
  behavior non_empty:
    assumes \valid(arena) && arena->offset > 0;
    ensures arena->offset == 0;
    ensures arena->padding_accum == 0;
    ensures arena->buffer == \old(arena->buffer);
    ensures arena->capacity == \old(arena->capacity);
    ensures arena_invariant(arena);
  complete behaviors;
  disjoint behaviors;
*/
static inline void arena_reset_secure(Arena* arena) {
    if (!arena || (arena->offset == 0u)) { return; }
    mem_secure_zero(arena->buffer, arena->offset);
    arena->offset        = 0;
    arena->padding_accum = 0;
    arena_debug_reset_(arena);
    arena_lifetime_restamp_(arena);
}

/* ============================================================================
   Allocation
   ============================================================================ */

/*@
  requires arena_invariant(arena);
  assigns *arena;
  behavior size_zero:
    assumes size == 0;
    ensures \result == \null;
    ensures arena->offset == \old(arena->offset);
    ensures arena_invariant(arena);
  behavior fits:
    assumes size > 0;
    assumes arena_can_fit(arena, size, CANON_DEFAULT_ALIGN);
    ensures \result != \null;
    ensures \valid((u8*)\result + (0 .. size - 1));
    ensures arena->offset >= \old(arena->offset) + size;
    ensures arena->offset <= arena->capacity;
    ensures arena_invariant(arena);
  behavior does_not_fit:
    assumes size > 0;
    assumes !arena_can_fit(arena, size, CANON_DEFAULT_ALIGN);
    ensures \result == \null;
    ensures arena->offset == \old(arena->offset);
    ensures arena_invariant(arena);
  complete behaviors;
  disjoint behaviors;
*/
static inline void* arena_alloc(Arena* arena, usize size) {
    void* current;
    void* aligned_ptr;
    void* result;
    usize pad;

    require_msg(arena != NULL, "arena_alloc: arena cannot be NULL");
    if (size == 0u) { return NULL; }

    current     = ptr_offset(arena->buffer, arena->offset);
    aligned_ptr = ptr_align_up(current, CANON_DEFAULT_ALIGN);
    pad         = ptr_span(aligned_ptr, current);

    if ((arena->offset > (CANON_USIZE_MAX - pad)) ||
        ((arena->offset + pad) > (CANON_USIZE_MAX - size)) ||
        ((arena->offset + pad + size) > arena->capacity)) {
        return NULL;
    }

    arena->offset        += pad;
    arena->padding_accum += pad;
    result                = ptr_offset(arena->buffer, arena->offset);
    arena->offset        += size;
    arena_debug_update_(arena);
    return result;
}

/*@
  requires arena_invariant(arena);
  requires is_power_of_two_logic(alignment);
  assigns *arena;
  behavior size_zero:
    assumes size == 0;
    ensures \result == \null;
    ensures arena->offset == \old(arena->offset);
    ensures arena_invariant(arena);
  behavior fits:
    assumes size > 0;
    assumes arena_can_fit(arena, size, alignment);
    ensures \result != \null;
    ensures \valid((u8*)\result + (0 .. size - 1));
    ensures arena->offset >= \old(arena->offset) + size;
    ensures arena->offset <= arena->capacity;
    ensures arena_invariant(arena);
  behavior does_not_fit:
    assumes size > 0;
    assumes !arena_can_fit(arena, size, alignment);
    ensures \result == \null;
    ensures arena->offset == \old(arena->offset);
    ensures arena_invariant(arena);
  complete behaviors;
  disjoint behaviors;
*/
static inline void* arena_alloc_aligned(Arena* arena, usize size, usize alignment) {
    void* current;
    void* aligned_ptr;
    void* result;
    usize pad;

    require_msg(arena != NULL, "arena_alloc_aligned: arena cannot be NULL");
    require_msg(is_power_of_two(alignment),
                "arena_alloc_aligned: alignment must be a power of 2");
    if (size == 0u) { return NULL; }

    current     = ptr_offset(arena->buffer, arena->offset);
    aligned_ptr = ptr_align_up(current, alignment);
    pad         = ptr_span(aligned_ptr, current);

    if ((arena->offset > (CANON_USIZE_MAX - pad)) ||
        ((arena->offset + pad) > (CANON_USIZE_MAX - size)) ||
        ((arena->offset + pad + size) > arena->capacity)) {
        return NULL;
    }

    arena->offset        += pad;
    arena->padding_accum += pad;
    result                = ptr_offset(arena->buffer, arena->offset);
    arena->offset        += size;
    arena_debug_update_(arena);
    return result;
}

/* ============================================================================
   Zero-initializing variants
   ============================================================================ */

/*@
  requires arena_invariant(arena);
  assigns *arena;
  ensures arena_invariant(arena);
  ensures \result == \null || \valid((u8*)\result + (0 .. size - 1));
  ensures \result != \null ==>
      \forall integer i; 0 <= i < size ==> ((u8*)\result)[i] == 0;
*/
static inline void* arena_alloc_zero(Arena* arena, usize size) {
    void* p = arena_alloc(arena, size);
    if (p != NULL) { mem_zero(p, size); }
    return p;
}

/*@
  requires arena_invariant(arena);
  requires is_power_of_two_logic(alignment);
  assigns *arena;
  ensures arena_invariant(arena);
  ensures \result == \null || \valid((u8*)\result + (0 .. size - 1));
  ensures \result != \null ==>
      \forall integer i; 0 <= i < size ==> ((u8*)\result)[i] == 0;
*/
static inline void* arena_alloc_aligned_zero(Arena* arena, usize size, usize alignment) {
    void* p = arena_alloc_aligned(arena, size, alignment);
    if (p != NULL) { mem_zero(p, size); }
    return p;
}

/* ============================================================================
   Try-alloc variants (bool return, output via pointer)

   Contract/code alignment note:
     The ACSL ensures clause states
         \result <==> (out != \null && *out != \null);
     This means \result must be FALSE when out == NULL, regardless of
     whether the internal allocation succeeded. The return expression
     `out != NULL && p != NULL` matches this contract exactly. Returning
     just `p != NULL` would violate the contract when caller passes
     NULL out and allocation succeeds (code says true, contract says
     false). The current form is the correct one — see
     test/core/arena_test.c::test_try_alloc_null_out for the test that
     pins this behaviour.
   ============================================================================ */

/*@
  requires arena_invariant(arena);
  requires out == \null || \valid(out);
  assigns *arena;
  behavior null_out:
    assumes out == \null;
    assigns *arena;
    ensures arena_invariant(arena);
    ensures \result == \false;
  behavior non_null_out:
    assumes \valid(out);
    assigns *arena, *out;
    ensures arena_invariant(arena);
    ensures *out == \null || \valid((u8*)*out + (0 .. size - 1));
    ensures \result <==> (*out != \null);
  complete behaviors;
  disjoint behaviors;
*/
static inline bool arena_try_alloc(Arena* arena, usize size, void** out) {
    void* p = arena_alloc(arena, size);
    if (out != NULL) { *out = p; }
    return (out != NULL) && (p != NULL);
}

/*@
  requires arena_invariant(arena);
  requires is_power_of_two_logic(alignment);
  requires out == \null || \valid(out);
  assigns *arena;
  behavior null_out:
    assumes out == \null;
    assigns *arena;
    ensures arena_invariant(arena);
    ensures \result == \false;
  behavior non_null_out:
    assumes \valid(out);
    assigns *arena, *out;
    ensures arena_invariant(arena);
    ensures *out == \null || \valid((u8*)*out + (0 .. size - 1));
    ensures \result <==> (*out != \null);
  complete behaviors;
  disjoint behaviors;
*/
static inline bool arena_try_alloc_aligned(Arena* arena, usize size, usize alignment, void** out) {
    void* p = arena_alloc_aligned(arena, size, alignment);
    if (out != NULL) { *out = p; }
    return (out != NULL) && (p != NULL);
}

/* ============================================================================
   Query
   ============================================================================ */

/*@
  requires arena == \null || \valid(arena);
  assigns \nothing;
  behavior null_arena:
    assumes arena == \null;
    ensures \result == 0;
  behavior non_null:
    assumes \valid(arena);
    ensures \result == arena->capacity;
  complete behaviors;
  disjoint behaviors;
*/
static inline usize arena_capacity(const Arena* arena) {
    return arena ? arena->capacity : 0u;
}

/*@
  requires arena == \null || arena_invariant(arena);
  assigns \nothing;
  behavior null_arena:
    assumes arena == \null;
    ensures \result == 0;
  behavior non_null:
    assumes \valid(arena);
    assumes arena->offset <= arena->capacity;
    ensures \result == arena->capacity - arena->offset;
  complete behaviors;
  disjoint behaviors;
*/
static inline usize arena_remaining(const Arena* arena) {
    return arena ? (arena->capacity - arena->offset) : 0u;
}

/*@
  requires arena == \null || \valid(arena);
  assigns \nothing;
  behavior null_arena:
    assumes arena == \null;
    ensures \result == 0;
  behavior non_null:
    assumes \valid(arena);
    ensures \result == arena->offset;
  complete behaviors;
  disjoint behaviors;
*/
static inline usize arena_used(const Arena* arena) {
    return arena ? arena->offset : 0u;
}

/*@
  requires arena == \null || \valid(arena);
  assigns \nothing;
  behavior null_arena:
    assumes arena == \null;
    ensures \result == \true;
  behavior non_null:
    assumes \valid(arena);
    ensures \result == (arena->offset == 0);
  complete behaviors;
  disjoint behaviors;
*/
static inline bool arena_is_empty(const Arena* arena) {
    return !arena || (arena->offset == 0u);
}

/*@
  requires arena == \null || \valid(arena);
  assigns \nothing;
  behavior null_arena:
    assumes arena == \null;
    ensures \result == \true;
  behavior non_null:
    assumes \valid(arena);
    ensures \result == (arena->offset >= arena->capacity);
  complete behaviors;
  disjoint behaviors;
*/
static inline bool arena_is_full(const Arena* arena) {
    return !arena || (arena->offset >= arena->capacity);
}

/* ============================================================================
   Checkpoint / rollback
   ============================================================================ */

/*@
  requires arena == \null || \valid(arena);
  assigns \nothing;
  behavior null_arena:
    assumes arena == \null;
    ensures \result == 0;
  behavior non_null:
    assumes \valid(arena);
    ensures \result == arena->offset;
  complete behaviors;
  disjoint behaviors;
*/
static inline ArenaMark arena_mark(const Arena* arena) {
    return arena ? arena->offset : 0u;
}

/*@
  requires arena == \null || arena_invariant(arena);
  requires arena != \null ==> mark <= arena->offset;
  assigns arena->offset;
  behavior null_arena:
    assumes arena == \null;
    assigns \nothing;
  behavior non_null:
    assumes \valid(arena);
    ensures arena->offset == mark;
    ensures arena->buffer == \old(arena->buffer);
    ensures arena->capacity == \old(arena->capacity);
    ensures arena_invariant(arena);
  complete behaviors;
  disjoint behaviors;
*/
static inline void arena_reset_to(Arena* arena, ArenaMark mark) {
    if (!arena) { return; }
    require_msg(mark <= arena->offset,
                "arena_reset_to: mark is ahead of current offset");
    arena->offset = mark;
}

/* ============================================================================
   Byte views — slice.h integration
   ============================================================================ */

/*@
  requires arena_invariant(arena);
  assigns \nothing;
  ensures \result.len == arena->offset;
  ensures \result.len == 0 || \result.ptr == arena->buffer;
*/
static inline bytes_t arena_as_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_as_bytes: arena cannot be NULL");
    return bytes_from(arena->buffer, arena->offset);
}

/*@
  requires arena_invariant(arena);
  assigns \nothing;
  ensures \result.ptr == arena->buffer;
  ensures \result.len == arena->capacity;
*/
static inline bytes_t arena_buffer_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_buffer_bytes: arena cannot be NULL");
    return bytes_from(arena->buffer, arena->capacity);
}

/*@
  requires arena_invariant(arena);
  assigns \nothing;
  ensures arena->offset >= arena->capacity ==>
      (\result.ptr == \null && \result.len == 0);
  ensures arena->offset < arena->capacity ==>
      \result.len == arena->capacity - arena->offset;
*/
static inline bytes_t arena_free_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_free_bytes: arena cannot be NULL");
    if (arena->offset >= arena->capacity) { return bytes_empty(); }
    return bytes_from(ptr_offset(arena->buffer, arena->offset),
                      arena->capacity - arena->offset);
}

/* ============================================================================
   Typed allocation macros

   DOCUMENTED, NOT WP-VERIFIED — the C preprocessor strips ACSL annotations
   from macro bodies before expansion, so these macros follow the same
   disposition as memory.h's mem_alloc_type family (see VERIFY-008 and the
   DEFINE_SLICE precedent in VERIFY-007): documented here, validated by unit
   tests in test/core/arena_test.c, expanded and exercised at call sites.

   Overflow guard on the array variants
   ─────────────────────────────────────────────────────────────────────────
   arena_alloc_array and arena_alloc_array_zero guard the
   `sizeof(Type) * (count)` multiply and return NULL on overflow, matching
   the contract of memory.h's mem_alloc_array (which routes through
   checked_mul). The guard is the division form:

       (count) <= CANON_USIZE_MAX / sizeof(Type)

   which is exact: sizeof(Type) is a compile-time constant and is never
   zero in conforming C, so the division is safe and the bound is tight.
   A `count` of 0 passes the guard and reaches arena_alloc(arena, 0),
   which returns NULL via its size_zero behavior — unchanged from the
   unguarded form.

   On overflow the arena is untouched: the guard short-circuits before
   arena_alloc is called, so offset, padding_accum, and debug counters
   are all preserved. NULL from these macros therefore means either
   "does not fit", "size was zero", or "count * sizeof(Type) overflows
   usize" — in every case the arena state is unchanged.

   WARNING — double evaluation
   ─────────────────────────────────────────────────────────────────────────
   `count` is evaluated TWICE in the array variants (once in the guard,
   once in the multiply). Do not pass an expression with side effects
   (`i++`, a function call). Bind it to a variable first. This is the
   same rule as checked_min / checked_max / checked_clamp in checked.h.

   `arena` and `Type` are evaluated once and are not affected.
   ============================================================================ */

/**
 * @brief Allocate one Type-sized object from the arena
 *
 * Equivalent to (Type*)arena_alloc(arena, sizeof(Type)).
 * Returns NULL if the object does not fit.
 */
#define arena_alloc_type(arena, Type) \
    ((Type*)arena_alloc((arena), sizeof(Type)))

/**
 * @brief Allocate an array of `count` Type objects from the arena
 *
 * The sizeof(Type) * count multiply is overflow-guarded: on overflow the
 * macro returns NULL without touching the arena (see block comment above).
 *
 * @warning `count` is evaluated twice — no side-effecting expressions.
 */
#define arena_alloc_array(arena, Type, count)                    \
    ((Type*)((count) <= CANON_USIZE_MAX / sizeof(Type)           \
        ? arena_alloc((arena), sizeof(Type) * (count))           \
        : NULL))

/**
 * @brief Allocate one zero-initialized Type-sized object from the arena
 *
 * Equivalent to (Type*)arena_alloc_zero(arena, sizeof(Type)).
 * Returns NULL if the object does not fit.
 */
#define arena_alloc_type_zero(arena, Type) \
    ((Type*)arena_alloc_zero((arena), sizeof(Type)))

/**
 * @brief Allocate a zero-initialized array of `count` Type objects
 *
 * The sizeof(Type) * count multiply is overflow-guarded: on overflow the
 * macro returns NULL without touching the arena (see block comment above).
 *
 * @warning `count` is evaluated twice — no side-effecting expressions.
 */
#define arena_alloc_array_zero(arena, Type, count)               \
    ((Type*)((count) <= CANON_USIZE_MAX / sizeof(Type)           \
        ? arena_alloc_zero((arena), sizeof(Type) * (count))      \
        : NULL))

#endif /* CANON_CORE_ARENA_H */
