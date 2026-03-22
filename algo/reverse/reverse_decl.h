/**
 * @file reverse_decl.h
 * @brief Forward declarations for Canon-C reverse (separate compilation support)
 *
 * Include this in headers that reference algo_reverse or algo_is_palindrome
 * without needing full definitions. Pair with reverse_defn.h in exactly one
 * .c file to generate the actual implementations.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header (e.g. my_module.h):
 * ```c
 * #include "algo/reverse/reverse_decl.h"
 * ```
 *
 * In your implementation unit (e.g. my_module.c):
 * ```c
 * #include "algo/reverse/reverse_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use reverse.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * reverse_decl.h may be included from any layer that needs to reference
 * algo_reverse or algo_is_palindrome. It generates extern declarations,
 * not definitions.
 *
 * @sa reverse.h       — header-only entry point (includes everything)
 * @sa reverse_defn.h  — generates definitions (include in exactly one .c)
 * @sa reverse_impl.h  — pure logic (included by defn, do not include directly)
 */
#ifndef CANON_ALGO_REVERSE_DECL_H
#define CANON_ALGO_REVERSE_DECL_H

#include "reverse_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/compare.h"
#include "core/ownership.h"

/* ============================================================================
 * External function declarations
 * ========================================================================= */

/**
 * @brief Reverses the elements of a flat array in place
 *
 * @param array     Pointer to first element (borrowed, modified in place)
 * @param len       Number of elements (0 and 1 are valid — no-op)
 * @param elem_size Size in bytes of each element (must be > 0)
 *
 * @sa reverse.h for full documentation
 */
extern void algo_reverse(
    borrowed(void*) array,
    usize           len,
    usize           elem_size);

/**
 * @brief Returns true if a flat array reads the same forwards and backwards
 *
 * Returns true for empty and single-element arrays (vacuous palindrome).
 *
 * @param array     Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 and 1 are valid — returns true)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param cmp       Comparator returning 0 if elements are equal (borrowed)
 * @param ctx       Optional context passed through to cmp (borrowed, may be NULL)
 *
 * @sa reverse.h for full documentation
 */
extern bool algo_is_palindrome(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx);

#endif /* CANON_ALGO_REVERSE_DECL_H */
