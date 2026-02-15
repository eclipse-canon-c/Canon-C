#ifndef CANON_SEMANTICS_BORROW_H
#define CANON_SEMANTICS_BORROW_H

#include "core/primitives/types.h"      // usize, bool
#include "core/primitives/contract.h"   // require_msg
#include "core/slice.h"                 // str_t, cbytes_t, slice_##type
#include "core/ownership.h"             // borrowed(T) annotation style

/**
 * @file semantics/borrow.h
 * @brief Semantic borrowing types — non-owning views with explicit lifetime intent
 *
 * Completes the ownership vocabulary from core/ownership.h.
 * ownership.h gives you annotation macros (borrowed(T)).
 * borrow.h gives you concrete, reusable types for common borrow patterns.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - All types are non-owning views — never free the .ptr / .str / .bytes
 * - Explicit source tag (const void*) for debugging and assertions
 * - Lifetime rule: borrowed value is valid only as long as source is alive
 * - No runtime enforcement — documentation + discipline
 *
 * Types provided:
 * - borrowed_ptr<T>   — generic non-owning pointer to any T
 * - borrowed_str      — non-owning string view (wraps str_t)
 * - borrowed_bytes    — non-owning byte view (wraps cbytes_t)
 * - borrowed_slice<T> — non-owning typed slice (wraps slice_##type)
 *
 * Why separate from slice.h?
 * ────────────────────────────────────────────────────────────────────────────
 * slice_t / str_t / bytes_t are pure {ptr, len} data structures.
 * They carry no ownership intent — a returned slice might be owned or borrowed.
 *
 * borrowed_* types make non-ownership explicit at the type level.
 * When you see borrowed_str in a signature, you immediately know:
 *   - Do NOT free it
 *   - Do NOT outlive its source
 *
 * Lifetime model (documentation convention):
 * ────────────────────────────────────────────────────────────────────────────
 * borrowed_str s = stringbuf_borrow_str(&sb);
 * // s is valid as long as sb is alive and unmodified
 * // s becomes invalid after: stringbuf_clear(&sb), arena_reset(sb.arena), free(sb)
 *
 * The source field is a debug tag — pass &owning_struct as source.
 * It is never dereferenced by borrow.h — only for human/debugger inspection.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * borrow.h lives in semantics/ and may include core/ only.
 * No data/, util/, or other semantics/ headers.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No allocations, no platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All types ≈ sizeof(ptr) + sizeof(usize) + sizeof(void*)
 * - All functions static inline — zero call overhead
 *
 * Quick start:
 * ```c
 * borrowed_str name = borrowed_str_from_cstr("Alice", NULL);
 * printf("%.*s\n", (int)name.str.len, name.str.ptr);
 *
 * u8 buf[256];
 * borrowed_bytes region = borrowed_bytes_from(buf, 256, buf);
 *
 * MyStruct obj = {0};
 * borrowed_ptr ref = borrowed_ptr_from(&obj, &obj);
 * MyStruct* p = (MyStruct*)borrowed_ptr_get(&ref);
 *
 * // Typed slice borrow
 * DEFINE_SLICE(int)
 * DEFINE_BORROWED_SLICE(int)
 * int arr[4] = {1, 2, 3, 4};
 * borrowed_slice_int view = borrowed_slice_int_from(slice_int_from(arr, 4), arr);
 * ```
 *
 * @sa core/slice.h — underlying str_t, cbytes_t, slice_##type
 * @sa core/ownership.h — borrowed(T) annotation macros
 */
/* ════════════════════════════════════════════════════════════════════════════
   borrowed_ptr — generic non-owning pointer
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Generic non-owning pointer with optional source tag
 *
 * Use when type is unknown or erased.
 * Prefer typed borrowed wrappers when type is known.
 *
 * Invariants:
 * - ptr may be NULL (absent borrow)
 * - source is debug-only tag — never dereferenced
 * - ptr must outlive this struct
 */
typedef struct {
    const void* ptr;    ///< Borrowed pointer (do not free)
    const void* source; ///< Owning object's address (debug tag)
} borrowed_ptr;

/** @brief Creates borrowed_ptr from raw pointer and owner */
static inline borrowed_ptr borrowed_ptr_from(const void* ptr, const void* source) {
    return (borrowed_ptr){ .ptr = ptr, .source = source };
}

/** @brief Empty (NULL) borrowed_ptr — represents absent borrow */
static inline borrowed_ptr borrowed_ptr_null(void) {
    return (borrowed_ptr){ .ptr = NULL, .source = NULL };
}

/** @brief Returns raw pointer (cast to correct type, do not free) */
static inline const void* borrowed_ptr_get(const borrowed_ptr* b) {
    require_msg(b != NULL, "borrowed_ptr_get: b cannot be NULL");
    return b->ptr;
}

/** @brief True if ptr != NULL */
static inline bool borrowed_ptr_is_valid(const borrowed_ptr* b) {
    return b != NULL && b->ptr != NULL;
}

/** @brief True if two borrowed_ptrs point to same address */
static inline bool borrowed_ptr_eq(borrowed_ptr a, borrowed_ptr b) {
    return a.ptr == b.ptr;
}

/* ════════════════════════════════════════════════════════════════════════════
   borrowed_str — non-owning string view
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Non-owning string view with explicit borrowing intent
 *
 * Wraps str_t — characters not null-terminated.
 * Do NOT free str.ptr — owned by source.
 */
typedef struct {
    str_t       str;    ///< Borrowed view (do not free str.ptr)
    const void* source; ///< Owning object's address (debug tag)
} borrowed_str;

/** @brief Creates from str_t and owner */
static inline borrowed_str borrowed_str_from(str_t s, const void* source) {
    return (borrowed_str){ .str = s, .source = source };
}

/** @brief Creates from null-terminated C string */
static inline borrowed_str borrowed_str_from_cstr(const char* cstr, const void* source) {
    return (borrowed_str){ .str = str_from_cstr(cstr), .source = source };
}

/** @brief Empty borrowed_str */
static inline borrowed_str borrowed_str_empty(void) {
    return (borrowed_str){ .str = str_empty(), .source = NULL };
}

/** @brief Returns underlying str_t (do not free ptr) */
static inline str_t borrowed_str_get(const borrowed_str* b) {
    require_msg(b != NULL, "borrowed_str_get: b cannot be NULL");
    return b->str;
}

/** @brief True if str.ptr != NULL */
static inline bool borrowed_str_is_valid(const borrowed_str* b) {
    return b != NULL && b->str.ptr != NULL;
}

/** @brief Length of borrowed string */
static inline usize borrowed_str_len(const borrowed_str* b) {
    return b ? b->str.len : 0;
}

/** @brief True if content is equal (ignores source) */
static inline bool borrowed_str_eq(borrowed_str a, borrowed_str b) {
    return str_equal(a.str, b.str);
}

/** @brief Sub-borrow [start, end) — inherits source */
static inline borrowed_str borrowed_str_slice(borrowed_str b, usize start, usize end) {
    return (borrowed_str){ .str = str_slice(b.str, start, end), .source = b.source };
}

/* ════════════════════════════════════════════════════════════════════════════
   borrowed_bytes — non-owning byte view
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Non-owning byte region view with explicit borrowing intent
 *
 * Wraps cbytes_t — read-only bytes.
 * Do NOT free bytes.ptr — owned by source.
 */
typedef struct {
    cbytes_t    bytes;  ///< Borrowed bytes (do not free bytes.ptr)
    const void* source; ///< Owning object's address (debug tag)
} borrowed_bytes;

/** @brief Creates from raw pointer/len and owner */
static inline borrowed_bytes borrowed_bytes_from(const void* ptr, usize len,
                                                  const void* source) {
    return (borrowed_bytes){ .bytes = cbytes_from(ptr, len), .source = source };
}

/** @brief Creates from existing cbytes_t and owner */
static inline borrowed_bytes borrowed_bytes_from_cbytes(cbytes_t b, const void* source) {
    return (borrowed_bytes){ .bytes = b, .source = source };
}

/** @brief Empty borrowed_bytes */
static inline borrowed_bytes borrowed_bytes_empty(void) {
    return (borrowed_bytes){ .bytes = { .ptr = NULL, .len = 0 }, .source = NULL };
}

/** @brief Returns underlying cbytes_t (do not free ptr) */
static inline cbytes_t borrowed_bytes_get(const borrowed_bytes* b) {
    require_msg(b != NULL, "borrowed_bytes_get: b cannot be NULL");
    return b->bytes;
}

/** @brief Byte count */
static inline usize borrowed_bytes_len(const borrowed_bytes* b) {
    return b ? b->bytes.len : 0;
}

/** @brief True if bytes.ptr != NULL */
static inline bool borrowed_bytes_is_valid(const borrowed_bytes* b) {
    return b != NULL && b->bytes.ptr != NULL;
}

/** @brief Sub-borrow [start, end) — inherits source */
static inline borrowed_bytes borrowed_bytes_slice(borrowed_bytes b, usize start, usize end) {
    cbytes_t full = b.bytes;
    if (!full.ptr || start >= full.len) return borrowed_bytes_empty();
    if (end > full.len) end = full.len;
    if (start >= end) return borrowed_bytes_empty();
    return (borrowed_bytes){
        .bytes = { .ptr = full.ptr + start, .len = end - start },
        .source = b.source
    };
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_BORROWED_SLICE — typed borrowed slice
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Generates typed borrowed_slice wrapper for element type `type`
 *
 * Requires prior DEFINE_SLICE(type).
 *
 * Generated type: borrowed_slice_##type
 * Generated helpers: from, empty, get, len, is_valid, at, slice, as_bytes
 *
 * Example usage:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_BORROWED_SLICE(int)
 *
 * int arr[4] = {1,2,3,4};
 * borrowed_slice_int view = borrowed_slice_int_from(slice_int_from(arr,4), arr);
 * ```
 */
#define DEFINE_BORROWED_SLICE(type) \
\
/** \
 * @brief Non-owning typed slice borrow for type \
 * \
 * Wraps slice_##type with borrowing intent. \
 * Do NOT free slice.ptr — owned by source. \
 */ \
typedef struct { \
    slice_##type slice; /**< Borrowed view (do not free slice.ptr) */ \
    const void*  source; /**< Owning object's address (debug tag) */ \
} borrowed_slice_##type; \
\
/** @brief Creates from slice_##type and owner */ \
static inline borrowed_slice_##type borrowed_slice_##type##_from( \
    slice_##type s, const void* source) { \
    return (borrowed_slice_##type){ .slice = s, .source = source }; \
} \
\
/** @brief Empty borrowed_slice_##type */ \
static inline borrowed_slice_##type borrowed_slice_##type##_empty(void) { \
    return (borrowed_slice_##type){ .slice = slice_##type##_empty(), .source = NULL }; \
} \
\
/** @brief Underlying slice_##type (do not free ptr) */ \
static inline slice_##type borrowed_slice_##type##_get( \
    const borrowed_slice_##type* b) { \
    require_msg(b != NULL, "borrowed_slice_" #type "_get: b cannot be NULL"); \
    return b->slice; \
} \
\
/** @brief Element count */ \
static inline usize borrowed_slice_##type##_len( \
    const borrowed_slice_##type* b) { \
    return b ? b->slice.len : 0; \
} \
\
/** @brief True if slice.ptr != NULL */ \
static inline bool borrowed_slice_##type##_is_valid( \
    const borrowed_slice_##type* b) { \
    return b != NULL && b->slice.ptr != NULL; \
} \
\
/** @brief Const pointer to element i (bounds-checked, NULL if OOB) */ \
static inline const type* borrowed_slice_##type##_at( \
    const borrowed_slice_##type* b, usize i) { \
    if (!b || !b->slice.ptr || i >= b->slice.len) return NULL; \
    return &b->slice.ptr[i]; \
} \
\
/** @brief Sub-borrow [start, end) — inherits source */ \
static inline borrowed_slice_##type borrowed_slice_##type##_slice( \
    borrowed_slice_##type b, usize start, usize end) { \
    return (borrowed_slice_##type){ \
        .slice = slice_##type##_slice(b.slice, start, end), \
        .source = b.source \
    }; \
} \
\
/** @brief Raw borrowed_bytes view over slice memory — inherits source */ \
static inline borrowed_bytes borrowed_slice_##type##_as_bytes( \
    const borrowed_slice_##type* b) { \
    if (!b || !b->slice.ptr) return borrowed_bytes_empty(); \
    return (borrowed_bytes){ \
        .bytes = cbytes_from((const u8*)b->slice.ptr, b->slice.len * sizeof(type)), \
        .source = b.source \
    }; \
}

#endif /* CANON_SEMANTICS_BORROW_H */
