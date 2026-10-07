/* Test program for tools/idioms: deliberately uses shapes the substrate never
 * uses — goto, recursion, a bit-field, a new ACSL built-in, a new clause kind,
 * setjmp/longjmp, and an unverified header — so each must be reported. The
 * union is a negative control: the substrate already uses unions (deque,
 * result), so it must NOT be reported. */
#include <setjmp.h>
#include "core/arena.h"
#include "util/str/intern.h"

typedef union { long i; void* p; } Cell;
typedef struct { unsigned flag : 1; unsigned rest : 31; } Bits;
static jmp_buf env;

/*@ requires \valid(c);
    requires \offset(c) >= 0;
    assigns *c;
    allocates \nothing;
*/
static long depth(Cell* c, int n) {
    if (n <= 0) { goto done; }
    c->i += depth(c, n - 1);
done:
    if (c->i < 0) { longjmp(env, 1); }
    return c->i;
}
