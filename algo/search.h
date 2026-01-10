// algo/search.h
#ifndef CANON_ALGO_SEARCH_H
#define CANON_ALGO_SEARCH_H
#include <stddef.h>

/**
 * @file search.h
 * @brief Binary search utilities for sorted sequences
 *
 * Provides efficient binary search operations (lower-bound style)
 * on generic sorted arrays. All functions require the input array
 * to be sorted according to the same comparison function.
 *
 * Core ideas:
 * - Zero-allocation, pure algorithmic
 * - Generic — works with any element type and size
 * - Safe: bounds checking, NULL protection
 * - Returns SIZE_MAX on not found (very explicit failure mode)
 * - Supports custom comparator with context pointer
 * - Strongly-typed convenience macros with expression-based comparators
 * - Designed for high-performance inner loops
 * - Compatible with vec.h style containers
 *
 * Typical use cases:
 * - Fast lookup of exact matches in sorted data
 * - Checking existence of elements efficiently
 * - Finding insertion points (lower_bound behavior)
 * - Implementing sorted containers / symbol tables
 * - Precondition checking in algorithms
 *
 * Usage examples:
 *
 * Typed search (exact match):
 * ```c
 * int numbers[] = {1, 3, 5, 7, 9, 11};
 * size_t idx = ALGO_LOWER_BOUND_TYPED(numbers, 6, int, &target, cmp_int, NULL);
 * if (idx != SIZE_MAX) {
 *     // found at numbers[idx]
 * }
 * ```
 *
 * Simple existence check:
 * ```c
 * if (ALGO_BINARY_SEARCH_TYPED(scores, count, int, &target_score, cmp_int, NULL)) {
 *     printf("Score exists!\n");
 * }
 * ```
 *
 * With context (custom comparison):
 * ```c
 * int cmp_string_case_insensitive(const void* a, const void* b, void* ctx) {
 *     return strcasecmp(*(const char**)a, *(const char**)b);
 * }
 *
 * const char* names[] = {"Alice", "bob", "Charlie", "dave"};
 * const char* key = "Bob";
 * size_t pos = ALGO_LOWER_BOUND_TYPED(names, 4, const char*, &key, cmp_string_case_insensitive, NULL);
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic lower-bound binary search (core implementation)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Finds the first position where key could be inserted without breaking order
 *        (or exact match position if exists)
 *
 * Classic lower_bound implementation.
 * Returns the smallest index i where cmp(array[i], key) >= 0,
 * or SIZE_MAX if key is not found (when used for exact match).
 *
 * @param array      Pointer to the first element of the sorted array
 * @param len        Number of elements in the array
 * @param elem_size  Size of each element in bytes (sizeof(Type))
 * @param key        Pointer to the value being searched for
 * @param cmp        Comparison function: returns <0 if a<b, 0 if a==b, >0 if a>b
 * @param ctx        Optional context pointer passed to comparator
 *
 * @return           Index of the first element >= key, or SIZE_MAX if not found
 *                   (when checking for exact match, compare again at returned index)
 */
static inline size_t algo_lower_bound(
    const void* array,
    size_t len,
    size_t elem_size,
    const void* key,
    algo_cmp_fn cmp,
    void* ctx
) {
    size_t low = 0;
    size_t high = len;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const void* elem = (const char*)array + mid * elem_size;

        int c = cmp(elem, key, ctx);
        if (c < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    // Now low == high == insertion point
    // For exact match check: verify cmp(array[low], key) == 0
    if (low < len && cmp((const char*)array + low * elem_size, key, ctx) == 0) {
        return low;
    }

    return SIZE_MAX;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (recommended)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe lower bound search — returns index or SIZE_MAX
 *
 * @param array     Sorted array of Type
 * @param len       Number of elements
 * @param Type      Element type (used for sizeof)
 * @param key       Pointer to the search key (same type as array elements)
 * @param cmp_expr  Expression that evaluates to comparison function:
 *                  int (*)(const void*, const void*, void*)
 * @param ctx       Optional context pointer (NULL if not needed)
 *
 * @return          Index of exact match or SIZE_MAX if not found
 */
#define ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp_expr, ctx) \
    ({ \
        size_t _idx = SIZE_MAX; \
        if ((array)) { \
            _idx = algo_lower_bound((array), (len), sizeof(Type), (key), \
                                   (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _idx; \
    })

/**
 * @brief Simple boolean existence check using binary search
 *
 * Thin wrapper around ALGO_LOWER_BOUND_TYPED for readability.
 *
 * @return true if exact match exists, false otherwise
 */
#define ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp_expr, ctx) \
    (ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp_expr, ctx) != SIZE_MAX)

#endif /* CANON_ALGO_SEARCH_H */
