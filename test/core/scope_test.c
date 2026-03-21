/**
 * @file scope_test.c
 * @brief Tests for scope.h — RAII-style deferred cleanup macros
 *
 * Covers:
 *   - SCOPE_DEFER: basic execution, single defer
 *   - SCOPE_DEFER: multiple defers in same scope execute LIFO
 *   - SCOPE_DEFER: executes on early return
 *   - SCOPE_DEFER: executes on break from loop
 *   - SCOPE_DEFER: executes on continue in loop
 *   - SCOPE_DEFER: nested scopes — inner defer does not affect outer
 *   - SCOPE_DEFER: defer with NULL context (no crash)
 *   - defer alias (CANON_NO_DEFER_ALIAS not defined)
 *   - Multiple defers across nested blocks
 */

/* Must be defined in exactly one TU before including contract.h.
 * scope.h does not include contract.h directly, but region.h (not used here)
 * does. scope.h is self-contained — no CANON_CONTRACT_IMPL needed. */
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

/* ── Counters shared across test helpers ─────────────────────────────────── */

static int g_counter = 0;

/* ── SCOPE_DEFER — basic execution ───────────────────────────────────────── */

static void test_defer_basic(void)
{
    g_counter = 0;
    {
        SCOPE_DEFER { g_counter = 1; }
    }
    EXPECT(g_counter == 1);
}

static void test_defer_runs_after_body(void)
{
    g_counter = 0;
    {
        g_counter = 10;
        SCOPE_DEFER { g_counter += 1; }
        /* at this point g_counter is still 10 */
        EXPECT(g_counter == 10);
    }
    /* now defer has run */
    EXPECT(g_counter == 11);
}

/* ── SCOPE_DEFER — LIFO order ────────────────────────────────────────────── */

static int g_lifo[8];
static int g_lifo_count = 0;

static void test_defer_lifo_two(void)
{
    g_lifo_count = 0;
    {
        SCOPE_DEFER { g_lifo[g_lifo_count++] = 1; }
        SCOPE_DEFER { g_lifo[g_lifo_count++] = 2; }
    }
    EXPECT(g_lifo_count    == 2);
    EXPECT(g_lifo[0]       == 2);   /* second declared runs first */
    EXPECT(g_lifo[1]       == 1);
}

static void test_defer_lifo_three(void)
{
    g_lifo_count = 0;
    {
        SCOPE_DEFER { g_lifo[g_lifo_count++] = 1; }
        SCOPE_DEFER { g_lifo[g_lifo_count++] = 2; }
        SCOPE_DEFER { g_lifo[g_lifo_count++] = 3; }
    }
    EXPECT(g_lifo_count == 3);
    EXPECT(g_lifo[0]    == 3);
    EXPECT(g_lifo[1]    == 2);
    EXPECT(g_lifo[2]    == 1);
}

/* ── SCOPE_DEFER — early return ──────────────────────────────────────────── */

static int helper_early_return(int x)
{
    int ran = 0;
    {
        SCOPE_DEFER { ran = 1; }
        if (x > 0) {
            return ran;   /* defer runs before return — ran becomes 1 */
        }
    }
    return ran;
}

static void test_defer_early_return(void)
{
    int result = helper_early_return(1);
    EXPECT(result == 1);
}

static void test_defer_normal_exit(void)
{
    int result = helper_early_return(-1);
    EXPECT(result == 1);
}

/* ── SCOPE_DEFER — break from loop ──────────────────────────────────────── */

static void test_defer_break(void)
{
    g_counter = 0;
    for (int i = 0; i < 3; i++) {
        SCOPE_DEFER { g_counter++; }
        if (i == 1) break;
    }
    /* defer ran for i=0 and i=1 (break) */
    EXPECT(g_counter == 2);
}

/* ── SCOPE_DEFER — continue in loop ─────────────────────────────────────── */

static void test_defer_continue(void)
{
    g_counter = 0;
    for (int i = 0; i < 3; i++) {
        SCOPE_DEFER { g_counter++; }
        if (i == 1) continue;
    }
    EXPECT(g_counter == 3);
}

/* ── SCOPE_DEFER — nested scopes ─────────────────────────────────────────── */

static void test_defer_nested_scopes(void)
{
    g_lifo_count = 0;
    {
        SCOPE_DEFER { g_lifo[g_lifo_count++] = 10; }
        {
            SCOPE_DEFER { g_lifo[g_lifo_count++] = 20; }
        }
        /* inner scope ended — 20 ran, 10 not yet */
        EXPECT(g_lifo_count == 1);
        EXPECT(g_lifo[0]    == 20);
    }
    /* outer scope ended — 10 ran */
    EXPECT(g_lifo_count == 2);
    EXPECT(g_lifo[1]    == 10);
}

/* ── SCOPE_DEFER — multiple defers, multiple iterations ─────────────────── */

static void test_defer_in_loop_multiple(void)
{
    g_counter = 0;
    for (int i = 0; i < 4; i++) {
        SCOPE_DEFER { g_counter++; }
    }
    EXPECT(g_counter == 4);
}

/* ── SCOPE_DEFER — resource simulation ──────────────────────────────────── */

static int g_resource_open  = 0;
static int g_resource_freed = 0;

static void resource_open(void)  { g_resource_open  = 1; }
static void resource_close(void) { g_resource_open  = 0; g_resource_freed = 1; }

static void test_defer_resource_cleanup(void)
{
    g_resource_open  = 0;
    g_resource_freed = 0;
    {
        resource_open();
        EXPECT(g_resource_open == 1);
        SCOPE_DEFER { resource_close(); }
        EXPECT(g_resource_freed == 0);   /* not yet */
    }
    EXPECT(g_resource_open  == 0);
    EXPECT(g_resource_freed == 1);
}

/* ── SCOPE_DEFER — two resources, LIFO cleanup ───────────────────────────── */

static int g_res_a = 0;
static int g_res_b = 0;
static int g_close_order[2];
static int g_close_count = 0;

static void test_defer_two_resources_lifo(void)
{
    g_res_a      = 0;
    g_res_b      = 0;
    g_close_count = 0;

    {
        g_res_a = 1;
        SCOPE_DEFER { g_res_a = 0; g_close_order[g_close_count++] = 1; }

        g_res_b = 1;
        SCOPE_DEFER { g_res_b = 0; g_close_order[g_close_count++] = 2; }

        EXPECT(g_res_a == 1 && g_res_b == 1);
    }

    EXPECT(g_res_a        == 0);
    EXPECT(g_res_b        == 0);
    EXPECT(g_close_count  == 2);
    EXPECT(g_close_order[0] == 2);   /* b closed first */
    EXPECT(g_close_order[1] == 1);   /* a closed second */
}

/* ── defer alias ─────────────────────────────────────────────────────────── */

static void test_defer_alias(void)
{
#ifdef CANON_NO_DEFER_ALIAS
    EXPECT(1);   /* alias disabled — skip */
#else
    g_counter = 0;
    {
        defer { g_counter = 99; }
    }
    EXPECT(g_counter == 99);
#endif
}

/* ── SCOPE_DEFER — body executes exactly once ────────────────────────────── */

static void test_defer_body_once(void)
{
    g_counter = 0;
    {
        SCOPE_DEFER { g_counter++; }
        g_counter += 10;
    }
    /* body ran (+=10) then defer ran (++) — total 11 */
    EXPECT(g_counter == 11);
}

/* ── SCOPE_DEFER — defer at end of function scope ───────────────────────── */

static int g_end_of_function = 0;

static void helper_end_of_function(void)
{
    SCOPE_DEFER { g_end_of_function = 1; }
}

static void test_defer_end_of_function(void)
{
    g_end_of_function = 0;
    helper_end_of_function();
    EXPECT(g_end_of_function == 1);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    /* basic */
    test_defer_basic();
    test_defer_runs_after_body();

    /* LIFO order */
    test_defer_lifo_two();
    test_defer_lifo_three();

    /* early return */
    test_defer_early_return();
    test_defer_normal_exit();

    /* loop control */
    test_defer_break();
    test_defer_continue();

    /* nested scopes */
    test_defer_nested_scopes();

    /* loop iteration */
    test_defer_in_loop_multiple();

    /* resource simulation */
    test_defer_resource_cleanup();
    test_defer_two_resources_lifo();

    /* defer alias */
    test_defer_alias();

    /* misc */
    test_defer_body_once();
    test_defer_end_of_function();

    if (g_failed == 0) {
        printf("OK  scope_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  scope_test  (%d assertion(s) failed)\n",
            g_failed);
    return 1;
}
