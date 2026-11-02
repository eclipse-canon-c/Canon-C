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

/* The option and vec instantiations hashmap_test.c and hashmap_multi_test.c
 * need for hashmap_range.h (VERIFY-054). In a header so that the static inline
 * functions the test does not call are not reported unused (clang reports
 * them only in the main file). */
#ifndef CANON_TEST_SUPPORT_HASHMAP_VECS_H
#define CANON_TEST_SUPPORT_HASHMAP_VECS_H

#include "core/primitives/types.h"
#include "semantics/option/option.h"
#include "data/vec/vec.h"

CANON_OPTION(u64)
CANON_OPTION(int)
CANON_OPTION(u32)
DEFINE_VEC(static inline, u64)
DEFINE_VEC(static inline, int)
DEFINE_VEC(static inline, u32)

#endif /* CANON_TEST_SUPPORT_HASHMAP_VECS_H */
