#ifndef CANON_ALGO_REVERSE_H
#define CANON_ALGO_REVERSE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "core/primitives/compare.h"   // provides algo_cmp_fn

/**
 * @file algo/reverse.h
 * @brief In-place sequence reversal algorithms
 *
 * Provides efficient, generic, in-place reversal of arrays and containers.
 * Uses a two-pointer technique with byte-level swapping to reverse elements
 * of any type in linear time with constant space.
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
 * - Seamless integration with data/ containers
 * - Maximum performance: no function pointers, excellent inlining
 * - Simple, understandable algorithm
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Two-pointer technique:
 * 1. Start with left pointer at array[0], right pointer at array[n-1]
 * 2. Swap elements at left and right
 * 3. Move left forward, right backward
 * 4. Repeat until left >= right (pointers meet in middle)
 *
 * Example: Reversing [1, 2, 3, 4, 5]
 * Step 1: [1, 2, 3, 4, 5] → swap array[0] ↔ array[4]
 * Step 2: [5, 2, 3, 4, 1] → swap array[1] ↔ array[3]
 * Step 3: [5, 4, 3, 2, 1] → left == right (done)
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 * reverse.h uses core/memory.h for mem_copy instead of memcpy directly.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - No allocations anywhere in this file
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions are thread-safe (no shared mutable state)
 * - Safe to call concurrently on different arrays from multiple threads
 * - Same array should not be modified concurrently without external synchronization
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(n) - exactly ⌊n/2⌋ swaps
 * - Space complexity: O(1) - small fixed-size temporary buffer
 * - In-place algorithm: modifies array directly
 * - No heap allocations
 * - Cache-friendly: sequential access pattern
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
 * - Sorting helper (reverse to get descending order)
 *
 * Quick start:
 * ```c
 * #include "algo/reverse.h"
 *
 * // Basic integer reversal
 * int scores[] = {10, 20, 30, 40, 50};
 * ALGO_REVERSE_TYPED(scores, 5, int);
 * // Result: {50, 40, 30, 20, 10}
 *
 * // Reversing strings array
 * const char* names[] = {"Alice", "Bob", "Charlie", "Dave"};
 * ALGO_REVERSE_TYPED(names, 4, const char*);
 * // Result: {"Dave", "Charlie", "Bob", "Alice"}
 *
 * // Generic (when type is not known at compile time)
 * struct Point { float x, y; } points[100];
 * algo_reverse(points, 100, sizeof(struct Point));
 *
 * // Partial reversal (subrange)
 * int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
 * // Reverse middle portion [3, 4, 5, 6]
 * algo_reverse(&data[2], 4, sizeof(int));
 * // Result: {1, 2, 6, 5, 4, 3, 7, 8}
 *
 * // Palindrome check
 * int arr[] = {1, 2, 3, 2, 1};
 * bool is_pal = algo_is_palindrome(arr, 5, sizeof(int), algo_cmp_int, NULL);
 * // is_pal = true
 * ```
 *
 * @sa core/primitives/compare.h — algo_cmp_fn for palindrome check
 * @sa core/memory.h — mem_copy used for element swapping
 * @sa core/slice.h — slice_##type used by DEFINE_ALGO_REVERSE
 * @sa core/ownership.h — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Swap buffer size limit
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Maximum element size supported by algo_reverse
 *
 * algo_reverse uses a fixed-size stack buffer for swapping elements.
 * Override by defining ALGO_REVERSE_SWAP_BUF_SIZE before including this header.
 * Default: 256 bytes (handles most types: doubles, small structs, pointers).
 */
#ifndef ALGO_REVERSE_SWAP_BUF_SIZE
    #define ALGO_REVERSE_SWAP_BUF_SIZE ((usize)256)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Public interface
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Reverses array of arbitrary elements in-place
 *
 * Uses two-pointer technique with byte-level swapping. Efficient for
 * all element sizes, from single bytes to large structs.
 *
 * Algorithm:
 * - Maintains left and right pointers
 * - Swaps elements at left and right positions
 * - Moves pointers toward center
 * - Stops when pointers meet (left >= right)
 * - Performs exactly ⌊n/2⌋ swaps
 *
 * @param array Pointer to the first element (borrowed — modified in place)
 * @param len Number of elements in the array
 * @param elem_size Size of each element in bytes (> 0)
 *
 * @return void
 *
 * @pre elem_size > 0
 * @pre elem_size <= ALGO_REVERSE_SWAP_BUF_SIZE
 * @pre If array != NULL, array points to valid memory of size len * elem_size
 *
 * @post Array elements are reversed: array[i] ↔ array[n-1-i]
 * @post Original order is lost
 * @post For even length: all elements swapped
 * @post For odd length: middle element stays in place
 *
 * Ownership:
 * - array: borrowed (modified in place, elements reordered)
 *
 * Performance:
 * - Time: O(n) - exactly ⌊n/2⌋ swaps
 * - Space: O(1) - fixed-size temporary buffer on stack
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays
 * - Not safe to call concurrently on same array without external sync
 *
 * Does nothing if:
 * - array is NULL
 * - len < 2 (0 or 1 elements already "reversed")
 */
static inline void algo_reverse(
    borrowed void* array,
    usize len,
    usize elem_size)
{
    require_msg(elem_size > 0, "algo_reverse: elem_size must be > 0");
    require_msg(elem_size <= ALGO_REVERSE_SWAP_BUF_SIZE,
                "algo_reverse: elem_size exceeds ALGO_REVERSE_SWAP_BUF_SIZE");
   
    /* Handle NULL or trivial cases */
    if (!array || len < 2) {
        return;
    }
   
    u8* bytes = (u8*)array;
    u8* left = bytes;
    u8* right = bytes + (len - 1) * elem_size;
   
    /* Temporary buffer for swapping */
    u8 temp[ALGO_REVERSE_SWAP_BUF_SIZE];
   
    /* Two-pointer technique: swap from ends toward middle */
    while (left < right) {
        /* Three-way swap using temp buffer */
        mem_copy(temp, left, elem_size);
        mem_copy(left, right, elem_size);
        mem_copy(right, temp, elem_size);
       
        /* Move pointers toward center */
        left += elem_size;
        right -= elem_size;
    }
}

/**
 * @brief Checks if array is a palindrome
 *
 * Non-destructive check - array is not modified.
 * Requires a comparison function to check element equality.
 *
 * @param array Pointer to first element (borrowed, read-only)
 * @param len Number of elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param cmp Comparison function (borrowed — returns 0 if elements equal)
 * @param ctx Optional context (borrowed, may be NULL)
 *
 * @return true if palindrome, false otherwise
 *
 * @pre elem_size > 0
 * @pre If array != NULL, array points to valid array of len elements
 *
 * Ownership:
 * - array: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Performance:
 * - Time: O(n/2) comparisons
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 */
static inline bool algo_is_palindrome(
    borrowed const void* array,
    usize len,
    usize elem_size,
    borrowed algo_cmp_fn cmp,
    borrowed void* ctx)
{
    require_msg(elem_size > 0, "algo_is_palindrome: elem_size must be > 0");
    require_msg(cmp != NULL, "algo_is_palindrome: cmp function cannot be NULL");
   
    if (!array || len < 2) {
        return true; /* Empty or single-element arrays are palindromes */
    }
   
    const u8* bytes = (const u8*)array;
    usize left_idx = 0;
    usize right_idx = len - 1;
   
    while (left_idx < right_idx) {
        const void* left_elem = bytes + left_idx * elem_size;
        const void* right_elem = bytes + right_idx * elem_size;
       
        if (cmp(left_elem, right_elem, ctx) != 0) {
            return false; /* Mismatch found */
        }
       
        ++left_idx;
        --right_idx;
    }
   
    return true; /* All pairs matched */
}

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════
   GNU extension versions (statement expressions) provide better ergonomics.
   C99 fallback versions available when CANON_NO_GNU_EXTENSIONS is defined.
   ════════════════════════════════════════════════════════════════════════════ */
#ifndef CANON_NO_GNU_EXTENSIONS
/**
 * @def ALGO_REVERSE_TYPED(array, len, Type)
 * @brief Type-safe in-place reverse for known element type (GNU version)
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 */
#define ALGO_REVERSE_TYPED(array, len, Type) \
    do { \
        if ((array) && (len) >= 2) { \
            algo_reverse((array), (len), sizeof(Type)); \
        } \
    } while (0)

/**
 * @def ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp, ctx)
 * @brief Type-safe palindrome check — returns bool (GNU version)
 */
#define ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp, ctx) \
    ({ \
        bool _result = true; \
        if ((array) && (len) >= 2) { \
            _result = algo_is_palindrome((array), (len), sizeof(Type), \
                                        (algo_cmp_fn)(cmp), (ctx)); \
        } \
        _result; \
    })
#else /* CANON_NO_GNU_EXTENSIONS */
/* C99 fallback versions */
#define ALGO_REVERSE_TYPED(array, len, Type) \
    do { \
        if ((array) && (len) >= 2) { \
            algo_reverse((array), (len), sizeof(Type)); \
        } \
    } while (0)

#define ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp, ctx) \
    ((array) && (len) >= 2 ? algo_is_palindrome((array), (len), sizeof(Type), \
                                                (algo_cmp_fn)(cmp), (ctx)) : true)
#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_REVERSE — typed slice variant per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type).
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Generates reverse/palindrome functions that accept slice_##type directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 * - algo_reverse_slice_##type(sv) → void
 * - algo_is_palindrome_slice_##type(sv, cmp, ctx) → bool
 */
#define DEFINE_ALGO_REVERSE(type) \
\
/** \
 * @brief Reverses the elements of a slice_##type in-place \
 * \
 * @param sv Slice (borrowed non-owning view — reverses underlying array) \
 */ \
static inline void algo_reverse_slice_##type( \
    borrowed slice_##type sv) \
{ \
    if (!sv.ptr || sv.len < 2) return; \
    algo_reverse(sv.ptr, sv.len, sizeof(type)); \
} \
\
/** \
 * @brief Returns true if slice_##type is a palindrome \
 * \
 * @param sv Slice to check (borrowed, read-only) \
 * @param cmp Comparator (borrowed) \
 * @param ctx Optional context (borrowed, may be NULL) \
 */ \
static inline bool algo_is_palindrome_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_cmp_fn cmp, \
    borrowed void* ctx) \
{ \
    return algo_is_palindrome(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

#endif /* CANON_ALGO_REVERSE_H */
