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

#ifndef CANON_DATA_RANGE_H
#define CANON_DATA_RANGE_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/checked.h"
#include "semantics/option/option.h"

/* VERIFY-028 F3: guarded, exactly as bitset.h guards option_usize, so a
 * caller (or vmacros/vdrivers/range_verify.h) can interpose its own
 * instantiation, and a second includer cannot redefine it. */
#ifndef CANON_OPTION_ISIZE_DEFINED
    #define CANON_OPTION_ISIZE_DEFINED
    /* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
    CANON_OPTION(isize)
#endif

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
 * - step == ISIZE_MIN: rejected at construction (cannot be negated safely)
 * - Overflow-safe: uses checked_add_isize() to detect and saturate on overflow
 * - Empty ranges are valid and safe (e.g. range_make(5, 3, 1))
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses isize (ptrdiff_t equivalent) and usize from primitives/types.h
 * - No platform-specific code
 * - RANGE_FOR uses typeof() on GNU C / C23; define CANON_NO_GNU_EXTENSIONS
 *   for a strict C99 fallback (var must be isize-compatible in that case)
 * - Nesting RANGE_FOR on MSVC requires explicit block scoping to avoid
 *   C4456 (variable shadowing of _r/_rp); use manual outer loops instead.
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
 * - range_skip(n):   O(1) — direct arithmetic advance (clamped to end)
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
 * ISIZE_MIN step constraint:
 * ────────────────────────────────────────────────────────────────────────────
 * range_make() rejects step == ISIZE_MIN via require_msg(). This is necessary
 * because range_len() and other internal operations need to compute the
 * absolute value of step using -step, which is undefined behavior in C when
 * step == ISIZE_MIN (the result -ISIZE_MIN is not representable in isize).
 *
 * In practice this is not a real-world limitation — ISIZE_MIN as a step
 * value would mean "decrement by the largest representable negative number
 * each iteration," which produces at most one or two iterations before
 * overflow regardless. The constraint catches the corner case at the
 * construction site rather than producing UB inside range_len.
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
 * @sa core/primitives/checked.h — checked_add_isize() for overflow-safe arithmetic
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
 * Invariants (when constructed via range_make):
 * - step != 0 (normalized to +1 if 0 is provided at construction)
 * - step != ISIZE_MIN (rejected at construction)
 * - If step > 0: empty when current >= end
 * - If step < 0: empty when current <= end
 *
 * Do not modify fields directly during iteration — use range_next() only.
 * Constructing a range struct directly bypassing range_make() is undefined
 * behavior; the invariants above must hold.
 */
typedef struct {
    isize current; ///< Next value to be returned by range_next()
    isize end;     ///< Exclusive bound
    isize step;    ///< Step size (positive = ascending, negative = descending)
} range;

/* ── ACSL logic layer (VERIFY-028) ──────────────────────────────────────────
 * range_wf    : exactly the header's two documented invariants, nothing added.
 * range_empty : mirrors range_is_empty, including the step == 0 arm that is
 *               dead under range_wf (kept: the struct is public).
 * range_count : the exact number of values still to be yielded, over
 *               mathematical integers — no saturation, no wrap. ACSL `/` on
 *               integers truncates toward zero, as C99 does; span - 1 >= 0
 *               wherever it is evaluated, so the two agree.
 * No lemmas, deliberately: VERIFY-028 P1/P6 test whether the count ensures
 * prove WITHOUT help. Adding one later is legitimate, but is a recorded
 * change to the experiment, and a lemma that does not prove is a residual.
 */
/*@
  predicate range_wf(range r) =
      r.step != 0 && r.step != CANON_ISIZE_MIN;

  predicate range_empty(range r) =
      r.step == 0
   || (r.step > 0 && r.current >= r.end)
   || (r.step < 0 && r.current <= r.end);

  logic integer range_count(range r) =
      r.step > 0 ? (r.current >= r.end ? 0
                     : (r.end - r.current - 1) / r.step + 1)
    : r.step < 0 ? (r.current <= r.end ? 0
                     : (r.current - r.end - 1) / (-r.step) + 1)
    : 0;
*/

/* ════════════════════════════════════════════════════════════════════════════
   Construction
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates a range [start, end) with given step
 *
 * @param start Starting value (inclusive)
 * @param end   Exclusive end bound
 * @param step  Increment/decrement per step (0 normalized to +1, ISIZE_MIN rejected)
 * @return Initialized range ready for iteration
 *
 * Behavior by step direction:
 * - step > 0: generates start, start+step, ... while current < end
 * - step < 0: generates start, start+step, ... while current > end
 * - step == 0: normalized to step = 1 (ascending by 1)
 * - step == ISIZE_MIN: rejected via require_msg() — cannot be negated safely
 *
 * @pre step != ISIZE_MIN (checked via require_msg)
 *
 * @post result.step != 0
 * @post result.step != ISIZE_MIN
 * @post Empty ranges are valid and safe — range_has_next() returns false immediately
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
/*@
  requires step != CANON_ISIZE_MIN;
  assigns \nothing;
  ensures \result.current == start;
  ensures \result.end == end;
  ensures \result.step == (step == 0 ? 1 : step);
  ensures range_wf(\result);
*/
static inline range range_make(isize start, isize end, isize step) {
    require_msg(step != CANON_ISIZE_MIN,
                "range_make: step cannot be ISIZE_MIN (would overflow on negation)");
    const isize st = (step == 0) ? 1 : step;
    return (range){ .current = start, .end = end, .step = st };
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
/*@
  assigns \nothing;
  ensures \result.current == 0;
  ensures \result.end == end;
  ensures \result.step == 1;
  ensures range_wf(\result);
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
/*@
  assigns \nothing;
  ensures \result.current == start;
  ensures \result.end == end;
  ensures \result.step == 1;
  ensures range_wf(\result);
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
/*@
  assigns \nothing;
  ensures \result.current == start;
  ensures \result.end == 0;
  ensures \result.step == -1;
  ensures range_wf(\result);
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
/*@
  assigns \nothing;
  ensures \result.current == start;
  ensures \result.end == end;
  ensures \result.step == -1;
  ensures range_wf(\result);
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
/*@
  requires r == \null || \valid_read(r);
  assigns \nothing;
  behavior null:
    assumes r == \null;
    ensures \result == \true;
  behavior nonnull:
    assumes r != \null;
    ensures \result <==> range_empty(*r);
  complete behaviors;
  disjoint behaviors;
*/
static inline bool range_is_empty(const range* r) {
    if (!r) { return true; }
    if (r->step > 0) { return r->current >= r->end; }
    if (r->step < 0) { return r->current <= r->end; }
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
/*@
  requires r == \null || \valid_read(r);
  assigns \nothing;
  ensures \result <==> (r != \null && !range_empty(*r));
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
/*@
  requires r == \null || \valid_read(r);
  assigns \nothing;
  ensures \result <==> (r != \null && !range_empty(*r));
*/
static inline bool range_is_valid(const range* r) {
    return r && !range_is_empty(r);
}

/**
 * @brief Calculates the total number of remaining elements
 *
 * Pure arithmetic — does NOT consume the range.
 *
 * @param r Range to measure (NULL-safe)
 * @return Exact element count, or 0 if NULL or empty
 *
 * Exact over the whole isize domain (VERIFY-028 F1). The span of a range
 * can reach 2*ISIZE_MAX + 1 = USIZE_MAX, which does NOT fit in isize, so
 * it is computed in usize: casting isize -> usize is value-preserving
 * modulo 2^N, and the unsigned difference of the two casts is the true
 * span whenever the range is non-empty. The count (span-1)/|step| + 1 is
 * at most span, so it always fits in usize — no saturation is needed.
 * (Before F1 the span was an isize subtraction: undefined behaviour for
 * any range wider than ISIZE_MAX, observed returning 0.)
 *
 * Note on abs_step computation:
 * The negation -r->step is safe here because range_make() rejects
 * step == ISIZE_MIN at construction. Any range constructed through
 * the public API has step in [ISIZE_MIN+1, ISIZE_MAX], for which
 * -step is always representable.
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
/*@
  requires r == \null || (\valid_read(r) && range_wf(*r));
  assigns \nothing;
  behavior null:
    assumes r == \null;
    ensures \result == 0;
  behavior nonnull:
    assumes r != \null;
    ensures len_count: \result == range_count(*r);
  complete behaviors;
  disjoint behaviors;
*/
static inline usize range_len(const range* r) {
    if (!r || range_is_empty(r)) { return 0; }

    /* VERIFY-028 F1: all arithmetic in usize. |step| as 0 - (usize)step
     * (modular) rather than -step, so no signed negation is needed at all. */
    const usize abs_step = (r->step > 0) ? (usize)r->step
                                         : ((usize)0 - (usize)r->step);
    const usize span     = (r->step > 0) ? ((usize)r->end - (usize)r->current)
                                         : ((usize)r->current - (usize)r->end);

    /* non-empty => span >= 1, and abs_step >= 1 */
    return ((span - 1u) / abs_step) + 1u;
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
/*@
  requires r == \null || (\valid_read(r) && range_wf(*r));
  assigns \nothing;
  behavior null:
    assumes r == \null;
    ensures \result == 0;
  behavior nonnull:
    assumes r != \null;
    ensures len_count: \result == range_count(*r);
  complete behaviors;
  disjoint behaviors;
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
/*@
  requires r == \null || \valid_read(r);
  requires out == \null || \valid(out);
  assigns *out;
  behavior absent:
    assumes r == \null || out == \null || range_empty(*r);
    assigns \nothing;
    ensures \result == \false;
  behavior present:
    assumes r != \null && out != \null && !range_empty(*r);
    assigns *out;
    ensures \result == \true;
    ensures *out == \old(r->current);
  complete behaviors;
  disjoint behaviors;
*/
static inline bool range_peek(const range* r, isize* out) {
    if (!r || !out || range_is_empty(r)) { return false; }
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
/*@
  requires r == \null || \valid_read(r);
  assigns \nothing;
  behavior absent:
    assumes r == \null || range_empty(*r);
    ensures !\result.has_value;
  behavior present:
    assumes r != \null && !range_empty(*r);
    ensures \result.has_value;
    ensures \result.value == r->current;
  complete behaviors;
  disjoint behaviors;
*/
static inline option_isize range_peek_option(const range* r) {
    isize val;
    if (range_peek(r, &val)) { return option_isize_some(val); }
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
 * @post r->current is advanced by r->step using checked_add_isize()
 * @post On overflow, saturates to r->end for safety
 *
 * ⚠️ WARNING: Always check range_has_next() before calling, or use RANGE_FOR.
 *    Calling on an exhausted range is a logic error.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
/*@
  requires \valid(r) && range_wf(*r) && !range_empty(*r);
  assigns r->current;
  ensures \result == \old(r->current);
  ensures r->end == \old(r->end) && r->step == \old(r->step);
  ensures range_wf(*r);
  ensures next_inside:
    ((\old(r->step) > 0 && \old(r->current) + \old(r->step) < \old(r->end))
  || (\old(r->step) < 0 && \old(r->current) + \old(r->step) > \old(r->end)))
      ==> r->current == \old(r->current) + \old(r->step);
  ensures next_exhaust:
    !((\old(r->step) > 0 && \old(r->current) + \old(r->step) < \old(r->end))
   || (\old(r->step) < 0 && \old(r->current) + \old(r->step) > \old(r->end)))
      ==> r->current == r->end;
  ensures next_count:
    range_count(*r) == range_count(\old(*r)) - 1;
*/
static inline isize range_next(range* r) {
    require_msg(r != NULL, "range_next: r cannot be NULL");
    /* VERIFY-028 F4: a caller obligation, so require_msg (active in release
     * builds unless CANON_NO_REQUIRE), not ensure_msg (removed by NDEBUG). */
    require_msg(range_has_next(r), "range_next: called on exhausted range");

    isize value = r->current;
    isize next_value;

    if (checked_add_isize(r->current, r->step, &next_value)) {
        r->current = next_value;

        if ((r->step > 0) && (r->current >= r->end)) {
            r->current = r->end;
        } else if ((r->step < 0) && (r->current <= r->end)) {
            r->current = r->end;
        }
    } else {
        r->current = r->end;
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
/*@
  requires r == \null || \valid(r);
  assigns r->current;
  behavior null:
    assumes r == \null;
    assigns \nothing;
  behavior nonnull:
    assumes r != \null;
    assigns r->current;
    ensures r->current == new_start;
    ensures r->end == \old(r->end) && r->step == \old(r->step);
  complete behaviors;
  disjoint behaviors;
*/
static inline void range_reset(range* r, isize new_start) {
    if (r != NULL) { r->current = new_start; }
}

/**
 * @brief Skips n elements forward in O(1) using direct arithmetic
 *
 * Advances current by (n * step); if that would reach or cross end, the
 * range is exhausted (current = end).
 *
 * @param r Range to advance (NULL-safe)
 * @param n Number of elements to skip
 *
 * @note Does not iterate — computes new position directly
 * @note Exact over the whole isize domain (VERIFY-028 F2): for every n,
 *       range_len afterwards == max(0, range_len before - n). Before F2
 *       the jump was an isize product: n > ISIZE_MAX reversed direction
 *       (F2a), and on ranges wider than ISIZE_MAX a jump that overflowed
 *       isize exhausted the range early even when the target was inside
 *       it (F2b).
 *
 * Example:
 * ```c
 * range r = range_make(0, 10, 1);
 * range_skip(&r, 5);
 * isize val = range_next(&r);  // val == 5
 * ```
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
/*@
  requires r == \null || (\valid(r) && range_wf(*r));
  assigns r->current;
  behavior noop:
    assumes r == \null || n == 0 || range_empty(*r);
    assigns \nothing;
  behavior advance:
    assumes r != \null && n != 0 && !range_empty(*r);
    assigns r->current;
    ensures r->end == \old(r->end) && r->step == \old(r->step);
    ensures skip_inside:
      ((\old(r->step) > 0 && \old(r->current) + n * \old(r->step) < \old(r->end))
    || (\old(r->step) < 0 && \old(r->current) + n * \old(r->step) > \old(r->end)))
        ==> r->current == \old(r->current) + n * \old(r->step);
    ensures skip_exhaust:
      !((\old(r->step) > 0 && \old(r->current) + n * \old(r->step) < \old(r->end))
     || (\old(r->step) < 0 && \old(r->current) + n * \old(r->step) > \old(r->end)))
        ==> r->current == r->end;
    ensures skip_count:
      range_count(*r) == (n >= range_count(\old(*r)) ? 0
                                                      : range_count(\old(*r)) - n);
  complete behaviors;
  disjoint behaviors;
*/
static inline void range_skip(range* r, usize n) {
    if (!r || (n == 0u) || range_is_empty(r)) { return; }

    /* VERIFY-028 F2: decide exhaustion by COUNT, before any arithmetic. */
    const usize count = range_len(r);
    if (n >= count) {
        r->current = r->end;
        return;
    }

    /* n < count = (span-1)/|step| + 1  ==>  n*|step| <= span-1 < USIZE_MAX,
     * so the offset fits in usize, and the target lies strictly between
     * current and end, so it is representable as an isize. The offset may
     * still exceed ISIZE_MAX (span can reach USIZE_MAX), so it is applied in
     * at most two steps of <= ISIZE_MAX each; each intermediate value lies
     * between current and the target. No unsigned-to-signed conversion of an
     * out-of-range value occurs. */
    const usize abs_step = (r->step > 0) ? (usize)r->step
                                         : ((usize)0 - (usize)r->step);
    usize offset = n * abs_step;
    const usize half = (usize)CANON_ISIZE_MAX;

    if (r->step > 0) {
        if (offset > half) {
            r->current = r->current + CANON_ISIZE_MAX;
            offset     = offset - half;
        }
        r->current = r->current + (isize)offset;
    } else {
        if (offset > half) {
            r->current = r->current - CANON_ISIZE_MAX;
            offset     = offset - half;
        }
        r->current = r->current - (isize)offset;
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
 * @note With CANON_NO_GNU_EXTENSIONS: var must be isize-compatible (cast to isize)
 * @note Without CANON_NO_GNU_EXTENSIONS: var may be any type compatible with
 *       isize via typeof() (GNU C / C23)
 * @note break and continue work normally inside the loop body
 * @note r_expr is evaluated exactly once at loop start
 *
 * @warning Nesting RANGE_FOR on MSVC triggers C4456 (shadowing of _r/_rp).
 *          For portable nested iteration, use a manual outer loop:
 *          ```c
 *          range outer = range_upto(3);
 *          while (range_has_next(&outer)) {
 *              isize x = range_next(&outer);
 *              isize y;
 *              RANGE_FOR(y, range_upto(3)) { ... }
 *          }
 *          ```
 *
 * Basic usage:
 * ```c
 * int i;
 * RANGE_FOR(i, range_make(0, 10, 1)) {
 *     printf("%d ", i);  // 0 1 2 3 4 5 6 7 8 9
 * }
 * ```
 */
#ifndef CANON_NO_GNU_EXTENSIONS
    #define RANGE_FOR(var, r_expr) \
        for (range _r = (r_expr), *_rp = &_r; \
             range_has_next(_rp) && ((var) = (typeof(var))range_next(_rp), true); )
#else
    /* Strict C99 fallback: var must be isize-compatible */
    #define RANGE_FOR(var, r_expr) \
        for (range _r = (r_expr), *_rp = &_r; \
             range_has_next(_rp) && ((var) = (isize)range_next(_rp), true); )
#endif

#endif /* CANON_DATA_RANGE_H */
