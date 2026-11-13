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

#ifndef CANON_INTEGRATIONS_AUTOSAR_CANON_AUTOSAR_H
#define CANON_INTEGRATIONS_AUTOSAR_CANON_AUTOSAR_H

/*
 *
 * Canon-C -- integrations/autosar/canon_autosar.h
 * Canon-C in an AUTOSAR Classic build: its types beside the platform's,
 * its Error and bool beside Std_ReturnType and boolean, and its failed
 * contract checks reported to the Default Error Tracer (DET).
 *
 * Opt-in: no Canon-C header includes this one. It needs the project's
 * AUTOSAR headers on the include path: Std_Types.h, which brings
 * Platform_Types.h, and Det.h. See docs/autosar-classic.md.
 *
 * -- Configuration -------------------------------------------------------------
 * Define these before including this header, the same in every translation
 * unit that includes it:
 *
 *   CANON_AUTOSAR_MODULE_ID      Required. The ModuleId of Canon-C's DET
 *                                reports. AUTOSAR reserves 0..254 for basic
 *                                software modules; complex drivers use 255
 *                                or 2048..4095 (SWS DefaultErrorTracer,
 *                                8.2.1.1).
 *   CANON_AUTOSAR_INSTANCE_ID    The InstanceId. Default 0.
 *   CANON_AUTOSAR_API_ID         The ApiId. Default 0: one for every Canon-C
 *                                function, since the handler is not told
 *                                which function's check failed.
 *   CANON_AUTOSAR_E_CONTRACT     The ErrorId. Default 1.
 *   CANON_AUTOSAR_NO_64BIT_TYPES Skips the check of uint64 and sint64, for a
 *                                Platform_Types.h older than AUTOSAR 4.1.2,
 *                                which added them.
 *
 * -- What the integrator supplies ----------------------------------------------
 *
 *   void canon_autosar_safe_state(void);
 *
 * It puts the ECU in its safe state, as the project defines it (a reset, a
 * shutdown through the ECU State Manager), and does not return. A program
 * that installs the handler below does not link without it.
 *
 * -- Failed contract checks -----------------------------------------------------
 * canon_autosar_init() installs canon_autosar_contract_handler as Canon-C's
 * contract handler; call it before any other Canon-C call. Until then a
 * failed check reaches the default handler, which writes to stderr and
 * calls abort(). The handler reports the failure with Det_ReportError,
 * AUTOSAR's call for development errors, then calls
 * canon_autosar_safe_state(). AUTOSAR's DET stops execution in
 * Det_ReportError (SWS_Det_00026 in R24-11); a DET that returns reaches the
 * safe state. If canon_autosar_safe_state() returns, the handler loops
 * forever, since a contract handler must not return. The file, line,
 * function and expression Canon-C passes are dropped: Det_ReportError has
 * no place for them.
 *
 * Canon-C's checks are on unless CANON_NO_REQUIRE is defined
 * (core/primitives/contract.h), whatever the project's DevErrorDetect says.
 *
 * -- Types ----------------------------------------------------------------------
 * The checks below fail the build where a platform type's width or
 * signedness differs from its Canon-C counterpart's. They check values,
 * not types: uint32 and u32 can be different C types of the same width
 * (unsigned long and unsigned int), so convert values between them, not
 * pointers.
 */

#include "Std_Types.h"
#include "Det.h"

#include "core/primitives/types.h"     /* u8 ... i64, f32, f64, bool */
#include "core/primitives/contract.h"  /* contract_set_handler, static_require */
#include "semantics/error.h"           /* Error, ERR_OK */

#ifndef CANON_AUTOSAR_MODULE_ID
    #error "canon_autosar.h: define CANON_AUTOSAR_MODULE_ID, the ModuleId of Canon-C's DET reports"
#endif
#ifndef CANON_AUTOSAR_INSTANCE_ID
    #define CANON_AUTOSAR_INSTANCE_ID 0u
#endif
#ifndef CANON_AUTOSAR_API_ID
    #define CANON_AUTOSAR_API_ID 0u
#endif
#ifndef CANON_AUTOSAR_E_CONTRACT
    #define CANON_AUTOSAR_E_CONTRACT 1u
#endif

/* ============================================================================
   THE PLATFORM'S TYPES AND CANON-C'S: THE SAME WIDTHS AND SIGNEDNESS
   ============================================================================ */

/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(uint8)  == sizeof(u8))  && ((uint8)-1  > (uint8)0),  canon_autosar_uint8_is_u8);
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(uint16) == sizeof(u16)) && ((uint16)-1 > (uint16)0), canon_autosar_uint16_is_u16);
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(uint32) == sizeof(u32)) && ((uint32)-1 > (uint32)0), canon_autosar_uint32_is_u32);
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(sint8)  == sizeof(i8))  && ((sint8)-1  < (sint8)0),  canon_autosar_sint8_is_i8);
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(sint16) == sizeof(i16)) && ((sint16)-1 < (sint16)0), canon_autosar_sint16_is_i16);
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(sint32) == sizeof(i32)) && ((sint32)-1 < (sint32)0), canon_autosar_sint32_is_i32);
#ifndef CANON_AUTOSAR_NO_64BIT_TYPES
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(uint64) == sizeof(u64)) && ((uint64)-1 > (uint64)0), canon_autosar_uint64_is_u64);
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require((sizeof(sint64) == sizeof(i64)) && ((sint64)-1 < (sint64)0), canon_autosar_sint64_is_i64);
#endif
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require(sizeof(float32) == sizeof(f32), canon_autosar_float32_is_f32);
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
static_require(sizeof(float64) == sizeof(f64), canon_autosar_float64_is_f64);

/* ============================================================================
   ERROR AND BOOL, STD_RETURNTYPE AND BOOLEAN
   ============================================================================ */

/**
 * @brief Std_ReturnType for an Error: E_OK for ERR_OK, E_NOT_OK otherwise.
 *
 * @param e  Error code.
 * @return   E_OK if e is ERR_OK, E_NOT_OK for any other value.
 */
static inline Std_ReturnType canon_autosar_std_return(Error e) {
    return (e == ERR_OK) ? (Std_ReturnType)E_OK : (Std_ReturnType)E_NOT_OK;
}

/**
 * @brief AUTOSAR boolean for a bool: TRUE or FALSE.
 *
 * Correct whether the platform's boolean is an unsigned integer, with TRUE
 * 1 and FALSE 0 (before AUTOSAR R22-11), or C99's _Bool, with TRUE true and
 * FALSE false (since R22-11).
 */
static inline boolean canon_autosar_boolean(bool b) {
    return b ? (boolean)TRUE : (boolean)FALSE;
}

/**
 * @brief bool for an AUTOSAR boolean: true for any value but FALSE.
 */
static inline bool canon_autosar_bool(boolean b) {
    return b != (boolean)FALSE;
}

/* ============================================================================
   FAILED CONTRACT CHECKS: DET, THEN THE SAFE STATE
   ============================================================================ */

/**
 * @brief The integrator's: puts the ECU in its safe state. Must not return.
 *
 * Declared here, defined by the project.
 */
void canon_autosar_safe_state(void);

/**
 * @brief Canon-C's contract handler for an AUTOSAR build.
 *
 * Reports the failure with Det_ReportError (CANON_AUTOSAR_MODULE_ID,
 * CANON_AUTOSAR_INSTANCE_ID, CANON_AUTOSAR_API_ID,
 * CANON_AUTOSAR_E_CONTRACT), then calls canon_autosar_safe_state(). Does
 * not return. The arguments, Canon-C's description of the failure, are not
 * passed on.
 */
static inline void canon_autosar_contract_handler(
    const char* file,
    int         line,
    const char* func,
    const char* expr,
    const char* msg
) {
    (void)file;
    (void)line;
    (void)func;
    (void)expr;
    (void)msg;
    (void)Det_ReportError((uint16)CANON_AUTOSAR_MODULE_ID,
                          (uint8)CANON_AUTOSAR_INSTANCE_ID,
                          (uint8)CANON_AUTOSAR_API_ID,
                          (uint8)CANON_AUTOSAR_E_CONTRACT);
    canon_autosar_safe_state();
    for (;;) {
        /* canon_autosar_safe_state() returned: stop here. */
    }
}

/**
 * @brief Installs canon_autosar_contract_handler as Canon-C's handler.
 *
 * Call once, before any other Canon-C call and before other tasks run:
 * contract_set_handler() is not thread-safe.
 */
static inline void canon_autosar_init(void) {
    contract_set_handler(canon_autosar_contract_handler);
}

#endif /* CANON_INTEGRATIONS_AUTOSAR_CANON_AUTOSAR_H */
