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

/* VERIFY-039 G4, listing entry 6 -- docs/deviations.md: remove's out over
 * the vec_Rec it removes from, with out's due on the vec's len.
 *
 * Rec is `struct { u32 id; i64 due; }` (vmacros/vdrivers/
 * vec_struct_cc_experiment.h, the cc-vec_struct unit). remove requires out
 * valid and, since G1, separated from v's buffer; until the G4 entry 6
 * repair nothing separated it from *v. Since the repair remove requires
 * that too, so the call below is outside its contract; its run-time
 * behaviour is unchanged.
 *
 * On LP64 a Rec at the vec's own address has its id over the vec's items
 * field, its padding over the rest of that field, and its due over the
 * vec's len: an i64 over a usize, signed and unsigned counterparts (C11
 * 6.5p7), which WP's typed model keeps in two arrays (sint64 and uint64).
 *
 * remove(v, 2, out) at len 3, the element's due 1. remove stores the
 * element into *out, so v->len reads 1, through its own type, the
 * counterpart of the i64 stored there; i < v->len - 1 is then false, so
 * there is no move and remove reads v->items no more; v->len-- stores 0,
 * over out->due. ensures 2, *out == \old(v->items[i]), and ensures 3,
 * v->len == \old(v->len) - 1, are false; WP proves both, the store to len
 * and the element's due lying in different arrays of its model.
 *
 * Every access is defined C. Both blocks are heap storage. The library reads
 * v->len after the element store, a counterpart read, and v->items only
 * before it. The probe reads out->id through its own type, out->due as an
 * i64, the counterpart of the usize stored there last, and through memcpy,
 * and v->len through its own type. It does not read v->items, whose bytes
 * now hold out's id and padding: ensures 4 and 5 read it, and that read is
 * the undefined step of G4's scope. pop's `*out = v->items[--v->len]`
 * stores len and *out unsequenced, so out over len is undefined there, and
 * pop is not an entry. On ILP32 due covers len and capacity, two usize
 * fields of another width, and the probe says so and stops.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_vec_rec_out.c -o p && ./p
 * Expected on LP64: ensures 1 holds, ensures 2 and 3 VIOLATED; exit 1,
 * sanitizers clean. The same without the two defines (the default build).
 * On ILP32: "not applicable", exit 0.
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vmacros/vdrivers/vec_struct_cc_experiment.h"

#if defined(__GNUC__)
#  define COUNTERPARTS \
     ((__builtin_types_compatible_p(i64, long) && \
       __builtin_types_compatible_p(usize, unsigned long)) || \
      (__builtin_types_compatible_p(i64, long long) && \
       __builtin_types_compatible_p(usize, unsigned long long)))
#else
#  define COUNTERPARTS 0
#endif

enum { CAP = 16, LEN = 3, I = 2 };

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    size_t vsize = sizeof(vec_Rec) > sizeof(Rec) ? sizeof(vec_Rec) : sizeof(Rec);
    vec_Rec* v = malloc(vsize);
    Rec* items = malloc(CAP * sizeof(Rec));
    Rec* out;
    Rec removed;
    result__Bool_Error r;
    usize len_after;
    u32 id_after;
    i64 due_after, due_bytes;
    usize k;
    int bad = 0;

    if (v == NULL || items == NULL) { free(v); free(items); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!COUNTERPARTS || offsetof(vec_Rec, len) != offsetof(Rec, due) ||
        sizeof(usize) != sizeof(i64)) {
        printf("not applicable: a Rec's due does not lie on a vec_Rec's len, "
               "or i64 and usize are not counterparts, here\n");
        free(v); free(items);
        return 0;
    }

    for (k = 0u; k < (usize)CAP; k++) {
        Rec e;
        e.id = (u32)k;
        e.due = (i64)(100 + k);
        items[k] = e;
    }
    items[I].due = 1;           /* v->len reads 1 after the element store */
    v->items = items;
    v->len = LEN;
    v->capacity = CAP;
    removed = items[I];
    out = (Rec*)(void*)v;       /* id over items, due over len */

    printf("remove(v, %d, out) at len %d, out at the vec, its due on len:\n", I, LEN);
    r = vec_Rec_remove(v, I, out);
    len_after = v->len;
    id_after = out->id;
    due_after = out->due;
    memcpy(&due_bytes, (const char*)out + offsetof(Rec, due), sizeof due_bytes);
    printf("      len %zu; *out = {%lu, %lld}; the element was {%lu, %lld}\n",
           (size_t)len_after, (unsigned long)id_after, (long long)due_after,
           (unsigned long)removed.id, (long long)removed.due);
    bad += say("ensures 1: the result is ok", r.is_ok);
    bad += say("ensures 2: *out == \\old(v->items[i])",
               id_after == removed.id && due_after == removed.due &&
               due_bytes == removed.due);
    bad += say("ensures 3: v->len == \\old(v->len) - 1", len_after == (usize)(LEN - 1));
    printf("    ensures 4 and 5 read v->items, which now holds out's id: not read\n");

    free(items);
    free(v);
    return bad ? 1 : 0;
}
