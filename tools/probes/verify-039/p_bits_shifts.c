/*
 * VERIFY-039 G15 probe: four of bits.h's postconditions read as ACSL reads
 * them, as repaired and as they stood before.
 *
 * ACSL gives a C integer its mathematical value and evaluates `<<` without
 * bound: `value << s` is value * 2^s, which can reach 2^64 and beyond, where
 * C's `<<` on a u64 keeps the low 64 bits. Until G15, bits_rotl's and
 * bits_rotr's nonzero_shift postconditions and bits_insert's full_width and
 * normal ones stated their result with such shifts and no cast to u64, and
 * were false for the calls below. Since the repair each shift is cast to
 * u64, as C computes it. This file evaluates each postcondition's right-hand
 * side over 128 bits, which hold every term for the arguments below, in
 * both forms, and compares it with what the function returns:
 *
 *   bits_rotl(0x8000000000000001, 1)  the header's own example, 3
 *   bits_rotr(0x8000000000000001, 1)  the header's own example,
 *                                     0xC000000000000000
 *   bits_insert(0, 1ULL << 63, 1, 64) full_width: 0
 *   bits_insert(0, 1ULL << 62, 2, 63) normal, the field past bit 63: 0
 *
 * and, as controls, the same functions where no term reaches bit 64. Each
 * line prints "holds" or "VIOLATED" for the repaired form, and the form
 * before G15 with its value: VIOLATED for the four, holds for the controls.
 * The exit status is 1 if a repaired form is VIOLATED; the forms before G15
 * are printed, not gated. Frama-C's E-ACSL, which checks a contract at run
 * time with unbounded integers, reported the four postconditions before G15
 * failed and the controls' held (VERIFY-039 G15).
 *
 * From the repository root:
 *     cc -std=c99 -I . tools/probes/verify-039/p_bits_shifts.c \
 *        -o p_bits_shifts && ./p_bits_shifts
 * The three functions have no builtin path, so -DCANON_BITS_FORCE_FALLBACK,
 * the bodies WP verifies, changes nothing here.
 */
#include <stdio.h>
#include "core/primitives/bits.h"

/* A 128-bit unsigned value as two u64 halves, so that the probe needs no
 * compiler extension and runs on ILP32 too. */
typedef struct { u64 hi, lo; } w128;

static w128 w_of(u64 v) { w128 r; r.hi = 0u; r.lo = v; return r; }

static w128 w_shl(w128 a, unsigned s) /* s < 128 */
{
    w128 r;
    if (s == 0u) {
        r = a;
    } else if (s < 64u) {
        r.hi = (a.hi << s) | (a.lo >> (64u - s));
        r.lo = a.lo << s;
    } else {
        r.hi = a.lo << (s - 64u);
        r.lo = 0u;
    }
    return r;
}

static w128 w_or(w128 a, w128 b)  { a.hi |= b.hi; a.lo |= b.lo; return a; }
static w128 w_and(w128 a, w128 b) { a.hi &= b.hi; a.lo &= b.lo; return a; }
static w128 w_xor(w128 a, w128 b) { a.hi ^= b.hi; a.lo ^= b.lo; return a; }

static w128 w_dec(w128 a) /* a > 0 */
{
    if (a.lo == 0u) { a.hi -= 1u; }
    a.lo -= 1u;
    return a;
}

/* (u64)x in ACSL: x modulo 2^64, the low half. */
static w128 w_u64(w128 a) { a.hi = 0u; return a; }

static int violated;

static int equal(u64 result, w128 acsl)
{
    return acsl.hi == 0u && acsl.lo == result;
}

static void check(const char *call, u64 result, w128 now, w128 before)
{
    const int holds = equal(result, now);
    printf("%-34s returns 0x%016llx\n", call, (unsigned long long)result);
    printf("    repaired: %s; before G15: 0x", holds ? "holds" : "VIOLATED");
    if (before.hi != 0u) {
        printf("%llx%016llx", (unsigned long long)before.hi,
               (unsigned long long)before.lo);
    } else {
        printf("%016llx", (unsigned long long)before.lo);
    }
    printf(", %s\n", equal(result, before) ? "holds" : "VIOLATED");
    if (!holds) { violated = 1; }
}

/* bits_rotl, behavior nonzero_shift, s = shift & 63:
 *   repaired:   \result == ((u64)(value << s) | (value >> (u64)(64 - s)))
 *   before G15: \result == ((value << s) | (value >> (64 - s)))         */
static w128 rotl_post(u64 value, u32 shift, int repaired)
{
    const unsigned s = shift & 63u;
    w128 left = w_shl(w_of(value), s);
    if (repaired) { left = w_u64(left); }
    return w_or(left, w_of(value >> (64u - s)));
}

/* bits_rotr, behavior nonzero_shift, s = shift & 63:
 *   repaired:   \result == ((value >> s) | (u64)(value << (u64)(64 - s)))
 *   before G15: \result == ((value >> s) | (value << (64 - s)))         */
static w128 rotr_post(u64 value, u32 shift, int repaired)
{
    const unsigned s = shift & 63u;
    w128 left = w_shl(w_of(value), 64u - s);
    if (repaired) { left = w_u64(left); }
    return w_or(w_of(value >> s), left);
}

/* bits_insert, behavior full_width:
 *   repaired:   \result == (u64)(src << start)
 *   before G15: \result == (src << start)                               */
static w128 insert_full_post(u64 src, u32 start, int repaired)
{
    const w128 r = w_shl(w_of(src), start);
    return repaired ? w_u64(r) : r;
}

/* bits_insert, behavior normal:
 *   \let m = ((u64)1 << count) - 1;
 *   repaired:   \let shifted_mask = (u64)(m << start);
 *               \result == ((dst & (0xFFFFFFFFFFFFFFFFULL ^ shifted_mask)) |
 *                           ((u64)(src << start) & shifted_mask))
 *   before G15: the same with neither shift cast                        */
static w128 insert_normal_post(u64 dst, u64 src, u32 start, u32 count,
                               int repaired)
{
    const w128 m = w_dec(w_shl(w_of(1u), count));
    w128 sm = w_shl(m, start);
    w128 sh = w_shl(w_of(src), start);
    if (repaired) { sm = w_u64(sm); sh = w_u64(sh); }
    return w_or(w_and(w_of(dst), w_xor(w_of(0xFFFFFFFFFFFFFFFFULL), sm)),
                w_and(sh, sm));
}

int main(void)
{
    puts("the four:");
    check("bits_rotl(0x8000000000000001, 1)",
          bits_rotl(0x8000000000000001ULL, 1u),
          rotl_post(0x8000000000000001ULL, 1u, 1),
          rotl_post(0x8000000000000001ULL, 1u, 0));
    check("bits_rotr(0x8000000000000001, 1)",
          bits_rotr(0x8000000000000001ULL, 1u),
          rotr_post(0x8000000000000001ULL, 1u, 1),
          rotr_post(0x8000000000000001ULL, 1u, 0));
    check("bits_insert(0, 1ULL << 63, 1, 64)",
          bits_insert(0u, 0x8000000000000000ULL, 1u, 64u),
          insert_full_post(0x8000000000000000ULL, 1u, 1),
          insert_full_post(0x8000000000000000ULL, 1u, 0));
    check("bits_insert(0, 1ULL << 62, 2, 63)",
          bits_insert(0u, 0x4000000000000000ULL, 2u, 63u),
          insert_normal_post(0u, 0x4000000000000000ULL, 2u, 63u, 1),
          insert_normal_post(0u, 0x4000000000000000ULL, 2u, 63u, 0));

    puts("controls, no term reaching bit 64:");
    check("bits_rotl(3, 1)", bits_rotl(3u, 1u),
          rotl_post(3u, 1u, 1), rotl_post(3u, 1u, 0));
    check("bits_rotr(1, 1)", bits_rotr(1u, 1u),
          rotr_post(1u, 1u, 1), rotr_post(1u, 1u, 0));
    check("bits_insert(0, 5, 0, 64)", bits_insert(0u, 5u, 0u, 64u),
          insert_full_post(5u, 0u, 1), insert_full_post(5u, 0u, 0));
    check("bits_insert(0xF0, 5, 2, 4)", bits_insert(0xF0u, 5u, 2u, 4u),
          insert_normal_post(0xF0u, 5u, 2u, 4u, 1),
          insert_normal_post(0xF0u, 5u, 2u, 4u, 0));
    return violated;
}
