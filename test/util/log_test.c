/**
 * @file log_test.c
 * @brief Unit tests for util/log/log.h and util/log/log_macros.h
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   log_vfmt_to    — NULL stream, NULL fmt, all three levels, formatted output
 *   log_fmt_to     — formatted wrapper, correct prefix + content
 *   log_to         — plain string, NULL msg
 *   log_msg        — plain string to default stream
 *   log_fmt        — formatted to default stream
 *   macros         — LOG_INFO/WARN/ERROR, _MSG, _FMT, _CHECKED compile + run
 *   error codes    — ERR_INVALID_ARG for NULL, correct error_message()
 *
 * Strategy
 * ───────────────────────────────────────────────────────────────────────────
 *   Output is captured via tmpfile() — write to it, rewind, read back,
 *   verify content. All tmpfile() calls are NULL-guarded.
 */

#define CANON_CONTRACT_IMPL
#include "util/log/log.h"
#include "util/log/log_macros.h"
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
 * Helper: read tmpfile contents into buffer (NULL-safe)
 * ====================================================================== */

static int read_tmpfile(FILE* f, char* buf, int bufsize) {
    int len;
    if (!f) { buf[0] = '\0'; return 0; }
    rewind(f);
    len = (int)fread(buf, 1, (size_t)(bufsize - 1), f);
    buf[len] = '\0';
    return len;
}

static bool contains(const char* buf, const char* substr) {
    return strstr(buf, substr) != NULL;
}

/* =========================================================================
 * log_vfmt_to / log_fmt_to — NULL args return ERR_INVALID_ARG
 * ====================================================================== */

TEST(null_stream) {
    result_bool_Error r = log_fmt_to(NULL, LOG_INFO, "test");
    EXPECT(result_bool_Error_is_err(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

TEST(null_fmt) {
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_fmt_to(f, LOG_INFO, NULL);
    EXPECT(result_bool_Error_is_err(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
    fclose(f);
}

/* =========================================================================
 * log_to — NULL msg returns ERR_INVALID_ARG
 * ====================================================================== */

TEST(null_msg) {
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_to(f, LOG_INFO, NULL);
    EXPECT(result_bool_Error_is_err(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
    fclose(f);
}

/* =========================================================================
 * Prefix correctness — each level writes correct prefix
 * ====================================================================== */

TEST(prefix_info) {
    char buf[256];
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_fmt_to(f, LOG_INFO, "hello");
    EXPECT(result_bool_Error_is_ok(r));
    read_tmpfile(f, buf, sizeof(buf));
    EXPECT(contains(buf, "[INFO] "));
    EXPECT(contains(buf, "hello"));
    fclose(f);
}

TEST(prefix_warn) {
    char buf[256];
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_fmt_to(f, LOG_WARN, "caution");
    EXPECT(result_bool_Error_is_ok(r));
    read_tmpfile(f, buf, sizeof(buf));
    EXPECT(contains(buf, "[WARN] "));
    EXPECT(contains(buf, "caution"));
    fclose(f);
}

TEST(prefix_error) {
    char buf[256];
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_fmt_to(f, LOG_ERROR, "failure");
    EXPECT(result_bool_Error_is_ok(r));
    read_tmpfile(f, buf, sizeof(buf));
    EXPECT(contains(buf, "[ERROR] "));
    EXPECT(contains(buf, "failure"));
    fclose(f);
}

/* =========================================================================
 * Format string interpolation
 * ====================================================================== */

TEST(formatted_output) {
    char buf[256];
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_fmt_to(f, LOG_INFO, "count=%d name=%s", 42, "test");
    EXPECT(result_bool_Error_is_ok(r));
    read_tmpfile(f, buf, sizeof(buf));
    EXPECT(contains(buf, "count=42"));
    EXPECT(contains(buf, "name=test"));
    fclose(f);
}

/* =========================================================================
 * Newline termination — each log line ends with \n
 * ====================================================================== */

TEST(newline_termination) {
    char buf[256];
    int len;
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    (void)log_fmt_to(f, LOG_INFO, "line");
    len = read_tmpfile(f, buf, sizeof(buf));
    EXPECT(len > 0);
    if (len > 0) {
        EXPECT(buf[len - 1] == '\n');
    }
    fclose(f);
}

/* =========================================================================
 * log_to — plain string (no format specifiers)
 * ====================================================================== */

TEST(plain_string) {
    char buf[256];
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_to(f, LOG_WARN, "plain message");
    EXPECT(result_bool_Error_is_ok(r));
    read_tmpfile(f, buf, sizeof(buf));
    EXPECT(contains(buf, "[WARN] "));
    EXPECT(contains(buf, "plain message"));
    fclose(f);
}

/* =========================================================================
 * log_msg — routes to default stream (just verify it returns Ok)
 * ====================================================================== */

TEST(log_msg_ok) {
    result_bool_Error r = log_msg(LOG_INFO, "msg test");
    EXPECT(result_bool_Error_is_ok(r));
}

TEST(log_msg_null) {
    result_bool_Error r = log_msg(LOG_INFO, NULL);
    EXPECT(result_bool_Error_is_err(r));
    EXPECT(result_bool_Error_unwrap_err(r) == ERR_INVALID_ARG);
}

/* =========================================================================
 * log_fmt — routes to default stream (just verify it returns Ok)
 * ====================================================================== */

TEST(log_fmt_ok) {
    result_bool_Error r = log_fmt(LOG_WARN, "value=%d", 7);
    EXPECT(result_bool_Error_is_ok(r));
}

/* =========================================================================
 * Multiple log lines to same stream
 * ====================================================================== */

TEST(multiple_lines) {
    char buf[1024];
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }

    (void)log_fmt_to(f, LOG_INFO,  "first");
    (void)log_fmt_to(f, LOG_WARN,  "second");
    (void)log_fmt_to(f, LOG_ERROR, "third");

    read_tmpfile(f, buf, sizeof(buf));
    EXPECT(contains(buf, "[INFO] first"));
    EXPECT(contains(buf, "[WARN] second"));
    EXPECT(contains(buf, "[ERROR] third"));
    fclose(f);
}

/* =========================================================================
 * Empty message — valid, produces prefix + newline
 * ====================================================================== */

TEST(empty_message) {
    char buf[256];
    FILE* f = tmpfile();
    if (!f) { EXPECT(false); return; }
    result_bool_Error r = log_to(f, LOG_INFO, "");
    EXPECT(result_bool_Error_is_ok(r));
    read_tmpfile(f, buf, sizeof(buf));
    EXPECT(contains(buf, "[INFO] "));
    fclose(f);
}

/* =========================================================================
 * error_message — returns human-readable strings for log error codes
 * ====================================================================== */

TEST(error_messages) {
    EXPECT(strstr(error_message(ERR_INVALID_ARG), "invalid") != NULL ||
           strstr(error_message(ERR_INVALID_ARG), "argument") != NULL);
    EXPECT(strstr(error_message(ERR_IO_FAILED), "I/O") != NULL ||
           strstr(error_message(ERR_IO_FAILED), "failed") != NULL);
}

/* =========================================================================
 * Fire-and-forget macros — compile and execute without crash
 * ====================================================================== */

TEST(macros_compile) {
    LOG_INFO("info macro");
    LOG_WARN("warn macro");
    LOG_ERROR("error macro");

    LOG_INFO_MSG("info msg");
    LOG_WARN_MSG("warn msg");
    LOG_ERROR_MSG("error msg");

    LOG_INFO_FMT("fmt %d", 1);
    LOG_WARN_FMT("fmt %d", 2);
    LOG_ERROR_FMT("fmt %d", 3);

    LOG_INFO_CHECKED("checked %d", 4);
    LOG_WARN_CHECKED("checked %d", 5);
    LOG_ERROR_CHECKED("checked %d", 6);

    EXPECT(true);
}

/* =========================================================================
 * Entry point
 * ====================================================================== */

int main(void) {
    RUN(null_stream);
    RUN(null_fmt);
    RUN(null_msg);

    RUN(prefix_info);
    RUN(prefix_warn);
    RUN(prefix_error);

    RUN(formatted_output);
    RUN(newline_termination);
    RUN(plain_string);

    RUN(log_msg_ok);
    RUN(log_msg_null);
    RUN(log_fmt_ok);

    RUN(multiple_lines);
    RUN(empty_message);
    RUN(error_messages);
    RUN(macros_compile);

    printf("\nlog_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
