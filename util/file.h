// util/file.h
#ifndef CANON_UTIL_FILE_H
#define CANON_UTIL_FILE_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include "semantics/option.h"
#include "semantics/result.h"
#include "semantics/error.h"
#include "core/arena.h"
#include "core/scope.h"

/**
 * @file file.h
 * @brief Safe, explicit, and observable file I/O operations
 *
 * Provides high-level helpers for common file tasks with:
 *   - Explicit error handling via Result/Option
 *   - Preference for arena-backed allocation (zero-heap when possible)
 *   - Strict ownership rules (arena-owned or heap-owned with free)
 *   - Null-safe, bounds-checked, and deterministic behavior
 *   - RAII-style cleanup via SCOPE_DEFER
 *
 * Key design principles:
 *   - Prefer arena allocation for reading (temporary, deterministic lifetime)
 *   - Heap fallback only when arena not available
 *   - All failures are observable (no silent errors)
 *   - Binary mode ("rb"/"wb") for portability
 *   - No buffering surprises (fflush where needed)
 *
 * Usage recommendations:
 *   - Use arena-backed read for temporary parsing/config loading
 *   - Use heap read only when returning persistent string
 *   - Always check Result/Option before using returned data
 *   - Free heap-allocated results with str_free() or mem_free()
 */

/**
 * @brief Result type alias for write operations (bytes written or error)
 */
CANON_C_DEFINE_RESULT(size_t, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Reading entire file — arena-backed (preferred)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into arena-allocated buffer (null-terminated)
 *
 * @param path   Null-terminated file path
 * @param arena  Valid, initialized Arena with sufficient space
 * @return       Some(arena-owned null-terminated buffer) on success,
 *               None on failure (file not found, read error, alloc fail)
 *
 * Behavior:
 *   - Opens in binary mode ("rb")
 *   - Determines file size via fseek/ftell
 *   - Allocates size + 1 for null terminator
 *   - Reads exactly file size bytes
 *   - Null-terminates result (safe for string use)
 *   - Uses SCOPE_DEFER for guaranteed fclose
 *
 * Ownership: Returned buffer is owned by arena — valid until arena_reset()
 * Lifetime: Same as arena — do not free manually
 */
static inline option_charp file_read_all_arena(
    const char* path,
    Arena* arena
) {
    if (!path || !arena) return option_charp_none();

    FILE* f = fopen(path, "rb");
    if (!f) return option_charp_none();

    SCOPE_DEFER { fclose(f); };

    if (fseek(f, 0, SEEK_END) != 0) return option_charp_none();
    long len = ftell(f);
    if (len < 0) return option_charp_none();

    if (fseek(f, 0, SEEK_SET) != 0) return option_charp_none();

    size_t size = (size_t)len + 1;  // +1 for null terminator
    char* buf = arena_alloc(arena, size);
    if (!buf) return option_charp_none();

    if (fread(buf, 1, len, f) != (size_t)len) return option_charp_none();

    buf[len] = '\0';
    return option_charp_some(buf);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading entire file — heap fallback (when arena not available)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into heap-allocated buffer (null-terminated)
 *
 * @param path Null-terminated file path
 * @return     Some(heap-owned null-terminated buffer) on success,
 *             None on failure
 *
 * Implementation:
 *   - Uses small scratch arena for initial read
 *   - Copies result to heap via str_alloc_copy
 *   - Frees scratch arena automatically
 *
 * Ownership: Caller must free returned pointer with str_free() or mem_free()
 * Use: Only when arena not available or persistent string needed
 */
static inline option_charp file_read_all(const char* path) {
    Arena scratch;
    uint8_t temp[4096];
    arena_init(&scratch, temp, sizeof(temp));

    SCOPE_DEFER { arena_reset(&scratch); };

    option_charp res = file_read_all_arena(path, &scratch);
    if (option_charp_is_none(res)) return option_charp_none();

    return str_alloc_copy(option_charp_unwrap(res));
}

/* ────────────────────────────────────────────────────────────────────────────
   Writing entire content to file
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Writes entire string content to file (binary mode)
 *
 * @param path     Null-terminated file path
 * @param content  Null-terminated content to write
 * @return         Ok(bytes written) on success,
 *                 Err(reason) on failure (invalid arg, open/write error)
 *
 * Behavior:
 *   - Opens in binary mode ("wb") — overwrites existing file
 *   - Writes exactly strlen(content) bytes
 *   - Uses SCOPE_DEFER for guaranteed fclose
 *   - Flushes automatically via fclose
 *
 * Returns number of bytes written (should match strlen(content))
 */
static inline result_size_t_Error file_write_all(
    const char* path,
    const char* content
) {
    if (!path || !content) return RESULT_ERR(size_t, ERR_INVALID_ARG);

    FILE* f = fopen(path, "wb");
    if (!f) return RESULT_ERR(size_t, ERR_IO_FAILED);

    SCOPE_DEFER { fclose(f); };

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);

    if (written != len) return RESULT_ERR(size_t, ERR_IO_FAILED);

    return RESULT_OK(size_t, written);
}

#endif /* CANON_UTIL_FILE_H */
