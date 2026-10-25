/* VERIFY-041 probe of the committed vec_range.h (findings F1, F2).
 * From the repository root, in both builds:
 *   gcc -std=c99 -fsanitize=address,undefined [-DCANON_NO_REQUIRE] \
 *       -I core/primitives -I core -I semantics -I data -I . \
 *       tools/probes/verify-041/probe_committed.c -o p && for a in w s m r; do ./p $a; done
 * w: values beyond int (F1); s: step 0 built by hand; m: step ISIZE_MIN built
 * by hand (F2); r: step ISIZE_MIN through range_make (F2; with
 * -DCANON_NO_REQUIRE the guard is compiled out, without it it aborts). */
#define CANON_CONTRACT_IMPL
#include <stdio.h>

#include "semantics/option/option.h"
#include "data/vec/vec.h"
#include "data/vec/vec_range.h"
CANON_OPTION(int)
DEFINE_VEC(static inline, int)
DEFINE_VEC_RANGE(static inline, int)
int main(int argc, char **argv) {
    int buf[16]; vec_int v = vec_int_init(buf, 16);
    if (argc > 1 && argv[1][0] == 's') {          /* step 0, built by hand */
        range r = { 0, 10, 0 };
        result__Bool_Error res = vec_int_extend_from_range(&v, r);
        printf("step0: ok=%d len=%zu\n", (int)res.is_ok, (size_t)v.len);
    } else if (argc > 1 && argv[1][0] == 'm') {   /* ISIZE_MIN step by hand */
        range r = { 0, -10, PTRDIFF_MIN };
        result__Bool_Error res = vec_int_extend_from_range(&v, r);
        printf("stepmin: ok=%d len=%zu\n", (int)res.is_ok, (size_t)v.len);
    } else if (argc > 1 && argv[1][0] == 'r') {   /* ISIZE_MIN through range_make */
        range r = range_make(0, -10, PTRDIFF_MIN);
        result__Bool_Error res = vec_int_extend_from_range(&v, r);
        printf("make_min: step=%lld ok=%d len=%zu\n", (long long)r.step, (int)res.is_ok, (size_t)v.len);
    } else {                                      /* values beyond int */
        range r = range_make((isize)2147483647 - 1, (isize)2147483647 + 2, 1);
        result__Bool_Error res = vec_int_extend_from_range(&v, r);
        printf("wide: ok=%d len=%zu:", (int)res.is_ok, (size_t)v.len);
        for (usize i = 0; i < v.len; i++) printf(" %d", buf[i]);
        printf("\n");
    }
    return 0;
}
