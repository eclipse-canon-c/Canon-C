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

/* tools/probes/verify-084/compare_order_verify.h                  [VERIFY-084]
 * ============================================================================
 * WP driver: each of compare.h's 24 comparators is a total order.
 * Written by gen_order.py (tools/probes/verify-084/); edit that, not this.
 *
 * algo_cmp_fn's documentation (core/primitives/compare.h) asks a comparator
 * to be reflexive, antisymmetric and transitive. The contract of each of
 * compare.h's comparators states that its result is -1, 0 or 1, and nothing
 * that relates two calls, so WP cannot derive any of the three from it.
 *
 * Here each comparator cmp has three functions that call it and assert the
 * property of its results, for any a, b and c that meet its precondition
 * and any ctx:
 *
 *   order_T_reflexive      reflexive      cmp(a, a) == 0
 *   order_T_antisymmetric  antisymmetric  cmp(b, a) == -cmp(a, b)
 *   order_T_transitive     transitive_lt  cmp(a, b) < 0 && cmp(b, c) < 0
 *                                           ==> cmp(a, c) < 0
 *                          transitive_eq  cmp(a, b) == 0 && cmp(b, c) == 0
 *                                           ==> cmp(a, c) == 0
 *
 * With antisymmetry the two transitive ones give the rest of what a sort
 * needs: cmp(a, b) > 0 && cmp(b, c) > 0 ==> cmp(a, c) > 0, and when
 * cmp(a, b) == 0, cmp(a, c) and cmp(b, c) have the same sign.
 *
 * WP proves them with Frama-C's -inline-calls and -remove-inlined, each
 * given the 24 comparators' names: each call is replaced by the
 * comparator's body, and a _desc comparator's call to its ascending one
 * too, so the assertions are proved of the comparators' code. The kernel
 * keeps each inlined body's contract as a statement contract, which WP has
 * not supported since Frama-C 23 (its manual, 1.6 and 2.1); the reads of
 * the inlined bodies are checked by -wp-rte's run-time checks instead. The
 * comparators then leave the analysed unit; frama-c-compare proves them.
 * Frama-C's RPP plug-in proves such properties by self-composition too,
 * but needs Frama-C 33 (docs/for-future.md).
 * ============================================================================ */

#ifndef CANON_VDRIVER_COMPARE_ORDER_VERIFY_H
#define CANON_VDRIVER_COMPARE_ORDER_VERIFY_H

#include "core/primitives/compare.h"

/* ── u8 ─────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_u8_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u8(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_u8_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u8(a, b, ctx);
    int ba = algo_cmp_u8(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_u8_transitive(const void *a, const void *b, const void *c,
                         void *ctx)
{
    int ab = algo_cmp_u8(a, b, ctx);
    int bc = algo_cmp_u8(b, c, ctx);
    int ac = algo_cmp_u8(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_u8_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u8_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_u8_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u8_desc(a, b, ctx);
    int ba = algo_cmp_u8_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_u8_desc_transitive(const void *a, const void *b, const void *c,
                              void *ctx)
{
    int ab = algo_cmp_u8_desc(a, b, ctx);
    int bc = algo_cmp_u8_desc(b, c, ctx);
    int ac = algo_cmp_u8_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── u16 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_u16_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u16(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_u16_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u16(a, b, ctx);
    int ba = algo_cmp_u16(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_u16_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_u16(a, b, ctx);
    int bc = algo_cmp_u16(b, c, ctx);
    int ac = algo_cmp_u16(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_u16_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u16_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_u16_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u16_desc(a, b, ctx);
    int ba = algo_cmp_u16_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_u16_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_u16_desc(a, b, ctx);
    int bc = algo_cmp_u16_desc(b, c, ctx);
    int ac = algo_cmp_u16_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── u32 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_u32_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u32(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_u32_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u32(a, b, ctx);
    int ba = algo_cmp_u32(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_u32_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_u32(a, b, ctx);
    int bc = algo_cmp_u32(b, c, ctx);
    int ac = algo_cmp_u32(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_u32_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u32_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_u32_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u32_desc(a, b, ctx);
    int ba = algo_cmp_u32_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_u32_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_u32_desc(a, b, ctx);
    int bc = algo_cmp_u32_desc(b, c, ctx);
    int ac = algo_cmp_u32_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── u64 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_u64_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u64(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_u64_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u64(a, b, ctx);
    int ba = algo_cmp_u64(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_u64_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_u64(a, b, ctx);
    int bc = algo_cmp_u64(b, c, ctx);
    int ac = algo_cmp_u64(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_u64_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_u64_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_u64_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u64_desc(a, b, ctx);
    int ba = algo_cmp_u64_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_u64_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_u64_desc(a, b, ctx);
    int bc = algo_cmp_u64_desc(b, c, ctx);
    int ac = algo_cmp_u64_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── i8 ─────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_i8_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i8(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_i8_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i8(a, b, ctx);
    int ba = algo_cmp_i8(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_i8_transitive(const void *a, const void *b, const void *c,
                         void *ctx)
{
    int ab = algo_cmp_i8(a, b, ctx);
    int bc = algo_cmp_i8(b, c, ctx);
    int ac = algo_cmp_i8(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_i8_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i8_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_i8_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i8_desc(a, b, ctx);
    int ba = algo_cmp_i8_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_i8_desc_transitive(const void *a, const void *b, const void *c,
                              void *ctx)
{
    int ab = algo_cmp_i8_desc(a, b, ctx);
    int bc = algo_cmp_i8_desc(b, c, ctx);
    int ac = algo_cmp_i8_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── i16 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_i16_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i16(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_i16_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i16(a, b, ctx);
    int ba = algo_cmp_i16(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_i16_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_i16(a, b, ctx);
    int bc = algo_cmp_i16(b, c, ctx);
    int ac = algo_cmp_i16(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_i16_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i16_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_i16_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i16_desc(a, b, ctx);
    int ba = algo_cmp_i16_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_i16_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_i16_desc(a, b, ctx);
    int bc = algo_cmp_i16_desc(b, c, ctx);
    int ac = algo_cmp_i16_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── i32 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_i32_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i32(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_i32_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i32(a, b, ctx);
    int ba = algo_cmp_i32(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_i32_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_i32(a, b, ctx);
    int bc = algo_cmp_i32(b, c, ctx);
    int ac = algo_cmp_i32(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_i32_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i32_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_i32_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i32_desc(a, b, ctx);
    int ba = algo_cmp_i32_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_i32_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_i32_desc(a, b, ctx);
    int bc = algo_cmp_i32_desc(b, c, ctx);
    int ac = algo_cmp_i32_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── i64 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_i64_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i64(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_i64_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i64(a, b, ctx);
    int ba = algo_cmp_i64(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_i64_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_i64(a, b, ctx);
    int bc = algo_cmp_i64(b, c, ctx);
    int ac = algo_cmp_i64(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_i64_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_i64_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_i64_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i64_desc(a, b, ctx);
    int ba = algo_cmp_i64_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_i64_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_i64_desc(a, b, ctx);
    int bc = algo_cmp_i64_desc(b, c, ctx);
    int ac = algo_cmp_i64_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── usize ──────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_usize_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_usize(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_usize_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_usize(a, b, ctx);
    int ba = algo_cmp_usize(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_usize_transitive(const void *a, const void *b, const void *c,
                            void *ctx)
{
    int ab = algo_cmp_usize(a, b, ctx);
    int bc = algo_cmp_usize(b, c, ctx);
    int ac = algo_cmp_usize(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_usize_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_usize_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_usize_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_usize_desc(a, b, ctx);
    int ba = algo_cmp_usize_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_usize_desc_transitive(const void *a, const void *b, const void *c,
                                 void *ctx)
{
    int ab = algo_cmp_usize_desc(a, b, ctx);
    int bc = algo_cmp_usize_desc(b, c, ctx);
    int ac = algo_cmp_usize_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── isize ──────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_isize_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_isize(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_isize_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_isize(a, b, ctx);
    int ba = algo_cmp_isize(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_isize_transitive(const void *a, const void *b, const void *c,
                            void *ctx)
{
    int ab = algo_cmp_isize(a, b, ctx);
    int bc = algo_cmp_isize(b, c, ctx);
    int ac = algo_cmp_isize(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_isize_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_isize_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_isize_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_isize_desc(a, b, ctx);
    int ba = algo_cmp_isize_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_isize_desc_transitive(const void *a, const void *b, const void *c,
                                 void *ctx)
{
    int ab = algo_cmp_isize_desc(a, b, ctx);
    int bc = algo_cmp_isize_desc(b, c, ctx);
    int ac = algo_cmp_isize_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── f32 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_f32_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_f32(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_f32_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f32(a, b, ctx);
    int ba = algo_cmp_f32(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_f32_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_f32(a, b, ctx);
    int bc = algo_cmp_f32(b, c, ctx);
    int ac = algo_cmp_f32(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_f32_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_f32_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_f32_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f32_desc(a, b, ctx);
    int ba = algo_cmp_f32_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_f32_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_f32_desc(a, b, ctx);
    int bc = algo_cmp_f32_desc(b, c, ctx);
    int ac = algo_cmp_f32_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/* ── f64 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_f64_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_f64(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_f64_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f64(a, b, ctx);
    int ba = algo_cmp_f64(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_f64_transitive(const void *a, const void *b, const void *c,
                          void *ctx)
{
    int ab = algo_cmp_f64(a, b, ctx);
    int bc = algo_cmp_f64(b, c, ctx);
    int ac = algo_cmp_f64(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_f64_desc_reflexive(const void *a, void *ctx)
{
    int aa = algo_cmp_f64_desc(a, a, ctx);
    /*@ assert reflexive: aa == 0; */
    (void)aa;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_f64_desc_antisymmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f64_desc(a, b, ctx);
    int ba = algo_cmp_f64_desc(b, a, ctx);
    /*@ assert antisymmetric: ba == -ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)c + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_f64_desc_transitive(const void *a, const void *b, const void *c,
                               void *ctx)
{
    int ab = algo_cmp_f64_desc(a, b, ctx);
    int bc = algo_cmp_f64_desc(b, c, ctx);
    int ac = algo_cmp_f64_desc(a, c, ctx);
    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */
    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */
    (void)ab;
    (void)bc;
    (void)ac;
}

#endif /* CANON_VDRIVER_COMPARE_ORDER_VERIFY_H */
