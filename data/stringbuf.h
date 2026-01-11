// data/stringbuf.h
#ifndef CANON_DATA_STRINGBUF_H
#define CANON_DATA_STRINGBUF_H

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "core/arena.h"
#include "core/memory.h"

/**
 * @file stringbuf.h
 * @brief Fixed-capacity incremental string builder (arena-backed or caller-owned buffer)
 *
 * StringBuf provides efficient, safe string concatenation/formatting in a **fixed-size buffer**.
 * It is deliberately **NOT** a growable string like std::string, StringBuilder in C#, or Rust's String.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, variadic macros)
 *   - Depends on arena.h and memory.h from this library
 *   - Uses standard library: strlen, vsnprintf, snprintf
 *   - No platform-specific code
 *
 * Thread-safety: Each StringBuf instance is independent - not thread-safe for
 *                concurrent modifications. Caller must synchronize if needed.
 *
 * Performance:
 *   - Zero overhead abstraction - direct buffer manipulation
 *   - All append operations are O(n) where n is the appended string length
 *   - No hidden allocations after initialization
 *   - Cache-friendly contiguous storage
 *
 *                           KEY DESIGN PRINCIPLES
 * ──────────────────────────────────────────────────────────────────────────────
 *  • Capacity is **fixed** at initialization — **no automatic growth/reallocation**
 *  • Preferred usage: backed by Arena (buffer lives until arena reset)
 *  • Alternative: caller-owned fixed buffer (stack/static/heap)
 *  • All append operations return bool → fail gracefully when full
 *  • Always null-terminated (even when empty or after failed append)
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
 *   ✓ Known or bounded string length
 *   ✓ Format strings without sprintf buffer overflow risks
 *
 * Do NOT use when you want:
 *   ✗ Automatic growth / unlimited appends
 *   ✗ "I don't care about capacity" convenience
 *   ✗ Dynamic resizing based on usage
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
 *  • Arena-backed buffers become invalid after arena_reset() / arena destruction
 *  • Buffer must have room for null terminator (capacity includes the '\0')
 *
 * Typical safe usage patterns:
 *
 *   // 1. Arena-backed (most common & recommended)
 *   uint8_t arena_buf[64 * 1024];
 *   Arena temp;
 *   arena_init(&temp, arena_buf, sizeof(arena_buf));
 *   
 *   StringBuf sb;
 *   if (!stringbuf_init_arena(&sb, &temp, 4096)) { 
 *       // Handle error
 *   }
 *   stringbuf_append_fmt(&sb, "User: %s, Score: %d", name, score);
 *   // sb.data valid until arena_reset(&temp)
 *
 *   // 2. Stack-allocated fixed buffer (zero allocation)
 *   char buf[512];
 *   StringBuf path;
 *   stringbuf_init_buffer(&path, buf, sizeof(buf));
 *   stringbuf_append(&path, "/home/user/docs/");
 *   stringbuf_append(&path, filename);
 *   // buf contains result, valid until end of scope
 *
 *   // 3. Error handling
 *   if (!stringbuf_append(&sb, long_string)) {
 *       fprintf(stderr, "Buffer full, %zu/%zu used\n",
 *               stringbuf_len(&sb), stringbuf_capacity(&sb));
 *   }
 */

/**
 * @brief String builder with fixed capacity
 *
 * Maintains a null-terminated string in a fixed-size buffer.
 * Can be backed by an Arena or a caller-provided buffer.
 *
 * Fields:
 *   - arena: Arena used for allocation (NULL if using caller-owned buffer)
 *   - data: Pointer to buffer (always null-terminated when valid)
 *   - len: Current string length (excluding '\0')
 *   - capacity: Total buffer size **including** space for null terminator
 *
 * Invariants:
 *   - data[len] == '\0' (always null-terminated)
 *   - len < capacity (always room for null terminator)
 *   - If arena != NULL, data was allocated from that arena
 *
 * Do not access fields directly - use the provided functions.
 */
typedef struct {
    Arena* arena;     ///< Arena used for allocation (NULL = caller-owned buffer)
    char*  data;      ///< Pointer to buffer (always null-terminated when valid)
    size_t len;       ///< Current string length (excluding '\0')
    size_t capacity;  ///< Total buffer size **including** space for null terminator
} StringBuf;

/**
 * @brief Initializes StringBuf using Arena-allocated buffer (recommended)
 *
 * Allocates initial buffer from the given arena.
 * The buffer will be valid until the arena is reset or destroyed.
 *
 * @param sb          Pointer to uninitialized StringBuf
 * @param arena       Valid, initialized Arena with sufficient space
 * @param initial_cap Total capacity including null terminator (must be > 0)
 * @return            true on success, false on failure
 *
 * Preconditions:
 *   - sb != NULL
 *   - arena != NULL and initialized
 *   - initial_cap > 0
 *   - arena has at least initial_cap bytes remaining
 *
 * Postconditions on success:
 *   - sb is initialized with empty string
 *   - sb->data is allocated from arena
 *   - sb->data[0] == '\0'
 *   - sb->len == 0
 *   - sb->capacity == initial_cap
 *
 * Postconditions on failure:
 *   - sb state is undefined (do not use)
 *
 * Returns false if:
 *   - Any parameter is NULL
 *   - initial_cap is 0
 *   - Arena allocation fails (insufficient space)
 *
 * Example:
 *   StringBuf sb;
 *   if (!stringbuf_init_arena(&sb, &arena, 1024)) {
 *       fprintf(stderr, "Failed to allocate string buffer\n");
 *       return ERROR;
 *   }
 */
static inline bool stringbuf_init_arena(StringBuf* sb, Arena* arena, size_t initial_cap) {
    assert(sb != NULL && "stringbuf_init_arena: sb parameter cannot be NULL");
    assert(arena != NULL && "stringbuf_init_arena: arena parameter cannot be NULL");
    assert(initial_cap > 0 && "stringbuf_init_arena: initial_cap must be greater than 0");
    
    if (!sb || !arena || initial_cap == 0) {
        return false;
    }
    
    char* buf = (char*)arena_alloc(arena, initial_cap);
    if (!buf) {
        return false;
    }
    
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
 *
 * @param sb     Pointer to uninitialized StringBuf
 * @param buffer Pointer to buffer (must remain valid)
 * @param cap    Total capacity including null terminator (must be > 0)
 *
 * Preconditions:
 *   - sb != NULL
 *   - buffer != NULL
 *   - cap > 0
 *   - buffer has at least cap bytes
 *
 * Postconditions:
 *   - sb is initialized with empty string
 *   - sb->data points to buffer
 *   - sb->data[0] == '\0'
 *   - sb->len == 0
 *   - sb->capacity == cap
 *   - sb->arena == NULL (not arena-backed)
 *
 * Warning: If buffer becomes invalid (e.g., stack variable goes out of scope),
 *          the StringBuf becomes invalid. Caller must manage lifetime.
 *
 * Example:
 *   char buf[256];
 *   StringBuf path;
 *   stringbuf_init_buffer(&path, buf, sizeof(buf));
 */
static inline void stringbuf_init_buffer(StringBuf* sb, char* buffer, size_t cap) {
    assert(sb != NULL && "stringbuf_init_buffer: sb parameter cannot be NULL");
    assert(buffer != NULL && "stringbuf_init_buffer: buffer parameter cannot be NULL");
    assert(cap > 0 && "stringbuf_init_buffer: cap must be greater than 0");
    
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
 * Copies the string to the end of the buffer if space is available.
 * On failure, the buffer is unchanged and remains null-terminated.
 *
 * @param sb StringBuf to append to
 * @param s  String to append (NULL-safe, treated as empty string)
 * @return   true if successfully appended, false otherwise
 *
 * Returns false if:
 *   - sb is NULL or invalid
 *   - Not enough remaining space (len + strlen(s) + 1 > capacity)
 *
 * Performance: O(strlen(s)) - copies string once
 *
 * Example:
 *   if (!stringbuf_append(&sb, "Hello, ")) {
 *       fprintf(stderr, "Append failed - buffer full\n");
 *   }
 *   stringbuf_append(&sb, "World!");
 */
static inline bool stringbuf_append(StringBuf* sb, const char* s) {
    if (!sb || !sb->data) {
        return false;
    }
    
    if (!s) {
        return true;  // Appending NULL is a no-op (success)
    }
    
    size_t add_len = strlen(s);
    
    // Check for overflow in addition
    if (add_len > SIZE_MAX - sb->len - 1) {
        return false;
    }
    
    if (sb->len + add_len + 1 > sb->capacity) {
        return false;
    }
    
    mem_copy(sb->data + sb->len, s, add_len);
    sb->len += add_len;
    sb->data[sb->len] = '\0';
    
    return true;
}

/**
 * @brief Appends a single character
 *
 * More efficient than appending a 1-character string.
 *
 * @param sb StringBuf to append to
 * @param c  Character to append
 * @return   true if successfully appended, false if buffer full
 *
 * Example:
 *   stringbuf_append_char(&sb, '\n');
 */
static inline bool stringbuf_append_char(StringBuf* sb, char c) {
    if (!sb || !sb->data) {
        return false;
    }
    
    if (sb->len + 2 > sb->capacity) {
        return false;  // Need room for char + '\0'
    }
    
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
    
    return true;
}

/**
 * @brief Appends formatted text (printf-style)
 *
 * Uses vsnprintf to format and append text.
 * On failure, the buffer is unchanged and remains null-terminated.
 *
 * @param sb  StringBuf to append to
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 * @return    true on success, false on format error or insufficient capacity
 *
 * Returns false if:
 *   - sb or fmt is NULL
 *   - Format string is invalid (vsnprintf error)
 *   - Insufficient space for formatted result
 *
 * Performance: Uses vsnprintf twice - once to measure, once to write
 *
 * Example:
 *   if (!stringbuf_append_fmt(&sb, "Score: %d/%d (%.1f%%)", 
 *                             score, max, percentage)) {
 *       fprintf(stderr, "Format failed or buffer full\n");
 *   }
 */
static inline bool stringbuf_append_fmt(StringBuf* sb, const char* fmt, ...) {
    if (!sb || !fmt || !sb->data) {
        return false;
    }
    
    va_list args;
    
    // First pass: measure how much space we need
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (needed < 0) {
        return false;  // Format error
    }
    
    // Check for overflow and capacity
    if ((size_t)needed > SIZE_MAX - sb->len - 1 ||
        sb->len + (size_t)needed + 1 > sb->capacity) {
        return false;
    }
    
    // Second pass: actually write the formatted string
    va_start(args, fmt);
    int written = vsnprintf(sb->data + sb->len, sb->capacity - sb->len, fmt, args);
    va_end(args);
    
    if (written < 0 || written != needed) {
        return false;  // Shouldn't happen, but be safe
    }
    
    sb->len += (size_t)written;
    
    return true;
}

/**
 * @brief Appends a substring (first n characters)
 *
 * Safely appends at most n characters from string s.
 * Automatically handles cases where n > strlen(s).
 *
 * @param sb StringBuf to append to
 * @param s  String to append from (NULL-safe)
 * @param n  Maximum number of characters to append
 * @return   true if successfully appended, false if buffer full
 *
 * Example:
 *   stringbuf_append_n(&sb, long_string, 10);  // Append first 10 chars
 */
static inline bool stringbuf_append_n(StringBuf* sb, const char* s, size_t n) {
    if (!sb || !sb->data) {
        return false;
    }
    
    if (!s || n == 0) {
        return true;  // No-op
    }
    
    // Find actual length to copy (min of n and strlen(s))
    size_t actual_len = 0;
    while (actual_len < n && s[actual_len] != '\0') {
        actual_len++;
    }
    
    // Check for overflow and capacity
    if (actual_len > SIZE_MAX - sb->len - 1 ||
        sb->len + actual_len + 1 > sb->capacity) {
        return false;
    }
    
    mem_copy(sb->data + sb->len, s, actual_len);
    sb->len += actual_len;
    sb->data[sb->len] = '\0';
    
    return true;
}

/**
 * @brief Returns the current null-terminated string (borrowed view)
 *
 * Always safe to call — returns "" on invalid/empty builder.
 * Pointer valid until arena reset (if arena-backed) or buffer destruction.
 *
 * @param sb StringBuf to access (NULL-safe)
 * @return   Pointer to null-terminated string, never NULL
 *
 * Note: The returned pointer is borrowed - do not free it.
 *       It becomes invalid if the StringBuf or backing buffer is destroyed.
 *
 * Example:
 *   printf("Result: %s\n", stringbuf_str(&sb));
 */
static inline const char* stringbuf_str(const StringBuf* sb) {
    return (sb && sb->data) ? sb->data : "";
}

/**
 * @brief Current length of the string (excluding null terminator)
 *
 * @param sb StringBuf to query (NULL-safe)
 * @return   Number of characters in string (not counting '\0')
 *
 * Example:
 *   printf("Length: %zu\n", stringbuf_len(&sb));
 */
static inline size_t stringbuf_len(const StringBuf* sb) {
    return sb ? sb->len : 0;
}

/**
 * @brief Total capacity including null terminator
 *
 * @param sb StringBuf to query (NULL-safe)
 * @return   Total buffer size in bytes
 *
 * Example:
 *   printf("Using %zu/%zu bytes\n", stringbuf_len(&sb), 
 *                                    stringbuf_capacity(&sb));
 */
static inline size_t stringbuf_capacity(const StringBuf* sb) {
    return sb ? sb->capacity : 0;
}

/**
 * @brief Number of characters that can still be appended
 *
 * @param sb StringBuf to query (NULL-safe)
 * @return   Number of characters of free space (excluding '\0')
 *
 * Example:
 *   if (stringbuf_remaining(&sb) < 100) {
 *       fprintf(stderr, "Warning: buffer nearly full\n");
 *   }
 */
static inline size_t stringbuf_remaining(const StringBuf* sb) {
    if (!sb || sb->capacity == 0) {
        return 0;
    }
    // capacity - 1 for null terminator, - len for used space
    return (sb->capacity > sb->len + 1) ? (sb->capacity - sb->len - 1) : 0;
}

/**
 * @brief Checks if buffer is empty
 *
 * @param sb StringBuf to check (NULL-safe)
 * @return   true if length is 0, false otherwise
 */
static inline bool stringbuf_is_empty(const StringBuf* sb) {
    return !sb || sb->len == 0;
}

/**
 * @brief Checks if buffer is full (no space for more characters)
 *
 * @param sb StringBuf to check (NULL-safe)
 * @return   true if no space remaining, false otherwise
 */
static inline bool stringbuf_is_full(const StringBuf* sb) {
    return sb && (sb->len + 1 >= sb->capacity);
}

/**
 * @brief Clears the string (sets length to 0)
 *
 * Resets to empty string without deallocating buffer.
 * O(1) operation.
 *
 * @param sb StringBuf to clear
 *
 * Example:
 *   stringbuf_clear(&sb);  // Reuse buffer for new string
 */
static inline void stringbuf_clear(StringBuf* sb) {
    if (sb && sb->data) {
        sb->data[0] = '\0';
        sb->len = 0;
    }
}

/**
 * @brief Truncates string to specified length
 *
 * If new_len >= current length, does nothing.
 * If new_len < current length, truncates and maintains null termination.
 *
 * @param sb      StringBuf to truncate
 * @param new_len New length (must be <= current length)
 *
 * Example:
 *   stringbuf_truncate(&sb, 10);  // Keep only first 10 characters
 */
static inline void stringbuf_truncate(StringBuf* sb, size_t new_len) {
    if (sb && sb->data && new_len < sb->len) {
        sb->len = new_len;
        sb->data[sb->len] = '\0';
    }
}

/**
 * @brief Checks if StringBuf is arena-backed
 *
 * @param sb StringBuf to check (NULL-safe)
 * @return   true if backed by arena, false if caller-owned buffer
 */
static inline bool stringbuf_is_arena_backed(const StringBuf* sb) {
    return sb && sb->arena != NULL;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────

    #include "stringbuf.h"
    #include <stdio.h>
    
    void example_arena_backed(void) {
        // Create arena
        uint8_t arena_buf[4096];
        Arena arena;
        arena_init(&arena, arena_buf, sizeof(arena_buf));
        
        // Initialize StringBuf
        StringBuf log;
        if (!stringbuf_init_arena(&log, &arena, 512)) {
            fprintf(stderr, "Failed to allocate string buffer\n");
            return;
        }
        
        // Build string
        stringbuf_append(&log, "[INFO] ");
        stringbuf_append(&log, "Application started");
        stringbuf_append_char(&log, '\n');
        stringbuf_append_fmt(&log, "PID: %d, Version: %s", getpid(), "1.0.0");
        
        printf("%s\n", stringbuf_str(&log));
        
        // Check capacity
        printf("Used %zu/%zu bytes\n", 
               stringbuf_len(&log), 
               stringbuf_capacity(&log));
        
        // Buffer valid until arena reset
        arena_reset(&arena);
    }
    
    void example_stack_buffer(void) {
        char buf[256];
        StringBuf path;
        stringbuf_init_buffer(&path, buf, sizeof(buf));
        
        const char* home = "/home/user";
        const char* file = "document.txt";
        
        stringbuf_append(&path, home);
        stringbuf_append(&path, "/");
        stringbuf_append(&path, file);
        
        printf("Path: %s\n", stringbuf_str(&path));
    }
    
    void example_error_handling(void) {
        char small_buf[16];
        StringBuf sb;
        stringbuf_init_buffer(&sb, small_buf, sizeof(small_buf));
        
        if (!stringbuf_append(&sb, "This is a very long string")) {
            fprintf(stderr, "Append failed - buffer full\n");
            fprintf(stderr, "Used %zu/%zu bytes\n",
                    stringbuf_len(&sb),
                    stringbuf_capacity(&sb));
        }
        
        // Partial content is still valid and null-terminated
        printf("Partial: %s\n", stringbuf_str(&sb));
    }
    
    void example_formatted_output(void) {
        char buf[128];
        StringBuf report;
        stringbuf_init_buffer(&report, buf, sizeof(buf));
        
        int score = 95;
        int max_score = 100;
        float percentage = (float)score / max_score * 100.0f;
        
        stringbuf_append(&report, "Test Results:\n");
        stringbuf_append_fmt(&report, "Score: %d/%d\n", score, max_score);
        stringbuf_append_fmt(&report, "Grade: %.1f%%", percentage);
        
        printf("%s\n", stringbuf_str(&report));
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_DATA_STRINGBUF_H */
