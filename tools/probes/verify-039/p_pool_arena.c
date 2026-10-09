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

/* VERIFY-039 G5 -- docs/deviations.md: a Pool that overlaps its Arena.
 *
 * pool_init requires \valid(pool) and arena_invariant(arena), and that the
 * Pool lie outside the arena's free tail; until the G5 repair nothing
 * separated *pool from *arena. Since the repair, pool_init requires that
 * too, so the call below is outside its contract; its run-time behaviour is
 * unchanged. pool_init stores the Pool's first four fields, calls arena_alloc,
 * then stores base_mark and end_mark. Here the Arena lies at the Pool's
 * base_mark field in one heap block, so that base_mark and end_mark are the
 * bytes of the Arena's buffer and capacity fields. The four stores before
 * the call miss the Arena, arena_alloc runs on an intact Arena, and the two
 * stores after it overwrite buffer with the region's offset (0) and
 * capacity with the arena's new offset. pool_init returns true, and the
 * Arena can no longer be used: its buffer field holds bytes stored as a
 * usize, and reading it through its own type would be undefined (C11
 * 6.5p7). Read as bytes it is a null pointer, so on that reading the first
 * postcondition, pool_invariant(pool), is false: arena_invariant requires
 * the buffer to be valid.
 *
 * Every access is defined C: the block is heap storage, each Arena field the
 * library reads after a Pool store was last stored with a compatible type,
 * and the probe reads the overwritten fields only through memcpy and
 * memcmp. The placement is computed from offsetof, so it holds on LP64 and
 * ILP32 alike.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_pool_arena.c -o p && ./p
 * Expected: pool_init returns true; ensures 2 and 3 hold; ensures 1,
 * pool_invariant(pool), VIOLATED on the overwritten bytes; exit 1,
 * sanitizers clean. The same without the two defines (the default build).
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/arena.h"
#include "core/pool.h"

typedef char g5_marks_cover_buffer_and_capacity[
    (offsetof(Pool, end_mark) - offsetof(Pool, base_mark) ==
     offsetof(Arena, capacity) - offsetof(Arena, buffer)) ? 1 : -1];

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    /* The Pool first, the Arena at its base_mark field. */
    const size_t at = offsetof(Pool, base_mark) - offsetof(Arena, buffer);
    unsigned char* block = malloc(at + sizeof(Arena) + sizeof(Pool));
    unsigned char* slots = malloc(256);
    Pool*  pool;
    Arena* arena;
    u8*    buffer_field;
    usize  capacity_before;
    usize  capacity_after;
    int    ok;
    int    bad = 0;

    if (block == NULL || slots == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    pool  = (Pool*)(void*)block;
    arena = (Arena*)(void*)(block + at);
    printf("sizeof(Pool) = %zu, sizeof(Arena) = %zu; the Arena at Pool + %zu\n",
           (size_t)sizeof(Pool), (size_t)sizeof(Arena), at);

    arena_init(arena, slots, 256);
    capacity_before = arena->capacity;

    ok = pool_init(pool, arena, 32, 4);
    printf("pool_init(pool, arena, 32, 4) returned %s\n", ok ? "true" : "false");
    if (!ok) { return 3; }

    /* buffer and capacity were last stored through pool->base_mark and
     * pool->end_mark: read their bytes. */
    memcpy(&buffer_field, &arena->buffer, sizeof buffer_field);
    memcpy(&capacity_after, &arena->capacity, sizeof capacity_after);
    printf("  Arena after the call: buffer field %s, capacity %zu (was %zu)\n",
           memcmp(&arena->buffer, &slots, sizeof slots) == 0 ? "unchanged" : "overwritten",
           (size_t)capacity_after, (size_t)capacity_before);

    bad += say("ensures 2: pool->used == 0", pool->used == 0u);
    bad += say("ensures 3: pool->capacity == max_objects", pool->capacity == 4u);
    bad += say("ensures 1: pool_invariant(pool), on the bytes",
               buffer_field != NULL &&
               memcmp(&arena->buffer, &slots, sizeof slots) == 0);

    free(slots);
    free(block);
    return bad ? 1 : 0;
}
