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

/* test/core/primitives/lifetime_test.c
 * ============================================================================
 * Sequential tests for the lifetime token generator (VERIFY-021).
 *
 * Division of labour with test/concurrency/lifetime_token_test.c: that file
 * tests the COUNTER under concurrency — whether two threads can draw the same
 * value. This file tests the SEQUENTIAL properties, which are the ones the
 * ACSL contract claims, and it is the runtime witness for them. Neither file
 * subsumes the other and neither reimplements the generator: both call the
 * shipped `canon_lifetime_next_id_` directly, per the one-source-of-truth
 * rule in docs/vmacros.md.
 *
 * ── What the contract claims, and what this file checks ───────────────────
 *
 *   ensures \result != REGION_ID_STATIC
 *       Checked here over every owner kind the library can be handed. This
 *       is the property with teeth: handing out 0 would mark the owner as
 *       static-lifetime, so borrows over it would stop expiring and the
 *       instrument would fail OPEN.
 *
 *   assigns counter_
 *       Not directly observable from outside the function — `counter_` has
 *       internal linkage inside the function body. Its EFFECT is observable
 *       and is checked: two calls with the same owner return different ids,
 *       which is only possible if the counter advanced.
 *
 * ── What is deliberately NOT tested, and why ──────────────────────────────
 *
 * The REGION_ID_STATIC guard's TRUE leg is NOT driven, and cannot be driven
 * from a hosted test. It fires exactly when `c_ == (uintptr_t)owner_` — the
 * call counter equal to the owner's address. On this runner that requires the
 * counter to walk to a stack or heap address, order 1e14 calls. On a
 * low-address embedded target it is a few thousand calls and is ordinary,
 * which is why the guard is live code and not dead code.
 *
 * The correct disposition is therefore an MC/DC JUSTIFICATION ROW, not a
 * test — the bitset J1-J4 precedent (MCDC-012), where a branch dead by
 * invariant took a justification rather than a contrived driver. Forcing this
 * one would mean reimplementing the generator with an injectable counter,
 * i.e. verifying a copy, which the project does not do.
 *
 * ── A property the generator does NOT have ────────────────────────────────
 *
 * Ids are not injective. The guard maps the xor-cancelling case onto 1, and 1
 * is also produced directly when `counter ^ address == 1`, so two distinct
 * owners can receive the same id. The rate is negligible and this is not a
 * defect. It is recorded because a reader may otherwise assume uniqueness.
 * test_distinct_live_owners below checks the property that actually holds in
 * practice — distinct simultaneously-live owners get distinct ids — and its
 * comment records that this is an observation, not a guarantee.
 * ============================================================================ */

/* The CI matrix builds this file BOTH ways: the ordinary suite leaves
 * CANON_LIFETIME=off, so the test must switch the machinery on itself; the
 * lifetime-debug job configures with CANON_LIFETIME=debug, which adds
 * CANON_LIFETIME_DEBUG project-wide via add_compile_definitions. An
 * unconditional #define collides with the command-line one under -Werror
 * (GCC -Werror, Clang -Wmacro-redefined, MSVC C4005). */
#ifndef CANON_LIFETIME_DEBUG
#  define CANON_LIFETIME_DEBUG
#endif
#include "core/primitives/lifetime.h"

#include <stdio.h>
#include <stdlib.h>

static int g_failed = 0;

#define EXPECT(cond)                                                \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL %s:%d  %s\n",                     \
                    __FILE__, __LINE__, #cond);                     \
            g_failed++;                                             \
        }                                                           \
    } while (0)

/* ── The contracted postcondition ────────────────────────────────────────── */

/* ensures \result != REGION_ID_STATIC, over every owner kind. */
static void test_never_returns_reserved_id(void)
{
    static int  static_owner;
    int         stack_owner = 0;
    int         stack_array[8] = {0};
    int*        heap_owner = (int*)malloc(sizeof(int));
    unsigned    i;

    EXPECT(heap_owner != NULL);
    if (heap_owner == NULL) { return; }

    EXPECT(canon_lifetime_next_id_(&static_owner) != REGION_ID_STATIC);
    EXPECT(canon_lifetime_next_id_(&stack_owner)  != REGION_ID_STATIC);
    EXPECT(canon_lifetime_next_id_(heap_owner)    != REGION_ID_STATIC);

    for (i = 0; i < 8u; i++) {
        EXPECT(canon_lifetime_next_id_(&stack_array[i]) != REGION_ID_STATIC);
    }

    /* A NULL owner is not a contract violation here: the function reads no
     * memory through the pointer, it only casts it. id = counter ^ 0 =
     * counter, which is non-zero because the counter starts at 1 and the
     * guard catches the wrapped case. Checked rather than assumed, because
     * "null owner" is exactly the input a caller reaches by accident. */
    EXPECT(canon_lifetime_next_id_(NULL) != REGION_ID_STATIC);

    free(heap_owner);
}

/* Sustained: the postcondition is a claim about every call, not the first
 * few. 100k calls is cheap and would catch a counter-derived id landing on
 * zero within the range the test can reach. The counter is a u32: an
 * unsigned int of 16 bits (MSP430) never reaches 100000 (VERIFY-097). */
static void test_never_returns_reserved_id_sustained(void)
{
    int      owner = 0;
    u32      i;
    int      zero_seen = 0;

    for (i = 0; i < 100000u; i++) {
        if (canon_lifetime_next_id_(&owner) == REGION_ID_STATIC) {
            zero_seen = 1;
        }
    }
    EXPECT(zero_seen == 0);
}

/* ── The observable effect of `assigns counter_` ─────────────────────────── */

/* The same owner queried twice must yield different ids. The address term is
 * identical across the two calls, so any difference is the counter's, which
 * makes this the external witness for the assigns clause — and the direct
 * regression test for the historical `assigns \nothing`, which was false
 * precisely because the counter moves. */
static void test_counter_advances(void)
{
    int         owner = 0;
    region_id_t first  = canon_lifetime_next_id_(&owner);
    region_id_t second = canon_lifetime_next_id_(&owner);
    region_id_t third  = canon_lifetime_next_id_(&owner);

    EXPECT(first  != second);
    EXPECT(second != third);
    EXPECT(first  != third);
}

/* ── The property that holds in practice but is not guaranteed ───────────── */

/* Distinct simultaneously-live owners receive distinct ids. This is what the
 * borrow checker actually depends on, and it holds because live objects have
 * distinct addresses. It is an OBSERVATION, not a contract claim: injectivity
 * is false in general (see the file header), and no ACSL clause asserts it.
 * If this test ever fails it is a real regression in the derivation, not a
 * violated guarantee — treat it as a signal to re-read the derivation. */
static void test_distinct_live_owners(void)
{
    enum { N = 64 };
    int         owners[N] = {0};
    region_id_t ids[N];
    unsigned    i, j;

    for (i = 0; i < (unsigned)N; i++) {
        ids[i] = canon_lifetime_next_id_(&owners[i]);
    }
    for (i = 0; i < (unsigned)N; i++) {
        for (j = i + 1u; j < (unsigned)N; j++) {
            EXPECT(ids[i] != ids[j]);
        }
    }
}

/* ── The ladder actually selected what the build asked for ───────────────── */

/* A tripwire, not a property test. CANON_LIFETIME_ATOMIC_LEVEL_ is defined on
 * every path as of CI #1243-#1245 (MISRA 20.9), so an undefined level here
 * means that fix regressed and the #if selection silently reads it as 0. */
static void test_ladder_level_defined(void)
{
#if !defined(CANON_LIFETIME_ATOMIC_LEVEL_)
    EXPECT(0 && "CANON_LIFETIME_ATOMIC_LEVEL_ undefined - MISRA 20.9 fix regressed");
#else
    EXPECT(CANON_LIFETIME_ATOMIC_LEVEL_ >= 1 && CANON_LIFETIME_ATOMIC_LEVEL_ <= 4);
#endif
#if defined(CANON_LIFETIME_NO_ATOMICS)
    /* The verified configuration. If this ever selects anything but level 4,
     * the frama-c-lifetime job is proving a body other than the one its
     * contract is gated on. */
    EXPECT(CANON_LIFETIME_ATOMIC_LEVEL_ == 4);
    EXPECT(CANON_LIFETIME_ATOMIC_IDS == 0);
#endif
}

int main(void)
{
    test_never_returns_reserved_id();
    test_never_returns_reserved_id_sustained();
    test_counter_advances();
    test_distinct_live_owners();
    test_ladder_level_defined();

    if (g_failed == 0) {
        printf("OK  lifetime_test  (all assertions passed, level %d)\n",
               CANON_LIFETIME_ATOMIC_LEVEL_);
        return 0;
    }

    fprintf(stderr, "FAILED  lifetime_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}
