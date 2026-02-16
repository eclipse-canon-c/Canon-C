#ifndef CANON_DATA_PRIORITY_QUEUE_H
#define CANON_DATA_PRIORITY_QUEUE_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/compare.h"
#include "core/memory.h"
#include "core/slice.h"
#include "semantics/option/option.h"   // for option_type
#include "semantics/result/result.h"   // for result_bool_Error

/**
 * @file data/priority_queue.h
 * @brief Fixed-capacity binary heap (priority queue) with caller-owned buffer
 *
 * A PriorityQueue is a binary min-heap backed by a flat contiguous buffer.
 * The element with the lowest comparator value is always at the top.
 * Use a descending comparator for max-heap behavior.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed capacity — no dynamic growth, no hidden allocation
 * - Caller owns the backing buffer (stack, arena, static, etc.)
 * - Min-heap by default (ascending comparator)
 * - O(log n) push/pop, O(1) peek
 * - Returns option_type / result_bool_Error for idiomatic Canon-C style
 * - bytes_t view of current contents via pq_as_bytes()
 *
 * Min-heap vs max-heap:
 * ────────────────────────────────────────────────────────────────────────────
 * - Min-heap (default): use algo_cmp_int or ascending comparator
 * - Max-heap: use algo_cmp_int_desc or descending comparator
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * data/priority_queue.h is in data/ — depends only on core/ and semantics/
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe. Caller must synchronize if shared across threads.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pq_push_result / pq_push:            O(log n)
 * - pq_pop_option / pq_pop:              O(log n)
 * - pq_peek_option / pq_peek:            O(1)
 * - pq_remove_at_result / pq_remove_at:  O(log n)
 * - pq_as_bytes:                         O(1)
 *
 * @sa core/primitives/compare.h — algo_cmp_fn and built-in comparators
 * @sa semantics/option/option.h — option_type for peek/pop
 * @sa semantics/result/result.h — result_bool_Error for fallible operations
 * @sa core/slice.h — bytes_t view of heap contents
 * @sa core/memory.h — mem_copy() for element operations
 */
/* ════════════════════════════════════════════════════════════════════════════
   PriorityQueue struct
   ════════════════════════════════════════════════════════════════════════════ */
typedef struct {
    void* data;         ///< Caller-owned buffer
    usize len;          ///< Current number of elements
    usize capacity;     ///< Fixed maximum number of elements
    usize elem_size;    ///< Size of each element in bytes
    algo_cmp_fn cmp;    ///< Three-way comparator (min-heap: <= 0 means parent first)
    void* ctx;          ///< Optional context passed to cmp (may be NULL)
} PriorityQueue;

/* ───────────────────────────────────────────────────────────────────────────
   Internal helpers
   ─────────────────────────────────────────────────────────────────────────── */
static inline usize pq_parent(usize i) { return (i - 1) / 2; }
static inline usize pq_left_child(usize i) { return 2 * i + 1; }
static inline usize pq_right_child(usize i) { return 2 * i + 2; }

/**
 * @brief Swaps two elements in the heap
 *
 * Uses mem_swap() for elements <= CANON_MEM_SWAP_MAX (typically 256 bytes).
 * For larger elements, falls back to byte-by-byte swapping.
 */
static inline void pq_swap(PriorityQueue* pq, usize a, usize b) {
    if (a == b) return;
    usize es = pq->elem_size;
    void* pa = ptr_elem(pq->data, a, es);
    void* pb = ptr_elem(pq->data, b, es);
    
    if (es <= CANON_MEM_SWAP_MAX) {
        mem_swap(pa, pb, es);
    } else {
        // Fallback for large elements exceeding mem_swap buffer
        unsigned char* ba = (unsigned char*)pa;
        unsigned char* bb = (unsigned char*)pb;
        for (usize k = 0; k < es; k++) {
            unsigned char t = ba[k]; ba[k] = bb[k]; bb[k] = t;
        }
    }
}

static inline void pq_sift_up(PriorityQueue* pq, usize i) {
    while (i > 0) {
        usize p = pq_parent(i);
        void* pe = ptr_elem(pq->data, p, pq->elem_size);
        void* ie = ptr_elem(pq->data, i, pq->elem_size);
        if (pq->cmp(pe, ie, pq->ctx) <= 0) break;
        pq_swap(pq, p, i);
        i = p;
    }
}

static inline void pq_sift_down(PriorityQueue* pq, usize i) {
    while (true) {
        usize smallest = i;
        usize left = pq_left_child(i);
        usize right = pq_right_child(i);
        if (left < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* le = ptr_elem(pq->data, left, pq->elem_size);
            if (pq->cmp(le, sl, pq->ctx) < 0) smallest = left;
        }
        if (right < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* re = ptr_elem(pq->data, right, pq->elem_size);
            if (pq->cmp(re, sl, pq->ctx) < 0) smallest = right;
        }
        if (smallest == i) break;
        pq_swap(pq, i, smallest);
        i = smallest;
    }
}

/* ───────────────────────────────────────────────────────────────────────────
   Initialization
   ─────────────────────────────────────────────────────────────────────────── */
static inline void pq_init(
    PriorityQueue* pq,
    void* buffer,
    usize capacity,
    usize elem_size,
    algo_cmp_fn cmp,
    void* ctx)
{
    require_msg(pq != NULL, "pq_init: pq cannot be NULL");
    require_msg(buffer != NULL, "pq_init: buffer cannot be NULL");
    require_msg(capacity > 0, "pq_init: capacity must be > 0");
    require_msg(elem_size > 0, "pq_init: elem_size must be > 0");
    require_msg(cmp != NULL, "pq_init: cmp cannot be NULL");

    pq->data     = buffer;
    pq->len      = 0;
    pq->capacity = capacity;
    pq->elem_size = elem_size;
    pq->cmp      = cmp;
    pq->ctx      = ctx;
}

static inline void pq_heapify(PriorityQueue* pq, usize len) {
    require_msg(pq != NULL, "pq_heapify: pq cannot be NULL");
    if (!pq || len == 0) return;
    if (len > pq->capacity) len = pq->capacity;
    pq->len = len;
    if (len < 2) return;
    usize i = pq_parent(len - 1) + 1;
    while (i-- > 0) {
        pq_sift_down(pq, i);
    }
}

/* ───────────────────────────────────────────────────────────────────────────
   Core operations — modern Canon-C style (preferred)
   ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Inserts an element into the heap (fallible)
 * @return result_bool_Error — Ok(true) on success, Err on full / invalid
 */
static inline result_bool_Error pq_push_result(
    PriorityQueue* pq,
    borrowed const void* elem)
{
    if (!pq || !elem) {
        return result_bool_Error_err(ERR_INVALID_ARG);
    }
    if (pq->len >= pq->capacity) {
        return result_bool_Error_err(ERR_CAPACITY_EXCEEDED);
    }

    mem_copy(ptr_elem(pq->data, pq->len, pq->elem_size), elem, pq->elem_size);
    pq->len++;
    pq_sift_up(pq, pq->len - 1);
    return result_bool_Error_ok(true);
}

/**
 * @brief Removes and returns the top element (optional value)
 * @return option_type — Some(value) if non-empty, None otherwise
 */
static inline option_type pq_pop_option(PriorityQueue* pq) {
    if (!pq || pq->len == 0) {
        return option_type_none();
    }

    type top;
    mem_copy(&top, ptr_elem(pq->data, 0, pq->elem_size), sizeof(type));

    pq->len--;
    if (pq->len > 0) {
        mem_copy(ptr_elem(pq->data, 0, pq->elem_size),
                 ptr_elem(pq->data, pq->len, pq->elem_size),
                 pq->elem_size);
        pq_sift_down(pq, 0);
    }

    return option_type_some(top);
}

/**
 * @brief Returns the top element without removing it
 * @return option_type — Some(value) if non-empty, None otherwise
 */
static inline option_type pq_peek_option(const PriorityQueue* pq) {
    if (!pq || pq->len == 0) {
        return option_type_none();
    }
    type* top = ptr_elem_const(pq->data, 0, pq->elem_size);
    return option_type_some(*top);
}

/**
 * @brief Removes element at given heap index (fallible)
 * @return result_bool_Error — Ok(true) on success, Err on invalid index
 */
static inline result_bool_Error pq_remove_at_result(
    PriorityQueue* pq,
    usize i)
{
    if (!pq || i >= pq->len) {
        return result_bool_Error_err(ERR_OUT_OF_RANGE);
    }

    pq->len--;
    if (i == pq->len) {
        return result_bool_Error_ok(true);
    }

    mem_copy(ptr_elem(pq->data, i, pq->elem_size),
             ptr_elem(pq->data, pq->len, pq->elem_size),
             pq->elem_size);

    pq_sift_up(pq, i);
    pq_sift_down(pq, i);
    return result_bool_Error_ok(true);
}

/* ───────────────────────────────────────────────────────────────────────────
   Compatibility / legacy bool + out-param versions
   (still available, but option/result variants preferred)
   ─────────────────────────────────────────────────────────────────────────── */

static inline bool pq_push(PriorityQueue* pq, const void* elem) {
    return result_bool_Error_is_ok(pq_push_result(pq, elem));
}

static inline bool pq_pop(PriorityQueue* pq, void* out) {
    option_type opt = pq_pop_option(pq);
    if (option_type_is_none(opt)) return false;
    if (out) mem_copy(out, &opt.value, pq->elem_size);
    return true;
}

static inline bool pq_peek(const PriorityQueue* pq, void* out) {
    option_type opt = pq_peek_option(pq);
    if (option_type_is_none(opt)) return false;
    if (out) mem_copy(out, &opt.value, pq->elem_size);
    return true;
}

static inline bool pq_remove_at(PriorityQueue* pq, usize i) {
    return result_bool_Error_is_ok(pq_remove_at_result(pq, i));
}

/* ───────────────────────────────────────────────────────────────────────────
   Queries
   ─────────────────────────────────────────────────────────────────────────── */
static inline usize pq_len(const PriorityQueue* pq)          { return pq ? pq->len : 0; }
static inline usize pq_capacity(const PriorityQueue* pq)     { return pq ? pq->capacity : 0; }
static inline usize pq_remaining(const PriorityQueue* pq)    { return pq ? (pq->capacity - pq->len) : 0; }
static inline bool  pq_is_empty(const PriorityQueue* pq)     { return !pq || pq->len == 0; }
static inline bool  pq_is_full(const PriorityQueue* pq)      { return !pq || pq->len >= pq->capacity; }

static inline bytes_t pq_as_bytes(const PriorityQueue* pq) {
    if (!pq || !pq->data || pq->len == 0) return bytes_empty();
    return bytes_from(pq->data, pq->len * pq->elem_size);
}

/* ───────────────────────────────────────────────────────────────────────────
   Typed macro wrapper
   ─────────────────────────────────────────────────────────────────────────── */
#define DEFINE_PRIORITY_QUEUE(type) \
\
typedef struct { PriorityQueue _pq; } pq_##type; \
\
static inline void pq_##type##_init( \
    pq_##type* h, type* buf, usize cap, algo_cmp_fn cmp, void* ctx) { \
    pq_init(&h->_pq, buf, cap, sizeof(type), cmp, ctx); \
} \
\
static inline void pq_##type##_heapify(pq_##type* h, usize len) { \
    pq_heapify(&h->_pq, len); \
} \
\
static inline result_bool_Error pq_##type##_push_result(pq_##type* h, type val) { \
    return pq_push_result(&h->_pq, &val); \
} \
\
static inline option_##type pq_##type##_pop_option(pq_##type* h) { \
    return pq_pop_option(&h->_pq); \
} \
\
static inline option_##type pq_##type##_peek_option(const pq_##type* h) { \
    return pq_peek_option(&h->_pq); \
} \
\
static inline result_bool_Error pq_##type##_remove_at_result(pq_##type* h, usize i) { \
    return pq_remove_at_result(&h->_pq, i); \
} \
\
/* Legacy / compatibility */ \
static inline bool pq_##type##_push(pq_##type* h, type val) { \
    return pq_push(&h->_pq, &val); \
} \
static inline bool pq_##type##_pop(pq_##type* h, type* out) { \
    return pq_pop(&h->_pq, out); \
} \
static inline bool pq_##type##_peek(const pq_##type* h, type* out) { \
    return pq_peek(&h->_pq, out); \
} \
static inline bool pq_##type##_remove_at(pq_##type* h, usize i) { \
    return pq_remove_at(&h->_pq, i); \
} \
\
static inline usize pq_##type##_len(const pq_##type* h)          { return pq_len(&h->_pq); } \
static inline usize pq_##type##_capacity(const pq_##type* h)     { return pq_capacity(&h->_pq); } \
static inline bool  pq_##type##_is_empty(const pq_##type* h)     { return pq_is_empty(&h->_pq); } \
static inline bool  pq_##type##_is_full(const pq_##type* h)      { return pq_is_full(&h->_pq); } \
static inline bytes_t pq_##type##_as_bytes(const pq_##type* h)   { return pq_as_bytes(&h->_pq); }

#endif /* CANON_DATA_PRIORITY_QUEUE_H */
