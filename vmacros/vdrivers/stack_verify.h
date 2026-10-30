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

/* vmacros/vdrivers/stack_verify.h                                [VERIFY-049]
 *
 * WP driver for data/stack.h, at int. stack.h's contracts live in the macro
 * body, so the unit is preprocessed with -CC (VERIFY-025), as cc-vec is.
 * The prefix is vec_cc_experiment.h's, verbatim: option at int, result at
 * (Bool, Error), vec at int with its own macro-body contracts. Every vec
 * function a stack function calls is therefore defined, contracted and
 * proved in this unit too; the stack's 14 functions follow. No clause is
 * added here: the stack's contracts are type-free and live in stack.h.
 */
#ifndef CANON_VDRIVER_STACK_VERIFY_H
#define CANON_VDRIVER_STACK_VERIFY_H

#include "semantics/option/option_defn.h"
DEFINE_OPTION_STRUCT(int)
DEFINE_OPTION_FUNCTIONS(static inline, int)

#include "semantics/error.h"
#include "semantics/result/result_defn.h"
DEFINE_RESULT_TYPEDEF(bool, Error)
DEFINE_RESULT_STRUCT(bool, Error)
DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)
#define CANON_RESULT_BOOL_ERROR_DEFINED

#include "data/vec/vec_defn.h"
DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_FUNCTIONS(static inline, int)

#include "data/stack.h"
DEFINE_STACK(static inline, int)

#endif /* CANON_VDRIVER_STACK_VERIFY_H */
