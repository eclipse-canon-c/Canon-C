/*
 * VERIFY-039 G16 probe, the top of the address space: ptr_align_up and
 * ptr_align_padding at an address within align - 1 bytes of its end.
 *
 * Both functions pass p's address, as a usize, to a helper whose contract
 * requires n <= CANON_USIZE_MAX - (align - 1) (align_up's third
 * precondition, and align_padding's), and neither requires anything of p
 * but that it be non-null. Above that bound n + align - 1 wraps. This file
 * maps a page near the top of a 32-bit address space, at 0xFFFF0000, and
 * asks both functions for the next 128 KiB boundary above 0xFFFF0001,
 * which is 2^32, one past the last address:
 *
 *   - align_up's precondition at ptr_align_up's call, and align_padding's
 *     at ptr_align_padding's: VIOLATED;
 *   - ptr_align_up's result, which its comment gives as "Aligned pointer
 *     >= p, or NULL if p == NULL": the wrapped sum is masked to 0, so the
 *     result is NULL, VIOLATED;
 *   - ptr_align_padding's result, 0xFFFF, the distance to 2^32 that its
 *     postconditions state, `< align` and `exact` read over integers:
 *     holds, the wrap in align_padding's body cancelling in usize.
 *
 * The exit status is 1 if any check is VIOLATED. On LP64 the address space
 * ends far above any address a user process is given: "not applicable",
 * exit 0; also if the page cannot be mapped at that address.
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

    check("align_up's requires_3 at ptr_align_up's call",
          n <= CANON_USIZE_MAX - (align - 1u));
    up = ptr_align_up(p, align);
    printf("ptr_align_up(p, align) = %p\n", up);
    check("ptr_align_up: result >= p, as its comment says",
          (uintptr_t)up >= (uintptr_t)p);

    check("align_padding's requires_3 at ptr_align_padding's call",
          n <= CANON_USIZE_MAX - (align - 1u));
    pad = ptr_align_padding(p, align);
    printf("ptr_align_padding(p, align) = %#lx\n", (unsigned long)pad);
    /* Over integers: the next multiple of align at or above n is 2^32
     * (n > 2^32 - align), so `exact` gives 2^32 - n, below align. */
    check("ptr_align_padding: \\result == 2^32 - n and \\result < align",
          pad == (usize)(0u - n) && pad < align);

    (void)munmap(page, 4096u);
    return violated;
}
