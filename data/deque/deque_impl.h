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

#ifndef CANON_DATA_DEQUE_IMPL_H
#define CANON_DATA_DEQUE_IMPL_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"      /* CANON_DEQUE_MAX_CAPACITY fallback: CANON_VEC_MAX_CAPACITY */
#include "core/primitives/contract.h"    /* require_msg, ensure_msg */
#include "core/ownership.h"              /* borrowed() */
#include "semantics/result/result.h"     /* CANON_RESULT */
#include "semantics/error.h"             /* Error, ERR_* */

#ifdef CANON_LIFETIME_DEBUG
    #include <stdint.h>                       /* uintptr_t */
    #include "core/primitives/lifetime.h"     /* region_id_t, lifetime_t */
#endif

/**
 * @file deque_impl.h
 * @brief Pure implementation logic macros for the Canon-C deque module
 *
 * Contains all function body macros. No naming decisions are made here —
 * every identifier is received as a parameter from deque_defn.h.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero naming assumptions — all identifiers are parameters
 * - Ring buffer arithmetic uses usize throughout — no raw size_t
 * - Preconditions use require_msg(), debug invariants use ensure_msg()
 * - CANON_DEQUE_MAX_CAPACITY enforced at init time
 * - Option variants provided for pop and peek alongside Result/bool variants
 * - Three-tier push API: Result (full diagnostics), try (bool), unchecked (void)
 * - All pointer parameters annotated with borrowed() per ownership.h conventions
 *
 * Ring buffer invariant:
 * ────────────────────────────────────────────────────────────────────────────
 * - head = index of front element (where pop_front reads)
 * - tail = index where next push_back writes
 * - size = current element count (0 <= size <= capacity)
 * - full when size == capacity
 * - empty when size == 0
 * - all indices are modulo capacity
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 * ────────────────────────────────────────────────────────────────────────────
 *   - Embeds a lifetime_t lt field (id + open) on each generated deque type.
 *   - init and empty open a fresh lifetime; the ID is derived from a per-TU
 *     monotonic counter XOR'd with the constructor's local address. The
 *     counter is necessary because empty() returns by value — the local's
 *     stack slot is reused across calls and the bare address would collide.
 *   - Deque has NO destructor. The buffer is caller-owned and the struct
 *     is caller-allocated; the deque module has no hook to call "close" on.
 *     Consequently, the substrate cannot catch use-of-deque-after-the-
 *     struct-goes-out-of-scope. Callers must ensure all borrows die before
 *     the deque does — the same discipline required of any caller-owned
 *     storage in Canon-C.
 *   - swap exchanges the entire struct (including lt) between two deques.
 *     A borrow that captured a's old id will read a's new id (= b's old id)
 *     and mismatch — invalidation by struct-copy, no extra restamp call.
 *   - Deque has FIXED CAPACITY — it never relocates its buffer. Every
 *     in-place mutation (push_front/back, pop_front/back, clear) does NOT
 *     touch lt — the buffer address is unchanged and existing borrows
 *     remain valid. The substrate does not catch use-of-removed-element;
 *     that bug class is out of scope (matches vec/dynvec contract).
 *   Zero cost in release builds — struct layout is identical without the flag.
 *
 * Do NOT include this file directly. Use:
 * - data/deque/deque_defn.h  — to instantiate a typed deque
 * - data/deque/deque.h       — for the full user-facing API
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * deque_impl.h may only include from core/ and semantics/.
 * No data/ headers may be included here.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All generated functions are thread-safe per instance.
 * Concurrent access to the same deque instance requires external locking.
 *
 * @sa deque_mangle.h, deque_defn.h, deque_decl.h, deque.h
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
 */

/* ════════════════════════════════════════════════════════════════════════════
   CANON_DEQUE_MAX_CAPACITY — practical limit
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Maximum capacity for any deque instance
 *
 * Reuses CANON_VEC_MAX_CAPACITY from limits.h (both are 1GB by default).
 * Override by defining CANON_DEQUE_MAX_CAPACITY before including this file.
 *
 * Must be defined before any IMPL_DEQUE_INIT expansion.
 */
#ifndef CANON_DEQUE_MAX_CAPACITY
    #define CANON_DEQUE_MAX_CAPACITY CANON_VEC_MAX_CAPACITY
#endif

/* ════════════════════════════════════════════════════════════════════════════
   result__Bool_Error instantiation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiate result__Bool_Error exactly once across all translation units
 *
 * push_front, push_back, pop_front, pop_back all return result__Bool_Error.
 * Guard prevents duplicate definition if multiple deque types are instantiated,
 * or if both vec and deque are instantiated in the same translation unit.
 *
 * Note: CANON_RESULT(bool, Error) token-pastes to result__Bool_Error because
 * bool expands to _Bool in C99. The generated type name is result__Bool_Error,
 * not result_bool_Error. All push/pop signatures and call sites use the correct
 * generated name.
 *
 * Must be defined before any IMPL_DEQUE_PUSH_* or POP_* expansion.
 */
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    CANON_RESULT(bool, Error)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime macros for the field and the open body
   ════════════════════════════════════════════════════════════════════════════
   These are defined ONCE here (outside IMPL_DEQUE_STRUCT) so the same field
   declaration and helper body expand identically into every instantiation.

   Deque has no close body — it has no destructor (no free function). Only
   an open body is needed (for init and empty). Swap invalidation is handled
   by struct copy of the entire deque, including the embedded lt field.

   In release builds (CANON_LIFETIME_DEBUG undefined):
     - DEQUE_LIFETIME_FIELD_ expands to nothing — no lt field on the struct
     - DEQUE_LIFETIME_OPEN_BODY_ expands to nothing — helper becomes a no-op
       (the trailing `(void)v;` in the helper absorbs the unused-parameter
       warning)
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    #define DEQUE_LIFETIME_FIELD_                                       \
        lifetime_t lt; /**< [debug] Lifetime token: id + open */

    /* Per-TU counter used to derive unique lifetime ids.
     *
     * Same rationale as vec: empty() returns by value, so &local-variable
     * collides across consecutive calls when the compiler reuses the stack
     * slot. A monotonic counter ensures successive constructor calls within
     * a TU produce distinct ids. The XOR with the local address adds
     * cross-TU diversity.
     *
     * No thread-safety guarantee on concurrent construction — same as every
     * other Canon-C container. Callers must externally synchronize.
     *
     * REGION_ID_STATIC (0) is reserved; the counter starts at 1 and the
     * derivation is defensively guarded against producing 0.
     */
    static inline region_id_t deque_lifetime_next_id_(void* dp) {
        static region_id_t counter_ = 1;
        region_id_t id = (region_id_t)(counter_++)
                       ^ (region_id_t)(uintptr_t)(dp);
        if (id == REGION_ID_STATIC) { id = (region_id_t)1; }
        return id;
    }

    #define DEQUE_LIFETIME_OPEN_BODY_(dp)                               \
        do {                                                            \
            (dp)->lt.id   = deque_lifetime_next_id_((dp));              \
            (dp)->lt.open = true;                                       \
        } while (0);
#else
    #define DEQUE_LIFETIME_FIELD_            /* empty */
    #define DEQUE_LIFETIME_OPEN_BODY_(dp)    /* empty */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_DEQUE_STRUCT — struct typedef for the deque
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates the struct typedef for a typed deque
 *
 * Ring buffer layout:
 * - buffer[head] is the front element
 * - buffer[(tail-1+capacity)%capacity] is the back element
 * - Elements wrap around via modulo arithmetic
 *
 * Also generates a per-instantiation static inline lifetime-open helper
 * (named via fn_lt_open). The helper wraps DEQUE_LIFETIME_OPEN_BODY_ and
 * absorbs the (void)d unused-parameter warning suppression so that release
 * builds (where the body is empty) compile cleanly.
 *
 * The lifetime helper name is passed in as a parameter rather than formed
 * via token-pasting inside the macro, because DequeType is itself the
 * expansion of MANGLE_DEQUE_TYPE(type) — and C99 does not re-scan macro
 * arguments before they participate in ##. Threading the name through is
 * the same pattern every other IMPL_DEQUE_* macro uses.
 *
 * @param DequeType    Mangled deque type name
 * @param DequeTag     Mangled deque struct tag
 * @param fn_lt_open   Mangled name of the lifetime-open helper
 * @param type         Element type
 *
 * Performance:
 * - Time:  O(1) — compile-time only
 * - Space: sizeof(DequeType) = sizeof(type*) + 4*sizeof(usize)
 *                              [+ sizeof(lifetime_t) under CANON_LIFETIME_DEBUG]
 *          Total memory: sizeof(DequeType) + capacity*sizeof(type)
 */
#define IMPL_DEQUE_STRUCT(DequeType, DequeTag, fn_lt_open, type) \
typedef struct DequeTag { \
    type* buffer;   /**< Caller-owned element buffer (must remain valid for lifetime) */ \
    usize capacity; /**< Fixed maximum number of elements (never changes after init) */ \
    usize head;     /**< Index of front element (pop_front reads here) */ \
    usize tail;     /**< Index where next push_back writes */ \
    usize size;     /**< Current element count (0 <= size <= capacity) */ \
    DEQUE_LIFETIME_FIELD_ \
} DequeType; \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Internal: lifetime helper (per-instantiation, compiled away in release) \
   ════════════════════════════════════════════════════════════════════════════ \
   - fn_lt_open: sets id (via TU-local counter) and marks open. \
                 Used by init and empty. No close helper — deque has no \
                 destructor. \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
static inline void fn_lt_open(DequeType* d) { \
    DEQUE_LIFETIME_OPEN_BODY_(d) \
    (void)d; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Constructor
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a deque using a caller-owned buffer
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(DequeType*) d, borrowed(type*) buffer, usize capacity);
 * ```
 *
 * @pre d != NULL
 * @pre buffer != NULL
 * @pre capacity > 0
 * @pre capacity <= CANON_DEQUE_MAX_CAPACITY / sizeof(type)
 *
 * @post d->buffer == buffer
 * @post d->capacity == capacity
 * @post d->head == 0, d->tail == 0, d->size == 0
 *
 * @note Capacity is fixed forever — no growth possible.
 * @note require_msg() panics on precondition violation — never returns on failure.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token. The ID is
 * derived from a per-TU counter XOR'd with d's address — borrows constructed
 * against this deque carry this ID.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation, wraps caller-provided buffer
 */
#define IMPL_DEQUE_INIT(linkage, DequeType, fn, fn_lt_open, type) \
linkage void fn(borrowed(DequeType*) d, borrowed(type*) buffer, usize capacity) { \
    require_msg(d != NULL,      #fn ": d cannot be NULL"); \
    require_msg(buffer != NULL, #fn ": buffer cannot be NULL"); \
    require_msg(capacity > 0,   #fn ": capacity must be > 0"); \
    require_msg(capacity <= CANON_DEQUE_MAX_CAPACITY / sizeof(type), \
        #fn ": capacity exceeds CANON_DEQUE_MAX_CAPACITY"); \
    d->buffer   = buffer; \
    d->capacity = capacity; \
    d->head     = 0; \
    d->tail     = 0; \
    d->size     = 0; \
    fn_lt_open(d); \
}

/**
 * @brief Returns an empty zero-initialized deque (no buffer)
 *
 * Generated function signature:
 * ```c
 * DequeType fn(void);
 * ```
 *
 * @post result.buffer == NULL, result.capacity == 0, result.size == 0
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token, same as init.
 * Even though the deque has no buffer, opening the token keeps the struct
 * semantically usable — a subsequent caller can swap it with a populated
 * deque without tripping lifetime checks on the swap partner.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_EMPTY(linkage, DequeType, fn, fn_lt_open) \
linkage DequeType fn(void) { \
    DequeType d; \
    d.buffer   = NULL; \
    d.capacity = 0; \
    d.head     = 0; \
    d.tail     = 0; \
    d.size     = 0; \
    fn_lt_open(&d); \
    return d; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Query functions
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns current number of elements
 *
 * Generated function signature:
 * ```c
 * usize fn(borrowed(const DequeType*) d);
 * ```
 *
 * @post Returns 0 if d == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_LEN(linkage, DequeType, fn) \
linkage usize fn(borrowed(const DequeType*) d) { \
    return d ? d->size : 0; \
}

/**
 * @brief Returns fixed maximum capacity
 *
 * Generated function signature:
 * ```c
 * usize fn(borrowed(const DequeType*) d);
 * ```
 *
 * @post Returns 0 if d == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_CAPACITY(linkage, DequeType, fn) \
linkage usize fn(borrowed(const DequeType*) d) { \
    return d ? d->capacity : 0; \
}

/**
 * @brief Returns number of free slots remaining (capacity - size)
 *
 * Generated function signature:
 * ```c
 * usize fn(borrowed(const DequeType*) d);
 * ```
 *
 * @post Returns 0 if d == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_REMAINING(linkage, DequeType, fn) \
linkage usize fn(borrowed(const DequeType*) d) { \
    return d ? (d->capacity - d->size) : 0; \
}

/**
 * @brief Returns true if the deque has no elements (size == 0)
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const DequeType*) d);
 * ```
 *
 * @post Returns true if d == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_IS_EMPTY(linkage, DequeType, fn) \
linkage bool fn(borrowed(const DequeType*) d) { \
    return !d || d->size == 0; \
}

/**
 * @brief Returns true if the deque is at capacity (size == capacity)
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const DequeType*) d);
 * ```
 *
 * @post Returns true if d == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_IS_FULL(linkage, DequeType, fn) \
linkage bool fn(borrowed(const DequeType*) d) { \
    return !d || d->size >= d->capacity; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Push operations — Result tier (full diagnostics)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Inserts element at the front of the deque, returns Result
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(DequeType*) d, type item);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)       if d == NULL or d->buffer == NULL
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if d->size >= d->capacity
 * @post On Ok: item is at buffer[new_head], d->size incremented by 1
 *
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT touch lt. Fixed capacity means
 * no buffer relocation; existing borrows remain valid.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation
 */
#define IMPL_DEQUE_PUSH_FRONT(linkage, DequeType, fn, type) \
linkage result__Bool_Error fn(borrowed(DequeType*) d, type item) { \
    if (!d || !d->buffer) return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (d->size >= d->capacity) return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    d->head = (d->head == 0) ? d->capacity - 1 : d->head - 1; \
    d->buffer[d->head] = item; \
    d->size++; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Inserts element at the back of the deque, returns Result
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(DequeType*) d, type item);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)       if d == NULL or d->buffer == NULL
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if d->size >= d->capacity
 * @post On Ok: item written at buffer[old_tail], tail advanced, size incremented
 *
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT touch lt. In-place mutation only.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation
 */
#define IMPL_DEQUE_PUSH_BACK(linkage, DequeType, fn, type) \
linkage result__Bool_Error fn(borrowed(DequeType*) d, type item) { \
    if (!d || !d->buffer) return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (d->size >= d->capacity) return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    d->buffer[d->tail] = item; \
    d->tail = (d->tail + 1) % d->capacity; \
    d->size++; \
    return result__Bool_Error_ok(true); \
}

/* ════════════════════════════════════════════════════════════════════════════
   Push operations — try tier (bool, no Result overhead)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Inserts element at the front, returns bool (no Result overhead)
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(DequeType*) d, type item);
 * ```
 *
 * @post Returns false if d == NULL, d->buffer == NULL, or d is full
 * @post On true: item is at buffer[new_head], d->size incremented by 1
 *
 * @remark O(1) time, O(1) space — no allocation, no Result construction
 * @remark Preferred over push_front in hot paths where only pass/fail matters
 *
 * @sa IMPL_DEQUE_PUSH_FRONT — Result variant with specific error codes
 * @sa IMPL_DEQUE_PUSH_FRONT_UNCHECKED — unchecked variant for maximum throughput
 */
#define IMPL_DEQUE_TRY_PUSH_FRONT(linkage, DequeType, fn, type) \
linkage bool fn(borrowed(DequeType*) d, type item) { \
    if (!d || !d->buffer || d->size >= d->capacity) return false; \
    d->head = (d->head == 0) ? d->capacity - 1 : d->head - 1; \
    d->buffer[d->head] = item; \
    d->size++; \
    return true; \
}

/**
 * @brief Inserts element at the back, returns bool (no Result overhead)
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(DequeType*) d, type item);
 * ```
 *
 * @post Returns false if d == NULL, d->buffer == NULL, or d is full
 * @post On true: item written at buffer[old_tail], tail advanced, size incremented
 *
 * @remark O(1) time, O(1) space — no allocation, no Result construction
 * @remark Preferred over push_back in hot paths where only pass/fail matters
 *
 * @sa IMPL_DEQUE_PUSH_BACK — Result variant with specific error codes
 * @sa IMPL_DEQUE_PUSH_BACK_UNCHECKED — unchecked variant for maximum throughput
 */
#define IMPL_DEQUE_TRY_PUSH_BACK(linkage, DequeType, fn, type) \
linkage bool fn(borrowed(DequeType*) d, type item) { \
    if (!d || !d->buffer || d->size >= d->capacity) return false; \
    d->buffer[d->tail] = item; \
    d->tail = (d->tail + 1) % d->capacity; \
    d->size++; \
    return true; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Push operations — unchecked tier (void, debug-only assertions)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Inserts element at the front without any checking (fast path)
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(DequeType*) d, type item);
 * ```
 *
 * @pre d != NULL
 * @pre d->buffer != NULL
 * @pre d->size < d->capacity — caller must guarantee this
 *
 * @note Preconditions enforced by ensure_msg() in debug builds only.
 *       Undefined behavior in release builds if violated.
 *
 * @remark O(1) time, O(1) space — zero overhead in release builds
 * @remark Use in ISRs and tight loops where you have already checked is_full
 *
 * @sa IMPL_DEQUE_PUSH_FRONT — Result variant with full diagnostics
 * @sa IMPL_DEQUE_TRY_PUSH_FRONT — bool variant with runtime checks
 */
#define IMPL_DEQUE_PUSH_FRONT_UNCHECKED(linkage, DequeType, fn, type) \
linkage void fn(borrowed(DequeType*) d, type item) { \
    ensure_msg(d != NULL,              #fn ": d cannot be NULL"); \
    ensure_msg(d->buffer != NULL,      #fn ": d->buffer cannot be NULL"); \
    ensure_msg(d->size < d->capacity,  #fn ": deque is full"); \
    d->head = (d->head == 0) ? d->capacity - 1 : d->head - 1; \
    d->buffer[d->head] = item; \
    d->size++; \
}

/**
 * @brief Inserts element at the back without any checking (fast path)
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(DequeType*) d, type item);
 * ```
 *
 * @pre d != NULL
 * @pre d->buffer != NULL
 * @pre d->size < d->capacity — caller must guarantee this
 *
 * @note Preconditions enforced by ensure_msg() in debug builds only.
 *       Undefined behavior in release builds if violated.
 *
 * @remark O(1) time, O(1) space — zero overhead in release builds
 * @remark Use in ISRs and tight loops where you have already checked is_full
 *
 * @sa IMPL_DEQUE_PUSH_BACK — Result variant with full diagnostics
 * @sa IMPL_DEQUE_TRY_PUSH_BACK — bool variant with runtime checks
 */
#define IMPL_DEQUE_PUSH_BACK_UNCHECKED(linkage, DequeType, fn, type) \
linkage void fn(borrowed(DequeType*) d, type item) { \
    ensure_msg(d != NULL,              #fn ": d cannot be NULL"); \
    ensure_msg(d->buffer != NULL,      #fn ": d->buffer cannot be NULL"); \
    ensure_msg(d->size < d->capacity,  #fn ": deque is full"); \
    d->buffer[d->tail] = item; \
    d->tail = (d->tail + 1) % d->capacity; \
    d->size++; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Pop operations
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Removes and returns the front element via Result
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(DequeType*) d, borrowed(type*) out);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)   if d == NULL, out == NULL, or d->buffer == NULL
 * @post Returns Err(ERR_INVALID_STATE) if d->size == 0
 * @post On Ok: *out holds removed element, head advanced, size decremented
 *
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT touch lt. In-place mutation only.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_POP_FRONT(linkage, DequeType, fn, type) \
linkage result__Bool_Error fn(borrowed(DequeType*) d, borrowed(type*) out) { \
    if (!d || !out || !d->buffer) return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (d->size == 0) return result__Bool_Error_err(ERR_INVALID_STATE); \
    *out = d->buffer[d->head]; \
    d->head = (d->head + 1) % d->capacity; \
    d->size--; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Removes and returns the back element via Result
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(DequeType*) d, borrowed(type*) out);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)   if d == NULL, out == NULL, or d->buffer == NULL
 * @post Returns Err(ERR_INVALID_STATE) if d->size == 0
 * @post On Ok: *out holds removed element, tail decremented, size decremented
 *
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT touch lt. In-place mutation only.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_POP_BACK(linkage, DequeType, fn, type) \
linkage result__Bool_Error fn(borrowed(DequeType*) d, borrowed(type*) out) { \
    if (!d || !out || !d->buffer) return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (d->size == 0) return result__Bool_Error_err(ERR_INVALID_STATE); \
    d->tail = (d->tail == 0) ? d->capacity - 1 : d->tail - 1; \
    *out = d->buffer[d->tail]; \
    d->size--; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Removes and returns the front element as Option<type>
 *
 * Generated function signature:
 * ```c
 * OptionType fn(borrowed(DequeType*) d);
 * ```
 *
 * @post Returns None if d == NULL, d->buffer == NULL, or d->size == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_POP_FRONT_OPTION(linkage, DequeType, fn, fn_pop_front, OptionType, fn_some, fn_none, fn_result_is_ok, type) \
linkage OptionType fn(borrowed(DequeType*) d) { \
    type out = {0}; \
    if (fn_result_is_ok(fn_pop_front(d, &out))) \
        return fn_some(out); \
    return fn_none(); \
}

/**
 * @brief Removes and returns the back element as Option<type>
 *
 * Generated function signature:
 * ```c
 * OptionType fn(borrowed(DequeType*) d);
 * ```
 *
 * @post Returns None if d == NULL, d->buffer == NULL, or d->size == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_POP_BACK_OPTION(linkage, DequeType, fn, fn_pop_back, OptionType, fn_some, fn_none, fn_result_is_ok, type) \
linkage OptionType fn(borrowed(DequeType*) d) { \
    type out = {0}; \
    if (fn_result_is_ok(fn_pop_back(d, &out))) \
        return fn_some(out); \
    return fn_none(); \
}

/* ════════════════════════════════════════════════════════════════════════════
   Peek operations (non-mutating)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Copies front element into *out without removing it
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const DequeType*) d, borrowed(type*) out);
 * ```
 *
 * @pre d   != NULL (programming error — triggers require_msg)
 * @pre out != NULL (programming error — triggers require_msg)
 *
 * @post Returns false if deque is empty — this is a normal query result
 * @post *out set on true return only
 * @post d is unchanged
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_PEEK_FRONT(linkage, DequeType, fn, type) \
linkage bool fn(borrowed(const DequeType*) d, borrowed(type*) out) { \
    require_msg(d   != NULL, #fn ": d cannot be NULL"); \
    require_msg(out != NULL, #fn ": out cannot be NULL"); \
    if (d->size == 0) { return false; } \
    *out = d->buffer[d->head]; \
    return true; \
}

/**
 * @brief Copies back element into *out without removing it
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const DequeType*) d, borrowed(type*) out);
 * ```
 *
 * @pre d   != NULL (programming error — triggers require_msg)
 * @pre out != NULL (programming error — triggers require_msg)
 *
 * @post Returns false if deque is empty — this is a normal query result
 * @post *out set on true return only
 * @post d is unchanged
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_PEEK_BACK(linkage, DequeType, fn, type) \
linkage bool fn(borrowed(const DequeType*) d, borrowed(type*) out) { \
    require_msg(d   != NULL, #fn ": d cannot be NULL"); \
    require_msg(out != NULL, #fn ": out cannot be NULL"); \
    if (d->size == 0) { return false; } \
    usize back_idx = (d->tail == 0) ? d->capacity - 1 : d->tail - 1; \
    *out = d->buffer[back_idx]; \
    return true; \
}

/**
 * @brief Returns front element as Option<type> without removing it
 *
 * Generated function signature:
 * ```c
 * OptionType fn(borrowed(const DequeType*) d);
 * ```
 *
 * @pre d != NULL (programming error — triggers require_msg via peek_front)
 *
 * @post Returns None if d->size == 0
 * @post d is unchanged
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_PEEK_FRONT_OPTION(linkage, DequeType, fn, fn_peek_front, OptionType, fn_some, fn_none, type) \
linkage OptionType fn(borrowed(const DequeType*) d) { \
    type out = {0}; \
    if (fn_peek_front(d, &out)) { \
        return fn_some(out); \
    } \
    return fn_none(); \
}

/**
 * @brief Returns back element as Option<type> without removing it
 *
 * Generated function signature:
 * ```c
 * OptionType fn(borrowed(const DequeType*) d);
 * ```
 *
 * @pre d != NULL (programming error — triggers require_msg via peek_back)
 *
 * @post Returns None if d->size == 0
 * @post d is unchanged
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_PEEK_BACK_OPTION(linkage, DequeType, fn, fn_peek_back, OptionType, fn_some, fn_none, type) \
linkage OptionType fn(borrowed(const DequeType*) d) { \
    type out = {0}; \
    if (fn_peek_back(d, &out)) { \
        return fn_some(out); \
    } \
    return fn_none(); \
}

/* ════════════════════════════════════════════════════════════════════════════
   Misc operations
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resets the deque to empty state (O(1), does not zero buffer)
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(DequeType*) d);
 * ```
 *
 * @post d->size == 0, d->head == 0, d->tail == 0
 * @post Buffer contents are NOT zeroed — only logical state is reset
 * @note NULL-safe — does nothing if d == NULL
 * @note For sensitive data, zero the buffer manually before calling clear
 *
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT touch lt. The buffer is unchanged
 * and existing borrows to addresses inside it still point at valid memory.
 * The substrate does not catch use-of-removed-element — that bug class is
 * out of scope (matches vec/dynvec contract).
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_CLEAR(linkage, DequeType, fn) \
linkage void fn(borrowed(DequeType*) d) { \
    if (!d) { return; } \
    d->size = 0; \
    d->head = 0; \
    d->tail = 0; \
}

/**
 * @brief Swaps two deques in O(1) by exchanging their struct values
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(DequeType*) a, borrowed(DequeType*) b);
 * ```
 *
 * @pre a != NULL
 * @pre b != NULL
 *
 * Lifetime (CANON_LIFETIME_DEBUG): the struct copy exchanges the entire
 * DequeType including the embedded lt field. After swap, &a->lt still
 * points at a's struct but contains b's former (id, open) pair.
 * A borrow that captured a's old id will read the new id (= b's old id)
 * and mismatch — the safety check fires correctly without an explicit
 * restamp call.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — struct copy on stack
 */
#define IMPL_DEQUE_SWAP(linkage, DequeType, fn) \
linkage void fn(borrowed(DequeType*) a, borrowed(DequeType*) b) { \
    require_msg(a != NULL, #fn ": a cannot be NULL"); \
    require_msg(b != NULL, #fn ": b cannot be NULL"); \
    DequeType tmp = *a; \
    *a = *b; \
    *b = tmp; \
}

#endif /* CANON_DATA_DEQUE_IMPL_H */
