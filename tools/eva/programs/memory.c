/*
 * memory.h: heap blocks (allocated, checked, freed), alignment queries,
 * copying, moving, filling, comparing and swapping buffers, raw and
 * through byte views.
 */
#define CANON_CONTRACT_IMPL
#include "core/memory.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    u8 a[16];
    u8 b[16];
    u8 scratch[16];
    mem_set(a, 0x11, sizeof a);
    mem_zero(b, sizeof b);

    CHECK(mem_is_all(a, 0x11, sizeof a) && mem_is_zero(b, sizeof b));
    CHECK(!mem_regions_overlap(a, b, sizeof a) && mem_regions_overlap(a, a + 8, 9u));
    CHECK(mem_align(13u) % sizeof(void *) == 0u && mem_align(13u) >= 13u);
    CHECK(mem_align_to(13u, 8u) == 16u && mem_is_power_of_two(8u) && !mem_is_power_of_two(6u));
    CHECK(mem_is_aligned(a, 1u) && mem_get_alignment(a) >= 1u);

    mem_copy(b, a, 8u);
    CHECK(mem_equal(a, b, 8u) && !mem_equal(a, b, 16u));
    CHECK(mem_compare(a, b, 8u) == 0 && mem_compare(a, b, 16u) > 0);
    mem_move(a + 2, a, 8u);           /* overlapping */
    CHECK(a[9] == 0x11u);
    mem_swap(a, b, 16u);
    CHECK(b[0] == 0x11u && a[15] == 0u);
    mem_swap_buf(a, b, 16u, scratch, sizeof scratch);
    CHECK(a[0] == 0x11u && b[15] == 0u);
    mem_secure_zero(a, 4u);
    CHECK(mem_is_zero(a, 4u));

    bytes_t va = bytes_from(a, sizeof a);
    bytes_t vb = bytes_from(b, sizeof b);
    mem_set_bytes(vb, 0x22);
    CHECK(mem_copy_bytes(va, bytes_as_const(vb)) == 16u);
    CHECK(mem_equal_bytes(bytes_as_const(va), bytes_as_const(vb)));
    CHECK(mem_move_bytes(bytes_slice(va, 0u, 8u), cbytes_from(a + 4, 8u)) == 8u);
    mem_zero_bytes(vb);
    CHECK(mem_is_zero_bytes(bytes_as_const(vb)));
    mem_secure_zero_bytes(va);
    CHECK(mem_is_zero_bytes(bytes_as_const(va)));

    u32 *block = mem_alloc(4u * sizeof(u32));
    if (block != NULL) {
        block[3] = 7u;
        CHECK(block[3] == 7u);
        mem_free(block);
    }
    u64 *arr = mem_alloc_array_checked(sizeof(u64), 4u);
    if (arr != NULL) {
        arr[0] = 1u;
        mem_free(arr);
    }
    CHECK(mem_alloc_array_checked(SIZE_MAX, 2u) == NULL);
    mem_free(NULL);

    END(fails);
    return fails != 0;
}
