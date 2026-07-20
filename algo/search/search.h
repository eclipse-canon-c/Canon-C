/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

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
 * All five: O(log n) time, O(1) space.
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

#define ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    algo_lower_bound((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

#define ALGO_UPPER_BOUND_TYPED(array, len, Type, key, cmp, ctx) \
    algo_upper_bound((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

#define ALGO_FIND_SORTED_TYPED(array, len, Type, key, cmp, ctx) \
    algo_find_sorted((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

#define ALGO_BINARY_SEARCH_TYPED(array, len, Type, key, cmp, ctx) \
    algo_binary_search((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx))

#define ALGO_EQUAL_RANGE_TYPED(array, len, Type, key, cmp, ctx, out_range) \
    algo_equal_range((array), (usize)(len), sizeof(Type), \
        (key), (algo_cmp_fn)(cmp), (ctx), (out_range))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_SEARCH — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   Generates fully typed functions accepting slice_##type directly.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates all five search functions operating on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * All slice wrappers enforce contracts (key, cmp, out_range non-NULL)
 * before delegating to the generic functions, consistent with the
 * contract pattern used by filter, find, fold, and any_all.
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h invariants). All functions short-circuit on len == 0 after
 * contract checks, so a NULL ptr with len == 0 is safe.
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
    require_msg(key != NULL, \
        "algo_lower_bound_slice_" #type ": key cannot be NULL"); \
    require_msg(cmp != NULL, \
        "algo_lower_bound_slice_" #type ": cmp cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_lower_bound_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len == 0) { return 0; } \
    return algo_lower_bound(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline usize ALGO_UPPER_BOUND_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    require_msg(key != NULL, \
        "algo_upper_bound_slice_" #type ": key cannot be NULL"); \
    require_msg(cmp != NULL, \
        "algo_upper_bound_slice_" #type ": cmp cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_upper_bound_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len == 0) { return 0; } \
    return algo_upper_bound(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline usize ALGO_FIND_SORTED_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    require_msg(key != NULL, \
        "algo_find_sorted_slice_" #type ": key cannot be NULL"); \
    require_msg(cmp != NULL, \
        "algo_find_sorted_slice_" #type ": cmp cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_find_sorted_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len == 0) { return CANON_USIZE_MAX; } \
    return algo_find_sorted(sv.ptr, sv.len, sizeof(type), key, cmp, ctx); \
} \
\
static inline bool ALGO_BINARY_SEARCH_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(const type*)   key, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    require_msg(key != NULL, \
        "algo_binary_search_slice_" #type ": key cannot be NULL"); \
    require_msg(cmp != NULL, \
        "algo_binary_search_slice_" #type ": cmp cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_binary_search_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len == 0) { return false; } \
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
    require_msg(key       != NULL, \
        "algo_equal_range_slice_" #type ": key cannot be NULL"); \
    require_msg(cmp       != NULL, \
        "algo_equal_range_slice_" #type ": cmp cannot be NULL"); \
    require_msg(out_range != NULL, \
        "algo_equal_range_slice_" #type ": out_range cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_equal_range_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len == 0) { out_range[0] = 0; out_range[1] = 0; return; } \
    algo_equal_range(sv.ptr, sv.len, sizeof(type), key, cmp, ctx, out_range); \
}

#endif /* CANON_ALGO_SEARCH_H */
