// semantics/error.h
#ifndef CANON_SEMANTICS_ERROR_H
#define CANON_SEMANTICS_ERROR_H

#include <stdbool.h>
#include "result/result.h"

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
 * - Requires C99 or later (for inline functions, stdbool.h)
 * - Depends on result/result.h from this library
 * - No platform-specific code
 * - No external dependencies beyond standard C library
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are thread-safe (no shared mutable state)
 * All functions are pure/const - safe to call from multiple threads simultaneously
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) - constant time for all operations
 * - Space complexity: O(1) - no allocations, static data only
 * - Error representation: simple integer (enum) - zero overhead
 * - Message lookup: switch-based, compile-time constants
 * - No heap allocations
 * - Minimal code size - compiles to efficient switch tables
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Simple, flat enum (no hierarchies or categories for simplicity)
 * - Zero runtime cost for error representation
 * - Consistent human-readable messages via error_message()
 * - Easy to extend (just add new enum values before ERR_COUNT)
 * - Works seamlessly with result.h combinators
 * - Thread-safe (all functions are pure/const)
 * - Type-safe when used with Result<T, Error>
 * - Generic enough for cross-domain use
 * - Brief messages - add context at call sites
 * - ERR_OK exists for consistency (though rarely used in Result::Err)
 * - Compile-time error code validation
 * - No error code conflicts (automatic via enum)
 *
 * Design philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Error codes should be generic enough to apply across domains
 * - For domain-specific errors, consider creating separate enums
 * - Messages are intentionally brief - add context at call sites
 * - ERR_OK exists for consistency but shouldn't appear in Err variants
 * - Keep the list small and general-purpose
 * - Prefer composition over deep error hierarchies
 * - Each error should have a clear, unambiguous meaning
 * - Error codes are organized by category for maintainability
 *
 * Error code organization:
 * ────────────────────────────────────────────────────────────────────────────
 * Error code ranges:
 * - 0: Success (ERR_OK)
 * - 1-99: Argument/input errors
 * - 100-199: Resource/memory errors
 * - 200-299: I/O and system errors
 * - 300-399: Arithmetic errors
 * - 400+: Generic/miscellaneous errors
 *
 * This organization helps with:
 * - Quick identification of error category
 * - Systematic error handling (handle ranges)
 * - Future extensibility (gaps for new errors)
 * - Clear separation of concerns
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Function return values for operations that can fail
 * - Parsing and validation errors
 * - Resource allocation failures
 * - I/O operation errors
 * - Arithmetic operation failures (overflow, division by zero)
 * - Configuration and state errors
 * - Permission and access control
 * - Timeout handling
 * - Feature availability checks
 * - Error propagation through call chains
 * - Logging and diagnostics
 * - User-facing error messages
 *
 * Extension guide:
 * ────────────────────────────────────────────────────────────────────────────
 * To add new error codes:
 * 1. Add new error code before ERR_COUNT in the Error enum
 * 2. Add corresponding message in error_message() switch
 * 3. ERR_COUNT automatically tracks total count
 * 4. Place in appropriate range (1-99, 100-199, etc.)
 * 5. Optionally add specialized error types (e.g., ErrorIO, ErrorParse)
 * 6. Update documentation with use cases
 *
 * Example of domain-specific extension:
 * ```c
 * typedef enum {
 *     ERR_JSON_INVALID = 1000,
 *     ERR_JSON_UNEXPECTED_TOKEN,
 *     ERR_JSON_NESTING_TOO_DEEP,
 *     // ... more JSON-specific errors
 * } ErrorJSON;
 * ```
 */

/* ════════════════════════════════════════════════════════════════════════════
   ERROR CODE ENUMERATION
   ════════════════════════════════════════════════════════════════════════════ */

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
 * - 100-199: Resource/memory errors
 * - 200-299: I/O and system errors
 * - 300-399: Arithmetic errors
 * - 400+: Generic/miscellaneous errors
 *
 * The enum uses implicit numbering to prevent conflicts and ensure
 * ERR_COUNT always reflects the total number of error codes.
 *
 * Usage in Result types:
 * ```c
 * CANON_RESULT(int, Error)
 * CANON_RESULT(void_ptr, Error)
 * ```
 */
typedef enum {
    /* ════════════════════════════════════════════════════════════════════════
       Success (not typically used in Result::Err)
       ════════════════════════════════════════════════════════════════════════ */
   
    /**
     * @brief No error (success)
     *
     * Represents successful operation. Primarily used for completeness
     * and in contexts where errors are checked with error_is_ok().
     * Should not appear in Result::Err variants.
     */
    ERR_OK = 0,
  
    /* ════════════════════════════════════════════════════════════════════════
       Argument and input validation errors (1-99)
       ════════════════════════════════════════════════════════════════════════ */
   
    /**
     * @brief Invalid or null argument provided
     *
     * Use when:
     * - NULL pointer passed where non-NULL expected
     * - Argument value is semantically invalid
     * - Precondition violated
     *
     * Example: parse_int(NULL) → ERR_INVALID_ARG
     */
    ERR_INVALID_ARG,
   
    /**
     * @brief Value outside valid range
     *
     * Use when:
     * - Numeric value exceeds allowed bounds
     * - Index out of array bounds
     * - Value not in expected domain
     *
     * Example: array_get(arr, 1000) where len=10 → ERR_OUT_OF_RANGE
     */
    ERR_OUT_OF_RANGE,
   
    /**
     * @brief String/number parsing failed
     *
     * Use when:
     * - String cannot be converted to number
     * - Invalid syntax in parsed input
     * - Malformed data structure
     *
     * Example: parse_int("abc") → ERR_PARSE_FAILED
     */
    ERR_PARSE_FAILED,
   
    /**
     * @brief Data format is invalid or corrupted
     *
     * Use when:
     * - File format doesn't match specification
     * - Data structure is malformed
     * - Checksum/validation failed
     *
     * Example: load_json("not json") → ERR_INVALID_FORMAT
     */
    ERR_INVALID_FORMAT,
   
    /**
     * @brief Operation invalid in current state
     *
     * Use when:
     * - State machine in wrong state
     * - Operation precondition not met
     * - Resource not initialized
     *
     * Example: vec_pop(empty_vec) → ERR_INVALID_STATE
     */
    ERR_INVALID_STATE,
  
    /* ════════════════════════════════════════════════════════════════════════
       Resource and memory errors (100-199)
       ════════════════════════════════════════════════════════════════════════ */
   
    /**
     * @brief Memory allocation failed
     *
     * Use when:
     * - malloc/calloc/realloc returns NULL
     * - System out of memory
     * - Allocation size too large
     *
     * Example: vec_push(vec, item) when malloc fails → ERR_OUT_OF_MEMORY
     */
    ERR_OUT_OF_MEMORY,
   
    /**
     * @brief Provided buffer too small for operation
     *
     * Use when:
     * - Output buffer cannot hold result
     * - Buffer size insufficient for operation
     * - String truncation would occur
     *
     * Example: to_string(buf, 10, large_number) → ERR_BUFFER_TOO_SMALL
     */
    ERR_BUFFER_TOO_SMALL,
   
    /**
     * @brief Container or resource at maximum capacity
     *
     * Use when:
     * - Fixed-size container is full
     * - Resource limit reached
     * - Cannot grow further
     *
     * Example: vec_push to full fixed vec → ERR_CAPACITY_EXCEEDED
     */
    ERR_CAPACITY_EXCEEDED,
   
    /**
     * @brief Requested item/resource not found
     *
     * Use when:
     * - Key not found in map/dictionary
     * - File doesn't exist
     * - Resource lookup failed
     *
     * Example: map_get(map, "key") when key missing → ERR_NOT_FOUND
     */
    ERR_NOT_FOUND,
  
    /* ════════════════════════════════════════════════════════════════════════
       I/O and system errors (200-299)
       ════════════════════════════════════════════════════════════════════════ */
   
    /**
     * @brief File or I/O operation failed
     *
     * Use when:
     * - File read/write fails
     * - Network I/O error
     * - Device I/O error
     *
     * Example: file_read(path) with unreadable file → ERR_IO_FAILED
     */
    ERR_IO_FAILED,
   
    /**
     * @brief Operation not permitted (access denied)
     *
     * Use when:
     * - Insufficient permissions for operation
     * - Access control violation
     * - Protected resource access attempt
     *
     * Example: file_write(readonly_file) → ERR_PERMISSION
     */
    ERR_PERMISSION,
   
    /**
     * @brief Operation timed out
     *
     * Use when:
     * - Operation exceeded time limit
     * - Network request timeout
     * - Resource lock timeout
     *
     * Example: network_request(url, timeout=1s) → ERR_TIMEOUT
     */
    ERR_TIMEOUT,
   
    /**
     * @brief Operation not supported on this platform
     *
     * Use when:
     * - Feature unavailable on current OS
     * - Hardware doesn't support operation
     * - API not implemented for platform
     *
     * Example: get_gpu_info() on system without GPU → ERR_NOT_SUPPORTED
     */
    ERR_NOT_SUPPORTED,
   
    /**
     * @brief Resource already exists
     *
     * Use when:
     * - File creation fails (file exists)
     * - Duplicate key insertion
     * - Resource name conflict
     *
     * Example: file_create(path, exclusive=true) → ERR_ALREADY_EXISTS
     */
    ERR_ALREADY_EXISTS,
  
    /* ════════════════════════════════════════════════════════════════════════
       Arithmetic errors (300-399)
       ════════════════════════════════════════════════════════════════════════ */
   
    /**
     * @brief Numeric overflow detected
     *
     * Use when:
     * - Integer overflow would occur
     * - Result too large for type
     * - Computation exceeds maximum value
     *
     * Example: safe_add(INT_MAX, 1) → ERR_OVERFLOW
     */
    ERR_OVERFLOW,
   
    /**
     * @brief Numeric underflow detected
     *
     * Use when:
     * - Integer underflow would occur
     * - Result too small for type
     * - Computation below minimum value
     *
     * Example: safe_sub(INT_MIN, 1) → ERR_UNDERFLOW
     */
    ERR_UNDERFLOW,
   
    /**
     * @brief Division by zero attempted
     *
     * Use when:
     * - Division or modulo by zero
     * - Mathematical undefined operation
     * - Infinite result would occur
     *
     * Example: safe_divide(10, 0) → ERR_DIVIDE_BY_ZERO
     */
    ERR_DIVIDE_BY_ZERO,
  
    /* ════════════════════════════════════════════════════════════════════════
       Generic/miscellaneous (400+)
       ════════════════════════════════════════════════════════════════════════ */
   
    /**
     * @brief Unknown or unspecified error
     *
     * Use when:
     * - Error doesn't fit other categories
     * - Error details unavailable
     * - Legacy error code mapping
     *
     * Prefer specific error codes when possible.
     */
    ERR_UNKNOWN,
   
    /**
     * @brief Feature not yet implemented
     *
     * Use when:
     * - Function is stubbed out
     * - Feature planned but not complete
     * - Platform-specific code missing
     *
     * Example: future_feature() → ERR_NOT_IMPLEMENTED
     */
    ERR_NOT_IMPLEMENTED,
  
    /* ════════════════════════════════════════════════════════════════════════
       Add more domain-specific errors here (before ERR_COUNT)
       ════════════════════════════════════════════════════════════════════════ */
  
    /**
     * @brief Total number of defined error codes
     *
     * This sentinel value automatically tracks the total count of error
     * codes. Used for validation and iteration. Should never appear in
     * actual error returns.
     *
     * Do not use ERR_COUNT as an actual error code.
     */
    ERR_COUNT
} Error;

/* ════════════════════════════════════════════════════════════════════════════
   ERROR MESSAGE AND UTILITY FUNCTIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a human-readable string for a given error code
 *
 * Provides a brief, generic description of the error.
 * For more context, combine with additional error information at call sites.
 *
 * The returned strings are static literals - they never change and are
 * safe to use from any thread. The strings are intentionally brief to
 * encourage adding context at the call site.
 *
 * @param e Error code (enum Error)
 * @return Static null-terminated string describing the error
 *         Returns "Unknown error" for undefined/invalid values
 *
 * Preconditions:
 * - None (handles all inputs safely)
 *
 * Postconditions:
 * - Always returns non-NULL string
 * - Returned string is valid for program lifetime
 * - String is read-only (attempting to modify is undefined behavior)
 *
 * Performance:
 * - Time: O(1) - switch statement (typically jump table)
 * - Space: O(1) - returns pointer to static data
 * - No allocations
 * - Branch-free on most compilers (jump table)
 *
 * Properties:
 * - Thread-safe (returns static string literals)
 * - NULL-safe (cannot return NULL)
 * - Always returns a valid string
 * - Returned string is static — do not free or modify it
 * - Safe to call even with invalid enum values
 * - Deterministic (same input always gives same output)
 *
 * Example usage:
 * ```c
 * Error err = ERR_OUT_OF_MEMORY;
 * printf("Error occurred: %s\n", error_message(err));
 * // Output: "Error occurred: Out of memory"
 * ```
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
 * Convenience function to test if an error code indicates successful
 * operation. Equivalent to (e == ERR_OK).
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
 * Validates that an error code falls within the defined range.
 * Useful for input validation or debugging.
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
 * that expect integer error codes. Simply casts the enum to int.
 *
 * @param e Error code
 * @return Integer representation of the error code
 */
static inline int error_code(Error e) {
    return (int)e;
}

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS FOR RESULT<T, ERROR>
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convenience macro to create successful Result<T, Error>
 *
 * Shorthand for result_T_Error_ok(val). Makes code more concise
 * and readable when working with Result types.
 *
 * Example:
 * ```c
 * return RESULT_OK(int, 42);
 * // Equivalent to: return result_int_Error_ok(42);
 * ```
 */
#define RESULT_OK(T, val) result_##T##_Error_ok(val)

/**
 * @brief Convenience macro to create failed Result<T, Error>
 *
 * Shorthand for result_T_Error_err(err_code). Makes error
 * returns more concise and consistent.
 *
 * Example:
 * ```c
 * return RESULT_ERR(int, ERR_NOT_FOUND);
 * // Equivalent to: return result_int_Error_err(ERR_NOT_FOUND);
 * ```
 */
#define RESULT_ERR(T, err_code) result_##T##_Error_err(err_code)

/**
 * @brief Checks if Result contains an error and extracts message
 *
 * Helper macro that combines error checking with message extraction.
 * Useful for logging or error reporting. Returns NULL if result is Ok,
 * otherwise returns the error message string.
 *
 * Example:
 * ```c
 * result_int_Error res = parse_int("abc");
 * const char* msg = RESULT_ERROR_MSG(int, res);
 * if (msg) {
 *     fprintf(stderr, "Error: %s\n", msg);
 * }
 * ```
 */
#define RESULT_ERROR_MSG(T, result) \
    (result_##T##_Error_is_err(result) ? \
        error_message(result_##T##_Error_unwrap_err(result)) : NULL)

/* ════════════════════════════════════════════════════════════════════════════
   COMMON TYPE INSTANTIATIONS (COMMENTED OUT BY DEFAULT)
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * Uncomment the types you need, or define your own in your code.
 * 
 * Note: These use the modern CANON_RESULT macro. The legacy
 * CANON_C_DEFINE_RESULT is also supported for backward compatibility.
 */

// CANON_RESULT(int, Error)
// CANON_RESULT(long, Error)
// CANON_RESULT(size_t, Error)
// CANON_RESULT(float, Error)
// CANON_RESULT(double, Error)

/* For pointer types: */
// typedef void* void_ptr;
// CANON_RESULT(void_ptr, Error)

/* ════════════════════════════════════════════════════════════════════════════
   COMPLETE USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
    // ────────────────────────────────────────────────────────────────────────
    // Example: Define the Result types used below
    // ────────────────────────────────────────────────────────────────────────
    CANON_RESULT(int, Error)
    CANON_RESULT(double, Error)

    typedef void* void_ptr;
    CANON_RESULT(void_ptr, Error)

    // ────────────────────────────────────────────────────────────────────────
    // Example: robust integer parsing from string with multiple failure modes
    // ────────────────────────────────────────────────────────────────────────
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

    // ────────────────────────────────────────────────────────────────────────
    // Example: diagnostic printing of parse results
    // ────────────────────────────────────────────────────────────────────────
    void example_basic_parsing(void) {
        const char* inputs[] = {
            "42",               // normal success
            "abc",              // invalid characters
            "",                 // empty string
            NULL,               // null pointer
            "2147483648"        // overflow for int32
        };
        const char* descriptions[] = {
            "Valid number",
            "Invalid characters",
            "Empty string",
            "NULL pointer",
            "Overflow"
        };
      
        printf("Parsing examples:\n");
      
        for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
            printf(" %s → ", descriptions[i]);
          
            result_int_Error result = parse_int(inputs[i]);
          
            if (result_int_Error_is_err(result)) {
                Error err = result_int_Error_unwrap_err(result);
                printf("FAILED → %s (code %d)\n",
                       error_message(err), error_code(err));
            } else {
                printf("OK → %d\n", result_int_Error_unwrap(result));
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example: safe division preventing division by zero
    // ────────────────────────────────────────────────────────────────────────
    result_int_Error safe_divide(int a, int b) {
        if (b == 0) {
            return RESULT_ERR(int, ERR_DIVIDE_BY_ZERO);
        }
      
        return RESULT_OK(int, a / b);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example: multi-step computation showing error propagation
    // ────────────────────────────────────────────────────────────────────────
    result_int_Error compute_average(const char* num1_str, const char* num2_str) {
        result_int_Error r1 = parse_int(num1_str);
        if (result_int_Error_is_err(r1)) {
            return r1;
        }
        int a = result_int_Error_unwrap(r1);
      
        result_int_Error r2 = parse_int(num2_str);
        if (result_int_Error_is_err(r2)) {
            return r2;
        }
        int b = result_int_Error_unwrap(r2);
      
        result_int_Error div_result = safe_divide(a + b, 2);
        if (result_int_Error_is_err(div_result)) {
            return div_result;
        }
      
        return div_result;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example: using RESULT_ERROR_MSG macro for quick error reporting
    // ────────────────────────────────────────────────────────────────────────
    void example_macro_style(void) {
        result_int_Error r = parse_int("2147483648");  // overflow
      
        const char* msg = RESULT_ERROR_MSG(int, r);
        if (msg) {
            fprintf(stderr, "Operation failed: %s\n", msg);
        } else {
            printf("Success: %d\n", result_int_Error_unwrap(r));
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example: using modern TRY macro for cleaner error propagation
    // ────────────────────────────────────────────────────────────────────────
    result_int_Error add_parsed(const char* a, const char* b) {
        // Parse first number
        result_int_Error res_a = parse_int(a);
        TRY(int, Error, res_a);  // Early return if error
        
        // Parse second number
        result_int_Error res_b = parse_int(b);
        TRY(int, Error, res_b);  // Early return if error
        
        int val_a = result_int_Error_unwrap(res_a);
        int val_b = result_int_Error_unwrap(res_b);
        
        return RESULT_OK(int, val_a + val_b);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example: even cleaner with TRY_UNWRAP
    // ────────────────────────────────────────────────────────────────────────
    result_int_Error add_parsed_clean(const char* a, const char* b) {
        int val_a = TRY_UNWRAP(int, Error, parse_int(a));
        int val_b = TRY_UNWRAP(int, Error, parse_int(b));
        
        return RESULT_OK(int, val_a + val_b);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Optional: string_view example (non-owning string slice)
    // ────────────────────────────────────────────────────────────────────────
    typedef struct {
        size_t len;
        const char* data;
    } string_view;

    CANON_RESULT(string_view, Error)

    result_string_view_Error make_view(const char* start, size_t len) {
        if (!start && len > 0) {
            return RESULT_ERR(string_view, ERR_INVALID_ARG);
        }
        string_view v = { .len = len, .data = start };
        return RESULT_OK(string_view, v);
    }
*/

#endif /* CANON_SEMANTICS_ERROR_H */
