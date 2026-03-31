/**
 * @file reverse.h
 * @brief In-place sequence reversal and palindrome checking
 *
 * This is the entry point for header-only usage. Including this file
 * generates statically-inlined implementations of algo_reverse and
 * algo_is_palindrome, plus typed macro wrappers and the DEFINE_ALGO_REVERSE
 * instantiation macro.
 *
 * For separate compilation (external linkage), use reverse_decl.h in
 * headers and reverse_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Level 1 — Generic (void* interface, works on any type):
 * ```c
 * int arr[] = {1, 2, 3, 4, 5};
 * algo_reverse(arr, 5, sizeof(int));
 * // arr = {5, 4, 3, 2, 1}
 *
 * static int cmp_int(const void* a, const void* b, void* ctx) {
 *     (void)ctx;
 *     return *(const int*)a - *(const int*)b;
 * }
 * int pal[] = {1, 2, 3, 2, 1};
 * bool is_pal = algo_is_palindrome(pal, 5, sizeof(int), cmp_int, NULL); // true
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof, same fn signature):
 * ```c
 * ALGO_REVERSE_TYPED(arr, 5, int);
 * bool is_pal = ALGO_IS_PALINDROME_TYPED(pal, 5, int, cmp_int, NULL);
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, fully optimizable):
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_REVERSE(int)
 *
 * int buf[] = {5, 4, 3, 2, 1};
 * slice_int sv = slice_int_from(buf, 5);
 *
 * algo_reverse_slice_int(sv);
 * // buf = {1, 2, 3, 4, 5}
 *
 * int pal_buf[] = {1, 2, 3, 2, 1};
 * slice_int sv_pal = slice_int_from(pal_buf, 5);
 * bool is_pal = algo_is_palindrome_slice_int(sv_pal, cmp_int, NULL); // true
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * EMPTY AND SINGLE-ELEMENT BEHAVIOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_reverse() — len == 0 or len == 1: returns immediately, no-op.
 *                  No contracts fire beyond the standard preconditions.
 *
 * algo_is_palindrome() — len == 0 or len == 1: returns true.
 *                        Vacuous palindrome — no pair exists that could fail.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * SWAP BUFFER SIZE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_reverse uses a fixed stack buffer of ALGO_REVERSE_SWAP_BUF_SIZE bytes
 * (default 256) for swapping. Override before including this header:
 * ```c
 * #define ALGO_REVERSE_SWAP_BUF_SIZE ((usize)1024)
 * #include "algo/reverse/reverse.h"
 * ```
 * Elements larger than the buffer trigger a contract failure before any swap.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_reverse(array, len, elem_size)                      → void
 *   algo_is_palindrome(array, len, elem_size, cmp, ctx)      → bool
 *
 * Typed macros:
 *   ALGO_REVERSE_TYPED(array, len, Type)                     → void
 *   ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp, ctx)     → bool
 *
 * Typed instantiation (call DEFINE_ALGO_REVERSE(type) first):
 *   algo_reverse_slice_##type(sv)                            → void
 *   algo_is_palindrome_slice_##type(sv, cmp, ctx)            → bool
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PERFORMANCE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_reverse / ALGO_REVERSE_TYPED / algo_reverse_slice_##type:
 *   Best case:  O(1) — len == 0 or len == 1 (immediate return, no swaps)
 *   Worst case: O(n) — exactly ⌊n/2⌋ swaps, n = len
 *   Average:    O(n) — always visits every pair; no early exit
 *   Space:      O(1) — ALGO_REVERSE_SWAP_BUF_SIZE bytes on the stack
 *   Swap cost:  3 × elem_size bytes copied per swap (temp ← left, left ← right,
 *               right ← temp); both mem_copy calls are to/from the same fixed
 *               stack buffer, so cache behaviour is excellent for small elements.
 *
 * algo_is_palindrome / ALGO_IS_PALINDROME_TYPED / algo_is_palindrome_slice_##type:
 *   Best case:  O(1) — len < 2 (immediate true), or first pair mismatches
 *   Worst case: O(n/2) — all ⌊n/2⌋ pairs compared (true result or last pair fails)
 *   Average:    O(k) where k = index of first mismatching pair from either end
 *   Space:      O(1) — no allocation; two pointer indices on the stack
 *   cmp calls:  0 (len < 2) to ⌊n/2⌋ (palindrome or mismatch at center)
 *               cmp is never called with NULL pointers.
 *
 * Level comparison:
 *   Level 1 — Generic: one stride multiply per pointer per iteration.
 *   Level 2 — Typed macro: sizeof is compile-time, eliminating the multiply.
 *   Level 3 — Typed slice: element access through typed pointer arithmetic;
 *             for algo_reverse_slice_##type the impl still delegates to the
 *             generic algo_reverse internally (same cost as level 2 when
 *             sizeof(type) is a compile-time constant visible to the optimizer).
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * COMPARATOR SIGNATURE (algo_is_palindrome)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * All three levels use algo_cmp_fn from core/primitives/compare.h:
 *   int cmp(const void* a, const void* b, void* ctx)
 *
 * Return 0 if elements are equal, any nonzero value otherwise.
 * a and b are never NULL — they always point to valid array elements.
 * ctx is the optional caller context passed through unchanged; may be NULL.
 *
 * @sa reverse_mangle.h — name and swap buffer customization
 * @sa reverse_decl.h   — forward declarations for separate compilation
 * @sa reverse_defn.h   — definitions for separate compilation
 * @sa reverse_impl.h   — pure logic (do not include directly)
 * @sa core/slice.h     — slice_##type used by DEFINE_ALGO_REVERSE
 * @sa core/primitives/compare.h — algo_cmp_fn definition
 */

#ifndef CANON_ALGO_REVERSE_H
#define CANON_ALGO_REVERSE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — all functions are inlined at call sites.
 */
#define ALGO_REVERSE_LINKAGE static inline

#include "reverse_impl.h"   /* implementation logic — NOT reverse_defn.h */

#undef ALGO_REVERSE_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_REVERSE_TYPED(array, len, Type)
 * @brief Type-safe in-place reversal of a C array
 */
#define ALGO_REVERSE_TYPED(array, len, Type) \
    algo_reverse((array), (len), sizeof(Type))

/**
 * @def ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp, ctx)
 * @brief Type-safe palindrome check over a C array
 */
#define ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp, ctx) \
    algo_is_palindrome((array), (len), sizeof(Type), \
        (algo_cmp_fn)(cmp), (ctx))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_REVERSE — typed slice variants per element type
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(type) to have been called first.
   Generates fully typed functions accepting slice_##type directly.
   No void* anywhere — the compiler sees the actual element type.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates reverse and palindrome functions operating on slice_##type
 *
 * Prerequisites: DEFINE_SLICE(type) must have been called.
 *
 * Generated functions:
 *   algo_reverse_slice_##type(sv) → void
 *     Reverses the elements of sv in place. No-op when sv.len < 2.
 *
 *   algo_is_palindrome_slice_##type(sv, cmp, ctx) → bool
 *     Returns true if sv reads the same forwards and backwards.
 *     Returns true when sv.len < 2 (vacuous palindrome).
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h invariants). Both functions guard against len < 2 after
 * contract checks, so a NULL ptr with len == 0 triggers the contract
 * for non-empty slices only.
 *
 * @param type Element type — must match a prior DEFINE_SLICE(type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_ALGO_REVERSE(int)
 *
 * static int cmp_int(const void* a, const void* b, void* ctx) {
 *     (void)ctx;
 *     return *(const int*)a - *(const int*)b;
 * }
 *
 * int buf[] = {1, 2, 3, 4, 5};
 * slice_int sv = slice_int_from(buf, 5);
 *
 * algo_reverse_slice_int(sv);
 * // buf = {5, 4, 3, 2, 1}
 *
 * int pal_buf[] = {1, 2, 3, 2, 1};
 * slice_int sv_pal = slice_int_from(pal_buf, 5);
 * bool is_pal = algo_is_palindrome_slice_int(sv_pal, cmp_int, NULL); // true
 *
 * // Empty slice — no crash, vacuous results
 * slice_int empty = slice_int_empty();
 * algo_reverse_slice_int(empty);                               // no-op
 * bool r = algo_is_palindrome_slice_int(empty, cmp_int, NULL); // true
 * ```
 */
#define DEFINE_ALGO_REVERSE(type) \
\
static inline void ALGO_REVERSE_SLICE_FN(type)( \
    borrowed(slice_##type) sv) \
{ \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_reverse_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len < 2) return; \
    algo_reverse(sv.ptr, sv.len, sizeof(type)); \
} \
\
static inline bool ALGO_IS_PALINDROME_SLICE_FN(type)( \
    borrowed(slice_##type)  sv, \
    borrowed(algo_cmp_fn)   cmp, \
    borrowed(void*)         ctx) \
{ \
    require_msg(cmp != NULL, \
        "algo_is_palindrome_slice_" #type ": cmp cannot be NULL"); \
    require_msg(sv.len == 0 || sv.ptr != NULL, \
        "algo_is_palindrome_slice_" #type ": non-empty slice has NULL ptr"); \
    if (sv.len < 2) return true; \
    return algo_is_palindrome(sv.ptr, sv.len, sizeof(type), cmp, ctx); \
}

#endif /* CANON_ALGO_REVERSE_H */
