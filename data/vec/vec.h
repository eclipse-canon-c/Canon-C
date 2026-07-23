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

#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include "core/primitives/ptr.h"
#include "core/slice.h"
#include "data/vec/vec_defn.h"

/**
 * @file vec.h
 * @brief Bounded typed vectors with explicit caller-owned buffers
 *
 * This is the primary user-facing header for the Canon-C vec module.
 * Including this file gives you the full header-only API using default
 * mangled names (vec_##type).
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed capacity — no automatic growth
 * - Caller owns the buffer (stack, heap, arena, static)
 * - All bounds-checked operations return Result<bool, Error> or Option<T>
 * - Unchecked variants available for hot paths (debug-mode guarded)
 * - Three allocation strategies: buffer-init, heap, arena
 * - slice_##type and bytes_t views via DEFINE_VEC_SLICE (zero-copy)
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - You always know where memory comes from
 * - You always know when an operation can fail
 * - No hidden allocations, no surprise resizes
 * - For auto-growing vectors, use data/convenience/dynvec.h explicitly
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - push/pop:              O(1), no allocation
 * - insert/remove:         O(n), shifts elements
 * - append_array/extend:   O(k), copies k elements
 * - get/set/at:            O(1)
 * - iter_next:             O(1) per step
 * - as_slice / as_bytes:   O(1), zero-copy view
 * - alloc:                 O(1), single mem_alloc
 * - arena_alloc:           O(1), arena bump
 * - struct size:           sizeof(type*) + 2*sizeof(usize)
 * - element overhead:      0 bytes beyond sizeof(type) per element
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Each vector instance is independent — no shared state
 * - Concurrent reads on the same instance are safe
 * - Concurrent modifications require external synchronization
 * - Slices and views are invalidated by any modification to the vec
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - C11 _Static_assert used when available (non-fatal fallback otherwise)
 * - VEC_LIKELY / VEC_UNLIKELY use __builtin_expect on GNU C / Clang;
 *   define CANON_NO_GNU_EXTENSIONS to disable
 *
 * Quick start:
 * ```c
 * #include "data/vec/vec.h"
 *
 * // Instantiate a typed vector for int
 * DEFINE_VEC(static inline, int)
 *
 * // Stack-backed vector
 * int buf[64];
 * vec_int v = vec_int_init(buf, 64);
 * vec_int_push(&v, 10);
 * vec_int_push(&v, 20);
 *
 * // Heap-backed vector
 * vec_int h = vec_int_alloc(128);
 * vec_int_push(&h, 99);
 * vec_int_free(&h);
 *
 * // Arena-backed vector
 * vec_int a = vec_int_arena_alloc(&arena, 32);
 * vec_int_push(&a, 1);
 * // no free needed — arena owns the memory
 *
 * // Slice and byte views (requires DEFINE_SLICE then DEFINE_VEC_SLICE)
 * DEFINE_SLICE(int)            // from core/slice.h — define once per type
 * DEFINE_VEC_SLICE(int)        // wire up vec → slice integration
 *
 * slice_int sv = vec_int_as_slice(&v);   // typed view, no copy
 * bytes_t   bv = vec_int_as_bytes(&v);   // raw byte view
 *
 * // Pointer types (typedef first)
 * typedef void* voidptr;
 * DEFINE_VEC(static inline, voidptr)
 * ```
 *
 * Separate compilation (large projects):
 * ```c
 * // In tasks.h:
 * #include "data/vec/vec_decl.h"
 * DECLARE_VEC(Task)
 *
 * // In tasks.c:
 * #include "data/vec/vec_defn.h"
 * DEFINE_VEC(, Task)
 * ```
 *
 * Extension headers (optional):
 * ```c
 * #include "data/vec/vec_range.h"  // adds vec_extend_from_range
 * #include "data/vec/vec_fmt.h"    // adds vec_to_stringbuf
 * ```
 *
 * @sa vec_defn.h, vec_decl.h, vec_mangle.h, vec_impl.h
 * @sa vec_range.h, vec_fmt.h
 * @sa core/slice.h                     — DEFINE_SLICE, slice_##type, bytes_t
 * @sa data/convenience/dynvec.h        — auto-growing variant
 * @sa data/convenience/smallvec.h      — inline-first variant
 */

/* ════════════════════════════════════════════════════════════════════════════
   C11 static assert helper (non-fatal in C99)
   ════════════════════════════════════════════════════════════════════════════ */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /**
     * @brief Static assertion for vec element types (C11 only)
     *
     * Verifies element type has non-zero size at compile time.
     * Silently disabled in C99 mode.
     */
    #define VEC_ASSERT_TYPE(type) \
        _Static_assert(sizeof(type) > 0, "vec: element (type) must have non-zero size")
#else
    #define VEC_ASSERT_TYPE(type) /* C99: no static assert */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Branch hint helpers
   ════════════════════════════════════════════════════════════════════════════ */

#if (defined(__GNUC__) || defined(__clang__)) && !defined(CANON_NO_GNU_EXTENSIONS)
    /**
     * @brief Hints to the compiler that condition is likely true
     *
     * Used on hot paths (push to non-full vec, get within bounds).
     * Disabled when CANON_NO_GNU_EXTENSIONS is defined.
     */
    #define VEC_LIKELY(x)   __builtin_expect(!!(x), 1)
    /**
     * @brief Hints to the compiler that condition is likely false
     *
     * Used on error paths (push to full vec, out-of-bounds access).
     * Disabled when CANON_NO_GNU_EXTENSIONS is defined.
     */
    #define VEC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define VEC_LIKELY(x)   (x)
    #define VEC_UNLIKELY(x) (x)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_VEC_SLICE — slice.h integration for a vec type
   ════════════════════════════════════════════════════════════════════════════
   Call this AFTER both DEFINE_VEC(linkage, type) and DEFINE_SLICE(type).
   Generates zero-copy view functions that return canonical slice types
   from core/slice.h instead of raw pointer + length pairs.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Wires up slice_##type and bytes_t view functions for a vec type
 *
 * Prerequisites:
 * - DEFINE_VEC(linkage, type) must have been called
 * - DEFINE_SLICE(type) from core/slice.h must have been called
 *
 * Generated functions:
 * - vec_##type##_as_slice(v)       → slice_##type (typed view of [items, items+len))
 * - vec_##type##_as_slice_full(v)  → slice_##type (typed view of [items, items+cap))
 * - vec_##type##_as_bytes(v)       → bytes_t (raw byte view of used elements)
 *
 * All views are non-owning — they become invalid after any modification
 * to the vec (push, pop, insert, remove) or after the vec is freed/reset.
 *
 * @param type Element type — must match a prior DEFINE_VEC(linkage, type) call
 *
 * Example:
 * ```c
 * DEFINE_VEC(static inline, int)
 * DEFINE_SLICE(int)
 * DEFINE_VEC_SLICE(int)
 *
 * int buf[8];
 * vec_int v = vec_int_init(buf, 8);
 * vec_int_push(&v, 1);
 * vec_int_push(&v, 2);
 * vec_int_push(&v, 3);
 *
 * slice_int sv = vec_int_as_slice(&v);   // {ptr=buf, len=3}
 * int val;
 * slice_int_get(sv, 1, &val);  // val = 2
 *
 * bytes_t bv = vec_int_as_bytes(&v);     // {ptr=buf, len=12} (3 * sizeof(int))
 * ```
 */
#define DEFINE_VEC_SLICE(type) \
\
/** \
 * @brief Returns a typed slice_##type view over the vec's current elements \
 * \
 * Covers [items, items + len) — only the initialized elements. \
 * Does NOT include unfilled capacity slots. \
 * Non-owning — do not free the returned slice's ptr. \
 * \
 * @param v borrowed(const vec_##type*) — NULL-safe, returns empty slice \
 * @return slice_##type with ptr == v->items and len == v->len \
 * \
 * Performance: O(1) \
 */ \
static inline slice_##type vec_##type##_as_slice( \
    borrowed(const MANGLE_VEC_TYPE(type)*) v) { \
    if (!v || !v->items) { return slice_##type##_empty(); } \
    return slice_##type##_from(v->items, v->len); \
} \
\
/** \
 * @brief Returns a typed slice_##type view over the vec's full capacity \
 * \
 * Covers [items, items + capacity) — includes uninitialized slots beyond len. \
 * Use only when you need to inspect or pre-fill the entire buffer. \
 * \
 * @param v borrowed(const vec_##type*) — NULL-safe \
 * @return slice_##type with ptr == v->items and len == v->capacity \
 * \
 * Performance: O(1) \
 */ \
static inline slice_##type vec_##type##_as_slice_full( \
    borrowed(const MANGLE_VEC_TYPE(type)*) v) { \
    if (!v || !v->items) { return slice_##type##_empty(); } \
    return slice_##type##_from(v->items, v->capacity); \
} \
\
/** \
 * @brief Returns a bytes_t raw byte view over the vec's current elements \
 * \
 * Covers [items, items + len) in bytes — len * sizeof(type) total bytes. \
 * Useful for serialization, hashing, or passing to byte-level APIs. \
 * Non-owning — do not free the returned bytes_t's ptr. \
 * \
 * @param v borrowed(const vec_##type*) — NULL-safe \
 * @return bytes_t with ptr == (u8*)v->items and len == v->len * sizeof(type) \
 * \
 * Performance: O(1) \
 */ \
static inline bytes_t vec_##type##_as_bytes( \
    borrowed(const MANGLE_VEC_TYPE(type)*) v) { \
    if (!v || !v->items) { return bytes_empty(); } \
    return bytes_from(v->items, v->len * sizeof(type)); \
}

#endif /* CANON_DATA_VEC_H */
