/* tools/probes/verify-039/p_remove_frame.c
 *
 * vec_remove's `ok` behavior promises
 *   ensures 4: \forall integer k; 0 <= k < i ==> v->items[k] == \old(v->items[k]);
 *   ensures 5: \forall integer k; i <= k < v->len ==> v->items[k] == \old(v->items[k + 1]);
 * with only `out == \null || \valid(out)` required of `out`. The body copies
 * `*out = v->items[i]` before shifting, so an `out` inside the buffer is
 * overwritten (before i) or shifted (after i). Both cases are defined C.
 *
 * Residuals: typed_cast_vec_int_remove_ok_ensures_4_part6, _4_part7 and
 * _5_part6, filed under VERIFY-018 category (d) (mem_copy/mem_move frame-only).
 *
 *   cc -std=c99 -Wall -Wextra -I. -Icore/primitives -Icore -Isemantics -Idata \
 *      -fsanitize=address,undefined tools/probes/verify-039/p_remove_frame.c -o /tmp/r && /tmp/r
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "vmacros/vdrivers/vec_cc_experiment.h"

static int check(const char* label, usize i, usize j) {
    int buf[4];
    vec_int v = vec_int_init(buf, 4);
    int old[4];
    usize k;
    int e4 = 1, e5 = 1;
    (void)vec_int_push(&v, 10); (void)vec_int_push(&v, 20);
    (void)vec_int_push(&v, 30); (void)vec_int_push(&v, 40);
    for (k = 0; k < 4; k++) { old[k] = v.items[k]; }
    result__Bool_Error r = vec_int_remove(&v, i, &v.items[j]);   /* out inside the buffer */
    for (k = 0; k < i; k++)     { if (v.items[k] != old[k])     { e4 = 0; } }
    for (k = i; k < v.len; k++) { if (v.items[k] != old[k + 1]) { e5 = 0; } }
    printf("%-34s is_ok=%d items=[%d, %d, %d]  ensures 4: %-8s ensures 5: %s\n", label,
           (int)r.is_ok, v.items[0], v.items[1], v.items[2],
           e4 ? "holds" : "VIOLATED", e5 ? "holds" : "VIOLATED");
    return e4 && e5;
}

int main(void) {
    int a = check("remove(v, 2, &items[0]) [10..40]:", 2, 0);
    int b = check("remove(v, 1, &items[3]) [10..40]:", 1, 3);
    return (a && b) ? 0 : 1;
}
