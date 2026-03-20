/**
 * @file bits_test.c
 * @brief Unit tests (and optional fuzz harness) for bits.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput exercises the same logic with
 *              arbitrary byte input, letting libFuzzer explore edge cases.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   bits_test        / bits_clear   / bits_toggle / bits_set
 *   bits_extract     / bits_insert
 *   bits_popcount    / bits_clz     / bits_ctz
 *   bits_ffs         / bits_fls
 *   bits_rotl        / bits_rotr
 *   bits_is_power_of_two / bits_next_power_of_two
 *   bits_bswap16     / bits_bswap32 / bits_bswap64
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   No 0b binary literals — they are a C23/GNU extension and are rejected
 *   by strict C99 compilers (-Wpedantic -Werror). All constants use hex or
 *   decimal notation with comments showing the binary pattern for clarity.
 *   Loop variables are declared before the loop body (C99 requirement for
 *   some compilers). Array initialisers are used in place of C99 compound
 *   literals where mixed int/u32 comparisons would otherwise arise.
 */

#include "bits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name)  do { test_##name(); } while (0)

#define EXPECT(expr)                                                \
    do {                                                            \
        if (expr) {                                                 \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr, "FAIL  %s:%d  %s\n",                   \
                    __FILE__, __LINE__, #expr);                     \
        }                                                           \
    } while (0)

/* =========================================================================
 * Basic single-bit operations
 * ====================================================================== */

TEST(bits_test_op) {
    /* 0xA == 0b1010 */
    EXPECT(bits_test(0xAu, 1) == true);   /* bit 1 set   */
    EXPECT(bits_test(0xAu, 0) == false);  /* bit 0 clear */
    EXPECT(bits_test(0xAu, 2) == false);  /* bit 2 clear */
    EXPECT(bits_test(0xAu, 3) == true);   /* bit 3 set   */

    /* MSB of u64 */
    EXPECT(bits_test(0x8000000000000000ULL, 63) == true);
    EXPECT(bits_test(0x8000000000000000ULL, 62) == false);

    /* All zeros / all ones */
    EXPECT(bits_test(0ULL,  0)  == false);
    EXPECT(bits_test(~0ULL, 0)  == true);
    EXPECT(bits_test(~0ULL, 63) == true);
}

TEST(bits_set_op) {
    /* 0x0 | (1<<3) == 0x8 */
    EXPECT(bits_set(0x0u, 3) == 0x8u);
    /* 0xA | (1<<0) == 0xB  (0b1010 -> 0b1011) */
    EXPECT(bits_set(0xAu, 0) == 0xBu);
    /* Setting an already-set bit is idempotent: 0xF | (1<<2) == 0xF */
    EXPECT(bits_set(0xFu, 2) == 0xFu);
    /* Set MSB */
    EXPECT(bits_set(0ULL, 63) == 0x8000000000000000ULL);
}

TEST(bits_clear_op) {
    /* 0xF & ~(1<<2) == 0xB  (0b1111 -> 0b1011) */
    EXPECT(bits_clear(0xFu, 2) == 0xBu);
    /* 0xA & ~(1<<1) == 0x8  (0b1010 -> 0b1000) */
    EXPECT(bits_clear(0xAu, 1) == 0x8u);
    /* Clearing an already-clear bit is idempotent */
    EXPECT(bits_clear(0xAu, 0) == 0xAu);
    /* Clear MSB */
    EXPECT(bits_clear(0x8000000000000000ULL, 63) == 0ULL);
}

TEST(bits_toggle_op) {
    /* 0xA == 0b1010 */
    u64 v = 0xAu;
    v = bits_toggle(v, 0);  EXPECT(v == 0xBu);  /* 0b1010 -> 0b1011 */
    v = bits_toggle(v, 0);  EXPECT(v == 0xAu);  /* 0b1011 -> 0b1010 */
    /* Toggle MSB */
    EXPECT(bits_toggle(0ULL,                  63) == 0x8000000000000000ULL);
    EXPECT(bits_toggle(0x8000000000000000ULL, 63) == 0ULL);
}

/* =========================================================================
 * Multi-bit operations
 * ====================================================================== */

TEST(bits_extract_op) {
    /* 0xD6 == 0b11010110; bits[2..5] == 0b0101 == 0x5 */
    EXPECT(bits_extract(0xD6u, 2, 4) == 0x5u);

    /* Zero count -> 0 */
    EXPECT(bits_extract(0xFFFFFFFFFFFFFFFFULL, 0, 0) == 0ULL);

    /* Full width (count >= 64) */
    EXPECT(bits_extract(0xDEADBEEFCAFEBABEULL, 0, 64) ==
           0xDEADBEEFCAFEBABEULL);

    /* Single bit: 0xAA == 0b10101010 */
    EXPECT(bits_extract(0xAAu, 1, 1) == 1u);  /* bit 1 set   */
    EXPECT(bits_extract(0xAAu, 0, 1) == 0u);  /* bit 0 clear */

    /* High bits */
    EXPECT(bits_extract(0xF000000000000000ULL, 60, 4) == 0xFu);

    /* Network header example from header comment:
     * 0xB5330F0F == 0b10110101_00110011_00001111_00001111
     * bits[28..31] == 0b1011 == 0xB
     * bits[24..27] == 0b0101 == 0x5                         */
    {
        u32 hdr = 0xB5330F0Fu;
        EXPECT(bits_extract(hdr, 28, 4) == 0xBu);
        EXPECT(bits_extract(hdr, 24, 4) == 0x5u);
    }
}

TEST(bits_insert_op) {
    /* dst=0xF0 (0b11110000), src=0x5 (0b0101), start=2, count=4
     * -> 0b11010100 == 0xD4                                  */
    {
        u64 result = bits_insert(0xF0u, 0x5u, 2, 4);
        EXPECT(result == 0xD4u);
    }

    /* Zero count -> dst unchanged */
    EXPECT(bits_insert(0xDEADBEEFu, 0xFFu, 0, 0) == 0xDEADBEEFu);

    /* Insert single bit */
    EXPECT(bits_insert(0x0u, 1u, 2, 1) == 0x4u);  /* 0b0000 -> 0b0100 */
    EXPECT(bits_insert(0xFu, 0u, 2, 1) == 0xBu);  /* 0b1111 -> 0b1011 */

    /* Round-trip with extract */
    {
        u64 original = 0xCAFEBABEDEADBEEFULL;
        u64 field    = bits_extract(original, 16, 8);
        u64 restored = bits_insert(original, field, 16, 8);
        EXPECT(restored == original);
    }

    /* Insert into high bits */
    EXPECT(bits_insert(0ULL, 0xFu, 60, 4) == 0xF000000000000000ULL);
}

/* =========================================================================
 * Bit counting
 * ====================================================================== */

TEST(bits_popcount_op) {
    /* 0xB5 == 0b10110101 -> 5 set bits */
    EXPECT(bits_popcount(0xB5u) == 5u);
    EXPECT(bits_popcount(0ULL)  == 0u);
    EXPECT(bits_popcount(~0ULL) == 64u);
    EXPECT(bits_popcount(1ULL)  == 1u);
    EXPECT(bits_popcount(0x5555555555555555ULL) == 32u);
    EXPECT(bits_popcount(0xAAAAAAAAAAAAAAAAULL) == 32u);
    /* Upper 32 bits set (example from header) */
    EXPECT(bits_popcount(0xFFFFFFFF00000000ULL) == 32u);
}

TEST(bits_clz_op) {
    EXPECT(bits_clz(0ULL) == 64u);
    EXPECT(bits_clz(1ULL) == 63u);
    EXPECT(bits_clz(2ULL) == 62u);
    EXPECT(bits_clz(0x8000000000000000ULL) == 0u);
    /* 0xA == 0b00001010 in 64-bit context -> 60 leading zeros */
    EXPECT(bits_clz(0xAu) == 60u);
    EXPECT(bits_clz(~0ULL) == 0u);
}

TEST(bits_ctz_op) {
    u32 i;
    EXPECT(bits_ctz(0ULL) == 64u);
    EXPECT(bits_ctz(1ULL) == 0u);
    EXPECT(bits_ctz(2ULL) == 1u);
    /* 0xA0 == 0b10100000 -> 5 trailing zeros */
    EXPECT(bits_ctz(0xA0u) == 5u);
    EXPECT(bits_ctz(0x8000000000000000ULL) == 63u);
    EXPECT(bits_ctz(~0ULL) == 0u);
    /* Powers of two */
    for (i = 0; i < 64u; i++) {
        EXPECT(bits_ctz(1ULL << i) == i);
    }
}

TEST(bits_ffs_op) {
    u64 vals[5];
    u32 i;

    EXPECT(bits_ffs(0ULL) == 0u);          /* No bits set        */
    EXPECT(bits_ffs(1ULL) == 1u);          /* Bit 0 -> position 1 */
    /* 0xA0 == 0b10100000 -> lowest set bit is bit 5 -> ffs returns 6 */
    EXPECT(bits_ffs(0xA0u) == 6u);
    EXPECT(bits_ffs(0x8000000000000000ULL) == 64u);

    /* Matches ctz+1 for any non-zero value */
    vals[0] = 1ULL;
    vals[1] = 2ULL;
    vals[2] = 6ULL;
    vals[3] = 0xDEADBEEFu;
    vals[4] = 0x8000000000000000ULL;
    for (i = 0; i < 5u; i++) {
        EXPECT(bits_ffs(vals[i]) == bits_ctz(vals[i]) + 1u);
    }
}

TEST(bits_fls_op) {
    u64 vals[5];
    u32 i;

    EXPECT(bits_fls(0ULL) == 0u);          /* No bits set        */
    EXPECT(bits_fls(1ULL) == 1u);
    /* 0xA0 == 0b10100000 -> highest set bit is bit 7 -> fls returns 8 */
    EXPECT(bits_fls(0xA0u) == 8u);
    EXPECT(bits_fls(0x8000000000000000ULL) == 64u);

    /* Matches 64 - clz for any non-zero value */
    vals[0] = 1ULL;
    vals[1] = 2ULL;
    vals[2] = 0xFFu;
    vals[3] = 0xDEADBEEFu;
    vals[4] = 0x8000000000000000ULL;
    for (i = 0; i < 5u; i++) {
        EXPECT(bits_fls(vals[i]) == 64u - bits_clz(vals[i]));
    }
}

/* =========================================================================
 * Rotation
 * ====================================================================== */

TEST(bits_rotl_op) {
    u64 val = 0x8000000000000001ULL;
    /* MSB and LSB set, rotate left 1:
     * 0x8000000000000001 -> 0x0000000000000003 */
    EXPECT(bits_rotl(val, 1) == 0x0000000000000003ULL);

    /* Rotate by 0 -> identity */
    EXPECT(bits_rotl(0xDEADBEEFCAFEBABEULL, 0) ==
           0xDEADBEEFCAFEBABEULL);

    /* Rotate by 64 -> identity (shift & 63 == 0) */
    EXPECT(bits_rotl(0xDEADBEEFCAFEBABEULL, 64) ==
           0xDEADBEEFCAFEBABEULL);

    /* rotl then rotr should recover original */
    {
        u64 orig = 0x123456789ABCDEF0ULL;
        EXPECT(bits_rotr(bits_rotl(orig, 13), 13) == orig);
    }

    /* All-ones and all-zeros are invariant under rotation */
    EXPECT(bits_rotl(~0ULL, 17) == ~0ULL);
    EXPECT(bits_rotl(0ULL,  17) == 0ULL);
}

TEST(bits_rotr_op) {
    u64 val = 0x8000000000000001ULL;
    /* MSB and LSB set, rotate right 1:
     * 0x8000000000000001 -> 0xC000000000000000 */
    EXPECT(bits_rotr(val, 1) == 0xC000000000000000ULL);

    /* Rotate by 0 -> identity */
    EXPECT(bits_rotr(0xDEADBEEFCAFEBABEULL, 0) ==
           0xDEADBEEFCAFEBABEULL);

    /* Rotate by 64 -> identity */
    EXPECT(bits_rotr(0xDEADBEEFCAFEBABEULL, 64) ==
           0xDEADBEEFCAFEBABEULL);

    /* rotr then rotl should recover original */
    {
        u64 orig = 0xFEDCBA9876543210ULL;
        EXPECT(bits_rotl(bits_rotr(orig, 27), 27) == orig);
    }

    EXPECT(bits_rotr(~0ULL, 23) == ~0ULL);
    EXPECT(bits_rotr(0ULL,  23) == 0ULL);
}

/* =========================================================================
 * Power-of-two
 * ====================================================================== */

TEST(bits_is_power_of_two_op) {
    u32 i;

    EXPECT(bits_is_power_of_two(1ULL)       == true);
    EXPECT(bits_is_power_of_two(2ULL)       == true);
    EXPECT(bits_is_power_of_two(4ULL)       == true);
    EXPECT(bits_is_power_of_two(16ULL)      == true);
    EXPECT(bits_is_power_of_two(1ULL << 63) == true);

    EXPECT(bits_is_power_of_two(0ULL)  == false);
    EXPECT(bits_is_power_of_two(3ULL)  == false);
    EXPECT(bits_is_power_of_two(15ULL) == false);
    EXPECT(bits_is_power_of_two(~0ULL) == false);
    EXPECT(bits_is_power_of_two(6ULL)  == false);

    /* All true powers 2^0 ... 2^63 */
    for (i = 0; i < 64u; i++) {
        EXPECT(bits_is_power_of_two(1ULL << i) == true);
    }
}

TEST(bits_next_power_of_two_op) {
    u64 inputs[8];
    int i;

    EXPECT(bits_next_power_of_two(0ULL)    == 0ULL);
    EXPECT(bits_next_power_of_two(1ULL)    == 1ULL);
    EXPECT(bits_next_power_of_two(2ULL)    == 2ULL);
    EXPECT(bits_next_power_of_two(3ULL)    == 4ULL);
    EXPECT(bits_next_power_of_two(17ULL)   == 32ULL);
    EXPECT(bits_next_power_of_two(32ULL)   == 32ULL);
    EXPECT(bits_next_power_of_two(1000ULL) == 1024ULL);

    /* Overflow: values > 2^63 -> 0 */
    EXPECT(bits_next_power_of_two((1ULL << 63) + 1ULL) == 0ULL);
    EXPECT(bits_next_power_of_two(~0ULL) == 0ULL);

    /* Result is always a power of two (for non-overflow, non-zero inputs) */
    inputs[0] = 1ULL;
    inputs[1] = 5ULL;
    inputs[2] = 7ULL;
    inputs[3] = 100ULL;
    inputs[4] = 255ULL;
    inputs[5] = 256ULL;
    inputs[6] = 1023ULL;
    inputs[7] = 1ULL << 62;
    for (i = 0; i < 8; i++) {
        u64 r = bits_next_power_of_two(inputs[i]);
        EXPECT(r >= inputs[i]);
        EXPECT(bits_is_power_of_two(r));
    }
}

/* =========================================================================
 * Byte reversal
 * ====================================================================== */

TEST(bits_bswap16_op) {
    EXPECT(bits_bswap16(0x1234u) == 0x3412u);
    EXPECT(bits_bswap16(0x0001u) == 0x0100u);
    EXPECT(bits_bswap16(0xFF00u) == 0x00FFu);
    EXPECT(bits_bswap16(0x0000u) == 0x0000u);
    EXPECT(bits_bswap16(0xFFFFu) == 0xFFFFu);
    /* Double-swap is identity */
    EXPECT(bits_bswap16(bits_bswap16(0xABCDu)) == 0xABCDu);
}

TEST(bits_bswap32_op) {
    EXPECT(bits_bswap32(0x12345678u) == 0x78563412u);
    EXPECT(bits_bswap32(0x00000001u) == 0x01000000u);
    EXPECT(bits_bswap32(0xFF000000u) == 0x000000FFu);
    EXPECT(bits_bswap32(0x00000000u) == 0x00000000u);
    EXPECT(bits_bswap32(0xFFFFFFFFu) == 0xFFFFFFFFu);
    /* Double-swap is identity */
    EXPECT(bits_bswap32(bits_bswap32(0xDEADBEEFu)) == 0xDEADBEEFu);
}

TEST(bits_bswap64_op) {
    EXPECT(bits_bswap64(0x0102030405060708ULL) == 0x0807060504030201ULL);
    EXPECT(bits_bswap64(0xFF00000000000000ULL) == 0x00000000000000FFULL);
    EXPECT(bits_bswap64(0x0000000000000001ULL) == 0x0100000000000000ULL);
    EXPECT(bits_bswap64(0ULL)  == 0ULL);
    EXPECT(bits_bswap64(~0ULL) == ~0ULL);
    /* Double-swap is identity */
    EXPECT(bits_bswap64(bits_bswap64(0xDEADBEEFCAFEBABEULL)) ==
           0xDEADBEEFCAFEBABEULL);
}

/* =========================================================================
 * Cross-function consistency checks
 * ====================================================================== */

TEST(clz_ctz_popcount_consistency) {
    /* For any power of two 2^k:
     *   popcount == 1
     *   ctz      == k
     *   clz      == 63 - k
     */
    u32 k;
    for (k = 0; k < 64u; k++) {
        u64 v = 1ULL << k;
        EXPECT(bits_popcount(v) == 1u);
        EXPECT(bits_ctz(v)      == k);
        EXPECT(bits_clz(v)      == 63u - k);
    }
}

TEST(set_clear_toggle_consistency) {
    u64 v = 0ULL;
    u32 i;

    /* Set all bits one at a time and verify popcount grows */
    for (i = 0; i < 64u; i++) {
        v = bits_set(v, i);
        EXPECT(bits_test(v, i));
        EXPECT(bits_popcount(v) == i + 1u);
    }

    /* Clear all bits one at a time */
    for (i = 0; i < 64u; i++) {
        v = bits_clear(v, i);
        EXPECT(!bits_test(v, i));
    }
    EXPECT(v == 0ULL);

    /* Toggle each bit twice -> back to original */
    {
        u64 original = 0xDEADBEEFCAFEBABEULL;
        u64 toggled  = original;
        for (i = 0; i < 64u; i++) toggled = bits_toggle(toggled, i);
        for (i = 0; i < 64u; i++) toggled = bits_toggle(toggled, i);
        EXPECT(toggled == original);
    }
}

TEST(extract_insert_roundtrip) {
    /* Exhaustive round-trip: extract field, re-insert it -> same value */
    u64 base = 0xFEDCBA9876543210ULL;
    u32 start;
    for (start = 0; start < 64u; start++) {
        u32 count;
        for (count = 1u; count <= 64u - start; count++) {
            u64 field   = bits_extract(base, start, count);
            u64 rebuilt = bits_insert(base, field, start, count);
            EXPECT(rebuilt == base);
        }
    }
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING
/* ---------------------------------------------------------------------- */
/* Fuzz harness - libFuzzer entry point                                    */
/*                                                                          */
/* Interprets the raw byte buffer as:                                       */
/*   bytes [0..7]  -> u64 value  (little-endian memcpy)                   */
/*   bytes [8]     -> bit index  (0-63, masked)                            */
/*   bytes [9]     -> shift      (0-63, masked)                            */
/*   bytes [10]    -> field start (0-63, masked)                           */
/*   bytes [11]    -> field count (1-64, clamped)                          */
/* ---------------------------------------------------------------------- */
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    u64 value;
    u32 bit, shift, start, count;

    if (size < 12) return 0;

    value = 0;
    memcpy(&value, data, 8);

    bit   = (u32)data[8]  & 63u;
    shift = (u32)data[9]  & 63u;
    start = (u32)data[10] & 63u;
    count = ((u32)data[11] & 63u) + 1u;  /* 1..64 */
    if (start + count > 64u) count = 64u - start;
    if (count == 0u) count = 1u;

    /* Single-bit ops */
    (void)bits_test(value, bit);
    (void)bits_set(value, bit);
    (void)bits_clear(value, bit);
    (void)bits_toggle(value, bit);

    /* Multi-bit ops: extract->insert round-trip must be identity */
    {
        u64 field   = bits_extract(value, start, count);
        u64 rebuilt = bits_insert(value, field, start, count);
        if (rebuilt != value) __builtin_trap();
    }

    /* Counting */
    (void)bits_popcount(value);
    (void)bits_clz(value);
    (void)bits_ctz(value);
    (void)bits_ffs(value);
    (void)bits_fls(value);

    /* Rotation: rotl->rotr must recover original */
    {
        u64 rotated = bits_rotl(value, shift);
        if (bits_rotr(rotated, shift) != value) __builtin_trap();
    }

    /* Power-of-two */
    {
        u64 next = bits_next_power_of_two(value);
        if (next != 0u && !bits_is_power_of_two(next)) __builtin_trap();
        if (next != 0u && next < value)                 __builtin_trap();
    }

    /* Byte swap: double-swap must be identity */
    {
        u16 v16 = (u16)value;
        u32 v32 = (u32)value;
        if (bits_bswap16(bits_bswap16(v16)) != v16)     __builtin_trap();
        if (bits_bswap32(bits_bswap32(v32)) != v32)     __builtin_trap();
        if (bits_bswap64(bits_bswap64(value)) != value)  __builtin_trap();
    }

    return 0;
}

#else /* CANON_FUZZING not defined -> unit test binary */

int main(void) {
    /* Basic single-bit */
    RUN(bits_test_op);
    RUN(bits_set_op);
    RUN(bits_clear_op);
    RUN(bits_toggle_op);

    /* Multi-bit */
    RUN(bits_extract_op);
    RUN(bits_insert_op);

    /* Counting */
    RUN(bits_popcount_op);
    RUN(bits_clz_op);
    RUN(bits_ctz_op);
    RUN(bits_ffs_op);
    RUN(bits_fls_op);

    /* Rotation */
    RUN(bits_rotl_op);
    RUN(bits_rotr_op);

    /* Power-of-two */
    RUN(bits_is_power_of_two_op);
    RUN(bits_next_power_of_two_op);

    /* Byte reversal */
    RUN(bits_bswap16_op);
    RUN(bits_bswap32_op);
    RUN(bits_bswap64_op);

    /* Cross-function consistency */
    RUN(clz_ctz_popcount_consistency);
    RUN(set_clear_toggle_consistency);
    RUN(extract_insert_roundtrip);

    printf("\nbits_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
