#ifndef CANON_SEMANTICS_BORROW_H
#define CANON_SEMANTICS_BORROW_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/slice.h"
#include "core/ownership.h"

/**
 * @file semantics/borrow.h
 * @brief Semantic borrowing types — non-owning views with explicit lifetime intent
 *
 * This header completes the ownership story that core/ownership.h begins.
 * ownership.h gives you the annotation vocabulary (owned, borrowed, moved).
 * borrow.h gives you the concrete types that express non-ownership at the
 * type level for the most common borrowing patterns in Canon-C.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - borrowed_ptr    — a non-owning pointer to any T, with explicit lifetime note
 * - borrowed_str    — a non-owning view into a character sequence (wraps str_t)
 * - borrowed_bytes  — a non-owning view into a byte region (wraps cbytes_t)
 * - borrowed_slice  — a non-owning typed element view (wraps slice_##type)
 *
 * Relationship to other modules:
 * ────────────────────────────────────────────────────────────────────────────
 * - core/slice.h      — provides str_t, cbytes_t, slice_##type as the
 *                       underlying data; borrow.h wraps them with intent
 * - core/ownership.h  — provides the owned/borrowed annotation vocabulary;
 *                       borrow.h builds concrete types on top
 * - semantics/option  — borrowed_ptr can express nullable borrows via
 *                       None when the source does not exist
 *
 * Why not just use slice_t directly?
 * ────────────────────────────────────────────────────────────────────────────
 * slice_t (and str_t, bytes_t) are pure data structures — {ptr, len} pairs.
 * They carry no semantic claim about ownership. A function returning a str_t
 * might have allocated it (owned) or be returning a view (borrowed) — you
 * cannot tell from the type alone.
 *
 * borrowed_str, borrowed_bytes, borrowed_slice make the non-ownership claim
 * explicit at the type level. When you see borrowed_str in a return type,
 * you know immediately: do not free this, do not outlive its source.
 *
 * Lifetime model:
 * ────────────────────────────────────────────────────────────────────────────
 * C has no lifetime parameters. Canon-C uses a documentation convention:
 *
 *   borrowed_str s = stringbuf_borrow_str(&sb);
 *   // s is valid as long as sb is alive and unmodified
 *   // s becomes invalid after: stringbuf_clear(&sb), arena_reset(), free()
 *
 * The `source` field (where present) is a void* tag identifying the owner.
 * It is never dereferenced — it exists for debugging and assertion only.
 * Pass the address of the owning struct as source when constructing a borrow.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * borrow.h lives in semantics/ and may include core/ only.
 * No data/, util/, or other semantics/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - No allocations anywhere in this file
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All types are structs of {ptr, len} or {ptr} — sizeof is minimal
 * - All functions are static inline — zero call overhead
 * - No allocations, no copies
 *
 * Quick start:
 * ```c
 * #include "semantics/borrow.h"
 *
 * // Borrow a string view from a StringBuf
 * borrowed_str name = borrowed_str_from_cstr("Alice");
 * printf("%.*s\n", (int)name.str.len, name.str.ptr);
 *
 * // Borrow a byte region from a buffer
 * u8 buf[256];
 * borrowed_bytes region = borrowed_bytes_from(buf, 256, buf);
 *
 * // Borrow a typed pointer (non-owning reference)
 * MyStruct obj = {0};
 * borrowed_ptr ref = borrowed_ptr_from(&obj, &obj);
 * MyStruct* p = (MyStruct*)borrowed_ptr_get(&ref);
 *
 * // Typed slice borrow
 * DEFINE_SLICE(int)
 * DEFINE_BORROWED_SLICE(int)
 * int arr[4] = {1, 2, 3, 4};
 * borrowed_slice_int view = borrowed_slice_int_from(
 *     slice_int_from(arr, 4), arr);
 * ```
 *
 * @sa core/slice.h     — underlying str_t, bytes_t, slice_##type types
 * @sa core/ownership.h — owned/borrowed annotation macros
 */

/* ════════════════════════════════════════════════════════════════════════════
   borrowed_ptr — non-owning pointer to any T
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Non-owning pointer to any value, with optional source tag
 *
 * The weakest borrowing primitive — just a void* with explicit non-ownership
 * intent. Use typed borrowed wrappers (borrowed_str, borrowed_slice) when
 * the type is known. Use borrowed_ptr for generic or type-erased APIs.
 *
 * Invariants:
 * - ptr may be NULL (representing an absent borrow)
 * - source is informational only — never dereferenced by borrow.h
 * - Caller must ensure ptr remains valid for the lifetime of this struct
 *
 * Fields:
 * - ptr:    The borrowed pointer (non-owning, do not free)
 * - source: The owning object's address (for debugging and assertions)
 */
typedef struct {
    const void* ptr;    ///< Borrowed pointer (non-owning, do not free)
    const void* source; ///< Address of the owning object (debug tag, never dereferenced)
} borrowed_ptr;

/**
 * @brief Creates a borrowed_ptr from a raw pointer and its owner
 *
 * @param ptr    Pointer to borrow (may be NULL)
 * @param source Address of the owning struct (may be NULL if unknown)
 * @return borrowed_ptr wrapping ptr
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_ptr borrowed_ptr_from(const void* ptr, const void* source) {
    return (borrowed_ptr){ .ptr = ptr, .source = source };
}

/**
 * @brief Returns an empty (NULL) borrowed_ptr
 *
 * Represents the absence of a borrow — analogous to None in Option.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_ptr borrowed_ptr_null(void) {
    return (borrowed_ptr){ .ptr = NULL, .source = NULL };
}

/**
 * @brief Returns the raw borrowed pointer (do not free)
 *
 * @param b Pointer to borrowed_ptr (must not be NULL)
 * @return const void* — caller must cast to appropriate type
 *
 * @pre b != NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline const void* borrowed_ptr_get(const borrowed_ptr* b) {
    require_msg(b != NULL, "borrowed_ptr_get: b cannot be NULL");
    return b->ptr;
}

/**
 * @brief Returns true if the borrowed pointer is non-NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool borrowed_ptr_is_valid(const borrowed_ptr* b) {
    return b != NULL && b->ptr != NULL;
}

/**
 * @brief Returns true if two borrowed_ptrs point to the same address
 *
 * Does not compare source tags — only the borrowed pointer addresses.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool borrowed_ptr_eq(borrowed_ptr a, borrowed_ptr b) {
    return a.ptr == b.ptr;
}

/* ════════════════════════════════════════════════════════════════════════════
   borrowed_str — non-owning string view
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Non-owning view into a character sequence, with explicit source tag
 *
 * Wraps str_t (from core/slice.h) with semantic borrowing intent.
 * The character data is not null-terminated — length is always explicit.
 * Do not free str.ptr — it is owned by source.
 *
 * Invariants:
 * - str.ptr != NULL || str.len == 0
 * - Characters [str.ptr, str.ptr + str.len) are readable
 * - Caller must ensure str.ptr remains valid for lifetime of this struct
 * - source is a debug tag — never dereferenced by borrow.h
 */
typedef struct {
    str_t       str;    ///< Borrowed string view (do not free str.ptr)
    const void* source; ///< Address of the owning object (debug tag)
} borrowed_str;

/**
 * @brief Creates a borrowed_str from a str_t and its owning object
 *
 * @param s      The string view to borrow
 * @param source Address of the owner (for debugging; may be NULL)
 * @return borrowed_str wrapping s
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_str borrowed_str_from(str_t s, const void* source) {
    return (borrowed_str){ .str = s, .source = source };
}

/**
 * @brief Creates a borrowed_str from a null-terminated C string
 *
 * @param cstr  Null-terminated string (NULL returns empty borrowed_str)
 * @param source Address of the owner (may be NULL)
 * @return borrowed_str wrapping cstr (length computed via strlen)
 *
 * @note O(n) due to strlen — use borrowed_str_from() when length is known.
 *
 * Performance:
 * - Time:  O(strlen(cstr))
 * - Space: O(1)
 */
static inline borrowed_str borrowed_str_from_cstr(const char* cstr, const void* source) {
    return (borrowed_str){ .str = str_from_cstr(cstr), .source = source };
}

/**
 * @brief Returns an empty borrowed_str
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_str borrowed_str_empty(void) {
    return (borrowed_str){ .str = str_empty(), .source = NULL };
}

/**
 * @brief Returns the underlying str_t (do not free the pointer inside)
 *
 * @param b Pointer to borrowed_str (must not be NULL)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline str_t borrowed_str_get(const borrowed_str* b) {
    require_msg(b != NULL, "borrowed_str_get: b cannot be NULL");
    return b->str;
}

/**
 * @brief Returns true if the borrowed string is non-empty
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool borrowed_str_is_valid(const borrowed_str* b) {
    return b != NULL && b->str.ptr != NULL;
}

/**
 * @brief Returns the character count of the borrowed string
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize borrowed_str_len(const borrowed_str* b) {
    return b ? b->str.len : 0;
}

/**
 * @brief Returns true if two borrowed_strs have identical content
 *
 * Compares character data — does not compare source tags.
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 */
static inline bool borrowed_str_eq(borrowed_str a, borrowed_str b) {
    return str_equal(a.str, b.str);
}

/**
 * @brief Returns a sub-borrow of [start, end) — no allocation
 *
 * Source tag is inherited from the original borrowed_str.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_str borrowed_str_slice(borrowed_str b, usize start, usize end) {
    return (borrowed_str){ .str = str_slice(b.str, start, end), .source = b.source };
}

/* ════════════════════════════════════════════════════════════════════════════
   borrowed_bytes — non-owning byte region view
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Non-owning view into a byte region, with explicit source tag
 *
 * Wraps cbytes_t (from core/slice.h) with semantic borrowing intent.
 * The read-only byte region is not owned — do not free bytes.ptr.
 *
 * Invariants:
 * - bytes.ptr != NULL || bytes.len == 0
 * - Bytes [bytes.ptr, bytes.ptr + bytes.len) are readable
 * - source is a debug tag — never dereferenced by borrow.h
 */
typedef struct {
    cbytes_t    bytes;  ///< Borrowed byte view (do not free bytes.ptr)
    const void* source; ///< Address of the owning object (debug tag)
} borrowed_bytes;

/**
 * @brief Creates a borrowed_bytes from a raw pointer, length, and owner
 *
 * @param ptr    Pointer to byte data (NULL only if len == 0)
 * @param len    Number of bytes
 * @param source Address of the owner (may be NULL)
 * @return borrowed_bytes wrapping [ptr, ptr+len)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_bytes borrowed_bytes_from(const void* ptr, usize len,
                                                  const void* source) {
    return (borrowed_bytes){
        .bytes  = cbytes_from(ptr, len),
        .source = source
    };
}

/**
 * @brief Creates a borrowed_bytes from an existing cbytes_t and owner
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_bytes borrowed_bytes_from_cbytes(cbytes_t b, const void* source) {
    return (borrowed_bytes){ .bytes = b, .source = source };
}

/**
 * @brief Returns an empty borrowed_bytes
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_bytes borrowed_bytes_empty(void) {
    return (borrowed_bytes){ .bytes = { .ptr = NULL, .len = 0 }, .source = NULL };
}

/**
 * @brief Returns the underlying cbytes_t (do not free the pointer inside)
 *
 * @param b Pointer to borrowed_bytes (must not be NULL)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline cbytes_t borrowed_bytes_get(const borrowed_bytes* b) {
    require_msg(b != NULL, "borrowed_bytes_get: b cannot be NULL");
    return b->bytes;
}

/**
 * @brief Returns the byte count of the borrowed region
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize borrowed_bytes_len(const borrowed_bytes* b) {
    return b ? b->bytes.len : 0;
}

/**
 * @brief Returns true if the borrowed byte region is non-NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool borrowed_bytes_is_valid(const borrowed_bytes* b) {
    return b != NULL && b->bytes.ptr != NULL;
}

/**
 * @brief Returns a sub-borrow of [start, end) — no allocation
 *
 * Source tag is inherited from the original borrowed_bytes.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline borrowed_bytes borrowed_bytes_slice(borrowed_bytes b, usize start, usize end) {
    cbytes_t full = b.bytes;
    if (!full.ptr || start >= full.len) return borrowed_bytes_empty();
    if (end > full.len) end = full.len;
    if (start >= end) return borrowed_bytes_empty();
    return (borrowed_bytes){
        .bytes  = { .ptr = full.ptr + start, .len = end - start },
        .source = b.source
    };
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_BORROWED_SLICE — typed non-owning slice borrow
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a typed borrowed slice wrapper for element type `type`
 *
 * Requires DEFINE_SLICE(type) to have been called first.
 *
 * Generated type:
 * - borrowed_slice_##type  — non-owning view with source tag
 *
 * Generated functions:
 * - borrowed_slice_##type##_from(slice, source)  → borrowed_slice_##type
 * - borrowed_slice_##type##_empty()              → borrowed_slice_##type
 * - borrowed_slice_##type##_get(b)               → slice_##type
 * - borrowed_slice_##type##_len(b)               → usize
 * - borrowed_slice_##type##_is_valid(b)          → bool
 * - borrowed_slice_##type##_at(b, i)             → const type* (bounds-checked)
 * - borrowed_slice_##type##_slice(b, start, end) → borrowed_slice_##type
 * - borrowed_slice_##type##_as_bytes(b)          → borrowed_bytes
 *
 * @param type Element type — DEFINE_SLICE(type) must be called first
 *
 * Example:
 * ```c
 * DEFINE_SLICE(int)
 * DEFINE_BORROWED_SLICE(int)
 *
 * int arr[4] = {1, 2, 3, 4};
 * borrowed_slice_int view = borrowed_slice_int_from(
 *     slice_int_from(arr, 4), arr);
 *
 * const int* p = borrowed_slice_int_at(&view, 2);  // → &arr[2]
 * borrowed_slice_int sub = borrowed_slice_int_slice(view, 1, 3);
 * ```
 */
#define DEFINE_BORROWED_SLICE(type) \
\
/** \
 * @brief Non-owning typed slice view for type, with explicit source tag \
 * \
 * Wraps slice_##type with borrowing semantics. \
 * Do not free slice.ptr — it is owned by source. \
 */ \
typedef struct { \
    slice_##type slice;  /**< Borrowed typed view (do not free slice.ptr) */ \
    const void*  source; /**< Address of the owning object (debug tag) */ \
} borrowed_slice_##type; \
\
/** \
 * @brief Creates a borrowed_slice_##type from a slice_##type and its owner \
 * \
 * @pre DEFINE_SLICE(type) has been called \
 * \
 * Performance: O(1) \
 */ \
static inline borrowed_slice_##type borrowed_slice_##type##_from( \
    slice_##type s, const void* source) { \
    return (borrowed_slice_##type){ .slice = s, .source = source }; \
} \
\
/** @brief Returns an empty borrowed_slice_##type. O(1) */ \
static inline borrowed_slice_##type borrowed_slice_##type##_empty(void) { \
    return (borrowed_slice_##type){ \
        .slice  = slice_##type##_empty(), \
        .source = NULL \
    }; \
} \
\
/** \
 * @brief Returns the underlying slice_##type (do not free the pointer inside) \
 * \
 * @pre b != NULL \
 * \
 * Performance: O(1) \
 */ \
static inline slice_##type borrowed_slice_##type##_get( \
    const borrowed_slice_##type* b) { \
    require_msg(b != NULL, "borrowed_slice_" #type "_get: b cannot be NULL"); \
    return b->slice; \
} \
\
/** @brief Returns element count of the borrowed slice. O(1) */ \
static inline usize borrowed_slice_##type##_len( \
    const borrowed_slice_##type* b) { \
    return b ? b->slice.len : 0; \
} \
\
/** @brief Returns true if slice.ptr != NULL. O(1) */ \
static inline bool borrowed_slice_##type##_is_valid( \
    const borrowed_slice_##type* b) { \
    return b != NULL && b->slice.ptr != NULL; \
} \
\
/** \
 * @brief Returns a const pointer to element i (bounds-checked) \
 * \
 * @return const type* — do NOT free. NULL if b == NULL or i >= len. \
 * \
 * Performance: O(1) \
 */ \
static inline const type* borrowed_slice_##type##_at( \
    const borrowed_slice_##type* b, usize i) { \
    if (!b || !b->slice.ptr || i >= b->slice.len) return NULL; \
    return &b->slice.ptr[i]; \
} \
\
/** \
 * @brief Returns a sub-borrow of [start, end) — no allocation \
 * \
 * Source tag is inherited from the original borrowed_slice. \
 * \
 * Performance: O(1) \
 */ \
static inline borrowed_slice_##type borrowed_slice_##type##_slice( \
    borrowed_slice_##type b, usize start, usize end) { \
    return (borrowed_slice_##type){ \
        .slice  = slice_##type##_slice(b.slice, start, end), \
        .source = b.source \
    }; \
} \
\
/** \
 * @brief Returns a borrowed_bytes view over the slice's raw memory \
 * \
 * Byte length = slice.len * sizeof(type). \
 * Source tag is inherited. \
 * \
 * Performance: O(1) \
 */ \
static inline borrowed_bytes borrowed_slice_##type##_as_bytes( \
    const borrowed_slice_##type* b) { \
    if (!b || !b->slice.ptr) return borrowed_bytes_empty(); \
    return (borrowed_bytes){ \
        .bytes  = cbytes_from(b->slice.ptr, b->slice.len * sizeof(type)), \
        .source = b->source \
    }; \
}

#endif /* CANON_SEMANTICS_BORROW_H */
