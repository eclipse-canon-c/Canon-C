// data/deque.h
#ifndef CANON_DATA_DEQUE_H
#define CANON_DATA_DEQUE_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @file deque.h
 * @brief Bounded double-ended queue (deque) using ring buffer over caller-owned storage
 *
 * Provides efficient O(1) operations for push/pop at both ends without allocations.
 * Uses a fixed-capacity circular buffer (ring buffer) pattern.
 *
 * Key properties:
 *   - Fixed maximum capacity (no dynamic resizing)
 *   - Caller owns and provides the buffer (stack, arena, static, etc.)
 *   - All operations are bounds-checked and null-safe
 *   - Zero hidden state or overhead beyond the buffer itself
 *   - Perfect for queues, deques, sliding windows, undo/redo, etc.
 *
 * Implementation details:
 *   - Uses modular arithmetic for wrap-around
 *   - head = position of first element (front)
 *   - tail = position where next element will be inserted (back)
 *   - size tracks current number of elements
 *   - Full when size == capacity
 *   - Empty when size == 0
 *
 * Usage example:
 *   DEFINE_DEQUE(int);
 *   int buf[64];
 *   deque_int d;
 *   deque_int_init(&d, buf, 64);
 *   deque_int_push_back(&d, 42);
 *   int front;
 *   if (deque_int_pop_front(&d, &front)) {
 *       // use front (42)
 *   }
 */

/**
 * @brief Defines a type-specific bounded deque (double-ended queue)
 *
 * Generates:
 *   - struct deque_##Type with ring buffer fields
 *   - Full set of init/push/pop/size/empty operations
 *
 * @param Type Element type (e.g. int, const char*, struct Event*)
 *
 * All operations return bool (true = success, false = failure due to full/empty/invalid)
 */
#define DEFINE_DEQUE(Type) \
typedef struct { \
    Type*  buffer;     ///< Caller-owned fixed buffer \
    size_t capacity;   ///< Maximum number of elements (fixed) \
    size_t head;       ///< Index of front element (first to pop) \
    size_t tail;       ///< Index where next push_back will insert \
    size_t size;       ///< Current number of elements (0 to capacity) \
} deque_##Type; \
\
/** \
 * @brief Initializes the deque with a caller-provided buffer \
 * @param d        Pointer to uninitialized deque_##Type \
 * @param buffer   Array of Type (must remain valid during deque lifetime) \
 * @param capacity Maximum number of elements buffer can hold (> 0) \
 */ \
static inline void deque_##Type##_init(deque_##Type* d, Type* buffer, size_t capacity) { \
    if (d && buffer && capacity > 0) { \
        *d = (deque_##Type){ \
            .buffer   = buffer, \
            .capacity = capacity, \
            .head     = 0, \
            .tail     = 0, \
            .size     = 0 \
        }; \
    } \
} \
\
/** \
 * @brief Pushes an item to the front of the deque \
 * @param d     Valid deque instance \
 * @param item  Value to push \
 * @return      true if pushed successfully, false if full or invalid \
 */ \
static inline bool deque_##Type##_push_front(deque_##Type* d, Type item) { \
    if (!d || d->size >= d->capacity) return false; \
    d->head = (d->head == 0) ? d->capacity - 1 : d->head - 1; \
    d->buffer[d->head] = item; \
    d->size++; \
    return true; \
} \
\
/** \
 * @brief Pushes an item to the back of the deque \
 * @param d     Valid deque instance \
 * @param item  Value to push \
 * @return      true if pushed successfully, false if full or invalid \
 */ \
static inline bool deque_##Type##_push_back(deque_##Type* d, Type item) { \
    if (!d || d->size >= d->capacity) return false; \
    d->buffer[d->tail] = item; \
    d->tail = (d->tail + 1) % d->capacity; \
    d->size++; \
    return true; \
} \
\
/** \
 * @brief Pops and returns the item from the front of the deque \
 * @param d    Valid deque instance \
 * @param out  Pointer to receive the popped value \
 * @return     true if popped successfully, false if empty or invalid \
 */ \
static inline bool deque_##Type##_pop_front(deque_##Type* d, Type* out) { \
    if (!d || !out || d->size == 0) return false; \
    *out = d->buffer[d->head]; \
    d->head = (d->head + 1) % d->capacity; \
    d->size--; \
    return true; \
} \
\
/** \
 * @brief Pops and returns the item from the back of the deque \
 * @param d    Valid deque instance \
 * @param out  Pointer to receive the popped value \
 * @return     true if popped successfully, false if empty or invalid \
 */ \
static inline bool deque_##Type##_pop_back(deque_##Type* d, Type* out) { \
    if (!d || !out || d->size == 0) return false; \
    d->tail = (d->tail == 0) ? d->capacity - 1 : d->tail - 1; \
    *out = d->buffer[d->tail]; \
    d->size--; \
    return true; \
} \
\
/** \
 * @brief Returns the current number of elements in the deque \
 */ \
static inline size_t deque_##Type##_size(const deque_##Type* d) { \
    return d ? d->size : 0; \
} \
\
/** \
 * @brief Checks if the deque is empty \
 */ \
static inline bool deque_##Type##_empty(const deque_##Type* d) { \
    return deque_##Type##_size(d) == 0; \
}

#endif /* CANON_DATA_DEQUE_H */
