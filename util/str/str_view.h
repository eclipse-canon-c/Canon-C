#ifndef CANON_UTIL_STR_VIEW_H
#define CANON_UTIL_STR_VIEW_H

#include <stdbool.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "core/memory.h"
#include "util/str/str.h"

/**
 * @file util/str/str_view.h
 * @brief Minimal immutable borrowed string view (pointer + length)
 *
 * Zero-allocation, const-correct, binary-safe string slice.
 * No null-terminator required — length is always explicit.
 *
 * Core properties:
 * ────────────────────────────────────────────────────────────────────────────
 * - sizeof(str_view_t) = 16 bytes on 64-bit (ptr + len)
 * - No heap usage unless you explicitly copy
 * - Null-safe: {NULL, 0} is a valid empty view
 * - Borrowed lifetime — valid only as long as source data exists
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * - str_view_t is passed by value — always valid as a struct
 * - {NULL, 0} is a valid sentinel (null view) — not a contract violation
 * - str_view_to_cstr: buf must not be NULL — contract violation
 * - Functions that operate on view contents guard against NULL ptr
 *   internally when len > 0
 *
 * Usage patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * - Tokenizing / parsing without allocation
 * - Substring passing without copying
 * - Hash table keys / interning results
 * - Split / join intermediate results
 */

/**
 * @brief Immutable string view (slice)
 */
typedef struct {
    const char* ptr;
    usize       len;
} str_view_t;

/* ────────────────────────────────────────────────────────────────────────────
   Creation
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Returns a null view {NULL, 0} */
static inline str_view_t str_view_null(void) {
    str_view_t v;
    v.ptr = NULL;
    v.len = 0;
    return v;
}

/** @brief Returns an empty view {"", 0} — ptr is valid but length is zero */
static inline str_view_t str_view_empty(void) {
    str_view_t v;
    v.ptr = "";
    v.len = 0;
    return v;
}

/** @brief Creates a view from a null-terminated string (NULL → null view) */
static inline str_view_t str_view_from(borrowed(const char*) s) {
    if (!s) return str_view_null();
    {
        str_view_t v;
        v.ptr = s;
        v.len = str_len(s);
        return v;
    }
}

/** @brief Creates a view from a pointer and explicit length */
static inline str_view_t str_view_from_len(borrowed(const char*) p, usize len) {
    str_view_t v;
    v.ptr = p;
    v.len = len;
    return v;
}

/* ────────────────────────────────────────────────────────────────────────────
   Basic queries
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Returns the length of the view */
static inline usize str_view_len(str_view_t v) {
    return v.len;
}

/** @brief Returns true if the view has zero length */
static inline bool str_view_is_empty(str_view_t v) {
    return v.len == 0;
}

/** @brief Returns true if the view pointer is NULL */
static inline bool str_view_is_null(str_view_t v) {
    return v.ptr == NULL;
}

/** @brief Returns true if the view has a valid pointer and non-zero length */
static inline bool str_view_has_data(str_view_t v) {
    return v.ptr != NULL && v.len > 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Comparison
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Returns true if two views have identical content */
static inline bool str_view_equals(str_view_t a, str_view_t b) {
    if (a.len != b.len) return false;
    if (a.len == 0) return true;
    return mem_compare(a.ptr, b.ptr, a.len) == 0;
}

/** @brief Lexicographic comparison of two views */
static inline int str_view_compare(str_view_t a, str_view_t b) {
    usize min_len = a.len < b.len ? a.len : b.len;
    if (min_len > 0) {
        int cmp = mem_compare(a.ptr, b.ptr, min_len);
        if (cmp != 0) return cmp;
    }
    return (a.len < b.len) ? -1 : (a.len > b.len) ? 1 : 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Prefix / suffix checks
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Returns true if the view starts with the given prefix */
static inline bool str_view_starts_with(str_view_t v, str_view_t prefix) {
    if (prefix.len > v.len) return false;
    if (prefix.len == 0) return true;
    return mem_compare(v.ptr, prefix.ptr, prefix.len) == 0;
}

/** @brief Returns true if the view ends with the given suffix */
static inline bool str_view_ends_with(str_view_t v, str_view_t suffix) {
    if (suffix.len > v.len) return false;
    if (suffix.len == 0) return true;
    return mem_compare(v.ptr + v.len - suffix.len, suffix.ptr, suffix.len) == 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Substring / slicing
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Returns a subview starting at start with at most len characters */
static inline str_view_t str_view_sub(str_view_t v, usize start, usize len) {
    usize avail;
    if (start >= v.len) return str_view_empty();
    avail = v.len - start;
    return str_view_from_len(v.ptr + start, len < avail ? len : avail);
}

/** @brief Returns the first n characters as a view */
static inline str_view_t str_view_prefix(str_view_t v, usize n) {
    return str_view_sub(v, 0, n);
}

/** @brief Returns the last n characters as a view */
static inline str_view_t str_view_suffix(str_view_t v, usize n) {
    if (n >= v.len) return v;
    return str_view_from_len(v.ptr + v.len - n, n);
}

/* ────────────────────────────────────────────────────────────────────────────
   Character search
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Returns index of first occurrence of c, or v.len if not found */
static inline usize str_view_find_char(str_view_t v, char c) {
    usize i;
    for (i = 0; i < v.len; i++) {
        if (v.ptr[i] == c) return i;
    }
    return v.len;
}

/** @brief Returns true if the view contains the character */
static inline bool str_view_contains_char(str_view_t v, char c) {
    return str_view_find_char(v, c) < v.len;
}

/* ────────────────────────────────────────────────────────────────────────────
   C-string helpers
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Compares view against a null-terminated string */
static inline bool str_view_equals_cstr(str_view_t v, borrowed(const char*) s) {
    return str_view_equals(v, str_view_from(s));
}

/** @brief Returns true if the view starts with a null-terminated prefix */
static inline bool str_view_starts_with_cstr(str_view_t v, borrowed(const char*) prefix) {
    return str_view_starts_with(v, str_view_from(prefix));
}

/** @brief Returns true if the view ends with a null-terminated suffix */
static inline bool str_view_ends_with_cstr(str_view_t v, borrowed(const char*) suffix) {
    return str_view_ends_with(v, str_view_from(suffix));
}

/* ────────────────────────────────────────────────────────────────────────────
   Copy to null-terminated buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Copies view content into a null-terminated buffer
 *
 * @param v        View to copy
 * @param buf      Destination buffer (must not be NULL — contract)
 * @param buf_size Size of buffer in bytes (must fit v.len + 1)
 * @return true if content fit and was copied, false if buffer too small
 */
static inline bool str_view_to_cstr(str_view_t v, char* buf, usize buf_size) {
    require_msg(buf != NULL, "str_view_to_cstr: buf is NULL");
    if (buf_size <= v.len) return false;
    if (v.len > 0) mem_copy(buf, v.ptr, v.len);
    buf[v.len] = '\0';
    return true;
}

#endif /* CANON_UTIL_STR_VIEW_H */
