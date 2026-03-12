#ifndef CANON_CORE_MEMORY_H
#define CANON_CORE_MEMORY_H

#include <string.h>                     // memcpy, memmove, memset, memcmp
#include <stdint.h>                     // uintptr_t
#include <stdlib.h>                     // malloc, free

#include "core/primitives/types.h"      // u8, usize, bool
#include "core/primitives/limits.h"     // CANON_USIZE_MAX, CANON_DEFAULT_ALIGN
#include "core/primitives/contract.h"   // require_msg, ensure_msg
#include "core/primitives/ptr.h"        // align_up, ptr_is_aligned
#include "core/primitives/bits.h"       // is_power_of_two
#include "core/slice.h"                 // bytes_t, cbytes_t

/**
 * @file memory.h
 * @brief Safe, explicit, low-level memory manipulation and alignment utilities
 *
 * Thin, safety-checked wrappers around memcpy, memmove, memset, memcmp,
 * malloc, free — plus alignment helpers and bytes_t/cbytes_t slice variants.
 *
 * Design:
 * ────────────────────────────────────────────────────────────────────────────
 * - NULL and zero-size safe on all operations (documented per function)
 * - Overflow-protected alignment calculations
 * - Preconditions enforced via require_msg (contract.h); no silent fallbacks
 * - No hidden allocations, no global state, no thread-local state
 * - Type-safe macros reduce sizeof boilerplate
 * - C99 only — no GNU extensions, no platform intrinsics
 *
 * NULL contract (uniform across all functions):
 * ────────────────────────────────────────────────────────────────────────────
 * - Operations on NULL pointers are no-ops (copy, move, zero, set, swap)
 * - Comparisons treat NULL == NULL as true, NULL != non-NULL as false
 * - Predicates (mem_is_all, mem_is_zero) on NULL return false, not true
 * - size == 0 with valid pointers is always a no-op / returns 0 or true
 *
 * Heap allocation:
 * ────────────────────────────────────────────────────────────────────────────
 * mem_alloc/mem_free are explicit named wrappers over malloc/free.
 * Prefer arena_alloc() for temporary, scoped allocations.
 *
 * Relationship to ptr.h:
 * ────────────────────────────────────────────────────────────────────────────
 * ptr.h handles pointer arithmetic and alignment at the address level.
 * memory.h handles byte-region operations and delegates alignment math to ptr.h.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Alignment helpers, alloc, free:        O(1)
 * - copy, move, zero, set, compare, equal: O(n)
 * - is_all, is_zero, swap:                 O(n)
 *
 * Thread-safety: all functions are fully thread-safe (no shared mutable state).
 *
 * @sa core/primitives/ptr.h  — pointer arithmetic and alignment
 * @sa core/slice.h           — bytes_t / cbytes_t
 * @sa core/arena.h           — preferred allocator for scoped memory
 */

/* ════════════════════════════════════════════════════════════════════════════
   mem_swap stack-buffer limit
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Maximum size (bytes) supported by mem_swap
 *
 * mem_swap uses a fixed-size stack buffer (no VLAs).
 * For larger swaps use mem_swap_buf() with a caller-provided scratch buffer.
 * Override before including this header if needed. Default: 256.
 */
#ifndef CANON_MEM_SWAP_MAX
    #define CANON_MEM_SWAP_MAX ((usize)256)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Internal helpers
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if [a, a+size) and [b, b+size) overlap
 * @note  Used internally; exposed for testing via mem_regions_overlap.
 */
static inline bool mem_regions_overlap(const void* a, const void* b, usize size) {
    if (!a || !b || size == 0) return false;
    const u8* pa = (const u8*)a;
    const u8* pb = (const u8*)b;
    return (pa < pb + size) && (pb < pa + size);
}

/* ════════════════════════════════════════════════════════════════════════════
   Heap allocation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocates size bytes on the heap
 *
 * Returns NULL if size == 0 or allocation fails.
 * Caller must free with mem_free().
 *
 * @param size Bytes to allocate (0 → NULL)
 */
static inline void* mem_alloc(usize size) {
    if (size == 0) return NULL;
    return malloc(size);
}

/**
 * @brief Frees memory allocated by mem_alloc
 *
 * NULL-safe. Do not use on memory from arena_alloc() or pool_alloc().
 *
 * @param ptr Pointer to free (NULL-safe)
 */
static inline void mem_free(void* ptr) {
    free(ptr);
}

/* ════════════════════════════════════════════════════════════════════════════
   Alignment utilities
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Rounds size up to the next multiple of CANON_DEFAULT_ALIGN
 *
 * @param size Bytes to align (0 → 0)
 * @return Aligned size, or CANON_USIZE_MAX on overflow
 */
static inline usize mem_align(usize size) {
    const usize natural = CANON_DEFAULT_ALIGN;
    if (size == 0) return 0;
    if (size > CANON_USIZE_MAX - (natural - 1u)) return CANON_USIZE_MAX;
    return align_up(size, natural);
}

/**
 * @brief Rounds size up to the next multiple of a power-of-2 alignment
 *
 * @param size      Bytes to align (0 → 0)
 * @param alignment Power-of-2 alignment (> 0)
 * @return Aligned size, or CANON_USIZE_MAX on overflow
 *
 * @pre alignment > 0 && is_power_of_two(alignment)
 */
static inline usize mem_align_to(usize size, usize alignment) {
    require_msg(alignment > 0,             "mem_align_to: alignment must be > 0");
    require_msg(is_power_of_two(alignment),"mem_align_to: alignment must be a power of 2");
    if (size == 0) return 0;
    if (size > CANON_USIZE_MAX - (alignment - 1u)) return CANON_USIZE_MAX;
    return align_up(size, alignment);
}

/**
 * @brief Returns true if ptr satisfies the given alignment
 *
 * @param ptr       Pointer to test (NULL → false)
 * @param alignment Power-of-2 alignment (> 0)
 *
 * @pre alignment > 0 && is_power_of_two(alignment)
 */
static inline bool mem_is_aligned(const void* ptr, usize alignment) {
    require_msg(alignment > 0,             "mem_is_aligned: alignment must be > 0");
    require_msg(is_power_of_two(alignment),"mem_is_aligned: alignment must be a power of 2");
    return ptr_is_aligned(ptr, alignment);
}

/**
 * @brief Returns the largest power-of-2 alignment of a pointer address
 *
 * Returns the lowest set bit of the address, which equals the maximum
 * power-of-2 by which the address is naturally aligned.
 *
 * @param ptr Pointer to inspect (NULL → 0)
 * @return Power-of-2 alignment in bytes, or 0 if ptr is NULL
 */
static inline usize mem_get_alignment(const void* ptr) {
    if (!ptr) return 0;
    const uintptr_t addr = (uintptr_t)ptr;
    /* lowest set bit == largest power-of-2 that divides addr */
    return (usize)(addr & (uintptr_t)(-(intptr_t)addr));
}

/**
 * @brief Returns true if n is a non-zero power of two
 */
static inline bool mem_is_power_of_two(usize n) {
    return is_power_of_two(n);
}

/* ════════════════════════════════════════════════════════════════════════════
   Safe memory operations — raw pointer variants
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Copies non-overlapping memory regions
 *
 * No-op if dest or src is NULL, or size == 0.
 *
 * @pre dest and src must not overlap — use mem_move() for overlapping regions
 */
static inline void mem_copy(void* restrict dest, const void* restrict src, usize size) {
    if (!dest || !src || size == 0) return;
    require_msg(!mem_regions_overlap(dest, src, size),
                "mem_copy: regions overlap — use mem_move");
    memcpy(dest, src, size);
}

/**
 * @brief Moves memory, handling overlapping regions correctly
 *
 * No-op if dest or src is NULL, or size == 0.
 */
static inline void mem_move(void* dest, const void* src, usize size) {
    if (!dest || !src || size == 0) return;
    memmove(dest, src, size);
}

/**
 * @brief Zero-fills a memory region
 *
 * No-op if ptr is NULL or size == 0.
 */
static inline void mem_zero(void* ptr, usize size) {
    if (!ptr || size == 0) return;
    memset(ptr, 0, size);
}

/**
 * @brief Securely zero-fills a memory region (prevents compiler elimination)
 *
 * Uses memset_s (C11 Annex K) when available, otherwise a volatile write loop.
 * No-op if ptr is NULL or size == 0.
 *
 * @note Use for clearing cryptographic keys, passwords, and sensitive buffers.
 */
static inline void mem_secure_zero(void* ptr, usize size) {
    if (!ptr || size == 0) return;
#if defined(__STDC_LIB_EXT1__) && __STDC_LIB_EXT1__
    memset_s(ptr, size, 0, size);
#else
    volatile u8* p = (volatile u8*)ptr;
    for (usize i = 0; i < size; i++) p[i] = 0;
#endif
}

/**
 * @brief Fills a memory region with a repeated byte value
 *
 * No-op if ptr is NULL or size == 0.
 */
static inline void mem_set(void* ptr, int value, usize size) {
    if (!ptr || size == 0) return;
    memset(ptr, value, size);
}

/**
 * @brief Compares two memory regions (memcmp semantics)
 *
 * NULL contract: both NULL → 0. One NULL → non-zero (unequal).
 * size == 0 with valid pointers → 0.
 *
 * @note Not constant-time. Do not use to compare cryptographic secrets.
 */
static inline int mem_compare(const void* a, const void* b, usize size) {
    if (a == b)   return 0;      /* covers both-NULL and same-pointer */
    if (!a || !b) return (!a) ? -1 : 1; /* NULL sorts before non-NULL */
    if (size == 0) return 0;
    return memcmp(a, b, size);
}

/**
 * @brief Returns true if two memory regions are byte-for-byte identical
 *
 * NULL contract: both NULL → true. One NULL → false. size == 0 → true.
 */
static inline bool mem_equal(const void* a, const void* b, usize size) {
    if (a == b)   return true;   /* covers both-NULL and same-pointer */
    if (!a || !b) return false;
    if (size == 0) return true;
    return memcmp(a, b, size) == 0;
}

/**
 * @brief Returns true if every byte in a region equals value
 *
 * NULL or size == 0 → false.
 */
static inline bool mem_is_all(const void* ptr, int value, usize size) {
    if (!ptr || size == 0) return false;
    const u8* p = (const u8*)ptr;
    const u8  v = (u8)value;
    for (usize i = 0; i < size; i++) {
        if (p[i] != v) return false;
    }
    return true;
}

/**
 * @brief Returns true if every byte in a region is zero
 *
 * NULL or size == 0 → false.
 */
static inline bool mem_is_zero(const void* ptr, usize size) {
    return mem_is_all(ptr, 0, size);
}

/**
 * @brief Swaps the contents of two non-overlapping regions (stack buffer)
 *
 * No-op if a or b is NULL, or size == 0.
 * For size > CANON_MEM_SWAP_MAX, use mem_swap_buf().
 *
 * @pre size <= CANON_MEM_SWAP_MAX
 * @pre a and b must not overlap
 */
static inline void mem_swap(void* a, void* b, usize size) {
    if (!a || !b || size == 0) return;
    require_msg(size <= CANON_MEM_SWAP_MAX,
                "mem_swap: size exceeds CANON_MEM_SWAP_MAX — use mem_swap_buf");
    require_msg(!mem_regions_overlap(a, b, size),
                "mem_swap: regions overlap");
    u8 tmp[CANON_MEM_SWAP_MAX];
    memcpy(tmp, a,   size);
    memcpy(a,   b,   size);
    memcpy(b,   tmp, size);
}

/**
 * @brief Swaps two non-overlapping regions using a caller-provided scratch buffer
 *
 * Handles sizes larger than CANON_MEM_SWAP_MAX.
 * No-op if a, b, or scratch is NULL, or size == 0.
 *
 * @pre scratch_len >= size
 * @pre a, b, and scratch must not overlap each other
 */
static inline void mem_swap_buf(void* a, void* b, usize size,
                                void* scratch, usize scratch_len) {
    if (!a || !b || !scratch || size == 0) return;
    require_msg(scratch_len >= size, "mem_swap_buf: scratch buffer too small");
    require_msg(!mem_regions_overlap(a, b,       size), "mem_swap_buf: a and b overlap");
    require_msg(!mem_regions_overlap(a, scratch, size), "mem_swap_buf: a and scratch overlap");
    require_msg(!mem_regions_overlap(b, scratch, size), "mem_swap_buf: b and scratch overlap");
    memcpy(scratch, a,       size);
    memcpy(a,       b,       size);
    memcpy(b,       scratch, size);
}

/* ════════════════════════════════════════════════════════════════════════════
   bytes_t / cbytes_t variants
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Copies src into dest (bounds-checked)
 *
 * @pre dest.len >= src.len
 * @return Bytes copied, or 0 on failure
 */
static inline usize mem_copy_bytes(bytes_t dest, cbytes_t src) {
    if (!dest.ptr || !src.ptr || src.len == 0) return 0;
    require_msg(dest.len >= src.len, "mem_copy_bytes: dest smaller than src");
    if (dest.len < src.len) return 0;
    require_msg(!mem_regions_overlap(dest.ptr, src.ptr, src.len),
                "mem_copy_bytes: regions overlap — use mem_move_bytes");
    memcpy(dest.ptr, src.ptr, src.len);
    return src.len;
}

/**
 * @brief Moves src into dest, handling overlapping regions
 *
 * @pre dest.len >= src.len
 * @return Bytes moved, or 0 on failure
 */
static inline usize mem_move_bytes(bytes_t dest, cbytes_t src) {
    if (!dest.ptr || !src.ptr || src.len == 0) return 0;
    require_msg(dest.len >= src.len, "mem_move_bytes: dest smaller than src");
    if (dest.len < src.len) return 0;
    memmove(dest.ptr, src.ptr, src.len);
    return src.len;
}

/**
 * @brief Zero-fills an entire bytes_t region
 */
static inline void mem_zero_bytes(bytes_t b) {
    if (!b.ptr || b.len == 0) return;
    memset(b.ptr, 0, b.len);
}

/**
 * @brief Fills a bytes_t region with a repeated byte value
 */
static inline void mem_set_bytes(bytes_t b, int value) {
    if (!b.ptr || b.len == 0) return;
    memset(b.ptr, value, b.len);
}

/**
 * @brief Returns true if two cbytes_t regions are byte-for-byte identical
 *
 * Different lengths → false. Both empty (len == 0, ptr may differ) → true.
 */
static inline bool mem_equal_bytes(cbytes_t a, cbytes_t b) {
    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;   /* covers both-NULL with equal len */
    if (!a.ptr || !b.ptr) return false;
    if (a.len == 0) return true;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/**
 * @brief Returns true if every byte in a cbytes_t region is zero
 *
 * NULL ptr or len == 0 → false.
 */
static inline bool mem_is_zero_bytes(cbytes_t b) {
    return mem_is_all(b.ptr, 0, b.len);
}

/**
 * @brief Securely zero-fills a bytes_t region (prevents compiler elimination)
 */
static inline void mem_secure_zero_bytes(bytes_t b) {
    mem_secure_zero(b.ptr, b.len);
}

/* ════════════════════════════════════════════════════════════════════════════
   Type-safe convenience macros
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Zero-initializes a single object via pointer */
#define mem_zero_type(ptr)              mem_zero((ptr), sizeof(*(ptr)))

/**
 * @brief Securely zero-initializes a single object via pointer
 * @note  Zeroes only the pointed-to value, not any referenced memory.
 *        For pointer members, zero those explicitly before calling this.
 */
#define mem_secure_zero_type(ptr)       mem_secure_zero((ptr), sizeof(*(ptr)))

/** @brief Zero-initializes an entire fixed-size array */
#define mem_zero_array(array)           mem_zero((array), sizeof(array))

/** @brief Copies one object to another of the same type */
#define mem_copy_type(dest, src)        mem_copy((dest), (src), sizeof(*(dest)))

/** @brief Returns true if two same-type objects are byte-equal */
#define mem_equal_type(a, b)            mem_equal((a), (b), sizeof(*(a)))

/**
 * @brief Allocates one object of Type on the heap
 *
 * Caller must free with mem_free().
 * Returns NULL on allocation failure.
 */
#define mem_alloc_type(Type)            ((Type*)mem_alloc(sizeof(Type)))

/**
 * @brief Allocates an array of count objects of Type on the heap
 *
 * Caller must free with mem_free().
 * Returns NULL on allocation failure or if count == 0.
 *
 * @warning Does not check for sizeof(Type) * count overflow.
 *          Caller is responsible for validating count before use.
 */
#define mem_alloc_array(Type, count)    ((Type*)mem_alloc(sizeof(Type) * (count)))

#endif /* CANON_CORE_MEMORY_H */
