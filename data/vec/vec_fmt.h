/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_DATA_VEC_FMT_H
#define CANON_DATA_VEC_FMT_H

#include "data/vec/vec_impl.h"
#include "data/vec/vec_mangle.h"
#include "data/stringbuf.h"

/**
 * @file vec_fmt.h
 * @brief Optional StringBuf formatting extension for Canon-C typed vectors
 *
 * Adds to_stringbuf and to_stringbuf_cb to any typed vector instantiated
 * via DEFINE_VEC. Depends on data/stringbuf.h for the StringBuf type.
 *
 * This file is intentionally separate from vec_defn.h to keep the core
 * vec module free of data/stringbuf.h as a dependency. Only include it
 * when you need vector-to-string formatting.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Two formatting strategies: printf-style format string, or callback
 * - No hidden allocation — writes into caller-owned StringBuf
 * - Callback variant enables custom type formatting without format strings
 * - Reports failure — both return bool; when the StringBuf fills up they stop
 *   at the first element that does not fit and keep what was written
 *   (VERIFY-042 F1: before, such an element was skipped and later ones
 *   appended after the gap, silently)
 * - NULL-safe — false on NULL inputs, nothing written
 *
 * Ownership model:
 * ────────────────────────────────────────────────────────────────────────────
 * All generated functions borrow every pointer they receive. Neither the
 * vector nor the StringBuf is consumed or owned. borrowed() annotations
 * from core/ownership.h (pulled in transitively via data/stringbuf.h) are
 * applied to every generated parameter to make this explicit.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends on data/stringbuf.h and data/vec/vec_impl.h
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * vec_fmt.h is in data/vec/. It may depend on:
 * - core/
 * - semantics/
 * - data/stringbuf.h  (same layer, different module — explicit dependency)
 * - data/vec/         (same module)
 *
 * core/ownership.h is available transitively through data/stringbuf.h.
 * No additional include is needed.
 *
 * Usage — format a vec_int into a StringBuf:
 * ```c
 * #include "data/vec/vec.h"
 * #include "data/vec/vec_fmt.h"
 *
 * DEFINE_VEC(static inline, int)
 * DEFINE_VEC_FMT(static inline, int)
 *
 * int buf[4];
 * vec_int v = vec_int_init(buf, 4);
 * int src[4] = {1, 2, 3, 4};
 * vec_int_append_array(&v, src, 4);
 *
 * char out[256];
 * StringBuf sb;
 * stringbuf_init_buffer(&sb, out, sizeof(out));
 *
 * // Printf-style:
 * bool ok = vec_int_to_stringbuf(&v, &sb, "%d ");
 * // ok, and sb now contains "1 2 3 4 "
 *
 * // Callback-style (for custom types or complex formatting):
 * bool fmt_int(StringBuf* sb, int val) {
 *     return stringbuf_printf(sb, "[%d]", val);
 * }
 * ok = vec_int_to_stringbuf_cb(&v, &sb, fmt_int);
 * // ok, and sb now contains "1 2 3 4 [1][2][3][4]"
 * ```
 *
 * @sa vec.h, vec_defn.h, data/stringbuf.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_TO_STRINGBUF — printf-style formatting
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Implementation macro for to_stringbuf (printf-style format string)
 *
 * @param linkage  Function linkage
 * @param VecType  Mangled vector type name
 * @param fn       Mangled function name
 * @param type     Element type
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const VecType*) v, borrowed(StringBuf*) sb, borrowed(const char*) fmt);
 * ```
 *
 * @pre fmt is a valid printf format string for type (e.g. "%d" for int)
 * @pre No byte of fmt, its terminator included, lies in the StringBuf's
 *      unused tail [data + len, data + capacity): each element's append
 *      writes there, and the format is read again for the next element
 *      (VERIFY-042 F2; the same holds for any %s argument). Pointing into
 *      the current string [data, data + len) is fine.
 * @pre The vector's elements do not lie in that tail either.
 *
 * @post Each element present at the call, in order, formatted via
 *       stringbuf_printf(sb, fmt, element), appended after what sb holds
 * @return true if every element was appended (or the vector is empty);
 *         false if v, sb or fmt is NULL, if sb has no buffer, or at the
 *         first element that does not fit -- the elements before it stay
 *         appended, and no later one is
 *
 * @note Caller is responsible for ensuring fmt is appropriate for type.
 *       Mismatched format strings produce undefined behavior (same as printf).
 *
 * Performance:
 * - Time:  O(n) where n = v->len
 * - Space: O(1) — writes into caller-owned StringBuf, no allocation
 */
/* VERIFY-042: the contract and the loop's annotation live in the macro body,
 * as vec_range.h's do, and reach the prover only under -CC; frama-c-vec-fmt
 * preprocesses so. They name no macro parameter. */
#define IMPL_VEC_TO_STRINGBUF(linkage, VecType, fn, type)                      \
/*@ requires v_ok:    v == \null || (\valid_read(v) && v->len <= v->capacity && (v->len > 0 ==> \valid_read(v->items + (0 .. v->len - 1)))); \
    requires sb_ok:   sb == \null || stringbuf_ok(sb);                         \
    requires fmt_ok:  fmt == \null || valid_read_string(fmt);                  \
    requires fmt_sep: !(v != \null && sb != \null && fmt != \null && sb->data != \null && v->len > 0) \
    || \separated(fmt + (0 .. strlen(fmt)), sb->data + (sb->len .. sb->capacity - 1)); \
    requires v_sep:   !(v != \null && sb != \null && fmt != \null && sb->data != \null && v->len > 0) \
    || \separated(v->items + (0 .. v->len - 1), sb->data + (sb->len .. sb->capacity - 1)); \
    assigns sb->len, sb->data[sb->len .. sb->capacity - 1];                    \
    behavior no_args:                                                          \
    assumes v == \null || sb == \null || fmt == \null;                         \
    assigns \nothing;                                                          \
    ensures \result == \false;                                                 \
    behavior empty:                                                            \
    assumes v != \null && sb != \null && fmt != \null && v->len == 0;          \
    assigns \nothing;                                                          \
    ensures \result == \true;                                                  \
    behavior no_buffer:                                                        \
    assumes v != \null && sb != \null && fmt != \null && v->len > 0 && sb->data == \null; \
    assigns \nothing;                                                          \
    ensures \result == \false;                                                 \
    behavior format:                                                           \
    assumes v != \null && sb != \null && fmt != \null && v->len > 0 && sb->data != \null; \
    assigns sb->len, sb->data[sb->len .. sb->capacity - 1];                    \
    ensures ok:     stringbuf_wf(sb);                                          \
    ensures grown:  \old(sb->len) <= sb->len;                                  \
    ensures prefix: stringbuf_prefix{Pre, Post}(sb, \old(sb->len));            \
    complete behaviors;                                                        \
    disjoint behaviors; */                                                     \
linkage bool fn(                                                               \
        borrowed(const VecType*) v,                                            \
        borrowed(StringBuf*)     sb,                                           \
        borrowed(const char*)    fmt)                                          \
{                                                                              \
    if (!v || !sb || !fmt) { return false; }                                   \
    const usize n = v->len;                                                    \
    const type* const items = v->items;                                        \
    if (n == 0) { return true; }                                               \
    if (!sb->data) { return false; }                                           \
    /*@ loop invariant 0 <= i <= n;                                            \
        loop invariant stringbuf_wf(sb);                                       \
        loop invariant sb->data == \at(sb->data, Pre) && sb->capacity == \at(sb->capacity, Pre); \
        loop invariant \at(sb->len, Pre) <= sb->len;                           \
        loop invariant stringbuf_prefix{Pre, Here}(sb, \at(sb->len, Pre));     \
        loop invariant valid_read_string(fmt);                                 \
        loop invariant \separated(fmt + (0 .. strlen(fmt)), sb->data + (sb->len .. sb->capacity - 1)); \
        loop assigns i, sb->len, sb->data[\at(sb->len, Pre) .. sb->capacity - 1]; \
        loop variant n - i; */                                                 \
    for (usize i = 0; i < n; i++) {                                            \
        if (!stringbuf_printf(sb, fmt, items[i])) { return false; }            \
    }                                                                          \
    return true;                                                               \
}

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_TO_STRINGBUF_CB — callback-style formatting
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Implementation macro for to_stringbuf_cb (callback-based formatting)
 *
 * @param linkage  Function linkage
 * @param VecType  Mangled vector type name
 * @param fn       Mangled function name
 * @param type     Element type
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const VecType*) v, borrowed(StringBuf*) sb, bool (*cb)(borrowed(StringBuf*), type));
 * ```
 *
 * @pre cb, if not NULL, is a valid function pointer
 *
 * @post Each element present at the call passed to cb(sb, element), in
 *       order. The length and buffer are read once, at entry, so a callback
 *       that adds to the vector does not extend the loop (VERIFY-042 F3).
 * @return true if every call returned true (or the vector is empty); false
 *         if v, sb or cb is NULL, or at the first call that returns false,
 *         after which no element is passed
 *
 * @note The callback receives the element by value.
 *       For large structs, consider a pointer-based approach.
 * @note The callback writes to sb and reports, by its result, whether that
 *       succeeded; returning stringbuf_printf's result is the usual form.
 *
 * Performance:
 * - Time:  O(n * cb_cost) where n = v->len
 * - Space: O(1) — no allocation, writes via caller-provided callback
 */
#define IMPL_VEC_TO_STRINGBUF_CB(linkage, VecType, fn, type)                   \
/*@ requires v_ok:  v == \null || (\valid_read(v) && v->len <= v->capacity && (v->len > 0 ==> \valid_read(v->items + (0 .. v->len - 1)))); \
    requires cb_ok: (v != \null && sb != \null && cb != \null && v->len > 0) ==> \valid_function(cb); \
    terminates !(v != \null && sb != \null && cb != \null && v->len > 0);      \
    exits v != \null && sb != \null && cb != \null && \old(v->len) > 0;        \
    behavior no_args:                                                          \
    assumes v == \null || sb == \null || cb == \null;                          \
    assigns \nothing;                                                          \
    ensures \result == \false;                                                 \
    behavior empty:                                                            \
    assumes v != \null && sb != \null && cb != \null && v->len == 0;           \
    assigns \nothing;                                                          \
    ensures \result == \true;                                                  \
    behavior call:                                                             \
    assumes v != \null && sb != \null && cb != \null && v->len > 0;            \
    complete behaviors;                                                        \
    disjoint behaviors; */                                                     \
linkage bool fn(                                                               \
        borrowed(const VecType*) v,                                            \
        borrowed(StringBuf*)     sb,                                           \
        bool (*cb)(borrowed(StringBuf*), type))                                \
{                                                                              \
    if (!v || !sb || !cb) { return false; }                                    \
    const usize n = v->len;                                                    \
    const type* const items = v->items;                                        \
    if (n == 0) { return true; }                                               \
    /*@ loop invariant 0 <= i <= n;                                            \
        loop variant n - i; */                                                 \
    for (usize i = 0; i < n; i++) {                                            \
        if (!cb(sb, items[i])) { return false; }                               \
    }                                                                          \
    return true;                                                               \
}

/* ════════════════════════════════════════════════════════════════════════════
   Mangle macros for fmt functions
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the printf-style format function
 *
 * Default: vec_##type##_to_stringbuf
 * Override before including this file if custom naming is needed.
 */
#ifndef MANGLE_VEC_TO_STRINGBUF
    #define MANGLE_VEC_TO_STRINGBUF(type)    vec_##type##_to_stringbuf
#endif

/**
 * @brief Name of the callback-style format function
 *
 * Default: vec_##type##_to_stringbuf_cb
 * Override before including this file if custom naming is needed.
 */
#ifndef MANGLE_VEC_TO_STRINGBUF_CB
    #define MANGLE_VEC_TO_STRINGBUF_CB(type) vec_##type##_to_stringbuf_cb
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_VEC_FMT — emit both formatting functions for a typed vector
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Adds to_stringbuf and to_stringbuf_cb to an existing typed vector
 *
 * Must be called AFTER DEFINE_VEC(linkage, type) for the same type.
 *
 * Generated functions (using default mangle):
 * - vec_##type##_to_stringbuf(v, sb, fmt)   → bool
 * - vec_##type##_to_stringbuf_cb(v, sb, cb) → bool
 *
 * @param linkage Function linkage (should match the DEFINE_VEC call)
 * @param type    Element type
 *
 * Example:
 * ```c
 * DEFINE_VEC(static inline, int)
 * DEFINE_VEC_FMT(static inline, int)
 *
 * // Now available:
 * // bool vec_int_to_stringbuf(borrowed(const vec_int*) v,
 * //                           borrowed(StringBuf*) sb,
 * //                           borrowed(const char*) fmt);
 * // bool vec_int_to_stringbuf_cb(borrowed(const vec_int*) v,
 * //                              borrowed(StringBuf*) sb,
 * //                              bool (*cb)(borrowed(StringBuf*), int));
 * ```
 */
#define DEFINE_VEC_FMT(linkage, type)                                               \
    IMPL_VEC_TO_STRINGBUF(   linkage,                                               \
        MANGLE_VEC_TYPE(type),                                                      \
        MANGLE_VEC_TO_STRINGBUF(type),                                              \
        type)                                                                       \
    IMPL_VEC_TO_STRINGBUF_CB(linkage,                                               \
        MANGLE_VEC_TYPE(type),                                                      \
        MANGLE_VEC_TO_STRINGBUF_CB(type),                                           \
        type)

#endif /* CANON_DATA_VEC_FMT_H */
