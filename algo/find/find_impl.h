/**
 * @file find_impl.h
 * @brief Pure implementation logic for Canon-C find
 *
 * This file contains the function body for algo_find() only.
 * algo_find() returns bool — it has no dependency on option.h.
 * The option-returning slice variants (DEFINE_ALGO_FIND) live in
 * find.h as a macro and are not part of the linkage-controlled impl.
 *
 * Do NOT include this file directly. Include find.h (header-only) or
 * use find_decl.h + find_defn.h for separate compilation.
 *
 * Algorithm: linear search with short-circuit
 * ────────────────────────────────────────────────────────────────────────────
 * algo_find scans left-to-right and returns on the first element for which
 * pred returns true. Both out_index and out_elem are optional output
 * parameters — pass NULL to ignore either. On no match, neither output
 * parameter is written.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * find_impl.h includes only:
 *   - find_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/compare.h
 *   - core/primitives/ptr.h
 *   - core/ownership.h
 *
 * No semantics/ dependency — option.h is not needed here.
 *
 * @sa find.h       — user-facing entry point (include this)
 * @sa find_mangle.h — name conventions
 * @sa find_decl.h  — forward declarations for separate compilation
 * @sa find_defn.h  — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_FIND_LINKAGE
    #error "find_impl.h must not be included directly. Include find.h instead."
#endif

#include "find_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/ownership.h"

/* ============================================================================
 * algo_find — linear search with short-circuit, optional output params
 * ========================================================================= */

/**
 * @brief Finds the first element in a flat array satisfying the predicate
 *
 * Performs a left-to-right linear scan. Stops immediately on the first
 * element for which pred returns true. Both output parameters are optional
 * — pass NULL for either to ignore that output.
 *
 * On no match, neither out_index nor out_elem is written.
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns false)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate — return true on match (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 * @param out_index Optional — receives index of first match (borrowed, may be NULL)
 * @param out_elem  Optional — receives const pointer to match (borrowed, may be NULL)
 *
 * @return true if a match was found, false otherwise
 *
 * @pre base != NULL      — triggers require_msg
 * @pre pred != NULL      — triggers require_msg
 * @pre elem_size > 0     — triggers require_msg
 *
 * Performance:
 * - Time best:  O(1) — len == 0 (immediate false), or first element matches
 *                       (one pred call, immediate return)
 * - Time worst: O(n) — no element matches pred (full scan, n = len)
 * - Time avg:   O(k) where k = index of first match + 1
 * - Space:      O(1) — no heap allocation, no recursion, constant stack frame
 * - Pred calls: 0 (len == 0) to n (no match); stops on first match;
 *                pred is never called with a NULL elem pointer
 * - Outputs:    out_index and out_elem are optional (NULL skips the write);
 *                neither is written on no match — always check the bool return
 */
ALGO_FIND_LINKAGE bool algo_find(
    borrowed(const void*)   base,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_pred_fn)  pred,
    borrowed(void*)         ctx,
    borrowed(usize*)        out_index,
    borrowed(const void**)  out_elem)
{
    require_msg(base      != NULL, "algo_find: base cannot be NULL");
    require_msg(pred      != NULL, "algo_find: pred cannot be NULL");
    require_msg(elem_size >  0,    "algo_find: elem_size must be > 0");

    if (len == 0) return false;

    for (usize i = 0; i < len; i++) {
        const void* elem = ptr_elem_const(base, i, elem_size);
        if (pred(elem, ctx)) {
            if (out_index) *out_index = i;
            if (out_elem)  *out_elem  = elem;
            return true;
        }
    }
    return false;
}
