// util/string.h
#ifndef CANON_UTIL_STRING_H
#define CANON_UTIL_STRING_H

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "core/memory.h"
#include "semantics/option.h"

/**
 * @file string.h
 * @brief Explicit, safe string utilities with strict ownership rules
 *
 * Provides safe string manipulation functions divided into two categories:
 *   1. **Owned** (allocating) functions — return Option<char*>:
 *      - Caller owns the returned string on Some
 *      - Caller must free it with str_free() or mem_free()
 *   2. **Borrowed** (buffer-based) functions — operate on caller-provided buffers:
 *      - No allocation, no ownership transfer
 *      - Always safe, bounded, and null-safe
 *
 * All functions follow these invariants:
 *   - Null-safe (graceful handling of NULL pointers)
 *   - Bounds-checked (never overflow caller buffers)
 *   - Explicit failure modes (bool return or Option)
 *   - No hidden allocations or global state
 *
 * Usage guidelines:
 *   - Prefer borrowed functions when possible (zero-cost, safer)
 *   - Use allocating functions only when dynamic length is required
 *   - Always check return values (Option or bool) before using results
 *
 * Example (owned):
 *   option_charp s = str_alloc_copy("hello");
 *   if (option_is_some(s)) {
 *       printf("%s\n", option_unwrap(s));
 *       str_free(option_unwrap(s));
 *   }
 *
 * Example (borrowed):
 *   char buf[32];
 *   if (str_copy_into(buf, sizeof(buf), "hello world")) {
 *       printf("%s\n", buf);
 *   }
 */

/* ────────────────────────────────────────────────────────────────────────────
   Owned strings — allocating functions (caller must free)
   ──────────────────────────────────────────────────────────────────────────── */

CANON_C_DEFINE_OPTION(char*)  // option_charp for allocation results

/**
 * @brief Allocates and copies a null-terminated string (caller owns result)
 *
 * @param s Source null-terminated string (may be NULL)
 * @return  Some(owned copy) on success, None on failure (NULL input or alloc fail)
 *
 * Ownership: Caller must eventually call str_free() or mem_free() on Some value
 */
static inline option_charp str_alloc_copy(const char* s) {
    if (!s) return option_charp_none();

    const size_t len = strlen(s);
    char* out = (char*)mem_alloc(len + 1);
    if (!out) return option_charp_none();

    mem_copy(out, s, len + 1);
    return option_charp_some(out);
}

/**
 * @brief Allocates and concatenates two strings (caller owns result)
 *
 * @param a First null-terminated string
 * @param b Second null-terminated string
 * @return  Some(owned concatenation) on success, None on failure
 *
 * Ownership: Caller must free the result on Some
 */
static inline option_charp str_alloc_concat(const char* a, const char* b) {
    if (!a || !b) return option_charp_none();

    const size_t len_a = strlen(a);
    const size_t len_b = strlen(b);
    const size_t total = len_a + len_b;

    char* out = (char*)mem_alloc(total + 1);
    if (!out) return option_charp_none();

    mem_copy(out, a, len_a);
    mem_copy(out + len_a, b, len_b + 1);  // copies null terminator
    return option_charp_some(out);
}

/**
 * @brief Allocates a substring (caller owns result)
 *
 * @param s     Source null-terminated string
 * @param start Starting index (inclusive)
 * @param len   Length of substring (clamped to available space)
 * @return      Some(owned substring) on success, None on failure
 *
 * Clamps len if start + len exceeds source length.
 * Ownership: Caller must free on Some
 */
static inline option_charp str_alloc_sub(const char* s, size_t start, size_t len) {
    if (!s) return option_charp_none();

    const size_t s_len = strlen(s);
    if (start >= s_len) return option_charp_none();

    if (start + len > s_len) len = s_len - start;

    char* out = (char*)mem_alloc(len + 1);
    if (!out) return option_charp_none();

    mem_copy(out, s + start, len);
    out[len] = '\0';
    return option_charp_some(out);
}

/**
 * @brief Explicit free helper for allocated strings
 *
 * @param s Owned string pointer (from str_alloc_* functions)
 *
 * Reminder: Always free owned strings to prevent leaks.
 * Safe to call with NULL.
 */
static inline void str_free(char* s) {
    mem_free(s);
}

/* ────────────────────────────────────────────────────────────────────────────
   Borrowed strings — buffer-based operations (no allocation)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Safely copies source string into fixed destination buffer
 *
 * @param dest      Destination buffer (caller-owned)
 * @param dest_size Size of destination buffer in bytes (including null terminator)
 * @param src       Null-terminated source string
 * @return          true on success, false if truncated or invalid
 */
static inline bool str_copy_into(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0 || !src) return false;

    const size_t len = strlen(src);
    if (len + 1 > dest_size) return false;

    mem_copy(dest, src, len + 1);
    return true;
}

/**
 * @brief Safely concatenates two strings into fixed destination buffer
 *
 * @param dest      Destination buffer
 * @param dest_size Size of destination buffer
 * @param a         First null-terminated string
 * @param b         Second null-terminated string
 * @return          true on success, false if truncated or invalid
 */
static inline bool str_concat_into(char* dest, size_t dest_size, const char* a, const char* b) {
    if (!dest || dest_size == 0 || !a || !b) return false;

    const size_t len_a = strlen(a);
    const size_t len_b = strlen(b);

    if (len_a + len_b + 1 > dest_size) return false;

    mem_copy(dest, a, len_a);
    mem_copy(dest + len_a, b, len_b + 1);
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Common predicates (pure functions, no allocation, no side effects)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if two null-terminated strings are equal
 *
 * Safe against NULL pointers (returns false if either is NULL).
 * Returns true if both are NULL.
 */
static inline bool str_equals(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

/**
 * @brief Checks if string starts with given prefix
 *
 * Safe: returns false on NULL inputs or empty prefix mismatch.
 */
static inline bool str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return false;

    const size_t plen = strlen(prefix);
    return plen == 0 || strncmp(s, prefix, plen) == 0;
}

/**
 * @brief Checks if string ends with given suffix
 *
 * Safe: returns false on NULL inputs or when suffix is longer than string.
 */
static inline bool str_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return false;

    const size_t slen = strlen(s);
    const size_t flen = strlen(suffix);

    if (flen > slen) return false;
    return strcmp(s + slen - flen, suffix) == 0;
}

#endif /* CANON_UTIL_STRING_H */
