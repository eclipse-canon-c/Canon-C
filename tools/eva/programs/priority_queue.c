/*
 * priority_queue.h: a min-queue of u32 over a buffer, with compare.h's
 * comparator as its proof requires: pushed until full, peeked, popped in
 * order, an element removed by index, a buffer heapified in place; and the
 * calls it refuses.
 */
#define CANON_CONTRACT_IMPL
#include "data/priority_queue.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    u32 storage[6];
    PriorityQueue pq;
    u32 x = 0u;
    static const u32 input[5] = { 50u, 10u, 40u, 20u, 30u };

    pq_init(&pq, storage, 6u, sizeof(u32), algo_cmp_u32, NULL);
    CHECK(pq_is_empty(&pq) && pq_capacity(&pq) == 6u && pq_remaining(&pq) == 6u);
    for (usize i = 0u; i < 5u; i++) {
        CHECK(pq_push(&pq, &input[i]));
    }
    CHECK(result__Bool_Error_is_ok(pq_push_result(&pq, &input[0])));
    CHECK(pq_is_full(&pq) && pq_len(&pq) == 6u);
    CHECK(!pq_push(&pq, &input[1]) && result__Bool_Error_is_err(pq_push_result(&pq, &input[1])));

    const u32 *top = pq_peek_raw(&pq);
    CHECK(top != NULL && *top == 10u);
    CHECK(pq_peek(&pq, &x) && x == 10u);
    CHECK(pq_pop(&pq, &x) && x == 10u);
    CHECK(pq_pop_raw(&pq, &x) && x == 20u);
    CHECK(pq_remove_at(&pq, 0u));                                  /* 30 */
    CHECK(result__Bool_Error_is_ok(pq_remove_at_result(&pq, 0u))); /* 40 */
    CHECK(pq_len(&pq) == 2u && pq_pop(&pq, &x) && x == 50u);
    CHECK(!pq_remove_at(&pq, 5u));
    CHECK(pq_as_bytes(&pq).len == pq_len(&pq) * sizeof(u32));
    CHECK(pq_as_cbytes(&pq).len == pq_len(&pq) * sizeof(u32));

    /* A buffer filled first, then made a heap. */
    u32 raw[4] = { 9u, 3u, 7u, 1u };
    PriorityQueue heap;
    pq_init(&heap, raw, 4u, sizeof(u32), algo_cmp_u32, NULL);
    pq_heapify(&heap, 4u);
    CHECK(pq_pop(&heap, &x) && x == 1u && pq_pop(&heap, &x) && x == 3u);

    END(fails);
    return fails != 0;
}
