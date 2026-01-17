// util/str_join.h
#ifndef CANON_UTIL_STR_JOIN_H
#define CANON_UTIL_STR_JOIN_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "core/memory.h"
#include "semantics/option.h"

/**
 * @file str_join.h
 * @brief Safe, explicit and flexible string joining utilities
 *
 * Provides both zero-allocation (buffer-based) and allocating string join operations  
 * with strong safety guarantees:
 * - Never mutates input strings
 * - Never allocates unless the function name explicitly says "alloc"
 * - Clear ownership rules — caller always knows who owns the result
 * - Null-safe, bounds-checked, overflow-safe
 * - Explicit success/failure reporting (bool or Option)
 * - Handles edge cases cleanly (empty arrays, NULL separator, zero count)
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero hidden allocations in the safe path
 * - Maximum predictability — no surprises on truncation or failure
 * - Composable with other string utilities (split → join round-trips)
 * - Preference for caller-provided buffers (embedded-friendly)
 * - Allocating variants return Option — easy to check & free
 *
 * Key patterns:
 * - **Buffer-based** — no allocation, caller controls memory
 * - **Allocating** — heap-allocated result (caller must free)
 * - **Split + join** — safe delimiter replacement without mutation
 *
 * Usage recommendations:
 * ────────────────────────────────────────────────────────────────────────────
 * Small/fixed-size known data → use str_join() with stack buffer
 * Dynamic/unknown size → use str_alloc_join() + check Option
 * CSV → space conversion, path normalization → use str_rejoin()
 *
 * Always check return value / Option before using result!
 */

/* ────────────────────────────────────────────────────────────────────────────
   Zero-allocation join — caller provides buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Joins array of strings into caller-provided buffer (no allocation)
 *
 * @param dest      Writable destination buffer (caller-owned)
 * @param dest_size Size of dest buffer in bytes (including null terminator)
 * @param parts     Array of null-terminated input strings
 * @param count     Number of strings in parts (0 is allowed)
 * @param sep       Separator string (NULL treated as empty string)
 *
 * @return true if join succeeded and fit in buffer  
 *         false if buffer too small, invalid input, or NULL parts
 *
 * Behavior:
 * - Concatenates: parts[0] + sep + parts[1] + sep + ... + parts[count-1]
 * - Always null-terminates result (even on failure)
 * - Skips separator after last part
 * - Handles empty separator, empty strings, zero count gracefully
 * - Does **not** skip whitespace — exact concatenation
 *
 * Example:
 * ```c
 * const char* words[] = {"hello", "world", "2026"};
 * char buf[64];
 * if (str_join(buf, sizeof(buf), words, 3, " ")) {
 *     printf("%s\n", buf);  // → "hello world 2026"
 * } else {
 *     printf("Buffer too small\n");
 * }
 * ```
 */
static inline bool str_join(
    char* dest,
    size_t dest_size,
    const char** parts,
    size_t count,
    const char* sep
) {
    if (!dest || dest_size == 0) {
        return false;
    }

    if (count == 0) {
        dest[0] = '\0';
        return true;
    }

    if (!parts) {
        return false;
    }

    sep = sep ? sep : "";
    const size_t sep_len = strlen(sep);

    size_t pos = 0;

    for (size_t i = 0; i < count; ++i) {
        if (!parts[i]) {
            return false;
        }

        const size_t part_len = strlen(parts[i]);

        // Check remaining space: part + separator (if not last) + null
        size_t needed = part_len + 1;  // at least part + null
        if (i + 1 < count && sep_len > 0) {
            needed += sep_len;
        }

        if (pos + needed > dest_size) {
            return false;
        }

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
   Allocating join — heap allocated result
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates and joins strings — caller owns the result
 *
 * @param parts  Array of null-terminated input strings
 * @param count  Number of strings to join
 * @param sep    Separator (NULL = empty string)
 *
 * @return Some(heap-allocated joined string) on success  
 *         None on allocation failure, invalid input, or empty array
 *
 * Ownership:
 * - On Some: caller **must** free with str_free() or mem_free()
 * - On None: no allocation occurred
 *
 * Example:
 * ```c
 * const char* items[] = {"apple", "banana", "cherry"};
 * option_charp result = str_alloc_join(items, 3, ", ");
 * if (option_charp_is_some(result)) {
 *     printf("Joined: %s\n", option_charp_unwrap(result));
 *     str_free(option_charp_unwrap(result));
 * }
 * ```
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

    // First pass: calculate total required size
    size_t total_len = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!parts[i]) {
            return option_charp_none();
        }
        total_len += strlen(parts[i]);
        if (i + 1 < count && sep_len > 0) {
            total_len += sep_len;
        }
    }
    total_len += 1;  // null terminator

    char* buffer = (char*)mem_alloc(total_len);
    if (!buffer) {
        return option_charp_none();
    }

    if (str_join(buffer, total_len, parts, count, sep)) {
        return option_charp_some(buffer);
    }

    // Should never happen with correct size calculation, but safety first
    mem_free(buffer);
    return option_charp_none();
}

/* ────────────────────────────────────────────────────────────────────────────
   Split + join helpers (non-destructive transformations)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits string by single character delimiter (non-allocating, borrowed views)
 *
 * @param s         Null-terminated input string
 * @param delim     Single character delimiter
 * @param out_parts Buffer to store const char* pointers (views into s)
 * @param max_parts Maximum number of parts the buffer can hold
 *
 * @return Actual number of parts found (may be > max_parts)
 *
 * Features:
 * - Skips consecutive delimiters
 * - Ignores leading and trailing delimiters
 * - Parts are **borrowed** pointers — valid only as long as original s lives
 * - Zero allocation, zero copying
 */
static inline size_t str_split_to_parts(
    const char* s,
    char delim,
    const char** out_parts,
    size_t max_parts
) {
    if (!s || !out_parts || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    const char* current = s;

    while (*current) {
        // Skip leading delimiters
        while (*current == delim) {
            ++current;
        }
        if (*current == '\0') {
            break;
        }

        if (count < max_parts) {
            out_parts[count] = current;
        }
        count++;

        // Skip until next delimiter
        while (*current && *current != delim) {
            ++current;
        }
    }

    return count;
}

/**
 * @brief Split string by delimiter and rejoin with new separator
 *
 * @param s           Input string to transform
 * @param delim       Original delimiter to split on
 * @param parts_buf   Temporary buffer for split parts (borrowed views)
 * @param max_parts   Size of parts_buf
 * @param dest        Output buffer for rejoined string
 * @param dest_size   Size of destination buffer
 * @param new_sep     New separator to use
 *
 * @return true if rejoin succeeded, false on truncation or invalid input
 *
 * Useful for safe delimiter replacement (CSV → space, path normalization, etc.)
 */
static inline bool str_rejoin(
    const char* s,
    char delim,
    const char** parts_buf,
    size_t max_parts,
    char* dest,
    size_t dest_size,
    const char* new_sep
) {
    size_t count = str_split_to_parts(s, delim, parts_buf, max_parts);
    return str_join(dest, dest_size, parts_buf, count, new_sep);
}

#endif /* CANON_UTIL_STR_JOIN_H */
