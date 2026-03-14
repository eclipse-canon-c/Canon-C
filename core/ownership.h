#ifndef CANON_CORE_OWNERSHIP_H
#define CANON_CORE_OWNERSHIP_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"

/**
 * @file core/ownership.h
 * @brief Explicit ownership and borrowing annotations for Canon-C APIs
 *
 * Ownership is visible at every call site. Every function signature using
 * these annotations is machine-readable, Doxygen-visible, and grep-able.
 *
 * Core ideas:
 * ─────────────────────────────────────────────────────────────────────────
 * - Annotations only — no enforcement, no runtime overhead
 * - C99 has no destructors or move semantics — we do not pretend otherwise
 * - owned(T*)    → this pointer transfers ownership to the callee or caller
 * - borrowed(T*) → this pointer is a non-owning view; caller retains ownership
 * - moved(T*)    → value has been logically moved; do not use original after this
 * - dropped(T*)  → value has been released; pointer is now invalid
 *
 * IMPORTANT — macro argument convention:
 * ─────────────────────────────────────────────────────────────────────────
 * Always pass the full pointer type as T, including the asterisk:
 *
 *   owned(Arena*)         ← correct
 *   owned(Arena)          ← wrong — produces Arena, not Arena*
 *   borrowed(const char*) ← correct
 *
 * These macros expand to T exactly. The * must be part of the argument.
 * This is intentional: it keeps the macros pure C99 with zero magic.
 *
 * What this gives you:
 * ─────────────────────────────────────────────────────────────────────────
 * - API intent visible in declarations without reading documentation
 * - Doxygen and IDE tooling can surface ownership at a glance
 * - Grep-able: find all owned(T*) parameters across the codebase instantly
 * - No false safety: annotations are promises, not guarantees
 *
 * What this does NOT give you:
 * ─────────────────────────────────────────────────────────────────────────
 * - No compiler enforcement of ownership rules
 * - No prevention of use-after-move or double-free
 * - No automatic cleanup / destructors
 * - These are documentation tools, not type system features
 *
 * Naming convention for functions:
 * ─────────────────────────────────────────────────────────────────────────
 * - _create() / _init()  → return owned(T*) — transfer to caller; caller must free
 * - _free() / _destroy() → take owned(T*)   — consume and release; caller must not use after
 * - _push() / _set()     → take borrowed(T*) — do not consume; caller retains ownership
 * - _str() / _data()     → return borrowed(T*) — view into owned memory; caller must NOT free
 *
 * Dependency rule:
 * ─────────────────────────────────────────────────────────────────────────
 * ownership.h lives in core/ and may only include from core/primitives/.
 * contract.h is required for require_msg() used in DEFINE_OWNED helpers.
 *
 * Quick start:
 * ```c
 * owned(Arena*)         arena_create(owned(u8*) buffer, usize size);
 * owned(void*)          arena_alloc(borrowed(Arena*) arena, usize size);
 * void                  arena_destroy(owned(Arena*) arena);
 * borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb);
 * ```
 *
 * Optional: stronger type safety with wrappers
 * ─────────────────────────────────────────────────────────────────────────
 * Use DEFINE_OWNED / DEFINE_BORROWED when you want a distinct type that
 * cannot be accidentally passed where a raw pointer is expected.
 *
 * @sa semantics/borrow.h — borrowed_slice, borrowed_str built on top of this
 */

/* ══════════════════════════════════════════════════════════════════════════
   Lightweight annotation macros (typedef-based)
   ══════════════════════════════════════════════════════════════════════════
   These expand to T exactly — no runtime cost, no type system change.
   Their value is documentation, grep-ability, and Doxygen visibility.

   Always pass T with its asterisk: owned(Arena*), borrowed(const char*)
   ══════════════════════════════════════════════════════════════════════════ */

/**
 * @def owned(T)
 * @brief Annotates a pointer as transferring ownership
 *
 * Pass the full pointer type: owned(Arena*), owned(u8*)
 *
 * On a parameter: the function takes ownership — caller must not use or free
 * the pointer afterward.
 *
 * On a return type: the caller receives ownership and is responsible for
 * eventually freeing or dropping the value.
 *
 * Expands to T exactly at the compiler level.
 */
#define owned(T) T

/**
 * @def borrowed(T)
 * @brief Annotates a pointer as non-owning (view only)
 *
 * Pass the full pointer type: borrowed(Arena*), borrowed(const char*)
 *
 * On a parameter: the function does not take ownership — caller retains
 * responsibility for the lifetime and cleanup of the value.
 *
 * On a return type: the returned pointer is a view into memory owned by
 * something else — the caller must NOT free it and must not use it after
 * the owner is freed or reset.
 *
 * Expands to T exactly at the compiler level.
 */
#define borrowed(T) T

/**
 * @def moved(T)
 * @brief Annotates a pointer as being logically moved
 *
 * Pass the full pointer type: moved(Arena*)
 *
 * Signals that after this call, the original value should be treated as
 * invalid. C cannot enforce this — it is a promise from the caller.
 *
 * Expands to T exactly at the compiler level.
 */
#define moved(T) T

/**
 * @def dropped(T)
 * @brief Annotates a pointer that has been released and is now invalid
 *
 * Pass the full pointer type: dropped(Arena*)
 *
 * Used on parameters of _free() / _drop() functions to signal that the
 * value is consumed and must not be dereferenced after the call.
 *
 * Example:
 *   void arena_destroy(dropped(Arena*) arena);
 *
 * Expands to T exactly at the compiler level.
 */
#define dropped(T) T

/* ══════════════════════════════════════════════════════════════════════════
   Drop helpers — explicit resource release convention
   ══════════════════════════════════════════════════════════════════════════ */

/**
 * @def CANON_DROP(ptr, free_fn)
 * @brief Explicitly drops an owned pointer and NULLs it
 *
 * Calls free_fn(ptr), then sets ptr to NULL to prevent use-after-free.
 * Asserts that both ptr and free_fn are non-NULL before calling.
 *
 * @param ptr     Owned pointer to release (must be a non-NULL lvalue)
 * @param free_fn Cleanup function compatible with void fn(T*); must be non-NULL
 */
#define CANON_DROP(ptr, free_fn)                                              \
    do {                                                                       \
        require_msg((ptr)     != NULL, "CANON_DROP: ptr cannot be NULL");     \
        require_msg((free_fn) != NULL, "CANON_DROP: free_fn cannot be NULL"); \
        (free_fn)(ptr);                                                        \
        (ptr) = NULL;                                                          \
    } while (0)

/**
 * @def CANON_DROP_IF(ptr, free_fn)
 * @brief Drops an owned pointer only if it is non-NULL, then NULLs it
 *
 * Safe variant of CANON_DROP for cases where ptr may legitimately be NULL
 * (e.g., lazily-initialized resources). Asserts free_fn is non-NULL.
 *
 * @param ptr     Owned pointer to release (lvalue; may be NULL)
 * @param free_fn Cleanup function compatible with void fn(T*); must be non-NULL
 */
#define CANON_DROP_IF(ptr, free_fn)                                               \
    do {                                                                           \
        require_msg((free_fn) != NULL, "CANON_DROP_IF: free_fn cannot be NULL");  \
        if ((ptr) != NULL) {                                                       \
            (free_fn)(ptr);                                                        \
            (ptr) = NULL;                                                          \
        }                                                                          \
    } while (0)

/* ══════════════════════════════════════════════════════════════════════════
   DEFINE_OWNED / DEFINE_BORROWED — struct-based strong wrappers (optional)
   ══════════════════════════════════════════════════════════════════════════
   Use the annotation macros above for most APIs.
   Use DEFINE_OWNED / DEFINE_BORROWED when you want a distinct type that
   cannot be accidentally passed where a raw pointer is expected.

   Requires: contract.h (for require_msg)
   ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a strongly-typed owned wrapper for a pointer-to-type
 *
 * Creates struct `owned_##type` wrapping `type*`. Cannot be implicitly
 * converted to a raw pointer — the caller must explicitly unwrap or drop it.
 *
 * Generated type:    owned_##type
 * Generated helpers:
 *   owned_T_wrap(ptr)         — wrap a raw pointer (takes ownership)
 *   owned_T_unwrap(o)         — extract raw pointer; marks wrapper as consumed
 *   owned_T_borrow(o)         — read raw pointer without consuming
 *   owned_T_is_valid(o)       — returns true if wrapper holds a non-NULL pointer
 *   owned_T_drop(o, free_fn)  — free the held pointer and NULL it
 *
 * @note owned_T_unwrap asserts the wrapper has not already been consumed.
 *       Calling it twice on the same wrapper is a contract violation.
 */
#define DEFINE_OWNED(type)                                                          \
                                                                                    \
typedef struct {                                                                    \
    type* ptr; /**< Owned pointer — must be dropped before going out of scope */   \
} owned_##type;                                                                     \
                                                                                    \
static inline owned_##type owned_##type##_wrap(type* ptr) {                        \
    return (owned_##type){ .ptr = ptr };                                            \
}                                                                                   \
                                                                                    \
static inline type* owned_##type##_unwrap(owned_##type* o) {                       \
    type* raw;                                                                      \
    require_msg(o != NULL,      "owned_" #type "_unwrap: o cannot be NULL");        \
    require_msg(o->ptr != NULL, "owned_" #type "_unwrap: already consumed");        \
    raw    = o->ptr;                                                                \
    o->ptr = NULL;                                                                  \
    return raw;                                                                     \
}                                                                                   \
                                                                                    \
static inline type* owned_##type##_borrow(const owned_##type* o) {                 \
    require_msg(o != NULL, "owned_" #type "_borrow: o cannot be NULL");             \
    return o->ptr;                                                                  \
}                                                                                   \
                                                                                    \
static inline bool owned_##type##_is_valid(const owned_##type* o) {                \
    return o != NULL && o->ptr != NULL;                                             \
}                                                                                   \
                                                                                    \
static inline void owned_##type##_drop(                                             \
        owned_##type* o, void (*free_fn)(type*)) {                                  \
    require_msg(o != NULL,       "owned_" #type "_drop: o cannot be NULL");         \
    require_msg(free_fn != NULL, "owned_" #type "_drop: free_fn cannot be NULL");   \
    if (o->ptr != NULL) {                                                           \
        free_fn(o->ptr);                                                            \
        o->ptr = NULL;                                                              \
    }                                                                               \
}

/**
 * @brief Generates a strongly-typed borrowed wrapper for a const pointer-to-type
 *
 * Creates struct `borrowed_##type` wrapping `const type*`. Makes it clear
 * at the type level that this pointer is non-owning and must NOT be freed.
 *
 * Generated type:    borrowed_##type
 * Generated helpers:
 *   borrowed_T_from(ptr)   — wrap a raw const pointer (non-owning)
 *   borrowed_T_get(b)      — read the raw pointer
 *   borrowed_T_is_valid(b) — returns true if wrapper holds a non-NULL pointer
 */
#define DEFINE_BORROWED(type)                                                            \
                                                                                         \
typedef struct {                                                                         \
    const type* ptr; /**< Borrowed pointer — must NOT be freed by holder */             \
} borrowed_##type;                                                                       \
                                                                                         \
static inline borrowed_##type borrowed_##type##_from(const type* ptr) {                 \
    return (borrowed_##type){ .ptr = ptr };                                              \
}                                                                                        \
                                                                                         \
static inline const type* borrowed_##type##_get(const borrowed_##type* b) {             \
    require_msg(b != NULL, "borrowed_" #type "_get: b cannot be NULL");                  \
    return b->ptr;                                                                       \
}                                                                                        \
                                                                                         \
static inline bool borrowed_##type##_is_valid(const borrowed_##type* b) {               \
    return b != NULL && b->ptr != NULL;                                                  \
}

/* ══════════════════════════════════════════════════════════════════════════
   Ownership convention summary — for Doxygen and code review
   ══════════════════════════════════════════════════════════════════════════

   CALLER OWNS, FUNCTION BORROWS (most common read/write operations):
     void vec_push(borrowed(canon_vec_int*) v, int item);

   FUNCTION RETURNS OWNED (caller must free):
     owned(char*) str_duplicate(borrowed(const char*) src);

   FUNCTION TAKES OWNED (callee responsible after call):
     void dynvec_int_free(dropped(dynvec_int*) v);

   FUNCTION RETURNS BORROWED (caller must NOT free):
     borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb);

   LOGICAL MOVE (caller must not use original after call):
     void queue_push(borrowed(Queue*) q, moved(Node*) node);

   ══════════════════════════════════════════════════════════════════════════ */

#endif /* CANON_CORE_OWNERSHIP_H */
