#ifndef CANON_CORE_MEMORY_H
#define CANON_CORE_MEMORY_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include <string.h>

/**
 * @file memory.h
 * @brief Safe, explicit, low-level memory manipulation and alignment utilities
 *
 * Provides thin, safety-checked wrappers around standard memory functions
 * (memcpy, memmove, memset, memcmp) and powerful alignment helpers.
 * Designed for use in custom allocators (arenas, pools, slab), parsers,
 * and performance-critical code where explicit control is required.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Null-safe and zero-size safe on all operations
 * - Overflow-protected alignment calculations
 * - Explicit over implicit — preconditions checked via contract.h
 * - No hidden allocations, no thread-local state
 * - Type-safe convenience macros reduce sizeof boilerplate
 * - Portable — relies only on C99 standard library + Canon-C primitives
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions: O(1) for alignment helpers, O(n) for memory ops
 * - Alignment helpers compile to 2-5 instructions on most platforms
 * - Wrappers usually inlined and optimized identically to direct stdlib calls
 * - mem_is_all / mem_is_zero: linear scan
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are fully thread-safe — no shared mutable state.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline functions)
 * - Uses only: memcpy, memmove, memset, memcmp, uintptr_t
 * - No platform-specific intrinsics or assembly
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * memory.h sits at the bottom of core/. It depends only on primitives/.
 * No other core module may be included here.
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Constant-time comparison of secrets (use dedicated crypto function)
 * - Very large regions where SIMD-optimized routines exist
 * - Replacing compiler builtins in hot loops
 *
 * @sa mem_align(), mem_align_to(), mem_zero(), mem_copy(), mem_move()
 */

/* ────────────────────────────────────────────────────────────────────────────
   mem_swap fixed buffer limit
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Maximum size (bytes) supported by mem_swap
 *
 * mem_swap uses a fixed-size stack buffer to avoid VLAs (which are
 * optional in C11+). Swapping regions larger than this requires
 * caller-managed scratch memory.
 *
 * Override by defining CANON_MEM_SWAP_MAX before including this header.
 *
 * @remark Default: 256 bytes — covers most struct swap use cases
 */
#ifndef CANON_MEM_SWAP_MAX
    #define CANON_MEM_SWAP_MAX ((usize)256)
#endif

/* ────────────────────────────────────────────────────────────────────────────
   Alignment utilities
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Rounds size up to the next multiple of max_align_t
 *
 * Natural alignment for the current platform:
 * - Usually 8 bytes on 64-bit systems
 * - Usually 4 bytes on 32-bit systems
 *
 * @param size Bytes to align (0 allowed → returns 0)
 * @return Smallest multiple of sizeof(max_align_t) >= size,
 *         or CANON_USIZE_MAX if overflow would occur
 *
 * @sa mem_align_to()
 */
static inline usize mem_align(usize size) {
    const usize align = sizeof(max_align_t);
    if (size == 0) return 0;
    if (size > CANON_USIZE_MAX - (align - 1u)) {
        return CANON_USIZE_MAX;
    }
    return (size + align - 1u) & ~(align - 1u);
}

/**
 * @brief Rounds size up to the next multiple of a power-of-2 alignment
 *
 * @param size      Bytes to align (0 allowed → returns 0)
 * @param alignment Power-of-2 alignment value (1, 2, 4, 8, 16, ...)
 *
 * @return Smallest multiple of alignment >= size,
 *         or CANON_USIZE_MAX on overflow or invalid alignment
 *
 * @pre alignment > 0 and is a power of 2
 *
 * @sa mem_align(), mem_is_aligned()
 */
static inline usize mem_align_to(usize size, usize alignment) {
    require_msg(alignment > 0,
        "mem_align_to: alignment must be > 0");
    require_msg((alignment & (alignment - 1u)) == 0,
        "mem_align_to: alignment must be a power of 2");
    if (alignment == 0 || (alignment & (alignment - 1u)) != 0) {
        return CANON_USIZE_MAX;
    }
    if (size == 0) return 0;
    if (size > CANON_USIZE_MAX - (alignment - 1u)) {
        return CANON_USIZE_MAX;
    }
    return (size + alignment - 1u) & ~(alignment - 1u);
}

/**
 * @brief Checks whether a pointer satisfies a given alignment requirement
 *
 * @param ptr       Pointer to test (NULL → false)
 * @param alignment Power-of-2 alignment to check (1, 2, 4, 8, ...)
 *
 * @return true if ptr is aligned to alignment bytes, false otherwise
 *
 * @pre alignment is a power of 2
 *
 * @sa mem_get_alignment()
 */
static inline bool mem_is_aligned(const void* ptr, usize alignment) {
    require_msg(alignment > 0,
        "mem_is_aligned: alignment must be > 0");
    require_msg((alignment & (alignment - 1u)) == 0,
        "mem_is_aligned: alignment must be a power of 2");
    if (!ptr || alignment == 0 || (alignment & (alignment - 1u)) != 0) {
        return false;
    }
    return ((uintptr_t)ptr & (alignment - 1u)) == 0;
}

/**
 * @brief Determines the current alignment of a pointer
 *
 * Returns the largest power of 2 that divides the pointer address.
 *
 * @param ptr Pointer to inspect (NULL → 0)
 * @return Alignment in bytes (power of 2), or 0 if ptr is NULL
 */
static inline usize mem_get_alignment(const void* ptr) {
    if (!ptr) return 0;
    uintptr_t addr = (uintptr_t)ptr;
    if (addr == 0) return CANON_USIZE_MAX;
    return (usize)(addr & (~addr + 1u));
}

/**
 * @brief Checks if a size is a power of two
 *
 * @param n Value to test
 * @return true if n > 0 and is a power of 2, false otherwise
 */
static inline bool mem_is_power_of_two(usize n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Safe memory operations
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Copies non-overlapping memory regions (safe memcpy wrapper)
 *
 * @param dest Destination memory (NULL → no-op)
 * @param src  Source memory (NULL → no-op)
 * @param size Number of bytes to copy (0 → no-op)
 *
 * @pre If size > 0, dest and src must not overlap (checked in debug builds)
 * @note For overlapping regions use mem_move() instead.
 *
 * @sa mem_move(), mem_copy_type()
 */
static inline void mem_copy(void* restrict dest, const void* restrict src, usize size) {
    if (!dest || !src || size == 0) return;
    ensure_msg(dest != src,
        "mem_copy: overlapping regions — use mem_move instead");
    memcpy(dest, src, size);
}

/**
 * @brief Moves memory, correctly handling overlapping regions
 *
 * @param dest Destination memory (may overlap src)
 * @param src  Source memory
 * @param size Number of bytes to move (0 → no-op)
 *
 * @sa mem_copy()
 */
static inline void mem_move(void* dest, const void* src, usize size) {
    if (!dest || !src || size == 0) return;
    memmove(dest, src, size);
}

/**
 * @brief Zero-fills a memory region
 *
 * @param ptr  Memory to clear (NULL → no-op)
 * @param size Number of bytes to zero (0 → no-op)
 *
 * @sa mem_zero_type(), mem_zero_array(), mem_secure_zero()
 */
static inline void mem_zero(void* ptr, usize size) {
    if (!ptr || size == 0) return;
    memset(ptr, 0, size);
}

/**
 * @brief Securely zero-fills a memory region (crypto-sensitive data)
 *
 * Uses memset_s when available (C11 Annex K), otherwise volatile write loop.
 * Prevents the compiler from optimizing away the zeroing.
 *
 * @param ptr  Memory to clear (NULL → no-op)
 * @param size Number of bytes to zero (0 → no-op)
 */
static inline void mem_secure_zero(void* ptr, usize size) {
    if (!ptr || size == 0) return;
#if defined(__STDC_LIB_EXT1__) && __STDC_LIB_EXT1__
    memset_s(ptr, size, 0, size);
#else
    volatile u8* p = (volatile u8*)ptr;
    usize remaining = size;
    while (remaining--) *p++ = 0;
#endif
}

/**
 * @brief Fills memory with a repeated byte value
 *
 * @param ptr   Memory region to fill (NULL → no-op)
 * @param value Byte value (0-255)
 * @param size  Number of bytes to set (0 → no-op)
 *
 * @sa mem_zero()
 */
static inline void mem_set(void* ptr, int value, usize size) {
    if (!ptr || size == 0) return;
    memset(ptr, value, size);
}

/**
 * @brief Compares two memory regions byte-by-byte
 *
 * @return < 0 if a < b at first differing byte
 *           0 if regions are equal
 *         > 0 if a > b at first differing byte
 *
 * @remark NULL pointers or size=0 → returns 0
 * @remark Not constant-time — do NOT use for cryptographic secrets
 *
 * @sa mem_equal()
 */
static inline int mem_compare(const void* a, const void* b, usize size) {
    if (!a || !b || size == 0) return 0;
    return memcmp(a, b, size);
}

/**
 * @brief Checks whether two memory regions are byte-for-byte identical
 *
 * @return true if equal, false otherwise
 *
 * @remark Both NULL → true
 * @remark size=0 → true
 *
 * @sa mem_compare(), mem_equal_type()
 */
static inline bool mem_equal(const void* a, const void* b, usize size) {
    if (!a || !b) return a == b;
    if (size == 0) return true;
    return memcmp(a, b, size) == 0;
}

/**
 * @brief Checks whether all bytes in a region have the same value
 *
 * @param ptr   Memory region to inspect (NULL → true)
 * @param value Expected byte value
 * @param size  Number of bytes to check (0 → true)
 *
 * @return true if every byte == value, false otherwise
 *
 * @sa mem_is_zero()
 */
static inline bool mem_is_all(const void* ptr, int value, usize size) {
    if (!ptr || size == 0) return true;
    const u8* p = (const u8*)ptr;
    const u8 v = (u8)value;
    for (usize i = 0; i < size; i++) {
        if (p[i] != v) return false;
    }
    return true;
}

/**
 * @brief Checks if a memory region is entirely zero
 *
 * @sa mem_is_all()
 */
static inline bool mem_is_zero(const void* ptr, usize size) {
    return mem_is_all(ptr, 0, size);
}

/**
 * @brief Swaps contents of two non-overlapping memory regions
 *
 * Uses a fixed-size stack buffer. Size must not exceed CANON_MEM_SWAP_MAX
 * (default 256 bytes). For larger swaps, use caller-managed scratch memory.
 *
 * @param a    First region (NULL → no-op)
 * @param b    Second region (NULL → no-op)
 * @param size Number of bytes (must be same for both)
 *
 * @pre size <= CANON_MEM_SWAP_MAX
 * @pre a and b do not overlap
 */
static inline void mem_swap(void* a, void* b, usize size) {
    if (!a || !b || size == 0) return;
    require_msg(size <= CANON_MEM_SWAP_MAX,
        "mem_swap: size exceeds CANON_MEM_SWAP_MAX — use caller-managed scratch buffer");
    u8 tmp[CANON_MEM_SWAP_MAX];
    mem_copy(tmp, a, size);
    mem_copy(a, b, size);
    mem_copy(b, tmp, size);
}

/* ────────────────────────────────────────────────────────────────────────────
   Type-safe convenience macros
   ──────────────────────────────────────────────────────────────────────────── */

/** Zero-initializes a single object */
#define mem_zero_type(ptr)              mem_zero((ptr), sizeof(*(ptr)))

/** Securely zero-initializes a single object (crypto-sensitive) */
#define mem_secure_zero_type(ptr)       mem_secure_zero((ptr), sizeof(*(ptr)))

/** Zero-initializes an entire fixed-size array */
#define mem_zero_array(array)           mem_zero((array), sizeof(array))

/** Copies one object to another of the same type */
#define mem_copy_type(dest, src)        mem_copy((dest), (src), sizeof(*(dest)))

/** Compares two objects of the same type */
#define mem_compare_type(a, b)          mem_compare((a), (b), sizeof(*(a)))

/** Checks if two objects of the same type are equal */
#define mem_equal_type(a, b)            mem_equal((a), (b), sizeof(*(a)))

#endif /* CANON_CORE_MEMORY_H */
