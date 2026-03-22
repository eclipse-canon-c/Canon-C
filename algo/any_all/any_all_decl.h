/**
 * @file any_all_decl.h
 * @brief Forward declarations for Canon-C any_all (separate compilation support)
 *
 * Include this in headers that reference algo_any or algo_all without
 * needing full definitions. Pair with any_all_defn.h in exactly one .c
 * file to generate the actual implementations.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header (e.g. my_module.h):
 * ```c
 * #include "algo/any_all/any_all_decl.h"
 * ```
 *
 * In your implementation unit (e.g. my_module.c):
 * ```c
 * #include "algo/any_all/any_all_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use any_all.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * any_all_decl.h may be included from any layer that needs to reference
 * algo_any or algo_all. It generates extern declarations, not definitions.
 *
 * @sa any_all.h       — header-only entry point (includes everything)
 * @sa any_all_defn.h  — generates definitions (include in exactly one .c)
 * @sa any_all_impl.h  — pure logic (included by defn)
 */
#ifndef CANON_ALGO_ANY_ALL_DECL_H
#define CANON_ALGO_ANY_ALL_DECL_H

#include "any_all_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/compare.h"
#include "core/ownership.h"

/* ============================================================================
 * External function declarations
 * ========================================================================= */

/**
 * @brief Returns true if ANY element in a flat array satisfies the predicate
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns false)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 * @return true if any element satisfies pred, false otherwise
 *
 * @sa any_all.h for full documentation
 */
extern bool algo_any(
    borrowed(const void*)  base,
    usize                  len,
    usize                  elem_size,
    borrowed(algo_pred_fn) pred,
    borrowed(void*)        ctx);

/**
 * @brief Returns true if ALL elements in a flat array satisfy the predicate
 *
 * Returns true for empty sequences (vacuous truth).
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns true)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 * @return true if all elements satisfy pred or len == 0, false otherwise
 *
 * @sa any_all.h for full documentation
 */
extern bool algo_all(
    borrowed(const void*)  base,
    usize                  len,
    usize                  elem_size,
    borrowed(algo_pred_fn) pred,
    borrowed(void*)        ctx);

#endif /* CANON_ALGO_ANY_ALL_DECL_H */
