/* tools/probes/verify-039/p_pop_frame.c
 *
 * Is vec_pop's frame postcondition true for every call its contract admits?
 *
 * vec_pop's `ok` behavior promises, as its fourth ensures,
 *     \forall integer k; 0 <= k < v->len ==> v->items[k] == \old(v->items[k]);
 * and its precondition asks only `out == \null || \valid(out)`: nothing keeps
 * `out` from pointing into the vector's own buffer. The body is
 *     *out = v->items[--v->len];
 * a direct element copy (no mem_copy or mem_move). If `out` points at a
 * remaining element, the copy overwrites it, and the frame is false for a
 * call the contract admits.
 *
 * The residual is typed_cast_vec_int_pop_ok_ensures_4_part5 (cc-vec, vec) and
 * its renamed copy in vec_struct; VERIFY-018 files it under category (d),
 * whose stated root cause is mem_copy/mem_move's frame-only contracts.
 *
 * Build and run (both configurations; sanitizers show the call is defined C):
 *   cc -std=c99 -Wall -Wextra -I. -Icore/primitives -Icore -Isemantics -Idata \
 *      -fsanitize=address,undefined tools/probes/verify-039/p_pop_frame.c -o /tmp/p && /tmp/p
 *   ... add -DCANON_NO_REQUIRE -DNDEBUG for the proof configuration.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "vmacros/vdrivers/vec_cc_experiment.h"

int main(void) {
    int buf[4];
    vec_int v = vec_int_init(buf, 4);
    int old[4];
    usize k;

    (void)vec_int_push(&v, 10);
    (void)vec_int_push(&v, 20);
    (void)vec_int_push(&v, 30);
    for (k = 0; k < v.len; k++) { old[k] = v.items[k]; }

    /* Admissible: v valid, v->items != NULL, v->len == 3 > 0 (behavior ok),
     * and out == &v.items[0] is \valid. No overlap between source items[2]
     * and destination items[0], so the C call is well defined. */
    result__Bool_Error r = vec_int_pop(&v, &v.items[0]);

    int ok_behavior = r.is_ok && v.len == 2;
    int frame_holds = 1;
    for (k = 0; k < v.len; k++) {
        if (v.items[k] != old[k]) { frame_holds = 0; }
    }
    printf("pop(&v, &v.items[0]) on [10, 20, 30]: is_ok=%d len=%zu items=[%d, %d]\n",
           (int)r.is_ok, (size_t)v.len, v.items[0], v.items[1]);
    printf("ensures 1-3 (ok, len-1, *out == old last): %s\n",
           (ok_behavior && v.items[0] == old[2]) ? "hold" : "VIOLATED");
    printf("ensures 4 (frame: items[k] unchanged for k < len): %s\n",
           frame_holds ? "holds" : "VIOLATED");
    return frame_holds ? 0 : 1;
}
