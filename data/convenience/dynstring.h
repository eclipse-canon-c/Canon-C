// data/convenience/dynstring.h
#ifndef CANON_DATA_CONVENIENCE_DYNSTRING_H
#define CANON_DATA_CONVENIENCE_DYNSTRING_H

/**
 * @file convenience/dynstring.h
 * @brief Auto-growing string builder with hidden allocation
 *
 * CONVENIENCE LAYER - trades explicitness for ergonomics
 *
 * Differences from data/stringbuf.h:
 * ────────────────────────────────────
 * - Automatic heap allocation (hidden)
 * - Automatic growth on overflow (implicit)
 * - Owns its own memory (no caller-provided buffers)
 * - Simplified API (fewer choices to make)
 *
 * When to use this:
 * ─────────────────
 * ✓ Building strings of unknown length
 * ✓ Rapid prototyping
 * ✓ Desktop/server applications
 * ✓ When convenience matters more than determinism
 * ✓ JSON/XML generation
 * ✓ Log message formatting
 * ✓ Dynamic output generation
 *
 * When to use data/stringbuf.h instead:
 * ──────────────────────────────────────
 * ✓ Performance-critical code
 * ✓ Embedded systems
 * ✓ Real-time systems
 * ✓ When you know maximum size
 * ✓ When you need deterministic allocation behavior
 * ✓ Arena-backed strings
 *
 * Growth strategy:
 * ────────────────
 * - Initial capacity: 64 bytes
 * - Growth factor: 2x (amortized O(1) append)
 * - No automatic shrinking
 * - Can explicitly shrink_to_fit() if needed
 *
 * Memory management:
 * ──────────────────
 * - Always heap-allocated
 * - Must call dynstring_free() to avoid leaks
 * - No arena support (use data/stringbuf.h for arena-backed)
 *
 * Performance:
 * ────────────
 * - Append: Amortized O(1), worst-case O(n) on realloc
 * - All operations may allocate
 * - Null-terminated always maintained
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/* ─────────────────────────────────────────────────────────────
   Configuration
   ───────────────────────────────────────────────────────────── */

#ifndef DYNSTRING_INITIAL_CAPACITY
#define DYNSTRING_INITIAL_CAPACITY 64
#endif

#ifndef DYNSTRING_GROWTH_FACTOR
#define DYNSTRING_GROWTH_FACTOR 2
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define DYNSTRING_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define DYNSTRING_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define DYNSTRING_LIKELY(x)   (x)
    #define DYNSTRING_UNLIKELY(x) (x)
#endif

/* ─────────────────────────────────────────────────────────────
   DYNSTRING DEFINITION
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Auto-growing string builder
 *
 * Maintains a null-terminated string in a heap-allocated buffer
 * that grows automatically on overflow.
 *
 * Fields:
 *   - data: Heap-allocated buffer (always null-terminated)
 *   - len: Current string length (excluding '\0')
 *   - cap: Total buffer capacity (including space for '\0')
 *
 * Invariants:
 *   - data[len] == '\0' (always null-terminated)
 *   - len < cap (always room for null terminator)
 *   - data is always heap-allocated (or NULL if empty)
 *
 * ⚠️ Do not access fields directly - use provided functions.
 */
typedef struct {
    char*  data;  ///< Heap-allocated buffer (always null-terminated)
    size_t len;   ///< Current string length (excluding '\0')
    size_t cap;   ///< Total buffer capacity (including '\0')
} DynString;

/* ─────────────────────────────────────────────────────────────
   Internal helpers
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Ensures capacity for at least min_cap characters
 *
 * ⚠️ Internal use only - may allocate memory!
 *
 * @param s       String to grow
 * @param min_cap Minimum required capacity (including null terminator)
 * @return        true on success, false on allocation failure
 */
static inline bool dynstring_ensure_capacity(DynString* s, size_t min_cap) {
    assert(s);
    
    if (s->cap >= min_cap) {
        return true;  // Already have enough capacity
    }
    
    // Calculate new capacity (at least double, but ensure min_cap)
    size_t new_cap = s->cap == 0 ? DYNSTRING_INITIAL_CAPACITY : s->cap * DYNSTRING_GROWTH_FACTOR;
    if (new_cap < min_cap) {
        new_cap = min_cap;
    }
    
    // Reallocate
    char* new_data = (char*)realloc(s->data, new_cap);
    if (!new_data) {
        return false;  // Allocation failed
    }
    
    s->data = new_data;
    s->cap = new_cap;
    
    return true;
}

/* ─────────────────────────────────────────────────────────────
   Constructors
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Creates empty string
 *
 * No allocation until first append.
 *
 * Performance:
 * - Time: O(1)
 * - Space: sizeof(DynString)
 * - Heap: none
 *
 * Example:
 *   DynString s = dynstring_init();
 *   dynstring_append(&s, "Hello");
 *   dynstring_free(&s);
 */
static inline DynString dynstring_init(void) {
    return (DynString){
        .data = NULL,
        .len = 0,
        .cap = 0
    };
}

/**
 * @brief Creates string with pre-allocated capacity
 *
 * Useful when expected size is known.
 *
 * Performance:
 * - Time: O(1)
 * - Space: sizeof(DynString) + capacity
 * - Heap: capacity bytes
 *
 * Example:
 *   DynString s = dynstring_with_capacity(1024);
 *   // Can append up to 1023 chars without reallocation
 *   dynstring_free(&s);
 */
static inline DynString dynstring_with_capacity(size_t capacity) {
    DynString s = dynstring_init();
    
    if (capacity > 0) {
        s.data = (char*)malloc(capacity);
        if (s.data) {
            s.data[0] = '\0';
            s.cap = capacity;
        }
    }
    
    return s;
}

/**
 * @brief Creates string from C string
 *
 * Allocates and copies the input string.
 *
 * Performance:
 * - Time: O(n) where n = strlen(str)
 * - Space: sizeof(DynString) + strlen(str) + 1
 * - Heap: strlen(str) + 1 bytes
 *
 * Example:
 *   DynString s = dynstring_from("Hello, World!");
 *   dynstring_free(&s);
 */
static inline DynString dynstring_from(const char* str) {
    DynString s = dynstring_init();
    
    if (str) {
        size_t len = strlen(str);
        size_t cap = len + 1;
        
        s.data = (char*)malloc(cap);
        if (s.data) {
            memcpy(s.data, str, len + 1);  // Include null terminator
            s.len = len;
            s.cap = cap;
        }
    }
    
    return s;
}

/* ─────────────────────────────────────────────────────────────
   Queries
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Returns current length (excluding null terminator)
 *
 * Performance: O(1)
 */
static inline size_t dynstring_len(const DynString* s) {
    return s ? s->len : 0;
}

/**
 * @brief Returns current capacity (including null terminator)
 *
 * Performance: O(1)
 */
static inline size_t dynstring_capacity(const DynString* s) {
    return s ? s->cap : 0;
}

/**
 * @brief Checks if string is empty
 *
 * Performance: O(1)
 */
static inline bool dynstring_is_empty(const DynString* s) {
    return !s || s->len == 0;
}

/**
 * @brief Returns C string (borrowed, null-terminated)
 *
 * Always safe to call - returns "" for empty/NULL string.
 * Pointer valid until next modification or free.
 *
 * Performance: O(1)
 *
 * Example:
 *   printf("%s\n", dynstring_str(&s));
 */
static inline const char* dynstring_str(const DynString* s) {
    return (s && s->data) ? s->data : "";
}

/* ─────────────────────────────────────────────────────────────
   Modification (auto-growing)
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Appends C string
 *
 * ⚠️ May allocate memory!
 *
 * Performance:
 * - Amortized time: O(n) where n = strlen(str)
 * - Worst-case: O(len + n) on realloc
 * - Space: May allocate
 *
 * Example:
 *   DynString s = dynstring_init();
 *   dynstring_append(&s, "Hello");
 *   dynstring_append(&s, " World");
 *   printf("%s\n", dynstring_str(&s));  // "Hello World"
 *   dynstring_free(&s);
 */
static inline bool dynstring_append(DynString* s, const char* str) {
    if (!s) return false;
    if (!str) return true;  // Appending NULL is no-op
    
    size_t add_len = strlen(str);
    if (add_len == 0) return true;
    
    // Ensure capacity for current + new + null terminator
    size_t required_cap = s->len + add_len + 1;
    if (DYNSTRING_UNLIKELY(!dynstring_ensure_capacity(s, required_cap))) {
        return false;  // Allocation failed
    }
    
    // Append
    memcpy(s->data + s->len, str, add_len);
    s->len += add_len;
    s->data[s->len] = '\0';
    
    return true;
}

/**
 * @brief Appends single character
 *
 * ⚠️ May allocate memory!
 *
 * Performance:
 * - Amortized time: O(1)
 * - Worst-case: O(len) on realloc
 *
 * Example:
 *   dynstring_append_char(&s, '\n');
 */
static inline bool dynstring_append_char(DynString* s, char c) {
    if (!s) return false;
    
    // Ensure capacity for current + char + null terminator
    size_t required_cap = s->len + 2;
    if (DYNSTRING_UNLIKELY(!dynstring_ensure_capacity(s, required_cap))) {
        return false;
    }
    
    s->data[s->len++] = c;
    s->data[s->len] = '\0';
    
    return true;
}

/**
 * @brief Appends formatted text (printf-style)
 *
 * ⚠️ May allocate memory!
 *
 * Performance:
 * - Time: O(n) where n = formatted string length
 * - Space: May allocate
 *
 * Example:
 *   dynstring_append_fmt(&s, "Score: %d/%d", score, max);
 */
static inline bool dynstring_append_fmt(DynString* s, const char* fmt, ...) {
    if (!s || !fmt) return false;
    
    va_list args;
    
    // Measure required size
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (needed < 0) return false;
    
    // Ensure capacity
    size_t required_cap = s->len + (size_t)needed + 1;
    if (!dynstring_ensure_capacity(s, required_cap)) {
        return false;
    }
    
    // Format
    va_start(args, fmt);
    int written = vsnprintf(s->data + s->len, s->cap - s->len, fmt, args);
    va_end(args);
    
    if (written < 0 || written != needed) {
        return false;
    }
    
    s->len += (size_t)written;
    
    return true;
}

/**
 * @brief Appends substring (first n characters)
 *
 * ⚠️ May allocate memory!
 *
 * Performance:
 * - Time: O(n)
 * - Space: May allocate
 *
 * Example:
 *   dynstring_append_n(&s, long_string, 10);  // Append first 10 chars
 */
static inline bool dynstring_append_n(DynString* s, const char* str, size_t n) {
    if (!s) return false;
    if (!str || n == 0) return true;
    
    // Find actual length to copy
    size_t actual_len = 0;
    while (actual_len < n && str[actual_len] != '\0') {
        actual_len++;
    }
    
    if (actual_len == 0) return true;
    
    // Ensure capacity
    size_t required_cap = s->len + actual_len + 1;
    if (!dynstring_ensure_capacity(s, required_cap)) {
        return false;
    }
    
    // Append
    memcpy(s->data + s->len, str, actual_len);
    s->len += actual_len;
    s->data[s->len] = '\0';
    
    return true;
}

/**
 * @brief Clears the string (sets length to 0)
 *
 * Does not free memory - use shrink_to_fit() to free excess.
 *
 * Performance: O(1)
 *
 * Example:
 *   dynstring_clear(&s);  // Reuse for new string
 */
static inline void dynstring_clear(DynString* s) {
    if (s && s->data) {
        s->len = 0;
        s->data[0] = '\0';
    }
}

/**
 * @brief Truncates string to specified length
 *
 * If new_len >= current length, does nothing.
 *
 * Performance: O(1)
 *
 * Example:
 *   dynstring_truncate(&s, 10);  // Keep only first 10 characters
 */
static inline void dynstring_truncate(DynString* s, size_t new_len) {
    if (s && s->data && new_len < s->len) {
        s->len = new_len;
        s->data[s->len] = '\0';
    }
}

/* ─────────────────────────────────────────────────────────────
   Capacity management
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Reserves capacity for at least min_cap characters
 *
 * ⚠️ May allocate memory!
 *
 * Performance:
 * - Time: O(1) if cap >= min_cap, else O(len)
 * - Space: May allocate
 *
 * Example:
 *   dynstring_reserve(&s, 1024);  // Pre-allocate for efficiency
 */
static inline bool dynstring_reserve(DynString* s, size_t min_cap) {
    if (!s) return false;
    return dynstring_ensure_capacity(s, min_cap);
}

/**
 * @brief Shrinks capacity to fit current length
 *
 * Frees excess memory.
 *
 * Performance:
 * - Time: O(len)
 * - Space: Frees (cap - len - 1) bytes
 *
 * Example:
 *   dynstring_shrink_to_fit(&s);  // Free unused capacity
 */
static inline bool dynstring_shrink_to_fit(DynString* s) {
    if (!s || !s->data) return true;
    
    if (s->len == 0) {
        // Empty string - free everything
        free(s->data);
        s->data = NULL;
        s->cap = 0;
        return true;
    }
    
    if (s->cap == s->len + 1) {
        return true;  // Already minimal
    }
    
    // Reallocate to exact size
    size_t new_cap = s->len + 1;
    char* new_data = (char*)realloc(s->data, new_cap);
    if (!new_data) return false;
    
    s->data = new_data;
    s->cap = new_cap;
    
    return true;
}

/* ─────────────────────────────────────────────────────────────
   Memory management
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Frees all memory
 *
 * ⚠️ MUST be called to avoid memory leaks!
 *
 * Performance:
 * - Time: O(1)
 * - Space: Frees cap bytes
 *
 * Example:
 *   DynString s = dynstring_init();
 *   // ... use s ...
 *   dynstring_free(&s);  // IMPORTANT: Don't forget!
 */
static inline void dynstring_free(DynString* s) {
    if (s && s->data) {
        free(s->data);
        s->data = NULL;
        s->len = 0;
        s->cap = 0;
    }
}

/* ─────────────────────────────────────────────────────────────
   Utility
   ───────────────────────────────────────────────────────────── */

/**
 * @brief Creates a heap-allocated copy of the string
 *
 * Caller must free the returned string.
 *
 * Performance:
 * - Time: O(len)
 * - Space: Allocates len + 1 bytes
 *
 * Example:
 *   char* copy = dynstring_to_cstr(&s);
 *   // ... use copy ...
 *   free(copy);
 */
static inline char* dynstring_to_cstr(const DynString* s) {
    if (!s || !s->data) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    char* copy = (char*)malloc(s->len + 1);
    if (copy) {
        memcpy(copy, s->data, s->len + 1);
    }
    
    return copy;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────

    #include "dynstring.h"
    #include <stdio.h>
    
    // Example 1: Basic usage
    void example_basic(void) {
        DynString s = dynstring_init();
        
        dynstring_append(&s, "Hello");
        dynstring_append(&s, " ");
        dynstring_append(&s, "World");
        dynstring_append_char(&s, '!');
        
        printf("%s\n", dynstring_str(&s));  // "Hello World!"
        
        dynstring_free(&s);
    }
    
    // Example 2: Formatted output
    void example_formatted(void) {
        DynString s = dynstring_init();
        
        dynstring_append(&s, "Results:\n");
        
        for (int i = 0; i < 5; i++) {
            dynstring_append_fmt(&s, "  Item %d: %d\n", i, i * 10);
        }
        
        printf("%s", dynstring_str(&s));
        
        dynstring_free(&s);
    }
    
    // Example 3: JSON generation
    void example_json(void) {
        DynString json = dynstring_init();
        
        dynstring_append(&json, "{\n");
        dynstring_append_fmt(&json, "  \"name\": \"%s\",\n", "Alice");
        dynstring_append_fmt(&json, "  \"age\": %d,\n", 30);
        dynstring_append(&json, "  \"active\": true\n");
        dynstring_append(&json, "}");
        
        printf("%s\n", dynstring_str(&json));
        
        dynstring_free(&json);
    }
    
    // Example 4: Pre-allocation for efficiency
    void example_prealloc(void) {
        DynString s = dynstring_with_capacity(1024);
        
        // Can append up to 1023 chars without reallocation
        for (int i = 0; i < 100; i++) {
            dynstring_append(&s, "test ");
        }
        
        printf("Length: %zu, Capacity: %zu\n", 
               dynstring_len(&s), dynstring_capacity(&s));
        
        dynstring_free(&s);
    }
    
    // Example 5: Building from existing string
    void example_from_string(void) {
        DynString s = dynstring_from("Initial content");
        
        dynstring_append(&s, " - extended");
        
        printf("%s\n", dynstring_str(&s));
        
        dynstring_free(&s);
    }
    
    // Example 6: Clear and reuse
    void example_reuse(void) {
        DynString s = dynstring_init();
        
        // First use
        dynstring_append(&s, "First message");
        printf("%s\n", dynstring_str(&s));
        
        // Clear and reuse
        dynstring_clear(&s);
        dynstring_append(&s, "Second message");
        printf("%s\n", dynstring_str(&s));
        
        dynstring_free(&s);
    }
    
    // Example 7: Memory management
    void example_memory_mgmt(void) {
        DynString s = dynstring_with_capacity(1024);
        
        dynstring_append(&s, "Small text");
        
        // Capacity is 1024, but only using ~10 bytes
        printf("Before shrink: %zu/%zu\n", 
               dynstring_len(&s), dynstring_capacity(&s));
        
        dynstring_shrink_to_fit(&s);
        
        printf("After shrink: %zu/%zu\n", 
               dynstring_len(&s), dynstring_capacity(&s));
        
        dynstring_free(&s);
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_DATA_CONVENIENCE_DYNSTRING_H */
