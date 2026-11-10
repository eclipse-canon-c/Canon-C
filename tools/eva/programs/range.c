/*
 * range.h (with option_isize, as its proof instantiates it,
 * vmacros/vdrivers/range_verify.h): ranges up, down and by steps, walked
 * to their end, peeked, skipped and reset; and the empty ones.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/range_verify.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    isize v = 0;

    range up = range_upto(5);
    isize sum = 0;
    while (range_has_next(&up)) { sum += range_next(&up); }
    CHECK(sum == 10 && !range_has_next(&up) && range_remaining(&up) == 0u);

    range evens = range_make(0, 10, 2);
    CHECK(range_is_valid(&evens) && range_len(&evens) == 5u);
    CHECK(range_peek(&evens, &v) && v == 0);
    range_skip(&evens, 2u);
    CHECK(range_next(&evens) == 4 && range_remaining(&evens) == 2u);
    range_reset(&evens, 1);
    CHECK(range_next(&evens) == 1);

    range down = range_downto(3, -3);
    CHECK(range_len(&down) == 6u && range_next(&down) == 3);
    range from = range_from_to(-2, 2);
    CHECK(range_len(&from) == 4u && range_next(&from) == -2);
    range count_down = range_downfrom(3);
    CHECK(range_len(&count_down) == 3u && range_next(&count_down) == 3);

    range none = range_from_to(4, 4);
    CHECK(range_is_empty(&none) && !range_peek(&none, &v) && range_len(&none) == 0u);
    CHECK(range_is_empty(NULL) && range_len(NULL) == 0u);

    END(fails);
    return fails != 0;
}
