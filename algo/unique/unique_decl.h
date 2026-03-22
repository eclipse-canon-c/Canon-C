/**
 * @file unique_decl.h
 * @brief Forward declarations for Canon-C unique (separate compilation support)
 *
 * Include this in headers that reference algo_unique without needing the
 * full definition. Pair with unique_defn.h in exactly one .c file to
 * generate the actual implementation.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header (e.g. my_module.h):
 * ```c
 * #include "algo/unique/unique_decl.h"
 * ```
 *
 * In your implementation unit (e.g. my_module.c):
 * ```c
 * #include "algo/unique/unique_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use unique.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * unique_decl.h may be included from any layer that needs to reference
 * algo_unique. It generates an extern declaration, not a definition.
 *
 * @sa unique.h       — header-only entry point (includes everything)
 * @sa unique_defn.h  — generates definition (include in exactly one .c)
 * @sa unique_impl.h  — pure logic (included by defn, do not include directly)
 */
#ifndef CANON_ALGO_UNIQUE_DECL_H
#define CANON_ALGO_UNIQUE_DECL_H

#include "unique_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/compare.h"
#include "core/ownership.h"

/* ============================================================================
 * External function declaration
 * ========================================================================= */

/**
 * @brief Removes consecutive duplicate elements from a flat array in place
 *
 * @param array     Pointer to first element (borrowed, modified in place)
 * @param len       Number of elements (0 and 1 are valid — returns len)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param cmp       Comparator — returns 0 if elements are equal (borrowed)
 * @param ctx       Optional context passed to cmp (borrowed, may be NULL)
 *
 * @return New logical length after removing consecutive duplicates.
 *
 * @sa unique.h for full documentation
 */
extern usize algo_unique(
    borrowed(void*)         array,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx);

#endif /* CANON_ALGO_UNIQUE_DECL_H */
