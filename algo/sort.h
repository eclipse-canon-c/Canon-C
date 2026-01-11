// algo/sort.h
#ifndef CANON_ALGO_SORT_H
#define CANON_ALGO_SORT_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include "core/memory.h"

/**
 * @file sort.h
 * @brief Generic in-place sorting algorithms with stable guarantees
 *
 * Provides high-performance stable sorting with customizable comparators.
 * Implements a hybrid approach: insertion sort for small arrays, merge sort
 * for larger datasets. All algorithms are stable, preserving relative order
 * of equal elements.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h)
 *   - Depends on memory.h from this library
 *   - Uses statement expressions (GNU extension) in macros
 *   - Define CANON_NO_GNU_EXTENSIONS to disable macro versions
 *
 * Thread-safety: Functions are pure (no shared state) - safe to call from
 *                multiple threads on different data. Caller must ensure
 *                temp_buffer is not shared across threads.
 *
 * Performance:
 *   - Insertion sort (len < 16): O(n²) worst case, O(n) best case
 *   - Merge sort (len ≥ 16): O(n log n) guaranteed
 *   - Space: O(1) for small arrays, O(n) temporary buffer for merge sort
 *   - Stable: Equal elements maintain their relative order
 *   - Cache-friendly: Sequential memory access patterns
 *   - Adaptive: Faster on partially sorted data
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Stable sort guaranteed — equal elements keep relative order
 * - Hybrid strategy: insertion sort (small) + merge sort (large)
 * - Generic byte-level implementation (any element type)
 * - Custom comparator + optional context for flexible ordering
 * - Efficient: O(n log n) time, minimal overhead
 * - Safe: NULL checks, trivial cases (len < 2), assert validations
 * - Strongly typed macros for compile-time safety
 * - Perfect companion to algo_unique, vec containers, binary search
 * - No hidden allocations — caller provides temp buffer when needed
 * - Classic workflow: sort → unique → binary_search / process
 *
 * Stability guarantees:
 * ────────────────────────────────────────────────────────────────────────────
 * Stability means if two elements compare equal, their original relative
 * order is preserved. This is crucial for:
 *   - Multi-level sorting (sort by secondary key, then primary key)
 *   - Preserving insertion order for equal keys
 *   - Predictable, deterministic results
 *
 * Example:
 *   struct Record { int priority; int timestamp; };
 *   // Sort by timestamp first, then by priority
 *   sort(records, by_timestamp);  // stable!
 *   sort(records, by_priority);   // stable preserves timestamp order
 *   // Result: sorted by priority, ties broken by timestamp
 *
 * Algorithm selection:
 * ────────────────────────────────────────────────────────────────────────────
 * - len < 2:  No operation needed
 * - len < 16: Insertion sort (fast for small arrays, cache-friendly)
 * - len ≥ 16: Merge sort (guaranteed O(n log n), stable, predictable)
 *
 * Insertion sort is optimal for small arrays because:
 *   - Simple, minimal overhead
 *   - Excellent cache locality
 *   - Adaptive (O(n) for nearly sorted data)
 *   - No recursion overhead
 *
 * Merge sort is chosen for large arrays because:
 *   - Guaranteed O(n log n) performance (no worst-case quadratic)
 *   - Stable by design
 *   - Predictable behavior on all input patterns
 *   - Good cache performance with proper implementation
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Sorting arrays of numbers, strings, structs, pointers
 * - Preparing data for binary search (must be sorted first)
 * - Deduplication workflow: sort → algo_unique → shrink
 * - Implementing priority queues, ordered containers
 * - Multi-level sorting (sort by multiple keys)
 * - Custom ordering: case-insensitive, locale-aware, reverse, etc.
 * - Sorting by computed fields or complex criteria
 * - Database-like operations: ORDER BY, GROUP BY preprocessing
 * - Event processing: sorting by timestamp, priority
 * - Graph algorithms: edge sorting, topological sort preparation
 *
 * Temporary buffer management:
 * ────────────────────────────────────────────────────────────────────────────
 * For arrays with len ≥ 16, merge sort requires temporary space:
 *   - Size needed: len * elem_size bytes
 *   - Can be stack-allocated for small/medium arrays
 *   - Should be heap-allocated for large arrays
 *   - Can be reused across multiple sort operations
 *   - If NULL provided for large arrays, falls back to insertion sort
 *
 * Stack vs heap allocation guidelines:
 *   - Stack (fast): len * elem_size < ~8KB recommended
 *   - Heap (safe): len * elem_size ≥ 8KB or unknown size
 *
 * Example buffer allocation:
 * ```c
 * // Small array: stack buffer is fine
 * int arr[100];
 * int temp[100];
 * algo_sort(arr, 100, sizeof(int), cmp_int, NULL, temp);
 *
 * // Large array: use heap
 * int* big_arr = malloc(10000 * sizeof(int));
 * int* temp_buf = malloc(10000 * sizeof(int));
 * algo_sort(big_arr, 10000, sizeof(int), cmp_int, NULL, temp_buf);
 * free(temp_buf);
 * free(big_arr);
 *
 * // Alternative: NULL temp buffer → falls back to insertion sort
 * algo_sort(big_arr, 10000, sizeof(int), cmp_int, NULL, NULL);
 * // (works but slower for large arrays)
 * ```
 *
 * Usage examples:
 *
 * Basic integer sorting:
 * ```c
 * int cmp_int(const void* a, const void* b, void* ctx) {
 *     int x = *(const int*)a;
 *     int y = *(const int*)b;
 *     return (x > y) - (x < y);
 * }
 *
 * int values[] = {64, 34, 25, 12, 22, 11, 90};
 * ALGO_SORT_TYPED(values, 7, int, cmp_int, NULL);
 * // Result: {11, 12, 22, 25, 34, 64, 90}
 * ```
 *
 * Sorting strings (case-insensitive):
 * ```c
 * int cmp_string_ci(const void* a, const void* b, void* ctx) {
 *     const char* s1 = *(const char**)a;
 *     const char* s2 = *(const char**)b;
 *     return strcasecmp(s1, s2);
 * }
 *
 * const char* names[] = {"Charlie", "alice", "Bob", "dave"};
 * const char* temp_buf[4];
 * algo_sort(names, 4, sizeof(const char*), cmp_string_ci, NULL, temp_buf);
 * // Result: {"alice", "Bob", "Charlie", "dave"}
 * ```
 *
 * Sorting structs with context:
 * ```c
 * struct Person { const char* name; int age; };
 *
 * int cmp_by_field(const void* a, const void* b, void* ctx) {
 *     const char* field = (const char*)ctx;
 *     const struct Person* p1 = (const struct Person*)a;
 *     const struct Person* p2 = (const struct Person*)b;
 *     
 *     if (strcmp(field, "age") == 0) {
 *         return (p1->age > p2->age) - (p1->age < p2->age);
 *     } else if (strcmp(field, "name") == 0) {
 *         return strcmp(p1->name, p2->name);
 *     }
 *     return 0;
 * }
 *
 * struct Person people[100];
 * struct Person temp[100];
 * algo_sort(people, 100, sizeof(struct Person), cmp_by_field, "age", temp);
 * ```
 *
 * Vector sorting with deduplication:
 * ```c
 * vec_int numbers = ...;  // {5, 2, 3, 2, 1, 3, 5}
 * 
 * int temp_buf[100];
 * ALGO_SORT_VEC(numbers, int, cmp_int, NULL, temp_buf);
 * // numbers: {1, 2, 2, 3, 3, 5, 5}
 * 
 * ALGO_UNIQUE_CONSECUTIVE_VEC(numbers, int, cmp_int, NULL);
 * // numbers: {1, 2, 3, 5}  (len updated)
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Comparison function typedef
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Comparison function type for sorting algorithms
 *
 * @param a   Pointer to first element
 * @param b   Pointer to second element
 * @param ctx Optional context pointer (can be NULL)
 *
 * @return    Negative if a < b, 0 if a == b, positive if a > b
 *
 * Important: For stable sorting, the comparator must be consistent:
 *   - cmp(a, b) < 0  implies  cmp(b, a) > 0
 *   - cmp(a, b) == 0  implies  cmp(b, a) == 0
 *   - cmp(a, a) == 0  (reflexive)
 *   - If cmp(a, b) < 0 and cmp(b, c) < 0, then cmp(a, c) < 0 (transitive)
 */
typedef int (*algo_cmp_fn)(const void* a, const void* b, void* ctx);

/* ────────────────────────────────────────────────────────────────────────────
   Internal helper: Merge operation for merge sort
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Merges two sorted halves into a single sorted sequence
 *
 * Internal helper for merge sort. Assumes left and right halves are
 * already sorted.
 *
 * @param array      Base array containing both halves
 * @param left       Start index of left half
 * @param mid        End of left half (exclusive) / start of right half
 * @param right      End of right half (exclusive)
 * @param elem_size  Size of each element in bytes
 * @param cmp        Comparison function
 * @param ctx        Optional context
 * @param temp       Temporary buffer (size: (right-left) * elem_size)
 */
static inline void algo_merge(
    void* array,
    size_t left,
    size_t mid,
    size_t right,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx,
    void* temp
) {
    assert(array != NULL);
    assert(temp != NULL);
    assert(left <= mid && mid <= right);
    
    char* bytes = (char*)array;
    char* temp_bytes = (char*)temp;
    
    size_t i = left;      // Left half index
    size_t j = mid;       // Right half index
    size_t k = 0;         // Temp buffer index
    
    // Merge while both halves have elements
    while (i < mid && j < right) {
        const void* left_elem = bytes + i * elem_size;
        const void* right_elem = bytes + j * elem_size;
        
        // Use <= for stability: if equal, take from left (preserves order)
        if (cmp(left_elem, right_elem, ctx) <= 0) {
            mem_copy(temp_bytes + k * elem_size, left_elem, elem_size);
            ++i;
        } else {
            mem_copy(temp_bytes + k * elem_size, right_elem, elem_size);
            ++j;
        }
        ++k;
    }
    
    // Copy remaining elements from left half (if any)
    while (i < mid) {
        mem_copy(temp_bytes + k * elem_size, bytes + i * elem_size, elem_size);
        ++i;
        ++k;
    }
    
    // Copy remaining elements from right half (if any)
    while (j < right) {
        mem_copy(temp_bytes + k * elem_size, bytes + j * elem_size, elem_size);
        ++j;
        ++k;
    }
    
    // Copy merged result back to original array
    mem_copy(bytes + left * elem_size, temp_bytes, (right - left) * elem_size);
}

/**
 * @brief Recursive merge sort implementation
 *
 * Internal helper that recursively sorts array[left..right).
 *
 * @param array      Array to sort
 * @param left       Start index (inclusive)
 * @param right      End index (exclusive)
 * @param elem_size  Size of each element
 * @param cmp        Comparison function
 * @param ctx        Optional context
 * @param temp       Temporary buffer for merging
 */
static inline void algo_merge_sort_recursive(
    void* array,
    size_t left,
    size_t right,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx,
    void* temp
) {
    // Base case: 1 or 0 elements (already sorted)
    if (right - left <= 1) {
        return;
    }
    
    // Small array optimization: use insertion sort
    if (right - left < 16) {
        char* bytes = (char*)array;
        for (size_t i = left + 1; i < right; ++i) {
            for (size_t j = i; j > left; --j) {
                const void* a = bytes + (j - 1) * elem_size;
                const void* b = bytes + j * elem_size;
                
                // Stable: only swap if strictly greater
                if (cmp(a, b, ctx) <= 0) break;
                
                // Swap using temporary storage
                mem_swap(bytes + (j - 1) * elem_size, bytes + j * elem_size, elem_size);
            }
        }
        return;
    }
    
    // Divide
    size_t mid = left + (right - left) / 2;
    
    // Conquer: recursively sort both halves
    algo_merge_sort_recursive(array, left, mid, elem_size, cmp, ctx, temp);
    algo_merge_sort_recursive(array, mid, right, elem_size, cmp, ctx, temp);
    
    // Combine: merge sorted halves
    algo_merge(array, left, mid, right, elem_size, cmp, ctx, temp);
}

/* ────────────────────────────────────────────────────────────────────────────
   Public sorting interface
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Sorts array in-place using hybrid insertion/merge sort
 *
 * Stable sorting algorithm that automatically selects the best strategy:
 *   - Arrays with len < 16: Insertion sort (optimal for small data)
 *   - Arrays with len ≥ 16: Merge sort (guaranteed O(n log n))
 *
 * The algorithm is stable: equal elements maintain their relative order.
 * This is essential for multi-level sorting and predictable behavior.
 *
 * @param array       Pointer to the first element (NULL-safe)
 * @param len         Number of elements
 * @param elem_size   Size of each element in bytes (sizeof(Type))
 * @param cmp         Comparison function: <0 if a<b, 0 if equal, >0 if a>b
 * @param ctx         Optional context pointer passed to comparator (NULL-safe)
 * @param temp_buffer Temporary buffer for merge sort (size: len * elem_size)
 *                    - Required for len ≥ 16 (merge sort)
 *                    - Ignored for len < 16 (insertion sort)
 *                    - If NULL for large arrays, falls back to insertion sort
 *
 * Preconditions:
 *   - If array != NULL: array points to valid memory of size len * elem_size
 *   - elem_size > 0
 *   - cmp is a valid, consistent comparison function
 *   - If len ≥ 16 and temp_buffer != NULL: temp_buffer has len * elem_size bytes
 *
 * Postconditions:
 *   - Array is sorted according to the comparator
 *   - Stable: equal elements maintain relative order
 *   - temp_buffer contents are undefined (scratch space)
 *
 * Performance:
 *   - Time complexity:
 *     * Best case: O(n) for nearly sorted data (insertion sort adaptive)
 *     * Average case: O(n log n) (merge sort dominates)
 *     * Worst case: O(n log n) guaranteed (merge sort)
 *   - Space complexity:
 *     * len < 16: O(1) (insertion sort in-place)
 *     * len ≥ 16 with temp_buffer: O(n) (merge sort requires temp space)
 *     * len ≥ 16 without temp_buffer: O(1) (fallback to insertion sort)
 *   - Comparisons: O(n log n) worst case
 *   - Stability: Guaranteed for all array sizes
 *
 * Does nothing if:
 *   - array is NULL
 *   - len < 2 (already sorted)
 *   - cmp is NULL
 *
 * Example:
 *   int arr[] = {64, 34, 25, 12, 22, 11, 90};
 *   int temp[7];
 *   algo_sort(arr, 7, sizeof(int), cmp_int, NULL, temp);
 *   // Result: {11, 12, 22, 25, 34, 64, 90}
 *
 * Example with fallback:
 *   // Large array without temp buffer
 *   int big_arr[10000];
 *   algo_sort(big_arr, 10000, sizeof(int), cmp_int, NULL, NULL);
 *   // Falls back to insertion sort (slower but works)
 */
static inline void algo_sort(
    void* array,
    size_t len,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx,
    void* temp_buffer
) {
    assert(elem_size > 0 && "algo_sort: elem_size must be > 0");
    assert(cmp != NULL && "algo_sort: cmp function cannot be NULL");
    
    // Handle NULL or trivial cases
    if (!array || len < 2) {
        return;
    }
    
    char* bytes = (char*)array;
    
    // Small array: use insertion sort (fast, cache-friendly)
    if (len < 16) {
        for (size_t i = 1; i < len; ++i) {
            for (size_t j = i; j > 0; --j) {
                const void* a = bytes + (j - 1) * elem_size;
                const void* b = bytes + j * elem_size;
                
                // Stable: only swap if strictly greater
                if (cmp(a, b, ctx) <= 0) break;
                
                // Swap elements
                mem_swap(bytes + (j - 1) * elem_size, bytes + j * elem_size, elem_size);
            }
        }
        return;
    }
    
    // Large array: use merge sort if temp buffer available
    if (temp_buffer) {
        algo_merge_sort_recursive(array, 0, len, elem_size, cmp, ctx, temp_buffer);
    } else {
        // Fallback: insertion sort without temp buffer
        // Slower (O(n²)) but guaranteed to work
        for (size_t i = 1; i < len; ++i) {
            for (size_t j = i; j > 0; --j) {
                const void* a = bytes + (j - 1) * elem_size;
                const void* b = bytes + j * elem_size;
                
                if (cmp(a, b, ctx) <= 0) break;
                
                mem_swap(bytes + (j - 1) * elem_size, bytes + j * elem_size, elem_size);
            }
        }
    }
}

/**
 * @brief Sorts a vector in-place using hybrid insertion/merge sort
 *
 * Specialized version for vec-like structures with items and len fields.
 * Automatically handles temporary buffer management for small vectors
 * (uses stack allocation for reasonable sizes).
 *
 * @param vec_ptr    Pointer to vector structure (NULL-safe)
 * @param elem_size  Size of each element in bytes
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 * @param temp_buf   Temporary buffer (size: vec.len * elem_size)
 *                   If NULL, falls back to insertion sort for large vectors
 *
 * Example:
 *   vec_int numbers = ...;
 *   int temp[100];
 *   algo_sort_vec(&numbers, sizeof(int), cmp_int, NULL, temp);
 */
static inline void algo_sort_vec(
    void* vec_ptr,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx,
    void* temp_buffer
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
    
    algo_sort(vec->items, vec->len, elem_size, cmp, ctx, temp_buffer);
}

/* ────────────────────────────────────────────────────────────────────────────
   Utility: Check if array is sorted
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if array is sorted according to comparator
 *
 * Non-destructive verification function. Useful for:
 *   - Debugging / testing
 *   - Checking if sort is needed before sorting
 *   - Verifying sort algorithm correctness
 *
 * @param array      Pointer to first element (NULL-safe)
 * @param len        Number of elements
 * @param elem_size  Size of each element in bytes
 * @param cmp        Comparison function
 * @param ctx        Optional context (NULL-safe)
 *
 * @return           true if sorted (or len < 2), false otherwise
 *
 * Performance: O(n) time, O(1) space
 *
 * Example:
 *   int arr[] = {1, 2, 3, 4, 5};
 *   bool sorted = algo_is_sorted(arr, 5, sizeof(int), cmp_int, NULL);
 *   // sorted = true
 */
static inline bool algo_is_sorted(
    const void* array,
    size_t len,
    size_t elem_size,
    algo_cmp_fn cmp,
    void* ctx
) {
    assert(elem_size > 0 && "algo_is_sorted: elem_size must be > 0");
    assert(cmp != NULL && "algo_is_sorted: cmp function cannot be NULL");
    
    if (!array || len < 2) {
        return true;  // Empty or single-element arrays are sorted
    }
    
    const char* bytes = (const char*)array;
    
    for (size_t i = 1; i < len; ++i) {
        const void* prev = bytes + (i - 1) * elem_size;
        const void* curr = bytes + i * elem_size;
        
        // If any element is less than previous, not sorted
        if (cmp(prev, curr, ctx) > 0) {
            return false;
        }
    }
    
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (recommended for type safety)
   ──────────────────────────────────────────────────────────────────────────── */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Type-safe in-place sort for arrays
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 * Automatically allocates small temporary buffers on stack.
 *
 * @param array    Array to sort (modified in-place)
 * @param len      Number of elements
 * @param Type     Element type (used for sizeof)
 * @param cmp_expr Expression that evaluates to comparison function
 * @param ctx      Optional context pointer (NULL if not needed)
 *
 * Note: For large arrays (len ≥ 16), uses stack temp buffer of size len.
 *       For very large arrays (> ~1000 elements), consider using algo_sort()
 *       directly with heap-allocated temp buffer.
 *
 * Example:
 *   int data[] = {64, 34, 25, 12, 22, 11, 90};
 *   ALGO_SORT_TYPED(data, 7, int, cmp_int, NULL);
 */
#define ALGO_SORT_TYPED(array, len, Type, cmp_expr, ctx) \
    do { \
        if ((array) && (len) >= 2) { \
            size_t _len = (len); \
            Type _temp_buf[_len < 16 ? 1 : _len]; \
            void* _temp = _len < 16 ? NULL : _temp_buf; \
            algo_sort((array), _len, sizeof(Type), \
                      (algo_cmp_fn)(cmp_expr), (ctx), _temp); \
        } \
    } while (0)

/**
 * @brief Type-safe in-place sort for vec containers
 *
 * Automatically handles vector structure and temporary buffer allocation.
 * Updates vector in-place with sorted contents.
 *
 * @param vec      Vector to sort (modified in-place)
 * @param Type     Element type of the vector
 * @param cmp_expr Comparison function expression
 * @param ctx      Optional context
 *
 * Example:
 *   vec_int numbers = ...;
 *   ALGO_SORT_VEC(numbers, int, cmp_int, NULL);
 */
#define ALGO_SORT_VEC(vec, Type, cmp_expr, ctx) \
    do { \
        if ((vec).items && (vec).len >= 2) { \
            size_t _len = (vec).len; \
            Type _temp_buf[_len < 16 ? 1 : _len]; \
            void* _temp = _len < 16 ? NULL : _temp_buf; \
            algo_sort((vec).items, _len, sizeof(Type), \
                      (algo_cmp_fn)(cmp_expr), (ctx), _temp); \
        } \
    } while (0)

/**
 * @brief Type-safe sorted check for arrays
 *
 * @param array    Array to check
 * @param len      Number of elements
 * @param Type     Element type
 * @param cmp_expr Comparison function
 * @param ctx      Optional context
 *
 * @return         true if sorted, false otherwise
 */
#define ALGO_IS_SORTED_TYPED(array, len, Type, cmp_expr, ctx) \
    ({ \
        bool _result = true; \
        if ((array) && (len) >= 2) { \
            _result = algo_is_sorted((array), (len), sizeof(Type), \
                                     (algo_cmp_fn)(cmp_expr), (ctx)); \
        } \
        _result; \
    })

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ────────────────────────────────────────────────────────────────────────────
   Common comparison functions (convenience helpers)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Standard integer comparator (ascending order)
 *
 * @param a   Pointer to first int
 * @param b   Pointer to second int
 * @param ctx Unused (can be NULL)
 * @return    Negative if *a < *b, 0 if equal, positive if *a > *b
 */
static inline int algo_cmp_int(const void* a, const void* b, void* ctx) {
    (void)ctx;
    int x = *(const int*)a;
    int y = *(const int*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Standard size_t comparator (ascending order)
 */
static inline int algo_cmp_size(const void* a, const void* b, void* ctx) {
    (void)ctx;
    size_t x = *(const size_t*)a;
    size_t y = *(const size_t*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Standard double comparator (ascending order)
 *
 * Handles NaN values: NaN is considered greater than any number
 */
static inline int algo_cmp_double(const void* a, const void* b, void* ctx) {
    (void)ctx;
    double x = *(const double*)a;
    double y = *(const double*)b;
    
    // Handle NaN: NaN > any number, NaN == NaN
    if (x != x) return (y != y) ? 0 : 1;   // x is NaN
    if (y != y) return -1;                  // y is NaN, x is not
    
    return (x > y) - (x < y);
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ──────────────────────────────────────────────────────────────────────────

    #include "algo/sort.h"
    #include "algo/unique.h"
    #include <stdio.h>
    #include <string.h>
    
    // Example 1: Basic integer sorting
    void example_basic(void) {
        int arr[] = {64, 34, 25, 12, 22, 11, 90};
        size_t len = 7;
        
        printf("Before: ");
        for (size_t i = 0; i < len; i++) printf("%d ", arr[i]);
        printf("\n");
        
        ALGO_SORT_TYPED(arr, len, int, algo_cmp_int, NULL);
        
        printf("After:  ");
        for (size_t i = 0; i < len; i++) printf("%d ", arr[i]);
        printf("\n");
        // Output: 11 12 22 25 34 64 90
    }
    
    // Example 2: Sorting with deduplication
    void example_sort_unique(void) {
        int arr[] = {5, 2, 3, 2, 1, 3, 5, 5, 1};
        size_t len = 9;
        
        printf("Original: ");
        for (size_t i = 0; i < len; i++) printf("%d ", arr[i]);
        printf("\n");
        
        // Step 1: Sort
        ALGO_SORT_TYPED(arr, len, int, algo_cmp_int, NULL);
        printf("Sorted:   ");
        for (size_t i = 0; i < len; i++) printf("%d ", arr[i]);
        printf("\n");
        
        // Step 2: Remove duplicates
        size_t new_len = algo_unique_consecutive(arr, len, sizeof(int), 
                                                  algo_cmp_int, NULL);
        printf("Unique:   ");
        for (size_t i = 0; i < new_len; i++) printf("%d ", arr[i]);
        printf("\n");
        // Output: 1 2 3 5
    }
    
    // Example 3: Case-insensitive string sorting
    int cmp_string_ci(const void* a, const void* b, void* ctx) {
        (void)ctx;
        const char* s1 = *(const char**)a;
        const char* s2 = *(const char**)b;
        return strcasecmp(s1, s2);
    }
    
    void example_strings(void) {
        const char* names[] = {"Charlie", "alice", "Bob", "dave", "Eve"};
        size_t len = 5;
        
        const char* temp[5];
        algo_sort(names, len, sizeof(const char*), cmp_string_ci, NULL, temp);
        
        printf("Sorted (case-insensitive): ");
        for (size_t i = 0; i < len; i++) {
            printf("%s ", names[i]);
        }
        printf("\n");
        // Output: alice Bob Charlie dave Eve
    }
    
    // Example 4: Sorting structs
    struct Person {
        const char* name;
        int age;
    };
    
    int cmp_person_by_age(const void* a, const void* b, void* ctx) {
        (void)ctx;
        const struct Person* p1 = (const struct Person*)a;
        const struct Person* p2 = (const struct Person*)b;
        return (p1->age > p2->age) - (p1->age < p2->age);
    }
    
    void example_structs(void) {
        struct Person people[] = {
            {"Alice", 30},
            {"Bob", 25},
            {"Charlie", 35},
            {"Dave", 25}
        };
        size_t len = 4;
        
        struct Person temp[4];
        algo_sort(people, len, sizeof(struct Person), 
                  cmp_person_by_age, NULL, temp);
        
        printf("Sorted by age:\n");
        for (size_t i = 0; i < len; i++) {
            printf("  %s: %d\n", people[i].name, people[i].age);
        }
        // Bob and Dave (both 25) maintain relative order (stable!)
    }
    
    // Example 5: Multi-level sorting (stable sort property)
    int cmp_person_by_name(const void* a, const void* b, void* ctx) {
        (void)ctx;
        const struct Person* p1 = (const struct Person*)a;
        const struct Person* p2 = (const struct Person*)b;
        return strcmp(p1->name, p2->name);
    }
    
    void example_multi_level_sort(void) {
        struct Person people[] = {
            {"Charlie", 25},
            {"Alice", 30},
            {"Bob", 25},
            {"Dave", 30}
        };
        size_t len = 4;
        
        struct Person temp[4];
        
        // First, sort by name (secondary key)
        algo_sort(people, len, sizeof(struct Person), 
                  cmp_person_by_name, NULL, temp);
        
        // Then, sort by age (primary key)
        // Stable sort preserves name order for same age!
        algo_sort(people, len, sizeof(struct Person), 
                  cmp_person_by_age, NULL, temp);
        
        printf("Sorted by age, then name:\n");
        for (size_t i = 0; i < len; i++) {
            printf("  %s: %d\n", people[i].name, people[i].age);
        }
        // Output: Bob(25), Charlie(25), Alice(30), Dave(30)
        // Within each age group, alphabetical order is preserved!
    }
    
    // Example 6: Vector sorting
    #include "data/vec.h"
    
    DEFINE_VEC(int)
    
    void example_vector(void) {
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        int data[] = {64, 34, 25, 12, 22, 11, 90};
        for (int i = 0; i < 7; i++) {
            vec_int_push(&numbers, data[i]);
        }
        
        printf("Before: length = %zu\n", vec_int_len(&numbers));
        
        ALGO_SORT_VEC(numbers, int, algo_cmp_int, NULL);
        
        printf("After:  ");
        for (size_t i = 0; i < numbers.len; i++) {
            printf("%d ", numbers.items[i]);
        }
        printf("\n");
    }
    
    // Example 7: Large array with heap allocation
    void example_large_array(void) {
        size_t len = 100000;
        int* arr = malloc(len * sizeof(int));
        int* temp = malloc(len * sizeof(int));
        
        // Fill with random data
        for (size_t i = 0; i < len; i++) {
            arr[i] = rand() % 10000;
        }
        
        printf("Sorting %zu elements...\n", len);
        algo_sort(arr, len, sizeof(int), algo_cmp_int, NULL, temp);
        
        // Verify sorted
        bool sorted = algo_is_sorted(arr, len, sizeof(int), algo_cmp_int, NULL);
        printf("Is sorted: %s\n", sorted ? "yes" : "no");
        
        free(temp);
        free(arr);
    }
    
    // Example 8: Checking if sort is needed
    void example_check_sorted(void) {
        int sorted_arr[] = {1, 2, 3, 4, 5};
        int unsorted_arr[] = {5, 2, 3, 1, 4};
        
        if (!ALGO_IS_SORTED_TYPED(sorted_arr, 5, int, algo_cmp_int, NULL)) {
            printf("Sorting sorted_arr...\n");
            ALGO_SORT_TYPED(sorted_arr, 5, int, algo_cmp_int, NULL);
        } else {
            printf("sorted_arr is already sorted, skipping\n");
        }
        
        if (!ALGO_IS_SORTED_TYPED(unsorted_arr, 5, int, algo_cmp_int, NULL)) {
            printf("Sorting unsorted_arr...\n");
            ALGO_SORT_TYPED(unsorted_arr, 5, int, algo_cmp_int, NULL);
        }
    }
    
    // Example 9: Reverse order sorting
    int cmp_int_reverse(const void* a, const void* b, void* ctx) {
        (void)ctx;
        int x = *(const int*)a;
        int y = *(const int*)b;
        return (y > x) - (y < x);  // Reversed comparison
    }
    
    void example_reverse_sort(void) {
        int arr[] = {1, 5, 3, 9, 2, 7};
        size_t len = 6;
        
        ALGO_SORT_TYPED(arr, len, int, cmp_int_reverse, NULL);
        
        printf("Descending order: ");
        for (size_t i = 0; i < len; i++) {
            printf("%d ", arr[i]);
        }
        printf("\n");
        // Output: 9 7 5 3 2 1
    }
    
    // Example 10: Sorting with context
    struct SortContext {
        bool reverse;
        bool case_sensitive;
    };
    
    int cmp_string_ctx(const void* a, const void* b, void* ctx) {
        const char* s1 = *(const char**)a;
        const char* s2 = *(const char**)b;
        struct SortContext* context = (struct SortContext*)ctx;
        
        int result;
        if (context->case_sensitive) {
            result = strcmp(s1, s2);
        } else {
            result = strcasecmp(s1, s2);
        }
        
        return context->reverse ? -result : result;
    }
    
    void example_context_sort(void) {
        const char* words[] = {"Zebra", "apple", "Banana", "cherry"};
        size_t len = 4;
        const char* temp[4];
        
        struct SortContext ctx = {.reverse = false, .case_sensitive = false};
        
        printf("Case-insensitive ascending:\n");
        algo_sort(words, len, sizeof(const char*), cmp_string_ctx, &ctx, temp);
        for (size_t i = 0; i < len; i++) {
            printf("  %s\n", words[i]);
        }
        
        ctx.reverse = true;
        printf("\nCase-insensitive descending:\n");
        algo_sort(words, len, sizeof(const char*), cmp_string_ctx, &ctx, temp);
        for (size_t i = 0; i < len; i++) {
            printf("  %s\n", words[i]);
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_ALGO_SORT_H */
