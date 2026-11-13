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
 * Platform_Types.h -- a stand-in for AUTOSAR's, for Canon-C's tests of
 * integrations/autosar/canon_autosar.h. Not an AUTOSAR implementation: it
 * has the types the adapter checks and uses, with the widths and
 * signedness AUTOSAR CP SWS PlatformTypes gives them, built on <stdint.h>.
 *
 * boolean comes in the two forms AUTOSAR has given it:
 * - by default, unsigned char with TRUE 1 and FALSE 0, as in R4.3.1
 *   (SWS_Platform_00026, SWS_Platform_00027 and its annex,
 *   SWS_Platform_00056);
 * - with CANON_AUTOSAR_STUB_BOOLEAN_BOOL defined, C99's _Bool with TRUE
 *   true and FALSE false, as since R22-11 (SWS_Platform_00026,
 *   SWS_Platform_00027, SWS_Platform_00056).
 */

#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>

typedef uint8_t  uint8;    /* SWS_Platform_00013 */
typedef uint16_t uint16;   /* SWS_Platform_00014 */
typedef uint32_t uint32;   /* SWS_Platform_00015 */
typedef uint64_t uint64;   /* SWS_Platform_00066 */
typedef int8_t   sint8;    /* SWS_Platform_00016 */
typedef int16_t  sint16;   /* SWS_Platform_00017 */
typedef int32_t  sint32;   /* SWS_Platform_00018 */
typedef int64_t  sint64;   /* SWS_Platform_00067 */
typedef float    float32;  /* SWS_Platform_00041 */
typedef double   float64;  /* SWS_Platform_00042 */

#if defined(CANON_AUTOSAR_STUB_BOOLEAN_BOOL)
    #include <stdbool.h>
    typedef _Bool boolean;
    #ifndef TRUE
        #define TRUE true
    #endif
    #ifndef FALSE
        #define FALSE false
    #endif
#else
    typedef unsigned char boolean;
    #ifndef TRUE
        #define TRUE 1
    #endif
    #ifndef FALSE
        #define FALSE 0
    #endif
#endif

#endif /* PLATFORM_TYPES_H */
