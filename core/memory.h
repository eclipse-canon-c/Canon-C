#ifndef CANON_CORE_MEMORY_H
#define CANON_CORE_MEMORY_H

#include <string.h>                     // memcpy, memmove, memset, memcmp

#include "core/primitives/types.h"      // u8, usize, bool, (and transitively uintptr_t via <stdint.h>)
#include "core/primitives/limits.h"     // CANON_USIZE_MAX, CANON_DEFAULT_ALIGN
#include "core/primitives/contract.h"   // require_msg
#include "core/primitives/ptr.h"        // align_up, ptr_is_aligned
#include "core/primitives/bits.h"       // is_power_of_two()
#include "core/slice.h"                 // bytes_t, cbytes_t

/**
 * @file memory.h
 * @brief Safe, explicit, low-level memory manipulation and alignment utilities
 *
 * Provides thin, safety-checked wrappers around standard memory functions
 * (memcpy, memmove, memset, memcmp) and alignment helpers.
 * Designed for use in custom allocators (arenas, pools), parsers,
 * and performance-critical code where explicit control is required.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Null-safe and zero-size safe on all operations
 * - Overflow-protected alignment calculations
 * - Explicit over implicit — preconditions checked via contract.h
 * - No hidden allocations, no thread-local state
 * - Type-safe convenience macros reduce sizeof boilerplate
 * - bytes_t / cbytes_t variants for all copy/move/zero/compare operations
 * - Portable — relies only on C99 standard library + Canon-C primitives
 *
 * Relationship to ptr.h:
 * ────────────────────────────────────────────────────────────────────────────
 * core/primitives/ptr.h handles pointer arithmetic and alignment at the
 * address level. memory.h handles byte-region operations (copy, move, zero,
 * compare) and delegates alignment math to ptr.h. The two are complementary:
 * ptr.h asks "where is this address?" — memory.h asks "what's in this region?"
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Alignment helpers: O(1)
 * - mem_copy, mem_move, mem_zero, mem_set: O(n)
 * - mem_compare, mem_equal: O(n)
 * - mem_is_all, mem_is_zero: O(n)
 * - mem_swap: O(n), capped at CANON_MEM_SWAP_MAX
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are fully thread-safe — no shared mutable state.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses only: memcpy, memmove, memset, memcmp from <string.h>
 * - No platform-specific intrinsics or assembly
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * memory.h sits at the bottom of core/. It depends only on primitives/
 * and core/slice.h. No other core module may be included here.
 *
 * @sa core/primitives/ptr.h — pointer arithmetic, alignment, ptr_align_up
 * @sa core/slice.h          — bytes_t / cbytes_t used by _bytes variants here
 */

/* ════════════════════════════════════════════════════════════════════════════
   mem_swap stack buffer limit
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Maximum size (bytes) supported by mem_swap
 *
 * mem_swap uses a fixed-size stack buffer to avoid VLAs.
 * Override by defining CANON_MEM_SWAP_MAX before including this header.
 * Default: 256 bytes.
 */
#ifndef CANON_MEM_SWAP_MAX
    #define CANON_MEM_SWAP_MAX ((usize)256)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Alignment utilities (delegate to ptr.h)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Rounds size up to the next multiple of max_align_t (natural alignment)
 *
 * @param size Bytes to align (0 → returns 0)
 * @return Smallest multiple of sizeof(max_align_t) >= size,
 *         or CANON_USIZE_MAX if overflow would occur
 *
 * @note Delegates to align_up() from core/primitives/ptr.h.
 * Performance: O(1)
 */
static inline usize mem_align(usize size) {
    const usize natural = sizeof(max_align_t);
    if (size == 0) return 0;
    if (size > CANON_USIZE_MAX - (natural - 1u)) return CANON_USIZE_MAX;
    return align_up(size, natural);
}

/**
 * @brief Rounds size up to the next multiple of a power-of-2 alignment
 *
 * @param size      Bytes to align (0 → returns 0)
 * @param alignment Power-of-2 alignment (> 0)
 * @return Smallest multiple of alignment >= size,
 *         or CANON_USIZE_MAX on overflow or invalid alignment
 *
 * @pre alignment > 0 && is_power_of_two(alignment)
 *
 * @note Delegates to align_up() from core/primitives/ptr.h.
 * Performance: O(1)
 */
static inline usize mem_align_to(usize size, usize alignment) {
    require_msg(alignment > 0,
        "mem_align_to: alignment must be > 0");
    require_msg(is_power_of_two(alignment),
        "mem_align_to: alignment must be a power of 2");
    if (!is_power_of_two(alignment)) return CANON_USIZE_MAX;
    if (size == 0) return 0;
    if (size > CANON_USIZE_MAX - (alignment - 1u)) return CANON_USIZE_MAX;
    return align_up(size, alignment);
}

/**
 * @brief Returns true if ptr satisfies the given alignment requirement
 *
 * @param ptr       Pointer to test (NULL → false)
 * @param alignment Power-of-2 alignment (> 0)
 *
 * @note Delegates to ptr_is_aligned() from core/primitives/ptr.h.
 * Performance: O(1)
 */
static inline bool mem_is_aligned(const void* ptr, usize alignment) {
    require_msg(alignment > 0,
        "mem_is_aligned: alignment must be > 0");
    require_msg(is_power_of_two(alignment),
        "mem_is_aligned: alignment must be a power of 2");
    return ptr_is_aligned(ptr, alignment);
}

/**
 * @brief Returns the largest power-of-2 alignment of a pointer address
 *
 * @param ptr Pointer to inspect (NULL → 0)
 * @return Alignment in bytes (power of 2), or 0 if ptr is NULL
 *
 * Performance: O(1)
 */
static inline usize mem_get_alignment(const void* ptr) {
    if (!ptr) return 0;
    uintptr_t addr = (uintptr_t)ptr;
    if (addr == 0) return CANON_USIZE_MAX;
    return (usize)(addr & (~addr + 1u));
}

/**
 * @brief Returns true if n is a power of two and > 0
 *
 * @note Delegates to is_power_of_two() from core/primitives/ptr.h.
 * Performance: O(1)
 */
static inline bool mem_is_power_of_two(usize n) {
    return is_power_of_two(n);
}

/* ════════════════════════════════════════════════════════════════════════════
   Safe memory operations — raw pointer variants
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Copies non-overlapping memory regions (safe memcpy wrapper)
 *
 * @pre dest and src must not overlap
 * @note For overlapping regions use mem_move() instead.
 *
 * Performance: O(n)
 */
static inline void mem_copy(void* restrict dest, const void* restrict src, usize size) {
    if (!dest || !src || size == 0) return;
    ensure_msg(dest != src,
        "mem_copy: dest == src — use mem_move for overlapping regions");
    memcpy(dest, src, size);
}

/**
 * @brief Moves memory, correctly handling overlapping regions
 *
 * Performance: O(n)
 */
static inline void mem_move(void* dest, const void* src, usize size) {
    if (!dest || !src || size == 0) return;
    memmove(dest, src, size);
}

/**
 * @brief Zero-fills a memory region
 *
 * Performance: O(n)
 */
static inline void mem_zero(void* ptr, usize size) {
    if (!ptr || size == 0) return;
    memset(ptr, 0, size);
}

/**
 * @brief Securely zero-fills a memory region (prevents compiler optimization)
 *
 * Uses memset_s when available (C11 Annex K), otherwise a volatile write loop.
 *
 * Performance: O(n)
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
 * @brief Fills a memory region with a repeated byte value
 *
 * Performance: O(n)
 */
static inline void mem_set(void* ptr, int value, usize size) {
    if (!ptr || size == 0) return;
    memset(ptr, value, size);
}

/**
 * @brief Compares two memory regions byte-by-byte (memcmp semantics)
 *
 * @note NULL or size == 0 → returns 0
 * @note Not constant-time — do NOT use for cryptographic secrets
 *
 * Performance: O(n)
 */
static inline int mem_compare(const void* a, const void* b, usize size) {
    if (!a || !b || size == 0) return 0;
    return memcmp(a, b, size);
}

/**
 * @brief Returns true if two memory regions are byte-for-byte identical
 *
 * @note Both NULL → true. size == 0 → true.
 *
 * Performance: O(n)
 */
static inline bool mem_equal(const void* a, const void* b, usize size) {
    if (!a || !b) return a == b;
    if (size == 0) return true;
    return memcmp(a, b, size) == 0;
}

/**
 * @brief Returns true if every byte in a region equals value
 *
 * Performance: O(n)
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
 * @brief Returns true if every byte in a region is zero
 *
 * Performance: O(n)
 */
static inline bool mem_is_zero(const void* ptr, usize size) {
    return mem_is_all(ptr, 0, size);
}

/**
 * @brief Swaps contents of two non-overlapping memory regions
 *
 * @pre size <= CANON_MEM_SWAP_MAX
 * @pre a and b do not overlap
 *
 * Performance: O(n)
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

/* ════════════════════════════════════════════════════════════════════════════
   bytes_t / cbytes_t variants — slice.h integration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Copies src bytes into dest (bounds-checked via view lengths)
 *
 * @pre dest.len >= src.len
 * @return Number of bytes copied, or 0 on failure
 *
 * Performance: O(n)
 */
static inline usize mem_copy_bytes(bytes_t dest, cbytes_t src) {
    if (!dest.ptr || !src.ptr || src.len == 0) return 0;
    require_msg(dest.len >= src.len,
        "mem_copy_bytes: dest is smaller than src");
    if (dest.len < src.len) return 0;
    memcpy(dest.ptr, src.ptr, src.len);
    return src.len;
}

/**
 * @brief Moves src bytes into dest, handling overlapping regions
 *
 * @pre dest.len >= src.len
 * @return Number of bytes moved, or 0 on failure
 *
 * Performance: O(n)
 */
static inline usize mem_move_bytes(bytes_t dest, cbytes_t src) {
    if (!dest.ptr || !src.ptr || src.len == 0) return 0;
    require_msg(dest.len >= src.len,
        "mem_move_bytes: dest is smaller than src");
    if (dest.len < src.len) return 0;
    memmove(dest.ptr, src.ptr, src.len);
    return src.len;
}

/**
 * @brief Zero-fills an entire bytes_t region
 *
 * Performance: O(n)
 */
static inline void mem_zero_bytes(bytes_t b) {
    if (!b.ptr || b.len == 0) return;
    memset(b.ptr, 0, b.len);
}

/**
 * @brief Fills a bytes_t region with a repeated byte value
 *
 * Performance: O(n)
 */
static inline void mem_set_bytes(bytes_t b, int value) {
    if (!b.ptr || b.len == 0) return;
    memset(b.ptr, value, b.len);
}

/**
 * @brief Returns true if two cbytes_t regions are byte-for-byte identical
 *
 * Performance: O(n)
 */
static inline bool mem_equal_bytes(cbytes_t a, cbytes_t b) {
    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;
    if (!a.ptr || !b.ptr) return false;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/**
 * @brief Returns true if every byte in a cbytes_t region is zero
 *
 * Performance: O(n)
 */
static inline bool mem_is_zero_bytes(cbytes_t b) {
    return mem_is_all(b.ptr, 0, b.len);
}

/**
 * @brief Securely zero-fills a bytes_t region (prevents compiler optimization)
 *
 * Performance: O(n)
 */
static inline void mem_secure_zero_bytes(bytes_t b) {
    mem_secure_zero(b.ptr, b.len);
}

/* ════════════════════════════════════════════════════════════════════════════
   Type-safe convenience macros
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Zero-initializes a single object */
#define mem_zero_type(ptr)              mem_zero((ptr), sizeof(*(ptr)))

/** @brief Securely zero-initializes a single object (crypto-sensitive) */
#define mem_secure_zero_type(ptr)       mem_secure_zero((ptr), sizeof(*(ptr)))

/** @brief Zero-initializes an entire fixed-size array */
#define mem_zero_array(array)           mem_zero((array), sizeof(array))

/** @brief Copies one object to another of the same type */
#define mem_copy_type(dest, src)        mem_copy((dest), (src), sizeof(*(dest)))

/** @brief Compares two objects of the same type */
#define mem_compare_type(a, b)          mem_compare((a), (b), sizeof(*(a)))

/** @brief Returns true if two objects of the same type are byte-equal */
#define mem_equal_type(a, b)            mem_equal((a), (b), sizeof(*(a)))

#endif /* CANON_CORE_MEMORY_H */
