// algo/map.h
#ifndef CANON_ALGO_MAP_H
#define CANON_ALGO_MAP_H

#include <stddef.h>
#include "data/vec.h"  // For vec integration macros (optional)

/**
 * @file map.h
 * @brief Element-wise transformations (functional mapping over sequences)
 *
 * Applies a transformation function to each element of an input sequence,
 * writing results to a caller-provided output buffer.
 *
 * Core ideas:
 *   - Input is read-only — never mutated
 *   - Output storage is fully managed by caller (pre-allocated)
 *   - No allocation inside functions
 *   - No ownership transfer
 *   - Supports same-type and different-type mapping
 *   - Explicit in-place mutation variant (when desired)
 *   - Bounded and safe (uses minimum of input/output lengths)
 *   - Seamless integration with vec.h containers
 *
 * Typical use cases:
 *   - Convert types (int → double, string → length)
 *   - Apply transformations (square numbers, uppercase strings)
 *   - In-place modification (increment all values)
 *   - Safe bounded processing (never overflows output)
 *
 * Transformation function signatures:
 *   - Different types: void fn(OutType* out, const InType* in)
 *   - In-place:      void fn(Type* elem)
 *
 * Usage example (typed):
 *   void square(const int* in, double* out) { *out = (*in) * (*in); }
 *   int input[] = {1, 2, 3};
 *   double output[3];
 *   ALGO_MAP_TYPED(output, input, 3, double, int, square);
 *
 * Usage example (in-place):
 *   void increment(int* elem) { (*elem)++; }
 *   int arr[] = {10, 20, 30};
 *   ALGO_MAP_INPLACE(arr, 3, int, increment);
 *   // arr now {11, 21, 31}
 *
 * Usage example (vec):
 *   vec_int in_vec = ...;
 *   vec_double out_vec = vec_double_init(...);
 *   ALGO_MAP_VEC(out_vec, in_vec, double, int, square);
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic map (flexible, different input/output types possible)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Transformation function type (different input/output)
 *
 * @param out  Writable pointer to output element
 * @param in   Read-only pointer to input element
 */
typedef void (*algo_map_fn)(void* out, const void* in);

/**
 * @brief Applies transformation to each input element
 *
 * Writes results to caller-provided output array.
 * Uses minimum of input/output lengths to prevent overflow.
 *
 * @param input     Array of input pointers (or cast from contiguous array)
 * @param output    Array of output pointers (pre-allocated writable storage)
 * @param in_len    Number of input elements
 * @param out_len   Number of output slots available
 * @param f         Mapping function
 *
 * Does nothing on invalid input (NULL input/output/f).
 * Safe: never writes beyond min(in_len, out_len).
 */
static inline void algo_map(
    const void** input,   // array of input pointers
    void** output,        // array of output pointers
    size_t in_len,
    size_t out_len,
    algo_map_fn f
) {
    if (!input || !output || !f) return;

    const size_t len = (in_len < out_len) ? in_len : out_len;
    for (size_t i = 0; i < len; ++i) {
        f(output[i], input[i]);
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed mapping (recommended — type-safe and clean)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Typed map with different input/output types
 *
 * @param out_array Output array of OutType (caller provides writable storage)
 * @param in_array  Input array of InType (read-only)
 * @param len       Number of elements to process (uses this length)
 * @param OutType   Output element type
 * @param InType    Input element type
 * @param fn        Mapping function: void (*)(OutType*, const InType*)
 */
#define ALGO_MAP_TYPED(out_array, in_array, len, OutType, InType, fn) \
    do { \
        if ((out_array) && (in_array) && (fn)) { \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len; ++_i) { \
                fn(&(out_array)[_i], &(in_array)[_i]); \
            } \
        } \
    } while (0)

/**
 * @brief Explicit in-place mapping (mutates input array)
 *
 * @param array   Array of Type (read-write)
 * @param len     Number of elements
 * @param Type    Element type
 * @param fn      In-place function: void (*)(Type*)
 */
#define ALGO_MAP_INPLACE(array, len, Type, fn) \
    do { \
        if ((array) && (fn)) { \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len; ++_i) { \
                fn(&(array)[_i]); \
            } \
        } \
    } while (0)

/* ────────────────────────────────────────────────────────────────────────────
   Vec integration (most convenient for Canon-C containers)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Maps input vec to output vec (different types possible)
 *
 * Uses minimum of input/output lengths.
 * Output vec must have sufficient capacity (caller responsibility).
 */
#define ALGO_MAP_VEC(out_vec, in_vec, OutType, InType, fn) \
    ALGO_MAP_TYPED( \
        (out_vec).items, \
        (in_vec).items, \
        ((out_vec).len < (in_vec).len ? (out_vec).len : (in_vec).len), \
        OutType, InType, \
        fn \
    )

/**
 * @brief Maps input vec to output vec (same type)
 *
 * Convenience wrapper when input/output types match.
 */
#define ALGO_MAP_VEC_SAME_TYPE(vec_out, vec_in, Type, fn) \
    ALGO_MAP_VEC(vec_out, vec_in, Type, Type, fn)

#endif /* CANON_ALGO_MAP_H */
