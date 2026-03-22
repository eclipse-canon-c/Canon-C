/**
 * @file unique_impl.h
 * @brief Pure implementation logic for Canon-C unique (consecutive duplicate removal)
 *
 * This file contains the function body for algo_unique(). It has zero naming
 * assumptions beyond the fixed function name — every linkage specifier comes
 * from ALGO_UNIQUE_LINKAGE set by the includer. Do NOT include this file
 * directly. Include unique.h (header-only) or use unique_decl.h + unique_defn.h
 * for separate compilation.
 *
 * Algorithm: single-pass two-pointer scan
 * ────────────────────────────────────────────────────────────────────────────
 * Maintains two indices into the array — read and write:
 *
 *   write starts at 1 (the first slot after the anchor element).
 *   read  walks from 1 to len-1.
 *
 * At each step, the element at read is compared to the last element written
 * (at write-1). If they differ, the element is copied to write (unless
 * write == read, avoiding a self-copy) and write is advanced. If they are
 * equal, read advances but write does not — the duplicate is skipped.
 *
 * After the pass, write holds the new logical length. Elements in
 * [write, len) are stale and must not be read by the caller.
 *
 * Empty and single-element inputs are trivially unique — the function
 * returns len immediately after the precondition checks.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * unique_impl.h includes only:
 *   - unique_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/compare.h
 *   - core/primitives/ptr.h
 *   - core/memory.h
 *   - core/ownership.h
 *
 * @sa unique.h        — user-facing entry point (include this)
 * @sa unique_mangle.h — name conventions
 * @sa unique_decl.h   — forward declarations for separate compilation
 * @sa unique_defn.h   — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_UNIQUE_LINKAGE
    #error "unique_impl.h must not be included directly. Include unique.h instead."
#endif

#include "unique_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/compare.h"
#include "core/primitives/ptr.h"
#include "core/memory.h"
#include "core/ownership.h"

/* ============================================================================
 * algo_unique — consecutive duplicate removal over a flat array
 * ========================================================================= */

/**
 * @brief Removes consecutive duplicate elements from a flat array in place
 *
 * Collapses each run of consecutive equal elements into a single occurrence,
 * preserving the relative order of first appearances. Returns the new logical
 * length of the unique prefix — elements at indices [0, return) are valid;
 * elements at [return, len) contain stale data and must not be read.
 *
 * This removes CONSECUTIVE duplicates only. For full deduplication, sort
 * the array with the same comparator before calling this function.
 *
 * Empty (len == 0) and single-element (len == 1) arrays are trivially unique —
 * the function returns len immediately. NULL array returns 0.
 *
 * @param array     Pointer to first element (borrowed, modified in place)
 * @param len       Number of elements (0 and 1 are valid — returns len)
 * @param elem_size Size in bytes of each element (must be > 0)
 * @param cmp       Comparator — returns 0 if elements are equal (borrowed)
 * @param ctx       Optional context passed to cmp (borrowed, may be NULL)
 *
 * @pre elem_size > 0  — triggers require_msg
 * @pre cmp != NULL    — triggers require_msg
 *
 * @return New logical length in [0, len].
 *         Returns 0 if array is NULL.
 *         Returns len if len <= 1.
 *
 * @post array[0..return-1] contains no consecutive equal elements
 * @post Relative order of first occurrences is preserved
 * @post array[return..len-1] contains stale data — do not read
 * @post No allocations performed
 *
 * Performance:
 * - Time:  O(n) — single pass, at most n-1 comparisons
 * - Space: O(1) — no allocation; at most n-1 element copies
 */
ALGO_UNIQUE_LINKAGE usize algo_unique(
    borrowed(void*)         array,
    usize                   len,
    usize                   elem_size,
    borrowed(algo_cmp_fn)   cmp,
    borrowed(void*)         ctx)
{
    require_msg(elem_size > 0,    "algo_unique: elem_size must be > 0");
    require_msg(cmp       != NULL, "algo_unique: cmp cannot be NULL");

    if (!array || len <= 1) return len;

    usize write = 1;

    for (usize read = 1; read < len; ++read) {
        const void* prev = ptr_elem_const(array, write - 1, elem_size);
        const void* curr = ptr_elem_const(array, read,      elem_size);

        if (cmp(prev, curr, ctx) != 0) {
            /* curr differs from the last unique element — keep it */
            if (write != read) {
                /* avoid self-copy when no duplicates have been seen yet */
                mem_copy(ptr_elem(array, write, elem_size), curr, elem_size);
            }
            ++write;
        }
        /* equal: skip curr — do not advance write */
    }

    return write;
}
