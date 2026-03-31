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
 * SWAP BUFFER SIZE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_sort uses a fixed stack buffer of ALGO_SORT_SWAP_BUF_SIZE bytes
 * (default 256) for swapping individual elements during insertion sort.
 * Elements larger than this buffer trigger a contract failure — consistent
 * with algo_reverse's ALGO_REVERSE_SWAP_BUF_SIZE contract. Override before
 * including this header:
 * ```c
 * #define ALGO_SORT_SWAP_BUF_SIZE ((usize)1024)
 * #include "algo/sort/sort.h"
 * ```
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
 * algo_sort: O(n log n) with temp, O(n²) without. Stable.
 * algo_is_sorted: O(n) worst, O(1) best, O(1) space.
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
 */
#define ALGO_SORT_LINKAGE static inline

#include "sort_impl.h"   /* implementation logic — NOT sort_defn.h */

#undef ALGO_SORT_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

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

#define ALGO_IS_SORTED_TYPED(base, len, Type, cmp, ctx) \
    algo_is_sorted((base), (usize)(len), sizeof(Type), \
        (algo_cmp_fn)(cmp), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_SORT — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates sort and sorted-check functions operating on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Slice wrappers enforce contracts (cmp non-NULL, non-empty slice has
 * non-NULL ptr) before delegating, consistent with the contract pattern
 * used by filter, find, fold, any_all, reverse, and search.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
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
    require_msg(cmp != NULL, \
        "algo_sort_slice_" #type ": cmp cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_sort_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len < 2) return; \
    void* tmp = (temp && temp_cap >= sv.len) ? (void*)temp : NULL; \
    algo_sort(sv.ptr, sv.len, sizeof(type), cmp, ctx, tmp); \
} \
\
static inline bool ALGO_IS_SORTED_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    require_msg(cmp != NULL, \
        "algo_is_sorted_slice_" #type ": cmp cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_is_sorted_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len < 2) return true; \
    return algo_is_sorted(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

#endif /* CANON_ALGO_SORT_H */
