/*
 * region.h: a region that owns an arena and two cleanup hooks, nested in
 * an outer region; its queries and assertions, and its end, which runs the
 * hooks last-registered first and resets the arena. Also lifetime.h's
 * assertion, which region.h uses for borrows.
 */
#define CANON_CONTRACT_IMPL
#include "core/region.h"
#include "eva_program.h"

static int order[2];
static int ran;

static void first_hook(void *ctx)  { (void)ctx; order[ran++ % 2] = 1; }
static void second_hook(void *ctx) { (void)ctx; order[ran++ % 2] = 2; }

static u64 backing[8];

int main(void)
{
    int fails = 0;
    Region outer;
    Region r;
    Arena a;
    arena_init(&a, backing, sizeof backing);

    region_begin(&outer);
    region_begin(&r);
    region_set_parent(&r, &outer);
    region_attach_arena(&r, &a);
    CHECK(region_register(&r, first_hook, NULL));
    CHECK(region_register(&r, second_hook, NULL));
    CHECK(region_is_open(&r) && region_has_parent(&r) && !region_has_parent(&outer));
    CHECK(region_hook_count(&r) == 2u && region_id(&r) != region_id(&outer));
    CHECK(region_id(NULL) == 0u && !region_is_open(NULL) && region_hook_count(NULL) == 0u);
    region_assert_open(&r);
    region_assert_borrow_valid(&r, region_id(&r));
    lifetime_assert_valid(region_id(&r), region_is_open(&r), region_id(&r), "region.c");

    CHECK(arena_alloc(&a, 16u) != NULL && !arena_is_empty(&a));
    region_end(&r);
    CHECK(!region_is_open(&r) && ran == 2 && order[0] == 2 && order[1] == 1);
    CHECK(arena_is_empty(&a));
    region_end(&outer);
    CHECK(!region_is_open(&outer));

    END(fails);
    return fails != 0;
}
