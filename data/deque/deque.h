#ifndef CANON_DATA_DEQUE_H
#define CANON_DATA_DEQUE_H

#include "data/deque/deque_defn.h"

/**
 * @file deque.h
 * @brief Bounded double-ended queue (ring buffer) with explicit caller-owned storage
 *
 * This is the primary user-facing header for the Canon-C deque module.
 * Including this file gives you the full header-only API using default
 * mangled names (canon_deque_##type).
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed capacity — no automatic growth
 * - Ring buffer — O(1) push/pop at both front and back
 * - Caller owns the buffer (stack, arena, static, heap)
 * - All push/pop operations return Result<bool, Error>
 * - Option variants provided for pop and peek
 * - No hidden allocations, no surprise resizes
 *
 * Ring buffer invariant:
 * ────────────────────────────────────────────────────────────────────────────
 * - head  = index of front element (where pop_front reads)
 * - tail  = index where next push_back writes
 * - size  = current element count (0 <= size <= capacity)
 * - full  when size == capacity
 * - empty when size == 0
 * - all index arithmetic is modulo capacity
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - You always know where memory comes from
 * - You always know when an operation can fail
 * - Fixed capacity is intentional — real-time safe, deterministic
 * - For auto-growing deques, build a wrapper using dynvec.h
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - push_front / push_back:  O(1), no allocation
 * - pop_front / pop_back:    O(1)
 * - peek_front / peek_back:  O(1)
 * - len / capacity / remaining / is_empty / is_full: O(1)
 * - clear:                   O(1)
 * - swap:                    O(1)
 * - struct size:             sizeof(type*) + 4*sizeof(usize)
 * - element overhead:        0 bytes beyond sizeof(type) per element
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Each deque instance is independent — no shared state
 * - Concurrent reads on the same instance are safe
 * - Concurrent modifications require external synchronization
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 *
 * Quick start:
 * ```c
 * #include "data/deque/deque.h"
 *
 * // Instantiate a typed deque for int
 * DEFINE_DEQUE(static inline, int)
 *
 * // Stack-backed deque
 * int buf[64];
 * canon_deque_int d;
 * canon_deque_int_init(&d, buf, 64);
 *
 * canon_deque_int_push_back(&d, 10);
 * canon_deque_int_push_front(&d, 5);
 * // d is now: [5, 10]
 *
 * int val;
 * canon_deque_int_pop_front(&d, &val);  // val = 5
 * canon_deque_int_pop_back(&d, &val);   // val = 10
 *
 * // Option variants — no out param needed
 * option_int front = canon_deque_int_pop_front_option(&d);
 * option_int back  = canon_deque_int_peek_back_option(&d);
 *
 * // Sliding window pattern
 * while (has_data()) {
 *     if (canon_deque_int_is_full(&d)) {
 *         int old;
 *         canon_deque_int_pop_front(&d, &old);
 *     }
 *     canon_deque_int_push_back(&d, next_value());
 * }
 *
 * // Pointer types (typedef first)
 * typedef void* voidptr;
 * DEFINE_DEQUE(static inline, voidptr)
 * ```
 *
 * Separate compilation (large projects):
 * ```c
 * // In tasks.h:
 * #include "data/deque/deque_decl.h"
 * DECLARE_DEQUE(Task)
 *
 * // In tasks.c:
 * #include "data/deque/deque_defn.h"
 * DEFINE_DEQUE(, Task)
 * ```
 *
 * Common use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Task queues (push_back to enqueue, pop_front to dequeue)
 * - Sliding window algorithms (push_back + pop_front when full)
 * - Undo/redo stacks (push_back, pop_back to undo, push_back again to redo)
 * - BFS frontier buffers
 * - Rate-limiting / token buckets
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Random access by index (use vec instead)
 * - Auto-growing containers (build wrapper or use dynvec)
 * - Multi-threaded access without external synchronization
 *
 * @sa deque_defn.h, deque_decl.h, deque_mangle.h, deque_impl.h
 * @sa data/queue.h  (FIFO-only wrapper around deque)
 * @sa data/stack.h  (LIFO-only wrapper around vec)
 */

#endif /* CANON_DATA_DEQUE_H */
