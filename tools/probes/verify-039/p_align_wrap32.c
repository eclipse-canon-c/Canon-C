/*
 * VERIFY-039 G16 probe, the top of the address space: ptr_align_up and
 * ptr_align_padding at an address within align - 1 bytes of its end, read
 * against the repaired contracts.
 *
 * Until G16 both functions passed p's address, as a usize, to a helper
 * whose contract required n <= CANON_USIZE_MAX - (align - 1) (align_up's
 * third precondition, and align_padding's), and neither required anything
 * of p but that it be non-null. Above that bound n + align - 1 wraps. This
 * file maps a page near the top of a 32-bit address space, at 0xFFFF0000,
 * and asks both functions for the next 128 KiB boundary above 0xFFFF0001,
 * which is 2^32, one past the last address. Before G16 the two calls met
 * every precondition the functions stated and broke their helpers'.
 * Since the repair:
 *
 *   - ptr_align_up requires fits, the same bound on a non-null p, so this
 *     call is outside its contract: fits is reported "excluded". It
 *     returns NULL, as before; its comment no longer promises a result
 *     at or above p for such a p;
 *   - align_padding computes (0 - n) & (align - 1) and requires no bound,
 *     so ptr_align_padding's call meets every precondition. Its result,
 *     0xFFFF, is the distance to 2^32 that its postconditions state,
 *     `< align` and `exact` read over integers: holds.
 *
 * The exit status is 1 if fits holds at this address, or if the padding's
 * postconditions fail. On LP64 the address space ends far above any
 * address a user process is given: "not applicable", exit 0; also if the
 * page cannot be mapped at that address.
 *
 * From the repository root (ILP32, Linux):
 *     cc -m32 -std=c99 -I . tools/probes/verify-039/p_align_wrap32.c \
 *        -o p_align_wrap32 && ./p_align_wrap32
 */
#define _DEFAULT_SOURCE
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <sys/mman.h>
#include "core/primitives/ptr.h"

static int violated;

static void check(const char *what, int holds)
{
    printf("%-58s %s\n", what, holds ? "holds" : "VIOLATED");
    if (!holds) { violated = 1; }
}

int main(void)
{
    const usize align = 0x20000u;
    void *const want = (void *)(uintptr_t)0xFFFF0000u;
    unsigned char *page;
    unsigned char *p;
    usize n;
    void *up;
    usize pad;

    if (sizeof(void *) != 4u) {
        puts("not applicable: not an ILP32 build");
        return 0;
    }
    page = mmap(want, 4096u, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED || (void *)page != want) {
        puts("not applicable: no page mapped at 0xFFFF0000");
        return 0;
    }
    p = page + 1;
    n = (usize)(uintptr_t)p;
    printf("p = %p, align = %#lx\n", (void *)p, (unsigned long)align);

    if (n <= CANON_USIZE_MAX - (align - 1u)) {
        printf("%-58s %s\n", "ptr_align_up's fits at this call", "holds");
        violated = 1;
    } else {
        printf("%-58s %s\n", "ptr_align_up's fits at this call",
               "excluded");
    }
    up = ptr_align_up(p, align);
    printf("ptr_align_up(p, align) = %p, a call outside the contract\n", up);

    pad = ptr_align_padding(p, align);
    printf("ptr_align_padding(p, align) = %#lx\n", (unsigned long)pad);
    /* Over integers: the next multiple of align at or above n is 2^32
     * (n > 2^32 - align), so `exact` gives 2^32 - n, below align. */
    check("ptr_align_padding: \\result == 2^32 - n and \\result < align",
          pad == (usize)(0u - n) && pad < align);
    check("align_padding(n, align), its only postcondition",
          align_padding(n, align) == (usize)(0u - n));

    (void)munmap(page, 4096u);
    return violated;
}
