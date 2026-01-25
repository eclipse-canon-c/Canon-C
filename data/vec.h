// data/vec.h
#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include "core/memory.h"
#include "core/arena.h"
#include "semantics/result.h"

/**
 * @file vec.h
 * @brief Fixed-capacity dynamic vector with **explicit caller-owned buffer** and optional heap/allocation support
 *
 * Canon-C vector is a **bounded**, **type-safe**, **explicit ownership** container.
 * It provides predictable memory behavior, composable error handling, and explicit lifetime control.
 *
 * Features:
 *  - Stack, heap, or arena-backed buffer
 *  - Capacity fixed at initialization
 *  - All operations bounds-checked, returning Result for error handling
 *  - Optional explicit heap allocation via vec_alloc / vec_free
 *  - Iterators for safe, intention-revealing loops
 *  - Slice/subvector support for non-copy views
 *
 * Portability: Requires C99 or later
 * Thread-safety: Each vector instance independent; caller must synchronize for concurrent access
 *
 * Performance & Memory:
 *  - Push/Pop: O(1)
 *  - Insert/Remove: O(n)
 *  - Extend: O(k) where k = number of elements being copied
 *  - Memory: contiguous buffer, no hidden allocations
 *
 * Key Design Principles:
 *  - Caller owns the buffer (stack, heap, arena, or static)
 *  - Capacity fixed at initialization (no automatic growth)
 *  - All operations bounds-checked by default
 *  - Push/pop/insert operations return Result → composable error handling
 *  - No hidden state, no global variables, fully deterministic
 *
 * Use Canon-C vector when:
 *  ✓ Predictable memory behavior
 *  ✓ Explicit ownership semantics
 *  ✓ Deterministic performance
 *  ✓ Error handling via Result
 *
 * Do NOT use when:
 *  ✗ Automatic resizing or amortized growth is required
 *  ✗ You want implicit memory management
 *
 * Typical patterns:
 *
 *   // 1. Stack allocated - zero heap allocation
 *   int numbers_buf[512];
 *   vec_int numbers = vec_int_init(numbers_buf, 512);
 *
 *   // 2. Arena allocated - explicit lifetime
 *   int* arena_buf = arena_alloc_array(&arena, int, 1024);
 *   vec_int large = vec_int_init(arena_buf, 1024);
 *
 *   // 3. Heap allocated - explicit ownership
 *   vec_int heap_vec = vec_int_alloc(1000);
 *   vec_int_free(&heap_vec);
 *
 *   // 4. Check capacity before bulk operations
 *   if (vec_int_remaining(&v) < items_to_add) {
 *       return result_bool_constcharptr_err("not enough capacity");
 *   }
 */

/* ────────────────────────────────────────────────────────────────
   Result type for vector operations
   ──────────────────────────────────────────────────────────────── */
typedef const char* constcharptr;
CANON_C_DEFINE_RESULT(bool, constcharptr)

/* ────────────────────────────────────────────────────────────────
   Generic void* vector (internal)
   ──────────────────────────────────────────────────────────────── */
typedef struct {
    void** items;   ///< Caller-owned buffer
    size_t len;     ///< Current number of elements
    size_t capacity;///< Maximum number of elements
} vec_voidptr;

static inline vec_voidptr vec_voidptr_init(void** buffer, size_t capacity) {
    assert(buffer != NULL || capacity == 0);
    assert(capacity <= SIZE_MAX / sizeof(void*));
    return (vec_voidptr){ .items = buffer, .len = 0, .capacity = capacity };
}

static inline vec_voidptr vec_voidptr_empty(void) { return (vec_voidptr){0}; }

static inline bool vec_voidptr_is_empty(const vec_voidptr* v) { return !v || v->len == 0; }
static inline bool vec_voidptr_is_full(const vec_voidptr* v) { return v && v->len >= v->capacity; }
static inline size_t vec_voidptr_len(const vec_voidptr* v) { return v ? v->len : 0; }
static inline size_t vec_voidptr_capacity(const vec_voidptr* v) { return v ? v->capacity : 0; }
static inline size_t vec_voidptr_remaining(const vec_voidptr* v) { return v ? (v->capacity - v->len) : 0; }

static inline bool vec_voidptr_get(const vec_voidptr* v, size_t i, void** out) {
    if (!v || !out || i >= v->len) return false;
    *out = v->items[i];
    return true;
}

static inline void* vec_voidptr_get_unchecked(const vec_voidptr* v, size_t i) {
    assert(v && v->items && i < v->len);
    return v->items[i];
}

static inline result_bool_constcharptr vec_voidptr_push(vec_voidptr* v, void* item) {
    if (!v || !v->items) return result_bool_constcharptr_err("vec_push: null vector or buffer");
    if (v->len >= v->capacity) return result_bool_constcharptr_err("vec_push: capacity exceeded");
    v->items[v->len++] = item;
    return result_bool_constcharptr_ok(true);
}

static inline result_bool_constcharptr vec_voidptr_pop(vec_voidptr* v, void** out) {
    if (!v || !out || !v->items) return result_bool_constcharptr_err("vec_pop: null vector or buffer");
    if (v->len == 0) return result_bool_constcharptr_err("vec_pop: empty vector");
    *out = v->items[--v->len];
    return result_bool_constcharptr_ok(true);
}

static inline void vec_voidptr_clear(vec_voidptr* v) { if (v) v->len = 0; }
static inline void** vec_voidptr_first(const vec_voidptr* v) { return (v && v->len > 0) ? &v->items[0] : NULL; }
static inline void** vec_voidptr_last(const vec_voidptr* v) { return (v && v->len > 0) ? &v->items[v->len - 1] : NULL; }

/* ────────────────────────────────────────────────────────────────
   Typed vector macro
   ──────────────────────────────────────────────────────────────── */
#define DEFINE_VEC(type) \
typedef struct { \
    type* items; \
    size_t len; \
    size_t capacity; \
} vec_##type; \
\
/** Initialize vector with caller-provided buffer */ \
static inline vec_##type vec_##type##_init(type* buffer, size_t capacity) { \
    assert(buffer != NULL || capacity == 0); \
    assert(capacity <= SIZE_MAX / sizeof(type)); \
    return (vec_##type){ .items = buffer, .len = 0, .capacity = capacity }; \
} \
\
/** Create empty vector (no storage) */ \
static inline vec_##type vec_##type##_empty(void) { return (vec_##type){0}; } \
\
/* Basic queries */ \
static inline bool vec_##type##_is_empty(const vec_##type* v) { return !v || v->len == 0; } \
static inline bool vec_##type##_is_full(const vec_##type* v) { return v && v->len >= v->capacity; } \
static inline size_t vec_##type##_len(const vec_##type* v) { return v ? v->len : 0; } \
static inline size_t vec_##type##_capacity(const vec_##type* v) { return v ? v->capacity : 0; } \
static inline size_t vec_##type##_remaining(const vec_##type* v) { return v ? (v->capacity - v->len) : 0; } \
\
/* Element access */ \
static inline bool vec_##type##_get(const vec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->items[i]; \
    return true; \
} \
static inline type vec_##type##_get_unchecked(const vec_##type* v, size_t i) { \
    assert(v && v->items && i < v->len); \
    return v->items[i]; \
} \
static inline bool vec_##type##_set(vec_##type* v, size_t i, type val) { \
    if (!v || i >= v->len) return false; \
    v->items[i] = val; \
    return true; \
} \
\
/* Push / Pop */ \
static inline result_bool_constcharptr vec_##type##_push(vec_##type* v, type item) { \
    if (!v || !v->items) return result_bool_constcharptr_err("vec_push: null vector or buffer"); \
    if (v->len >= v->capacity) return result_bool_constcharptr_err("vec_push: capacity exceeded"); \
    v->items[v->len++] = item; \
    return result_bool_constcharptr_ok(true); \
} \
static inline result_bool_constcharptr vec_##type##_pop(vec_##type* v, type* out) { \
    if (!v || !out || !v->items) return result_bool_constcharptr_err("vec_pop: null vector or buffer"); \
    if (v->len == 0) return result_bool_constcharptr_err("vec_pop: empty vector"); \
    *out = v->items[--v->len]; \
    return result_bool_constcharptr_ok(true); \
} \
\
/* Clear */ \
static inline void vec_##type##_clear(vec_##type* v) { if (v) v->len = 0; } \
\
/* First / Last / Data */ \
static inline type* vec_##type##_first(const vec_##type* v) { return (v && v->len > 0) ? &v->items[0] : NULL; } \
static inline type* vec_##type##_last(const vec_##type* v) { return (v && v->len > 0) ? &v->items[v->len - 1] : NULL; } \
static inline type* vec_##type##_data(const vec_##type* v) { return v ? v->items : NULL; } \
\
/* Insert / Remove / Extend */ \
static inline result_bool_constcharptr vec_##type##_insert(vec_##type* v, size_t index, type item) { \
    if (!v || !v->items) return result_bool_constcharptr_err("vec_insert: null vector or buffer"); \
    if (index > v->len) return result_bool_constcharptr_err("vec_insert: index out of bounds"); \
    if (v->len >= v->capacity) return result_bool_constcharptr_err("vec_insert: capacity exceeded"); \
    for (size_t i = v->len; i > index; i--) v->items[i] = v->items[i-1]; \
    v->items[index] = item; \
    v->len++; \
    return result_bool_constcharptr_ok(true); \
} \
static inline result_bool_constcharptr vec_##type##_remove(vec_##type* v, size_t index, type* out) { \
    if (!v || !v->items || !out) return result_bool_constcharptr_err("vec_remove: null vector or buffer"); \
    if (v->len == 0) return result_bool_constcharptr_err("vec_remove: empty vector"); \
    if (index >= v->len) return result_bool_constcharptr_err("vec_remove: index out of bounds"); \
    *out = v->items[index]; \
    for (size_t i = index; i < v->len-1; i++) v->items[i] = v->items[i+1]; \
    v->len--; \
    return result_bool_constcharptr_ok(true); \
} \
static inline result_bool_constcharptr vec_##type##_extend(vec_##type* v, const type* src, size_t count) { \
    if (!v || !v->items || !src) return result_bool_constcharptr_err("vec_extend: null vector or buffer"); \
    if (v->len + count > v->capacity) return result_bool_constcharptr_err("vec_extend: capacity exceeded"); \
    for (size_t i = 0; i < count; i++) v->items[v->len+i] = src[i]; \
    v->len += count; \
    return result_bool_constcharptr_ok(true); \
} \
\
/* ────────────────────────────────────────────────────────────────
   Phase 1 Enhancement: Heap-backed allocation
   ──────────────────────────────────────────────────────────────── */ \
static inline vec_##type vec_##type##_alloc(size_t capacity) { \
    if (capacity == 0) return vec_##type##_empty(); \
    type* buf = (type*)malloc(capacity * sizeof(type)); \
    if (!buf) return vec_##type##_empty(); \
    return vec_##type##_init(buf, capacity); \
} \
static inline void vec_##type##_free(vec_##type* v) { \
    if (v && v->items) { \
        free(v->items); \
        v->items = NULL; \
        v->len = 0; \
        v->capacity = 0; \
    } \
} \
\
/* ────────────────────────────────────────────────────────────────
   Phase 1 Enhancement: Arena-backed allocation
   ──────────────────────────────────────────────────────────────── */ \
static inline vec_##type vec_##type##_arena_alloc(Arena* arena, size_t capacity) { \
    if (!arena || capacity == 0) return vec_##type##_empty(); \
    type* buf = arena_alloc_array(arena, type, capacity); \
    return vec_##type##_init(buf, capacity); \
} \
\
/* ────────────────────────────────────────────────────────────────
   Phase 1 Enhancement: Iterator / Range support
   ──────────────────────────────────────────────────────────────── */ \
typedef struct { \
    vec_##type* vec; \
    size_t index; \
} vec_##type##_iter; \
static inline vec_##type##_iter vec_##type##_iter_init(vec_##type* v) { return (vec_##type##_iter){ .vec = v, .index = 0 }; } \
static inline bool vec_##type##_iter_next(vec_##type##_iter* it, type* out) { \
    if (!it || !it->vec || !out) return false; \
    if (it->index >= it->vec->len) return false; \
    *out = it->vec->items[it->index++]; \
    return true; \
} \
\
/* ────────────────────────────────────────────────────────────────
   Phase 1 Enhancement: Slice / Subvector views
   ──────────────────────────────────────────────────────────────── */ \
typedef struct { \
    type* items; \
    size_t len; \
} vec_##type##_slice; \
static inline vec_##type##_slice vec_##type##_slice_init(vec_##type* v, size_t start, size_t end) { \
    vec_##type##_slice s = {0}; \
    if (!v || start > end || end > v->len) return s; \
    s.items = &v->items[start]; \
    s.len = end - start; \
    return s; \
} \
static inline type* vec_##type##_slice_get(const vec_##type##_slice* s, size_t i) { \
    assert(s && i < s->len); \
    return &s->items[i]; \
}

/* ────────────────────────────────────────────────────────────────
   Examples
   ──────────────────────────────────────────────────────────────── */
/*
#include "vec.h"

DEFINE_VEC(int)

void example_basic(void) {
    // Stack-allocated
    int buf[10];
    vec_int v = vec_int_init(buf, 10);

    vec_int_push(&v, 1);
    vec_int_push(&v, 2);

    int val;
    if (vec_int_pop(&v, &val) == result_bool_constcharptr_ok(true)) {
        printf("Popped: %d\n", val);
    }

    // Iterator
    vec_int_iter it = vec_int_iter_init(&v);
    while (vec_int_iter_next(&it, &val)) {
        printf("%d ", val);
    }
    printf("\n");

    // Slice
    vec_int_slice s = vec_int_slice_init(&v, 0, vec_int_len(&v));
    for (size_t i = 0; i < s.len; i++) {
        printf("%d ", *vec_int_slice_get(&s, i));
    }
    printf("\n");
}

void example_heap(void) {
    vec_int v = vec_int_alloc(5);
    vec_int_push(&v, 10);
    vec_int_free(&v);
}

void example_arena(Arena* arena) {
    vec_int v = vec_int_arena_alloc(arena, 10);
    vec_int_push(&v, 42);
}
*/

#endif /* CANON_DATA_VEC_H */
