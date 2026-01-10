// data/queue.h
#ifndef CANON_DATA_QUEUE_H
#define CANON_DATA_QUEUE_H

#include "data/deque.h"  // underlying fixed-capacity double-ended queue

/**
 * @file queue.h
 * @brief Simple FIFO (First-In-First-Out) queue – thin wrapper over deque
 *
 * Provides a clean, intention-revealing FIFO interface using the same fixed-capacity
 * buffer as `deque`. It is **not** a growable container.
 *
 *                           CRITICAL DESIGN NOTE
 * ──────────────────────────────────────────────────────────────────────────────
 * FIXED CAPACITY – NO AUTOMATIC GROWTH
 *
 * • Capacity is **fixed** at initialization (caller provides the buffer)
 * • The queue will **NEVER** automatically resize or reallocate
 * • When full: enqueue operations **fail** (return false)
 * • When empty: dequeue operations **fail** (return false)
 * • No silent truncation, no hidden allocations
 *
 * This is **intentional design**:
 *   - Maximum predictability & real-time safety
 *   - Zero surprise allocations
 *   - Perfect for arena, stack, embedded, real-time, or bounded buffer usage
 *
 * If you need a queue that grows automatically:
 *   → You must implement it yourself (e.g. using realloc + doubling strategy)
 *   → Or use a different container / external library
 *
 *                           Key Properties
 * ──────────────────────────────────────────────────────────────────────────────
 * • Zero runtime overhead – most operations are direct deque calls
 * • Bounds-checked enqueue/dequeue with clear success/failure
 * • Caller fully owns the underlying buffer (stack, arena, static, heap...)
 * • Type-safe via macro (DEFINE_QUEUE)
 * • Ideal for: task queues, BFS, message passing, bounded buffers, event queues...
 *
 *                           Recommended Usage Patterns
 * ──────────────────────────────────────────────────────────────────────────────
 *
 * // 1. Stack-allocated – zero dynamic allocation
 * DEFINE_QUEUE(int);
 * int buf[128];
 * queue_int tasks;
 * queue_int_init(&tasks, buf, 128);
 *
 * // 2. Arena-backed – explicit lifetime
 * int* arena_buf = arena_alloc_array(&my_arena, int, 256);
 * queue_int messages;
 * queue_int_init(&messages, arena_buf, 256);
 *
 * // Pre-check before bulk enqueue (recommended)
 * if (queue_int_len(&q) + items_to_add > queue_int_capacity(&q)) {
 *     return error_not_enough_space;
 * }
 */

#define DEFINE_QUEUE(Type) \
DEFINE_DEQUE(Type) \
\
typedef deque_##Type queue_##Type; \
\
/** \
 * @brief Initializes the queue with caller-provided fixed buffer \
 * @param q         Pointer to uninitialized queue_##Type \
 * @param buffer    Array of Type – must remain valid for the lifetime of the queue \
 * @param capacity  Maximum number of elements the buffer can hold \
 * \
 * @note Capacity is fixed forever – no growth possible later \
 */ \
static inline void queue_##Type##_init(queue_##Type* q, Type* buffer, size_t capacity) { \
    deque_##Type##_init(q, buffer, capacity); \
} \
\
/** \
 * @brief Adds an item to the back of the queue (enqueue) \
 * @param q    Valid queue instance \
 * @param item Value to add \
 * @return true on success, false if queue is full or invalid pointers \
 */ \
static inline bool queue_##Type##_enqueue(queue_##Type* q, Type item) { \
    return deque_##Type##_push_back(q, item); \
} \
\
/** \
 * @brief Removes and returns the item from the front of the queue (dequeue) \
 * @param q    Valid queue instance \
 * @param out  Where to store the dequeued value \
 * @return true on success, false if queue is empty or invalid pointers \
 */ \
static inline bool queue_##Type##_dequeue(queue_##Type* q, Type* out) { \
    return deque_##Type##_pop_front(q, out); \
} \
\
/** \
 * @brief Returns current number of elements in the queue \
 */ \
static inline size_t queue_##Type##_len(const queue_##Type* q) { \
    return deque_##Type##_len(q); \
} \
\
/** \
 * @brief Returns maximum capacity of the queue (fixed) \
 */ \
static inline size_t queue_##Type##_capacity(const queue_##Type* q) { \
    return deque_##Type##_capacity(q); \
} \
\
/** \
 * @brief Checks if queue is empty \
 */ \
static inline bool queue_##Type##_is_empty(const queue_##Type* q) { \
    return deque_##Type##_is_empty(q); \
} \
\
/** \
 * @brief Checks if queue is full (cannot enqueue more) \
 */ \
static inline bool queue_##Type##_is_full(const queue_##Type* q) { \
    return deque_##Type##_is_full(q); \
} \
\
/** \
 * @brief Clears the queue (sets length to 0, buffer unchanged) \
 */ \
static inline void queue_##Type##_clear(queue_##Type* q) { \
    deque_##Type##_clear(q); \
}

#endif /* CANON_DATA_QUEUE_H */
