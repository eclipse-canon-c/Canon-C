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

/* VERIFY-039 G4, listing entry 3, and G6 -- docs/deviations.md: a Bitset
 * whose word_count field is its own first word.
 *
 * bitset_mut does not separate *bs from bs->words, and bitset_clear_all and
 * bitset_set_all require only bitset_mut(bs) (bitset_init requires the
 * separation, but nothing obliges a Bitset to come from bitset_init). The
 * two functions write the words through `char *`: mem_zero and mem_set. In
 * WP's typed model a `char` write cannot reach the Bitset's fields, so
 * clear_all's `ensures bitset_mut(bs)` proves, and so does set_all's call
 * to bitset_clear_padding, which requires bitset_mut(bs).
 *
 * Here the Bitset lies at the start of a heap block and its words array
 * starts at the Bitset's word_count field, so that word_count lies in
 * words[0]. The capacity is a multiple of 64, so bitset_clear_padding
 * writes nothing and the state after bitset_set_all is the state at that
 * call. In this part every access is defined C: the block is heap
 * storage, the clobbering writes are character-type writes, and nothing
 * reads words[0] as a u64.
 *
 * G6, the second part: with glibc on LP64, and in the x86_64 machdep WP
 * uses, u64 and usize are both unsigned long, so bitset_set, bitset_clear
 * and bitset_toggle read and store a word over word_count through the same
 * type, defined C, with no character write. Their ensures bitset_mut(bs), pinned
 * residuals, are false; so is bitset_assign's, which WP proves from their
 * contracts. Where u64 and usize are different types (ILP32; macOS, where
 * u64 is unsigned long long), the compound assignment would read
 * word_count through u64, which is undefined, and the part is skipped. The check is
 * __builtin_types_compatible_p (gcc, clang); other compilers skip it.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_bitset_self.c -o p && ./p
 * Expected: bitset_mut holds before each call and is VIOLATED after it
 * (word_count 0 after clear_all, all ones after set_all); on LP64 Linux,
 * also after set, clear, toggle and both assigns (word_count 3 or 0);
 * exit 1, sanitizers clean. The same without the two defines (the default
 * build).
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "data/bitset.h"

#if defined(__GNUC__)
#  define G6_SAME_TYPE __builtin_types_compatible_p(u64, usize)
#else
#  define G6_SAME_TYPE 0
#endif

/* bitset_mut's conjuncts that C can check (\valid aside). */
static int mut_ok(const Bitset* bs) {
    return bs->words != NULL && bs->capacity > 0 &&
           bs->capacity <= CANON_USIZE_MAX - 63u &&
           bs->word_count == (bs->capacity + 63u) / 64u;
}

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

/* The Bitset at the start of the block, its words starting at word_count. */
static Bitset* self_hosted(unsigned char* block) {
    Bitset* bs = (Bitset*)(void*)block;
    u64* words = (u64*)(void*)(block + offsetof(Bitset, word_count));
    bs->words = words;
    bs->capacity = 128u;
    bs->word_count = 2u;        /* inside words[0] */
    words[1] = 0u;
    return bs;
}

int main(void) {
    unsigned char* block = malloc(offsetof(Bitset, word_count) + 2u * sizeof(u64));
    Bitset* bs;
    int bad = 0;

    if (block == NULL) { return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("sizeof(Bitset) = %zu; words start at the Bitset + %zu (its word_count)\n",
           (size_t)sizeof(Bitset), (size_t)offsetof(Bitset, word_count));

    bs = self_hosted(block);
    printf("bitset_clear_all(bs), capacity 128, two words:\n");
    bad += say("before the call: bitset_mut(bs)", mut_ok(bs));
    bitset_clear_all(bs);
    printf("      word_count is now %zu\n", (size_t)bs->word_count);
    bad += say("ensures bitset_mut(bs)", mut_ok(bs));

    bs = self_hosted(block);
    printf("bitset_set_all(bs), capacity 128, two words:\n");
    bad += say("before the call: bitset_mut(bs)", mut_ok(bs));
    bitset_set_all(bs);
    printf("      word_count is now %#zx\n", (size_t)bs->word_count);
    bad += say("at the clear_padding call: requires bitset_mut(bs)", mut_ok(bs));
    bad += say("ensures bitset_mut(bs)", mut_ok(bs));

    if (!G6_SAME_TYPE) {
        printf("G6 part: not applicable, u64 and usize are different types here\n");
    } else {
        static const struct { const char* call; int which; } ops[] = {
            {"bitset_set(bs, 0)", 0}, {"bitset_clear(bs, 1)", 1},
            {"bitset_toggle(bs, 0)", 2}, {"bitset_assign(bs, 0, true)", 3},
            {"bitset_assign(bs, 1, false)", 4}};
        size_t k;
        printf("G6: word stores over word_count (same type here):\n");
        for (k = 0; k < sizeof ops / sizeof ops[0]; k++) {
            char what[96];
            bs = self_hosted(block);
            switch (ops[k].which) {
                case 0: bitset_set(bs, 0u); break;
                case 1: bitset_clear(bs, 1u); break;
                case 2: bitset_toggle(bs, 0u); break;
                case 3: bitset_assign(bs, 0u, true); break;
                default: bitset_assign(bs, 1u, false); break;
            }
            (void)snprintf(what, sizeof what, "%s: ensures bitset_mut(bs), word_count %zu",
                           ops[k].call, (size_t)bs->word_count);
            bad += say(what, mut_ok(bs));
        }
    }

    free(block);
    return bad ? 1 : 0;
}
