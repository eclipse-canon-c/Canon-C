/*
 * checked.h: each operation, for each width, once with operands it can
 * compute and once with operands it reports (overflow, division by zero,
 * ISIZE_MIN / -1).
 */
#define CANON_CONTRACT_IMPL
#include "core/primitives/checked.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    usize z = 0;
    u8 b = 0;
    u16 h = 0;
    u32 w = 0;
    u64 q = 0;
    isize s = 0;

    CHECK(checked_add(40u, 2u, &z) && z == 42u);
    CHECK(!checked_add(CANON_USIZE_MAX, 1u, &z));
    CHECK(checked_add_u8(200u, 55u, &b) && b == 255u);
    CHECK(!checked_add_u8(200u, 56u, &b));
    CHECK(checked_add_u16(60000u, 5535u, &h) && h == 65535u);
    CHECK(!checked_add_u16(60000u, 5536u, &h));
    CHECK(checked_add_u32(4000000000u, 294967295u, &w) && w == CANON_U32_MAX);
    CHECK(!checked_add_u32(CANON_U32_MAX, 1u, &w));
    CHECK(checked_add_u64(1u, 2u, &q) && q == 3u);
    CHECK(!checked_add_u64(CANON_U64_MAX, 1u, &q));

    CHECK(checked_sub(42u, 2u, &z) && z == 40u);
    CHECK(!checked_sub(1u, 2u, &z));
    CHECK(checked_sub_u8(10u, 10u, &b) && b == 0u);
    CHECK(!checked_sub_u8(0u, 1u, &b));
    CHECK(checked_sub_u16(1000u, 1u, &h) && h == 999u);
    CHECK(!checked_sub_u16(0u, 1u, &h));
    CHECK(checked_sub_u32(7u, 3u, &w) && w == 4u);
    CHECK(!checked_sub_u32(3u, 7u, &w));
    CHECK(checked_sub_u64(7u, 7u, &q) && q == 0u);
    CHECK(!checked_sub_u64(0u, 1u, &q));

    CHECK(checked_mul(6u, 7u, &z) && z == 42u);
    CHECK(!checked_mul(CANON_USIZE_MAX, 2u, &z));
    CHECK(checked_mul_u8(15u, 17u, &b) && b == 255u);
    CHECK(!checked_mul_u8(16u, 16u, &b));
    CHECK(checked_mul_u16(255u, 257u, &h) && h == 65535u);
    CHECK(!checked_mul_u16(256u, 256u, &h));
    CHECK(checked_mul_u32(65535u, 65537u, &w) && w == CANON_U32_MAX);
    CHECK(!checked_mul_u32(65536u, 65536u, &w));
    CHECK(checked_mul_u64(4294967296u, 4294967295u, &q));
    CHECK(!checked_mul_u64(4294967296u, 4294967296u, &q));

    CHECK(checked_add_isize(-5, 3, &s) && s == -2);
    CHECK(!checked_add_isize(CANON_ISIZE_MAX, 1, &s));
    CHECK(checked_sub_isize(-5, 3, &s) && s == -8);
    CHECK(!checked_sub_isize(CANON_ISIZE_MIN, 1, &s));
    CHECK(checked_mul_isize(-6, 7, &s) && s == -42);
    CHECK(!checked_mul_isize(CANON_ISIZE_MIN, -1, &s));

    CHECK(checked_div(42u, 5u, &z) && z == 8u);
    CHECK(!checked_div(42u, 0u, &z));
    CHECK(checked_div_u8(255u, 16u, &b) && b == 15u);
    CHECK(!checked_div_u8(1u, 0u, &b));
    CHECK(checked_div_u16(65535u, 255u, &h) && h == 257u);
    CHECK(!checked_div_u16(1u, 0u, &h));
    CHECK(checked_div_u32(100u, 7u, &w) && w == 14u);
    CHECK(!checked_div_u32(1u, 0u, &w));
    CHECK(checked_div_u64(100u, 7u, &q) && q == 14u);
    CHECK(!checked_div_u64(1u, 0u, &q));
    CHECK(checked_div_isize(-42, 5, &s) && s == -8);
    CHECK(!checked_div_isize(-42, 0, &s));
    CHECK(!checked_div_isize(CANON_ISIZE_MIN, -1, &s));

    CHECK(checked_mod(42u, 5u, &z) && z == 2u);
    CHECK(!checked_mod(42u, 0u, &z));
    CHECK(checked_mod_u8(255u, 16u, &b) && b == 15u);
    CHECK(!checked_mod_u8(1u, 0u, &b));
    CHECK(checked_mod_u16(1000u, 7u, &h) && h == 6u);
    CHECK(!checked_mod_u16(1u, 0u, &h));
    CHECK(checked_mod_u32(100u, 7u, &w) && w == 2u);
    CHECK(!checked_mod_u32(1u, 0u, &w));
    CHECK(checked_mod_u64(100u, 7u, &q) && q == 2u);
    CHECK(!checked_mod_u64(1u, 0u, &q));
    CHECK(checked_mod_isize(-42, 5, &s) && s == -2);
    CHECK(!checked_mod_isize(-42, 0, &s));
    CHECK(!checked_mod_isize(CANON_ISIZE_MIN, -1, &s));

    END(fails);
    return fails != 0;
}
