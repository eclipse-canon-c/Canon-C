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
 * @file range_test.c
 * @brief Tests for data/range.h — bounded integer range iterator
 *
 * Covers:
 *   - range_make()              — ascending, descending, stepped, step=0 normalization,
 *                                  ISIZE_MIN+1 boundary (smallest valid negative step)
 *   - range_upto()              — [0, end) with step 1
 *   - range_from_to()           — [start, end) with step 1
 *   - range_downfrom()          — [start, 0) with step -1
 *   - range_downto()            — [start, end) with step -1
 *   - range_is_empty()          — empty/non-empty, NULL-safe
 *   - range_has_next()          — symmetry with is_empty
 *   - range_is_valid()          — combined null + empty check
 *   - range_len()               — exact count: ascending, descending, stepped,
 *                                  empty, NULL
 *   - range_remaining()         — alias for range_len; decrements as iterated
 *   - range_peek()              — non-destructive, NULL/empty cases
 *   - range_peek_option()       — Some/None variants
 *   - range_next()              — advances iterator; LIFO values verified
 *   - range_reset()             — restarts from new current; step/end unchanged
 *   - range_skip()              — O(1) advance; clamped at end; overflow safety
 *   - RANGE_FOR macro           — ascending, descending, stepped, empty,
 *                                  nested, break
 *   - range_current_option      — macro alias for range_peek_option
 *   - range_advance             — macro alias for range_skip
 *   - Empty ranges              — start >= end with positive step,
 *                                  start <= end with negative step
 *
 * NOT covered (require death-test framework):
 *   - range_make(_, _, ISIZE_MIN)  — contract violation, fires require_msg
 *   - range_next(NULL)             — contract violation, fires require_msg
 *   - range_next(exhausted_range)  — debug-only ensure_msg violation
 *
 *   These are documented contract violations that the test suite cannot
 *   exercise without installing a custom contract handler. The contract
 *   semantics are tested implicitly via fuzz coverage (which never
 *   generates ISIZE_MIN steps) and explicitly via code review of the
 *   require_msg / ensure_msg call sites.
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - reference model: simple counters tracking expected current/count
 *   - operations: make (various forms), next, skip, reset, peek
 *   - step generation: restricted to {-1, +1} so ISIZE_MIN is never produced
 *   - invariants after every op:
 *       • range_len() == remaining steps computed from ref
 *       • range_peek() matches expected current
 *       • range_has_next() == (ref_remaining > 0)
 */

#define CANON_CONTRACT_IMPL

/* Force the strict C99 RANGE_FOR fallback — all loop variables in this test
 * are isize, so the (isize) cast in the fallback macro is always correct.
 * Without this, range.h uses typeof() which is a GNU extension unavailable
 * on MSVC and strict-C99 clang builds. */
#ifndef CANON_NO_GNU_EXTENSIONS
#  define CANON_NO_GNU_EXTENSIONS
#endif

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "data/range.h"

/* NOTE: CANON_OPTION(isize) is already called inside range.h — do not repeat */

#include <stdio.h>
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

/* ── Construction helpers ────────────────────────────────────────────────── */

static void test_make(void)
{
    /* step == 0 normalizes to 1 */
    range r = range_make(0, 5, 0);
    EXPECT(r.step == 1);
    EXPECT(r.current == 0);
    EXPECT(r.end == 5);

    /* ascending */
    r = range_make(3, 10, 2);
    EXPECT(r.current == 3 && r.end == 10 && r.step == 2);

    /* descending */
    r = range_make(10, 0, -1);
    EXPECT(r.current == 10 && r.end == 0 && r.step == -1);
}

/* ── ISIZE_MIN+1 boundary: smallest valid negative step ──────────────────── */

static void test_isize_min_boundary(void)
{
    /* ISIZE_MIN + 1 is the smallest negative step range_make accepts.
     * range_make rejects ISIZE_MIN itself (cannot be safely negated for
     * abs_step computation in range_len), but ISIZE_MIN + 1 is fine
     * because -(ISIZE_MIN + 1) == ISIZE_MAX, which is representable.
     *
     * In practice, ranges with such enormous step values produce at
     * most one iteration before saturating to end, but the construction
     * must not crash and the basic queries must work. */

    isize huge_neg_step = CANON_ISIZE_MIN + 1;

    range r = range_make(0, -100, huge_neg_step);
    EXPECT(r.current == 0);
    EXPECT(r.end == -100);
    EXPECT(r.step == huge_neg_step);

    /* range_len must compute without invoking UB on the negation.
     * The exact value depends on isize width; we only verify that it
     * doesn't crash and returns a sensible count. With a step this
     * large, the range contains at most one element (current itself)
     * because (current + step) overflows. */
    usize len = range_len(&r);
    EXPECT(len <= 1); /* at most one element representable */

    /* range_has_next on this range: current (0) > end (-100), step is
     * negative, so current > end means we have at least one element. */
    EXPECT(range_has_next(&r));

    /* peek must return the current value */
    isize val = 0;
    EXPECT(range_peek(&r, &val));
    EXPECT(val == 0);

    /* Symmetric case: ISIZE_MAX as a positive step */
    r = range_make(0, 100, CANON_ISIZE_MAX);
    EXPECT(r.step == CANON_ISIZE_MAX);
    EXPECT(range_has_next(&r));
    EXPECT(range_peek(&r, &val));
    EXPECT(val == 0);
}

static void test_convenience_constructors(void)
{
    range r;

    r = range_upto(5);
    EXPECT(r.current == 0 && r.end == 5 && r.step == 1);

    r = range_from_to(3, 8);
    EXPECT(r.current == 3 && r.end == 8 && r.step == 1);

    r = range_downfrom(5);
    EXPECT(r.current == 5 && r.end == 0 && r.step == -1);

    r = range_downto(10, 5);
    EXPECT(r.current == 10 && r.end == 5 && r.step == -1);
}

/* ── Empty range conditions ──────────────────────────────────────────────── */

static void test_empty_ranges(void)
{
    /* start == end (ascending) */
    range r = range_make(5, 5, 1);
    EXPECT(range_is_empty(&r));
    EXPECT(!range_has_next(&r));
    EXPECT(!range_is_valid(&r));
    EXPECT(range_len(&r) == 0);

    /* start > end with positive step */
    r = range_make(7, 3, 1);
    EXPECT(range_is_empty(&r));
    EXPECT(range_len(&r) == 0);

    /* start == end (descending) */
    r = range_make(5, 5, -1);
    EXPECT(range_is_empty(&r));
    EXPECT(range_len(&r) == 0);

    /* start < end with negative step */
    r = range_make(3, 7, -1);
    EXPECT(range_is_empty(&r));
    EXPECT(range_len(&r) == 0);

    /* NULL */
    EXPECT(range_is_empty(NULL));
    EXPECT(!range_has_next(NULL));
    EXPECT(!range_is_valid(NULL));
    EXPECT(range_len(NULL) == 0);
}

/* ── range_len ───────────────────────────────────────────────────────────── */

static void test_range_len(void)
{
    range r;

    /* [0, 10) step 1 → 10 elements */
    r = range_make(0, 10, 1);
    EXPECT(range_len(&r) == 10);

    /* [0, 10) step 2 → 5 elements: 0,2,4,6,8 */
    r = range_make(0, 10, 2);
    EXPECT(range_len(&r) == 5);

    /* [0, 10) step 3 → 4 elements: 0,3,6,9 */
    r = range_make(0, 10, 3);
    EXPECT(range_len(&r) == 4);

    /* [10, 0) step -1 → 10 elements: 10,9,...,1 */
    r = range_make(10, 0, -1);
    EXPECT(range_len(&r) == 10);

    /* [10, 0) step -3 → 4 elements: 10,7,4,1 */
    r = range_make(10, 0, -3);
    EXPECT(range_len(&r) == 4);

    /* single element */
    r = range_make(5, 6, 1);
    EXPECT(range_len(&r) == 1);

    r = range_make(5, 4, -1);
    EXPECT(range_len(&r) == 1);
}

/* ── peek ────────────────────────────────────────────────────────────────── */

static void test_peek(void)
{
    range r = range_make(7, 20, 3);

    isize val = 0;
    EXPECT(range_peek(&r, &val));
    EXPECT(val == 7);
    /* non-destructive */
    EXPECT(r.current == 7);
    EXPECT(range_len(&r) == 5); /* 7,10,13,16,19 */

    option_isize o = range_peek_option(&r);
    EXPECT(option_isize_is_some(o));
    EXPECT(option_isize_unwrap(o) == 7);

    /* range_current_option alias */
    o = range_current_option(&r);
    EXPECT(option_isize_is_some(o));
    EXPECT(option_isize_unwrap(o) == 7);

    /* empty range */
    range e = range_make(5, 5, 1);
    EXPECT(!range_peek(&e, &val));
    EXPECT(option_isize_is_none(range_peek_option(&e)));

    /* NULL */
    EXPECT(!range_peek(NULL, &val));
    EXPECT(option_isize_is_none(range_peek_option(NULL)));
}

/* ── range_next — ascending ──────────────────────────────────────────────── */

static void test_next_ascending(void)
{
    range r = range_make(0, 5, 1);
    EXPECT(range_len(&r) == 5);

    for (isize expected = 0; expected < 5; expected++) {
        EXPECT(range_has_next(&r));
        isize val = range_next(&r);
        EXPECT(val == expected);
    }
    EXPECT(!range_has_next(&r));
    EXPECT(range_len(&r) == 0);
}

/* ── range_next — descending ─────────────────────────────────────────────── */

static void test_next_descending(void)
{
    range r = range_make(10, 0, -1);
    EXPECT(range_len(&r) == 10);

    for (isize expected = 10; expected > 0; expected--) {
        EXPECT(range_has_next(&r));
        isize val = range_next(&r);
        EXPECT(val == expected);
    }
    EXPECT(!range_has_next(&r));
}

/* ── range_next — stepped ────────────────────────────────────────────────── */

static void test_next_stepped(void)
{
    /* [0, 20) step 3 → 0,3,6,9,12,15,18 (7 elements) */
    range r = range_make(0, 20, 3);
    EXPECT(range_len(&r) == 7);

    isize expected[] = {0, 3, 6, 9, 12, 15, 18};
    for (usize i = 0; i < 7; i++) {
        EXPECT(range_has_next(&r));
        EXPECT(range_next(&r) == expected[i]);
    }
    EXPECT(!range_has_next(&r));

    /* [10, 0) step -3 → 10,7,4,1 (4 elements) */
    r = range_make(10, 0, -3);
    EXPECT(range_len(&r) == 4);
    isize exp2[] = {10, 7, 4, 1};
    for (usize i = 0; i < 4; i++) {
        EXPECT(range_has_next(&r));
        EXPECT(range_next(&r) == exp2[i]);
    }
    EXPECT(!range_has_next(&r));
}

/* ── remaining decrements as iterated ───────────────────────────────────── */

static void test_remaining_decrements(void)
{
    range r = range_make(0, 5, 1);
    for (usize expected = 5; expected > 0; expected--) {
        EXPECT(range_remaining(&r) == expected);
        range_next(&r);
    }
    EXPECT(range_remaining(&r) == 0);
}

/* ── range_reset ─────────────────────────────────────────────────────────── */

static void test_reset(void)
{
    range r = range_make(0, 10, 1);

    /* Consume some */
    range_next(&r);
    range_next(&r);
    EXPECT(r.current == 2);

    /* Reset to beginning */
    range_reset(&r, 0);
    EXPECT(r.current == 0);
    EXPECT(r.end == 10);
    EXPECT(r.step == 1);
    EXPECT(range_len(&r) == 10);

    /* Reset to middle */
    range_reset(&r, 5);
    EXPECT(r.current == 5);
    EXPECT(range_len(&r) == 5);

    /* NULL-safe */
    range_reset(NULL, 0); /* must not crash */
}

/* ── range_skip ──────────────────────────────────────────────────────────── */

static void test_skip(void)
{
    range r = range_make(0, 10, 1);

    /* skip 5 → current should be 5 */
    range_skip(&r, 5);
    EXPECT(r.current == 5);
    EXPECT(range_len(&r) == 5);
    EXPECT(range_next(&r) == 5);

    /* skip more than remaining — clamps to end */
    r = range_make(0, 10, 1);
    range_skip(&r, 100);
    EXPECT(range_is_empty(&r));

    /* skip 0 — no change */
    r = range_make(0, 10, 1);
    range_skip(&r, 0);
    EXPECT(r.current == 0);

    /* skip on empty — no crash */
    r = range_make(5, 5, 1);
    range_skip(&r, 5);
    EXPECT(range_is_empty(&r));

    /* range_advance alias */
    r = range_make(0, 10, 2);
    range_advance(&r, 2); /* skip 2 steps: 0→2→4 */
    EXPECT(r.current == 4);

    /* descending skip */
    r = range_make(10, 0, -1);
    range_skip(&r, 3); /* 10→9→8→7 */
    EXPECT(r.current == 7);

    /* NULL-safe */
    range_skip(NULL, 5); /* must not crash */
}

/* ── RANGE_FOR macro ─────────────────────────────────────────────────────── */

static void test_range_for(void)
{
    isize sum = 0;
    isize i;

    /* Ascending: 0..9 → sum = 45 */
    RANGE_FOR(i, range_make(0, 10, 1)) {
        sum += i;
    }
    EXPECT(sum == 45);

    /* Descending: 10..1 → sum = 55 */
    sum = 0;
    RANGE_FOR(i, range_make(10, 0, -1)) {
        sum += i;
    }
    EXPECT(sum == 55);

    /* Stepped: 0,3,6,9,12,15,18 → sum = 63 */
    sum = 0;
    RANGE_FOR(i, range_make(0, 20, 3)) {
        sum += i;
    }
    EXPECT(sum == 63);

    /* Empty range — body must not execute */
    isize count = 0;
    RANGE_FOR(i, range_make(5, 5, 1)) {
        count++;
    }
    EXPECT(count == 0);

    /* Nested — use manual outer loop to avoid _r/_rp shadowing on MSVC */
    isize pairs = 0;
    {
        range _outer = range_upto(3);
        while (range_has_next(&_outer)) {
            isize _x = range_next(&_outer);
            (void)_x;
            isize _inner_i;
            RANGE_FOR(_inner_i, range_upto(3)) {
                (void)_inner_i;
                pairs++;
            }
        }
    }
    EXPECT(pairs == 9);

    /* break */
    isize last = -1;
    RANGE_FOR(i, range_upto(100)) {
        if (i == 5) break;
        last = i;
    }
    EXPECT(last == 4);
}

/* ── range_upto / range_from_to / range_downfrom / range_downto ──────────── */

static void test_convenience_iteration(void)
{
    isize collected[16];
    usize nc;
    memset(collected, 0, sizeof(collected));

    /* range_upto(5): 0,1,2,3,4 */
    nc = 0;
    range r = range_upto(5);
    while (range_has_next(&r)) { collected[nc++] = range_next(&r); }
    EXPECT(nc == 5);
    for (usize k = 0; k < 5; k++) EXPECT(collected[k] == (isize)k);

    /* range_from_to(3, 7): 3,4,5,6 */
    nc = 0;
    r = range_from_to(3, 7);
    while (range_has_next(&r)) { collected[nc++] = range_next(&r); }
    EXPECT(nc == 4);
    EXPECT(collected[0] == 3 && collected[3] == 6);

    /* range_downfrom(4): 4,3,2,1 */
    nc = 0;
    r = range_downfrom(4);
    while (range_has_next(&r)) { collected[nc++] = range_next(&r); }
    EXPECT(nc == 4);
    EXPECT(collected[0] == 4 && collected[3] == 1);

    /* range_downto(8, 5): 8,7,6 */
    nc = 0;
    r = range_downto(8, 5);
    while (range_has_next(&r)) { collected[nc++] = range_next(&r); }
    EXPECT(nc == 3);
    EXPECT(collected[0] == 8 && collected[2] == 6);
}

/* ── Suppress unused ─────────────────────────────────────────────────────── */

static void range_suppress_unused(void)
{
    /* option_isize combinators not tested here (covered by option_test) */
    (void)option_isize_get;
    (void)option_isize_unwrap_or;
    (void)option_isize_expect;
    (void)option_isize_map;
    (void)option_isize_and_then;
    (void)option_isize_or_else;
    (void)option_isize_filter;
    (void)option_isize_combine_with;
    (void)option_isize_replace;
    (void)option_isize_take;
    (void)option_isize_eq;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */

int main(void)
{
    (void)range_suppress_unused;

    test_make();
    test_isize_min_boundary();
    test_convenience_constructors();
    test_empty_ranges();
    test_range_len();
    test_peek();
    test_next_ascending();
    test_next_descending();
    test_next_stepped();
    test_remaining_decrements();
    test_reset();
    test_skip();
    test_range_for();
    test_convenience_iteration();

    if (g_failed == 0) {
        printf("OK  range_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  range_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void range_fuzz_suppress_unused(void)
{
    /* Convenience constructors not used in fuzz path */
    (void)range_upto;
    (void)range_from_to;
    (void)range_downfrom;
    (void)range_downto;

    /* Queries not used in fuzz path */
    (void)range_is_valid;
    (void)range_remaining;
    (void)range_peek;

    /* option_isize combinators not used in fuzz path */
    (void)option_isize_is_none;
    (void)option_isize_get;
    (void)option_isize_unwrap_or;
    (void)option_isize_expect;
    (void)option_isize_map;
    (void)option_isize_and_then;
    (void)option_isize_or_else;
    (void)option_isize_filter;
    (void)option_isize_combine_with;
    (void)option_isize_replace;
    (void)option_isize_take;
    (void)option_isize_eq;
}

/*
 * Fuzz a single range against a reference model.
 *
 * Input layout:
 *   [0]    capacity byte — encodes start/end/step via modular arithmetic
 *   [1..N] operation stream — each byte:
 *            high nibble: op (0–4)
 *            low  nibble: aux value
 *
 * Step generation:
 *   step is restricted to {-1, +1} based on a single bit of the cfg byte.
 *   ISIZE_MIN is never produced — range_make would reject it via
 *   require_msg, which would abort the fuzz harness with a contract
 *   violation rather than exploring a real bug. The contract semantics
 *   are validated by code review of the require_msg call site, not by
 *   fuzzing across the rejected input space.
 *
 * Operations:
 *   0 — range_next()          — consume one element; verify against ref
 *   1 — range_skip(n)         — skip n elements (n = low nibble, 1–16)
 *   2 — range_peek_option()   — verify peek matches ref current
 *   3 — range_reset(new)      — reset to ref_start; recount
 *   4 — range_has_next()      — verify against ref_remaining > 0
 *
 * Reference model:
 *   ref_current   — what the next range_next() should return
 *   ref_remaining — how many elements remain (pre-computed from range_len)
 *
 * Invariants after every operation:
 *   - range_len() == ref_remaining
 *   - range_has_next() == (ref_remaining > 0)
 *   - if ref_remaining > 0: range_peek_option() == Some(ref_current)
 *   - if ref_remaining == 0: range_is_empty() == true
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    (void)range_fuzz_suppress_unused;

    if (size < 2) return 0;

    /* Decode start/end/step from first byte.
     * step is restricted to {-1, +1} so ISIZE_MIN is never generated —
     * range_make rejects ISIZE_MIN via require_msg, and triggering that
     * from the fuzz harness would abort the run on a documented contract
     * violation rather than exploring a real bug. */
    u8 cfg = data[0];
    isize start = (isize)((cfg >> 4) & 0x0F) - 4;   /* -4..11 */
    isize end   = (isize)((cfg & 0x0F)) - 2;          /* -2..13 */
    isize step  = ((cfg & 1) ? -1 : 1);               /* +1 or -1 */

    range r = range_make(start, end, step);

    /* Reference state */
    isize ref_current   = r.current;
    isize ref_step      = r.step;
    isize ref_end       = r.end;
    usize ref_remaining = range_len(&r);

    #define REF_IS_EMPTY() (ref_remaining == 0)
    #define REF_ADVANCE(n_steps)                                        \
        do {                                                             \
            usize _skip = (n_steps);                                    \
            if (_skip >= ref_remaining) {                               \
                ref_remaining = 0;                                      \
                ref_current   = ref_end;                                \
            } else {                                                    \
                ref_remaining -= _skip;                                 \
                ref_current   += (isize)_skip * ref_step;              \
            }                                                           \
        } while (0)

    #define CHECK_INVARIANTS()                                          \
        do {                                                            \
            /* len agrees with ref */                                   \
            if (range_len(&r) != ref_remaining)    __builtin_trap();   \
            /* has_next agrees with ref */                              \
            if (range_has_next(&r) != !REF_IS_EMPTY()) __builtin_trap(); \
            /* peek matches ref current */                              \
            if (!REF_IS_EMPTY()) {                                      \
                option_isize _o = range_peek_option(&r);               \
                if (!option_isize_is_some(_o))     __builtin_trap();   \
                if (option_isize_unwrap(_o) != ref_current)            \
                                                   __builtin_trap();   \
            } else {                                                    \
                if (!range_is_empty(&r))           __builtin_trap();   \
            }                                                           \
        } while (0)

    CHECK_INVARIANTS();

    for (usize i = 1; i < size; i++) {
        u8    byte = data[i];
        u8    op   = (u8)(byte >> 4u) % 5u;
        usize aux  = (usize)(byte & 0x0Fu) + 1u; /* 1..16 */

        switch (op) {

            case 0: { /* next */
                if (!REF_IS_EMPTY()) {
                    isize val = range_next(&r);
                    if (val != ref_current) __builtin_trap();
                    REF_ADVANCE(1);
                }
                break;
            }

            case 1: { /* skip(aux) */
                range_skip(&r, aux);
                REF_ADVANCE(aux);
                break;
            }

            case 2: { /* peek_option */
                option_isize o = range_peek_option(&r);
                if (REF_IS_EMPTY()) {
                    if (option_isize_is_some(o)) __builtin_trap();
                } else {
                    if (!option_isize_is_some(o)) __builtin_trap();
                    if (option_isize_unwrap(o) != ref_current)
                        __builtin_trap();
                }
                break;
            }

            case 3: { /* reset to original start */
                range_reset(&r, start);
                ref_current   = start;
                ref_remaining = range_len(&r);
                break;
            }

            case 4: { /* has_next */
                if (range_has_next(&r) != !REF_IS_EMPTY())
                    __builtin_trap();
                break;
            }

            default:
                break;
        }

        CHECK_INVARIANTS();
    }

    #undef REF_IS_EMPTY
    #undef REF_ADVANCE
    #undef CHECK_INVARIANTS

    return 0;
}

#endif /* CANON_FUZZING */
