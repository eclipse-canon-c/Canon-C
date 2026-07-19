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
 * @file any_all_impl.h
 * @brief Pure implementation logic for Canon-C any_all (existential and universal quantification)
 *
 * This file contains the function bodies for algo_any() and algo_all().
 * It has zero naming assumptions beyond the fixed function names — every
 * linkage specifier comes from ALGO_ANY_ALL_LINKAGE set by the includer.
 * Do NOT include this file directly. Include any_all.h (header-only) or
 * use any_all_decl.h + any_all_defn.h for separate compilation.
 *
 * Algorithm: short-circuit linear scan
 * ────────────────────────────────────────────────────────────────────────────
 * Both algo_any and algo_all perform a left-to-right linear scan with
 * early termination:
 *
 * algo_any  — returns true immediately on first element satisfying pred.
 *             Returns false only after all elements fail. Empty → false.
 *
 * algo_all  — returns false immediately on first element failing pred.
 *             Returns true only after all elements pass. Empty → true
 *             (vacuous truth — standard mathematical convention).
 *
 * The two operations are duals:
 *   algo_any(base, 0, ...) == false
 *   algo_all(base, 0, ...) == true
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * any_all_impl.h includes only:
 *   - any_all_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/compare.h
 *   - core/primitives/ptr.h
 *   - core/ownership.h
 *
 * @sa any_all.h       — user-facing entry point (include this)
 * @sa any_all_mangle.h — name conventions
 * @sa any_all_decl.h  — forward declarations for separate compilation
 * @sa any_all_defn.h  — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_ANY_ALL_LINKAGE
    #error "any_all_impl.h must not be included directly. Include any_all.h instead."
#endif

#include "any_all_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/ownership.h"

/* ============================================================================
 * algo_any — existential quantification over a flat array
 * ========================================================================= */

/**
 * @brief Returns true if ANY element in a flat array satisfies the predicate
 *
 * Implements ∃: returns true as soon as one element satisfies pred.
 * Short-circuits — stops at the first match.
 *
 * Returns false for empty sequences (len == 0). No element satisfies
 * the predicate when there are no elements.
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns false)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 *
 * @return true if ∃i: pred(&base[i], ctx) == true, false otherwise
 *
 * @pre base != NULL      — triggers require_msg
 * @pre pred != NULL      — triggers require_msg
 * @pre elem_size > 0     — triggers require_msg
 *
 * Performance:
 * - Time best:  O(1) — first element satisfies pred (immediate return)
 * - Time worst: O(n) — no element satisfies pred (full scan, n = len)
 * - Time avg:   O(k) where k = index of first match + 1
 * - Space:      O(1) — no allocation, no recursion, constant stack frame
 * - Pred calls: 0 (len == 0) to n (no match); never called with NULL elem
 */
ALGO_ANY_ALL_LINKAGE bool algo_any(
    borrowed(const void*) base,
    usize                 len,
    usize                 elem_size,
    borrowed(algo_pred_fn) pred,
    borrowed(void*)        ctx)
{
    require_msg(base      != NULL, "algo_any: base cannot be NULL");
    require_msg(pred      != NULL, "algo_any: pred cannot be NULL");
    require_msg(elem_size >  0u,    "algo_any: elem_size must be > 0");

    for (usize i = 0; i < len; i++) {
        if (pred(ptr_elem_const(base, i, elem_size), ctx)) return true;
    }
    return false;
}

/* ============================================================================
 * algo_all — universal quantification over a flat array
 * ========================================================================= */

/**
 * @brief Returns true if ALL elements in a flat array satisfy the predicate
 *
 * Implements ∀: returns false as soon as one element fails pred.
 * Short-circuits — stops at the first failure.
 *
 * Returns true for empty sequences (len == 0) — vacuous truth.
 * No element violates the predicate when there are no elements.
 * This matches standard mathematical convention and every major language
 * standard library (C++, Rust, Python, Haskell).
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns true)
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context passed to pred (borrowed, may be NULL)
 *
 * @return true if ∀i: pred(&base[i], ctx) == true, or len == 0
 *
 * @pre base != NULL      — triggers require_msg
 * @pre pred != NULL      — triggers require_msg
 * @pre elem_size > 0     — triggers require_msg
 *
 * Performance:
 * - Time best:  O(1) — empty sequence (no iterations, vacuous truth), or
 *                       first element fails pred (immediate return)
 * - Time worst: O(n) — all elements satisfy pred (full scan, n = len)
 * - Time avg:   O(k) where k = index of first failure + 1
 * - Space:      O(1) — no allocation, no recursion, constant stack frame
 * - Pred calls: 0 (len == 0) to n (all pass); never called with NULL elem
 */
ALGO_ANY_ALL_LINKAGE bool algo_all(
    borrowed(const void*) base,
    usize                 len,
    usize                 elem_size,
    borrowed(algo_pred_fn) pred,
    borrowed(void*)        ctx)
{
    require_msg(base      != NULL, "algo_all: base cannot be NULL");
    require_msg(pred      != NULL, "algo_all: pred cannot be NULL");
    require_msg(elem_size >  0u,    "algo_all: elem_size must be > 0");

    for (usize i = 0; i < len; i++) {
        if (!pred(ptr_elem_const(base, i, elem_size), ctx)) return false;
    }
    return true; /* vacuous truth when len == 0 */
}
