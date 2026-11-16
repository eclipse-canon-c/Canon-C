/*
 * VERIFY-086: the heap, called only where documented. No function calls
 * core/memory.h's mem_alloc, mem_free or mem_alloc_array_checked but those
 * documented to reach the heap: mem_alloc_array_checked, which allocates
 * through mem_alloc, and vec's heap constructor and destructor,
 * instantiated in the proof units as vec_int_alloc and vec_int_free
 * (data/vec/vec.h: "Three allocation strategies: buffer-init, heap,
 * arena"). A name a unit does not have is ignored in \targets
 * (unknown-func, made inactive), and \tguard makes a name it does not
 * declare true in the check.
 */
/*@ meta \prop,
      \name(heap_wrappers_only_where_documented),
      \targets(\diff(\ALL, {mem_alloc_array_checked, vec_int_alloc, vec_int_free})),
      \context(\calling),
      \tguard(\called != mem_alloc) && \tguard(\called != mem_free) &&
      \tguard(\called != mem_alloc_array_checked);
*/
