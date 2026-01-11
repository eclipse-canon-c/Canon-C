// data/range.h
#ifndef CANON_DATA_RANGE_H
#define CANON_DATA_RANGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

/**
 * @file range.h
 * @brief Explicit, bounded integer range generator (iterator-style)
 *
 * Generates sequential integers with full control over start, end, and step.
 * Supports ascending/descending, signed/unsigned semantics.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, stdint.h, ptrdiff_t)
 *   - No dependencies on other library headers
 *   - No platform-specific code
 *
 * Thread-safety: Each range instance is independent - safe to use from multiple
 *                threads if each thread has its own range instance
 *
 * Performance:
 *   - Zero overhead abstraction - compiles to simple integer arithmetic
 *   - All operations are O(1)
 *   - No allocations
 *   - No hidden state
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
 *   - Can pre-calculate exact iteration count
 *
 * If you need an infinite sequence or dynamic growth → this is not the right tool.
 *
 *                           Semantics Summary
 * ──────────────────────────────────────────────────────────────────────────────
 * • [start, end) pattern → start inclusive, end exclusive (like C++ std::ranges, Python range())
 * • step > 0  → ascending  (0..10 with step 1 → 0,1,2,...,9)
 * • step < 0  → descending (10..0 with step -1 → 10,9,...,1)
 * • step == 0 → normalized to +1 during creation (for safety)
 * • Overflow-safe length estimation (returns SIZE_MAX when too large)
 * • Iterator state: modifying range during iteration advances it
 *
 *                           Recommended Usage Patterns
 * ──────────────────────────────────────────────────────────────────────────────
 *
 * // 1. Classic ascending range
 * int i;
 * RANGE_FOR(i, range_make(0, 10, 1)) {
 *     printf("%d ", i);  // prints: 0 1 2 3 4 5 6 7 8 9
 * }
 *
 * // 2. Descending with step
 * int i;
 * RANGE_FOR(i, range_make(20, 5, -3)) {
 *     // i = 20, 17, 14, 11, 8
 * }
 *
 * // 3. Pre-allocate array based on range length
 * range r = range_make(0, 1000, 1);
 * size_t count = range_len(&r);
 * if (count == SIZE_MAX) { 
 *     fprintf(stderr, "Range too large\n");
 *     return ERROR;
 * }
 * int* array = malloc(sizeof(int) * count);
 *
 * // 4. Convenience constructors
 * RANGE_FOR(i, range_upto(10)) {        // 0..9
 *     printf("%d ", i);
 * }
 *
 * RANGE_FOR(i, range_from_to(5, 15)) {  // 5..14
 *     printf("%d ", i);
 * }
 *
 * RANGE_FOR(i, range_downfrom(10)) {    // 10..1
 *     printf("%d ", i);
 * }
 */

/**
 * @brief Integer range iterator
 *
 * Represents a bounded sequence of integers [start, end) with given step.
 * Maintains iteration state - calling range_next() advances the iterator.
 *
 * Fields:
 *   - current: Next value to be returned by range_next()
 *   - end: Exclusive bound (iteration stops when current reaches/crosses this)
 *   - step: Step size (positive = ascending, negative = descending)
 *
 * Invariants:
 *   - step != 0 (normalized to 1 if 0 is provided)
 *   - If step > 0: range is empty when current >= end
 *   - If step < 0: range is empty when current <= end
 *
 * Do not modify fields directly during iteration - use range_next() instead.
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
 *
 * @param start Starting value (inclusive)
 * @param end   Exclusive end bound
 * @param step  Increment/decrement (0 is normalized to +1 for safety)
 * @return      Initialized range ready for iteration
 *
 * Behavior:
 *   - If step > 0: generates start, start+step, start+2*step, ... while < end
 *   - If step < 0: generates start, start+step, start+2*step, ... while > end
 *   - If step == 0: normalized to step = 1 (ascending by 1)
 *
 * Empty ranges are valid and safe:
 *   - range_make(5, 3, 1) → empty (start >= end with positive step)
 *   - range_make(3, 5, -1) → empty (start <= end with negative step)
 *
 * Examples:
 *   range r1 = range_make(0, 10, 1);   // 0,1,2,...,9
 *   range r2 = range_make(10, 0, -1);  // 10,9,8,...,1
 *   range r3 = range_make(0, 20, 5);   // 0,5,10,15
 *   range r4 = range_make(5, 5, 1);    // empty
 */
static inline range range_make(ptrdiff_t start, ptrdiff_t end, ptrdiff_t step) {
    // Normalize zero step to 1 to prevent infinite loops
    if (step == 0) {
        step = 1;
    }
    
    return (range){
        .current = start,
        .end = end,
        .step = step
    };
}

/**
 * @brief Creates range [0, end) with step 1
 *
 * Convenience constructor for the common case of counting from 0.
 *
 * @param end Exclusive upper bound
 * @return    Range from 0 to end-1
 *
 * Example:
 *   range r = range_upto(10);  // 0,1,2,...,9
 */
static inline range range_upto(ptrdiff_t end) {
    return range_make(0, end, 1);
}

/**
 * @brief Creates range [start, end) with step 1
 *
 * Convenience constructor for ascending ranges.
 *
 * @param start Inclusive start value
 * @param end   Exclusive end value
 * @return      Range from start to end-1
 *
 * Example:
 *   range r = range_from_to(5, 15);  // 5,6,7,...,14
 */
static inline range range_from_to(ptrdiff_t start, ptrdiff_t end) {
    return range_make(start, end, 1);
}

/**
 * @brief Creates descending range [start, 0) with step -1
 *
 * Convenience constructor for counting down to (but not including) 0.
 *
 * @param start Starting value (inclusive)
 * @return      Range from start down to 1
 *
 * Example:
 *   range r = range_downfrom(10);  // 10,9,8,...,1
 */
static inline range range_downfrom(ptrdiff_t start) {
    return range_make(start, 0, -1);
}

/**
 * @brief Creates descending range [start, end) with step -1
 *
 * Convenience constructor for descending ranges.
 *
 * @param start Starting value (inclusive)
 * @param end   Exclusive lower bound
 * @return      Range from start down to end+1
 *
 * Example:
 *   range r = range_downto(10, 5);  // 10,9,8,7,6
 */
static inline range range_downto(ptrdiff_t start, ptrdiff_t end) {
    return range_make(start, end, -1);
}

/* ────────────────────────────────────────────────────────────────────────────
   Queries
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if range has no more elements
 *
 * @param r Range to check (NULL-safe)
 * @return  true if range is exhausted or NULL, false otherwise
 *
 * A range is empty when:
 *   - r is NULL
 *   - step > 0 and current >= end
 *   - step < 0 and current <= end
 *
 * Performance: O(1)
 */
static inline bool range_is_empty(const range* r) {
    if (!r) return true;
    
    if (r->step > 0) {
        return r->current >= r->end;
    } else if (r->step < 0) {
        return r->current <= r->end;
    }
    
    return true;  // step == 0 should not happen due to normalization
}

/**
 * @brief Checks if range has at least one more element
 *
 * Equivalent to !range_is_empty(r).
 *
 * @param r Range to check (NULL-safe)
 * @return  true if at least one element remains, false otherwise
 *
 * Performance: O(1)
 */
static inline bool range_has_next(const range* r) {
    return !range_is_empty(r);
}

/**
 * @brief Calculates the total number of elements in the range
 *
 * This does NOT consume the range - it's a pure calculation based on
 * the current state.
 *
 * @param r Range to measure (NULL-safe)
 * @return  Exact count when possible, SIZE_MAX if too large to represent, 0 if NULL/empty
 *
 * Returns SIZE_MAX if:
 *   - The range is so large it cannot be represented in size_t
 *   - Prevents overflow in calculations
 *
 * Performance: O(1) - pure calculation, no iteration
 *
 * Examples:
 *   range_len(&range_make(0, 10, 1))   → 10
 *   range_len(&range_make(0, 10, 2))   → 5
 *   range_len(&range_make(10, 0, -1))  → 10
 *   range_len(&range_make(5, 5, 1))    → 0 (empty)
 */
static inline size_t range_len(const range* r) {
    if (!r || range_is_empty(r)) {
        return 0;
    }
    
    if (r->step > 0) {
        ptrdiff_t diff = r->end - r->current;
        
        // Check if difference is too large
        if (diff > (ptrdiff_t)SIZE_MAX) {
            return SIZE_MAX;
        }
        
        // Calculate: ceil(diff / step) = (diff - 1) / step + 1
        // But we need to be careful with edge cases
        if (diff <= 0) return 0;
        
        return (size_t)((diff - 1) / r->step + 1);
    } else {  // step < 0
        ptrdiff_t diff = r->current - r->end;
        
        // Check if difference is too large
        if (diff > (ptrdiff_t)SIZE_MAX) {
            return SIZE_MAX;
        }
        
        if (diff <= 0) return 0;
        
        // Use absolute value of step for calculation
        ptrdiff_t abs_step = -r->step;
        return (size_t)((diff - 1) / abs_step + 1);
    }
}

/**
 * @brief Returns the number of remaining elements without consuming them
 *
 * Alias for range_len() for clarity in some contexts.
 *
 * @param r Range to measure (NULL-safe)
 * @return  Number of elements remaining
 */
static inline size_t range_remaining(const range* r) {
    return range_len(r);
}

/**
 * @brief Gets the current value without advancing
 *
 * Peeks at the next value that would be returned by range_next().
 *
 * @param r   Range to peek (NULL-safe)
 * @param out Pointer to store current value (NULL-safe)
 * @return    true if value was retrieved, false if range is empty/NULL
 *
 * Performance: O(1)
 *
 * Example:
 *   range r = range_make(5, 10, 1);
 *   ptrdiff_t val;
 *   if (range_peek(&r, &val)) {
 *       printf("Next value will be: %td\n", val);  // 5
 *   }
 */
static inline bool range_peek(const range* r, ptrdiff_t* out) {
    if (!r || !out || range_is_empty(r)) {
        return false;
    }
    
    *out = r->current;
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Iteration
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns next value and advances the iterator
 *
 * Consumes one element from the range and returns its value.
 *
 * @param r Range to advance (NULL-safe but returns 0)
 * @return  Next value in sequence
 *
 * ⚠️  WARNING: Undefined behavior if called when range_has_next() == false
 * Always check range_has_next() before calling, or use RANGE_FOR macro.
 *
 * Performance: O(1)
 *
 * Example:
 *   range r = range_make(0, 3, 1);
 *   while (range_has_next(&r)) {
 *       ptrdiff_t val = range_next(&r);
 *       printf("%td ", val);  // 0 1 2
 *   }
 */
static inline ptrdiff_t range_next(range* r) {
    assert(r != NULL && "range_next: range cannot be NULL");
    assert(range_has_next(r) && "range_next: called on empty range");
    
    if (!r) {
        return 0;  // Safety fallback (should not be used)
    }
    
    ptrdiff_t value = r->current;
    
    // Advance iterator with overflow protection
    if (r->step > 0) {
        // Check if we can add step without overflow
        if (r->current > PTRDIFF_MAX - r->step) {
            r->current = r->end;  // Exhaust range
        } else if (r->current + r->step >= r->end) {
            r->current = r->end;  // Reached/passed end
        } else {
            r->current += r->step;
        }
    } else {  // step < 0
        // Check if we can add negative step without underflow
        if (r->current < PTRDIFF_MIN - r->step) {
            r->current = r->end;  // Exhaust range
        } else if (r->current + r->step <= r->end) {
            r->current = r->end;  // Reached/passed end
        } else {
            r->current += r->step;
        }
    }
    
    return value;
}

/**
 * @brief Resets range to a new starting position
 *
 * Useful for reusing a range or implementing restart functionality.
 * The end and step remain unchanged.
 *
 * @param r         Range to reset
 * @param new_start New starting value
 *
 * Example:
 *   range r = range_make(0, 10, 1);
 *   // ... iterate ...
 *   range_reset(&r, 0);  // Start over from beginning
 */
static inline void range_reset(range* r, ptrdiff_t new_start) {
    if (r) {
        r->current = new_start;
    }
}

/**
 * @brief Skips n elements forward in the range
 *
 * @param r Range to advance
 * @param n Number of elements to skip
 *
 * Example:
 *   range r = range_make(0, 10, 1);
 *   range_skip(&r, 5);
 *   ptrdiff_t val = range_next(&r);  // Returns 5
 */
static inline void range_skip(range* r, size_t n) {
    if (!r) return;
    
    for (size_t i = 0; i < n && range_has_next(r); i++) {
        range_next(r);
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   For-loop integration (recommended)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Clean for-loop syntax for range iteration
 *
 * Provides Python-like range iteration syntax for C.
 * The variable is updated with each iteration value.
 *
 * @param var     Loop variable (will be assigned each value)
 * @param r_expr  Expression that evaluates to a range
 *
 * Usage notes:
 *   - var should be declared before the loop (type: ptrdiff_t or compatible)
 *   - r_expr is evaluated once at loop start
 *   - Loop body can use break/continue normally
 *   - var contains the current iteration value inside the loop
 *
 * Examples:
 *   int i;
 *   RANGE_FOR(i, range_make(1, 10, 2)) {
 *       printf("%d ", i);  // 1 3 5 7 9
 *   }
 *
 *   ptrdiff_t val;
 *   RANGE_FOR(val, range_upto(5)) {
 *       printf("%td ", val);  // 0 1 2 3 4
 *   }
 *
 *   // Nested loops work fine
 *   int i, j;
 *   RANGE_FOR(i, range_upto(3)) {
 *       RANGE_FOR(j, range_upto(3)) {
 *           printf("(%d,%d) ", i, j);
 *       }
 *   }
 */
#define RANGE_FOR(var, r_expr) \
    for (range _r = (r_expr), *_rp = &_r; \
         range_has_next(_rp) && ((var) = (typeof(var))range_next(_rp), true); )

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────

    #include "range.h"
    #include <stdio.h>
    
    void example_basic(void) {
        // Simple ascending range
        int i;
        RANGE_FOR(i, range_make(0, 10, 1)) {
            printf("%d ", i);  // 0 1 2 3 4 5 6 7 8 9
        }
        printf("\n");
        
        // Convenience constructor
        RANGE_FOR(i, range_upto(5)) {
            printf("%d ", i);  // 0 1 2 3 4
        }
        printf("\n");
        
        // Descending
        RANGE_FOR(i, range_make(10, 0, -1)) {
            printf("%d ", i);  // 10 9 8 7 6 5 4 3 2 1
        }
        printf("\n");
        
        // With step
        RANGE_FOR(i, range_make(0, 20, 3)) {
            printf("%d ", i);  // 0 3 6 9 12 15 18
        }
        printf("\n");
    }
    
    void example_manual_iteration(void) {
        range r = range_make(5, 10, 1);
        
        while (range_has_next(&r)) {
            ptrdiff_t val = range_next(&r);
            printf("%td ", val);  // 5 6 7 8 9
        }
        printf("\n");
    }
    
    void example_preallocation(void) {
        range r = range_make(0, 1000, 1);
        size_t count = range_len(&r);
        
        if (count == SIZE_MAX) {
            fprintf(stderr, "Range too large\n");
            return;
        }
        
        int* array = malloc(sizeof(int) * count);
        if (!array) return;
        
        size_t idx = 0;
        while (range_has_next(&r)) {
            array[idx++] = (int)range_next(&r);
        }
        
        printf("Filled array with %zu elements\n", idx);
        free(array);
    }
    
    void example_peek_and_skip(void) {
        range r = range_make(0, 20, 1);
        
        ptrdiff_t val;
        if (range_peek(&r, &val)) {
            printf("Next value: %td\n", val);  // 0
        }
        
        range_skip(&r, 5);  // Skip first 5 elements
        
        if (range_peek(&r, &val)) {
            printf("After skip: %td\n", val);  // 5
        }
    }
    
    void example_nested_loops(void) {
        int i, j;
        RANGE_FOR(i, range_upto(3)) {
            RANGE_FOR(j, range_upto(3)) {
                printf("(%d,%d) ", i, j);
            }
            printf("\n");
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_DATA_RANGE_H */
