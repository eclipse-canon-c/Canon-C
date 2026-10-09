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

/* VERIFY-039 G4, listing entry 5 -- docs/deviations.md: a vec<Rec> that
 * lives in its own live elements, where its len is a Rec's due.
 *
 * Rec is `struct { u32 id; i64 due; }` (vmacros/vdrivers/
 * vec_struct_cc_experiment.h, the cc-vec_struct unit). On LP64 a vec_Rec
 * placed at items[0] has its len field exactly where Rec[0].due is: a
 * usize over an i64, signed and unsigned counterparts (C11 6.5p7), which
 * WP's typed model keeps in two arrays (uint64 and sint64). So every store
 * the library makes to v->len changes Rec[0], an element below i or below
 * \old(v->len), and the frame postconditions that WP proves across the store
 * are false: `v->items[0] == \old(v->items[0])` fails on the due member.
 *
 * The vec's items field lies over Rec[0].id and its padding and its
 * capacity over Rec[1]'s; the library reads them through their own types
 * and stores no element there, and the probe reads only Rec[0].due, through
 * a plain i64 lvalue, the counterpart of the usize stored there, and through
 * memcpy. Every access is defined C. On ILP32 len is four bytes and covers
 * only half of due, and the probe says so and stops.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_vec_rec_live.c -o p && ./p
 * Expected on LP64: for insert, remove, push, try_push, push_unchecked,
 * pop and append_array, Rec[0].due follows len and the frame is VIOLATED;
 * exit 1, sanitizers clean. The same without the two defines (the default
 * build). On ILP32: "not applicable", exit 0.
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vmacros/vdrivers/vec_struct_cc_experiment.h"

enum { CAP = 16 };

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

/* Rec[0].due through a plain i64 lvalue: the object there was last stored
 * as v->len, a usize, and i64 is its signed counterpart. */
static i64 due0(const Rec* items) {
    return *(const i64*)(const void*)((const char*)items + offsetof(Rec, due));
}
static i64 due0_bytes(const Rec* items) {
    i64 x;
    memcpy(&x, (const char*)items + offsetof(Rec, due), sizeof x);
    return x;
}

/* The vec at items[0]: its fields over Rec[0] and Rec[1]'s id and padding;
 * elements from Rec[2] on. */
static vec_Rec* setup(Rec* items, usize len) {
    vec_Rec* v = (vec_Rec*)(void*)items;
    usize k;
    v->items = items;
    v->len = len;
    v->capacity = CAP;
    for (k = 2u; k < CAP; k++) {
        Rec r;
        r.id = (u32)k;
        r.due = (i64)(1000 + k);
        items[k] = r;
    }
    return v;
}

static int frame(const char* call, const char* clause, const Rec* items, i64 before) {
    char what[96];
    printf("%s: Rec[0].due %lld -> %lld\n", call, (long long)before, (long long)due0(items));
    (void)snprintf(what, sizeof what, "%s, at k = 0", clause);
    return say(what, due0(items) == before && due0_bytes(items) == before);
}

int main(void) {
    Rec* items = malloc(CAP * sizeof(Rec));
    vec_Rec* v;
    Rec item = {7u, 77};
    Rec out = {0u, 0};
    Rec src[2] = {{5u, 55}, {6u, 66}};
    i64 before;
    int bad = 0;

    if (items == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (offsetof(vec_Rec, len) != offsetof(Rec, due) || sizeof(usize) != sizeof(i64)) {
        printf("not applicable: vec_Rec's len does not lie on Rec[0].due here\n");
        free(items);
        return 0;
    }
    printf("vec_Rec at items[0]; len lies on Rec[0].due; capacity %d\n", CAP);

    v = setup(items, 2u); before = due0(items);
    (void)vec_Rec_insert(v, 2u, item);                  /* i == len: no move */
    bad += frame("insert(v, 2, item) at len 2", "ensures 4: items[k] unchanged for k < i", items, before);

    v = setup(items, 3u); before = due0(items);
    (void)vec_Rec_remove(v, 2u, &out);                  /* i == len - 1: no move */
    bad += frame("remove(v, 2, &out) at len 3", "ensures 4: items[k] unchanged for k < i", items, before);

    v = setup(items, 2u); before = due0(items);
    (void)vec_Rec_push(v, item);
    bad += frame("push(v, item) at len 2", "ensures 4: items[k] unchanged, k < old len", items, before);

    v = setup(items, 2u); before = due0(items);
    (void)vec_Rec_try_push(v, item);
    bad += frame("try_push(v, item) at len 2", "ensures 4: items[k] unchanged, k < old len", items, before);

    v = setup(items, 2u); before = due0(items);
    vec_Rec_push_unchecked(v, item);
    bad += frame("push_unchecked(v, item) at len 2", "ensures 3: items[k] unchanged, k < old len", items, before);

    v = setup(items, 3u); before = due0(items);
    (void)vec_Rec_pop(v, &out);
    bad += frame("pop(v, &out) at len 3", "ensures 4: items[k] unchanged for k < len", items, before);

    v = setup(items, 2u); before = due0(items);
    (void)vec_Rec_append_array(v, src, 2u);
    bad += frame("append_array(v, src, 2) at len 2", "ensures 3: items[k] unchanged, k < old len", items, before);

    free(items);
    return bad ? 1 : 0;
}
