#ifndef CANON_UTIL_LOG_MACROS_H
#define CANON_UTIL_LOG_MACROS_H

#include "util/log/log.h"
#include "semantics/error.h"

/**
 * @file util/log/log_macros.h
 * @brief Ergonomic, safe logging macros layered on log.h
 *
 * Provides a clean, expressive macro interface for the most common logging
 * patterns:
 * - Simple literal messages (most efficient)
 * - printf-style formatted messages
 * - Fire-and-forget convenience (production default)
 * - Checked variants in debug builds (fail loudly during development)
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Logging should feel natural — one macro call, no boilerplate
 * - Zero runtime cost in release builds (NDEBUG eliminates checks)
 * - Simple messages cheaper than formatted (no va_list overhead)
 * - Failures visible in debug builds — never silent during development
 * - Same three levels as log.h: INFO, WARN, ERROR
 * - No allocations — all formatting happens on stack
 * - Composable with log.h Result API when explicit checking is needed
 * - Conditional compilation via standard NDEBUG macro
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(n) where n = message length
 * - Space: O(1) — zero heap, minimal stack usage
 * - Checked variants: zero overhead when NDEBUG is defined
 * - _MSG macros: faster path (log_msg → fputs, no vfprintf)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Inherits thread-safety from log.h — not thread-safe on shared streams
 * without external synchronization. The macros themselves introduce no
 * additional shared state.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - C99 or later (variadic macros)
 * - Depends only on util/log/log.h
 * - Standard NDEBUG controls debug/release behavior
 * - No platform-specific code or compiler extensions
 *
 * Macro variants:
 * ────────────────────────────────────────────────────────────────────────────
 * Suffix    | Release (NDEBUG)  | Debug (!NDEBUG)              | Use when
 * ----------|-------------------|------------------------------|-------------------
 * _MSG      | Fire-and-forget   | Fire-and-forget              | Literals / constants
 * _FMT      | Fire-and-forget   | Fire-and-forget              | Most formatted logs
 * _CHECKED  | Same as _FMT      | Logs failure to stderr       | Critical / audit logs
 *
 * Best practices:
 * ────────────────────────────────────────────────────────────────────────────
 * - Use _MSG for string literals — cheaper, no format overhead
 * - Use _FMT for dynamic data
 * - Use _CHECKED for audit, security, or must-not-fail log paths in debug
 * - Never put side-effects in macro arguments — may evaluate more than once
 *   in the checked variants
 * - Avoid format-string injection — use _MSG or explicit "%s" for user input
 *
 * @sa util/log/log.h — underlying Result-based API
 * @sa semantics/error.h — Error codes and error_message()
 */

/* ────────────────────────────────────────────────────────────────────────────
   Simple message macros — fastest path, safest for literals
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Logs a literal or constant string at INFO level (fire-and-forget)
 *
 * Routes through log_msg() — uses fputs internally, no vfprintf overhead.
 * Prefer this over LOG_INFO_FMT when no formatting is needed.
 *
 * @param msg Static string or const char* (no format specifiers)
 */
#define LOG_INFO_MSG(msg)  (void)log_msg(LOG_INFO,  (msg))

/**
 * @brief Logs a literal or constant string at WARN level (fire-and-forget)
 */
#define LOG_WARN_MSG(msg)  (void)log_msg(LOG_WARN,  (msg))

/**
 * @brief Logs a literal or constant string at ERROR level (fire-and-forget)
 */
#define LOG_ERROR_MSG(msg) (void)log_msg(LOG_ERROR, (msg))

/* ────────────────────────────────────────────────────────────────────────────
   Formatted logging macros — most commonly used
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief printf-style formatted logging at INFO level (fire-and-forget)
 *
 * Failures are silently discarded. For observable failures use log_fmt()
 * directly and inspect the returned result_bool_Error.
 *
 * @param ... Format string followed by arguments
 */
#define LOG_INFO_FMT(...)  (void)log_fmt(LOG_INFO,  __VA_ARGS__)

/**
 * @brief printf-style formatted logging at WARN level (fire-and-forget)
 */
#define LOG_WARN_FMT(...)  (void)log_fmt(LOG_WARN,  __VA_ARGS__)

/**
 * @brief printf-style formatted logging at ERROR level (fire-and-forget)
 */
#define LOG_ERROR_FMT(...) (void)log_fmt(LOG_ERROR, __VA_ARGS__)

/* ────────────────────────────────────────────────────────────────────────────
   Checked variants — development safety net
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * In release builds (NDEBUG defined):
 *   Identical to the _FMT variants — fire-and-forget, zero overhead.
 *
 * In debug builds (NDEBUG not defined):
 *   Captures the result_bool_Error returned by log_fmt(). On failure,
 *   writes a secondary diagnostic to stderr via log_fmt_to(), including
 *   the human-readable error_message() string for the failure code.
 *
 * Use these for:
 * - Audit / security log paths that must not silently fail in development
 * - Any log call where a silent drop during testing would hide a real problem
 *
 * @remark Do not put side-effects in macro arguments — in debug builds the
 *         arguments are evaluated once for the primary log call, but the
 *         result inspection adds a branch. Safe, but be aware.
 * @remark The secondary stderr write (on failure) is itself fire-and-forget —
 *         if stderr is also broken, there is nothing further we can do.
 */
#ifdef NDEBUG

#define LOG_INFO_CHECKED(...)  (void)log_fmt(LOG_INFO,  __VA_ARGS__)
#define LOG_WARN_CHECKED(...)  (void)log_fmt(LOG_WARN,  __VA_ARGS__)
#define LOG_ERROR_CHECKED(...) (void)log_fmt(LOG_ERROR, __VA_ARGS__)

#else

#define LOG_INFO_CHECKED(...) \
    do { \
        result_bool_Error __r = log_fmt(LOG_INFO, __VA_ARGS__); \
        if (result_bool_Error_is_err(__r)) { \
            (void)log_fmt_to(stderr, LOG_ERROR, \
                "[LOG-FAIL] INFO failed: %s", \
                error_message(result_bool_Error_unwrap_err(__r))); \
        } \
    } while (0)

#define LOG_WARN_CHECKED(...) \
    do { \
        result_bool_Error __r = log_fmt(LOG_WARN, __VA_ARGS__); \
        if (result_bool_Error_is_err(__r)) { \
            (void)log_fmt_to(stderr, LOG_ERROR, \
                "[LOG-FAIL] WARN failed: %s", \
                error_message(result_bool_Error_unwrap_err(__r))); \
        } \
    } while (0)

#define LOG_ERROR_CHECKED(...) \
    do { \
        result_bool_Error __r = log_fmt(LOG_ERROR, __VA_ARGS__); \
        if (result_bool_Error_is_err(__r)) { \
            (void)log_fmt_to(stderr, LOG_ERROR, \
                "[LOG-FAIL] ERROR failed: %s", \
                error_message(result_bool_Error_unwrap_err(__r))); \
        } \
    } while (0)

#endif /* NDEBUG */

#endif /* CANON_UTIL_LOG_MACROS_H */
