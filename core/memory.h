/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_CORE_MEMORY_H
#define CANON_CORE_MEMORY_H

#include <string.h>                     /* memcpy, memmove, memset, memcmp */
#include <stdint.h>                     /* uintptr_t */
#include <stdlib.h>                     /* malloc, free */

#include "core/primitives/types.h"      /* u8, usize, bool */
#include "core/primitives/limits.h"     /* CANON_USIZE_MAX, CANON_DEFAULT_ALIGN */
#include "core/primitives/contract.h"   /* require_msg, ensure_msg */
#include "core/primitives/checked.h"    /* checked_mul */
#include "core/primitives/ptr.h"        /* align_up, ptr_is_aligned, is_power_of_two */
#include "core/slice.h"                 /* bytes_t, cbytes_t */

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
 * - Overflow-protected array allocation (via mem_alloc_array_checked)
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
 * mem_alloc_array_checked routes through checked_mul (from checked.h) to
 * detect element_size * count overflow before calling mem_alloc — eliminating
 * the silent wraparound hazard that would otherwise produce a heap-buffer
 * overflow when the caller computes count*sizeof(T) with a hand-rolled
 * multiplication. The mem_alloc_array(Type, count) macro is now a thin
 * wrapper that supplies sizeof(Type) and the typed cast; its arithmetic
 * happens inside the verified function.
 *
 * Prefer arena_alloc() for temporary, scoped allocations.
 *
 * Relationship to ptr.h and checked.h:
 * ────────────────────────────────────────────────────────────────────────────
 * ptr.h handles pointer arithmetic and alignment at the address level.
 * checked.h provides overflow-detecting integer arithmetic.
 * memory.h handles byte-region operations and delegates alignment math to
 * ptr.h, overflow-checked multiplication to checked.h.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Alignment helpers, alloc, free:        O(1)
 * - copy, move, zero, set, compare, equal: O(n)
 * - is_all, is_zero, swap:                 O(n)
 *
 * Thread-safety: all functions are fully thread-safe (no shared mutable state).
 *
 * Verification scope (Round 2 — full surface):
 * ────────────────────────────────────────────────────────────────────────────
 * All 22 user-facing functions in memory.h carry ACSL contracts and are
 * WP-verified, organized in three groups:
 *
 *   Group 1 — alignment and allocation (round 1, 9 functions):
 *     mem_regions_overlap, mem_alloc, mem_free, mem_alloc_array_checked,
 *     mem_align, mem_align_to, mem_is_aligned, mem_get_alignment,
 *     mem_is_power_of_two
 *
 *   Group 2 — raw memory operations (round 2, 11 functions):
 *     mem_copy, mem_move, mem_zero, mem_secure_zero, mem_set,
 *     mem_compare, mem_equal, mem_is_all, mem_is_zero, mem_swap, mem_swap_buf
 *
 *   Group 3 — bytes_t/cbytes_t variants (round 2, 7 functions):
 *     mem_copy_bytes, mem_move_bytes, mem_zero_bytes, mem_set_bytes,
 *     mem_equal_bytes, mem_is_zero_bytes, mem_secure_zero_bytes
 *
 * Residuals are documented in docs/deviations.md under VERIFY-008. The
 * inherited residuals from checked.h, ptr.h, and slice.h propagate
 * unchanged — composable verification's central claim.
 *
 * Macro instantiations (mem_alloc_array, mem_alloc_type, mem_zero_type, etc.)
 * are documented but not directly WP-verified — see VERIFY-007 (slice.h
 * DEFINE_SLICE precedent) for the macro verification rationale.
 *
 * @sa core/primitives/ptr.h     — pointer arithmetic and alignment
 * @sa core/primitives/checked.h — overflow-detecting arithmetic
 * @sa core/slice.h              — bytes_t / cbytes_t
 * @sa core/arena.h              — preferred allocator for scoped memory
 */

/* ════════════════════════════════════════════════════════════════════════════
   ACSL predicates (verification-only)
   ════════════════════════════════════════════════════════════════════════════ */

/*@
  // Two byte-regions overlap iff each region's start lies within the
  // other's range. Empty regions (size == 0) never overlap.
  predicate regions_overlap{L}(char *a, char *b, integer size) =
      size > 0 && a < b + size && b < a + size;

  // Address-level alignment: ptr is aligned to a power-of-2 boundary
  // iff its low (alignment - 1) bits are zero.
  predicate is_aligned_addr(void *ptr, integer alignment) =
      alignment > 0 && ((unsigned long)ptr & (unsigned long)(alignment - 1)) == 0;

  // Bytes-region read validity: either size == 0 (no read needed) or
  // every byte in [ptr, ptr + size) is readable.
  predicate mem_valid_read{L}(void *ptr, integer size) =
      size == 0 || \valid_read((char *)ptr + (0 .. size - 1));

  // Bytes-region write validity: either size == 0 or every byte is writable.
  predicate mem_valid_write{L}(void *ptr, integer size) =
      size == 0 || \valid((char *)ptr + (0 .. size - 1));
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
/*@
  assigns  \nothing;

  behavior null_a:
    assumes a == \null;
    ensures \result == \false;

  behavior null_b:
    assumes a != \null && b == \null;
    ensures \result == \false;

  behavior zero_size:
    assumes a != \null && b != \null && size == 0;
    ensures \result == \false;

  behavior overlapping:
    assumes a != \null && b != \null && size > 0;
    ensures \result <==> regions_overlap((char *)a, (char *)b, (integer)size);

  complete behaviors;
  disjoint behaviors;
*/
static inline bool mem_regions_overlap(const void* a, const void* b, usize size) {
    if (!a || !b || (size == 0u)) { return false; }
    const u8* pa = (const u8*)a;
    const u8* pb = (const u8*)b;
    return (pa < (pb + size)) && (pb < (pa + size));
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
/*@
  assigns \nothing;

  behavior zero_size:
    assumes size == 0;
    ensures \result == \null;

  behavior nonzero_size:
    assumes size > 0;
    ensures \result == \null || \fresh(\result, size);

  complete behaviors;
  disjoint behaviors;
*/
static inline void* mem_alloc(usize size) {
    if (size == 0u) { return NULL; }
    return malloc(size);
}

/**
 * @brief Frees memory allocated by mem_alloc
 *
 * NULL-safe. Do not use on memory from arena_alloc() or pool_alloc().
 *
 * @param ptr Pointer to free (NULL-safe)
 */
/*@
  requires ptr == \null || \freeable(ptr);
  assigns  \nothing;
  ensures  \true;
*/
static inline void mem_free(void* ptr) {
    free(ptr);
}

/**
 * @brief Allocates an array of count elements of element_size bytes each.
 *
 * Computes total size as element_size * count using checked multiplication
 * (checked_mul from checked.h). Returns NULL if the multiplication
 * overflows usize, if either argument is zero, or if the underlying
 * allocation fails.
 *
 * Prefer the type-safe macro mem_alloc_array(Type, count) at call sites;
 * this function is the verified primitive the macro routes through.
 *
 * NULL contract:
 *   - element_size == 0 OR count == 0      → NULL (consistent with mem_alloc)
 *   - element_size * count overflows usize → NULL (detected by checked_mul)
 *   - malloc fails                         → NULL
 *   - otherwise                            → pointer to a buffer of total bytes
 *
 * Caller must free with mem_free().
 *
 * Compositional verification: this function's correctness is a transitive
 * consequence of checked_mul's verified overflow detection plus mem_alloc's
 * allocation contract. The README's translation table maps "Arithmetic
 * operations" to checked.h; this function applies that principle at the
 * memory.h boundary so that callers and macro consumers inherit the
 * substrate's overflow safety automatically.
 *
 * @param element_size Size of each element in bytes (0 → NULL)
 * @param count        Number of elements (0 → NULL)
 *
 * @return Pointer to the allocated buffer, or NULL on overflow / allocation
 *         failure / either argument zero.
 *
 * Example:
 * ```c
 * MyStruct* arr = mem_alloc_array_checked(sizeof(MyStruct), user_count);
 * if (!arr) return ERROR_ALLOC;
 * ```
 *
 * @sa mem_alloc(), mem_alloc_array (macro), checked_mul()
 */
/*@
  assigns \nothing;

  behavior zero_element_size:
    assumes element_size == 0;
    ensures \result == \null;

  behavior zero_count:
    assumes element_size > 0 && count == 0;
    ensures \result == \null;

  behavior overflow:
    assumes element_size > 0 && count > 0 &&
            element_size * count > CANON_USIZE_MAX;
    ensures \result == \null;

  behavior nonoverflow:
    assumes element_size > 0 && count > 0 &&
            element_size * count <= CANON_USIZE_MAX;
    ensures \result == \null || \fresh(\result, element_size * count);

  complete behaviors;
  disjoint behaviors;
*/
static inline void* mem_alloc_array_checked(usize element_size, usize count) {
    usize total;
    if ((element_size == 0u) || (count == 0u)) { return NULL; }
    if (!checked_mul(element_size, count, &total)) { return NULL; }
    return mem_alloc(total);
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
/*@
  assigns \nothing;

  behavior zero:
    assumes size == 0;
    ensures \result == 0;

  behavior overflow:
    assumes size > 0 && size > CANON_USIZE_MAX - (CANON_DEFAULT_ALIGN - 1);
    ensures \result == CANON_USIZE_MAX;

  behavior normal:
    assumes size > 0 && size <= CANON_USIZE_MAX - (CANON_DEFAULT_ALIGN - 1);
    ensures \result >= size;
    ensures \result % CANON_DEFAULT_ALIGN == 0;

  complete behaviors;
  disjoint behaviors;
*/
static inline usize mem_align(usize size) {
    const usize natural = CANON_DEFAULT_ALIGN;
    if (size == 0u) { return 0u; }
    if (size > (CANON_USIZE_MAX - (natural - 1u))) { return CANON_USIZE_MAX; }
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
/*@
  requires alignment > 0;
  requires (alignment & (alignment - 1)) == 0;
  assigns  \nothing;

  behavior zero:
    assumes size == 0;
    ensures \result == 0;

  behavior overflow:
    assumes size > 0 && size > CANON_USIZE_MAX - (alignment - 1);
    ensures \result == CANON_USIZE_MAX;

  behavior normal:
    assumes size > 0 && size <= CANON_USIZE_MAX - (alignment - 1);
    ensures \result >= size;
    ensures \result % alignment == 0;

  complete behaviors;
  disjoint behaviors;
*/
static inline usize mem_align_to(usize size, usize alignment) {
    require_msg(alignment > 0u,              "mem_align_to: alignment must be > 0");
    require_msg(is_power_of_two(alignment), "mem_align_to: alignment must be a power of 2");
    if (size == 0u) { return 0u; }
    if (size > (CANON_USIZE_MAX - (alignment - 1u))) { return CANON_USIZE_MAX; }
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
/*@
  requires alignment > 0;
  requires (alignment & (alignment - 1)) == 0;
  assigns  \nothing;

  behavior null:
    assumes ptr == \null;
    ensures \result == \false;

  behavior nonnull_aligned:
    assumes ptr != \null;
    assumes is_aligned_addr((void *)ptr, (integer)alignment);
    ensures \result == \true;

  behavior nonnull_unaligned:
    assumes ptr != \null;
    assumes !is_aligned_addr((void *)ptr, (integer)alignment);
    ensures \result == \false;

  complete behaviors;
  disjoint behaviors;
*/
static inline bool mem_is_aligned(const void* ptr, usize alignment) {
    require_msg(alignment > 0u,              "mem_is_aligned: alignment must be > 0");
    require_msg(is_power_of_two(alignment), "mem_is_aligned: alignment must be a power of 2");
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
/*@
  assigns \nothing;

  behavior null:
    assumes ptr == \null;
    ensures \result == 0;

  behavior nonnull:
    assumes ptr != \null;
    ensures \result >= 1;
    ensures (\result & (\result - 1)) == 0;

  complete behaviors;
  disjoint behaviors;
*/
static inline usize mem_get_alignment(const void* ptr) {
    uintptr_t addr;
    if (!ptr) { return 0; }
    addr = (uintptr_t)ptr;
    /* lowest set bit == largest power-of-2 that divides addr */
    return (usize)(addr & (uintptr_t)(-(intptr_t)addr));
}

/**
 * @brief Returns true if n is a non-zero power of two
 */
/*@
  assigns \nothing;
  ensures \result == (n > 0 && (n & (n - 1)) == 0);
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
/*@
  requires dest == \null || src == \null || size == 0 ||
           mem_valid_write(dest, (integer)size);
  requires dest == \null || src == \null || size == 0 ||
           mem_valid_read((void *)src, (integer)size);
  requires dest == \null || src == \null || size == 0 ||
           !regions_overlap((char *)dest, (char *)src, (integer)size);

  behavior null_or_zero:
    assumes dest == \null || src == \null || size == 0;
    assigns \nothing;

  behavior copy:
    assumes dest != \null && src != \null && size > 0;
    assigns ((char *)dest)[0 .. size - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_copy(void* restrict dest, const void* restrict src, usize size) {
    if (!dest || !src || (size == 0u)) { return; }
    require_msg(!mem_regions_overlap(dest, src, size),
                "mem_copy: regions overlap — use mem_move");
    (void)memcpy(dest, src, size);
}

/**
 * @brief Moves memory, handling overlapping regions correctly
 *
 * No-op if dest or src is NULL, or size == 0.
 */
/*@
  requires dest == \null || src == \null || size == 0 ||
           mem_valid_write(dest, (integer)size);
  requires dest == \null || src == \null || size == 0 ||
           mem_valid_read((void *)src, (integer)size);

  behavior null_or_zero:
    assumes dest == \null || src == \null || size == 0;
    assigns \nothing;

  behavior move:
    assumes dest != \null && src != \null && size > 0;
    assigns ((char *)dest)[0 .. size - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_move(void* dest, const void* src, usize size) {
    if (!dest || !src || (size == 0u)) { return; }
    (void)memmove(dest, src, size);
}

/**
 * @brief Zero-fills a memory region
 *
 * No-op if ptr is NULL or size == 0.
 */
/*@
  requires ptr == \null || size == 0 ||
           mem_valid_write(ptr, (integer)size);

  behavior null_or_zero:
    assumes ptr == \null || size == 0;
    assigns \nothing;

  behavior zero:
    assumes ptr != \null && size > 0;
    assigns ((char *)ptr)[0 .. size - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_zero(void* ptr, usize size) {
    if (!ptr || (size == 0u)) { return; }
    (void)memset(ptr, 0, size);
}

/**
 * @brief Securely zero-fills a memory region (prevents compiler elimination)
 *
 * Uses memset_s (C11 Annex K) when available, otherwise a volatile write loop.
 * No-op if ptr is NULL or size == 0.
 *
 * @note Use for clearing cryptographic keys, passwords, and sensitive buffers.
 */
/*@
  requires ptr == \null || size == 0 ||
           mem_valid_write(ptr, (integer)size);

  behavior null_or_zero:
    assumes ptr == \null || size == 0;
    assigns \nothing;

  behavior secure_zero:
    assumes ptr != \null && size > 0;
    assigns ((char *)ptr)[0 .. size - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_secure_zero(void* ptr, usize size) {
    if (!ptr || (size == 0u)) { return; }
#if defined(__STDC_LIB_EXT1__) && __STDC_LIB_EXT1__ && !defined(__FRAMAC__)
    memset_s(ptr, size, 0, size);
#else
    volatile u8* p = (volatile u8*)ptr;
    usize i;
    /*@
      loop invariant 0 <= i <= size;
      loop assigns i, p[0 .. size - 1];
      loop variant size - i;
    */
    for (i = 0; i < size; i++) { p[i] = 0; }
#endif
}

/**
 * @brief Fills a memory region with a repeated byte value
 *
 * No-op if ptr is NULL or size == 0.
 */
/*@
  requires ptr == \null || size == 0 ||
           mem_valid_write(ptr, (integer)size);

  behavior null_or_zero:
    assumes ptr == \null || size == 0;
    assigns \nothing;

  behavior set:
    assumes ptr != \null && size > 0;
    assigns ((char *)ptr)[0 .. size - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_set(void* ptr, int value, usize size) {
    if (!ptr || (size == 0u)) { return; }
    (void)memset(ptr, value, size);
}

/**
 * @brief Compares two memory regions (memcmp semantics)
 *
 * NULL contract: both NULL → 0. One NULL → non-zero (unequal).
 * size == 0 with valid pointers → 0.
 *
 * @note Not constant-time. Do not use to compare cryptographic secrets.
 * @note On the compare path both regions must contain initialized bytes
 *       (ACSL \initialized precondition; the Valgrind CI job enforces
 *       the same property at runtime).
 */
/*@
  requires a == \null || b == \null || size == 0 || a == b ||
           (mem_valid_read((void *)a, (integer)size) &&
            mem_valid_read((void *)b, (integer)size));
  assigns  \nothing;

  behavior both_null_or_equal:
    assumes a == b;
    ensures \result == 0;

  behavior a_null:
    assumes a != b && a == \null;
    ensures \result == -1;

  behavior b_null:
    assumes a != b && a != \null && b == \null;
    ensures \result == 1;

  behavior zero_size:
    assumes a != b && a != \null && b != \null && size == 0;
    ensures \result == 0;

  behavior memcmp:
    assumes a != b && a != \null && b != \null && size > 0;
    requires init_a: \initialized((char*)a + (0 .. (integer)size - 1));
    requires init_b: \initialized((char*)b + (0 .. (integer)size - 1));

  complete behaviors;
  disjoint behaviors;
*/
static inline int mem_compare(const void* a, const void* b, usize size) {
    if (a == b)    { return 0; }                    /* covers both-NULL and same-pointer */
    if (!a || !b)  { return (!a) ? -1 : 1; }        /* NULL sorts before non-NULL */
    if (size == 0u) { return 0u; }
    return memcmp(a, b, size);
}

/**
 * @brief Returns true if two memory regions are byte-for-byte identical
 *
 * NULL contract: both NULL → true. One NULL → false. size == 0 → true.
 *
 * @note On the compare path both regions must contain initialized bytes
 *       (ACSL \initialized precondition; the Valgrind CI job enforces
 *       the same property at runtime).
 */
/*@
  requires a == \null || b == \null || size == 0 || a == b ||
           (mem_valid_read((void *)a, (integer)size) &&
            mem_valid_read((void *)b, (integer)size));
  assigns  \nothing;

  behavior both_null_or_equal:
    assumes a == b;
    ensures \result == \true;

  behavior one_null:
    assumes a != b && (a == \null || b == \null);
    ensures \result == \false;

  behavior zero_size:
    assumes a != b && a != \null && b != \null && size == 0;
    ensures \result == \true;

  behavior compare:
    assumes a != b && a != \null && b != \null && size > 0;
    requires init_a: \initialized((char*)a + (0 .. (integer)size - 1));
    requires init_b: \initialized((char*)b + (0 .. (integer)size - 1));

  complete behaviors;
  disjoint behaviors;
*/
static inline bool mem_equal(const void* a, const void* b, usize size) {
    if (a == b)    { return true; }                 /* covers both-NULL and same-pointer */
    if (!a || !b)  { return false; }
    if (size == 0u) { return true; }
    return memcmp(a, b, size) == 0;
}

/**
 * @brief Returns true if every byte in a region equals value
 *
 * NULL or size == 0 → false.
 */
/*@
  requires ptr == \null || size == 0 ||
           mem_valid_read((void *)ptr, (integer)size);
  assigns  \nothing;

  behavior null_or_zero:
    assumes ptr == \null || size == 0;
    ensures \result == \false;

  behavior scan:
    assumes ptr != \null && size > 0;

  complete behaviors;
  disjoint behaviors;
*/
static inline bool mem_is_all(const void* ptr, int value, usize size) {
    const u8* p;
    u8        v;
    usize     i;
    if (!ptr || (size == 0u)) { return false; }
    p = (const u8*)ptr;
    v = (u8)value;
    /*@
      loop invariant 0 <= i <= size;
      loop invariant \forall integer k; 0 <= k < i ==> p[k] == v;
      loop assigns i;
      loop variant size - i;
    */
    for (i = 0; i < size; i++) {
        if (p[i] != v) { return false; }
    }
    return true;
}

/**
 * @brief Returns true if every byte in a region is zero
 *
 * NULL or size == 0 → false.
 */
/*@
  requires ptr == \null || size == 0 ||
           mem_valid_read((void *)ptr, (integer)size);
  assigns  \nothing;
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
/*@
  requires a == \null || b == \null || size == 0 ||
           (size <= CANON_MEM_SWAP_MAX &&
            mem_valid_write(a, (integer)size) &&
            mem_valid_write(b, (integer)size));
  requires a == \null || b == \null || size == 0 ||
           !regions_overlap((char *)a, (char *)b, (integer)size);

  behavior null_or_zero:
    assumes a == \null || b == \null || size == 0;
    assigns \nothing;

  behavior swap:
    assumes a != \null && b != \null && size > 0;
    assigns ((char *)a)[0 .. size - 1], ((char *)b)[0 .. size - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_swap(void* a, void* b, usize size) {
    u8 tmp[CANON_MEM_SWAP_MAX];
    if (!a || !b || (size == 0u)) { return; }
    require_msg(size <= CANON_MEM_SWAP_MAX,
                "mem_swap: size exceeds CANON_MEM_SWAP_MAX — use mem_swap_buf");
    require_msg(!mem_regions_overlap(a, b, size),
                "mem_swap: regions overlap");
    (void)memcpy(tmp, a,   size);
    (void)memcpy(a,   b,   size);
    (void)memcpy(b,   tmp, size);
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
/*@
  requires a == \null || b == \null || scratch == \null || size == 0 ||
           (scratch_len >= size &&
            mem_valid_write(a, (integer)size) &&
            mem_valid_write(b, (integer)size) &&
            mem_valid_write(scratch, (integer)size));
  requires a == \null || b == \null || scratch == \null || size == 0 ||
           !regions_overlap((char *)a, (char *)b, (integer)size);
  requires a == \null || b == \null || scratch == \null || size == 0 ||
           !regions_overlap((char *)a, (char *)scratch, (integer)size);
  requires a == \null || b == \null || scratch == \null || size == 0 ||
           !regions_overlap((char *)b, (char *)scratch, (integer)size);

  behavior null_or_zero:
    assumes a == \null || b == \null || scratch == \null || size == 0;
    assigns \nothing;

  behavior swap:
    assumes a != \null && b != \null && scratch != \null && size > 0;
    assigns ((char *)a)[0 .. size - 1],
            ((char *)b)[0 .. size - 1],
            ((char *)scratch)[0 .. size - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_swap_buf(void* a, void* b, usize size,
                                void* scratch, usize scratch_len) {
    /* scratch_len is referenced only by require_msg below. Under
     * -DCANON_NO_REQUIRE (used by the coverage CI job to keep MC/DC
     * measurement aligned with the WP proof), require_msg expands to
     * nothing and scratch_len becomes unreferenced. The explicit cast
     * silences -Wunused-parameter -Werror without generating any code. */
    (void)scratch_len;
    if (!a || !b || !scratch || (size == 0u)) { return; }
    require_msg(scratch_len >= size,             "mem_swap_buf: scratch buffer too small");
    require_msg(!mem_regions_overlap(a, b,       size), "mem_swap_buf: a and b overlap");
    require_msg(!mem_regions_overlap(a, scratch, size), "mem_swap_buf: a and scratch overlap");
    require_msg(!mem_regions_overlap(b, scratch, size), "mem_swap_buf: b and scratch overlap");
    (void)memcpy(scratch, a,       size);
    (void)memcpy(a,       b,       size);
    (void)memcpy(b,       scratch, size);
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
/*@
  requires dest.ptr == \null || src.ptr == \null || src.len == 0 ||
           dest.len < src.len ||
           (mem_valid_write(dest.ptr, (integer)src.len) &&
            mem_valid_read((void *)src.ptr, (integer)src.len));
  requires dest.ptr == \null || src.ptr == \null || src.len == 0 ||
           dest.len < src.len ||
           !regions_overlap((char *)dest.ptr, (char *)src.ptr,
                            (integer)src.len);

  behavior null_or_zero_or_too_small:
    assumes dest.ptr == \null || src.ptr == \null || src.len == 0 ||
            dest.len < src.len;
    assigns \nothing;
    ensures \result == 0;

  behavior copy:
    assumes dest.ptr != \null && src.ptr != \null && src.len > 0 &&
            dest.len >= src.len;
    assigns ((char *)dest.ptr)[0 .. src.len - 1];
    ensures \result == src.len;

  complete behaviors;
  disjoint behaviors;
*/
static inline usize mem_copy_bytes(bytes_t dest, cbytes_t src) {
    if (!dest.ptr || !src.ptr || (src.len == 0u)) { return 0u; }
    require_msg(dest.len >= src.len, "mem_copy_bytes: dest smaller than src");
    if (dest.len < src.len) { return 0; }
    require_msg(!mem_regions_overlap(dest.ptr, src.ptr, src.len),
                "mem_copy_bytes: regions overlap — use mem_move_bytes");
    (void)memcpy(dest.ptr, src.ptr, src.len);
    return src.len;
}

/**
 * @brief Moves src into dest, handling overlapping regions
 *
 * @pre dest.len >= src.len
 * @return Bytes moved, or 0 on failure
 */
/*@
  requires dest.ptr == \null || src.ptr == \null || src.len == 0 ||
           dest.len < src.len ||
           (mem_valid_write(dest.ptr, (integer)src.len) &&
            mem_valid_read((void *)src.ptr, (integer)src.len));

  behavior null_or_zero_or_too_small:
    assumes dest.ptr == \null || src.ptr == \null || src.len == 0 ||
            dest.len < src.len;
    assigns \nothing;
    ensures \result == 0;

  behavior move:
    assumes dest.ptr != \null && src.ptr != \null && src.len > 0 &&
            dest.len >= src.len;
    assigns ((char *)dest.ptr)[0 .. src.len - 1];
    ensures \result == src.len;

  complete behaviors;
  disjoint behaviors;
*/
static inline usize mem_move_bytes(bytes_t dest, cbytes_t src) {
    if (!dest.ptr || !src.ptr || (src.len == 0u)) { return 0u; }
    require_msg(dest.len >= src.len, "mem_move_bytes: dest smaller than src");
    if (dest.len < src.len) { return 0; }
    (void)memmove(dest.ptr, src.ptr, src.len);
    return src.len;
}

/**
 * @brief Zero-fills an entire bytes_t region
 */
/*@
  requires b.ptr == \null || b.len == 0 ||
           mem_valid_write(b.ptr, (integer)b.len);

  behavior null_or_zero:
    assumes b.ptr == \null || b.len == 0;
    assigns \nothing;

  behavior zero:
    assumes b.ptr != \null && b.len > 0;
    assigns ((char *)b.ptr)[0 .. b.len - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_zero_bytes(bytes_t b) {
    if (!b.ptr || (b.len == 0u)) { return; }
    (void)memset(b.ptr, 0, b.len);
}

/**
 * @brief Fills a bytes_t region with a repeated byte value
 */
/*@
  requires b.ptr == \null || b.len == 0 ||
           mem_valid_write(b.ptr, (integer)b.len);

  behavior null_or_zero:
    assumes b.ptr == \null || b.len == 0;
    assigns \nothing;

  behavior set:
    assumes b.ptr != \null && b.len > 0;
    assigns ((char *)b.ptr)[0 .. b.len - 1];

  complete behaviors;
  disjoint behaviors;
*/
static inline void mem_set_bytes(bytes_t b, int value) {
    if (!b.ptr || (b.len == 0u)) { return; }
    (void)memset(b.ptr, value, b.len);
}

/**
 * @brief Returns true if two cbytes_t regions are byte-for-byte identical
 *
 * Different lengths → false. Both empty (len == 0, ptr may differ) → true.
 *
 * @note On the compare path both regions must contain initialized bytes
 *       (ACSL \initialized precondition; the Valgrind CI job enforces
 *       the same property at runtime).
 */
/*@
  requires a.ptr == \null || b.ptr == \null || a.len == 0 || b.len == 0 ||
           a.ptr == b.ptr ||
           (mem_valid_read((void *)a.ptr, (integer)a.len) &&
            mem_valid_read((void *)b.ptr, (integer)b.len));
  assigns  \nothing;

  behavior different_lengths:
    assumes a.len != b.len;
    ensures \result == \false;

  behavior same_pointer:
    assumes a.len == b.len && a.ptr == b.ptr;
    ensures \result == \true;

  behavior one_null:
    assumes a.len == b.len && a.ptr != b.ptr &&
            (a.ptr == \null || b.ptr == \null);
    ensures \result == \false;

  behavior zero_len:
    assumes a.len == b.len && a.ptr != b.ptr &&
            a.ptr != \null && b.ptr != \null && a.len == 0;
    ensures \result == \true;

  behavior compare:
    assumes a.len == b.len && a.ptr != b.ptr &&
            a.ptr != \null && b.ptr != \null && a.len > 0;
    requires init_a: \initialized((char*)a.ptr + (0 .. (integer)a.len - 1));
    requires init_b: \initialized((char*)b.ptr + (0 .. (integer)b.len - 1));

  complete behaviors;
  disjoint behaviors;
*/
static inline bool mem_equal_bytes(cbytes_t a, cbytes_t b) {
    if (a.len != b.len) { return false; }
    if (a.ptr == b.ptr) { return true; }            /* covers both-NULL with equal len */
    if (!a.ptr || !b.ptr) { return false; }
    if (a.len == 0u) { return true; }
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/**
 * @brief Returns true if every byte in a cbytes_t region is zero
 *
 * NULL ptr or len == 0 → false.
 */
/*@
  requires b.ptr == \null || b.len == 0 ||
           mem_valid_read((void *)b.ptr, (integer)b.len);
  assigns  \nothing;
*/
static inline bool mem_is_zero_bytes(cbytes_t b) {
    return mem_is_all(b.ptr, 0, b.len);
}

/**
 * @brief Securely zero-fills a bytes_t region (prevents compiler elimination)
 */
/*@
  requires b.ptr == \null || b.len == 0 ||
           mem_valid_write(b.ptr, (integer)b.len);

  behavior null_or_zero:
    assumes b.ptr == \null || b.len == 0;
    assigns \nothing;

  behavior secure_zero:
    assumes b.ptr != \null && b.len > 0;
    assigns ((char *)b.ptr)[0 .. b.len - 1];

  complete behaviors;
  disjoint behaviors;
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
 * @brief Allocates an array of count objects of Type on the heap.
 *
 * Verified-safe: routes through mem_alloc_array_checked, which uses
 * checked_mul to detect sizeof(Type) * count overflow before calling
 * the underlying allocator. Returns NULL on overflow, on count == 0,
 * or on allocation failure.
 *
 * Caller must free with mem_free().
 *
 * Verification scope: per the slice.h precedent (VERIFY-007 macros),
 * this macro is documented but not directly WP-verified. Its body is
 * Type-independent except for sizeof(Type), and its correctness is a
 * transitive consequence of mem_alloc_array_checked's verified
 * behavior plus C's sizeof semantics. Validated by testing on a
 * representative type (i32) and through the overflow test cases
 * in memory_test.c.
 *
 * Spec (transitively verified through mem_alloc_array_checked):
 *   - sizeof(Type) > 0  (always true for valid types)
 *   - count == 0                            → NULL
 *   - sizeof(Type) * count overflows usize  → NULL
 *   - malloc fails                          → NULL
 *   - otherwise                             → typed pointer to sizeof(Type)*count bytes
 */
#define mem_alloc_array(Type, count) \
    ((Type*)mem_alloc_array_checked(sizeof(Type), (count)))

#endif /* CANON_CORE_MEMORY_H */
