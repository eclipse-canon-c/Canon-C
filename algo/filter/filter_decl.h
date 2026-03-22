/**
 * @file filter_decl.h
 * @brief Forward declaration for Canon-C filter (separate compilation support)
 *
 * Include this in headers that reference algo_filter without needing
 * the full definition. Pair with filter_defn.h in exactly one .c file
 * to generate the actual implementation.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header (e.g. my_module.h):
 * ```c
 * #include "algo/filter/filter_decl.h"
 * ```
 *
 * In your implementation unit (e.g. my_module.c):
 * ```c
 * #include "algo/filter/filter_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use filter.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * filter_decl.h may be included from any layer that needs to reference
 * algo_filter. It generates an extern declaration, not a definition.
 *
 * @sa filter.h       — header-only entry point (includes everything)
 * @sa filter_defn.h  — generates definition (include in exactly one .c)
 * @sa filter_impl.h  — pure logic (included by defn)
 */
#ifndef CANON_ALGO_FILTER_DECL_H
#define CANON_ALGO_FILTER_DECL_H

#include "filter_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/compare.h"
#include "core/ownership.h"

/**
 * @brief Filters a flat array into a caller-provided output buffer
 *
 * @param base      Pointer to first input element (borrowed, read-only)
 * @param len       Number of input elements (0 is valid — returns 0)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate — return true to keep element (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 * @param out       Caller-provided output buffer (borrowed, writable)
 * @param out_cap   Maximum elements out can hold (0 is valid — returns 0)
 * @return          Number of elements written to out (0 .. out_cap)
 *
 * @sa filter.h for full documentation
 */
extern usize algo_filter(
    borrowed(const void*)  base,
    usize                  len,
    usize                  elem_size,
    borrowed(algo_pred_fn) pred,
    borrowed(void*)        ctx,
    borrowed(void*)        out,
    usize                  out_cap);

#endif /* CANON_ALGO_FILTER_DECL_H */
