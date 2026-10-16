/*
 * VERIFY-039 G13 probe, the calls: a call that meets every precondition of
 * a function that calls memcmp, and makes memcmp's danglingness
 * precondition false.
 *
 * Frama-C's libc gives memcmp
 *     requires danglingness:s1: non_escaping(s1, n);
 *     requires danglingness:s2: non_escaping(s2, n);
 * with non_escaping(s, n) = \forall integer i; 0 <= i < n ==>
 * !\dangling((char *)s + i). ACSL's \dangling(p) is about the content of *p:
 * "its value is (or contains bits of) a dangling address" (the ACSL
 * reference manual, "Dangling pointers"). The bytes of a pointer object
 * whose block has been freed are such bytes. Until VERIFY-039 G13's repair
 * no contract of the eight functions called below that call memcmp, nor of
 * borrowed_str_eq, which calls str_equal, required the bytes it compares to
 * hold no dangling address; each required them readable and, on the
 * comparing path, initialized, and the bytes of a pointer object are both.
 * Since the repair each requires it, as the quantifier written out
 * (no_dangling_a and no_dangling_b, or no_dangling_s and the other view's).
 *
 * Each entry point frees a block, keeps its address in two pointer
 * objects, p and q, and compares their bytes. Run with Eva, one entry point
 * at a time (-main), as .github/workflows/g13-probe.yml does. On the
 * contracts before the repair the probe predicted, and run #1 found, for
 * each, that memcmp's danglingness:s1 is invalid, that every other
 * precondition Eva evaluates is valid, danglingness:s2 aside, and that the
 * analysis counts no alarm. On the repaired contracts it predicts the same
 * of the called function's own first such clause, at the call below: Eva
 * rejects the call at the function's boundary. Eva stops the path at the
 * invalid precondition, so it does not reach the second clause; reached,
 * that would be invalid too.
 *
 * From the repository root, for one entry point:
 *     frama-c -eva -main via_mem_equal \
 *       -cpp-extra-args="-I core/primitives -I core -I semantics -I . \
 *                        -DCANON_NO_REQUIRE -DNDEBUG" \
 *       tools/probes/verify-039/p_dangling_calls.c
 *
 * Not a run-time test: the memcmp call is itself the step in question, and
 * no sanitizer flags it.
 */
#include <stdbool.h>
#include <stdlib.h>
#include "core/memory.h"
#include "core/slice.h"
#include "semantics/borrow.h"

int via_mem_compare(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);                       /* p and q now hold a freed address */
    return mem_compare(&p, &q, sizeof p);
}

int via_mem_equal(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)mem_equal(&p, &q, sizeof p);
}

int via_mem_equal_bytes(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)mem_equal_bytes(cbytes_from(&p, sizeof p),
                                cbytes_from(&q, sizeof q));
}

int via_bytes_equal(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)bytes_equal(bytes_from(&p, sizeof p),
                            bytes_from(&q, sizeof q));
}

int via_str_equal(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)str_equal(str_from((const char *)&p, sizeof p),
                          str_from((const char *)&q, sizeof q));
}

int via_str_starts_with(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)str_starts_with(str_from((const char *)&p, sizeof p),
                                str_from((const char *)&q, sizeof q));
}

int via_str_ends_with(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)str_ends_with(str_from((const char *)&p, sizeof p),
                              str_from((const char *)&q, sizeof q));
}

int via_borrowed_bytes_eq(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)borrowed_bytes_eq(borrowed_bytes_from(&p, sizeof p, "p"),
                                  borrowed_bytes_from(&q, sizeof q, "q"));
}

int via_borrowed_str_eq(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);
    return (int)borrowed_str_eq(
        borrowed_str_from(str_from((const char *)&p, sizeof p), "p"),
        borrowed_str_from(str_from((const char *)&q, sizeof q), "q"));
}
