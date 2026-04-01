#ifndef CANON_UTIL_STR_SPLIT_H
#define CANON_UTIL_STR_SPLIT_H

#include <stdbool.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "core/memory.h"
#include "util/str/str.h"

/**
 * @file util/str/str_split.h
 * @brief Zero-allocation, non-destructive string splitting and trimming
 *
 * Provides efficient string parsing utilities with borrowed view pattern and
 * in-place trimming operations. Implements zero-copy splitting that returns
 * pointers into the original string, along with buffer-modifying trim functions.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends only on Canon-C core and util/str modules (types, contract, ownership, memory, str)
 * - No direct standard library usage
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * - s (input string) must not be NULL — violated → contract abort
 * - parts_out must not be NULL — violated → contract abort
 * - max_parts == 0 is a data condition → returns 0 (not a contract violation)
 * - In-place trim: s must not be NULL — violated → contract abort
 *
 * Borrowed views pattern:
 * ────────────────────────────────────────────────────────────────────────────
 * Split functions return pointers directly into the original string.
 * Views are NOT null-terminated. They remain valid ONLY while the
 * original string exists. Do NOT free individual views.
 *
 * Split modes:
 * ────────────────────────────────────────────────────────────────────────────
 * - str_split: skip empties — consecutive delimiters treated as one
 * - str_split_keep_empty: keep empties — preserves field structure (CSV)
 *
 * @sa util/str/str_join.h — joining utilities
 */

/* ────────────────────────────────────────────────────────────────────────────
   Internal helper — whitespace check
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if a character is ASCII whitespace
 *
 * Avoids <ctype.h> and locale dependency.
 * Recognizes: space, tab, newline, carriage return, form feed, vertical tab.
 *
 * @remark Internal — use str_trim_whitespace_inplace() at call sites.
 */
static inline bool canon_split_is_ws_(char c) {
    switch (c) {
        case ' ': case '\t': case '\n': case '\r':
        case '\f': case '\v':
            return true;
        default:
            return false;
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Splitting — returns borrowed views (zero-copy, non-owning)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits string by single character delimiter, skipping empty segments
 *
 * Consecutive delimiters are treated as one. Leading and trailing
 * delimiters are skipped. Returns borrowed pointers into the original
 * string (zero-copy, NOT null-terminated).
 *
 * @param s         Null-terminated input string (must not be NULL — contract)
 * @param delim     Single character delimiter
 * @param parts_out Buffer to store pointers to start of each part
 *                  (must not be NULL — contract)
 * @param max_parts Maximum number of parts the buffer can hold (0 → returns 0)
 * @return Number of parts found and written
 */
static inline usize str_split(
    borrowed(const char*)  s,
    char                   delim,
    const char**           parts_out,
    usize                  max_parts)
{
    usize count;
    const char* p;

    require_msg(s         != NULL, "str_split: s is NULL");
    require_msg(parts_out != NULL, "str_split: parts_out is NULL");

    if (max_parts == 0) return 0;

    count = 0;
    p     = s;

    while (*p) {
        /* Skip consecutive delimiters */
        while (*p == delim) ++p;
        if (!*p) break;

        /* Store start of token if space available */
        if (count < max_parts) {
            parts_out[count++] = p;
        } else {
            break;
        }

        /* Skip to next delimiter or end */
        while (*p && *p != delim) ++p;
    }

    return count;
}

/**
 * @brief Splits string by delimiter — keeps empty fields
 *
 * Consecutive delimiters create empty fields. Preserves structure
 * of delimited data. Returns borrowed pointers into the original
 * string (zero-copy, NOT null-terminated).
 *
 * @param s         Null-terminated input string (must not be NULL — contract)
 * @param delim     Single character delimiter
 * @param parts_out Buffer to store pointers to start of each part
 *                  (must not be NULL — contract)
 * @param max_parts Maximum number of parts the buffer can hold (0 → returns 0)
 * @return Number of parts found and written
 */
static inline usize str_split_keep_empty(
    borrowed(const char*)  s,
    char                   delim,
    const char**           parts_out,
    usize                  max_parts)
{
    usize count;
    const char* p;
    const char* start;

    require_msg(s         != NULL, "str_split_keep_empty: s is NULL");
    require_msg(parts_out != NULL, "str_split_keep_empty: parts_out is NULL");

    if (max_parts == 0) return 0;

    count = 0;
    p     = s;
    start = s;

    while (*p || start != p) {
        if (*p == delim || *p == '\0') {
            if (count < max_parts) {
                parts_out[count++] = start;
            } else {
                break;
            }
            start = p + 1;
        }

        if (*p == '\0') break;
        ++p;
    }

    return count;
}

/* ────────────────────────────────────────────────────────────────────────────
   In-place trimming — modifies caller-owned buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Trims specific character from both ends of string (in-place)
 *
 * Removes all occurrences of a specific character from the beginning and
 * end of a string, modifying the buffer in-place.
 *
 * @param s        Writable null-terminated string (must not be NULL — contract)
 * @param trim_ch  Character to remove from both ends
 * @return Pointer to trimmed string (same as input s)
 */
static inline char* str_trim_char_inplace(char* s, char trim_ch) {
    char* start;
    char* end;

    require_msg(s != NULL, "str_trim_char_inplace: s is NULL");

    if (!*s) return s;

    /* Trim leading */
    start = s;
    while (*start == trim_ch) ++start;

    /* All trimmed → empty string */
    if (!*start) {
        s[0] = '\0';
        return s;
    }

    /* Move content left if needed */
    if (start > s) {
        mem_move(s, start, str_len(start) + 1);
    }

    /* Trim trailing */
    end = s + str_len(s);
    while (end > s && *(end - 1) == trim_ch) --end;
    *end = '\0';

    return s;
}

/**
 * @brief Trims all whitespace characters from both ends (in-place)
 *
 * Removes leading and trailing whitespace (space, tab, newline,
 * carriage return, form feed, vertical tab).
 *
 * @param s Writable null-terminated string (must not be NULL — contract)
 * @return Pointer to trimmed string (same as input s)
 */
static inline char* str_trim_whitespace_inplace(char* s) {
    char* start;
    char* end;

    require_msg(s != NULL, "str_trim_whitespace_inplace: s is NULL");

    if (!*s) return s;

    /* Trim leading whitespace */
    start = s;
    while (*start && canon_split_is_ws_(*start)) ++start;

    /* All whitespace → empty string */
    if (!*start) {
        s[0] = '\0';
        return s;
    }

    /* Move content left if needed */
    if (start > s) {
        mem_move(s, start, str_len(start) + 1);
    }

    /* Trim trailing whitespace */
    end = s + str_len(s);
    while (end > s && canon_split_is_ws_(*(end - 1))) --end;
    *end = '\0';

    return s;
}

#endif /* CANON_UTIL_STR_SPLIT_H */
