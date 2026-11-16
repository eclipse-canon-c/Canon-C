/*
 * bitset_views.ya's rule, broken: a view taken before its bitset was
 * closed, read after the bitset was initialised again and before any new
 * view was taken. Aorai with Eva: breaks, at the call marked "refused".
 * Built with CANON_LIFETIME_DEBUG, the read fails its check as it runs: the
 * new init drew a new lifetime.
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
    borrowed_bytes view = bitset_as_borrowed_bytes(&bs);
    bitset_close(&bs);
    bitset_init(&bs, words, 100u);
    cbytes_t old = borrowed_bytes_get(&view);   /* refused */
    CHECK(old.len == sizeof words);

    END(fails);
    return fails != 0;
}
