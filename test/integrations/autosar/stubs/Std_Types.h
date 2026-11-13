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
 * Std_Types.h -- a stand-in for AUTOSAR's, for Canon-C's tests of
 * integrations/autosar/canon_autosar.h. Not an AUTOSAR implementation:
 * the definitions the adapter uses, as AUTOSAR CP SWS StandardTypes gives
 * them.
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include "Platform_Types.h"

typedef uint8 Std_ReturnType;        /* SWS_Std_00005 */

#ifndef STATUSTYPEDEFINED            /* SWS_Std_00006 */
    #define STATUSTYPEDEFINED
    #define E_OK 0x00u
    typedef unsigned char StatusType;
#endif
#define E_NOT_OK 0x01u

#define STD_ON  0x01u                /* SWS_Std_00010 */
#define STD_OFF 0x00u

#endif /* STD_TYPES_H */
