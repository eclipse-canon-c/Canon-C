/**
 * @file filter_impl.h
 * @brief Pure implementation logic for Canon-C filter
 *
 * This file contains the function body for algo_filter().
 * It has zero naming assumptions — the linkage specifier comes from
 * ALGO_FILTER_LINKAGE set by the includer.
 * Do NOT include this file directly. Include filter.h (header-only) or
 * use filter_decl.h + filter_defn.h for separate compilation.
 *
 * Algorithm: linear scan with selective copy
 * ────────────────────────────────────────────────────────────────────────────
 * algo_filter iterates all input elements left-to-right. For each element
 * where pred returns true, it copies the element into the output buffer.
 * Stops writing when the output buffer is full (but also stops scanning —
 * there is no benefit to continuing once the output is saturated).
 * Relative order of matching elements is always preserved.
 *
 * Truncation behavior:
 * ────────────────────────────────────────────────────────────────────────────
 * When the output buffer fills before all input is consumed, the function
 * stops immediately and returns out_cap. The caller can detect truncation
 * by comparing the return value to out_cap — if equal, more elements may
 * have matched beyond the buffer. If a full count of matching elements is
 * needed without truncation, provide an output buffer at least as large as
 * the input.
 *
 * Parameter ordering:
 * ────────────────────────────────────────────────────────────────────────────
 * algo_filter follows the C standard library convention of destination-first
 * (same as memcpy, algo_map, and memmove). The output buffer and its capacity
 * precede the input buffer and its length. This is consistent across all
 * Canon-C algo/ functions that produce output into a separate buffer.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * filter_impl.h includes only:
 *   - filter_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/compare.h
 *   - core/primitives/ptr.h
 *   - core/memory.h
 *   - core/ownership.h
 *
 * @sa filter.h       — user-facing entry point (include this)
 * @sa filter_mangle.h — name conventions
 * @sa filter_decl.h  — forward declarations for separate compilation
 * @sa filter_defn.h  — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_FILTER_LINKAGE
    #error "filter_impl.h must not be included directly. Include filter.h instead."
#endif

#include "filter_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/ownership.h"

/* ============================================================================
 * algo_filter — selective copy into caller-provided output buffer
 * ========================================================================= */

/**
 * @brief Filters a flat array into a caller-provided output buffer
 *
 * Iterates all input elements left-to-right. Copies each element for which
 * pred returns true into out. Stops as soon as out_cap elements have been
 * written — does not continue scanning after the output is full.
 *
 * Relative order of matching elements is preserved (stable).
 *
 * Truncation detection: if the return value equals out_cap, the output
 * may be truncated — additional matching elements may exist beyond what
 * was written. To guarantee no truncation, provide out_cap >= in_len.
 *
 * Parameter order follows the C standard library destination-first convention
 * (memcpy, algo_map): output buffer first, then input buffer.
 *
 * @param out       Caller-provided output buffer (borrowed, writable)
 * @param out_cap   Maximum elements out can hold (0 is valid — returns 0)
 * @param base      Pointer to first input element (borrowed, read-only)
 * @param len       Number of input elements (0 is valid — returns 0)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate — return true to keep element (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 *
 * @return Number of elements written to out (0 .. out_cap)
 *
 * @pre out  != NULL      — triggers require_msg
 * @pre base != NULL      — triggers require_msg
 * @pre pred != NULL      — triggers require_msg
 * @pre elem_size > 0     — triggers require_msg
 *
 * Performance:
 * - Time best:  O(1) — len == 0 or out_cap == 0 (immediate return before
 *                       any iteration)
 * - Time worst: O(n) — all n elements checked; occurs when no element
 *                       matches (full scan, 0 copies) or when all match
 *                       and out_cap >= n (full scan, n copies)
 * - Time avg:   O(k) where k = position of last written element + 1;
 *                       scanning stops as soon as out_cap matches are found
 * - Space:      O(1) — no heap allocation, no recursion, constant stack
 *                       frame regardless of len or out_cap
 * - Pred calls: 0 (len == 0 or out_cap == 0) to n (all elements checked);
 *                       stops calling pred once out_cap matches are written;
 *                       pred is never called with a NULL elem pointer
 * - Copies:     at most min(matching_elements, out_cap) element copies;
 *                       each copy is elem_size bytes via mem_copy
 * - Stability:  relative order of matching elements is always preserved
 */
ALGO_FILTER_LINKAGE usize algo_filter(
    borrowed(void*)        out,
    usize                  out_cap,
    borrowed(const void*)  base,
    usize                  len,
    usize                  elem_size,
    borrowed(algo_pred_fn) pred,
    borrowed(void*)        ctx)
{
    require_msg(out       != NULL, "algo_filter: out cannot be NULL");
    require_msg(base      != NULL, "algo_filter: base cannot be NULL");
    require_msg(pred      != NULL, "algo_filter: pred cannot be NULL");
    require_msg(elem_size >  0,    "algo_filter: elem_size must be > 0");

    if (len == 0 || out_cap == 0) return 0;

    usize written = 0;
    for (usize i = 0; i < len; i++) {
        const void* elem = ptr_elem_const(base, i, elem_size);
        if (pred(elem, ctx)) {
            mem_copy(ptr_elem(out, written, elem_size), elem, elem_size);
            written++;
            if (written >= out_cap) break; /* output full — stop */
        }
    }
    return written;
}
