#ifndef CANON_DATA_STRINGBUF_H
#define CANON_DATA_STRINGBUF_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/checked.h"
#include "core/memory.h"
#include "core/arena.h"
#include "core/slice.h"
#include "core/ownership.h"

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
 * Ownership model:
 * ────────────────────────────────────────────────────────────────────────────
 * StringBuf does not own its backing buffer. It borrows either a region of an
 * Arena (arena-backed) or a caller-owned char array (buffer-backed). All
 * pointer parameters are annotated with borrowed() from core/ownership.h to
 * make this explicit at every call site. The backing buffer must outlive every
 * StringBuf that views it.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * stringbuf.h is data/. It may include core/primitives/, core/, and semantics/.
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
 * va_list double-pass contract:
 * ────────────────────────────────────────────────────────────────────────────
 * Both stringbuf_append_fmt and stringbuf_append_fmt_va use two vsnprintf
 * passes (measure, then write). Each pass requires a fresh va_list:
 *
 * - stringbuf_append_fmt:    va_start / va_end for the measure pass;
 *                            va_start / va_end for the write pass.
 *                            Two independent va_start calls on the same __VA_ARGS__.
 *
 * - stringbuf_append_fmt_va: The caller-supplied args must not be consumed.
 *                            Both the measure pass and the write pass each
 *                            receive their own va_copy. The original args is
 *                            left intact for the caller's va_end. This is the
 *                            only portable pattern on ABIs where va_list is an
 *                            array type (e.g. AArch64 AAPCS64) and therefore
 *                            non-replayable after use.
 *
 * Overflow safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All length arithmetic uses checked_add() from core/primitives/checked.h.
 * No manual two-step overflow guards appear in this file.
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
 * @sa core/ownership.h   — borrowed() annotation used throughout
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
    borrowed(Arena*) arena;    ///< Backing arena (NULL = caller-owned buffer); not owned
    char*            data;     ///< Buffer pointer (always null-terminated when valid)
    usize            len;      ///< Current string length (excluding '\0')
    usize            capacity; ///< Total buffer size including space for '\0'
} StringBuf;

/* ════════════════════════════════════════════════════════════════════════════
   Constructors
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes StringBuf using an Arena-allocated buffer (recommended)
 *
 * @param sb          Pointer to uninitialized StringBuf
 * @param arena       Valid, initialized Arena (borrowed — StringBuf does not own it)
 * @param initial_cap Total capacity including null terminator (must be > 1)
 * @return true on success, false if the arena cannot satisfy the allocation
 *
 * @pre sb != NULL, arena != NULL, initial_cap > 1
 *
 * @post On success: sb->data[0] == '\0', sb->len == 0, sb->capacity == initial_cap
 *
 * Performance:
 * - Time:  O(1) — single arena bump
 * - Space: O(initial_cap) consumed from arena
 */
static inline bool stringbuf_init_arena(
        borrowed(StringBuf*) sb,
        borrowed(Arena*)     arena,
        usize                initial_cap)
{
    require_msg(sb != NULL,      "stringbuf_init_arena: sb cannot be NULL");
    require_msg(arena != NULL,   "stringbuf_init_arena: arena cannot be NULL");
    require_msg(initial_cap > 1, "stringbuf_init_arena: initial_cap must be > 1");

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
 * @param buffer Pointer to char buffer (borrowed — must remain valid for StringBuf lifetime)
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
static inline void stringbuf_init_buffer(
        borrowed(StringBuf*) sb,
        borrowed(char*)      buffer,
        usize                cap)
{
    require_msg(sb != NULL,     "stringbuf_init_buffer: sb cannot be NULL");
    require_msg(buffer != NULL, "stringbuf_init_buffer: buffer cannot be NULL");
    require_msg(cap > 1,        "stringbuf_init_buffer: cap must be > 1");

    buffer[0] = '\0';
    *sb = (StringBuf){
        .arena    = NULL,
        .data     = buffer,
        .len      = 0,
        .capacity = cap
    };
}

/* ════════════════════════════════════════════════════════════════════════════
   Internal: common append-bytes helper
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Core append implementation — copies add_len bytes from src into sb.
 *
 * Called by all append variants after they have resolved src and add_len.
 * Not part of the public API — internal linkage only.
 *
 * Overflow safety:
 *   checked_add(sb->len, add_len, &end) guards sb->len + add_len.
 *   checked_add(end, 1u, &with_nul)     guards the +1 for the null terminator.
 *   Both must succeed and with_nul must not exceed capacity.
 *
 * @pre sb != NULL && sb->data != NULL
 * @pre src != NULL
 * @pre add_len > 0
 * @pre [src, src+add_len) does not overlap sb->data (caller must guarantee)
 */
static inline bool _stringbuf_append_bytes(
        borrowed(StringBuf*)  sb,
        borrowed(const char*) src,
        usize                 add_len)
{
    usize end;
    usize with_nul;

    if (!checked_add(sb->len, add_len, &end))   return false;
    if (!checked_add(end, 1u, &with_nul))        return false;
    if (with_nul > sb->capacity)                 return false;

    mem_copy((u8*)sb->data + sb->len, src, add_len);
    sb->len = end;
    sb->data[sb->len] = '\0';
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Append operations
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Appends a null-terminated C string (bounds-checked)
 *
 * Prefer stringbuf_append_str() at call sites that already hold a str_t
 * to avoid the strlen scan.
 *
 * @param sb StringBuf to append to
 * @param s  Null-terminated string to append (NULL = no-op, returns true)
 * @return true on success, false if insufficient space
 *
 * Performance: O(strlen(s))
 */
static inline bool stringbuf_append(
        borrowed(StringBuf*)  sb,
        borrowed(const char*) s)
{
    if (!sb || !sb->data) return false;
    if (!s) return true;
    return _stringbuf_append_bytes(sb, s, (usize)strlen(s));
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
static inline bool stringbuf_append_str(
        borrowed(StringBuf*) sb,
        str_t                s)
{
    if (!sb || !sb->data)     return false;
    if (!s.ptr || s.len == 0) return true;
    return _stringbuf_append_bytes(sb, s.ptr, s.len);
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
static inline bool stringbuf_append_char(
        borrowed(StringBuf*) sb,
        char                 c)
{
    usize with_nul;

    if (!sb || !sb->data) return false;
    /* Need room for c plus the null terminator: len + 2 <= capacity.
     * checked_add guards against len already being at USIZE_MAX. */
    if (!checked_add(sb->len, 2u, &with_nul)) return false;
    if (with_nul > sb->capacity)              return false;

    sb->data[sb->len++] = c;
    sb->data[sb->len]   = '\0';
    return true;
}

/**
 * @brief Appends formatted text (printf-style)
 *
 * Uses two independent vsnprintf passes (measure then write), each with its
 * own va_start / va_end pair. This is the only portable pattern for replaying
 * variadic arguments across all C99 ABIs.
 *
 * On failure the buffer is unchanged and remains null-terminated.
 *
 * @param sb  StringBuf to append to
 * @param fmt printf-style format string
 * @param ... Format arguments
 * @return true on success, false on failure or insufficient space
 *
 * Performance: O(n) — two vsnprintf passes
 */
static inline bool stringbuf_append_fmt(
        borrowed(StringBuf*)  sb,
        borrowed(const char*) fmt,
        ...)
{
    va_list args;
    int     needed_i;
    usize   needed;
    usize   end;
    usize   with_nul;
    int     written;

    if (!sb || !fmt || !sb->data) return false;

    /* Pass 1: measure */
    va_start(args, fmt);
    needed_i = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed_i < 0) return false;
    needed = (usize)needed_i;

    if (!checked_add(sb->len, needed, &end))  return false;
    if (!checked_add(end, 1u, &with_nul))      return false;
    if (with_nul > sb->capacity)               return false;

    /* Pass 2: write — fresh va_start required; prior va_end consumed args */
    va_start(args, fmt);
    written = vsnprintf(
        (char*)((u8*)sb->data + sb->len),
        sb->capacity - sb->len,
        fmt, args);
    va_end(args);

    if (written < 0 || (usize)written != needed) return false;
    sb->len = end;
    return true;
}

/**
 * @brief Appends formatted text using an existing va_list
 *
 * Both the measure pass and the write pass receive their own va_copy so
 * that the original args is never consumed. The caller retains full
 * responsibility for va_end(args) as normal.
 *
 * va_list portability:
 *   On ABIs where va_list is an array type (e.g. AArch64 AAPCS64), a va_list
 *   cannot be replayed after use — even by value assignment. va_copy is the
 *   only portable solution. Both passes use independent va_copy instances;
 *   the original args is left intact throughout.
 *
 * @param sb   StringBuf to append to
 * @param fmt  Format string
 * @param args va_list of arguments (caller manages va_start/va_end)
 * @return true on success, false on failure
 *
 * Performance: O(n) — two vsnprintf passes via va_copy
 */
static inline bool stringbuf_append_fmt_va(
        borrowed(StringBuf*)  sb,
        borrowed(const char*) fmt,
        va_list               args)
{
    va_list measure_args;
    va_list write_args;
    int     needed_i;
    usize   needed;
    usize   end;
    usize   with_nul;
    int     written;

    if (!sb || !fmt || !sb->data) return false;

    /* Pass 1: measure — independent copy so original args is not consumed */
    va_copy(measure_args, args);
    needed_i = vsnprintf(NULL, 0, fmt, measure_args);
    va_end(measure_args);

    if (needed_i < 0) return false;
    needed = (usize)needed_i;

    if (!checked_add(sb->len, needed, &end))  return false;
    if (!checked_add(end, 1u, &with_nul))      return false;
    if (with_nul > sb->capacity)               return false;

    /* Pass 2: write — second independent copy, original args still intact */
    va_copy(write_args, args);
    written = vsnprintf(
        (char*)((u8*)sb->data + sb->len),
        sb->capacity - sb->len,
        fmt, write_args);
    va_end(write_args);

    if (written < 0 || (usize)written != needed) return false;
    sb->len = end;
    return true;
}

/**
 * @brief Appends at most n characters from string s
 *
 * Appends min(n, strlen(s)) characters. Stops at the first null byte
 * in s or after n characters, whichever comes first.
 *
 * @param sb StringBuf to append to
 * @param s  Source string (NULL = no-op, returns true)
 * @param n  Maximum number of characters to append
 * @return true on success, false if insufficient space
 *
 * Performance: O(min(n, strlen(s)))
 */
static inline bool stringbuf_append_n(
        borrowed(StringBuf*)  sb,
        borrowed(const char*) s,
        usize                 n)
{
    usize actual_len;

    if (!sb || !sb->data) return false;
    if (!s || n == 0)     return true;

    /* Determine how many bytes to actually copy: stop at null or n */
    actual_len = 0;
    while (actual_len < n && s[actual_len] != '\0') actual_len++;

    if (actual_len == 0) return true;
    return _stringbuf_append_bytes(sb, s, actual_len);
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
 * @note Do NOT free the returned pointer — it is borrowed from the StringBuf.
 *
 * Performance: O(1)
 */
static inline borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb) {
    return (sb && sb->data) ? sb->data : "";
}

/**
 * @brief Returns a str_t non-owning view over the current string contents
 *
 * The returned str_t is valid until the backing buffer is destroyed,
 * the arena is reset, or the StringBuf is cleared/truncated.
 * Do NOT free str_t.ptr — it is borrowed from the StringBuf's backing buffer.
 *
 * @param sb StringBuf to view
 * @return str_t — empty str_t if sb == NULL or data == NULL
 *
 * Performance: O(1)
 */
static inline str_t stringbuf_as_str(borrowed(const StringBuf*) sb) {
    if (!sb || !sb->data) return str_empty();
    return str_from(sb->data, sb->len);
}

/**
 * @brief Returns a bytes_t non-owning view over the string's raw bytes
 *
 * Covers [data, data + len) — does NOT include the null terminator byte.
 * Useful for passing StringBuf contents to byte-level APIs.
 *
 * @param sb StringBuf to view
 * @return bytes_t — empty bytes_t if sb == NULL or data == NULL
 *
 * Performance: O(1)
 */
static inline bytes_t stringbuf_as_bytes(borrowed(const StringBuf*) sb) {
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
static inline bytes_t stringbuf_buffer_bytes(borrowed(const StringBuf*) sb) {
    if (!sb || !sb->data) return bytes_empty();
    return bytes_from(sb->data, sb->capacity);
}

/* ════════════════════════════════════════════════════════════════════════════
   Query
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Returns current string length (excluding '\0'). NULL-safe. O(1) */
static inline usize stringbuf_len(borrowed(const StringBuf*) sb) {
    return sb ? sb->len : 0;
}

/** @brief Returns total buffer capacity (including '\0' byte). NULL-safe. O(1) */
static inline usize stringbuf_capacity(borrowed(const StringBuf*) sb) {
    return sb ? sb->capacity : 0;
}

/**
 * @brief Returns number of characters still appendable. NULL-safe. O(1)
 *
 * Equivalent to capacity - len - 1. Returns 0 if full or sb == NULL.
 * Uses checked_sub to guard against len + 1 exceeding capacity without
 * wrapping.
 */
static inline usize stringbuf_remaining(borrowed(const StringBuf*) sb) {
    usize usable;
    if (!sb || sb->capacity == 0) return 0;
    /* capacity - (len + 1): the -1 reserves the null terminator slot.
     * checked_sub returns false (→ 0) when len + 1 >= capacity, i.e. full. */
    if (!checked_sub(sb->capacity, sb->len + 1u, &usable)) return 0;
    return usable;
}

/** @brief Returns true if string is empty (len == 0). NULL-safe. O(1) */
static inline bool stringbuf_is_empty(borrowed(const StringBuf*) sb) {
    return !sb || sb->len == 0;
}

/**
 * @brief Returns true if no more characters can be appended. NULL-safe. O(1)
 *
 * Full when len + 1 >= capacity, i.e. only the null terminator slot remains.
 * Equivalent to stringbuf_remaining() == 0.
 */
static inline bool stringbuf_is_full(borrowed(const StringBuf*) sb) {
    return !sb || (sb->len + 1u >= sb->capacity);
}

/** @brief Returns true if buffer was allocated from an arena. NULL-safe. O(1) */
static inline bool stringbuf_is_arena_backed(borrowed(const StringBuf*) sb) {
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
static inline void stringbuf_clear(borrowed(StringBuf*) sb) {
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
static inline void stringbuf_truncate(borrowed(StringBuf*) sb, usize new_len) {
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
