// util/log_macros.h
#ifndef CANON_UTIL_LOG_MACROS_H
#define CANON_UTIL_LOG_MACROS_H

#include "util/log.h"

/**
 * @file log_macros.h
 * @brief Convenient, safe and expressive logging macros
 *
 * Provides ergonomic macro interface for common logging operations with clear
 * separation between simple messages and formatted output. Implements both
 * fire-and-forget convenience macros and debug-aware checked variants that
 * detect logging failures during development.
 *
 * Portability:
 *   - Requires C99 or later (for variadic macros)
 *   - Depends on log.h from this library (for log_msg, log_fmt)
 *   - Uses standard NDEBUG macro for conditional compilation
 *   - fprintf to stderr in debug mode (requires <stdio.h>)
 *   - Platform-independent (no OS-specific features)
 *
 * Thread-safety: Inherits thread-safety from underlying log.h functions.
 *                Macros themselves are safe (no shared state). Multiple
 *                threads can log simultaneously if log.h backend is thread-safe.
 *
 * Performance:
 *   - Zero overhead in release builds with NDEBUG defined
 *   - Simple message macros: ~100-500ns (no formatting, direct write)
 *   - Formatted macros: ~1-5μs (depends on format complexity)
 *   - Checked macros (debug): +~50ns for error checking overhead
 *   - No heap allocations - stack-based formatting via log.h
 *   - Minimal code size impact (inline macro expansions)
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Logging should be effortless — one macro call, done
 * - Errors in logging shouldn't crash your program
 * - But errors in logging shouldn't be invisible either (debug mode)
 * - Simple messages should be faster than formatted ones
 * - Production code shouldn't pay for debug-time safety checks
 * - Standard log levels: INFO (informational), WARN (concerning), ERROR (failure)
 * - Fire-and-forget default — explicit checking only when needed
 * - Conditional compilation separates debug/release behavior
 * - Zero allocations — all formatting stack-based
 * - Type-safe variadic arguments via __VA_ARGS__
 *
 * Three-tier logging approach:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. INFO  — Informational messages (server started, user logged in, etc.)
 *           Use for: normal operations, status updates, tracking flow
 * 
 * 2. WARN  — Warning messages (deprecated API, retry attempt, etc.)
 *           Use for: concerning but non-fatal situations, degraded performance
 * 
 * 3. ERROR — Error messages (connection failed, invalid input, etc.)
 *           Use for: failures, exceptions, things that went wrong
 *
 * No TRACE/DEBUG levels by design — use conditional compilation or a separate
 * debug logging system if you need development-only verbose logging.
 *
 * Macro variants explained:
 * ────────────────────────────────────────────────────────────────────────────
 * There are three styles of logging macros:
 *
 * 1. *_MSG macros — Simple string output (most efficient):
 *    LOG_INFO_MSG("Server initialized");
 *    - Takes a single string argument (no formatting)
 *    - Fastest option — direct write, no vfprintf overhead
 *    - Use for: literal/static messages
 *
 * 2. *_FMT macros — Printf-style formatting (most common):
 *    LOG_INFO_FMT("Connected to %s:%d", host, port);
 *    - Takes format string + variadic arguments
 *    - Formatted via vfprintf/vsnprintf
 *    - Use for: messages with dynamic data
 *
 * 3. *_CHECKED macros — Verified logging (debug safety):
 *    LOG_ERROR_CHECKED("Failed to open %s: %s", file, strerror(errno));
 *    - Debug mode: checks if logging succeeded, prints meta-error on failure
 *    - Release mode: identical to *_FMT macros (zero overhead)
 *    - Use during: development/testing to catch logging infrastructure issues
 *
 * Fire-and-forget vs checked:
 * ────────────────────────────────────────────────────────────────────────────
 * FIRE-AND-FORGET (*_MSG, *_FMT):
 * - Calls log function, discards Result with (void) cast
 * - Log failures are silent — program continues regardless
 * - Appropriate for: production code where logging is best-effort
 * - Minimal overhead: just the log operation itself
 *
 * CHECKED (*_CHECKED):
 * - Debug mode: inspects Result, prints to stderr if logging failed
 * - Release mode: becomes fire-and-forget (NDEBUG strips checks)
 * - Appropriate for: development, catching infrastructure issues
 * - Meta-error format: "[LOG-FAIL] Could not write error message: <reason>"
 *
 * When logging itself fails (disk full, stream closed, permission denied),
 * checked macros help you notice during development. In production, these
 * compile away to normal fire-and-forget behavior.
 *
 * NDEBUG behavior:
 * ────────────────────────────────────────────────────────────────────────────
 * The standard NDEBUG macro controls checked macro behavior:
 *
 * Debug builds (NDEBUG not defined):
 *   - Assertions enabled
 *   - Checked macros perform error detection
 *   - Meta-logging to stderr on log failures
 *   - Slightly larger code size
 *   - ~50ns overhead per checked log call
 *
 * Release builds (NDEBUG defined):
 *   - Assertions disabled
 *   - Checked macros become plain fire-and-forget
 *   - No meta-logging
 *   - Minimal code size
 *   - Zero overhead compared to normal macros
 *
 * Compilation examples:
 *   gcc -DNDEBUG ...        # Release: checked = fire-and-forget
 *   gcc ...                 # Debug: checked = verified
 *
 * Common usage patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * Application startup:
 *   LOG_INFO_MSG("Application starting");
 *   LOG_INFO_FMT("Version %s built on %s", VERSION, __DATE__);
 *
 * User actions:
 *   LOG_INFO_FMT("User '%s' logged in from %s", username, ip);
 *   LOG_WARN_FMT("User '%s' failed login attempt #%d", username, attempts);
 *
 * Error reporting:
 *   LOG_ERROR_FMT("Database connection failed: %s", error_message);
 *   LOG_ERROR_CHECKED("Fatal: config file missing: %s", path);
 *
 * Development/debugging:
 *   LOG_INFO_CHECKED("Processing request %zu", request_id);
 *   LOG_WARN_CHECKED("Cache miss for key '%s'", key);
 *
 * Simple status messages:
 *   LOG_INFO_MSG("Server ready");
 *   LOG_WARN_MSG("Deprecated API called");
 *   LOG_ERROR_MSG("Out of memory");
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Application lifecycle logging (startup, shutdown)
 * - Request/response tracking in servers
 * - Error condition reporting
 * - Security event logging (authentication, authorization)
 * - Performance warning detection
 * - Configuration loading status
 * - Resource allocation/deallocation tracking
 * - API deprecation warnings
 * - Integration point monitoring
 * - User action auditing
 *
 * Performance considerations:
 * ────────────────────────────────────────────────────────────────────────────
 * Logging performance from fastest to slowest:
 *
 * 1. LOG_*_MSG (no formatting):        ~100-500ns
 *    - Direct string write
 *    - No vfprintf overhead
 *    - Minimal stack usage
 *
 * 2. LOG_*_FMT (simple format):        ~1-3μs
 *    - Format string with 1-3 arguments
 *    - Stack-based formatting buffer
 *    - Single write operation
 *
 * 3. LOG_*_FMT (complex format):       ~3-10μs
 *    - Many arguments or complex specifiers
 *    - More stack/CPU for formatting
 *    - Still single write
 *
 * 4. LOG_*_CHECKED (debug only):       +~50ns overhead
 *    - Result checking and possible fprintf
 *    - Only in debug builds
 *    - Release builds: same as unchecked
 *
 * Best practices:
 * - Use *_MSG when possible for constant strings
 * - Avoid logging in tight loops (hot paths)
 * - Consider log level filtering for production
 * - Keep format strings simple
 * - Pre-calculate expensive values before logging
 *
 * Example performance anti-pattern:
 * ```c
 * // DON'T DO THIS in hot loop:
 * for (int i = 0; i < 1000000; i++) {
 *     LOG_INFO_FMT("Processing item %d", i);  // 1-10ms total!
 * }
 * 
 * // Better:
 * LOG_INFO_MSG("Processing started");
 * for (int i = 0; i < 1000000; i++) {
 *     process_item(i);
 * }
 * LOG_INFO_MSG("Processing complete");
 * ```
 *
 * Integration with log.h:
 * ────────────────────────────────────────────────────────────────────────────
 * These macros are thin wrappers around log.h functions:
 *
 * - LOG_*_MSG(msg)  → log_msg(level, msg)
 * - LOG_*_FMT(...)  → log_fmt(level, ...)
 *
 * All actual logging logic lives in log.h (output destination, formatting,
 * timestamps, etc.). This file only provides convenient macro syntax.
 *
 * To configure logging behavior (output file, format, etc.), see log.h
 * documentation. These macros just provide the interface.
 *
 * Error handling philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * Logging errors are fundamentally different from application errors:
 *
 * - Application errors: must be handled (user data at risk)
 * - Logging errors: can be ignored (diagnostic data lost, but app continues)
 *
 * Therefore:
 * - Production: logging is best-effort, failures are acceptable
 * - Development: logging failures indicate infrastructure issues
 *
 * This is why checked variants exist only in debug builds — they help you
 * notice when your logging setup is broken (wrong permissions, disk full,
 * invalid stream, etc.) during development, but don't burden production code.
 *
 * Usage examples:
 *
 * Basic application logging:
 * ```c
 * int main(void) {
 *     LOG_INFO_MSG("Application started");
 *     
 *     if (!initialize_systems()) {
 *         LOG_ERROR_MSG("System initialization failed");
 *         return 1;
 *     }
 *     
 *     LOG_INFO_FMT("Listening on port %d", PORT);
 *     
 *     run_server();
 *     
 *     LOG_INFO_MSG("Shutting down");
 *     return 0;
 * }
 * ```
 *
 * Error reporting with context:
 * ```c
 * FILE* open_config(const char* path) {
 *     FILE* f = fopen(path, "r");
 *     if (!f) {
 *         LOG_ERROR_FMT("Failed to open config '%s': %s", 
 *                       path, strerror(errno));
 *         return NULL;
 *     }
 *     
 *     LOG_INFO_FMT("Loaded config from '%s'", path);
 *     return f;
 * }
 * ```
 *
 * Warning for concerning situations:
 * ```c
 * void process_request(Request* req) {
 *     if (req->size > MAX_SIZE) {
 *         LOG_WARN_FMT("Large request: %zu bytes (max %zu)", 
 *                      req->size, MAX_SIZE);
 *     }
 *     
 *     if (req->use_deprecated_api) {
 *         LOG_WARN_MSG("Client using deprecated API v1");
 *     }
 *     
 *     // continue processing...
 * }
 * ```
 *
 * Development debugging with checked macros:
 * ```c
 * void debug_session(void) {
 *     LOG_INFO_CHECKED("Debug session started");
 *     
 *     for (size_t i = 0; i < data_count; i++) {
 *         LOG_INFO_CHECKED("Processing record %zu/%zu", i + 1, data_count);
 *         process_record(i);
 *     }
 *     
 *     LOG_INFO_CHECKED("Debug session complete");
 * }
 * // In debug builds, you'll see meta-errors if logging fails
 * // In release builds, identical to LOG_INFO_FMT
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Simple message macros (most efficient for literal strings)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Logs a simple static/literal message at INFO level
 *
 * Most efficient logging option for constant strings. Directly writes the
 * message without any formatting overhead. Use this when you have a static
 * string and don't need to include any dynamic data.
 *
 * This is a fire-and-forget macro — the log operation's Result is discarded
 * with a (void) cast. If logging fails (disk full, stream closed, etc.),
 * the program continues without notification.
 *
 * @param msg Message string (literal or const char* variable, no formatting)
 *
 * Behavior:
 *   - Calls log_msg(LOG_INFO, msg)
 *   - Discards return value
 *   - Never blocks (beyond the write syscall itself)
 *   - No error indication
 *
 * Performance:
 *   - Time: ~100-500ns typical
 *   - Space: Stack only (no heap)
 *   - I/O: Single write operation
 *
 * Thread-safety: Safe to call from multiple threads (log.h handles locking)
 *
 * Example:
 * ```c
 * LOG_INFO_MSG("Server initialized successfully");
 * LOG_INFO_MSG("Ready to accept connections");
 * 
 * const char* status = "All systems operational";
 * LOG_INFO_MSG(status);
 * ```
 *
 * When to use:
 *   ✓ Static/literal strings
 *   ✓ Status messages
 *   ✓ Simple state transitions
 *   ✓ High-frequency simple logs
 *
 * When NOT to use:
 *   ✗ Need to include variables (use LOG_INFO_FMT)
 *   ✗ Need error checking (use LOG_INFO_CHECKED)
 *   ✗ Dynamic message construction
 */
#define LOG_INFO_MSG(msg)    (void)log_msg(LOG_INFO, (msg))

/**
 * @brief Logs a simple static/literal message at WARN level
 *
 * Warning-level equivalent of LOG_INFO_MSG. Use for concerning situations
 * that don't represent errors but should be noticed.
 *
 * @param msg Warning message string
 *
 * Example:
 * ```c
 * LOG_WARN_MSG("Deprecated function called");
 * LOG_WARN_MSG("Connection retry attempt");
 * LOG_WARN_MSG("Cache size approaching limit");
 * ```
 *
 * Appropriate warnings:
 *   - Deprecated API usage
 *   - Resource limits approaching
 *   - Retry attempts
 *   - Fallback behaviors triggered
 *   - Non-fatal anomalies
 */
#define LOG_WARN_MSG(msg)    (void)log_msg(LOG_WARN, (msg))

/**
 * @brief Logs a simple static/literal message at ERROR level
 *
 * Error-level equivalent of LOG_INFO_MSG. Use for failures and error
 * conditions that don't include dynamic context.
 *
 * @param msg Error message string
 *
 * Example:
 * ```c
 * LOG_ERROR_MSG("Out of memory");
 * LOG_ERROR_MSG("Database connection lost");
 * LOG_ERROR_MSG("Invalid configuration");
 * ```
 *
 * Appropriate errors:
 *   - Resource exhaustion
 *   - Connection failures
 *   - Invalid state
 *   - Assertion failures
 *   - Fatal errors
 */
#define LOG_ERROR_MSG(msg)   (void)log_msg(LOG_ERROR, (msg))

/* ────────────────────────────────────────────────────────────────────────────
   Formatted logging macros (most commonly used)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging at INFO level
 *
 * Printf-style formatted logging for informational messages with dynamic
 * data. Most commonly used logging macro. Takes a format string and
 * variadic arguments, formats them via log.h, and writes the result.
 *
 * This is a fire-and-forget macro — logging failures are silent.
 *
 * @param ... printf-style format string followed by arguments
 *
 * Behavior:
 *   - Calls log_fmt(LOG_INFO, format, args...)
 *   - Formats message using vsnprintf/vfprintf
 *   - Discards Result (fire-and-forget)
 *   - No error indication
 *
 * Performance:
 *   - Time: ~1-5μs depending on format complexity
 *   - Space: Stack-based formatting buffer
 *   - I/O: Single write after formatting
 *
 * Format specifiers: Standard printf format specifiers are supported:
 *   %d, %i    - int
 *   %u        - unsigned int
 *   %ld, %lu  - long, unsigned long
 *   %zu       - size_t
 *   %f, %lf   - double
 *   %s        - string
 *   %p        - pointer
 *   %x, %X    - hexadecimal
 *   %%        - literal %
 *
 * Example:
 * ```c
 * LOG_INFO_FMT("Server started on port %d", port);
 * LOG_INFO_FMT("User '%s' connected from %s", username, ip_address);
 * LOG_INFO_FMT("Processed %zu requests in %.2f seconds", count, elapsed);
 * LOG_INFO_FMT("Memory usage: %zu/%zu MB", used_mb, total_mb);
 * ```
 *
 * Common patterns:
 * ```c
 * // Numeric values
 * LOG_INFO_FMT("Thread %d ready", thread_id);
 * 
 * // Strings and identifiers
 * LOG_INFO_FMT("Loaded module: %s", module_name);
 * 
 * // Multiple values
 * LOG_INFO_FMT("Connection %d from %s:%d", conn_id, host, port);
 * 
 * // Floating point
 * LOG_INFO_FMT("Average latency: %.3f ms", avg_latency);
 * 
 * // Hexadecimal (debugging)
 * LOG_INFO_FMT("Session ID: 0x%016lx", session_id);
 * ```
 *
 * When to use:
 *   ✓ Messages with dynamic data
 *   ✓ Logging variables, counters, IDs
 *   ✓ User actions with context
 *   ✓ General application logging
 *
 * When NOT to use:
 *   ✗ Static strings only (use LOG_INFO_MSG instead)
 *   ✗ Hot loops (performance sensitive)
 *   ✗ When you need error checking (use LOG_INFO_CHECKED)
 */
#define LOG_INFO_FMT(...)    (void)log_fmt(LOG_INFO, __VA_ARGS__)

/**
 * @brief Formatted logging at WARN level
 *
 * Warning-level equivalent of LOG_INFO_FMT. Use for concerning situations
 * with dynamic context that should be investigated but aren't fatal errors.
 *
 * @param ... printf-style format string and arguments
 *
 * Example:
 * ```c
 * LOG_WARN_FMT("Retry attempt %d/%d for %s", attempt, max_attempts, resource);
 * LOG_WARN_FMT("Request from %s exceeded rate limit: %d req/s", ip, rate);
 * LOG_WARN_FMT("Cache hit ratio low: %.1f%% (threshold: %.1f%%)", ratio, threshold);
 * LOG_WARN_FMT("Function %s is deprecated, use %s instead", old_name, new_name);
 * ```
 *
 * Appropriate warnings:
 *   - Resource usage approaching limits
 *   - Retry logic triggered
 *   - Performance degradation detected
 *   - Configuration suboptimal
 *   - Deprecated features in use
 */
#define LOG_WARN_FMT(...)    (void)log_fmt(LOG_WARN, __VA_ARGS__)

/**
 * @brief Formatted logging at ERROR level
 *
 * Error-level equivalent of LOG_INFO_FMT. Use for failures and error
 * conditions with dynamic context.
 *
 * @param ... printf-style format string and arguments
 *
 * Example:
 * ```c
 * LOG_ERROR_FMT("Failed to open file '%s': %s", path, strerror(errno));
 * LOG_ERROR_FMT("Database query failed: %s", db_error_msg);
 * LOG_ERROR_FMT("Invalid request: expected %s, got %s", expected, actual);
 * LOG_ERROR_FMT("Connection to %s:%d timed out after %d seconds", host, port, timeout);
 * ```
 *
 * Best practices:
 *   - Include enough context to diagnose the problem
 *   - Log error codes, errno values, or error messages
 *   - Include operation that failed
 *   - Include resource identifier (file, connection, etc.)
 *   - Avoid redundant information
 *
 * Good error logging:
 * ```c
 * LOG_ERROR_FMT("Failed to connect to database %s: %s", db_name, error);
 * ```
 *
 * Poor error logging:
 * ```c
 * LOG_ERROR_MSG("Error");  // Not enough context!
 * ```
 */
#define LOG_ERROR_FMT(...)   (void)log_fmt(LOG_ERROR, __VA_ARGS__)

/* ────────────────────────────────────────────────────────────────────────────
   Checked variants — safety net during development
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checked logging macros (behavior changes with NDEBUG)
 *
 * These macros provide error detection for logging operations in debug
 * builds, helping you catch infrastructure problems (disk full, closed
 * streams, permission errors) during development. In release builds, they
 * become identical to regular fire-and-forget macros.
 *
 * DEBUG MODE (NDEBUG not defined):
 * ────────────────────────────────────────────────────────────────────────────
 * - Executes the log operation
 * - Checks the returned Result<bool, const char*>
 * - On failure (Err case):
 *   - Prints meta-error to stderr
 *   - Format: "[LOG-FAIL] Could not write error message: <reason>"
 * - Program continues normally (doesn't abort)
 *
 * RELEASE MODE (NDEBUG defined):
 * ────────────────────────────────────────────────────────────────────────────
 * - Identical to LOG_*_FMT macros
 * - No error checking
 * - No meta-logging
 * - Zero overhead
 * - Fire-and-forget behavior
 *
 * When to use checked macros:
 *   ✓ Development and testing
 *   ✓ Critical error logging you want verified
 *   ✓ Debugging logging infrastructure issues
 *   ✓ When log failures should be visible
 *
 * When NOT to use:
 *   ✗ Production-only code (use regular macros)
 *   ✗ High-frequency logging (use regular macros)
 *   ✗ When log failures are acceptable
 *
 * Common infrastructure issues caught:
 *   - Log file permissions wrong
 *   - Disk full / quota exceeded
 *   - Log stream closed prematurely
 *   - Invalid file descriptor
 *   - Network log sink unreachable
 *   - Filesystem mounted read-only
 *
 * Example meta-error output (debug mode):
 * ```
 * [LOG-FAIL] Could not write error message: No space left on device
 * [LOG-FAIL] Could not write error message: Permission denied
 * [LOG-FAIL] Could not write error message: Bad file descriptor
 * ```
 *
 * Compilation differences:
 * ```c
 * // debug_build.c (compiled without -DNDEBUG)
 * LOG_ERROR_CHECKED("Connection failed: %s", error);
 * // Expands to:
 * do {
 *     result_bool_constcharp _r = log_fmt(LOG_ERROR, "Connection failed: %s", error);
 *     if (result_bool_constcharp_is_err(_r)) {
 *         fprintf(stderr, "[LOG-FAIL] Could not write error message: %s\n",
 *                 result_bool_constcharp_unwrap_err(_r));
 *     }
 * } while (0)
 * 
 * // release_build.c (compiled with -DNDEBUG)
 * LOG_ERROR_CHECKED("Connection failed: %s", error);
 * // Expands to:
 * (void)log_fmt(LOG_ERROR, "Connection failed: %s", error)
 * ```
 */

#ifdef NDEBUG
    /**
     * @brief Checked INFO logging (release: fire-and-forget)
     *
     * In release builds, identical to LOG_INFO_FMT.
     * No error checking, minimal overhead.
     */
    #define LOG_INFO_CHECKED(...)   LOG_INFO_FMT(__VA_ARGS__)
    
    /**
     * @brief Checked WARN logging (release: fire-and-forget)
     *
     * In release builds, identical to LOG_WARN_FMT.
     * No error checking, minimal overhead.
     */
    #define LOG_WARN_CHECKED(...)   LOG_WARN_FMT(__VA_ARGS__)
    
    /**
     * @brief Checked ERROR logging (release: fire-and-forget)
     *
     * In release builds, identical to LOG_ERROR_FMT.
     * No error checking, minimal overhead.
     */
    #define LOG_ERROR_CHECKED(...)  LOG_ERROR_FMT(__VA_ARGS__)
#else
    /**
     * @brief Checked INFO logging (debug: verified)
     *
     * In debug builds, logs message and checks for success.
     * On logging failure, prints meta-error to stderr.
     *
     * @param ... Format string and arguments
     *
     * Example:
     * ```c
     * LOG_INFO_CHECKED("Processing batch %zu/%zu", current, total);
     * // If logging fails (e.g., disk full):
     * // stderr: [LOG-FAIL] Could not write error message: No space left on device
     * ```
     */
    #define LOG_INFO_CHECKED(...)   \
        do { \
            result_bool_constcharp _r = log_fmt(LOG_INFO, __VA_ARGS__); \
            if (result_bool_constcharp_is_err(_r)) { \
                fprintf(stderr, "[LOG-FAIL] Could not write info message: %s\n", \
                        result_bool_constcharp_unwrap_err(_r)); \
            } \
        } while (0)

    /**
     * @brief Checked WARN logging (debug: verified)
     *
     * In debug builds, logs warning and checks for success.
     * On logging failure, prints meta-error to stderr.
     *
     * @param ... Format string and arguments
     *
     * Example:
     * ```c
     * LOG_WARN_CHECKED("Cache miss for key '%s'", key);
     * ```
     */
    #define LOG_WARN_CHECKED(...)   \
        do { \
            result_bool_constcharp _r = log_fmt(LOG_WARN, __VA_ARGS__); \
            if (result_bool_constcharp_is_err(_r)) { \
                fprintf(stderr, "[LOG-FAIL] Could not write warning message: %s\n", \
                        result_bool_constcharp_unwrap_err(_r)); \
            } \
        } while (0)

    /**
     * @brief Checked ERROR logging (debug: verified)
     *
     * In debug builds, logs error and checks for success.
     * On logging failure, prints meta-error to stderr.
     *
     * This is particularly important for error logging — if you can't even
     * log the error, you want to know about it during development.
     *
     * @param ... Format string and arguments
     *
     * Example:
     * ```c
     * LOG_ERROR_CHECKED("Failed to initialize module %s: %s", 
     *                   module_name, strerror(errno));
     * // If logging fails:
     * // stderr: [LOG-FAIL] Could not write error message: Permission denied
     * ```
     */
    #define LOG_ERROR_CHECKED(...)  \
        do { \
            result_bool_constcharp _r = log_fmt(LOG_ERROR, __VA_ARGS__); \
            if (result_bool_constcharp_is_err(_r)) { \
                fprintf(stderr, "[LOG-FAIL] Could not write error message: %s\n", \
                        result_bool_constcharp_unwrap_err(_r)); \
            } \
        } while (0)
#endif

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/log_macros.h"
    #include <errno.h>
    #include <string.h>
    
    // Example 1: Basic application lifecycle logging
    void example_application_lifecycle(void) {
        LOG_INFO_MSG("Application starting");
        
        if (!init_subsystems()) {
            LOG_ERROR_MSG("Subsystem initialization failed");
            return;
        }
        
        LOG_INFO_MSG("All subsystems initialized");
        LOG_INFO_FMT("Version %s ready", VERSION);
        
        run_main_loop();
        
        LOG_INFO_MSG("Shutting down gracefully");
    }
    
    // Example 2: Server request logging
    void example_request_logging(const char* client_ip, int port) {
        LOG_INFO_FMT("New connection from %s:%d", client_ip, port);
        
        Request req;
        if (!parse_request(&req)) {
            LOG_ERROR_FMT("Invalid request from %s:%d", client_ip, port);
            return;
        }
        
        LOG_INFO_FMT("Processing %s request to %s from %s", 
                     req.method, req.path, client_ip);
        
        Response resp = handle_request(&req);
        
        LOG_INFO_FMT("Responded with status %d to %s", resp.status, client_ip);
    }
    
    // Example 3: Error reporting with context
    void example_error_with_context(const char* filename) {
        FILE* f = fopen(filename, "r");
        if (!f) {
            LOG_ERROR_FMT("Failed to open '%s': %s", 
                          filename, strerror(errno));
            return;
        }
        
        LOG_INFO_FMT("Successfully opened '%s'", filename);
        
        // Process file...
        
        fclose(f);
    }
    
    // Example 4: Warning conditions
    void example_warnings(size_t cache_size, size_t max_size) {
        float usage = (float)cache_size / max_size * 100.0f;
        
        if (usage > 90.0f) {
            LOG_WARN_FMT("Cache usage critical: %.1f%% (%zu/%zu bytes)",
                         usage, cache_size, max_size);
        } else if (usage > 75.0f) {
            LOG_WARN_FMT("Cache usage high: %.1f%%", usage);
        }
        
        if (cache_size > max_size) {
            LOG_ERROR_FMT("Cache overflow: %zu bytes exceeds limit of %zu",
                          cache_size, max_size);
        }
    }
    
    // Example 5: Deprecated API warnings
    void old_api_function(void) {
        LOG_WARN_MSG("old_api_function() is deprecated, use new_api_function()");
        // ... implementation
    }
    
    // Example 6: User authentication logging
    void example_authentication(const char* username, const char* ip) {
        LOG_INFO_FMT("Login attempt for user '%s' from %s", username, ip);
        
        if (!verify_credentials(username)) {
            LOG_WARN_FMT("Failed login for '%s' from %s - invalid credentials",
                         username, ip);
            return;
        }
        
        if (!check_rate_limit(ip)) {
            LOG_WARN_FMT("Rate limit exceeded for IP %s (user: %s)", 
                         ip, username);
            return;
        }
        
        LOG_INFO_FMT("User '%s' logged in from %s", username, ip);
    }
    
    // Example 7: Database operation logging
    void example_database_ops(const char* query) {
        LOG_INFO_FMT("Executing query: %s", query);
        
        DBResult result = db_execute(query);
        if (result.error) {
            LOG_ERROR_FMT("Query failed: %s (error: %s)", 
                          query, result.error_msg);
            return;
        }
        
        LOG_INFO_FMT("Query succeeded, %zu rows affected", result.rows_affected);
    }
    
    // Example 8: Resource allocation tracking
    void* example_allocation(size_t size, const char* purpose) {
        LOG_INFO_FMT("Allocating %zu bytes for %s", size, purpose);
        
        void* ptr = malloc(size);
        if (!ptr) {
            LOG_ERROR_FMT("Failed to allocate %zu bytes for %s", 
                          size, purpose);
            return NULL;
        }
        
        LOG_INFO_FMT("Allocated %zu bytes at %p for %s", 
                     size, ptr, purpose);
        return ptr;
    }
    
    // Example 9: Performance monitoring
    void example_performance_tracking(void) {
        double start = get_time();
        
        process_batch();
        
        double elapsed = get_time() - start;
        
        if (elapsed > 1.0) {
            LOG_WARN_FMT("Batch processing slow: %.2f seconds (threshold: 1.0)",
                         elapsed);
        } else {
            LOG_INFO_FMT("Batch processed in %.2f seconds", elapsed);
        }
    }
    
    // Example 10: Checked logging in development
    void example_checked_logging(void) {
        #ifndef NDEBUG
        LOG_INFO_MSG("Debug mode active - using checked logging");
        #endif
        
        // These will print meta-errors in debug mode if logging fails
        LOG_INFO_CHECKED("Processing started at timestamp %ld", time(NULL));
        
        for (size_t i = 0; i < 100; i++) {
            if (i % 10 == 0) {
                LOG_INFO_CHECKED("Progress: %zu%%", i);
            }
            process_item(i);
        }
        
        LOG_INFO_CHECKED("Processing complete");
    }
    
    // Example 11: Configuration loading
    void example_config_loading(const char* config_file) {
        LOG_INFO_FMT("Loading configuration from '%s'", config_file);
        
        Config cfg;
        ConfigError err = load_config(config_file, &cfg);
        
        if (err != CONFIG_OK) {
            LOG_ERROR_FMT("Failed to load config from '%s': %s",
                          config_file, config_error_string(err));
            
            LOG_WARN_MSG("Using default configuration");
            cfg = default_config();
        } else {
            LOG_INFO_FMT("Configuration loaded: %zu settings", cfg.count);
        }
    }
    
    // Example 12: Network connection handling
    void example_network_connection(const char* host, int port) {
        LOG_INFO_FMT("Connecting to %s:%d", host, port);
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            LOG_ERROR_FMT("Failed to create socket: %s", strerror(errno));
            return;
        }
        
        if (connect_with_timeout(sock, host, port, 5) < 0) {
            LOG_ERROR_FMT("Connection to %s:%d failed: %s",
                          host, port, strerror(errno));
            close(sock);
            return;
        }
        
        LOG_INFO_FMT("Connected to %s:%d (socket %d)", host, port, sock);
    }
    
    // Example 13: Retry logic with warnings
    void example_retry_logic(const char* operation) {
        const int MAX_RETRIES = 3;
        
        for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
            LOG_INFO_FMT("Attempting %s (try %d/%d)", 
                         operation, attempt, MAX_RETRIES);
            
            if (try_operation()) {
                LOG_INFO_FMT("%s succeeded on attempt %d", operation, attempt);
                return;
            }
            
            if (attempt < MAX_RETRIES) {
                LOG_WARN_FMT("%s failed, retrying (%d/%d)", 
                             operation, attempt, MAX_RETRIES);
                sleep(attempt);  // Exponential backoff
            }
        }
        
        LOG_ERROR_FMT("%s failed after %d attempts", operation, MAX_RETRIES);
    }
    
    // Example 14: State transition logging
    typedef enum { STATE_INIT, STATE_RUNNING, STATE_PAUSED, STATE_STOPPED } State;
    
    void example_state_machine(State old_state, State new_state) {
        const char* state_names[] = {"INIT", "RUNNING", "PAUSED", "STOPPED"};
        
        LOG_INFO_FMT("State transition: %s -> %s",
                     state_names[old_state], state_names[new_state]);
        
        if (new_state == STATE_STOPPED && old_state != STATE_PAUSED) {
            LOG_WARN_FMT("Unexpected stop from %s state", 
                         state_names[old_state]);
        }
    }
    
    // Example 15: Batch processing with progress
    void example_batch_processing(size_t total_items) {
        LOG_INFO_FMT("Starting batch processing: %zu items", total_items);
        
        size_t processed = 0;
        size_t failed = 0;
        
        for (size_t i = 0; i < total_items; i++) {
            if (process_item(i)) {
                processed++;
            } else {
                failed++;
                LOG_WARN_FMT("Item %zu failed processing", i);
            }
            
            // Log progress every 10%
            if ((i + 1) % (total_items / 10) == 0) {
                size_t pct = ((i + 1) * 100) / total_items;
                LOG_INFO_FMT("Progress: %zu%% (%zu/%zu processed, %zu failed)",
                             pct, processed, total_items, failed);
            }
        }
        
        LOG_INFO_FMT("Batch complete: %zu succeeded, %zu failed", 
                     processed, failed);
        
        if (failed > 0) {
            float failure_rate = (float)failed / total_items * 100.0f;
            if (failure_rate > 10.0f) {
                LOG_ERROR_FMT("High failure rate: %.1f%%", failure_rate);
            } else {
                LOG_WARN_FMT("Some failures: %.1f%%", failure_rate);
            }
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_LOG_MACROS_H */
