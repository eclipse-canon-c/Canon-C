/*
 * VERIFY-085 run 1b: tools/aorai/programs/hashmap_stale.c with ACSL checks,
 * which WP proves or not but never assumes, for the probe's part main. In
 * run 1, WP proved the precondition of the call marked refused, which Eva
 * finds invalid. A check of \false that WP proves marks a point WP reaches
 * with hypotheses that contradict each other; iter_zero, false at the call,
 * proved, would mean WP takes iter to be 0 there.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/hashmap_verify.h"
#include "eva_program.h"

static u64 backing[64];   /* 512 bytes, aligned for the slots */

int main(void)
{
    int fails = 0;
    hashmap map;
    if (!result__Bool_Error_is_ok(hashmap_init(&map, bytes_from(backing, sizeof backing), 8u, NULL))) {
        return 1;
    }
    /*@ check after_init: \false; */
    for (u64 k = 1u; k <= 3u; k++) {
        const int v = (int)k;
        CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &k, &v)));
    }
    /*@ check after_loop: \false; */

    usize iter = 0u;
    const u64 *key = NULL;
    const int *val = NULL;
    CHECK(hashmap_iter_next(&map, &iter, &key, &val));
    /*@ check after_first_next: \false; */
    const u64 four = 4u;
    const int v4 = 4;
    CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &four, &v4)));
    /*@ check after_insert: \false; */
    /*@ check iter_zero: iter == 0; */
    (void)hashmap_iter_next(&map, &iter, &key, &val);   /* refused */

    END(fails);
    return fails != 0;
}
