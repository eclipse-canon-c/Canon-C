// data/vec.h
#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include <stddef.h>
#include <stdbool.h>
#include "core/memory.h"     // for mem_copy etc. (if extended later)
#include "semantics/result.h"

/**
 * @file vec.h
 * @brief Fixed-capacity dynamic vector with **explicit caller-owned buffer**
 *
 * This is NOT a std::vector / Vec<T> with automatic growth.
 * Instead it provides a **bounded**, **safe**, **type-safe** contiguous sequence
 * where the caller completely controls allocation and lifetime.
 *
 *                           KEY DESIGN PRINCIPLES
 * ──────────────────────────────────────────────────────────────────────────────
 *  • Caller owns and provides the storage buffer (stack, arena, heap, static…)
 *  • Capacity is **fixed at initialization** — **no automatic realloc/growth**
 *  • All operations are **bounds-checked** by default
 *  • Push/pop/insert operations return Result → composable error handling
 *  • Zero hidden state, zero implicit allocations, zero global variables
 *  • Maximum transparency and predictability — ideal for:
 *      - real-time systems
 *      - embedded / memory-constrained environments
 *      - arena-style allocation patterns
 *      - code that must not fail due to unexpected out-of-memory
 *
 *                          WHEN TO USE THIS VECTOR
 * ──────────────────────────────────────────────────────────────────────────────
 * Use Canon vec when you want:
 *   ✓ Predictable memory behavior
 *   ✓ No surprise allocations during runtime
 *   ✓ Strong ownership semantics
 *   ✓ Error handling via Result
 *
 * Do NOT use this vector when you want:
 *   ✗ Automatic resizing / amortized O(1) append
 *   ✗ "I don't want to think about capacity" convenience
 *
 * In those cases consider:
 *   - Your own growable vector (using realloc or custom strategy)
 *   - stringbuf.h (which does grow)
 *   - External libraries with automatic growth
 *
 *                          IMPORTANT LIMITATIONS
 * ──────────────────────────────────────────────────────────────────────────────
 *  • Once full → push/insert operations **fail** (return Err)
 *  • No reserve/grow/ensure_capacity functions (by design)
 *  • You must know/estimate maximum size **before** initialization
 *
 * Typical patterns to stay safe:
 *
 *   // 1. Stack allocated - very common & zero allocation
 *   int numbers_buf[512];
 *   vec_int numbers = vec_int_init(numbers_buf, 512);
 *
 *   // 2. Arena backed - very explicit lifetime
 *   int* arena_buf = arena_alloc_array(arena, int, 1024);
 *   vec_int large = vec_int_init(arena_buf, 1024);
 *
 *   // 3. Pre-check before bulk operations
 *   if (vec_int_capacity(&v) - vec_int_len(&v) < items_to_add) {
 *       return error_not_enough_space;
 *   }
 */

CANON_C_DEFINE_RESULT_UNIT(const char*)   // result_bool_constcharp

/* ────────────────────────────────────────────────────────────────────────────
   Generic base vector (void**) — mostly for internal use
   ──────────────────────────────────────────────────────────────────────────── */

typedef struct {
    void** items;
    size_t len;
    size_t capacity;
} vec_voidp;

static inline vec_voidp vec_voidp_init(void** buffer, size_t capacity) {
    return (vec_voidp){ .items = buffer, .len = 0, .capacity = capacity };
}

static inline vec_voidp vec_voidp_empty(void) {
    return (vec_voidp){0};
}

static inline bool vec_voidp_is_empty(const vec_voidp* v) {
    return !v || v->len == 0;
}

static inline bool vec_voidp_is_full(const vec_voidp* v) {
    return v && v->len >= v->capacity;
}

static inline size_t vec_voidp_len(const vec_voidp* v) {
    return v ? v->len : 0;
}

static inline size_t vec_voidp_capacity(const vec_voidp* v) {
    return v ? v->capacity : 0;
}

static inline bool vec_voidp_get(const vec_voidp* v, size_t i, void** out) {
    if (!v || !out || i >= v->len) return false;
    *out = v->items[i];
    return true;
}

static inline void* vec_voidp_get_unchecked(const vec_voidp* v, size_t i) {
    return v->items[i];
}

static inline result_bool_constcharp vec_voidp_push(vec_voidp* v, void* item) {
    if (!v || !v->items)
        return result_bool_constcharp_err("null vec or buffer");
    if (v->len >= v->capacity)
        return result_bool_constcharp_err("capacity exceeded");
    v->items[v->len++] = item;
    return result_bool_constcharp_ok(true);
}

static inline result_bool_constcharp vec_voidp_pop(vec_voidp* v, void** out) {
    if (!v || !out || !v->items)
        return result_bool_constcharp_err("null vec or buffer");
    if (v->len == 0)
        return result_bool_constcharp_err("pop from empty vec");
    *out = v->items[--v->len];
    return result_bool_constcharp_ok(true);
}

static inline void vec_voidp_clear(vec_voidp* v) {
    if (v) v->len = 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Typed vector — recommended way to use (type-safe macros)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Define a type-safe fixed-capacity vector for any element type
 * @param type Any complete type (int, float, struct Point, const char*, etc.)
 *
 * Generates:
 *   - struct vec_##type
 *   - vec_##type##_init(...)
 *   - vec_##type##_push(...)
 *   - etc.
 *
 * Example:
 *     DEFINE_VEC(int);
 *     int buf[256];
 *     vec_int scores = vec_int_init(buf, 256);
 */
#define DEFINE_VEC(type) \
typedef struct { \
    type* items; \
    size_t len; \
    size_t capacity; \
} vec_##type; \
\
static inline vec_##type vec_##type##_init(type* buffer, size_t capacity) { \
    return (vec_##type){ .items = buffer, .len = 0, .capacity = capacity }; \
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
static inline bool vec_##type##_get(const vec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->items[i]; \
    return true; \
} \
\
static inline type vec_##type##_get_unchecked(const vec_##type* v, size_t i) { \
    return v->items[i]; \
} \
\
static inline result_bool_constcharp vec_##type##_push(vec_##type* v, type item) { \
    if (!v || !v->items) return result_bool_constcharp_err("null vec or buffer"); \
    if (v->len >= v->capacity) return result_bool_constcharp_err("capacity exceeded"); \
    v->items[v->len++] = item; \
    return result_bool_constcharp_ok(true); \
} \
\
static inline result_bool_constcharp vec_##type##_pop(vec_##type* v, type* out) { \
    if (!v || !out || !v->items) return result_bool_constcharp_err("null vec or buffer"); \
    if (v->len == 0) return result_bool_constcharp_err("pop from empty vec"); \
    *out = v->items[--v->len]; \
    return result_bool_constcharp_ok(true); \
} \
\
static inline void vec_##type##_clear(vec_##type* v) { \
    if (v) v->len = 0; \
}

#endif /* CANON_DATA_VEC_H */
