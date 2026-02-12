#ifndef CANON_DATA_VEC_RANGE_H
#define CANON_DATA_VEC_RANGE_H

#include "data/vec/vec_impl.h"
#include "data/vec/vec_mangle.h"
#include "data/range.h"

/**
 * @file vec_range.h
 * @brief Optional range-fill extension for Canon-C typed vectors
 *
 * Adds extend_from_range to any typed vector instantiated via DEFINE_VEC.
 * Depends on data/range.h for the IntRange type and range iteration.
 *
 * This file is intentionally separate from vec_defn.h to keep the core
 * vec module free of data/range.h as a dependency. Only include it when
 * you need range-to-vector filling.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - No hidden allocation — fills into existing bounded buffer
 * - Supports ascending and descending ranges
 * - Overflow-protected element count calculation via checked.h
 * - Returns Result<bool, Error> on failure — never silently truncates
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends on data/range.h and data/vec/vec_impl.h
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * vec_range.h is in data/vec/ (ext). It may depend on:
 * - core/
 * - semantics/
 * - data/range.h     (same layer, different module — explicit dependency)
 * - data/vec/        (same module)
 *
 * Usage — extend a vec_int with values from an IntRange:
 * ```c
 * #include "data/vec/vec.h"
 * #include "data/vec/vec_range.h"
 *
 * DEFINE_VEC(static inline, int)
 * DEFINE_VEC_RANGE(static inline, int)
 *
 * int buf[16];
 * canon_vec_int v = canon_vec_int_init(buf, 16);
 *
 * IntRange r = range_make(0, 10);  // 0..10
 * result_bool_Error res = canon_vec_int_extend_from_range(&v, r);
 * // v now contains [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
 *
 * IntRange desc = range_make(5, 0);  // 5..0 descending
 * canon_vec_int_extend_from_range(&v, desc);
 * // appends [5, 4, 3, 2, 1]
 * ```
 *
 * @sa vec.h, vec_defn.h, data/range.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_EXTEND_FROM_RANGE — implementation macro
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Implementation macro for extend_from_range
 *
 * @param linkage  Function linkage
 * @param VecType  Mangled vector type name
 * @param fn       Mangled function name
 * @param type     Element type (must be castable from isize for range values)
 *
 * Generated function signature:
 * ```c
 * result_bool_Error fn(VecType* v, IntRange r);
 * ```
 *
 * @pre v != NULL
 * @pre v->items != NULL
 * @pre The range element count must not overflow usize (checked)
 * @pre v->len + count <= v->capacity (checked)
 *
 * @post On Ok: range values appended to v in iteration order
 * @post Returns Err(ERR_INVALID_ARG)       if v == NULL or v->items == NULL
 * @post Returns Err(ERR_OVERFLOW)          if range element count overflows usize
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if insufficient space in v
 *
 * @note Element values are cast from isize to type.
 *       Ensure type can represent the range values without truncation.
 *       For signed ranges into unsigned types, caller is responsible for validation.
 *
 * Performance:
 * - Time:  O(count) where count = abs(r.end - r.start)
 * - Space: O(1) — no allocation, fills into existing buffer
 */
#define IMPL_VEC_EXTEND_FROM_RANGE(linkage, VecType, fn, type) \
linkage result_bool_Error fn(VecType* v, IntRange r) { \
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    isize diff = (r.end >= r.start) \
        ? (isize)(r.end - r.start) \
        : (isize)(r.start - r.end); \
    usize count = (usize)diff; \
    usize total; \
    if (!checked_add(v->len, count, &total)) \
        return result_bool_Error_err(ERR_OVERFLOW); \
    if (total > v->capacity) \
        return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    isize step = (r.end >= r.start) ? 1 : -1; \
    isize val  = r.start; \
    for (usize i = 0; i < count; i++, val += step) { \
        v->items[v->len + i] = (type)val; \
    } \
    v->len = total; \
    return result_bool_Error_ok(true); \
}

/* ════════════════════════════════════════════════════════════════════════════
   MANGLE_VEC_EXTEND_FROM_RANGE — name mangling macro
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the extend_from_range function for a given element type
 *
 * Default: canon_vec_##type##_extend_from_range
 * Override before including this file if custom naming is needed.
 */
#ifndef MANGLE_VEC_EXTEND_FROM_RANGE
    #define MANGLE_VEC_EXTEND_FROM_RANGE(type) canon_vec_##type##_extend_from_range
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_VEC_RANGE — emit extend_from_range for a typed vector
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Adds extend_from_range to an existing typed vector instantiation
 *
 * Must be called AFTER DEFINE_VEC(linkage, type) for the same type.
 *
 * Generated function (using default mangle):
 * - canon_vec_##type##_extend_from_range(v, r) → result_bool_Error
 *
 * @param linkage Function linkage (should match the DEFINE_VEC call)
 * @param type    Element type (must be castable from isize)
 *
 * Example:
 * ```c
 * DEFINE_VEC(static inline, int)
 * DEFINE_VEC_RANGE(static inline, int)
 *
 * // Now available:
 * // result_bool_Error canon_vec_int_extend_from_range(canon_vec_int* v, IntRange r);
 * ```
 *
 * @note Only makes semantic sense for numeric element types (int, float, i32, etc.)
 *       The range values are cast from isize — ensure the element type can hold them.
 */
#define DEFINE_VEC_RANGE(linkage, type) \
    IMPL_VEC_EXTEND_FROM_RANGE(linkage, MANGLE_VEC_TYPE(type), MANGLE_VEC_EXTEND_FROM_RANGE(type), type)

#endif /* CANON_DATA_VEC_RANGE_H */
