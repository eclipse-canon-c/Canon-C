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
    /* LSB */
    EXPECT(bits_test(0b1010u, 1) == true);
    EXPECT(bits_test(0b1010u, 0) == false);
    EXPECT(bits_test(0b1010u, 2) == false);
    EXPECT(bits_test(0b1010u, 3) == true);

    /* MSB of u64 */
    EXPECT(bits_test(0x8000000000000000ULL, 63) == true);
    EXPECT(bits_test(0x8000000000000000ULL, 62) == false);

    /* All zeros / all ones */
    EXPECT(bits_test(0ULL, 0)  == false);
    EXPECT(bits_test(~0ULL, 0) == true);
    EXPECT(bits_test(~0ULL, 63) == true);
}

TEST(bits_set_op) {
    EXPECT(bits_set(0b0000u, 3) == 0b1000u);
    EXPECT(bits_set(0b1010u, 0) == 0b1011u);
    /* Setting an already-set bit is idempotent */
    EXPECT(bits_set(0b1111u, 2) == 0b1111u);
    /* Set MSB */
    EXPECT(bits_set(0ULL, 63) == 0x8000000000000000ULL);
}

TEST(bits_clear_op) {
    EXPECT(bits_clear(0b1111u, 2) == 0b1011u);
    EXPECT(bits_clear(0b1010u, 1) == 0b1000u);
    /* Clearing an already-clear bit is idempotent */
    EXPECT(bits_clear(0b1010u, 0) == 0b1010u);
    /* Clear MSB */
    EXPECT(bits_clear(0x8000000000000000ULL, 63) == 0ULL);
}

TEST(bits_toggle_op) {
    u64 v = 0b1010u;
    v = bits_toggle(v, 0);  EXPECT(v == 0b1011u);
    v = bits_toggle(v, 0);  EXPECT(v == 0b1010u);
    /* Toggle MSB */
    EXPECT(bits_toggle(0ULL,               63) == 0x8000000000000000ULL);
    EXPECT(bits_toggle(0x8000000000000000ULL, 63) == 0ULL);
}

/* =========================================================================
 * Multi-bit operations
 * ====================================================================== */

TEST(bits_extract_op) {
    /* Example from the header */
    EXPECT(bits_extract(0b11010110u, 2, 4) == 0b0101u);

    /* Zero count → 0 */
    EXPECT(bits_extract(0xFFFFFFFFFFFFFFFFULL, 0, 0) == 0ULL);

    /* Full width (count >= 64) */
    EXPECT(bits_extract(0xDEADBEEFCAFEBABEULL, 0, 64) ==
           0xDEADBEEFCAFEBABEULL);

    /* Single bit */
    EXPECT(bits_extract(0b10101010u, 1, 1) == 1u);
    EXPECT(bits_extract(0b10101010u, 0, 1) == 0u);

    /* High bits */
    EXPECT(bits_extract(0xF000000000000000ULL, 60, 4) == 0xFu);

    /* Network header example from header comment */
    u32 hdr = 0b10110101001100110000111100001111u;
    EXPECT(bits_extract(hdr, 28, 4) == 0b1011u);
    EXPECT(bits_extract(hdr, 24, 4) == 0b0101u);
}

TEST(bits_insert_op) {
    /* Example from the header */
    u64 result = bits_insert(0b11110000u, 0b0101u, 2, 4);
    EXPECT(result == 0b11010100u);

    /* Zero count → dst unchanged */
    EXPECT(bits_insert(0xDEADBEEFu, 0xFFu, 0, 0) == 0xDEADBEEFu);

    /* Insert single bit */
    EXPECT(bits_insert(0b0000u, 1u, 2, 1) == 0b0100u);
    EXPECT(bits_insert(0b1111u, 0u, 2, 1) == 0b1011u);

    /* Round-trip with extract */
    u64 original = 0xCAFEBABEDEADBEEFULL;
    u64 field    = bits_extract(original, 16, 8);
    u64 restored = bits_insert(original, field, 16, 8);
    EXPECT(restored == original);

    /* Insert into high bits */
    EXPECT(bits_insert(0ULL, 0xFu, 60, 4) == 0xF000000000000000ULL);
}

/* =========================================================================
 * Bit counting
 * ====================================================================== */

TEST(bits_popcount_op) {
    EXPECT(bits_popcount(0b10110101u) == 5u);
    EXPECT(bits_popcount(0ULL) == 0u);
    EXPECT(bits_popcount(~0ULL) == 64u);
    EXPECT(bits_popcount(1ULL) == 1u);
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
    EXPECT(bits_clz(0b00001010u) == 60u);  /* Header example */
    EXPECT(bits_clz(~0ULL) == 0u);
    /* Any value with MSB set → 0 leading zeros */
    EXPECT(bits_clz(0xFFFFFFFFFFFFFFFFULL) == 0u);
}

TEST(bits_ctz_op) {
    EXPECT(bits_ctz(0ULL) == 64u);
    EXPECT(bits_ctz(1ULL) == 0u);
    EXPECT(bits_ctz(2ULL) == 1u);
    EXPECT(bits_ctz(0b10100000u) == 5u);  /* Header example */
    EXPECT(bits_ctz(0x8000000000000000ULL) == 63u);
    EXPECT(bits_ctz(~0ULL) == 0u);
    /* Powers of two */
    for (u32 i = 0; i < 64; i++) {
        EXPECT(bits_ctz(1ULL << i) == i);
    }
}

TEST(bits_ffs_op) {
    EXPECT(bits_ffs(0ULL) == 0u);           /* No bits set */
    EXPECT(bits_ffs(1ULL) == 1u);           /* Bit 0 → position 1 */
    EXPECT(bits_ffs(0b10100000u) == 6u);    /* Header example */
    EXPECT(bits_ffs(0x8000000000000000ULL) == 64u);
    /* Matches ctz+1 for any non-zero value */
    u64 vals[] = { 1ULL, 2ULL, 6ULL, 0xDEADBEEFu, 0x8000000000000000ULL };
    for (int i = 0; i < 5; i++) {
        EXPECT(bits_ffs(vals[i]) == bits_ctz(vals[i]) + 1);
    }
}

TEST(bits_fls_op) {
    EXPECT(bits_fls(0ULL) == 0u);           /* No bits set */
    EXPECT(bits_fls(1ULL) == 1u);
    EXPECT(bits_fls(0b10100000u) == 8u);    /* Header example */
    EXPECT(bits_fls(0x8000000000000000ULL) == 64u);
    /* Matches 64 - clz for any non-zero value */
    u64 vals[] = { 1ULL, 2ULL, 255ULL, 0xDEADBEEFu, 0x8000000000000000ULL };
    for (int i = 0; i < 5; i++) {
        EXPECT(bits_fls(vals[i]) == 64u - bits_clz(vals[i]));
    }
}

/* =========================================================================
 * Rotation
 * ====================================================================== */

TEST(bits_rotl_op) {
    /* Header example: MSB and LSB set, rotate left 1 */
    u64 val = 0x8000000000000001ULL;
    EXPECT(bits_rotl(val, 1) == 0x0000000000000003ULL);

    /* Rotate by 0 → identity */
    EXPECT(bits_rotl(0xDEADBEEFCAFEBABEULL, 0) ==
           0xDEADBEEFCAFEBABEULL);

    /* Rotate by 64 → identity (shift & 63 == 0) */
    EXPECT(bits_rotl(0xDEADBEEFCAFEBABEULL, 64) ==
           0xDEADBEEFCAFEBABEULL);

    /* rotl then rotr should recover original */
    u64 orig = 0x123456789ABCDEF0ULL;
    EXPECT(bits_rotr(bits_rotl(orig, 13), 13) == orig);

    /* All-ones is invariant under rotation */
    EXPECT(bits_rotl(~0ULL, 17) == ~0ULL);
    /* All-zeros is invariant */
    EXPECT(bits_rotl(0ULL, 17) == 0ULL);
}

TEST(bits_rotr_op) {
    /* Header example: MSB and LSB set, rotate right 1 */
    u64 val = 0x8000000000000001ULL;
    EXPECT(bits_rotr(val, 1) == 0xC000000000000000ULL);

    /* Rotate by 0 → identity */
    EXPECT(bits_rotr(0xDEADBEEFCAFEBABEULL, 0) ==
           0xDEADBEEFCAFEBABEULL);

    /* Rotate by 64 → identity */
    EXPECT(bits_rotr(0xDEADBEEFCAFEBABEULL, 64) ==
           0xDEADBEEFCAFEBABEULL);

    /* rotr then rotl should recover original */
    u64 orig = 0xFEDCBA9876543210ULL;
    EXPECT(bits_rotl(bits_rotr(orig, 27), 27) == orig);

    EXPECT(bits_rotr(~0ULL, 23) == ~0ULL);
    EXPECT(bits_rotr(0ULL,  23) == 0ULL);
}

/* =========================================================================
 * Power-of-two
 * ====================================================================== */

TEST(bits_is_power_of_two_op) {
    /* True cases */
    EXPECT(bits_is_power_of_two(1ULL)  == true);
    EXPECT(bits_is_power_of_two(2ULL)  == true);
    EXPECT(bits_is_power_of_two(4ULL)  == true);
    EXPECT(bits_is_power_of_two(16ULL) == true);
    EXPECT(bits_is_power_of_two(1ULL << 63) == true);

    /* False cases */
    EXPECT(bits_is_power_of_two(0ULL)  == false);
    EXPECT(bits_is_power_of_two(3ULL)  == false);
    EXPECT(bits_is_power_of_two(15ULL) == false);
    EXPECT(bits_is_power_of_two(~0ULL) == false);
    EXPECT(bits_is_power_of_two(6ULL)  == false);

    /* All true powers 2^0 … 2^63 */
    for (u32 i = 0; i < 64; i++) {
        EXPECT(bits_is_power_of_two(1ULL << i) == true);
    }
}

TEST(bits_next_power_of_two_op) {
    EXPECT(bits_next_power_of_two(0ULL)   == 0ULL);  /* Special: 0 → 0 */
    EXPECT(bits_next_power_of_two(1ULL)   == 1ULL);
    EXPECT(bits_next_power_of_two(2ULL)   == 2ULL);
    EXPECT(bits_next_power_of_two(3ULL)   == 4ULL);
    EXPECT(bits_next_power_of_two(17ULL)  == 32ULL);
    EXPECT(bits_next_power_of_two(32ULL)  == 32ULL);
    EXPECT(bits_next_power_of_two(1000ULL)== 1024ULL);

    /* Overflow: values > 2^63 → 0 */
    EXPECT(bits_next_power_of_two((1ULL << 63) + 1ULL) == 0ULL);
    EXPECT(bits_next_power_of_two(~0ULL) == 0ULL);

    /* Result is always a power of two (for non-overflow, non-zero inputs) */
    u64 inputs[] = { 1, 5, 7, 100, 255, 256, 1023, 1ULL << 62 };
    for (int i = 0; i < 8; i++) {
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
    EXPECT(bits_bswap64(0ULL) == 0ULL);
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
    for (u32 k = 0; k < 64; k++) {
        u64 v = 1ULL << k;
        EXPECT(bits_popcount(v) == 1u);
        EXPECT(bits_ctz(v)      == k);
        EXPECT(bits_clz(v)      == 63u - k);
    }
}

TEST(set_clear_toggle_consistency) {
    u64 v = 0ULL;
    for (u32 i = 0; i < 64; i++) {
        v = bits_set(v, i);
        EXPECT(bits_test(v, i));
        EXPECT(bits_popcount(v) == i + 1);
    }
    /* Now clear them all */
    for (u32 i = 0; i < 64; i++) {
        v = bits_clear(v, i);
        EXPECT(!bits_test(v, i));
    }
    EXPECT(v == 0ULL);
    /* Toggle each bit twice → back to original */
    u64 original = 0xDEADBEEFCAFEBABEULL;
    u64 toggled  = original;
    for (u32 i = 0; i < 64; i++) toggled = bits_toggle(toggled, i);
    for (u32 i = 0; i < 64; i++) toggled = bits_toggle(toggled, i);
    EXPECT(toggled == original);
}

TEST(extract_insert_roundtrip) {
    /* Exhaustive round-trip: extract field, re-insert it → same value */
    u64 base = 0xFEDCBA9876543210ULL;
    for (u32 start = 0; start < 64; start++) {
        for (u32 count = 1; count <= 64 - start; count++) {
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
/* Fuzz harness — libFuzzer entry point                                    */
/*                                                                          */
/* Interprets the raw byte buffer as:                                       */
/*   bytes [0..7]  → u64 value  (little-endian memcpy)                    */
/*   bytes [8..8]  → bit index  (0-63, masked)                             */
/*   bytes [9..9]  → shift      (0-63, masked)                             */
/*   bytes [10..10]→ field start (0-63, masked)                            */
/*   bytes [11..11]→ field count (1-64, clamped)                           */
/* ---------------------------------------------------------------------- */
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    if (size < 12) return 0;

    u64 value = 0;
    memcpy(&value, data, 8);

    u32 bit   = data[8]  & 63u;
    u32 shift = data[9]  & 63u;
    u32 start = data[10] & 63u;
    u32 count = (u32)(data[11] & 63u) + 1u;  /* 1..64 */
    if (start + count > 64) count = 64 - start;
    if (count == 0) count = 1;

    /* Single-bit ops */
    (void)bits_test(value, bit);
    (void)bits_set(value, bit);
    (void)bits_clear(value, bit);
    (void)bits_toggle(value, bit);

    /* Multi-bit ops */
    u64 field = bits_extract(value, start, count);
    u64 rebuilt = bits_insert(value, field, start, count);
    /* Invariant: round-trip must be identity */
    if (rebuilt != value) __builtin_trap();

    /* Counting */
    (void)bits_popcount(value);
    (void)bits_clz(value);
    (void)bits_ctz(value);
    (void)bits_ffs(value);
    (void)bits_fls(value);

    /* Rotation invariants */
    u64 rotated = bits_rotl(value, shift);
    if (bits_rotr(rotated, shift) != value) __builtin_trap();

    /* Power-of-two */
    (void)bits_is_power_of_two(value);
    u64 next = bits_next_power_of_two(value);
    if (next != 0 && !bits_is_power_of_two(next)) __builtin_trap();
    if (next != 0 && next < value) __builtin_trap();

    /* Byte swap — double-swap must be identity */
    u16 v16 = (u16)value;
    if (bits_bswap16(bits_bswap16(v16)) != v16) __builtin_trap();
    u32 v32 = (u32)value;
    if (bits_bswap32(bits_bswap32(v32)) != v32) __builtin_trap();
    if (bits_bswap64(bits_bswap64(value)) != value) __builtin_trap();

    return 0;
}

#else /* CANON_FUZZING not defined → unit test binary */

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
