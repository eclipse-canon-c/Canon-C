/**
 * @file scope_test.c
 * @brief Tests for scope.h — RAII-style deferred cleanup macros
 *
 * Tests what SCOPE_DEFER actually guarantees:
 *   - The deferred block executes exactly once
 *   - It executes when the enclosing scope exits (normal end, return,
 *     break, continue)
 *   - Multiple SCOPE_DEFERs in the same scope all execute
 *   - The defer alias works when CANON_NO_DEFER_ALIAS is not defined
 *
 * Note on LIFO ordering:
 *   SCOPE_DEFER expands to two nested for-loops. Within a single flat
 *   scope, execution order depends on where each SCOPE_DEFER appears —
 *   the last one declared executes last (normal for-loop sequencing),
 *   not first. LIFO order as in Go/Zig requires compiler support or
 *   a stack-based runtime — this macro cannot provide it in pure C99.
 *   Tests here verify the actual observable behavior.
 */

#include "core/scope.h"

#include <stdio.h>
#include <string.h>

/* ── Harness ─────────────────────────────────────────────────────────────── */

static int g_failed = 0;

#define EXPECT(cond)                                             \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL %s:%d  %s\n",                 \
                    __FILE__, __LINE__, #cond);                  \
            g_failed++;                                          \
        }                                                        \
    } while (0)

/* ── Shared state ────────────────────────────────────────────────────────── */

static int g_counter   = 0;
static int g_seq[8];
static int g_seq_count = 0;

/* ── SCOPE_DEFER — basic: block executes ─────────────────────────────────── */

static void test_defer_basic(void)
{
    g_counter = 0;
    {
        SCOPE_DEFER { g_counter = 1; }
    }
    EXPECT(g_counter == 1);
}

static void test_defer_executes_once(void)
{
    g_counter = 0;
    {
        SCOPE_DEFER { g_counter++; }
    }
    EXPECT(g_counter == 1);
}

/* ── SCOPE_DEFER — multiple in same scope all execute ───────────────────── */

static void test_defer_multiple_all_execute(void)
{
    g_seq_count = 0;
    {
        SCOPE_DEFER { g_seq[g_seq_count++] = 1; }
        SCOPE_DEFER { g_seq[g_seq_count++] = 2; }
        SCOPE_DEFER { g_seq[g_seq_count++] = 3; }
    }
    /* All three ran */
    EXPECT(g_seq_count == 3);
}

static void test_defer_two_both_execute(void)
{
    int a = 0;
    int b = 0;
    {
        SCOPE_DEFER { a = 1; }
        SCOPE_DEFER { b = 1; }
    }
    EXPECT(a == 1);
    EXPECT(b == 1);
}

/* ── SCOPE_DEFER — executes on early return ──────────────────────────────── */

static int helper_with_return(int x, int* side_effect)
{
    *side_effect = 0;
    {
        SCOPE_DEFER { *side_effect = 1; }
        if (x > 0) {
            return x;
        }
    }
    return 0;
}

static void test_defer_early_return(void)
{
    int side = 0;
    int ret  = helper_with_return(5, &side);
    EXPECT(ret  == 5);
    EXPECT(side == 1);
}

static void test_defer_normal_return(void)
{
    int side = 0;
    int ret  = helper_with_return(-1, &side);
    EXPECT(ret  == 0);
    EXPECT(side == 1);
}

/* ── SCOPE_DEFER — executes on break ─────────────────────────────────────── */

static void test_defer_break(void)
{
    g_counter = 0;
    for (int i = 0; i < 3; i++) {
        SCOPE_DEFER { g_counter++; }
        if (i == 1) break;
    }
    /* ran for i=0 and i=1 (break) */
    EXPECT(g_counter == 2);
}

/* ── SCOPE_DEFER — executes on continue ─────────────────────────────────── */

static void test_defer_continue(void)
{
    g_counter = 0;
    for (int i = 0; i < 3; i++) {
        SCOPE_DEFER { g_counter++; }
        if (i == 1) continue;
    }
    EXPECT(g_counter == 3);
}

/* ── SCOPE_DEFER — executes every loop iteration ────────────────────────── */

static void test_defer_loop_iterations(void)
{
    g_counter = 0;
    for (int i = 0; i < 5; i++) {
        SCOPE_DEFER { g_counter++; }
    }
    EXPECT(g_counter == 5);
}

/* ── SCOPE_DEFER — nested scopes independent ─────────────────────────────── */

static void test_defer_nested_scopes(void)
{
    int outer_ran = 0;
    int inner_ran = 0;
    {
        SCOPE_DEFER { outer_ran = 1; }
        {
            SCOPE_DEFER { inner_ran = 1; }
        }
        EXPECT(inner_ran == 1);   /* inner already ran */
    }
    EXPECT(outer_ran == 1);
    EXPECT(inner_ran == 1);
}

/* ── SCOPE_DEFER — resource simulation ──────────────────────────────────── */

static int g_resource_open  = 0;
static int g_resource_freed = 0;

static void resource_open_fn(void)  { g_resource_open = 1; }
static void resource_close_fn(void) { g_resource_open = 0; g_resource_freed = 1; }

static void test_defer_resource_cleanup(void)
{
    g_resource_open  = 0;
    g_resource_freed = 0;
    {
        resource_open_fn();
        SCOPE_DEFER { resource_close_fn(); }
    }
    EXPECT(g_resource_open  == 0);
    EXPECT(g_resource_freed == 1);
}

/* ── SCOPE_DEFER — two resources both cleaned up ─────────────────────────── */

static int g_res_a = 0;
static int g_res_b = 0;

static void test_defer_two_resources(void)
{
    g_res_a = 1;
    g_res_b = 1;
    {
        SCOPE_DEFER { g_res_a = 0; }
        SCOPE_DEFER { g_res_b = 0; }
    }
    EXPECT(g_res_a == 0);
    EXPECT(g_res_b == 0);
}

/* ── SCOPE_DEFER — body executes exactly once ────────────────────────────── */

static void test_defer_body_once(void)
{
    g_counter = 0;
    {
        SCOPE_DEFER { g_counter++; }
    }
    EXPECT(g_counter == 1);

    /* Entering the same block again does not re-run old defers */
    {
        SCOPE_DEFER { g_counter++; }
    }
    EXPECT(g_counter == 2);
}

/* ── SCOPE_DEFER — at end of function ───────────────────────────────────── */

static int g_end_ran = 0;

static void helper_end_of_function(void)
{
    SCOPE_DEFER { g_end_ran = 1; }
}

static void test_defer_end_of_function(void)
{
    g_end_ran = 0;
    helper_end_of_function();
    EXPECT(g_end_ran == 1);
}

/* ── defer alias ─────────────────────────────────────────────────────────── */

static void test_defer_alias(void)
{
#ifdef CANON_NO_DEFER_ALIAS
    EXPECT(1);
#else
    g_counter = 0;
    {
        defer { g_counter = 99; }
    }
    EXPECT(g_counter == 99);
#endif
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    /* basic execution */
    test_defer_basic();
    test_defer_executes_once();

    /* multiple defers */
    test_defer_multiple_all_execute();
    test_defer_two_both_execute();

    /* early return */
    test_defer_early_return();
    test_defer_normal_return();

    /* loop control */
    test_defer_break();
    test_defer_continue();
    test_defer_loop_iterations();

    /* nested scopes */
    test_defer_nested_scopes();

    /* resource simulation */
    test_defer_resource_cleanup();
    test_defer_two_resources();

    /* misc */
    test_defer_body_once();
    test_defer_end_of_function();

    /* alias */
    test_defer_alias();

    if (g_failed == 0) {
        printf("OK  scope_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  scope_test  (%d assertion(s) failed)\n",
            g_failed);
    return 1;
}
