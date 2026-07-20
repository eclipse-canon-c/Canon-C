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
 * - NULL-safe — silently does nothing on NULL inputs
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
 * vec_int_to_stringbuf(&v, &sb, "%d ");
 * // sb now contains "1 2 3 4 "
 *
 * // Callback-style (for custom types or complex formatting):
 * void fmt_int(StringBuf* sb, int val) {
 *     stringbuf_printf(sb, "[%d]", val);
 * }
 * vec_int_to_stringbuf_cb(&v, &sb, fmt_int);
 * // sb now contains "[1][2][3][4]"
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
 * void fn(borrowed(const VecType*) v, borrowed(StringBuf*) sb, borrowed(const char*) fmt);
 * ```
 *
 * @pre fmt is a valid printf format string for type (e.g. "%d" for int)
 *
 * @post Each element formatted via stringbuf_printf(sb, fmt, element)
 * @post Does nothing if v == NULL, sb == NULL, or fmt == NULL
 * @post Does nothing if v->len == 0
 *
 * @note Caller is responsible for ensuring fmt is appropriate for type.
 *       Mismatched format strings produce undefined behavior (same as printf).
 * @note StringBuf silently truncates if capacity is exceeded.
 *       Check stringbuf_remaining() before calling if overflow matters.
 *
 * Performance:
 * - Time:  O(n) where n = v->len
 * - Space: O(1) — writes into caller-owned StringBuf, no allocation
 */
#define IMPL_VEC_TO_STRINGBUF(linkage, VecType, fn, type)                          \
linkage void fn(                                                                    \
        borrowed(const VecType*) v,                                                 \
        borrowed(StringBuf*)     sb,                                                \
        borrowed(const char*)    fmt)                                               \
{                                                                                   \
    usize i;                                                                        \
    if (!v || !sb || !fmt) { return; }                                              \
    for (i = 0; i < v->len; i++) {                                                  \
        stringbuf_printf(sb, fmt, v->items[i]);                                     \
    }                                                                               \
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
 * void fn(borrowed(const VecType*) v, borrowed(StringBuf*) sb, void (*cb)(borrowed(StringBuf*), type));
 * ```
 *
 * @pre cb is a valid function pointer — not NULL
 *
 * @post Each element passed to cb(sb, element) in order
 * @post Does nothing if v == NULL, sb == NULL, or cb == NULL
 * @post Does nothing if v->len == 0
 *
 * @note The callback receives the element by value.
 *       For large structs, consider a pointer-based approach.
 * @note The callback is responsible for writing to sb.
 *       StringBuf silently truncates if capacity is exceeded.
 *
 * Performance:
 * - Time:  O(n * cb_cost) where n = v->len
 * - Space: O(1) — no allocation, writes via caller-provided callback
 */
#define IMPL_VEC_TO_STRINGBUF_CB(linkage, VecType, fn, type)                       \
linkage void fn(                                                                    \
        borrowed(const VecType*) v,                                                 \
        borrowed(StringBuf*)     sb,                                                \
        void (*cb)(borrowed(StringBuf*), type))                                     \
{                                                                                   \
    usize i;                                                                        \
    if (!v || !sb || !cb) { return; }                                               \
    for (i = 0; i < v->len; i++) {                                                  \
        cb(sb, v->items[i]);                                                        \
    }                                                                               \
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
 * - vec_##type##_to_stringbuf(v, sb, fmt)   → void
 * - vec_##type##_to_stringbuf_cb(v, sb, cb) → void
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
 * // void vec_int_to_stringbuf(borrowed(const vec_int*) v,
 * //                                 borrowed(StringBuf*) sb,
 * //                                 borrowed(const char*) fmt);
 * // void vec_int_to_stringbuf_cb(borrowed(const vec_int*) v,
 * //                                    borrowed(StringBuf*) sb,
 * //                                    void (*cb)(borrowed(StringBuf*), int));
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
