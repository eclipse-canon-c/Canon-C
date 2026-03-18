#ifndef CANON_ALGO_MAP_H
#define CANON_ALGO_MAP_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/slice.h"
#include "core/ownership.h"

/**
 * @file algo/map.h
 * @brief Element-wise transformations (functional mapping over sequences)
 *
 * Provides efficient, type-safe functional mapping operations that apply
 * transformation functions to each element of an input sequence. Supports
 * both same-type and cross-type transformations with zero heap allocations.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Functional programming pattern: map operation
 * - Input is read-only — never mutated (except in-place variant)
 * - Output storage is fully managed by caller (pre-allocated)
 * - No allocation inside functions - caller provides all buffers
 * - No ownership transfer - caller maintains control
 * - Supports same-type and different-type mapping (int→double, etc.)
 * - Explicit in-place mutation variant (when desired)
 * - Bounded and safe (uses minimum of input/output lengths)
 * - Zero-overhead abstraction - compiles to tight loops
 * - Type-safe macros prevent common errors
 * - Composable with other functional operations
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Basic map operation:
 *   1. For each element at index i in input array:
 *   2. Apply transformation function: output[i] = f(input[i])
 *   3. Continue until all elements processed
 *
 * Example: Mapping [1, 2, 3] through square function
 *   Input:  [1, 2, 3]
 *   Step 1: output[0] = square(1) = 1
 *   Step 2: output[1] = square(2) = 4
 *   Step 3: output[2] = square(3) = 9
 *   Result: [1, 4, 9]
 *
 * In-place variant:
 *   1. For each element at index i:
 *   2. Modify in-place: array[i] = f(array[i])
 *   3. Original data is overwritten
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * map.h has minimal dependencies - just types, slices, and ownership.
 * map.h does NOT depend on other algo/ headers (no comparisons needed).
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - No allocations anywhere in this file
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations are thread-safe (no shared mutable state)
 * - Safe to call concurrently on different arrays from multiple threads
 * - Same array should not be modified concurrently without external synchronization
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(n) - exactly n function calls
 * - Space complexity: O(1) - no allocations
 * - No heap allocations
 * - Cache-friendly: sequential access pattern
 * - Compiler can inline transformation functions for zero overhead
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Type conversions (int → double, string → length, etc.)
 * - Mathematical transformations (square, sqrt, absolute value)
 * - String operations (uppercase, lowercase, trim)
 * - Data normalization/denormalization
 * - Unit conversions (meters → feet, celsius → fahrenheit)
 * - In-place modifications (increment all values, clamp ranges)
 * - Parsing/serialization pipelines
 * - Feature extraction from data
 * - Applying business logic to datasets
 * - Image processing (pixel-wise operations)
 * - Signal processing (sample transformations)
 *
 * Quick start:
 * ```c
 * #include "algo/map.h"
 *
 * // Type conversion
 * void int_to_double(double* out, const int* in) {
 *     *out = (double)(*in);
 * }
 * int input[] = {1, 2, 3, 4, 5};
 * double output[5];
 * ALGO_MAP_TYPED(output, input, 5, double, int, int_to_double);
 * // Result: output = {1.0, 2.0, 3.0, 4.0, 5.0}
 *
 * // Mathematical transformation
 * void square(double* out, const int* in) {
 *     *out = (*in) * (*in);
 * }
 * int vals[] = {1, 2, 3};
 * double squared[3];
 * ALGO_MAP_TYPED(squared, vals, 3, double, int, square);
 * // Result: squared = {1.0, 4.0, 9.0}
 *
 * // In-place modification
 * void increment(int* elem) {
 *     (*elem)++;
 * }
 * int arr[] = {10, 20, 30};
 * ALGO_MAP_INPLACE(arr, 3, int, increment);
 * // Result: arr = {11, 21, 31}
 *
 * // Slice variant
 * DEFINE_SLICE(int)
 * DEFINE_SLICE(double)
 * DEFINE_ALGO_MAP(int, double)
 * slice_int sv_in = slice_int_from(input, 5);
 * slice_double sv_out = slice_double_from(output, 5);
 * algo_map_slice_int_double(sv_out, sv_in, int_to_double);
 * ```
 *
 * @sa core/slice.h           — slice_##type used by DEFINE_ALGO_MAP
 * @sa core/ownership.h       — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_MAP_TYPED(out_array, in_array, len, OutType, InType, fn)
 * @brief Type-safe map with different input/output types
 *
 * Applies a transformation function to each element of an input array,
 * writing results to an output array. Provides compile-time type safety
 * by explicitly specifying input and output types.
 *
 * This is the recommended interface for most use cases. It prevents type
 * errors at compile time and generates efficient code with excellent
 * optimization potential.
 *
 * The transformation function receives pointers to individual elements,
 * allowing it to read from input and write to output without copying
 * entire elements.
 *
 * @param out_array Output array of OutType (borrowed — caller provides writable storage)
 * @param in_array  Input array of InType (borrowed, read-only — never modified)
 * @param len       Number of elements to process
 * @param OutType   Output element type (e.g., double, struct Point)
 * @param InType    Input element type (e.g., int, const char*)
 * @param fn        Mapping function: void (*)(OutType*, const InType*)
 *
 * @pre out_array points to writable memory for at least len elements
 * @pre in_array points to readable memory for at least len elements
 * @pre fn is a valid function with signature: void fn(OutType*, const InType*)
 * @pre len <= capacity of both arrays
 *
 * @post out_array[i] = result of fn(in_array[i]) for all i in [0, len)
 * @post in_array is unchanged
 * @post No allocations performed
 *
 * Ownership:
 * - out_array: borrowed (writable buffer provided by caller)
 * - in_array: borrowed (read-only, not modified)
 * - fn: borrowed (function pointer used during call only)
 *
 * Performance:
 * - Time:  O(n) - exactly n function calls
 * - Space: O(1)
 * - Compiler can inline fn for zero function call overhead
 * - Generates tight, vectorizable loops
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays
 * - Not safe to call concurrently on same arrays without external sync
 *
 * Example - Type conversion:
 * ```c
 * void to_double(double* out, const int* in) {
 *     *out = (double)(*in);
 * }
 * int scores[] = {85, 90, 78, 92};
 * double normalized[4];
 * ALGO_MAP_TYPED(normalized, scores, 4, double, int, to_double);
 * ```
 *
 * Example - Mathematical transformation:
 * ```c
 * void square(double* out, const int* in) {
 *     *out = (*in) * (*in);
 * }
 * int values[] = {1, 2, 3, 4, 5};
 * double squared[5];
 * ALGO_MAP_TYPED(squared, values, 5, double, int, square);
 * // squared = {1.0, 4.0, 9.0, 16.0, 25.0}
 * ```
 */
#define ALGO_MAP_TYPED(out_array, in_array, len, OutType, InType, fn) \
    do { \
        if ((out_array) && (in_array) && (fn)) { \
            const usize _len = (usize)(len); \
            for (usize _i = 0; _i < _len; ++_i) { \
                fn(&(out_array)[_i], &(in_array)[_i]); \
            } \
        } \
    } while (0)

/**
 * @def ALGO_MAP_INPLACE(array, len, Type, fn)
 * @brief Explicit in-place mapping (mutates input array)
 *
 * Applies a transformation function to each element of an array, modifying
 * the array in place. Unlike ALGO_MAP_TYPED which reads from one array and
 * writes to another, this variant modifies elements directly.
 *
 * Use this when you want to transform data without allocating a separate
 * output buffer, such as incrementing values, clamping ranges, or applying
 * filters.
 *
 * The transformation function receives a writable pointer to each element
 * and can modify it directly.
 *
 * @param array Array of Type (borrowed — read-write, will be modified)
 * @param len   Number of elements to process
 * @param Type  Element type
 * @param fn    In-place function: void (*)(Type*)
 *
 * @pre array points to writable memory for at least len elements
 * @pre fn is a valid function with signature: void fn(Type*)
 * @pre len <= array capacity
 *
 * @post array[i] = result of fn(array[i]) for all i in [0, len)
 * @post Original values are lost (overwritten)
 * @post No allocations performed
 *
 * Ownership:
 * - array: borrowed (modified in place but not freed)
 * - fn: borrowed (function pointer used during call only)
 *
 * Performance:
 * - Time:  O(n) - exactly n function calls
 * - Space: O(1)
 * - In-place: no extra memory for output
 * - Compiler can inline fn for optimal performance
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays
 * - Not safe to call concurrently on same array without external sync
 *
 * Example - Increment all values:
 * ```c
 * void increment(int* elem) {
 *     (*elem)++;
 * }
 * int counters[] = {0, 5, 10, 15};
 * ALGO_MAP_INPLACE(counters, 4, int, increment);
 * // counters = {1, 6, 11, 16}
 * ```
 *
 * Example - Clamp values to range:
 * ```c
 * void clamp_0_100(int* elem) {
 *     if (*elem < 0) *elem = 0;
 *     if (*elem > 100) *elem = 100;
 * }
 * int scores[] = {-10, 50, 150, 75};
 * ALGO_MAP_INPLACE(scores, 4, int, clamp_0_100);
 * // scores = {0, 50, 100, 75}
 * ```
 *
 * Warning: Original data is lost! If you need to preserve the original,
 * use ALGO_MAP_TYPED to write to a separate output array.
 */
#define ALGO_MAP_INPLACE(array, len, Type, fn) \
    do { \
        if ((array) && (fn)) { \
            const usize _len = (usize)(len); \
            for (usize _i = 0; _i < _len; ++_i) { \
                fn(&(array)[_i]); \
            } \
        } \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_MAP — typed slice variant per element type pair
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(in_type) and DEFINE_SLICE(out_type).
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates map/map_inplace functions that accept slice_##type directly
 *
 * Prerequisites:
 * - DEFINE_SLICE(in_type) must have been called
 * - DEFINE_SLICE(out_type) must have been called (may be same as in_type)
 *
 * Generated functions:
 * - algo_map_slice_##in_type##_##out_type(out_sv, in_sv, fn)       → void
 * - algo_map_inplace_slice_##type(sv, fn)                          → void
 *
 * @param in_type  Input element type — must match a prior DEFINE_SLICE(in_type) call
 * @param out_type Output element type — must match a prior DEFINE_SLICE(out_type) call
 *
 * Ownership:
 * - in_sv: borrowed (read-only)
 * - out_sv: borrowed (writable buffer)
 * - sv: borrowed (modified in place for inplace variant)
 * - fn: borrowed (function pointer used during call only)
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_SLICE(double)
 * DEFINE_ALGO_MAP(int, double)
 *
 * void square(double* out, const int* in) {
 *     *out = (*in) * (*in);
 * }
 *
 * int arr[] = {1, 2, 3, 4, 5};
 * double result[5];
 * slice_int sv_in = slice_int_from(arr, 5);
 * slice_double sv_out = slice_double_from(result, 5);
 *
 * algo_map_slice_int_double(sv_out, sv_in, square);
 * // result = {1.0, 4.0, 9.0, 16.0, 25.0}
 *
 * // In-place variant (same type)
 * DEFINE_ALGO_MAP(int, int)
 * void increment(int* elem) { (*elem)++; }
 * algo_map_inplace_slice_int(sv_in, increment);
 * // arr = {2, 3, 4, 5, 6}
 * ```
 */
#define DEFINE_ALGO_MAP(in_type, out_type) \
\
/** \
 * @brief Maps from slice_##in_type to slice_##out_type \
 * \
 * @param out_sv Output slice (borrowed — writable buffer) \
 * @param in_sv  Input slice (borrowed, read-only) \
 * @param fn     Mapping function: void (*)(out_type*, const in_type*) (borrowed) \
 * \
 * @pre DEFINE_SLICE(in_type) and DEFINE_SLICE(out_type) have been called \
 * @pre out_sv.ptr points to writable buffer if non-NULL \
 * @pre in_sv.ptr points to readable buffer if non-NULL \
 * @pre out_sv.len >= in_sv.len (or will only process min(out_sv.len, in_sv.len)) \
 * \
 * Ownership: \
 * - out_sv: borrowed (writable buffer) \
 * - in_sv: borrowed (read-only) \
 * - fn: borrowed (used during call only) \
 * \
 * Performance: O(min(out_sv.len, in_sv.len)) time, O(1) space \
 * \
 * Thread-safety: Safe to call on different slices concurrently \
 */ \
static inline void algo_map_slice_##in_type##_##out_type( \
    borrowed slice_##out_type out_sv, \
    borrowed slice_##in_type  in_sv, \
    borrowed void (*fn)(out_type*, const in_type*)) \
{ \
    if (!out_sv.ptr || !in_sv.ptr || !fn) return; \
    const usize len = (out_sv.len < in_sv.len) ? out_sv.len : in_sv.len; \
    for (usize i = 0; i < len; ++i) { \
        fn(&out_sv.ptr[i], &in_sv.ptr[i]); \
    } \
} \
\
/** \
 * @brief In-place map for slice_##in_type (only when in_type == out_type) \
 * \
 * @param sv Slice (borrowed — modified in place) \
 * @param fn In-place function: void (*)(in_type*) (borrowed) \
 * \
 * @pre DEFINE_SLICE(in_type) has been called \
 * @pre sv.ptr points to writable buffer if non-NULL \
 * \
 * Ownership: \
 * - sv: borrowed (modified in place) \
 * - fn: borrowed (used during call only) \
 * \
 * Performance: O(sv.len) time, O(1) space \
 * \
 * Thread-safety: Safe to call on different slices concurrently \
 */ \
static inline void algo_map_inplace_slice_##in_type( \
    borrowed slice_##in_type sv, \
    borrowed void (*fn)(in_type*)) \
{ \
    if (!sv.ptr || !fn) return; \
    for (usize i = 0; i < sv.len; ++i) { \
        fn(&sv.ptr[i]); \
    } \
}

/* ════════════════════════════════════════════════════════════════════════════
   Vec integration macros (optional convenience for data/vec.h containers)
   ════════════════════════════════════════════════════════════════════════════
   Note: These macros work with any container that has .items and .len fields.
         No need to include data/vec.h unless you're actually using vec types.
         The macros use duck typing to access .items and .len fields.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_MAP_VEC(out_vec, in_vec, OutType, InType, fn)
 * @brief Maps input vec to output vec (different types possible)
 *
 * Convenience wrapper for mapping between vec containers. Automatically
 * handles vec structure internals (items pointer and length).
 *
 * Uses minimum of input/output lengths to prevent overflow. This ensures
 * safe operation even if vecs have different sizes.
 *
 * @param out_vec   Output vector (borrowed — must have sufficient capacity)
 * @param in_vec    Input vector (borrowed, read-only)
 * @param OutType   Output element type
 * @param InType    Input element type
 * @param fn        Mapping function: void (*)(OutType*, const InType*) (borrowed)
 *
 * @pre out_vec.items points to writable memory
 * @pre in_vec.items points to readable memory
 * @pre out_vec.len <= out_vec.capacity
 * @pre Caller has ensured out_vec has sufficient capacity
 *
 * @post First min(out_vec.len, in_vec.len) elements transformed
 * @post Remaining elements in out_vec unchanged
 * @post in_vec unchanged
 *
 * Ownership:
 * - out_vec: borrowed (writable buffer)
 * - in_vec: borrowed (read-only)
 * - fn: borrowed (function pointer used during call only)
 *
 * Performance:
 * - Time:  O(min(out_vec.len, in_vec.len))
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently on different vecs
 *
 * Example:
 * ```c
 * void square(double* out, const int* in) {
 *     *out = (*in) * (*in);
 * }
 * 
 * vec_int input = ...;  // {1, 2, 3, 4}
 * vec_double output = vec_double_init(buffer, 10);
 * output.len = 4;  // Reserve space
 * 
 * ALGO_MAP_VEC(output, input, double, int, square);
 * // output = {1.0, 4.0, 9.0, 16.0}
 * ```
 *
 * Safety note:
 * - Caller must ensure output vec has capacity for desired elements
 * - Set output.len before calling to specify how many to process
 */
#define ALGO_MAP_VEC(out_vec, in_vec, OutType, InType, fn) \
    ALGO_MAP_TYPED((out_vec).items, (in_vec).items, \
                   ((out_vec).len < (in_vec).len ? (out_vec).len : (in_vec).len), \
                   OutType, InType, fn)

/**
 * @def ALGO_MAP_INPLACE_VEC(vec, Type, fn)
 * @brief In-place map for vec containers
 *
 * Convenience wrapper for in-place transformation of vec containers.
 * Modifies all elements in the vec directly.
 *
 * @param vec  Vector to modify (borrowed — modified in place)
 * @param Type Element type
 * @param fn   In-place function: void (*)(Type*) (borrowed)
 *
 * @pre vec.items points to writable memory
 * @pre vec.len <= vec.capacity
 *
 * @post vec.items[i] = result of fn(vec.items[i]) for all i in [0, vec.len)
 * @post Original values are lost (overwritten)
 *
 * Ownership:
 * - vec: borrowed (modified in place)
 * - fn: borrowed (function pointer used during call only)
 *
 * Performance:
 * - Time:  O(vec.len)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently on different vecs
 *
 * Example:
 * ```c
 * void increment(int* elem) {
 *     (*elem)++;
 * }
 * 
 * vec_int numbers = ...;  // {10, 20, 30}
 * ALGO_MAP_INPLACE_VEC(numbers, int, increment);
 * // numbers = {11, 21, 31}
 * ```
 */
#define ALGO_MAP_INPLACE_VEC(vec, Type, fn) \
    ALGO_MAP_INPLACE((vec).items, (vec).len, Type, fn)

#endif /* CANON_ALGO_MAP_H */
