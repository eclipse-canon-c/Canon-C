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
 * DOUBLE INSTANTIATION NOTE
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * DEFINE_ALGO_MAP(in_type, out_type) generates an in-place variant
 * algo_map_inplace_slice_##in_type that depends only on in_type. Calling
 * DEFINE_ALGO_MAP(int, double) and DEFINE_ALGO_MAP(int, float) will
 * generate algo_map_inplace_slice_int twice, causing a redefinition error.
 *
 * To avoid this, use DEFINE_ALGO_MAP for the first call, then
 * DEFINE_ALGO_MAP_CROSS for subsequent calls with the same in_type.
 * Or use DEFINE_ALGO_MAP_INPLACE_ONLY and DEFINE_ALGO_MAP_CROSS
 * separately for full control.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * OWNERSHIP ANNOTATIONS
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Function pointer parameters in DEFINE_ALGO_MAP macros cannot use
 * borrowed() because the function pointer types contain commas that
 * break C preprocessor macro argument parsing. These fn parameters are
 * semantically borrowed — they are not retained beyond the call.
 * All other parameters use borrowed() as expected.
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
 * Typed instantiation:
 *   DEFINE_ALGO_MAP(in_type, out_type)     — cross-type + in-place
 *   DEFINE_ALGO_MAP_CROSS(in_type, out_type) — cross-type only
 *   DEFINE_ALGO_MAP_INPLACE_ONLY(in_type)    — in-place only
 *
 * Generated functions:
 *   algo_map_slice_##in_type##_##out_type(sv_out, sv_in, fn) → void
 *   algo_map_inplace_slice_##in_type(sv, fn)                 → void
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
   DEFINE_ALGO_MAP_CROSS — cross-type slice variant only
   ════════════════════════════════════════════════════════════════════════════ */

#define DEFINE_ALGO_MAP_CROSS(in_type, out_type) \
\
static inline void ALGO_MAP_SLICE_FN(in_type, out_type)( \
    borrowed(slice_##out_type)  sv_out, \
    borrowed(slice_##in_type)   sv_in, \
    void (*fn)(out_type*, const in_type*)) \
{ \
    require_msg(fn != NULL, \
        "algo_map_slice_" #in_type "_" #out_type ": fn cannot be NULL"); \
    const usize _len = (sv_out.len < sv_in.len) ? sv_out.len : sv_in.len; \
    for (usize _i = 0; _i < _len; _i++) { \
        fn(&sv_out.ptr[_i], &sv_in.ptr[_i]); \
    } \
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_MAP_INPLACE_ONLY — in-place slice variant only
   ════════════════════════════════════════════════════════════════════════════ */

#define DEFINE_ALGO_MAP_INPLACE_ONLY(in_type) \
\
static inline void ALGO_MAP_INPLACE_SLICE_FN(in_type)( \
    borrowed(slice_##in_type)  sv, \
    void (*fn)(in_type*)) \
{ \
    require_msg(fn != NULL, \
        "algo_map_inplace_slice_" #in_type ": fn cannot be NULL"); \
    for (usize _i = 0; _i < sv.len; _i++) { \
        fn(&sv.ptr[_i]); \
    } \
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ALGO_MAP — both cross-type and in-place (convenience)
   ════════════════════════════════════════════════════════════════════════════ */

#define DEFINE_ALGO_MAP(in_type, out_type) \
    DEFINE_ALGO_MAP_CROSS(in_type, out_type) \
    DEFINE_ALGO_MAP_INPLACE_ONLY(in_type)

#endif /* CANON_ALGO_MAP_H */
