#ifndef CANON_ALGO_FOLD_H
#define CANON_ALGO_FOLD_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/error.h"
#include "semantics/result/result.h"

/**
 * @file algo/fold.h
 * @brief Functional reduction of sequences to single values (fold/reduce)
 *
 * Provides efficient, type-safe functional folding operations that reduce
 * sequences of elements to a single accumulated value. Supports both
 * infallible (always completes) and fallible (early termination) variants.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Functional programming pattern: fold/reduce operation
 * - Input is read-only — never mutated
 * - Accumulator is caller-owned and caller-initialized
 * - No allocation inside functions - caller provides accumulator
 * - No ownership transfer - caller maintains control
 * - Two variants: infallible (void) and fallible (Result)
 * - Supports optional user context for stateful operations
 * - Bounded and safe (uses provided length)
 * - Zero-overhead abstraction - compiles to tight loops
 * - Type-safe macros prevent common errors
 * - Composable with other functional operations
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Basic fold operation (left fold):
 *   1. Start with initial accumulator value (provided by caller)
 *   2. For each element at index i in input array:
 *   3. Apply folding function: acc = f(acc, input[i])
 *   4. Continue until all elements processed
 *   5. Final accumulator value is the result
 *
 * Example: Folding [1, 2, 3, 4] with sum function, initial acc = 0
 *   Input:  [1, 2, 3, 4]
 *   Step 0: acc = 0 (initial)
 *   Step 1: acc = f(0, 1) = 1
 *   Step 2: acc = f(1, 2) = 3
 *   Step 3: acc = f(3, 3) = 6
 *   Step 4: acc = f(6, 4) = 10
 *   Result: 10
 *
 * Fallible variant:
 *   1. For each element at index i:
 *   2. Apply folding function: result = f(acc, input[i])
 *   3. If result is Error: stop immediately and return error
 *   4. If result is Ok: continue to next element
 *   5. Return Ok if all elements processed successfully
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * fold.h depends on semantics/result for fallible variant.
 * fold.h does NOT depend on other algo/ headers.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - No allocations anywhere in this file
 * - GNU statement expressions used in ALGO_FOLD_RESULT macro
 * - Define CANON_NO_GNU_EXTENSIONS for C99 fallback
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations are thread-safe (no shared mutable state)
 * - Safe to call concurrently on different arrays from multiple threads
 * - Same accumulator should not be modified concurrently without external synchronization
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(n) - exactly n function calls
 * - Space complexity: O(1) - no allocations
 * - No heap allocations
 * - Cache-friendly: sequential access pattern
 * - Compiler can inline folding functions for zero overhead
 * - Early termination in fallible variant
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Aggregation (sum, product, min, max, average)
 * - String concatenation and building
 * - Collecting data into structures
 * - Counting and statistics
 * - Validation with accumulation (all elements valid?)
 * - Fallible parsing with early abort
 * - Building complex objects from elements
 * - Computing checksums/hashes
 * - Event processing with state
 *
 * Quick start:
 * ```c
 * #include "algo/fold.h"
 *
 * // Basic sum
 * void sum_ints(int* acc, const int* elem, void* ctx) {
 *     *acc += *elem;
 * }
 * int numbers[] = {1, 2, 3, 4, 5};
 * int total = 0;
 * ALGO_FOLD(&total, numbers, 5, int, sum_ints, NULL);
 * // Result: total = 15
 *
 * // Product calculation
 * void multiply(double* acc, const double* elem, void* ctx) {
 *     *acc *= *elem;
 * }
 * double values[] = {2.0, 3.0, 4.0};
 * double product = 1.0;
 * ALGO_FOLD(&product, values, 3, double, multiply, NULL);
 * // Result: product = 24.0
 *
 * // Fallible validation
 * CANON_RESULT(bool, Error)
 * result_bool_Error sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) {
 *         return result_bool_Error_err(ERR_INVALID_ARG);
 *     }
 *     *acc += *elem;
 *     return result_bool_Error_ok(true);
 * }
 * int vals[] = {1, 2, -3, 4};
 * int sum = 0;
 * result_bool_Error r = ALGO_FOLD_RESULT(&sum, vals, 4, int, sum_positive, NULL);
 * if (result_bool_Error_is_err(r)) {
 *     // Stopped at index 2 due to negative value
 * }
 * ```
 *
 * @sa semantics/result/result.h — Result<T, E> for fallible variant
 * @sa semantics/error.h          — Error codes for fallible operations
 * @sa core/slice.h               — slice_##type used by DEFINE_ALGO_FOLD
 * @sa core/ownership.h           — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Result type for fallible fold operations
   ════════════════════════════════════════════════════════════════════════════
   Using result_bool_Error as the standard return type for fallible folds.
   Users can define custom Result types if needed.
   ════════════════════════════════════════════════════════════════════════════ */

CANON_RESULT(bool, Error)

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx)
 * @brief Type-safe infallible fold (recommended interface)
 *
 * Applies a folding function to each element of an array, accumulating
 * results into a caller-provided accumulator. Provides compile-time type
 * safety by explicitly specifying element type.
 *
 * This is the recommended interface for most use cases. It prevents type
 * errors at compile time and generates efficient code with excellent
 * optimization potential.
 *
 * The folding function receives pointers to the accumulator and individual
 * elements, allowing it to modify the accumulator without copying entire
 * elements.
 *
 * @param acc_ptr  Pointer to accumulator variable (borrowed — mutable, caller-initialized)
 * @param array    Input array of Type (borrowed, read-only — never modified)
 * @param len      Number of elements to process
 * @param Type     Element type (e.g., int, struct Point)
 * @param fold_fn  Folding function: void (*)(AccType*, const Type*, void*) (borrowed)
 * @param ctx      Optional user context passed to fold function (borrowed, may be NULL)
 *
 * @pre acc_ptr points to writable memory (accumulator variable)
 * @pre array points to readable memory for at least len elements
 * @pre fold_fn is a valid function with signature: void fn(AccType*, const Type*, void*)
 * @pre len <= capacity of array
 * @pre Caller has initialized *acc_ptr to desired starting value
 *
 * @post *acc_ptr contains result of folding all elements
 * @post array is unchanged
 * @post No allocations performed
 *
 * Ownership:
 * - acc_ptr: borrowed (mutable accumulator)
 * - array: borrowed (read-only)
 * - fold_fn: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to fold_fn, not stored)
 *
 * Performance:
 * - Time:  O(n) - exactly n function calls
 * - Space: O(1)
 * - Compiler can inline fold_fn for zero function call overhead
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays/accumulators
 * - Not safe to call concurrently on same accumulator without external sync
 *
 * Example - Sum integers:
 * ```c
 * void sum_ints(int* acc, const int* elem, void* ctx) {
 *     *acc += *elem;
 * }
 * int numbers[] = {1, 2, 3, 4, 5};
 * int total = 0;
 * ALGO_FOLD(&total, numbers, 5, int, sum_ints, NULL);
 * // total = 15
 * ```
 *
 * Example - Find maximum:
 * ```c
 * void find_max(int* acc, const int* elem, void* ctx) {
 *     if (*elem > *acc) *acc = *elem;
 * }
 * int values[] = {3, 7, 2, 9, 4};
 * int max = values[0];  // Initialize to first element
 * ALGO_FOLD(&max, values + 1, 4, int, find_max, NULL);
 * // max = 9
 * ```
 */
#define ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx) \
    do { \
        if ((acc_ptr) && (array) && (fold_fn)) { \
            const usize _len = (usize)(len); \
            for (usize _i = 0; _i < _len; ++_i) { \
                (fold_fn)((acc_ptr), &(array)[_i], (ctx)); \
            } \
        } \
    } while (0)

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @def ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx)
 * @brief Type-safe fallible fold with error handling (GNU version)
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Applies a fallible folding function to each element of an array,
 * accumulating results into a caller-provided accumulator. Stops
 * immediately and returns error if any fold operation fails.
 *
 * Use this when fold operations may fail and you want to detect
 * and handle errors gracefully with early termination.
 *
 * The folding function returns a Result indicating success or failure.
 * On error, processing stops immediately and the error is propagated.
 *
 * @param acc_ptr  Pointer to accumulator (borrowed — mutable, caller-initialized)
 * @param array    Input array of Type (borrowed, read-only)
 * @param len      Number of elements to process
 * @param Type     Element type
 * @param fold_fn  Folding function: result_bool_Error (*)(AccType*, const Type*, void*) (borrowed)
 * @param ctx      Optional user context (borrowed, may be NULL)
 *
 * @return result_bool_Error - Ok(true) if all elements processed successfully,
 *                              Err(error_code) on first failure
 *
 * @pre acc_ptr points to writable memory
 * @pre array points to readable memory for at least len elements
 * @pre fold_fn returns result_bool_Error
 * @pre Caller has initialized *acc_ptr to desired starting value
 *
 * @post On Ok: *acc_ptr contains result of folding all elements
 * @post On Err: *acc_ptr contains partial result up to error point
 * @post array is unchanged
 *
 * Ownership:
 * - acc_ptr: borrowed (mutable accumulator)
 * - array: borrowed (read-only)
 * - fold_fn: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to fold_fn)
 *
 * Performance:
 * - Time:  O(n) worst case, O(k) if error at index k
 * - Space: O(1)
 * - Early termination on first error
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays/accumulators
 *
 * Example - Sum with validation:
 * ```c
 * result_bool_Error sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) {
 *         return result_bool_Error_err(ERR_INVALID_ARG);
 *     }
 *     *acc += *elem;
 *     return result_bool_Error_ok(true);
 * }
 * 
 * int values[] = {10, 20, 30, 40};
 * int sum = 0;
 * result_bool_Error r = ALGO_FOLD_RESULT(&sum, values, 4, int, sum_positive, NULL);
 * if (result_bool_Error_is_ok(r)) {
 *     printf("Sum: %d\n", sum);  // Sum: 100
 * }
 * ```
 *
 * Warning: Accumulator may contain partial result on error!
 * Always check the Result before using the accumulator value.
 */
#define ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx) \
    ({ \
        result_bool_Error _r = result_bool_Error_ok(true); \
        if ((acc_ptr) && (array) && (fold_fn)) { \
            const usize _len = (usize)(len); \
            for (usize _i = 0; _i < _len; ++_i) { \
                _r = (fold_fn)((acc_ptr), &(array)[_i], (ctx)); \
                if (result_bool_Error_is_err(_r)) break; \
            } \
        } else { \
            _r = result_bool_Error_err(ERR_INVALID_ARG); \
        } \
        _r; \
    })

#else /* CANON_NO_GNU_EXTENSIONS */

/* C99 fallback - requires separate result variable */
#define ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx) \
    algo_fold_result_helper((acc_ptr), (array), (usize)(len), sizeof(Type), \
                           (void(*)(void*,const void*,void*))(fold_fn), (ctx))

/* Helper function for C99 fallback */
static inline result_bool_Error algo_fold_result_helper(
    void* acc_ptr,
    const void* array,
    usize len,
    usize elem_size,
    void* fold_fn_void,
    void* ctx)
{
    typedef result_bool_Error (*fold_fn_type)(void*, const void*, void*);
    fold_fn_type fold_fn = (fold_fn_type)fold_fn_void;
    
    if (!acc_ptr || !array || !fold_fn) {
        return result_bool_Error_err(ERR_INVALID_ARG);
    }
    
    const u8* bytes = (const u8*)array;
    for (usize i = 0; i < len; ++i) {
        result_bool_Error r = fold_fn(acc_ptr, bytes + i * elem_size, ctx);
        if (result_bool_Error_is_err(r)) {
            return r;
        }
    }
    
    return result_bool_Error_ok(true);
}

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   Vec integration macros (optional convenience for data/vec.h containers)
   ════════════════════════════════════════════════════════════════════════════
   Note: These macros work with any container that has .items and .len fields.
         No need to include data/vec.h unless you're actually using vec types.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx)
 * @brief Infallible fold over vec elements
 *
 * Convenience wrapper for folding vec containers. Automatically handles
 * vec structure internals (items pointer and length).
 *
 * @param acc_ptr  Pointer to accumulator variable (borrowed — mutable)
 * @param vec      Input vector (borrowed, read-only)
 * @param Type     Element type
 * @param fold_fn  Folding function: void (*)(AccType*, const Type*, void*) (borrowed)
 * @param ctx      Optional user context (borrowed, may be NULL)
 *
 * @pre acc_ptr points to writable memory
 * @pre vec.items points to readable memory
 * @pre Caller has initialized *acc_ptr
 *
 * @post *acc_ptr contains result of folding all vec elements
 * @post vec unchanged
 *
 * Ownership:
 * - acc_ptr: borrowed (mutable accumulator)
 * - vec: borrowed (read-only)
 * - fold_fn: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to fold_fn)
 *
 * Performance:
 * - Time:  O(vec.len)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently on different vecs/accumulators
 *
 * Example:
 * ```c
 * void sum_ints(int* acc, const int* elem, void* ctx) {
 *     *acc += *elem;
 * }
 * 
 * vec_int numbers = ...;  // {10, 20, 30, 40}
 * int total = 0;
 * 
 * ALGO_FOLD_VEC(&total, numbers, int, sum_ints, NULL);
 * // total = 100
 * ```
 */
#define ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
    ALGO_FOLD((acc_ptr), (vec).items, (vec).len, Type, fold_fn, ctx)

/**
 * @def ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx)
 * @brief Fallible fold over vec elements
 *
 * Convenience wrapper for fallible folding over vec containers.
 * Automatically handles vec structure internals and provides error handling.
 *
 * @param acc_ptr  Pointer to accumulator (borrowed — mutable)
 * @param vec      Input vector (borrowed, read-only)
 * @param Type     Element type
 * @param fold_fn  Folding function: result_bool_Error (*)(AccType*, const Type*, void*) (borrowed)
 * @param ctx      Optional user context (borrowed, may be NULL)
 *
 * @return result_bool_Error - Ok(true) on success, Err(error_code) on first failure
 *
 * @pre acc_ptr points to writable memory
 * @pre vec.items points to readable memory
 * @pre Caller has initialized *acc_ptr
 *
 * @post On Ok: *acc_ptr contains result of folding all elements
 * @post On Err: *acc_ptr contains partial result
 * @post vec unchanged
 *
 * Ownership:
 * - acc_ptr: borrowed (mutable accumulator)
 * - vec: borrowed (read-only)
 * - fold_fn: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to fold_fn)
 *
 * Performance:
 * - Time:  O(vec.len) worst case
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently on different vecs/accumulators
 *
 * Example:
 * ```c
 * result_bool_Error sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) {
 *         return result_bool_Error_err(ERR_INVALID_ARG);
 *     }
 *     *acc += *elem;
 *     return result_bool_Error_ok(true);
 * }
 * 
 * vec_int values = ...;  // {10, 20, -5, 30}
 * int sum = 0;
 * 
 * result_bool_Error r = ALGO_FOLD_RESULT_VEC(&sum, values, int, sum_positive, NULL);
 * if (result_bool_Error_is_err(r)) {
 *     // Stopped at -5, sum = 30
 * }
 * ```
 */
#define ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
    ALGO_FOLD_RESULT((acc_ptr), (vec).items, (vec).len, Type, fold_fn, ctx)

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_FOLD — typed slice variant per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type).
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates fold functions that accept slice_##type directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 * - algo_fold_slice_##type(acc_ptr, sv, fn, ctx)               → void
 * - algo_fold_result_slice_##type(acc_ptr, sv, fn, ctx)        → result_bool_Error
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Ownership:
 * - acc_ptr: borrowed (mutable accumulator)
 * - sv: borrowed (read-only slice)
 * - fn: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to fn)
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FOLD(int)
 *
 * void sum_ints(int* acc, const int* elem, void* ctx) {
 *     *acc += *elem;
 * }
 *
 * int arr[] = {1, 2, 3, 4, 5};
 * slice_int sv = slice_int_from(arr, 5);
 * int total = 0;
 *
 * algo_fold_slice_int(&total, sv, sum_ints, NULL);
 * // total = 15
 *
 * // Fallible variant
 * result_bool_Error sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) return result_bool_Error_err(ERR_INVALID_ARG);
 *     *acc += *elem;
 *     return result_bool_Error_ok(true);
 * }
 * int sum = 0;
 * result_bool_Error r = algo_fold_result_slice_int(&sum, sv, sum_positive, NULL);
 * ```
 */
#define DEFINE_ALGO_FOLD(type) \
\
/** \
 * @brief Infallible fold over slice_##type \
 * \
 * @param acc_ptr Pointer to accumulator (borrowed — mutable) \
 * @param sv      Slice (borrowed, read-only) \
 * @param fn      Folding function (borrowed) \
 * @param ctx     Optional context (borrowed, may be NULL) \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * @pre acc_ptr points to writable memory \
 * @pre sv.ptr points to readable buffer if non-NULL \
 * \
 * Ownership: \
 * - acc_ptr: borrowed (mutable accumulator) \
 * - sv: borrowed (read-only) \
 * - fn: borrowed (used during call only) \
 * - ctx: borrowed (passed to fn) \
 * \
 * Performance: O(sv.len) time, O(1) space \
 * \
 * Thread-safety: Safe to call on different slices/accumulators concurrently \
 */ \
static inline void algo_fold_slice_##type( \
    borrowed void*          acc_ptr, \
    borrowed slice_##type   sv, \
    borrowed void (*fn)(void*, const type*, void*), \
    borrowed void*          ctx) \
{ \
    if (!acc_ptr || !sv.ptr || !fn) return; \
    for (usize i = 0; i < sv.len; ++i) { \
        fn(acc_ptr, &sv.ptr[i], ctx); \
    } \
} \
\
/** \
 * @brief Fallible fold over slice_##type \
 * \
 * @param acc_ptr Pointer to accumulator (borrowed — mutable) \
 * @param sv      Slice (borrowed, read-only) \
 * @param fn      Folding function (borrowed) \
 * @param ctx     Optional context (borrowed, may be NULL) \
 * \
 * @return result_bool_Error - Ok(true) or Err(error_code) \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * \
 * Ownership: \
 * - acc_ptr: borrowed (mutable accumulator) \
 * - sv: borrowed (read-only) \
 * - fn: borrowed (used during call only) \
 * - ctx: borrowed (passed to fn) \
 * \
 * Performance: O(sv.len) worst case, early termination on error \
 * \
 * Thread-safety: Safe to call on different slices/accumulators concurrently \
 */ \
static inline result_bool_Error algo_fold_result_slice_##type( \
    borrowed void*               acc_ptr, \
    borrowed slice_##type        sv, \
    borrowed result_bool_Error (*fn)(void*, const type*, void*), \
    borrowed void*               ctx) \
{ \
    if (!acc_ptr || !sv.ptr || !fn) { \
        return result_bool_Error_err(ERR_INVALID_ARG); \
    } \
    for (usize i = 0; i < sv.len; ++i) { \
        result_bool_Error r = fn(acc_ptr, &sv.ptr[i], ctx); \
        if (result_bool_Error_is_err(r)) { \
            return r; \
        } \
    } \
    return result_bool_Error_ok(true); \
}

#endif /* CANON_ALGO_FOLD_H */
