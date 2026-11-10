/*
 * VERIFY-066, Eva's error probe: r0_full.c
 *
 * Canon-C's arena_alloc (core/arena.h), standalone: the Arena, its
 * invariant, arena_can_fit as core/arena.h writes it, arena_alloc's contract
 * with its three behaviours, and a body that computes what the contract
 * states. Frama-C 29's Eva stops on entering arena_alloc, in every program
 * that calls it, with the uncaught exception Eva__Eval_terms.Reduce_to_bottom
 * (docs/deviations.md, VERIFY-066, the fix's run).
 *
 *     frama-c -eva r0_full.c
 */
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *buffer;
    size_t capacity;
    size_t offset;
} Arena;

/*@ predicate arena_invariant(Arena *a) =
      \valid(a) &&
      a->capacity > 0 &&
      a->capacity <= 1073741824 &&
      a->offset <= a->capacity &&
      \valid(a->buffer + (0 .. a->capacity - 1)) &&
      \separated(a, a->buffer + (0 .. a->capacity - 1));

    predicate arena_can_fit{L}(Arena *a, integer size, integer alignment) =
      \let cur  = a->offset;
      \let addr = (size_t)(uintptr_t)(a->buffer + cur);
      \let pad  = ((addr + alignment - 1) & ~(alignment - 1)) - addr;
      cur <= SIZE_MAX - pad &&
      cur + pad <= SIZE_MAX - size &&
      cur + pad + size <= a->capacity;
*/

/*@ requires arena_invariant(arena);
    assigns *arena;
    behavior size_zero:
      assumes size == 0;
      ensures \result == \null;
    behavior fits:
      assumes size > 0;
      assumes arena_can_fit(arena, size, 16);
      ensures \result != \null;
    behavior does_not_fit:
      assumes size > 0;
      assumes !arena_can_fit(arena, size, 16);
      ensures \result == \null;
    complete behaviors;
    disjoint behaviors;
*/
void *arena_alloc(Arena *arena, size_t size)
{
    uintptr_t addr;
    size_t pad;
    void *result;

    if (size == 0u) {
        return NULL;
    }
    addr = (uintptr_t)(arena->buffer + arena->offset);
    pad = (size_t)(((addr + 15u) & ~(uintptr_t)15u) - addr);
    if (arena->offset > SIZE_MAX - pad ||
        arena->offset + pad > SIZE_MAX - size ||
        arena->offset + pad + size > arena->capacity) {
        return NULL;
    }
    arena->offset += pad;
    result = arena->buffer + arena->offset;
    arena->offset += size;
    return result;
}

static unsigned long long backing[32];   /* 256 bytes */

int main(void)
{
    Arena a;
    a.buffer = (unsigned char *)backing;
    a.capacity = sizeof backing;
    a.offset = 0;
    return arena_alloc(&a, 10u) == NULL;
}
