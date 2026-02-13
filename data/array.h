#ifndef CANON_DATA_ARRAY_H
#define CANON_DATA_ARRAY_H

#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/slice.h"
#include "semantics/option.h"

/**
 * @file data/array.h
 * @brief Fixed-size typed array — the missing companion to vec
 *
 * array<T, N> is the compile-time-sized sibling of vec<T>.
 * Where vec has a runtime-fixed capacity chosen at init,
 * array has a capacity baked into its type at compile time.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Capacity N is a compile-time constant — encoded in the generated type
 * - Always fully initialized — no "unset" elements, no length tracking
 * - Zero heap allocation — the element buffer lives inside the struct
 * - Safe indexing — bounds-checked get/set, option variants for out-of-bounds
 * - Iteration via slice — array_as_slice() yields a typed non-owning view
 * - Suitable for embedded, real-time, and stack-critical code
 *
 * Relationship to other containers:
 * ────────────────────────────────────────────────────────────────────────────
 * - array<T, N>    — compile-time N, always full, lives in the struct
 * - vec<T>         — runtime N (fixed at init), tracks length, caller buffer
 * - smallvec<T, N> — inline-first, N elements inline then spills to heap
 * - dynvec<T>      — runtime, auto-growing, heap-allocated
 *
 * Use array when:
 * ────────────────────────────────────────────────────────────────────────────
 * - The number of elements is known at compile time and never changes
 * - You need the buffer to live inside the struct (no external pointer)
 * - You are targeting embedded/real-time environments
 * - You want the simplest possible typed container
 *
 * Do not use array when:
 * ────────────────────────────────────────────────────────────────────────────
 * - Capacity varies at runtime (use vec)
 * - You need a length/count separate from capacity (use vec)
 * - You need automatic growth (use dynvec or smallvec)
 *
 * Memory layout:
 * ────────────────────────────────────────────────────────────────────────────
 * - sizeof(array_##type##_##N) == N * sizeof(type)
 * - No hidden padding beyond what the C compiler adds for alignment
 * - The struct contains exactly one field: type items[N]
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * array.h lives in data/ and may include core/ and semantics/ only.
 * No other data/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses <string.h> for memset (array_fill_bytes) and memcmp (array_equal)
 * - No platform-specific code
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each array instance is independent — no shared state.
 * Concurrent reads are safe. Concurrent writes require synchronization.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
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
 * // Option variant — no out-param
 * option_int opt = array_int_8_get_option(&a, 3);  // Some(42)
 * option_int oob = array_int_8_get_option(&a, 99); // None
 *
 * // Slice view — zero-copy, non-owning
 * slice_int sv = array_int_8_as_slice(&a);
 *
 * // Fill all elements with a value
 * array_int_8_fill(&a, 0);
 * ```
 *
 * Naming convention:
 * - array_##type##_##N  — e.g. array_int_8, array_float_16, array_u8_64
 * - All functions follow: array_##type##_##N##_operation(...)
 *
 * @sa data/vec/vec.h    — runtime-capacity companion
 * @sa core/slice.h      — slice_##type used by array_as_slice
 */

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_ARRAY — instantiate a fixed-size typed array
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a fixed-size typed array for element type `type` and
 *        compile-time capacity `N`
 *
 * Requirements:
 * - N > 0 — checked via static assertion equivalent at instantiation
 * - type must be trivially copyable (memcpy-safe)
 * - DEFINE_SLICE(type) must be called before using array_as_slice
 * - option_##type must exist before using get_option / last_option / first_option
 *
 * Generated type:
 * - array_##type##_##N  — struct containing type items[N]
 *
 * Generated functions:
 *
 * Constructors:
 * - array_##type##_##N##_zero()                    → array (all bytes zeroed)
 * - array_##type##_##N##_fill(val)                 → array (all elements = val)
 * - array_##type##_##N##_from_ptr(src, count)      → array (copy from pointer)
 *
 * Queries:
 * - array_##type##_##N##_len()                     → usize (always N)
 * - array_##type##_##N##_size_bytes()              → usize (N * sizeof(type))
 *
 * Element access:
 * - array_##type##_##N##_get(a, i, out)            → bool (bounds-checked)
 * - array_##type##_##N##_get_option(a, i)          → option_##type
 * - array_##type##_##N##_get_unchecked(a, i)       → type (debug-checked)
 * - array_##type##_##N##_set(a, i, val)            → bool (bounds-checked)
 * - array_##type##_##N##_at(a, i)                  → type* (NULL if OOB)
 * - array_##type##_##N##_first(a)                  → type*
 * - array_##type##_##N##_last(a)                   → type*
 * - array_##type##_##N##_first_option(a)           → option_##type
 * - array_##type##_##N##_last_option(a)            → option_##type
 *
 * Mutation:
 * - array_##type##_##N##_fill_all(a, val)          → void (set every element)
 * - array_##type##_##N##_copy_from(dst, src)       → void (copy all N elements)
 *
 * Comparison:
 * - array_##type##_##N##_equal(a, b)               → bool (element-wise)
 *
 * Views (zero-copy):
 * - array_##type##_##N##_as_slice(a)               → slice_##type
 * - array_##type##_##N##_as_bytes(a)               → bytes_t
 * - array_##type##_##N##_as_cbytes(a)              → cbytes_t
 *
 * @param type Element type — must be a valid C identifier
 * @param N    Compile-time capacity (must be > 0)
 */
#define DEFINE_ARRAY(type, N) \
\
/** \
 * @brief Fixed-size array of exactly N elements of type \
 * \
 * All N elements are always present — there is no length field. \
 * sizeof(array_##type##_##N) == N * sizeof(type) (plus any struct padding). \
 * \
 * Do not access items directly in generic code — use the provided functions. \
 */ \
typedef struct { \
    type items[N]; /**< Element storage — N elements, always initialized */ \
} array_##type##_##N; \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Constructors \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Returns an array with all bytes zeroed \
 * \
 * Suitable for numeric types, pointers, and any type where zero-bytes \
 * is a valid initialized state. \
 * \
 * @return array_##type##_##N with all bytes == 0 \
 * \
 * Performance: \
 * - Time:  O(N) — memset \
 * - Space: sizeof(array_##type##_##N) on the stack \
 */ \
static inline array_##type##_##N array_##type##_##N##_zero(void) { \
    array_##type##_##N a; \
    memset(a.items, 0, sizeof(a.items)); \
    return a; \
} \
\
/** \
 * @brief Returns an array with every element set to val \
 * \
 * @param val Value to assign to every element \
 * @return array_##type##_##N with all elements == val \
 * \
 * Performance: \
 * - Time:  O(N) \
 * - Space: sizeof(array_##type##_##N) on the stack \
 */ \
static inline array_##type##_##N array_##type##_##N##_fill(type val) { \
    array_##type##_##N a; \
    for (usize _i = 0; _i < (usize)(N); _i++) { \
        a.items[_i] = val; \
    } \
    return a; \
} \
\
/** \
 * @brief Returns an array populated by copying count elements from src \
 * \
 * If count < N, remaining elements are zeroed. \
 * If count > N, only the first N elements are copied. \
 * \
 * @param src   Source pointer (must point to at least min(count, N) elements) \
 * @param count Number of elements to copy from src \
 * @return array_##type##_##N initialized from src \
 * \
 * @pre src != NULL if count > 0 \
 * \
 * Performance: \
 * - Time:  O(N) \
 * - Space: sizeof(array_##type##_##N) on the stack \
 */ \
static inline array_##type##_##N array_##type##_##N##_from_ptr( \
    const type* src, usize count) { \
    array_##type##_##N a; \
    memset(a.items, 0, sizeof(a.items)); \
    if (!src || count == 0) return a; \
    usize copy_n = (count < (usize)(N)) ? count : (usize)(N); \
    memcpy(a.items, src, copy_n * sizeof(type)); \
    return a; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Queries \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Returns the number of elements (always N) \
 * \
 * Compile-time constant — exists for generic code that queries length. \
 * \
 * Performance: O(1) \
 */ \
static inline usize array_##type##_##N##_len(void) { \
    return (usize)(N); \
} \
\
/** \
 * @brief Returns the total size in bytes (N * sizeof(type)) \
 * \
 * Performance: O(1) \
 */ \
static inline usize array_##type##_##N##_size_bytes(void) { \
    return (usize)(N) * sizeof(type); \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Element access \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Copies element at index i into *out (bounds-checked) \
 * \
 * @param a   Array to read from (must not be NULL) \
 * @param i   Element index \
 * @param out Pointer to store the element value \
 * @return true on success, false if a == NULL, out == NULL, or i >= N \
 * \
 * Performance: O(1) \
 */ \
static inline bool array_##type##_##N##_get( \
    const array_##type##_##N* a, usize i, type* out) { \
    if (!a || !out || i >= (usize)(N)) return false; \
    *out = a->items[i]; \
    return true; \
} \
\
/** \
 * @brief Returns element at index i as option_##type \
 * \
 * @param a Array to read from (NULL returns None) \
 * @param i Element index \
 * @return Some(element) on success, None if i >= N or a == NULL \
 * \
 * Performance: O(1) \
 */ \
static inline option_##type array_##type##_##N##_get_option( \
    const array_##type##_##N* a, usize i) { \
    if (!a || i >= (usize)(N)) return option_##type##_none(); \
    return option_##type##_some(a->items[i]); \
} \
\
/** \
 * @brief Returns element at index i without bounds checking (fast path) \
 * \
 * @pre a != NULL \
 * @pre i < N — caller must guarantee this \
 * \
 * @note Checked via ensure_msg() in debug builds only. \
 * \
 * Performance: O(1) \
 */ \
static inline type array_##type##_##N##_get_unchecked( \
    const array_##type##_##N* a, usize i) { \
    ensure_msg(a != NULL,        "array_" #type "_" #N "_get_unchecked: a cannot be NULL"); \
    ensure_msg(i < (usize)(N),   "array_" #type "_" #N "_get_unchecked: index out of bounds"); \
    return a->items[i]; \
} \
\
/** \
 * @brief Sets element at index i to val (bounds-checked) \
 * \
 * @param a   Array to write to (must not be NULL) \
 * @param i   Element index \
 * @param val Value to set \
 * @return true on success, false if a == NULL or i >= N \
 * \
 * Performance: O(1) \
 */ \
static inline bool array_##type##_##N##_set( \
    array_##type##_##N* a, usize i, type val) { \
    if (!a || i >= (usize)(N)) return false; \
    a->items[i] = val; \
    return true; \
} \
\
/** \
 * @brief Returns a pointer to element at index i, or NULL if out of bounds \
 * \
 * Performance: O(1) \
 */ \
static inline type* array_##type##_##N##_at( \
    array_##type##_##N* a, usize i) { \
    if (!a || i >= (usize)(N)) return NULL; \
    return &a->items[i]; \
} \
\
/** @brief Returns pointer to first element. Never NULL if a != NULL. O(1) */ \
static inline type* array_##type##_##N##_first(array_##type##_##N* a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_first: a cannot be NULL"); \
    return &a->items[0]; \
} \
\
/** @brief Returns pointer to last element. Never NULL if a != NULL. O(1) */ \
static inline type* array_##type##_##N##_last(array_##type##_##N* a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_last: a cannot be NULL"); \
    return &a->items[(usize)(N) - 1]; \
} \
\
/** @brief Returns first element as option_##type. O(1) */ \
static inline option_##type array_##type##_##N##_first_option( \
    const array_##type##_##N* a) { \
    if (!a) return option_##type##_none(); \
    return option_##type##_some(a->items[0]); \
} \
\
/** @brief Returns last element as option_##type. O(1) */ \
static inline option_##type array_##type##_##N##_last_option( \
    const array_##type##_##N* a) { \
    if (!a) return option_##type##_none(); \
    return option_##type##_some(a->items[(usize)(N) - 1]); \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Mutation \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Sets every element to val \
 * \
 * @param a   Array to fill (NULL-safe) \
 * @param val Value to assign to every element \
 * \
 * Performance: O(N) \
 */ \
static inline void array_##type##_##N##_fill_all( \
    array_##type##_##N* a, type val) { \
    if (!a) return; \
    for (usize _i = 0; _i < (usize)(N); _i++) { \
        a->items[_i] = val; \
    } \
} \
\
/** \
 * @brief Copies all N elements from src into dst \
 * \
 * @param dst Destination array (must not be NULL) \
 * @param src Source array (must not be NULL) \
 * \
 * @pre dst != NULL \
 * @pre src != NULL \
 * \
 * Performance: O(N) — memcpy \
 */ \
static inline void array_##type##_##N##_copy_from( \
    array_##type##_##N* dst, const array_##type##_##N* src) { \
    require_msg(dst != NULL, "array_" #type "_" #N "_copy_from: dst cannot be NULL"); \
    require_msg(src != NULL, "array_" #type "_" #N "_copy_from: src cannot be NULL"); \
    memcpy(dst->items, src->items, sizeof(dst->items)); \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Comparison \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Returns true if all N elements of a and b are byte-equal \
 * \
 * Uses memcmp — suitable for types with no padding and no pointer members. \
 * For types with padding or pointer semantics, compare element-by-element \
 * using the appropriate comparator. \
 * \
 * @param a First array (NULL returns false) \
 * @param b Second array (NULL returns false) \
 * @return true if all N elements compare equal byte-for-byte \
 * \
 * Performance: O(N) \
 */ \
static inline bool array_##type##_##N##_equal( \
    const array_##type##_##N* a, const array_##type##_##N* b) { \
    if (!a || !b) return false; \
    if (a == b)  return true; \
    return memcmp(a->items, b->items, sizeof(a->items)) == 0; \
} \
\
/* ════════════════════════════════════════════════════════════════════════════ \
   Views — zero-copy non-owning access \
   ════════════════════════════════════════════════════════════════════════════ */ \
\
/** \
 * @brief Returns a typed slice_##type view over all N elements \
 * \
 * @pre DEFINE_SLICE(type) must have been called \
 * \
 * @param a Array to view (must not be NULL) \
 * @return slice_##type — non-owning, valid while a is alive \
 * \
 * Performance: O(1) \
 */ \
static inline slice_##type array_##type##_##N##_as_slice( \
    array_##type##_##N* a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_as_slice: a cannot be NULL"); \
    return slice_##type##_from(a->items, (usize)(N)); \
} \
\
/** \
 * @brief Returns a const typed slice view over all N elements \
 * \
 * Performance: O(1) \
 */ \
static inline slice_##type array_##type##_##N##_as_slice_const( \
    const array_##type##_##N* a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_as_slice_const: a cannot be NULL"); \
    return slice_##type##_from((type*)a->items, (usize)(N)); \
} \
\
/** \
 * @brief Returns a mutable bytes_t view over all N * sizeof(type) bytes \
 * \
 * @param a Array to view (must not be NULL) \
 * @return bytes_t — non-owning byte view, valid while a is alive \
 * \
 * Performance: O(1) \
 */ \
static inline bytes_t array_##type##_##N##_as_bytes(array_##type##_##N* a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_as_bytes: a cannot be NULL"); \
    return bytes_from(a->items, (usize)(N) * sizeof(type)); \
} \
\
/** \
 * @brief Returns a read-only cbytes_t view over all N * sizeof(type) bytes \
 * \
 * Performance: O(1) \
 */ \
static inline cbytes_t array_##type##_##N##_as_cbytes( \
    const array_##type##_##N* a) { \
    require_msg(a != NULL, "array_" #type "_" #N "_as_cbytes: a cannot be NULL"); \
    return cbytes_from(a->items, (usize)(N) * sizeof(type)); \
}

/* ════════════════════════════════════════════════════════════════════════════
   ARRAY_FOR — range-based iteration over an array
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ARRAY_FOR(type, N, arr_ptr, idx_var)
 * @brief Iterates over all elements of an array by index
 *
 * Declares a usize loop variable `idx_var` running from 0 to N-1.
 * Access elements via arr_ptr->items[idx_var] or array_##type##_##N##_at().
 *
 * @param type    Element type
 * @param N       Array capacity
 * @param arr_ptr Pointer to array_##type##_##N
 * @param idx_var Name of the index variable to declare
 *
 * Example:
 * ```c
 * DEFINE_ARRAY(int, 4)
 * array_int_4 a = array_int_4_fill(0);
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
 * @brief Iterates over all elements of an array by element pointer
 *
 * Declares a `type*` loop variable `elem_ptr` pointing to each element.
 * Suitable for in-place mutation without index arithmetic.
 *
 * @param type     Element type
 * @param N        Array capacity
 * @param arr_ptr  Pointer to array_##type##_##N
 * @param elem_ptr Name of the element pointer variable to declare
 *
 * Example:
 * ```c
 * ARRAY_FOR_PTR(int, 4, &a, p) {
 *     *p *= 2;
 * }
 * ```
 */
#define ARRAY_FOR_PTR(type, N, arr_ptr, elem_ptr) \
    for (type* elem_ptr = ((arr_ptr) ? &(arr_ptr)->items[0] : NULL); \
         elem_ptr && (usize)(elem_ptr - &(arr_ptr)->items[0]) < (usize)(N); \
         elem_ptr++)

#endif /* CANON_DATA_ARRAY_H */
