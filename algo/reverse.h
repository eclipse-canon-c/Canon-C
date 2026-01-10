// algo/reverse.h
#ifndef CANON_ALGO_REVERSE_H
#define CANON_ALGO_REVERSE_H
#include <stddef.h>
#include "core/memory.h"   // For mem_copy (safe byte-level copy)

/**
 * @file reverse.h
 * @brief In-place and safe sequence reversal algorithms
 *
 * Provides efficient, generic, in-place reversal of arrays, with strong typing
 * and vec.h container integration.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - In-place by default — minimal memory usage (only tiny temp buffer)
 * - Never allocates heap memory
 * - Works with any element size (including large structs)
 * - Safe against NULL pointers and trivial cases (len < 2)
 * - Uses byte-level manipulation with safe mem_copy
 * - Strongly typed convenience macros
 * - Seamless integration with vec.h style containers
 * - No function pointers — maximum performance & inlining potential
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Reverse order of numbers, strings, points, records...
 * - Prepare data for processing in reverse order
 * - Undo stack-like operations
 * - Algorithm preprocessing step (palindrome checks, etc.)
 * - Quick reordering of small-to-medium arrays/vectors
 *
 * Usage examples:
 *
 * Basic typed reversal:
 * ```c
 * int scores[] = {10, 20, 30, 40, 50};
 * ALGO_REVERSE_TYPED(scores, 5, int);
 * // now: {50, 40, 30, 20, 10}
 * ```
 *
 * With vec container:
 * ```c
 * vec_string names = ...; // {"Alice", "Bob", "Charlie"}
 * ALGO_REVERSE_VEC(names, string);
 * // now: {"Charlie", "Bob", "Alice"}
 * ```
 *
 * Generic (when type is not known at macro time):
 * ```c
 * struct Point { float x, y; } points[100];
 * algo_reverse(points, 100, sizeof(struct Point));
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Generic byte-wise in-place reverse
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reverses array of arbitrary elements in-place
 *
 * Uses two-pointer technique + small temporary buffer.
 * Safe for any element size (up to 256 bytes — enough for most real types).
 *
 * @param array      Pointer to the first element
 * @param len        Number of elements in the array
 * @param elem_size  Size of each element in bytes (sizeof(Type))
 *
 * Does nothing if:
 * - array is NULL
 * - len < 2
 */
static inline void algo_reverse(
    void* array,
    size_t len,
    size_t elem_size
) {
    if (!array || len < 2) return;

    char* bytes = (char*)array;
    char* left  = bytes;
    char* right = bytes + (len - 1) * elem_size;

    char tmp[256];  // Small temporary buffer — safe for most real-world types

    while (left < right) {
        mem_copy(tmp,   left,  elem_size);
        mem_copy(left,  right, elem_size);
        mem_copy(right, tmp,   elem_size);

        left  += elem_size;
        right -= elem_size;
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed convenience macros (recommended)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Type-safe in-place reverse for known element type
 *
 * @param array  Array of Type (will be modified in-place)
 * @param len    Number of elements
 * @param Type   Element type (used only for sizeof)
 */
#define ALGO_REVERSE_TYPED(array, len, Type) \
    do { \
        if ((array) && (len) >= 2) { \
            algo_reverse((array), (len), sizeof(Type)); \
        } \
    } while (0)

/**
 * @brief Reverse entire contents of a vec container in-place
 *
 * Assumes vec has .items and .len fields (Canon vec style)
 *
 * @param vec   Vector to reverse (modified in-place)
 * @param Type  Element type stored in the vector
 */
#define ALGO_REVERSE_VEC(vec, Type) \
    ALGO_REVERSE_TYPED((vec).items, (vec).len, Type)

#endif /* CANON_ALGO_REVERSE_H */
