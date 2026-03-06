#ifndef CANON_UTIL_STRING_H
#define CANON_UTIL_STRING_H

#include "core/primitives/types.h"       // usize, bool
#include "core/primitives/contract.h"    // require_msg, ensure_msg
#include "core/memory.h"                 // mem_alloc, mem_free, mem_copy, str_len, str_compare, str_ncompare
#include "semantics/option/option.h"  // CANON_OPTION

/**
 * @file util/str/string.h
 * @brief Safe, explicit and ownership-aware string utilities
 *
 * Provides modern string manipulation utilities with clear ownership semantics
 * and explicit allocation boundaries. Implements both heap-allocating and
 * buffer-based operations, along with pure predicate functions for string
 * comparison and checking.
 *
 * Portability:
 * - Requires C99 or later
 * - Depends only on Canon-C core modules (types, memory, primitives, option)
 * - No direct standard library string functions used
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 *
 * Performance:
 * - Time complexity: O(n) where n = string length for most operations
 * - Space complexity: O(1) for borrowed operations, O(n) for owned
 * - Owned operations: one mem_alloc per allocation
 * - Borrowed operations: zero allocations
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
 *
 * @sa util/str/split.h  — non-mutating string splitting
 * @sa util/str/join.h   — string joining
 * @sa util/str/view.h   — immutable borrowed string view
 */

/* ────────────────────────────────────────────────────────────────────────────
   option_charp — canonical owned string Option type
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Typedef for heap-owned char pointer — enables CANON_OPTION instantiation
 *
 * option_charp is the canonical return type for all str_alloc_* functions.
 * It makes non-ownership and allocation explicit at every call site.
 */
typedef char* charp;
CANON_OPTION(charp)

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
 *
 * Performance: O(n)
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
 *
 * Performance: O(n)
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
 *
 * Performance: O(n)
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
 *
 * @param s Pointer to owned string (NULL-safe)
 *
 * Performance: O(1)
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
 * All-or-nothing — copies only if source fits completely (including null terminator).
 *
 * @param dest      Writable destination buffer
 * @param dest_size Size of buffer in bytes (including null terminator)
 * @param src       Null-terminated source string
 * @return true on successful full copy, false if would overflow or invalid input
 *
 * Performance: O(n)
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
 *
 * @param dest      Writable destination buffer
 * @param dest_size Size of buffer in bytes (including null terminator)
 * @param a         First null-terminated string
 * @param b         Second null-terminated string
 * @return true on successful full concatenation, false if would overflow
 *
 * Performance: O(n)
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
 *
 * @param a First string (may be NULL)
 * @param b Second string (may be NULL)
 * @return true if strings are identical (including both NULL), false otherwise
 *
 * Performance: O(n)
 */
static inline bool str_equals(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return str_compare(a, b) == 0;
}

/**
 * @brief Checks if string starts with given prefix
 *
 * @param s      String to check (may be NULL)
 * @param prefix Prefix to look for (may be NULL)
 * @return true if s begins with prefix (empty prefix → always true)
 *
 * Performance: O(n)
 */
static inline bool str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return false;
    const usize prefix_len = str_len(prefix);
    if (prefix_len == 0) return true;
    return str_ncompare(s, prefix, prefix_len) == 0;
}

/**
 * @brief Checks if string ends with given suffix
 *
 * @param s      String to check (may be NULL)
 * @param suffix Suffix to look for (may be NULL)
 * @return true if s ends with suffix, false otherwise
 *
 * Performance: O(n)
 */
static inline bool str_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return false;
    const usize s_len = str_len(s);
    const usize suffix_len = str_len(suffix);
    if (suffix_len > s_len) return false;
    return str_compare(s + s_len - suffix_len, suffix) == 0;
}

#endif /* CANON_UTIL_STRING_H */
