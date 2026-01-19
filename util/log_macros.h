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
 * - Requires C99 or later (for variadic macros)
 * - Depends on log.h from this library (for log_msg, log_fmt)
 * - Uses standard NDEBUG macro for conditional compilation
 * - fprintf to stderr in debug mode (requires <stdio.h>)
 * - Platform-independent (no OS-specific features)
 *
 * Thread-safety: Inherits thread-safety from underlying log.h functions.
 * Macros themselves are safe (no shared state). Multiple
 * threads can log simultaneously if log.h backend is thread-safe.
 *
 * Performance:
 * - Time complexity: O(n) where n = message length or formatted output size
 * - Space complexity: O(1) — zero heap allocations, stack-only
 * - No dynamic allocation overhead
 * - Checked macros: zero overhead in release builds (NDEBUG)
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
 * (Three-tier logging approach / Macro variants explained /
 *  Fire-and-forget vs checked / NDEBUG behavior /
 *  Common usage patterns / Typical use cases /
 *  Performance considerations / Best practices /
 *  Integration with log.h / Error handling philosophy /
 *  Usage examples sections remain unchanged from original)
 */

/* ────────────────────────────────────────────────────────────────────────────
   Simple message macros (most efficient for literal strings)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Logs a simple static/literal message at INFO level
 *
 * Most efficient logging option for constant strings. Directly writes the
 * message without any formatting overhead.
 *
 * Fire-and-forget: logging failures are silent.
 *
 * @param msg Message string (literal or const char* variable, no formatting)
 */
#define LOG_INFO_MSG(msg) (void)log_msg(LOG_INFO, (msg))

/**
 * @brief Logs a simple static/literal message at WARN level
 */
#define LOG_WARN_MSG(msg) (void)log_msg(LOG_WARN, (msg))

/**
 * @brief Logs a simple static/literal message at ERROR level
 */
#define LOG_ERROR_MSG(msg) (void)log_msg(LOG_ERROR, (msg))

/* ────────────────────────────────────────────────────────────────────────────
   Formatted logging macros (most commonly used)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging at INFO level
 *
 * Printf-style formatted logging for informational messages with dynamic data.
 * Fire-and-forget: logging failures are silent.
 *
 * @param ... printf-style format string followed by arguments
 */
#define LOG_INFO_FMT(...) (void)log_fmt(LOG_INFO, __VA_ARGS__)

/**
 * @brief Formatted logging at WARN level
 */
#define LOG_WARN_FMT(...) (void)log_fmt(LOG_WARN, __VA_ARGS__)

/**
 * @brief Formatted logging at ERROR level
 */
#define LOG_ERROR_FMT(...) (void)log_fmt(LOG_ERROR, __VA_ARGS__)

/* ────────────────────────────────────────────────────────────────────────────
   Checked variants — safety net during development
   ──────────────────────────────────────────────────────────────────────────── */

#ifdef NDEBUG
    #define LOG_INFO_CHECKED(...)   LOG_INFO_FMT(__VA_ARGS__)
    #define LOG_WARN_CHECKED(...)   LOG_WARN_FMT(__VA_ARGS__)
    #define LOG_ERROR_CHECKED(...)  LOG_ERROR_FMT(__VA_ARGS__)
#else
    #define LOG_INFO_CHECKED(...) \
        do { \
            result_bool_constcharp _r = log_fmt(LOG_INFO, __VA_ARGS__); \
            if (result_bool_constcharp_is_err(_r)) { \
                fprintf(stderr, "[LOG-FAIL] Could not write info message: %s\n", \
                        result_bool_constcharp_unwrap_err(_r)); \
            } \
        } while (0)

    #define LOG_WARN_CHECKED(...) \
        do { \
            result_bool_constcharp _r = log_fmt(LOG_WARN, __VA_ARGS__); \
            if (result_bool_constcharp_is_err(_r)) { \
                fprintf(stderr, "[LOG-FAIL] Could not write warning message: %s\n", \
                        result_bool_constcharp_unwrap_err(_r)); \
            } \
        } while (0)

    #define LOG_ERROR_CHECKED(...) \
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
   ────────────────────────────────────────────────────────────────────────── */

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
            sleep(attempt); // Exponential backoff
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

#endif /* CANON_UTIL_LOG_MACROS_H */
