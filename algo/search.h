#ifndef CANON_ALGO_SEARCH_H
#define CANON_ALGO_SEARCH_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "core/primitives/compare.h"   // provides algo_cmp_fn

/**
 * @file algo/search.h
 * @brief Binary search utilities for sorted sequences
 *
 * Provides efficient binary search operations on generic sorted arrays.
 * Implements classic binary search variants: lower_bound, upper_bound,
 * equal_range, and simple existence checks. All functions require input
 * arrays to be sorted according to the same comparison function used for
 * searching.
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
 * - Statement expressions (GNU extension) used in macros
 * - Define CANON_NO_GNU_EXTENSIONS to disable expression macro versions
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
 * - Space complexity: O(1) - no allocations, iterative implementation
 * - Comparisons: At most ⌈log₂(n)⌉ + 1 comparisons
 * - Branch prediction friendly (iterative vs recursive)
 * - Cache-friendly for arrays that fit in cache
 *
 * Search variants explained:
 * ────────────────────────────────────────────────────────────────────────────
 * - lower_bound: First position where element >= key (insertion point)
 * - upper_bound: First position where element > key (end of equal range)
 * - binary_search: Boolean check - does exact match exist?
 * - equal_range: Returns [lower, upper) range of all equal elements
 *
 * Visual example with array [1, 2, 2, 2, 5, 7, 9] searching for key=2:
 * Indices: 0 1 2 3 4 5 6
 * Values: [1, 2, 2, 2, 5, 7, 9]
 * ↑ ↑
 * lower_bound(2) = 1 (first ≥ 2)
 * upper_bound(2) = 4 (first > 2)
 * equal_range(2) = [1, 4) (all elements equal to 2)
 * binary_search(2) = true (exists)
 *
 * Searching for key=3 (not present):
 * lower_bound(3) = 4 (insertion point to maintain order)
 * upper_bound(3) = 4 (same as lower when not found)
 * equal_range(3) = [4, 4) (empty range)
 * binary_search(3) = false (does not exist)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fast exact match lookup in sorted data
 * - Checking existence of elements efficiently
 * - Finding insertion points to maintain sort order
 * - Range queries: finding all elements equal to a key
 * - Implementing sorted containers, symbol tables, dictionaries
 * - Database-like operations: WHERE, JOIN with sorted keys
 * - Finding closest elements (predecessor/successor)
 * - Merging sorted sequences
 * - Duplicate detection in sorted data
 *
 * Quick start:
 * ```c
 * #include "algo/search.h"
 *
 * int numbers[] = {1, 3, 5, 7, 9, 11, 13, 15};
 * int key = 7;
 *
 * usize idx = algo_lower_bound(numbers, 8, sizeof(int), &key, algo_cmp_int, NULL);
 * if (idx != CANON_USIZE_MAX) {
 *     printf("Found %d at index %zu\n", numbers[idx], idx);
 * }
 *
 * // Typed macro version
 * idx = ALGO_LOWER_BOUND_TYPED(numbers, 8, int, &key, algo_cmp_int, NULL);
 *
 * // Simple existence check
 * if (algo_binary_search(numbers, 8, sizeof(int), &key, algo_cmp_int, NULL)) {
 *     printf("Found!\n");
 * }
 *
 * // Finding insertion point
 * int new_val = 6;
 * usize pos = algo_lower_bound_insert(numbers, 8, sizeof(int), &new_val,
 *                                     algo_cmp_int, NULL);
 * // pos = 3 (insert between 5 and 7)
 *
 * // Equal range
 * int arr[] = {1, 2, 2, 2, 5, 7, 9};
 * usize range[2];
 * algo_equal_range(arr, 7, sizeof(int), &key, algo_cmp_int, NULL, range);
 * // range = {1, 4} - all 2's are at indices [1, 4)
 * ```
 *
 * @sa core/primitives/compare.h — algo_cmp_fn definition
 * @sa core/slice.h — slice_##type used by DEFINE_ALGO_SEARCH
 * @sa core/ownership.h — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Public interface
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Finds the first position where key could be inserted (lower_bound)
 *
 * Classic lower_bound implementation - returns the first index where
 * array[i] >= key. This is the insertion point to maintain sorted order.
 * If an exact match exists, returns its index (or the first index if
 * there are duplicates).
 *
 * The array MUST be sorted according to the comparison function.
 *
 * Algorithm:
 * - Binary search with iterative implementation
 * - Maintains invariant: all elements before 'low' are < key
 * - All elements at or after 'high' are >= key
 * - Converges to the first position >= key
 *
 * @param array Pointer to first element of sorted array (borrowed, read-only)
 * @param len Number of elements in array
 * @param elem_size Size of each element in bytes (> 0)
 * @param key Pointer to the search key (borrowed, read-only)
 * @param cmp Comparison function (borrowed — must match sort order)
 * @param ctx Optional context passed to comparator (borrowed, may be NULL)
 *
 * @return Index of exact match, or CANON_USIZE_MAX if not found
 * To get insertion point even when not found, use
 * algo_lower_bound_insert() instead
 *
 * @pre elem_size > 0
 * @pre If array != NULL, array is sorted according to cmp
 * @pre If cmp != NULL, cmp is a valid, consistent comparison function
 * @pre Same cmp was used to sort the array
 *
 * @post If return != CANON_USIZE_MAX: array[return] == key (exact match)
 * @post If return == CANON_USIZE_MAX: key not found in array
 * @post Array is unchanged (read-only operation)
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - key: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp, not stored)
 *
 * Performance:
 * - Time: O(log n) - at most ⌈log₂(n)⌉ + 1 comparisons
 * - Space: O(1) - no allocations, iterative (no stack depth)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
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
    require_msg(cmp != NULL, "algo_lower_bound: cmp function cannot be NULL");
   
    /* Handle NULL or empty cases */
    if (!array || !key || len == 0) {
        return CANON_USIZE_MAX;
    }
   
    const u8* bytes = (const u8*)array;
    usize low = 0;
    usize high = len;
   
    /* Binary search: find first position where array[i] >= key */
    while (low < high) {
        usize mid = low + (high - low) / 2; /* Avoid overflow */
        const void* elem = bytes + mid * elem_size;
       
        int result = cmp(elem, key, ctx);
        if (result < 0) {
            /* elem < key: search right half */
            low = mid + 1;
        } else {
            /* elem >= key: search left half (including mid) */
            high = mid;
        }
    }
   
    /* Now low == high == first position where array[i] >= key */
    /* Verify exact match */
    if (low < len) {
        const void* found = bytes + low * elem_size;
        if (cmp(found, key, ctx) == 0) {
            return low; /* Exact match */
        }
    }
   
    return CANON_USIZE_MAX; /* Not found */
}

/* ───────────────────────────────────────────────────────────────────────────
   The rest of the file remains completely unchanged
   ─────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_SEARCH_H */
