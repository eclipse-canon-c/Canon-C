#ifndef CANON_CORE_PRIMITIVES_COMPARE_H
#define CANON_CORE_PRIMITIVES_COMPARE_H

#include "core/primitives/types.h"

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
   Built-in comparators
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Ascending int comparator */
static inline int algo_cmp_int(const void* a, const void* b, void* ctx) {
    (void)ctx;
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);
}

/** @brief Descending int comparator */
static inline int algo_cmp_int_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_int(b, a, ctx);
}

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

/** @brief Ascending double comparator — NaN sorted last */
static inline int algo_cmp_double(const void* a, const void* b, void* ctx) {
    (void)ctx;
    double x = *(const double*)a, y = *(const double*)b;
    if (x != x) return (y != y) ? 0 : 1;  /* x is NaN */
    if (y != y) return -1;                 /* y is NaN, x is not */
    return (x > y) - (x < y);
}

/** @brief Descending double comparator — NaN sorted last */
static inline int algo_cmp_double_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_double(b, a, ctx);
}

/** @brief Ascending u8 comparator */
static inline int algo_cmp_u8(const void* a, const void* b, void* ctx) {
    (void)ctx;
    u8 x = *(const u8*)a, y = *(const u8*)b;
    return (x > y) - (x < y);
}

/** @brief Ascending i64 comparator */
static inline int algo_cmp_i64(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i64 x = *(const i64*)a, y = *(const i64*)b;
    return (x > y) - (x < y);
}

#endif /* CANON_CORE_PRIMITIVES_COMPARE_H */
