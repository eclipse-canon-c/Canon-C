/*
 * bitset_views.ya's rule, broken: a view read after its bitset was closed.
 * Aorai with Eva: breaks, at the call marked "refused". Built with
 * CANON_LIFETIME_DEBUG, the read fails its check as it runs.
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
    bitset_close(&bs);
    cbytes_t gone = borrowed_bytes_get(&view);   /* refused */
    CHECK(gone.len == sizeof words);

    END(fails);
    return fails != 0;
}
