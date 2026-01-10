// data/range.h
#ifndef CANON_DATA_RANGE_H
#define CANON_DATA_RANGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>   // for SIZE_MAX
#include <limits.h>   // for INT_MAX/MIN if signed ranges used

/**
 * @file range.h
 * @brief Explicit integer range generator (iterator-style)
 *
 * Generates sequential integers with full control over start/end/step.
 * Supports both ascending and descending ranges, signed/unsigned semantics.
 *
 * Key properties:
 *   - No allocation, no hidden state, no ownership
 *   - Bounded and explicit (safe against off-by-one errors)
 *   - Zero-cost abstraction (just a small struct + inline functions)
 *   - Safe overflow handling in length estimation
 *   - Designed for clean, readable loops without manual index management
 *
 * Typical usage:
 *   range r = range_make(0, 10, 1);      // 0..9
 *   ptrdiff_t i;
 *   while (range_has_next(&r)) {
 *       i = range_next(&r);
 *       // use i
 *   }
 *
 * Or with macro for nicer for-loop syntax:
 *   int i;
 *   RANGE_FOR(i, range_make(5, 0, -1)) {
 *       printf("%d ", i);  // 5 4 3 2 1
 *   }
 */

/**
 * @brief Opaque iterator for generating sequential integers
 *
 * Fields are public for inspection, but should be manipulated only via functions.
 * - current: next value to be returned
 * - end:     exclusive upper/lower bound
 * - step:    increment/decrement (positive or negative, non-zero)
 */
typedef struct {
    ptrdiff_t current;  ///< Next value to be returned by range_next()
    ptrdiff_t end;      ///< Exclusive bound (range stops when current reaches/crosses this)
    ptrdiff_t step;     ///< Step size (positive for ascending, negative for descending)
} range;

/* ────────────────────────────────────────────────────────────────────────────
   Construction
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Creates a range [start, end) with given step
 *
 * Normalizes step == 0 to +1.
 * Empty ranges are possible (e.g. start >= end with positive step).
 *
 * @param start  Starting value (inclusive)
 * @param end    Exclusive end bound
 * @param step   Increment/decrement (0 becomes +1)
 * @return       Initialized range ready for iteration
 */
static inline range range_make(ptrdiff_t start, ptrdiff_t end, ptrdiff_t step) {
    if (step == 0) step = 1;
    return (range){
        .current = start,
        .end     = end,
        .step    = step
    };
}

/**
 * @brief Creates range [0, end) with step +1
 */
static inline range range_upto(ptrdiff_t end) {
    return range_make(0, end, 1);
}

/**
 * @brief Creates range [start, end) with step +1
 */
static inline range range_from_to(ptrdiff_t start, ptrdiff_t end) {
    return range_make(start, end, 1);
}

/**
 * @brief Creates descending range [start, 0) with step -1
 */
static inline range range_downfrom(ptrdiff_t start) {
    return range_make(start, 0, -1);
}

/* ────────────────────────────────────────────────────────────────────────────
   Queries
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if the range is exhausted (no more values)
 */
static inline bool range_is_empty(const range* r) {
    if (!r) return true;
    if (r->step > 0) return r->current >= r->end;
    if (r->step < 0) return r->current <= r->end;
    return true;  // step == 0 should not happen after construction
}

/**
 * @brief Checks if there is at least one more value available
 */
static inline bool range_has_next(const range* r) {
    return !range_is_empty(r);
}

/**
 * @brief Estimates the number of remaining elements (safe against overflow)
 *
 * Returns SIZE_MAX if the range is too large to represent exactly.
 * Useful for pre-allocation or progress tracking.
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
 * @brief Returns the next value and advances the iterator
 *
 * Undefined behavior if called when range_is_empty() == true.
 * On last value: returns it and exhausts the range.
 *
 * @param r Valid range (must have next value)
 * @return  Next integer in sequence
 */
static inline ptrdiff_t range_next(range* r) {
    if (!r || range_is_empty(r)) return 0;  // safety fallback

    ptrdiff_t value = r->current;

    if (r->step > 0) {
        if (r->current > r->end - r->step) {
            r->current = r->end;  // exhaust
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

/**
 * @brief Resets the range iterator to a new starting point
 *
 * Useful for reusing the same range struct multiple times.
 *
 * @param r     Valid range
 * @param start New starting value
 */
static inline void range_reset(range* r, ptrdiff_t start) {
    if (r) r->current = start;
}

/* ────────────────────────────────────────────────────────────────────────────
   For-loop integration macro (recommended for clean loops)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Macro for clean for-loop style iteration over a range
 *
 * Declares a local range copy and iterates until exhausted.
 * Assigns each value to the given variable.
 *
 * Example:
 *   int i;
 *   RANGE_FOR(i, range_make(0, 10, 2)) {
 *       printf("%d ", i);  // 0 2 4 6 8
 *   }
 */
#define RANGE_FOR(var, r_expr) \
    for (range _r = (r_expr), *_rp = &_r; \
         range_has_next(_rp) && ((var) = range_next(_rp), 1); )

#endif /* CANON_DATA_RANGE_H */
