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
 * wrappers write *out, which their default assigns clause omits.
 *
 * arena_try_alloc and arena_try_alloc_aligned declare "assigns *arena;" for
 * the default behaviour and "assigns *arena, *out;" only inside the
 * non_null_out behaviour. ACSL's default-behaviour assigns holds on every
 * call: every location outside *arena keeps its value. Both functions store
 * the allocation into *out whenever out is non-NULL, so the default clause
 * is false for the ordinary call below, where out is the caller's variable.
 *
 * Build from the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       tools/probes/verify-039/p_try_alloc_assigns.c -o p && ./p
 * Add -fsanitize=address,undefined -fno-sanitize-recover=all to confirm the
 * calls are defined C. Expected: for both functions the caller's pointer
 * changes although it lies outside *arena, so the default clause is
 * VIOLATED; exit 1.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "core/arena.h"

static _Alignas(64) unsigned char buf[256];

static int report(const char* fn, const Arena* a, void* const* out,
                  const void* before, const void* after, bool ret) {
    int inside = ((const void*)out >= (const void*)a) &&
                 ((const void*)out < (const void*)(a + 1));
    int changed = (before != after);
    printf("%s: returned %s; *out %p -> %p; out inside *arena: %s\n",
           fn, ret ? "true" : "false", before, after, inside ? "yes" : "no");
    printf("  default behaviour, assigns *arena: %s\n",
           (changed && !inside) ? "VIOLATED (a location outside *arena changed)"
                                : "holds");
    return changed && !inside;
}

int main(void) {
    Arena a;
    void* p = NULL;
    bool ret;
    int bad = 0;

    arena_init(&a, buf, sizeof buf);
    ret = arena_try_alloc(&a, 8, &p);
    bad += report("arena_try_alloc", &a, &p, NULL, p, ret);

    p = NULL;
    arena_init(&a, buf, sizeof buf);
    ret = arena_try_alloc_aligned(&a, 8, 16, &p);
    bad += report("arena_try_alloc_aligned", &a, &p, NULL, p, ret);

    return bad ? 1 : 0;
}
