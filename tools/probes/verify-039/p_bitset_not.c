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

/* VERIFY-039 G8 -- docs/deviations.md: bitset_not's postcondition.
 *
 * bitset_not's live behaviour ensures
 *     \forall integer k; 0 <= k < bs->word_count ==>
 *         bs->words[k] == ~\old(bs->words[k]);
 * and its loop carries the same equation as an invariant. In ACSL a C
 * integer is promoted to a mathematical integer and ~x is -x - 1 (ACSL
 * 1.18, 2.2.4 and 2.2.4.6), so for a u64 word the right-hand side is
 * negative and the left-hand side is not: as written, the postcondition is
 * false for every call. C cannot evaluate that reading, so this probe
 * checks the reading the comment intends, with the complement taken in u64:
 * bs->words[k] == (u64)~old[k]. It holds for every word but, possibly, the
 * last, which bitset_not hands to bitset_clear_padding after the loop: for a
 * capacity that is not a multiple of 64, the bits from capacity % 64 up are
 * cleared again, so the last word is the complement only if all of them
 * were set on entry, which bitset_pad excludes and bitset_not does not
 * require.
 *
 * Two calls on freshly initialised Bitsets, all bits clear: capacity 100
 * (two words, the last holding 36 bits) and capacity 128 (two full words).
 * bitset_mut and bitset_pad hold before each call, so both are admitted.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_bitset_not.c -o p && ./p
 * Expected: at capacity 100, words[0] is the complement and words[1] is
 * VIOLATED (0x0000000fffffffff, not 0xffffffffffffffff); at capacity 128
 * both hold; bitset_pad holds after both calls; exit 1, sanitizers clean.
 * The same without the two defines (the default build).
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "data/bitset.h"

/* bitset_mut's conjuncts that C can check (\valid aside). */
static int mut_ok(const Bitset* bs) {
    return bs->words != NULL && bs->capacity > 0 &&
           bs->capacity <= CANON_USIZE_MAX - 63u &&
           bs->word_count == (bs->capacity + 63u) / 64u;
}

/* bitset_pad, as the predicate states it. */
static int pad_ok(const Bitset* bs) {
    return bs->capacity % 64u == 0u ||
           (bs->words[bs->word_count - 1u] >> (bs->capacity % 64u)) == 0u;
}

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

static int run(usize capacity) {
    u64    words[2];
    u64    old[2];
    Bitset bs;
    usize  k;
    int    bad = 0;

    bitset_init(&bs, words, capacity);
    printf("capacity %zu, %zu words\n", (size_t)capacity, (size_t)bs.word_count);
    (void)say("before bitset_not: bitset_mut(bs)", mut_ok(&bs));
    (void)say("before bitset_not: bitset_pad(bs)", pad_ok(&bs));
    for (k = 0; k < bs.word_count; k++) { old[k] = bs.words[k]; }

    bitset_not(&bs);

    for (k = 0; k < bs.word_count; k++) {
        char what[64];
        int  holds = bs.words[k] == (u64)~old[k];
        (void)snprintf(what, sizeof what, "ensures 1 in u64, k = %zu", (size_t)k);
        bad += say(what, holds);
        if (!holds) {
            printf("      words[%zu] = 0x%016llx, (u64)~old = 0x%016llx\n",
                   (size_t)k, (unsigned long long)bs.words[k],
                   (unsigned long long)(u64)~old[k]);
        }
    }
    bad += say("after bitset_not: bitset_pad(bs)", pad_ok(&bs));
    return bad;
}

int main(void) {
    int bad = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    bad += run(100u);
    bad += run(128u);
    return bad ? 1 : 0;
}
