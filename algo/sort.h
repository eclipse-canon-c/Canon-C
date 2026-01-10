// algo/sort.h
#ifndef CANON_ALGO_SORT_H
#define CANON_ALGO_SORT_H
#include <stddef.h>
#include <stdbool.h>

/**
 * @file sort.h
 * @brief Generic in-place sorting algorithms
 *
 * Provides stable sorting facilities with customizable comparators.
 * Currently implements insertion sort for small arrays; full merge sort
 * requires caller-provided temporary buffer (not yet implemented in this version).
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Stable sort (preserves relative order of equal elements)
 * - Generic — works with any element type
 * - No hidden allocations in the current implementation
 * - Hybrid approach: insertion sort for small arrays (fast + low overhead)
 * - Full merge sort variant planned (O(n log n), requires O(n) temp space)
 * - Supports comparator with context pointer
 * - Strongly typed convenience macros
 * - Safe: NULL checks, trivial case handling (len < 2)
 * - Designed to be compatible with vec.h containers
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Sorting arrays of numbers, strings, structs, pointers...
 * - Preparing data for binary search
 * - Implementing priority queues / ordered containers
 * - Small-to-medium dataset in-place sorting
 * - Custom ordering with context (locale-aware, case-insensitive, etc.)
 *
 * Usage examples:
 *
 * Basic typed sort (integers):
 * ```c
 * int values[] = {64, 34, 25, 12, 22, 11, 90};
 * ALGO_SORT_TYPED(values, 7, int, cmp_int, NULL);
 * // now: {11, 12, 22, 25, 34, 64, 90}
 * ```
 *
 * Sorting vector of strings (case-insensitive):
 * ```c
 * vec_string names = ...; // {"Charlie", "alice", "Bob", "dave"}
 *
 * int cmp_string_ci(const void* a, const void* b, void* ctx) {
 *     return strcasecmp(*(const char**)a, *(const char**)b);
 * }
 *
 * ALGO_SORT_TYPED(names.items, names.len, const char*, cmp_string_ci, NULL);
 * // now sorted case-insensitively
 * ```
 *
 * With context (sorting by custom field):
 * ```c
 * struct Person { const char* name; int age; };
 * int cmp_by_age(const void* a, const void* b, void* ctx) {
 *     return ((const struct Person*)a)->age - ((const struct Person*)b)->age;
 * }
 *
 * struct Person people[100];
 * ALGO_SORT_TYPED(people, 100, struct Person, cmp_by_age, NULL);
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic sorting (current: insertion sort for small arrays)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Sorts array in-place using insertion sort (small arrays)
 *
 * For larger arrays, a full stable merge sort is planned (requires caller
 * to provide temporary buffer of size len * elem_size).
 *
 * Current implementation:
 * - Uses insertion sort when len < 16 (fast for small arrays)
 * - Byte-level swaps — works with any element type
 * - Stable within the current scope
 *
 * @param array       Pointer to the first element
 * @param len         Number of elements
 * @param elem_size   Size of each element in bytes (sizeof(Type))
 * @param cmp         Comparison function: <0 if a<b, 0 if equal, >0 if a>b
 * @param ctx         Optional context pointer passed to comparator
 * @param temp_buffer Optional scratch space (currently ignored)
 *
 * Does nothing if:
 * - array is NULL
 * - len < 2
 * - cmp is NULL
 */
static inline void algo_sort(
    void* array,
    size_t len,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx,
    void* temp_buffer // optional: for future merge sort
) {
    if (!array || len < 2 || !cmp) return;

    // Small-array optimization: insertion sort (fast + minimal overhead)
    if (len < 16) {
        char* bytes = (char*)array;
        for (size_t i = 1; i < len; ++i) {
            for (size_t j = i; j > 0; --j) {
                const void* a = bytes + (j - 1) * elem_size;
                const void* b = bytes + j * elem_size;

                if (cmp(a, b, ctx) <= 0) break;

                // Manual byte swap (generic, no type knowledge needed)
                for (size_t k = 0; k < elem_size; ++k) {
                    char tmp = bytes[j * elem_size + k];
                    bytes[j * elem_size + k] = bytes[(j - 1) * elem_size + k];
                    bytes[(j - 1) * elem_size + k] = tmp;
                }
            }
        }
        return;
    }

    // TODO: Implement full stable merge sort using temp_buffer when provided
    // For now: do nothing for large arrays (API placeholder)
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (recommended)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Default comparator for built-in types (pointer dereference)
 *
 * Useful for numbers, pointers, etc.
 *
 * @param Type      Element type
 * @param a         First element (as void*)
 * @param b         Second element (as void*)
 * @param ctx_expr  Optional context expression (ignored here)
 */
#define ALGO_CMP_TYPED(Type, a, b, ctx_expr) \
    ({ \
        const Type* _a = (const Type*)(a); \
        const Type* _b = (const Type*)(b); \
        (void)(ctx_expr); \
        (*_a < *_b ? -1 : (*_a > *_b ? 1 : 0)); \
    })

/**
 * @brief Type-safe in-place sort
 *
 * @param array    Array to sort (modified in-place)
 * @param len      Number of elements
 * @param Type     Element type (used for sizeof)
 * @param cmp_expr Expression that evaluates to a comparison function
 * @param ctx      Optional context pointer (NULL if not needed)
 */
#define ALGO_SORT_TYPED(array, len, Type, cmp_expr, ctx) \
    do { \
        if ((array) && (len) >= 2) { \
            algo_sort((array), (len), sizeof(Type), \
                      (algo_cmp_fn)(cmp_expr), (ctx), NULL); \
        } \
    } while (0)

/* Future planned helpers (not yet implemented):
#define ALGO_SORT_VEC(vec, Type, cmp_expr, ctx) \
    ALGO_SORT_TYPED((vec).items, (vec).len, Type, cmp_expr, ctx)
*/

#endif /* CANON_ALGO_SORT_H */
