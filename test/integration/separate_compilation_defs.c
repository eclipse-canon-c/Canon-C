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

/* The definition side of separate_compilation_test (VERIFY-049): one
 * translation unit, external linkage, as the headers' examples show. It
 * does not include the shared header: definitions and declarations meet
 * only at link time, which is what the test checks. */
#include "semantics/option/option_defn.h"
#include "data/vec/vec_defn.h"
#include "data/deque/deque_defn.h"
#include "data/stack.h"
#include "data/queue.h"

typedef struct { int id; } SepTask;

DEFINE_OPTION_ALL(, SepTask)   /* external: the header declares it extern */
DEFINE_VEC(, SepTask)
DEFINE_STACK(, SepTask)
DEFINE_DEQUE(, SepTask)
DEFINE_QUEUE(, SepTask)
