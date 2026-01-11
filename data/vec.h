// data/vec.h
#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include "core/memory.h"
#include "semantics/result.h"

/**
 * @file vec.h
 * @brief Fixed-capacity dynamic vector with **explicit caller-owned buffer**
 *
 * This is NOT a std::vector / Vec<T> with automatic growth.
 * Instead it provides a **bounded**, **safe**, **type-safe** contiguous sequence
 * where the caller completely controls allocation and lifetime.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, compound literals)
 *   - Depends on memory.h and result.h from this library
 *   - No platform-specific code
 *
 * Thread-safety: Each vector instance is independent - not thread-safe for
 *                concurrent modifications. Caller must synchronize if needed.
 *
 * Performance: 
 *   - Zero overhead abstraction - compiles to direct array access
 *   - All operations are O(1) except those that shift elements
 *   - No hidden allocations
 *   - Cache-friendly contiguous storage
 *
 *                           KEY DESIGN PRINCIPLES
 * ──────────────────────────────────────────────────────────────────────────────
 *  • Caller owns and provides the storage buffer (stack, arena, heap, static…)
 *  • Capacity is **fixed at initialization** — **no automatic realloc/growth**
 *  • All operations are **bounds-checked** by default
 *  • Push/pop/insert operations return Result → composable error handling
 *  • Zero hidden state, zero implicit allocations, zero global variables
 *  • Maximum transparency and predictability — ideal for:
 *      - Real-time systems
 *      - Embedded / memory-constrained environments
 *      - Arena-style allocation patterns
 *      - Code that must not fail due to unexpected out-of-memory
 *
 *                          WHEN TO USE THIS VECTOR
 * ──────────────────────────────────────────────────────────────────────────────
 * Use Canon vec when you want:
 *   ✓ Predictable memory behavior
 *   ✓ No surprise allocations during runtime
 *   ✓ Strong ownership semantics
 *   ✓ Error handling via Result
 *   ✓ Deterministic performance
 *   ✓ Explicit capacity management
 *
 * Do NOT use this vector when you want:
 *   ✗ Automatic resizing / amortized O(1) append
 *   ✗ "I don't want to think about capacity" convenience
 *   ✗ Dynamic growth based on usage patterns
 *
 * In those cases consider:
 *   - Your own growable vector (using realloc or custom strategy)
 *   - External libraries with automatic growth
 *   - Higher-level container abstractions
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
 *   int* arena_buf = arena_alloc_array(&arena, int, 1024);
 *   vec_int large = vec_int_init(arena_buf, 1024);
 *
 *   // 3. Pre-check before bulk operations
 *   if (vec_int_remaining(&v) < items_to_add) {
 *       return ERR_CAPACITY_EXCEEDED;
 *   }
 *
 *   // 4. Error handling with Result
 *   result_bool_constcharptr r = vec_int_push(&v, 42);
 *   if (result_bool_constcharptr_is_err(r)) {
 *       const char* err = result_bool_constcharptr_unwrap_err(r);
 *       fprintf(stderr, "Push failed: %s\n", err);
 *   }
 */

/* ────────────────────────────────────────────────────────────────────────────
   Result type for vector operations
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * Define Result<bool, const char*> for vector operations
 * Used by push, pop, insert, remove operations to indicate success/failure
 */
typedef const char* constcharptr;
CANON_C_DEFINE_RESULT(bool, constcharptr)

/* ────────────────────────────────────────────────────────────────────────────
   Generic base vector (void*) — mostly for internal use
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Generic pointer vector (internal)
 *
 * Used as foundation for typed vectors. Not typically used directly.
 * Prefer DEFINE_VEC(type) for type-safe vectors.
 */
typedef struct {
    void** items;    ///< Pointer to buffer (caller-owned)
    size_t len;      ///< Current number of elements
    size_t capacity; ///< Maximum number of elements
} vec_voidptr;

/**
 * @brief Initializes a generic pointer vector
 */
static inline vec_voidptr vec_voidptr_init(void** buffer, size_t capacity) {
    assert(buffer != NULL || capacity == 0);
    assert(capacity <= SIZE_MAX / sizeof(void*));
    return (vec_voidptr){ .items = buffer, .len = 0, .capacity = capacity };
}

/**
 * @brief Creates an empty vector (no storage)
 */
static inline vec_voidptr vec_voidptr_empty(void) {
    return (vec_voidptr){0};
}

/**
 * @brief Checks if vector is empty
 */
static inline bool vec_voidptr_is_empty(const vec_voidptr* v) {
    return !v || v->len == 0;
}

/**
 * @brief Checks if vector is full
 */
static inline bool vec_voidptr_is_full(const vec_voidptr* v) {
    return v && v->len >= v->capacity;
}

/**
 * @brief Returns current number of elements
 */
static inline size_t vec_voidptr_len(const vec_voidptr* v) {
    return v ? v->len : 0;
}

/**
 * @brief Returns maximum capacity
 */
static inline size_t vec_voidptr_capacity(const vec_voidptr* v) {
    return v ? v->capacity : 0;
}

/**
 * @brief Returns number of free slots
 */
static inline size_t vec_voidptr_remaining(const vec_voidptr* v) {
    return v ? (v->capacity - v->len) : 0;
}

/**
 * @brief Safely gets element at index
 */
static inline bool vec_voidptr_get(const vec_voidptr* v, size_t i, void** out) {
    if (!v || !out || i >= v->len) return false;
    *out = v->items[i];
    return true;
}

/**
 * @brief Gets element without bounds checking (UNSAFE)
 */
static inline void* vec_voidptr_get_unchecked(const vec_voidptr* v, size_t i) {
    assert(v != NULL && "vec_voidptr_get_unchecked: vector cannot be NULL");
    assert(v->items != NULL && "vec_voidptr_get_unchecked: items cannot be NULL");
    assert(i < v->len && "vec_voidptr_get_unchecked: index out of bounds");
    return v->items[i];
}

/**
 * @brief Pushes element to end of vector
 */
static inline result_bool_constcharptr vec_voidptr_push(vec_voidptr* v, void* item) {
    if (!v || !v->items)
        return result_bool_constcharptr_err("null vec or buffer");
    if (v->len >= v->capacity)
        return result_bool_constcharptr_err("capacity exceeded");
    v->items[v->len++] = item;
    return result_bool_constcharptr_ok(true);
}

/**
 * @brief Pops element from end of vector
 */
static inline result_bool_constcharptr vec_voidptr_pop(vec_voidptr* v, void** out) {
    if (!v || !out || !v->items)
        return result_bool_constcharptr_err("null vec or buffer");
    if (v->len == 0)
        return result_bool_constcharptr_err("pop from empty vec");
    *out = v->items[--v->len];
    return result_bool_constcharptr_ok(true);
}

/**
 * @brief Clears all elements (sets length to 0)
 */
static inline void vec_voidptr_clear(vec_voidptr* v) {
    if (v) v->len = 0;
}

/**
 * @brief Gets pointer to first element
 */
static inline void** vec_voidptr_first(const vec_voidptr* v) {
    return (v && v->len > 0) ? &v->items[0] : NULL;
}

/**
 * @brief Gets pointer to last element
 */
static inline void** vec_voidptr_last(const vec_voidptr* v) {
    return (v && v->len > 0) ? &v->items[v->len - 1] : NULL;
}

/* ────────────────────────────────────────────────────────────────────────────
   Typed vector — recommended way to use (type-safe macros)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Define a type-safe fixed-capacity vector for any element type
 *
 * Generates a complete vector implementation for the specified type,
 * including struct definition and all associated functions.
 *
 * @param type Any complete type (int, float, struct Point, const char*, etc.)
 *
 * Generated type: vec_##type
 * Generated functions:
 *   - vec_##type##_init(buffer, capacity) - Initialize vector
 *   - vec_##type##_empty() - Create empty vector
 *   - vec_##type##_is_empty(v) - Check if empty
 *   - vec_##type##_is_full(v) - Check if full
 *   - vec_##type##_len(v) - Get length
 *   - vec_##type##_capacity(v) - Get capacity
 *   - vec_##type##_remaining(v) - Get free slots
 *   - vec_##type##_get(v, i, &out) - Safe element access
 *   - vec_##type##_get_unchecked(v, i) - Unsafe element access
 *   - vec_##type##_set(v, i, val) - Safe element update
 *   - vec_##type##_push(v, item) - Add to end
 *   - vec_##type##_pop(v, &out) - Remove from end
 *   - vec_##type##_clear(v) - Remove all elements
 *   - vec_##type##_first(v) - Get first element pointer
 *   - vec_##type##_last(v) - Get last element pointer
 *   - vec_##type##_data(v) - Get raw buffer pointer
 *
 * Usage:
 *   DEFINE_VEC(int)        // Defines vec_int
 *   DEFINE_VEC(float)      // Defines vec_float
 *   DEFINE_VEC(MyStruct)   // Defines vec_MyStruct
 *
 * Example:
 *     DEFINE_VEC(int);
 *     
 *     int buf[256];
 *     vec_int scores = vec_int_init(buf, 256);
 *     
 *     vec_int_push(&scores, 42);
 *     vec_int_push(&scores, 100);
 *     
 *     int value;
 *     if (vec_int_get(&scores, 0, &value)) {
 *         printf("First score: %d\n", value);
 *     }
 *
 * Note: This must be used at file or global scope, not inside functions.
 *       Use once per type in a header or source file.
 */
#define DEFINE_VEC(type) \
\
/** \
 * @brief Fixed-capacity vector for type 'type' \
 * \
 * Fields: \
 *   - items: Pointer to caller-owned buffer \
 *   - len: Current number of elements \
 *   - capacity: Maximum number of elements \
 * \
 * Do not access fields directly - use the provided functions. \
 */ \
typedef struct { \
    type* items;     \
    size_t len;      \
    size_t capacity; \
} vec_##type; \
\
/** \
 * @brief Initializes vector with caller-provided buffer \
 * \
 * @param buffer   Pointer to storage (must remain valid) \
 * @param capacity Number of elements buffer can hold \
 * @return         Initialized vector \
 * \
 * Preconditions: \
 *   - buffer != NULL (or capacity == 0) \
 *   - buffer has space for capacity elements \
 * \
 * Example: \
 *   int buf[100]; \
 *   vec_int v = vec_int_init(buf, 100); \
 */ \
static inline vec_##type vec_##type##_init(type* buffer, size_t capacity) { \
    assert(buffer != NULL || capacity == 0); \
    assert(capacity <= SIZE_MAX / sizeof(type)); \
    return (vec_##type){ .items = buffer, .len = 0, .capacity = capacity }; \
} \
\
/** \
 * @brief Creates an empty vector with no storage \
 * \
 * Useful as a placeholder or initial state. \
 * Cannot be used until initialized with vec_##type##_init. \
 */ \
static inline vec_##type vec_##type##_empty(void) { \
    return (vec_##type){0}; \
} \
\
/** \
 * @brief Checks if vector contains no elements \
 */ \
static inline bool vec_##type##_is_empty(const vec_##type* v) { \
    return !v || v->len == 0; \
} \
\
/** \
 * @brief Checks if vector has reached capacity \
 */ \
static inline bool vec_##type##_is_full(const vec_##type* v) { \
    return v && v->len >= v->capacity; \
} \
\
/** \
 * @brief Returns current number of elements \
 */ \
static inline size_t vec_##type##_len(const vec_##type* v) { \
    return v ? v->len : 0; \
} \
\
/** \
 * @brief Returns maximum capacity \
 */ \
static inline size_t vec_##type##_capacity(const vec_##type* v) { \
    return v ? v->capacity : 0; \
} \
\
/** \
 * @brief Returns number of free slots remaining \
 */ \
static inline size_t vec_##type##_remaining(const vec_##type* v) { \
    return v ? (v->capacity - v->len) : 0; \
} \
\
/** \
 * @brief Safely gets element at index \
 * \
 * @param v   Vector to access \
 * @param i   Index (must be < len) \
 * @param out Pointer to store element \
 * @return    true if successful, false if index out of bounds \
 * \
 * Example: \
 *   int value; \
 *   if (vec_int_get(&v, 5, &value)) { \
 *       printf("Element 5: %d\n", value); \
 *   } \
 */ \
static inline bool vec_##type##_get(const vec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->items[i]; \
    return true; \
} \
\
/** \
 * @brief Gets element without bounds checking (UNSAFE) \
 * \
 * ⚠️  WARNING: No bounds checking! Undefined behavior if i >= len. \
 * Only use when you've already verified the index is valid. \
 * \
 * @param v Vector to access \
 * @param i Index (must be < len) \
 * @return  Element at index \
 * \
 * Panics: In debug builds if index is out of bounds \
 */ \
static inline type vec_##type##_get_unchecked(const vec_##type* v, size_t i) { \
    assert(v != NULL && "vec_get_unchecked: vector cannot be NULL"); \
    assert(v->items != NULL && "vec_get_unchecked: items cannot be NULL"); \
    assert(i < v->len && "vec_get_unchecked: index out of bounds"); \
    return v->items[i]; \
} \
\
/** \
 * @brief Safely sets element at index \
 * \
 * @param v   Vector to modify \
 * @param i   Index (must be < len) \
 * @param val Value to set \
 * @return    true if successful, false if index out of bounds \
 * \
 * Example: \
 *   if (vec_int_set(&v, 5, 42)) { \
 *       printf("Set element 5 to 42\n"); \
 *   } \
 */ \
static inline bool vec_##type##_set(vec_##type* v, size_t i, type val) { \
    if (!v || i >= v->len) return false; \
    v->items[i] = val; \
    return true; \
} \
\
/** \
 * @brief Pushes element to end of vector \
 * \
 * @param v    Vector to modify \
 * @param item Element to add \
 * @return     Ok(true) if successful, Err(msg) if full or invalid \
 * \
 * Example: \
 *   result_bool_constcharptr r = vec_int_push(&v, 42); \
 *   if (result_bool_constcharptr_is_err(r)) { \
 *       fprintf(stderr, "Push failed\n"); \
 *   } \
 */ \
static inline result_bool_constcharptr vec_##type##_push(vec_##type* v, type item) { \
    if (!v || !v->items) \
        return result_bool_constcharptr_err("null vec or buffer"); \
    if (v->len >= v->capacity) \
        return result_bool_constcharptr_err("capacity exceeded"); \
    v->items[v->len++] = item; \
    return result_bool_constcharptr_ok(true); \
} \
\
/** \
 * @brief Pops element from end of vector \
 * \
 * @param v   Vector to modify \
 * @param out Pointer to store popped element \
 * @return    Ok(true) if successful, Err(msg) if empty or invalid \
 * \
 * Example: \
 *   int value; \
 *   if (result_bool_constcharptr_is_ok(vec_int_pop(&v, &value))) { \
 *       printf("Popped: %d\n", value); \
 *   } \
 */ \
static inline result_bool_constcharptr vec_##type##_pop(vec_##type* v, type* out) { \
    if (!v || !out || !v->items) \
        return result_bool_constcharptr_err("null vec or buffer"); \
    if (v->len == 0) \
        return result_bool_constcharptr_err("pop from empty vec"); \
    *out = v->items[--v->len]; \
    return result_bool_constcharptr_ok(true); \
} \
\
/** \
 * @brief Clears all elements (sets length to 0) \
 * \
 * Does not modify buffer contents, only resets length. \
 * O(1) operation. \
 */ \
static inline void vec_##type##_clear(vec_##type* v) { \
    if (v) v->len = 0; \
} \
\
/** \
 * @brief Gets pointer to first element \
 * \
 * @param v Vector to access \
 * @return  Pointer to first element, or NULL if empty \
 * \
 * Example: \
 *   int* first = vec_int_first(&v); \
 *   if (first) printf("First: %d\n", *first); \
 */ \
static inline type* vec_##type##_first(const vec_##type* v) { \
    return (v && v->len > 0) ? &v->items[0] : NULL; \
} \
\
/** \
 * @brief Gets pointer to last element \
 * \
 * @param v Vector to access \
 * @return  Pointer to last element, or NULL if empty \
 */ \
static inline type* vec_##type##_last(const vec_##type* v) { \
    return (v && v->len > 0) ? &v->items[v->len - 1] : NULL; \
} \
\
/** \
 * @brief Gets pointer to underlying buffer \
 * \
 * Useful for passing to APIs that expect raw arrays. \
 * \
 * @param v Vector to access \
 * @return  Pointer to buffer, or NULL if vector is NULL \
 * \
 * Example: \
 *   qsort(vec_int_data(&v), vec_int_len(&v), sizeof(int), compare); \
 */ \
static inline type* vec_##type##_data(const vec_##type* v) { \
    return v ? v->items : NULL; \
}

/* ────────────────────────────────────────────────────────────────────────────
   Common type instantiations
   ────────────────────────────────────────────────────────────────────────────
   
   Uncomment the types you need:
   
   DEFINE_VEC(int)
   DEFINE_VEC(unsigned)
   DEFINE_VEC(long)
   DEFINE_VEC(size_t)
   DEFINE_VEC(float)
   DEFINE_VEC(double)
   DEFINE_VEC(char)
   
   typedef const char* constcharptr;
   DEFINE_VEC(constcharptr)  // String vector
   
   typedef void* voidptr;
   DEFINE_VEC(voidptr)  // Generic pointer vector
   
   ──────────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────

    #include "vec.h"
    
    // Define vector type
    DEFINE_VEC(int)
    
    void example_basic(void) {
        // Stack-allocated storage
        int buffer[100];
        vec_int numbers = vec_int_init(buffer, 100);
        
        // Add elements
        vec_int_push(&numbers, 42);
        vec_int_push(&numbers, 100);
        vec_int_push(&numbers, 7);
        
        printf("Length: %zu\n", vec_int_len(&numbers));
        
        // Access elements
        int val;
        if (vec_int_get(&numbers, 1, &val)) {
            printf("Element 1: %d\n", val);
        }
        
        // Pop elements
        if (result_bool_constcharptr_is_ok(vec_int_pop(&numbers, &val))) {
            printf("Popped: %d\n", val);
        }
        
        // Iterate
        for (size_t i = 0; i < vec_int_len(&numbers); i++) {
            printf("%d ", vec_int_get_unchecked(&numbers, i));
        }
        printf("\n");
        
        // Clear
        vec_int_clear(&numbers);
    }
    
    void example_with_arena(Arena* arena) {
        // Arena-allocated storage
        int* buf = arena_alloc_array(arena, int, 1000);
        vec_int large = vec_int_init(buf, 1000);
        
        // Check capacity before bulk operations
        if (vec_int_remaining(&large) >= 500) {
            for (int i = 0; i < 500; i++) {
                vec_int_push(&large, i);
            }
        }
    }
    
    void example_error_handling(void) {
        int buf[2];
        vec_int small = vec_int_init(buf, 2);
        
        vec_int_push(&small, 1);
        vec_int_push(&small, 2);
        
        // This will fail - vector is full
        result_bool_constcharptr r = vec_int_push(&small, 3);
        if (result_bool_constcharptr_is_err(r)) {
            const char* err = result_bool_constcharptr_unwrap_err(r);
            fprintf(stderr, "Error: %s\n", err);
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_DATA_VEC_H */
