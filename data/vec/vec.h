#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include "data/vec/vec_defn.h"

/**
 * @file vec.h
 * @brief Bounded typed vectors with explicit caller-owned buffers
 *
 * This is the primary user-facing header for the Canon-C vec module.
 * Including this file gives you the full header-only API using default
 * mangled names (canon_vec_##type).
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed capacity — no automatic growth
 * - Caller owns the buffer (stack, heap, arena, static)
 * - All bounds-checked operations return Result<bool, Error> or Option<T>
 * - Unchecked variants available for hot paths (debug-mode guarded)
 * - Three allocation strategies: buffer-init, heap, arena
 *
 * Design principles:
 * ────────────────────────────────────────────────────────────────────────────
 * - You always know where memory comes from
 * - You always know when an operation can fail
 * - No hidden allocations, no surprise resizes
 * - For auto-growing vectors, use data/convenience/dynvec.h explicitly
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - push/pop:              O(1), no allocation
 * - insert/remove:         O(n), shifts elements
 * - append_array/extend:   O(k), copies k elements
 * - get/set/at:            O(1)
 * - iter_next:             O(1) per step
 * - slice_init:            O(1), zero-copy
 * - alloc:                 O(1), single malloc
 * - arena_alloc:           O(1), arena bump
 * - struct size:           sizeof(type*) + 2*sizeof(usize)
 * - element overhead:      0 bytes beyond sizeof(type) per element
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Each vector instance is independent — no shared state
 * - Concurrent reads on the same instance are safe
 * - Concurrent modifications require external synchronization
 * - Iterators and slices are invalidated by any modification
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - C11 _Static_assert used when available (non-fatal fallback otherwise)
 *
 * Quick start:
 * ```c
 * #include "data/vec/vec.h"
 *
 * // Instantiate a typed vector for int
 * DEFINE_VEC(static inline, int)
 *
 * // Stack-backed vector
 * int buf[64];
 * canon_vec_int v = canon_vec_int_init(buf, 64);
 * canon_vec_int_push(&v, 10);
 * canon_vec_int_push(&v, 20);
 *
 * // Heap-backed vector
 * canon_vec_int h = canon_vec_int_alloc(128);
 * canon_vec_int_push(&h, 99);
 * canon_vec_int_free(&h);
 *
 * // Arena-backed vector
 * canon_vec_int a = canon_vec_int_arena_alloc(&arena, 32);
 * canon_vec_int_push(&a, 1);
 * // no free needed — arena owns the memory
 *
 * // Pointer types (typedef first)
 * typedef void* voidptr;
 * DEFINE_VEC(static inline, voidptr)
 * ```
 *
 * Separate compilation (large projects):
 * ```c
 * // In tasks.h:
 * #include "data/vec/vec_decl.h"
 * DECLARE_VEC(Task)
 *
 * // In tasks.c:
 * #include "data/vec/vec_defn.h"
 * DEFINE_VEC(, Task)
 * ```
 *
 * Extension headers (optional):
 * ```c
 * #include "data/vec/vec_range.h"  // adds vec_extend_from_range
 * #include "data/vec/vec_fmt.h"    // adds vec_to_stringbuf
 * ```
 *
 * @sa vec_defn.h, vec_decl.h, vec_mangle.h, vec_impl.h
 * @sa vec_range.h, vec_fmt.h
 * @sa data/convenience/dynvec.h (auto-growing variant)
 * @sa data/convenience/smallvec.h (inline-first variant)
 */

/* ════════════════════════════════════════════════════════════════════════════
   C11 static assert helper (non-fatal in C99)
   ════════════════════════════════════════════════════════════════════════════ */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /**
     * @brief Static assertion for vec element types (C11 only)
     *
     * Verifies element type has non-zero size at compile time.
     * Silently disabled in C99 mode.
     */
    #define VEC_ASSERT_TYPE(type) \
        _Static_assert(sizeof(type) > 0, "vec: element type must have non-zero size")
#else
    #define VEC_ASSERT_TYPE(type) /* C99: no static assert */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Branch hint helpers
   ════════════════════════════════════════════════════════════════════════════ */

#if defined(__GNUC__) || defined(__clang__)
    /**
     * @brief Hints to the compiler that condition is likely true
     *
     * Used on hot paths (push to non-full vec, get within bounds).
     * Has no effect on non-GCC/Clang compilers.
     */
    #define VEC_LIKELY(x)   __builtin_expect(!!(x), 1)
    /**
     * @brief Hints to the compiler that condition is likely false
     *
     * Used on error paths (push to full vec, out-of-bounds get).
     */
    #define VEC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define VEC_LIKELY(x)   (x)
    #define VEC_UNLIKELY(x) (x)
#endif

#endif /* CANON_DATA_VEC_H */
