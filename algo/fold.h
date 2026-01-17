// algo/fold.h
#ifndef CANON_ALGO_FOLD_H
#define CANON_ALGO_FOLD_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros
#include "semantics/result.h"

/**
 * @file fold.h
 * @brief Functional reduction of sequences to single values (fold/reduce)
 *
 * Provides efficient, type-safe functional folding operations that reduce
 * sequences of elements to a single accumulated value. Supports both
 * infallible (always completes) and fallible (early termination) variants.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stddef.h, stdbool.h)
 *   - Optional integration with vec.h containers
 *   - Optional integration with result.h for error handling
 *   - Works on any platform supporting standard C
 *
 * Thread-safety: Functions are pure (no shared state) - safe to call from
 *                multiple threads on different data
 *
 * Performance:
 *   - Time complexity: O(n) - exactly n function calls
 *   - Space complexity: O(1) - no allocations
 *   - No heap allocations
 *   - Cache-friendly: sequential access pattern
 *   - Compiler can inline folding functions
 *   - Early termination in fallible variant
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
 * - Seamless integration with vec.h containers
 * - Zero-overhead abstraction - compiles to tight loops
 * - Type-safe macros prevent common errors
 * - Composable with other functional operations
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
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Aggregation (sum, product, min, max, average)
 * - String concatenation and building
 * - Collecting data into structures
 * - Counting and statistics
 * - Validation with accumulation (all elements valid?)
 * - Fallible parsing with early abort
 * - Building complex objects from elements
 * - Merging data from multiple sources
 * - Accumulating results into collections
 * - Computing checksums/hashes
 * - Tree/graph traversal with accumulation
 * - Event processing with state
 * - Data transformation pipelines
 * - Functional composition chains
 *
 * Fold function signatures:
 * ────────────────────────────────────────────────────────────────────────────
 * Infallible (always completes):
 *   void fold(AccType* acc, const ElemType* elem, void* ctx) {
 *       // Modify *acc based on *elem
 *       // ctx is optional user context
 *   }
 *
 * Fallible (can terminate early):
 *   result_bool_constcharp fold(AccType* acc, const ElemType* elem, void* ctx) {
 *       if (/* error condition */) {
 *           return result_bool_constcharp_err("reason");
 *       }
 *       // Modify *acc based on *elem
 *       return result_bool_constcharp_ok(true);
 *   }
 *
 * Usage examples:
 *
 * Basic sum:
 * ```c
 * void sum_ints(int* acc, const int* elem, void* ctx) {
 *     *acc += *elem;
 * }
 * int numbers[] = {1, 2, 3, 4, 5};
 * int total = 0;
 * ALGO_FOLD(&total, numbers, 5, int, sum_ints, NULL);
 * // Result: total = 15
 * ```
 *
 * Product calculation:
 * ```c
 * void multiply(double* acc, const double* elem, void* ctx) {
 *     *acc *= *elem;
 * }
 * double values[] = {2.0, 3.0, 4.0};
 * double product = 1.0;
 * ALGO_FOLD(&product, values, 3, double, multiply, NULL);
 * // Result: product = 24.0
 * ```
 *
 * Fallible validation:
 * ```c
 * result_bool_constcharp sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) {
 *         return result_bool_constcharp_err("negative value encountered");
 *     }
 *     *acc += *elem;
 *     return result_bool_constcharp_ok(true);
 * }
 * int values[] = {1, 2, -3, 4};
 * int sum = 0;
 * result_bool_constcharp r = ALGO_FOLD_RESULT(&sum, values, 4, int, sum_positive, NULL);
 * if (result_bool_constcharp_is_err(r)) {
 *     // Stopped at index 2 due to negative value
 * }
 * ```
 *
 * With vec containers:
 * ```c
 * vec_int numbers = ...; // {10, 20, 30, 40}
 * int sum = 0;
 * ALGO_FOLD_VEC(&sum, numbers, int, sum_ints, NULL);
 * // Result: sum = 100
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic fold (flexible, pointer-based interface)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Infallible fold function type (generic version)
 *
 * Signature for functions that accumulate values without possibility of failure.
 * The function receives pointers to the accumulator and current element.
 *
 * The accumulator is mutable and should be modified in-place. The element
 * is read-only and should not be modified.
 *
 * @param acc   Writable pointer to accumulator (modified by function)
 * @param elem  Read-only pointer to current element (not modified)
 * @param ctx   Optional user context for stateful operations (can be NULL)
 *
 * Example implementation:
 * ```c
 * void sum_accumulator(void* acc, const void* elem, void* ctx) {
 *     *(int*)acc += *(const int*)elem;
 * }
 * ```
 */
typedef void (*algo_fold_fn)(void* acc, const void* elem, void* ctx);

/**
 * @brief Fallible fold function type (generic version)
 *
 * Signature for functions that accumulate values with possibility of failure.
 * Returns a Result indicating success or error. On error, folding stops
 * immediately and the error is propagated.
 *
 * @param acc   Writable pointer to accumulator
 * @param elem  Read-only pointer to current element
 * @param ctx   Optional user context
 * @return      Ok(true) to continue processing,
 *              Err(reason) to stop and propagate error
 *
 * Example implementation:
 * ```c
 * result_bool_constcharp checked_sum(void* acc, const void* elem, void* ctx) {
 *     int val = *(const int*)elem;
 *     if (val < 0) {
 *         return result_bool_constcharp_err("negative value");
 *     }
 *     *(int*)acc += val;
 *     return result_bool_constcharp_ok(true);
 * }
 * ```
 */
typedef result_bool_constcharp (*algo_fold_result_fn)(
    void* acc,
    const void* elem,
    void* ctx
);

/* ────────────────────────────────────────────────────────────────────────────
   Infallible fold (always completes, void callback)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reduces sequence to single value using accumulator (infallible)
 *
 * Processes array of element pointers left-to-right, applying the folding
 * function to each element. The accumulator is modified in-place and contains
 * the final result after all elements are processed.
 *
 * This is the low-level generic interface. For most use cases, prefer the
 * type-safe ALGO_FOLD macro instead.
 *
 * Algorithm:
 *   - Iterates through elements 0 to len-1
 *   - Calls f(acc, items[i], ctx) for each index
 *   - Accumulator is modified in-place
 *   - Always processes entire sequence (no early termination)
 *   - Returns nothing (void)
 *
 * @param acc    Caller-owned mutable accumulator (initial value provided by caller)
 * @param items  Array of element pointers (read-only elements)
 * @param len    Number of elements to process
 * @param f      Folding function to apply to each element
 * @param ctx    Optional user context passed to fold function (can be NULL)
 *
 * Preconditions:
 *   - If acc != NULL: acc points to valid writable memory
 *   - If items != NULL: items points to array of len valid pointers
 *   - Each items[i] points to valid readable memory
 *   - f is a valid function pointer (if not NULL)
 *   - Caller has initialized accumulator to desired starting value
 *
 * Postconditions:
 *   - *acc contains result of folding all elements
 *   - Input array is unchanged
 *   - No heap allocations performed
 *
 * Performance:
 *   - Time: O(n) where n = len
 *   - Space: O(1) - no allocations
 *   - Function calls: exactly n calls to f
 *   - No heap operations
 *
 * Does nothing if:
 *   - acc is NULL
 *   - items is NULL
 *   - f is NULL
 *   - len == 0
 *
 * Example:
 * ```c
 * void sum_fn(void* acc, const void* elem, void* ctx) {
 *     *(int*)acc += *(const int*)elem;
 * }
 * 
 * int initial = 0;
 * const int* elems[4] = {&a, &b, &c, &d};
 * algo_fold(&initial, (const void**)elems, 4, sum_fn, NULL);
 * ```
 *
 * Safety notes:
 *   - Caller must initialize accumulator before calling
 *   - No bounds checking on individual pointer dereferences
 *   - Caller responsible for memory management
 *   - Silent failure on NULL inputs (no error reporting)
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
 * @brief Reduces sequence to single value with error handling (fallible)
 *
 * Processes array of element pointers left-to-right, applying the folding
 * function to each element. Stops immediately and returns error if any
 * fold operation fails.
 *
 * This variant allows for validation, parsing, or other operations that
 * may fail. The first error encountered stops processing and is returned
 * to the caller.
 *
 * Algorithm:
 *   - Iterates through elements 0 to len-1
 *   - Calls result = f(acc, items[i], ctx) for each index
 *   - If result is Error: stop immediately and return error
 *   - If result is Ok: continue to next element
 *   - Return Ok(true) if all elements processed successfully
 *
 * @param acc    Caller-owned mutable accumulator (initial value provided)
 * @param items  Array of element pointers (read-only elements)
 * @param len    Number of elements to process
 * @param f      Fallible folding function
 * @param ctx    Optional user context passed to fold function (can be NULL)
 * @return       Ok(true) if all elements processed successfully,
 *               Err(reason) on first failure (stops early)
 *
 * Preconditions:
 *   - If acc != NULL: acc points to valid writable memory
 *   - If items != NULL: items points to array of len valid pointers
 *   - Each items[i] points to valid readable memory
 *   - f is a valid function pointer (if not NULL)
 *   - Caller has initialized accumulator to desired starting value
 *
 * Postconditions:
 *   - On Ok(true): *acc contains result of folding all elements
 *   - On Err: *acc contains partially folded result up to error point
 *   - Input array is unchanged
 *   - No heap allocations performed
 *   - Error message indicates reason for failure
 *
 * Performance:
 *   - Time: O(n) worst case, O(k) if error at index k
 *   - Space: O(1) - no allocations
 *   - Function calls: up to n calls to f (stops on first error)
 *   - Early termination saves unnecessary processing
 *
 * Returns error immediately if:
 *   - acc is NULL
 *   - items is NULL
 *   - f is NULL
 *   - Any fold function call returns Err
 *
 * Example:
 * ```c
 * result_bool_constcharp checked_sum(void* acc, const void* elem, void* ctx) {
 *     int val = *(const int*)elem;
 *     if (val < 0) {
 *         return result_bool_constcharp_err("negative value");
 *     }
 *     *(int*)acc += val;
 *     return result_bool_constcharp_ok(true);
 * }
 * 
 * int sum = 0;
 * const int* elems[4] = {&a, &b, &c, &d};
 * result_bool_constcharp r = algo_fold_result(&sum, (const void**)elems, 4, 
 *                                             checked_sum, NULL);
 * if (result_bool_constcharp_is_err(r)) {
 *     printf("Error: %s\n", result_bool_constcharp_unwrap_err(r));
 * }
 * ```
 *
 * Safety notes:
 *   - Accumulator may be partially modified on error
 *   - Caller should handle error cases appropriately
 *   - Error messages should be checked for details
 *   - Early termination is a feature, not a bug
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
            return r;  // Early exit with error
        }
    }

    return result_bool_constcharp_ok(true);
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed macros (recommended — type-safe and clean)
   ──────────────────────────────────────────────────────────────────────────── */

/**
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
 * @param acc_ptr  Pointer to accumulator variable (mutable, caller-initialized)
 * @param array    Input array of Type (read-only, never modified)
 * @param len      Number of elements to process
 * @param Type     Element type (e.g., int, struct Point)
 * @param fold_fn  Folding function: void (*)(AccType*, const Type*, void*)
 * @param ctx      Optional user context passed to fold function (can be NULL)
 *
 * Preconditions:
 *   - acc_ptr points to writable memory (accumulator variable)
 *   - array points to readable memory for at least len elements
 *   - fold_fn is a valid function with signature: void fn(AccType*, const Type*, void*)
 *   - len <= capacity of array
 *   - Caller has initialized *acc_ptr to desired starting value
 *
 * Postconditions:
 *   - *acc_ptr contains result of folding all elements
 *   - array is unchanged
 *   - No allocations performed
 *
 * Performance:
 *   - Time: O(n) - exactly n function calls
 *   - Space: O(1)
 *   - Compiler can inline fold_fn for zero function call overhead
 *   - Generates tight, vectorizable loops
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
 *     if (*elem > *acc) {
 *         *acc = *elem;
 *     }
 * }
 * int values[] = {3, 7, 2, 9, 4};
 * int max = values[0];  // Initialize to first element
 * ALGO_FOLD(&max, values + 1, 4, int, find_max, NULL);
 * // max = 9
 * ```
 *
 * Example - String concatenation length:
 * ```c
 * void add_length(size_t* acc, const char* const* str, void* ctx) {
 *     *acc += strlen(*str);
 * }
 * const char* words[] = {"Hello", " ", "World"};
 * size_t total_len = 0;
 * ALGO_FOLD(&total_len, words, 3, const char*, add_length, NULL);
 * // total_len = 11
 * ```
 *
 * Example - With context (weighted sum):
 * ```c
 * typedef struct { double* weights; } Context;
 * 
 * void weighted_sum(double* acc, const double* elem, void* ctx) {
 *     Context* c = (Context*)ctx;
 *     static size_t idx = 0;
 *     *acc += (*elem) * c->weights[idx++];
 * }
 * 
 * double values[] = {1.0, 2.0, 3.0};
 * double weights[] = {0.5, 0.3, 0.2};
 * Context ctx = {weights};
 * double result = 0.0;
 * ALGO_FOLD(&result, values, 3, double, weighted_sum, &ctx);
 * // result = 1.0*0.5 + 2.0*0.3 + 3.0*0.2 = 1.7
 * ```
 *
 * Example - Struct accumulation:
 * ```c
 * struct Stats { int sum; int count; double avg; };
 * 
 * void update_stats(struct Stats* acc, const int* elem, void* ctx) {
 *     acc->sum += *elem;
 *     acc->count++;
 *     acc->avg = (double)acc->sum / acc->count;
 * }
 * 
 * int scores[] = {85, 90, 78, 92, 88};
 * struct Stats stats = {0, 0, 0.0};
 * ALGO_FOLD(&stats, scores, 5, int, update_stats, NULL);
 * // stats.sum = 433, stats.count = 5, stats.avg = 86.6
 * ```
 */
#define ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx) \
    do { \
        if ((acc_ptr) && (array) && (fold_fn)) { \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len; ++_i) { \
                (fold_fn)((acc_ptr), &(array)[_i], (ctx)); \
            } \
        } \
    } while (0)

/**
 * @brief Type-safe fallible fold with error handling
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
 * @param acc_ptr  Pointer to accumulator variable (mutable, caller-initialized)
 * @param array    Input array of Type (read-only)
 * @param len      Number of elements to process
 * @param Type     Element type
 * @param fold_fn  Folding function: result_bool_constcharp (*)(AccType*, const Type*, void*)
 * @param ctx      Optional user context (can be NULL)
 * @return         Ok(true) if all elements processed successfully,
 *                 Err(reason) on first failure
 *
 * Preconditions:
 *   - acc_ptr points to writable memory (accumulator variable)
 *   - array points to readable memory for at least len elements
 *   - fold_fn is valid function returning result_bool_constcharp
 *   - len <= capacity of array
 *   - Caller has initialized *acc_ptr to desired starting value
 *
 * Postconditions:
 *   - On Ok: *acc_ptr contains result of folding all elements
 *   - On Err: *acc_ptr contains partial result up to error point
 *   - array is unchanged
 *   - No allocations performed
 *
 * Performance:
 *   - Time: O(n) worst case, O(k) if error at index k
 *   - Space: O(1)
 *   - Early termination on first error
 *   - Compiler can inline fold_fn
 *
 * Example - Sum with validation:
 * ```c
 * result_bool_constcharp sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) {
 *         return result_bool_constcharp_err("negative value not allowed");
 *     }
 *     *acc += *elem;
 *     return result_bool_constcharp_ok(true);
 * }
 * 
 * int values[] = {10, 20, 30, 40};
 * int sum = 0;
 * result_bool_constcharp r = ALGO_FOLD_RESULT(&sum, values, 4, int, 
 *                                              sum_positive, NULL);
 * if (result_bool_constcharp_is_ok(r)) {
 *     printf("Sum: %d\n", sum);  // Sum: 100
 * }
 * ```
 *
 * Example - Division with zero check:
 * ```c
 * result_bool_constcharp safe_divide(double* acc, const double* elem, void* ctx) {
 *     if (*elem == 0.0) {
 *         return result_bool_constcharp_err("division by zero");
 *     }
 *     *acc /= *elem;
 *     return result_bool_constcharp_ok(true);
 * }
 * 
 * double divisors[] = {2.0, 4.0, 5.0};
 * double result = 1000.0;
 * result_bool_constcharp r = ALGO_FOLD_RESULT(&result, divisors, 3, double,
 *                                              safe_divide, NULL);
 * // result = 1000.0 / 2.0 / 4.0 / 5.0 = 25.0
 * ```
 *
 * Example - Range validation:
 * ```c
 * result_bool_constcharp validate_range(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0 || *elem > 100) {
 *         return result_bool_constcharp_err("value out of range [0, 100]");
 *     }
 *     *acc += *elem;
 *     return result_bool_constcharp_ok(true);
 * }
 * 
 * int scores[] = {85, 90, 150, 78};  // 150 is invalid
 * int total = 0;
 * result_bool_constcharp r = ALGO_FOLD_RESULT(&total, scores, 4, int,
 *                                              validate_range, NULL);
 * if (result_bool_constcharp_is_err(r)) {
 *     printf("Error: %s\n", result_bool_constcharp_unwrap_err(r));
 *     // Error: value out of range [0, 100]
 *     // total = 175 (partial sum of 85 + 90)
 * }
 * ```
 *
 * Example - Parsing with accumulation:
 * ```c
 * result_bool_constcharp parse_and_sum(int* acc, const char* const* str, void* ctx) {
 *     char* endptr;
 *     long val = strtol(*str, &endptr, 10);
 *     if (*endptr != '\0') {
 *         return result_bool_constcharp_err("invalid integer format");
 *     }
 *     if (val < INT_MIN || val > INT_MAX) {
 *         return result_bool_constcharp_err("integer overflow");
 *     }
 *     *acc += (int)val;
 *     return result_bool_constcharp_ok(true);
 * }
 * 
 * const char* numbers[] = {"10", "20", "invalid", "40"};
 * int sum = 0;
 * result_bool_constcharp r = ALGO_FOLD_RESULT(&sum, numbers, 4, const char*,
 *                                              parse_and_sum, NULL);
 * // Stops at "invalid", sum = 30
 * ```
 *
 * Warning: Accumulator may contain partial result on error!
 * Always check the Result before using the accumulator value.
 */
#define ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx) \
    ({ \
        result_bool_constcharp _r = result_bool_constcharp_ok(true); \
        if ((acc_ptr) && (array) && (fold_fn)) { \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len; ++_i) { \
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
 * Convenience wrapper for folding vec containers. Automatically handles
 * vec structure internals (items pointer and length).
 *
 * @param acc_ptr  Pointer to accumulator variable
 * @param vec      Input vector (read-only)
 * @param Type     Element type
 * @param fold_fn  Folding function: void (*)(AccType*, const Type*, void*)
 * @param ctx      Optional user context
 *
 * Preconditions:
 *   - acc_ptr points to writable memory
 *   - vec.items points to readable memory
 *   - Caller has initialized *acc_ptr
 *
 * Postconditions:
 *   - *acc_ptr contains result of folding all vec elements
 *   - vec unchanged
 *
 * Performance:
 *   - Time: O(vec.len)
 *   - Space: O(1)
 *   - No allocations
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
 * @brief Fallible fold over vec elements
 *
 * Convenience wrapper for fallible folding over vec containers.
 * Automatically handles vec structure internals and provides error handling.
 *
 * @param acc_ptr  Pointer to accumulator variable
 * @param vec      Input vector (read-only)
 * @param Type     Element type
 * @param fold_fn  Folding function: result_bool_constcharp (*)(AccType*, const Type*, void*)
 * @param ctx      Optional user context
 * @return         Ok(true) on success, Err(reason) on first failure
 *
 * Preconditions:
 *   - acc_ptr points to writable memory
 *   - vec.items points to readable memory
 *   - Caller has initialized *acc_ptr
 *
 * Postconditions:
 *   - On Ok: *acc_ptr contains result of folding all elements
 *   - On Err: *acc_ptr contains partial result
 *   - vec unchanged
 *
 * Performance:
 *   - Time: O(vec.len) worst case
 *   - Space: O(1)
 *   - Early termination on error
 *
 * Example:
 * ```c
 * result_bool_constcharp sum_positive(int* acc, const int* elem, void* ctx) {
 *     if (*elem < 0) {
 *         return result_bool_constcharp_err("negative value");
 *     }
 *     *acc += *elem;
 *     return result_bool_constcharp_ok(true);
 * }
 * 
 * vec_int values = ...;  // {10, 20, -5, 30}
 * int sum = 0;
 * 
 * result_bool_constcharp r = ALGO_FOLD_RESULT_VEC(&sum, values, int,
 *                                                   sum_positive, NULL);
 * if (result_bool_constcharp_is_err(r)) {
 *     // Stopped at -5, sum = 30
 * }
 * ```
 */
#define ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx) \
    ALGO_FOLD_RESULT((acc_ptr), (vec).items, (vec).len, Type, fold_fn, ctx)

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/fold.h"
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <limits.h>
    
    // Example 1: Sum of integers
    void sum_ints(int* acc, const int* elem, void* ctx) {
        *acc += *elem;
    }
    
    void example_sum(void) {
        int numbers[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int total = 0;
        
        ALGO_FOLD(&total, numbers, 10, int, sum_ints, NULL);
        
        printf("Sum of 1-10: %d\n", total);
        // Output: Sum of 1-10: 55
    }
    
    // Example 2: Product of numbers
    void multiply(double* acc, const double* elem, void* ctx) {
        *acc *= *elem;
    }
    
    void example_product(void) {
        double values[] = {2.0, 3.0, 4.0, 5.0};
        double product = 1.0;  // Important: initialize to 1 for multiplication
        
        ALGO_FOLD(&product, values, 4, double, multiply, NULL);
        
        printf("Product: %.0f\n", product);
        // Output: Product: 120
    }
    
    // Example 3: Find maximum value
    void find_max(int* acc, const int* elem, void* ctx) {
        if (*elem > *acc) {
            *acc = *elem;
        }
    }
    
    void example_max(void) {
        int values[] = {3, 7, 2, 9, 4, 1, 8};
        int max = values[0];  // Initialize to first element
        
        ALGO_FOLD(&max, values + 1, 6, int, find_max, NULL);
        
        printf("Maximum value: %d\n", max);
        // Output: Maximum value: 9
    }
    
    // Example 4: Find minimum value
    void find_min(int* acc, const int* elem, void* ctx) {
        if (*elem < *acc) {
            *acc = *elem;
        }
    }
    
    void example_min(void) {
        int values[] = {3, 7, 2, 9, 4, 1, 8};
        int min = values[0];
        
        ALGO_FOLD(&min, values + 1, 6, int, find_min, NULL);
        
        printf("Minimum value: %d\n", min);
        // Output: Minimum value: 1
    }
    
    // Example 5: Count elements
    void count_elements(int* acc, const int* elem, void* ctx) {
        (*acc)++;
    }
    
    void example_count(void) {
        int values[] = {10, 20, 30, 40, 50};
        int count = 0;
        
        ALGO_FOLD(&count, values, 5, int, count_elements, NULL);
        
        printf("Count: %d\n", count);
        // Output: Count: 5
    }
    
    // Example 6: Average calculation
    typedef struct {
        double sum;
        int count;
    } Average;
    
    void accumulate_average(Average* acc, const int* elem, void* ctx) {
        acc->sum += *elem;
        acc->count++;
    }
    
    void example_average(void) {
        int scores[] = {85, 90, 78, 92, 88, 95};
        Average avg = {0.0, 0};
        
        ALGO_FOLD(&avg, scores, 6, int, accumulate_average, NULL);
        
        double result = avg.sum / avg.count;
        printf("Average score: %.2f\n", result);
        // Output: Average score: 88.00
    }
    
    // Example 7: String concatenation length
    void add_string_length(size_t* acc, const char* const* str, void* ctx) {
        *acc += strlen(*str);
    }
    
    void example_string_length(void) {
        const char* words[] = {"Hello", " ", "World", "!", " ", "From", " ", "C"};
        size_t total_length = 0;
        
        ALGO_FOLD(&total_length, words, 8, const char*, add_string_length, NULL);
        
        printf("Total character count: %zu\n", total_length);
        // Output: Total character count: 18
    }
    
    // Example 8: Fallible sum (positive numbers only)
    result_bool_constcharp sum_positive(int* acc, const int* elem, void* ctx) {
        if (*elem < 0) {
            return result_bool_constcharp_err("negative value encountered");
        }
        *acc += *elem;
        return result_bool_constcharp_ok(true);
    }
    
    void example_fallible_sum(void) {
        int valid[] = {10, 20, 30, 40};
        int invalid[] = {10, 20, -5, 40};
        int sum;
        
        // Valid case
        sum = 0;
        result_bool_constcharp r1 = ALGO_FOLD_RESULT(&sum, valid, 4, int, 
                                                      sum_positive, NULL);
        if (result_bool_constcharp_is_ok(r1)) {
            printf("Valid sum: %d\n", sum);  // Valid sum: 100
        }
        
        // Invalid case
        sum = 0;
        result_bool_constcharp r2 = ALGO_FOLD_RESULT(&sum, invalid, 4, int,
                                                      sum_positive, NULL);
        if (result_bool_constcharp_is_err(r2)) {
            printf("Error: %s (partial sum: %d)\n", 
                   result_bool_constcharp_unwrap_err(r2), sum);
            // Error: negative value encountered (partial sum: 30)
        }
    }
    
    // Example 9: Division with zero check
    result_bool_constcharp safe_divide(double* acc, const double* elem, void* ctx) {
        if (*elem == 0.0) {
            return result_bool_constcharp_err("division by zero");
        }
        *acc /= *elem;
        return result_bool_constcharp_ok(true);
    }
    
    void example_safe_division(void) {
        double divisors[] = {2.0, 4.0, 5.0};
        double result = 1000.0;
        
        result_bool_constcharp r = ALGO_FOLD_RESULT(&result, divisors, 3, double,
                                                     safe_divide, NULL);
        if (result_bool_constcharp_is_ok(r)) {
            printf("Division result: %.2f\n", result);
            // Division result: 25.00  (1000 / 2 / 4 / 5)
        }
    }
    
    // Example 10: Weighted sum with context
    typedef struct {
        const double* weights;
        size_t index;
    } WeightContext;
    
    void weighted_sum(double* acc, const double* elem, void* ctx) {
        WeightContext* wctx = (WeightContext*)ctx;
        *acc += (*elem) * wctx->weights[wctx->index++];
    }
    
    void example_weighted_sum(void) {
        double values[] = {10.0, 20.0, 30.0, 40.0};
        double weights[] = {0.1, 0.2, 0.3, 0.4};
        WeightContext wctx = {weights, 0};
        double result = 0.0;
        
        ALGO_FOLD(&result, values, 4, double, weighted_sum, &wctx);
        
        printf("Weighted sum: %.2f\n", result);
        // Weighted sum: 30.00  (10*0.1 + 20*0.2 + 30*0.3 + 40*0.4)
    }
    
    // Example 11: Counting specific values
    typedef struct {
        int target;
        int count;
    } CountContext;
    
    void count_target(CountContext* acc, const int* elem, void* ctx) {
        if (*elem == acc->target) {
            acc->count++;
        }
    }
    
    void example_count_specific(void) {
        int values[] = {1, 2, 3, 2, 4, 2, 5, 2};
        CountContext ctx = {2, 0};  // Count occurrences of 2
        
        ALGO_FOLD(&ctx, values, 8, int, count_target, NULL);
        
        printf("Count of %d: %d\n", ctx.target, ctx.count);
        // Output: Count of 2: 4
    }
    
    // Example 12: Statistics accumulation
    typedef struct {
        int min;
        int max;
        int sum;
        int count;
        double mean;
    } Statistics;
    
    void update_statistics(Statistics* acc, const int* elem, void* ctx) {
        if (acc->count == 0) {
            acc->min = *elem;
            acc->max = *elem;
        } else {
            if (*elem < acc->min) acc->min = *elem;
            if (*elem > acc->max) acc->max = *elem;
        }
        acc->sum += *elem;
        acc->count++;
        acc->mean = (double)acc->sum / acc->count;
    }
    
    void example_statistics(void) {
        int data[] = {23, 45, 12, 67, 34, 89, 56, 78};
        Statistics stats = {0, 0, 0, 0, 0.0};
        
        ALGO_FOLD(&stats, data, 8, int, update_statistics, NULL);
        
        printf("Statistics:\n");
        printf("  Min:   %d\n", stats.min);
        printf("  Max:   %d\n", stats.max);
        printf("  Sum:   %d\n", stats.sum);
        printf("  Count: %d\n", stats.count);
        printf("  Mean:  %.2f\n", stats.mean);
        // Output:
        // Statistics:
        //   Min:   12
        //   Max:   89
        //   Sum:   404
        //   Count: 8
        //   Mean:  50.50
    }
    
    // Example 13: String parsing with validation
    result_bool_constcharp parse_int_checked(int* acc, const char* const* str, 
                                              void* ctx) {
        char* endptr;
        errno = 0;
        long val = strtol(*str, &endptr, 10);
        
        if (errno == ERANGE || val < INT_MIN || val > INT_MAX) {
            return result_bool_constcharp_err("integer overflow");
        }
        if (*endptr != '\0') {
            return result_bool_constcharp_err("invalid integer format");
        }
        
        *acc += (int)val;
        return result_bool_constcharp_ok(true);
    }
    
    void example_parse_integers(void) {
        const char* valid_nums[] = {"10", "20", "30", "40"};
        const char* invalid_nums[] = {"10", "20", "abc", "40"};
        int sum;
        
        // Valid parsing
        sum = 0;
        result_bool_constcharp r1 = ALGO_FOLD_RESULT(&sum, valid_nums, 4, 
                                                      const char*, 
                                                      parse_int_checked, NULL);
        if (result_bool_constcharp_is_ok(r1)) {
            printf("Parsed sum: %d\n", sum);  // Parsed sum: 100
        }
        
        // Invalid parsing
        sum = 0;
        result_bool_constcharp r2 = ALGO_FOLD_RESULT(&sum, invalid_nums, 4,
                                                      const char*,
                                                      parse_int_checked, NULL);
        if (result_bool_constcharp_is_err(r2)) {
            printf("Parse error: %s (partial: %d)\n",
                   result_bool_constcharp_unwrap_err(r2), sum);
            // Parse error: invalid integer format (partial: 30)
        }
    }
    
    // Example 14: Vec integration (infallible)
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_vec_fold(void) {
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        for (int i = 1; i <= 10; i++) {
            vec_int_push(&numbers, i);
        }
        
        int sum = 0;
        ALGO_FOLD_VEC(&sum, numbers, int, sum_ints, NULL);
        
        printf("Vec sum: %d\n", sum);
        // Output: Vec sum: 55
    }
    
    // Example 15: Vec integration (fallible)
    void example_vec_fold_fallible(void) {
        int buffer[100];
        vec_int values = vec_int_init(buffer, 100);
        
        vec_int_push(&values, 10);
        vec_int_push(&values, 20);
        vec_int_push(&values, -5);  // Invalid
        vec_int_push(&values, 30);
        
        int sum = 0;
        result_bool_constcharp r = ALGO_FOLD_RESULT_VEC(&sum, values, int,
                                                         sum_positive, NULL);
        
        if (result_bool_constcharp_is_err(r)) {
            printf("Vec fold error: %s (partial: %d)\n",
                   result_bool_constcharp_unwrap_err(r), sum);
            // Vec fold error: negative value encountered (partial: 30)
        }
    }
    
    // Example 16: Concatenating struct fields
    typedef struct {
        int id;
        double value;
    } Record;
    
    void sum_record_values(double* acc, const Record* rec, void* ctx) {
        *acc += rec->value;
    }
    
    void example_struct_fold(void) {
        Record records[] = {
            {1, 10.5},
            {2, 20.3},
            {3, 15.7},
            {4, 30.2}
        };
        double total = 0.0;
        
        ALGO_FOLD(&total, records, 4, Record, sum_record_values, NULL);
        
        printf("Total value: %.2f\n", total);
        // Output: Total value: 76.70
    }
    
    // Example 17: Boolean all (check if all elements satisfy condition)
    typedef struct {
        bool all_positive;
    } AllContext;
    
    void check_all_positive(AllContext* acc, const int* elem, void* ctx) {
        if (*elem <= 0) {
            acc->all_positive = false;
        }
    }
    
    void example_all_positive(void) {
        int positive[] = {1, 2, 3, 4, 5};
        int mixed[] = {1, 2, -3, 4, 5};
        AllContext ctx;
        
        // All positive
        ctx.all_positive = true;
        ALGO_FOLD(&ctx, positive, 5, int, check_all_positive, NULL);
        printf("All positive? %s\n", ctx.all_positive ? "Yes" : "No");
        // All positive? Yes
        
        // Mixed
        ctx.all_positive = true;
        ALGO_FOLD(&ctx, mixed, 5, int, check_all_positive, NULL);
        printf("All positive? %s\n", ctx.all_positive ? "Yes" : "No");
        // All positive? No
    }
    
    // Example 18: Building a histogram
    #define MAX_VAL 10
    
    typedef struct {
        int bins[MAX_VAL];
    } Histogram;
    
    void build_histogram(Histogram* acc, const int* elem, void* ctx) {
        if (*elem >= 0 && *elem < MAX_VAL) {
            acc->bins[*elem]++;
        }
    }
    
    void example_histogram(void) {
        int values[] = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5};
        Histogram hist = {0};
        
        ALGO_FOLD(&hist, values, 15, int, build_histogram, NULL);
        
        printf("Histogram:\n");
        for (int i = 0; i < MAX_VAL; i++) {
            if (hist.bins[i] > 0) {
                printf("  %d: ", i);
                for (int j = 0; j < hist.bins[i]; j++) {
                    printf("*");
                }
                printf(" (%d)\n", hist.bins[i]);
            }
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_FOLD_H */
