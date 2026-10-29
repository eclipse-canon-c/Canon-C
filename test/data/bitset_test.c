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

/**
 * @file bitset_test.c
 * @brief Tests for data/bitset.h — fixed-capacity bitset
 *
 * Covers:
 *   - bitset_init()               — valid init, all bits cleared
 *   - bitset_set/clear/toggle/    — O(1) single-bit ops, OOB fires contract,
 *     test/assign                   NULL is no-op / returns false
 *   - bitset_set_all/clear_all    — bulk fill; padding invariant holds
 *   - bitset_not                  — invert; padding stays zero
 *   - bitset_and/or/xor           — in-place word ops; mismatched sizes
 *   - bitset_count                — popcount across words
 *   - bitset_is_empty/is_full     — O(n/64) queries
 *   - bitset_is_disjoint          — overlap detection
 *   - bitset_capacity             — returns capacity, 0 for NULL
 *   - bitset_find_first/next/last — sentinel variants; BITSET_NPOS on empty
 *   - bitset_find_*_option        — typed variants; Some/None
 *   - BITSET_FOR_EACH             — iteration over set bits
 *   - bitset_as_bytes             — raw byte view; length = word_count * 8
 *   - Word-boundary cases         — 64-bit (1 word), 65-bit (2 words),
 *                                   128-bit (2 words), non-multiple-of-64
 *   - Padding invariant           — bits beyond capacity remain zero after
 *                                   set_all, not, xor
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - ref u64 tracks ground-truth bit state for a 64-bit bitset
 *   - Operations: set, clear, toggle, clear_all, set_all, not, and, or, xor
 *   - After every op: test(i) == (ref >> i) & 1 for all i in [0, 64)
 *   - count == popcount(ref)
 *   - is_empty == (ref == 0)
 *   - find_first matches __builtin_ctzll(ref) (or NPOS when ref == 0)
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "core/slice.h"
#include "semantics/option/option.h"

/* option_usize is instantiated BY bitset.h since 2026-08-22, behind the
 * CANON_OPTION_USIZE_DEFINED guard (vec_impl.h's convention). This file used
 * to carry a bare CANON_OPTION(usize) here, with a comment explaining that
 * bitset.h required its includer to pre-instantiate. That requirement is
 * gone: bitset.h is now self-contained, and a pre-instantiation here would
 * be a redefinition. To interpose a DIFFERENT option_usize — as
 * vmacros/vdrivers/bitset_verify.h does, with contracts — define
 * CANON_OPTION_USIZE_DEFINED first and emit your own. */

#include "data/bitset.h"

#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
   Unit test build
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                                  \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n",                      \
                    __FILE__, __LINE__, #cond);                       \
            g_failed++;                                               \
        }                                                             \
    } while (0)

/* ── init ────────────────────────────────────────────────────────────────── */

static void test_init(void)
{
    u64 words[BITSET_WORD_COUNT(128)];
    Bitset bs;
    bitset_init(&bs, words, 128);

    EXPECT(bs.capacity   == 128);
    EXPECT(bs.word_count == 2);
    EXPECT(bitset_count(&bs)    == 0);
    EXPECT(bitset_is_empty(&bs));
    EXPECT(!bitset_is_full(&bs));
    EXPECT(bitset_capacity(&bs) == 128);
}

/* ── set / clear / test / toggle / assign ───────────────────────────────── */

static void test_single_bit_ops(void)
{
    u64 words[BITSET_WORD_COUNT(64)];
    Bitset bs;
    bitset_init(&bs, words, 64);

    /* set and test */
    bitset_set(&bs, 0);
    bitset_set(&bs, 63);
    EXPECT(bitset_test(&bs, 0));
    EXPECT(bitset_test(&bs, 63));
    EXPECT(!bitset_test(&bs, 1));
    EXPECT(bitset_count(&bs) == 2);

    /* clear */
    bitset_clear(&bs, 0);
    EXPECT(!bitset_test(&bs, 0));
    EXPECT(bitset_count(&bs) == 1);

    /* toggle */
    bitset_toggle(&bs, 63); /* 1 → 0 */
    EXPECT(!bitset_test(&bs, 63));
    bitset_toggle(&bs, 63); /* 0 → 1 */
    EXPECT(bitset_test(&bs, 63));

    /* assign */
    bitset_assign(&bs, 5, true);
    EXPECT(bitset_test(&bs, 5));
    bitset_assign(&bs, 5, false);
    EXPECT(!bitset_test(&bs, 5));

    /* NULL is no-op / returns false */
    bitset_set(NULL, 0);
    bitset_clear(NULL, 0);
    bitset_toggle(NULL, 0);
    EXPECT(!bitset_test(NULL, 0));
    bitset_assign(NULL, 0, true);
}

/* ── set_all / clear_all / not — padding invariant ───────────────────────── */

static void test_bulk_ops_padding(void)
{
    /* Non-multiple-of-64: 70 bits — last word has 6 used bits, 58 padding */
    u64 words[BITSET_WORD_COUNT(70)];
    Bitset bs;
    bitset_init(&bs, words, 70);

    bitset_set_all(&bs);
    EXPECT(bitset_count(&bs) == 70);
    EXPECT(bitset_is_full(&bs));
    /* Padding bits in last word must be zero */
    EXPECT((words[1] >> 6) == 0);

    bitset_not(&bs);
    EXPECT(bitset_count(&bs) == 0);
    EXPECT(bitset_is_empty(&bs));
    /* After not of all-ones (70 bits), result must be zero */
    EXPECT((words[1] >> 6) == 0);

    bitset_set_all(&bs);
    bitset_clear_all(&bs);
    EXPECT(bitset_count(&bs) == 0);
    EXPECT(bitset_is_empty(&bs));

    /* NULL is no-op */
    bitset_set_all(NULL);
    bitset_clear_all(NULL);
    bitset_not(NULL);
}

/* ── and / or / xor ─────────────────────────────────────────────────────── */

static void test_bitwise_ops(void)
{
    u64 wa[BITSET_WORD_COUNT(64)];
    u64 wb[BITSET_WORD_COUNT(64)];
    Bitset a, b;
    bitset_init(&a, wa, 64);
    bitset_init(&b, wb, 64);

    /* a = {0, 1, 2}, b = {1, 2, 3} */
    bitset_set(&a, 0); bitset_set(&a, 1); bitset_set(&a, 2);
    bitset_set(&b, 1); bitset_set(&b, 2); bitset_set(&b, 3);

    /* OR: a |= b → {0,1,2,3} */
    bitset_or(&a, &b);
    EXPECT(bitset_count(&a) == 4);
    EXPECT(bitset_test(&a, 0) && bitset_test(&a, 3));

    /* AND: a &= b → {1,2,3} */
    bitset_and(&a, &b);
    EXPECT(bitset_count(&a) == 3);
    EXPECT(!bitset_test(&a, 0));

    /* XOR: a ^= b → {}, all cancel */
    bitset_xor(&a, &b);
    EXPECT(bitset_is_empty(&a));

    /* XOR padding invariant with 70-bit bitset */
    u64 wc[BITSET_WORD_COUNT(70)];
    u64 wd[BITSET_WORD_COUNT(70)];
    Bitset c, d;
    bitset_init(&c, wc, 70);
    bitset_init(&d, wd, 70);
    bitset_set_all(&c);
    bitset_set_all(&d);
    bitset_xor(&c, &d);
    EXPECT(bitset_is_empty(&c));
    EXPECT((wc[1] >> 6) == 0); /* padding still zero */

    /* NULL is no-op */
    bitset_and(NULL, &b);
    bitset_or(NULL, &b);
    bitset_xor(NULL, &b);
    bitset_and(&a, NULL);
    bitset_or(&a, NULL);
    bitset_xor(&a, NULL);
}

/* ── count / is_empty / is_full / is_disjoint ───────────────────────────── */

static void test_queries(void)
{
    u64 wa[BITSET_WORD_COUNT(64)];
    u64 wb[BITSET_WORD_COUNT(64)];
    Bitset a, b;
    bitset_init(&a, wa, 64);
    bitset_init(&b, wb, 64);

    EXPECT(bitset_is_empty(&a));
    EXPECT(!bitset_is_full(&a));
    EXPECT(bitset_count(&a) == 0);

    bitset_set_all(&a);
    EXPECT(bitset_is_full(&a));
    EXPECT(bitset_count(&a) == 64);
    EXPECT(!bitset_is_empty(&a));

    /* disjoint: a = all ones, b = empty → disjoint only when b is empty */
    EXPECT(bitset_is_disjoint(&a, &b)); /* empty b is disjoint with anything */
    bitset_set(&b, 0);
    EXPECT(!bitset_is_disjoint(&a, &b)); /* now they share bit 0 */

    /* disjoint: non-overlapping */
    bitset_clear_all(&a);
    bitset_clear_all(&b);
    bitset_set(&a, 5);
    bitset_set(&b, 10);
    EXPECT(bitset_is_disjoint(&a, &b));
    bitset_set(&b, 5);
    EXPECT(!bitset_is_disjoint(&a, &b));

    /* NULL */
    EXPECT(bitset_count(NULL) == 0);
    EXPECT(bitset_is_empty(NULL));
    EXPECT(!bitset_is_full(NULL));
    EXPECT(bitset_is_disjoint(NULL, &b));
    EXPECT(bitset_is_disjoint(&a, NULL));
    EXPECT(bitset_capacity(NULL) == 0);
}

/* ── find_first / find_next / find_last (sentinel) ──────────────────────── */

static void test_find_sentinel(void)
{
    u64 words[BITSET_WORD_COUNT(128)];
    Bitset bs;
    bitset_init(&bs, words, 128);

    /* Empty */
    EXPECT(bitset_find_first(&bs) == BITSET_NPOS);
    EXPECT(bitset_find_last(&bs)  == BITSET_NPOS);
    EXPECT(bitset_find_next(&bs, 0) == BITSET_NPOS);

    /* Single bit */
    bitset_set(&bs, 100);
    EXPECT(bitset_find_first(&bs) == 100);
    EXPECT(bitset_find_last(&bs)  == 100);
    EXPECT(bitset_find_next(&bs, 100) == BITSET_NPOS);

    /* Multiple bits spanning two words */
    bitset_set(&bs, 0);
    bitset_set(&bs, 63);
    bitset_set(&bs, 64);
    bitset_set(&bs, 127);

    EXPECT(bitset_find_first(&bs) == 0);
    EXPECT(bitset_find_last(&bs)  == 127);

    usize i = bitset_find_first(&bs);
    usize prev = 0;
    int   count = 0;
    while (i != BITSET_NPOS) {
        EXPECT(i >= prev);
        prev = i;
        count++;
        i = bitset_find_next(&bs, i);
    }
    EXPECT(count == 5); /* 0, 63, 64, 100, 127 */

    /* NULL */
    EXPECT(bitset_find_first(NULL) == BITSET_NPOS);
    EXPECT(bitset_find_last(NULL)  == BITSET_NPOS);
    EXPECT(bitset_find_next(NULL, 0) == BITSET_NPOS);
}

/* ── find_*_option (typed) ───────────────────────────────────────────────── */

static void test_find_option(void)
{
    u64 words[BITSET_WORD_COUNT(64)];
    Bitset bs;
    bitset_init(&bs, words, 64);

    EXPECT(option_usize_is_none(bitset_find_first_option(&bs)));
    EXPECT(option_usize_is_none(bitset_find_last_option(&bs)));
    EXPECT(option_usize_is_none(bitset_find_next_option(&bs, 0)));

    bitset_set(&bs, 7);
    bitset_set(&bs, 42);

    option_usize f = bitset_find_first_option(&bs);
    EXPECT(option_usize_is_some(f));
    EXPECT(option_usize_unwrap(f) == 7);

    option_usize l = bitset_find_last_option(&bs);
    EXPECT(option_usize_is_some(l));
    EXPECT(option_usize_unwrap(l) == 42);

    option_usize n = bitset_find_next_option(&bs, 7);
    EXPECT(option_usize_is_some(n));
    EXPECT(option_usize_unwrap(n) == 42);

    EXPECT(option_usize_is_none(bitset_find_next_option(&bs, 42)));

    /* Option-style iteration */
    int  icount = 0;
    option_usize cur = bitset_find_first_option(&bs);
    while (option_usize_is_some(cur)) {
        icount++;
        cur = bitset_find_next_option(&bs, option_usize_unwrap(cur));
    }
    EXPECT(icount == 2);

    /* NULL */
    EXPECT(option_usize_is_none(bitset_find_first_option(NULL)));
    EXPECT(option_usize_is_none(bitset_find_last_option(NULL)));
    EXPECT(option_usize_is_none(bitset_find_next_option(NULL, 0)));
}

/* ── BITSET_FOR_EACH ─────────────────────────────────────────────────────── */

static void test_for_each(void)
{
    u64 words[BITSET_WORD_COUNT(128)];
    Bitset bs;
    bitset_init(&bs, words, 128);

    usize expected[] = {1, 17, 64, 65, 127};
    usize n_expected = 5;

    for (usize k = 0; k < n_expected; k++) {
        bitset_set(&bs, expected[k]);
    }

    usize collected[8];
    usize nc = 0;
    BITSET_FOR_EACH(&bs, i) {
        collected[nc++] = i;
    }

    EXPECT(nc == n_expected);
    for (usize k = 0; k < n_expected; k++) {
        EXPECT(collected[k] == expected[k]);
    }

    /* Empty bitset — loop body must not execute */
    bitset_clear_all(&bs);
    nc = 0;
    BITSET_FOR_EACH(&bs, j) { nc++; }
    EXPECT(nc == 0);
}

/* ── as_bytes ────────────────────────────────────────────────────────────── */

static void test_as_bytes(void)
{
    u64 words[BITSET_WORD_COUNT(64)];
    Bitset bs;
    bitset_init(&bs, words, 64);

    bytes_t bv = bitset_as_bytes(&bs);
    EXPECT(bv.len == 8); /* 1 word * 8 bytes */

    /* VERIFY-020 F2 fallout. This used to carry a bare comment saying
     * "bv.ptr is always non-NULL here, bs.words is a stack array" --
     * an unchecked human assertion, and clang-analyzer disagreed the moment
     * F2 landed. bitset_as_bytes returns bytes_empty() (ptr == NULL) when
     * words == NULL. On that path the OLD bitset_set dereferenced words
     * unconditionally, so the analyzer's null-deref report landed inside
     * data/bitset.h -- non-user code, suppressed by -header-filter. With the
     * F2 guard, bitset_set returns early instead, the path survives into
     * THIS file, and the deref of bv.ptr[0] is reported where it is visible.
     *
     * The finding was always reachable. Fixing the UB moved the report from
     * a suppressed location to a reported one -- the same shape as the
     * MISRA per-line masking note: a count that rises after cleanup can be
     * surfacing rather than regression.
     *
     * str_view_test, stringbuf_test and slice_test all assert this before
     * indexing a view. bitset_test was the outlier. */
    EXPECT(bv.ptr != NULL);
    if (bv.ptr == NULL) { return; }

    /* The view is the words' object representation, in the host's byte
     * order (VERIFY-045 B1). On a little-endian host bit 0 is in byte 0
     * and bit 63 in byte 7; on a big-endian one, byte 7 and byte 0. The
     * test asked for the little-endian layout and failed on s390x. */
    {
        const u64 probe = 1u;
        const int little = (*(const unsigned char*)&probe == 1u);
        const usize lsb = little ? 0u : 7u;
        const usize msb = little ? 7u : 0u;

        bitset_set(&bs, 0);
        EXPECT(bv.ptr[lsb] == 1);

        bitset_set(&bs, 63);
        EXPECT(bv.ptr[msb] == 0x80);

        /* The byte order is the host's: the same bytes as the words. */
        EXPECT(memcmp(bv.ptr, words, sizeof words) == 0);
    }

    /* NULL */
    bytes_t nb = bitset_as_bytes(NULL);
    EXPECT(nb.ptr == NULL && nb.len == 0);
}

/* ── word-boundary cases ─────────────────────────────────────────────────── */

static void test_word_boundary(void)
{
    /* Exactly 64 bits — 1 word, no padding */
    {
        u64 words[1];
        Bitset bs;
        bitset_init(&bs, words, 64);
        bitset_set_all(&bs);
        EXPECT(bitset_count(&bs) == 64);
        EXPECT(words[0] == (u64)-1); /* all 64 bits set */
        bitset_not(&bs);
        EXPECT(bitset_count(&bs) == 0);
        EXPECT(words[0] == 0);
    }

    /* 65 bits — 2 words, only bit 0 of word 1 used */
    {
        u64 words[2];
        Bitset bs;
        bitset_init(&bs, words, 65);
        bitset_set_all(&bs);
        EXPECT(bitset_count(&bs) == 65);
        EXPECT(words[0] == (u64)-1);
        EXPECT(words[1] == 1); /* only one bit in second word */
        /* Padding bits 1..63 of word 1 must be zero */
        EXPECT((words[1] >> 1) == 0);
    }

    /* Bit exactly at word boundary */
    {
        u64 words[BITSET_WORD_COUNT(128)];
        Bitset bs;
        bitset_init(&bs, words, 128);
        bitset_set(&bs, 63);
        bitset_set(&bs, 64);
        EXPECT(bitset_test(&bs, 63));
        EXPECT(bitset_test(&bs, 64));
        EXPECT((words[0] >> 63) == 1);
        EXPECT((words[1] & 1)   == 1);
        EXPECT(bitset_find_first(&bs) == 63);
        EXPECT(bitset_find_next(&bs, 63) == 64);
        EXPECT(bitset_find_last(&bs)  == 64);
    }
}

/* ── Suppress unused internal helpers ────────────────────────────────────── */

static void bitset_suppress_unused(void)
{
    /* Internal helpers — static inline, exposed but not called directly */
    (void)bitset_word_of;
    (void)bitset_mask_of;
    (void)bitset_clear_padding;

    /* option_usize combinators not tested here (covered by option_test) */
    (void)option_usize_get;
    (void)option_usize_unwrap_or;
    (void)option_usize_expect;
    (void)option_usize_map;
    (void)option_usize_and_then;
    (void)option_usize_or_else;
    (void)option_usize_filter;
    (void)option_usize_combine_with;
    (void)option_usize_replace;
    (void)option_usize_take;
    (void)option_usize_eq;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

static void test_bitset_cbytes_accessor(void)
{
    static u64 w[4];
    Bitset bs;
    bitset_init(&bs, w, 4u * 64u);
    const Bitset* cb = &bs;
    EXPECT(bitset_as_cbytes(cb).len == bitset_as_bytes(&bs).len);
    EXPECT(bitset_as_cbytes(NULL).len == 0u);
}

/* ── VERIFY-020 F1: the capacity-overflow guard ──────────────────────────────
 *
 * BITSET_WORD_COUNT(n) is (n + 63) / 64 computed in usize, which WRAPS above
 * CANON_USIZE_MAX - 63. Measured on the unguarded header: capacity ==
 * USIZE_MAX yields word_count == 0, mem_zero writes zero bytes, and
 * bitset_init SUCCEEDS SILENTLY. Every subsequent `i < bs->capacity` index
 * guard then passes for any i, so bs->words[i / 64] becomes an out-of-bounds
 * write at a caller-controlled offset.
 *
 * The realistic trigger is not an absurd literal. It is an ordinary upstream
 * underflow -- bitset_init(&bs, buf, n - 1) with n == 0 -- which is exactly
 * what this test performs.
 *
 * NOTE ON WHAT THIS TEST CAN AND CANNOT SEE. require_msg is ((void)0) under
 * -DCANON_NO_REQUIRE, the flag the coverage and WP jobs both use, so the
 * guard compiles out there and this test cannot assert on it in that
 * configuration. It asserts only that the CONTRACT-CHECKED build rejects the
 * input. That is the library's model for every other constructor guard, not
 * a weakness peculiar to this one -- but it means the release configuration
 * is still exposed, and that is stated in VERIFY-020 F1 rather than papered
 * over by a green test. */
#ifndef CANON_NO_REQUIRE
static int g_f1_fired = 0;
static void f1_handler(const char* file, int line, const char* func,
                       const char* expr, const char* msg) {
    (void)file; (void)line; (void)func; (void)expr; (void)msg;
    g_f1_fired = 1;
}
#endif

static void test_f1_capacity_overflow_guard(void)
{
#ifdef CANON_NO_REQUIRE
    /* Guard compiles out; nothing to assert. Documented above. */
#else
    u64 words[BITSET_WORD_COUNT(64)];
    Bitset bs;
    usize n = 0;

    contract_set_handler(f1_handler);
    g_f1_fired = 0;
    bitset_init(&bs, words, n - 1u);      /* underflows to USIZE_MAX */
    EXPECT(g_f1_fired == 1);

    /* The bound itself: USIZE_MAX - 63 is the first REJECTED capacity, and
     * the guard must not fire on ordinary sizes. */
    g_f1_fired = 0;
    bitset_init(&bs, words, 64);
    EXPECT(g_f1_fired == 0);

    contract_set_handler(NULL);
#endif
}

/* ── MCDC-012: the uninitialised-Bitset battery ──────────────────────────────
 *
 * Drives the `!bs->words` TRUE leg of every function that guards on it.
 * Measured at CI #1240/#1251/#1252, data/bitset.h sat at 109/126 MC/DC with
 * SEVENTEEN outcomes uncovered; eleven of them were this one leg, once each
 * in clear_all, set_all, not, count, is_empty, is_full, find_first,
 * find_next, find_last, as_bytes and as_cbytes. Every one needs the same
 * input: a Bitset that is non-NULL but whose words pointer is NULL.
 *
 * `Bitset b = {0}` is exactly that state, and it is REACHABLE BY ORDINARY C
 * — a declared-but-never-initialised Bitset. It is not a synthetic poke at
 * private state.
 *
 * THIS BATTERY WAS THE COVERAGE-SIDE EVIDENCE FOR FINDING F2, AND NOW
 * RECORDS ITS FIX.
 * bitset_set / bitset_clear / bitset_toggle / bitset_test used to check only
 * `!bs`, then guard the index with require_msg — which is ((void)0) under
 * -DCANON_NO_REQUIRE, the flag BOTH the coverage job and the WP job use. On
 * this very object those four dereferenced words == NULL. They were
 * therefore DELIBERATELY ABSENT from this function, and THAT ABSENCE WAS THE
 * FINDING. Structurally it was deque's F1, except bitset's asymmetry was
 * WITHIN one family rather than between two.
 *
 * The old comment here said: do not "complete" this battery by adding the
 * four, that would be UB in the configuration being measured. The fix is
 * what makes completing it legal, so the four are now present at the end,
 * and this test is the evidence that the UB is gone rather than the evidence
 * that it exists. bitset_assign joins them: it delegates to set / clear and
 * so was exposed transitively, which the original scoping of F2 missed. */
static void test_uninitialised_bitset(void)
{
    Bitset b = {0};          /* non-NULL, words == NULL, capacity == 0 */

    /* void bulk ops: silent no-ops */
    bitset_clear_all(&b);
    bitset_set_all(&b);
    bitset_not(&b);

    /* queries: documented NULL-ish returns */
    EXPECT(bitset_count(&b)    == 0);
    EXPECT(bitset_is_empty(&b));          /* true, as for NULL */
    EXPECT(!bitset_is_full(&b));          /* false, as for NULL */

    /* searches: all three sentinels */
    EXPECT(bitset_find_first(&b)   == BITSET_NPOS);
    EXPECT(bitset_find_next(&b, 0) == BITSET_NPOS);
    EXPECT(bitset_find_last(&b)    == BITSET_NPOS);

    /* views: empty, not a NULL-deref */
    EXPECT(bitset_as_bytes(&b).len  == 0);
    EXPECT(bitset_as_cbytes(&b).len == 0);

    /* ── The five F2 functions, previously omitted because they were UB here.
     * Each must be a silent no-op / false, exactly like the eleven above. */
    bitset_set(&b, 0);              /* was: OOB write through words[0] */
    bitset_clear(&b, 0);
    bitset_toggle(&b, 0);
    bitset_assign(&b, 0, true);     /* delegates to set / clear */
    EXPECT(bitset_test(&b, 0) == false);
}

/* ── MCDC-012: the two remaining drivable outcomes ───────────────────────────
 *
 * (1) bitset_find_next's `prev >= bs->capacity` TRUE leg. The INNER guard at
 *     `start >= bs->capacity` was already 2/2; this is the first-guard leg,
 *     reached by passing prev == capacity outright.
 *
 * (2) bitset_and's tail loop `for (w = n; w < bs->word_count; w++)`, entered
 *     only when bs has MORE words than other. Nothing in the suite ANDed
 *     unequal capacities, so the loop was never entered — it was also the
 *     file's ONLY uncovered LINE (bitset.h:792, 99.28% of 139).
 *
 * The four outcomes NOT driven here are the `(x < bs->capacity) ? x : NPOS`
 * FALSE legs in find_first / find_next (x2) / find_last. Those fire only if a
 * set bit is found at an index >= capacity, which the PADDING INVARIANT
 * forbids: bits at [capacity, word_count*64) are always zero, so bits_ctz /
 * bits_clz can never report one. They are defensive branches, dead by
 * invariant, and reaching them would require writing bs.words[] directly
 * behind the API to assert behaviour the library does not promise. They take
 * justification rows (MCDC-012 J1-J4), not a test.
 *
 * Note the cross-stream symmetry worth recording in VERIFY-020: the SAME gap
 * appears in the proof stream as the bitset_pad ==> bitset_pad_meaning
 * manual-proof obligation. Coverage cannot reach these branches because the
 * invariant forbids it; WP cannot prove them redundant because pad_meaning is
 * not machine-checked. One fact, two instruments. diag.h line 293 is the
 * existing "invariant-dead" precedent. */
static void test_mcdc_residual_legs(void)
{
    /* (1) prev >= capacity, first guard */
    {
        u64 words[BITSET_WORD_COUNT(64)];
        Bitset bs;
        bitset_init(&bs, words, 64);
        bitset_set(&bs, 0);
        EXPECT(bitset_find_next(&bs, 64)  == BITSET_NPOS);  /* prev == cap  */
        EXPECT(bitset_find_next(&bs, 999) == BITSET_NPOS);  /* prev >  cap  */
    }

    /* (2) bitset_and tail loop: bs wider than other */
    {
        u64 wide_words[BITSET_WORD_COUNT(192)];   /* 3 words */
        u64 narrow_words[BITSET_WORD_COUNT(64)];  /* 1 word  */
        Bitset wide, narrow;
        bitset_init(&wide,   wide_words,   192);
        bitset_init(&narrow, narrow_words, 64);

        bitset_set(&wide, 5);      /* word 0 — survives if also in narrow */
        bitset_set(&wide, 70);     /* word 1 — beyond narrow, must clear  */
        bitset_set(&wide, 150);    /* word 2 — beyond narrow, must clear  */
        bitset_set(&narrow, 5);

        bitset_and(&wide, &narrow);

        EXPECT(bitset_test(&wide, 5));        /* common bit kept          */
        EXPECT(!bitset_test(&wide, 70));      /* tail word zeroed         */
        EXPECT(!bitset_test(&wide, 150));     /* tail word zeroed         */
        EXPECT(bitset_count(&wide) == 1);
    }
}

int main(void)
{
    test_bitset_cbytes_accessor();
    (void)bitset_suppress_unused;

    test_init();
    test_single_bit_ops();
    test_bulk_ops_padding();
    test_bitwise_ops();
    test_queries();
    test_find_sentinel();
    test_find_option();
    test_for_each();
    test_as_bytes();
    test_word_boundary();
    test_uninitialised_bitset();
    test_mcdc_residual_legs();
    test_f1_capacity_overflow_guard();

    if (g_failed == 0) {
        printf("OK  bitset_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  bitset_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void bitset_fuzz_suppress_unused(void)
{
    /* Functions not used in fuzz path */
    (void)bitset_word_of;
    (void)bitset_mask_of;
    (void)bitset_clear_padding;
    (void)bitset_is_full;
    (void)bitset_is_disjoint;
    (void)bitset_find_last;
    (void)bitset_find_last_option;
    (void)bitset_find_next_option;
    (void)bitset_as_bytes;

    /* option_usize combinators not used in fuzz path */
    (void)option_usize_is_none;
    (void)option_usize_get;
    (void)option_usize_unwrap_or;
    (void)option_usize_expect;
    (void)option_usize_map;
    (void)option_usize_and_then;
    (void)option_usize_or_else;
    (void)option_usize_filter;
    (void)option_usize_combine_with;
    (void)option_usize_replace;
    (void)option_usize_take;
    (void)option_usize_eq;
}

/*
 * Fuzz a 64-bit bitset against a u64 reference.
 *
 * Input layout:
 *   [0]    operation count hint (ignored — we iterate through all bytes)
 *   [1..N] operation stream — each byte:
 *            high nibble: operation (0–8)
 *            low  nibble: bit index within [0, 63] via (nibble * 4) % 64
 *                         or value for set_all/clear_all/not
 *
 * Operations (cap = 64, reference = u64 ref):
 *   0 — set(i)           ref |= (1ULL << i)
 *   1 — clear(i)         ref &= ~(1ULL << i)
 *   2 — toggle(i)        ref ^= (1ULL << i)
 *   3 — clear_all        ref = 0
 *   4 — set_all          ref = ~0ULL
 *   5 — not              ref = ~ref
 *   6 — and(mask)        ref &= mask_from(nibble)
 *   7 — or(mask)         ref |= mask_from(nibble)
 *   8 — xor(mask)        ref ^= mask_from(nibble)
 *
 * Invariants checked after every op:
 *   - test(i) == (ref >> i) & 1 for all i in [0, 64)
 *   - count == __builtin_popcountll(ref)
 *   - is_empty == (ref == 0)
 *   - find_first == first set bit in ref (or NPOS)
 *   - find_first_option matches sentinel
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    u64    words[1];
    u64    words2[1];
    Bitset bs, mask_bs;

    (void)bitset_fuzz_suppress_unused;

    if (size < 2) return 0;

    bitset_init(&bs,     words,  64);
    bitset_init(&mask_bs, words2, 64);

    u64 ref = 0;

    #define CHECK_INVARIANTS()                                              \
        do {                                                                \
            /* test(i) agrees with ref for all 64 bits */                   \
            for (usize k__ = 0; k__ < 64; k__++) {                         \
                bool expected__ = ((ref >> k__) & 1u) != 0;                \
                if (bitset_test(&bs, k__) != expected__)                    \
                    __builtin_trap();                                        \
            }                                                               \
            /* count */                                                     \
            if (bitset_count(&bs) != (usize)__builtin_popcountll(ref))     \
                __builtin_trap();                                            \
            /* is_empty */                                                  \
            if (bitset_is_empty(&bs) != (ref == 0))                        \
                __builtin_trap();                                            \
            /* find_first vs ref */                                         \
            {                                                               \
                usize ff__ = bitset_find_first(&bs);                        \
                option_usize ffo__ = bitset_find_first_option(&bs);         \
                if (ref == 0) {                                             \
                    if (ff__ != BITSET_NPOS)           __builtin_trap();    \
                    if (option_usize_is_some(ffo__))   __builtin_trap();    \
                } else {                                                    \
                    usize exp_ff__ = (usize)__builtin_ctzll(ref);           \
                    if (ff__ != exp_ff__)               __builtin_trap();   \
                    if (!option_usize_is_some(ffo__))  __builtin_trap();    \
                    if (option_usize_unwrap(ffo__) != exp_ff__)             \
                                                       __builtin_trap();    \
                }                                                           \
            }                                                               \
        } while (0)

    for (usize i = 1; i < size; i++) {
        u8    byte = data[i];
        u8    op   = (u8)(byte >> 4u) % 9u;
        usize idx  = (usize)((byte & 0x0Fu) * 4u) % 64u; /* 0,4,8,...,60 */
        u64   mb   = (u64)1 << idx; /* single-bit mask for and/or/xor ops */

        switch (op) {
            case 0: bitset_set(&bs, idx);    ref |=  mb; break;
            case 1: bitset_clear(&bs, idx);  ref &= ~mb; break;
            case 2: bitset_toggle(&bs, idx); ref ^=  mb; break;
            case 3: bitset_clear_all(&bs);   ref = 0;    break;
            case 4: bitset_set_all(&bs);     ref = ~(u64)0; break;
            case 5: bitset_not(&bs);         ref = ~ref; break;
            case 6: /* and with a single-bit mask */
                words2[0] = mb;
                mask_bs.capacity   = 64;
                mask_bs.word_count = 1;
                bitset_and(&bs, &mask_bs);
                ref &= mb;
                break;
            case 7: /* or with a single-bit mask */
                words2[0] = mb;
                bitset_or(&bs, &mask_bs);
                ref |= mb;
                break;
            case 8: /* xor with a single-bit mask */
                words2[0] = mb;
                bitset_xor(&bs, &mask_bs);
                ref ^= mb;
                break;
            default: break;
        }

        CHECK_INVARIANTS();
    }

    #undef CHECK_INVARIANTS

    return 0;
}

#endif /* CANON_FUZZING */
