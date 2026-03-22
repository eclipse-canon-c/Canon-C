/**
 * @file filter.h
 * @brief Select elements from a sequence matching a predicate
 *
 * This is the entry point for header-only usage. Including this file
 * generates a statically-inlined implementation of algo_filter, plus
 * typed macro wrappers and the DEFINE_ALGO_FILTER instantiation macro.
 *
 * For separate compilation (external linkage), use filter_decl.h in
 * headers and filter_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Level 1 — Generic (void* interface, works on any type):
 * ```c
 * static bool is_positive(const void* elem, void* ctx) {
 *     (void)ctx;
 *     return *(const int*)elem > 0;
 * }
 *
 * int in[]  = {-1, 2, -3, 4, 0, 5};
 * int out[6];
 * usize n = algo_filter(in, 6, sizeof(int), is_positive, NULL, out, 6);
 * // n == 3, out == {2, 4, 5}
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof):
 * ```c
 * usize n = ALGO_FILTER_TYPED(out, 6, in, 6, int, is_positive, NULL);
 * // n == 3, out == {2, 4, 5}
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, fully optimizable):
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FILTER(int)
 *
 * slice_int sv = slice_int_from(in, 6);
 * usize n = algo_filter_slice_int(sv, out, 6, is_positive, NULL);
 * // n == 3, out == {2, 4, 5}
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * TRUNCATION BEHAVIOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * When the output buffer fills before all input is consumed, the function
 * stops immediately and returns out_cap. To detect truncation, compare
 * the return value to out_cap — if equal, more matches may exist beyond
 * what was written. To guarantee no truncation, provide out_cap >= len.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_filter(base, len, elem_size, pred, ctx, out, out_cap) → usize
 *
 * Typed macro:
 *   ALGO_FILTER_TYPED(out, out_cap, in, in_len, Type, pred, ctx) → usize
 *
 * Typed instantiation (call DEFINE_ALGO_FILTER(type) first):
 *   algo_filter_slice_##type(sv, out, out_cap, pred, ctx) → usize
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PREDICATE SIGNATURE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * All three levels use algo_pred_fn from core/primitives/compare.h:
 *   bool pred(const void* elem, void* ctx)
 *
 * Return true to keep the element, false to discard it.
 * elem is never NULL — it always points to a valid input element.
 * ctx is the optional caller context passed through unchanged; may be NULL.
 *
 * @sa filter_mangle.h  — name customization for slice variants
 * @sa filter_decl.h    — forward declaration for separate compilation
 * @sa filter_defn.h    — definition for separate compilation
 * @sa filter_impl.h    — pure logic (do not include directly)
 * @sa core/primitives/compare.h — algo_pred_fn definition
 */

#ifndef CANON_ALGO_FILTER_H
#define CANON_ALGO_FILTER_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — all functions are inlined at call sites.
 */
#define ALGO_FILTER_LINKAGE static inline

#include "filter_defn.h"

#undef ALGO_FILTER_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macro — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_FILTER_TYPED(out, out_cap, in, in_len, Type, pred, ctx)
 * @brief Type-safe filter from one C array into another
 *
 * Wraps algo_filter() with automatic sizeof(Type). The out_cap parameter
 * is explicit — never use sizeof(array)/sizeof(array[0]) here, as that
 * breaks when out is a pointer rather than an array.
 *
 * @param out     Output array of Type (borrowed, writable)
 * @param out_cap Maximum elements out can hold
 * @param in      Input array of Type (borrowed, read-only)
 * @param in_len  Number of input elements
 * @param Type    Element type — used for sizeof only
 * @param pred    Predicate: bool (*)(const void*, void*) (borrowed)
 * @param ctx     Optional context (borrowed, may be NULL)
 *
 * @return usize — number of elements written to out
 */
#define ALGO_FILTER_TYPED(out, out_cap, in, in_len, Type, pred, ctx) \
    algo_filter( \
        (in), (in_len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx), \
        (out), (out_cap))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_FILTER — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   Generates a fully typed function accepting slice_##type input directly.
   No void* anywhere — the compiler sees the actual element type.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a filter function operating directly on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated function:
 *   algo_filter_slice_##type(sv, out, out_cap, pred, ctx) → usize
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0. The loop
 * never executes when sv.len == 0, so a NULL ptr is safe. A non-NULL
 * pred and non-NULL out with valid out_cap are always required.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FILTER(int)
 *
 * static bool is_even(const void* elem, void* ctx) {
 *     (void)ctx;
 *     return (*(const int*)elem % 2) == 0;
 * }
 *
 * int in[]  = {1, 2, 3, 4, 5, 6};
 * int out[6];
 * slice_int sv = slice_int_from(in, 6);
 * usize n = algo_filter_slice_int(sv, out, 6, is_even, NULL);
 * // n == 3, out == {2, 4, 6}
 *
 * // Empty slice — no crash, returns 0
 * slice_int empty = slice_int_empty();
 * usize m = algo_filter_slice_int(empty, out, 6, is_even, NULL); // 0
 * ```
 */
#define DEFINE_ALGO_FILTER(type) \
\
/** \
 * @brief Filters slice_##type elements into a caller-provided output array \
 * \
 * Copies elements for which pred returns true into out, up to out_cap. \
 * Stops when out_cap elements are written. Relative order is preserved. \
 * \
 * @param sv      Input slice (borrowed, read-only) \
 * @param out     Output array of type (borrowed, writable) \
 * @param out_cap Maximum elements out can hold \
 * @param pred    Predicate function (algo_pred_fn) \
 * @param ctx     Optional context (borrowed, may be NULL) \
 * @return usize — number of elements written to out \
 */ \
static inline usize ALGO_FILTER_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(type*)         out, \
    usize                   out_cap, \
    borrowed(algo_pred_fn)  pred, \
    borrowed(void*)         ctx) \
{ \
    require_msg(pred != NULL, "algo_filter_slice_" #type ": pred cannot be NULL"); \
    require_msg(out  != NULL, "algo_filter_slice_" #type ": out cannot be NULL");  \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_filter_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len == 0 || out_cap == 0) return 0; \
    usize _written = 0; \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) { \
            out[_written++] = sv.ptr[_i]; \
            if (_written >= out_cap) break; \
        } \
    } \
    return _written; \
}

#endif /* CANON_ALGO_FILTER_H */
