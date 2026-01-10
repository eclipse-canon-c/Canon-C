// core/memory.h
#ifndef CANON_CORE_MEMORY_H
#define CANON_CORE_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

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
 * - Include overflow protection where applicable
 * - Validate preconditions via assertions in debug builds
 *
 * Portability:
 * - Requires C99 or later (for inline, stdint.h, stdbool.h)
 * - Uses standard library functions only (memcpy, memmove, memset, memcmp)
 * - No platform-specific code
 *
 * Thread-safety: All functions are thread-safe (no shared state)
 */

/* ============================================================
   Alignment utilities
   ============================================================ */

/**
 * @brief Rounds size up to the next multiple of max_align_t
 *
 * Useful for natural alignment of most types. Natural alignment is typically:
 * - 8 bytes on 64-bit systems
 * - 4 bytes on 32-bit systems
 *
 * Safe against overflow: returns SIZE_MAX if alignment would overflow.
 *
 * @param size Bytes to align (0 is valid, returns 0)
 * @return     Aligned size (multiple of sizeof(max_align_t)), or SIZE_MAX on overflow
 *
 * Example:
 *   size_t aligned = mem_align(37);  // Returns 40 on typical 64-bit system
 *
 * Performance: O(1), compiles to a few CPU instructions
 */
static inline size_t mem_align(size_t size) {
    const size_t align = sizeof(max_align_t);
    
    // Handle zero size explicitly
    if (size == 0) return 0;
    
    // Prevent overflow in addition
    if (size > SIZE_MAX - (align - 1u)) {
        return SIZE_MAX;
    }
    
    return (size + align - 1u) & ~(align - 1u);
}

/**
 * @brief Rounds size up to the next multiple of a specific power-of-two alignment
 *
 * @param size       Bytes to align (0 is valid, returns 0)
 * @param alignment  Required alignment (must be power of 2: 1, 2, 4, 8, 16, 32, ...)
 * @return           Aligned size or SIZE_MAX on overflow
 *
 * Preconditions:
 * - alignment must be a power of 2
 * - In debug builds, this is validated via assertion
 *
 * Example:
 *   size_t aligned = mem_align_to(100, 16);  // Returns 112
 *
 * Returns SIZE_MAX if:
 * - Alignment would cause overflow
 * - alignment is 0 (invalid)
 * - alignment is not power of 2 (invalid, but only checked in debug)
 */
static inline size_t mem_align_to(size_t size, size_t alignment) {
    assert(alignment > 0 && "mem_align_to: alignment must be greater than 0");
    assert((alignment & (alignment - 1u)) == 0 && "mem_align_to: alignment must be power of 2");
    
    // Handle invalid or zero alignment
    if (alignment == 0 || (alignment & (alignment - 1u)) != 0) {
        return SIZE_MAX;
    }
    
    // Handle zero size explicitly
    if (size == 0) return 0;
    
    // Prevent overflow in addition
    if (size > SIZE_MAX - (alignment - 1u)) {
        return SIZE_MAX;
    }
    
    return (size + alignment - 1u) & ~(alignment - 1u);
}

/**
 * @brief Checks if a pointer is aligned to a specific boundary
 *
 * @param ptr        Pointer to check (NULL returns false)
 * @param alignment  Required alignment (must be power of 2)
 * @return           true if ptr is aligned to 'alignment' bytes, false otherwise
 *
 * Example:
 *   void* p = malloc(100);
 *   if (mem_is_aligned(p, 16)) {
 *       // Can safely use 16-byte aligned SIMD operations
 *   }
 */
static inline bool mem_is_aligned(const void* ptr, size_t alignment) {
    assert(alignment > 0 && "mem_is_aligned: alignment must be greater than 0");
    assert((alignment & (alignment - 1u)) == 0 && "mem_is_aligned: alignment must be power of 2");
    
    if (!ptr || alignment == 0 || (alignment & (alignment - 1u)) != 0) {
        return false;
    }
    
    return ((uintptr_t)ptr & (alignment - 1u)) == 0;
}

/**
 * @brief Returns the alignment of a pointer (largest power of 2 that divides the address)
 *
 * @param ptr Pointer to check (NULL returns 0)
 * @return    Alignment in bytes (always a power of 2), or 0 if ptr is NULL
 *
 * Example:
 *   void* p = (void*)0x1000;
 *   size_t align = mem_get_alignment(p);  // Returns 4096 (0x1000)
 */
static inline size_t mem_get_alignment(const void* ptr) {
    if (!ptr) return 0;
    
    uintptr_t addr = (uintptr_t)ptr;
    if (addr == 0) return SIZE_MAX;  // Zero address is aligned to everything
    
    // Count trailing zeros to find alignment
    // This finds the largest power of 2 that divides addr
    return addr & (~addr + 1u);
}

/* ============================================================
   Safe memory operations
   ============================================================ */

/**
 * @brief Copies non-overlapping memory regions (wrapper around memcpy)
 *
 * Does nothing if dest/src is NULL or size == 0.
 *
 * @param dest Destination pointer (must not overlap with src)
 * @param src  Source pointer (must not overlap with dest)
 * @param size Number of bytes to copy
 *
 * Preconditions:
 * - dest and src must not overlap (undefined behavior if they do)
 * - In debug builds, asserts that dest != src when size > 0
 *
 * Note: For overlapping regions, use mem_move() instead.
 *
 * Performance: O(size), typically optimized to use fast platform-specific instructions
 */
static inline void mem_copy(void* dest, const void* src, size_t size) {
    if (!dest || !src || size == 0) return;
    
    assert(dest != src && "mem_copy: dest and src must not be the same pointer");
    
    memcpy(dest, src, size);
}

/**
 * @brief Moves memory regions (handles overlap correctly)
 *
 * Wrapper around memmove — safe for overlapping regions.
 * Does nothing if dest/src is NULL or size == 0.
 *
 * @param dest Destination pointer (may overlap with src)
 * @param src  Source pointer (may overlap with dest)
 * @param size Number of bytes to move
 *
 * Use this instead of mem_copy() when regions might overlap.
 * Slightly slower than mem_copy() but handles all cases correctly.
 *
 * Performance: O(size)
 */
static inline void mem_move(void* dest, const void* src, size_t size) {
    if (!dest || !src || size == 0) return;
    
    memmove(dest, src, size);
}

/**
 * @brief Zero-fills a memory region
 *
 * Wrapper around memset(ptr, 0, size).
 * Does nothing if ptr is NULL or size == 0.
 *
 * @param ptr  Memory block to zero
 * @param size Number of bytes to zero
 *
 * Common uses:
 * - Initialize structs: mem_zero(&my_struct, sizeof(my_struct))
 * - Clear arrays: mem_zero(array, sizeof(array))
 * - Sanitize sensitive data before freeing
 *
 * Note: For security-critical zeroing (preventing compiler optimization),
 *       use memset_s() or explicit_bzero() if available on your platform.
 *
 * Performance: O(size), highly optimized by compilers
 */
static inline void mem_zero(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    
    memset(ptr, 0, size);
}

/**
 * @brief Fills a memory region with a specific byte value
 *
 * Wrapper around memset.
 * Does nothing if ptr is NULL or size == 0.
 *
 * @param ptr    Memory block to fill
 * @param value  Byte value to fill with (0-255, values outside are masked)
 * @param size   Number of bytes to fill
 *
 * Example:
 *   mem_set(buffer, 0xFF, 100);  // Fill with 0xFF bytes
 *
 * Performance: O(size)
 */
static inline void mem_set(void* ptr, int value, size_t size) {
    if (!ptr || size == 0) return;
    
    memset(ptr, value, size);
}

/**
 * @brief Compares two memory regions (like memcmp)
 *
 * Returns:
 *   - < 0 if a < b (first differing byte in a is less than in b)
 *   -   0 if equal (all bytes match)
 *   - > 0 if a > b (first differing byte in a is greater than in b)
 *
 * Safe: returns 0 if any pointer is NULL or size == 0.
 *
 * @param a    First memory region
 * @param b    Second memory region
 * @param size Number of bytes to compare
 *
 * Note: Comparison is done byte-by-byte as unsigned char.
 *       Not constant-time (vulnerable to timing attacks for secret data).
 *
 * Performance: O(size), optimized by compilers
 */
static inline int mem_compare(const void* a, const void* b, size_t size) {
    if (!a || !b || size == 0) {
        return 0;
    }
    
    return memcmp(a, b, size);
}

/**
 * @brief Checks if two memory regions are equal
 *
 * More readable than mem_compare() == 0 for boolean checks.
 *
 * @param a    First memory region
 * @param b    Second memory region
 * @param size Number of bytes to compare
 * @return     true if all bytes match, false otherwise
 *
 * Returns true (equal) if:
 * - Both pointers are NULL
 * - size is 0
 * - All bytes match
 *
 * Example:
 *   if (mem_equal(buffer1, buffer2, 256)) {
 *       // Buffers are identical
 *   }
 */
static inline bool mem_equal(const void* a, const void* b, size_t size) {
    if (!a || !b) {
        return (a == b);  // Both NULL = equal, one NULL = not equal
    }
    
    if (size == 0) return true;
    
    return memcmp(a, b, size) == 0;
}

/**
 * @brief Checks if a memory region contains only a specific byte value
 *
 * Useful for verifying zero-initialization or detecting patterns.
 *
 * @param ptr   Memory region to check (NULL returns true)
 * @param value Byte value to check for (0-255)
 * @param size  Number of bytes to check
 * @return      true if all bytes equal 'value', false otherwise
 *
 * Example:
 *   if (mem_is_all(buffer, 0, 1024)) {
 *       // Buffer is fully zeroed
 *   }
 *
 * Performance: O(size)
 */
static inline bool mem_is_all(const void* ptr, int value, size_t size) {
    if (!ptr || size == 0) return true;
    
    const unsigned char* bytes = (const unsigned char*)ptr;
    const unsigned char byte_val = (unsigned char)value;
    
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != byte_val) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Checks if a memory region is all zeros
 *
 * Convenience wrapper around mem_is_all(ptr, 0, size).
 *
 * @param ptr  Memory region to check (NULL returns true)
 * @param size Number of bytes to check
 * @return     true if all bytes are zero, false otherwise
 */
static inline bool mem_is_zero(const void* ptr, size_t size) {
    return mem_is_all(ptr, 0, size);
}

/* ============================================================
   Type-safe memory operations (macros)
   ============================================================ */

/**
 * @brief Zero-initializes a single object of any type
 *
 * Example: mem_zero_type(&my_struct);
 */
#define mem_zero_type(ptr) \
    mem_zero((ptr), sizeof(*(ptr)))

/**
 * @brief Zero-initializes an array
 *
 * Example: int arr[100]; mem_zero_array(arr);
 */
#define mem_zero_array(array) \
    mem_zero((array), sizeof(array))

/**
 * @brief Copies one object to another (same type)
 *
 * Example: mem_copy_type(&dest, &src);
 */
#define mem_copy_type(dest, src) \
    mem_copy((dest), (src), sizeof(*(dest)))

/**
 * @brief Compares two objects of the same type
 *
 * Example: if (mem_compare_type(&a, &b) == 0) { ... }
 */
#define mem_compare_type(a, b) \
    mem_compare((a), (b), sizeof(*(a)))

/**
 * @brief Checks if two objects of the same type are equal
 *
 * Example: if (mem_equal_type(&a, &b)) { ... }
 */
#define mem_equal_type(a, b) \
    mem_equal((a), (b), sizeof(*(a)))

/* ────────────────────────────────────────────────────────────────────────────
   Usage Examples (not compiled, for documentation only)
   ────────────────────────────────────────────────────────────────────────────

    #include "memory.h"
    
    void example(void) {
        // Alignment helpers
        size_t natural = mem_align(37);      // 40 on 64-bit
        size_t custom  = mem_align_to(37, 16); // 48
        
        // Pointer alignment checks
        void* ptr = malloc(100);
        if (mem_is_aligned(ptr, 16)) {
            // Can use SIMD operations
        }
        
        // Safe memory operations
        char buffer[256];
        mem_zero(buffer, sizeof(buffer));
        
        struct Data { int x, y; } a, b;
        mem_zero_type(&a);
        mem_copy_type(&b, &a);
        
        if (mem_equal_type(&a, &b)) {
            // Objects are identical
        }
        
        // Verification
        if (mem_is_zero(buffer, sizeof(buffer))) {
            // Buffer is fully zeroed
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_MEMORY_H */
