/*
 * hashmap_iteration.ya's rule, followed: an iteration left before its end,
 * the map then changed, a new iteration started from 0 and run to its end,
 * and the map changed after it. Aorai with Eva: follows.
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
    for (u64 k = 1u; k <= 4u; k++) {
        const int v = (int)k;
        CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &k, &v)));
    }

    usize iter = 0u;
    const u64 *key = NULL;
    const int *val = NULL;
    CHECK(hashmap_iter_next(&map, &iter, &key, &val));   /* left before its end */
    const u64 five = 5u;
    const int v5 = 5;
    CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &five, &v5)));

    iter = 0u;                                           /* a new iteration */
    int seen = 0;
    while (hashmap_iter_next(&map, &iter, &key, &val)) { seen++; }
    CHECK(seen == 5);
    CHECK(result_hashmap_val_t_Error_unwrap_or(hashmap_remove(&map, &five), 0) == 5);
    hashmap_clear(&map);
    CHECK(hashmap_is_empty(&map));

    END(fails);
    return fails != 0;
}
