#ifndef CANON_DATA_PRIORITY_QUEUE_H
#define CANON_DATA_PRIORITY_QUEUE_H

#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/compare.h"
#include "core/slice.h"

/**
 * @file data/priority_queue.h
 * @brief Fixed-capacity binary heap (priority queue) with caller-owned buffer
 *
 * A PriorityQueue is a binary min-heap backed by a flat contiguous buffer.
 * The element with the lowest comparator value is always at the top (peek/pop).
 * Pass a reversed comparator for a max-heap.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Capacity is fixed at initialization — no growth, no reallocation
 * - Caller owns the backing buffer (stack, arena, static)
 * - Min-heap by default — comparator determines order
 * - No allocation inside any function
 * - O(log n) push and pop, O(1) peek
 * - bytes_t view of heap contents via pq_as_bytes()
 *
 * Min-heap vs max-heap:
 * ────────────────────────────────────────────────────────────────────────────
 * The comparator determines which element is "smallest" (i.e., at the top).
 * - Min-heap (default): use algo_cmp_int or any ascending comparator
 * - Max-heap: use algo_cmp_int_desc or any descending comparator
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * data/priority_queue.h is data/.
 * No other data/ headers may be included here.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe. Caller must synchronize if shared across threads.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses <string.h> for memcpy in element swap — no Canon-C substitute at data/
 * - No platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pq_push:    O(log n)
 * - pq_pop:     O(log n)
 * - pq_peek:    O(1)
 * - pq_remove:  O(log n)
 * - pq_as_bytes: O(1)
 * - All queries: O(1)
 *
 * Buffer sizing:
 * ────────────────────────────────────────────────────────────────────────────
 * The buffer must hold at least capacity * elem_size bytes.
 *
 * Quick start:
 * ```c
 * #include "data/priority_queue.h"
 *
 * // Min-heap of ints — stack-backed
 * int buf[64];
 * PriorityQueue pq;
 * pq_init(&pq, buf, 64, sizeof(int), algo_cmp_int, NULL);
 *
 * int v;
 * pq_push(&pq, &(int){5});
 * pq_push(&pq, &(int){1});
 * pq_push(&pq, &(int){3});
 * pq_peek(&pq, &v);  // v == 1
 * pq_pop(&pq, &v);   // v == 1, heap now {3, 5}
 *
 * // Max-heap — just reverse the comparator
 * pq_init(&pq, buf, 64, sizeof(int), algo_cmp_int_desc, NULL);
 *
 * // Type-safe macro wrapper
 * DEFINE_PRIORITY_QUEUE(int)
 * pq_int h;
 * pq_int_init(&h, buf, 64, algo_cmp_int, NULL);
 * pq_int_push(&h, 42);
 * int top; pq_int_peek(&h, &top);
 * ```
 *
 * @sa core/primitives/compare.h — algo_cmp_fn comparator type, built-in comparators
 * @sa core/slice.h — bytes_t used by pq_as_bytes()
 * @sa core/primitives/ptr.h — ptr_elem used for heap index access
 */

/* ════════════════════════════════════════════════════════════════════════════
   PriorityQueue struct
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fixed-capacity binary min-heap
 *
 * Do not access fields directly — use the provided functions.
 *
 * Invariant: for all i > 0, cmp(parent(i), i) <= 0 (min-heap property).
 */
typedef struct {
    void*        data;      ///< Backing buffer (caller-owned)
    usize        len;       ///< Current number of elements
    usize        capacity;  ///< Maximum number of elements
    usize        elem_size; ///< Size of each element in bytes
    algo_cmp_fn  cmp;       ///< Comparator (determines heap order)
    void*        ctx;       ///< Optional context passed to cmp (may be NULL)
} PriorityQueue;

/* ════════════════════════════════════════════════════════════════════════════
   Internal heap helpers
   ════════════════════════════════════════════════════════════════════════════ */

static inline usize pq_parent(usize i)      { return (i - 1) / 2; }
static inline usize pq_left_child(usize i)  { return 2 * i + 1; }
static inline usize pq_right_child(usize i) { return 2 * i + 2; }

static inline void pq_swap(PriorityQueue* pq, usize a, usize b) {
    if (a == b) return;
    /* fixed 256-byte stack buffer — same pattern as algo/sort.h */
    unsigned char tmp[256];
    usize es = pq->elem_size;
    void* pa = ptr_elem(pq->data, a, es);
    void* pb = ptr_elem(pq->data, b, es);
    if (es <= 256) {
        memcpy(tmp, pa, es);
        memcpy(pa,  pb, es);
        memcpy(pb,  tmp, es);
    } else {
        unsigned char* ba = (unsigned char*)pa;
        unsigned char* bb = (unsigned char*)pb;
        for (usize k = 0; k < es; k++) {
            unsigned char t = ba[k]; ba[k] = bb[k]; bb[k] = t;
        }
    }
}

/** @brief Sifts element at index i upward to restore heap property after push */
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

/** @brief Sifts element at index i downward to restore heap property after pop */
static inline void pq_sift_down(PriorityQueue* pq, usize i) {
    while (true) {
        usize smallest = i;
        usize left     = pq_left_child(i);
        usize right    = pq_right_child(i);

        if (left < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* le = ptr_elem(pq->data, left,     pq->elem_size);
            if (pq->cmp(le, sl, pq->ctx) < 0) smallest = left;
        }
        if (right < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* re = ptr_elem(pq->data, right,    pq->elem_size);
            if (pq->cmp(re, sl, pq->ctx) < 0) smallest = right;
        }
        if (smallest == i) break;
        pq_swap(pq, i, smallest);
        i = smallest;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Initialization
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a PriorityQueue with a caller-provided buffer
 *
 * @param pq        Pointer to uninitialized PriorityQueue
 * @param buffer    Caller-provided buffer of at least capacity * elem_size bytes
 * @param capacity  Maximum number of elements (> 0)
 * @param elem_size Size of each element in bytes (> 0)
 * @param cmp       Comparator — determines heap order (min = ascending)
 * @param ctx       Optional context passed to cmp (may be NULL)
 *
 * @pre pq != NULL, buffer != NULL, capacity > 0, elem_size > 0, cmp != NULL
 * @post pq->len == 0, heap ready to use
 *
 * Performance: O(1)
 */
static inline void pq_init(
    PriorityQueue* pq,
    void*          buffer,
    usize          capacity,
    usize          elem_size,
    algo_cmp_fn    cmp,
    void*          ctx)
{
    require_msg(pq != NULL,       "pq_init: pq cannot be NULL");
    require_msg(buffer != NULL,   "pq_init: buffer cannot be NULL");
    require_msg(capacity > 0,     "pq_init: capacity must be > 0");
    require_msg(elem_size > 0,    "pq_init: elem_size must be > 0");
    require_msg(cmp != NULL,      "pq_init: cmp cannot be NULL");

    if (!pq || !buffer || capacity == 0 || elem_size == 0 || !cmp) return;

    pq->data      = buffer;
    pq->len       = 0;
    pq->capacity  = capacity;
    pq->elem_size = elem_size;
    pq->cmp       = cmp;
    pq->ctx       = ctx;
}

/**
 * @brief Builds a heap in-place from an existing filled buffer (heapify)
 *
 * Use when you want to initialize from a pre-populated array.
 * Sets len = capacity and restores the heap property in O(n).
 *
 * @param pq        Pointer to PriorityQueue initialized via pq_init()
 * @param len       Number of valid elements already in buffer (≤ capacity)
 *
 * Performance: O(n) — Floyd's heapify algorithm
 */
static inline void pq_heapify(PriorityQueue* pq, usize len) {
    require_msg(pq != NULL, "pq_heapify: pq cannot be NULL");
    if (!pq || len == 0) return;
    if (len > pq->capacity) len = pq->capacity;
    pq->len = len;
    if (len < 2) return;
    /* sift down from last non-leaf to root */
    usize i = pq_parent(len - 1) + 1;
    while (i-- > 0) {
        pq_sift_down(pq, i);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Core operations
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Inserts an element into the heap
 *
 * @param pq    Valid PriorityQueue
 * @param elem  Pointer to element to copy in (elem_size bytes are copied)
 * @return true on success, false if pq == NULL, elem == NULL, or heap full
 *
 * @post Heap property maintained
 *
 * Performance: O(log n)
 */
static inline bool pq_push(PriorityQueue* pq, const void* elem) {
    require_msg(pq != NULL,   "pq_push: pq cannot be NULL");
    require_msg(elem != NULL, "pq_push: elem cannot be NULL");
    if (!pq || !elem || pq->len >= pq->capacity) return false;

    memcpy(ptr_elem(pq->data, pq->len, pq->elem_size), elem, pq->elem_size);
    pq->len++;
    pq_sift_up(pq, pq->len - 1);
    return true;
}

/**
 * @brief Removes and returns the top element (min by default)
 *
 * @param pq    Valid PriorityQueue
 * @param out   Receives a copy of the top element (elem_size bytes written)
 *              May be NULL if the value is not needed
 * @return true on success, false if pq == NULL or heap empty
 *
 * @post Top element removed, heap property maintained
 *
 * Performance: O(log n)
 */
static inline bool pq_pop(PriorityQueue* pq, void* out) {
    require_msg(pq != NULL, "pq_pop: pq cannot be NULL");
    if (!pq || pq->len == 0) return false;

    if (out) memcpy(out, ptr_elem(pq->data, 0, pq->elem_size), pq->elem_size);

    pq->len--;
    if (pq->len > 0) {
        /* move last element to root, then sift down */
        memcpy(ptr_elem(pq->data, 0,        pq->elem_size),
               ptr_elem(pq->data, pq->len,  pq->elem_size),
               pq->elem_size);
        pq_sift_down(pq, 0);
    }
    return true;
}

/**
 * @brief Returns a pointer to the top element without removing it
 *
 * @param pq    Valid PriorityQueue
 * @param out   Receives a copy of the top element (may be NULL — just checks)
 * @return true if heap is non-empty, false otherwise
 *
 * Performance: O(1)
 */
static inline bool pq_peek(const PriorityQueue* pq, void* out) {
    if (!pq || pq->len == 0) return false;
    if (out) memcpy(out, ptr_elem_const(pq->data, 0, pq->elem_size), pq->elem_size);
    return true;
}

/**
 * @brief Removes the element at internal heap index i
 *
 * For general use when you have tracked an element's position.
 * Most callers use pq_pop() — this is for advanced removal patterns.
 *
 * @param pq  Valid PriorityQueue
 * @param i   Heap index (must be < pq->len)
 * @return true on success, false if out of bounds
 *
 * Performance: O(log n)
 */
static inline bool pq_remove_at(PriorityQueue* pq, usize i) {
    if (!pq || i >= pq->len) return false;

    pq->len--;
    if (i == pq->len) return true; /* last element — just shrink */

    memcpy(ptr_elem(pq->data, i,       pq->elem_size),
           ptr_elem(pq->data, pq->len, pq->elem_size),
           pq->elem_size);

    /* element may need to go up or down */
    pq_sift_up(pq, i);
    pq_sift_down(pq, i);
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Query — O(1)
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Returns the current number of elements. NULL-safe. */
static inline usize pq_len(const PriorityQueue* pq) {
    return pq ? pq->len : 0;
}

/** @brief Returns the maximum number of elements. NULL-safe. */
static inline usize pq_capacity(const PriorityQueue* pq) {
    return pq ? pq->capacity : 0;
}

/** @brief Returns the number of free slots. NULL-safe. */
static inline usize pq_remaining(const PriorityQueue* pq) {
    return pq ? (pq->capacity - pq->len) : 0;
}

/** @brief Returns true if the heap is empty. NULL-safe. */
static inline bool pq_is_empty(const PriorityQueue* pq) {
    return !pq || pq->len == 0;
}

/** @brief Returns true if the heap is full. NULL-safe. */
static inline bool pq_is_full(const PriorityQueue* pq) {
    return !pq || pq->len >= pq->capacity;
}

/* ════════════════════════════════════════════════════════════════════════════
   View — O(1)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a bytes_t view over the current heap elements
 *
 * Covers [data, data + len * elem_size). The bytes are in heap order
 * (not sorted order). Non-owning — do not free.
 *
 * Performance: O(1)
 */
static inline bytes_t pq_as_bytes(const PriorityQueue* pq) {
    if (!pq || !pq->data || pq->len == 0) return bytes_empty();
    return bytes_from(pq->data, pq->len * pq->elem_size);
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_PRIORITY_QUEUE — type-safe wrapper per element type
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a type-safe priority queue wrapper for a given element type
 *
 * Generated type and functions:
 * - pq_##type                    struct (wraps PriorityQueue)
 * - pq_##type##_init(h, buf, cap, cmp, ctx)
 * - pq_##type##_heapify(h, len)
 * - pq_##type##_push(h, val)     → bool (value, not pointer)
 * - pq_##type##_pop(h, out)      → bool (out is type*, may be NULL)
 * - pq_##type##_peek(h, out)     → bool (out is type*, may be NULL)
 * - pq_##type##_len(h)           → usize
 * - pq_##type##_capacity(h)      → usize
 * - pq_##type##_is_empty(h)      → bool
 * - pq_##type##_is_full(h)       → bool
 *
 * @param type Element type — must be a plain value type (no pointers in name)
 *
 * Example:
 * ```c
 * DEFINE_PRIORITY_QUEUE(int)
 *
 * int buf[32];
 * pq_int h;
 * pq_int_init(&h, buf, 32, algo_cmp_int, NULL);
 *
 * pq_int_push(&h, 5);
 * pq_int_push(&h, 1);
 * pq_int_push(&h, 3);
 *
 * int top;
 * pq_int_peek(&h, &top);  // top == 1
 * pq_int_pop(&h, &top);   // top == 1
 * ```
 */
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
static inline bool pq_##type##_push(pq_##type* h, type val) { \
    return pq_push(&h->_pq, &val); \
} \
\
static inline bool pq_##type##_pop(pq_##type* h, type* out) { \
    return pq_pop(&h->_pq, out); \
} \
\
static inline bool pq_##type##_peek(const pq_##type* h, type* out) { \
    return pq_peek(&h->_pq, out); \
} \
\
static inline usize pq_##type##_len(const pq_##type* h) { \
    return pq_len(&h->_pq); \
} \
\
static inline usize pq_##type##_capacity(const pq_##type* h) { \
    return pq_capacity(&h->_pq); \
} \
\
static inline bool pq_##type##_is_empty(const pq_##type* h) { \
    return pq_is_empty(&h->_pq); \
} \
\
static inline bool pq_##type##_is_full(const pq_##type* h) { \
    return pq_is_full(&h->_pq); \
}

#endif /* CANON_DATA_PRIORITY_QUEUE_H */
