/**
 * @file str_join_test.c
 * @brief Unit tests (and optional fuzz harness) for util/str/str_join.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput feeds arbitrary strings to
 *              str_join and str_split_to_parts, verifying invariants.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   str_join            — basic, separator, empty parts, zero count,
 *                         buffer overflow, exact fit, NULL element,
 *                         NULL separator, single element
 *   str_alloc_join      — basic, zero count, NULL element, free
 *   str_split_to_parts  — basic, consecutive delimiters, leading/trailing,
 *                         scratch exhaustion, max_parts limit
 *   str_rejoin          — delimiter replacement, no matches
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL dest, NULL parts array, NULL scratch_buf are contract violations
 *   (require_msg → abort). Not tested here. NULL element inside parts[]
 *   is a data error (returns false/None) and IS tested.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Loop variables are declared before the loop body (C99).
 *   All char buffers are zero-initialized to satisfy static analysis.
 */

#define CANON_CONTRACT_IMPL
#include "util/str/str_join.h"
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
 * str_join — basic
 * ====================================================================== */

TEST(join_basic) {
    char buf[64] = {0};
    const char* parts[] = {"hello", "world"};
    EXPECT(str_join(buf, sizeof(buf), parts, 2, " ") == true);
    EXPECT(str_equals(buf, "hello world"));
}

TEST(join_comma_sep) {
    char buf[64] = {0};
    const char* parts[] = {"a", "b", "c"};
    EXPECT(str_join(buf, sizeof(buf), parts, 3, ", ") == true);
    EXPECT(str_equals(buf, "a, b, c"));
}

TEST(join_no_sep) {
    char buf[64] = {0};
    const char* parts[] = {"foo", "bar"};
    EXPECT(str_join(buf, sizeof(buf), parts, 2, "") == true);
    EXPECT(str_equals(buf, "foobar"));
}

TEST(join_null_sep) {
    char buf[64] = {0};
    const char* parts[] = {"foo", "bar"};
    EXPECT(str_join(buf, sizeof(buf), parts, 2, NULL) == true);
    EXPECT(str_equals(buf, "foobar"));
}

/* =========================================================================
 * str_join — single element
 * ====================================================================== */

TEST(join_single) {
    char buf[64] = {0};
    const char* parts[] = {"only"};
    EXPECT(str_join(buf, sizeof(buf), parts, 1, ",") == true);
    EXPECT(str_equals(buf, "only"));
}

/* =========================================================================
 * str_join — zero count
 * ====================================================================== */

TEST(join_zero_count) {
    char buf[64] = {0};
    const char* parts[] = {"ignored"};
    buf[0] = 'X';
    EXPECT(str_join(buf, sizeof(buf), parts, 0, ",") == true);
    EXPECT(str_equals(buf, ""));
}

/* =========================================================================
 * str_join — empty strings in parts
 * ====================================================================== */

TEST(join_empty_parts) {
    char buf[64] = {0};
    const char* parts[] = {"", "b", ""};
    EXPECT(str_join(buf, sizeof(buf), parts, 3, "-") == true);
    EXPECT(str_equals(buf, "-b-"));
}

TEST(join_all_empty) {
    char buf[64] = {0};
    const char* parts[] = {"", "", ""};
    EXPECT(str_join(buf, sizeof(buf), parts, 3, ",") == true);
    EXPECT(str_equals(buf, ",,"));
}

/* =========================================================================
 * str_join — buffer overflow
 * ====================================================================== */

TEST(join_overflow) {
    char buf[8] = {0};
    const char* parts[] = {"hello", "world"};
    EXPECT(str_join(buf, sizeof(buf), parts, 2, " ") == false);
    EXPECT(buf[0] == '\0'); /* dest cleared on failure */
}

/* =========================================================================
 * str_join — exact fit
 * ====================================================================== */

TEST(join_exact_fit) {
    /* "a-b" = 3 chars + '\0' = 4 bytes */
    char buf[4] = {0};
    const char* parts[] = {"a", "b"};
    EXPECT(str_join(buf, sizeof(buf), parts, 2, "-") == true);
    EXPECT(str_equals(buf, "a-b"));
}

TEST(join_one_byte_short) {
    char buf[3] = {0};
    const char* parts[] = {"a", "b"};
    EXPECT(str_join(buf, sizeof(buf), parts, 2, "-") == false);
    EXPECT(buf[0] == '\0');
}

/* =========================================================================
 * str_join — NULL element in parts
 * ====================================================================== */

TEST(join_null_element) {
    char buf[64] = {0};
    const char* parts[] = {"a", NULL, "c"};
    EXPECT(str_join(buf, sizeof(buf), parts, 3, ",") == false);
    EXPECT(buf[0] == '\0');
}

/* =========================================================================
 * str_alloc_join — basic
 * ====================================================================== */

TEST(alloc_join_basic) {
    const char* parts[] = {"hello", "world"};
    option_charp r = str_alloc_join(parts, 2, " ");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "hello world"));
    str_free(s);
}

TEST(alloc_join_multi_sep) {
    const char* parts[] = {"x", "y", "z"};
    option_charp r = str_alloc_join(parts, 3, "---");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "x---y---z"));
    str_free(s);
}

/* =========================================================================
 * str_alloc_join — zero count
 * ====================================================================== */

TEST(alloc_join_zero) {
    const char* parts[] = {"ignored"};
    option_charp r = str_alloc_join(parts, 0, ",");
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_len(s) == 0);
    str_free(s);
}

/* =========================================================================
 * str_alloc_join — NULL element
 * ====================================================================== */

TEST(alloc_join_null_element) {
    const char* parts[] = {"a", NULL};
    option_charp r = str_alloc_join(parts, 2, ",");
    EXPECT(option_charp_is_none(r));
}

/* =========================================================================
 * str_alloc_join — NULL separator
 * ====================================================================== */

TEST(alloc_join_null_sep) {
    const char* parts[] = {"ab", "cd"};
    option_charp r = str_alloc_join(parts, 2, NULL);
    EXPECT(option_charp_is_some(r));
    char* s = option_charp_unwrap(r);
    EXPECT(str_equals(s, "abcd"));
    str_free(s);
}

/* =========================================================================
 * str_split_to_parts — basic
 * ====================================================================== */

TEST(split_basic) {
    const char* out[8];
    char scratch[64] = {0};
    usize n = str_split_to_parts("a.b.c", '.', out, 8, scratch, sizeof(scratch));
    EXPECT(n == 3);
    EXPECT(str_equals(out[0], "a"));
    EXPECT(str_equals(out[1], "b"));
    EXPECT(str_equals(out[2], "c"));
}

/* =========================================================================
 * str_split_to_parts — consecutive delimiters (skipped)
 * ====================================================================== */

TEST(split_consecutive) {
    const char* out[8];
    char scratch[64] = {0};
    usize n = str_split_to_parts("a..b...c", '.', out, 8, scratch, sizeof(scratch));
    EXPECT(n == 3);
    EXPECT(str_equals(out[0], "a"));
    EXPECT(str_equals(out[1], "b"));
    EXPECT(str_equals(out[2], "c"));
}

/* =========================================================================
 * str_split_to_parts — leading/trailing delimiters
 * ====================================================================== */

TEST(split_leading_trailing) {
    const char* out[8];
    char scratch[64] = {0};
    usize n = str_split_to_parts(".a.b.", '.', out, 8, scratch, sizeof(scratch));
    EXPECT(n == 2);
    EXPECT(str_equals(out[0], "a"));
    EXPECT(str_equals(out[1], "b"));
}

/* =========================================================================
 * str_split_to_parts — single element (no delimiter)
 * ====================================================================== */

TEST(split_no_delim) {
    const char* out[8];
    char scratch[64] = {0};
    usize n = str_split_to_parts("hello", '.', out, 8, scratch, sizeof(scratch));
    EXPECT(n == 1);
    EXPECT(str_equals(out[0], "hello"));
}

/* =========================================================================
 * str_split_to_parts — empty string
 * ====================================================================== */

TEST(split_empty) {
    const char* out[8];
    char scratch[64] = {0};
    usize n = str_split_to_parts("", '.', out, 8, scratch, sizeof(scratch));
    EXPECT(n == 0);
}

/* =========================================================================
 * str_split_to_parts — all delimiters
 * ====================================================================== */

TEST(split_all_delims) {
    const char* out[8];
    char scratch[64] = {0};
    usize n = str_split_to_parts("...", '.', out, 8, scratch, sizeof(scratch));
    EXPECT(n == 0);
}

/* =========================================================================
 * str_split_to_parts — max_parts limit
 * ====================================================================== */

TEST(split_max_parts) {
    const char* out[2];
    char scratch[64] = {0};
    /* Only room for 2 parts, but input has 4 */
    usize n = str_split_to_parts("a.b.c.d", '.', out, 2, scratch, sizeof(scratch));
    /* Returns 0 because it hits max_parts on the 3rd element */
    EXPECT(n == 0);
}

/* =========================================================================
 * str_split_to_parts — scratch exhaustion
 * ====================================================================== */

TEST(split_scratch_exhaustion) {
    const char* out[8];
    char scratch[4] = {0}; /* Too small for "hello" + '\0' */
    usize n = str_split_to_parts("hello.world", '.', out, 8, scratch, sizeof(scratch));
    EXPECT(n == 0);
}

/* =========================================================================
 * str_rejoin — basic delimiter replacement
 * ====================================================================== */

TEST(rejoin_basic) {
    char dest[64] = {0};
    const char* pbuf[8];
    char scratch[64] = {0};
    EXPECT(str_rejoin("a.b.c", '.', pbuf, 8, scratch, sizeof(scratch),
                      dest, sizeof(dest), "/") == true);
    EXPECT(str_equals(dest, "a/b/c"));
}

TEST(rejoin_multi_char_sep) {
    char dest[64] = {0};
    const char* pbuf[8];
    char scratch[64] = {0};
    EXPECT(str_rejoin("x-y-z", '-', pbuf, 8, scratch, sizeof(scratch),
                      dest, sizeof(dest), " -> ") == true);
    EXPECT(str_equals(dest, "x -> y -> z"));
}

/* =========================================================================
 * str_rejoin — no matches (single part)
 * ====================================================================== */

TEST(rejoin_no_match) {
    char dest[64] = {0};
    const char* pbuf[8];
    char scratch[64] = {0};
    EXPECT(str_rejoin("hello", '.', pbuf, 8, scratch, sizeof(scratch),
                      dest, sizeof(dest), "/") == true);
    EXPECT(str_equals(dest, "hello"));
}

/* =========================================================================
 * str_rejoin — empty input
 * ====================================================================== */

TEST(rejoin_empty) {
    char dest[64] = {0};
    const char* pbuf[8];
    char scratch[64] = {0};
    EXPECT(str_rejoin("", '.', pbuf, 8, scratch, sizeof(scratch),
                      dest, sizeof(dest), "/") == false);
    EXPECT(dest[0] == '\0');
}

/* =========================================================================
 * str_rejoin — dest too small
 * ====================================================================== */

TEST(rejoin_dest_overflow) {
    char dest[4] = {0};
    const char* pbuf[8];
    char scratch[64] = {0};
    EXPECT(str_rejoin("hello.world", '.', pbuf, 8, scratch, sizeof(scratch),
                      dest, sizeof(dest), "/") == false);
    EXPECT(dest[0] == '\0');
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    char buf_a[128];
    char buf_b[16];
    char dest[256] = {0};
    const char* parts[8];
    char scratch[256] = {0};
    usize half, len_a, len_b, n, i;

    if (size < 4) return 0;

    /* Build two null-terminated strings from input */
    half = size / 2;
    len_a = half < 127 ? half : 127;
    len_b = (size - half) < 15 ? (size - half) : 15;

    memcpy(buf_a, data, len_a);
    buf_a[len_a] = '\0';
    memcpy(buf_b, data + half, len_b);
    buf_b[len_b] = '\0';

    /* str_split_to_parts + str_join round-trip invariant:
     * split then join should not crash and result fits if dest is large enough */
    if (len_b > 0) {
        char delim = buf_b[0];
        n = str_split_to_parts(buf_a, delim, parts, 8, scratch, sizeof(scratch));

        if (n > 0) {
            /* All parts must be non-NULL */
            for (i = 0; i < n; i++) {
                if (!parts[i]) __builtin_trap();
            }

            /* Join with same delimiter should not crash */
            char sep[2];
            sep[0] = delim;
            sep[1] = '\0';
            (void)str_join(dest, sizeof(dest), parts, n, sep);
        }
    }

    /* str_join with known-good parts should always succeed in large buffer */
    {
        const char* fixed[] = {buf_a, buf_b};
        if (!str_join(dest, sizeof(dest), fixed, 2, ",")) {
            /* Should only fail if total > 256, which is impossible with 127+15+1+1 */
            __builtin_trap();
        }
    }

    return 0;
}

#else

int main(void) {
    /* str_join */
    RUN(join_basic);
    RUN(join_comma_sep);
    RUN(join_no_sep);
    RUN(join_null_sep);
    RUN(join_single);
    RUN(join_zero_count);
    RUN(join_empty_parts);
    RUN(join_all_empty);
    RUN(join_overflow);
    RUN(join_exact_fit);
    RUN(join_one_byte_short);
    RUN(join_null_element);

    /* str_alloc_join */
    RUN(alloc_join_basic);
    RUN(alloc_join_multi_sep);
    RUN(alloc_join_zero);
    RUN(alloc_join_null_element);
    RUN(alloc_join_null_sep);

    /* str_split_to_parts */
    RUN(split_basic);
    RUN(split_consecutive);
    RUN(split_leading_trailing);
    RUN(split_no_delim);
    RUN(split_empty);
    RUN(split_all_delims);
    RUN(split_max_parts);
    RUN(split_scratch_exhaustion);

    /* str_rejoin */
    RUN(rejoin_basic);
    RUN(rejoin_multi_char_sep);
    RUN(rejoin_no_match);
    RUN(rejoin_empty);
    RUN(rejoin_dest_overflow);

    printf("\nstr_join_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
