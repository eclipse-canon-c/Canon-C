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
 * Det.h -- a stand-in for AUTOSAR's Default Error Tracer interface, for
 * Canon-C's tests of integrations/autosar/canon_autosar.h. Not an AUTOSAR
 * implementation: Det_ReportError as AUTOSAR CP SWS DefaultErrorTracer
 * declares it (SWS_Det_00009). The test that includes it defines it.
 */

#ifndef DET_H
#define DET_H

#include "Std_Types.h"

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                               uint8 ApiId, uint8 ErrorId);

#endif /* DET_H */
