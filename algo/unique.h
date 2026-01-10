// algo/unique.h
#ifndef CANON_ALGO_UNIQUE_H
#define CANON_ALGO_UNIQUE_H
#include <stddef.h>
#include "data/vec.h"      // For vec integration macros
#include "core/memory.h"   // For mem_copy (optional, can fallback to memcpy)

/**
 * @file unique.h
 * @brief Remove consecutive duplicate elements (in-place)
 *
 * Collapses runs of consecutive equal elements into a single occurrence,
 * preserving the order of first appearances.
 * Most powerful when used after sorting (→ full deduplication).
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - In-place — zero extra allocation
 * - Single linear pass → O(n) time
 * - Preserves first occurrence of each value
 * - Generic byte-level implementation (any element type)
 * - Safe: NULL checks, trivial cases (len ≤ 1)
 * - Custom comparator + optional context
 * - Very efficient move semantics (only copies when necessary)
 * - Perfect companion to algo_sort + vec containers
 * - Classic workflow: sort → unique → resize/shrink
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Deduplicate sorted arrays / vectors
 * - Clean up measurement series, logs, timestamps
 * - Remove consecutive repeated commands / events
 * - Prepare data for frequency counting or grouping
 * - Reduce size of sorted containers before further processing
 *
 * Usage examples:
 *
 * After sorting — full deduplication:
 * ```c
 * int arr[] = {1, 2, 2, 3, 3, 3, 4, 5, 5, 5};
 * ALGO_SORT_TYPED(arr, 10, int, cmp_int, NULL);
 * size_t new_len = ALGO_UNIQUE_CONSECUTIVE_TYPED(arr, 10, int, cmp_int, NULL);
 * // now arr[0..4] = {1,2,3,4,5}   new_len == 5
 * ```
 *
 * Vec workflow (most common pattern):
 * ```c
 * vec_string tokens = ...; // possibly with duplicates
 * ALGO_SORT_VEC(tokens, const char*, cmp_string, NULL);
 * ALGO_UNIQUE_CONSECUTIVE_VEC(tokens, const char*, cmp_string, NULL);
 * // tokens.len is now updated — no duplicates remain
 * vec_shrink(&tokens); // optional: reduce capacity if desired
 * ```
 *
 * Custom comparator (e.g. case-insensitive):
 * ```c
 * const char* words[] = {"Zebra", "apple", "Apple", "banana", "BANANA"};
 * // assume already sorted case-insensitively...
 * size_t unique_count = ALGO_UNIQUE_CONSECUTIVE_TYPED(
 *     words, 5, const char*,
 *     cmp_string_case_insensitive, NULL);
 * // now: {"Zebra", "apple", "banana", ...}
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic consecutive unique (single-pass, in-place)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Removes consecutive duplicate elements in-place
 *
 * Collapses groups of consecutive equal elements into one.
 * Returns the new logical length of the unique prefix.
 *
 * Most effective after sorting with the same comparator.
 *
 * @param array      Pointer to first element
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes (sizeof(Type))
 * @param cmp        Comparator: returns 0 if elements are considered equal
 * @param ctx        Optional context passed to comparator
 *
 * @return           New length of the array after removing consecutive duplicates
 *                   (valid elements are now in array[0] .. array[return-1])
 */
static inline size_t algo_unique_consecutive(
    void* array,
    size_t len,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx
) {
    if (!array || len <= 1) return len;

    char* bytes = (char*)array;
    size_t write = 1;

    for (size_t read = 1; read < len; ++read) {
        const void* prev = bytes + (write - 1) * elem_size;
        const void* curr = bytes + read * elem_size;

        if (cmp(prev, curr, ctx) != 0) {
            if (write != read) {
                mem_copy(bytes + write * elem_size, curr, elem_size);
            }
            ++write;
        }
    }

    return write;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (recommended)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe version — returns new length
 *
 * @param array     Array to modify in-place
 * @param len       Original number of elements
 * @param Type      Element type (used for sizeof)
 * @param cmp_expr  Expression that evaluates to comparison function
 * @param ctx       Optional context pointer (can be NULL)
 *
 * @return          New number of elements after deduplication
 */
#define ALGO_UNIQUE_CONSECUTIVE_TYPED(array, len, Type, cmp_expr, ctx) \
    ({ \
        size_t _new_len = (len); \
        if ((array)) { \
            _new_len = algo_unique_consecutive( \
                (array), (len), sizeof(Type), \
                (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _new_len; \
    })

/**
 * @brief In-place consecutive unique for vec containers
 *
 * Updates .len directly — very convenient pattern:
 *   sort(vec) → unique(vec) → (optional) shrink(vec)
 *
 * @param vec       Vector to process (modified in-place)
 * @param Type      Element type of the vector
 * @param cmp_expr  Comparison function expression
 * @param ctx       Optional context
 */
#define ALGO_UNIQUE_CONSECUTIVE_VEC(vec, Type, cmp_expr, ctx) \
    do { \
        if ((vec).items) { \
            size_t _new_len = algo_unique_consecutive( \
                (vec).items, (vec).len, sizeof(Type), \
                (algo_cmp_fn)(cmp_expr), (ctx)); \
            (vec).len = _new_len; \
        } \
    } while (0)

#endif /* CANON_ALGO_UNIQUE_H */
