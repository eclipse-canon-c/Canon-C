// data/stack.h
#ifndef CANON_DATA_STACK_H
#define CANON_DATA_STACK_H

#include "data/vec.h"  // underlying fixed-capacity vector implementation

/**
 * @file stack.h
 * @brief Simple LIFO (Last-In-First-Out) stack – thin wrapper over vec
 *
 * Provides a clean, intention-revealing stack interface using the same fixed-capacity
 * buffer as `vec`. It is **not** a growable container.
 *
 *                           CRITICAL DESIGN NOTE
 * ──────────────────────────────────────────────────────────────────────────────
 * FIXED CAPACITY – NO AUTOMATIC GROWTH
 *
 * • Capacity is **fixed** at initialization (caller provides the buffer)
 * • The stack will **NEVER** automatically resize or reallocate
 * • When full: push operations **fail** (return false)
 * • When empty: pop operations **fail** (return false)
 * • No silent truncation, no hidden allocations
 *
 * This is **intentional**:
 *   - Maximum predictability
 *   - Zero surprise allocations (ideal for real-time, embedded, arena usage)
 *   - Strong ownership & lifetime control
 *
 * If you need a stack that grows automatically:
 *   → You must implement it yourself (e.g. using realloc + doubling strategy)
 *   → Or use a different container / external library
 *
 *                           Key Properties
 * ──────────────────────────────────────────────────────────────────────────────
 * • Zero runtime overhead – most operations are direct vec calls
 * • Bounds-checked push/pop with clear success/failure
 * • Caller fully owns the underlying buffer (stack, arena, static, heap...)
 * • Type-safe via macro (DEFINE_STACK)
 * • Perfect for: undo systems, expression evaluation, call stack simulation, backtracking...
 *
 *                           Recommended Usage Patterns
 * ──────────────────────────────────────────────────────────────────────────────
 *
 * // 1. Stack-allocated – zero dynamic allocation
 * DEFINE_STACK(int);
 * int buf[128];
 * stack_int undo_stack;
 * stack_int_init(&undo_stack, buf, 128);
 *
 * // 2. Arena-backed – explicit lifetime
 * int* arena_buf = arena_alloc_array(&my_arena, int, 256);
 * stack_int history;
 * stack_int_init(&history, arena_buf, 256);
 *
 * // Pre-check before bulk pushes (recommended)
 * if (stack_int_len(&s) + items_to_add > stack_int_capacity(&s)) {
 *     return error_not_enough_space;
 * }
 */

#define DEFINE_STACK(Type) \
typedef vec_##Type stack_##Type; \
\
/** \
 * @brief Initializes the stack with caller-provided fixed buffer \
 * @param s     Pointer to uninitialized stack_##Type \
 * @param buffer Array of Type – must remain valid for the lifetime of the stack \
 * @param capacity Maximum number of elements the buffer can hold \
 * \
 * @note Capacity is fixed forever – no growth possible later \
 */ \
static inline void stack_##Type##_init(stack_##Type* s, Type* buffer, size_t capacity) { \
    vec_##Type##_init(s, buffer, capacity); \
} \
\
/** \
 * @brief Pushes an item onto the top of the stack \
 * @param s    Valid stack instance \
 * @param item Value to push \
 * @return true on success, false if stack is full or invalid pointers \
 */ \
static inline bool stack_##Type##_push(stack_##Type* s, Type item) { \
    result_bool_constcharp r = vec_##Type##_push(s, item); \
    return result_bool_constcharp_is_ok(r); \
} \
\
/** \
 * @brief Pops the top item from the stack \
 * @param s    Valid stack instance \
 * @param out  Where to store the popped value \
 * @return true on success, false if stack is empty or invalid pointers \
 */ \
static inline bool stack_##Type##_pop(stack_##Type* s, Type* out) { \
    return result_bool_constcharp_is_ok(vec_##Type##_pop(s, out)); \
} \
\
/** \
 * @brief Returns current number of elements in the stack \
 */ \
static inline size_t stack_##Type##_len(const stack_##Type* s) { \
    return vec_##Type##_len(s); \
} \
\
/** \
 * @brief Returns maximum capacity of the stack (fixed) \
 */ \
static inline size_t stack_##Type##_capacity(const stack_##Type* s) { \
    return vec_##Type##_capacity(s); \
} \
\
/** \
 * @brief Checks if stack is empty \
 */ \
static inline bool stack_##Type##_is_empty(const stack_##Type* s) { \
    return vec_##Type##_is_empty(s); \
} \
\
/** \
 * @brief Checks if stack is full (cannot push more) \
 */ \
static inline bool stack_##Type##_is_full(const stack_##Type* s) { \
    return vec_##Type##_is_full(s); \
} \
\
/** \
 * @brief Clears the stack (sets length to 0, buffer unchanged) \
 */ \
static inline void stack_##Type##_clear(stack_##Type* s) { \
    vec_##Type##_clear(s); \
}

#endif /* CANON_DATA_STACK_H */
