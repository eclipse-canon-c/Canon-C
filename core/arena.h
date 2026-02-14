#ifndef CANON_CORE_ARENA_H
#define CANON_CORE_ARENA_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"

/**
 * @file arena.h
 * @brief Linear bump allocator with explicit lifetime control
 *
 * Provides a very fast, simple, predictable arena (bump-pointer) allocator
 * that allocates sequentially from a fixed, caller-owned buffer.
 *
 * No individual deallocation is supported — memory is released only via
 * full reset or rollback to a previous mark.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Extremely fast: pointer bump + minimal alignment padding
 * - Zero hidden allocations or metadata overhead
 * - Caller completely controls memory lifetime and buffer
 * - Cache-friendly: all allocations are contiguous
 * - Explicit lifetime: reset or rollback required to reuse memory
 * - Two reset modes: fast (just offset=0) and secure (memset 0)
 * - Checkpoint/rollback pattern via ArenaMark
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) for alloc, O(n) for secure reset
 * - Space complexity: O(1) metadata (~24 bytes per arena)
 * - No per-allocation overhead (no headers/footers)
 * - Alignment padding is the only "waste"
 * - Typical alloc: <10 CPU cycles (just add + align)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe by design.
 * No internal locking — caller must synchronize access when used
 * from multiple threads (mutex, per-thread arenas, etc.).
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * arena.h is core/. It depends only on primitives/, core/memory.h, and
 * core/slice.h. No data/, semantics/, algo/, or util/ headers may be
 * included here.
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Per-frame / per-request temporary allocations
 * - Parser / lexer scratch memory
 * - AST / DOM / intermediate representation nodes
 * - Command buffers, transaction scratchpads
 * - Any workload with clear "begin → work → reset" phases
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Long-lived objects that outlive the arena
 * - General-purpose allocation (use malloc / pool instead)
 * - Objects that require individual deallocation
 * - Multi-threaded allocation without external synchronization
 *
 * @sa arena_alloc(), arena_alloc_aligned(), arena_reset_to(), arena_mark()
 * @sa core/region.h — attach an arena to a lifetime region
 */

/* ════════════════════════════════════════════════════════════════════════════
   Arena struct and mark type
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Arena instance — holds buffer, capacity and current position
 *
 * Fields are public for inspection but must not be modified directly.
 * Use arena_init(), arena_reset(), and arena_reset_to() to manage state.
 */
typedef struct Arena {
    u8*   buffer;   ///< Start of the memory block (caller-owned)
    usize capacity; ///< Total usable bytes in buffer
    usize offset;   ///< Current bump pointer offset from buffer start
} Arena;

/**
 * @brief Opaque checkpoint type for partial rollback
 *
 * Returned by arena_mark(), consumed by arena_reset_to().
 * Treat as opaque — do not rely on its internal representation.
 *
 * @sa arena_mark(), arena_reset_to()
 */
typedef usize ArenaMark;

/* ════════════════════════════════════════════════════════════════════════════
   Initialization & state management
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes an arena using caller-provided memory
 *
 * @param arena    Valid pointer to uninitialized Arena struct
 * @param buffer   Pointer to memory block (must remain valid for arena lifetime)
 * @param capacity Size of buffer in bytes (> 0)
 *
 * @pre arena != NULL
 * @pre buffer != NULL
 * @pre capacity > 0
 * @pre capacity <= CANON_ARENA_MAX_SIZE
 *
 * @post arena->buffer == buffer
 * @post arena->capacity == capacity
 * @post arena->offset == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void arena_init(Arena* arena, void* buffer, usize capacity) {
    require_msg(arena != NULL,   "arena_init: arena cannot be NULL");
    require_msg(buffer != NULL,  "arena_init: buffer cannot be NULL");
    require_msg(capacity > 0,    "arena_init: capacity must be > 0");
    require_msg(capacity <= CANON_ARENA_MAX_SIZE,
        "arena_init: capacity exceeds CANON_ARENA_MAX_SIZE");
    if (!arena || !buffer || capacity == 0) return;
    arena->buffer   = (u8*)buffer;
    arena->capacity = capacity;
    arena->offset   = 0;
}

/**
 * @brief Resets arena to empty state (fast path)
 *
 * Invalidates all previous allocations by moving offset back to 0.
 * Memory content is NOT cleared — use arena_reset_secure() for sensitive data.
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void arena_reset(Arena* arena) {
    if (arena) arena->offset = 0;
}

/**
 * @brief Resets arena and securely wipes all previously allocated memory
 *
 * Use when arena contained sensitive data (keys, passwords, tokens, etc.).
 * Slower than arena_reset() — O(used bytes).
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * Performance:
 * - Time:  O(used bytes)
 * - Space: O(1)
 */
static inline void arena_reset_secure(Arena* arena) {
    if (arena && arena->offset > 0) {
        mem_secure_zero(arena->buffer, arena->offset);
        arena->offset = 0;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Allocation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocates size bytes with natural alignment
 *
 * Natural alignment is sizeof(max_align_t):
 * - 8 bytes on 64-bit platforms
 * - 4 bytes on 32-bit platforms
 *
 * Uses ptr_align_up() and ptr_span() from core/primitives/ptr.h for all
 * pointer arithmetic — no raw uintptr_t casts in the function body.
 *
 * @param arena Valid initialized arena
 * @param size  Bytes to allocate (> 0)
 *
 * @return Pointer to aligned memory block, or NULL if:
 *         - arena == NULL or size == 0
 *         - insufficient remaining space (after alignment padding)
 *         - alignment padding overflows usize
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void* arena_alloc(Arena* arena, usize size) {
    require_msg(arena != NULL, "arena_alloc: arena cannot be NULL");
    if (!arena || size == 0) return NULL;

    void* current     = ptr_offset(arena->buffer, arena->offset);
    void* aligned_ptr = ptr_align_up(current, CANON_MAX_ALIGN);
    usize pad         = ptr_span(aligned_ptr, current);

    if (arena->offset > CANON_USIZE_MAX - pad)         return NULL;
    if (arena->offset + pad > CANON_USIZE_MAX - size)  return NULL;
    if (arena->offset + pad + size > arena->capacity)  return NULL;

    arena->offset += pad;
    void* result = ptr_offset(arena->buffer, arena->offset);
    arena->offset += size;
    return result;
}

/**
 * @brief Allocates size bytes with a specific power-of-2 alignment
 *
 * Uses ptr_align_up() and ptr_span() from core/primitives/ptr.h — replaces
 * the raw uintptr_t alignment arithmetic from the original implementation.
 *
 * @param arena     Valid initialized arena
 * @param size      Bytes to allocate (> 0)
 * @param alignment Power-of-2 alignment requirement (1, 2, 4, 8, 16, ...)
 *
 * @return Aligned pointer or NULL if:
 *         - arena == NULL, size == 0, or alignment not power of 2
 *         - insufficient space (including alignment padding)
 *
 * @pre is_power_of_two(alignment)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 *
 * Example:
 * ```c
 * float* v = arena_alloc_aligned(&arena, sizeof(float) * 4, 16);
 * ```
 */
static inline void* arena_alloc_aligned(Arena* arena, usize size, usize alignment) {
    require_msg(arena != NULL,          "arena_alloc_aligned: arena cannot be NULL");
    require_msg(is_power_of_two(alignment),
        "arena_alloc_aligned: alignment must be a power of 2");
    if (!arena || size == 0 || !is_power_of_two(alignment)) return NULL;

    void* current     = ptr_offset(arena->buffer, arena->offset);
    void* aligned_ptr = ptr_align_up(current, alignment);
    usize pad         = ptr_span(aligned_ptr, current);

    if (arena->offset > CANON_USIZE_MAX - pad         ||
        arena->offset + pad > CANON_USIZE_MAX - size  ||
        arena->offset + pad + size > arena->capacity) {
        return NULL;
    }

    arena->offset += pad;
    void* result = ptr_offset(arena->buffer, arena->offset);
    arena->offset += size;
    return result;
}

/* ════════════════════════════════════════════════════════════════════════════
   Zero-initializing variants
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocates size bytes and zero-initializes them
 *
 * @param arena Valid arena
 * @param size  Bytes to allocate and zero (> 0)
 * @return Zeroed pointer or NULL on failure
 *
 * Performance:
 * - Time:  O(size)
 * - Space: O(1)
 */
static inline void* arena_alloc_zero(Arena* arena, usize size) {
    void* p = arena_alloc(arena, size);
    if (p) mem_zero(p, size);
    return p;
}

/**
 * @brief Allocates aligned size bytes and zero-initializes them
 *
 * @param arena     Valid arena
 * @param size      Bytes to allocate and zero (> 0)
 * @param alignment Power-of-2 alignment
 * @return Zeroed aligned pointer or NULL on failure
 *
 * Performance:
 * - Time:  O(size)
 * - Space: O(1)
 */
static inline void* arena_alloc_aligned_zero(Arena* arena, usize size, usize alignment) {
    void* p = arena_alloc_aligned(arena, size, alignment);
    if (p) mem_zero(p, size);
    return p;
}

/* ════════════════════════════════════════════════════════════════════════════
   Query & checkpoint
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns total capacity of the arena in bytes
 *
 * @param arena Arena to query (NULL-safe)
 *
 * Performance: O(1)
 */
static inline usize arena_capacity(const Arena* arena) {
    return arena ? arena->capacity : 0;
}

/**
 * @brief Returns number of bytes still available for allocation
 *
 * @param arena Arena to query (NULL-safe)
 *
 * Performance: O(1)
 */
static inline usize arena_remaining(const Arena* arena) {
    return arena ? (arena->capacity - arena->offset) : 0;
}

/**
 * @brief Returns number of bytes currently allocated
 *
 * @param arena Arena to query (NULL-safe)
 *
 * Performance: O(1)
 */
static inline usize arena_used(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Returns true if the arena has no active allocations
 *
 * @param arena Arena to check (NULL-safe)
 *
 * Performance: O(1)
 */
static inline bool arena_is_empty(const Arena* arena) {
    return !arena || arena->offset == 0;
}

/**
 * @brief Returns true if the arena has no remaining space
 *
 * @param arena Arena to check (NULL-safe)
 *
 * Performance: O(1)
 */
static inline bool arena_is_full(const Arena* arena) {
    return !arena || arena->offset >= arena->capacity;
}

/**
 * @brief Creates a checkpoint of the current allocation position
 *
 * Pair with arena_reset_to() for scoped temporary allocations:
 * ```c
 * ArenaMark mark = arena_mark(&arena);
 * // ... temporary allocations ...
 * arena_reset_to(&arena, mark); // all temporaries invalidated
 * ```
 *
 * @param arena Arena to mark (NULL-safe → 0)
 * @return Opaque mark value representing current offset
 *
 * Performance: O(1)
 */
static inline ArenaMark arena_mark(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Rolls back arena state to a previous mark
 *
 * Invalidates all allocations made after the mark was taken.
 * Memory content is NOT cleared.
 *
 * @param arena Arena to roll back (NULL-safe)
 * @param mark  Value previously returned by arena_mark()
 *
 * @pre mark <= arena->offset
 *
 * Performance: O(1)
 */
static inline void arena_reset_to(Arena* arena, ArenaMark mark) {
    if (!arena) return;
    ensure_msg(mark <= arena->offset,
        "arena_reset_to: mark is ahead of current offset");
    if (mark <= arena->offset) arena->offset = mark;
}

/* ════════════════════════════════════════════════════════════════════════════
   Byte views — slice.h integration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a bytes_t view over the currently allocated region
 *
 * Covers [buffer, buffer + offset) — all bytes allocated so far.
 * The view is non-owning and becomes invalid after any reset.
 *
 * @param arena Arena to view (must not be NULL)
 * @return bytes_t over the used portion of the arena buffer
 *
 * @pre arena != NULL
 *
 * Performance: O(1)
 */
static inline bytes_t arena_as_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_as_bytes: arena cannot be NULL");
    return bytes_from(arena->buffer, arena->offset);
}

/**
 * @brief Returns a bytes_t view over the entire arena buffer (used + free)
 *
 * Covers [buffer, buffer + capacity). Use arena_as_bytes() for used only.
 *
 * @param arena Arena to view (must not be NULL)
 * @return bytes_t over the full arena buffer
 *
 * @pre arena != NULL
 *
 * Performance: O(1)
 */
static inline bytes_t arena_buffer_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_buffer_bytes: arena cannot be NULL");
    return bytes_from(arena->buffer, arena->capacity);
}

/**
 * @brief Returns a bytes_t view over the remaining (unallocated) region
 *
 * Covers [buffer + offset, buffer + capacity). Useful for passing the
 * free region to I/O operations that fill memory directly.
 *
 * @param arena Arena to view (must not be NULL)
 * @return bytes_t over the free portion — empty if arena is full
 *
 * @pre arena != NULL
 *
 * Performance: O(1)
 */
static inline bytes_t arena_free_bytes(const Arena* arena) {
    require_msg(arena != NULL, "arena_free_bytes: arena cannot be NULL");
    if (arena->offset >= arena->capacity) return bytes_empty();
    return bytes_from(
        ptr_offset(arena->buffer, arena->offset),
        arena->capacity - arena->offset
    );
}

/* ════════════════════════════════════════════════════════════════════════════
   Try-alloc variants
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Attempts allocation, returns success flag and pointer via out param
 *
 * @param arena Valid arena
 * @param size  Bytes to allocate
 * @param out   Receives allocated pointer (NULL on failure)
 * @return true on success, false on failure
 *
 * Performance: O(1)
 */
static inline bool arena_try_alloc(Arena* arena, usize size, void** out) {
    void* p = arena_alloc(arena, size);
    if (out) *out = p;
    return p != NULL;
}

/**
 * @brief Attempts aligned allocation, returns success and pointer via out param
 *
 * @param arena     Valid arena
 * @param size      Bytes to allocate
 * @param alignment Power-of-2 alignment
 * @param out       Receives allocated pointer (NULL on failure)
 * @return true on success, false on failure
 *
 * Performance: O(1)
 */
static inline bool arena_try_alloc_aligned(Arena* arena, usize size, usize alignment, void** out) {
    void* p = arena_alloc_aligned(arena, size, alignment);
    if (out) *out = p;
    return p != NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
   Convenience typed allocation macros
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocates one object of Type from arena
 *
 * @param arena Arena to allocate from
 * @param Type  Type of object to allocate
 * @return Pointer to Type, or NULL on failure
 */
#define arena_alloc_type(arena, Type) \
    ((Type*)arena_alloc((arena), sizeof(Type)))

/**
 * @brief Allocates an array of count objects of Type from arena
 *
 * @param arena Arena to allocate from
 * @param Type  Element type
 * @param count Number of elements
 * @return Pointer to Type array, or NULL on failure
 *
 * @warning sizeof(Type) * count is NOT overflow-checked.
 * For untrusted count values, use checked_mul() before calling:
 * ```c
 * usize total;
 * if (!checked_mul(sizeof(Type), count, &total)) { handle_overflow(); }
 * Type* arr = arena_alloc(arena, total);
 * ```
 */
#define arena_alloc_array(arena, Type, count) \
    ((Type*)arena_alloc((arena), sizeof(Type) * (count)))

/**
 * @brief Allocates and zero-initializes one object of Type
 *
 * @param arena Arena to allocate from
 * @param Type  Type of object
 * @return Zeroed pointer to Type, or NULL on failure
 */
#define arena_alloc_type_zero(arena, Type) \
    ((Type*)arena_alloc_zero((arena), sizeof(Type)))

/**
 * @brief Allocates and zero-initializes an array of count objects of Type
 *
 * @param arena Arena to allocate from
 * @param Type  Element type
 * @param count Number of elements
 * @return Zeroed pointer to Type array, or NULL on failure
 *
 * @warning Same overflow caveat as arena_alloc_array.
 */
#define arena_alloc_array_zero(arena, Type, count) \
    ((Type*)arena_alloc_zero((arena), sizeof(Type) * (count)))

#endif /* CANON_CORE_ARENA_H */
