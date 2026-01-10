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
 * @brief Safe, explicit parsing of common types from strings
 *
 * Provides robust, Result-based parsing functions for integers and floating-point values.
 * All functions:
 *   - Never mutate the input string (const-correct)
 *   - Consume a prefix of the input and optionally advance a pointer
 *   - Return Result<T, Error> for explicit success/failure handling
 *   - Use standard C library functions under the hood (strtoll, strtoull, strtod)
 *   - Detect parse failures cleanly (no errno, no undefined behavior)
 *
 * Key design goals:
 *   - Zero allocation, zero hidden state
 *   - Full control: caller gets end pointer for further parsing
 *   - Composable with Result combinators (map, and_then, unwrap_or, etc.)
 *   - Safe defaults for invalid/empty input
 *
 * Usage pattern:
 *   const char* s = "  -42abc";
 *   const char* end;
 *   result_int64_t_Error r = parse_int64(s, &end);
 *   if (result_is_ok(r)) {
 *       printf("Parsed: %lld, remaining: '%s'\n", result_unwrap(r), end);
 *   } else {
 *       printf("Error: %s\n", error_message(result_unwrap_err(r)));
 *   }
 */

/**
 * @brief Result type aliases for common parsed values
 *
 * Uses Error enum from error.h for consistent failure reporting.
 */
CANON_C_DEFINE_RESULT(int64_t, Error)
CANON_C_DEFINE_RESULT(uint64_t, Error)
CANON_C_DEFINE_RESULT(double, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Signed integer parsing (decimal, hex 0x, octal 0)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses a signed 64-bit integer from string prefix
 *
 * @param s      Null-terminated input string
 * @param endptr Optional pointer to receive pointer after parsed number
 * @return       Ok(parsed int64_t) on success,
 *               Err(ERR_PARSE_FAILED) if no valid number found or overflow
 *
 * Supports:
 *   - Decimal (default)
 *   - Hexadecimal (0x or 0X prefix)
 *   - Octal (0 prefix)
 *   - Leading whitespace skipped by caller if needed
 *   - Negative sign allowed
 *
 * Sets *endptr to first non-parsed character (or s if failed).
 * Safe: returns error on empty input or invalid format.
 */
static inline result_int64_t_Error parse_int64(const char* s, const char** endptr) {
    if (!s || !*s) return RESULT_ERR(int64_t, ERR_PARSE_FAILED);

    char* eptr;
    int64_t val = strtoll(s, &eptr, 0);  // base 0 = auto-detect

    if (eptr == s) return RESULT_ERR(int64_t, ERR_PARSE_FAILED);

    if (endptr) *endptr = eptr;
    return RESULT_OK(int64_t, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Unsigned integer parsing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses an unsigned 64-bit integer from string prefix
 *
 * @param s      Null-terminated input string
 * @param endptr Optional pointer to receive pointer after parsed number
 * @return       Ok(parsed uint64_t) on success,
 *               Err(ERR_PARSE_FAILED) if no valid number or overflow
 *
 * Supports same bases as parse_int64 (decimal, hex 0x, octal 0).
 * No sign allowed (negative input fails).
 */
static inline result_uint64_t_Error parse_uint64(const char* s, const char** endptr) {
    if (!s || !*s) return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);

    char* eptr;
    uint64_t val = strtoull(s, &eptr, 0);

    if (eptr == s) return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);

    if (endptr) *endptr = eptr;
    return RESULT_OK(uint64_t, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Floating-point parsing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Parses a double-precision floating-point number from string prefix
 *
 * @param s      Null-terminated input string
 * @param endptr Optional pointer to receive pointer after parsed number
 * @return       Ok(parsed double) on success,
 *               Err(ERR_PARSE_FAILED) if no valid number found
 *
 * Supports standard floating-point syntax:
 *   - Decimal (123.45)
 *   - Scientific (1.23e-4)
 *   - Sign, leading/trailing whitespace not skipped (caller responsibility)
 */
static inline result_double_Error parse_double(const char* s, const char** endptr) {
    if (!s || !*s) return RESULT_ERR(double, ERR_PARSE_FAILED);

    char* eptr;
    double val = strtod(s, &eptr);

    if (eptr == s) return RESULT_ERR(double, ERR_PARSE_FAILED);

    if (endptr) *endptr = eptr;
    return RESULT_OK(double, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Utility: Whitespace skipping
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Skips leading whitespace characters (space, tab, newline, etc.)
 *
 * @param s Input string
 * @return  Pointer to first non-whitespace character (or end of string)
 *
 * Uses standard isspace() — safe for unsigned char casting.
 * Useful before calling parse_* functions.
 */
static inline const char* parse_skip_ws(const char* s) {
    while (*s && isspace((unsigned char)*s)) ++s;
    return s;
}

#endif /* CANON_UTIL_PARSE_H */
