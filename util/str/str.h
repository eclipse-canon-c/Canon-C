#ifndef CANON_UTIL_STR_H
#define CANON_UTIL_STR_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
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
 * - Depends only on Canon-C core modules (types, contract, memory, option)
 * - No direct standard library string functions used
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
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
 * @param s Null-terminated string (NULL → 0)
 * @return Number of characters before null terminator
 */
static inline usize str_len(const char* s) {
    if (!s) return 0;
    usize len = 0;
    while (s[len]) len++;
    return len;
}

/**
 * @brief Lexicographic comparison of two null-terminated strings
 *
 * @param a First string (must not be NULL)
 * @param b Second string (must not be NULL)
 * @return <0 if a < b, 0 if equal, >0 if a > b
 */
static inline int str_compare(const char* a, const char* b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/**
 * @brief Lexicographic comparison of at most n characters
 *
 * @param a First string (must not be NULL)
 * @param b Second string (must not be NULL)
 * @param n Maximum number of characters to compare
 * @return <0 if a < b, 0 if equal, >0 if a > b
 */
static inline int str_ncompare(const char* a, const char* b, usize n) {
    if (!a || !b || n == 0) return (!a && !b) ? 0 : (a ? 1 : -1);
    while (n > 0 && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* ────────────────────────────────────────────────────────────────────────────
   Owned strings — heap allocation (caller must free with str_free)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates a new heap copy of a null-terminated string
 *
 * @param s Source null-terminated string (may be NULL)
 * @return Some(owned heap-allocated copy) on success
 *         None on NULL input or allocation failure
 *
 * @remark Caller MUST free with str_free()
 */
static inline option_charp str_alloc_copy(const char* s) {
    if (!s) return option_charp_none();
    const usize len = str_len(s);
    char* copy = (char*)mem_alloc(len + 1);
    if (!copy) return option_charp_none();
    mem_copy(copy, s, len + 1);
    return option_charp_some(copy);
}

/**
 * @brief Allocates concatenation of two null-terminated strings
 *
 * @param a First string (must not be NULL)
 * @param b Second string (must not be NULL)
 * @return Some(owned concatenated string) on success
 *         None on NULL input or allocation failure
 *
 * @remark Caller MUST free with str_free()
 */
static inline option_charp str_alloc_concat(const char* a, const char* b) {
    if (!a || !b) return option_charp_none();
    const usize len_a = str_len(a);
    const usize len_b = str_len(b);
    char* result = (char*)mem_alloc(len_a + len_b + 1);
    if (!result) return option_charp_none();
    mem_copy(result, a, len_a);
    mem_copy(result + len_a, b, len_b + 1);
    return option_charp_some(result);
}

/**
 * @brief Allocates a substring from source string
 *
 * @param s     Source null-terminated string (must not be NULL)
 * @param start Starting index (0-based, inclusive)
 * @param len   Desired length (clamped if exceeds remaining string)
 * @return Some(owned substring) on success
 *         None on NULL input, invalid range, or allocation failure
 *
 * @remark Caller MUST free with str_free()
 */
static inline option_charp str_alloc_sub(const char* s, usize start, usize len) {
    if (!s) return option_charp_none();
    const usize s_len = str_len(s);
    if (start >= s_len) return option_charp_none();
    if (start + len > s_len) len = s_len - start;
    char* result = (char*)mem_alloc(len + 1);
    if (!result) return option_charp_none();
    mem_copy(result, s + start, len);
    result[len] = '\0';
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
 */
static inline bool str_copy_into(char* dest, usize dest_size, const char* src) {
    if (!dest || dest_size == 0 || !src) return false;
    const usize src_len = str_len(src);
    if (src_len + 1 > dest_size) return false;
    mem_copy(dest, src, src_len + 1);
    return true;
}

/**
 * @brief Safely concatenates two strings into fixed-size buffer
 *
 * All-or-nothing — concatenates only if result fits completely.
 */
static inline bool str_concat_into(char* dest, usize dest_size, const char* a, const char* b) {
    if (!dest || dest_size == 0 || !a || !b) return false;
    const usize len_a = str_len(a);
    const usize len_b = str_len(b);
    if (len_a + len_b + 1 > dest_size) return false;
    mem_copy(dest, a, len_a);
    mem_copy(dest + len_a, b, len_b + 1);
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Pure predicates — fast, const-correct string checks
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if two null-terminated strings are equal
 */
static inline bool str_equals(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return str_compare(a, b) == 0;
}

/**
 * @brief Checks if null-terminated string starts with given prefix
 *
 * Named cstr_starts_with to avoid collision with str_starts_with(str_t, str_t)
 * defined in core/slice.h for slice-based strings.
 */
static inline bool cstr_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return false;
    const usize prefix_len = str_len(prefix);
    if (prefix_len == 0) return true;
    return str_ncompare(s, prefix, prefix_len) == 0;
}

/**
 * @brief Checks if null-terminated string ends with given suffix
 *
 * Named cstr_ends_with to avoid collision with str_ends_with(str_t, str_t)
 * defined in core/slice.h for slice-based strings.
 */
static inline bool cstr_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return false;
    const usize s_len = str_len(s);
    const usize suffix_len = str_len(suffix);
    if (suffix_len > s_len) return false;
    return str_compare(s + s_len - suffix_len, suffix) == 0;
}

#endif /* CANON_UTIL_STR_H */
