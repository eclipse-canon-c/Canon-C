#ifndef CANON_ALGO_FIND_H
#define CANON_ALGO_FIND_H
#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/option.h"
#include "core/primitives/compare.h"   // provides algo_pred_fn

/**
 * @file algo/find.h
 * @brief Locate the first element in a sequence matching a predicate
 *
 * Scans a sequence left-to-right and returns the index and/or a pointer to
 * the first element satisfying the predicate. Short-circuits on first match.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Input is read-only — never mutated
 * - Short-circuit evaluation — stops immediately on first match
 * - No allocation — O(1) space
 * - Predicate type is algo_pred_fn from core/primitives/compare.h
 * - Three levels: generic flat-array, typed inline macro, slice+option variant
 * - Slice variants return option_##type — Canon-C idiom for "found or absent"
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Linear search with early termination:
 * 1. Iterate through elements left-to-right
 * 2. Apply predicate to each element
 * 3. On first true result: return immediately with index/pointer
 * 4. If no match found: return false or None
 *
 * Example: Finding first negative in [5, 3, -2, 7]
 * Step 0: Check 5 — pred(5) = false, continue
 * Step 1: Check 3 — pred(3) = false, continue
 * Step 2: Check -2 — pred(-2) = true, return index=2, value=-2 ✓
 * (7 is never checked — short-circuit)
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * find.h depends on semantics/option.h for option_##type in slice variants.
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
 * - Time complexity: O(k) where k = index of first match (worst case O(n))
 * - Space complexity: O(1) — no allocation
 * - Short-circuit: stops immediately on first match
 * - Best case: O(1) if first element matches
 * - Worst case: O(n) if no match or last element matches
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Finding first element matching criteria
 * - Searching for specific values with custom conditions
 * - Locating outliers or invalid data
 * - Finding minimum/maximum with custom comparison
 * - Detecting presence of pattern in data
 * - Early validation (fail fast on invalid data)
 * - Index lookup for further processing
 *
 * Quick start:
 * ```c
 * #include "algo/find.h"
 *
 * bool is_negative(const void* e, void* ctx) {
 *     return *(const int*)e < 0;
 * }
 *
 * int arr[] = {5, 3, -2, 7};
 *
 * // Generic — returns index via out param
 * usize idx;
 * const void* elem;
 * bool found = algo_find(arr, 4, sizeof(int), is_negative, NULL, &idx, &elem);
 * if (found) {
 *     printf("Found at index %zu: %d\n", idx, *(const int*)elem);
 * }
 *
 * // Typed macro — clean call, typed out_elem
 * const int* p = NULL;
 * if (ALGO_FIND_TYPED(arr, 4, int, is_negative, NULL, &idx, &p)) {
 *     printf("Found at index %zu: %d\n", idx, *p);
 * }
 *
 * // Slice variant — returns option_int
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FIND(int)
 * slice_int sv = slice_int_from(arr, 4);
 * option_int opt = algo_find_slice_int(sv, is_negative, NULL);
 * if (opt.has_value) {
 *     printf("Found: %d\n", opt.value);
 * }
 *
 * // Index-only variant
 * usize idx = algo_find_index_slice_int(sv, is_negative, NULL);
 * if (idx != CANON_USIZE_MAX) {
 *     printf("Found at index: %zu\n", idx);
 * }
 * ```
 *
 * @sa core/primitives/compare.h — algo_pred_fn predicate type
 * @sa core/slice.h — slice_##type used by DEFINE_ALGO_FIND
 * @sa semantics/option.h — option_##type returned by slice variant
 * @sa core/primitives/ptr.h — ptr_elem_const used in generic interface
 * @sa core/ownership.h — borrowed macro for non-owning parameters
 */
/* ════════════════════════════════════════════════════════════════════════════
   Generic flat-array interface
   ════════════════════════════════════════════════════════════════════════════
   Uses ptr_elem_const() from ptr.h to stride over the flat array.
   Both out_index and out_elem are optional — pass NULL to ignore.
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Finds the first element in a flat array satisfying the predicate
 *
 * Performs linear search with short-circuit on first match.
 * Both output parameters are optional — pass NULL to ignore either.
 *
 * @param base Pointer to first element (borrowed, read-only)
 * @param len Number of elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred Predicate — return true on match (borrowed)
 * @param ctx Optional context passed to pred (borrowed, may be NULL)
 * @param out_index Optional — receives index of first match (borrowed, may be NULL)
 * @param out_elem Optional — receives const pointer to match (borrowed, may be NULL)
 *
 * @return true if a match was found, false otherwise
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 *
 * @post On true: out_index and/or out_elem contain match information
 * @post On false: out_index and out_elem are not modified
 * @post base array is unchanged (read-only operation)
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred, not stored)
 * - out_index: borrowed (output parameter, written if non-NULL)
 * - out_elem: borrowed (output parameter, written if non-NULL)
 *
 * Performance:
 * - Time: O(k) where k = index of first match, O(n) worst case
 * - Space: O(1)
 * - Short-circuit: stops on first match
 *
 * Thread-safety:
 * - Safe to call concurrently as long as base is not being modified
 *
 * Returns false immediately if:
 * - base == NULL
 * - pred == NULL
 * - len == 0
 * - elem_size == 0
 */
static inline bool algo_find(
    borrowed const void* base,
    usize len,
    usize elem_size,
    borrowed algo_pred_fn pred,
    borrowed void* ctx,
    borrowed usize* out_index, /* optional */
    borrowed const void** out_elem) /* optional */
{
    require_msg(elem_size > 0, "algo_find: elem_size must be > 0");
   
    if (!base || !pred || len == 0) return false;
    for (usize i = 0; i < len; i++) {
        const void* elem = ptr_elem_const(base, i, elem_size);
        if (pred(elem, ctx)) {
            if (out_index) *out_index = i;
            if (out_elem) *out_elem = elem;
            return true;
        }
    }
    return false;
}
/* ════════════════════════════════════════════════════════════════════════════
   Typed macro — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════
   C99-portable. Typed out_elem avoids the void** cast at the call site.
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @def ALGO_FIND_TYPED(base, len, Type, pred, ctx, out_index, out_elem_ptr)
 * @brief Type-safe find over a C array with optional typed output pointer
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type)
 * and avoiding void pointer casts at the call site.
 *
 * @param base Input array of Type (borrowed, read-only)
 * @param len Number of elements
 * @param Type Element type
 * @param pred Predicate: algo_pred_fn (borrowed)
 * @param ctx Optional context (borrowed, may be NULL)
 * @param out_index Optional usize* — receives index of match (borrowed, may be NULL)
 * @param out_elem_ptr Optional const Type** — receives pointer to match (borrowed, may be NULL)
 *
 * @return bool — true if found, false otherwise
 */
#define ALGO_FIND_TYPED(base, len, Type, pred, ctx, out_index, out_elem_ptr) \
    algo_find( \
        (base), (len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx), \
        (out_index), \
        (const void**)(out_elem_ptr))
/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_FIND — typed slice + option variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires:
   - DEFINE_SLICE(type) from core/slice.h
   - DEFINE_OPTION(type) from semantics/option.h (or option_##type exists)
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Generates find functions that accept slice_##type and return option_##type
 *
 * Prerequisites:
 * - DEFINE_SLICE(type) must have been called
 * - option_##type must exist (via DEFINE_OPTION(type))
 *
 * Generated functions:
 * - algo_find_slice_##type(sv, pred, ctx) → option_##type
 * - algo_find_index_slice_##type(sv, pred, ctx) → usize (CANON_USIZE_MAX if not found)
 */
#define DEFINE_ALGO_FIND(type) \
\
/** \
 * @brief Returns the first element in slice_##type satisfying pred as option_##type \
 * \
 * @param sv Input slice (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @return option_##type — Some(value) on match, None if not found or empty \
 */ \
static inline option_##type algo_find_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_pred_fn pred, \
    borrowed void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return option_##type##_none(); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) { \
            return option_##type##_some(sv.ptr[_i]); \
        } \
    } \
    return option_##type##_none(); \
} \
\
/** \
 * @brief Returns the index of the first matching element, or CANON_USIZE_MAX if not found \
 * \
 * @param sv Input slice (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @return usize — index of first match, or CANON_USIZE_MAX if not found \
 */ \
static inline usize algo_find_index_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_pred_fn pred, \
    borrowed void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return CANON_USIZE_MAX; \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) return _i; \
    } \
    return CANON_USIZE_MAX; \
}

#endif /* CANON_ALGO_FIND_H */
