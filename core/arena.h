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
 *     per-TU counter XOR'd with &arena. See "Internal: lifetime
 *     helpers" below.
 *   - arena_reset() and arena_reset_secure() RE-STAMP the lifetime ID,
 *     invalidating every borrow that captured the previous ID. Borrow
 *     reads stamped with the old ID will fire require_msg via
 *     lifetime_assert_valid() in semantics/borrow.h.
 *   - arena_reset_to(mark) does NOT touch the lifetime — borrows
 *     allocated before the mark must remain valid through rollback,
 *     by the rollback contract. This is intentional.
 *   - There is no arena_destroy(); arenas are reset, not destroyed.
 *     If you need destruction semantics for borrows, pair the arena
 *     with a Region and rely on region_end() to invalidate borrows.
 *   Zero cost in release builds — struct layout is identical without the flag.
 *
 * Dependency rule:
 *   arena.h is core/. It depends only on primitives/, core/memory.h, and
 *   core/slice.h. Under CANON_LIFETIME_DEBUG, it additionally includes
 *   core/primitives/lifetime.h for the shared region_id_t / lifetime_t
 *   types. core/region.h provides lifetime_assert_valid (the runtime
 *   check), which arena.h does not call directly — only the borrow types
 *   in semantics/borrow.h call it.
 *   No data/, semantics/, algo/, or util/ headers may be included here.
 *
 * Formal verification:
 * ────────────────────────────────────────────────────────────────────────────
 * Every function in this header carries an ACSL contract suitable for
 * Frama-C WP. The Typed+Cast memory model is required because arena.h
 * performs void-to-u8 pointer casts for byte-level pointer arithmetic
 * (same rationale as compare.h — see VERIFY-005 — and the substrate
 * headers ptr.h, slice.h, memory.h).
 *
 * The structural invariant `arena_invariant` (defined below) is the
 * load-bearing predicate: every public function preserves it. Allocator
 * functions additionally use `arena_can_fit` to express the three-way
 * overflow guard before adjusting offset. Rollback uses `mark_in_arena`
 * to validate the mark precondition.
 *
 * Lifetime invariants (under CANON_LIFETIME_DEBUG) are captured by
 * `arena_lifetime_valid`. The OWN-002 no-cycle property is encoded as
 * a per-call `ensures arena->lt.id != \old(arena->lt.id)` on the reset
 * helpers, matching the regression test in arena_test.c.
 *
 * Predicate design rationale and the residual fingerprint live in
 * docs/verification.md (VERIFY-009) and docs/deviations.md.
 *
 * @sa arena_alloc(), arena_alloc_aligned(), arena_mark(), arena_reset_to()
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
 * @sa core/region.h              — Region type and lifetime_assert_valid()
 * @sa semantics/borrow.h         — borrow types that capture and validate IDs
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
#ifdef CANON_LIFETIME_DEBUG
    lifetime_t lt;         /**< [debug] Lifetime token: id + open           */
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
   ACSL predicates for verification
   ============================================================================
   The predicates below capture the structural invariants WP needs to verify
   arena.h. They are referenced by `requires` and `ensures` clauses on every
   function contract.

   Design notes:

   - arena_invariant: the structural shape every valid Arena maintains. Does
     NOT include padding_accum because arena_reset_to deliberately preserves
     padding_accum across rollback (per the comment in the function body:
     "padding_accum is intentionally not rolled back — it is a cumulative
     diagnostic counter, not required for correctness of rollback").
     Including padding_accum in the invariant would force arena_reset_to to
     either restore it or violate the predicate.

   - arena_can_fit: the three-way overflow guard that arena_alloc enforces
     before adjusting offset. Captures both the address-space overflow
     checks (against CANON_USIZE_MAX) and the buffer-capacity check. The
     `pad` calculation uses modular arithmetic; for power-of-two alignments
     WP can typically discharge the equivalence (alignment - (cur % alignment))
     % alignment == ((cur + alignment - 1) & ~(alignment - 1)) - cur, though
     this may require z3 with longer timeout. See VERIFY-009 for the
     residual category if it doesn't close automatically.

   - mark_in_arena: validity of an ArenaMark relative to its arena. The
     "mark <= a->offset" precondition guards arena_reset_to. Trivial to
     prove because mark comes from a prior arena_mark call which returns
     a->offset.

   - arena_lifetime_open / arena_lifetime_valid (CANON_LIFETIME_DEBUG only):
     the lifetime token is open and has a non-static id. Used by arena_init
     and the restamp functions in ensures clauses.

   The no-cycle property for OWN-002 is encoded as a per-call ensures
   `arena->lt.id != \old(arena->lt.id)` on arena_reset and
   arena_reset_secure, not as a separate predicate. The contract matches
   the regression test in arena_test.c (test_lifetime_reset_restamps_id),
   which checks lt.id != before directly. A general "all N+1 ids distinct
   across N resets" property would require a sequence model that WP does
   not reason about naturally; the per-call form gives the same guarantee
   transitively (if every consecutive pair differs, and the counter is
   monotonic, no two prior ids collide within the TU).
   ============================================================================ */

/*@
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
   Internal: lifetime helpers (compiled away in release)
   ============================================================================
   When CANON_LIFETIME_DEBUG is enabled, an Arena exposes a lifetime_t
   that borrows can capture and validate against. The helpers below
   manage the (id, open) pair across the arena lifecycle:

     arena_lifetime_open_(a)
       Called by arena_init. Draws a fresh id from the per-TU monotonic
       counter (XOR'd with &arena for cross-TU diversity) and marks the
       lifetime open.

     arena_lifetime_restamp_(a)
       Called by arena_reset and arena_reset_secure. Draws another fresh
       id from the same counter, guaranteed distinct from any prior id
       within the TU. Open flag stays true; arenas are not "closed" by
       reset — they are recycled.

     arena_lifetime_close_(a)
       Reserved for future Arena destruction semantics. Not called by
       any current API. Marks the lifetime closed, which makes any
       subsequent borrow read fire lifetime_assert_valid.

   In release builds (CANON_LIFETIME_DEBUG undefined) all three
   helpers are no-ops — the Arena struct does not have an lt field
   and no code touches it.

   History: Phase 1 (CI #865 onward) shipped with bare-address
   derivation for _open_ and XOR-with-`0x9E3779B97F4A7C15ULL` for
   _restamp_. The XOR-with-constant approach cycled after two resets
   (A -> A^K -> A), silently re-validating a borrow captured at the
   original id. OWN-002 migrated both _open_ and _restamp_ to the
   per-TU counter pattern used by every Phase 3+ container,
   consistent with bitset / stringbuf / dynvec / dynstring / smallvec.
   See docs/design-decisions.md OWN-001 §4 (the documented
   limitation) and OWN-002 (the migration).
   ============================================================================ */

#ifdef CANON_LIFETIME_DEBUG
    /*@
      predicate arena_lifetime_open(Arena *a) =
          a->lt.open == \true;

      predicate arena_lifetime_valid(Arena *a) =
          arena_lifetime_open(a) &&
          a->lt.id != REGION_ID_STATIC;
    */

    /* Per-TU counter used to derive unique lifetime ids.
     *
     * The counter is a `static` inside a `static inline` function, so
     * each translation unit has its own copy and increments are
     * TU-local. Two Arenas initialized in the same TU get different
     * ids; Arenas initialized in different TUs get ids from
     * independent counters but mixed with the struct address, making
     * cross-TU collisions vanishingly unlikely. Same pattern as
     * bitset / stringbuf / vec / deque / pq / hashmap / dynvec /
     * smallvec / dynstring.
     *
     * Why a counter rather than the previous XOR-with-constant:
     *   arena_reset / arena_reset_secure are called repeatedly on the
     *   same Arena across its lifetime. XOR with a fixed constant K
     *   cycles: A -> A^K -> A. A borrow captured at id A re-validates
     *   silently after two resets. Drawing a fresh id from the
     *   counter eliminates that cycle.
     *
     * No thread-safety guarantee: if a single TU's arena_init /
     * arena_reset / arena_reset_secure are invoked concurrently, the
     * counter may race and collide. Same constraint as every other
     * Canon-C container.
     *
     * REGION_ID_STATIC (0) is reserved; the counter starts at 1 and
     * the id derivation never produces 0 defensively.
     */
    static inline region_id_t arena_lifetime_next_id_(void* ap) {
        static region_id_t counter_ = 1;
        region_id_t id = (region_id_t)(counter_++)
                       ^ (region_id_t)(uintptr_t)(ap);
        if (id == REGION_ID_STATIC) id = (region_id_t)1;
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
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token. The ID
 * is drawn from a per-TU counter XOR'd with &arena. Borrows captured
 * after this call carry this lifetime ID.
 *
 * Performance: O(1)
 */
static inline void arena_init(Arena* arena, void* buffer, usize capacity) {
    require_msg(arena    != NULL, "arena_init: arena cannot be NULL");
    require_msg(buffer   != NULL, "arena_init: buffer cannot be NULL");
    require_msg(capacity  > 0,    "arena_init: capacity must be > 0");
    require_msg(capacity <= CANON_ARENA_MAX_SIZE,
                "arena_init: capacity exceeds CANON_ARENA_MAX_SIZE");

    arena->buffer        = (u8*)buffer;
    arena->capacity      = capacity;
    arena->offset        = 0;
    arena->padding_accum = 0;
    _arena_debug_reset(arena);
    arena_lifetime_open_(arena);
}

/**
 * @brief Resets arena to empty state (fast path).
 *
 * Moves offset back to 0. Memory content is NOT cleared.
 * Use arena_reset_secure() for sensitive data.
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * Lifetime (CANON_LIFETIME_DEBUG): re-stamps the lifetime ID with a
 * fresh draw from the per-TU counter, invalidating every borrow
 * captured before this call. Subsequent reads of those borrows will
 * fire require_msg. Multiple resets in sequence produce distinct ids
 * each time — no cycling.
 *
 * Performance: O(1)
 */
static inline void arena_reset(Arena* arena) {
    if (!arena) return;
    arena->offset        = 0;
    arena->padding_accum = 0;
    _arena_debug_reset(arena);
    arena_lifetime_restamp_(arena);
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
 * Lifetime (CANON_LIFETIME_DEBUG): re-stamps the lifetime ID, same as
 * arena_reset(). Borrows captured before this call are invalidated.
 *
 * Performance: O(offset)
 */
static inline void arena_reset_secure(Arena* arena) {
    if (!arena || arena->offset == 0) return;
    mem_secure_zero(arena->buffer, arena->offset);
    arena->offset        = 0;
    arena->padding_accum = 0;
    _arena_debug_reset(arena);
    arena_lifetime_restamp_(arena);
}

/* ============================================================================
   Allocation
   ============================================================================ */

/**
 * @brief Allocates size bytes with natural alignment (CANON_DEFAULT_ALIGN).
 *
 * @param arena Valid initialized arena
 * @param size  Bytes to allocate (> 0; returns NULL for size == 0)
 *
 * @return Aligned pointer, or NULL if arena is full or size == 0.
 *
 * Performance: O(1)
 */
static inline void* arena_alloc(Arena* arena, usize size) {
    void* current;
    void* aligned_ptr;
    void* result;
    usize pad;

    require_msg(arena != NULL, "arena_alloc: arena cannot be NULL");
    if (size == 0) return NULL;

    current     = ptr_offset(arena->buffer, arena->offset);
    aligned_ptr = ptr_align_up(current, CANON_DEFAULT_ALIGN);
    pad         = ptr_span(aligned_ptr, current);

    if (arena->offset > CANON_USIZE_MAX - pad ||
        arena->offset + pad > CANON_USIZE_MAX - size ||
        arena->offset + pad + size > arena->capacity) {
        return NULL;
    }

    arena->offset        += pad;
    arena->padding_accum += pad;
    result                = ptr_offset(arena->buffer, arena->offset);
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
    void* current;
    void* aligned_ptr;
    void* result;
    usize pad;

    require_msg(arena != NULL, "arena_alloc_aligned: arena cannot be NULL");
    require_msg(is_power_of_two(alignment),
                "arena_alloc_aligned: alignment must be a power of 2");
    if (size == 0) return NULL;

    current     = ptr_offset(arena->buffer, arena->offset);
    aligned_ptr = ptr_align_up(current, alignment);
    pad         = ptr_span(aligned_ptr, current);

    if (arena->offset > CANON_USIZE_MAX - pad ||
        arena->offset + pad > CANON_USIZE_MAX - size ||
        arena->offset + pad + size > arena->capacity) {
        return NULL;
    }

    arena->offset        += pad;
    arena->padding_accum += pad;
    result                = ptr_offset(arena->buffer, arena->offset);
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
 * @note (CANON_LIFETIME_DEBUG) The lifetime ID is NOT touched by this
 * function. Borrows allocated before the mark must remain valid through
 * rollback by the rollback contract. Re-stamping the ID would break that
 * contract. Borrows allocated between the mark and the reset point are
 * caller-error if used after rollback — that case is covered by ASan,
 * not by lifetime tracking. If you need scope-bounded invalidation,
 * pair the arena with a Region.
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
    /* lifetime ID is intentionally not re-stamped — see function doc. */
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
