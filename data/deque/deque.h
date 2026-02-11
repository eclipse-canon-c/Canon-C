// data/deque.h
#ifndef CANON_DATA_DEQUE_H
#define CANON_DATA_DEQUE_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

/**
 * @file deque.h
 * @brief Bounded double-ended queue (deque) using ring buffer over caller-owned storage
 *
 * Efficient O(1) push/pop operations at both ends with **fixed capacity**.
 * Uses a classic circular buffer (ring buffer) implementation.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h)
 *   - No dependencies on other library headers
 *   - No platform-specific code
 *
 * Thread-safety: Each deque instance is independent - not thread-safe for
 *                concurrent modifications. Caller must synchronize if needed.
 *                Consider lock-free ring buffer algorithms for concurrent access.
 *
 * Performance:
 *   - Zero overhead abstraction - compiles to simple modular arithmetic
 *   - All operations are O(1)
 *   - No allocations after initialization
 *   - Cache-friendly when used sequentially
 *   - Ring buffer prevents need to shift elements
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
 *   - Deterministic performance characteristics
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
 * deque_int_push_back(&window, 42);
 * deque_int_push_front(&window, 99);
 *
 * int value;
 * if (deque_int_pop_front(&window, &value)) {
 *     printf("Front: %d\n", value);
 * }
 *
 * // 2. Arena-backed – explicit lifetime
 * int* arena_buf = arena_alloc_array(&my_arena, int, 256);
 * deque_int tasks;
 * deque_int_init(&tasks, arena_buf, 256);
 *
 * // 3. Pre-check before bulk operations (strongly recommended)
 * if (deque_int_remaining(&d) < items_to_add) {
 *     return ERROR_NOT_ENOUGH_SPACE;
 * }
 * for (int i = 0; i < items_to_add; i++) {
 *     deque_int_push_back(&d, items[i]);  // Guaranteed to succeed
 * }
 *
 * // 4. Sliding window pattern
 * while (has_data()) {
 *     if (deque_int_is_full(&window)) {
 *         int old;
 *         deque_int_pop_front(&window, &old);
 *     }
 *     deque_int_push_back(&window, get_next_value());
 *     process_window(&window);
 * }
 */

/**
 * @brief Define a type-safe fixed-capacity double-ended queue for any element type
 *
 * Generates a complete deque implementation for the specified type,
 * including struct definition and all associated functions.
 *
 * @param Type Any complete type (int, float, struct Node, etc.)
 *
 * Generated type: deque_##Type
 * Generated functions:
 *   - deque_##Type##_init(d, buffer, capacity) - Initialize deque
 *   - deque_##Type##_push_front(d, item) - Add to front
 *   - deque_##Type##_push_back(d, item) - Add to back
 *   - deque_##Type##_pop_front(d, &out) - Remove from front
 *   - deque_##Type##_pop_back(d, &out) - Remove from back
 *   - deque_##Type##_peek_front(d, &out) - View front without removing
 *   - deque_##Type##_peek_back(d, &out) - View back without removing
 *   - deque_##Type##_len(d) - Get current size
 *   - deque_##Type##_capacity(d) - Get maximum capacity
 *   - deque_##Type##_remaining(d) - Get free space
 *   - deque_##Type##_is_empty(d) - Check if empty
 *   - deque_##Type##_is_full(d) - Check if full
 *   - deque_##Type##_clear(d) - Remove all elements
 *
 * Usage:
 *   DEFINE_DEQUE(int)        // Defines deque_int
 *   DEFINE_DEQUE(float)      // Defines deque_float
 *   DEFINE_DEQUE(MyStruct)   // Defines deque_MyStruct
 *
 * Example:
 *   DEFINE_DEQUE(int);
 *   
 *   int buf[32];
 *   deque_int d;
 *   deque_int_init(&d, buf, 32);
 *   
 *   deque_int_push_back(&d, 10);
 *   deque_int_push_back(&d, 20);
 *   deque_int_push_front(&d, 5);
 *   
 *   // Deque now: [5, 10, 20]
 *   
 *   int val;
 *   deque_int_pop_front(&d, &val);  // val = 5
 *   deque_int_pop_back(&d, &val);   // val = 20
 *
 * Note: This must be used at file or global scope, not inside functions.
 */
#define DEFINE_DEQUE(Type) \
\
/** \
 * @brief Fixed-capacity double-ended queue for Type \
 * \
 * Ring buffer implementation with head/tail pointers. \
 * \
 * Fields: \
 *   - buffer: Caller-owned storage \
 *   - capacity: Fixed maximum elements \
 *   - head: Index of front element \
 *   - tail: Index where next push_back inserts \
 *   - size: Current element count \
 * \
 * Do not access fields directly - use the provided functions. \
 */ \
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
 * \
 * @param d        Pointer to uninitialized deque_##Type \
 * @param buffer   Array of Type – must remain valid for deque lifetime \
 * @param capacity Maximum number of elements buffer can hold (> 0) \
 * \
 * Preconditions: \
 *   - d != NULL \
 *   - buffer != NULL (or capacity == 0) \
 *   - capacity > 0 \
 *   - buffer has space for capacity elements \
 * \
 * Postconditions: \
 *   - d is initialized as empty deque \
 *   - d can hold up to capacity elements \
 *   - head = tail = 0, size = 0 \
 * \
 * Note: Capacity is **fixed forever** – no growth possible later \
 * \
 * Example: \
 *   int buf[64]; \
 *   deque_int d; \
 *   deque_int_init(&d, buf, 64); \
 */ \
static inline void deque_##Type##_init(deque_##Type* d, Type* buffer, size_t capacity) { \
    assert(d != NULL && "deque_init: d parameter cannot be NULL"); \
    assert(buffer != NULL || capacity == 0); \
    assert(capacity > 0 && "deque_init: capacity must be greater than 0"); \
    \
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
 * \
 * Inserts element at the beginning. Existing elements shift right logically. \
 * \
 * @param d    Valid deque instance \
 * @param item Value to add \
 * @return     true on success, false if full or invalid \
 * \
 * Returns false if: \
 *   - d is NULL \
 *   - Deque is full (size >= capacity) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   deque_int_push_front(&d, 42); \
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
 * \
 * Inserts element at the end. \
 * \
 * @param d    Valid deque instance \
 * @param item Value to add \
 * @return     true on success, false if full or invalid \
 * \
 * Returns false if: \
 *   - d is NULL \
 *   - Deque is full (size >= capacity) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   deque_int_push_back(&d, 99); \
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
 * \
 * Removes and returns the first element. \
 * \
 * @param d   Valid deque instance \
 * @param out Pointer to store popped value (NULL-safe) \
 * @return    true on success, false if empty or invalid \
 * \
 * Returns false if: \
 *   - d is NULL \
 *   - out is NULL \
 *   - Deque is empty (size == 0) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   int val; \
 *   if (deque_int_pop_front(&d, &val)) { \
 *       printf("Front was: %d\n", val); \
 *   } \
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
 * \
 * Removes and returns the last element. \
 * \
 * @param d   Valid deque instance \
 * @param out Pointer to store popped value (NULL-safe) \
 * @return    true on success, false if empty or invalid \
 * \
 * Returns false if: \
 *   - d is NULL \
 *   - out is NULL \
 *   - Deque is empty (size == 0) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   int val; \
 *   if (deque_int_pop_back(&d, &val)) { \
 *       printf("Back was: %d\n", val); \
 *   } \
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
 * @brief Peeks at front element without removing it \
 * \
 * @param d   Valid deque instance \
 * @param out Pointer to store front value (NULL-safe) \
 * @return    true on success, false if empty or invalid \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   int front; \
 *   if (deque_int_peek_front(&d, &front)) { \
 *       printf("Front is: %d\n", front); \
 *   } \
 */ \
static inline bool deque_##Type##_peek_front(const deque_##Type* d, Type* out) { \
    if (!d || !out || d->size == 0) return false; \
    *out = d->buffer[d->head]; \
    return true; \
} \
\
/** \
 * @brief Peeks at back element without removing it \
 * \
 * @param d   Valid deque instance \
 * @param out Pointer to store back value (NULL-safe) \
 * @return    true on success, false if empty or invalid \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   int back; \
 *   if (deque_int_peek_back(&d, &back)) { \
 *       printf("Back is: %d\n", back); \
 *   } \
 */ \
static inline bool deque_##Type##_peek_back(const deque_##Type* d, Type* out) { \
    if (!d || !out || d->size == 0) return false; \
    size_t back_idx = (d->tail == 0) ? d->capacity - 1 : d->tail - 1; \
    *out = d->buffer[back_idx]; \
    return true; \
} \
\
/** \
 * @brief Returns current number of elements \
 * \
 * @param d Deque to query (NULL-safe) \
 * @return  Number of elements currently in deque \
 * \
 * Performance: O(1) \
 */ \
static inline size_t deque_##Type##_len(const deque_##Type* d) { \
    return d ? d->size : 0; \
} \
\
/** \
 * @brief Returns fixed maximum capacity \
 * \
 * @param d Deque to query (NULL-safe) \
 * @return  Maximum number of elements deque can hold \
 * \
 * Performance: O(1) \
 */ \
static inline size_t deque_##Type##_capacity(const deque_##Type* d) { \
    return d ? d->capacity : 0; \
} \
\
/** \
 * @brief Returns number of free slots remaining \
 * \
 * Equivalent to capacity - len. \
 * \
 * @param d Deque to query (NULL-safe) \
 * @return  Number of elements that can still be pushed \
 * \
 * Performance: O(1) \
 */ \
static inline size_t deque_##Type##_remaining(const deque_##Type* d) { \
    return d ? (d->capacity - d->size) : 0; \
} \
\
/** \
 * @brief Checks if deque is empty (size == 0) \
 * \
 * @param d Deque to check (NULL-safe) \
 * @return  true if empty, false otherwise \
 * \
 * Performance: O(1) \
 */ \
static inline bool deque_##Type##_is_empty(const deque_##Type* d) { \
    return deque_##Type##_len(d) == 0; \
} \
\
/** \
 * @brief Checks if deque is full (size == capacity) \
 * \
 * @param d Deque to check (NULL-safe) \
 * @return  true if full, false otherwise \
 * \
 * Performance: O(1) \
 */ \
static inline bool deque_##Type##_is_full(const deque_##Type* d) { \
    return d && d->size >= d->capacity; \
} \
\
/** \
 * @brief Clears the deque (sets size=0, resets head/tail) \
 * \
 * Does not modify buffer contents, only resets logical state. \
 * All previously pushed values become invalid. \
 * \
 * @param d Deque to clear \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   deque_int_clear(&d);  // Reuse deque for new sequence \
 */ \
static inline void deque_##Type##_clear(deque_##Type* d) { \
    if (d) { \
        d->size = 0; \
        d->head = 0; \
        d->tail = 0; \
    } \
}

/* ────────────────────────────────────────────────────────────────────────────
   Common type instantiations
   ────────────────────────────────────────────────────────────────────────────
   
   Uncomment the types you need:
   
   DEFINE_DEQUE(int)
   DEFINE_DEQUE(unsigned)
   DEFINE_DEQUE(long)
   DEFINE_DEQUE(size_t)
   DEFINE_DEQUE(float)
   DEFINE_DEQUE(double)
   DEFINE_DEQUE(char)
   
   typedef void* voidptr;
   DEFINE_DEQUE(voidptr)
   
   ──────────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────

    #include "deque.h"
    #include <stdio.h>
    
    // Define type
    DEFINE_DEQUE(int)
    
    // Example 1: Basic operations
    void example_basic(void) {
        int buffer[16];
        deque_int d;
        deque_int_init(&d, buffer, 16);
        
        // Add elements
        deque_int_push_back(&d, 10);
        deque_int_push_back(&d, 20);
        deque_int_push_front(&d, 5);
        deque_int_push_front(&d, 1);
        
        // Deque now: [1, 5, 10, 20]
        printf("Size: %zu\n", deque_int_len(&d));
        
        // Peek at ends
        int front, back;
        if (deque_int_peek_front(&d, &front)) {
            printf("Front: %d\n", front);  // 1
        }
        if (deque_int_peek_back(&d, &back)) {
            printf("Back: %d\n", back);    // 20
        }
        
        // Pop from both ends
        deque_int_pop_front(&d, &front);   // front = 1
        deque_int_pop_back(&d, &back);     // back = 20
        
        printf("After pops, size: %zu\n", deque_int_len(&d));  // 2
    }
    
    // Example 2: Sliding window
    void example_sliding_window(void) {
        int buffer[5];
        deque_int window;
        deque_int_init(&window, buffer, 5);
        
        int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        
        for (int i = 0; i < 10; i++) {
            // Maintain window of size 5
            if (deque_int_is_full(&window)) {
                int old;
                deque_int_pop_front(&window, &old);
            }
            deque_int_push_back(&window, data[i]);
            
            // Process current window
            printf("Window [");
            for (size_t j = 0; j < deque_int_len(&window); j++) {
                // Note: this is just for demonstration
                // Real ring buffer access would need special handling
            }
            printf("]\n");
        }
    }
    
    // Example 3: Undo/Redo system
    typedef struct {
        char action[32];
        int value;
    } Command;
    
    DEFINE_DEQUE(Command)
    
    void example_undo_redo(void) {
        Command undo_buf[50], redo_buf[50];
        deque_Command undo, redo;
        deque_Command_init(&undo, undo_buf, 50);
        deque_Command_init(&redo, redo_buf, 50);
        
        // Execute command
        Command cmd = {"insert", 42};
        deque_Command_push_back(&undo, cmd);
        
        // Undo
        Command to_undo;
        if (deque_Command_pop_back(&undo, &to_undo)) {
            printf("Undoing: %s\n", to_undo.action);
            deque_Command_push_back(&redo, to_undo);
        }
        
        // Redo
        Command to_redo;
        if (deque_Command_pop_back(&redo, &to_redo)) {
            printf("Redoing: %s\n", to_redo.action);
            deque_Command_push_back(&undo, to_redo);
        }
    }
    
    // Example 4: Error handling
    void example_error_handling(void) {
        int small_buf[2];
        deque_int d;
        deque_int_init(&d, small_buf, 2);
        
        // Fill deque
        deque_int_push_back(&d, 1);
        deque_int_push_back(&d, 2);
        
        // This will fail - deque is full
        if (!deque_int_push_back(&d, 3)) {
            fprintf(stderr, "Deque full: %zu/%zu elements\n",
                    deque_int_len(&d),
                    deque_int_capacity(&d));
        }
        
        // Check before push
        if (deque_int_remaining(&d) > 0) {
            deque_int_push_back(&d, 4);
        } else {
            fprintf(stderr, "No space remaining\n");
        }
    }
    
    // Example 5: Arena-backed deque
    void example_arena(Arena* arena) {
        int* buf = arena_alloc_array(arena, int, 128);
        deque_int d;
        deque_int_init(&d, buf, 128);
        
        // Use deque...
        for (int i = 0; i < 50; i++) {
            deque_int_push_back(&d, i);
        }
        
        // Deque lifetime tied to arena
        // Becomes invalid after arena_reset()
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_DATA_DEQUE_H */
