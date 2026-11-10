/*
 * array.h, instantiated as an array of 8 ints with slice and option as its
 * proof instantiates them (vmacros/vdrivers/array_verify.h): arrays made,
 * read, written, compared and copied, and the slice of one.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/array_verify.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    static const int src[5] = { 1, 2, 3, 4, 5 };
    array_int_8 a = array_int_8_from_ptr(src, 5u);
    array_int_8 z = array_int_8_zero();
    array_int_8 f = array_int_8_fill(7);
    int x = 0;

    CHECK(array_int_8_len() == 8u && array_int_8_size_bytes() == 8u * sizeof(int));
    CHECK(array_int_8_get(&a, 4u, &x) && x == 5 && !array_int_8_get(&a, 8u, &x));
    CHECK(array_int_8_get_unchecked(&a, 5u) == 0 && array_int_8_get_unchecked(&f, 7u) == 7);
    CHECK(option_int_unwrap_or(array_int_8_get_option(&a, 0u), -1) == 1);
    CHECK(option_int_is_none(array_int_8_get_option(&a, 8u)));
    CHECK(array_int_8_set(&a, 7u, 8) && !array_int_8_set(&a, 8u, 0));
    CHECK(*array_int_8_at(&a, 1u) == 2 && array_int_8_at(&a, 8u) == NULL);
    CHECK(*array_int_8_first(&a) == 1 && *array_int_8_last(&a) == 8);
    CHECK(option_int_unwrap_or(array_int_8_first_option(&a), 0) == 1);
    CHECK(option_int_unwrap_or(array_int_8_last_option(&a), 0) == 8);
    CHECK(!array_int_8_equal(&z, &f));
    array_int_8_fill_all(&z, 7);
    CHECK(array_int_8_equal(&z, &f));
    array_int_8_copy_from(&z, &a);
    CHECK(array_int_8_equal(&z, &a));
    CHECK(array_int_8_as_bytes(&a).len == sizeof(int) * 8u);
    CHECK(array_int_8_as_cbytes(&a).len == sizeof(int) * 8u);

    slice_int s = array_int_8_as_slice(&a);
    CHECK(slice_int_len(s) == 8u && !slice_int_is_empty(s));
    CHECK(slice_int_get(s, 2u, &x) && x == 3 && slice_int_get_unchecked(s, 3u) == 4);
    CHECK(*slice_int_at(s, 0u) == 1 && *slice_int_first(s) == 1 && *slice_int_last(s) == 8);
    slice_int mid = slice_int_slice(s, 2u, 5u);
    CHECK(slice_int_len(mid) == 3u && *slice_int_first(mid) == 3);
    CHECK(slice_int_len(slice_int_take(s, 3u)) == 3u && slice_int_len(slice_int_skip(s, 6u)) == 2u);
    CHECK(slice_int_as_bytes(mid).len == 3u * sizeof(int));
    CHECK(slice_int_as_cbytes(mid).len == 3u * sizeof(int));
    int raw[2] = { 9, 10 };
    slice_int r = slice_int_from(raw, 2u);
    CHECK(slice_int_len(r) == 2u && slice_int_is_empty(slice_int_empty()));

    END(fails);
    return fails != 0;
}
