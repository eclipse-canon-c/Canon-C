/*
 * VERIFY-086: the heap, called only where documented. No function calls
 * malloc, calloc, realloc or free but core/memory.h's two wrappers,
 * mem_alloc and mem_free. MetAcsl puts a check at each call of every other
 * function. A name the unit does not declare cannot be called there:
 * \tguard makes that part of the check true.
 */
/*@ meta \prop,
      \name(heap_raw_only_in_memory_wrappers),
      \targets(\diff(\ALL, {mem_alloc, mem_free})),
      \context(\calling),
      \tguard(\called != malloc) && \tguard(\called != calloc) &&
      \tguard(\called != realloc) && \tguard(\called != free);
*/
