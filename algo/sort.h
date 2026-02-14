#ifndef CANON_ALGO_SORT_H
#define CANON_ALGO_SORT_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"

/**
 * @file algo/sort.h
 * @brief Stable in-place sort — hybrid insertion/merge sort
 *
 * Sorts a contiguous array in-place using a caller-supplied comparator.
 * Automatically selects insertion sort (len < 16) or merge sort (len >= 16).
 * Both are stable — equal elements preserve their original relative order.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Stable: equal elements keep their original relative order
 * - Hybrid: insertion sort for small arrays, merge sort for large ones
 * - No allocation inside functions — caller provides temp buffer for merge sort
 * - Inner swap uses a fixed 256-byte stack buffer — no VLAs, no capacity issues
 * - Predicate: algo_cmp_fn (three-way, context-aware)
 * - Explicit ownership: all borrowed parameters marked with borrowed macro
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * algo/ headers may include core/ and semantics/ only.
 * sort.h uses core/memory.h for mem_copy instead of memcpy directly.
 *
 * Algorithm selection:
 * ────────────────────────────────────────────────────────────────────────────
 * - len < 2:  no-op
 * - len < 16: insertion sort — O(n²) worst, O(n) best, cache-friendly
 * - len ≥ 16: merge sort — O(n log n) guaranteed, stable, requires temp buffer
 *             if temp_buffer == NULL, falls back to insertion sort
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time:  O(n log n) with temp buffer, O(n²) fallback
 * - Space: O(n) for temp buffer (caller-provided), O(log n) stack (recursion)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions are thread-safe (no shared mutable state)
 * - Safe to sort different arrays concurrently from multiple threads
 * - Same array should not be sorted concurrently without external synchronization
 *
 * Quick start:
 * ```c
 * #include "algo/sort.h"
 *
 * int cmp_int(const void* a, const void* b, void* ctx) {
 *     int x = *(const int*)a, y = *(const int*)b;
 *     return (x > y) - (x < y);
 * }
 *
 * int arr[] = {64, 34, 25, 12, 22, 11, 90};
 * int tmp[7];
 * algo_sort(arr, 7, sizeof(int), cmp_int, NULL, tmp);
 * // arr == {11, 12, 22, 25, 34, 64, 90}
 *
 * // Typed macro — fixed stack temp, no VLA
 * ALGO_SORT_TYPED(arr, 7, int, cmp_int, NULL);
 *
 * // Slice variant
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_SORT(int)
 * int tmp2[7];
 * slice_int sv = slice_int_from(arr, 7);
 * algo_sort_slice_int(sv, cmp_int, NULL, tmp2, 7);
 * ```
 *
 * @sa core/primitives/ptr.h  — ptr_elem used for index access in internals
 * @sa core/memory.h           — mem_copy, mem_move used for element operations
 * @sa core/slice.h            — slice_##type used by DEFINE_ALGO_SORT
 * @sa core/ownership.h        — borrowed macro for non-owning parameters
 */

/* ════════════════════════════════════════════════════════════════════════════
   Comparator type
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Three-way comparison function for sort operations
 *
 * @param a   Pointer to first element (never NULL)
 * @param b   Pointer to second element (never NULL)
 * @param ctx Optional caller context (may be NULL)
 * @return < 0 if a < b, 0 if a == b, > 0 if a > b
 *
 * Must be consistent (total order):
 * - cmp(a, a) == 0
 * - cmp(a, b) < 0 implies cmp(b, a) > 0
 * - Transitive: cmp(a,b) < 0 && cmp(b,c) < 0 implies cmp(a,c) < 0
 *
 * Thread-safety: Must be thread-safe if sorting from multiple threads
 * Performance: Called O(n log n) times — keep it fast
 */
typedef int (*algo_cmp_fn)(const void* a, const void* b, void* ctx);

/* ════════════════════════════════════════════════════════════════════════════
   Internal swap helper
   ════════════════════════════════════════════════════════════════════════════
   Uses a fixed 256-byte stack buffer — no VLA, no CANON_MEM_SWAP_MAX
   truncation risk. Falls back to a byte-by-byte loop for larger elements.
   ════════════════════════════════════════════════════════════════════════════ */

#define ALGO_SORT_SWAP_BUF_SIZE ((usize)256)

static inline void algo_sort_swap(void* a, void* b, usize elem_size) {
    u8 tmp[ALGO_SORT_SWAP_BUF_SIZE];
    if (elem_size <= ALGO_SORT_SWAP_BUF_SIZE) {
        mem_copy(tmp, a, elem_size);
        mem_copy(a, b, elem_size);
        mem_copy(b, tmp, elem_size);
    } else {
        /* byte-by-byte for oversized elements — rare, correctness over speed */
        u8* pa = (u8*)a;
        u8* pb = (u8*)b;
        for (usize k = 0; k < elem_size; k++) {
            u8 t = pa[k];
            pa[k] = pb[k];
            pb[k] = t;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Internal: insertion sort over [left, right)
   ════════════════════════════════════════════════════════════════════════════ */

static inline void algo_insertion_sort_range(
    void*        base,
    usize        left,
    usize        right,
    usize        elem_size,
    algo_cmp_fn  cmp,
    void*        ctx)
{
    for (usize i = left + 1; i < right; i++) {
        for (usize j = i; j > left; j--) {
            void* a = ptr_elem(base, j - 1, elem_size);
            void* b = ptr_elem(base, j,     elem_size);
            if (cmp(a, b, ctx) <= 0) break;
            algo_sort_swap(a, b, elem_size);
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Internal: merge [left, mid) and [mid, right) via temp buffer
   ════════════════════════════════════════════════════════════════════════════ */

static inline void algo_merge(
    void*       base,
    usize       left,
    usize       mid,
    usize       right,
    usize       elem_size,
    algo_cmp_fn cmp,
    void*       ctx,
    void*       temp)
{
    usize i = left, j = mid, k = 0;

    while (i < mid && j < right) {
        const void* le = ptr_elem_const(base, i, elem_size);
        const void* re = ptr_elem_const(base, j, elem_size);
        /* <= for stability: equal elements from left come first */
        if (cmp(le, re, ctx) <= 0) {
            mem_copy(ptr_elem(temp, k, elem_size), le, elem_size);
            i++;
        } else {
            mem_copy(ptr_elem(temp, k, elem_size), re, elem_size);
            j++;
        }
        k++;
    }
    while (i < mid) {
        mem_copy(ptr_elem(temp, k, elem_size),
                 ptr_elem_const(base, i, elem_size), elem_size);
        i++; k++;
    }
    while (j < right) {
        mem_copy(ptr_elem(temp, k, elem_size),
                 ptr_elem_const(base, j, elem_size), elem_size);
        j++; k++;
    }
    mem_copy(ptr_elem(base, left, elem_size), temp, (right - left) * elem_size);
}

/* ════════════════════════════════════════════════════════════════════════════
   Internal: recursive merge sort over [left, right)
   ════════════════════════════════════════════════════════════════════════════ */

static inline void algo_merge_sort_range(
    void*       base,
    usize       left,
    usize       right,
    usize       elem_size,
    algo_cmp_fn cmp,
    void*       ctx,
    void*       temp)
{
    if (right - left <= 1) return;
    if (right - left < 16) {
        algo_insertion_sort_range(base, left, right, elem_size, cmp, ctx);
        return;
    }
    usize mid = left + (right - left) / 2;
    algo_merge_sort_range(base, left, mid,   elem_size, cmp, ctx, temp);
    algo_merge_sort_range(base, mid,  right, elem_size, cmp, ctx, temp);
    algo_merge(base, left, mid, right, elem_size, cmp, ctx, temp);
}

/* ════════════════════════════════════════════════════════════════════════════
   Public interface
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sorts a flat array in-place (stable, hybrid insertion/merge sort)
 *
 * @param base        Pointer to first element (borrowed — not owned, do not free)
 * @param len         Number of elements
 * @param elem_size   Size of each element in bytes (> 0)
 * @param cmp         Three-way comparator (borrowed — must remain valid during call)
 * @param ctx         Optional context passed to cmp (borrowed, may be NULL)
 * @param temp_buffer Caller-provided scratch buffer of len * elem_size bytes
 *                    (borrowed — only used during this call, not stored).
 *                    Required for merge sort (len >= 16). Pass NULL to fall
 *                    back to O(n²) insertion sort for any length.
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 * @pre If cmp != NULL, cmp implements a valid total order
 * @pre If temp_buffer != NULL, it has capacity for len elements
 * @pre base, temp_buffer do not overlap (undefined behavior if they do)
 *
 * @post Array is sorted in ascending order according to cmp
 * @post Equal elements preserve their original relative order (stable)
 * @post temp_buffer contents are undefined after return
 *
 * Ownership:
 * - base: borrowed (not modified ownership, elements reordered in place)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp, not stored)
 * - temp_buffer: borrowed (used as scratch space, not stored)
 *
 * Performance:
 * - Time:  O(n log n) with temp_buffer, O(n²) without
 * - Space: O(n) temp_buffer (caller-provided), O(log n) stack (recursion)
 *
 * Thread-safety:
 * - Safe to call concurrently on different arrays
 * - Not safe to call concurrently on same array without external sync
 */
static inline void algo_sort(
    borrowed void*       base,
    usize                len,
    usize                elem_size,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx,
    borrowed void*       temp_buffer)
{
    require_msg(elem_size > 0, "algo_sort: elem_size must be > 0");
    if (!base || len < 2 || !cmp || elem_size == 0) return;

    if (len < 16 || !temp_buffer) {
        algo_insertion_sort_range(base, 0, len, elem_size, cmp, ctx);
    } else {
        algo_merge_sort_range(base, 0, len, elem_size, cmp, ctx, temp_buffer);
    }
}

/**
 * @brief Returns true if array is sorted according to cmp (non-destructive)
 *
 * @param base      Pointer to first element (borrowed — not modified)
 * @param len       Number of elements
 * @param elem_size Size of each element in bytes
 * @param cmp       Comparator (borrowed)
 * @param ctx       Optional context (borrowed, may be NULL)
 * @return true if sorted or len < 2, false otherwise
 *
 * @pre elem_size > 0
 * @pre If base != NULL, base points to valid array of len elements
 *
 * Ownership:
 * - base: borrowed (read-only)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Thread-safety:
 * - Safe to call concurrently as long as array is not being modified
 */
static inline bool algo_is_sorted(
    borrowed const void* base,
    usize                len,
    usize                elem_size,
    borrowed algo_cmp_fn cmp,
    borrowed void*       ctx)
{
    if (!base || len < 2 || !cmp) return true;
    for (usize i = 1; i < len; i++) {
        if (cmp(ptr_elem_const(base, i - 1, elem_size),
                ptr_elem_const(base, i,     elem_size), ctx) > 0) {
            return false;
        }
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════
   C99-portable — no VLAs, no GCC statement expressions.
   Uses a fixed-size stack buffer capped at ALGO_SORT_STACK_TEMP_MAX elements.
   For arrays requiring a larger temp buffer, call algo_sort() directly with
   a caller-managed heap or arena buffer.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Maximum elements for which ALGO_SORT_TYPED allocates a stack temp buffer
 *
 * If len > this value, ALGO_SORT_TYPED passes NULL for temp_buffer and falls
 * back to insertion sort. For large arrays, call algo_sort() directly with a
 * properly sized caller-managed buffer.
 *
 * Default: 128 elements. Override by defining before including this header.
 */
#ifndef ALGO_SORT_STACK_TEMP_MAX
    #define ALGO_SORT_STACK_TEMP_MAX 128
#endif

/**
 * @def ALGO_SORT_TYPED(base, len, Type, cmp, ctx)
 * @brief Type-safe in-place sort with automatic stack temp buffer
 *
 * For len <= ALGO_SORT_STACK_TEMP_MAX: allocates a stack temp buffer and
 * uses merge sort. For larger arrays: passes NULL and falls back to
 * insertion sort. Call algo_sort() directly for large arrays.
 *
 * @param base Array of Type to sort in-place (borrowed)
 * @param len  Number of elements
 * @param Type Element type
 * @param cmp  algo_cmp_fn comparator (borrowed)
 * @param ctx  Optional context (borrowed, may be NULL)
 *
 * @note base is borrowed — array is modified in place but not freed
 *
 * Performance:
 * - Time:  O(n log n) if len <= ALGO_SORT_STACK_TEMP_MAX, O(n²) otherwise
 * - Space: O(n) stack for small arrays, O(1) for large
 *
 * Example:
 * ```c
 * int arr[] = {5, 2, 8, 1, 9};
 * ALGO_SORT_TYPED(arr, 5, int, algo_cmp_int, NULL);
 * // arr == {1, 2, 5, 8, 9}
 * ```
 */
#define ALGO_SORT_TYPED(base, len, Type, cmp, ctx)                          \
    do {                                                                     \
        usize _len = (usize)(len);                                           \
        if ((base) && _len >= 2 && (cmp)) {                                  \
            if (_len <= (usize)ALGO_SORT_STACK_TEMP_MAX) {                   \
                Type _tmp[ALGO_SORT_STACK_TEMP_MAX];                         \
                algo_sort((base), _len, sizeof(Type),                        \
                          (algo_cmp_fn)(cmp), (ctx), _tmp);                  \
            } else {                                                         \
                /* fallback — O(n²) for large arrays */                      \
                algo_sort((base), _len, sizeof(Type),                        \
                          (algo_cmp_fn)(cmp), (ctx), NULL);                  \
            }                                                                \
        }                                                                    \
    } while (0)

/**
 * @def ALGO_IS_SORTED_TYPED(base, len, Type, cmp, ctx)
 * @brief Type-safe sorted check — evaluates to bool
 *
 * C99-portable — no GCC statement expression.
 * Delegates to algo_is_sorted() with sizeof(Type).
 *
 * @param base Array to check (borrowed, read-only)
 * @param len  Number of elements
 * @param Type Element type
 * @param cmp  algo_cmp_fn comparator (borrowed)
 * @param ctx  Optional context (borrowed, may be NULL)
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 *
 * Example:
 * ```c
 * int arr[] = {1, 2, 3};
 * bool ok = ALGO_IS_SORTED_TYPED(arr, 3, int, algo_cmp_int, NULL); // true
 * ```
 */
#define ALGO_IS_SORTED_TYPED(base, len, Type, cmp, ctx) \
    algo_is_sorted((base), (usize)(len), sizeof(Type), \
                   (algo_cmp_fn)(cmp), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_SORT — typed slice variant per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type).
   Caller provides temp_buffer + temp_cap separately — no hidden allocation.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates sort/is_sorted functions that accept slice_##type directly
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 * - algo_sort_slice_##type(sv, cmp, ctx, temp, temp_cap)  → void
 * - algo_is_sorted_slice_##type(sv, cmp, ctx)             → bool
 *
 * temp and temp_cap describe a caller-provided scratch buffer. If
 * temp == NULL or temp_cap < sv.len, falls back to insertion sort.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Ownership:
 * - sv: borrowed (slice is non-owning view, but underlying array is modified)
 * - cmp: borrowed (function pointer used during call only)
 * - ctx: borrowed (passed through to cmp)
 * - temp: borrowed (scratch space used during call only)
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_SORT(int)
 *
 * int arr[] = {5, 3, 1, 4, 2};
 * int tmp[5];
 * slice_int sv = slice_int_from(arr, 5);
 *
 * algo_sort_slice_int(sv, algo_cmp_int, NULL, tmp, 5);
 * // arr == {1, 2, 3, 4, 5}
 *
 * bool ok = algo_is_sorted_slice_int(sv, algo_cmp_int, NULL); // true
 * ```
 */
#define DEFINE_ALGO_SORT(type) \
\
/** \
 * @brief Sorts the elements of a slice_##type in-place \
 * \
 * @param sv        Slice (borrowed non-owning view — sorts the underlying array) \
 * @param cmp       Three-way comparator (borrowed) \
 * @param ctx       Optional context (borrowed, may be NULL) \
 * @param temp      Caller-provided scratch buffer of type[temp_cap] (borrowed) \
 * @param temp_cap  Number of elements temp can hold \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * @pre sv.ptr points to valid array if non-NULL \
 * @pre temp points to valid buffer of temp_cap elements if non-NULL \
 * @pre sv.ptr and temp do not overlap \
 * \
 * Ownership: \
 * - sv: borrowed (underlying array modified in place) \
 * - cmp: borrowed (used during call only) \
 * - ctx: borrowed (passed to cmp) \
 * - temp: borrowed (scratch space, not stored) \
 * \
 * Performance: O(n log n) if temp_cap >= sv.len, O(n²) otherwise \
 * \
 * Thread-safety: Safe to call on different slices concurrently \
 */ \
static inline void algo_sort_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx, \
    borrowed type*        temp, \
    usize                 temp_cap) \
{ \
    if (!sv.ptr || sv.len < 2 || !cmp) return; \
    void* tmp = (temp && temp_cap >= sv.len) ? (void*)temp : NULL; \
    algo_sort(sv.ptr, sv.len, sizeof(type), cmp, ctx, tmp); \
} \
\
/** \
 * @brief Returns true if slice_##type is sorted according to cmp \
 * \
 * @param sv  Slice to check (borrowed, read-only) \
 * @param cmp Comparator (borrowed) \
 * @param ctx Optional context (borrowed, may be NULL) \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * \
 * Ownership: \
 * - sv: borrowed (read-only) \
 * - cmp: borrowed (used during call only) \
 * - ctx: borrowed (passed to cmp) \
 * \
 * Performance: O(n) time, O(1) space \
 * \
 * Thread-safety: Safe to call concurrently if array not being modified \
 */ \
static inline bool algo_is_sorted_slice_##type( \
    borrowed slice_##type sv, \
    borrowed algo_cmp_fn  cmp, \
    borrowed void*        ctx) \
{ \
    return algo_is_sorted(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

/* ════════════════════════════════════════════════════════════════════════════
   Built-in comparators
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ascending int comparator
 *
 * Performance: O(1)
 * Thread-safety: Safe (no shared state)
 */
static inline int algo_cmp_int(const void* a, const void* b, void* ctx) {
    (void)ctx;
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Descending int comparator
 *
 * Performance: O(1)
 * Thread-safety: Safe (no shared state)
 */
static inline int algo_cmp_int_desc(const void* a, const void* b, void* ctx) {
    return algo_cmp_int(b, a, ctx);
}

/**
 * @brief Ascending usize comparator
 *
 * Performance: O(1)
 * Thread-safety: Safe (no shared state)
 */
static inline int algo_cmp_usize(const void* a, const void* b, void* ctx) {
    (void)ctx;
    usize x = *(const usize*)a, y = *(const usize*)b;
    return (x > y) - (x < y);
}

/**
 * @brief Ascending double comparator — NaN sorted last
 *
 * Performance: O(1)
 * Thread-safety: Safe (no shared state)
 *
 * @note NaN handling: NaN == NaN returns 0, NaN > any_number returns 1
 */
static inline int algo_cmp_double(const void* a, const void* b, void* ctx) {
    (void)ctx;
    double x = *(const double*)a, y = *(const double*)b;
    if (x != x) return (y != y) ? 0 : 1;  /* x is NaN */
    if (y != y) return -1;                 /* y is NaN, x is not */
    return (x > y) - (x < y);
}

#endif /* CANON_ALGO_SORT_H */
