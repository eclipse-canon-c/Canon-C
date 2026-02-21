#ifndef CANON_UTIL_LOG_H
#define CANON_UTIL_LOG_H

#include "core/primitives/types.h"       // bool
#include "core/memory.h"                 // str_len (for future-proofing)
#include "../../semantics/result/result.h"   // result_bool_constcharp (adjust path if needed)
#include <stdarg.h>                      // va_list, va_start, va_end — unavoidable for variadic

/**
 * @file util/log.h
 * @brief Minimal, explicit, zero-allocation logging with observable failures
 *
 * Provides a simple, safe logging facility where **every I/O operation**
 * returns `result_bool_constcharp` — forcing callers to consciously handle (or ignore)
 * potential failures. No global state, no hidden configuration, no silent drops.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit over implicit — every log attempt returns success/failure
 * - Zero allocations — stack-only, no malloc/free anywhere
 * - No global state — caller fully controls output streams
 * - Three fixed severity levels (INFO/WARN/ERROR) — no runtime filtering
 * - Automatic flushing — logs are visible immediately
 * - Static error strings — no dynamic allocation for failure reasons
 * - Composable — works seamlessly with Result combinators
 * - Fire-and-forget macros — convenience for non-critical logs
 * - Binary-safe — can log any byte sequence
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(n) where n = formatted message length
 * - Space: O(1) — zero heap, stack usage < 1 KiB even with large formats
 * - I/O: 3–4 writes (prefix + message + newline) + 1 flush
 * - No buffering surprises — fflush ensures visibility
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Reentrant but **not thread-safe** on the same FILE* stream
 * - Concurrent writes to same stream → interleaved/garbled output
 * - Safe if: different threads use different streams, or external locking used
 * - Recommended: per-thread log files or mutex around shared stream
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - C99 or later (va_list, inline functions)
 * - Depends on Canon-C core modules + <stdarg.h> (for variadic)
 * - Pure ANSI C — relies only on fprintf/vfprintf/fputs/fputc/fflush
 * - No platform-specific code or extensions
 *
 * Why explicit error handling for logging?
 * ────────────────────────────────────────────────────────────────────────────
 * Traditional loggers silently swallow I/O errors → dangerous in production:
 * - Disk full → logs dropped → false sense of health
 * - Permission denied → audit/security logs missing
 * - Network log sink down → critical errors invisible
 *
 * This library makes failures **observable** — caller decides:
 * - Ignore (fire-and-forget macros)
 * - Log to fallback stream
 * - Abort on audit log failure
 * - Count failures and alert
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Debug / info / warning / error output in CLI tools
 * - Audit/security logging (must never fail silently)
 * - Embedded systems with constrained resources
 * - Performance-critical paths where allocation is forbidden
 * - Applications requiring deterministic behavior
 * - Testing / CI environments (observable failures)
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - High-volume structured logging (use dedicated library)
 * - Async / buffered logging with delayed writes
 * - Runtime-configurable log levels / filters
 * - Signal handlers (vfprintf not async-signal-safe)
 * - Very large messages (> stack limit)
 *
 * @sa log_fmt(), log_msg(), LOG_*_FMT macros
 */
/**
 * @brief Logging severity levels
 *
 * Controls prefix and (for convenience functions) default stream:
 * - INFO / WARN → stdout
 * - ERROR → stderr
 */
typedef enum {
    LOG_INFO,   ///< "[INFO] " → stdout
    LOG_WARN,   ///< "[WARN] " → stdout
    LOG_ERROR   ///< "[ERROR] " → stderr
} log_level;

/**
 * @brief Result type for logging operations
 *
 * - Ok(true): message written and flushed successfully
 * - Err(const char*): static error reason (do NOT free!)
 */
CANON_C_DEFINE_RESULT(bool, const char*)

/* ────────────────────────────────────────────────────────────────────────────
   Core low-level API
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Low-level formatted logging with va_list (foundation function)
 *
 * All other logging functions delegate here. Use this when building custom wrappers.
 *
 * @param stream Valid open FILE* (not NULL)
 * @param level Severity (affects prefix only)
 * @param fmt printf-style format string (not NULL)
 * @param args va_list initialized with va_start()
 *
 * @return Ok(true) if prefix + message + newline + flush all succeeded
 *         Err("static reason") on any failure
 *
 * @remark Output format: "[LEVEL] " + formatted message + "\n"
 * @remark Explicit fflush() ensures immediate visibility
 * @remark NOT async-signal-safe — do not call from signal handlers
 * @remark Partial output possible on failure (non-atomic)
 */
static inline result_bool_constcharp log_vfmt_to(
    FILE* stream,
    log_level level,
    const char* fmt,
    va_list args
) {
    if (!stream) return RESULT_ERR(bool, "null output stream");
    if (!fmt) return RESULT_ERR(bool, "null format string");

    const char* prefix;
    switch (level) {
        case LOG_INFO:  prefix = "[INFO] "; break;
        case LOG_WARN:  prefix = "[WARN] "; break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
        default:        prefix = "[????] "; break;
    }

    if (fputs(prefix, stream) == EOF)
        return RESULT_ERR(bool, "failed to write log prefix");

    if (vfprintf(stream, fmt, args) < 0)
        return RESULT_ERR(bool, "failed to format/write message");

    if (fputc('\n', stream) == EOF)
        return RESULT_ERR(bool, "failed to write newline");

    if (fflush(stream) == EOF)
        return RESULT_ERR(bool, "failed to flush stream");

    return RESULT_OK(bool, true);
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience wrappers
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Formatted logging to explicit stream
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
 * @brief Plain string logging to explicit stream
 *
 * Safer when logging user-controlled strings (no format-string risks).
 */
static inline result_bool_constcharp log_to(
    FILE* stream,
    log_level level,
    const char* msg
) {
    if (!msg) return RESULT_ERR(bool, "null message");
    if (!stream) return RESULT_ERR(bool, "null output stream");
    return log_fmt_to(stream, level, "%s", msg);
}

/**
 * @brief Formatted logging to default stream (stdout/stderr)
 *
 * INFO/WARN → stdout, ERROR → stderr
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
 * @brief Plain string logging to default stream
 */
static inline result_bool_constcharp log_msg(
    log_level level,
    const char* msg
) {
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    return log_to(stream, level, msg);
}

/* ────────────────────────────────────────────────────────────────────────────
   Fire-and-forget macros (ignore failures)
   ──────────────────────────────────────────────────────────────────────────── */
#define LOG_INFO(msg)     (void)log_msg(LOG_INFO, (msg))
#define LOG_WARN(msg)     (void)log_msg(LOG_WARN, (msg))
#define LOG_ERROR(msg)    (void)log_msg(LOG_ERROR, (msg))
#define LOG_INFO_FMT(...) (void)log_fmt(LOG_INFO, (__VA_ARGS__))
#define LOG_WARN_FMT(...) (void)log_fmt(LOG_WARN, (__VA_ARGS__))
#define LOG_ERROR_FMT(...) (void)log_fmt(LOG_ERROR, (__VA_ARGS__))

#endif /* CANON_UTIL_LOG_H */
