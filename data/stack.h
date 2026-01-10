// data/stack.h
#ifndef CANON_DATA_STACK_H
#define CANON_DATA_STACK_H

#include "data/vec.h"  // underlying vector implementation

/**
 * @file stack.h
 * @brief Simple LIFO (Last-In-First-Out) stack as a thin wrapper over vec
 *
 * Provides a clean, intention-revealing stack interface backed by a bounded vec.
 * All storage is caller-owned (stack uses the same fixed buffer as vec).
 *
 * Key properties:
 *   - Fixed maximum capacity (no dynamic growth)
 *   - Zero overhead wrapper (most functions are direct calls to vec)
 *   - Push/pop operations are bounds-checked and safe
 *   - Returns bool for success/failure (true = operation succeeded)
 *   - No hidden state or allocations
 *
 * Recommended usage:
 *   - Use when you want LIFO semantics with clear naming (push/pop)
 *   - Perfect for undo stacks, call stacks, expression evaluation, etc.
 *   - Always initialize with a caller-provided buffer (stack/arena/static)
 *
 * Example:
 *   DEFINE_STACK(int);
 *   int buf[128];
 *   stack_int s;
 *   stack_int_init(&s, buf, 128);
 *   stack_int_push(&s, 42);
 *   int top;
 *   if (stack_int_pop(&s, &top)) {
 *       // use top
 *   }
 */

/**
 * @brief Defines a type-specific stack backed by vec_##Type
 *
 * Generates: typedef stack_##Type as alias of vec_##Type
 *            + init/push/pop functions with stack-friendly names
 *
 * @param Type Element type (e.g. int, const char*, struct Node*)
 *
 * All operations delegate to the corresponding vec functions.
 * Push/pop return bool (true on success, false on full/empty/error).
 */
#define DEFINE_STACK(Type) \
typedef vec_##Type stack_##Type; \
\
/** \
 * @brief Initializes the stack with a caller-provided buffer \
 * @param s        Pointer to uninitialized stack_##Type \
 * @param buffer   Array of Type (must remain valid) \
 * @param capacity Maximum number of elements buffer can hold \
 */ \
static inline void stack_##Type##_init(stack_##Type* s, Type* buffer, size_t capacity) { \
    vec_##Type##_init(s, buffer, capacity); \
} \
\
/** \
 * @brief Pushes an item onto the top of the stack \
 * @param s     Valid stack instance \
 * @param item  Value to push \
 * @return      true if pushed successfully, false if stack is full or invalid \
 */ \
static inline bool stack_##Type##_push(stack_##Type* s, Type item) { \
    result_bool_constcharp r = vec_##Type##_push(s, item); \
    return result_bool_constcharp_is_ok(r); \
} \
\
/** \
 * @brief Pops the top item from the stack \
 * @param s    Valid stack instance \
 * @param out  Pointer to receive the popped value \
 * @return     true if popped successfully, false if stack is empty or invalid \
 */ \
static inline bool stack_##Type##_pop(stack_##Type* s, Type* out) { \
    return result_bool_constcharp_is_ok(vec_##Type##_pop(s, out)); \
}

#endif /* CANON_DATA_STACK_H */
