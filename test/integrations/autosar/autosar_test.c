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

/*
 * autosar_test.c -- integrations/autosar/canon_autosar.h, with the
 * stand-in AUTOSAR headers in test/integrations/autosar/stubs/.
 *
 * Checks, in this order:
 *   - the conversions: canon_autosar_std_return for ERR_OK and every other
 *     code semantics/error.h names; canon_autosar_boolean for true and
 *     false; canon_autosar_bool for FALSE, TRUE and every other value of an
 *     unsigned char;
 *   - canon_autosar_init installs canon_autosar_contract_handler;
 *   - a failed require() reaches it, which reports to DET once, with the
 *     configured ModuleId, InstanceId, ApiId and ErrorId, then calls
 *     canon_autosar_safe_state(). The test's DET returns, as a DET that does
 *     not stop execution would; its safe state checks the report and ends
 *     the program, the handler's last step. If the handler returned
 *     instead, main() reports it and fails.
 *
 * Built twice (CMakeLists.txt): autosar_test, with boolean as unsigned char
 * (AUTOSAR 4.3.1) and every ID configured; autosar_bool_test, with boolean
 * as _Bool (R22-11) and the IDs left at their defaults
 * (CANON_AUTOSAR_STUB_BOOLEAN_BOOL, CANON_AUTOSAR_TEST_DEFAULTS).
 *
 * Under CANON_NO_REQUIRE (the coverage build) require() compiles to
 * nothing, so the test calls the installed handler itself.
 */

#define CANON_CONTRACT_IMPL

#define CANON_AUTOSAR_MODULE_ID 2048u   /* complex drivers: 255 or 2048..4095 */
#if defined(CANON_AUTOSAR_TEST_DEFAULTS)
    #define WANT_INSTANCE_ID 0u
    #define WANT_API_ID      0u
    #define WANT_ERROR_ID    1u
#else
    #define CANON_AUTOSAR_INSTANCE_ID 3u
    #define CANON_AUTOSAR_API_ID      0x10u
    #define CANON_AUTOSAR_E_CONTRACT  0x02u
    #define WANT_INSTANCE_ID 3u
    #define WANT_API_ID      0x10u
    #define WANT_ERROR_ID    0x02u
#endif

#include "canon_autosar.h"

#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
 * Minimal test harness
 * ====================================================================== */

static int g_checks   = 0;
static int g_failures = 0;

#define EXPECT(cond) \
    do { \
        g_checks++; \
        if (!(cond)) { \
            g_failures++; \
            (void)printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

/* =========================================================================
 * The test's DET and safe state
 * ====================================================================== */

static unsigned g_det_calls = 0u;
static uint16   g_det_module = 0u;
static uint8    g_det_instance = 0u;
static uint8    g_det_api = 0u;
static uint8    g_det_error = 0u;

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                               uint8 ApiId, uint8 ErrorId) {
    g_det_calls++;
    g_det_module   = ModuleId;
    g_det_instance = InstanceId;
    g_det_api      = ApiId;
    g_det_error    = ErrorId;
    return E_OK;
}

void canon_autosar_safe_state(void) {
    EXPECT(g_det_calls == 1u);
    EXPECT(g_det_module == (uint16)2048u);
    EXPECT(g_det_instance == (uint8)WANT_INSTANCE_ID);
    EXPECT(g_det_api == (uint8)WANT_API_ID);
    EXPECT(g_det_error == (uint8)WANT_ERROR_ID);
    (void)printf("autosar_test: %d checks, %d failed\n", g_checks, g_failures);
    exit((g_failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* =========================================================================
 * Conversions
 * ====================================================================== */

static void test_conversions(void) {
    /* Every code semantics/error.h names, but ERR_OK and the sentinel. */
    static const Error failures[] = {
        ERR_INVALID_ARG, ERR_OUT_OF_RANGE, ERR_PARSE_FAILED,
        ERR_INVALID_FORMAT, ERR_INVALID_STATE,
        ERR_OUT_OF_MEMORY, ERR_BUFFER_TOO_SMALL, ERR_CAPACITY_EXCEEDED,
        ERR_NOT_FOUND,
        ERR_IO_FAILED, ERR_PERMISSION, ERR_TIMEOUT, ERR_NOT_SUPPORTED,
        ERR_ALREADY_EXISTS,
        ERR_OVERFLOW, ERR_UNDERFLOW, ERR_DIVIDE_BY_ZERO,
        ERR_UNKNOWN, ERR_NOT_IMPLEMENTED
    };
    size_t i;
    unsigned v;
    unsigned wrong = 0u;

    EXPECT(canon_autosar_std_return(ERR_OK) == (Std_ReturnType)E_OK);
    for (i = 0u; i < sizeof failures / sizeof failures[0]; i++) {
        EXPECT(canon_autosar_std_return(failures[i]) == (Std_ReturnType)E_NOT_OK);
    }

    EXPECT(canon_autosar_boolean(true) == (boolean)TRUE);
    EXPECT(canon_autosar_boolean(false) == (boolean)FALSE);
    EXPECT(canon_autosar_bool((boolean)FALSE) == false);
    EXPECT(canon_autosar_bool((boolean)TRUE) == true);
    /* Every value but FALSE is true: 1 to 255 where boolean is unsigned
       char; where it is _Bool, each of them converts to TRUE. */
    for (v = 1u; v <= 255u; v++) {
        if (!canon_autosar_bool((boolean)v)) {
            wrong++;
        }
    }
    EXPECT(wrong == 0u);
    EXPECT(canon_autosar_bool(canon_autosar_boolean(true)) == true);
    EXPECT(canon_autosar_bool(canon_autosar_boolean(false)) == false);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void) {
    volatile int zero = 0;

    test_conversions();

    canon_autosar_init();
    EXPECT(canon_contract_handler == canon_autosar_contract_handler);
    EXPECT(g_det_calls == 0u);

#if !defined(CANON_NO_REQUIRE)
    require(zero == 1);
#else
    canon_contract_handler(__FILE__, __LINE__, __func__, "zero == 1", NULL);
#endif

    /* Reached only if the handler returned. */
    (void)printf("  FAIL the contract handler returned (zero is %d)\n", (int)zero);
    (void)printf("autosar_test: %d checks, %d failed\n", g_checks, g_failures + 1);
    return EXIT_FAILURE;
}
