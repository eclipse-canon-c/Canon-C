// util/str_split.h
#ifndef CANON_UTIL_STR_SPLIT_H
#define CANON_UTIL_STR_SPLIT_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>  // for isspace()

/**
 * @file str_split.h
 * @brief Zero-allocation, non-destructive string splitting and trimming
 *
 * Provides efficient, safe string splitting and trimming utilities with:
 * - **Zero memory allocation**
 * - **No modification** of input strings (splitting returns borrowed views)
 * - **Borrowed views** — pointers into original data (valid while source lives)
 * - **Null-safe** and reasonably bounds-checked operations
 * - **Two splitting modes**: skipping empty fields vs keeping them
 * - **In-place trimming** — modifies caller-owned buffer only
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - Maximum safety: never write beyond provided buffers
 * - Maximum predictability: clear behavior on empty input, consecutive delimiters
 * - Borrowed views pattern: zero-copy, perfect for parsing/tokenizing
 * - Composable with str_join.h (split → process → join)
 * - Lightweight: suitable for embedded/real-time code
 *
 * Important notes:
 * - Split functions return **non-null-terminated views** — they point to substrings
 *   ending at next delimiter or '\0'. Use length information or strncmp when needed.
 * - In-place trim functions modify the input buffer — caller must own it
 * - All functions handle NULL input gracefully (safe returns)
 *
 * Typical usage patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * Tokenizing CSV (skip empties):
 *   const char* parts[32];
 *   size_t n = str_split(line, ',', parts, 32);
 *
 * Keeping empty fields (strict CSV):
 *   size_t n = str_split_keep_empty(row, ',', fields, 16);
 *
 * Cleaning user input:
 *   str_trim_whitespace_inplace(user_input);
 */

/* ────────────────────────────────────────────────────────────────────────────
   Splitting — returns borrowed views (zero-copy, non-owning)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits string by single character delimiter, skipping empty segments
 *
 * @param s         Null-terminated input string
 * @param delim     Single character delimiter
 * @param parts_out Buffer to store pointers to start of each part
 * @param max_parts Maximum number of parts the buffer can hold
 *
 * @return Number of parts found and written (≤ max_parts)
 *
 * Behavior:
 * - Skips consecutive delimiters
 * - Ignores leading and trailing delimiters
 * - Returns **borrowed views** — pointers into original s
 * - Parts are **not null-terminated** — they end at next delim or '\0'
 * - Safe: returns 0 on NULL input or zero-capacity buffer
 *
 * Example:
 * ```c
 * const char* s = "hello,,world,  ,test";
 * const char* parts[10];
 * size_t n = str_split(s, ',', parts, 10);
 * // n = 3
 * // parts[0] → "hello"   (ends at first ',')
 * // parts[1] → "world"
 * // parts[2] → "test"
 * ```
 */
static inline size_t str_split(
    const char* s,
    char delim,
    const char** parts_out,
    size_t max_parts
) {
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

        // Store start of current token
        if (count < max_parts) {
            parts_out[count++] = p;
        }

        // Skip to next delimiter or end
        while (*p && *p != delim) {
            ++p;
        }
    }

    return count;
}

/**
 * @brief Splits string by delimiter — keeps empty fields
 *
 * @param s         Null-terminated input string
 * @param delim     Single character delimiter
 * @param parts_out Buffer for part start pointers
 * @param max_parts Maximum number of parts buffer can hold
 *
 * @return Number of parts written (≤ max_parts)
 *
 * Behavior:
 * - Preserves empty fields between consecutive delimiters
 * - Includes empty field after trailing delimiter
 * - Still returns **borrowed views** — not null-terminated
 *
 * Example:
 * ```c
 * const char* csv = "a,,b,";
 * const char* fields[10];
 * size_t n = str_split_keep_empty(csv, ',', fields, 10);
 * // n = 4
 * // fields: ["a", "", "b", ""]
 * ```
 */
static inline size_t str_split_keep_empty(
    const char* s,
    char delim,
    const char** parts_out,
    size_t max_parts
) {
    if (!s || !parts_out || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    const char* p = s;
    const char* start = s;

    while (*p || start != p) {  // Process last field even if empty
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

/* ────────────────────────────────────────────────────────────────────────────
   In-place trimming — modifies caller-owned buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Trims specific character from both ends of string (in-place)
 *
 * @param s       Null-terminated string to trim (must be writable)
 * @param trim_ch Character to remove from both ends
 *
 * @return Pointer to (possibly moved) start of trimmed string
 *
 * Behavior:
 * - Modifies buffer in-place using memmove
 * - Handles empty string and all-trimmed cases correctly
 * - Safe: returns s even if no change
 */
static inline char* str_trim_char_inplace(char* s, char trim_ch) {
    if (!s || !*s) {
        return s;
    }

    // Trim leading
    char* start = s;
    while (*start == trim_ch) {
        ++start;
    }

    // Nothing left
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
 * @brief Trims all whitespace characters from both ends (in-place)
 *
 * @param s Writable null-terminated string
 * @return Pointer to trimmed start (same as input unless shifted)
 *
 * Uses isspace() with proper unsigned char cast — locale-aware
 */
static inline char* str_trim_whitespace_inplace(char* s) {
    if (!s || !*s) {
        return s;
    }

    // Trim leading whitespace
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

    // Trim trailing whitespace
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) {
        --end;
    }

    *end = '\0';
    return s;
}

#endif /* CANON_UTIL_STR_SPLIT_H */
