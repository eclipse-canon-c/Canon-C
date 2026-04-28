#ifndef CANON_DATA_VEC_IMPL_H
#define CANON_DATA_VEC_IMPL_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/checked.h"
#include "core/memory.h"
#include "core/arena.h"
#include "core/ownership.h"              /* borrowed(), owned() */
#include "semantics/result/result.h"
#include "semantics/error.h"

/**
 * @file vec_impl.h
 * @brief Pure implementation logic macros for the Canon-C vec module
 *
 * Contains all function body macros. No naming decisions are made here —
 * every identifier is received as a parameter from vec_defn.h.
 * This strict separation means you can change all names in vec_mangle.h
 * without touching any logic here.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero naming assumptions — all identifiers are parameters
 * - All size arithmetic is overflow-protected via checked.h
 * - Preconditions use require_msg(), debug invariants use ensure_msg()
 * - usize used throughout — no raw size_t
 * - mem_copy/mem_move used — no raw memcpy/memmove
 * - mem_alloc/mem_free used — no raw malloc/free
 * - CANON_VEC_MAX_CAPACITY enforced at construction time
 * - All pointer parameters annotated with borrowed() per ownership.h conventions
 *
 * Do NOT include this file directly. Use:
 * - data/vec/vec_defn.h  — to instantiate a typed vector
 * - data/vec/vec.h       — for the full user-facing API
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * vec_impl.h may only include from core/ and semantics/.
 * data/range.h and data/stringbuf.h are NOT included here —
 * they belong in the extension files vec_range.h and vec_fmt.h.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All generated functions are thread-safe per instance.
 * Concurrent access to the same vector instance requires external locking.
 *
 * @sa vec_mangle.h, vec_defn.h, vec_decl.h, vec.h
 */

/* ────────────────────────────────────────────────────────────────────────────
   result__Bool_Error instantiation
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Instantiate result__Bool_Error exactly once across all translation units
 *
 * push, pop, insert, remove all return result__Bool_Error.
 * Guard prevents duplicate definition if multiple vec types are instantiated.
 *
 * Note: CANON_RESULT(bool, Error) token-pastes to result__Bool_Error because
 * bool expands to _Bool in C99. The generated type name is result__Bool_Error,
 * not result_bool_Error. All push/pop/insert/remove signatures and call sites
 * use the correct generated name.
 */
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    CANON_RESULT(bool, Error)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_STRUCTS — struct typedefs for vec, iter, slice
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates the three struct typedefs for a typed vector
 *
 * Produces:
 * - VecType   — the vector struct (items, len, capacity)
 * - IterType  — forward iterator struct (vec pointer + index)
 * - SliceType — zero-copy view struct (items pointer + len)
 *
 * @param VecType   Mangled vector type name
 * @param VecTag    Mangled vector struct tag
 * @param IterType  Mangled iterator type name
 * @param IterTag   Mangled iterator struct tag
 * @param SliceType Mangled slice type name
 * @param SliceTag  Mangled slice struct tag
 * @param type      Element type
 *
 * Performance:
 * - Time:  O(1) — compile-time only
 * - Space: sizeof(VecType) = sizeof(type*) + 2*sizeof(usize)
 *          sizeof(IterType) = sizeof(VecType*) + sizeof(usize)
 *          sizeof(SliceType) = sizeof(type*) + sizeof(usize)
 */
#define IMPL_VEC_STRUCTS(VecType, VecTag, IterType, IterTag, SliceType, SliceTag, type) \
\
typedef struct VecTag { \
    type* items;    /**< Caller-owned element buffer (NULL iff capacity == 0) */ \
    usize len;      /**< Current number of valid elements (0 <= len <= capacity) */ \
    usize capacity; /**< Maximum elements buffer can hold */ \
} VecType; \
\
typedef struct IterTag { \
    VecType* vec;   /**< Vector being iterated (invalidated by any modification) */ \
    usize    index; /**< Current position (0 = start) */ \
} IterType; \
\
typedef struct SliceTag { \
    type* items; /**< Pointer into parent vector's buffer (not owned) */ \
    usize len;   /**< Number of elements in the slice */ \
} SliceType;

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_INIT — caller-owned buffer constructor
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a vector using a caller-owned buffer
 *
 * Generated function signature:
 * ```c
 * VecType fn(borrowed(type*) buffer, usize capacity);
 * ```
 *
 * @pre buffer != NULL || capacity == 0
 * @pre capacity <= CANON_VEC_MAX_CAPACITY / sizeof(type)
 *
 * @post result.items == buffer
 * @post result.len == 0
 * @post result.capacity == capacity
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation, wraps caller-provided buffer
 */
#define IMPL_VEC_INIT(linkage, VecType, fn, type) \
linkage VecType fn(borrowed(type*) buffer, usize capacity) { \
    require_msg(buffer != NULL || capacity == 0, \
        #fn ": buffer cannot be NULL when capacity > 0"); \
    if (capacity > CANON_VEC_MAX_CAPACITY / sizeof(type)) { \
        return (VecType){0}; \
    } \
    return (VecType){ .items = buffer, .len = 0, .capacity = capacity }; \
}

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_EMPTY — null/zero constructor
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns an empty zero-initialized vector (no buffer)
 *
 * Generated function signature:
 * ```c
 * VecType fn(void);
 * ```
 *
 * @post result.items == NULL
 * @post result.len == 0
 * @post result.capacity == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_EMPTY(linkage, VecType, fn) \
linkage VecType fn(void) { \
    return (VecType){0}; \
}

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_ALLOC — heap allocation constructor
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocates a vector buffer on the heap with fixed capacity
 *
 * Generated function signature:
 * ```c
 * VecType fn_alloc(usize capacity);
 * ```
 *
 * @pre capacity > 0 (capacity == 0 returns empty vec, not an error)
 * @pre capacity <= CANON_VEC_MAX_CAPACITY / sizeof(type)
 *
 * @post On success: result.items != NULL, result.capacity == capacity, result.len == 0
 * @post On failure (OOM or overflow): returns fn_empty()
 *
 * @note Caller must call fn_free() when done.
 * @note Capacity is FIXED — no automatic growth.
 *       For auto-growing vectors, use data/convenience/dynvec.h.
 *
 * Performance:
 * - Time:  O(1) amortized (single mem_alloc)
 * - Space: O(capacity * sizeof(type))
 */
#define IMPL_VEC_ALLOC(linkage, VecType, fn_alloc, fn_empty, fn_init, type) \
linkage VecType fn_alloc(usize capacity) { \
    if (capacity == 0) return fn_empty(); \
    if (capacity > CANON_VEC_MAX_CAPACITY / sizeof(type)) { \
        return fn_empty(); \
    } \
    usize total_bytes; \
    if (!checked_mul(capacity, (usize)sizeof(type), &total_bytes)) { \
        return fn_empty(); \
    } \
    type* buf = (type*)mem_alloc(total_bytes); \
    if (!buf) return fn_empty(); \
    return fn_init(buf, capacity); \
}

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_FREE — heap deallocation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Frees a heap-allocated vector's buffer
 *
 * Generated function signature:
 * ```c
 * void fn(dropped(VecType*) v);
 * ```
 *
 * @pre v was allocated via fn_alloc() — NOT stack or arena backed
 *
 * @post v->items == NULL
 * @post v->len == 0
 * @post v->capacity == 0
 *
 * @note NULL-safe — does nothing if v == NULL or v->items == NULL
 * @note Does NOT free elements pointed to by items (caller responsibility)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_FREE(linkage, VecType, fn) \
linkage void fn(dropped(VecType*) v) { \
    if (!v || !v->items) return; \
    mem_free(v->items); \
    v->items    = NULL; \
    v->len      = 0; \
    v->capacity = 0; \
}

/* ════════════════════════════════════════════════════════════════════════════
   IMPL_VEC_ARENA_ALLOC — arena allocation constructor
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocates a vector buffer from an arena with fixed capacity
 *
 * Generated function signature:
 * ```c
 * VecType fn_alloc(borrowed(Arena*) arena, usize capacity);
 * ```
 *
 * @pre arena != NULL and initialized via arena_init()
 * @pre capacity <= CANON_VEC_MAX_CAPACITY / sizeof(type)
 *
 * @post On success: result.items points into arena, result.capacity == capacity
 * @post On failure (insufficient arena space or overflow): returns fn_empty()
 *
 * @note Buffer lifetime is tied to the arena — no free() required.
 * @note Capacity is FIXED — arena-backed vectors cannot grow.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(capacity * sizeof(type)) consumed from arena
 */
#define IMPL_VEC_ARENA_ALLOC(linkage, VecType, fn_alloc, fn_empty, fn_init, type) \
linkage VecType fn_alloc(borrowed(Arena*) arena, usize capacity) { \
    if (!arena || capacity == 0) return fn_empty(); \
    if (capacity > CANON_VEC_MAX_CAPACITY / sizeof(type)) { \
        return fn_empty(); \
    } \
    usize total_bytes; \
    if (!checked_mul(capacity, (usize)sizeof(type), &total_bytes)) { \
        return fn_empty(); \
    } \
    type* buf = (type*)arena_alloc(arena, total_bytes); \
    if (!buf) return fn_empty(); \
    return fn_init(buf, capacity); \
}

/* ════════════════════════════════════════════════════════════════════════════
   Query functions
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the current number of elements
 *
 * Generated function signature:
 * ```c
 * usize fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns 0 if v == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_LEN(linkage, VecType, fn) \
linkage usize fn(borrowed(const VecType*) v) { \
    return v ? v->len : 0; \
}

/**
 * @brief Returns the maximum number of elements
 *
 * Generated function signature:
 * ```c
 * usize fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns 0 if v == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_CAPACITY(linkage, VecType, fn) \
linkage usize fn(borrowed(const VecType*) v) { \
    return v ? v->capacity : 0; \
}

/**
 * @brief Returns remaining free slots (capacity - len)
 *
 * Generated function signature:
 * ```c
 * usize fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns 0 if v == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_REMAINING(linkage, VecType, fn) \
linkage usize fn(borrowed(const VecType*) v) { \
    return v ? (v->capacity - v->len) : 0; \
}

/**
 * @brief Returns true if the vector has no elements
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns true if v == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_IS_EMPTY(linkage, VecType, fn) \
linkage bool fn(borrowed(const VecType*) v) { \
    return !v || v->len == 0; \
}

/**
 * @brief Returns true if the vector is at capacity
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns true if v == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_IS_FULL(linkage, VecType, fn) \
linkage bool fn(borrowed(const VecType*) v) { \
    return !v || v->len >= v->capacity; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Element access
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Copies element at index into *out (bounds-checked)
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(const VecType*) v, usize i, borrowed(type*) out);
 * ```
 *
 * @post *out is set if return is true
 * @post Returns false if v == NULL, out == NULL, or i >= v->len
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_GET(linkage, VecType, fn, type) \
linkage bool fn(borrowed(const VecType*) v, usize i, borrowed(type*) out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->items[i]; \
    return true; \
}

/**
 * @brief Returns element at index as Option<type>
 *
 * Generated function signature:
 * ```c
 * OptionType fn(borrowed(const VecType*) v, usize i);
 * ```
 *
 * @post Returns None if v == NULL or i >= v->len
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_GET_OPTION(linkage, VecType, fn, OptionType, fn_some, fn_none, type) \
linkage OptionType fn(borrowed(const VecType*) v, usize i) { \
    if (!v || i >= v->len) return fn_none(); \
    return fn_some(v->items[i]); \
}

/**
 * @brief Returns element at index without bounds checking
 *
 * Generated function signature:
 * ```c
 * type fn(borrowed(const VecType*) v, usize i);
 * ```
 *
 * @pre v != NULL
 * @pre v->items != NULL
 * @pre i < v->len — caller must guarantee this
 *
 * @note Preconditions checked via ensure_msg() in debug builds only.
 *       Undefined behavior in release builds if violated.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_GET_UNCHECKED(linkage, VecType, fn, type) \
linkage type fn(borrowed(const VecType*) v, usize i) { \
    ensure_msg(v != NULL,        #fn ": v cannot be NULL"); \
    ensure_msg(v->items != NULL, #fn ": v->items cannot be NULL"); \
    ensure_msg(i < v->len,       #fn ": index out of bounds"); \
    return v->items[i]; \
}

/**
 * @brief Returns a borrowed pointer to the element at index (bounds-checked via ensure)
 *
 * Generated function signature:
 * ```c
 * borrowed(type*) fn(borrowed(const VecType*) v, usize i);
 * ```
 *
 * @pre v != NULL
 * @pre i < v->len
 *
 * @note Use for in-place mutation. Pointer is invalidated by any modification.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_AT(linkage, VecType, fn, type) \
linkage borrowed(type*) fn(borrowed(const VecType*) v, usize i) { \
    ensure_msg(v != NULL,  #fn ": v cannot be NULL"); \
    ensure_msg(i < v->len, #fn ": index out of bounds"); \
    return &v->items[i]; \
}

/**
 * @brief Sets element at index (bounds-checked)
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(VecType*) v, usize i, type val);
 * ```
 *
 * @post Returns false if v == NULL or i >= v->len
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_SET(linkage, VecType, fn, type) \
linkage bool fn(borrowed(VecType*) v, usize i, type val) { \
    if (!v || i >= v->len) return false; \
    v->items[i] = val; \
    return true; \
}

/**
 * @brief Returns a borrowed pointer to the first element, or NULL if empty
 *
 * Generated function signature:
 * ```c
 * borrowed(type*) fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns NULL if v == NULL or v->len == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_FIRST(linkage, VecType, fn, type) \
linkage borrowed(type*) fn(borrowed(const VecType*) v) { \
    return (v && v->len > 0) ? &v->items[0] : NULL; \
}

/**
 * @brief Returns a borrowed pointer to the last element, or NULL if empty
 *
 * Generated function signature:
 * ```c
 * borrowed(type*) fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns NULL if v == NULL or v->len == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_LAST(linkage, VecType, fn, type) \
linkage borrowed(type*) fn(borrowed(const VecType*) v) { \
    return (v && v->len > 0) ? &v->items[v->len - 1] : NULL; \
}

/**
 * @brief Returns a borrowed pointer to the raw buffer
 *
 * Generated function signature:
 * ```c
 * borrowed(type*) fn(borrowed(const VecType*) v);
 * ```
 *
 * @post Returns NULL if v == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_DATA(linkage, VecType, fn, type) \
linkage borrowed(type*) fn(borrowed(const VecType*) v) { \
    return v ? v->items : NULL; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Modification — push
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Appends one element, returns result__Bool_Error
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(VecType*) v, type item);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)       if v == NULL or v->items == NULL
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if v->len >= v->capacity
 * @post On Ok: v->len incremented by 1, item stored at v->items[old_len]
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation
 */
#define IMPL_VEC_PUSH(linkage, VecType, fn, type) \
linkage result__Bool_Error fn(borrowed(VecType*) v, type item) { \
    if (!v || !v->items) return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (v->len >= v->capacity) return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    v->items[v->len++] = item; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Appends one element, returns bool (no Result overhead)
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(VecType*) v, type item);
 * ```
 *
 * @post Returns false if v == NULL, v->items == NULL, or v is full
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_TRY_PUSH(linkage, VecType, fn, type) \
linkage bool fn(borrowed(VecType*) v, type item) { \
    if (!v || !v->items || v->len >= v->capacity) return false; \
    v->items[v->len++] = item; \
    return true; \
}

/**
 * @brief Appends one element without any checking (fast path)
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(VecType*) v, type item);
 * ```
 *
 * @pre v != NULL
 * @pre v->items != NULL
 * @pre v->len < v->capacity — caller must guarantee this
 *
 * @note Preconditions enforced by ensure_msg() in debug builds only.
 *       Undefined behavior in release builds if violated.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_PUSH_UNCHECKED(linkage, VecType, fn, type) \
linkage void fn(borrowed(VecType*) v, type item) { \
    ensure_msg(v != NULL,            #fn ": v cannot be NULL"); \
    ensure_msg(v->items != NULL,     #fn ": v->items cannot be NULL"); \
    ensure_msg(v->len < v->capacity, #fn ": vector is full"); \
    v->items[v->len++] = item; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Modification — pop
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Removes and returns last element via result__Bool_Error
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(VecType*) v, borrowed(type*) out);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)   if v == NULL, out == NULL, or v->items == NULL
 * @post Returns Err(ERR_INVALID_STATE) if v->len == 0
 * @post On Ok: v->len decremented by 1, *out holds removed element
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_POP(linkage, VecType, fn, type) \
linkage result__Bool_Error fn(borrowed(VecType*) v, borrowed(type*) out) { \
    if (!v || !out || !v->items) return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (v->len == 0) return result__Bool_Error_err(ERR_INVALID_STATE); \
    *out = v->items[--v->len]; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Removes and returns last element as Option<type>
 *
 * Generated function signature:
 * ```c
 * OptionType fn(borrowed(VecType*) v);
 * ```
 *
 * @post Returns None if v == NULL, v->items == NULL, or v->len == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_POP_OPTION(linkage, VecType, fn, fn_pop, OptionType, fn_some, fn_none, fn_result_is_ok, type) \
linkage OptionType fn(borrowed(VecType*) v) { \
    type out = {0}; \
    if (fn_result_is_ok(fn_pop(v, &out))) \
        return fn_some(out); \
    return fn_none(); \
}

/**
 * @brief Resets length to 0 (does not zero memory)
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(VecType*) v);
 * ```
 *
 * @note NULL-safe — does nothing if v == NULL
 * @note Memory contents are not zeroed. For sensitive data, zero manually first.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_CLEAR(linkage, VecType, fn) \
linkage void fn(borrowed(VecType*) v) { \
    if (v) v->len = 0; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Modification — insert / remove
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Inserts element at index i, shifting elements right
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(VecType*) v, usize i, type item);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)       if v or v->items is NULL
 * @post Returns Err(ERR_OUT_OF_RANGE)      if i > v->len
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if v->len >= v->capacity
 * @post On Ok: element at i is item, elements [i..old_len) shifted right by 1
 *
 * Performance:
 * - Time:  O(n) — shifts up to n elements right
 * - Space: O(1) — no allocation
 */
#define IMPL_VEC_INSERT(linkage, VecType, fn, type) \
linkage result__Bool_Error fn(borrowed(VecType*) v, usize i, type item) { \
    if (!v || !v->items)       return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (i > v->len)            return result__Bool_Error_err(ERR_OUT_OF_RANGE); \
    if (v->len >= v->capacity) return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    if (i < v->len) { \
        mem_move(&v->items[i + 1], &v->items[i], (v->len - i) * sizeof(type)); \
    } \
    v->items[i] = item; \
    v->len++; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Removes element at index i, shifting elements left, returns via Result
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(VecType*) v, usize i, borrowed(type*) out);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)   if v, v->items, or out is NULL
 * @post Returns Err(ERR_INVALID_STATE) if v->len == 0
 * @post Returns Err(ERR_OUT_OF_RANGE)  if i >= v->len
 * @post On Ok: *out holds removed element, elements [i+1..len) shifted left by 1
 *
 * Performance:
 * - Time:  O(n) — shifts up to n elements left
 * - Space: O(1)
 */
#define IMPL_VEC_REMOVE(linkage, VecType, fn, type) \
linkage result__Bool_Error fn(borrowed(VecType*) v, usize i, borrowed(type*) out) { \
    if (!v || !v->items || !out) return result__Bool_Error_err(ERR_INVALID_ARG); \
    if (v->len == 0)             return result__Bool_Error_err(ERR_INVALID_STATE); \
    if (i >= v->len)             return result__Bool_Error_err(ERR_OUT_OF_RANGE); \
    *out = v->items[i]; \
    if (i < v->len - 1) { \
        mem_move(&v->items[i], &v->items[i + 1], (v->len - i - 1) * sizeof(type)); \
    } \
    v->len--; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Removes element at index i, returns as Option<type>
 *
 * Generated function signature:
 * ```c
 * OptionType fn(borrowed(VecType*) v, usize i);
 * ```
 *
 * @post Returns None if v == NULL, v->items == NULL, v->len == 0, or i >= v->len
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 */
#define IMPL_VEC_REMOVE_OPTION(linkage, VecType, fn, fn_remove, OptionType, fn_some, fn_none, fn_result_is_ok, type) \
linkage OptionType fn(borrowed(VecType*) v, usize i) { \
    type out = {0}; \
    if (fn_result_is_ok(fn_remove(v, i, &out))) \
        return fn_some(out); \
    return fn_none(); \
}

/* ════════════════════════════════════════════════════════════════════════════
   Bulk operations
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Appends count elements from src array
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(VecType*) v, borrowed(const type*) src, usize count);
 * ```
 *
 * @post Returns Err(ERR_INVALID_ARG)       if v, v->items, or src is NULL
 * @post Returns Err(ERR_OVERFLOW)          if v->len + count overflows usize
 * @post Returns Err(ERR_CAPACITY_EXCEEDED) if total > v->capacity
 * @post On Ok: v->len increased by count, elements copied from src
 *
 * Performance:
 * - Time:  O(count)
 * - Space: O(1) — no allocation, copies into existing buffer
 */
#define IMPL_VEC_APPEND_ARRAY(linkage, VecType, fn, type) \
linkage result__Bool_Error fn(borrowed(VecType*) v, borrowed(const type*) src, usize count) { \
    if (!v || !v->items || !src) return result__Bool_Error_err(ERR_INVALID_ARG); \
    usize total; \
    if (!checked_add(v->len, count, &total)) \
        return result__Bool_Error_err(ERR_OVERFLOW); \
    if (total > v->capacity) \
        return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    mem_copy(&v->items[v->len], src, count * sizeof(type)); \
    v->len = total; \
    return result__Bool_Error_ok(true); \
}

/**
 * @brief Appends count elements from src (alias for append_array)
 *
 * Generated function signature:
 * ```c
 * result__Bool_Error fn(borrowed(VecType*) v, borrowed(const type*) src, usize count);
 * ```
 *
 * @sa IMPL_VEC_APPEND_ARRAY — identical semantics
 *
 * Performance:
 * - Time:  O(count)
 * - Space: O(1)
 */
#define IMPL_VEC_EXTEND(linkage, VecType, fn, fn_append_array, type) \
linkage result__Bool_Error fn(borrowed(VecType*) v, borrowed(const type*) src, usize count) { \
    return fn_append_array(v, src, count); \
}

/**
 * @brief Appends value repeated count times (up to remaining capacity)
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(VecType*) v, type value, usize count);
 * ```
 *
 * @note Silently fills min(count, remaining) elements — does not error on overflow.
 *       If strict filling is required, check remaining() before calling.
 * @note NULL-safe — does nothing if v == NULL or v->items == NULL
 *
 * Performance:
 * - Time:  O(min(count, remaining))
 * - Space: O(1)
 */
#define IMPL_VEC_FILL(linkage, VecType, fn, type) \
linkage void fn(borrowed(VecType*) v, type value, usize count) { \
    if (!v || !v->items) return; \
    usize remaining = v->capacity - v->len; \
    usize to_fill   = (count < remaining) ? count : remaining; \
    for (usize i = 0; i < to_fill; i++) { \
        v->items[v->len + i] = value; \
    } \
    v->len += to_fill; \
}

/**
 * @brief Swaps two vectors in O(1) by exchanging their struct values
 *
 * Generated function signature:
 * ```c
 * void fn(borrowed(VecType*) a, borrowed(VecType*) b);
 * ```
 *
 * @pre a != NULL
 * @pre b != NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — struct copy on stack
 */
#define IMPL_VEC_SWAP(linkage, VecType, fn) \
linkage void fn(borrowed(VecType*) a, borrowed(VecType*) b) { \
    require_msg(a != NULL, #fn ": a cannot be NULL"); \
    require_msg(b != NULL, #fn ": b cannot be NULL"); \
    VecType tmp = *a; \
    *a = *b; \
    *b = tmp; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Iterator
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates a forward iterator positioned at the first element
 *
 * Generated function signature:
 * ```c
 * IterType fn(borrowed(VecType*) v);
 * ```
 *
 * @note Invalidated by any modification to v.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — iterator is stack-allocated
 */
#define IMPL_VEC_ITER_INIT(linkage, IterType, fn, VecType) \
linkage IterType fn(borrowed(VecType*) v) { \
    return (IterType){ .vec = v, .index = 0 }; \
}

/**
 * @brief Advances iterator and copies next element into *out
 *
 * Generated function signature:
 * ```c
 * bool fn(borrowed(IterType*) it, borrowed(type*) out);
 * ```
 *
 * @post Returns false when iteration is exhausted
 * @post *out is set on true return only
 *
 * Typical usage:
 * ```c
 * vec_int_iter it = vec_int_iter_init(&v);
 * int val;
 * while (vec_int_iter_next(&it, &val)) {
 *     // use val
 * }
 * ```
 *
 * Performance:
 * - Time:  O(1) per call
 * - Space: O(1)
 */
#define IMPL_VEC_ITER_NEXT(linkage, IterType, fn, type) \
linkage bool fn(borrowed(IterType*) it, borrowed(type*) out) { \
    if (!it || !it->vec || !out) return false; \
    if (it->index >= it->vec->len) return false; \
    *out = it->vec->items[it->index++]; \
    return true; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Slice
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates a zero-copy slice view of elements [start, end)
 *
 * Generated function signature:
 * ```c
 * SliceType fn(borrowed(VecType*) v, usize start, usize end);
 * ```
 *
 * @post result.items points into v's buffer at &v->items[start]
 * @post result.len == end - start
 * @post Returns empty slice (items=NULL, len=0) on invalid range
 *
 * @note Invalidated by any modification to v.
 * @note Does not copy data — O(1) regardless of slice size.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation, pointer into existing buffer
 */
#define IMPL_VEC_SLICE_INIT(linkage, SliceType, fn, VecType, type) \
linkage SliceType fn(borrowed(VecType*) v, usize start, usize end) { \
    SliceType s = {0}; \
    if (!v || start > end || end > v->len) return s; \
    s.items = &v->items[start]; \
    s.len   = end - start; \
    return s; \
}

/**
 * @brief Returns a borrowed pointer to slice element at index (debug-checked)
 *
 * Generated function signature:
 * ```c
 * borrowed(type*) fn(borrowed(const SliceType*) s, usize i);
 * ```
 *
 * @pre s != NULL
 * @pre i < s->len — caller must guarantee this
 *
 * @note Preconditions enforced by ensure_msg() in debug builds only.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
#define IMPL_VEC_SLICE_GET(linkage, SliceType, fn, type) \
linkage borrowed(type*) fn(borrowed(const SliceType*) s, usize i) { \
    ensure_msg(s != NULL,  #fn ": s cannot be NULL"); \
    ensure_msg(i < s->len, #fn ": index out of bounds"); \
    return &s->items[i]; \
}

#endif /* CANON_DATA_VEC_IMPL_H */
