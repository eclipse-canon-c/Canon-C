#ifndef CANON_DATA_STACK_H
#define CANON_DATA_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include "semantics/result.h"
#include "semantics/error.h"
#include "vec.h"

/**
 * @file stack.h
 * @brief Simple LIFO (Last-In-First-Out) stack – thin wrapper over vec
 *
 * Provides a clean, intention-revealing stack interface using the same fixed-capacity
 * buffer as `vec`. It is **not** a growable container.
 *
 * Portability:
 * - Requires C99 or later (for inline functions, stdbool.h)
 * - Depends on vec.h, result.h, error.h from this library
 * - No platform-specific code
 *
 * Thread-safety: Each stack instance is independent - not thread-safe for
 * concurrent modifications. Caller must synchronize if needed.
 *
 * Performance:
 * - Zero overhead abstraction - direct passthrough to vec operations
 * - Push/pop are O(1)
 * - All query operations are O(1)
 * - No hidden allocations
 *
 * CRITICAL DESIGN NOTE
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
 * - Maximum predictability
 * - Zero surprise allocations (ideal for real-time, embedded, arena usage)
 * - Strong ownership & lifetime control
 * - Deterministic performance
 *
 * If you need a stack that grows automatically:
 * → Use data/convenience/dynvec.h as backing storage
 * → Or implement your own wrapper with growth strategy
 *
 * Key Properties
 * ──────────────────────────────────────────────────────────────────────────────
 * • Zero runtime overhead – most operations are direct vec calls
 * • Bounds-checked push/pop with clear success/failure
 * • Caller fully owns the underlying buffer (stack, arena, static, heap...)
 * • Type-safe via macro (DEFINE_STACK)
 * • Perfect for: undo systems, expression evaluation, call stack simulation,
 * backtracking algorithms, recursive descent parsers
 *
 * Recommended Usage Patterns
 * ──────────────────────────────────────────────────────────────────────────────
 *
 * // 1. Stack-allocated – zero dynamic allocation
 * DEFINE_STACK(int);
 * int buf[128];
 * stack_int undo_stack;
 * stack_int_init(&undo_stack, buf, 128);
 *
 * stack_int_push(&undo_stack, 42);
 * int value;
 * if (stack_int_pop(&undo_stack, &value)) {
 *     printf("Popped: %d\n", value);
 * }
 *
 * // 2. Arena-backed – explicit lifetime
 * int* arena_buf = arena_alloc_array(&my_arena, int, 256);
 * stack_int history;
 * stack_int_init(&history, arena_buf, 256);
 *
 * // 3. Pre-check before bulk pushes (recommended)
 * if (stack_int_remaining(&s) < items_to_add) {
 *     return ERROR_NOT_ENOUGH_SPACE;
 * }
 * for (int i = 0; i < items_to_add; i++) {
 *     stack_int_push(&s, items[i]); // Guaranteed to succeed
 * }
 *
 * // 4. Error handling
 * if (!stack_int_push(&s, value)) {
 *     fprintf(stderr, "Stack full (%zu/%zu)\n",
 *             stack_int_len(&s), stack_int_capacity(&s));
 * }
 */

/**
 * @brief Define a type-safe fixed-capacity stack for any element type
 *
 * Generates a complete stack implementation for the specified type,
 * including type definition and all associated functions.
 *
 * This is a thin wrapper over vec_##Type - all operations delegate to
 * the underlying vector with LIFO semantics.
 *
 * @param Type Any complete type (int, float, struct Point, etc.)
 *
 * Generated type: stack_##Type
 * Generated functions:
 * - stack_##Type##_init(s, buffer, capacity) - Initialize stack
 * - stack_##Type##_push(s, item) - Push item onto stack
 * - stack_##Type##_pop(s, &out) - Pop item from stack
 * - stack_##Type##_peek(s, &out) - View top without popping
 * - stack_##Type##_peek_option(s) - View top as Option<Type>
 * - stack_##Type##_len(s) - Get current size
 * - stack_##Type##_capacity(s) - Get maximum capacity
 * - stack_##Type##_remaining(s) - Get free space
 * - stack_##Type##_is_empty(s) - Check if empty
 * - stack_##Type##_is_full(s) - Check if full
 * - stack_##Type##_clear(s) - Remove all elements
 *
 * Precondition: DEFINE_VEC(Type) must have been called first
 *
 * Usage:
 * DEFINE_VEC(int) // Required first
 * DEFINE_STACK(int) // Then define stack
 *
 * Example:
 * DEFINE_VEC(int);
 * DEFINE_STACK(int);
 *
 * int buf[64];
 * stack_int s;
 * stack_int_init(&s, buf, 64);
 *
 * stack_int_push(&s, 10);
 * stack_int_push(&s, 20);
 *
 * int top;
 * if (stack_int_peek(&s, &top)) {
 *     printf("Top: %d\n", top); // 20
 * }
 *
 * stack_int_pop(&s, &top);
 * printf("Popped: %d\n", top); // 20
 *
 * Note: This must be used at file or global scope, not inside functions.
 */
#define DEFINE_STACK(Type) \
\
/** \
 * @brief Fixed-capacity LIFO stack for Type \
 * \
 * This is a type alias for vec_##Type - all vector operations are available. \
 * Use stack-specific functions for clearer intent. \
 */ \
typedef vec_##Type stack_##Type; \
\
/** \
 * @brief Initializes the stack with caller-provided fixed buffer \
 * \
 * @param s Pointer to uninitialized stack_##Type \
 * @param buffer Array of Type – must remain valid for stack lifetime \
 * @param capacity Maximum number of elements the buffer can hold \
 * \
 * Preconditions: \
 * - s != NULL \
 * - buffer != NULL (or capacity == 0) \
 * - buffer has space for capacity elements \
 * \
 * Postconditions: \
 * - s is initialized as empty stack \
 * - s can hold up to capacity elements \
 * \
 * Note: Capacity is fixed forever – no growth possible later \
 * \
 * Example: \
 * int buf[100]; \
 * stack_int s; \
 * stack_int_init(&s, buf, 100); \
 */ \
static inline void stack_##Type##_init(stack_##Type* s, Type* buffer, size_t capacity) { \
    assert(s != NULL && "stack_init: s parameter cannot be NULL"); \
    assert(buffer != NULL || capacity == 0); \
    *s = vec_##Type##_init(buffer, capacity); \
} \
\
/** \
 * @brief Pushes an item onto the top of the stack \
 * \
 * @param s Valid stack instance \
 * @param item Value to push \
 * @return true on success, false if stack is full or invalid \
 * \
 * Returns false if: \
 * - s is NULL or invalid \
 * - Stack is full (len >= capacity) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 * if (!stack_int_push(&s, 42)) { \
 *     fprintf(stderr, "Stack overflow\n"); \
 * } \
 */ \
static inline bool stack_##Type##_push(stack_##Type* s, Type item) { \
    result_bool_Error r = vec_##Type##_push(s, item); \
    return result_bool_Error_is_ok(r); \
} \
\
/** \
 * @brief Pops the top item from the stack \
 * \
 * Removes and returns the most recently pushed item. \
 * \
 * @param s Valid stack instance \
 * @param out Pointer to store the popped value (NULL-safe) \
 * @return true on success, false if stack is empty or invalid \
 * \
 * Returns false if: \
 * - s is NULL or invalid \
 * - out is NULL \
 * - Stack is empty (len == 0) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 * int value; \
 * if (stack_int_pop(&s, &value)) { \
 *     printf("Popped: %d\n", value); \
 * } else { \
 *     fprintf(stderr, "Stack underflow\n"); \
 * } \
 */ \
static inline bool stack_##Type##_pop(stack_##Type* s, Type* out) { \
    result_bool_Error r = vec_##Type##_pop(s, out); \
    return result_bool_Error_is_ok(r); \
} \
\
/** \
 * @brief Peeks at the top item without removing it \
 * \
 * Returns the value that would be popped, without modifying the stack. \
 * \
 * @param s Valid stack instance \
 * @param out Pointer to store the top value (NULL-safe) \
 * @return true on success, false if stack is empty or invalid \
 * \
 * Returns false if: \
 * - s is NULL or invalid \
 * - out is NULL \
 * - Stack is empty (len == 0) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 * int top; \
 * if (stack_int_peek(&s, &top)) { \
 *     printf("Top is %d (not removed)\n", top); \
 * } \
 */ \
static inline bool stack_##Type##_peek(const stack_##Type* s, Type* out) { \
    if (!s || !out || vec_##Type##_is_empty(s)) { \
        return false; \
    } \
    Type* last = vec_##Type##_last(s); \
    if (!last) { \
        return false; \
    } \
    *out = *last; \
    return true; \
} \
\
/** \
 * @brief Peeks at the top item and returns it as Option<Type> \
 * \
 * Safe alternative to peek() — returns None if empty or invalid. \
 * \
 * @param s Valid stack instance \
 * @return Some(top) if stack not empty, None otherwise \
 * \
 * Performance: O(1) \
 */ \
static inline option_##Type stack_##Type##_peek_option(const stack_##Type* s) { \
    Type val; \
    if (stack_##Type##_peek(s, &val)) { \
        return option_##Type##_some(val); \
    } \
    return option_##Type##_none(); \
} \
\
/** \
 * @brief Returns current number of elements in the stack \
 * \
 * @param s Stack to query (NULL-safe) \
 * @return Number of elements currently on stack \
 * \
 * Performance: O(1) \
 */ \
static inline size_t stack_##Type##_len(const stack_##Type* s) { \
    return vec_##Type##_len(s); \
} \
\
/** \
 * @brief Returns maximum capacity of the stack (fixed) \
 * \
 * @param s Stack to query (NULL-safe) \
 * @return Maximum number of elements stack can hold \
 * \
 * Performance: O(1) \
 */ \
static inline size_t stack_##Type##_capacity(const stack_##Type* s) { \
    return vec_##Type##_capacity(s); \
} \
\
/** \
 * @brief Returns number of free slots remaining \
 * \
 * Equivalent to capacity - len. \
 * \
 * @param s Stack to query (NULL-safe) \
 * @return Number of elements that can still be pushed \
 * \
 * Example: \
 * if (stack_int_remaining(&s) < 10) { \
 *     fprintf(stderr, "Warning: stack nearly full\n"); \
 * } \
 * \
 * Performance: O(1) \
 */ \
static inline size_t stack_##Type##_remaining(const stack_##Type* s) { \
    return vec_##Type##_remaining(s); \
} \
\
/** \
 * @brief Checks if stack is empty \
 * \
 * @param s Stack to check (NULL-safe) \
 * @return true if len == 0, false otherwise \
 * \
 * Performance: O(1) \
 */ \
static inline bool stack_##Type##_is_empty(const stack_##Type* s) { \
    return vec_##Type##_is_empty(s); \
} \
\
/** \
 * @brief Checks if stack is full (cannot push more) \
 * \
 * @param s Stack to check (NULL-safe) \
 * @return true if len >= capacity, false otherwise \
 * \
 * Performance: O(1) \
 */ \
static inline bool stack_##Type##_is_full(const stack_##Type* s) { \
    return vec_##Type##_is_full(s); \
} \
\
/** \
 * @brief Clears the stack (sets length to 0, buffer unchanged) \
 * \
 * Does not modify buffer contents, only resets length to 0. \
 * All previously pushed values become invalid. \
 * \
 * @param s Stack to clear \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 * stack_int_clear(&s); // Reuse stack for new sequence \
 */ \
static inline void stack_##Type##_clear(stack_##Type* s) { \
    vec_##Type##_clear(s); \
}

/* Optional alias for clearer LIFO naming — common in stack APIs */
/* #define stack_##Type##_top stack_##Type##_peek */
/* #define stack_##Type##_top_option stack_##Type##_peek_option */

/* ────────────────────────────────────────────────────────────────────────────
   Common type instantiations
   ────────────────────────────────────────────────────────────────────────────

   Uncomment after defining the corresponding vec types:

   DEFINE_VEC(int)
   DEFINE_STACK(int)

   DEFINE_VEC(char)
   DEFINE_STACK(char)

   DEFINE_VEC(size_t)
   DEFINE_STACK(size_t)

   typedef void* voidptr;
   DEFINE_VEC(voidptr)
   DEFINE_STACK(voidptr)

   ──────────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────
    #include "stack.h"
    #include <stdio.h>

    // Define types
    DEFINE_VEC(int)
    DEFINE_STACK(int)

    // Example 1: Basic stack operations
    void example_basic(void) {
        int buffer[32];
        stack_int s;
        stack_int_init(&s, buffer, 32);

        // Push elements
        stack_int_push(&s, 10);
        stack_int_push(&s, 20);
        stack_int_push(&s, 30);

        printf("Stack size: %zu\n", stack_int_len(&s));

        // Peek at top
        int top;
        if (stack_int_peek(&s, &top)) {
            printf("Top element: %d\n", top); // 30
        }

        // Pop elements
        while (!stack_int_is_empty(&s)) {
            int value;
            stack_int_pop(&s, &value);
            printf("Popped: %d\n", value);
        }
    }

    // Example 2: Expression evaluation (reverse Polish notation)
    int evaluate_rpn(const char* expr) {
        int buffer[64];
        stack_int s;
        stack_int_init(&s, buffer, 64);

        // Parse and evaluate: "3 4 + 2 *" = (3+4)*2 = 14
        for (const char* p = expr; *p; p++) {
            if (*p >= '0' && *p <= '9') {
                stack_int_push(&s, *p - '0');
            } else if (*p == '+') {
                int b, a;
                stack_int_pop(&s, &b);
                stack_int_pop(&s, &a);
                stack_int_push(&s, a + b);
            } else if (*p == '*') {
                int b, a;
                stack_int_pop(&s, &b);
                stack_int_pop(&s, &a);
                stack_int_push(&s, a * b);
            }
        }

        int result;
        stack_int_pop(&s, &result);
        return result;
    }

    // Example 3: Undo system
    typedef struct {
        int type;
        int data;
    } Action;

    DEFINE_VEC(Action)
    DEFINE_STACK(Action)

    void example_undo_system(void) {
        Action buffer[100];
        stack_Action undo;
        stack_Action_init(&undo, buffer, 100);

        // Perform actions
        stack_Action_push(&undo, (Action){.type = 1, .data = 42});
        stack_Action_push(&undo, (Action){.type = 2, .data = 99});

        // Undo last action
        Action last;
        if (stack_Action_pop(&undo, &last)) {
            printf("Undoing action type %d\n", last.type);
        }
    }

    // Example 4: Error handling
    void example_error_handling(void) {
        int small_buf[2];
        stack_int s;
        stack_int_init(&s, small_buf, 2);

        // Fill stack
        stack_int_push(&s, 1);
        stack_int_push(&s, 2);

        // This will fail - stack is full
        if (!stack_int_push(&s, 3)) {
            fprintf(stderr, "Stack overflow: %zu/%zu elements\n",
                    stack_int_len(&s),
                    stack_int_capacity(&s));
        }

        // Check before push
        if (stack_int_remaining(&s) > 0) {
            stack_int_push(&s, 4);
        } else {
            fprintf(stderr, "No space remaining\n");
        }
    }

    // Example 5: Arena-backed stack
    void example_arena(Arena* arena) {
        int* buf = arena_alloc_array(arena, int, 256);
        stack_int s;
        stack_int_init(&s, buf, 256);

        // Use stack...
        for (int i = 0; i < 100; i++) {
            stack_int_push(&s, i);
        }

        // Stack lifetime tied to arena
        // Becomes invalid after arena_reset()
    }
   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_DATA_STACK_H */
