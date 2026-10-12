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

/* VERIFY-039 G4, listing entry 5 -- docs/deviations.md, the proved-goal
 * argument, group E: a goal resting on remove's that the entry missed, for
 * a vec<Rec> in its own live elements.
 *
 * Rec is `struct { u32 id; i64 due; }` (vmacros/vdrivers/
 * vec_struct_cc_experiment.h, the cc-vec_struct unit). remove shifts the
 * elements after i down by one with mem_move, a write through `char *`,
 * and then reads v->len for the decrement. WP's typed model takes len to
 * be unchanged by the move, and proves ensures 3, v->len == \old(v->len) -
 * 1; remove_option, which calls remove with a local out, proves its own
 * from it (some_ensures_3). Entry 6 found ensures 3 false for an out over
 * the vec; remove_option's out is a local and cannot lie there. Until the
 * G4 vec repair (66b54e0) nothing kept the vec off its buffer either. Since
 * the repair remove and remove_option require that, so the calls below are
 * outside their contracts; their run-time behaviour is unchanged.
 *
 * On LP64 a vec_Rec placed 8 bytes into items[1] has its items field over
 * items[1].due, its len over items[2]'s id and padding and its capacity over
 * items[2].due. items[3]'s id and padding hold a usize x, 9, stored through
 * its own type. remove(v, 0, out) at len 4 copies items[0] into *out and
 * moves items[1 .. 3] down, so items[2] takes items[3]'s bytes whole and
 * len reads x through its own type; the decrement stores 8. The vec's items
 * field now holds the old capacity's bytes; nothing reads it after the
 * move. So ensures 3, and remove_option's some_ensures_3, are false: len is
 * 8, not 3. ensures 1 and 2 hold, and some_ensures and some_ensures_2.
 *
 * Every access made is defined C, on entry 5's reading of C11 6.5p6 and
 * 6.5p7: the buffer is heap storage; the vec's fields and x are stored
 * through their own types and moved whole by memmove, which gives the
 * copies their effective types; the library reads v->items and v->len only
 * through their own types, v->items only before the move, and reads
 * items[0], which the vec does not overlap, through `Rec`; the probe reads
 * v->len, the result and out. It reads neither v->items nor the moved
 * elements, whose bytes now hold the vec's: ensures 4 and 5 read them.
 * On ILP32 the fields lie elsewhere, and the probe says so and stops.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_vec_rec_remove_option.c -o p && ./p
 * Expected on LP64: remove, ensures 1 and 2 hold, ensures 3 VIOLATED;
 * remove_option, some_ensures and some_ensures_2 hold, some_ensures_3
 * VIOLATED; exit 1, sanitizers clean. The same without the two defines
 * (the default build). On ILP32: "not applicable", exit 0.
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

enum { CAP = 16, LEN = 4, I = 0, X = 9 };

static int say(const char* what, int holds) {
    printf("    %-62s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

static int same(Rec a, Rec b) { return a.id == b.id && a.due == b.due; }

/* The elements, then x in items[3]'s id and padding, then the vec 8 bytes
 * into items[1]. */
static vec_Rec* setup(Rec* items) {
    vec_Rec* v = (vec_Rec*)(void*)((char*)&items[1] + 8);
    usize k;
    for (k = 0u; k < (usize)CAP; k++) {
        Rec e;
        e.id = (u32)k;
        e.due = (i64)(100 + k);
        items[k] = e;
    }
    *(usize*)(void*)&items[3] = X;
    v->items = items;
    v->len = LEN;
    v->capacity = CAP;
    return v;
}

int main(void) {
    Rec* items = malloc(CAP * sizeof(Rec));
    vec_Rec* v;
    Rec out = {0u, 0};
    Rec removed;
    result__Bool_Error r;
    option_Rec o;
    usize len_after;
    int bad = 0;

    if (items == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!COUNTERPARTS || sizeof(usize) != sizeof(i64) || offsetof(Rec, due) != 8u ||
        offsetof(vec_Rec, len) != 8u || offsetof(vec_Rec, capacity) != 16u ||
        sizeof(Rec) != 16u) {
        printf("not applicable: a vec_Rec 8 bytes into a Rec does not have its len on "
               "the next Rec's id and padding, or i64 and usize are not counterparts, here\n");
        free(items);
        return 0;
    }

    v = setup(items);
    removed = items[I];
    printf("remove(v, %d, &out) at len %d, the vec 8 bytes into items[1], x = %d in "
           "items[3]:\n", I, LEN, X);
    r = vec_Rec_remove(v, I, &out);
    len_after = v->len;
    printf("      len %zu; *out {%lu, %lld}; the element was {%lu, %lld}\n",
           (size_t)len_after, (unsigned long)out.id, (long long)out.due,
           (unsigned long)removed.id, (long long)removed.due);
    bad += say("ensures 1: the result is ok", r.is_ok);
    bad += say("ensures 2: *out == \\old(v->items[i])", same(out, removed));
    bad += say("ensures 3: v->len == \\old(v->len) - 1", len_after == (usize)LEN - 1u);

    v = setup(items);
    removed = items[I];
    printf("remove_option(v, %d), the same placement:\n", I);
    o = vec_Rec_remove_option(v, I);
    len_after = v->len;
    printf("      len %zu; has_value %d, value {%lu, %lld}\n", (size_t)len_after,
           (int)o.has_value, (unsigned long)o.value.id, (long long)o.value.due);
    bad += say("some_ensures: \\result.has_value", o.has_value);
    bad += say("some_ensures_2: \\result.value == \\old(v->items[i])",
               o.has_value && same(o.value, removed));
    bad += say("some_ensures_3: v->len == \\old(v->len) - 1", len_after == (usize)LEN - 1u);
    printf("    ensures 4 and 5 read v->items and the moved elements, which now hold "
           "the vec's bytes: not read\n");

    free(items);
    return bad ? 1 : 0;
}
