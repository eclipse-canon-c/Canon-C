#ifndef CANON_SEMANTICS_ERROR_H
#define CANON_SEMANTICS_ERROR_H

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fikret Güney Ersezer
//
// This file is licensed under MIT (not the project-default MPL-2.0).
//
// Reason: error.h defines the canonical Error enum and error_message()
// used across the entire library. The extension guide explicitly invites
// users to add new error codes before ERR_COUNT, which constitutes a
// direct modification to this file. Additionally, domain-specific
// deployments — embedded firmware, protocol stacks, safety systems —
// routinely need to remap, remove, or add error codes to match platform
// constraints or interface requirements. Requiring those modifications
// to be disclosed under MPL-2.0 would penalize exactly the users for
// whom this file was designed to be extensible. MIT makes the intended
// extension path legally unambiguous.

/*
 *
 * Canon-C -- error.h
 * Common error codes and human-readable messages.
 *
 * Requires C99 or later. No platform-specific code.
 * Depends on core/primitives/types.h from this library.
 *
 * -- Portability --------------------------------------------------------------
 * Tested with GCC, Clang, and MSVC on Windows, Linux, and macOS.
 *
 * -- Thread-safety ------------------------------------------------------------
 * All functions are thread-safe: pure/const, no shared mutable state.
 *
 * -- Performance --------------------------------------------------------------
 * Time:  O(1) for all operations.
 * Space: O(1) -- static data only, no allocations.
 * Error representation: plain integer (enum).
 * Message lookup: switch-based (typically compiled to a jump table).
 *
 * -- Design -------------------------------------------------------------------
 * - Simple flat enum (no hierarchies).
 * - Zero runtime cost for error representation.
 * - Consistent messages via error_message().
 * - Easy to extend: add new codes before ERR_COUNT.
 * - Thread-safe pure functions.
 * - Generic enough for cross-domain use.
 * - Brief messages -- add context at call sites.
 * - No result.h dependency in this file. For Result<T, Error> integration,
 *   include "error_result.h" separately (opt-in).
 *
 * -- Error code ranges --------------------------------------------------------
 *   0:       Success (ERR_OK)
 *   1-99:    Argument / input errors
 *   100-199: Resource / memory errors
 *   200-299: I/O and system errors
 *   300-399: Arithmetic errors
 *   400+:    Generic / miscellaneous
 *
 * -- Extension guide ----------------------------------------------------------
 * 1. Add new code before ERR_COUNT in the enum below.
 * 2. Add a matching case in error_message().
 * 3. (Optional) Define a domain-specific enum (e.g. ErrorJson) for subsystems.
 *
 * -- Typical use --------------------------------------------------------------
 *
 *   Error validate_age(int age) {
 *       if (age < 0 || age > 150) return ERR_OUT_OF_RANGE;
 *       return ERR_OK;
 *   }
 *
 *   Error e = validate_age(input);
 *   if (e != ERR_OK) {
 *       fprintf(stderr, "validation failed: %s\n", error_message(e));
 *   }
 */

#include "core/primitives/types.h" /* bool, usize, u8 ... i64, f32, f64 -- Canon-C base types */

/* ============================================================================
   ERROR CODE ENUMERATION
   ============================================================================ */

/**
 * @brief Common error codes used across the library.
 *
 * ERR_OK (0) represents success. It is rarely used as the error variant of a
 * Result type; its primary use is as a direct return value or sentinel.
 *
 * Add new domain-specific codes before ERR_COUNT. Keep the list small and
 * general-purpose; prefer a separate enum for highly domain-specific errors.
 *
 * Note: intentional gaps exist between ranges (e.g. 6-99, 106-199) to allow
 * future codes to be added within each range without breaking ABI. Gap values
 * are NOT valid error codes -- error_in_range() returns true for them because
 * it checks the integer range, not enum membership. error_message() returns
 * "Unknown error" for gap values via the default branch.
 */
typedef enum {
    /* Success */
    ERR_OK = 0,

    /* Argument and input validation errors (1-99) */
    ERR_INVALID_ARG,        /* NULL pointer, invalid value, precondition failed */
    ERR_OUT_OF_RANGE,       /* Index or value exceeds valid bounds              */
    ERR_PARSE_FAILED,       /* String or number parsing failed                  */
    ERR_INVALID_FORMAT,     /* Data format is corrupted or unrecognised         */
    ERR_INVALID_STATE,      /* Operation is invalid in the current state        */

    /* Resource and memory errors (100-199) */
    ERR_OUT_OF_MEMORY  = 100, /* Allocation failed                              */
    ERR_BUFFER_TOO_SMALL,     /* Provided buffer too small to hold the result   */
    ERR_CAPACITY_EXCEEDED,    /* Fixed container or resource is full            */
    ERR_NOT_FOUND,            /* Requested key, item, or resource not found     */

    /* I/O and system errors (200-299) */
    ERR_IO_FAILED      = 200, /* File, network, or device I/O error             */
    ERR_PERMISSION,           /* Access denied                                  */
    ERR_TIMEOUT,              /* Operation timed out                            */
    ERR_NOT_SUPPORTED,        /* Feature unavailable on this platform           */
    ERR_ALREADY_EXISTS,       /* Resource or file already exists                */

    /* Arithmetic errors (300-399) */
    ERR_OVERFLOW       = 300, /* Numeric overflow                               */
    ERR_UNDERFLOW,            /* Numeric underflow                              */
    ERR_DIVIDE_BY_ZERO,       /* Division or modulo by zero                     */

    /* Generic / miscellaneous (400+) */
    ERR_UNKNOWN        = 400, /* Catch-all / unspecified error                  */
    ERR_NOT_IMPLEMENTED,      /* Feature is stubbed or not yet complete         */

    /*
     * Sentinel -- do not use as a real error code.
     * Tracks the total count of defined codes for validation.
     */
    ERR_COUNT
} Error;

/* ============================================================================
   ERROR MESSAGE AND UTILITY FUNCTIONS
   ============================================================================ */

/**
 * @brief Returns a brief, human-readable string for an error code.
 *
 * Returns a static string literal. Never returns NULL. Lifetime is the entire
 * program. Thread-safe. Strings are intentionally short -- add context at the
 * call site.
 *
 * For codes outside the defined range, or for gap values within a range that
 * have not been assigned, returns "Unknown error".
 *
 * @param e  Error code to describe.
 * @return   Pointer to a static, null-terminated string. Never NULL.
 */
static inline const char *error_message(Error e) {
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
 * @brief Returns true if the error code represents success (ERR_OK).
 *
 * @param e  Error code to test.
 * @return   true if e == ERR_OK, false otherwise.
 */
static inline bool error_is_ok(Error e) {
    return e == ERR_OK;
}

/**
 * @brief Returns true if the integer value of e falls within [ERR_OK, ERR_COUNT).
 *
 * This checks the integer range only -- it does NOT verify that e is a named,
 * assigned enum member. Gap values between ranges (e.g. 6-99, 106-199) pass
 * this check even though they are not defined error codes. For those values,
 * error_message() returns "Unknown error" via the default branch.
 *
 * Use this function to guard against wildly out-of-range or negative values
 * (possible via explicit cast to Error in C). Do not use it to assert that a
 * code is a known, meaningful error -- compare directly against the enum
 * members instead.
 *
 * Note: ERR_COUNT itself is excluded (returns false).
 *
 * @param e  Error code to check.
 * @return   true if (int)e is in [0, ERR_COUNT), false otherwise.
 */
static inline bool error_in_range(Error e) {
    return (int)e >= (int)ERR_OK && (int)e < (int)ERR_COUNT;
}

/**
 * @brief Returns the error code as a plain int.
 *
 * Useful for logging, protocol encoding, or interoperability with APIs that
 * expect integer error codes.
 *
 * @param e  Error code.
 * @return   Integer representation of e.
 */
static inline int error_code(Error e) {
    return (int)e;
}

/*
 * -- Result<T, Error> integration ---------------------------------------------
 *
 * Convenience macros for use with result.h are intentionally NOT included here.
 * Including them would couple this header to result.h and to the token-pasting
 * naming convention used by CANON_RESULT, making this file's behavior depend
 * on a separate abstraction that cannot be understood by reading this header
 * alone.
 *
 * To use Error with result.h, include "error_result.h" (opt-in):
 *
 *   #include "result/result.h"
 *   #include "semantics/error_result.h"
 *
 * error_result.h defines RESULT_OK, RESULT_ERR, and RESULT_ERROR_MSG in terms
 * of both headers, keeping each file's responsibilities clear and separate.
 */

#endif /* CANON_SEMANTICS_ERROR_H */
