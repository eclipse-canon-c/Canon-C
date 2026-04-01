#ifndef CANON_UTIL_FILE_H
#define CANON_UTIL_FILE_H

#include <stdbool.h>
#include <stdio.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "core/memory.h"
#include "core/arena.h"
#include "semantics/result/result.h"
#include "semantics/error.h"
#include "util/str/str.h"

/**
 * @file util/file.h
 * @brief Safe, explicit file I/O with Result/Option error handling and arena support
 *
 * High-level file utilities emphasizing:
 * - Observable failures via Result<T,Error> and Option<T>
 * - Deterministic memory usage via arena allocation (preferred)
 * - Binary mode by default for cross-platform consistency
 * - Contracts for programming errors (NULL arguments)
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * - path must not be NULL — violated → contract abort
 * - arena/scratch must not be NULL — violated → contract abort
 * - content must not be NULL — violated → contract abort
 * - I/O failures (file not found, write error, arena exhausted) are data
 *   errors returned as None or Err — not contract violations
 *
 * Reading strategy:
 * ────────────────────────────────────────────────────────────────────────────
 * file_read_all_arena() uses a two-phase approach:
 *
 * Phase 1 — seek-based (fast path):
 *   Uses fseek/ftell to determine file size upfront, allocates once,
 *   reads in a single fread call.
 *
 * Phase 2 — streaming fallback (portability path):
 *   If fseek fails (pipes, sockets, exotic filesystems), falls back to
 *   incremental fread using all remaining arena space.
 *
 * Resource cleanup:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions use __attribute__((cleanup)) on GCC/Clang when
 * CANON_NO_GNU_EXTENSIONS is NOT defined. MSVC falls through to the
 * plain declaration path automatically (no __attribute__ support).
 *
 * Side effects:
 * ────────────────────────────────────────────────────────────────────────────
 * - File system operations (fopen, fwrite, rename, remove) modify external state
 * - file_write_all_atomic() creates and removes a temporary ".tmp" file
 * - file_exists() opens and immediately closes the file (TOCTOU advisory)
 */

/* ────────────────────────────────────────────────────────────────────────────
   Result type for write operations
   ──────────────────────────────────────────────────────────────────────────── */
CANON_RESULT(usize, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Internal: automatic FILE* cleanup
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Cleanup function for __attribute__((cleanup)) on FILE* variables
 *
 * NULL-safe by design — cleanup functions must handle the case where
 * fopen() failed and the variable holds NULL.
 *
 * @remark Internal — do not call directly.
 */
static inline void canon_file_auto_close_(FILE** f) {
    if (f && *f) {
        fclose(*f);
        *f = NULL;
    }
}

/**
 * @def FILE_AUTOCLOSE(name, path, mode)
 * @brief Declares a FILE* that is automatically closed on scope exit
 *
 * Uses __attribute__((cleanup)) on GCC/Clang when GNU extensions are enabled.
 * MSVC does not support __attribute__ — detected automatically via compiler check.
 * When cleanup is unavailable, callers MUST fclose() manually before every return.
 */
#if !defined(CANON_NO_GNU_EXTENSIONS) && (defined(__GNUC__) || defined(__clang__))
    #define FILE_AUTOCLOSE(name, path, mode) \
        FILE* name __attribute__((cleanup(canon_file_auto_close_))) = fopen(path, mode)
#else
    #define FILE_AUTOCLOSE(name, path, mode) \
        FILE* name = fopen(path, mode)
#endif

/* ────────────────────────────────────────────────────────────────────────────
   Internal: streaming fread fallback for non-seekable streams
   ──────────────────────────────────────────────────────────────────────────── */

#ifndef FILE_READ_CHUNK_SIZE
    #define FILE_READ_CHUNK_SIZE ((usize)4096)
#endif

/**
 * @brief Reads a non-seekable stream into arena using a single contiguous allocation
 *
 * @param f     Open FILE* in binary read mode (must not be NULL — contract)
 * @param arena Arena with sufficient remaining space (must not be NULL — contract)
 * @return Some(char*) on success, None on error or insufficient arena space
 *
 * @remark Internal — use file_read_all_arena() instead.
 */
static inline option_charp canon_file_read_stream_(
    borrowed(FILE*)  f,
    borrowed(Arena*) arena)
{
    require_msg(f     != NULL, "canon_file_read_stream_: f is NULL");
    require_msg(arena != NULL, "canon_file_read_stream_: arena is NULL");

    usize available = arena_remaining(arena);
    if (available < 2) return option_charp_none();

    ArenaMark mark = arena_mark(arena);
    char* base = (char*)arena_alloc(arena, available);
    if (!base) return option_charp_none();

    usize usable = available - 1;
    usize total  = 0;

    while (total < usable) {
        usize chunk = usable - total;
        if (chunk > FILE_READ_CHUNK_SIZE) chunk = FILE_READ_CHUNK_SIZE;

        usize n = fread(base + total, 1, chunk, f);
        total += n;

        if (n < chunk) {
            if (ferror(f)) {
                arena_reset_to(arena, mark);
                return option_charp_none();
            }
            break;
        }
    }

    base[total] = '\0';

    arena_reset_to(arena, mark);
    char* result = (char*)arena_alloc(arena, total + 1);
    if (!result) return option_charp_none();

    result[total] = '\0';
    return option_charp_some(result);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading — arena-backed (preferred, zero-heap)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into arena-allocated, null-terminated buffer
 *
 * @param path  Null-terminated file path (must not be NULL — contract)
 * @param arena Valid initialized arena (must not be NULL — contract)
 * @return Some(arena-owned char*) on success, None on I/O failure
 *
 * @remark Returned pointer MUST NOT be freed — valid until arena reset
 * @remark Binary mode ("rb") — no line-ending translation
 */
static inline option_charp file_read_all_arena(
    borrowed(const char*) path,
    borrowed(Arena*)      arena)
{
    require_msg(path  != NULL, "file_read_all_arena: path is NULL");
    require_msg(arena != NULL, "file_read_all_arena: arena is NULL");

    FILE_AUTOCLOSE(f, path, "rb");
    if (!f) return option_charp_none();

    if (fseek(f, 0, SEEK_END) == 0) {
        long len = ftell(f);

        if (len >= 0) {
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

    return canon_file_read_stream_(f, arena);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading — heap-backed (persistent data)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Reads entire file into heap-allocated, null-terminated buffer
 *
 * @param path    Null-terminated file path (must not be NULL — contract)
 * @param scratch Initialized arena for temporary use (must not be NULL — contract)
 * @return Some(heap-owned char*) on success, None on failure
 *
 * @remark Caller MUST free returned string with str_free()
 * @remark Binary mode ("rb")
 */
static inline option_charp file_read_all(
    borrowed(const char*) path,
    borrowed(Arena*)      scratch)
{
    require_msg(path    != NULL, "file_read_all: path is NULL");
    require_msg(scratch != NULL, "file_read_all: scratch is NULL");

    option_charp tmp = file_read_all_arena(path, scratch);
    if (option_charp_is_none(tmp)) return option_charp_none();

    return str_alloc_copy(option_charp_unwrap(tmp));
}

/* ────────────────────────────────────────────────────────────────────────────
   Writing
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Writes entire content to file (binary mode)
 *
 * @param path    Null-terminated file path (must not be NULL — contract)
 * @param content Null-terminated content to write (must not be NULL — contract)
 * @return Ok(bytes written) on success
 *         Err(ERR_IO_FAILED) on file open or write failure
 */
static inline result_usize_Error file_write_all(
    borrowed(const char*) path,
    borrowed(const char*) content)
{
    require_msg(path    != NULL, "file_write_all: path is NULL");
    require_msg(content != NULL, "file_write_all: content is NULL");

    FILE_AUTOCLOSE(f, path, "wb");
    if (!f) return result_usize_Error_err(ERR_IO_FAILED);

    usize len = str_len(content);
    if (fwrite(content, 1, len, f) != len) return result_usize_Error_err(ERR_IO_FAILED);

    return result_usize_Error_ok(len);
}

/**
 * @brief Maximum supported file path length for atomic writes
 *
 * Override by defining FILE_MAX_PATH before including this header.
 */
#ifndef FILE_MAX_PATH
    #define FILE_MAX_PATH ((usize)512)
#endif

/**
 * @brief Atomic write: writes to temp file then renames
 *
 * @param path    Final file path (must not be NULL — contract)
 * @param content Null-terminated content to write (must not be NULL — contract)
 * @return Ok(bytes written) on success
 *         Err(ERR_BUFFER_TOO_SMALL) if path is too long for temp buffer
 *         Err(ERR_IO_FAILED) on write or rename failure
 */
static inline result_usize_Error file_write_all_atomic(
    borrowed(const char*) path,
    borrowed(const char*) content)
{
    require_msg(path    != NULL, "file_write_all_atomic: path is NULL");
    require_msg(content != NULL, "file_write_all_atomic: content is NULL");

    char tmp[FILE_MAX_PATH];
    usize path_len = str_len(path);

    if (path_len + 5 > FILE_MAX_PATH) return result_usize_Error_err(ERR_BUFFER_TOO_SMALL);

    mem_copy(tmp, path, path_len);
    mem_copy(tmp + path_len, ".tmp", 5);

    result_usize_Error r = file_write_all(tmp, content);
    if (result_usize_Error_is_err(r)) {
        remove(tmp);
        return r;
    }

    if (rename(tmp, path) != 0) {
        remove(tmp);
        return result_usize_Error_err(ERR_IO_FAILED);
    }

    return r;
}

/* ────────────────────────────────────────────────────────────────────────────
   Helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if file exists and is readable
 *
 * @param path Null-terminated file path (must not be NULL — contract)
 * @return true if file can be opened for reading, false otherwise
 *
 * @remark Advisory only — TOCTOU risk. Do not use as a security gate.
 */
static inline bool file_exists(borrowed(const char*) path) {
    require_msg(path != NULL, "file_exists: path is NULL");
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

#endif /* CANON_UTIL_FILE_H */
