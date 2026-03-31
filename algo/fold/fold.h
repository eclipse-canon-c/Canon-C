/**
 * @file fold.h
 * @brief Functional reduction of sequences to single values (fold/reduce)
 *
 * This is the entry point for header-only usage. Provides both infallible
 * and fallible fold operations at three levels of type safety.
 *
 * For separate compilation, include fold_decl.h in headers and
 * fold_defn.h in exactly one .c file.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * ARCHITECTURAL NOTE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * fold is architecturally different from the other eight algo/ modules.
 * In modules like filter, find, sort, etc., Level 1 is a generic function
 * (algo_filter, algo_find, algo_sort) defined in *_impl.h with void*
 * parameters and a linkage macro. Level 2 typed macros wrap Level 1.
 *
 * fold cannot follow this pattern because the accumulator type and element
 * type are both caller-determined and may differ from each other. A generic
 * void*-based algo_fold() function would require unsafe function pointer
 * casts (casting typed fold functions to a void*-based signature), which
 * is undefined behavior in C99.
 *
 * Therefore:
 *   - Level 1 and Level 2 are unified as ALGO_FOLD / ALGO_FOLD_RESULT
 *     macros that expand to direct for-loops with typed array indexing.
 *   - Level 3 (DEFINE_ALGO_FOLD) generates typed slice functions with
 *     void* acc_ptr (to allow arbitrary accumulator types) and typed
 *     element pointers.
 *   - fold_impl.h contains only the CANON_RESULT(bool, Error)
 *     instantiation needed for fallible fold — not the core algorithm.
 *
 * This deviation is intentional and documented here for readers who have
 * seen the consistent 3-level pattern in the other eight modules.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Infallible fold — accumulates without possibility of error:
 * ```c
 * void sum_ints(int* acc, const int* elem, void* ctx) {
 *     *acc += *elem;
 * }
 * int numbers[] = {1, 2, 3, 4, 5};
 * int total = 0;
 * ALGO_FOLD(&total, numbers, 5, int, sum_ints, NULL);
 * // total == 15
 * ```
 *
 * Fallible fold — stops on first error (GNU version, preferred):
 * ```c
 * result_bool_Error sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) return result_bool_Error_err(ERR_INVALID_ARG);
 *     *acc += *elem;
 *     return result_bool_Error_ok(true);
 * }
 * int vals[] = {1, 2, -3, 4};
 * int sum = 0;
 * result_bool_Error r = ALGO_FOLD_RESULT(&sum, vals, 4, int, sum_positive, NULL);
 * if (result_bool_Error_is_err(r)) { ... } // stopped at index 2
 * ```
 *
 * Slice variant — typed, no void*:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FOLD(int)
 *
 * slice_int sv = slice_int_from(numbers, 5);
 * int total = 0;
 * algo_fold_slice_int(&total, sv, sum_ints, NULL);
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * FOLD FUNCTION SIGNATURES
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Infallible: void fn(AccType* acc, const ElemType* elem, void* ctx)
 * Fallible:   result_bool_Error fn(AccType* acc, const ElemType* elem, void* ctx)
 *
 * The accumulator type and element type may differ — fold is not restricted
 * to same-type accumulation.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic macros:
 *   ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx) → void
 *   ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx) → result_bool_Error
 *     [GNU mode — returns value directly via statement expression]
 *   ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx, out_result) → void
 *     [C99 mode — writes result into pre-declared out_result variable]
 *
 * Vec convenience wrappers:
 *   ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx) → void
 *   ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx) → result_bool_Error [GNU]
 *   ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx, out_r) → void [C99]
 *
 * Typed slice instantiation (call DEFINE_ALGO_FOLD(type) first):
 *   algo_fold_slice_##type(acc_ptr, sv, fn, ctx) → void
 *   algo_fold_result_slice_##type(acc_ptr, sv, fn, ctx) → result_bool_Error
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PERFORMANCE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Infallible: O(n) always. Fallible: O(1) best (first Err), O(n) worst.
 * All variants: O(1) space, no allocation.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * @sa fold_mangle.h — name customization for slice variants
 * @sa fold_decl.h   — type declarations for separate compilation
 * @sa fold_defn.h   — definitions for separate compilation
 * @sa fold_impl.h   — support logic (do not include directly)
 * @sa semantics/result/result.h — result_bool_Error
 * @sa semantics/error.h         — ERR_INVALID_ARG and other error codes
 */

#ifndef CANON_ALGO_FOLD_H
#define CANON_ALGO_FOLD_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/error.h"
#include "semantics/result/result.h"

#define ALGO_FOLD_LINKAGE static inline
#include "fold_impl.h"   /* implementation support — NOT fold_defn.h */
#undef ALGO_FOLD_LINKAGE

/*
 * C99 bool/bool mangling fix:
 * In C99, `bool` expands to `_Bool` before token-pasting inside CANON_RESULT,
 * so CANON_RESULT(bool, Error) generates `result__Bool_Error` (double underscore).
 * The stable name `result_bool_Error` used throughout this header and in user
 * code is provided by the typedef and function-name aliases below.
 */
#ifndef CANON_RESULT_BOOL_ERROR_COMPAT
#define CANON_RESULT_BOOL_ERROR_COMPAT
typedef result__Bool_Error result_bool_Error;
#define result_bool_Error_ok         result__Bool_Error_ok
#define result_bool_Error_err        result__Bool_Error_err
#define result_bool_Error_is_ok      result__Bool_Error_is_ok
#define result_bool_Error_is_err     result__Bool_Error_is_err
#define result_bool_Error_get_ok     result__Bool_Error_get_ok
#define result_bool_Error_get_err    result__Bool_Error_get_err
#define result_bool_Error_unwrap     result__Bool_Error_unwrap
#define result_bool_Error_unwrap_err result__Bool_Error_unwrap_err
#define result_bool_Error_unwrap_or  result__Bool_Error_unwrap_or
#define result_bool_Error_expect     result__Bool_Error_expect
#define result_bool_Error_map        result__Bool_Error_map
#define result_bool_Error_map_err    result__Bool_Error_map_err
#define result_bool_Error_and_then   result__Bool_Error_and_then
#define result_bool_Error_or_else    result__Bool_Error_or_else
#define result_bool_Error_and        result__Bool_Error_and
#define result_bool_Error_or         result__Bool_Error_or
#define result_bool_Error_eq         result__Bool_Error_eq
#endif /* CANON_RESULT_BOOL_ERROR_COMPAT */

/* ════════════════════════════════════════════════════════════════════════════
   ALGO_FOLD — infallible fold over a C array
   ════════════════════════════════════════════════════════════════════════════ */

#define ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx) \
    do { \
        require_msg((acc_ptr) != NULL, "ALGO_FOLD: acc_ptr cannot be NULL"); \
        require_msg((array)   != NULL, "ALGO_FOLD: array cannot be NULL");   \
        require_msg((fold_fn) != NULL, "ALGO_FOLD: fold_fn cannot be NULL"); \
        const usize _fold_len = (usize)(len); \
        for (usize _fold_i = 0; _fold_i < _fold_len; ++_fold_i) { \
            (fold_fn)((acc_ptr), &(array)[_fold_i], (ctx)); \
        } \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════════
   ALGO_FOLD_RESULT — fallible fold, GNU and C99 variants
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_NO_GNU_EXTENSIONS

#define ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx) \
    ({ \
        require_msg((acc_ptr) != NULL, "ALGO_FOLD_RESULT: acc_ptr cannot be NULL"); \
        require_msg((array)   != NULL, "ALGO_FOLD_RESULT: array cannot be NULL");   \
        require_msg((fold_fn) != NULL, "ALGO_FOLD_RESULT: fold_fn cannot be NULL"); \
        result_bool_Error _fr = result_bool_Error_ok(true); \
        const usize _fr_len = (usize)(len); \
        for (usize _fr_i = 0; _fr_i < _fr_len; ++_fr_i) { \
            _fr = (fold_fn)((acc_ptr), &(array)[_fr_i], (ctx)); \
            if (result_bool_Error_is_err(_fr)) break; \
        } \
        _fr; \
    })

#else /* CANON_NO_GNU_EXTENSIONS — strict C99 */

#define ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx, out_result) \
    do { \
        require_msg((acc_ptr) != NULL, "ALGO_FOLD_RESULT: acc_ptr cannot be NULL"); \
        require_msg((array)   != NULL, "ALGO_FOLD_RESULT: array cannot be NULL");   \
        require_msg((fold_fn) != NULL, "ALGO_FOLD_RESULT: fold_fn cannot be NULL"); \
        (out_result) = result_bool_Error_ok(true); \
        const usize _fr99_len = (usize)(len); \
        for (usize _fr99_i = 0; _fr99_i < _fr99_len; ++_fr99_i) { \
            (out_result) = (fold_fn)((acc_ptr), &(array)[_fr99_i], (ctx)); \
            if (result_bool_Error_is_err(out_result)) break; \
        } \
    } while (0)

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   Vec convenience wrappers
   ════════════════════════════════════════════════════════════════════════════ */

#define ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
    ALGO_FOLD((acc_ptr), (vec).items, (vec).len, Type, (fold_fn), (ctx))

#ifndef CANON_NO_GNU_EXTENSIONS
    #define ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
        ALGO_FOLD_RESULT((acc_ptr), (vec).items, (vec).len, Type, (fold_fn), (ctx))
#else
    #define ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx, out_result) \
        ALGO_FOLD_RESULT((acc_ptr), (vec).items, (vec).len, Type, (fold_fn), (ctx), (out_result))
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_FOLD — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) from core/slice.h.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates fold functions operating directly on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 *   algo_fold_slice_##type(acc_ptr, sv, fn, ctx) → void
 *   algo_fold_result_slice_##type(acc_ptr, sv, fn, ctx) → result_bool_Error
 *
 * Note on acc_ptr type: the accumulator type may differ from the element
 * type (e.g. folding int elements into a double sum). The fn parameter
 * takes void* acc to allow arbitrary accumulator types. Cast inside
 * fn to the actual accumulator type.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 */
#define DEFINE_ALGO_FOLD(type) \
\
static inline void ALGO_FOLD_SLICE_FN(type)( \
    borrowed(void*)                             acc_ptr, \
    borrowed(slice_##type)                      sv, \
    borrowed(void (*)(void*, const type*, void*)) fn, \
    borrowed(void*)                             ctx) \
{ \
    require_msg(acc_ptr != NULL, \
        "algo_fold_slice_" #type ": acc_ptr cannot be NULL"); \
    require_msg(fn != NULL, \
        "algo_fold_slice_" #type ": fn cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_fold_slice_" #type ": non-empty slice has NULL ptr"); \
    for (usize _i = 0; _i < sv.len; ++_i) { \
        fn(acc_ptr, &sv.ptr[_i], ctx); \
    } \
} \
\
static inline result_bool_Error ALGO_FOLD_RESULT_SLICE_FN(type)( \
    borrowed(void*)                                              acc_ptr, \
    borrowed(slice_##type)                                       sv, \
    borrowed(result_bool_Error (*)(void*, const type*, void*))   fn, \
    borrowed(void*)                                              ctx) \
{ \
    require_msg(acc_ptr != NULL, \
        "algo_fold_result_slice_" #type ": acc_ptr cannot be NULL"); \
    require_msg(fn != NULL, \
        "algo_fold_result_slice_" #type ": fn cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_fold_result_slice_" #type ": non-empty slice has NULL ptr"); \
    for (usize _i = 0; _i < sv.len; ++_i) { \
        result_bool_Error _r = fn(acc_ptr, &sv.ptr[_i], ctx); \
        if (result_bool_Error_is_err(_r)) return _r; \
    } \
    return result_bool_Error_ok(true); \
}

#endif /* CANON_ALGO_FOLD_H */
