#ifndef CANON_ALGO_FILTER_H
#define CANON_ALGO_FILTER_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "core/primitives/compare.h"   // provides algo_pred_fn
#include "algo/any_all.h"              // (still needed if you use ALGO_ANY_TYPED etc. elsewhere, otherwise can be removed)

/**
 * @file algo/filter.h
 * @brief Select elements from a sequence matching a predicate
 *
 * Copies elements that satisfy a predicate into a caller-provided output
 * buffer. Preserves relative order (stable). Truncates safely when the
 * output buffer is full.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Input is read-only — never mutated
 * - Output buffer is caller-provided — no allocation
 * - Predicate type is algo_pred_fn from compare.h
 * - Safe truncation when output capacity is reached
 * - Three levels: generic flat-array, typed macro, slice variants
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Algorithm explanation:
 * ────────────────────────────────────────────────────────────────────────────
 * Linear scan with selective copying:
 * 1. Iterate through all input elements left-to-right
 * 2. Apply predicate to each element
 * 3. If predicate returns true and output has space: copy element
 * 4. If output buffer is full: stop copying but continue checking
 * 5. Return count of elements written
 *
 * Example: Filtering [−1, 2, −3, 4, 0, 5] for positive numbers
 * Input: [−1, 2, −3, 4, 0, 5]
 * Step 0: Check −1 — pred(−1) = false, skip
 * Step 1: Check 2 — pred(2) = true, copy → out[0] = 2
 * Step 2: Check −3 — pred(−3) = false, skip
 * Step 3: Check 4 — pred(4) = true, copy → out[1] = 4
 * Step 4: Check 0 — pred(0) = false, skip
 * Step 5: Check 5 — pred(5) = true, copy → out[2] = 5
 * Result: out = [2, 4, 5], count = 3
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/, semantics/, and data/ only.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No GCC statement expressions — all macros are C99 portable
 * - No platform-specific code
 * - No allocations anywhere in this file
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions are thread-safe (no shared mutable state)
 * - Safe to call concurrently on different arrays from multiple threads
 * - Same arrays should not be accessed concurrently without external synchronization
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(n) — all input elements are checked (no short-circuit)
 * - Space complexity: O(1) — no allocation
 * - Copies: At most min(matching_elements, out_cap) element copies
 * - No short-circuit: predicate called for every input element
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Selecting elements matching criteria
 * - Removing invalid or unwanted data
 * - Extracting subset based on predicate
 * - Data cleaning and validation
 * - Collecting specific items from dataset
 * - Creating filtered views
 * - Preparing data for further processing
 *
 * Quick start:
 * ```c
 * #include "algo/filter.h"
 *
 * bool is_positive(const void* e, void* ctx) {
 *     return *(const int*)e > 0;
 * }
 *
 * int in[] = {-1, 2, -3, 4, 0, 5};
 * int out[6];
 *
 * // Generic — explicit elem_size and out_cap
 * usize n = algo_filter(in, 6, sizeof(int), is_positive, NULL, out, 6);
 * // n = 3, out = {2, 4, 5}
 *
 * // Typed macro — Type and out_cap explicit
 * usize n = ALGO_FILTER_TYPED(out, 6, in, 6, int, is_positive, NULL);
 * // n = 3, out = {2, 4, 5}
 *
 * // Slice variant
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FILTER(int)
 * slice_int sv = slice_int_from(in, 6);
 * usize n = algo_filter_slice_int(sv, out, 6, is_positive, NULL);
 * // n = 3, out = {2, 4, 5}
 * ```
 *
 * @sa core/primitives/compare.h — algo_pred_fn predicate type
 * @sa core/slice.h — slice_##type used by DEFINE_ALGO_FILTER
 * @sa core/primitives/ptr.h — ptr_elem_const used in generic interface
 * @sa core/memory.h — mem_copy used for element copying
 * @sa core/ownership.h — borrowed macro for non-owning parameters
 */
/* ════════════════════════════════════════════════════════════════════════════
   Generic flat-array interface
   ════════════════════════════════════════════════════════════════════════════
   Uses ptr_elem_const() from ptr.h to index input by byte stride.
   Uses ptr_elem() to index output by byte stride.
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Filters a flat array into a caller-provided output buffer
 *
 * Iterates all input elements. For each element where pred returns true,
 * copies it into the output buffer. Stops writing (but keeps iterating
 * input) when out_cap is reached.
 *
 * @param base Pointer to first input element (borrowed, read-only)
 * @param len Number of input elements
 * @param elem_size Size of each element in bytes (> 0)
 * @param pred Predicate — return true to keep element (borrowed)
 * @param ctx Optional context passed to pred (borrowed, may be NULL)
 * @param out Caller-provided output buffer (borrowed — writable)
 * @param out_cap Maximum elements out can hold
 *
 * @return Number of elements written to out (≤ out_cap)
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 * @pre If out != NULL, out points to writable buffer for out_cap elements
 *
 * @post out contains min(matching_elements, out_cap) filtered elements
 * @post base is unchanged (read-only operation)
 * @post Relative order of matching elements is preserved
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - pred: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to pred, not stored)
 * - out: borrowed (writable buffer provided by caller)
 *
 * Performance:
 * - Time: O(n) — all input elements are checked
 * - Space: O(1)
 * - Copies: At most min(matching_elements, out_cap)
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays
 * - Not safe if base or out are being modified concurrently
 *
 * Returns 0 if:
 * - base == NULL
 * - pred == NULL
 * - out == NULL
 * - len == 0
 * - elem_size == 0
 * - out_cap == 0
 */
static inline usize algo_filter(
    borrowed const void* base,
    usize len,
    usize elem_size,
    borrowed algo_pred_fn pred,
    borrowed void* ctx,
    borrowed void* out,
    usize out_cap)
{
    require_msg(elem_size > 0, "algo_filter: elem_size must be > 0");
   
    if (!base || !pred || !out || len == 0 || out_cap == 0) {
        return 0;
    }
    usize written = 0;
    for (usize i = 0; i < len; i++) {
        const void* elem = ptr_elem_const(base, i, elem_size);
        if (pred(elem, ctx)) {
            if (written >= out_cap) break; /* truncate */
            void* dest = ptr_elem(out, written, elem_size);
            mem_copy(dest, elem, elem_size);
            written++;
        }
    }
    return written;
}
/* ════════════════════════════════════════════════════════════════════════════
   Typed macro — recommended
   ════════════════════════════════════════════════════════════════════════════
   C99-portable — no GCC statement expressions.
   out_cap is explicit — avoids the sizeof(array)/sizeof(array[0]) trap
   that breaks when out is passed as a pointer.
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @def ALGO_FILTER_TYPED(out, out_cap, in, in_len, Type, pred, ctx)
 * @brief Type-safe filter from one C array into another
 *
 * Provides compile-time type safety by automatically calculating sizeof(Type).
 *
 * @param out Output array of Type (borrowed — caller-provided writable buffer)
 * @param out_cap Maximum elements out can hold (explicit — not sizeof/sizeof)
 * @param in Input array of Type (borrowed, read-only)
 * @param in_len Number of input elements
 * @param Type Element type
 * @param pred Predicate: algo_pred_fn (borrowed)
 * @param ctx Optional context (borrowed, may be NULL)
 *
 * @return usize — number of elements written to out
 */
#define ALGO_FILTER_TYPED(out, out_cap, in, in_len, Type, pred, ctx) \
    algo_filter( \
        (in), (in_len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx), \
        (out), (out_cap))
/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_FILTER — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Generates a filter function that accepts slice_##type input directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated function:
 * - algo_filter_slice_##type(sv, out, out_cap, pred, ctx) → usize
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 */
#define DEFINE_ALGO_FILTER(type) \
\
/** \
 * @brief Filters slice_##type elements into a caller-provided output array \
 * \
 * @param sv Input slice (borrowed, read-only) \
 * @param out Output array of type (borrowed — caller-provided writable buffer) \
 * @param out_cap Maximum elements out can hold \
 * @param pred Predicate function (algo_pred_fn) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @return usize — number of elements written to out \
 */ \
static inline usize algo_filter_slice_##type( \
    borrowed slice_##type sv, \
    borrowed type* out, \
    usize out_cap, \
    borrowed algo_pred_fn pred, \
    borrowed void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !out || out_cap == 0 || !pred) return 0; \
    usize written = 0; \
    for (usize _i = 0; _i < sv.len && written < out_cap; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) { \
            out[written++] = sv.ptr[_i]; \
        } \
    } \
    return written; \
}

#endif /* CANON_ALGO_FILTER_H */
