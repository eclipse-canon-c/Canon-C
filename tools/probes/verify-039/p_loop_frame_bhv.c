/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

/* VERIFY-039 G19 -- docs/deviations.md: a loop's frame stated for one
 * behaviour.
 *
 * G19's decision has region_end() claim its loop's frame only in the
 * behaviour in which no hook is called, as ACSL writes it:
 * `for no_hook: loop assigns ...`. ACSL gives such a clause for the
 * executions in which that behaviour's assumption holds on entry, and for no
 * other. f() below states its loop's frame, `i` alone, for behaviour zero,
 * where the loop writes nothing else, and for no other: where x != 0 the
 * loop writes *p. Its postcondition, that *p is unchanged, is therefore
 * false for every call with x != 0, as main() shows. g() is the same
 * function with the frame stated for behaviour nonzero, the one in which the
 * loop writes *p: there the clause itself is false.
 *
 * WP 25, run locally as the Typed+Cast units run their headers, proves every
 * goal of f(), the postcondition among them: it checks the frame only under
 * zero's assumption, where the loop's write is never reached, and it takes
 * the frame as the loop's effect on every path. Of g()'s goals it proves all
 * but the frame's at the write, checked under nonzero's assumption, and
 * proves g()'s postcondition from the same frame. The probe runs WP 29 on
 * both (g19-probe.yml, report F), predicted before it ran (VERIFY-039, "G19
 * — committed prediction for the probe").
 *
 * main() calls each with x == 0 and with x != 0 and prints whether the
 * postcondition held; it exits 0 when it held for x == 0 and not for x != 0,
 * in both functions, as the reading above says, and 1 otherwise.
 */
#include <stdio.h>

/*@
  requires \valid(p);
  ensures  *p == \old(*p);
  behavior zero:
    assumes x == 0;
  behavior nonzero:
    assumes x != 0;
  complete behaviors;
  disjoint behaviors;
*/
void f(int x, int *p) {
    int i;
    /*@
      loop invariant 0 <= i <= 3;
      for zero: loop assigns i;
      loop variant 3 - i;
    */
    for (i = 0; i < 3; i++) {
        if (x != 0) {
            *p = 5;
        }
    }
}

/*@
  requires \valid(p);
  ensures  *p == \old(*p);
  behavior zero:
    assumes x == 0;
  behavior nonzero:
    assumes x != 0;
  complete behaviors;
  disjoint behaviors;
*/
void g(int x, int *p) {
    int i;
    /*@
      loop invariant 0 <= i <= 3;
      for nonzero: loop assigns i;
      loop variant 3 - i;
    */
    for (i = 0; i < 3; i++) {
        if (x != 0) {
            *p = 5;
        }
    }
}

static int show(const char *name, int x, int after) {
    printf("%s(%d, p): *p %d after, 0 before: the postcondition %s\n",
           name, x, after, after == 0 ? "holds" : "VIOLATED");
    return after == 0;
}

int main(void) {
    int a = 0, b = 0, c = 0, d = 0;
    int ok;
    f(0, &a);
    f(1, &b);
    g(0, &c);
    g(1, &d);
    ok = show("f", 0, a);
    ok = !show("f", 1, b) && ok;
    ok = show("g", 0, c) && ok;
    ok = !show("g", 1, d) && ok;
    return ok ? 0 : 1;
}
