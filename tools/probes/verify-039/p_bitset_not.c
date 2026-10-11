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
 * Until the G8 repair, bitset_not's live behaviour ensured
 *     \forall integer k; 0 <= k < bs->word_count ==>
 *         bs->words[k] == ~\old(bs->words[k]);
 * and its loop carried the same equation as an invariant. In ACSL a C
 * integer is promoted to a mathematical integer and ~x is -x - 1 (ACSL
 * 1.18, 2.2.4 and 2.2.4.6), so for a u64 word the right-hand side is
 * negative and the left-hand side is not: as written, the postcondition was
 * false for every call. C cannot evaluate that reading, so the first check
 * below reads it as the comment intended, with the complement taken in u64:
 * bs->words[k] == (u64)~old[k]. It holds for every word but, possibly, the
 * last, which bitset_not hands to bitset_clear_padding after the loop: for a
 * capacity that is not a multiple of 64, the bits from capacity % 64 up are
 * cleared again, so the last word is the complement only if all of them
 * were set on entry, which bitset_pad excludes and bitset_not does not
 * require.
 *
 * Since the repair, the contract takes the complement in u64 and states
 * the last word as bitset_clear_padding leaves it, which
 * bitset_clear_padding now states too: unchanged when capacity % 64 == 0,
 * and otherwise its old value and ((u64)1 << (capacity % 64)) - 1. The
 * second and third checks are those clauses: bitset_not's and
 * bitset_clear_padding's.
 *
 * Each capacity from 1, 63, 64, 65, 100, 127, 128 and 129, with three
 * patterns written into the words of a freshly initialised Bitset: all
 * clear, every padding bit set (bitset_pad false, bitset_mut true, so the
 * call is admitted), and a fixed mixed pattern.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_bitset_not.c -o p && ./p
 * Expected: the reading in u64 of the old postcondition VIOLATED for the
 * last word at every capacity that is not a multiple of 64, except when
 * every padding bit was set on entry, and holding at 64 and 128; the
 * repaired clauses of both functions, and bitset_pad after bitset_not,
 * hold in all 48 cases; exit 0, sanitizers clean. The same without the two
 * defines (the default build). The exit status counts the repaired clauses
 * only; the old reading's violations are the finding.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "data/bitset.h"

#define MAXW 3u

/* bitset_mut's conjuncts that C can check (\valid and \separated aside). */
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

/* The mask of the repaired clauses, for capacity % 64 != 0. */
static u64 last_mask(usize capacity) {
    return ((u64)1u << (capacity % 64u)) - 1u;
}

/* Pattern 0: all clear. 1: every padding bit set, the rest clear (at a
 * capacity that is a multiple of 64 there is none, and it is pattern 0).
 * 2: a fixed mixed pattern. */
static void fill(Bitset* bs, int pattern) {
    usize k;
    for (k = 0; k < bs->word_count; k++) {
        bs->words[k] = pattern == 2
            ? (u64)0x0123456789abcdefULL ^ ((u64)(k + 1u) * (u64)0x9e3779b97f4a7c15ULL)
            : 0u;
    }
    if (pattern == 1 && bs->capacity % 64u != 0u) {
        bs->words[bs->word_count - 1u] = (u64)~last_mask(bs->capacity);
    }
}

static const char* const pattern_name[3] = { "all clear", "padding set", "mixed" };

static int run(usize capacity, int pattern, unsigned* old_violations) {
    u64    words[MAXW];
    u64    old[MAXW];
    Bitset bs;
    usize  k, last;
    int    bad = 0, old_ok = 1, not_ok = 1, cp_ok = 1;

    bitset_init(&bs, words, capacity);
    fill(&bs, pattern);
    last = bs.word_count - 1u;
    if (!mut_ok(&bs)) {
        printf("capacity %3zu, %-11s: bitset_mut VIOLATED before the call\n",
               (size_t)capacity, pattern_name[pattern]);
        return 1;
    }

    /* bitset_not */
    for (k = 0; k < bs.word_count; k++) { old[k] = bs.words[k]; }
    bitset_not(&bs);
    for (k = 0; k < bs.word_count; k++) {
        if (bs.words[k] != (u64)~old[k]) { old_ok = 0; }
    }
    for (k = 0; k < last; k++) {
        if (bs.words[k] != (u64)~old[k]) { not_ok = 0; }
    }
    if (capacity % 64u == 0u) {
        if (bs.words[last] != (u64)~old[last]) { not_ok = 0; }
    } else {
        if (bs.words[last] != ((u64)~old[last] & last_mask(capacity))) { not_ok = 0; }
    }
    if (!pad_ok(&bs)) { not_ok = 0; }

    /* bitset_clear_padding, on the same pattern */
    fill(&bs, pattern);
    for (k = 0; k < bs.word_count; k++) { old[k] = bs.words[k]; }
    bitset_clear_padding(&bs);
    for (k = 0; k < last; k++) {
        if (bs.words[k] != old[k]) { cp_ok = 0; }
    }
    if (capacity % 64u == 0u) {
        if (bs.words[last] != old[last]) { cp_ok = 0; }
    } else {
        if (bs.words[last] != (old[last] & last_mask(capacity))) { cp_ok = 0; }
    }

    printf("capacity %3zu, %-11s: old reading in u64 %-8s  bitset_not repaired %-8s  "
           "bitset_clear_padding %s\n",
           (size_t)capacity, pattern_name[pattern], old_ok ? "holds" : "VIOLATED",
           not_ok ? "holds" : "VIOLATED", cp_ok ? "holds" : "VIOLATED");
    if (!old_ok) { (*old_violations)++; }
    bad += !not_ok;
    bad += !cp_ok;
    return bad;
}

int main(void) {
    static const usize caps[] = { 1u, 63u, 64u, 65u, 100u, 127u, 128u, 129u };
    unsigned old_violations = 0;
    int      bad = 0;
    size_t   c;
    int      p;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (c = 0; c < sizeof caps / sizeof caps[0]; c++) {
        for (p = 0; p < 3; p++) { bad += run(caps[c], p, &old_violations); }
    }
    printf("old reading VIOLATED in %u of 24 cases; repaired clauses VIOLATED in %d of 48\n",
           old_violations, bad);
    return bad ? 1 : 0;
}
