#ifndef CANON_ALGO_FILTER_H
#define CANON_ALGO_FILTER_H

#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "algo/any_all.h"

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
 * - Predicate type is algo_pred_fn from algo/any_all.h — no duplicate typedef
 * - Safe truncation when output capacity is reached
 * - Three levels: generic flat-array, typed macro, slice variants
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/ and semantics/ only.
 * algo/filter.h additionally includes algo/any_all.h for algo_pred_fn.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No GCC statement expressions — all macros are C99 portable
 * - No platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(n) — all elements are checked (no short-circuit)
 * - Space: O(1) — no allocation
 *
 * Quick start:
 * ```c
 * #include "algo/filter.h"
 *
 * bool is_positive(const void* e, void* ctx) { return *(const int*)e > 0; }
 *
 * int in[]  = {-1, 2, -3, 4, 0, 5};
 * int out[6];
 *
 * // Generic — explicit elem_size and out_cap
 * usize n = algo_filter(in, 6, sizeof(int), is_positive, NULL, out, 6);
 *
 * // Typed macro — Type and out_cap explicit, no sizeof guessing
 * usize n = ALGO_FILTER_TYPED(out, 6, in, 6, int, is_positive, NULL);
 *
 * // Slice variant
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FILTER(int)
 * slice_int sv = slice_int_from(in, 6);
 * usize n = algo_filter_slice_int(sv, out, 6, is_positive, NULL);
 * ```
 *
 * @sa algo/any_all.h — algo_pred_fn predicate type
 * @sa core/slice.h   — slice_##type used by DEFINE_ALGO_FILTER
 * @sa core/primitives/ptr.h — ptr_elem_const used in generic interface
 */

/* ════════════════════════════════════════════════════════════════════════════
   Generic flat-array interface
   ════════════════════════════════════════════════════════════════════════════
   Uses ptr_elem_const() from ptr.h to index input by byte stride.
   Uses ptr_elem() to index output by byte stride.
   Predicate type is algo_pred_fn from any_all.h — no duplicate typedef.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Filters a flat array into a caller-provided output buffer
 *
 * Iterates all elements. For each element where pred returns true,
 * copies it into the output buffer. Stops writing (but keeps iterating
 * input) when out_cap is reached.
 *
 * @param base      Pointer to first input element (NULL-safe)
 * @param len       Number of input elements
 * @param elem_size Size of each element in bytes (sizeof(type))
 * @param pred      Predicate — return true to keep element (NULL-safe)
 * @param ctx       Optional context passed to pred (may be NULL)
 * @param out       Caller-provided output buffer (NULL-safe)
 * @param out_cap   Maximum elements out can hold
 * @return Number of elements written to out (≤ out_cap)
 *
 * @note Returns 0 if base == NULL, pred == NULL, out == NULL,
 *       len == 0, elem_size == 0, or out_cap == 0
 * @note Relative order of matching elements is preserved
 *
 * Performance:
 * - Time:  O(n) — all input elements are checked
 * - Space: O(1)
 */
static inline usize algo_filter(
    const void*  base,
    usize        len,
    usize        elem_size,
    algo_pred_fn pred,
    void*        ctx,
    void*        out,
    usize        out_cap)
{
    if (!base || !pred || !out || len == 0 || elem_size == 0 || out_cap == 0) {
        return 0;
    }

    usize written = 0;
    for (usize i = 0; i < len; i++) {
        const void* elem = ptr_elem_const(base, i, elem_size);
        if (pred(elem, ctx)) {
            if (written >= out_cap) break; /* truncate */
            void* dest = ptr_elem(out, written, elem_size);
            memcpy(dest, elem, elem_size);
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
 * @param out     Output array of Type (caller-provided writable buffer)
 * @param out_cap Maximum elements out can hold (explicit — not sizeof/sizeof)
 * @param in      Input array of Type (read-only)
 * @param in_len  Number of input elements
 * @param Type    Element type
 * @param pred    Predicate: bool (*)(const void*, void*) — algo_pred_fn
 * @param ctx     Optional context (may be NULL)
 * @return usize — number of elements written to out
 *
 * Example:
 * ```c
 * bool is_even(const void* e, void* ctx) {
 *     return (*(const int*)e % 2) == 0;
 * }
 * int in[]  = {1, 2, 3, 4, 5, 6};
 * int out[6];
 * usize n = ALGO_FILTER_TYPED(out, 6, in, 6, int, is_even, NULL);
 * // n == 3, out == {2, 4, 6}
 * ```
 */
#define ALGO_FILTER_TYPED(out, out_cap, in, in_len, Type, pred, ctx) \
    algo_filter( \
        (in),  (in_len),  sizeof(Type), \
        (algo_pred_fn)(pred), (ctx), \
        (out), (out_cap))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_FILTER — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   The predicate receives a typed const type* — no void* cast at the call site.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a filter function that accepts slice_##type input directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated function:
 * - algo_filter_slice_##type(sv, out, out_cap, pred, ctx) → usize
 *
 * The predicate receives a const type* to each element — no void* cast needed.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FILTER(int)
 *
 * bool is_positive(const int* e, void* ctx) { return *e > 0; }
 *
 * int arr[] = {-1, 2, -3, 4, 5};
 * int out[5];
 * slice_int sv = slice_int_from(arr, 5);
 *
 * usize n = algo_filter_slice_int(sv, out, 5, is_positive, NULL);
 * // n == 3, out == {2, 4, 5}
 * ```
 */
#define DEFINE_ALGO_FILTER(type) \
\
/** \
 * @brief Filters slice_##type elements into a caller-provided output array \
 * \
 * @param sv      Input slice (non-owning) \
 * @param out     Output array of type (caller-provided) \
 * @param out_cap Maximum elements out can hold \
 * @param pred    Predicate: bool (*)(const type*, void*) \
 * @param ctx     Optional context (may be NULL) \
 * @return usize — number of elements written to out \
 * \
 * Performance: O(n) time, O(1) space \
 */ \
static inline usize algo_filter_slice_##type( \
    slice_##type sv, \
    type*        out, \
    usize        out_cap, \
    bool       (*pred)(const type*, void*), \
    void*        ctx) \
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
