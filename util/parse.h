#ifndef CANON_UTIL_PARSE_H
#define CANON_UTIL_PARSE_H

#include "core/primitives/types.h"       // usize, bool, int64_t, uint64_t, double
#include "core/memory.h"                 // str_len (used in helpers)
#include "../../semantics/result/result.h"   // result_*_Error (adjust path if needed)
#include "../../semantics/error.h"           // ERR_PARSE_FAILED

/**
 * @file util/parse.h
 * @brief Safe, explicit and composable string-to-value parsing
 *
 * Provides robust string parsing functions for common numeric types with
 * modern Result-based error handling. Implements zero-allocation, const-correct
 * parsing with explicit error propagation and composable design patterns.
 *
 * Portability:
 * - Requires C99 or later
 * - Depends only on Canon-C core modules (types, memory, result, error)
 * - Uses standard strto* functions (strtoll, strtoull, strtod) for correctness
 * - No platform-specific features
 * - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 * errno is thread-local in modern C (C11/POSIX).
 *
 * Performance:
 * - Time complexity: O(n) where n = number of digits/characters parsed
 * - Space complexity: O(1) - zero allocations, stack-only operation
 * - Parse operations: Single pass through input string
 * - No dynamic memory - all work done on stack
 * - Minimal overhead over raw strto* functions (thin wrapper)
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit over implicit — all errors returned as values (Result pattern)
 * - No errno checking required — failures encoded in Result type
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
 * - Ok(value): Parsing succeeded, value is valid
 * - Err(ERR_PARSE_FAILED): Parsing failed (invalid format, overflow, etc.)
 *
 * Prefix parsing and endptr pattern:
 * ────────────────────────────────────────────────────────────────────────────
 * All parsing functions implement "prefix parsing" — they consume characters
 * from the beginning until they hit something invalid, then stop.
 *
 * The optional endptr parameter gets updated to point to the first character
 * that was NOT parsed. This enables powerful patterns:
 *
 * 1. CHAINED PARSING:
 * ```c
 * const char* p = "123 456 789";
 * p = parse_skip_ws(p);
 * result_int64_t_Error r1 = parse_int64(p, &p);
 * p = parse_skip_ws(p);
 * result_int64_t_Error r2 = parse_int64(p, &p);
 * ```
 *
 * 2. VALIDATION (entire string consumed):
 * ```c
 * const char* end;
 * result_int64_t_Error r = parse_int64(str, &end);
 * if (result_is_ok(r) && *parse_skip_ws(end) == '\0') {
 *     // Entire string was valid number
 * }
 * ```
 *
 * Whitespace handling philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * Functions do NOT automatically skip leading whitespace.
 * Use parse_skip_ws() explicitly when needed.
 *
 * Number format support:
 * ────────────────────────────────────────────────────────────────────────────
 * Integer parsing (parse_int64, parse_uint64):
 * - Decimal, hexadecimal (0x), octal (0)
 * - Signs: + and - for signed (unsigned rejects -)
 *
 * Floating-point parsing (parse_double):
 * - Decimal, scientific notation, hex floats (C99)
 * - Special values: inf, -inf, nan
 * - Overflow → ±HUGE_VAL (still Ok), underflow → ±0.0 (still Ok)
 */
/* ────────────────────────────────────────────────────────────────────────────
   Result type aliases
   ──────────────────────────────────────────────────────────────────────────── */
CANON_C_DEFINE_RESULT(int64_t, Error)
CANON_C_DEFINE_RESULT(uint64_t, Error)
CANON_C_DEFINE_RESULT(double, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Internal helper — whitespace check (replaces isspace)
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Checks if a character is whitespace (C locale equivalent)
 *
 * Avoids <ctype.h> and locale dependency.
 */
static inline bool is_whitespace_char(char c) {
    switch (c) {
        case ' ': case '\t': case '\n': case '\r':
        case '\f': case '\v':
            return true;
        default:
            return false;
    }
}

/**
 * @brief Skips leading whitespace characters
 *
 * Advances pointer until non-whitespace or end of string.
 *
 * @param s Input string (may be NULL)
 * @return Pointer to first non-whitespace or '\0'
 */
static inline const char* parse_skip_ws(const char* s) {
    if (!s) return s;
    while (*s && is_whitespace_char(*s)) {
        ++s;
    }
    return s;
}

/* ────────────────────────────────────────────────────────────────────────────
   Signed 64-bit integer parsing
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Parses signed 64-bit integer from string prefix
 *
 * Supports decimal, hex (0x), octal (0). Does NOT skip leading whitespace.
 *
 * @param s Input string
 * @param endptr Optional: set to first unparsed character
 * @return Ok(parsed value) or Err(ERR_PARSE_FAILED)
 */
static inline result_int64_t_Error parse_int64(
    const char* s,
    const char** endptr
) {
    if (!s || !*s) {
        if (endptr) *endptr = s;
        return RESULT_ERR(int64_t, ERR_PARSE_FAILED);
    }

    char* eptr;
    errno = 0;
    int64_t val = strtoll(s, &eptr, 0);  // base 0 = auto-detect

    if (eptr == s || errno == ERANGE) {
        if (endptr) *endptr = s;
        return RESULT_ERR(int64_t, ERR_PARSE_FAILED);
    }

    if (endptr) *endptr = eptr;
    return RESULT_OK(int64_t, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Unsigned 64-bit integer parsing
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Parses unsigned 64-bit integer from string prefix
 *
 * Rejects negative values.
 *
 * @param s Input string
 * @param endptr Optional: set to first unparsed character
 * @return Ok(parsed value) or Err(ERR_PARSE_FAILED)
 */
static inline result_uint64_t_Error parse_uint64(
    const char* s,
    const char** endptr
) {
    if (!s || !*s) {
        if (endptr) *endptr = s;
        return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);
    }

    // Reject negative sign immediately
    if (*s == '-') {
        if (endptr) *endptr = s;
        return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);
    }

    char* eptr;
    errno = 0;
    uint64_t val = strtoull(s, &eptr, 0);

    if (eptr == s || errno == ERANGE) {
        if (endptr) *endptr = s;
        return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);
    }

    if (endptr) *endptr = eptr;
    return RESULT_OK(uint64_t, val);
}

/* ────────────────────────────────────────────────────────────────────────────
   Double-precision floating-point parsing
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Parses double from string prefix
 *
 * Supports decimal, scientific, hex floats. Overflow → ±inf (still Ok).
 *
 * @param s Input string
 * @param endptr Optional: set to first unparsed character
 * @return Ok(parsed value) or Err(ERR_PARSE_FAILED)
 */
static inline result_double_Error parse_double(
    const char* s,
    const char** endptr
) {
    if (!s || !*s) {
        if (endptr) *endptr = s;
        return RESULT_ERR(double, ERR_PARSE_FAILED);
    }

    char* eptr;
    double val = strtod(s, &eptr);

    if (eptr == s) {
        if (endptr) *endptr = s;
        return RESULT_ERR(double, ERR_PARSE_FAILED);
    }

    if (endptr) *endptr = eptr;
    return RESULT_OK(double, val);
}

#endif /* CANON_UTIL_PARSE_H */
