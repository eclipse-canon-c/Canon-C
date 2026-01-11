// algo/unique.h
#ifndef CANON_ALGO_UNIQUE_H
#define CANON_ALGO_UNIQUE_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include "core/memory.h"
#include "algo/sort.h"  // For algo_cmp_fn typedef

/**
 * @file unique.h
 * @brief Remove consecutive duplicate elements (in-place)
 *
 * Collapses runs of consecutive equal elements into a single occurrence,
 * preserving the order of first appearances.
 * Most powerful when used after sorting (→ full deduplication).
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h)
 *   - Depends on memory.h and sort.h from this library
 *   - Uses statement expressions (GNU extension) in macros
 *   - Define CANON_NO_GNU_EXTENSIONS to disable macro versions
 *
 * Thread-safety: Functions are pure (no shared state) - safe to call from
 *                multiple threads on different data
 *
 * Performance:
 *   - Single linear pass: O(n) time complexity
 *   - In-place algorithm: O(1) space complexity
 *   - Only copies elements when necessary (write < read)
 *   - Very efficient for already-unique or nearly-unique data
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - In-place — zero extra allocation
 * - Single linear pass → O(n) time
 * - Preserves first occurrence of each consecutive group
 * - Generic byte-level implementation (any element type)
 * - Safe: NULL checks, trivial cases (len ≤ 1)
 * - Custom comparator + optional context
 * - Very efficient move semantics (only copies when necessary)
 * - Perfect companion to algo_sort + vec containers
 * - Classic workflow: sort → unique → resize/shrink
 *
 * Important: This removes CONSECUTIVE duplicates only!
 * ────────────────────────────────────────────────────────────────────────────
 * For full deduplication, sort first with the same comparator:
 *   [3,1,2,1,2,3] → (unsorted unique) → [3,1,2,1,2,3] (no change!)
 *   [3,1,2,1,2,3] → (sort) → [1,1,2,2,3,3] → (unique) → [1,2,3] ✓
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Deduplicate sorted arrays / vectors
 * - Clean up measurement series, logs, timestamps
 * - Remove consecutive repeated commands / events
 * - Prepare data for frequency counting or grouping
 * - Reduce size of sorted containers before further processing
 * - Run-length encoding preprocessing
 * - Collapse adjacent identical states in state machines
 *
 * Usage examples:
 *
 * After sorting — full deduplication:
 * ```c
 * int arr[] = {5, 2, 3, 2, 1, 3, 5, 5};
 * algo_sort(arr, 8, sizeof(int), cmp_int, NULL);
 * // arr is now: [1, 2, 2, 3, 3, 5, 5, 5]
 * size_t new_len = algo_unique_consecutive(arr, 8, sizeof(int), cmp_int, NULL);
 * // now arr[0..4] = {1,2,3,5}   new_len == 4
 * ```
 *
 * Vec workflow (most common pattern):
 * ```c
 * vec_string tokens = ...;  // possibly with duplicates
 * algo_sort_vec(&tokens, sizeof(const char*), cmp_string, NULL);
 * size_t new_len = algo_unique_consecutive_vec(&tokens, sizeof(const char*), 
 *                                              cmp_string, NULL);
 * // tokens.len is now updated — no consecutive duplicates remain
 * ```
 *
 * Custom comparator (e.g. case-insensitive):
 * ```c
 * const char* words[] = {"apple", "Apple", "banana", "BANANA", "zebra"};
 * // assume already sorted case-insensitively...
 * size_t unique_count = algo_unique_consecutive(
 *     words, 5, sizeof(const char*),
 *     cmp_string_case_insensitive, NULL);
 * // now: {"apple", "banana", "zebra"}
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic consecutive unique (single-pass, in-place)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Removes consecutive duplicate elements in-place
 *
 * Collapses groups of consecutive equal elements into one.
 * Returns the new logical length of the unique prefix.
 *
 * Algorithm:
 *   - Maintains two pointers: read and write
 *   - Compares each element with the last unique element
 *   - Only copies element if different from previous
 *   - Elements at indices [0, return_value) are unique and consecutive-duplicate-free
 *   - Elements at indices [return_value, len) are invalid (old data)
 *
 * Most effective after sorting with the same comparator.
 *
 * @param array      Pointer to first element (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes (sizeof(Type))
 * @param cmp        Comparator: returns 0 if elements are considered equal
 * @param ctx        Optional context passed to comparator (NULL-safe)
 *
 * @return           New length of the array after removing consecutive duplicates
 *                   Valid elements are now in array[0] .. array[return-1]
 *                   Returns 0 if array is NULL, returns len if len <= 1
 *
 * Preconditions:
 *   - If array != NULL: array points to valid memory of size len * elem_size
 *   - elem_size > 0
 *   - cmp is a valid comparison function
 *
 * Postconditions:
 *   - Array is modified in-place
 *   - First return_value elements contain no consecutive duplicates
 *   - Relative order of first occurrences is preserved
 *   - Original elements beyond return_value are invalid
 *
 * Performance:
 *   - Time: O(n) - single pass through array
 *   - Space: O(1) - in-place algorithm
 *   - Comparisons: At most n-1 comparisons
 *   - Copies: At most n-1 element copies (when all unique)
 *
 * Example:
 *   int arr[] = {1, 1, 2, 2, 2, 3, 4, 4, 5};
 *   size_t new_len = algo_unique_consecutive(arr, 9, sizeof(int), cmp_int, NULL);
 *   // new_len = 5, arr = {1, 2, 3, 4, 5, ...}
 */
static inline size_t algo_unique_consecutive(
    void* array,
    size_t len,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx
) {
    assert(elem_size > 0 && "algo_unique_consecutive: elem_size must be > 0");
    assert(cmp != NULL && "algo_unique_consecutive: cmp function cannot be NULL");
    
    // Handle NULL or trivial cases
    if (!array || len <= 1) {
        return len;
    }
    
    char* bytes = (char*)array;
    size_t write = 1;  // Position to write next unique element
    
    // Single pass: compare each element with the last unique element
    for (size_t read = 1; read < len; ++read) {
        const void* prev = bytes + (write - 1) * elem_size;
        const void* curr = bytes + read * elem_size;
        
        // If current is different from last unique element
        if (cmp(prev, curr, ctx) != 0) {
            // Only copy if we're not already at the write position
            // (optimization: avoid self-copy)
            if (write != read) {
                mem_copy(bytes + write * elem_size, curr, elem_size);
            }
            ++write;
        }
        // If equal, skip (don't increment write)
    }
    
    return write;
}

/**
 * @brief Removes consecutive duplicates from a vector in-place
 *
 * Specialized version for vec-like structures with items and len fields.
 * Automatically updates the vector's length field.
 *
 * @param vec       Pointer to vector structure (NULL-safe)
 * @param elem_size Size of each element in bytes
 * @param cmp       Comparison function
 * @param ctx       Optional context (NULL-safe)
 *
 * @return          New length after deduplication, 0 if vec is NULL
 *
 * Example:
 *   vec_int numbers = ...;
 *   algo_sort_vec(&numbers, sizeof(int), cmp_int, NULL);
 *   size_t new_len = algo_unique_consecutive_vec(&numbers, sizeof(int), 
 *                                                 cmp_int, NULL);
 */
static inline size_t algo_unique_consecutive_vec(
    void* vec_ptr,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx
) {
    if (!vec_ptr) return 0;
    
    // Assume vec has 'items' and 'len' fields at standard offsets
    // This works for vec_T, deque_T, etc.
    typedef struct {
        void* items;
        size_t len;
    } vec_like;
    
    vec_like* vec = (vec_like*)vec_ptr;
    
    if (!vec->items || vec->len == 0) {
        return 0;
    }
    
    size_t new_len = algo_unique_consecutive(
        vec->items,
        vec->len,
        elem_size,
        cmp,
        ctx
    );
    
    vec->len = new_len;
    return new_len;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (require GNU extensions or C23)
   ──────────────────────────────────────────────────────────────────────────── */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Type-safe version for arrays — returns new length
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 *
 * @param array     Array to modify in-place
 * @param len       Original number of elements
 * @param Type      Element type (used for sizeof)
 * @param cmp_expr  Expression that evaluates to comparison function
 * @param ctx       Optional context pointer (can be NULL)
 *
 * @return          New number of elements after deduplication
 *
 * Example:
 *   int data[] = {1, 1, 2, 3, 3, 4};
 *   size_t unique = ALGO_UNIQUE_CONSECUTIVE_TYPED(data, 6, int, cmp_int, NULL);
 *   // unique = 4, data = {1, 2, 3, 4, ...}
 */
#define ALGO_UNIQUE_CONSECUTIVE_TYPED(array, len, Type, cmp_expr, ctx) \
    ({ \
        size_t _new_len = (len); \
        if ((array)) { \
            _new_len = algo_unique_consecutive( \
                (array), (len), sizeof(Type), \
                (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _new_len; \
    })

/**
 * @brief In-place consecutive unique for vec containers
 *
 * Updates .len directly — very convenient pattern:
 *   sort(vec) → unique(vec) → (optional) shrink(vec)
 *
 * This macro automatically detects the vector's item type.
 *
 * @param vec       Vector to process (modified in-place)
 * @param Type      Element type of the vector
 * @param cmp_expr  Comparison function expression
 * @param ctx       Optional context
 *
 * Example:
 *   vec_int numbers = ...;
 *   ALGO_SORT_VEC(numbers, int, cmp_int, NULL);
 *   ALGO_UNIQUE_CONSECUTIVE_VEC(numbers, int, cmp_int, NULL);
 *   // numbers.len is now updated with unique count
 */
#define ALGO_UNIQUE_CONSECUTIVE_VEC(vec, Type, cmp_expr, ctx) \
    do { \
        if ((vec).items) { \
            size_t _new_len = algo_unique_consecutive( \
                (vec).items, (vec).len, sizeof(Type), \
                (algo_cmp_fn)(cmp_expr), (ctx)); \
            (vec).len = _new_len; \
        } \
    } while (0)

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ────────────────────────────────────────────────────────────────────────────
   Helper: Count consecutive duplicates (non-destructive query)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Counts how many elements would remain after unique operation
 *
 * Non-destructive version - does not modify the array.
 * Useful for pre-allocation or progress reporting.
 *
 * @param array      Pointer to first element (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 *
 * @return           Number of unique consecutive elements
 *
 * Performance: O(n) time, O(1) space
 *
 * Example:
 *   int arr[] = {1, 1, 2, 2, 3};
 *   size_t count = algo_count_unique_consecutive(arr, 5, sizeof(int), 
 *                                                 cmp_int, NULL);
 *   // count = 3 (array unchanged)
 */
static inline size_t algo_count_unique_consecutive(
    const void* array,
    size_t len,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx
) {
    assert(elem_size > 0 && "algo_count_unique_consecutive: elem_size must be > 0");
    assert(cmp != NULL && "algo_count_unique_consecutive: cmp function cannot be NULL");
    
    if (!array || len <= 1) {
        return len;
    }
    
    const char* bytes = (const char*)array;
    size_t count = 1;  // First element is always unique
    
    for (size_t i = 1; i < len; ++i) {
        const void* prev = bytes + (i - 1) * elem_size;
        const void* curr = bytes + i * elem_size;
        
        if (cmp(prev, curr, ctx) != 0) {
            ++count;
        }
    }
    
    return count;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/unique.h"
    #include "algo/sort.h"
    #include <stdio.h>
    
    // Example comparison function
    int cmp_int(const void* a, const void* b, void* ctx) {
        int x = *(const int*)a;
        int y = *(const int*)b;
        return (x > y) - (x < y);
    }
    
    // Example 1: Basic deduplication
    void example_basic(void) {
        int arr[] = {1, 1, 2, 2, 2, 3, 4, 4, 5};
        size_t len = 9;
        
        size_t new_len = algo_unique_consecutive(arr, len, sizeof(int), 
                                                  cmp_int, NULL);
        
        printf("Original: 9 elements\n");
        printf("After unique: %zu elements\n", new_len);
        printf("Result: ");
        for (size_t i = 0; i < new_len; i++) {
            printf("%d ", arr[i]);  // 1 2 3 4 5
        }
        printf("\n");
    }
    
    // Example 2: Full deduplication (sort + unique)
    void example_full_dedup(void) {
        int arr[] = {5, 2, 3, 2, 1, 3, 5, 5, 1};
        size_t len = 9;
        
        printf("Original: ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", arr[i]);  // 5 2 3 2 1 3 5 5 1
        }
        printf("\n");
        
        // Step 1: Sort
        algo_sort(arr, len, sizeof(int), cmp_int, NULL);
        printf("Sorted: ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", arr[i]);  // 1 1 2 2 3 3 5 5 5
        }
        printf("\n");
        
        // Step 2: Remove consecutive duplicates
        size_t new_len = algo_unique_consecutive(arr, len, sizeof(int), 
                                                  cmp_int, NULL);
        printf("Unique: ");
        for (size_t i = 0; i < new_len; i++) {
            printf("%d ", arr[i]);  // 1 2 3 5
        }
        printf("\n");
    }
    
    // Example 3: With vectors
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_with_vec(void) {
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        // Add some duplicates
        int data[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
        for (int i = 0; i < 10; i++) {
            vec_int_push(&numbers, data[i]);
        }
        
        printf("Original length: %zu\n", vec_int_len(&numbers));
        
        // Sort + unique workflow
        algo_sort(numbers.items, numbers.len, sizeof(int), cmp_int, NULL);
        size_t new_len = algo_unique_consecutive_vec(&numbers, sizeof(int), 
                                                      cmp_int, NULL);
        
        printf("After sort+unique: %zu\n", new_len);
        // Result: {1, 2, 3, 4, 5, 6, 9}
    }
    
    // Example 4: Count without modifying
    void example_count(void) {
        int arr[] = {1, 1, 2, 2, 2, 3, 4, 4, 5};
        size_t len = 9;
        
        size_t unique_count = algo_count_unique_consecutive(arr, len, 
                                                             sizeof(int), 
                                                             cmp_int, NULL);
        
        printf("Array has %zu unique consecutive elements\n", unique_count);
        printf("Array unchanged: ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", arr[i]);  // Original preserved
        }
        printf("\n");
    }
    
    // Example 5: Custom comparator (case-insensitive strings)
    int cmp_string_ci(const void* a, const void* b, void* ctx) {
        const char* s1 = *(const char**)a;
        const char* s2 = *(const char**)b;
        return strcasecmp(s1, s2);
    }
    
    void example_custom_cmp(void) {
        const char* words[] = {
            "apple", "Apple", "APPLE",
            "banana", "Banana",
            "cherry"
        };
        size_t len = 6;
        
        // Sort case-insensitively
        algo_sort(words, len, sizeof(const char*), cmp_string_ci, NULL);
        
        // Remove consecutive case-insensitive duplicates
        size_t new_len = algo_unique_consecutive(words, len, 
                                                  sizeof(const char*), 
                                                  cmp_string_ci, NULL);
        
        printf("Unique words: ");
        for (size_t i = 0; i < new_len; i++) {
            printf("%s ", words[i]);  // apple banana cherry
        }
        printf("\n");
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_UNIQUE_H */
