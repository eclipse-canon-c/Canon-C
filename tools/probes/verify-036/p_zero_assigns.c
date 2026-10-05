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

/* VERIFY-036 F5 -- docs/deviations.md, VERIFY-009 Category 2c and VERIFY-010
 * Category 2d: the zero wrappers write bytes their assigns clauses omit.
 *
 * arena_alloc_zero declares "assigns *arena;" and pool_alloc_zero declares
 * "assigns pool->used;", but both zero the returned bytes, which live in the
 * caller's buffer. ACSL assigns: every unlisted location keeps its value, so
 * each clause is false whenever those bytes were not already zero.
 *
 * Build from the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. tools/probes/verify-036/p_zero_assigns.c -o p && ./p
 * Expected: both lines report a byte that was 0xFF and is now 0x00, at a
 * location outside the declared assigns set.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <string.h>
#include "core/pool.h"

static _Alignas(64) unsigned char buf[256];

int main(void) {
    Arena a;
    Pool p;
    unsigned char* r;
    unsigned char* s;

    memset(buf, 0xFF, sizeof buf);
    arena_init(&a, buf, sizeof buf);
    r = arena_alloc_zero(&a, 8);
    printf("arena_alloc_zero: buf[%td] was 0xFF, now 0x%02X; inside *arena: %s\n",
           r - buf, r[0], ((void*)r >= (void*)&a && (void*)r < (void*)(&a + 1)) ? "yes" : "no");

    memset(buf, 0xFF, sizeof buf);
    arena_init(&a, buf, sizeof buf);
    if (!pool_init(&p, &a, 16, 4)) { puts("pool_init failed"); return 1; }
    memset(buf, 0xFF, sizeof buf);
    s = pool_alloc_zero(&p);
    printf("pool_alloc_zero:  buf[%td] was 0xFF, now 0x%02X; is pool->used: %s\n",
           s - buf, s[0], ((void*)s == (void*)&p.used) ? "yes" : "no");
    return 0;
}
