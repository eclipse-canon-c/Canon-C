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
 * - Three-tier push API: Result (full diagnostics), try (bool), unchecked (void)
 * - All pop operations return Result<bool, Error>
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
 * Push API tiers:
 * ────────────────────────────────────────────────────────────────────────────
 * - push_front / push_back:
 *     Returns result__Bool_Error with specific error codes.
 *     Use when you need to know WHY the push failed (NULL arg vs full).
 *
 * - try_push_front / try_push_back:
 *     Returns bool — true on success, false on any failure.
 *     Use in normal code paths where only pass/fail matters.
 *     No Result construction overhead.
 *
 * - push_front_unchecked / push_back_unchecked:
 *     Returns void — no checks in release builds.
 *     Preconditions enforced by ensure_msg() in debug builds only.
 *     Use in ISRs and tight loops where you have already verified
 *     the deque is not full via is_full().
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - push_front / push_back:              O(1), no allocation
 * - try_push_front / try_push_back:      O(1), no allocation, no Result
 * - push_front_unchecked / _unchecked:   O(1), zero overhead in release
 * - pop_front / pop_back:                O(1)
 * - peek_front / peek_back:              O(1)
 * - len / capacity / remaining / is_empty / is_full: O(1)
 * - clear:                               O(1)
 * - swap:                                O(1)
 * - struct size:                         sizeof(type*) + 4*sizeof(usize)
 * - element overhead:                    0 bytes beyond sizeof(type) per element
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
 * // Option must be instantiated before DEFINE_DEQUE
 * CANON_OPTION(int)
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
 * // Bool variant — no Result overhead
 * if (!canon_deque_int_try_push_back(&d, 42)) {
 *     // deque is full or NULL — handle gracefully
 * }
 *
 * // Unchecked variant — ISR hot path (caller guarantees not full)
 * if (!canon_deque_int_is_full(&d)) {
 *     canon_deque_int_push_back_unchecked(&d, sensor_reading);
 * }
 *
 * // Option variants — no out-param needed
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
 * CANON_OPTION(voidptr)
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
 * CANON_OPTION(Task)
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
 * - ISR data ingestion (push_back_unchecked in interrupt, pop_front in main loop)
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
