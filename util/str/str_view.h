#ifndef CANON_UTIL_STR_VIEW_H
#define CANON_UTIL_STR_VIEW_H

#include "core/primitives/types.h"   // usize, bool
#include "core/memory.h"             // str_len, str_ncompare, mem_equal, mem_compare

/**
 * @file util/str_view.h
 * @brief Minimal immutable borrowed string view (pointer + length)
 *
 * Zero-allocation, const-correct, binary-safe string slice.
 * No null-terminator required — length is always explicit.
 *
 * Core properties:
 * - sizeof(str_view_t) = 16 bytes on 64-bit (ptr + len)
 * - No heap usage unless you explicitly copy
 * - Null-safe: {NULL, 0} is valid empty view
 * - Borrowed lifetime — valid only as long as source data exists
 *
 * Usage patterns:
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

static inline str_view_t str_view_null(void) {
    return (str_view_t){ NULL, 0 };
}

static inline str_view_t str_view_empty(void) {
    return (str_view_t){ "", 0 };
}

static inline str_view_t str_view_from(const char* s) {
    return s ? (str_view_t){ s, str_len(s) } : str_view_null();
}

static inline str_view_t str_view_from_len(const char* p, usize len) {
    return (str_view_t){ p, len };
}

/* ────────────────────────────────────────────────────────────────────────────
   Basic queries
   ──────────────────────────────────────────────────────────────────────────── */

static inline usize str_view_len(str_view_t v) {
    return v.len;
}

static inline bool str_view_is_empty(str_view_t v) {
    return v.len == 0;
}

static inline bool str_view_is_null(str_view_t v) {
    return v.ptr == NULL;
}

static inline bool str_view_has_data(str_view_t v) {
    return v.ptr != NULL && v.len > 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Comparison
   ──────────────────────────────────────────────────────────────────────────── */

static inline bool str_view_equals(str_view_t a, str_view_t b) {
    return a.len == b.len && mem_equal(a.ptr, b.ptr, a.len);
}

static inline int str_view_compare(str_view_t a, str_view_t b) {
    usize min_len = a.len < b.len ? a.len : b.len;
    int cmp = mem_compare(a.ptr, b.ptr, min_len);
    if (cmp != 0) return cmp;
    return (a.len < b.len) ? -1 : (a.len > b.len) ? 1 : 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Prefix / suffix checks (very common)
   ──────────────────────────────────────────────────────────────────────────── */

static inline bool str_view_starts_with(str_view_t v, str_view_t prefix) {
    return prefix.len <= v.len && str_ncompare(v.ptr, prefix.ptr, prefix.len) == 0;
}

static inline bool str_view_ends_with(str_view_t v, str_view_t suffix) {
    return suffix.len <= v.len && str_ncompare(v.ptr + v.len - suffix.len, suffix.ptr, suffix.len) == 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Substring / slicing (core operations)
   ──────────────────────────────────────────────────────────────────────────── */

static inline str_view_t str_view_sub(str_view_t v, usize start, usize len) {
    if (start >= v.len) return str_view_empty();
    usize avail = v.len - start;
    return (str_view_t){ v.ptr + start, len < avail ? len : avail };
}

static inline str_view_t str_view_prefix(str_view_t v, usize n) {
    return str_view_sub(v, 0, n);
}

static inline str_view_t str_view_suffix(str_view_t v, usize n) {
    if (n >= v.len) return v;
    return (str_view_t){ v.ptr + v.len - n, n };
}

/* ────────────────────────────────────────────────────────────────────────────
   Character search (fast & common)
   ──────────────────────────────────────────────────────────────────────────── */

static inline usize str_view_find_char(str_view_t v, char c) {
    for (usize i = 0; i < v.len; i++) {
        if (v.ptr[i] == c) return i;
    }
    return v.len;  // not found
}

static inline bool str_view_contains_char(str_view_t v, char c) {
    return str_view_find_char(v, c) < v.len;
}

/* ────────────────────────────────────────────────────────────────────────────
   Quick helpers
   ──────────────────────────────────────────────────────────────────────────── */

static inline bool str_view_equals_cstr(str_view_t v, const char* s) {
    return str_view_equals(v, str_view_from(s));
}

static inline bool str_view_starts_with_cstr(str_view_t v, const char* prefix) {
    return str_view_starts_with(v, str_view_from(prefix));
}

static inline bool str_view_ends_with_cstr(str_view_t v, const char* suffix) {
    return str_view_ends_with(v, str_view_from(suffix));
}

/* ────────────────────────────────────────────────────────────────────────────
   Optional: copy to null-terminated buffer (when needed)
   ──────────────────────────────────────────────────────────────────────────── */

static inline bool str_view_to_cstr(str_view_t v, char* buf, usize buf_size) {
    if (buf_size <= v.len) return false;
    if (v.len > 0) mem_copy(buf, v.ptr, v.len);
    buf[v.len] = '\0';
    return true;
}

#endif /* CANON_UTIL_STR_VIEW_H */
