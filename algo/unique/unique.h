/**
 * @file unique.h
 * @brief Remove consecutive duplicate elements (in-place)
 *
 * This is the entry point for header-only usage. Including this file
 * generates a statically-inlined implementation of algo_unique, plus
 * a typed macro wrapper and the DEFINE_ALGO_UNIQUE instantiation macro.
 *
 * For separate compilation (external linkage), use unique_decl.h in
 * headers and unique_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * IMPORTANT: CONSECUTIVE DUPLICATES ONLY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_unique collapses runs of consecutive equal elements — it does not
 * perform full deduplication across non-adjacent positions. For full
 * deduplication, sort the array first with the same comparator:
 *
 *   // Wrong — non-adjacent duplicates survive:
 *   int arr[] = {3, 1, 2, 1, 2, 3};
 *   usize n = algo_unique(arr, 6, sizeof(int), cmp_int, NULL);
 *   // n == 6 — nothing removed (no consecutive equal pairs)
 *
 *   // Correct — sort first, then unique:
 *   int tmp[6];
 *   algo_sort(arr, 6, sizeof(int), cmp_int, NULL, tmp);
 *   // arr = {1, 1, 2, 2, 3, 3}
 *   usize n = algo_unique(arr, 6, sizeof(int), cmp_int, NULL);
 *   // n == 3, arr[0..2] = {1, 2, 3}
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Level 1 — Generic (void* interface, works on any type):
 * ```c
 * int arr[] = {1, 1, 2, 2, 3};
 * usize n = algo_unique(arr, 5, sizeof(int), algo_cmp_int, NULL);
 * // n == 3, arr[0..2] = {1, 2, 3}
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof, same fn signature):
 * ```c
 * usize n = ALGO_UNIQUE_TYPED(arr, 5, int, algo_cmp_int, NULL);
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, fully optimizable):
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_UNIQUE(int)
 *
 * int buf[] = {1, 1, 2, 2, 3};
 * slice_int sv = slice_int_from(buf, 5);
 * usize n = algo_unique_slice_int(sv, algo_cmp_int, NULL);
 * // n == 3, buf[0..2] = {1, 2, 3}
 * // caller must update sv.len = n if they want a valid slice view
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * RETURN VALUE AND CALLER RESPONSIBILITY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_unique returns the new logical length. Elements at [0, return) are
 * valid. Elements at [return, len) contain stale data — do not read them.
 * The slice or container length must be updated by the caller:
 *
 *   sv.len = algo_unique_slice_int(sv, cmp, NULL);
 *   // or for a vec:
 *   vec.len = algo_unique(vec.items, vec.len, sizeof(T), cmp, NULL);
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * EMPTY AND SINGLE-ELEMENT BEHAVIOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * len == 0 → returns 0
 * len == 1 → returns 1 (trivially unique, no comparisons made)
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PERFORMANCE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_unique / ALGO_UNIQUE_TYPED / algo_unique_slice_##type:
 *   Time:   O(n) — single pass; exactly n-1 comparisons for len >= 2;
 *           0 comparisons for len <= 1.
 *   Space:  O(1) — no allocation. At most n-1 element copies when every
 *           element is unique; 0 copies when all elements are equal (only
 *           write advances, but write == read self-copies are skipped).
 *   Best case:  O(1) — len <= 1 (immediate return)
 *   Worst case: O(n) — always, since every element is visited exactly once
 *   cmp calls:  exactly n-1 (for n >= 2), 0 otherwise
 *   Element copies: between 0 and n-1 depending on duplicate density
 *
 * Note: self-copies are avoided — when no duplicates have been seen,
 * write == read and the copy is skipped, so a fully-unique array has
 * zero element copies.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_unique(array, len, elem_size, cmp, ctx)          → usize
 *
 * Typed macro:
 *   ALGO_UNIQUE_TYPED(array, len, Type, cmp, ctx)         → usize
 *
 * Typed instantiation (call DEFINE_ALGO_UNIQUE(type) first):
 *   algo_unique_slice_##type(sv, cmp, ctx)                → usize
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * COMPARATOR SIGNATURE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * All three levels use algo_cmp_fn from core/primitives/compare.h:
 *   int cmp(const void* a, const void* b, void* ctx)
 *
 * Return 0 if elements are equal, any nonzero value otherwise.
 * a and b are never NULL — they always point to valid array elements.
 * ctx is the optional caller context passed through unchanged; may be NULL.
 *
 * @sa unique_mangle.h — name customization for slice variant
 * @sa unique_decl.h   — forward declaration for separate compilation
 * @sa unique_defn.h   — definition for separate compilation
 * @sa unique_impl.h   — pure logic (do not include directly)
 * @sa core/slice.h    — slice_##type used by DEFINE_ALGO_UNIQUE
 * @sa core/primitives/compare.h — algo_cmp_fn definition
 */

#ifndef CANON_ALGO_UNIQUE_H
#define CANON_ALGO_UNIQUE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — algo_unique is inlined at call sites.
 */
#define ALGO_UNIQUE_LINKAGE static inline

#include "unique_impl.h"   /* implementation logic — NOT unique_defn.h */

#undef ALGO_UNIQUE_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macro — single consistent calling convention
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_UNIQUE_TYPED(array, len, Type, cmp, ctx)
 * @brief Type-safe unique — removes consecutive duplicates, returns new length
 *
 * Wraps algo_unique() with automatic sizeof(Type), eliminating the manual
 * elem_size argument. Always returns usize — no GNU extensions required.
 *
 * Performance: O(n) time, O(1) space.
 *
 * @param array Array of Type (borrowed, modified in place)
 * @param len   Number of elements (0 and 1 are valid — returns len)
 * @param Type  Element type — used for sizeof only
 * @param cmp   Comparator: returns 0 if elements are equal (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return usize — new logical length after removing consecutive duplicates
 */
#define ALGO_UNIQUE_TYPED(array, len, Type, cmp, ctx) \
    algo_unique((array), (usize)(len), sizeof(Type), \
        (algo_cmp_fn)(cmp), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_UNIQUE — typed slice variant per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   Generates a fully typed function accepting slice_##type directly.
   No void* anywhere — the compiler sees the actual element type.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a unique function operating directly on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated function:
 *   algo_unique_slice_##type(sv, cmp, ctx) → usize
 *     Removes consecutive duplicates from the backing array of sv in place.
 *     Returns the new logical length of the unique prefix.
 *     The slice view itself is NOT updated — the caller must assign the
 *     returned value back to sv.len (or their vec.len) after the call.
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h invariants). algo_unique returns len immediately when len <= 1,
 * so a NULL ptr with len == 0 is safe after the precondition checks.
 *
 * Performance: O(n) time, O(1) space.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_UNIQUE(int)
 *
 * int buf[] = {1, 1, 2, 2, 3};
 * slice_int sv = slice_int_from(buf, 5);
 * sv.len = algo_unique_slice_int(sv, algo_cmp_int, NULL);
 * // sv.len == 3, buf[0..2] = {1, 2, 3}
 *
 * // Empty slice — no crash, returns 0:
 * slice_int empty = slice_int_empty();
 * usize n = algo_unique_slice_int(empty, algo_cmp_int, NULL); // 0
 * ```
 */
#define DEFINE_ALGO_UNIQUE(type) \
\
static inline usize ALGO_UNIQUE_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    return algo_unique(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

#endif /* CANON_ALGO_UNIQUE_H */
