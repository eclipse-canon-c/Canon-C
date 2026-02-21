#ifndef CANON_UTIL_FILE_H
#define CANON_UTIL_FILE_H

#include "core/primitives/types.h"       // usize, bool, uint8_t
#include "core/memory.h"                 // mem_copy
#include "core/arena.h"                  // Arena, arena_alloc, arena_init, arena_reset
#include "core/scope.h"                  // SCOPE_DEFER
#include "util/str/string.h"             // option_charp, str_alloc_copy, str_free, str_len
#include "../../semantics/result/result.h"   // CANON_RESULT
#include "../../semantics/error.h"           // Error, ERR_*, RESULT_OK, RESULT_ERR

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
 * Reading strategy:
 * ────────────────────────────────────────────────────────────────────────────
 * file_read_all_arena() uses a two-phase approach:
 *
 * Phase 1 — seek-based (fast path):
 *   Uses fseek/ftell to determine file size upfront, allocates once,
 *   reads in a single fread call. Works for regular files on all
 *   common platforms.
 *
 * Phase 2 — streaming fallback (portability path):
 *   If fseek fails (pipes, sockets, exotic filesystems, very large files),
 *   automatically falls back to incremental fread into arena in fixed-size
 *   chunks. No heap involved — purely arena-backed.
 *
 * Allocation strategies:
 * ────────────────────────────────────────────────────────────────────────────
 * | Strategy | Use when                  | Lifetime          | Cleanup        | Functions                  |
 * |----------|---------------------------|-------------------|----------------|----------------------------|
 * | Arena    | Temporary data, configs   | Until arena reset | Automatic      | file_read_all_arena()      |
 * | Heap     | Persistent strings        | Until caller frees| Caller         | file_read_all()            |
 *
 * Binary mode rationale:
 * - Avoids Windows CR/LF translation
 * - ftell() returns correct byte count on seekable files
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
 * - Piped input or non-seekable streams (via automatic fallback)
 *
 * @sa file_read_all_arena() — preferred zero-heap path (auto-selects strategy)
 * @sa file_read_all()       — persistent heap path
 */

/* ────────────────────────────────────────────────────────────────────────────
   Result type for write operations
   ──────────────────────────────────────────────────────────────────────────── */
CANON_RESULT(usize, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Internal: streaming fread fallback for non-seekable streams
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Chunk size for streaming fread fallback
 *
 * Override by defining FILE_READ_CHUNK_SIZE before including this header.
 * Default: 4096 bytes (one typical page).
 */
#ifndef FILE_READ_CHUNK_SIZE
    #define FILE_READ_CHUNK_SIZE ((usize)4096)
#endif

/**
 * @brief Reads a file incrementally via fread into arena (streaming path)
 *
 * Used automatically by file_read_all_arena() when fseek fails.
 * Reads in FILE_READ_CHUNK_SIZE chunks, growing the arena allocation
 * incrementally. Returns null-terminated buffer on success.
 *
 * @param f     Open FILE* in binary read mode
 * @param arena Arena to allocate into
 * @return Some(char*) on success, None on arena exhaustion or read error
 *
 * @remark Internal — do not call directly. Use file_read_all_arena().
 */
static inline option_charp _file_read_stream(FILE* f, Arena* arena) {
    usize total = 0;
    char* base  = NULL;

    while (1) {
        char* chunk = (char*)arena_alloc(arena, FILE_READ_CHUNK_SIZE);
        if (!chunk) return option_charp_none();

        if (total == 0) base = chunk;

        usize n = fread(chunk, 1, FILE_READ_CHUNK_SIZE, f);
        total += n;

        if (n < FILE_READ_CHUNK_SIZE) {
            if (ferror(f)) return option_charp_none();

            char* term = (char*)arena_alloc(arena, 1);
            if (!term) return option_charp_none();
            *term = '\0';

            return option_charp_some(base);
        }
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading — arena-backed (preferred, zero-heap)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into arena-allocated, null-terminated buffer
 *
 * Preferred method — zero heap allocation, deterministic lifetime.
 *
 * Strategy selection is automatic:
 * - Seekable files: fseek/ftell for size, single fread (fast)
 * - Non-seekable streams: incremental fread in chunks (portable fallback)
 *
 * @param path  Null-terminated file path
 * @param arena Valid initialized arena with sufficient space
 * @return Some(arena-owned char*) on success — null-terminated buffer
 *         None on any failure (file not found, arena exhausted, read error)
 *
 * @remark Returned pointer MUST NOT be freed
 * @remark Binary mode ("rb") — no line-ending translation
 * @remark Buffer is valid until arena is reset or destroyed
 */
static inline option_charp file_read_all_arena(const char* path, Arena* arena) {
    if (!path || !arena) return option_charp_none();

    FILE* f = fopen(path, "rb");
    if (!f) return option_charp_none();
    SCOPE_DEFER { fclose(f); };

    /* Phase 1: seek-based fast path */
    if (fseek(f, 0, SEEK_END) == 0) {
        long len = ftell(f);

        if (len >= 0) {
            /* Overflow check for very large files */
            if ((usize)len + 1 < (usize)len) return option_charp_none();

            if (fseek(f, 0, SEEK_SET) != 0) return option_charp_none();

            usize size = (usize)len + 1;
            char* buf  = (char*)arena_alloc(arena, size);
            if (!buf) return option_charp_none();

            if (fread(buf, 1, (usize)len, f) != (usize)len) return option_charp_none();

            buf[len] = '\0';
            return option_charp_some(buf);
        }
    }

    /* Phase 2: streaming fallback for non-seekable streams */
    return _file_read_stream(f, arena);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading — heap-backed (persistent data)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into heap-allocated, null-terminated buffer
 *
 * Caller must free result with str_free().
 * Uses file_read_all_arena() internally with a scratch arena,
 * then copies result to heap for persistent lifetime.
 *
 * @param path Null-terminated file path
 * @return Some(heap-owned char*) on success — null-terminated buffer
 *         None on any failure
 *
 * @remark Caller MUST free returned string with str_free()
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
 * @param path    Null-terminated file path
 * @param content Null-terminated content to write
 * @return Ok(bytes written) on success
 *         Err(ERR_INVALID_ARG) if path or content is NULL
 *         Err(ERR_IO_FAILED) on file open or write failure
 */
static inline result_usize_Error file_write_all(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(usize, ERR_INVALID_ARG);

    FILE* f = fopen(path, "wb");
    if (!f) return RESULT_ERR(usize, ERR_IO_FAILED);
    SCOPE_DEFER { fclose(f); };

    usize len = str_len(content);
    if (fwrite(content, 1, len, f) != len) return RESULT_ERR(usize, ERR_IO_FAILED);

    return RESULT_OK(usize, len);
}

/**
 * @brief Atomic write: writes to temp file then renames
 *
 * Safer for critical files — crash between write and rename leaves
 * the original file intact. On success, original is atomically replaced.
 *
 * @param path    Final file path
 * @param content Null-terminated content to write
 * @return Ok(bytes written) on success
 *         Err(ERR_INVALID_ARG) if path or content is NULL
 *         Err(ERR_BUFFER_TOO_SMALL) if path is too long
 *         Err(ERR_IO_FAILED) on write or rename failure
 */
static inline result_usize_Error file_write_all_atomic(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(usize, ERR_INVALID_ARG);

    char tmp[512];
    usize path_len = str_len(path);
    if (path_len + 5 > sizeof(tmp)) return RESULT_ERR(usize, ERR_BUFFER_TOO_SMALL);

    mem_copy(tmp, path, path_len);
    mem_copy(tmp + path_len, ".tmp", 5);

    result_usize_Error r = file_write_all(tmp, content);
    if (result_usize_Error_is_err(r)) {
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
 * @brief Checks if file exists and is readable
 *
 * @param path Null-terminated file path
 * @return true if file exists and can be opened for reading, false otherwise
 */
static inline bool file_exists(const char* path) {
    if (!path) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

#endif /* CANON_UTIL_FILE_H */
