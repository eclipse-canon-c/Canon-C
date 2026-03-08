#ifndef CANON_ALGO_ANY_ALL_H
#define CANON_ALGO_ANY_ALL_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/compare.h"          // provides algo_pred_fn
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
 * ⚠️  EMPTY SEQUENCE BEHAVIOR — READ BEFORE USE:
 * ────────────────────────────────────────────────────────────────────────────
 * Both algo_any() and algo_all() return FALSE for empty sequences (len == 0).
 *
 * This departs from standard mathematical convention, where:
 *   - ∃x. P(x) over {} = false   ← matches this implementation
 *   - ∀x. P(x) over {} = true    ← does NOT match (vacuous truth)
 *
 * Rationale: returning true from algo_all() on an empty input is a common
 * source of bugs in defensive code (e.g. "all inputs are valid" passing when
 * there were no inputs). The non-vacuous behavior forces callers to be
 * explicit about empty-sequence intent.
 *
 * If vacuous truth is required, guard the call site:
 * ```c
 * bool result = (len == 0) ? true : algo_all(base, len, elem_size, pred, ctx);
 * ```
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
 * Step 0: Check 1 — pred(1) = false, continue
 * Step 1: Check 2 — pred(2) = false, continue
 * Step 2: Check -3 — pred(-3) = true, return true ✓
 * (4 is never checked — short-circuit)
 *
 * ALL (universal quantification):
 * 1. Iterate through elements left-to-right
 * 2. Apply predicate to each element
 * 3. On first false: return false immediately (short-circuit)
 * 4. If all true: return true
 *
 * Example: ALL positive in [1, 2, -3, 4]
 * Step 0: Check 1 — pred(1) = true, continue
 * Step 1: Check 2 — pred(2) = true, continue
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
 * @param base Pointer to first element of the array (borrowed, read-only)
 * @param len Number of elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred Predicate function (borrowed)
 * @param ctx Optional context passed to predicate (borrowed, may be NULL)
 *
 * @return true if ∃i: pred(&base[i]) == true, false otherwise
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred, not stored)
 *
 * Performance:
 * - Time: O(k) where k = index of first match (worst case O(n))
 * - Space: O(1)
 * - Best case: O(1) if first element matches
 *
 * Returns false immediately if:
 * - base == NULL
 * - pred == NULL
 * - len == 0
 * - elem_size == 0
 */
static inline bool algo_any(
    borrowed const void* base,
    usize len,
    usize elem_size,
    borrowed algo_pred_fn pred,
    borrowed void* ctx)
{
    require_msg(elem_size > 0, "algo_any: elem_size must be > 0");
   
    if (!base || !pred || len == 0) return false;
   
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
 * ⚠️  EMPTY SEQUENCE: returns false when len == 0.
 * This deliberately does NOT implement vacuous truth (unlike mathematics,
 * where ∀x.P(x) over an empty set is true). If you need vacuous truth,
 * guard explicitly: (len == 0) ? true : algo_all(...)
 *
 * @param base Pointer to first element (borrowed, read-only)
 * @param len Number of elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred Predicate function (borrowed)
 * @param ctx Optional context (borrowed, may be NULL)
 *
 * @return true if ∀i: pred(&base[i]) == true AND len > 0, false otherwise
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred, not stored)
 *
 * Performance:
 * - Time: O(k) where k = index of first failure (worst case O(n))
 * - Space: O(1)
 * - Best case: O(1) if first element fails
 *
 * Returns false if:
 * - base == NULL
 * - pred == NULL
 * - len == 0 (empty sequences return false — no vacuous truth; see note above)
 * - elem_size == 0
 * - Any element fails the predicate
 */
static inline bool algo_all(
    borrowed const void* base,
    usize len,
    usize elem_size,
    borrowed algo_pred_fn pred,
    borrowed void* ctx)
{
    require_msg(elem_size > 0, "algo_all: elem_size must be > 0");
   
    if (!base || !pred || len == 0) return false;
   
    for (usize i = 0; i < len; i++) {
        if (!pred(ptr_elem_const(base, i, elem_size), ctx)) return false;
    }
    return true;
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
 * @param len Number of elements
 * @param Type Element type
 * @param pred Predicate: bool (*)(const Type*, void*) (borrowed)
 * @param ctx Optional context (borrowed, may be NULL)
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
 * ⚠️  Returns false for empty arrays (len == 0). See algo_all() for details.
 *
 * @param items Array of Type (borrowed, read-only)
 * @param len Number of elements
 * @param Type Element type
 * @param pred Predicate: bool (*)(const Type*, void*) (borrowed)
 * @param ctx Optional context (borrowed, may be NULL)
 *
 * @return bool — true if all elements satisfy pred AND len > 0
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
 * - algo_all_slice_##type(sv, pred, ctx) → bool
 *
 * ⚠️  Both generated functions return false for empty slices (sv.len == 0).
 * See the ⚠️ EMPTY SEQUENCE note in the file header for details.
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
 * @param sv Typed slice view (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @return true if ∃ element satisfying pred, false if empty or none match \
 */ \
static inline bool algo_any_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_pred_fn pred, \
    borrowed void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return false; \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Returns true if all elements in slice_##type satisfy predicate \
 * \
 * ⚠️  Returns false for empty slices (sv.len == 0) — no vacuous truth. \
 * Guard explicitly if needed: (sv.len == 0) ? true : algo_all_slice_##type(...) \
 * \
 * @param sv Typed slice view (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @return true if ∀ elements satisfy pred AND sv.len > 0 \
 */ \
static inline bool algo_all_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_pred_fn pred, \
    borrowed void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return false; \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (!pred(&sv.ptr[_i], ctx)) return false; \
    } \
    return true; \
}

#endif /* CANON_ALGO_ANY_ALL_H */
