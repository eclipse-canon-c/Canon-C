#ifndef CANON_UTIL_LOG_H
#define CANON_UTIL_LOG_H

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#include "core/primitives/types.h"
#include "core/ownership.h"
#include "semantics/error.h"

/**
 * @file util/log/log.h
 * @brief Minimal, explicit, zero-allocation logging with observable failures
 *
 * Provides a simple, safe logging facility where every I/O operation
 * returns result_bool_Error — forcing callers to consciously handle (or
 * ignore) potential failures. No global state, no hidden configuration,
 * no silent drops.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit over implicit — every log attempt returns success/failure
 * - Zero allocations — stack-only, no malloc/free anywhere
 * - No global state — caller fully controls output streams
 * - Three fixed severity levels (INFO/WARN/ERROR) — no runtime filtering
 * - Automatic flushing — logs are visible immediately
 * - Structured error codes — caller can branch on ERR_IO_FAILED,
 *   ERR_INVALID_ARG, etc. Use error_message() for human-readable strings
 *
 * Error handling rationale:
 * ────────────────────────────────────────────────────────────────────────────
 * NULL stream and NULL format are data errors (Err(ERR_INVALID_ARG)),
 * not contract violations. A logger that aborts your program on bad
 * input defeats its purpose. The caller decides what to do with the
 * failure — ignore it, retry to a fallback stream, or escalate.
 *
 * Runtime dependency:
 * ────────────────────────────────────────────────────────────────────────────
 * This module depends on <stdio.h> (FILE*, vfprintf, fputs, fputc, fflush)
 * and <stdarg.h> (va_list). All Canon-C layers below util/ remain stdio-free.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Reentrant but not thread-safe on the same FILE* stream
 * - Safe when: different threads use different streams, or external locking
 *
 * Error codes returned:
 * ────────────────────────────────────────────────────────────────────────────
 * - ERR_INVALID_ARG  — NULL stream, NULL format string, or NULL message
 * - ERR_IO_FAILED    — fputs, vfprintf, fputc, or fflush returned EOF/error
 *
 * @sa util/log/log_macros.h — ergonomic fire-and-forget macro layer
 * @sa semantics/error.h     — Error codes and error_message()
 */

/**
 * @brief Logging severity levels
 *
 * Controls the prefix written before each message and the default
 * output stream used by the convenience functions:
 * - LOG_INFO / LOG_WARN → stdout
 * - LOG_ERROR           → stderr
 */
typedef enum {
    LOG_INFO,   ///< "[INFO] "  → stdout
    LOG_WARN,   ///< "[WARN] "  → stdout
    LOG_ERROR   ///< "[ERROR] " → stderr
} log_level;

/* ────────────────────────────────────────────────────────────────────────────
   Core low-level API
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Low-level formatted logging with va_list (foundation function)
 *
 * All other logging functions delegate here.
 *
 * @param stream  Valid open FILE* (NULL → Err(ERR_INVALID_ARG))
 * @param level   Severity level — controls the written prefix only
 * @param fmt     printf-style format string (NULL → Err(ERR_INVALID_ARG))
 * @param args    va_list initialized by caller via va_start()
 *
 * @return Ok(true)              on full success
 *         Err(ERR_INVALID_ARG)  if stream or fmt is NULL
 *         Err(ERR_IO_FAILED)    if any write or flush fails
 */
static inline result_bool_Error log_vfmt_to(
    borrowed(FILE*)       stream,
    log_level             level,
    borrowed(const char*) fmt,
    va_list               args)
{
    if (!stream) return result_bool_Error_err(ERR_INVALID_ARG);
    if (!fmt)    return result_bool_Error_err(ERR_INVALID_ARG);

    const char* prefix;
    switch (level) {
        case LOG_INFO:  prefix = "[INFO] ";  break;
        case LOG_WARN:  prefix = "[WARN] ";  break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
        default:        prefix = "[????] ";  break;
    }

    if (fputs(prefix, stream) == EOF)
        return result_bool_Error_err(ERR_IO_FAILED);

    if (vfprintf(stream, fmt, args) < 0)
        return result_bool_Error_err(ERR_IO_FAILED);

    if (fputc('\n', stream) == EOF)
        return result_bool_Error_err(ERR_IO_FAILED);

    if (fflush(stream) == EOF)
        return result_bool_Error_err(ERR_IO_FAILED);

    return result_bool_Error_ok(true);
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience wrappers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging to an explicit stream
 *
 * @param stream  Valid open FILE* (NULL → Err(ERR_INVALID_ARG))
 * @param level   Severity level
 * @param fmt     printf-style format string (NULL → Err(ERR_INVALID_ARG))
 * @param ...     Format arguments
 * @return Same as log_vfmt_to()
 */
static inline result_bool_Error log_fmt_to(
    borrowed(FILE*)       stream,
    log_level             level,
    borrowed(const char*) fmt,
    ...)
{
    va_list args;
    va_start(args, fmt);
    result_bool_Error res = log_vfmt_to(stream, level, fmt, args);
    va_end(args);
    return res;
}

/**
 * @brief Plain string logging to an explicit stream
 *
 * Prefer over log_fmt_to() for user-controlled strings — eliminates
 * format-string injection risk.
 *
 * @param stream  Valid open FILE* (NULL → Err(ERR_INVALID_ARG))
 * @param level   Severity level
 * @param msg     Null-terminated message (NULL → Err(ERR_INVALID_ARG))
 * @return Same as log_vfmt_to()
 */
static inline result_bool_Error log_to(
    borrowed(FILE*)       stream,
    log_level             level,
    borrowed(const char*) msg)
{
    if (!msg)    return result_bool_Error_err(ERR_INVALID_ARG);
    if (!stream) return result_bool_Error_err(ERR_INVALID_ARG);
    return log_fmt_to(stream, level, "%s", msg);
}

/**
 * @brief Formatted logging to the default stream for the given level
 *
 * LOG_INFO / LOG_WARN → stdout, LOG_ERROR → stderr
 */
static inline result_bool_Error log_fmt(
    log_level             level,
    borrowed(const char*) fmt,
    ...)
{
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    va_list args;
    va_start(args, fmt);
    result_bool_Error res = log_vfmt_to(stream, level, fmt, args);
    va_end(args);
    return res;
}

/**
 * @brief Plain string logging to the default stream for the given level
 *
 * LOG_INFO / LOG_WARN → stdout, LOG_ERROR → stderr
 */
static inline result_bool_Error log_msg(
    log_level             level,
    borrowed(const char*) msg)
{
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    return log_to(stream, level, msg);
}

#endif /* CANON_UTIL_LOG_H */
