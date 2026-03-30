/**
 * @file search.h
 * @brief Binary search utilities for sorted sequences
 *
 * This is the entry point for header-only usage. Including this file
 * generates statically-inlined implementations of all five search
 * functions, plus typed macro wrappers and the DEFINE_ALGO_SEARCH
 * instantiation macro.
 *
 * For separate compilation (external linkage), use search_decl.h in
 * headers and search_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PRECONDITION: ARRAYS MUST BE SORTED
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * All functions in this module require the input array to be sorted
 * according to the same comparator used in the search call. Searching
 * an unsorted array produces wrong results — not crashes. The array is
 * never validated for sortedness; that is the caller's responsibility.
 *
 * Always use the same comparator for sorting and searching:
 *   algo_sort(arr, n, sizeof(T), my_cmp, ctx, tmp);       // sort
 *   algo_lower_bound(arr, n, sizeof(T), &k, my_cmp, ctx); // search ✓
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Level 1 — Generic (void* interface, works on any type):
 * ```c
 * int arr[] = {1, 2, 2, 2, 5, 7, 9};
 * int key   = 2;
 *
 * usize lo = algo_lower_bound(arr, 7, sizeof(int), &key, algo_cmp_int, NULL);
 * // lo = 1 (first index where arr[i] >= 2)
 *
 * usize hi = algo_upper_bound(arr, 7, sizeof(int), &key, algo_cmp_int, NULL);
 * // hi = 4 (first index where arr[i] > 2)
 *
 * usize idx = algo_find_sorted(arr, 7, sizeof(int), &key, algo_cmp_int, NULL);
 * // idx = 1 (first exact match), or CANON_USIZE_MAX if absent
 *
 * bool found = algo_binary_search(arr, 7, sizeof(int), &key, algo_cmp_int, NULL);
 * // found = true
 *
 * usize range[2];
 * algo_equal_range(arr, 7, sizeof(int), &key, algo_cmp_int, NULL, range);
 * // range = {1, 4} — indices [1, 4) all equal 2
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof, same fn signature):
 * ```c
 * usize lo    = ALGO_LOWER_BOUND_TYPED(arr, 7, int, &key, algo_cmp_int, NULL);
 * usize hi    = ALGO_UPPER_BOUND_TYPED(arr, 7, int, &key, algo_cmp_int, NULL);
 * usize idx   = ALGO_FIND_SORTED_TYPED(arr, 7, int, &key, algo_cmp_int, NULL);
 * bool  found = ALGO_BINARY_SEARCH_TYPED(arr, 7, int, &key, algo_cmp_int, NULL);
 * usize range[2];
 * ALGO_EQUAL_RANGE_TYPED(arr, 7, int, &key, algo_cmp_int, NULL, range);
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, fully optimizable):
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_SEARCH(int)
 *
 * int buf[] = {1, 2, 2, 2, 5, 7, 9};
 * slice_int sv = slice_int_from(buf, 7);
 * int key = 2;
 *
 * usize lo    = algo_lower_bound_slice_int(sv, &key, algo_cmp_int, NULL);
 * usize hi    = algo_upper_bound_slice_int(sv, &key, algo_cmp_int, NULL);
 * usize idx   = algo_find_sorted_slice_int(sv, &key, algo_cmp_int, NULL);
 * bool  found = algo_binary_search_slice_int(sv, &key, algo_cmp_int, NULL);
 * usize range[2];
 * algo_equal_range_slice_int(sv, &key, algo_cmp_int, NULL, range);
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * EMPTY ARRAY BEHAVIOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * len == 0 is valid for all functions:
 *   algo_lower_bound    → 0
 *   algo_upper_bound    → 0
 *   algo_find_sorted    → CANON_USIZE_MAX  (not found)
 *   algo_binary_search  → false
 *   algo_equal_range    → writes [0, 0)
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * FUNCTION SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 *   lower_bound   — first index where array[i] >= key; always in [0, len]
 *   upper_bound   — first index where array[i] >  key; always in [0, len]
 *   find_sorted   — first exact match index, or CANON_USIZE_MAX
 *   binary_search — bool: does an exact match exist?
 *   equal_range   — writes [lower, upper) of all equal elements into out[2]
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PERFORMANCE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * All five functions use iterative binary search — no recursion, O(1) stack.
 * The midpoint is always computed as low + (high − low) / 2 to avoid the
 * overflow that arises from (low + high) / 2 when indices approach USIZE_MAX.
 *
 * algo_lower_bound / ALGO_LOWER_BOUND_TYPED / algo_lower_bound_slice_##type:
 *   Time:  O(log n) — at most ⌈log₂(n)⌉ + 1 comparisons
 *   Space: O(1)
 *   cmp calls: always ⌈log₂(n)⌉ (no early exit; finds a position, not a match)
 *
 * algo_upper_bound / ALGO_UPPER_BOUND_TYPED / algo_upper_bound_slice_##type:
 *   Time:  O(log n) — independent search with mirrored pivot condition
 *   Space: O(1)
 *   cmp calls: always ⌈log₂(n)⌉
 *
 * algo_find_sorted / ALGO_FIND_SORTED_TYPED / algo_find_sorted_slice_##type:
 *   Time:  O(log n) — lower_bound_impl then one verification comparison
 *   Space: O(1)
 *   Best case: O(1) when len == 0 (immediate CANON_USIZE_MAX)
 *
 * algo_binary_search / ALGO_BINARY_SEARCH_TYPED / algo_binary_search_slice_##type:
 *   Time:  O(log n) — thin wrapper over algo_find_sorted
 *   Space: O(1)
 *
 * algo_equal_range / ALGO_EQUAL_RANGE_TYPED / algo_equal_range_slice_##type:
 *   Time:  O(log n) — two independent binary searches (lower + upper)
 *   Space: O(1)
 *   cmp calls: at most 2 × ⌈log₂(n)⌉
 *
 * Level comparison:
 *   Level 1 — Generic: one stride multiply per comparison.
 *   Level 2 — Typed macro: sizeof is compile-time; multiply elided by optimizer.
 *   Level 3 — Typed slice: delegates to the generic implementation; sizeof(type)
 *              is a compile-time constant visible to the optimizer at the call site.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_lower_bound(array, len, elem_size, key, cmp, ctx)         → usize
 *   algo_upper_bound(array, len, elem_size, key, cmp, ctx)         → usize
 *   algo_find_sorted(array, len, elem_size, key, cmp, ctx)         → usize
 *   algo_binary_search(array, len, elem_size, key, cmp, ctx)       → bool
 *   algo_equal_range(array, len, elem_size, key, cmp, ctx, out[2]) → void
 *
 * Typed macros:
 *   ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx)         → usize
 *   ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp, ctx)         → usize
 *   ALGO_FIND_SORTED_TYPED(array, len, Type, key, cmp, ctx)         → usize
 *   ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx)       → bool
 *   ALGO_EQUAL_RANGE_TYPED(array, len, Type, key, cmp, ctx, out[2]) → void
 *
 * Typed instantiation (call DEFINE_ALGO_SEARCH(type) first):
 *   algo_lower_bound_slice_##type(sv, key, cmp, ctx)         → usize
 *   algo_upper_bound_slice_##type(sv, key, cmp, ctx)         → usize
 *   algo_find_sorted_slice_##type(sv, key, cmp, ctx)         → usize
 *   algo_binary_search_slice_##type(sv, key, cmp, ctx)       → bool
 *   algo_equal_range_slice_##type(sv, key, cmp, ctx, out[2]) → void
 *
 * @sa search_mangle.h — name customization for slice variants
 * @sa search_decl.h   — forward declarations for separate compilation
 * @sa search_defn.h   — definitions for separate compilation
 * @sa search_impl.h   — pure logic (do not include directly)
 * @sa core/slice.h    — slice_##type used by DEFINE_ALGO_SEARCH
 * @sa core/primitives/compare.h — algo_cmp_fn definition
 * @sa core/primitives/limits.h  — CANON_USIZE_MAX sentinel value
 */

#ifndef CANON_ALGO_SEARCH_H
#define CANON_ALGO_SEARCH_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — all public functions are inlined at call sites.
 * algo_lower_bound_impl is always static inline regardless of this setting.
 */
#define ALGO_SEARCH_LINKAGE static inline

#include "search_impl.h"   /* implementation logic — NOT search_defn.h */

#undef ALGO_SEARCH_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe insertion-point lookup — always in [0, len], never CANON_USIZE_MAX
 *
 * Performance: O(log n), O(1) space.
 *
 * @param array C array of Type (borrowed, read-only, must be sorted)
 * @param len   Number of elements (0 is valid — returns 0)
 * @param Type  Element type — used for sizeof only
 * @param key   Pointer to the search key (borrowed, read-only)
 * @param cmp   Comparator matching the sort order (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 * @return usize — first index where array[i] >= *key, in [0, len]
 */
#define ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    algo_lower_bound((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

/**
 * @def ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe upper-bound lookup — always in [0, len], never CANON_USIZE_MAX
 *
 * Performance: O(log n), O(1) space.
 *
 * @param array C array of Type (borrowed, read-only, must be sorted)
 * @param len   Number of elements (0 is valid — returns 0)
 * @param Type  Element type — used for sizeof only
 * @param key   Pointer to the search key (borrowed, read-only)
 * @param cmp   Comparator matching the sort order (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 * @return usize — first index where array[i] > *key, in [0, len]
 */
#define ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    algo_upper_bound((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

/**
 * @def ALGO_FIND_SORTED_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe exact-match lookup — returns CANON_USIZE_MAX if not found
 *
 * Performance: O(log n), O(1) space.
 *
 * @param array C array of Type (borrowed, read-only, must be sorted)
 * @param len   Number of elements (0 is valid — returns CANON_USIZE_MAX)
 * @param Type  Element type — used for sizeof only
 * @param key   Pointer to the search key (borrowed, read-only)
 * @param cmp   Comparator matching the sort order (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 * @return usize — index of first exact match, or CANON_USIZE_MAX
 */
#define ALGO_FIND_SORTED_TYPED(array, len, Type, key, cmp, ctx) \
    algo_find_sorted((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

/**
 * @def ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx)
 * @brief Type-safe boolean existence check
 *
 * Performance: O(log n), O(1) space.
 *
 * @param array C array of Type (borrowed, read-only, must be sorted)
 * @param len   Number of elements (0 is valid — returns false)
 * @param Type  Element type — used for sizeof only
 * @param key   Pointer to the search key (borrowed, read-only)
 * @param cmp   Comparator matching the sort order (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 * @return bool — true if an element equal to *key exists
 */
#define ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx) \
    algo_binary_search((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

/**
 * @def ALGO_EQUAL_RANGE_TYPED(array, len, Type, key, cmp, ctx, out_range)
 * @brief Type-safe equal-range query — writes [lower, upper) into out_range[2]
 *
 * Performance: O(log n) — two binary searches, O(1) space.
 *
 * @param array     C array of Type (borrowed, read-only, must be sorted)
 * @param len       Number of elements (0 is valid — writes [0, 0))
 * @param Type      Element type — used for sizeof only
 * @param key       Pointer to the search key (borrowed, read-only)
 * @param cmp       Comparator matching the sort order (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 * @param out_range usize[2] output array (owned by caller)
 */
#define ALGO_EQUAL_RANGE_TYPED(array, len, Type, key, cmp, ctx, out_range) \
    algo_equal_range((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx), (out_range))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_SEARCH — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   Generates fully typed functions accepting slice_##type directly.
   No void* anywhere — the compiler sees the actual element type.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates all five search functions operating on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 *   algo_lower_bound_slice_##type(sv, key, cmp, ctx)         → usize
 *   algo_upper_bound_slice_##type(sv, key, cmp, ctx)         → usize
 *   algo_find_sorted_slice_##type(sv, key, cmp, ctx)         → usize
 *   algo_binary_search_slice_##type(sv, key, cmp, ctx)       → bool
 *   algo_equal_range_slice_##type(sv, key, cmp, ctx, out[2]) → void
 *
 * The key parameter is typed (const type*) — no void* at call sites.
 * The comparator remains algo_cmp_fn (const void*, const void*, void*).
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h invariants). All functions handle len == 0 before touching
 * any element, so a NULL ptr with len == 0 is safe.
 *
 * Performance: all five are O(log n) time, O(1) space.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 */
#define DEFINE_ALGO_SEARCH(type) \
\
static inline usize ALGO_LOWER_BOUND_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    return algo_lower_bound(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline usize ALGO_UPPER_BOUND_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    return algo_upper_bound(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline usize ALGO_FIND_SORTED_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    return algo_find_sorted(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline bool ALGO_BINARY_SEARCH_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    return algo_binary_search(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline void ALGO_EQUAL_RANGE_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx, \
    usize                   out_range[2]) \
{ \
    algo_equal_range(sv.ptr, sv.len, sizeof(type), key, cmp, ctx, out_range); \
}

#endif /* CANON_ALGO_SEARCH_H */
