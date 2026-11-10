/*
 * vec.h, instantiated for int as its proof instantiates it
 * (vmacros/vdrivers/vec_verify.h): a vector over a stack buffer filled,
 * read, edited, iterated and sliced; one on the heap and one in an arena;
 * and the calls the vector refuses (full, out of range, empty).
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/vec_verify.h"
#include "eva_program.h"

static u64 backing[16];

int main(void)
{
    int fails = 0;
    int storage[8];
    vec_int v = vec_int_init(storage, 8u);
    int x = 0;

    CHECK(vec_int_is_empty(&v) && vec_int_capacity(&v) == 8u);
    CHECK(result__Bool_Error_is_ok(vec_int_push(&v, 1)));
    CHECK(vec_int_try_push(&v, 2));
    vec_int_push_unchecked(&v, 3);
    CHECK(vec_int_len(&v) == 3u && vec_int_remaining(&v) == 5u);
    CHECK(vec_int_get(&v, 1u, &x) && x == 2 && !vec_int_get(&v, 3u, &x));
    CHECK(option_int_unwrap_or(vec_int_get_option(&v, 2u), 0) == 3);
    CHECK(option_int_is_none(vec_int_get_option(&v, 7u)));
    CHECK(vec_int_get_unchecked(&v, 0u) == 1);
    int *second = vec_int_at(&v, 1u);
    CHECK(second != NULL && *second == 2);
    CHECK(vec_int_set(&v, 1u, 20) && !vec_int_set(&v, 9u, 0));
    CHECK(*vec_int_first(&v) == 1 && *vec_int_last(&v) == 3 && vec_int_data(&v) == storage);

    CHECK(result__Bool_Error_is_ok(vec_int_insert(&v, 0u, 0)));      /* 0 1 20 3 */
    CHECK(result__Bool_Error_is_ok(vec_int_remove(&v, 2u, &x)) && x == 20);
    CHECK(option_int_unwrap_or(vec_int_remove_option(&v, 0u), -1) == 0);  /* 1 3 */
    static const int more[3] = { 4, 5, 6 };
    CHECK(result__Bool_Error_is_ok(vec_int_append_array(&v, more, 3u)));    /* 1 3 4 5 6 */
    CHECK(result__Bool_Error_is_ok(vec_int_extend(&v, more, 2u)));          /* 1 3 4 5 6 4 5 */
    CHECK(vec_int_len(&v) == 7u);
    CHECK(result__Bool_Error_is_err(vec_int_append_array(&v, more, 3u)));   /* too big */
    CHECK(result__Bool_Error_is_ok(vec_int_pop(&v, &x)) && x == 5);
    CHECK(option_int_unwrap_or(vec_int_pop_option(&v), -1) == 4);

    int sum = 0;
    vec_int_iter it = vec_int_iter_init(&v);
    while (vec_int_iter_next(&it, &x)) { sum += x; }
    CHECK(sum == 19);
    vec_int_slice s = vec_int_slice_init(&v, 1u, 3u);
    int *in_slice = vec_int_slice_get(&s, 1u);
    CHECK(s.len == 2u && in_slice != NULL && *in_slice == 4);

    vec_int_fill(&v, 9, 3u);
    CHECK(vec_int_len(&v) == 8u && *vec_int_last(&v) == 9 && vec_int_is_full(&v));
    CHECK(!vec_int_try_push(&v, 10) && result__Bool_Error_is_err(vec_int_push(&v, 10)));

    vec_int heap = vec_int_alloc(4u);
    if (vec_int_data(&heap) != NULL) {
        CHECK(vec_int_try_push(&heap, 42));
        vec_int_swap(&v, &heap);
        CHECK(vec_int_len(&v) == 1u && vec_int_len(&heap) == 8u);
        vec_int_swap(&v, &heap);
        vec_int_free(&heap);
    }
    Arena a;
    arena_init(&a, backing, sizeof backing);
    vec_int in_arena = vec_int_arena_alloc(&a, 4u);
    CHECK(vec_int_capacity(&in_arena) == 4u && vec_int_try_push(&in_arena, 1));
    vec_int none = vec_int_empty();
    CHECK(vec_int_len(&none) == 0u && !vec_int_try_push(&none, 1));

    vec_int_clear(&v);
    CHECK(vec_int_is_empty(&v) && option_int_is_none(vec_int_pop_option(&v)));

    END(fails);
    return fails != 0;
}
