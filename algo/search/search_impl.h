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
 * @file search_impl.h
 * @brief Pure implementation logic for Canon-C search (binary search utilities)
 *
 * This file contains the function bodies for algo_lower_bound(),
 * algo_upper_bound(), algo_find_sorted(), algo_binary_search(), and
 * algo_equal_range(). It has zero naming assumptions beyond the fixed
 * function names — every linkage specifier for public functions comes
 * from ALGO_SEARCH_LINKAGE set by the includer.
 * Do NOT include this file directly. Include search.h (header-only) or
 * use search_decl.h + search_defn.h for separate compilation.
 *
 * Internal helper:
 * ────────────────────────────────────────────────────────────────────────────
 * algo_lower_bound_impl is always static inline regardless of
 * ALGO_SEARCH_LINKAGE. It is a translation-unit-local helper shared by
 * algo_lower_bound, algo_find_sorted, and algo_equal_range. It is never
 * externally visible and is not declared in search_decl.h. Callers outside
 * this module must use the public API only.
 *
 * Algorithm: iterative binary search
 * ────────────────────────────────────────────────────────────────────────────
 * All five functions use iterative binary search — no recursion, no stack
 * growth beyond O(1). The midpoint is computed as low + (high - low) / 2
 * to avoid the classic integer overflow that arises from (low + high) / 2
 * when indices approach USIZE_MAX.
 *
 * algo_lower_bound — finds the first index where array[i] >= key.
 *                    Delegates to algo_lower_bound_impl after precondition checks.
 *
 * algo_upper_bound — finds the first index where array[i] > key.
 *                    Independent binary search with a mirrored pivot condition.
 *
 * algo_find_sorted — calls algo_lower_bound_impl, then verifies an exact match
 *                    at the returned position. Returns CANON_USIZE_MAX on miss.
 *
 * algo_binary_search — thin wrapper over algo_find_sorted, returns bool.
 *
 * algo_equal_range — calls both algo_lower_bound_impl and algo_upper_bound
 *                    and writes the two positions into out_range[2].
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * search_impl.h includes only:
 *   - search_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/limits.h   (for CANON_USIZE_MAX)
 *   - core/primitives/contract.h
 *   - core/primitives/compare.h
 *   - core/primitives/ptr.h
 *   - core/ownership.h
 *
 * @sa search.h        — user-facing entry point (include this)
 * @sa search_mangle.h — name conventions
 * @sa search_decl.h   — forward declarations for separate compilation
 * @sa search_defn.h   — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_SEARCH_LINKAGE
    #error "search_impl.h must not be included directly. Include search.h instead."
#endif

#include "search_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/ownership.h"

/* ============================================================================
 * Internal helper — always static inline, never externally visible
 * ========================================================================= */

/**
 * @brief Core lower-bound binary search — first index where array[i] >= key
 *
 * This is the shared implementation used by algo_lower_bound,
 * algo_find_sorted, and algo_equal_range. Precondition checks are the
 * responsibility of each public caller — this function does none.
 *
 * The midpoint uses low + (high - low) / 2 to avoid overflow when
 * indices approach CANON_USIZE_MAX.
 *
 * @param array     Pointer to first element (read-only, must not be NULL)
 * @param len       Number of elements (must be > 0)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param key       Search key (read-only, must not be NULL)
 * @param cmp       Comparator (must not be NULL)
 * @param ctx       Optional context (may be NULL)
 *
 * @return First index in [0, len] where array[i] >= key.
 *         Returns len when all elements are strictly less than key.
 */
static inline usize algo_lower_bound_impl(
    const void*     array,
    usize           len,
    usize           elem_size,
    const void*     key,
    algo_cmp_fn     cmp,
    void*           ctx)
{
    usize low  = 0;
    usize high = len;

    while (low < high) {
        usize mid = low + (high - low) / 2u;
        if (cmp(ptr_elem_const(array, mid, elem_size), key, ctx) < 0) {
            low = mid + 1u;
        } else {
            high = mid;
        }
    }
    return low;
}

/* ============================================================================
 * algo_lower_bound — first index where array[i] >= key
 * ========================================================================= */

/**
 * @brief Returns the first index where array[i] >= key (lower bound)
 *
 * Returns this position whether or not key is present in the array.
 * This is the correct insertion point to maintain sorted order.
 *
 * Empty input (len == 0) returns 0 — the only valid insertion point.
 * Never returns CANON_USIZE_MAX.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns 0)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param key       Search key (borrowed, read-only)
 * @param cmp       Comparator — must match the sort order (borrowed)
 * @param ctx       Optional context passed to cmp (borrowed, may be NULL)
 *
 * @pre array     != NULL — triggers require_msg
 * @pre key       != NULL — triggers require_msg
 * @pre cmp       != NULL — triggers require_msg
 * @pre elem_size >  0    — triggers require_msg
 * @pre array is sorted according to cmp
 *
 * @return index in [0, len]; returns len if all elements are < key
 *
 * Performance:
 * - Time:  O(log n) — at most ⌈log₂(n)⌉ + 1 comparisons
 * - Space: O(1)
 */
ALGO_SEARCH_LINKAGE usize algo_lower_bound(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx)
{
    require_msg(array     != NULL, "algo_lower_bound: array cannot be NULL");
    require_msg(key       != NULL, "algo_lower_bound: key cannot be NULL");
    require_msg(cmp       != NULL, "algo_lower_bound: cmp cannot be NULL");
    require_msg(elem_size >  0u,    "algo_lower_bound: elem_size must be > 0");

    if (len == 0u) { return 0u; }
    return algo_lower_bound_impl(array, len, elem_size, key, cmp, ctx);
}

/* ============================================================================
 * algo_upper_bound — first index where array[i] > key
 * ========================================================================= */

/**
 * @brief Returns the first index where array[i] > key (upper bound)
 *
 * Combined with algo_lower_bound, gives the half-open range [lower, upper)
 * of all elements equal to key. When key is absent, lower == upper.
 *
 * Empty input (len == 0) returns 0.
 * Never returns CANON_USIZE_MAX.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns 0)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param key       Search key (borrowed, read-only)
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @pre array     != NULL — triggers require_msg
 * @pre key       != NULL — triggers require_msg
 * @pre cmp       != NULL — triggers require_msg
 * @pre elem_size >  0    — triggers require_msg
 * @pre array is sorted according to cmp
 *
 * @return index in [0, len]; returns len if all elements are <= key
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 */
ALGO_SEARCH_LINKAGE usize algo_upper_bound(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx)
{
    require_msg(array     != NULL, "algo_upper_bound: array cannot be NULL");
    require_msg(key       != NULL, "algo_upper_bound: key cannot be NULL");
    require_msg(cmp       != NULL, "algo_upper_bound: cmp cannot be NULL");
    require_msg(elem_size >  0u,    "algo_upper_bound: elem_size must be > 0");

    if (len == 0u) { return 0u; }

    usize low  = 0;
    usize high = len;

    while (low < high) {
        usize mid = low + (high - low) / 2u;
        if (cmp(ptr_elem_const(array, mid, elem_size), key, ctx) <= 0) {
            low = mid + 1u;  /* elem <= key: search right */
        } else {
            high = mid;     /* elem >  key: search left  */
        }
    }
    return low;
}

/* ============================================================================
 * algo_find_sorted — first exact match, or CANON_USIZE_MAX
 * ========================================================================= */

/**
 * @brief Returns the index of the first element equal to key, or CANON_USIZE_MAX
 *
 * Delegates to algo_lower_bound_impl, then verifies the element at the
 * returned position is an exact match. Returns CANON_USIZE_MAX on any miss.
 *
 * Unlike algo_lower_bound, this function does not return an insertion point
 * when the key is absent — it signals failure via CANON_USIZE_MAX.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns CANON_USIZE_MAX)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param key       Search key (borrowed, read-only)
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @pre array     != NULL — triggers require_msg
 * @pre key       != NULL — triggers require_msg
 * @pre cmp       != NULL — triggers require_msg
 * @pre elem_size >  0    — triggers require_msg
 * @pre array is sorted according to cmp
 *
 * @return index of first exact match, or CANON_USIZE_MAX if not found
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 */
ALGO_SEARCH_LINKAGE usize algo_find_sorted(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx)
{
    require_msg(array     != NULL, "algo_find_sorted: array cannot be NULL");
    require_msg(key       != NULL, "algo_find_sorted: key cannot be NULL");
    require_msg(cmp       != NULL, "algo_find_sorted: cmp cannot be NULL");
    require_msg(elem_size >  0u,    "algo_find_sorted: elem_size must be > 0");

    if (len == 0u) { return CANON_USIZE_MAX; }

    usize pos = algo_lower_bound_impl(array, len, elem_size, key, cmp, ctx);

    if ((pos < len) && (cmp(ptr_elem_const(array, pos, elem_size), key, ctx) == 0)) {
        return pos;
    }
    return CANON_USIZE_MAX;
}

/* ============================================================================
 * algo_binary_search — boolean existence check
 * ========================================================================= */

/**
 * @brief Returns true if an element equal to key exists in the sorted array
 *
 * Thin wrapper over algo_find_sorted. Returns true when the result is
 * not CANON_USIZE_MAX.
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns false)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param key       Search key (borrowed, read-only)
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @pre Preconditions forwarded to algo_find_sorted
 * @pre array is sorted according to cmp
 *
 * @return true if key found, false otherwise
 *
 * Performance:
 * - Time:  O(log n)
 * - Space: O(1)
 */
ALGO_SEARCH_LINKAGE bool algo_binary_search(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx)
{
    return algo_find_sorted(array, len, elem_size, key, cmp, ctx) != CANON_USIZE_MAX;
}

/* ============================================================================
 * algo_equal_range — half-open range [lower, upper) of all equal elements
 * ========================================================================= */

/**
 * @brief Writes into out_range[2] the range [lower, upper) of all elements
 *        equal to key
 *
 * out_range[0] = first index where array[i] >= key (lower bound)
 * out_range[1] = first index where array[i] >  key (upper bound)
 *
 * All elements at indices in [out_range[0], out_range[1]) compare equal to key.
 * When key is absent, out_range[0] == out_range[1] (empty range).
 * When len == 0, writes [0, 0).
 *
 * @param array     Pointer to first element of sorted array (borrowed, read-only)
 * @param len       Number of elements (0 is valid — writes [0, 0))
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param key       Search key (borrowed, read-only)
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 * @param out_range Output array of exactly 2 usize values (owned by caller)
 *
 * @pre array     != NULL — triggers require_msg
 * @pre key       != NULL — triggers require_msg
 * @pre cmp       != NULL — triggers require_msg
 * @pre elem_size >  0    — triggers require_msg
 * @pre out_range != NULL — triggers require_msg
 * @pre array is sorted according to cmp
 *
 * @post out_range[0] <= out_range[1]
 * @post both values are in [0, len]
 *
 * Performance:
 * - Time:  O(log n) — two independent binary searches
 * - Space: O(1)
 */
ALGO_SEARCH_LINKAGE void algo_equal_range(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx,
    usize                   out_range[2])
{
    require_msg(array     != NULL, "algo_equal_range: array cannot be NULL");
    require_msg(key       != NULL, "algo_equal_range: key cannot be NULL");
    require_msg(cmp       != NULL, "algo_equal_range: cmp cannot be NULL");
    require_msg(elem_size >  0u,    "algo_equal_range: elem_size must be > 0");
    require_msg(out_range != NULL, "algo_equal_range: out_range cannot be NULL");

    if (len == 0u) {
        out_range[0] = 0;
        out_range[1] = 0;
        return;
    }

    out_range[0] = algo_lower_bound_impl(array, len, elem_size, key, cmp, ctx);
    out_range[1] = algo_upper_bound(array, len, elem_size, key, cmp, ctx);
}
