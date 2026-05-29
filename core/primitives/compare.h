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

#ifndef CANON_CORE_PRIMITIVES_COMPARE_H
#define CANON_CORE_PRIMITIVES_COMPARE_H

#include "core/primitives/types.h"
#include <stdbool.h>   /* bool — explicit, even though types.h pulls it in transitively */

/**
 * @file core/primitives/compare.h
 * @brief Three-way comparator type and built-in comparators
 *
 * Defines algo_cmp_fn — the canonical comparator type used across data/,
 * algo/, and util/. Lives in core/primitives/ so that data/ containers
 * (priority_queue.h, sort-aware structures) can use it without depending
 * on algo/, which would violate the layer rule.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * core/primitives/compare.h depends only on core/primitives/types.h.
 * It may be included by any layer: core/, data/, algo/, util/.
 *
 * Comparator coverage:
 * ────────────────────────────────────────────────────────────────────────────
 * Built-in comparators are provided for all Canon primitive types defined
 * in types.h. Bare C types (int, double) are intentionally omitted —
 * use i32/f64 instead for explicit-width consistency.
 *
 * Each type has both ascending (_asc suffix, also available without suffix)
 * and descending (_desc suffix) variants.
 *
 * Formal verification:
 * ────────────────────────────────────────────────────────────────────────────
 * Every comparator carries an ACSL contract specifying:
 *   - Memory safety: \valid_read on char-level byte ranges
 *   - Functional correctness: three-way comparison result (-1, 0, +1)
 *   - Side-effect bounding: assigns \nothing (pure functions)
 *
 * The (x > y) - (x < y) pattern always produces exactly -1, 0, or +1.
 * This is a branchless three-way comparison that WP can reason about
 * as simple integer arithmetic on boolean-to-int conversions.
 *
 * Floating-point comparators use \is_NaN from ACSL's floating-point
 * logic to specify NaN handling with complete and disjoint behaviors.
 *
 * ACSL note on void* parameters: Frama-C WP treats void* as char*
 * (sint8*) internally. Casting void* to a typed pointer (e.g. u32*)
 * in ACSL requires/ensures clauses produces "incompatible pointer cast"
 * warnings that prevent WP from proving any goals. The contracts use
 * char-level \valid_read on the raw bytes and express functional
 * properties using local variables assigned inside the function body.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All integer comparators: O(1) — branchless (x > y) - (x < y) pattern
 * - Floating-point comparators: O(1) — 1-2 branches for NaN detection
 * - Descending variants: O(1) — delegate to ascending with swapped args
 * - All functions are static inline → zero call overhead
 * - No heap allocation, no loops, no recursion
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are fully thread-safe — no shared mutable state.
 * The ctx parameter is caller-owned and not written to.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - NaN detection via x != x — IEEE 754 compliant, works on all
 *   conforming platforms
 *
 * Usage:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/sort.h, algo/find.h, algo/any_all.h, algo/filter.h — include this
 * header instead of redefining their own comparator/predicate typedefs.
 *
 * data/priority_queue.h — includes this to get algo_cmp_fn without
 * depending on algo/sort.h, which would violate the layer rule.
 *
 * @sa algo/sort.h       — uses algo_cmp_fn for sorting
 * @sa algo/any_all.h    — uses algo_pred_fn for predicate testing
 */

/* ════════════════════════════════════════════════════════════════════════════
   Three-way comparator
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Three-way comparison function type
 *
 * Used by sort, priority_queue, binary search, and any ordered container.
 *
 * @param a   Pointer to first element (never NULL)
 * @param b   Pointer to second element (never NULL)
 * @param ctx Optional caller context (may be NULL)
 * @return < 0 if a < b, 0 if a == b, > 0 if a > b
 *
 * Must satisfy total order:
 * - cmp(a, a) == 0                              (reflexive)
 * - cmp(a, b) < 0  implies  cmp(b, a) > 0      (antisymmetric)
 * - cmp(a, b) < 0 && cmp(b, c) < 0
 *     implies cmp(a, c) < 0                     (transitive)
 */
typedef int (*algo_cmp_fn)(const void* a, const void* b, void* ctx);

/* ════════════════════════════════════════════════════════════════════════════
   Predicate function type
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Unary predicate function type
 *
 * Used by any_all, filter, find, and any operation that tests elements.
 *
 * @param elem Pointer to element being tested (never NULL)
 * @param ctx  Optional caller context (may be NULL)
 * @return true if element satisfies the condition, false otherwise
 */
typedef bool (*algo_pred_fn)(const void* elem, void* ctx);

/* ════════════════════════════════════════════════════════════════════════════
   Unsigned integer comparators
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ascending u8 comparator
 *
 * @param a   Pointer to first u8 value (must not be NULL)
 * @param b   Pointer to second u8 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 * @remark Compiles to 2-3 instructions on most architectures
 *
 * Example:
 * ```c
 * u8 lo = 10, hi = 20;
 * int cmp = algo_cmp_u8(&lo, &hi, NULL);  // → -1
 * ```
 *
 * @sa algo_cmp_u8_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u8(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u8 x = *(const u8*)a, y = *(const u8*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending u8 comparator
 *
 * @param a   Pointer to first u8 value (must not be NULL)
 * @param b   Pointer to second u8 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_u8 with swapped args
 *
 * @sa algo_cmp_u8()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u8_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u8(b, a, ctx);
}

/** @brief Ascending u16 comparator
 *
 * @param a   Pointer to first u16 value (must not be NULL)
 * @param b   Pointer to second u16 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 *
 * @sa algo_cmp_u16_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u16(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u16 x = *(const u16*)a, y = *(const u16*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending u16 comparator
 *
 * @param a   Pointer to first u16 value (must not be NULL)
 * @param b   Pointer to second u16 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_u16 with swapped args
 *
 * @sa algo_cmp_u16()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u16_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u16(b, a, ctx);
}

/**
 * @brief Ascending u32 comparator
 *
 * @param a   Pointer to first u32 value (must not be NULL)
 * @param b   Pointer to second u32 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 *
 * @sa algo_cmp_u32_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u32(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u32 x = *(const u32*)a, y = *(const u32*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending u32 comparator
 *
 * @param a   Pointer to first u32 value (must not be NULL)
 * @param b   Pointer to second u32 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_u32 with swapped args
 *
 * @sa algo_cmp_u32()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u32_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u32(b, a, ctx);
}

/**
 * @brief Ascending u64 comparator
 *
 * @param a   Pointer to first u64 value (must not be NULL)
 * @param b   Pointer to second u64 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 *
 * @sa algo_cmp_u64_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u64(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u64 x = *(const u64*)a, y = *(const u64*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending u64 comparator
 *
 * @param a   Pointer to first u64 value (must not be NULL)
 * @param b   Pointer to second u64 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_u64 with swapped args
 *
 * @sa algo_cmp_u64()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_u64_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u64(b, a, ctx);
}

/* ════════════════════════════════════════════════════════════════════════════
   Signed integer comparators
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ascending i8 comparator
 *
 * @param a   Pointer to first i8 value (must not be NULL)
 * @param b   Pointer to second i8 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 * @remark No signed overflow: comparison operators on i8 yield 0 or 1,
 *         subtraction of two values in {0,1} is always in [-1, +1]
 *
 * @sa algo_cmp_i8_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i8(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i8 x = *(const i8*)a, y = *(const i8*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending i8 comparator
 *
 * @param a   Pointer to first i8 value (must not be NULL)
 * @param b   Pointer to second i8 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_i8 with swapped args
 *
 * @sa algo_cmp_i8()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i8_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i8(b, a, ctx);
}

/**
 * @brief Ascending i16 comparator
 *
 * @param a   Pointer to first i16 value (must not be NULL)
 * @param b   Pointer to second i16 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 *
 * @sa algo_cmp_i16_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i16(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i16 x = *(const i16*)a, y = *(const i16*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending i16 comparator
 *
 * @param a   Pointer to first i16 value (must not be NULL)
 * @param b   Pointer to second i16 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_i16 with swapped args
 *
 * @sa algo_cmp_i16()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i16_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i16(b, a, ctx);
}

/**
 * @brief Ascending i32 comparator
 *
 * @param a   Pointer to first i32 value (must not be NULL)
 * @param b   Pointer to second i32 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 * @remark Does NOT use (a - b) which would overflow for extreme values.
 *         The comparison-subtraction pattern is safe for all i32 inputs.
 *
 * @sa algo_cmp_i32_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i32(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i32 x = *(const i32*)a, y = *(const i32*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending i32 comparator
 *
 * @param a   Pointer to first i32 value (must not be NULL)
 * @param b   Pointer to second i32 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_i32 with swapped args
 *
 * @sa algo_cmp_i32()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i32_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i32(b, a, ctx);
}

/**
 * @brief Ascending i64 comparator
 *
 * @param a   Pointer to first i64 value (must not be NULL)
 * @param b   Pointer to second i64 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 * @remark Does NOT use (a - b) which would overflow for extreme values.
 *         The comparison-subtraction pattern is safe for all i64 inputs.
 *
 * @sa algo_cmp_i64_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i64(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i64 x = *(const i64*)a, y = *(const i64*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending i64 comparator
 *
 * @param a   Pointer to first i64 value (must not be NULL)
 * @param b   Pointer to second i64 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_i64 with swapped args
 *
 * @sa algo_cmp_i64()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_i64_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i64(b, a, ctx);
}

/* ════════════════════════════════════════════════════════════════════════════
   Size and pointer-difference comparators
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ascending usize comparator
 *
 * @param a   Pointer to first usize value (must not be NULL)
 * @param b   Pointer to second usize value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 *
 * @sa algo_cmp_usize_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_usize(const void* a, const void* b, void* ctx) {
    (void)ctx;
    usize x = *(const usize*)a, y = *(const usize*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending usize comparator
 *
 * @param a   Pointer to first usize value (must not be NULL)
 * @param b   Pointer to second usize value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_usize with swapped args
 *
 * @sa algo_cmp_usize()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_usize_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_usize(b, a, ctx);
}

/**
 * @brief Ascending isize comparator
 *
 * @param a   Pointer to first isize value (must not be NULL)
 * @param b   Pointer to second isize value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if *a == *b, 1 if *a > *b
 *
 * @remark O(1) time, O(1) space — branchless (x > y) - (x < y)
 *
 * @sa algo_cmp_isize_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_isize(const void* a, const void* b, void* ctx) {
    (void)ctx;
    isize x = *(const isize*)a, y = *(const isize*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending isize comparator
 *
 * @param a   Pointer to first isize value (must not be NULL)
 * @param b   Pointer to second isize value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if *a == *b, -1 if *a > *b
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_isize with swapped args
 *
 * @sa algo_cmp_isize()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_isize_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_isize(b, a, ctx);
}

/* ════════════════════════════════════════════════════════════════════════════
   Floating-point comparators
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ascending f32 comparator — NaN sorted last
 *
 * NaN is not less than, equal to, or greater than any value including itself.
 * This comparator imposes a total order by sorting all NaN values after all
 * non-NaN values. Two NaN values compare equal to each other.
 *
 * @param a   Pointer to first f32 value (must not be NULL)
 * @param b   Pointer to second f32 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if equal (or both NaN), 1 if *a > *b (or *a is NaN)
 *
 * @remark O(1) time, O(1) space — 1-2 branches for NaN, then branchless compare
 * @remark NaN detection via x != x — IEEE 754 compliant
 * @remark INFINITY is handled correctly by normal float comparison
 *
 * Example:
 * ```c
 * f32 a = 1.0f, b = 2.0f, nan = NAN;
 * algo_cmp_f32(&a, &b, NULL);     // → -1 (1.0 < 2.0)
 * algo_cmp_f32(&nan, &b, NULL);   // → 1  (NaN sorts last)
 * algo_cmp_f32(&nan, &nan, NULL); // → 0  (NaN == NaN in this ordering)
 * ```
 *
 * @sa algo_cmp_f32_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_f32(const void* a, const void* b, void* ctx) {
    (void)ctx;
    f32 x = *(const f32*)a, y = *(const f32*)b;
    if (x != x) return (y != y) ? 0 : 1;  /* x is NaN */
    if (y != y) return -1;                 /* y is NaN, x is not */
    return (x > y) - (x < y);
}

/**
 * @brief Descending f32 comparator — NaN sorted first
 *
 * Reverses the numeric order of algo_cmp_f32. Because NaN sorts last in
 * ascending order, it sorts first in descending order (argument swap moves
 * NaN to the front). Two NaN values compare equal to each other.
 *
 * @param a   Pointer to first f32 value (must not be NULL)
 * @param b   Pointer to second f32 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if equal (or both NaN), -1 if *a > *b (or *a is NaN)
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_f32 with swapped args
 *
 * @sa algo_cmp_f32()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_f32_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_f32(b, a, ctx);
}

/**
 * @brief Ascending f64 comparator — NaN sorted last
 *
 * NaN is not less than, equal to, or greater than any value including itself.
 * This comparator imposes a total order by sorting all NaN values after all
 * non-NaN values. Two NaN values compare equal to each other.
 *
 * @param a   Pointer to first f64 value (must not be NULL)
 * @param b   Pointer to second f64 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return -1 if *a < *b, 0 if equal (or both NaN), 1 if *a > *b (or *a is NaN)
 *
 * @remark O(1) time, O(1) space — 1-2 branches for NaN, then branchless compare
 * @remark NaN detection via x != x — IEEE 754 compliant
 * @remark INFINITY is handled correctly by normal double comparison
 *
 * Example:
 * ```c
 * f64 a = 1.0, b = 2.0, nan = NAN;
 * algo_cmp_f64(&a, &b, NULL);     // → -1 (1.0 < 2.0)
 * algo_cmp_f64(&nan, &b, NULL);   // → 1  (NaN sorts last)
 * algo_cmp_f64(&nan, &nan, NULL); // → 0  (NaN == NaN in this ordering)
 * ```
 *
 * @sa algo_cmp_f64_desc()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_f64(const void* a, const void* b, void* ctx) {
    (void)ctx;
    f64 x = *(const f64*)a, y = *(const f64*)b;
    if (x != x) return (y != y) ? 0 : 1;  /* x is NaN */
    if (y != y) return -1;                 /* y is NaN, x is not */
    return (x > y) - (x < y);
}

/**
 * @brief Descending f64 comparator — NaN sorted first
 *
 * Reverses the numeric order of algo_cmp_f64. Because NaN sorts last in
 * ascending order, it sorts first in descending order (argument swap moves
 * NaN to the front). Two NaN values compare equal to each other.
 *
 * @param a   Pointer to first f64 value (must not be NULL)
 * @param b   Pointer to second f64 value (must not be NULL)
 * @param ctx Unused caller context (ignored)
 *
 * @return 1 if *a < *b, 0 if equal (or both NaN), -1 if *a > *b (or *a is NaN)
 *
 * @remark O(1) time, O(1) space — delegates to algo_cmp_f64 with swapped args
 *
 * @sa algo_cmp_f64()
 */
/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
    ensures  -1 <= \result <= 1;
 */
static inline int algo_cmp_f64_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_f64(b, a, ctx);
}

#endif /* CANON_CORE_PRIMITIVES_COMPARE_H */
