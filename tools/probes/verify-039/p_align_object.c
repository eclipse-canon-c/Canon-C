/*
 * VERIFY-039 G16 probe, the object: where ptr_align_up's and
 * ptr_align_down's results lie, read against the repaired contracts.
 *
 * Until G16 both functions' nonnull behaviours said that the result has
 * p's base address (same_base), lies on the stated side of p (not_below,
 * not_above) and within align - 1 bytes of it (bounded): claims about the
 * result as a pointer into p's block. The result is p's address rounded as
 * an integer and converted back, and the rounded address can lie outside
 * p's object. Where it lay in another object, same_base was false;
 * wherever it left p's object the other four had no defined value, C
 * defining neither the order nor the difference of the two pointers (C11
 * 6.5.8p5, 6.5.6p9). Since the repair each function states one
 * postcondition, address: (usize)(uintptr_t)\result is p's address rounded,
 * as an integer. ptr_align_up also requires fits: p's address at most
 * CANON_USIZE_MAX - (align - 1), short of the wrap at the top of the
 * address space that p_align_wrap32.c shows. This file rounds five pointers
 * and reports, for each, where the result lies, which the contracts no
 * longer claim, and whether address holds:
 *
 *   A  ptr_align_down(p + 3, 4096), p from calloc(64, 1): the header's own
 *      example, the start of p's page, below p's block unless the block
 *      begins on a page ("not applicable" if it does)
 *   B  ptr_align_up(ten + 9, 64), ten a 10-byte array aligned to 16: the
 *      next 64-byte boundary, at least 16 bytes past ten's start, so past
 *      its end
 *   E  ptr_align_down(r + 1, a), p, q and r three 64-byte heap blocks
 *      allocated in that order, for the least power of two a from 16 to
 *      4096 that puts the result inside q's block ("not applicable" if
 *      none does, as where the allocator hands the blocks out downward)
 *   C  ptr_align_up(big + 1, 16) and
 *   D  ptr_align_down(big + 17, 16), big a 64-byte array aligned to 64:
 *      controls, the results inside big
 *
 * Addresses are compared as uintptr_t, as the functions compute them: a
 * pointer comparison across objects would be undefined. Each case prints
 * where the result lies: inside p's object (one past its end counting as
 * in it), inside another object the probe owns, where same_base was false
 * before G16, or outside every object the probe owns. The exit status is 1
 * if address fails in some case, or if fits does not hold for a pointer
 * rounded up.
 *
 * From the repository root:
 *     cc -std=c99 -I . tools/probes/verify-039/p_align_object.c \
 *        -o p_align_object && ./p_align_object
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <stdlib.h>
#include "core/primitives/ptr.h"

static unsigned char ten[10] __attribute__((aligned(16)));
static unsigned char big[64] __attribute__((aligned(64)));

typedef struct { const char *name; const void *base; usize size; } object;

static object objs[5];

static int violated;

/* a inside o; for p's own object, one past its end counts as inside */
static int in(const object *o, uintptr_t a, int own)
{
    const uintptr_t b = (uintptr_t)o->base;
    return a >= b && (own ? a <= b + o->size : a < b + o->size);
}

/* res = ptr_align_up(p, align) when up, else ptr_align_down(p, align);
 * own indexes p's object in objs. */
static void report(const char *call, int own, const void *p, const void *res,
                   usize align, int up)
{
    const usize pa = (usize)(uintptr_t)p, ra = (usize)(uintptr_t)res;
    const int fits = !up || pa <= CANON_USIZE_MAX - (align - 1u);
    const usize want = up ? ((pa + align - 1u) & ~(align - 1u))
                          : (pa & ~(align - 1u));
    int k, other = -1;
    printf("%s: p %p, result %p\n", call, p, res);
    if (in(&objs[own], (uintptr_t)ra, 1)) {
        printf("    inside %s, p's object\n", objs[own].name);
    } else {
        for (k = 0; k < 5; k++) {
            if (k != own && in(&objs[k], (uintptr_t)ra, 0)) {
                other = k;
                break;
            }
        }
        if (other >= 0) {
            printf("    inside %s, another object (same_base, before G16: "
                   "false)\n", objs[other].name);
        } else {
            printf("    outside %s and every other object of the probe's\n",
                   objs[own].name);
        }
    }
    if (up) {
        printf("    fits: %s\n", fits ? "holds" : "VIOLATED");
    }
    printf("    address: %s\n", fits && ra == want ? "holds" : "VIOLATED");
    if (!fits || ra != want) { violated = 1; }
}

int main(void)
{
    unsigned char *p = calloc(64u, 1u);
    unsigned char *q = calloc(64u, 1u);
    unsigned char *r = calloc(64u, 1u);
    usize a;
    int found = 0;

    if (p == NULL || q == NULL || r == NULL) { return 2; }
    objs[0].name = "p's block"; objs[0].base = p;   objs[0].size = 64u;
    objs[1].name = "q's block"; objs[1].base = q;   objs[1].size = 64u;
    objs[2].name = "r's block"; objs[2].base = r;   objs[2].size = 64u;
    objs[3].name = "ten";       objs[3].base = ten; objs[3].size = sizeof ten;
    objs[4].name = "big";       objs[4].base = big; objs[4].size = sizeof big;

    if (((uintptr_t)p & 4095u) == 0u) {
        puts("A ptr_align_down(p + 3, 4096): not applicable, "
             "p begins a page");
    } else {
        report("A ptr_align_down(p + 3, 4096)", 0, p + 3,
               ptr_align_down(p + 3, 4096u), 4096u, 0);
    }
    report("B ptr_align_up(ten + 9, 64)", 3, ten + 9,
           ptr_align_up(ten + 9, 64u), 64u, 1);
    for (a = 16u; a <= 4096u && !found; a <<= 1) {
        const uintptr_t ra = (uintptr_t)ptr_align_down(r + 1, a);
        if (ra >= (uintptr_t)q && ra < (uintptr_t)q + 64u) {
            char call[40];
            (void)snprintf(call, sizeof call, "E ptr_align_down(r + 1, %lu)",
                           (unsigned long)a);
            report(call, 2, r + 1, ptr_align_down(r + 1, a), a, 0);
            found = 1;
        }
    }
    if (!found) {
        puts("E ptr_align_down(r + 1, a): not applicable, no a from 16 to "
             "4096 puts the result inside q's block");
    }
    report("C ptr_align_up(big + 1, 16)", 4, big + 1,
           ptr_align_up(big + 1, 16u), 16u, 1);
    report("D ptr_align_down(big + 17, 16)", 4, big + 17,
           ptr_align_down(big + 17, 16u), 16u, 0);

    free(p);
    free(q);
    free(r);
    return violated;
}
