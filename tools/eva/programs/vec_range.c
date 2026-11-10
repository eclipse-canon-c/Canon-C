/*
 * vec_range.h, instantiated for int as its proof instantiates it
 * (vmacros/vdrivers/vec_range_verify.h, which defines the vector type but
 * not its functions, so the vector is made from its fields): a vector
 * extended from ranges up, down and by steps, and a range it has no room
 * for.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/vec_range_verify.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    int storage[10];
    vec_int v = { storage, 0u, 10u };

    CHECK(result__Bool_Error_is_ok(vec_int_extend_from_range(&v, range_upto(4))));   /* 0 1 2 3 */
    CHECK(result__Bool_Error_is_ok(vec_int_extend_from_range(&v, range_downto(3, 0))));  /* 3 2 1 */
    CHECK(result__Bool_Error_is_ok(vec_int_extend_from_range(&v, range_make(10, 16, 3))));  /* 10 13 */
    CHECK(v.len == 9u && storage[0] == 0 && storage[4] == 3 && storage[8] == 13);
    CHECK(result__Bool_Error_is_err(vec_int_extend_from_range(&v, range_upto(2))));
    CHECK(v.len == 9u);
    CHECK(result__Bool_Error_is_ok(vec_int_extend_from_range(&v, range_from_to(5, 5))));
    CHECK(result__Bool_Error_is_err(vec_int_extend_from_range(NULL, range_upto(1))));

    END(fails);
    return fails != 0;
}
