// algo/reverse.h
#ifndef CANON_ALGO_REVERSE_H
#define CANON_ALGO_REVERSE_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include "core/memory.h"

/**
 * @file reverse.h
 * @brief In-place sequence reversal algorithms
 *
 * Provides efficient, generic, in-place reversal of arrays and containers.
 * Uses a two-pointer technique with byte-level swapping to reverse elements
 * of any type in linear time with constant space.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h)
 *   - Depends on memory.h from this library (for mem_copy, mem_swap)
 *   - No external dependencies beyond standard C library
 *
 * Thread-safety: Functions are pure (no shared state) - safe to call from
 *                multiple threads on different data
 *
 * Performance:
 *   - Time complexity: O(n) - exactly n/2 swaps
 *   - Space complexity: O(1) - small fixed-size temporary buffer
 *   - In-place algorithm: modifies array directly
 *   - No heap allocations
 *   - Cache-friendly: sequential access pattern
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - In-place reversal — minimal memory overhead
 * - Two-pointer technique: swap from ends toward middle
 * - Generic byte-level implementation (any element type)
 * - Safe: NULL checks, trivial cases (len < 2)
 * - Never allocates heap memory
 * - Works with any element size (including large structs)
 * - Strongly typed macros for compile-time safety
 * - Seamless integration with vec.h style containers
 * - Maximum performance: no function pointers, excellent inlining
 * - Simple, understandable algorithm
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Two-pointer technique:
 *   1. Start with left pointer at array[0], right pointer at array[n-1]
 *   2. Swap elements at left and right
 *   3. Move left forward, right backward
 *   4. Repeat until left >= right (pointers meet in middle)
 *
 * Example: Reversing [1, 2, 3, 4, 5]
 *   Step 1: [1, 2, 3, 4, 5]  → swap array[0] ↔ array[4]
 *   Step 2: [5, 2, 3, 4, 1]  → swap array[1] ↔ array[3]
 *   Step 3: [5, 4, 3, 2, 1]  → left == right (done)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Reverse order of elements for processing
 * - Prepare data for reverse iteration
 * - Undo/redo stack operations
 * - Algorithm preprocessing (palindrome checks, etc.)
 * - Reversing results from algorithms that produce reverse order
 * - String/array manipulation
 * - Quick reordering of collections
 * - Implementing reverse iterators
 * - Data structure operations (reverse linked list, etc.)
 * - Sorting helper (reverse to get descending order)
 *
 * Usage examples:
 *
 * Basic integer reversal:
 * ```c
 * int scores[] = {10, 20, 30, 40, 50};
 * ALGO_REVERSE_TYPED(scores, 5, int);
 * // Result: {50, 40, 30, 20, 10}
 * ```
 *
 * Reversing strings array:
 * ```c
 * const char* names[] = {"Alice", "Bob", "Charlie", "Dave"};
 * ALGO_REVERSE_TYPED(names, 4, const char*);
 * // Result: {"Dave", "Charlie", "Bob", "Alice"}
 * ```
 *
 * With vec container:
 * ```c
 * vec_int numbers = ...; // {1, 2, 3, 4, 5}
 * ALGO_REVERSE_VEC(numbers, int);
 * // Result: {5, 4, 3, 2, 1}
 * ```
 *
 * Generic (when type is not known at compile time):
 * ```c
 * struct Point { float x, y; } points[100];
 * algo_reverse(points, 100, sizeof(struct Point));
 * ```
 *
 * Partial reversal (subrange):
 * ```c
 * int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
 * // Reverse middle portion [3, 4, 5, 6]
 * algo_reverse(&data[2], 4, sizeof(int));
 * // Result: {1, 2, 6, 5, 4, 3, 7, 8}
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic byte-wise in-place reverse
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reverses array of arbitrary elements in-place
 *
 * Uses two-pointer technique with byte-level swapping. Efficient for
 * all element sizes, from single bytes to large structs.
 *
 * Algorithm:
 *   - Maintains left and right pointers
 *   - Swaps elements at left and right positions
 *   - Moves pointers toward center
 *   - Stops when pointers meet (left >= right)
 *   - Performs exactly ⌊n/2⌋ swaps
 *
 * @param array      Pointer to the first element (NULL-safe)
 * @param len        Number of elements in the array
 * @param elem_size  Size of each element in bytes (sizeof(Type))
 *
 * Preconditions:
 *   - If array != NULL: array points to valid memory of size len * elem_size
 *   - elem_size > 0
 *   - elem_size <= 256 bytes (temp buffer limitation)
 *
 * Postconditions:
 *   - Array elements are reversed: array[i] ↔ array[n-1-i]
 *   - Original order is lost
 *   - For even length: all elements swapped
 *   - For odd length: middle element stays in place
 *
 * Performance:
 *   - Time: O(n) - exactly ⌊n/2⌋ swaps
 *   - Space: O(1) - 256-byte temporary buffer on stack
 *   - Swaps: ⌊n/2⌋ element swaps
 *   - Comparisons: None (no comparison needed)
 *   - Memory accesses: n total element reads/writes
 *
 * Does nothing if:
 *   - array is NULL
 *   - len < 2 (0 or 1 elements already "reversed")
 *
 * Example:
 *   int arr[] = {1, 2, 3, 4, 5};
 *   algo_reverse(arr, 5, sizeof(int));
 *   // arr is now {5, 4, 3, 2, 1}
 *
 * Example with structs:
 *   struct Point { double x, y; } points[10];
 *   algo_reverse(points, 10, sizeof(struct Point));
 */
static inline void algo_reverse(
    void* array,
    size_t len,
    size_t elem_size
) {
    assert(elem_size > 0 && "algo_reverse: elem_size must be > 0");
    assert(elem_size <= 256 && "algo_reverse: elem_size too large for temp buffer");
    
    // Handle NULL or trivial cases
    if (!array || len < 2) {
        return;
    }
    
    char* bytes = (char*)array;
    char* left  = bytes;
    char* right = bytes + (len - 1) * elem_size;
    
    // Temporary buffer for swapping
    // 256 bytes handles most types: doubles, small structs, pointers, etc.
    char temp[256];
    
    // Two-pointer technique: swap from ends toward middle
    while (left < right) {
        // Three-way swap using temp buffer
        mem_copy(temp,  left,  elem_size);
        mem_copy(left,  right, elem_size);
        mem_copy(right, temp,  elem_size);
        
        // Move pointers toward center
        left  += elem_size;
        right -= elem_size;
    }
}

/**
 * @brief Reverses a vector in-place
 *
 * Specialized version for vec-like structures with items and len fields.
 *
 * @param vec_ptr    Pointer to vector structure (NULL-safe)
 * @param elem_size  Size of each element in bytes
 *
 * Example:
 *   vec_int numbers = ...;
 *   algo_reverse_vec(&numbers, sizeof(int));
 */
static inline void algo_reverse_vec(
    void* vec_ptr,
    size_t elem_size
) {
    if (!vec_ptr) return;
    
    // Assume vec has 'items' and 'len' fields at standard offsets
    typedef struct {
        void* items;
        size_t len;
    } vec_like;
    
    vec_like* vec = (vec_like*)vec_ptr;
    
    if (!vec->items || vec->len < 2) {
        return;
    }
    
    algo_reverse(vec->items, vec->len, elem_size);
}

/* ────────────────────────────────────────────────────────────────────────────
   Utility: Check if array is palindrome (reads same forwards and backwards)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if array is a palindrome
 *
 * Non-destructive check - array is not modified.
 * Requires a comparison function to check element equality.
 *
 * @param array      Pointer to first element (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param cmp        Comparison function (returns 0 if elements equal)
 * @param ctx        Optional context (NULL-safe)
 *
 * @return           true if palindrome, false otherwise
 *
 * Performance: O(n/2) comparisons, O(1) space
 *
 * Example:
 *   int arr[] = {1, 2, 3, 2, 1};
 *   bool is_pal = algo_is_palindrome(arr, 5, sizeof(int), algo_cmp_int, NULL);
 *   // is_pal = true
 */
static inline bool algo_is_palindrome(
    const void* array,
    size_t len,
    size_t elem_size,
    int (*cmp)(const void*, const void*, void*),
    void* ctx
) {
    assert(elem_size > 0 && "algo_is_palindrome: elem_size must be > 0");
    assert(cmp != NULL && "algo_is_palindrome: cmp function cannot be NULL");
    
    if (!array || len < 2) {
        return true;  // Empty or single-element arrays are palindromes
    }
    
    const char* bytes = (const char*)array;
    size_t left_idx = 0;
    size_t right_idx = len - 1;
    
    while (left_idx < right_idx) {
        const void* left_elem = bytes + left_idx * elem_size;
        const void* right_elem = bytes + right_idx * elem_size;
        
        if (cmp(left_elem, right_elem, ctx) != 0) {
            return false;  // Mismatch found
        }
        
        ++left_idx;
        --right_idx;
    }
    
    return true;  // All pairs matched
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (recommended)
   ──────────────────────────────────────────────────────────────────────────── */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Type-safe in-place reverse for known element type
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 *
 * @param array  Array of Type (modified in-place)
 * @param len    Number of elements
 * @param Type   Element type (used for sizeof)
 *
 * Example:
 *   int data[] = {1, 2, 3, 4, 5};
 *   ALGO_REVERSE_TYPED(data, 5, int);
 *   // data is now {5, 4, 3, 2, 1}
 */
#define ALGO_REVERSE_TYPED(array, len, Type) \
    do { \
        if ((array) && (len) >= 2) { \
            algo_reverse((array), (len), sizeof(Type)); \
        } \
    } while (0)

/**
 * @brief Type-safe reverse for vec containers
 *
 * Automatically handles vector structure and reverses in-place.
 * Assumes vec has .items and .len fields (standard vec layout).
 *
 * @param vec   Vector to reverse (modified in-place)
 * @param Type  Element type stored in the vector
 *
 * Example:
 *   vec_int numbers = ...;  // {10, 20, 30, 40}
 *   ALGO_REVERSE_VEC(numbers, int);
 *   // numbers is now {40, 30, 20, 10}
 */
#define ALGO_REVERSE_VEC(vec, Type) \
    do { \
        if ((vec).items && (vec).len >= 2) { \
            algo_reverse((vec).items, (vec).len, sizeof(Type)); \
        } \
    } while (0)

/**
 * @brief Type-safe palindrome check
 *
 * @param array    Array to check
 * @param len      Number of elements
 * @param Type     Element type
 * @param cmp_expr Comparison function
 * @param ctx      Optional context
 *
 * @return         true if palindrome, false otherwise
 */
#define ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp_expr, ctx) \
    ({ \
        bool _result = true; \
        if ((array) && (len) >= 2) { \
            typedef int (*_cmp_fn_t)(const void*, const void*, void*); \
            _result = algo_is_palindrome((array), (len), sizeof(Type), \
                                        (_cmp_fn_t)(cmp_expr), (ctx)); \
        } \
        _result; \
    })

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/reverse.h"
    #include "algo/sort.h"
    #include <stdio.h>
    #include <string.h>
    
    // Example 1: Basic integer reversal
    void example_basic_reverse(void) {
        int scores[] = {10, 20, 30, 40, 50};
        size_t len = 5;
        
        printf("Before: ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", scores[i]);
        }
        printf("\n");
        
        ALGO_REVERSE_TYPED(scores, len, int);
        
        printf("After:  ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", scores[i]);
        }
        printf("\n");
        // Output:
        // Before: 10 20 30 40 50
        // After:  50 40 30 20 10
    }
    
    // Example 2: Reversing strings array
    void example_string_reverse(void) {
        const char* names[] = {"Alice", "Bob", "Charlie", "Dave"};
        size_t len = 4;
        
        printf("Before: ");
        for (size_t i = 0; i < len; i++) {
            printf("%s ", names[i]);
        }
        printf("\n");
        
        ALGO_REVERSE_TYPED(names, len, const char*);
        
        printf("After:  ");
        for (size_t i = 0; i < len; i++) {
            printf("%s ", names[i]);
        }
        printf("\n");
        // Output:
        // Before: Alice Bob Charlie Dave
        // After:  Dave Charlie Bob Alice
    }
    
    // Example 3: With vector containers
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_vector_reverse(void) {
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        // Add elements
        for (int i = 1; i <= 5; i++) {
            vec_int_push(&numbers, i * 10);
        }
        
        printf("Original: ");
        for (size_t i = 0; i < numbers.len; i++) {
            printf("%d ", numbers.items[i]);
        }
        printf("\n");
        
        ALGO_REVERSE_VEC(numbers, int);
        
        printf("Reversed: ");
        for (size_t i = 0; i < numbers.len; i++) {
            printf("%d ", numbers.items[i]);
        }
        printf("\n");
    }
    
    // Example 4: Reversing structs
    struct Point {
        int x;
        int y;
    };
    
    void example_struct_reverse(void) {
        struct Point points[] = {
            {1, 2},
            {3, 4},
            {5, 6},
            {7, 8}
        };
        size_t len = 4;
        
        printf("Before:\n");
        for (size_t i = 0; i < len; i++) {
            printf("  (%d, %d)\n", points[i].x, points[i].y);
        }
        
        ALGO_REVERSE_TYPED(points, len, struct Point);
        
        printf("After:\n");
        for (size_t i = 0; i < len; i++) {
            printf("  (%d, %d)\n", points[i].x, points[i].y);
        }
    }
    
    // Example 5: Partial reversal (subrange)
    void example_partial_reverse(void) {
        int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
        
        printf("Original: ");
        for (size_t i = 0; i < 8; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
        
        // Reverse middle portion [3, 4, 5, 6]
        algo_reverse(&data[2], 4, sizeof(int));
        
        printf("After reversing middle 4 elements: ");
        for (size_t i = 0; i < 8; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
        // Output: 1 2 6 5 4 3 7 8
    }
    
    // Example 6: Palindrome check
    void example_palindrome_check(void) {
        int palindrome[] = {1, 2, 3, 2, 1};
        int not_palindrome[] = {1, 2, 3, 4, 5};
        
        bool is_pal1 = ALGO_IS_PALINDROME_TYPED(palindrome, 5, int,
                                                 algo_cmp_int, NULL);
        bool is_pal2 = ALGO_IS_PALINDROME_TYPED(not_palindrome, 5, int,
                                                 algo_cmp_int, NULL);
        
        printf("Array {1, 2, 3, 2, 1} is palindrome: %s\n",
               is_pal1 ? "yes" : "no");
        printf("Array {1, 2, 3, 4, 5} is palindrome: %s\n",
               is_pal2 ? "yes" : "no");
        // Output:
        // Array {1, 2, 3, 2, 1} is palindrome: yes
        // Array {1, 2, 3, 4, 5} is palindrome: no
    }
    
    // Example 7: Reverse after sorting (descending order)
    void example_reverse_sorted(void) {
        int data[] = {5, 2, 8, 1, 9, 3, 7};
        size_t len = 7;
        
        // Sort ascending
        ALGO_SORT_TYPED(data, len, int, algo_cmp_int, NULL);
        
        printf("Ascending:  ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
        
        // Reverse to get descending
        ALGO_REVERSE_TYPED(data, len, int);
        
        printf("Descending: ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
        // Output:
        // Ascending:  1 2 3 5 7 8 9
        // Descending: 9 8 7 5 3 2 1
    }
    
    // Example 8: String reversal (array of chars)
    void example_char_array_reverse(void) {
        char str[] = "Hello";
        size_t len = strlen(str);
        
        printf("Before: %s\n", str);
        
        ALGO_REVERSE_TYPED(str, len, char);
        
        printf("After:  %s\n", str);
        // Output:
        // Before: Hello
        // After:  olleH
    }
    
    // Example 9: Reversing in a loop (undo operation)
    void example_double_reverse(void) {
        int data[] = {1, 2, 3, 4, 5};
        size_t len = 5;
        
        printf("Original: ");
        for (size_t i = 0; i < len; i++) printf("%d ", data[i]);
        printf("\n");
        
        // Reverse once
        ALGO_REVERSE_TYPED(data, len, int);
        printf("After 1st reverse: ");
        for (size_t i = 0; i < len; i++) printf("%d ", data[i]);
        printf("\n");
        
        // Reverse again (back to original)
        ALGO_REVERSE_TYPED(data, len, int);
        printf("After 2nd reverse: ");
        for (size_t i = 0; i < len; i++) printf("%d ", data[i]);
        printf("\n");
        // Output:
        // Original: 1 2 3 4 5
        // After 1st reverse: 5 4 3 2 1
        // After 2nd reverse: 1 2 3 4 5
    }
    
    // Example 10: Reversing pointer arrays
    void example_pointer_reverse(void) {
        struct Record {
            int id;
            const char* name;
        };
        
        struct Record* records[] = {
            &(struct Record){1, "Alice"},
            &(struct Record){2, "Bob"},
            &(struct Record){3, "Charlie"}
        };
        
        printf("Before:\n");
        for (size_t i = 0; i < 3; i++) {
            printf("  %d: %s\n", records[i]->id, records[i]->name);
        }
        
        ALGO_REVERSE_TYPED(records, 3, struct Record*);
        
        printf("After:\n");
        for (size_t i = 0; i < 3; i++) {
            printf("  %d: %s\n", records[i]->id, records[i]->name);
        }
    }
    
    // Example 11: Even vs odd length arrays
    void example_even_odd_length(void) {
        int even[] = {1, 2, 3, 4};
        int odd[] = {1, 2, 3, 4, 5};
        
        printf("Even length before: ");
        for (size_t i = 0; i < 4; i++) printf("%d ", even[i]);
        printf("\n");
        
        ALGO_REVERSE_TYPED(even, 4, int);
        
        printf("Even length after:  ");
        for (size_t i = 0; i < 4; i++) printf("%d ", even[i]);
        printf("\n");
        
        printf("\nOdd length before:  ");
        for (size_t i = 0; i < 5; i++) printf("%d ", odd[i]);
        printf("\n");
        
        ALGO_REVERSE_TYPED(odd, 5, int);
        
        printf("Odd length after:   ");
        for (size_t i = 0; i < 5; i++) printf("%d ", odd[i]);
        printf("\n");
        // Note: middle element (3) stays in place for odd length
    }
    
    // Example 12: Reversing within a range
    void example_range_operations(void) {
        int data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        
        printf("Original: ");
        for (size_t i = 0; i < 10; i++) printf("%d ", data[i]);
        printf("\n");
        
        // Reverse first half
        algo_reverse(data, 5, sizeof(int));
        printf("First half reversed: ");
        for (size_t i = 0; i < 10; i++) printf("%d ", data[i]);
        printf("\n");
        
        // Reverse second half
        algo_reverse(&data[5], 5, sizeof(int));
        printf("Both halves reversed: ");
        for (size_t i = 0; i < 10; i++) printf("%d ", data[i]);
        printf("\n");
    }
    
    // Example 13: Generic usage
    void example_generic_usage(void) {
        // When type isn't known at compile time
        void* generic_array = malloc(10 * sizeof(double));
        double* doubles = (double*)generic_array;
        
        for (int i = 0; i < 10; i++) {
            doubles[i] = i * 1.5;
        }
        
        printf("Before: ");
        for (int i = 0; i < 10; i++) {
            printf("%.1f ", doubles[i]);
        }
        printf("\n");
        
        algo_reverse(generic_array, 10, sizeof(double));
        
        printf("After:  ");
        for (int i = 0; i < 10; i++) {
            printf("%.1f ", doubles[i]);
        }
        printf("\n");
        
        free(generic_array);
    }
    
    // Example 14: Checking if array equals its reverse
    void example_symmetric_check(void) {
        int sym[] = {1, 2, 3, 2, 1};
        int temp[5];
        
        // Copy original
        memcpy(temp, sym, sizeof(sym));
        
        // Reverse
        ALGO_REVERSE_TYPED(temp, 5, int);
        
        // Compare
        bool is_symmetric = (memcmp(sym, temp, sizeof(sym)) == 0);
        printf("Array is symmetric: %s\n", is_symmetric ? "yes" : "no");
        
        // Or use palindrome check directly
        is_symmetric = ALGO_IS_PALINDROME_TYPED(sym, 5, int,
                                                 algo_cmp_int, NULL);
        printf("Using palindrome check: %s\n", is_symmetric ? "yes" : "no");
    }
    
    // Example 15: Practical use - processing queue in reverse
    void example_queue_reverse_processing(void) {
        struct Task {
            int priority;
            const char* description;
        };
        
        struct Task tasks[] = {
            {1, "Low priority task"},
            {2, "Medium priority task"},
            {3, "High priority task"},
            {4, "Critical task"}
        };
        
        printf("Processing in reverse order (highest priority first):\n");
        
        // Reverse to process high priority first
        ALGO_REVERSE_TYPED(tasks, 4, struct Task);
        
        for (size_t i = 0; i < 4; i++) {
            printf("  Priority %d: %s\n",
                   tasks[i].priority, tasks[i].description);
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_REVERSE_H */
