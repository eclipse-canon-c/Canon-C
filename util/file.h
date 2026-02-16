#ifndef CANON_UTIL_FILE_H
#define CANON_UTIL_FILE_H

#include "core/primitives/types.h"       // usize, bool, uint8_t
#include "core/memory.h"                 // str_len, mem_copy, str_free
#include "core/arena.h"                  // Arena, arena_alloc
#include "core/scope.h"                  // SCOPE_DEFER
#include "../../semantics/option/option.h"   // option_charp (adjust path if needed)
#include "../../semantics/result/result.h"   // result_size_t_Error
#include "../../semantics/error.h"           // ERR_* codes

/**
 * @file util/file.h
 * @brief Safe, explicit file I/O with Result/Option error handling and arena support
 *
 * High-level file utilities emphasizing:
 * - Observable failures via Result<T,Error> and Option<T>
 * - Deterministic memory usage via arena allocation (preferred)
 * - Automatic resource cleanup via SCOPE_DEFER
 * - Binary mode by default for cross-platform consistency
 * - No silent failures or hidden errno usage
 *
 * Portability note:
 * ────────────────────────────────────────────────────────────────────────────
 * file_read_all_arena / file_read_all use fseek/ftell to determine size.
 * ISO C does not guarantee this for:
 * - Very large files (> LONG_MAX bytes)
 * - Non-seekable streams (pipes, sockets)
 * - Certain exotic filesystems
 * For full portability or very large/non-seekable files, consider streaming
 * with fread + manual arena growth.
 *
 * Allocation strategies:
 * ────────────────────────────────────────────────────────────────────────────
 * | Strategy | Use when                  | Lifetime          | Cleanup responsibility | Typical functions          |
 * |----------|---------------------------|-------------------|------------------------|----------------------------|
 * | Arena    | Temporary data, configs   | Until arena reset | Automatic (arena)      | file_read_all_arena()      |
 * | Heap     | Persistent strings        | Until caller frees| Caller (str_free)      | file_read_all()            |
 *
 * Binary mode rationale:
 * - Avoids Windows CR/LF translation
 * - ftell() returns correct byte count
 * - Works for both text and binary files
 *
 * Error handling:
 * - Option<T> for may-fail-without-details
 * - Result<T,Error> for operations needing error reason
 *
 * Typical use cases:
 * - Loading configs, shaders, scripts
 * - Simple logging
 * - Small-to-medium file caching
 *
 * @sa file_read_all_arena() — preferred zero-heap path
 * @sa file_read_all() — persistent heap path
 */
/* ────────────────────────────────────────────────────────────────────────────
   Result type for write operations
   ──────────────────────────────────────────────────────────────────────────── */
CANON_C_DEFINE_RESULT(usize, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Reading — arena-backed (preferred, zero-heap)
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Reads entire file into arena-allocated, null-terminated buffer
 *
 * Preferred method — zero heap allocation, deterministic lifetime.
 *
 * @param path Null-terminated file path
 * @param arena Valid initialized arena with sufficient space
 * @return Some(arena-owned char*) on success — buffer + '\0'
 *         None on any failure
 *
 * @remark Returned pointer MUST NOT be freed
 * @remark Binary mode ("rb") — no line-ending translation
 */
static inline option_charp file_read_all_arena(const char* path, Arena* arena) {
    if (!path || !arena) return option_charp_none();

    FILE* f = fopen(path, "rb");
    if (!f) return option_charp_none();
    SCOPE_DEFER { fclose(f); };

    if (fseek(f, 0, SEEK_END) != 0) return option_charp_none();
    long len = ftell(f);
    if (len < 0) return option_charp_none();

    // Overflow check (very large files)
    if ((usize)len + 1 < (usize)len) return option_charp_none();

    if (fseek(f, 0, SEEK_SET) != 0) return option_charp_none();

    usize size = (usize)len + 1;
    char* buf = (char*)arena_alloc(arena, size);
    if (!buf) return option_charp_none();

    if (fread(buf, 1, (usize)len, f) != (usize)len) return option_charp_none();

    buf[len] = '\0';
    return option_charp_some(buf);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading — heap-backed (persistent data)
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Reads entire file into heap-allocated, null-terminated buffer
 *
 * Caller must free result with str_free() or mem_free().
 *
 * @param path Null-terminated file path
 * @return Some(heap-owned char*) on success — buffer + '\0'
 *         None on any failure
 *
 * @remark Caller MUST free returned string
 * @remark Binary mode ("rb")
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
 * @brief Writes entire content to file (binary mode)
 *
 * @param path Null-terminated file path
 * @param content Null-terminated content to write
 * @return Ok(bytes written) on success
 *         Err(error code) on failure
 */
static inline result_usize_Error file_write_all(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(usize, ERR_INVALID_ARG);

    FILE* f = fopen(path, "wb");
    if (!f) return RESULT_ERR(usize, ERR_IO_FAILED);
    SCOPE_DEFER { fclose(f); };

    usize len = str_len(content);
    if (fwrite(content, 1, len, f) != len) {
        return RESULT_ERR(usize, ERR_IO_FAILED);
    }

    return RESULT_OK(usize, len);
}

/**
 * @brief Atomic write: writes to temp file then renames
 *
 * Safer for critical files (crash-resistant).
 *
 * @param path Final file path
 * @param content Content to write
 * @return Ok(bytes written) on success
 *         Err(error code) on failure
 */
static inline result_usize_Error file_write_all_atomic(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(usize, ERR_INVALID_ARG);

    char tmp[512];
    // Build temp path: path.tmp
    usize path_len = str_len(path);
    if (path_len + 5 > sizeof(tmp)) return RESULT_ERR(usize, ERR_BUFFER_OVERFLOW);

    mem_copy(tmp, path, path_len);
    mem_copy(tmp + path_len, ".tmp", 5);

    result_usize_Error r = file_write_all(tmp, content);
    if (result_is_err(r)) {
        remove(tmp);
        return r;
    }

    if (rename(tmp, path) != 0) {
        remove(tmp);
        return RESULT_ERR(usize, ERR_IO_FAILED);
    }

    return r;
}

/* ────────────────────────────────────────────────────────────────────────────
   Helpers
   ──────────────────────────────────────────────────────────────────────────── */
/**
 * @brief Checks if file exists (simple probe)
 *
 * @param path Null-terminated file path
 * @return true if file exists and is readable, false otherwise
 */
static inline bool file_exists(const char* path) {
    if (!path) return false;

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fclose(f);
    return true;
}

#endif /* CANON_UTIL_FILE_H */
