#ifndef CANON_ALGO_SEARCH_H
#define CANON_ALGO_SEARCH_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "core/primitives/compare.h"   /* provides algo_cmp_fn */

/**
 * @file algo/search.h
 * @brief Binary search utilities for sorted sequences
 *
 * Provides efficient binary search operations on generic sorted arrays.
 * All functions require input arrays to be sorted according to the same
 * comparison function used for searching.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Logarithmic search on sorted data — O(log n) vs O(n) linear search
 * - Multiple search variants for different use cases
 * - Generic byte-level implementation (any element type)
 * - Custom comparator + optional context for flexible ordering
 * - Safe: NULL checks, bounds validation
 * - Strongly typed macros for compile-time safety
 * - Zero allocations — pure algorithmic operations
 * - Perfect companion to algo_sort for sorted container operations
 * - Classic workflow: sort → search → process
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Search function summary:
 * ────────────────────────────────────────────────────────────────────────────
 * - algo_lower_bound():  First index where array[i] >= key.
 *                        Returns position even when key is absent.
 *                        Returns 0 for empty arrays.
 *                        Never returns CANON_USIZE_MAX.
 *                        Use for insertion point / range queries.
 *                        Equivalent to C++ std::lower_bound.
 *
 * - algo_upper_bound():  First index where array[i] > key.
 *                        Returns position even when key is absent.
 *                        Returns 0 for empty arrays.
 *                        Never returns CANON_USIZE_MAX.
 *                        Equivalent to C++ std::upper_bound.
 *
 * - algo_find_sorted():  Returns index of first EXACT match, or
 *                        CANON_USIZE_MAX if key is not present.
 *                        Use for lookup by key.
 *
 * - algo_binary_search(): Boolean existence check — does key exist?
 *
 * - algo_equal_range():  Returns [lower, upper) range of all equal elements.
 *
 * Precondition handling:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions treat NULL pointers and elem_size == 0 as programmer errors
 * and fire require_msg (always-on panic).
 *
 * len == 0 is valid input for all functions:
 * - algo_lower_bound():   returns 0 (insert at beginning)
 * - algo_upper_bound():   returns 0 (insert at beginning)
 * - algo_find_sorted():   returns CANON_USIZE_MAX (key not found)
 * - algo_binary_search(): returns false (not found)
 * - algo_equal_range():   writes [0, 0) (empty range)
 *
 * Important: Arrays MUST be sorted!
 * ────────────────────────────────────────────────────────────────────────────
 * Binary search only works correctly on sorted arrays. Using these functions
 * on unsorted data produces undefined results (not crashes, just wrong answers).
 *
 * Always use the SAME comparator for sorting and searching:
 * ✓ algo_sort(arr, len, sizeof(T), cmp_func, ctx, temp);
 * ✓ algo_lower_bound(arr, len, sizeof(T), &key, cmp_func, ctx);
 *
 * ✗ algo_sort(arr, len, sizeof(T), cmp_ascending, NULL, temp);
 * ✗ algo_lower_bound(arr, len, sizeof(T), &key, cmp_descending, NULL);
 * // Different comparators = wrong results!
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - No allocations anywhere in this file
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions are thread-safe (no shared mutable state)
 * - Safe to call concurrently on different arrays from multiple threads
 * - Same array should not be modified concurrently without external synchronization
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(log n) for all search operations
 * - Space complexity: O(1) — no allocations, iterative implementation
 * - Comparisons: At most ⌈log₂(n)⌉ + 1 comparisons
 * - Branch prediction friendly (iterative vs recursive)
 * - Cache-friendly for arrays that fit in cache
 *
 * Visual example with array [1, 2, 2, 2, 5, 7, 9] searching for key=2:
 * Indices: 0  1  2  3  4  5  6
 * Values: [1, 2, 2, 2, 5, 7, 9]
 *
 * lower_bound(2)   = 1              (first index where array[i] >= 2)
 * lower_bound(3)   = 4              (insertion point even when absent)
 * upper_bound(2)   = 4              (first index where array[i] > 2)
 * find_sorted(2)   = 1              (first exact match)
 * equal_range(2)   = [1, 4)         (all elements equal to 2)
 * binary_search(2) = true           (exists)
 *
 * Searching for key=3 (not present):
 * lower_bound(3)   = 4              (insertion point to maintain order)
 * upper_bound(3)   = 4              (same as lower_bound when absent)
 * find_sorted(3)   = CANON_USIZE_MAX  (not found)
 * equal_range(3)   = [4, 4)         (empty range)
 * binary_search(3) = false          (does not exist)
 *
 * Empty array (len=0):
 * lower_bound      = 0
 * upper_bound      = 0
 * find_sorted      = CANON_USIZE_MAX
 * equal_range      = [0, 0)
 * binary_search    = false
 *
 * Quick start:
 * ```c
 * #include "algo/search.h"
 *
 * int numbers[] = {1, 3, 5, 7, 9, 11, 13, 15};
 * int key = 7;
 *
 * // Insertion point — works even when key is absent, even for len==0
 * int new_val = 6;
 * usize pos = algo_lower_bound(numbers, 8, sizeof(int), &new_val,
 *                              algo_cmp_int, NULL);
 * // pos = 3 (insert between 5 and 7)
 *
 * // Exact match lookup — CANON_USIZE_MAX if not found
 * usize idx = algo_find_sorted(numbers, 8, sizeof(int), &key,
 *                              algo_cmp_int, NULL);
 * if (idx != CANON_USIZE_MAX) {
 *     printf("Found %d at index %zu\n", numbers[idx], idx);
 * }
 *
 * // Simple existence check
 * if (algo_binary_search(numbers, 8, sizeof(int), &key, algo_cmp_int, NULL)) {
 *     printf("Found!\n");
 * }
 *
 * // Equal range
 * int arr[] = {1, 2, 2, 2, 5, 7, 9};
 * int key2 = 2;
 * usize range[2];
 * algo_equal_range(arr, 7, sizeof(int), &key2, algo_cmp_int, NULL, range);
 * // range = {1, 4} — all 2's are at indices [1, 4)
 * ```
 *
 * @sa core/primitives/compare.h — algo_cmp_fn definition
 * @sa core/slice.h — slice_##type used by DEFINE_ALGO_SEARCH
 * @sa core/ownership.h — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Internal helper — shared lower_bound implementation
   Returns first index where array[i] >= key. Always returns a valid
   position in [0, len] — never CANON_USIZE_MAX. Called by algo_lower_bound,
   algo_find_sorted, and algo_equal_range.
   ════════════════════════════════════════════════════════════════════════════ */
static inline usize algo_lower_bound_impl(
    const void* array,
    usize len,
    usize elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx)
{
    const u8* bytes = (const u8*)array;
    usize low  = 0;
    usize high = len;

    while (low < high) {
        usize mid        = low + (high - low) / 2;
        const void* elem = bytes + mid * elem_size;
        if (cmp(elem, key, ctx) < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

/* ════════════════════════════════════════════════════════════════════════════
   Public interface
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Finds the insertion point for key — first index where array[i] >= key
 *
 * Standard lower_bound: returns the first position where key could be
 * inserted to keep the array sorted. Returns this position even when key
 * is not present in the array.
 *
 * len == 0 is valid — returns 0 (correct insertion point for empty array).
 * Never returns CANON_USIZE_MAX.
 *
 * Equivalent to C++ std::lower_bound.
 *
 * The array MUST be sorted according to the comparison function.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements in array (0 is valid)
 * @param elem_size Size of each element in bytes (> 0)
 * @param key       Pointer to the search key (borrowed, read-only)
 * @param cmp       Comparison function (borrowed — must match sort order)
 * @param ctx       Optional context passed to comparator (borrowed, may be NULL)
 *
 * @return First index where array[i] >= key, in range [0, len].
 *         Returns len if all elements are < key.
 *         Returns 0 if len == 0 (empty array).
 *         Never returns CANON_USIZE_MAX.
 *
 * @pre elem_size > 0 — triggers require_msg (always-on panic)
 * @pre cmp != NULL   — triggers require_msg (always-on panic)
 * @pre array != NULL — triggers require_msg (always-on panic)
 * @pre key != NULL   — triggers require_msg (always-on panic)
 * @pre array is sorted according to cmp
 *
 * @post return value is in [0, len]
 * @post If return < len: array[return] >= key
 * @post If return > 0:   array[return - 1] < key
 * @post Array is unchanged (read-only operation)
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 */
static inline usize algo_lower_bound(
    borrowed const void* array,
    usize len,
    usize elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void* ctx)
{
    require_msg(elem_size > 0, "algo_lower_bound: elem_size must be > 0");
    require_msg(cmp   != NULL, "algo_lower_bound: cmp function cannot be NULL");
    require_msg(array != NULL, "algo_lower_bound: array cannot be NULL");
    require_msg(key   != NULL, "algo_lower_bound: key cannot be NULL");

    if (len == 0) return 0;

    return algo_lower_bound_impl(array, len, elem_size, key, cmp, ctx);
}

/**
 * @brief Finds the first position where array[i] > key — standard upper_bound
 *
 * Returns the first index where array[i] > key. Combined with
 * algo_lower_bound(), gives the half-open range [lower, upper) of all
 * elements equal to key.
 *
 * len == 0 is valid — returns 0.
 * Never returns CANON_USIZE_MAX.
 *
 * Equivalent to C++ std::upper_bound.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements in array (0 is valid)
 * @param elem_size Size of each element in bytes (> 0)
 * @param key       Pointer to the search key (borrowed, read-only)
 * @param cmp       Comparison function (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @return First index where array[i] > key, in range [0, len].
 *         Returns len if all elements are <= key.
 *         Returns 0 if len == 0 (empty array).
 *         Never returns CANON_USIZE_MAX.
 *
 * @pre elem_size > 0 — triggers require_msg (always-on panic)
 * @pre cmp != NULL   — triggers require_msg (always-on panic)
 * @pre array != NULL — triggers require_msg (always-on panic)
 * @pre key != NULL   — triggers require_msg (always-on panic)
 * @pre array is sorted according to cmp
 *
 * @post return value is in [0, len]
 * @post If return < len: array[return] > key
 * @post If return > 0:   array[return - 1] <= key
 * @post Array is unchanged (read-only operation)
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 */
static inline usize algo_upper_bound(
    borrowed const void* array,
    usize len,
    usize elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void* ctx)
{
    require_msg(elem_size > 0, "algo_upper_bound: elem_size must be > 0");
    require_msg(cmp   != NULL, "algo_upper_bound: cmp function cannot be NULL");
    require_msg(array != NULL, "algo_upper_bound: array cannot be NULL");
    require_msg(key   != NULL, "algo_upper_bound: key cannot be NULL");

    if (len == 0) return 0;

    const u8* bytes = (const u8*)array;
    usize low  = 0;
    usize high = len;

    while (low < high) {
        usize mid        = low + (high - low) / 2;
        const void* elem = bytes + mid * elem_size;
        if (cmp(elem, key, ctx) <= 0) {
            low = mid + 1;  /* elem <= key: search right */
        } else {
            high = mid;     /* elem > key:  search left  */
        }
    }
    return low;
}

/**
 * @brief Finds the index of the first exact match in a sorted array
 *
 * Internally delegates to algo_lower_bound() then verifies an exact match.
 * Returns CANON_USIZE_MAX if the key is not present.
 *
 * Unlike algo_lower_bound(), this function does NOT return an insertion
 * point — it either finds an exact match or signals failure via
 * CANON_USIZE_MAX. Use algo_lower_bound() when you need the insertion
 * point regardless of whether the key exists.
 *
 * The array MUST be sorted according to the comparison function.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements in array (0 is valid — returns CANON_USIZE_MAX)
 * @param elem_size Size of each element in bytes (> 0)
 * @param key       Pointer to the search key (borrowed, read-only)
 * @param cmp       Comparison function (borrowed — must match sort order)
 * @param ctx       Optional context passed to comparator (borrowed, may be NULL)
 *
 * @return Index of first exact match, or CANON_USIZE_MAX if not found.
 *
 * @pre elem_size > 0 — triggers require_msg (always-on panic)
 * @pre cmp != NULL   — triggers require_msg (always-on panic)
 * @pre array != NULL — triggers require_msg (always-on panic)
 * @pre key != NULL   — triggers require_msg (always-on panic)
 * @pre array is sorted according to cmp
 *
 * @post If return != CANON_USIZE_MAX: array[return] == key (exact match)
 * @post If return == CANON_USIZE_MAX: key not found (or len == 0)
 * @post Array is unchanged (read-only operation)
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 */
static inline usize algo_find_sorted(
    borrowed const void* array,
    usize len,
    usize elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void* ctx)
{
    require_msg(elem_size > 0, "algo_find_sorted: elem_size must be > 0");
    require_msg(cmp   != NULL, "algo_find_sorted: cmp function cannot be NULL");
    require_msg(array != NULL, "algo_find_sorted: array cannot be NULL");
    require_msg(key   != NULL, "algo_find_sorted: key cannot be NULL");

    if (len == 0) return CANON_USIZE_MAX;

    usize pos = algo_lower_bound_impl(array, len, elem_size, key, cmp, ctx);

    if (pos < len) {
        const void* found = (const u8*)array + pos * elem_size;
        if (cmp(found, key, ctx) == 0) {
            return pos; /* exact match */
        }
    }

    return CANON_USIZE_MAX; /* not found */
}

/**
 * @brief Boolean existence check — does an exact match exist?
 *
 * Returns true if at least one element compares equal to key.
 * Delegates to algo_find_sorted() internally.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements in array (0 is valid — returns false)
 * @param elem_size Size of each element in bytes (> 0)
 * @param key       Pointer to the search key (borrowed, read-only)
 * @param cmp       Comparison function (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @return true if key is found, false otherwise
 *
 * @pre elem_size > 0 — triggers require_msg (always-on panic)
 * @pre cmp != NULL   — triggers require_msg (always-on panic)
 * @pre array != NULL — triggers require_msg (always-on panic)
 * @pre key != NULL   — triggers require_msg (always-on panic)
 * @pre array is sorted according to cmp
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 */
static inline bool algo_binary_search(
    borrowed const void* array,
    usize len,
    usize elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void* ctx)
{
    return algo_find_sorted(array, len, elem_size, key, cmp, ctx) != CANON_USIZE_MAX;
}

/**
 * @brief Returns the half-open range [lower, upper) of all elements equal to key
 *
 * Combines algo_lower_bound() and algo_upper_bound() to find the range of
 * all elements equal to key. Writes two indices into out_range:
 *   out_range[0] = first index where array[i] >= key  (lower bound)
 *   out_range[1] = first index where array[i] >  key  (upper bound)
 *
 * The range [out_range[0], out_range[1]) contains all elements equal to key.
 * If key is not present, out_range[0] == out_range[1] (empty range).
 * If len == 0, writes [0, 0).
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements in array (0 is valid — writes [0, 0))
 * @param elem_size Size of each element in bytes (> 0)
 * @param key       Pointer to the search key (borrowed, read-only)
 * @param cmp       Comparison function (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 * @param out_range Output array of exactly 2 usize values (owned by caller)
 *
 * @pre elem_size > 0    — triggers require_msg (always-on panic)
 * @pre cmp != NULL      — triggers require_msg (always-on panic)
 * @pre array != NULL    — triggers require_msg (always-on panic)
 * @pre key != NULL      — triggers require_msg (always-on panic)
 * @pre out_range != NULL — triggers require_msg (always-on panic)
 * @pre array is sorted according to cmp
 *
 * @post out_range[0] <= out_range[1]
 * @post out_range[0] and out_range[1] are both in [0, len]
 * @post All elements in [out_range[0], out_range[1]) are equal to key
 * @post If out_range[0] == out_range[1]: key not present in array
 * @post Array is unchanged (read-only operation)
 *
 * Performance:
 * - Time:  O(log n) — two independent binary searches
 * - Space: O(1)
 */
static inline void algo_equal_range(
    borrowed const void* array,
    usize len,
    usize elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void* ctx,
    usize out_range[2])
{
    require_msg(elem_size > 0,      "algo_equal_range: elem_size must be > 0");
    require_msg(cmp       != NULL,  "algo_equal_range: cmp function cannot be NULL");
    require_msg(array     != NULL,  "algo_equal_range: array cannot be NULL");
    require_msg(key       != NULL,  "algo_equal_range: key cannot be NULL");
    require_msg(out_range != NULL,  "algo_equal_range: out_range cannot be NULL");

    if (len == 0) {
        out_range[0] = 0;
        out_range[1] = 0;
        return;
    }

    out_range[0] = algo_lower_bound_impl(array, len, elem_size, key, cmp, ctx);
    out_range[1] = algo_upper_bound(array, len, elem_size, key, cmp, ctx);
}

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe insertion-point lookup — always returns position in [0, len]
 */
#define ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    algo_lower_bound((array), (usize)(len), sizeof(Type), \
                     (key), (algo_cmp_fn)(cmp), (ctx))

/**
 * @def ALGO_FIND_SORTED_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe exact-match lookup — returns CANON_USIZE_MAX if not found
 */
#define ALGO_FIND_SORTED_TYPED(array, len, Type, key, cmp, ctx) \
    algo_find_sorted((array), (usize)(len), sizeof(Type), \
                     (key), (algo_cmp_fn)(cmp), (ctx))

/**
 * @def ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe boolean existence check
 */
#define ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx) \
    algo_binary_search((array), (usize)(len), sizeof(Type), \
                       (key), (algo_cmp_fn)(cmp), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_SEARCH — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type).
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates search functions that accept slice_##type directly
 *
 * Prerequisites:
 * - DEFINE_SLICE(type) must have been called
 *
 * Generated functions:
 * - algo_lower_bound_slice_##type(sv, key, cmp, ctx)
 *     → usize: insertion point in [0, sv.len] — never CANON_USIZE_MAX
 * - algo_find_sorted_slice_##type(sv, key, cmp, ctx)
 *     → usize: first exact match index, or CANON_USIZE_MAX if not found
 * - algo_binary_search_slice_##type(sv, key, cmp, ctx)
 *     → bool: true if exact match exists
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 */
#define DEFINE_ALGO_SEARCH(type) \
\
static inline usize algo_lower_bound_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type* key, \
    borrowed algo_cmp_fn cmp, \
    borrowed void* ctx) \
{ \
    return algo_lower_bound(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline usize algo_find_sorted_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type* key, \
    borrowed algo_cmp_fn cmp, \
    borrowed void* ctx) \
{ \
    return algo_find_sorted(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline bool algo_binary_search_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type* key, \
    borrowed algo_cmp_fn cmp, \
    borrowed void* ctx) \
{ \
    return algo_binary_search(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
}

#endif /* CANON_ALGO_SEARCH_H */
