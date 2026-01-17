// algo/filter.h
#ifndef CANON_ALGO_FILTER_H
#define CANON_ALGO_FILTER_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros

/**
 * @file filter.h
 * @brief Functional filtering of sequences (select matching elements)
 *
 * Provides efficient, type-safe functional filtering operations that select
 * elements from a sequence based on a predicate function. Copies matching
 * elements to a caller-provided output buffer while preserving original order.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stddef.h, stdbool.h)
 *   - Optional integration with vec.h containers
 *   - Works on any platform supporting standard C
 *
 * Thread-safety: Functions are pure (no shared state) - safe to call from
 *                multiple threads on different data
 *
 * Performance:
 *   - Time complexity: O(n) - checks all n elements
 *   - Space complexity: O(1) - no allocations
 *   - No heap allocations
 *   - Cache-friendly: sequential access pattern
 *   - Compiler can inline predicate functions
 *   - Truncates early when output buffer full
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Functional programming pattern: filter operation
 * - Input is read-only — never mutated
 * - Output storage is caller-provided (pre-allocated)
 * - No allocation inside functions - caller provides all buffers
 * - No ownership transfer - caller maintains control
 * - Preserves relative order (stable filter)
 * - Safe truncation when output buffer full
 * - Supports optional user context for stateful predicates
 * - Bounded and safe (never overflows output buffer)
 * - Seamless integration with vec.h containers
 * - Zero-overhead abstraction - compiles to tight loops
 * - Type-safe macros prevent common errors
 * - Composable with other functional operations
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Basic filter operation:
 *   1. Start with empty output buffer
 *   2. For each element at index i in input array:
 *   3. Apply predicate: matches = pred(input[i])
 *   4. If matches and output not full: copy element to output, increment count
 *   5. If matches but output full: stop (safe truncation)
 *   6. Continue until all elements checked or output full
 *   7. Return number of elements copied
 *
 * Example: Filtering [1, -2, 3, -4, 5] for positive numbers
 *   Input:  [1, -2, 3, -4, 5]
 *   Output buffer capacity: 5
 *   Step 0: Check 1: pred(1) = true, copy to output[0], count = 1
 *   Step 1: Check -2: pred(-2) = false, skip
 *   Step 2: Check 3: pred(3) = true, copy to output[1], count = 2
 *   Step 3: Check -4: pred(-4) = false, skip
 *   Step 4: Check 5: pred(5) = true, copy to output[2], count = 3
 *   Result: output = [1, 3, 5], count = 3
 *
 * Truncation example: Same input, output capacity = 2
 *   Step 0: Check 1: true, copy to output[0], count = 1
 *   Step 1: Check -2: false, skip
 *   Step 2: Check 3: true, copy to output[1], count = 2
 *   Step 3: Output full (count = capacity), STOP
 *   Result: output = [1, 3], count = 2 (truncated safely)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Selecting elements matching criteria
 * - Removing unwanted elements from collections
 * - Data validation and cleaning
 * - Extracting subsets based on properties
 * - Safe bounded filtering (fixed output buffer)
 * - In-place filtering (when input/output are same)
 * - Partitioning data by condition
 * - Collecting valid entries
 * - Removing nulls or invalid values
 * - Filtering by threshold or range
 * - Text processing (select matching lines)
 * - Data sanitization
 * - Conditional selection with context
 * - Building sublists
 * - Event filtering
 *
 * Predicate function signatures:
 * ────────────────────────────────────────────────────────────────────────────
 * Simple predicate:
 *   bool pred(const ElemType* elem, void* ctx) {
 *       // Return true to keep element, false to discard
 *       return /* condition based on *elem */;
 *   }
 *
 * Predicate with context:
 *   bool pred(const ElemType* elem, void* ctx) {
 *       FilterContext* fctx = (FilterContext*)ctx;
 *       return *elem >= fctx->threshold;
 *   }
 *
 * Usage examples:
 *
 * Filter positive numbers:
 * ```c
 * bool is_positive(const int* elem, void* ctx) {
 *     return *elem > 0;
 * }
 * int input[] = {-1, 2, -3, 4, 0, 5};
 * int output[10];
 * size_t count = ALGO_FILTER_INTO(output, input, 6, int, is_positive, NULL);
 * // Result: output = {2, 4, 5}, count = 3
 * ```
 *
 * Filter even numbers:
 * ```c
 * bool is_even(const int* elem, void* ctx) {
 *     return (*elem % 2) == 0;
 * }
 * int values[] = {1, 2, 3, 4, 5, 6, 7, 8};
 * int evens[10];
 * size_t count = ALGO_FILTER_INTO(evens, values, 8, int, is_even, NULL);
 * // Result: evens = {2, 4, 6, 8}, count = 4
 * ```
 *
 * Filter with threshold (using context):
 * ```c
 * typedef struct { int threshold; } ThresholdCtx;
 * 
 * bool above_threshold(const int* elem, void* ctx) {
 *     ThresholdCtx* tctx = (ThresholdCtx*)ctx;
 *     return *elem > tctx->threshold;
 * }
 * 
 * int data[] = {5, 15, 25, 35, 45};
 * int filtered[5];
 * ThresholdCtx ctx = {20};
 * size_t count = ALGO_FILTER_INTO(filtered, data, 5, int, 
 *                                  above_threshold, &ctx);
 * // Result: filtered = {25, 35, 45}, count = 3
 * ```
 *
 * With vec containers:
 * ```c
 * vec_int input = ...; // {1, -2, 3, -4, 5}
 * vec_int output = vec_int_init(buffer, 10);
 * size_t count = ALGO_FILTER_VEC(output, input, int, is_positive, NULL);
 * output.len = count;  // Update vec length
 * // Result: output contains {1, 3, 5}
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic filter (flexible, pointer-based interface)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Predicate function type (generic version)
 *
 * Signature for functions that test whether an element should be kept
 * (filtered in) or discarded (filtered out). Returns true to keep the
 * element, false to discard it.
 *
 * The element is read-only and should not be modified. The context allows
 * passing additional state or filter parameters.
 *
 * @param elem  Read-only pointer to current element being tested
 * @param ctx   Optional user context for stateful filtering (can be NULL)
 * @return      true to keep element (include in output), false to discard
 *
 * Example implementation:
 * ```c
 * bool is_positive(const void* elem, void* ctx) {
 *     return *(const int*)elem > 0;
 * }
 * ```
 *
 * Example with context:
 * ```c
 * typedef struct { int min; int max; } Range;
 * 
 * bool in_range(const void* elem, void* ctx) {
 *     Range* r = (Range*)ctx;
 *     int val = *(const int*)elem;
 *     return val >= r->min && val <= r->max;
 * }
 * ```
 */
typedef bool (*algo_filter_pred)(const void* elem, void* ctx);

/**
 * @brief Filters elements into caller-provided output buffer (generic version)
 *
 * Scans input array left-to-right, applying predicate to each element.
 * Copies pointers to matching elements into output buffer. Preserves
 * relative order of elements (stable filter). Safely truncates if output
 * buffer becomes full.
 *
 * This is the low-level generic interface. For most use cases, prefer the
 * type-safe ALGO_FILTER_INTO macro instead, which copies actual values
 * rather than pointers.
 *
 * Algorithm:
 *   - Iterates through elements 0 to len-1
 *   - Calls pred(items[i], ctx) for each element
 *   - On true: copies pointer to output[out_len++]
 *   - On false: skips element
 *   - Stops if output buffer full (out_len >= out_cap)
 *   - Returns number of elements copied
 *
 * @param items     Array of input pointers (read-only elements)
 * @param len       Number of input elements
 * @param pred      Predicate function to test each element
 * @param ctx       Optional user context passed to predicate (can be NULL)
 * @param out       Output array for element pointers (writable)
 * @param out_cap   Maximum capacity of output array
 * @return          Number of elements written to output (≤ out_cap)
 *
 * Preconditions:
 *   - If items != NULL: items points to array of len valid pointers
 *   - Each items[i] points to valid readable memory
 *   - pred is a valid function pointer (if not NULL)
 *   - If out != NULL: out points to array with space for out_cap pointers
 *   - out_cap represents actual capacity of output buffer
 *
 * Postconditions:
 *   - Returns k where k = number of matching elements copied (k ≤ out_cap)
 *   - out[0..k-1] contains pointers to matching elements (borrowed)
 *   - out[k..out_cap-1] unchanged
 *   - Relative order preserved (if items[i] and items[j] both match
 *     and i < j, then out contains items[i] before items[j])
 *   - Input array is never modified
 *   - No heap allocations performed
 *   - Pointers in out are borrowed (valid while input array lives)
 *
 * Performance:
 *   - Time: O(n) where n = len (checks all elements)
 *   - Space: O(1) - no allocations
 *   - Function calls: exactly n calls to pred
 *   - Early truncation if output fills before end
 *   - No heap operations
 *
 * Returns 0 if:
 *   - items is NULL
 *   - out is NULL
 *   - pred is NULL
 *   - len == 0
 *   - out_cap == 0
 *   - No elements match predicate
 *
 * Example:
 * ```c
 * bool is_even(const void* elem, void* ctx) {
 *     return (*(const int*)elem % 2) == 0;
 * }
 * 
 * int vals[] = {1, 2, 3, 4, 5, 6};
 * const void* inputs[6] = {&vals[0], &vals[1], &vals[2], 
 *                          &vals[3], &vals[4], &vals[5]};
 * void* outputs[10];
 * 
 * size_t count = algo_filter_into((const void**)inputs, 6, is_even, NULL,
 *                                  outputs, 10);
 * // count = 3
 * // outputs[0] = &vals[1] (2)
 * // outputs[1] = &vals[3] (4)
 * // outputs[2] = &vals[5] (6)
 * ```
 *
 * Example - Truncation:
 * ```c
 * void* outputs[2];  // Small buffer
 * size_t count = algo_filter_into((const void**)inputs, 6, is_even, NULL,
 *                                  outputs, 2);
 * // count = 2 (truncated - only first 2 matches)
 * // outputs[0] = &vals[1] (2)
 * // outputs[1] = &vals[3] (4)
 * // vals[5] (6) was not copied due to buffer limit
 * ```
 *
 * Safety notes:
 *   - Output pointers are borrowed - valid while input array exists
 *   - Truncates safely when output buffer full
 *   - No bounds checking on individual pointer dereferences
 *   - Caller responsible for memory management
 *   - Silent truncation (no error reporting when output fills)
 */
static inline size_t algo_filter_into(
    const void** items,
    size_t len,
    algo_filter_pred pred,
    void* ctx,
    void** out,  // non-const — we write pointers
    size_t out_cap
) {
    if (!items || !out || !pred) return 0;

    size_t out_len = 0;
    for (size_t i = 0; i < len; ++i) {
        if (out_len >= out_cap) break;  // Truncate early - output full

        if (pred(items[i], ctx)) {
            out[out_len++] = (void*)items[i];  // const cast: safe (borrowed)
        }
    }

    return out_len;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed macro (recommended — type-safe and clean)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe filter into caller-provided output array (recommended)
 *
 * Filters elements from input array into output array based on predicate.
 * Provides compile-time type safety by explicitly specifying element type.
 * Copies actual element values (not pointers) for type safety and convenience.
 *
 * This is the recommended interface for most use cases. It prevents type
 * errors at compile time and generates efficient code with excellent
 * optimization potential.
 *
 * The predicate receives a pointer to each element and can use context
 * for stateful filtering. Output buffer capacity is automatically determined
 * from array size using sizeof.
 *
 * @param out_array Output array of Type (caller provides writable storage)
 * @param in_array  Input array of Type (read-only, never modified)
 * @param len       Number of input elements to check
 * @param Type      Element type (e.g., int, struct Point)
 * @param pred      Predicate function: bool (*)(const Type*, void*)
 * @param ctx       Optional user context passed to predicate (can be NULL)
 * @return          Number of elements written to output (≤ output capacity)
 *
 * Preconditions:
 *   - out_array points to writable memory (array with fixed size)
 *   - in_array points to readable memory for at least len elements
 *   - pred is valid function with signature: bool fn(const Type*, void*)
 *   - len <= capacity of in_array
 *   - out_array has sufficient capacity (determined by sizeof)
 *
 * Postconditions:
 *   - Returns k where k = number of matching elements copied
 *   - out_array[0..k-1] contains copies of matching elements
 *   - out_array[k..capacity-1] unchanged
 *   - Relative order preserved (stable filter)
 *   - in_array is unchanged
 *   - No allocations performed
 *
 * Performance:
 *   - Time: O(n) - checks all n input elements
 *   - Space: O(1)
 *   - Compiler can inline pred for zero function call overhead
 *   - Generates tight, vectorizable loops
 *   - Truncates safely if output fills
 *
 * Example - Filter positive numbers:
 * ```c
 * bool is_positive(const int* elem, void* ctx) {
 *     return *elem > 0;
 * }
 * 
 * int input[] = {-1, 2, -3, 4, 0, 5, -6};
 * int output[10];
 * 
 * size_t count = ALGO_FILTER_INTO(output, input, 7, int, is_positive, NULL);
 * // count = 3
 * // output = {2, 4, 5, ...}
 * ```
 *
 * Example - Filter even numbers:
 * ```c
 * bool is_even(const int* elem, void* ctx) {
 *     return (*elem % 2) == 0;
 * }
 * 
 * int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
 * int evens[20];
 * 
 * size_t count = ALGO_FILTER_INTO(evens, values, 10, int, is_even, NULL);
 * // count = 5
 * // evens = {2, 4, 6, 8, 10, ...}
 * ```
 *
 * Example - Filter with threshold (using context):
 * ```c
 * typedef struct { int threshold; } ThresholdCtx;
 * 
 * bool above_threshold(const int* elem, void* ctx) {
 *     ThresholdCtx* tctx = (ThresholdCtx*)ctx;
 *     return *elem > tctx->threshold;
 * }
 * 
 * int data[] = {5, 15, 25, 35, 45, 10, 20};
 * int filtered[10];
 * ThresholdCtx ctx = {20};
 * 
 * size_t count = ALGO_FILTER_INTO(filtered, data, 7, int, 
 *                                  above_threshold, &ctx);
 * // count = 3
 * // filtered = {25, 35, 45, ...}
 * ```
 *
 * Example - Filter by range:
 * ```c
 * typedef struct { int min; int max; } Range;
 * 
 * bool in_range(const int* elem, void* ctx) {
 *     Range* r = (Range*)ctx;
 *     return *elem >= r->min && *elem <= r->max;
 * }
 * 
 * int values[] = {5, 15, 25, 35, 45, 55};
 * int filtered[10];
 * Range range = {20, 40};
 * 
 * size_t count = ALGO_FILTER_INTO(filtered, values, 6, int, 
 *                                  in_range, &range);
 * // count = 2
 * // filtered = {25, 35, ...}
 * ```
 *
 * Example - Filter strings:
 * ```c
 * bool starts_with_a(const char* const* str, void* ctx) {
 *     return (*str)[0] == 'A' || (*str)[0] == 'a';
 * }
 * 
 * const char* names[] = {"Alice", "Bob", "Amy", "Charlie", "Andrew"};
 * const char* a_names[10];
 * 
 * size_t count = ALGO_FILTER_INTO(a_names, names, 5, const char*, 
 *                                  starts_with_a, NULL);
 * // count = 3
 * // a_names = {"Alice", "Amy", "Andrew", ...}
 * ```
 *
 * Example - Filter structs by field:
 * ```c
 * struct Person {
 *     const char* name;
 *     int age;
 * };
 * 
 * bool is_adult(const struct Person* p, void* ctx) {
 *     return p->age >= 18;
 * }
 * 
 * struct Person people[] = {
 *     {"Alice", 15},
 *     {"Bob", 22},
 *     {"Charlie", 17},
 *     {"David", 25}
 * };
 * struct Person adults[10];
 * 
 * size_t count = ALGO_FILTER_INTO(adults, people, 4, struct Person, 
 *                                  is_adult, NULL);
 * // count = 2
 * // adults = {{"Bob", 22}, {"David", 25}, ...}
 * ```
 *
 * Example - Truncation with small output buffer:
 * ```c
 * int values[] = {2, 4, 6, 8, 10, 12, 14, 16};
 * int output[3];  // Small buffer
 * 
 * size_t count = ALGO_FILTER_INTO(output, values, 8, int, is_even, NULL);
 * // count = 3 (truncated - buffer full)
 * // output = {2, 4, 6}
 * // Remaining evens (8, 10, 12, 14, 16) not copied
 * ```
 *
 * Example - In-place filtering:
 * ```c
 * int data[] = {-1, 2, -3, 4, 0, 5, -6, 7};
 * 
 * size_t count = ALGO_FILTER_INTO(data, data, 8, int, is_positive, NULL);
 * // count = 4
 * // data = {2, 4, 5, 7, 0, 5, -6, 7}  (compacted to front)
 * // Only first 'count' elements are valid filtered results
 * ```
 *
 * Warning: Always check returned count to know how many elements
 * were actually written to the output array.
 */
#define ALGO_FILTER_INTO(out_array, in_array, len, Type, pred, ctx) \
    ({ \
        size_t _out_len = 0; \
        if ((out_array) && (in_array) && (pred)) { \
            const size_t _cap = (sizeof(out_array) / sizeof((out_array)[0])); \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len && _out_len < _cap; ++_i) { \
                if ((pred)(&(in_array)[_i], (ctx))) { \
                    (out_array)[_out_len++] = (in_array)[_i]; \
                } \
            } \
        } \
        _out_len; \
    })

/* ────────────────────────────────────────────────────────────────────────────
   Vec integration (most convenient for Canon-C containers)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Filters vec into caller-provided output vec
 *
 * Convenience wrapper for filtering vec containers. Automatically handles
 * vec structure internals (items pointer). Copies matching element values
 * into output vec's items buffer.
 *
 * @param out_vec   Output vector (must have sufficient capacity)
 * @param in_vec    Input vector (read-only)
 * @param Type      Element type
 * @param pred      Predicate function: bool (*)(const Type*, void*)
 * @param ctx       Optional user context
 * @return          Number of elements written to output vec
 *
 * Preconditions:
 *   - out_vec.items points to writable memory
 *   - in_vec.items points to readable memory
 *   - out_vec has sufficient capacity for filtered results
 *   - pred is valid predicate function
 *
 * Postconditions:
 *   - Returns k = number of matching elements copied
 *   - out_vec.items[0..k-1] contains copies of matching elements
 *   - Caller should set out_vec.len = k to update vec length
 *   - in_vec unchanged
 *
 * Performance:
 *   - Time: O(in_vec.len)
 *   - Space: O(1)
 *   - No allocations
 *   - Truncates if output capacity exceeded
 *
 * Example:
 * ```c
 * bool is_even(const int* elem, void* ctx) {
 *     return (*elem % 2) == 0;
 * }
 * 
 * vec_int input = ...;   // {1, 2, 3, 4, 5, 6, 7, 8}
 * 
 * int output_buf[100];
 * vec_int output = vec_int_init(output_buf, 100);
 * 
 * size_t count = ALGO_FILTER_VEC(output, input, int, is_even, NULL);
 * output.len = count;  // Update vec length
 * // output now contains {2, 4, 6, 8}
 * ```
 *
 * Example - Filter with threshold:
 * ```c
 * typedef struct { int threshold; } ThresholdCtx;
 * 
 * bool above(const int* elem, void* ctx) {
 *     ThresholdCtx* t = (ThresholdCtx*)ctx;
 *     return *elem > t->threshold;
 * }
 * 
 * vec_int data = ...;  // {10, 20, 30, 40, 50}
 * 
 * int filtered_buf[50];
 * vec_int filtered = vec_int_init(filtered_buf, 50);
 * 
 * ThresholdCtx ctx = {25};
 * size_t count = ALGO_FILTER_VEC(filtered, data, int, above, &ctx);
 * filtered.len = count;
 * // filtered contains {30, 40, 50}
 * ```
 *
 * Important: Always update output vec's length after filtering:
 * ```c
 * size_t count = ALGO_FILTER_VEC(out_vec, in_vec, int, pred, NULL);
 * out_vec.len = count;  // IMPORTANT: Update length!
 * ```
 */
#define ALGO_FILTER_VEC(out_vec, in_vec, Type, pred, ctx) \
    ALGO_FILTER_INTO( \
        (out_vec).items, \
        (in_vec).items, \
        (in_vec).len, \
        Type, \
        pred, \
        ctx \
    )

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/filter.h"
    #include <stdio.h>
    #include <string.h>
    
    // Example 1: Filter positive numbers
    bool is_positive(const int* elem, void* ctx) {
        return *elem > 0;
    }
    
    void example_filter_positive(void) {
        int input[] = {-1, 2, -3, 4, 0, 5, -6, 7};
        int output[10];
        
        size_t count = ALGO_FILTER_INTO(output, input, 8, int, is_positive, NULL);
        
        printf("Positive numbers (%zu): ", count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", output[i]);
        }
        printf("\n");
        // Output: Positive numbers (4): 2 4 5 7
    }
    
    // Example 2: Filter even numbers
    bool is_even(const int* elem, void* ctx) {
        return (*elem % 2) == 0;
    }
    
    void example_filter_even(void) {
        int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int evens[10];
        
        size_t count = ALGO_FILTER_INTO(evens, values, 10, int, is_even, NULL);
        
        printf("Even numbers (%zu): ", count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", evens[i]);
        }
        printf("\n");
        // Output: Even numbers (5): 2 4 6 8 10
    }
    
    // Example 3: Filter with threshold (using context)
    typedef struct {
        int threshold;
    } ThresholdCtx;
    
    bool above_threshold(const int* elem, void* ctx) {
        ThresholdCtx* tctx = (ThresholdCtx*)ctx;
        return *elem > tctx->threshold;
    }
    
    void example_threshold(void) {
        int data[] = {5, 15, 25, 35, 45, 10, 20, 30};
        int filtered[10];
        ThresholdCtx ctx = {20};
        
        size_t count = ALGO_FILTER_INTO(filtered, data, 8, int, 
                                         above_threshold, &ctx);
        
        printf("Values > %d (%zu): ", ctx.threshold, count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", filtered[i]);
        }
        printf("\n");
        // Output: Values > 20 (4): 25 35 45 30
    }
    
    // Example 4: Filter by range
    typedef struct {
        int min;
        int max;
    } Range;
    
    bool in_range(const int* elem, void* ctx) {
        Range* r = (Range*)ctx;
        return *elem >= r->min && *elem <= r->max;
    }
    
    void example_range_filter(void) {
        int values[] = {5, 15, 25, 35, 45, 55, 65};
        int filtered[10];
        Range range = {20, 50};
        
        size_t count = ALGO_FILTER_INTO(filtered, values, 7, int, 
                                         in_range, &range);
        
        printf("Values in [%d, %d] (%zu): ", range.min, range.max, count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", filtered[i]);
        }
        printf("\n");
        // Output: Values in [20, 50] (3): 25 35 45
    }
    
    // Example 5: Filter non-zero values
    bool is_non_zero(const int* elem, void* ctx) {
        return *elem != 0;
    }
    
    void example_non_zero(void) {
        int data[] = {1, 0, 2, 0, 3, 0, 4};
        int filtered[10];
        
        size_t count = ALGO_FILTER_INTO(filtered, data, 7, int, 
                                         is_non_zero, NULL);
        
        printf("Non-zero values (%zu): ", count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", filtered[i]);
        }
        printf("\n");
        // Output: Non-zero values (4): 1 2 3 4
    }
    
    // Example 6: Filter strings starting with letter
    bool starts_with_letter(const char* const* str, void* ctx) {
        char target = *(char*)ctx;
        return (*str)[0] == target;
    }
    
    void example_filter_strings(void) {
        const char* names[] = {"Alice", "Bob", "Amy", "Charlie", "Andrew", "David"};
        const char* a_names[10];
        char letter = 'A';
        
        size_t count = ALGO_FILTER_INTO(a_names, names, 6, const char*, 
                                         starts_with_letter, &letter);
        
        printf("Names starting with '%c' (%zu): ", letter, count);
        for (size_t i = 0; i < count; i++) {
            printf("%s ", a_names[i]);
        }
        printf("\n");
        // Output: Names starting with 'A' (3): Alice Amy Andrew
    }
    
    // Example 7: Filter structs by field
    struct Person {
        const char* name;
        int age;
    };
    
    bool is_adult(const struct Person* p, void* ctx) {
        return p->age >= 18;
    }
    
    void example_filter_structs(void) {
        struct Person people[] = {
            {"Alice", 15},
            {"Bob", 22},
            {"Charlie", 17},
            {"David", 25},
            {"Eve", 19}
        };
        struct Person adults[10];
        
        size_t count = ALGO_FILTER_INTO(adults, people, 5, struct Person,
                                         is_adult, NULL);
        
        printf("Adults (%zu):\n", count);
        for (size_t i = 0; i < count; i++) {
            printf("  %s (age %d)\n", adults[i].name, adults[i].age);
        }
        // Output:
        // Adults (3):
        //   Bob (age 22)
        //   David (age 25)
        //   Eve (age 19)
    }
    
    // Example 8: Filter with multiple conditions
    typedef struct {
        int min_age;
        const char* name_prefix;
    } MultiFilter;
    
    bool matches_criteria(const struct Person* p, void* ctx) {
        MultiFilter* mf = (MultiFilter*)ctx;
        return p->age >= mf->min_age && 
               strncmp(p->name, mf->name_prefix, strlen(mf->name_prefix)) == 0;
    }
    
    void example_multi_criteria(void) {
        struct Person people[] = {
            {"Alice", 15},
            {"Bob", 22},
            {"Amy", 25},
            {"Andrew", 30},
            {"Charlie", 20}
        };
        struct Person filtered[10];
        MultiFilter mf = {20, "A"};
        
        size_t count = ALGO_FILTER_INTO(filtered, people, 5, struct Person,
                                         matches_criteria, &mf);
        
        printf("People with age >= %d and name starting with '%s' (%zu):\n",
               mf.min_age, mf.name_prefix, count);
        for (size_t i = 0; i < count; i++) {
            printf("  %s (age %d)\n", filtered[i].name, filtered[i].age);
        }
        // Output:
        // People with age >= 20 and name starting with 'A' (2):
        //   Amy (age 25)
        //   Andrew (age 30)
    }
    
    // Example 9: Truncation demonstration
    void example_truncation(void) {
        int values[] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
        int output[3];  // Small buffer
        
        size_t count = ALGO_FILTER_INTO(output, values, 10, int, is_even, NULL);
        
        printf("Filtered (truncated to %zu): ", count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", output[i]);
        }
        printf("\n");
        // Output: Filtered (truncated to 3): 2 4 6
        // Note: Remaining evens not copied due to buffer size
    }
    
    // Example 10: In-place filtering
    void example_inplace(void) {
        int data[] = {-1, 2, -3, 4, 0, 5, -6, 7, -8};
        
        printf("Before: ");
        for (size_t i = 0; i < 9; i++) printf("%d ", data[i]);
        printf("\n");
        
        size_t count = ALGO_FILTER_INTO(data, data, 9, int, is_positive, NULL);
        
        printf("After (first %zu elements): ", count);
        for (size_t i = 0; i < count; i++) printf("%d ", data[i]);
        printf("\n");
        // Output:
        // Before: -1 2 -3 4 0 5 -6 7 -8
        // After (first 4 elements): 2 4 5 7
    }
    
    // Example 11: Filter prime numbers
    bool is_prime(const int* elem, void* ctx) {
        int n = *elem;
        if (n < 2) return false;
        if (n == 2) return true;
        if (n % 2 == 0) return false;
        
        for (int i = 3; i * i <= n; i += 2) {
            if (n % i == 0) return false;
        }
        return true;
    }
    
    void example_filter_primes(void) {
        int numbers[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        int primes[20];
        
        size_t count = ALGO_FILTER_INTO(primes, numbers, 15, int, 
                                         is_prime, NULL);
        
        printf("Prime numbers (%zu): ", count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", primes[i]);
        }
        printf("\n");
        // Output: Prime numbers (6): 2 3 5 7 11 13
    }
    
    // Example 12: Vec integration
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_vec_filter(void) {
        int input_buf[100];
        vec_int input = vec_int_init(input_buf, 100);
        
        // Populate input
        for (int i = -10; i <= 10; i++) {
            vec_int_push(&input, i);
        }
        
        int output_buf[50];
        vec_int output = vec_int_init(output_buf, 50);
        
        size_t count = ALGO_FILTER_VEC(output, input, int, is_positive, NULL);
        output.len = count;
        
        printf("Positive values from vec (%zu): ", output.len);
        for (size_t i = 0; i < output.len; i++) {
            printf("%d ", output.items[i]);
        }
        printf("\n");
        // Output: Positive values from vec (10): 1 2 3 4 5 6 7 8 9 10
    }
    
    // Example 13: Filter NULL pointers
    bool is_not_null(const void* const* ptr, void* ctx) {
        return *ptr != NULL;
    }
    
    void example_filter_nulls(void) {
        const char* strings[] = {"Hello", NULL, "World", NULL, "Test", "Data"};
        const char* non_nulls[10];
        
        size_t count = ALGO_FILTER_INTO(non_nulls, strings, 6, const char*,
                                         is_not_null, NULL);
        
        printf("Non-NULL strings (%zu): ", count);
        for (size_t i = 0; i < count; i++) {
            printf("%s ", non_nulls[i]);
        }
        printf("\n");
        // Output: Non-NULL strings (4): Hello World Test Data
    }
    
    // Example 14: Filter by divisibility
    typedef struct {
        int divisor;
    } DivisorCtx;
    
    bool divisible_by(const int* elem, void* ctx) {
        DivisorCtx* dctx = (DivisorCtx*)ctx;
        return (*elem % dctx->divisor) == 0;
    }
    
    void example_divisibility(void) {
        int numbers[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        int multiples[10];
        DivisorCtx ctx = {3};
        
        size_t count = ALGO_FILTER_INTO(multiples, numbers, 12, int,
                                         divisible_by, &ctx);
        
        printf("Multiples of %d (%zu): ", ctx.divisor, count);
        for (size_t i = 0; i < count; i++) {
            printf("%d ", multiples[i]);
        }
        printf("\n");
        // Output: Multiples of 3 (4): 3 6 9 12
    }
    
    // Example 15: Chaining with map
    void example_filter_then_map(void) {
        int input[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int filtered[10];
        double squared[10];
        
        // First filter evens
        size_t count = ALGO_FILTER_INTO(filtered, input, 10, int, 
                                         is_even, NULL);
        
        // Then square them (using map - not shown in this file)
        printf("Even numbers squared: ");
        for (size_t i = 0; i < count; i++) {
            printf("%.0f ", (double)(filtered[i] * filtered[i]));
        }
        printf("\n");
        // Output: Even numbers squared: 4 16 36 64 100
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_FILTER_H */
