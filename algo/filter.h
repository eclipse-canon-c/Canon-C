// algo/filter.h
#ifndef CANON_ALGO_FILTER_H
#define CANON_ALGO_FILTER_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros

/**
 * @file filter.h
 * @brief Functional-style filtering over sequences (select elements matching predicate)
 *
 * Selects and copies elements that satisfy a predicate into a caller-provided output buffer.
 * Preserves original relative order (stable filter).
 *
 * Key properties:
 *   - Read-only input — never modifies source array
 *   - No allocation, no mutation of input, no ownership transfer
 *   - Short-circuits when output buffer is full (truncates safely)
 *   - Supports optional user context (void* ctx)
 *   - Generic (void*) and strongly-typed versions
 *   - Seamless integration with vec.h containers
 *
 * Typical use cases:
 *   - Collect matching items into fixed buffer
 *   - Filter in-place (when input/output same buffer)
 *   - Safe bounded processing (never overflows)
 *   - Zero-heap filtering
 *
 * Predicate signature:
 *   bool pred(const void* elem, void* ctx)  // or const Type* for typed
 *
 * Usage example (generic):
 *   bool is_positive(const void* elem, void* ctx) {
 *       return *(int*)elem > 0;
 *   }
 *   int input[] = {-1, 2, -3, 4, 0};
 *   const void* output[5];
 *   size_t count = algo_filter_into((const void**)input, 5, is_positive, NULL, output, 5);
 *   // output[0] = &input[1], output[1] = &input[3], count = 2
 *
 * Usage example (typed + vec):
 *   bool is_even(const int* elem, void* ctx) { return *elem % 2 == 0; }
 *   vec_int in_vec = ...;
 *   int out_buf[10];
 *   size_t count = ALGO_FILTER_INTO(out_buf, in_vec.items, in_vec.len, int, is_even, NULL);
 */

/**
 * @brief Predicate function type (generic version)
 *
 * @param elem  Pointer to current element
 * @param ctx   Optional user context (may be NULL)
 * @return      true to keep the element, false to discard
 */
typedef bool (*algo_filter_pred)(const void* elem, void* ctx);

/* ────────────────────────────────────────────────────────────────────────────
   Generic version (void* elements — flexible but less type-safe)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Filters elements into caller-provided output buffer
 *
 * Copies pointers to matching elements (borrowed views).
 * Preserves order. Truncates if output capacity insufficient.
 *
 * @param items     Array of void* pointers (input)
 * @param len       Number of input elements
 * @param pred      Predicate function
 * @param ctx       Optional user context (passed to pred)
 * @param out       Output array of void* (non-const — we write pointers)
 * @param out_cap   Maximum number of elements output can hold
 * @return          Number of elements written (≤ out_cap)
 *
 * Returns 0 on invalid input (NULL items/out/pred).
 * Short-circuits when output is full.
 * Safe: const-cast on pointers is valid (borrowed, read-only use).
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
        if (out_len >= out_cap) break;  // truncate early

        if (pred(items[i], ctx)) {
            out[out_len++] = (void*)items[i];  // const cast: safe (borrowed)
        }
    }

    return out_len;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed version (recommended — safer, cleaner syntax)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Typed filter into caller-provided output array
 *
 * Uses compound statement expression (GNU C/C++ extension, widely supported).
 * Copies actual values (not pointers) — type-safe.
 *
 * @param out_array Output array of Type (caller provides)
 * @param in_array  Input array of Type
 * @param len       Number of input elements
 * @param Type      Element type
 * @param pred      Predicate: bool (*)(const Type*, void*)
 * @param ctx       Optional context
 * @return          Number of elements written (≤ array size)
 */
#define ALGO_FILTER_INTO(out_array, in_array, len, Type, pred, ctx) \
    ({ \
        size_t _out_len = 0; \
        if ((out_array) && (in_array) && (pred)) { \
            const size_t _cap = (sizeof(out_array) / sizeof((out_array)[0])); \
            for (size_t _i = 0; _i < (len) && _out_len < _cap; ++_i) { \
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
 * @brief Filters vec into caller-provided output buffer
 *
 * Convenience wrapper over ALGO_FILTER_INTO
 * Copies values (not pointers) into out_buf
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

#endif /* CANON_ALGO_FILTER_H */
