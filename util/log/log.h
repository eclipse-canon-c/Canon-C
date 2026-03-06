#ifndef CANON_UTIL_LOG_H
#define CANON_UTIL_LOG_H

#include "core/primitives/types.h"
#include "semantics/error.h"
#include <stdarg.h>
#include <stdio.h>

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
 * - Composable — works seamlessly with Result combinators
 * - Fire-and-forget macros — convenience for non-critical logs
 *
 * Runtime dependency:
 * ────────────────────────────────────────────────────────────────────────────
 * This module depends on <stdio.h> (FILE*, vfprintf, fputs, fputc, fflush)
 * and <stdarg.h> (va_list). This is an intentional and acknowledged coupling
 * to the C runtime. It makes log.h unsuitable for bare-metal embedded targets
 * without a hosted stdio environment. All Canon-C layers below util/ remain
 * stdio-free.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(n) where n = formatted message length
 * - Space: O(1) — zero heap, stack usage < 1 KiB even with large formats
 * - I/O: 3-4 writes (prefix + message + newline) + 1 flush per call
 * - No buffering surprises — fflush() ensures immediate visibility
 * - Note: all functions are static inline — each including translation unit
 *   gets its own copy. Acceptable for a logging path; avoid in tight loops.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Reentrant but not thread-safe on the same FILE* stream
 * - Concurrent writes to same stream → interleaved/garbled output
 * - Safe when: different threads use different streams, or external locking
 * - Recommended: per-thread log files or a mutex around the shared stream
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - C99 or later (va_list, static inline, variadic macros)
 * - Requires hosted C runtime (stdio.h must be available)
 * - No platform-specific code or compiler extensions used
 *
 * Error codes returned:
 * ────────────────────────────────────────────────────────────────────────────
 * - ERR_INVALID_ARG  — NULL stream, NULL format string, or NULL message
 * - ERR_IO_FAILED    — fputs, vfprintf, fputc, or fflush returned EOF/error
 *
 * Why Result<bool, Error> instead of Result<bool, constcharp>?
 * ────────────────────────────────────────────────────────────────────────────
 * Using the library's standard Error type allows callers to:
 * - Programmatically branch on failure kind (ERR_IO_FAILED vs ERR_INVALID_ARG)
 * - Use error_message() when a human-readable string is needed
 * - Compose with all existing Result combinators without special cases
 * A raw const char* error is only useful for printing — Error does that
 * and more, and keeps log.h consistent with the rest of Canon-C.
 *
 * Why explicit error handling for logging?
 * ────────────────────────────────────────────────────────────────────────────
 * Traditional loggers silently swallow I/O errors — dangerous in production:
 * - Disk full → logs dropped → false sense of health
 * - Permission denied → audit/security logs missing
 * - Network log sink down → critical errors invisible
 *
 * This library makes failures observable — caller decides:
 * - Ignore failures entirely (fire-and-forget macros)
 * - Log to a fallback stream
 * - Abort on audit log failure
 * - Count failures and alert upstream
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Bare-metal / embedded targets without hosted stdio
 * - High-volume structured logging (use a dedicated library)
 * - Async / buffered logging with delayed writes
 * - Runtime-configurable log levels or filters
 * - Signal handlers (vfprintf is not async-signal-safe)
 * - Very large messages that exceed the stack
 *
 * @sa util/log/log_macros.h — ergonomic macro layer over this API
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

/**
 * @brief Result type for all logging operations
 *
 * - Ok(true)               — message written and flushed successfully
 * - Err(ERR_INVALID_ARG)   — NULL stream, format string, or message passed
 * - Err(ERR_IO_FAILED)     — any write or flush operation failed
 *
 * Use error_message(result_bool_Error_unwrap_err(r)) to get a
 * human-readable description of the failure when needed.
 *
 * Note: CANON_RESULT(bool, Error) is already instantiated inside
 * semantics/error.h which is included above. Do not instantiate it again
 * in any translation unit that includes this header.
 */

/* ────────────────────────────────────────────────────────────────────────────
   Core low-level API
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Low-level formatted logging with va_list (foundation function)
 *
 * All other logging functions delegate here. Use this directly only when
 * building custom wrappers or middleware layers over the logging API.
 *
 * @param stream  Valid open FILE* — must not be NULL
 * @param level   Severity level — controls the written prefix only
 * @param fmt     printf-style format string — must not be NULL
 * @param args    va_list initialized by caller via va_start()
 *
 * @return Ok(true)              on full success (prefix + msg + newline + flush)
 *         Err(ERR_INVALID_ARG)  if stream or fmt is NULL
 *         Err(ERR_IO_FAILED)    if any write or flush operation fails
 *
 * @remark Output format: "[LEVEL] " + formatted message + "\n"
 * @remark fflush() is called explicitly — output is always immediately visible
 * @remark Partial output is possible on failure — writes are not atomic
 * @remark NOT async-signal-safe — do not call from signal handlers
 */
static inline result_bool_Error log_vfmt_to(
    FILE*       stream,
    log_level   level,
    const char* fmt,
    va_list     args
) {
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
 * Thin variadic wrapper over log_vfmt_to(). Use when the destination
 * stream must be controlled by the caller.
 *
 * @param stream  Valid open FILE* — must not be NULL
 * @param level   Severity level
 * @param fmt     printf-style format string — must not be NULL
 * @param ...     Format arguments
 *
 * @return Same as log_vfmt_to()
 */
static inline result_bool_Error log_fmt_to(
    FILE*       stream,
    log_level   level,
    const char* fmt,
    ...
) {
    va_list args;
    va_start(args, fmt);
    result_bool_Error res = log_vfmt_to(stream, level, fmt, args);
    va_end(args);
    return res;
}

/**
 * @brief Plain string logging to an explicit stream
 *
 * Prefer over log_fmt_to() when logging user-controlled or untrusted
 * strings — eliminates format-string injection risk entirely.
 *
 * @param stream  Valid open FILE* — must not be NULL
 * @param level   Severity level
 * @param msg     Null-terminated message string — must not be NULL
 *
 * @return Same as log_vfmt_to()
 */
static inline result_bool_Error log_to(
    FILE*       stream,
    log_level   level,
    const char* msg
) {
    if (!msg)    return result_bool_Error_err(ERR_INVALID_ARG);
    if (!stream) return result_bool_Error_err(ERR_INVALID_ARG);
    return log_fmt_to(stream, level, "%s", msg);
}

/**
 * @brief Formatted logging to the default stream for the given level
 *
 * LOG_INFO / LOG_WARN → stdout
 * LOG_ERROR           → stderr
 *
 * @param level   Severity level
 * @param fmt     printf-style format string — must not be NULL
 * @param ...     Format arguments
 *
 * @return Same as log_vfmt_to()
 */
static inline result_bool_Error log_fmt(
    log_level   level,
    const char* fmt,
    ...
) {
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
 * LOG_INFO / LOG_WARN → stdout
 * LOG_ERROR           → stderr
 *
 * Prefer over log_fmt() when logging user-controlled or untrusted strings.
 *
 * @param level   Severity level
 * @param msg     Null-terminated message string — must not be NULL
 *
 * @return Same as log_vfmt_to()
 */
static inline result_bool_Error log_msg(
    log_level   level,
    const char* msg
) {
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    return log_to(stream, level, msg);
}

/* ────────────────────────────────────────────────────────────────────────────
   Fire-and-forget macros (failures intentionally ignored)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * These macros discard the Result return value via (void) cast.
 * Use them when a logging failure is genuinely non-critical and you
 * want call-site brevity. For audit, security, or critical paths,
 * call the underlying functions directly and handle the Result.
 *
 * LOG_INFO  / LOG_WARN  / LOG_ERROR      — plain string, default stream
 * LOG_INFO_FMT / LOG_WARN_FMT / LOG_ERROR_FMT — formatted, default stream
 */
#define LOG_INFO(msg)      (void)log_msg(LOG_INFO,  (msg))
#define LOG_WARN(msg)      (void)log_msg(LOG_WARN,  (msg))
#define LOG_ERROR(msg)     (void)log_msg(LOG_ERROR, (msg))
#define LOG_INFO_FMT(...)  (void)log_fmt(LOG_INFO,  __VA_ARGS__)
#define LOG_WARN_FMT(...)  (void)log_fmt(LOG_WARN,  __VA_ARGS__)
#define LOG_ERROR_FMT(...) (void)log_fmt(LOG_ERROR, __VA_ARGS__)

#endif /* CANON_UTIL_LOG_H */
