#ifndef CANON_ALGO_FIND_H
#define CANON_ALGO_FIND_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "semantics/option.h"
#include "algo/any_all.h"

/**
 * @file algo/find.h
 * @brief Locate the first element in a sequence matching a predicate
 *
 * Scans a sequence left-to-right and returns the index and/or a pointer to
 * the first element satisfying the predicate. Short-circuits on first match.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Input is read-only — never mutated
 * - Short-circuit evaluation — stops immediately on first match
 * - No allocation — O(1) space
 * - Predicate type is algo_pred_fn from algo/any_all.h — no duplicate typedef
 * - Three levels: generic flat-array, typed inline function, slice+option variant
 * - Slice variants return option_##type — Canon-C idiom for "found or absent"
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/ and semantics/ only.
 * algo/find.h additionally includes algo/any_all.h for algo_pred_fn.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No GCC statement expressions — all macros are C99 portable
 * - No platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(k) where k = index of first match (worst case O(n))
 * - Space: O(1) — no allocation
 *
 * Quick start:
 * ```c
 * #include "algo/find.h"
 *
 * bool is_negative(const void* e, void* ctx) { return *(const int*)e < 0; }
 *
 * int arr[] = {5, 3, -2, 7};
 *
 * // Generic — returns index via out param
 * usize idx;
 * const void* elem;
 * bool found = algo_find(arr, 4, sizeof(int), is_negative, NULL, &idx, &elem);
 *
 * // Typed inline — clean call, typed out_elem
 * const int* p = NULL;
 * bool found = algo_find_typed(arr, 4, int, is_negative, NULL, &idx, &p);
 *
 * // Slice variant — returns option_int
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FIND(int)
 * slice_int sv = slice_int_from(arr, 4);
 * option_int opt = algo_find_slice_int(sv, is_negative, NULL);
 * if (opt.has_value) { printf("%d\n", opt.value); }
 * ```
 *
 * @sa algo/any_all.h    — algo_pred_fn predicate type
 * @sa core/slice.h      — slice_##type used by DEFINE_ALGO_FIND
 * @sa semantics/option.h — option_##type returned by slice variant
 * @sa core/primitives/ptr.h — ptr_elem_const used in generic interface
 */

/* ════════════════════════════════════════════════════════════════════════════
   Generic flat-array interface
   ════════════════════════════════════════════════════════════════════════════
   Uses ptr_elem_const() from ptr.h to stride over the flat array.
   Both out_index and out_elem are optional — pass NULL to ignore.
   Predicate type is algo_pred_fn from any_all.h.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Finds the first element in a flat array satisfying the predicate
 *
 * @param base      Pointer to first element (NULL-safe)
 * @param len       Number of elements
 * @param elem_size Size of each element in bytes (sizeof(type))
 * @param pred      Predicate — return true on match (NULL-safe)
 * @param ctx       Optional context passed to pred (may be NULL)
 * @param out_index Optional — receives index of first match (may be NULL)
 * @param out_elem  Optional — receives const pointer to match (may be NULL)
 * @return true if a match was found, false otherwise
 *
 * @note On false, out_index and out_elem are not modified
 * @note Returns false immediately if base == NULL, pred == NULL, or len == 0
 *
 * Performance:
 * - Time:  O(k) short-circuit, O(n) worst case
 * - Space: O(1)
 */
static inline bool algo_find(
    const void*   base,
    usize         len,
    usize         elem_size,
    algo_pred_fn  pred,
    void*         ctx,
    usize*        out_index,  /* optional */
    const void**  out_elem)   /* optional */
{
    if (!base || !pred || len == 0 || elem_size == 0) return false;

    for (usize i = 0; i < len; i++) {
        const void* elem = ptr_elem_const(base, i, elem_size);
        if (pred(elem, ctx)) {
            if (out_index) *out_index = i;
            if (out_elem)  *out_elem  = elem;
            return true;
        }
    }
    return false;
}

/* ════════════════════════════════════════════════════════════════════════════
   Typed macro — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════
   C99-portable. Typed out_elem avoids the void** cast at the call site.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_FIND_TYPED(base, len, Type, pred, ctx, out_index, out_elem_ptr)
 * @brief Type-safe find over a C array with optional typed output pointer
 *
 * @param base         Input array of Type (read-only)
 * @param len          Number of elements
 * @param Type         Element type
 * @param pred         Predicate: algo_pred_fn (bool (*)(const void*, void*))
 * @param ctx          Optional context (may be NULL)
 * @param out_index    Optional usize* — receives index of match (may be NULL)
 * @param out_elem_ptr Optional const Type** — receives pointer to match (may be NULL)
 * @return bool — true if found
 *
 * Example:
 * ```c
 * bool is_negative(const void* e, void* ctx) { return *(const int*)e < 0; }
 *
 * int arr[] = {5, 3, -2, 7};
 * usize idx;
 * const int* p = NULL;
 * if (ALGO_FIND_TYPED(arr, 4, int, is_negative, NULL, &idx, &p)) {
 *     printf("found %d at %zu\n", *p, idx);
 * }
 * ```
 */
#define ALGO_FIND_TYPED(base, len, Type, pred, ctx, out_index, out_elem_ptr) \
    algo_find( \
        (base), (len), sizeof(Type), \
        (algo_pred_fn)(pred), (ctx), \
        (out_index), \
        (const void**)(out_elem_ptr))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_FIND — typed slice + option variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires:
   - DEFINE_SLICE(type) from core/slice.h
   - DEFINE_OPTION(type) from semantics/option.h (or option.h pulled in type)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates find functions that accept slice_##type and return option_##type
 *
 * Prerequisites:
 * - DEFINE_SLICE(type) must have been called
 * - option_##type must exist (via DEFINE_OPTION(type))
 *
 * Generated functions:
 * - algo_find_slice_##type(sv, pred, ctx)          → option_##type
 * - algo_find_index_slice_##type(sv, pred, ctx)    → usize (CANON_USIZE_MAX if not found)
 *
 * The predicate receives a typed const type* — no void* cast needed.
 * The value variant returns option_##type (Some on found, None on not found).
 * The index variant returns the element index or CANON_USIZE_MAX if not found.
 *
 * @param type Element type — must match prior DEFINE_SLICE and DEFINE_OPTION calls
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_FIND(int)
 *
 * bool is_negative(const int* e, void* ctx) { return *e < 0; }
 *
 * int arr[] = {5, 3, -2, 7};
 * slice_int sv = slice_int_from(arr, 4);
 *
 * option_int opt = algo_find_slice_int(sv, is_negative, NULL);
 * if (opt.has_value) { printf("found: %d\n", opt.value); }
 *
 * usize idx = algo_find_index_slice_int(sv, is_negative, NULL);
 * if (idx != CANON_USIZE_MAX) { printf("at index: %zu\n", idx); }
 * ```
 */
#define DEFINE_ALGO_FIND(type) \
\
/** \
 * @brief Returns the first element in slice_##type satisfying pred as option_##type \
 * \
 * @param sv   Input slice (non-owning) \
 * @param pred Predicate: bool (*)(const type*, void*) \
 * @param ctx  Optional context (may be NULL) \
 * @return option_##type — Some(value) on match, None if not found or empty \
 * \
 * Performance: O(k) short-circuit, O(n) worst case \
 */ \
static inline option_##type algo_find_slice_##type( \
    slice_##type sv, \
    bool (*pred)(const type*, void*), \
    void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return option_##type##_none(); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) { \
            return option_##type##_some(sv.ptr[_i]); \
        } \
    } \
    return option_##type##_none(); \
} \
\
/** \
 * @brief Returns the index of the first matching element, or CANON_USIZE_MAX if not found \
 * \
 * @param sv   Input slice (non-owning) \
 * @param pred Predicate: bool (*)(const type*, void*) \
 * @param ctx  Optional context (may be NULL) \
 * @return usize — index of first match, or CANON_USIZE_MAX if not found \
 * \
 * Performance: O(k) short-circuit, O(n) worst case \
 */ \
static inline usize algo_find_index_slice_##type( \
    slice_##type sv, \
    bool (*pred)(const type*, void*), \
    void* ctx) \
{ \
    if (!sv.ptr || sv.len == 0 || !pred) return CANON_USIZE_MAX; \
    for (usize _i = 0; _i < sv.len; _i++) { \
        if (pred(&sv.ptr[_i], ctx)) return _i; \
    } \
    return CANON_USIZE_MAX; \
}

#endif /* CANON_ALGO_FIND_H */
