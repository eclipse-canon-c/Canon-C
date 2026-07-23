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
 * - Annotations are documentation by default — zero runtime overhead
 * - C99 has no destructors or move semantics — we do not pretend otherwise
 * - owned(T*)    -> this pointer transfers ownership to the callee or caller
 * - borrowed(T*) -> this pointer is a non-owning view; caller retains ownership
 * - moved(T*)    -> value has been logically moved; do not use original after this
 * - dropped(T*)  -> value has been released; pointer is now invalid
 *
 * IMPORTANT — macro argument convention:
 * ─────────────────────────────────────────────────────────────────────────
 * Always pass the full pointer type as T, including the asterisk:
 *
 *   owned(Arena*)         -- correct
 *   owned(Arena)          -- wrong: produces Arena, not Arena*
 *   borrowed(const char*) -- correct
 *
 * These macros expand to T exactly. The * must be part of the argument.
 * This is intentional: it keeps the macros pure C99 with zero magic.
 *
 * What this gives you:
 * ─────────────────────────────────────────────────────────────────────────
 * - API intent visible in declarations without reading documentation
 * - Doxygen and IDE tooling can surface ownership at a glance
 * - Grep-able: find all owned(T*) parameters across the codebase instantly
 * - With CANON_LIFETIME_DEBUG: runtime detection of dangling borrows and
 *   use-after-reset at first misuse (see "build modes" section below)
 * - For compile-time prevention of mixing owned and borrowed types,
 *   use DEFINE_OWNED(T) and DEFINE_BORROWED(T) below — they generate
 *   distinct wrapper struct types that the compiler enforces at API
 *   boundaries. Opt-in per type, no build flag required.
 *
 * What this does NOT give you:
 * ─────────────────────────────────────────────────────────────────────────
 *   - In any build:           no automatic cleanup or destructors;
 *                             C99 has none, and Canon-C does not pretend
 *
 *   - In any build:           no prevention of use-after-move on raw
 *                             pointers; moved() is a caller promise
 *
 *   - Default build:          no compile-time prevention of mixing
 *                             owned() and borrowed() at call sites —
 *                             the annotation macros expand to T identically.
 *                             Use DEFINE_OWNED / DEFINE_BORROWED below
 *                             to opt into compile-time enforcement
 *                             per type.
 *
 *   - With CANON_LIFETIME_DEBUG: borrows captured before a source reset
 *                                or destruction DO fire require_msg at
 *                                first read; this catches the dangling-
 *                                borrow class of bugs at runtime
 *
 * Naming convention for functions:
 * ─────────────────────────────────────────────────────────────────────────
 * - _create() / _init()  -> return owned(T*) (transfer to caller; caller must free)
 * - _free() / _destroy() -> take owned(T*)   (consume and release; caller must not use after)
 * - _push() / _set()     -> take borrowed(T*) (do not consume; caller retains ownership)
 * - _str() / _data()     -> return borrowed(T*) (view into owned memory; caller must NOT free)
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
 * cannot be accidentally passed where a raw pointer is expected. The
 * generated owned_T / borrowed_T types have explicit wrap/unwrap/borrow/get
 * helpers that the compiler enforces at call sites.
 *
 * @sa semantics/borrow.h         — borrowed_slice, borrowed_str built on top of this
 * @sa core/primitives/lifetime.h — canonical home of lifetime_t and region_id_t
 * @sa core/region.h              — lifetime_assert_valid runtime check
 */

/* ══════════════════════════════════════════════════════════════════════════
   Ownership and lifetime build modes — policy summary
   ══════════════════════════════════════════════════════════════════════════
   Canon-C's ownership story has three mostly-independent layers:

   1. ANNOTATIONS (this file, all builds)
      The owned() / borrowed() / moved() / dropped() macros below are
      pure documentation. They expand to T in every build configuration.
      Their value is at the call site: signatures and code review.
      No build flag changes their behavior today.

   2. RUNTIME LIFETIME TRACKING (CANON_LIFETIME_DEBUG, opt-in)
      An opt-in debug-build mode in which ownership-bearing structs
      (Arena, dynvec, hashmap, stringbuf, ...) embed a lifetime token
      (lifetime_t = { region_id_t id; bool open; } from core/region.h)
      and borrow types capture the source's ID at construction. Borrow
      reads then assert that the source is still open and the captured
      ID still matches — catching dangling borrows and use-after-reset
      at first misuse via require_msg.

      Cost when off (the default):
        - Zero. No fields added, no checks compiled in, identical
          to today's behavior.

      Cost when on:
        - Each opted-in ownership-bearing struct grows by sizeof(lifetime_t)
          (typically 16 bytes — u64 + bool + padding).
        - Each borrow grows by one pointer + one u64 (typically 16 bytes).
        - Each borrow read performs two compares and one branch.

      Enable via CMake:
        cmake -DCANON_LIFETIME=debug -B build

      The flag is project-wide and must be consistent across all
      translation units that share Canon-C types — same constraint as
      NDEBUG. Mixing debug and release Canon-C objects in one binary
      is undefined.

      Mechanism source: core/region.h defines lifetime_t and
      lifetime_assert_valid; modules opting in embed lifetime_t directly.

   3. COMPILE-TIME ENFORCEMENT (DEFINE_OWNED / DEFINE_BORROWED, opt-in)
      For compile-time prevention of owned-vs-borrowed type confusion
      at API boundaries, use DEFINE_OWNED(T) and DEFINE_BORROWED(T)
      below. These generate distinct wrapper struct types — owned_T
      and borrowed_T — that the compiler refuses to mix.

      Opt-in per type, no build flag required. The cost is one macro
      invocation per type plus explicit wrap/unwrap/borrow at API
      boundaries. The benefit is real compile-time enforcement that
      catches type-level ownership confusion at the first use site.

      For deeper static analysis (use-after-move, double-drop, lifetime
      parameters across function calls) — properties C99's type system
      cannot enforce directly — use Frama-C, Polyspace, or LDRA on the
      source.

      Cost when not used:
        - Zero. The lowercase annotations remain documentation only.

      Cost when used:
        - One DEFINE_OWNED(T) per type (one-line declaration).
        - One wrap/unwrap/borrow helper at each API boundary.
        - The wrapper struct adds one pointer of indirection at runtime.

   Quick reference:
   ─────────────────────────────────────────────────────────────────────
     Default build              — annotations as documentation; zero cost
     CANON_LIFETIME_DEBUG       — adds runtime lifetime tracking
     DEFINE_OWNED(T) /
       DEFINE_BORROWED(T)       — opt-in compile-time enforcement per type
     CANON_STRICT               — promotes ensure_msg() to always-on; works
                                  whether or not lifetime tracking is enabled
   ══════════════════════════════════════════════════════════════════════════ */

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
        require_msg((ptr)     != NULL, "CANON_DROP: (ptr) cannot be NULL");   \
        require_msg((free_fn) != NULL, "CANON_DROP: (free_fn) cannot be NULL"); \
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
        require_msg((free_fn) != NULL, "CANON_DROP_IF: (free_fn) cannot be NULL"); \
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
 *   owned_T_wrap(ptr)         -- wrap a raw pointer (takes ownership)
 *   owned_T_unwrap(o)         -- extract raw pointer; marks wrapper as consumed
 *   owned_T_borrow(o)         -- read raw pointer without consuming
 *   owned_T_is_valid(o)       -- returns true if wrapper holds a non-NULL pointer
 *   owned_T_drop(o, free_fn)  -- free the held pointer and NULL it
 *
 * @note owned_T_unwrap asserts the wrapper has not already been consumed.
 *       Calling it twice on the same wrapper is a contract violation.
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_OWNED(type)                                                          \
                                                                                    \
typedef struct {                                                                    \
    type* ptr; /**< Owned pointer — must be dropped before going out of scope */   \
} owned_##type;                                                                     \
                                                                                    \
static inline owned_##type owned_##type##_wrap(type* ptr) {                      \
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
        owned_##type* o, void (*free_fn)(type*)) {                                \
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
 *   borrowed_T_from(ptr)   -- wrap a raw const pointer (non-owning)
 *   borrowed_T_get(b)      -- read the raw pointer
 *   borrowed_T_is_valid(b) -- returns true if wrapper holds a non-NULL pointer
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
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
     void vec_push(borrowed(vec_int*) v, int item);

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
