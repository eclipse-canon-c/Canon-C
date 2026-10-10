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

/* VERIFY-039 G7 -- docs/deviations.md: pool_reset after the pool's arena has
 * been reset.
 *
 * pool_reset requires pool_invariant(pool) and calls
 * arena_reset_to(pool->arena, pool->base_mark), which requires
 * mark <= arena->offset. Until the G7 repair pool_invariant did not state
 * that relation, so arena_reset, which sets the offset to 0, left
 * pool_invariant true; nothing in the arena's contracts keeps the relation.
 * The pool header lists that sequence as unsafe; no contract excluded it.
 * Since the repair, pool_invariant states base_mark <= arena->offset, so the
 * pool_reset call below is outside its contract. The probe's checks and
 * exit status are unchanged.
 *
 * Here one byte is taken from the arena before pool_init, so base_mark is
 * 16, and the arena is then reset. pool_invariant as it stood before G7
 * holds, so pool_reset was admitted, and its call to arena_reset_to has
 * mark 16 and offset 0: the call-site precondition
 * (pool_reset_call_arena_reset_to_requires_2) is false. In the proof
 * configuration the call goes ahead and moves the offset forward to 16; in
 * the default build arena_reset_to's require_msg stops the program, the
 * run-time check catching the same false precondition.
 *
 * From the repository root, in the proof configuration:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_pool_reset_mark.c -o p && ./p
 * Expected: pool_invariant as it stood before G7 holds before pool_reset,
 * the call-site precondition is VIOLATED, pool_reset returns with the
 * offset moved forward; exit 1, sanitizers clean. Without the two defines
 * the contract handler aborts the program inside arena_reset_to (exit 134).
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <stdlib.h>
#include "core/arena.h"
#include "core/pool.h"

/* The conjuncts of arena_invariant and pool_invariant that C can check, as
 * they stood before G7; for the \valid ones, non-null. The separations hold
 * by construction: the Arena and the Pool are locals, the buffer heap. G7's
 * conjunct, base_mark <= arena->offset, is the call-site check below. */
static int pool_ok(const Pool* p) {
    const Arena* a;
    if (p == NULL) { return 0; }
    a = p->arena;
    return a != NULL && a->buffer != NULL && a->capacity > 0 &&
           a->capacity <= CANON_ARENA_MAX_SIZE &&
           a->offset <= a->capacity && p->object_size > 0 &&
           p->used <= p->capacity &&
           p->capacity <= CANON_USIZE_MAX / p->object_size &&
           p->base_mark <= p->end_mark && p->end_mark <= a->capacity &&
           p->end_mark - p->base_mark == p->capacity * p->object_size;
}

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    unsigned char* buffer = malloc(512);
    Arena arena;
    Pool  pool;
    int   bad = 0;

    if (buffer == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    arena_init(&arena, buffer, 512);
    (void)arena_alloc(&arena, 1);          /* the pool's region starts past it */
    if (!pool_init(&pool, &arena, 32, 4)) { return 3; }
    arena_reset(&arena);                   /* unsafe per the header; admitted */

    printf("after arena_reset: base_mark %zu, end_mark %zu, arena offset %zu\n",
           (size_t)pool.base_mark, (size_t)pool.end_mark, (size_t)arena.offset);
    bad += say("before pool_reset: pool_invariant(pool) before G7", pool_ok(&pool));
    bad += say("pool_reset's call to arena_reset_to: mark <= offset",
               pool.base_mark <= arena.offset);
    pool_reset(&pool);
    printf("after pool_reset: used %zu, arena offset %zu\n",
           (size_t)pool.used, (size_t)arena.offset);

    free(buffer);
    return bad ? 1 : 0;
}
