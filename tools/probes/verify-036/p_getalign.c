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

/* VERIFY-036 F1 -- docs/deviations.md, VERIFY-008 Category 2 -- the residual
 * typed_cast_mem_get_alignment_assert_rte_signed_overflow.
 *
 * The record attributes it to the cast round-trip losing integer bounds.
 * But -(intptr_t)addr really overflows when addr has only its top bit set.
 * On 32-bit targets that address is 0x80000000, an ordinary user-space
 * address. (Fix: negate in unsigned arithmetic, e.g. addr & (0u - addr).)
 *
 * Build from the repository root:
 *   gcc -std=c99 -fsanitize=signed-integer-overflow -I. tools/probes/verify-036/p_getalign.c -o p && ./p
 * Expected: UBSan "negation of ... cannot be represented".
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <stdint.h>
#include "core/memory.h"

int main(void) {
    const void* p = (const void*)((uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1));
    printf("mem_get_alignment(%p) = %zu\n", p, (size_t)mem_get_alignment(p));
    return 0;
}
