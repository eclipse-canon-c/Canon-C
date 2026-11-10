/*
 * stack.h, instantiated for int with vec, option and result as its proof
 * instantiates them (vmacros/vdrivers/stack_verify.h): a stack over a
 * buffer, pushed until full, peeked, popped until empty, refused, cleared.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/stack_verify.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    int storage[3];
    stack_int s;
    int x = 0;

    stack_int_init(&s, storage, 3u);
    CHECK(stack_int_is_empty(&s) && stack_int_capacity(&s) == 3u);
    CHECK(result__Bool_Error_is_ok(stack_int_push(&s, 1)));
    CHECK(stack_int_try_push(&s, 2));
    stack_int_push_unchecked(&s, 3);
    CHECK(stack_int_is_full(&s) && stack_int_len(&s) == 3u && stack_int_remaining(&s) == 0u);
    CHECK(!stack_int_try_push(&s, 4) && result__Bool_Error_is_err(stack_int_push(&s, 4)));
    CHECK(stack_int_peek(&s, &x) && x == 3);
    CHECK(option_int_unwrap_or(stack_int_peek_option(&s), 0) == 3);
    CHECK(result__Bool_Error_is_ok(stack_int_pop(&s, &x)) && x == 3);
    CHECK(option_int_unwrap_or(stack_int_pop_option(&s), 0) == 2);
    stack_int_clear(&s);
    CHECK(stack_int_is_empty(&s) && !stack_int_peek(&s, &x));
    CHECK(option_int_is_none(stack_int_pop_option(&s)));
    CHECK(result__Bool_Error_is_err(stack_int_pop(&s, &x)));

    END(fails);
    return fails != 0;
}
