/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_DATA_ARRAY_H
#define CANON_DATA_ARRAY_H

#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/checked.h"
#include "core/ownership.h"
#include "core/slice.h"
#include "semantics/option/option.h"

/**
 * @file data/array.h
 * @brief Fixed-size typed array -- the missing companion to vec
 *
 * array<T, N> is the compile-time-sized sibling of vec<T>.
 * Where vec has a runtime-fixed capacity chosen at init,
 * array has a capacity baked into its type at compile time.
 *
 * Core ideas:
 * ----------------------------------------------------------------------------
 * - Capacity N is a compile-time constant -- encoded in the generated type
 * - Always fully initialized -- no "unset" elements, no length tracking
 * - Zero heap allocation -- the element buffer lives inside the struct
 * - Safe indexing -- bounds-checked get/set, option variants for out-of-bounds
 * - Iteration via slice -- array_as_slice() yields a typed non-owning view
 * - Suitable for embedded, real-time, and stack-critical code
 *
 * Ownership conventions (see core/ownership.h):
 * ----------------------------------------------------------------------------
 * The array struct is value-typed (buffer lives inside the struct, not on the
 * heap), so owned/borrowed annotations apply to pointers-to-array passed
 * across API boundaries. All functions in this header borrow their inputs:
 *
 * - _from_ptr(src, count)  -- src is borrowed(const type*)
 * - _get(a, i, out)        -- a is borrowed(const array*), out is borrowed(type*)
 * - _get_option(a, i)      -- a is borrowed(const array*)
 * - _get_unchecked(a, i)   -- a is borrowed(const array*)
 * - _set(a, i, val)        -- a is borrowed(array*)
 * - _at(a, i)              -- a is borrowed(array*); returns borrowed(type*)
 * - _first(a)              -- a is borrowed(array*); returns borrowed(type*)
 * - _last(a)               -- a is borrowed(array*); returns borrowed(type*)
 * - _fill_all(a, val)      -- a is borrowed(array*)
 * - _copy_from(dst, src)   -- both borrowed
 * - _equal(a, b)           -- both borrowed(const array*)
 * - _as_slice(a)           -- a is borrowed(array*); returns borrowed(slice_##type)
 * - _as_bytes(a)           -- a is borrowed(array*); returns borrowed(bytes_t)
 * - _as_cbytes(a)          -- a is borrowed(const array*); returns borrowed(cbytes_t)
 *
 * Relationship to other containers:
 * ----------------------------------------------------------------------------
 * - array<T, N>    -- compile-time N, always full, lives in the struct
 * - vec<T>         -- runtime N (fixed at init), tracks length, caller buffer
 * - smallvec<T, N> -- inline-first, N elements inline then spills to heap
 * - dynvec<T>      -- runtime, auto-growing, heap-allocated
 *
 * Use array when:
 * ----------------------------------------------------------------------------
 * - The number of elements is known at compile time and never changes
 * - You need the buffer to live inside the struct (no external pointer)
 * - You are targeting embedded/real-time environments
 * - You want the simplest possible typed container
 *
 * Do not use array when:
 * ----------------------------------------------------------------------------
 * - Capacity varies at runtime (use vec)
 * - You need a length/count separate from capacity (use vec)
 * - You need automatic growth (use dynvec or smallvec)
 *
 * Memory layout:
 * ----------------------------------------------------------------------------
 * - sizeof(array_##type##_##N) == N * sizeof(type)
 * - No hidden padding beyond what the C compiler adds for alignment
 * - The struct contains exactly one field: type items[N]
 *
 * Dependency rule:
 * ----------------------------------------------------------------------------
 * array.h lives in data/ and may include core/ and semantics/ only.
 * No other data/ headers may be included here.
 *
 * Portability:
 * ----------------------------------------------------------------------------
 * - Requires C99 or later
 * - Uses <string.h> for memset and memcmp
 * - Uses checked_mul() from core/primitives/checked.h for overflow-safe
 *   size calculations
 * - No platform-specific code
 *
 * CANON_PANIC:
 * ----------------------------------------------------------------------------
 * This header includes semantics/option/option.h, which defines CANON_PANIC
 * if it has not already been defined. CANON_PANIC is invoked by option unwrap()
 * and expect() on None. If you need a custom panic handler (e.g. for embedded
 * or certified builds), define CANON_PANIC before including this header:
 *
 *   #define CANON_PANIC(msg) do { log_fatal(msg); system_reset(); } while(0)
 *   #include "data/array.h"
 *
 * The default behaviour is assert(0) in debug builds and abort() in release.
 * See semantics/option/option.h for full documentation.
 *
 * Thread-safety:
 * ----------------------------------------------------------------------------
 * Each array instance is independent -- no shared state.
 * Concurrent reads are safe. Concurrent writes require synchronization.
 *
 * Performance:
 * ----------------------------------------------------------------------------
 * - get / set / at:        O(1)
 * - first / last:          O(1)
 * - fill:                  O(N)
 * - copy:                  O(N)
 * - equal:                 O(N)
 * - as_slice / as_bytes:   O(1)
 * - No allocations anywhere in this file
 *
 * Quick start:
 * ```c
 * #include "data/array.h"
 *
 * CANON_OPTION(int)    // must precede DEFINE_ARRAY when using _option functions
 * DEFINE_SLICE(int)    // must precede DEFINE_ARRAY when using as_slice
 * DEFINE_ARRAY(int, 8)
 *
 * // Zero-initialize all elements
 * array_int_8 a = array_int_8_zero();
 *
 * // Set and get
 * array_int_8_set(&a, 3, 42);
 * int val;
 * array_int_8_get(&a, 3, &val);  // val = 42
 *
 * // Option variant -- no out-param
 * option_int opt = array_int_8_get_option(&a, 3);  // Some(42)
 * option_int oob = array_int_8_get_option(&a, 99); // None
 *
 * // Slice view -- zero-copy, non-owning
 * slice_int sv = array_int_8_as_slice(&a);
 *
 * // Fill all elements with a value
 * array_int_8_fill_all(&a, 0);
 * ```
 *
 * Naming convention:
 * - array_##type##_##N  -- e.g. array_int_8, array_float_16, array_u8_64
 * - All functions follow: array_##type##_##N##_operation(...)
 *
 * @sa data/vec/vec.h             -- runtime-capacity companion
 * @sa core/slice.h               -- slice_##type used by array_as_slice
 * @sa core/ownership.h           -- owned/borrowed/moved/dropped annotations
 * @sa core/primitives/checked.h  -- checked_mul() for overflow-safe arithmetic
 * @sa semantics/option/option.h  -- CANON_OPTION(type) must precede DEFINE_ARRAY
 */

/* ============================================================================
   DEFINE_ARRAY -- instantiate a fixed-size typed array
   ============================================================================ */

/**
 * @brief Instantiates a fixed-size typed array for element type `type` and
 *        compile-time capacity `N`
 *
 * Requirements:
 * - N > 0 -- checked via static_require at instantiation
 * - type must be trivially copyable (memcpy-safe)
 * - DEFINE_SLICE(type) must be called before DEFINE_ARRAY to use as_slice
 * - CANON_OPTION(type) must be called before DEFINE_ARRAY to use any
 *   _option functions (get_option, first_option, last_option)
 *
 * Note on static_require:
 * The static assertion message incorporates both type and N
 * (array_##type##_##N##_capacity_must_be_greater_than_zero) so that multiple
 * DEFINE_ARRAY calls in the same translation unit each produce a unique
 * typedef name and do not conflict with each other.
 *
 * Generated type:
 * - array_##type##_##N  -- struct containing type items[N]
 *
 * Generated functions:
 *
 * Constructors:
 * - array_##type##_##N##_zero()                    -> array (all bytes zeroed)
 * - array_##type##_##N##_fill(val)                 -> array (all elements = val)
 * - array_##type##_##N##_from_ptr(src, count)      -> array (copy from borrowed(const type*))
 *
 * Queries:
 * - array_##type##_##N##_len()                     -> usize (always N)
 * - array_##type##_##N##_size_bytes()              -> usize (N * sizeof(type))
 *
 * Element access:
 * - array_##type##_##N##_get(a, i, out)            -> bool   (borrowed params, bounds-checked)
 * - array_##type##_##N##_get_option(a, i)          -> option_##type
 * - array_##type##_##N##_get_unchecked(a, i)       -> type   (debug-checked)
 * - array_##type##_##N##_set(a, i, val)            -> bool   (bounds-checked)
 * - array_##type##_##N##_at(a, i)                  -> borrowed(type*) or NULL if OOB
 * - array_##type##_##N##_first(a)                  -> borrowed(type*)
 * - array_##type##_##N##_last(a)                   -> borrowed(type*)
 * - array_##type##_##N##_first_option(a)           -> option_##type
 * - array_##type##_##N##_last_option(a)            -> option_##type
 *
 * Mutation:
 * - array_##type##_##N##_fill_all(a, val)          -> void
 * - array_##type##_##N##_copy_from(dst, src)       -> void (both borrowed)
 *
 * Comparison:
 * - array_##type##_##N##_equal(a, b)               -> bool (both borrowed(const array*))
 *
 * Views (zero-copy, all return borrowed; valid while array is alive):
 * - array_##type##_##N##_as_slice(a)               -> borrowed(slice_##type)
 * - array_##type##_##N##_as_bytes(a)               -> borrowed(bytes_t)
 * - array_##type##_##N##_as_cbytes(a)              -> borrowed(cbytes_t)
 *
 * Note: _as_slice_const has been removed. slice_##type carries a mutable
 * pointer and cannot safely represent a view into a const array. Pass a
 * non-const pointer to _as_slice, or access elements via _get / _at.
 *
 * @param type Element type -- must be a valid C identifier
 * @param N    Compile-time capacity (must be > 0)
 */
#define DEFINE_ARRAY(type, N) \
\
static_require((N) > 0, array_##type##_##N##_capacity_must_be_greater_than_zero); \
\
typedef struct { \
    type items[N]; \
} array_##type##_##N; \
\
/* ============================================================================
   Constructors
   ============================================================================ */ \
\
static inline array_##type##_##N array_##type##_##N##_zero(void) { \
    array_##type##_##N a; \
    memset(a.items, 0, sizeof(a.items)); \
    return a; \
} \
\
static inline array_##type##_##N array_##type##_##N##_fill(type val) { \
    array_##type##_##N a; \
    usize _i; \
    for (_i = 0; _i < (usize)(N); _i++) { \
        a.items[_i] = val; \
    } \
    return a; \
} \
\
/** \
 * @brief Returns an array populated by copying count elements from src. \
 * \
 * @param src   borrowed(const type*) -- non-owning view; caller retains ownership \
 * @param count number of elements to copy \
 * \
 * If count < N, remaining elements are zeroed. \
 * If count > N, only the first N elements are copied. \
 * Returns a zeroed array if src == NULL or count == 0. \
 */ \
static inline array_##type##_##N array_##type##_##N##_from_ptr( \
    borrowed(const type *) src, usize count) { \
    array_##type##_##N a; \
    usize copy_n; \
    usize byte_size; \
    memset(a.items, 0, sizeof(a.items)); \
    if (!src || count == 0) { return a; } \
    copy_n = (count < (usize)(N)) ? count : (usize)(N); \
    if (!checked_mul(copy_n, sizeof(type), &byte_size)) { return a; } \
    memcpy(a.items, src, byte_size); \
    return a; \
} \
\
/* ============================================================================
   Queries
   ============================================================================ */ \
\
/** @brief Returns the number of elements (always N). O(1) */ \
static inline usize array_##type##_##N##_len(void) { \
    return (usize)(N); \
} \
\
/** \
 * @brief Returns the total size in bytes (N * sizeof(type)). \
 * Returns CANON_USIZE_MAX on overflow (unreachable in practice). O(1) \
 */ \
static inline usize array_##type##_##N##_size_bytes(void) { \
    usize result; \
    if (!checked_mul((usize)(N), sizeof(type), &result)) { \
        return CANON_USIZE_MAX; \
    } \
    return result; \
} \
\
/* ============================================================================
   Element access
   ============================================================================ */ \
\
/** \
 * @brief Copies element at index i into *out. \
 * @param a   borrowed(const array_##type##_##N*) \
 * @param out borrowed(type*) -- written into caller's storage \
 * @return true on success, false if a == NULL, out == NULL, or i >= N. O(1) \
 */ \
static inline bool array_##type##_##N##_get( \
    borrowed(const array_##type##_##N *) a, usize i, borrowed(type *) out) { \
    if (!a || !out || i >= (usize)(N)) { return false; } \
    *out = a->items[i]; \
    return true; \
} \
\
/** \
 * @brief Returns element at index i as option_##type. \
 * @param a   borrowed(const array_##type##_##N*) \
 * @return Some(element) on success, None if a == NULL or i >= N. O(1) \
 * @pre CANON_OPTION(type) must have been called before DEFINE_ARRAY(type, N). \
 */ \
static inline option_##type array_##type##_##N##_get_option( \
    borrowed(const array_##type##_##N *) a, usize i) { \
    if (!a || i >= (usize)(N)) { return option_##type##_none(); } \
    return option_##type##_some(a->items[i]); \
} \
\
/** \
 * @brief Returns element at index i without bounds checking (fast path). \
 * @param a   borrowed(const array_##type##_##N*) \
 * @pre a != NULL \
 * @pre i < N -- caller must guarantee this \
 * @note Checked via ensure_msg() in debug builds only. O(1) \
 */ \
static inline type array_##type##_##N##_get_unchecked( \
    borrowed(const array_##type##_##N *) a, usize i) { \
    ensure_msg(a != NULL,       "array_" #type "_" #N "_get_unchecked: a cannot be NULL"); \
    ensure_msg(i < (usize)(N),  "array_" #type "_" #N "_get_unchecked: index out of bounds"); \
    return a->items[i]; \
} \
\
/** \
 * @brief Sets element at index i to val. \
 * @param a   borrowed(array_##type##_##N*) \
 * @return true on success, false if a == NULL or i >= N. O(1) \
 */ \
static inline bool array_##type##_##N##_set( \
    borrowed(array_##type##_##N *) a, usize i, type val) { \
    if (!a || i >= (usize)(N)) { return false; } \
    a->items[i] = val; \
    return true; \
} \
\
/** \
 * @brief Returns a borrowed pointer to element at index i, or NULL if OOB. O(1) \
 * @param a   borrowed(array_##type##_##N*) \
 * @return borrowed(type*) -- view into a's storage; caller must NOT free \
 */ \
static inline borrowed(type *) array_##type##_##N##_at( \
    borrowed(array_##type##_##N *) a, usize i) { \
    if (!a || i >= (usize)(N)) { return NULL; } \
    return &a->items[i]; \
} \
\
/** \
 * @brief Returns a borrowed pointer to the first element. Never NULL if a != NULL. O(1) \
 * @param a   borrowed(array_##type##_##N*) \
 * @return borrowed(type*) -- view into a's storage; caller must NOT free \
 */ \
static inline borrowed(type *) array_##type##_##N##_first( \
    borrowed(array_##type##_##N *) a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_first: a cannot be NULL"); \
    return &a->items[0]; \
} \
\
/** \
 * @brief Returns a borrowed pointer to the last element. Never NULL if a != NULL. O(1) \
 * @param a   borrowed(array_##type##_##N*) \
 * @return borrowed(type*) -- view into a's storage; caller must NOT free \
 */ \
static inline borrowed(type *) array_##type##_##N##_last( \
    borrowed(array_##type##_##N *) a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_last: a cannot be NULL"); \
    return &a->items[(usize)(N) - 1u]; \
} \
\
/** \
 * @brief Returns first element as option_##type. O(1) \
 * @param a   borrowed(const array_##type##_##N*) \
 * @pre CANON_OPTION(type) must have been called before DEFINE_ARRAY(type, N). \
 */ \
static inline option_##type array_##type##_##N##_first_option( \
    borrowed(const array_##type##_##N *) a) { \
    if (!a) { return option_##type##_none(); } \
    return option_##type##_some(a->items[0]); \
} \
\
/** \
 * @brief Returns last element as option_##type. O(1) \
 * @param a   borrowed(const array_##type##_##N*) \
 * @pre CANON_OPTION(type) must have been called before DEFINE_ARRAY(type, N). \
 */ \
static inline option_##type array_##type##_##N##_last_option( \
    borrowed(const array_##type##_##N *) a) { \
    if (!a) { return option_##type##_none(); } \
    return option_##type##_some(a->items[(usize)(N) - 1u]); \
} \
\
/* ============================================================================
   Mutation
   ============================================================================ */ \
\
/** \
 * @brief Sets every element to val. NULL-safe: no-op when a == NULL. O(N) \
 * @param a   borrowed(array_##type##_##N*) \
 */ \
static inline void array_##type##_##N##_fill_all( \
    borrowed(array_##type##_##N *) a, type val) { \
    usize _i; \
    if (!a) { return; } \
    for (_i = 0; _i < (usize)(N); _i++) { \
        a->items[_i] = val; \
    } \
} \
\
/** \
 * @brief Copies all N elements from src into dst. O(N) \
 * @param dst borrowed(array_##type##_##N*)       -- caller retains ownership \
 * @param src borrowed(const array_##type##_##N*) -- caller retains ownership \
 * @pre dst != NULL \
 * @pre src != NULL \
 */ \
static inline void array_##type##_##N##_copy_from( \
    borrowed(array_##type##_##N *)       dst, \
    borrowed(const array_##type##_##N *) src) { \
    require_msg(dst != NULL, "array_" #type "_" #N "_copy_from: dst cannot be NULL"); \
    require_msg(src != NULL, "array_" #type "_" #N "_copy_from: src cannot be NULL"); \
    memcpy(dst->items, src->items, sizeof(dst->items)); \
} \
\
/* ============================================================================
   Comparison
   ============================================================================ */ \
\
/** \
 * @brief Returns true if all N elements of a and b are byte-equal. O(N) \
 * @param a   borrowed(const array_##type##_##N*) \
 * @param b   borrowed(const array_##type##_##N*) \
 * \
 * Uses memcmp -- suitable for types with no padding and no pointer members. \
 * For types with padding or pointer semantics, compare element-by-element \
 * using the appropriate comparator. \
 * \
 * @return false if a == NULL or b == NULL. \
 */ \
static inline bool array_##type##_##N##_equal( \
    borrowed(const array_##type##_##N *) a, \
    borrowed(const array_##type##_##N *) b) { \
    if (!a || !b) { return false; } \
    if (a == b)  { return true;  } \
    return memcmp(a->items, b->items, sizeof(a->items)) == 0; \
} \
\
/* ============================================================================
   Views -- zero-copy non-owning access
   ============================================================================ */ \
\
/** \
 * @brief Returns a borrowed slice_##type view over all N elements. O(1) \
 * \
 * @param a   borrowed(array_##type##_##N*) \
 * @return borrowed(slice_##type) -- view into a's storage; \
 *         caller must NOT free; valid only while a is alive and unmodified. \
 * \
 * @pre DEFINE_SLICE(type) must have been called before DEFINE_ARRAY(type, N). \
 * @pre a != NULL \
 */ \
static inline borrowed(slice_##type) array_##type##_##N##_as_slice( \
    borrowed(array_##type##_##N *) a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_as_slice: a cannot be NULL"); \
    return slice_##type##_from(a->items, (usize)(N)); \
} \
\
/** \
 * @brief Returns a borrowed bytes_t mutable view over all N * sizeof(type) bytes. O(1) \
 * \
 * @param a   borrowed(array_##type##_##N*) \
 * @return borrowed(bytes_t) -- caller must NOT free; valid only while a is alive. \
 * Returns bytes_empty() on overflow (unreachable in practice). \
 * @pre a != NULL \
 */ \
static inline borrowed(bytes_t) array_##type##_##N##_as_bytes( \
    borrowed(array_##type##_##N *) a) { \
    usize byte_size; \
    require_msg(a != NULL, "array_" #type "_" #N "_as_bytes: a cannot be NULL"); \
    if (!checked_mul((usize)(N), sizeof(type), &byte_size)) { \
        return bytes_empty(); \
    } \
    return bytes_from(a->items, byte_size); \
} \
\
/** \
 * @brief Returns a borrowed cbytes_t read-only view over all N * sizeof(type) bytes. O(1) \
 * \
 * @param a   borrowed(const array_##type##_##N*) \
 * @return borrowed(cbytes_t) -- caller must NOT free; valid only while a is alive. \
 * Returns cbytes_empty() on overflow (unreachable in practice). \
 * @pre a != NULL \
 */ \
static inline borrowed(cbytes_t) array_##type##_##N##_as_cbytes( \
    borrowed(const array_##type##_##N *) a) { \
    usize byte_size; \
    require_msg(a != NULL, "array_" #type "_" #N "_as_cbytes: a cannot be NULL"); \
    if (!checked_mul((usize)(N), sizeof(type), &byte_size)) { \
        return cbytes_empty(); \
    } \
    return cbytes_from(a->items, byte_size); \
}

/* ============================================================================
   ARRAY_FOR -- range-based iteration over an array
   ============================================================================ */

/**
 * @def ARRAY_FOR(type, N, arr_ptr, idx_var)
 * @brief Iterates over all elements of an array by index.
 *
 * Declares a usize loop variable `idx_var` running from 0 to N-1.
 * Access elements via arr_ptr->items[idx_var] or array_##type##_##N##_at().
 * Loop body does not execute when arr_ptr == NULL.
 *
 * @param type    Element type
 * @param N       Array capacity
 * @param arr_ptr borrowed(array_##type##_##N*) -- NULL-safe: no iterations if NULL
 * @param idx_var Name of the index variable to declare
 *
 * Example:
 * ```c
 * DEFINE_ARRAY(int, 4)
 * array_int_4 a = array_int_4_zero();
 *
 * ARRAY_FOR(int, 4, &a, i) {
 *     a.items[i] = (int)i * 2;
 * }
 * ```
 */
#define ARRAY_FOR(type, N, arr_ptr, idx_var) \
    for (usize idx_var = 0; (arr_ptr) != NULL && idx_var < (usize)(N); idx_var++)

/**
 * @def ARRAY_FOR_PTR(type, N, arr_ptr, elem_ptr)
 * @brief Iterates over all elements of an array by element pointer.
 *
 * Declares a borrowed `type*` loop variable `elem_ptr` pointing to each element.
 * Suitable for in-place mutation without index arithmetic.
 * Loop body does not execute when arr_ptr == NULL.
 *
 * @param type     Element type
 * @param N        Array capacity
 * @param arr_ptr  borrowed(array_##type##_##N*) -- NULL-safe: no iterations if NULL
 * @param elem_ptr Name of the borrowed element pointer variable to declare
 *
 * Example:
 * ```c
 * array_int_8* ap = &a;   // go through a pointer so &a != NULL warning is avoided
 * ARRAY_FOR_PTR(int, 8, ap, p) {
 *     *p *= 2;
 * }
 * ```
 */
#define ARRAY_FOR_PTR(type, N, arr_ptr, elem_ptr) \
    for (type *elem_ptr = ((arr_ptr) ? &(arr_ptr)->items[0] : NULL); \
         elem_ptr && (usize)(elem_ptr - &(arr_ptr)->items[0]) < (usize)(N); \
         elem_ptr++)

#endif /* CANON_DATA_ARRAY_H */
