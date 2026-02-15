#ifndef CANON_DATA_STRINGBUF_H
#define CANON_DATA_STRINGBUF_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/arena.h"
#include "core/slice.h"

/**
 * @file stringbuf.h
 * @brief Fixed-capacity incremental string builder (arena-backed or caller-owned buffer)
 *
 * StringBuf provides efficient, safe string concatenation and formatting into a
 * fixed-size buffer. It is deliberately NOT a growable string.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Capacity is fixed at initialization — no automatic growth or reallocation
 * - Always null-terminated — even when empty or after a failed append
 * - All appends return bool — fail gracefully and leave buffer unchanged
 * - Two backing strategies: Arena-allocated (recommended) or caller-owned buffer
 * - Zero hidden state, no global variables, minimal overhead
 * - str_t / bytes_t views via stringbuf_as_str() / stringbuf_as_bytes()
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * stringbuf.h is data/. It may include core/ only.
 * No other data/ headers may be included here.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each StringBuf instance is independent — no shared state.
 * Concurrent modifications require external synchronization.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (vsnprintf, va_copy, inline)
 * - Uses <stdarg.h>, <stdio.h>, <string.h> — no Canon-C replacements exist
 * - No platform-specific code
 *
 * - <stdarg.h>: va_list/va_start/va_end are compiler ABI intrinsics — not a library concern
 * - <stdio.h>:  vsnprintf is the only way to implement printf-style formatting — no Canon-C substitute
 * - <string.h>: strlen required by stringbuf_append(const char*) — stringbuf_append_str(str_t)
 *               avoids it entirely if you prefer length-explicit call sites
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - append:            O(n) where n = appended string length
 * - append_char:       O(1)
 * - append_fmt:        O(n) — two vsnprintf passes (measure then write)
 * - append_n:          O(min(n, strlen(s)))
 * - append_str:        O(n) — accepts str_t directly, no strlen needed
 * - as_str / as_bytes: O(1) — view construction
 * - All queries:       O(1)
 * - No allocations after initialization
 *
 * Important limitations:
 * ────────────────────────────────────────────────────────────────────────────
 * - Once full, all appends fail (return false) — buffer is unchanged
 * - capacity includes the null terminator — usable chars = capacity - 1
 * - Arena-backed buffers become invalid after arena_reset()
 * - No reserve, grow, or realloc functions — by design
 *
 * Quick start:
 * ```c
 * #include "data/stringbuf.h"
 *
 * // Arena-backed (recommended)
 * StringBuf sb;
 * if (!stringbuf_init_arena(&sb, &arena, 1024)) { ... }
 * stringbuf_append(&sb, "Hello, ");
 * stringbuf_append_fmt(&sb, "%s!", name);
 * printf("%s\n", stringbuf_str(&sb));
 *
 * // Stack-backed (zero allocation)
 * char buf[256];
 * StringBuf path;
 * stringbuf_init_buffer(&path, buf, sizeof(buf));
 * stringbuf_append(&path, "/home/user/");
 * stringbuf_append(&path, filename);
 *
 * // str_t / bytes_t views
 * str_t   sv = stringbuf_as_str(&sb);     // borrowed, no copy
 * bytes_t bv = stringbuf_as_bytes(&sb);   // raw byte view
 * ```
 *
 * @sa core/slice.h       — str_t, bytes_t used for views
 * @sa data/vec/vec_fmt.h — format a vec into a StringBuf
 */

/* ════════════════════════════════════════════════════════════════════════════
   StringBuf struct
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fixed-capacity string builder
 *
 * Invariants:
 * - data[len] == '\0' (always null-terminated)
 * - len < capacity (always room for null terminator)
 * - If arena != NULL, data was allocated from that arena
 *
 * Do not access or modify fields directly — use the provided functions.
 */
typedef struct {
    Arena* arena;    ///< Backing arena (NULL = caller-owned buffer)
    char*  data;     ///< Buffer pointer (always null-terminated when valid)
    usize  len;      ///< Current string length (excluding '\0')
    usize  capacity; ///< Total buffer size including space for '\0'
} StringBuf;

/* ════════════════════════════════════════════════════════════════════════════
   Constructors
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes StringBuf using an Arena-allocated buffer (recommended)
 *
 * @param sb          Pointer to uninitialized StringBuf
 * @param arena       Valid, initialized Arena
 * @param initial_cap Total capacity including null terminator (must be > 1)
 * @return true on success, false on failure
 *
 * @pre sb != NULL, arena != NULL, initial_cap > 1
 *
 * @post On success: sb->data[0] == '\0', sb->len == 0, sb->capacity == initial_cap
 *
 * Performance:
 * - Time:  O(1) — single arena bump
 * - Space: O(initial_cap) consumed from arena
 */
static inline bool stringbuf_init_arena(StringBuf* sb, Arena* arena, usize initial_cap) {
    require_msg(sb != NULL,      "stringbuf_init_arena: sb cannot be NULL");
    require_msg(arena != NULL,   "stringbuf_init_arena: arena cannot be NULL");
    require_msg(initial_cap > 1, "stringbuf_init_arena: initial_cap must be > 1");

    if (!sb || !arena || initial_cap == 0) return false;

    char* buf = (char*)arena_alloc(arena, initial_cap);
    if (!buf) return false;

    buf[0] = '\0';
    *sb = (StringBuf){
        .arena    = arena,
        .data     = buf,
        .len      = 0,
        .capacity = initial_cap
    };
    return true;
}

/**
 * @brief Initializes StringBuf with a caller-owned fixed buffer
 *
 * @param sb     Pointer to uninitialized StringBuf
 * @param buffer Pointer to char buffer (must remain valid for StringBuf lifetime)
 * @param cap    Total capacity including null terminator (must be > 1)
 *
 * @pre sb != NULL, buffer != NULL, cap > 1
 *
 * @post sb->data == buffer, sb->data[0] == '\0', sb->arena == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation
 */
static inline void stringbuf_init_buffer(StringBuf* sb, char* buffer, usize cap) {
    require_msg(sb != NULL,     "stringbuf_init_buffer: sb cannot be NULL");
    require_msg(buffer != NULL, "stringbuf_init_buffer: buffer cannot be NULL");
    require_msg(cap > 1,        "stringbuf_init_buffer: cap must be > 1");

    if (!sb || !buffer || cap == 0) return;

    buffer[0] = '\0';
    *sb = (StringBuf){
        .arena    = NULL,
        .data     = buffer,
        .len      = 0,
        .capacity = cap
    };
}

/* ════════════════════════════════════════════════════════════════════════════
   Append operations
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Appends a null-terminated C string (bounds-checked)
 *
 * @param sb StringBuf to append to
 * @param s  String to append (NULL = no-op, returns true)
 * @return true on success, false if insufficient space
 *
 * Performance: O(strlen(s))
 */
static inline bool stringbuf_append(StringBuf* sb, const char* s) {
    if (!sb || !sb->data) return false;
    if (!s) return true;

    usize add_len = (usize)strlen(s);
    if (add_len > CANON_USIZE_MAX - sb->len - 1) return false;
    if (sb->len + add_len + 1 > sb->capacity)    return false;

    /* ptr_offset replaces: sb->data + sb->len */
    mem_copy(ptr_offset(sb->data, sb->len), s, add_len);
    sb->len += add_len;
    sb->data[sb->len] = '\0';
    return true;
}

/**
 * @brief Appends a str_t view (no null terminator required in source)
 *
 * Accepts the canonical str_t type from core/slice.h directly.
 * Length is taken from s.len — no strlen scan needed.
 *
 * @param sb StringBuf to append to
 * @param s  str_t view to append (empty str_t = no-op, returns true)
 * @return true on success, false if insufficient space
 *
 * Performance: O(s.len)
 */
static inline bool stringbuf_append_str(StringBuf* sb, str_t s) {
    if (!sb || !sb->data) return false;
    if (!s.ptr || s.len == 0) return true;

    if (s.len > CANON_USIZE_MAX - sb->len - 1) return false;
    if (sb->len + s.len + 1 > sb->capacity)    return false;

    mem_copy(ptr_offset(sb->data, sb->len), s.ptr, s.len);
    sb->len += s.len;
    sb->data[sb->len] = '\0';
    return true;
}

/**
 * @brief Appends a single character
 *
 * @param sb StringBuf to append to
 * @param c  Character to append
 * @return true on success, false if buffer full
 *
 * Performance: O(1)
 */
static inline bool stringbuf_append_char(StringBuf* sb, char c) {
    if (!sb || !sb->data) return false;
    if (sb->len + 2 > sb->capacity) return false;

    sb->data[sb->len++] = c;
    sb->data[sb->len]   = '\0';
    return true;
}

/**
 * @brief Appends formatted text (printf-style)
 *
 * Uses vsnprintf internally — two passes (measure then write).
 * On failure the buffer is unchanged and remains null-terminated.
 *
 * @param sb  StringBuf to append to
 * @param fmt printf-style format string
 * @param ... Format arguments
 * @return true on success, false on failure or insufficient space
 *
 * Performance: O(n) — two vsnprintf passes
 */
static inline bool stringbuf_append_fmt(StringBuf* sb, const char* fmt, ...) {
    if (!sb || !fmt || !sb->data) return false;

    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0) return false;
    if ((usize)needed > CANON_USIZE_MAX - sb->len - 1 ||
        sb->len + (usize)needed + 1 > sb->capacity) {
        return false;
    }

    va_start(args, fmt);
    int written = vsnprintf(
        (char*)ptr_offset(sb->data, sb->len),
        sb->capacity - sb->len,
        fmt, args);
    va_end(args);

    if (written < 0 || written != needed) return false;
    sb->len += (usize)written;
    return true;
}

/**
 * @brief Appends formatted text using an existing va_list
 *
 * @param sb   StringBuf to append to
 * @param fmt  Format string
 * @param args va_list of arguments (caller manages va_start/va_end)
 * @return true on success, false on failure
 *
 * Performance: O(n) — two vsnprintf passes
 */
static inline bool stringbuf_append_fmt_va(StringBuf* sb, const char* fmt, va_list args) {
    if (!sb || !fmt || !sb->data) return false;

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) return false;
    if ((usize)needed > CANON_USIZE_MAX - sb->len - 1 ||
        sb->len + (usize)needed + 1 > sb->capacity) {
        return false;
    }

    int written = vsnprintf(
        (char*)ptr_offset(sb->data, sb->len),
        sb->capacity - sb->len,
        fmt, args);
    if (written < 0 || written != needed) return false;
    sb->len += (usize)written;
    return true;
}

/**
 * @brief Appends at most n characters from string s
 *
 * Appends min(n, strlen(s)) characters.
 *
 * @param sb StringBuf to append to
 * @param s  Source string (NULL = no-op, returns true)
 * @param n  Maximum number of characters to append
 * @return true on success, false if insufficient space
 *
 * Performance: O(min(n, strlen(s)))
 */
static inline bool stringbuf_append_n(StringBuf* sb, const char* s, usize n) {
    if (!sb || !sb->data) return false;
    if (!s || n == 0)     return true;

    usize actual_len = 0;
    while (actual_len < n && s[actual_len] != '\0') actual_len++;

    if (actual_len > CANON_USIZE_MAX - sb->len - 1) return false;
    if (sb->len + actual_len + 1 > sb->capacity)    return false;

    mem_copy(ptr_offset(sb->data, sb->len), s, actual_len);
    sb->len += actual_len;
    sb->data[sb->len] = '\0';
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Access and views
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a raw C string pointer (null-terminated, borrowed)
 *
 * Always safe — returns "" for NULL or uninitialised StringBuf.
 * Pointer is valid until the backing buffer is destroyed or arena is reset.
 *
 * @note Do NOT free the returned pointer.
 *
 * Performance: O(1)
 */
static inline const char* stringbuf_str(const StringBuf* sb) {
    return (sb && sb->data) ? sb->data : "";
}

/**
 * @brief Returns a str_t non-owning view over the current string contents
 *
 * The returned str_t is valid until the backing buffer is destroyed,
 * the arena is reset, or the StringBuf is cleared/truncated.
 * Do NOT free str_t.ptr — it is owned by the StringBuf's backing buffer.
 *
 * @param sb StringBuf to view (must not be NULL)
 * @return str_t — empty str_t if sb == NULL or data == NULL
 *
 * Performance: O(1)
 */
static inline str_t stringbuf_as_str(const StringBuf* sb) {
    if (!sb || !sb->data) return str_empty();
    return str_from(sb->data, sb->len);
}

/**
 * @brief Returns a bytes_t non-owning view over the string's raw bytes
 *
 * Covers [data, data + len) — does NOT include the null terminator byte.
 * Useful for passing StringBuf contents to byte-level APIs.
 *
 * @param sb StringBuf to view (must not be NULL)
 * @return bytes_t — empty bytes_t if sb == NULL or data == NULL
 *
 * Performance: O(1)
 */
static inline bytes_t stringbuf_as_bytes(const StringBuf* sb) {
    if (!sb || !sb->data) return bytes_empty();
    return bytes_from(sb->data, sb->len);
}

/**
 * @brief Returns a bytes_t view over the entire buffer including free space
 *
 * Covers [data, data + capacity). Includes used + null terminator + free.
 * Useful for bulk inspection or passing the full buffer to I/O.
 *
 * Performance: O(1)
 */
static inline bytes_t stringbuf_buffer_bytes(const StringBuf* sb) {
    if (!sb || !sb->data) return bytes_empty();
    return bytes_from(sb->data, sb->capacity);
}

/* ════════════════════════════════════════════════════════════════════════════
   Query
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Returns current string length (excluding '\0'). NULL-safe. O(1) */
static inline usize stringbuf_len(const StringBuf* sb) {
    return sb ? sb->len : 0;
}

/** @brief Returns total buffer capacity (including '\0' byte). NULL-safe. O(1) */
static inline usize stringbuf_capacity(const StringBuf* sb) {
    return sb ? sb->capacity : 0;
}

/** @brief Returns number of characters still appendable. NULL-safe. O(1) */
static inline usize stringbuf_remaining(const StringBuf* sb) {
    if (!sb || sb->capacity == 0) return 0;
    return (sb->capacity > sb->len + 1) ? (sb->capacity - sb->len - 1) : 0;
}

/** @brief Returns true if string is empty (len == 0). NULL-safe. O(1) */
static inline bool stringbuf_is_empty(const StringBuf* sb) {
    return !sb || sb->len == 0;
}

/** @brief Returns true if no more characters can be appended. NULL-safe. O(1) */
static inline bool stringbuf_is_full(const StringBuf* sb) {
    return !sb || (sb->len + 1 >= sb->capacity);
}

/** @brief Returns true if buffer was allocated from an arena. NULL-safe. O(1) */
static inline bool stringbuf_is_arena_backed(const StringBuf* sb) {
    return sb && sb->arena != NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
   Mutation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resets the string to empty without freeing the buffer
 *
 * @post sb->len == 0, sb->data[0] == '\0'
 *
 * Performance: O(1)
 */
static inline void stringbuf_clear(StringBuf* sb) {
    if (sb && sb->data) {
        sb->data[0] = '\0';
        sb->len     = 0;
    }
}

/**
 * @brief Truncates the string to new_len characters
 *
 * No-op if new_len >= sb->len.
 *
 * @post If new_len < sb->len: sb->len == new_len, sb->data[new_len] == '\0'
 *
 * Performance: O(1)
 */
static inline void stringbuf_truncate(StringBuf* sb, usize new_len) {
    if (sb && sb->data && new_len < sb->len) {
        sb->len           = new_len;
        sb->data[sb->len] = '\0';
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Convenience alias
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def stringbuf_printf
 * @brief Alias for stringbuf_append_fmt — matches printf naming conventions
 *
 * ```c
 * stringbuf_printf(&sb, "x=%d y=%d", x, y);
 * ```
 */
#define stringbuf_printf stringbuf_append_fmt

#endif /* CANON_DATA_STRINGBUF_H */
