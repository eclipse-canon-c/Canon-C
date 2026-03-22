/**
 * @file sort_decl.h
 * @brief Forward declarations for Canon-C sort (separate compilation support)
 *
 * Include this in headers that reference algo_sort or algo_is_sorted without
 * needing full definitions. Pair with sort_defn.h in exactly one .c file to
 * generate the actual implementations.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header (e.g. my_module.h):
 * ```c
 * #include "algo/sort/sort_decl.h"
 * ```
 *
 * In your implementation unit (e.g. my_module.c):
 * ```c
 * #include "algo/sort/sort_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use sort.h directly.
 *
 * Note: the four internal helpers (algo_sort_swap, algo_insertion_sort_range,
 * algo_merge, algo_merge_sort_range) are always static inline and are NOT
 * declared here. They are never part of the public API.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * sort_decl.h may be included from any layer that needs to reference
 * algo_sort or algo_is_sorted. It generates extern declarations only.
 *
 * @sa sort.h       — header-only entry point (includes everything)
 * @sa sort_defn.h  — generates definitions (include in exactly one .c)
 * @sa sort_impl.h  — pure logic (included by defn, do not include directly)
 */
#ifndef CANON_ALGO_SORT_DECL_H
#define CANON_ALGO_SORT_DECL_H

#include "sort_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/compare.h"
#include "core/ownership.h"

/* ============================================================================
 * External function declarations
 * ========================================================================= */

/**
 * @brief Sorts a flat array in place (stable hybrid insertion/merge sort)
 *
 * @param base        Pointer to first element (borrowed, modified in place)
 * @param len         Number of elements (0 and 1 are valid — no-op)
 * @param elem_size   Size in bytes of each element (must be > 0)
 * @param cmp         Three-way comparator (borrowed)
 * @param ctx         Optional context (borrowed, may be NULL)
 * @param temp_buffer Scratch buffer of len * elem_size bytes, or NULL (borrowed)
 *
 * @sa sort.h for full documentation
 */
extern void algo_sort(
    borrowed(void*)         base,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx,
    borrowed(void*)         temp_buffer);

/**
 * @brief Returns true if a flat array is sorted according to cmp
 *
 * Returns true for empty and single-element arrays (vacuously sorted).
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 and 1 are valid — returns true)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @sa sort.h for full documentation
 */
extern bool algo_is_sorted(
    borrowed(const void*)   base,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx);

#endif /* CANON_ALGO_SORT_DECL_H */
