// algo/map.h
#ifndef CANON_ALGO_MAP_H
#define CANON_ALGO_MAP_H

#include <stddef.h>
#include "data/vec.h"  // For vec integration macros (optional)

/**
 * @file map.h
 * @brief Element-wise transformations (functional mapping over sequences)
 *
 * Provides efficient, type-safe functional mapping operations that apply
 * transformation functions to each element of an input sequence. Supports
 * both same-type and cross-type transformations with zero heap allocations.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stddef.h)
 *   - Optional integration with vec.h containers
 *   - No external dependencies beyond standard C library
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
 *   - Compiler can inline transformation functions
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
 * - Seamless integration with vec.h containers
 * - Zero-overhead abstraction - compiles to tight loops
 * - Type-safe macros prevent common errors
 * - Composable with other functional operations
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Basic map operation:
 *   1. For each element at index i in input array:
 *   2. Apply transformation function: output[i] = f(input[i])
 *   3. Continue until all elements processed
 *   4. Uses min(input_len, output_len) to prevent buffer overflow
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
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Type conversions (int → double, string → length, etc.)
 * - Mathematical transformations (square, sqrt, absolute value)
 * - String operations (uppercase, lowercase, trim)
 * - Data normalization/denormalization
 * - Unit conversions (meters → feet, celsius → fahrenheit)
 * - In-place modifications (increment all values, clamp ranges)
 * - Safe bounded processing (never overflows output buffer)
 * - Parsing/serialization pipelines
 * - Feature extraction from data
 * - Applying business logic to datasets
 * - Image processing (pixel-wise operations)
 * - Signal processing (sample transformations)
 * - Data cleaning and preprocessing
 * - Functional composition chains
 *
 * Transformation function signatures:
 * ────────────────────────────────────────────────────────────────────────────
 * Different types:
 *   void transform(OutType* out, const InType* in) {
 *       *out = /* compute from *in */;
 *   }
 *
 * In-place:
 *   void modify(Type* elem) {
 *       *elem = /* new value based on *elem */;
 *   }
 *
 * Usage examples:
 *
 * Basic type conversion:
 * ```c
 * void int_to_double(double* out, const int* in) {
 *     *out = (double)(*in);
 * }
 * int input[] = {1, 2, 3, 4, 5};
 * double output[5];
 * ALGO_MAP_TYPED(output, input, 5, double, int, int_to_double);
 * // Result: output = {1.0, 2.0, 3.0, 4.0, 5.0}
 * ```
 *
 * Mathematical transformation:
 * ```c
 * void square(double* out, const int* in) {
 *     *out = (*in) * (*in);
 * }
 * int input[] = {1, 2, 3};
 * double output[3];
 * ALGO_MAP_TYPED(output, input, 3, double, int, square);
 * // Result: output = {1.0, 4.0, 9.0}
 * ```
 *
 * In-place modification:
 * ```c
 * void increment(int* elem) {
 *     (*elem)++;
 * }
 * int arr[] = {10, 20, 30};
 * ALGO_MAP_INPLACE(arr, 3, int, increment);
 * // Result: arr = {11, 21, 31}
 * ```
 *
 * With vec containers:
 * ```c
 * vec_int in_vec = ...; // {1, 2, 3, 4}
 * vec_double out_vec = vec_double_init(...);
 * ALGO_MAP_VEC(out_vec, in_vec, double, int, square);
 * // Result: out_vec = {1.0, 4.0, 9.0, 16.0}
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic map (flexible, different input/output types possible)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Transformation function type (different input/output)
 *
 * Signature for functions that transform input elements to output elements.
 * Input is read-only (const), output is writable.
 *
 * The function receives pointers to elements, not the elements directly,
 * allowing it to work with any element size.
 *
 * @param out  Writable pointer to output element (modified by function)
 * @param in   Read-only pointer to input element (not modified)
 *
 * Example implementation:
 * ```c
 * void double_value(void* out, const void* in) {
 *     *(int*)out = (*(const int*)in) * 2;
 * }
 * ```
 */
typedef void (*algo_map_fn)(void* out, const void* in);

/**
 * @brief Applies transformation to each input element (generic version)
 *
 * Processes arrays of element pointers, applying the transformation function
 * to each input/output pair. Uses the minimum of input and output lengths
 * to ensure safe, bounded operation that never overflows buffers.
 *
 * This is the low-level generic interface. For most use cases, prefer the
 * type-safe ALGO_MAP_TYPED macro instead.
 *
 * Algorithm:
 *   - Determines safe length: min(in_len, out_len)
 *   - Iterates through elements 0 to safe_length-1
 *   - Calls f(output[i], input[i]) for each index
 *   - Guarantees no buffer overflow
 *   - Stops at shorter array's boundary
 *
 * @param input     Array of input pointers (read-only elements)
 * @param output    Array of output pointers (pre-allocated writable storage)
 * @param in_len    Number of input elements available
 * @param out_len   Number of output slots available
 * @param f         Mapping function to apply to each element
 *
 * Preconditions:
 *   - If input != NULL: input points to array of in_len valid pointers
 *   - If output != NULL: output points to array of out_len valid pointers
 *   - Each input[i] points to valid readable memory
 *   - Each output[i] points to valid writable memory
 *   - f is a valid function pointer (if not NULL)
 *
 * Postconditions:
 *   - For each i in [0, min(in_len, out_len)):
 *     output[i] contains result of f(input[i])
 *   - Elements beyond min(in_len, out_len) are unchanged
 *   - No heap allocations performed
 *   - Input array is never modified
 *
 * Performance:
 *   - Time: O(n) where n = min(in_len, out_len)
 *   - Space: O(1) - no allocations
 *   - Function calls: exactly n calls to f
 *   - No heap operations
 *
 * Does nothing if:
 *   - input is NULL
 *   - output is NULL
 *   - f is NULL
 *   - min(in_len, out_len) == 0
 *
 * Example:
 * ```c
 * const int* inputs[3] = {&a, &b, &c};
 * double* outputs[3] = {&x, &y, &z};
 * algo_map(inputs, outputs, 3, 3, my_transform);
 * ```
 *
 * Safety notes:
 *   - Always uses minimum length to prevent overflow
 *   - Caller must ensure output has sufficient capacity
 *   - No bounds checking on individual pointer dereferences
 *   - Caller responsible for memory management
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
 * @param out_array Output array of OutType (caller provides writable storage)
 * @param in_array  Input array of InType (read-only, never modified)
 * @param len       Number of elements to process
 * @param OutType   Output element type (e.g., double, struct Point)
 * @param InType    Input element type (e.g., int, const char*)
 * @param fn        Mapping function: void (*)(OutType*, const InType*)
 *
 * Preconditions:
 *   - out_array points to writable memory for at least len elements
 *   - in_array points to readable memory for at least len elements
 *   - fn is a valid function with signature: void fn(OutType*, const InType*)
 *   - len <= capacity of both arrays
 *
 * Postconditions:
 *   - out_array[i] = result of fn(in_array[i]) for all i in [0, len)
 *   - in_array is unchanged
 *   - No allocations performed
 *
 * Performance:
 *   - Time: O(n) - exactly n function calls
 *   - Space: O(1)
 *   - Compiler can inline fn for zero function call overhead
 *   - Generates tight, vectorizable loops
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
 *
 * Example - String operations:
 * ```c
 * void string_length(size_t* out, const char* const* in) {
 *     *out = strlen(*in);
 * }
 * const char* names[] = {"Alice", "Bob", "Charlie"};
 * size_t lengths[3];
 * ALGO_MAP_TYPED(lengths, names, 3, size_t, const char*, string_length);
 * // lengths = {5, 3, 7}
 * ```
 *
 * Example - Struct transformation:
 * ```c
 * struct Point { int x, y; };
 * struct Vector { double dx, dy; };
 * 
 * void point_to_vector(struct Vector* out, const struct Point* in) {
 *     out->dx = (double)in->x;
 *     out->dy = (double)in->y;
 * }
 * 
 * struct Point points[] = {{1, 2}, {3, 4}, {5, 6}};
 * struct Vector vectors[3];
 * ALGO_MAP_TYPED(vectors, points, 3, struct Vector, struct Point, 
 *                point_to_vector);
 * ```
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
 * @param array   Array of Type (read-write, will be modified)
 * @param len     Number of elements to process
 * @param Type    Element type
 * @param fn      In-place function: void (*)(Type*)
 *
 * Preconditions:
 *   - array points to writable memory for at least len elements
 *   - fn is a valid function with signature: void fn(Type*)
 *   - len <= array capacity
 *
 * Postconditions:
 *   - array[i] = result of fn(array[i]) for all i in [0, len)
 *   - Original values are lost (overwritten)
 *   - No allocations performed
 *
 * Performance:
 *   - Time: O(n) - exactly n function calls
 *   - Space: O(1)
 *   - In-place: no extra memory for output
 *   - Compiler can inline fn for optimal performance
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
 * Example - Double all values:
 * ```c
 * void double_it(int* elem) {
 *     *elem *= 2;
 * }
 * int values[] = {1, 2, 3, 4, 5};
 * ALGO_MAP_INPLACE(values, 5, int, double_it);
 * // values = {2, 4, 6, 8, 10}
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
 * Example - Uppercase string characters:
 * ```c
 * void to_upper(char* c) {
 *     if (*c >= 'a' && *c <= 'z') {
 *         *c = *c - 'a' + 'A';
 *     }
 * }
 * char str[] = "hello";
 * ALGO_MAP_INPLACE(str, 5, char, to_upper);
 * // str = "HELLO"
 * ```
 *
 * Example - Normalize to unit interval [0, 1]:
 * ```c
 * void normalize(double* elem) {
 *     *elem = (*elem - min_val) / (max_val - min_val);
 * }
 * double data[] = {10.0, 20.0, 30.0, 40.0};
 * // Assuming min_val = 10.0, max_val = 40.0
 * ALGO_MAP_INPLACE(data, 4, double, normalize);
 * // data = {0.0, 0.333, 0.666, 1.0}
 * ```
 *
 * Warning: Original data is lost! If you need to preserve the original,
 * use ALGO_MAP_TYPED to write to a separate output array.
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
 * Convenience wrapper for mapping between vec containers. Automatically
 * handles vec structure internals (items pointer and length).
 *
 * Uses minimum of input/output lengths to prevent overflow. This ensures
 * safe operation even if vecs have different sizes.
 *
 * @param out_vec   Output vector (must have sufficient capacity)
 * @param in_vec    Input vector (read-only)
 * @param OutType   Output element type
 * @param InType    Input element type
 * @param fn        Mapping function: void (*)(OutType*, const InType*)
 *
 * Preconditions:
 *   - out_vec.items points to writable memory
 *   - in_vec.items points to readable memory
 *   - out_vec.len <= out_vec.capacity
 *   - Caller has ensured out_vec has sufficient capacity
 *
 * Postconditions:
 *   - First min(out_vec.len, in_vec.len) elements transformed
 *   - Remaining elements in out_vec unchanged
 *   - in_vec unchanged
 *
 * Performance:
 *   - Time: O(min(out_vec.len, in_vec.len))
 *   - Space: O(1)
 *   - No allocations
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
 *   - Caller must ensure output vec has capacity for desired elements
 *   - Set output.len before calling to specify how many to process
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
 * Convenience wrapper when input and output types are identical.
 * Saves typing and makes intent clearer.
 *
 * @param vec_out   Output vector (same type as input)
 * @param vec_in    Input vector
 * @param Type      Element type (same for both)
 * @param fn        Mapping function: void (*)(Type*, const Type*)
 *
 * Example:
 * ```c
 * void double_value(int* out, const int* in) {
 *     *out = (*in) * 2;
 * }
 * 
 * vec_int input = ...;   // {10, 20, 30}
 * vec_int output = vec_int_init(buffer, 10);
 * output.len = 3;
 * 
 * ALGO_MAP_VEC_SAME_TYPE(output, input, int, double_value);
 * // output = {20, 40, 60}
 * ```
 */
#define ALGO_MAP_VEC_SAME_TYPE(vec_out, vec_in, Type, fn) \
    ALGO_MAP_VEC(vec_out, vec_in, Type, Type, fn)

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/map.h"
    #include <stdio.h>
    #include <string.h>
    #include <ctype.h>
    #include <math.h>
    
    // Example 1: Basic type conversion (int to double)
    void int_to_double(double* out, const int* in) {
        *out = (double)(*in);
    }
    
    void example_type_conversion(void) {
        int integers[] = {1, 2, 3, 4, 5};
        double doubles[5];
        
        ALGO_MAP_TYPED(doubles, integers, 5, double, int, int_to_double);
        
        printf("Integers: ");
        for (size_t i = 0; i < 5; i++) printf("%d ", integers[i]);
        printf("\nDoubles:  ");
        for (size_t i = 0; i < 5; i++) printf("%.1f ", doubles[i]);
        printf("\n");
        // Output:
        // Integers: 1 2 3 4 5
        // Doubles:  1.0 2.0 3.0 4.0 5.0
    }
    
    // Example 2: Mathematical transformation (square)
    void square(double* out, const int* in) {
        *out = (*in) * (*in);
    }
    
    void example_square(void) {
        int values[] = {1, 2, 3, 4, 5};
        double squared[5];
        
        ALGO_MAP_TYPED(squared, values, 5, double, int, square);
        
        printf("Values:  ");
        for (size_t i = 0; i < 5; i++) printf("%d ", values[i]);
        printf("\nSquared: ");
        for (size_t i = 0; i < 5; i++) printf("%.0f ", squared[i]);
        printf("\n");
        // Output:
        // Values:  1 2 3 4 5
        // Squared: 1 4 9 16 25
    }
    
    // Example 3: In-place increment
    void increment(int* elem) {
        (*elem)++;
    }
    
    void example_inplace_increment(void) {
        int counters[] = {10, 20, 30, 40, 50};
        
        printf("Before: ");
        for (size_t i = 0; i < 5; i++) printf("%d ", counters[i]);
        printf("\n");
        
        ALGO_MAP_INPLACE(counters, 5, int, increment);
        
        printf("After:  ");
        for (size_t i = 0; i < 5; i++) printf("%d ", counters[i]);
        printf("\n");
        // Output:
        // Before: 10 20 30 40 50
        // After:  11 21 31 41 51
    }
    
    // Example 4: String length extraction
    void string_length(size_t* out, const char* const* in) {
        *out = strlen(*in);
    }
    
    void example_string_lengths(void) {
        const char* names[] = {"Alice", "Bob", "Charlie", "David"};
        size_t lengths[4];
        
        ALGO_MAP_TYPED(lengths, names, 4, size_t, const char*, string_length);
        
        printf("Names and lengths:\n");
        for (size_t i = 0; i < 4; i++) {
            printf("  %s: %zu\n", names[i], lengths[i]);
        }
        // Output:
        // Names and lengths:
        //   Alice: 5
        //   Bob: 3
        //   Charlie: 7
        //   David: 5
    }
    
    // Example 5: Celsius to Fahrenheit conversion
    void celsius_to_fahrenheit(double* out, const double* in) {
        *out = (*in * 9.0 / 5.0) + 32.0;
    }
    
    void example_temperature_conversion(void) {
        double celsius[] = {0.0, 100.0, 37.0, -40.0, 20.0};
        double fahrenheit[5];
        
        ALGO_MAP_TYPED(fahrenheit, celsius, 5, double, double,
                       celsius_to_fahrenheit);
        
        printf("Temperature conversion:\n");
        for (size_t i = 0; i < 5; i++) {
            printf("  %.1f°C = %.1f°F\n", celsius[i], fahrenheit[i]);
        }
        // Output:
        // Temperature conversion:
        //   0.0°C = 32.0°F
        //   100.0°C = 212.0°F
        //   37.0°C = 98.6°F
        //   -40.0°C = -40.0°F
        //   20.0°C = 68.0°F
    }
    
    // Example 6: Struct transformation
    struct Point {
        int x;
        int y;
    };
    
    struct PolarPoint {
        double r;      // radius
        double theta;  // angle in radians
    };
    
    void cartesian_to_polar(struct PolarPoint* out, const struct Point* in) {
        out->r = sqrt(in->x * in->x + in->y * in->y);
        out->theta = atan2(in->y, in->x);
    }
    
    void example_struct_conversion(void) {
        struct Point cartesian[] = {
            {3, 4},
            {1, 1},
            {0, 5},
            {-3, 4}
        };
        struct PolarPoint polar[4];
        
        ALGO_MAP_TYPED(polar, cartesian, 4, struct PolarPoint, struct Point,
                       cartesian_to_polar);
        
        printf("Cartesian to Polar:\n");
        for (size_t i = 0; i < 4; i++) {
            printf("  (%d, %d) → (r=%.2f, θ=%.2f)\n",
                   cartesian[i].x, cartesian[i].y,
                   polar[i].r, polar[i].theta);
        }
    }
    
    // Example 7: Clamping values to range [0, 100]
    void clamp_0_100(int* elem) {
        if (*elem < 0) *elem = 0;
        if (*elem > 100) *elem = 100;
    }
    
    void example_clamp(void) {
        int scores[] = {-10, 50, 150, 75, 200, 0, 100};
        
        printf("Before clamping: ");
        for (size_t i = 0; i < 7; i++) printf("%d ", scores[i]);
        printf("\n");
        
        ALGO_MAP_INPLACE(scores, 7, int, clamp_0_100);
        
        printf("After clamping:  ");
        for (size_t i = 0; i < 7; i++) printf("%d ", scores[i]);
        printf("\n");
        // Output:
        // Before clamping: -10 50 150 75 200 0 100
        // After clamping:  0 50 100 75 100 0 100
    }
    
    // Example 8: String to uppercase (in-place)
    void char_to_upper(char* c) {
        if (*c >= 'a' && *c <= 'z') {
            *c = *c - 'a' + 'A';
        }
    }
    
    void example_uppercase(void) {
        char str[] = "hello world";
        size_t len = strlen(str);
        
        printf("Before: %s\n", str);
        
        ALGO_MAP_INPLACE(str, len, char, char_to_upper);
        
        printf("After:  %s\n", str);
        // Output:
        // Before: hello world
        // After:  HELLO WORLD
    }
    
    // Example 9: Absolute value
    void absolute_value(int* elem) {
        if (*elem < 0) {
            *elem = -(*elem);
        }
    }
    
    void example_absolute(void) {
        int values[] = {-5, 3, -8, 0, 12, -1};
        
        printf("Before: ");
        for (size_t i = 0; i < 6; i++) printf("%d ", values[i]);
        printf("\n");
        
        ALGO_MAP_INPLACE(values, 6, int, absolute_value);
        
        printf("After:  ");
        for (size_t i = 0; i < 6; i++) printf("%d ", values[i]);
        printf("\n");
        // Output:
        // Before: -5 3 -8 0 12 -1
        // After:  5 3 8 0 12 1
    }
    
    // Example 10: Extracting struct field
    struct Person {
        const char* name;
        int age;
        double salary;
    };
    
    void extract_age(int* out, const struct Person* in) {
        *out = in->age;
    }
    
    void example_field_extraction(void) {
        struct Person people[] = {
            {"Alice", 30, 75000.0},
            {"Bob", 25, 60000.0},
            {"Charlie", 35, 90000.0}
        };
        int ages[3];
        
        ALGO_MAP_TYPED(ages, people, 3, int, struct Person, extract_age);
        
        printf("Ages: ");
        for (size_t i = 0; i < 3; i++) printf("%d ", ages[i]);
        printf("\n");
        // Output: Ages: 30 25 35
    }
    
    // Example 11: Normalizing to percentage
    void to_percentage(double* out, const int* in) {
        *out = (*in) / 100.0;
    }
    
    void example_percentage(void) {
        int scores[] = {85, 90, 78, 92, 88};
        double percentages[5];
        
        ALGO_MAP_TYPED(percentages, scores, 5, double, int, to_percentage);
        
        printf("Score -> Percentage:\n");
        for (size_t i = 0; i < 5; i++) {
            printf("  %d -> %.2f\n", scores[i], percentages[i]);
        }
    }
    
    // Example 12: Safe bounded mapping (different lengths)
    void double_value(int* out, const int* in) {
        *out = (*in) * 2;
    }
    
    void example_bounded_mapping(void) {
        int input[] = {1, 2, 3, 4, 5, 6, 7, 8};
        int output[5];  // Smaller output buffer
        
        ALGO_MAP_TYPED(output, input, 5, int, int, double_value);
        
        printf("Input (first 5):  ");
        for (size_t i = 0; i < 5; i++) printf("%d ", input[i]);
        printf("\nOutput: ");
        for (size_t i = 0; i < 5; i++) printf("%d ", output[i]);
        printf("\n");
        // Only processes 5 elements (output buffer size)
        // Remaining input elements ignored
    }
    
    // Example 13: Vec integration
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    DEFINE_VEC(double)
    
    void triple(double* out, const int* in) {
        *out = (*in) * 3.0;
    }
    
    void example_vec_mapping(void) {
        int input_buf[100];
        double output_buf[100];
        
        vec_int input = vec_int_init(input_buf, 100);
        vec_double output = vec_double_init(output_buf, 100);
        
        // Populate input
        for (int i = 1; i <= 5; i++) {
            vec_int_push(&input, i * 10);
        }
        
        // Set output length
        output.len = input.len;
        
        // Map
        ALGO_MAP_VEC(output, input, double, int, triple);
        
        printf("Input:  ");
        for (size_t i = 0; i < input.len; i++) {
            printf("%d ", input.items[i]);
        }
        printf("\nOutput: ");
        for (size_t i = 0; i < output.len; i++) {
            printf("%.0f ", output.items[i]);
        }
        printf("\n");
        // Output:
        // Input:  10 20 30 40 50
        // Output: 30 60 90 120 150
    }
    
    // Example 14: Chaining transformations
    void example_chained_transformations(void) {
        int original[] = {1, 2, 3, 4, 5};
        int doubled[5];
        int final[5];
        
        // First transformation: double
        ALGO_MAP_TYPED(doubled, original, 5, int, int, double_value);
        
        // Second transformation: increment
        void inc_wrapper(int* out, const int* in) {
            *out = *in + 1;
        }
        ALGO_MAP_TYPED(final, doubled, 5, int, int, inc_wrapper);
        
        printf("Original: ");
        for (size_t i = 0; i < 5; i++) printf("%d ", original[i]);
        printf("\nDoubled:  ");
        for (size_t i = 0; i < 5; i++) printf("%d ", doubled[i]);
        printf("\nFinal:    ");
        for (size_t i = 0; i < 5; i++) printf("%d ", final[i]);
        printf("\n");
    }
    
    // Example 15: Image processing (grayscale)
    struct RGB {
        unsigned char r, g, b;
    };
    
    void rgb_to_grayscale(unsigned char* out, const struct RGB* in) {
        // Standard grayscale formula
        *out = (unsigned char)(0.299 * in->r + 0.587 * in->g + 0.114 * in->b);
    }
    
    void example_image_grayscale(void) {
        struct RGB pixels[] = {
            {255, 0, 0},    // Red
            {0, 255, 0},    // Green
            {0, 0, 255},    // Blue
            {255, 255, 0},  // Yellow
            {255, 255, 255} // White
        };
        unsigned char grayscale[5];
        
        ALGO_MAP_TYPED(grayscale, pixels, 5, unsigned char, struct RGB,
                       rgb_to_grayscale);
        
        printf("RGB to Grayscale:\n");
        for (size_t i = 0; i < 5; i++) {
            printf("  RGB(%d,%d,%d) → Gray(%d)\n",
                   pixels[i].r, pixels[i].g, pixels[i].b, grayscale[i]);
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_MAP_H */
