#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "core/memory.h"
#include "core/arena.h"
#include "core/scope.h"
#include "data/range.h"
#include "data/stringbuf.h"
#include "semantics/result.h"
#include "semantics/error.h"
#include "semantics/option.h"

/**
 * @file vec.h
 * @brief Bounded dynamic vectors with explicit caller-owned buffer
 *
 * Canon-C vector is a **bounded**, **type-safe**, **explicit ownership** container.
 *
 * Supports:
 * - Stack, heap, or arena-backed buffers
 * - Typed vectors via macro
 * - Generic `void*` vector
 * - Iterators, slices, and range integration
 * - Optional writing to `stringbuf`
 * - Optional safe access via `Option<T>`
 *
 * Design Principles:
 * - Caller owns the buffer (stack, heap, arena, static)
 * - Fixed capacity (no automatic growth)
 * - Bounds-checked operations returning Result<T, Error>
 * - Deterministic memory and performance
 *
 * Performance & Memory:
 * - Push/Pop: O(1), memory: +sizeof(T) per element
 * - Insert/Remove: O(n), memory: no extra allocations
 * - Extend: O(k), memory: contiguous buffer usage for k elements
 * - Iterators: O(1) per step
 * - Slice/Subvector: O(1), no copy
 * - Generic `void*` vector: +sizeof(void*) per element
 * - Typed vectors: +sizeof(T) per element
 * - Heap allocation: +capacity*sizeof(T)
 * - Arena allocation: +capacity*sizeof(T) via arena
 *
 * Portability:
 * - Requires C99 or later (inline functions, stdbool, compound literals)
 * - Optional C11 for _Static_assert
 * - All core functionality works in strict C99
 *
 * Thread-safety:
 * - Each vector instance is independent - no shared state
 * - Concurrent reads safe if no thread modifying
 * - Concurrent modifications require external synchronization
 * - Iterator invalidation: modifications invalidate all iterators
 *
 * Note on capacity:
 * - vec.h is FIXED CAPACITY - no automatic growth
 * - For auto-growing vectors, use data/convenience/dynvec.h
 * - For inline-first with one spill, use data/convenience/smallvec.h
 */

/* ────────────────────────────────────────────────────────────────
   Configuration & Feature Detection
   ──────────────────────────────────────────────────────────────── */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define VEC_HAS_C11 1
    #define VEC_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define VEC_HAS_C11 0
    #define VEC_STATIC_ASSERT(cond, msg) /* C99: no static assert */
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define VEC_LIKELY(x) __builtin_expect(!!(x), 1)
    #define VEC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define VEC_LIKELY(x) (x)
    #define VEC_UNLIKELY(x) (x)
#endif

/* ────────────────────────────────────────────────────────────────
   Result type for vector operations
   ──────────────────────────────────────────────────────────────── */

CANON_C_DEFINE_RESULT(bool, Error)

/* ────────────────────────────────────────────────────────────────
   Generic void* vector
   ──────────────────────────────────────────────────────────────── */

/**
 * @brief Generic void* vector - dynamically sized array with fixed capacity
 *
 * Stores pointers to any type. Useful for heterogeneous collections.
 *
 * Fields:
 * - items: Caller-owned buffer of void* pointers
 * - len: Current number of valid elements (0 <= len <= capacity)
 * - capacity: Maximum elements the buffer can hold
 *
 * Memory layout:
 * - sizeof(vec_voidptr) = sizeof(void**) + 2*sizeof(size_t)
 * - Total memory: sizeof(vec_voidptr) + capacity*sizeof(void*)
 *
 * ⚠️ Do not modify fields directly - use provided functions.
 */
typedef struct {
    void** items;      ///< Caller-owned buffer
    size_t len;        ///< Current number of elements
    size_t capacity;   ///< Maximum elements
} vec_voidptr;

/**
 * @brief Initializes vector with caller-owned buffer
 *
 * @param buffer Caller-owned array of void* (NULL allowed if capacity==0)
 * @param capacity Maximum number of elements
 * @return Initialized vector with len=0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline vec_voidptr vec_voidptr_init(void** buffer, size_t capacity) {
    assert(buffer != NULL || capacity == 0);
    return (vec_voidptr){ .items = buffer, .len = 0, .capacity = capacity };
}

/**
 * @brief Creates empty vector
 *
 * @return Empty vector with NULL buffer, len=0, capacity=0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline vec_voidptr vec_voidptr_empty(void) {
    return (vec_voidptr){0};
}

/**
 * @brief Checks if vector is empty
 *
 * @param v Vector to check (NULL-safe)
 * @return true if empty
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_is_empty(const vec_voidptr* v) {
    return !v || v->len == 0;
}

/**
 * @brief Checks if vector is at capacity
 *
 * @param v Vector to check (NULL-safe)
 * @return true if full
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_is_full(const vec_voidptr* v) {
    return v && v->len >= v->capacity;
}

/**
 * @brief Returns current length
 *
 * @param v Vector to query (NULL-safe)
 * @return Number of elements
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline size_t vec_voidptr_len(const vec_voidptr* v) {
    return v ? v->len : 0;
}

/**
 * @brief Returns capacity
 *
 * @param v Vector to query (NULL-safe)
 * @return Maximum elements
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline size_t vec_voidptr_capacity(const vec_voidptr* v) {
    return v ? v->capacity : 0;
}

/**
 * @brief Returns remaining space
 *
 * @param v Vector to query (NULL-safe)
 * @return capacity - len
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline size_t vec_voidptr_remaining(const vec_voidptr* v) {
    return v ? (v->capacity - v->len) : 0;
}

/**
 * @brief Safely retrieves element at index
 *
 * @param v Vector to access
 * @param i Index to retrieve
 * @param out Pointer to store result
 * @return true if successful, false if out of bounds
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_get(const vec_voidptr* v, size_t i, void** out) {
    if (!v || !out || i >= v->len) return false;
    *out = v->items[i];
    return true;
}

/**
 * @brief Retrieves element as Option
 *
 * @param v Vector to access
 * @param i Index to retrieve
 * @return Some(element) or None
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline option_voidptr vec_voidptr_get_option(const vec_voidptr* v, size_t i) {
    if (!v || i >= v->len) return option_voidptr_none();
    return option_voidptr_some(v->items[i]);
}

/**
 * @brief Retrieves element without bounds checking
 *
 * ⚠️ WARNING: Undefined behavior if i >= len!
 *
 * @param v Vector to access
 * @param i Index to retrieve
 * @return Element at index i
 *
 * Panics: If i >= len (debug builds)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* vec_voidptr_get_unchecked(const vec_voidptr* v, size_t i) {
    assert(v && v->items && i < v->len);
    return v->items[i];
}

/**
 * @brief Appends element
 *
 * @param v Vector to push to
 * @param item Item to append
 * @return Ok(true) on success, Err on failure
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline result_bool_Error vec_voidptr_push(vec_voidptr* v, void* item) {
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG);
    if (v->len >= v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED);
    v->items[v->len++] = item;
    return result_bool_Error_ok(true);
}

/**
 * @brief Appends element without bounds checking
 *
 * ⚠️ WARNING: Undefined behavior if full!
 *
 * @param v Vector to push to
 * @param item Item to append
 *
 * Panics: If len >= capacity (debug builds)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void vec_voidptr_push_unchecked(vec_voidptr* v, void* item) {
    assert(v && v->items && v->len < v->capacity);
    v->items[v->len++] = item;
}

/**
 * @brief Tries to append element
 *
 * @param v Vector to push to
 * @param item Item to append
 * @return true on success, false on failure
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_try_push(vec_voidptr* v, void* item) {
    if (!v || !v->items || v->len >= v->capacity) return false;
    v->items[v->len++] = item;
    return true;
}

/**
 * @brief Removes last element
 *
 * @param v Vector to pop from
 * @param out Pointer to store result
 * @return Ok(true) on success, Err on failure
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline result_bool_Error vec_voidptr_pop(vec_voidptr* v, void** out) {
    if (!v || !out || !v->items) return result_bool_Error_err(ERR_INVALID_ARG);
    if (v->len == 0) return result_bool_Error_err(ERR_INVALID_STATE);
    *out = v->items[--v->len];
    return result_bool_Error_ok(true);
}

/**
 * @brief Removes last element as Option
 *
 * @param v Vector to pop from
 * @return Some(element) or None
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline option_voidptr vec_voidptr_pop_option(vec_voidptr* v) {
    void* out;
    if (result_bool_Error_is_ok(vec_voidptr_pop(v, &out))) {
        return option_voidptr_some(out);
    }
    return option_voidptr_none();
}

/**
 * @brief Clears all elements
 *
 * @param v Vector to clear (NULL-safe)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void vec_voidptr_clear(vec_voidptr* v) {
    if (v) v->len = 0;
}

/**
 * @brief Returns pointer to first element
 *
 * @param v Vector to access (NULL-safe)
 * @return Pointer to first or NULL
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void** vec_voidptr_first(const vec_voidptr* v) {
    return (v && v->len > 0) ? &v->items[0] : NULL;
}

/**
 * @brief Returns pointer to last element
 *
 * @param v Vector to access (NULL-safe)
 * @return Pointer to last or NULL
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void** vec_voidptr_last(const vec_voidptr* v) {
    return (v && v->len > 0) ? &v->items[v->len - 1] : NULL;
}

/**
 * @brief Returns raw data pointer
 *
 * @param v Vector to access (NULL-safe)
 * @return Buffer pointer or NULL
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void** vec_voidptr_data(const vec_voidptr* v) {
    return v ? v->items : NULL;
}

/**
 * @brief Swaps two vectors
 *
 * @param a First vector
 * @param b Second vector
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void vec_voidptr_swap(vec_voidptr* a, vec_voidptr* b) {
    assert(a && b);
    vec_voidptr tmp = *a;
    *a = *b;
    *b = tmp;
}

/**
 * @brief Appends array of elements
 *
 * @param v Vector to extend
 * @param src Source array
 * @param count Number of elements
 * @return Ok(true) on success, Err on failure
 *
 * Performance:
 * - Time: O(count)
 * - Space: O(1)
 */
static inline result_bool_Error vec_voidptr_append_array(
    vec_voidptr* v, const void* const* src, size_t count) {
    if (!v || !v->items || !src) return result_bool_Error_err(ERR_INVALID_ARG);
    if (v->len + count > v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED);
    memcpy(&v->items[v->len], src, count * sizeof(void*));
    v->len += count;
    return result_bool_Error_ok(true);
}

/* Heap allocation */
/**
 * @brief Allocates vector on heap with fixed capacity
 *
 * ⚠️ Capacity is fixed - no automatic growth
 * ⚠️ For auto-growing vectors, use data/convenience/dynvec.h
 *
 * @param capacity Maximum elements
 * @return Initialized vector or empty on failure
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(capacity)
 */
static inline vec_voidptr vec_voidptr_alloc(size_t capacity) {
    if (capacity == 0) return vec_voidptr_empty();
    void** buf = (void**)malloc(capacity * sizeof(void*));
    if (!buf) return vec_voidptr_empty();
    return vec_voidptr_init(buf, capacity);
}

/**
 * @brief Frees heap-allocated vector
 *
 * ⚠️ Only for heap-allocated vectors!
 * ⚠️ Does not free pointed-to objects
 *
 * @param v Vector to free (NULL-safe)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void vec_voidptr_free(vec_voidptr* v) {
    if (v && v->items) {
        free(v->items);
        v->items = NULL;
        v->len = 0;
        v->capacity = 0;
    }
}

/* Arena allocation */
/**
 * @brief Allocates vector from arena with fixed capacity
 *
 * ⚠️ Capacity is fixed - no growth possible with arena
 *
 * @param arena Arena to allocate from
 * @param capacity Maximum elements
 * @return Initialized vector or empty on failure
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(capacity)
 */
static inline vec_voidptr vec_voidptr_arena_alloc(Arena* arena, size_t capacity) {
    if (!arena || capacity == 0) return vec_voidptr_empty();
    void** buf = arena_alloc_array(arena, void*, capacity);
    if (!buf) return vec_voidptr_empty();
    return vec_voidptr_init(buf, capacity);
}

/* Iterators */
/**
 * @brief Forward iterator
 *
 * ⚠️ Invalidated by vector modifications
 */
typedef struct {
    vec_voidptr* vec;   ///< Vector being iterated
    size_t index;       ///< Current position
} vec_voidptr_iter;

/**
 * @brief Creates iterator at start
 *
 * @param v Vector to iterate
 * @return Iterator at position 0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline vec_voidptr_iter vec_voidptr_iter_init(vec_voidptr* v) {
    return (vec_voidptr_iter){ .vec = v, .index = 0 };
}

/**
 * @brief Advances iterator
 *
 * @param it Iterator to advance
 * @param out Pointer to store element
 * @return true if element retrieved, false if end
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_iter_next(vec_voidptr_iter* it, void** out) {
    if (!it || !it->vec || !out) return false;
    if (it->index >= it->vec->len) return false;
    *out = it->vec->items[it->index++];
    return true;
}

/* Slice */
/**
 * @brief Zero-copy slice view
 *
 * ⚠️ Invalidated by vector modifications
 */
typedef struct {
    void** items;   ///< Pointer into parent vector
    size_t len;     ///< Number of elements
} vec_voidptr_slice;

/**
 * @brief Creates slice from [start, end)
 *
 * @param v Vector to slice
 * @param start Start index (inclusive)
 * @param end End index (exclusive)
 * @return Slice or empty on invalid range
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline vec_voidptr_slice vec_voidptr_slice_init(
    vec_voidptr* v, size_t start, size_t end) {
    vec_voidptr_slice s = {0};
    if (!v || start > end || end > v->len) return s;
    s.items = &v->items[start];
    s.len = end - start;
    return s;
}

/**
 * @brief Accesses slice element
 *
 * ⚠️ WARNING: Undefined behavior if i >= len!
 *
 * @param s Slice to access
 * @param i Index within slice
 * @return Pointer to element
 *
 * Panics: If i >= len (debug builds)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void** vec_voidptr_slice_get(const vec_voidptr_slice* s, size_t i) {
    assert(s && i < s->len);
    return &s->items[i];
}

/* ────────────────────────────────────────────────────────────────
   Typed vector macro
   ──────────────────────────────────────────────────────────────── */

/**
 * @brief Defines typed vector for given element type
 *
 * Generates complete vector implementation including struct and all functions.
 *
 * Generated type: vec_##type
 *
 * Generated functions:
 * - Constructors: vec_##type##_init, vec_##type##_empty, vec_##type##_alloc, vec_##type##_arena_alloc
 * - Queries: vec_##type##_len, vec_##type##_capacity, vec_##type##_is_empty, vec_##type##_is_full, vec_##type##_remaining
 * - Access: vec_##type##_get, vec_##type##_get_option, vec_##type##_get_unchecked, vec_##type##_at, vec_##type##_set
 * - Modification: vec_##type##_push, vec_##type##_try_push, vec_##type##_push_unchecked, vec_##type##_pop, vec_##type##_pop_option, vec_##type##_clear
 * - Insertion/Removal: vec_##type##_insert, vec_##type##_remove, vec_##type##_remove_option
 * - Bulk: vec_##type##_append_array, vec_##type##_extend, vec_##type##_fill
 * - Pointers: vec_##type##_first, vec_##type##_last, vec_##type##_data
 * - Utility: vec_##type##_swap, vec_##type##_free
 * - Iteration: vec_##type##_iter_init, vec_##type##_iter_next
 * - Slicing: vec_##type##_slice_init, vec_##type##_slice_get
 * - Range: vec_##type##_extend_from_range
 * - Display: vec_##type##_to_stringbuf, vec_##type##_to_stringbuf_cb
 *
 * Type name convention:
 * For pointer types, use typedef first:
 * typedef const char* constcharptr;
 * DEFINE_VEC(constcharptr)
 *
 * Usage:
 * DEFINE_VEC(int) // vec_int
 * DEFINE_VEC(float) // vec_float
 *
 * Note: Use at file or global scope, not inside functions.
 *
 * @param type Element type for vector
 */
#define DEFINE_VEC(type) \
\
typedef struct { \
    type* items; \
    size_t len; \
    size_t capacity; \
} vec_##type; \
\
VEC_STATIC_ASSERT(sizeof(type) > 0, "vec element type must have non-zero size"); \
\
static inline vec_##type vec_##type##_init(type* buffer, size_t capacity) { \
    assert(buffer || capacity == 0); \
    return (vec_##type){buffer, 0, capacity}; \
} \
\
static inline vec_##type vec_##type##_empty(void) { \
    return (vec_##type){0}; \
} \
\
static inline bool vec_##type##_is_empty(const vec_##type* v) { \
    return !v || v->len == 0; \
} \
\
static inline bool vec_##type##_is_full(const vec_##type* v) { \
    return v && v->len >= v->capacity; \
} \
\
static inline size_t vec_##type##_len(const vec_##type* v) { \
    return v ? v->len : 0; \
} \
\
static inline size_t vec_##type##_capacity(const vec_##type* v) { \
    return v ? v->capacity : 0; \
} \
\
static inline size_t vec_##type##_remaining(const vec_##type* v) { \
    return v ? (v->capacity - v->len) : 0; \
} \
\
static inline bool vec_##type##_get(const vec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->items[i]; \
    return true; \
} \
\
static inline option_##type vec_##type##_get_option(const vec_##type* v, size_t i) { \
    if (!v || i >= v->len) return option_##type##_none(); \
    return option_##type##_some(v->items[i]); \
} \
\
static inline type vec_##type##_get_unchecked(const vec_##type* v, size_t i) { \
    assert(v && v->items && i < v->len); \
    return v->items[i]; \
} \
\
static inline type* vec_##type##_at(const vec_##type* v, size_t i) { \
    assert(v && i < v->len); \
    return &v->items[i]; \
} \
\
static inline bool vec_##type##_set(vec_##type* v, size_t i, type val) { \
    if (!v || i >= v->len) return false; \
    v->items[i] = val; \
    return true; \
} \
\
static inline result_bool_Error vec_##type##_push(vec_##type* v, type item) { \
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len >= v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    v->items[v->len++] = item; \
    return result_bool_Error_ok(true); \
} \
\
static inline bool vec_##type##_try_push(vec_##type* v, type item) { \
    if (!v || !v->items || v->len >= v->capacity) return false; \
    v->items[v->len++] = item; \
    return true; \
} \
\
static inline void vec_##type##_push_unchecked(vec_##type* v, type item) { \
    assert(v && v->items && v->len < v->capacity); \
    v->items[v->len++] = item; \
} \
\
static inline result_bool_Error vec_##type##_pop(vec_##type* v, type* out) { \
    if (!v || !out || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len == 0) return result_bool_Error_err(ERR_INVALID_STATE); \
    *out = v->items[--v->len]; \
    return result_bool_Error_ok(true); \
} \
\
static inline option_##type vec_##type##_pop_option(vec_##type* v) { \
    type out; \
    if (result_bool_Error_is_ok(vec_##type##_pop(v, &out))) \
        return option_##type##_some(out); \
    return option_##type##_none(); \
} \
\
static inline void vec_##type##_clear(vec_##type* v) { \
    if (v) v->len = 0; \
} \
\
static inline type* vec_##type##_first(const vec_##type* v) { \
    return (v && v->len > 0) ? &v->items[0] : NULL; \
} \
\
static inline type* vec_##type##_last(const vec_##type* v) { \
    return (v && v->len > 0) ? &v->items[v->len - 1] : NULL; \
} \
\
static inline type* vec_##type##_data(const vec_##type* v) { \
    return v ? v->items : NULL; \
} \
\
static inline result_bool_Error vec_##type##_insert(vec_##type* v, size_t i, type item) { \
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (i > v->len) return result_bool_Error_err(ERR_OUT_OF_RANGE); \
    if (v->len >= v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    if (i < v->len) { \
        memmove(&v->items[i + 1], &v->items[i], (v->len - i) * sizeof(type)); \
    } \
    v->items[i] = item; \
    v->len++; \
    return result_bool_Error_ok(true); \
} \
\
static inline result_bool_Error vec_##type##_remove(vec_##type* v, size_t i, type* out) { \
    if (!v || !v->items || !out) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len == 0) return result_bool_Error_err(ERR_INVALID_STATE); \
    if (i >= v->len) return result_bool_Error_err(ERR_OUT_OF_RANGE); \
    *out = v->items[i]; \
    if (i < v->len - 1) { \
        memmove(&v->items[i], &v->items[i + 1], (v->len - i - 1) * sizeof(type)); \
    } \
    v->len--; \
    return result_bool_Error_ok(true); \
} \
\
static inline option_##type vec_##type##_remove_option(vec_##type* v, size_t i) { \
    type out; \
    if (result_bool_Error_is_ok(vec_##type##_remove(v, i, &out))) \
        return option_##type##_some(out); \
    return option_##type##_none(); \
} \
\
static inline result_bool_Error vec_##type##_append_array( \
    vec_##type* v, const type* src, size_t count) { \
    if (!v || !v->items || !src) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len + count > v->capacity) \
        return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    memcpy(&v->items[v->len], src, count * sizeof(type)); \
    v->len += count; \
    return result_bool_Error_ok(true); \
} \
\
static inline result_bool_Error vec_##type##_extend( \
    vec_##type* v, const type* src, size_t count) { \
    return vec_##type##_append_array(v, src, count); \
} \
\
static inline void vec_##type##_fill(vec_##type* v, type value, size_t count) { \
    if (!v || !v->items) return; \
    size_t to_fill = (count < v->capacity - v->len) ? count : (v->capacity - v->len); \
    for (size_t i = 0; i < to_fill; i++) { \
        v->items[v->len + i] = value; \
    } \
    v->len += to_fill; \
} \
\
static inline vec_##type vec_##type##_alloc(size_t capacity) { \
    if (capacity == 0) return vec_##type##_empty(); \
    type* buf = (type*)malloc(capacity * sizeof(type)); \
    if (!buf) return vec_##type##_empty(); \
    return vec_##type##_init(buf, capacity); \
} \
\
static inline void vec_##type##_free(vec_##type* v) { \
    if (v && v->items) { \
        free(v->items); \
        v->items = NULL; \
        v->len = 0; \
        v->capacity = 0; \
    } \
} \
\
static inline vec_##type vec_##type##_arena_alloc(Arena* arena, size_t capacity) { \
    if (!arena || capacity == 0) return vec_##type##_empty(); \
    type* buf = arena_alloc_array(arena, type, capacity); \
    if (!buf) return vec_##type##_empty(); \
    return vec_##type##_init(buf, capacity); \
} \
\
static inline void vec_##type##_swap(vec_##type* a, vec_##type* b) { \
    assert(a && b); \
    vec_##type tmp = *a; \
    *a = *b; \
    *b = tmp; \
} \
\
typedef struct { \
    vec_##type* vec; \
    size_t index; \
} vec_##type##_iter; \
\
static inline vec_##type##_iter vec_##type##_iter_init(vec_##type* v) { \
    return (vec_##type##_iter){.vec = v, .index = 0}; \
} \
\
static inline bool vec_##type##_iter_next(vec_##type##_iter* it, type* out) { \
    if (!it || !it->vec || !out) return false; \
    if (it->index >= it->vec->len) return false; \
    *out = it->vec->items[it->index++]; \
    return true; \
} \
\
typedef struct { \
    type* items; \
    size_t len; \
} vec_##type##_slice; \
\
static inline vec_##type##_slice vec_##type##_slice_init( \
    vec_##type* v, size_t start, size_t end) { \
    vec_##type##_slice s = {0}; \
    if (!v || start > end || end > v->len) return s; \
    s.items = &v->items[start]; \
    s.len = end - start; \
    return s; \
} \
\
static inline type* vec_##type##_slice_get(const vec_##type##_slice* s, size_t i) { \
    assert(s && i < s->len); \
    return &s->items[i]; \
} \
\
static inline result_bool_Error vec_##type##_extend_from_range( \
    vec_##type* v, IntRange r) { \
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    size_t step = (r.end >= r.start) ? 1 : (size_t)-1; \
    size_t count = (r.end >= r.start) ? (r.end - r.start) : (r.start - r.end); \
    if (v->len + count > v->capacity) \
        return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    for (size_t i = 0; i < count; i++) { \
        v->items[v->len + i] = (type)(r.start + i * step); \
    } \
    v->len += count; \
    return result_bool_Error_ok(true); \
} \
\
static inline void vec_##type##_to_stringbuf( \
    const vec_##type* v, StringBuf* sb, const char* fmt) { \
    if (!v || !sb || !fmt) return; \
    for (size_t i = 0; i < v->len; i++) { \
        stringbuf_printf(sb, fmt, v->items[i]); \
    } \
} \
\
static inline void vec_##type##_to_stringbuf_cb( \
    const vec_##type* v, StringBuf* sb, void(*cb)(StringBuf*, type)) { \
    if (!v || !sb || !cb) return; \
    for (size_t i = 0; i < v->len; i++) { \
        cb(sb, v->items[i]); \
    } \
}

#endif /* CANON_DATA_VEC_H */
