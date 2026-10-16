/*
 * VERIFY-039 G13 probe, the forms: how WP translates memcmp's danglingness
 * precondition, and the two ways a contract can state it.
 *
 * WP does not implement \dangling ("Allocation, initialization and
 * danglingness not yet implemented"). It replaces such an atom by false
 * where it stands as a goal and by true where it stands as a hypothesis
 * (LogicSemantics.ml, pred_protected), and it compiles the body of a logic
 * predicate once, as a goal (LogicCompiler.ml, compile_lbpred). In libc's
 *     non_escaping(s, n) = \forall integer i; 0 <= i < n ==>
 *                          !\dangling((char *)s + i)
 * the atom sits under a negation, so it becomes true and the body
 * \forall integer i; 0 <= i < n ==> \false: WP's non_escaping(s, n) is
 * n <= 0, whatever s. The probe runs WP on seven functions, as the
 * Typed+Cast units run it, and predicts:
 *
 *   today         a memcmp caller with today's kind of contract: its two
 *                 danglingness goals are not proved;
 *   libc_pred     non_escaping required, with n > 0: ensures \false is
 *                 PROVED, the hypothesis being false in WP's translation;
 *   quantified    the same requirement stated as the quantifier itself:
 *                 ensures \false is not proved, the hypothesis being
 *                 ignored;
 *   recommended   a memcmp caller requiring the quantifier for each
 *                 operand: its two danglingness goals are not proved;
 *   caller        a caller of recommended requiring the same: its goals at
 *                 the call for recommended's two requirements are not
 *                 proved;
 *   other         non_escaping of one pointer required, with no bound on n:
 *                 non_escaping of another ensured, and PROVED, the pointer
 *                 being ignored;
 *   zero          n == 0 required: non_escaping ensured, and proved, so the
 *                 translation is n <= 0, not false.
 *
 * Every other goal of the seven is measured, not predicted. WP runs on the
 * seven only (-wp-fct); Eva runs from the two entry points at the end, which
 * call recommended, and predicts:
 *
 *   eva_freed     the bytes of two pointers to a freed block: recommended's
 *                 da invalid at the call, memcmp not reached, db aside;
 *   eva_live      the bytes of two ints: every precondition valid.
 *
 * From the repository root:
 *     frama-c -wp -wp-rte -wp-model Typed+Cast -wp-prover alt-ergo,z3,cvc5 \
 *       -wp-timeout 120 -wp-split \
 *       -wp-fct today,libc_pred,quantified,recommended,caller,other,zero \
 *       -cpp-extra-args="-I core/primitives -I core -I semantics -I . \
 *                        -DCANON_NO_REQUIRE -DNDEBUG" \
 *       tools/probes/verify-039/p_dangling_forms.c
 *     frama-c -eva -main eva_freed -cpp-extra-args="..." \
 *       tools/probes/verify-039/p_dangling_forms.c
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*@ requires va: \valid_read((char *)a + (0 .. n - 1));
    requires vb: \valid_read((char *)b + (0 .. n - 1));
    requires ia: \initialized((char *)a + (0 .. n - 1));
    requires ib: \initialized((char *)b + (0 .. n - 1));
    assigns \nothing;
*/
int today(const void *a, const void *b, size_t n)
{
    return memcmp(a, b, n);
}

/*@ requires pos: n > 0;
    requires da: non_escaping((void *)a, n);
    assigns \nothing;
    ensures absurd: \false;
*/
int libc_pred(const void *a, size_t n)
{
    (void)a; (void)n;
    return 0;
}

/*@ requires pos: n > 0;
    requires da: \forall integer i; 0 <= i < n ==> !\dangling((char *)a + i);
    assigns \nothing;
    ensures absurd: \false;
*/
int quantified(const void *a, size_t n)
{
    (void)a; (void)n;
    return 0;
}

/*@ requires va: \valid_read((char *)a + (0 .. n - 1));
    requires vb: \valid_read((char *)b + (0 .. n - 1));
    requires ia: \initialized((char *)a + (0 .. n - 1));
    requires ib: \initialized((char *)b + (0 .. n - 1));
    requires da: \forall integer i; 0 <= i < n ==> !\dangling((char *)a + i);
    requires db: \forall integer i; 0 <= i < n ==> !\dangling((char *)b + i);
    assigns \nothing;
*/
int recommended(const void *a, const void *b, size_t n)
{
    return memcmp(a, b, n);
}

/*@ requires va: \valid_read((char *)a + (0 .. n - 1));
    requires vb: \valid_read((char *)b + (0 .. n - 1));
    requires ia: \initialized((char *)a + (0 .. n - 1));
    requires ib: \initialized((char *)b + (0 .. n - 1));
    requires da: \forall integer i; 0 <= i < n ==> !\dangling((char *)a + i);
    requires db: \forall integer i; 0 <= i < n ==> !\dangling((char *)b + i);
    assigns \nothing;
*/
int caller(const void *a, const void *b, size_t n)
{
    return recommended(a, b, n);
}

/*@ requires da: non_escaping((void *)a, n);
    assigns \nothing;
    ensures other_pointer: non_escaping((void *)b, n);
*/
int other(const void *a, const void *b, size_t n)
{
    (void)a; (void)b; (void)n;
    return 0;
}

/*@ requires empty: n == 0;
    assigns \nothing;
    ensures holds: non_escaping((void *)a, n);
*/
int zero(const void *a, size_t n)
{
    (void)a; (void)n;
    return 0;
}

int eva_freed(void)
{
    int *p = malloc(sizeof *p);
    int *q = p;
    if (p == NULL) { return 0; }
    free(p);                       /* p and q now hold a freed address */
    return recommended(&p, &q, sizeof p);
}

int eva_live(void)
{
    int x = 1;
    int y = 1;
    return recommended(&x, &y, sizeof x);
}
