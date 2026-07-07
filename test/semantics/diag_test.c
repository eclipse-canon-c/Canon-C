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
 * @file diag_test.c
 * @brief Tests for semantics/diag.h — structured diagnostic chain
 *
 * Covers:
 *   - diag_init: zeroed state, depth == 0
 *   - diag_clear: resets depth to 0, NULL-safe
 *   - diag_push: single frame, field storage, return value (no overflow),
 *                NULL msg, long msg truncation, NULL diag (no-op)
 *   - Overflow behaviour: oldest frame discarded, chain shifted, depth clamped
 *   - DIAG_PUSH: macro captures __FILE__ / __LINE__ / __func__ correctly
 *   - DIAG_PUSH_FMT: formatted message stored correctly (GNU extension path,
 *                     guarded by #ifndef CANON_NO_GNU_EXTENSIONS)
 *   - diag_depth: correct count, NULL-safe
 *   - diag_has_error: true when depth > 0, false when empty, NULL-safe
 *   - diag_frame_at: in-bounds, out-of-bounds, NULL diag
 *   - diag_root: first frame, empty chain, NULL diag
 *   - diag_latest: last frame, single frame, empty chain, NULL diag
 *   - diag_root_code / diag_latest_code: correct codes, empty/NULL chains
 *   - diag_print: NULL-safe (diag NULL, stream NULL, empty chain),
 *                 non-empty chain output, "?" / "(no message)" placeholders
 *   - diag_render_frame: content and format, snprintf return semantics,
 *                        truncation, placeholders, NULL/0-size guards
 *   - diag_render: full-chain content and ordering, would-be total under
 *                  truncation, placeholders, NULL/0 guards, empty chain,
 *                  cross-check against per-frame renders
 *   - DIAG_RETURN_IF: pushes and returns on true condition, skips on false
 *   - DIAG_PROPAGATE: pushes and returns when call returns false, skips on true
 *   - Reuse after diag_clear: new pushes land at index 0
 *   - Frame ordering: root = first pushed, latest = last pushed
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Feeds random (depth_count, msg_len, msg_content) into diag_push to
 *     exercise the overflow/shift path and the message copy length clamp.
 *     Verifies depth never exceeds DIAG_MAX_FRAMES and message is always
 *     null-terminated within DIAG_MAX_MSG_LEN bytes.
 */

#define CANON_CONTRACT_IMPL

/* diag.h defines DIAG_PUSH_FMT using ##__VA_ARGS__ (GNU token-pasting
 * extension). Clang with -Werror fires -Wgnu-zero-variadic-macro-arguments
 * when parsing that macro definition even if it is never called. Suppress
 * the warning around the include; the macro itself is tested under
 * #ifndef CANON_NO_GNU_EXTENSIONS where the extension is expected. */
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#include "semantics/diag.h"

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include <stdio.h>
#include <string.h>

/* ── Harness ──────────────────────────────────────────────────────────────── */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                                \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL %s:%d  %s\n",                    \
                    __FILE__, __LINE__, #cond);                     \
            g_failed++;                                             \
        }                                                           \
    } while (0)

#define EXPECT_NOT_NULL(ptr)                                        \
    do {                                                            \
        if ((ptr) == NULL) {                                        \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",            \
                    __FILE__, __LINE__, #ptr);                      \
            g_failed++;                                             \
            return;                                                 \
        }                                                           \
    } while (0)

/* ============================================================================
   diag_init
   ============================================================================ */

static void test_init_depth_is_zero(void)
{
    Diag d = diag_init();
    EXPECT(d.depth == 0);
}

static void test_init_has_no_error(void)
{
    Diag d = diag_init();
    EXPECT(!diag_has_error(&d));
}

static void test_init_depth_query_zero(void)
{
    Diag d = diag_init();
    EXPECT(diag_depth(&d) == 0);
}

/* ============================================================================
   diag_clear
   ============================================================================ */

static void test_clear_resets_depth(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    EXPECT(d.depth == 1);
    diag_clear(&d);
    EXPECT(d.depth == 0);
}

static void test_clear_null_safe(void)
{
    diag_clear(NULL);
    EXPECT(1); /* must not crash */
}

static void test_clear_allows_reuse(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN,   "first");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_IO_FAILED, "second");
    diag_clear(&d);
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_TIMEOUT, "after clear");
    EXPECT(d.depth == 1);
    EXPECT(d.frames[0].code == ERR_TIMEOUT);
    EXPECT(strcmp(d.frames[0].message, "after clear") == 0);
}

/* ============================================================================
   diag_push — basic frame storage
   ============================================================================ */

static void test_push_increases_depth(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_ARG, "msg");
    EXPECT(d.depth == 1);
}

static void test_push_stores_code(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_OUT_OF_RANGE, "range");
    EXPECT(d.frames[0].code == ERR_OUT_OF_RANGE);
}

static void test_push_stores_message(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_PARSE_FAILED, "bad int");
    EXPECT(strcmp(d.frames[0].message, "bad int") == 0);
}

static void test_push_stores_file_and_func(void)
{
    Diag       d    = diag_init();
    const char *file = __FILE__;
    const char *func = __func__;
    diag_push(&d, file, (usize)__LINE__, func, ERR_UNKNOWN, NULL);
    /* Compare content rather than pointer identity to avoid MSVC C4130 */
    EXPECT(strcmp(d.frames[0].file, file) == 0);
    EXPECT(strcmp(d.frames[0].func, func) == 0);
}

static void test_push_stores_line(void)
{
    Diag  d    = diag_init();
    usize line = (usize)__LINE__ + 1u;
    diag_push(&d, __FILE__, line, __func__, ERR_UNKNOWN, NULL);
    EXPECT(d.frames[0].line == line);
}

static void test_push_null_msg_yields_empty_message(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, NULL);
    EXPECT(d.frames[0].message[0] == '\0');
}

static void test_push_empty_msg_stored(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "");
    EXPECT(d.frames[0].message[0] == '\0');
}

static void test_push_returns_false_when_no_overflow(void)
{
    Diag d    = diag_init();
    bool drop = diag_push(&d, __FILE__, (usize)__LINE__, __func__,
                           ERR_UNKNOWN, "ok");
    EXPECT(!drop);
}

static void test_push_null_diag_returns_false(void)
{
    bool drop = diag_push(NULL, __FILE__, (usize)__LINE__, __func__,
                          ERR_UNKNOWN, "ignored");
    EXPECT(!drop);
}

static void test_push_null_diag_does_not_crash(void)
{
    diag_push(NULL, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    EXPECT(1);
}

/* ── Message truncation ──────────────────────────────────────────────────── */

static void test_push_long_msg_truncated_to_max(void)
{
    /* Build a message longer than DIAG_MAX_MSG_LEN */
    char   long_msg[DIAG_MAX_MSG_LEN + 64];
    usize  i;
    Diag   d = diag_init();

    for (i = 0; i < sizeof(long_msg) - 1u; i++) {
        long_msg[i] = 'A';
    }
    long_msg[sizeof(long_msg) - 1u] = '\0';

    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, long_msg);

    /* Message must be null-terminated within the inline array */
    EXPECT(d.frames[0].message[DIAG_MAX_MSG_LEN - 1u] == '\0');
    /* Length stored must be exactly DIAG_MAX_MSG_LEN - 1 bytes of content */
    EXPECT(strlen(d.frames[0].message) == DIAG_MAX_MSG_LEN - 1u);
}

static void test_push_msg_exactly_max_minus_one(void)
{
    /* A message of exactly DIAG_MAX_MSG_LEN-1 chars must fit without truncation */
    char   msg[DIAG_MAX_MSG_LEN];
    usize  i;
    Diag   d = diag_init();

    for (i = 0; i < DIAG_MAX_MSG_LEN - 1u; i++) {
        msg[i] = 'B';
    }
    msg[DIAG_MAX_MSG_LEN - 1u] = '\0';

    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, msg);
    EXPECT(strlen(d.frames[0].message) == DIAG_MAX_MSG_LEN - 1u);
    EXPECT(d.frames[0].message[DIAG_MAX_MSG_LEN - 1u] == '\0');
}

/* ============================================================================
   Overflow — oldest frame discarded, chain shifted
   ============================================================================ */

static void test_overflow_returns_true(void)
{
    Diag d = diag_init();
    usize i;
    bool  drop = false;

    /* Fill to capacity */
    for (i = 0; i < DIAG_MAX_FRAMES; i++) {
        drop = diag_push(&d, __FILE__, (usize)__LINE__, __func__,
                         ERR_UNKNOWN, "fill");
        EXPECT(!drop);
    }
    /* One more — must report overflow */
    drop = diag_push(&d, __FILE__, (usize)__LINE__, __func__,
                     ERR_UNKNOWN, "overflow");
    EXPECT(drop);
}

static void test_overflow_depth_stays_at_max(void)
{
    Diag  d = diag_init();
    usize i;

    for (i = 0; i < DIAG_MAX_FRAMES + 3u; i++) {
        diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    }
    EXPECT(d.depth == DIAG_MAX_FRAMES);
}

static void test_overflow_oldest_frame_discarded(void)
{
    Diag d = diag_init();
    usize i;

    /* Push DIAG_MAX_FRAMES frames with distinct codes, then one more */
    for (i = 0; i < DIAG_MAX_FRAMES; i++) {
        diag_push(&d, __FILE__, (usize)__LINE__, __func__,
                  ERR_INVALID_ARG, "root");
    }
    /* This push should drop the oldest (ERR_INVALID_ARG) frame */
    diag_push(&d, __FILE__, (usize)__LINE__, __func__,
              ERR_TIMEOUT, "surface");

    /* Root is now the second frame originally pushed */
    EXPECT(diag_root(&d) != NULL);
    EXPECT(diag_root(&d)->code == ERR_INVALID_ARG);
    /* Latest is the one just pushed */
    EXPECT(diag_latest(&d)->code == ERR_TIMEOUT);
}

static void test_overflow_chain_shift_preserves_order(void)
{
    /* Push frames with distinct sequential codes, overflow by 2, verify order */
    Diag  d = diag_init();
    usize i;

    /* Use ERR_INVALID_ARG(1) through ERR_INVALID_STATE(5) for first 5 frames,
       then overflow with ERR_OUT_OF_MEMORY(100) and ERR_BUFFER_TOO_SMALL(101) */
    Error codes[] = {
        ERR_INVALID_ARG,
        ERR_OUT_OF_RANGE,
        ERR_PARSE_FAILED,
        ERR_INVALID_FORMAT,
        ERR_INVALID_STATE,
        ERR_OUT_OF_MEMORY,
        ERR_BUFFER_TOO_SMALL,
        ERR_CAPACITY_EXCEEDED,
        ERR_NOT_FOUND,
        ERR_IO_FAILED
    };
    /* Enough to fill and overflow regardless of DIAG_MAX_FRAMES (default 8) */
    usize n = sizeof(codes) / sizeof(codes[0]);

    for (i = 0; i < n; i++) {
        diag_push(&d, __FILE__, (usize)__LINE__, __func__, codes[i], "x");
    }

    EXPECT(d.depth == DIAG_MAX_FRAMES);

    /* Verify the surviving chain is the last DIAG_MAX_FRAMES codes in order */
    for (i = 0; i < DIAG_MAX_FRAMES; i++) {
        usize src = (n - DIAG_MAX_FRAMES) + i;
        EXPECT(d.frames[i].code == codes[src]);
    }
}

/* ============================================================================
   DIAG_PUSH macro — location capture
   ============================================================================ */

static void test_diag_push_macro_captures_location(void)
{
    Diag             d    = diag_init();
    const char      *file = __FILE__;
    usize            line;
    const DiagFrame *f;

    line = (usize)(__LINE__ + 1); /* must match the macro call below */
    DIAG_PUSH(&d, ERR_PARSE_FAILED, "macro test");

    f = diag_root(&d);
    EXPECT_NOT_NULL(f);
    EXPECT(f->code == ERR_PARSE_FAILED);
    EXPECT(strcmp(f->message, "macro test") == 0);
    /* __FILE__ is a string literal — compare content, not pointer identity,
     * to avoid MSVC C4130 (logical operation on address of string constant) */
    EXPECT(strcmp(f->file, file) == 0);
    EXPECT(f->line == line);
}

static void test_diag_push_macro_null_diag_no_crash(void)
{
    DIAG_PUSH(NULL, ERR_UNKNOWN, "ignored");
    EXPECT(1);
}

/* ============================================================================
   DIAG_PUSH_FMT macro — formatted message (GNU extension path)
   ============================================================================ */

#ifndef CANON_NO_GNU_EXTENSIONS

/* MSVC C4130: "logical operation on address of string constant"
 * fires from require_msg() calls inside the DIAG_PUSH_FMT macro body
 * because MSVC evaluates the stringified condition as a string literal
 * address in a boolean context. Suppress around these three call sites. */
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4130)
#endif

static void test_diag_push_fmt_formats_message(void)
{
    Diag             d = diag_init();
    char             tmp[64];
    const DiagFrame *f;

    DIAG_PUSH_FMT(&d, ERR_OUT_OF_RANGE, tmp, sizeof(tmp),
                  "index %d out of range [0, %d)", 7, 4);

    f = diag_root(&d);
    EXPECT_NOT_NULL(f);
    EXPECT(f->code == ERR_OUT_OF_RANGE);
    EXPECT(strcmp(f->message, "index 7 out of range [0, 4)") == 0);
}

static void test_diag_push_fmt_truncates_long_format(void)
{
    Diag d = diag_init();
    char tmp[DIAG_MAX_MSG_LEN + 32];

    /* Format a string that will exceed DIAG_MAX_MSG_LEN when rendered */
    DIAG_PUSH_FMT(&d, ERR_UNKNOWN, tmp, sizeof(tmp),
                  "%0*d", (int)(DIAG_MAX_MSG_LEN + 10u), 0);

    /* diag_push must have clamped the copy to DIAG_MAX_MSG_LEN-1 bytes */
    EXPECT(d.frames[0].message[DIAG_MAX_MSG_LEN - 1u] == '\0');
    EXPECT(strlen(d.frames[0].message) == DIAG_MAX_MSG_LEN - 1u);
}

static void test_diag_push_fmt_null_diag_no_crash(void)
{
    char tmp[32];
    DIAG_PUSH_FMT(NULL, ERR_UNKNOWN, tmp, sizeof(tmp), "no crash %d", 0);
    EXPECT(1);
}

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ============================================================================
   diag_depth
   ============================================================================ */

static void test_depth_increments_per_push(void)
{
    Diag  d = diag_init();
    usize i;
    for (i = 0; i < DIAG_MAX_FRAMES; i++) {
        EXPECT(diag_depth(&d) == i);
        diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, NULL);
    }
    EXPECT(diag_depth(&d) == DIAG_MAX_FRAMES);
}

static void test_depth_null_safe(void)
{
    EXPECT(diag_depth(NULL) == 0);
}

/* ============================================================================
   diag_has_error
   ============================================================================ */

static void test_has_error_false_when_empty(void)
{
    Diag d = diag_init();
    EXPECT(!diag_has_error(&d));
}

static void test_has_error_true_after_push(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, NULL);
    EXPECT(diag_has_error(&d));
}

static void test_has_error_false_after_clear(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, NULL);
    diag_clear(&d);
    EXPECT(!diag_has_error(&d));
}

static void test_has_error_null_safe(void)
{
    EXPECT(!diag_has_error(NULL));
}

/* ============================================================================
   diag_frame_at
   ============================================================================ */

static void test_frame_at_index_zero(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_ARG, "first");
    const DiagFrame *f = diag_frame_at(&d, 0);
    EXPECT_NOT_NULL(f);
    EXPECT(f->code == ERR_INVALID_ARG);
}

static void test_frame_at_last_index(void)
{
    Diag  d = diag_init();
    usize i;
    for (i = 0; i < DIAG_MAX_FRAMES; i++) {
        diag_push(&d, __FILE__, (usize)__LINE__, __func__,
                  (i == DIAG_MAX_FRAMES - 1u) ? ERR_TIMEOUT : ERR_UNKNOWN,
                  NULL);
    }
    const DiagFrame *f = diag_frame_at(&d, DIAG_MAX_FRAMES - 1u);
    EXPECT_NOT_NULL(f);
    EXPECT(f->code == ERR_TIMEOUT);
}

static void test_frame_at_out_of_bounds_returns_null(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, NULL);
    /* Use depth+1 and DIAG_MAX_FRAMES+1 rather than a large literal (e.g. 99)
     * to avoid Cppcheck arrayIndexOutOfBounds: it traces the concrete value
     * into diag_frame_at and flags the array access even though the guard
     * i >= d->depth would prevent it. Both values are still clearly OOB. */
    EXPECT(diag_frame_at(&d, 1)                   == NULL);
    EXPECT(diag_frame_at(&d, DIAG_MAX_FRAMES + 1u) == NULL);
}

static void test_frame_at_empty_chain_returns_null(void)
{
    Diag d = diag_init();
    EXPECT(diag_frame_at(&d, 0) == NULL);
}

static void test_frame_at_null_diag_returns_null(void)
{
    EXPECT(diag_frame_at(NULL, 0) == NULL);
}

/* ============================================================================
   diag_root
   ============================================================================ */

static void test_root_returns_first_frame(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_ARG,  "root");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_OUT_OF_RANGE, "surface");
    const DiagFrame *f = diag_root(&d);
    EXPECT_NOT_NULL(f);
    EXPECT(f->code == ERR_INVALID_ARG);
    EXPECT(strcmp(f->message, "root") == 0);
}

static void test_root_single_frame(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_IO_FAILED, "only");
    EXPECT_NOT_NULL(diag_root(&d));
    EXPECT(diag_root(&d)->code == ERR_IO_FAILED);
}

static void test_root_empty_chain_returns_null(void)
{
    Diag d = diag_init();
    EXPECT(diag_root(&d) == NULL);
}

static void test_root_null_diag_returns_null(void)
{
    EXPECT(diag_root(NULL) == NULL);
}

/* ============================================================================
   diag_latest
   ============================================================================ */

static void test_latest_returns_last_frame(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_ARG, "first");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_TIMEOUT,     "last");
    const DiagFrame *f = diag_latest(&d);
    EXPECT_NOT_NULL(f);
    EXPECT(f->code == ERR_TIMEOUT);
    EXPECT(strcmp(f->message, "last") == 0);
}

static void test_latest_single_frame_same_as_root(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_NOT_FOUND, "only");
    EXPECT(diag_root(&d) == diag_latest(&d));
}

static void test_latest_empty_chain_returns_null(void)
{
    Diag d = diag_init();
    EXPECT(diag_latest(&d) == NULL);
}

static void test_latest_null_diag_returns_null(void)
{
    EXPECT(diag_latest(NULL) == NULL);
}

static void test_latest_updates_after_overflow(void)
{
    Diag  d = diag_init();
    usize i;
    for (i = 0; i < DIAG_MAX_FRAMES + 1u; i++) {
        diag_push(&d, __FILE__, (usize)__LINE__, __func__,
                  (i == DIAG_MAX_FRAMES) ? ERR_PERMISSION : ERR_UNKNOWN,
                  NULL);
    }
    EXPECT(diag_latest(&d)->code == ERR_PERMISSION);
}

/* ============================================================================
   diag_root_code / diag_latest_code
   ============================================================================ */

static void test_root_code_correct(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_OVERFLOW,  "root");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNDERFLOW, "surface");
    EXPECT(diag_root_code(&d) == ERR_OVERFLOW);
}

static void test_latest_code_correct(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_OVERFLOW,  "root");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNDERFLOW, "surface");
    EXPECT(diag_latest_code(&d) == ERR_UNDERFLOW);
}

static void test_root_code_empty_returns_err_ok(void)
{
    Diag d = diag_init();
    EXPECT(diag_root_code(&d) == ERR_OK);
}

static void test_latest_code_empty_returns_err_ok(void)
{
    Diag d = diag_init();
    EXPECT(diag_latest_code(&d) == ERR_OK);
}

static void test_root_code_null_returns_err_ok(void)
{
    EXPECT(diag_root_code(NULL) == ERR_OK);
}

static void test_latest_code_null_returns_err_ok(void)
{
    EXPECT(diag_latest_code(NULL) == ERR_OK);
}

/* ============================================================================
   diag_print — NULL-safety and rendered-output correctness
   ============================================================================ */

static void test_print_null_diag_no_crash(void)
{
    diag_print(NULL, stderr);
    EXPECT(1);
}

static void test_print_null_stream_no_crash(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    diag_print(&d, NULL);
    EXPECT(1);
}

static void test_print_empty_chain_no_crash(void)
{
    Diag d = diag_init();
    diag_print(&d, stderr);
    EXPECT(1);
}

static void test_print_nonempty_chain_writes_output(void)
{
    Diag  d = diag_init();
    FILE *f = tmpfile();
    long  pos;

    if (f == NULL) {
        /* Restricted environment (e.g. a Windows runner denying temp-file
         * creation) — skip. The coverage job runs on Linux, where tmpfile()
         * succeeds, so the MC/DC outcomes are still measured. */
        EXPECT(1);
        return;
    }

    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_PARSE_FAILED, "root");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_IO_FAILED,    "surface");

    diag_print(&d, f);

    pos = ftell(f);
    EXPECT(pos > 0);
    fclose(f);
}

static void test_print_placeholders_for_null_fields(void)
{
    /* Frames with NULL file/func/msg must print "?" / "(no message)" —
     * exercises the false side of all three ternaries in diag_print. */
    Diag  d = diag_init();
    FILE *f = tmpfile();
    char  buf[512];
    usize nread;

    if (f == NULL) { EXPECT(1); return; }

    diag_push(&d, NULL, 0u, NULL, ERR_UNKNOWN, NULL);
    diag_print(&d, f);

    rewind(f);
    nread = fread(buf, 1u, sizeof(buf) - 1u, f);
    buf[nread] = '\0';
    fclose(f);

    EXPECT(nread > 0u);
    EXPECT(strstr(buf, "?")            != NULL);
    EXPECT(strstr(buf, "(no message)") != NULL);
}

/* ============================================================================
   diag_render_frame — content, snprintf semantics, truncation, guards
   ============================================================================ */

static void test_render_frame_basic_content_and_length(void)
{
    Diag             d = diag_init();
    char             buf[256];
    usize            n;
    const DiagFrame *f;

    diag_push(&d, __FILE__, (usize)__LINE__, __func__,
              ERR_OUT_OF_RANGE, "hello render");
    f = diag_root(&d);
    EXPECT_NOT_NULL(f);

    n = diag_render_frame(f, 0u, buf, sizeof(buf));

    EXPECT(n > 0u);
    EXPECT(n == strlen(buf));                  /* untruncated: n == strlen */
    EXPECT(strstr(buf, "hello render") != NULL);
    EXPECT(strstr(buf, "[0]") == buf);         /* index prefix leads */
    EXPECT(strchr(buf, '\n') == NULL);         /* no trailing newline */
}

static void test_render_frame_truncation_semantics(void)
{
    /* snprintf semantics: returns the would-be length regardless of
     * buf_size; truncated output is null-terminated at buf_size - 1. */
    Diag             d = diag_init();
    char             big[256];
    char             small[16];
    usize            n_big;
    usize            n_small;
    const DiagFrame *f;

    diag_push(&d, __FILE__, (usize)__LINE__, __func__,
              ERR_UNKNOWN, "a message long enough to truncate");
    f = diag_root(&d);
    EXPECT_NOT_NULL(f);

    n_big   = diag_render_frame(f, 0u, big,   sizeof(big));
    n_small = diag_render_frame(f, 0u, small, sizeof(small));

    EXPECT(n_small == n_big);                  /* would-be count, not written */
    EXPECT(n_small >= sizeof(small));          /* truncation detectable */
    EXPECT(strlen(small) == sizeof(small) - 1u);
}

static void test_render_frame_placeholders_for_null_fields(void)
{
    Diag             d = diag_init();
    char             buf[256];
    usize            n;
    const DiagFrame *f;

    diag_push(&d, NULL, 0u, NULL, ERR_UNKNOWN, NULL);
    f = diag_root(&d);
    EXPECT_NOT_NULL(f);

    n = diag_render_frame(f, 3u, buf, sizeof(buf));

    EXPECT(n > 0u);
    EXPECT(strstr(buf, "?")            != NULL);
    EXPECT(strstr(buf, "(no message)") != NULL);
    EXPECT(strstr(buf, "[3]") == buf);         /* caller-supplied index used */
}

static void test_render_frame_null_frame_returns_zero(void)
{
    char buf[64];
    buf[0] = 'X';
    EXPECT(diag_render_frame(NULL, 0u, buf, sizeof(buf)) == 0u);
    EXPECT(buf[0] == '\0');                    /* cleared by initial guard */
}

static void test_render_frame_null_buf_returns_zero(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    EXPECT(diag_render_frame(diag_root(&d), 0u, NULL, 64u) == 0u);
}

static void test_render_frame_zero_size_returns_zero_buf_untouched(void)
{
    Diag d = diag_init();
    char buf[8];
    buf[0] = 'X';
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    EXPECT(diag_render_frame(diag_root(&d), 0u, buf, 0u) == 0u);
    EXPECT(buf[0] == 'X');                     /* size 0: nothing written */
}

/* ============================================================================
   diag_render — full chain, total accounting, truncation, guards
   ============================================================================ */

static void test_render_full_chain_content_and_total(void)
{
    Diag        d = diag_init();
    char        buf[1024];
    char        line[256];
    usize       total;
    usize       sum      = 0u;
    usize       newlines = 0u;
    usize       i;
    const char *pa;
    const char *pb;
    const char *pc;

    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_PARSE_FAILED,   "aaa");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_FORMAT, "bbb");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_STATE,  "ccc");

    total = diag_render(&d, buf, sizeof(buf));

    EXPECT(total > 0u);
    EXPECT(total == strlen(buf));              /* untruncated: total == strlen */

    for (i = 0u; buf[i] != '\0'; i++) {
        if (buf[i] == '\n') { newlines++; }
    }
    EXPECT(newlines == 3u);                    /* one newline per frame */

    pa = strstr(buf, "\"aaa\"");
    pb = strstr(buf, "\"bbb\"");
    pc = strstr(buf, "\"ccc\"");
    EXPECT(pa != NULL);
    EXPECT(pb != NULL);
    EXPECT(pc != NULL);
    EXPECT(pa != NULL && pb != NULL && pa < pb);   /* root-to-surface order */
    EXPECT(pb != NULL && pc != NULL && pb < pc);

    /* Cross-check against diag_render_frame: the chain total must equal
     * the sum of per-frame renders plus one newline per frame. */
    for (i = 0u; i < diag_depth(&d); i++) {
        sum += diag_render_frame(diag_frame_at(&d, i), i, line, sizeof(line));
        sum += 1u; /* '\n' appended per frame by diag_render */
    }
    EXPECT(sum == total);
}

static void test_render_truncated_returns_full_total(void)
{
    /* Exercises the buffer-full measuring path: once total >= buf_size,
     * remaining frames go through the (dst = buf, rem = 0) branch. The
     * return value must equal the untruncated would-be total. */
    Diag  d = diag_init();
    char  big[1024];
    char  tiny[8];
    usize total_big;
    usize total_tiny;

    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "frame one");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "frame two");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "frame three");

    total_big  = diag_render(&d, big,  sizeof(big));
    total_tiny = diag_render(&d, tiny, sizeof(tiny));

    EXPECT(total_tiny == total_big);           /* total independent of buf_size */
    EXPECT(total_tiny >= sizeof(tiny));        /* truncation detectable */
    EXPECT(strlen(tiny) == sizeof(tiny) - 1u); /* null-terminated at limit */
}

static void test_render_placeholders_for_null_fields(void)
{
    Diag  d = diag_init();
    char  buf[512];
    usize total;

    diag_push(&d, NULL, 0u, NULL, ERR_UNKNOWN, NULL);
    total = diag_render(&d, buf, sizeof(buf));

    EXPECT(total > 0u);
    EXPECT(strstr(buf, "?")            != NULL);
    EXPECT(strstr(buf, "(no message)") != NULL);
}

static void test_render_null_diag_returns_zero(void)
{
    char buf[64];
    buf[0] = 'X';
    EXPECT(diag_render(NULL, buf, sizeof(buf)) == 0u);
    EXPECT(buf[0] == '\0');                    /* cleared by initial guard */
}

static void test_render_empty_chain_returns_zero_empty_buf(void)
{
    Diag d = diag_init();
    char buf[64];
    buf[0] = 'X';
    EXPECT(diag_render(&d, buf, sizeof(buf)) == 0u);
    EXPECT(buf[0] == '\0');
}

static void test_render_null_buf_returns_zero(void)
{
    /* Documents the "buf NULL-safe: returns 0" contract. Requires the
     * diag_render guard that rejects buf == NULL — against the pre-fix
     * header this call would reach snprintf(NULL, n > 0), the UB that
     * guard exists to prevent. */
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    EXPECT(diag_render(&d, NULL, 64u) == 0u);
}

static void test_render_zero_size_returns_zero_buf_untouched(void)
{
    Diag d = diag_init();
    char buf[8];
    buf[0] = 'X';
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_UNKNOWN, "x");
    EXPECT(diag_render(&d, buf, 0u) == 0u);
    EXPECT(buf[0] == 'X');                     /* size 0: nothing written */
}

/* ============================================================================
   Frame ordering — root = first pushed, latest = last pushed
   ============================================================================ */

static void test_frame_ordering_three_pushes(void)
{
    Diag d = diag_init();
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_PARSE_FAILED,    "a");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_FORMAT,  "b");
    diag_push(&d, __FILE__, (usize)__LINE__, __func__, ERR_INVALID_STATE,   "c");

    EXPECT(diag_depth(&d) == 3);
    EXPECT(d.frames[0].code == ERR_PARSE_FAILED);
    EXPECT(d.frames[1].code == ERR_INVALID_FORMAT);
    EXPECT(d.frames[2].code == ERR_INVALID_STATE);
    EXPECT(strcmp(d.frames[0].message, "a") == 0);
    EXPECT(strcmp(d.frames[1].message, "b") == 0);
    EXPECT(strcmp(d.frames[2].message, "c") == 0);
}

/* ============================================================================
   DIAG_RETURN_IF
   ============================================================================ */

static bool helper_return_if_true(Diag *d, bool *reached_end)
{
    *reached_end = false;
    DIAG_RETURN_IF(true, d, ERR_INVALID_ARG, "triggered", false);
    *reached_end = true;
    return true;
}

static bool helper_return_if_false(Diag *d, bool *reached_end)
{
    *reached_end = false;
    DIAG_RETURN_IF(false, d, ERR_INVALID_ARG, "should not push", false);
    *reached_end = true;
    return true;
}

static void test_diag_return_if_true_pushes_and_returns(void)
{
    Diag d           = diag_init();
    bool reached_end = true;
    bool result      = helper_return_if_true(&d, &reached_end);

    EXPECT(!result);
    EXPECT(!reached_end);
    EXPECT(diag_depth(&d) == 1);
    EXPECT(diag_root_code(&d) == ERR_INVALID_ARG);
    EXPECT(strcmp(d.frames[0].message, "triggered") == 0);
}

static void test_diag_return_if_false_skips(void)
{
    Diag d           = diag_init();
    bool reached_end = false;
    bool result      = helper_return_if_false(&d, &reached_end);

    EXPECT(result);
    EXPECT(reached_end);
    EXPECT(diag_depth(&d) == 0);
}

static void test_diag_return_if_null_diag_no_crash(void)
{
    /* Condition true, NULL diag — must not crash, must still return */
    Diag d           = diag_init();
    bool reached_end = true;
    bool result      = helper_return_if_true(NULL, &reached_end);
    (void)d;
    EXPECT(!result);
    EXPECT(!reached_end);
}

/* ============================================================================
   DIAG_PROPAGATE
   ============================================================================ */

static bool call_returns_false(void) { return false; }
static bool call_returns_true(void)  { return true;  }

static bool helper_propagate_false(Diag *d, bool *reached_end)
{
    *reached_end = false;
    DIAG_PROPAGATE(call_returns_false(), d, ERR_IO_FAILED, "propagated", false);
    *reached_end = true;
    return true;
}

static bool helper_propagate_true(Diag *d, bool *reached_end)
{
    *reached_end = false;
    DIAG_PROPAGATE(call_returns_true(), d, ERR_IO_FAILED, "should not push", false);
    *reached_end = true;
    return true;
}

static void test_diag_propagate_false_pushes_and_returns(void)
{
    Diag d           = diag_init();
    bool reached_end = true;
    bool result      = helper_propagate_false(&d, &reached_end);

    EXPECT(!result);
    EXPECT(!reached_end);
    EXPECT(diag_depth(&d) == 1);
    EXPECT(diag_root_code(&d) == ERR_IO_FAILED);
    EXPECT(strcmp(d.frames[0].message, "propagated") == 0);
}

static void test_diag_propagate_true_skips(void)
{
    Diag d           = diag_init();
    bool reached_end = false;
    bool result      = helper_propagate_true(&d, &reached_end);

    EXPECT(result);
    EXPECT(reached_end);
    EXPECT(diag_depth(&d) == 0);
}

static void test_diag_propagate_null_diag_no_crash(void)
{
    bool reached_end = true;
    bool result      = helper_propagate_false(NULL, &reached_end);
    EXPECT(!result);
    EXPECT(!reached_end);
}

/* ============================================================================
   Unit test entry point
   ============================================================================ */

int main(void)
{
    /* diag_init */
    test_init_depth_is_zero();
    test_init_has_no_error();
    test_init_depth_query_zero();

    /* diag_clear */
    test_clear_resets_depth();
    test_clear_null_safe();
    test_clear_allows_reuse();

    /* diag_push — basic */
    test_push_increases_depth();
    test_push_stores_code();
    test_push_stores_message();
    test_push_stores_file_and_func();
    test_push_stores_line();
    test_push_null_msg_yields_empty_message();
    test_push_empty_msg_stored();
    test_push_returns_false_when_no_overflow();
    test_push_null_diag_returns_false();
    test_push_null_diag_does_not_crash();

    /* diag_push — message truncation */
    test_push_long_msg_truncated_to_max();
    test_push_msg_exactly_max_minus_one();

    /* overflow */
    test_overflow_returns_true();
    test_overflow_depth_stays_at_max();
    test_overflow_oldest_frame_discarded();
    test_overflow_chain_shift_preserves_order();

    /* DIAG_PUSH macro */
    test_diag_push_macro_captures_location();
    test_diag_push_macro_null_diag_no_crash();

    /* DIAG_PUSH_FMT macro */
#ifndef CANON_NO_GNU_EXTENSIONS
    test_diag_push_fmt_formats_message();
    test_diag_push_fmt_truncates_long_format();
    test_diag_push_fmt_null_diag_no_crash();
#endif

    /* diag_depth */
    test_depth_increments_per_push();
    test_depth_null_safe();

    /* diag_has_error */
    test_has_error_false_when_empty();
    test_has_error_true_after_push();
    test_has_error_false_after_clear();
    test_has_error_null_safe();

    /* diag_frame_at */
    test_frame_at_index_zero();
    test_frame_at_last_index();
    test_frame_at_out_of_bounds_returns_null();
    test_frame_at_empty_chain_returns_null();
    test_frame_at_null_diag_returns_null();

    /* diag_root */
    test_root_returns_first_frame();
    test_root_single_frame();
    test_root_empty_chain_returns_null();
    test_root_null_diag_returns_null();

    /* diag_latest */
    test_latest_returns_last_frame();
    test_latest_single_frame_same_as_root();
    test_latest_empty_chain_returns_null();
    test_latest_null_diag_returns_null();
    test_latest_updates_after_overflow();

    /* diag_root_code / diag_latest_code */
    test_root_code_correct();
    test_latest_code_correct();
    test_root_code_empty_returns_err_ok();
    test_latest_code_empty_returns_err_ok();
    test_root_code_null_returns_err_ok();
    test_latest_code_null_returns_err_ok();

    /* diag_print */
    test_print_null_diag_no_crash();
    test_print_null_stream_no_crash();
    test_print_empty_chain_no_crash();
    test_print_nonempty_chain_writes_output();
    test_print_placeholders_for_null_fields();

    /* diag_render_frame */
    test_render_frame_basic_content_and_length();
    test_render_frame_truncation_semantics();
    test_render_frame_placeholders_for_null_fields();
    test_render_frame_null_frame_returns_zero();
    test_render_frame_null_buf_returns_zero();
    test_render_frame_zero_size_returns_zero_buf_untouched();

    /* diag_render */
    test_render_full_chain_content_and_total();
    test_render_truncated_returns_full_total();
    test_render_placeholders_for_null_fields();
    test_render_null_diag_returns_zero();
    test_render_empty_chain_returns_zero_empty_buf();
    test_render_null_buf_returns_zero();
    test_render_zero_size_returns_zero_buf_untouched();

    /* Frame ordering */
    test_frame_ordering_three_pushes();

    /* DIAG_RETURN_IF */
    test_diag_return_if_true_pushes_and_returns();
    test_diag_return_if_false_skips();
    test_diag_return_if_null_diag_no_crash();

    /* DIAG_PROPAGATE */
    test_diag_propagate_false_pushes_and_returns();
    test_diag_propagate_true_skips();
    test_diag_propagate_null_diag_no_crash();

    if (g_failed == 0) {
        printf("OK  diag_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  diag_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ── Fuzz entry point ─────────────────────────────────────────────────────
 *
 * Targets the two non-trivial paths in diag_push:
 *   1. Overflow/shift: depth == DIAG_MAX_FRAMES → memmove + reuse
 *   2. Message copy length clamp: manual memcpy loop with bounded len
 *
 * Input layout (variable length, up to 256 bytes):
 *   byte[0]     — number of initial pushes before the probing push (0..255)
 *                 modulo (DIAG_MAX_FRAMES + 2) to exercise near/at/over
 *   byte[1]     — error code index (modulo ERR_COUNT used as raw int cast)
 *   byte[2..N]  — message bytes (raw, may contain embedded nulls or be
 *                 longer than DIAG_MAX_MSG_LEN; used as-is via diag_push
 *                 which treats it as a C string from byte[2] onward)
 *
 * The message is built from fuzz bytes: we copy into a local buffer and
 * always null-terminate at the last byte so diag_push sees a valid C string.
 * This lets the fuzzer explore all message lengths including exact-fit and
 * overflow without undefined behaviour in the harness itself.
 *
 * Invariants checked after every sequence:
 *   - d.depth <= DIAG_MAX_FRAMES
 *   - Every frame's message is null-terminated within DIAG_MAX_MSG_LEN bytes
 *   - diag_root() and diag_latest() return NULL iff depth == 0
 */

#define FUZZ_MSG_BUF ((usize)512)

int LLVMFuzzerTestOneInput(const u8 *data, usize size)
{
    Diag  d;
    usize n_pre;
    usize err_idx;
    char  msg_buf[FUZZ_MSG_BUF];
    usize msg_len;
    usize i;
    u8    raw[3];

    memset(raw, 0, sizeof(raw));
    if (size > 0) { raw[0] = data[0]; }
    if (size > 1) { raw[1] = data[1]; }

    n_pre   = (usize)raw[0] % (DIAG_MAX_FRAMES + 2u);
    err_idx = (usize)raw[1] % (usize)ERR_COUNT;

    /* Build message from remaining fuzz bytes */
    msg_len = (size > 2u) ? (size - 2u) : 0u;
    if (msg_len >= FUZZ_MSG_BUF) { msg_len = FUZZ_MSG_BUF - 1u; }
    if (msg_len > 0u) {
        memcpy(msg_buf, data + 2u, msg_len);
    }
    msg_buf[msg_len] = '\0'; /* always valid C string */

    d = diag_init();

    /* Push n_pre frames to reach near/at/over capacity */
    for (i = 0; i < n_pre; i++) {
        diag_push(&d, __FILE__, i, __func__, ERR_UNKNOWN, "fill");
    }

    /* Probe push with fuzz-derived code and message */
    diag_push(&d, __FILE__, (usize)__LINE__, __func__,
              (Error)err_idx, msg_buf);

    /* ── Invariant checks ────────────────────────────────────────────────── */

    if (d.depth > DIAG_MAX_FRAMES) {
        __builtin_trap(); /* depth overflow — corrupt internal state */
    }

    for (i = 0; i < d.depth; i++) {
        /* Every message must be null-terminated within the inline array */
        usize j;
        int found_null = 0;
        for (j = 0; j < DIAG_MAX_MSG_LEN; j++) {
            if (d.frames[i].message[j] == '\0') {
                found_null = 1;
                break;
            }
        }
        if (!found_null) {
            __builtin_trap(); /* message not null-terminated — buffer overrun */
        }
    }

    if (d.depth == 0) {
        if (diag_root(&d)   != NULL) { __builtin_trap(); }
        if (diag_latest(&d) != NULL) { __builtin_trap(); }
    } else {
        if (diag_root(&d)   == NULL) { __builtin_trap(); }
        if (diag_latest(&d) == NULL) { __builtin_trap(); }
    }

    return 0;
}

#endif /* CANON_FUZZING */
