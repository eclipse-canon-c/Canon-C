/*
 * VERIFY-085 run 2: hashmap_stale_checks.c with its buffer declared as the
 * bytes bytes_from and hashmap_init read it as, u8[512] aligned for the
 * slots, in place of u64[64]. WP's typed model counts an array in cells of
 * its type: u64[64] is 64 cells, and bytes_from's precondition asks for 512
 * of them. If that is why WP proved the checks of \false in run 1b, it
 * proves none of them here. For the probe's part main only.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/hashmap_verify.h"
#include "eva_program.h"

static u8 backing[512] __attribute__((aligned(8)));   /* aligned for the slots */

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
