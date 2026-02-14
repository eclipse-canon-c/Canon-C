#ifndef CANON_ALGO_UNIQUE_H
#define CANON_ALGO_UNIQUE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "algo/sort.h"  // For algo_cmp_fn typedef

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
 *   [3,1,2,1,2,3] → (unsorted unique) → [3,1,2,1,2,3] (no change!)
 *   [3,1,2,1,2,3] → (sort) → [1,1,2,2,3,3] → (unique) → [1,2,3] ✓
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * unique.h uses core/memory.h for mem_copy instead of memcpy directly.
 * unique.h depends on algo/sort.h only for the algo_cmp_fn typedef.
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
 * #include "algo/sort.h"
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
 * @sa algo/sort.h           — companion sorting algorithm
 * @sa core/memory.h          — mem_copy used for element operations
 * @sa core/slice.h           — slice_##type used by DEFINE_ALGO_UNIQUE
 * @sa core/ownership.h       — borrowed macro for non-owning parameters
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
 *   - Maintains two pointers: read and write
 *   - Compares each element with the last unique element
 *   - Only copies element if different from previous
 *   - Elements at indices [0, return_value) are unique and consecutive-duplicate-free
 *   - Elements at indices [return_value, len) are invalid (old data)
 *
 * Most effective after sorting with the same comparator.
 *
 * @param array      Pointer to first element (borrowed — modified in place but not freed)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes (> 0)
 * @param cmp        Comparator: returns 0 if elements are equal (borrowed)
 * @param ctx        Optional context passed to comparator (borrowed, may be NULL)
 *
 * @return           New length of the array after removing consecutive duplicates
 *                   Valid elements are now in array[0] .. array[return-1]
 *                   Returns 0 if array is NULL, returns len if len <= 1
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
 * - Time:  O(n) - single pass through array
 * - Space: O(1) - in-place algorithm
 * - Comparisons: At most n-1 comparisons
 * - Copies: At most n-1 element copies (when all unique)
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays
 * - Not safe to call concurrently on same array without external sync
 *
 * Example:
 * ```c
 * int arr[] = {1, 1, 2, 2, 2, 3, 4, 4, 5};
 * usize new_len = algo_unique(arr, 9, sizeof(int), cmp_int, NULL);
 * // new_len = 5, arr = {1, 2, 3, 4, 5, ...}
 * ```
 */
static inline usize algo_unique(
    borrowed void*       array,
    usize                len,
    usize                elem_size,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
{
    require_msg(elem_size > 0, "algo_unique: elem_size must be > 0");
    require_msg(cmp != NULL, "algo_unique: cmp function cannot be NULL");
    
    /* Handle NULL or trivial cases */
    if (!array || len <= 1) {
        return len;
    }
    
    u8* bytes = (u8*)array;
    usize write = 1;  /* Position to write next unique element */
    
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

/**
 * @brief Returns true if array has no consecutive duplicates (non-destructive)
 *
 * @param array      Pointer to first element (borrowed, read-only)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param cmp        Comparator (borrowed)
 * @param ctx        Optional context (borrowed, may be NULL)
 * @return true if no consecutive duplicates exist or len <= 1, false otherwise
 *
 * @pre elem_size > 0
 * @pre If array != NULL, array points to valid array of len elements
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 *
 * Example:
 * ```c
 * int arr[] = {1, 2, 3, 4, 5};
 * bool is_unique = algo_has_no_consecutive_duplicates(arr, 5, sizeof(int), cmp_int, NULL);
 * // true - array unchanged
 * ```
 */
static inline bool algo_has_no_consecutive_duplicates(
    borrowed const void* array,
    usize                len,
    usize                elem_size,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
{
    if (!array || len <= 1 || !cmp) return true;
    
    const u8* bytes = (const u8*)array;
    
    for (usize i = 1; i < len; ++i) {
        const void* prev = bytes + (i - 1) * elem_size;
        const void* curr = bytes + i * elem_size;
        
        if (cmp(prev, curr, ctx) == 0) {
            return false;  /* Found consecutive duplicates */
        }
    }
    
    return true;
}

/**
 * @brief Counts how many elements would remain after unique operation
 *
 * Non-destructive version - does not modify the array.
 * Useful for pre-allocation or progress reporting.
 *
 * @param array      Pointer to first element (borrowed, read-only)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param cmp        Comparison function (borrowed)
 * @param ctx        Optional context (borrowed, may be NULL)
 *
 * @return           Number of unique consecutive elements
 *
 * @pre elem_size > 0
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 *
 * Example:
 * ```c
 * int arr[] = {1, 1, 2, 2, 3};
 * usize count = algo_count_unique(arr, 5, sizeof(int), cmp_int, NULL);
 * // count = 3 (array unchanged)
 * ```
 */
static inline usize algo_count_unique(
    borrowed const void* array,
    usize                len,
    usize                elem_size,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
{
    require_msg(elem_size > 0, "algo_count_unique: elem_size must be > 0");
    require_msg(cmp != NULL, "algo_count_unique: cmp function cannot be NULL");
    
    if (!array || len <= 1) {
        return len;
    }
    
    const u8* bytes = (const u8*)array;
    usize count = 1;  /* First element is always unique */
    
    for (usize i = 1; i < len; ++i) {
        const void* prev = bytes + (i - 1) * elem_size;
        const void* curr = bytes + i * elem_size;
        
        if (cmp(prev, curr, ctx) != 0) {
            ++count;
        }
    }
    
    return count;
}

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════
   GNU extension versions (statement expressions) provide better ergonomics.
   C99 fallback versions available when CANON_NO_GNU_EXTENSIONS is defined.
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @def ALGO_UNIQUE_TYPED(array, len, Type, cmp, ctx)
 * @brief Type-safe in-place unique — returns new length (GNU version)
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Modifies array in-place and returns the new length.
 * This version can be used in expressions.
 *
 * @param array Array of Type to deduplicate (borrowed)
 * @param len   Current number of elements
 * @param Type  Element type
 * @param cmp   algo_cmp_fn comparator (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return New length after deduplication
 *
 * @note array is borrowed — array is modified in place but not freed
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Example:
 * ```c
 * int arr[] = {1, 1, 2, 2, 3};
 * usize new_len = ALGO_UNIQUE_TYPED(arr, 5, int, cmp_int, NULL);
 * // new_len = 3, arr = {1, 2, 3, ...}
 * ```
 */
#define ALGO_UNIQUE_TYPED(array, len, Type, cmp, ctx)                       \
    ({                                                                       \
        usize _new_len = (len);                                              \
        if ((array)) {                                                       \
            _new_len = algo_unique((array), _new_len, sizeof(Type),         \
                                   (algo_cmp_fn)(cmp), (ctx));               \
        }                                                                    \
        _new_len;                                                            \
    })

#else /* CANON_NO_GNU_EXTENSIONS */

/**
 * @def ALGO_UNIQUE_TYPED(array, len_ptr, Type, cmp, ctx)
 * @brief Type-safe in-place unique with length update (C99 fallback version)
 *
 * C99-compatible version that updates length via pointer.
 * Cannot be used in expressions.
 *
 * @param array   Array of Type to deduplicate (borrowed)
 * @param len_ptr Pointer to length variable (updated with new length)
 * @param Type    Element type
 * @param cmp     algo_cmp_fn comparator (borrowed)
 * @param ctx     Optional context (borrowed, may be NULL)
 *
 * @note array is borrowed — array is modified in place but not freed
 * @note len_ptr is modified — new length is written to *len_ptr
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Example:
 * ```c
 * int arr[] = {1, 1, 2, 2, 3};
 * usize len = 5;
 * ALGO_UNIQUE_TYPED(arr, &len, int, cmp_int, NULL);
 * // len is now 3, arr = {1, 2, 3, ...}
 * ```
 */
#define ALGO_UNIQUE_TYPED(array, len_ptr, Type, cmp, ctx)                   \
    do {                                                                     \
        if ((array) && (len_ptr)) {                                          \
            usize _new_len = algo_unique((array), *(len_ptr), sizeof(Type), \
                                         (algo_cmp_fn)(cmp), (ctx));         \
            *(len_ptr) = _new_len;                                           \
        }                                                                    \
    } while (0)

#endif /* CANON_NO_GNU_EXTENSIONS */

/**
 * @def ALGO_HAS_NO_CONSECUTIVE_DUPLICATES_TYPED(array, len, Type, cmp, ctx)
 * @brief Type-safe check for consecutive duplicates — evaluates to bool
 *
 * @param array Array to check (borrowed, read-only)
 * @param len   Number of elements
 * @param Type  Element type
 * @param cmp   algo_cmp_fn comparator (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Example:
 * ```c
 * int arr[] = {1, 2, 3};
 * bool ok = ALGO_HAS_NO_CONSECUTIVE_DUPLICATES_TYPED(arr, 3, int, cmp_int, NULL);
 * // true
 * ```
 */
#define ALGO_HAS_NO_CONSECUTIVE_DUPLICATES_TYPED(array, len, Type, cmp, ctx) \
    algo_has_no_consecutive_duplicates((array), (usize)(len), sizeof(Type),  \
                                       (algo_cmp_fn)(cmp), (ctx))

/**
 * @def ALGO_COUNT_UNIQUE_TYPED(array, len, Type, cmp, ctx)
 * @brief Type-safe count of unique consecutive elements — evaluates to usize
 *
 * @param array Array to count (borrowed, read-only)
 * @param len   Number of elements
 * @param Type  Element type
 * @param cmp   algo_cmp_fn comparator (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Example:
 * ```c
 * int arr[] = {1, 1, 2, 2, 3};
 * usize count = ALGO_COUNT_UNIQUE_TYPED(arr, 5, int, cmp_int, NULL);
 * // count = 3
 * ```
 */
#define ALGO_COUNT_UNIQUE_TYPED(array, len, Type, cmp, ctx) \
    algo_count_unique((array), (usize)(len), sizeof(Type),  \
                      (algo_cmp_fn)(cmp), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_UNIQUE — typed slice variant per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type).
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates unique/count/check functions that accept slice_##type directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 * - algo_unique_slice_##type(sv, cmp, ctx)                        → usize
 * - algo_has_no_consecutive_duplicates_slice_##type(sv, cmp, ctx) → bool
 * - algo_count_unique_slice_##type(sv, cmp, ctx)                  → usize
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Ownership:
 * - sv: borrowed (slice is non-owning view, underlying array is modified by unique)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_UNIQUE(int)
 *
 * int arr[] = {1, 1, 2, 2, 3};
 * slice_int sv = slice_int_from(arr, 5);
 *
 * usize new_len = algo_unique_slice_int(sv, algo_cmp_int, NULL);
 * // new_len = 3, arr = {1, 2, 3, ...}
 *
 * bool ok = algo_has_no_consecutive_duplicates_slice_int(
 *     slice_int_from(arr, new_len), algo_cmp_int, NULL);
 * // true
 * ```
 */
#define DEFINE_ALGO_UNIQUE(type) \
\
/** \
 * @brief Removes consecutive duplicates from slice_##type in-place \
 * \
 * @param sv  Slice (borrowed non-owning view — modifies underlying array) \
 * @param cmp Three-way comparator (borrowed) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @return New logical length after deduplication \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * @pre sv.ptr points to valid array if non-NULL \
 * \
 * Ownership: \
 * - sv: borrowed (underlying array modified in place) \
 * - cmp: borrowed (used during call only) \
 * - ctx: borrowed (passed to cmp) \
 * \
 * Performance: O(n) time, O(1) space \
 * \
 * Thread-safety: Safe to call on different slices concurrently \
 */ \
static inline usize algo_unique_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    if (!sv.ptr || sv.len <= 1 || !cmp) return sv.len; \
    return algo_unique(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
} \
\
/** \
 * @brief Returns true if slice_##type has no consecutive duplicates \
 * \
 * @param sv  Slice to check (borrowed, read-only) \
 * @param cmp Comparator (borrowed) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * \
 * Ownership: \
 * - sv: borrowed (read-only) \
 * - cmp: borrowed (used during call only) \
 * - ctx: borrowed (passed to cmp) \
 * \
 * Performance: O(n) time, O(1) space \
 * \
 * Thread-safety: Safe to call concurrently if array not being modified \
 */ \
static inline bool algo_has_no_consecutive_duplicates_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    return algo_has_no_consecutive_duplicates(sv.ptr, sv.len, sizeof(type), \
                                              cmp, ctx); \
} \
\
/** \
 * @brief Counts unique consecutive elements in slice_##type \
 * \
 * @param sv  Slice to count (borrowed, read-only) \
 * @param cmp Comparator (borrowed) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * \
 * Ownership: \
 * - sv: borrowed (read-only) \
 * - cmp: borrowed (used during call only) \
 * - ctx: borrowed (passed to cmp) \
 * \
 * Performance: O(n) time, O(1) space \
 * \
 * Thread-safety: Safe to call concurrently if array not being modified \
 */ \
static inline usize algo_count_unique_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    return algo_count_unique(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

#endif /* CANON_ALGO_UNIQUE_H */
