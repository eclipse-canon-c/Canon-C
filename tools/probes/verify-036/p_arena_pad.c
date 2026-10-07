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

/* VERIFY-036 F4 -- docs/deviations.md, VERIFY-009 Category 2b (arena fits / does_not_fit, 16 goals).
 *
 * The block's argument rests on "the C pad equals the ACSL pad". The ACSL
 * predicate arena_can_fit computes pad from the OFFSET; the C body computes
 * it from the ADDRESS (ptr_align_up(buffer + offset)). They agree only when
 * the buffer is aligned to the requested alignment, and neither arena_init
 * nor arena_invariant requires that. Each case below satisfies the contract's
 * precondition and violates the selected behaviour's postcondition.
 *
 * Build from the repository root:
 *   gcc -std=c99 -Wall -Wextra -I. tools/probes/verify-036/p_arena_pad.c -o p && ./p
 * Same result under the proof configuration: add -DCANON_NO_REQUIRE -DNDEBUG.
 *
 * Since F4 (e953219) arena_can_fit pads from the address. The probe checks
 * both predicates: the pre-F4 one must report three VIOLATED lines, the
 * current one three ok lines. It is now a regression test for F4.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <stdint.h>
#include "core/arena.h"

/* arena_can_fit before F4: pad computed from the offset */
static int acsl_can_fit_pre_f4(const Arena* a, usize size, usize alignment) {
    usize cur = a->offset;
    usize pad = (alignment - (cur % alignment)) % alignment;
    return cur <= CANON_USIZE_MAX - pad &&
           cur + pad <= CANON_USIZE_MAX - size &&
           cur + pad + size <= a->capacity;
}

/* arena_can_fit since F4, transcribed from core/arena.h: pad from the address */
static int acsl_can_fit(const Arena* a, usize size, usize alignment) {
    usize cur  = a->offset;
    usize addr = (usize)(uintptr_t)(a->buffer + cur);
    usize pad  = ((addr + alignment - 1) & ~(alignment - 1)) - addr;
    return cur <= CANON_USIZE_MAX - pad &&
           cur + pad <= CANON_USIZE_MAX - size &&
           cur + pad + size <= a->capacity;
}

static _Alignas(64) unsigned char storage[512];

static void report(const char* label, const Arena* a, usize size, usize al, void* r) {
    int old_fits = acsl_can_fit_pre_f4(a, size, al);
    int fits     = acsl_can_fit(a, size, al);
    printf("%-34s code returned %-8s  pre-F4 contract: %-8s  current contract: %s\n",
           label, r ? "non-NULL" : "NULL",
           (old_fits == (r != NULL)) ? "ok" : "VIOLATED",
           (fits == (r != NULL)) ? "ok" : "VIOLATED");
}

int main(void) {
    Arena a;
    usize D = (usize)CANON_DEFAULT_ALIGN;
    Arena snapshot;
    void* r;

    /* 1: buffer % 16 == 1, offset 0, size 100, capacity 100 */
    arena_init(&a, storage + 1, 100);
    snapshot = a; r = arena_alloc(&a, 100);
    report("1. arena_alloc, buffer%16==1", &snapshot, 100, D, r);

    /* 2: buffer % 16 == 15; one ordinary allocation leaves offset 17 */
    arena_init(&a, storage + 15, 100);
    (void)arena_alloc(&a, 16);
    snapshot = a; r = arena_alloc(&a, 83);
    report("2. arena_alloc, buffer%16==15", &snapshot, 83, D, r);

    /* 3: malloc-style 16-aligned buffer, alignment 64 requested */
    arena_init(&a, storage + 16, 256);
    snapshot = a; r = arena_alloc_aligned(&a, 256, 64);
    report("3. arena_alloc_aligned(.., 64)", &snapshot, 256, 64, r);
    return 0;
}
