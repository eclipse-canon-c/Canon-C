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
 * len == 0 → returns 0 (or 0 if array is NULL)
 * len == 1 → returns 1 (trivially unique, no comparisons made)
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

#include "unique_defn.h"

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
 * @param array Array of Type (borrowed, modified in place)
 * @param len   Number of elements (0 and 1 are valid — returns len)
 * @param Type  Element type — used for sizeof only
 * @param cmp   Comparator: returns 0 if elements are equal (borrowed)
 * @param ctx   Optional context (borrowed, may be NULL)
 *
 * @return usize — new logical length after removing consecutive duplicates
 *
 * Example:
 * ```c
 * int arr[] = {1, 1, 2, 2, 3};
 * usize n = ALGO_UNIQUE_TYPED(arr, 5, int, algo_cmp_int, NULL);
 * // n == 3, arr[0..2] = {1, 2, 3}
 * ```
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
 * slice.h invariants). The function returns len immediately when len <= 1,
 * so a NULL ptr with len == 0 is safe.
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
/** \
 * @brief Removes consecutive duplicates from a slice_##type in place \
 * \
 * Returns the new logical length. Elements at sv.ptr[0..return-1] are \
 * valid; elements at sv.ptr[return..sv.len-1] contain stale data. \
 * The caller must update sv.len to the returned value. \
 * \
 * @param sv  Typed slice over the array to deduplicate (borrowed) \
 * @param cmp Comparator — returns 0 if elements are equal (borrowed) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * @return New logical length after removing consecutive duplicates \
 */ \
static inline usize ALGO_UNIQUE_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    return algo_unique(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

#endif /* CANON_ALGO_UNIQUE_H */
