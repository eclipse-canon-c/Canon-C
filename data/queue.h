/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_DATA_QUEUE_H
#define CANON_DATA_QUEUE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "semantics/option/option.h"
#include "data/deque/deque.h"

/**
 * @file queue.h
 * @brief Bounded FIFO queue — thin wrapper over deque_##type
 *
 * Provides a clean, intention-revealing FIFO interface with fixed capacity.
 * All operations delegate directly to the underlying deque with zero overhead.
 * enqueue = push_back, dequeue = pop_front, peek = peek_front.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero overhead — every function is a direct deque passthrough
 * - FIFO semantics only — no front-insert, no back-pop (use deque for that)
 * - Three-tier enqueue API: Result (full diagnostics), try (bool), unchecked (void)
 * - result__Bool_Error from enqueue/dequeue — matches deque and vec
 * - Option variants for dequeue and peek — no out-param required
 * - Caller owns the buffer (stack, arena, static, heap)
 * - Fixed capacity is intentional — real-time safe, deterministic
 * - All pointer parameters annotated with borrowed() per ownership.h conventions
 *
 * Enqueue API tiers:
 * ────────────────────────────────────────────────────────────────────────────
 * - enqueue(q, item):
 *     Returns result__Bool_Error with specific error codes.
 *     Use when you need to know WHY the enqueue failed (NULL arg vs full).
 *
 * - try_enqueue(q, item):
 *     Returns bool — true on success, false on any failure.
 *     Use in normal code paths where only pass/fail matters.
 *     No Result construction overhead.
 *
 * - enqueue_unchecked(q, item):
 *     Returns void — no checks in release builds.
 *     Preconditions enforced by ensure_msg() in debug builds only.
 *     Use in ISRs and tight loops where you have already verified
 *     the queue is not full via is_full().
 *
 * Note on result__Bool_Error:
 * ────────────────────────────────────────────────────────────────────────────
 * CANON_RESULT(bool, Error) token-pastes to result__Bool_Error (not
 * result_bool_Error) because bool expands to _Bool before ## in C99.
 * All enqueue/dequeue signatures use result__Bool_Error to match the
 * type instantiated by deque_impl.h.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * queue.h is data/. It wraps data/deque/deque.h.
 * DEFINE_DEQUE(linkage, type) must be called before DEFINE_QUEUE(linkage, type).
 *
 * Include dependencies (direct only):
 * ────────────────────────────────────────────────────────────────────────────
 * - core/primitives/types.h    — usize and bool used directly in DECLARE_QUEUE
 *                                extern signatures and peek's return type
 * - core/primitives/contract.h — require_msg used directly in init
 * - core/ownership.h           — borrowed() used directly in all generated
 *                                function signatures and DECLARE_QUEUE externs
 * - semantics/option/option.h  — option_##type named directly in dequeue_option
 *                                and peek_option signatures; not transitive
 *                                via deque.h
 * - data/deque/deque.h         — all MANGLE_DEQUE_* macros used directly
 *
 * Intentionally excluded:
 * - semantics/result/result.h  — result__Bool_Error is named in enqueue/dequeue
 *                                signatures but the type is already instantiated
 *                                transitively by deque_impl.h via CANON_RESULT.
 *                                queue.h never calls CANON_RESULT itself.
 * - semantics/error.h          — no ERR_* codes referenced directly
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
 * - enqueue:                 O(1) — ring buffer step, no allocation
 * - try_enqueue:             O(1) — no allocation, no Result
 * - enqueue_unchecked:       O(1) — zero overhead in release
 * - dequeue:                 O(1)
 * - peek:                    O(1)
 * - len / capacity / remaining / is_empty / is_full: O(1)
 * - clear:                   O(1)
 * - struct size:             same as deque_##type (typedef alias)
 * - element overhead:        0 bytes beyond sizeof(type) per element
 *
 * Quick start:
 * ```c
 * #include "data/queue.h"
 *
 * // Option and Deque must be instantiated first
 * CANON_OPTION(int)
 * DEFINE_DEQUE(static inline, int)
 * DEFINE_QUEUE(static inline, int)
 *
 * // Stack-backed queue
 * int buf[128];
 * queue_int q;
 * queue_int_init(&q, buf, 128);
 *
 * queue_int_enqueue(&q, 10);
 * queue_int_enqueue(&q, 20);
 *
 * int val;
 * queue_int_dequeue(&q, &val);  // val = 10 (FIFO)
 *
 * // Bool variant — no Result overhead
 * if (!queue_int_try_enqueue(&q, 42)) {
 *     // queue is full or NULL — handle gracefully
 * }
 *
 * // Unchecked variant — ISR hot path (caller guarantees not full)
 * if (!queue_int_is_full(&q)) {
 *     queue_int_enqueue_unchecked(&q, sensor_reading);
 * }
 *
 * // Option variant — no out-param needed
 * option_int next = queue_int_dequeue_option(&q);  // Some(20)
 *
 * // Peek without removing
 * option_int front = queue_int_peek_option(&q);
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
 * CANON_OPTION(Task)
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
 * - ISR data ingestion (enqueue_unchecked in interrupt, dequeue in main loop)
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
 * @brief Instantiates a typed FIFO queue as a thin wrapper over deque_##type
 *
 * Generated type (typedef alias, no new struct):
 * - queue_##type  (alias for deque_##type)
 *
 * Generated functions:
 *
 * Constructor:
 * - queue_##type##_init(q, buffer, capacity)  → void
 *
 * Queries:
 * - queue_##type##_len(q)                     → usize
 * - queue_##type##_capacity(q)                → usize
 * - queue_##type##_remaining(q)               → usize
 * - queue_##type##_is_empty(q)                → bool
 * - queue_##type##_is_full(q)                 → bool
 *
 * Enqueue (Result variant — full diagnostics):
 * - queue_##type##_enqueue(q, item)           → result__Bool_Error
 *
 * Enqueue (bool variant — no Result overhead):
 * - queue_##type##_try_enqueue(q, item)       → bool
 *
 * Enqueue (unchecked variant — debug-only assertions):
 * - queue_##type##_enqueue_unchecked(q, item) → void
 *
 * Dequeue (Result variant):
 * - queue_##type##_dequeue(q, out)            → result__Bool_Error
 *
 * Dequeue (Option variant):
 * - queue_##type##_dequeue_option(q)          → option_##type
 *
 * Peek (bool variant):
 * - queue_##type##_peek(q, out)               → bool
 *
 * Peek (Option variant):
 * - queue_##type##_peek_option(q)             → option_##type
 *
 * Misc:
 * - queue_##type##_clear(q)                   → void
 *
 * @param linkage C linkage specifier: `static inline`, `static`, or empty
 * @param type    Element type — DEFINE_DEQUE(linkage, type) must be called first
 *
 * @pre CANON_OPTION(type) has already been called for the same type
 * @pre DEFINE_DEQUE(linkage, type) has already been called for the same type
 *
 * @note queue_##type is a typedef alias for deque_##type — all deque
 *       functions remain usable. DEFINE_QUEUE only adds FIFO-named wrappers
 *       for clarity.
 */
#define DEFINE_QUEUE(linkage, type) \
\
/** \
 * @brief Fixed-capacity FIFO queue for type \
 * \
 * Typedef alias for deque_##type. \
 * All deque_##type functions are available; \
 * use queue_##type functions for FIFO-specific clarity. \
 */ \
typedef MANGLE_DEQUE_TYPE(type) queue_##type; \
\
/** \
 * @brief Initializes the queue with a caller-owned buffer \
 * \
 * @param q        borrowed(queue_##type*) — must not be NULL \
 * @param buffer   borrowed(type*) — must remain valid for queue lifetime \
 * @param capacity Maximum number of elements (> 0) \
 * \
 * @pre q != NULL \
 * @pre buffer != NULL \
 * @pre capacity > 0 \
 * \
 * @post Queue is empty, ready for enqueue/dequeue \
 * @post Capacity is fixed — no growth possible \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) — wraps caller-provided buffer \
 */ \
linkage void queue_##type##_init( \
    borrowed(queue_##type*) q, borrowed(type*) buffer, usize capacity) \
{ \
    require_msg(q != NULL, "queue_" #type "_init: q cannot be NULL"); \
    MANGLE_DEQUE_INIT(type)(q, buffer, capacity); \
} \
\
/** \
 * @brief Adds an item to the back of the queue (enqueue), returns Result \
 * \
 * FIFO ordering: items are dequeued in the order they were enqueued. \
 * \
 * @param q    borrowed(queue_##type*) — initialized queue instance \
 * @param item Value to add \
 * @return result__Bool_Error — Ok(true) on success \
 * \
 * @post Returns Err(ERR_INVALID_ARG)       if q == NULL or q->buffer == NULL \
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if queue is full \
 * \
 * Performance: \
 * - Time:  O(1) — ring buffer tail advance \
 * - Space: O(1) — no allocation \
 */ \
linkage result__Bool_Error queue_##type##_enqueue( \
    borrowed(queue_##type*) q, type item) \
{ \
    return MANGLE_DEQUE_PUSH_BACK(type)(q, item); \
} \
\
/** \
 * @brief Adds an item to the back of the queue, returns bool (no Result overhead) \
 * \
 * @param q    borrowed(queue_##type*) — initialized queue instance \
 * @param item Value to add \
 * @return true on success, false if q == NULL, q->buffer == NULL, or queue is full \
 * \
 * @remark O(1) time, O(1) space — no allocation, no Result construction \
 * @remark Preferred over enqueue in hot paths where only pass/fail matters \
 * \
 * @sa queue_##type##_enqueue — Result variant with specific error codes \
 * @sa queue_##type##_enqueue_unchecked — unchecked variant for maximum throughput \
 */ \
linkage bool queue_##type##_try_enqueue( \
    borrowed(queue_##type*) q, type item) \
{ \
    return MANGLE_DEQUE_TRY_PUSH_BACK(type)(q, item); \
} \
\
/** \
 * @brief Adds an item to the back of the queue without any checking (fast path) \
 * \
 * @param q    borrowed(queue_##type*) — initialized queue instance \
 * @param item Value to add \
 * \
 * @pre q != NULL \
 * @pre q->buffer != NULL \
 * @pre Queue is not full — caller must guarantee this \
 * \
 * @note Preconditions enforced by ensure_msg() in debug builds only. \
 *       Undefined behavior in release builds if violated. \
 * \
 * @remark O(1) time, O(1) space — zero overhead in release builds \
 * @remark Use in ISRs and tight loops where you have already checked is_full \
 * \
 * @sa queue_##type##_enqueue — Result variant with full diagnostics \
 * @sa queue_##type##_try_enqueue — bool variant with runtime checks \
 */ \
linkage void queue_##type##_enqueue_unchecked( \
    borrowed(queue_##type*) q, type item) \
{ \
    MANGLE_DEQUE_PUSH_BACK_UNCHECKED(type)(q, item); \
} \
\
/** \
 * @brief Removes and returns the front item (dequeue) \
 * \
 * Removes the oldest element — the one that was enqueued first. \
 * \
 * @param q   borrowed(queue_##type*) — initialized queue instance \
 * @param out borrowed(type*) — pointer to store the dequeued value \
 * @return result__Bool_Error — Ok(true) on success \
 * \
 * @post Returns Err(ERR_INVALID_ARG)   if q == NULL, out == NULL, or q->buffer == NULL \
 * @post Returns Err(ERR_INVALID_STATE) if queue is empty \
 * \
 * Performance: \
 * - Time:  O(1) — ring buffer head advance \
 * - Space: O(1) \
 */ \
linkage result__Bool_Error queue_##type##_dequeue( \
    borrowed(queue_##type*) q, borrowed(type*) out) \
{ \
    return MANGLE_DEQUE_POP_FRONT(type)(q, out); \
} \
\
/** \
 * @brief Removes and returns the front item as Option<type> \
 * \
 * @param q borrowed(queue_##type*) — initialized queue instance \
 * @return option_##type — Some(item) on success, None if empty or invalid \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage option_##type queue_##type##_dequeue_option( \
    borrowed(queue_##type*) q) \
{ \
    return MANGLE_DEQUE_POP_FRONT_OPTION(type)(q); \
} \
\
/** \
 * @brief Returns the front item without removing it \
 * \
 * Use peek_option() when the caller has not already confirmed the queue \
 * is non-empty. Use peek() as a fast path after an is_empty() check. \
 * \
 * @param q   borrowed(const queue_##type*) — must not be NULL \
 * @param out borrowed(type*) — must not be NULL \
 * @return true if a value was written to *out, false if queue is empty \
 * \
 * @pre q   != NULL (programming error — triggers require_msg) \
 * @pre out != NULL (programming error — triggers require_msg) \
 * \
 * @post Queue is unchanged \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool queue_##type##_peek( \
    borrowed(const queue_##type*) q, borrowed(type*) out) \
{ \
    return MANGLE_DEQUE_PEEK_FRONT(type)(q, out); \
} \
\
/** \
 * @brief Returns the front item as Option<type> without removing it \
 * \
 * Preferred over peek() when the caller has not already confirmed the \
 * queue is non-empty. Returns None cleanly instead of requiring a \
 * prior is_empty() check. \
 * \
 * @param q borrowed(const queue_##type*) — must not be NULL \
 * @return option_##type — Some(item) if non-empty, None if empty \
 * \
 * @pre q != NULL (programming error — triggers require_msg via peek_front) \
 * \
 * @post Queue is unchanged \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage option_##type queue_##type##_peek_option( \
    borrowed(const queue_##type*) q) \
{ \
    return MANGLE_DEQUE_PEEK_FRONT_OPTION(type)(q); \
} \
\
/** \
 * @brief Returns the current number of elements \
 * \
 * @param q borrowed(const queue_##type*) — NULL-safe, returns 0 if NULL \
 * @return usize \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize queue_##type##_len( \
    borrowed(const queue_##type*) q) \
{ \
    return MANGLE_DEQUE_LEN(type)(q); \
} \
\
/** \
 * @brief Returns the fixed maximum capacity \
 * \
 * @param q borrowed(const queue_##type*) — NULL-safe, returns 0 if NULL \
 * @return usize \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize queue_##type##_capacity( \
    borrowed(const queue_##type*) q) \
{ \
    return MANGLE_DEQUE_CAPACITY(type)(q); \
} \
\
/** \
 * @brief Returns remaining free slots (capacity - len) \
 * \
 * @param q borrowed(const queue_##type*) — NULL-safe, returns 0 if NULL \
 * @return usize \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage usize queue_##type##_remaining( \
    borrowed(const queue_##type*) q) \
{ \
    return MANGLE_DEQUE_REMAINING(type)(q); \
} \
\
/** \
 * @brief Returns true if the queue has no elements (len == 0) \
 * \
 * @param q borrowed(const queue_##type*) — NULL-safe, returns true if NULL \
 * @return bool \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool queue_##type##_is_empty( \
    borrowed(const queue_##type*) q) \
{ \
    return MANGLE_DEQUE_IS_EMPTY(type)(q); \
} \
\
/** \
 * @brief Returns true if the queue is at capacity (len == capacity) \
 * \
 * @param q borrowed(const queue_##type*) — NULL-safe, returns true if NULL \
 * @return bool \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage bool queue_##type##_is_full( \
    borrowed(const queue_##type*) q) \
{ \
    return MANGLE_DEQUE_IS_FULL(type)(q); \
} \
\
/** \
 * @brief Resets the queue to empty state (O(1), does not zero buffer) \
 * \
 * @param q borrowed(queue_##type*) — NULL-safe \
 * \
 * @post q->size == 0, head == 0, tail == 0 \
 * @post Buffer contents are NOT zeroed — only logical state is reset \
 * @note For sensitive data, zero the buffer manually before calling clear \
 * \
 * Performance: \
 * - Time:  O(1) \
 * - Space: O(1) \
 */ \
linkage void queue_##type##_clear(borrowed(queue_##type*) q) { \
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
 * Note: enqueue and dequeue return result__Bool_Error (not result_bool_Error).
 * CANON_RESULT(bool, Error) token-pastes to result__Bool_Error in C99 because
 * bool expands to _Bool before ## sees it.
 *
 * @param type Element type — DECLARE_DEQUE(type) must be called first
 *
 * @pre DECLARE_DEQUE(type) has already been called for the same type
 * @pre option_##type is available (from CANON_OPTION(type))
 *
 * Example:
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
 * CANON_OPTION(Task)
 * DEFINE_DEQUE(, Task)
 * DEFINE_QUEUE(, Task)
 * ```
 */
#define DECLARE_QUEUE(type) \
\
typedef MANGLE_DEQUE_TYPE(type) queue_##type; \
\
extern void                queue_##type##_init(borrowed(queue_##type*) q, borrowed(type*) buffer, usize capacity); \
extern result__Bool_Error  queue_##type##_enqueue(borrowed(queue_##type*) q, type item); \
extern bool                queue_##type##_try_enqueue(borrowed(queue_##type*) q, type item); \
extern void                queue_##type##_enqueue_unchecked(borrowed(queue_##type*) q, type item); \
extern result__Bool_Error  queue_##type##_dequeue(borrowed(queue_##type*) q, borrowed(type*) out); \
extern option_##type       queue_##type##_dequeue_option(borrowed(queue_##type*) q); \
extern bool                queue_##type##_peek(borrowed(const queue_##type*) q, borrowed(type*) out); \
extern option_##type       queue_##type##_peek_option(borrowed(const queue_##type*) q); \
extern usize               queue_##type##_len(borrowed(const queue_##type*) q); \
extern usize               queue_##type##_capacity(borrowed(const queue_##type*) q); \
extern usize               queue_##type##_remaining(borrowed(const queue_##type*) q); \
extern bool                queue_##type##_is_empty(borrowed(const queue_##type*) q); \
extern bool                queue_##type##_is_full(borrowed(const queue_##type*) q); \
extern void                queue_##type##_clear(borrowed(queue_##type*) q);

#endif /* CANON_DATA_QUEUE_H */
