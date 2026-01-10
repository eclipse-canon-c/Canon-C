// algo/fold.h
#ifndef CANON_ALGO_FOLD_H
#define CANON_ALGO_FOLD_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros
#include "semantics/result.h"

/**
 * @file fold.h
 * @brief Functional-style reduction of sequences to a single value (fold/left-fold)
 *
 * Reduces a sequence of elements into a single accumulator value by repeatedly
 * applying a folding function.
 *
 * Key properties:
 *   - Read-only input — never modifies source array
 *   - Accumulator is caller-owned, caller-initialized, caller-mutable
 *   - Zero allocation, zero mutation of input, zero ownership transfer
 *   - Supports optional user context (void* ctx)
 *   - Two variants:
 *     - **Infallible** — always completes, void callback
 *     - **Fallible** — early termination on error, returns Result
 *   - Generic (void*) and strongly-typed versions
 *   - Seamless integration with vec.h containers
 *
 * Typical use cases:
 *   - Sum/product/min/max of numbers
 *   - String concatenation
 *   - Collecting results into a struct
 *   - Fallible processing (e.g. parsing with early abort on error)
 *
 * Fold function signatures:
 *   - Infallible: void fold(void* acc, const void* item, void* ctx)
 *   - Fallible: result_bool_constcharp fold(void* acc, const void* item, void* ctx)
 *              (Ok(true) = continue, Err(reason) = stop and return error)
 *
 * Usage example (infallible):
 *   void sum_acc(void* acc, const void* item, void* ctx) {
 *       *(int*)acc += *(int*)item;
 *   }
 *   int total = 0;
 *   int arr[] = {1, 2, 3, 4};
 *   algo_fold(&total, (const void**)arr, 4, sum_acc, NULL);
 *   // total == 10
 *
 * Usage example (fallible + vec):
 *   result_bool_constcharp add_checked(void* acc, const void* item, void* ctx) {
 *       int* sum = acc;
 *       int val = *(int*)item;
 *       if (val < 0) return result_bool_constcharp_err("negative value");
 *       *sum += val;
 *       return result_bool_constcharp_ok(true);
 *   }
 *   vec_int v = ...;
 *   int sum = 0;
 *   result_bool_constcharp r = ALGO_FOLD_RESULT_VEC(&sum, v, int, add_checked, NULL);
 *   if (result_is_err(r)) {
 *       // handle error
 *   }
 */

/**
 * @brief Infallible fold function type (generic version)
 *
 * @param acc   Mutable accumulator (caller-owned)
 * @param item  Current element (read-only)
 * @param ctx   Optional user context
 */
typedef void (*algo_fold_fn)(void* acc, const void* item, void* ctx);

/**
 * @brief Fallible fold function type (generic version)
 *
 * @param acc   Mutable accumulator
 * @param item  Current element
 * @param ctx   Optional context
 * @return      Ok(true) = continue, Err(reason) = stop and propagate error
 */
typedef result_bool_constcharp (*algo_fold_result_fn)(
    void* acc,
    const void* item,
    void* ctx
);

/* ────────────────────────────────────────────────────────────────────────────
   Infallible fold (always completes, void callback)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reduces sequence into caller-provided accumulator (infallible)
 *
 * Applies fold function left-to-right.
 * Accumulator mutated in-place.
 *
 * @param acc    Caller-owned mutable accumulator
 * @param items  Array of void* pointers
 * @param len    Number of elements
 * @param f      Fold function
 * @param ctx    Optional user context
 *
 * Does nothing on invalid input (NULL acc/items/f).
 * No early termination — always processes entire sequence.
 */
static inline void algo_fold(
    void* acc,
    const void** items,
    size_t len,
    algo_fold_fn f,
    void* ctx
) {
    if (!acc || !items || !f) return;

    for (size_t i = 0; i < len; ++i) {
        f(acc, items[i], ctx);
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Fallible fold (early termination on error)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Fallible reduction with early exit on error
 *
 * Applies fold function left-to-right.
 * Stops and returns first error encountered.
 *
 * @param acc    Caller-owned mutable accumulator
 * @param items  Array of void* pointers
 * @param len    Number of elements
 * @param f      Fallible fold function
 * @param ctx    Optional context
 * @return       Ok(true) on full success,
 *               Err(reason) on first failure (stops early)
 *
 * Propagates error from fold function.
 * Invalid input returns immediate error.
 */
static inline result_bool_constcharp algo_fold_result(
    void* acc,
    const void** items,
    size_t len,
    algo_fold_result_fn f,
    void* ctx
) {
    if (!acc || !items || !f) {
        return result_bool_constcharp_err("invalid fold arguments");
    }

    for (size_t i = 0; i < len; ++i) {
        result_bool_constcharp r = f(acc, items[i], ctx);
        if (result_bool_constcharp_is_err(r)) {
            return r;  // early exit with error
        }
    }

    return result_bool_constcharp_ok(true);
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed macros (recommended — safer, cleaner syntax)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Infallible typed fold (mutates acc_ptr in-place)
 *
 * @param acc_ptr  Pointer to accumulator variable
 * @param array    Input array of Type
 * @param len      Number of elements
 * @param Type     Element type
 * @param fold_fn  Fold function: void (*)(AccType*, const Type*, void*)
 * @param ctx      Optional context
 */
#define ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx) \
    do { \
        if ((acc_ptr) && (array) && (fold_fn)) { \
            for (size_t _i = 0; _i < (len); ++_i) { \
                (fold_fn)((acc_ptr), &(array)[_i], (ctx)); \
            } \
        } \
    } while (0)

/**
 * @brief Fallible typed fold (returns Result on first error)
 *
 * @param acc_ptr  Pointer to accumulator variable
 * @param array    Input array of Type
 * @param len      Number of elements
 * @param Type     Element type
 * @param fold_fn  Fold function: result_bool_constcharp (*)(AccType*, const Type*, void*)
 * @param ctx      Optional context
 * @return         Ok(true) on success, Err(reason) on first failure
 */
#define ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx) \
    ({ \
        result_bool_constcharp _r = result_bool_constcharp_ok(true); \
        if ((acc_ptr) && (array) && (fold_fn)) { \
            for (size_t _i = 0; _i < (len); ++_i) { \
                _r = (fold_fn)((acc_ptr), &(array)[_i], (ctx)); \
                if (result_bool_constcharp_is_err(_r)) break; \
            } \
        } else { \
            _r = result_bool_constcharp_err("invalid fold arguments"); \
        } \
        _r; \
    })

/* ────────────────────────────────────────────────────────────────────────────
   Vec integration (most convenient for Canon-C containers)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Infallible fold over vec elements
 *
 * Convenience wrapper over ALGO_FOLD
 */
#define ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
    ALGO_FOLD((acc_ptr), (vec).items, (vec).len, Type, fold_fn, ctx)

/**
 * @brief Fallible fold over vec elements
 *
 * Convenience wrapper over ALGO_FOLD_RESULT
 */
#define ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
    ALGO_FOLD_RESULT((acc_ptr), (vec).items, (vec).len, Type, fold_fn, ctx)

#endif /* CANON_ALGO_FOLD_H */
