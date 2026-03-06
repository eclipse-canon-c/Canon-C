#ifndef CANON_UTIL_STR_JOIN_H
#define CANON_UTIL_STR_JOIN_H

#include "core/primitives/types.h"
#include "core/memory.h"
#include "semantics/option/option.h"
#include "util/str/string.h"

/**
 * @file util/str/str_join.h
 * @brief Safe, explicit and flexible string joining utilities
 *
 * Provides efficient string concatenation operations with multiple allocation
 * strategies. Implements both zero-allocation buffer-based joining and heap
 * allocation variants, along with split-rejoin utilities for delimiter
 * transformation.
 *
 * Portability:
 * - Requires C99 or later
 * - Depends only on Canon-C core modules (types, memory, option, string)
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
 *    - Success: caller MUST free with str_free()
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
 * Buffer sizing:
 * ────────────────────────────────────────────────────────────────────────────
 * Required size = sum(str_len(parts[i])) + (count-1) * str_len(sep) + 1
 *
 * The "+1" is for the null terminator. If buffer too small, function returns
 * false and sets dest[0] = '\0'. NO partial results.
 *
 * str_rejoin constraint:
 * ────────────────────────────────────────────────────────────────────────────
 * str_rejoin requires null-terminated parts. It must NOT be used with raw
 * borrowed views from str_split() — those are not null-terminated. Only
 * pass null-terminated string arrays to str_rejoin. See str_split.h for
 * the borrowed view pattern and its constraints.
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
 *
 * @sa util/str/string.h  — option_charp, str_alloc_copy, str_free
 * @sa util/str/str_split.h — splitting utilities (borrowed views)
 */

/* ────────────────────────────────────────────────────────────────────────────
   Zero-allocation join — caller provides buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Joins array of null-terminated strings into caller-provided buffer
 *
 * Concatenates strings with separator between elements, writing into
 * caller-provided buffer. Never allocates memory. All-or-nothing —
 * on failure sets dest[0] = '\0' and returns false.
 *
 * @param dest      Writable destination buffer
 * @param dest_size Size of dest buffer in bytes (including '\0')
 * @param parts     Array of null-terminated input strings — must not be NULL
 * @param count     Number of strings in parts (0 is valid → empty string)
 * @param sep       Separator string (NULL treated as empty "")
 * @return true if join succeeded and fit in buffer
 *         false if buffer too small, invalid input, or NULL element in parts
 */
static inline bool str_join(
    char*        dest,
    usize        dest_size,
    const char** parts,
    usize        count,
    const char*  sep
) {
    if (!dest || dest_size == 0) return false;

    if (count == 0) {
        dest[0] = '\0';
        return true;
    }

    if (!parts) {
        dest[0] = '\0';
        return false;
    }

    sep = sep ? sep : "";
    const usize sep_len = str_len(sep);

    usize pos = 0;

    for (usize i = 0; i < count; ++i) {
        if (!parts[i]) {
            dest[0] = '\0';
            return false;
        }

        const usize part_len = str_len(parts[i]);

        usize needed = part_len + 1;
        if (i + 1 < count && sep_len > 0) needed += sep_len;

        if (pos + needed > dest_size) {
            dest[0] = '\0';
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
 * @brief Allocates and joins null-terminated strings — caller owns the result
 *
 * Allocates exact size needed and joins strings into it.
 * Caller MUST free the result with str_free().
 *
 * @param parts Array of null-terminated input strings
 * @param count Number of strings to join (0 returns empty string)
 * @param sep   Separator string (NULL treated as empty "")
 * @return Some(heap-allocated joined string) on success
 *         None on NULL element in parts, or allocation failure
 */
static inline option_charp str_alloc_join(
    const char** parts,
    usize        count,
    const char*  sep
) {
    if (!parts || count == 0) return str_alloc_copy("");

    sep = sep ? sep : "";
    const usize sep_len = str_len(sep);

    usize total_len = 1; /* null terminator */
    for (usize i = 0; i < count; ++i) {
        if (!parts[i]) return option_charp_none();
        total_len += str_len(parts[i]);
        if (i + 1 < count && sep_len > 0) total_len += sep_len;
    }

    char* buffer = (char*)mem_alloc(total_len);
    if (!buffer) return option_charp_none();

    if (str_join(buffer, total_len, parts, count, sep)) {
        return option_charp_some(buffer);
    }

    mem_free(buffer);
    return option_charp_none();
}

/* ────────────────────────────────────────────────────────────────────────────
   Split + rejoin helper
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits a null-terminated string by delimiter into null-terminated parts
 *
 * Unlike str_split() in str_split.h which returns borrowed (non-null-terminated)
 * views, this function writes null-terminated copies into a caller-provided
 * scratch buffer. This makes the parts safe to pass to str_join() and str_len().
 *
 * The scratch buffer holds all copied part data contiguously. Parts in
 * out_parts point into scratch_buf — they are valid only as long as
 * scratch_buf is valid and unmodified.
 *
 * @param s           Null-terminated input string
 * @param delim       Single character delimiter
 * @param out_parts   Buffer to store pointers to null-terminated parts
 * @param max_parts   Capacity of out_parts
 * @param scratch_buf Writable buffer to hold copied part data
 * @param scratch_size Size of scratch_buf in bytes
 * @return Number of parts found and written, or 0 on invalid input or
 *         insufficient scratch space
 */
static inline usize str_split_to_parts(
    const char*  s,
    char         delim,
    const char** out_parts,
    usize        max_parts,
    char*        scratch_buf,
    usize        scratch_size
) {
    if (!s || !out_parts || max_parts == 0 || !scratch_buf || scratch_size == 0)
        return 0;

    usize count     = 0;
    usize scratch_pos = 0;
    const char* p   = s;

    while (*p) {
        while (*p == delim) ++p;
        if (!*p) break;

        const char* start = p;
        while (*p && *p != delim) ++p;
        usize part_len = (usize)(p - start);

        /* need part_len + 1 for null terminator in scratch */
        if (scratch_pos + part_len + 1 > scratch_size) return 0;
        if (count >= max_parts) return 0;

        mem_copy(scratch_buf + scratch_pos, start, part_len);
        scratch_buf[scratch_pos + part_len] = '\0';
        out_parts[count] = scratch_buf + scratch_pos;
        scratch_pos += part_len + 1;
        count++;
    }

    return count;
}

/**
 * @brief Splits string by delimiter and rejoins with a new separator
 *
 * Combines splitting and joining in one operation. Parts are null-terminated
 * copies written into scratch_buf before being joined — safe to pass to
 * str_join() internally.
 *
 * @param s           Null-terminated input string
 * @param delim       Original delimiter character
 * @param parts_buf   Temporary buffer for split part pointers
 * @param max_parts   Capacity of parts_buf
 * @param scratch_buf Writable buffer to hold null-terminated part copies
 * @param scratch_size Size of scratch_buf in bytes
 * @param dest        Output buffer for joined result
 * @param dest_size   Size of dest in bytes
 * @param new_sep     New separator string (NULL treated as empty "")
 * @return true if rejoin succeeded and fit in dest
 *         false on invalid input, insufficient scratch, or dest overflow
 */
static inline bool str_rejoin(
    const char*  s,
    char         delim,
    const char** parts_buf,
    usize        max_parts,
    char*        scratch_buf,
    usize        scratch_size,
    char*        dest,
    usize        dest_size,
    const char*  new_sep
) {
    usize count = str_split_to_parts(
        s, delim, parts_buf, max_parts, scratch_buf, scratch_size
    );
    if (count == 0) {
        if (dest && dest_size > 0) dest[0] = '\0';
        return false;
    }
    return str_join(dest, dest_size, parts_buf, count, new_sep);
}

#endif /* CANON_UTIL_STR_JOIN_H */
