/*
 * bitset.h (with option_usize, as its proof instantiates it,
 * vmacros/vdrivers/bitset_verify.h): a set of 100 bits, over two words,
 * set, cleared, toggled, searched, counted and combined with another.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/bitset_verify.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    u64 words[BITSET_WORD_COUNT(100)];
    u64 other_words[BITSET_WORD_COUNT(100)];
    Bitset bs;
    Bitset other;
    usize i = 0u;

    bitset_init(&bs, words, 100u);
    bitset_init(&other, other_words, 100u);
    CHECK(bitset_capacity(&bs) == 100u && bitset_is_empty(&bs));
    CHECK(bitset_word_of(70u) == 1u && bitset_mask_of(70u) == (u64)1u << 6);
    bitset_set(&bs, 3u);
    bitset_set(&bs, 64u);
    bitset_set(&bs, 99u);
    bitset_toggle(&bs, 5u);
    bitset_assign(&bs, 7u, true);
    bitset_clear(&bs, 7u);
    CHECK(bitset_test(&bs, 64u) && bitset_test(&bs, 5u) && !bitset_test(&bs, 7u));
    CHECK(bitset_count(&bs) == 4u);
    CHECK(bitset_find_first(&bs) == 3u && bitset_find_next(&bs, 5u) == 64u);
    CHECK(bitset_find_last(&bs) == 99u && bitset_find_next(&bs, 99u) == BITSET_NPOS);
    CHECK(option_usize_get(bitset_find_first_option(&bs), &i) && i == 3u);
    CHECK(option_usize_get(bitset_find_next_option(&bs, 3u), &i) && i == 5u);
    CHECK(option_usize_get(bitset_find_last_option(&bs), &i) && i == 99u);

    bitset_set(&other, 64u);
    bitset_set(&other, 10u);
    CHECK(!bitset_is_disjoint(&bs, &other));
    bitset_and(&other, &bs);                         /* {64} */
    CHECK(bitset_count(&other) == 1u);
    bitset_or(&other, &bs);                          /* bs */
    bitset_xor(&other, &bs);                         /* {} */
    CHECK(bitset_is_empty(&other) && bitset_is_disjoint(&bs, &other));
    bitset_not(&other);
    CHECK(bitset_is_full(&other) && bitset_count(&other) == 100u);
    bitset_clear_all(&other);
    bitset_set_all(&bs);
    CHECK(bitset_is_full(&bs) && bitset_is_empty(&other));
    bitset_clear_padding(&bs);

    CHECK(bitset_as_bytes(&bs).len == sizeof words);
    CHECK(bitset_as_cbytes(&bs).len == sizeof words);
    borrowed_bytes view = bitset_as_borrowed_bytes(&bs);
    CHECK(borrowed_bytes_len(&view) == sizeof words);
    bitset_close(&bs);
    bitset_close(&other);

    END(fails);
    return fails != 0;
}
