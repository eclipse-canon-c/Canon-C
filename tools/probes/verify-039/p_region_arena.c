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

/* VERIFY-039 G20 -- docs/deviations.md: a Region that overlaps its Arena.
 *
 * region_invariant(r) stated \valid(r), the hook count's bound, and
 * arena_invariant(r->arena) for a non-null arena; region_attach_arena
 * required arena_invariant(arena). Until G20's repair nothing separated *r
 * from *r->arena, so a Region that overlaps its Arena was admitted, as a Pool
 * that overlapped its Arena was until G5's repair (p_pool_arena.c). Since the
 * repair (642ad8a) region_invariant separates the Region from an attached
 * Arena, and region_attach_arena requires the Arena it attaches to lie apart
 * from the Region, so the calls of cases 1 to 4 are outside the contracts;
 * cases 5 and 6 stay admitted. The checks below read the contracts as they
 * stood before the repair, and the probe's run-time behaviour is unchanged.
 * Each case below places the Arena in one heap block with the Region, on the
 * member a function stores:
 *
 *   1. region_set_parent, the Arena at the Region's parent member: the store
 *      of parent puts the parent's address in the Arena's buffer member, and
 *      the 256 bytes its capacity names are not valid there, so
 *      region_invariant(r), the second postcondition, is false.
 *   2. region_register, the Arena at slot 0: the stores of fn and ctx put
 *      the hook's address in its buffer and 0 in its capacity, and the
 *      postcondition region_invariant(r) is false.
 *   3. region_attach_arena, the Arena at the Region's arena member, its
 *      buffer the address of a second Arena, A2, which r->arena then names
 *      and which meets the invariant: the store of arena makes the Arena's
 *      buffer its own address, so the Arena is not separated from its
 *      buffer and region_invariant(r) is false.
 *   4. region_end, no hook called: the Arena's capacity, 1, at the Region's
 *      num_hooks (both usize), slot 0's fn null. The store num_hooks = 0
 *      makes the capacity 0 before arena_reset is called, so arena_reset's
 *      precondition, arena_invariant(saved arena), is false at the call.
 *      arena_reset writes only offset and padding_accum, so the capacity
 *      read after the call is the one the call was made with.
 *   5. region_end, the Arena over num_hooks as in case 4 but not attached:
 *      every precondition holds, region_invariant's arena part among them
 *      (r->arena is null), so G20's repair, which separates a Region from the
 *      Arena attached to it, still admits the call. The count reads 1 and
 *      slot 0's fn is null, so the loop visits a slot that holds no hook and
 *      takes the false side of its `if (h->fn != NULL)`, which MCDC-005 calls
 *      unreachable through the public API.
 *   6. region_end with no Arena and no overlap: hook A in slot 0, and hook B
 *      in slot 1, which re-opens its own Region with region_begin. The loop
 *      calls B first; B's region_begin nulls slot 0, so the loop then visits
 *      slot 0 with a null fn, takes the same false side, and A, registered,
 *      never runs. Every call meets its contract: both hooks are valid
 *      functions and keep the hook rule, B leaving slot 0, below its own,
 *      with a null fn.
 *
 * Every precondition as it stood before the repair holds before each call,
 * on the bytes. Every access is defined C. In cases 1 to 5 the block is heap
 * storage, each member the library reads was last stored through a
 * compatible type, and the probe reads the members a store of the other
 * struct overwrote only through memcpy, or through the type they were last
 * stored with (the count, a usize like the capacity stored on it, in cases 4
 * and 5); the placements are computed from offsetof, so they hold on LP64
 * and ILP32 alike. Case 6's Region is on the stack, with no Arena and no
 * overlap.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_region_arena.c -o p && ./p
 * Expected: in each case the preconditions, as they stood before the repair,
 * hold; the postconditions region_invariant(r) of cases 1 to 3, arena_reset's
 * precondition in case 4, in case 5 MCDC-005's claim, that every slot below
 * the count holds a hook, and in case 6 its claim that every slot the loop
 * visits holds one (hook A runs), VIOLATED; the other checks hold; exit 1,
 * sanitizers clean. The same without the two defines (the default build).
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/arena.h"
#include "core/region.h"

/* Case 3 needs the Arena's buffer member first, so that the Arena placed at
 * the Region's arena member has its buffer on r->arena's bytes. */
typedef char g20_arena_buffer_first[(offsetof(Arena, buffer) == 0u) ? 1 : -1];

static void hook(void* ctx) { (void)ctx; }

static int hook_a_ran = 0;
static void hook_a(void* ctx) { (void)ctx; hook_a_ran = 1; }
static void hook_b(void* ctx) { region_begin((Region*)ctx); }

static int say(const char* what, int holds) {
    printf("    %-66s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

/* arena_invariant as far as the probe can check it, on the bytes: capacity
 * above 0, offset within it, and the buffer and capacity the ones given. */
static int arena_bytes(const Arena* a, const void* want_buffer, usize want_cap) {
    void* buf;
    usize cap;
    usize off;
    memcpy(&buf, &a->buffer, sizeof buf);
    memcpy(&cap, &a->capacity, sizeof cap);
    memcpy(&off, &a->offset, sizeof off);
    printf("      Arena: buffer %p (given %p), capacity %zu, offset %zu\n",
           buf, want_buffer, (size_t)cap, (size_t)off);
    return cap > 0u && off <= cap && buf == want_buffer && cap == want_cap;
}

int main(void) {
    int bad = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("sizeof(Region) = %zu, sizeof(Arena) = %zu\n",
           (size_t)sizeof(Region), (size_t)sizeof(Arena));

    /* 1. region_set_parent: the Arena at the Region's parent member. */
    {
        unsigned char* block = malloc(sizeof(Region) + sizeof(Arena));
        unsigned char* slots = malloc(256);
        Region* parent = malloc(sizeof(Region));
        Region* r;
        Arena* a;
        if (block == NULL || slots == NULL || parent == NULL) { return 2; }
        r = (Region*)(void*)block;
        a = (Arena*)(void*)(block + offsetof(Region, parent));
        region_begin(r);
        region_begin(parent);
        arena_init(a, slots, 256);
        region_attach_arena(r, a);
        printf("1. region_set_parent, the Arena at Region + %zu (parent)\n",
               offsetof(Region, parent));
        bad += say("pre: region_invariant(r), arena_invariant on the bytes",
                   r->num_hooks == 0u && arena_bytes(a, slots, 256));
        region_set_parent(r, parent);
        bad += say("post 1: r->parent == parent", r->parent == parent);
        bad += say("post 2: region_invariant(r), arena_invariant on the bytes",
                   arena_bytes(a, slots, 256));
        free(parent);
        free(slots);
        free(block);
    }

    /* 2. region_register: the Arena at slot 0. */
    {
        unsigned char* block = malloc(sizeof(Region) + sizeof(Arena));
        unsigned char* slots = malloc(256);
        Region* r;
        Arena* a;
        bool ok;
        if (block == NULL || slots == NULL) { return 2; }
        r = (Region*)(void*)block;
        a = (Arena*)(void*)(block + offsetof(Region, cleanups));
        region_begin(r);
        arena_init(a, slots, 256);
        region_attach_arena(r, a);
        printf("2. region_register, the Arena at Region + %zu (slot 0)\n",
               offsetof(Region, cleanups));
        bad += say("pre: region_invariant(r), arena_invariant on the bytes",
                   r->num_hooks == 0u && arena_bytes(a, slots, 256));
        ok = region_register(r, hook, NULL);
        bad += say("post has_space: true returned, count 1", ok && r->num_hooks == 1u);
        bad += say("post: region_invariant(r), arena_invariant on the bytes",
                   arena_bytes(a, slots, 256));
        free(slots);
        free(block);
    }

    /* 3. region_attach_arena: the Arena at the Region's arena member, its
     * buffer, the bytes of r->arena, naming a second Arena, A2. */
    {
        unsigned char* block = malloc(sizeof(Region) + sizeof(Arena));
        unsigned char* slots = malloc(256);
        Arena* a2 = malloc(sizeof(Arena));
        Region* r;
        Arena* a;
        Arena* named;
        void* buf;
        if (block == NULL || slots == NULL || a2 == NULL) { return 2; }
        r = (Region*)(void*)block;
        a = (Arena*)(void*)(block + offsetof(Region, arena));
        region_begin(r);
        arena_init(a2, slots, 256);
        a->buffer = (u8*)(void*)a2;
        a->capacity = sizeof(Arena);
        a->offset = 0u;
        a->padding_accum = 0u;
        memcpy(&named, &r->arena, sizeof named);
        printf("3. region_attach_arena, the Arena at Region + %zu (arena); "
               "r->arena on the bytes %p, A2 %p\n",
               offsetof(Region, arena), (void*)named, (void*)a2);
        bad += say("pre: region_invariant(r): r->arena is A2, which meets it",
                   named == a2 && a2->capacity == 256u && a2->offset == 0u);
        bad += say("pre: arena_invariant(arena), its buffer A2, on the bytes",
                   arena_bytes(a, a2, sizeof(Arena)));
        region_attach_arena(r, a);
        bad += say("post 1: r->arena == arena", r->arena == a);
        memcpy(&buf, &a->buffer, sizeof buf);
        bad += say("post 2: region_invariant(r): the Arena outside its buffer",
                   buf != (void*)a);
        free(a2);
        free(slots);
        free(block);
    }

    /* 4. region_end, no hook called: the Arena's capacity at num_hooks. */
    {
        const size_t at = offsetof(Region, num_hooks) - offsetof(Arena, capacity);
        unsigned char* block = malloc(at + sizeof(Arena) + sizeof(Region));
        unsigned char* slots = malloc(16);
        Region* r;
        Arena* a;
        usize cap;
        if (block == NULL || slots == NULL) { return 2; }
        r = (Region*)(void*)block;
        a = (Arena*)(void*)(block + at);
        region_begin(r);
        arena_init(a, slots, 1);        /* capacity 1: r->num_hooks reads 1 */
        region_attach_arena(r, a);
        printf("4. region_end, the Arena at Region + %zu (its capacity at num_hooks); "
               "num_hooks %zu, slot 0's fn %s\n",
               at, (size_t)r->num_hooks, r->cleanups[0].fn != NULL ? "set" : "null");
        bad += say("pre: region_invariant(r), arena_invariant on the bytes",
                   r->num_hooks <= (usize)REGION_MAX_CLEANUP && arena_bytes(a, slots, 1));
        region_end(r);
        memcpy(&cap, &a->capacity, sizeof cap);
        bad += say("at the call: arena_reset requires arena_invariant (capacity > 0)",
                   cap > 0u);
        bad += say("post: closed, count 0, arena null",
                   !r->open && r->num_hooks == 0u && r->arena == NULL);
        free(slots);
        free(block);
    }

    /* 5. region_end, the Arena over num_hooks as in case 4, not attached. */
    {
        const size_t at = offsetof(Region, num_hooks) - offsetof(Arena, capacity);
        unsigned char* block = malloc(at + sizeof(Arena) + sizeof(Region));
        unsigned char* slots = malloc(16);
        Region* r;
        Arena* a;
        if (block == NULL || slots == NULL) { return 2; }
        r = (Region*)(void*)block;
        a = (Arena*)(void*)(block + at);
        region_begin(r);
        arena_init(a, slots, 1);        /* capacity 1: r->num_hooks reads 1 */
        printf("5. region_end, the Arena at Region + %zu as in case 4, not attached; "
               "num_hooks %zu, slot 0's fn %s\n",
               at, (size_t)r->num_hooks, r->cleanups[0].fn != NULL ? "set" : "null");
        bad += say("pre: region_invariant(r): count <= REGION_MAX_CLEANUP, arena null",
                   r->num_hooks <= (usize)REGION_MAX_CLEANUP && r->arena == NULL);
        bad += say("MCDC-005: every slot below the count holds a hook",
                   !(r->num_hooks >= 1u && r->cleanups[0].fn == NULL));
        region_end(r);
        bad += say("post: closed, count 0, arena null",
                   !r->open && r->num_hooks == 0u && r->arena == NULL);
        free(slots);
        free(block);
    }

    /* 6. region_end, no Arena, no overlap: hook B re-opens its own Region. */
    {
        Region r;
        region_begin(&r);
        if (!region_register(&r, hook_a, NULL)) { return 2; }
        if (!region_register(&r, hook_b, &r))   { return 2; }
        printf("6. region_end, hook A in slot 0, hook B in slot 1 re-opening its Region; "
               "count %zu\n", (size_t)r.num_hooks);
        bad += say("pre: region_invariant(r), every slot below the count holds a hook",
                   r.num_hooks == 2u && r.cleanups[0].fn != NULL && r.cleanups[1].fn != NULL);
        region_end(&r);
        bad += say("MCDC-005: every slot the loop visits holds a hook (hook A ran)",
                   hook_a_ran != 0);
        bad += say("post: closed, count 0, arena null",
                   !r.open && r.num_hooks == 0u && r.arena == NULL);
    }
    return bad ? 1 : 0;
}
