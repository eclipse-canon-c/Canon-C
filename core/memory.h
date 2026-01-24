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
 * - Explicit over implicit — preconditions checked in debug builds
 * - No hidden allocations, no thread-local state
 * - Type-safe convenience macros reduce sizeof boilerplate
 * - Portable — relies only on C99 standard library
 * - Debug assertions catch misuse early
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions: O(1) for alignment helpers, O(n) for memory ops
 * - Alignment helpers compile to 2–5 instructions on most platforms
 * - Wrappers usually inlined and optimized identically to direct stdlib calls
 * - mem_is_all / mem_is_zero: linear scan (no SIMD yet — could be added)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are fully thread-safe — no shared mutable state.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline functions, stdint.h, stdbool.h)
 * - Uses only: memcpy, memmove, memset, memcmp, uintptr_t
 * - No platform-specific intrinsics or assembly
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Custom allocators (arena alignment padding, pool block sizing)
 * - Struct initialization / zeroing before use
 * - Safe buffer copying in parsers / serializers
 * - SIMD alignment verification
 * - Constant-time-ish comparison avoidance documentation
 * - Sanitizing / zeroing sensitive memory regions
 * - Debugging memory pattern checks
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Constant-time comparison of secrets (use dedicated crypto function)
 * - Very large regions where SIMD-optimized routines exist
 * - Overaligned allocations beyond max_align_t (use mem_align_to)
 * - Replacing compiler builtins (__builtin_memcpy etc.) in hot loops
 *
 * @sa mem_align(), mem_align_to(), mem_zero(), mem_copy(), mem_move()
 */

/* ────────────────────────────────────────────────────────────────────────────
   Alignment utilities
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Rounds size up to the next multiple of `max_align_t`
 *
 * This is the "natural" alignment most types expect on the current platform:
 *   - Usually 8 bytes on 64-bit systems
 *   - Usually 4 bytes on 32-bit systems
 *
 * @param size Bytes to align (0 is allowed → returns 0)
 * @return Smallest multiple of `sizeof(max_align_t)` ≥ size,
 *         or `SIZE_MAX` if overflow would occur
 *
 * @remark Overflow-safe: checks before adding padding
 *
 * Example:
 * ```c
 * size_t aligned = mem_align(37);     // → 40 on 64-bit
 * size_t zero    = mem_align(0);      // → 0
 * ```
 *
 * @sa mem_align_to()
 */
static inline size_t mem_align(size_t size) {
    const size_t align = sizeof(max_align_t);

    if (size == 0) return 0;

    // Overflow protection
    if (size > SIZE_MAX - (align - 1u)) {
        return SIZE_MAX;
    }

    return (size + align - 1u) & ~(align - 1u);
}

/**
 * @brief Rounds size up to the next multiple of requested power-of-2 alignment
 *
 * @param size      Bytes to align (0 is allowed → returns 0)
 * @param alignment Power-of-2 alignment value (1,2,4,8,16,…)
 *
 * @return Smallest multiple of alignment ≥ size,
 *         or `SIZE_MAX` on overflow or invalid alignment
 *
 * @pre alignment > 0 and is power of 2 (asserted in debug builds)
 *
 * Example:
 * ```c
 * size_t a = mem_align_to(100, 16);   // → 112
 * size_t b = mem_align_to(64, 64);    // → 64
 * ```
 *
 * @sa mem_align(), mem_is_aligned()
 */
static inline size_t mem_align_to(size_t size, size_t alignment) {
    assert(alignment > 0 && "mem_align_to: alignment must be > 0");
    assert((alignment & (alignment - 1u)) == 0 && "mem_align_to: alignment must be power of 2");

    if (alignment == 0 || (alignment & (alignment - 1u)) != 0) {
        return SIZE_MAX;
    }

    if (size == 0) return 0;

    if (size > SIZE_MAX - (alignment - 1u)) {
        return SIZE_MAX;
    }

    return (size + alignment - 1u) & ~(alignment - 1u);
}

/**
 * @brief Checks whether a pointer satisfies a given alignment requirement
 *
 * @param ptr       Pointer to test (NULL → false)
 * @param alignment Power-of-2 alignment to check (1,2,4,8,…)
 *
 * @return `true` if ptr is aligned to `alignment` bytes, `false` otherwise
 *
 * @pre alignment is power of 2 (asserted in debug builds)
 *
 * Example:
 * ```c
 * if (mem_is_aligned(my_ptr, 32)) {
 *     // safe to use AVX / NEON load
 * }
 * ```
 *
 * @sa mem_get_alignment()
 */
static inline bool mem_is_aligned(const void* ptr, size_t alignment) {
    assert(alignment > 0 && "mem_is_aligned: alignment must be > 0");
    assert((alignment & (alignment - 1u)) == 0 && "mem_is_aligned: alignment must be power of 2");

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
 *
 * @remark For ptr == NULL returns 0 (not SIZE_MAX)
 * @remark Very large alignments (e.g. page-aligned) are correctly detected
 *
 * Example:
 * ```c
 * void* p = aligned_alloc(4096, 4096);
 * size_t align = mem_get_alignment(p); // → 4096
 * ```
 */
static inline size_t mem_get_alignment(const void* ptr) {
    if (!ptr) return 0;

    uintptr_t addr = (uintptr_t)ptr;
    if (addr == 0) return SIZE_MAX; // technically aligned to everything

    // isolate lowest set bit
    return addr & (~addr + 1u);
}

/* ────────────────────────────────────────────────────────────────────────────
   Safe memory operations
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Copies non-overlapping memory regions (safe memcpy wrapper)
 *
 * @param dest Destination memory
 * @param src  Source memory
 * @param size Number of bytes to copy
 *
 * @pre If size > 0, dest and src must not overlap (asserted in debug)
 * @pre dest and src may be NULL (operation becomes no-op)
 *
 * @note For possibly overlapping regions use mem_move() instead.
 *
 * @sa mem_move(), mem_copy_type()
 */
static inline void mem_copy(void* restrict dest, const void* restrict src, size_t size) {
    if (!dest || !src || size == 0) return;

    assert(dest != src && "mem_copy: overlapping regions — use mem_move instead");

    memcpy(dest, src, size);
}

/**
 * @brief Moves memory, correctly handling overlapping regions
 *
 * @param dest Destination memory (may overlap src)
 * @param src  Source memory
 * @param size Number of bytes to move
 *
 * @remark NULL or zero-size inputs are gracefully ignored
 *
 * @sa mem_copy()
 */
static inline void mem_move(void* dest, const void* src, size_t size) {
    if (!dest || !src || size == 0) return;
    memmove(dest, src, size);
}

/**
 * @brief Zero-fills a memory region
 *
 * @param ptr  Memory to clear
 * @param size Number of bytes to zero
 *
 * @remark Commonly used to initialize structs or sanitize memory
 * @remark For crypto-sensitive zeroing consider explicit_bzero() or memset_s()
 *
 * @sa mem_zero_type(), mem_zero_array()
 */
static inline void mem_zero(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    memset(ptr, 0, size);
}

/**
 * @brief Fills memory with a repeated byte value
 *
 * @param ptr   Memory region to fill
 * @param value Byte value (0–255)
 * @param size  Number of bytes to set
 *
 * @sa mem_zero()
 */
static inline void mem_set(void* ptr, int value, size_t size) {
    if (!ptr || size == 0) return;
    memset(ptr, value, size);
}

/**
 * @brief Compares two memory regions byte-by-byte
 *
 * @return
 *   - < 0 if a < b at first differing byte
 *   -   0 if regions are equal
 *   - > 0 if a > b at first differing byte
 *
 * @remark NULL pointers or size=0 → returns 0
 * @remark Not constant-time — do **not** use for cryptographic secrets
 *
 * @sa mem_equal()
 */
static inline int mem_compare(const void* a, const void* b, size_t size) {
    if (!a || !b || size == 0) return 0;
    return memcmp(a, b, size);
}

/**
 * @brief Checks whether two memory regions are byte-for-byte identical
 *
 * @return `true` if equal, `false` otherwise
 *
 * @remark Both NULL → true
 * @remark size=0 → true
 *
 * @sa mem_compare(), mem_equal_type()
 */
static inline bool mem_equal(const void* a, const void* b, size_t size) {
    if (!a || !b) return a == b;
    if (size == 0) return true;
    return memcmp(a, b, size) == 0;
}

/**
 * @brief Checks whether all bytes in a region have the same value
 *
 * @param ptr   Memory region to inspect
 * @param value Expected byte value
 * @param size  Number of bytes to check
 *
 * @return `true` if every byte == value, `false` otherwise
 *
 * @remark NULL or size=0 → returns `true`
 *
 * @sa mem_is_zero()
 */
static inline bool mem_is_all(const void* ptr, int value, size_t size) {
    if (!ptr || size == 0) return true;

    const unsigned char* p = (const unsigned char*)ptr;
    const unsigned char v = (unsigned char)value;

    for (size_t i = 0; i < size; i++) {
        if (p[i] != v) return false;
    }
    return true;
}

/**
 * @brief Convenience function: checks if region is entirely zero
 *
 * @sa mem_is_all()
 */
static inline bool mem_is_zero(const void* ptr, size_t size) {
    return mem_is_all(ptr, 0, size);
}

/* ────────────────────────────────────────────────────────────────────────────
   Type-safe convenience macros
   ──────────────────────────────────────────────────────────────────────────── */

/** Zero-initializes a single object */
#define mem_zero_type(ptr)          mem_zero((ptr), sizeof(*(ptr)))

/** Zero-initializes an entire array */
#define mem_zero_array(array)       mem_zero((array), sizeof(array))

/** Copies one object to another (same type) */
#define mem_copy_type(dest, src)    mem_copy((dest), (src), sizeof(*(dest)))

/** Compares two objects of the same type */
#define mem_compare_type(a, b)      mem_compare((a), (b), sizeof(*(a)))

/** Checks if two objects of the same type are equal */
#define mem_equal_type(a, b)        mem_equal((a), (b), sizeof(*(a)))

/* ────────────────────────────────────────────────────────────────────────────
   Usage Examples (documentation only)
   ────────────────────────────────────────────────────────────────────────────

#include "core/memory.h"

void example(void) {
    // Alignment helpers
    size_t pad = mem_align(45);               // → 48 (on 64-bit)
    size_t pad16 = mem_align_to(45, 16);      // → 48

    float f;
    if (mem_is_aligned(&f, _Alignof(float))) {
        // guaranteed by language — just demo
    }

    // Safe initialization
    struct Packet { uint32_t len; char data[512]; } pkt;
    mem_zero_type(&pkt);

    // Safe copy
    struct Packet copy;
    mem_copy_type(&copy, &pkt);

    // Pattern check
    char buf[1024];
    mem_set(buf, 0xAA, sizeof(buf));
    if (mem_is_all(buf, 0xAA, sizeof(buf))) {
        // yes
    }

    // Overlap-safe move
    char overlap[32] = "1234567890";
    mem_move(overlap + 4, overlap, 10);     // safe shift
}

──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_MEMORY_H */
