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
 * misra_adapter.h -- integrations/autosar/canon_autosar.h as the misra job
 * (.github/workflows/analysis.yml) reads it: with the ModuleId the adapter
 * requires, and the stand-in AUTOSAR headers beside this file on the
 * include path. The job reports no finding in this directory, which holds
 * test scaffolding, not the library.
 */

#ifndef CANON_TEST_INTEGRATIONS_AUTOSAR_MISRA_ADAPTER_H
#define CANON_TEST_INTEGRATIONS_AUTOSAR_MISRA_ADAPTER_H

#define CANON_AUTOSAR_MODULE_ID 2048u

#include "canon_autosar.h"

#endif /* CANON_TEST_INTEGRATIONS_AUTOSAR_MISRA_ADAPTER_H */
