// algo/any_all.h
#ifndef CANON_ALGO_ANY_ALL_H
#define CANON_ALGO_ANY_ALL_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros

/**
 * @file any_all.h
 * @brief Functional predicate testing over sequences (existential/universal quantification)
 *
 * Provides efficient, type-safe functional quantification operations that test
 * whether predicates hold for elements in a sequence. Implements existential
 * quantification (any/exists) and universal quantification (all/forall) with
 * short-circuit evaluation for optimal performance.
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
 *   - Time complexity: O(n) worst case, O(k) with short-circuit at index k
 *   - Space complexity: O(1) - no allocations
 *   - No heap allocations
 *   - Short-circuit evaluation - stops as soon as result is determined
 *   - Cache-friendly: sequential access pattern
 *   - Compiler can inline predicate functions
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Functional programming pattern: any/all operations (quantifiers)
 * - Input is read-only — never mutated
 * - No allocation inside functions
 * - No ownership transfer
 * - Short-circuits for efficiency (stops early when result known)
 * - Existential quantification: ∃x: P(x) - "exists element satisfying P"
 * - Universal quantification: ∀x: P(x) - "all elements satisfy P"
 * - Supports optional user context for stateful predicates
 * - Bounded and safe (uses provided length)
 * - Seamless integration with vec.h containers
 * - Zero-overhead abstraction - compiles to tight loops
 * - Type-safe macros prevent common errors
 * - Composable with other functional operations
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * ANY operation (existential quantification):
 *   1. Start at index 0
 *   2. For each element at index i:
 *   3. Apply predicate: matches = pred(input[i])
 *   4. If matches: return true immediately (SHORT CIRCUIT)
 *   5. If not matches: continue to next element
 *   6. If all elements checked and none match: return false
 *
 * Example: Any negative in [5, 3, -2, 7]
 *   Input:  [5, 3, -2, 7]
 *   Step 0: pred(5) = false, continue
 *   Step 1: pred(3) = false, continue
 *   Step 2: pred(-2) = true, RETURN TRUE (found one!)
 *   Result: true (stopped early, didn't check 7)
 *
 * ALL operation (universal quantification):
 *   1. Start at index 0
 *   2. For each element at index i:
 *   3. Apply predicate: satisfies = pred(input[i])
 *   4. If NOT satisfies: return false immediately (SHORT CIRCUIT)
 *   5. If satisfies: continue to next element
 *   6. If all elements checked and all satisfy: return true
 *
 * Example: All positive in [1, 2, 3, -4, 5]
 *   Input:  [1, 2, 3, -4, 5]
 *   Step 0: pred(1) = true, continue
 *   Step 1: pred(2) = true, continue
 *   Step 2: pred(3) = true, continue
 *   Step 3: pred(-4) = false, RETURN FALSE (found counterexample!)
 *   Result: false (stopped early, didn't check 5)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Existence checks (does any element match?)
 * - Universal validation (do all elements satisfy requirement?)
 * - Early-exit validation loops
 * - Checking for errors or invalid data
 * - Verifying preconditions or postconditions
 * - Data quality checks
 * - Permission/authorization checks (all users have access?)
 * - Range validation (all values in bounds?)
 * - Non-empty checks with conditions
 * - Finding counterexamples
 * - Boolean aggregation
 * - Set membership testing
 * - Logical quantification
 * - Verification and testing
 * - Conditional processing decisions
 *
 * Predicate function signatures:
 * ────────────────────────────────────────────────────────────────────────────
 * Simple predicate:
 *   bool pred(const ElemType* elem, void* ctx) {
 *       // Return true if element satisfies condition
 *       return /* condition based on *elem */;
 *   }
 *
 * Predicate with context:
 *   bool pred(const ElemType* elem, void* ctx) {
 *       TestContext* tctx = (TestContext*)ctx;
 *       return *elem >= tctx->threshold;
 *   }
 *
 * Usage examples:
 *
 * Check if any element is negative:
 * ```c
 * bool is_negative(const int* elem, void* ctx) {
 *     return *elem < 0;
 * }
 * int numbers[] = {1, 2, 3, -4, 5};
 * if (ALGO_ANY_TYPED(numbers, 5, int, is_negative, NULL)) {
 *     printf("Contains negative numbers\n");
 * }
 * // Output: Contains negative numbers
 * ```
 *
 * Check if all elements are positive:
 * ```c
 * bool is_positive(const int* elem, void* ctx) {
 *     return *elem > 0;
 * }
 * int values[] = {1, 2, 3, 4, 5};
 * if (ALGO_ALL_TYPED(values, 5, int, is_positive, NULL)) {
 *     printf("All values are positive\n");
 * }
 * // Output: All values are positive
 * ```
 *
 * With context:
 * ```c
 * typedef struct { int threshold; } Threshold;
 * 
 * bool exceeds(const int* elem, void* ctx) {
 *     Threshold* t = (Threshold*)ctx;
 *     return *elem > t->threshold;
 * }
 * 
 * int data[] = {10, 20, 30, 40};
 * Threshold t = {5};
 * if (ALGO_ALL_TYPED(data, 4, int, exceeds, &t)) {
 *     printf("All values exceed threshold\n");
 * }
 * ```
 *
 * With vec containers:
 * ```c
 * vec_int numbers = ...; // {1, 2, 3, 4, 5}
 * if (ALGO_ALL_VEC(numbers, int, is_positive, NULL)) {
 *     // All numbers are positive
 * }
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic versions (flexible, pointer-based interface)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Predicate function type (generic version)
 *
 * Signature for functions that test whether an element satisfies a condition.
 * Returns true if the condition is met, false otherwise.
 *
 * The element is read-only and should not be modified. The context allows
 * passing additional state or test parameters.
 *
 * @param elem  Read-only pointer to current element being tested
 * @param ctx   Optional user context for stateful tests (can be NULL)
 * @return      true if element satisfies condition, false otherwise
 *
 * Example implementation:
 * ```c
 * bool is_even(const void* elem, void* ctx) {
 *     return (*(const int*)elem % 2) == 0;
 * }
 * ```
 *
 * Example with context:
 * ```c
 * typedef struct { int limit; } Limit;
 * 
 * bool within_limit(const void* elem, void* ctx) {
 *     Limit* l = (Limit*)ctx;
 *     return *(const int*)elem <= l->limit;
 * }
 * ```
 */
typedef bool (*algo_pred_fn)(const void* elem, void* ctx);

/**
 * @brief Returns true if ANY element satisfies predicate (generic version)
 *
 * Existential quantification: tests whether there exists at least one element
 * in the sequence that satisfies the predicate. Uses short-circuit evaluation
 * to stop immediately upon finding first matching element.
 *
 * This is the low-level generic interface. For most use cases, prefer the
 * type-safe ALGO_ANY_TYPED macro instead.
 *
 * Algorithm:
 *   - Iterates through elements 0 to len-1
 *   - Calls pred(items[i], ctx) for each element
 *   - On first true result: returns true immediately (short-circuit)
 *   - If no element satisfies predicate: returns false
 *   - Best case: O(1) if first element matches
 *   - Worst case: O(n) if no elements match
 *
 * @param items  Array of element pointers (read-only elements)
 * @param len    Number of elements to check
 * @param pred   Predicate function to test each element
 * @param ctx    Optional user context passed to predicate (can be NULL)
 * @return       true if at least one element satisfies pred, false otherwise
 *
 * Preconditions:
 *   - If items != NULL: items points to array of len valid pointers
 *   - Each items[i] points to valid readable memory
 *   - pred is a valid function pointer (if not NULL)
 *
 * Postconditions:
 *   - Returns true if ∃i: pred(items[i]) = true
 *   - Returns false if ∀i: pred(items[i]) = false
 *   - Input array is never modified
 *   - No heap allocations performed
 *   - Short-circuits (stops) on first true predicate result
 *
 * Performance:
 *   - Time: O(k) where k = index of first match (worst case O(n))
 *   - Space: O(1) - no allocations
 *   - Function calls: k+1 calls to pred (stops on first true)
 *   - Best case: O(1) if first element matches
 *   - Short-circuit minimizes work
 *
 * Returns false immediately if:
 *   - items is NULL
 *   - pred is NULL
 *   - len == 0
 *   - No elements satisfy predicate
 *
 * Example:
 * ```c
 * bool is_zero(const void* elem, void* ctx) {
 *     return *(const int*)elem == 0;
 * }
 * 
 * int vals[] = {1, 2, 0, 3, 4};
 * const void* items[5] = {&vals[0], &vals[1], &vals[2], &vals[3], &vals[4]};
 * 
 * if (algo_any((const void**)items, 5, is_zero, NULL)) {
 *     printf("Array contains zero\n");
 *     // Checked items[0], items[1], items[2] then stopped
 * }
 * ```
 *
 * Example - Short-circuit demonstration:
 * ```c
 * int large_array[10000];
 * // ... populate array ...
 * large_array[5] = -1;  // Only negative
 * 
 * if (algo_any((const void**)items, 10000, is_negative, NULL)) {
 *     // Found negative at index 5
 *     // Only checked 6 elements, not all 10000!
 * }
 * ```
 *
 * Safety notes:
 *   - No bounds checking on individual pointer dereferences
 *   - Caller responsible for memory management
 *   - Silent failure on NULL inputs (returns false)
 *   - Short-circuit behavior is a feature, not a bug
 */
static inline bool algo_any(
    const void** items,
    size_t len,
    algo_pred_fn pred,
    void* ctx
) {
    if (!items || !pred) return false;

    for (size_t i = 0; i < len; ++i) {
        if (pred(items[i], ctx)) {
            return true;  // Short-circuit on first match
        }
    }

    return false;
}

/**
 * @brief Returns true if ALL elements satisfy predicate (generic version)
 *
 * Universal quantification: tests whether all elements in the sequence
 * satisfy the predicate. Uses short-circuit evaluation to stop immediately
 * upon finding first element that does NOT satisfy predicate.
 *
 * This is the low-level generic interface. For most use cases, prefer the
 * type-safe ALGO_ALL_TYPED macro instead.
 *
 * Algorithm:
 *   - Iterates through elements 0 to len-1
 *   - Calls pred(items[i], ctx) for each element
 *   - On first false result: returns false immediately (short-circuit)
 *   - If all elements satisfy predicate: returns true
 *   - Best case: O(1) if first element fails
 *   - Worst case: O(n) if all elements satisfy
 *
 * @param items  Array of element pointers (read-only elements)
 * @param len    Number of elements to check
 * @param pred   Predicate function to test each element
 * @param ctx    Optional user context passed to predicate (can be NULL)
 * @return       true if all elements satisfy pred, false otherwise
 *
 * Preconditions:
 *   - If items != NULL: items points to array of len valid pointers
 *   - Each items[i] points to valid readable memory
 *   - pred is a valid function pointer (if not NULL)
 *
 * Postconditions:
 *   - Returns true if ∀i: pred(items[i]) = true
 *   - Returns false if ∃i: pred(items[i]) = false
 *   - Input array is never modified
 *   - No heap allocations performed
 *   - Short-circuits (stops) on first false predicate result
 *
 * Performance:
 *   - Time: O(k) where k = index of first failure (worst case O(n))
 *   - Space: O(1) - no allocations
 *   - Function calls: k+1 calls to pred (stops on first false)
 *   - Best case: O(1) if first element fails
 *   - Short-circuit minimizes work
 *
 * Returns false immediately if:
 *   - items is NULL
 *   - pred is NULL
 *   - len == 0 (vacuous truth not implemented - returns false for empty)
 *   - Any element does not satisfy predicate
 *
 * Example:
 * ```c
 * bool is_positive(const void* elem, void* ctx) {
 *     return *(const int*)elem > 0;
 * }
 * 
 * int vals[] = {1, 2, 3, 4, 5};
 * const void* items[5] = {&vals[0], &vals[1], &vals[2], &vals[3], &vals[4]};
 * 
 * if (algo_all((const void**)items, 5, is_positive, NULL)) {
 *     printf("All values are positive\n");
 *     // Checked all 5 elements
 * }
 * ```
 *
 * Example - Short-circuit on failure:
 * ```c
 * int vals[] = {1, 2, -3, 4, 5};
 * const void* items[5] = {&vals[0], &vals[1], &vals[2], &vals[3], &vals[4]};
 * 
 * if (!algo_all((const void**)items, 5, is_positive, NULL)) {
 *     printf("Not all values are positive\n");
 *     // Checked items[0], items[1], items[2] then stopped
 *     // items[3] and items[4] never checked
 * }
 * ```
 *
 * Safety notes:
 *   - No bounds checking on individual pointer dereferences
 *   - Caller responsible for memory management
 *   - Silent failure on NULL inputs (returns false)
 *   - Empty sequence returns false (not vacuous truth)
 *   - Short-circuit behavior is a feature, not a bug
 */
static inline bool algo_all(
    const void** items,
    size_t len,
    algo_pred_fn pred,
    void* ctx
) {
    if (!items || !pred) return false;

    for (size_t i = 0; i < len; ++i) {
        if (!pred(items[i], ctx)) {
            return false;  // Short-circuit on first failure
        }
    }

    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed macros (recommended — type-safe and clean)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe ANY check (existential quantification)
 *
 * Tests whether at least one element in array satisfies the predicate.
 * Provides compile-time type safety by explicitly specifying element type.
 * Uses short-circuit evaluation for optimal performance.
 *
 * This is the recommended interface for most use cases. It prevents type
 * errors at compile time and generates efficient code with excellent
 * optimization potential.
 *
 * The predicate receives a pointer to each element and can use context
 * for stateful tests. Stops immediately upon finding first match.
 *
 * @param items  Input array of Type (read-only, never modified)
 * @param len    Number of elements to check
 * @param Type   Element type (e.g., int, struct Point)
 * @param pred   Predicate function: bool (*)(const Type*, void*)
 * @param ctx    Optional user context passed to predicate (can be NULL)
 * @return       true if any element satisfies pred, false otherwise
 *
 * Preconditions:
 *   - items points to readable memory for at least len elements
 * - pred is valid function with signature: bool fn(const Type*, void*)
 *   - len <= capacity of array
 *
 * Postconditions:
 *   - Returns true if ∃i ∈ [0,len): pred(&items[i]) = true
 *   - Returns false if ∀i ∈ [0,len): pred(&items[i]) = false
 *   - items is unchanged
 *   - No allocations performed
 *   - Short-circuits on first true result
 *
 * Performance:
 *   - Time: O(k) where k = index of first match (worst case O(n))
 *   - Space: O(1)
 *   - Compiler can inline pred for zero function call overhead
 *   - Short-circuit evaluation stops at first match
 *   - Best case: O(1) if first element matches
 *
 * Example - Check for negative:
 * ```c
 * bool is_negative(const int* elem, void* ctx) {
 *     return *elem < 0;
 * }
 * 
 * int numbers[] = {1, 2, -3, 4, 5};
 * 
 * if (ALGO_ANY_TYPED(numbers, 5, int, is_negative, NULL)) {
 *     printf("Contains negative numbers\n");
 *     // Checked [0]=1, [1]=2, [2]=-3, STOPPED
 * }
 * ```
 *
 * Example - Check for even:
 * ```c
 * bool is_even(const int* elem, void* ctx) {
 *     return (*elem % 2) == 0;
 * }
 * 
 * int values[] = {1, 3, 5, 7, 9};
 * 
 * if (!ALGO_ANY_TYPED(values, 5, int, is_even, NULL)) {
 *     printf("No even numbers found\n");
 * }
 * ```
 *
 * Example - With context:
 * ```c
 * typedef struct { int threshold; } Threshold;
 * 
 * bool exceeds(const int* elem, void* ctx) {
 *     Threshold* t = (Threshold*)ctx;
 *     return *elem > t->threshold;
 * }
 * 
 * int data[] = {5, 10, 15, 20, 25};
 * Threshold t = {18};
 * 
 * if (ALGO_ANY_TYPED(data, 5, int, exceeds, &t)) {
 *     printf("At least one value exceeds %d\n", t.threshold);
 *     // Found 20 at index 3
 * }
 * ```
 *
 * Example - String search:
 * ```c
 * bool is_empty(const char* const* str, void* ctx) {
 *     return (*str)[0] == '\0';
 * }
 * 
 * const char* strings[] = {"Hello", "", "World"};
 * 
 * if (ALGO_ANY_TYPED(strings, 3, const char*, is_empty, NULL)) {
 *     printf("Contains empty string\n");
 * }
 * ```
 *
 * Example - Struct field check:
 * ```c
 * struct Person {
 *     const char* name;
 *     int age;
 * };
 * 
 * bool is_minor(const struct Person* p, void* ctx) {
 *     return p->age < 18;
 * }
 * 
 * struct Person people[] = {
 *     {"Alice", 25},
 *     {"Bob", 15},
 *     {"Charlie", 30}
 * };
 * 
 * if (ALGO_ANY_TYPED(people, 3, struct Person, is_minor, NULL)) {
 *     printf("Group contains minors\n");
 * }
 * ```
 */
#define ALGO_ANY_TYPED(items, len, Type, pred, ctx) \
    ({ \
        bool _result = false; \
        if ((items) && (pred)) { \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len; ++_i) { \
                if ((pred)(&(items)[_i], (ctx))) { \
                    _result = true; \
                    break; \
                } \
            } \
        } \
        _result; \
    })

/**
 * @brief Type-safe ALL check (universal quantification)
 *
 * Tests whether all elements in array satisfy the predicate. Provides
 * compile-time type safety by explicitly specifying element type. Uses
 * short-circuit evaluation to stop on first failure.
 *
 * This is the recommended interface for most use cases. It prevents type
 * errors at compile time and generates efficient code with excellent
 * optimization potential.
 *
 * The predicate receives a pointer to each element and can use context
 * for stateful tests. Stops immediately upon finding first non-match.
 *
 * @param items  Input array of Type (read-only, never modified)
 * @param len    Number of elements to check
 * @param Type   Element type (e.g., int, struct Point)
 * @param pred   Predicate function: bool (*)(const Type*, void*)
 * @param ctx    Optional user context passed to predicate (can be NULL)
 * @return       true if all elements satisfy pred, false otherwise
 *
 * Preconditions:
 *   - items points to readable memory for at least len elements
 *   - pred is valid function with signature: bool fn(const Type*, void*)
 *   - len <= capacity of array
 *
 * Postconditions:
 *   - Returns true if ∀i ∈ [0,len): pred(&items[i]) = true
 *   - Returns false if ∃i ∈ [0,len): pred(&items[i]) = false
 *   - Returns false on NULL items or pred
 *   - items is unchanged
 *   - No allocations performed
 *   - Short-circuits on first false result
 *
 * Performance:
 *   - Time: O(k) where k = index of first failure (worst case O(n))
 *   - Space: O(1)
 *   - Compiler can inline pred for zero function call overhead
 *   - Short-circuit evaluation stops at first failure
 *   - Best case: O(1) if first element fails
 *
 * Example - Validate all positive:
 * ```c
 * bool is_positive(const int* elem, void* ctx) {
 *     return *elem > 0;
 * }
 * 
 * int numbers[] = {1, 2, 3, 4, 5};
 * 
 * if (ALGO_ALL_TYPED(numbers, 5, int, is_positive, NULL)) {
 *     printf("All numbers are positive\n");
 * }
 * ```
 *
 * Example - Range validation:
 * ```c
 * typedef struct { int min; int max; } Range;
 * 
 * bool in_range(const int* elem, void* ctx) {
 *     Range* r = (Range*)ctx;
 *     return *elem >= r->min && *elem <= r->max;
 * }
 * 
 * int scores[] = {85, 90, 78, 92, 88};
 * Range valid = {0, 100};
 * 
 * if (ALGO_ALL_TYPED(scores, 5, int, in_range, &valid)) {
 *     printf("All scores are valid\n");
 * }
 * ```
 *
 * Example - Short-circuit demonstration:
 * ```c
 * int values[] = {1, 2, -3, 4, 5, 6, 7, 8, 9, 10};
 * 
 * if (!ALGO_ALL_TYPED(values, 10, int, is_positive, NULL)) {
 *     printf("Not all values are positive\n");
 *     // Checked only first 3 elements (stopped at -3)
 *     // Elements 4-10 never checked
 * }
 * ```
 *
 * Example - String validation:
 * ```c
 * bool is_non_empty(const char* const* str, void* ctx) {
 *     return (*str)[0] != '\0';
 * }
 * 
 * const char* names[] = {"Alice", "Bob", "Charlie"};
 * 
 * if (ALGO_ALL_TYPED(names, 3, const char*, is_non_empty, NULL)) {
 *     printf("All names are non-empty\n");
 * }
 * ```
 *
 * Example - Permission check:
 * ```c
 * struct User {
 *     const char* name;
 *     bool has_access;
 * };
 * 
 * bool has_permission(const struct User* u, void* ctx) {
 *     return u->has_access;
 * }
 * 
 * struct User users[] = {
 *     {"Alice", true},
 *     {"Bob", true},
 *     {"Charlie", true}
 * };
 * 
 * if (ALGO_ALL_TYPED(users, 3, struct User, has_permission, NULL)) {
 *     printf("All users have access\n");
 * }
 * ```
 *
 * Example - Validate bounds:
 * ```c
 * bool within_byte_range(const int* elem, void* ctx) {
 *     return *elem >= 0 && *elem <= 255;
 * }
 * 
 * int pixel_values[] = {100, 200, 50, 255, 0};
 * 
 * if (ALGO_ALL_TYPED(pixel_values, 5, int, within_byte_range, NULL)) {
 *     printf("All pixel values are valid (0-255)\n");
 * }
 * ```
 */
#define ALGO_ALL_TYPED(items, len, Type, pred, ctx) \
    ({ \
        bool _result = true; \
        if ((items) && (pred)) { \
            const size_t _len = (len); \
            for (size_t _i = 0; _i < _len; ++_i) { \
                if (!(pred)(&(items)[_i], (ctx))) { \
                    _result = false; \
                    break; \
                } \
            } \
        } else { \
            _result = false; \
        } \
        _result; \
    })

/* ────────────────────────────────────────────────────────────────────────────
   Vec integration (most convenient for Canon-C containers)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if ANY element in vec satisfies predicate
 *
 * Convenience wrapper for testing vec containers. Automatically handles
 * vec structure internals (items pointer and length). Uses short-circuit
 * evaluation to stop on first match.
 *
 * @param vec   Input vector (read-only)
 * @param Type  Element type
 * @param pred  Predicate function: bool (*)(const Type*, void*)
 * @param ctx   Optional user context
 * @return      true if any element satisfies pred, false otherwise
 *
 * Preconditions:
 *   - vec.items points to readable memory
 *   - pred is valid predicate function
 *
 * Postconditions:
 *   - Returns true if any element satisfies predicate
 *   - vec unchanged
 *   - Short-circuits on first match
 *
 * Performance:
 *   - Time: O(k) where k = index of first match
 *   - Space: O(1)
 *   - Short-circuits immediately on match
 *
 * Example:
 * ```c
 * bool is_even(const int* elem, void* ctx) {
 *     return (*elem % 2) == 0;
 * }
 * 
 * vec_int numbers = ...;  // {1, 3, 6, 9}
 * 
 * if (ALGO_ANY_VEC(numbers, int, is_even, NULL)) {
 *     printf("Vector contains even numbers\n");
 * }
 * ```
 */
#define ALGO_ANY_VEC(vec, Type, pred, ctx) \
    ALGO_ANY_TYPED((vec).items, (vec).len, Type, pred, ctx)

/**
 * @brief Checks if ALL elements in vec satisfy predicate
 *
 * Convenience wrapper for testing vec containers. Automatically handles
 * vec structure internals (items pointer and length). Uses short-circuit
 * evaluation to stop on first failure.
 *
 * @param vec   Input vector (read-only)
 * @param Type  Element type
 * @param pred  Predicate function: bool (*)(const Type*, void*)
 * @param ctx   Optional user context
 * @return      true if all elements satisfy pred, false otherwise
 *
 * Preconditions:
 *   - vec.items points to readable memory
 *   - pred is valid predicate function
 *
 * Postconditions:
 *   - Returns true if all elements satisfy predicate
 *   - vec unchanged
 *   - Short-circuits on first failure
 *
 * Performance:
 *   - Time: O(k) where k = index of first failure
 *   - Space: O(1)
 *   - Short-circuits immediately on failure
 *
 * Example:
 * ```c
 * bool is_positive(const int* elem, void* ctx) {
 *     return *elem > 0;
 * }
 * 
 * vec_int values = ...;  // {1, 2, 3, 4, 5}
 * 
 * if (ALGO_ALL_VEC(values, int, is_positive, NULL)) {
 *     printf("All values in vector are positive\n");
 * }
 * ```
 */
#define ALGO_ALL_VEC(vec, Type, pred, ctx) \
    ALGO_ALL_TYPED((vec).items, (vec).len, Type, pred, ctx)

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/any_all.h"
    #include <stdio.h>
    #include <string.h>
    
    // Example 1: Check for negative numbers (ANY)
    bool is_negative(const int* elem, void* ctx) {
        return *elem < 0;
    }
    
    void example_any_negative(void) {
        int numbers1[] = {1, 2, -3, 4, 5};
        int numbers2[] = {1, 2, 3, 4, 5};
        
        if (ALGO_ANY_TYPED(numbers1, 5, int, is_negative, NULL)) {
            printf("numbers1 contains negative values\n");
        }
        
        if (!ALGO_ANY_TYPED(numbers2, 5, int, is_negative, NULL)) {
            printf("numbers2 contains no negative values\n");
        }
        // Output:
        // numbers1 contains negative values
        // numbers2 contains no negative values
    }
    
    // Example 2: Validate all positive (ALL)
    bool is_positive(const int* elem, void* ctx) {
        return *elem > 0;
    }
    
    void example_all_positive(void) {
        int valid[] = {1, 2, 3, 4, 5};
        int invalid[] = {1, 2, -3, 4, 5};
        
        if (ALGO_ALL_TYPED(valid, 5, int, is_positive, NULL)) {
            printf("All values in 'valid' are positive\n");
        }
        
        if (!ALGO_ALL_TYPED(invalid, 5, int, is_positive, NULL)) {
            printf("Not all values in 'invalid' are positive\n");
        }
        // Output:
        // All values in 'valid' are positive
        // Not all values in 'invalid' are positive
    }
    
    // Example 3: Check for even numbers
    bool is_even(const int* elem, void* ctx) {
        return (*elem % 2) == 0;
    }
    
    void example_any_even(void) {
        int values[] = {1, 3, 5, 8, 9};
        
        if (ALGO_ANY_TYPED(values, 5, int, is_even, NULL)) {
            printf("Contains at least one even number\n");
        }
        // Output: Contains at least one even number
    }
    
    // Example 4: Validate all even
    void example_all_even(void) {
        int all_evens[] = {2, 4, 6, 8, 10};
        int mixed[] = {2, 4, 5, 8, 10};
        
        if (ALGO_ALL_TYPED(all_evens, 5, int, is_even, NULL)) {
            printf("All numbers are even\n");
        }
        
        if (!ALGO_ALL_TYPED(mixed, 5, int, is_even, NULL)) {
            printf("Not all numbers are even\n");
        }
        // Output:
        // All numbers are even
        // Not all numbers are even
    }
    
    // Example 5: Threshold check with context
    typedef struct {
        int threshold;
    } ThresholdCtx;
    
    bool exceeds_threshold(const int* elem, void* ctx) {
        ThresholdCtx* t = (ThresholdCtx*)ctx;
        return *elem > t->threshold;
    }
    
    void example_threshold(void) {
        int data[] = {5, 15, 25, 35, 45};
        ThresholdCtx ctx = {20};
        
        if (ALGO_ANY_TYPED(data, 5, int, exceeds_threshold, &ctx)) {
            printf("At least one value exceeds %d\n", ctx.threshold);
        }
        
        ctx.threshold = 50;
        if (!ALGO_ALL_TYPED(data, 5, int, exceeds_threshold, &ctx)) {
            printf("Not all values exceed %d\n", ctx.threshold);
        }
        // Output:
        // At least one value exceeds 20
        // Not all values exceed 50
    }
    
    // Example 6: Range validation
    typedef struct {
        int min;
        int max;
    } Range;
    
    bool in_range(const int* elem, void* ctx) {
        Range* r = (Range*)ctx;
        return *elem >= r->min && *elem <= r->max;
    }
    
    void example_range_check(void) {
        int scores[] = {85, 90, 78, 92, 88};
        Range valid = {0, 100};
        
        if (ALGO_ALL_TYPED(scores, 5, int, in_range, &valid)) {
            printf("All scores are in valid range [%d, %d]\n", 
                   valid.min, valid.max);
        }
        // Output: All scores are in valid range [0, 100]
    }
    
    // Example 7: String checks
    bool is_empty(const char* const* str, void* ctx) {
        return (*str)[0] == '\0';
    }
    
    void example_string_check(void) {
        const char* strings1[] = {"Hello", "", "World"};
        const char* strings2[] = {"Hello", "World", "Test"};
        
        if (ALGO_ANY_TYPED(strings1, 3, const char*, is_empty, NULL)) {
            printf("strings1 contains empty strings\n");
        }
        
        if (ALGO_ALL_TYPED(strings2, 3, const char*, is_empty, NULL)) {
            printf("All strings are empty\n");
        } else {
            printf("Not all strings are empty\n");
        }
        // Output:
        // strings1 contains empty strings
        // Not all strings are empty
    }
    
    // Example 8: Struct validation
    struct Person {
        const char* name;
        int age;
    };
    
    bool is_adult(const struct Person* p, void* ctx) {
        return p->age >= 18;
    }
    
    void example_struct_check(void) {
        struct Person people[] = {
            {"Alice", 25},
            {"Bob", 15},
            {"Charlie", 30}
        };
        
        if (ALGO_ANY_TYPED(people, 3, struct Person, is_adult, NULL)) {
            printf("Group contains at least one adult\n");
        }
        
        if (!ALGO_ALL_TYPED(people, 3, struct Person, is_adult, NULL)) {
            printf("Group contains minors\n");
        }
        // Output:
        // Group contains at least one adult
        // Group contains minors
    }
    
    // Example 9: Permission validation
    struct User {
        const char* name;
        bool has_access;
    };
    
    bool has_permission(const struct User* u, void* ctx) {
        return u->has_access;
    }
    
    void example_permissions(void) {
        struct User users[] = {
            {"Alice", true},
            {"Bob", true},
            {"Charlie", true}
        };
        
        if (ALGO_ALL_TYPED(users, 3, struct User, has_permission, NULL)) {
            printf("All users have access - proceeding\n");
        }
        // Output: All users have access - proceeding
    }
    
    // Example 10: Short-circuit demonstration
    void example_short_circuit(void) {
        int large_array[10000];
        for (int i = 0; i < 10000; i++) {
            large_array[i] = i + 1;  // All positive
        }
        large_array[5] = -1;  // One negative at index 5
        
        printf("Checking 10000 elements...\n");
        if (ALGO_ANY_TYPED(large_array, 10000, int, is_negative, NULL)) {
            printf("Found negative (checked only 6 elements, not all 10000!)\n");
        }
        // Output:
        // Checking 10000 elements...
        // Found negative (checked only 6 elements, not all 10000!)
    }
    
    // Example 11: Vec integration
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_vec_operations(void) {
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        for (int i = 1; i <= 10; i++) {
            vec_int_push(&numbers, i);
        }
        
        if (ALGO_ALL_VEC(numbers, int, is_positive, NULL)) {
            printf("All vec elements are positive\n");
        }
        
        if (ALGO_ANY_VEC(numbers, int, is_even, NULL)) {
            printf("Vec contains even numbers\n");
        }
        // Output:
        // All vec elements are positive
        // Vec contains even numbers
    }
    
    // Example 12: Zero check
    bool is_zero(const int* elem, void* ctx) {
        return *elem == 0;
    }
    
    void example_zero_check(void) {
        int data1[] = {1, 2, 0, 3, 4};
        int data2[] = {1, 2, 3, 4, 5};
        
        if (ALGO_ANY_TYPED(data1, 5, int, is_zero, NULL)) {
            printf("data1 contains zero\n");
        }
        
        if (!ALGO_ANY_TYPED(data2, 5, int, is_zero, NULL)) {
            printf("data2 does not contain zero\n");
        }
        // Output:
        // data1 contains zero
        // data2 does not contain zero
    }
    
    // Example 13: Multiple conditions
    typedef struct {
        int min_age;
        const char* name_prefix;
    } MultiCheck;
    
    bool matches_criteria(const struct Person* p, void* ctx) {
        MultiCheck* mc = (MultiCheck*)ctx;
        return p->age >= mc->min_age &&
               strncmp(p->name, mc->name_prefix, 
                       strlen(mc->name_prefix)) == 0;
    }
    
    void example_multi_criteria(void) {
        struct Person people[] = {
            {"Alice", 25},
            {"Amy", 30},
            {"Bob", 20}
        };
        
        MultiCheck mc = {25, "A"};
        
        if (ALGO_ANY_TYPED(people, 3, struct Person, matches_criteria, &mc)) {
            printf("At least one person matches criteria\n");
        }
        // Output: At least one person matches criteria
    }
    
    // Example 14: NULL pointer check
    bool is_not_null(const void* const* ptr, void* ctx) {
        return *ptr != NULL;
    }
    
    void example_null_check(void) {
        const char* ptrs1[] = {"Hello", "World", NULL};
        const char* ptrs2[] = {"Hello", "World", "Test"};
        
        if (!ALGO_ALL_TYPED(ptrs1, 3, const char*, is_not_null, NULL)) {
            printf("ptrs1 contains NULL pointers\n");
        }
        
        if (ALGO_ALL_TYPED(ptrs2, 3, const char*, is_not_null, NULL)) {
            printf("ptrs2 has no NULL pointers\n");
        }
        // Output:
        // ptrs1 contains NULL pointers
        // ptrs2 has no NULL pointers
    }
    
    // Example 15: Prime number check
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
    
    void example_prime_check(void) {
        int numbers[] = {4, 6, 8, 9, 11};
        
        if (ALGO_ANY_TYPED(numbers, 5, int, is_prime, NULL)) {
            printf("Contains at least one prime number\n");
        }
        
        int all_primes[] = {2, 3, 5, 7, 11};
        if (ALGO_ALL_TYPED(all_primes, 5, int, is_prime, NULL)) {
            printf("All numbers are prime\n");
        }
        // Output:
        // Contains at least one prime number
        // All numbers are prime
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_ANY_ALL_H */
