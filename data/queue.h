// data/queue.h
#ifndef CANON_DATA_QUEUE_H
#define CANON_DATA_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "deque.h"

/**
 * @file queue.h
 * @brief Simple FIFO (First-In-First-Out) queue – thin wrapper over deque
 *
 * Provides a clean, intention-revealing FIFO interface using the same fixed-capacity
 * buffer as `deque`. It is **not** a growable container.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h)
 *   - Depends on deque.h from this library
 *   - No platform-specific code
 *
 * Thread-safety: Each queue instance is independent - not thread-safe for
 *                concurrent modifications. Caller must synchronize if needed.
 *                Consider using a lock-free ring buffer for concurrent access.
 *
 * Performance:
 *   - Zero overhead abstraction - direct passthrough to deque operations
 *   - Enqueue/dequeue are O(1) amortized (uses circular buffer internally)
 *   - All query operations are O(1)
 *   - No hidden allocations
 *   - Cache-friendly when used sequentially
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
 *   - Deterministic performance characteristics
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
 * • Circular buffer implementation for efficiency
 * • Ideal for: task queues, BFS algorithms, message passing, bounded buffers, 
 *             event queues, producer-consumer patterns
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
 * queue_int_enqueue(&tasks, 42);
 * int value;
 * if (queue_int_dequeue(&tasks, &value)) {
 *     printf("Dequeued: %d\n", value);
 * }
 *
 * // 2. Arena-backed – explicit lifetime
 * int* arena_buf = arena_alloc_array(&my_arena, int, 256);
 * queue_int messages;
 * queue_int_init(&messages, arena_buf, 256);
 *
 * // 3. Pre-check before bulk enqueue (recommended)
 * if (queue_int_remaining(&q) < items_to_add) {
 *     return ERROR_NOT_ENOUGH_SPACE;
 * }
 * for (int i = 0; i < items_to_add; i++) {
 *     queue_int_enqueue(&q, items[i]);  // Guaranteed to succeed
 * }
 *
 * // 4. Error handling
 * if (!queue_int_enqueue(&q, value)) {
 *     fprintf(stderr, "Queue full (%zu/%zu)\n",
 *             queue_int_len(&q), queue_int_capacity(&q));
 * }
 */

/**
 * @brief Define a type-safe fixed-capacity FIFO queue for any element type
 *
 * Generates a complete queue implementation for the specified type,
 * including type definition and all associated functions.
 *
 * This is a thin wrapper over deque_##Type - all operations delegate to
 * the underlying deque with FIFO semantics (enqueue at back, dequeue from front).
 *
 * @param Type Any complete type (int, float, struct Task, etc.)
 *
 * Generated type: queue_##Type
 * Generated functions:
 *   - queue_##Type##_init(q, buffer, capacity) - Initialize queue
 *   - queue_##Type##_enqueue(q, item) - Add item to back
 *   - queue_##Type##_dequeue(q, &out) - Remove item from front
 *   - queue_##Type##_peek(q, &out) - View front without removing
 *   - queue_##Type##_len(q) - Get current size
 *   - queue_##Type##_capacity(q) - Get maximum capacity
 *   - queue_##Type##_remaining(q) - Get free space
 *   - queue_##Type##_is_empty(q) - Check if empty
 *   - queue_##Type##_is_full(q) - Check if full
 *   - queue_##Type##_clear(q) - Remove all elements
 *
 * Precondition: DEFINE_DEQUE(Type) must have been called first
 *
 * Usage:
 *   DEFINE_DEQUE(int)     // Required first
 *   DEFINE_QUEUE(int)     // Then define queue
 *
 * Example:
 *   DEFINE_DEQUE(int);
 *   DEFINE_QUEUE(int);
 *   
 *   int buf[64];
 *   queue_int q;
 *   queue_int_init(&q, buf, 64);
 *   
 *   queue_int_enqueue(&q, 10);
 *   queue_int_enqueue(&q, 20);
 *   queue_int_enqueue(&q, 30);
 *   
 *   int front;
 *   if (queue_int_peek(&q, &front)) {
 *       printf("Front: %d\n", front);  // 10
 *   }
 *   
 *   queue_int_dequeue(&q, &front);
 *   printf("Dequeued: %d\n", front);   // 10
 *   
 *   // Next dequeue gets 20, then 30
 *
 * Note: This must be used at file or global scope, not inside functions.
 */
#define DEFINE_QUEUE(Type) \
\
/** \
 * @brief Fixed-capacity FIFO queue for Type \
 * \
 * This is a type alias for deque_##Type - all deque operations are available. \
 * Use queue-specific functions for clearer intent. \
 */ \
typedef deque_##Type queue_##Type; \
\
/** \
 * @brief Initializes the queue with caller-provided fixed buffer \
 * \
 * @param q        Pointer to uninitialized queue_##Type \
 * @param buffer   Array of Type – must remain valid for queue lifetime \
 * @param capacity Maximum number of elements the buffer can hold \
 * \
 * Preconditions: \
 *   - q != NULL \
 *   - buffer != NULL (or capacity == 0) \
 *   - buffer has space for capacity elements \
 * \
 * Postconditions: \
 *   - q is initialized as empty queue \
 *   - q can hold up to capacity elements \
 * \
 * Note: Capacity is fixed forever – no growth possible later \
 * \
 * Example: \
 *   int buf[100]; \
 *   queue_int q; \
 *   queue_int_init(&q, buf, 100); \
 */ \
static inline void queue_##Type##_init(queue_##Type* q, Type* buffer, size_t capacity) { \
    assert(q != NULL && "queue_init: q parameter cannot be NULL"); \
    assert(buffer != NULL || capacity == 0); \
    deque_##Type##_init(q, buffer, capacity); \
} \
\
/** \
 * @brief Adds an item to the back of the queue (enqueue) \
 * \
 * Inserts element at the rear of the queue. \
 * First-in-first-out: items are dequeued in the order they were enqueued. \
 * \
 * @param q    Valid queue instance \
 * @param item Value to add \
 * @return     true on success, false if queue is full or invalid \
 * \
 * Returns false if: \
 *   - q is NULL or invalid \
 *   - Queue is full (len >= capacity) \
 * \
 * Performance: O(1) amortized \
 * \
 * Example: \
 *   if (!queue_int_enqueue(&q, 42)) { \
 *       fprintf(stderr, "Queue overflow\n"); \
 *   } \
 */ \
static inline bool queue_##Type##_enqueue(queue_##Type* q, Type item) { \
    return deque_##Type##_push_back(q, item); \
} \
\
/** \
 * @brief Removes and returns the item from the front of the queue (dequeue) \
 * \
 * Removes the oldest element (the one that was enqueued first). \
 * \
 * @param q   Valid queue instance \
 * @param out Pointer to store the dequeued value (NULL-safe) \
 * @return    true on success, false if queue is empty or invalid \
 * \
 * Returns false if: \
 *   - q is NULL or invalid \
 *   - out is NULL \
 *   - Queue is empty (len == 0) \
 * \
 * Performance: O(1) amortized \
 * \
 * Example: \
 *   int value; \
 *   if (queue_int_dequeue(&q, &value)) { \
 *       printf("Dequeued: %d\n", value); \
 *   } else { \
 *       fprintf(stderr, "Queue underflow\n"); \
 *   } \
 */ \
static inline bool queue_##Type##_dequeue(queue_##Type* q, Type* out) { \
    return deque_##Type##_pop_front(q, out); \
} \
\
/** \
 * @brief Peeks at the front item without removing it \
 * \
 * Returns the value that would be dequeued, without modifying the queue. \
 * \
 * @param q   Valid queue instance \
 * @param out Pointer to store the front value (NULL-safe) \
 * @return    true on success, false if queue is empty or invalid \
 * \
 * Returns false if: \
 *   - q is NULL or invalid \
 *   - out is NULL \
 *   - Queue is empty (len == 0) \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   int front; \
 *   if (queue_int_peek(&q, &front)) { \
 *       printf("Front is %d (not removed)\n", front); \
 *   } \
 */ \
static inline bool queue_##Type##_peek(const queue_##Type* q, Type* out) { \
    return deque_##Type##_peek_front(q, out); \
} \
\
/** \
 * @brief Returns current number of elements in the queue \
 * \
 * @param q Queue to query (NULL-safe) \
 * @return  Number of elements currently in queue \
 * \
 * Performance: O(1) \
 */ \
static inline size_t queue_##Type##_len(const queue_##Type* q) { \
    return deque_##Type##_len(q); \
} \
\
/** \
 * @brief Returns maximum capacity of the queue (fixed) \
 * \
 * @param q Queue to query (NULL-safe) \
 * @return  Maximum number of elements queue can hold \
 * \
 * Performance: O(1) \
 */ \
static inline size_t queue_##Type##_capacity(const queue_##Type* q) { \
    return deque_##Type##_capacity(q); \
} \
\
/** \
 * @brief Returns number of free slots remaining \
 * \
 * Equivalent to capacity - len. \
 * \
 * @param q Queue to query (NULL-safe) \
 * @return  Number of elements that can still be enqueued \
 * \
 * Example: \
 *   if (queue_int_remaining(&q) < 10) { \
 *       fprintf(stderr, "Warning: queue nearly full\n"); \
 *   } \
 * \
 * Performance: O(1) \
 */ \
static inline size_t queue_##Type##_remaining(const queue_##Type* q) { \
    return deque_##Type##_remaining(q); \
} \
\
/** \
 * @brief Checks if queue is empty \
 * \
 * @param q Queue to check (NULL-safe) \
 * @return  true if len == 0, false otherwise \
 * \
 * Performance: O(1) \
 */ \
static inline bool queue_##Type##_is_empty(const queue_##Type* q) { \
    return deque_##Type##_is_empty(q); \
} \
\
/** \
 * @brief Checks if queue is full (cannot enqueue more) \
 * \
 * @param q Queue to check (NULL-safe) \
 * @return  true if len >= capacity, false otherwise \
 * \
 * Performance: O(1) \
 */ \
static inline bool queue_##Type##_is_full(const queue_##Type* q) { \
    return deque_##Type##_is_full(q); \
} \
\
/** \
 * @brief Clears the queue (sets length to 0, buffer unchanged) \
 * \
 * Does not modify buffer contents, only resets length to 0. \
 * All previously enqueued values become invalid. \
 * \
 * @param q Queue to clear \
 * \
 * Performance: O(1) \
 * \
 * Example: \
 *   queue_int_clear(&q);  // Reuse queue for new sequence \
 */ \
static inline void queue_##Type##_clear(queue_##Type* q) { \
    deque_##Type##_clear(q); \
}

/* ────────────────────────────────────────────────────────────────────────────
   Common type instantiations
   ────────────────────────────────────────────────────────────────────────────
   
   Uncomment after defining the corresponding deque types:
   
   DEFINE_DEQUE(int)
   DEFINE_QUEUE(int)
   
   DEFINE_DEQUE(char)
   DEFINE_QUEUE(char)
   
   typedef void* voidptr;
   DEFINE_DEQUE(voidptr)
   DEFINE_QUEUE(voidptr)
   
   // For task/message passing
   typedef struct Task {
       int id;
       void (*func)(void*);
       void* arg;
   } Task;
   DEFINE_DEQUE(Task)
   DEFINE_QUEUE(Task)
   
   ──────────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────

    #include "queue.h"
    #include <stdio.h>
    
    // Define types
    DEFINE_DEQUE(int)
    DEFINE_QUEUE(int)
    
    // Example 1: Basic queue operations
    void example_basic(void) {
        int buffer[32];
        queue_int q;
        queue_int_init(&q, buffer, 32);
        
        // Enqueue elements
        queue_int_enqueue(&q, 10);
        queue_int_enqueue(&q, 20);
        queue_int_enqueue(&q, 30);
        
        printf("Queue size: %zu\n", queue_int_len(&q));
        
        // Peek at front
        int front;
        if (queue_int_peek(&q, &front)) {
            printf("Front element: %d\n", front);  // 10
        }
        
        // Dequeue elements (FIFO order)
        while (!queue_int_is_empty(&q)) {
            int value;
            queue_int_dequeue(&q, &value);
            printf("Dequeued: %d\n", value);  // 10, 20, 30
        }
    }
    
    // Example 2: BFS algorithm
    typedef struct {
        int x, y;
    } Point;
    
    DEFINE_DEQUE(Point)
    DEFINE_QUEUE(Point)
    
    void bfs_example(void) {
        Point buffer[1000];
        queue_Point q;
        queue_Point_init(&q, buffer, 1000);
        
        // Start BFS from origin
        queue_Point_enqueue(&q, (Point){0, 0});
        
        while (!queue_Point_is_empty(&q)) {
            Point current;
            queue_Point_dequeue(&q, &current);
            
            // Process current point
            printf("Visiting (%d, %d)\n", current.x, current.y);
            
            // Add neighbors to queue
            // queue_Point_enqueue(&q, neighbor1);
            // queue_Point_enqueue(&q, neighbor2);
        }
    }
    
    // Example 3: Producer-consumer pattern
    typedef struct {
        int id;
        char data[64];
    } Message;
    
    DEFINE_DEQUE(Message)
    DEFINE_QUEUE(Message)
    
    void producer_consumer_example(void) {
        Message buffer[100];
        queue_Message q;
        queue_Message_init(&q, buffer, 100);
        
        // Producer
        for (int i = 0; i < 10; i++) {
            Message msg = {.id = i};
            snprintf(msg.data, sizeof(msg.data), "Message %d", i);
            
            if (!queue_Message_enqueue(&q, msg)) {
                fprintf(stderr, "Queue full!\n");
                break;
            }
        }
        
        // Consumer
        while (!queue_Message_is_empty(&q)) {
            Message msg;
            queue_Message_dequeue(&q, &msg);
            printf("Processing: %s\n", msg.data);
        }
    }
    
    // Example 4: Task queue with error handling
    typedef struct {
        int priority;
        void (*execute)(void);
    } Task;
    
    DEFINE_DEQUE(Task)
    DEFINE_QUEUE(Task)
    
    void task_queue_example(void) {
        Task buffer[50];
        queue_Task tasks;
        queue_Task_init(&tasks, buffer, 50);
        
        // Check space before adding
        if (queue_Task_remaining(&tasks) >= 5) {
            // Safe to add 5 tasks
            for (int i = 0; i < 5; i++) {
                Task t = {.priority = i};
                queue_Task_enqueue(&tasks, t);
            }
        }
        
        // Process tasks
        Task task;
        while (queue_Task_dequeue(&tasks, &task)) {
            printf("Executing task priority %d\n", task.priority);
            if (task.execute) {
                task.execute();
            }
        }
    }
    
    // Example 5: Arena-backed queue
    void example_arena(Arena* arena) {
        int* buf = arena_alloc_array(arena, int, 256);
        queue_int q;
        queue_int_init(&q, buf, 256);
        
        // Use queue...
        for (int i = 0; i < 100; i++) {
            queue_int_enqueue(&q, i);
        }
        
        // Process first 50
        for (int i = 0; i < 50; i++) {
            int val;
            queue_int_dequeue(&q, &val);
            printf("%d ", val);
        }
        
        // Queue lifetime tied to arena
        // Becomes invalid after arena_reset()
    }
    
    // Example 6: Error handling
    void example_error_handling(void) {
        int small_buf[2];
        queue_int q;
        queue_int_init(&q, small_buf, 2);
        
        // Fill queue
        queue_int_enqueue(&q, 1);
        queue_int_enqueue(&q, 2);
        
        // This will fail - queue is full
        if (!queue_int_enqueue(&q, 3)) {
            fprintf(stderr, "Queue overflow: %zu/%zu elements\n",
                    queue_int_len(&q),
                    queue_int_capacity(&q));
        }
        
        // Check before enqueue
        if (!queue_int_is_full(&q)) {
            queue_int_enqueue(&q, 4);
        } else {
            fprintf(stderr, "No space remaining\n");
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_DATA_QUEUE_H */
