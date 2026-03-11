#ifndef CANON_CORE_ARENA_H
#define CANON_CORE_ARENA_H
#include <stdint.h> // uintptr_t (via ptr.h, explicit for clarity)
#include "core/primitives/types.h" // u8, usize
#include "core/primitives/limits.h" // CANON_ARENA_MAX_SIZE, CANON_USIZE_MAX, CANON_MAX_ALIGN
#include "core/primitives/contract.h" // require_msg, ensure_msg
#include "core/primitives/ptr.h" // ptr_offset, ptr_align_up, ptr_span
#include "core/primitives/bits.h" // is_power_of_two()
#include "core/memory.h" // mem_zero, mem_secure_zero
#include "core/slice.h" // bytes_t, bytes_from, bytes_empty
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
 * Debug instrumentation (optional):
 * ────────────────────────────────────────────────────────────────────────────
 * Define CANON_ARENA_DEBUG before including this header to enable
 * additional tracking fields in the Arena struct:
 *
 * #define CANON_ARENA_DEBUG
 * #include "core/arena.h"
 *
 * When enabled:
 * - alloc_count: total number of successful arena_alloc() calls
 * - peak: high watermark of bytes used (maximum offset ever reached)
 *
 * Both fields are zero-initialized by arena_init() and reset by arena_reset().
 * Zero cost in release builds — struct layout is identical without the flag.
 * Use arena_stats() to retrieve a snapshot of debug state.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) for alloc, O(n) for secure reset
 * - Space complexity: O(1) metadata (~24 bytes per arena, ~40 with debug)
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
  *
  * When CANON_ARENA_DEBUG is defined, two additional fields are present:
  * - alloc_count: number of successful allocations since last reset
  * - peak: maximum bytes used since last reset (high watermark)
  */
 typedef struct Arena {
     u8* buffer; ///< Start of the memory block (caller-owned)
     usize capacity; ///< Total usable bytes in buffer
     usize offset; ///< Current bump pointer offset from buffer start
 #ifdef CANON_ARENA_DEBUG
     usize alloc_count; ///< [debug] Number of successful arena_alloc() calls
     usize peak; ///< [debug] High watermark: maximum offset ever reached
 #endif
 } Arena;
 /**
  * @brief Opaque checkpoint type for partial rollback
  *
  * Returned by arena_mark(), consumed by arena_reset_to().
  * Treat as opaque — do not rely on its internal representation.
  */
 typedef usize ArenaMark;
 /* ════════════════════════════════════════════════════════════════════════════
    Debug stats snapshot (only available with CANON_ARENA_DEBUG)
    ════════════════════════════════════════════════════════════════════════════ */
 #ifdef CANON_ARENA_DEBUG
 /**
  * @brief Snapshot of arena debug state
  *
  * Returned by arena_stats(). All values reflect state since last reset.
  */
 typedef struct {
     usize used; ///< Bytes currently allocated
     usize remaining; ///< Bytes still available
     usize capacity; ///< Total arena capacity
     usize peak; ///< Maximum bytes ever used (high watermark)
     usize alloc_count; ///< Total number of successful allocations
 } ArenaStats;
 /**
  * @brief Returns a snapshot of current arena debug state
  *
  * Only available when CANON_ARENA_DEBUG is defined.
  *
  * @param arena Valid initialized arena (NULL-safe — returns zeroed stats)
  * @return ArenaStats snapshot
  */
 static inline ArenaStats arena_stats(const Arena* arena) {
     if (!arena) {
         ArenaStats s = {0};
         return s;
     }
     ArenaStats s;
     s.used = arena->offset;
     s.remaining = arena->capacity - arena->offset;
     s.capacity = arena->capacity;
     s.peak = arena->peak;
     s.alloc_count = arena->alloc_count;
     return s;
 }
 #endif /* CANON_ARENA_DEBUG */
 /* ════════════════════════════════════════════════════════════════════════════
    Internal: debug field update helper (compiled away in release)
    ════════════════════════════════════════════════════════════════════════════ */
 #ifdef CANON_ARENA_DEBUG
     #define _arena_debug_update(arena) \
         do { \
             (arena)->alloc_count += 1; \
             if ((arena)->offset > (arena)->peak) \
                 (arena)->peak = (arena)->offset; \
         } while (0)
     #define _arena_debug_reset(arena) \
         do { \
             (arena)->alloc_count = 0; \
             (arena)->peak = 0; \
         } while (0)
 #else
     #define _arena_debug_update(arena) ((void)0)
     #define _arena_debug_reset(arena) ((void)0)
 #endif
 /* ════════════════════════════════════════════════════════════════════════════
    Initialization & state management
    ════════════════════════════════════════════════════════════════════════════ */
 /**
  * @brief Initializes an arena using caller-provided memory
  *
  * @param arena Valid pointer to uninitialized Arena struct
  * @param buffer Pointer to memory block (must remain valid for arena lifetime)
  * @param capacity Size of buffer in bytes (> 0)
  *
  * @pre arena != NULL && buffer != NULL && capacity > 0
  * @pre capacity <= CANON_ARENA_MAX_SIZE
  *
  * @post arena->buffer == buffer
  * @post arena->capacity == capacity
  * @post arena->offset == 0
  * @post [debug] arena->alloc_count == 0, arena->peak == 0
  *
  * Performance: O(1)
  */
 static inline void arena_init(Arena* arena, void* buffer, usize capacity) {
     require_msg(arena != NULL, "arena_init: arena cannot be NULL");
     require_msg(buffer != NULL, "arena_init: buffer cannot be NULL");
     require_msg(capacity > 0, "arena_init: capacity must be > 0");
     require_msg(capacity <= CANON_ARENA_MAX_SIZE,
                 "arena_init: capacity exceeds CANON_ARENA_MAX_SIZE");

     arena->buffer = (u8*)buffer;
     arena->capacity = capacity;
     arena->offset = 0;
     _arena_debug_reset(arena);
 }
 /**
  * @brief Resets arena to empty state (fast path)
  *
  * Invalidates all previous allocations by moving offset back to 0.
  * Memory content is NOT cleared — use arena_reset_secure() for sensitive data.
  *
  * @note [debug] Resets alloc_count and peak to 0.
  *
  * @param arena Arena to reset (NULL-safe)
  *
  * Performance: O(1)
  */
 static inline void arena_reset(Arena* arena) {
     if (!arena) return;
     arena->offset = 0;
     _arena_debug_reset(arena);
 }
 /**
  * @brief Resets arena and securely wipes all previously allocated memory
  *
  * Use when arena contained sensitive data (keys, passwords, tokens, etc.).
  * Slower than arena_reset() — O(used bytes).
  *
  * @note [debug] Resets alloc_count and peak to 0.
  *
  * @param arena Arena to reset (NULL-safe)
  *
  * Performance: O(used bytes)
  */
 static inline void arena_reset_secure(Arena* arena) {
     if (arena && arena->offset > 0) {
         mem_secure_zero(arena->buffer, arena->offset);
         arena->offset = 0;
         _arena_debug_reset(arena);
     }
 }
 /* ════════════════════════════════════════════════════════════════════════════
    Allocation
    ════════════════════════════════════════════════════════════════════════════ */
 /**
  * @brief Allocates size bytes with natural alignment
  *
  * Natural alignment uses CANON_MAX_ALIGN (platform default, usually 8 or 16 bytes).
  *
  * Uses ptr_align_up() and ptr_span() from core/primitives/ptr.h for all
  * pointer arithmetic — no raw uintptr_t casts in the function body.
  *
  * @note [debug] On success, increments alloc_count and updates peak.
  *
  * @param arena Valid initialized arena
  * @param size Bytes to allocate (> 0)
  *
  * @return Pointer to aligned memory block, or NULL on failure
  *
  * Performance: O(1)
  */
 static inline void* arena_alloc(Arena* arena, usize size) {
     require_msg(arena != NULL, "arena_alloc: arena cannot be NULL");
     if (size == 0) return NULL;

     void* current = ptr_offset(arena->buffer, arena->offset);
     void* aligned_ptr = ptr_align_up(current, CANON_MAX_ALIGN);
     usize pad = ptr_span(aligned_ptr, current);
     if (arena->offset > CANON_USIZE_MAX - pad ||
         arena->offset + pad > CANON_USIZE_MAX - size ||
         arena->offset + pad + size > arena->capacity) {
         return NULL;
     }
     arena->offset += pad;
     void* result = ptr_offset(arena->buffer, arena->offset);
     arena->offset += size;
     _arena_debug_update(arena);
     return result;
 }
 /**
  * @brief Allocates size bytes with a specific power-of-2 alignment
  *
  * @note [debug] On success, increments alloc_count and updates peak.
  *
  * @param arena Valid initialized arena
  * @param size Bytes to allocate (> 0)
  * @param alignment Power-of-2 alignment requirement (1, 2, 4, 8, 16, ...)
  *
  * @return Aligned pointer or NULL if:
  * - arena == NULL, size == 0, or alignment not power of 2
  * - insufficient space (including alignment padding)
  *
  * @pre is_power_of_two(alignment)
  *
  * Performance: O(1)
  */
 static inline void* arena_alloc_aligned(Arena* arena, usize size, usize alignment) {
     require_msg(arena != NULL, "arena_alloc_aligned: arena cannot be NULL");
     require_msg(is_power_of_two(alignment),
                 "arena_alloc_aligned: alignment must be a power of 2");

     if (size == 0) return NULL;

     void* current = ptr_offset(arena->buffer, arena->offset);
     void* aligned_ptr = ptr_align_up(current, alignment);
     usize pad = ptr_span(aligned_ptr, current);
     if (arena->offset > CANON_USIZE_MAX - pad ||
         arena->offset + pad > CANON_USIZE_MAX - size ||
         arena->offset + pad + size > arena->capacity) {
         return NULL;
     }
     arena->offset += pad;
     void* result = ptr_offset(arena->buffer, arena->offset);
     arena->offset += size;
     _arena_debug_update(arena);
     return result;
 }
 /* ════════════════════════════════════════════════════════════════════════════
    Zero-initializing variants
    ════════════════════════════════════════════════════════════════════════════ */
 /**
  * @brief Allocates size bytes and zero-initializes them
  *
  * Performance: O(size)
  */
 static inline void* arena_alloc_zero(Arena* arena, usize size) {
     void* p = arena_alloc(arena, size);
     if (p) mem_zero(p, size);
     return p;
 }
 /**
  * @brief Allocates aligned size bytes and zero-initializes them
  *
  * Performance: O(size)
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
  * @brief Returns total capacity of the arena in bytes (NULL-safe)
  */
 static inline usize arena_capacity(const Arena* arena) {
     return arena ? arena->capacity : 0;
 }
 /**
  * @brief Returns number of bytes still available for allocation (NULL-safe)
  */
 static inline usize arena_remaining(const Arena* arena) {
     return arena ? (arena->capacity - arena->offset) : 0;
 }
 /**
  * @brief Returns number of bytes currently allocated (NULL-safe)
  */
 static inline usize arena_used(const Arena* arena) {
     return arena ? arena->offset : 0;
 }
 /**
  * @brief Returns true if the arena has no active allocations (NULL-safe)
  */
 static inline bool arena_is_empty(const Arena* arena) {
     return !arena || arena->offset == 0;
 }
 /**
  * @brief Returns true if the arena has no remaining space (NULL-safe)
  */
 static inline bool arena_is_full(const Arena* arena) {
     return !arena || arena->offset >= arena->capacity;
 }
 /**
  * @brief Creates a checkpoint of the current allocation position
  *
  * Pair with arena_reset_to() for scoped temporary allocations.
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
  * @note [debug] alloc_count and peak are NOT rolled back — they reflect
  * cumulative activity since last full reset, not since the mark.
  *
  * @pre mark <= arena->offset
  */
 static inline void arena_reset_to(Arena* arena, ArenaMark mark) {
     if (!arena) return;
     require_msg(mark <= arena->offset,
                 "arena_reset_to: mark is ahead of current offset");
     arena->offset = mark;
 }
 /* ════════════════════════════════════════════════════════════════════════════
    Byte views — slice.h integration
    ════════════════════════════════════════════════════════════════════════════ */
 /**
  * @brief Returns a bytes_t view over the currently allocated region
  *
  * Covers [buffer, buffer + offset) — all bytes allocated so far.
  * Non-owning — becomes invalid after any reset.
  */
 static inline bytes_t arena_as_bytes(const Arena* arena) {
     require_msg(arena != NULL, "arena_as_bytes: arena cannot be NULL");
     return bytes_from(arena->buffer, arena->offset);
 }
 /**
  * @brief Returns a bytes_t view over the entire arena buffer (used + free)
  *
  * Covers [buffer, buffer + capacity). Use arena_as_bytes() for used only.
  */
 static inline bytes_t arena_buffer_bytes(const Arena* arena) {
     require_msg(arena != NULL, "arena_buffer_bytes: arena cannot be NULL");
     return bytes_from(arena->buffer, arena->capacity);
 }
 /**
  * @brief Returns a bytes_t view over the remaining (unallocated) region
  *
  * Covers [buffer + offset, buffer + capacity).
  * Useful for passing free space to I/O operations.
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
 /* ════════════════════════════════════════════════════════════════════════════
    Convenience typed allocation macros
    ════════════════════════════════════════════════════════════════════════════ */
 /** @brief Allocates one object of Type from arena */
 #define arena_alloc_type(arena, Type) \
     ((Type*)arena_alloc((arena), sizeof(Type)))
 /**
  * @brief Allocates an array of count objects of Type from arena
  *
  * @warning sizeof(Type) * count is NOT overflow-checked.
  * For untrusted count values, use checked_mul() first:
  * ```c
  * usize total;
  * if (!checked_mul(sizeof(Type), count, &total)) { /* handle overflow */ }
  * Type* arr = arena_alloc(arena, total);
  * ```
  */
 #define arena_alloc_array(arena, Type, count) \
     ((Type*)arena_alloc((arena), sizeof(Type) * (count)))
 /** @brief Allocates and zero-initializes one object of Type */
 #define arena_alloc_type_zero(arena, Type) \
     ((Type*)arena_alloc_zero((arena), sizeof(Type)))
 /**
  * @brief Allocates and zero-initializes an array of count objects of Type
  *
  * Same overflow warning as arena_alloc_array.
  */
 #define arena_alloc_array_zero(arena, Type, count) \
     ((Type*)arena_alloc_zero((arena), sizeof(Type) * (count)))
 #endif /* CANON_CORE_ARENA_H */
