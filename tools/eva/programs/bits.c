/* bits.h: every operation on a few values, the edge bits 0 and 63 among them. */
#define CANON_CONTRACT_IMPL
#include "core/primitives/bits.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    const u64 v = 0x00F0u;

    CHECK(bits_test(v, 4u) && !bits_test(v, 3u));
    CHECK(bits_set(0u, 63u) == (u64)1u << 63);
    CHECK(bits_clear(v, 4u) == 0x00E0u);
    CHECK(bits_toggle(v, 0u) == 0x00F1u);
    CHECK(bits_extract(v, 4u, 4u) == 0xFu);
    CHECK(bits_extract(UINT64_MAX, 0u, 64u) == UINT64_MAX);
    CHECK(bits_insert(0u, 0x5u, 60u, 4u) == (u64)0x5u << 60);
    CHECK(bits_popcount(v) == 4u);
    CHECK(bits_popcount(UINT64_MAX) == 64u);
    CHECK(bits_clz(1u) == 63u && bits_clz(0u) == 64u);
    CHECK(bits_ctz(v) == 4u && bits_ctz(0u) == 64u);
    CHECK(bits_ffs(v) == 5u && bits_ffs(0u) == 0u);
    CHECK(bits_fls(v) == 8u && bits_fls(0u) == 0u);
    CHECK(bits_rotl(1u, 63u) == (u64)1u << 63);
    CHECK(bits_rotr(1u, 1u) == (u64)1u << 63);
    CHECK(bits_rotl(v, 0u) == v && bits_rotr(v, 64u) == v);
    CHECK(bits_is_power_of_two(64u) && !bits_is_power_of_two(0u) && !bits_is_power_of_two(v));
    CHECK(bits_next_power_of_two(65u) == 128u && bits_next_power_of_two(1u) == 1u);
    CHECK(bits_bswap16(0x1234u) == 0x3412u);
    CHECK(bits_bswap32(0x12345678u) == 0x78563412u);
    CHECK(bits_bswap64(0x0102030405060708u) == 0x0807060504030201u);

    END(fails);
    return fails != 0;
}
