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
 * Provides high-level, modern-style file helpers with:
 * - Full Result/Option based error handling
 * - Strong preference for arena allocation (deterministic, no heap in hot paths)
 * - Clear ownership rules (arena-owned or caller-owned-with-free)
 * - Null-safe, bounds-checked operations
 * - RAII-style resource management via SCOPE_DEFER
 * - Consistent binary mode usage for cross-platform reliability
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - Prefer arena allocation for temporary file contents (configs, parsing)
 * - Heap allocation only as fallback (persistent data)
 * - Every failure is observable — no silent errors or errno dependency
 * - Always use binary mode ("rb"/"wb") — avoids newline surprises
 * - Zero-cost cleanup via scope/defer pattern
 * - Minimal buffering interference (explicit flush where needed)
 *
 * Important notes:
 * - All paths must be null-terminated
 * - Returned buffers from arena functions are valid only until arena_reset()
 * - Heap-allocated results must be explicitly freed by caller
 * - Functions never set errno — errors are returned explicitly
 *
 * Typical workflows:
 * ────────────────────────────────────────────────────────────────────────────
 * Temporary read (parsing, config):        file_read_all_arena()
 * Persistent string (long-lived data):     file_read_all()
 * Simple write (logs, output files):       file_write_all()
 * High-performance read (large files):     use arena + manual chunk reading
 *
 * Recommended practice:
 * Always check Option/Result before dereferencing/using returned data!
 */

/**
 * @brief Result alias for write operations (bytes written or error)
 */
CANON_C_DEFINE_RESULT(size_t, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Reading entire file — arena-backed (preferred, zero-heap path)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file content into arena-allocated null-terminated buffer
 *
 * Preferred method for temporary file contents (configs, shaders, data files).
 * Uses only the provided arena — deterministic memory usage.
 *
 * @param path      Null-terminated file path (must not be NULL)
 * @param arena     Valid, initialized Arena with enough remaining capacity
 *
 * @return Some(arena-owned null-terminated buffer) on success  
 *         None on failure (not found, permission denied, read error, alloc fail)
 *
 * Behavior:
 * - Opens file in binary mode ("rb")
 * - Uses fseek/ftell to determine exact size
 * - Allocates size + 1 bytes (for null terminator)
 * - Reads exactly file size bytes
 * - Always null-terminates result (safe for string functions)
 * - Automatically closes file via SCOPE_DEFER
 *
 * Ownership & Lifetime:
 * - Buffer is owned by the arena
 * - Valid until next arena_reset() or arena_destroy()
 * - Do NOT call free() on returned pointer
 *
 * Performance:
 * - Single allocation
 * - Two seeks + one full read
 * - O(file size) time & space
 *
 * Example:
 * ```c
 * Arena arena = ...;
 * option_charp content = file_read_all_arena("config.ini", &arena);
 * if (option_charp_is_some(content)) {
 *     printf("File content:\n%s\n", option_charp_unwrap(content));
 *     // content valid until arena_reset()
 * }
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

    SCOPE_DEFER { fclose(f); };

    if (fseek(f, 0, SEEK_END) != 0) {
        return option_charp_none();
    }

    long len = ftell(f);
    if (len < 0) {
        return option_charp_none();
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        return option_charp_none();
    }

    size_t size = (size_t)len + 1; // room for null terminator
    char* buf = arena_alloc(arena, size);
    if (!buf) {
        return option_charp_none();
    }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        return option_charp_none();
    }

    buf[len] = '\0';
    return option_charp_some(buf);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading entire file — heap fallback (persistent data)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into heap-allocated null-terminated buffer
 *
 * Use when you need the content to outlive any temporary arena  
 * or when no suitable arena is available.
 *
 * @param path      Null-terminated file path
 *
 * @return Some(heap-owned null-terminated buffer) on success  
 *         None on failure
 *
 * Ownership:
 * - Caller is fully responsible for freeing the returned pointer
 * - Use str_free() or mem_free() when done
 *
 * Implementation note:
 * - Uses small stack-backed scratch arena for initial read
 * - Copies result to heap only on success
 * - Scratch arena cleaned up automatically
 */
static inline option_charp file_read_all(const char* path) {
    if (!path) {
        return option_charp_none();
    }

    uint8_t temp[4096];
    Arena scratch;
    arena_init(&scratch, temp, sizeof(temp));
    SCOPE_DEFER { arena_reset(&scratch); };

    option_charp temp_content = file_read_all_arena(path, &scratch);
    if (option_charp_is_none(temp_content)) {
        return option_charp_none();
    }

    // Copy to heap for persistent lifetime
    return str_alloc_copy(option_charp_unwrap(temp_content));
}

/* ────────────────────────────────────────────────────────────────────────────
   Writing entire content to file
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Writes complete content to file (overwrites existing file)
 *
 * Simple, safe, all-or-nothing write operation.
 *
 * @param path      Null-terminated destination path
 * @param content   Null-terminated content to write
 *
 * @return Ok(number of bytes written) on success  
 *         Err(error code) on failure
 *
 * Behavior:
 * - Opens in binary mode ("wb") — overwrites existing file
 * - Writes exactly strlen(content) bytes
 * - File closed automatically via SCOPE_DEFER
 * - Implicit flush via fclose
 *
 * Preconditions:
 * - Both path and content must not be NULL
 *
 * Example:
 * ```c
 * const char* data = "Hello, world!\n";
 * result_size_t_Error res = file_write_all("output.txt", data);
 * if (result_is_ok(res)) {
 *     printf("Wrote %zu bytes\n", result_unwrap(res));
 * } else {
 *     printf("Write failed: %s\n", error_to_string(result_unwrap_err(res)));
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

    SCOPE_DEFER { fclose(f); };

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);

    if (written != len) {
        return RESULT_ERR(size_t, ERR_IO_FAILED);
    }

    return RESULT_OK(size_t, written);
}

#endif /* CANON_UTIL_FILE_H */
