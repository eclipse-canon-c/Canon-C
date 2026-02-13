#ifndef CANON_DATA_STRINGBUF_H
#define CANON_DATA_STRINGBUF_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/memory.h"
#include "core/arena.h"

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
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - append:            O(n) where n = appended string length
 * - append_char:       O(1)
 * - append_fmt:        O(n) — two vsnprintf passes (measure then write)
 * - append_n:          O(min(n, strlen(s)))
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
 * // Arena-backed (recommended — buffer lifetime tied to arena)
 * StringBuf sb;
 * if (!stringbuf_init_arena(&sb, &arena, 1024)) {
 *     // handle error
 * }
 * stringbuf_append(&sb, "Hello, ");
 * stringbuf_append_fmt(&sb, "%s!", name);
 * printf("%s\n", stringbuf_str(&sb));
 *
 * // Stack-backed (zero allocation, fixed scope)
 * char buf[256];
 * StringBuf path;
 * stringbuf_init_buffer(&path, buf, sizeof(buf));
 * stringbuf_append(&path, "/home/user/");
 * stringbuf_append(&path, filename);
 * ```
 *
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
 *
 * Memory layout:
 * - sizeof(StringBuf) = sizeof(Arena*) + sizeof(char*) + 2*sizeof(usize)
 * - Backing buffer: capacity bytes (includes null terminator)
 */
typedef struct {
    Arena* arena;   ///< Backing arena (NULL = caller-owned buffer)
    char*  data;    ///< Buffer pointer (always null-terminated when valid)
    usize  len;     ///< Current string length (excluding '\0')
    usize  capacity; ///< Total buffer size including space for '\0'
} StringBuf;

/* ════════════════════════════════════════════════════════════════════════════
   Constructors
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes StringBuf using an Arena-allocated buffer (recommended)
 *
 * Allocates initial_cap bytes from the given arena and sets up the StringBuf.
 * Buffer lifetime is tied to the arena — it becomes invalid after arena_reset().
 *
 * @param sb          Pointer to uninitialized StringBuf
 * @param arena       Valid, initialized Arena with sufficient remaining space
 * @param initial_cap Total capacity including null terminator (must be > 1)
 * @return true on success, false on failure
 *
 * @pre sb != NULL
 * @pre arena != NULL and initialized via arena_init()
 * @pre initial_cap > 1 (capacity must fit at least one character + null terminator)
 *
 * @post On success: sb->data[0] == '\0', sb->len == 0, sb->capacity == initial_cap
 * @post On failure: sb state is undefined — do not use
 *
 * @note Returns false if any parameter is NULL, initial_cap == 0,
 *       or arena has insufficient remaining space.
 *
 * Performance:
 * - Time:  O(1) — single arena bump
 * - Space: O(initial_cap) consumed from arena
 */
static inline bool stringbuf_init_arena(StringBuf* sb, Arena* arena, usize initial_cap) {
    require_msg(sb != NULL,         "stringbuf_init_arena: sb cannot be NULL");
    require_msg(arena != NULL,      "stringbuf_init_arena: arena cannot be NULL");
    require_msg(initial_cap > 1,    "stringbuf_init_arena: initial_cap must be > 1");

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
 * Wraps an existing buffer. The caller must ensure the buffer remains valid
 * for the lifetime of the StringBuf.
 *
 * @param sb     Pointer to uninitialized StringBuf
 * @param buffer Pointer to char buffer (must remain valid)
 * @param cap    Total capacity including null terminator (must be > 1)
 *
 * @pre sb != NULL
 * @pre buffer != NULL
 * @pre cap > 1
 *
 * @post sb->data == buffer, sb->data[0] == '\0', sb->len == 0
 * @post sb->arena == NULL (not arena-backed)
 *
 * @warning Buffer must remain valid for the entire lifetime of the StringBuf.
 *          Stack-allocated buffers become invalid when their scope ends.
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
 * @brief Appends a null-terminated string (bounds-checked)
 *
 * On failure the buffer is unchanged and remains null-terminated.
 *
 * @param sb StringBuf to append to
 * @param s  String to append (NULL treated as empty string — no-op, returns true)
 * @return true on success, false if sb is invalid or insufficient space
 *
 * @post On true: sb->data ends with s, sb->len increased by strlen(s)
 * @post On false: sb is unchanged
 *
 * Performance:
 * - Time:  O(strlen(s))
 * - Space: O(1) — no allocation
 */
static inline bool stringbuf_append(StringBuf* sb, const char* s) {
    if (!sb || !sb->data) return false;
    if (!s) return true; /* NULL is a no-op */

    usize add_len = (usize)strlen(s);

    /* Overflow check: len + add_len + 1 must not exceed capacity */
    if (add_len > CANON_USIZE_MAX - sb->len - 1) return false;
    if (sb->len + add_len + 1 > sb->capacity)    return false;

    mem_copy(sb->data + sb->len, s, add_len);
    sb->len += add_len;
    sb->data[sb->len] = '\0';
    return true;
}

/**
 * @brief Appends a single character
 *
 * More efficient than stringbuf_append for single characters.
 *
 * @param sb StringBuf to append to
 * @param c  Character to append
 * @return true on success, false if sb is invalid or buffer full
 *
 * @post On true: c appended, sb->len incremented by 1
 * @post On false: sb is unchanged
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1) — no allocation
 */
static inline bool stringbuf_append_char(StringBuf* sb, char c) {
    if (!sb || !sb->data) return false;
    if (sb->len + 2 > sb->capacity) return false; /* char + '\0' */

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
 * @return true on success, false if sb or fmt is NULL, format error, or insufficient space
 *
 * @post On true: formatted text appended, sb->len increased accordingly
 * @post On false: sb is unchanged
 *
 * Performance:
 * - Time:  O(n) — two vsnprintf passes where n = formatted string length
 * - Space: O(1) — no allocation
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
    int written = vsnprintf(sb->data + sb->len, sb->capacity - sb->len, fmt, args);
    va_end(args);

    if (written < 0 || written != needed) return false;

    sb->len += (usize)written;
    return true;
}

/**
 * @brief Appends formatted text using an existing va_list
 *
 * Useful when forwarding variadic arguments from a wrapper function.
 * The caller is responsible for va_start/va_end on the original va_list.
 *
 * @param sb   StringBuf to append to
 * @param fmt  Format string
 * @param args va_list of arguments
 * @return true on success, false on failure
 *
 * @post On true: formatted text appended
 * @post On false: sb is unchanged
 *
 * Performance:
 * - Time:  O(n) — two vsnprintf passes
 * - Space: O(1) — no allocation
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

    int written = vsnprintf(sb->data + sb->len, sb->capacity - sb->len, fmt, args);
    if (written < 0 || written != needed) return false;

    sb->len += (usize)written;
    return true;
}

/**
 * @brief Appends at most n characters from string s
 *
 * Appends min(n, strlen(s)) characters — stops at null terminator or n,
 * whichever comes first.
 *
 * @param sb StringBuf to append to
 * @param s  Source string (NULL treated as empty — no-op, returns true)
 * @param n  Maximum number of characters to append
 * @return true on success, false if insufficient space
 *
 * @post On true: up to n chars appended, buffer null-terminated
 * @post On false: sb is unchanged
 *
 * Performance:
 * - Time:  O(min(n, strlen(s)))
 * - Space: O(1) — no allocation
 */
static inline bool stringbuf_append_n(StringBuf* sb, const char* s, usize n) {
    if (!sb || !sb->data) return false;
    if (!s || n == 0)     return true; /* no-op */

    usize actual_len = 0;
    while (actual_len < n && s[actual_len] != '\0') {
        actual_len++;
    }

    if (actual_len > CANON_USIZE_MAX - sb->len - 1) return false;
    if (sb->len + actual_len + 1 > sb->capacity)    return false;

    mem_copy(sb->data + sb->len, s, actual_len);
    sb->len += actual_len;
    sb->data[sb->len] = '\0';
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Access
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a pointer to the current null-terminated string (borrowed view)
 *
 * Always safe to call — returns "" on invalid or empty StringBuf.
 * Pointer is valid until the backing buffer is destroyed or the arena is reset.
 *
 * @param sb StringBuf to access (NULL-safe)
 * @return Pointer to null-terminated string — never NULL
 *
 * @note The returned pointer is borrowed — do not free it.
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline const char* stringbuf_str(const StringBuf* sb) {
    return (sb && sb->data) ? sb->data : "";
}

/* ════════════════════════════════════════════════════════════════════════════
   Query functions
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the current string length (excluding null terminator)
 *
 * @param sb StringBuf to query (NULL-safe)
 * @return usize — 0 if sb == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize stringbuf_len(const StringBuf* sb) {
    return sb ? sb->len : 0;
}

/**
 * @brief Returns the total buffer capacity (including null terminator byte)
 *
 * Usable characters = capacity - 1.
 *
 * @param sb StringBuf to query (NULL-safe)
 * @return usize — 0 if sb == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize stringbuf_capacity(const StringBuf* sb) {
    return sb ? sb->capacity : 0;
}

/**
 * @brief Returns the number of characters that can still be appended
 *
 * Equivalent to capacity - len - 1 (the -1 reserves space for '\0').
 *
 * @param sb StringBuf to query (NULL-safe)
 * @return usize — 0 if sb == NULL or buffer is full
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize stringbuf_remaining(const StringBuf* sb) {
    if (!sb || sb->capacity == 0) return 0;
    return (sb->capacity > sb->len + 1) ? (sb->capacity - sb->len - 1) : 0;
}

/**
 * @brief Returns true if the string is empty (len == 0)
 *
 * @param sb StringBuf to check (NULL-safe)
 * @return true if empty or NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool stringbuf_is_empty(const StringBuf* sb) {
    return !sb || sb->len == 0;
}

/**
 * @brief Returns true if no more characters can be appended
 *
 * @param sb StringBuf to check (NULL-safe)
 * @return true if full or NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool stringbuf_is_full(const StringBuf* sb) {
    return !sb || (sb->len + 1 >= sb->capacity);
}

/**
 * @brief Returns true if the StringBuf was allocated from an arena
 *
 * @param sb StringBuf to check (NULL-safe)
 * @return true if arena != NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool stringbuf_is_arena_backed(const StringBuf* sb) {
    return sb && sb->arena != NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
   Mutation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resets the string to empty without freeing the buffer
 *
 * @param sb StringBuf to clear (NULL-safe)
 *
 * @post sb->len == 0, sb->data[0] == '\0'
 * @post Buffer is NOT zeroed — only logical state is reset
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
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
 * Does nothing if new_len >= sb->len.
 *
 * @param sb      StringBuf to truncate (NULL-safe)
 * @param new_len New length — must be <= current length to have effect
 *
 * @post If new_len < sb->len: sb->len == new_len, sb->data[new_len] == '\0'
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void stringbuf_truncate(StringBuf* sb, usize new_len) {
    if (sb && sb->data && new_len < sb->len) {
        sb->len            = new_len;
        sb->data[sb->len]  = '\0';
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Convenience alias
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Alias for stringbuf_append_fmt — matches printf naming conventions
 *
 * Some users prefer the shorter name in heavy formatting code.
 * Both names compile to the same function call.
 *
 * Usage:
 * ```c
 * stringbuf_printf(&sb, "x=%d y=%d", x, y);
 * ```
 */
#define stringbuf_printf stringbuf_append_fmt

#endif /* CANON_DATA_STRINGBUF_H */
