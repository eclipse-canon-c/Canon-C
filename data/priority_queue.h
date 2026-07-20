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

#ifndef CANON_DATA_PRIORITY_QUEUE_H
#define CANON_DATA_PRIORITY_QUEUE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/compare.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/error.h"
#include "semantics/option/option.h"
#include "semantics/result/result.h"

#ifdef CANON_LIFETIME_DEBUG
    #include <stdint.h>                       /* uintptr_t */
    #include "core/primitives/lifetime.h"     /* region_id_t, lifetime_t */
#endif

/*
 * Instantiate the Result type used by fallible operations.
 *
 * NOTE: CANON_RESULT(bool, Error) token-pastes to result__Bool_Error
 * (not result_bool_Error) because bool expands to _Bool before ## in C99.
 * All function signatures and call sites use result__Bool_Error accordingly.
 *
 * Guarded against multi-TU collision when vec/deque/priority_queue/hashmap
 * are all included in the same translation unit (see test/semantics/borrow_test.c).
 * Matches the pattern already used in vec_impl.h and deque_impl.h.
 */
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    CANON_RESULT(bool, Error)
#endif

/**
 * @file data/priority_queue.h
 * @brief Fixed-capacity binary min-heap (priority queue) with caller-owned buffer
 *
 * A PriorityQueue is a binary min-heap backed by a flat contiguous buffer.
 * The element with the lowest comparator value is always at the top.
 * Use a descending comparator for max-heap behavior.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed capacity — no dynamic growth, no hidden allocation
 * - Caller owns the backing buffer (stack, arena, static, etc.)
 * - Min-heap by default; pass a descending comparator for max-heap
 * - O(log n) push/pop, O(1) peek
 * - Fallible operations return result__Bool_Error
 * - Typed peek/pop return option_T via DEFINE_PRIORITY_QUEUE(T)
 * - bytes_t view of current contents via pq_as_bytes()
 *
 * NULL contract (uniform across all functions):
 * ────────────────────────────────────────────────────────────────────────────
 * - A NULL PriorityQueue* is a silent no-op for void functions
 * - Query functions return 0, false, or a safe sentinel for NULL input
 * - Invalid arguments on a valid non-NULL PriorityQueue* fire require_msg —
 *   these are programming errors, not recoverable conditions
 *
 * Min-heap vs max-heap:
 * ────────────────────────────────────────────────────────────────────────────
 * - Min-heap (default): pass algo_cmp_i32 or any ascending comparator
 * - Max-heap: pass algo_cmp_i32_desc or any descending comparator
 *
 * Typed usage (recommended):
 * ────────────────────────────────────────────────────────────────────────────
 * Use DEFINE_PRIORITY_QUEUE(T) to get a fully type-safe wrapper that
 * returns option_T from pop/peek instead of raw void* out-params.
 * Requires CANON_OPTION(T) to be instantiated first.
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 * ────────────────────────────────────────────────────────────────────────────
 *   - Embeds a lifetime_t lt field on the PriorityQueue struct. The field
 *     is inherited transparently by every DEFINE_PRIORITY_QUEUE(T) typed
 *     wrapper — typed wrappers just contain `PriorityQueue _pq` and read
 *     `h->_pq.lt` exactly like any other field.
 *   - pq_init opens a fresh lifetime. The ID is derived from a per-TU
 *     monotonic counter XOR'd with the constructor's address (same pattern
 *     as vec/deque — defensive against any future shift to a value-return
 *     constructor shape).
 *   - PriorityQueue has NO destructor. The buffer is caller-owned and the
 *     struct is caller-allocated; there is no hook on which to call close.
 *     The lifetime stays open for the life of the struct. Use-of-PQ-
 *     after-the-struct-goes-out-of-scope is the caller's responsibility,
 *     not the substrate's. (Same contract as deque.)
 *   - Restamp-on-mutation: every operation that can change the element
 *     at index 0 — push_result, pop_raw, remove_at_result, heapify — re-
 *     derives lt.id at the end of the operation (on the success path).
 *     A borrow that captured the previous id (e.g. against the address
 *     returned by pq_peek_raw before the mutation) reads the new id at
 *     the same &pq->lt address and mismatches — invalidation by id-bump,
 *     analogous to vec's swap-via-struct-copy semantics.
 *   - Operations that do NOT restamp: peek_raw/peek, queries (len,
 *     capacity, remaining, is_empty, is_full, as_bytes). The bool legacy
 *     wrappers (pq_push, pq_pop, pq_peek, pq_remove_at) inherit the
 *     restamp transitively through the result variants they delegate to.
 *   - Internal helpers (pq_swap, pq_sift_up, pq_sift_down) deliberately
 *     do NOT restamp. The restamp belongs at the public-operation
 *     boundary so a single push (which may call sift_up many times)
 *     produces exactly one id bump, not one per internal swap.
 *   Zero cost in release builds — struct layout is identical without
 *   the flag.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * data/priority_queue.h is in data/ — depends on core/ and semantics/.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe. Caller must synchronize if shared across threads.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pq_push / pq_push_result:           O(log n)
 * - pq_pop_raw / pq_pop:                O(log n)
 * - pq_peek_raw / pq_peek:              O(1)
 * - pq_remove_at / pq_remove_at_result: O(log n)
 * - pq_heapify:                         O(n) — Floyd's algorithm
 * - pq_as_bytes:                        O(1)
 *
 * Quick start:
 * ```c
 * CANON_OPTION(int)
 * DEFINE_PRIORITY_QUEUE(int)
 *
 * int buf[64];
 * pq_int h;
 * pq_int_init(&h, buf, 64, algo_cmp_i32, NULL);
 * pq_int_push_result(&h, 42);
 * pq_int_push_result(&h, 7);
 *
 * option_int top = pq_int_pop_option(&h);  // Some(7) — min-heap
 * ```
 *
 * @sa core/primitives/compare.h — algo_cmp_fn and built-in comparators
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
 * @sa semantics/option/option.h — option_T for typed peek/pop
 * @sa semantics/result/result.h — result__Bool_Error for fallible operations
 * @sa semantics/error.h         — Error enum (ERR_INVALID_ARG, etc.)
 * @sa core/ownership.h          — borrowed() annotation
 * @sa core/slice.h              — bytes_t view of heap contents
 * @sa core/memory.h             — mem_copy, mem_swap, CANON_MEM_SWAP_MAX
 */

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime field and helpers
   ════════════════════════════════════════════════════════════════════════════
   Field is conditionally injected via PQ_LIFETIME_FIELD_. Helpers are
   conditionally defined; in release builds they are empty no-ops (and the
   field doesn't exist on the struct).

   PriorityQueue is a single base struct shared across all typed wrappers —
   so a single pair of file-scoped helpers (open + restamp) suffices for
   every DEFINE_PRIORITY_QUEUE(T) instantiation. Unlike vec/deque, no
   per-instantiation helper is needed because nothing in this module
   token-pastes against the lifetime helper name.

   Restamp uses the same counter-mixed derivation as open. The result is
   a fresh id distinct from the previous one (the counter monotonically
   increments) — borrows that captured the old id now mismatch.
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    #define PQ_LIFETIME_FIELD_ \
        lifetime_t lt; /**< [debug] Lifetime token: id + open */

    /* Per-TU counter used to derive unique lifetime ids.
     *
     * The counter ensures successive id derivations within a TU produce
     * distinct values. The XOR with the struct address adds cross-TU
     * diversity. Used by both open (on pq_init) and restamp (on every
     * mutating operation).
     *
     * No thread-safety guarantee — concurrent construction or concurrent
     * mutation requires external synchronization, the same constraint
     * that applies to every Canon-C container.
     *
     * REGION_ID_STATIC (0) is reserved; the counter starts at 1 and the
     * derivation is defensively guarded against producing 0.
     */
    static inline region_id_t pq_lifetime_next_id_(void* pqp) {
        static region_id_t counter_ = 1;
        region_id_t id = (region_id_t)(counter_++)
                       ^ (region_id_t)(uintptr_t)(pqp);
        if (id == REGION_ID_STATIC) { id = (region_id_t)1; }
        return id;
    }
#else
    #define PQ_LIFETIME_FIELD_  /* empty */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   PriorityQueue struct
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fixed-capacity binary min-heap
 *
 * Do not access fields directly — use the provided functions.
 * Always initialize with pq_init() before use.
 *
 * Under CANON_LIFETIME_DEBUG, an additional lifetime_t lt field is appended
 * for borrow-invalidation tracking. Typed wrappers (DEFINE_PRIORITY_QUEUE)
 * read this field transparently as `h->_pq.lt`.
 */
typedef struct {
    void*       data;      ///< Caller-owned element buffer
    usize       len;       ///< Current number of elements
    usize       capacity;  ///< Fixed maximum number of elements
    usize       elem_size; ///< Size of each element in bytes
    algo_cmp_fn cmp;       ///< Three-way comparator (< 0 means parent first)
    void*       ctx;       ///< Optional context passed to cmp (may be NULL)
    PQ_LIFETIME_FIELD_
} PriorityQueue;

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime open/restamp helpers (file-scoped, after struct defn)
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    /** @brief Opens a fresh lifetime token on the queue (called by pq_init). */
    static inline void pq_lifetime_open_(PriorityQueue* pq) {
        pq->lt.id   = pq_lifetime_next_id_(pq);
        pq->lt.open = true;
    }
    /** @brief Derives a new id, invalidating any prior borrows. */
    static inline void pq_lifetime_restamp_(PriorityQueue* pq) {
        pq->lt.id = pq_lifetime_next_id_(pq);
        /* lt.open stays true — restamp is not destruction */
    }
#else
    static inline void pq_lifetime_open_(PriorityQueue* pq)    { (void)pq; }
    static inline void pq_lifetime_restamp_(PriorityQueue* pq) { (void)pq; }
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Internal helpers
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Returns index of parent node — @pre i > 0 */
static inline usize pq_parent(usize i) {
    require_msg(i > 0u, "pq_parent: i must be > 0 (root has no parent)");
    return (i - 1u) / 2u;
}
/** @brief Returns index of left child */
static inline usize pq_left_child(usize i)  { return 2u * i + 1u; }
/** @brief Returns index of right child */
static inline usize pq_right_child(usize i) { return 2u * i + 2u; }

/**
 * @brief Swaps two elements in the heap
 *
 * @pre pq != NULL
 * @pre a < pq->len && b < pq->len
 *
 * Lifetime: does NOT restamp. The restamp belongs at the public-operation
 * boundary so a single push/pop produces exactly one id bump, not one per
 * internal swap.
 */
static inline void pq_swap(borrowed(PriorityQueue*) pq, usize a, usize b) {
    require_msg(pq != NULL, "pq_swap: pq cannot be NULL");
    if (a == b) { return; }
    usize es = pq->elem_size;
    void* pa = ptr_elem(pq->data, a, es);
    void* pb = ptr_elem(pq->data, b, es);
    if (es <= CANON_MEM_SWAP_MAX) {
        mem_swap(pa, pb, es);
    } else {
        /* Fallback for elements larger than mem_swap's stack buffer */
        unsigned char* ba = (unsigned char*)pa;
        unsigned char* bb = (unsigned char*)pb;
        for (usize k = 0; k < es; k++) {
            unsigned char t = ba[k]; ba[k] = bb[k]; bb[k] = t;
        }
    }
}

/**
 * @brief Restores heap invariant upward from index i
 *
 * @pre pq != NULL
 *
 * Lifetime: does NOT restamp (internal helper).
 */
static inline void pq_sift_up(borrowed(PriorityQueue*) pq, usize i) {
    require_msg(pq != NULL, "pq_sift_up: pq cannot be NULL");
    while (i > 0u) {
        usize p  = pq_parent(i);
        void* pe = ptr_elem(pq->data, p, pq->elem_size);
        void* ie = ptr_elem(pq->data, i, pq->elem_size);
        if (pq->cmp(pe, ie, pq->ctx) <= 0) { break; }
        pq_swap(pq, p, i);
        i = p;
    }
}

/**
 * @brief Restores heap invariant downward from index i
 *
 * @pre pq != NULL
 *
 * Lifetime: does NOT restamp (internal helper).
 */
static inline void pq_sift_down(borrowed(PriorityQueue*) pq, usize i) {
    require_msg(pq != NULL, "pq_sift_down: pq cannot be NULL");
    while (true) {
        usize smallest = i;
        usize left     = pq_left_child(i);
        usize right    = pq_right_child(i);
        if (left < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* le = ptr_elem(pq->data, left,     pq->elem_size);
            if (pq->cmp(le, sl, pq->ctx) < 0) { smallest = left; }
        }
        if (right < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* re = ptr_elem(pq->data, right,    pq->elem_size);
            if (pq->cmp(re, sl, pq->ctx) < 0) { smallest = right; }
        }
        if (smallest == i) { break; }
        pq_swap(pq, i, smallest);
        i = smallest;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Initialization
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a PriorityQueue over a caller-owned buffer
 *
 * @param pq        Pointer to uninitialized PriorityQueue
 * @param buffer    Caller-owned element buffer — must hold at least
 *                  capacity * elem_size bytes and remain valid for the
 *                  lifetime of the queue
 * @param capacity  Maximum number of elements (> 0)
 * @param elem_size Size of each element in bytes (> 0)
 * @param cmp       Three-way comparator — must not be NULL
 * @param ctx       Optional context forwarded to cmp (may be NULL)
 *
 * @pre pq != NULL
 * @pre buffer != NULL
 * @pre capacity > 0
 * @pre elem_size > 0
 * @pre cmp != NULL
 *
 * @post pq->len == 0
 * @post pq->capacity == capacity
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token. The ID is
 * derived from a per-TU counter XOR'd with pq's address — borrows constructed
 * against this queue carry this ID.
 *
 * Performance: O(1)
 */
static inline void pq_init(
    borrowed(PriorityQueue*) pq,
    borrowed(void*)          buffer,
    usize                    capacity,
    usize                    elem_size,
    algo_cmp_fn              cmp,
    void*                    ctx)
{
    require_msg(pq        != NULL, "pq_init: pq cannot be NULL");
    require_msg(buffer    != NULL, "pq_init: buffer cannot be NULL");
    require_msg(capacity   > 0u,    "pq_init: capacity must be > 0");
    require_msg(elem_size  > 0u,    "pq_init: elem_size must be > 0");
    require_msg(cmp       != NULL, "pq_init: cmp cannot be NULL");

    pq->data      = buffer;
    pq->len       = 0;
    pq->capacity  = capacity;
    pq->elem_size = elem_size;
    pq->cmp       = cmp;
    pq->ctx       = ctx;
    pq_lifetime_open_(pq);
}

/**
 * @brief Builds a valid heap in-place from len pre-existing elements
 *
 * Uses Floyd's algorithm — O(n), not O(n log n). Prefer this over pushing
 * elements one at a time when bulk-initializing from an existing array.
 *
 * The first `len` elements of pq->data must already be populated by the
 * caller. pq_heapify rearranges them in-place to satisfy the heap invariant.
 * If len > pq->capacity it is silently clamped to pq->capacity.
 *
 * NULL pq is a no-op.
 *
 * @pre pq->data contains len valid elements of size pq->elem_size
 * @post pq->len == min(len, pq->capacity)
 * @post Heap invariant holds for all nodes
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS. Floyd's algorithm reshuffles
 * the whole heap; any borrow against a prior element position is invalid.
 *
 * Performance: O(n) — Floyd's algorithm
 */
static inline void pq_heapify(borrowed(PriorityQueue*) pq, usize len) {
    if (!pq) { return; }
    require_msg(pq->data    != NULL, "pq_heapify: pq not initialized (data is NULL)");
    require_msg(pq->cmp     != NULL, "pq_heapify: pq not initialized (cmp is NULL)");
    require_msg(pq->capacity > 0u,    "pq_heapify: pq not initialized (capacity is 0)");
    if (len == 0u) { pq->len = 0u; pq_lifetime_restamp_(pq); return; }
    if (len > pq->capacity) { len = pq->capacity; }
    pq->len = len;
    if (len >= 2u) {
        usize i = pq_parent(len - 1u) + 1u;
        while (i-- > 0) {
            pq_sift_down(pq, i);
        }
    }
    pq_lifetime_restamp_(pq);
}

/* ════════════════════════════════════════════════════════════════════════════
   Core operations — result__Bool_Error / raw out-param (untyped base layer)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Inserts an element into the heap (fallible, preferred)
 *
 * NULL pq or elem returns Err(ERR_INVALID_ARG).
 * Full queue returns Err(ERR_CAPACITY_EXCEEDED).
 *
 * @return result__Bool_Error — Ok(true) on success, Err on failure
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS on the success path. Sift-up
 * may move the new element up to position 0, displacing whatever was there.
 * A borrow against the previous index-0 element is invalid after push.
 * The error paths (NULL args, capacity exceeded) do NOT restamp — nothing
 * changed.
 *
 * Performance: O(log n)
 */
static inline result__Bool_Error pq_push_result(
    borrowed(PriorityQueue*)  pq,
    borrowed(const void*)     elem)
{
    if (!pq || !elem) { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (pq->len >= pq->capacity) { return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); }
    mem_copy(ptr_elem(pq->data, pq->len, pq->elem_size), elem, pq->elem_size);
    pq->len++;
    pq_sift_up(pq, pq->len - 1u);
    pq_lifetime_restamp_(pq);
    return result__Bool_Error_ok(true);
}

/**
 * @brief Removes the top element and copies it into out
 *
 * NULL pq or empty queue returns false. NULL out is permitted — the element
 * is removed from the heap even if out is NULL (discard semantics).
 *
 * For a type-safe option-returning variant use pq_##T##_pop_option()
 * from DEFINE_PRIORITY_QUEUE(T).
 *
 * @return true if an element was removed, false if empty or pq is NULL
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS on the success path. The
 * element at index 0 is removed; the last element is moved to position 0
 * and sift-down rearranges. A borrow against the previous index-0 is
 * invalid after pop.
 *
 * Performance: O(log n)
 */
static inline bool pq_pop_raw(borrowed(PriorityQueue*) pq, void* out) {
    if (!pq || pq->len == 0u) return false;
    if (out) mem_copy(out, ptr_elem(pq->data, 0, pq->elem_size), pq->elem_size);
    pq->len--;
    if (pq->len > 0u) {
        mem_copy(ptr_elem(pq->data, 0,       pq->elem_size),
                 ptr_elem(pq->data, pq->len, pq->elem_size),
                 pq->elem_size);
        pq_sift_down(pq, 0);
    }
    pq_lifetime_restamp_(pq);
    return true;
}

/**
 * @brief Returns a pointer to the top element without removing it
 *
 * NULL pq or empty queue returns NULL.
 * The returned pointer is valid until the next mutating operation.
 *
 * For a type-safe option-returning variant use pq_##T##_peek_option()
 * from DEFINE_PRIORITY_QUEUE(T).
 *
 * @return Pointer to the top element, or NULL if empty or pq is NULL
 *
 * Lifetime: does NOT restamp. Peek is non-mutating.
 *
 * Performance: O(1)
 */
static inline const void* pq_peek_raw(borrowed(const PriorityQueue*) pq) {
    if (!pq || pq->len == 0u) return NULL;
    return ptr_elem_const(pq->data, 0, pq->elem_size);
}

/**
 * @brief Removes the element at heap index i (fallible, preferred)
 *
 * Replaces the removed slot with the last element, then runs both
 * pq_sift_up and pq_sift_down to restore the heap invariant regardless
 * of whether the replacement is smaller or larger than its neighbors.
 *
 * NULL pq or out-of-range i returns Err(ERR_OUT_OF_RANGE).
 *
 * @return result__Bool_Error — Ok(true) on success, Err on failure
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS on the success path. The
 * removed slot is filled with the last element and reorganized — any
 * borrow against a prior position is invalid.
 *
 * Performance: O(log n)
 */
static inline result__Bool_Error pq_remove_at_result(
    borrowed(PriorityQueue*) pq,
    usize                    i)
{
    if (!pq)          { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (i >= pq->len) { return result__Bool_Error_err(ERR_OUT_OF_RANGE); }
    pq->len--;
    if (i == pq->len) {
        /* removed last element — no rearrangement, but the heap composition
         * changed (one element gone), so prior borrows are still invalidated */
        pq_lifetime_restamp_(pq);
        return result__Bool_Error_ok(true);
    }
    mem_copy(ptr_elem(pq->data, i,       pq->elem_size),
             ptr_elem(pq->data, pq->len, pq->elem_size),
             pq->elem_size);
    pq_sift_up(pq, i);
    pq_sift_down(pq, i);
    pq_lifetime_restamp_(pq);
    return result__Bool_Error_ok(true);
}

/* ════════════════════════════════════════════════════════════════════════════
   Legacy bool wrappers — kept for compatibility, result variants preferred
   ════════════════════════════════════════════════════════════════════════════
   These delegate to the result/raw variants above. Restamp happens
   transitively through the inner call — no additional bookkeeping here.
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Inserts elem — returns true on success. Prefer pq_push_result(). */
static inline bool pq_push(borrowed(PriorityQueue*) pq, borrowed(const void*) elem) {
    return result__Bool_Error_is_ok(pq_push_result(pq, elem));
}

/** @brief Removes and copies the top element into out. Prefer pq_pop_raw(). */
static inline bool pq_pop(borrowed(PriorityQueue*) pq, void* out) {
    return pq_pop_raw(pq, out);
}

/** @brief Copies the top element into out without removing it. Prefer pq_peek_raw(). */
static inline bool pq_peek(borrowed(const PriorityQueue*) pq, void* out) {
    const void* top = pq_peek_raw(pq);
    if (!top || !out) { return false; }
    mem_copy(out, top, pq->elem_size);
    return true;
}

/** @brief Removes element at index i — returns true on success. Prefer pq_remove_at_result(). */
static inline bool pq_remove_at(borrowed(PriorityQueue*) pq, usize i) {
    return result__Bool_Error_is_ok(pq_remove_at_result(pq, i));
}

/* ════════════════════════════════════════════════════════════════════════════
   Queries
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Returns current element count. NULL pq returns 0. */
static inline usize pq_len(borrowed(const PriorityQueue*) pq) {
    return pq ? pq->len : 0u;
}

/** @brief Returns maximum element capacity. NULL pq returns 0. */
static inline usize pq_capacity(borrowed(const PriorityQueue*) pq) {
    return pq ? pq->capacity : 0u;
}

/** @brief Returns number of remaining free slots. NULL pq returns 0. */
static inline usize pq_remaining(borrowed(const PriorityQueue*) pq) {
    return pq ? (pq->capacity - pq->len) : 0u;
}

/** @brief Returns true if the queue has no elements. NULL pq returns true. */
static inline bool pq_is_empty(borrowed(const PriorityQueue*) pq) {
    return !pq || pq->len == 0u;
}

/** @brief Returns true if the queue is at capacity. NULL pq returns false. */
static inline bool pq_is_full(borrowed(const PriorityQueue*) pq) {
    return pq && pq->len >= pq->capacity;
}

/**
 * @brief Returns a bytes_t view over the live heap elements
 *
 * Covers only [0, len). NULL or empty pq returns bytes_empty().
 * Non-owning — do not free the returned bytes_t.ptr.
 *
 * Performance: O(1)
 */
static inline bytes_t pq_as_bytes(borrowed(const PriorityQueue*) pq) {
    if (!pq || !pq->data || pq->len == 0u) return bytes_empty();
    return bytes_from(pq->data, pq->len * pq->elem_size);
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_PRIORITY_QUEUE(type) — typed wrapper macro
   ════════════════════════════════════════════════════════════════════════════
   Generates a type-safe pq_T wrapper over PriorityQueue.
   Pop and peek return option_T instead of raw void* out-params.

   Requires CANON_OPTION(type) to be instantiated before expanding this macro.

   The typed wrapper just contains a PriorityQueue _pq member — the lt
   field (and all lifetime bookkeeping) is inherited transparently from
   the base struct. No per-type lifetime helpers needed.

   Usage:
   ```c
   CANON_OPTION(int)
   DEFINE_PRIORITY_QUEUE(int)

   int buf[64];
   pq_int h;
   pq_int_init(&h, buf, 64, algo_cmp_i32, NULL);
   pq_int_push_result(&h, 42);
   pq_int_push_result(&h, 7);

   option_int top = pq_int_pop_option(&h);  // Some(7) — min-heap
   if (option_int_is_some(top)) {
       printf("%d\n", option_int_unwrap(top));
   }
   ```
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a fully type-safe priority queue for a given element type
 *
 * @param type Element type — CANON_OPTION(type) must be instantiated first
 *
 * Note: pq_##type##_push_result and pq_##type##_remove_at_result return
 * result__Bool_Error (not result_bool_Error) — CANON_RESULT(bool, Error)
 * token-pastes to result__Bool_Error in C99 because bool expands to _Bool
 * before ## sees it.
 */
#define DEFINE_PRIORITY_QUEUE(type)                                                          \
                                                                                             \
typedef struct { PriorityQueue _pq; } pq_##type;                                            \
                                                                                             \
/** Initializes a typed priority queue over a caller-owned buffer */                         \
static inline void pq_##type##_init(                                                         \
    borrowed(pq_##type*) h,                                                                  \
    borrowed(type*)      buf,                                                                \
    usize                cap,                                                                \
    algo_cmp_fn          cmp,                                                                \
    void*                ctx)                                                                \
{                                                                                            \
    pq_init(&h->_pq, buf, cap, sizeof(type), cmp, ctx);                                     \
}                                                                                            \
                                                                                             \
/** Heapifies len pre-existing elements already in the buffer — O(n) */                      \
static inline void pq_##type##_heapify(borrowed(pq_##type*) h, usize len) {                 \
    pq_heapify(&h->_pq, len);                                                                \
}                                                                                            \
                                                                                             \
/** Inserts val — returns result__Bool_Error */                                               \
static inline result__Bool_Error pq_##type##_push_result(                                    \
    borrowed(pq_##type*) h, type val)                                                        \
{                                                                                            \
    return pq_push_result(&h->_pq, &val);                                                    \
}                                                                                            \
                                                                                             \
/** Removes and returns the top element as option_##type */                                   \
static inline option_##type pq_##type##_pop_option(borrowed(pq_##type*) h) {                \
    type val = {0}; /* zero-init so val is never uninitialized on any path */               \
    if (!pq_pop_raw(&h->_pq, &val)) { return option_##type##_none(); }                      \
    return option_##type##_some(val);                                                        \
}                                                                                            \
                                                                                             \
/** Returns the top element as option_##type without removing it */                           \
static inline option_##type pq_##type##_peek_option(borrowed(const pq_##type*) h) {         \
    const void* raw = pq_peek_raw(&h->_pq);                                                  \
    if (!raw) { return option_##type##_none(); }                                             \
    type val;                                                                                \
    mem_copy(&val, raw, sizeof(type));                                                       \
    return option_##type##_some(val);                                                        \
}                                                                                            \
                                                                                             \
/** Removes element at heap index i — returns result__Bool_Error */                           \
static inline result__Bool_Error pq_##type##_remove_at_result(                              \
    borrowed(pq_##type*) h, usize i)                                                         \
{                                                                                            \
    return pq_remove_at_result(&h->_pq, i);                                                  \
}                                                                                            \
                                                                                             \
/* ── Legacy bool wrappers ──────────────────────────────────────────────── */               \
static inline bool pq_##type##_push(borrowed(pq_##type*) h, type val) {                     \
    return pq_push(&h->_pq, &val);                                                           \
}                                                                                            \
static inline bool pq_##type##_pop(borrowed(pq_##type*) h, type* out) {                     \
    return pq_pop_raw(&h->_pq, out);                                                         \
}                                                                                            \
static inline bool pq_##type##_peek(borrowed(const pq_##type*) h, type* out) {              \
    return pq_peek(&h->_pq, out);                                                            \
}                                                                                            \
static inline bool pq_##type##_remove_at(borrowed(pq_##type*) h, usize i) {                 \
    return pq_remove_at(&h->_pq, i);                                                         \
}                                                                                            \
                                                                                             \
/* ── Queries ────────────────────────────────────────────────────────────── */              \
static inline usize   pq_##type##_len(borrowed(const pq_##type*) h)       { return pq_len(&h->_pq); }      \
static inline usize   pq_##type##_capacity(borrowed(const pq_##type*) h)  { return pq_capacity(&h->_pq); } \
static inline usize   pq_##type##_remaining(borrowed(const pq_##type*) h) { return pq_remaining(&h->_pq); }\
static inline bool    pq_##type##_is_empty(borrowed(const pq_##type*) h)  { return pq_is_empty(&h->_pq); } \
static inline bool    pq_##type##_is_full(borrowed(const pq_##type*) h)   { return pq_is_full(&h->_pq); }  \
static inline bytes_t pq_##type##_as_bytes(borrowed(const pq_##type*) h)  { return pq_as_bytes(&h->_pq); }

#endif /* CANON_DATA_PRIORITY_QUEUE_H */
