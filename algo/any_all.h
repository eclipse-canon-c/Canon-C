#ifndef CANON_ALGO_ANY_ALL_H
#define CANON_ALGO_ANY_ALL_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"

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
 * - Three levels: generic flat-array, typed macro, cbytes_t / slice variants
 * - Predicates receive a const pointer to each element and an optional context
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/ and semantics/ only.
 * No data/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No GCC statement expressions — all macros are C99 portable
 * - No platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(k) where k = index of first match/failure (worst case O(n))
 * - Space: O(1) — no allocations
 *
 * Quick start:
 * ```c
 * #include "algo/any_all.h"
 *
 * bool is_negative(const int* elem, void* ctx) { return *elem < 0; }
 *
 * int nums[] = {1, 2, -3, 4};
 *
 * // Typed macro — recommended
 * if (ALGO_ANY_TYPED(nums, 4, int, is_negative, NULL)) { ... }
 * if (ALGO_ALL_TYPED(nums, 4, int, is_negative, NULL)) { ... }
 *
 * // Slice variant
 * DEFINE_SLICE(int)
 * slice_int sv = slice_int_from(nums, 4);
 * if (algo_any_slice_int(sv, is_negative, NULL)) { ... }
 * ```
 *
 * @sa core/slice.h — slice_##type, cbytes_t used by slice variants
 * @sa core/primitives/ptr.h — ptr_elem_const used in generic interface
 */

/* ════════════════════════════════════════════════════════════════════════════
   Predicate type
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Predicate function type for any/all operations
 *
 * @param elem  Read-only pointer to the current element (never NULL)
 * @param ctx   Optional caller context (may be NULL)
 * @return true if the element satisfies the condition, false otherwise
 *
 * Example:
 * ```c
 * bool is_even(const void* elem, void* ctx) {
 *     return (*(const int*)elem % 2) == 0;
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
 * @param base      Pointer to first element of the array (NULL-safe)
 * @param len       Number of elements
 * @param elem_size Size of each element in bytes (sizeof(type))
 * @param pred      Predicate function (NULL-safe — returns false)
 * @param ctx       Optional context passed to predicate (may be NULL)
 * @return true if ∃i: pred(&base[i]) == true, false otherwise
 *
 * @note Returns false immediately if base == NULL, pred == NULL, or len == 0
 *
 * Performance:
 * - Time:  O(k) where k = index of first match (worst case O(n))
 * - Space: O(1)
 */
static inline bool algo_any(
    const void* base,
    usize       len,
    usize       elem_size,
    algo_pred_fn pred,
    void*       ctx)
{
    if (!base || !pred || len == 0 || elem_size == 0) return false;
    for (usize i = 0; i < len; i++) {
        if (pred(ptr_elem_const(base, i, elem_size), ctx)) return true;
    }
    return false;
}

/**
 * @brief Returns true if ALL elements in a flat array satisfy predicate
 *
 * @param base      Pointer to first element (NULL-safe)
 * @param len       Number of elements
 * @param elem_size Size of each element in bytes
 * @param pred      Predicate function (NULL-safe — returns false)
 * @param ctx       Optional context (may be NULL)
 * @return true if ∀i: pred(&base[i]) == true, false otherwise
 *
 * @note Returns false if base == NULL, pred == NULL, or len == 0
 * @note Empty sequences return false (no vacuous truth)
 *
 * Performance:
 * - Time:  O(k) where k = index of first failure (worst case O(n))
 * - Space: O(1)
 */
static inline bool algo_all(
    const void* base,
    usize       len,
    usize       elem_size,
    algo_pred_fn pred,
    void*       ctx)
{
    if (!base || !pred || len == 0 || elem_size == 0) return false;
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
 * @param items Array of Type (read-only)
 * @param len   Number of elements
 * @param Type  Element type
 * @param pred  Predicate: bool (*)(const Type*, void*)
 * @param ctx   Optional context (may be NULL)
 * @return bool — true if any element satisfies pred
 *
 * Example:
 * ```c
 * bool is_negative(const int* e, void* ctx) { return *e < 0; }
 * int nums[] = {1, -2, 3};
 * if (ALGO_ANY_TYPED(nums, 3, int, is_negative, NULL)) { ... }
 * ```
 */
#define ALGO_ANY_TYPED(items, len, Type, pred, ctx) \
    algo_any((items), (len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx))

/**
 * @def ALGO_ALL_TYPED(items, len, Type, pred, ctx)
 * @brief Type-safe ALL check over a C array
 *
 * @param items Array of Type (read-only)
 * @param len   Number of elements
 * @param Type  Element type
 * @param pred  Predicate: bool (*)(const Type*, void*)
 * @param ctx   Optional context (may be NULL)
 * @return bool — true if all elements satisfy pred
 *
 * Example:
 * ```c
 * bool is_positive(const int* e, void* ctx) { return *e > 0; }
 * int nums[] = {1, 2, 3};
 * if (ALGO_ALL_TYPED(nums, 3, int, is_positive, NULL)) { ... }
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
 * if (algo_any_slice_int(sv, is_zero, NULL)) { ... }
 * if (algo_all_slice_int(sv, is_zero, NULL)) { ... }
 * ```
 */
#define DEFINE_ALGO_ANY_ALL(type) \
\
/** \
 * @brief Returns true if any element in slice_##type satisfies predicate \
 * \
 * @param sv   Typed slice view (non-owning) \
 * @param pred Predicate: bool (*)(const type*, void*) \
 * @param ctx  Optional context (may be NULL) \
 * @return true if ∃ element satisfying pred \
 * \
 * Performance: O(k) short-circuit, O(n) worst case \
 */ \
static inline bool algo_any_slice_##type( \
    slice_##type sv, \
    bool (*pred)(const type*, void*), \
    void* ctx) \
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
 * @param sv   Typed slice view (non-owning) \
 * @param pred Predicate: bool (*)(const type*, void*) \
 * @param ctx  Optional context (may be NULL) \
 * @return true if ∀ elements satisfy pred (false if empty) \
 * \
 * Performance: O(k) short-circuit, O(n) worst case \
 */ \
static inline bool algo_all_slice_##type( \
    slice_##type sv, \
    bool (*pred)(const type*, void*), \
    void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return false; \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (!pred(&sv.ptr[_i], ctx)) return false; \
    } \
    return true; \
}

#endif /* CANON_ALGO_ANY_ALL_H */
