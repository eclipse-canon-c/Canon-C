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
 * @file reverse_impl.h
 * @brief Pure implementation logic for Canon-C reverse (in-place reversal and palindrome check)
 *
 * This file contains the function bodies for algo_reverse() and
 * algo_is_palindrome(). It has zero naming assumptions beyond the fixed
 * function names — every linkage specifier comes from ALGO_REVERSE_LINKAGE
 * set by the includer. Do NOT include this file directly. Include reverse.h
 * (header-only) or use reverse_decl.h + reverse_defn.h for separate compilation.
 *
 * Algorithm: two-pointer in-place swap
 * ────────────────────────────────────────────────────────────────────────────
 * algo_reverse — maintains a left pointer (starts at element 0) and a right
 *               pointer (starts at element len-1). Swaps the elements at each
 *               pointer using a fixed-size stack buffer, then advances left
 *               forward and right backward until they meet. Performs exactly
 *               ⌊n/2⌋ swaps. Empty and single-element inputs are no-ops.
 *
 * algo_is_palindrome — performs the same two-pointer traversal but compares
 *                      elements instead of swapping them. Short-circuits on
 *                      the first mismatch. Empty and single-element inputs
 *                      return true (vacuous palindrome — no pair can mismatch).
 *
 * The two operations are structurally dual:
 *   algo_reverse   mutates — swap at every pair
 *   algo_is_palindrome reads — compare at every pair, return false on mismatch
 *
 * Contract convention:
 * ────────────────────────────────────────────────────────────────────────────
 * Both functions enforce array != NULL via require_msg (consistent with
 * algo_any, algo_all, algo_filter, algo_find, algo_map). The len < 2
 * early return is a separate check AFTER the contract — a NULL array
 * is always a programming error, regardless of len.
 *
 * Internal helpers in this file omit borrowed() annotations — ownership
 * is enforced at the public API boundary only.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * reverse_impl.h includes only:
 *   - reverse_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/compare.h
 *   - core/primitives/ptr.h
 *   - core/memory.h
 *   - core/ownership.h
 *
 * @sa reverse.h        — user-facing entry point (include this)
 * @sa reverse_mangle.h — name conventions and swap buffer size
 * @sa reverse_decl.h   — forward declarations for separate compilation
 * @sa reverse_defn.h   — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_REVERSE_LINKAGE
    #error "reverse_impl.h must not be included directly. Include reverse.h instead."
#endif

#include "reverse_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/ownership.h"

/* ============================================================================
 * algo_reverse — in-place reversal of a flat array
 * ========================================================================= */

/**
 * @brief Reverses the elements of a flat array in place
 *
 * Uses the two-pointer technique: left starts at element 0, right starts at
 * element len-1. Each iteration swaps the two elements through a fixed-size
 * stack buffer, then advances the pointers toward the center. The loop ends
 * when left meets or passes right.
 *
 * The swap buffer is allocated on the stack and sized by ALGO_REVERSE_SWAP_BUF_SIZE
 * (default 256 bytes, overridable in reverse_mangle.h). Elements larger than
 * the buffer are caught by contract before any swap is attempted.
 *
 * Empty input (len == 0) and single-element input (len == 1) are no-ops —
 * the function returns immediately after the contract checks.
 *
 * @param array     Pointer to first element (borrowed, modified in place)
 * @param len       Number of elements (0 and 1 are valid — no-op)
 * @param elem_size Size in bytes of each element (must be > 0)
 *
 * @pre array     != NULL — triggers require_msg
 * @pre elem_size >  0    — triggers require_msg
 * @pre elem_size <= ALGO_REVERSE_SWAP_BUF_SIZE — triggers require_msg
 *
 * @post array[i] and array[len-1-i] are swapped for all i in [0, ⌊len/2⌋)
 * @post Original element order is lost
 * @post No allocations performed
 *
 * Performance:
 * - Time:  O(n) — exactly ⌊n/2⌋ swaps, each copying 3 × elem_size bytes
 * - Space: O(1) — fixed-size stack buffer of ALGO_REVERSE_SWAP_BUF_SIZE bytes
 */
ALGO_REVERSE_LINKAGE void algo_reverse(
    borrowed(void*) array,
    usize           len,
    usize           elem_size)
{
    require_msg(array     != NULL,
        "algo_reverse: array cannot be NULL");
    require_msg(elem_size > 0u,
        "algo_reverse: elem_size must be > 0");
    require_msg(elem_size <= ALGO_REVERSE_SWAP_BUF_SIZE,
        "algo_reverse: elem_size exceeds ALGO_REVERSE_SWAP_BUF_SIZE");

    if (len < 2u) return;

    u8  temp[ALGO_REVERSE_SWAP_BUF_SIZE];
    u8* left  = (u8*)array;
    u8* right = (u8*)array + (len - 1u) * elem_size;

    while (left < right) {
        mem_copy(temp,  left,  elem_size);
        mem_copy(left,  right, elem_size);
        mem_copy(right, temp,  elem_size);
        left  += elem_size;
        right -= elem_size;
    }
}

/* ============================================================================
 * algo_is_palindrome — element-wise palindrome check over a flat array
 * ========================================================================= */

/**
 * @brief Returns true if a flat array reads the same forwards and backwards
 *
 * Uses the same two-pointer traversal as algo_reverse but compares instead
 * of swapping. Short-circuits on the first mismatched pair — the comparator
 * is called at most ⌊n/2⌋ times and may be called fewer times.
 *
 * Empty input (len == 0) and single-element input (len == 1) return true —
 * vacuous palindrome: no pair exists that could fail the check.
 *
 * The array is never modified.
 *
 * @param array     Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 and 1 are valid — returns true)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param cmp       Comparator returning 0 if elements are equal (borrowed)
 * @param ctx       Optional context passed through to cmp (borrowed, may be NULL)
 *
 * @pre array     != NULL — triggers require_msg
 * @pre elem_size > 0     — triggers require_msg
 * @pre cmp       != NULL — triggers require_msg
 *
 * @return true if all symmetric pairs compare equal, or len < 2
 *
 * Performance:
 * - Time:  O(n/2) comparisons in the worst case; O(1) on first mismatch
 * - Space: O(1) — no allocation
 */
ALGO_REVERSE_LINKAGE bool algo_is_palindrome(
    borrowed(const void*)   array,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx)
{
    require_msg(array     != NULL, "algo_is_palindrome: array cannot be NULL");
    require_msg(elem_size > 0u,     "algo_is_palindrome: elem_size must be > 0");
    require_msg(cmp       != NULL, "algo_is_palindrome: cmp cannot be NULL");

    if (len < 2u) return true;

    usize left_idx  = 0;
    usize right_idx = len - 1u;

    while (left_idx < right_idx) {
        if (cmp(ptr_elem_const(array, left_idx,  elem_size),
                ptr_elem_const(array, right_idx, elem_size),
                ctx) != 0) {
            return false;
        }
        ++left_idx;
        --right_idx;
    }

    return true;
}
