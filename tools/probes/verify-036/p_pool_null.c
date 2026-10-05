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

/* VERIFY-036 F2 -- docs/deviations.md, VERIFY-010 Category 2b -- the residual
 * typed_cast_pool_alloc_assert_rte_mem_access (paper section 3.3).
 *
 * pool_alloc's contract admits pool == \null (behavior null_pool: ensures
 * \result == \null). The body's only null check is require_msg, which
 * -DCANON_NO_REQUIRE -- every WP job's configuration -- compiles to ((void)0).
 * The first memory access is then pool->used. The slot itself is never
 * dereferenced in pool_alloc.
 *
 * Build from the repository root:
 *   gcc -std=c99 -DCANON_NO_REQUIRE -DNDEBUG -I. tools/probes/verify-036/p_pool_null.c -o p && ./p
 * Expected: segmentation fault (default build: contract-violation abort).
 * Neither configuration returns NULL as the null_pool behaviour promises.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "core/pool.h"

int main(void) {
    printf("pool_alloc(NULL): contract promises NULL\n");
    fflush(stdout);
    void* r = pool_alloc(NULL);
    printf("returned %s\n", r ? "non-NULL" : "NULL");
    return 0;
}
