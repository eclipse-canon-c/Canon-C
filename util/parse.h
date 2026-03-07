#ifndef CANON_UTIL_PARSE_H
#define CANON_UTIL_PARSE_H

#include "core/primitives/types.h"
#include "semantics/result/result.h"
#include "semantics/error.h"
#include <errno.h>
#include <stdlib.h>

/**
 * @file util/parse.h
 * @brief Safe, explicit and composable string-to-value parsing
 *
 * Provides robust string parsing functions for common numeric types with
 * Result-based error handling. Implements zero-allocation, const-correct
 * parsing with explicit error propagation and composable design patterns.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends on Canon-C core modules (types, result, error)
 * - Uses standard strto* functions (strtoll, strtoull, strtod) via <stdlib.h>
 * - errno via <errno.h> — thread-local in C11/POSIX environments
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 * errno is thread-local in modern C (C11/POSIX).
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(n) where n = number of digits/characters parsed
 * - Space complexity: O(1) — zero allocations, stack-only operation
 * - Single pass through input string
 * - Minimal overhead over raw strto* functions (thin wrapper)
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit over implicit — all errors returned as values (Result pattern)
 * - No errno checking required by caller — failures encoded in Result type
 * - Zero allocations — purely computational, no heap involvement
 * - Const-correct — input strings never modified
 * - Prefix parsing — consume only what's valid, leave rest untouched
 * - Composable design — endptr pattern enables chaining multiple parses
 * - Whitespace explicit — caller controls when/if to skip whitespace
 * - Full standard library support — hex (0x), octal (0), scientific notation
 * - Overflow detection — invalid values never silently accepted
 * - Null-safe — graceful handling of NULL/empty inputs
 *
 * Result-based error handling:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions return Result<T, Error>:
 * - Ok(value):              parsing succeeded, value is valid
 * - Err(ERR_PARSE_FAILED):  invalid format, overflow, or empty input
 *
 * Prefix parsing and endptr pattern:
 * ────────────────────────────────────────────────────────────────────────────
 * All parsing functions implement prefix parsing — they consume characters
 * from the beginning until they hit something invalid, then stop.
 *
 * The optional endptr parameter gets updated to point to the first character
 * that was NOT parsed. This enables powerful patterns:
 *
 * 1. CHAINED PARSING:
 * ```c
 * const char* p = "123 456 789";
 * p = parse_skip_ws(p);
 * result_i64_Error r1 = parse_i64(p, &p);
 * p = parse_skip_ws(p);
 * result_i64_Error r2 = parse_i64(p, &p);
 * ```
 *
 * 2. FULL STRING VALIDATION:
 * ```c
 * const char* end;
 * result_i64_Error r = parse_i64(str, &end);
 * if (result_i64_Error_is_ok(r) && *parse_skip_ws(end) == '\0') {
 *     // entire string was a valid number
 * }
 * ```
 *
 * Whitespace handling:
 * ────────────────────────────────────────────────────────────────────────────
 * Functions do NOT automatically skip leading whitespace.
 * Use parse_skip_ws() explicitly when needed.
 *
 * Number format support:
 * ────────────────────────────────────────────────────────────────────────────
 * Integer parsing (parse_i64, parse_u64):
 * - Decimal, hexadecimal (0x), octal (0) via base-0 auto-detection
 * - Signs: + and - accepted for signed; unsigned rejects leading -
 *
 * Floating-point parsing (parse_f64):
 * - Decimal, scientific notation, hex floats (C99)
 * - Special values: inf, -inf, nan
 * - Overflow → ±HUGE_VAL (still Ok), underflow → ±0.0 (still Ok)
 *
 * Canon-C type aliases used:
 * ────────────────────────────────────────────────────────────────────────────
 * - i64  (int64_t)   — signed 64-bit integer
 * - u64  (uint64_t)  — unsigned 64-bit integer
 * - f64  (double)    — 64-bit floating point
 *
 * These aliases are defined in core/primitives/types.h and are used here
 * instead of int64_t/uint64_t/double to satisfy CANON_RESULT name mangling
 * requirements — type names must be valid C identifiers.
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Locale-aware parsing (uses C locale only)
 * - Arbitrary-precision arithmetic
 * - Signal handlers (strtoll/strtod not async-signal-safe)
 */

/* ────────────────────────────────────────────────────────────────────────────
   Result type instantiations
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * i64, u64, f64 are Canon-C type aliases from core/primitives/types.h.
 * They are used instead of int64_t/uint64_t/double because CANON_RESULT
 * uses the type name for name mangling — the names must be valid C identifiers
 * with no spaces or special characters.
 */
CANON_RESULT(i64, Error)
CANON_RESULT(u64, Error)
CANON_RESULT(f64, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Internal helper — whitespace check
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if a character is ASCII whitespace
 *
 * Avoids <ctype.h> and locale dependency.
 * Recognizes: space, tab, newline, carriage return, form feed, vertical tab.
 *
 * @remark Internal — use parse_skip_ws() at call sites.
 */
static inline bool _parse_is_whitespace(char c) {
    switch (c) {
        case ' ': case '\t': case '\n': case '\r':
        case '\f': case '\v':
            return true;
        default:
            return false;
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Whitespace utility
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Advances pointer past leading whitespace characters
 *
 * Does not modify the string — returns a pointer into the original.
 * NULL-safe — returns NULL if input is NULL.
 *
 * @param s Input string (may be NULL)
 * @return Pointer to first non-whitespace character, or to '\0' at end
 */
static inline const char* parse_skip_ws(const char* s) {
    if (!s) return s;
    while (*s && _parse_is_whitespace(*s)) ++s;
    return s;
}

/* ────────────────────────────────────────────────────────────────────────────
   Signed 64-bit integer parsing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses a signed 64-bit integer from the start of a string
 *
 * Supports decimal, hexadecimal (0x/0X), and octal (0) via base-0
 * auto-detection. Does NOT skip leading whitespace — use parse_skip_ws()
 * first if needed.
 *
 * @param s       Null-terminated input string — must not be NULL
 * @param endptr  Optional — set to first unparsed character on return;
 *                set to s on failure
 * @return Ok(i64)              on success
 *         Err(ERR_PARSE_FAILED) on NULL/empty input, invalid format, or overflow
 */
static inline result_i64_Error parse_i64(const char* s, const char** endptr) {
    if (!s || !*s) {
        if (endptr) *endptr = s;
        return result_i64_Error_err(ERR_PARSE_FAILED);
    }

    char* eptr;
    errno = 0;
    i64 val = (i64)strtoll(s, &eptr, 0);

    if (eptr == s || errno == ERANGE) {
        if (endptr) *endptr = s;
        return result_i64_Error_err(ERR_PARSE_FAILED);
    }

    if (endptr) *endptr = eptr;
    return result_i64_Error_ok(val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Unsigned 64-bit integer parsing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses an unsigned 64-bit integer from the start of a string
 *
 * Supports decimal, hexadecimal (0x/0X), and octal (0) via base-0
 * auto-detection. Explicitly rejects a leading '-' sign — negative values
 * are never silently wrapped.
 *
 * @param s       Null-terminated input string — must not be NULL
 * @param endptr  Optional — set to first unparsed character on return;
 *                set to s on failure
 * @return Ok(u64)              on success
 *         Err(ERR_PARSE_FAILED) on NULL/empty input, negative sign,
 *                               invalid format, or overflow
 */
static inline result_u64_Error parse_u64(const char* s, const char** endptr) {
    if (!s || !*s) {
        if (endptr) *endptr = s;
        return result_u64_Error_err(ERR_PARSE_FAILED);
    }

    if (*s == '-') {
        if (endptr) *endptr = s;
        return result_u64_Error_err(ERR_PARSE_FAILED);
    }

    char* eptr;
    errno = 0;
    u64 val = (u64)strtoull(s, &eptr, 0);

    if (eptr == s || errno == ERANGE) {
        if (endptr) *endptr = s;
        return result_u64_Error_err(ERR_PARSE_FAILED);
    }

    if (endptr) *endptr = eptr;
    return result_u64_Error_ok(val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Double-precision floating-point parsing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses a double-precision float from the start of a string
 *
 * Supports decimal notation, scientific notation (e/E), hex floats (0x, C99),
 * and special values (inf, -inf, nan). Overflow → ±HUGE_VAL (still Ok).
 * Underflow → ±0.0 (still Ok). Only returns Err if no valid prefix found.
 *
 * @param s       Null-terminated input string — must not be NULL
 * @param endptr  Optional — set to first unparsed character on return;
 *                set to s on failure
 * @return Ok(f64)              on success
 *         Err(ERR_PARSE_FAILED) on NULL/empty input or no valid prefix
 */
static inline result_f64_Error parse_f64(const char* s, const char** endptr) {
    if (!s || !*s) {
        if (endptr) *endptr = s;
        return result_f64_Error_err(ERR_PARSE_FAILED);
    }

    char* eptr;
    f64 val = (f64)strtod(s, &eptr);

    if (eptr == s) {
        if (endptr) *endptr = s;
        return result_f64_Error_err(ERR_PARSE_FAILED);
    }

    if (endptr) *endptr = eptr;
    return result_f64_Error_ok(val);
}

#endif /* CANON_UTIL_PARSE_H */
