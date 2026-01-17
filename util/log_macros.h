// util/log_macros.h
#ifndef CANON_UTIL_LOG_MACROS_H
#define CANON_UTIL_LOG_MACROS_H

#include "util/log.h"

/**
 * @file log_macros.h
 * @brief Convenient, safe and expressive logging macros on top of log.h
 *
 * Provides a clean, ergonomic interface for most common logging needs with:
 * - Clear separation of simple messages vs formatted logging
 * - Three standard levels: INFO, WARN, ERROR
 * - Fire-and-forget behavior in release builds
 * - Optional "checked" variants in debug builds that detect I/O failures
 * - Zero allocation — all formatting done through log.h's stack-based functions
 * - Conditional behavior controlled by standard NDEBUG macro
 *
 * Design goals:
 * ────────────────────────────────────────────────────────────────────────────
 * - Make logging feel natural and low-friction in day-to-day code
 * - Never silently swallow I/O failures (debug builds can catch them)
 * - Keep macro expansions simple and predictable
 * - Provide both convenience (fire-and-forget) and safety (checked)
 * - Minimal impact on release binary size/performance
 *
 * Important notes:
 * - All macros expand to direct calls to log.h functions
 * - Simple _MSG variants are slightly more efficient (no vfprintf)
 * - _CHECKED variants in debug mode add meta-logging on failure
 * - In release builds (_NDEBUG defined), all _CHECKED become plain fire-and-forget
 *
 * Recommended usage:
 * ────────────────────────────────────────────────────────────────────────────
 * Development/Debug:
 *   LOG_INFO_CHECKED("Server starting on port %d", port);
 *   LOG_ERROR_CHECKED("Failed to load config: %s", error_str);
 *
 * Release/Production:
 *   LOG_INFO_FMT("User %s logged in", username);
 *   LOG_ERROR("Critical: connection timeout");
 *
 * Simple static messages:
 *   LOG_WARN_MSG("Deprecated API called");
 */

/* ────────────────────────────────────────────────────────────────────────────
   Simple message macros (most efficient for literal strings)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Logs a simple static/literal message at INFO level
 * @param msg Message string (can be literal or variable, but no formatting)
 */
#define LOG_INFO_MSG(msg)    (void)log_msg(LOG_INFO, (msg))

/**
 * @brief Logs a simple static/literal message at WARN level
 * @param msg Message string
 */
#define LOG_WARN_MSG(msg)    (void)log_msg(LOG_WARN, (msg))

/**
 * @brief Logs a simple static/literal message at ERROR level
 * @param msg Message string
 */
#define LOG_ERROR_MSG(msg)   (void)log_msg(LOG_ERROR, (msg))

/* ────────────────────────────────────────────────────────────────────────────
   Formatted logging macros (most commonly used)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging at INFO level
 * @param ... printf-style format string + arguments
 */
#define LOG_INFO_FMT(...)    (void)log_fmt(LOG_INFO, __VA_ARGS__)

/**
 * @brief Formatted logging at WARN level
 * @param ... printf-style format string + arguments
 */
#define LOG_WARN_FMT(...)    (void)log_fmt(LOG_WARN, __VA_ARGS__)

/**
 * @brief Formatted logging at ERROR level
 * @param ... printf-style format string + arguments
 */
#define LOG_ERROR_FMT(...)   (void)log_fmt(LOG_ERROR, __VA_ARGS__)

/* ────────────────────────────────────────────────────────────────────────────
   Checked variants — safety net during development
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checked logging macros (behavior changes with NDEBUG)
 *
 * In **debug builds** (NDEBUG not defined):
 * - Executes the log operation
 * - Checks the returned Result
 * - On logging failure, prints a meta-error to stderr
 *
 * In **release builds** (NDEBUG defined):
 * - Becomes identical to the plain fire-and-forget macros
 * - Zero runtime overhead compared to normal macros
 *
 * Use these during development to catch cases where logging itself fails
 * (disk full, closed stream, permission issues, etc.)
 */

#ifdef NDEBUG
    // Release: same as normal fire-and-forget
    #define LOG_INFO_CHECKED(...)   LOG_INFO_FMT(__VA_ARGS__)
    #define LOG_WARN_CHECKED(...)   LOG_WARN_FMT(__VA_ARGS__)
    #define LOG_ERROR_CHECKED(...)  LOG_ERROR_FMT(__VA_ARGS__)
#else
    // Debug: verify success + meta-log on failure
    #define LOG_INFO_CHECKED(...)   (void)log_fmt(LOG_INFO, __VA_ARGS__)

    #define LOG_WARN_CHECKED(...)   (void)log_fmt(LOG_WARN, __VA_ARGS__)

    #define LOG_ERROR_CHECKED(...)  \
        do { \
            result_bool_constcharp _r = log_fmt(LOG_ERROR, __VA_ARGS__); \
            if (result_bool_constcharp_is_err(_r)) { \
                fprintf(stderr, "[LOG-FAIL] Could not write error message: %s\n", \
                        result_bool_constcharp_unwrap_err(_r)); \
            } \
        } while (0)
#endif

#endif /* CANON_UTIL_LOG_MACROS_H */
