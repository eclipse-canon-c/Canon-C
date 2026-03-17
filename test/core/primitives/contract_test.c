/**
 * @file contract_test.c
 * @brief Tests for contract.h
 *
 * Strategy for "should fire" tests:
 * ─────────────────────────────────
 * Contract violations call abort() by default. To test that a violation
 * fires without terminating the process, we install a custom handler that
 * calls longjmp() instead of abort(). This is portable (C99, no fork/wait)
 * and works on Windows and Linux alike.
 *
 * The ASSERT_FIRES / ASSERT_SILENT macros encapsulate the pattern:
 *   1. Set a longjmp target with setjmp().
 *   2. Execute the expression.
 *   3. If the handler fires → longjmp back → record pass.
 *   4. If no handler fires  → fall through  → record fail.
 *
 * Flag matrix tests:
 * ──────────────────
 * ensure() and unreachable() behave differently under NDEBUG and CANON_STRICT.
 * Those combinations require separate compilation units compiled with the
 * appropriate flags. They live in:
 *   contract_test_ndebug.c   — compiled with -DNDEBUG
 *   contract_test_strict.c   — compiled with -DCANON_STRICT
 *   contract_test_no_req.c   — compiled with -DCANON_NO_REQUIRE
 *
 * static_require negative test:
 * ─────────────────────────────
 * A false static_require must fail at compile time. That is verified by
 * contract_test_static_fail.c, which is expected NOT to compile. The build
 * system marks it as a negative compilation test.
 */

#define CANON_CONTRACT_IMPL
#include "contract.h"

#include <stdio.h>
#include <setjmp.h>
#include <string.h>

/* ============================================================================
 * Test Helpers
 * ========================================================================= */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define PASS(label) \
    do { tests_run++; tests_passed++; printf("PASS: %s\n", label); } while (0)

#define FAIL(label) \
    do { \
        tests_run++; tests_failed++; \
        fprintf(stderr, "FAIL: %s  (%s:%d)\n", label, __FILE__, __LINE__); \
    } while (0)

#define ASSERT_TRUE(label, expr) \
    do { if (expr) PASS(label); else FAIL(label); } while (0)

#define ASSERT_FALSE(label, expr) \
    do { if (!(expr)) PASS(label); else FAIL(label); } while (0)

/* ============================================================================
 * longjmp-based violation capture
 *
 * ASSERT_FIRES(label, stmt):
 *   Passes if stmt causes the contract handler to fire.
 *
 * ASSERT_SILENT(label, stmt):
 *   Passes if stmt does NOT cause the contract handler to fire.
 * ========================================================================= */

/* Shared jump buffer for the capture handler */
static jmp_buf  _contract_jmp;
static int      _handler_fired;

/* Fields captured from the last handler invocation */
static char     _cap_file[256];
static int      _cap_line;
static char     _cap_func[256];
static char     _cap_expr[256];
static char     _cap_msg[256];   /* empty string when msg == NULL */

static void _capture_handler(
    const char* file, int line, const char* func,
    const char* expr, const char* msg)
{
    /* Copy fields before jumping — they may be stack-allocated by the macro */
    strncpy(_cap_file, file ? file : "", sizeof(_cap_file) - 1);
    strncpy(_cap_func, func ? func : "", sizeof(_cap_func) - 1);
    strncpy(_cap_expr, expr ? expr : "", sizeof(_cap_expr) - 1);
    strncpy(_cap_msg,  msg  ? msg  : "", sizeof(_cap_msg)  - 1);
    _cap_line = line;
    _handler_fired = 1;
    longjmp(_contract_jmp, 1);
}

/*
 * Execute stmt inside a setjmp frame with the capture handler installed.
 * After the macro, _handler_fired == 1 if the handler was called.
 * The previous handler is always restored, even if stmt longjmps.
 */
#define _CAPTURE_VIOLATION(stmt)                               \
    do {                                                       \
        contract_handler_fn _prev = canon_contract_handler;   \
        contract_set_handler(_capture_handler);               \
        _handler_fired = 0;                                    \
        memset(_cap_file, 0, sizeof(_cap_file));               \
        memset(_cap_func, 0, sizeof(_cap_func));               \
        memset(_cap_expr, 0, sizeof(_cap_expr));               \
        memset(_cap_msg,  0, sizeof(_cap_msg));                \
        _cap_line = 0;                                         \
        if (setjmp(_contract_jmp) == 0) {                      \
            stmt;                                              \
        }                                                      \
        contract_set_handler(_prev);                           \
    } while (0)

#define ASSERT_FIRES(label, stmt)          \
    do {                                   \
        _CAPTURE_VIOLATION(stmt);          \
        if (_handler_fired) PASS(label);   \
        else                FAIL(label);   \
    } while (0)

#define ASSERT_SILENT(label, stmt)          \
    do {                                    \
        _CAPTURE_VIOLATION(stmt);           \
        if (!_handler_fired) PASS(label);   \
        else                 FAIL(label);   \
    } while (0)

/* ============================================================================
 * 1. require() — always-on precondition
 * ========================================================================= */

static void test_require(void) {
    /* True condition: handler must not fire */
    ASSERT_SILENT("require: true condition does not fire",
        require(1 == 1));

    ASSERT_SILENT("require: true expression does not fire",
        require(2 + 2 == 4));

    /* False condition: handler must fire */
    ASSERT_FIRES("require: false condition fires",
        require(1 == 2));

    ASSERT_FIRES("require: zero fires",
        require(0));

    /* The captured expression must be the stringified condition */
    _CAPTURE_VIOLATION(require(1 == 2));
    ASSERT_TRUE("require: expr captured correctly",
        strcmp(_cap_expr, "1 == 2") == 0);

    /* File and function must be populated */
    ASSERT_TRUE("require: file is populated",  _cap_file[0] != '\0');
    ASSERT_TRUE("require: func is populated",  _cap_func[0] != '\0');
    ASSERT_TRUE("require: line is positive",   _cap_line > 0);

    /* msg must be NULL → captured as empty string */
    ASSERT_TRUE("require: msg is empty when not provided",
        _cap_msg[0] == '\0');
}

/* ============================================================================
 * 2. require_msg() — always-on precondition with message
 * ========================================================================= */

static void test_require_msg(void) {
    ASSERT_SILENT("require_msg: true condition does not fire",
        require_msg(1 == 1, "should not fire"));

    ASSERT_FIRES("require_msg: false condition fires",
        require_msg(0, "expected failure"));

    /* Message must be forwarded to the handler */
    _CAPTURE_VIOLATION(require_msg(0, "sentinel message"));
    ASSERT_TRUE("require_msg: message captured correctly",
        strcmp(_cap_msg, "sentinel message") == 0);

    _CAPTURE_VIOLATION(require_msg(1 == 2, "bad arithmetic"));
    ASSERT_TRUE("require_msg: expr captured correctly",
        strcmp(_cap_expr, "1 == 2") == 0);
    ASSERT_TRUE("require_msg: message captured correctly",
        strcmp(_cap_msg, "bad arithmetic") == 0);
}

/* ============================================================================
 * 3. ensure() — debug-only (this TU is compiled without NDEBUG/CANON_STRICT)
 *
 * In this default configuration, ensure() behaves identically to require().
 * The NDEBUG and CANON_STRICT variants are tested in separate TUs.
 * ========================================================================= */

static void test_ensure_default(void) {
    ASSERT_SILENT("ensure (default): true condition does not fire",
        ensure(1 == 1));

    ASSERT_FIRES("ensure (default): false condition fires",
        ensure(0));

    ASSERT_FIRES("ensure_msg (default): false condition fires",
        ensure_msg(0, "debug check"));

    _CAPTURE_VIOLATION(ensure_msg(0, "ensure sentinel"));
    ASSERT_TRUE("ensure_msg (default): message captured",
        strcmp(_cap_msg, "ensure sentinel") == 0);
}

/* ============================================================================
 * 4. panic() — unconditional, never disabled
 * ========================================================================= */

static void test_panic(void) {
    ASSERT_FIRES("panic: always fires",
        panic("fatal error"));

    _CAPTURE_VIOLATION(panic("panic sentinel"));
    ASSERT_TRUE("panic: message captured",
        strcmp(_cap_msg, "panic sentinel") == 0);

    /* expr field for panic must be the literal string "panic" */
    _CAPTURE_VIOLATION(panic("anything"));
    ASSERT_TRUE("panic: expr field is 'panic'",
        strcmp(_cap_expr, "panic") == 0);
}

/* ============================================================================
 * 5. unreachable() — fires in debug builds (no NDEBUG, no CANON_STRICT here)
 * ========================================================================= */

static void test_unreachable(void) {
    ASSERT_FIRES("unreachable: fires in debug build",
        unreachable());

    ASSERT_FIRES("unreachable_msg: fires in debug build",
        unreachable_msg("should not get here"));

    _CAPTURE_VIOLATION(unreachable_msg("unreachable sentinel"));
    ASSERT_TRUE("unreachable_msg: message captured",
        strcmp(_cap_msg, "unreachable sentinel") == 0);

    /* expr field must identify the site as an unreachable path */
    _CAPTURE_VIOLATION(unreachable());
    ASSERT_TRUE("unreachable: expr field is 'unreachable code path'",
        strcmp(_cap_expr, "unreachable code path") == 0);
}

/* ============================================================================
 * 6. contract_set_handler() — installation and dispatch
 * ========================================================================= */

static int _custom_handler_called = 0;

static void _custom_handler(
    const char* file, int line, const char* func,
    const char* expr, const char* msg)
{
    (void)file; (void)line; (void)func; (void)expr; (void)msg;
    _custom_handler_called = 1;
    longjmp(_contract_jmp, 1);
}

static void test_set_handler(void) {
    contract_handler_fn original = canon_contract_handler;

    /* Install custom handler and verify it is called */
    contract_set_handler(_custom_handler);
    _custom_handler_called = 0;
    if (setjmp(_contract_jmp) == 0) {
        require(0);
    }
    contract_set_handler(original);
    ASSERT_TRUE("set_handler: custom handler is called on violation",
        _custom_handler_called == 1);

    /* NULL restores the default handler */
    contract_set_handler(NULL);
    ASSERT_TRUE("set_handler: NULL restores default handler",
        canon_contract_handler == contract_default_handler);
    contract_set_handler(original);

    /* Handler installed before a call fires, not the one installed after */
    contract_set_handler(_capture_handler);
    _handler_fired = 0;
    if (setjmp(_contract_jmp) == 0) {
        require(0);  /* must call _capture_handler, not default */
    }
    contract_set_handler(original);
    ASSERT_TRUE("set_handler: handler active at call site is invoked",
        _handler_fired == 1);
}

/* ============================================================================
 * 7. Handler fields — file, line, func accuracy
 * ========================================================================= */

static void _field_check_helper(void) {
    require(0);  /* line is captured here */
}

static void test_handler_fields(void) {
    /* Line number must match the actual source line of the violation */
    _CAPTURE_VIOLATION(require(0));
    int captured_line = _cap_line;
    ASSERT_TRUE("fields: line number is positive", captured_line > 0);

    /* File must end in contract_test.c */
    int flen = (int)strlen(_cap_file);
    int slen = (int)strlen("contract_test.c");
    ASSERT_TRUE("fields: file ends in contract_test.c",
        flen >= slen &&
        strcmp(_cap_file + flen - slen, "contract_test.c") == 0);

    /* func must be the name of the calling function */
    _CAPTURE_VIOLATION(_field_check_helper());
    ASSERT_TRUE("fields: func is _field_check_helper",
        strcmp(_cap_func, "_field_check_helper") == 0);

    /* Two consecutive violations produce different line numbers */
    int line_a, line_b;
    _CAPTURE_VIOLATION(require(0)); line_a = _cap_line;
    _CAPTURE_VIOLATION(require(0)); line_b = _cap_line;
    ASSERT_TRUE("fields: consecutive violations have different line numbers",
        line_a != line_b);
}

/* ============================================================================
 * 8. static_require — compile-time assertions
 *
 * Positive cases only (negative case must fail at compile time — see
 * contract_test_static_fail.c which is expected NOT to compile).
 * ========================================================================= */

/* These must compile without error */
static_require(sizeof(char) == 1,         char_is_one_byte);
static_require(sizeof(int)  >= 2,         int_at_least_two_bytes);
static_require(1 + 1 == 2,               arithmetic_is_sane);
static_require(sizeof(void*) >= 1,        pointer_has_size);

static void test_static_require(void) {
    /* If we reached this point, all static_require above compiled — pass */
    PASS("static_require: true conditions compile cleanly");
}

/* ============================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("contract_test (default build: require ON, ensure debug-only)\n");
    printf("──────────────────────────────────────────────────────────────\n");

    test_require();
    test_require_msg();
    test_ensure_default();
    test_panic();
    test_unreachable();
    test_set_handler();
    test_handler_fields();
    test_static_require();

    printf("\nResults: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED\n", tests_failed);
        return 1;
    }
    printf(" — all tests passed!\n");
    return 0;
}
