// util/string.h
#ifndef CANON_UTIL_STRING_H
#define CANON_UTIL_STRING_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "core/memory.h"
#include "semantics/option.h"

/**
 * @file string.h
 * @brief Safe, explicit and ownership-aware string utilities
 *
 * Provides modern, safe string manipulation with strict separation between:
 * - **Owned** (allocating) operations → return Option<char*>, caller must free
 * - **Borrowed** (buffer-based) operations → zero allocation, caller provides storage
 * - **Pure predicates** → fast, const-correct comparisons & checks
 *
 * Core design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - **Null-safe** — graceful handling of NULL pointers (no crashes)
 * - **Bounds-safe** — never overflow caller-provided buffers
 * - **Explicit ownership** — allocating functions return Option, free required
 * - **Zero hidden state** — no globals, no thread-locals
 * - **Predictable failure** — clear success/failure via bool or Option
 * - **Minimal & composable** — works well with str_join.h, parse.h, etc.
 *
 * Usage recommendations:
 * ────────────────────────────────────────────────────────────────────────────
 * - Prefer **borrowed** functions (str_copy_into, str_concat_into) for performance & safety
 * - Use **owned** functions (str_alloc_*) only when dynamic sizing is truly needed
 * - Always check Option or bool return value before dereferencing/using result
 * - Free every owned string exactly once (str_free or mem_free)
 *
 * Example patterns:
 * ```c
 * // Owned (dynamic)
 * option_charp greeting = str_alloc_concat("Hello, ", name);
 * if (option_charp_is_some(greeting)) {
 *     printf("%s\n", option_charp_unwrap(greeting));
 *     str_free(option_charp_unwrap(greeting));
 * }
 *
 * // Borrowed (fixed-size, zero-allocation)
 * char buf[128];
 * if (str_copy_into(buf, sizeof(buf), "Status: OK")) {
 *     printf("%s\n", buf);
 * }
 * ```
 */

/**
 * @brief Option alias for owned (heap-allocated) strings
 */
CANON_C_DEFINE_OPTION(char*)  // → option_charp

/* ────────────────────────────────────────────────────────────────────────────
   Owned strings — heap allocation (caller must free)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates a new copy of a null-terminated string
 *
 * @param s Source string (may be NULL)
 * @return Some(owned heap copy) on success  
 *         None on NULL input or allocation failure
 *
 * Ownership: On Some, caller **must** call str_free() or mem_free()
 */
static inline option_charp str_alloc_copy(const char* s) {
    if (!s) {
        return option_charp_none();
    }

    const size_t len = strlen(s);
    char* copy = (char*)mem_alloc(len + 1);
    if (!copy) {
        return option_charp_none();
    }

    mem_copy(copy, s, len + 1);
    return option_charp_some(copy);
}

/**
 * @brief Allocates concatenation of two null-terminated strings
 *
 * @param a First string
 * @param b Second string
 * @return Some(owned concatenation) on success  
 *         None on NULL input or allocation failure
 *
 * Ownership: Caller must free on Some
 */
static inline option_charp str_alloc_concat(const char* a, const char* b) {
    if (!a || !b) {
        return option_charp_none();
    }

    const size_t len_a = strlen(a);
    const size_t len_b = strlen(b);
    const size_t total = len_a + len_b;

    char* result = (char*)mem_alloc(total + 1);
    if (!result) {
        return option_charp_none();
    }

    mem_copy(result, a, len_a);
    mem_copy(result + len_a, b, len_b + 1);  // includes null terminator

    return option_charp_some(result);
}

/**
 * @brief Allocates a substring from source
 *
 * @param s     Source null-terminated string
 * @param start Starting index (0-based, inclusive)
 * @param len   Desired length (clamped if exceeds source)
 * @return Some(owned substring) on success  
 *         None on NULL source, invalid range or alloc failure
 *
 * Clamping: if start + len > strlen(s), takes up to end of string
 */
static inline option_charp str_alloc_sub(
    const char* s,
    size_t start,
    size_t len
) {
    if (!s) {
        return option_charp_none();
    }

    const size_t s_len = strlen(s);
    if (start >= s_len) {
        return option_charp_none();
    }

    if (start + len > s_len) {
        len = s_len - start;
    }

    char* result = (char*)mem_alloc(len + 1);
    if (!result) {
        return option_charp_none();
    }

    mem_copy(result, s + start, len);
    result[len] = '\0';

    return option_charp_some(result);
}

/**
 * @brief Explicit free function for strings allocated by str_alloc_* functions
 *
 * @param s Pointer to owned string (NULL-safe)
 *
 * Safe to call multiple times or with NULL.
 * Simply forwards to mem_free().
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
 * @param dest      Writable destination buffer
 * @param dest_size Size of buffer (including space for null terminator)
 * @param src       Null-terminated source string
 * @return true on successful full copy  
 *         false if truncated, NULL input, or zero-size buffer
 *
 * Always null-terminates (even on truncation — but truncation = failure)
 */
static inline bool str_copy_into(
    char* dest,
    size_t dest_size,
    const char* src
) {
    if (!dest || dest_size == 0 || !src) {
        return false;
    }

    const size_t src_len = strlen(src);
    if (src_len + 1 > dest_size) {
        return false;
    }

    mem_copy(dest, src, src_len + 1);
    return true;
}

/**
 * @brief Safely concatenates two strings into fixed-size buffer
 *
 * @param dest      Destination buffer
 * @param dest_size Size of buffer (incl. null terminator space)
 * @param a         First null-terminated string
 * @param b         Second null-terminated string
 * @return true on successful full concatenation  
 *         false if would overflow, NULL input, or zero-size buffer
 */
static inline bool str_concat_into(
    char* dest,
    size_t dest_size,
    const char* a,
    const char* b
) {
    if (!dest || dest_size == 0 || !a || !b) {
        return false;
    }

    const size_t len_a = strlen(a);
    const size_t len_b = strlen(b);

    if (len_a + len_b + 1 > dest_size) {
        return false;
    }

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
 * @return true if strings are identical (including both NULL)  
 *         false otherwise
 *
 * Null-safe: NULL vs NULL → true, NULL vs non-NULL → false
 */
static inline bool str_equals(const char* a, const char* b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return false;
    }
    return strcmp(a, b) == 0;
}

/**
 * @brief Checks if string starts with given prefix
 *
 * @return true if s begins with prefix (empty prefix → always true)  
 *         false otherwise
 *
 * Null-safe: returns false if either pointer is NULL
 */
static inline bool str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) {
        return false;
    }

    const size_t prefix_len = strlen(prefix);
    return prefix_len == 0 || strncmp(s, prefix, prefix_len) == 0;
}

/**
 * @brief Checks if string ends with given suffix
 *
 * @return true if s ends with suffix  
 *         false otherwise (including if suffix longer than s)
 *
 * Null-safe: returns false if either pointer is NULL
 */
static inline bool str_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) {
        return false;
    }

    const size_t s_len = strlen(s);
    const size_t suffix_len = strlen(suffix);

    if (suffix_len > s_len) {
        return false;
    }

    return strcmp(s + s_len - suffix_len, suffix) == 0;
}

#endif /* CANON_UTIL_STRING_H */
