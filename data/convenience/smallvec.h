#ifndef CANON_DATA_SMALLVEC_H
#define CANON_DATA_SMALLVEC_H

/**
 * @file smallvec.h
 *
 * @brief Inline-first vector with at-most-one spill
 *
 * smallvec<T> is a performance-oriented container optimized for:
 * - Small element counts
 * - Stack or arena locality
 * - Predictable allocation behavior
 *
 * Design rules:
 * ─────────────
 * - Inline storage is always used first
 * - At most ONE spill to heap or arena
 * - No automatic shrinking
 * - No hidden realloc loops
 * - Header-only, macro-generated, type-safe
 *
 * Conceptually:
 *   smallvec = stack buffer + optional owned backing store
 *
 * This type is intentionally simpler than vec<T>.
 * Use vec<T> when you need:
 *   - External buffers
 *   - Explicit ownership
 *   - Multi-stage growth
 */

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "core/arena.h"
#include "data/vec.h"

/* ─────────────────────────────────────────────────────────────
   Branch prediction helpers
   ───────────────────────────────────────────────────────────── */

#ifndef SMALLVEC_LIKELY
#define SMALLVEC_LIKELY(x)   (x)
#define SMALLVEC_UNLIKELY(x) (x)
#endif

/* ─────────────────────────────────────────────────────────────
   SMALLVEC DEFINITION
   ───────────────────────────────────────────────────────────── */

/**
 * DEFINE_SMALLVEC(type, INLINE_CAP)
 *
 * Generates a fully-typed inline-first vector:
 *   - smallvec_<type>
 *
 * Requirements:
 * ─────────────
 * - INLINE_CAP > 0
 * - type must be trivially copyable (memcpy-safe)
 *
 * Memory layout:
 * ──────────────
 * - inline_buf lives inside the struct
 * - data initially aliases inline_buf
 * - after spill, data points to heap or arena memory
 *
 * Spill rules:
 * ─────────────
 * - Spill occurs only when len == cap
 * - Spill happens at most once
 * - Capacity doubles on spill (amortized O(1))
 */
#define DEFINE_SMALLVEC(type, INLINE_CAP) \
\
typedef struct { \
    type*  data;          /* Active buffer */ \
    size_t len;           /* Number of elements */ \
    size_t cap;           /* Capacity of active buffer */ \
    Arena* arena;         /* Optional arena backing */ \
    bool   using_inline;  /* True if data == inline_buf */ \
    type   inline_buf[INLINE_CAP]; \
} smallvec_##type; \
\
/* ───────────────────────────────────────────────────────── \
   Constructors \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Initializes a smallvec using inline storage \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: sizeof(smallvec_##type) \
 * - Heap: none \
 */ \
static inline smallvec_##type smallvec_##type##_init(void) { \
    smallvec_##type v; \
    v.data = v.inline_buf; \
    v.len = 0; \
    v.cap = INLINE_CAP; \
    v.arena = NULL; \
    v.using_inline = true; \
    return v; \
} \
\
/** \
 * @brief Initializes a smallvec with arena spill \
 * \
 * Spill allocation (if needed) comes from arena. \
 */ \
static inline smallvec_##type smallvec_##type##_init_arena(Arena* arena) { \
    smallvec_##type v = smallvec_##type##_init(); \
    v.arena = arena; \
    return v; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Query operations \
   ───────────────────────────────────────────────────────── */ \
\
static inline size_t smallvec_##type##_len(const smallvec_##type* v) { \
    return v ? v->len : 0; \
} \
\
static inline size_t smallvec_##type##_capacity(const smallvec_##type* v) { \
    return v ? v->cap : 0; \
} \
\
static inline bool smallvec_##type##_is_empty(const smallvec_##type* v) { \
    return !v || v->len == 0; \
} \
\
static inline bool smallvec_##type##_using_inline(const smallvec_##type* v) { \
    return v && v->using_inline; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Element access \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Bounds-checked access \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool smallvec_##type##_get( \
    const smallvec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->data[i]; \
    return true; \
} \
\
/** \
 * @brief Unchecked access (zero overhead) \
 * \
 * ⚠️ Undefined behavior if i >= len \
 */ \
static inline type smallvec_##type##_get_unchecked( \
    const smallvec_##type* v, size_t i) { \
    assert(v && i < v->len); \
    return v->data[i]; \
} \
\
static inline type* smallvec_##type##_data(const smallvec_##type* v) { \
    return v ? v->data : NULL; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Internal spill (single-shot) \
   ───────────────────────────────────────────────────────── */ \
\
static inline bool smallvec_##type##_spill( \
    smallvec_##type* v, size_t min_cap) { \
    assert(v && v->using_inline); \
    \
    size_t new_cap = v->cap * 2; \
    if (new_cap < min_cap) new_cap = min_cap; \
    \
    type* new_buf = v->arena \
        ? arena_alloc_array(v->arena, type, new_cap) \
        : (type*)malloc(new_cap * sizeof(type)); \
    \
    if (!new_buf) return false; \
    \
    memcpy(new_buf, v->inline_buf, v->len * sizeof(type)); \
    v->data = new_buf; \
    v->cap = new_cap; \
    v->using_inline = false; \
    return true; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Push / Pop \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Appends element, spilling if needed \
 * \
 * Performance: \
 * - Amortized time: O(1) \
 * - Worst-case: O(n) on spill \
 * - Heap: ≤ 1 allocation \
 */ \
static inline bool smallvec_##type##_push( \
    smallvec_##type* v, type value) { \
    if (!v) return false; \
    \
    if (SMALLVEC_LIKELY(v->len < v->cap)) { \
        v->data[v->len++] = value; \
        return true; \
    } \
    \
    if (v->using_inline && \
        !smallvec_##type##_spill(v, v->len + 1)) { \
        return false; \
    } \
    \
    if (v->len >= v->cap) return false; \
    v->data[v->len++] = value; \
    return true; \
} \
\
static inline bool smallvec_##type##_pop( \
    smallvec_##type* v, type* out) { \
    if (!v || !out || v->len == 0) return false; \
    *out = v->data[--v->len]; \
    return true; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Insert / Remove (O(n)) \
   ───────────────────────────────────────────────────────── */ \
\
static inline bool smallvec_##type##_insert( \
    smallvec_##type* v, size_t i, type value) { \
    if (!v || i > v->len) return false; \
    \
    if (v->len == v->cap) { \
        if (!v->using_inline || \
            !smallvec_##type##_spill(v, v->len + 1)) { \
            return false; \
        } \
    } \
    \
    memmove(&v->data[i + 1], &v->data[i], \
            (v->len - i) * sizeof(type)); \
    v->data[i] = value; \
    v->len++; \
    return true; \
} \
\
static inline bool smallvec_##type##_remove( \
    smallvec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->data[i]; \
    memmove(&v->data[i], &v->data[i + 1], \
            (v->len - i - 1) * sizeof(type)); \
    v->len--; \
    return true; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Bulk operations \
   ───────────────────────────────────────────────────────── */ \
\
static inline bool smallvec_##type##_extend( \
    smallvec_##type* v, const type* src, size_t count) { \
    if (!v || !src) return false; \
    \
    if (v->len + count > v->cap) { \
        if (!v->using_inline || \
            !smallvec_##type##_spill(v, v->len + count)) { \
            return false; \
        } \
    } \
    \
    memcpy(&v->data[v->len], src, count * sizeof(type)); \
    v->len += count; \
    return true; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Clear / Free \
   ───────────────────────────────────────────────────────── */ \
\
static inline void smallvec_##type##_clear(smallvec_##type* v) { \
    if (v) v->len = 0; \
} \
\
static inline void smallvec_##type##_free(smallvec_##type* v) { \
    if (!v) return; \
    if (!v->using_inline && !v->arena) { \
        free(v->data); \
    } \
    *v = smallvec_##type##_init(); \
} \
\
/* ───────────────────────────────────────────────────────── \
   Borrowed vec view (zero-copy) \
   ───────────────────────────────────────────────────────── */ \
\
static inline vec_##type smallvec_##type##_as_vec( \
    smallvec_##type* v) { \
    assert(v); \
    return vec_##type##_init(v->data, v->cap); \
}

#endif /* CANON_DATA_SMALLVEC_H */
