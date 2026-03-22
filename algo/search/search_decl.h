/**
 * @file search_decl.h
 * @brief Forward declarations for Canon-C search (separate compilation support)
 *
 * Include this in headers that reference any search function without
 * needing full definitions. Pair with search_defn.h in exactly one .c
 * file to generate the actual implementations.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header (e.g. my_module.h):
 * ```c
 * #include "algo/search/search_decl.h"
 * ```
 *
 * In your implementation unit (e.g. my_module.c):
 * ```c
 * #include "algo/search/search_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use search.h directly.
 *
 * Note: algo_lower_bound_impl is an internal helper and is NOT declared
 * here. It is always translation-unit-local and never part of the public API.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * search_decl.h may be included from any layer that needs to reference
 * search functions. It generates extern declarations, not definitions.
 *
 * @sa search.h       — header-only entry point (includes everything)
 * @sa search_defn.h  — generates definitions (include in exactly one .c)
 * @sa search_impl.h  — pure logic (included by defn, do not include directly)
 */
#ifndef CANON_ALGO_SEARCH_DECL_H
#define CANON_ALGO_SEARCH_DECL_H

#include "search_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/compare.h"
#include "core/ownership.h"

/* ============================================================================
 * External function declarations
 * ========================================================================= */

/**
 * @brief Returns the first index where array[i] >= key (insertion point)
 *
 * @sa search.h for full documentation
 */
extern usize algo_lower_bound(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx);

/**
 * @brief Returns the first index where array[i] > key
 *
 * @sa search.h for full documentation
 */
extern usize algo_upper_bound(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx);

/**
 * @brief Returns the index of the first exact match, or CANON_USIZE_MAX
 *
 * @sa search.h for full documentation
 */
extern usize algo_find_sorted(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx);

/**
 * @brief Returns true if an element equal to key exists in the sorted array
 *
 * @sa search.h for full documentation
 */
extern bool algo_binary_search(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx);

/**
 * @brief Writes the range [lower, upper) of all elements equal to key
 *        into out_range[2]
 *
 * @sa search.h for full documentation
 */
extern void algo_equal_range(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(const void*)   key,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx,
    usize                   out_range[2]);

#endif /* CANON_ALGO_SEARCH_DECL_H */
