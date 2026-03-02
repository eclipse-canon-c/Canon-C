#ifndef CANON_UTIL_FILE_H
#define CANON_UTIL_FILE_H

#include "core/primitives/types.h"       // usize, bool, uint8_t
#include "core/primitives/contract.h"    // require_msg
#include "core/memory.h"                 // mem_copy
#include "core/arena.h"                  // Arena, arena_alloc, arena_init, arena_reset,
                                         // arena_remaining, arena_mark, arena_reset_to
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
 *   If fseek fails (pipes, sockets, exotic filesystems), falls back to
 *   incremental fread using all remaining arena space as a single contiguous
 *   buffer. Unused tail is reclaimed via arena_mark/arena_reset_to.
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
 * Resource cleanup:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions use __attribute__((cleanup)) when GNU extensions are available
 * (i.e. when CANON_NO_GNU_EXTENSIONS is NOT defined). This ensures FILE*
 * handles are always closed on every return path including early returns.
 *
 * When CANON_NO_GNU_EXTENSIONS IS defined, all functions fall back to
 * explicit fclose() before every return. This is verbose but correct.
 *
 * Error handling:
 * - Option<T> for may-fail-without-details
 * - Result<T,Error> for operations needing error reason
 *
 * @sa file_read_all_arena() — preferred zero-heap path (auto-selects strategy)
 * @sa file_read_all()       — persistent heap path (caller provides scratch arena)
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
 * Called automatically when a FILE* variable goes out of scope.
 * NULL-safe — safe to call even if fopen() failed.
 *
 * @remark Internal — do not call directly.
 */
static inline void _file_auto_close(FILE** f) {
    if (f && *f) {
        fclose(*f);
        *f = NULL;
    }
}

/**
 * @def FILE_AUTOCLOSE(name, path, mode)
 * @brief Declares a FILE* that is automatically closed on scope exit
 *
 * Uses __attribute__((cleanup)) when GNU extensions are available.
 * Falls back to a plain declaration when CANON_NO_GNU_EXTENSIONS is defined —
 * in that case, callers MUST fclose() manually before every return.
 *
 * Usage:
 * ```c
 * FILE_AUTOCLOSE(f, path, "rb");
 * if (!f) return option_charp_none();
 * // f is closed automatically on any return (GNU) or manually (no-GNU)
 * ```
 */
#ifndef CANON_NO_GNU_EXTENSIONS
    #define FILE_AUTOCLOSE(name, path, mode) \
        FILE* name __attribute__((cleanup(_file_auto_close))) = fopen(path, mode)
#else
    /* Without GNU extensions: plain declaration, caller must fclose manually */
    #define FILE_AUTOCLOSE(name, path, mode) \
        FILE* name = fopen(path, mode)
#endif

/* ────────────────────────────────────────────────────────────────────────────
   Internal: streaming fread fallback for non-seekable streams
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Chunk size for streaming fread iterations
 *
 * Does NOT control allocation size — the entire remaining arena space is
 * allocated upfront as one contiguous buffer. This constant only controls
 * how many bytes are requested per fread() call within that buffer.
 *
 * Override by defining FILE_READ_CHUNK_SIZE before including this header.
 * Default: 4096 bytes (one typical page).
 */
#ifndef FILE_READ_CHUNK_SIZE
    #define FILE_READ_CHUNK_SIZE ((usize)4096)
#endif

/**
 * @brief Reads a non-seekable stream into arena using a single contiguous allocation
 *
 * Allocates all remaining arena space upfront as one buffer, reads into it
 * in FILE_READ_CHUNK_SIZE increments, then reclaims unused tail via
 * arena_mark/arena_reset_to.
 *
 * Contiguity guarantee: safe because a single arena_alloc() call always
 * returns one contiguous block. No multi-alloc chunk stitching is performed.
 *
 * Tail reclaim note: arena_reset_to() does not zero memory. The immediately
 * following arena_alloc() on a bump allocator returns the same base address.
 * This is documented behavior of Canon-C's arena — see arena.h.
 *
 * @param f     Open FILE* in binary read mode (non-seekable)
 * @param arena Arena with sufficient remaining space
 * @return Some(char*) pointing to null-terminated buffer on success
 *         None if arena has < 2 bytes remaining, or on read error
 *
 * @remark Internal — do not call directly. Use file_read_all_arena().
 */
static inline option_charp _file_read_stream(FILE* f, Arena* arena) {
    usize available = arena_remaining(arena);
    if (available < 2) return option_charp_none(); /* need ≥ 1 byte content + null */

    /* Single contiguous allocation — no chunk-stitching, no gap risk */
    ArenaMark mark = arena_mark(arena);
    char* base = (char*)arena_alloc(arena, available);
    if (!base) return option_charp_none();

    usize usable = available - 1; /* reserve last byte for null terminator */
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
            break; /* EOF */
        }
    }

    base[total] = '\0';

    /*
     * Reclaim unused tail:
     * Reset to mark, re-alloc exactly (total + 1) bytes.
     * arena_reset_to() does not zero memory, and a bump allocator always
     * returns the same address for the same offset — so base remains valid
     * and result == base. See arena.h for bump allocation guarantees.
     */
    arena_reset_to(arena, mark);
    char* result = (char*)arena_alloc(arena, total + 1);
    if (!result) return option_charp_none(); /* should not happen */

    result[total] = '\0'; /* re-terminate after re-alloc */
    return option_charp_some(result);
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
 * - Non-seekable streams: single large arena alloc with incremental fread
 *
 * FILE* is automatically closed on all return paths when GNU extensions
 * are available. See FILE_AUTOCLOSE for fallback behavior.
 *
 * @param path  Null-terminated file path
 * @param arena Valid initialized arena with sufficient space for file + 1 byte
 * @return Some(arena-owned char*) on success — null-terminated buffer
 *         None on any failure (file not found, arena exhausted, read error)
 *
 * @remark Returned pointer MUST NOT be freed
 * @remark Binary mode ("rb") — no line-ending translation
 * @remark Buffer is valid until arena is reset or destroyed
 */
static inline option_charp file_read_all_arena(const char* path, Arena* arena) {
    if (!path || !arena) return option_charp_none();

    FILE_AUTOCLOSE(f, path, "rb");
    if (!f) return option_charp_none();

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
 * Uses file_read_all_arena() internally with a caller-provided scratch arena,
 * then copies the result to the heap for a persistent lifetime.
 *
 * The scratch arena is used only during this call and can be reset immediately
 * after. Its required size is: file size + 1 byte (null) + alignment padding.
 *
 * Caller must free result with str_free().
 *
 * Example:
 * ```c
 * uint8_t scratch_buf[8192];
 * Arena scratch;
 * arena_init(&scratch, scratch_buf, sizeof(scratch_buf));
 *
 * option_charp result = file_read_all("config.txt", &scratch);
 * arena_reset(&scratch); // scratch is no longer needed
 *
 * if (option_charp_is_some(result)) {
 *     char* s = option_charp_unwrap(result);
 *     // use s...
 *     str_free(s);
 * }
 * ```
 *
 * @param path    Null-terminated file path
 * @param scratch Initialized arena sized for at least (file_size + 1 + alignment)
 * @return Some(heap-owned char*) on success — null-terminated buffer
 *         None on any failure (file not found, arena too small, read error)
 *
 * @remark Caller MUST free returned string with str_free()
 * @remark scratch arena content is unspecified after this call — reset or reuse freely
 * @remark Binary mode ("rb")
 */
static inline option_charp file_read_all(const char* path, Arena* scratch) {
    if (!path || !scratch) return option_charp_none();

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
 * FILE* is automatically closed on all return paths when GNU extensions
 * are available. See FILE_AUTOCLOSE for fallback behavior.
 *
 * @param path    Null-terminated file path
 * @param content Null-terminated content to write
 * @return Ok(bytes written) on success
 *         Err(ERR_INVALID_ARG) if path or content is NULL
 *         Err(ERR_IO_FAILED) on file open or write failure
 */
static inline result_usize_Error file_write_all(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(usize, ERR_INVALID_ARG);

    FILE_AUTOCLOSE(f, path, "wb");
    if (!f) return RESULT_ERR(usize, ERR_IO_FAILED);

    usize len = str_len(content);
    if (fwrite(content, 1, len, f) != len) return RESULT_ERR(usize, ERR_IO_FAILED);

    return RESULT_OK(usize, len);
}

/**
 * @brief Atomic write: writes to temp file then renames
 *
 * Safer for critical files — a crash between write and rename leaves
 * the original file intact. On success, original is atomically replaced.
 *
 * FILE* is automatically closed on all return paths when GNU extensions
 * are available. See FILE_AUTOCLOSE for fallback behavior.
 *
 * @param path    Final file path
 * @param content Null-terminated content to write
 * @return Ok(bytes written) on success
 *         Err(ERR_INVALID_ARG) if path or content is NULL
 *         Err(ERR_BUFFER_TOO_SMALL) if path is too long for temp buffer
 *         Err(ERR_IO_FAILED) on write or rename failure
 */

/**
 * @brief Maximum supported file path length for atomic writes
 *
 * 512 bytes covers the vast majority of real-world paths. POSIX PATH_MAX
 * is typically 4096, but that would require stack or heap allocation.
 * Override by defining FILE_MAX_PATH before including this header.
 *
 * @remark Paths longer than this return Err(ERR_BUFFER_TOO_SMALL).
 */
#ifndef FILE_MAX_PATH
    #define FILE_MAX_PATH ((usize)512)
#endif

static inline result_usize_Error file_write_all_atomic(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(usize, ERR_INVALID_ARG);

    char tmp[FILE_MAX_PATH];
    usize path_len = str_len(path);

    /* +5 for ".tmp\0" */
    if (path_len + 5 > FILE_MAX_PATH) return RESULT_ERR(usize, ERR_BUFFER_TOO_SMALL);

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
 *
 * @remark Result is advisory — file may be deleted between this check and use (TOCTOU).
 *         Do not use as a security gate; use only for diagnostic or optional logic.
 */
static inline bool file_exists(const char* path) {
    if (!path) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

#endif /* CANON_UTIL_FILE_H */
