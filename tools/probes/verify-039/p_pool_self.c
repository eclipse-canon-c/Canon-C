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

/* VERIFY-039 G4, listing entries 1 and 2 -- docs/deviations.md:
 * pool_reset_secure's `ensures pool_invariant(pool)`.
 *
 * pool_reset_secure zeroes the pool's slots through `char *`
 * (mem_secure_zero) and then calls pool_reset. Its postcondition
 * pool_invariant(pool) reads the Pool's fields and, through
 * arena_invariant, the Arena's. WP proves it across the `char` write
 * (VERIFY-039 G4). In C it is false when the zeroed slots cover either
 * struct, and two placements make them do so:
 *
 *   B (entry 2): the Pool lives inside the slot region it manages, which
 *     pool_init admits: it separates neither *pool from the arena's free
 *     tail nor, in pool_invariant, *pool from its slots. The Arena is
 *     outside its buffer, as arena_init requires since the G4 arena repair.
 *   A (entry 1): the Arena lives inside its own buffer and the pool's
 *     slots cover it. Since the G4 arena repair, arena_init's contract
 *     excludes this placement; the run-time behaviour is unchanged.
 *
 * From the repository root, in the proof configuration:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_pool_self.c -o p && ./p
 * Expected: in both cases pool_invariant VIOLATED; exit 1, sanitizers clean.
 * Without the two defines, case B is the same, and case A is skipped: there
 * pool_reset's ensure_msg stops the program before pool_reset_secure returns,
 * so no postcondition is reached.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <stdlib.h>
#include "core/arena.h"
#include "core/pool.h"

/* The conjuncts of arena_invariant and pool_invariant that C can check. */
static int arena_ok(const Arena* a) {
    return a != NULL && a->buffer != NULL && a->capacity > 0 &&
           a->capacity <= CANON_ARENA_MAX_SIZE && a->offset <= a->capacity;
}
static int pool_ok(const Pool* p) {
    return arena_ok(p->arena) && p->object_size > 0 && p->used <= p->capacity &&
           p->base_mark <= p->end_mark && p->end_mark <= p->arena->capacity &&
           p->end_mark - p->base_mark == p->capacity * p->object_size;
}

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    unsigned char* block = malloc(256);
    Arena  outside;
    Pool*  pool;
    int bad = 0;

    if (block == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("sizeof(Arena) = %zu, sizeof(Pool) = %zu\n",
           (size_t)sizeof(Arena), (size_t)sizeof(Pool));

    /* B: the Arena outside its buffer, the Pool at the start of that buffer;
     *    pool_init takes the slots from offset 0, so slot 0 is the Pool. */
    arena_init(&outside, block, 256);
    pool = (Pool*)(void*)block;
    if (!pool_init(pool, &outside, 64, 2)) { return 3; }
    printf("B: Pool inside its slot region (slots at offsets %zu .. %zu):\n",
           (size_t)pool->base_mark, (size_t)pool->end_mark - 1u);
    bad += say("before the call: pool_invariant(pool)", pool_ok(pool));
    (void)pool_alloc(pool);
    pool_reset_secure(pool);
    bad += say("pool_reset_secure: ensures pool_invariant(pool)", pool_ok(pool));

#ifdef CANON_NO_REQUIRE
    {
        /* A: the Arena at the start of its own buffer; the pool's slots,
         *    taken from offset 0, cover it. */
        Arena* a = (Arena*)(void*)block;
        Pool   p;
        arena_init(a, block, 256);
        if (!pool_init(&p, a, 32, 4)) { return 4; }
        printf("A: Arena inside its own buffer, covered by the slots:\n");
        bad += say("before the call: pool_invariant(&p)", pool_ok(&p));
        (void)pool_alloc(&p);
        pool_reset_secure(&p);
        bad += say("pool_reset_secure: ensures pool_invariant(&p)", pool_ok(&p));
    }
#else
    printf("A: skipped outside the proof configuration (ensure_msg stops pool_reset)\n");
#endif

    free(block);
    return bad ? 1 : 0;
}
