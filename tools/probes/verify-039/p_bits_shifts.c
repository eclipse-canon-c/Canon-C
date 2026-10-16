/*
 * VERIFY-039 G15 probe: four of bits.h's postconditions read as ACSL reads
 * them.
 *
 * ACSL gives a C integer its mathematical value and evaluates `<<` without
 * bound: `value << s` is value * 2^s, which can reach 2^64 and beyond, where
 * C's `<<` on a u64 keeps the low 64 bits. bits_rotl's and bits_rotr's
 * nonzero_shift postconditions and bits_insert's full_width and normal ones
 * state their result with such shifts and no cast to u64. This file
 * evaluates each one's right-hand side over 128 bits, which hold every term
 * for the arguments below, and compares it with what the function returns:
 *
 *   bits_rotl(0x8000000000000001, 1)  the header's own example, 3
 *   bits_rotr(0x8000000000000001, 1)  the header's own example,
 *                                     0xC000000000000000
 *   bits_insert(0, 1ULL << 63, 1, 64) full_width: 0
 *   bits_insert(0, 1ULL << 62, 2, 63) normal, the field past bit 63: 0
 *
 * and, as controls, the same functions where no term reaches bit 64. Each
 * check prints "holds" or "VIOLATED"; the exit status is 1 if any is
 * VIOLATED. Frama-C's E-ACSL, which checks a contract at run time with
 * unbounded integers, reports the same four postconditions failed and the
 * controls' held (VERIFY-039 G15).
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

static int violated;

static void check(const char *call, u64 result, w128 acsl)
{
    const int holds = (acsl.hi == 0u && acsl.lo == result);
    printf("%-34s returns 0x%016llx; ACSL's value 0x", call,
           (unsigned long long)result);
    if (acsl.hi != 0u) {
        printf("%llx%016llx", (unsigned long long)acsl.hi,
               (unsigned long long)acsl.lo);
    } else {
        printf("%016llx", (unsigned long long)acsl.lo);
    }
    printf(": %s\n", holds ? "holds" : "VIOLATED");
    if (!holds) { violated = 1; }
}

/* bits_rotl, behavior nonzero_shift:
 *   \let s = shift & 63; \result == ((value << s) | (value >> (64 - s))) */
static w128 rotl_post(u64 value, u32 shift)
{
    const unsigned s = shift & 63u;
    return w_or(w_shl(w_of(value), s), w_of(value >> (64u - s)));
}

/* bits_rotr, behavior nonzero_shift:
 *   \let s = shift & 63; \result == ((value >> s) | (value << (64 - s))) */
static w128 rotr_post(u64 value, u32 shift)
{
    const unsigned s = shift & 63u;
    return w_or(w_of(value >> s), w_shl(w_of(value), 64u - s));
}

/* bits_insert, behavior full_width: \result == (src << start) */
static w128 insert_full_post(u64 src, u32 start)
{
    return w_shl(w_of(src), start);
}

/* bits_insert, behavior normal:
 *   \let m = ((u64)1 << count) - 1; \let shifted_mask = m << start;
 *   \result == ((dst & (0xFFFFFFFFFFFFFFFFULL ^ shifted_mask)) |
 *               ((src << start) & shifted_mask)) */
static w128 insert_normal_post(u64 dst, u64 src, u32 start, u32 count)
{
    const w128 m  = w_dec(w_shl(w_of(1u), count));
    const w128 sm = w_shl(m, start);
    return w_or(w_and(w_of(dst), w_xor(w_of(0xFFFFFFFFFFFFFFFFULL), sm)),
                w_and(w_shl(w_of(src), start), sm));
}

int main(void)
{
    puts("the four:");
    check("bits_rotl(0x8000000000000001, 1)",
          bits_rotl(0x8000000000000001ULL, 1u),
          rotl_post(0x8000000000000001ULL, 1u));
    check("bits_rotr(0x8000000000000001, 1)",
          bits_rotr(0x8000000000000001ULL, 1u),
          rotr_post(0x8000000000000001ULL, 1u));
    check("bits_insert(0, 1ULL << 63, 1, 64)",
          bits_insert(0u, 0x8000000000000000ULL, 1u, 64u),
          insert_full_post(0x8000000000000000ULL, 1u));
    check("bits_insert(0, 1ULL << 62, 2, 63)",
          bits_insert(0u, 0x4000000000000000ULL, 2u, 63u),
          insert_normal_post(0u, 0x4000000000000000ULL, 2u, 63u));

    puts("controls, no term reaching bit 64:");
    check("bits_rotl(3, 1)", bits_rotl(3u, 1u), rotl_post(3u, 1u));
    check("bits_rotr(1, 1)", bits_rotr(1u, 1u), rotr_post(1u, 1u));
    check("bits_insert(0, 5, 0, 64)", bits_insert(0u, 5u, 0u, 64u),
          insert_full_post(5u, 0u));
    check("bits_insert(0xF0, 5, 2, 4)", bits_insert(0xF0u, 5u, 2u, 4u),
          insert_normal_post(0xF0u, 5u, 2u, 4u));
    return violated;
}
