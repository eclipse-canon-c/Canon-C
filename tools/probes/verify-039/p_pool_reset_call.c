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

/* VERIFY-039 G4, entries 1 and 2 -- docs/deviations.md, the proved-goal
 * argument, group A: the goals of pool_reset_secure the entries missed.
 *
 * Entries 1 and 2 named pool_reset_secure's `ensures pool_invariant(pool)`
 * (p_pool_self.c). WP proves it from pool_reset's postcondition. The goal
 * that crosses the zeroing is pool_reset's precondition at the second call,
 * `pool == \null || pool_invariant(pool)` (call_pool_reset_2_requires,
 * proved in frama-c-pool). This probe observes the state at that call, on
 * entry to pool_reset, through -finstrument-functions, and both
 * postconditions after the call, in three placements:
 *
 *   A, B: p_pool_self.c's. In A the zeroed slot covers the Arena, inside
 *     its own buffer; in B it covers the Pool, at the start of its slots.
 *   C: the Pool over the end of slot 0, so that the zeroing covers its
 *     `arena` field and not its `used` field: pool->arena becomes null,
 *     pool_reset returns at once, and `used` stays 1. So the other
 *     postcondition, `ensures pool->used == 0` (secure_ensures), is false
 *     too. WP proves it from pool_reset's postcondition as well.
 *
 * Until the G4 pool repair, pool_init admitted B and C: it did not separate
 * the Pool from the arena's free tail. Until the G4 arena repair,
 * arena_init admitted A. Since the repairs the calls are outside those
 * contracts; the run-time behaviour is unchanged. Every write into the
 * Pool or the Arena from the slots is a character-type write, and every
 * later read is through the field's declared type.
 *
 * From the repository root, in the proof configuration:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -finstrument-functions \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_pool_reset_call.c -o p && ./p
 * Expected: at the call, pool_invariant VIOLATED in A, B and C; after it,
 * pool_invariant VIOLATED in all three and pool->used == 0 VIOLATED in C;
 * exit 1, sanitizers clean. Without the two defines, B and C are the same,
 * and A is skipped: there pool_reset's ensure_msg stops the program.
 */
#define CANON_CONTRACT_IMPL
#include <stdint.h>
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

/* The Pool whose pool_reset_secure is running, and pool_ok at the first
 * entry to pool_reset during it: -1 if pool_reset was not entered. */
static const Pool* watched;
static int at_call = -1;
static int in_hook;

void __cyg_profile_func_enter(void* fn, void* site)
    __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void* fn, void* site)
    __attribute__((no_instrument_function));

void __cyg_profile_func_enter(void* fn, void* site) {
    (void)site;
    if (in_hook || watched == NULL || at_call != -1) { return; }
    /* Compared as integers: ISO C converts any pointer to one. */
    if ((uintptr_t)fn == (uintptr_t)&pool_reset) {
        in_hook = 1;
        at_call = pool_ok(watched);
        in_hook = 0;
    }
}
void __cyg_profile_func_exit(void* fn, void* site) { (void)fn; (void)site; }

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

/* pool_reset_secure(p), with pool_used(p) > 0, as the three cases run it:
 * the number of checks VIOLATED, or -1 if pool_reset was not entered. */
static int run(Pool* p) {
    int bad = 0;
    watched = p;
    at_call = -1;
    pool_reset_secure(p);
    watched = NULL;
    if (at_call == -1) {
        printf("    pool_reset was not entered: -finstrument-functions missing?\n");
        return -1;
    }
    bad += say("at the call to pool_reset: pool_invariant(pool)", at_call);
    bad += say("pool_reset_secure: ensures pool->used == 0", p->used == 0u);
    bad += say("pool_reset_secure: ensures pool_invariant(pool)", pool_ok(p));
    return bad;
}

int main(void) {
    unsigned char* block = malloc(256);
    Arena  outside;
    Pool*  pool;
    int bad = 0, r;

    if (block == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("sizeof(Arena) = %zu, sizeof(Pool) = %zu, CANON_DEFAULT_ALIGN = %zu\n",
           (size_t)sizeof(Arena), (size_t)sizeof(Pool), (size_t)CANON_DEFAULT_ALIGN);

    /* B: the Arena outside its buffer, the Pool at the start of that buffer;
     *    pool_init takes the slots from offset 0, so slot 0 is the Pool. */
    arena_init(&outside, block, 256);
    pool = (Pool*)(void*)block;
    if (!pool_init(pool, &outside, 64, 2)) { return 3; }
    printf("B: Pool inside its slot region, at slot 0:\n");
    (void)pool_alloc(pool);
    if ((r = run(pool)) < 0) { return 2; }
    bad += r;

    /* C: the Pool over the end of slot 0, its `arena` field the slot's last
     *    bytes. Slots of CANON_DEFAULT_ALIGN bytes from offset 0. */
    arena_init(&outside, block, 256);
    pool = (Pool*)(void*)(block + CANON_DEFAULT_ALIGN - sizeof(Arena*));
    if (!pool_init(pool, &outside, CANON_DEFAULT_ALIGN, 4)) { return 4; }
    if (pool->base_mark != 0u) { return 5; }
    printf("C: Pool at offset %zu, its arena field the end of slot 0 (%zu bytes):\n",
           (size_t)((unsigned char*)pool - block), (size_t)pool->object_size);
    (void)pool_alloc(pool);
    if ((r = run(pool)) < 0) { return 2; }
    bad += r;

#ifdef CANON_NO_REQUIRE
    {
        /* A: the Arena at the start of its own buffer; the pool's slots,
         *    taken from offset 0, cover it. */
        Arena* a = (Arena*)(void*)block;
        Pool   p;
        arena_init(a, block, 256);
        if (!pool_init(&p, a, 32, 4)) { return 6; }
        printf("A: Arena inside its own buffer, covered by slot 0:\n");
        (void)pool_alloc(&p);
        if ((r = run(&p)) < 0) { return 2; }
        bad += r;
    }
#else
    printf("A: skipped outside the proof configuration (ensure_msg stops pool_reset)\n");
#endif

    free(block);
    return bad ? 1 : 0;
}
