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

/* VERIFY-039 G1 -- docs/deviations.md, the proved-goal argument, group E:
 * a goal G1 missed.
 *
 * remove copies the element into *out and then shifts the elements after
 * it down by one with mem_move, a write through `char *`. Its ensures 2,
 * *out == \old(v->items[i]), is proved in frama-c-vec and cc-vec: WP's
 * typed model keeps the int *out apart from the bytes mem_move writes, so
 * it takes *out to be unchanged by the move. Until G1's repair (40cccd9)
 * remove required of out only that it be valid, so out could lie inside
 * the buffer. Where it lies in the move's destination, items[i .. len - 2],
 * the move overwrites the copied element, and ensures 2 is false. G1 named
 * the frames out falsifies, ensures 4 and 5, which were residuals; ensures
 * 2 was proved, and no entry named it. Since G1 remove requires out to be
 * separated from the buffer, so the call below is outside its contract;
 * its run-time behaviour is unchanged.
 *
 * remove(&v, 1, &v.items[1]) on [10, 20, 30, 40]: *out = items[1] stores 20
 * where it was, the move then writes 30 there, so *out is 30. ensures 1, 3,
 * 4 and 5 hold. Every access is defined C: out is an int in an int array,
 * the copy is an int store, and the move copies whole ints.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_vec_remove_out_moved.c -o p && ./p
 * Expected: ensures 1, 3, 4 and 5 hold, ensures 2 VIOLATED; exit 1,
 * sanitizers clean. The same without the two defines (the default build),
 * at LP64 and ILP32.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "vmacros/vdrivers/vec_cc_experiment.h"

static int say(const char* what, int holds) {
    printf("    %-62s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    int buf[4];
    vec_int v = vec_int_init(buf, 4);
    int old[4];
    int removed;
    usize k;
    int e4 = 1, e5 = 1, bad = 0;
    result__Bool_Error r;

    setvbuf(stdout, NULL, _IONBF, 0);
    (void)vec_int_push(&v, 10); (void)vec_int_push(&v, 20);
    (void)vec_int_push(&v, 30); (void)vec_int_push(&v, 40);
    for (k = 0u; k < 4u; k++) { old[k] = v.items[k]; }
    removed = v.items[1];

    printf("remove(&v, 1, &v.items[1]) on [10, 20, 30, 40], out in the move's destination:\n");
    r = vec_int_remove(&v, 1u, &v.items[1]);
    printf("      len %zu; items [%d, %d, %d]; *out %d; the element was %d\n",
           (size_t)v.len, v.items[0], v.items[1], v.items[2], v.items[1], removed);
    for (k = 0u; k < 1u; k++)    { if (v.items[k] != old[k])     { e4 = 0; } }
    for (k = 1u; k < v.len; k++) { if (v.items[k] != old[k + 1]) { e5 = 0; } }
    bad += say("ensures 1: the result is ok", r.is_ok);
    bad += say("ensures 2: *out == \\old(v->items[i])", v.items[1] == removed);
    bad += say("ensures 3: v->len == \\old(v->len) - 1", v.len == 3u);
    bad += say("ensures 4: items[k] unchanged for k < i", e4);
    bad += say("ensures 5: items[k] == \\old(items[k + 1]) for i <= k < len", e5);
    return bad ? 1 : 0;
}
