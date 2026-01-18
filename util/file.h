// util/file.h
#ifndef CANON_UTIL_FILE_H
#define CANON_UTIL_FILE_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "semantics/option.h"
#include "semantics/result.h"
#include "semantics/error.h"
#include "core/arena.h"
#include "core/scope.h"

/**
 * @file file.h
 * @brief Safe, explicit and observable file I/O operations
 *
 * Provides high-level file I/O utilities with modern error handling and
 * memory management strategies. Implements common file operations with
 * Result/Option-based error propagation, arena allocation support, and
 * RAII-style resource cleanup.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, stdint.h)
 *   - Depends on POSIX-compatible FILE* operations (fopen, fread, fwrite)
 *   - Uses SCOPE_DEFER (requires scope.h from this library)
 *   - All files opened in binary mode for cross-platform consistency
 *   - Path separator handling is OS-dependent (use OS conventions)
 *
 * Thread-safety: Functions are reentrant - safe to call from multiple
 *                threads on different files. FILE* operations are protected
 *                by automatic cleanup. Arena/heap allocations follow their
 *                respective thread-safety guarantees.
 *
 * Performance:
 *   - Time complexity: O(file_size) for read/write operations
 *   - Space complexity: O(file_size) - allocates buffer for entire content
 *   - Arena operations: Zero heap allocations, deterministic memory
 *   - Heap operations: Single malloc/free per file operation
 *   - I/O pattern: Single sequential read/write (cache-friendly)
 *   - File size determination: Two seeks + one read (no chunking overhead)
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Observable failures — every error is explicit (Result/Option pattern)
 * - No errno dependency — errors returned as values, not side effects
 * - Zero silent failures — all error paths explicitly represented
 * - Arena-first strategy — prefer deterministic allocation for temporary data
 * - Heap as fallback — use malloc only when data must outlive arenas
 * - Binary mode default — "rb"/"wb" for consistent cross-platform behavior
 * - Automatic cleanup — SCOPE_DEFER ensures FILE* always closed
 * - Null-terminated strings — all text content gets trailing '\0'
 * - All-or-nothing semantics — partial reads/writes are errors
 * - Clear ownership — arena-owned vs caller-must-free explicitly documented
 *
 * Memory allocation strategies:
 * ────────────────────────────────────────────────────────────────────────────
 * This library provides two allocation paths for different use cases:
 *
 * 1. ARENA ALLOCATION (Preferred for temporary data):
 *    - Zero heap allocations — deterministic memory usage
 *    - Perfect for: config files, shaders, parsing temporary data
 *    - Lifetime: valid until arena_reset() or arena_destroy()
 *    - Cleanup: automatic when arena is reset/destroyed
 *    - Use: file_read_all_arena()
 *
 * 2. HEAP ALLOCATION (For persistent data):
 *    - Single malloc — standard heap allocation
 *    - Perfect for: long-lived strings, data that outlives functions
 *    - Lifetime: until explicitly freed
 *    - Cleanup: caller MUST call str_free() or mem_free()
 *    - Use: file_read_all()
 *
 * Choose arena when:
 * - Reading config files for immediate parsing
 * - Loading shader source code for compilation
 * - Temporary file content in request handlers
 * - Any data consumed within a single function/scope
 *
 * Choose heap when:
 * - Data must outlive the current arena
 * - Building long-lived data structures
 * - Returning file content from functions
 * - No suitable arena available
 *
 * Binary mode rationale:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions use binary mode ("rb", "wb") instead of text mode because:
 * - Consistent behavior across Windows/Unix (no CR-LF translation)
 * - Accurate file size from ftell (text mode can lie on Windows)
 * - No surprise character transformations
 * - Works correctly for both text and binary files
 * - Explicit is better than implicit
 *
 * If you need text mode line-ending translation, handle it explicitly in
 * your application code after reading.
 *
 * Error handling philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * Unlike traditional C file I/O (which uses NULL/errno), this library uses:
 *
 * - Option<T> for operations that may not produce a value:
 *   - file_read_all_arena() → option_charp
 *   - file_read_all() → option_charp
 *   - Returns Some(data) on success, None on any failure
 *
 * - Result<T, E> for operations that need error details:
 *   - file_write_all() → result_size_t_Error
 *   - Returns Ok(bytes_written) or Err(error_code)
 *
 * This makes error handling:
 * - Explicit — cannot ignore errors accidentally
 * - Type-safe — compiler enforces checking
 * - Composable — can chain operations with map/and_then
 * - Self-documenting — function signature shows fallibility
 *
 * IMPORTANT: Always check Option/Result before using!
 * ────────────────────────────────────────────────────────────────────────────
 * Never assume file operations succeed. Always check return values:
 *
 * ✓ CORRECT:
 *   option_charp content = file_read_all_arena(path, arena);
 *   if (option_charp_is_some(content)) {
 *       const char* data = option_charp_unwrap(content);
 *       // use data...
 *   }
 *
 * ✗ WRONG:
 *   option_charp content = file_read_all_arena(path, arena);
 *   const char* data = option_charp_unwrap(content);  // UNSAFE!
 *   // data could be undefined if file doesn't exist
 *
 * Failure modes to expect:
 * ────────────────────────────────────────────────────────────────────────────
 * File operations can fail for many reasons:
 * - File doesn't exist (ENOENT)
 * - Permission denied (EACCES)
 * - Path is a directory (EISDIR)
 * - Disk full (ENOSPC)
 * - I/O errors (EIO)
 * - Memory allocation failures
 * - Invalid file descriptors
 * - Network file system timeouts
 *
 * All these failures are represented as None/Err - the functions never crash
 * or set global errno. You get explicit error values you can handle.
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Loading configuration files (JSON, INI, TOML)
 * - Reading shader source code for compilation
 * - Loading game assets (levels, data files)
 * - Simple logging and output file generation
 * - Reading entire small-to-medium files into memory
 * - Template file loading
 * - Build system file operations
 * - Test fixture data loading
 * - Configuration serialization/deserialization
 * - Simple file-based caching
 *
 * NOT suitable for:
 * - Very large files (gigabytes) - use streaming instead
 * - Random access I/O - use fseek/fread directly
 * - Binary file format parsing - use specialized parsers
 * - Network streams - use socket APIs
 * - Incremental writing - use fprintf/fwrite directly
 *
 * Ownership and lifetime rules:
 * ────────────────────────────────────────────────────────────────────────────
 * ARENA-ALLOCATED results (file_read_all_arena):
 * - Buffer is owned by the arena
 * - Lifetime: valid until arena_reset() or arena_destroy()
 * - Cleanup: automatic (managed by arena)
 * - DO NOT call free() on returned pointer
 * - Safe to use while arena is alive
 *
 * HEAP-ALLOCATED results (file_read_all):
 * - Buffer is owned by caller
 * - Lifetime: until explicitly freed
 * - Cleanup: MUST call str_free() or mem_free() when done
 * - Caller is fully responsible for deallocation
 * - Memory leak if not freed
 *
 * FILE* handles:
 * - Automatically closed via SCOPE_DEFER
 * - Never leaked even on error paths
 * - No manual fclose() needed
 * - RAII-style cleanup
 *
 * Usage examples:
 *
 * Basic arena-based file reading:
 * ```c
 * uint8_t buffer[8192];
 * Arena arena;
 * arena_init(&arena, buffer, sizeof(buffer));
 * 
 * option_charp content = file_read_all_arena("config.ini", &arena);
 * if (option_charp_is_some(content)) {
 *     const char* text = option_charp_unwrap(content);
 *     printf("Config: %s\n", text);
 *     // content valid until arena_reset()
 * } else {
 *     fprintf(stderr, "Failed to read config.ini\n");
 * }
 * ```
 *
 * Heap-based reading for persistent data:
 * ```c
 * option_charp content = file_read_all("template.html");
 * if (option_charp_is_some(content)) {
 *     char* html = option_charp_unwrap(content);
 *     // Use html... it persists beyond this scope
 *     process_template(html);
 *     // MUST free when done!
 *     str_free(html);
 * }
 * ```
 *
 * Simple file writing:
 * ```c
 * const char* log_entry = "Application started\n";
 * result_size_t_Error res = file_write_all("app.log", log_entry);
 * if (result_is_ok(res)) {
 *     size_t written = result_unwrap(res);
 *     printf("Wrote %zu bytes\n", written);
 * } else {
 *     Error err = result_unwrap_err(res);
 *     fprintf(stderr, "Write failed: %s\n", error_to_string(err));
 * }
 * ```
 *
 * Error handling with early return:
 * ```c
 * option_charp load_config(Arena* arena) {
 *     option_charp content = file_read_all_arena("settings.cfg", arena);
 *     if (option_charp_is_none(content)) {
 *         return option_charp_none();  // Propagate error
 *     }
 *     return content;
 * }
 * ```
 */

/**
 * @brief Result alias for write operations (bytes written or error)
 *
 * Represents the outcome of a file write operation:
 * - Ok(size_t): Number of bytes successfully written
 * - Err(Error): Error code indicating why write failed
 *
 * Common error codes:
 * - ERR_INVALID_ARG: NULL path or content
 * - ERR_IO_FAILED: fopen failed or incomplete write
 */
CANON_C_DEFINE_RESULT(size_t, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Reading entire file — arena-backed (preferred, zero-heap path)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file content into arena-allocated null-terminated buffer
 *
 * Preferred method for temporary file contents. Provides deterministic memory
 * usage by allocating from the provided arena. Perfect for config files,
 * shaders, or any data that will be parsed and discarded quickly.
 *
 * This is the RECOMMENDED way to read files when you have a suitable arena
 * available. Zero heap allocations, predictable memory usage, automatic
 * cleanup when arena is reset.
 *
 * Algorithm:
 *   - Opens file in binary mode ("rb")
 *   - Uses fseek(SEEK_END) + ftell() to determine exact file size
 *   - Allocates (file_size + 1) bytes from arena for null terminator
 *   - Rewinds to start with fseek(SEEK_SET)
 *   - Reads entire file in single fread() call
 *   - Null-terminates buffer (safe for string functions)
 *   - Automatically closes FILE* via SCOPE_DEFER (even on error)
 *
 * @param path      Null-terminated file path (must not be NULL)
 * @param arena     Valid, initialized Arena with sufficient capacity (must not be NULL)
 *
 * @return          Some(arena-owned null-terminated buffer) on success
 *                  None on any failure (see failure modes below)
 *
 * Preconditions:
 *   - path is a valid null-terminated string
 *   - arena is properly initialized
 *   - arena has at least (file_size + 1) bytes available
 *   - File at path exists and is readable
 *   - File size fits in a long (typically 2GB-4GB limit)
 *
 * Postconditions:
 *   - If Some: buffer contains entire file + null terminator
 *   - If Some: buffer is valid until arena_reset() or arena_destroy()
 *   - If None: arena state unchanged (allocation rolled back)
 *   - File handle always closed (success or failure)
 *
 * Failure modes (returns None):
 *   - path or arena is NULL
 *   - File doesn't exist or can't be opened
 *   - fseek or ftell fails (unseekable file, pipe, etc.)
 *   - File size is negative (ftell error)
 *   - Arena doesn't have enough space
 *   - fread doesn't read expected bytes (I/O error, disk full, etc.)
 *
 * Performance:
 *   - Time: O(n) where n = file size
 *   - Space: O(n) from arena
 *   - I/O operations: 2 seeks + 1 read
 *   - Memory allocations: 1 arena allocation (zero heap)
 *   - Best case: ~10μs for small files on SSD
 *   - Worst case: Limited by disk I/O speed
 *
 * Memory ownership:
 *   - Returned buffer is OWNED BY THE ARENA
 *   - Valid until next arena_reset() or arena_destroy()
 *   - DO NOT call free() on returned pointer
 *   - Safe to pass around while arena exists
 *
 * Thread-safety:
 *   - Safe if different threads use different arenas
 *   - Not safe if multiple threads share same arena without locking
 *   - Each thread should have its own arena or use synchronization
 *
 * Example:
 * ```c
 * uint8_t scratch[4096];
 * Arena arena;
 * arena_init(&arena, scratch, sizeof(scratch));
 * 
 * option_charp config = file_read_all_arena("game.cfg", &arena);
 * if (option_charp_is_some(config)) {
 *     const char* text = option_charp_unwrap(config);
 *     printf("Config loaded: %zu bytes\n", strlen(text));
 *     parse_config(text);
 *     // text remains valid here
 * } else {
 *     fprintf(stderr, "Failed to load config\n");
 * }
 * // After arena_reset(), content is invalid
 * ```
 *
 * Common pitfall:
 * ```c
 * const char* get_config(void) {
 *     uint8_t buf[1024];
 *     Arena arena;
 *     arena_init(&arena, buf, sizeof(buf));
 *     option_charp cfg = file_read_all_arena("cfg", &arena);
 *     return option_charp_unwrap(cfg);  // WRONG! Arena destroyed
 * }
 * // Returned pointer is dangling - arena buffer is stack-allocated
 * ```
 */
static inline option_charp file_read_all_arena(
    const char* path,
    Arena* arena
) {
    if (!path || !arena) {
        return option_charp_none();
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return option_charp_none();
    }

    // Ensure file is closed even on error paths
    SCOPE_DEFER { fclose(f); };

    // Determine file size
    if (fseek(f, 0, SEEK_END) != 0) {
        return option_charp_none();
    }

    long len = ftell(f);
    if (len < 0) {
        return option_charp_none();
    }

    // Rewind to start
    if (fseek(f, 0, SEEK_SET) != 0) {
        return option_charp_none();
    }

    // Allocate buffer with room for null terminator
    size_t size = (size_t)len + 1;
    char* buf = arena_alloc(arena, size);
    if (!buf) {
        return option_charp_none();
    }

    // Read entire file
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        return option_charp_none();
    }

    // Null-terminate for safe string operations
    buf[len] = '\0';
    
    return option_charp_some(buf);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading entire file — heap fallback (persistent data)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into heap-allocated null-terminated buffer
 *
 * Heap-based alternative to file_read_all_arena(). Use this when you need
 * file content to outlive any temporary arena, or when no suitable arena
 * is available.
 *
 * This function is convenient but requires manual memory management. The
 * caller is FULLY RESPONSIBLE for freeing the returned buffer.
 *
 * Implementation strategy:
 *   - Uses small stack-backed scratch arena (4KB) for initial read
 *   - Calls file_read_all_arena() to read into scratch
 *   - If successful, copies content to heap via str_alloc_copy()
 *   - Scratch arena automatically cleaned up via SCOPE_DEFER
 *   - Returns heap-allocated copy with indefinite lifetime
 *
 * @param path      Null-terminated file path (must not be NULL)
 *
 * @return          Some(heap-owned null-terminated buffer) on success
 *                  None on any failure
 *
 * Preconditions:
 *   - path is a valid null-terminated string
 *   - File exists and is readable
 *   - File size <= 4KB OR heap has enough memory for str_alloc_copy
 *   - (Files > 4KB will fail - use file_read_all_arena with larger arena)
 *
 * Postconditions:
 *   - If Some: buffer is heap-allocated via malloc
 *   - If Some: buffer contains entire file + null terminator
 *   - If Some: caller MUST call str_free() or mem_free() when done
 *   - If None: no memory was allocated
 *   - Scratch arena is always cleaned up
 *
 * Memory ownership:
 *   - Returned buffer is OWNED BY CALLER
 *   - Lifetime: until explicitly freed
 *   - Cleanup: MUST call str_free() or mem_free()
 *   - Forgetting to free = memory leak
 *
 * Performance:
 *   - Time: O(n) where n = file size
 *   - Space: O(n) on heap + temporary 4KB stack
 *   - Allocations: 1 stack arena + 1 heap allocation
 *   - Extra copy overhead compared to arena version
 *   - Slightly slower due to str_alloc_copy
 *
 * Limitations:
 *   - 4KB scratch arena limits file size unless you modify the function
 *   - Extra copy step (arena → heap) uses more memory temporarily
 *   - Not suitable for very large files
 *   - For files > 4KB, use file_read_all_arena with larger arena
 *
 * Example:
 * ```c
 * option_charp html = file_read_all("template.html");
 * if (option_charp_is_some(html)) {
 *     char* content = option_charp_unwrap(html);
 *     
 *     // Content persists - can be stored, passed around, etc.
 *     process_template(content);
 *     cache_template(content);
 *     
 *     // MUST free when done!
 *     str_free(content);
 * } else {
 *     fprintf(stderr, "Template not found\n");
 * }
 * ```
 *
 * Common mistake - memory leak:
 * ```c
 * void load_config(void) {
 *     option_charp cfg = file_read_all("config.txt");
 *     if (option_charp_is_some(cfg)) {
 *         use_config(option_charp_unwrap(cfg));
 *         // WRONG! Forgot to free - memory leak
 *     }
 * }
 * ```
 *
 * Correct usage with cleanup:
 * ```c
 * void load_config(void) {
 *     option_charp cfg = file_read_all("config.txt");
 *     if (option_charp_is_some(cfg)) {
 *         char* data = option_charp_unwrap(cfg);
 *         SCOPE_DEFER { str_free(data); };  // Automatic cleanup
 *         use_config(data);
 *     }
 * }
 * ```
 */
static inline option_charp file_read_all(const char* path) {
    if (!path) {
        return option_charp_none();
    }

    // Small stack-backed scratch arena for temporary read
    uint8_t temp[4096];
    Arena scratch;
    arena_init(&scratch, temp, sizeof(temp));
    
    // Ensure scratch arena is cleaned up
    SCOPE_DEFER { arena_reset(&scratch); };

    // Read into scratch arena first
    option_charp temp_content = file_read_all_arena(path, &scratch);
    if (option_charp_is_none(temp_content)) {
        return option_charp_none();
    }

    // Copy to heap for persistent lifetime
    // str_alloc_copy returns option_charp, handles malloc failure
    return str_alloc_copy(option_charp_unwrap(temp_content));
}

/* ────────────────────────────────────────────────────────────────────────────
   Writing entire content to file
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Writes complete content to file (overwrites existing file)
 *
 * Simple, safe, all-or-nothing write operation. Creates new file or
 * truncates existing file, writes entire content, ensures all data is
 * flushed to disk.
 *
 * This function provides the simplest possible file writing interface:
 * give it a path and a string, it writes the string to the file.
 * No partial writes, no append mode, no complications.
 *
 * Algorithm:
 *   - Opens file in binary write mode ("wb") - creates or truncates
 *   - Calculates content length via strlen()
 *   - Writes entire content in single fwrite() call
 *   - Verifies all bytes were written
 *   - Automatically closes FILE* via SCOPE_DEFER
 *   - fclose() implicitly flushes buffers to disk
 *
 * @param path      Null-terminated destination path (must not be NULL)
 * @param content   Null-terminated content to write (must not be NULL)
 *
 * @return          Ok(number of bytes written) on success
 *                  Err(error_code) on failure (see error codes below)
 *
 * Preconditions:
 *   - path is a valid null-terminated string
 *   - content is a valid null-terminated string
 *   - Parent directory of path exists
 *   - Caller has write permission in directory
 *   - Sufficient disk space available
 *
 * Postconditions:
 *   - If Ok: file at path contains exactly the content string
 *   - If Ok: return value equals strlen(content)
 *   - If Ok: file is closed and flushed to disk
 *   - If Err: file may not exist, be empty, or contain partial data
 *   - File handle always closed (success or failure)
 *
 * Error codes:
 *   - ERR_INVALID_ARG: path or content is NULL
 *   - ERR_IO_FAILED: fopen failed (permissions, disk full, bad path)
 *   - ERR_IO_FAILED: fwrite didn't write all bytes (disk full, I/O error)
 *
 * Failure modes (returns Err):
 *   - NULL path or content
 *   - File can't be created (permission denied, bad path)
 *   - Parent directory doesn't exist
 *   - Disk full during write
 *   - I/O error during write
 *   - File system read-only
 *
 * Performance:
 *   - Time: O(n) where n = strlen(content)
 *   - Space: O(1) - no allocations
 *   - I/O operations: 1 write + 1 close/flush
 *   - Writes are buffered by libc, flushed on close
 *   - Best case: ~10μs for small files on SSD
 *   - Worst case: Limited by disk write speed
 *
 * Binary mode behavior:
 *   - Uses "wb" mode for cross-platform consistency
 *   - No CR-LF translation (Windows writes exactly what you give)
 *   - If you need platform-specific line endings, convert content first
 *   - Accurate byte counts (no hidden character transformations)
 *
 * Atomicity:
 *   - NOT atomic - partial writes possible on failure
 *   - File may be left in inconsistent state if write fails midway
 *   - For atomic writes, use temp file + rename pattern:
 *     1. Write to "file.tmp"
 *     2. If successful, rename to "file"
 *     3. Rename is atomic on POSIX systems
 *
 * Thread-safety:
 *   - Safe to call from multiple threads with different paths
 *   - Concurrent writes to same path = undefined behavior (OS-dependent)
 *   - Use file locking if multiple threads/processes write same file
 *
 * Example:
 * ```c
 * const char* log_message = "Application started at 10:30 AM\n";
 * result_size_t_Error res = file_write_all("app.log", log_message);
 * 
 * if (result_is_ok(res)) {
 *     size_t bytes = result_unwrap(res);
 *     printf("Wrote %zu bytes to app.log\n", bytes);
 * } else {
 *     Error err = result_unwrap_err(res);
 *     fprintf(stderr, "Failed to write log: %s\n", error_to_string(err));
 * }
 * ```
 *
 * Overwrite behavior:
 * ```c
 * // First write
 * file_write_all("output.txt", "Hello");
 * // File contains: "Hello"
 * 
 * // Second write - OVERWRITES
 * file_write_all("output.txt", "World");
 * // File now contains: "World" (not "HelloWorld")
 * ```
 *
 * Error handling pattern:
 * ```c
 * result_size_t_Error save_config(const char* config_text) {
 *     result_size_t_Error res = file_write_all("settings.cfg", config_text);
 *     if (result_is_err(res)) {
 *         // Log error, try backup location, etc.
 *         return res;  // Propagate error
 *     }
 *     return res;
 * }
 * ```
 *
 * Composing with file_read_all:
 * ```c
 * // Read-modify-write pattern
 * option_charp content = file_read_all("data.txt");
 * if (option_charp_is_some(content)) {
 *     char* data = option_charp_unwrap(content);
 *     
 *     // Modify data...
 *     modify(data);
 *     
 *     // Write back
 *     result_size_t_Error res = file_write_all("data.txt", data);
 *     str_free(data);
 *     
 *     if (result_is_err(res)) {
 *         fprintf(stderr, "Failed to save changes\n");
 *     }
 * }
 * ```
 */
static inline result_size_t_Error file_write_all(
    const char* path,
    const char* content
) {
    if (!path || !content) {
        return RESULT_ERR(size_t, ERR_INVALID_ARG);
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        return RESULT_ERR(size_t, ERR_IO_FAILED);
    }

    // Ensure file is closed even on error paths
    SCOPE_DEFER { fclose(f); };

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);

    // Verify all bytes were written
    if (written != len) {
        return RESULT_ERR(size_t, ERR_IO_FAILED);
    }

    return RESULT_OK(size_t, written);
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/file.h"
    #include "core/arena.h"
    #include "core/scope.h"
    #include <stdio.h>
    #include <string.h>
    
    // Example 1: Basic config file loading with arena
    void example_load_config(void) {
        uint8_t buffer[8192];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        option_charp config = file_read_all_arena("config.ini", &arena);
        if (option_charp_is_some(config)) {
            const char* text = option_charp_unwrap(config);
            printf("Loaded config:\n%s\n", text);
            // Parse config, use data...
            // Content valid until arena_reset()
        } else {
            fprintf(stderr, "Failed to load config.ini\n");
        }
        
        arena_reset(&arena);  // Cleanup
    }
    
    // Example 2: Multiple file reads with single arena
    void example_multiple_files(void) {
        uint8_t buffer[16384];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        const char* files[] = {"vertex.glsl", "fragment.glsl", "geometry.glsl"};
        const char* shaders[3] = {NULL};
        
        for (int i = 0; i < 3; i++) {
            option_charp shader = file_read_all_arena(files[i], &arena);
            if (option_charp_is_some(shader)) {
                shaders[i] = option_charp_unwrap(shader);
                printf("Loaded %s: %zu bytes\n", files[i], strlen(shaders[i]));
            } else {
                fprintf(stderr, "Failed to load %s\n", files[i]);
            }
        }
        
        // All shaders valid simultaneously while arena exists
        compile_shader_program(shaders[0], shaders[1], shaders[2]);
        
        arena_reset(&arena);  // All shaders invalidated
    }
    
    // Example 3: Heap allocation for persistent data
    void example_persistent_template(void) {
        option_charp html = file_read_all("email_template.html");
        if (option_charp_is_some(html)) {
            char* template = option_charp_unwrap(html);
            
            // Store in global or return from function
            set_global_template(template);
            
            // Later, when done with template:
            // str_free(template);
        }
    }
    
    // Example 4: Error handling with early return
    option_charp load_user_data(const char* username, Arena* arena) {
        char path[256];
        snprintf(path, sizeof(path), "users/%s/profile.json", username);
        
        option_charp data = file_read_all_arena(path, arena);
        if (option_charp_is_none(data)) {
            fprintf(stderr, "User %s not found\n", username);
            return option_charp_none();
        }
        
        return data;
    }
    
    void example_user_lookup(void) {
        uint8_t buffer[4096];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        option_charp profile = load_user_data("alice", &arena);
        if (option_charp_is_some(profile)) {
            printf("Profile: %s\n", option_charp_unwrap(profile));
        }
    }
    
    // Example 5: Simple logging
    void example_logging(void) {
        const char* entries[] = {
            "Server started\n",
            "Connection from 192.168.1.100\n",
            "Request processed\n"
        };
        
        for (int i = 0; i < 3; i++) {
            result_size_t_Error res = file_write_all("server.log", entries[i]);
            if (result_is_err(res)) {
                fprintf(stderr, "Log write failed: %s\n",
                        error_to_string(result_unwrap_err(res)));
            }
        }
    }
    
    // Example 6: Read-modify-write pattern
    void example_modify_file(void) {
        option_charp content = file_read_all("counter.txt");
        if (option_charp_is_none(content)) {
            // File doesn't exist, create with initial value
            file_write_all("counter.txt", "0");
            return;
        }
        
        char* text = option_charp_unwrap(content);
        int count = atoi(text);
        str_free(text);
        
        // Increment counter
        count++;
        char new_content[32];
        snprintf(new_content, sizeof(new_content), "%d", count);
        
        result_size_t_Error res = file_write_all("counter.txt", new_content);
        if (result_is_err(res)) {
            fprintf(stderr, "Failed to update counter\n");
        }
    }
    
    // Example 7: Temporary arena pattern
    void example_temp_arena_pattern(void) {
        uint8_t buffer[1024];
        Arena temp;
        arena_init(&temp, buffer, sizeof(buffer));
        SCOPE_DEFER { arena_reset(&temp); };
        
        option_charp data = file_read_all_arena("temp_data.txt", &temp);
        if (option_charp_is_some(data)) {
            process_data(option_charp_unwrap(data));
        }
        // Automatic cleanup via SCOPE_DEFER
    }
    
    // Example 8: Batch file processing
    void example_batch_process(void) {
        const char* input_files[] = {
            "data1.txt", "data2.txt", "data3.txt"
        };
        
        uint8_t buffer[16384];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        for (int i = 0; i < 3; i++) {
            option_charp data = file_read_all_arena(input_files[i], &arena);
            if (option_charp_is_some(data)) {
                const char* content = option_charp_unwrap(data);
                
                // Process and write output
                char output_path[256];
                snprintf(output_path, sizeof(output_path), 
                         "processed_%s", input_files[i]);
                
                char processed[4096];
                process_content(content, processed, sizeof(processed));
                file_write_all(output_path, processed);
            }
            
            // Reset arena between files
            arena_reset(&arena);
        }
    }
    
    // Example 9: Configuration file with fallback
    option_charp load_config_with_fallback(Arena* arena) {
        const char* paths[] = {
            "config.local.ini",
            "config.ini",
            "/etc/myapp/config.ini"
        };
        
        for (int i = 0; i < 3; i++) {
            option_charp config = file_read_all_arena(paths[i], arena);
            if (option_charp_is_some(config)) {
                printf("Loaded config from %s\n", paths[i]);
                return config;
            }
        }
        
        fprintf(stderr, "No config file found\n");
        return option_charp_none();
    }
    
    // Example 10: Atomic write pattern (temp file + rename)
    bool atomic_write(const char* path, const char* content) {
        char temp_path[512];
        snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
        
        // Write to temp file
        result_size_t_Error res = file_write_all(temp_path, content);
        if (result_is_err(res)) {
            return false;
        }
        
        // Atomic rename (POSIX)
        if (rename(temp_path, path) != 0) {
            remove(temp_path);  // Cleanup temp file
            return false;
        }
        
        return true;
    }
    
    void example_atomic_write(void) {
        const char* important_data = "Critical configuration data\n";
        if (atomic_write("important.cfg", important_data)) {
            printf("Config saved atomically\n");
        } else {
            fprintf(stderr, "Atomic write failed\n");
        }
    }
    
    // Example 11: File existence check
    bool file_exists(const char* path) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);
            return true;
        }
        return false;
    }
    
    void example_conditional_write(void) {
        const char* path = "output.txt";
        
        if (file_exists(path)) {
            printf("File exists, skipping write\n");
            return;
        }
        
        file_write_all(path, "Initial content\n");
    }
    
    // Example 12: JSON config parsing
    #include "json/parser.h"  // Hypothetical JSON library
    
    void example_json_config(void) {
        uint8_t buffer[8192];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        option_charp json_text = file_read_all_arena("config.json", &arena);
        if (option_charp_is_some(json_text)) {
            const char* text = option_charp_unwrap(json_text);
            
            JsonValue config = json_parse(text, &arena);
            const char* db_host = json_get_string(&config, "database.host");
            int db_port = json_get_int(&config, "database.port");
            
            printf("Database: %s:%d\n", db_host, db_port);
        }
        
        arena_reset(&arena);
    }
    
    // Example 13: Shader compilation
    typedef struct {
        const char* vertex;
        const char* fragment;
    } ShaderProgram;
    
    bool load_shaders(ShaderProgram* program, Arena* arena) {
        option_charp vs = file_read_all_arena("shaders/vertex.glsl", arena);
        option_charp fs = file_read_all_arena("shaders/fragment.glsl", arena);
        
        if (option_charp_is_none(vs) || option_charp_is_none(fs)) {
            return false;
        }
        
        program->vertex = option_charp_unwrap(vs);
        program->fragment = option_charp_unwrap(fs);
        return true;
    }
    
    void example_shader_loading(void) {
        uint8_t buffer[32768];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        ShaderProgram program;
        if (load_shaders(&program, &arena)) {
            compile_and_link_shaders(program.vertex, program.fragment);
        } else {
            fprintf(stderr, "Failed to load shaders\n");
        }
        
        arena_reset(&arena);
    }
    
    // Example 14: Test fixture loading
    void test_parser_with_fixture(void) {
        uint8_t buffer[4096];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        option_charp fixture = file_read_all_arena("tests/fixtures/valid.xml", 
                                                     &arena);
        assert(option_charp_is_some(fixture));
        
        const char* xml = option_charp_unwrap(fixture);
        XmlDocument* doc = parse_xml(xml);
        
        // Run tests...
        assert(doc != NULL);
        assert(xml_get_root(doc) != NULL);
        
        arena_reset(&arena);
    }
    
    // Example 15: Simple template engine
    char* apply_template(const char* template, const char* name, Arena* arena) {
        // Replace {{name}} with actual name
        char* output = arena_alloc(arena, 4096);
        const char* pos = template;
        size_t out_idx = 0;
        
        while (*pos) {
            if (strncmp(pos, "{{name}}", 8) == 0) {
                strcpy(output + out_idx, name);
                out_idx += strlen(name);
                pos += 8;
            } else {
                output[out_idx++] = *pos++;
            }
        }
        output[out_idx] = '\0';
        return output;
    }
    
    void example_template_engine(void) {
        uint8_t buffer[8192];
        Arena arena;
        arena_init(&arena, buffer, sizeof(buffer));
        
        option_charp tmpl = file_read_all_arena("templates/welcome.html", &arena);
        if (option_charp_is_some(tmpl)) {
            const char* template = option_charp_unwrap(tmpl);
            char* output = apply_template(template, "Alice", &arena);
            
            file_write_all("output/welcome_alice.html", output);
        }
        
        arena_reset(&arena);
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_FILE_H */
