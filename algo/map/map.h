/**
 * @file map.h
 * @brief Element-wise transformation over sequences
 *
 * This is the entry point for header-only usage. Including this file
 * generates statically-inlined implementations of algo_map and
 * algo_map_inplace, plus typed macro wrappers and the DEFINE_ALGO_MAP
 * instantiation macro.
 *
 * For separate compilation (external linkage), use map_decl.h in
 * headers and map_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Level 1 — Generic (void* interface, works on any type):
 * ```c
 * static void int_to_double(void* out, const void* in) {
 *     *(double*)out = (double)(*(const int*)in);
 * }
 *
 * int    input[4]  = {1, 2, 3, 4};
 * double output[4] = {0};
 * algo_map(output, input, 4, sizeof(double), sizeof(int), int_to_double);
 * // output = {1.0, 2.0, 3.0, 4.0}
 *
 * static void increment(void* elem) {
 *     (*(int*)elem)++;
 * }
 * algo_map_inplace(input, 4, sizeof(int), increment);
 * // input = {2, 3, 4, 5}
 * ```
 *
 * Level 2 — Typed macro (compile-time sizeof, same fn signature):
 * ```c
 * ALGO_MAP_TYPED(output, input, 4, double, int, int_to_double);
 * ALGO_MAP_INPLACE_TYPED(input, 4, int, increment);
 * ```
 *
 * Level 3 — Typed slice instantiation (no void*, fully optimizable):
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_SLICE(double)
 * DEFINE_ALGO_MAP(int, double)
 *
 * int    buf_in[4]  = {1, 2, 3, 4};
 * double buf_out[4] = {0};
 * slice_int    sv_in  = slice_int_from(buf_in,  4);
 * slice_double sv_out = slice_double_from(buf_out, 4);
 *
 * algo_map_slice_int_double(sv_out, sv_in, int_to_double);
 * // buf_out = {1.0, 2.0, 3.0, 4.0}
 *
 * // In-place (same type only):
 * DEFINE_ALGO_MAP(int, int)
 * algo_map_inplace_slice_int(sv_in, increment);
 * // buf_in = {2, 3, 4, 5}
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * EMPTY SEQUENCE BEHAVIOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * algo_map() and algo_map_inplace() treat len == 0 as a valid no-op.
 * fn is never called. No memory is read or written. No contract fires.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Generic:
 *   algo_map(out, in, len, out_elem_size, in_elem_size, fn)  → void
 *   algo_map_inplace(base, len, elem_size, fn)               → void
 *
 * Typed macros:
 *   ALGO_MAP_TYPED(out, in, len, OutType, InType, fn)        → void
 *   ALGO_MAP_INPLACE_TYPED(arr, len, Type, fn)               → void
 *
 * Typed instantiation (call DEFINE_ALGO_MAP(in_type, out_type) first):
 *   algo_map_slice_##in_type##_##out_type(sv_out, sv_in, fn) → void
 *   algo_map_inplace_slice_##in_type(sv, fn)                 → void
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PERFORMANCE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Time complexity — all variants:
 *
 *   algo_map / ALGO_MAP_TYPED / algo_map_slice_##in##_##out:
 *     Always O(n) — every element is visited and fn is called exactly once,
 *     n = len (or min(sv_out.len, sv_in.len) for the slice variant).
 *     No early exit. Best == Worst == O(n).
 *
 *   algo_map_inplace / ALGO_MAP_INPLACE_TYPED / algo_map_inplace_slice_##type:
 *     Always O(n) — same guarantee; fn called exactly once per element.
 *     No early exit.
 *
 *   Special case: len == 0 (or either slice is empty):
 *     O(1) — fn is never called, returns immediately.
 *
 * Space complexity — all variants:
 *   O(1) — no heap allocation, no recursion, constant stack frame.
 *   algo_map writes through the caller-supplied output buffer; it does
 *   not allocate one.
 *
 * fn call count:
 *   Exactly n calls (0 when len == 0). fn is never called with a NULL
 *   pointer. Each call receives a valid output element slot and a valid
 *   input element.
 *
 * Level comparison:
 *   Level 1 — Generic: two stride multiplies per element (ptr_elem_const
 *             for input, ptr_elem for output) when in/out sizes differ.
 *   Level 2 — Typed macro: sizeof is a compile-time constant, eliminating
 *             the multiply in optimized builds.
 *   Level 3 — Typed slice: fn receives typed pointers directly; no void*
 *             casts at the call site, no stride multiply. Best codegen;
 *             preferred when both element types are known at instantiation.
 *   In-place variants (all levels): single stride multiply eliminated at
 *             level 2+; the output is the input buffer, so no separate
 *             output stride computation.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * TRANSFORMATION FUNCTION SIGNATURES
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Cross-type (algo_map, ALGO_MAP_TYPED, algo_map_slice_##in##_##out):
 *   void fn(void* out, const void* in)
 *   out points to the output element slot — write exactly one element.
 *   in  points to the input element — read-only, do not modify.
 *   Neither pointer may be retained beyond the call.
 *
 * In-place (algo_map_inplace, ALGO_MAP_INPLACE_TYPED, algo_map_inplace_slice_##type):
 *   void fn(void* elem)
 *   elem points to the element — read current value, write transformed result.
 *   The pointer must not be retained beyond the call.
 *
 * @sa map_mangle.h — name customization for slice variants
 * @sa map_decl.h   — forward declarations for separate compilation
 * @sa map_defn.h   — definitions for separate compilation
 * @sa map_impl.h   — pure logic (do not include directly)
 * @sa core/slice.h — slice_##type used by DEFINE_ALGO_MAP
 */

#ifndef CANON_ALGO_MAP_H
#define CANON_ALGO_MAP_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "core/ownership.h"

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — all functions are inlined at call sites.
 */
#define ALGO_MAP_LINKAGE static inline

#include "map_impl.h"   /* implementation logic — NOT map_defn.h */

#undef ALGO_MAP_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros — recommended for direct array use
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALGO_MAP_TYPED(out, in, len, OutType, InType, fn)
 * @brief Type-safe map from InType array to OutType array
 *
 * Wraps algo_map() with automatic sizeof(OutType) and sizeof(InType),
 * eliminating manual elem_size arguments and reducing the chance of
 * sizeof mismatches.
 *
 * fn must have signature: void fn(void* out, const void* in)
 * Both out and in point to individual elements of their respective types.
 *
 * @param out     Output array of OutType (borrowed, writable)
 * @param in      Input array of InType (borrowed, read-only)
 * @param len     Number of elements to process (0 is valid — no-op)
 * @param OutType Output element type — used for sizeof and pointer arithmetic
 * @param InType  Input element type — used for sizeof and pointer arithmetic
 * @param fn      Transformation function: void (*)(void*, const void*) (borrowed)
 *
 * Performance:
 * - Time:  O(n) — fn called exactly n times (0 calls when len == 0)
 * - Space: O(1) — writes into caller-supplied output buffer
 *
 * @pre out != NULL, in != NULL, fn != NULL — enforced by algo_map contracts
 * @pre out and in point to buffers of at least len elements each
 * @pre out and in do not overlap
 *
 * @post out[i] = fn(in[i]) for all i in [0, len)
 * @post in is unchanged
 */
#define ALGO_MAP_TYPED(out, in, len, OutType, InType, fn) \
    algo_map((out), (in), (len), \
        sizeof(OutType), sizeof(InType), \
        (algo_map_fn)(fn))

/**
 * @def ALGO_MAP_INPLACE_TYPED(arr, len, Type, fn)
 * @brief Type-safe in-place map over a C array
 *
 * Wraps algo_map_inplace() with automatic sizeof(Type), eliminating
 * the manual elem_size argument.
 *
 * fn must have signature: void fn(void* elem)
 * elem points to an individual element of Type.
 *
 * @param arr  Array of Type (borrowed, modified in place)
 * @param len  Number of elements to process (0 is valid — no-op)
 * @param Type Element type — used for sizeof and pointer arithmetic
 * @param fn   In-place transformation: void (*)(void*) (borrowed)
 *
 * Performance:
 * - Time:  O(n) — fn called exactly n times (0 calls when len == 0)
 * - Space: O(1) — modifies array in place, no extra allocation
 *
 * @pre arr != NULL, fn != NULL — enforced by algo_map_inplace contracts
 * @pre arr points to a buffer of at least len elements
 *
 * @post arr[i] = fn(arr[i]) for all i in [0, len)
 * @post Original values are lost (overwritten)
 *
 * Warning: data is mutated in place. If the original must be preserved,
 * use ALGO_MAP_TYPED with a separate output buffer.
 */
#define ALGO_MAP_INPLACE_TYPED(arr, len, Type, fn) \
    algo_map_inplace((arr), (len), sizeof(Type), \
        (algo_map_inplace_fn)(fn))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_MAP — typed slice variants per element type pair
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(in_type) and DEFINE_SLICE(out_type).
   Generates fully typed functions accepting slice_##type directly.
   No void* anywhere — the compiler sees the actual element types.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates map functions operating directly on typed slices
 *
 * Prerequisites:
 * - DEFINE_SLICE(in_type) must have been called
 * - DEFINE_SLICE(out_type) must have been called (may be same as in_type)
 *
 * Generated functions:
 *   algo_map_slice_##in_type##_##out_type(sv_out, sv_in, fn) → void
 *     Transforms each element of sv_in through fn, writing to sv_out.
 *     Processes min(sv_out.len, sv_in.len) elements. Both slices are
 *     borrowed — caller retains ownership of all backing buffers.
 *
 *   algo_map_inplace_slice_##in_type(sv, fn) → void
 *     Applies fn to each element of sv in place. Useful when in_type and
 *     out_type are the same. Generated regardless of out_type — it depends
 *     only on in_type. Calling it when in_type != out_type is a type error.
 *
 * The transformation functions receive typed pointers, not void*. The
 * compiler sees the exact element types at every call site.
 *
 * Cross-type fn signature:   void fn(out_type* out, const in_type* in)
 * In-place fn signature:     void fn(in_type* elem)
 *
 * Empty slice safety: sv.ptr may be NULL when sv.len == 0 (valid per
 * slice.h invariants). The loop never executes when len == 0, so a
 * NULL ptr is safe.
 *
 * Performance:
 *   algo_map_slice_##in##_##out: O(min(sv_out.len, sv_in.len)) — all elements
 *   algo_map_inplace_slice_##in: O(sv.len) — all elements
 *   Both: O(1) space, fn called exactly that many times (0 if either is empty)
 *
 * @param in_type  Input element type — must match a prior DEFINE_SLICE(in_type) call
 * @param out_type Output element type — must match a prior DEFINE_SLICE(out_type) call
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_SLICE(double)
 * DEFINE_ALGO_MAP(int, double)
 *
 * static void square(double* out, const int* in) {
 *     *out = (double)(*in) * (double)(*in);
 * }
 *
 * int    buf_in[]  = {1, 2, 3, 4, 5};
 * double buf_out[5] = {0};
 * slice_int    sv_in  = slice_int_from(buf_in,  5);
 * slice_double sv_out = slice_double_from(buf_out, 5);
 *
 * algo_map_slice_int_double(sv_out, sv_in, square);
 * // buf_out = {1.0, 4.0, 9.0, 16.0, 25.0}
 *
 * // In-place (same type only):
 * DEFINE_ALGO_MAP(int, int)
 * static void increment(int* elem) { (*elem)++; }
 * algo_map_inplace_slice_int(sv_in, increment);
 * // buf_in = {2, 3, 4, 5, 6}
 *
 * // Empty slice — no crash, no fn calls
 * slice_int empty = slice_int_empty();
 * algo_map_inplace_slice_int(empty, increment); // no-op
 * ```
 */
#define DEFINE_ALGO_MAP(in_type, out_type) \
\
/** \
 * @brief Maps each element of slice_##in_type into slice_##out_type \
 * \
 * Processes min(sv_out.len, sv_in.len) elements. \
 * fn is called exactly once per processed element. \
 * \
 * @param sv_out Output slice (borrowed, writable) \
 * @param sv_in  Input slice (borrowed, read-only) \
 * @param fn     Transformation: void (*)(out_type*, const in_type*) (borrowed) \
 * \
 * @pre fn != NULL — triggers require_msg \
 * @pre sv_out.ptr != NULL or sv_out.len == 0 (slice.h invariant) \
 * @pre sv_in.ptr  != NULL or sv_in.len  == 0 (slice.h invariant) \
 */ \
static inline void ALGO_MAP_SLICE_FN(in_type, out_type)( \
    borrowed(slice_##out_type)                         sv_out, \
    borrowed(slice_##in_type)                          sv_in, \
    borrowed(void (*)(out_type*, const in_type*))      fn) \
{ \
    require_msg(fn != NULL, \
        "algo_map_slice_" #in_type "_" #out_type ": fn cannot be NULL"); \
    const usize _len = (sv_out.len < sv_in.len) ? sv_out.len : sv_in.len; \
    for (usize _i = 0; _i < _len; _i++) { \
        fn(&sv_out.ptr[_i], &sv_in.ptr[_i]); \
    } \
} \
\
/** \
 * @brief Applies fn to each element of slice_##in_type in place \
 * \
 * Intended for same-type transformations (in_type == out_type). \
 * The generated function depends only on in_type. \
 * \
 * @param sv Slice (borrowed, modified in place) \
 * @param fn In-place transformation: void (*)(in_type*) (borrowed) \
 * \
 * @pre fn != NULL — triggers require_msg \
 * @pre sv.ptr != NULL or sv.len == 0 (slice.h invariant) \
 * \
 * Warning: original values are overwritten. \
 */ \
static inline void ALGO_MAP_INPLACE_SLICE_FN(in_type)( \
    borrowed(slice_##in_type)       sv, \
    borrowed(void (*)(in_type*))    fn) \
{ \
    require_msg(fn != NULL, \
        "algo_map_inplace_slice_" #in_type ": fn cannot be NULL"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        fn(&sv.ptr[_i]); \
    } \
}

#endif /* CANON_ALGO_MAP_H */
