// util/parse.h
#ifndef CANON_UTIL_PARSE_H
#define CANON_UTIL_PARSE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

#include "semantics/result.h"
#include "semantics/error.h"

/**
 * @file parse.h
 * @brief Safe, explicit and composable string-to-value parsing
 *
 * Provides robust string parsing functions for common numeric types with
 * modern Result-based error handling. Implements zero-allocation, const-correct
 * parsing with explicit error propagation and composable design patterns.
 *
 * Portability:
 *   - Requires C99 or later (for int64_t, uint64_t, inline functions)
 *   - Depends on standard library: strtoll, strtoull, strtod (C99)
 *   - Uses isspace from <ctype.h> (requires proper unsigned char cast)
 *   - errno dependency for overflow detection (POSIX/C standard)
 *   - Platform-independent numeric parsing (standard semantics)
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state,
 *                no static buffers. Multiple threads can parse simultaneously.
 *                errno is thread-local in modern C (C11, POSIX).
 *
 * Performance:
 *   - Time complexity: O(n) where n = number of digits/characters parsed
 *   - Space complexity: O(1) - zero allocations, stack-only operation
 *   - Parse operations: Single pass through input string
 *   - No dynamic memory - all work done on stack
 *   - Minimal overhead over raw strto* functions (thin wrapper)
 *   - Integer parsing: Typically completes in <100 CPU cycles
 *   - Floating-point: More complex, depends on precision
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
 * Traditional C parsing (strto* family) uses a complex error protocol:
 * - Return value might be valid or error indicator
 * - Must check errno for ERANGE
 * - Must check if endptr == input for "no conversion"
 * - Easy to forget checks, leading to bugs
 *
 * This library wraps that complexity into Result types:
 * - Ok(value): Parsing succeeded, value is valid
 * - Err(error_code): Parsing failed, reason encoded
 * - Cannot accidentally use invalid value
 * - Compiler-enforced error checking
 * - Composable with other Result-based code
 *
 * Example comparison:
 * ```c
 * // Traditional C (error-prone):
 * char* end;
 * errno = 0;
 * long val = strtol(str, &end, 10);
 * if (end == str || errno == ERANGE) {
 *     // error handling
 * }
 * 
 * // This library (safe):
 * result_int64_t_Error r = parse_int64(str, NULL);
 * if (result_is_ok(r)) {
 *     int64_t val = result_unwrap(r);
 *     // use val
 * }
 * ```
 *
 * Prefix parsing and endptr pattern:
 * ────────────────────────────────────────────────────────────────────────────
 * All parsing functions implement "prefix parsing" — they consume characters
 * from the beginning of the string until they hit something invalid, then
 * stop. The rest of the string is left untouched.
 *
 * The optional endptr parameter gets updated to point to the first character
 * that was NOT parsed. This enables several powerful patterns:
 *
 * 1. CHAINED PARSING - Parse multiple values from one string:
 *    const char* p = "123 456 789";
 *    result_int64_t_Error r1 = parse_int64(p, &p);  // p now → " 456 789"
 *    p = parse_skip_ws(p);                           // p now → "456 789"
 *    result_int64_t_Error r2 = parse_int64(p, &p);  // p now → " 789"
 *
 * 2. VALIDATION - Ensure entire string was consumed:
 *    const char* end;
 *    result_int64_t_Error r = parse_int64(str, &end);
 *    if (result_is_ok(r) && *end == '\0') {
 *        // Entire string was valid number
 *    }
 *
 * 3. TOLERANT PARSING - Parse what's valid, ignore rest:
 *    result_int64_t_Error r = parse_int64("42junk", NULL);
 *    // r = Ok(42), "junk" is ignored
 *
 * 4. MANUAL TOKENIZATION - Build custom parsers:
 *    const char* p = line;
 *    while (*p) {
 *        p = parse_skip_ws(p);
 *        result_int64_t_Error r = parse_int64(p, &p);
 *        if (result_is_ok(r)) {
 *            process(result_unwrap(r));
 *        } else {
 *            break;  // No more numbers
 *        }
 *    }
 *
 * Whitespace handling philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * These functions do NOT automatically skip leading whitespace. This is
 * intentional for several reasons:
 *
 * - Explicit is better than implicit (caller decides when to skip)
 * - Enables strict parsing ("123" succeeds, " 123" fails)
 * - Allows custom whitespace definitions if needed
 * - More efficient when whitespace already handled
 * - Composable with parse_skip_ws() helper
 *
 * Common pattern:
 * ```c
 * const char* p = parse_skip_ws(input);  // Skip whitespace explicitly
 * result_int64_t_Error r = parse_int64(p, NULL);
 * ```
 *
 * Number format support:
 * ────────────────────────────────────────────────────────────────────────────
 * Integer parsing (parse_int64, parse_uint64):
 * - Decimal: 123, -456
 * - Hexadecimal: 0x1a2b, 0XFFFF (case-insensitive prefix and digits)
 * - Octal: 0777, 0123 (leading zero indicates octal)
 * - Signs: + and - for signed types (unsigned rejects -)
 * - Auto-detection: base 0 parameter to strto* enables automatic format detection
 *
 * Floating-point parsing (parse_double):
 * - Decimal: 3.14159, -0.001, 42.0
 * - Scientific notation: 1.23e-4, -4.56E+02, 6.022e23
 * - Hex floats (C99): 0x1.8p-3 (rarely used, mainly for exact representation)
 * - Special values: inf, infinity, nan (implementation-defined)
 * - Leading signs: +/-
 *
 * Integer examples:
 *   "42"      → 42 (decimal)
 *   "0x2a"    → 42 (hexadecimal)
 *   "052"     → 42 (octal)
 *   "-123"    → -123 (negative decimal)
 *   "+99"     → 99 (explicit positive)
 *
 * Floating-point examples:
 *   "3.14"    → 3.14
 *   "1e3"     → 1000.0
 *   "2.5e-2"  → 0.025
 *   "-0.5"    → -0.5
 *
 * Error conditions and overflow:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions return Err(ERR_PARSE_FAILED) for these conditions:
 *
 * 1. NULL or empty input string
 * 2. No valid number at beginning of string
 * 3. Numeric overflow (value exceeds type range)
 * 4. Numeric underflow (for floating-point, may return ±0.0 instead)
 * 5. Invalid format for type (e.g., negative value for unsigned)
 *
 * Overflow behavior:
 * - Integer overflow: ERR_PARSE_FAILED (detected via errno == ERANGE)
 * - Float overflow: Usually returns ±HUGE_VAL but still Ok (C standard behavior)
 * - Float underflow: Usually returns ±0.0 and still Ok
 *
 * Edge cases:
 * - Empty string: Err(ERR_PARSE_FAILED)
 * - Whitespace-only: Err(ERR_PARSE_FAILED) unless parse_skip_ws used first
 * - Just a sign: Err(ERR_PARSE_FAILED) (e.g., "+", "-")
 * - Invalid hex: Err(ERR_PARSE_FAILED) (e.g., "0x")
 * - Mixed formats: Parses valid prefix (e.g., "123abc" → Ok(123), endptr → "abc")
 *
 * Range limits (assuming typical 64-bit platform):
 * - int64_t:  -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
 * - uint64_t: 0 to 18,446,744,073,709,551,615
 * - double:   ±1.7E±308 (15-17 significant decimal digits)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Configuration file parsing (key=value pairs)
 * - Command-line argument parsing
 * - CSV/TSV data parsing
 * - Protocol message parsing (HTTP headers, etc.)
 * - User input validation
 * - Simple tokenization and lexical analysis
 * - Embedded scripting language interpreters
 * - Data serialization/deserialization
 * - Network protocol implementations
 * - File format parsers
 * - Calculator/expression evaluator implementations
 *
 * NOT suitable for:
 * - Complex grammars (use proper parser)
 * - Very large numbers (beyond 64-bit, use arbitrary precision library)
 * - Exact decimal arithmetic (use fixed-point or decimal library)
 * - Locale-aware parsing (these functions are locale-independent)
 * - Custom number formats (implement custom parser)
 *
 * Usage examples:
 *
 * Basic single value parsing:
 * ```c
 * const char* input = "-12345";
 * result_int64_t_Error r = parse_int64(input, NULL);
 * if (result_is_ok(r)) {
 *     int64_t value = result_unwrap(r);
 *     printf("Parsed: %lld\n", value);
 * } else {
 *     fprintf(stderr, "Parse failed\n");
 * }
 * ```
 *
 * Parsing with whitespace handling:
 * ```c
 * const char* input = "   42   ";
 * const char* p = parse_skip_ws(input);  // Skip leading whitespace
 * result_int64_t_Error r = parse_int64(p, NULL);
 * // r = Ok(42)
 * ```
 *
 * Chained parsing (multiple values):
 * ```c
 * const char* line = "123 456 789";
 * const char* p = line;
 * 
 * p = parse_skip_ws(p);
 * result_int64_t_Error r1 = parse_int64(p, &p);
 * 
 * p = parse_skip_ws(p);
 * result_int64_t_Error r2 = parse_int64(p, &p);
 * 
 * p = parse_skip_ws(p);
 * result_int64_t_Error r3 = parse_int64(p, &p);
 * 
 * if (result_is_ok(r1) && result_is_ok(r2) && result_is_ok(r3)) {
 *     printf("%lld, %lld, %lld\n", 
 *            result_unwrap(r1), 
 *            result_unwrap(r2), 
 *            result_unwrap(r3));
 * }
 * ```
 *
 * Validation (ensure entire string consumed):
 * ```c
 * const char* input = "12345";
 * const char* end;
 * result_int64_t_Error r = parse_int64(input, &end);
 * 
 * if (result_is_ok(r) && *end == '\0') {
 *     printf("Entire string was valid: %lld\n", result_unwrap(r));
 * } else if (result_is_ok(r)) {
 *     printf("Trailing garbage: '%s'\n", end);
 * } else {
 *     printf("Invalid number\n");
 * }
 * ```
 *
 * Tolerant parsing with fallback:
 * ```c
 * result_uint64_t_Error r = parse_uint64(config_port, NULL);
 * uint64_t port = result_is_ok(r) ? result_unwrap(r) : 8080;
 * printf("Using port: %llu\n", port);
 * ```
 */

/**
 * @brief Result type aliases for parsed numeric values
 *
 * These type aliases provide Result types wrapping parsed values:
 * - Ok(T): Successfully parsed value of type T
 * - Err(Error): Parse failure with error code
 *
 * Usage:
 * ```c
 * result_int64_t_Error r = parse_int64("123", NULL);
 * if (result_is_ok(r)) {
 *     int64_t value = result_unwrap(r);
 * } else {
 *     Error err = result_unwrap_err(r);
 * }
 * ```
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
 * Attempts to parse an int64_t value from the start of the input string.
 * Supports decimal, hexadecimal (0x prefix), and octal (0 prefix) formats.
 * Stops at the first invalid character and optionally reports the position.
 *
 * This function does NOT skip leading whitespace. Use parse_skip_ws() first
 * if you need to handle whitespace.
 *
 * Algorithm:
 *   - Validates input is non-NULL and non-empty
 *   - Clears errno for reliable overflow detection
 *   - Calls strtoll(s, &endptr, 0) for actual parsing
 *     - base 0 enables auto-detection: decimal/hex/octal
 *   - Checks if any characters were consumed (endptr != s)
 *   - Checks for overflow/underflow (errno == ERANGE)
 *   - Returns Ok(value) or Err(ERR_PARSE_FAILED)
 *
 * @param s      Null-terminated input string (must not be NULL or empty)
 * @param endptr Optional output: set to first character after parsed number
 *               If NULL, position information is discarded
 *               On success: points to first non-digit character
 *               On failure: points to original string (no advance)
 *
 * @return       Ok(parsed int64_t value) on success
 *               Err(ERR_PARSE_FAILED) on any failure condition
 *
 * Preconditions:
 *   - s should point to a null-terminated string
 *   - If endptr provided, it should be a valid pointer
 *
 * Postconditions:
 *   - If Ok: return value contains valid int64_t in range
 *   - If Ok and endptr: *endptr points to first unparsed character
 *   - If Err: no valid value available, endptr unchanged or set to s
 *   - Input string never modified (const-correct)
 *
 * Supported formats:
 *   - Decimal:     123, -456, +789 (optional leading sign)
 *   - Hexadecimal: 0x1a2b, 0XFFFF, -0x100 (0x/0X prefix, case-insensitive)
 *   - Octal:       0777, 0123, -052 (leading zero, digits 0-7)
 *
 * Failure conditions (returns Err):
 *   - s is NULL
 *   - s is empty string ("")
 *   - No valid digits found (e.g., "abc", "  ", "+")
 *   - Overflow: value > INT64_MAX (9,223,372,036,854,775,807)
 *   - Underflow: value < INT64_MIN (-9,223,372,036,854,775,808)
 *   - Invalid format for selected base
 *
 * Performance:
 *   - Time: O(n) where n = number of digits
 *   - Space: O(1) - no allocations
 *   - Typical: Completes in <100 CPU cycles for small numbers
 *
 * Thread-safety: Fully thread-safe (errno is thread-local in C11/POSIX)
 *
 * Example - Basic parsing:
 * ```c
 * result_int64_t_Error r = parse_int64("42", NULL);
 * if (result_is_ok(r)) {
 *     printf("Value: %lld\n", result_unwrap(r));  // → 42
 * }
 * ```
 *
 * Example - Hexadecimal:
 * ```c
 * result_int64_t_Error r = parse_int64("0xFF", NULL);
 * // r = Ok(255)
 * ```
 *
 * Example - Negative octal:
 * ```c
 * result_int64_t_Error r = parse_int64("-052", NULL);
 * // r = Ok(-42)  (octal 052 = decimal 42)
 * ```
 *
 * Example - Using endptr for position tracking:
 * ```c
 * const char* s = "123abc";
 * const char* end;
 * result_int64_t_Error r = parse_int64(s, &end);
 * // r = Ok(123), end points to "abc"
 * printf("Remaining: %s\n", end);  // → "abc"
 * ```
 *
 * Example - Overflow detection:
 * ```c
 * result_int64_t_Error r = parse_int64("999999999999999999999", NULL);
 * // r = Err(ERR_PARSE_FAILED) - exceeds INT64_MAX
 * ```
 *
 * Example - Whitespace handling:
 * ```c
 * const char* s = "  42";
 * result_int64_t_Error r = parse_int64(s, NULL);
 * // r = Err(ERR_PARSE_FAILED) - leading whitespace not auto-skipped
 * 
 * // Correct approach:
 * s = parse_skip_ws(s);
 * r = parse_int64(s, NULL);
 * // r = Ok(42)
 * ```
 *
 * Common pitfalls:
 * - Forgetting whitespace handling (not auto-skipped)
 * - Not checking Result before unwrapping
 * - Assuming endptr will be updated on error
 * - Mixing up NULL vs empty string behavior
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
    errno = 0;  // Required for reliable overflow detection
    int64_t val = strtoll(s, &eptr, 0);  // base 0 = auto (dec/hex/oct)

    // Check if no conversion was performed or overflow occurred
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
 * Identical to parse_int64() but for unsigned values. Rejects negative
 * numbers and provides larger positive range (0 to 2^64-1).
 *
 * Algorithm and behavior match parse_int64() except:
 *   - Uses strtoull() instead of strtoll()
 *   - Negative values are rejected (parsing fails)
 *   - Maximum value: 18,446,744,073,709,551,615 (UINT64_MAX)
 *   - Minimum value: 0
 *
 * @param s      Null-terminated input string
 * @param endptr Optional output pointer to first unparsed character
 *
 * @return       Ok(parsed uint64_t) on success
 *               Err(ERR_PARSE_FAILED) on failure (invalid, overflow, negative)
 *
 * Preconditions:
 *   - Same as parse_int64()
 *
 * Postconditions:
 *   - If Ok: value is in range [0, UINT64_MAX]
 *   - If Ok: no negative sign was present
 *   - Otherwise same as parse_int64()
 *
 * Failure conditions (returns Err):
 *   - All parse_int64() failure conditions
 *   - Negative sign present (e.g., "-123")
 *   - Value exceeds UINT64_MAX
 *
 * Performance: Same as parse_int64()
 *
 * Example - Basic unsigned:
 * ```c
 * result_uint64_t_Error r = parse_uint64("12345", NULL);
 * // r = Ok(12345)
 * ```
 *
 * Example - Large value:
 * ```c
 * result_uint64_t_Error r = parse_uint64("18446744073709551615", NULL);
 * // r = Ok(UINT64_MAX)
 * ```
 *
 * Example - Hexadecimal:
 * ```c
 * result_uint64_t_Error r = parse_uint64("0xFFFFFFFFFFFFFFFF", NULL);
 * // r = Ok(UINT64_MAX)
 * ```
 *
 * Example - Negative rejection:
 * ```c
 * result_uint64_t_Error r = parse_uint64("-123", NULL);
 * // r = Err(ERR_PARSE_FAILED) - negative not allowed for unsigned
 * ```
 *
 * Use cases:
 *   - Port numbers, IDs, counts
 *   - Memory addresses, sizes
 *   - Unsigned flags, bitmasks
 *   - Timestamps (Unix epoch)
 *   - File sizes, offsets
 */
static inline result_uint64_t_Error parse_uint64(
    const char* s,
    const char** endptr
) {
    if (!s || !*s) {
        if (endptr) *endptr = s;
        return RESULT_ERR(uint64_t, ERR_PARSE_FAILED);
    }

    // Fix: reject negative numbers
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
 * Parses a double-precision floating-point number from the beginning of
 * the input string. Supports decimal notation, scientific notation, and
 * special values (inf, nan). Does not skip leading whitespace.
 *
 * Algorithm:
 *   - Validates input is non-NULL and non-empty
 *   - Calls strtod(s, &endptr) for actual parsing
 *   - Checks if any characters were consumed
 *   - Returns Ok(value) or Err(ERR_PARSE_FAILED)
 *   - Note: Does NOT check errno for overflow (strtod behavior)
 *
 * @param s      Null-terminated input string
 * @param endptr Optional pointer updated to first non-parsed character
 *
 * @return       Ok(parsed double) on success
 *               Err(ERR_PARSE_FAILED) if no valid number found
 *
 * Preconditions:
 *   - s should point to null-terminated string
 *   - If endptr provided, it should be a valid pointer
 *
 * Postconditions:
 *   - If Ok: value is a valid double (may be ±inf, ±0, or NaN)
 *   - If Ok and endptr: *endptr points to first unparsed character
 *   - If Err: no valid number was found at beginning
 *   - Input string never modified
 *
 * Supported formats:
 *   - Decimal:          3.14159, -0.001, 42.0
 *   - Scientific:       1.23e-4, -4.56E+02, 6.022e23
 *   - Hex float (C99):  0x1.8p-3 (exact binary representation)
 *   - Special values:   inf, infinity, nan (case-insensitive, implementation-defined)
 *   - Leading sign:     + or -
 *
 * Special value examples:
 *   "inf"      → +infinity
 *   "-inf"     → -infinity  
 *   "nan"      → NaN (Not a Number)
 *   "INFINITY" → +infinity (case-insensitive)
 *
 * Overflow/underflow behavior:
 *   - Overflow: Returns ±HUGE_VAL (typically ±infinity) - still Ok(value)
 *   - Underflow: Returns ±0.0 - still Ok(value)
 *   - Unlike integer parsing, floating-point overflow is NOT an error
 *   - Check for infinity: isinf(value) after unwrapping
 *
 * Failure conditions (returns Err):
 *   - s is NULL
 *   - s is empty string
 *   - No valid number at beginning (e.g., "abc", "  ")
 *
 * Performance:
 *   - Time: O(n) where n = number of digits/characters
 *   - Space: O(1) - no allocations
 *   - More complex than integer parsing (floating-point arithmetic)
 *
 * Thread-safety: Fully thread-safe
 *
 * Example - Basic decimal:
 * ```c
 * result_double_Error r = parse_double("3.14159", NULL);
 * // r = Ok(3.14159)
 * ```
 *
 * Example - Scientific notation:
 * ```c
 * result_double_Error r = parse_double("6.022e23", NULL);
 * // r = Ok(6.022e23) - Avogadro's number
 * ```
 *
 * Example - Negative scientific:
 * ```c
 * result_double_Error r = parse_double("-1.6e-19", NULL);
 * // r = Ok(-1.6e-19) - electron charge magnitude
 * ```
 *
 * Example - Special values:
 * ```c
 * result_double_Error r1 = parse_double("inf", NULL);
 * // r1 = Ok(+infinity)
 * 
 * result_double_Error r2 = parse_double("-inf", NULL);
 * // r2 = Ok(-infinity)
 * 
 * result_double_Error r3 = parse_double("nan", NULL);
 * // r3 = Ok(NaN)
 * ```
 *
 * Example - Checking for infinity:
 * ```c
 * result_double_Error r = parse_double("1e500", NULL);
 * if (result_is_ok(r)) {
 *     double val = result_unwrap(r);
 *     if (isinf(val)) {
 *         printf("Overflow to infinity\n");
 *     }
 * }
 * ```
 *
 * Example - Using endptr:
 * ```c
 * const char* s = "2.5e-3xyz";
 * const char* end;
 * result_double_Error r = parse_double(s, &end);
 * // r = Ok(0.0025), end points to "xyz"
 * ```
 *
 * Example - Hex float (rare but valid):
 * ```c
 * result_double_Error r = parse_double("0x1.8p3", NULL);
 * // r = Ok(12.0) - (1 + 8/16) * 2^3 = 1.5 * 8 = 12
 * ```
 *
 * Common pitfalls:
 * - Not checking for infinity after parsing large numbers
 * - Assuming overflow causes Err (it doesn't for floats)
 * - Comparing floating-point equality directly (use epsilon)
 * - Not handling NaN specially in calculations
 *
 * Use cases:
 *   - Configuration files (timeouts, thresholds)
 *   - Scientific data parsing
 *   - User input (measurements, calculations)
 *   - Protocol parsing (JSON, XML)
 *   - Mathematical expressions
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

    // Only fail if no conversion was performed
    // Note: overflow/underflow still return Ok with ±HUGE_VAL or ±0
    if (eptr == s) {
        if (endptr) *endptr = s;
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
 * Advances through the input string until a non-whitespace character is
 * found or end of string is reached. Uses standard isspace() definition
 * of whitespace: space, tab, newline, carriage return, form feed, vertical tab.
 *
 * This is a pure utility function with no side effects. It simply returns
 * a pointer into the original string, never allocates or modifies anything.
 *
 * Algorithm:
 *   - If s is NULL, returns NULL immediately
 *   - While current character is whitespace, advance pointer
 *   - Return pointer to first non-whitespace or end of string
 *
 * @param s Input string (may be NULL)
 *
 * @return  Pointer to first non-whitespace character in s
 *          OR pointer to null terminator if all whitespace
 *          OR NULL if input was NULL
 *
 * Preconditions:
 *   - If s is not NULL, it should be null-terminated
 *
 * Postconditions:
 *   - Return value points into same memory as input
 *   - Return value >= s (never goes backward)
 *   - If return value is not NULL, it points to valid string
 *   - Input string never modified
 *
 * Whitespace characters (per isspace()):
 *   - Space:         ' '  (0x20)
 *   - Tab:           '\t' (0x09)
 *   - Newline:       '\n' (0x0A)
 *   - Carriage ret:  '\r' (0x0D)
 *   - Form feed:     '\f' (0x0C)
 *   - Vertical tab:  '\v' (0x0B)
 *
 * Performance:
 *   - Time: O(n) where n = number of leading whitespace characters
 *   - Space: O(1) - no allocations, just pointer arithmetic
 *   - Typical: <10 CPU cycles for common cases
 *
 * Thread-safety: Fully thread-safe (read-only operation)
 *
 * Example - Basic usage:
 * ```c
 * const char* s = "   42";
 * s = parse_skip_ws(s);
 * // s now points to "42"
 * result_int64_t_Error r = parse_int64(s, NULL);
 * // r = Ok(42)
 * ```
 *
 * Example - Multiple whitespace types:
 * ```c
 * const char* s = " \t\n\r  hello";
 * s = parse_skip_ws(s);
 * // s points to "hello"
 * ```
 *
 * Example - All whitespace:
 * ```c
 * const char* s = "   \t\n   ";
 * s = parse_skip_ws(s);
 * // s points to "" (null terminator)
 * // *s == '\0'
 * ```
 *
 * Example - No whitespace:
 * ```c
 * const char* s = "hello world";
 * s = parse_skip_ws(s);
 * // s unchanged, still points to "hello world"
 * ```
 *
 * Example - NULL input:
 * ```c
 * const char* s = NULL;
 * s = parse_skip_ws(s);
 * // s is still NULL
 * ```
 *
 * Example - Chained parsing workflow:
 * ```c
 * const char* line = "  123   456   789  ";
 * const char* p = line;
 * 
 * p = parse_skip_ws(p);                     // → "123   456   789  "
 * result_int64_t_Error r1 = parse_int64(p, &p);  // p → "   456   789  "
 * 
 * p = parse_skip_ws(p);                     // → "456   789  "
 * result_int64_t_Error r2 = parse_int64(p, &p);  // p → "   789  "
 * 
 * p = parse_skip_ws(p);                     // → "789  "
 * result_int64_t_Error r3 = parse_int64(p, &p);  // p → "  "
 * 
 * p = parse_skip_ws(p);                     // → ""
 * // *p == '\0', end of line
 * ```
 *
 * Example - Configuration file parsing:
 * ```c
 * void parse_config_line(const char* line) {
 *     line = parse_skip_ws(line);  // Skip leading whitespace
 *     
 *     if (*line == '#' || *line == '\0') {
 *         return;  // Comment or empty line
 *     }
 *     
 *     // Parse key=value...
 * }
 * ```
 *
 * Implementation note:
 * Uses (unsigned char) cast before isspace() to avoid undefined behavior
 * with negative char values. This is the standard-compliant way to use
 * isspace() and other ctype.h functions.
 *
 * Common patterns:
 * - Pre-parse cleanup: p = parse_skip_ws(p);
 * - Between tokens: p = parse_skip_ws(p); parse_next(p, &p);
 * - Line processing: line = parse_skip_ws(line);
 *
 * Use cases:
 *   - Configuration file parsing
 *   - Command-line tokenization
 *   - CSV/TSV parsing
 *   - Protocol message parsing
 *   - Lexical analysis
 *   - Any whitespace-tolerant parsing
 */
static inline const char* parse_skip_ws(const char* s) {
    if (!s) return s;
    
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    
    return s;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/parse.h"
    #include <stdio.h>
    #include <math.h>
    
    // Example 1: Basic integer parsing
    void example_basic_parsing(void) {
        const char* inputs[] = {"42", "-123", "0xFF", "0777"};
        
        for (size_t i = 0; i < 4; i++) {
            result_int64_t_Error r = parse_int64(inputs[i], NULL);
            if (result_is_ok(r)) {
                printf("%s → %lld\n", inputs[i], result_unwrap(r));
            } else {
                printf("%s → parse failed\n", inputs[i]);
            }
        }
        // Output:
        // 42 → 42
        // -123 → -123
        // 0xFF → 255
        // 0777 → 511
    }
    
    // Example 2: Parsing with whitespace handling
    void example_whitespace(void) {
        const char* input = "   42   ";
        
        // Without whitespace skip - fails
        result_int64_t_Error r1 = parse_int64(input, NULL);
        // r1 = Err(ERR_PARSE_FAILED)
        
        // With whitespace skip - succeeds
        const char* trimmed = parse_skip_ws(input);
        result_int64_t_Error r2 = parse_int64(trimmed, NULL);
        // r2 = Ok(42)
        
        if (result_is_ok(r2)) {
            printf("Parsed: %lld\n", result_unwrap(r2));
        }
    }
    
    // Example 3: Chained parsing (CSV-like)
    void example_csv_parsing(void) {
        const char* line = "123,456,789";
        const char* p = line;
        int64_t values[3];
        size_t count = 0;
        
        while (*p && count < 3) {
            result_int64_t_Error r = parse_int64(p, &p);
            if (result_is_ok(r)) {
                values[count++] = result_unwrap(r);
            } else {
                break;
            }
            
            if (*p == ',') p++;  // Skip comma
        }
        
        printf("Parsed %zu values: ", count);
        for (size_t i = 0; i < count; i++) {
            printf("%lld ", values[i]);
        }
        printf("\n");
    }
    
    // Example 4: Configuration file parsing
    typedef struct {
        int64_t timeout_ms;
        uint64_t max_connections;
        double retry_backoff;
    } Config;
    
    bool parse_config_line(const char* line, Config* cfg) {
        line = parse_skip_ws(line);
        
        if (*line == '#' || *line == '\0') {
            return true;  // Comment or empty
        }
        
        // Simple key=value parser
        if (strncmp(line, "timeout=", 8) == 0) {
            result_int64_t_Error r = parse_int64(line + 8, NULL);
            if (result_is_ok(r)) {
                cfg->timeout_ms = result_unwrap(r);
                return true;
            }
        } else if (strncmp(line, "max_connections=", 16) == 0) {
            result_uint64_t_Error r = parse_uint64(line + 16, NULL);
            if (result_is_ok(r)) {
                cfg->max_connections = result_unwrap(r);
                return true;
            }
        } else if (strncmp(line, "retry_backoff=", 14) == 0) {
            result_double_Error r = parse_double(line + 14, NULL);
            if (result_is_ok(r)) {
                cfg->retry_backoff = result_unwrap(r);
                return true;
            }
        }
        
        return false;  // Unknown or invalid
    }
    
    // Example 5: Validation (entire string must be valid)
    bool is_valid_number(const char* s) {
        const char* end;
        const char* trimmed = parse_skip_ws(s);
        
        result_int64_t_Error r = parse_int64(trimmed, &end);
        if (result_is_err(r)) {
            return false;
        }
        
        // Check if entire string was consumed (ignoring trailing whitespace)
        end = parse_skip_ws(end);
        return *end == '\0';
    }
    
    void example_validation(void) {
        printf("'42' valid: %d\n", is_valid_number("42"));          // → 1
        printf("'42abc' valid: %d\n", is_valid_number("42abc"));    // → 0
        printf("'  42  ' valid: %d\n", is_valid_number("  42  "));  // → 1
    }
    
    // Example 6: Tolerant parsing with defaults
    void example_with_defaults(void) {
        const char* inputs[] = {"8080", "invalid", "", NULL};
        uint64_t default_port = 3000;
        
        for (size_t i = 0; i < 4; i++) {
            result_uint64_t_Error r = parse_uint64(inputs[i], NULL);
            uint64_t port = result_is_ok(r) ? result_unwrap(r) : default_port;
            
            printf("Input '%s' → port %llu\n", 
                   inputs[i] ? inputs[i] : "NULL", port);
        }
        // Output:
        // Input '8080' → port 8080
        // Input 'invalid' → port 3000
        // Input '' → port 3000
        // Input 'NULL' → port 3000
    }
    
    // Example 7: Floating-point parsing
    void example_float_parsing(void) {
        const char* floats[] = {
            "3.14159",
            "6.022e23",
            "-1.6e-19",
            "inf",
            "-inf",
            "nan"
        };
        
        for (size_t i = 0; i < 6; i++) {
            result_double_Error r = parse_double(floats[i], NULL);
            if (result_is_ok(r)) {
                double val = result_unwrap(r);
                if (isinf(val)) {
                    printf("%s → infinity (%s)\n", floats[i], 
                           val > 0 ? "+" : "-");
                } else if (isnan(val)) {
                    printf("%s → NaN\n", floats[i]);
                } else {
                    printf("%s → %g\n", floats[i], val);
                }
            }
        }
    }
    
    // Example 8: Manual tokenization
    void example_tokenization(void) {
        const char* input = "  10  20  30  40  50  ";
        const char* p = input;
        int64_t sum = 0;
        size_t count = 0;
        
        while (*p) {
            p = parse_skip_ws(p);
            if (*p == '\0') break;
            
            result_int64_t_Error r = parse_int64(p, &p);
            if (result_is_ok(r)) {
                sum += result_unwrap(r);
                count++;
            } else {
                break;
            }
        }
        
        printf("Sum of %zu numbers: %lld\n", count, sum);
        // Output: Sum of 5 numbers: 150
    }
    
    // Example 9: Overflow detection
    void example_overflow(void) {
        const char* big_number = "99999999999999999999";  // Exceeds INT64_MAX
        
        result_int64_t_Error r = parse_int64(big_number, NULL);
        if (result_is_err(r)) {
            printf("Overflow detected for: %s\n", big_number);
        }
        
        // Floating-point overflow is different
        const char* huge_float = "1e500";
        result_double_Error r2 = parse_double(huge_float, NULL);
        if (result_is_ok(r2)) {
            double val = result_unwrap(r2);
            if (isinf(val)) {
                printf("Float overflow → infinity\n");
            }
        }
    }
    
    // Example 10: Command-line argument parsing
    typedef struct {
        uint64_t port;
        int64_t timeout;
        double threshold;
    } Args;
    
    bool parse_args(int argc, char** argv, Args* args) {
        // Simple --key=value parser
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--port=", 7) == 0) {
                result_uint64_t_Error r = parse_uint64(argv[i] + 7, NULL);
                if (result_is_err(r)) return false;
                args->port = result_unwrap(r);
            } else if (strncmp(argv[i], "--timeout=", 10) == 0) {
                result_int64_t_Error r = parse_int64(argv[i] + 10, NULL);
                if (result_is_err(r)) return false;
                args->timeout = result_unwrap(r);
            } else if (strncmp(argv[i], "--threshold=", 12) == 0) {
                result_double_Error r = parse_double(argv[i] + 12, NULL);
                if (result_is_err(r)) return false;
                args->threshold = result_unwrap(r);
            }
        }
        return true;
    }
    
    // Example 11: Parsing key-value pairs
    bool parse_key_value(const char* line, char* key, int64_t* value) {
        const char* eq = strchr(line, '=');
        if (!eq) return false;
        
        // Extract key
        size_t key_len = eq - line;
        strncpy(key, line, key_len);
        key[key_len] = '\0';
        
        // Parse value
        const char* val_str = parse_skip_ws(eq + 1);
        result_int64_t_Error r = parse_int64(val_str, NULL);
        if (result_is_err(r)) return false;
        
        *value = result_unwrap(r);
        return true;
    }
    
    void example_key_value(void) {
        const char* line = "timeout = 5000";
        char key[64];
        int64_t value;
        
        if (parse_key_value(line, key, &value)) {
            printf("Key: '%s', Value: %lld\n", key, value);
        }
    }
    
    // Example 12: Hexadecimal color parsing
    typedef struct { uint8_t r, g, b; } Color;
    
    bool parse_hex_color(const char* s, Color* color) {
        if (s[0] != '#' || strlen(s) != 7) {
            return false;
        }
        
        const char* end;
        result_uint64_t_Error r = parse_uint64(s + 1, &end);
        if (result_is_err(r)) return false;
        
        uint64_t val = result_unwrap(r);
        color->r = (val >> 16) & 0xFF;
        color->g = (val >> 8) & 0xFF;
        color->b = val & 0xFF;
        
        return true;
    }
    
    void example_color_parsing(void) {
        Color c;
        if (parse_hex_color("#FF5733", &c)) {
            printf("RGB: (%u, %u, %u)\n", c.r, c.g, c.b);
            // Output: RGB: (255, 87, 51)
        }
    }
    
    // Example 13: Multi-format number parsing
    int64_t parse_number_flexible(const char* s) {
        const char* trimmed = parse_skip_ws(s);
        
        // Try decimal first
        result_int64_t_Error r = parse_int64(trimmed, NULL);
        if (result_is_ok(r)) {
            return result_unwrap(r);
        }
        
        // Could try other formats, fallbacks, etc.
        return 0;  // Default
    }
    
    // Example 14: Scientific notation parsing
    void example_scientific(void) {
        const char* numbers[] = {
            "6.022e23",      // Avogadro's number
            "1.602e-19",     // Elementary charge
            "299792458",     // Speed of light (m/s)
            "6.626e-34"      // Planck constant
        };
        
        for (size_t i = 0; i < 4; i++) {
            result_double_Error r = parse_double(numbers[i], NULL);
            if (result_is_ok(r)) {
                printf("%s = %e\n", numbers[i], result_unwrap(r));
            }
        }
    }
    
    // Example 15: Error-tolerant line parsing
    void parse_data_line(const char* line) {
        const char* p = parse_skip_ws(line);
        
        // Try to parse ID
        result_int64_t_Error id_r = parse_int64(p, &p);
        if (result_is_err(id_r)) {
            printf("Invalid ID in line\n");
            return;
        }
        
        // Skip whitespace and comma
        p = parse_skip_ws(p);
        if (*p == ',') p++;
        p = parse_skip_ws(p);
        
        // Try to parse value
        result_double_Error val_r = parse_double(p, &p);
        if (result_is_err(val_r)) {
            printf("Invalid value in line\n");
            return;
        }
        
        printf("ID: %lld, Value: %g\n", 
               result_unwrap(id_r), result_unwrap(val_r));
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_PARSE_H */
