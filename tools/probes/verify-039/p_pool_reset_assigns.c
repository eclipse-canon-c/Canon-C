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

/* VERIFY-039 G4, entry 2 -- docs/deviations.md, the proved-goal argument,
 * group A: pool_reset_secure's `assigns`, false for a call its contract
 * admitted until the G4 pool repair.
 *
 * pool_reset_secure assigns pool->used, *(pool->arena) and the pool's slots,
 * all taken at entry. Its proved `assigns_normal` (frama-c-pool) checks the
 * effect of its call to pool_reset, made after the zeroing, against
 * *(pool->arena) as it was at entry, so its proof takes pool->arena to be
 * unchanged by the `char` write (VERIFY-039 G4). pool_invariant makes
 * neither the slots' start, buffer + base_mark, nor object_size a multiple
 * of a pointer's alignment. pool_init does both, since arena_alloc aligns
 * the address it returns and mem_align the size, but the contract admits a
 * Pool whose fields are set otherwise. Then the zeroing can end inside
 * pool->arena and turn it into another Arena's address, which pool_reset
 * resets and re-reserves.
 *
 * Here the Pool lies at slot 0 of its own slot region, with object_size 1
 * and used 1, so pool_reset_secure zeroes one byte: the first byte of
 * pool->arena, its low byte on the project's little-endian targets. The
 * real Arena lies 64 bytes past a 256-aligned decoy Arena in the same heap
 * block, so the zeroed pointer is the decoy's address. At entry
 * pool_invariant holds but for the conjunct the G4 pool repair added, which
 * keeps the Pool out of its slots; since the repair the call is outside
 * the contract, and its run-time behaviour is unchanged.
 *
 * Every access is defined C: the Arenas and the Pool are heap objects given
 * their types by the stores that build them, the zeroing is a
 * character-type write, and pool->arena is then read through its declared
 * type. Its bytes are a pointer into the same heap block as before, whose
 * address the probe converts to an integer when it places the decoy.
 *
 * From the repository root, in the proof configuration:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_pool_reset_assigns.c -o p && ./p
 * Expected: at entry pool_invariant holds but for the repair's conjunct;
 * after the call pool->arena is the decoy, the decoy's offset 80 (it was
 * 100), the real Arena unchanged; both postconditions hold, pool_invariant
 * whole, the repair's conjunct included (the decoy's slots are elsewhere),
 * and the `assigns` is VIOLATED; exit 1, sanitizers clean. The same without
 * the two defines (the default build).
 */
#define CANON_CONTRACT_IMPL
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/arena.h"
#include "core/pool.h"

static int arena_ok(const Arena* a) {
    return a != NULL && a->buffer != NULL && a->capacity > 0 &&
           a->capacity <= CANON_ARENA_MAX_SIZE && a->offset <= a->capacity;
}
static int overlaps(const void* p, size_t n, const void* q, size_t m) {
    uintptr_t a = (uintptr_t)p, b = (uintptr_t)q;
    return a < b + m && b < a + n;
}
/* pool_invariant's conjuncts but the G4 pool repair's, checked in C. */
static int pool_ok_but_repair(const Pool* p) {
    return arena_ok(p->arena) &&
           !overlaps(p->arena, sizeof(Arena), p->arena->buffer,
                     p->arena->capacity) &&                  /* entry 1's */
           p->object_size > 0 && p->used <= p->capacity &&
           p->capacity <= CANON_USIZE_MAX / p->object_size &&
           p->base_mark <= p->end_mark && p->end_mark <= p->arena->capacity &&
           p->end_mark - p->base_mark == p->capacity * p->object_size &&
           !overlaps(p, sizeof(Pool), p->arena, sizeof(Arena)) && /* G5's */
           p->base_mark <= p->arena->offset;                       /* G7's */
}
/* The G4 pool repair's conjunct: the Pool outside its slots. */
static int pool_out_of_slots(const Pool* p) {
    return !overlaps(p, sizeof(Pool), p->arena->buffer + p->base_mark,
                     p->end_mark - p->base_mark);
}
static int say(const char* what, int holds) {
    printf("    %-62s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    unsigned char* block = malloc(1024);   /* the two Arenas */
    unsigned char* slots = malloc(256);    /* the real Arena's buffer; the Pool lies in it */
    unsigned char* dbuf  = malloc(256);    /* the decoy's buffer */
    unsigned char decoy_before[sizeof(Arena)], real_before[sizeof(Arena)];
    Arena *decoy, *real;
    Pool* pool;
    size_t off;
    int bad = 0;

    if (!block || !slots || !dbuf) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    /* The decoy at the block's first 256-aligned byte, the real Arena 64
     * bytes on: their addresses differ in the low byte only. */
    off   = (size_t)((256u - ((uintptr_t)block & 255u)) & 255u);
    decoy = (Arena*)(void*)(block + off);
    real  = (Arena*)(void*)(block + off + 64u);

    arena_init(decoy, dbuf, 256);
    (void)arena_alloc(decoy, 100);         /* the decoy's offset 100 */
    arena_init(real, slots, 256);
    (void)arena_alloc(real, 80);           /* the real Arena's offset 80 */
    if (decoy->offset != 100u || real->offset != 80u) { return 3; }

    /* The Pool at slot 0 of its slots [16, 80): one-byte objects. */
    pool = (Pool*)(void*)(slots + 16);
    pool->arena       = real;
    pool->object_size = 1;
    pool->capacity    = 64;
    pool->used        = 1;
    pool->base_mark   = 16;
    pool->end_mark    = 80;

    printf("sizeof(Arena) %zu, sizeof(Pool) %zu; the real Arena 64 bytes past the decoy,"
           " the Pool at slots + 16\n", (size_t)sizeof(Arena), (size_t)sizeof(Pool));
    printf("at entry:\n");
    bad += say("pool_invariant but the pool repair's conjunct", pool_ok_but_repair(pool));
    printf("    %-62s %s\n", "the pool repair's conjunct: the Pool outside its slots",
           pool_out_of_slots(pool) ? "holds" : "VIOLATED (the excluded placement)");
    if (!pool_ok_but_repair(pool)) { printf("NOT admitted\n"); return 4; }

    memcpy(decoy_before, decoy, sizeof decoy_before);
    memcpy(real_before, real, sizeof real_before);
    pool_reset_secure(pool);

    printf("after pool_reset_secure(pool):\n");
    printf("    pool->arena is %s; the decoy's offset is %zu (it was 100); the real Arena %s\n",
           pool->arena == decoy ? "the decoy" : pool->arena == real ? "unchanged" : "elsewhere",
           (size_t)decoy->offset,
           memcmp(real_before, real, sizeof real_before) == 0 ? "unchanged" : "changed");
    bad += say("ensures pool->used == 0 (secure_ensures)", pool->used == 0u);
    bad += say("ensures pool_invariant(pool), but the repair's conjunct",
               pool_ok_but_repair(pool));
    bad += say("ensures pool_invariant(pool): the repair's conjunct",
               pool_out_of_slots(pool));
    bad += say("assigns: the decoy, outside the clause, unchanged",
               memcmp(decoy_before, decoy, sizeof decoy_before) == 0);
    free(dbuf);
    free(slots);
    free(block);
    return bad ? 1 : 0;
}
