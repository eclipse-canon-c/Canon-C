// data/range.h
#ifndef CANON_DATA_RANGE_H
#define CANON_DATA_RANGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>   // for SIZE_MAX
#include <limits.h>   // for INT_MAX/MIN if signed ranges used

/**
 * @file range.h
 * @brief Explicit, bounded integer range generator (iterator-style)
 *
 * Generates sequential integers with full control over start, end, and step.
 * Supports ascending/descending, signed/unsigned semantics.
 *
 *                           CRITICAL DESIGN NOTE
 * ──────────────────────────────────────────────────────────────────────────────
 * FIXED & BOUNDED – NO GROWTH, NO DYNAMIC BEHAVIOR
 *
 * • This is **not** an infinite generator
 * • The range is **completely determined** at creation (start, end, step)
 * • It has a **fixed number of elements** (or zero)
 * • It will **never** grow, extend, or change bounds during iteration
 * • Empty ranges are perfectly valid (e.g. start >= end with positive step)
 *
 * This design ensures:
 *   - Zero allocations, zero hidden state
 *   - Maximum predictability
 *   - Safe against off-by-one errors
 *   - Excellent for pre-allocation, progress bars, bounds checking
 *
 * If you need an infinite sequence or dynamic growth → this is not the right tool.
 *
 *                           Semantics Summary
 * ──────────────────────────────────────────────────────────────────────────────
 * • [start, end) pattern → start inclusive, end exclusive (like C++ std::ranges)
 * • step > 0  → ascending  (0..9 with step 1 → 0,1,2,...,8)
 * • step < 0  → descending (10..0 with step -1 → 10,9,...,1)
 * • step == 0 → normalized to +1 during creation
 * • Overflow-safe length estimation (returns SIZE_MAX when too large)
 *
 *                           Recommended Usage Patterns
 * ──────────────────────────────────────────────────────────────────────────────
 *
 * // 1. Classic ascending range
 * RANGE_FOR(int i, range_make(0, 10, 1)) {
 *     printf("%d ", i);  // prints: 0 1 2 3 4 5 6 7 8 9
 * }
 *
 * // 2. Descending with step
 * RANGE_FOR(int i, range_make(20, 5, -3)) {
 *     // 20, 17, 14, 11, 8
 * }
 *
 * // 3. Pre-allocate array based on range length
 * size_t count = range_len(&r);
 * if (count == SIZE_MAX) { /* too big - handle error */ }
 * int* array = malloc(sizeof(int) * count);
 */

typedef struct {
    ptrdiff_t current;  ///< Next value to be returned by range_next()
    ptrdiff_t end;      ///< Exclusive bound (iteration stops when current reaches/crosses this)
    ptrdiff_t step;     ///< Step size (positive = ascending, negative = descending)
} range;

/* ────────────────────────────────────────────────────────────────────────────
   Construction
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Creates a range [start, end) with given step
 * @param start Starting value (inclusive)
 * @param end   Exclusive end bound
 * @param step  Increment/decrement (0 is normalized to +1)
 * @return Initialized range ready for iteration
 *
 * Empty ranges are valid (e.g. start=5, end=3, step=1 → empty)
 */
static inline range range_make(ptrdiff_t start, ptrdiff_t end, ptrdiff_t step) {
    if (step == 0) step = 1;
    return (range){
        .current = start,
        .end = end,
        .step = step
    };
}

static inline range range_upto(ptrdiff_t end) {
    return range_make(0, end, 1);
}

static inline range range_from_to(ptrdiff_t start, ptrdiff_t end) {
    return range_make(start, end, 1);
}

static inline range range_downfrom(ptrdiff_t start) {
    return range_make(start, 0, -1);
}

/* ────────────────────────────────────────────────────────────────────────────
   Queries
   ──────────────────────────────────────────────────────────────────────────── */

static inline bool range_is_empty(const range* r) {
    if (!r) return true;
    if (r->step > 0) return r->current >= r->end;
    if (r->step < 0) return r->current <= r->end;
    return true;  // step == 0 should not happen
}

static inline bool range_has_next(const range* r) {
    return !range_is_empty(r);
}

/**
 * @brief Estimates the number of remaining elements (safe against overflow)
 * @return Exact count when possible, SIZE_MAX if too large to represent
 */
static inline size_t range_len(const range* r) {
    if (!r || range_is_empty(r)) return 0;
    if (r->step > 0) {
        if (r->end - r->current > (ptrdiff_t)SIZE_MAX) return SIZE_MAX;
        return (size_t)((r->end - r->current - 1) / r->step + 1);
    } else {
        if (r->current - r->end > (ptrdiff_t)SIZE_MAX) return SIZE_MAX;
        return (size_t)((r->current - r->end - 1) / -r->step + 1);
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Iteration
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns next value and advances the iterator
 * @warning Undefined behavior if called when range_has_next() == false
 */
static inline ptrdiff_t range_next(range* r) {
    if (!r || range_is_empty(r)) return 0; // safety fallback (should not be used)
    ptrdiff_t value = r->current;

    if (r->step > 0) {
        if (r->current > r->end - r->step) {
            r->current = r->end; // exhaust
        } else {
            r->current += r->step;
        }
    } else {
        if (r->current < r->end - r->step) {
            r->current = r->end;
        } else {
            r->current += r->step;
        }
    }
    return value;
}

static inline void range_reset(range* r, ptrdiff_t new_start) {
    if (r) r->current = new_start;
}

/* ────────────────────────────────────────────────────────────────────────────
   For-loop integration (recommended)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Clean for-loop syntax for range iteration
 * @example:
 *     int i;
 *     RANGE_FOR(i, range_make(1, 10, 2)) {
 *         // i = 1,3,5,7,9
 *     }
 */
#define RANGE_FOR(var, r_expr) \
    for (range _r = (r_expr), *_rp = &_r; \
         range_has_next(_rp) && ((var) = range_next(_rp), true); )

#endif /* CANON_DATA_RANGE_H */
