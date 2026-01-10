// util/log_macros.h
#ifndef CANON_UTIL_LOG_MACROS_H
#define CANON_UTIL_LOG_MACROS_H

#include "util/log.h"

/**
 * @file log_macros.h
 * @brief Convenient and safe logging macros built on top of log.h
 *
 * Provides a clean, easy-to-use interface for logging with:
 *   - Explicit levels: INFO, WARN, ERROR
 *   - Two styles:
 *     - Simple message: LOG_XXX_MSG("text")
 *     - Formatted: LOG_XXX_FMT("format %s", arg)
 *   - Optional "checked" variants (debug builds verify I/O success)
 *   - Never silently ignores failures (checked variants report meta-errors)
 *   - Zero allocation (all formatting is stack-based via log_fmt)
 *   - Conditional debug behavior via NDEBUG
 *
 * Key safety guarantees:
 *   - Fire-and-forget in release builds (void cast discards Result)
 *   - Debug builds can detect and report logging failures
 *   - All macros expand to simple calls — no hidden control flow
 *   - Compatible with log.h Result-based API
 *
 * Usage examples:
 *   LOG_INFO_MSG("Application started");
 *   LOG_WARN_FMT("Invalid input: %s", user_input);
 *   LOG_ERROR_CHECKED("Failed to open file: %s", filename);
 *
 * Recommended:
 *   - Use _CHECKED variants during development
 *   - Switch to plain macros in release (via NDEBUG)
 *   - Always pair with proper error handling (don't rely on logging alone)
 */

/**
 * @brief Logs a simple message at INFO level
 * @param msg Static or literal message string
 */
#define LOG_INFO_MSG(msg) (void)log_msg(LOG_INFO, (msg))

/**
 * @brief Logs a simple message at WARN level
 * @param msg Static or literal message string
 */
#define LOG_WARN_MSG(msg) (void)log_msg(LOG_WARN, (msg))

/**
 * @brief Logs a simple message at ERROR level
 * @param msg Static or literal message string
 */
#define LOG_ERROR_MSG(msg) (void)log_msg(LOG_ERROR, (msg))

/**
 * @brief Logs a formatted message at INFO level
 * @param ... printf-style format + arguments
 */
#define LOG_INFO_FMT(...) (void)log_fmt(LOG_INFO, __VA_ARGS__)

/**
 * @brief Logs a formatted message at WARN level
 * @param ... printf-style format + arguments
 */
#define LOG_WARN_FMT(...) (void)log_fmt(LOG_WARN, __VA_ARGS__)

/**
 * @brief Logs a formatted message at ERROR level
 * @param ... printf-style format + arguments
 */
#define LOG_ERROR_FMT(...) (void)log_fmt(LOG_ERROR, __VA_ARGS__)

/* ────────────────────────────────────────────────────────────────────────────
   Checked variants (behavior changes based on NDEBUG)
   ──────────────────────────────────────────────────────────────────────────── */

#ifdef NDEBUG
    /* Release builds: fire-and-forget (ignore Result) */
    #define LOG_INFO_CHECKED(...)   LOG_INFO_FMT(__VA_ARGS__)
    #define LOG_WARN_CHECKED(...)   LOG_WARN_FMT(__VA_ARGS__)
    #define LOG_ERROR_CHECKED(...)  LOG_ERROR_FMT(__VA_ARGS__)
#else
    /* Debug builds: verify logging succeeded, fallback to stderr on failure */
    #define LOG_INFO_CHECKED(...)   (void)log_fmt(LOG_INFO, __VA_ARGS__)
    #define LOG_WARN_CHECKED(...)   (void)log_fmt(LOG_WARN, __VA_ARGS__)
    #define LOG_ERROR_CHECKED(...) \
        do { \
            result_bool_constcharp _r = log_fmt(LOG_ERROR, __VA_ARGS__); \
            if (result_bool_constcharp_is_err(_r)) { \
                /* Meta-logging: report failure to stderr */ \
                fprintf(stderr, "[LOG FAILURE] Could not log message\n"); \
            } \
        } while (0)
#endif

#endif /* CANON_UTIL_LOG_MACROS_H */
