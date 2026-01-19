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
 * Provides a zero-allocation, no-global-state logging system with explicit
 * error handling. Every logging operation returns a Result, making all I/O
 * failures observable and forcing conscious decisions about error handling.
 * Designed for simplicity, safety, and predictability in constrained
 * environments where silent failures are unacceptable.
 *
 * Portability:
 * - Requires C99 or later (for inline functions, va_list, stdarg.h)
 * - Depends on standard FILE* operations (fprintf, fputs, fflush)
 * - No platform-specific code - pure ANSI C I/O
 * - Works on any platform with stdio.h support
 * - No external dependencies beyond result.h from this library
 * - Thread-safe only if underlying FILE* operations are locked
 *
 * Thread-safety: Functions are reentrant but NOT thread-safe without
 * external synchronization. Multiple threads writing to
 * the same FILE* concurrently will produce interleaved
 * output. Use per-thread streams or mutex locks for
 * thread-safe logging. No global state to protect.
 *
 * Performance:
 * - Time complexity: O(n) where n = formatted message length
 * - Space complexity: O(1) — zero heap allocations, stack-only
 * - Buffer handling: relies on libc buffering
 * - No dynamic allocation overhead
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit error handling — every log operation returns Result<bool, const char*>
 * - No silent failures — I/O errors must be acknowledged or explicitly ignored
 * - Zero allocations — stack-based formatting, no malloc/free
 * - No global state — no log levels, no global log file, no hidden config
 * - Composable — Result-based API works with map, and_then, unwrap_or
 * - Explicit streams — caller controls where logs go (stdout, stderr, file)
 * - Automatic flushing — ensures logs are visible immediately
 * - Simple severity model — three levels: INFO, WARN, ERROR
 * - Fire-and-forget option — convenience macros for common "ignore errors" case
 * - Deterministic — no buffering surprises, no race conditions
 * - Debuggable — static error messages, no need to manage error strings
 *
 * Why explicit error handling for logging?
 * ────────────────────────────────────────────────────────────────────────────
 * Most logging libraries silently swallow I/O errors. This creates dangerous
 * scenarios in production:
 *
 * Silent failure examples:
 * - Disk full → logs silently dropped → monitoring thinks system is healthy
 * - Permission denied → security audit log fails → compliance violation
 * - Network log server down → error logs lost → debugging impossible
 * - File descriptor limit → new log file can't open → silent failure
 *
 * Canon-C philosophy: Make failures observable, let caller decide handling.
 *
 * (Three approaches to handling log failures / Severity levels explained /
 *  Output format / Memory and allocation / Stack usage /
 *  Stream handling and flushing / Default streams / Typical use cases /
 *  NOT suitable for / Integration patterns / Error handling strategies /
 *  API design decisions / Usage examples sections remain unchanged)
 */

/**
 * @brief Logging severity levels
 *
 * Simple three-level severity system. These are just labels that affect
 * the prefix and default stream selection. No filtering or runtime
 * configuration - all messages are always written.
 *
 * Implementation note: This is an enum, not flags, so you can't combine
 * levels. Each log message has exactly one level.
 */
typedef enum {
    LOG_INFO,   ///< Informational messages (default: stdout, prefix: "[INFO] ")
    LOG_WARN,   ///< Warnings and recoverable issues (default: stdout, prefix: "[WARN] ")
    LOG_ERROR   ///< Errors and critical failures (default: stderr, prefix: "[ERROR] ")
} log_level;

/**
 * @brief Result type for all logging operations
 *
 * Represents the outcome of a logging operation:
 * - Ok(true): Message was successfully written and flushed
 * - Err(const char*): I/O failure with static error message
 *
 * Error messages are static strings - do NOT free them.
 *
 * Possible error messages:
 * - "null output stream" - stream parameter was NULL
 * - "null format string" - fmt parameter was NULL
 * - "null message" - msg parameter was NULL (log_to only)
 * - "failed to write log prefix" - fputs() failed
 * - "failed to format/write message" - vfprintf() failed
 * - "failed to write newline" - fputc() failed
 * - "failed to flush stream" - fflush() failed
 */
CANON_C_DEFINE_RESULT(bool, const char*)

/* ────────────────────────────────────────────────────────────────────────────
   Core: Low-level formatted logging with va_list (most flexible)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Low-level formatted logging using va_list to any stream
 *
 * Foundation of all logging functions. Takes a va_list for maximum flexibility
 * when building wrappers. Most users should use log_fmt() or LOG_*_FMT()
 * macros instead.
 *
 * This is the only function that actually performs I/O. All other logging
 * functions delegate to this one.
 *
 * Algorithm:
 * 1. Validate inputs (stream and fmt not NULL)
 * 2. Select prefix based on log level
 * 3. Write prefix to stream via fputs()
 * 4. Format and write message via vfprintf()
 * 5. Write newline via fputc()
 * 6. Explicitly flush stream via fflush()
 * 7. Return Ok(true) if all steps succeeded
 * 8. Return Err(reason) if any step failed
 *
 * @param stream Output stream (stdout, stderr, file stream, etc.) - must not be NULL
 * @param level Severity level (controls prefix only: "[INFO] ", "[WARN] ", "[ERROR] ")
 * @param fmt printf-style format string - must not be NULL
 * @param args Variable arguments list (va_list) - must be initialized with va_start()
 *
 * @return Ok(true) if entire operation succeeded (write + flush)
 *         Err("static reason") if any step failed
 *
 * Preconditions:
 * - stream is a valid, open FILE* (not NULL)
 * - fmt is a valid format string (not NULL)
 * - args has been initialized with va_start()
 * - Format string and args match (UB if they don't - no validation possible)
 *
 * Postconditions:
 * - If Ok: stream contains "[LEVEL] formatted_message\n" and is flushed
 * - If Err: stream may contain partial output (fputs/fprintf are not atomic)
 * - args is consumed (do not reuse - call va_end() and reinitialize if needed)
 * - Stream position advanced
 * - Stream error indicator may be set on failure
 *
 * Output format:
 * "[LEVEL] " + vfprintf(fmt, args) + "\n"
 *
 * Failure modes (returns Err):
 * - stream is NULL → "null output stream"
 * - fmt is NULL → "null format string"
 * - fputs fails (prefix) → "failed to write log prefix"
 * - vfprintf fails → "failed to format/write message"
 * - fputc fails (newline) → "failed to write newline"
 * - fflush fails → "failed to flush stream"
 *
 * Performance:
 * - Time complexity: O(n) where n = formatted message length
 * - Space complexity: O(1) — no allocations, stack-only
 * - I/O operations: 3–4 writes (prefix, message, newline) + 1 flush
 *
 * Thread-safety:
 * - NOT thread-safe without external locking
 * - Concurrent calls with same stream = interleaved output
 * - Use flockfile/funlockfile or mutex for thread-safe logging
 * - Different streams = safe (no shared state)
 *
 * Signal-safety:
 * - NOT async-signal-safe (vfprintf is not)
 * - Do not call from signal handlers
 *
 * Example:
 * ```c
 * void my_logger(FILE* output, const char* fmt, ...) {
 *     va_list args;
 *     va_start(args, fmt);
 *     result_bool_constcharp res = log_vfmt_to(output, LOG_INFO, fmt, args);
 *     va_end(args);
 *
 *     if (result_is_err(res)) {
 *         fprintf(stderr, "Logging failed: %s\n", result_unwrap_err(res));
 *     }
 * }
 * ```
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
    // Select prefix based on level
    const char* prefix = "";
    switch (level) {
        case LOG_INFO: prefix = "[INFO] "; break;
        case LOG_WARN: prefix = "[WARN] "; break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
    }
    // Write prefix
    if (fputs(prefix, stream) == EOF) {
        return result_bool_constcharp_err("failed to write log prefix");
    }
    // Format and write message
    if (vfprintf(stream, fmt, args) < 0) {
        return result_bool_constcharp_err("failed to format/write message");
    }
    // Write newline
    if (fputc('\n', stream) == EOF) {
        return result_bool_constcharp_err("failed to write newline");
    }
    // Flush to ensure visibility
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
 * Wrapper around log_vfmt_to() that accepts variadic arguments directly
 * instead of va_list. More convenient for most use cases.
 *
 * @param stream Output stream (must not be NULL)
 * @param level Log level (determines prefix)
 * @param fmt printf-style format string (must not be NULL)
 * @param ... Format arguments matching format string
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
 * Logs a plain string without format specifiers. Safer than log_fmt_to()
 * when logging user-controlled strings (no format string vulnerabilities).
 *
 * @param stream Output stream (must not be NULL)
 * @param level Log level (determines prefix)
 * @param msg Message string (must not be NULL, treated as literal text)
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
 * Convenience wrapper that automatically selects the appropriate stream
 * based on log level:
 * - LOG_INFO → stdout
 * - LOG_WARN → stdout
 * - LOG_ERROR → stderr
 *
 * Most commonly used logging function for applications that log to console.
 *
 * @param level Log level (determines both prefix and stream)
 * @param fmt printf-style format string (must not be NULL)
 * @param ... Format arguments matching format string
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
 * Convenience wrapper for logging plain strings to default streams.
 *
 * @param level Log level (determines both prefix and stream)
 * @param msg Message string (must not be NULL, treated as literal)
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
 * Convenience macros for the common case where logging failures are
 * non-critical and can be safely ignored. These simply cast the Result
 * to void, discarding any error information.
 */
#define LOG_INFO(msg)       (void)log_msg(LOG_INFO, (msg))
#define LOG_WARN(msg)       (void)log_msg(LOG_WARN, (msg))
#define LOG_ERROR(msg)      (void)log_msg(LOG_ERROR, (msg))
#define LOG_INFO_FMT(...)   (void)log_fmt(LOG_INFO, __VA_ARGS__)
#define LOG_WARN_FMT(...)   (void)log_fmt(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR_FMT(...)  (void)log_fmt(LOG_ERROR, __VA_ARGS__)

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ────────────────────────────────────────────────────────────────────────── */

#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>

// Example 1: Basic fire-and-forget logging
void example_basic_logging(void) {
    LOG_INFO("Application started");
    LOG_INFO_FMT("Running on %s version %s", OS_NAME, VERSION);
    
    int port = 8080;
    LOG_INFO_FMT("Server listening on port %d", port);
    
    LOG_WARN("Configuration file not found, using defaults");
    LOG_ERROR_FMT("Failed to connect to database at %s", db_host);
}

// Example 2: Explicit error handling for critical logs
void example_critical_logging(void) {
    const char* username = "admin";
    const char* action = "DELETE_DATABASE";
    
    result_bool_constcharp res = log_fmt(LOG_ERROR,
                                          "AUDIT: User %s performed %s",
                                          username, action);
    if (result_is_err(res)) {
        fprintf(stderr, "FATAL: Audit log failed: %s\n",
                result_unwrap_err(res));
        abort(); // Security requirement: audit logs must never fail
    }
}

// Example 3: Logging to file
void example_file_logging(void) {
    FILE* logfile = fopen("application.log", "a");
    if (!logfile) {
        LOG_ERROR("Failed to open log file");
        return;
    }
    
    log_fmt_to(logfile, LOG_INFO, "Session started at %ld", time(NULL));
    log_fmt_to(logfile, LOG_INFO, "Processing user request");
    log_fmt_to(logfile, LOG_WARN, "High memory usage detected");
    
    fclose(logfile);
}

// Example 4: Fallback logging strategy
void example_fallback_logging(void) {
    FILE* primary = fopen("primary.log", "a");
    FILE* backup = fopen("backup.log", "a");
    
    result_bool_constcharp res = log_fmt_to(primary, LOG_INFO,
                                             "Important event occurred");
    if (result_is_err(res) && backup) {
        // Primary failed, try backup
        log_fmt_to(backup, LOG_WARN,
                  "Event logged to backup (primary failed)");
    }
    
    if (primary) fclose(primary);
    if (backup) fclose(backup);
}

// Example 5: Custom wrapper with timestamps
result_bool_constcharp log_with_time(log_level level, const char* fmt, ...) {
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
             localtime(&now));
    
    char full_fmt[512];
    snprintf(full_fmt, sizeof(full_fmt), "[%s] %s", timestamp, fmt);
    
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    
    va_list args;
    va_start(args, fmt);
    result_bool_constcharp res = log_vfmt_to(stream, level, full_fmt, args);
    va_end(args);
    
    return res;
}

void example_custom_logger(void) {
    log_with_time(LOG_INFO, "Custom timestamped log");
    log_with_time(LOG_ERROR, "Error at %s:%d", __FILE__, __LINE__);
}

// Example 6: Accumulating log failures
void example_failure_tracking(void) {
    FILE* logfile = fopen("events.log", "a");
    int failures = 0;
    
    for (int i = 0; i < 100; i++) {
        result_bool_constcharp res = log_fmt_to(logfile, LOG_INFO,
                                                 "Event %d processed", i);
        if (result_is_err(res)) {
            failures++;
        }
    }
    
    if (failures > 0) {
        LOG_WARN_FMT("Lost %d log messages due to I/O errors", failures);
    }
    
    if (logfile) fclose(logfile);
}

// Example 7: Per-module logging
typedef struct {
    const char* module_name;
    FILE* log_stream;
} ModuleLogger;

result_bool_constcharp module_log(ModuleLogger* logger, log_level level,
                                   const char* fmt, ...) {
    char full_fmt[512];
    snprintf(full_fmt, sizeof(full_fmt), "[%s] %s",
             logger->module_name, fmt);
    
    va_list args;
    va_start(args, fmt);
    result_bool_constcharp res = log_vfmt_to(logger->log_stream, level,
                                               full_fmt, args);
    va_end(args);
    return res;
}

void example_module_logging(void) {
    ModuleLogger auth_logger = {
        .module_name = "AUTH",
        .log_stream = stdout
    };
    
    module_log(&auth_logger, LOG_INFO, "User login attempt");
    module_log(&auth_logger, LOG_ERROR, "Invalid password");
}

// Example 8: Conditional compilation for debug logs
#ifdef DEBUG
#define DEBUG_LOG(msg) LOG_INFO(msg)
#define DEBUG_LOG_FMT(...) LOG_INFO_FMT(__VA_ARGS__)
#else
#define DEBUG_LOG(msg) ((void)0)
#define DEBUG_LOG_FMT(...) ((void)0)
#endif

void example_debug_logging(void) {
    DEBUG_LOG("Entering critical section");
    DEBUG_LOG_FMT("Variable x = %d, y = %d", x, y);
    // These compile to nothing in release builds
}

// Example 9: Safe logging of user input
void example_safe_user_input(void) {
    char user_input[256];
    printf("Enter a message: ");
    fgets(user_input, sizeof(user_input), stdin);
    
    // SAFE: user_input treated as literal text
    log_to(stdout, LOG_INFO, user_input);
    
    // ALSO SAFE: explicit format string
    log_fmt_to(stdout, LOG_INFO, "User said: %s", user_input);
    
    // UNSAFE: format string vulnerability if user_input contains %n, etc.
    // log_fmt_to(stdout, LOG_INFO, user_input); // DON'T DO THIS
}

// Example 10: Error recovery with logging
bool connect_to_database(const char* host, int max_retries) {
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        LOG_INFO_FMT("Connection attempt %d/%d to %s",
                    attempt, max_retries, host);
        
        if (try_connect(host)) {
            LOG_INFO_FMT("Connected to %s on attempt %d", host, attempt);
            return true;
        }
        
        LOG_WARN_FMT("Connection attempt %d failed, retrying...", attempt);
        sleep(1);
    }
    
    LOG_ERROR_FMT("Failed to connect after %d attempts", max_retries);
    return false;
}

// Example 11: Structured logging pattern
typedef struct {
    const char* user_id;
    const char* action;
    const char* resource;
    bool success;
} AuditEvent;

result_bool_constcharp log_audit_event(const AuditEvent* event) {
    return log_fmt(LOG_INFO, "AUDIT: user=%s action=%s resource=%s result=%s",
                  event->user_id,
                  event->action,
                  event->resource,
                  event->success ? "SUCCESS" : "FAILURE");
}

void example_audit_logging(void) {
    AuditEvent event = {
        .user_id = "user123",
        .action = "DELETE",
        .resource = "/files/document.txt",
        .success = true
    };
    
    result_bool_constcharp res = log_audit_event(&event);
    if (result_is_err(res)) {
        // Audit logging failed - this is critical
        fprintf(stderr, "CRITICAL: Audit log failed\n");
    }
}

// Example 12: Multi-stream logging (console + file)
void example_dual_logging(void) {
    FILE* logfile = fopen("application.log", "a");
    
    const char* msg = "Important system event";
    
    // Log to both console and file
    log_fmt(LOG_INFO, "%s", msg);
    if (logfile) {
        log_fmt_to(logfile, LOG_INFO, "%s", msg);
        fclose(logfile);
    }
}

// Example 13: Performance-sensitive logging with batching
void example_batch_logging(void) {
    FILE* logfile = fopen("performance.log", "a");
    if (!logfile) return;
    
    // Collect multiple log messages
    char batch[4096] = "";
    for (int i = 0; i < 100; i++) {
        char line[64];
        snprintf(line, sizeof(line), "[INFO] Event %d\n", i);
        strcat(batch, line);
    }
    
    // Write all at once (more efficient)
    fputs(batch, logfile);
    fflush(logfile);
    
    fclose(logfile);
}

// Example 14: Log rotation helper
bool rotate_log_if_needed(const char* logpath, size_t max_size) {
    FILE* f = fopen(logpath, "r");
    if (!f) return false;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    
    if (size > (long)max_size) {
        char old_path[512];
        snprintf(old_path, sizeof(old_path), "%s.old", logpath);
        rename(logpath, old_path);
        
        LOG_INFO_FMT("Rotated log file %s (size: %ld bytes)",
                    logpath, size);
        return true;
    }
    return false;
}

// Example 15: Integration with error handling system
typedef enum {
    ERR_OK,
    ERR_FILE_NOT_FOUND,
    ERR_PERMISSION_DENIED,
    ERR_OUT_OF_MEMORY
} ErrorCode;

const char* error_to_string(ErrorCode err) {
    switch (err) {
        case ERR_OK: return "Success";
        case ERR_FILE_NOT_FOUND: return "File not found";
        case ERR_PERMISSION_DENIED: return "Permission denied";
        case ERR_OUT_OF_MEMORY: return "Out of memory";
        default: return "Unknown error";
    }
}

ErrorCode process_file(const char* path) {
    LOG_INFO_FMT("Processing file: %s", path);
    
    FILE* f = fopen(path, "r");
    if (!f) {
        ErrorCode err = ERR_FILE_NOT_FOUND;
        LOG_ERROR_FMT("Failed to open %s: %s", path, error_to_string(err));
        return err;
    }
    
    // Process file...
    LOG_INFO_FMT("Successfully processed %s", path);
    fclose(f);
    return ERR_OK;
}

#endif /* CANON_UTIL_LOG_H */
