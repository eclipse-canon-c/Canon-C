/* Test program for tools/idioms: uses only the verified vocabulary — the
 * verified vec<int> instantiation, arena, catalogued ACSL — so n(P) must be 0. */
#include "core/arena.h"
#include "vmacros/vdrivers/vec_verify.h"

/*@ requires \valid(a) && \valid(v);
    requires a->offset <= a->capacity;
    assigns *a, *v;
    ensures \result == \true || \result == \false;
*/
static bool fill_three(Arena* a, vec_int* v) {
    int i;
    /*@ loop invariant 0 <= i <= 3;
        loop assigns i, *v;
        loop variant 3 - i;
    */
    for (i = 0; i < 3; i++) {
        if (!result__Bool_Error_is_ok(vec_int_push(v, i))) { return false; }
    }
    return arena_used(a) <= arena_capacity(a);
}
