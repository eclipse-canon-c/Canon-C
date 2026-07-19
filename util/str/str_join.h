/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_UTIL_STR_JOIN_H
#define CANON_UTIL_STR_JOIN_H

#include <stdbool.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "core/memory.h"
#include "semantics/option/option.h"
#include "util/str/str.h"

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
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends only on Canon-C core, semantic and util modules
 * - No direct standard library string functions used
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * - dest must not be NULL — violated → contract abort
 * - dest_size must be > 0 — violated → contract abort
 * - parts array must not be NULL — violated → contract abort
 * - NULL element inside parts[] is a data error → returns false/None
 * - NULL separator is valid → treated as empty string ""
 * - count == 0 is valid → produces empty string ""
 * - Buffer too small is a data error → returns false/None
 *
 * Joining behavior:
 * ────────────────────────────────────────────────────────────────────────────
 * Pattern: parts[0] + sep + parts[1] + sep + ... + parts[n-1]
 *
 * - Separator appears BETWEEN elements only
 * - NULL separator → simple concatenation with no delimiter
 * - Empty strings in parts[] → preserved
 * - Zero count → produces empty string ""
 * - NULL in parts[] → error (returns false/None)
 *
 * Buffer sizing:
 * ────────────────────────────────────────────────────────────────────────────
 * Required size = sum(str_len(parts[i])) + (count-1) * str_len(sep) + 1
 *
 * str_split_to_parts behavior:
 * ────────────────────────────────────────────────────────────────────────────
 * Consecutive delimiters are treated as a single separator — empty parts
 * between consecutive delimiters are skipped. Leading and trailing
 * delimiters are also skipped.
 *
 * @sa util/str/str.h       — option_charp, str_alloc_copy, str_free
 * @sa util/str/str_split.h — splitting utilities (borrowed views)
 */

/* ────────────────────────────────────────────────────────────────────────────
   Zero-allocation join — caller provides buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Joins array of null-terminated strings into caller-provided buffer
 *
 * All-or-nothing — on failure sets dest[0] = '\0' and returns false.
 *
 * @param dest      Writable destination buffer (must not be NULL — contract)
 * @param dest_size Size of dest buffer in bytes (must be > 0 — contract)
 * @param parts     Array of null-terminated strings (must not be NULL — contract)
 * @param count     Number of strings in parts (0 → empty string)
 * @param sep       Separator string (NULL treated as empty "")
 * @return true if join succeeded, false if buffer too small or NULL element
 */
static inline bool str_join(
    char*                          dest,
    usize                          dest_size,
    borrowed(const char* const*)   parts,
    usize                          count,
    borrowed(const char*)          sep)
{
    usize i;
    usize pos;
    usize sep_len;

    require_msg(dest      != NULL, "str_join: dest is NULL");
    require_msg(dest_size  > 0u,    "str_join: dest_size is 0");

    if (count == 0u) {
        dest[0] = '\0';
        return true;
    }

    require_msg(parts != NULL, "str_join: parts is NULL");

    sep = sep ? sep : "";
    sep_len = str_len(sep);
    pos = 0;

    for (i = 0; i < count; ++i) {
        usize part_len;
        usize needed;

        if (!parts[i]) {
            dest[0] = '\0';
            return false;
        }

        part_len = str_len(parts[i]);
        needed = part_len + 1u;
        if (i + 1u < count && sep_len > 0u) needed += sep_len;

        if (pos + needed > dest_size) {
            dest[0] = '\0';
            return false;
        }

        mem_copy(dest + pos, parts[i], part_len);
        pos += part_len;

        if (i + 1u < count && sep_len > 0u) {
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
 * Caller MUST free the result with str_free().
 *
 * @param parts Array of null-terminated strings (must not be NULL — contract)
 * @param count Number of strings to join (0 → allocated empty string)
 * @param sep   Separator string (NULL treated as empty "")
 * @return Some(heap-allocated joined string) on success
 *         None on NULL element in parts, or allocation failure
 */
static inline option_charp str_alloc_join(
    borrowed(const char* const*)  parts,
    usize                         count,
    borrowed(const char*)         sep)
{
    usize i;
    usize total_len;
    usize sep_len;
    char* buffer;

    if (count == 0u) return str_alloc_copy("");

    require_msg(parts != NULL, "str_alloc_join: parts is NULL");

    sep = sep ? sep : "";
    sep_len = str_len(sep);

    total_len = 1; /* null terminator */
    for (i = 0; i < count; ++i) {
        if (!parts[i]) return option_charp_none();
        total_len += str_len(parts[i]);
        if (i + 1u < count && sep_len > 0u) total_len += sep_len;
    }

    buffer = (char*)mem_alloc(total_len);
    if (!buffer) return option_charp_none();

    if (str_join(buffer, total_len, parts, count, sep)) {
        return option_charp_some(buffer);
    }

    mem_free(buffer);
    return option_charp_none();
}

/* ────────────────────────────────────────────────────────────────────────────
   Split + rejoin helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits a string by delimiter into null-terminated parts in scratch buffer
 *
 * Consecutive delimiters are treated as a single separator — empty parts
 * between consecutive delimiters are skipped. Leading and trailing
 * delimiters are also skipped.
 *
 * Parts in out_parts point into scratch_buf — valid only as long as
 * scratch_buf is valid and unmodified.
 *
 * @param s           Null-terminated input string (must not be NULL — contract)
 * @param delim       Single character delimiter
 * @param out_parts   Buffer to store part pointers (must not be NULL — contract)
 * @param max_parts   Capacity of out_parts (must be > 0)
 * @param scratch_buf Writable buffer for part data (must not be NULL — contract)
 * @param scratch_size Size of scratch_buf in bytes (must be > 0)
 * @return Number of parts found, or 0 if insufficient scratch space
 */
static inline usize str_split_to_parts(
    borrowed(const char*)  s,
    char                   delim,
    const char**           out_parts,
    usize                  max_parts,
    char*                  scratch_buf,
    usize                  scratch_size)
{
    usize count;
    usize scratch_pos;
    const char* p;

    require_msg(s           != NULL, "str_split_to_parts: s is NULL");
    require_msg(out_parts   != NULL, "str_split_to_parts: out_parts is NULL");
    require_msg(scratch_buf != NULL, "str_split_to_parts: scratch_buf is NULL");

    if (max_parts == 0u || scratch_size == 0u) return 0u;

    count       = 0;
    scratch_pos = 0;
    p           = s;

    while (*p) {
        const char* start;
        usize part_len;

        while (*p == delim) ++p;
        if (!*p) break;

        start = p;
        while (*p && *p != delim) ++p;
        part_len = (usize)(p - start);

        if (scratch_pos + part_len + 1u > scratch_size) return 0u;
        if (count >= max_parts) return 0;

        mem_copy(scratch_buf + scratch_pos, start, part_len);
        scratch_buf[scratch_pos + part_len] = '\0';
        out_parts[count] = scratch_buf + scratch_pos;
        scratch_pos += part_len + 1u;
        count++;
    }

    return count;
}

/**
 * @brief Splits string by delimiter and rejoins with a new separator
 *
 * @param s            Null-terminated input (must not be NULL — contract)
 * @param delim        Original delimiter character
 * @param parts_buf    Temporary buffer for part pointers (must not be NULL — contract)
 * @param max_parts    Capacity of parts_buf
 * @param scratch_buf  Buffer for null-terminated part copies (must not be NULL — contract)
 * @param scratch_size Size of scratch_buf in bytes
 * @param dest         Output buffer for joined result (must not be NULL — contract)
 * @param dest_size    Size of dest in bytes (must be > 0 — contract)
 * @param new_sep      New separator string (NULL treated as empty "")
 * @return true if rejoin succeeded, false on insufficient space
 */
static inline bool str_rejoin(
    borrowed(const char*)  s,
    char                   delim,
    const char**           parts_buf,
    usize                  max_parts,
    char*                  scratch_buf,
    usize                  scratch_size,
    char*                  dest,
    usize                  dest_size,
    borrowed(const char*)  new_sep)
{
    usize count;

    require_msg(s           != NULL, "str_rejoin: s is NULL");
    require_msg(parts_buf   != NULL, "str_rejoin: parts_buf is NULL");
    require_msg(scratch_buf != NULL, "str_rejoin: scratch_buf is NULL");
    require_msg(dest        != NULL, "str_rejoin: dest is NULL");
    require_msg(dest_size    > 0u,    "str_rejoin: dest_size is 0");

    count = str_split_to_parts(
        s, delim, parts_buf, max_parts, scratch_buf, scratch_size
    );

    if (count == 0u) {
        dest[0] = '\0';
        return false;
    }

    return str_join(dest, dest_size, parts_buf, count, new_sep);
}

#endif /* CANON_UTIL_STR_JOIN_H */
