#ifndef CANON_SEMANTICS_ERROR_H
#define CANON_SEMANTICS_ERROR_H

#include "core/primitives/types.h"   // usize, bool
#include "result/result.h"           // CANON_RESULT macro

/**
 * @file error.h
 * @brief Common error codes and human-readable messages
 *
 * Provides a small, extensible set of standard error codes intended for use
 * with Result<T, Error> throughout the library. Designed for zero-overhead
 * error handling with consistent, human-readable error messages.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Depends on result/result.h from this library
 * - No platform-specific code
 * - No external dependencies beyond standard C
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are thread-safe (pure/const, no shared mutable state)
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(1) — constant time for all operations
 * - Space: O(1) — static data only, no allocations
 * - Error representation: simple integer (enum)
 * - Message lookup: switch-based (usually jump table)
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Simple flat enum (no hierarchies)
 * - Zero runtime cost for error representation
 * - Consistent messages via error_message()
 * - Easy to extend (add before ERR_COUNT)
 * - Works seamlessly with result.h combinators
 * - Thread-safe pure functions
 * - Generic enough for cross-domain use
 * - Brief messages — add context at call sites
 *
 * Error code ranges (for organization):
 * - 0:     Success (ERR_OK)
 * - 1-99:  Argument/input errors
 * - 100-199: Resource/memory errors
 * - 200-299: I/O and system errors
 * - 300-399: Arithmetic errors
 * - 400+:  Generic/miscellaneous
 *
 * Extension guide:
 * 1. Add new code before ERR_COUNT in the enum
 * 2. Add case in error_message() switch
 * 3. (Optional) Add domain-specific enum (e.g. ErrorJson)
 *
 * Typical use:
 * ```c
 * CANON_RESULT(int, Error)
 *
 * result_int_Error parse_number(const char* s) {
 *     if (!s) return RESULT_ERR(int, ERR_INVALID_ARG);
 *     // ...
 *     return RESULT_OK(int, value);
 * }
 * ```
 */
/* ════════════════════════════════════════════════════════════════════════════
   ERROR CODE ENUMERATION
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Common error codes used across the library
 *
 * General-purpose error codes suitable for most applications.
 * Always use ERR_OK = 0 for success (rarely appears in Result::Err).
 * Add new specific errors before ERR_COUNT when needed.
 * Keep the list small and general-purpose.
 */
typedef enum {
    /* Success (not typically used in Result::Err) */
    ERR_OK = 0,

    /* Argument and input validation errors (1-99) */
    ERR_INVALID_ARG,        // NULL pointer, invalid value, precondition failed
    ERR_OUT_OF_RANGE,       // Index/value exceeds bounds
    ERR_PARSE_FAILED,       // String/number parsing failed
    ERR_INVALID_FORMAT,     // Data format corrupted/invalid
    ERR_INVALID_STATE,      // Operation invalid in current state

    /* Resource and memory errors (100-199) */
    ERR_OUT_OF_MEMORY,      // Allocation failed
    ERR_BUFFER_TOO_SMALL,   // Buffer too small for result
    ERR_CAPACITY_EXCEEDED,  // Fixed container/resource full
    ERR_NOT_FOUND,          // Key/resource not found

    /* I/O and system errors (200-299) */
    ERR_IO_FAILED,          // File/network/device I/O error
    ERR_PERMISSION,         // Access denied
    ERR_TIMEOUT,            // Operation timed out
    ERR_NOT_SUPPORTED,      // Feature unavailable on platform
    ERR_ALREADY_EXISTS,     // Resource/file already exists

    /* Arithmetic errors (300-399) */
    ERR_OVERFLOW,           // Numeric overflow
    ERR_UNDERFLOW,          // Numeric underflow
    ERR_DIVIDE_BY_ZERO,     // Division/modulo by zero

    /* Generic/miscellaneous (400+) */
    ERR_UNKNOWN,            // Catch-all / unspecified error
    ERR_NOT_IMPLEMENTED,    // Feature stubbed / not complete

    /* Sentinel — tracks total count. Do not use as real error. */
    ERR_COUNT
} Error;

/* ════════════════════════════════════════════════════════════════════════════
   ERROR MESSAGE AND UTILITY FUNCTIONS
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Returns a brief, human-readable string for an error code
 *
 * Returns static string literal — thread-safe, never NULL, lifetime of program.
 * Strings are intentionally short — add context at call sites.
 *
 * @param e Error code
 * @return Static description string ("Unknown error" for invalid values)
 */
static inline const char* error_message(Error e) {
    switch (e) {
        case ERR_OK:                return "No error";
        case ERR_INVALID_ARG:       return "Invalid argument";
        case ERR_OUT_OF_RANGE:      return "Value out of range";
        case ERR_PARSE_FAILED:      return "Parse failed";
        case ERR_INVALID_FORMAT:    return "Invalid format";
        case ERR_INVALID_STATE:     return "Invalid state";
        case ERR_OUT_OF_MEMORY:     return "Out of memory";
        case ERR_BUFFER_TOO_SMALL:  return "Buffer too small";
        case ERR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case ERR_NOT_FOUND:         return "Not found";
        case ERR_IO_FAILED:         return "I/O operation failed";
        case ERR_PERMISSION:        return "Permission denied";
        case ERR_TIMEOUT:           return "Operation timed out";
        case ERR_NOT_SUPPORTED:     return "Not supported";
        case ERR_ALREADY_EXISTS:    return "Already exists";
        case ERR_OVERFLOW:          return "Numeric overflow";
        case ERR_UNDERFLOW:         return "Numeric underflow";
        case ERR_DIVIDE_BY_ZERO:    return "Division by zero";
        case ERR_UNKNOWN:           return "Unknown error";
        case ERR_NOT_IMPLEMENTED:   return "Not implemented";
        case ERR_COUNT:             return "Invalid error code (ERR_COUNT)";
        default:                    return "Unknown error";
    }
}

/**
 * @brief Checks if error code represents success
 */
static inline bool error_is_ok(Error e) {
    return e == ERR_OK;
}

/**
 * @brief Checks if error code is within valid range
 */
static inline bool error_is_valid(Error e) {
    return e >= ERR_OK && e < ERR_COUNT;
}

/**
 * @brief Returns error code as plain integer
 */
static inline int error_code(Error e) {
    return (int)e;
}

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS FOR RESULT<T, Error>
   ════════════════════════════════════════════════════════════════════════════ */
/** @brief Shorthand for result_T_Error_ok(val) */
#define RESULT_OK(T, val)   result_##T##_Error_ok(val)

/** @brief Shorthand for result_T_Error_err(err_code) */
#define RESULT_ERR(T, code) result_##T##_Error_err(code)

/** @brief Returns error message if result is Err, NULL if Ok */
#define RESULT_ERROR_MSG(T, res) \
    (result_##T##_Error_is_err(res) ? \
        error_message(result_##T##_Error_unwrap_err(res)) : NULL)

#endif /* CANON_SEMANTICS_ERROR_H */
