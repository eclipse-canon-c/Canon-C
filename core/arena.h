#ifndef CANON_CORE_ARENA_H
#define CANON_CORE_ARENA_H

#include <stdbool.h>             /* bool, true, false (C99) */

#include "core/primitives/types.h"    /* u8, usize */
#include "core/primitives/limits.h"   /* CANON_ARENA_MAX_SIZE, CANON_USIZE_MAX, CANON_MAX_ALIGN */
#include "core/primitives/contract.h" /* require_msg */
#include "core/primitives/ptr.h"      /* ptr_offset, ptr_align_up, ptr_span */
#include "core/primitives/bits.h"     /* is_power_of_two */
#include "core/memory.h"              /* mem_zero, mem_secure_zero */
#include "core/slice.h"               /* bytes_t, bytes_from, bytes_empty */

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
 * Dependency rule:
 *   arena.h is core/. It depends only on primitives/, core/memory.h, and
 *   core/slice.h. No data/, semantics/, algo/, or util/ headers may be
 *   included here.
 *
 * @sa arena_alloc(), arena_alloc_aligned(), arena_mark(), arena_reset_to()
 */

/* ============================================================================
   Arena struct and mark type
   ============================================================================ */

/**
 * @brief Arena instance.
 *
 * Fields are public for inspection but must not be modified directly.
 * Use the API functions below to manage state.
 *
 * padding_accum tracks cumulative alignment padding bytes consumed since
 * the last full reset. This is needed so arena_reset_secure() can wipe
 * the entire used+padding region, not just allocated bytes.
 */
typedef struct Arena {
    u8*   buffer;          /**< Start of caller-owned memory block          */
    usize capacity;        /**< Total usable bytes in buffer                */
    usize offset;          /**< Bump pointer: bytes consumed (used+padding) */
    usize padding_accum;   /**< Cumulative alignment padding since last reset */
#ifdef CANON_ARENA_DEBUG
    usize alloc_count;     /**< [debug] Successful alloc calls since reset  */
    usize peak;            /**< [debug] Max offset ever reached since reset  */
#endif
} Arena;

/**
 * @brief Opaque checkpoint for partial rollback.
 *
 * Returned by arena_mark(), consumed by arena_reset_to().
 * Treat as opaque — do not rely on internal representation.
 */
typedef usize ArenaMark;

/* ============================================================================
   Debug stats (only available with CANON_ARENA_DEBUG)
   ============================================================================ */

#ifdef CANON_ARENA_DEBUG
/**
 * @brief Snapshot of arena debug state.
 * All values reflect state since last full reset.
 */
typedef struct {
    usize used;        /**< Bytes currently consumed (allocations + padding) */
    usize remaining;   /**< Bytes still available                            */
    usize capacity;    /**< Total arena capacity                             */
    usize peak;        /**< Maximum bytes ever consumed (high watermark)     */
    usize alloc_count; /**< Total successful allocations                     */
} ArenaStats;

/**
 * @brief Returns a snapshot of current arena debug state.
 * NULL-safe — returns zeroed stats for NULL arena.
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
    #define _arena_debug_update(a) \
        do { \
            (a)->alloc_count += 1; \
            if ((a)->offset > (a)->peak) (a)->peak = (a)->offset; \
        } while (0)
    #define _arena_debug_reset(a) \
        do { (a)->alloc_count = 0; (a)->peak = 0; } while (0)
#else
    #define _arena_debug_update(a) ((void)0)
    #define _arena_debug_reset(a)  ((void)0)
#endif

/* ============================================================================
   Initialization & reset
   ============================================================================ */

/**
 * @brief Initializes an arena from a caller-provided buffer.
 *
 * @param arena    Valid pointer to uninitialized Arena struct
 * @param buffer   Pointer to memory block (must remain valid for arena lifetime)
 * @param capacity Size of buffer in bytes (> 0, <= CANON_ARENA_MAX_SIZE)
 *
 * @pre arena != NULL && buffer != NULL && capacity > 0
 * @pre capacity <= CANON_ARENA_MAX_SIZE
 *
 * Performance: O(1)
 */
static inline void arena_init(Arena* arena, void* buffer, usize capacity) {
    require_msg(arena    != NULL, "arena_init: arena cannot be NULL");
    require_msg(buffer   != NULL, "arena_init: buffer cannot be NULL");
    require_msg(capacity  > 0,    "arena_init: capacity must be > 0");
    require_msg(capacity <= CANON_ARENA_MAX_SIZE,
                "arena_init: capacity exceeds CANON_ARENA_MAX_SIZE");

    arena->buffer       = (u8*)buffer;
    arena->capacity     = capacity;
    arena->offset       = 0;
    arena->padding_accum = 0;
    _arena_debug_reset(arena);
}

/**
 * @brief Resets arena to empty state (fast path).
 *
 * Moves offset back to 0. Memory content is NOT cleared.
 * Use arena_reset_secure() for sensitive data.
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * Performance: O(1)
 */
static inline void arena_reset(Arena* arena) {
    if (!arena) return;
    arena->offset       = 0;
    arena->padding_accum = 0;
    _arena_debug_reset(arena);
}

/**
 * @brief Resets arena and securely wipes all consumed memory.
 *
 * Zeros the full consumed region: allocations AND alignment padding bytes,
 * so no residual data remains anywhere in the used range.
 *
 * Slower than arena_reset() — O(offset).
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * Performance: O(offset)
 */
static inline void arena_reset_secure(Arena* arena) {
    if (!arena || arena->offset == 0) return;
    mem_secure_zero(arena->buffer, arena->offset);
    arena->offset        = 0;
    arena->padding_accum = 0;
    _arena_debug_reset(arena);
}

/* ============================================================================
   Allocation
   ============================================================================ */

/**
 * @brief Allocates size bytes with natural alignment (CANON_MAX_ALIGN).
 *
 * @param arena Valid initialized arena
 * @param size  Bytes to allocate (> 0; returns NULL for size == 0)
 *
 * @return Aligned pointer, or NULL if arena is full or size == 0.
 *
 * Performance: O(1)
 */
static inline void* arena_alloc(Arena* arena, usize size) {
    require_msg(arena != NULL, "arena_alloc: arena cannot be NULL");
    if (size == 0) return NULL;

    void* current     = ptr_offset(arena->buffer, arena->offset);
    void* aligned_ptr = ptr_align_up(current, CANON_MAX_ALIGN);
    usize pad         = ptr_span(aligned_ptr, current);

    if (arena->offset > CANON_USIZE_MAX - pad ||
        arena->offset + pad > CANON_USIZE_MAX - size ||
        arena->offset + pad + size > arena->capacity) {
        return NULL;
    }

    arena->offset        += pad;
    arena->padding_accum += pad;
    void* result          = ptr_offset(arena->buffer, arena->offset);
    arena->offset        += size;
    _arena_debug_update(arena);
    return result;
}

/**
 * @brief Allocates size bytes with a specific power-of-2 alignment.
 *
 * @param arena     Valid initialized arena
 * @param size      Bytes to allocate (> 0; returns NULL for size == 0)
 * @param alignment Power-of-2 alignment (1, 2, 4, 8, 16, ...)
 *
 * @pre is_power_of_two(alignment)
 *
 * @return Aligned pointer, or NULL if arena is full or size == 0.
 *
 * Performance: O(1)
 */
static inline void* arena_alloc_aligned(Arena* arena, usize size, usize alignment) {
    require_msg(arena != NULL, "arena_alloc_aligned: arena cannot be NULL");
    require_msg(is_power_of_two(alignment),
                "arena_alloc_aligned: alignment must be a power of 2");
    if (size == 0) return NULL;

    void* current     = ptr_offset(arena->buffer, arena->offset);
    void* aligned_ptr = ptr_align_up(current, alignment);
    usize pad         = ptr_span(aligned_ptr, current);

    if (arena->offset > CANON_USIZE_MAX - pad ||
        arena->offset + pad > CANON_USIZE_MAX - size ||
        arena->offset + pad + size > arena->capacity) {
        return NULL;
    }

    arena->offset        += pad;
    arena->padding_accum += pad;
    void* result          = ptr_offset(arena->buffer, arena->offset);
    arena->offset        += size;
    _arena_debug_update(arena);
    return result;
}

/* ============================================================================
   Zero-initializing variants
   ============================================================================ */

/** @brief Allocates size bytes and zero-initializes them. O(size) */
static inline void* arena_alloc_zero(Arena* arena, usize size) {
    void* p = arena_alloc(arena, size);
    if (p) mem_zero(p, size);
    return p;
}

/** @brief Allocates aligned size bytes and zero-initializes them. O(size) */
static inline void* arena_alloc_aligned_zero(Arena* arena, usize size, usize alignment) {
    void* p = arena_alloc_aligned(arena, size, alignment);
    if (p) mem_zero(p, size);
    return p;
}

/* ============================================================================
   Try-alloc variants (bool return, output via pointer)
   ============================================================================ */

static inline bool arena_try_alloc(Arena* arena, usize size, void** out) {
    void* p = arena_alloc(arena, size);
    if (out) *out = p;
    return p != NULL;
}

static inline bool arena_try_alloc_aligned(Arena* arena, usize size, usize alignment, void** out) {
    void* p = arena_alloc_aligned(arena, size, alignment);
    if (out) *out = p;
    return p != NULL;
}

/* ============================================================================
   Query
   ============================================================================ */

/** @brief Total capacity of the arena in bytes. NULL-safe. */
static inline usize arena_capacity(const Arena* arena) {
    return arena ? arena->capacity : 0;
}

/** @brief Bytes still available for allocation. NULL-safe. */
static inline usize arena_remaining(const Arena* arena) {
    return arena ? (arena->capacity - arena->offset) : 0;
}

/** @brief Bytes currently consumed (allocations + alignment padding). NULL-safe. */
static inline usize arena_used(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/** @brief True if the arena has no active allocations. NULL-safe. */
static inline bool arena_is_empty(const Arena* arena) {
    return !arena || arena->offset == 0;
}

/** @brief True if the arena has no remaining space. NULL-safe. */
static inline bool arena_is_full(const Arena* arena) {
    return !arena || arena->offset >= arena->capacity;
}

/* ============================================================================
   Checkpoint / rollback
   ============================================================================ */

/**
 * @brief Saves the current allocation position as a rollback point.
 *
 * Pair with arena_reset_to() for scoped temporary allocations:
 *
 *   ArenaMark m = arena_mark(&arena);
 *   // ... temporary work ...
 *   arena_reset_to(&arena, m);
 *
 * @param arena Arena to checkpoint (NULL-safe — returns 0)
 */
static inline ArenaMark arena_mark(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Rolls back arena state to a previously saved mark.
 *
 * Invalidates all allocations made after the mark was taken.
 * Memory content is NOT cleared — use arena_reset_secure() for that.
 *
 * @note alloc_count and peak are NOT rolled back. They reflect cumulative
 * activity since the last full reset, not since the mark.
 *
 * @pre mark <= arena->offset
 *
 * @param arena Arena to roll back (NULL-safe)
 * @param mark  Value returned by a prior arena_mark() on this arena
 */
static inline void arena_reset_to(Arena* arena, ArenaMark mark) {
    if (!arena) return;
    require_msg(mark <= arena->offset,
                "arena_reset_to: mark is ahead of current offset");
    arena->offset = mark;
    /* padding_accum is intentionally not rolled back — it is a cumulative
       diagnostic counter, not required for correctness of rollback. */
}

/* ============================================================================
   Byte views — slice.h integration
   ============================================================================ */

/**
 * @brief View over the currently consumed region [buffer, buffer+offset).
 * Non-owning — becomes invalid after any reset.
 */
static inline bytes_t arena_as_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_as_bytes: arena cannot be NULL");
    return bytes_from(arena->buffer, arena->offset);
}

/**
 * @brief View over the entire buffer [buffer, buffer+capacity).
 * Use arena_as_bytes() for the used portion only.
 */
static inline bytes_t arena_buffer_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_buffer_bytes: arena cannot be NULL");
    return bytes_from(arena->buffer, arena->capacity);
}

/**
 * @brief View over the remaining (unallocated) region [buffer+offset, buffer+capacity).
 * Useful for passing free space to I/O operations.
 */
static inline bytes_t arena_free_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_free_bytes: arena cannot be NULL");
    if (arena->offset >= arena->capacity) return bytes_empty();
    return bytes_from(ptr_offset(arena->buffer, arena->offset),
                      arena->capacity - arena->offset);
}

/* ============================================================================
   Typed allocation macros
   ============================================================================ */

/** @brief Allocates one object of Type from arena. */
#define arena_alloc_type(arena, Type) \
    ((Type*)arena_alloc((arena), sizeof(Type)))

/**
 * @brief Allocates an array of count objects of Type from arena.
 *
 * sizeof(Type) * count is NOT overflow-checked in this macro.
 * For untrusted count values, compute the size separately with a
 * checked multiplication before calling arena_alloc() directly.
 */
#define arena_alloc_array(arena, Type, count) \
    ((Type*)arena_alloc((arena), sizeof(Type) * (count)))

/** @brief Allocates and zero-initializes one object of Type. */
#define arena_alloc_type_zero(arena, Type) \
    ((Type*)arena_alloc_zero((arena), sizeof(Type)))

/**
 * @brief Allocates and zero-initializes an array of count objects of Type.
 * Same overflow caveat as arena_alloc_array.
 */
#define arena_alloc_array_zero(arena, Type, count) \
    ((Type*)arena_alloc_zero((arena), sizeof(Type) * (count)))

#endif /* CANON_CORE_ARENA_H */
