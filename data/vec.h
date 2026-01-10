// data/vec.h
#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include <stddef.h>
#include <stdbool.h>
#include "core/memory.h"   // for mem_copy, etc. (if extended later)
#include "semantics/result.h"

/**
 * @file vec.h
 * @brief Bounded dynamic vector with explicit caller-owned buffer
 *
 * Provides a safe, bounded contiguous sequence (like std::vector but no allocation/resizing).
 * Key properties:
 *   - Caller provides and owns the buffer (stack, arena, static, etc.)
 *   - Fixed maximum capacity (no growth)
 *   - All operations are explicit and bounds-checked
 *   - Push/pop return Result for composable, safe error handling
 *   - Zero hidden state or allocations
 *
 * Usage patterns:
 *   - Stack-allocated: char buf[1024]; vec_int v = vec_int_init((int*)buf, 1024/sizeof(int));
 *   - Arena-backed: vec_str v = vec_str_init(arena_alloc_array(arena, const char*, 100), 100);
 *   - Error handling: result_bool_constcharp res = vec_int_push(&v, 42); if (result_is_err(res)) { ... }
 */

/* ────────────────────────────────────────────────────────────────────────────
   Result type for unit success/failure (true = success, const char* error)
   ──────────────────────────────────────────────────────────────────────────── */
CANON_C_DEFINE_RESULT_UNIT(const char*)  // result_bool_constcharp

/* ────────────────────────────────────────────────────────────────────────────
   Generic base vector (void** items) — for low-level or untyped use
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Generic vector of void* pointers (bounded, caller-owned buffer)
 */
typedef struct {
    void** items;     ///< Pointer to caller-owned buffer
    size_t len;       ///< Current number of elements
    size_t capacity;  ///< Maximum number of elements (fixed)
} vec_voidp;

/**
 * @brief Initializes a generic vector with caller-provided buffer
 * @param buffer   Pointer to array of void* (must remain valid)
 * @param capacity Maximum number of elements the buffer can hold
 * @return         Initialized vec_voidp
 */
static inline vec_voidp vec_voidp_init(void** buffer, size_t capacity) {
    return (vec_voidp){ .items = buffer, .len = 0, .capacity = capacity };
}

/**
 * @brief Returns an empty/invalid generic vector (useful for defaults)
 */
static inline vec_voidp vec_voidp_empty(void) {
    return (vec_voidp){0};
}

/**
 * @brief Checks if vector is empty
 */
static inline bool vec_voidp_is_empty(const vec_voidp* v) {
    return !v || v->len == 0;
}

/**
 * @brief Checks if vector is full (cannot push more)
 */
static inline bool vec_voidp_is_full(const vec_voidp* v) {
    return v && v->len >= v->capacity;
}

/**
 * @brief Returns current number of elements
 */
static inline size_t vec_voidp_len(const vec_voidp* v) {
    return v ? v->len : 0;
}

/**
 * @brief Returns maximum capacity
 */
static inline size_t vec_voidp_capacity(const vec_voidp* v) {
    return v ? v->capacity : 0;
}

/**
 * @brief Safely gets element at index (with bounds check)
 * @param v    Vector instance
 * @param i    Index (0 <= i < len)
 * @param out  Pointer to receive the item
 * @return     true if successful, false if invalid index or null pointers
 */
static inline bool vec_voidp_get(const vec_voidp* v, size_t i, void** out) {
    if (!v || !out || i >= v->len) return false;
    *out = v->items[i];
    return true;
}

/**
 * @brief Gets element at index without bounds check (unsafe — use with caution)
 */
static inline void* vec_voidp_get_unchecked(const vec_voidp* v, size_t i) {
    return v->items[i];
}

/**
 * @brief Pushes an item to the end if capacity allows
 * @param v     Vector instance
 * @param item  Pointer to add
 * @return      Ok(true) on success, Err(message) on failure
 */
static inline result_bool_constcharp vec_voidp_push(vec_voidp* v, void* item) {
    if (!v || !v->items)
        return result_bool_constcharp_err("null vec or buffer");
    if (v->len >= v->capacity)
        return result_bool_constcharp_err("capacity exceeded");

    v->items[v->len++] = item;
    return result_bool_constcharp_ok(true);
}

/**
 * @brief Pops the last item if vector is not empty
 * @param v    Vector instance
 * @param out  Pointer to receive the popped item
 * @return     Ok(true) on success, Err(message) on failure
 */
static inline result_bool_constcharp vec_voidp_pop(vec_voidp* v, void** out) {
    if (!v || !out || !v->items)
        return result_bool_constcharp_err("null vec or buffer");
    if (v->len == 0)
        return result_bool_constcharp_err("pop from empty vec");

    *out = v->items[--v->len];
    return result_bool_constcharp_ok(true);
}

/**
 * @brief Clears the vector (sets length to 0 — does not zero buffer)
 */
static inline void vec_voidp_clear(vec_voidp* v) {
    if (v) v->len = 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Typed vector macro — recommended way to use (type-safe)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Defines a type-safe vector for a specific element type
 *
 * Generates: struct vec_##type + full set of typed functions.
 * All operations are bounds-checked and return Result where appropriate.
 *
 * @param type Element type (e.g. int, const char*, struct Point)
 *
 * Example usage:
 *   DEFINE_VEC(int);
 *   int buf[100];
 *   vec_int v = vec_int_init(buf, 100);
 *   vec_int_push(&v, 42);
 */
#define DEFINE_VEC(type) \
typedef struct { \
    type* items; \
    size_t len; \
    size_t capacity; \
} vec_##type; \
\
/** \
 * @brief Initializes typed vector with caller-provided buffer \
 * @param buffer   Array of type (must remain valid) \
 * @param capacity Max number of elements buffer can hold \
 * @return         Initialized vec_##type \
 */ \
static inline vec_##type vec_##type##_init(type* buffer, size_t capacity) { \
    return (vec_##type){ .items = buffer, .len = 0, .capacity = capacity }; \
} \
\
/** \
 * @brief Returns empty/invalid typed vector (for defaults) \
 */ \
static inline vec_##type vec_##type##_empty(void) { \
    return (vec_##type){0}; \
} \
\
static inline bool vec_##type##_is_empty(const vec_##type* v) { return !v || v->len == 0; } \
static inline bool vec_##type##_is_full(const vec_##type* v) { return v && v->len >= v->capacity; } \
static inline size_t vec_##type##_len(const vec_##type* v) { return v ? v->len : 0; } \
static inline size_t vec_##type##_capacity(const vec_##type* v){ return v ? v->capacity : 0; } \
\
/** \
 * @brief Safely gets element at index \
 * @param v    Vector \
 * @param i    Index \
 * @param out  Where to store the value \
 * @return     true on success, false on invalid index/pointers \
 */ \
static inline bool vec_##type##_get(const vec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->items[i]; \
    return true; \
} \
\
/** \
 * @brief Gets element at index without check (unsafe) \
 */ \
static inline type vec_##type##_get_unchecked(const vec_##type* v, size_t i) { \
    return v->items[i]; \
} \
\
/** \
 * @brief Pushes a value to the end if space available \
 * @return Ok(true) on success, Err(message) on failure \
 */ \
static inline result_bool_constcharp vec_##type##_push(vec_##type* v, type item) { \
    if (!v || !v->items) return result_bool_constcharp_err("null vec or buffer"); \
    if (v->len >= v->capacity) return result_bool_constcharp_err("capacity exceeded"); \
    v->items[v->len++] = item; \
    return result_bool_constcharp_ok(true); \
} \
\
/** \
 * @brief Pops the last value if not empty \
 * @return Ok(true) on success, Err(message) on failure \
 */ \
static inline result_bool_constcharp vec_##type##_pop(vec_##type* v, type* out) { \
    if (!v || !out || !v->items) return result_bool_constcharp_err("null vec or buffer"); \
    if (v->len == 0) return result_bool_constcharp_err("pop from empty vec"); \
    *out = v->items[--v->len]; \
    return result_bool_constcharp_ok(true); \
} \
\
/** \
 * @brief Clears the vector (len = 0, buffer unchanged) \
 */ \
static inline void vec_##type##_clear(vec_##type* v) { \
    if (v) v->len = 0; \
}

#endif /* CANON_DATA_VEC_H */
