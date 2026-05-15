#ifndef CANON_DATA_CONVENIENCE_DYNVEC_H
#define CANON_DATA_CONVENIENCE_DYNVEC_H

#include <stdlib.h>
#include "core/primitives/types.h"       /* usize, bool */
#include "core/primitives/contract.h"    /* require_msg, ensure_msg */
#include "core/memory.h"                 /* mem_copy, mem_move */

#ifdef CANON_LIFETIME_DEBUG
    #include "core/primitives/lifetime.h"  /* region_id_t, lifetime_t */
#endif

/**
 * @file convenience/dynvec.h
 * @brief Auto-growing typed vector with hidden heap allocation
 *
 * CONVENIENCE LAYER — trades explicitness for ergonomics.
 * All pushes grow automatically. Caller must call dynvec_##type##_free() when done.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Automatic heap allocation — no caller-provided buffer needed
 * - Automatic 2× growth on overflow
 * - Owns its own memory — must free to avoid leaks
 * - No arena support — use data/vec/vec.h for arena-backed vectors
 * - Type-safe via DEFINE_DYNVEC macro
 *
 * When to use this instead of data/vec/vec.h:
 * ────────────────────────────────────────────────────────────────────────────
 * - Collections of unknown or unbounded size
 * - Rapid prototyping
 * - Desktop/server applications with ample memory
 * - When convenience matters more than determinism
 *
 * When to use data/vec/vec.h instead:
 * ────────────────────────────────────────────────────────────────────────────
 * - Performance-critical or real-time code
 * - Embedded systems or bounded-memory environments
 * - When you need explicit control over allocation strategy
 * - Arena-backed or stack-backed vectors
 *
 * Growth strategy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Initial capacity: DYNVEC_INITIAL_CAPACITY (default 8 elements)
 * - Growth factor: DYNVEC_GROWTH_FACTOR (default 2×)
 * - No automatic shrinking — call dynvec_##type##_shrink_to_fit() explicitly
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 * ────────────────────────────────────────────────────────────────────────────
 *   - Embeds a lifetime_t lt field (id + open) on each dynvec_##type instance.
 *   - dynvec_##type##_init() and dynvec_##type##_with_capacity() open a fresh
 *     lifetime; the ID is derived from &v.
 *   - dynvec_##type##_grow() and dynvec_##type##_shrink_to_fit() RESTAMP the
 *     lifetime ID conditionally — only when realloc actually moves the
 *     buffer. Operations that don't trigger reallocation (push when cap is
 *     sufficient, pop, clear, get, set, remove) do NOT restamp because the
 *     buffer address is unchanged and existing borrows remain valid.
 *   - dynvec_##type##_free() CLOSES the lifetime (lt.open = false). Any
 *     borrow that captured this dynvec's ID will fire require_msg via
 *     lifetime_assert_valid() on the next read.
 *   - dynvec_##type##_clear() does NOT restamp. Clear only sets len = 0;
 *     the buffer is unchanged. Lifetime tracking catches use-after-relocation
 *     and use-after-free; it does NOT catch use-of-removed-element, because
 *     the memory addresses of removed elements are still valid bytes in the
 *     same buffer owned by the same dynvec. That bug class is out of scope
 *     for the substrate.
 *   Zero cost in release builds — struct layout is identical without the flag.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each dynvec instance is independent — no shared state.
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
 * - push: Amortized O(1), worst-case O(n) on realloc
 * - pop: O(1)
 * - insert: O(n) — shifts elements right, may realloc
 * - remove: O(n) — shifts elements left
 * - extend: O(count), may realloc
 * - get/set/at: O(1)
 * - reserve: O(1) if cap sufficient, else O(n) on realloc
 * - shrink_to_fit: O(n) — realloc to exact size
 * - free: O(1)
 * - struct size: sizeof(type*) + 2*sizeof(usize)
 *
 * Quick start:
 * ```c
 * #include "data/convenience/dynvec.h"
 *
 * DEFINE_DYNVEC(int)
 *
 * dynvec_int v = dynvec_int_init();
 * dynvec_int_push(&v, 10);
 * dynvec_int_push(&v, 20);
 * dynvec_int_push(&v, 30);
 *
 * int val;
 * dynvec_int_pop(&v, &val); // val = 30
 * dynvec_int_free(&v); // REQUIRED — always call this
 * ```
 *
 * @sa data/vec/vec.h               — fixed-capacity, explicit-allocation alternative
 * @sa core/primitives/lifetime.h   — canonical home of region_id_t and lifetime_t
 * @sa core/region.h                — lifetime_assert_valid runtime check
 */

/* ════════════════════════════════════════════════════════════════════════════
   Configuration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initial heap allocation in elements when buffer is first needed
 *
 * Override before including this file.
 * Default: 8 elements.
 */
#ifndef DYNVEC_INITIAL_CAPACITY
    #define DYNVEC_INITIAL_CAPACITY ((usize)8)
#endif

/**
 * @brief Capacity growth multiplier applied when the buffer is full
 *
 * Default: 2 (doubles on each realloc).
 */
#ifndef DYNVEC_GROWTH_FACTOR
    #define DYNVEC_GROWTH_FACTOR ((usize)2)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Branch hint helpers
   ════════════════════════════════════════════════════════════════════════════ */

#if defined(__GNUC__) || defined(__clang__)
    #define DYNVEC_LIKELY(x) __builtin_expect(!!(x), 1)
    #define DYNVEC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define DYNVEC_LIKELY(x) (x)
    #define DYNVEC_UNLIKELY(x) (x)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime macros for the field and the helper bodies
   ════════════════════════════════════════════════════════════════════════════
   These are defined ONCE here (outside DEFINE_DYNVEC) so the same field
   declaration and helper body expand identically into every instantiation.

   In release builds (CANON_LIFETIME_DEBUG undefined):
     - DYNVEC_LIFETIME_FIELD_ expands to nothing — no lt field on the struct
     - The three _BODY_ macros expand to nothing — helpers become no-ops
       (the trailing `(void)v;` in each helper absorbs the unused-parameter
       warning)
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    #define DYNVEC_LIFETIME_FIELD_                                      \
        lifetime_t lt; /**< [debug] Lifetime token: id + open */
    #define DYNVEC_LIFETIME_OPEN_BODY_(vp)                              \
        do {                                                            \
            (vp)->lt.id   = (region_id_t)(uintptr_t)(vp);              \
            (vp)->lt.open = true;                                       \
        } while (0);
    /* XOR with golden-ratio constant guarantees the new id differs from
       the old one even when the dynvec address is reused. */
    #define DYNVEC_LIFETIME_RESTAMP_BODY_(vp)                           \
        do {                                                            \
            (vp)->lt.id ^= (region_id_t)0x9E3779B97F4A7C15ULL;          \
        } while (0);
    #define DYNVEC_LIFETIME_CLOSE_BODY_(vp)                             \
        do { (vp)->lt.open = false; } while (0);
#else
    #define DYNVEC_LIFETIME_FIELD_           /* empty */
    #define DYNVEC_LIFETIME_OPEN_BODY_(vp)   /* empty */
    #define DYNVEC_LIFETIME_RESTAMP_BODY_(vp) /* empty */
    #define DYNVEC_LIFETIME_CLOSE_BODY_(vp)  /* empty */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_DYNVEC — instantiate a typed auto-growing vector
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a fully typed auto-growing vector for element type `type`
 *
 * Requirements:
 * - type must be trivially copyable (memcpy-safe)
 * - For pointer types, typedef first: typedef void* voidptr; DEFINE_DYNVEC(voidptr)
 *
 * Generated type: `dynvec_##type`
 *
 * Generated functions: (see quick start for examples)
 */
#define DEFINE_DYNVEC(type) \
\
/** \
 * @brief Auto-growing heap-allocated vector for type \
 * \
 * Invariants: \
 * - data != NULL when cap > 0 \
 * - len <= cap \
 * \
 * Do not access fields directly — use the provided functions. \
 * \
 * Memory layout: \
 * - sizeof(dynvec_##type) = sizeof(type*) + 2*sizeof(usize) [+ sizeof(lifetime_t) under CANON_LIFETIME_DEBUG] \
 * - Backing buffer: cap * sizeof(type) bytes on the heap \
 */ \
typedef struct { \
    type* data;     /**< Heap-owned element buffer (NULL until first push) */ \
    usize len;      /**< Current element count */ \
    usize cap;      /**< Allocated buffer capacity in elements */ \
    DYNVEC_LIFETIME_FIELD_ \
} dynvec_##type; \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Internal: lifetime helpers (per-instantiation, compiled away in release) \
   ════════════════════════════════════════════════════════════════════════════ \
   See file docblock for the full contract. Summary: \
     - _open_:    sets id (from &v) and marks open. Used by constructors. \
     - _restamp_: XORs id with the golden-ratio constant. Used by grow / \
                  shrink when realloc moves the buffer. \
     - _close_:   marks closed. Used by free. \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
static inline void dynvec_##type##_lifetime_open_(dynvec_##type* v) { \
    DYNVEC_LIFETIME_OPEN_BODY_(v) \
    (void)v; \
} \
\
static inline void dynvec_##type##_lifetime_restamp_(dynvec_##type* v) { \
    DYNVEC_LIFETIME_RESTAMP_BODY_(v) \
    (void)v; \
} \
\
static inline void dynvec_##type##_lifetime_close_(dynvec_##type* v) { \
    DYNVEC_LIFETIME_CLOSE_BODY_(v) \
    (void)v; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Internal helper — capacity growth \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Grows buffer to at least min_cap elements \
 * \
 * @pre v != NULL — checked via ensure_msg() in debug builds \
 * \
 * @note Internal use only. \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): restamps lt.id ONLY if realloc moved the \
 * buffer. If the buffer was reallocated in place (new_data == old_data) or \
 * if cap was already sufficient, lt.id is preserved and existing borrows \
 * remain valid. \
 * \
 * Performance: \
 * - Time: O(n) — realloc copies existing elements \
 * - Space: Allocates up to 2× current cap * sizeof(type) \
 */ \
static inline bool dynvec_##type##_grow(dynvec_##type* v, usize min_cap) { \
    ensure_msg(v != NULL, "dynvec_" #type "_grow: v cannot be NULL"); \
    if (!v) return false; \
    usize new_cap = (v->cap == 0) \
        ? DYNVEC_INITIAL_CAPACITY \
        : v->cap * DYNVEC_GROWTH_FACTOR; \
    if (new_cap < min_cap) new_cap = min_cap; \
    type* old_data = v->data; \
    type* new_data = (type*)realloc(v->data, new_cap * sizeof(type)); \
    if (!new_data) return false; \
    v->data = new_data; \
    v->cap = new_cap; \
    if (new_data != old_data) { \
        dynvec_##type##_lifetime_restamp_(v); \
    } \
    return true; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Constructors \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Creates an empty dynvec with no initial allocation \
 * \
 * No heap allocation until the first push. \
 * \
 * @return Zero-initialized dynvec_##type \
 * \
 * @post result.data == NULL, result.len == 0, result.cap == 0 \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token. The ID is \
 * derived from the returned struct's address — borrows constructed against \
 * this dynvec carry this ID. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: sizeof(dynvec_##type) — no heap allocation \
 */ \
static inline dynvec_##type dynvec_##type##_init(void) { \
    dynvec_##type v; \
    v.data = NULL; \
    v.len  = 0; \
    v.cap  = 0; \
    dynvec_##type##_lifetime_open_(&v); \
    return v; \
} \
\
/** \
 * @brief Creates a dynvec with pre-allocated heap capacity \
 * \
 * Useful when the expected element count is known, avoiding \
 * intermediate reallocations. \
 * \
 * @param capacity Number of elements to pre-allocate \
 * @return Initialized dynvec_##type with reserved capacity \
 * \
 * @post On success: result.cap == capacity, result.len == 0 \
 * @post On failure (OOM or capacity == 0): returns dynvec_##type##_init() \
 * \
 * @note Call dynvec_##type##_free() when done. \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token, same as init. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: capacity * sizeof(type) bytes on the heap \
 */ \
static inline dynvec_##type dynvec_##type##_with_capacity(usize capacity) { \
    dynvec_##type v = dynvec_##type##_init(); \
    if (capacity == 0) return v; \
    v.data = (type*)malloc(capacity * sizeof(type)); \
    if (v.data) v.cap = capacity; \
    return v; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Queries \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Returns the current element count \
 * \
 * @param v dynvec to query (NULL-safe) \
 * @return usize — 0 if v == NULL \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline usize dynvec_##type##_len(const dynvec_##type* v) { \
    return v ? v->len : 0; \
} \
\
/** \
 * @brief Returns the current heap capacity in elements \
 * \
 * @param v dynvec to query (NULL-safe) \
 * @return usize — 0 if v == NULL or no allocation yet \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline usize dynvec_##type##_capacity(const dynvec_##type* v) { \
    return v ? v->cap : 0; \
} \
\
/** \
 * @brief Returns true if the vector has no elements \
 * \
 * @param v dynvec to check (NULL-safe) \
 * @return true if empty or NULL \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool dynvec_##type##_is_empty(const dynvec_##type* v) { \
    return !v || v->len == 0; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Element access \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Copies element at index i into *out (bounds-checked) \
 * \
 * @param v dynvec to read from (NULL-safe) \
 * @param i Index to read \
 * @param out Pointer to store element value \
 * @return true on success, false if v == NULL, out == NULL, or i >= v->len \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool dynvec_##type##_get( \
    const dynvec_##type* v, usize i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->data[i]; \
    return true; \
} \
\
/** \
 * @brief Returns element at index i without bounds checking (fast path) \
 * \
 * @param v dynvec to read from \
 * @param i Index to read \
 * @return Element value \
 * \
 * @pre v != NULL \
 * @pre i < v->len — caller must guarantee this \
 * \
 * @note Preconditions checked via ensure_msg() in debug builds only. \
 * Undefined behavior in release builds if violated. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type dynvec_##type##_get_unchecked( \
    const dynvec_##type* v, usize i) { \
    ensure_msg(v != NULL, "dynvec_" #type "_get_unchecked: v cannot be NULL"); \
    ensure_msg(i < v->len, "dynvec_" #type "_get_unchecked: index out of bounds"); \
    return v->data[i]; \
} \
\
/** \
 * @brief Sets element at index i to value (bounds-checked) \
 * \
 * @param v dynvec to write to (NULL-safe) \
 * @param i Index to write \
 * @param value Value to set \
 * @return true on success, false if v == NULL or i >= v->len \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool dynvec_##type##_set( \
    dynvec_##type* v, usize i, type value) { \
    if (!v || i >= v->len) return false; \
    v->data[i] = value; \
    return true; \
} \
\
/** \
 * @brief Returns the raw buffer pointer \
 * \
 * @param v dynvec to query (NULL-safe) \
 * @return type* — NULL if v == NULL or buffer not yet allocated \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* dynvec_##type##_data(const dynvec_##type* v) { \
    return v ? v->data : NULL; \
} \
\
/** \
 * @brief Returns pointer to first element, or NULL if empty \
 * \
 * @param v dynvec to query (NULL-safe) \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* dynvec_##type##_first(const dynvec_##type* v) { \
    return (v && v->len > 0) ? &v->data[0] : NULL; \
} \
\
/** \
 * @brief Returns pointer to last element, or NULL if empty \
 * \
 * @param v dynvec to query (NULL-safe) \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* dynvec_##type##_last(const dynvec_##type* v) { \
    return (v && v->len > 0) ? &v->data[v->len - 1] : NULL; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Modification (auto-growing) \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Appends an element, growing the buffer if needed \
 * \
 * @param v dynvec to push into \
 * @param value Element to append \
 * @return true on success, false on allocation failure or v == NULL \
 * \
 * @post On true: element appended, v->len incremented by 1 \
 * @post On false: v is unchanged \
 * \
 * @note May allocate heap memory. \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): if growth was triggered AND the buffer \
 * was relocated, the lifetime ID is restamped. If no growth was needed, \
 * the lifetime ID is preserved. \
 * \
 * Performance: \
 * - Time: Amortized O(1), worst-case O(n) on realloc \
 * - Space: May allocate up to 2× current cap * sizeof(type) \
 */ \
static inline bool dynvec_##type##_push( \
    dynvec_##type* v, type value) { \
    if (!v) return false; \
    if (DYNVEC_UNLIKELY(v->len >= v->cap)) { \
        if (!dynvec_##type##_grow(v, v->len + 1)) return false; \
    } \
    v->data[v->len++] = value; \
    return true; \
} \
\
/** \
 * @brief Removes and returns the last element \
 * \
 * @param v dynvec to pop from (NULL-safe) \
 * @param out Pointer to store the removed element \
 * @return true on success, false if v == NULL, out == NULL, or v->len == 0 \
 * \
 * Performance: \
 * - Time: O(1) — no shrinking \
 * - Space: O(1) \
 */ \
static inline bool dynvec_##type##_pop( \
    dynvec_##type* v, type* out) { \
    if (!v || !out || v->len == 0) return false; \
    *out = v->data[--v->len]; \
    return true; \
} \
\
/** \
 * @brief Inserts element at index i, shifting elements right, growing if needed \
 * \
 * @param v dynvec to insert into \
 * @param i Insertion index (must be <= v->len) \
 * @param value Element to insert \
 * @return true on success, false on v == NULL, i > v->len, or allocation failure \
 * \
 * @note May allocate heap memory. \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): same as push — restamps only if realloc \
 * moves the buffer. \
 * \
 * Performance: \
 * - Time: O(n) — shifts elements, may realloc \
 * - Space: May allocate up to 2× current cap * sizeof(type) \
 */ \
static inline bool dynvec_##type##_insert( \
    dynvec_##type* v, usize i, type value) { \
    if (!v || i > v->len) return false; \
    if (DYNVEC_UNLIKELY(v->len >= v->cap)) { \
        if (!dynvec_##type##_grow(v, v->len + 1)) return false; \
    } \
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
 * @param v dynvec to remove from (NULL-safe) \
 * @param i Index to remove (must be < v->len) \
 * @param out Pointer to store the removed element \
 * @return true on success, false if v == NULL, out == NULL, or i >= v->len \
 * \
 * Performance: \
 * - Time: O(n) — shifts elements left \
 * - Space: O(1) — no shrinking \
 */ \
static inline bool dynvec_##type##_remove( \
    dynvec_##type* v, usize i, type* out) { \
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
/** \
 * @brief Resets the element count to 0 without freeing the buffer \
 * \
 * @param v dynvec to clear (NULL-safe) \
 * \
 * @post v->len == 0 \
 * @note Buffer contents are not zeroed — only logical state is reset. \
 * Use dynvec_##type##_shrink_to_fit() to also release excess memory. \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT restamp. The buffer is unchanged \
 * and existing borrows to addresses in [0, old_len) still point at valid \
 * memory. The substrate does not catch use-of-removed-element — see the \
 * file docblock. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline void dynvec_##type##_clear(dynvec_##type* v) { \
    if (v) v->len = 0; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Bulk operations \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Appends count elements from src array, growing if needed \
 * \
 * @param v dynvec to extend \
 * @param src Source array (must point to at least count elements) \
 * @param count Number of elements to append \
 * @return true on success, false on v == NULL, src == NULL, or allocation failure \
 * \
 * @note May allocate heap memory. \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): same as push — restamps only if realloc \
 * moves the buffer. \
 * \
 * Performance: \
 * - Time: O(count), plus possible O(n) realloc \
 * - Space: May allocate up to 2× current cap * sizeof(type) \
 */ \
static inline bool dynvec_##type##_extend( \
    dynvec_##type* v, const type* src, usize count) { \
    if (!v || !src) return false; \
    if (count == 0) return true; \
    if (v->len + count > v->cap) { \
        if (!dynvec_##type##_grow(v, v->len + count)) return false; \
    } \
    mem_copy(&v->data[v->len], src, count * sizeof(type)); \
    v->len += count; \
    return true; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Capacity management \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Ensures the buffer has capacity for at least min_cap elements \
 * \
 * Does nothing if current capacity is already sufficient. \
 * \
 * @param v dynvec to reserve into (NULL-safe) \
 * @param min_cap Minimum number of elements the buffer must hold \
 * @return true on success (or if already sufficient), false on allocation failure \
 * \
 * @note May allocate heap memory. \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): if reserve triggers a grow that relocates \
 * the buffer, the lifetime ID is restamped (via grow). If cap is already \
 * sufficient, the lifetime ID is preserved. \
 * \
 * Performance: \
 * - Time: O(1) if cap >= min_cap, else O(n) on realloc \
 * - Space: May allocate \
 */ \
static inline bool dynvec_##type##_reserve( \
    dynvec_##type* v, usize min_cap) { \
    if (!v) return false; \
    if (v->cap >= min_cap) return true; \
    return dynvec_##type##_grow(v, min_cap); \
} \
\
/** \
 * @brief Shrinks the buffer to exactly fit the current element count \
 * \
 * Frees excess capacity. If len == 0, frees the entire buffer. \
 * \
 * @param v dynvec to shrink (NULL-safe) \
 * @return true on success, false on realloc failure (v is unchanged on failure) \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): \
 *   - If len == 0: frees the buffer and RESTAMPS the lifetime ID. The \
 *     container is recycled to an empty state — subsequent operations \
 *     work normally (push will grow from zero). lt.open stays true. \
 *   - If len > 0 and realloc moves the buffer: RESTAMPS. \
 *   - If len > 0 and realloc returns the same address (or cap was already \
 *     equal to len): lifetime ID is preserved. \
 * \
 * Performance: \
 * - Time: O(n) — realloc \
 * - Space: Frees (cap - len) * sizeof(type) bytes \
 */ \
static inline bool dynvec_##type##_shrink_to_fit(dynvec_##type* v) { \
    if (!v || v->len == v->cap) return true; \
    if (v->len == 0) { \
        free(v->data); \
        v->data = NULL; \
        v->cap = 0; \
        dynvec_##type##_lifetime_restamp_(v); \
        return true; \
    } \
    type* old_data = v->data; \
    type* new_data = (type*)realloc(v->data, v->len * sizeof(type)); \
    if (!new_data) return false; \
    v->data = new_data; \
    v->cap = v->len; \
    if (new_data != old_data) { \
        dynvec_##type##_lifetime_restamp_(v); \
    } \
    return true; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Memory management \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Frees the heap buffer and resets all fields to zero \
 * \
 * @param v dynvec to free (NULL-safe) \
 * \
 * @post v->data == NULL, v->len == 0, v->cap == 0 \
 * \
 * ⚠️ MUST be called to avoid memory leaks. \
 * Subsequent use requires reinitializing via dynvec_##type##_init(). \
 * \
 * Lifetime (CANON_LIFETIME_DEBUG): CLOSES the lifetime (lt.open = false). \
 * Any borrow that captured this dynvec's ID will fire require_msg on the \
 * next read. The lifetime is reopened only by a fresh init / with_capacity \
 * call. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: Frees cap * sizeof(type) bytes \
 */ \
static inline void dynvec_##type##_free(dynvec_##type* v) { \
    if (!v) return; \
    free(v->data); \
    v->data = NULL; \
    v->len = 0; \
    v->cap = 0; \
    dynvec_##type##_lifetime_close_(v); \
}

#endif /* CANON_DATA_CONVENIENCE_DYNVEC_H */
