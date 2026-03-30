/**
 * @file sort.h
 * @brief Stable in-place sort — hybrid insertion/merge sort
 *
 * This is the entry point for header-only usage. Including this file
 * generates statically-inlined implementations of algo_sort and
 * algo_is_sorted, plus typed macro wrappers and the DEFINE_ALGO_SORT
 * instantiation macro.
 *
 * For separate compilation (external linkage), use sort_decl.h in
 * headers and sort_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Level 1 — Generic (void* interface, works on any type):
 * ```c
 * int arr[] = {64, 34, 25, 12, 22, 11, 90};
 * int tmp[7];
 * algo_sort(arr, 7, sizeof(int), algo_cmp_int, NULL, tmp);
 * // arr = {11, 12, 22, 25, 34, 64, 90}
 *
 * bool ok = algo_is_sorted(arr, 7, sizeof(int), algo_cmp_int, NULL); // true
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof, automatic stack temp):
 * ```c
 * ALGO_SORT_TYPED(arr, 7, int, algo_cmp_int, NULL);
 * bool ok = ALGO_IS_SORTED_TYPED(arr, 7, int, algo_cmp_int, NULL);
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, fully optimizable):
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_SORT(int)
 *
 * int buf[] = {64, 34, 25, 12, 22, 11, 90};
 * int tmp[7];
 * slice_int sv = slice_int_from(buf, 7);
 *
 * algo_sort_slice_int(sv, algo_cmp_int, NULL, tmp, 7);
 * // buf = {11, 12, 22, 25, 34, 64, 90}
 *
 * bool ok = algo_is_sorted_slice_int(sv, algo_cmp_int, NULL); // true
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * ALGORITHM SELECTION
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_sort selects the strategy at runtime:
 *   len < 2           → no-op
 *   len < 16          → insertion sort (O(n²) worst, O(n) best)
 *   len >= 16 + temp  → merge sort (O(n log n) guaranteed, stable)
 *   len >= 16, !temp  → insertion sort fallback (O(n²))
 *
 * Both insertion sort and merge sort are stable — equal elements preserve
 * their original relative order.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * TEMP BUFFER
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_sort requires a scratch buffer of at least len * elem_size bytes for
 * merge sort. Contents are undefined after the call. The buffer must not
 * overlap with the array being sorted. Pass NULL to force insertion sort.
 *
 * ALGO_SORT_TYPED allocates a stack array of Type[ALGO_SORT_STACK_TEMP_MAX]
 * (default 128). For arrays larger than ALGO_SORT_STACK_TEMP_MAX, call
 * algo_sort() directly with a caller-managed heap or arena buffer.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * EMPTY AND SINGLE-ELEMENT BEHAVIOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_sort()      — len == 0 or len == 1: returns immediately, no-op.
 * algo_is_sorted() — len == 0 or len == 1: returns true (vacuously sorted).
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PERFORMANCE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_sort / ALGO_SORT_TYPED / algo_sort_slice_##type:
 *   len < 2:            O(1) — immediate return
 *   len in [2, 15]:     O(n²) worst / O(n) best — insertion sort,
 *                       cache-friendly, optimal for short sequences
 *   len >= 16, temp:    O(n log n) worst/average — stable merge sort,
 *                       recursion depth O(log n), O(n) additional space
 *   len >= 16, no temp: O(n²) worst — insertion sort fallback; useful when
 *                       heap or arena allocation is unavailable
 *   Stable: yes — equal elements always preserve original relative order
 *
 * algo_is_sorted / ALGO_IS_SORTED_TYPED / algo_is_sorted_slice_##type:
 *   Best case:  O(1) — first pair is out of order
 *   Worst case: O(n) — array is sorted (or has mismatch only at last pair)
 *   Space:      O(1) — no allocation, two pointer indices on the stack
 *   cmp calls:  0 (len < 2) to n-1 (sorted array)
 *   Array is never modified.
 *
 * Level comparison:
 *   Level 1 — Generic: one stride multiply per element access.
 *   Level 2 — Typed macro: sizeof is compile-time; the stack temp array is
 *              a fixed-size array (not a VLA), so no stack size uncertainty.
 *   Level 3 — Typed slice: delegates to the generic implementation;
 *              sizeof(type) is a compile-time constant at the call site.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_sort(base, len, elem_size, cmp, ctx, temp_buffer)  → void
 *   algo_is_sorted(base, len, elem_size, cmp, ctx)          → bool
 *
 * Typed macros:
 *   ALGO_SORT_TYPED(base, len, Type, cmp, ctx)              → void
 *   ALGO_IS_SORTED_TYPED(base, len, Type, cmp, ctx)         → bool
 *
 * Typed instantiation (call DEFINE_ALGO_SORT(type) first):
 *   algo_sort_slice_##type(sv, cmp, ctx, temp, temp_cap)    → void
 *   algo_is_sorted_slice_##type(sv, cmp, ctx)               → bool
 *
 * @sa sort_mangle.h — name and size constant customization
 * @sa sort_decl.h   — forward declarations for separate compilation
 * @sa sort_defn.h   — definitions for separate compilation
 * @sa sort_impl.h   — pure logic (do not include directly)
 * @sa core/slice.h  — slice_##type used by DEFINE_ALGO_SORT
 * @sa core/primitives/compare.h — algo_cmp_fn and built-in comparators
 */

#ifndef CANON_ALGO_SORT_H
#define CANON_ALGO_SORT_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — algo_sort and algo_is_sorted are inlined
 * at call sites. The four internal helpers are always static inline regardless.
 */
#define ALGO_SORT_LINKAGE static inline

#include "sort_impl.h"   /* implementation logic — NOT sort_defn.h */

#undef ALGO_SORT_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_SORT_TYPED(base, len, Type, cmp, ctx)
 * @brief Type-safe in-place sort with automatic stack temp buffer
 *
 * For len <= ALGO_SORT_STACK_TEMP_MAX: allocates a fixed-size stack array
 * of Type[ALGO_SORT_STACK_TEMP_MAX] as the merge sort scratch buffer.
 * For len > ALGO_SORT_STACK_TEMP_MAX: passes NULL, falls back to O(n²)
 * insertion sort. For large arrays, call algo_sort() directly with a
 * caller-managed heap or arena buffer.
 *
 * ALGO_SORT_STACK_TEMP_MAX is a compile-time constant (default 128) —
 * the stack array is not a VLA.
 *
 * @param base Array of Type to sort in-place (borrowed, modified in place)
 * @param len  Number of elements
 * @param Type Element type — used for sizeof and stack array declaration
 * @param cmp  algo_cmp_fn comparator (borrowed)
 * @param ctx  Optional context (borrowed, may be NULL)
 *
 * Performance:
 * - Time:  O(n log n) if len <= ALGO_SORT_STACK_TEMP_MAX, O(n²) otherwise
 * - Space: O(ALGO_SORT_STACK_TEMP_MAX * sizeof(Type)) stack for small arrays
 */
#define ALGO_SORT_TYPED(base, len, Type, cmp, ctx) \
    do { \
        usize _asl_len = (usize)(len); \
        if (_asl_len <= (usize)ALGO_SORT_STACK_TEMP_MAX) { \
            Type _asl_tmp[ALGO_SORT_STACK_TEMP_MAX]; \
            algo_sort((base), _asl_len, sizeof(Type), \
                      (algo_cmp_fn)(cmp), (ctx), _asl_tmp); \
        } else { \
            algo_sort((base), _asl_len, sizeof(Type), \
                      (algo_cmp_fn)(cmp), (ctx), NULL); \
        } \
    } while (0)

/**
 * @def ALGO_IS_SORTED_TYPED(base, len, Type, cmp, ctx)
 * @brief Type-safe sorted check — evaluates to bool
 *
 * Wraps algo_is_sorted() with automatic sizeof(Type).
 * Returns true for empty and single-element arrays.
 *
 * @param base Array of Type (borrowed, read-only)
 * @param len  Number of elements (0 and 1 are valid — returns true)
 * @param Type Element type — used for sizeof only
 * @param cmp  algo_cmp_fn comparator (borrowed)
 * @param ctx  Optional context (borrowed, may be NULL)
 *
 * Performance: O(n) worst, O(1) best, O(1) space.
 *
 * @return bool — true if sorted or len < 2
 */
#define ALGO_IS_SORTED_TYPED(base, len, Type, cmp, ctx) \
    algo_is_sorted((base), (usize)(len), sizeof(Type), \
        (algo_cmp_fn)(cmp), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_SORT — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   Generates fully typed functions accepting slice_##type directly.
   No void* anywhere — the compiler sees the actual element type.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates sort and sorted-check functions operating on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 *   algo_sort_slice_##type(sv, cmp, ctx, temp, temp_cap) → void
 *     Sorts sv in place. temp and temp_cap describe a caller-provided scratch
 *     buffer of type[temp_cap] elements. If temp == NULL or temp_cap < sv.len,
 *     falls back to insertion sort. Caller retains ownership of temp.
 *
 *   algo_is_sorted_slice_##type(sv, cmp, ctx) → bool
 *     Returns true if sv.ptr is sorted according to cmp.
 *     Returns true when sv.len < 2 (vacuously sorted).
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h invariants). algo_sort_slice guards against len < 2 before
 * delegating. algo_is_sorted handles NULL base internally (returns true).
 *
 * Performance:
 *   algo_sort_slice_##type:     same as algo_sort — O(n log n) with temp,
 *                               O(n²) without
 *   algo_is_sorted_slice_##type: O(n) worst, O(1) best, O(1) space
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_SORT(int)
 *
 * int buf[] = {5, 3, 1, 4, 2};
 * int tmp[5];
 * slice_int sv = slice_int_from(buf, 5);
 *
 * algo_sort_slice_int(sv, algo_cmp_int, NULL, tmp, 5);
 * // buf = {1, 2, 3, 4, 5}
 *
 * bool sorted = algo_is_sorted_slice_int(sv, algo_cmp_int, NULL); // true
 *
 * // No temp — falls back to insertion sort (O(n²)):
 * algo_sort_slice_int(sv, algo_cmp_int, NULL, NULL, 0);
 *
 * // Empty slice — no crash, no-op:
 * slice_int empty = slice_int_empty();
 * algo_sort_slice_int(empty, algo_cmp_int, NULL, NULL, 0); // no-op
 * bool r = algo_is_sorted_slice_int(empty, algo_cmp_int, NULL); // true
 * ```
 */
#define DEFINE_ALGO_SORT(type) \
\
static inline void ALGO_SORT_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx, \
    borrowed(type*)         temp, \
    usize                   temp_cap) \
{ \
    if (!sv.ptr || sv.len < 2) return; \
    void* tmp = (temp && temp_cap >= sv.len) ? (void*)temp : NULL; \
    algo_sort(sv.ptr, sv.len, sizeof(type), cmp, ctx, tmp); \
} \
\
static inline bool ALGO_IS_SORTED_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    return algo_is_sorted(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

#endif /* CANON_ALGO_SORT_H */
