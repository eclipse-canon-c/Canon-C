/*
 * lifetime.h (CANON_LIFETIME_DEBUG): the ids two owners receive, which
 * must not be REGION_ID_STATIC, kept in lifetime_t as the modules that
 * embed it keep them.
 */
#define CANON_CONTRACT_IMPL
#include "core/primitives/lifetime.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    static int owner_a;
    static int owner_b;
    lifetime_t a = { canon_lifetime_next_id_(&owner_a), true };
    lifetime_t b = { canon_lifetime_next_id_(&owner_b), true };
    CHECK(a.id != REGION_ID_STATIC && b.id != REGION_ID_STATIC);
    b.open = false;
    CHECK(a.open && !b.open);

    END(fails);
    return fails != 0;
}
