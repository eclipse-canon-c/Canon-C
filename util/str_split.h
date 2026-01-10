// util/str_split.h
#ifndef CANON_UTIL_STR_SPLIT_H
#define CANON_UTIL_STR_SPLIT_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "core/memory.h"   // for memmove (used in trim)

/**
 * @file str_split.h
 * @brief Safe, non-mutating string splitting and trimming utilities
 *
 * Provides zero-allocation, non-destructive string processing functions.
 * All operations:
 *   - Never modify the input string (const-correct where possible)
 *   - Never allocate memory
 *   - Never take ownership
 *   - Return borrowed views (pointers into original string)
 *   - Are null-safe and bounds-checked
 *
 * Perfect for parsing, tokenization, CSV/line splitting, config parsing, etc.
 * without copying or destroying the source string.
 *
 * Core philosophy:
 *   - Split functions return borrowed substrings (valid as long as source lives)
 *   - Trimming functions are explicitly in-place (named "..._in_place")
 *   - Caller provides storage for results (array of const char*)
 *
 * Usage example (split):
 *   const char* parts[16];
 *   size_t count = str_split("a,b,,d,e", ',', parts, 16);
 *   // parts[0] = "a", parts[1] = "b", parts[2] = "d", parts[3] = "e"
 *   // count = 4 (empty segments skipped)
 *
 * Usage example (trim):
 *   char buf[] = "   hello world   ";
 *   str_trim_whitespace(buf);
 *   // buf now == "hello world"
 */

/* ────────────────────────────────────────────────────────────────────────────
   Core: Non-mutating split (safe, recommended)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits a string by single character delimiter into borrowed views
 *
 * @param s         Null-terminated input string (const, never modified)
 * @param delim     Single character delimiter
 * @param out_parts Array of const char* to receive part pointers (borrowed views)
 * @param max_parts Maximum number of parts to write (size of out_parts)
 * @return          Number of parts actually written (≤ max_parts)
 *
 * Behavior:
 *   - Skips consecutive delimiters (no empty parts produced)
 *   - Skips leading/trailing delimiters
 *   - If delim == '\0', treats whole string as single part
 *   - Stops when max_parts reached or input exhausted
 *   - All returned pointers are valid as long as original `s` lives
 *   - Null-safe: returns 0 on invalid input
 *
 * Note: Does **not** null-terminate individual parts — they are length-implicit
 * (stop at next delimiter or end of string).
 */
static inline size_t str_split(
    const char* s,
    char delim,
    const char** out_parts,
    size_t max_parts
) {
    if (!s || !out_parts || max_parts == 0) return 0;

    /* Special case: no delimiter → single part */
    if (delim == '\0') {
        out_parts[0] = s;
        return 1;
    }

    size_t count = 0;
    const char* start = s;

    while (*start) {
        /* Skip leading delimiters */
        while (*start == delim) ++start;
        if (*start == '\0') break;

        if (count < max_parts) {
            out_parts[count++] = start;
        }

        /* Find end of part */
        while (*start && *start != delim) ++start;
        /* Next loop will skip the delimiter if present */
    }

    return count;
}

/* ────────────────────────────────────────────────────────────────────────────
   Trimming (explicit in-place mutation)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Removes leading and trailing instances of a character (in-place)
 *
 * @param s        Null-terminated string to trim (modified in-place)
 * @param trim_ch  Character to remove from start and end
 * @return         Pointer to the trimmed string (may be same as input)
 *
 * Behavior:
 *   - Shifts content left if leading chars removed
 *   - Null-terminates after trimming trailing chars
 *   - Safe on empty or all-trim-char strings (results in empty string)
 *   - Null-safe: returns NULL if input is NULL
 */
static inline char* str_trim_in_place(char* s, char trim_ch) {
    if (!s) return NULL;

    /* Trim leading */
    char* start = s;
    while (*start == trim_ch) ++start;

    /* If entire string was trim chars */
    if (*start == '\0') {
        s[0] = '\0';
        return s;
    }

    /* Move content if needed */
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    /* Trim trailing */
    char* end = s + strlen(s);
    while (end > s && *(end - 1) == trim_ch) --end;
    *end = '\0';

    return s;
}

/**
 * @brief Removes leading/trailing whitespace (space, tab, newline, carriage return)
 *
 * Convenience wrapper around str_trim_in_place with common whitespace chars.
 *
 * @param s Null-terminated string to trim (modified in-place)
 * @return  Pointer to trimmed string (may be same as input)
 */
static inline char* str_trim_whitespace(char* s) {
    if (!s) return NULL;

    char* start = s;
    while (*start && strchr(" \t\n\r", *start)) ++start;

    if (*start == '\0') {
        s[0] = '\0';
        return s;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    char* end = s + strlen(s);
    while (end > s && strchr(" \t\n\r", *(end - 1))) --end;
    *end = '\0';

    return s;
}

#endif /* CANON_UTIL_STR_SPLIT_H */
