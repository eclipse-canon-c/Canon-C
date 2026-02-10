#ifndef CANON_UTIL_FILE_H
#define CANON_UTIL_FILE_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
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
 * ⚠️ Portability note:
 * Some functions (file_read_all_arena / file_read_all) use fseek() + ftell() to determine file size.
 * ISO C does not guarantee this works for:
 *   - Very large files exceeding LONG_MAX
 *   - Non-seekable streams (pipes, sockets)
 *   - Certain exotic or platform-specific filesystems
 * Typical binary files on common platforms are fine.
 * For full portability or very large/non-seekable files, consider streaming with fread + manual realloc.
 * 
 * NOT suitable for:
 *   - Random-access or memory-mapped I/O
 *   - Network protocols (use sockets instead)
 *
 * Allocation strategies:
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
 *
 * Error handling:
 * - Option<T> for may-fail-without-details
 * - Result<T,Error> for operations needing error reason
 *
 * Typical use cases:
 * - Loading configuration, shader or script sources
 * - Simple logging
 * - Small-to-medium file caching
 *
 * @sa file_read_all_arena(), file_read_all(), file_write_all(), file_write_all_atomic()
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
 * @param path  Null-terminated file path
 * @param arena Valid initialized arena with sufficient space
 *
 * @return `Some(arena-owned char*)` on success — buffer contains file content + '\0'
 *         `None` on any failure
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
    if ((size_t)len + 1 < (size_t)len) return option_charp_none(); // overflow check
    if (fseek(f, 0, SEEK_SET) != 0) return option_charp_none();

    size_t size = (size_t)len + 1;
    char* buf = arena_alloc(arena, size);
    if (!buf) return option_charp_none();

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) return option_charp_none();
    buf[len] = '\0';

    return option_charp_some(buf);
}

/* ────────────────────────────────────────────────────────────────────────────
   Reading — heap-backed (persistent data)
   ──────────────────────────────────────────────────────────────────────────── */

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

static inline result_size_t_Error file_write_all(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(size_t, ERR_INVALID_ARG);

    FILE* f = fopen(path, "wb");
    if (!f) return RESULT_ERR(size_t, ERR_IO_FAILED);
    SCOPE_DEFER { fclose(f); };

    size_t len = strlen(content);
    if (fwrite(content, 1, len, f) != len) return RESULT_ERR(size_t, ERR_IO_FAILED);

    return RESULT_OK(size_t, len);
}

/**
 * @brief Atomic write (writes to temp file + rename)
 */
static inline result_size_t_Error file_write_all_atomic(const char* path, const char* content) {
    if (!path || !content) return RESULT_ERR(size_t, ERR_INVALID_ARG);

    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    result_size_t_Error r = file_write_all(tmp, content);
    if (result_is_err(r)) return r;

    if (rename(tmp, path) != 0) {
        remove(tmp);
        return RESULT_ERR(size_t, ERR_IO_FAILED);
    }
    return r;
}

/* ────────────────────────────────────────────────────────────────────────────
   Helper: file exists
   ──────────────────────────────────────────────────────────────────────────── */

static inline bool file_exists(const char* path) {
    if (!path) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Usage Examples (documentation only)
   ────────────────────────────────────────────────────────────────────────────

#include "util/file.h"
#include "core/arena.h"
#include "core/scope.h"

void load_config_temp(void) {
    uint8_t buf[8192];
    Arena a; arena_init(&a, buf, sizeof(buf));

    option_charp cfg = file_read_all_arena("config.ini", &a);
    if (option_charp_is_some(cfg)) {
        parse_config(option_charp_unwrap(cfg));
    }
}

void load_template(void) {
    option_charp html = file_read_all("welcome.html");
    if (option_charp_is_some(html)) {
        char* t = option_charp_unwrap(html);
        SCOPE_DEFER { str_free(t); };
        render_page(t);
    }
}

void log_event(const char* msg) {
    result_size_t_Error r = file_write_all("app.log", msg);
    if (result_is_err(r)) {
        // handle error
    }
}

bool atomic_write(const char* path, const char* content) {
    result_size_t_Error r = file_write_all_atomic(path, content);
    return result_is_ok(r);
}

──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_FILE_H */
