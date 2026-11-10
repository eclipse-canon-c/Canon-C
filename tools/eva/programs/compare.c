/*
 * compare.h: each comparator sorts a small array, through the function
 * pointer a sort takes; the float comparators sort a NaN too, which they
 * order last (or first, descending).
 */
#include <string.h>
#define CANON_CONTRACT_IMPL
#include "core/primitives/compare.h"
#include "eva_program.h"

typedef int (*cmp_fn)(const void *a, const void *b, void *ctx);

/* An insertion sort of n elements of size sz, by cmp: the way a caller uses
   the comparators. tmp holds one element. */
static void sort(u8 *base, usize n, usize sz, cmp_fn cmp, u8 *tmp)
{
    for (usize i = 1u; i < n; i++) {
        memcpy(tmp, base + i * sz, sz);
        usize j = i;
        while (j > 0u && cmp(base + (j - 1u) * sz, tmp, NULL) > 0) {
            memcpy(base + j * sz, base + (j - 1u) * sz, sz);
            j--;
        }
        memcpy(base + j * sz, tmp, sz);
    }
}

int main(void)
{
    int fails = 0;
    u8 tmp[8];

    u8 a8[4] = { 3u, 255u, 0u, 7u };
    sort(a8, 4u, sizeof a8[0], algo_cmp_u8, tmp);
    CHECK(a8[0] == 0u && a8[1] == 3u && a8[2] == 7u && a8[3] == 255u);
    sort(a8, 4u, sizeof a8[0], algo_cmp_u8_desc, tmp);
    CHECK(a8[0] == 255u && a8[3] == 0u);

    u16 a16[3] = { 500u, 65535u, 1u };
    sort((u8 *)a16, 3u, sizeof a16[0], algo_cmp_u16, tmp);
    CHECK(a16[0] == 1u && a16[2] == 65535u);
    sort((u8 *)a16, 3u, sizeof a16[0], algo_cmp_u16_desc, tmp);
    CHECK(a16[0] == 65535u && a16[2] == 1u);

    u32 a32[3] = { 70000u, 0u, 4000000000u };
    sort((u8 *)a32, 3u, sizeof a32[0], algo_cmp_u32, tmp);
    CHECK(a32[0] == 0u && a32[2] == 4000000000u);
    sort((u8 *)a32, 3u, sizeof a32[0], algo_cmp_u32_desc, tmp);
    CHECK(a32[0] == 4000000000u);

    u64 a64[3] = { 1u, UINT64_MAX, 0u };
    sort((u8 *)a64, 3u, sizeof a64[0], algo_cmp_u64, tmp);
    CHECK(a64[0] == 0u && a64[2] == UINT64_MAX);
    sort((u8 *)a64, 3u, sizeof a64[0], algo_cmp_u64_desc, tmp);
    CHECK(a64[0] == UINT64_MAX);

    i8 s8[3] = { 5, -128, 127 };
    sort((u8 *)s8, 3u, sizeof s8[0], algo_cmp_i8, tmp);
    CHECK(s8[0] == -128 && s8[2] == 127);
    sort((u8 *)s8, 3u, sizeof s8[0], algo_cmp_i8_desc, tmp);
    CHECK(s8[0] == 127);

    i16 s16[3] = { 0, -32768, 32767 };
    sort((u8 *)s16, 3u, sizeof s16[0], algo_cmp_i16, tmp);
    CHECK(s16[0] == -32768 && s16[2] == 32767);
    sort((u8 *)s16, 3u, sizeof s16[0], algo_cmp_i16_desc, tmp);
    CHECK(s16[0] == 32767);

    i32 s32[3] = { -1, INT32_MIN, INT32_MAX };
    sort((u8 *)s32, 3u, sizeof s32[0], algo_cmp_i32, tmp);
    CHECK(s32[0] == INT32_MIN && s32[1] == -1);
    sort((u8 *)s32, 3u, sizeof s32[0], algo_cmp_i32_desc, tmp);
    CHECK(s32[0] == INT32_MAX);

    i64 s64[3] = { INT64_MAX, 0, INT64_MIN };
    sort((u8 *)s64, 3u, sizeof s64[0], algo_cmp_i64, tmp);
    CHECK(s64[0] == INT64_MIN && s64[2] == INT64_MAX);
    sort((u8 *)s64, 3u, sizeof s64[0], algo_cmp_i64_desc, tmp);
    CHECK(s64[0] == INT64_MAX);

    usize z[3] = { 2u, SIZE_MAX, 1u };
    sort((u8 *)z, 3u, sizeof z[0], algo_cmp_usize, tmp);
    CHECK(z[0] == 1u && z[2] == SIZE_MAX);
    sort((u8 *)z, 3u, sizeof z[0], algo_cmp_usize_desc, tmp);
    CHECK(z[0] == SIZE_MAX);

    isize s[3] = { 2, PTRDIFF_MIN, -2 };
    sort((u8 *)s, 3u, sizeof s[0], algo_cmp_isize, tmp);
    CHECK(s[0] == PTRDIFF_MIN && s[2] == 2);
    sort((u8 *)s, 3u, sizeof s[0], algo_cmp_isize_desc, tmp);
    CHECK(s[0] == 2);

    /* A quiet NaN, from its bits: no arithmetic makes it. */
    union { u32 bits; f32 value; } nan32 = { 0x7FC00000u };
    union { u64 bits; f64 value; } nan64 = { 0x7FF8000000000000u };
    f32 f[3] = { 2.5f, nan32.value, -1.0f };
    sort((u8 *)f, 3u, sizeof f[0], algo_cmp_f32, tmp);
    CHECK(f[0] == -1.0f && f[1] == 2.5f && f[2] != f[2]);
    sort((u8 *)f, 3u, sizeof f[0], algo_cmp_f32_desc, tmp);
    CHECK(f[0] != f[0] && f[1] == 2.5f);
    f64 d[3] = { nan64.value, 0x1p1000, -0x1p1000 };
    sort((u8 *)d, 3u, sizeof d[0], algo_cmp_f64, tmp);
    CHECK(d[0] == -0x1p1000 && d[1] == 0x1p1000 && d[2] != d[2]);
    sort((u8 *)d, 3u, sizeof d[0], algo_cmp_f64_desc, tmp);
    CHECK(d[0] != d[0]);

    END(fails);
    return fails != 0;
}
