/**
 * @file str_split_test.c
 * @brief Unit tests (and optional fuzz harness) for util/str/str_split.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput feeds arbitrary strings to
 *              str_split, str_split_keep_empty, and trim functions,
 *              verifying invariants on every input.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   str_split                  — basic, consecutive delims, leading/trailing,
 *                                empty input, all delims, max_parts limit,
 *                                single element, max_parts==0
 *   str_split_keep_empty       — basic, consecutive (creates empties),
 *                                leading/trailing, empty input, single element
 *   str_trim_char_inplace      — both ends, leading only, trailing only,
 *                                no match, all trimmed, empty, single char
 *   str_trim_whitespace_inplace — mixed whitespace, no whitespace,
 *                                all whitespace, tabs and newlines
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL input to str_split, str_split_keep_empty, str_trim_char_inplace,
 *   and str_trim_whitespace_inplace is a contract violation (require_msg →
 *   abort). Not tested here.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Loop variables declared before loop body (C99). All char buffers
 *   zero-initialized to satisfy static analysis. Borrowed views from
 *   str_split are NOT null-terminated — tests use str_ncompare or
 *   calculate length via pointer arithmetic.
 */

#define CANON_CONTRACT_IMPL
#include "util/str/str_split.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Portability: unused-function suppression
 * ====================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_MAYBE_UNUSED __attribute__((unused))
#else
    #define CANON_MAYBE_UNUSED
#endif

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) static CANON_MAYBE_UNUSED void test_##name(void)
#define RUN(name)  do { test_##name(); } while (0)

#define EXPECT(expr)                                                \
    do {                                                            \
        if (expr) {                                                 \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr, "FAIL  %s:%d  %s\n",                   \
                    __FILE__, __LINE__, #expr);                     \
        }                                                           \
    } while (0)

/* =========================================================================
 * Helper: check borrowed view matches expected string
 *
 * Borrowed views from str_split are NOT null-terminated. This helper
 * compares a view (pointer into original string) against an expected
 * null-terminated string by scanning forward to the next delimiter or
 * end of string to determine the view length.
 * ====================================================================== */

static bool view_equals(const char* view, char delim, const char* expected) {
    const char* end;
    usize view_len, exp_len;

    if (!view || !expected) return false;

    /* Find end of this view: next delimiter or end of string */
    end = view;
    while (*end && *end != delim) end++;
    view_len = (usize)(end - view);
    exp_len  = str_len(expected);

    if (view_len != exp_len) return false;
    return str_ncompare(view, expected, view_len) == 0;
}

/* =========================================================================
 * str_split — basic
 * ====================================================================== */

TEST(split_basic) {
    const char* input = "a,b,c";
    const char* parts[8];
    usize n = str_split(input, ',', parts, 8);
    EXPECT(n == 3);
    EXPECT(view_equals(parts[0], ',', "a"));
    EXPECT(view_equals(parts[1], ',', "b"));
    EXPECT(view_equals(parts[2], ',', "c"));
}

TEST(split_space_delim) {
    const char* input = "hello world test";
    const char* parts[8];
    usize n = str_split(input, ' ', parts, 8);
    EXPECT(n == 3);
    EXPECT(view_equals(parts[0], ' ', "hello"));
    EXPECT(view_equals(parts[1], ' ', "world"));
    EXPECT(view_equals(parts[2], ' ', "test"));
}

/* =========================================================================
 * str_split — consecutive delimiters (skipped)
 * ====================================================================== */

TEST(split_consecutive) {
    const char* input = "a,,b,,,c";
    const char* parts[8];
    usize n = str_split(input, ',', parts, 8);
    EXPECT(n == 3);
    EXPECT(view_equals(parts[0], ',', "a"));
    EXPECT(view_equals(parts[1], ',', "b"));
    EXPECT(view_equals(parts[2], ',', "c"));
}

/* =========================================================================
 * str_split — leading/trailing delimiters
 * ====================================================================== */

TEST(split_leading_trailing) {
    const char* input = ",a,b,";
    const char* parts[8];
    usize n = str_split(input, ',', parts, 8);
    EXPECT(n == 2);
    EXPECT(view_equals(parts[0], ',', "a"));
    EXPECT(view_equals(parts[1], ',', "b"));
}

/* =========================================================================
 * str_split — empty input
 * ====================================================================== */

TEST(split_empty) {
    const char* parts[8];
    usize n = str_split("", ',', parts, 8);
    EXPECT(n == 0);
}

/* =========================================================================
 * str_split — all delimiters
 * ====================================================================== */

TEST(split_all_delims) {
    const char* parts[8];
    usize n = str_split(",,,", ',', parts, 8);
    EXPECT(n == 0);
}

/* =========================================================================
 * str_split — single element (no delimiter)
 * ====================================================================== */

TEST(split_single) {
    const char* input = "hello";
    const char* parts[8];
    usize n = str_split(input, ',', parts, 8);
    EXPECT(n == 1);
    EXPECT(view_equals(parts[0], ',', "hello"));
}

/* =========================================================================
 * str_split — max_parts limit
 * ====================================================================== */

TEST(split_max_parts) {
    const char* input = "a,b,c,d,e";
    const char* parts[3];
    usize n = str_split(input, ',', parts, 3);
    EXPECT(n == 3);
    EXPECT(view_equals(parts[0], ',', "a"));
    EXPECT(view_equals(parts[1], ',', "b"));
    EXPECT(view_equals(parts[2], ',', "c"));
}

/* =========================================================================
 * str_split — max_parts == 0
 * ====================================================================== */

TEST(split_zero_max) {
    const char* parts[8];
    usize n = str_split("a,b", ',', parts, 0);
    EXPECT(n == 0);
}

/* =========================================================================
 * str_split_keep_empty — basic
 * ====================================================================== */

TEST(keep_empty_basic) {
    const char* input = "a,b,c";
    const char* parts[8];
    usize n = str_split_keep_empty(input, ',', parts, 8);
    EXPECT(n == 3);
    EXPECT(view_equals(parts[0], ',', "a"));
    EXPECT(view_equals(parts[1], ',', "b"));
    EXPECT(view_equals(parts[2], ',', "c"));
}

/* =========================================================================
 * str_split_keep_empty — consecutive delimiters create empty fields
 * ====================================================================== */

TEST(keep_empty_consecutive) {
    const char* input = "a,,b";
    const char* parts[8];
    usize n = str_split_keep_empty(input, ',', parts, 8);
    EXPECT(n == 3);
    EXPECT(view_equals(parts[0], ',', "a"));
    /* parts[1] points to the second comma — empty field */
    EXPECT(parts[1][0] == ',');
    EXPECT(view_equals(parts[2], ',', "b"));
}

/* =========================================================================
 * str_split_keep_empty — leading delimiter creates empty first field
 * ====================================================================== */

TEST(keep_empty_leading) {
    const char* input = ",a,b";
    const char* parts[8];
    usize n = str_split_keep_empty(input, ',', parts, 8);
    EXPECT(n == 3);
    /* First field is empty — points to the comma */
    EXPECT(parts[0][0] == ',');
    EXPECT(view_equals(parts[1], ',', "a"));
    EXPECT(view_equals(parts[2], ',', "b"));
}

/* =========================================================================
 * str_split_keep_empty — trailing delimiter creates empty last field
 * ====================================================================== */

TEST(keep_empty_trailing) {
    const char* input = "a,b,";
    const char* parts[8];
    usize n = str_split_keep_empty(input, ',', parts, 8);
    EXPECT(n == 3);
    EXPECT(view_equals(parts[0], ',', "a"));
    EXPECT(view_equals(parts[1], ',', "b"));
    /* Last field is empty — points to '\0' */
    EXPECT(parts[2][0] == '\0');
}

/* =========================================================================
 * str_split_keep_empty — empty input
 * ====================================================================== */

TEST(keep_empty_empty_input) {
    const char* parts[8];
    usize n = str_split_keep_empty("", ',', parts, 8);
    /* Empty string produces zero fields (no content, no delimiters) */
    EXPECT(n == 0);
}

/* =========================================================================
 * str_split_keep_empty — single element
 * ====================================================================== */

TEST(keep_empty_single) {
    const char* input = "hello";
    const char* parts[8];
    usize n = str_split_keep_empty(input, ',', parts, 8);
    EXPECT(n == 1);
    EXPECT(view_equals(parts[0], ',', "hello"));
}

/* =========================================================================
 * str_split_keep_empty — max_parts == 0
 * ====================================================================== */

TEST(keep_empty_zero_max) {
    const char* parts[8];
    usize n = str_split_keep_empty("a,b", ',', parts, 0);
    EXPECT(n == 0);
}

/* =========================================================================
 * str_trim_char_inplace — both ends
 * ====================================================================== */

TEST(trim_char_both) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "***hello***");
    str_trim_char_inplace(buf, '*');
    EXPECT(str_equals(buf, "hello"));
}

/* =========================================================================
 * str_trim_char_inplace — leading only
 * ====================================================================== */

TEST(trim_char_leading) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "---hello");
    str_trim_char_inplace(buf, '-');
    EXPECT(str_equals(buf, "hello"));
}

/* =========================================================================
 * str_trim_char_inplace — trailing only
 * ====================================================================== */

TEST(trim_char_trailing) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "hello---");
    str_trim_char_inplace(buf, '-');
    EXPECT(str_equals(buf, "hello"));
}

/* =========================================================================
 * str_trim_char_inplace — no match
 * ====================================================================== */

TEST(trim_char_no_match) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "hello");
    str_trim_char_inplace(buf, '*');
    EXPECT(str_equals(buf, "hello"));
}

/* =========================================================================
 * str_trim_char_inplace — all trimmed
 * ====================================================================== */

TEST(trim_char_all) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "****");
    str_trim_char_inplace(buf, '*');
    EXPECT(str_equals(buf, ""));
    EXPECT(str_len(buf) == 0);
}

/* =========================================================================
 * str_trim_char_inplace — empty input
 * ====================================================================== */

TEST(trim_char_empty) {
    char buf[4] = {0};
    str_trim_char_inplace(buf, '*');
    EXPECT(str_equals(buf, ""));
}

/* =========================================================================
 * str_trim_char_inplace — single char match
 * ====================================================================== */

TEST(trim_char_single) {
    char buf[4] = {0};
    str_copy_into(buf, sizeof(buf), "*");
    str_trim_char_inplace(buf, '*');
    EXPECT(str_equals(buf, ""));
}

/* =========================================================================
 * str_trim_char_inplace — single char no match
 * ====================================================================== */

TEST(trim_char_single_no_match) {
    char buf[4] = {0};
    str_copy_into(buf, sizeof(buf), "x");
    str_trim_char_inplace(buf, '*');
    EXPECT(str_equals(buf, "x"));
}

/* =========================================================================
 * str_trim_char_inplace — char in middle not trimmed
 * ====================================================================== */

TEST(trim_char_middle) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "*he*llo*");
    str_trim_char_inplace(buf, '*');
    EXPECT(str_equals(buf, "he*llo"));
}

/* =========================================================================
 * str_trim_whitespace_inplace — mixed whitespace
 * ====================================================================== */

TEST(trim_ws_mixed) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "  \t hello \n ");
    str_trim_whitespace_inplace(buf);
    EXPECT(str_equals(buf, "hello"));
}

/* =========================================================================
 * str_trim_whitespace_inplace — no whitespace
 * ====================================================================== */

TEST(trim_ws_none) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "hello");
    str_trim_whitespace_inplace(buf);
    EXPECT(str_equals(buf, "hello"));
}

/* =========================================================================
 * str_trim_whitespace_inplace — all whitespace
 * ====================================================================== */

TEST(trim_ws_all) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "   \t\n\r  ");
    str_trim_whitespace_inplace(buf);
    EXPECT(str_equals(buf, ""));
}

/* =========================================================================
 * str_trim_whitespace_inplace — tabs and newlines
 * ====================================================================== */

TEST(trim_ws_tabs_newlines) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "\t\ndata\r\n");
    str_trim_whitespace_inplace(buf);
    EXPECT(str_equals(buf, "data"));
}

/* =========================================================================
 * str_trim_whitespace_inplace — empty input
 * ====================================================================== */

TEST(trim_ws_empty) {
    char buf[4] = {0};
    str_trim_whitespace_inplace(buf);
    EXPECT(str_equals(buf, ""));
}

/* =========================================================================
 * str_trim_whitespace_inplace — preserves internal whitespace
 * ====================================================================== */

TEST(trim_ws_internal) {
    char buf[32] = {0};
    str_copy_into(buf, sizeof(buf), "  hello world  ");
    str_trim_whitespace_inplace(buf);
    EXPECT(str_equals(buf, "hello world"));
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    char buf[128];
    const char* parts[16];
    usize n, i, len;

    if (size == 0 || size > 126) return 0;

    memcpy(buf, data, size);
    buf[size] = '\0';

    /* str_split: count must be <= max_parts */
    n = str_split(buf, ',', parts, 16);
    if (n > 16) __builtin_trap();

    /* All returned pointers must be within buf */
    for (i = 0; i < n; i++) {
        if (parts[i] < buf || parts[i] >= buf + size) __builtin_trap();
    }

    /* str_split_keep_empty: count must be <= max_parts */
    n = str_split_keep_empty(buf, ',', parts, 16);
    if (n > 16) __builtin_trap();

    for (i = 0; i < n; i++) {
        if (parts[i] < buf || parts[i] > buf + size) __builtin_trap();
    }

    /* str_trim_char_inplace: result must be <= original length */
    len = str_len(buf);
    str_trim_char_inplace(buf, buf[0]);
    if (str_len(buf) > len) __builtin_trap();

    /* str_trim_whitespace_inplace: result must be <= original length */
    memcpy(buf, data, size);
    buf[size] = '\0';
    len = str_len(buf);
    str_trim_whitespace_inplace(buf);
    if (str_len(buf) > len) __builtin_trap();

    return 0;
}

#else

int main(void) {
    /* str_split */
    RUN(split_basic);
    RUN(split_space_delim);
    RUN(split_consecutive);
    RUN(split_leading_trailing);
    RUN(split_empty);
    RUN(split_all_delims);
    RUN(split_single);
    RUN(split_max_parts);
    RUN(split_zero_max);

    /* str_split_keep_empty */
    RUN(keep_empty_basic);
    RUN(keep_empty_consecutive);
    RUN(keep_empty_leading);
    RUN(keep_empty_trailing);
    RUN(keep_empty_empty_input);
    RUN(keep_empty_single);
    RUN(keep_empty_zero_max);

    /* str_trim_char_inplace */
    RUN(trim_char_both);
    RUN(trim_char_leading);
    RUN(trim_char_trailing);
    RUN(trim_char_no_match);
    RUN(trim_char_all);
    RUN(trim_char_empty);
    RUN(trim_char_single);
    RUN(trim_char_single_no_match);
    RUN(trim_char_middle);

    /* str_trim_whitespace_inplace */
    RUN(trim_ws_mixed);
    RUN(trim_ws_none);
    RUN(trim_ws_all);
    RUN(trim_ws_tabs_newlines);
    RUN(trim_ws_empty);
    RUN(trim_ws_internal);

    printf("\nstr_split_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
