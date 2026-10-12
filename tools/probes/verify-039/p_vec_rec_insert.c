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
 * argument, group E: goals of insert the entry missed, for a vec<Rec> in
 * its own buffer.
 *
 * Rec is `struct { u32 id; i64 due; }` (vmacros/vdrivers/
 * vec_struct_cc_experiment.h, the cc-vec_struct unit). insert moves the
 * elements from i up by one with mem_move, a write through `char *`, and
 * then reads v->items, for the element store, and v->len, for the
 * increment. WP's typed model keeps the vec's fields apart from the bytes
 * mem_move writes, so it takes both to be unchanged by the move; and it
 * takes len to be unchanged by the element store, an i64's and a u32's.
 * Until the G4 vec repair (66b54e0) nothing kept the vec off its buffer.
 * Since the repair insert requires that, so the calls below are outside
 * its contract; their run-time behaviour is unchanged.
 *
 * On LP64 a vec_Rec at items[len], the first free slot, has its items field
 * where that slot's id and padding are and its len where its due is. The
 * move's last step copies the last element, items[len - 1], onto the vec.
 * Here that element holds, in its id and padding, a pointer to another
 * block q, stored through `Rec *`, and in its due an i64 d. A Rec is 16
 * bytes and the move shifts by one, so the copy moves the pointer and the
 * i64 whole, each to an 8-aligned place: v->items reads q through its own
 * type, and v->len reads d through `usize`, its unsigned counterpart.
 *
 * Case A, q four elements long, d 7: insert(v, 1, item) at len 3 stores the
 * item into q[1] and len becomes 8. ensures 2, v->len == \old(v->len) + 1,
 * the `assigns` of insert and of its ok behaviour, and ensures 4 (the frame
 * below i, entry 5's) are false; ensures 1 and 3 hold. Case B, q one
 * element long: after the move v->items + 1 lies past q's block, so the
 * run-time check that the element store's target is writable
 * (assert_rte_mem_access_9) is false. The probe's mem_move, which replaces
 * the library's in the expansion, stops the call there through longjmp, so
 * the store is never made.
 *
 * Case C, entry 5's own placement, the vec at items[0], its len over
 * items[0].due: insert(v, 0, item) at len 2. The move writes items[1 .. 2],
 * over the vec's capacity, which nothing reads after, and leaves its items
 * and len. The element store then lands on the vec, its id over items and
 * its due over len, and the next statement, v->len++, reads that due
 * through `usize`: len becomes 78, and ensures 2 is false again. Entry 5
 * called insert there at len, which stores the element away from the vec.
 * Where the element store of push, try_push or push_unchecked would land
 * on the vec, they store the element and len in one expression,
 * unsequenced, and the call is undefined (C11 6.5p2); at entry 5's len 2
 * they store it away from the vec too.
 *
 * Every access made is defined C, on entry 5's reading of C11 6.5p6 and
 * 6.5p7: the buffer and q are heap storage; the pointer and the i64 are
 * stored through their own types and copied whole by memmove, which gives
 * the copies their effective types; the library reads v->items through
 * `Rec *`, in case C before the element store over it, and v->len through
 * `usize`, the counterpart of the i64 there; the probe reads the same, and
 * q's elements through their own types, and in case C only v->len. On
 * ILP32 a vec_Rec's len is four bytes and covers only half of a due, and
 * the probe says so and stops.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_vec_rec_insert.c -o p && ./p
 * Expected on LP64: case A, ensures 1 and 3 hold, ensures 2, both `assigns`
 * and ensures 4 VIOLATED; case B, the check VIOLATED, the call stopped
 * after the move; case C, ensures 1 holds, ensures 2 VIOLATED; exit 1,
 * sanitizers clean. The same without the two defines (the default build).
 * On ILP32: "not applicable", exit 0.
 */
#define CANON_CONTRACT_IMPL
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/memory.h"

/* The library's mem_move, observed at its return; case B stops the call
 * there. Defined before the vec expansion, which calls it by this name. */
static int stopped_after_move(void);
static jmp_buf back;
static void probe_mem_move(void* dest, const void* src, usize size) {
    mem_move(dest, src, size);
    if (stopped_after_move()) { longjmp(back, 1); }
}
#define mem_move probe_mem_move
#include "vmacros/vdrivers/vec_struct_cc_experiment.h"
#undef mem_move

#if defined(__GNUC__)
#  define COUNTERPARTS \
     ((__builtin_types_compatible_p(i64, long) && \
       __builtin_types_compatible_p(usize, unsigned long)) || \
      (__builtin_types_compatible_p(i64, long long) && \
       __builtin_types_compatible_p(usize, unsigned long long)))
#else
#  define COUNTERPARTS 0
#endif

enum { CAP = 16, LEN = 3, I = 1, D = 7 };

static Rec*     items;          /* the buffer */
static vec_Rec* watched;        /* case B: the vec, read after the move */
static Rec*     short_block;    /* case B: q, one element long */
static Rec*     items_seen;     /* case B: v->items after the move */

static int stopped_after_move(void) {
    if (watched == NULL) { return 0; }
    items_seen = watched->items;
    return items_seen == short_block;
}

static int say(const char* what, int holds) {
    printf("    %-62s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

static int same(Rec a, Rec b) { return a.id == b.id && a.due == b.due; }

/* The vec at items[LEN]; the last element, items[LEN - 1], holds q in its
 * id and padding, through `Rec *`, and d in its due, through i64. */
static vec_Rec* setup(Rec* q, i64 d) {
    vec_Rec* v = (vec_Rec*)(void*)&items[LEN];
    usize k;
    for (k = 0u; k < (usize)LEN - 1u; k++) {
        Rec e;
        e.id = (u32)k;
        e.due = (i64)(100 + k);
        items[k] = e;
    }
    *(Rec**)(void*)&items[LEN - 1] = q;
    *(i64*)(void*)((char*)&items[LEN - 1] + offsetof(Rec, due)) = d;
    v->items = items;
    v->len = LEN;
    v->capacity = CAP;
    return v;
}

/* Case B: q one element long; the probe's mem_move stops the call after
 * the move, before the element store. */
static int case_b(Rec item) {
    vec_Rec* v = setup(short_block, D);
    watched = v;
    printf("B: the same with q one element long:\n");
    if (setjmp(back) == 0) {
        (void)vec_Rec_insert(v, I, item);
        watched = NULL;
        printf("insert returned: NOT as placed\n");
        return 1;
    }
    watched = NULL;
    printf("      after the move v->items %s q, a block of 1 element; i = %d\n",
           items_seen == short_block ? "==" : "!=", I);
    printf("      the call stopped there; the store was not made\n");
    return say("rte mem_access_9: v->items + i writable", items_seen != short_block);
}

/* Case C: entry 5's placement, the vec at items[0]; the element store at
 * items[0] lands on the vec. v->items is not read after it. */
static int case_c(Rec item) {
    vec_Rec* v = (vec_Rec*)(void*)&items[0];
    result__Bool_Error r;
    usize k, len_after;
    int bad = 0;
    for (k = 0u; k < (usize)CAP; k++) {
        Rec e;
        e.id = (u32)k;
        e.due = (i64)(100 + k);
        items[k] = e;
    }
    v->items = items;
    v->len = 2u;
    v->capacity = CAP;
    printf("C: insert(v, 0, item) at len 2, the vec at items[0] (entry 5's placement):\n");
    r = vec_Rec_insert(v, 0u, item);
    len_after = v->len;
    printf("      len %zu; item.due %lld\n", (size_t)len_after, (long long)item.due);
    bad += say("ensures 1: the result is ok", r.is_ok);
    bad += say("ensures 2: v->len == \\old(v->len) + 1", len_after == 3u);
    printf("    ensures 3 and 4 read v->items, which now holds the item's id: not read\n");
    return bad;
}

int main(void) {
    Rec* q;
    vec_Rec* v;
    Rec item = {7u, 77};
    Rec old0, q0, q1;
    result__Bool_Error r;
    usize k, len_after;
    Rec* items_after;
    int bad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!COUNTERPARTS || sizeof(usize) != sizeof(i64) || offsetof(vec_Rec, items) != 0u ||
        offsetof(vec_Rec, len) != offsetof(Rec, due) || sizeof(Rec*) > offsetof(Rec, due) ||
        sizeof(vec_Rec) > 2u * sizeof(Rec)) {
        printf("not applicable: a vec_Rec's items and len do not lie on a Rec's id "
               "and due, or i64 and usize are not counterparts, here\n");
        return 0;
    }
    items = malloc(CAP * sizeof(Rec));
    q = malloc(4u * sizeof(Rec));
    short_block = malloc(sizeof(Rec));
    if (items == NULL || q == NULL || short_block == NULL) {
        free(items); free(q); free(short_block);
        return 2;
    }
    for (k = 0u; k < 4u; k++) {
        Rec e;
        e.id = (u32)(90u + k);
        e.due = (i64)(900 + 10 * (long)k);
        q[k] = e;
    }
    short_block[0] = q[0];

    /* Case A. */
    v = setup(q, D);
    old0 = items[0];
    q0 = q[0];
    q1 = q[1];
    printf("A: insert(v, %d, item) at len %d, the vec at items[%d], the last element "
           "{q, %d}:\n", I, LEN, LEN, D);
    r = vec_Rec_insert(v, I, item);
    len_after = v->len;
    items_after = v->items;
    printf("      v->items %s q; len %zu; q[1] {%lu, %lld}, was {%lu, %lld}\n",
           items_after == q ? "==" : "!=", (size_t)len_after,
           (unsigned long)q[1].id, (long long)q[1].due,
           (unsigned long)q1.id, (long long)q1.due);
    bad += say("ensures 1: the result is ok", r.is_ok);
    bad += say("ensures 2: v->len == \\old(v->len) + 1", len_after == (usize)LEN + 1u);
    bad += say("ensures 3: v->items[i] == item", same(items_after[I], item));
    bad += say("ensures 4: v->items[k] unchanged for k < i (entry 5's)",
               same(items_after[0], old0));
    bad += say("assigns: nothing outside v->len, v->items[0 .. capacity - 1]",
               same(q[1], q1) && same(q[0], q0));
    bad += say("ok's assigns: nothing outside v->len, v->items[i .. len]",
               same(q[1], q1) && same(q[0], q0));

    bad += case_b(item);
    bad += case_c(item);

    free(short_block);
    free(q);
    free(items);
    return bad ? 1 : 0;
}
