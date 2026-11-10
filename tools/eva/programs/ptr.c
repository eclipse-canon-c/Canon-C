/*
 * ptr.h: alignment of sizes and of pointers into a buffer, offsets,
 * distances and range tests within the buffer, element addresses, the
 * null fallbacks; and contract.h's handler setter.
 */
#define CANON_CONTRACT_IMPL
#include "core/primitives/ptr.h"
#include "eva_program.h"

static void on_violation(const char *file, int line, const char *func,
                         const char *expr, const char *msg)
{
    (void)file; (void)line; (void)func; (void)expr; (void)msg;
    abort();
}

int main(void)
{
    int fails = 0;
    static u64 words[8];
    u8 *const buf = (u8 *)words;          /* 64 bytes, aligned for u64 */
    u8 *const end = buf + sizeof words;

    contract_set_handler(on_violation);

    CHECK(is_power_of_two(64u) && !is_power_of_two(0u) && !is_power_of_two(12u));
    CHECK(is_aligned(48u, 16u) && !is_aligned(50u, 16u));
    CHECK(align_up(50u, 16u) == 64u && align_up(48u, 16u) == 48u);
    CHECK(align_down(50u, 16u) == 48u);
    CHECK(align_padding(50u, 16u) == 14u && align_padding(64u, 16u) == 0u);

    /* An address one byte into the buffer, aligned up and down to 8. */
    u8 *odd = ptr_offset(buf, 1u);
    usize pad = ptr_align_padding(odd, 8u);
    CHECK(pad < 8u);
    u8 *up = ptr_align_up(odd, 8u);
    u8 *down = ptr_align_down(odd, 8u);
    CHECK(up != NULL && down != NULL);
    CHECK(ptr_is_aligned(buf, 8u) && !ptr_is_aligned(odd, 2u));
    CHECK(ptr_align_up(NULL, 8u) == NULL && ptr_align_down(NULL, 8u) == NULL);
    CHECK(!ptr_is_aligned(NULL, 8u) && ptr_align_padding(NULL, 8u) == 0u);

    u8 *mid = ptr_offset(buf, 32u);
    const u8 *cmid = ptr_offset_const(buf, 32u);
    CHECK(mid == cmid && ptr_offset(NULL, 4u) == NULL);
    CHECK(ptr_retreat(mid, 32u) == buf && ptr_retreat(NULL, 1u) == NULL);
    CHECK(ptr_diff(mid, buf) == 32 && ptr_diff(buf, mid) == -32);
    CHECK(ptr_span(end - 1, buf) == 63u);
    CHECK(ptr_in_range(mid, buf, end) && !ptr_in_range(end, buf, end));
    CHECK(!ptr_in_range(NULL, buf, end));
    CHECK(ptr_range_in_range(mid, 32u, buf, end) && !ptr_range_in_range(mid, 33u, buf, end));

    u64 *third = ptr_elem(words, 3u, sizeof words[0]);
    *third = 42u;
    const u64 *cthird = ptr_elem_const(words, 3u, sizeof words[0]);
    CHECK(*cthird == 42u && words[3] == 42u);

    CHECK(ptr_or(NULL, buf) == buf && ptr_or(mid, buf) == mid);
    CHECK(ptr_or_const(NULL, cmid) == cmid);
    CHECK(ptr_is_valid(buf) && !ptr_is_valid(NULL));

    END(fails);
    return fails != 0;
}
