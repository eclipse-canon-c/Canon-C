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
/* VERIFY-028 F5: the loop is bounded by the SAME count the capacity check
 * used, never by range_has_next. Before, a wrong range_len (F1: 0 for any
 * range wider than ISIZE_MAX) passed the capacity check and the loop then
 * wrote every element — a buffer overflow (ASan: 3 elements into capacity
 * 1). F1 makes the count exact; this bound makes the capacity check and the
 * writes agree by construction, whatever range_len returns. */
/* VERIFY-041: the contract and the loop's annotation live in the macro body,
 * as IMPL_VEC_FILL's do. They are comments, so they name no macro parameter:
 * the preprocessor substitutes none inside a comment. They reach the prover
 * only under -CC; frama-c-vec-range preprocesses so. The one clause that must
 * name the element type is on a prototype in vmacros/vdrivers/
 * vec_range_verify.h, at int. Run 1 verifies this contract on the code as it
 * stood: VERIFY-041's F1 and F2 are not yet fixed here. */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define IMPL_VEC_EXTEND_FROM_RANGE(linkage, VecType, fn, type)                 \
/*@ requires v == \null || ((\valid_read(v) && v->len <= v->capacity && v->capacity <= CANON_VEC_MAX_CAPACITY / sizeof(*v->items) && (v->items == \null ==> v->capacity == 0) && (v->capacity > 0 ==> \valid_read(v->items + (0 .. v->capacity - 1)))) && \valid(v) && (v->capacity > 0 ==> \valid(v->items + (0 .. v->capacity - 1))) && \separated(v, v->items + (0 .. v->capacity - 1))); \
    assigns v->len, v->items[0 .. v->capacity - 1];                            \
    behavior invalid:                                                          \
    assumes v == \null || (v != \null && v->items == \null)                    \
    || r.step == CANON_ISIZE_MIN;                                              \
    assigns \nothing;                                                          \
    ensures \result.is_ok == \false && \result.val.err == ERR_INVALID_ARG;     \
    behavior overflow:                                                         \
    assumes v != \null && v->items != \null && r.step != CANON_ISIZE_MIN       \
    && v->len + range_count(r) > CANON_USIZE_MAX;                              \
    assigns \nothing;                                                          \
    ensures \result.is_ok == \false && \result.val.err == ERR_OVERFLOW;        \
    behavior too_big:                                                          \
    assumes v != \null && v->items != \null && r.step != CANON_ISIZE_MIN       \
    && v->len + range_count(r) <= CANON_USIZE_MAX                              \
    && v->len + range_count(r) > v->capacity;                                  \
    assigns \nothing;                                                          \
    ensures \result.is_ok == \false && \result.val.err == ERR_CAPACITY_EXCEEDED; \
    behavior room:                                                             \
    assumes v != \null && v->items != \null && r.step != CANON_ISIZE_MIN       \
    && v->len + range_count(r) <= v->capacity;                                 \
    assigns v->len, v->items[v->len .. v->capacity - 1];                       \
    ensures \result.is_ok ==> v->len == \old(v->len) + range_count(r);         \
    ensures \result.is_ok ==> \forall integer k; 0 <= k < range_count(r)       \
    ==> v->items[\old(v->len) + k] == r.current + k * r.step;                  \
    ensures !\result.is_ok ==> \result.val.err == ERR_OUT_OF_RANGE             \
    && v->len == \old(v->len);                                                 \
    ensures \forall integer k; 0 <= k < \old(v->len)                           \
    ==> v->items[k] == \old(v->items[k]);                                      \
    complete behaviors;                                                        \
    disjoint behaviors; */                                                     \
linkage result__Bool_Error fn(borrowed(VecType*) v, range r) {                 \
    if (!v || !v->items) { return result__Bool_Error_err(ERR_INVALID_ARG); }   \
    usize count = range_len(&r);                                               \
    usize total;                                                               \
    if (!checked_add(v->len, count, &total)) {                                 \
        return result__Bool_Error_err(ERR_OVERFLOW);                           \
    }                                                                          \
    if (total > v->capacity) {                                                 \
        return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED);                  \
    }                                                                          \
    /*@ loop invariant 0 <= i <= count;                                        \
        loop invariant r.end == \at(r.end, Pre) && r.step == \at(r.step, Pre); \
        loop invariant range_count(r) == count - i;                            \
        loop invariant i < count ==> range_wf(r)                               \
        && r.current == \at(r.current, Pre) + i * r.step;                      \
        loop invariant \forall integer k; 0 <= k < i                           \
        ==> v->items[v->len + k] == \at(r.current, Pre) + k * r.step;          \
        loop assigns i, r.current, v->items[v->len .. v->len + count - 1];     \
        loop variant count - i; */                                             \
    for (usize i = 0; i < count; i++) {                                        \
        v->items[v->len + i] = (type)range_next(&r);                           \
    }                                                                          \
    v->len = total;                                                            \
    return result__Bool_Error_ok(true);                                        \
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
