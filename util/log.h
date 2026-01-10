// util/log.h
#ifndef CANON_UTIL_LOG_H
#define CANON_UTIL_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include "semantics/result.h"

/**
 * @file log.h
 * @brief Minimal, explicit, observable logging with Result-based error handling
 *
 * Provides a simple, zero-allocation logging system with:
 *   - Three explicit levels: INFO, WARN, ERROR
 *   - No global state — everything is explicit (stream, level, message)
 *   - All operations return Result<bool, const char*> for full observability
 *   - No silent failures — I/O errors are returned and must be handled
 *   - Default streams: stdout for INFO/WARN, stderr for ERROR
 *   - Stack-based formatting (no heap allocation)
 *
 * Key design goals:
 *   - Explicit side effects (caller sees every failure)
 *   - Composable with Result combinators (unwrap, map, and_then, etc.)
 *   - Safe for embedded/real-time systems (no malloc, no threads)
 *   - Optional fire-and-forget macros (see log_macros.h)
 *
 * Usage pattern:
 *   result_bool_constcharp r = log_fmt(LOG_INFO, "Starting application v%s", "1.2.0");
 *   if (result_is_err(r)) {
 *       fprintf(stderr, "Logging failed: %s\n", result_unwrap_err(r));
 *   }
 *
 * Or with macros (fire-and-forget):
 *   LOG_INFO_FMT("User logged in: %s", username);
 *   LOG_ERROR("Critical failure detected");
 */

/**
 * @brief Logging severity levels
 */
typedef enum {
    LOG_INFO,   ///< Informational messages (stdout)
    LOG_WARN,   ///< Warnings (stdout)
    LOG_ERROR   ///< Errors and critical issues (stderr)
} log_level;

/**
 * @brief Result type for logging operations
 *
 * Ok(true)  = success
 * Err(msg)  = failure with static error message (do not free)
 */
CANON_C_DEFINE_RESULT(bool, const char*)  // true = success

/* ────────────────────────────────────────────────────────────────────────────
   Core: Low-level formatted logging with va_list (most flexible)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Low-level formatted logging to explicit stream using va_list
 *
 * @param stream  Output stream (stdout, stderr, file, etc.)
 * @param level   Log severity (determines prefix)
 * @param fmt     printf-style format string
 * @param args    Variable arguments (va_list)
 * @return        Ok(true) on success, Err("reason") on any I/O failure
 *
 * Writes: [LEVEL] formatted message + newline
 * Flushes stream after write.
 * Never allocates — fully stack-based.
 */
static inline result_bool_constcharp log_vfmt_to(
    FILE* stream,
    log_level level,
    const char* fmt,
    va_list args
) {
    if (!stream) return result_bool_constcharp_err("null output stream");
    if (!fmt) return result_bool_constcharp_err("null format string");

    const char* prefix = "";
    switch (level) {
        case LOG_INFO:  prefix = "[INFO] "; break;
        case LOG_WARN:  prefix = "[WARN] "; break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
    }

    if (fputs(prefix, stream) == EOF)
        return result_bool_constcharp_err("failed to write log prefix");

    if (vfprintf(stream, fmt, args) < 0)
        return result_bool_constcharp_err("failed to write formatted message");

    if (fputc('\n', stream) == EOF)
        return result_bool_constcharp_err("failed to write newline");

    if (fflush(stream) == EOF)
        return result_bool_constcharp_err("failed to flush stream");

    return result_bool_constcharp_ok(true);
}

/* ────────────────────────────────────────────────────────────────────────────
   Formatted logging to explicit stream (convenience wrapper)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging to explicit stream
 *
 * @param stream  Output stream
 * @param level   Log level
 * @param fmt     printf-style format
 * @param ...     Format arguments
 * @return        Ok(true) on success, Err(reason) on failure
 */
static inline result_bool_constcharp log_fmt_to(
    FILE* stream,
    log_level level,
    const char* fmt,
    ...
) {
    va_list args;
    va_start(args, fmt);
    result_bool_constcharp res = log_vfmt_to(stream, level, fmt, args);
    va_end(args);
    return res;
}

/**
 * @brief Simple string message logging to explicit stream
 *
 * @param stream  Output stream
 * @param level   Log level
 * @param msg     Message string (may be NULL)
 * @return        Ok(true) on success, Err(reason) on failure
 */
static inline result_bool_constcharp log_to(
    FILE* stream,
    log_level level,
    const char* msg
) {
    if (!msg) return result_bool_constcharp_err("null message");
    return log_fmt_to(stream, level, "%s", msg);
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience: Default streams (stdout for INFO/WARN, stderr for ERROR)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging to default stream (stdout/stderr)
 *
 * @param level  Log level
 * @param fmt    printf-style format
 * @param ...    Format arguments
 * @return       Ok(true) on success, Err(reason) on failure
 */
static inline result_bool_constcharp log_fmt(
    log_level level,
    const char* fmt,
    ...
) {
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;

    va_list args;
    va_start(args, fmt);
    result_bool_constcharp res = log_vfmt_to(stream, level, fmt, args);
    va_end(args);

    return res;
}

/**
 * @brief Simple message logging to default stream
 *
 * @param level  Log level
 * @param msg    Message string
 * @return       Ok(true) on success, Err(reason) on failure
 */
static inline result_bool_constcharp log_msg(
    log_level level,
    const char* msg
) {
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    return log_to(stream, level, msg);
}

/* ────────────────────────────────────────────────────────────────────────────
   Fire-and-forget macros (common in release builds)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Fire-and-forget logging macros (ignore Result)
 *
 * These expand to void casts — use when you don't care about I/O failure.
 * See log_macros.h for more advanced checked variants.
 */
#define LOG_INFO(msg)       (void)log_msg(LOG_INFO, (msg))
#define LOG_WARN(msg)       (void)log_msg(LOG_WARN, (msg))
#define LOG_ERROR(msg)      (void)log_msg(LOG_ERROR, (msg))
#define LOG_INFO_FMT(...)   (void)log_fmt(LOG_INFO, __VA_ARGS__)
#define LOG_WARN_FMT(...)   (void)log_fmt(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR_FMT(...)  (void)log_fmt(LOG_ERROR, __VA_ARGS__)

#endif /* CANON_UTIL_LOG_H */
