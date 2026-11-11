/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

/**
 * @file sort_impl.h
 * @brief Pure implementation logic for Canon-C sort (stable hybrid sort)
 *
 * This file contains the function bodies for algo_sort() and
 * algo_is_sorted(), plus the four internal helpers they depend on.
 * It has zero naming assumptions beyond the fixed function names —
 * every linkage specifier for public functions comes from ALGO_SORT_LINKAGE
 * set by the includer. Do NOT include this file directly. Include sort.h
 * (header-only) or use sort_decl.h + sort_defn.h for separate compilation.
 *
 * Internal helpers:
 * ────────────────────────────────────────────────────────────────────────────
 * All four helpers are always static inline regardless of ALGO_SORT_LINKAGE.
 * They are translation-unit-local implementation details, never externally
 * visible, and not declared in sort_decl.h:
 *
 *   algo_sort_swap             — swaps two elements via a fixed stack buffer
 *   algo_insertion_sort_range  — insertion sort over index range [left, right)
 *   algo_merge                 — merges [left, mid) and [mid, right) via temp
 *   algo_merge_sort_range      — bottom-up merge sort over [left, right)
 *
 * ALGO_SORT_LINKAGE applies only to the two public functions: algo_sort
 * and algo_is_sorted.
 *
 * Internal helpers omit borrowed() annotations — ownership is enforced
 * at the public API boundary only.
 *
 * Algorithm: stable hybrid insertion/merge sort
 * ────────────────────────────────────────────────────────────────────────────
 * algo_sort selects between two strategies based on len and temp availability:
 *   len < 2           → no-op
 *   len < 16          → insertion sort (O(n²) worst, O(n) best, cache-friendly)
 *   len >= 16 + temp  → merge sort (O(n log n) guaranteed, stable)
 *   len >= 16, !temp  → insertion sort fallback (O(n²))
 *
 * Merge sort is bottom-up: runs of 8 elements sorted by insertion, then
 * merged in pairs, the width doubling each pass; no recursion (VERIFY-068).
 * The <= comparator in the merge step ensures stability — equal elements
 * from the left half always precede those from the right half.
 *
 * algo_is_sorted performs a single left-to-right pass, returning false on
 * the first adjacent pair that is out of order.
 *
 * Contract convention:
 * ────────────────────────────────────────────────────────────────────────────
 * Both public functions enforce base != NULL via require_msg (consistent
 * with algo_any, algo_all, algo_filter, algo_find, algo_map, algo_reverse).
 * The len < 2 early return is a separate check AFTER the contract — a NULL
 * base is always a programming error, regardless of len.
 *
 * algo_sort_swap enforces elem_size <= ALGO_SORT_SWAP_BUF_SIZE via
 * require_msg (consistent with algo_reverse). Oversized elements are a
 * contract violation, not a silent degradation path.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * sort_impl.h includes only:
 *   - sort_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/compare.h
 *   - core/primitives/ptr.h
 *   - core/memory.h
 *   - core/ownership.h
 *
 * @sa sort.h        — user-facing entry point (include this)
 * @sa sort_mangle.h — name conventions and size constants
 * @sa sort_decl.h   — forward declarations for separate compilation
 * @sa sort_defn.h   — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_SORT_LINKAGE
    #error "sort_impl.h must not be included directly. Include sort.h instead."
#endif

#include "sort_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/ownership.h"

/* ============================================================================
 * Internal helpers — always static inline, never externally visible
 * ========================================================================= */

/**
 * @brief Swaps two elements of elem_size bytes
 *
 * Uses a fixed stack buffer of ALGO_SORT_SWAP_BUF_SIZE bytes. Elements
 * larger than the buffer trigger a contract failure — consistent with
 * algo_reverse's ALGO_REVERSE_SWAP_BUF_SIZE contract.
 *
 * @param a         Pointer to first element (borrowed, modified in place)
 * @param b         Pointer to second element (borrowed, modified in place)
 * @param elem_size Size in bytes of each element
 *
 * @pre elem_size <= ALGO_SORT_SWAP_BUF_SIZE — triggers require_msg
 */
static inline void algo_sort_swap(void* a, void* b, usize elem_size) {
    require_msg(elem_size <= ALGO_SORT_SWAP_BUF_SIZE,
        "algo_sort_swap: elem_size exceeds ALGO_SORT_SWAP_BUF_SIZE");

    u8 tmp[ALGO_SORT_SWAP_BUF_SIZE];
    mem_copy(tmp, a, elem_size);
    mem_copy(a, b, elem_size);
    mem_copy(b, tmp, elem_size);
}

/**
 * @brief Insertion sort over index range [left, right) within base
 *
 * O(n²) worst case, O(n) best case on nearly-sorted input. Cache-friendly
 * for small ranges. Stable — equal elements preserve original relative order.
 * Used directly by algo_sort for len < 16, and by algo_merge_sort_range for
 * its runs of 8 elements (the last may be shorter).
 *
 * @param base      Pointer to array start (borrowed, modified in place)
 * @param left      Inclusive start index
 * @param right     Exclusive end index
 * @param elem_size Size in bytes of each element
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 */
static inline void algo_insertion_sort_range(
    void*       base,
    usize       left,
    usize       right,
    usize       elem_size,
    algo_cmp_fn cmp,
    void*       ctx)
{
    for (usize i = left + 1u; i < right; i++) {
        for (usize j = i; j > left; j--) {
            void* a = ptr_elem(base, j - 1u, elem_size);
            void* b = ptr_elem(base, j,     elem_size);
            if (cmp(a, b, ctx) <= 0) { break; }
            algo_sort_swap(a, b, elem_size);
        }
    }
}

/**
 * @brief Merges two sorted adjacent sub-ranges [left, mid) and [mid, right)
 *        using temp as scratch space
 *
 * Copies the merged result into temp, then writes it back into base at
 * [left, right). The <= comparator ensures stability: equal elements from
 * the left half are placed before those from the right half.
 *
 * @param base      Pointer to array start (borrowed, modified in place)
 * @param left      Inclusive start index of left half
 * @param mid       Exclusive end of left half / inclusive start of right half
 * @param right     Exclusive end index of right half
 * @param elem_size Size in bytes of each element
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 * @param temp      Scratch buffer with capacity for at least (right-left) elements
 */
static inline void algo_merge(
    void*       base,
    usize       left,
    usize       mid,
    usize       right,
    usize       elem_size,
    algo_cmp_fn cmp,
    void*       ctx,
    void*       temp)
{
    usize i = left;
    usize j = mid;
    usize k = 0;

    while ((i < mid) && (j < right)) {
        const void* le = ptr_elem_const(base, i, elem_size);
        const void* re = ptr_elem_const(base, j, elem_size);
        /* <= for stability: equal elements from left half come first */
        if (cmp(le, re, ctx) <= 0) {
            mem_copy(ptr_elem(temp, k, elem_size), le, elem_size);
            i++;
        } else {
            mem_copy(ptr_elem(temp, k, elem_size), re, elem_size);
            j++;
        }
        k++;
    }
    while (i < mid) {
        mem_copy(ptr_elem(temp, k, elem_size),
                 ptr_elem_const(base, i, elem_size), elem_size);
        i++; k++;
    }
    while (j < right) {
        mem_copy(ptr_elem(temp, k, elem_size),
                 ptr_elem_const(base, j, elem_size), elem_size);
        j++; k++;
    }

    mem_copy(ptr_elem(base, left, elem_size), temp, (right - left) * elem_size);
}

/**
 * @brief Bottom-up merge sort over index range [left, right)
 *
 * Sorts [left, right) in runs of 8 elements by insertion sort (the last
 * run may be shorter), then merges adjacent runs in pairs via algo_merge,
 * the run width doubling each pass, until one run covers the range. The
 * hybrid is faster in practice, insertion sort outperforming merge sort at
 * small sizes. Runs of 8 keep the comparisons of the recursive form this
 * replaces, whose insertion-sorted pieces had 8 to 15 elements: the same
 * count at power-of-two lengths, and about as many elsewhere (VERIFY-068).
 * The passes are loops: no recursion (MISRA C:2012 rule 17.2), and a fixed
 * stack, whatever the length.
 *
 * Each pass leaves [left, right) in sorted runs of the pass's width, the
 * last one possibly shorter; a last run without a partner stays as it is
 * until a wider pass merges it. Merging adjacent runs with algo_merge's <=
 * keeps the sort stable, as the recursive form it replaces was.
 *
 * @param base      Pointer to array start (borrowed, modified in place)
 * @param left      Inclusive start index
 * @param right     Exclusive end index (right - left >= 1)
 * @param elem_size Size in bytes of each element
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 * @param temp      Scratch buffer with capacity for at least (right-left) elements
 */
static inline void algo_merge_sort_range(
    void*       base,
    usize       left,
    usize       right,
    usize       elem_size,
    algo_cmp_fn cmp,
    void*       ctx,
    void*       temp)
{
    usize n     = right - left;
    usize start = left;
    usize width = 8u;

    /* Runs of 8 elements, sorted by insertion; the last may be shorter. */
    while ((right - start) > 8u) {
        algo_insertion_sort_range(base, start, start + 8u, elem_size, cmp, ctx);
        start += 8u;
    }
    algo_insertion_sort_range(base, start, right, elem_size, cmp, ctx);

    /* Adjacent runs of `width` elements merged in pairs, the width doubling
       each pass. No sum below overflows: each stays below right. */
    while (width < n) {
        usize lo = left;
        while ((right - lo) > width) {
            usize mid = lo + width;
            usize hi  = ((right - mid) > width) ? (mid + width) : right;
            algo_merge(base, lo, mid, hi, elem_size, cmp, ctx, temp);
            lo = hi;
        }
        /* Past half the length, this pass made one run of [left, right). */
        width = (width > (n / 2u)) ? n : (2u * width);
    }
}

/* ============================================================================
 * algo_sort — stable hybrid sort of a flat array
 * ========================================================================= */

/**
 * @brief Sorts a flat array in place using a stable hybrid insertion/merge sort
 *
 * Selects the sort strategy based on len and temp_buffer availability:
 *   len < 2                         → no-op, returns immediately
 *   len < 16                        → insertion sort
 *   len >= 16 and temp_buffer valid → merge sort (O(n log n), stable)
 *   len >= 16 and temp_buffer NULL  → insertion sort fallback (O(n²))
 *
 * temp_buffer must hold at least len * elem_size bytes. Its contents are
 * undefined after the call. It must not overlap with base.
 *
 * @param base        Pointer to first element (borrowed, modified in place)
 * @param len         Number of elements (0 and 1 are valid — no-op)
 * @param elem_size   Size in bytes of each element (must be > 0)
 * @param cmp         Three-way comparator (borrowed)
 * @param ctx         Optional context passed to cmp (borrowed, may be NULL)
 * @param temp_buffer Caller-provided scratch buffer of len * elem_size bytes,
 *                    or NULL to force insertion sort (borrowed)
 *
 * @pre base      != NULL — triggers require_msg
 * @pre elem_size > 0     — triggers require_msg
 * @pre cmp       != NULL — triggers require_msg
 * @pre elem_size <= ALGO_SORT_SWAP_BUF_SIZE — enforced by algo_sort_swap
 *
 * @post Array elements are in non-decreasing order according to cmp
 * @post Equal elements preserve their original relative order (stable)
 * @post temp_buffer contents are undefined after return
 * @post No allocations performed
 *
 * Performance:
 * - Time:  O(n log n) with temp_buffer; O(n²) without
 * - Space: O(n) temp_buffer (caller-provided); O(1) stack, no recursion
 */
ALGO_SORT_LINKAGE void algo_sort(
    borrowed(void*)         base,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx,
    borrowed(void*)         temp_buffer)
{
    require_msg(base      != NULL, "algo_sort: base cannot be NULL");
    require_msg(elem_size > 0u,     "algo_sort: elem_size must be > 0");
    require_msg(cmp       != NULL, "algo_sort: cmp cannot be NULL");

    if (len < 2u) { return; }

    if ((len < 16u) || !temp_buffer) {
        algo_insertion_sort_range(base, 0, len, elem_size, cmp, ctx);
    } else {
        algo_merge_sort_range(base, 0, len, elem_size, cmp, ctx, temp_buffer);
    }
}

/* ============================================================================
 * algo_is_sorted — non-destructive sorted check over a flat array
 * ========================================================================= */

/**
 * @brief Returns true if a flat array is sorted according to cmp
 *
 * Performs a single left-to-right pass, comparing each adjacent pair.
 * Short-circuits on the first out-of-order pair. The array is never modified.
 *
 * Empty and single-element arrays return true — vacuously sorted.
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 and 1 are valid — returns true)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context passed to cmp (borrowed, may be NULL)
 *
 * @pre base      != NULL — triggers require_msg
 * @pre elem_size > 0     — triggers require_msg
 * @pre cmp       != NULL — triggers require_msg
 *
 * @return true if sorted or len < 2, false on first out-of-order pair
 *
 * Performance:
 * - Time:  O(n) worst case; O(1) on first out-of-order pair
 * - Space: O(1)
 */
ALGO_SORT_LINKAGE bool algo_is_sorted(
    borrowed(const void*)   base,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx)
{
    require_msg(base      != NULL, "algo_is_sorted: base cannot be NULL");
    require_msg(elem_size > 0u,     "algo_is_sorted: elem_size must be > 0");
    require_msg(cmp       != NULL, "algo_is_sorted: cmp cannot be NULL");

    if (len < 2u) { return true; }

    for (usize i = 1; i < len; i++) {
        if (cmp(ptr_elem_const(base, i - 1u, elem_size),
                ptr_elem_const(base, i,     elem_size),
                ctx) > 0) {
            return false;
        }
    }
    return true;
}
