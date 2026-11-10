/*
 * VERIFY-066, Eva's error probe: r4_onebhv.c
 *
 * r1_min.c with one behaviour: no negation of the predicate, no complete or
 * disjoint clause.
 *
 *     frama-c -eva r4_onebhv.c
 */
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *buffer;
    size_t capacity;
    size_t offset;
} Arena;

/*@ predicate fits(Arena *a, integer size) =
      \let addr = (size_t)(uintptr_t)(a->buffer + a->offset);
      \let pad  = ((addr + 15) & ~15) - addr;
      a->offset + pad + size <= a->capacity;
*/

/*@ requires \valid(a);
    requires \valid(a->buffer + (0 .. a->capacity - 1));
    requires a->offset <= a->capacity <= 4096;
    assigns a->offset;
    behavior fits:
      assumes fits(a, size);
      ensures \result != \null;
*/
void *alloc(Arena *a, size_t size)
{
    uintptr_t addr = (uintptr_t)(a->buffer + a->offset);
    size_t pad = (size_t)(((addr + 15u) & ~(uintptr_t)15u) - addr);
    void *p;

    if (a->offset + pad + size > a->capacity) {
        return NULL;
    }
    a->offset += pad;
    p = a->buffer + a->offset;
    a->offset += size;
    return p;
}

static unsigned long long backing[32];   /* 256 bytes */

int main(void)
{
    Arena a;
    a.buffer = (unsigned char *)backing;
    a.capacity = sizeof backing;
    a.offset = 0;
    return alloc(&a, 10u) == NULL;
}
