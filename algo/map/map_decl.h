/**
 * @file map_decl.h
 * @brief Forward declarations for Canon-C map (separate compilation support)
 *
 * Include this in headers that reference algo_map or algo_map_inplace without
 * needing full definitions. Pair with map_defn.h in exactly one .c file to
 * generate the actual implementations.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In your header (e.g. my_module.h):
 * ```c
 * #include "algo/map/map_decl.h"
 * ```
 *
 * In your implementation unit (e.g. my_module.c):
 * ```c
 * #include "algo/map/map_defn.h"
 * ```
 *
 * For header-only usage with static linkage, use map.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * map_decl.h may be included from any layer that needs to reference
 * algo_map or algo_map_inplace. It generates extern declarations, not
 * definitions. The function pointer types (algo_map_fn, algo_map_inplace_fn)
 * are also declared here so callers can form correct function pointers without
 * including the full implementation.
 *
 * @sa map.h       — header-only entry point (includes everything)
 * @sa map_defn.h  — generates definitions (include in exactly one .c)
 * @sa map_impl.h  — pure logic (included by defn, do not include directly)
 */
#ifndef CANON_ALGO_MAP_DECL_H
#define CANON_ALGO_MAP_DECL_H

#include "map_mangle.h"

#include "core/primitives/types.h"
#include "core/ownership.h"

/* ============================================================================
 * Function pointer types (declared here for consumers of map_decl.h)
 * ========================================================================= */

/**
 * @brief Transformation function for algo_map (cross-type)
 *
 * @param out Writable pointer to the output element slot (borrowed)
 * @param in  Read-only pointer to the input element (borrowed)
 *
 * @sa map_impl.h for full documentation
 */
typedef void (*algo_map_fn)(void* out, const void* in);

/**
 * @brief Transformation function for algo_map_inplace (same-type)
 *
 * @param elem Writable pointer to the element (borrowed)
 *
 * @sa map_impl.h for full documentation
 */
typedef void (*algo_map_inplace_fn)(void* elem);

/* ============================================================================
 * External function declarations
 * ========================================================================= */

/**
 * @brief Applies a transformation to each element of an input array,
 *        writing results into a separate output array
 *
 * @param out           Pointer to first output element (borrowed, writable)
 * @param in            Pointer to first input element (borrowed, read-only)
 * @param len           Number of elements to process (0 is valid — no-op)
 * @param out_elem_size Size in bytes of each output element (must be > 0)
 * @param in_elem_size  Size in bytes of each input element (must be > 0)
 * @param fn            Transformation function (borrowed)
 *
 * @sa map.h for full documentation
 */
extern void algo_map(
    borrowed(void*)         out,
    borrowed(const void*)   in,
    usize                   len,
    usize                   out_elem_size,
    usize                   in_elem_size,
    borrowed(algo_map_fn)   fn);

/**
 * @brief Applies a transformation to each element of an array in place
 *
 * @param base      Pointer to first element (borrowed, read-write)
 * @param len       Number of elements to process (0 is valid — no-op)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param fn        In-place transformation function (borrowed)
 *
 * @sa map.h for full documentation
 */
extern void algo_map_inplace(
    borrowed(void*)                 base,
    usize                           len,
    usize                           elem_size,
    borrowed(algo_map_inplace_fn)   fn);

#endif /* CANON_ALGO_MAP_DECL_H */
