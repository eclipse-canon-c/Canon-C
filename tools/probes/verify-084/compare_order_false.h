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

/* tools/probes/verify-084/compare_order_false.h                   [VERIFY-084]
 * ============================================================================
 * The negative control of compare_order_verify.h: assertions that are
 * false, which WP must not prove with the same options.
 * Written by gen_order.py (tools/probes/verify-084/); edit that, not this.
 *
 *   order_false_T_symmetric  symmetric   cmp(b, a) == cmp(a, b): false
 *                                        whenever a and b compare unequal
 *   order_false_T_self_equal self_equal  for f32 and f64, that C's x == x
 *                                        holds: false for a NaN
 *
 * If WP proved one, the hypotheses would be contradictory, or WP's model
 * would have no unequal values or, for self_equal, no NaN, and the proofs
 * of compare_order_verify.h would say nothing about those cases.
 * ============================================================================ */

#ifndef CANON_VDRIVER_COMPARE_ORDER_FALSE_H
#define CANON_VDRIVER_COMPARE_ORDER_FALSE_H

#include "core/primitives/compare.h"

/* ── u8 ─────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_false_u8_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u8(a, b, ctx);
    int ba = algo_cmp_u8(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u8) - 1));
    assigns  \nothing;
 */
void order_false_u8_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u8_desc(a, b, ctx);
    int ba = algo_cmp_u8_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── u16 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_false_u16_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u16(a, b, ctx);
    int ba = algo_cmp_u16(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u16) - 1));
    assigns  \nothing;
 */
void order_false_u16_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u16_desc(a, b, ctx);
    int ba = algo_cmp_u16_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── u32 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_false_u32_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u32(a, b, ctx);
    int ba = algo_cmp_u32(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u32) - 1));
    assigns  \nothing;
 */
void order_false_u32_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u32_desc(a, b, ctx);
    int ba = algo_cmp_u32_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── u64 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_false_u64_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u64(a, b, ctx);
    int ba = algo_cmp_u64(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(u64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(u64) - 1));
    assigns  \nothing;
 */
void order_false_u64_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_u64_desc(a, b, ctx);
    int ba = algo_cmp_u64_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── i8 ─────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_false_i8_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i8(a, b, ctx);
    int ba = algo_cmp_i8(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i8) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i8) - 1));
    assigns  \nothing;
 */
void order_false_i8_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i8_desc(a, b, ctx);
    int ba = algo_cmp_i8_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── i16 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_false_i16_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i16(a, b, ctx);
    int ba = algo_cmp_i16(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i16) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i16) - 1));
    assigns  \nothing;
 */
void order_false_i16_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i16_desc(a, b, ctx);
    int ba = algo_cmp_i16_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── i32 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_false_i32_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i32(a, b, ctx);
    int ba = algo_cmp_i32(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i32) - 1));
    assigns  \nothing;
 */
void order_false_i32_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i32_desc(a, b, ctx);
    int ba = algo_cmp_i32_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── i64 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_false_i64_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i64(a, b, ctx);
    int ba = algo_cmp_i64(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(i64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(i64) - 1));
    assigns  \nothing;
 */
void order_false_i64_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_i64_desc(a, b, ctx);
    int ba = algo_cmp_i64_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── usize ──────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_false_usize_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_usize(a, b, ctx);
    int ba = algo_cmp_usize(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(usize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(usize) - 1));
    assigns  \nothing;
 */
void order_false_usize_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_usize_desc(a, b, ctx);
    int ba = algo_cmp_usize_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── isize ──────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_false_isize_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_isize(a, b, ctx);
    int ba = algo_cmp_isize(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(isize) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(isize) - 1));
    assigns  \nothing;
 */
void order_false_isize_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_isize_desc(a, b, ctx);
    int ba = algo_cmp_isize_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── f32 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_false_f32_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f32(a, b, ctx);
    int ba = algo_cmp_f32(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_false_f32_self_equal(const void *a)
{
    f32 x = *(const f32 *)a;
    int eq = (x == x);
    /*@ assert self_equal: eq == 1; */
    (void)eq;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f32) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f32) - 1));
    assigns  \nothing;
 */
void order_false_f32_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f32_desc(a, b, ctx);
    int ba = algo_cmp_f32_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/* ── f64 ────────────────────────────────────────────────────────────────── */

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_false_f64_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f64(a, b, ctx);
    int ba = algo_cmp_f64(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_false_f64_self_equal(const void *a)
{
    f64 x = *(const f64 *)a;
    int eq = (x == x);
    /*@ assert self_equal: eq == 1; */
    (void)eq;
}

/*@
    requires \valid_read((char *)a + (0 .. sizeof(f64) - 1));
    requires \valid_read((char *)b + (0 .. sizeof(f64) - 1));
    assigns  \nothing;
 */
void order_false_f64_desc_symmetric(const void *a, const void *b, void *ctx)
{
    int ab = algo_cmp_f64_desc(a, b, ctx);
    int ba = algo_cmp_f64_desc(b, a, ctx);
    /*@ assert symmetric: ba == ab; */
    (void)ab;
    (void)ba;
}

#endif /* CANON_VDRIVER_COMPARE_ORDER_FALSE_H */
