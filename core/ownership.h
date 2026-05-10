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
 * - With CANON_OWNERSHIP_STRICT and DEFINE_OWNED/DEFINE_BORROWED: compile-
 *   time prevention of mixing owned and borrowed types at call sites,
 *   via the OWN/TAKE/BORROW/GET ceremony macros below
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
 *                             the annotation macros expand to T identically
 *
 *   - With CANON_LIFETIME_DEBUG: borrows captured before a source reset
 *                                or destruction DO fire require_msg at
 *                                first read; this catches the dangling-
 *                                borrow class of bugs at runtime
 *
 *   - With CANON_OWNERSHIP_STRICT: when paired with DEFINE_OWNED-generated
 *                                  types and the OWN/TAKE/BORROW/GET
 *                                  ceremony macros, mismatches between
 *                                  owned and borrowed wrappers become
 *                                  compile errors. Raw owned(T*) /
 *                                  borrowed(T*) annotations are unaffected
 *                                  by this flag — they remain pure docs.
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
 * OWN/TAKE/BORROW/GET ceremony macros provide concise call-site syntax
 * for working with these types.
 *
 * @sa semantics/borrow.h — borrowed_slice, borrowed_str built on top of this
 * @sa core/region.h      — lifetime_t and lifetime_assert_valid drive
 *                          CANON_LIFETIME_DEBUG runtime checks
 */

/* ══════════════════════════════════════════════════════════════════════════
   Ownership and lifetime build modes — policy summary
   ══════════════════════════════════════════════════════════════════════════
   Canon-C's ownership story has three mostly-independent layers:

   1. ANNOTATIONS (this file, all builds)
      The owned() / borrowed() / moved() / dropped() macros below are
      pure documentation. They expand to T in every build configuration.
      Their value is at the call site: signatures and code review.
      No build flag changes their behavior.

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

   3. STRICT TYPED WRAPPERS (CANON_OWNERSHIP_STRICT, opt-in)
      An opt-in mode in which the OWN/TAKE/BORROW/GET ceremony macros
      become non-trivial: they call into the wrap/unwrap helpers
      generated by DEFINE_OWNED and DEFINE_BORROWED, providing a more
      explicit call-site convention for typed ownership wrappers.

      In default builds these macros are no-ops (they collapse to the
      raw pointer arguments), so code written using them works the
      same way regardless of mode. In strict builds, mismatches between
      wrapper types and raw pointers become compile errors.

      The flag does NOT change owned() / borrowed() macro expansion —
      those remain T in every mode. The strict-typed wrappers are
      opted into per-type via DEFINE_OWNED / DEFINE_BORROWED, not
      globally via the flag.

      Cost when off (the default):
        - Zero. The ceremony macros expand to their raw arguments.

      Cost when on:
        - One struct field per wrapper instance.
        - Compile-time-only checks; no runtime cost beyond DEFINE_OWNED's
          existing require_msg calls in unwrap helpers.

      Enable via CMake:
        cmake -DCANON_OWNERSHIP=strict -B build

      The flag is project-wide and must be consistent across all
      translation units that share Canon-C types — same constraint as
      NDEBUG.

   Quick reference:
   ─────────────────────────────────────────────────────────────────────
     Default build              — annotations as documentation; zero cost
     CANON_LIFETIME_DEBUG       — adds runtime lifetime tracking
     CANON_OWNERSHIP_STRICT     — adds compile-time wrapper type checks
                                  via OWN/TAKE/BORROW/GET ceremony
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
 *   owned_T_wrap(ptr)         -- wrap a raw pointer (takes ownership)
 *   owned_T_unwrap(o)         -- extract raw pointer; marks wrapper as consumed
 *   owned_T_borrow(o)         -- read raw pointer without consuming
 *   owned_T_is_valid(o)       -- returns true if wrapper holds a non-NULL pointer
 *   owned_T_drop(o, free_fn)  -- free the held pointer and NULL it
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
 *   borrowed_T_from(ptr)   -- wrap a raw const pointer (non-owning)
 *   borrowed_T_get(b)      -- read the raw pointer
 *   borrowed_T_is_valid(b) -- returns true if wrapper holds a non-NULL pointer
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
   OWN / TAKE / BORROW / GET — call-site ceremony for DEFINE_OWNED types
   ══════════════════════════════════════════════════════════════════════════
   These macros provide a concise call-site syntax for the wrap/unwrap
   helpers generated by DEFINE_OWNED and DEFINE_BORROWED. They work in
   every build mode but only become non-trivial under CANON_OWNERSHIP_STRICT.

   Typical use:
     DEFINE_OWNED(Arena)
     DEFINE_BORROWED(Arena)

     // Wrap a raw pointer at the construction site
     owned_Arena oa = OWN(Arena, arena_create(buf, size));

     // Borrow the pointer without consuming the wrapper
     Arena* raw = GET(Arena, BORROW(Arena, oa.ptr));

     // Consume the wrapper, marking it as taken
     Arena* taken = TAKE(Arena, &oa);

   The ceremony makes ownership transfer explicit at every call site.
   In default builds these macros expand to the raw arguments; in strict
   builds they invoke the type-checked wrap/unwrap helpers, so mismatches
   become compile errors.
   ══════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_OWNERSHIP_STRICT

/**
 * @def OWN(type, raw_ptr)
 * @brief Wraps a raw pointer in an owned_##type, taking ownership
 *
 * Strict-mode form. Equivalent to owned_##type##_wrap(raw_ptr).
 *
 * @param type    Type name previously passed to DEFINE_OWNED(type)
 * @param raw_ptr Raw type* pointer to wrap
 * @return owned_##type wrapper
 */
#define OWN(type, raw_ptr) owned_##type##_wrap(raw_ptr)

/**
 * @def TAKE(type, owned_ptr)
 * @brief Unwraps and consumes an owned_##type, returning the raw pointer
 *
 * Strict-mode form. Equivalent to owned_##type##_unwrap(owned_ptr).
 * Asserts the wrapper has not already been consumed.
 *
 * @param type      Type name previously passed to DEFINE_OWNED(type)
 * @param owned_ptr Pointer to an owned_##type wrapper (lvalue)
 * @return Raw type* pointer
 */
#define TAKE(type, owned_ptr) owned_##type##_unwrap(owned_ptr)

/**
 * @def BORROW(type, raw_ptr)
 * @brief Wraps a raw const pointer in a borrowed_##type
 *
 * Strict-mode form. Equivalent to borrowed_##type##_from(raw_ptr).
 *
 * @param type    Type name previously passed to DEFINE_BORROWED(type)
 * @param raw_ptr Raw const type* pointer to wrap
 * @return borrowed_##type wrapper
 */
#define BORROW(type, raw_ptr) borrowed_##type##_from(raw_ptr)

/**
 * @def GET(type, borrowed_ptr)
 * @brief Reads the raw pointer from a borrowed_##type wrapper
 *
 * Strict-mode form. Equivalent to borrowed_##type##_get(borrowed_ptr).
 *
 * @param type         Type name previously passed to DEFINE_BORROWED(type)
 * @param borrowed_ptr Pointer to a borrowed_##type wrapper
 * @return Raw const type* pointer
 */
#define GET(type, borrowed_ptr) borrowed_##type##_get(borrowed_ptr)

#else /* !CANON_OWNERSHIP_STRICT */

/**
 * @def OWN(type, raw_ptr)
 * @brief Default-mode no-op: expands to the raw pointer argument
 *
 * In default builds the ceremony collapses; raw pointers flow through
 * unchanged. Use DEFINE_OWNED + CANON_OWNERSHIP_STRICT for type-checked
 * mode where this macro invokes owned_##type##_wrap.
 */
#define OWN(type, raw_ptr) (raw_ptr)

/**
 * @def TAKE(type, owned_ptr)
 * @brief Default-mode no-op: expands to the argument
 */
#define TAKE(type, owned_ptr) (owned_ptr)

/**
 * @def BORROW(type, raw_ptr)
 * @brief Default-mode no-op: expands to the raw pointer argument
 */
#define BORROW(type, raw_ptr) (raw_ptr)

/**
 * @def GET(type, borrowed_ptr)
 * @brief Default-mode no-op: expands to the argument
 */
#define GET(type, borrowed_ptr) (borrowed_ptr)

#endif /* CANON_OWNERSHIP_STRICT */

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
