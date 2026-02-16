#ifndef CANON_UTIL_STR_SPLIT_H
#define CANON_UTIL_STR_SPLIT_H

#include "core/primitives/types.h"       // usize, bool
#include "core/memory.h"                 // str_len, mem_move, mem_zero

/**
 * @file util/str_split.h
 * @brief Zero-allocation, non-destructive string splitting and trimming
 *
 * Provides efficient string parsing utilities with borrowed view pattern and
 * in-place trimming operations. Implements zero-copy splitting that returns
 * pointers into the original string, along with buffer-modifying trim functions.
 *
 * Portability:
 * - Requires C99 or later
 * - Depends only on Canon-C core modules (types, memory)
 * - No direct standard library usage
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 * Safe to call from multiple threads as long as:
 * - Different strings are being processed
 * - Input strings aren't being modified during split operations
 * - In-place trim functions: caller ensures exclusive access
 *
 * Performance:
 * - Time complexity: O(n) where n = str_len(input)
 * - Space complexity: O(1) - zero allocations for all operations
 * - Split operations: Single pass, zero copying
 * - Trim operations: In-place modification, minimal moves
 * - No hidden costs
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero-copy parsing via borrowed views pattern
 * - Never allocate - caller provides all storage
 * - Non-destructive splitting - input string unchanged
 * - Borrowed views valid only while original string lives
 * - In-place operations clearly marked ("inplace" in name)
 * - Two splitting modes: skip empties (typical) vs keep empties (strict CSV)
 * - Null-safe throughout - graceful handling of edge cases
 * - Const-correct - split functions don't modify input
 * - Predictable behavior on empty input, consecutive delimiters
 * - Perfect for parsing, tokenization, CSV processing
 *
 * Borrowed views pattern:
 * ────────────────────────────────────────────────────────────────────────────
 * Split functions return "borrowed views" - pointers directly into the original
 * string. This is a zero-copy optimization:
 *
 * Advantages:
 * - Zero allocations - extremely fast
 * - Zero copying - optimal memory usage
 * - Perfect for parsing where you just need to examine parts
 * - Composable with other operations (split → process → join)
 *
 * Constraints:
 * - Views are NOT null-terminated (they're substrings within a larger string)
 * - Views remain valid ONLY while original string exists
 * - DO NOT free individual views (they're not allocated)
 * - DO NOT modify through these pointers (they point to const data)
 * - Must use str_ncompare or calculate length, not strcmp
 *
 * Lifetime example:
 * ```c
 * void process_first_token(const char* input) {
 *     const char* parts[10];
 *     size_t count = str_split(input, ',', parts, 10);
 *     if (count > 0) {
 *         process_token(parts[0]); // OK - input still alive
 *     }
 * }
 * ```
 *
 * Working with borrowed views:
 * ────────────────────────────────────────────────────────────────────────────
 * Since views are not null-terminated, handle them specially:
 *
 * METHOD 1 - Calculate length:
 * ```c
 * const char* p = parts[0];
 * const char* end = p;
 * while (*end && *end != delim) end++;
 * usize len = end - p;
 * ```
 *
 * METHOD 2 - Use with str_ncompare:
 * ```c
 * if (str_ncompare(parts[0], "header", 6) == 0) { ... }
 * ```
 *
 * METHOD 3 - Print with precision:
 * ```c
 * printf("%.*s\n", (int)len, parts[0]);
 * ```
 *
 * Split modes: Skip empties vs Keep empties:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. SKIP EMPTIES (str_split):
 *    - Consecutive delimiters treated as one
 *    - Leading/trailing delimiters ignored
 *    - Empty fields skipped
 *    - Perfect for whitespace-delimited text, tokenization
 *
 * 2. KEEP EMPTIES (str_split_keep_empty):
 *    - Consecutive delimiters create empty fields
 *    - Trailing delimiter creates empty field at end
 *    - Preserves structure of delimited data
 *    - Perfect for CSV, fixed-width, strict formats
 *
 * Choose skip empties for natural language / tokenization.
 * Choose keep empties for structured data (CSV, columns).
 */
/* ────────────────────────────────────────────────────────────────────────────
   Internal helper — whitespace check (replaces isspace)
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Checks if a character is whitespace (C locale equivalent)
 *
 * Replaces isspace() to avoid locale dependency and <ctype.h>.
 * Recognizes: space, tab, newline, carriage return, form feed, vertical tab.
 */
static inline bool is_whitespace_char(char c) {
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
 * Parses a string into tokens separated by a delimiter, skipping consecutive
 * delimiters and leading/trailing delimiters. Returns borrowed pointers into
 * the original string (zero-copy).
 *
 * @param s Null-terminated input string
 * @param delim Single character delimiter
 * @param parts_out Buffer to store pointers to start of each part
 * @param max_parts Maximum number of parts the buffer can hold
 * @return Number of parts found and written (≤ max_parts)
 */
static inline usize str_split(
    const char* s,
    char delim,
    const char** parts_out,
    usize max_parts
) {
    if (!s || !parts_out || max_parts == 0) {
        return 0;
    }

    usize count = 0;
    const char* p = s;

    while (*p) {
        // Skip consecutive delimiters
        while (*p == delim) {
            ++p;
        }

        // End after skipping delimiters
        if (!*p) {
            break;
        }

        // Store start of token if space available
        if (count < max_parts) {
            parts_out[count++] = p;
        } else {
            break;  // Buffer full
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
 * Parses a string into tokens separated by a delimiter, preserving empty
 * fields between consecutive delimiters. Returns borrowed pointers into
 * the original string (zero-copy).
 *
 * @param s Null-terminated input string
 * @param delim Single character delimiter
 * @param parts_out Buffer to store pointers to start of each part
 * @param max_parts Maximum number of parts the buffer can hold
 * @return Number of parts found and written (≤ max_parts)
 */
static inline usize str_split_keep_empty(
    const char* s,
    char delim,
    const char** parts_out,
    usize max_parts
) {
    if (!s || !parts_out || max_parts == 0) {
        return 0;
    }

    usize count = 0;
    const char* p = s;
    const char* start = s;

    while (*p || start != p) {
        if (*p == delim || *p == '\0') {
            // Store this field
            if (count < max_parts) {
                parts_out[count++] = start;
            } else {
                break;
            }

            // Next field starts after delimiter
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
 * Removes all occurrences of a specific character from the beginning and
 * end of a string, modifying the buffer in-place.
 *
 * @param s Writable null-terminated string to trim
 * @param trim_ch Character to remove from both ends
 * @return Pointer to trimmed string (same as input s)
 */
static inline char* str_trim_char_inplace(char* s, char trim_ch) {
    if (!s || !*s) {
        return s;
    }

    // Trim leading characters
    char* start = s;
    while (*start == trim_ch) {
        ++start;
    }

    // All trimmed → empty string
    if (!*start) {
        s[0] = '\0';
        return s;
    }

    // Move content left if needed
    if (start > s) {
        mem_move(s, start, str_len(start) + 1);
    }

    // Trim trailing characters
    char* end = s + str_len(s);
    while (end > s && *(end - 1) == trim_ch) {
        --end;
    }

    // Null-terminate at trimmed position
    *end = '\0';

    return s;
}

/**
 * @brief Trims all whitespace characters from both ends (in-place)
 *
 * Removes leading and trailing whitespace using is_whitespace_char().
 *
 * @param s Writable null-terminated string to trim
 * @return Pointer to trimmed string (same as input s)
 */
static inline char* str_trim_whitespace_inplace(char* s) {
    if (!s || !*s) {
        return s;
    }

    // Trim leading whitespace
    char* start = s;
    while (*start && is_whitespace_char(*start)) {
        ++start;
    }

    // All whitespace → empty string
    if (!*start) {
        s[0] = '\0';
        return s;
    }

    // Move content left if needed
    if (start > s) {
        mem_move(s, start, str_len(start) + 1);
    }

    // Trim trailing whitespace
    char* end = s + str_len(s);
    while (end > s && is_whitespace_char(*(end - 1))) {
        --end;
    }

    // Null-terminate
    *end = '\0';

    return s;
}

#endif /* CANON_UTIL_STR_SPLIT_H */
