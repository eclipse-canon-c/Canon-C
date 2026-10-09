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

/* VERIFY-039 G4 -- docs/deviations.md: frames across byte-level callees.
 *
 * mem_zero states its write through `char *`, inside its `zero` behaviour:
 *   assigns ((char *)ptr)[0 .. size - 1];
 *   ensures zeroed: \forall integer i; 0 <= i < size ==> ((char *)ptr)[i] == 0;
 * It has no default `assigns`, so WP 29 frames a call by the union of its
 * complete behaviours' clauses. WP's typed model keeps one memory array per
 * integer kind, and `char` is `sint8` on the project's machdeps: in the
 * model a call to mem_zero changes only the `sint8` array (and the
 * initialization map), whatever object it zeroes.
 *
 * Each function below zeroes one object through mem_zero and states two
 * postconditions of it: `_frame`, that it is unchanged, and `_zero`, that it
 * is zero. In C every `_frame` is false whenever the object was non-zero,
 * and every `_zero` is true. At `int` and `u8` the object lives outside the
 * `sint8` array; at `char` it lives in it.
 *
 * As C, from the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_frame_typed.c -o p && ./p
 * Expected: all three `_frame` claims VIOLATED, all three `_zero` claims
 * hold; exit 0 (exit 1 if C behaves otherwise), sanitizers clean.
 *
 * With WP: .github/workflows/g4-probe.yml, under the units' own options and
 * restricted to the six named postconditions (-wp-prop). main() is outside
 * the analysed code (Frama-C defines __FRAMAC__). The predicted result is in
 * docs/deviations.md, VERIFY-039 G4, committed before this file.
 */
#include <stdio.h>
#include "core/memory.h"

/*@ requires \valid(a);
    assigns ((char *)a)[0 .. sizeof(int) - 1];
    ensures g4_int_frame: *a == \old(*a);
    ensures g4_int_zero:  *a == 0;
*/
static void g4_zero_int(int* a) { mem_zero(a, sizeof *a); }

/*@ requires \valid(b);
    assigns ((char *)b)[0 .. 0];
    ensures g4_u8_frame: *b == \old(*b);
    ensures g4_u8_zero:  *b == 0;
*/
static void g4_zero_u8(u8* b) { mem_zero(b, 1u); }

/*@ requires \valid(c);
    assigns c[0 .. 0];
    ensures g4_char_frame: *c == \old(*c);
    ensures g4_char_zero:  *c == 0;
*/
static void g4_zero_char(char* c) { mem_zero(c, 1u); }

#ifndef __FRAMAC__
static int report(const char* type, int unchanged, int zero) {
    printf("%-4s after mem_zero: g4_%s_frame (unchanged) %-8s  g4_%s_zero (zero) %s\n",
           type, type, unchanged ? "holds" : "VIOLATED", type, zero ? "holds" : "VIOLATED");
    return (!unchanged && zero) ? 0 : 1;
}

int main(void) {
    int  a = 0x01020304;
    u8   b = 0xABu;
    char c = 'x';
    const int  a0 = a;
    const u8   b0 = b;
    const char c0 = c;
    int bad = 0;

    g4_zero_int(&a);
    g4_zero_u8(&b);
    g4_zero_char(&c);
    bad += report("int",  a == a0, a == 0);
    bad += report("u8",   b == b0, b == 0);
    bad += report("char", c == c0, c == 0);
    printf("%s\n", bad ? "UNEXPECTED: C did not behave as stated above"
                       : "every _frame claim is false in C, every _zero claim true");
    return bad ? 1 : 0;
}
#endif
