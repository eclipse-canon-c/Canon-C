// data/deque.h
#ifndef CANON_DATA_DEQUE_H
#define CANON_DATA_DEQUE_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @file deque.h
 * @brief Bounded double-ended queue (deque) using ring buffer over caller-owned storage
 *
 * Efficient O(1) push/pop operations at both ends with **fixed capacity**.
 * Uses a classic circular buffer (ring buffer) implementation.
 *
 *                           CRITICAL DESIGN NOTE
 * ──────────────────────────────────────────────────────────────────────────────
 * FIXED CAPACITY – NO AUTOMATIC GROWTH / NO REALLOCATION
 *
 * • Capacity is **fixed forever** at initialization (caller provides the buffer)
 * • The deque will **NEVER** automatically resize, grow, or reallocate
 * • When full (size == capacity): all push operations **fail** (return false)
 * • When empty (size == 0): all pop operations **fail** (return false)
 * • No silent truncation, no hidden allocations, no dynamic memory usage
 *
 * This is **intentional design**:
 *   - Maximum predictability and real-time safety
 *   - Zero surprise allocations during runtime
 *   - Perfect for arena, stack, embedded, real-time, bounded buffers,
 *     sliding windows, undo/redo systems, task queues, etc.
 *
 * If you need a deque that grows automatically:
 *   → Implement your own wrapper (e.g., realloc + doubling when full)
 *   → Or use a different container / external library
 *
 *                           Key Properties
 * ──────────────────────────────────────────────────────────────────────────────
 * • Ring buffer: efficient wrap-around using modular arithmetic
 * • head = index of front element (first to pop_front)
 * • tail = index where next push_back will insert
 * • size = current number of elements (0 to capacity)
 * • Full when size == capacity
 * • Empty when size == 0
 * • Caller owns the buffer (stack, arena, static, heap...)
 * • All operations are bounds-checked and null-safe
 * • Zero overhead beyond the buffer itself
 *
 *                           Recommended Usage Patterns
 * ──────────────────────────────────────────────────────────────────────────────
 *
 * // 1. Stack-allocated – zero dynamic allocation
 * DEFINE_DEQUE(int);
 * int buf[64];
 * deque_int window;
 * deque_int_init(&window, buf, 64);
 *
 * // 2. Arena-backed – explicit lifetime
 * int* arena_buf = arena_alloc_array(&my_arena, int, 256);
 * deque_int tasks;
 * deque_int_init(&tasks, arena_buf, 256);
 *
 * // Pre-check before bulk operations (strongly recommended)
 * if (deque_int_size(&d) + items_to_add > deque_int_capacity(&d)) {
 *     return error_not_enough_space;
 * }
 */

#define DEFINE_DEQUE(Type) \
typedef struct { \
    Type* buffer;     ///< Caller-owned fixed buffer (must remain valid) \
    size_t capacity;  ///< Fixed maximum number of elements (never changes) \
    size_t head;      ///< Index of front element (pop_front reads here) \
    size_t tail;      ///< Index where next push_back inserts \
    size_t size;      ///< Current number of elements (0..capacity) \
} deque_##Type; \
\
/** \
 * @brief Initializes the deque with caller-provided fixed buffer \
 * @param d         Pointer to uninitialized deque_##Type \
 * @param buffer    Array of Type – must remain valid for deque lifetime \
 * @param capacity  Maximum number of elements buffer can hold (> 0) \
 * \
 * @note Capacity is **fixed forever** – no growth possible later \
 */ \
static inline void deque_##Type##_init(deque_##Type* d, Type* buffer, size_t capacity) { \
    if (d && buffer && capacity > 0) { \
        *d = (deque_##Type){ \
            .buffer = buffer, \
            .capacity = capacity, \
            .head = 0, \
            .tail = 0, \
            .size = 0 \
        }; \
    } \
} \
\
/** \
 * @brief Pushes an item to the front (left) of the deque \
 * @return true on success, false if deque is full or invalid pointers \
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
 * @brief Pushes an item to the back (right) of the deque \
 * @return true on success, false if deque is full or invalid pointers \
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
 * @brief Pops and returns item from the front (left) \
 * @return true on success, false if empty or invalid pointers \
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
 * @brief Pops and returns item from the back (right) \
 * @return true on success, false if empty or invalid pointers \
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
 * @brief Returns current number of elements \
 */ \
static inline size_t deque_##Type##_size(const deque_##Type* d) { \
    return d ? d->size : 0; \
} \
\
/** \
 * @brief Returns fixed maximum capacity \
 */ \
static inline size_t deque_##Type##_capacity(const deque_##Type* d) { \
    return d ? d->capacity : 0; \
} \
\
/** \
 * @brief Checks if deque is empty (size == 0) \
 */ \
static inline bool deque_##Type##_empty(const deque_##Type* d) { \
    return deque_##Type##_size(d) == 0; \
} \
\
/** \
 * @brief Checks if deque is full (size == capacity) \
 */ \
static inline bool deque_##Type##_full(const deque_##Type* d) { \
    return deque_##Type##_size(d) == deque_##Type##_capacity(d); \
} \
\
/** \
 * @brief Clears the deque (sets size=0, head/tail unchanged) \
 * Buffer contents are not zeroed — only logical size is reset \
 */ \
static inline void deque_##Type##_clear(deque_##Type* d) { \
    if (d) d->size = 0; \
}

#endif /* CANON_DATA_DEQUE_H */
