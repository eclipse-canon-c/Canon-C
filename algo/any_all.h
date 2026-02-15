#ifndef CANON_ALGO_ANY_ALL_H
#define CANON_ALGO_ANY_ALL_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
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
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * ANY (existential quantification):
 *   1. Iterate through elements left-to-right
 *   2. Apply predicate to each element
 *   3. On first true: return true immediately (short-circuit)
 *   4. If all false: return false
 *
 * Example: ANY negative in [1, 2, -3, 4]
 *   Step 0: Check 1 — pred(1) = false, continue
 *   Step 1: Check 2 — pred(2) = false, continue
 *   Step 2: Check -3 — pred(-3) = true, return true ✓
 *   (4 is never checked — short-circuit)
 *
 * ALL (universal quantification):
 *   1. Iterate through elements left-to-right
 *   2. Apply predicate to each element
 *   3. On first false: return false immediately (short-circuit)
 *   4. If all true: return true
 *
 * Example: ALL positive in [1, 2, -3, 4]
 *   Step 0: Check 1 — pred(1) = true, continue
 *   Step 1: Check 2 — pred(2) = true, continue
 *   Step 2: Check -3 — pred(-3) = false, return false ✓
 *   (4 is never checked — short-circuit)
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * any_all.h has minimal dependencies — just types, slices, and ownership.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No GCC statement expressions — all macros are C99 portable
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
 * - Data quality: checking if all data meets standards
 * - Boolean logic: implementing complex conditions
 *
 * Quick start:
 * ```c
 * #include "algo/any_all.h"
 *
 * bool is_negative(const int* elem, void* ctx) { 
 *     return *elem < 0; 
 * }
 *
 * int nums[] = {1, 2, -3, 4};
 *
 * // Typed macros — recommended
 * if (ALGO_ANY_TYPED(nums, 4, int, is_negative, NULL)) {
 *     printf("Found negative number\n");
 * }
 * if (ALGO_ALL_TYPED(nums, 4, int, is_negative, NULL)) {
 *     printf("All numbers are negative\n");
 * }
 *
 * // Slice variant
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_ANY_ALL(int)
 * slice_int sv = slice_int_from(nums, 4);
 * if (algo_any_slice_int(sv, is_negative, NULL)) { ... }
 * if (algo_all_slice_int(sv, is_negative, NULL)) { ... }
 * ```
 *
 * @sa core/slice.h           — slice_##type used by slice variants
 * @sa core/primitives/ptr.h  — ptr_elem_const used in generic interface
 * @sa core/ownership.h       — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Predicate type
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Predicate function type for any/all operations
 *
 * @param elem  Read-only pointer to the current element (borrowed, never NULL)
 * @param ctx   Optional caller context (borrowed, may be NULL)
 *
 * @return true if the element satisfies the condition, false otherwise
 *
 * Ownership:
 * - elem: borrowed (read-only, valid only during callback)
 * - ctx: borrowed (passed through from caller)
 *
 * Example:
 * ```c
 * bool is_even(const void* elem, void* ctx) {
 *     return (*(const int*)elem % 2) == 0;
 * }
 * ```
 *
 * Example with context:
 * ```c
 * bool is_greater_than(const void* elem, void* ctx) {
 *     int threshold = *(int*)ctx;
 *     return *(const int*)elem > threshold;
 * }
 * ```
 */
typedef bool (*algo_pred_fn)(const void* elem, void* ctx);

/* ════════════════════════════════════════════════════════════════════════════
   Generic flat-array interface
   ════════════════════════════════════════════════════════════════════════════
   Operates on a flat contiguous array (not an array of pointers).
   Uses ptr_elem_const() from ptr.h to index each element by byte offset.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if ANY element in a flat array satisfies predicate
 *
 * Implements existential quantification (∃): checks if at least one element
 * satisfies the predicate. Short-circuits on first match.
 *
 * @param base      Pointer to first element of the array (borrowed, read-only)
 * @param len       Number of elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context passed to predicate (borrowed, may be NULL)
 *
 * @return true if ∃i: pred(&base[i]) == true, false otherwise
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 *
 * @post base is unchanged (read-only operation)
 * @post Returns immediately on first match (short-circuit)
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred, not stored)
 *
 * Performance:
 * - Time:  O(k) where k = index of first match (worst case O(n))
 * - Space: O(1)
 * - Best case: O(1) if first element matches
 *
 * Thread-safety:
 * - Safe to call concurrently as long as base is not being modified
 *
 * Returns false immediately if:
 * - base == NULL
 * - pred == NULL
 * - len == 0
 * - elem_size == 0
 *
 * Example:
 * ```c
 * bool is_zero(const void* e, void* ctx) {
 *     return *(const int*)e == 0;
 * }
 *
 * int arr[] = {1, 2, 0, 4};
 * if (algo_any(arr, 4, sizeof(int), is_zero, NULL)) {
 *     printf("Found zero\n");  // Stops at index 2
 * }
 * ```
 */
static inline bool algo_any(
    borrowed const void*  base,
    usize                 len,
    usize                 elem_size,
    borrowed algo_pred_fn pred,
    borrowed void*        ctx)
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
 * @param base      Pointer to first element (borrowed, read-only)
 * @param len       Number of elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred      Predicate function (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 *
 * @return true if ∀i: pred(&base[i]) == true, false otherwise
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 *
 * @post base is unchanged (read-only operation)
 * @post Returns immediately on first failure (short-circuit)
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred, not stored)
 *
 * Performance:
 * - Time:  O(k) where k = index of first failure (worst case O(n))
 * - Space: O(1)
 * - Best case: O(1) if first element fails
 *
 * Thread-safety:
 * - Safe to call concurrently as long as base is not being modified
 *
 * Returns false if:
 * - base == NULL
 * - pred == NULL
 * - len == 0 (empty sequences return false - no vacuous truth)
 * - elem_size == 0
 * - Any element fails the predicate
 *
 * Example:
 * ```c
 * bool is_positive(const void* e, void* ctx) {
 *     return *(const int*)e > 0;
 * }
 *
 * int arr[] = {1, 2, 3, 4};
 * if (algo_all(arr, 4, sizeof(int), is_positive, NULL)) {
 *     printf("All positive\n");  // Checks all 4 elements
 * }
 *
 * int arr2[] = {1, -2, 3, 4};
 * if (algo_all(arr2, 4, sizeof(int), is_positive, NULL)) {
 *     // Not reached
 * } else {
 *     printf("Not all positive\n");  // Stops at index 1
 * }
 * ```
 */
static inline bool algo_all(
    borrowed const void*  base,
    usize                 len,
    usize                 elem_size,
    borrowed algo_pred_fn pred,
    borrowed void*        ctx)
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
   ════════════════════════════════════════════════════════════════════════════
   C99-portable — no GCC statement expressions.
   Delegate to algo_any / algo_all with sizeof(Type) baked in.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_ANY_TYPED(items, len, Type, pred, ctx)
 * @brief Type-safe ANY check over a C array
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 *
 * @param items Array of Type (borrowed, read-only)
 * @param len   Number of elements
 * @param Type  Element type
 * @param pred  Predicate: bool (*)(const Type*, void*) (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return bool — true if any element satisfies pred
 *
 * Ownership:
 * - items: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred)
 *
 * Performance:
 * - Time:  O(k) short-circuit
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as items is not being modified
 *
 * Example:
 * ```c
 * bool is_negative(const int* e, void* ctx) { 
 *     return *e < 0; 
 * }
 * int nums[] = {1, -2, 3};
 * if (ALGO_ANY_TYPED(nums, 3, int, is_negative, NULL)) {
 *     printf("Found negative\n");
 * }
 * ```
 *
 * Example with context:
 * ```c
 * bool is_divisible_by(const int* e, void* ctx) {
 *     int divisor = *(int*)ctx;
 *     return (*e % divisor) == 0;
 * }
 * int divisor = 3;
 * if (ALGO_ANY_TYPED(nums, 3, int, is_divisible_by, &divisor)) { ... }
 * ```
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
 * @param items Array of Type (borrowed, read-only)
 * @param len   Number of elements
 * @param Type  Element type
 * @param pred  Predicate: bool (*)(const Type*, void*) (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return bool — true if all elements satisfy pred
 *
 * Ownership:
 * - items: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred)
 *
 * Performance:
 * - Time:  O(k) short-circuit
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as items is not being modified
 *
 * Example:
 * ```c
 * bool is_positive(const int* e, void* ctx) { 
 *     return *e > 0; 
 * }
 * int nums[] = {1, 2, 3};
 * if (ALGO_ALL_TYPED(nums, 3, int, is_positive, NULL)) {
 *     printf("All positive\n");
 * }
 * ```
 *
 * Example - Validation:
 * ```c
 * bool is_valid_score(const int* e, void* ctx) {
 *     return *e >= 0 && *e <= 100;
 * }
 * int scores[] = {85, 90, 78, 92};
 * if (ALGO_ALL_TYPED(scores, 4, int, is_valid_score, NULL)) {
 *     printf("All scores valid\n");
 * }
 * ```
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
 * - algo_any_slice_##type(sv, pred, ctx)  → bool
 * - algo_all_slice_##type(sv, pred, ctx)  → bool
 *
 * The predicate receives a const type* pointer to each element.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Ownership:
 * - sv: borrowed (read-only slice)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred)
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_ANY_ALL(int)
 *
 * bool is_zero(const int* e, void* ctx) { return *e == 0; }
 *
 * int arr[] = {1, 0, 2};
 * slice_int sv = slice_int_from(arr, 3);
 *
 * if (algo_any_slice_int(sv, is_zero, NULL)) {
 *     printf("Contains zero\n");
 * }
 * if (algo_all_slice_int(sv, is_zero, NULL)) {
 *     printf("All zeros\n");
 * }
 * ```
 */
#define DEFINE_ALGO_ANY_ALL(type) \
\
/** \
 * @brief Returns true if any element in slice_##type satisfies predicate \
 * \
 * @param sv   Typed slice view (borrowed, read-only) \
 * @param pred Predicate: bool (*)(const type*, void*) (borrowed) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * \
 * @return true if ∃ element satisfying pred \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * @pre sv.ptr points to valid array if non-NULL \
 * \
 * Ownership: \
 * - sv: borrowed (read-only) \
 * - pred: borrowed (function pointer used during call only) \
 * - ctx: borrowed (passed through to pred) \
 * \
 * Performance: \
 * - Time:  O(k) short-circuit, O(n) worst case \
 * - Space: O(1) \
 * \
 * Thread-safety: \
 * - Safe to call concurrently as long as slice is not being modified \
 */ \
static inline bool algo_any_slice_##type( \
    borrowed slice_##type sv, \
    borrowed bool (*pred)(const type*, void*), \
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
 * @param sv   Typed slice view (borrowed, read-only) \
 * @param pred Predicate: bool (*)(const type*, void*) (borrowed) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * \
 * @return true if ∀ elements satisfy pred (false if empty) \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * @pre sv.ptr points to valid array if non-NULL \
 * \
 * Ownership: \
 * - sv: borrowed (read-only) \
 * - pred: borrowed (function pointer used during call only) \
 * - ctx: borrowed (passed through to pred) \
 * \
 * Performance: \
 * - Time:  O(k) short-circuit, O(n) worst case \
 * - Space: O(1) \
 * \
 * Thread-safety: \
 * - Safe to call concurrently as long as slice is not being modified \
 */ \
static inline bool algo_all_slice_##type( \
    borrowed slice_##type sv, \
    borrowed bool (*pred)(const type*, void*), \
    borrowed void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return false; \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (!pred(&sv.ptr[_i], ctx)) return false; \
    } \
    return true; \
}

#endif /* CANON_ALGO_ANY_ALL_H */
