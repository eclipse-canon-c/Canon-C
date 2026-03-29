/**
 * @file any_all.h
 * @brief Predicate testing over sequences — existential and universal quantification
 *
 * This is the entry point for header-only usage. Including this file
 * generates statically-inlined implementations of algo_any and algo_all,
 * plus typed macro wrappers and the DEFINE_ALGO_ANY_ALL instantiation macro.
 *
 * For separate compilation (external linkage), use any_all_decl.h in
 * headers and any_all_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Level 1 — Generic (void* interface, works on any type):
 * ```c
 * static bool is_negative(const void* elem, void* ctx) {
 *     (void)ctx;
 *     return *(const int*)elem < 0;
 * }
 *
 * int arr[] = {1, 2, -3, 4};
 * bool found = algo_any(arr, 4, sizeof(int), is_negative, NULL); // true
 * bool all_pos = algo_all(arr, 4, sizeof(int), is_negative, NULL); // false
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof, same pred signature):
 * ```c
 * bool found   = ALGO_ANY_TYPED(arr, 4, int, is_negative, NULL);
 * bool all_pos = ALGO_ALL_TYPED(arr, 4, int, is_negative, NULL);
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, fully optimizable):
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_ANY_ALL(int)
 *
 * int buf[] = {1, 2, -3, 4};
 * slice_int sv = slice_int_from(buf, 4);
 * bool found   = algo_any_slice_int(sv, is_negative, NULL);  // true
 * bool all_pos = algo_all_slice_int(sv, is_negative, NULL);  // false
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * EMPTY SEQUENCE BEHAVIOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_any() returns false for empty sequences — no element satisfies
 *            the predicate when there are no elements.
 *            ∃x. P(x) over {} = false
 *
 * algo_all() returns true for empty sequences — vacuous truth.
 *            No element violates the predicate when there are no elements.
 *            ∀x. P(x) over {} = true
 *
 * This matches standard mathematical convention and every major language
 * standard library (C++, Rust, Python, Haskell). The two operations are
 * duals: algo_all(empty) == true, algo_any(empty) == false.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_any(base, len, elem_size, pred, ctx) → bool
 *   algo_all(base, len, elem_size, pred, ctx) → bool
 *
 * Typed macros:
 *   ALGO_ANY_TYPED(items, len, Type, pred, ctx) → bool
 *   ALGO_ALL_TYPED(items, len, Type, pred, ctx) → bool
 *
 * Typed instantiation (call DEFINE_ALGO_ANY_ALL(type) first):
 *   algo_any_slice_##type(sv, pred, ctx) → bool
 *   algo_all_slice_##type(sv, pred, ctx) → bool
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PERFORMANCE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Time complexity — all three API levels:
 *
 *   algo_any / ALGO_ANY_TYPED / algo_any_slice_##type
 *     Best case:  O(1) — first element satisfies pred (immediate return)
 *     Worst case: O(n) — no element satisfies pred (full scan)
 *     Average:    O(k) where k = index of first match + 1
 *
 *   algo_all / ALGO_ALL_TYPED / algo_all_slice_##type
 *     Best case:  O(1) — first element fails pred (immediate return)
 *                 O(1) — empty sequence (no iterations, returns true)
 *     Worst case: O(n) — all elements satisfy pred (full scan)
 *     Average:    O(k) where k = index of first failure + 1
 *
 * Space complexity — all variants:
 *   O(1) — no heap allocation, no recursion, no auxiliary storage.
 *   Stack frame is constant regardless of sequence length.
 *
 * Predicate call count:
 *   Both functions short-circuit. pred is called at most n times and
 *   as few as 0 times (empty sequence) or 1 time (immediate match/fail).
 *   pred is never called with a NULL elem pointer.
 *
 * Level comparison:
 *
 *   Level 1 — Generic (algo_any / algo_all)
 *     Extra cost: one pointer arithmetic step per element via ptr_elem_const
 *     (byte-offset cast). Negligible — a single multiply-add per iteration.
 *     Suitable for any flat C array of any element type.
 *
 *   Level 2 — Typed macro (ALGO_ANY_TYPED / ALGO_ALL_TYPED)
 *     Identical generated code to Level 1. sizeof(Type) is a compile-time
 *     constant so the multiply is eliminated by the optimizer. Zero overhead
 *     vs. Level 1 in optimized builds.
 *
 *   Level 3 — Typed slice (algo_any_slice_##type / algo_all_slice_##type)
 *     No void* anywhere. The compiler sees the actual element type, enabling
 *     direct pointer indexing (&sv.ptr[i]) with no stride multiply.
 *     Best codegen: eligible for auto-vectorization on modern CPUs.
 *     Recommended for hot paths and large arrays.
 *
 * Cache behavior:
 *   Linear scan — sequential memory access pattern. Optimal cache locality
 *   for arrays that fit in L1/L2. For very large arrays pred is the primary
 *   cost; the scan itself is branch-predictor-friendly (single loop counter).
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PREDICATE SIGNATURE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * All three levels use the same predicate type (algo_pred_fn from compare.h):
 *   bool pred(const void* elem, void* ctx)
 *
 * elem is never NULL — it always points to a valid array element.
 * ctx  is the optional caller context passed through unchanged; may be NULL.
 *
 * @sa any_all_mangle.h — name customization for slice variants
 * @sa any_all_decl.h   — forward declarations for separate compilation
 * @sa any_all_defn.h   — definitions for separate compilation
 * @sa any_all_impl.h   — pure logic (do not include directly)
 * @sa core/primitives/compare.h — algo_pred_fn definition
 */

#ifndef CANON_ALGO_ANY_ALL_H
#define CANON_ALGO_ANY_ALL_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — all functions are inlined at call sites.
 */
#define ALGO_ANY_ALL_LINKAGE static inline

#include "any_all_impl.h"   /* implementation logic — NOT any_all_defn.h */

#undef ALGO_ANY_ALL_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_ANY_TYPED(items, len, Type, pred, ctx)
 * @brief Type-safe ANY check over a C array
 *
 * Wraps algo_any() with automatic sizeof(Type), eliminating the manual
 * elem_size argument and reducing the chance of sizeof mismatches.
 *
 * @param items C array of Type (borrowed, read-only)
 * @param len   Number of elements (0 is valid — returns false)
 * @param Type  Element type — used for sizeof only
 * @param pred  Predicate: bool (*)(const void*, void*) (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return bool — true if any element satisfies pred, false otherwise
 */
#define ALGO_ANY_TYPED(items, len, Type, pred, ctx) \
    algo_any((items), (len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx))

/**
 * @def ALGO_ALL_TYPED(items, len, Type, pred, ctx)
 * @brief Type-safe ALL check over a C array
 *
 * Wraps algo_all() with automatic sizeof(Type). Returns true for empty
 * arrays (len == 0) — vacuous truth.
 *
 * @param items C array of Type (borrowed, read-only)
 * @param len   Number of elements (0 is valid — returns true)
 * @param Type  Element type — used for sizeof only
 * @param pred  Predicate: bool (*)(const void*, void*) (borrowed)
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
   Generates fully typed functions accepting slice_##type directly.
   No void* anywhere — the compiler sees the actual element type.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates any/all functions operating directly on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 *   algo_any_slice_##type(sv, pred, ctx) → bool
 *     Returns false for empty slices (sv.len == 0).
 *
 *   algo_all_slice_##type(sv, pred, ctx) → bool
 *     Returns true for empty slices (sv.len == 0) — vacuous truth.
 *
 * The predicate receives a const type* pointer to each element and
 * the caller-supplied ctx. Same algo_pred_fn signature as the generic level.
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h invariants). The loop never executes when sv.len == 0, so a
 * NULL ptr is safe. Only pred is required to be non-NULL.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_ANY_ALL(int)
 *
 * static bool is_negative(const void* elem, void* ctx) {
 *     (void)ctx;
 *     return *(const int*)elem < 0;
 * }
 *
 * int buf[] = {1, 2, -3};
 * slice_int sv = slice_int_from(buf, 3);
 * bool found = algo_any_slice_int(sv, is_negative, NULL); // true
 * bool all   = algo_all_slice_int(sv, is_negative, NULL); // false
 *
 * // Empty slice — no crash, correct vacuous results
 * slice_int empty = slice_int_empty();
 * bool any_empty = algo_any_slice_int(empty, is_negative, NULL); // false
 * bool all_empty = algo_all_slice_int(empty, is_negative, NULL); // true
 * ```
 */
#define DEFINE_ALGO_ANY_ALL(type) \
\
/** \
 * @brief Returns true if any element in slice_##type satisfies predicate \
 * \
 * Short-circuits on first match. Returns false for empty slices. \
 * \
 * @param sv   Typed slice (borrowed, read-only) \
 * @param pred Predicate function — bool (*)(const void*, void*) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * @return true if any element satisfies pred, false if empty or none match \
 */ \
static inline bool ALGO_ANY_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_pred_fn)  pred, \
    borrowed(void*)         ctx) \
{ \
    require_msg(pred != NULL, "algo_any_slice_" #type ": pred cannot be NULL"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Returns true if all elements in slice_##type satisfy predicate \
 * \
 * Short-circuits on first failure. Returns true for empty slices \
 * (vacuous truth — no element violates the predicate). \
 * \
 * @param sv   Typed slice (borrowed, read-only) \
 * @param pred Predicate function — bool (*)(const void*, void*) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * @return true if all elements satisfy pred or sv.len == 0, false otherwise \
 */ \
static inline bool ALGO_ALL_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_pred_fn)  pred, \
    borrowed(void*)         ctx) \
{ \
    require_msg(pred != NULL, "algo_all_slice_" #type ": pred cannot be NULL"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (!pred(&sv.ptr[_i], ctx)) return false; \
    } \
    return true; /* vacuous truth when sv.len == 0 */ \
}

#endif /* CANON_ALGO_ANY_ALL_H */
