/*
 * deque.h, instantiated for int as its proof instantiates it
 * (vmacros/vdrivers/deque_verify.h): a ring buffer pushed and popped at
 * both ends past its wrap point, filled, refused, swapped and cleared.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/deque_verify.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    int storage[4];
    int other_storage[2];
    deque_int d;
    deque_int other;
    int x = 0;

    deque_int_init(&d, storage, 4u);
    deque_int_init(&other, other_storage, 2u);
    CHECK(deque_int_is_empty(&d) && deque_int_capacity(&d) == 4u);
    CHECK(result__Bool_Error_is_ok(deque_int_push_back(&d, 2)));
    CHECK(result__Bool_Error_is_ok(deque_int_push_front(&d, 1)));   /* wraps */
    CHECK(deque_int_try_push_back(&d, 3) && deque_int_try_push_front(&d, 0));
    CHECK(deque_int_is_full(&d) && deque_int_remaining(&d) == 0u && deque_int_len(&d) == 4u);
    CHECK(!deque_int_try_push_back(&d, 9) && result__Bool_Error_is_err(deque_int_push_front(&d, 9)));
    CHECK(deque_int_peek_front(&d, &x) && x == 0);
    CHECK(deque_int_peek_back(&d, &x) && x == 3);
    CHECK(result__Bool_Error_is_ok(deque_int_pop_front(&d, &x)) && x == 0);
    CHECK(result__Bool_Error_is_ok(deque_int_pop_back(&d, &x)) && x == 3);
    deque_int_push_back_unchecked(&d, 4);
    deque_int_push_front_unchecked(&d, -1);                         /* -1 1 2 4 */
    CHECK(deque_int_peek_front(&d, &x) && x == -1);

    deque_int_swap(&d, &other);
    CHECK(deque_int_len(&other) == 4u && deque_int_is_empty(&d));
    deque_int_swap(&d, &other);
    deque_int_clear(&d);
    CHECK(deque_int_is_empty(&d) && result__Bool_Error_is_err(deque_int_pop_back(&d, &x)));
    CHECK(!deque_int_peek_front(&d, &x));
    deque_int none = deque_int_empty();
    CHECK(deque_int_len(&none) == 0u && !deque_int_try_push_back(&none, 1));

    END(fails);
    return fails != 0;
}
