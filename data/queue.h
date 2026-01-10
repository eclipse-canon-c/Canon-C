// data/queue.h
#ifndef CANON_DATA_QUEUE_H
#define CANON_DATA_QUEUE_H

#include "data/deque.h"  // underlying double-ended queue implementation

/**
 * @file queue.h
 * @brief Simple FIFO (First-In-First-Out) queue as a thin wrapper over deque
 *
 * Provides a clean, intention-revealing queue interface backed by a bounded deque.
 * All storage is caller-owned (uses the same fixed buffer as deque).
 *
 * Key properties:
 *   - Fixed maximum capacity (no dynamic growth)
 *   - Zero runtime overhead wrapper (direct delegation to deque)
 *   - Enqueue/dequeue operations are bounds-checked and safe
 *   - Returns bool for success/failure (true = operation succeeded)
 *   - No hidden state or allocations
 *
 * Recommended usage:
 *   - Use when you want classic FIFO semantics with clear naming (enqueue/dequeue)
 *   - Ideal for task queues, breadth-first search, message passing, buffers, etc.
 *   - Always initialize with a caller-provided buffer (stack/arena/static)
 *
 * Example:
 *   DEFINE_QUEUE(int);
 *   int buf[128];
 *   queue_int q;
 *   queue_int_init(&q, buf, 128);
 *   queue_int_enqueue(&q, 42);
 *   int front;
 *   if (queue_int_dequeue(&q, &front)) {
 *       // use front (42)
 *   }
 */

/**
 * @brief Defines a type-specific FIFO queue backed by deque_##Type
 *
 * Generates:
 *   - typedef queue_##Type as alias of deque_##Type
 *   - init/enqueue/dequeue functions with queue-friendly names
 *
 * @param Type Element type (e.g. int, const char*, struct Task*)
 *
 * All operations delegate directly to the corresponding deque functions:
 *   - enqueue → push_back
 *   - dequeue → pop_front
 */
#define DEFINE_QUEUE(Type) \
DEFINE_DEQUE(Type) \
\
typedef deque_##Type queue_##Type; \
\
/** \
 * @brief Initializes the queue with a caller-provided buffer \
 * @param q        Pointer to uninitialized queue_##Type \
 * @param buffer   Array of Type (must remain valid during queue lifetime) \
 * @param capacity Maximum number of elements buffer can hold \
 */ \
static inline void queue_##Type##_init(queue_##Type* q, Type* buffer, size_t capacity) { \
    deque_##Type##_init(q, buffer, capacity); \
} \
\
/** \
 * @brief Adds an item to the back of the queue (enqueue) \
 * @param q     Valid queue instance \
 * @param item  Value to add \
 * @return      true if enqueued successfully, false if queue is full or invalid \
 */ \
static inline bool queue_##Type##_enqueue(queue_##Type* q, Type item) { \
    return deque_##Type##_push_back(q, item); \
} \
\
/** \
 * @brief Removes and returns the item from the front of the queue (dequeue) \
 * @param q    Valid queue instance \
 * @param out  Pointer to receive the dequeued value \
 * @return     true if dequeued successfully, false if queue is empty or invalid \
 */ \
static inline bool queue_##Type##_dequeue(queue_##Type* q, Type* out) { \
    return deque_##Type##_pop_front(q, out); \
}

#endif /* CANON_DATA_QUEUE_H */
