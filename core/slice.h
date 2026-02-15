#ifndef CANON_CORE_SLICE_H
#define CANON_CORE_SLICE_H

#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"

/**
 * @file core/slice.h
 * @brief Non-owning views into contiguous memory — the canonical "borrow" primitive
 *
 * A slice is a {pointer, length} pair. It does not own its memory.
 * The caller who created the underlying buffer is responsible for its lifetime.
 * Slices become invalid when their backing buffer is freed or the arena is reset.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Two types: bytes_t (untyped byte view) and slice_t (typed element view)
 * - Neither type owns memory — both are borrowed views
 * - All operations are bounds-checked and NULL-safe
 * - Slicing produces sub-slices — no copying
 * - Typed slices (DEFINE_SLICE) are generated per element type via macro
 * - bytes_t is the universal byte-level view used by arena, memory, and I/O
 *
 * Ownership model:
 * ────────────────────────────────────────────────────────────────────────────
 * - slice_t / bytes_t do NOT own memory — they are always borrowed
 * - The backing buffer must outlive every slice that views it
 * - Passing a slice into a function does not transfer ownership
 * - Slices from stack buffers are invalid after the declaring scope ends
 * - Slices from arena buffers are invalid after arena_reset()
 *
 * Relationship to other modules:
 * ────────────────────────────────────────────────────────────────────────────
 * - core/memory.h    — mem_copy / mem_move operate on bytes_t
 * - core/arena.h     — arena_alloc returns a bytes_t view of allocated region
 * - data/vec/vec.h   — vec_as_slice() yields a typed slice view
 * - data/stringbuf.h — stringbuf_as_bytes() yields a bytes_t view
 * - semantics/borrow.h — borrowed_slice / borrowed_str build on slice_t
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * slice.h lives in core/ and may only include from core/
 * No data/, semantics/, or util/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses <string.h> for memcmp only
 * - No platform-specific code
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Slices are value types — copying is safe from any thread.
 * Mutating through a mutable slice pointer requires external synchronization.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations: O(1) except bytes_equal / slice_equal which are O(n)
 * - No allocations anywhere in this file
 * - All functions are static inline — zero call overhead
 *
 * Quick start:
 * ```c
 * #include "core/slice.h"
 *
 * // Byte view (untyped)
 * u8 buf[256];
 * bytes_t b = bytes_from(buf, 256);
 * bytes_t first_half = bytes_slice(b, 0, 128);
 *
 * // String view (read-only)
 * str_t s = str_from_cstr("hello");
 * str_t word = str_slice(s, 0, 3);  // "hel"
 *
 * // Typed slice (generated per type)
 * DEFINE_SLICE(int)
 * int arr[8] = {1,2,3,4,5,6,7,8};
 * slice_int sv = slice_int_from(arr, 8);
 * int val;
 * slice_int_get(sv, 2, &val);  // val = 3
 * ```
 *
 * @sa semantics/borrow.h    — ownership-annotated slice wrappers
 */

/* ════════════════════════════════════════════════════════════════════════════
   bytes_t — untyped mutable byte view
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Mutable non-owning view into a contiguous byte region
 *
 * The canonical type for raw memory operations. Used by arena, memory,
 * pool, and I/O subsystems. Does not own the pointed-to memory.
 *
 * Invariants:
 * - ptr != NULL || len == 0
 * - All bytes [ptr, ptr+len) are readable and writable
 * - Lifetime of ptr must exceed lifetime of this struct
 */
typedef struct {
    u8*   ptr; ///< Pointer to first byte (may be NULL only if len == 0)
    usize len; ///< Number of bytes in view
} bytes_t;

/**
 * @brief Read-only non-owning view into a contiguous byte region
 *
 * Const variant of bytes_t. Used when the backing buffer must not be modified.
 * Functions that only read use cbytes_t; functions that write use bytes_t.
 */
typedef struct {
    const u8* ptr; ///< Pointer to first byte (read-only)
    usize     len; ///< Number of bytes in view
} cbytes_t;

/* ════════════════════════════════════════════════════════════════════════════
   str_t — read-only string view (no null terminator required)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read-only non-owning view into a character sequence
 *
 * str_t is a {const char*, len} pair. The pointed-to characters are NOT
 * required to be null-terminated — length is always explicit.
 * Suitable for string slicing, parsing, and comparison without copying.
 *
 * Invariants:
 * - ptr != NULL || len == 0
 * - Characters [ptr, ptr+len) are readable
 * - No null terminator requirement
 */
typedef struct {
    const char* ptr; ///< Pointer to first character (read-only, not necessarily null-terminated)
    usize       len; ///< Number of characters (NOT including any null terminator)
} str_t;

/* ════════════════════════════════════════════════════════════════════════════
   bytes_t — construction
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates a bytes_t view over an existing buffer
 *
 * @param ptr Pointer to byte buffer (NULL only if len == 0)
 * @param len Number of bytes in the view
 * @return bytes_t wrapping [ptr, ptr+len)
 *
 * @pre ptr != NULL || len == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bytes_t bytes_from(void* ptr, usize len) {
    require_msg(ptr != NULL || len == 0, "bytes_from: ptr cannot be NULL when len > 0");
    return (bytes_t){ .ptr = (u8*)ptr, .len = len };
}

/**
 * @brief Creates an empty bytes_t (ptr == NULL, len == 0)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bytes_t bytes_empty(void) {
    return (bytes_t){ .ptr = NULL, .len = 0 };
}

/**
 * @brief Creates a cbytes_t read-only view over an existing buffer
 *
 * @param ptr Pointer to byte buffer (NULL only if len == 0)
 * @param len Number of bytes in the view
 * @return cbytes_t wrapping [ptr, ptr+len)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline cbytes_t cbytes_from(const void* ptr, usize len) {
    require_msg(ptr != NULL || len == 0, "cbytes_from: ptr cannot be NULL when len > 0");
    return (cbytes_t){ .ptr = (const u8*)ptr, .len = len };
}

/**
 * @brief Converts a bytes_t to a cbytes_t (add const)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline cbytes_t bytes_as_const(bytes_t b) {
    return (cbytes_t){ .ptr = b.ptr, .len = b.len };
}

/* ════════════════════════════════════════════════════════════════════════════
   bytes_t — queries
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if the byte view has no bytes
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool bytes_is_empty(bytes_t b) {
    return b.len == 0;
}

/**
 * @brief Returns a pointer to byte at index i (bounds-checked)
 *
 * @param b bytes_t to index into
 * @param i Byte index
 * @return Pointer to byte at index i, or NULL if out of bounds
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline u8* bytes_at(bytes_t b, usize i) {
    if (!b.ptr || i >= b.len) return NULL;
    return b.ptr + i;
}

/**
 * @brief Returns true if two byte views have identical contents
 *
 * @param a First byte view
 * @param b Second byte view
 * @return true if a.len == b.len and all bytes match
 *
 * Performance:
 * - Time:  O(n) where n = min(a.len, b.len)
 * - Space: O(1)
 */
static inline bool bytes_equal(bytes_t a, bytes_t b) {
    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;
    if (!a.ptr || !b.ptr) return false;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   bytes_t — slicing (no copy)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a sub-view of bytes_t from [start, end)
 *
 * @param b     Source byte view
 * @param start Inclusive start index
 * @param end   Exclusive end index (clamped to b.len if end > b.len)
 * @return bytes_t sub-view — empty if start >= end or b is invalid
 *
 * @post result.ptr points into b's backing buffer — no allocation
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bytes_t bytes_slice(bytes_t b, usize start, usize end) {
    if (!b.ptr || start >= b.len) return bytes_empty();
    if (end > b.len) end = b.len;
    if (start >= end) return bytes_empty();
    return (bytes_t){ .ptr = b.ptr + start, .len = end - start };
}

/**
 * @brief Returns the first n bytes of a byte view
 *
 * @param b bytes_t to take from
 * @param n Number of bytes (clamped to b.len)
 * @return bytes_t sub-view of first n bytes
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bytes_t bytes_take(bytes_t b, usize n) {
    return bytes_slice(b, 0, n);
}

/**
 * @brief Returns bytes after skipping the first n bytes
 *
 * @param b bytes_t to skip into
 * @param n Number of bytes to skip (clamped to b.len)
 * @return bytes_t sub-view of remaining bytes after skip
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bytes_t bytes_skip(bytes_t b, usize n) {
    if (!b.ptr || n >= b.len) return bytes_empty();
    return (bytes_t){ .ptr = b.ptr + n, .len = b.len - n };
}

/* ════════════════════════════════════════════════════════════════════════════
   str_t — construction
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates a str_t from a pointer and explicit length
 *
 * @param ptr Pointer to character data (NULL only if len == 0)
 * @param len Number of characters
 * @return str_t wrapping [ptr, ptr+len)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline str_t str_from(const char* ptr, usize len) {
    require_msg(ptr != NULL || len == 0, "str_from: ptr cannot be NULL when len > 0");
    return (str_t){ .ptr = ptr, .len = len };
}

/**
 * @brief Creates a str_t from a null-terminated C string
 *
 * @param cstr Null-terminated string (NULL returns empty str_t)
 * @return str_t — length is computed via strlen, null terminator excluded
 *
 * @note O(n) due to strlen — use str_from() when length is already known.
 *
 * Performance:
 * - Time:  O(n) where n = strlen(cstr)
 * - Space: O(1)
 */
static inline str_t str_from_cstr(const char* cstr) {
    if (!cstr) return (str_t){ .ptr = NULL, .len = 0 };
    return (str_t){ .ptr = cstr, .len = (usize)strlen(cstr) };
}

/**
 * @brief Returns an empty str_t (ptr == NULL, len == 0)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline str_t str_empty(void) {
    return (str_t){ .ptr = NULL, .len = 0 };
}

/* ════════════════════════════════════════════════════════════════════════════
   str_t — queries
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if the string view has no characters
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool str_is_empty(str_t s) {
    return s.len == 0;
}

/**
 * @brief Returns true if two string views have identical contents
 *
 * @param a First string view
 * @param b Second string view
 * @return true if a.len == b.len and all characters match
 *
 * Performance:
 * - Time:  O(n)
 * - Space: O(1)
 */
static inline bool str_equal(str_t a, str_t b) {
    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;
    if (!a.ptr || !b.ptr) return false;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/**
 * @brief Returns true if str_t starts with prefix
 *
 * @param s      String to check
 * @param prefix Prefix to look for
 * @return true if s begins with prefix
 *
 * Performance:
 * - Time:  O(prefix.len)
 * - Space: O(1)
 */
static inline bool str_starts_with(str_t s, str_t prefix) {
    if (prefix.len > s.len) return false;
    if (!prefix.ptr || prefix.len == 0) return true;
    return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

/**
 * @brief Returns true if str_t ends with suffix
 *
 * @param s      String to check
 * @param suffix Suffix to look for
 * @return true if s ends with suffix
 *
 * Performance:
 * - Time:  O(suffix.len)
 * - Space: O(1)
 */
static inline bool str_ends_with(str_t s, str_t suffix) {
    if (suffix.len > s.len) return false;
    if (!suffix.ptr || suffix.len == 0) return true;
    return memcmp(s.ptr + (s.len - suffix.len), suffix.ptr, suffix.len) == 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   str_t — slicing (no copy)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a sub-view of str_t from [start, end)
 *
 * @param s     Source string view
 * @param start Inclusive start index
 * @param end   Exclusive end index (clamped to s.len if end > s.len)
 * @return str_t sub-view — no allocation
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline str_t str_slice(str_t s, usize start, usize end) {
    if (!s.ptr || start >= s.len) return str_empty();
    if (end > s.len) end = s.len;
    if (start >= end) return str_empty();
    return (str_t){ .ptr = s.ptr + start, .len = end - start };
}

/**
 * @brief Returns the first n characters of a str_t
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline str_t str_take(str_t s, usize n) {
    return str_slice(s, 0, n);
}

/**
 * @brief Returns characters after skipping the first n
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline str_t str_skip(str_t s, usize n) {
    if (!s.ptr || n >= s.len) return str_empty();
    return (str_t){ .ptr = s.ptr + n, .len = s.len - n };
}

/**
 * @brief Converts str_t to a cbytes_t byte view
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline cbytes_t str_as_bytes(str_t s) {
    return (cbytes_t){ .ptr = (const u8*)s.ptr, .len = s.len };
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_SLICE — typed element view
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a typed non-owning slice for element type `type`
 *
 * Generated type:
 * - slice_##type  — {type*, usize} non-owning view
 *
 * Generated functions:
 * - slice_##type##_from(ptr, len)        → slice_##type
 * - slice_##type##_empty()               → slice_##type
 * - slice_##type##_len(s)                → usize
 * - slice_##type##_is_empty(s)           → bool
 * - slice_##type##_get(s, i, out)        → bool (bounds-checked)
 * - slice_##type##_get_unchecked(s, i)   → type (debug-checked)
 * - slice_##type##_at(s, i)              → type* (ptr, NULL if OOB)
 * - slice_##type##_first(s)              → type* (NULL if empty)
 * - slice_##type##_last(s)               → type* (NULL if empty)
 * - slice_##type##_slice(s, start, end)  → slice_##type (sub-view)
 * - slice_##type##_take(s, n)            → slice_##type (first n)
 * - slice_##type##_skip(s, n)            → slice_##type (after n)
 * - slice_##type##_as_bytes(s)           → bytes_t (raw byte view)
 *
 * @param type Element type — must be a valid C identifier
 *             For pointer types, typedef first: typedef void* voidptr;
 */
#define DEFINE_SLICE(type) \
\
/** \
 * @brief Non-owning typed view into a contiguous array of type elements \
 * \
 * Does not own memory. Lifetime of ptr must exceed lifetime of this struct. \
 */ \
typedef struct { \
    type* ptr; /**< Pointer to first element (NULL only if len == 0) */ \
    usize len; /**< Number of elements in view */ \
} slice_##type; \
\
/** \
 * @brief Creates a slice_##type from a pointer and explicit length \
 * \
 * @pre ptr != NULL || len == 0 \
 * \
 * Performance: O(1) \
 */ \
static inline slice_##type slice_##type##_from(type* ptr, usize len) { \
    require_msg(ptr != NULL || len == 0, \
        "slice_" #type "_from: ptr cannot be NULL when len > 0"); \
    return (slice_##type){ .ptr = ptr, .len = len }; \
} \
\
/** @brief Returns an empty slice_##type (ptr == NULL, len == 0). O(1) */ \
static inline slice_##type slice_##type##_empty(void) { \
    return (slice_##type){ .ptr = NULL, .len = 0 }; \
} \
\
/** @brief Returns element count. O(1) */ \
static inline usize slice_##type##_len(slice_##type s) { \
    return s.len; \
} \
\
/** @brief Returns true if len == 0. O(1) */ \
static inline bool slice_##type##_is_empty(slice_##type s) { \
    return s.len == 0; \
} \
\
/** \
 * @brief Copies element at index i into *out (bounds-checked) \
 * \
 * @return true on success, false if out == NULL or i >= len \
 * \
 * Performance: O(1) \
 */ \
static inline bool slice_##type##_get(slice_##type s, usize i, type* out) { \
    if (!s.ptr || !out || i >= s.len) return false; \
    *out = s.ptr[i]; \
    return true; \
} \
\
/** \
 * @brief Returns element at index i without bounds checking \
 * \
 * @pre i < s.len — checked via ensure_msg() in debug builds only \
 * \
 * Performance: O(1) \
 */ \
static inline type slice_##type##_get_unchecked(slice_##type s, usize i) { \
    ensure_msg(s.ptr != NULL, "slice_" #type "_get_unchecked: ptr is NULL"); \
    ensure_msg(i < s.len,     "slice_" #type "_get_unchecked: index out of bounds"); \
    return s.ptr[i]; \
} \
\
/** \
 * @brief Returns pointer to element at index i, or NULL if out of bounds \
 * \
 * Performance: O(1) \
 */ \
static inline type* slice_##type##_at(slice_##type s, usize i) { \
    if (!s.ptr || i >= s.len) return NULL; \
    return &s.ptr[i]; \
} \
\
/** @brief Returns pointer to first element, or NULL if empty. O(1) */ \
static inline type* slice_##type##_first(slice_##type s) { \
    return (s.ptr && s.len > 0) ? &s.ptr[0] : NULL; \
} \
\
/** @brief Returns pointer to last element, or NULL if empty. O(1) */ \
static inline type* slice_##type##_last(slice_##type s) { \
    return (s.ptr && s.len > 0) ? &s.ptr[s.len - 1] : NULL; \
} \
\
/** \
 * @brief Returns sub-view [start, end) — no allocation \
 * \
 * end is clamped to s.len if end > s.len. \
 * Returns empty slice if start >= end or slice is invalid. \
 * \
 * Performance: O(1) \
 */ \
static inline slice_##type slice_##type##_slice(slice_##type s, usize start, usize end) { \
    if (!s.ptr || start >= s.len) return slice_##type##_empty(); \
    if (end > s.len) end = s.len; \
    if (start >= end) return slice_##type##_empty(); \
    return (slice_##type){ .ptr = s.ptr + start, .len = end - start }; \
} \
\
/** @brief Returns first n elements. end clamped to s.len. O(1) */ \
static inline slice_##type slice_##type##_take(slice_##type s, usize n) { \
    return slice_##type##_slice(s, 0, n); \
} \
\
/** @brief Returns elements after skipping first n. O(1) */ \
static inline slice_##type slice_##type##_skip(slice_##type s, usize n) { \
    if (!s.ptr || n >= s.len) return slice_##type##_empty(); \
    return (slice_##type){ .ptr = s.ptr + n, .len = s.len - n }; \
} \
\
/** \
 * @brief Returns a raw bytes_t view over the slice's backing memory \
 * \
 * Byte length = s.len * sizeof(type). O(1) \
 */ \
static inline bytes_t slice_##type##_as_bytes(slice_##type s) { \
    if (!s.ptr) return bytes_empty(); \
    return (bytes_t){ .ptr = (u8*)s.ptr, .len = s.len * sizeof(type) }; \
}

#endif /* CANON_CORE_SLICE_H */
