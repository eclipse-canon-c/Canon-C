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

/* VERIFY-039 G2 -- docs/deviations.md, VERIFY-009 Category 2c: the try
 * wrappers write *out, which their default assigns clause omitted.
 *
 * Before G2, arena_try_alloc and arena_try_alloc_aligned declared
 * "assigns *arena;" for the default behaviour and listed *out only inside the
 * non_null_out behaviour. ACSL's default-behaviour assigns holds on every
 * call: every location outside the listed ones keeps its value. Both
 * functions store the allocation into *out whenever out is non-NULL, so that
 * clause was false for the ordinary call below, where out is the caller's
 * variable. G2 lists *out in the default clause as well.
 *
 * The probe watches three locations outside *arena: the caller's pointer
 * (which out points to), a second pointer next to it, and every byte of the
 * arena's buffer. It reports each clause against what changed.
 *
 * Build from the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       tools/probes/verify-039/p_try_alloc_assigns.c -o p && ./p
 * Add -fsanitize=address,undefined -fno-sanitize-recover=all to confirm the
 * calls are defined C. Expected, for both functions: only the caller's
 * pointer changes, so the pre-G2 clause is VIOLATED and the current clause
 * holds on every watched location. Exit 0 when the current clause holds in
 * both cases (a regression test since G2's fix).
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <string.h>
#include "core/arena.h"

static _Alignas(64) unsigned char buf[256];

static int buf_untouched(void) {
    usize i;
    for (i = 0; i < sizeof buf; i++) {
        if (buf[i] != 0xABu) { return 0; }
    }
    return 1;
}

/* Returns 1 if the current clause (assigns *arena, *out) is violated. */
static int check(const char* fn, bool ret, const void* p, const void* q,
                 const void* q_before) {
    int out_changed = (p != NULL);     /* *out started as NULL */
    int others_kept = (q == q_before) && buf_untouched();
    printf("%s: returned %s; *out NULL -> %p; neighbour %s; buffer %s\n",
           fn, ret ? "true" : "false", p,
           (q == q_before) ? "unchanged" : "CHANGED",
           buf_untouched() ? "unchanged" : "CHANGED");
    printf("  pre-G2  default assigns *arena:       %s\n",
           out_changed ? "VIOLATED (*out lies outside *arena and changed)" : "holds");
    printf("  current default assigns *arena, *out: %s\n",
           others_kept ? "holds" : "VIOLATED (a location outside *arena and *out changed)");
    return !others_kept;
}

int main(void) {
    Arena a;
    struct { void* p; void* q; } caller;   /* out, and a neighbour of *out */
    bool ret;
    int bad = 0;

    memset(buf, 0xAB, sizeof buf);
    arena_init(&a, buf, sizeof buf);
    caller.p = NULL; caller.q = (void*)&a;
    ret = arena_try_alloc(&a, 8, &caller.p);
    bad += check("arena_try_alloc", ret, caller.p, caller.q, (void*)&a);

    memset(buf, 0xAB, sizeof buf);
    arena_init(&a, buf, sizeof buf);
    caller.p = NULL; caller.q = (void*)&a;
    ret = arena_try_alloc_aligned(&a, 8, 16, &caller.p);
    bad += check("arena_try_alloc_aligned", ret, caller.p, caller.q, (void*)&a);

    return bad ? 1 : 0;
}
