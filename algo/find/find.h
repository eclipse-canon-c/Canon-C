/**
 * @file find.h
 * @brief Locate the first element in a sequence matching a predicate
 *
 * This is the entry point for header-only usage. Including this file
 * generates a statically-inlined implementation of algo_find, plus
 * typed macro wrappers and the DEFINE_ALGO_FIND instantiation macro.
 *
 * For separate compilation (external linkage), use find_decl.h in
 * headers and find_defn.h in exactly one .c file instead.
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
 * int arr[] = {5, 3, -2, 7};
 * usize idx;
 * const void* elem;
 * bool found = algo_find(arr, 4, sizeof(int), is_negative, NULL, &idx, &elem);
 * if (found) { ... }
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof, typed output pointer):
 * ```c
 * const int* p = NULL;
 * bool found = ALGO_FIND_TYPED(arr, 4, int, is_negative, NULL, &idx, &p);
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, returns option_##type):
 * ```c
 * DEFINE_SLICE(int)
 * CANON_OPTION(int)          // must be called before DEFINE_ALGO_FIND
 * DEFINE_ALGO_FIND(int)
 *
 * slice_int sv = slice_int_from(arr, 4);
 *
 * option_int opt = algo_find_slice_int(sv, is_negative, NULL);
 * if (option_int_is_some(opt)) { int val = option_int_unwrap(opt); }
 *
 * usize idx = algo_find_index_slice_int(sv, is_negative, NULL);
 * if (idx != CANON_USIZE_MAX) { ... }
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_find(base, len, elem_size, pred, ctx, out_index, out_elem) → bool
 *
 * Typed macro:
 *   ALGO_FIND_TYPED(base, len, Type, pred, ctx, out_index, out_elem_ptr) → bool
 *
 * Typed instantiation (call DEFINE_ALGO_FIND(type) first):
 *   algo_find_slice_##type(sv, pred, ctx) → option_##type
 *   algo_find_index_slice_##type(sv, pred, ctx) → usize (CANON_USIZE_MAX if not found)
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * NOT-FOUND SENTINEL
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_find_index_slice_##type returns CANON_USIZE_MAX (from limits.h)
 * when no element matches. This is the maximum value of usize — it is
 * never a valid index into any array that fits in memory.
 *
 * @sa find_mangle.h  — name customization for slice variants
 * @sa find_decl.h    — forward declaration for separate compilation
 * @sa find_defn.h    — definition for separate compilation
 * @sa find_impl.h    — pure logic (do not include directly)
 * @sa core/primitives/compare.h  — algo_pred_fn definition
 * @sa core/primitives/limits.h   — CANON_USIZE_MAX
 * @sa semantics/option/option.h  — option_##type returned by slice variant
 */

#ifndef CANON_ALGO_FIND_H
#define CANON_ALGO_FIND_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/option/option.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — all functions are inlined at call sites.
 */
#define ALGO_FIND_LINKAGE static inline

#include "find_defn.h"

#undef ALGO_FIND_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macro — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_FIND_TYPED(base, len, Type, pred, ctx, out_index, out_elem_ptr)
 * @brief Type-safe find over a C array with optional typed output pointer
 *
 * Wraps algo_find() with automatic sizeof(Type) and avoids the
 * void** cast on the output element pointer at the call site.
 *
 * @param base         Input array of Type (borrowed, read-only)
 * @param len          Number of elements
 * @param Type         Element type — used for sizeof only
 * @param pred         Predicate: bool (*)(const void*, void*) (borrowed)
 * @param ctx          Optional context (borrowed, may be NULL)
 * @param out_index    Optional usize* — receives index of match (may be NULL)
 * @param out_elem_ptr Optional const Type** — receives pointer to match (may be NULL)
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
   - DEFINE_SLICE(type)   from core/slice.h
   - CANON_OPTION(type)   from semantics/option/option.h
   Both must be called BEFORE DEFINE_ALGO_FIND(type).
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates find functions operating directly on slice_##type
 *
 * Prerequisites (must be called before this macro):
 * - DEFINE_SLICE(type)  — provides slice_##type
 * - CANON_OPTION(type)  — provides option_##type, option_##type##_some(),
 *                         option_##type##_none()
 *
 * Generated functions:
 *
 *   algo_find_slice_##type(sv, pred, ctx) → option_##type
 *     Returns Some(value) on first match, None if empty or no match.
 *
 *   algo_find_index_slice_##type(sv, pred, ctx) → usize
 *     Returns the index of the first match.
 *     Returns CANON_USIZE_MAX if not found — never a valid array index.
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h). The loop never executes when sv.len == 0. Only pred is
 * required to be non-NULL.
 *
 * @param type Element type — must match prior DEFINE_SLICE and CANON_OPTION calls
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * CANON_OPTION(int)
 * DEFINE_ALGO_FIND(int)
 *
 * static bool is_negative(const void* e, void* ctx) {
 *     (void)ctx; return *(const int*)e < 0;
 * }
 *
 * int buf[] = {5, 3, -2, 7};
 * slice_int sv = slice_int_from(buf, 4);
 *
 * option_int opt = algo_find_slice_int(sv, is_negative, NULL);
 * if (option_int_is_some(opt)) {
 *     int val = option_int_unwrap(opt); // val == -2
 * }
 *
 * usize idx = algo_find_index_slice_int(sv, is_negative, NULL); // idx == 2
 * ```
 */
#define DEFINE_ALGO_FIND(type) \
\
/** \
 * @brief Returns the first element in slice_##type satisfying pred \
 * \
 * @param sv   Input slice (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * @return option_##type — Some(value) on match, None if not found or empty \
 */ \
static inline option_##type ALGO_FIND_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_pred_fn)  pred, \
    borrowed(void*)         ctx) \
{ \
    require_msg(pred != NULL, \
        "algo_find_slice_" #type ": pred cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_find_slice_" #type ": non-empty slice has NULL ptr"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) { \
            return option_##type##_some(sv.ptr[_i]); \
        } \
    } \
    return option_##type##_none(); \
} \
\
/** \
 * @brief Returns the index of the first element in slice_##type satisfying pred \
 * \
 * Returns CANON_USIZE_MAX if no element matches — never a valid index. \
 * \
 * @param sv   Input slice (borrowed, read-only) \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx  Optional context (borrowed, may be NULL) \
 * @return usize — index of first match, or CANON_USIZE_MAX if not found \
 */ \
static inline usize ALGO_FIND_INDEX_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_pred_fn)  pred, \
    borrowed(void*)         ctx) \
{ \
    require_msg(pred != NULL, \
        "algo_find_index_slice_" #type ": pred cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_find_index_slice_" #type ": non-empty slice has NULL ptr"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) return _i; \
    } \
    return CANON_USIZE_MAX; \
}

#endif /* CANON_ALGO_FIND_H */
