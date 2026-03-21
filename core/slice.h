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
 *
 * Types:
 *   bytes_t   — mutable untyped byte view
 *   cbytes_t  — read-only untyped byte view
 *   str_t     — read-only character view (no null terminator required)
 *   DEFINE_SLICE(T) — typed element view, generated per type
 *
 * Ownership:
 *   No slice type owns memory. The backing buffer must outlive every slice
 *   that views it. Passing a slice does not transfer ownership.
 *
 * Bounds safety:
 *   All operations are bounds-checked and NULL-safe, except functions
 *   explicitly suffixed _unchecked — those are debug-only checked (ensure_msg)
 *   and are undefined behavior in release builds on bad input.
 *
 * Performance:
 *   All operations O(1) except *_equal which are O(n).
 *   No allocations. All functions static inline.
 *
 * Portability:
 *   C99. <string.h> for memcmp/strlen only. No platform-specific code.
 *
 * Dependencies:
 *   slice.h lives in core/ and includes only from core/.
 */

/* ============================================================
   bytes_t / cbytes_t — untyped byte views
   ============================================================ */

/** Mutable non-owning byte view. Invariant: ptr != NULL || len == 0. */
typedef struct { u8*       ptr; usize len; } bytes_t;

/** Read-only non-owning byte view. Invariant: ptr != NULL || len == 0. */
typedef struct { const u8* ptr; usize len; } cbytes_t;

/** Read-only non-owning character view. No null terminator required.
 *  Invariant: ptr != NULL || len == 0. */
typedef struct { const char* ptr; usize len; } str_t;

/* ============================================================
   bytes_t — construction
   ============================================================ */

/** Wraps [ptr, ptr+len) as a mutable byte view.
 *  @pre ptr != NULL || len == 0 */
static inline bytes_t bytes_from(void* ptr, usize len) {
    require_msg(ptr != NULL || len == 0, "bytes_from: NULL ptr with non-zero len");
    return (bytes_t){ .ptr = (u8*)ptr, .len = len };
}

/** Returns an empty bytes_t (ptr == NULL, len == 0). */
static inline bytes_t bytes_empty(void) {
    return (bytes_t){ .ptr = NULL, .len = 0 };
}

/** Wraps [ptr, ptr+len) as a read-only byte view.
 *  @pre ptr != NULL || len == 0 */
static inline cbytes_t cbytes_from(const void* ptr, usize len) {
    require_msg(ptr != NULL || len == 0, "cbytes_from: NULL ptr with non-zero len");
    return (cbytes_t){ .ptr = (const u8*)ptr, .len = len };
}

/** Returns an empty cbytes_t (ptr == NULL, len == 0). */
static inline cbytes_t cbytes_empty(void) {
    return (cbytes_t){ .ptr = NULL, .len = 0 };
}

/** Adds const: converts bytes_t to cbytes_t. */
static inline cbytes_t bytes_as_const(bytes_t b) {
    return (cbytes_t){ .ptr = b.ptr, .len = b.len };
}

/* ============================================================
   bytes_t — queries
   ============================================================ */

/** Returns true if len == 0. */
static inline bool bytes_is_empty(bytes_t b) {
    return b.len == 0;
}

/** Returns pointer to byte at index i, or NULL if out of bounds. */
static inline u8* bytes_at(bytes_t b, usize i) {
    if (!b.ptr || i >= b.len) return NULL;
    return b.ptr + i;
}

/** Returns true if a and b have identical contents (O(n)). */
static inline bool bytes_equal(bytes_t a, bytes_t b) {
    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;  /* same pointer — covers two bytes_empty() */
    if (!a.ptr || !b.ptr) return false;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/* ============================================================
   bytes_t — slicing (no copy)
   ============================================================ */

/** Returns sub-view [start, end). end clamped to b.len. O(1). */
static inline bytes_t bytes_slice(bytes_t b, usize start, usize end) {
    if (!b.ptr || start >= b.len) return bytes_empty();
    if (end > b.len) end = b.len;
    if (start >= end) return bytes_empty();
    return (bytes_t){ .ptr = b.ptr + start, .len = end - start };
}

/** Returns first n bytes. n clamped to b.len. O(1). */
static inline bytes_t bytes_take(bytes_t b, usize n) {
    return bytes_slice(b, 0, n);
}

/** Returns bytes after skipping the first n. O(1). */
static inline bytes_t bytes_skip(bytes_t b, usize n) {
    if (!b.ptr || n >= b.len) return bytes_empty();
    return (bytes_t){ .ptr = b.ptr + n, .len = b.len - n };
}

/* ============================================================
   str_t — construction
   ============================================================ */

/** Creates str_t from pointer and explicit length.
 *  @pre ptr != NULL || len == 0 */
static inline str_t str_from(const char* ptr, usize len) {
    require_msg(ptr != NULL || len == 0, "str_from: NULL ptr with non-zero len");
    return (str_t){ .ptr = ptr, .len = len };
}

/** Creates str_t from a null-terminated C string. O(n) via strlen.
 *  Returns empty str_t if cstr == NULL. */
static inline str_t str_from_cstr(const char* cstr) {
    if (!cstr) return (str_t){ .ptr = NULL, .len = 0 };
    return (str_t){ .ptr = cstr, .len = (usize)strlen(cstr) };
}

/** Returns an empty str_t (ptr == NULL, len == 0). */
static inline str_t str_empty(void) {
    return (str_t){ .ptr = NULL, .len = 0 };
}

/* ============================================================
   str_t — queries
   ============================================================ */

/** Returns true if len == 0. */
static inline bool str_is_empty(str_t s) {
    return s.len == 0;
}

/** Returns true if a and b have identical contents (O(n)). */
static inline bool str_equal(str_t a, str_t b) {
    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;
    if (!a.ptr || !b.ptr) return false;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/** Returns true if s begins with prefix. O(prefix.len). */
static inline bool str_starts_with(str_t s, str_t prefix) {
    if (prefix.len > s.len) return false;
    if (!prefix.ptr || prefix.len == 0) return true;
    return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

/** Returns true if s ends with suffix. O(suffix.len). */
static inline bool str_ends_with(str_t s, str_t suffix) {
    if (suffix.len > s.len) return false;
    if (!suffix.ptr || suffix.len == 0) return true;
    return memcmp(s.ptr + (s.len - suffix.len), suffix.ptr, suffix.len) == 0;
}

/* ============================================================
   str_t — slicing (no copy)
   ============================================================ */

/** Returns sub-view [start, end). end clamped to s.len. O(1). */
static inline str_t str_slice(str_t s, usize start, usize end) {
    if (!s.ptr || start >= s.len) return str_empty();
    if (end > s.len) end = s.len;
    if (start >= end) return str_empty();
    return (str_t){ .ptr = s.ptr + start, .len = end - start };
}

/** Returns first n characters. n clamped to s.len. O(1). */
static inline str_t str_take(str_t s, usize n) {
    return str_slice(s, 0, n);
}

/** Returns characters after skipping the first n. O(1). */
static inline str_t str_skip(str_t s, usize n) {
    if (!s.ptr || n >= s.len) return str_empty();
    return (str_t){ .ptr = s.ptr + n, .len = s.len - n };
}

/** Reinterprets str_t as a read-only byte view. O(1). */
static inline cbytes_t str_as_bytes(str_t s) {
    return (cbytes_t){ .ptr = (const u8*)s.ptr, .len = s.len };
}

/* ============================================================
   DEFINE_SLICE(type) — typed non-owning element view
   ============================================================
 *
 * Generates:
 *   slice_T                         struct { T* ptr; usize len; }
 *   slice_T_from(ptr, len)          → slice_T
 *   slice_T_empty()                 → slice_T
 *   slice_T_len(s)                  → usize
 *   slice_T_is_empty(s)             → bool
 *   slice_T_get(s, i, out)          → bool  (bounds-checked, copies element)
 *   slice_T_get_unchecked(s, i)     → T     (debug-only check — UB in release on bad input)
 *   slice_T_at(s, i)                → T*    (NULL if OOB)
 *   slice_T_first(s)                → T*    (NULL if empty)
 *   slice_T_last(s)                 → T*    (NULL if empty)
 *   slice_T_slice(s, start, end)    → slice_T
 *   slice_T_take(s, n)              → slice_T
 *   slice_T_skip(s, n)              → slice_T
 *   slice_T_as_bytes(s)             → bytes_t   (mutable, since T* is mutable)
 *   slice_T_as_cbytes(s)            → cbytes_t  (read-only byte view)
 *
 * Usage:
 *   DEFINE_SLICE(int)
 *   int arr[4] = {1,2,3,4};
 *   slice_int sv = slice_int_from(arr, 4);
 *   int val;
 *   slice_int_get(sv, 2, &val);   // val == 3
 *
 * For pointer types, typedef first:
 *   typedef void* voidptr;
 *   DEFINE_SLICE(voidptr)
 */
#define DEFINE_SLICE(type)                                                          \
                                                                                    \
typedef struct { type* ptr; usize len; } slice_##type;                             \
                                                                                    \
/** Creates slice_##type from ptr and len. @pre ptr!=NULL||len==0 */                \
static inline slice_##type slice_##type##_from(type* ptr, usize len) {             \
    require_msg(ptr != NULL || len == 0,                                            \
        "slice_" #type "_from: NULL ptr with non-zero len");                        \
    return (slice_##type){ .ptr = ptr, .len = len };                                \
}                                                                                   \
                                                                                    \
/** Returns empty slice_##type (ptr==NULL, len==0). */                             \
static inline slice_##type slice_##type##_empty(void) {                            \
    return (slice_##type){ .ptr = NULL, .len = 0 };                                 \
}                                                                                   \
                                                                                    \
/** Returns element count. */                                                       \
static inline usize slice_##type##_len(slice_##type s) {                           \
    return s.len;                                                                   \
}                                                                                   \
                                                                                    \
/** Returns true if len == 0. */                                                    \
static inline bool slice_##type##_is_empty(slice_##type s) {                       \
    return s.len == 0;                                                              \
}                                                                                   \
                                                                                    \
/** Copies element at i into *out. Returns false if out==NULL or i>=len. */        \
static inline bool slice_##type##_get(slice_##type s, usize i, type* out) {        \
    if (!s.ptr || !out || i >= s.len) return false;                                 \
    *out = s.ptr[i]; return true;                                                   \
}                                                                                   \
                                                                                    \
/** Returns element at i. Debug-only bounds check — UB in release on bad input. */ \
static inline type slice_##type##_get_unchecked(slice_##type s, usize i) {         \
    ensure_msg(s.ptr != NULL, "slice_" #type "_get_unchecked: NULL ptr");           \
    ensure_msg(i < s.len,     "slice_" #type "_get_unchecked: index out of bounds");\
    /* cppcheck-suppress returnDanglingLifetime */                                  \
    return s.ptr[i];                                                                \
}                                                                                   \
                                                                                    \
/** Returns pointer to element at i, or NULL if out of bounds. */                  \
static inline type* slice_##type##_at(slice_##type s, usize i) {                   \
    if (!s.ptr || i >= s.len) return NULL;                                          \
    return &s.ptr[i];                                                               \
}                                                                                   \
                                                                                    \
/** Returns pointer to first element, or NULL if empty. */                         \
static inline type* slice_##type##_first(slice_##type s) {                         \
    return (s.ptr && s.len > 0) ? &s.ptr[0] : NULL;                                \
}                                                                                   \
                                                                                    \
/** Returns pointer to last element, or NULL if empty. */                          \
static inline type* slice_##type##_last(slice_##type s) {                          \
    return (s.ptr && s.len > 0) ? &s.ptr[s.len - 1] : NULL;                        \
}                                                                                   \
                                                                                    \
/** Returns sub-view [start, end). end clamped to s.len. O(1). */                  \
static inline slice_##type slice_##type##_slice(                                    \
        slice_##type s, usize start, usize end) {                                   \
    if (!s.ptr || start >= s.len) return slice_##type##_empty();                    \
    if (end > s.len) end = s.len;                                                   \
    if (start >= end) return slice_##type##_empty();                                \
    return (slice_##type){ .ptr = s.ptr + start, .len = end - start };              \
}                                                                                   \
                                                                                    \
/** Returns first n elements. n clamped to s.len. O(1). */                         \
static inline slice_##type slice_##type##_take(slice_##type s, usize n) {          \
    return slice_##type##_slice(s, 0, n);                                           \
}                                                                                   \
                                                                                    \
/** Returns elements after skipping first n. O(1). */                              \
static inline slice_##type slice_##type##_skip(slice_##type s, usize n) {          \
    if (!s.ptr || n >= s.len) return slice_##type##_empty();                        \
    return (slice_##type){ .ptr = s.ptr + n, .len = s.len - n };                    \
}                                                                                   \
                                                                                    \
/** Mutable raw byte view over backing memory. len = s.len * sizeof(type). */      \
static inline bytes_t slice_##type##_as_bytes(slice_##type s) {                    \
    if (!s.ptr) return bytes_empty();                                               \
    return (bytes_t){ .ptr = (u8*)s.ptr, .len = s.len * sizeof(type) };            \
}                                                                                   \
                                                                                    \
/** Read-only raw byte view over backing memory. len = s.len * sizeof(type). */    \
static inline cbytes_t slice_##type##_as_cbytes(slice_##type s) {                  \
    if (!s.ptr) return (cbytes_t){ .ptr = NULL, .len = 0 };                        \
    return (cbytes_t){ .ptr = (const u8*)s.ptr, .len = s.len * sizeof(type) };     \
}

#endif /* CANON_CORE_SLICE_H */
