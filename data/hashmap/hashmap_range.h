/**
 * @file hashmap_range.h
 * @brief Optional extension: collect hashmap keys or values into a canon_vec
 *
 * Provides helpers that drain a hashmap's contents into caller-owned
 * canon_vec instances, and a HASHMAP_FOR_EACH macro for ergonomic iteration.
 * Useful when you need a snapshot of keys or values for sorting, indexed
 * access, or passing to algo/ functions.
 *
 * This is an OPTIONAL extension. It is NOT included by hashmap.h by default.
 * Include it explicitly only when you need bulk collection operations.
 *
 * Dependencies:
 * ────────────────────────────────────────────────────────────────────────────
 * Requires data/vec/vec.h. The key and value vec types must be instantiated
 * with DEFINE_VEC before including this header:
 *
 *   DEFINE_VEC(static inline, u64)  →  canon_vec_u64
 *   DEFINE_VEC(static inline, int)  →  canon_vec_int
 *
 * Unlike hashmap_fmt.h, this extension does NOT require you to supply push
 * function names manually. They are derived directly from HASHMAP_KEY_TYPE
 * and HASHMAP_VAL_TYPE using the canon_vec naming convention:
 *
 *   push fn = canon_vec_##HASHMAP_KEY_TYPE##_push
 *
 * This works because DEFINE_VEC always generates functions with this exact
 * naming pattern. No HASHMAP_KEY_PUSH_FN or HASHMAP_VAL_PUSH_FN needed.
 *
 * Required user definitions (must be set before including this file):
 * ────────────────────────────────────────────────────────────────────────────
 * These must already be set (they are the same macros used for hashmap.h):
 *   HASHMAP_KEY_TYPE   — e.g. u64
 *   HASHMAP_VAL_TYPE   — e.g. int
 *   HASHMAP_FN(name)   — function prefix (from hashmap_mangle.h)
 *
 * And ensure the corresponding vec types have been instantiated:
 *   DEFINE_VEC(static inline, u64)
 *   DEFINE_VEC(static inline, int)
 *
 * Note on result__Bool_Error:
 * ────────────────────────────────────────────────────────────────────────────
 * collect_keys and collect_values return result__Bool_Error (not
 * result_bool_Error) because CANON_RESULT(bool, Error) token-pastes to
 * result__Bool_Error in C99 — bool expands to _Bool before ## sees it.
 *
 * Usage example:
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "data/vec/vec.h"
 * DEFINE_VEC(static inline, u64)
 * DEFINE_VEC(static inline, int)
 *
 * #define HASHMAP_KEY_TYPE  u64
 * #define HASHMAP_VAL_TYPE  int
 * #define HASHMAP_HASH_FN   hash_u64
 * #define HASHMAP_EQ_FN     eq_u64
 * #include "data/hashmap/hashmap.h"
 * #include "data/hashmap/hashmap_range.h"
 *
 * // Collect all keys into a caller-owned vec
 * u64 key_buf[64];
 * canon_vec_u64 keys = canon_vec_u64_init(key_buf, 64);
 * result__Bool_Error r = hashmap_collect_keys(&my_map, &keys);
 *
 * // Collect all values into a caller-owned vec
 * int val_buf[64];
 * canon_vec_int vals = canon_vec_int_init(val_buf, 64);
 * result__Bool_Error r2 = hashmap_collect_values(&my_map, &vals);
 *
 * // Ergonomic iteration macro (no vec needed)
 * const u64* k;
 * const int* v;
 * HASHMAP_FOR_EACH(&my_map, k, v) {
 *     printf("%llu => %d\n", (unsigned long long)*k, *v);
 * }
 * ```
 *
 * @sa hashmap.h      — main entry point, must be included before this
 * @sa data/vec/vec.h — required dependency (DEFINE_VEC)
 */

#ifndef CANON_DATA_HASHMAP_RANGE_H
#define CANON_DATA_HASHMAP_RANGE_H

#include "hashmap_mangle.h"
#include "data/vec/vec.h"
#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"
#include "semantics/result/result.h"
#include "semantics/error.h"

/* ============================================================================
 * Validate required definitions
 * ========================================================================= */
#ifndef HASHMAP_KEY_TYPE
    #error "hashmap_range.h: HASHMAP_KEY_TYPE must be defined (same as for hashmap.h)"
#endif
#ifndef HASHMAP_VAL_TYPE
    #error "hashmap_range.h: HASHMAP_VAL_TYPE must be defined (same as for hashmap.h)"
#endif

/* ============================================================================
 * Internal: derive canon_vec push function names from key/value types
 * ========================================================================= */

/** @brief Expands to the canon_vec type for a given element type */
#define _HM_RANGE_VEC(type)  canon_vec_##type

/** @brief Calls the canon_vec push function for a given element type */
#define _HM_RANGE_PUSH(type, vec_ptr, elem_ptr) \
    canon_vec_##type##_push((vec_ptr), (elem_ptr))

/* ============================================================================
 * hashmap_collect_keys
 * ========================================================================= */

/**
 * @brief Collects all keys from the hashmap into a caller-owned canon_vec
 *
 * Iterates all occupied slots and pushes each key into *out using
 * canon_vec_##HASHMAP_KEY_TYPE##_push(). The vec must have sufficient
 * capacity — if it fills before all keys are pushed, returns
 * Err(ERR_CAPACITY_EXCEEDED). Keys are appended to whatever is already
 * in the vec; it is NOT cleared before insertion.
 *
 * @param map  Pointer to initialized hashmap (must not be NULL)
 * @param out  Caller-owned canon_vec for keys to append into (must not be NULL)
 * @return     result__Bool_Error — Ok(true) on success,
 *             Err(ERR_CAPACITY_EXCEEDED) if vec fills before all keys are pushed,
 *             Err(ERR_INVALID_ARG) if any pointer is NULL
 *
 * Performance:
 * - Time:  O(capacity) — iterates all slots, pushes only occupied ones
 * - Space: O(1) — no allocation; output goes into caller's vec buffer
 */
static inline result__Bool_Error HASHMAP_FN(collect_keys)(
    const HASHMAP_TYPE_NAME*                   map,
    borrowed(_HM_RANGE_VEC(HASHMAP_KEY_TYPE)*) out
) {
    if (!map) return result__Bool_Error_err(ERR_INVALID_ARG);
    if (!out) return result__Bool_Error_err(ERR_INVALID_ARG);
    require_msg(map->slots != NULL, "hashmap_collect_keys: map is uninitialized");

    usize iter = 0;
    const HASHMAP_KEY_TYPE* k;
    const HASHMAP_VAL_TYPE* v;

    while (HASHMAP_FN(iter_next)(map, &iter, &k, &v)) {
        (void)v; /* only collecting keys */
        result__Bool_Error push_res = _HM_RANGE_PUSH(HASHMAP_KEY_TYPE, out, k);
        if (result__Bool_Error_is_err(push_res))
            return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED);
    }

    return result__Bool_Error_ok(true);
}

/* ============================================================================
 * hashmap_collect_values
 * ========================================================================= */

/**
 * @brief Collects all values from the hashmap into a caller-owned canon_vec
 *
 * Iterates all occupied slots and appends each value into *out using
 * canon_vec_##HASHMAP_VAL_TYPE##_push(). Value ordering corresponds to
 * the order returned by iter_next (unspecified). The vec is NOT cleared
 * before insertion.
 *
 * @param map  Pointer to initialized hashmap (must not be NULL)
 * @param out  Caller-owned canon_vec for values to append into (must not be NULL)
 * @return     result__Bool_Error — Ok(true) on success,
 *             Err(ERR_CAPACITY_EXCEEDED) if vec fills before all values are pushed,
 *             Err(ERR_INVALID_ARG) if any pointer is NULL
 *
 * Performance:
 * - Time:  O(capacity)
 * - Space: O(1)
 */
static inline result__Bool_Error HASHMAP_FN(collect_values)(
    const HASHMAP_TYPE_NAME*                   map,
    borrowed(_HM_RANGE_VEC(HASHMAP_VAL_TYPE)*) out
) {
    if (!map) return result__Bool_Error_err(ERR_INVALID_ARG);
    if (!out) return result__Bool_Error_err(ERR_INVALID_ARG);
    require_msg(map->slots != NULL, "hashmap_collect_values: map is uninitialized");

    usize iter = 0;
    const HASHMAP_KEY_TYPE* k;
    const HASHMAP_VAL_TYPE* v;

    while (HASHMAP_FN(iter_next)(map, &iter, &k, &v)) {
        (void)k; /* only collecting values */
        result__Bool_Error push_res = _HM_RANGE_PUSH(HASHMAP_VAL_TYPE, out, v);
        if (result__Bool_Error_is_err(push_res))
            return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED);
    }

    return result__Bool_Error_ok(true);
}

/* ============================================================================
 * HASHMAP_FOR_EACH — ergonomic iteration macro
 * ========================================================================= */

/**
 * @def HASHMAP_FOR_EACH
 * @brief Clean for-loop syntax for iterating all key-value pairs
 *
 * Expands to a for-loop that visits every occupied slot. Inside the loop body,
 * key_var and val_var are const pointers to the current slot's key and value.
 * No vec, no allocation — direct forward iteration over the map.
 *
 * The map must not be mutated during iteration.
 *
 * @param map_ptr   Pointer to initialized hashmap
 * @param key_var   Declared variable of type (const HASHMAP_KEY_TYPE*)
 * @param val_var   Declared variable of type (const HASHMAP_VAL_TYPE*)
 *
 * @note key_var and val_var must be declared before the macro invocation
 * @note break and continue work normally
 * @note Uses __LINE__ to generate unique iterator variable names,
 *       allowing nested HASHMAP_FOR_EACH loops without name collisions
 */
#define _HM_ITER_VAR_(line)  _hm_iter_##line
#define _HM_ITER_VAR(line)   _HM_ITER_VAR_(line)

#define HASHMAP_FOR_EACH(map_ptr, key_var, val_var)           \
    for (usize _HM_ITER_VAR(__LINE__) = 0;                    \
         HASHMAP_FN(iter_next)(                                \
             (map_ptr),                                        \
             &_HM_ITER_VAR(__LINE__),                         \
             (const HASHMAP_KEY_TYPE**)&(key_var),            \
             (const HASHMAP_VAL_TYPE**)&(val_var)             \
         ); )

/* ============================================================================
 * Clean up internal macros
 * ========================================================================= */
#undef _HM_RANGE_VEC
#undef _HM_RANGE_PUSH

#endif /* CANON_DATA_HASHMAP_RANGE_H */
