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
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
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

/** @brief Ascending u8 comparator */
static inline int algo_cmp_u8(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u8 x = *(const u8*)a, y = *(const u8*)b;
    return (x > y) - (x < y);
}

/** @brief Descending u8 comparator */
static inline int algo_cmp_u8_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u8(b, a, ctx);
}

/** @brief Ascending u16 comparator */
static inline int algo_cmp_u16(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u16 x = *(const u16*)a, y = *(const u16*)b;
    return (x > y) - (x < y);
}

/** @brief Descending u16 comparator */
static inline int algo_cmp_u16_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u16(b, a, ctx);
}

/** @brief Ascending u32 comparator */
static inline int algo_cmp_u32(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u32 x = *(const u32*)a, y = *(const u32*)b;
    return (x > y) - (x < y);
}

/** @brief Descending u32 comparator */
static inline int algo_cmp_u32_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u32(b, a, ctx);
}

/** @brief Ascending u64 comparator */
static inline int algo_cmp_u64(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u64 x = *(const u64*)a, y = *(const u64*)b;
    return (x > y) - (x < y);
}

/** @brief Descending u64 comparator */
static inline int algo_cmp_u64_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_u64(b, a, ctx);
}

/* ════════════════════════════════════════════════════════════════════════════
   Signed integer comparators
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Ascending i8 comparator */
static inline int algo_cmp_i8(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i8 x = *(const i8*)a, y = *(const i8*)b;
    return (x > y) - (x < y);
}

/** @brief Descending i8 comparator */
static inline int algo_cmp_i8_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i8(b, a, ctx);
}

/** @brief Ascending i16 comparator */
static inline int algo_cmp_i16(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i16 x = *(const i16*)a, y = *(const i16*)b;
    return (x > y) - (x < y);
}

/** @brief Descending i16 comparator */
static inline int algo_cmp_i16_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i16(b, a, ctx);
}

/** @brief Ascending i32 comparator */
static inline int algo_cmp_i32(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i32 x = *(const i32*)a, y = *(const i32*)b;
    return (x > y) - (x < y);
}

/** @brief Descending i32 comparator */
static inline int algo_cmp_i32_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i32(b, a, ctx);
}

/** @brief Ascending i64 comparator */
static inline int algo_cmp_i64(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i64 x = *(const i64*)a, y = *(const i64*)b;
    return (x > y) - (x < y);
}

/** @brief Descending i64 comparator */
static inline int algo_cmp_i64_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_i64(b, a, ctx);
}

/* ════════════════════════════════════════════════════════════════════════════
   Size and pointer-difference comparators
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Ascending usize comparator */
static inline int algo_cmp_usize(const void* a, const void* b, void* ctx) {
    (void)ctx;
    usize x = *(const usize*)a, y = *(const usize*)b;
    return (x > y) - (x < y);
}

/** @brief Descending usize comparator */
static inline int algo_cmp_usize_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_usize(b, a, ctx);
}

/** @brief Ascending isize comparator */
static inline int algo_cmp_isize(const void* a, const void* b, void* ctx) {
    (void)ctx;
    isize x = *(const isize*)a, y = *(const isize*)b;
    return (x > y) - (x < y);
}

/** @brief Descending isize comparator */
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
 */
static inline int algo_cmp_f64_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_f64(b, a, ctx);
}

#endif /* CANON_CORE_PRIMITIVES_COMPARE_H */
