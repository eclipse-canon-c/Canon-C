#include <stdio.h>
#include <stdlib.h>
#include "bits.h"

/* ============================================================================
 * Test Helpers
 * ========================================================================= */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(label, expected, actual)                                      \
    do {                                                                        \
        tests_run++;                                                            \
        if ((u64)(expected) == (u64)(actual)) {                                 \
            tests_passed++;                                                     \
        } else {                                                                \
            tests_failed++;                                                     \
            fprintf(stderr, "FAIL: %s — expected %llu, got %llu  (%s:%d)\n",   \
                    label, (unsigned long long)(expected),                      \
                    (unsigned long long)(actual), __FILE__, __LINE__);          \
        }                                                                       \
    } while (0)

#define ASSERT_TRUE(label, expr)  ASSERT_EQ(label, 1, !!(expr))
#define ASSERT_FALSE(label, expr) ASSERT_EQ(label, 0, !!(expr))

/* ============================================================================
 * bits_test / bits_set / bits_clear / bits_toggle
 * ========================================================================= */

static void test_bits_test(void) {
    ASSERT_TRUE ("bits_test: bit 1 set in 0b1010",   bits_test(0b1010, 1));
    ASSERT_FALSE("bits_test: bit 2 clear in 0b1010", bits_test(0b1010, 2));
    ASSERT_TRUE ("bits_test: bit 0 set in 1",        bits_test(1, 0));
    ASSERT_FALSE("bits_test: no bits set in 0",      bits_test(0, 0));
    ASSERT_TRUE ("bits_test: MSB set",               bits_test(1ULL << 63, 63));
}

static void test_bits_set(void) {
    ASSERT_EQ("bits_set: set bit 3",       0b1000ULL, bits_set(0b0000, 3));
    ASSERT_EQ("bits_set: already set",     0b1111ULL, bits_set(0b1111, 2));
    ASSERT_EQ("bits_set: set bit 0",       0b0001ULL, bits_set(0b0000, 0));
    ASSERT_EQ("bits_set: set MSB",         1ULL << 63, bits_set(0, 63));
}

static void test_bits_clear(void) {
    ASSERT_EQ("bits_clear: clear bit 2",   0b1011ULL, bits_clear(0b1111, 2));
    ASSERT_EQ("bits_clear: already clear", 0b1010ULL, bits_clear(0b1010, 0));
    ASSERT_EQ("bits_clear: clear bit 0",   0b1110ULL, bits_clear(0b1111, 0));
    ASSERT_EQ("bits_clear: clear MSB",     0ULL,      bits_clear(1ULL << 63, 63));
}

static void test_bits_toggle(void) {
    ASSERT_EQ("bits_toggle: 0→1",          0b1011ULL, bits_toggle(0b1010, 0));
    ASSERT_EQ("bits_toggle: 1→0",          0b1010ULL, bits_toggle(0b1011, 0));
    ASSERT_EQ("bits_toggle: double toggle", 0b1010ULL, bits_toggle(bits_toggle(0b1010, 3), 3));
}

/* ============================================================================
 * bits_extract / bits_insert
 * ========================================================================= */

static void test_bits_extract(void) {
    ASSERT_EQ("bits_extract: middle 4 bits", 0b0101ULL, bits_extract(0b11010110, 2, 4));
    ASSERT_EQ("bits_extract: bit 0",         1ULL,      bits_extract(0b1111, 0, 1));
    ASSERT_EQ("bits_extract: count 0",       0ULL,      bits_extract(0b1111, 0, 0));
    ASSERT_EQ("bits_extract: full 64 bits",  ~0ULL,     bits_extract(~0ULL, 0, 64));
    ASSERT_EQ("bits_extract: top 4 bits",    0b1010ULL, bits_extract(0b10100000, 4, 4));
}

static void test_bits_insert(void) {
    ASSERT_EQ("bits_insert: basic",          0b11010100ULL, bits_insert(0b11110000, 0b0101, 2, 4));
    ASSERT_EQ("bits_insert: count 0",        0b11110000ULL, bits_insert(0b11110000, 0b1111, 0, 0));
    ASSERT_EQ("bits_insert: at bit 0",       0b11110101ULL, bits_insert(0b11110000, 0b0101, 0, 4));
    /* round-trip: insert then extract */
    u64 val = bits_insert(0ULL, 0b1011, 4, 4);
    ASSERT_EQ("bits_insert/extract round-trip", 0b1011ULL, bits_extract(val, 4, 4));
}

/* ============================================================================
 * bits_popcount
 * ========================================================================= */

static void test_bits_popcount(void) {
    ASSERT_EQ("bits_popcount: 0",          0U, bits_popcount(0));
    ASSERT_EQ("bits_popcount: 1",          1U, bits_popcount(1));
    ASSERT_EQ("bits_popcount: 0b10110101", 5U, bits_popcount(0b10110101));
    ASSERT_EQ("bits_popcount: all 64",    64U, bits_popcount(~0ULL));
    ASSERT_EQ("bits_popcount: 0xF0F0",    8U,  bits_popcount(0xF0F0));
}

/* ============================================================================
 * bits_clz / bits_ctz / bits_ffs / bits_fls
 * ========================================================================= */

static void test_bits_clz(void) {
    ASSERT_EQ("bits_clz: 0",         64U, bits_clz(0));
    ASSERT_EQ("bits_clz: 1",         63U, bits_clz(1));
    ASSERT_EQ("bits_clz: 0b1000",    60U, bits_clz(0b1000));
    ASSERT_EQ("bits_clz: MSB set",    0U, bits_clz(1ULL << 63));
    ASSERT_EQ("bits_clz: all ones",   0U, bits_clz(~0ULL));
}

static void test_bits_ctz(void) {
    ASSERT_EQ("bits_ctz: 0",          64U, bits_ctz(0));
    ASSERT_EQ("bits_ctz: 1",           0U, bits_ctz(1));
    ASSERT_EQ("bits_ctz: 0b10100000",  5U, bits_ctz(0b10100000));
    ASSERT_EQ("bits_ctz: MSB only",   63U, bits_ctz(1ULL << 63));
    ASSERT_EQ("bits_ctz: all ones",    0U, bits_ctz(~0ULL));
}

static void test_bits_ffs(void) {
    ASSERT_EQ("bits_ffs: 0",           0U, bits_ffs(0));
    ASSERT_EQ("bits_ffs: 1",           1U, bits_ffs(1));
    ASSERT_EQ("bits_ffs: 0b10100000",  6U, bits_ffs(0b10100000));
    ASSERT_EQ("bits_ffs: MSB only",   64U, bits_ffs(1ULL << 63));
}

static void test_bits_fls(void) {
    ASSERT_EQ("bits_fls: 0",           0U, bits_fls(0));
    ASSERT_EQ("bits_fls: 1",           1U, bits_fls(1));
    ASSERT_EQ("bits_fls: 0b10100000",  8U, bits_fls(0b10100000));
    ASSERT_EQ("bits_fls: MSB only",   64U, bits_fls(1ULL << 63));
    ASSERT_EQ("bits_fls: all ones",   64U, bits_fls(~0ULL));
}

/* ============================================================================
 * bits_rotl / bits_rotr
 * ========================================================================= */

static void test_bits_rotl(void) {
    ASSERT_EQ("bits_rotl: shift 0",     0b1011ULL,  bits_rotl(0b1011, 0));
    ASSERT_EQ("bits_rotl: shift 1",     0b10110ULL, bits_rotl(0b1011, 1));
    /* Rotating by 64 should be identity */
    ASSERT_EQ("bits_rotl: shift 64",    0b1011ULL,  bits_rotl(0b1011, 64));
    /* Round-trip: rotate left then right */
    ASSERT_EQ("bits_rotl round-trip",   0xDEADBEEFULL, bits_rotr(bits_rotl(0xDEADBEEFULL, 17), 17));
}

static void test_bits_rotr(void) {
    ASSERT_EQ("bits_rotr: shift 0",     0b1011ULL, bits_rotr(0b1011, 0));
    /* MSB should wrap around */
    ASSERT_EQ("bits_rotr: wrap MSB",    1ULL << 63, bits_rotr(1ULL, 1));
    ASSERT_EQ("bits_rotr: shift 64",    0b1011ULL,  bits_rotr(0b1011, 64));
}

/* ============================================================================
 * bits_is_power_of_two / bits_next_power_of_two
 * ========================================================================= */

static void test_bits_is_power_of_two(void) {
    ASSERT_FALSE("bits_is_power_of_two: 0",  bits_is_power_of_two(0));
    ASSERT_TRUE ("bits_is_power_of_two: 1",  bits_is_power_of_two(1));
    ASSERT_TRUE ("bits_is_power_of_two: 2",  bits_is_power_of_two(2));
    ASSERT_TRUE ("bits_is_power_of_two: 16", bits_is_power_of_two(16));
    ASSERT_FALSE("bits_is_power_of_two: 15", bits_is_power_of_two(15));
    ASSERT_FALSE("bits_is_power_of_two: 3",  bits_is_power_of_two(3));
    ASSERT_TRUE ("bits_is_power_of_two: 1<<63", bits_is_power_of_two(1ULL << 63));
}

static void test_bits_next_power_of_two(void) {
    ASSERT_EQ("bits_next_power_of_two: 0",    0ULL,   bits_next_power_of_two(0));
    ASSERT_EQ("bits_next_power_of_two: 1",    1ULL,   bits_next_power_of_two(1));
    ASSERT_EQ("bits_next_power_of_two: 2",    2ULL,   bits_next_power_of_two(2));
    ASSERT_EQ("bits_next_power_of_two: 17",   32ULL,  bits_next_power_of_two(17));
    ASSERT_EQ("bits_next_power_of_two: 32",   32ULL,  bits_next_power_of_two(32));
    ASSERT_EQ("bits_next_power_of_two: 1000", 1024ULL,bits_next_power_of_two(1000));
    /* Overflow: value > 2^63 → 0 */
    ASSERT_EQ("bits_next_power_of_two: overflow", 0ULL, bits_next_power_of_two((1ULL << 63) + 1));
}

/* ============================================================================
 * bits_bswap16 / bits_bswap32 / bits_bswap64
 * ========================================================================= */

static void test_bits_bswap(void) {
    ASSERT_EQ("bits_bswap16: 0x1234",       0x3412U,            bits_bswap16(0x1234));
    ASSERT_EQ("bits_bswap16: identity",     0x0000U,            bits_bswap16(0x0000));
    ASSERT_EQ("bits_bswap32: 0x12345678",   0x78563412UL,       bits_bswap32(0x12345678));
    ASSERT_EQ("bits_bswap32: round-trip",   0x12345678UL,       bits_bswap32(bits_bswap32(0x12345678)));
    ASSERT_EQ("bits_bswap64: 0x0102030405060708",
              0x0807060504030201ULL, bits_bswap64(0x0102030405060708ULL));
    ASSERT_EQ("bits_bswap64: round-trip",
              0xDEADBEEFCAFEBABEULL, bits_bswap64(bits_bswap64(0xDEADBEEFCAFEBABEULL)));
}

/* ============================================================================
 * Main
 * ========================================================================= */

int main(void) {
    test_bits_test();
    test_bits_set();
    test_bits_clear();
    test_bits_toggle();

    test_bits_extract();
    test_bits_insert();

    test_bits_popcount();

    test_bits_clz();
    test_bits_ctz();
    test_bits_ffs();
    test_bits_fls();

    test_bits_rotl();
    test_bits_rotr();

    test_bits_is_power_of_two();
    test_bits_next_power_of_two();

    test_bits_bswap();

    printf("\nResults: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED\n", tests_failed);
        return 1;
    }
    printf(" — all tests passed!\n");
    return 0;
}
