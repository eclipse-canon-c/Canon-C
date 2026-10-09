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

/* VERIFY-039 G4, listing entry 5 -- docs/deviations.md: a vec that lives in
 * the free tail of its own items buffer.
 *
 * The vec contracts require the vec readable and writable and its items
 * valid up to capacity; nothing separates *v from v->items. append_array
 * copies the new elements with mem_copy, through `char *`. In WP's typed
 * model a `char` write cannot reach v->items, a pointer, so ensures 3,
 * \forall k < \old(v->len): v->items[k] == \old(v->items[k]), proves.
 *
 * Here the vec struct lies at items[len], the first free slot, so the copy
 * of the new elements overwrites its items field. The two "ints" appended
 * are the bytes of a pointer to another array, stored there as an int *,
 * so after the call v->items points to that array, and ensures 3 compares
 * it with the old elements. The vec's len is stored again after the copy,
 * so ensures 1 and 2 hold. Every access is defined C: the buffer is heap
 * storage; the copy is a character-type copy, which gives the overwritten
 * bytes the effective type of their source, int *, so the probe reads
 * v->items through its own type; and append_array reads no element of src
 * as an int. The same vec<int> instantiation CI verifies (the bare -CC
 * driver, as the G1 probes use).
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_vec_append_self.c -o p && ./p
 * Expected: ensures 1 and 2 hold, ensures 3 VIOLATED; exit 1, sanitizers
 * clean. The same without the two defines (the default build).
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vmacros/vdrivers/vec_cc_experiment.h"

struct vec_align { char c; vec_int v; };

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    enum { CAP = 16, LEN = 2 };
    static int other[LEN] = {7, 7};
    int* items = malloc(CAP * sizeof(int));
    int** cells = malloc(2u * sizeof(int*));
    vec_int* v;
    const int* src;
    result__Bool_Error r;
    int bad = 0;

    if (items == NULL || cells == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if ((LEN * sizeof(int)) % offsetof(struct vec_align, v) != 0u ||
        sizeof(int*) > 2u * sizeof(int)) { return 3; }

    /* The vec at items[LEN], the first free slot; two elements before it. */
    v = (vec_int*)(void*)&items[LEN];
    v->items = items;
    v->len = LEN;
    v->capacity = CAP;
    items[0] = 1;
    items[1] = 2;
    cells[0] = other;                        /* the new elements' bytes */
    cells[1] = other;
    src = (const int*)(const void*)cells;

    printf("vec at items[%d], sizeof(vec_int) = %zu; items = [1, 2], capacity %d\n",
           LEN, (size_t)sizeof(vec_int), CAP);
    r = vec_int_append_array(v, src, 2);
    printf("append_array(v, src, 2): is_ok=%d, len=%zu, v->items %s\n",
           (int)r.is_ok, (size_t)v->len,
           v->items == items ? "unchanged" : (v->items == other ? "now points at other[]" : "changed"));

    bad += say("ensures 1: \\result.is_ok", r.is_ok);
    bad += say("ensures 2: v->len == \\old(v->len) + count", v->len == LEN + 2u);
    /* Evaluate ensures 3 through the field, where it now points. */
    bad += say("ensures 3: v->items[k] == \\old(v->items[k]), k < 2",
               v->items == items ? (items[0] == 1 && items[1] == 2)
               : v->items == other ? (other[0] == 1 && other[1] == 2)
               : 0);

    free(cells);
    free(items);
    return bad ? 1 : 0;
}
