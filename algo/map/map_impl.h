/**
 * @file map_impl.h
 * @brief Pure implementation logic for Canon-C map (element-wise transformation)
 *
 * This file contains the function bodies for algo_map() and algo_map_inplace().
 * It has zero naming assumptions beyond the fixed function names — every
 * linkage specifier comes from ALGO_MAP_LINKAGE set by the includer.
 * Do NOT include this file directly. Include map.h (header-only) or
 * use map_decl.h + map_defn.h for separate compilation.
 *
 * Algorithm: linear pass with element-wise transformation
 * ────────────────────────────────────────────────────────────────────────────
 * algo_map — reads each element from input, writes transformed result to
 *            output. Input and output may have different element sizes.
 *            No short-circuit — every element is always processed.
 *            Empty input (len == 0) is valid — no iterations, no-op.
 *
 * algo_map_inplace — applies a transformation to each element of a single
 *                    array directly. Original values are overwritten.
 *                    Empty input (len == 0) is valid — no-op.
 *
 * Function pointer types:
 * ────────────────────────────────────────────────────────────────────────────
 * algo_map_fn:
 *   void (*)(void* out, const void* in)
 *   Receives a writable pointer to the output element and a read-only
 *   pointer to the input element. Must write exactly one output element.
 *
 * algo_map_inplace_fn:
 *   void (*)(void* elem)
 *   Receives a writable pointer to the element. Modifies it in place.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * map_impl.h includes only:
 *   - map_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/ptr.h
 *   - core/ownership.h
 *
 * @sa map.h        — user-facing entry point (include this)
 * @sa map_mangle.h — name conventions
 * @sa map_decl.h   — forward declarations for separate compilation
 * @sa map_defn.h   — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_MAP_LINKAGE
    #error "map_impl.h must not be included directly. Include map.h instead."
#endif

#include "map_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/ownership.h"

/* ============================================================================
 * Function pointer types
 * ========================================================================= */

/**
 * @brief Transformation function for algo_map (cross-type)
 *
 * Receives a writable pointer to the output element and a read-only pointer
 * to the input element. The function must write exactly one output element.
 * It must not retain either pointer beyond the call.
 *
 * @param out Writable pointer to the output element slot (borrowed)
 * @param in  Read-only pointer to the input element (borrowed)
 */
typedef void (*algo_map_fn)(void* out, const void* in);

/**
 * @brief Transformation function for algo_map_inplace (same-type)
 *
 * Receives a writable pointer to the element. Reads the current value,
 * computes the transformed result, and writes it back through the same
 * pointer. Must not retain the pointer beyond the call.
 *
 * @param elem Writable pointer to the element (borrowed)
 */
typedef void (*algo_map_inplace_fn)(void* elem);

/* ============================================================================
 * algo_map — element-wise cross-type transformation over flat arrays
 * ========================================================================= */

/**
 * @brief Applies a transformation function to each element of an input array,
 *        writing results into a separate output array
 *
 * Iterates left-to-right. Every element is always processed — there is no
 * short-circuit. Input and output element sizes may differ (e.g. int → double).
 * Input is never modified. Output is written sequentially.
 *
 * Empty input (len == 0) is valid — the function returns immediately without
 * calling fn.
 *
 * @param out          Pointer to first output element (borrowed, writable)
 * @param in           Pointer to first input element (borrowed, read-only)
 * @param len          Number of elements to process (0 is valid — no-op)
 * @param out_elem_size Size in bytes of each output element (must be > 0)
 * @param in_elem_size  Size in bytes of each input element (must be > 0)
 * @param fn           Transformation function (borrowed)
 *
 * @pre out != NULL        — triggers require_msg
 * @pre in  != NULL        — triggers require_msg
 * @pre fn  != NULL        — triggers require_msg
 * @pre out_elem_size > 0  — triggers require_msg
 * @pre in_elem_size  > 0  — triggers require_msg
 * @pre out and in do not overlap (undefined behavior if they do)
 *
 * @post out[i] = fn(in[i]) for all i in [0, len)
 * @post in is unchanged
 * @post No allocations performed
 *
 * Performance:
 * - Time:  O(n) — exactly n calls to fn
 * - Space: O(1) — no allocation
 */
ALGO_MAP_LINKAGE void algo_map(
    borrowed(void*)         out,
    borrowed(const void*)   in,
    usize                   len,
    usize                   out_elem_size,
    usize                   in_elem_size,
    borrowed(algo_map_fn)   fn)
{
    require_msg(out           != NULL, "algo_map: out cannot be NULL");
    require_msg(in            != NULL, "algo_map: in cannot be NULL");
    require_msg(fn            != NULL, "algo_map: fn cannot be NULL");
    require_msg(out_elem_size >  0,    "algo_map: out_elem_size must be > 0");
    require_msg(in_elem_size  >  0,    "algo_map: in_elem_size must be > 0");

    for (usize i = 0; i < len; i++) {
        fn(ptr_elem(out, i, out_elem_size),
           ptr_elem_const(in, i, in_elem_size));
    }
}

/* ============================================================================
 * algo_map_inplace — element-wise in-place transformation over a flat array
 * ========================================================================= */

/**
 * @brief Applies a transformation function to each element of an array,
 *        modifying it in place
 *
 * Iterates left-to-right. Every element is always processed — there is no
 * short-circuit. Original values are overwritten; there is no way to recover
 * them after the call. If the original data must be preserved, use algo_map
 * with a separate output buffer instead.
 *
 * Empty input (len == 0) is valid — the function returns immediately without
 * calling fn.
 *
 * @param base      Pointer to first element (borrowed, read-write)
 * @param len       Number of elements to process (0 is valid — no-op)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param fn        In-place transformation function (borrowed)
 *
 * @pre base      != NULL — triggers require_msg
 * @pre fn        != NULL — triggers require_msg
 * @pre elem_size >  0    — triggers require_msg
 *
 * @post base[i] = fn(base[i]) for all i in [0, len)
 * @post Original values are lost (overwritten)
 * @post No allocations performed
 *
 * Performance:
 * - Time:  O(n) — exactly n calls to fn
 * - Space: O(1) — no allocation
 */
ALGO_MAP_LINKAGE void algo_map_inplace(
    borrowed(void*)                 base,
    usize                           len,
    usize                           elem_size,
    borrowed(algo_map_inplace_fn)   fn)
{
    require_msg(base      != NULL, "algo_map_inplace: base cannot be NULL");
    require_msg(fn        != NULL, "algo_map_inplace: fn cannot be NULL");
    require_msg(elem_size >  0,    "algo_map_inplace: elem_size must be > 0");

    for (usize i = 0; i < len; i++) {
        fn(ptr_elem(base, i, elem_size));
    }
}
