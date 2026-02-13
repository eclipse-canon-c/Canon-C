#ifndef CANON_CORE_OWNERSHIP_H
#define CANON_CORE_OWNERSHIP_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"

/**
 * @file core/ownership.h
 * @brief Explicit ownership and borrowing annotations for Canon-C APIs
 *
 * Canon-C's identity claim is that ownership is visible at every call site.
 * Without this header, that claim lives only in comments and naming conventions.
 * With it, every function signature becomes machine-readable and Doxygen-visible.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Annotations only — no enforcement, no runtime overhead
 * - C99 has no destructors or move semantics — we do not pretend otherwise
 * - owned(T)    → this pointer transfers ownership to the callee or caller
 * - borrowed(T) → this pointer is a non-owning view; caller retains ownership
 * - moved(T)    → value has been logically moved; do not use original after this
 * - dropped(T)  → value has been released; pointer is now invalid
 * - These are typedef aliases — the compiler sees them as identical to T*
 *
 * What this gives you:
 * ────────────────────────────────────────────────────────────────────────────
 * - API intent visible in declarations without reading documentation
 * - Doxygen and IDE tooling can surface ownership at a glance
 * - Grep-able: find all owned(T) parameters across the codebase instantly
 * - No false safety: annotations are promises, not guarantees
 *
 * What this does NOT give you:
 * ────────────────────────────────────────────────────────────────────────────
 * - No compiler enforcement of ownership rules
 * - No prevention of use-after-move or double-free
 * - No automatic cleanup / destructors
 * - These are documentation tools, not type system features
 *
 * Naming convention for functions:
 * ────────────────────────────────────────────────────────────────────────────
 * - Functions that take owned(T)    consume the value; caller must not use it
 * - Functions that return owned(T)  transfer ownership to caller; caller must free
 * - Functions that take borrowed(T) do not consume; caller retains responsibility
 * - Functions that return borrowed(T) return a view; caller must NOT free it
 * - _free() functions always take owned(T) — they consume and release
 * - _init() functions always return owned(T) — they transfer ownership to caller
 * - _as_slice(), _str(), _data() return borrowed(T) — views into owned memory
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * ownership.h lives in core/ and may only include from core/primitives/.
 * No data/, semantics/, util/, or other core/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Pure preprocessor — zero runtime impact
 * - Compatible with all C compilers and static analysis tools
 *
 * Quick start:
 * ```c
 * #include "core/ownership.h"
 *
 * // Arena owns its backing buffer after init
 * owned(Arena) arena_create(owned(u8*) buffer, usize size);
 *
 * // Vec borrows the buffer — caller retains ownership
 * canon_vec_int vec_int_init(borrowed(int*) buffer, usize cap);
 *
 * // Alloc returns an owned pointer — caller must free or arena_reset
 * owned(void*) arena_alloc(borrowed(Arena*) arena, usize size);
 *
 * // Free consumes ownership — do not use arena after this
 * void arena_destroy(owned(Arena*) arena);
 *
 * // View returns a borrowed pointer — do not free, do not outlive source
 * borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb);
 *
 * // Move signals intent — original should not be used after
 * void take_ownership(moved(MyStruct*) s);
 * ```
 *
 * Usage with DEFINE_OWNED / DEFINE_BORROWED:
 * ```c
 * // For strongly-typed owned/borrowed wrappers (struct-based, see below)
 * DEFINE_OWNED(Arena)
 * DEFINE_BORROWED(Arena)
 *
 * owned_Arena a = owned_Arena_wrap(arena_create(...));
 * borrowed_Arena b = owned_Arena_borrow(&a);
 * owned_Arena_drop(&a, arena_destroy);
 * ```
 *
 * @sa core/slice.h      — borrowed views into buffers (slice_t, bytes_t, str_t)
 * @sa semantics/borrow.h — borrowed_slice, borrowed_str built on top of this
 */

/* ════════════════════════════════════════════════════════════════════════════
   Lightweight annotation macros (typedef-based)
   ════════════════════════════════════════════════════════════════════════════
   These expand to T* exactly — no runtime cost, no type system change.
   Their value is documentation, grep-ability, and Doxygen visibility.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def owned(T)
 * @brief Annotates a pointer as transferring ownership
 *
 * When used on a parameter: the function takes ownership — caller must not use
 * the pointer afterward and must not free it themselves.
 *
 * When used on a return type: the caller receives ownership and is responsible
 * for eventually freeing or dropping the value.
 *
 * Equivalent to T* at the compiler level.
 *
 * Example:
 * ```c
 * owned(char*) duplicate_string(borrowed(const char*) src);
 * void         free_string(owned(char*) s);
 * ```
 */
#define owned(T) T

/**
 * @def borrowed(T)
 * @brief Annotates a pointer as non-owning (view only)
 *
 * When used on a parameter: the function does not take ownership — caller
 * retains responsibility for the lifetime and cleanup of the value.
 *
 * When used on a return type: the returned pointer is a view into memory
 * owned by something else — the caller must NOT free it and must not
 * use it after the owner is freed or reset.
 *
 * Equivalent to T* at the compiler level.
 *
 * Example:
 * ```c
 * borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb);
 * ```
 */
#define borrowed(T) T

/**
 * @def moved(T)
 * @brief Annotates a pointer as being logically moved
 *
 * Signals that after this call, the original value should be treated as
 * invalid. C cannot enforce this — it is a promise from the caller.
 *
 * Equivalent to T* at the compiler level.
 *
 * Example:
 * ```c
 * // After this call, do not use 'buf' — ownership has transferred
 * void take_buffer(moved(u8*) buf, usize len);
 * ```
 */
#define moved(T) T

/**
 * @def dropped(T)
 * @brief Annotates a pointer that has been released and is now invalid
 *
 * Used in _free() and _drop() return types or parameter annotations to signal
 * that the value is consumed and must not be dereferenced after the call.
 *
 * Equivalent to T* at the compiler level.
 *
 * Example:
 * ```c
 * void dynvec_int_free(dropped(dynvec_int*) v);
 * ```
 */
#define dropped(T) T

/* ════════════════════════════════════════════════════════════════════════════
   Drop helper — explicit resource release convention
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def CANON_DROP(ptr, free_fn)
 * @brief Explicitly drops an owned pointer and NULLs it
 *
 * Calls free_fn(ptr), then sets ptr to NULL to prevent use-after-free.
 * The NULL assignment does not prevent UB if the caller dereferences it,
 * but makes the intent visible and helps debuggers.
 *
 * @param ptr     Owned pointer to release (must be an lvalue)
 * @param free_fn Cleanup function compatible with void fn(T*)
 *
 * Example:
 * ```c
 * owned(DynString*) s = /* ... *\/;
 * CANON_DROP(s, dynstring_free);
 * // s is now NULL — use is a visible mistake
 * ```
 */
#define CANON_DROP(ptr, free_fn) \
    do { \
        free_fn(ptr); \
        (ptr) = NULL; \
    } while (0)

/**
 * @def CANON_DROP_IF(ptr, free_fn)
 * @brief Drops an owned pointer only if it is non-NULL, then NULLs it
 *
 * Safe variant of CANON_DROP for cases where ptr may legitimately be NULL
 * (e.g., lazily-initialized resources).
 *
 * @param ptr     Owned pointer to release (must be an lvalue)
 * @param free_fn Cleanup function compatible with void fn(T*)
 *
 * Example:
 * ```c
 * owned(char*) buf = NULL;
 * // ... conditionally assigned ...
 * CANON_DROP_IF(buf, free);  // safe whether or not buf was set
 * ```
 */
#define CANON_DROP_IF(ptr, free_fn) \
    do { \
        if ((ptr) != NULL) { \
            free_fn(ptr); \
            (ptr) = NULL; \
        } \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_OWNED / DEFINE_BORROWED — struct-based strong wrappers (optional)
   ════════════════════════════════════════════════════════════════════════════
   Use the annotation macros above for most APIs.
   Use DEFINE_OWNED / DEFINE_BORROWED when you want a distinct type that
   cannot be accidentally passed where a raw pointer is expected, giving
   the compiler a chance to catch ownership mistakes at the type level.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a strongly-typed owned wrapper for a pointer-to-type
 *
 * Creates a struct `owned_##type` that wraps `type*` and cannot be implicitly
 * converted to a raw pointer. The caller must explicitly unwrap or drop it.
 *
 * Generated type:
 * - owned_##type  — struct wrapping type* with ownership semantics
 *
 * Generated functions:
 * - owned_##type##_wrap(ptr)              → owned_##type (take ownership)
 * - owned_##type##_unwrap(o)              → type* (release raw pointer)
 * - owned_##type##_borrow(o)              → type* (borrow without releasing)
 * - owned_##type##_is_valid(o)            → bool (ptr != NULL)
 * - owned_##type##_drop(o, free_fn)       → void (call free_fn, set NULL)
 *
 * @param type The pointed-to type (not the pointer type — e.g. Arena, not Arena*)
 *
 * Example:
 * ```c
 * DEFINE_OWNED(Arena)
 *
 * owned_Arena a = owned_Arena_wrap(arena_create(buf, size));
 * Arena* raw = owned_Arena_borrow(&a);   // use without releasing
 * owned_Arena_drop(&a, arena_destroy);   // release and NULL
 * ```
 */
#define DEFINE_OWNED(type) \
\
/** @brief Strongly-typed owned pointer wrapper for type */ \
typedef struct { \
    type* ptr; /**< Owned pointer — must be dropped before going out of scope */ \
} owned_##type; \
\
/** \
 * @brief Wraps a raw pointer as an owned_##type, taking ownership \
 * \
 * @param ptr Pointer to take ownership of (may be NULL) \
 * @return owned_##type holding ptr \
 * \
 * Performance: O(1) \
 */ \
static inline owned_##type owned_##type##_wrap(type* ptr) { \
    return (owned_##type){ .ptr = ptr }; \
} \
\
/** \
 * @brief Releases the raw pointer from the owned wrapper (transfers ownership out) \
 * \
 * @param o Pointer to owned_##type to unwrap \
 * @return Raw type* — caller takes ownership and must eventually free it \
 * \
 * @post o->ptr == NULL (wrapper is emptied) \
 * \
 * Performance: O(1) \
 */ \
static inline type* owned_##type##_unwrap(owned_##type* o) { \
    require_msg(o != NULL, "owned_" #type "_unwrap: o cannot be NULL"); \
    type* raw = o->ptr; \
    o->ptr = NULL; \
    return raw; \
} \
\
/** \
 * @brief Borrows the raw pointer without releasing ownership \
 * \
 * @param o Pointer to owned_##type to borrow from \
 * @return Raw type* — caller does NOT take ownership, must not free it \
 * \
 * @post o->ptr is unchanged \
 * \
 * Performance: O(1) \
 */ \
static inline type* owned_##type##_borrow(const owned_##type* o) { \
    require_msg(o != NULL, "owned_" #type "_borrow: o cannot be NULL"); \
    return o->ptr; \
} \
\
/** \
 * @brief Returns true if the owned pointer is non-NULL \
 * \
 * Performance: O(1) \
 */ \
static inline bool owned_##type##_is_valid(const owned_##type* o) { \
    return o != NULL && o->ptr != NULL; \
} \
\
/** \
 * @brief Drops the owned pointer by calling free_fn, then NULLs it \
 * \
 * @param o       Pointer to owned_##type to drop \
 * @param free_fn Cleanup function: void fn(type*) \
 * \
 * @post o->ptr == NULL \
 * \
 * Performance: O(1) plus cost of free_fn \
 */ \
static inline void owned_##type##_drop( \
    owned_##type* o, void (*free_fn)(type*)) { \
    require_msg(o != NULL,       "owned_" #type "_drop: o cannot be NULL"); \
    require_msg(free_fn != NULL, "owned_" #type "_drop: free_fn cannot be NULL"); \
    if (o->ptr) { \
        free_fn(o->ptr); \
        o->ptr = NULL; \
    } \
}

/**
 * @brief Generates a strongly-typed borrowed wrapper for a const pointer-to-type
 *
 * Creates a struct `borrowed_##type` that wraps `const type*` and makes it
 * clear at the type level that this pointer is non-owning.
 *
 * Generated type:
 * - borrowed_##type  — struct wrapping const type* with borrowing semantics
 *
 * Generated functions:
 * - borrowed_##type##_from(ptr)        → borrowed_##type
 * - borrowed_##type##_get(b)           → const type* (access the pointer)
 * - borrowed_##type##_is_valid(b)      → bool (ptr != NULL)
 *
 * @param type The pointed-to type (not the pointer type)
 *
 * Example:
 * ```c
 * DEFINE_OWNED(Arena)
 * DEFINE_BORROWED(Arena)
 *
 * owned_Arena a = owned_Arena_wrap(arena_create(buf, size));
 * borrowed_Arena view = borrowed_Arena_from(owned_Arena_borrow(&a));
 * const Arena* raw = borrowed_Arena_get(&view);
 * owned_Arena_drop(&a, arena_destroy);
 * // view is now invalid — it was borrowing from a
 * ```
 */
#define DEFINE_BORROWED(type) \
\
/** @brief Strongly-typed borrowed (non-owning) pointer wrapper for type */ \
typedef struct { \
    const type* ptr; /**< Borrowed pointer — must NOT be freed by holder */ \
} borrowed_##type; \
\
/** \
 * @brief Creates a borrowed_##type from a raw const pointer \
 * \
 * @param ptr Pointer to borrow (may be NULL) \
 * @return borrowed_##type holding ptr \
 * \
 * Performance: O(1) \
 */ \
static inline borrowed_##type borrowed_##type##_from(const type* ptr) { \
    return (borrowed_##type){ .ptr = ptr }; \
} \
\
/** \
 * @brief Returns the raw borrowed pointer (read-only access) \
 * \
 * @param b Pointer to borrowed_##type \
 * @return const type* — do NOT free this pointer \
 * \
 * Performance: O(1) \
 */ \
static inline const type* borrowed_##type##_get(const borrowed_##type* b) { \
    require_msg(b != NULL, "borrowed_" #type "_get: b cannot be NULL"); \
    return b->ptr; \
} \
\
/** \
 * @brief Returns true if the borrowed pointer is non-NULL \
 * \
 * Performance: O(1) \
 */ \
static inline bool borrowed_##type##_is_valid(const borrowed_##type* b) { \
    return b != NULL && b->ptr != NULL; \
}

/* ════════════════════════════════════════════════════════════════════════════
   Ownership convention summary — for Doxygen and code review
   ════════════════════════════════════════════════════════════════════════════

   CALLER OWNS, FUNCTION BORROWS (most common):
     void vec_push(borrowed(canon_vec_int*) v, int item);
     ↑ vec_push does not take ownership of v — caller still responsible

   FUNCTION RETURNS OWNED (caller must free):
     owned(char*) str_duplicate(borrowed(const char*) src);
     ↑ caller must call free() or equivalent on the returned pointer

   FUNCTION TAKES OWNED (callee responsible after call):
     void dynvec_int_free(dropped(dynvec_int*) v);
     ↑ do not use v after this call

   FUNCTION RETURNS BORROWED (caller must NOT free):
     borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb);
     ↑ returned pointer is valid only as long as sb is alive

   ════════════════════════════════════════════════════════════════════════════ */

#endif /* CANON_CORE_OWNERSHIP_H */
