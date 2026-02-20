/**
 * @file hashmap_range.h
 * @brief Optional extension: collect hashmap keys or values into a vec
 *
 * Provides helpers that drain a hashmap's contents into caller-owned vecs
 * using the range/vec pattern from data/. Useful when you need a sorted or
 * indexed snapshot of the keys or values for further processing.
 *
 * This is an OPTIONAL extension. It is NOT included by hashmap.h by default.
 * Include it explicitly only when you need bulk collection operations.
 *
 * Dependencies:
 * ────────────────────────────────────────────────────────────────────────────
 * Requires data/vec/vec.h. The key and value vec types must be instantiated
 * with DEFINE_VEC (or the appropriate Canon-C vec macro) for the key and
 * value types before including this header.
 *
 * Required user definitions:
 * ────────────────────────────────────────────────────────────────────────────
 * Before including this file, define:
 *
 *   HASHMAP_KEY_VEC_TYPE  — the vec type for keys (e.g. vec_u64)
 *   HASHMAP_VAL_VEC_TYPE  — the vec type for values (e.g. vec_int)
 *
 * And ensure the corresponding vec types have been instantiated:
 *   DEFINE_VEC(u64)   →  vec_u64
 *   DEFINE_VEC(int)   →  vec_int
 *
 * Usage example:
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "data/vec/vec.h"
 * DEFINE_VEC(u64)
 * DEFINE_VEC(int)
 *
 * #define HASHMAP_KEY_TYPE     u64
 * #define HASHMAP_VAL_TYPE     int
 * #define HASHMAP_HASH_FN      hash_u64
 * #define HASHMAP_EQ_FN        eq_u64
 * #define HASHMAP_KEY_VEC_TYPE vec_u64
 * #define HASHMAP_VAL_VEC_TYPE vec_int
 * #include "data/hashmap/hashmap.h"
 * #include "data/hashmap/hashmap_range.h"
 *
 * // Collect all keys into a caller-owned vec
 * u64 key_buf[64];
 * vec_u64 keys = vec_u64_from(key_buf, 64);
 * result_bool_Error r = hashmap_collect_keys(&my_map, &keys);
 *
 * // Collect all values into a caller-owned vec
 * int val_buf[64];
 * vec_int vals = vec_int_from(val_buf, 64);
 * result_bool_Error r2 = hashmap_collect_values(&my_map, &vals);
 * ```
 *
 * @sa hashmap.h      — main entry point
 * @sa data/vec/vec.h — required dependency
 * @sa data/range.h   — integer range iteration (conceptual model)
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
 * Required user-defined vec types
 * ========================================================================= */
#ifndef HASHMAP_KEY_VEC_TYPE
    #error "hashmap_range.h: define HASHMAP_KEY_VEC_TYPE (e.g. vec_u64) before including"
#endif
#ifndef HASHMAP_VAL_VEC_TYPE
    #error "hashmap_range.h: define HASHMAP_VAL_VEC_TYPE (e.g. vec_int) before including"
#endif

/* ============================================================================
 * hashmap_collect_keys — copy all keys into a caller-owned vec
 * ========================================================================= */

/**
 * @brief Collects all keys from the hashmap into a caller-owned vec
 *
 * Iterates all occupied slots and pushes each key into *out.
 * The vec must have sufficient capacity — if the vec fills before all keys
 * are collected, returns Err(ERR_CAPACITY_EXCEEDED). The vec is NOT cleared
 * before insertion; keys are appended to whatever is already in it.
 *
 * Key ordering is unspecified (insertion order is not preserved by open
 * addressing). Sort the resulting vec if ordered output is required.
 *
 * @param map  Pointer to initialized hashmap
 * @param out  Caller-owned vec to append keys into (must not be NULL)
 * @return     result_bool_Error — Ok(true) on success,
 *             Err(ERR_CAPACITY_EXCEEDED) if vec is too small,
 *             Err(ERR_INVALID_ARG) if any pointer is NULL
 *
 * Performance:
 * - Time:  O(capacity) — iterates all slots
 * - Space: O(1) — no allocation; output goes into caller's vec
 */
static inline result_bool_Error HASHMAP_FN(collect_keys)(
    const HASHMAP_TYPE_NAME* map,
    borrowed(HASHMAP_KEY_VEC_TYPE*) out
) {
    if (!map) return RESULT_ERR(bool, ERR_INVALID_ARG);
    if (!out) return RESULT_ERR(bool, ERR_INVALID_ARG);
    require_msg(map->slots != NULL, "hashmap_collect_keys: map is uninitialized");

    usize iter = 0;
    const HASHMAP_KEY_TYPE* k;
    const HASHMAP_VAL_TYPE* v;

    while (HASHMAP_FN(iter_next)(map, &iter, &k, &v)) {
        /*
         * vec_push returns a result. We use the boolean pattern:
         * if it fails, the vec is full.
         *
         * Note: the exact push function name depends on the vec instantiation.
         * Canon-C vec uses: vec_TYPE_push(vec*, const TYPE*) → result_bool_Error
         * We call via the vec type's known API surface.
         */
        (void)v; /* suppress unused-variable warning — we only want keys */

        /* Attempt push; vec_T_push returns result_bool_Error */
        /* We use a generic approach: memcpy-style push via vec internal API */
        /* 
         * Because we cannot know the exact push fn name generically,
         * the user must ensure HASHMAP_KEY_VEC_TYPE has a compatible push.
         * The pattern below works for any Canon-C vec instantiated with DEFINE_VEC.
         *
         * HASHMAP_KEY_VEC_TYPE is e.g. vec_u64, so the push fn is vec_u64_push.
         * We delegate via the macro-generated name pattern.
         */
        #define _HM_KEY_PUSH HASHMAP_FN_CONCAT(HASHMAP_KEY_VEC_TYPE, _push)

        /* 
         * Fallback: use a helper that the caller must provide if the above
         * does not resolve cleanly. Document this as a known limitation.
         *
         * In practice, callers using this extension should call:
         *   while (hashmap_iter_next(&map, &iter, &k, &v))
         *       vec_u64_push(&keys, k);
         * directly. This extension provides a convenience wrapper.
         */

        /*
         * IMPLEMENTATION NOTE:
         * C99 macros cannot generically compose two macro values into a function
         * name without token pasting. The user must supply HASHMAP_KEY_PUSH_FN
         * to make this fully generic. If not supplied, we emit a compile error.
         */
        #ifndef HASHMAP_KEY_PUSH_FN
            #error "hashmap_range.h: define HASHMAP_KEY_PUSH_FN (result_bool_Error fn(VecType*, const K*)) before including"
        #endif

        result_bool_Error push_res = HASHMAP_KEY_PUSH_FN(out, k);
        if (result_bool_Error_is_err(push_res))
            return RESULT_ERR(bool, ERR_CAPACITY_EXCEEDED);
    }

    return RESULT_OK(bool, true);
}

/* ============================================================================
 * hashmap_collect_values — copy all values into a caller-owned vec
 * ========================================================================= */

/**
 * @brief Collects all values from the hashmap into a caller-owned vec
 *
 * Iterates all occupied slots and appends each value into *out.
 * Value ordering corresponds to the order returned by iter_next (unspecified).
 * The vec is NOT cleared before insertion.
 *
 * @param map  Pointer to initialized hashmap
 * @param out  Caller-owned vec to append values into (must not be NULL)
 * @return     result_bool_Error — Ok(true) on success,
 *             Err(ERR_CAPACITY_EXCEEDED) if vec is too small,
 *             Err(ERR_INVALID_ARG) if any pointer is NULL
 *
 * Performance:
 * - Time:  O(capacity)
 * - Space: O(1)
 */
static inline result_bool_Error HASHMAP_FN(collect_values)(
    const HASHMAP_TYPE_NAME*         map,
    borrowed(HASHMAP_VAL_VEC_TYPE*) out
) {
    if (!map) return RESULT_ERR(bool, ERR_INVALID_ARG);
    if (!out) return RESULT_ERR(bool, ERR_INVALID_ARG);
    require_msg(map->slots != NULL, "hashmap_collect_values: map is uninitialized");

    #ifndef HASHMAP_VAL_PUSH_FN
        #error "hashmap_range.h: define HASHMAP_VAL_PUSH_FN (result_bool_Error fn(VecType*, const V*)) before including"
    #endif

    usize iter = 0;
    const HASHMAP_KEY_TYPE* k;
    const HASHMAP_VAL_TYPE* v;

    while (HASHMAP_FN(iter_next)(map, &iter, &k, &v)) {
        (void)k;
        result_bool_Error push_res = HASHMAP_VAL_PUSH_FN(out, v);
        if (result_bool_Error_is_err(push_res))
            return RESULT_ERR(bool, ERR_CAPACITY_EXCEEDED);
    }

    return RESULT_OK(bool, true);
}

/* ============================================================================
 * hashmap_iter_as_range — structured key–value iteration helper
 * ========================================================================= */

/**
 * @brief Convenience macro for iterating all key–value pairs
 *
 * Expands to a for-loop that visits every occupied slot. Inside the loop body,
 * `_key` and `_val` are const pointers to the current slot's key and value.
 *
 * The map must not be mutated during iteration.
 *
 * @param map_ptr   Pointer to initialized hashmap
 * @param key_var   Name for the const KEY* loop variable
 * @param val_var   Name for the const VAL* loop variable
 *
 * Usage:
 * ```c
 * HASHMAP_FOR_EACH(&my_map, k, v) {
 *     printf("key=%llu\n", *k);
 * }
 * ```
 */
#define HASHMAP_FOR_EACH(map_ptr, key_var, val_var) \
    for (usize _hm_iter_ = 0; \
         HASHMAP_FN(iter_next)( \
             (map_ptr), &_hm_iter_, \
             (const HASHMAP_KEY_TYPE**)&(key_var), \
             (const HASHMAP_VAL_TYPE**)&(val_var) \
         ); )

#endif /* CANON_DATA_HASHMAP_RANGE_H */
