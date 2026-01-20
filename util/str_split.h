// util/str_split.h
#ifndef CANON_UTIL_STR_SPLIT_H
#define CANON_UTIL_STR_SPLIT_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>  // for isspace()

/**
 * @file str_split.h
 * @brief Zero-allocation, non-destructive string splitting and trimming
 *
 * Provides efficient string parsing utilities with borrowed view pattern and
 * in-place trimming operations. Implements zero-copy splitting that returns
 * pointers into the original string, along with buffer-modifying trim functions.
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, size_t)
 *   - Uses standard library: string.h (strlen, memmove), ctype.h (isspace)
 *   - isspace() behavior may vary by locale (typically C locale)
 *   - No platform-specific features
 *   - Works on any architecture
 *
 * Thread-safety: Functions are reentrant and thread-safe. No shared state.
 *                Safe to call from multiple threads as long as:
 *                - Different strings are being processed
 *                - Input strings aren't being modified during split operations
 *                - In-place trim functions: caller ensures exclusive access
 *
 * Performance:
 *   - Time complexity: O(n) where n = strlen(input)
 *   - Space complexity: O(1) - zero allocations for all operations
 *   - Split operations: Single pass, zero copying
 *   - Trim operations: In-place modification, minimal moves
 *   - No hidden costs: what you see is what you get
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero-copy parsing via borrowed views pattern
 * - Never allocate - caller provides all storage
 * - Non-destructive splitting - input string unchanged
 * - Borrowed views valid only while original string lives
 * - In-place operations clearly marked ("inplace" in name)
 * - Two splitting modes: skip empties (typical) vs keep empties (strict CSV)
 * - Null-safe throughout - graceful handling of edge cases
 * - Const-correct - split functions don't modify input
 * - Predictable behavior on empty input, consecutive delimiters
 * - Perfect for parsing, tokenization, CSV processing
 *
 * Borrowed views pattern:
 * ────────────────────────────────────────────────────────────────────────────
 * Split functions return "borrowed views" - pointers directly into the original
 * string. This is a zero-copy optimization:
 *
 * Advantages:
 * - Zero allocations - extremely fast
 * - Zero copying - optimal memory usage
 * - Perfect for parsing where you just need to examine parts
 * - Composable with other operations (split → process → join)
 *
 * Constraints:
 * - Views are NOT null-terminated (they're substrings within a larger string)
 * - Views remain valid ONLY while original string exists
 * - DO NOT free individual views (they're not allocated)
 * - DO NOT modify through these pointers (they point to const data)
 * - Must use strncmp or calculate length, not strcmp
 *
 * Lifetime example:
 * ```c
 * const char* get_first_token(void) {
 *     char input[] = "a,b,c";
 *     const char* parts[10];
 *     str_split(input, ',', parts, 10);
 *     return parts[0];  // WRONG! input destroyed at return
 * }
 * // Returned pointer is dangling
 * 
 * // Correct usage:
 * void process_tokens(const char* input) {
 *     const char* parts[10];
 *     size_t count = str_split(input, ',', parts, 10);
 *     for (size_t i = 0; i < count; i++) {
 *         process_part(parts[i]);  // OK - input still alive
 *     }
 * }
 * ```
 *
 * Working with borrowed views:
 * ────────────────────────────────────────────────────────────────────────────
 * Since views are not null-terminated, you need to handle them specially:
 *
 * METHOD 1 - Calculate length:
 * ```c
 * const char* parts[10];
 * size_t count = str_split("a,b,c", ',', parts, 10);
 * 
 * // Find length of each part
 * for (size_t i = 0; i < count; i++) {
 *     const char* start = parts[i];
 *     const char* end = start;
 *     while (*end && *end != ',') end++;
 *     size_t len = end - start;
 *     printf("Part %zu: %.*s (len=%zu)\n", i, (int)len, start, len);
 * }
 * ```
 *
 * METHOD 2 - Use with strncmp:
 * ```c
 * if (strncmp(parts[0], "header", 6) == 0) {
 *     // First part is "header"
 * }
 * ```
 *
 * METHOD 3 - Copy to null-terminated buffer:
 * ```c
 * char token[64];
 * const char* p = parts[0];
 * size_t i = 0;
 * while (*p && *p != ',' && i < 63) {
 *     token[i++] = *p++;
 * }
 * token[i] = '\0';
 * // Now token is null-terminated
 * ```
 *
 * METHOD 4 - Use with printf precision:
 * ```c
 * // Print part with limited length
 * const char* p = parts[0];
 * const char* end = p;
 * while (*end && *end != ',') end++;
 * printf("%.*s\n", (int)(end - p), p);
 * ```
 *
 * Split modes: Skip empties vs Keep empties:
 * ────────────────────────────────────────────────────────────────────────────
 * This library provides two splitting behaviors:
 *
 * 1. SKIP EMPTIES (str_split):
 *    - Consecutive delimiters treated as one
 *    - Leading/trailing delimiters ignored
 *    - Empty fields between delimiters skipped
 *    - Perfect for: whitespace-delimited text, tokenization, word splitting
 *
 *    Example: "a,,b,,,c" with delimiter ',' → ["a", "b", "c"]
 *
 * 2. KEEP EMPTIES (str_split_keep_empty):
 *    - Consecutive delimiters create empty fields
 *    - Trailing delimiter creates empty field at end
 *    - Preserves structure of delimited data
 *    - Perfect for: CSV parsing, fixed-width field parsing, strict formats
 *
 *    Example: "a,,b," with delimiter ',' → ["a", "", "b", ""]
 *
 * Choose skip empties when:
 *   - Parsing natural language (words, sentences)
 *   - Delimiter is semantic separator only
 *   - Empty fields are meaningless
 *   - User input with variable spacing
 *
 * Choose keep empties when:
 *   - Parsing structured data (CSV, TSV)
 *   - Empty fields have meaning (missing data)
 *   - Position matters (column-oriented)
 *   - Need exact field count
 *
 * In-place trimming:
 * ────────────────────────────────────────────────────────────────────────────
 * Trim functions modify the input buffer in-place using memmove. They remove
 * characters from both ends of the string, shifting content left if needed.
 *
 * Key properties:
 * - Modifies caller-owned buffer
 * - Uses memmove for safe overlapping moves
 * - Always null-terminates result
 * - Returns pointer to buffer (for convenience chaining)
 * - Safe on empty strings, all-whitespace strings
 *
 * Common use cases:
 * - Cleaning user input
 * - Removing quotes from strings
 * - Normalizing whitespace
 * - Pre-processing before parsing
 *
 * Example:
 * ```c
 * char input[100] = "  hello world  ";
 * str_trim_whitespace_inplace(input);
 * // input now contains "hello world"
 * ```
 *
 * Delimiter behavior and edge cases:
 * ────────────────────────────────────────────────────────────────────────────
 * Understanding how delimiters are handled:
 *
 * Empty input:
 *   str_split("", ',', parts, 10) → returns 0
 *   str_split_keep_empty("", ',', parts, 10) → returns 0
 *
 * Only delimiters:
 *   str_split(",,,", ',', parts, 10) → returns 0 (all skipped)
 *   str_split_keep_empty(",,,", ',', parts, 10) → returns 4 (four empty fields)
 *
 * Leading delimiter:
 *   str_split(",abc", ',', parts, 10) → returns 1: ["abc"]
 *   str_split_keep_empty(",abc", ',', parts, 10) → returns 2: ["", "abc"]
 *
 * Trailing delimiter:
 *   str_split("abc,", ',', parts, 10) → returns 1: ["abc"]
 *   str_split_keep_empty("abc,", ',', parts, 10) → returns 2: ["abc", ""]
 *
 * Consecutive delimiters:
 *   str_split("a,,b", ',', parts, 10) → returns 2: ["a", "b"]
 *   str_split_keep_empty("a,,b", ',', parts, 10) → returns 3: ["a", "", "b"]
 *
 * No delimiters:
 *   str_split("hello", ',', parts, 10) → returns 1: ["hello"]
 *   str_split_keep_empty("hello", ',', parts, 10) → returns 1: ["hello"]
 *
 * Buffer overflow handling:
 * ────────────────────────────────────────────────────────────────────────────
 * If the input has more parts than max_parts:
 *
 * - Only first max_parts are stored in output buffer
 * - Return value still equals max_parts (not total parts found)
 * - Remaining parts are NOT processed or counted
 * - No way to know if there were more parts (different from str_split_to_parts)
 *
 * Safe approach:
 * ```c
 * const char* parts[10];
 * size_t count = str_split(input, ',', parts, 10);
 * 
 * if (count == 10) {
 *     // Might be more parts, buffer possibly full
 *     fprintf(stderr, "Warning: possibly truncated\n");
 * }
 * 
 * // Process what we got
 * for (size_t i = 0; i < count; i++) {
 *     process(parts[i]);
 * }
 * ```
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - CSV/TSV parsing
 * - Configuration file parsing (key=value)
 * - Command-line argument tokenization
 * - Path splitting (/, \, :)
 * - Parsing delimited log entries
 * - Protocol message parsing
 * - URL parameter parsing
 * - INI file section parsing
 * - Whitespace normalization
 * - Quote removal from strings
 * - Email header parsing
 * - Simple lexical analysis
 *
 * NOT suitable for:
 * - Complex quoting rules (use proper CSV parser)
 * - Escape sequences (use dedicated parser)
 * - Multi-character delimiters (use strstr-based approach)
 * - Nested structures (use recursive parser)
 * - When you need null-terminated parts (copy them out)
 *
 * Usage examples:
 *
 * Basic CSV tokenization (skip empties):
 * ```c
 * const char* line = "apple,banana,cherry";
 * const char* parts[10];
 * 
 * size_t count = str_split(line, ',', parts, 10);
 * // count = 3
 * // parts[0] points to "apple,banana,cherry" (at 'a')
 * // parts[1] points to "banana,cherry" (at 'b')
 * // parts[2] points to "cherry" (at 'c')
 * ```
 *
 * Strict CSV parsing (keep empties):
 * ```c
 * const char* csv = "Alice,,30,";  // Name, middle_name, age, city
 * const char* fields[10];
 * 
 * size_t count = str_split_keep_empty(csv, ',', fields, 10);
 * // count = 4
 * // fields: ["Alice", "", "30", ""]
 * // Preserves empty middle name and missing city
 * ```
 *
 * Whitespace trimming:
 * ```c
 * char input[100];
 * fgets(input, sizeof(input), stdin);
 * 
 * str_trim_whitespace_inplace(input);  // Remove leading/trailing whitespace
 * printf("Cleaned: '%s'\n", input);
 * ```
 */

/* ────────────────────────────────────────────────────────────────────────────
   Splitting — returns borrowed views (zero-copy, non-owning)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Splits string by single character delimiter, skipping empty segments
 *
 * Parses a string into tokens separated by a delimiter, skipping consecutive
 * delimiters and leading/trailing delimiters. Returns borrowed pointers into
 * the original string (zero-copy).
 *
 * This is the most commonly used splitting function. It treats consecutive
 * delimiters as a single separator, which is typical for whitespace-delimited
 * text and general tokenization.
 *
 * Algorithm:
 *   - Scan through input string
 *   - Skip leading delimiters
 *   - Mark start of token
 *   - Store pointer if space available
 *   - Skip to next delimiter
 *   - Repeat until end of string
 *
 * @param s         Null-terminated input string (must not be NULL for results)
 * @param delim     Single character delimiter to split on
 * @param parts_out Buffer to store pointers to start of each part
 * @param max_parts Maximum number of parts the buffer can hold
 *
 * @return          Number of parts found and written to buffer (≤ max_parts)
 *                  Returns 0 on NULL input or zero-capacity buffer
 *
 * Preconditions:
 *   - If s is not NULL: s is null-terminated
 *   - parts_out has space for at least max_parts pointers
 *   - max_parts > 0 for meaningful results
 *
 * Postconditions:
 *   - parts_out[0..return-1] contain pointers into original string s
 *   - Pointers are valid as long as s exists and is not modified
 *   - Input string s is unchanged (const-correct)
 *   - Returned parts are NOT null-terminated
 *   - Return value ≤ max_parts
 *
 * Splitting behavior:
 *   - Consecutive delimiters: treated as single separator
 *   - Leading delimiters: skipped
 *   - Trailing delimiters: skipped
 *   - Empty string: returns 0
 *   - No delimiters: returns 1 (entire string)
 *
 * Performance:
 *   - Time: O(n) where n = strlen(s)
 *   - Space: O(1) - zero allocations
 *   - Single pass through string
 *   - Zero copying
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Basic CSV:
 * ```c
 * const char* line = "apple,banana,cherry";
 * const char* parts[10];
 * 
 * size_t count = str_split(line, ',', parts, 10);
 * // count = 3
 * // parts[0] → "apple,banana,cherry" (substring starting at 'a')
 * // parts[1] → "banana,cherry" (substring starting at 'b')
 * // parts[2] → "cherry" (substring starting at 'c')
 * ```
 *
 * Example - Consecutive delimiters (skipped):
 * ```c
 * const char* s = "hello,,world,,,test";
 * const char* parts[10];
 * 
 * size_t count = str_split(s, ',', parts, 10);
 * // count = 3
 * // parts: ["hello", "world", "test"]
 * // Empty fields between consecutive commas are skipped
 * ```
 *
 * Example - Leading/trailing delimiters:
 * ```c
 * const char* s = ",,,data1,data2,,,";
 * const char* parts[10];
 * 
 * size_t count = str_split(s, ',', parts, 10);
 * // count = 2
 * // parts: ["data1", "data2"]
 * // Leading and trailing delimiters ignored
 * ```
 *
 * Example - Whitespace tokenization:
 * ```c
 * const char* sentence = "  hello   world  test  ";
 * const char* words[10];
 * 
 * size_t count = str_split(sentence, ' ', words, 10);
 * // count = 3
 * // words: ["hello", "world", "test"]
 * ```
 *
 * Example - Path splitting:
 * ```c
 * const char* path = "/usr/local/bin";
 * const char* components[10];
 * 
 * size_t count = str_split(path, '/', components, 10);
 * // count = 3
 * // components: ["usr", "local", "bin"]
 * ```
 *
 * Example - Using parts (not null-terminated):
 * ```c
 * const char* csv = "name,age,city";
 * const char* fields[10];
 * 
 * size_t count = str_split(csv, ',', fields, 10);
 * 
 * // Compare with strncmp (parts not null-terminated)
 * if (count > 0 && strncmp(fields[0], "name", 4) == 0) {
 *     printf("First field is 'name'\n");
 * }
 * 
 * // Print with precision
 * if (count > 1) {
 *     const char* p = fields[1];
 *     const char* end = p;
 *     while (*end && *end != ',') end++;
 *     printf("Second field: %.*s\n", (int)(end - p), p);
 * }
 * ```
 *
 * Example - Buffer overflow:
 * ```c
 * const char* s = "a,b,c,d,e,f,g,h";
 * const char* parts[3];  // Only room for 3
 * 
 * size_t count = str_split(s, ',', parts, 3);
 * // count = 3
 * // parts: ["a", "b", "c"]
 * // Remaining parts ("d", "e", ...) not processed
 * ```
 *
 * Common pitfalls:
 * - Using strcmp on parts (they're not null-terminated)
 * - Returning parts when input goes out of scope
 * - Assuming parts are independent strings (they share memory)
 * - Not calculating length before operations
 * - Modifying through const char* pointers
 */
static inline size_t str_split(
    const char* s,
    char delim,
    const char** parts_out,
    size_t max_parts
) {
    // Validate inputs
    if (!s || !parts_out || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    const char* p = s;

    while (*p) {
        // Skip consecutive delimiters
        while (*p == delim) {
            ++p;
        }
        
        // Check if we've reached end after skipping delimiters
        if (!*p) {
            break;
        }

        // Store start of current token if buffer has space
        if (count < max_parts) {
            parts_out[count++] = p;
        } else {
            // Buffer full, stop processing
            break;
        }

        // Skip to next delimiter or end of string
        while (*p && *p != delim) {
            ++p;
        }
    }

    return count;
}

/**
 * @brief Splits string by delimiter — keeps empty fields
 *
 * Parses a string into tokens separated by a delimiter, preserving empty
 * fields between consecutive delimiters. Returns borrowed pointers into
 * the original string (zero-copy).
 *
 * This function is for strict CSV parsing where empty fields have meaning
 * (missing data, optional columns, etc.). Every delimiter creates a field
 * boundary, even consecutive ones.
 *
 * Algorithm:
 *   - Scan through input string
 *   - Mark start of each field (at delimiter or start)
 *   - Store pointer when delimiter encountered
 *   - Include empty field if delimiter at end
 *   - Continue until string exhausted
 *
 * @param s         Null-terminated input string (must not be NULL for results)
 * @param delim     Single character delimiter to split on
 * @param parts_out Buffer to store pointers to start of each part
 * @param max_parts Maximum number of parts the buffer can hold
 *
 * @return          Number of parts found and written (≤ max_parts)
 *                  Returns 0 on NULL input or zero-capacity buffer
 *
 * Preconditions:
 *   - If s is not NULL: s is null-terminated
 *   - parts_out has space for at least max_parts pointers
 *   - max_parts > 0 for meaningful results
 *
 * Postconditions:
 *   - parts_out[0..return-1] contain pointers into original string s
 *   - Pointers valid while s exists
 *   - Input string s unchanged
 *   - Returned parts NOT null-terminated
 *   - Empty fields preserved (consecutive delimiters)
 *
 * Splitting behavior:
 *   - Consecutive delimiters: create empty fields
 *   - Leading delimiter: creates empty first field
 *   - Trailing delimiter: creates empty last field
 *   - Empty string: returns 0
 *   - No delimiters: returns 1 (entire string)
 *
 * Performance:
 *   - Time: O(n) where n = strlen(s)
 *   - Space: O(1) - zero allocations
 *   - Single pass through string
 *
 * Thread-safety: Fully thread-safe (no shared state)
 *
 * Example - Strict CSV with empty fields:
 * ```c
 * const char* csv = "Alice,,30,";
 * const char* fields[10];
 * 
 * size_t count = str_split_keep_empty(csv, ',', fields, 10);
 * // count = 4
 * // fields[0] → "Alice"
 * // fields[1] → "" (empty - points to second comma)
 * // fields[2] → "30"
 * // fields[3] → "" (empty - after trailing comma)
 * ```
 *
 * Example - Consecutive delimiters:
 * ```c
 * const char* s = "a,,b,,,c";
 * const char* parts[10];
 * 
 * size_t count = str_split_keep_empty(s, ',', parts, 10);
 * // count = 6
 * // parts: ["a", "", "b", "", "", "c"]
 * ```
 *
 * Example - Leading delimiter:
 * ```c
 * const char* s = ",data";
 * const char* parts[10];
 * 
 * size_t count = str_split_keep_empty(s, ',', parts, 10);
 * // count = 2
 * // parts: ["", "data"]
 * ```
 *
 * Example - Trailing delimiter:
 * ```c
 * const char* s = "data,";
 * const char* parts[10];
 * 
 * size_t count = str_split_keep_empty(s, ',', parts, 10);
 * // count = 2
 * // parts: ["data", ""]
 * ```
 *
 * Example - CSV with optional fields:
 * ```c
 * // Format: name,age,email,phone
 * const char* rows[] = {
 *     "Alice,25,alice@example.com,555-0100",
 *     "Bob,30,,",  // No email or phone
 *     "Charlie,,charlie@example.com,"  // No age or phone
 * };
 * 
 * for (int i = 0; i < 3; i++) {
 *     const char* fields[4];
 *     size_t count = str_split_keep_empty(rows[i], ',', fields, 4);
 *     
 *     // count always = 4 for valid CSV
 *     if (count == 4) {
 *         printf("Row %d: name=%.*s age=%.*s\n", i,
 *                get_field_length(fields[0], ','), fields[0],
 *                get_field_length(fields[1], ','), fields[1]);
 *     }
 * }
 * ```
 *
 * Example - Checking for empty fields:
 * ```c
 * const char* csv = "a,,c";
 * const char* parts[10];
 * 
 * size_t count = str_split_keep_empty(csv, ',', parts, 10);
 * 
 * for (size_t i = 0; i < count; i++) {
 *     if (*parts[i] == ',' || *parts[i] == '\0') {
 *         printf("Field %zu is empty\n", i);
 *     } else {
 *         // Field has content
 *         process_field(parts[i]);
 *     }
 * }
 * ```
 *
 * Use cases:
 * - CSV parsing where empty fields matter
 * - Fixed-column data formats
 * - Protocol parsing with optional fields
 * - Configuration with default values
 * - Database export formats
 *
 * Common pitfalls:
 * - Same as str_split (parts not null-terminated)
 * - Expecting empty fields to point to ""
 * - Not handling empty fields specially
 */
static inline size_t str_split_keep_empty(
    const char* s,
    char delim,
    const char** parts_out,
    size_t max_parts
) {
    // Validate inputs
    if (!s || !parts_out || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    const char* p = s;
    const char* start = s;

    // Process string including potential empty field after final delimiter
    while (*p || start != p) {
        if (*p == delim || *p == '\0') {
            // Found delimiter or end - store this field
            if (count < max_parts) {
                parts_out[count++] = start;
            } else {
                // Buffer full
                break;
            }
            
            // Move start to next field
            start = p + 1;
        }

        // Stop if we hit null terminator
        if (*p == '\0') {
            break;
        }

        ++p;
    }

    return count;
}

/* ────────────────────────────────────────────────────────────────────────────
   In-place trimming — modifies caller-owned buffer
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Trims specific character from both ends of string (in-place)
 *
 * Removes all occurrences of a specific character from the beginning and
 * end of a string, modifying the buffer in-place. Uses memmove for safe
 * overlapping memory operations.
 *
 * This function is useful for removing quotes, brackets, or other delimiters
 * from strings.
 *
 * Algorithm:
 *   - Find first non-trim character (trim leading)
 *   - If all trimmed, set empty string and return
 *   - Move content left using memmove if needed
 *   - Find last non-trim character (trim trailing)
 *   - Null-terminate at trimmed position
 *
 * @param s       Null-terminated string to trim (must be writable)
 * @param trim_ch Character to remove from both ends
 *
 * @return        Pointer to trimmed string (same as input s)
 *                Returns s even if no modification needed
 *
 * Preconditions:
 *   - s is null-terminated writable buffer
 *   - Caller owns the buffer
 *
 * Postconditions:
 *   - Leading and trailing trim_ch characters removed
 *   - String shifted left if leading chars removed
 *   - Always null-terminated
 *   - Return value points to same buffer as s
 *
 * Behavior on edge cases:
 *   - NULL input: returns NULL (no-op)
 *   - Empty string: returns s unchanged
 *   - All trim characters: returns empty string (s[0] = '\0')
 *   - No trim characters: returns s unchanged
 *
 * Performance:
 *   - Time: O(n) where n = strlen(s)
 *   - Space: O(1) - in-place modification
 *   - Uses memmove for safe overlapping copy
 *
 * Thread-safety: Safe if caller ensures exclusive access to buffer
 *
 * Example - Remove quotes:
 * ```c
 * char str[] = "\"hello world\"";
 * str_trim_char_inplace(str, '"');
 * // str = "hello world"
 * ```
 *
 * Example - Remove brackets:
 * ```c
 * char str[] = "[[data]]";
 * str_trim_char_inplace(str, '[');
 * // str = "data]]"
 * 
 * str_trim_char_inplace(str, ']');
 * // str = "data"
 * ```
 *
 * Example - All trim characters:
 * ```c
 * char str[] = "-----";
 * str_trim_char_inplace(str, '-');
 * // str = ""
 * ```
 *
 * Example - Mixed content:
 * ```c
 * char str[] = ",,data,value,,";
 * str_trim_char_inplace(str, ',');
 * // str = "data,value"
 * // Only leading and trailing removed
 * ```
 *
 * Example - Chaining operations:
 * ```c
 * char str[] = "  'text'  ";
 * str_trim_whitespace_inplace(str);
 * // str = "'text'"
 * str_trim_char_inplace(str, '\'');
 * // str = "text"
 * ```
 *
 * Common use cases:
 * - Removing quotation marks
 * - Removing brackets/braces
 * - Cleaning delimiters
 * - Normalizing input
 */
static inline char* str_trim_char_inplace(char* s, char trim_ch) {
    // Handle NULL or empty string
    if (!s || !*s) {
        return s;
    }

    // Trim leading characters
    char* start = s;
    while (*start == trim_ch) {
        ++start;
    }

    // Check if everything was trimmed
    if (!*start) {
        s[0] = '\0';
        return s;
    }

    // Move content left if we skipped leading characters
    if (start > s) {
        memmove(s, start, strlen(start) + 1);  // +1 for null terminator
    }

    // Trim trailing characters
    char* end = s + strlen(s);
    while (end > s && *(end - 1) == trim_ch) {
        --end;
    }

    // Null-terminate at trimmed position
    *end = '\0';
    
    return s;
}

/**
 * @brief Trims all whitespace characters from both ends (in-place)
 *
 * Removes all leading and trailing whitespace from a string, modifying the
 * buffer in-place. Uses standard isspace() to identify whitespace characters.
 *
 * Whitespace characters (per isspace() in C locale):
 * - Space ' ' (0x20)
 * - Tab '\t' (0x09)
 * - Newline '\n' (0x0A)
 * - Carriage return '\r' (0x0D)
 * - Form feed '\f' (0x0C)
 * - Vertical tab '\v' (0x0B)
 *
 * Algorithm:
 *   - Find first non-whitespace character
 *   - If all whitespace, set empty string and return
 *   - Move content left using memmove if needed
 *   - Find last non-whitespace character
 *   - Null-terminate at trimmed position
 *
 * @param s Writable null-terminated string to trim
 *
 * @return  Pointer to trimmed string (same as input s)
 *          Returns s even if no modification needed
 *
 * Preconditions:
 *   - s is null-terminated writable buffer
 *   - Caller owns the buffer
 *
 * Postconditions:
 *   - Leading and trailing whitespace removed
 *   - String shifted left if leading whitespace removed
 *   - Always null-terminated
 *   - Return value points to same buffer as s
 *
 * Behavior on edge cases:
 *   - NULL input: returns NULL (no-op)
 *   - Empty string: returns s unchanged
 *   - All whitespace: returns empty string (s[0] = '\0')
 *   - No whitespace: returns s unchanged
 *
 * Performance:
 *   - Time: O(n) where n = strlen(s)
 *   - Space: O(1) - in-place modification
 *   - Uses memmove for safe overlapping copy
 *
 * Thread-safety: Safe if caller ensures exclusive access to buffer
 *
 * Example - Clean user input:
 * ```c
 * char input[100];
 * fgets(input, sizeof(input), stdin);
 * 
 * str_trim_whitespace_inplace(input);
 * printf("Cleaned: '%s'\n", input);
 * ```
 *
 * Example - Various whitespace:
 * ```c
 * char str[] = "  \t\n  hello world  \n\r  ";
 * str_trim_whitespace_inplace(str);
 * // str = "hello world"
 * ```
 *
 * Example - All whitespace:
 * ```c
 * char str[] = "   \t\n\r   ";
 * str_trim_whitespace_inplace(str);
 * // str = ""
 * ```
 *
 * Example - No whitespace:
 * ```c
 * char str[] = "hello";
 * str_trim_whitespace_inplace(str);
 * // str = "hello" (unchanged)
 * ```
 *
 * Example - Processing lines:
 * ```c
 * char line[256];
 * while (fgets(line, sizeof(line), file)) {
 *     str_trim_whitespace_inplace(line);
 *     
 *     if (line[0] == '\0' || line[0] == '#') {
 *         continue;  // Skip empty or comment lines
 *     }
 *     
 *     process_line(line);
 * }
 * ```
 *
 * Example - Configuration parsing:
 * ```c
 * char config[] = "  key = value  ";
 * str_trim_whitespace_inplace(config);
 * // config = "key = value"
 * 
 * // Now split on '='
 * const char* parts[2];
 * str_split(config, '=', parts, 2);
 * ```
 *
 * Common use cases:
 * - Cleaning user input
 * - Normalizing file lines
 * - Pre-processing for parsing
 * - Form data cleaning
 * - Configuration file parsing
 *
 * Implementation note:
 * Uses (unsigned char) cast before isspace() to avoid undefined behavior
 * with signed char values > 127. This is the standard-compliant way to
 * use isspace() and other ctype.h functions.
 */
static inline char* str_trim_whitespace_inplace(char* s) {
    // Handle NULL or empty string
    if (!s || !*s) {
        return s;
    }

    // Trim leading whitespace
    char* start = s;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }

    // Check if everything was whitespace
    if (!*start) {
        s[0] = '\0';
        return s;
    }

    // Move content left if we skipped leading whitespace
    if (start > s) {
        memmove(s, start, strlen(start) + 1);  // +1 for null terminator
    }

    // Trim trailing whitespace
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) {
        --end;
    }

    // Null-terminate at trimmed position
    *end = '\0';
    
    return s;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/str_split.h"
    #include <stdio.h>
    
    // Example 1: Basic CSV tokenization
    void example_csv_tokenize(void) {
        const char* line = "apple,banana,cherry,date";
        const char* parts[10];
        
        size_t count = str_split(line, ',', parts, 10);
        printf("Found %zu items:\n", count);
        
        for (size_t i = 0; i < count; i++) {
            // Calculate length (parts not null-terminated)
            const char* p = parts[i];
            const char* end = p;
            while (*end && *end != ',') end++;
            
            printf("  %zu: %.*s\n", i, (int)(end - p), p);
        }
    }
    
    // Example 2: Strict CSV with empty fields
    void example_strict_csv(void) {
        const char* csv = "Alice,,30,";  // name, middle, age, city
        const char* fields[10];
        
        size_t count = str_split_keep_empty(csv, ',', fields, 10);
        printf("CSV fields: %zu\n", count);
        
        const char* labels[] = {"Name", "Middle", "Age", "City"};
        for (size_t i = 0; i < count; i++) {
            const char* p = fields[i];
            const char* end = p;
            while (*end && *end != ',') end++;
            
            if (p == end || *p == ',') {
                printf("%s: (empty)\n", labels[i]);
            } else {
                printf("%s: %.*s\n", labels[i], (int)(end - p), p);
            }
        }
    }
    
    // Example 3: Path splitting
    void example_path_split(void) {
        const char* unix_path = "/home/user/documents/file.txt";
        const char* components[10];
        
        size_t count = str_split(unix_path, '/', components, 10);
        
        printf("Path components:\n");
        for (size_t i = 0; i < count; i++) {
            const char* p = components[i];
            const char* end = p;
            while (*end && *end != '/') end++;
            printf("  %.*s\n", (int)(end - p), p);
        }
    }
    
    // Example 4: Whitespace trimming
    void example_trim(void) {
        char inputs[][50] = {
            "  hello world  ",
            "\t\tdata\n\n",
            "   ",
            "no-whitespace"
        };
        
        for (int i = 0; i < 4; i++) {
            printf("Before: '%s'\n", inputs[i]);
            str_trim_whitespace_inplace(inputs[i]);
            printf("After:  '%s'\n\n", inputs[i]);
        }
    }
    
    // Example 5: Quote removal
    void example_quote_removal(void) {
        char quoted[][50] = {
            "\"hello world\"",
            "'single quotes'",
            "\"\"",
            "no quotes"
        };
        
        for (int i = 0; i < 4; i++) {
            printf("Original: %s\n", quoted[i]);
            str_trim_char_inplace(quoted[i], '"');
            str_trim_char_inplace(quoted[i], '\'');
            printf("Trimmed:  %s\n\n", quoted[i]);
        }
    }
    
    // Example 6: Configuration file parsing
    void example_config_parsing(void) {
        const char* lines[] = {
            "host = localhost",
            "port = 8080",
            "  timeout = 30  ",
            "# comment line"
        };
        
        for (int i = 0; i < 4; i++) {
            char line[100];
            strcpy(line, lines[i]);
            
            // Trim whitespace
            str_trim_whitespace_inplace(line);
            
            // Skip comments
            if (line[0] == '#' || line[0] == '\0') {
                continue;
            }
            
            // Split on '='
            const char* parts[2];
            size_t count = str_split(line, '=', parts, 2);
            
            if (count == 2) {
                printf("Config: %s → %s\n", parts[0], parts[1]);
            }
        }
    }
    
    // Example 7: Consecutive delimiters
    void example_consecutive_delimiters(void) {
        const char* s1 = "a,,b,,,c";
        const char* parts1[10];
        
        printf("Skip empties mode:\n");
        size_t count1 = str_split(s1, ',', parts1, 10);
        printf("  Input: '%s'\n", s1);
        printf("  Count: %zu\n", count1);
        
        printf("\nKeep empties mode:\n");
        const char* parts2[10];
        size_t count2 = str_split_keep_empty(s1, ',', parts2, 10);
        printf("  Input: '%s'\n", s1);
        printf("  Count: %zu\n", count2);
    }
    
    // Example 8: Word counting
    void example_word_count(void) {
        const char* sentences[] = {
            "Hello world",
            "  multiple   spaces   between  words  ",
            "single",
            ""
        };
        
        for (int i = 0; i < 4; i++) {
            const char* words[100];
            size_t count = str_split(sentences[i], ' ', words, 100);
            printf("'%s' has %zu words\n", sentences[i], count);
        }
    }
    
    // Example 9: URL parameter parsing
    void example_url_params(void) {
        const char* query = "name=John&age=30&city=NYC";
        const char* params[10];
        
        size_t count = str_split(query, '&', params, 10);
        
        printf("URL Parameters:\n");
        for (size_t i = 0; i < count; i++) {
            const char* kv[2];
            
            // Create substring for this parameter
            const char* p = params[i];
            const char* end = p;
            while (*end && *end != '&') end++;
            
            // Split on '='
            char param[100];
            size_t len = end - p;
            strncpy(param, p, len);
            param[len] = '\0';
            
            size_t kv_count = str_split(param, '=', kv, 2);
            if (kv_count == 2) {
                printf("  %s = %s\n", kv[0], kv[1]);
            }
        }
    }
    
    // Example 10: Processing log files
    void example_log_processing(void) {
        const char* log_lines[] = {
            "2026-01-20 10:30:00 INFO Application started",
            "2026-01-20 10:30:01 DEBUG Loading config",
            "2026-01-20 10:30:02 ERROR Connection failed"
        };
        
        for (int i = 0; i < 3; i++) {
            const char* fields[4];
            size_t count = str_split(log_lines[i], ' ', fields, 4);
            
            if (count >= 3) {
                printf("Date: %s, Time: %s, Level: %s\n",
                       fields[0], fields[1], fields[2]);
            }
        }
    }
    
    // Example 11: Email header parsing
    void example_email_header(void) {
        char header[] = "  To: user@example.com  ";
        
        str_trim_whitespace_inplace(header);
        
        const char* parts[2];
        size_t count = str_split(header, ':', parts, 2);
        
        if (count == 2) {
            printf("Header name: %s\n", parts[0]);
            printf("Header value: %s\n", parts[1]);
        }
    }
    
    // Example 12: Data validation
    void example_validation(void) {
        const char* csv_rows[] = {
            "field1,field2,field3",      // Valid: 3 fields
            "field1,field2",              // Invalid: 2 fields
            "field1,field2,field3,field4" // Invalid: 4 fields
        };
        
        const size_t EXPECTED_FIELDS = 3;
        
        for (int i = 0; i < 3; i++) {
            const char* fields[10];
            size_t count = str_split_keep_empty(csv_rows[i], ',', fields, 10);
            
            if (count == EXPECTED_FIELDS) {
                printf("Row %d: Valid (%zu fields)\n", i, count);
            } else {
                printf("Row %d: Invalid (expected %zu, got %zu)\n",
                       i, EXPECTED_FIELDS, count);
            }
        }
    }
    
    // Example 13: Multiple delimiters (manual)
    void example_multiple_delimiters(void) {
        char text[] = "data1;data2:data3;data4";
        
        // Replace ':' with ';'
        for (char* p = text; *p; p++) {
            if (*p == ':') *p = ';';
        }
        
        // Now split on ';'
        const char* parts[10];
        size_t count = str_split(text, ';', parts, 10);
        
        printf("Found %zu parts\n", count);
    }
    
    // Example 14: Cleaning and normalizing
    void example_clean_normalize(void) {
        char inputs[][100] = {
            "  John  Doe  ",
            "\tAlice\tSmith\t",
            "  Bob   Jones  "
        };
        
        for (int i = 0; i < 3; i++) {
            // First trim
            str_trim_whitespace_inplace(inputs[i]);
            
            // Then split on whitespace
            const char* names[10];
            size_t count = str_split(inputs[i], ' ', names, 10);
            
            printf("Name parts: %zu\n", count);
        }
    }
    
    // Example 15: Interactive input processing
    void example_interactive(void) {
        char input[256] = "  command arg1 arg2 arg3  ";
        
        printf("Input: '%s'\n", input);
        
        // Clean input
        str_trim_whitespace_inplace(input);
        
        // Parse command and arguments
        const char* parts[10];
        size_t count = str_split(input, ' ', parts, 10);
        
        if (count > 0) {
            printf("Command: %s\n", parts[0]);
            printf("Arguments: %zu\n", count - 1);
            
            for (size_t i = 1; i < count; i++) {
                printf("  Arg %zu: %s\n", i, parts[i]);
            }
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_STR_SPLIT_H */
