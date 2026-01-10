// util/str_join.h
#ifndef CANON_UTIL_STR_JOIN_H
#define CANON_UTIL_STR_JOIN_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "core/memory.h"
#include "semantics/option.h"
#include "util/string.h"  // for str_equals, etc. if needed

/**
 * @file str_join.h
 * @brief Safe string joining utilities (buffer-based and allocating)
 *
 * Provides zero-allocation and allocating join functions with strict safety:
 *   - Never mutate input strings
 *   - Never allocate unless explicitly named "alloc"
 *   - Never take ownership unless returning Option<char*>
 *   - Return explicit success/failure (bool or Option)
 *   - Null-safe and bounds-checked
 *
 * All functions handle:
 *   - NULL separator (treated as empty string)
 *   - Zero count (produces empty string)
 *   - Invalid inputs gracefully
 *
 * Key patterns:
 *   - Buffer-based: caller provides destination → no allocation
 *   - Allocating: returns Option<char*> → caller must free on Some
 *   - Split + join round-trip helpers for safe transformations
 *
 * Usage example (buffer-based):
 *   const char* parts[] = {"hello", "world", "2026"};
 *   char buf[64];
 *   if (str_join(buf, sizeof(buf), parts, 3, " ")) {
 *       printf("%s\n", buf);  // "hello world 2026"
 *   }
 *
 * Usage example (allocating):
 *   option_charp joined = str_alloc_join(parts, 3, ", ");
 *   if (option_is_some(joined)) {
 *       printf("%s\n", option_unwrap(joined));
 *       str_free(option_unwrap(joined));
 *   }
 */

/* ────────────────────────────────────────────────────────────────────────────
   Buffer-based join (no allocation, no input mutation)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Joins multiple strings into a caller-provided buffer
 *
 * @param dest      Destination buffer (caller-owned, writable)
 * @param dest_size Size of destination buffer in bytes (incl. null terminator)
 * @param parts     Array of null-terminated input strings
 * @param count     Number of parts to join (may be 0)
 * @param sep       Separator string (NULL treated as empty)
 * @return          true on success (joined fits), false on:
 *                  - insufficient space
 *                  - invalid pointers
 *                  - any part is NULL
 *
 * Behavior:
 *   - Concatenates parts[0] + sep + parts[1] + sep + ... + parts[count-1]
 *   - Always null-terminates result
 *   - Handles empty separator, zero count, empty parts
 */
static inline bool str_join(
    char* dest,
    size_t dest_size,
    const char** parts,
    size_t count,
    const char* sep
) {
    if (!dest || dest_size == 0) return false;
    if (count == 0) {
        dest[0] = '\0';
        return true;
    }
    if (!parts) return false;

    sep = sep ? sep : "";
    const size_t sep_len = strlen(sep);

    size_t pos = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!parts[i]) return false;

        const size_t part_len = strlen(parts[i]);

        // Check space for part + separator (except after last) + null
        const size_t needed = part_len +
                              (i + 1 < count && sep_len > 0 ? sep_len : 0) +
                              1;

        if (pos + needed > dest_size) return false;

        mem_copy(dest + pos, parts[i], part_len);
        pos += part_len;

        if (i + 1 < count && sep_len > 0) {
            mem_copy(dest + pos, sep, sep_len);
            pos += sep_len;
        }
    }

    dest[pos] = '\0';
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Allocating join (caller must free on Some)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates and joins multiple strings (caller owns result)
 *
 * @param parts Array of null-terminated input strings
 * @param count Number of parts to join
 * @param sep   Separator string (NULL = empty)
 * @return      Some(owned joined string) on success,
 *              None on allocation failure or invalid input
 *
 * Ownership: On Some, caller must call str_free() or mem_free() on the pointer
 */
static inline option_charp str_alloc_join(
    const char** parts,
    size_t count,
    const char* sep
) {
    if (!parts || count == 0) {
        return str_alloc_copy("");
    }

    sep = sep ? sep : "";
    const size_t sep_len = strlen(sep);

    // Compute total length needed
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!parts[i]) return option_charp_none();
        total += strlen(parts[i]);
        if (i + 1 < count && sep_len > 0) total += sep_len;
    }
    total += 1;  // null terminator

    char* out = (char*)mem_alloc(total);
    if (!out) return option_charp_none();

    if (str_join(out, total, parts, count, sep)) {
        return option_charp_some(out);
    }

    mem_free(out);
    return option_charp_none();
}

/* ────────────────────────────────────────────────────────────────────────────
   Non-mutating split + join (safe round-trip transformations)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits a string into borrowed parts (non-mutating, no allocation)
 *
 * @param s         Null-terminated input string
 * @param delim     Single character delimiter
 * @param out_parts Buffer to receive const char* pointers (borrowed views)
 * @param max_parts Maximum number of parts to store
 * @return          Number of parts found/written
 *
 * Skips consecutive delimiters and leading/trailing ones.
 * Parts are pointers into original `s` — valid only as long as `s` lives.
 */
static inline size_t str_split_to_parts(
    const char* s,
    char delim,
    const char** out_parts,
    size_t max_parts
) {
    if (!s || !out_parts || max_parts == 0) return 0;

    size_t count = 0;
    const char* start = s;

    while (*start) {
        // Skip leading delimiters
        while (*start == delim) ++start;
        if (*start == '\0') break;

        if (count < max_parts) {
            out_parts[count++] = start;
        }

        // Find next delimiter
        while (*start && *start != delim) ++start;
    }

    return count;
}

/**
 * @brief Splits a string by delimiter and rejoins with new separator
 *
 * Convenience for safe delimiter replacement (e.g. comma → space)
 *
 * @param s           Input string to split/rejoin
 * @param delim       Original delimiter to split on
 * @param parts_buffer Buffer for temporary borrowed parts (caller provides)
 * @param max_parts   Size of parts_buffer
 * @param dest        Destination buffer for rejoined string
 * @param dest_size   Size of destination buffer
 * @param sep         New separator for joining
 * @return            true if rejoined successfully, false on truncation/error
 */
static inline bool str_rejoin(
    const char* s,
    char delim,
    const char** parts_buffer,
    size_t max_parts,
    char* dest,
    size_t dest_size,
    const char* sep
) {
    size_t count = str_split_to_parts(s, delim, parts_buffer, max_parts);
    return str_join(dest, dest_size, parts_buffer, count, sep);
}

#endif /* CANON_UTIL_STR_JOIN_H */
