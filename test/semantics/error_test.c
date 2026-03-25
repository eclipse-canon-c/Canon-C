/*
 * Canon-C -- test/semantics/error_test.c
 *
 * Tests for semantics/error.h:
 *   - error_message()  : all named codes, ERR_COUNT, gap values, out-of-range
 *   - error_is_ok()    : true only for ERR_OK
 *   - error_in_range() : [ERR_OK, ERR_COUNT), gap pass, ERR_COUNT and negatives fail
 *   - error_code()     : correct int representations
 *   - Enum uniqueness  : all named codes are distinct
 *
 * Not FUZZABLE: error.h is a pure lookup table. Every path is reachable via
 * exhaustive unit testing; a fuzzer adds no finding capability here.
 *
 * Build (from repo root):
 *   cmake -B build && cmake --build build && ctest --test-dir build -R error_test
 */

#include <stddef.h>  /* NULL              */
#include <stdio.h>   /* fprintf, printf   */
#include <string.h>  /* strlen            */

#include "error.h"

/* ============================================================================
   Minimal test harness
   ============================================================================ */

static int g_total  = 0;
static int g_failed = 0;

#define EXPECT(cond)                                                       \
    do {                                                                   \
        ++g_total;                                                         \
        if (!(cond)) {                                                     \
            ++g_failed;                                                    \
            fprintf(stderr, "  FAIL  %s:%d  %s\n",                        \
                    __FILE__, __LINE__, #cond);                            \
        }                                                                  \
    } while (0)

#define SECTION(name)  printf("  [ %s ]\n", name)

/* ============================================================================
   Helper: cast a raw int to Error to probe gap values and out-of-range codes
   without triggering -Wconversion on the enum.
   ============================================================================ */
static Error raw(int v) { return (Error)v; }

/* ============================================================================
   1. error_message() — named codes return non-NULL, non-empty strings
   ============================================================================ */
static void test_message_named_codes(void) {
    SECTION("error_message: named codes are non-NULL and non-empty");

    /* Success */
    EXPECT(error_message(ERR_OK)                != NULL);
    EXPECT(strlen(error_message(ERR_OK))         > 0);

    /* Argument / input (1-5) */
    EXPECT(error_message(ERR_INVALID_ARG)       != NULL);
    EXPECT(strlen(error_message(ERR_INVALID_ARG)) > 0);

    EXPECT(error_message(ERR_OUT_OF_RANGE)      != NULL);
    EXPECT(strlen(error_message(ERR_OUT_OF_RANGE)) > 0);

    EXPECT(error_message(ERR_PARSE_FAILED)      != NULL);
    EXPECT(strlen(error_message(ERR_PARSE_FAILED)) > 0);

    EXPECT(error_message(ERR_INVALID_FORMAT)    != NULL);
    EXPECT(strlen(error_message(ERR_INVALID_FORMAT)) > 0);

    EXPECT(error_message(ERR_INVALID_STATE)     != NULL);
    EXPECT(strlen(error_message(ERR_INVALID_STATE)) > 0);

    /* Resource / memory (100-103) */
    EXPECT(error_message(ERR_OUT_OF_MEMORY)     != NULL);
    EXPECT(strlen(error_message(ERR_OUT_OF_MEMORY)) > 0);

    EXPECT(error_message(ERR_BUFFER_TOO_SMALL)  != NULL);
    EXPECT(strlen(error_message(ERR_BUFFER_TOO_SMALL)) > 0);

    EXPECT(error_message(ERR_CAPACITY_EXCEEDED) != NULL);
    EXPECT(strlen(error_message(ERR_CAPACITY_EXCEEDED)) > 0);

    EXPECT(error_message(ERR_NOT_FOUND)         != NULL);
    EXPECT(strlen(error_message(ERR_NOT_FOUND)) > 0);

    /* I/O / system (200-204) */
    EXPECT(error_message(ERR_IO_FAILED)         != NULL);
    EXPECT(strlen(error_message(ERR_IO_FAILED)) > 0);

    EXPECT(error_message(ERR_PERMISSION)        != NULL);
    EXPECT(strlen(error_message(ERR_PERMISSION)) > 0);

    EXPECT(error_message(ERR_TIMEOUT)           != NULL);
    EXPECT(strlen(error_message(ERR_TIMEOUT))   > 0);

    EXPECT(error_message(ERR_NOT_SUPPORTED)     != NULL);
    EXPECT(strlen(error_message(ERR_NOT_SUPPORTED)) > 0);

    EXPECT(error_message(ERR_ALREADY_EXISTS)    != NULL);
    EXPECT(strlen(error_message(ERR_ALREADY_EXISTS)) > 0);

    /* Arithmetic (300-302) */
    EXPECT(error_message(ERR_OVERFLOW)          != NULL);
    EXPECT(strlen(error_message(ERR_OVERFLOW))  > 0);

    EXPECT(error_message(ERR_UNDERFLOW)         != NULL);
    EXPECT(strlen(error_message(ERR_UNDERFLOW)) > 0);

    EXPECT(error_message(ERR_DIVIDE_BY_ZERO)    != NULL);
    EXPECT(strlen(error_message(ERR_DIVIDE_BY_ZERO)) > 0);

    /* Generic (400-401) */
    EXPECT(error_message(ERR_UNKNOWN)           != NULL);
    EXPECT(strlen(error_message(ERR_UNKNOWN))   > 0);

    EXPECT(error_message(ERR_NOT_IMPLEMENTED)   != NULL);
    EXPECT(strlen(error_message(ERR_NOT_IMPLEMENTED)) > 0);
}

/* ============================================================================
   2. error_message() — ERR_COUNT returns its dedicated sentinel string
   ============================================================================ */
static void test_message_err_count(void) {
    SECTION("error_message: ERR_COUNT returns sentinel string");

    const char *msg = error_message(ERR_COUNT);
    EXPECT(msg != NULL);
    EXPECT(strlen(msg) > 0);
    /*
     * The header documents the exact string. Pin it so a future edit
     * to the message text is a conscious, visible change.
     */
    EXPECT(strcmp(msg, "Invalid error code (ERR_COUNT)") == 0);
}

/* ============================================================================
   3. error_message() — gap values (unassigned integers within a range)
      must return "Unknown error" via the default branch, never NULL.
   ============================================================================ */
static void test_message_gap_values(void) {
    SECTION("error_message: gap values return non-NULL \"Unknown error\"");

    /* Gaps in argument range: 6-99 */
    EXPECT(error_message(raw(6))   != NULL);
    EXPECT(strcmp(error_message(raw(6)),  "Unknown error") == 0);
    EXPECT(error_message(raw(50))  != NULL);
    EXPECT(strcmp(error_message(raw(50)), "Unknown error") == 0);
    EXPECT(error_message(raw(99))  != NULL);
    EXPECT(strcmp(error_message(raw(99)), "Unknown error") == 0);

    /* Gaps in resource range: 104-199 */
    EXPECT(error_message(raw(104)) != NULL);
    EXPECT(strcmp(error_message(raw(104)), "Unknown error") == 0);
    EXPECT(error_message(raw(150)) != NULL);
    EXPECT(strcmp(error_message(raw(150)), "Unknown error") == 0);
    EXPECT(error_message(raw(199)) != NULL);
    EXPECT(strcmp(error_message(raw(199)), "Unknown error") == 0);

    /* Gaps in I/O range: 205-299 */
    EXPECT(error_message(raw(205)) != NULL);
    EXPECT(strcmp(error_message(raw(205)), "Unknown error") == 0);

    /* Gaps in arithmetic range: 303-399 */
    EXPECT(error_message(raw(303)) != NULL);
    EXPECT(strcmp(error_message(raw(303)), "Unknown error") == 0);

    /* Values beyond ERR_COUNT (ERR_NOT_IMPLEMENTED=401, ERR_COUNT=402) */
    EXPECT(error_message(raw(500)) != NULL);
    EXPECT(strcmp(error_message(raw(500)), "Unknown error") == 0);
}

/* ============================================================================
   4. error_message() — values entirely outside [0, ERR_COUNT)
      The header guarantees non-NULL via the default branch.
   ============================================================================ */
static void test_message_out_of_range(void) {
    SECTION("error_message: out-of-range values return non-NULL");

    EXPECT(error_message(raw(-1))   != NULL);
    EXPECT(error_message(raw(-100)) != NULL);
    EXPECT(error_message(raw(9999)) != NULL);

    EXPECT(strcmp(error_message(raw(-1)),   "Unknown error") == 0);
    EXPECT(strcmp(error_message(raw(9999)), "Unknown error") == 0);
}

/* ============================================================================
   5. error_is_ok() — true only for ERR_OK
   ============================================================================ */
static void test_is_ok(void) {
    SECTION("error_is_ok: true only for ERR_OK");

    EXPECT( error_is_ok(ERR_OK));
    EXPECT(!error_is_ok(ERR_INVALID_ARG));
    EXPECT(!error_is_ok(ERR_OUT_OF_RANGE));
    EXPECT(!error_is_ok(ERR_PARSE_FAILED));
    EXPECT(!error_is_ok(ERR_INVALID_FORMAT));
    EXPECT(!error_is_ok(ERR_INVALID_STATE));
    EXPECT(!error_is_ok(ERR_OUT_OF_MEMORY));
    EXPECT(!error_is_ok(ERR_BUFFER_TOO_SMALL));
    EXPECT(!error_is_ok(ERR_CAPACITY_EXCEEDED));
    EXPECT(!error_is_ok(ERR_NOT_FOUND));
    EXPECT(!error_is_ok(ERR_IO_FAILED));
    EXPECT(!error_is_ok(ERR_PERMISSION));
    EXPECT(!error_is_ok(ERR_TIMEOUT));
    EXPECT(!error_is_ok(ERR_NOT_SUPPORTED));
    EXPECT(!error_is_ok(ERR_ALREADY_EXISTS));
    EXPECT(!error_is_ok(ERR_OVERFLOW));
    EXPECT(!error_is_ok(ERR_UNDERFLOW));
    EXPECT(!error_is_ok(ERR_DIVIDE_BY_ZERO));
    EXPECT(!error_is_ok(ERR_UNKNOWN));
    EXPECT(!error_is_ok(ERR_NOT_IMPLEMENTED));
    EXPECT(!error_is_ok(ERR_COUNT));

    /* Raw / out-of-range values are never OK */
    EXPECT(!error_is_ok(raw(-1)));
    EXPECT(!error_is_ok(raw(9999)));
}

/* ============================================================================
   6. error_in_range() — [0, ERR_COUNT) passes; ERR_COUNT and negatives fail.
      Gap values (unassigned integers within a range) must PASS — the header
      documents this explicitly: the function checks the integer range, not
      enum membership.
   ============================================================================ */
static void test_in_range(void) {
    SECTION("error_in_range: named codes within [0, ERR_COUNT) return true");

    EXPECT(error_in_range(ERR_OK));
    EXPECT(error_in_range(ERR_INVALID_ARG));
    EXPECT(error_in_range(ERR_OUT_OF_RANGE));
    EXPECT(error_in_range(ERR_PARSE_FAILED));
    EXPECT(error_in_range(ERR_INVALID_FORMAT));
    EXPECT(error_in_range(ERR_INVALID_STATE));
    EXPECT(error_in_range(ERR_OUT_OF_MEMORY));
    EXPECT(error_in_range(ERR_BUFFER_TOO_SMALL));
    EXPECT(error_in_range(ERR_CAPACITY_EXCEEDED));
    EXPECT(error_in_range(ERR_NOT_FOUND));
    EXPECT(error_in_range(ERR_IO_FAILED));
    EXPECT(error_in_range(ERR_PERMISSION));
    EXPECT(error_in_range(ERR_TIMEOUT));
    EXPECT(error_in_range(ERR_NOT_SUPPORTED));
    EXPECT(error_in_range(ERR_ALREADY_EXISTS));
    EXPECT(error_in_range(ERR_OVERFLOW));
    EXPECT(error_in_range(ERR_UNDERFLOW));
    EXPECT(error_in_range(ERR_DIVIDE_BY_ZERO));
    EXPECT(error_in_range(ERR_UNKNOWN));
    EXPECT(error_in_range(ERR_NOT_IMPLEMENTED));

    SECTION("error_in_range: gap values within a range still return true");

    /* Gaps are unassigned but lie inside [0, ERR_COUNT) — documented to pass */
    EXPECT(error_in_range(raw(6)));
    EXPECT(error_in_range(raw(50)));
    EXPECT(error_in_range(raw(99)));
    EXPECT(error_in_range(raw(104)));
    EXPECT(error_in_range(raw(150)));
    EXPECT(error_in_range(raw(199)));
    EXPECT(error_in_range(raw(205)));
    EXPECT(error_in_range(raw(303)));

    SECTION("error_in_range: ERR_COUNT itself returns false");

    EXPECT(!error_in_range(ERR_COUNT));

    SECTION("error_in_range: negative and large values return false");

    EXPECT(!error_in_range(raw(-1)));
    EXPECT(!error_in_range(raw(-100)));
    EXPECT(!error_in_range(raw(9999)));
}

/* ============================================================================
   7. error_code() — integer values match enum assignments
   ============================================================================ */
static void test_error_code(void) {
    SECTION("error_code: integer values match enum assignments");

    EXPECT(error_code(ERR_OK)                ==   0);

    /* Argument / input: sequential from 1 */
    EXPECT(error_code(ERR_INVALID_ARG)       ==   1);
    EXPECT(error_code(ERR_OUT_OF_RANGE)      ==   2);
    EXPECT(error_code(ERR_PARSE_FAILED)      ==   3);
    EXPECT(error_code(ERR_INVALID_FORMAT)    ==   4);
    EXPECT(error_code(ERR_INVALID_STATE)     ==   5);

    /* Resource / memory: anchored at 100 */
    EXPECT(error_code(ERR_OUT_OF_MEMORY)     == 100);
    EXPECT(error_code(ERR_BUFFER_TOO_SMALL)  == 101);
    EXPECT(error_code(ERR_CAPACITY_EXCEEDED) == 102);
    EXPECT(error_code(ERR_NOT_FOUND)         == 103);

    /* I/O / system: anchored at 200 */
    EXPECT(error_code(ERR_IO_FAILED)         == 200);
    EXPECT(error_code(ERR_PERMISSION)        == 201);
    EXPECT(error_code(ERR_TIMEOUT)           == 202);
    EXPECT(error_code(ERR_NOT_SUPPORTED)     == 203);
    EXPECT(error_code(ERR_ALREADY_EXISTS)    == 204);

    /* Arithmetic: anchored at 300 */
    EXPECT(error_code(ERR_OVERFLOW)          == 300);
    EXPECT(error_code(ERR_UNDERFLOW)         == 301);
    EXPECT(error_code(ERR_DIVIDE_BY_ZERO)    == 302);

    /* Generic: anchored at 400 */
    EXPECT(error_code(ERR_UNKNOWN)           == 400);
    EXPECT(error_code(ERR_NOT_IMPLEMENTED)   == 401);
}

/* ============================================================================
   8. Enum uniqueness — every named code has a distinct integer value.
      This guards against accidental duplicates if the enum is extended.
   ============================================================================ */
static void test_enum_uniqueness(void) {
    SECTION("enum uniqueness: all named codes are distinct");

    /*
     * The anchor values separate the ranges; check cross-range gaps
     * are preserved by verifying that no two codes share an integer value.
     * We do this by asserting the anchors themselves and checking
     * sequential codes differ by exactly 1 within each range.
     */
    EXPECT(error_code(ERR_INVALID_ARG)       != error_code(ERR_OK));
    EXPECT(error_code(ERR_OUT_OF_RANGE)      != error_code(ERR_INVALID_ARG));
    EXPECT(error_code(ERR_PARSE_FAILED)      != error_code(ERR_OUT_OF_RANGE));
    EXPECT(error_code(ERR_INVALID_FORMAT)    != error_code(ERR_PARSE_FAILED));
    EXPECT(error_code(ERR_INVALID_STATE)     != error_code(ERR_INVALID_FORMAT));

    /* Gap between argument range and resource range */
    EXPECT(error_code(ERR_OUT_OF_MEMORY)     != error_code(ERR_INVALID_STATE));

    EXPECT(error_code(ERR_BUFFER_TOO_SMALL)  != error_code(ERR_OUT_OF_MEMORY));
    EXPECT(error_code(ERR_CAPACITY_EXCEEDED) != error_code(ERR_BUFFER_TOO_SMALL));
    EXPECT(error_code(ERR_NOT_FOUND)         != error_code(ERR_CAPACITY_EXCEEDED));

    /* Gap between resource range and I/O range */
    EXPECT(error_code(ERR_IO_FAILED)         != error_code(ERR_NOT_FOUND));

    EXPECT(error_code(ERR_PERMISSION)        != error_code(ERR_IO_FAILED));
    EXPECT(error_code(ERR_TIMEOUT)           != error_code(ERR_PERMISSION));
    EXPECT(error_code(ERR_NOT_SUPPORTED)     != error_code(ERR_TIMEOUT));
    EXPECT(error_code(ERR_ALREADY_EXISTS)    != error_code(ERR_NOT_SUPPORTED));

    /* Gap between I/O range and arithmetic range */
    EXPECT(error_code(ERR_OVERFLOW)          != error_code(ERR_ALREADY_EXISTS));

    EXPECT(error_code(ERR_UNDERFLOW)         != error_code(ERR_OVERFLOW));
    EXPECT(error_code(ERR_DIVIDE_BY_ZERO)    != error_code(ERR_UNDERFLOW));

    /* Gap between arithmetic range and generic range */
    EXPECT(error_code(ERR_UNKNOWN)           != error_code(ERR_DIVIDE_BY_ZERO));

    EXPECT(error_code(ERR_NOT_IMPLEMENTED)   != error_code(ERR_UNKNOWN));
}

/* ============================================================================
   9. error_message() — message strings are consistent with error_code()
      Spot-check: the message for each anchored code contains no stale text
      (i.e. the switch was not copy-pasted incorrectly across ranges).
   ============================================================================ */
static void test_message_consistency(void) {
    SECTION("error_message: spot-check messages are not swapped across ranges");

    /*
     * We do not hardcode the full message strings beyond ERR_COUNT (already
     * pinned above), because spelling is not part of the public contract.
     * Instead we verify that anchor codes for each range do not return the
     * success or a neighbouring range's message.
     */
    EXPECT(strcmp(error_message(ERR_OK),           error_message(ERR_INVALID_ARG))   != 0);
    EXPECT(strcmp(error_message(ERR_OUT_OF_MEMORY), error_message(ERR_IO_FAILED))     != 0);
    EXPECT(strcmp(error_message(ERR_IO_FAILED),     error_message(ERR_OVERFLOW))      != 0);
    EXPECT(strcmp(error_message(ERR_OVERFLOW),      error_message(ERR_UNKNOWN))       != 0);

    /* ERR_UNKNOWN and the default branch both return "Unknown error" — that is
       intentional and documented. Do NOT assert they differ. */
    EXPECT(strcmp(error_message(ERR_UNKNOWN), "Unknown error") == 0);
    EXPECT(strcmp(error_message(raw(6)),      "Unknown error") == 0);
}

/* ============================================================================
   Entry point
   ============================================================================ */
int main(void) {
    printf("error_test\n");

    test_message_named_codes();
    test_message_err_count();
    test_message_gap_values();
    test_message_out_of_range();
    test_is_ok();
    test_in_range();
    test_error_code();
    test_enum_uniqueness();
    test_message_consistency();

    printf("\n  %d / %d passed\n", g_total - g_failed, g_total);

    if (g_failed > 0) {
        fprintf(stderr, "  %d test(s) FAILED\n", g_failed);
        return 1;
    }

    return 0;
}
