// semantics/error.h
#ifndef CANON_SEMANTICS_ERROR_H
#define CANON_SEMANTICS_ERROR_H
#include <stdbool.h>
#include "result.h"
/**
 * @file error.h
 * @brief Common error codes and human-readable messages
 *
 * Provides a small, extensible set of standard error codes intended for use
 * with Result<T, Error> throughout the library.
 *
 * Key design goals:
 * - Simple, flat enum (no hierarchies or categories for simplicity)
 * - Zero runtime cost for error representation
 * - Consistent human-readable messages via error_message()
 * - Easy to extend (just add new enum values before ERR_COUNT)
 * - Works seamlessly with result.h combinators
 * - Thread-safe (all functions are pure/const)
 *
 * Portability:
 * - Requires C99 or later (for inline functions)
 * - Depends on result.h from this library
 * - No platform-specific code
 *
 * Thread-safety: All functions are thread-safe (no shared mutable state)
 *
 * Performance:
 * - Time complexity: O(1) — constant time for all operations
 * - Space complexity: O(1) — no allocations, static data only
 * - Error representation: simple integer (enum)
 * - Message lookup: switch-based, compile-time constants
 *
 * Extension guide:
 * 1. Add new error code before ERR_COUNT in the Error enum
 * 2. Add corresponding message in error_message() switch
 * 3. Update ERR_COUNT automatically tracks total
 * 4. Optionally add specialized error types (e.g., ErrorIO, ErrorParse)
 *
 * Recommended usage pattern:
 * // Define Result types
 * CANON_C_DEFINE_RESULT(int, Error)
 *
 * result_int_Error parse_int(const char* s) {
 *     if (!s) return RESULT_ERR(int, ERR_INVALID_ARG);
 *     // ... parsing logic ...
 *     if (parse_failed) return RESULT_ERR(int, ERR_PARSE_FAILED);
 *     return RESULT_OK(int, value);
 * }
 *
 * // Usage:
 * result_int_Error r = parse_int("42");
 * if (result_int_Error_is_err(r)) {
 *     Error err = result_int_Error_unwrap_err(r);
 *     fprintf(stderr, "Error: %s\n", error_message(err));
 *     return -1;
 * }
 * int value = result_int_Error_unwrap(r);
 *
 * Design philosophy:
 * - Error codes should be generic enough to apply across domains
 * - For domain-specific errors, consider creating separate enums
 * - Messages are intentionally brief - add context at call sites
 * - ERR_OK exists for consistency but shouldn't appear in Err variants
 */

/**
 * @brief Common error codes used across the library
 *
 * General-purpose error codes suitable for most applications.
 * Always use ERR_OK = 0 for success (though it shouldn't appear in Result::Err).
 * Add new specific errors before ERR_COUNT when needed.
 * Keep the list small and general-purpose.
 *
 * Error code ranges:
 * - 0: Success (ERR_OK)
 * - 1-99: Argument/input errors
 * - 100+: Runtime/resource errors
 *
 * Usage in Result types:
 * CANON_C_DEFINE_RESULT(int, Error)
 * CANON_C_DEFINE_RESULT(void_ptr, Error)
 */
typedef enum {
    /* Success (not typically used in Result::Err) */
    ERR_OK = 0, ///< No error (success) - for completeness
   
    /* Argument and input validation errors (1-99) */
    ERR_INVALID_ARG,      ///< Invalid or null argument provided
    ERR_OUT_OF_RANGE,     ///< Value outside valid range
    ERR_PARSE_FAILED,     ///< String/number parsing failed
    ERR_INVALID_FORMAT,   ///< Data format is invalid or corrupted
    ERR_INVALID_STATE,    ///< Operation invalid in current state
   
    /* Resource and memory errors (100-199) */
    ERR_OUT_OF_MEMORY,       ///< Memory allocation failed
    ERR_BUFFER_TOO_SMALL,    ///< Provided buffer too small for operation
    ERR_CAPACITY_EXCEEDED,   ///< Container or resource at maximum capacity
    ERR_NOT_FOUND,           ///< Requested item/resource not found
   
    /* I/O and system errors (200-299) */
    ERR_IO_FAILED,           ///< File or I/O operation failed
    ERR_PERMISSION,          ///< Operation not permitted (access denied)
    ERR_TIMEOUT,             ///< Operation timed out
    ERR_NOT_SUPPORTED,       ///< Operation not supported on this platform
    ERR_ALREADY_EXISTS,      ///< Resource already exists
   
    /* Arithmetic errors (300-399) */
    ERR_OVERFLOW,            ///< Numeric overflow detected
    ERR_UNDERFLOW,           ///< Numeric underflow detected
    ERR_DIVIDE_BY_ZERO,      ///< Division by zero attempted
   
    /* Generic/miscellaneous (400+) */
    ERR_UNKNOWN,             ///< Unknown or unspecified error
    ERR_NOT_IMPLEMENTED,     ///< Feature not yet implemented
   
    /* Add more domain-specific errors here (before ERR_COUNT)... */
   
    ERR_COUNT                ///< Total number of defined error codes (for validation)
} Error;

/**
 * @brief Returns a human-readable string for a given error code
 *
 * Provides a brief, generic description of the error.
 * For more context, combine with additional error information at call sites.
 *
 * @param e Error code (enum Error)
 * @return Static null-terminated string describing the error
 * Returns "Unknown error" for undefined/invalid values
 *
 * Properties:
 * - Thread-safe (returns static string literals)
 * - NULL-safe (cannot return NULL)
 * - Always returns a valid string
 * - Returned string is static — do not free or modify it
 * - Safe to call even with invalid enum values
 */
static inline const char* error_message(Error e) {
    switch (e) {
        /* Success */
        case ERR_OK:
            return "No error";
       
        /* Argument and input validation errors */
        case ERR_INVALID_ARG:
            return "Invalid argument";
        case ERR_OUT_OF_RANGE:
            return "Value out of range";
        case ERR_PARSE_FAILED:
            return "Parse failed";
        case ERR_INVALID_FORMAT:
            return "Invalid format";
        case ERR_INVALID_STATE:
            return "Invalid state";
       
        /* Resource and memory errors */
        case ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case ERR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case ERR_CAPACITY_EXCEEDED:
            return "Capacity exceeded";
        case ERR_NOT_FOUND:
            return "Not found";
       
        /* I/O and system errors */
        case ERR_IO_FAILED:
            return "I/O operation failed";
        case ERR_PERMISSION:
            return "Permission denied";
        case ERR_TIMEOUT:
            return "Operation timed out";
        case ERR_NOT_SUPPORTED:
            return "Not supported";
        case ERR_ALREADY_EXISTS:
            return "Already exists";
       
        /* Arithmetic errors */
        case ERR_OVERFLOW:
            return "Numeric overflow";
        case ERR_UNDERFLOW:
            return "Numeric underflow";
        case ERR_DIVIDE_BY_ZERO:
            return "Division by zero";
       
        /* Generic/miscellaneous */
        case ERR_UNKNOWN:
            return "Unknown error";
        case ERR_NOT_IMPLEMENTED:
            return "Not implemented";
       
        /* Sentinel */
        case ERR_COUNT:
            return "Invalid error code (ERR_COUNT)";
       
        /* Catch-all for invalid values */
        default:
            return "Unknown error";
    }
}

/**
 * @brief Checks if an error code represents success
 *
 * @param e Error code to check
 * @return true if e == ERR_OK, false otherwise
 */
static inline bool error_is_ok(Error e) {
    return e == ERR_OK;
}

/**
 * @brief Checks if an error code is valid (within defined range)
 *
 * @param e Error code to validate
 * @return true if e is between ERR_OK and ERR_COUNT-1, false otherwise
 */
static inline bool error_is_valid(Error e) {
    return e >= ERR_OK && e < ERR_COUNT;
}

/**
 * @brief Returns the error code value as an integer
 *
 * Useful for logging, serialization, or interfacing with C APIs
 * that expect integer error codes.
 *
 * @param e Error code
 * @return Integer representation of the error code
 */
static inline int error_code(Error e) {
    return (int)e;
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience macros for Result<T, Error>
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Convenience macro to create successful Result<T, Error>
 *
 * Shorthand for result_##T##_Error_ok(val)
 */
#define RESULT_OK(T, val) result_##T##_Error_ok(val)

/**
 * @brief Convenience macro to create failed Result<T, Error>
 *
 * Shorthand for result_##T##_Error_err(err_code)
 */
#define RESULT_ERR(T, err_code) result_##T##_Error_err(err_code)

/**
 * @brief Checks if Result contains an error and extracts message
 *
 * Helper macro that combines error checking with message extraction.
 * Useful for logging or error reporting.
 */
#define RESULT_ERROR_MSG(T, result) \
    (result_##T##_Error_is_err(result) ? \
        error_message(result_##T##_Error_unwrap_err(result)) : NULL)

/* ────────────────────────────────────────────────────────────────────────────
   Common type instantiations
   ──────────────────────────────────────────────────────────────────────────── */

/* Uncomment the types you need: */
// CANON_C_DEFINE_RESULT(int, Error)
// CANON_C_DEFINE_RESULT(long, Error)
// CANON_C_DEFINE_RESULT(size_t, Error)
// CANON_C_DEFINE_RESULT(float, Error)
// CANON_C_DEFINE_RESULT(double, Error)

/* typedef void* void_ptr; */
// CANON_C_DEFINE_RESULT(void_ptr, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ──────────────────────────────────────────────────────────────────────────── */

#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Define Result type */
CANON_C_DEFINE_RESULT(int, Error)

/* Function that can fail with standard error codes */
result_int_Error parse_int(const char* str) {
    if (!str) {
        return RESULT_ERR(int, ERR_INVALID_ARG);
    }
    
    if (strlen(str) == 0) {
        return RESULT_ERR(int, ERR_INVALID_FORMAT);
    }
    
    char* end;
    long val = strtol(str, &end, 10);
    
    if (end == str || *end != '\0') {
        return RESULT_ERR(int, ERR_PARSE_FAILED);
    }
    
    if (val > INT_MAX || val < INT_MIN) {
        return RESULT_ERR(int, ERR_OVERFLOW);
    }
    
    return RESULT_OK(int, (int)val);
}

result_int_Error safe_divide(int a, int b) {
    if (b == 0) {
        return RESULT_ERR(int, ERR_DIVIDE_BY_ZERO);
    }
    return RESULT_OK(int, a / b);
}

/* Example 1: Explicit error handling */
void example_explicit(void) {
    result_int_Error result = parse_int("42");
    
    if (result_int_Error_is_err(result)) {
        Error err = result_int_Error_unwrap_err(result);
        fprintf(stderr, "Parse failed: %s (code %d)\n",
                error_message(err), error_code(err));
        return;
    }
    
    int value = result_int_Error_unwrap(result);
    printf("Parsed value: %d\n", value);
}

/* Example 2: Using convenience macros */
void example_macros(void) {
    result_int_Error result = parse_int("not a number");
    
    const char* err_msg = RESULT_ERROR_MSG(int, result);
    if (err_msg) {
        fprintf(stderr, "Error: %s\n", err_msg);
    } else {
        printf("Success: %d\n", result_int_Error_unwrap(result));
    }
}

/* Example 3: Error propagation */
result_int_Error compute(const char* a_str, const char* b_str) {
    // Parse first number
    result_int_Error a_result = parse_int(a_str);
    if (result_int_Error_is_err(a_result)) {
        return a_result; // Propagate error
    }
    int a = result_int_Error_unwrap(a_result);
    
    // Parse second number
    result_int_Error b_result = parse_int(b_str);
    if (result_int_Error_is_err(b_result)) {
        return b_result; // Propagate error
    }
    int b = result_int_Error_unwrap(b_result);
    
    // Perform division
    return safe_divide(a, b);
}

/* Example 4: Error validation */
void example_validation(void) {
    Error err = ERR_OUT_OF_MEMORY;
    
    if (error_is_valid(err)) {
        printf("Valid error: %s\n", error_message(err));
    }
    
    if (!error_is_ok(err)) {
        printf("Error occurred: code %d\n", error_code(err));
    }
}

#endif /* CANON_SEMANTICS_ERROR_H */
