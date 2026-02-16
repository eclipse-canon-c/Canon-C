#ifndef CANON_UTIL_STR_VIEW_H
#define CANON_UTIL_STR_VIEW_H

#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/slice.h"

/**
 * @file util/str_view.h
 * @brief String view operations — scanning, searching, trimming, splitting
 *
 * str_view.h is the operations layer on top of the str_t type from
 * core/slice.h. It does not define a new type — all functions operate on
 * str_t directly. Everything returns str_t or a primitive.
 * No allocation, no mutation, no null-terminated output required.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - All functions accept str_t — no raw char* in any public API
 * - All functions are non-mutating — inputs are never modified
 * - No allocation anywhere — results are sub-views of the input
 * - SV_NPOS signals "not found" for all index-returning functions
 * - str_t does not require a null terminator — lengths always explicit
 * - <string.h> used for memchr / memcmp only — no strlen anywhere
 *
 * What core/slice.h already provides (not duplicated here):
 * ────────────────────────────────────────────────────────────────────────────
 * - str_from, str_from_cstr, str_empty           — construction
 * - str_is_empty, str_equal                      — basic queries
 * - str_starts_with, str_ends_with               — prefix/suffix
 * - str_slice, str_take, str_skip, str_as_bytes  — slicing
 *
 * What this file adds:
 * ────────────────────────────────────────────────────────────────────────────
 * Searching:
 *   sv_find_char        — first occurrence of a character → usize / SV_NPOS
 *   sv_rfind_char       — last occurrence of a character  → usize / SV_NPOS
 *   sv_find             — first occurrence of a substring → usize / SV_NPOS
 *   sv_rfind            — last occurrence of a substring  → usize / SV_NPOS
 *   sv_contains_char    — character membership            → bool
 *   sv_contains         — substring membership            → bool
 *   sv_count_char       — count occurrences of a char     → usize
 *
 * Trimming:
 *   sv_trim_left        — strip leading whitespace        → str_t
 *   sv_trim_right       — strip trailing whitespace       → str_t
 *   sv_trim             — strip both ends whitespace      → str_t
 *   sv_trim_chars_left  — strip leading chars from set    → str_t
 *   sv_trim_chars_right — strip trailing chars from set   → str_t
 *   sv_trim_chars       — strip both ends from set        → str_t
 *
 * Splitting:
 *   sv_split_at_char    — split on first occurrence of char  → bool + two str_t
 *   sv_split_at         — split on first occurrence of sub   → bool + two str_t
 *   sv_split_at_index   — split at byte index                → void + two str_t
 *   sv_lines_next       — iterate lines without allocation   → bool + str_t
 *
 * Comparison:
 *   sv_compare          — lexicographic three-way comparison → int
 *   sv_compare_ci       — case-insensitive ASCII comparison  → int
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * util/str_view.h is util/. It depends only on core/.
 * No data/, semantics/, or algo/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - <string.h>: memchr (find char), memcmp (compare) — no Canon-C substitute
 *   at this layer. strlen is never used — all lengths come from str_t.len.
 * - No platform-specific code
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are pure — no shared state. Fully thread-safe.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - sv_find_char / sv_rfind_char:  O(n)
 * - sv_find / sv_rfind:            O(n*m) naive search — suitable for most uses
 * - sv_count_char:                 O(n)
 * - sv_trim_*:                     O(n) worst case
 * - sv_split_*:                    O(n) or O(n*m)
 * - sv_compare / sv_compare_ci:    O(min(a.len, b.len))
 *
 * Quick start:
 * ```c
 * #include "util/str_view.h"
 *
 * str_t s = str_from_cstr("  hello, world  ");
 *
 * str_t trimmed = sv_trim(s);               // "hello, world"
 *
 * usize i = sv_find_char(s, ',');           // 7
 * bool  b = sv_contains(s, str_from_cstr("world")); // true
 *
 * str_t before, after;
 * if (sv_split_at_char(s, ',', &before, &after)) {
 *     // before = "  hello", after = " world  "
 * }
 *
 * // Iterate lines
 * str_t text = str_from_cstr("line1\nline2\nline3");
 * str_t rest = text;
 * str_t line;
 * while (sv_lines_next(&rest, &line)) {
 *     // process line
 * }
 * ```
 *
 * @sa core/slice.h        — str_t type, construction, basic slicing
 * @sa util/str_split.h    — richer multi-split utilities (produces many pieces)
 * @sa util/string.h       — null-terminated string safe ops (copy, concat)
 */

/* ════════════════════════════════════════════════════════════════════════════
   Sentinel
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sentinel value returned when a search finds nothing
 *
 * Consistent with BITSET_NPOS and algo_find_index_slice_##type sentinel.
 */
#define SV_NPOS CANON_USIZE_MAX

/* ════════════════════════════════════════════════════════════════════════════
   Internal helpers
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Returns true if c is ASCII whitespace (space, tab, CR, LF, FF, VT) */
static inline bool sv__is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\r' || c == '\f' || c == '\v';
}

/** @brief Returns true if c appears in the chars set view */
static inline bool sv__in_set(char c, str_t chars) {
    if (!chars.ptr) return false;
    for (usize i = 0; i < chars.len; i++) {
        if (chars.ptr[i] == c) return true;
    }
    return false;
}

/** @brief Lowercase an ASCII character (non-ASCII unchanged) */
static inline char sv__to_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* ════════════════════════════════════════════════════════════════════════════
   Searching — by character
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the index of the first occurrence of c in s
 *
 * @param s str_t to search
 * @param c Character to find
 * @return Index of first occurrence, or SV_NPOS if not found
 *
 * Performance: O(n)
 */
static inline usize sv_find_char(str_t s, char c) {
    if (!s.ptr || s.len == 0) return SV_NPOS;
    const void* p = memchr(s.ptr, (unsigned char)c, s.len);
    if (!p) return SV_NPOS;
    return (usize)((const char*)p - s.ptr);
}

/**
 * @brief Returns the index of the last occurrence of c in s
 *
 * @param s str_t to search
 * @param c Character to find
 * @return Index of last occurrence, or SV_NPOS if not found
 *
 * Performance: O(n)
 */
static inline usize sv_rfind_char(str_t s, char c) {
    if (!s.ptr || s.len == 0) return SV_NPOS;
    usize i = s.len;
    while (i-- > 0) {
        if (s.ptr[i] == c) return i;
    }
    return SV_NPOS;
}

/**
 * @brief Returns true if c appears anywhere in s
 *
 * Performance: O(n)
 */
static inline bool sv_contains_char(str_t s, char c) {
    return sv_find_char(s, c) != SV_NPOS;
}

/**
 * @brief Returns the number of times c appears in s
 *
 * Performance: O(n)
 */
static inline usize sv_count_char(str_t s, char c) {
    if (!s.ptr) return 0;
    usize count = 0;
    for (usize i = 0; i < s.len; i++) {
        if (s.ptr[i] == c) count++;
    }
    return count;
}

/* ════════════════════════════════════════════════════════════════════════════
   Searching — by substring
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the index of the first occurrence of needle in haystack
 *
 * Uses a naive O(n*m) search — adequate for typical string sizes.
 * Returns SV_NPOS if needle is not found or is longer than haystack.
 * An empty needle is always found at index 0.
 *
 * @param haystack str_t to search in
 * @param needle   str_t to search for
 * @return Start index of first match, or SV_NPOS
 *
 * Performance: O(n*m)
 */
static inline usize sv_find(str_t haystack, str_t needle) {
    if (!haystack.ptr) return SV_NPOS;
    if (needle.len == 0) return 0;
    if (needle.len > haystack.len) return SV_NPOS;
    if (!needle.ptr) return SV_NPOS;

    usize limit = haystack.len - needle.len;
    for (usize i = 0; i <= limit; i++) {
        if (memcmp(haystack.ptr + i, needle.ptr, needle.len) == 0) {
            return i;
        }
    }
    return SV_NPOS;
}

/**
 * @brief Returns the index of the last occurrence of needle in haystack
 *
 * Returns SV_NPOS if needle is not found or is longer than haystack.
 * An empty needle is always found at haystack.len.
 *
 * @param haystack str_t to search in
 * @param needle   str_t to search for
 * @return Start index of last match, or SV_NPOS
 *
 * Performance: O(n*m)
 */
static inline usize sv_rfind(str_t haystack, str_t needle) {
    if (!haystack.ptr) return SV_NPOS;
    if (needle.len == 0) return haystack.len;
    if (needle.len > haystack.len) return SV_NPOS;
    if (!needle.ptr) return SV_NPOS;

    usize i = haystack.len - needle.len + 1;
    while (i-- > 0) {
        if (memcmp(haystack.ptr + i, needle.ptr, needle.len) == 0) {
            return i;
        }
    }
    return SV_NPOS;
}

/**
 * @brief Returns true if needle appears anywhere in haystack
 *
 * Performance: O(n*m)
 */
static inline bool sv_contains(str_t haystack, str_t needle) {
    return sv_find(haystack, needle) != SV_NPOS;
}

/* ════════════════════════════════════════════════════════════════════════════
   Trimming — whitespace
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a view with leading whitespace removed
 *
 * Strips space, tab, CR, LF, FF, VT from the left.
 * Returns empty str_t if s is entirely whitespace.
 *
 * Performance: O(n)
 */
static inline str_t sv_trim_left(str_t s) {
    if (!s.ptr) return str_empty();
    usize i = 0;
    while (i < s.len && sv__is_ws(s.ptr[i])) i++;
    return str_from(s.ptr + i, s.len - i);
}

/**
 * @brief Returns a view with trailing whitespace removed
 *
 * Strips space, tab, CR, LF, FF, VT from the right.
 * Returns empty str_t if s is entirely whitespace.
 *
 * Performance: O(n)
 */
static inline str_t sv_trim_right(str_t s) {
    if (!s.ptr) return str_empty();
    usize end = s.len;
    while (end > 0 && sv__is_ws(s.ptr[end - 1])) end--;
    return str_from(s.ptr, end);
}

/**
 * @brief Returns a view with leading and trailing whitespace removed
 *
 * Performance: O(n)
 */
static inline str_t sv_trim(str_t s) {
    return sv_trim_right(sv_trim_left(s));
}

/* ════════════════════════════════════════════════════════════════════════════
   Trimming — character set
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a view with leading characters from the set removed
 *
 * @param s     Input string view
 * @param chars Set of characters to strip (any char in this view is stripped)
 * @return str_t with leading set-chars removed
 *
 * Example:
 * ```c
 * sv_trim_chars_left(str_from_cstr("///path"), str_from_cstr("/"))
 * // → "path"
 * ```
 *
 * Performance: O(n * chars.len)
 */
static inline str_t sv_trim_chars_left(str_t s, str_t chars) {
    if (!s.ptr) return str_empty();
    usize i = 0;
    while (i < s.len && sv__in_set(s.ptr[i], chars)) i++;
    return str_from(s.ptr + i, s.len - i);
}

/**
 * @brief Returns a view with trailing characters from the set removed
 *
 * Performance: O(n * chars.len)
 */
static inline str_t sv_trim_chars_right(str_t s, str_t chars) {
    if (!s.ptr) return str_empty();
    usize end = s.len;
    while (end > 0 && sv__in_set(s.ptr[end - 1], chars)) end--;
    return str_from(s.ptr, end);
}

/**
 * @brief Returns a view with leading and trailing characters from the set removed
 *
 * Performance: O(n * chars.len)
 */
static inline str_t sv_trim_chars(str_t s, str_t chars) {
    return sv_trim_chars_right(sv_trim_chars_left(s, chars), chars);
}

/* ════════════════════════════════════════════════════════════════════════════
   Splitting
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Splits s at the first occurrence of c into two views
 *
 * If c is found at index i:
 *   *before = s[0, i)      (does not include c)
 *   *after  = s[i+1, end)  (does not include c)
 *   returns true
 *
 * If c is not found:
 *   *before = s (entire string)
 *   *after  = empty str_t
 *   returns false
 *
 * Either out pointer may be NULL if that half is not needed.
 *
 * @param s      Input string view
 * @param c      Delimiter character
 * @param before Receives the part before the delimiter (may be NULL)
 * @param after  Receives the part after the delimiter (may be NULL)
 * @return true if c was found, false otherwise
 *
 * Performance: O(n)
 */
static inline bool sv_split_at_char(str_t s, char c, str_t* before, str_t* after) {
    usize idx = sv_find_char(s, c);
    if (idx == SV_NPOS) {
        if (before) *before = s;
        if (after)  *after  = str_empty();
        return false;
    }
    if (before) *before = str_from(s.ptr, idx);
    if (after)  *after  = str_from(s.ptr + idx + 1, s.len - idx - 1);
    return true;
}

/**
 * @brief Splits s at the first occurrence of needle into two views
 *
 * If needle is found at index i:
 *   *before = s[0, i)                  (does not include needle)
 *   *after  = s[i + needle.len, end)   (does not include needle)
 *   returns true
 *
 * If needle is not found:
 *   *before = s
 *   *after  = empty str_t
 *   returns false
 *
 * Either out pointer may be NULL.
 *
 * @param s      Input string view
 * @param needle Delimiter substring
 * @param before Receives the part before the needle (may be NULL)
 * @param after  Receives the part after the needle (may be NULL)
 * @return true if needle was found, false otherwise
 *
 * Performance: O(n*m)
 */
static inline bool sv_split_at(str_t s, str_t needle, str_t* before, str_t* after) {
    usize idx = sv_find(s, needle);
    if (idx == SV_NPOS) {
        if (before) *before = s;
        if (after)  *after  = str_empty();
        return false;
    }
    if (before) *before = str_from(s.ptr, idx);
    if (after)  *after  = str_from(s.ptr + idx + needle.len,
                                    s.len - idx - needle.len);
    return true;
}

/**
 * @brief Splits s at a byte index into two views
 *
 * *left  = s[0, index)
 * *right = s[index, end)
 *
 * If index >= s.len: *left = s, *right = empty.
 * Either out pointer may be NULL.
 *
 * @param s     Input string view
 * @param index Split position (0 = empty left, s.len = empty right)
 * @param left  Receives the first part (may be NULL)
 * @param right Receives the second part (may be NULL)
 *
 * Performance: O(1)
 */
static inline void sv_split_at_index(str_t s, usize index, str_t* left, str_t* right) {
    if (!s.ptr || index >= s.len) {
        if (left)  *left  = s;
        if (right) *right = str_empty();
        return;
    }
    if (left)  *left  = str_from(s.ptr, index);
    if (right) *right = str_from(s.ptr + index, s.len - index);
}

/* ════════════════════════════════════════════════════════════════════════════
   Line iteration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Yields the next line from *rest, advancing it past the newline
 *
 * Handles LF (\n) and CRLF (\r\n). The returned line does NOT include
 * the newline character(s). *rest is advanced past the newline on each call.
 *
 * Usage:
 * ```c
 * str_t text = str_from_cstr("line1\nline2\r\nline3");
 * str_t rest = text;
 * str_t line;
 * while (sv_lines_next(&rest, &line)) {
 *     // process line (no newline included)
 * }
 * // After the loop, rest is empty and the last line has been yielded.
 * ```
 *
 * The final line (without a trailing newline) is still yielded.
 *
 * @param rest  In/out: the remaining unparsed text (updated each call)
 * @param line  Out: the next line, without its newline terminator
 * @return true if a line was yielded, false when rest is exhausted
 *
 * Performance: O(n) per call where n = length until next newline
 */
static inline bool sv_lines_next(str_t* rest, str_t* line) {
    require_msg(rest != NULL, "sv_lines_next: rest cannot be NULL");
    require_msg(line != NULL, "sv_lines_next: line cannot be NULL");

    if (!rest || !line) return false;
    if (!rest->ptr || rest->len == 0) return false;

    usize lf = sv_find_char(*rest, '\n');

    if (lf == SV_NPOS) {
        /* last line — no newline at end */
        *line = *rest;
        *rest = str_empty();
        return true;
    }

    /* strip \r before \n for CRLF support */
    usize line_end = lf;
    if (line_end > 0 && rest->ptr[line_end - 1] == '\r') {
        line_end--;
    }

    *line = str_from(rest->ptr, line_end);
    *rest = str_from(rest->ptr + lf + 1, rest->len - lf - 1);
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Comparison
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Lexicographic three-way comparison of two str_t views
 *
 * Compares byte-by-byte up to min(a.len, b.len). If a common prefix exists,
 * shorter string compares less.
 *
 * @param a First string view
 * @param b Second string view
 * @return < 0 if a < b, 0 if a == b, > 0 if a > b
 *
 * Performance: O(min(a.len, b.len))
 */
static inline int sv_compare(str_t a, str_t b) {
    if (!a.ptr && !b.ptr) return 0;
    if (!a.ptr) return -1;
    if (!b.ptr) return  1;

    usize min_len = a.len < b.len ? a.len : b.len;
    if (min_len > 0) {
        int cmp = memcmp(a.ptr, b.ptr, min_len);
        if (cmp != 0) return cmp;
    }
    if (a.len < b.len) return -1;
    if (a.len > b.len) return  1;
    return 0;
}

/**
 * @brief Case-insensitive ASCII lexicographic comparison
 *
 * Only ASCII letters (A-Z) are folded to lowercase. Non-ASCII bytes
 * are compared by their raw byte value.
 *
 * @param a First string view
 * @param b Second string view
 * @return < 0 if a < b (case-insensitive), 0 if equal, > 0 if a > b
 *
 * Performance: O(min(a.len, b.len))
 */
static inline int sv_compare_ci(str_t a, str_t b) {
    if (!a.ptr && !b.ptr) return 0;
    if (!a.ptr) return -1;
    if (!b.ptr) return  1;

    usize min_len = a.len < b.len ? a.len : b.len;
    for (usize i = 0; i < min_len; i++) {
        char ca = sv__to_lower(a.ptr[i]);
        char cb = sv__to_lower(b.ptr[i]);
        if (ca < cb) return -1;
        if (ca > cb) return  1;
    }
    if (a.len < b.len) return -1;
    if (a.len > b.len) return  1;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   Convenience — algo_cmp_fn compatible comparators for str_t
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief algo_cmp_fn-compatible comparator for str_t (lexicographic)
 *
 * Suitable for use with algo/sort.h on arrays of str_t.
 *
 * @param a   Pointer to first str_t
 * @param b   Pointer to second str_t
 * @param ctx Unused (may be NULL)
 */
static inline int sv_cmp(const void* a, const void* b, void* ctx) {
    (void)ctx;
    return sv_compare(*(const str_t*)a, *(const str_t*)b);
}

/**
 * @brief algo_cmp_fn-compatible case-insensitive comparator for str_t
 *
 * @param a   Pointer to first str_t
 * @param b   Pointer to second str_t
 * @param ctx Unused (may be NULL)
 */
static inline int sv_cmp_ci(const void* a, const void* b, void* ctx) {
    (void)ctx;
    return sv_compare_ci(*(const str_t*)a, *(const str_t*)b);
}

#endif /* CANON_UTIL_STR_VIEW_H */
