/**
 * @file str_view_test.c
 * @brief Unit tests (and optional fuzz harness) for util/str/str_view.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput feeds arbitrary byte sequences to
 *              view creation, comparison, slicing, and search, verifying
 *              invariants on every input.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   str_view_null / empty / from / from_len — creation
 *   str_view_len / is_empty / is_null / has_data — queries
 *   str_view_equals / compare — comparison
 *   str_view_starts_with / ends_with — prefix/suffix
 *   str_view_sub / prefix / suffix — slicing
 *   str_view_find_char / contains_char — search
 *   str_view_equals_cstr / starts_with_cstr / ends_with_cstr — cstr helpers
 *   str_view_to_cstr — copy to buffer
 *   struct size — 16 bytes on 64-bit
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   str_view_to_cstr with NULL buf is a contract violation (require_msg →
 *   abort). Not tested here. {NULL, 0} is a valid null view, not a
 *   contract violation.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Loop variables declared before the loop body (C99). All char buffers
 *   zero-initialized to satisfy static analysis.
 */

#define CANON_CONTRACT_IMPL
#include "util/str/str_view.h"
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
 * Creation — str_view_null
 * ====================================================================== */

TEST(null_view) {
    str_view_t v = str_view_null();
    EXPECT(v.ptr == NULL);
    EXPECT(v.len == 0);
    EXPECT(str_view_is_null(v));
    EXPECT(str_view_is_empty(v));
    EXPECT(!str_view_has_data(v));
}

/* =========================================================================
 * Creation — str_view_empty
 * ====================================================================== */

TEST(empty_view) {
    str_view_t v = str_view_empty();
    EXPECT(v.ptr != NULL);
    EXPECT(v.len == 0);
    EXPECT(!str_view_is_null(v));
    EXPECT(str_view_is_empty(v));
    EXPECT(!str_view_has_data(v));
}

/* =========================================================================
 * Creation — str_view_from
 * ====================================================================== */

TEST(from_cstr) {
    str_view_t v = str_view_from("hello");
    EXPECT(v.len == 5);
    EXPECT(!str_view_is_null(v));
    EXPECT(!str_view_is_empty(v));
    EXPECT(str_view_has_data(v));
}

TEST(from_null) {
    str_view_t v = str_view_from(NULL);
    EXPECT(str_view_is_null(v));
    EXPECT(v.len == 0);
}

TEST(from_empty_cstr) {
    str_view_t v = str_view_from("");
    EXPECT(!str_view_is_null(v));
    EXPECT(str_view_is_empty(v));
    EXPECT(v.len == 0);
}

/* =========================================================================
 * Creation — str_view_from_len
 * ====================================================================== */

TEST(from_len) {
    const char* data = "hello world";
    str_view_t v = str_view_from_len(data + 6, 5);
    EXPECT(v.len == 5);
    EXPECT(str_view_equals_cstr(v, "world"));
}

TEST(from_len_zero) {
    str_view_t v = str_view_from_len("hello", 0);
    EXPECT(str_view_is_empty(v));
    EXPECT(!str_view_is_null(v));
}

/* =========================================================================
 * Queries — str_view_len
 * ====================================================================== */

TEST(len) {
    EXPECT(str_view_len(str_view_from("abc")) == 3);
    EXPECT(str_view_len(str_view_null()) == 0);
    EXPECT(str_view_len(str_view_empty()) == 0);
}

/* =========================================================================
 * Comparison — str_view_equals
 * ====================================================================== */

TEST(equals_same) {
    str_view_t a = str_view_from("hello");
    str_view_t b = str_view_from("hello");
    EXPECT(str_view_equals(a, b));
}

TEST(equals_different) {
    str_view_t a = str_view_from("hello");
    str_view_t b = str_view_from("world");
    EXPECT(!str_view_equals(a, b));
}

TEST(equals_different_length) {
    str_view_t a = str_view_from("hello");
    str_view_t b = str_view_from("hell");
    EXPECT(!str_view_equals(a, b));
}

TEST(equals_both_empty) {
    EXPECT(str_view_equals(str_view_empty(), str_view_empty()));
    EXPECT(str_view_equals(str_view_null(), str_view_null()));
    /* null and empty both have len==0, so they compare equal */
    EXPECT(str_view_equals(str_view_null(), str_view_empty()));
}

/* =========================================================================
 * Comparison — str_view_compare
 * ====================================================================== */

TEST(compare_equal) {
    str_view_t a = str_view_from("abc");
    str_view_t b = str_view_from("abc");
    EXPECT(str_view_compare(a, b) == 0);
}

TEST(compare_less) {
    str_view_t a = str_view_from("abc");
    str_view_t b = str_view_from("abd");
    EXPECT(str_view_compare(a, b) < 0);
}

TEST(compare_greater) {
    str_view_t a = str_view_from("abd");
    str_view_t b = str_view_from("abc");
    EXPECT(str_view_compare(a, b) > 0);
}

TEST(compare_prefix_shorter) {
    str_view_t a = str_view_from("abc");
    str_view_t b = str_view_from("abcd");
    EXPECT(str_view_compare(a, b) < 0);
}

TEST(compare_prefix_longer) {
    str_view_t a = str_view_from("abcd");
    str_view_t b = str_view_from("abc");
    EXPECT(str_view_compare(a, b) > 0);
}

TEST(compare_empties) {
    EXPECT(str_view_compare(str_view_empty(), str_view_empty()) == 0);
}

/* =========================================================================
 * Prefix / suffix — str_view_starts_with
 * ====================================================================== */

TEST(starts_with_match) {
    str_view_t v = str_view_from("hello world");
    str_view_t p = str_view_from("hello");
    EXPECT(str_view_starts_with(v, p));
}

TEST(starts_with_no_match) {
    str_view_t v = str_view_from("hello world");
    str_view_t p = str_view_from("world");
    EXPECT(!str_view_starts_with(v, p));
}

TEST(starts_with_empty_prefix) {
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_starts_with(v, str_view_empty()));
}

TEST(starts_with_full) {
    str_view_t v = str_view_from("hello");
    str_view_t p = str_view_from("hello");
    EXPECT(str_view_starts_with(v, p));
}

TEST(starts_with_longer) {
    str_view_t v = str_view_from("hi");
    str_view_t p = str_view_from("hello");
    EXPECT(!str_view_starts_with(v, p));
}

/* =========================================================================
 * Prefix / suffix — str_view_ends_with
 * ====================================================================== */

TEST(ends_with_match) {
    str_view_t v = str_view_from("hello world");
    str_view_t s = str_view_from("world");
    EXPECT(str_view_ends_with(v, s));
}

TEST(ends_with_no_match) {
    str_view_t v = str_view_from("hello world");
    str_view_t s = str_view_from("hello");
    EXPECT(!str_view_ends_with(v, s));
}

TEST(ends_with_empty_suffix) {
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_ends_with(v, str_view_empty()));
}

TEST(ends_with_full) {
    str_view_t v = str_view_from("hello");
    str_view_t s = str_view_from("hello");
    EXPECT(str_view_ends_with(v, s));
}

TEST(ends_with_longer) {
    str_view_t v = str_view_from("lo");
    str_view_t s = str_view_from("hello");
    EXPECT(!str_view_ends_with(v, s));
}

/* =========================================================================
 * Slicing — str_view_sub
 * ====================================================================== */

TEST(sub_middle) {
    str_view_t v = str_view_from("hello world");
    str_view_t s = str_view_sub(v, 6, 5);
    EXPECT(str_view_equals_cstr(s, "world"));
}

TEST(sub_start) {
    str_view_t v = str_view_from("hello world");
    str_view_t s = str_view_sub(v, 0, 5);
    EXPECT(str_view_equals_cstr(s, "hello"));
}

TEST(sub_clamped) {
    str_view_t v = str_view_from("hello");
    str_view_t s = str_view_sub(v, 3, 100);
    EXPECT(str_view_equals_cstr(s, "lo"));
}

TEST(sub_out_of_range) {
    str_view_t v = str_view_from("hello");
    str_view_t s = str_view_sub(v, 10, 5);
    EXPECT(str_view_is_empty(s));
}

TEST(sub_zero_len) {
    str_view_t v = str_view_from("hello");
    str_view_t s = str_view_sub(v, 2, 0);
    EXPECT(str_view_is_empty(s));
}

/* =========================================================================
 * Slicing — str_view_prefix / str_view_suffix
 * ====================================================================== */

TEST(prefix_op) {
    str_view_t v = str_view_from("hello world");
    str_view_t p = str_view_prefix(v, 5);
    EXPECT(str_view_equals_cstr(p, "hello"));
}

TEST(prefix_larger_than_len) {
    str_view_t v = str_view_from("hi");
    str_view_t p = str_view_prefix(v, 100);
    EXPECT(str_view_equals_cstr(p, "hi"));
}

TEST(suffix_op) {
    str_view_t v = str_view_from("hello world");
    str_view_t s = str_view_suffix(v, 5);
    EXPECT(str_view_equals_cstr(s, "world"));
}

TEST(suffix_larger_than_len) {
    str_view_t v = str_view_from("hi");
    str_view_t s = str_view_suffix(v, 100);
    EXPECT(str_view_equals_cstr(s, "hi"));
}

/* =========================================================================
 * Search — str_view_find_char
 * ====================================================================== */

TEST(find_char_found) {
    str_view_t v = str_view_from("hello world");
    EXPECT(str_view_find_char(v, ' ') == 5);
    EXPECT(str_view_find_char(v, 'h') == 0);
    EXPECT(str_view_find_char(v, 'd') == 10);
}

TEST(find_char_not_found) {
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_find_char(v, 'x') == v.len);
}

TEST(find_char_empty) {
    str_view_t v = str_view_empty();
    EXPECT(str_view_find_char(v, 'x') == 0);
}

/* =========================================================================
 * Search — str_view_contains_char
 * ====================================================================== */

TEST(contains_found) {
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_contains_char(v, 'e'));
    EXPECT(str_view_contains_char(v, 'o'));
}

TEST(contains_not_found) {
    str_view_t v = str_view_from("hello");
    EXPECT(!str_view_contains_char(v, 'x'));
}

/* =========================================================================
 * C-string helpers
 * ====================================================================== */

TEST(equals_cstr_match) {
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_equals_cstr(v, "hello"));
}

TEST(equals_cstr_no_match) {
    str_view_t v = str_view_from("hello");
    EXPECT(!str_view_equals_cstr(v, "world"));
}

TEST(starts_with_cstr_match) {
    str_view_t v = str_view_from("hello world");
    EXPECT(str_view_starts_with_cstr(v, "hello"));
}

TEST(ends_with_cstr_match) {
    str_view_t v = str_view_from("hello world");
    EXPECT(str_view_ends_with_cstr(v, "world"));
}

/* =========================================================================
 * str_view_to_cstr
 * ====================================================================== */

TEST(to_cstr_fits) {
    char buf[16] = {0};
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_to_cstr(v, buf, sizeof(buf)) == true);
    EXPECT(strcmp(buf, "hello") == 0);
}

TEST(to_cstr_exact) {
    char buf[6] = {0}; /* "hello" + '\0' */
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_to_cstr(v, buf, sizeof(buf)) == true);
    EXPECT(strcmp(buf, "hello") == 0);
}

TEST(to_cstr_overflow) {
    char buf[4] = {0};
    str_view_t v = str_view_from("hello");
    EXPECT(str_view_to_cstr(v, buf, sizeof(buf)) == false);
}

TEST(to_cstr_empty_view) {
    char buf[4] = {0};
    str_view_t v = str_view_empty();
    EXPECT(str_view_to_cstr(v, buf, sizeof(buf)) == true);
    EXPECT(strcmp(buf, "") == 0);
}

TEST(to_cstr_null_view) {
    char buf[4] = {0};
    str_view_t v = str_view_null();
    /* len==0, buf_size > 0 → copies nothing, sets buf[0]='\0' */
    EXPECT(str_view_to_cstr(v, buf, sizeof(buf)) == true);
    EXPECT(strcmp(buf, "") == 0);
}

/* =========================================================================
 * Struct size
 * ====================================================================== */

TEST(struct_size) {
    EXPECT(sizeof(str_view_t) == sizeof(const char*) + sizeof(usize));
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    str_view_t v;
    str_view_t sub;
    char buf[256] = {0};

    if (size == 0) return 0;

    v = str_view_from_len((const char*)data, size);

    /* len must match */
    if (str_view_len(v) != size) __builtin_trap();

    /* reflexive equality */
    if (!str_view_equals(v, v)) __builtin_trap();

    /* compare reflexive */
    if (str_view_compare(v, v) != 0) __builtin_trap();

    /* starts/ends with self */
    if (!str_view_starts_with(v, v)) __builtin_trap();
    if (!str_view_ends_with(v, v)) __builtin_trap();

    /* starts/ends with empty */
    if (!str_view_starts_with(v, str_view_empty())) __builtin_trap();
    if (!str_view_ends_with(v, str_view_empty())) __builtin_trap();

    /* sub(0, len) == self */
    sub = str_view_sub(v, 0, v.len);
    if (!str_view_equals(sub, v)) __builtin_trap();

    /* prefix(len) == self */
    if (!str_view_equals(str_view_prefix(v, v.len), v)) __builtin_trap();

    /* suffix(len) == self */
    if (!str_view_equals(str_view_suffix(v, v.len), v)) __builtin_trap();

    /* find_char result <= len */
    {
        usize idx = str_view_find_char(v, (char)data[0]);
        if (idx > v.len) __builtin_trap();
        /* first char must be found at index 0 */
        if (idx != 0) __builtin_trap();
    }

    /* to_cstr with large buffer must succeed */
    if (size < 255) {
        if (!str_view_to_cstr(v, buf, sizeof(buf))) __builtin_trap();
        if (str_len(buf) != size) __builtin_trap();
    }

    return 0;
}

#else

int main(void) {
    /* Creation */
    RUN(null_view);
    RUN(empty_view);
    RUN(from_cstr);
    RUN(from_null);
    RUN(from_empty_cstr);
    RUN(from_len);
    RUN(from_len_zero);

    /* Queries */
    RUN(len);

    /* Comparison */
    RUN(equals_same);
    RUN(equals_different);
    RUN(equals_different_length);
    RUN(equals_both_empty);
    RUN(compare_equal);
    RUN(compare_less);
    RUN(compare_greater);
    RUN(compare_prefix_shorter);
    RUN(compare_prefix_longer);
    RUN(compare_empties);

    /* Prefix / suffix */
    RUN(starts_with_match);
    RUN(starts_with_no_match);
    RUN(starts_with_empty_prefix);
    RUN(starts_with_full);
    RUN(starts_with_longer);
    RUN(ends_with_match);
    RUN(ends_with_no_match);
    RUN(ends_with_empty_suffix);
    RUN(ends_with_full);
    RUN(ends_with_longer);

    /* Slicing */
    RUN(sub_middle);
    RUN(sub_start);
    RUN(sub_clamped);
    RUN(sub_out_of_range);
    RUN(sub_zero_len);
    RUN(prefix_op);
    RUN(prefix_larger_than_len);
    RUN(suffix_op);
    RUN(suffix_larger_than_len);

    /* Search */
    RUN(find_char_found);
    RUN(find_char_not_found);
    RUN(find_char_empty);
    RUN(contains_found);
    RUN(contains_not_found);

    /* C-string helpers */
    RUN(equals_cstr_match);
    RUN(equals_cstr_no_match);
    RUN(starts_with_cstr_match);
    RUN(ends_with_cstr_match);

    /* to_cstr */
    RUN(to_cstr_fits);
    RUN(to_cstr_exact);
    RUN(to_cstr_overflow);
    RUN(to_cstr_empty_view);
    RUN(to_cstr_null_view);

    /* Struct */
    RUN(struct_size);

    printf("\nstr_view_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
