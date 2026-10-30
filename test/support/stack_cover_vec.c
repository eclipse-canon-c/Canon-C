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

/* The definitions stack_cover.c links against (MCDC-019): option and vec at
 * int, external linkage. They live here, under test/, so that their
 * macro-body conditions, stamped to this file, are filtered out of the
 * coverage table, which reports vec at vmacros/coverage/vec_cover.c. The
 * stack_cover.c row then holds stack.h's conditions alone. */
#include "semantics/option/option_defn.h"
#include "data/vec/vec_defn.h"

DEFINE_OPTION_ALL(, int)
DEFINE_VEC(, int)
