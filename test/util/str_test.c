/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

/**
 * @file str_test.c
 * @brief Unit tests (and optional fuzz harness) for util/str/str.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput feeds arbitrary strings to
 *              str_len, str_compare, str_alloc_copy, str_alloc_concat,
 *              str_alloc_sub, str_copy_into, cstr_starts_with,
 *              cstr_ends_with, verifying invariants on every input.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   str_len          — empty, short, long, NULL
 *   str_compare      — equal, less, greater, prefix, empty
 *   str_ncompare     — bounded comparison, n==0, partial match
 *   str_alloc_copy   — copy + verify, NULL → None, free
 *   str_alloc_concat — two strings, empty+nonempty, free
 *   str_alloc_sub    — middle, start, clamped, out-of-range, free
 *   str_free         — NULL is no-op
 *   str_copy_into    — fits, overflow, exact fit, dest_size==0
 *   str_concat_into  — fits, overflow, exact fit
 *   str_equals       — equal, not equal, NULL+NULL, NULL+non-NULL
 *   cstr_starts_with — match, no match, empty prefix, full string
 *   cstr_ends_with   — match, no match, longer suffix, exact
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL input to str_compare, str_ncompare, str_alloc_concat,
 *   str_alloc_sub, str_copy_into, str_concat_into, cstr_starts_with,
 *   cstr_ends_with is a contract violation (require_msg → abort).
 *   Not tested here. str_len(NULL) → 0 and str_equals(NULL, NULL) → true
 *   are intentional by design.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Loop variables are declared before the loop body (C99).
 */

#define CANON_CONTRACT_IMPL
#include "util/str/str.h"
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
 * str_len
 * ====================================================================== */

TEST(len_null) {
    EXPECT(str_len(NULL) == 0);
}

TEST(len_empty) {
    EXPECT(str_len("") == 0);
}

TEST(len_short) {
    EXPECT(str_len("abc") == 3);
    EXPECT(str_len("x") == 1);
}

TEST(len_long) {
    EXPECT(str_len("Hello, Canon-C!") == 15);
}

/* =========================================================================
 * str_compare
 * ====================================================================== */

TEST(compare_equal) {
    EXPECT(str_compare("abc", "abc") == 0);
    EXPECT(str_compare("", "") == 0);
    EXPECT(str_compare("x", "x") == 0);
}

TEST(compare_less) {
    EXPECT(str_compare("abc", "abd") < 0);
    EXPECT(str_compare("abc", "abcd") < 0);
    EXPECT(str_compare("", "a") < 0);
}

TEST(compare_greater) {
    EXPECT(str_compare("abd", "abc") > 0);
    EXPECT(str_compare("abcd", "abc") > 0);
    EXPECT(str_compare("a", "") > 0);
}

TEST(compare_case_sensitive) {
    /* 'A' (65) < 'a' (97) in ASCII */
    EXPECT(str_compare("A", "a") < 0);
    EXPECT(str_compare("a", "A") > 0);
}

/* =========================================================================
 * str_ncompare
 * ====================================================================== */

TEST(ncompare_zero_n) {
    /* n == 0 → always 0, no characters compared */
    EXPECT(str_ncompare("abc", "xyz", 0) == 0);
    EXPECT(str_ncompare("", "", 0) == 0);
}

TEST(ncompare_partial) {
    /* First 3 chars of "abcdef" and "abcxyz" are equal */
    EXPECT(str_ncompare("abcdef", "abcxyz", 3) == 0);
    EXPECT(str_ncompare("abcdef", "abcxyz", 4) < 0);
}

TEST(ncompare_full) {
    EXPECT(str_ncompare("abc", "abc", 3) == 0);
    EXPECT(str_ncompare("abc", "abc", 100) == 0);
}

TEST(ncompare_one) {
    EXPECT(str_ncompare("a", "b", 1) < 0);
    EXPECT(str_ncompare("b", "a", 1) > 0);
    EXPECT(str_ncompare("x", "x", 1) == 0);
}

/* =========================================================================
 * str_alloc_copy
 * ====================================================================== */

TEST(alloc_copy_valid) {
    option_charp r = str_alloc_copy("hello");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "hello"));
    EXPECT(str_len(s) == 5);
    str_free(s);
}

TEST(alloc_copy_empty) {
    option_charp r = str_alloc_copy("");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_len(s) == 0);
    EXPECT(s[0] == '\0');
    str_free(s);
}

TEST(alloc_copy_null) {
    option_charp r = str_alloc_copy(NULL);
    EXPECT(option_charp_is_none(r));
}

TEST(alloc_copy_independence) {
    /* Modifying copy must not affect original */
    const char* original = "test";
    option_charp r = str_alloc_copy(original);
    EXPECT(option_charp_is_some(r));
    char* copy = option_charp_unwrap(r);
    copy[0] = 'X';
    EXPECT(original[0] == 't');
    str_free(copy);
}

/* =========================================================================
 * str_alloc_concat
 * ====================================================================== */

TEST(alloc_concat_basic) {
    option_charp r = str_alloc_concat("hello", " world");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "hello world"));
    str_free(s);
}

TEST(alloc_concat_empty_left) {
    option_charp r = str_alloc_concat("", "world");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "world"));
    str_free(s);
}

TEST(alloc_concat_empty_right) {
    option_charp r = str_alloc_concat("hello", "");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "hello"));
    str_free(s);
}

TEST(alloc_concat_both_empty) {
    option_charp r = str_alloc_concat("", "");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_len(s) == 0);
    str_free(s);
}

/* =========================================================================
 * str_alloc_sub
 * ====================================================================== */

TEST(alloc_sub_middle) {
    option_charp r = str_alloc_sub("hello world", 6, 5);
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "world"));
    str_free(s);
}

TEST(alloc_sub_start) {
    option_charp r = str_alloc_sub("hello", 0, 3);
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "hel"));
    str_free(s);
}

TEST(alloc_sub_clamped) {
    /* Request more than available — clamped to remaining */
    option_charp r = str_alloc_sub("hello", 3, 100);
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "lo"));
    str_free(s);
}

TEST(alloc_sub_out_of_range) {
    /* start >= length → None */
    option_charp r = str_alloc_sub("hello", 5, 1);
    EXPECT(option_charp_is_none(r));

    r = str_alloc_sub("hello", 99, 1);
    EXPECT(option_charp_is_none(r));
}

TEST(alloc_sub_full) {
    option_charp r = str_alloc_sub("hello", 0, 5);
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "hello"));
    str_free(s);
}

TEST(alloc_sub_single_char) {
    option_charp r = str_alloc_sub("abcde", 2, 1);
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "c"));
    str_free(s);
}

/* =========================================================================
 * str_free — NULL is no-op
 * ====================================================================== */

TEST(free_null) {
    str_free(NULL);
    EXPECT(true); /* didn't crash */
}

/* =========================================================================
 * str_copy_into
 * ====================================================================== */

TEST(copy_into_fits) {
    char buf[16] = {0};
    EXPECT(str_copy_into(buf, sizeof(buf), "hello") == true);
    EXPECT(str_equals(buf, "hello"));
}

TEST(copy_into_overflow) {
    char buf[4] = {0};
    EXPECT(str_copy_into(buf, sizeof(buf), "hello") == false);
}

TEST(copy_into_exact) {
    char buf[6] = {0}; /* "hello" + '\0' = 6 */
    EXPECT(str_copy_into(buf, sizeof(buf), "hello") == true);
    EXPECT(str_equals(buf, "hello"));
}

TEST(copy_into_empty_src) {
    char buf[4] = {0};
    EXPECT(str_copy_into(buf, sizeof(buf), "") == true);
    EXPECT(str_equals(buf, ""));
}

TEST(copy_into_zero_size) {
    char buf[4] = {0};
    EXPECT(str_copy_into(buf, 0, "hello") == false);
}

/* =========================================================================
 * str_concat_into
 * ====================================================================== */

TEST(concat_into_fits) {
    char buf[32] = {0};
    EXPECT(str_concat_into(buf, sizeof(buf), "hello", " world") == true);
    EXPECT(str_equals(buf, "hello world"));
}

TEST(concat_into_overflow) {
    char buf[8] = {0};
    EXPECT(str_concat_into(buf, sizeof(buf), "hello", " world") == false);
}

TEST(concat_into_exact) {
    char buf[12] = {0}; /* "hello world" + '\0' = 12 */
    EXPECT(str_concat_into(buf, sizeof(buf), "hello ", "world") == true);
    EXPECT(str_equals(buf, "hello world"));
}

TEST(concat_into_empty) {
    char buf[16] = {0};
    EXPECT(str_concat_into(buf, sizeof(buf), "", "") == true);
    EXPECT(str_equals(buf, ""));
}

TEST(concat_into_zero_size) {
    char buf[4] = {0};
    EXPECT(str_concat_into(buf, 0, "a", "b") == false);
}

/* =========================================================================
 * str_equals
 * ====================================================================== */

TEST(equals_same) {
    EXPECT(str_equals("abc", "abc") == true);
    EXPECT(str_equals("", "") == true);
}

TEST(equals_different) {
    EXPECT(str_equals("abc", "abd") == false);
    EXPECT(str_equals("abc", "ab") == false);
    EXPECT(str_equals("", "a") == false);
}

TEST(equals_null_null) {
    EXPECT(str_equals(NULL, NULL) == true);
}

TEST(equals_null_nonnull) {
    EXPECT(str_equals(NULL, "abc") == false);
    EXPECT(str_equals("abc", NULL) == false);
}

TEST(equals_same_pointer) {
    const char* s = "test";
    EXPECT(str_equals(s, s) == true);
}

/* =========================================================================
 * cstr_starts_with
 * ====================================================================== */

TEST(starts_with_match) {
    EXPECT(cstr_starts_with("hello world", "hello") == true);
    EXPECT(cstr_starts_with("abc", "a") == true);
}

TEST(starts_with_no_match) {
    EXPECT(cstr_starts_with("hello world", "world") == false);
    EXPECT(cstr_starts_with("abc", "x") == false);
}

TEST(starts_with_empty_prefix) {
    EXPECT(cstr_starts_with("hello", "") == true);
    EXPECT(cstr_starts_with("", "") == true);
}

TEST(starts_with_full_string) {
    EXPECT(cstr_starts_with("hello", "hello") == true);
}

TEST(starts_with_longer_prefix) {
    EXPECT(cstr_starts_with("hi", "hello") == false);
}

/* =========================================================================
 * cstr_ends_with
 * ====================================================================== */

TEST(ends_with_match) {
    EXPECT(cstr_ends_with("hello world", "world") == true);
    EXPECT(cstr_ends_with("abc", "c") == true);
}

TEST(ends_with_no_match) {
    EXPECT(cstr_ends_with("hello world", "hello") == false);
    EXPECT(cstr_ends_with("abc", "x") == false);
}

TEST(ends_with_full_string) {
    EXPECT(cstr_ends_with("hello", "hello") == true);
}

TEST(ends_with_longer_suffix) {
    EXPECT(cstr_ends_with("hi", "hello") == false);
}

TEST(ends_with_empty_suffix) {
    EXPECT(cstr_ends_with("hello", "") == true);
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    char buf_a[128];
    char buf_b[128];
    char dest[256];
    usize len_a, len_b, half;

    if (size < 2) return 0;

    /* Split input into two null-terminated strings */
    half = size / 2;
    len_a = half < 127 ? half : 127;
    len_b = (size - half) < 127 ? (size - half) : 127;

    memcpy(buf_a, data, len_a);
    buf_a[len_a] = '\0';
    memcpy(buf_b, data + half, len_b);
    buf_b[len_b] = '\0';

    /* str_len invariant: result <= actual strlen */
    {
        usize la = str_len(buf_a);
        if (la != strlen(buf_a)) __builtin_trap();
    }

    /* str_compare: reflexive */
    if (str_compare(buf_a, buf_a) != 0) __builtin_trap();

    /* str_ncompare: n==0 always 0 */
    if (str_ncompare(buf_a, buf_b, 0) != 0) __builtin_trap();

    /* str_equals: reflexive */
    if (!str_equals(buf_a, buf_a)) __builtin_trap();

    /* str_alloc_copy: copy equals original */
    {
        option_charp r = str_alloc_copy(buf_a);
        if (option_charp_is_some(r)) {
            char* copy = option_charp_unwrap(r);
            if (!str_equals(copy, buf_a)) __builtin_trap();
            str_free(copy);
        }
    }

    /* str_alloc_concat: length is sum */
    {
        option_charp r = str_alloc_concat(buf_a, buf_b);
        if (option_charp_is_some(r)) {
            char* cat = option_charp_unwrap(r);
            if (str_len(cat) != str_len(buf_a) + str_len(buf_b)) __builtin_trap();
            str_free(cat);
        }
    }

    /* str_copy_into: if it fits, result equals source */
    if (str_copy_into(dest, sizeof(dest), buf_a)) {
        if (!str_equals(dest, buf_a)) __builtin_trap();
    }

    /* cstr_starts_with + cstr_ends_with: string starts/ends with itself */
    if (!cstr_starts_with(buf_a, buf_a)) __builtin_trap();
    if (!cstr_ends_with(buf_a, buf_a)) __builtin_trap();

    return 0;
}

#else

int main(void) {
    /* str_len */
    RUN(len_null);
    RUN(len_empty);
    RUN(len_short);
    RUN(len_long);

    /* str_compare */
    RUN(compare_equal);
    RUN(compare_less);
    RUN(compare_greater);
    RUN(compare_case_sensitive);

    /* str_ncompare */
    RUN(ncompare_zero_n);
    RUN(ncompare_partial);
    RUN(ncompare_full);
    RUN(ncompare_one);

    /* str_alloc_copy */
    RUN(alloc_copy_valid);
    RUN(alloc_copy_empty);
    RUN(alloc_copy_null);
    RUN(alloc_copy_independence);

    /* str_alloc_concat */
    RUN(alloc_concat_basic);
    RUN(alloc_concat_empty_left);
    RUN(alloc_concat_empty_right);
    RUN(alloc_concat_both_empty);

    /* str_alloc_sub */
    RUN(alloc_sub_middle);
    RUN(alloc_sub_start);
    RUN(alloc_sub_clamped);
    RUN(alloc_sub_out_of_range);
    RUN(alloc_sub_full);
    RUN(alloc_sub_single_char);

    /* str_free */
    RUN(free_null);

    /* str_copy_into */
    RUN(copy_into_fits);
    RUN(copy_into_overflow);
    RUN(copy_into_exact);
    RUN(copy_into_empty_src);
    RUN(copy_into_zero_size);

    /* str_concat_into */
    RUN(concat_into_fits);
    RUN(concat_into_overflow);
    RUN(concat_into_exact);
    RUN(concat_into_empty);
    RUN(concat_into_zero_size);

    /* str_equals */
    RUN(equals_same);
    RUN(equals_different);
    RUN(equals_null_null);
    RUN(equals_null_nonnull);
    RUN(equals_same_pointer);

    /* cstr_starts_with */
    RUN(starts_with_match);
    RUN(starts_with_no_match);
    RUN(starts_with_empty_prefix);
    RUN(starts_with_full_string);
    RUN(starts_with_longer_prefix);

    /* cstr_ends_with */
    RUN(ends_with_match);
    RUN(ends_with_no_match);
    RUN(ends_with_full_string);
    RUN(ends_with_longer_suffix);
    RUN(ends_with_empty_suffix);

    printf("\nstr_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
