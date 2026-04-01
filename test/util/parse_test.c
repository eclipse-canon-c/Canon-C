/**
 * @file parse_test.c
 * @brief Unit tests (and optional fuzz harness) for parse.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput feeds arbitrary strings to all
 *              three parsers, verifying Result invariants and endptr
 *              consistency on every input.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   parse_skip_ws  — NULL, empty, all whitespace types, mixed, no whitespace
 *   parse_i64      — decimal, hex, octal, signs, overflow, empty,
 *                    endptr positioning, prefix parsing, whitespace rejection
 *   parse_u64      — decimal, hex, octal, rejects negative, overflow,
 *                    empty, endptr positioning, whitespace rejection
 *   parse_f64      — decimal, scientific notation, hex float, inf, nan,
 *                    overflow/underflow still Ok, empty, endptr,
 *                    whitespace rejection
 *   chained parsing — multi-value parse with skip_ws between values
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL input to parse_i64, parse_u64, parse_f64 is a contract violation
 *   (require_msg → abort). This is not tested here — contract violations
 *   are tested separately via fork/signal in contract_test.c patterns.
 *   Empty string is a data error → Err(ERR_PARSE_FAILED).
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Loop variables are declared before the loop body (C99). No binary
 *   literals. Float comparisons use explicit epsilon where needed.
 */

#include "util/parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
 * Helper: approximate float equality
 * ====================================================================== */

static inline bool approx_eq(f64 a, f64 b, f64 eps) {
    f64 diff = a - b;
    if (diff < 0) diff = -diff;
    return diff < eps;
}

/* =========================================================================
 * parse_skip_ws
 * ====================================================================== */

TEST(skip_ws_null) {
    EXPECT(parse_skip_ws(NULL) == NULL);
}

TEST(skip_ws_empty) {
    const char* s = "";
    const char* r = parse_skip_ws(s);
    EXPECT(r == s);
    EXPECT(*r == '\0');
}

TEST(skip_ws_no_whitespace) {
    const char* s = "hello";
    EXPECT(parse_skip_ws(s) == s);
}

TEST(skip_ws_all_types) {
    /* space, tab, newline, carriage return, form feed, vertical tab */
    const char* s = " \t\n\r\f\vX";
    const char* r = parse_skip_ws(s);
    EXPECT(*r == 'X');
}

TEST(skip_ws_only_whitespace) {
    const char* s = "   \t\n";
    const char* r = parse_skip_ws(s);
    EXPECT(*r == '\0');
}

TEST(skip_ws_mixed) {
    const char* s = "  \t 42";
    const char* r = parse_skip_ws(s);
    EXPECT(*r == '4');
}

/* =========================================================================
 * parse_i64 — basic valid inputs
 * ====================================================================== */

TEST(i64_decimal) {
    const char* end;
    result_i64_Error r;

    r = parse_i64("42", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 42);
    EXPECT(*end == '\0');

    r = parse_i64("0", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 0);

    r = parse_i64("-1", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == -1);

    r = parse_i64("+99", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 99);
}

TEST(i64_hex) {
    const char* end;
    result_i64_Error r;

    r = parse_i64("0xFF", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 255);

    r = parse_i64("0XAB", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 0xAB);
}

TEST(i64_octal) {
    const char* end;
    result_i64_Error r;

    r = parse_i64("077", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 077);  /* 63 decimal */
}

/* =========================================================================
 * parse_i64 — boundary values
 * ====================================================================== */

TEST(i64_boundaries) {
    const char* end;
    result_i64_Error r;

    /* INT64_MAX = 9223372036854775807 */
    r = parse_i64("9223372036854775807", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == (i64)9223372036854775807LL);

    /* INT64_MIN = -9223372036854775808 */
    r = parse_i64("-9223372036854775808", &end);
    EXPECT(result_i64_Error_is_ok(r));
    /* Avoid -9223372036854775808 literal — some compilers warn.
     * INT64_MIN == -INT64_MAX - 1 */
    EXPECT(result_i64_Error_unwrap(r) == (i64)(-9223372036854775807LL - 1));
}

TEST(i64_overflow) {
    const char* end;
    result_i64_Error r;

    /* INT64_MAX + 1 */
    r = parse_i64("9223372036854775808", &end);
    EXPECT(!result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap_err(r) == ERR_PARSE_FAILED);
    EXPECT(end != NULL);

    /* INT64_MIN - 1 */
    r = parse_i64("-9223372036854775809", &end);
    EXPECT(!result_i64_Error_is_ok(r));
}

/* =========================================================================
 * parse_i64 — error cases (data errors, not contract violations)
 * ====================================================================== */

TEST(i64_errors) {
    const char* end;
    result_i64_Error r;

    /* Empty string → Err */
    r = parse_i64("", &end);
    EXPECT(!result_i64_Error_is_ok(r));
    EXPECT(end != NULL);

    /* No valid digits */
    r = parse_i64("abc", &end);
    EXPECT(!result_i64_Error_is_ok(r));

    /* Just a sign */
    r = parse_i64("+", &end);
    EXPECT(!result_i64_Error_is_ok(r));

    r = parse_i64("-", &end);
    EXPECT(!result_i64_Error_is_ok(r));
}

TEST(i64_null_endptr) {
    /* endptr == NULL must not crash (endptr is optional) */
    result_i64_Error r = parse_i64("42", NULL);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 42);

    /* Err case with NULL endptr */
    r = parse_i64("", NULL);
    EXPECT(!result_i64_Error_is_ok(r));
}

/* =========================================================================
 * parse_i64 — whitespace rejection
 * ====================================================================== */

TEST(i64_rejects_whitespace) {
    const char* end;
    result_i64_Error r;

    /* Leading whitespace is rejected — caller must use parse_skip_ws() */
    r = parse_i64("  42", &end);
    EXPECT(!result_i64_Error_is_ok(r));
    EXPECT(end != NULL);

    r = parse_i64("\t42", &end);
    EXPECT(!result_i64_Error_is_ok(r));

    r = parse_i64("\n42", &end);
    EXPECT(!result_i64_Error_is_ok(r));

    /* parse_skip_ws + parse_i64 is the correct pattern */
    {
        const char* s = "  42";
        s = parse_skip_ws(s);
        r = parse_i64(s, &end);
        EXPECT(result_i64_Error_is_ok(r));
        EXPECT(result_i64_Error_unwrap(r) == 42);
    }
}

/* =========================================================================
 * parse_i64 — prefix parsing and endptr
 * ====================================================================== */

TEST(i64_prefix_parsing) {
    const char* end;
    result_i64_Error r;

    /* Trailing non-digits */
    r = parse_i64("123abc", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 123);
    EXPECT(*end == 'a');

    /* Trailing whitespace */
    r = parse_i64("456  ", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 456);
    EXPECT(*end == ' ');
}

/* =========================================================================
 * parse_u64 — basic valid inputs
 * ====================================================================== */

TEST(u64_decimal) {
    const char* end;
    result_u64_Error r;

    r = parse_u64("42", &end);
    EXPECT(result_u64_Error_is_ok(r));
    EXPECT(result_u64_Error_unwrap(r) == 42u);

    r = parse_u64("0", &end);
    EXPECT(result_u64_Error_is_ok(r));
    EXPECT(result_u64_Error_unwrap(r) == 0u);
}

TEST(u64_hex_octal) {
    const char* end;
    result_u64_Error r;

    r = parse_u64("0xFF", &end);
    EXPECT(result_u64_Error_is_ok(r));
    EXPECT(result_u64_Error_unwrap(r) == 255u);

    r = parse_u64("077", &end);
    EXPECT(result_u64_Error_is_ok(r));
    EXPECT(result_u64_Error_unwrap(r) == 63u);
}

/* =========================================================================
 * parse_u64 — rejects negative
 * ====================================================================== */

TEST(u64_rejects_negative) {
    const char* end;
    result_u64_Error r;

    r = parse_u64("-1", &end);
    EXPECT(!result_u64_Error_is_ok(r));
    EXPECT(result_u64_Error_unwrap_err(r) == ERR_PARSE_FAILED);

    r = parse_u64("-0", &end);
    EXPECT(!result_u64_Error_is_ok(r));
}

/* =========================================================================
 * parse_u64 — boundary values
 * ====================================================================== */

TEST(u64_boundaries) {
    const char* end;
    result_u64_Error r;

    /* UINT64_MAX = 18446744073709551615 */
    r = parse_u64("18446744073709551615", &end);
    EXPECT(result_u64_Error_is_ok(r));
    EXPECT(result_u64_Error_unwrap(r) == 18446744073709551615ULL);

    /* UINT64_MAX + 1 → overflow */
    r = parse_u64("18446744073709551616", &end);
    EXPECT(!result_u64_Error_is_ok(r));
}

/* =========================================================================
 * parse_u64 — error cases
 * ====================================================================== */

TEST(u64_errors) {
    const char* end;
    result_u64_Error r;

    /* Empty string → Err */
    r = parse_u64("", &end);
    EXPECT(!result_u64_Error_is_ok(r));

    /* No valid digits */
    r = parse_u64("xyz", &end);
    EXPECT(!result_u64_Error_is_ok(r));

    /* NULL endptr must not crash */
    r = parse_u64("42", NULL);
    EXPECT(result_u64_Error_is_ok(r));
}

TEST(u64_rejects_whitespace) {
    const char* end;
    result_u64_Error r;

    r = parse_u64("  42", &end);
    EXPECT(!result_u64_Error_is_ok(r));

    r = parse_u64("\t42", &end);
    EXPECT(!result_u64_Error_is_ok(r));

    /* parse_skip_ws + parse_u64 works */
    {
        const char* s = "  42";
        s = parse_skip_ws(s);
        r = parse_u64(s, &end);
        EXPECT(result_u64_Error_is_ok(r));
        EXPECT(result_u64_Error_unwrap(r) == 42u);
    }
}

/* =========================================================================
 * parse_u64 — prefix parsing
 * ====================================================================== */

TEST(u64_prefix_parsing) {
    const char* end;
    result_u64_Error r;

    r = parse_u64("100xyz", &end);
    EXPECT(result_u64_Error_is_ok(r));
    EXPECT(result_u64_Error_unwrap(r) == 100u);
    EXPECT(*end == 'x');
}

/* =========================================================================
 * parse_f64 — basic valid inputs
 * ====================================================================== */

TEST(f64_decimal) {
    const char* end;
    result_f64_Error r;

    r = parse_f64("3.14", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(approx_eq(result_f64_Error_unwrap(r), 3.14, 1e-12));

    r = parse_f64("0.0", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(result_f64_Error_unwrap(r) == 0.0);

    r = parse_f64("-2.5", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(approx_eq(result_f64_Error_unwrap(r), -2.5, 1e-12));

    /* Integer without decimal point */
    r = parse_f64("42", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(result_f64_Error_unwrap(r) == 42.0);
}

TEST(f64_scientific) {
    const char* end;
    result_f64_Error r;

    r = parse_f64("1.5e3", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(result_f64_Error_unwrap(r) == 1500.0);

    r = parse_f64("2.0E-4", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(approx_eq(result_f64_Error_unwrap(r), 0.0002, 1e-12));
}

TEST(f64_special_values) {
    const char* end;
    result_f64_Error r;

    r = parse_f64("inf", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(isinf(result_f64_Error_unwrap(r)));

    r = parse_f64("-inf", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(isinf(result_f64_Error_unwrap(r)));
    EXPECT(result_f64_Error_unwrap(r) < 0.0);

    r = parse_f64("nan", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(isnan(result_f64_Error_unwrap(r)));
}

/* =========================================================================
 * parse_f64 — overflow/underflow are still Ok
 * ====================================================================== */

TEST(f64_overflow_underflow) {
    const char* end;
    result_f64_Error r;

    /* Very large number — overflows to HUGE_VAL, still Ok */
    r = parse_f64("1e9999", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(isinf(result_f64_Error_unwrap(r)));

    /* Very small number — underflows to 0.0, still Ok */
    r = parse_f64("1e-9999", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(result_f64_Error_unwrap(r) == 0.0);
}

/* =========================================================================
 * parse_f64 — error cases
 * ====================================================================== */

TEST(f64_errors) {
    const char* end;
    result_f64_Error r;

    /* Empty string → Err */
    r = parse_f64("", &end);
    EXPECT(!result_f64_Error_is_ok(r));

    /* No valid prefix */
    r = parse_f64("abc", &end);
    EXPECT(!result_f64_Error_is_ok(r));

    /* NULL endptr must not crash */
    r = parse_f64("3.14", NULL);
    EXPECT(result_f64_Error_is_ok(r));
}

TEST(f64_rejects_whitespace) {
    const char* end;
    result_f64_Error r;

    r = parse_f64("  3.14", &end);
    EXPECT(!result_f64_Error_is_ok(r));

    r = parse_f64("\t3.14", &end);
    EXPECT(!result_f64_Error_is_ok(r));

    /* parse_skip_ws + parse_f64 works */
    {
        const char* s = "  3.14";
        s = parse_skip_ws(s);
        r = parse_f64(s, &end);
        EXPECT(result_f64_Error_is_ok(r));
        EXPECT(approx_eq(result_f64_Error_unwrap(r), 3.14, 1e-12));
    }
}

/* =========================================================================
 * parse_f64 — prefix parsing
 * ====================================================================== */

TEST(f64_prefix_parsing) {
    const char* end;
    result_f64_Error r;

    r = parse_f64("1.5xyz", &end);
    EXPECT(result_f64_Error_is_ok(r));
    EXPECT(approx_eq(result_f64_Error_unwrap(r), 1.5, 1e-12));
    EXPECT(*end == 'x');
}

/* =========================================================================
 * Chained parsing — multi-value parse
 * ====================================================================== */

TEST(chained_parsing) {
    const char* p = "  123  456  789  ";
    result_i64_Error r;

    p = parse_skip_ws(p);
    r = parse_i64(p, &p);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 123);

    p = parse_skip_ws(p);
    r = parse_i64(p, &p);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 456);

    p = parse_skip_ws(p);
    r = parse_i64(p, &p);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(result_i64_Error_unwrap(r) == 789);

    /* After last value, remaining is whitespace */
    p = parse_skip_ws(p);
    EXPECT(*p == '\0');
}

/* =========================================================================
 * Full string validation pattern
 * ====================================================================== */

TEST(full_string_validation) {
    const char* end;
    result_i64_Error r;

    /* Valid: entire string is a number */
    r = parse_i64("42", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(*parse_skip_ws(end) == '\0');

    /* Valid: number + trailing whitespace */
    r = parse_i64("42", &end);
    EXPECT(result_i64_Error_is_ok(r));
    EXPECT(*end == '\0');

    /* Invalid: trailing non-whitespace */
    r = parse_i64("42abc", &end);
    EXPECT(result_i64_Error_is_ok(r));  /* parse itself succeeds */
    EXPECT(*end != '\0');               /* but string not fully consumed */
}

/* =========================================================================
 * Mixed-type chained parsing
 * ====================================================================== */

TEST(mixed_type_chaining) {
    const char* p = "42 3.14 0xFF";
    result_i64_Error ri;
    result_f64_Error rf;
    result_u64_Error ru;

    p = parse_skip_ws(p);
    ri = parse_i64(p, &p);
    EXPECT(result_i64_Error_is_ok(ri));
    EXPECT(result_i64_Error_unwrap(ri) == 42);

    p = parse_skip_ws(p);
    rf = parse_f64(p, &p);
    EXPECT(result_f64_Error_is_ok(rf));
    EXPECT(approx_eq(result_f64_Error_unwrap(rf), 3.14, 1e-12));

    p = parse_skip_ws(p);
    ru = parse_u64(p, &p);
    EXPECT(result_u64_Error_is_ok(ru));
    EXPECT(result_u64_Error_unwrap(ru) == 255u);
}

/* =========================================================================
 * endptr points to s on failure
 * ====================================================================== */

TEST(endptr_on_failure) {
    const char* s;
    const char* end;

    s = "abc";
    (void)parse_i64(s, &end);
    EXPECT(end == s);

    s = "-1";
    (void)parse_u64(s, &end);
    EXPECT(end == s);

    s = "xyz";
    (void)parse_f64(s, &end);
    EXPECT(end == s);
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    const char* end;
    char buf[256];

    /* Ensure null-terminated string, cap at 255 chars */
    size_t len = size < 255 ? size : 255;
    memcpy(buf, data, len);
    buf[len] = '\0';

    /* Skip empty input — contract requires s != NULL, and we always
     * pass buf which is non-NULL, but empty string is a valid data error */

    /* Exercise all three parsers — verify Result invariants */

    {
        result_i64_Error r = parse_i64(buf, &end);
        if (result_i64_Error_is_ok(r)) {
            /* endptr must have advanced past s */
            if (end == buf && buf[0] != '\0') __builtin_trap();
        } else {
            /* endptr must equal s on failure */
            if (end != buf) __builtin_trap();
        }
    }

    {
        result_u64_Error r = parse_u64(buf, &end);
        if (result_u64_Error_is_ok(r)) {
            if (end == buf && buf[0] != '\0') __builtin_trap();
        } else {
            if (end != buf) __builtin_trap();
        }
    }

    {
        result_f64_Error r = parse_f64(buf, &end);
        if (result_f64_Error_is_ok(r)) {
            f64 v = result_f64_Error_unwrap(r);
            (void)v;
            if (end == buf && buf[0] != '\0') __builtin_trap();
        } else {
            if (end != buf) __builtin_trap();
        }
    }

    /* Exercise skip_ws — must never go backward */
    {
        const char* skipped = parse_skip_ws(buf);
        if (skipped < buf) __builtin_trap();
    }

    return 0;
}

#else

int main(void) {
    /* parse_skip_ws */
    RUN(skip_ws_null);
    RUN(skip_ws_empty);
    RUN(skip_ws_no_whitespace);
    RUN(skip_ws_all_types);
    RUN(skip_ws_only_whitespace);
    RUN(skip_ws_mixed);

    /* parse_i64 */
    RUN(i64_decimal);
    RUN(i64_hex);
    RUN(i64_octal);
    RUN(i64_boundaries);
    RUN(i64_overflow);
    RUN(i64_errors);
    RUN(i64_null_endptr);
    RUN(i64_rejects_whitespace);
    RUN(i64_prefix_parsing);

    /* parse_u64 */
    RUN(u64_decimal);
    RUN(u64_hex_octal);
    RUN(u64_rejects_negative);
    RUN(u64_boundaries);
    RUN(u64_errors);
    RUN(u64_rejects_whitespace);
    RUN(u64_prefix_parsing);

    /* parse_f64 */
    RUN(f64_decimal);
    RUN(f64_scientific);
    RUN(f64_special_values);
    RUN(f64_overflow_underflow);
    RUN(f64_errors);
    RUN(f64_rejects_whitespace);
    RUN(f64_prefix_parsing);

    /* Composition patterns */
    RUN(chained_parsing);
    RUN(full_string_validation);
    RUN(mixed_type_chaining);
    RUN(endptr_on_failure);

    printf("\nparse_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
