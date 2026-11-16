/*
 * hashmap_iteration.ya's rule, broken: an iteration goes on after the map
 * changed, which hashmap.h's "Do not mutate the map during iteration"
 * forbids. Aorai with Eva: breaks, at the call marked "refused".
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
    for (u64 k = 1u; k <= 3u; k++) {
        const int v = (int)k;
        CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &k, &v)));
    }

    usize iter = 0u;
    const u64 *key = NULL;
    const int *val = NULL;
    CHECK(hashmap_iter_next(&map, &iter, &key, &val));
    const u64 four = 4u;
    const int v4 = 4;
    CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &four, &v4)));
    (void)hashmap_iter_next(&map, &iter, &key, &val);   /* refused */

    END(fails);
    return fails != 0;
}
