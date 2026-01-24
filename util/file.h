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
 * @brief Safe, explicit file I/O with Result/Option error handling and arena support
 *
 * High-level file utilities that emphasize:
 * - observable failures via Result<T,Error> and Option<T>
 * - deterministic memory usage via arena allocation (preferred path)
 * - automatic resource cleanup via SCOPE_DEFER
 * - binary mode by default for cross-platform consistency
 * - no silent failures or hidden errno usage
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit error propagation — never assume success
 * - Arena-first philosophy — zero-heap path for temporary data
 * - Heap fallback for persistent / long-lived content
 * - Binary mode ("rb"/"wb") — no CR/LF surprises, accurate sizes
 * - RAII-style FILE* cleanup via SCOPE_DEFER
 * - Null-terminated output buffers (safe for string functions)
 * - All-or-nothing semantics (partial read/write = error)
 * - Clear ownership transfer rules documented per function
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(n) where n = file size or content length
 * - Space: O(n) for content + O(1) control structures
 * - Arena path: zero heap allocations
 * - Heap path: exactly one malloc + one temporary arena
 * - I/O: single sequential read/write pass + seeks
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Reentrant and thread-safe when using different files/arenas
 * - FILE* protected by SCOPE_DEFER (no leak on concurrent errors)
 * - Arena thread-safety follows arena.h rules (usually per-thread arenas)
 * - Concurrent writes to same path → undefined (use locking)
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - C99 or later
 * - POSIX-compatible FILE* semantics assumed
 * - Binary mode ensures consistent behavior (Windows/Unix)
 * - Relies on: fopen/fread/fwrite/fseek/ftell/fclose
 *
 * Allocation strategies — choose wisely:
 * ────────────────────────────────────────────────────────────────────────────
 * | Strategy       | Use when                              | Lifetime                  | Cleanup responsibility     | Typical functions          |
 * |----------------|---------------------------------------|---------------------------|-----------------------------|----------------------------|
 * | Arena          | Temporary data, configs, shaders      | Until arena reset         | Automatic (arena)           | file_read_all_arena()      |
 * | Heap           | Persistent strings, returned values   | Until caller frees        | Caller (str_free / free)    | file_read_all()            |
 *
 * Binary mode rationale:
 * - Avoids Windows text-mode CR/LF translation
 * - ftell() returns correct byte count
 * - Works for both text and binary files
 * - If line-ending conversion needed → do it explicitly after reading
 *
 * Error handling:
 * - Option<T> for may-fail-without-details (read → Some/None)
 * - Result<T,Error> for operations needing error reason (write → Ok/Err)
 * - Never use value without checking — undefined behavior otherwise
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Loading configuration (JSON, INI, TOML, YAML)
 * - Reading shader / script source code
 * - Game asset / level data loading
 * - Simple logging and debug output
 * - Template / email / report generation
 * - Build-system file operations
 * - Test fixture data
 * - Small-to-medium file caching
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Very large files (> few MB) — use streaming
 * - Random-access or memory-mapped I/O
 * - Complex binary formats (use dedicated parsers)
 * - Real-time / incremental writing
 * - Network protocols (use sockets)
 *
 * @sa file_read_all_arena(), file_read_all(), file_write_all()
 */

/**
 * @brief Result type for write operations (bytes written or error)
 */
CANON_C_DEFINE_RESULT(size_t, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Reading — arena-backed (preferred, zero-heap)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into arena-allocated, null-terminated buffer
 *
 * Preferred method for temporary file contents (configs, shaders, templates).
 * Deterministic memory usage, no heap allocations, automatic cleanup on arena reset.
 *
 * @param path  Null-terminated file path
 * @param arena Valid initialized arena with sufficient space
 *
 * @return `Some(arena-owned char*)` on success — buffer contains file content + '\0'
 *         `None` on any failure (file missing, I/O error, out of arena space, …)
 *
 * @pre  path != NULL, arena != NULL and initialized
 * @post If Some: buffer valid until arena reset/destroy
 * @post File always closed (SCOPE_DEFER)
 *
 * @remark Returned pointer MUST NOT be freed
 * @remark Binary mode ("rb") — no line-ending translation
 *
 * Failure conditions (None):
 * - NULL path/arena
 * - fopen failed (ENOENT, EACCES, EISDIR, …)
 * - fseek/ftell failed (pipe, unseekable device)
 * - Negative file size
 * - Insufficient arena space
 * - fread read fewer bytes than expected
 *
 * @sa file_read_all() for heap variant
 */
static inline option_charp file_read_all_arena(const char* path, Arena* arena) {
    if (!path || !arena) return option_charp_none();

    FILE* f = fopen(path, "rb");
    if (!f) return option_charp_none();
    SCOPE_DEFER { fclose(f); };

    if (fseek(f, 0, SEEK_END) != 0) return option_charp_none();
    long len = ftell(f);
    if (len < 0) return option_charp_none();
    if (fseek(f, 0, SEEK_SET) != 0) return option_charp_none();

    size_t size = (size_t)len + 1;
    char* buf = arena_alloc(arena, size);
    if (!buf) return option_charp_none();

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        return option_charp_none();
    }
    buf[len] = '\0';

    return option_charp_some(buf);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading — heap-backed (persistent data)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into heap-allocated, null-terminated buffer
 *
 * Use when content must outlive any temporary arena or when no arena is available.
 * Caller is **fully responsible** for freeing the returned buffer.
 *
 * @param path Null-terminated file path
 *
 * @return `Some(heap-owned char*)` on success
 *         `None` on failure
 *
 * @remark Internally uses 4 KiB stack scratch arena + one heap copy
 * @remark For large files prefer `file_read_all_arena()` with dedicated arena
 * @remark Must call `str_free()` or `free()` when done — or use SCOPE_DEFER
 *
 * @sa file_read_all_arena()
 */
static inline option_charp file_read_all(const char* path) {
    if (!path) return option_charp_none();

    uint8_t temp[4096];
    Arena scratch;
    arena_init(&scratch, temp, sizeof(temp));
    SCOPE_DEFER { arena_reset(&scratch); };

    option_charp tmp = file_read_all_arena(path, &scratch);
    if (option_charp_is_none(tmp)) return option_charp_none();

    return str_alloc_copy(option_charp_unwrap(tmp));
}

/* ────────────────────────────────────────────────────────────────────────────
   Writing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Writes entire content to file (overwrites if exists)
 *
 * All-or-nothing write: either all bytes are written or error is returned.
 * Uses binary mode ("wb") for consistency.
 *
 * @param path    Destination path
 * @param content Null-terminated content to write
 *
 * @return `Ok(bytes_written)` on success (equals strlen(content))
 *         `Err(error_code)` on failure
 *
 * @pre path != NULL, content != NULL
 *
 * @remark File created/truncated on open
 * @remark Not atomic — partial write possible on error
 * @remark For atomic write → write to temp file + rename
 *
 * Common errors:
 * - ERR_INVALID_ARG   (NULL arguments)
 * - ERR_IO_FAILED     (fopen failed, disk full, permission, partial fwrite)
 *
 * @sa atomic write pattern in examples
 */
static inline result_size_t_Error file_write_all(const char* path, const char* content) {
    if (!path || !content) {
        return RESULT_ERR(size_t, ERR_INVALID_ARG);
    }

    FILE* f = fopen(path, "wb");
    if (!f) return RESULT_ERR(size_t, ERR_IO_FAILED);
    SCOPE_DEFER { fclose(f); };

    size_t len = strlen(content);
    if (fwrite(content, 1, len, f) != len) {
        return RESULT_ERR(size_t, ERR_IO_FAILED);
    }

    return RESULT_OK(size_t, len);
}

/* ────────────────────────────────────────────────────────────────────────────
   Usage Examples (documentation only)
   ────────────────────────────────────────────────────────────────────────────

#include "util/file.h"
#include "core/arena.h"
#include "core/scope.h"

// Temporary config load (arena, zero heap)
void load_config_temp(void) {
    uint8_t buf[8192];
    Arena a; arena_init(&a, buf, sizeof(buf));

    option_charp cfg = file_read_all_arena("config.ini", &a);
    if (option_charp_is_some(cfg)) {
        parse_config(option_charp_unwrap(cfg));
    }

    // auto cleanup on arena_reset() or scope exit
}

// Persistent data (heap, caller frees)
void load_template(void) {
    option_charp html = file_read_all("welcome.html");
    if (option_charp_is_some(html)) {
        char* t = option_charp_unwrap(html);
        SCOPE_DEFER { str_free(t); };
        render_page(t);
    }
}

// Simple logging
void log_event(const char* msg) {
    result_size_t_Error r = file_write_all("app.log", msg);
    if (result_is_err(r)) {
        // handle error
    }
}

// Atomic write helper (temp + rename)
bool atomic_write(const char* path, const char* content) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    result_size_t_Error r = file_write_all(tmp, content);
    if (result_is_err(r)) return false;

    if (rename(tmp, path) != 0) {
        remove(tmp);
        return false;
    }
    return true;
}

──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_FILE_H */
