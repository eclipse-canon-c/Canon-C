/**
 * @file contract_test.c
 * @brief Unit tests for contract.h
 *
 * Strategy
 * ───────────────────────────────────────────────────────────────────────────
 * contract.h macros call canon_contract_handler on violation and that
 * handler must not return. Testing violation behaviour therefore requires
 * intercepting the handler before it calls abort(). We do this by:
 *
 *   1. Installing a capture handler via contract_set_handler() that records
 *      the violation arguments into static buffers and then longjmp()s back
 *      to the test site instead of calling abort().
 *   2. Using setjmp() around each "should fire" call to catch the jump.
 *   3. Restoring the capture handler after each test group so subsequent
 *      passing tests still use a safe handler.
 *
 * This requires CANON_CONTRACT_IMPL in exactly one TU — this file is that TU.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   require()        — fires on false, silent on true
 *   require_msg()    — fires on false with correct message, silent on true
 *   ensure()         — fires on false in debug, silent in release (NDEBUG)
 *   ensure_msg()     — fires on false with correct message
 *   unreachable()    — always fires in debug builds
 *   unreachable_msg()— fires with correct message
 *   panic()          — always fires, correct message
 *   contract_set_handler() — custom handler is called; NULL restores default
 *   static_require() — compile-time; tested via static_require(1 == 1, ...)
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   setjmp/longjmp are C89/C99 standard. No 0b literals. Variables declared
 *   before use. CANON_MAYBE_UNUSED suppresses -Wunused-function for the
 *   static helper functions below.
 */

/* Must be defined in exactly one TU before including contract.h */
#define CANON_CONTRACT_IMPL
#include "contract.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Portability
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
 * Capture handler
 *
 * Installed via contract_set_handler() before any "should fire" test.
 * Records the violation arguments into static buffers, then longjmp()s
 * back to the setjmp() site in the test. Never calls abort().
 * ====================================================================== */

#define CAP_BUF 256

static jmp_buf          _cap_jmp;
static int              _cap_fired  = 0;
static int              _cap_line   = 0;
static char             _cap_file[CAP_BUF];
static char             _cap_func[CAP_BUF];
static char             _cap_expr[CAP_BUF];
static char             _cap_msg[CAP_BUF];   /* empty string when msg == NULL */

static CANON_MAYBE_UNUSED void _capture_handler(
    const char* file, int line, const char* func,
    const char* expr, const char* msg)
{
    _cap_fired = 1;
    _cap_line  = line;
    strncpy(_cap_file, file ? file : "", CAP_BUF - 1);
    _cap_file[CAP_BUF - 1] = '\0';
    strncpy(_cap_func, func ? func : "", CAP_BUF - 1);
    _cap_func[CAP_BUF - 1] = '\0';
    strncpy(_cap_expr, expr ? expr : "", CAP_BUF - 1);
    _cap_expr[CAP_BUF - 1] = '\0';
    strncpy(_cap_msg, msg ? msg : "", CAP_BUF - 1);
    _cap_msg[CAP_BUF - 1] = '\0';
    longjmp(_cap_jmp, 1);
}

/* Reset capture state before each "should fire" block */
static void _cap_reset(void) {
    _cap_fired = 0;
    _cap_line  = 0;
    _cap_file[0] = '\0';
    _cap_func[0] = '\0';
    _cap_expr[0] = '\0';
    _cap_msg[0]  = '\0';
}

/*
 * FIRES(stmt) — execute stmt expecting a violation.
 * Sets _cap_fired=1 if the handler was called, 0 if not.
 */
#define FIRES(stmt)                     \
    do {                                \
        _cap_reset();                   \
        if (setjmp(_cap_jmp) == 0) {    \
            stmt;                       \
        }                               \
    } while (0)

/*
 * SILENT(stmt) — execute stmt expecting no violation.
 * If the handler fires anyway the longjmp skips the rest of the block,
 * which is benign for our test structure.
 */
#define SILENT(stmt)                    \
    do {                                \
        _cap_reset();                   \
        if (setjmp(_cap_jmp) == 0) {    \
            stmt;                       \
            /* _cap_fired remains 0 */  \
        }                               \
    } while (0)

/* =========================================================================
 * require() tests
 * ====================================================================== */

static void test_require(void) {
    contract_set_handler(_capture_handler);

    /* True condition — must not fire */
    SILENT(require(1 == 1));
    EXPECT(_cap_fired == 0);

    SILENT(require(1 > 0));
    EXPECT(_cap_fired == 0);

    /* False condition — must fire */
    FIRES(require(0));
    EXPECT(_cap_fired == 1);

    FIRES(require(1 == 2));
    EXPECT(_cap_fired == 1);

    /* expr string is captured (not NULL) */
    FIRES(require(0 == 1));
    EXPECT(_cap_fired == 1);
    EXPECT(_cap_expr[0] != '\0');

    /* msg is NULL for require() (no message variant) */
    FIRES(require(0));
    EXPECT(_cap_fired == 1);
    EXPECT(_cap_msg[0] == '\0');  /* handler received NULL → stored as "" */

    /* file and func are populated */
    FIRES(require(0));
    EXPECT(_cap_fired == 1);
    EXPECT(_cap_file[0] != '\0');
    EXPECT(_cap_func[0] != '\0');
    EXPECT(_cap_line > 0);

    contract_set_handler(_capture_handler);  /* ensure still installed */
}

/* =========================================================================
 * require_msg() tests
 * ====================================================================== */

static void test_require_msg(void) {
    contract_set_handler(_capture_handler);

    /* True condition — must not fire */
    SILENT(require_msg(1 == 1, "should not fire"));
    EXPECT(_cap_fired == 0);

    /* False condition — must fire with correct message */
    FIRES(require_msg(0, "sentinel_message"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "sentinel_message") == 0);

    FIRES(require_msg(1 == 2, "another message"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "another message") == 0);

    /* expr string is not empty */
    FIRES(require_msg(0, "check expr"));
    EXPECT(_cap_expr[0] != '\0');

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * ensure() tests
 *
 * ensure() is debug-only. In a Release build (NDEBUG defined without
 * CANON_STRICT) it compiles away completely. We test what is observable
 * in the current build configuration.
 * ====================================================================== */

static void test_ensure(void) {
    contract_set_handler(_capture_handler);

#if defined(CANON_STRICT)
    /* CANON_STRICT: ensure is always-on */
    SILENT(ensure(1 == 1));
    EXPECT(_cap_fired == 0);

    FIRES(ensure(0));
    EXPECT(_cap_fired == 1);

    FIRES(ensure(1 == 2));
    EXPECT(_cap_fired == 1);

#elif defined(NDEBUG)
    /* Release build without CANON_STRICT: ensure compiles to no-op */
    FIRES(ensure(0));   /* even (0) must NOT fire */
    EXPECT(_cap_fired == 0);

#else
    /* Debug build: ensure behaves like require */
    SILENT(ensure(1 == 1));
    EXPECT(_cap_fired == 0);

    FIRES(ensure(0));
    EXPECT(_cap_fired == 1);

    FIRES(ensure(1 == 2));
    EXPECT(_cap_fired == 1);
#endif

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * ensure_msg() tests
 * ====================================================================== */

static void test_ensure_msg(void) {
    contract_set_handler(_capture_handler);

#if defined(CANON_STRICT)
    SILENT(ensure_msg(1 == 1, "no fire"));
    EXPECT(_cap_fired == 0);

    FIRES(ensure_msg(0, "strict_ensure_msg"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "strict_ensure_msg") == 0);

#elif defined(NDEBUG)
    FIRES(ensure_msg(0, "release_ensure_msg"));
    EXPECT(_cap_fired == 0);  /* no-op in release */

#else
    SILENT(ensure_msg(1 == 1, "no fire"));
    EXPECT(_cap_fired == 0);

    FIRES(ensure_msg(0, "debug_ensure_msg"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "debug_ensure_msg") == 0);
#endif

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * unreachable() tests
 *
 * unreachable() fires the handler in debug builds and CANON_STRICT.
 * In Release builds without CANON_STRICT it expands to a compiler hint
 * that does not call the handler.
 * ====================================================================== */

static void test_unreachable(void) {
    contract_set_handler(_capture_handler);

#if defined(NDEBUG) && !defined(CANON_STRICT)
    /* Release build: unreachable() is a pure hint — must not fire */
    FIRES(unreachable());
    EXPECT(_cap_fired == 0);
#else
    /* Debug or CANON_STRICT: must fire */
    FIRES(unreachable());
    EXPECT(_cap_fired == 1);
    /* expr string contains "unreachable" */
    EXPECT(strstr(_cap_expr, "unreachable") != NULL);
    /* msg is NULL (no message variant) */
    EXPECT(_cap_msg[0] == '\0');
#endif

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * unreachable_msg() tests
 * ====================================================================== */

static void test_unreachable_msg(void) {
    contract_set_handler(_capture_handler);

#if defined(NDEBUG) && !defined(CANON_STRICT)
    FIRES(unreachable_msg("release_unreachable"));
    EXPECT(_cap_fired == 0);
#else
    FIRES(unreachable_msg("test_unreachable_msg_sentinel"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "test_unreachable_msg_sentinel") == 0);
    EXPECT(strstr(_cap_expr, "unreachable") != NULL);
#endif

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * panic() tests
 *
 * panic() is unconditional — never disabled by any flag.
 * ====================================================================== */

static void test_panic(void) {
    contract_set_handler(_capture_handler);

    FIRES(panic("test_panic_message"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "test_panic_message") == 0);

    /* expr is "panic" for panic() calls */
    FIRES(panic("second_panic"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_expr, "panic") == 0);
    EXPECT(strcmp(_cap_msg, "second_panic") == 0);

    /* file, func, line populated */
    FIRES(panic("location_check"));
    EXPECT(_cap_fired == 1);
    EXPECT(_cap_file[0] != '\0');
    EXPECT(_cap_func[0] != '\0');
    EXPECT(_cap_line > 0);

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * contract_set_handler() tests
 * ====================================================================== */

static int _custom_called = 0;
static char _custom_msg[CAP_BUF];

static CANON_MAYBE_UNUSED void _custom_handler(
    const char* file, int line, const char* func,
    const char* expr, const char* msg)
{
    (void)file; (void)line; (void)func; (void)expr;
    _custom_called = 1;
    strncpy(_custom_msg, msg ? msg : "", CAP_BUF - 1);
    _custom_msg[CAP_BUF - 1] = '\0';
    longjmp(_cap_jmp, 1);
}

static void test_set_handler(void) {
    /* Install custom handler — panic should call it */
    contract_set_handler(_custom_handler);
    _custom_called = 0;
    _custom_msg[0] = '\0';

    _cap_reset();
    if (setjmp(_cap_jmp) == 0) {
        panic("custom_handler_test");
    }
    EXPECT(_custom_called == 1);
    EXPECT(strcmp(_custom_msg, "custom_handler_test") == 0);

    /* Passing NULL restores default handler.
     * We can't easily test that abort() is called, but we can verify
     * the handler pointer changes back from our custom one by installing
     * the capture handler again and confirming it works. */
    contract_set_handler(NULL);
    contract_set_handler(_capture_handler);

    _cap_reset();
    FIRES(panic("after_null_restore"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "after_null_restore") == 0);

    /* Install a second custom handler, then switch to capture handler */
    contract_set_handler(_custom_handler);
    _custom_called = 0;
    if (setjmp(_cap_jmp) == 0) {
        require(0);
    }
    EXPECT(_custom_called == 1);

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * Diagnostic fields — file, line, func correctness
 *
 * Verifies that the handler receives the correct source location.
 * ====================================================================== */

static void test_diagnostic_fields(void) {
    /*
     * Declared volatile to prevent -Werror=clobbered: local variables
     * that are written before setjmp and read after longjmp must be
     * volatile so the compiler does not cache them in a register that
     * longjmp may restore to a stale value.
     */
    volatile int line_a = 0;
    volatile int line_b = 0;

    contract_set_handler(_capture_handler);

    /* require(0) — verify line number, file, and func are populated */
    _cap_reset();
    if (setjmp(_cap_jmp) == 0) {
        line_a = __LINE__; require(0);  /* same line so __LINE__ matches */
    }
    EXPECT(_cap_fired == 1);
    EXPECT(_cap_line == line_a);
    EXPECT(strstr(_cap_file, "contract_test") != NULL ||
           strstr(_cap_file, "contract_test.c") != NULL);
    EXPECT(_cap_func[0] != '\0');

    /* panic() — verify line number is captured */
    _cap_reset();
    if (setjmp(_cap_jmp) == 0) {
        line_b = __LINE__; panic("line_check");  /* same line */
    }
    EXPECT(_cap_fired == 1);
    EXPECT(_cap_line == line_b);
    EXPECT(_cap_line > 0);

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * static_require() tests
 *
 * static_require() is a compile-time assertion. If the condition is true
 * it produces no code at all. We can only test passing conditions at
 * runtime — a failing condition would be a compile error, not a test
 * failure, which is the correct and desired behaviour.
 * ====================================================================== */

/* These must compile without error — that is the test */
static_require(1 == 1, one_equals_one);
static_require(sizeof(char) == 1, char_is_one_byte);
static_require(sizeof(int) >= 2, int_at_least_two_bytes);
static_require(sizeof(u64) == 8, u64_is_eight_bytes);
static_require(sizeof(u32) == 4, u32_is_four_bytes);
static_require(sizeof(u16) == 2, u16_is_two_bytes);
static_require(sizeof(u8)  == 1, u8_is_one_byte);

static void test_static_require(void) {
    /* If we reached this function, all the static_require() calls above
     * compiled successfully — that is the entire test. */
    EXPECT(1);  /* always passes — confirms we got here */
}

/* =========================================================================
 * Interaction: require fires even if ensure would not (NDEBUG)
 * ====================================================================== */

static void test_require_fires_in_release(void) {
    contract_set_handler(_capture_handler);

    /* require() must always fire regardless of NDEBUG */
    FIRES(require(0));
    EXPECT(_cap_fired == 1);

    FIRES(require_msg(0, "require_in_release"));
    EXPECT(_cap_fired == 1);
    EXPECT(strcmp(_cap_msg, "require_in_release") == 0);

    contract_set_handler(_capture_handler);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void) {
    test_require();
    test_require_msg();
    test_ensure();
    test_ensure_msg();
    test_unreachable();
    test_unreachable_msg();
    test_panic();
    test_set_handler();
    test_diagnostic_fields();
    test_static_require();
    test_require_fires_in_release();

    printf("\ncontract_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
