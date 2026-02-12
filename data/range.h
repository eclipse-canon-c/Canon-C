#ifndef CANON_DATA_RANGE_H
#define CANON_DATA_RANGE_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "semantics/option.h"

/**
 * @file range.h
 * @brief Explicit bounded integer range generator (iterator-style)
 *
 * Generates sequential integers with full control over start, end, and step.
 * Supports ascending, descending, and stepped iteration over signed ranges.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Bounded — start, end, and step are fixed at creation
 * - Not an infinite generator — iteration count is always pre-calculable
 * - [start, end) semantics — start inclusive, end exclusive (Python/C++ style)
 * - step > 0: ascending; step < 0: descending; step == 0: normalized to +1
 * - Overflow-safe: advances saturate to end rather than wrapping
 * - Empty ranges are valid and safe (e.g. range_make(5, 3, 1))
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses isize (ptrdiff_t equivalent) and usize from primitives/types.h
 * - No platform-specific code
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each range instance is independent — safe to use from multiple threads
 * if each thread has its own instance.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations:  O(1)
 * - range_len():     O(1) — pure arithmetic, no iteration
 * - range_skip(n):   O(n) — iterates n steps
 * - No allocations anywhere
 *
 * Semantics summary:
 * ────────────────────────────────────────────────────────────────────────────
 * - range_make(0, 10, 1)  → 0, 1, 2, ..., 9        (ascending)
 * - range_make(10, 0, -1) → 10, 9, 8, ..., 1       (descending)
 * - range_make(0, 20, 5)  → 0, 5, 10, 15           (stepped)
 * - range_make(5, 5, 1)   → (empty)
 * - range_make(5, 3, 1)   → (empty — start >= end with positive step)
 * - range_make(3, 5, -1)  → (empty — start <= end with negative step)
 *
 * Quick start:
 * ```c
 * #include "data/range.h"
 *
 * // Macro-style for-loop (recommended)
 * int i;
 * RANGE_FOR(i, range_make(0, 10, 1)) {
 *     printf("%d ", i);  // 0 1 2 3 4 5 6 7 8 9
 * }
 *
 * // Manual iteration
 * range r = range_make(10, 0, -1);
 * while (range_has_next(&r)) {
 *     isize val = range_next(&r);  // 10, 9, ..., 1
 * }
 *
 * // Pre-allocation using exact count
 * range r = range_make(0, 1000, 1);
 * usize count = range_len(&r);
 * int* arr = malloc(count * sizeof(int));
 * ```
 *
 * @sa data/vec/vec_range.h — extend a vec with values from a range
 */

/* ════════════════════════════════════════════════════════════════════════════
   range struct
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Integer range iterator — bounded sequence [current, end) with step
 *
 * Maintains iteration state — calling range_next() advances the iterator.
 *
 * Fields:
 * - current: Next value to be returned by range_next()
 * - end:     Exclusive bound (iteration stops when current reaches/crosses this)
 * - step:    Increment per step (positive = ascending, negative = descending)
 *
 * Invariants:
 * - step != 0 (normalized to +1 if 0 is provided at construction)
 * - If step > 0: empty when current >= end
 * - If step < 0: empty when current <= end
 *
 * Do not modify fields directly during iteration — use range_next() only.
 */
typedef struct {
    isize current; ///< Next value to be returned by range_next()
    isize end;     ///< Exclusive bound
    isize step;    ///< Step size (positive = ascending, negative = descending)
} range;

/* ════════════════════════════════════════════════════════════════════════════
   Construction
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates a range [start, end) with given step
 *
 * @param start Starting value (inclusive)
 * @param end   Exclusive end bound
 * @param step  Increment/decrement per step (0 normalized to +1)
 * @return Initialized range ready for iteration
 *
 * Behavior by step direction:
 * - step > 0: generates start, start+step, ... while current < end
 * - step < 0: generates start, start+step, ... while current > end
 * - step == 0: normalized to step = 1 (ascending by 1)
 *
 * @post result.step != 0
 * @post Empty ranges are valid and safe — range_has_next() returns false immediately
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline range range_make(isize start, isize end, isize step) {
    if (step == 0) step = 1;
    return (range){ .current = start, .end = end, .step = step };
}

/**
 * @brief Creates ascending range [0, end) with step 1
 *
 * @param end Exclusive upper bound
 * @return Range from 0 to end-1
 *
 * Example: range_upto(5) → 0, 1, 2, 3, 4
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline range range_upto(isize end) {
    return range_make(0, end, 1);
}

/**
 * @brief Creates ascending range [start, end) with step 1
 *
 * @param start Inclusive start value
 * @param end   Exclusive end value
 * @return Range from start to end-1
 *
 * Example: range_from_to(5, 10) → 5, 6, 7, 8, 9
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline range range_from_to(isize start, isize end) {
    return range_make(start, end, 1);
}

/**
 * @brief Creates descending range [start, 0) with step -1
 *
 * @param start Starting value (inclusive)
 * @return Range from start down to 1
 *
 * Example: range_downfrom(5) → 5, 4, 3, 2, 1
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline range range_downfrom(isize start) {
    return range_make(start, 0, -1);
}

/**
 * @brief Creates descending range [start, end) with step -1
 *
 * @param start Starting value (inclusive)
 * @param end   Exclusive lower bound
 * @return Range from start down to end+1
 *
 * Example: range_downto(10, 5) → 10, 9, 8, 7, 6
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline range range_downto(isize start, isize end) {
    return range_make(start, end, -1);
}

/* ════════════════════════════════════════════════════════════════════════════
   Queries
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if the range has no remaining elements
 *
 * @param r Range to check (NULL-safe)
 * @return true if exhausted or r == NULL
 *
 * Empty conditions:
 * - r == NULL
 * - step > 0 and current >= end
 * - step < 0 and current <= end
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool range_is_empty(const range* r) {
    if (!r) return true;
    if (r->step > 0) return r->current >= r->end;
    if (r->step < 0) return r->current <= r->end;
    return true; /* step == 0 should not occur after normalization */
}

/**
 * @brief Returns true if the range has at least one remaining element
 *
 * Equivalent to !range_is_empty(r).
 *
 * @param r Range to check (NULL-safe)
 * @return true if at least one element remains
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool range_has_next(const range* r) {
    return !range_is_empty(r);
}

/**
 * @brief Returns true if r is non-NULL and has remaining elements
 *
 * @param r Range to check (NULL-safe)
 * @return true if range is usable
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool range_is_valid(const range* r) {
    return r && !range_is_empty(r);
}

/**
 * @brief Calculates the total number of remaining elements
 *
 * Pure arithmetic — does NOT consume the range.
 * Returns CANON_USIZE_MAX if the count cannot be represented in usize.
 *
 * @param r Range to measure (NULL-safe)
 * @return Exact element count, CANON_USIZE_MAX if too large, 0 if NULL or empty
 *
 * Examples:
 * - range_len(&range_make(0, 10, 1))  → 10
 * - range_len(&range_make(0, 10, 2))  → 5
 * - range_len(&range_make(10, 0, -1)) → 10
 * - range_len(&range_make(5, 5, 1))   → 0
 *
 * Performance:
 * - Time:  O(1) — pure arithmetic, no iteration
 * - Space: O(1)
 */
static inline usize range_len(const range* r) {
    if (!r || range_is_empty(r)) return 0;
    isize abs_step = r->step > 0 ? r->step : -r->step;
    isize diff     = r->step > 0 ? (r->end - r->current)
                                 : (r->current - r->end);
    if (diff <= 0) return 0;
    if (diff > (isize)CANON_USIZE_MAX) return CANON_USIZE_MAX;
    return (usize)((diff - 1) / abs_step + 1);
}

/**
 * @brief Returns the number of remaining elements (alias for range_len)
 *
 * @param r Range to measure (NULL-safe)
 * @return Number of elements remaining
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize range_remaining(const range* r) {
    return range_len(r);
}

/**
 * @brief Peeks at the next value without advancing
 *
 * @param r   Range to peek (NULL-safe)
 * @param out Pointer to store current value
 * @return true if value was retrieved, false if empty or invalid
 *
 * @post r is unchanged — peek does not consume elements
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool range_peek(const range* r, isize* out) {
    if (!r || !out || range_is_empty(r)) return false;
    *out = r->current;
    return true;
}

/**
 * @brief Peeks at the next value as Option<isize>
 *
 * @param r Range to peek (NULL-safe)
 * @return Some(next) if available, None if empty or NULL
 *
 * @post r is unchanged
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline option_isize range_peek_option(const range* r) {
    isize val;
    if (range_peek(r, &val)) return option_isize_some(val);
    return option_isize_none();
}

/** @brief Alias for range_peek_option — symmetry with iterator naming */
#define range_current_option range_peek_option

/* ════════════════════════════════════════════════════════════════════════════
   Iteration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the next value and advances the iterator
 *
 * @param r Range to advance
 * @return Next value in sequence
 *
 * @pre r != NULL — checked via require_msg() (hard precondition)
 * @pre range_has_next(r) == true — checked via ensure_msg() (debug only)
 *
 * @post r->current is advanced by r->step (saturates to r->end on overflow)
 * @note Returns 0 as a safety fallback in release builds if called on empty range
 *
 * ⚠️ WARNING: Always check range_has_next() before calling, or use RANGE_FOR.
 *    Calling on an exhausted range is a logic error.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline isize range_next(range* r) {
    require_msg(r != NULL, "range_next: r cannot be NULL");
    ensure_msg(range_has_next(r), "range_next: called on exhausted range");
    if (!r || !range_has_next(r)) return 0;

    isize value = r->current;

    /* Overflow-safe advance — saturate to end rather than wrapping */
    if (r->step > 0) {
        if (r->current > CANON_ISIZE_MAX - r->step) {
            r->current = r->end; /* saturate on overflow */
        } else {
            r->current += r->step;
            if (r->current >= r->end) r->current = r->end;
        }
    } else { /* step < 0 */
        if (r->current < CANON_ISIZE_MIN - r->step) {
            r->current = r->end; /* saturate on underflow */
        } else {
            r->current += r->step;
            if (r->current <= r->end) r->current = r->end;
        }
    }

    return value;
}

/**
 * @brief Resets the range to a new starting position
 *
 * @param r         Range to reset (NULL-safe)
 * @param new_start New current value to start from
 *
 * @post r->current == new_start
 * @post r->end and r->step are unchanged
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void range_reset(range* r, isize new_start) {
    if (r) r->current = new_start;
}

/**
 * @brief Skips n elements forward, consuming them
 *
 * @param r Range to advance (NULL-safe)
 * @param n Number of elements to skip
 *
 * @note Stops early if range is exhausted before n steps are taken
 *
 * Example:
 * ```c
 * range r = range_make(0, 10, 1);
 * range_skip(&r, 5);
 * isize val = range_next(&r);  // val == 5
 * ```
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 */
static inline void range_skip(range* r, usize n) {
    if (!r) return;
    for (usize i = 0; i < n && range_has_next(r); i++) {
        range_next(r);
    }
}

/** @brief Alias for range_skip */
#define range_advance range_skip

/* ════════════════════════════════════════════════════════════════════════════
   RANGE_FOR — for-loop integration macro
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def RANGE_FOR
 * @brief Clean for-loop syntax for range iteration
 *
 * Provides Python/Go-style range iteration without manual range_has_next
 * and range_next calls.
 *
 * @param var    Loop variable — assigned the current value each iteration
 * @param r_expr Expression that evaluates to a range (evaluated once)
 *
 * @note var must be declared before the loop
 * @note var type must be compatible with isize (assignment via cast)
 * @note break and continue work normally inside the loop body
 * @note r_expr is evaluated exactly once at loop start
 *
 * @warning Do not modify the internal range variable (_r) inside the loop
 *
 * Basic usage:
 * ```c
 * int i;
 * RANGE_FOR(i, range_make(0, 10, 1)) {
 *     printf("%d ", i);  // 0 1 2 3 4 5 6 7 8 9
 * }
 * ```
 *
 * Descending:
 * ```c
 * int i;
 * RANGE_FOR(i, range_make(10, 0, -1)) {
 *     printf("%d ", i);  // 10 9 8 7 6 5 4 3 2 1
 * }
 * ```
 *
 * Stepped:
 * ```c
 * int i;
 * RANGE_FOR(i, range_make(0, 20, 3)) {
 *     printf("%d ", i);  // 0 3 6 9 12 15 18
 * }
 * ```
 *
 * Nested (each loop has its own _r / _rp via shadowing):
 * ```c
 * int i, j;
 * RANGE_FOR(i, range_upto(3)) {
 *     RANGE_FOR(j, range_upto(3)) {
 *         printf("(%d,%d) ", i, j);
 *     }
 * }
 * ```
 *
 * With break:
 * ```c
 * int i;
 * RANGE_FOR(i, range_upto(100)) {
 *     if (i == 10) break;
 *     printf("%d ", i);
 * }
 * ```
 */
#define RANGE_FOR(var, r_expr) \
    for (range _r = (r_expr), *_rp = &_r; \
         range_has_next(_rp) && ((var) = (typeof(var))range_next(_rp), true); )

#endif /* CANON_DATA_RANGE_H */
