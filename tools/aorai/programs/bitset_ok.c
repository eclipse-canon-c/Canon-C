/*
 * bitset_views.ya's rule, followed: a view of a bitset read while it is
 * open; the bitset closed, initialised again, and a new view taken and
 * read. Aorai with Eva: follows. Built with CANON_LIFETIME_DEBUG, which
 * checks each read as it runs, it returns 0.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/bitset_verify.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    u64 words[BITSET_WORD_COUNT(100)];
    Bitset bs;

    bitset_init(&bs, words, 100u);
    bitset_set(&bs, 3u);
    borrowed_bytes view = bitset_as_borrowed_bytes(&bs);
    CHECK(borrowed_bytes_get(&view).len == sizeof words);
    bitset_close(&bs);

    bitset_init(&bs, words, 100u);
    view = bitset_as_borrowed_bytes(&bs);
    CHECK(borrowed_bytes_get(&view).len == sizeof words && !bitset_test(&bs, 3u));
    bitset_close(&bs);

    END(fails);
    return fails != 0;
}
