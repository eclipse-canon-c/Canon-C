// algo/search.h
#ifndef CANON_ALGO_SEARCH_H
#define CANON_ALGO_SEARCH_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include "algo/sort.h"  // For algo_cmp_fn typedef

/**
 * @file search.h
 * @brief Binary search utilities for sorted sequences
 *
 * Provides efficient binary search operations on generic sorted arrays.
 * Implements classic binary search variants: lower_bound, upper_bound,
 * equal_range, and simple existence checks. All functions require input
 * arrays to be sorted according to the same comparison function used for
 * searching.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, SIZE_MAX)
 *   - Depends on sort.h from this library (for algo_cmp_fn typedef)
 *   - Uses statement expressions (GNU extension) in macros
 *   - Define CANON_NO_GNU_EXTENSIONS to disable macro versions
 *
 * Thread-safety: Functions are pure (no shared state) - safe to call from
 *                multiple threads on different data
 *
 * Performance:
 *   - Time complexity: O(log n) for all search operations
 *   - Space complexity: O(1) - no allocations, iterative implementation
 *   - Comparisons: At most log₂(n) + 1 comparisons
 *   - Branch prediction friendly (iterative vs recursive)
 *   - Cache-friendly for arrays that fit in cache
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Logarithmic search on sorted data — O(log n) vs O(n) linear search
 * - Multiple search variants for different use cases
 * - Generic byte-level implementation (any element type)
 * - Custom comparator + optional context for flexible ordering
 * - Safe: NULL checks, bounds validation, SIZE_MAX for "not found"
 * - Strongly typed macros for compile-time safety
 * - Zero allocations — pure algorithmic operations
 * - Perfect companion to algo_sort for sorted container operations
 * - Classic workflow: sort → search → process
 * - Compatible with vec.h and other container types
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
 * - Precondition validation in algorithms
 *
 * Usage examples:
 *
 * Basic exact match search:
 * ```c
 * int numbers[] = {1, 3, 5, 7, 9, 11, 13, 15};
 * int key = 7;
 * 
 * size_t idx = ALGO_LOWER_BOUND_TYPED(numbers, 8, int, &key, 
 *                                     algo_cmp_int, NULL);
 * if (idx != SIZE_MAX) {
 *     printf("Found %d at index %zu\n", numbers[idx], idx);
 * } else {
 *     printf("Not found\n");
 * }
 * ```
 *
 * Simple existence check:
 * ```c
 * int scores[] = {65, 72, 85, 90, 95};
 * int target = 85;
 * 
 * if (ALGO_BINARY_SEARCH_TYPED(scores, 5, int, &target, 
 *                               algo_cmp_int, NULL)) {
 *     printf("Score exists!\n");
 * }
 * ```
 *
 * Finding insertion point:
 * ```c
 * int sorted[] = {1, 3, 5, 7, 9};
 * int new_value = 6;
 * 
 * size_t insert_pos = algo_lower_bound(sorted, 5, sizeof(int), 
 *                                       &new_value, algo_cmp_int, NULL);
 * // insert_pos = 3 (between 5 and 7)
 * // Can insert at this position to maintain sort order
 * ```
 *
 * Finding all duplicates (equal range):
 * ```c
 * int data[] = {1, 2, 2, 2, 5, 7, 9};
 * int key = 2;
 * 
 * size_t range[2];
 * ALGO_EQUAL_RANGE_TYPED(data, 7, int, &key, algo_cmp_int, NULL, range);
 * 
 * printf("Elements equal to %d: ", key);
 * for (size_t i = range[0]; i < range[1]; i++) {
 *     printf("%d ", data[i]);
 * }
 * printf("\n");
 * // Output: Elements equal to 2: 2 2 2
 * ```
 *
 * Case-insensitive string search:
 * ```c
 * int cmp_string_ci(const void* a, const void* b, void* ctx) {
 *     return strcasecmp(*(const char**)a, *(const char**)b);
 * }
 *
 * const char* names[] = {"alice", "Bob", "charlie", "Dave"};
 * const char* key = "BOB";
 * 
 * size_t pos = ALGO_LOWER_BOUND_TYPED(names, 4, const char*, &key, 
 *                                     cmp_string_ci, NULL);
 * if (pos != SIZE_MAX) {
 *     printf("Found: %s\n", names[pos]);
 * }
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Core binary search operations
   ──────────────────────────────────────────────────────────────────────────── */

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
 * @param array      Pointer to first element of sorted array (NULL-safe)
 * @param len        Number of elements in array
 * @param elem_size  Size of each element in bytes (sizeof(Type))
 * @param key        Pointer to the search key (NULL-safe)
 * @param cmp        Comparison function (must match sort order)
 * @param ctx        Optional context passed to comparator (NULL-safe)
 *
 * @return           Index of exact match, or SIZE_MAX if not found
 *                   To get insertion point even when not found, use
 *                   algo_lower_bound_insert() instead
 *
 * Preconditions:
 *   - If array != NULL: array is sorted according to cmp
 *   - elem_size > 0
 *   - cmp is a valid, consistent comparison function
 *   - Same cmp was used to sort the array
 *
 * Postconditions:
 *   - If return != SIZE_MAX: array[return] == key (exact match)
 *   - If return == SIZE_MAX: key not found in array
 *   - Array is unchanged (read-only operation)
 *
 * Performance:
 *   - Time: O(log n) - at most ⌈log₂(n)⌉ + 1 comparisons
 *   - Space: O(1) - no allocations, iterative (no stack depth)
 *   - Best case: O(1) when key is at midpoint
 *   - Worst case: O(log n) guaranteed
 *
 * Example:
 *   int arr[] = {1, 3, 5, 7, 9, 11};
 *   int key = 7;
 *   size_t idx = algo_lower_bound(arr, 6, sizeof(int), &key, 
 *                                  algo_cmp_int, NULL);
 *   // idx = 3 (arr[3] == 7)
 *
 *   key = 8;
 *   idx = algo_lower_bound(arr, 6, sizeof(int), &key, algo_cmp_int, NULL);
 *   // idx = SIZE_MAX (8 not found)
 */
static inline size_t algo_lower_bound(
    const void* array,
    size_t len,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx
) {
    assert(elem_size > 0 && "algo_lower_bound: elem_size must be > 0");
    assert(cmp != NULL && "algo_lower_bound: cmp function cannot be NULL");
    
    // Handle NULL or empty cases
    if (!array || !key || len == 0) {
        return SIZE_MAX;
    }
    
    const char* bytes = (const char*)array;
    size_t low = 0;
    size_t high = len;
    
    // Binary search: find first position where array[i] >= key
    while (low < high) {
        size_t mid = low + (high - low) / 2;  // Avoid overflow
        const void* elem = bytes + mid * elem_size;
        
        int result = cmp(elem, key, ctx);
        if (result < 0) {
            // elem < key: search right half
            low = mid + 1;
        } else {
            // elem >= key: search left half (including mid)
            high = mid;
        }
    }
    
    // Now low == high == first position where array[i] >= key
    // Verify exact match
    if (low < len) {
        const void* found = bytes + low * elem_size;
        if (cmp(found, key, ctx) == 0) {
            return low;  // Exact match
        }
    }
    
    return SIZE_MAX;  // Not found
}

/**
 * @brief Finds insertion point (returns valid index even when not found)
 *
 * Similar to algo_lower_bound, but returns a valid insertion index
 * even when the key is not found. Never returns SIZE_MAX.
 *
 * @param array      Pointer to first element of sorted array (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param key        Pointer to search key (NULL-safe)
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 *
 * @return           Index where key should be inserted (0 to len inclusive)
 *                   Returns 0 if array is NULL
 *
 * Example:
 *   int arr[] = {1, 3, 5, 7, 9};
 *   int key = 6;
 *   size_t pos = algo_lower_bound_insert(arr, 5, sizeof(int), &key,
 *                                         algo_cmp_int, NULL);
 *   // pos = 3 (insert between 5 and 7)
 */
static inline size_t algo_lower_bound_insert(
    const void* array,
    size_t len,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx
) {
    assert(elem_size > 0 && "algo_lower_bound_insert: elem_size must be > 0");
    assert(cmp != NULL && "algo_lower_bound_insert: cmp function cannot be NULL");
    
    if (!array || !key || len == 0) {
        return 0;
    }
    
    const char* bytes = (const char*)array;
    size_t low = 0;
    size_t high = len;
    
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const void* elem = bytes + mid * elem_size;
        
        if (cmp(elem, key, ctx) < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    
    return low;  // Always valid: 0 <= low <= len
}

/**
 * @brief Finds the first position where element > key (upper_bound)
 *
 * Returns the first index where array[i] > key. This is useful for
 * finding the end of a range of equal elements, or the insertion
 * point when duplicates should go after existing equal elements.
 *
 * @param array      Pointer to first element of sorted array (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param key        Pointer to search key (NULL-safe)
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 *
 * @return           Index of first element > key (0 to len inclusive)
 *                   Returns 0 if array is NULL or all elements <= key
 *
 * Performance: O(log n) time, O(1) space
 *
 * Example:
 *   int arr[] = {1, 2, 2, 2, 5, 7};
 *   int key = 2;
 *   size_t pos = algo_upper_bound(arr, 6, sizeof(int), &key,
 *                                  algo_cmp_int, NULL);
 *   // pos = 4 (first element > 2 is at index 4: value 5)
 */
static inline size_t algo_upper_bound(
    const void* array,
    size_t len,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx
) {
    assert(elem_size > 0 && "algo_upper_bound: elem_size must be > 0");
    assert(cmp != NULL && "algo_upper_bound: cmp function cannot be NULL");
    
    if (!array || !key || len == 0) {
        return 0;
    }
    
    const char* bytes = (const char*)array;
    size_t low = 0;
    size_t high = len;
    
    // Find first position where array[i] > key
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const void* elem = bytes + mid * elem_size;
        
        if (cmp(elem, key, ctx) <= 0) {
            // elem <= key: search right half
            low = mid + 1;
        } else {
            // elem > key: search left half
            high = mid;
        }
    }
    
    return low;  // First position where array[i] > key
}

/**
 * @brief Finds range of all elements equal to key
 *
 * Returns [lower, upper) range containing all elements equal to key.
 * If key not found, lower == upper (empty range).
 *
 * @param array      Pointer to first element of sorted array (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param key        Pointer to search key (NULL-safe)
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 * @param range_out  Output array [2]: [0]=lower, [1]=upper (NULL-safe)
 *
 * Postconditions:
 *   - range_out[0] = lower_bound(key)
 *   - range_out[1] = upper_bound(key)
 *   - If found: range_out[0] < range_out[1]
 *   - If not found: range_out[0] == range_out[1]
 *
 * Performance: O(log n) time, O(1) space
 *
 * Example:
 *   int arr[] = {1, 2, 2, 2, 5, 7, 9};
 *   int key = 2;
 *   size_t range[2];
 *   algo_equal_range(arr, 7, sizeof(int), &key, algo_cmp_int, NULL, range);
 *   // range[0] = 1, range[1] = 4
 *   // Elements arr[1..3] all equal 2
 */
static inline void algo_equal_range(
    const void* array,
    size_t len,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx,
    size_t range_out[2]
) {
    assert(elem_size > 0 && "algo_equal_range: elem_size must be > 0");
    assert(cmp != NULL && "algo_equal_range: cmp function cannot be NULL");
    
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
 * @param array      Pointer to first element of sorted array (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param key        Pointer to search key (NULL-safe)
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 *
 * @return           true if exact match exists, false otherwise
 *
 * Performance: O(log n) time, O(1) space
 *
 * Example:
 *   int arr[] = {1, 3, 5, 7, 9};
 *   int key = 5;
 *   bool exists = algo_binary_search(arr, 5, sizeof(int), &key,
 *                                     algo_cmp_int, NULL);
 *   // exists = true
 */
static inline bool algo_binary_search(
    const void* array,
    size_t len,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx
) {
    return algo_lower_bound(array, len, elem_size, key, cmp, ctx) != SIZE_MAX;
}

/* ────────────────────────────────────────────────────────────────────────────
   Vector variants for containers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Binary search in vector structure
 *
 * Specialized version for vec-like structures with items and len fields.
 *
 * @param vec_ptr    Pointer to vector structure (NULL-safe)
 * @param elem_size  Size of each element in bytes
 * @param key        Pointer to search key (NULL-safe)
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 *
 * @return           Index of match or SIZE_MAX if not found
 */
static inline size_t algo_lower_bound_vec(
    const void* vec_ptr,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx
) {
    if (!vec_ptr) return SIZE_MAX;
    
    typedef struct {
        void* items;
        size_t len;
    } vec_like;
    
    const vec_like* vec = (const vec_like*)vec_ptr;
    
    if (!vec->items || vec->len == 0) {
        return SIZE_MAX;
    }
    
    return algo_lower_bound(vec->items, vec->len, elem_size, key, cmp, ctx);
}

/**
 * @brief Boolean existence check in vector
 */
static inline bool algo_binary_search_vec(
    const void* vec_ptr,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx
) {
    return algo_lower_bound_vec(vec_ptr, elem_size, key, cmp, ctx) != SIZE_MAX;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (recommended for type safety)
   ──────────────────────────────────────────────────────────────────────────── */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Type-safe lower bound search
 *
 * Returns index of exact match or SIZE_MAX if not found.
 *
 * @param array     Sorted array of Type
 * @param len       Number of elements
 * @param Type      Element type (used for sizeof)
 * @param key       Pointer to search key (Type*)
 * @param cmp_expr  Comparison function expression
 * @param ctx       Optional context pointer
 *
 * @return          Index or SIZE_MAX
 *
 * Example:
 *   int arr[] = {1, 3, 5, 7, 9};
 *   int key = 5;
 *   size_t idx = ALGO_LOWER_BOUND_TYPED(arr, 5, int, &key, 
 *                                       algo_cmp_int, NULL);
 */
#define ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp_expr, ctx) \
    ({ \
        size_t _idx = SIZE_MAX; \
        if ((array) && (key)) { \
            _idx = algo_lower_bound((array), (len), sizeof(Type), (key), \
                                   (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _idx; \
    })

/**
 * @brief Type-safe insertion point search
 *
 * Returns valid insertion index (never SIZE_MAX).
 */
#define ALGO_LOWER_BOUND_INSERT_TYPED(array, len, Type, key, cmp_expr, ctx) \
    ({ \
        size_t _idx = 0; \
        if ((array) && (key)) { \
            _idx = algo_lower_bound_insert((array), (len), sizeof(Type), (key), \
                                          (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _idx; \
    })

/**
 * @brief Type-safe upper bound search
 */
#define ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp_expr, ctx) \
    ({ \
        size_t _idx = 0; \
        if ((array) && (key)) { \
            _idx = algo_upper_bound((array), (len), sizeof(Type), (key), \
                                   (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _idx; \
    })

/**
 * @brief Type-safe equal range search
 *
 * @param range_out  size_t[2] array to receive [lower, upper)
 */
#define ALGO_EQUAL_RANGE_TYPED(array, len, Type, key, cmp_expr, ctx, range_out) \
    do { \
        if ((array) && (key) && (range_out)) { \
            algo_equal_range((array), (len), sizeof(Type), (key), \
                           (algo_cmp_fn)(cmp_expr), (ctx), (range_out)); \
        } \
    } while (0)

/**
 * @brief Type-safe boolean existence check
 *
 * Returns true if exact match exists, false otherwise.
 *
 * Example:
 *   if (ALGO_BINARY_SEARCH_TYPED(scores, 10, int, &target, 
 *                                 algo_cmp_int, NULL)) {
 *       printf("Found!\n");
 *   }
 */
#define ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp_expr, ctx) \
    ({ \
        bool _found = false; \
        if ((array) && (key)) { \
            _found = algo_binary_search((array), (len), sizeof(Type), (key), \
                                       (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _found; \
    })

/**
 * @brief Type-safe vector search
 */
#define ALGO_LOWER_BOUND_VEC(vec, Type, key, cmp_expr, ctx) \
    ({ \
        size_t _idx = SIZE_MAX; \
        if ((vec).items && (key)) { \
            _idx = algo_lower_bound((vec).items, (vec).len, sizeof(Type), (key), \
                                   (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _idx; \
    })

/**
 * @brief Type-safe vector existence check
 */
#define ALGO_BINARY_SEARCH_VEC(vec, Type, key, cmp_expr, ctx) \
    (ALGO_LOWER_BOUND_VEC(vec, Type, key, cmp_expr, ctx) != SIZE_MAX)

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/search.h"
    #include "algo/sort.h"
    #include <stdio.h>
    #include <string.h>
    
    // Example 1: Basic exact match search
    void example_basic_search(void) {
        int numbers[] = {1, 3, 5, 7, 9, 11, 13, 15};
        size_t len = 8;
        int key = 7;
        
        size_t idx = ALGO_LOWER_BOUND_TYPED(numbers, len, int, &key,
                                            algo_cmp_int, NULL);
        if (idx != SIZE_MAX) {
            printf("Found %d at index %zu\n", numbers[idx], idx);
        } else {
            printf("%d not found\n", key);
        }
        // Output: Found 7 at index 3
    }
    
    // Example 2: Simple existence check
    void example_existence(void) {
        int scores[] = {65, 72, 85, 90, 95};
        int target = 85;
        
        if (ALGO_BINARY_SEARCH_TYPED(scores, 5, int, &target,
                                      algo_cmp_int, NULL)) {
            printf("Score %d exists!\n", target);
        } else {
            printf("Score %d not found\n", target);
        }
    }
    
    // Example 3: Finding insertion point
    void example_insertion_point(void) {
        int sorted[] = {1, 3, 5, 7, 9};
        int new_values[] = {0, 4, 6, 10};
        
        for (size_t i = 0; i < 4; i++) {
            size_t pos = ALGO_LOWER_BOUND_INSERT_TYPED(sorted, 5, int,
                                                        &new_values[i],
                                                        algo_cmp_int, NULL);
            printf("Insert %d at position %zu\n", new_values[i], pos);
        }
        // Output:
        // Insert 0 at position 0
        // Insert 4 at position 2
        // Insert 6 at position 3
        // Insert 10 at position 5
    }
    
    // Example 4: Finding all duplicates (equal range)
    void example_equal_range(void) {
        int data[] = {1, 2, 2, 2, 5, 7, 7, 9};
        int key = 2;
        size_t range[2];
        
        ALGO_EQUAL_RANGE_TYPED(data, 8, int, &key, algo_cmp_int, NULL, range);
        
        printf("Elements equal to %d (indices %zu to %zu): ",
               key, range[0], range[1] - 1);
        for (size_t i = range[0]; i < range[1]; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
        // Output: Elements equal to 2 (indices 1 to 3): 2 2 2
        
        // Try with non-existent key
        key = 3;
        ALGO_EQUAL_RANGE_TYPED(data, 8, int, &key, algo_cmp_int, NULL, range);
        if (range[0] == range[1]) {
            printf("No elements equal to %d\n", key);
        }
    }
    
    // Example 5: Case-insensitive string search
    int cmp_string_ci(const void* a, const void* b, void* ctx) {
        (void)ctx;
        return strcasecmp(*(const char**)a, *(const char**)b);
    }
    
    void example_string_search(void) {
        // Must be sorted case-insensitively!
        const char* names[] = {"alice", "Bob", "charlie", "Dave", "Eve"};
        const char* keys[] = {"BOB", "frank", "ALICE"};
        
        for (size_t i = 0; i < 3; i++) {
            size_t idx = ALGO_LOWER_BOUND_TYPED(names, 5, const char*, 
                                                &keys[i], cmp_string_ci, NULL);
            if (idx != SIZE_MAX) {
                printf("Found '%s' (searched for '%s') at index %zu\n",
                       names[idx], keys[i], idx);
            } else {
                printf("'%s' not found\n", keys[i]);
            }
        }
        // Output:
        // Found 'alice' (searched for 'ALICE') at index 0
        // 'frank' not found
        // Found 'Bob' (searched for 'BOB') at index 1
    }
    
    // Example 6: Using with vectors
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_vector_search(void) {
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        // Add sorted data
        int data[] = {1, 3, 5, 7, 9, 11, 13, 15};
        for (int i = 0; i < 8; i++) {
            vec_int_push(&numbers, data[i]);
        }
        
        int key = 9;
        if (ALGO_BINARY_SEARCH_VEC(numbers, int, &key, algo_cmp_int, NULL)) {
            printf("Found %d in vector\n", key);
        }
        
        // Find insertion point
        key = 6;
        size_t pos = ALGO_LOWER_BOUND_INSERT_TYPED(numbers.items, numbers.len,
                                                    int, &key, algo_cmp_int, NULL);
        printf("Insert %d at position %zu\n", key, pos);
    }
    
    // Example 7: Finding closest elements
    void example_closest_elements(void) {
        int arr[] = {10, 20, 30, 40, 50, 60, 70};
        int key = 35;
        
        size_t pos = ALGO_LOWER_BOUND_INSERT_TYPED(arr, 7, int, &key,
                                                    algo_cmp_int, NULL);
        
        printf("Closest elements to %d:\n", key);
        if (pos > 0) {
            printf("  Predecessor: %d (at index %zu)\n", arr[pos-1], pos-1);
        }
        if (pos < 7) {
            printf("  Successor: %d (at index %zu)\n", arr[pos], pos);
        }
        // Output:
        // Closest elements to 35:
        //   Predecessor: 30 (at index 2)
        //   Successor: 40 (at index 3)
    }
    
    // Example 8: Range queries
    void example_range_query(void) {
        int data[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
        int min_val = 20;
        int max_val = 40;
        
        size_t start = ALGO_LOWER_BOUND_INSERT_TYPED(data, 10, int, &min_val,
                                                      algo_cmp_int, NULL);
        size_t end = ALGO_UPPER_BOUND_TYPED(data, 10, int, &max_val,
                                            algo_cmp_int, NULL);
        
        printf("Elements in range [%d, %d]: ", min_val, max_val);
        for (size_t i = start; i < end; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
        // Output: Elements in range [20, 40]: 20 25 30 35 40
    }
    
    // Example 9: Searching in sorted structs
    struct Record {
        int id;
        const char* name;
    };
    
    int cmp_record_by_id(const void* a, const void* b, void* ctx) {
        (void)ctx;
        const struct Record* r1 = (const struct Record*)a;
        const struct Record* r2 = (const struct Record*)b;
        return (r1->id > r2->id) - (r1->id < r2->id);
    }
    
    void example_struct_search(void) {
        struct Record records[] = {
            {101, "Alice"},
            {205, "Bob"},
            {310, "Charlie"},
            {415, "Dave"}
        };
        
        struct Record key = {205, NULL};  // Search by id only
        
        size_t idx = ALGO_LOWER_BOUND_TYPED(records, 4, struct Record, &key,
                                            cmp_record_by_id, NULL);
        if (idx != SIZE_MAX) {
            printf("Found record: ID=%d, Name=%s\n",
                   records[idx].id, records[idx].name);
        }
        // Output: Found record: ID=205, Name=Bob
    }
    
    // Example 10: Search with context
    struct SearchContext {
        int offset;
        bool absolute;
    };
    
    int cmp_with_offset(const void* a, const void* b, void* ctx) {
        int val_a = *(const int*)a;
        int val_b = *(const int*)b;
        struct SearchContext* sc = (struct SearchContext*)ctx;
        
        if (sc->absolute) {
            val_a = val_a < 0 ? -val_a : val_a;
            val_b = val_b < 0 ? -val_b : val_b;
        }
        
        val_a += sc->offset;
        val_b += sc->offset;
        
        return (val_a > val_b) - (val_a < val_b);
    }
    
    void example_context_search(void) {
        int data[] = {-10, -5, 0, 5, 10, 15, 20};
        
        struct SearchContext ctx = {.offset = 5, .absolute = false};
        int key = 0;
        
        // First sort with same context
        int temp[7];
        algo_sort(data, 7, sizeof(int), cmp_with_offset, &ctx, temp);
        
        // Then search with same context
        size_t idx = ALGO_LOWER_BOUND_TYPED(data, 7, int, &key,
                                            cmp_with_offset, &ctx);
        if (idx != SIZE_MAX) {
            printf("Found with offset context\n");
        }
    }
    
    // Example 11: Counting occurrences
    void example_count_occurrences(void) {
        int data[] = {1, 2, 2, 2, 3, 3, 5, 7, 7, 7, 7, 9};
        int key = 7;
        size_t range[2];
        
        ALGO_EQUAL_RANGE_TYPED(data, 12, int, &key, algo_cmp_int, NULL, range);
        
        size_t count = range[1] - range[0];
        printf("Value %d appears %zu times\n", key, count);
        // Output: Value 7 appears 4 times
    }
    
    // Example 12: Verifying sorted before search
    void example_verify_sorted(void) {
        int data[] = {1, 5, 3, 9, 2};  // NOT sorted!
        
        if (!ALGO_IS_SORTED_TYPED(data, 5, int, algo_cmp_int, NULL)) {
            printf("Array not sorted, sorting first...\n");
            ALGO_SORT_TYPED(data, 5, int, algo_cmp_int, NULL);
        }
        
        // Now safe to search
        int key = 3;
        if (ALGO_BINARY_SEARCH_TYPED(data, 5, int, &key, algo_cmp_int, NULL)) {
            printf("Found %d after sorting\n", key);
        }
    }
    
    // Example 13: Finding median using search
    void example_median_search(void) {
        int data[] = {1, 3, 5, 7, 9, 11, 13};
        size_t len = 7;
        
        // Median is at len/2
        int median_value = data[len / 2];
        
        // Verify we can find it
        size_t idx = ALGO_LOWER_BOUND_TYPED(data, len, int, &median_value,
                                            algo_cmp_int, NULL);
        printf("Median %d found at index %zu\n", median_value, idx);
    }
    
    // Example 14: Binary search in custom comparator order
    int cmp_reverse(const void* a, const void* b, void* ctx) {
        (void)ctx;
        int x = *(const int*)a;
        int y = *(const int*)b;
        return (y > x) - (y < x);  // Reversed!
    }
    
    void example_reverse_search(void) {
        int data[] = {90, 70, 50, 30, 10};  // Sorted descending
        int key = 50;
        
        // Must use same reversed comparator
        size_t idx = ALGO_LOWER_BOUND_TYPED(data, 5, int, &key,
                                            cmp_reverse, NULL);
        if (idx != SIZE_MAX) {
            printf("Found %d in reverse-sorted array at index %zu\n",
                   data[idx], idx);
        }
        // Output: Found 50 in reverse-sorted array at index 2
    }
    
    // Example 15: Practical use - symbol table lookup
    struct Symbol {
        const char* name;
        int address;
    };
    
    int cmp_symbol_by_name(const void* a, const void* b, void* ctx) {
        (void)ctx;
        const struct Symbol* s1 = (const struct Symbol*)a;
        const struct Symbol* s2 = (const struct Symbol*)b;
        return strcmp(s1->name, s2->name);
    }
    
    void example_symbol_table(void) {
        // Sorted by name
        struct Symbol symbols[] = {
            {"display", 0x1000},
            {"input", 0x2000},
            {"main", 0x3000},
            {"process", 0x4000}
        };
        
        const char* lookup_name = "process";
        struct Symbol key = {lookup_name, 0};
        
        size_t idx = ALGO_LOWER_BOUND_TYPED(symbols, 4, struct Symbol, &key,
                                            cmp_symbol_by_name, NULL);
        if (idx != SIZE_MAX) {
            printf("Symbol '%s' at address 0x%X\n",
                   symbols[idx].name, symbols[idx].address);
        }
        // Output: Symbol 'process' at address 0x4000
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_SEARCH_H */
