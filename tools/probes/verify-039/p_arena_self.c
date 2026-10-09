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

/* VERIFY-039 G4, listing entry 1 -- docs/deviations.md: an Arena that lives
 * inside its own buffer.
 *
 * arena_invariant does not separate *arena from arena->buffer, and no arena
 * contract requires it, so an Arena placed inside the block it manages is
 * admitted. Three functions write that block through `char *`:
 * arena_alloc_zero and arena_alloc_aligned_zero (mem_zero over the new
 * allocation) and arena_reset_secure (mem_secure_zero over the used bytes).
 * In WP's typed model a `char` write cannot reach the Arena's fields, so
 * their postconditions about those fields prove (VERIFY-039 G4). In C the
 * write zeroes the Arena itself when it lies in the written range.
 *
 * The same holds for a client. stringbuf_init_arena takes a block from the
 * arena and stores '\0' into it through `char *`; its `arena_kept` ensures
 * arena_invariant(arena). When that block starts at the Arena's capacity
 * field, the store zeroes the field's first byte, which on a little-endian
 * target (all of the project's) is its low byte: 255 becomes 0.
 *
 * Each case puts the Arena inside a 256-byte heap block (no declared type,
 * so every access is defined C), initializes it over the block, and makes
 * the call so that the written range covers the Arena. Every call met its
 * function's contract until the G4 arena repair; since then arena_init
 * requires the Arena to lie outside its buffer, so these calls are outside
 * its contract. The run-time behaviour is unchanged.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_arena_self.c -o p && ./p
 * Expected: all eight listed postconditions VIOLATED; exit 1, sanitizers
 * clean. The placements are computed from offsetof and CANON_DEFAULT_ALIGN,
 * so they hold on LP64 and ILP32 alike.
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/arena.h"
#include "data/stringbuf.h"

/* arena_invariant's conjuncts that C can check (\valid aside). */
static int invariant(const Arena* a) {
    return a->buffer != NULL && a->capacity > 0 &&
           a->capacity <= CANON_ARENA_MAX_SIZE && a->offset <= a->capacity;
}

static Arena* self_hosted(unsigned char* block) {
    Arena* a = (Arena*)(void*)(block + 128);   /* inside the block it manages */
    arena_init(a, block, 256);
    return a;
}

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

/* the address ensures: (u8*)result == (u8*)arena->buffer + (arena->offset - size) */
static int address_holds(const Arena* a, const void* p, usize size) {
    return p != NULL && a->buffer != NULL && a->offset >= size &&
           (const u8*)p == a->buffer + (a->offset - size);
}

int main(void) {
    unsigned char* block = malloc(256);
    Arena* a;
    void* p;
    u8* old_buffer;
    usize old_capacity;
    StringBuf sb;
    int bad = 0;

    if (block == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Arena at block + 128, sizeof(Arena) = %zu, buffer = block[0 .. 255]\n",
           (size_t)sizeof(Arena));

    a = self_hosted(block);
    p = arena_alloc_zero(a, 200);
    printf("arena_alloc_zero(a, 200):\n");
    bad += say("ensures arena_invariant(arena)", invariant(a));
    bad += say("ensures (u8*)\\result == buffer + (offset - size)", address_holds(a, p, 200));

    a = self_hosted(block);
    p = arena_alloc_aligned_zero(a, 200, 16);
    printf("arena_alloc_aligned_zero(a, 200, 16):\n");
    bad += say("ensures arena_invariant(arena)", invariant(a));
    bad += say("ensures (u8*)\\result == buffer + (offset - size)", address_holds(a, p, 200));

    a = self_hosted(block);
    (void)arena_alloc(a, 200);
    old_buffer = a->buffer;
    old_capacity = a->capacity;
    arena_reset_secure(a);
    printf("arena_reset_secure(a), after arena_alloc(a, 200):\n");
    bad += say("ensures arena->buffer == \\old(arena->buffer)", a->buffer == old_buffer);
    bad += say("ensures arena->capacity == \\old(arena->capacity)", a->capacity == old_capacity);
    bad += say("ensures arena_invariant(arena)", invariant(a));

    {
        /* arena_alloc puts its first block at block, rounded up to
         * CANON_DEFAULT_ALIGN, and after 128 bytes the next block starts
         * 128 bytes further on. Place the Arena so that its capacity field
         * starts exactly there. */
        u8* first = (u8*)(((uintptr_t)block + (uintptr_t)CANON_DEFAULT_ALIGN - 1u) &
                          ~((uintptr_t)CANON_DEFAULT_ALIGN - 1u));
        u8* next  = first + 128;
        a = (Arena*)(void*)(next - offsetof(Arena, capacity));
        arena_init(a, block, 255);
        (void)arena_alloc(a, 128);
        (void)stringbuf_init_arena(&sb, a, 64);
        printf("stringbuf_init_arena(&sb, a, 64), its block at &a->capacity (%s):\n",
               (u8*)sb.data == (u8*)&a->capacity ? "as placed" : "NOT as placed");
        bad += say("ensures arena_kept: arena_invariant(arena)", invariant(a));
    }

    free(block);
    printf("%d of 8 postconditions violated\n", bad);
    return bad ? 1 : 0;
}
