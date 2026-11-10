/*
 * arena.h: a frame of scratch allocations in a static buffer: plain,
 * aligned and zeroed allocations, the try_ forms, a request that does not
 * fit, a mark and a reset to it, the queries and the byte views, then the
 * resets.
 */
#define CANON_CONTRACT_IMPL
#include "core/arena.h"
#include "eva_program.h"

static u64 backing[32];   /* 256 bytes */

int main(void)
{
    int fails = 0;
    Arena a;
    arena_init(&a, backing, sizeof backing);
    CHECK(arena_capacity(&a) == 256u && arena_is_empty(&a) && !arena_is_full(&a));

    u8 *bytes = arena_alloc(&a, 10u);
    CHECK(bytes != NULL);
    if (bytes != NULL) { bytes[9] = 1u; }
    u64 *words = arena_alloc_aligned(&a, 4u * sizeof(u64), sizeof(u64));
    CHECK(words != NULL);
    if (words != NULL) { words[3] = 2u; }
    u32 *zeroed = arena_alloc_zero(&a, 4u * sizeof(u32));
    CHECK(zeroed != NULL && zeroed[3] == 0u);
    u16 *zal = arena_alloc_aligned_zero(&a, 2u * sizeof(u16), 2u);
    CHECK(zal != NULL && zal[1] == 0u);

    void *p = NULL;
    CHECK(arena_try_alloc(&a, 8u, &p) && p != NULL);
    CHECK(arena_try_alloc_aligned(&a, 8u, 8u, &p) && p != NULL);
    CHECK(arena_alloc(&a, 1000u) == NULL && !arena_try_alloc(&a, 1000u, &p));
    CHECK(arena_used(&a) + arena_remaining(&a) == arena_capacity(&a));

    ArenaMark mark = arena_mark(&a);
    CHECK(arena_alloc(&a, 16u) != NULL);
    arena_reset_to(&a, mark);
    CHECK(arena_mark(&a) == mark);

    CHECK(arena_as_bytes(&a).len == arena_used(&a));
    CHECK(arena_as_cbytes(&a).len == arena_used(&a));
    CHECK(arena_buffer_bytes(&a).len == 256u && arena_buffer_cbytes(&a).len == 256u);
    CHECK(arena_free_bytes(&a).len == arena_remaining(&a));
    CHECK(arena_free_cbytes(&a).len == arena_remaining(&a));

    arena_reset(&a);
    CHECK(arena_is_empty(&a) && arena_used(&a) == 0u);
    CHECK(arena_alloc(&a, 256u) != NULL && arena_is_full(&a));
    arena_reset_secure(&a);
    CHECK(arena_is_empty(&a));

    END(fails);
    return fails != 0;
}
