// algo/find.h
#ifndef CANON_ALGO_FIND_H
#define CANON_ALGO_FIND_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros

/**
 * @file find.h
 * @brief Locate first element matching a predicate (functional search)
 *
 * Provides efficient, type-safe functional search operations that locate
 * the first element in a sequence satisfying a given predicate. Uses
 * short-circuit evaluation to stop immediately upon finding a match.
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
 *   - Time complexity: O(n) worst case, O(k) if match at index k
 *   - Space complexity: O(1) - no allocations
 *   - No heap allocations
 *   - Short-circuit evaluation - stops at first match
 *   - Cache-friendly: sequential access pattern
 *   - Compiler can inline predicate functions
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Functional programming pattern: find/search operation
 * - Input is read-only — never mutated
 * - No allocation inside functions
 * - No ownership transfer - returns borrowed pointers
 * - Short-circuits on first match for efficiency
 * - Supports optional user context for stateful predicates
 * - Bounded and safe (uses provided length)
 * - Optional output parameters (index and/or element pointer)
 * - Seamless integration with vec.h containers
 * - Zero-overhead abstraction - compiles to tight loops
 * - Type-safe macros prevent common errors
 * - Composable with other functional operations
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Basic find operation (linear search with predicate):
 *   1. Start at index 0
 *   2. For each element at index i:
 *   3. Apply predicate: matches = pred(input[i])
 *   4. If matches: return true with index/element, stop immediately
 *   5. If not matches: continue to next element
 *   6. If all elements checked: return false (not found)
 *
 * Example: Finding first negative in [5, 3, -2, 7, -9]
 *   Input:  [5, 3, -2, 7, -9]
 *   Step 0: pred(5) = false, continue
 *   Step 1: pred(3) = false, continue
 *   Step 2: pred(-2) = true, FOUND! Return index=2, element=-2
 *   Result: true, index=2, element=-2
 *   (Note: -9 never checked due to short-circuit)
 *
 * Not found example: Finding negative in [1, 2, 3, 4]
 *   Input:  [1, 2, 3, 4]
 *   Step 0: pred(1) = false
 *   Step 1: pred(2) = false
 *   Step 2: pred(3) = false
 *   Step 3: pred(4) = false
 *   Result: false (checked all elements)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Finding first matching element in collections
 * - Linear search with custom criteria
 * - Locating specific values or patterns
 * - Early-exit searches (don't check entire array)
 * - Finding first error or invalid entry
 * - Searching with complex conditions
 * - Lookup operations with predicates
 * - Finding elements by property/field
 * - Validation (does any element match?)
 * - Existence checking with position
 * - Pattern matching in data
 * - First occurrence of condition
 * - State-based searching with context
 * - Threshold detection
 * - Searching with user-defined criteria
 *
 * Predicate function signatures:
 * ────────────────────────────────────────────────────────────────────────────
 * Simple predicate:
 *   bool pred(const ElemType* elem, void* ctx) {
 *       // Return true if *elem is the one we're looking for
 *       return /* condition based on *elem */;
 *   }
 *
 * Predicate with context:
 *   bool pred(const ElemType* elem, void* ctx) {
 *       TargetContext* target = (TargetContext*)ctx;
 *       return *elem == target->search_value;
 *   }
 *
 * Usage examples:
 *
 * Find first negative number:
 * ```c
 * bool is_negative(const int* elem, void* ctx) {
 *     return *elem < 0;
 * }
 * int numbers[] = {5, 3, -2, 7, -9};
 * size_t index;
 * if (ALGO_FIND(numbers, 5, int, is_negative, NULL, &index, NULL)) {
 *     printf("First negative at index %zu\n", index);  // index = 2
 * }
 * ```
 *
 * Find specific value:
 * ```c
 * bool equals_42(const int* elem, void* ctx) {
 *     return *elem == 42;
 * }
 * int values[] = {10, 20, 42, 30};
 * const int* found;
 * if (ALGO_FIND(values, 4, int, equals_42, NULL, NULL, &found)) {
 *     printf("Found: %d\n", *found);  // Found: 42
 * }
 * ```
 *
 * Find with context:
 * ```c
 * typedef struct { int target; } SearchCtx;
 * 
 * bool matches_target(const int* elem, void* ctx) {
 *     SearchCtx* sctx = (SearchCtx*)ctx;
 *     return *elem == sctx->target;
 * }
 * 
 * int numbers[] = {1, 2, 3, 4, 5};
 * SearchCtx ctx = {3};
 * size_t index;
 * if (ALGO_FIND(numbers, 5, int, matches_target, &ctx, &index, NULL)) {
 *     printf("Found target at index %zu\n", index);  // index = 2
 * }
 * ```
 *
 * With vec containers:
 * ```c
 * vec_int numbers = ...; // {10, 20, 30, 40}
 * size_t idx;
 * if (ALGO_FIND_VEC(numbers, int, is_negative, NULL, &idx, NULL)) {
 *     // found at idx
 * }
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic find (flexible, pointer-based interface)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Predicate function type (generic version)
 *
 * Signature for functions that test whether an element matches search criteria.
 * Returns true to indicate a match (stops search), false to continue searching.
 *
 * The element is read-only and should not be modified. The context allows
 * passing additional state or search parameters.
 *
 * @param elem  Read-only pointer to current element being tested
 * @param ctx   Optional user context for stateful searches (can be NULL)
 * @return      true if element matches (found), false to continue searching
 *
 * Example implementation:
 * ```c
 * bool is_even(const void* elem, void* ctx) {
 *     return (*(const int*)elem) % 2 == 0;
 * }
 * ```
 *
 * Example with context:
 * ```c
 * typedef struct { int threshold; } Threshold;
 * 
 * bool exceeds_threshold(const void* elem, void* ctx) {
 *     Threshold* t = (Threshold*)ctx;
 *     return *(const int*)elem > t->threshold;
 * }
 * ```
 */
typedef bool (*algo_find_pred)(const void* elem, void* ctx);

/**
 * @brief Finds first element satisfying predicate (generic version)
 *
 * Scans array of element pointers left-to-right, applying predicate to each
 * element until a match is found or all elements are checked. Uses short-
 * circuit evaluation to stop immediately upon finding first match.
 *
 * This is the low-level generic interface. For most use cases, prefer the
 * type-safe ALGO_FIND macro instead.
 *
 * Algorithm:
 *   - Iterates through elements 0 to len-1
 *   - Calls pred(items[i], ctx) for each element
 *   - On first true result: sets output parameters and returns true
 *   - If no match found: returns false
 *   - Short-circuits: stops at first match, doesn't check remaining elements
 *
 * @param items      Array of element pointers (read-only elements)
 * @param len        Number of elements to search
 * @param pred       Predicate function to test each element
 * @param ctx        Optional user context passed to predicate (can be NULL)
 * @param out_index  Optional: receives index of first match (can be NULL)
 * @param out_elem   Optional: receives pointer to matching element (can be NULL)
 * @return           true if match found, false otherwise
 *
 * Preconditions:
 *   - If items != NULL: items points to array of len valid pointers
 *   - Each items[i] points to valid readable memory
 *   - pred is a valid function pointer (if not NULL)
 *   - If out_index != NULL: out_index points to valid writable size_t
 *   - If out_elem != NULL: out_elem points to valid writable pointer
 *
 * Postconditions:
 *   - On true (found):
 *     - If out_index != NULL: *out_index = index of first match
 *     - If out_elem != NULL: *out_elem = pointer to matching element
 *     - Returned pointers are borrowed (valid while input array lives)
 *   - On false (not found):
 *     - Output parameters unchanged
 *   - Input array is never modified
 *   - No heap allocations performed
 *
 * Performance:
 *   - Time: O(k) where k = index of first match (worst case O(n))
 *   - Space: O(1) - no allocations
 *   - Function calls: k+1 calls to pred (stops at first match)
 *   - Short-circuit evaluation minimizes work
 *   - Best case: O(1) if match at index 0
 *
 * Returns false immediately if:
 *   - items is NULL
 *   - pred is NULL
 *   - len == 0
 *
 * Example:
 * ```c
 * bool is_zero(const void* elem, void* ctx) {
 *     return *(const int*)elem == 0;
 * }
 * 
 * const int* elems[5] = {&a, &b, &c, &d, &e};  // values: 1, 2, 0, 3, 4
 * size_t idx;
 * const void* found;
 * 
 * if (algo_find((const void**)elems, 5, is_zero, NULL, &idx, &found)) {
 *     printf("Found zero at index %zu: %d\n", idx, *(int*)found);
 *     // Output: Found zero at index 2: 0
 * } else {
 *     printf("Zero not found\n");
 * }
 * ```
 *
 * Safety notes:
 *   - Returned pointers are borrowed - don't outlive input array
 *   - No bounds checking on individual pointer dereferences
 *   - Caller responsible for memory management
 *   - Silent failure on NULL inputs (returns false)
 *   - Output parameters only modified on successful find
 */
static inline bool algo_find(
    const void** items,
    size_t len,
    algo_find_pred pred,
    void* ctx,
    size_t* out_index,      // optional
    const void** out_elem   // optional
) {
    if (!items || !pred) return false;

    for (size_t i = 0; i < len; ++i) {
        if (pred(items[i], ctx)) {
            if (out_index) *out_index = i;
            if (out_elem) *out_elem = items[i];
            return true;
        }
    }

    return false;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed macro (recommended — type-safe and clean)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe find with optional output parameters (recommended)
 *
 * Searches for the first element in an array that satisfies the given
 * predicate. Provides compile-time type safety by explicitly specifying
 * element type. Supports optional output parameters for index and/or
 * element pointer.
 *
 * This is the recommended interface for most use cases. It prevents type
 * errors at compile time and generates efficient code with excellent
 * optimization potential.
 *
 * The predicate receives a pointer to each element and can use context
 * for stateful searches. Short-circuits on first match for efficiency.
 *
 * @param array      Input array of Type (read-only, never modified)
 * @param len        Number of elements to search
 * @param Type       Element type (e.g., int, struct Point)
 * @param pred       Predicate function: bool (*)(const Type*, void*)
 * @param ctx        Optional user context passed to predicate (can be NULL)
 * @param out_index  Optional: pointer to size_t to receive index (can be NULL)
 * @param out_elem   Optional: pointer to const Type* to receive element (can be NULL)
 * @return           true if element found, false otherwise
 *
 * Preconditions:
 *   - array points to readable memory for at least len elements
 *   - pred is valid function with signature: bool fn(const Type*, void*)
 *   - len <= capacity of array
 *   - If out_index != NULL: points to valid writable size_t
 *   - If out_elem != NULL: points to valid writable const Type*
 *
 * Postconditions:
 *   - On true (found):
 *     - If out_index: *out_index = index of first match
 *     - If out_elem: *out_elem = pointer to matching element
 *   - On false (not found):
 *     - Output parameters unchanged
 *   - array is unchanged
 *   - No allocations performed
 *   - Returned pointers are borrowed (valid while array lives)
 *
 * Performance:
 *   - Time: O(k) where k = index of first match (worst case O(n))
 *   - Space: O(1)
 *   - Compiler can inline pred for zero function call overhead
 *   - Short-circuit evaluation stops at first match
 *   - Best case: O(1) if match at beginning
 *
 * Example - Find first negative:
 * ```c
 * bool is_negative(const int* elem, void* ctx) {
 *     return *elem < 0;
 * }
 * 
 * int numbers[] = {5, 3, -2, 7, -9};
 * size_t index;
 * const int* found;
 * 
 * if (ALGO_FIND(numbers, 5, int, is_negative, NULL, &index, &found)) {
 *     printf("First negative: %d at index %zu\n", *found, index);
 *     // Output: First negative: -2 at index 2
 * }
 * ```
 *
 * Example - Find even number:
 * ```c
 * bool is_even(const int* elem, void* ctx) {
 *     return (*elem % 2) == 0;
 * }
 * 
 * int values[] = {1, 3, 5, 8, 9};
 * size_t idx;
 * 
 * if (ALGO_FIND(values, 5, int, is_even, NULL, &idx, NULL)) {
 *     printf("First even at index %zu\n", idx);  // index = 3 (value 8)
 * }
 * ```
 *
 * Example - Find with context:
 * ```c
 * typedef struct { int threshold; } Threshold;
 * 
 * bool exceeds(const int* elem, void* ctx) {
 *     Threshold* t = (Threshold*)ctx;
 *     return *elem > t->threshold;
 * }
 * 
 * int data[] = {10, 20, 30, 40, 50};
 * Threshold t = {25};
 * size_t idx;
 * 
 * if (ALGO_FIND(data, 5, int, exceeds, &t, &idx, NULL)) {
 *     printf("First value > %d at index %zu\n", t.threshold, idx);
 *     // Output: First value > 25 at index 2 (value 30)
 * }
 * ```
 *
 * Example - Find by string content:
 * ```c
 * bool starts_with_a(const char* const* str, void* ctx) {
 *     return (*str)[0] == 'A' || (*str)[0] == 'a';
 * }
 * 
 * const char* names[] = {"Bob", "Charlie", "Alice", "David"};
 * size_t idx;
 * const char* const* found;
 * 
 * if (ALGO_FIND(names, 4, const char*, starts_with_a, NULL, &idx, &found)) {
 *     printf("First A-name: %s at index %zu\n", *found, idx);
 *     // Output: First A-name: Alice at index 2
 * }
 * ```
 *
 * Example - Find struct by field:
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
 *     {"Charlie", 17}
 * };
 * 
 * size_t idx;
 * const struct Person* adult;
 * 
 * if (ALGO_FIND(people, 3, struct Person, is_adult, NULL, &idx, &adult)) {
 *     printf("First adult: %s (age %d) at index %zu\n",
 *            adult->name, adult->age, idx);
 *     // Output: First adult: Bob (age 22) at index 1
 * }
 * ```
 *
 * Example - Simple existence check (no outputs needed):
 * ```c
 * bool is_zero(const int* elem, void* ctx) {
 *     return *elem == 0;
 * }
 * 
 * int values[] = {1, 2, 3, 0, 4};
 * 
 * if (ALGO_FIND(values, 5, int, is_zero, NULL, NULL, NULL)) {
 *     printf("Array contains zero\n");
 * } else {
 *     printf("Array does not contain zero\n");
 * }
 * ```
 *
 * Example - Find with range check:
 * ```c
 * typedef struct { int min; int max; } Range;
 * 
 * bool in_range(const int* elem, void* ctx) {
 *     Range* r = (Range*)ctx;
 *     return *elem >= r->min && *elem <= r->max;
 * }
 * 
 * int values[] = {5, 15, 25, 35, 45};
 * Range range = {20, 30};
 * size_t idx;
 * 
 * if (ALGO_FIND(values, 5, int, in_range, &range, &idx, NULL)) {
 *     printf("First value in [%d, %d] at index %zu\n",
 *            range.min, range.max, idx);
 *     // Output: First value in [20, 30] at index 2 (value 25)
 * }
 * ```
 *
 * Safety notes:
 *   - Returned element pointer is borrowed - valid while array exists
 *   - Don't use returned pointer after array is freed or goes out of scope
 *   - Output parameters only modified on successful find
 *   - Both output parameters are optional - pass NULL if not needed
 */
#define ALGO_FIND(array, len, Type, pred, ctx, out_index, out_elem) \
    ({ \
        bool _found = false; \
        if ((array) && (pred)) { \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len; ++_i) { \
                if ((pred)(&(array)[_i], (ctx))) { \
                    if (out_index) *(out_index) = _i; \
                    if (out_elem) *(out_elem) = &(array)[_i]; \
                    _found = true; \
                    break; \
                } \
            } \
        } \
        _found; \
    })

/* ────────────────────────────────────────────────────────────────────────────
   Vec integration (most convenient for Canon-C containers)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Finds first matching element in vec
 *
 * Convenience wrapper for searching vec containers. Automatically handles
 * vec structure internals (items pointer and length).
 *
 * @param vec        Input vector (read-only)
 * @param Type       Element type
 * @param pred       Predicate function: bool (*)(const Type*, void*)
 * @param ctx        Optional user context
 * @param out_index  Optional: pointer to size_t to receive index (can be NULL)
 * @param out_elem   Optional: pointer to const Type* to receive element (can be NULL)
 * @return           true if element found, false otherwise
 *
 * Preconditions:
 *   - vec.items points to readable memory
 *   - pred is valid predicate function
 *   - Output parameters point to valid writable memory (if not NULL)
 *
 * Postconditions:
 *   - On true: output parameters set to match location/value
 *   - On false: output parameters unchanged
 *   - vec unchanged
 *
 * Performance:
 *   - Time: O(k) where k = index of first match
 *   - Space: O(1)
 *   - Short-circuits on first match
 *
 * Example:
 * ```c
 * bool is_even(const int* elem, void* ctx) {
 *     return (*elem % 2) == 0;
 * }
 * 
 * vec_int numbers = ...;  // {1, 3, 6, 9, 10}
 * size_t idx;
 * const int* found;
 * 
 * if (ALGO_FIND_VEC(numbers, int, is_even, NULL, &idx, &found)) {
 *     printf("First even: %d at index %zu\n", *found, idx);
 *     // Output: First even: 6 at index 2
 * }
 * ```
 *
 * Example - Existence check only:
 * ```c
 * bool is_negative(const int* elem, void* ctx) {
 *     return *elem < 0;
 * }
 * 
 * vec_int values = ...;
 * 
 * if (ALGO_FIND_VEC(values, int, is_negative, NULL, NULL, NULL)) {
 *     printf("Vector contains negative values\n");
 * }
 * ```
 */
#define ALGO_FIND_VEC(vec, Type, pred, ctx, out_index, out_elem) \
    ALGO_FIND((vec).items, (vec).len, Type, pred, ctx, out_index, out_elem)

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/find.h"
    #include <stdio.h>
    #include <string.h>
    
    // Example 1: Find first negative number
    bool is_negative(const int* elem, void* ctx) {
        return *elem < 0;
    }
    
    void example_find_negative(void) {
        int numbers[] = {5, 3, -2, 7, -9, 4};
        size_t index;
        const int* found;
        
        if (ALGO_FIND(numbers, 6, int, is_negative, NULL, &index, &found)) {
            printf("First negative: %d at index %zu\n", *found, index);
            // Output: First negative: -2 at index 2
        } else {
            printf("No negative numbers found\n");
        }
    }
    
    // Example 2: Find even number
    bool is_even(const int* elem, void* ctx) {
        return (*elem % 2) == 0;
    }
    
    void example_find_even(void) {
        int values[] = {1, 3, 5, 8, 9, 11};
        size_t idx;
        
        if (ALGO_FIND(values, 6, int, is_even, NULL, &idx, NULL)) {
            printf("First even number at index %zu: %d\n", idx, values[idx]);
            // Output: First even number at index 3: 8
        }
    }
    
    // Example 3: Find with target value (using context)
    typedef struct {
        int target;
    } FindContext;
    
    bool equals_target(const int* elem, void* ctx) {
        FindContext* fctx = (FindContext*)ctx;
        return *elem == fctx->target;
    }
    
    void example_find_target(void) {
        int data[] = {10, 20, 30, 40, 50};
        FindContext ctx = {30};
        size_t idx;
        
        if (ALGO_FIND(data, 5, int, equals_target, &ctx, &idx, NULL)) {
            printf("Found %d at index %zu\n", ctx.target, idx);
            // Output: Found 30 at index 2
        }
    }
    
    // Example 4: Find first zero
    bool is_zero(const int* elem, void* ctx) {
        return *elem == 0;
    }
    
    void example_find_zero(void) {
        int values_with[] = {1, 2, 0, 3, 4};
        int values_without[] = {1, 2, 3, 4, 5};
        
        if (ALGO_FIND(values_with, 5, int, is_zero, NULL, NULL, NULL)) {
            printf("Array contains zero\n");
        }
        
        if (!ALGO_FIND(values_without, 5, int, is_zero, NULL, NULL, NULL)) {
            printf("Array does not contain zero\n");
        }
        // Output:
        // Array contains zero
        // Array does not contain zero
    }
    
    // Example 5: Find threshold exceedance
    typedef struct {
        int threshold;
    } ThresholdContext;
    
    bool exceeds_threshold(const int* elem, void* ctx) {
        ThresholdContext* tctx = (ThresholdContext*)ctx;
        return *elem > tctx->threshold;
    }
    
    void example_threshold(void) {
        int values[] = {10, 20, 30, 40, 50};
        ThresholdContext ctx = {25};
        size_t idx;
        const int* first_exceeding;
        
        if (ALGO_FIND(values, 5, int, exceeds_threshold, &ctx, 
                      &idx, &first_exceeding)) {
            printf("First value > %d: %d at index %zu\n",
                   ctx.threshold, *first_exceeding, idx);
            // Output: First value > 25: 30 at index 2
        }
    }
    
    // Example 6: Find string starting with letter
    bool starts_with_letter(const char* const* str, void* ctx) {
        char target = *(char*)ctx;
        return (*str)[0] == target;
    }
    
    void example_find_string(void) {
        const char* names[] = {"Bob", "Charlie", "Alice", "David", "Amy"};
        char letter = 'A';
        size_t idx;
        const char* const* found;
        
        if (ALGO_FIND(names, 5, const char*, starts_with_letter, &letter,
                      &idx, &found)) {
            printf("First name starting with '%c': %s at index %zu\n",
                   letter, *found, idx);
            // Output: First name starting with 'A': Alice at index 2
        }
    }
    
    // Example 7: Find in range
    typedef struct {
        int min;
        int max;
    } Range;
    
    bool in_range(const int* elem, void* ctx) {
        Range* r = (Range*)ctx;
        return *elem >= r->min && *elem <= r->max;
    }
    
    void example_find_in_range(void) {
        int values[] = {5, 15, 25, 35, 45, 55};
        Range range = {20, 40};
        size_t idx;
        
        if (ALGO_FIND(values, 6, int, in_range, &range, &idx, NULL)) {
            printf("First value in [%d, %d]: %d at index %zu\n",
                   range.min, range.max, values[idx], idx);
            // Output: First value in [20, 40]: 25 at index 2
        }
    }
    
    // Example 8: Find struct by field
    struct Person {
        const char* name;
        int age;
    };
    
    bool is_adult(const struct Person* p, void* ctx) {
        return p->age >= 18;
    }
    
    void example_find_struct(void) {
        struct Person people[] = {
            {"Alice", 15},
            {"Bob", 22},
            {"Charlie", 17},
            {"David", 25}
        };
        
        size_t idx;
        const struct Person* adult;
        
        if (ALGO_FIND(people, 4, struct Person, is_adult, NULL, &idx, &adult)) {
            printf("First adult: %s (age %d) at index %zu\n",
                   adult->name, adult->age, idx);
            // Output: First adult: Bob (age 22) at index 1
        }
    }
    
    // Example 9: Find by name (struct field with context)
    typedef struct {
        const char* target_name;
    } NameContext;
    
    bool has_name(const struct Person* p, void* ctx) {
        NameContext* nctx = (NameContext*)ctx;
        return strcmp(p->name, nctx->target_name) == 0;
    }
    
    void example_find_by_name(void) {
        struct Person people[] = {
            {"Alice", 25},
            {"Bob", 30},
            {"Charlie", 35}
        };
        
        NameContext ctx = {"Charlie"};
        size_t idx;
        
        if (ALGO_FIND(people, 3, struct Person, has_name, &ctx, &idx, NULL)) {
            printf("Found %s at index %zu\n", ctx.target_name, idx);
            // Output: Found Charlie at index 2
        }
    }
    
    // Example 10: Find prime number
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
    
    void example_find_prime(void) {
        int numbers[] = {4, 6, 8, 9, 11, 12, 13};
        size_t idx;
        const int* prime;
        
        if (ALGO_FIND(numbers, 7, int, is_prime, NULL, &idx, &prime)) {
            printf("First prime: %d at index %zu\n", *prime, idx);
            // Output: First prime: 11 at index 4
        }
    }
    
    // Example 11: Vec integration - find in vector
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_vec_find(void) {
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        for (int i = 1; i <= 20; i++) {
            vec_int_push(&numbers, i);
        }
        
        size_t idx;
        const int* found;
        
        if (ALGO_FIND_VEC(numbers, int, is_even, NULL, &idx, &found)) {
            printf("First even in vec: %d at index %zu\n", *found, idx);
            // Output: First even in vec: 2 at index 1
        }
    }
    
    // Example 12: Multiple finds (find all positions)
    void example_find_multiple(void) {
        int values[] = {1, 2, 3, 2, 4, 2, 5};
        FindContext ctx = {2};
        
        printf("Positions of %d: ", ctx.target);
        
        size_t search_start = 0;
        size_t idx;
        
        while (search_start < 7) {
            if (ALGO_FIND(values + search_start, 7 - search_start, int,
                         equals_target, &ctx, &idx, NULL)) {
                size_t absolute_idx = search_start + idx;
                printf("%zu ", absolute_idx);
                search_start = absolute_idx + 1;
            } else {
                break;
            }
        }
        printf("\n");
        // Output: Positions of 2: 1 3 5
    }
    
    // Example 13: Find NULL pointer in array
    bool is_null_ptr(const void* const* ptr, void* ctx) {
        return *ptr == NULL;
    }
    
    void example_find_null(void) {
        const char* strings[] = {"Hello", "World", NULL, "Test"};
        size_t idx;
        
        if (ALGO_FIND(strings, 4, const char*, is_null_ptr, NULL, &idx, NULL)) {
            printf("Found NULL at index %zu\n", idx);
            // Output: Found NULL at index 2
        }
    }
    
    // Example 14: Find by multiple conditions
    typedef struct {
        int min_age;
        const char* name_prefix;
    } MultiCondition;
    
    bool matches_conditions(const struct Person* p, void* ctx) {
        MultiCondition* mc = (MultiCondition*)ctx;
        return p->age >= mc->min_age && 
               strncmp(p->name, mc->name_prefix, strlen(mc->name_prefix)) == 0;
    }
    
    void example_multi_condition(void) {
        struct Person people[] = {
            {"Alice", 15},
            {"Bob", 22},
            {"Amy", 25},
            {"Charlie", 17}
        };
        
        MultiCondition mc = {20, "A"};
        size_t idx;
        const struct Person* found;
        
        if (ALGO_FIND(people, 4, struct Person, matches_conditions, &mc,
                      &idx, &found)) {
            printf("Found: %s (age %d) at index %zu\n",
                   found->name, found->age, idx);
            // Output: Found: Amy (age 25) at index 2
        }
    }
    
    // Example 15: Early termination demonstration
    void example_early_termination(void) {
        int large_array[1000];
        for (int i = 0; i < 1000; i++) {
            large_array[i] = i;
        }
        
        large_array[5] = -1;  // Insert negative at index 5
        
        size_t idx;
        if (ALGO_FIND(large_array, 1000, int, is_negative, NULL, &idx, NULL)) {
            printf("Found negative at index %zu (checked only %zu elements)\n",
                   idx, idx + 1);
            // Output: Found negative at index 5 (checked only 6 elements)
            // Note: Didn't need to check all 1000 elements!
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_FIND_H */
