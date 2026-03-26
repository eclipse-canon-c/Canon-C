/**
 * @file hashmap_impl.h
 * @brief Pure implementation logic for Canon-C hashmap (Robin Hood open addressing)
 *
 * This file contains the raw struct definition and all function bodies.
 * It has zero naming assumptions — every identifier comes from hashmap_mangle.h.
 * Do NOT include this file directly. Include hashmap.h (header-only) or use
 * hashmap_decl.h + hashmap_defn.h for separate compilation.
 *
 * Algorithm: Robin Hood hashing with linear probing
 * ────────────────────────────────────────────────────────────────────────────
 * Robin Hood hashing is open addressing with a fairness rule: during insertion,
 * if the new element has traveled farther from its home slot than the element
 * currently occupying a slot (i.e., the newcomer is "poorer"), they swap.
 * The displaced element then continues probing. This keeps probe sequence
 * lengths (PSL) short and variance low, making lookups fast and predictable.
 *
 * Key properties:
 * - No per-element allocation — all data lives in the caller-owned flat buffer
 * - Capacity must be a power of 2 — bitmask replaces modulo
 * - Load factor capped at 75% — enforced at insert time (ERR_CAPACITY_EXCEEDED)
 * - Cached hash in each slot — avoids rehashing during Robin Hood swaps
 * - Tombstone-free deletion — backward shift deletion restores contiguity
 *
 * Note on result__Bool_Error:
 * ────────────────────────────────────────────────────────────────────────────
 * CANON_RESULT(bool, Error) generates result__Bool_Error (not result_bool_Error)
 * because bool expands to _Bool before ## token-pasting in C99. All function
 * signatures and call sites use the correct generated name result__Bool_Error.
 *
 * @sa hashmap.h         — user-facing entry point (include this)
 * @sa hashmap_mangle.h  — name customization
 * @sa hashmap_decl.h    — forward declarations for separate compilation
 * @sa hashmap_defn.h    — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef HASHMAP_LINKAGE
    #error "hashmap_impl.h must not be included directly. Include hashmap.h instead."
#endif

#include "hashmap_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/bits.h"
#include "core/primitives/checked.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/option/option.h"
#include "semantics/result/result.h"
#include "semantics/error.h"

#include <string.h>  /* memset */

/* ============================================================================
 * Required user definitions (must be #defined before including hashmap_impl.h)
 * ========================================================================= */
#ifndef HASHMAP_KEY_TYPE
    #error "hashmap_impl.h: define HASHMAP_KEY_TYPE before including"
#endif
#ifndef HASHMAP_VAL_TYPE
    #error "hashmap_impl.h: define HASHMAP_VAL_TYPE before including"
#endif
#ifndef HASHMAP_HASH_FN
    #error "hashmap_impl.h: define HASHMAP_HASH_FN (u64 fn(const K*, void*)) before including"
#endif
#ifndef HASHMAP_EQ_FN
    #error "hashmap_impl.h: define HASHMAP_EQ_FN (bool fn(const K*, const K*, void*)) before including"
#endif

/* ============================================================================
 * Type aliases for readability inside impl
 * ========================================================================= */
typedef HASHMAP_KEY_TYPE hm_key_t;
typedef HASHMAP_VAL_TYPE hm_val_t;

/* ============================================================================
 * Result and Option instantiations needed by the API
 *
 * CANON_RESULT(bool, Error) generates result__Bool_Error (not result_bool_Error)
 * because bool expands to _Bool before ## in C99.
 * ========================================================================= */

CANON_OPTION(hm_val_t)
CANON_RESULT(bool, Error)
CANON_RESULT(hm_val_t, Error)

/* ============================================================================
 * Slot struct
 * ========================================================================= */

typedef struct {
    u64       hash;     /**< Cached full hash (non-zero for occupied slots) */
    u32       psl;      /**< Probe sequence length (distance from home slot) */
    bool      occupied; /**< Explicit occupancy flag */
    hm_key_t  key;     /**< Stored key */
    hm_val_t  value;   /**< Stored value */
} HASHMAP_SLOT_NAME;

/* ============================================================================
 * Hashmap struct
 * ========================================================================= */

typedef struct {
    HASHMAP_SLOT_NAME* slots;    /**< Flat slot array (points into caller buffer) */
    usize              capacity; /**< Total slot count (power of 2) */
    usize              len;      /**< Number of occupied slots */
    void*              ctx;      /**< Optional caller context forwarded to hash/eq fns */
} HASHMAP_TYPE_NAME;

/* ============================================================================
 * Internal helpers (not part of public API)
 * ========================================================================= */

static inline usize _hm_home(u64 hash, usize capacity) {
    return (usize)(hash & (u64)(capacity - 1));
}

static inline usize _hm_wrap(usize index, usize capacity) {
    return index & (capacity - 1);
}

static inline u64 _hm_normalize_hash(u64 h) {
    return h == 0 ? 1 : h;
}

/* ============================================================================
 * hashmap_buffer_size
 * ========================================================================= */

HASHMAP_LINKAGE usize _HM_BUFFER_SIZE(usize capacity) {
    usize total = 0;
    if (!checked_mul(capacity, sizeof(HASHMAP_SLOT_NAME), &total)) return 0;
    return total;
}

/* ============================================================================
 * hashmap_init
 * ========================================================================= */

HASHMAP_LINKAGE result__Bool_Error _HM_INIT(
    borrowed(HASHMAP_TYPE_NAME*) map,
    bytes_t                      buf,
    usize                        capacity,
    void*                        ctx
) {
    if (!map)           return result__Bool_Error_err(ERR_INVALID_ARG);
    if (!buf.ptr)       return result__Bool_Error_err(ERR_INVALID_ARG);
    if (capacity == 0)  return result__Bool_Error_err(ERR_INVALID_ARG);

    if (!bits_is_power_of_two((u64)capacity))
        return result__Bool_Error_err(ERR_INVALID_ARG);

    usize required = _HM_BUFFER_SIZE(capacity);
    if (required == 0 || buf.len < required)
        return result__Bool_Error_err(ERR_BUFFER_TOO_SMALL);

    map->slots    = (HASHMAP_SLOT_NAME*)buf.ptr;
    map->capacity = capacity;
    map->len      = 0;
    map->ctx      = ctx;

    memset(map->slots, 0, required);

    return result__Bool_Error_ok(true);
}

/* ============================================================================
 * hashmap_clear
 * ========================================================================= */

HASHMAP_LINKAGE void _HM_CLEAR(borrowed(HASHMAP_TYPE_NAME*) map) {
    require_msg(map != NULL, "hashmap_clear: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_clear: map is uninitialized");
    memset(map->slots, 0, map->capacity * sizeof(HASHMAP_SLOT_NAME));
    map->len = 0;
}

/* ============================================================================
 * State queries
 * ========================================================================= */

HASHMAP_LINKAGE usize _HM_LEN(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_len: map cannot be NULL");
    return map->len;
}

HASHMAP_LINKAGE usize _HM_CAPACITY(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_capacity: map cannot be NULL");
    return map->capacity;
}

HASHMAP_LINKAGE bool _HM_IS_EMPTY(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_is_empty: map cannot be NULL");
    return map->len == 0;
}

HASHMAP_LINKAGE f64 _HM_LOAD_FACTOR(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_load_factor: map cannot be NULL");
    if (map->capacity == 0) return 0.0;
    return (f64)map->len / (f64)map->capacity;
}

/* ============================================================================
 * hashmap_insert — Robin Hood insertion
 * ========================================================================= */

HASHMAP_LINKAGE result__Bool_Error _HM_INSERT(
    borrowed(HASHMAP_TYPE_NAME*)   map,
    const hm_key_t*                key,
    const hm_val_t*                val
) {
    if (!map)  return result__Bool_Error_err(ERR_INVALID_ARG);
    if (!key)  return result__Bool_Error_err(ERR_INVALID_ARG);
    if (!val)  return result__Bool_Error_err(ERR_INVALID_ARG);
    require_msg(map->slots != NULL, "hashmap_insert: map is uninitialized");

    /* Enforce 75% load cap */
    if (map->len >= map->capacity - (map->capacity / 4))
        return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED);

    u64   h         = _hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize home      = _hm_home(h, map->capacity);
    usize probe_idx = home;
    u32   psl       = 0;

    HASHMAP_SLOT_NAME incoming;
    incoming.hash     = h;
    incoming.psl      = 0;
    incoming.occupied = true;
    incoming.key      = *key;
    incoming.value    = *val;

    bool is_new_key = true;

    for (usize i = 0; i < map->capacity; i++) {
        HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied) {
            /* Empty slot — place incoming here */
            *slot = incoming;
            if (is_new_key) map->len++;
            return result__Bool_Error_ok(is_new_key);
        }

        /* Key already present — update value */
        if (slot->hash == incoming.hash &&
            HASHMAP_EQ_FN(&slot->key, &incoming.key, map->ctx))
        {
            slot->value = incoming.value;
            return result__Bool_Error_ok(false);
        }

        /*
         * Robin Hood: steal from the rich (low PSL).
         */
        if (slot->psl < psl) {
            HASHMAP_SLOT_NAME tmp = *slot;
            *slot    = incoming;
            incoming = tmp;
        }

        psl++;
        incoming.psl = psl;
        probe_idx = _hm_wrap(probe_idx + 1, map->capacity);
    }

    /* Should never reach here if load factor is respected */
    return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED);
}

/* ============================================================================
 * hashmap_get — lookup by key, returns Option<VAL>
 * ========================================================================= */

HASHMAP_LINKAGE option_hm_val_t _HM_GET(
    const HASHMAP_TYPE_NAME* map,
    const hm_key_t*          key
) {
    require_msg(map != NULL,        "hashmap_get: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_get: map is uninitialized");
    require_msg(key != NULL,        "hashmap_get: key cannot be NULL");

    u64   h         = _hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize probe_idx = _hm_home(h, map->capacity);
    u32   psl       = 0;

    for (usize i = 0; i < map->capacity; i++) {
        const HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied) return option_hm_val_t_none();
        if (slot->psl < psl) return option_hm_val_t_none();

        if (slot->hash == h && HASHMAP_EQ_FN(&slot->key, key, map->ctx))
            return option_hm_val_t_some(slot->value);

        psl++;
        probe_idx = _hm_wrap(probe_idx + 1, map->capacity);
    }

    return option_hm_val_t_none();
}

/* ============================================================================
 * hashmap_get_or_null
 * ========================================================================= */

HASHMAP_LINKAGE borrowed(hm_val_t*) _HM_GET_OR_NULL(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_key_t*              key
) {
    require_msg(map != NULL,        "hashmap_get_or_null: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_get_or_null: map is uninitialized");
    require_msg(key != NULL,        "hashmap_get_or_null: key cannot be NULL");

    u64   h         = _hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize probe_idx = _hm_home(h, map->capacity);
    u32   psl       = 0;

    for (usize i = 0; i < map->capacity; i++) {
        HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied)      return NULL;
        if (slot->psl < psl)      return NULL;

        if (slot->hash == h && HASHMAP_EQ_FN(&slot->key, key, map->ctx))
            return &slot->value;

        psl++;
        probe_idx = _hm_wrap(probe_idx + 1, map->capacity);
    }

    return NULL;
}

/* ============================================================================
 * hashmap_contains_key
 * ========================================================================= */

HASHMAP_LINKAGE bool _HM_CONTAINS_KEY(
    const HASHMAP_TYPE_NAME* map,
    const hm_key_t*          key
) {
    require_msg(map != NULL, "hashmap_contains_key: map cannot be NULL");
    require_msg(key != NULL, "hashmap_contains_key: key cannot be NULL");
    return option_hm_val_t_is_some(_HM_GET(map, key));
}

/* ============================================================================
 * hashmap_remove — remove by key, backward shift deletion
 * ========================================================================= */

HASHMAP_LINKAGE result_hm_val_t_Error _HM_REMOVE(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_key_t*              key
) {
    if (!map) return result_hm_val_t_Error_err(ERR_INVALID_ARG);
    if (!key) return result_hm_val_t_Error_err(ERR_INVALID_ARG);
    require_msg(map->slots != NULL, "hashmap_remove: map is uninitialized");

    u64   h         = _hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize probe_idx = _hm_home(h, map->capacity);
    u32   psl       = 0;
    usize found_idx = map->capacity; /* sentinel: not found */

    /* Phase 1: find the slot */
    for (usize i = 0; i < map->capacity; i++) {
        HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied)  break;
        if (slot->psl < psl)  break;

        if (slot->hash == h && HASHMAP_EQ_FN(&slot->key, key, map->ctx)) {
            found_idx = probe_idx;
            break;
        }

        psl++;
        probe_idx = _hm_wrap(probe_idx + 1, map->capacity);
    }

    if (found_idx == map->capacity)
        return result_hm_val_t_Error_err(ERR_NOT_FOUND);

    hm_val_t old_value = map->slots[found_idx].value;

    /* Phase 2: backward shift deletion */
    usize curr = found_idx;
    for (usize i = 0; i < map->capacity - 1; i++) {
        usize next = _hm_wrap(curr + 1, map->capacity);
        HASHMAP_SLOT_NAME* next_slot = &map->slots[next];

        if (!next_slot->occupied || next_slot->psl == 0) {
            map->slots[curr].occupied = false;
            map->slots[curr].hash     = 0;
            map->slots[curr].psl      = 0;
            break;
        }

        map->slots[curr]      = *next_slot;
        map->slots[curr].psl -= 1;
        curr = next;
    }

    map->len--;
    return result_hm_val_t_Error_ok(old_value);
}

/* ============================================================================
 * hashmap_iter_next
 * ========================================================================= */

HASHMAP_LINKAGE bool _HM_ITER_NEXT(
    const HASHMAP_TYPE_NAME* map,
    usize*                   iter,
    const hm_key_t**         key_out,
    const hm_val_t**         val_out
) {
    require_msg(map != NULL,        "hashmap_iter_next: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_iter_next: map is uninitialized");
    require_msg(iter != NULL,       "hashmap_iter_next: iter cannot be NULL");

    while (*iter < map->capacity) {
        const HASHMAP_SLOT_NAME* slot = &map->slots[*iter];
        (*iter)++;

        if (slot->occupied) {
            if (key_out) *key_out = &slot->key;
            if (val_out) *val_out = &slot->value;
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Clean up internal macros
 * ========================================================================= */
#undef hm_key_t
#undef hm_val_t
