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

/* VERIFY-036 F3 -- docs/deviations.md, VERIFY-009 Category 2a (ptr_span call-site requires, 4 goals).
 *
 * arena_alloc calls ptr_span(aligned_ptr, current) BEFORE its capacity guard.
 * Whenever offset + pad > capacity -- an ordinary failing allocation on a
 * nearly full arena, not only offset == capacity -- aligned_ptr lies beyond
 * one-past-the-end of the caller's buffer: ptr_span's \valid_read((char*)to)
 * is false, and the pointer subtraction inside ptr_span is outside C99 6.5.6p9.
 *
 * Build from the repository root:
 *   gcc -std=c99 -g -fsanitize=address,pointer-subtract -I. tools/probes/verify-036/p_arena_span.c -o p
 *   ASAN_OPTIONS=detect_invalid_pointer_pairs=2 ./p
 * Expected: AddressSanitizer: invalid-pointer-pair in ptr_span <- arena_alloc.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <stdlib.h>
#include "core/arena.h"

int main(void) {
    Arena a;
    unsigned char* buf = malloc(100);          /* object of exactly 100 bytes */
    if (!buf) { return 1; }
    arena_init(&a, buf, 100);
    (void)arena_alloc(&a, 97);                 /* arena now nearly full */
    printf("offset = %zu of 100; requesting 1 byte (will fail)\n", (size_t)a.offset);
    fflush(stdout);
    void* r = arena_alloc(&a, 1);              /* ptr_span runs before the guard */
    printf("arena_alloc(1) returned %s\n", r ? "non-NULL" : "NULL");
    free(buf);
    return 0;
}
