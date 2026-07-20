/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

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
 * Depends on data/range.h for the range type and range iteration API.
 *
 * This file is intentionally separate from vec_defn.h to keep the core
 * vec module free of data/range.h as a dependency. Only include it when
 * you need range-to-vector filling.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - No hidden allocation — fills into existing bounded buffer
 * - Supports ascending, descending, and stepped ranges (full range semantics)
 * - Overflow-protected element count calculation via range_len() + checked.h
 * - Returns result__Bool_Error on failure — never silently truncates
 *
 * Ownership model:
 * ────────────────────────────────────────────────────────────────────────────
 * The vector pointer is borrowed — the function does not take ownership.
 * borrowed() from core/ownership.h is available transitively via vec_impl.h.
 * The range is passed by value — no ownership annotation applies.
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
 * Usage — extend a vec_int with values from a range:
 * ```c
 * #include "data/vec/vec.h"
 * #include "data/vec/vec_range.h"
 *
 * DEFINE_VEC(static inline, int)
 * DEFINE_VEC_RANGE(static inline, int)
 *
 * int buf[16];
 * vec_int v = vec_int_init(buf, 16);
 *
 * range r = range_make(0, 10, 1);  // 0..9 ascending
 * result__Bool_Error res = vec_int_extend_from_range(&v, r);
 * // v now contains [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
 *
 * range desc = range_make(5, 0, -1);  // 5..1 descending
 * vec_int_extend_from_range(&v, desc);
 * // appends [5, 4, 3, 2, 1]
 *
 * range stepped = range_make(0, 20, 3);  // stepped by 3
 * vec_int_extend_from_range(&v, stepped);
 * // appends [0, 3, 6, 9, 12, 15, 18]
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
 * result__Bool_Error fn(borrowed(VecType*) v, range r);
 * ```
 *
 * @pre v != NULL
 * @pre v->items != NULL
 * @pre The range element count must not overflow usize (checked via range_len)
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
 * @note Full range semantics are honoured — ascending, descending, and stepped
 *       ranges all work correctly via range_len(), range_has_next(), range_next().
 *
 * Performance:
 * - Time:  O(count) where count = range_len(&r)
 * - Space: O(1) — no allocation, fills into existing buffer
 */
#define IMPL_VEC_EXTEND_FROM_RANGE(linkage, VecType, fn, type) \
linkage result__Bool_Error fn(borrowed(VecType*) v, range r) { \
    if (!v || !v->items) { return result__Bool_Error_err(ERR_INVALID_ARG); } \
    usize count = range_len(&r); \
    usize total; \
    if (!checked_add(v->len, count, &total)) { \
        return result__Bool_Error_err(ERR_OVERFLOW); \
    } \
    if (total > v->capacity) { \
        return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    } \
    for (usize i = 0; range_has_next(&r); i++) { \
        v->items[v->len + i] = (type)range_next(&r); \
    } \
    v->len = total; \
    return result__Bool_Error_ok(true); \
}

/* ════════════════════════════════════════════════════════════════════════════
   MANGLE_VEC_EXTEND_FROM_RANGE — name mangling macro
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the extend_from_range function for a given element type
 *
 * Default: vec_##type##_extend_from_range
 * Override before including this file if custom naming is needed.
 */
#ifndef MANGLE_VEC_EXTEND_FROM_RANGE
    #define MANGLE_VEC_EXTEND_FROM_RANGE(type) vec_##type##_extend_from_range
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
 * - vec_##type##_extend_from_range(v, r) → result__Bool_Error
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
 * // result__Bool_Error vec_int_extend_from_range(borrowed(vec_int*) v, range r);
 * ```
 *
 * @note Only makes semantic sense for numeric element types (int, float, i32, etc.)
 *       The range values are cast from isize — ensure the element type can hold them.
 */
#define DEFINE_VEC_RANGE(linkage, type) \
    IMPL_VEC_EXTEND_FROM_RANGE(linkage, MANGLE_VEC_TYPE(type), MANGLE_VEC_EXTEND_FROM_RANGE(type), type)

#endif /* CANON_DATA_VEC_RANGE_H */
