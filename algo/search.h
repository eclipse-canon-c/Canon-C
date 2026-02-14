#ifndef CANON_ALGO_SEARCH_H
#define CANON_ALGO_SEARCH_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "algo/sort.h"  // For algo_cmp_fn typedef

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
 *   ✓ algo_sort(arr, len, sizeof(T), cmp_func, ctx, temp);
 *   ✓ algo_lower_bound(arr, len, sizeof(T), &key, cmp_func, ctx);
 *
 *   ✗ algo_sort(arr, len, sizeof(T), cmp_ascending, NULL, temp);
 *   ✗ algo_lower_bound(arr, len, sizeof(T), &key, cmp_descending, NULL);
 *     // Different comparators = wrong results!
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * search.h depends on algo/sort.h only for the algo_cmp_fn typedef.
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
 *   Indices:        0  1  2  3  4  5  6
 *   Values:        [1, 2, 2, 2, 5, 7, 9]
 *                      ↑        ↑
 *   lower_bound(2) = 1  (first ≥ 2)
 *   upper_bound(2) = 4  (first > 2)
 *   equal_range(2) = [1, 4)  (all elements equal to 2)
 *   binary_search(2) = true  (exists)
 *
 *   Searching for key=3 (not present):
 *   lower_bound(3) = 4  (insertion point to maintain order)
 *   upper_bound(3) = 4  (same as lower when not found)
 *   equal_range(3) = [4, 4)  (empty range)
 *   binary_search(3) = false  (does not exist)
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
 * #include "algo/sort.h"
 *
 * int cmp_int(const void* a, const void* b, void* ctx) {
 *     int x = *(const int*)a, y = *(const int*)b;
 *     return (x > y) - (x < y);
 * }
 *
 * // Basic exact match search
 * int numbers[] = {1, 3, 5, 7, 9, 11, 13, 15};
 * int key = 7;
 * 
 * usize idx = algo_lower_bound(numbers, 8, sizeof(int), &key, cmp_int, NULL);
 * if (idx != CANON_USIZE_MAX) {
 *     printf("Found %d at index %zu\n", numbers[idx], idx);
 * }
 *
 * // Typed macro version
 * idx = ALGO_LOWER_BOUND_TYPED(numbers, 8, int, &key, cmp_int, NULL);
 *
 * // Simple existence check
 * if (algo_binary_search(numbers, 8, sizeof(int), &key, cmp_int, NULL)) {
 *     printf("Found!\n");
 * }
 *
 * // Finding insertion point
 * int new_val = 6;
 * usize pos = algo_lower_bound_insert(numbers, 8, sizeof(int), &new_val, 
 *                                     cmp_int, NULL);
 * // pos = 3 (insert between 5 and 7)
 *
 * // Equal range
 * int arr[] = {1, 2, 2, 2, 5, 7, 9};
 * usize range[2];
 * algo_equal_range(arr, 7, sizeof(int), &key, cmp_int, NULL, range);
 * // range = {1, 4} - all 2's are at indices [1, 4)
 * ```
 *
 * @sa algo/sort.h           — companion sorting algorithm
 * @sa core/slice.h           — slice_##type used by DEFINE_ALGO_SEARCH
 * @sa core/ownership.h       — borrowed macro for non-owning parameters
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
 *   - Binary search with iterative implementation
 *   - Maintains invariant: all elements before 'low' are < key
 *   - All elements at or after 'high' are >= key
 *   - Converges to the first position >= key
 *
 * @param array      Pointer to first element of sorted array (borrowed, read-only)
 * @param len        Number of elements in array
 * @param elem_size  Size of each element in bytes (> 0)
 * @param key        Pointer to the search key (borrowed, read-only)
 * @param cmp        Comparison function (borrowed — must match sort order)
 * @param ctx        Optional context passed to comparator (borrowed, may be NULL)
 *
 * @return           Index of exact match, or CANON_USIZE_MAX if not found
 *                   To get insertion point even when not found, use
 *                   algo_lower_bound_insert() instead
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
 * - Time:  O(log n) - at most ⌈log₂(n)⌉ + 1 comparisons
 * - Space: O(1) - no allocations, iterative (no stack depth)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 *
 * Example:
 * ```c
 * int arr[] = {1, 3, 5, 7, 9, 11};
 * int key = 7;
 * usize idx = algo_lower_bound(arr, 6, sizeof(int), &key, algo_cmp_int, NULL);
 * // idx = 3 (arr[3] == 7)
 *
 * key = 8;
 * idx = algo_lower_bound(arr, 6, sizeof(int), &key, algo_cmp_int, NULL);
 * // idx = CANON_USIZE_MAX (8 not found)
 * ```
 */
static inline usize algo_lower_bound(
    borrowed const void* array,
    usize                len,
    usize                elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
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
        usize mid = low + (high - low) / 2;  /* Avoid overflow */
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
            return low;  /* Exact match */
        }
    }
    
    return CANON_USIZE_MAX;  /* Not found */
}

/**
 * @brief Finds insertion point (returns valid index even when not found)
 *
 * Similar to algo_lower_bound, but returns a valid insertion index
 * even when the key is not found. Never returns CANON_USIZE_MAX.
 *
 * @param array      Pointer to first element of sorted array (borrowed, read-only)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes (> 0)
 * @param key        Pointer to search key (borrowed, read-only)
 * @param cmp        Comparison function (borrowed)
 * @param ctx        Optional context (borrowed, may be NULL)
 *
 * @return           Index where key should be inserted (0 to len inclusive)
 *                   Returns 0 if array is NULL
 *
 * @pre elem_size > 0
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - key: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 *
 * Example:
 * ```c
 * int arr[] = {1, 3, 5, 7, 9};
 * int key = 6;
 * usize pos = algo_lower_bound_insert(arr, 5, sizeof(int), &key, cmp_int, NULL);
 * // pos = 3 (insert between 5 and 7)
 * ```
 */
static inline usize algo_lower_bound_insert(
    borrowed const void* array,
    usize                len,
    usize                elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
{
    require_msg(elem_size > 0, "algo_lower_bound_insert: elem_size must be > 0");
    require_msg(cmp != NULL, "algo_lower_bound_insert: cmp function cannot be NULL");
    
    if (!array || !key || len == 0) {
        return 0;
    }
    
    const u8* bytes = (const u8*)array;
    usize low = 0;
    usize high = len;
    
    while (low < high) {
        usize mid = low + (high - low) / 2;
        const void* elem = bytes + mid * elem_size;
        
        if (cmp(elem, key, ctx) < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    
    return low;  /* Always valid: 0 <= low <= len */
}

/**
 * @brief Finds the first position where element > key (upper_bound)
 *
 * Returns the first index where array[i] > key. This is useful for
 * finding the end of a range of equal elements, or the insertion
 * point when duplicates should go after existing equal elements.
 *
 * @param array      Pointer to first element of sorted array (borrowed, read-only)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes (> 0)
 * @param key        Pointer to search key (borrowed, read-only)
 * @param cmp        Comparison function (borrowed)
 * @param ctx        Optional context (borrowed, may be NULL)
 *
 * @return           Index of first element > key (0 to len inclusive)
 *                   Returns 0 if array is NULL or all elements <= key
 *
 * @pre elem_size > 0
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - key: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 *
 * Example:
 * ```c
 * int arr[] = {1, 2, 2, 2, 5, 7};
 * int key = 2;
 * usize pos = algo_upper_bound(arr, 6, sizeof(int), &key, cmp_int, NULL);
 * // pos = 4 (first element > 2 is at index 4: value 5)
 * ```
 */
static inline usize algo_upper_bound(
    borrowed const void* array,
    usize                len,
    usize                elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
{
    require_msg(elem_size > 0, "algo_upper_bound: elem_size must be > 0");
    require_msg(cmp != NULL, "algo_upper_bound: cmp function cannot be NULL");
    
    if (!array || !key || len == 0) {
        return 0;
    }
    
    const u8* bytes = (const u8*)array;
    usize low = 0;
    usize high = len;
    
    /* Find first position where array[i] > key */
    while (low < high) {
        usize mid = low + (high - low) / 2;
        const void* elem = bytes + mid * elem_size;
        
        if (cmp(elem, key, ctx) <= 0) {
            /* elem <= key: search right half */
            low = mid + 1;
        } else {
            /* elem > key: search left half */
            high = mid;
        }
    }
    
    return low;  /* First position where array[i] > key */
}

/**
 * @brief Finds range of all elements equal to key
 *
 * Returns [lower, upper) range containing all elements equal to key.
 * If key not found, lower == upper (empty range).
 *
 * @param array      Pointer to first element of sorted array (borrowed, read-only)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes (> 0)
 * @param key        Pointer to search key (borrowed, read-only)
 * @param cmp        Comparison function (borrowed)
 * @param ctx        Optional context (borrowed, may be NULL)
 * @param range_out  Output array [2]: [0]=lower, [1]=upper (borrowed, write-only)
 *
 * @pre elem_size > 0
 *
 * @post range_out[0] = lower_bound(key)
 * @post range_out[1] = upper_bound(key)
 * @post If found: range_out[0] < range_out[1]
 * @post If not found: range_out[0] == range_out[1]
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - key: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 * - range_out: borrowed (output buffer, written to)
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 *
 * Example:
 * ```c
 * int arr[] = {1, 2, 2, 2, 5, 7, 9};
 * int key = 2;
 * usize range[2];
 * algo_equal_range(arr, 7, sizeof(int), &key, cmp_int, NULL, range);
 * // range[0] = 1, range[1] = 4
 * // Elements arr[1..3] all equal 2
 * ```
 */
static inline void algo_equal_range(
    borrowed const void* array,
    usize                len,
    usize                elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx,
    borrowed usize       range_out[2])
{
    require_msg(elem_size > 0, "algo_equal_range: elem_size must be > 0");
    require_msg(cmp != NULL, "algo_equal_range: cmp function cannot be NULL");
    
    if (!range_out) {
        return;
    }
    
    if (!array || !key || len == 0) {
        range_out[0] = 0;
        range_out[1] = 0;
        return;
    }
    
    range_out[0] = algo_lower_bound_insert(array, len, elem_size, key, cmp, ctx);
    range_out[1] = algo_upper_bound(array, len, elem_size, key, cmp, ctx);
}

/**
 * @brief Simple boolean existence check
 *
 * Checks if an exact match for key exists in the sorted array.
 * Convenient wrapper around algo_lower_bound for readability.
 *
 * @param array      Pointer to first element of sorted array (borrowed, read-only)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes (> 0)
 * @param key        Pointer to search key (borrowed, read-only)
 * @param cmp        Comparison function (borrowed)
 * @param ctx        Optional context (borrowed, may be NULL)
 *
 * @return           true if exact match exists, false otherwise
 *
 * @pre elem_size > 0
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - key: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 *
 * Example:
 * ```c
 * int arr[] = {1, 3, 5, 7, 9};
 * int key = 5;
 * bool exists = algo_binary_search(arr, 5, sizeof(int), &key, cmp_int, NULL);
 * // exists = true
 * ```
 */
static inline bool algo_binary_search(
    borrowed const void* array,
    usize                len,
    usize                elem_size,
    borrowed const void* key,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
{
    return algo_lower_bound(array, len, elem_size, key, cmp, ctx) != CANON_USIZE_MAX;
}

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════
   GNU extension versions (statement expressions) provide better ergonomics.
   C99 fallback versions available when CANON_NO_GNU_EXTENSIONS is defined.
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @def ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe lower bound search — returns index or CANON_USIZE_MAX
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * @param array Sorted array of Type (borrowed, read-only)
 * @param len   Number of elements
 * @param Type  Element type
 * @param key   Pointer to search key (Type*) (borrowed, read-only)
 * @param cmp   algo_cmp_fn comparator (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return Index of exact match or CANON_USIZE_MAX if not found
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 *
 * Example:
 * ```c
 * int arr[] = {1, 3, 5, 7, 9};
 * int key = 5;
 * usize idx = ALGO_LOWER_BOUND_TYPED(arr, 5, int, &key, cmp_int, NULL);
 * // idx = 2
 * ```
 */
#define ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    ({ \
        usize _idx = CANON_USIZE_MAX; \
        if ((array) && (key)) { \
            _idx = algo_lower_bound((array), (len), sizeof(Type), (key), \
                                   (algo_cmp_fn)(cmp), (ctx)); \
        } \
        _idx; \
    })

/**
 * @def ALGO_LOWER_BOUND_INSERT_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe insertion point search — returns valid index
 *
 * Requires: GNU C statement expressions or C23
 *
 * @return Valid insertion index (0 to len inclusive)
 */
#define ALGO_LOWER_BOUND_INSERT_TYPED(array, len, Type, key, cmp, ctx) \
    ({ \
        usize _idx = 0; \
        if ((array) && (key)) { \
            _idx = algo_lower_bound_insert((array), (len), sizeof(Type), (key), \
                                          (algo_cmp_fn)(cmp), (ctx)); \
        } \
        _idx; \
    })

/**
 * @def ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe upper bound search — returns index
 */
#define ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    ({ \
        usize _idx = 0; \
        if ((array) && (key)) { \
            _idx = algo_upper_bound((array), (len), sizeof(Type), (key), \
                                   (algo_cmp_fn)(cmp), (ctx)); \
        } \
        _idx; \
    })

/**
 * @def ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe boolean existence check — returns bool
 *
 * Example:
 * ```c
 * if (ALGO_BINARY_SEARCH_TYPED(scores, 10, int, &target, cmp_int, NULL)) {
 *     printf("Found!\n");
 * }
 * ```
 */
#define ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx) \
    ({ \
        bool _found = false; \
        if ((array) && (key)) { \
            _found = algo_binary_search((array), (len), sizeof(Type), (key), \
                                       (algo_cmp_fn)(cmp), (ctx)); \
        } \
        _found; \
    })

#else /* CANON_NO_GNU_EXTENSIONS */

/* C99 fallback versions - cannot be used in expressions */

#define ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    ((array) && (key) ? algo_lower_bound((array), (len), sizeof(Type), (key), \
                                        (algo_cmp_fn)(cmp), (ctx)) : CANON_USIZE_MAX)

#define ALGO_LOWER_BOUND_INSERT_TYPED(array, len, Type, key, cmp, ctx) \
    ((array) && (key) ? algo_lower_bound_insert((array), (len), sizeof(Type), (key), \
                                                (algo_cmp_fn)(cmp), (ctx)) : 0)

#define ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    ((array) && (key) ? algo_upper_bound((array), (len), sizeof(Type), (key), \
                                        (algo_cmp_fn)(cmp), (ctx)) : 0)

#define ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx) \
    ((array) && (key) ? algo_binary_search((array), (len), sizeof(Type), (key), \
                                          (algo_cmp_fn)(cmp), (ctx)) : false)

#endif /* CANON_NO_GNU_EXTENSIONS */

/**
 * @def ALGO_EQUAL_RANGE_TYPED(array, len, Type, key, cmp, ctx, range_out)
 * @brief Type-safe equal range search
 *
 * @param range_out usize[2] array to receive [lower, upper) (borrowed, write-only)
 *
 * Example:
 * ```c
 * int arr[] = {1, 2, 2, 2, 5};
 * int key = 2;
 * usize range[2];
 * ALGO_EQUAL_RANGE_TYPED(arr, 5, int, &key, cmp_int, NULL, range);
 * // range = {1, 4}
 * ```
 */
#define ALGO_EQUAL_RANGE_TYPED(array, len, Type, key, cmp, ctx, range_out) \
    do { \
        if ((array) && (key) && (range_out)) { \
            algo_equal_range((array), (len), sizeof(Type), (key), \
                           (algo_cmp_fn)(cmp), (ctx), (range_out)); \
        } \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_SEARCH — typed slice variant per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type).
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates search functions that accept slice_##type directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 * - algo_lower_bound_slice_##type(sv, key, cmp, ctx)        → usize
 * - algo_lower_bound_insert_slice_##type(sv, key, cmp, ctx) → usize
 * - algo_upper_bound_slice_##type(sv, key, cmp, ctx)        → usize
 * - algo_equal_range_slice_##type(sv, key, cmp, ctx, range) → void
 * - algo_binary_search_slice_##type(sv, key, cmp, ctx)      → bool
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Ownership:
 * - sv: borrowed (read-only)
 * - key: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 * - range: borrowed (output buffer, written to)
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_SEARCH(int)
 *
 * int arr[] = {1, 3, 5, 7, 9};
 * slice_int sv = slice_int_from(arr, 5);
 * int key = 5;
 *
 * usize idx = algo_lower_bound_slice_int(sv, &key, cmp_int, NULL);
 * // idx = 2
 *
 * bool exists = algo_binary_search_slice_int(sv, &key, cmp_int, NULL);
 * // exists = true
 * ```
 */
#define DEFINE_ALGO_SEARCH(type) \
\
/** \
 * @brief Finds exact match in slice_##type \
 * \
 * @param sv  Slice (borrowed, read-only) \
 * @param key Pointer to search key (borrowed, read-only) \
 * @param cmp Comparator (borrowed) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @return Index or CANON_USIZE_MAX if not found \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * @pre sv.ptr points to sorted array if non-NULL \
 * \
 * Ownership: All parameters borrowed (read-only) \
 * \
 * Performance: O(log n) time, O(1) space \
 * \
 * Thread-safety: Safe if array not being modified \
 */ \
static inline usize algo_lower_bound_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type*  key, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    if (!sv.ptr || !key || sv.len == 0 || !cmp) return CANON_USIZE_MAX; \
    return algo_lower_bound(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
/** \
 * @brief Finds insertion point in slice_##type \
 * \
 * @return Valid insertion index (0 to sv.len inclusive) \
 */ \
static inline usize algo_lower_bound_insert_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type*  key, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    if (!sv.ptr || !key || sv.len == 0 || !cmp) return 0; \
    return algo_lower_bound_insert(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
/** \
 * @brief Finds upper bound in slice_##type \
 * \
 * @return Index of first element > key \
 */ \
static inline usize algo_upper_bound_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type*  key, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    if (!sv.ptr || !key || sv.len == 0 || !cmp) return 0; \
    return algo_upper_bound(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
/** \
 * @brief Finds equal range in slice_##type \
 * \
 * @param range Output array [2]: [0]=lower, [1]=upper (borrowed, write-only) \
 */ \
static inline void algo_equal_range_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type*  key, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx, \
    borrowed usize        range[2]) \
{ \
    if (!range) return; \
    if (!sv.ptr || !key || sv.len == 0 || !cmp) { \
        range[0] = 0; \
        range[1] = 0; \
        return; \
    } \
    algo_equal_range(sv.ptr, sv.len, sizeof(type), key, cmp, ctx, range); \
} \
\
/** \
 * @brief Boolean existence check in slice_##type \
 * \
 * @return true if key exists in slice, false otherwise \
 */ \
static inline bool algo_binary_search_slice_##type( \
    borrowed slice_##type sv, \
    borrowed const type*  key, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    return algo_binary_search(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
}

#endif /* CANON_ALGO_SEARCH_H */
