// algo/find.h
#ifndef CANON_ALGO_FIND_H
#define CANON_ALGO_FIND_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros

/**
 * @file find.h
 * @brief Locate the first element matching a predicate (functional style)
 *
 * Scans a sequence left-to-right and returns the first element that satisfies
 * the given predicate. Short-circuits on first match.
 *
 * Key properties:
 *   - Read-only access — never modifies input
 *   - Short-circuiting — stops as soon as match is found
 *   - Zero allocation, zero mutation, zero ownership transfer
 *   - Supports optional user context (void* ctx)
 *   - Generic (void*) and strongly-typed versions
 *   - Seamless integration with vec.h containers
 *   - Optional output parameters for index and/or element pointer
 *
 * Typical use cases:
 *   - Find first matching item in array or vec
 *   - Search with early exit
 *   - Get both match status and position/value
 *
 * Predicate signature:
 *   bool pred(const void* elem, void* ctx)  // or const Type* for typed
 *
 * Usage example (generic):
 *   bool is_negative(const void* elem, void* ctx) {
 *       return *(int*)elem < 0;
 *   }
 *   int arr[] = {1, 3, -5, 7};
 *   size_t idx;
 *   const void* found;
 *   if (algo_find((const void**)arr, 4, is_negative, NULL, &idx, &found)) {
 *       printf("Found negative at index %zu: %d\n", idx, *(int*)found);
 *   }
 *
 * Usage example (typed + vec):
 *   bool is_zero(const int* elem, void* ctx) { return *elem == 0; }
 *   vec_int v = ...;
 *   size_t idx;
 *   if (ALGO_FIND_VEC(v, int, is_zero, NULL, &idx, NULL)) {
 *       // found at index idx
 *   }
 */

/**
 * @brief Predicate function type (generic version)
 *
 * @param elem  Pointer to current element
 * @param ctx   Optional user context (may be NULL)
 * @return      true if this is the desired element, false to continue
 */
typedef bool (*algo_find_pred)(const void* elem, void* ctx);

/* ────────────────────────────────────────────────────────────────────────────
   Generic version (void* elements — flexible but less type-safe)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Finds the first element satisfying the predicate
 *
 * Scans left-to-right, short-circuits on first match.
 *
 * @param items      Array of void* pointers (input)
 * @param len        Number of elements
 * @param pred       Predicate function
 * @param ctx        Optional user context (passed to pred)
 * @param out_index  Optional: receives index of first match (pass NULL if not needed)
 * @param out_elem   Optional: receives pointer to matching element (pass NULL if not needed)
 * @return           true if match found, false otherwise (or invalid input)
 *
 * Returns false immediately on invalid input (NULL items/pred).
 * All pointers returned are borrowed — valid only as long as input lives.
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
   Strongly typed version (recommended — safer, cleaner syntax)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Typed find with optional output parameters
 *
 * Uses compound statement expression (GNU C/C++ extension, widely supported).
 *
 * @param array      Array of Type
 * @param len        Number of elements
 * @param Type       Element type
 * @param pred       Predicate: bool (*)(const Type*, void*)
 * @param ctx        Optional context
 * @param out_index  Optional pointer to size_t (index)
 * @param out_elem   Optional pointer to const Type* (element pointer)
 * @return           true if found, false otherwise
 */
#define ALGO_FIND(array, len, Type, pred, ctx, out_index, out_elem) \
    ({ \
        bool _found = false; \
        if ((array) && (pred)) { \
            for (size_t _i = 0; _i < (len); ++_i) { \
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
 * Convenience wrapper over ALGO_FIND
 * Returns true if found, optionally sets index/element
 */
#define ALGO_FIND_VEC(vec, Type, pred, ctx, out_index, out_elem) \
    ALGO_FIND((vec).items, (vec).len, Type, pred, ctx, out_index, out_elem)

#endif /* CANON_ALGO_FIND_H */
