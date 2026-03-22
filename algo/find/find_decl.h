/**
 * @file find_decl.h
 * @brief Forward declaration for Canon-C find (separate compilation support)
 *
 * Include this in headers that reference algo_find without needing
 * the full definition. Pair with find_defn.h in exactly one .c file
 * to generate the actual implementation.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header:
 * ```c
 * #include "algo/find/find_decl.h"
 * ```
 *
 * In your implementation unit:
 * ```c
 * #include "algo/find/find_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use find.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * find_decl.h may be included from any layer that needs to reference
 * algo_find. It generates an extern declaration, not a definition.
 * No option.h dependency — algo_find returns bool, not option.
 *
 * @sa find.h       — header-only entry point (includes everything)
 * @sa find_defn.h  — generates definition (include in exactly one .c)
 * @sa find_impl.h  — pure logic (included by defn)
 */
#ifndef CANON_ALGO_FIND_DECL_H
#define CANON_ALGO_FIND_DECL_H

#include "find_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/compare.h"
#include "core/ownership.h"

/**
 * @brief Finds the first element in a flat array satisfying the predicate
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns false)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate — return true on match (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 * @param out_index Optional — receives index of first match (borrowed, may be NULL)
 * @param out_elem  Optional — receives const pointer to match (borrowed, may be NULL)
 * @return          true if a match was found, false otherwise
 *
 * @sa find.h for full documentation
 */
extern bool algo_find(
    borrowed(const void*)   base,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_pred_fn)  pred,
    borrowed(void*)         ctx,
    borrowed(usize*)        out_index,
    borrowed(const void**)  out_elem);

#endif /* CANON_ALGO_FIND_DECL_H */
