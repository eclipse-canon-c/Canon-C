#ifndef CANON_DATA_SMALLVEC_H
#define CANON_DATA_SMALLVEC_H

#include <stdlib.h>
#include "core/primitives/types.h"       /* usize, bool */
#include "core/primitives/contract.h"    /* require_msg, ensure_msg */
#include "core/memory.h"                 /* mem_copy, mem_move */
#include "core/arena.h"                  /* Arena*, arena_alloc_array */
#include "data/vec/vec.h"                /* MANGLE_VEC_TYPE, MANGLE_VEC_INIT */

/**
 * @file convenience/smallvec.h
 * @brief Inline-first vector with at-most-one spill to heap or arena
 *
 * smallvec is a performance-oriented container optimized for small element
 * counts where heap allocation is the exception rather than the rule.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Inline storage always used first (inside the struct — no heap needed)
 * - At most ONE spill: inline → heap (malloc) or inline → arena
 * - No automatic shrinking, no hidden realloc loops
 * - After spill, capacity is fixed — no further growth
 * - Header-only, macro-generated, type-safe
 *
 * Memory layout:
 * ────────────────────────────────────────────────────────────────────────────
 * - inline_buf[INLINE_CAP] lives inside the struct itself
 * - data initially aliases inline_buf (using_inline == true)
 * - After spill: data points to heap or arena memory, using_inline == false
 *
 * Spill rules:
 * ────────────────────────────────────────────────────────────────────────────
 * - Spill occurs exactly when len == cap and push/insert/extend is called
 * - Spill happens at most once — capacity doubles at spill time
 * - If arena != NULL: spill allocates from arena (no free needed)
 * - If arena == NULL: spill allocates via malloc (free on _free())
 * - After spill: push into a full non-inline buffer returns false (no regrow)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each smallvec instance is independent — no shared state.
 * Concurrent modifications require external synchronization.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses Canon-C core modules only
 * - No platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - push (inline): O(1) — no allocation
 * - push (spill): O(n) — single allocation + mem_copy, at most once
 * - push (post-spill): O(1) if cap not yet exhausted, false if full
 * - pop: O(1)
 * - insert / remove: O(n) — mem_move
 * - extend: O(count) — mem_copy, may trigger one spill
 * - get / first / last: O(1)
 * - clear: O(1)
 * - free: O(1)
 * - struct size: sizeof(type*) + 2*sizeof(usize) + sizeof(Arena*) +
 *   sizeof(bool) + INLINE_CAP*sizeof(type)
 *
 * Quick start:
 * ```c
 * #include "data/convenience/smallvec.h"
 *
 * DEFINE_SMALLVEC(int, 8)
 *
 * // Stack-local, inline storage — zero heap allocation for <= 8 elements
 * smallvec_int v = smallvec_int_init();
 * smallvec_int_push(&v, 1);
 * smallvec_int_push(&v, 2);
 * // ... no allocation yet
 *
 * // After the 9th push: spills to heap automatically
 * smallvec_int_free(&v); // REQUIRED if heap-spilled without arena
 *
 * // Arena-backed spill — no free needed
 * smallvec_int a = smallvec_int_init_arena(&my_arena);
 * smallvec_int_push(&a, 1);
 * // spill goes to arena — arena_reset() handles cleanup
 * ```
 *
 * When to use this instead of data/vec/vec.h:
 * ────────────────────────────────────────────────────────────────────────────
 * - Most elements fit in inline_buf (< INLINE_CAP)
 * - You want zero heap allocation for the common case
 * - You are OK with "at most once" growth semantics
 *
 * When to use data/vec/vec.h instead:
 * ────────────────────────────────────────────────────────────────────────────
 * - Element counts are large or highly variable
 * - You need full caller control over the backing buffer
 * - You need multi-stage growth or reuse across arena resets
 *
 * @sa data/vec/vec.h — fixed-capacity, explicit-allocation alternative
 * @sa data/convenience/dynvec.h — unlimited growth, always heap
 */

/* ════════════════════════════════════════════════════════════════════════════
   Branch hint helpers
   ════════════════════════════════════════════════════════════════════════════ */

#if defined(__GNUC__) || defined(__clang__)
    #define SMALLVEC_LIKELY(x) __builtin_expect(!!(x), 1)
    #define SMALLVEC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define SMALLVEC_LIKELY(x) (x)
    #define SMALLVEC_UNLIKELY(x) (x)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_SMALLVEC — instantiate a typed inline-first vector
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a typed inline-first vector for element type `type`
 *
 * Requirements:
 * - INLINE_CAP > 0
 * - type must be trivially copyable (mem_copy-safe)
 * - For pointer types, typedef first: typedef void* voidptr; DEFINE_SMALLVEC(voidptr, 8)
 *
 * Generated type: `smallvec_##type`
 *
 * Generated functions: (see quick start for examples)
 */
#define DEFINE_SMALLVEC(type, INLINE_CAP) \
\
/** \
 * @brief Inline-first vector for type with INLINE_CAP elements inline \
 * \
 * Invariants: \
 * - data == inline_buf when using_inline == true \
 * - data != inline_buf when using_inline == false (heap or arena spill) \
 * - len <= cap always \
 * - cap == INLINE_CAP when using_inline == true \
 * \
 * Do not access fields directly — use the provided functions. \
 * \
 * Memory layout: \
 * - sizeof(smallvec_##type) includes INLINE_CAP inline element slots \
 * - No heap allocation unless/until a spill occurs \
 */ \
typedef struct { \
    type* data;             /**< Active buffer (inline_buf or heap/arena) */ \
    usize len;              /**< Number of elements currently stored */ \
    usize cap;              /**< Capacity of active buffer in elements */ \
    Arena* arena;           /**< Spill destination (NULL = malloc) */ \
    bool using_inline;      /**< true if data == inline_buf */ \
    type inline_buf[INLINE_CAP]; /**< Inline storage — lives in the struct */ \
} smallvec_##type; \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Constructors \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Initializes a smallvec using inline storage — no heap allocation \
 * \
 * @return Initialized smallvec_##type pointing data at inline_buf \
 * \
 * @post v.data == v.inline_buf, v.len == 0, v.cap == INLINE_CAP \
 * @post v.using_inline == true, v.arena == NULL \
 * \
 * Performance: O(1), no heap allocation \
 */ \
static inline smallvec_##type smallvec_##type##_init(void) { \
    smallvec_##type v; \
    v.len = 0; \
    v.cap = INLINE_CAP; \
    v.arena = NULL; \
    v.using_inline = true; \
    v.data = v.inline_buf; \
    return v; \
} \
\
/** \
 * @brief Initializes a smallvec that spills into an arena when full \
 * \
 * Identical to smallvec_##type##_init() but stores the arena pointer \
 * so that the spill allocation comes from arena_alloc_array() instead \
 * of malloc(). arena_reset() handles cleanup — no _free() needed. \
 * \
 * @param arena Arena to use for spill allocation (must remain valid) \
 * @return Initialized smallvec_##type with inline storage \
 * \
 * @pre arena != NULL — checked via require_msg() \
 * \
 * Performance: O(1), no heap allocation \
 */ \
static inline smallvec_##type smallvec_##type##_init_arena(Arena* arena) { \
    require_msg(arena != NULL, "smallvec_" #type "_init_arena: arena cannot be NULL"); \
    smallvec_##type v = smallvec_##type##_init(); \
    v.arena = arena; \
    return v; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Internal helper — single-shot spill \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Spills inline buffer to heap or arena — called at most once \
 * \
 * Allocates new_cap = max(v->cap * 2, min_cap) elements from heap or arena, \
 * copies inline_buf into new allocation, then updates v->data, v->cap, \
 * and v->using_inline. \
 * \
 * @pre v != NULL — checked via ensure_msg() in debug builds \
 * @pre v->using_inline == true \
 * \
 * @note Internal use only — do not call directly. \
 * \
 * Performance: O(len) — single allocation + mem_copy \
 */ \
static inline bool smallvec_##type##_spill( \
    smallvec_##type* v, usize min_cap) { \
    ensure_msg(v != NULL, "smallvec_" #type "_spill: v cannot be NULL"); \
    ensure_msg(v->using_inline, "smallvec_" #type "_spill: not using inline buffer"); \
    if (!v || !v->using_inline) return false; \
    \
    usize new_cap = v->cap * 2; \
    if (new_cap < min_cap) new_cap = min_cap; \
    \
    type* new_buf = v->arena \
        ? (type*)arena_alloc_array(v->arena, type, new_cap) \
        : (type*)malloc(new_cap * sizeof(type)); \
    \
    if (!new_buf) return false; \
    \
    mem_copy(new_buf, v->inline_buf, v->len * sizeof(type)); \
    v->data = new_buf; \
    v->cap = new_cap; \
    v->using_inline = false; \
    return true; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Queries \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Returns the current element count \
 * \
 * @param v smallvec to query (NULL-safe) \
 * @return usize — 0 if v == NULL \
 */ \
static inline usize smallvec_##type##_len(const smallvec_##type* v) { \
    return v ? v->len : 0; \
} \
\
/** \
 * @brief Returns the current buffer capacity in elements \
 * \
 * @param v smallvec to query (NULL-safe) \
 * @return usize — 0 if v == NULL \
 */ \
static inline usize smallvec_##type##_capacity(const smallvec_##type* v) { \
    return v ? v->cap : 0; \
} \
\
/** \
 * @brief Returns true if the vector has no elements \
 * \
 * @param v smallvec to check (NULL-safe) \
 * @return true if empty or NULL \
 */ \
static inline bool smallvec_##type##_is_empty(const smallvec_##type* v) { \
    return !v || v->len == 0; \
} \
\
/** \
 * @brief Returns true if currently using inline storage (no spill yet) \
 * \
 * @param v smallvec to check (NULL-safe) \
 * @return true if using_inline == true and v != NULL \
 */ \
static inline bool smallvec_##type##_using_inline(const smallvec_##type* v) { \
    return v && v->using_inline; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Element access \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Copies element at index i into *out (bounds-checked) \
 * \
 * @param v smallvec to read from (NULL-safe) \
 * @param i Index to read \
 * @param out Pointer to store element value \
 * @return true on success, false if v == NULL, out == NULL, or i >= v->len \
 */ \
static inline bool smallvec_##type##_get( \
    const smallvec_##type* v, usize i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->data[i]; \
    return true; \
} \
\
/** \
 * @brief Returns element at index i without bounds checking (fast path) \
 * \
 * @param v smallvec to read from \
 * @param i Index to read \
 * @return Element value \
 * \
 * @pre v != NULL \
 * @pre i < v->len — caller must guarantee this \
 * \
 * @note Preconditions checked via ensure_msg() in debug builds only. \
 * Undefined behavior in release builds if violated. \
 */ \
static inline type smallvec_##type##_get_unchecked( \
    const smallvec_##type* v, usize i) { \
    ensure_msg(v != NULL, "smallvec_" #type "_get_unchecked: v cannot be NULL"); \
    ensure_msg(i < v->len, "smallvec_" #type "_get_unchecked: index out of bounds"); \
    return v->data[i]; \
} \
\
/** \
 * @brief Returns the raw buffer pointer \
 * \
 * @param v smallvec to query (NULL-safe) \
 * @return type* — NULL if v == NULL \
 */ \
static inline type* smallvec_##type##_data(const smallvec_##type* v) { \
    return v ? v->data : NULL; \
} \
\
/** \
 * @brief Returns pointer to first element, or NULL if empty \
 * \
 * @param v smallvec to query (NULL-safe) \
 */ \
static inline type* smallvec_##type##_first(const smallvec_##type* v) { \
    return (v && v->len > 0) ? &v->data[0] : NULL; \
} \
\
/** \
 * @brief Returns pointer to last element, or NULL if empty \
 * \
 * @param v smallvec to query (NULL-safe) \
 */ \
static inline type* smallvec_##type##_last(const smallvec_##type* v) { \
    return (v && v->len > 0) ? &v->data[v->len - 1] : NULL; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Push / Pop \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Appends an element, spilling to heap/arena if inline is full \
 * \
 * If still in inline storage and full: spills once to heap or arena. \
 * If already spilled and full: returns false (no second growth). \
 * \
 * @param v smallvec to push into \
 * @param value Element to append \
 * @return true on success, false on spill failure or v == NULL or post-spill full \
 * \
 * @post On true: element appended, v->len incremented by 1 \
 * @post On false: v is unchanged \
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
    if (v->using_inline && !smallvec_##type##_spill(v, v->len + 1)) { \
        return false; \
    } \
    \
    if (v->len >= v->cap) return false; /* post-spill still full */ \
    v->data[v->len++] = value; \
    return true; \
} \
\
/** \
 * @brief Removes and returns the last element \
 * \
 * @param v smallvec to pop from (NULL-safe) \
 * @param out Pointer to store the removed element \
 * @return true on success, false if v == NULL, out == NULL, or v->len == 0 \
 */ \
static inline bool smallvec_##type##_pop( \
    smallvec_##type* v, type* out) { \
    if (!v || !out || v->len == 0) return false; \
    *out = v->data[--v->len]; \
    return true; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Insert / Remove (O(n)) \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Inserts element at index i, shifting elements right (may spill once) \
 * \
 * @param v smallvec to insert into \
 * @param i Insertion index (must be <= v->len) \
 * @param value Element to insert \
 * @return true on success, false on v == NULL, i > v->len, or spill failure \
 * \
 * @note Returns false if already spilled and full — no second growth. \
 */ \
static inline bool smallvec_##type##_insert( \
    smallvec_##type* v, usize i, type value) { \
    if (!v || i > v->len) return false; \
    \
    if (v->len == v->cap) { \
        if (!v->using_inline || !smallvec_##type##_spill(v, v->len + 1)) { \
            return false; /* full and cannot grow */ \
        } \
    } \
    \
    if (i < v->len) { \
        mem_move(&v->data[i + 1], &v->data[i], \
                 (v->len - i) * sizeof(type)); \
    } \
    v->data[i] = value; \
    v->len++; \
    return true; \
} \
\
/** \
 * @brief Removes element at index i, shifting elements left \
 * \
 * @param v smallvec to remove from (NULL-safe) \
 * @param i Index to remove (must be < v->len) \
 * @param out Pointer to store the removed element \
 * @return true on success, false if v == NULL, out == NULL, or i >= v->len \
 */ \
static inline bool smallvec_##type##_remove( \
    smallvec_##type* v, usize i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->data[i]; \
    if (i < v->len - 1) { \
        mem_move(&v->data[i], &v->data[i + 1], \
                 (v->len - i - 1) * sizeof(type)); \
    } \
    v->len--; \
    return true; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Bulk operations \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Appends count elements from src array, spilling once if needed \
 * \
 * @param v smallvec to extend \
 * @param src Source array (must point to at least count elements) \
 * @param count Number of elements to append \
 * @return true on success, false on v == NULL, src == NULL, or spill failure \
 * \
 * @note Returns false if already spilled and insufficient remaining capacity. \
 */ \
static inline bool smallvec_##type##_extend( \
    smallvec_##type* v, const type* src, usize count) { \
    if (!v || !src) return false; \
    if (count == 0) return true; \
    \
    if (v->len + count > v->cap) { \
        if (!v->using_inline || !smallvec_##type##_spill(v, v->len + count)) { \
            return false; \
        } \
    } \
    \
    mem_copy(&v->data[v->len], src, count * sizeof(type)); \
    v->len += count; \
    return true; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Clear / Free \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Resets element count to 0 without freeing any buffer \
 * \
 * @param v smallvec to clear (NULL-safe) \
 * \
 * @post v->len == 0 \
 * @note Buffer is not zeroed. Post-spill heap buffer is not freed. \
 */ \
static inline void smallvec_##type##_clear(smallvec_##type* v) { \
    if (v) v->len = 0; \
} \
\
/** \
 * @brief Frees the spill buffer (if heap-allocated) and reinitializes inline \
 * \
 * @param v smallvec to free (NULL-safe) \
 * \
 * @post v is re-initialized to inline storage \
 * \
 * @note If the spill was into an arena, nothing is freed — arena_reset() \
 * handles cleanup. free() is called only if using_inline == false \
 * AND arena == NULL. \
 */ \
static inline void smallvec_##type##_free(smallvec_##type* v) { \
    if (!v) return; \
    if (!v->using_inline && !v->arena) { \
        free(v->data); \
    } \
    *v = smallvec_##type##_init(); \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Interop \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Returns a zero-copy borrowed canon_vec_##type view over the buffer \
 * \
 * Allows passing a smallvec's contents to any API that accepts a vec. \
 * The returned vec does NOT own the buffer — do not store or outlive v. \
 * \
 * @param v smallvec to borrow from \
 * @return canon_vec_##type backed by v->data with v->cap capacity \
 * \
 * @pre v != NULL — checked via require_msg() \
 * \
 * @note Mutating the returned vec's buffer also mutates v's buffer. \
 * Do not call vec functions that reallocate (e.g. vec_alloc). \
 */ \
static inline MANGLE_VEC_TYPE(type) smallvec_##type##_as_vec( \
    smallvec_##type* v) { \
    require_msg(v != NULL, "smallvec_" #type "_as_vec: v cannot be NULL"); \
    return MANGLE_VEC_INIT(type)(v->data, v->cap); \
}

#endif /* CANON_DATA_SMALLVEC_H */
