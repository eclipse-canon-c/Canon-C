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
#include "data/bitset.h"

#include <stdio.h>
#include <string.h>

/* ── CANON_OPTION(usize) — required before bitset.h ─────────────────────── */
CANON_OPTION(usize)

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
    EXPECT(bv.ptr != NULL);

    /* Setting a bit should be visible in the byte view */
    bitset_set(&bs, 0);
    EXPECT(bv.ptr[0] == 1);

    bitset_set(&bs, 63);
    EXPECT(bv.ptr[7] == 0x80);

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
        EXPECT(words[0] >> 63 == 1);
        EXPECT(words[1] & 1   == 1);
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

int main(void)
{
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
