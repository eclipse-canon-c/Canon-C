// core/arena.h
#ifndef CANON_CORE_ARENA_H
#define CANON_CORE_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "memory.h"

/**
 * @brief Linear (bump) allocator with explicit lifetime control
 *
 * Allocates sequentially from a caller-provided fixed-size buffer.
 * No individual deallocation is possible — only full reset or rollback to mark.
 * All allocations are permanent until arena_reset() or arena_reset_to().
 *
 * Key benefits:
 * - Extremely fast (just pointer bump + alignment padding)
 * - Zero hidden allocations
 * - Full control over memory lifetime
 * - Cache-friendly contiguous memory
 */
typedef struct Arena {
    uint8_t* buffer;   ///< Pointer to the start of the memory block
    size_t   capacity; ///< Total size of the buffer in bytes
    size_t   offset;   ///< Current allocation offset (next free position)
} Arena;

/**
 * @brief Initializes an arena with a caller-provided memory buffer
 *
 * @param arena     Must point to a valid Arena struct
 * @param buffer    Pointer to memory block (must remain valid for arena lifetime)
 * @param capacity  Size of the buffer in bytes (must be > 0)
 *
 * Preconditions:
 * - buffer != NULL
 * - capacity > 0
 * - buffer has at least 'capacity' usable bytes
 *
 * Postconditions:
 * - arena is ready for allocations
 * - offset is set to 0
 *
 * Ownership: Caller owns and must keep 'buffer' alive
 */
static inline void arena_init(Arena* arena, void* buffer, size_t capacity) {
    if (!arena || !buffer || capacity == 0) return;

    arena->buffer   = (uint8_t*)buffer;
    arena->capacity = capacity;
    arena->offset   = 0;
}

/**
 * @brief Allocates 'size' bytes from the arena (naturally aligned)
 *
 * @param arena Valid, initialized arena
 * @param size  Number of bytes to allocate (> 0)
 * @return      Pointer to allocated memory or NULL if:
 *              - size == 0
 *              - not enough remaining space
 */
static inline void* arena_alloc(Arena* arena, size_t size) {
    if (!arena || size == 0) return NULL;

    size = mem_align(size);  // Apply natural alignment (from memory.h)

    if (arena->offset + size > arena->capacity) {
        return NULL;
    }

    void* ptr = arena->buffer + arena->offset;
    arena->offset += size;
    return ptr;
}

/**
 * @brief Allocates 'size' bytes with explicit alignment requirement
 *
 * @param arena     Valid, initialized arena
 * @param size      Number of bytes to allocate (> 0)
 * @param alignment Required alignment (must be power of 2)
 * @return          Aligned pointer or NULL if allocation fails
 *
 * Note: Adds padding before the allocation if necessary
 */
static inline void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment) {
    if (!arena || size == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL; // invalid alignment (not power of 2)
    }

    uintptr_t current = (uintptr_t)(arena->buffer + arena->offset);
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t    padding = aligned - current;

    if (arena->offset + padding + size > arena->capacity) {
        return NULL;
    }

    arena->offset += padding;
    void* ptr = arena->buffer + arena->offset;
    arena->offset += size;
    return ptr;
}

/**
 * @brief Resets arena to initial empty state
 *
 * All previous allocations are invalidated.
 * Memory is **not** zeroed — only the offset is moved back.
 */
static inline void arena_reset(Arena* arena) {
    if (arena) {
        arena->offset = 0;
    }
}

/**
 * @brief Returns remaining free bytes in the arena
 */
static inline size_t arena_remaining(const Arena* arena) {
    return arena ? (arena->capacity - arena->offset) : 0;
}

/**
 * @brief Opaque type used for checkpoint/partial rollback
 */
typedef size_t ArenaMark;

/**
 * @brief Creates a checkpoint of current allocation position
 *
 * Useful for scoped temporary allocations (pair with arena_reset_to)
 */
static inline ArenaMark arena_mark(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Rolls back arena to a previous mark
 *
 * All allocations after the mark become invalid.
 *
 * @param mark Must be a valid previous mark (≤ current offset)
 */
static inline void arena_reset_to(Arena* arena, ArenaMark mark) {
    if (arena && mark <= arena->capacity) {
        arena->offset = mark;
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience typed allocation macros
   ──────────────────────────────────────────────────────────────────────────── */

/// Allocates one object of type Type
#define arena_alloc_type(arena, Type) \
    ((Type*)arena_alloc((arena), sizeof(Type)))

/// Allocates array of 'count' objects of type Type
#define arena_alloc_array(arena, Type, count) \
    ((Type*)arena_alloc((arena), sizeof(Type) * (count)))

/// Allocates and zero-initializes one object
#define arena_alloc_type_zero(arena, Type) \
    ({ Type* p = arena_alloc_type((arena), Type); if (p) memset(p, 0, sizeof(Type)); p; })

/// Allocates and zero-initializes array of 'count' objects
#define arena_alloc_array_zero(arena, Type, count) \
    ({ Type* p = arena_alloc_array((arena), Type, count); if (p) memset(p, 0, sizeof(Type)*(count)); p; })

#endif /* CANON_CORE_ARENA_H */
