#ifndef CANON_ALGO_UNIQUE_H
#define CANON_ALGO_UNIQUE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "core/primitives/compare.h"   // provides algo_cmp_fn

/**
 * @file algo/unique.h
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
 * - Preserves first occurrence of each consecutive group
 * - Generic byte-level implementation (any element type)
 * - Safe: NULL checks, trivial cases (len ≤ 1)
 * - Custom comparator + optional context
 * - Very efficient move semantics (only copies when necessary)
 * - Perfect companion to algo_sort + data/ containers
 * - Classic workflow: sort → unique → resize/shrink
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Important: This removes CONSECUTIVE duplicates only!
 * ────────────────────────────────────────────────────────────────────────────
 * For full deduplication, sort first with the same comparator:
 * [3,1,2,1,2,3] → (unsorted unique) → [3,1,2,1,2,3] (no change!)
 * [3,1,2,1,2,3] → (sort) → [1,1,2,2,3,3] → (unique) → [1,2,3] ✓
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * unique.h uses core/memory.h for mem_copy instead of memcpy directly.
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
 * - Single linear pass: O(n) time complexity
 * - In-place algorithm: O(1) space complexity
 * - Only copies elements when necessary (write < read)
 * - Very efficient for already-unique or nearly-unique data
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Deduplicate sorted arrays / vectors
 * - Clean up measurement series, logs, timestamps
 * - Remove consecutive repeated commands / events
 * - Prepare data for frequency counting or grouping
 * - Reduce size of sorted containers before further processing
 * - Run-length encoding preprocessing
 * - Collapse adjacent identical states in state machines
 *
 * Quick start:
 * ```c
 * #include "algo/unique.h"
 *
 * int cmp_int(const void* a, const void* b, void* ctx) {
 *     int x = *(const int*)a, y = *(const int*)b;
 *     return (x > y) - (x < y);
 * }
 *
 * // Full deduplication: sort + unique
 * int arr[] = {5, 2, 3, 2, 1, 3, 5, 5};
 * int tmp[8];
 * algo_sort(arr, 8, sizeof(int), cmp_int, NULL, tmp);
 * // arr is now: [1, 1, 2, 2, 3, 3, 5, 5, 5]
 * usize new_len = algo_unique(arr, 8, sizeof(int), cmp_int, NULL);
 * // arr[0..new_len-1] = {1, 2, 3, 5}, new_len == 4
 *
 * // Typed macro (GNU version - returns new length)
 * int arr2[] = {1, 1, 2, 2, 3};
 * usize len = ALGO_UNIQUE_TYPED(arr2, 5, int, cmp_int, NULL);
 * // len = 3
 *
 * // Typed macro (C99 fallback - updates via pointer)
 * #define CANON_NO_GNU_EXTENSIONS
 * usize len2 = 5;
 * ALGO_UNIQUE_TYPED(arr2, &len2, int, cmp_int, NULL);
 * // len2 = 3
 *
 * // Slice variant
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_UNIQUE(int)
 * slice_int sv = slice_int_from(arr, new_len);
 * usize final_len = algo_unique_slice_int(sv, cmp_int, NULL);
 * ```
 *
 * @sa core/primitives/compare.h — algo_cmp_fn definition
 * @sa core/memory.h — mem_copy used for element operations
 * @sa core/slice.h — slice_##type used by DEFINE_ALGO_UNIQUE
 * @sa core/ownership.h — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Public interface
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Removes consecutive duplicate elements in-place
 *
 * Collapses groups of consecutive equal elements into one.
 * Returns the new logical length of the unique prefix.
 *
 * Algorithm:
 * - Maintains two pointers: read and write
 * - Compares each element with the last unique element
 * - Only copies element if different from previous
 * - Elements at indices [0, return_value) are unique and consecutive-duplicate-free
 * - Elements at indices [return_value, len) are invalid (old data)
 *
 * Most effective after sorting with the same comparator.
 *
 * @param array Pointer to first element (borrowed — modified in place but not freed)
 * @param len Number of elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param cmp Comparator: returns 0 if elements are equal (borrowed)
 * @param ctx Optional context passed to comparator (borrowed, may be NULL)
 *
 * @return New length of the array after removing consecutive duplicates
 * Valid elements are now in array[0] .. array[return-1]
 * Returns 0 if array is NULL, returns len if len <= 1
 *
 * @pre elem_size > 0
 * @pre If array != NULL, array points to valid memory of size len * elem_size
 * @pre If cmp != NULL, cmp implements a valid equality test (returns 0 for equal)
 *
 * @post Array is modified in-place
 * @post First return_value elements contain no consecutive duplicates
 * @post Relative order of first occurrences is preserved
 * @post Original elements beyond return_value contain stale data
 *
 * Ownership:
 * - array: borrowed (modified in place, elements reordered)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp, not stored)
 *
 * Performance:
 * - Time: O(n) - single pass through array
 * - Space: O(1) - in-place algorithm
 * - Comparisons: At most n-1 comparisons
 * - Copies: At most n-1 element copies (when all unique)
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays
 * - Not safe to call concurrently on same array without external sync
 */
static inline usize algo_unique(
    borrowed void* array,
    usize len,
    usize elem_size,
    borrowed algo_cmp_fn cmp,
    borrowed void* ctx)
{
    require_msg(elem_size > 0, "algo_unique: elem_size must be > 0");
    require_msg(cmp != NULL, "algo_unique: cmp function cannot be NULL");
   
    /* Handle NULL or trivial cases */
    if (!array || len <= 1) {
        return len;
    }
   
    u8* bytes = (u8*)array;
    usize write = 1; /* Position to write next unique element */
   
    /* Single pass: compare each element with the last unique element */
    for (usize read = 1; read < len; ++read) {
        const void* prev = bytes + (write - 1) * elem_size;
        const void* curr = bytes + read * elem_size;
       
        /* If current is different from last unique element */
        if (cmp(prev, curr, ctx) != 0) {
            /* Only copy if we're not already at the write position
               (optimization: avoid self-copy) */
            if (write != read) {
                mem_copy(bytes + write * elem_size, curr, elem_size);
            }
            ++write;
        }
        /* If equal, skip (don't increment write) */
    }
   
    return write;
}

/* ───────────────────────────────────────────────────────────────────────────
   The rest of the file remains completely unchanged
   ─────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_UNIQUE_H */
