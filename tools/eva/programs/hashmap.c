/*
 * hashmap.h, instantiated with u64 keys and int values as its proof
 * instantiates it (vmacros/vdrivers/hashmap_verify.h): a map in a buffer
 * of 8 slots, filled, read, updated, iterated, emptied by removal, refused
 * when full, cleared.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/hashmap_verify.h"
#include "eva_program.h"

static u64 backing[64];   /* 512 bytes, aligned for the slots */

int main(void)
{
    int fails = 0;
    hashmap map;
    const usize need = hashmap_buffer_size(8u);
    CHECK(need > 0u && need <= sizeof backing);
    if (!result__Bool_Error_is_ok(hashmap_init(&map, bytes_from(backing, sizeof backing), 8u, NULL))) {
        return 1;   /* not here: the buffer is aligned for the slots */
    }
    CHECK(hashmap_is_empty(&map) && hashmap_capacity(&map) == 8u);

    for (u64 k = 1u; k <= 5u; k++) {
        const int v = (int)(k * 10u);
        CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &k, &v)));
    }
    CHECK(hashmap_len(&map) == 5u && hashmap_load_factor(&map) > 0.5);
    const u64 three = 3u;
    const u64 absent = 99u;
    CHECK(option_hashmap_val_t_unwrap_or(hashmap_get(&map, &three), 0) == 30);
    CHECK(option_hashmap_val_t_is_none(hashmap_get(&map, &absent)));
    CHECK(hashmap_contains_key(&map, &three) && !hashmap_contains_key(&map, &absent));
    int *slot = hashmap_get_or_null(&map, &three);
    CHECK(slot != NULL && hashmap_get_or_null(&map, &absent) == NULL);
    if (slot != NULL) { *slot = 33; }
    const int updated = 300;
    CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &three, &updated)));   /* replaces */
    CHECK(hashmap_len(&map) == 5u);

    usize iter = 0u;
    const u64 *key = NULL;
    const int *val = NULL;
    int seen = 0;
    while (hashmap_iter_next(&map, &iter, &key, &val)) { seen++; }
    CHECK(seen == 5);

    CHECK(result_hashmap_val_t_Error_unwrap_or(hashmap_remove(&map, &three), 0) == 300);
    CHECK(result_hashmap_val_t_Error_is_err(hashmap_remove(&map, &three)));
    for (u64 k = 10u; k < 12u; k++) {
        const int v = 1;
        CHECK(result__Bool_Error_is_ok(hashmap_insert(&map, &k, &v)));
    }
    /* Six entries in eight slots is the map's load limit: it refuses a
       seventh key, and a new value for a key it holds too, since it checks
       the limit before it looks for the key. */
    const u64 extra = 77u;
    const int v = 7;
    CHECK(hashmap_len(&map) == 6u && result__Bool_Error_is_err(hashmap_insert(&map, &extra, &v)));
    const u64 one = 1u;
    CHECK(result__Bool_Error_is_err(hashmap_insert(&map, &one, &v)));
    hashmap_clear(&map);
    CHECK(hashmap_is_empty(&map));

    END(fails);
    return fails != 0;
}
