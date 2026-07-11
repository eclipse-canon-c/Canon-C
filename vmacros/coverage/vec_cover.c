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

/* vmacros/coverage/vec_cover.c                                    [MCDC-010]
 * ============================================================================
 * MC/DC cover translation unit for the vec macro module (Shape B, confirmed).
 *
 * WHY THIS FILE EXISTS: same reason as option_cover.c — the condition sites
 * live in IMPL_VEC_* macro BODIES and are stamped to the expansion site.
 * test/data/vec_test.c's instantiation stamps them to a path the coverage
 * job's test-glob filter removes. This TU re-instantiates DEFINE_VEC(int)
 * outside test/ so the identical conditions survive the filter. It is the
 * MC/DC analogue of vmacros/vdrivers/vec_verify.h.
 *
 * NOT A CTEST. Built only under -DENABLE_COVERAGE_TUS=ON, invoked directly
 * by the coverage job, never registered with add_test(), never globbed.
 *
 * COMPILE FLAGS must match the WP run and the coverage build:
 *   -DCANON_NO_REQUIRE -DNDEBUG   --coverage -fcondition-coverage
 *
 * Under that flag pair the measured surface changes vs. a debug build:
 *   - require_msg / ensure_msg conditions VANISH (both macros -> ((void)0)).
 *     get_unchecked / at / push_unchecked / slice_get contribute calls and
 *     lines but ZERO conditions — they are still called below, on inputs
 *     satisfying their (now compiled-out) preconditions, because violating
 *     them under NDEBUG is straight UB.
 *   - swap(NULL, x) is likewise UB now (its require_msg is gone) — the
 *     NULL legs of swap are deliberately NOT exercised here.
 *
 * PREDICTED-UNCOVERABLE CONDITIONS (write-down before the first run):
 *   U1. alloc:       `!checked_mul(...)` true-outcome — INFEASIBLE. The
 *       preceding guard capacity <= CANON_VEC_MAX_CAPACITY/sizeof(type)
 *       bounds capacity*sizeof(type) <= CANON_VEC_MAX_CAPACITY, so the
 *       multiplication can never overflow. Justified-infeasible row.
 *   U2. arena_alloc: same condition, same argument. Justified-infeasible.
 *       (Upstream may instead choose to drop one of the two redundant
 *       guards; until then these are permanent justification rows.)
 *   U3. alloc:       `!buf` true-outcome — heap OOM is not deterministically
 *       forcible from a portable cover TU. Justified-environment row unless
 *       memory.h grows a failure-injection hook. NOTE the arena analogue IS
 *       covered below (exhausted arena makes arena_alloc return NULL
 *       deterministically) — U3 is heap-only.
 *   All other conditions in the measured set are driven to both outcomes.
 *
 * KNOWN EXCLUSION: slice_init on an items==NULL vec with [0,0) is pedantic
 * UB (NULL + 0; finding F2 in vec_verify.h) and is NOT exercised until the
 * upstream guard fix lands. The `!v` and range-check legs of slice_init's
 * condition are covered through other inputs.
 *
 * Facade coverage: this TU also instantiates DEFINE_SLICE(int) +
 * DEFINE_VEC_SLICE(int) so vec.h's three view functions (as_slice /
 * as_slice_full / as_bytes — a 2-condition guard each) are measured here
 * rather than lost to the test filter.
 *
 * Representative instantiation: int — same rationale as the WP driver.
 * ============================================================================ */

/* Handler symbol for the link (standalone binary), same as option_cover.c.
 * No vec path reaches it under CANON_NO_REQUIRE, but contract.h's extern
 * still needs a home. */
#define CANON_CONTRACT_IMPL

#include "semantics/option/option.h"
#include "data/vec/vec.h"      /* facade: pulls vec_defn.h + slice.h + ptr.h */
#include "core/slice.h"
#include "core/arena.h"
#include "core/primitives/limits.h"   /* CANON_VEC_MAX_CAPACITY, CANON_USIZE_MAX */

CANON_OPTION(int)
DEFINE_VEC(static inline, int)
DEFINE_SLICE(int)
DEFINE_VEC_SLICE(int)

/* Sink so the compiler cannot dead-strip under -O / NDEBUG. */
static volatile int    g_sink;
static volatile usize  g_usink;

static void observe(const vec_int* v)
{
    g_usink = vec_int_len(v);
    g_usink = vec_int_capacity(v);
    g_usink = vec_int_remaining(v);
    g_sink  = (int)vec_int_is_empty(v);
    g_sink  = (int)vec_int_is_full(v);
    g_sink  = (vec_int_data(v)  != NULL);
    g_sink  = (vec_int_first(v) != NULL);
    g_sink  = (vec_int_last(v)  != NULL);
}

int main(void)
{
    int buf[8];
    int out = 0;
    result__Bool_Error r;

    /* ── constructors ────────────────────────────────────────────────────
       init: capacity-ok leg and capacity-too-big leg; the NULL-buffer
       require is compiled out, but NULL+cap==0 is a legal, spec'd input. */
    vec_int v      = vec_int_init(buf, 8);
    vec_int vnull0 = vec_int_init(NULL, 0);                       /* legal   */
    vec_int vbig   = vec_int_init(buf,
                        CANON_VEC_MAX_CAPACITY / sizeof(int) + 1);/* too big */
    vec_int e      = vec_int_empty();                             /* items==NULL */
    g_sink = (int)vec_int_is_empty(&vnull0);
    g_sink = (int)vec_int_is_empty(&vbig);

    /* queries: NULL leg, empty leg, populated/full legs */
    observe(NULL);
    observe(&e);
    observe(&v);

    /* ── push family ───────────────────────────────────────────────────── */
    r = vec_int_push(NULL, 1);              (void)r;   /* !v          */
    r = vec_int_push(&e, 1);                (void)r;   /* !v->items   */
    r = vec_int_push(&v, 10);               (void)r;   /* ok          */
    g_sink = (int)vec_int_try_push(NULL, 1);           /* !v          */
    g_sink = (int)vec_int_try_push(&e, 1);             /* !items      */
    g_sink = (int)vec_int_try_push(&v, 20);            /* ok          */
    vec_int_push_unchecked(&v, 30);                    /* raw, precond ok */
    while (!vec_int_is_full(&v)) { (void)vec_int_try_push(&v, 0); }
    r = vec_int_push(&v, 99);               (void)r;   /* full        */
    g_sink = (int)vec_int_try_push(&v, 99);            /* full        */
    observe(&v);                                       /* is_full true leg */

    /* ── element access ────────────────────────────────────────────────── */
    g_sink = (int)vec_int_get(NULL, 0, &out);          /* !v          */
    g_sink = (int)vec_int_get(&v, 0, NULL);            /* !out        */
    g_sink = (int)vec_int_get(&v, 999, &out);          /* i >= len    */
    g_sink = (int)vec_int_get(&v, 1, &out);            /* hit         */
    g_sink = (int)option_int_is_some(vec_int_get_option(NULL, 0));
    g_sink = (int)option_int_is_some(vec_int_get_option(&v, 999));
    g_sink = (int)option_int_is_some(vec_int_get_option(&v, 0));
    g_sink = vec_int_get_unchecked(&v, 0);             /* raw, precond ok */
    g_sink = (vec_int_at(&v, 0) != NULL);              /* raw, precond ok */
    g_sink = (int)vec_int_set(NULL, 0, 7);             /* !v          */
    g_sink = (int)vec_int_set(&v, 999, 7);             /* i >= len    */
    g_sink = (int)vec_int_set(&v, 0, 7);               /* hit         */

    /* first/last on empty (len==0 leg) — NULL leg already via observe(NULL) */
    g_sink = (vec_int_first(&e) != NULL);
    g_sink = (vec_int_last(&e)  != NULL);

    /* ── pop family ────────────────────────────────────────────────────── */
    r = vec_int_pop(NULL, &out);            (void)r;   /* !v          */
    r = vec_int_pop(&v, NULL);              (void)r;   /* !out        */
    r = vec_int_pop(&e, &out);              (void)r;   /* !items      */
    r = vec_int_pop(&v, &out);              (void)r;   /* ok          */
    {
        int b2[2]; vec_int t = vec_int_init(b2, 2);
        r = vec_int_pop(&t, &out);          (void)r;   /* len == 0    */
        g_sink = (int)option_int_is_some(vec_int_pop_option(&t)); /* none */
        (void)vec_int_try_push(&t, 5);
        g_sink = (int)option_int_is_some(vec_int_pop_option(&t)); /* some */
    }

    /* ── clear ─────────────────────────────────────────────────────────── */
    vec_int_clear(NULL);                               /* !v          */
    vec_int_clear(&v);                                 /* live        */

    /* ── insert / remove ───────────────────────────────────────────────── */
    (void)vec_int_try_push(&v, 1);
    (void)vec_int_try_push(&v, 3);
    r = vec_int_insert(NULL, 0, 9);         (void)r;   /* !v          */
    r = vec_int_insert(&e, 0, 9);           (void)r;   /* !items      */
    r = vec_int_insert(&v, 999, 9);         (void)r;   /* i > len     */
    r = vec_int_insert(&v, 1, 2);           (void)r;   /* mid: shift (i<len) */
    r = vec_int_insert(&v, vec_int_len(&v), 4); (void)r; /* end: no shift   */
    while (!vec_int_is_full(&v)) { (void)vec_int_try_push(&v, 0); }
    r = vec_int_insert(&v, 0, 9);           (void)r;   /* full        */

    r = vec_int_remove(NULL, 0, &out);      (void)r;   /* !v          */
    r = vec_int_remove(&v, 0, NULL);        (void)r;   /* !out        */
    r = vec_int_remove(&e, 0, &out);        (void)r;   /* !items      */
    r = vec_int_remove(&v, 999, &out);      (void)r;   /* i >= len    */
    r = vec_int_remove(&v, 0, &out);        (void)r;   /* mid: shift (i<len-1) */
    r = vec_int_remove(&v, vec_int_len(&v) - 1, &out); /* last: no shift */
    (void)r;
    g_sink = (int)option_int_is_some(vec_int_remove_option(&v, 999)); /* none */
    g_sink = (int)option_int_is_some(vec_int_remove_option(&v, 0));   /* some */
    {
        int b1[1]; vec_int t = vec_int_init(b1, 1);
        r = vec_int_remove(&t, 0, &out);    (void)r;   /* len == 0    */
    }

    /* ── bulk ──────────────────────────────────────────────────────────── */
    {
        int b4[4]; vec_int t = vec_int_init(b4, 4);
        int src[3] = {1, 2, 3};
        r = vec_int_append_array(NULL, src, 1); (void)r;  /* !v        */
        r = vec_int_append_array(&t, NULL, 1);  (void)r;  /* !src      */
        r = vec_int_append_array(&e, src, 1);   (void)r;  /* !items    */
        r = vec_int_append_array(&t, src, 2);   (void)r;  /* ok        */
        r = vec_int_append_array(&t, src, CANON_USIZE_MAX);     /* checked_add
                                                             overflow (len=2) */
        (void)r;
        r = vec_int_append_array(&t, src, 3);   (void)r;  /* cap exceeded */
        r = vec_int_extend(&t, src, 1);         (void)r;  /* delegate ok  */
        r = vec_int_extend(&t, src, 3);         (void)r;  /* delegate err */
    }
    {
        int b4[4]; vec_int t = vec_int_init(b4, 4);
        vec_int_fill(NULL, 7, 2);                          /* !v        */
        vec_int_fill(&e, 7, 2);                            /* !items    */
        vec_int_fill(&t, 7, 2);          /* count <  remaining          */
        vec_int_fill(&t, 8, 99);         /* count >  remaining: truncate */
        vec_int_fill(&t, 9, 0);          /* count == 0: loop not entered */
        observe(&t);
    }

    /* ── swap (NULL legs are UB under CANON_NO_REQUIRE — not exercised) ── */
    {
        int ba[2], bb[2];
        vec_int a = vec_int_init(ba, 2), b = vec_int_init(bb, 2);
        (void)vec_int_try_push(&a, 1);
        vec_int_swap(&a, &b);
        observe(&a); observe(&b);
    }

    /* ── iterator ──────────────────────────────────────────────────────── */
    {
        int b3[3]; vec_int t = vec_int_init(b3, 3);
        (void)vec_int_try_push(&t, 1);
        (void)vec_int_try_push(&t, 2);
        vec_int_iter it = vec_int_iter_init(&t);
        g_sink = (int)vec_int_iter_next(NULL, &out);       /* !it       */
        g_sink = (int)vec_int_iter_next(&it, NULL);        /* !out      */
        while (vec_int_iter_next(&it, &out)) { g_sink = out; } /* yield x2,
                                                    then index >= len   */
        vec_int_iter nit = vec_int_iter_init(NULL);        /* vec == NULL */
        g_sink = (int)vec_int_iter_next(&nit, &out);       /* !it->vec  */
    }

    /* ── slice ─────────────────────────────────────────────────────────── */
    {
        int b4[4]; vec_int t = vec_int_init(b4, 4);
        (void)vec_int_try_push(&t, 1);
        (void)vec_int_try_push(&t, 2);
        (void)vec_int_try_push(&t, 3);
        vec_int_slice s;
        s = vec_int_slice_init(NULL, 0, 0);  g_usink = s.len;  /* !v        */
        s = vec_int_slice_init(&t, 2, 1);    g_usink = s.len;  /* start>end */
        s = vec_int_slice_init(&t, 0, 999);  g_usink = s.len;  /* end>len   */
        s = vec_int_slice_init(&t, 1, 3);    g_usink = s.len;  /* ok, sub   */
        g_sink = *vec_int_slice_get(&s, 0);        /* raw, precond ok */
        /* F2 exclusion: no slice_init(&e, 0, 0) until upstream guard fix. */
    }

    /* ── heap / arena constructors ─────────────────────────────────────── */
    {
        vec_int h0 = vec_int_alloc(0);                     /* zero leg  */
        vec_int hb = vec_int_alloc(
            CANON_VEC_MAX_CAPACITY / sizeof(int) + 1);     /* too-big leg */
        vec_int h  = vec_int_alloc(16);                    /* ok leg
                                     (U1 !checked_mul: infeasible;
                                      U3 !buf: not forcible — see header) */
        observe(&h0); observe(&hb); observe(&h);
        (void)vec_int_try_push(&h, 1);
        vec_int_free(NULL);                                /* !v        */
        vec_int_free(&h0);                                 /* items==NULL leg */
        vec_int_free(&h);                                  /* items!=NULL leg */
        observe(&h);                                       /* post-free state */
    }
    {
        /* arena_init(Arena*, void* buffer, usize capacity) — confirmed
         * against core/arena.h. Its require_msg guards are compiled out
         * under CANON_NO_REQUIRE; the args here satisfy them regardless. */
        unsigned char backing[64];
        Arena arena;
        arena_init(&arena, backing, sizeof(backing));
        vec_int a0 = vec_int_arena_alloc(NULL, 4);         /* !arena    */
        vec_int az = vec_int_arena_alloc(&arena, 0);       /* cap==0    */
        vec_int ab = vec_int_arena_alloc(&arena,
            CANON_VEC_MAX_CAPACITY / sizeof(int) + 1);     /* too-big   */
        vec_int a1 = vec_int_arena_alloc(&arena, 4);       /* ok
                                     (U2 !checked_mul: infeasible)      */
        vec_int a2 = vec_int_arena_alloc(&arena, 4096);    /* exhausted:
                                                              !buf TRUE  */
        observe(&a0); observe(&az); observe(&ab);
        observe(&a1); observe(&a2);
    }

    /* ── facade views (vec.h DEFINE_VEC_SLICE) ─────────────────────────── */
    {
        int b2[2]; vec_int t = vec_int_init(b2, 2);
        (void)vec_int_try_push(&t, 1);
        slice_int sv;
        bytes_t   bv;
        sv = vec_int_as_slice(NULL);       g_usink = sv.len;  /* !v      */
        sv = vec_int_as_slice(&e);         g_usink = sv.len;  /* !items  */
        sv = vec_int_as_slice(&t);         g_usink = sv.len;  /* live    */
        sv = vec_int_as_slice_full(NULL);  g_usink = sv.len;
        sv = vec_int_as_slice_full(&e);    g_usink = sv.len;
        sv = vec_int_as_slice_full(&t);    g_usink = sv.len;
        bv = vec_int_as_bytes(NULL);       g_usink = bv.len;
        bv = vec_int_as_bytes(&e);         g_usink = bv.len;
        bv = vec_int_as_bytes(&t);         g_usink = bv.len;
    }

    (void)g_sink; (void)g_usink;
    return 0;
}
