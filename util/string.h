// util/string.h
#ifndef CANON_UTIL_STRING_H
#define CANON_UTIL_STRING_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "core/memory.h"
#include "semantics/option.h"

/**
 * @file string.h
 * @brief Safe, explicit and ownership-aware string utilities
 *
 * Provides modern string manipulation utilities with clear ownership semantics
 * and explicit allocation boundaries. Implements both heap-allocating and
 * buffer-based operations, along with pure predicate functions for string
 * comparison and checking.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, size_t)
 *   - Depends on memory.h from this library (for mem_alloc, mem_copy, mem_free)
 *   - Uses standard string.h (strlen, strcmp, strncmp)
 *   - No platform-specific features
 *   - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 *                Safe to call from multiple threads simultaneously. Owned
 *                strings can be freed from any thread that owns them. Buffer
 *                operations safe if different buffers used.
 *
 * Performance:
 *   - Time complexity: O(n) where n = string length for most operations
 *   - Space complexity: O(1) for borrowed operations, O(n) for owned
 *   - Owned operations: one malloc per allocation
 *   - Borrowed operations: zero allocations
 *   - Predicate operations: optimized short-circuit evaluation
 *   - Minimal overhead over raw C string operations
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Clear ownership model — owned vs borrowed explicitly separated
 * - Null-safe throughout — graceful handling of NULL inputs
 * - Bounds-safe — buffer operations never overflow
 * - Explicit allocation — function names tell you what allocates
 * - No hidden failures — bool/Option return values always checked
 * - Zero hidden state — no globals or thread-locals
 * - Composable design — works with str_join, parse, and other modules
 * - Minimal API surface — focused on essentials
 * - Const-correct — predicates don't modify inputs
 * - Predictable behavior — no surprises on edge cases
 *
 * Ownership model:
 * ────────────────────────────────────────────────────────────────────────────
 * This library strictly separates two allocation strategies:
 *
 * 1. OWNED STRINGS (str_alloc_* functions):
 *    - Function allocates memory on heap
 *    - Returns Option<char*>
 *    - Caller OWNS the result
 *    - Caller MUST free with str_free() or mem_free()
 *    - Forgetting to free = memory leak
 *    - Valid until explicitly freed
 *    - Can be passed around freely
 *
 * 2. BORROWED STRINGS (str_*_into functions):
 *    - Caller provides buffer
 *    - Function writes into buffer
 *    - Returns bool (success/failure)
 *    - Zero allocations
 *    - Caller owns buffer (stack or heap)
 *    - No freeing needed (caller manages lifetime)
 *    - Fast and predictable
 *
 * Choose owned when:
 *   - Size unknown at compile time
 *   - Result needs to outlive function scope
 *   - Building dynamic data structures
 *   - Returning strings from functions
 *
 * Choose borrowed when:
 *   - Maximum size known
 *   - Want to avoid heap allocation
 *   - Working with stack buffers
 *   - Performance critical paths
 *   - Embedded systems
 *
 * Memory management rules:
 * ────────────────────────────────────────────────────────────────────────────
 * OWNED STRINGS:
 * - Every str_alloc_* that returns Some MUST be freed
 * - Free exactly once per allocation
 * - Use str_free() or mem_free() (they're equivalent)
 * - NULL is safe to free (no-op)
 * - Don't use after freeing (use-after-free)
 *
 * Example - correct:
 * ```c
 * option_charp s = str_alloc_copy("hello");
 * if (option_charp_is_some(s)) {
 *     char* str = option_charp_unwrap(s);
 *     use(str);
 *     str_free(str);  // MUST free
 * }
 * ```
 *
 * Example - memory leak:
 * ```c
 * option_charp s = str_alloc_copy("hello");
 * if (option_charp_is_some(s)) {
 *     use(option_charp_unwrap(s));
 *     // FORGOT TO FREE - memory leak!
 * }
 * ```
 *
 * Example - double free (wrong):
 * ```c
 * option_charp s = str_alloc_copy("hello");
 * if (option_charp_is_some(s)) {
 *     char* str = option_charp_unwrap(s);
 *     str_free(str);
 *     str_free(str);  // WRONG - double free!
 * }
 * ```
 *
 * BORROWED STRINGS:
 * - Caller owns buffer
 * - Caller manages lifetime
 * - No freeing needed for the operation itself
 * - If buffer is heap-allocated, caller frees it normally
 *
 * Example:
 * ```c
 * char buf[100];  // Stack buffer
 * str_copy_into(buf, sizeof(buf), "hello");
 * use(buf);
 * // No freeing needed - stack allocated
 * ```
 *
 * Error handling:
 * ────────────────────────────────────────────────────────────────────────────
 * OWNED FUNCTIONS (return Option):
 * - Always check with option_charp_is_some() before unwrapping
 * - None means: NULL input, allocation failed, or invalid operation
 * - Unwrapping None is undefined behavior
 *
 * BORROWED FUNCTIONS (return bool):
 * - true = success, operation completed
 * - false = failure (buffer too small, NULL input, invalid params)
 * - On false, buffer may be in undefined state (don't use)
 *
 * PREDICATES (return bool):
 * - true/false based on condition
 * - NULL inputs handled gracefully (usually return false)
 * - Safe to use directly in conditionals
 *
 * Common patterns:
 * ```c
 * // Owned - with error handling
 * option_charp result = str_alloc_concat(a, b);
 * if (option_charp_is_some(result)) {
 *     char* str = option_charp_unwrap(result);
 *     process(str);
 *     str_free(str);
 * } else {
 *     handle_error();
 * }
 *
 * // Borrowed - with error handling
 * char buf[256];
 * if (str_copy_into(buf, sizeof(buf), source)) {
 *     process(buf);
 * } else {
 *     handle_truncation();
 * }
 *
 * // Predicate - direct use
 * if (str_starts_with(filename, "test_")) {
 *     run_test(filename);
 * }
 * ```
 *
 * Null safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions handle NULL inputs gracefully:
 *
 * Owned functions (str_alloc_*):
 *   - NULL input → None
 *   - Never crash on NULL
 *   - Safe to pass NULL (just returns None)
 *
 * Borrowed functions (str_*_into):
 *   - NULL input → false
 *   - NULL buffer → false
 *   - Zero buffer size → false
 *
 * Predicates (str_equals, str_starts_with, etc.):
 *   - NULL inputs usually return false
 *   - str_equals(NULL, NULL) → true (both NULL = equal)
 *   - str_equals(NULL, "x") → false
 *
 * This means you can safely chain operations:
 * ```c
 * option_charp s = str_alloc_copy(input);  // input might be NULL
 * if (option_charp_is_some(s)) {
 *     // Only enter if allocation succeeded
 * }
 * ```
 *
 * Buffer sizing and truncation:
 * ────────────────────────────────────────────────────────────────────────────
 * Borrowed functions never partially fill buffers - they're all-or-nothing:
 *
 * - If source fits in buffer → copies completely, returns true
 * - If source doesn't fit → returns false (buffer state undefined)
 * - Always account for null terminator in size calculation
 *
 * Buffer size calculation:
 * ```c
 * // For str_copy_into:
 * required_size = strlen(source) + 1;  // +1 for null terminator
 *
 * // For str_concat_into:
 * required_size = strlen(a) + strlen(b) + 1;
 * ```
 *
 * Safe buffer sizing pattern:
 * ```c
 * // Know maximum size
 * char buf[MAX_SIZE];
 * if (str_copy_into(buf, sizeof(buf), source)) {
 *     // Success - buf contains complete copy
 * } else {
 *     // Failure - source was too long
 * }
 *
 * // Or use owned allocation for unknown sizes
 * option_charp copy = str_alloc_copy(source);
 * if (option_charp_is_some(copy)) {
 *     // Always succeeds for any size (if memory available)
 * }
 * ```
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Building strings from components
 * - Duplicating strings for modification
 * - Extracting substrings
 * - Path manipulation (concatenation, extension checking)
 * - String comparison and matching
 * - Prefix/suffix checking
 * - Configuration string handling
 * - User input processing
 * - Dynamic string building
 * - String deduplication
 *
 * NOT suitable for:
 * - Complex string formatting (use snprintf)
 * - Localized strings (use i18n library)
 * - Regular expressions (use regex library)
 * - Unicode operations (use UTF-8 library)
 * - Very large strings (use streaming)
 * - String interning (use dedicated implementation)
 *
 * Integration with other modules:
 * ────────────────────────────────────────────────────────────────────────────
 * This module works seamlessly with other string utilities:
 *
 * - str_join.h: Join multiple parts, then use str_alloc_copy if needed
 * - str_split.h: Split into parts, then str_alloc_sub to extract
 * - parse.h: Parse strings, then str_alloc_copy results if needed
 * - file.h: Read files, use str_equals to compare with expected content
 *
 * Example workflow:
 * ```c
 * // Read file
 * option_charp content = file_read_all("data.txt");
 * if (option_charp_is_none(content)) return;
 * 
 * char* text = option_charp_unwrap(content);
 * 
 * // Split into lines
 * const char* lines[100];
 * size_t count = str_split(text, '\n', lines, 100);
 * 
 * // Check each line
 * for (size_t i = 0; i < count; i++) {
 *     if (str_starts_with(lines[i], "ERROR:")) {
 *         // Extract error substring
 *         option_charp err = str_alloc_sub(lines[i], 7, 100);
 *         if (option_charp_is_some(err)) {
 *             log_error(option_charp_unwrap(err));
 *             str_free(option_charp_unwrap(err));
 *         }
 *     }
 * }
 * 
 * str_free(text);
 * ```
 *
 * Usage examples:
 *
 * Basic string copying:
 * ```c
 * // Owned - dynamic allocation
 * option_charp copy = str_alloc_copy("hello world");
 * if (option_charp_is_some(copy)) {
 *     char* str = option_charp_unwrap(copy);
 *     modify(str);  // Can modify owned copy
 *     str_free(str);
 * }
 * 
 * // Borrowed - stack buffer
 * char buf[64];
 * if (str_copy_into(buf, sizeof(buf), "hello world")) {
 *     printf("%s\n", buf);
 * }
 * ```
 *
 * String concatenation:
 * ```c
 * // Owned
 * option_charp greeting = str_alloc_concat("Hello, ", name);
 * if (option_charp_is_some(greeting)) {
 *     printf("%s\n", option_charp_unwrap(greeting));
 *     str_free(option_charp_unwrap(greeting));
 * }
 * 
 * // Borrowed
 * char message[256];
 * if (str_concat_into(message, sizeof(message), "Status: ", status)) {
 *     log_message(message);
 * }
 * ```
 *
 * String comparison:
 * ```c
 * if (str_equals(input, "quit")) {
 *     exit(0);
 * }
 * 
 * if (str_starts_with(filename, "test_")) {
 *     run_test(filename);
 * }
 * 
 * if (str_ends_with(filename, ".txt")) {
 *     process_text_file(filename);
 * }
 * ```
 */

/**
 * @brief Option type alias for owned (heap-allocated) strings
 *
 * Represents an optional string that may be Some(char*) or None.
 * Used by all str_alloc_* functions to indicate success or failure.
 *
 * - Some(char*): Operation succeeded, contains heap-allocated string
 * - None: Operation failed (NULL input, allocation failed, invalid params)
 *
 * Usage:
 * ```c
 * option_charp result = str_alloc_copy("hello");
 * if (option_charp_is_some(result)) {
 *     char* str = option_charp_unwrap(result);
 *     // use str...
 *     str_free(str);  // MUST free!
 * }
 * ```
 */
CANON_C_DEFINE_OPTION(charp, char*)

/* ────────────────────────────────────────────────────────────────────────────
   Owned strings — heap allocation (caller must free)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates a new copy of a null-terminated string
 *
 * Creates a heap-allocated duplicate of the input string. The returned
 * string is completely independent of the original - modifying one does
 * not affect the other.
 *
 * This is useful when you need a mutable copy, want to store a string
 * that outlives the original, or need ownership of string data.
 *
 * Algorithm:
 *   - Validate input (check for NULL)
 *   - Calculate length with strlen
 *   - Allocate length + 1 bytes (for null terminator)
 *   - Copy all bytes including null terminator
 *   - Return Option with result
 *
 * @param s Source null-terminated string (may be NULL)
 *
 * @return  Some(owned heap-allocated copy) on success
 *          None on NULL input or allocation failure
 *
 * Preconditions:
 *   - If s is not NULL: s must be null-terminated
 *
 * Postconditions:
 *   - If Some: returned string is identical to s
 *   - If Some: returned string is heap-allocated
 *   - If Some: caller MUST call str_free() or mem_free()
 *   - If None: no memory was allocated
 *   - Original string s is never modified
 *
 * Memory ownership:
 *   - Caller owns returned string
 *   - Must free with str_free() or mem_free()
 *   - Forgetting to free causes memory leak
 *
 * Performance:
 *   - Time: O(n) where n = strlen(s)
 *   - Space: O(n) - allocates n+1 bytes
 *   - One malloc, one memcpy
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Basic usage:
 * ```c
 * option_charp copy = str_alloc_copy("original");
 * if (option_charp_is_some(copy)) {
 *     char* str = option_charp_unwrap(copy);
 *     printf("%s\n", str);
 *     str_free(str);  // MUST free
 * }
 * ```
 *
 * Example - NULL input:
 * ```c
 * option_charp result = str_alloc_copy(NULL);
 * // result = None (safe, no crash)
 * ```
 *
 * Example - Modifiable copy:
 * ```c
 * const char* original = "hello";
 * option_charp copy = str_alloc_copy(original);
 * if (option_charp_is_some(copy)) {
 *     char* str = option_charp_unwrap(copy);
 *     str[0] = 'H';  // OK - we own the copy
 *     printf("%s\n", str);  // "Hello"
 *     str_free(str);
 * }
 * // original is unchanged ("hello")
 * ```
 *
 * Example - Storing in data structure:
 * ```c
 * struct Config {
 *     char* name;
 *     char* value;
 * };
 * 
 * void set_config(struct Config* cfg, const char* name, const char* value) {
 *     option_charp name_copy = str_alloc_copy(name);
 *     option_charp value_copy = str_alloc_copy(value);
 *     
 *     if (option_charp_is_some(name_copy) && option_charp_is_some(value_copy)) {
 *         cfg->name = option_charp_unwrap(name_copy);
 *         cfg->value = option_charp_unwrap(value_copy);
 *     }
 * }
 * ```
 *
 * Example - With automatic cleanup:
 * ```c
 * option_charp copy = str_alloc_copy("temp");
 * if (option_charp_is_some(copy)) {
 *     char* str = option_charp_unwrap(copy);
 *     SCOPE_DEFER { str_free(str); };  // Automatic cleanup
 *     
 *     process(str);
 *     // str freed automatically at scope exit
 * }
 * ```
 *
 * Common pitfalls:
 * - Forgetting to free (memory leak)
 * - Double freeing (undefined behavior)
 * - Using after freeing (use-after-free)
 * - Not checking Option before unwrapping
 */
static inline option_charp str_alloc_copy(const char* s) {
    if (!s) {
        return option_charp_none();
    }

    const size_t len = strlen(s);
    char* copy = (char*)mem_alloc(len + 1);
    if (!copy) {
        return option_charp_none();
    }

    mem_copy(copy, s, len + 1);  // Copy including null terminator
    return option_charp_some(copy);
}

/**
 * @brief Allocates concatenation of two null-terminated strings
 *
 * Creates a heap-allocated string containing the concatenation of two input
 * strings. The result is a new string: a + b with no separator.
 *
 * Algorithm:
 *   - Validate inputs (check for NULL)
 *   - Calculate lengths of both strings
 *   - Allocate len_a + len_b + 1 bytes
 *   - Copy first string
 *   - Copy second string immediately after first
 *   - Return Option with result
 *
 * @param a First null-terminated string (must not be NULL)
 * @param b Second null-terminated string (must not be NULL)
 *
 * @return  Some(owned concatenated string) on success
 *          None on NULL input or allocation failure
 *
 * Preconditions:
 *   - Both a and b must be null-terminated
 *
 * Postconditions:
 *   - If Some: result equals a concatenated with b
 *   - If Some: caller MUST free result
 *   - If None: no memory allocated
 *   - Input strings never modified
 *
 * Memory ownership:
 *   - Caller owns returned string
 *   - Must free with str_free() or mem_free()
 *
 * Performance:
 *   - Time: O(n + m) where n = strlen(a), m = strlen(b)
 *   - Space: O(n + m)
 *   - One malloc, two memcpy operations
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Basic concatenation:
 * ```c
 * option_charp greeting = str_alloc_concat("Hello, ", "World!");
 * if (option_charp_is_some(greeting)) {
 *     printf("%s\n", option_charp_unwrap(greeting));  // "Hello, World!"
 *     str_free(option_charp_unwrap(greeting));
 * }
 * ```
 *
 * Example - Building paths:
 * ```c
 * option_charp path = str_alloc_concat(directory, filename);
 * if (option_charp_is_some(path)) {
 *     open_file(option_charp_unwrap(path));
 *     str_free(option_charp_unwrap(path));
 * }
 * ```
 *
 * Example - Building messages:
 * ```c
 * option_charp msg = str_alloc_concat("Error: ", error_text);
 * if (option_charp_is_some(msg)) {
 *     log_error(option_charp_unwrap(msg));
 *     str_free(option_charp_unwrap(msg));
 * }
 * ```
 *
 * Example - Chaining concatenations:
 * ```c
 * option_charp ab = str_alloc_concat(a, b);
 * if (option_charp_is_none(ab)) return;
 * 
 * char* ab_str = option_charp_unwrap(ab);
 * option_charp abc = str_alloc_concat(ab_str, c);
 * str_free(ab_str);  // Free intermediate result
 * 
 * if (option_charp_is_some(abc)) {
 *     process(option_charp_unwrap(abc));
 *     str_free(option_charp_unwrap(abc));
 * }
 * ```
 *
 * Example - NULL input handling:
 * ```c
 * option_charp result = str_alloc_concat(NULL, "world");
 * // result = None (safe, no crash)
 * ```
 *
 * Common use cases:
 * - Building error messages
 * - Path construction
 * - String interpolation
 * - Protocol message building
 * - URL construction
 *
 * For multiple concatenations, consider str_join.h instead.
 */
static inline option_charp str_alloc_concat(const char* a, const char* b) {
    if (!a || !b) {
        return option_charp_none();
    }

    const size_t len_a = strlen(a);
    const size_t len_b = strlen(b);
    const size_t total = len_a + len_b;

    char* result = (char*)mem_alloc(total + 1);
    if (!result) {
        return option_charp_none();
    }

    mem_copy(result, a, len_a);
    mem_copy(result + len_a, b, len_b + 1);  // Includes null terminator

    return option_charp_some(result);
}

/**
 * @brief Allocates a substring from source string
 *
 * Extracts a portion of the source string and returns it as a new heap-
 * allocated string. The substring starts at the given index and continues
 * for the specified length (or until end of string, whichever comes first).
 *
 * Algorithm:
 *   - Validate input (check for NULL)
 *   - Calculate source length
 *   - Validate start index
 *   - Clamp length to available characters
 *   - Allocate length + 1 bytes
 *   - Copy substring
 *   - Null-terminate
 *   - Return Option with result
 *
 * @param s     Source null-terminated string (must not be NULL)
 * @param start Starting index (0-based, inclusive)
 * @param len   Desired length (clamped if exceeds remaining string)
 *
 * @return      Some(owned substring) on success
 *              None on NULL source, start >= strlen(s), or allocation failure
 *
 * Preconditions:
 *   - s is null-terminated
 *   - start is valid index or will return None
 *
 * Postconditions:
 *   - If Some: result contains characters s[start..start+len) or until end
 *   - If Some: caller MUST free result
 *   - If None: no memory allocated
 *   - Source string never modified
 *
 * Clamping behavior:
 *   - If start + len > strlen(s), takes characters up to end of string
 *   - If start >= strlen(s), returns None
 *   - Length 0 is valid (returns empty string)
 *
 * Memory ownership:
 *   - Caller owns returned string
 *   - Must free with str_free() or mem_free()
 *
 * Performance:
 *   - Time: O(n) where n = min(len, strlen(s) - start)
 *   - Space: O(n)
 *   - One malloc, one memcpy
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Extract substring:
 * ```c
 * option_charp sub = str_alloc_sub("Hello World", 6, 5);
 * if (option_charp_is_some(sub)) {
 *     printf("%s\n", option_charp_unwrap(sub));  // "World"
 *     str_free(option_charp_unwrap(sub));
 * }
 * ```
 *
 * Example - Clamping:
 * ```c
 * option_charp sub = str_alloc_sub("Hello", 2, 100);
 * if (option_charp_is_some(sub)) {
 *     printf("%s\n", option_charp_unwrap(sub));  // "llo" (clamped to end)
 *     str_free(option_charp_unwrap(sub));
 * }
 * ```
 *
 * Example - Invalid start:
 * ```c
 * option_charp sub = str_alloc_sub("Hello", 10, 5);
 * // sub = None (start >= strlen)
 * ```
 *
 * Example - Extract file extension:
 * ```c
 * const char* filename = "document.txt";
 * size_t len = strlen(filename);
 * 
 * // Find last dot
 * const char* dot = strrchr(filename, '.');
 * if (dot) {
 *     size_t dot_pos = dot - filename;
 *     option_charp ext = str_alloc_sub(filename, dot_pos + 1, len);
 *     if (option_charp_is_some(ext)) {
 *         printf("Extension: %s\n", option_charp_unwrap(ext));  // "txt"
 *         str_free(option_charp_unwrap(ext));
 *     }
 * }
 * ```
 *
 * Example - Extract first N characters:
 * ```c
 * option_charp prefix = str_alloc_sub("Hello World", 0, 5);
 * // prefix = Some("Hello")
 * ```
 *
 * Example - Extract from middle:
 * ```c
 * const char* text = "The quick brown fox";
 * option_charp word = str_alloc_sub(text, 4, 5);
 * if (option_charp_is_some(word)) {
 *     printf("%s\n", option_charp_unwrap(word));  // "quick"
 *     str_free(option_charp_unwrap(word));
 * }
 * ```
 *
 * Common use cases:
 * - Extracting tokens from parsed text
 * - Getting file extensions
 * - Extracting URL components
 * - Getting prefixes/suffixes
 * - Parsing fixed-width fields
 */
static inline option_charp str_alloc_sub(
    const char* s,
    size_t start,
    size_t len
) {
    if (!s) {
        return option_charp_none();
    }

    const size_t s_len = strlen(s);
    
    // Check if start is valid
    if (start >= s_len) {
        return option_charp_none();
    }

    // Clamp length to available characters
    if (start + len > s_len) {
        len = s_len - start;
    }

    char* result = (char*)mem_alloc(len + 1);
    if (!result) {
        return option_charp_none();
    }

    mem_copy(result, s + start, len);
    result[len] = '\0';

    return option_charp_some(result);
}

/**
 * @brief Explicit free function for strings allocated by str_alloc_* functions
 *
 * Frees memory allocated by str_alloc_copy, str_alloc_concat, or str_alloc_sub.
 * This is a convenience wrapper around mem_free() for clarity and consistency.
 *
 * Safe to call with NULL (no-op). Safe to call multiple times on same pointer
 * is undefined behavior (like standard free).
 *
 * @param s Pointer to owned string (NULL-safe)
 *
 * Preconditions:
 *   - If s is not NULL: must have been allocated by str_alloc_* or mem_alloc
 *   - Must not have been freed already
 *
 * Postconditions:
 *   - Memory is deallocated
 *   - Pointer becomes invalid (don't use after freeing)
 *
 * Thread-safety: Safe if pointer not shared across threads
 *
 * Example - Basic usage:
 * ```c
 * option_charp s = str_alloc_copy("hello");
 * if (option_charp_is_some(s)) {
 *     char* str = option_charp_unwrap(s);
 *     use(str);
 *     str_free(str);  // Free when done
 * }
 * ```
 *
 * Example - NULL is safe:
 * ```c
 * char* s = NULL;
 * str_free(s);  // No-op, safe
 * ```
 *
 * Common pitfall - use after free:
 * ```c
 * char* s = option_charp_unwrap(str_alloc_copy("hello"));
 * str_free(s);
 * printf("%s\n", s);  // WRONG - use after free!
 * ```
 *
 * Common pitfall - double free:
 * ```c
 * char* s = option_charp_unwrap(str_alloc_copy("hello"));
 * str_free(s);
 * str_free(s);  // WRONG - double free!
 * ```
 */
static inline void str_free(char* s) {
    mem_free(s);
}

/* ────────────────────────────────────────────────────────────────────────────
   Borrowed strings — zero-allocation, caller provides buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Safely copies source string into fixed-size buffer
 *
 * Copies the source string into a caller-provided buffer if it fits completely.
 * This is an all-or-nothing operation - either the entire string is copied
 * (returns true) or the operation fails (returns false).
 *
 * Unlike strncpy, this function:
 * - Never partially fills the buffer
 * - Always null-terminates on success
 * - Returns explicit success/failure
 * - Fails rather than truncates
 *
 * Algorithm:
 *   - Validate inputs (dest, size, src)
 *   - Calculate source length
 *   - Check if source + null terminator fits
 *   - If yes: copy entire string including null terminator, return true
 *   - If no: return false (buffer unchanged)
 *
 * @param dest      Writable destination buffer (must not be NULL)
 * @param dest_size Size of buffer in bytes (including space for null terminator)
 * @param src       Null-terminated source string (must not be NULL)
 *
 * @return          true on successful full copy
 *                  false if truncation would occur, or NULL input, or zero-size buffer
 *
 * Preconditions:
 *   - dest points to writable buffer of size dest_size
 *   - dest_size > 0
 *   - src is null-terminated
 *
 * Postconditions:
 *   - If true: dest contains complete copy of src, null-terminated
 *   - If false: dest state is undefined (don't use)
 *   - Source string never modified
 *
 * Buffer sizing:
 *   - Required size: strlen(src) + 1
 *   - Must account for null terminator
 *
 * Performance:
 *   - Time: O(n) where n = strlen(src)
 *   - Space: O(1) - no allocations
 *   - One strlen, one memcpy
 *
 * Thread-safety: Safe if different buffers used
 *
 * Example - Basic copy:
 * ```c
 * char buf[64];
 * if (str_copy_into(buf, sizeof(buf), "hello world")) {
 *     printf("%s\n", buf);  // "hello world"
 * }
 * ```
 *
 * Example - Too large:
 * ```c
 * char small[5];
 * if (!str_copy_into(small, sizeof(small), "hello world")) {
 *     printf("String too large\n");
 * }
 * ```
 *
 * Example - Exact fit:
 * ```c
 * char buf[6];  // "hello" + null = 6 bytes
 * if (str_copy_into(buf, sizeof(buf), "hello")) {
 *     printf("Perfect fit: %s\n", buf);
 * }
 * ```
 *
 * Example - Dynamic source:
 * ```c
 * char buf[256];
 * if (str_copy_into(buf, sizeof(buf), user_input)) {
 *     process(buf);
 * } else {
 *     fprintf(stderr, "Input too long (max %zu chars)\n", sizeof(buf) - 1);
 * }
 * ```
 *
 * Example - Stack buffer pattern:
 * ```c
 * void process_name(const char* name) {
 *     char buffer[MAX_NAME_LEN];
 *     if (str_copy_into(buffer, sizeof(buffer), name)) {
 *         // Safe to modify buffer
 *         normalize(buffer);
 *         store(buffer);
 *     }
 * }
 * ```
 *
 * Common use cases:
 * - Copying to stack buffers
 * - Preparing strings for modification
 * - Validating string length
 * - Safe string duplication without allocation
 */
static inline bool str_copy_into(
    char* dest,
    size_t dest_size,
    const char* src
) {
    if (!dest || dest_size == 0 || !src) {
        return false;
    }

    const size_t src_len = strlen(src);
    
    // Check if source + null terminator fits
    if (src_len + 1 > dest_size) {
        return false;
    }

    mem_copy(dest, src, src_len + 1);  // Copy including null terminator
    return true;
}

/**
 * @brief Safely concatenates two strings into fixed-size buffer
 *
 * Concatenates two strings into a caller-provided buffer if the result fits
 * completely. This is an all-or-nothing operation - either both strings are
 * copied (returns true) or the operation fails (returns false).
 *
 * Algorithm:
 *   - Validate inputs (dest, size, both sources)
 *   - Calculate lengths of both strings
 *   - Check if combined length + null terminator fits
 *   - If yes: copy first string, then second, return true
 *   - If no: return false (buffer unchanged)
 *
 * @param dest      Writable destination buffer (must not be NULL)
 * @param dest_size Size of buffer in bytes (including space for null terminator)
 * @param a         First null-terminated string (must not be NULL)
 * @param b         Second null-terminated string (must not be NULL)
 *
 * @return          true on successful full concatenation
 *                  false if would overflow, or NULL input, or zero-size buffer
 *
 * Preconditions:
 *   - dest points to writable buffer of size dest_size
 *   - dest_size > 0
 *   - Both a and b are null-terminated
 *
 * Postconditions:
 *   - If true: dest contains a + b, null-terminated
 *   - If false: dest state is undefined (don't use)
 *   - Source strings never modified
 *
 * Buffer sizing:
 *   - Required size: strlen(a) + strlen(b) + 1
 *
 * Performance:
 *   - Time: O(n + m) where n = strlen(a), m = strlen(b)
 *   - Space: O(1) - no allocations
 *   - Two strlen calls, two memcpy operations
 *
 * Thread-safety: Safe if different buffers used
 *
 * Example - Basic concatenation:
 * ```c
 * char buf[64];
 * if (str_concat_into(buf, sizeof(buf), "Hello, ", "World!")) {
 *     printf("%s\n", buf);  // "Hello, World!"
 * }
 * ```
 *
 * Example - Building paths:
 * ```c
 * char path[256];
 * if (str_concat_into(path, sizeof(path), directory, filename)) {
 *     open_file(path);
 * }
 * ```
 *
 * Example - Building messages:
 * ```c
 * char msg[128];
 * if (str_concat_into(msg, sizeof(msg), "Error: ", error_text)) {
 *     log_message(msg);
 * } else {
 *     log_message("Error: (message too long)");
 * }
 * ```
 *
 * Example - Overflow detection:
 * ```c
 * char small[10];
 * if (!str_concat_into(small, sizeof(small), "very long", " string")) {
 *     fprintf(stderr, "Concatenation would overflow\n");
 * }
 * ```
 *
 * Example - Status messages:
 * ```c
 * char status[256];
 * if (str_concat_into(status, sizeof(status), "Processing: ", filename)) {
 *     update_ui(status);
 * }
 * ```
 *
 * Common use cases:
 * - Building file paths
 * - Constructing messages
 * - Status string building
 * - Simple string interpolation
 * - Prefix/suffix addition
 */
static inline bool str_concat_into(
    char* dest,
    size_t dest_size,
    const char* a,
    const char* b
) {
    if (!dest || dest_size == 0 || !a || !b) {
        return false;
    }

    const size_t len_a = strlen(a);
    const size_t len_b = strlen(b);

    // Check if both strings + null terminator fit
    if (len_a + len_b + 1 > dest_size) {
        return false;
    }

    mem_copy(dest, a, len_a);
    mem_copy(dest + len_a, b, len_b + 1);  // Includes null terminator

    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Pure predicates — fast, const-correct string checks
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Checks if two null-terminated strings are equal
 *
 * Compares two strings for exact equality. Handles NULL inputs gracefully.
 *
 * Algorithm:
 *   - If pointers are same → true (including both NULL)
 *   - If one is NULL → false
 *   - Otherwise use strcmp
 *
 * @param a First string (may be NULL)
 * @param b Second string (may be NULL)
 *
 * @return  true if strings are identical (including both NULL)
 *          false otherwise
 *
 * Null handling:
 *   - str_equals(NULL, NULL) → true (both NULL = equal)
 *   - str_equals(NULL, "x") → false
 *   - str_equals("x", NULL) → false
 *
 * Performance:
 *   - Time: O(n) where n = string length (short-circuits on difference)
 *   - Space: O(1)
 *
 * Thread-safety: Fully thread-safe
 *
 * Example - Command checking:
 * ```c
 * if (str_equals(command, "quit")) {
 *     exit(0);
 * }
 * ```
 *
 * Example - Configuration:
 * ```c
 * if (str_equals(mode, "debug")) {
 *     enable_debug_logging();
 * }
 * ```
 *
 * Example - NULL safety:
 * ```c
 * const char* input = get_user_input();  // might return NULL
 * if (str_equals(input, "yes")) {
 *     // Safe - handles NULL gracefully
 * }
 * ```
 */
static inline bool str_equals(const char* a, const char* b) {
    // Same pointer (including both NULL)
    if (a == b) {
        return true;
    }
    
    // One is NULL, other isn't
    if (!a || !b) {
        return false;
    }
    
    return strcmp(a, b) == 0;
}

/**
 * @brief Checks if string starts with given prefix
 *
 * Tests whether a string begins with a specific prefix. Empty prefix always
 * matches (every string starts with empty string).
 *
 * Algorithm:
 *   - Validate inputs
 *   - If prefix is empty → true
 *   - Use strncmp to compare first N characters
 *
 * @param s      String to check (may be NULL)
 * @param prefix Prefix to look for (may be NULL)
 *
 * @return       true if s begins with prefix (empty prefix → always true)
 *               false otherwise or if inputs are NULL
 *
 * Performance:
 *   - Time: O(m) where m = strlen(prefix)
 *   - Space: O(1)
 *
 * Thread-safety: Fully thread-safe
 *
 * Example - File extension checking:
 * ```c
 * if (str_starts_with(filename, "test_")) {
 *     run_test(filename);
 * }
 * ```
 *
 * Example - Protocol checking:
 * ```c
 * if (str_starts_with(url, "https://")) {
 *     use_secure_connection(url);
 * }
 * ```
 *
 * Example - Command prefix:
 * ```c
 * if (str_starts_with(input, "/")) {
 *     process_command(input + 1);  // Skip the '/'
 * }
 * ```
 *
 * Example - Empty prefix:
 * ```c
 * assert(str_starts_with("anything", ""));  // Always true
 * ```
 */
static inline bool str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) {
        return false;
    }

    const size_t prefix_len = strlen(prefix);
    
    // Empty prefix always matches
    return prefix_len == 0 || strncmp(s, prefix, prefix_len) == 0;
}

/**
 * @brief Checks if string ends with given suffix
 *
 * Tests whether a string ends with a specific suffix.
 *
 * Algorithm:
 *   - Validate inputs
 *   - Calculate both lengths
 *   - If suffix longer than string → false
 *   - Compare last N characters where N = suffix length
 *
 * @param s      String to check (may be NULL)
 * @param suffix Suffix to look for (may be NULL)
 *
 * @return       true if s ends with suffix
 *               false otherwise (including if suffix longer than s)
 *
 * Performance:
 *   - Time: O(m) where m = strlen(suffix)
 *   - Space: O(1)
 *
 * Thread-safety: Fully thread-safe
 *
 * Example - File extension:
 * ```c
 * if (str_ends_with(filename, ".txt")) {
 *     process_text_file(filename);
 * } else if (str_ends_with(filename, ".jpg")) {
 *     process_image_file(filename);
 * }
 * ```
 *
 * Example - URL validation:
 * ```c
 * if (str_ends_with(url, "/")) {
 *     // URL ends with slash
 * }
 * ```
 *
 * Example - File type checking:
 * ```c
 * if (str_ends_with(path, ".c") || str_ends_with(path, ".h")) {
 *     compile_c_file(path);
 * }
 * ```
 */
static inline bool str_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) {
        return false;
    }

    const size_t s_len = strlen(s);
    const size_t suffix_len = strlen(suffix);

    // Suffix longer than string
    if (suffix_len > s_len) {
        return false;
    }

    return strcmp(s + s_len - suffix_len, suffix) == 0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/string.h"
    #include <stdio.h>
    
    // Example 1: Basic string copying
    void example_basic_copy(void) {
        // Owned
        option_charp copy = str_alloc_copy("hello world");
        if (option_charp_is_some(copy)) {
            char* str = option_charp_unwrap(copy);
            printf("Copy: %s\n", str);
            str_free(str);
        }
        
        // Borrowed
        char buf[64];
        if (str_copy_into(buf, sizeof(buf), "hello world")) {
            printf("Buffer: %s\n", buf);
        }
    }
    
    // Example 2: String concatenation
    void example_concatenation(void) {
        const char* first = "Hello, ";
        const char* second = "World!";
        
        // Owned
        option_charp greeting = str_alloc_concat(first, second);
        if (option_charp_is_some(greeting)) {
            printf("%s\n", option_charp_unwrap(greeting));
            str_free(option_charp_unwrap(greeting));
        }
        
        // Borrowed
        char msg[128];
        if (str_concat_into(msg, sizeof(msg), first, second)) {
            printf("%s\n", msg);
        }
    }
    
    // Example 3: Substring extraction
    void example_substring(void) {
        const char* text = "Hello World";
        
        option_charp sub = str_alloc_sub(text, 6, 5);
        if (option_charp_is_some(sub)) {
            printf("Substring: %s\n", option_charp_unwrap(sub));
            str_free(option_charp_unwrap(sub));
        }
    }
    
    // Example 4: String comparison
    void example_comparison(void) {
        const char* cmd = "quit";
        
        if (str_equals(cmd, "quit")) {
            printf("Exiting...\n");
        }
        
        if (str_equals(cmd, "help")) {
            printf("Showing help...\n");
        }
    }
    
    // Example 5: Prefix checking
    void example_prefix(void) {
        const char* files[] = {
            "test_parser.c",
            "test_lexer.c",
            "main.c",
            "test_util.c"
        };
        
        for (int i = 0; i < 4; i++) {
            if (str_starts_with(files[i], "test_")) {
                printf("Test file: %s\n", files[i]);
            }
        }
    }
    
    // Example 6: Suffix checking
    void example_suffix(void) {
        const char* files[] = {
            "document.txt",
            "image.jpg",
            "data.txt",
            "script.sh"
        };
        
        for (int i = 0; i < 4; i++) {
            if (str_ends_with(files[i], ".txt")) {
                printf("Text file: %s\n", files[i]);
            }
        }
    }
    
    // Example 7: Building paths
    void example_path_building(void) {
        const char* dir = "/home/user/";
        const char* file = "document.txt";
        
        option_charp path = str_alloc_concat(dir, file);
        if (option_charp_is_some(path)) {
            printf("Full path: %s\n", option_charp_unwrap(path));
            str_free(option_charp_unwrap(path));
        }
    }
    
    // Example 8: Error messages
    void example_error_messages(void) {
        const char* error_code = "404";
        const char* error_msg = "Not Found";
        
        char message[256];
        if (str_concat_into(message, sizeof(message), error_code, ": ")) {
            size_t len = strlen(message);
            if (str_copy_into(message + len, sizeof(message) - len, error_msg)) {
                printf("Error: %s\n", message);
            }
        }
    }
    
    // Example 9: Dynamic vs static allocation
    void example_allocation_choice(void) {
        // Known size - use buffer
        char stack_buf[64];
        if (str_copy_into(stack_buf, sizeof(stack_buf), "short")) {
            printf("Stack: %s\n", stack_buf);
        }
        
        // Unknown size - use allocation
        const char* unknown = get_user_input();
        option_charp heap_str = str_alloc_copy(unknown);
        if (option_charp_is_some(heap_str)) {
            printf("Heap: %s\n", option_charp_unwrap(heap_str));
            str_free(option_charp_unwrap(heap_str));
        }
    }
    
    // Example 10: File extension extraction
    void example_extension(void) {
        const char* filename = "document.txt";
        const char* dot = strrchr(filename, '.');
        
        if (dot) {
            size_t ext_start = dot - filename + 1;
            option_charp ext = str_alloc_sub(filename, ext_start, strlen(filename));
            if (option_charp_is_some(ext)) {
                printf("Extension: %s\n", option_charp_unwrap(ext));
                str_free(option_charp_unwrap(ext));
            }
        }
    }
    
    // Example 11: URL parsing
    void example_url_parsing(void) {
        const char* url = "https://example.com/path";
        
        if (str_starts_with(url, "https://")) {
            printf("Secure connection\n");
        } else if (str_starts_with(url, "http://")) {
            printf("Insecure connection\n");
        }
    }
    
    // Example 12: Command processing
    void example_command_processing(void) {
        const char* commands[] = {
            "/help",
            "/quit",
            "message",
            "/status"
        };
        
        for (int i = 0; i < 4; i++) {
            if (str_starts_with(commands[i], "/")) {
                option_charp cmd = str_alloc_sub(commands[i], 1, strlen(commands[i]));
                if (option_charp_is_some(cmd)) {
                    printf("Command: %s\n", option_charp_unwrap(cmd));
                    str_free(option_charp_unwrap(cmd));
                }
            } else {
                printf("Message: %s\n", commands[i]);
            }
        }
    }
    
    // Example 13: Configuration handling
    void example_config(void) {
        const char* key = "database.host";
        const char* value = "localhost";
        
        char config_line[256];
        if (str_concat_into(config_line, sizeof(config_line), key, " = ")) {
            size_t len = strlen(config_line);
            if (str_copy_into(config_line + len, sizeof(config_line) - len, value)) {
                printf("Config: %s\n", config_line);
            }
        }
    }
    
    // Example 14: String normalization
    void example_normalization(void) {
        const char* input = "  USER INPUT  ";
        
        option_charp copy = str_alloc_copy(input);
        if (option_charp_is_some(copy)) {
            char* str = option_charp_unwrap(copy);
            
            // Can modify owned copy
            str_trim_whitespace_inplace(str);
            
            printf("Normalized: '%s'\n", str);
            str_free(str);
        }
    }
    
    // Example 15: Multiple string operations
    void example_multiple_operations(void) {
        const char* first = "Hello";
        const char* second = "World";
        
        // Concatenate
        option_charp full = str_alloc_concat(first, " ");
        if (option_charp_is_none(full)) return;
        
        char* temp = option_charp_unwrap(full);
        option_charp final = str_alloc_concat(temp, second);
        str_free(temp);
        
        if (option_charp_is_some(final)) {
            char* result = option_charp_unwrap(final);
            printf("Result: %s\n", result);
            
            // Extract substring
            option_charp sub = str_alloc_sub(result, 0, 5);
            if (option_charp_is_some(sub)) {
                printf("First word: %s\n", option_charp_unwrap(sub));
                str_free(option_charp_unwrap(sub));
            }
            
            str_free(result);
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_STRING_H */
