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
 * DOUBLE INSTANTIATION GUARD
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * DEFINE_ALGO_MAP(in_type, out_type) generates an in-place variant
 * algo_map_inplace_slice_##in_type that depends only on in_type. Calling
 * DEFINE_ALGO_MAP(int, double) and DEFINE_ALGO_MAP(int, float) would
 * generate algo_map_inplace_slice_int twice, causing a redefinition error.
 * To prevent this, define CANON_ALGO_MAP_INPLACE_DEFINED_##in_type before
 * the second call, or use the guard documented in the DEFINE_ALGO_MAP
 * section below.
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
 * All variants: O(n) time, O(1) space, fn called exactly n times.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * TRANSFORMATION FUNCTION SIGNATURES
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Cross-type:  void fn(void* out, const void* in)
 * In-place:    void fn(void* elem)
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
 */
#define ALGO_MAP_LINKAGE static inline

#include "map_impl.h"   /* implementation logic — NOT map_defn.h */

#undef ALGO_MAP_LINKAGE

/* ════════════════════════════════════════════════════════════════════════════
   Typed macros
   ════════════════════════════════════════════════════════════════════════════ */

#define ALGO_MAP_TYPED(out, in, len, OutType, InType, fn) \
    algo_map((out), (in), (len), \
        sizeof(OutType), sizeof(InType), \
        (algo_map_fn)(fn))

#define ALGO_MAP_INPLACE_TYPED(arr, len, Type, fn) \
    algo_map_inplace((arr), (len), sizeof(Type), \
        (algo_map_inplace_fn)(fn))

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_MAP — typed slice variants per element type pair
   ════════════════════════════════════════════════════════════════════════════
   Requires DEFINE_SLICE(in_type) and DEFINE_SLICE(out_type).

   The in-place variant is guarded by CANON_ALGO_MAP_INPLACE_DEFINED_##in_type
   to prevent redefinition when DEFINE_ALGO_MAP is called multiple times with
   the same in_type but different out_types. The guard is automatically set
   on first instantiation.
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
 *   algo_map_inplace_slice_##in_type(sv, fn) → void
 *     (only generated on first call for a given in_type)
 *
 * @param in_type  Input element type
 * @param out_type Output element type
 */
#define DEFINE_ALGO_MAP(in_type, out_type) \
\
static inline void ALGO_MAP_SLICE_FN(in_type, out_type)( \
    borrowed(slice_##out_type)                          sv_out, \
    borrowed(slice_##in_type)                           sv_in, \
    borrowed(void (*)(out_type*, const in_type*))        fn) \
{ \
    require_msg(fn != NULL, \
        "algo_map_slice_" #in_type "_" #out_type ": fn cannot be NULL"); \
    const usize _len = (sv_out.len < sv_in.len) ? sv_out.len : sv_in.len; \
    for (usize _i = 0; _i < _len; _i++) { \
        fn(&sv_out.ptr[_i], &sv_in.ptr[_i]); \
    } \
} \
\
CANON_ALGO_MAP_INPLACE_IMPL_(in_type)

/* Internal: conditionally generate the in-place variant once per in_type */
#ifndef CANON_ALGO_MAP_INPLACE_GUARD_
#define CANON_ALGO_MAP_INPLACE_GUARD_

#define CANON_ALGO_MAP_INPLACE_IMPL_(in_type) \
    CANON_ALGO_MAP_INPLACE_IMPL_INNER_(in_type, \
        CANON_ALGO_MAP_INPLACE_DEFINED_##in_type)

/* Two-level dispatch: if the guard macro is defined, emit nothing */
#define CANON_ALGO_MAP_INPLACE_IMPL_INNER_(in_type, guard) \
    CANON_ALGO_MAP_INPLACE_SELECT_(in_type, guard)

/* Default: guard is not defined, so generate the function and set the guard */
#define CANON_ALGO_MAP_INPLACE_SELECT_(in_type, guard) \
static inline void ALGO_MAP_INPLACE_SLICE_FN(in_type)( \
    borrowed(slice_##in_type)                   sv, \
    borrowed(void (*)(in_type*))                 fn) \
{ \
    require_msg(fn != NULL, \
        "algo_map_inplace_slice_" #in_type ": fn cannot be NULL"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        fn(&sv.ptr[_i]); \
    } \
}

#endif /* CANON_ALGO_MAP_INPLACE_GUARD_ */

#endif /* CANON_ALGO_MAP_H */
