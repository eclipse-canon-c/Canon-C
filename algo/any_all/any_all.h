#ifndef CANON_ALGO_ANY_ALL_H
#define CANON_ALGO_ANY_ALL_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/compare.h"          /* provides algo_pred_fn */
#include "core/slice.h"
#include "core/ownership.h"

/**
 * @file algo/any_all.h
 * @brief Predicate testing over sequences — existential and universal quantification
 *
 * Tests whether a predicate holds for any or all elements in a sequence.
 * Both operations short-circuit — they stop as soon as the result is known.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Input is read-only — never mutated
 * - No allocation — O(1) space
 * - Short-circuit evaluation — stops at first match (any) or first failure (all)
 * - Three levels: generic flat-array, typed macro, slice variants
 * - Predicates receive a const pointer to each element and an optional context
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Empty sequence behavior:
 * ────────────────────────────────────────────────────────────────────────────
 * - algo_any():  returns false for empty sequences
 *               ∃x. P(x) over {} = false  ← standard mathematical result
 *
 * - algo_all():  returns true for empty sequences (vacuous truth)
 *               ∀x. P(x) over {} = true   ← standard mathematical result
 *
 * This matches the standard mathematical definitions and is consistent with
 * every major language standard library (C++, Rust, Python, Haskell).
 *
 * The two operations are duals of each other:
 *   algo_all(empty) == true   and   algo_any(empty) == false
 *
 * This identity holds for all inputs, including empty sequences, and is
 * essential for correct composition:
 *
 * Example where vacuous truth matters:
 * ```c
 * // "Are all inputs valid?" — correct answer for zero inputs is YES
 * bool inputs_ok = algo_all(inputs, count, sizeof(Input), is_valid, NULL);
 * if (!inputs_ok) { reject(); }
 * // With count == 0: inputs_ok = true → no rejection. Correct.
 * // Without vacuous truth: inputs_ok = false → spurious rejection. Wrong.
 * ```
 *
 * NULL and zero-size inputs:
 * ────────────────────────────────────────────────────────────────────────────
 * - base == NULL, pred == NULL, or elem_size == 0 are programmer errors
 *   and fire require_msg (always-on panic).
 * - len == 0 is valid:
 *   - algo_any()  returns false  (no element satisfies predicate)
 *   - algo_all()  returns true   (no element violates predicate)
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * ANY (existential quantification):
 * 1. Iterate through elements left-to-right
 * 2. Apply predicate to each element
 * 3. On first true: return true immediately (short-circuit)
 * 4. If all false: return false
 *
 * Example: ANY negative in [1, 2, -3, 4]
 * Step 0: Check 1  — pred(1)  = false, continue
 * Step 1: Check 2  — pred(2)  = false, continue
 * Step 2: Check -3 — pred(-3) = true, return true ✓
 * (4 is never checked — short-circuit)
 *
 * ALL (universal quantification):
 * 1. Iterate through elements left-to-right
 * 2. Apply predicate to each element
 * 3. On first false: return false immediately (short-circuit)
 * 4. If all true (or no elements): return true
 *
 * Example: ALL positive in [1, 2, -3, 4]
 * Step 0: Check 1  — pred(1)  = true, continue
 * Step 1: Check 2  — pred(2)  = true, continue
 * Step 2: Check -3 — pred(-3) = false, return false ✓
 * (4 is never checked — short-circuit)
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - No allocations anywhere in this file
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions are thread-safe (no shared mutable state)
 * - Safe to call concurrently on different arrays from multiple threads
 * - Same array should not be modified concurrently without external synchronization
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(k) where k = index of first match/failure (worst case O(n))
 * - Space complexity: O(1) — no allocations
 * - Short-circuit: stops immediately when result is known
 * - Best case: O(1) if first element determines result
 * - Worst case: O(n) if all elements must be checked
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Validation: checking if any element is invalid
 * - Existence: checking if any element matches criteria
 * - Completeness: checking if all elements satisfy requirement
 * - Early detection: finding first anomaly or violation
 * - Precondition checking: verifying all inputs are valid
 *
 * @sa core/primitives/compare.h — for algo_pred_fn definition
 * @sa core/slice.h — slice_##type used by slice variants
 * @sa core/primitives/ptr.h — ptr_elem_const used in generic interface
 * @sa core/ownership.h — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Generic flat-array interface
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if ANY element in a flat array satisfies predicate
 *
 * Implements existential quantification (∃): checks if at least one element
 * satisfies the predicate. Short-circuits on first match.
 *
 * Returns false for empty sequences (len == 0) — no element satisfies
 * the predicate if there are no elements.
 *
 * @param base      Pointer to first element of the array (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns false)
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context passed to predicate (borrowed, may be NULL)
 *
 * @return true if ∃i: pred(&base[i]) == true, false otherwise
 *
 * @pre base != NULL  — triggers require_msg (always-on panic)
 * @pre pred != NULL  — triggers require_msg (always-on panic)
 * @pre elem_size > 0 — triggers require_msg (always-on panic)
 * @pre base points to valid array of len elements (if len > 0)
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx:  borrowed (passed through to pred, not stored)
 *
 * Performance:
 * - Time:  O(k) where k = index of first match (worst case O(n))
 * - Space: O(1)
 * - Best case: O(1) if first element matches
 */
static inline bool algo_any(
    borrowed const void* base,
    usize len,
    usize elem_size,
    borrowed algo_pred_fn pred,
    borrowed void* ctx)
{
    require_msg(base      != NULL, "algo_any: base cannot be NULL");
    require_msg(pred      != NULL, "algo_any: pred cannot be NULL");
    require_msg(elem_size >  0,    "algo_any: elem_size must be > 0");

    for (usize i = 0; i < len; i++) {
        if (pred(ptr_elem_const(base, i, elem_size), ctx)) return true;
    }
    return false;
}

/**
 * @brief Returns true if ALL elements in a flat array satisfy predicate
 *
 * Implements universal quantification (∀): checks if every element
 * satisfies the predicate. Short-circuits on first failure.
 *
 * Returns true for empty sequences (len == 0) — vacuous truth.
 * No element violates the predicate if there are no elements.
 * This matches standard mathematical convention and every major
 * language standard library (C++, Rust, Python, Haskell).
 *
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements (0 is valid — returns true)
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @return true if ∀i: pred(&base[i]) == true, or len == 0. false otherwise.
 *
 * @pre base != NULL  — triggers require_msg (always-on panic)
 * @pre pred != NULL  — triggers require_msg (always-on panic)
 * @pre elem_size > 0 — triggers require_msg (always-on panic)
 * @pre base points to valid array of len elements (if len > 0)
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx:  borrowed (passed through to pred, not stored)
 *
 * Performance:
 * - Time:  O(k) where k = index of first failure (worst case O(n))
 * - Space: O(1)
 * - Best case: O(1) if first element fails
 */
static inline bool algo_all(
    borrowed const void* base,
    usize len,
    usize elem_size,
    borrowed algo_pred_fn pred,
    borrowed void* ctx)
{
    require_msg(base      != NULL, "algo_all: base cannot be NULL");
    require_msg(pred      != NULL, "algo_all: pred cannot be NULL");
    require_msg(elem_size >  0,    "algo_all: elem_size must be > 0");

    for (usize i = 0; i < len; i++) {
        if (!pred(ptr_elem_const(base, i, elem_size), ctx)) return false;
    }
    return true; /* vacuous truth when len == 0 */
}

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_ANY_TYPED(items, len, Type, pred, ctx)
 * @brief Type-safe ANY check over a C array
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 *
 * @param items Array of Type (borrowed, read-only)
 * @param len   Number of elements (0 is valid — returns false)
 * @param Type  Element type
 * @param pred  Predicate: bool (*)(const Type*, void*) (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return bool — true if any element satisfies pred
 */
#define ALGO_ANY_TYPED(items, len, Type, pred, ctx) \
    algo_any((items), (len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx))

/**
 * @def ALGO_ALL_TYPED(items, len, Type, pred, ctx)
 * @brief Type-safe ALL check over a C array
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 *
 * Returns true for empty arrays (len == 0) — vacuous truth.
 *
 * @param items Array of Type (borrowed, read-only)
 * @param len   Number of elements (0 is valid — returns true)
 * @param Type  Element type
 * @param pred  Predicate: bool (*)(const Type*, void*) (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return bool — true if all elements satisfy pred, or len == 0
 */
#define ALGO_ALL_TYPED(items, len, Type, pred, ctx) \
    algo_all((items), (len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_ANY_ALL — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   Generates functions that accept slice_##type directly.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates any/all functions that accept slice_##type directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 * - algo_any_slice_##type(sv, pred, ctx) → bool
 *     Returns false for empty slices (sv.len == 0)
 * - algo_all_slice_##type(sv, pred, ctx) → bool
 *     Returns true for empty slices (sv.len == 0) — vacuous truth
 *
 * The predicate receives a const type* pointer to each element.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 */
#define DEFINE_ALGO_ANY_ALL(type) \
\
/** \
 * @brief Returns true if any element in slice_##type satisfies predicate \
 * \
 * @param sv   Typed slice view (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * \
 * @return true if ∃ element satisfying pred, false if empty or none match \
 */ \
static inline bool algo_any_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_pred_fn pred, \
    borrowed void* ctx) \
{ \
    require_msg(sv.ptr != NULL, "algo_any_slice: ptr cannot be NULL"); \
    require_msg(pred   != NULL, "algo_any_slice: pred cannot be NULL"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Returns true if all elements in slice_##type satisfy predicate \
 * \
 * Returns true for empty slices (sv.len == 0) — vacuous truth. \
 * No element violates the predicate if there are no elements. \
 * \
 * @param sv   Typed slice view (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * \
 * @return true if ∀ elements satisfy pred, or sv.len == 0 \
 */ \
static inline bool algo_all_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_pred_fn pred, \
    borrowed void* ctx) \
{ \
    require_msg(sv.ptr != NULL, "algo_all_slice: ptr cannot be NULL"); \
    require_msg(pred   != NULL, "algo_all_slice: pred cannot be NULL"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (!pred(&sv.ptr[_i], ctx)) return false; \
    } \
    return true; /* vacuous truth when sv.len == 0 */ \
}

#endif /* CANON_ALGO_ANY_ALL_H */
