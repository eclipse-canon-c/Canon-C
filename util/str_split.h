#ifndef CANON_UTIL_STR_SPLIT_H
#define CANON_UTIL_STR_SPLIT_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>      // for isspace()

/**
 * @file str_split.h
 * @brief Zero-allocation, non-destructive string splitting & trimming
 *
 * All functions:
 *  - Never allocate memory
 *  - Never modify the input string (for splitting)
 *  - Return borrowed views (pointers into original data)
 *  - Are null-safe and reasonably bounds-checked
 */

// ─────────────────────────────────────────────────────────────────────────────
//  Splitting — returns borrowed views
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Split string by single char delimiter, skipping empty segments
 * @return number of parts written (≤ max_parts)
 *
 * Behavior:
 *   - skips consecutive delimiters
 *   - skips leading and trailing delimiters
 *   - returns views (pointers remain valid while source lives)
 *   - parts are NOT null-terminated — end at next delimiter or '\0'
 */
static inline size_t str_split(
    const char* s,
    char delim,
    const char** parts_out,
    size_t max_parts)
{
    if (!s || !parts_out || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    const char* p = s;

    while (*p) {
        // Skip delimiters
        while (*p == delim) {
            ++p;
        }
        if (!*p) {
            break;
        }

        // Store start of token
        if (count < max_parts) {
            parts_out[count++] = p;
        }

        // Skip until delimiter or end
        while (*p && *p != delim) {
            ++p;
        }
    }

    return count;
}

/**
 * @brief Split string by delimiter — KEEPING empty fields
 * @return number of parts written (≤ max_parts)
 *
 * Example: "a,,b," → ["a","","b",""]
 */
static inline size_t str_split_keep_empty(
    const char* s,
    char delim,
    const char** parts_out,
    size_t max_parts)
{
    if (!s || !parts_out || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    const char* p = s;
    const char* start = s;

    while (*p || start != p) {  // process last field even if empty
        if (*p == delim || *p == '\0') {
            if (count < max_parts) {
                parts_out[count++] = start;
            }
            start = p + 1;
        }
        if (*p == '\0') {
            break;
        }
        ++p;
    }

    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
//  In-place trimming
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Trim character from both ends (in-place)
 * @return pointer to (possibly moved) start of trimmed string
 */
static inline char* str_trim_char_inplace(char* s, char trim_ch)
{
    if (!s || !*s) {
        return s;
    }

    // Trim leading
    char* start = s;
    while (*start == trim_ch) {
        ++start;
    }

    // Nothing left → empty string
    if (!*start) {
        s[0] = '\0';
        return s;
    }

    // Move content left if needed
    if (start > s) {
        memmove(s, start, strlen(start) + 1);
    }

    // Trim trailing
    char* end = s + strlen(s);
    while (end > s && *(end - 1) == trim_ch) {
        --end;
    }
    *end = '\0';

    return s;
}

/**
 * @brief Trim all whitespace characters (using isspace())
 */
static inline char* str_trim_whitespace_inplace(char* s)
{
    if (!s || !*s) {
        return s;
    }

    // Leading
    char* start = s;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }

    if (!*start) {
        s[0] = '\0';
        return s;
    }

    if (start > s) {
        memmove(s, start, strlen(start) + 1);
    }

    // Trailing
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    *end = '\0';

    return s;
}

#endif
