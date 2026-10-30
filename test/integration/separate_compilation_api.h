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

/* The shared header of separate_compilation_test (VERIFY-049 S1, S2): the
 * declaration side of the documented separate-compilation path, written as
 * the headers' examples now show it. Included by the test, which only calls,
 * and by nothing else; separate_compilation_defs.c holds the definitions. */
#ifndef CANON_TEST_SEPARATE_COMPILATION_API_H
#define CANON_TEST_SEPARATE_COMPILATION_API_H

#include "semantics/option/option_decl.h"
#include "data/vec/vec_decl.h"
#include "data/deque/deque_decl.h"
#include "data/stack.h"
#include "data/queue.h"

/* A struct element type, as the examples use. */
typedef struct { int id; } SepTask;

DECLARE_OPTION_ALL(extern, SepTask)
DECLARE_VEC(SepTask)
DECLARE_STACK(SepTask)
DECLARE_DEQUE(SepTask)
DECLARE_QUEUE(SepTask)

#endif /* CANON_TEST_SEPARATE_COMPILATION_API_H */
