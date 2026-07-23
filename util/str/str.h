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

#ifndef CANON_UTIL_STR_H
#define CANON_UTIL_STR_H

#include <stdbool.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "core/memory.h"
#include "semantics/option/option.h"

/**
 * @file util/str/str.h
 * @brief Safe, explicit and ownership-aware string utilities
 *
 * Provides modern string manipulation utilities with clear ownership semantics
 * and explicit allocation boundaries. Implements both heap-allocating and
 * buffer-based operations, along with pure predicate functions for string
 * comparison and checking.
 *
 * Naming convention:
 * ────────────────────────────────────────────────────────────────────────────
 * Functions in this module operate on null-terminated const char* strings.
 * core/slice.h defines str_starts_with(str_t, str_t) and str_ends_with(str_t, str_t)
 * for slice-based operations. To avoid naming collisions, this module uses
 * cstr_starts_with / cstr_ends_with for the const char* variants.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends only on Canon-C core modules (types, contract, ownership, memory, option)
 * - No direct standard library string functions used
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * Functions that require valid input use require_msg for NULL pointer checks.
 * str_len is the exception — NULL returns 0 by convention (used as a building
 * block everywhere, including places where NULL is a valid sentinel).
 * str_equals also accepts NULL — two NULLs compare equal by design.
 *
 * Ownership model:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. OWNED STRINGS (str_alloc_* functions):
 *    - Allocates on heap via mem_alloc
 *    - Returns option_charp
 *    - Caller OWNS the result
 *    - Caller MUST free with str_free()
 *
 * 2. BORROWED STRINGS (str_*_into functions):
 *    - Caller provides buffer
 *    - Returns bool (success/failure)
 *    - Zero allocations
 */

/* ────────────────────────────────────────────────────────────────────────────
   option_charp — canonical owned string Option type
   ──────────────────────────────────────────────────────────────────────────── */

typedef char* charp;
CANON_OPTION(charp)

/* ────────────────────────────────────────────────────────────────────────────
   Primitive string operations — must be defined before use below
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns length of null-terminated string (excluding null terminator)
 *
 * NULL-safe by design — NULL returns 0. This is intentional: str_len is
 * used as a building block everywhere, including contexts where NULL is
 * a valid sentinel value.
 *
 * @param s Null-terminated string (NULL → 0)
 * @return Number of characters before null terminator
 */
static inline usize str_len(borrowed(const char*) s) {
    if (!s) { return 0; }
    usize len = 0;
    while (s[len] != '\0') { len++; }
    return len;
}

/**
 * @brief Lexicographic comparison of two null-terminated strings
 *
 * @param a First string (must not be NULL — contract)
 * @param b Second string (must not be NULL — contract)
 * @return <0 if a < b, 0 if equal, >0 if a > b
 */
static inline int str_compare(
    borrowed(const char*) a,
    borrowed(const char*) b)
{
    require_msg(a != NULL, "str_compare: a is NULL");
    require_msg(b != NULL, "str_compare: b is NULL");
    const char* pa = a;
    const char* pb = b;
    while (*pa && (*pa == *pb)) { pa++; pb++; }
    return (int)(unsigned char)*pa - (int)(unsigned char)*pb;
}

/**
 * @brief Lexicographic comparison of at most n characters
 *
 * When n == 0, no characters are compared — always returns 0.
 *
 * @param a First string (must not be NULL — contract)
 * @param b Second string (must not be NULL — contract)
 * @param n Maximum number of characters to compare (0 → always equal)
 * @return <0 if a < b, 0 if equal or n == 0, >0 if a > b
 */
static inline int str_ncompare(
    borrowed(const char*) a,
    borrowed(const char*) b,
    usize                 n)
{
    if (n == 0u) { return 0u; }
    require_msg(a != NULL, "str_ncompare: a is NULL");
    require_msg(b != NULL, "str_ncompare: b is NULL");
    const char* pa = a;
    const char* pb = b;
    usize m = n;
    while ((m > 1u) && *pa && (*pa == *pb)) { pa++; pb++; m--; }
    return (int)(unsigned char)*pa - (int)(unsigned char)*pb;
}

/* ────────────────────────────────────────────────────────────────────────────
   Owned strings — heap allocation (caller must free with str_free)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates a new heap copy of a null-terminated string
 *
 * NULL input returns None (not a contract violation) — this function is
 * frequently used at boundaries where NULL is a valid "no string" sentinel.
 *
 * @param s Source null-terminated string (NULL → None)
 * @return Some(owned heap-allocated copy) on success
 *         None on NULL input or allocation failure
 *
 * @remark Caller MUST free with str_free()
 */
static inline option_charp str_alloc_copy(borrowed(const char*) s) {
    if (!s) { return option_charp_none(); }
    const usize len = str_len(s);
    char* copy = (char*)mem_alloc(len + 1u);
    if (!copy) { return option_charp_none(); }
    mem_copy(copy, s, len + 1u);
    return option_charp_some(copy);
}

/**
 * @brief Allocates concatenation of two null-terminated strings
 *
 * @param a First string (must not be NULL — contract)
 * @param b Second string (must not be NULL — contract)
 * @return Some(owned concatenated string) on success
 *         None on allocation failure
 *
 * @remark Caller MUST free with str_free()
 */
static inline option_charp str_alloc_concat(
    borrowed(const char*) a,
    borrowed(const char*) b)
{
    require_msg(a != NULL, "str_alloc_concat: a is NULL");
    require_msg(b != NULL, "str_alloc_concat: b is NULL");
    const usize len_a = str_len(a);
    const usize len_b = str_len(b);
    char* result = (char*)mem_alloc(len_a + len_b + 1u);
    if (!result) { return option_charp_none(); }
    mem_copy(result, a, len_a);
    mem_copy(result + len_a, b, len_b + 1u);
    return option_charp_some(result);
}

/**
 * @brief Allocates a substring from source string
 *
 * @param s     Source null-terminated string (must not be NULL — contract)
 * @param start Starting index (0-based, inclusive)
 * @param len   Desired length (clamped if exceeds remaining string)
 * @return Some(owned substring) on success
 *         None on invalid range or allocation failure
 *
 * @remark Caller MUST free with str_free()
 */
static inline option_charp str_alloc_sub(
    borrowed(const char*) s,
    usize                 start,
    usize                 len)
{
    require_msg(s != NULL, "str_alloc_sub: s is NULL");
    const usize s_len = str_len(s);
    if (start >= s_len) { return option_charp_none(); }
    const usize l = ((start + len) > s_len) ? (s_len - start) : len;
    char* result = (char*)mem_alloc(l + 1u);
    if (!result) { return option_charp_none(); }
    mem_copy(result, s + start, l);
    result[l] = '\0';
    return option_charp_some(result);
}

/**
 * @brief Frees a string allocated by any str_alloc_* function
 *
 * NULL-safe — calling with NULL is a no-op.
 */
static inline void str_free(char* s) {
    mem_free(s);
}

/* ────────────────────────────────────────────────────────────────────────────
   Borrowed strings — zero-allocation, caller provides buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Safely copies source string into fixed-size buffer
 *
 * All-or-nothing — copies only if source fits completely.
 *
 * @param dest      Writable destination buffer (must not be NULL — contract)
 * @param dest_size Size of buffer in bytes (must be > 0)
 * @param src       Null-terminated source string (must not be NULL — contract)
 * @return true on successful full copy, false if would overflow
 */
static inline bool str_copy_into(
    char*                 dest,
    usize                 dest_size,
    borrowed(const char*) src)
{
    require_msg(dest != NULL, "str_copy_into: dest is NULL");
    require_msg(src  != NULL, "str_copy_into: src is NULL");
    if (dest_size == 0u) { return false; }
    const usize src_len = str_len(src);
    if ((src_len + 1u) > dest_size) { return false; }
    mem_copy(dest, src, src_len + 1u);
    return true;
}

/**
 * @brief Safely concatenates two strings into fixed-size buffer
 *
 * All-or-nothing — concatenates only if result fits completely.
 *
 * @param dest      Writable destination buffer (must not be NULL — contract)
 * @param dest_size Size of buffer in bytes (must be > 0)
 * @param a         First null-terminated string (must not be NULL — contract)
 * @param b         Second null-terminated string (must not be NULL — contract)
 * @return true on successful full concatenation, false if would overflow
 */
static inline bool str_concat_into(
    char*                 dest,
    usize                 dest_size,
    borrowed(const char*) a,
    borrowed(const char*) b)
{
    require_msg(dest != NULL, "str_concat_into: dest is NULL");
    require_msg(a    != NULL, "str_concat_into: a is NULL");
    require_msg(b    != NULL, "str_concat_into: b is NULL");
    if (dest_size == 0u) { return false; }
    const usize len_a = str_len(a);
    const usize len_b = str_len(b);
    if ((len_a + len_b + 1u) > dest_size) { return false; }
    mem_copy(dest, a, len_a);
    mem_copy(dest + len_a, b, len_b + 1u);
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Pure predicates — fast, const-correct string checks
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if two null-terminated strings are equal
 *
 * NULL-safe by design — two NULLs compare equal, NULL vs non-NULL is false.
 * This is intentional: str_equals is frequently used in conditionals where
 * one or both sides may be absent.
 */
static inline bool str_equals(
    borrowed(const char*) a,
    borrowed(const char*) b)
{
    if (a == b) { return true; }
    if (!a || !b) { return false; }
    return str_compare(a, b) == 0;
}

/**
 * @brief Checks if null-terminated string starts with given prefix
 *
 * Named cstr_starts_with to avoid collision with str_starts_with(str_t, str_t)
 * defined in core/slice.h for slice-based strings.
 *
 * @param s      String to check (must not be NULL — contract)
 * @param prefix Prefix to look for (must not be NULL — contract)
 * @return true if s begins with prefix (empty prefix → always true)
 */
static inline bool cstr_starts_with(
    borrowed(const char*) s,
    borrowed(const char*) prefix)
{
    require_msg(s      != NULL, "cstr_starts_with: s is NULL");
    require_msg(prefix != NULL, "cstr_starts_with: prefix is NULL");
    const usize prefix_len = str_len(prefix);
    if (prefix_len == 0u) { return true; }
    return str_ncompare(s, prefix, prefix_len) == 0;
}

/**
 * @brief Checks if null-terminated string ends with given suffix
 *
 * Named cstr_ends_with to avoid collision with str_ends_with(str_t, str_t)
 * defined in core/slice.h for slice-based strings.
 *
 * @param s      String to check (must not be NULL — contract)
 * @param suffix Suffix to look for (must not be NULL — contract)
 * @return true if s ends with suffix
 */
static inline bool cstr_ends_with(
    borrowed(const char*) s,
    borrowed(const char*) suffix)
{
    require_msg(s      != NULL, "cstr_ends_with: s is NULL");
    require_msg(suffix != NULL, "cstr_ends_with: suffix is NULL");
    const usize s_len = str_len(s);
    const usize suffix_len = str_len(suffix);
    if (suffix_len > s_len) { return false; }
    return str_compare(s + s_len - suffix_len, suffix) == 0;
}

#endif /* CANON_UTIL_STR_H */
