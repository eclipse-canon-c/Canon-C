#ifndef CANON_DATA_CONVENIENCE_DYNVEC_H
#define CANON_DATA_CONVENIENCE_DYNVEC_H

/**
 * @file convenience/dynvec.h
 * @brief Fully automatic dynamic vector with hidden allocation
 *
 * CONVENIENCE LAYER - trades explicitness for ergonomics
 *
 * Differences from data/vec.h:
 * ────────────────────────────
 * - Automatic heap allocation (hidden)
 * - Automatic growth on overflow (implicit)
 * - Owns its own memory (no caller-provided buffers)
 * - Simplified API (fewer choices to make)
 *
 * When to use this:
 * ─────────────────
 * ✓ Rapid prototyping
 * ✓ Collections of unknown/unbounded size
 * ✓ When convenience matters more than determinism
 * ✓ Desktop/server applications with ample memory
 *
 * When to use data/vec.h instead:
 * ────────────────────────────────
 * ✓ Performance-critical code
 * ✓ Embedded systems
 * ✓ Real-time systems
 * ✓ When you need deterministic allocation behavior
 * ✓ When you want explicit control over memory
 *
 * Growth strategy:
 * ────────────────
 * - Initial capacity: 8 elements
 * - Growth factor: 2x (amortized O(1) push)
 * - No automatic shrinking
 * - Can explicitly shrink_to_fit() if needed
 *
 * Memory management:
 * ──────────────────
 * - Always heap-allocated
 * - Must call dynvec_##type##_free() to avoid leaks
 * - No arena support (use data/vec.h for arena-backed)
 *
 * Performance:
 * ────────────
 * - Push: Amortized O(1), worst-case O(n) on realloc
 * - Pop: O(1)
 * - Insert/Remove: O(n)
 * - Access: O(1)
 */

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* ─────────────────────────────────────────────────────────────
   Configuration
   ───────────────────────────────────────────────────────────── */

#ifndef DYNVEC_INITIAL_CAPACITY
#define DYNVEC_INITIAL_CAPACITY 8
#endif

#ifndef DYNVEC_GROWTH_FACTOR
#define DYNVEC_GROWTH_FACTOR 2
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define DYNVEC_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define DYNVEC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define DYNVEC_LIKELY(x)   (x)
    #define DYNVEC_UNLIKELY(x) (x)
#endif

/* ─────────────────────────────────────────────────────────────
   DYNVEC DEFINITION
   ───────────────────────────────────────────────────────────── */

/**
 * DEFINE_DYNVEC(type)
 *
 * Generates a fully-typed dynamic vector with automatic growth:
 *   - dynvec_<type>
 *
 * Requirements:
 * ─────────────
 * - type must be trivially copyable (memcpy-safe)
 *
 * Memory model:
 * ─────────────
 * - Always owns heap-allocated buffer
 * - Grows automatically on overflow
 * - Must call _free() to release memory
 *
 * Generated API:
 * ──────────────
 * Constructors:
 *   - dynvec_##type##_init()           - Empty vector
 *   - dynvec_##type##_with_capacity()  - Pre-allocate capacity
 *
 * Capacity:
 *   - dynvec_##type##_len()
 *   - dynvec_##type##_capacity()
 *   - dynvec_##type##_is_empty()
 *   - dynvec_##type##_reserve()        - Ensure capacity
 *   - dynvec_##type##_shrink_to_fit()  - Free excess capacity
 *
 * Access:
 *   - dynvec_##type##_get()            - Bounds-checked
 *   - dynvec_##type##_get_unchecked()  - No bounds check
 *   - dynvec_##type##_set()            - Bounds-checked write
 *   - dynvec_##type##_data()           - Raw pointer
 *   - dynvec_##type##_first()
 *   - dynvec_##type##_last()
 *
 * Modification:
 *   - dynvec_##type##_push()           - Auto-grows if needed
 *   - dynvec_##type##_pop()            - Returns success/failure
 *   - dynvec_##type##_insert()         - Auto-grows if needed
 *   - dynvec_##type##_remove()
 *   - dynvec_##type##_clear()
 *
 * Bulk:
 *   - dynvec_##type##_extend()         - Append array
 *
 * Memory:
 *   - dynvec_##type##_free()           - Release memory
 */
#define DEFINE_DYNVEC(type) \
\
typedef struct { \
    type*  data;     /* Owned heap buffer */ \
    size_t len;      /* Number of elements */ \
    size_t cap;      /* Allocated capacity */ \
} dynvec_##type; \
\
/* ───────────────────────────────────────────────────────── \
   Internal helpers \
   ───────────────────────────────────────────────────────── */ \
\
static inline bool dynvec_##type##_grow(dynvec_##type* v, size_t min_cap) { \
    assert(v); \
    size_t new_cap = v->cap == 0 ? DYNVEC_INITIAL_CAPACITY : v->cap * DYNVEC_GROWTH_FACTOR; \
    if (new_cap < min_cap) new_cap = min_cap; \
    \
    type* new_data = (type*)realloc(v->data, new_cap * sizeof(type)); \
    if (!new_data) return false; \
    \
    v->data = new_data; \
    v->cap = new_cap; \
    return true; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Constructors \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Creates empty vector \
 * \
 * No allocation until first push. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: sizeof(dynvec_##type) \
 * - Heap: none \
 */ \
static inline dynvec_##type dynvec_##type##_init(void) { \
    dynvec_##type v; \
    v.data = NULL; \
    v.len = 0; \
    v.cap = 0; \
    return v; \
} \
\
/** \
 * @brief Creates vector with pre-allocated capacity \
 * \
 * Useful when expected size is known. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: sizeof(dynvec_##type) + capacity*sizeof(type) \
 * - Heap: capacity*sizeof(type) \
 */ \
static inline dynvec_##type dynvec_##type##_with_capacity(size_t capacity) { \
    dynvec_##type v = dynvec_##type##_init(); \
    if (capacity > 0) { \
        v.data = (type*)malloc(capacity * sizeof(type)); \
        if (v.data) v.cap = capacity; \
    } \
    return v; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Capacity queries \
   ───────────────────────────────────────────────────────── */ \
\
static inline size_t dynvec_##type##_len(const dynvec_##type* v) { \
    return v ? v->len : 0; \
} \
\
static inline size_t dynvec_##type##_capacity(const dynvec_##type* v) { \
    return v ? v->cap : 0; \
} \
\
static inline bool dynvec_##type##_is_empty(const dynvec_##type* v) { \
    return !v || v->len == 0; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Element access \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Bounds-checked read \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool dynvec_##type##_get( \
    const dynvec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->data[i]; \
    return true; \
} \
\
/** \
 * @brief Unchecked read (zero overhead) \
 * \
 * ⚠️ Undefined behavior if i >= len \
 */ \
static inline type dynvec_##type##_get_unchecked( \
    const dynvec_##type* v, size_t i) { \
    assert(v && i < v->len); \
    return v->data[i]; \
} \
\
/** \
 * @brief Bounds-checked write \
 */ \
static inline bool dynvec_##type##_set( \
    dynvec_##type* v, size_t i, type value) { \
    if (!v || i >= v->len) return false; \
    v->data[i] = value; \
    return true; \
} \
\
static inline type* dynvec_##type##_data(const dynvec_##type* v) { \
    return v ? v->data : NULL; \
} \
\
static inline type* dynvec_##type##_first(const dynvec_##type* v) { \
    return (v && v->len > 0) ? &v->data[0] : NULL; \
} \
\
static inline type* dynvec_##type##_last(const dynvec_##type* v) { \
    return (v && v->len > 0) ? &v->data[v->len - 1] : NULL; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Modification (auto-growing) \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Appends element, growing if needed \
 * \
 * ⚠️ May allocate memory! \
 * \
 * Performance: \
 * - Amortized time: O(1) \
 * - Worst-case: O(n) on realloc \
 * - Space: May allocate cap*2*sizeof(type) \
 */ \
static inline bool dynvec_##type##_push( \
    dynvec_##type* v, type value) { \
    if (!v) return false; \
    \
    if (DYNVEC_UNLIKELY(v->len >= v->cap)) { \
        if (!dynvec_##type##_grow(v, v->len + 1)) { \
            return false; \
        } \
    } \
    \
    v->data[v->len++] = value; \
    return true; \
} \
\
/** \
 * @brief Removes last element \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) (no shrinking) \
 */ \
static inline bool dynvec_##type##_pop( \
    dynvec_##type* v, type* out) { \
    if (!v || !out || v->len == 0) return false; \
    *out = v->data[--v->len]; \
    return true; \
} \
\
/** \
 * @brief Inserts element at index, growing if needed \
 * \
 * ⚠️ May allocate memory! \
 * \
 * Performance: \
 * - Time: O(n) \
 * - Space: May allocate on grow \
 */ \
static inline bool dynvec_##type##_insert( \
    dynvec_##type* v, size_t i, type value) { \
    if (!v || i > v->len) return false; \
    \
    if (DYNVEC_UNLIKELY(v->len >= v->cap)) { \
        if (!dynvec_##type##_grow(v, v->len + 1)) { \
            return false; \
        } \
    } \
    \
    if (i < v->len) { \
        memmove(&v->data[i + 1], &v->data[i], \
                (v->len - i) * sizeof(type)); \
    } \
    v->data[i] = value; \
    v->len++; \
    return true; \
} \
\
/** \
 * @brief Removes element at index \
 * \
 * Performance: \
 * - Time: O(n) \
 * - Space: O(1) \
 */ \
static inline bool dynvec_##type##_remove( \
    dynvec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->data[i]; \
    if (i < v->len - 1) { \
        memmove(&v->data[i], &v->data[i + 1], \
                (v->len - i - 1) * sizeof(type)); \
    } \
    v->len--; \
    return true; \
} \
\
/** \
 * @brief Clears all elements \
 * \
 * Does not free memory. Use shrink_to_fit() if needed. \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline void dynvec_##type##_clear(dynvec_##type* v) { \
    if (v) v->len = 0; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Bulk operations \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Appends array of elements, growing if needed \
 * \
 * ⚠️ May allocate memory! \
 * \
 * Performance: \
 * - Time: O(count) + possible O(n) realloc \
 * - Space: May allocate on grow \
 */ \
static inline bool dynvec_##type##_extend( \
    dynvec_##type* v, const type* src, size_t count) { \
    if (!v || !src) return false; \
    \
    if (v->len + count > v->cap) { \
        if (!dynvec_##type##_grow(v, v->len + count)) { \
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
   Capacity management \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Ensures vector has at least min_cap capacity \
 * \
 * ⚠️ May allocate memory! \
 * \
 * Performance: \
 * - Time: O(1) if cap >= min_cap, else O(n) \
 * - Space: May allocate \
 */ \
static inline bool dynvec_##type##_reserve( \
    dynvec_##type* v, size_t min_cap) { \
    if (!v) return false; \
    if (v->cap >= min_cap) return true; \
    return dynvec_##type##_grow(v, min_cap); \
} \
\
/** \
 * @brief Shrinks capacity to fit current length \
 * \
 * Useful to free excess memory. \
 * \
 * Performance: \
 * - Time: O(n) \
 * - Space: Frees (cap - len) * sizeof(type) \
 */ \
static inline bool dynvec_##type##_shrink_to_fit(dynvec_##type* v) { \
    if (!v || v->len == v->cap) return true; \
    if (v->len == 0) { \
        free(v->data); \
        v->data = NULL; \
        v->cap = 0; \
        return true; \
    } \
    type* new_data = (type*)realloc(v->data, v->len * sizeof(type)); \
    if (!new_data) return false; \
    v->data = new_data; \
    v->cap = v->len; \
    return true; \
} \
\
/* ───────────────────────────────────────────────────────── \
   Memory management \
   ───────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Frees all memory \
 * \
 * ⚠️ MUST be called to avoid memory leaks! \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: Frees cap * sizeof(type) \
 */ \
static inline void dynvec_##type##_free(dynvec_##type* v) { \
    if (!v) return; \
    free(v->data); \
    v->data = NULL; \
    v->len = 0; \
    v->cap = 0; \
}

#endif /* CANON_DATA_CONVENIENCE_DYNVEC_H */
