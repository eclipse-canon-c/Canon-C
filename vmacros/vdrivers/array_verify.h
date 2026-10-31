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

/* vmacros/vdrivers/array_verify.h                                [VERIFY-052]
 *
 * WP driver for data/array.h, at (int, 8). array.h's contracts live in the
 * macro body, so the unit is preprocessed with -CC (VERIFY-025). The prefix
 * defines what DEFINE_ARRAY needs: option at int, as option_cc_experiment.h
 * does, and DEFINE_SLICE(int) from core/slice.h, whose macro-body contracts
 * are slice_cc_experiment.h's at another type. The array's 20 functions
 * follow. No clause is added here: the array's contracts are type-free and
 * live in array.h. They name the element count as
 * sizeof(x.items) / sizeof(x.items[0]), since ACSL in a macro body cannot
 * name the macro's parameter N.
 */
#ifndef CANON_VDRIVER_ARRAY_VERIFY_H
#define CANON_VDRIVER_ARRAY_VERIFY_H

#include "semantics/option/option_defn.h"
DEFINE_OPTION_STRUCT(int)
DEFINE_OPTION_FUNCTIONS(static inline, int)

#include "core/slice.h"
DEFINE_SLICE(int)

#include "data/array.h"
DEFINE_ARRAY(int, 8)

#endif /* CANON_VDRIVER_ARRAY_VERIFY_H */
