#ifndef CANON_UTIL_LOG_MACROS_H
#define CANON_UTIL_LOG_MACROS_H

#include "util/log/log.h"
#include "semantics/error.h"

/**
 * @file util/log/log_macros.h
 * @brief Ergonomic, safe logging macros layered on log.h
 *
 * Provides a clean, expressive macro interface for the most common logging
 * patterns. All macros in this file are fire-and-forget by default — they
 * discard the Result return value via (void) cast. For observable failures,
 * call the underlying log.h functions directly.
 *
 * Macro variants:
 * ────────────────────────────────────────────────────────────────────────────
 * Suffix    | Release (NDEBUG)  | Debug (!NDEBUG)              | Use when
 * ----------|-------------------|------------------------------|-------------------
 * (none)    | Fire-and-forget   | Fire-and-forget              | Quick plain string
 * _MSG      | Fire-and-forget   | Fire-and-forget              | Literals / constants
 * _FMT      | Fire-and-forget   | Fire-and-forget              | Most formatted logs
 * _CHECKED  | Same as _FMT      | Logs failure to stderr       | Critical / audit logs
 *
 * @sa util/log/log.h — underlying Result-based API
 */

/* ────────────────────────────────────────────────────────────────────────────
   Plain message macros — fire-and-forget, fastest path
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief Log plain string at INFO level (fire-and-forget) */
#define LOG_INFO(msg)      (void)log_msg(LOG_INFO,  (msg))

/** @brief Log plain string at WARN level (fire-and-forget) */
#define LOG_WARN(msg)      (void)log_msg(LOG_WARN,  (msg))

/** @brief Log plain string at ERROR level (fire-and-forget) */
#define LOG_ERROR(msg)     (void)log_msg(LOG_ERROR, (msg))

/** @brief Alias for LOG_INFO — explicit _MSG suffix for clarity */
#define LOG_INFO_MSG(msg)  (void)log_msg(LOG_INFO,  (msg))

/** @brief Alias for LOG_WARN — explicit _MSG suffix for clarity */
#define LOG_WARN_MSG(msg)  (void)log_msg(LOG_WARN,  (msg))

/** @brief Alias for LOG_ERROR — explicit _MSG suffix for clarity */
#define LOG_ERROR_MSG(msg) (void)log_msg(LOG_ERROR, (msg))

/* ────────────────────────────────────────────────────────────────────────────
   Formatted logging macros — fire-and-forget
   ──────────────────────────────────────────────────────────────────────────── */

/** @brief printf-style formatted logging at INFO level (fire-and-forget) */
#define LOG_INFO_FMT(...)  (void)log_fmt(LOG_INFO,  __VA_ARGS__)

/** @brief printf-style formatted logging at WARN level (fire-and-forget) */
#define LOG_WARN_FMT(...)  (void)log_fmt(LOG_WARN,  __VA_ARGS__)

/** @brief printf-style formatted logging at ERROR level (fire-and-forget) */
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
 *   writes a secondary diagnostic to stderr including error_message().
 */
#ifdef NDEBUG

#define LOG_INFO_CHECKED(...)  (void)log_fmt(LOG_INFO,  __VA_ARGS__)
#define LOG_WARN_CHECKED(...)  (void)log_fmt(LOG_WARN,  __VA_ARGS__)
#define LOG_ERROR_CHECKED(...) (void)log_fmt(LOG_ERROR, __VA_ARGS__)

#else

#define LOG_INFO_CHECKED(...) \
    do { \
        result_bool_Error _lr = log_fmt(LOG_INFO, __VA_ARGS__); \
        if (result_bool_Error_is_err(_lr)) { \
            (void)log_fmt_to(stderr, LOG_ERROR, \
                "[LOG-FAIL] INFO failed: %s", \
                error_message(result_bool_Error_unwrap_err(_lr))); \
        } \
    } while (0)

#define LOG_WARN_CHECKED(...) \
    do { \
        result_bool_Error _lr = log_fmt(LOG_WARN, __VA_ARGS__); \
        if (result_bool_Error_is_err(_lr)) { \
            (void)log_fmt_to(stderr, LOG_ERROR, \
                "[LOG-FAIL] WARN failed: %s", \
                error_message(result_bool_Error_unwrap_err(_lr))); \
        } \
    } while (0)

#define LOG_ERROR_CHECKED(...) \
    do { \
        result_bool_Error _lr = log_fmt(LOG_ERROR, __VA_ARGS__); \
        if (result_bool_Error_is_err(_lr)) { \
            (void)log_fmt_to(stderr, LOG_ERROR, \
                "[LOG-FAIL] ERROR failed: %s", \
                error_message(result_bool_Error_unwrap_err(_lr))); \
        } \
    } while (0)

#endif /* NDEBUG */

#endif /* CANON_UTIL_LOG_MACROS_H */
