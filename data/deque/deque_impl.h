#ifndef CANON_DATA_DEQUE_IMPL_H
#define CANON_DATA_DEQUE_IMPL_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"      /* CANON_DEQUE_MAX_CAPACITY fallback: CANON_VEC_MAX_CAPACITY */
#include "core/primitives/contract.h"    /* require_msg, ensure_msg */
#include "core/ownership.h"              /* borrowed() */
#include "semantics/result/result.h"     /* CANON_RESULT */
#include "semantics/error.h"             /* Error, ERR_* */

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
 * @param DequeType Mangled deque type name
 * @param DequeTag  Mangled deque struct tag
 * @param type      Element type
 *
 * Performance:
 * - Time:  O(1) — compile-time only
 * - Space: sizeof(DequeType) = sizeof(type*) + 4*sizeof(usize)
 *          Total memory: sizeof(DequeType) + capacity*sizeof(type)
 */
#define IMPL_DEQUE_STRUCT(DequeType, DequeTag, type) \
typedef struct DequeTag { \
    type* buffer;   /**< Caller-owned element buffer (must remain valid for lifetime) */ \
    usize capacity; /**< Fixed maximum number of elements (never changes after init) */ \
    usize head;     /**< Index of front element (pop_front reads here) */ \
    usize tail;     /**< Index where next push_back writes */ \
    usize size;     /**< Current element count (0 <= size <= capacity) */ \
} DequeType;

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
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation, wraps caller-provided buffer
 */
#define IMPL_DEQUE_INIT(linkage, DequeType, fn, type) \
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
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_EMPTY(linkage, DequeType, fn) \
linkage DequeType fn(void) { \
    return (DequeType){0}; \
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
    if (d->size == 0) return false; \
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
    if (d->size == 0) return false; \
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
    if (fn_peek_front(d, &out)) \
        return fn_some(out); \
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
    if (fn_peek_back(d, &out)) \
        return fn_some(out); \
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
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_DEQUE_CLEAR(linkage, DequeType, fn) \
linkage void fn(borrowed(DequeType*) d) { \
    if (!d) return; \
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
