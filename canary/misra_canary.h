/* MISRA CI self-test file. Intentional violations.
 * If the pipeline reports zero findings from this file, the checker
 * is broken and the misra job fails. Do not fix these. Do not delete.
 * Not part of Canon-C — never include this header. */
#ifndef MISRA_CANARY_H
#define MISRA_CANARY_H
#include <stdio.h>
static int misra_canary_fn(int x) {
    int r = 0;
    if (x) goto misra_canary_out;
    r = x << 1;
misra_canary_out:
    return r;
}
#endif
