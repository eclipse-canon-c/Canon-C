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
 * @file scope_test.c
 * @brief Tests for scope.h DEFER macro.
 *
 * Every test here is designed to FAIL if the macro regressed to
 * immediate execution or if any documented exit-method behavior broke.
 * The key test is test_defer_runs_after_body — that single test would
 * have caught the original broken scope.h in five seconds.
 */

#include "scope.h"
#include <stdio.h>
#include <string.h>

static int g_failed = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        g_failed++; \
    } \
} while (0)

#define EXPECT_SEQ(arr, expected, n) do { \
    int _ok = 1; \
    if ((int)(sizeof(arr)/sizeof((arr)[0])) < (n)) _ok = 0; \
    for (int _i = 0; _i < (n) && _ok; _i++) \
        if ((arr)[_i] != (expected)[_i]) _ok = 0; \
    if (!_ok) { \
        fprintf(stderr, "FAIL %s:%d  seq mismatch: got [", __FILE__, __LINE__); \
        for (int _i = 0; _i < (n); _i++) \
            fprintf(stderr, "%d%s", (arr)[_i], _i+1<(n)?",":""); \
        fprintf(stderr, "] want ["); \
        for (int _i = 0; _i < (n); _i++) \
            fprintf(stderr, "%d%s", (expected)[_i], _i+1<(n)?",":""); \
        fprintf(stderr, "]\n"); \
        g_failed++; \
    } \
} while (0)

static int seq[32];
static int seqn = 0;
static void rec(int v) { seq[seqn++] = v; }
static void seq_reset(void) { seqn = 0; memset(seq, 0, sizeof(seq)); }

/* ════════════════════════════════════════════════════════════════════════
   THE CRITICAL TEST — would have caught the original broken scope.h
   Cleanup must run AFTER the body, not at declaration.
   ════════════════════════════════════════════════════════════════════════ */
static void test_defer_runs_after_body(void)
{
    seq_reset();
    rec(1);
    DEFER(rec(3)) {
        rec(2);
    }
    int want[] = {1, 2, 3};
    EXPECT_SEQ(seq, want, 3);
}

/* Variation: multiple body statements, cleanup runs after all of them */
static void test_defer_after_multiple_statements(void)
{
    seq_reset();
    DEFER(rec(99)) {
        rec(1);
        rec(2);
        rec(3);
    }
    int want[] = {1, 2, 3, 99};
    EXPECT_SEQ(seq, want, 4);
}

/* ════════════════════════════════════════════════════════════════════════
   Cleanup must fire on normal exit (baseline)
   ════════════════════════════════════════════════════════════════════════ */
static void test_normal_exit(void)
{
    int cleanup_ran = 0;
    DEFER(cleanup_ran = 1) {
        /* nothing */
    }
    EXPECT(cleanup_ran == 1);
}

/* ════════════════════════════════════════════════════════════════════════
   Cleanup must fire on `continue` (continue jumps to for-increment)
   ════════════════════════════════════════════════════════════════════════ */
static void test_continue_fires_cleanup(void)
{
    int fires = 0;
    for (int i = 0; i < 3; i++) {
        DEFER(fires++) {
            continue;
        }
    }
    EXPECT(fires == 3);
}

/* ════════════════════════════════════════════════════════════════════════
   Cleanup must NOT fire on break (documented limitation).
   We test this so that if C99 semantics ever change, we notice.

   Note: the `break`, `return`, and `goto` statements in the next three
   tests are gated behind `always_true` — a `volatile int` the optimizer
   cannot prove nonzero. At runtime this is always taken (always_true
   is initialized to 1 and never written), so behavior is unchanged,
   but it defeats MSVC's C4702 "unreachable code" warning which would
   otherwise fire on the code after the DEFER block. The tests verify
   the exact same property they would without the gate.
   ════════════════════════════════════════════════════════════════════════ */
static volatile int always_true = 1;

static void test_break_skips_cleanup(void)
{
    int cleanup_ran = 0;
    for (int i = 0; i < 1; i++) {
        DEFER(cleanup_ran = 1) {
            if (always_true) break;
        }
    }
    EXPECT(cleanup_ran == 0);  /* documented: break skips cleanup */
}

/* ════════════════════════════════════════════════════════════════════════
   Cleanup must NOT fire on return (documented limitation)
   ════════════════════════════════════════════════════════════════════════ */
static int g_return_cleanup = 0;
static int helper_return(void)
{
    DEFER(g_return_cleanup = 1) {
        if (always_true) return 42;
    }
    return 0;
}
static void test_return_skips_cleanup(void)
{
    g_return_cleanup = 0;
    int r = helper_return();
    EXPECT(r == 42);
    EXPECT(g_return_cleanup == 0);  /* documented */
}

/* ════════════════════════════════════════════════════════════════════════
   Cleanup must NOT fire on goto out (documented limitation)
   ════════════════════════════════════════════════════════════════════════ */
static void test_goto_skips_cleanup(void)
{
    int cleanup_ran = 0;
    DEFER(cleanup_ran = 1) {
        if (always_true) goto out;
    }
out:
    EXPECT(cleanup_ran == 0);  /* documented */
}

/* ════════════════════════════════════════════════════════════════════════
   Multiple DEFER in same scope — unique variable names via __LINE__
   ════════════════════════════════════════════════════════════════════════ */
static void test_multiple_defer_same_scope(void)
{
    seq_reset();
    DEFER(rec(10)) {
        DEFER(rec(20)) {
            rec(1);
        }
        /* rec(20) fires here */
        rec(2);
    }
    /* rec(10) fires here */
    int want[] = {1, 20, 2, 10};
    EXPECT_SEQ(seq, want, 4);
}

/* ════════════════════════════════════════════════════════════════════════
   Nesting — three levels deep, check LIFO-ish order for nested DEFERs
   ════════════════════════════════════════════════════════════════════════ */
static void test_three_level_nesting(void)
{
    seq_reset();
    DEFER(rec(30)) {
        DEFER(rec(20)) {
            DEFER(rec(10)) {
                rec(1);
            }
            rec(2);
        }
        rec(3);
    }
    int want[] = {1, 10, 2, 20, 3, 30};
    EXPECT_SEQ(seq, want, 6);
}

/* ════════════════════════════════════════════════════════════════════════
   DEFER inside a loop — cleanup runs every iteration
   ════════════════════════════════════════════════════════════════════════ */
static void test_defer_in_loop(void)
{
    int cleanup_count = 0;
    for (int i = 0; i < 5; i++) {
        DEFER(cleanup_count++) {
            /* body */
        }
    }
    EXPECT(cleanup_count == 5);
}

/* ════════════════════════════════════════════════════════════════════════
   Realistic: file open/close with DEFER.
   Uses a relative filename so it works in the test binary's CWD on any
   platform. Hardcoding `/tmp/...` would break on Windows, which has no
   `/tmp` directory.
   ════════════════════════════════════════════════════════════════════════ */
static void test_real_file(void)
{
    const char* path = "scope_defer_test.tmp";

    FILE* f = NULL;
    int wrote = 0;
    DEFER(f && fclose(f)) {
        f = fopen(path, "w");
        if (!f) break;
        fputs("hello", f);
        wrote = 1;
    }
    EXPECT(wrote == 1);

    /* Verify the file is actually closed and flushed */
    FILE* g = fopen(path, "r");
    EXPECT(g != NULL);
    char buf[16] = {0};
    if (g) {
        size_t r = fread(buf, 1, 15, g);
        (void)r;
        fclose(g);
    }
    EXPECT(strcmp(buf, "hello") == 0);
    remove(path);
}

/* ════════════════════════════════════════════════════════════════════════
   Comma-operator for multi-step cleanup
   ════════════════════════════════════════════════════════════════════════ */
static void test_comma_cleanup(void)
{
    int a = 0, b = 0, c = 0;
    DEFER((a = 1, b = 2, c = 3)) {
        EXPECT(a == 0 && b == 0 && c == 0);
    }
    EXPECT(a == 1);
    EXPECT(b == 2);
    EXPECT(c == 3);
}

/* ════════════════════════════════════════════════════════════════════════
   DEFER body observes cleanup has NOT yet happened
   ════════════════════════════════════════════════════════════════════════ */
static void test_body_before_cleanup(void)
{
    int flag = 0;
    DEFER(flag = 1) {
        EXPECT(flag == 0);  /* cleanup hasn't run yet */
    }
    EXPECT(flag == 1);      /* now it has */
}

int main(void)
{
    test_defer_runs_after_body();          /* the critical test */
    test_defer_after_multiple_statements();
    test_normal_exit();
    test_continue_fires_cleanup();
    test_break_skips_cleanup();
    test_return_skips_cleanup();
    test_goto_skips_cleanup();
    test_multiple_defer_same_scope();
    test_three_level_nesting();
    test_defer_in_loop();
    test_real_file();
    test_comma_cleanup();
    test_body_before_cleanup();

    if (g_failed == 0) {
        printf("OK  scope_test  all assertions passed\n");
        return 0;
    }
    fprintf(stderr, "FAILED  %d assertion(s)\n", g_failed);
    return 1;
}
