// util/parse.h
#ifndef CANON_UTIL_PARSE_H
#define CANON_UTIL_PARSE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "semantics/result.h"
#include "semantics/error.h"

/**
 * @file parse.h
 * @brief Safe, explicit and composable string-to-value parsing
 *
 * Provides robust, zero-allocation parsing functions for common numeric types  
 * with modern Result-based error handling.
 *
 * All functions:
 * - Are fully const-correct (never modify input)
 * - Parse only a prefix of the input string
 * - Optionally advance an end pointer (perfect for chained/tolerant parsing)
 * - Return Result<T, Error> — explicit success/failure
 * - Use standard libc functions under the hood (strtoll, strtoull, strtod)
 * - Detect all failure cases cleanly (no errno dependency)
 * - Have predictable, safe behavior on empty/invalid input
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero hidden state, zero allocations
 * - Maximum composability (Result + endptr pattern)
 * - Full control: caller decides how much of string to consume
 * - Clear distinction between parse failure and overflow
 * - Consistent error reporting via Error enum
 * - Helper for whitespace skipping (common pre-parse step)
 *
 * Typical usage patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Simple single value:
 *    result_int64_t_Error r = parse_int64(" -42 ", NULL);
 *
 * 2. Chained parsing (CSV-like, config lines):
 *    const char* p = line;
 *    p = parse_skip_ws(p);
 *    result_int64_t_Error id_r = parse_int64(p, &p);
 *    p = parse_skip_ws(p);
 *    result_double_Error value_r = parse_double(p, &p);
 *
 * 3. Tolerant parsing with fallback:
 *    result_uint64_t_Error r = parse_uint64(s, NULL);
 *    uint64_t port = result_unwrap_or(r, 8080);
 */

/**
 * @brief Result type aliases for parsed numeric values
 */
CANON_C_DEFINE_RESULT(int64_t,  Error)
CANON_C_DEFINE_RESULT(uint64_t, Error)
CANON_C_DEFINE_RESULT(double,   Error)

/* ────────────────────────────────────────────────────────────────────────────
   Signed 64-bit integer parsing (decimal, hex 0x, octal 0)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses a signed 64-bit integer from the beginning of a string
 *
 * @param s      Null-terminated input string (may be empty)
 * @param endptr If non-NULL, set to point to the first character after the number
 *
 * @return Ok(parsed int64_t) on success  
 *         Err(ERR_PARSE_FAILED) on failure (no digits, invalid format, overflow)
 *
 * Supported formats:
 * - Decimal:         12345, -42
 * - Hexadecimal:     0x1a2b, 0X7FFF
 * - Octal:           0777, 0123
 * - Leading sign:    +42, -999
 *
 * Notes:
 * - Does **not** skip leading whitespace (use parse_skip_ws first)
 * - Overflow → parse failure (ERR_PARSE_FAILED)
 * - Empty string / no valid number → failure
 * - endptr == s on failure
 *
 * Example:
 * ```c
 * const char* s = " -1234abc";
 * const char* end;
 * result_int64_t_Error r = parse_int64(s, &end);
 * if (result_is_ok(r)) {
 *     printf("Value: %lld, remaining: '%s'\n",
 *            result_unwrap(r), end);  // → Value: -1234, remaining: 'abc'
 * }
 * ```
 */
static inline result_int64_t_Error parse_int64(
    const char* s,
    const char** endptr
) {
    if (!s || !*s) {
        return RESULT_ERR(int64_t, ERR_PARSE_FAILED);
    }

    char* eptr;
    errno = 0;  // Required for reliable overflow detection
    int64_t val = strtoll(s, &eptr, 0);  // base 0 = auto (dec/hex/oct)

    if (eptr == s || errno == ERANGE) {
        return RESULT_ERR(int64_t, ERR_PARSE_FAILED);
    }

    if (endptr) {
        *endptr = eptr;
    }

    return RESULT_OK(int64_t, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Unsigned 64-bit integer parsing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses an unsigned 64-bit integer from string prefix
 *
 * @param s      Input string
 * @param endptr Optional output pointer to first unparsed character
 *
 * @return Ok(parsed uint64_t) on success  
 *         Err(ERR_PARSE_FAILED) on failure (invalid, overflow, negative)
 *
 * Behavior identical to parse_int64 except:
 * - No sign allowed (negative values fail)
 * - Larger range (up to 2⁶⁴-1)
 */
static inline result_uint64_t_Error parse_uint64(
    const char* s,
    const char** endptr
) {
    if (!s || !*s) {
        return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);
    }

    char* eptr;
    errno = 0;
    uint64_t val = strtoull(s, &eptr, 0);

    if (eptr == s || errno == ERANGE) {
        return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);
    }

    if (endptr) {
        *endptr = eptr;
    }

    return RESULT_OK(uint64_t, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Double-precision floating-point parsing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses a double from string prefix (standard floating-point syntax)
 *
 * @param s      Input string
 * @param endptr Optional pointer updated to first non-parsed character
 *
 * @return Ok(parsed double) on success  
 *         Err(ERR_PARSE_FAILED) on failure (no valid number)
 *
 * Supported formats:
 * - Plain decimal:    3.14159, -0.001
 * - Scientific:       1.23e-4, -4.56E+02
 * - Hex float (C99):  0x1.8p-3 (rarely used)
 *
 * Notes:
 * - Does **not** skip whitespace automatically
 * - Overflow/underflow → usually returns ±HUGE_VAL (still success)
 * - Only fails if no valid number was found at all
 */
static inline result_double_Error parse_double(
    const char* s,
    const char** endptr
) {
    if (!s || !*s) {
        return RESULT_ERR(double, ERR_PARSE_FAILED);
    }

    char* eptr;
    double val = strtod(s, &eptr);

    if (eptr == s) {
        return RESULT_ERR(double, ERR_PARSE_FAILED);
    }

    if (endptr) {
        *endptr = eptr;
    }

    return RESULT_OK(double, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Parsing helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Skips leading whitespace characters
 *
 * @param s Input string (may be NULL)
 * @return Pointer to first non-whitespace character  
 *         or end of string if all whitespace or NULL input
 *
 * Uses isspace() with proper unsigned char cast — safe & standard.
 *
 * Typical usage:
 * ```c
 * const char* p = "   42  hello";
 * p = parse_skip_ws(p);           // → "42  hello"
 * result_int64_t_Error r = parse_int64(p, &p);
 * // r = Ok(42), p → "  hello"
 * ```
 */
static inline const char* parse_skip_ws(const char* s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    return s;
}

#endif /* CANON_UTIL_PARSE_H */
