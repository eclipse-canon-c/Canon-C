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

/* VERIFY-039 G9 -- docs/deviations.md: append_array's src over the vec.
 *
 * append_array requires of src that its count elements be readable and
 * separated from the buffer; nothing keeps them off *v. Its ensures 4,
 * \forall k < count: v->items[\old(v->len) + k] == src[k], reads src after
 * the store to len. On ILP32 usize is unsigned int, so an int read of len
 * is a counterpart read (C11 6.5p7), and a src over len makes a defined
 * call that falsifies ensures 4. On LP64 usize is unsigned long, an int
 * read of its bytes is undefined, and the probe says "not applicable".
 *
 * The vec and its buffer are separate heap blocks, so the vec lies outside
 * its buffer as the contract requires; len 0, capacity 4, and
 * append_array(v, (const int *)&v->len, 1). The copy puts len's bytes, 0,
 * in items[0], which takes len's effective type (6.5p6) and is read back
 * through int, a counterpart; len becomes 1; ensures 4 compares 0 with 1.
 * Every precondition holds, and ensures 1 and 2 hold.
 *
 * From the repository root:
 *   gcc -m32 -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_vec_append_src_len.c -o p && ./p
 * Expected on ILP32: ensures 1 and 2 hold, ensures 4 VIOLATED; exit 1,
 * sanitizers clean, both build configurations (the same without the two
 * defines). On LP64: "not applicable", exit 0. In the default build
 * mem_copy's require_msg runs mem_regions_overlap, which compares the two
 * pointers with < across objects, undefined in C (6.5.8p5); it returns
 * false here. The proof configuration has no such check.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <stdlib.h>
#include "vmacros/vdrivers/vec_cc_experiment.h"

static int say(const char* what, int holds) {
    printf("    %-62s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    vec_int* v;
    int* items;
    const int* src;
    result__Bool_Error r;
    int lhs, rhs, bad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    /* The int read of len is a counterpart read only if usize is unsigned
     * int itself; gcc and clang tell the types apart. */
    if (!__builtin_types_compatible_p(usize, unsigned int)) {
        printf("usize is not unsigned int (%zu bytes): an int read of len is "
               "undefined here; not applicable\n", sizeof(usize));
        return 0;
    }
    v = malloc(sizeof *v);
    items = malloc(4u * sizeof(int));
    if (v == NULL || items == NULL) { free(v); free(items); return 2; }
    v->items = items;
    v->len = 0u;
    v->capacity = 4u;
    items[0] = 42;
    src = (const int*)(const void*)&v->len;

    printf("append_array(v, (const int *)&v->len, 1), len 0, capacity 4:\n");
    r = vec_int_append_array(v, src, 1u);
    lhs = v->items[0];   /* v->items[\old(v->len) + 0] */
    rhs = src[0];        /* len, read through int      */
    printf("      len %u; items[0] %d; src[0] %d\n", (unsigned)v->len, lhs, rhs);
    bad += say("ensures 1: the result is ok", r.is_ok);
    bad += say("ensures 2: v->len == \\old(v->len) + count", v->len == 1u);
    bad += say("ensures 4: items[\\old(len) + k] == src[k] for k < count", lhs == rhs);
    free(items);
    free(v);
    return bad ? 1 : 0;
}
