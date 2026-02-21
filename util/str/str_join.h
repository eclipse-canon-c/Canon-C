#ifndef CANON_UTIL_STR_JOIN_H
#define CANON_UTIL_STR_JOIN_H

#include "core/primitives/types.h"       // usize, bool
#include "core/memory.h"                 // str_len, mem_copy
#include "../../semantics/option/option.h"  // option_charp (adjust path if needed)

/**
 * @file util/str_join.h
 * @brief Safe, explicit and flexible string joining utilities
 *
 * Provides efficient string concatenation operations with multiple allocation
 * strategies. Implements both zero-allocation buffer-based joining and heap
 * allocation variants, along with split-rejoin utilities for delimiter
 * transformation.
 *
 * Portability:
 * - Requires C99 or later
 * - Depends only on Canon-C core modules (types, memory, option)
 * - No direct standard library string functions used
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 * Safe to call from multiple threads as long as different buffers are used.
 * Input strings must not be modified during operation.
 *
 * Performance:
 * - Time complexity: O(n) where n = total length of all input strings
 * - Space complexity: O(1) for buffer-based, O(n) for allocating variants
 * - Single-pass concatenation (no redundant copying)
 * - Allocation variants: one mem_alloc, one length calculation pass
 * - Split operations: O(n) single pass through input
 * - No hidden allocations in non-allocating functions
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit allocation — function names tell you what allocates
 * - Zero hidden surprises — truncation/failure always reported
 * - Const-correct — input strings never modified
 * - Buffer ownership clear — caller knows who owns memory
 * - Null-safe throughout — graceful handling of NULL inputs
 * - Bounds-checked — buffer overflow impossible
 * - Always null-terminated — even on failure, dest[0] = '\0'
 * - No silent truncation — returns false/None on insufficient space
 * - Composable design — split and join work together seamlessly
 * - Multiple allocation strategies for different use cases
 *
 * Allocation strategies:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. BUFFER-BASED (Zero allocation):
 *    - Function: str_join()
 *    - Caller provides destination buffer
 *    - No heap allocation
 *    - Perfect for: embedded, stack buffers, known max size
 *    - Returns: bool (success/failure)
 *
 * 2. HEAP-ALLOCATED (Dynamic allocation):
 *    - Function: str_alloc_join()
 *    - Allocates exact size needed
 *    - Returns: option_charp (Some or None)
 *    - Success: caller MUST free with str_free() or mem_free()
 *
 * Joining behavior:
 * ────────────────────────────────────────────────────────────────────────────
 * All join functions concatenate strings with a separator between elements:
 *
 * Pattern: parts[0] + sep + parts[1] + sep + ... + parts[n-1]
 *
 * Important behaviors:
 * - Separator appears BETWEEN elements only (not before first or after last)
 * - Empty separator ("" or NULL) → simple concatenation with no delimiter
 * - Empty strings in parts[] → preserved (contribute nothing but still counted)
 * - Zero count → produces empty string ""
 * - NULL in parts[] array → error (returns false/None)
 * - NULL separator → treated as empty string ""
 *
 * Buffer sizing and truncation:
 * ────────────────────────────────────────────────────────────────────────────
 * Buffer-based functions (str_join) require accurate size calculation:
 *
 * Required size = sum(str_len(parts[i])) + (count-1) * str_len(sep) + 1
 *
 * The "+1" is for the null terminator. If buffer too small, function returns
 * false and sets dest[0] = '\0'. NO partial results.
 *
 * Error handling:
 * ────────────────────────────────────────────────────────────────────────────
 * Buffer-based (str_join):
 * - false on ANY error condition
 * - Sets dest[0] = '\0' on failure (safe empty string)
 * - Never partially fills buffer (all-or-nothing)
 *
 * Heap-based (str_alloc_join):
 * - None on ANY error condition
 * - No memory allocated on None
 * - Must check option_charp_is_some() before unwrapping
 *
 * Error conditions:
 * - NULL dest pointer (buffer-based)
 * - Zero dest_size (buffer-based)
 * - NULL parts array
 * - NULL element in parts[] array
 * - Buffer too small (buffer-based)
 * - Allocation failure (heap-based)
 */
/* ────────────────────────────────────────────────────────────────────────────
   Zero-allocation join — caller provides buffer
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Joins array of strings into caller-provided buffer (no allocation)
 *
 * Concatenates strings with separator between elements, writing into
 * caller-provided buffer. Never allocates memory.
 *
 * @param dest Writable destination buffer
 * @param dest_size Size of dest buffer in bytes (including '\0')
 * @param parts Array of null-terminated input strings
 * @param count Number of strings in parts (0 is valid)
 * @param sep Separator string (NULL = empty "")
 * @return true if join succeeded and fit in buffer
 *         false if buffer too small, invalid input, or NULL parts
 */
static inline bool str_join(
    char* dest,
    usize dest_size,
    const char** parts,
    usize count,
    const char* sep
) {
    // Validate destination buffer
    if (!dest || dest_size == 0) {
        return false;
    }

    // Handle empty array → empty string
    if (count == 0) {
        dest[0] = '\0';
        return true;
    }

    // Validate parts array
    if (!parts) {
        dest[0] = '\0';
        return false;
    }

    // Treat NULL separator as empty
    sep = sep ? sep : "";
    const usize sep_len = str_len(sep);

    usize pos = 0;

    for (usize i = 0; i < count; ++i) {
        // Check for NULL part
        if (!parts[i]) {
            dest[0] = '\0';
            return false;
        }

        const usize part_len = str_len(parts[i]);

        // Space needed: part + separator (if not last) + null terminator
        usize needed = part_len + 1;
        if (i + 1 < count && sep_len > 0) {
            needed += sep_len;
        }

        // Check remaining space
        if (pos + needed > dest_size) {
            dest[0] = '\0';
            return false;
        }

        // Copy part
        mem_copy(dest + pos, parts[i], part_len);
        pos += part_len;

        // Add separator if not last
        if (i + 1 < count && sep_len > 0) {
            mem_copy(dest + pos, sep, sep_len);
            pos += sep_len;
        }
    }

    // Null-terminate
    dest[pos] = '\0';
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Allocating join — heap allocated result
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Allocates and joins strings — caller owns the result
 *
 * Allocates exact size needed and joins strings into it.
 *
 * @param parts Array of null-terminated input strings
 * @param count Number of strings to join (0 returns empty string)
 * @param sep Separator string (NULL = empty "")
 * @return Some(heap-allocated joined string) on success
 *         None on invalid input or allocation failure
 */
static inline option_charp str_alloc_join(
    const char** parts,
    usize count,
    const char* sep
) {
    // Handle empty array → empty string
    if (!parts || count == 0) {
        return str_alloc_copy("");
    }

    // Treat NULL separator as empty
    sep = sep ? sep : "";
    const usize sep_len = str_len(sep);

    // First pass: calculate exact size needed
    usize total_len = 1;  // null terminator
    for (usize i = 0; i < count; ++i) {
        // Check for NULL part
        if (!parts[i]) {
            return option_charp_none();
        }
        total_len += str_len(parts[i]);
        if (i + 1 < count && sep_len > 0) {
            total_len += sep_len;
        }
    }

    // Allocate exact size
    char* buffer = (char*)mem_alloc(total_len);
    if (!buffer) {
        return option_charp_none();
    }

    // Perform join
    if (str_join(buffer, total_len, parts, count, sep)) {
        return option_charp_some(buffer);
    }

    // Should never reach here with correct size, but safety first
    mem_free(buffer);
    return option_charp_none();
}

/* ────────────────────────────────────────────────────────────────────────────
   Split + join helpers (non-destructive transformations)
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Splits string by single character delimiter (non-allocating, borrowed views)
 *
 * Splits a string into an array of borrowed pointers (views) at delimiter
 * boundaries. Does not allocate or modify input.
 *
 * @param s Null-terminated input string
 * @param delim Single character delimiter
 * @param out_parts Buffer to store const char* pointers (views into s)
 * @param max_parts Size of out_parts
 * @return Actual number of parts found (may exceed max_parts)
 */
static inline usize str_split_to_parts(
    const char* s,
    char delim,
    const char** out_parts,
    usize max_parts
) {
    if (!s || !out_parts || max_parts == 0) {
        return 0;
    }

    usize count = 0;
    const char* current = s;

    while (*current) {
        // Skip leading delimiters
        while (*current == delim) {
            ++current;
        }

        // End after skipping delimiters
        if (*current == '\0') {
            break;
        }

        // Store pointer if space available
        if (count < max_parts) {
            out_parts[count] = current;
        }
        count++;

        // Skip until next delimiter or end
        while (*current && *current != delim) {
            ++current;
        }
    }

    return count;
}

/**
 * @brief Split string by delimiter and rejoin with new separator
 *
 * Combines splitting and joining in one operation.
 *
 * @param s Input string
 * @param delim Original delimiter
 * @param parts_buf Temporary buffer for split parts
 * @param max_parts Size of parts_buf
 * @param dest Output buffer
 * @param dest_size Size of dest
 * @param new_sep New separator (NULL = empty)
 * @return true if rejoin succeeded, false on truncation or invalid input
 */
static inline bool str_rejoin(
    const char* s,
    char delim,
    const char** parts_buf,
    usize max_parts,
    char* dest,
    usize dest_size,
    const char* new_sep
) {
    usize count = str_split_to_parts(s, delim, parts_buf, max_parts);
    return str_join(dest, dest_size, parts_buf, count, new_sep);
}

#endif /* CANON_UTIL_STR_JOIN_H */
