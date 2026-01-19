// util/str_join.h
#ifndef CANON_UTIL_STR_JOIN_H
#define CANON_UTIL_STR_JOIN_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "core/memory.h"
#include "semantics/option.h"

/**
 * @file str_join.h
 * @brief Safe, explicit and flexible string joining utilities
 *
 * Provides efficient string concatenation operations with multiple allocation
 * strategies. Implements both zero-allocation buffer-based joining and heap
 * allocation variants, along with split-rejoin utilities for delimiter
 * transformation.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, size_t)
 *   - Depends on memory.h from this library (for mem_alloc, mem_copy, mem_free)
 *   - Uses standard string.h (strlen)
 *   - No platform-specific features
 *   - Works on any architecture (no alignment requirements)
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 *                Safe to call from multiple threads simultaneously as long as
 *                different buffers are used. Input strings must not be modified
 *                during operation.
 *
 * Performance:
 *   - Time complexity: O(n) where n = total length of all input strings
 *   - Space complexity: O(1) for buffer-based, O(n) for allocating variants
 *   - Single-pass concatenation (no redundant copying)
 *   - Allocation variants: one malloc, one length calculation pass
 *   - Split operations: O(n) single pass through input
 *   - No hidden allocations in non-allocating functions
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit allocation — function names tell you what allocates
 * - Zero hidden surprises — truncation/failure always reported
 * - Const-correct — input strings never modified
 * - Buffer ownership clear — caller knows who owns memory
 * - Null-safe throughout — graceful handling of NULL inputs
 * - Bounds-checked — buffer overflow impossible
 * - Always null-terminated — even on failure, dest[0] = '\0'
 * - No silent truncation — returns false/None on insufficient space
 * - Composable design — split and join work together seamlessly
 * - Multiple allocation strategies for different use cases
 *
 * Allocation strategies:
 * ────────────────────────────────────────────────────────────────────────────
 * This library provides two distinct approaches to string joining:
 *
 * 1. BUFFER-BASED (Zero allocation):
 *    Function: str_join()
 *    - Caller provides destination buffer
 *    - No heap allocation whatsoever
 *    - Perfect for: embedded systems, stack buffers, known max size
 *    - Returns: bool (success/failure)
 *    - Failure: buffer too small, NULL inputs
 *    - Cleanup: none needed
 *
 * 2. HEAP-ALLOCATED (Dynamic allocation):
 *    Function: str_alloc_join()
 *    - Allocates exact size needed
 *    - Perfect for: unknown sizes, dynamic data
 *    - Returns: Option<char*> (Some or None)
 *    - Success: caller MUST free with str_free() or mem_free()
 *    - Failure: allocation failed, NULL inputs
 *    - Cleanup: caller responsible
 *
 * Choose buffer-based when:
 *   - Maximum size is known at compile time
 *   - Want to avoid heap allocation
 *   - Working in embedded/real-time systems
 *   - Using stack-allocated buffers
 *
 * Choose heap-allocated when:
 *   - Size unknown until runtime
 *   - Result needs to outlive function scope
 *   - Don't want to guess buffer size
 *   - Building dynamic data structures
 *
 * String joining behavior:
 * ────────────────────────────────────────────────────────────────────────────
 * All join functions concatenate strings with a separator between elements:
 *
 * Pattern: parts[0] + sep + parts[1] + sep + ... + parts[n-1]
 *
 * Important behaviors:
 * - Separator appears BETWEEN elements only (not before first or after last)
 * - Empty separator ("" or NULL) → simple concatenation with no delimiter
 * - Empty strings in parts[] → preserved (contribute nothing but still counted)
 * - Zero count → produces empty string ""
 * - NULL in parts[] array → error (returns false/None)
 * - NULL separator → treated as empty string ""
 *
 * Examples:
 *   join(["a", "b", "c"], ", ") → "a, b, c"
 *   join(["a", "b", "c"], "")   → "abc"
 *   join(["a", "b", "c"], NULL) → "abc"
 *   join(["a", "", "c"], ", ")  → "a, , c"
 *   join([], ", ")              → ""
 *
 * Buffer sizing and truncation:
 * ────────────────────────────────────────────────────────────────────────────
 * Buffer-based functions (str_join) require accurate size calculation:
 *
 * Required size = sum(strlen(parts[i])) + (count-1) * strlen(sep) + 1
 *
 * The "+1" is for the null terminator. If the buffer is too small, the
 * function returns false and sets dest[0] = '\0'. NO partial results.
 *
 * Example calculation:
 * ```c
 * parts = ["hello", "world"]  // 5 + 5 = 10
 * sep = ", "                   // 2
 * count = 2
 * 
 * size = 10 + (2-1)*2 + 1 = 10 + 2 + 1 = 13 bytes needed
 * ```
 *
 * Safe buffer sizing pattern:
 * ```c
 * // Calculate size first
 * size_t total = 1;  // null terminator
 * for (size_t i = 0; i < count; i++) {
 *     total += strlen(parts[i]);
 *     if (i + 1 < count) total += strlen(sep);
 * }
 * 
 * // Allocate exact size
 * char* buf = malloc(total);
 * str_join(buf, total, parts, count, sep);
 * ```
 *
 * Or just use str_alloc_join() which does this automatically!
 *
 * Error handling philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * Buffer-based functions (str_join):
 * - Return false on ANY error condition
 * - Set dest[0] = '\0' on failure (safe to use, just empty)
 * - Never partially fill buffer (all-or-nothing)
 * - Check return value before using result
 *
 * Heap-based functions (str_alloc_join):
 * - Return None on ANY error condition
 * - No memory allocated on None
 * - Must check with option_charp_is_some() before unwrapping
 * - Unwrapping None is undefined behavior
 *
 * Error conditions:
 * - NULL dest pointer (buffer-based)
 * - Zero dest_size (buffer-based)
 * - NULL parts array
 * - NULL element in parts[] array
 * - Buffer too small (buffer-based)
 * - Allocation failure (heap-based)
 *
 * All errors result in safe failure (no crashes, no undefined behavior).
 *
 * Split and rejoin pattern:
 * ────────────────────────────────────────────────────────────────────────────
 * This library provides utilities for non-destructive delimiter transformation:
 *
 * 1. str_split_to_parts() - Split string into borrowed views
 * 2. str_rejoin() - Split and rejoin with new separator in one call
 *
 * These enable safe delimiter replacement without modifying input:
 *
 * ```c
 * const char* csv = "apple,banana,cherry";
 * const char* parts[10];
 * char result[100];
 * 
 * // Transform CSV to space-separated
 * size_t count = str_split_to_parts(csv, ',', parts, 10);
 * str_join(result, sizeof(result), parts, count, " ");
 * // result = "apple banana cherry"
 * ```
 *
 * Or use the combined str_rejoin():
 * ```c
 * const char* csv = "apple,banana,cherry";
 * const char* parts[10];
 * char result[100];
 * 
 * str_rejoin(csv, ',', parts, 10, result, sizeof(result), " ");
 * // result = "apple banana cherry"
 * ```
 *
 * Important: Split views are BORROWED pointers into original string.
 * They remain valid only as long as the original string exists.
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - CSV/TSV generation from components
 * - Building file paths from components
 * - Constructing SQL queries with parameters
 * - Formatting display strings from data
 * - Building command-line arguments
 * - Creating URLs from parts
 * - Delimiter transformation (CSV → space, etc.)
 * - Configuration file generation
 * - Log message formatting
 * - Building JSON/XML strings (for simple cases)
 * - Creating comma-separated lists
 * - Concatenating error messages
 *
 * NOT suitable for:
 * - Very large strings (gigabytes) - use streaming
 * - Complex formatting (use snprintf or dedicated formatters)
 * - Incremental building with many appends (use string builder)
 * - Localized text (use proper i18n)
 *
 * Memory ownership rules:
 * ────────────────────────────────────────────────────────────────────────────
 * BUFFER-BASED (str_join):
 * - Caller owns destination buffer
 * - Caller allocates and frees (if heap) or manages scope (if stack)
 * - Function never allocates
 * - Result valid as long as dest buffer exists
 *
 * HEAP-BASED (str_alloc_join):
 * - Function allocates, caller owns result
 * - Caller MUST free with str_free() or mem_free()
 * - Forgetting to free = memory leak
 * - Result valid until freed
 *
 * SPLIT VIEWS (str_split_to_parts):
 * - Input string owns data
 * - Function returns borrowed pointers into input
 * - Views valid only while input string exists
 * - DO NOT free individual parts
 * - DO NOT modify through these pointers (they're const)
 *
 * Usage examples:
 *
 * Basic buffer-based joining:
 * ```c
 * const char* words[] = {"hello", "world", "2026"};
 * char buf[64];
 * 
 * if (str_join(buf, sizeof(buf), words, 3, " ")) {
 *     printf("%s\n", buf);  // → "hello world 2026"
 * } else {
 *     fprintf(stderr, "Buffer too small\n");
 * }
 * ```
 *
 * Heap-allocated joining:
 * ```c
 * const char* items[] = {"apple", "banana", "cherry"};
 * option_charp result = str_alloc_join(items, 3, ", ");
 * 
 * if (option_charp_is_some(result)) {
 *     char* str = option_charp_unwrap(result);
 *     printf("%s\n", str);  // → "apple, banana, cherry"
 *     str_free(str);  // MUST free!
 * } else {
 *     fprintf(stderr, "Join failed\n");
 * }
 * ```
 *
 * Split and rejoin:
 * ```c
 * const char* csv = "red,green,blue";
 * const char* parts[10];
 * char output[100];
 * 
 * str_rejoin(csv, ',', parts, 10, output, sizeof(output), " ");
 * printf("%s\n", output);  // → "red green blue"
 * ```
 */

/**
 * @brief Option type alias for string results
 *
 * Used by allocating functions to return optional heap-allocated strings.
 * - Some(char*): Success, caller must free
 * - None: Failure, no allocation occurred
 */
CANON_C_DEFINE_OPTION(charp, char*)

/* ────────────────────────────────────────────────────────────────────────────
   Zero-allocation join — caller provides buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Joins array of strings into caller-provided buffer (no allocation)
 *
 * Concatenates an array of strings with a separator between each element,
 * writing the result into a caller-provided buffer. Never allocates memory.
 * Returns false if the buffer is too small or inputs are invalid.
 *
 * This is the preferred method when maximum size is known or when heap
 * allocation must be avoided (embedded systems, real-time code, etc.).
 *
 * Algorithm:
 *   - Validate inputs (dest, parts array)
 *   - Handle edge case: count == 0 → empty string
 *   - For each part:
 *     - Check if part + separator + null fit in remaining space
 *     - If not enough space, return false immediately
 *     - Copy part into buffer
 *     - If not last element, copy separator
 *   - Null-terminate result
 *   - Return true
 *
 * @param dest      Writable destination buffer (must not be NULL)
 * @param dest_size Size of dest buffer in bytes (must include space for '\0')
 * @param parts     Array of null-terminated input strings (must not be NULL)
 * @param count     Number of strings in parts array (0 is valid)
 * @param sep       Separator string (NULL treated as empty string "")
 *
 * @return          true if join succeeded and result fits in buffer
 *                  false if buffer too small, invalid input, or NULL parts
 *
 * Preconditions:
 *   - dest is valid pointer to writable buffer
 *   - dest_size > 0
 *   - If count > 0: parts is valid pointer to array
 *   - All parts[i] are null-terminated (no NULL elements)
 *   - sep is null-terminated if not NULL
 *
 * Postconditions:
 *   - If true: dest contains joined string, null-terminated
 *   - If false: dest[0] = '\0' (safe to use as empty string)
 *   - Input strings never modified (const-correct)
 *   - No memory allocated
 *
 * Joining pattern:
 *   - Result: parts[0] + sep + parts[1] + sep + ... + parts[count-1]
 *   - Separator appears between elements only (not before/after)
 *   - Empty separator or NULL → simple concatenation
 *
 * Error conditions (returns false):
 *   - dest is NULL
 *   - dest_size is 0
 *   - parts is NULL (when count > 0)
 *   - Any parts[i] is NULL
 *   - Result doesn't fit in dest_size
 *
 * Performance:
 *   - Time: O(n) where n = total characters in all parts + separators
 *   - Space: O(1) - no allocations, only stack variables
 *   - Single pass through all strings
 *   - Minimal memory copying
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Basic joining:
 * ```c
 * const char* words[] = {"hello", "world"};
 * char buf[64];
 * 
 * if (str_join(buf, sizeof(buf), words, 2, " ")) {
 *     printf("%s\n", buf);  // → "hello world"
 * }
 * ```
 *
 * Example - CSV construction:
 * ```c
 * const char* fields[] = {"John", "Doe", "35", "Engineer"};
 * char csv[256];
 * 
 * str_join(csv, sizeof(csv), fields, 4, ",");
 * // csv = "John,Doe,35,Engineer"
 * ```
 *
 * Example - Path building:
 * ```c
 * const char* components[] = {"home", "user", "documents", "file.txt"};
 * char path[512];
 * 
 * str_join(path, sizeof(path), components, 4, "/");
 * // path = "home/user/documents/file.txt"
 * ```
 *
 * Example - Empty separator:
 * ```c
 * const char* parts[] = {"a", "b", "c"};
 * char buf[32];
 * 
 * str_join(buf, sizeof(buf), parts, 3, NULL);  // or ""
 * // buf = "abc"
 * ```
 *
 * Example - Error handling:
 * ```c
 * const char* parts[] = {"very", "long", "string", "list", ...};
 * char buf[10];  // Too small
 * 
 * if (!str_join(buf, sizeof(buf), parts, 100, " ")) {
 *     fprintf(stderr, "Buffer too small\n");
 *     // buf[0] == '\0' - safe to use as empty string
 * }
 * ```
 *
 * Example - Building SQL:
 * ```c
 * const char* columns[] = {"id", "name", "email", "created_at"};
 * char select[256];
 * 
 * str_join(select, sizeof(select), columns, 4, ", ");
 * printf("SELECT %s FROM users;\n", select);
 * // → SELECT id, name, email, created_at FROM users;
 * ```
 *
 * Example - Stack buffer sizing:
 * ```c
 * // Known maximum: 3 words, max 20 chars each, separator 2 chars
 * // Size = 3*20 + 2*2 + 1 = 65 bytes
 * char buf[65];
 * str_join(buf, sizeof(buf), words, 3, ", ");
 * ```
 *
 * Common pitfalls:
 * - Undersizing buffer (calculate: sum(parts) + (count-1)*sep + 1)
 * - Not checking return value (buffer might be truncated)
 * - Passing NULL in parts[] array (causes failure)
 * - Forgetting null terminator in size calculation
 *
 * Buffer size calculation:
 * ```c
 * // Safe way to calculate needed size:
 * size_t calculate_join_size(const char** parts, size_t count, const char* sep) {
 *     size_t total = 1;  // null terminator
 *     size_t sep_len = sep ? strlen(sep) : 0;
 *     
 *     for (size_t i = 0; i < count; i++) {
 *         total += strlen(parts[i]);
 *         if (i + 1 < count) total += sep_len;
 *     }
 *     return total;
 * }
 * ```
 */
static inline bool str_join(
    char* dest,
    size_t dest_size,
    const char** parts,
    size_t count,
    const char* sep
) {
    // Validate destination buffer
    if (!dest || dest_size == 0) {
        return false;
    }

    // Handle empty array - return empty string
    if (count == 0) {
        dest[0] = '\0';
        return true;
    }

    // Validate parts array
    if (!parts) {
        dest[0] = '\0';
        return false;
    }

    // Treat NULL separator as empty string
    sep = sep ? sep : "";
    const size_t sep_len = strlen(sep);

    size_t pos = 0;

    // Join all parts with separator
    for (size_t i = 0; i < count; ++i) {
        // Check for NULL part
        if (!parts[i]) {
            dest[0] = '\0';
            return false;
        }

        const size_t part_len = strlen(parts[i]);

        // Calculate space needed for this part
        // Need: part_len + separator (if not last) + null terminator
        size_t needed = part_len + 1;
        if (i + 1 < count && sep_len > 0) {
            needed += sep_len;
        }

        // Check if remaining space is sufficient
        if (pos + needed > dest_size) {
            dest[0] = '\0';
            return false;
        }

        // Copy part into buffer
        mem_copy(dest + pos, parts[i], part_len);
        pos += part_len;

        // Add separator if not last element
        if (i + 1 < count && sep_len > 0) {
            mem_copy(dest + pos, sep, sep_len);
            pos += sep_len;
        }
    }

    // Null-terminate result
    dest[pos] = '\0';
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Allocating join — heap allocated result
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates and joins strings — caller owns the result
 *
 * Dynamically allocates a buffer of exact size needed and joins strings
 * into it. Returns Option containing the result. Caller is responsible
 * for freeing the returned string.
 *
 * This is the convenient option when size is not known at compile time
 * or when dynamic allocation is acceptable. The function calculates the
 * exact size needed and allocates precisely that amount.
 *
 * Algorithm:
 *   - Validate inputs
 *   - First pass: calculate total size needed
 *   - Allocate buffer of exact size
 *   - Call str_join() to perform actual joining
 *   - Return Option with result or None on failure
 *
 * @param parts  Array of null-terminated input strings (must not be NULL if count > 0)
 * @param count  Number of strings to join (0 returns empty string)
 * @param sep    Separator string (NULL treated as empty string "")
 *
 * @return       Some(heap-allocated joined string) on success
 *               None on allocation failure, invalid input, or NULL parts
 *
 * Preconditions:
 *   - If count > 0: parts is valid pointer to array
 *   - All parts[i] are null-terminated (no NULL elements)
 *   - sep is null-terminated if not NULL
 *
 * Postconditions:
 *   - If Some: returned string is heap-allocated, null-terminated
 *   - If Some: caller MUST free with str_free() or mem_free()
 *   - If None: no memory was allocated
 *   - Input strings never modified
 *
 * Memory ownership:
 *   - Function allocates exact size: sum(parts) + (count-1)*sep + 1
 *   - Caller owns result and must free
 *   - Use str_free() or mem_free() to deallocate
 *   - Forgetting to free causes memory leak
 *
 * Error conditions (returns None):
 *   - parts is NULL (when count > 0)
 *   - Any parts[i] is NULL
 *   - Memory allocation failed
 *   - Internal join failed (should never happen with correct size)
 *
 * Performance:
 *   - Time: O(n) where n = total characters (two passes: size calc + copy)
 *   - Space: O(n) - allocates exactly n bytes for result
 *   - One malloc call
 *   - Optimal allocation (no waste)
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Basic usage:
 * ```c
 * const char* items[] = {"apple", "banana", "cherry"};
 * option_charp result = str_alloc_join(items, 3, ", ");
 * 
 * if (option_charp_is_some(result)) {
 *     char* str = option_charp_unwrap(result);
 *     printf("%s\n", str);  // → "apple, banana, cherry"
 *     str_free(str);  // MUST free!
 * } else {
 *     fprintf(stderr, "Join failed\n");
 * }
 * ```
 *
 * Example - Building dynamic list:
 * ```c
 * option_charp list = str_alloc_join(names, name_count, "; ");
 * if (option_charp_is_some(list)) {
 *     save_to_file(option_charp_unwrap(list));
 *     str_free(option_charp_unwrap(list));
 * }
 * ```
 *
 * Example - With cleanup pattern:
 * ```c
 * const char* parts[] = {"part1", "part2", "part3"};
 * option_charp result = str_alloc_join(parts, 3, "-");
 * 
 * if (option_charp_is_some(result)) {
 *     char* joined = option_charp_unwrap(result);
 *     SCOPE_DEFER { str_free(joined); };  // Automatic cleanup
 *     
 *     process_string(joined);
 *     // joined freed automatically at scope exit
 * }
 * ```
 *
 * Example - Empty array:
 * ```c
 * option_charp result = str_alloc_join(NULL, 0, ",");
 * // result = Some("") - returns empty string
 * if (option_charp_is_some(result)) {
 *     char* str = option_charp_unwrap(result);
 *     assert(strlen(str) == 0);  // Empty string
 *     str_free(str);
 * }
 * ```
 *
 * Example - URL construction:
 * ```c
 * const char* url_parts[] = {
 *     "https://example.com",
 *     "api",
 *     "v2",
 *     "users",
 *     "123"
 * };
 * 
 * option_charp url = str_alloc_join(url_parts, 5, "/");
 * if (option_charp_is_some(url)) {
 *     make_request(option_charp_unwrap(url));
 *     str_free(option_charp_unwrap(url));
 * }
 * ```
 *
 * Example - Error handling:
 * ```c
 * const char* parts[] = {"a", NULL, "c"};  // NULL in array!
 * option_charp result = str_alloc_join(parts, 3, ",");
 * 
 * if (option_charp_is_none(result)) {
 *     fprintf(stderr, "Join failed - check for NULL parts\n");
 * }
 * ```
 *
 * Example - Building command:
 * ```c
 * const char* args[] = {"git", "commit", "-m", "Initial commit"};
 * option_charp cmd = str_alloc_join(args, 4, " ");
 * 
 * if (option_charp_is_some(cmd)) {
 *     system(option_charp_unwrap(cmd));
 *     str_free(option_charp_unwrap(cmd));
 * }
 * ```
 *
 * Memory leak example (WRONG):
 * ```c
 * option_charp result = str_alloc_join(parts, count, ",");
 * if (option_charp_is_some(result)) {
 *     printf("%s\n", option_charp_unwrap(result));
 *     // FORGOT TO FREE - memory leak!
 * }
 * ```
 *
 * Correct cleanup:
 * ```c
 * option_charp result = str_alloc_join(parts, count, ",");
 * if (option_charp_is_some(result)) {
 *     char* str = option_charp_unwrap(result);
 *     printf("%s\n", str);
 *     str_free(str);  // Always free!
 * }
 * ```
 *
 * Common pitfalls:
 * - Forgetting to free result (memory leak)
 * - Not checking option before unwrapping (undefined behavior)
 * - Freeing result multiple times (double free)
 * - Using result after freeing (use-after-free)
 */
static inline option_charp str_alloc_join(
    const char** parts,
    size_t count,
    const char* sep
) {
    // Handle empty array - return empty string
    if (!parts || count == 0) {
        return str_alloc_copy("");
    }

    // Treat NULL separator as empty string
    sep = sep ? sep : "";
    const size_t sep_len = strlen(sep);

    // First pass: calculate total required size
    size_t total_len = 0;
    for (size_t i = 0; i < count; ++i) {
        // Check for NULL part
        if (!parts[i]) {
            return option_charp_none();
        }
        
        total_len += strlen(parts[i]);
        
        // Add separator length if not last element
        if (i + 1 < count && sep_len > 0) {
            total_len += sep_len;
        }
    }
    total_len += 1;  // Null terminator

    // Allocate buffer of exact size needed
    char* buffer = (char*)mem_alloc(total_len);
    if (!buffer) {
        return option_charp_none();
    }

    // Perform join into allocated buffer
    if (str_join(buffer, total_len, parts, count, sep)) {
        return option_charp_some(buffer);
    }

    // Should never happen with correct size calculation, but safety first
    mem_free(buffer);
    return option_charp_none();
}

/* ────────────────────────────────────────────────────────────────────────────
   Split + join helpers (non-destructive transformations)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits string by single character delimiter (non-allocating, borrowed views)
 *
 * Splits a string into an array of borrowed pointers (views) at delimiter
 * boundaries. Does not allocate memory or modify the input string. The
 * returned pointers point directly into the original string.
 *
 * This function skips consecutive delimiters and ignores leading/trailing
 * delimiters, producing only non-empty parts.
 *
 * IMPORTANT: The returned parts are borrowed views into the original string.
 * They remain valid only as long as the original string exists and is not
 * modified.
 *
 * Algorithm:
 *   - Validate inputs
 *   - For each character in input:
 *     - Skip leading delimiters
 *     - Mark start of part
 *     - Scan until next delimiter
 *     - Store pointer to part (if space in buffer)
 *     - Count part (even if buffer full)
 *   - Return total count found
 *
 * @param s         Null-terminated input string (must not be NULL)
 * @param delim     Single character delimiter to split on
 * @param out_parts Buffer to store const char* pointers (views into s)
 * @param max_parts Maximum number of parts the buffer can hold
 *
 * @return          Actual number of parts found (may exceed max_parts)
 *                  Returns 0 on invalid input
 *
 * Preconditions:
 *   - s is null-terminated string
 *   - out_parts is valid buffer of size max_parts
 *   - max_parts > 0 for meaningful results
 *
 * Postconditions:
 *   - out_parts[0..min(return, max_parts)-1] contain pointers into s
 *   - Pointers are valid as long as s exists
 *   - Input string s is unchanged
 *   - Return value indicates total parts found (may exceed max_parts)
 *
 * Splitting behavior:
 *   - Consecutive delimiters treated as single delimiter
 *   - Leading delimiters skipped
 *   - Trailing delimiters skipped
 *   - Empty string produces 0 parts
 *   - No delimiters produces 1 part (entire string)
 *
 * Important notes:
 *   - Parts are NOT null-terminated (they point into s)
 *   - Parts remain valid only while s exists
 *   - DO NOT free individual parts
 *   - DO NOT modify through these pointers
 *   - If return > max_parts, output buffer was too small
 *
 * Performance:
 *   - Time: O(n) where n = strlen(s)
 *   - Space: O(1) - no allocations, only pointer storage
 *   - Single pass through string
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Basic splitting:
 * ```c
 * const char* csv = "apple,banana,cherry";
 * const char* parts[10];
 * 
 * size_t count = str_split_to_parts(csv, ',', parts, 10);
 * // count = 3
 * // parts[0] points to "apple,banana,cherry" (at 'a')
 * // parts[1] points to "banana,cherry" (at 'b')
 * // parts[2] points to "cherry" (at 'c')
 * ```
 *
 * Example - Using with str_join:
 * ```c
 * const char* input = "one,two,three";
 * const char* parts[10];
 * char output[100];
 * 
 * size_t count = str_split_to_parts(input, ',', parts, 10);
 * str_join(output, sizeof(output), parts, count, " ");
 * // output = "one two three"
 * ```
 *
 * Example - Consecutive delimiters:
 * ```c
 * const char* s = "a,,b,,,c";
 * const char* parts[10];
 * 
 * size_t count = str_split_to_parts(s, ',', parts, 10);
 * // count = 3 (empty parts skipped)
 * // parts: "a", "b", "c"
 * ```
 *
 * Example - Leading/trailing delimiters:
 * ```c
 * const char* s = ",,,abc,def,,,";
 * const char* parts[10];
 * 
 * size_t count = str_split_to_parts(s, ',', parts, 10);
 * // count = 2
 * // parts: "abc", "def"
 * ```
 *
 * Example - Buffer too small:
 * ```c
 * const char* s = "a,b,c,d,e";
 * const char* parts[3];  // Only room for 3
 * 
 * size_t count = str_split_to_parts(s, ',', parts, 3);
 * // count = 5 (total found)
 * // parts[0..2] contain first 3 parts
 * // parts[3..4] were not stored (buffer too small)
 * 
 * if (count > 3) {
 *     printf("Warning: only stored %d of %zu parts\n", 3, count);
 * }
 * ```
 *
 * Example - Lifetime of views:
 * ```c
 * const char* get_first_part(void) {
 *     char local[] = "a,b,c";
 *     const char* parts[10];
 *     
 *     str_split_to_parts(local, ',', parts, 10);
 *     return parts[0];  // WRONG! local is destroyed
 * }
 * // Returned pointer is dangling
 * 
 * // Correct:
 * void process_parts(const char* input) {
 *     const char* parts[10];
 *     size_t count = str_split_to_parts(input, ',', parts, 10);
 *     
 *     for (size_t i = 0; i < count; i++) {
 *         process(parts[i]);  // OK - input still alive
 *     }
 * }
 * ```
 *
 * Common pitfalls:
 * - Returning parts when input string goes out of scope
 * - Freeing individual parts (they're not allocated)
 * - Assuming parts are null-terminated (they're not)
 * - Modifying through const char* (undefined behavior)
 * - Not checking return value vs max_parts
 */
static inline size_t str_split_to_parts(
    const char* s,
    char delim,
    const char** out_parts,
    size_t max_parts
) {
    // Validate inputs
    if (!s || !out_parts || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    const char* current = s;

    while (*current) {
        // Skip leading delimiters
        while (*current == delim) {
            ++current;
        }
        
        // Check if we've reached end of string
        if (*current == '\0') {
            break;
        }

        // Store pointer to this part if buffer has space
        if (count < max_parts) {
            out_parts[count] = current;
        }
        count++;

        // Skip until next delimiter or end
        while (*current && *current != delim) {
            ++current;
        }
    }

    return count;
}

/**
 * @brief Split string by delimiter and rejoin with new separator
 *
 * Combines splitting and joining in a single operation. Useful for delimiter
 * transformation without modifying the original string.
 *
 * This is a convenience function that combines str_split_to_parts() and
 * str_join() into one call, using the provided parts_buf as temporary storage.
 *
 * Algorithm:
 *   - Split input string using str_split_to_parts()
 *   - Join parts using str_join() with new separator
 *   - Return success/failure
 *
 * @param s           Input string to transform (must not be NULL)
 * @param delim       Original delimiter to split on
 * @param parts_buf   Temporary buffer for split parts (borrowed views)
 * @param max_parts   Size of parts_buf
 * @param dest        Output buffer for rejoined string
 * @param dest_size   Size of destination buffer
 * @param new_sep     New separator to use (NULL = empty string)
 *
 * @return            true if rejoin succeeded
 *                    false on truncation, invalid input, or too many parts
 *
 * Preconditions:
 *   - s is null-terminated string
 *   - parts_buf has space for at least max_parts pointers
 *   - dest has space for at least dest_size bytes
 *   - new_sep is null-terminated if not NULL
 *
 * Postconditions:
 *   - If true: dest contains rejoined string with new separator
 *   - If false: dest[0] = '\0' (safe to use as empty string)
 *   - Input string s never modified
 *   - parts_buf may contain temporary views (no cleanup needed)
 *
 * Performance:
 *   - Time: O(n) where n = strlen(s)
 *   - Space: O(1) - no allocations (uses provided buffers)
 *   - Two passes: split then join
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - CSV to space-separated:
 * ```c
 * const char* csv = "apple,banana,cherry";
 * const char* parts[10];
 * char result[100];
 * 
 * if (str_rejoin(csv, ',', parts, 10, result, sizeof(result), " ")) {
 *     printf("%s\n", result);  // → "apple banana cherry"
 * }
 * ```
 *
 * Example - Path normalization:
 * ```c
 * const char* windows_path = "C:\\Users\\Alice\\Documents";
 * const char* parts[20];
 * char unix_path[256];
 * 
 * str_rejoin(windows_path, '\\', parts, 20, unix_path, sizeof(unix_path), "/");
 * // unix_path = "C:/Users/Alice/Documents"
 * ```
 *
 * Example - Collapsing whitespace:
 * ```c
 * const char* text = "word1  word2   word3";
 * const char* parts[10];
 * char normalized[100];
 * 
 * str_rejoin(text, ' ', parts, 10, normalized, sizeof(normalized), " ");
 * // normalized = "word1 word2 word3"
 * ```
 *
 * Example - Error handling:
 * ```c
 * const char* long_csv = "a,b,c,d,e,f,g,h,i,j,k";
 * const char* parts[5];  // Too small
 * char result[100];
 * 
 * if (!str_rejoin(long_csv, ',', parts, 5, result, sizeof(result), ";")) {
 *     fprintf(stderr, "Too many parts or buffer too small\n");
 * }
 * ```
 *
 * Common use cases:
 * - CSV/TSV delimiter transformation
 * - Path separator normalization
 * - Whitespace collapsing
 * - Format conversion
 *
 * Common pitfalls:
 * - parts_buf too small for number of delimited parts
 * - dest buffer too small for rejoined result
 * - Not checking return value
 */
static inline bool str_rejoin(
    const char* s,
    char delim,
    const char** parts_buf,
    size_t max_parts,
    char* dest,
    size_t dest_size,
    const char* new_sep
) {
    // Split string into parts
    size_t count = str_split_to_parts(s, delim, parts_buf, max_parts);
    
    // Rejoin with new separator
    return str_join(dest, dest_size, parts_buf, count, new_sep);
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/str_join.h"
    #include <stdio.h>
    
    // Example 1: Basic string joining
    void example_basic(void) {
        const char* words[] = {"hello", "world", "2026"};
        char result[64];
        
        if (str_join(result, sizeof(result), words, 3, " ")) {
            printf("%s\n", result);  // → hello world 2026
        }
    }
    
    // Example 2: CSV generation
    void example_csv(void) {
        const char* fields[] = {"Name", "Age", "City"};
        char header[256];
        
        str_join(header, sizeof(header), fields, 3, ",");
        printf("%s\n", header);  // → Name,Age,City
        
        const char* row[] = {"Alice", "30", "NYC"};
        char data[256];
        str_join(data, sizeof(data), row, 3, ",");
        printf("%s\n", data);  // → Alice,30,NYC
    }
    
    // Example 3: Path construction
    void example_paths(void) {
        const char* unix_parts[] = {"home", "user", "docs", "file.txt"};
        char unix_path[512];
        str_join(unix_path, sizeof(unix_path), unix_parts, 4, "/");
        printf("Unix: %s\n", unix_path);
        
        const char* win_parts[] = {"C:", "Users", "Alice", "file.txt"};
        char win_path[512];
        str_join(win_path, sizeof(win_path), win_parts, 4, "\\");
        printf("Windows: %s\n", win_path);
    }
    
    // Example 4: Heap-allocated join
    void example_alloc(void) {
        const char* items[] = {"apple", "banana", "cherry", "date"};
        option_charp result = str_alloc_join(items, 4, ", ");
        
        if (option_charp_is_some(result)) {
            char* str = option_charp_unwrap(result);
            printf("Fruits: %s\n", str);
            str_free(str);  // Don't forget!
        }
    }
    
    // Example 5: SQL query building
    void example_sql(void) {
        const char* columns[] = {"id", "name", "email", "created_at"};
        char select[256];
        
        str_join(select, sizeof(select), columns, 4, ", ");
        printf("SELECT %s FROM users;\n", select);
        
        const char* conditions[] = {"active = 1", "age > 18", "verified = true"};
        char where[256];
        str_join(where, sizeof(where), conditions, 3, " AND ");
        printf("WHERE %s;\n", where);
    }
    
    // Example 6: Split and rejoin
    void example_split_rejoin(void) {
        const char* csv = "red,green,blue";
        const char* parts[10];
        char space_separated[100];
        
        size_t count = str_split_to_parts(csv, ',', parts, 10);
        str_join(space_separated, sizeof(space_separated), parts, count, " ");
        
        printf("CSV: %s\n", csv);
        printf("Space: %s\n", space_separated);
    }
    
    // Example 7: Delimiter transformation
    void example_rejoin(void) {
        const char* data = "apple,banana,cherry";
        const char* parts[10];
        char result[100];
        
        str_rejoin(data, ',', parts, 10, result, sizeof(result), " | ");
        printf("%s\n", result);  // → apple | banana | cherry
    }
    
    // Example 8: Error handling
    void example_error_handling(void) {
        const char* parts[] = {"very", "long", "list", "of", "many", "items"};
        char small_buf[10];  // Too small
        
        if (!str_join(small_buf, sizeof(small_buf), parts, 6, " ")) {
            fprintf(stderr, "Buffer too small\n");
            // small_buf[0] == '\0' - safe to use
        }
    }
    
    // Example 9: URL construction
    void example_url(void) {
        const char* base = "https://api.example.com";
        const char* path_parts[] = {"v2", "users", "123", "posts"};
        char path[256];
        
        str_join(path, sizeof(path), path_parts, 4, "/");
        printf("%s/%s\n", base, path);
    }
    
    // Example 10: Command building
    void example_command(void) {
        const char* args[] = {"docker", "run", "-it", "--rm", "ubuntu"};
        option_charp cmd = str_alloc_join(args, 5, " ");
        
        if (option_charp_is_some(cmd)) {
            printf("Running: %s\n", option_charp_unwrap(cmd));
            str_free(option_charp_unwrap(cmd));
        }
    }
    
    // Example 11: List formatting
    void example_list_formatting(void) {
        const char* names[] = {"Alice", "Bob", "Charlie"};
        char list[100];
        
        // Different separators
        str_join(list, sizeof(list), names, 3, ", ");
        printf("Comma: %s\n", list);
        
        str_join(list, sizeof(list), names, 3, " and ");
        printf("And: %s\n", list);
        
        str_join(list, sizeof(list), names, 3, " | ");
        printf("Pipe: %s\n", list);
    }
    
    // Example 12: Empty cases
    void example_empty_cases(void) {
        char buf[64];
        
        // Empty array
        if (str_join(buf, sizeof(buf), NULL, 0, ",")) {
            printf("Empty: '%s'\n", buf);  // → ''
        }
        
        // Empty separator
        const char* parts[] = {"a", "b", "c"};
        str_join(buf, sizeof(buf), parts, 3, "");
        printf("No sep: '%s'\n", buf);  // → 'abc'
        
        // NULL separator (treated as empty)
        str_join(buf, sizeof(buf), parts, 3, NULL);
        printf("NULL sep: '%s'\n", buf);  // → 'abc'
    }
    
    // Example 13: Configuration file generation
    void example_config(void) {
        const char* keys[] = {"host", "port", "timeout", "retries"};
        const char* values[] = {"localhost", "8080", "30", "3"};
        
        for (int i = 0; i < 4; i++) {
            const char* pair[] = {keys[i], values[i]};
            char line[256];
            str_join(line, sizeof(line), pair, 2, " = ");
            printf("%s\n", line);
        }
    }
    
    // Example 14: Breadcrumb navigation
    void example_breadcrumbs(void) {
        const char* path[] = {"Home", "Products", "Electronics", "Phones"};
        char breadcrumb[256];
        
        str_join(breadcrumb, sizeof(breadcrumb), path, 4, " > ");
        printf("%s\n", breadcrumb);
        // → Home > Products > Electronics > Phones
    }
    
    // Example 15: Tag list generation
    void example_tags(void) {
        const char* tags[] = {"c", "programming", "tutorial", "2026"};
        
        // HTML tags
        char html[256];
        const char* html_parts[8];
        for (int i = 0; i < 4; i++) {
            html_parts[i*2] = "<span>";
            html_parts[i*2+1] = tags[i];
        }
        option_charp result = str_alloc_join(html_parts, 8, "");
        if (option_charp_is_some(result)) {
            printf("HTML: %s\n", option_charp_unwrap(result));
            str_free(option_charp_unwrap(result));
        }
        
        // Hashtags
        char hashtags[256];
        str_join(hashtags, sizeof(hashtags), tags, 4, " #");
        printf("Tags: #%s\n", hashtags);
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_STR_JOIN_H */
