// semantics/error.h
#ifndef CANON_SEMANTICS_ERROR_H
#define CANON_SEMANTICS_ERROR_H

#include "semantics/result.h"

/**
 * @file error.h
 * @brief Common error codes and human-readable messages
 *
 * Provides a small, extensible set of standard error codes intended for use
 * with Result<T, Error> throughout the library.
 *
 * Key design goals:
 *   - Simple, flat enum (no hierarchies or categories yet)
 *   - Zero runtime cost for error representation
 *   - Consistent human-readable messages via error_message()
 *   - Easy to extend (just add new enum values before ERR_COUNT)
 *   - Works seamlessly with result.h combinators
 *
 * Recommended usage pattern:
 *   Result<int, Error> parse_int(const char* s) {
 *       if (!s) return RESULT_ERR(int, ERR_INVALID_ARG);
 *       // ... parsing logic ...
 *       return RESULT_OK(int, value);
 *   }
 *
 *   // Later:
 *   Result<int, Error> r = parse_int("42");
 *   if (result_is_err(r)) {
 *       printf("Error: %s\n", error_message(result_unwrap_err(r)));
 *   }
 */

/**
 * @brief Common error codes used across the library
 *
 * Always use ERR_OK = 0 for success (not an error).
 * Add new specific errors before ERR_COUNT when needed.
 * Keep the list small and general-purpose.
 */
typedef enum {
    ERR_OK = 0,              ///< No error (success)
    ERR_INVALID_ARG,         ///< Invalid or null argument
    ERR_OUT_OF_MEMORY,       ///< Memory allocation failed
    ERR_BUFFER_TOO_SMALL,    ///< Provided buffer too small for operation
    ERR_NOT_FOUND,           ///< Requested item/resource not found
    ERR_PARSE_FAILED,        ///< String/number parsing failed
    ERR_IO_FAILED,           ///< File or I/O operation failed
    ERR_OVERFLOW,            ///< Numeric overflow detected
    ERR_UNDERFLOW,           ///< Numeric underflow detected
    ERR_PERMISSION,          ///< Operation not permitted
    ERR_TIMEOUT,             ///< Operation timed out
    // Add more domain-specific errors here (before ERR_COUNT)...

    ERR_COUNT                ///< Total number of defined error codes (for validation)
} Error;

/**
 * @brief Returns a human-readable string for a given error code
 *
 * @param e Error code (enum Error)
 * @return  Static null-terminated string describing the error
 *          Returns "Unknown error" for undefined values
 *
 * Note: Returned string is static — do not free or modify it.
 * Always safe to call even with invalid enum values.
 */
static inline const char* error_message(Error e) {
    switch (e) {
        case ERR_OK:              return "No error";
        case ERR_INVALID_ARG:     return "Invalid argument";
        case ERR_OUT_OF_MEMORY:   return "Out of memory";
        case ERR_BUFFER_TOO_SMALL:return "Buffer too small";
        case ERR_NOT_FOUND:       return "Not found";
        case ERR_PARSE_FAILED:    return "Parse failed";
        case ERR_IO_FAILED:       return "I/O operation failed";
        case ERR_OVERFLOW:        return "Numeric overflow";
        case ERR_UNDERFLOW:       return "Numeric underflow";
        case ERR_PERMISSION:      return "Permission denied";
        case ERR_TIMEOUT:         return "Operation timed out";
        default:                  return "Unknown error";
    }
}

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

#endif /* CANON_SEMANTICS_ERROR_H */
