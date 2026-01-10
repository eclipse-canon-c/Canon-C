// core/memory.h
#ifndef CANON_CORE_MEMORY_H
#define CANON_CORE_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @file memory.h
 * @brief Low-level memory utilities with safety checks and alignment helpers
 *
 * Provides explicit, safe wrappers around standard memory functions (memcpy, memset, etc.)
 * plus alignment helpers for allocation and type-safe memory management.
 *
 * All functions:
 * - Are null-safe where reasonable
 * - Handle zero-size operations gracefully
 * - Have no hidden side effects
 * - Are suitable for use in arenas, pools, and other explicit allocators
 */

/* ============================================================
   Alignment utilities
   ============================================================ */

/**
 * @brief Rounds size up to the next multiple of max_align_t
 *
 * Useful for natural alignment of most types (safe against overflow).
 *
 * @param size Bytes to align (may be 0)
 * @return     Aligned size (multiple of sizeof(max_align_t))
 */
static inline size_t mem_align(size_t size) {
    const size_t align = sizeof(max_align_t);

    // Prevent overflow in addition
    if (size > SIZE_MAX - (align - 1u)) {
        return SIZE_MAX;
    }

    return (size + align - 1u) & ~(align - 1u);
}

/**
 * @brief Rounds size up to the next multiple of a specific power-of-two alignment
 *
 * @param size       Bytes to align (may be 0)
 * @param alignment  Required alignment (must be power of 2, e.g. 8, 16, 32)
 * @return           Aligned size or SIZE_MAX on overflow
 */
static inline size_t mem_align_to(size_t size, size_t alignment) {
    // Assume alignment is power of 2 (caller responsibility)
    if (size > SIZE_MAX - (alignment - 1u)) {
        return SIZE_MAX;
    }

    return (size + alignment - 1u) & ~(alignment - 1u);
}

/* ============================================================
   Safe memory operations
   ============================================================ */

/**
 * @brief Copies non-overlapping memory regions (wrapper around memcpy)
 *
 * Does nothing if dest/src is NULL or size == 0.
 *
 * @param dest Destination pointer
 * @param src  Source pointer
 * @param size Number of bytes to copy
 */
static inline void mem_copy(void* dest, const void* src, size_t size) {
    if (dest && src && size != 0) {
        memcpy(dest, src, size);
    }
}

/**
 * @brief Moves memory regions (handles overlap correctly)
 *
 * Wrapper around memmove — safe for overlapping regions.
 * Does nothing if dest/src is NULL or size == 0.
 */
static inline void mem_move(void* dest, const void* src, size_t size) {
    if (dest && src && size != 0) {
        memmove(dest, src, size);
    }
}

/**
 * @brief Zero-fills a memory region
 *
 * Wrapper around memset(ptr, 0, size).
 * Does nothing if ptr is NULL or size == 0.
 */
static inline void mem_zero(void* ptr, size_t size) {
    if (ptr && size != 0) {
        memset(ptr, 0, size);
    }
}

/**
 * @brief Fills a memory region with a specific byte value
 *
 * Wrapper around memset.
 * Does nothing if ptr is NULL or size == 0.
 *
 * @param ptr    Memory block to fill
 * @param value  Byte value to fill with (usually 0)
 * @param size   Number of bytes to fill
 */
static inline void mem_set(void* ptr, int value, size_t size) {
    if (ptr && size != 0) {
        memset(ptr, value, size);
    }
}

/**
 * @brief Compares two memory regions (like memcmp)
 *
 * Returns:
 *   - < 0 if a < b
 *   -   0 if equal
 *   - > 0 if a > b
 *
 * Safe: returns 0 if any pointer is NULL or size == 0.
 */
static inline int mem_compare(const void* a, const void* b, size_t size) {
    if (!a || !b || size == 0) {
        return 0;
    }
    return memcmp(a, b, size);
}

#endif /* CANON_CORE_MEMORY_H */
