// algo/any_all.h
#ifndef CANON_ALGO_ANY_ALL_H
#define CANON_ALGO_ANY_ALL_H

#include <stddef.h>
#include <stdbool.h>
#include "data/vec.h"  // For vec integration macros

/**
 * @file any_all.h
 * @brief Functional-style predicate checks over sequences (any/all)
 *
 * Provides short-circuiting checks to determine if:
 *   - **any** element satisfies a predicate (exists)
 *   - **all** elements satisfy a predicate (forall)
 *
 * Key properties:
 *   - Read-only access — never modifies input
 *   - Short-circuiting — stops as soon as result is known
 *   - Zero allocation, zero mutation, zero ownership
 *   - Supports optional user context (void* ctx)
 *   - Generic (void*) and strongly-typed versions
 *   - Seamless integration with vec.h containers
 *
 * Typical use cases:
 *   - Checking if any item matches a condition
 *   - Validating that all items satisfy requirements
 *   - Early exit in search/validation loops
 *
 * Predicate signature:
 *   bool pred(const void* elem, void* ctx)  // or const Type* for typed
 *
 * Usage example (generic):
 *   bool is_even(const void* elem, void* ctx) {
 *       return (*(int*)elem % 2 == 0);
 *   }
 *   int arr[] = {1, 3, 5, 7};
 *   if (algo_any((const void**)arr, 4, is_even, NULL)) {
 *       // at least one even
 *   }
 *
 * Usage example (typed + vec):
 *   bool is_positive(const int* elem, void* ctx) { return *elem > 0; }
 *   vec_int v = ...;
 *   if (ALGO_ANY_VEC(v, int, is_positive, NULL)) {
 *       // at least one positive
 *   }
 */

/**
 * @brief Predicate function type (generic version)
 *
 * @param elem  Pointer to current element
 * @param ctx   Optional user context (may be NULL)
 * @return      true if condition satisfied, false otherwise
 */
typedef bool (*algo_pred_fn)(const void* elem, void* ctx);

/* ────────────────────────────────────────────────────────────────────────────
   Generic versions (void* elements — flexible but less type-safe)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns true if at least one element satisfies the predicate
 *
 * Short-circuits on first true return.
 *
 * @param items  Array of void* pointers
 * @param len    Number of elements
 * @param pred   Predicate function
 * @param ctx    Optional user context (passed to pred)
 * @return       true if any element satisfies pred, false otherwise
 *
 * Returns false immediately on invalid input (NULL items/pred)
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
            return true;
        }
    }

    return false;
}

/**
 * @brief Returns true only if every element satisfies the predicate
 *
 * Short-circuits on first false return.
 *
 * @param items  Array of void* pointers
 * @param len    Number of elements
 * @param pred   Predicate function
 * @param ctx    Optional user context
 * @return       true if all elements satisfy pred, false otherwise
 *
 * Returns false immediately on invalid input or empty sequence
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
            return false;
        }
    }

    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Strongly typed versions (recommended — safer, cleaner syntax)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Typed version of algo_any (uses compound statement expression)
 *
 * @param items  Array of Type
 * @param len    Number of elements
 * @param Type   Element type
 * @param pred   Predicate: bool (*)(const Type*, void*)
 * @param ctx    Optional context
 * @return       true if any element satisfies pred
 */
#define ALGO_ANY_TYPED(items, len, Type, pred, ctx) \
    ({ \
        bool _result = false; \
        if ((items) && (pred)) { \
            for (size_t _i = 0; _i < (len); ++_i) { \
                if ((pred)(&(items)[_i], (ctx))) { \
                    _result = true; \
                    break; \
                } \
            } \
        } \
        _result; \
    })

/**
 * @brief Typed version of algo_all (compound statement expression)
 *
 * @param items  Array of Type
 * @param len    Number of elements
 * @param Type   Element type
 * @param pred   Predicate: bool (*)(const Type*, void*)
 * @param ctx    Optional context
 * @return       true if all elements satisfy pred
 */
#define ALGO_ALL_TYPED(items, len, Type, pred, ctx) \
    ({ \
        bool _result = true; \
        if ((items) && (pred)) { \
            for (size_t _i = 0; _i < (len); ++_i) { \
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
 * @brief Checks if any element in vec satisfies predicate
 *
 * Convenience wrapper over ALGO_ANY_TYPED
 */
#define ALGO_ANY_VEC(vec, Type, pred, ctx) \
    ALGO_ANY_TYPED((vec).items, (vec).len, Type, pred, ctx)

/**
 * @brief Checks if all elements in vec satisfy predicate
 *
 * Convenience wrapper over ALGO_ALL_TYPED
 */
#define ALGO_ALL_VEC(vec, Type, pred, ctx) \
    ALGO_ALL_TYPED((vec).items, (vec).len, Type, pred, ctx)

#endif /* CANON_ALGO_ANY_ALL_H */
