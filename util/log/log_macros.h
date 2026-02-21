#ifndef CANON_UTIL_LOG_MACROS_H
#define CANON_UTIL_LOG_MACROS_H

#include "util/log/log.h"

/**
 * @file util/log_macros.h
 * @brief Ergonomic, safe logging macros layered on log.h
 *
 * Provides a clean, expressive macro interface for the most common logging patterns:
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
 * - Composable with log.h Result API when explicit checking needed
 * - Conditional compilation via standard NDEBUG macro
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(n) where n = message length
 * - Space: O(1) — zero heap, minimal stack usage
 * - Checked variants: zero overhead when NDEBUG defined
 * - Simple _MSG macros: faster path (fputs only, no vfprintf)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Inherits thread-safety from log.h (not thread-safe on shared streams
 * without external synchronization). Macros themselves are thread-safe.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - C99 or later (variadic macros)
 * - Depends only on log.h
 * - Standard NDEBUG controls debug/release behavior
 * - No platform-specific code
 *
 * Macro variants:
 * ────────────────────────────────────────────────────────────────────────────
 * | Macro suffix | Behavior in release (NDEBUG) | Behavior in debug (!NDEBUG) | Use when |
 * |--------------|-------------------------------|-------------------------------------|----------|
 * | _MSG         | Fire-and-forget               | Fire-and-forget                     | Literal/constant strings |
 * | _FMT         | Fire-and-forget               | Fire-and-forget                     | Most formatted logging |
 * | _CHECKED     | Same as _FMT                  | Logs to stderr on failure           | Critical logs during development |
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Application lifecycle events (start/stop)
 * - Request/response logging in servers
 * - Error context reporting
 * - Performance warnings & metrics
 * - Audit/security events
 * - Debug/trace output (disabled in release)
 * - Resource allocation / state transitions
 *
 * Best practices:
 * ────────────────────────────────────────────────────────────────────────────
 * - Use _MSG for literals (cheaper, safer)
 * - Use _FMT for dynamic data
 * - Use _CHECKED for audit/security/critical logs in debug
 * - Never put side-effects in macro arguments (may evaluate twice in checked mode)
 * - Avoid format-string injection — use _MSG or explicit %s for user input
 *
 * @sa util/log.h — underlying Result-based API
 */
/* ────────────────────────────────────────────────────────────────────────────
   Simple message macros — fastest & safest for literals
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Logs a literal/constant string at INFO level (fire-and-forget)
 * @param msg Static string or const char* (no formatting)
 */
#define LOG_INFO_MSG(msg)     (void)log_msg(LOG_INFO, (msg))

/**
 * @brief Logs a literal/constant string at WARN level (fire-and-forget)
 */
#define LOG_WARN_MSG(msg)     (void)log_msg(LOG_WARN, (msg))

/**
 * @brief Logs a literal/constant string at ERROR level (fire-and-forget)
 */
#define LOG_ERROR_MSG(msg)    (void)log_msg(LOG_ERROR, (msg))

/* ────────────────────────────────────────────────────────────────────────────
   Formatted logging macros — most commonly used
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief printf-style formatted logging at INFO level (fire-and-forget)
 * @param ... Format string + arguments
 */
#define LOG_INFO_FMT(...)     (void)log_fmt(LOG_INFO, (__VA_ARGS__))

/**
 * @brief printf-style formatted logging at WARN level (fire-and-forget)
 */
#define LOG_WARN_FMT(...)     (void)log_fmt(LOG_WARN, (__VA_ARGS__))

/**
 * @brief printf-style formatted logging at ERROR level (fire-and-forget)
 */
#define LOG_ERROR_FMT(...)    (void)log_fmt(LOG_ERROR, (__VA_ARGS__))

/* ────────────────────────────────────────────────────────────────────────────
   Checked variants — development safety net
   ──────────────────────────────────────────────────────────────────────────── */
#ifdef NDEBUG
    #define LOG_INFO_CHECKED(...)     LOG_INFO_FMT(__VA_ARGS__)
    #define LOG_WARN_CHECKED(...)     LOG_WARN_FMT(__VA_ARGS__)
    #define LOG_ERROR_CHECKED(...)    LOG_ERROR_FMT(__VA_ARGS__)
#else
    #define LOG_INFO_CHECKED(...) \
        do { \
            result_bool_constcharp __r = log_fmt(LOG_INFO, (__VA_ARGS__)); \
            if (result_is_err(__r)) { \
                log_fmt_to(stderr, LOG_ERROR, "[LOG-FAIL] INFO failed: %s", \
                           result_unwrap_err(__r)); \
            } \
        } while (0)

    #define LOG_WARN_CHECKED(...) \
        do { \
            result_bool_constcharp __r = log_fmt(LOG_WARN, (__VA_ARGS__)); \
            if (result_is_err(__r)) { \
                log_fmt_to(stderr, LOG_ERROR, "[LOG-FAIL] WARN failed: %s", \
                           result_unwrap_err(__r)); \
            } \
        } while (0)

    #define LOG_ERROR_CHECKED(...) \
        do { \
            result_bool_constcharp __r = log_fmt(LOG_ERROR, (__VA_ARGS__)); \
            if (result_is_err(__r)) { \
                log_fmt_to(stderr, LOG_ERROR, "[LOG-FAIL] ERROR failed: %s", \
                           result_unwrap_err(__r)); \
            } \
        } while (0)
#endif

#endif /* CANON_UTIL_LOG_MACROS_H */
