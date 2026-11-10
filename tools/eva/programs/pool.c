/*
 * pool.h: fixed-size objects carved from an arena: allocation until the
 * pool is full, indexed access, the queries and byte views, the resets.
 */
#define CANON_CONTRACT_IMPL
#include "core/pool.h"
#include "eva_program.h"

typedef struct { u32 id; u32 value; } Item;

static u64 backing[16];   /* 128 bytes */

int main(void)
{
    int fails = 0;
    Arena a;
    Pool p;
    arena_init(&a, backing, sizeof backing);
    CHECK(pool_init(&p, &a, sizeof(Item), 4u));
    CHECK(pool_capacity(&p) == 4u && pool_is_empty(&p) && pool_object_size(&p) == mem_align(sizeof(Item)));

    Item *first = pool_alloc(&p);
    CHECK(first != NULL);
    if (first != NULL) { first->id = 1u; first->value = 10u; }
    Item *zeroed = pool_alloc_zero(&p);
    CHECK(zeroed != NULL && zeroed->value == 0u);
    void *out = NULL;
    CHECK(pool_try_alloc(&p, &out) && out != NULL);
    CHECK(pool_try_alloc_zero(&p, &out) && out != NULL);
    CHECK(pool_is_full(&p) && pool_remaining(&p) == 0u && pool_used(&p) == 4u);
    CHECK(pool_alloc(&p) == NULL && !pool_try_alloc(&p, &out));

    Item *again = pool_get(&p, 0u);
    const Item *cagain = pool_get_const(&p, 0u);
    CHECK(again == first && cagain == first && pool_get(&p, 4u) == NULL);
    CHECK(pool_memory_used(&p) == 4u * pool_object_size(&p));
    CHECK(pool_memory_reserved(&p) == pool_memory_used(&p));
    CHECK(pool_as_bytes(&p).len == pool_memory_used(&p));
    CHECK(pool_as_cbytes(&p).len == pool_memory_used(&p));
    CHECK(pool_reserved_bytes(&p).len == pool_memory_reserved(&p));
    CHECK(pool_reserved_cbytes(&p).len == pool_memory_reserved(&p));

    pool_reset(&p);
    CHECK(pool_is_empty(&p) && pool_alloc(&p) != NULL);
    pool_reset_secure(&p);
    CHECK(pool_is_empty(&p));

    END(fails);
    return fails != 0;
}
