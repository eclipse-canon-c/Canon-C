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
 * Fallible fold — C99 strict mode (CANON_NO_GNU_EXTENSIONS):
 * ```c
 * result_bool_Error r;
 * ALGO_FOLD_RESULT(&sum, vals, 4, int, sum_positive, NULL, r);
 * // Note the extra out-result parameter — required in C99 mode
 * if (result_bool_Error_is_err(r)) { ... }
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
 * Vec convenience wrappers (for containers with .items and .len fields):
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
 * Time complexity — all variants:
 *
 *   ALGO_FOLD / algo_fold_slice_##type (infallible):
 *     Always O(n) — every element is visited exactly once, n = len.
 *     No early exit — the accumulator receives every element.
 *     Best == Worst == O(n).
 *
 *   ALGO_FOLD_RESULT / algo_fold_result_slice_##type (fallible):
 *     Best case:  O(1) — first element causes fn to return Err
 *     Worst case: O(n) — all elements processed, all return Ok
 *     Average:    O(k) where k = index of first Err + 1
 *
 * Space complexity — all variants:
 *   O(1) — no heap allocation, no recursion, constant stack frame.
 *   The accumulator is caller-owned; fold writes through acc_ptr only.
 *
 * fn call count:
 *   Infallible: exactly n calls, always.
 *   Fallible: 1 (first element fails) to n (all succeed).
 *   fn is never called with a NULL acc_ptr or NULL elem pointer.
 *
 * Level comparison:
 *   ALGO_FOLD macro: expands to a direct for-loop with typed array
 *   indexing — zero overhead, no function pointer indirection at the
 *   outer loop level.
 *
 *   algo_fold_slice_##type: takes a void* acc_ptr to allow arbitrary
 *   accumulator types with a typed element pointer. The single indirect
 *   call per element (through fn) is the only overhead vs. inlined code.
 *   For hot paths where fn is known at compile time, the compiler can
 *   inline through the function pointer when LTO or static inline is used.
 *
 *   The fold operation itself does not copy elements — fn receives a
 *   const pointer to each element in place, so struct-sized elements
 *   incur no copy cost in the fold loop.
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
/* fold_impl.h already instantiates CANON_RESULT(bool, Error) */
#undef ALGO_FOLD_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   ALGO_FOLD — infallible fold over a C array
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx)
 * @brief Type-safe infallible fold over a C array
 *
 * Applies fold_fn to each element left-to-right, accumulating into
 * *acc_ptr. The caller initializes *acc_ptr before the call.
 *
 * @param acc_ptr Pointer to accumulator (borrowed, mutable)
 * @param array   Input array of Type (borrowed, read-only)
 * @param len     Number of elements
 * @param Type    Element type
 * @param fold_fn void (*)(AccType*, const Type*, void*)
 * @param ctx     Optional context (borrowed, may be NULL)
 *
 * Performance:
 * - Time:  O(n) — visits every element exactly once, no early exit
 * - Space: O(1) — no allocation; accumulator is caller-owned
 * - fn calls: exactly n (0 when len == 0)
 *
 * @pre acc_ptr != NULL  — triggers require_msg
 * @pre array   != NULL  — triggers require_msg
 * @pre fold_fn != NULL  — triggers require_msg
 */
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

/**
 * @def ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx)
 * @brief Fallible fold — returns result_bool_Error directly (GNU version)
 *
 * Requires GNU C statement expressions or C23.
 * Define CANON_NO_GNU_EXTENSIONS to use the C99 do-while variant instead.
 *
 * Stops on the first fold_fn that returns Err. The accumulator contains
 * the partial result up to the error point.
 *
 * @param acc_ptr Pointer to accumulator (borrowed, mutable)
 * @param array   Input array of Type (borrowed, read-only)
 * @param len     Number of elements
 * @param Type    Element type
 * @param fold_fn result_bool_Error (*)(AccType*, const Type*, void*)
 * @param ctx     Optional context (borrowed, may be NULL)
 *
 * @return result_bool_Error — Ok(true) if all elements processed, Err on failure
 *
 * Performance:
 * - Time best:  O(1) — first element causes fn to return Err
 * - Time worst: O(n) — all elements processed successfully
 * - Time avg:   O(k) where k = index of first Err + 1
 * - Space:      O(1) — no allocation
 * - fn calls:   1 (first fails) to n (all succeed)
 *
 * @pre acc_ptr != NULL  — triggers require_msg
 * @pre array   != NULL  — triggers require_msg
 * @pre fold_fn != NULL  — triggers require_msg
 */
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

/**
 * @def ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx, out_result)
 * @brief Fallible fold — writes result into pre-declared variable (C99 version)
 *
 * This is the strict C99 variant of ALGO_FOLD_RESULT. It takes one extra
 * parameter: a pre-declared result_bool_Error variable that receives the
 * outcome. This avoids GNU statement expressions and unsafe function pointer
 * casts.
 *
 * Usage:
 * ```c
 * result_bool_Error r;
 * ALGO_FOLD_RESULT(&sum, arr, len, int, my_fn, NULL, r);
 * if (result_bool_Error_is_err(r)) { ... }
 * ```
 *
 * @param acc_ptr    Pointer to accumulator (borrowed, mutable)
 * @param array      Input array of Type (borrowed, read-only)
 * @param len        Number of elements
 * @param Type       Element type
 * @param fold_fn    result_bool_Error (*)(AccType*, const Type*, void*)
 * @param ctx        Optional context (borrowed, may be NULL)
 * @param out_result Pre-declared result_bool_Error variable — receives outcome
 *
 * Performance:
 * - Time best:  O(1) — first element causes fn to return Err
 * - Time worst: O(n) — all elements processed successfully
 * - Time avg:   O(k) where k = index of first Err + 1
 * - Space:      O(1) — no allocation
 * - fn calls:   1 (first fails) to n (all succeed)
 *
 * @pre acc_ptr    != NULL — triggers require_msg
 * @pre array      != NULL — triggers require_msg
 * @pre fold_fn    != NULL — triggers require_msg
 */
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
   ════════════════════════════════════════════════════════════════════════════
   Work with any container that exposes .items (element pointer) and
   .len (element count) fields. Includes data/vec.h typed vectors.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx)
 * @brief Infallible fold over a container with .items and .len fields
 */
#define ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
    ALGO_FOLD((acc_ptr), (vec).items, (vec).len, Type, (fold_fn), (ctx))

/**
 * @def ALGO_FOLD_RESULT_VEC — fallible fold over a container with .items and .len
 *
 * GNU mode:  ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx)
 *            → returns result_bool_Error directly
 * C99 mode:  ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx, out_result)
 *            → writes into pre-declared out_result variable
 */
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
   Generates fully typed functions with no void* element pointer —
   the compiler sees the actual element type at each call site.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates fold functions operating directly on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 *
 *   algo_fold_slice_##type(acc_ptr, sv, fn, ctx) → void
 *     Infallible fold. fn: void (*)(void*, const type*, void*)
 *
 *   algo_fold_result_slice_##type(acc_ptr, sv, fn, ctx) → result_bool_Error
 *     Fallible fold. fn: result_bool_Error (*)(void*, const type*, void*)
 *     Stops on first Err return from fn.
 *
 * Note on acc_ptr type: the accumulator type may differ from the element
 * type (e.g. folding int elements into a double sum). The fn parameter
 * is typed as void* acc to allow arbitrary accumulator types. Cast inside
 * fn to the actual accumulator type.
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0. The loop never
 * executes when sv.len == 0. Only acc_ptr and fn are required non-NULL.
 *
 * Performance:
 *   algo_fold_slice_##type:
 *     Time:  O(n) — visits all n elements, no early exit
 *     Space: O(1) — no allocation; fn called with pointer into slice
 *     fn calls: exactly n (0 when sv.len == 0)
 *
 *   algo_fold_result_slice_##type:
 *     Time best:  O(1) — first element causes fn to return Err
 *     Time worst: O(n) — all elements return Ok
 *     Space: O(1) — no allocation
 *     fn calls: 1 to n
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FOLD(int)
 *
 * void sum_ints(void* acc, const int* elem, void* ctx) {
 *     *(int*)acc += *elem;
 * }
 *
 * int arr[] = {1, 2, 3, 4, 5};
 * slice_int sv = slice_int_from(arr, 5);
 * int total = 0;
 * algo_fold_slice_int(&total, sv, sum_ints, NULL);
 * // total == 15
 *
 * // Fallible variant
 * result_bool_Error sum_positive(void* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) return result_bool_Error_err(ERR_INVALID_ARG);
 *     *(int*)acc += *elem;
 *     return result_bool_Error_ok(true);
 * }
 * int sum = 0;
 * result_bool_Error r = algo_fold_result_slice_int(&sum, sv, sum_positive, NULL);
 * ```
 */
#define DEFINE_ALGO_FOLD(type) \
\
static inline void ALGO_FOLD_SLICE_FN(type)( \
    borrowed(void*)        acc_ptr, \
    borrowed(slice_##type) sv, \
    void (*fn)(void*, const type*, void*), \
    borrowed(void*)        ctx) \
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
    borrowed(void*)        acc_ptr, \
    borrowed(slice_##type) sv, \
    result_bool_Error (*fn)(void*, const type*, void*), \
    borrowed(void*)        ctx) \
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
