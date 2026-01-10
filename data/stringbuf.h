// data/stringbuf.h
#ifndef CANON_DATA_STRINGBUF_H
#define CANON_DATA_STRINGBUF_H

#include <stddef.h>
#include <stdarg.h>
#include "arena.h"
#include "util/string.h"   // for mem_copy, etc.

/**
 * @file stringbuf.h
 * @brief Incremental string builder with arena-backed or fixed-buffer storage
 *
 * StringBuf allows efficient appending of strings and formatted text
 * without repeated allocations or reallocations.
 *
 * Key properties:
 *   - Fixed capacity (no automatic growth)
 *   - Arena-backed (preferred) or fixed buffer
 *   - Allocated memory lives until arena reset
 *   - No ownership transfer — caller manages lifetime
 *   - Null-safe and bounds-checked operations
 *
 * Recommended usage:
 *   - Use arena-backed init for dynamic sizes
 *   - Use fixed buffer for static/known sizes
 *   - Always check return values (bool) for success
 *
 * Example (arena-backed):
 *   StringBuf sb;
 *   if (stringbuf_init_arena(&sb, &my_arena, 1024)) {
 *       stringbuf_append_fmt(&sb, "Hello %s!", "world");
 *       printf("%s\n", stringbuf_str(&sb));
 *   }
 */

/**
 * @brief Incremental string builder structure
 *
 * Holds a growing null-terminated string in a fixed-capacity buffer.
 * Can be backed by an Arena or a static/fixed buffer.
 */
typedef struct {
    Arena* arena;      ///< Arena used for allocation (NULL if fixed buffer)
    char*  data;       ///< Pointer to the string buffer (always null-terminated)
    size_t len;        ///< Current length of the string (excluding null terminator)
    size_t capacity;   ///< Total buffer size (including space for null terminator)
} StringBuf;

/**
 * @brief Initializes StringBuf using an Arena for allocation
 *
 * Preferred method — allocates initial buffer from arena.
 *
 * @param sb           Pointer to uninitialized StringBuf
 * @param arena        Valid initialized Arena
 * @param initial_cap  Initial capacity in bytes (should be > 0)
 * @return             true on success, false on failure (null pointers or alloc fail)
 *
 * Postconditions:
 *   - If success: sb->data is null-terminated empty string
 *   - sb->arena points to the provided arena
 */
static inline bool stringbuf_init_arena(StringBuf* sb, Arena* arena, size_t initial_cap) {
    if (!sb || !arena || initial_cap == 0) return false;

    char* buf = arena_alloc(arena, initial_cap);
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
 * @brief Initializes StringBuf with a caller-provided fixed buffer
 *
 * Fallback method when arena is not desired or available.
 *
 * @param sb      Pointer to uninitialized StringBuf
 * @param buffer  Caller-owned buffer (must remain valid)
 * @param cap     Buffer size in bytes (> 0)
 *
 * Preconditions:
 *   - buffer must be writable and large enough
 *   - cap > 0
 */
static inline void stringbuf_init_buffer(StringBuf* sb, char* buffer, size_t cap) {
    if (sb && buffer && cap > 0) {
        buffer[0] = '\0';
        *sb = (StringBuf){
            .arena    = NULL,
            .data     = buffer,
            .len      = 0,
            .capacity = cap
        };
    }
}

/**
 * @brief Appends a raw C-string to the buffer
 *
 * @param sb  Valid initialized StringBuf
 * @param s   Null-terminated string to append
 * @return    true if appended successfully, false if:
 *            - null pointers
 *            - not enough remaining capacity
 */
static inline bool stringbuf_append(StringBuf* sb, const char* s) {
    if (!sb || !s || !sb->data) return false;

    size_t add_len = strlen(s);
    if (sb->len + add_len + 1 > sb->capacity) return false;

    mem_copy(sb->data + sb->len, s, add_len + 1);
    sb->len += add_len;
    return true;
}

/**
 * @brief Appends formatted text (like sprintf)
 *
 * @param sb   Valid initialized StringBuf
 * @param fmt  printf-style format string
 * @param ...  Format arguments
 * @return     true on success, false if:
 *             - null pointers
 *             - format error
 *             - insufficient remaining capacity
 */
static inline bool stringbuf_append_fmt(StringBuf* sb, const char* fmt, ...) {
    if (!sb || !fmt || !sb->data) return false;

    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0 || sb->len + (size_t)needed + 1 > sb->capacity) return false;

    va_start(args, fmt);
    vsnprintf(sb->data + sb->len, sb->capacity - sb->len, fmt, args);
    va_end(args);

    sb->len += (size_t)needed;
    return true;
}

/**
 * @brief Returns the current built string (null-terminated, borrowed)
 *
 * @param sb Valid StringBuf
 * @return   Pointer to null-terminated string or "" if invalid
 *
 * Note: The returned pointer is valid until arena reset or buffer destruction
 */
static inline const char* stringbuf_str(const StringBuf* sb) {
    return sb && sb->data ? sb->data : "";
}

/**
 * @brief Returns current length of the string (excluding null terminator)
 */
static inline size_t stringbuf_len(const StringBuf* sb) {
    return sb ? sb->len : 0;
}

#endif /* CANON_DATA_STRINGBUF_H */
