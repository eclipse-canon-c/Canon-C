// data/stringbuf.h
#ifndef CANON_DATA_STRINGBUF_H
#define CANON_DATA_STRINGBUF_H

#include <stddef.h>
#include <stdarg.h>
#include "arena.h"
#include "util/string.h"  // for mem_copy, etc.

/**
 * @file stringbuf.h
 * @brief Fixed-capacity incremental string builder (arena-backed or caller-owned buffer)
 *
 * StringBuf provides efficient, safe string concatenation/formatting in a **fixed-size buffer**.
 * It is deliberately **NOT** a growable string like std::string, StringBuilder in C#, or Rust's String.
 *
 *                           KEY DESIGN PRINCIPLES
 * ──────────────────────────────────────────────────────────────────────────────
 *  • Capacity is **fixed** at initialization — **no automatic growth/reallocation**
 *  • Preferred usage: backed by Arena (buffer lives until arena reset)
 *  • Alternative: caller-owned fixed buffer (stack/static/heap)
 *  • All append operations return bool → fail gracefully when full
 *  • Always null-terminated (even when empty)
 *  • Zero hidden state, no global variables, minimal runtime overhead
 *  • Ideal for: formatting logs, building paths, JSON snippets, error messages...
 *    ... where you can reasonably predict/estimate maximum size upfront
 *
 *                          WHEN TO USE STRINGBUF
 * ──────────────────────────────────────────────────────────────────────────────
 * Use when you want:
 *   ✓ Predictable memory usage (no surprise allocations)
 *   ✓ Arena-friendly lifetime management
 *   ✓ Bounds-checked appends with clear failure mode
 *   ✓ Very low overhead
 *
 * Do NOT use when you want:
 *   ✗ Automatic growth / unlimited appends
 *   ✗ "I don't care about capacity" convenience
 *
 * In those cases consider:
 *   - Your own growable wrapper (using realloc or custom doubling strategy)
 *   - External libraries with dynamic growth
 *   - Building in multiple passes with temporary buffers
 *
 *                          IMPORTANT LIMITATIONS
 * ──────────────────────────────────────────────────────────────────────────────
 *  • Once full → all append operations **fail** (return false), string unchanged
 *  • No reserve/ensure_grow/append_with_realloc functions (by design)
 *  • You must estimate sufficient capacity **at initialization**
 *  • Arena-backed buffers become invalid after arena_reset() / arena_destroy()
 *
 * Typical safe usage patterns:
 *
 *   // 1. Arena-backed (most common & recommended)
 *   Arena temp = arena_create(64 * 1024);
 *   StringBuf sb;
 *   if (!stringbuf_init_arena(&sb, &temp, 4096)) { ... error ... }
 *   stringbuf_append_fmt(&sb, "User: %s, Score: %d", name, score);
 *   // sb.data valid until arena_reset(&temp) or arena_destroy(&temp)
 *
 *   // 2. Stack-allocated fixed buffer (zero allocation)
 *   char buf[512];
 *   StringBuf path;
 *   stringbuf_init_buffer(&path, buf, sizeof(buf));
 *   stringbuf_append(&path, "/home/user/docs/");
 *   stringbuf_append(&path, filename);
 *   // buf contains result, valid until end of scope
 */

typedef struct {
    Arena* arena;     ///< Arena used for allocation (NULL = fixed caller-owned buffer)
    char*  data;      ///< Pointer to buffer (always null-terminated when valid)
    size_t len;       ///< Current string length (excluding '\0')
    size_t capacity;  ///< Total buffer size **including** space for null terminator
} StringBuf;

/**
 * @brief Initializes StringBuf using Arena-allocated buffer (recommended)
 *
 * Allocates initial buffer from the given arena.
 * Fails if arena allocation fails or parameters invalid.
 *
 * @return true on success, false on failure (null ptrs / alloc fail / cap == 0)
 */
static inline bool stringbuf_init_arena(StringBuf* sb, Arena* arena, size_t initial_cap) {
    if (!sb || !arena || initial_cap == 0) return false;
    char* buf = arena_alloc(arena, initial_cap);
    if (!buf) return false;
    buf[0] = '\0';
    *sb = (StringBuf){
        .arena = arena,
        .data = buf,
        .len = 0,
        .capacity = initial_cap
    };
    return true;
}

/**
 * @brief Initializes StringBuf with a caller-provided fixed buffer
 *
 * Useful for stack/static buffers or when arena is not desired.
 * Caller must ensure buffer remains valid for the lifetime of StringBuf.
 */
static inline void stringbuf_init_buffer(StringBuf* sb, char* buffer, size_t cap) {
    if (sb && buffer && cap > 0) {
        buffer[0] = '\0';
        *sb = (StringBuf){
            .arena = NULL,
            .data = buffer,
            .len = 0,
            .capacity = cap
        };
    }
}

/**
 * @brief Appends a null-terminated string (safe, bounds-checked)
 *
 * @return true if successfully appended, false if:
 *   - invalid pointers
 *   - not enough remaining space (sb->len + strlen(s) + 1 > capacity)
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
 * @brief Appends formatted text (printf-style)
 *
 * Uses vsnprintf twice: first to measure, second to write.
 * Fails cleanly if format error or insufficient space.
 *
 * @return true on success, false on format error / insufficient capacity
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
 * @brief Returns the current null-terminated string (borrowed view)
 *
 * Always safe to call — returns "" on invalid/empty builder.
 * Pointer valid until arena reset (if arena-backed) or buffer destruction.
 */
static inline const char* stringbuf_str(const StringBuf* sb) {
    return sb && sb->data ? sb->data : "";
}

/**
 * @brief Current length of the string (excluding null terminator)
 */
static inline size_t stringbuf_len(const StringBuf* sb) {
    return sb ? sb->len : 0;
}

#endif /* CANON_DATA_STRINGBUF_H */
