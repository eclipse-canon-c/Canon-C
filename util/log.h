// util/log.h
#ifndef CANON_UTIL_LOG_H
#define CANON_UTIL_LOG_H

#include <stdio.h>
#include <stdarg.h>

#include "semantics/result.h"

/**
 * @file log.h
 * @brief Minimal, explicit and fully observable logging facility
 *
 * Provides a simple, zero-allocation, no-global-state logging system with:
 * - Three clearly defined severity levels: INFO, WARN, ERROR
 * - Every operation returns Result — full observability of success/failure
 * - No silent failures — I/O errors are always returned and must be handled
 * - Stack-based formatting (no heap allocation, safe for embedded/real-time)
 * - Default streams: stdout for INFO/WARN, stderr for ERROR
 * - Optional fire-and-forget macros for convenience
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit side-effects — caller sees every possible failure
 * - Composable with Result combinators (and_then, map_err, unwrap_or, etc.)
 * - Zero runtime dependencies beyond stdio
 * - Safe for constrained environments (no malloc, no threads, no buffering surprises)
 * - Predictable behavior — always flushes after write
 * - Static error messages — no need to free error strings
 *
 * Error handling philosophy:
 * - Most logging libraries swallow I/O errors → very dangerous in production
 * - Canon-C forces you to see failures → encourages proper error handling
 * - In practice many programs ignore logging failures → hence fire-and-forget macros
 *
 * Recommended usage patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Critical logging (want to know if log failed):
 *    result_bool_constcharp r = LOG_ERROR_FMT("Failed to open %s", path);
 *    if (result_is_err(r)) { /* handle logging failure */ }
 *
 * 2. Normal usage (fire-and-forget):
 *    LOG_INFO_FMT("Server listening on port %d", port);
 *
 * 3. Custom stream (file logging, network, etc.):
 *    log_fmt_to(logfile, LOG_INFO, "Connection from %s", ip);
 */

/**
 * @brief Logging severity levels
 */
typedef enum {
    LOG_INFO,   ///< Informational messages (default: stdout)
    LOG_WARN,   ///< Warnings and recoverable issues (default: stdout)
    LOG_ERROR   ///< Errors and critical failures (default: stderr)
} log_level;

/**
 * @brief Result type for all logging operations
 *
 * - Ok(true)   = message was successfully written
 * - Err(msg)   = I/O failure with static error message (do not free)
 */
CANON_C_DEFINE_RESULT(bool, const char*)

/* ────────────────────────────────────────────────────────────────────────────
   Core: Low-level formatted logging with va_list (most flexible)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Low-level formatted logging using va_list to any stream
 *
 * @param stream  Output stream (stdout, stderr, file stream, etc.)
 * @param level   Severity level (controls prefix only)
 * @param fmt     printf-style format string (must not be NULL)
 * @param args    Variable arguments list (va_list)
 *
 * @return Ok(true) on success  
 *         Err("static reason") on any failure
 *
 * Output format: `[LEVEL] ` + formatted message + newline
 *
 * Behavior:
 * - Writes prefix, message and newline atomically
 * - Explicitly flushes stream after write
 * - Never allocates memory
 * - Handles NULL stream/fmt safely (returns error)
 *
 * Performance: O(length of formatted string)
 */
static inline result_bool_constcharp log_vfmt_to(
    FILE* stream,
    log_level level,
    const char* fmt,
    va_list args
) {
    if (!stream) {
        return result_bool_constcharp_err("null output stream");
    }
    if (!fmt) {
        return result_bool_constcharp_err("null format string");
    }

    const char* prefix = "";
    switch (level) {
        case LOG_INFO:  prefix = "[INFO] ";  break;
        case LOG_WARN:  prefix = "[WARN] ";  break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
    }

    if (fputs(prefix, stream) == EOF) {
        return result_bool_constcharp_err("failed to write log prefix");
    }

    if (vfprintf(stream, fmt, args) < 0) {
        return result_bool_constcharp_err("failed to format/write message");
    }

    if (fputc('\n', stream) == EOF) {
        return result_bool_constcharp_err("failed to write newline");
    }

    if (fflush(stream) == EOF) {
        return result_bool_constcharp_err("failed to flush stream");
    }

    return result_bool_constcharp_ok(true);
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience wrappers — formatted & simple message
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging to any explicit stream
 *
 * @param stream  Output stream
 * @param level   Log level
 * @param fmt     printf-style format string
 * @param ...     Format arguments
 *
 * @return Ok(true) on success, Err(reason) on failure
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
 * @brief Simple (non-formatted) message logging to any stream
 *
 * @param stream  Output stream
 * @param level   Log level
 * @param msg     Message string (may not be NULL)
 *
 * @return Ok(true) on success, Err(reason) on failure
 */
static inline result_bool_constcharp log_to(
    FILE* stream,
    log_level level,
    const char* msg
) {
    if (!msg) {
        return result_bool_constcharp_err("null message");
    }
    return log_fmt_to(stream, level, "%s", msg);
}

/* ────────────────────────────────────────────────────────────────────────────
   Default stream convenience (stdout/stderr based on level)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging to default stream (stdout or stderr)
 *
 * @param level   Log level
 * @param fmt     printf-style format
 * @param ...     Format arguments
 *
 * @return Ok(true) on success, Err(reason) on failure
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
 * @param level   Log level
 * @param msg     Message string
 *
 * @return Ok(true) on success, Err(reason) on failure
 */
static inline result_bool_constcharp log_msg(
    log_level level,
    const char* msg
) {
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    return log_to(stream, level, msg);
}

/* ────────────────────────────────────────────────────────────────────────────
   Fire-and-forget convenience macros
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Fire-and-forget logging macros (ignore return value)
 *
 * Common in most applications where logging failure is non-critical.
 * These simply discard the Result — use the checked versions when needed.
 *
 * See log_macros.h for more advanced variants (conditional compilation, etc.)
 */
#define LOG_INFO(msg)         (void)log_msg(LOG_INFO,   (msg))
#define LOG_WARN(msg)         (void)log_msg(LOG_WARN,   (msg))
#define LOG_ERROR(msg)        (void)log_msg(LOG_ERROR,  (msg))

#define LOG_INFO_FMT(...)     (void)log_fmt(LOG_INFO,   __VA_ARGS__)
#define LOG_WARN_FMT(...)     (void)log_fmt(LOG_WARN,   __VA_ARGS__)
#define LOG_ERROR_FMT(...)    (void)log_fmt(LOG_ERROR,  __VA_ARGS__)

#endif /* CANON_UTIL_LOG_H */
