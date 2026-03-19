#ifndef CANON_DATA_STACK_H
#define CANON_DATA_STACK_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "semantics/option/option.h"
#include "data/vec/vec.h"

/**
 * @file stack.h
 * @brief Bounded LIFO stack — thin wrapper over canon_vec_##type
 *
 * Provides a clean, intention-revealing LIFO interface with fixed capacity.
 * All operations delegate directly to the underlying vec with zero overhead.
 * push = vec_push, pop = vec_pop, peek = vec_last.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero overhead — every function is a direct vec passthrough
 * - LIFO semantics only — no enqueue, no front-access (use deque for that)
 * - result_bool_Error from push/pop — matches vec and queue
 * - Option variants for pop and peek — no out-param required
 * - Caller owns the buffer (stack memory, arena, static, heap)
 * - Fixed capacity is intentional — real-time safe, deterministic
 * - All pointer parameters annotated with borrowed() per ownership.h conventions
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * stack.h is data/. It wraps data/vec/vec.h.
 * DEFINE_VEC(linkage, type) must be called before DEFINE_STACK(linkage, type).
 *
 * Include dependencies (direct only):
 * ────────────────────────────────────────────────────────────────────────────
 * - core/primitives/types.h    — usize and bool used directly in DECLARE_STACK
 *                                extern signatures and peek's return type
 * - core/primitives/contract.h — require_msg used directly in init and peek
 * - core/ownership.h           — borrowed() used directly in all generated
 *                                function signatures and DECLARE_STACK externs
 * - semantics/option/option.h  — option_##type_some/none called directly in
 *                                peek_option; type named in pop_option and
 *                                peek_option signatures; not transitive via vec.h
 * - data/vec/vec.h             — all MANGLE_VEC_* macros used directly
 *
 * Intentionally excluded:
 * - semantics/result/result.h  — result_bool_Error is named in push/pop
 *                                signatures but the type is already instantiated
 *                                transitively by vec_impl.h via CANON_RESULT.
 *                                stack.h never calls CANON_RESULT itself.
 * - semantics/error.h          — no ERR_* codes referenced directly
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each stack instance is independent — no shared state.
 * Concurrent modifications require external synchronization.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends on data/vec/vec.h
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - push / pop:                      O(1) — no allocation
 * - peek / peek_option:              O(1)
 * - len / capacity / remaining:      O(1)
 * - is_empty / is_full:              O(1)
 * - clear:                           O(1)
 * - struct size:                     same as canon_vec_##type (typedef alias)
 * - element overhead:                0 bytes beyond sizeof(type) per element
 *
 * Quick start:
 * ```c
 * #include "data/stack.h"
 *
 * // Vec and Option must be instantiated first
 * CANON_OPTION(int)
 * DEFINE_VEC(static inline, int)
 * DEFINE_STACK(static inline, int)
 *
 * // Stack-allocated buffer backing a LIFO stack
 * int buf[128];
 * canon_stack_int s;
 * canon_stack_int_init(&s, buf, 128);
 *
 * canon_stack_int_push(&s, 10);
 * canon_stack_int_push(&s, 20);
 *
 * int val;
 * canon_stack_int_pop(&s, &val);   // val = 20 (LIFO)
 *
 * // Option variant — no out-param needed
 * option_int top = canon_stack_int_pop_option(&s);   // Some(10)
 *
 * // Peek without popping
 * option_int peek = canon_stack_int_peek_option(&s);
 * ```
 *
 * Separate compilation:
 * ```c
 * // In tasks.h:
 * #include "data/vec/vec_decl.h"
 * #include "data/stack.h"
 * DECLARE_VEC(Task)
 * DECLARE_STACK(Task)
 *
 * // In tasks.c:
 * #include "data/vec/vec_defn.h"
 * #include "data/stack.h"
 * CANON_OPTION(Task)
 * DEFINE_VEC(, Task)
 * DEFINE_STACK(, Task)
 * ```
 *
 * Common use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Undo/redo history
 * - Expression evaluation (RPN, operator precedence)
 * - Recursive descent parser state
 * - Backtracking algorithms (DFS frontier)
 * - Call stack simulation
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - FIFO access (use queue.h)
 * - Double-ended access (use deque.h)
 * - Random access by index (use vec directly)
 * - Auto-growing containers (use data/convenience/dynvec.h)
 * - Multi-threaded access without external synchronization
 *
 * @sa data/vec/vec.h, data/queue.h, data/deque/deque.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_STACK — emit all typedefs and functions for a typed LIFO stack
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a typed LIFO stack as a thin wrapper over canon_vec_##type
 *
 * Generated type (typedef alias, no new struct):
 * - canon_stack_##type  (alias for canon_vec_##type)
 *
 * Generated functions:
 *
 * Constructor:
 * - canon_stack_##type##_init(s, buffer, capacity)  → void
 *
 * Queries:
 * - canon_stack_##type##_len(s)                     → usize
 * - canon_stack_##type##_capacity(s)                → usize
 * - canon_stack_##type##_remaining(s)               → usize
 * - canon_stack_##type##_is_empty(s)                → bool
 * - canon_stack_##type##_is_full(s)                 → bool
 *
 * Push / pop (Result variants):
 * - canon_stack_##type##_push(s, item)              → result_bool_Error
 * - canon_stack_##type##_pop(s, out)                → result_bool_Error
 *
 * Pop (Option variant):
 * - canon_stack_##type##_pop_option(s)              → option_##type
 *
 * Peek (bool + out-param variant):
 * - canon_stack_##type##_peek(s, out)               → bool
 *
 * Peek (Option variant — preferred when caller has not checked is_empty):
 * - canon_stack_##type##_peek_option(s)             → option_##type
 *
 * Misc:
 * - canon_stack_##type##_clear(s)                   → void
 *
 * @param linkage C linkage specifier: `static inline`, `static`, or empty
 * @param type    Element type — DEFINE_VEC(linkage, type) must be called first
 *
 * @pre CANON_OPTION(type) has already been called for the same type
 * @pre DEFINE_VEC(linkage, type) has already been called for the same type
 *
 * @note canon_stack_##type is a typedef alias for canon_vec_##type — all vec
 *       functions remain usable. DEFINE_STACK only adds LIFO-named wrappers
 *       for clarity.
 */
#define DEFINE_STACK(linkage, type) \
\
/** \
 * @brief Fixed-capacity LIFO stack for type \
 * \
 * Typedef alias for canon_vec_##type. \
 * All canon_vec_##type functions are available; \
 * use canon_stack_##type functions for LIFO-specific clarity. \
 */ \
typedef MANGLE_VEC_TYPE(type) canon_stack_##type; \
\
/** \
 * @brief Initializes the stack with a caller-owned buffer \
 * \
 * @param s        borrowed(canon_stack_##type*) — must not be NULL \
 * @param buffer   borrowed(type*) — must remain valid for stack lifetime \
 * @param capacity Maximum number of elements \
 * \
 * @pre s != NULL \
 * @pre buffer != NULL || capacity == 0 \
 * @pre capacity > 0 \
 * \
 * @post Stack is empty, ready for push/pop \
 * @post Capacity is fixed — no growth possible \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) — wraps caller-provided buffer \
 */ \
linkage void canon_stack_##type##_init( \
    borrowed(canon_stack_##type*) s, borrowed(type*) buffer, usize capacity) \
{ \
    require_msg(s != NULL, "canon_stack_" #type "_init: s cannot be NULL"); \
    *s = MANGLE_VEC_INIT(type)(buffer, capacity); \
} \
\
/** \
 * @brief Pushes an item onto the top of the stack \
 * \
 * @param s    borrowed(canon_stack_##type*) — initialized stack instance \
 * @param item Value to push \
 * @return result_bool_Error — Ok(true) on success \
 * \
 * @post Returns Err(ERR_INVALID_ARG)       if s == NULL or s->items == NULL \
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if stack is full \
 * \
 * Performance: \
 * - Time:  O(1) — no allocation \
 * - Space: O(1) \
 */ \
linkage result_bool_Error canon_stack_##type##_push( \
    borrowed(canon_stack_##type*) s, type item) \
{ \
    return MANGLE_VEC_PUSH(type)(s, item); \
} \
\
/** \
 * @brief Removes and returns the top item (LIFO order) \
 * \
 * @param s   borrowed(canon_stack_##type*) — initialized stack instance \
 * @param out borrowed(type*) — pointer to store the popped value \
 * @return result_bool_Error — Ok(true) on success \
 * \
 * @post Returns Err(ERR_INVALID_ARG)   if s == NULL, out == NULL, \
 *       or s->items == NULL \
 * @post Returns Err(ERR_INVALID_STATE) if stack is empty \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage result_bool_Error canon_stack_##type##_pop( \
    borrowed(canon_stack_##type*) s, borrowed(type*) out) \
{ \
    return MANGLE_VEC_POP(type)(s, out); \
} \
\
/** \
 * @brief Removes and returns the top item as Option<type> \
 * \
 * @param s borrowed(canon_stack_##type*) — initialized stack instance \
 * @return option_##type — Some(item) on success, None if empty or invalid \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage option_##type canon_stack_##type##_pop_option( \
    borrowed(canon_stack_##type*) s) \
{ \
    return MANGLE_VEC_POP_OPTION(type)(s); \
} \
\
/** \
 * @brief Returns the top item without removing it \
 * \
 * Use peek_option() when the caller has not already confirmed the stack \
 * is non-empty. Use peek() as a fast path after an is_empty() check. \
 * \
 * @param s   borrowed(const canon_stack_##type*) — must not be NULL \
 * @param out borrowed(type*) — must not be NULL \
 * @return true if a value was written to *out, false if stack is empty \
 * \
 * @pre s   != NULL (programming error — triggers require_msg) \
 * @pre out != NULL (programming error — triggers require_msg) \
 * \
 * @post Stack is unchanged \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool canon_stack_##type##_peek( \
    borrowed(const canon_stack_##type*) s, borrowed(type*) out) \
{ \
    require_msg(s   != NULL, "canon_stack_" #type "_peek: s cannot be NULL"); \
    require_msg(out != NULL, "canon_stack_" #type "_peek: out cannot be NULL"); \
    if (MANGLE_VEC_IS_EMPTY(type)(s)) return false; \
    type* last = MANGLE_VEC_LAST(type)(s); \
    if (!last) return false; \
    *out = *last; \
    return true; \
} \
\
/** \
 * @brief Returns the top item as Option<type> without removing it \
 * \
 * Preferred over peek() when the caller has not already confirmed the \
 * stack is non-empty. Returns None cleanly instead of requiring a \
 * prior is_empty() check. \
 * \
 * @param s borrowed(const canon_stack_##type*) — must not be NULL \
 * @return option_##type — Some(top) if non-empty, None if empty \
 * \
 * @pre s != NULL (programming error — triggers require_msg via peek) \
 * \
 * @post Stack is unchanged \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage option_##type canon_stack_##type##_peek_option( \
    borrowed(const canon_stack_##type*) s) \
{ \
    type val; \
    if (canon_stack_##type##_peek(s, &val)) \
        return option_##type##_some(val); \
    return option_##type##_none(); \
} \
\
/** \
 * @brief Returns the current number of elements \
 * \
 * @param s borrowed(const canon_stack_##type*) — NULL-safe, returns 0 if NULL \
 * @return usize \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize canon_stack_##type##_len( \
    borrowed(const canon_stack_##type*) s) \
{ \
    return MANGLE_VEC_LEN(type)(s); \
} \
\
/** \
 * @brief Returns the fixed maximum capacity \
 * \
 * @param s borrowed(const canon_stack_##type*) — NULL-safe, returns 0 if NULL \
 * @return usize \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize canon_stack_##type##_capacity( \
    borrowed(const canon_stack_##type*) s) \
{ \
    return MANGLE_VEC_CAPACITY(type)(s); \
} \
\
/** \
 * @brief Returns remaining free slots (capacity - len) \
 * \
 * @param s borrowed(const canon_stack_##type*) — NULL-safe, returns 0 if NULL \
 * @return usize \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize canon_stack_##type##_remaining( \
    borrowed(const canon_stack_##type*) s) \
{ \
    return MANGLE_VEC_REMAINING(type)(s); \
} \
\
/** \
 * @brief Returns true if the stack has no elements (len == 0) \
 * \
 * @param s borrowed(const canon_stack_##type*) — NULL-safe, returns true if NULL \
 * @return bool \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool canon_stack_##type##_is_empty( \
    borrowed(const canon_stack_##type*) s) \
{ \
    return MANGLE_VEC_IS_EMPTY(type)(s); \
} \
\
/** \
 * @brief Returns true if the stack is at capacity (len == capacity) \
 * \
 * @param s borrowed(const canon_stack_##type*) — NULL-safe, returns true if NULL \
 * @return bool \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool canon_stack_##type##_is_full( \
    borrowed(const canon_stack_##type*) s) \
{ \
    return MANGLE_VEC_IS_FULL(type)(s); \
} \
\
/** \
 * @brief Resets the stack to empty state (O(1), does not zero buffer) \
 * \
 * @param s borrowed(canon_stack_##type*) — NULL-safe \
 * \
 * @post s->len == 0 \
 * @post Buffer contents are NOT zeroed — only logical state is reset \
 * @note For sensitive data, zero the buffer manually before calling clear \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage void canon_stack_##type##_clear(borrowed(canon_stack_##type*) s) { \
    MANGLE_VEC_CLEAR(type)(s); \
}

/* ════════════════════════════════════════════════════════════════════════════
   DECLARE_STACK — forward-declare a typed stack (for separate compilation)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Forward-declares a typed stack and all its function signatures
 *
 * Use in shared headers alongside DECLARE_VEC(type).
 * Match with DEFINE_STACK(linkage, type) in exactly one .c file.
 *
 * @param type Element type — DECLARE_VEC(type) must be called first
 *
 * @pre DECLARE_VEC(type) has already been called for the same type
 * @pre option_##type is available (from CANON_OPTION(type))
 *
 * Example:
 * ```c
 * // In tasks.h:
 * #include "data/vec/vec_decl.h"
 * #include "data/stack.h"
 * DECLARE_VEC(Task)
 * DECLARE_STACK(Task)
 *
 * // In tasks.c:
 * #include "data/vec/vec_defn.h"
 * #include "data/stack.h"
 * CANON_OPTION(Task)
 * DEFINE_VEC(, Task)
 * DEFINE_STACK(, Task)
 * ```
 */
#define DECLARE_STACK(type) \
\
typedef MANGLE_VEC_TYPE(type) canon_stack_##type; \
\
extern void              canon_stack_##type##_init(borrowed(canon_stack_##type*) s, borrowed(type*) buffer, usize capacity); \
extern result_bool_Error canon_stack_##type##_push(borrowed(canon_stack_##type*) s, type item); \
extern result_bool_Error canon_stack_##type##_pop(borrowed(canon_stack_##type*) s, borrowed(type*) out); \
extern option_##type     canon_stack_##type##_pop_option(borrowed(canon_stack_##type*) s); \
extern bool              canon_stack_##type##_peek(borrowed(const canon_stack_##type*) s, borrowed(type*) out); \
extern option_##type     canon_stack_##type##_peek_option(borrowed(const canon_stack_##type*) s); \
extern usize             canon_stack_##type##_len(borrowed(const canon_stack_##type*) s); \
extern usize             canon_stack_##type##_capacity(borrowed(const canon_stack_##type*) s); \
extern usize             canon_stack_##type##_remaining(borrowed(const canon_stack_##type*) s); \
extern bool              canon_stack_##type##_is_empty(borrowed(const canon_stack_##type*) s); \
extern bool              canon_stack_##type##_is_full(borrowed(const canon_stack_##type*) s); \
extern void              canon_stack_##type##_clear(borrowed(canon_stack_##type*) s);

#endif /* CANON_DATA_STACK_H */
