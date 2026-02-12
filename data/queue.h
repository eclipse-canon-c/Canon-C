#ifndef CANON_DATA_QUEUE_H
#define CANON_DATA_QUEUE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "semantics/result.h"
#include "semantics/error.h"
#include "semantics/option.h"
#include "data/deque/deque.h"

/**
 * @file queue.h
 * @brief Bounded FIFO queue — thin wrapper over canon_deque_##type
 *
 * Provides a clean, intention-revealing FIFO interface with fixed capacity.
 * All operations delegate directly to the underlying deque with zero overhead.
 * enqueue = push_back, dequeue = pop_front, peek = peek_front.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero overhead — every function is a direct deque passthrough
 * - FIFO semantics only — no front-insert, no back-pop (use deque for that)
 * - Result<bool, Error> from enqueue/dequeue — matches deque and vec
 * - Option variants for dequeue and peek — no out-param required
 * - Caller owns the buffer (stack, arena, static, heap)
 * - Fixed capacity is intentional — real-time safe, deterministic
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * queue.h is data/. It wraps data/deque/deque.h.
 * DEFINE_DEQUE(linkage, type) must be called before DEFINE_QUEUE(linkage, type).
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each queue instance is independent — no shared state.
 * Concurrent modifications require external synchronization.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends on data/deque/deque.h
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - enqueue / dequeue:       O(1) — ring buffer step, no allocation
 * - peek:                    O(1)
 * - len / capacity / remaining / is_empty / is_full: O(1)
 * - clear:                   O(1)
 * - struct size:             same as canon_deque_##type (typedef alias)
 * - element overhead:        0 bytes beyond sizeof(type) per element
 *
 * Quick start:
 * ```c
 * #include "data/queue.h"
 *
 * // Deque must be instantiated first
 * DEFINE_DEQUE(static inline, int)
 * DEFINE_QUEUE(static inline, int)
 *
 * // Stack-backed queue
 * int buf[128];
 * canon_queue_int q;
 * canon_queue_int_init(&q, buf, 128);
 *
 * canon_queue_int_enqueue(&q, 10);
 * canon_queue_int_enqueue(&q, 20);
 *
 * int val;
 * canon_queue_int_dequeue(&q, &val);  // val = 10 (FIFO)
 *
 * // Option variant — no out param needed
 * option_int next = canon_queue_int_dequeue_option(&q);  // Some(20)
 *
 * // Peek without removing
 * option_int front = canon_queue_int_peek_option(&q);
 * ```
 *
 * Separate compilation:
 * ```c
 * // In tasks.h:
 * #include "data/deque/deque_decl.h"
 * #include "data/queue.h"
 * DECLARE_DEQUE(Task)
 * DECLARE_QUEUE(Task)
 *
 * // In tasks.c:
 * #include "data/deque/deque_defn.h"
 * #include "data/queue.h"
 * DEFINE_DEQUE(, Task)
 * DEFINE_QUEUE(, Task)
 * ```
 *
 * Common use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Task dispatch queues
 * - BFS frontier buffers
 * - Message passing between components
 * - Producer-consumer bounded buffers
 * - Rate-limited event queues
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Double-ended access (use deque directly)
 * - Random access by index (use vec)
 * - Auto-growing containers
 * - Multi-threaded access without external synchronization
 *
 * @sa data/deque/deque.h, data/stack.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_QUEUE — emit all typedefs and functions for a typed FIFO queue
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a typed FIFO queue as a thin wrapper over canon_deque_##type
 *
 * Generated type (typedef alias, no new struct):
 * - canon_queue_##type  (alias for canon_deque_##type)
 *
 * Generated functions:
 *
 * Constructor:
 * - canon_queue_##type##_init(q, buffer, capacity)  → void
 *
 * Queries:
 * - canon_queue_##type##_len(q)                     → usize
 * - canon_queue_##type##_capacity(q)                → usize
 * - canon_queue_##type##_remaining(q)               → usize
 * - canon_queue_##type##_is_empty(q)                → bool
 * - canon_queue_##type##_is_full(q)                 → bool
 *
 * Enqueue / dequeue (Result variants):
 * - canon_queue_##type##_enqueue(q, item)           → result_bool_Error
 * - canon_queue_##type##_dequeue(q, out)            → result_bool_Error
 *
 * Dequeue (Option variant):
 * - canon_queue_##type##_dequeue_option(q)          → option_##type
 *
 * Peek (bool variant):
 * - canon_queue_##type##_peek(q, out)               → bool
 *
 * Peek (Option variant):
 * - canon_queue_##type##_peek_option(q)             → option_##type
 *
 * Misc:
 * - canon_queue_##type##_clear(q)                   → void
 *
 * @param linkage C linkage specifier: `static inline`, `static`, or empty
 * @param type    Element type — DEFINE_DEQUE(linkage, type) must be called first
 *
 * @pre DEFINE_DEQUE(linkage, type) has already been called for the same type
 *
 * @note queue is a typedef alias for deque — all deque functions remain usable.
 *       DEFINE_QUEUE only adds the FIFO-named wrappers for clarity.
 */
#define DEFINE_QUEUE(linkage, type) \
\
/** \
 * @brief Fixed-capacity FIFO queue for type \
 * \
 * Typedef alias for canon_deque_##type. \
 * All canon_deque_##type functions are available; \
 * use canon_queue_##type functions for FIFO-specific clarity. \
 */ \
typedef MANGLE_DEQUE_TYPE(type) canon_queue_##type; \
\
/** \
 * @brief Initializes the queue with a caller-owned buffer \
 * \
 * @param q        Pointer to uninitialized canon_queue_##type \
 * @param buffer   Array of type — must remain valid for queue lifetime \
 * @param capacity Maximum number of elements (> 0) \
 * \
 * @pre q != NULL \
 * @pre buffer != NULL || capacity == 0 \
 * @pre capacity > 0 \
 * \
 * @post Queue is empty, ready for enqueue/dequeue \
 * @post Capacity is fixed — no growth possible \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) — wraps caller-provided buffer \
 */ \
linkage void canon_queue_##type##_init(canon_queue_##type* q, type* buffer, usize capacity) { \
    require_msg(q != NULL, "canon_queue_" #type "_init: q cannot be NULL"); \
    MANGLE_DEQUE_INIT(type)(q, buffer, capacity); \
} \
\
/** \
 * @brief Adds an item to the back of the queue (enqueue) \
 * \
 * FIFO ordering: items are dequeued in the order they were enqueued. \
 * \
 * @param q    Valid queue instance \
 * @param item Value to add \
 * @return result_bool_Error — Ok(true) on success \
 * \
 * @post Returns Err(ERR_INVALID_ARG)       if q == NULL or q->buffer == NULL \
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if queue is full \
 * \
 * Performance: \
 * - Time:  O(1) — ring buffer tail advance \
 * - Space: O(1) — no allocation \
 */ \
linkage result_bool_Error canon_queue_##type##_enqueue(canon_queue_##type* q, type item) { \
    return MANGLE_DEQUE_PUSH_BACK(type)(q, item); \
} \
\
/** \
 * @brief Removes and returns the front item (dequeue) \
 * \
 * Removes the oldest element — the one that was enqueued first. \
 * \
 * @param q   Valid queue instance \
 * @param out Pointer to store the dequeued value \
 * @return result_bool_Error — Ok(true) on success \
 * \
 * @pre out != NULL \
 * \
 * @post Returns Err(ERR_INVALID_ARG)   if q == NULL, out == NULL, or q->buffer == NULL \
 * @post Returns Err(ERR_INVALID_STATE) if queue is empty \
 * \
 * Performance: \
 * - Time:  O(1) — ring buffer head advance \
 * - Space: O(1) \
 */ \
linkage result_bool_Error canon_queue_##type##_dequeue(canon_queue_##type* q, type* out) { \
    return MANGLE_DEQUE_POP_FRONT(type)(q, out); \
} \
\
/** \
 * @brief Removes and returns the front item as Option<type> \
 * \
 * @param q Valid queue instance \
 * @return option_##type — Some(item) on success, None if empty or invalid \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage option_##type canon_queue_##type##_dequeue_option(canon_queue_##type* q) { \
    return MANGLE_DEQUE_POP_FRONT_OPTION(type)(q); \
} \
\
/** \
 * @brief Returns the front item without removing it \
 * \
 * @param q   Valid queue instance \
 * @param out Pointer to store the front value \
 * @return true on success, false if empty or invalid \
 * \
 * @post Queue is unchanged \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool canon_queue_##type##_peek(const canon_queue_##type* q, type* out) { \
    return MANGLE_DEQUE_PEEK_FRONT(type)(q, out); \
} \
\
/** \
 * @brief Returns the front item as Option<type> without removing it \
 * \
 * @param q Valid queue instance \
 * @return option_##type — Some(item) on success, None if empty or invalid \
 * \
 * @post Queue is unchanged \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage option_##type canon_queue_##type##_peek_option(const canon_queue_##type* q) { \
    return MANGLE_DEQUE_PEEK_FRONT_OPTION(type)(q); \
} \
\
/** \
 * @brief Returns the current number of elements \
 * \
 * @param q Queue to query (NULL-safe) \
 * @return usize — 0 if q == NULL \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize canon_queue_##type##_len(const canon_queue_##type* q) { \
    return MANGLE_DEQUE_LEN(type)(q); \
} \
\
/** \
 * @brief Returns the fixed maximum capacity \
 * \
 * @param q Queue to query (NULL-safe) \
 * @return usize — 0 if q == NULL \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize canon_queue_##type##_capacity(const canon_queue_##type* q) { \
    return MANGLE_DEQUE_CAPACITY(type)(q); \
} \
\
/** \
 * @brief Returns remaining free slots (capacity - len) \
 * \
 * @param q Queue to query (NULL-safe) \
 * @return usize — 0 if q == NULL \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize canon_queue_##type##_remaining(const canon_queue_##type* q) { \
    return MANGLE_DEQUE_REMAINING(type)(q); \
} \
\
/** \
 * @brief Returns true if the queue has no elements (len == 0) \
 * \
 * @param q Queue to check (NULL-safe) \
 * @return true if empty or NULL \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool canon_queue_##type##_is_empty(const canon_queue_##type* q) { \
    return MANGLE_DEQUE_IS_EMPTY(type)(q); \
} \
\
/** \
 * @brief Returns true if the queue is at capacity (len == capacity) \
 * \
 * @param q Queue to check (NULL-safe) \
 * @return true if full or NULL \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool canon_queue_##type##_is_full(const canon_queue_##type* q) { \
    return MANGLE_DEQUE_IS_FULL(type)(q); \
} \
\
/** \
 * @brief Resets the queue to empty state (O(1), does not zero buffer) \
 * \
 * @param q Queue to clear (NULL-safe) \
 * \
 * @post q->size == 0, head == 0, tail == 0 \
 * @post Buffer contents are NOT zeroed — only logical state is reset \
 * @note For sensitive data, zero the buffer manually before calling clear \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage void canon_queue_##type##_clear(canon_queue_##type* q) { \
    MANGLE_DEQUE_CLEAR(type)(q); \
}

/* ════════════════════════════════════════════════════════════════════════════
   DECLARE_QUEUE — forward-declare a typed queue (for separate compilation)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Forward-declares a typed queue and all its function signatures
 *
 * Use in shared headers alongside DECLARE_DEQUE(type).
 * Match with DEFINE_QUEUE(linkage, type) in exactly one .c file.
 *
 * @param type Element type — DECLARE_DEQUE(type) must be called first
 *
 * Example:
 * ```c
 * // In tasks.h:
 * #include "data/deque/deque_decl.h"
 * #include "data/queue.h"
 * DECLARE_DEQUE(Task)
 * DECLARE_QUEUE(Task)
 * ```
 */
#define DECLARE_QUEUE(type) \
\
typedef MANGLE_DEQUE_TYPE(type) canon_queue_##type; \
\
extern void              canon_queue_##type##_init(canon_queue_##type* q, type* buffer, usize capacity); \
extern result_bool_Error canon_queue_##type##_enqueue(canon_queue_##type* q, type item); \
extern result_bool_Error canon_queue_##type##_dequeue(canon_queue_##type* q, type* out); \
extern option_##type     canon_queue_##type##_dequeue_option(canon_queue_##type* q); \
extern bool              canon_queue_##type##_peek(const canon_queue_##type* q, type* out); \
extern option_##type     canon_queue_##type##_peek_option(const canon_queue_##type* q); \
extern usize             canon_queue_##type##_len(const canon_queue_##type* q); \
extern usize             canon_queue_##type##_capacity(const canon_queue_##type* q); \
extern usize             canon_queue_##type##_remaining(const canon_queue_##type* q); \
extern bool              canon_queue_##type##_is_empty(const canon_queue_##type* q); \
extern bool              canon_queue_##type##_is_full(const canon_queue_##type* q); \
extern void              canon_queue_##type##_clear(canon_queue_##type* q);

#endif /* CANON_DATA_QUEUE_H */
