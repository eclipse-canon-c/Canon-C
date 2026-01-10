// core/arena.h
#ifndef CANON_CORE_ARENA_H
#define CANON_CORE_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
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
 *
 * Alignment behavior:
 * - arena_alloc() uses natural alignment via mem_align()
 *   (typically 8 bytes on 64-bit, 4 bytes on 32-bit)
 * - arena_alloc_aligned() supports custom alignment requirements
 *
 * Portability notes:
 * - Requires C99 or later for inline functions and stdint.h
 * - Statement expression macros (*_zero) require GNU C extension
 *   Use CANON_NO_GNU_EXTENSIONS to disable them
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
 * - arena != NULL
 * - buffer != NULL
 * - capacity > 0
 * - buffer has at least 'capacity' usable bytes
 *
 * Postconditions:
 * - arena is ready for allocations
 * - offset is set to 0
 *
 * Ownership: Caller owns and must keep 'buffer' alive
 *
 * Note: In debug builds (CANON_DEBUG), assertions enforce preconditions.
 *       In release builds, invalid parameters result in uninitialized arena.
 */
static inline void arena_init(Arena* arena, void* buffer, size_t capacity) {
    assert(arena != NULL && "arena_init: arena parameter cannot be NULL");
    assert(buffer != NULL && "arena_init: buffer parameter cannot be NULL");
    assert(capacity > 0 && "arena_init: capacity must be greater than 0");
    
    if (!arena || !buffer || capacity == 0) return;

    arena->buffer   = (uint8_t*)buffer;
    arena->capacity = capacity;
    arena->offset   = 0;
}

/**
 * @brief Allocates 'size' bytes from the arena with natural alignment
 *
 * Natural alignment is applied via mem_align() which typically aligns to:
 * - 8 bytes on 64-bit systems
 * - 4 bytes on 32-bit systems
 *
 * @param arena Valid, initialized arena
 * @param size  Number of bytes to allocate (> 0)
 * @return      Pointer to allocated memory or NULL if:
 *              - arena is NULL
 *              - size == 0
 *              - not enough remaining space
 *
 * Alignment: Automatically applies natural alignment padding
 * Thread-safety: Not thread-safe (caller must synchronize if needed)
 */
static inline void* arena_alloc(Arena* arena, size_t size) {
    assert(arena != NULL && "arena_alloc: arena parameter cannot be NULL");
    
    if (!arena || size == 0) return NULL;

    size_t aligned_size = mem_align(size);
    
    // Check for overflow and capacity
    if (aligned_size < size || arena->offset + aligned_size > arena->capacity) {
        return NULL;
    }

    void* ptr = arena->buffer + arena->offset;
    arena->offset += aligned_size;
    return ptr;
}

/**
 * @brief Allocates 'size' bytes with explicit alignment requirement
 *
 * @param arena     Valid, initialized arena
 * @param size      Number of bytes to allocate (> 0)
 * @param alignment Required alignment (must be power of 2, e.g., 1, 2, 4, 8, 16, ...)
 * @return          Aligned pointer or NULL if allocation fails
 *
 * Adds padding before the allocation if necessary to meet alignment requirement.
 *
 * Example:
 *   void* ptr = arena_alloc_aligned(&arena, 100, 16); // 16-byte aligned
 *
 * Returns NULL if:
 * - arena is NULL
 * - size is 0
 * - alignment is not a power of 2
 * - insufficient space (including padding)
 */
static inline void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment) {
    assert(arena != NULL && "arena_alloc_aligned: arena parameter cannot be NULL");
    assert((alignment & (alignment - 1)) == 0 && "arena_alloc_aligned: alignment must be power of 2");
    
    if (!arena || size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL; // invalid alignment (not power of 2)
    }

    uintptr_t current = (uintptr_t)(arena->buffer + arena->offset);
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t    padding = (size_t)(aligned - current);

    // Check for overflow and capacity
    if (arena->offset + padding < arena->offset || 
        arena->offset + padding + size < arena->offset + padding ||
        arena->offset + padding + size > arena->capacity) {
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
 *
 * For security-sensitive applications, use arena_reset_secure() instead.
 *
 * @param arena Arena to reset (NULL-safe)
 */
static inline void arena_reset(Arena* arena) {
    if (arena) {
        arena->offset = 0;
    }
}

/**
 * @brief Resets arena and zeros all previously allocated memory
 *
 * Use this when deallocating memory that may have contained sensitive data
 * (passwords, cryptographic keys, personal information, etc.)
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * Performance: O(n) where n is the number of allocated bytes
 */
static inline void arena_reset_secure(Arena* arena) {
    if (arena && arena->offset > 0) {
        memset(arena->buffer, 0, arena->offset);
        arena->offset = 0;
    }
}

/**
 * @brief Returns remaining free bytes in the arena
 *
 * @param arena Arena to query (NULL returns 0)
 * @return Number of bytes available for allocation
 */
static inline size_t arena_remaining(const Arena* arena) {
    return arena ? (arena->capacity - arena->offset) : 0;
}

/**
 * @brief Returns number of bytes currently allocated
 *
 * @param arena Arena to query (NULL returns 0)
 * @return Number of bytes currently in use
 */
static inline size_t arena_used(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Opaque type used for checkpoint/partial rollback
 *
 * Represents a snapshot of the arena's allocation state at a point in time.
 * Use arena_mark() to create a checkpoint and arena_reset_to() to restore.
 */
typedef size_t ArenaMark;

/**
 * @brief Creates a checkpoint of current allocation position
 *
 * Useful for scoped temporary allocations. Pair with arena_reset_to().
 *
 * Example:
 *   ArenaMark mark = arena_mark(&arena);
 *   // ... do temporary allocations ...
 *   arena_reset_to(&arena, mark);  // Free temporary allocations
 *
 * @param arena Arena to mark (NULL returns 0)
 * @return Opaque mark value representing current position
 */
static inline ArenaMark arena_mark(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Rolls back arena to a previous mark
 *
 * All allocations after the mark become invalid and their memory is reclaimed.
 * The mark must have been obtained from arena_mark() on the same arena.
 *
 * @param arena Arena to reset (NULL-safe)
 * @param mark  Must be a valid previous mark (≤ current offset)
 *
 * Safety: In debug builds, asserts that mark ≤ current offset.
 *         In release builds, silently ignores invalid marks.
 *
 * Example:
 *   ArenaMark mark = arena_mark(&arena);
 *   void* temp = arena_alloc(&arena, 1024);
 *   // ... use temp ...
 *   arena_reset_to(&arena, mark);  // temp is now invalid
 */
static inline void arena_reset_to(Arena* arena, ArenaMark mark) {
    if (!arena) return;
    
    assert(mark <= arena->offset && "arena_reset_to: mark is beyond current offset");
    
    // Only reset if mark is valid (≤ current offset)
    if (mark <= arena->offset) {
        arena->offset = mark;
    }
}

/**
 * @brief Checks if arena is empty (no allocations)
 *
 * @param arena Arena to check (NULL returns true)
 * @return true if no bytes are allocated, false otherwise
 */
static inline int arena_is_empty(const Arena* arena) {
    return !arena || arena->offset == 0;
}

/**
 * @brief Checks if arena is full (no space remaining)
 *
 * @param arena Arena to check (NULL returns true)
 * @return true if no bytes remain, false otherwise
 */
static inline int arena_is_full(const Arena* arena) {
    return !arena || arena->offset >= arena->capacity;
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience typed allocation macros
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates one object of type Type
 *
 * Example: MyStruct* s = arena_alloc_type(&arena, MyStruct);
 */
#define arena_alloc_type(arena, Type) \
    ((Type*)arena_alloc((arena), sizeof(Type)))

/**
 * @brief Allocates array of 'count' objects of type Type
 *
 * Example: int* arr = arena_alloc_array(&arena, int, 100);
 */
#define arena_alloc_array(arena, Type, count) \
    ((Type*)arena_alloc((arena), sizeof(Type) * (count)))

/* GNU C extensions for zero-initialization (can be disabled) */
#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Allocates and zero-initializes one object
 *
 * Requires: GNU C statement expression extension
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Example: MyStruct* s = arena_alloc_type_zero(&arena, MyStruct);
 */
#define arena_alloc_type_zero(arena, Type) \
    ({ \
        Type* _p = arena_alloc_type((arena), Type); \
        if (_p) memset(_p, 0, sizeof(Type)); \
        _p; \
    })

/**
 * @brief Allocates and zero-initializes array of 'count' objects
 *
 * Requires: GNU C statement expression extension
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Example: int* arr = arena_alloc_array_zero(&arena, int, 100);
 */
#define arena_alloc_array_zero(arena, Type, count) \
    ({ \
        Type* _p = arena_alloc_array((arena), Type, count); \
        if (_p) memset(_p, 0, sizeof(Type) * (count)); \
        _p; \
    })

#else /* CANON_NO_GNU_EXTENSIONS */

/* Standard C fallback - requires expression can be evaluated twice */

/**
 * @brief Allocates and zero-initializes one object (Standard C version)
 *
 * Warning: Evaluates 'arena' parameter twice. Do not use with side effects.
 */
#define arena_alloc_type_zero(arena, Type) \
    ((Type*)memset(arena_alloc_type((arena), Type), 0, sizeof(Type)))

/**
 * @brief Allocates and zero-initializes array (Standard C version)
 *
 * Warning: Evaluates 'arena' and 'count' parameters twice. 
 *          Do not use with side effects.
 */
#define arena_alloc_array_zero(arena, Type, count) \
    ((Type*)memset(arena_alloc_array((arena), Type, count), 0, sizeof(Type) * (count)))

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ────────────────────────────────────────────────────────────────────────────
   Usage Example (not compiled, for documentation only)
   ────────────────────────────────────────────────────────────────────────────

    #include "arena.h"
    
    void example(void) {
        // Stack-allocated buffer
        uint8_t buffer[4096];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        // Basic allocation
        int* nums = arena_alloc_array(&arena, int, 100);
        
        // Scoped temporary allocations
        ArenaMark mark = arena_mark(&arena);
        char* temp = arena_alloc_array(&arena, char, 256);
        // ... use temp ...
        arena_reset_to(&arena, mark);  // temp is now invalid
        
        // Zero-initialized allocation
        typedef struct { int x, y; } Point;
        Point* p = arena_alloc_type_zero(&arena, Point);
        
        // Full reset when done
        arena_reset(&arena);
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_ARENA_H */
