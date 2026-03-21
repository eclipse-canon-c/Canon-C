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
 * Memory layout:
 * ────────────────────────────────────────────────────────────────────────────
 * The caller provides a flat u8 buffer. hashmap_init() partitions it into
 * an array of hashmap_slot structs. The slot array starts at the beginning
 * of the buffer, aligned to alignof(hashmap_slot).
 *
 * slot layout (per element):
 *   - u64  hash     — full cached hash (0 = unoccupied sentinel)
 *   - u32  psl      — probe sequence length (distance from home slot)
 *   - bool occupied — explicit occupancy flag (hash==0 is ambiguous)
 *   - K    key      — caller-defined key type
 *   - V    value    — caller-defined value type
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * hashmap_impl.h includes only:
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - core/primitives/bits.h
 *   - core/primitives/checked.h
 *   - core/slice.h
 *   - core/ownership.h
 *   - semantics/option/option.h
 *   - semantics/result/result.h
 *   - semantics/error.h
 *   - hashmap_mangle.h
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
 *
 * HASHMAP_KEY_TYPE   — key type (e.g. u64, str_t, MyKey)
 * HASHMAP_VAL_TYPE   — value type (e.g. int, void*, Record)
 * HASHMAP_HASH_FN    — hash function: u64 fn(const HASHMAP_KEY_TYPE* key, void* ctx)
 * HASHMAP_EQ_FN      — equality function: bool fn(const K* a, const K* b, void* ctx)
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
 *
 * NOTE: No leading underscore — double-underscore names after macro expansion
 * (e.g. option__hm_val_t) are reserved identifiers in C. Use hm_key_t / hm_val_t.
 * These are cleaned up with #undef at the end of this file.
 * ========================================================================= */
typedef HASHMAP_KEY_TYPE hm_key_t;
typedef HASHMAP_VAL_TYPE hm_val_t;

/* ============================================================================
 * Result and Option instantiations needed by the API
 * ========================================================================= */

/*
 * option_hm_val_t       — returned by hashmap_get() for lookups
 * result_bool_Error     — returned by insert
 * result_hm_val_t_Error — returned by remove (returns old value)
 */
CANON_OPTION(hm_val_t)
CANON_RESULT(bool, Error)
CANON_RESULT(hm_val_t, Error)

/* ============================================================================
 * Slot struct
 * ========================================================================= */

/**
 * @brief Internal storage slot for one key–value pair
 *
 * psl == 0 means the element is in its home bucket (optimal).
 * hash == 0 with occupied == false means the slot is empty.
 * hash is cached to avoid re-hashing during Robin Hood probing.
 */
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

/**
 * @brief Generic open-addressed Robin Hood hashmap with caller-owned buffer
 *
 * All state lives in the struct and the flat buffer provided at init time.
 * No allocation is performed by any hashmap operation after init.
 *
 * Capacity is always a power of 2. The hashmap refuses to insert when
 * load factor exceeds 75% (len >= capacity * 3 / 4), returning
 * ERR_CAPACITY_EXCEEDED. The caller must provision sufficient capacity upfront.
 *
 * Invariants:
 * - slots != NULL (after successful init)
 * - capacity is a power of 2
 * - len <= capacity * 3 / 4
 * - Every occupied slot has hash != 0 and occupied == true
 * - psl[i] == distance from slot i to the slot's home bucket
 *
 * Example:
 * ```c
 * #define HASHMAP_KEY_TYPE u64
 * #define HASHMAP_VAL_TYPE int
 * #define HASHMAP_HASH_FN  my_hash_u64
 * #define HASHMAP_EQ_FN    my_eq_u64
 * #include "data/hashmap/hashmap_impl.h"
 *
 * u8 buf[hashmap_buffer_size(64)];
 * hashmap map;
 * hashmap_init(&map, bytes_from(buf, sizeof(buf)), 64, NULL);
 * hashmap_insert(&map, &my_key, &my_val);
 * option_hm_val_t found = hashmap_get(&map, &my_key);
 * ```
 */
typedef struct {
    HASHMAP_SLOT_NAME* slots;    /**< Flat slot array (points into caller buffer) */
    usize              capacity; /**< Total slot count (power of 2) */
    usize              len;      /**< Number of occupied slots */
    void*              ctx;      /**< Optional caller context forwarded to hash/eq fns */
} HASHMAP_TYPE_NAME;

/* ============================================================================
 * Internal helpers (not part of public API)
 * ========================================================================= */

/**
 * @brief Computes the home slot index for a given hash
 *
 * Uses bitmask instead of modulo — valid only when capacity is power of 2.
 */
static inline usize _hm_home(u64 hash, usize capacity) {
    return (usize)(hash & (u64)(capacity - 1));
}

/**
 * @brief Wraps slot index around capacity boundary
 */
static inline usize _hm_wrap(usize index, usize capacity) {
    return index & (capacity - 1);
}

/**
 * @brief Normalizes a raw hash to ensure it is never 0 (empty sentinel)
 */
static inline u64 _hm_normalize_hash(u64 h) {
    return h == 0 ? 1 : h;
}

/* ============================================================================
 * hashmap_buffer_size — utility: compute required buffer byte count
 * ========================================================================= */

/**
 * @brief Returns the minimum buffer size in bytes for a given slot capacity
 *
 * Use this to size the caller-owned buffer before calling hashmap_init().
 * The capacity you pass here should be at least ceil(desired_elements / 0.75).
 * Round up to the next power of 2 using bits_next_power_of_two() if needed.
 *
 * @param capacity Desired slot capacity (must be power of 2)
 * @return Required buffer size in bytes, or 0 on overflow
 *
 * @pre bits_is_power_of_two(capacity)
 *
 * Example:
 * ```c
 * usize cap = bits_next_power_of_two(100 * 4 / 3);  // ~133 → 256
 * u8 buf[hashmap_buffer_size(256)];
 * ```
 */
HASHMAP_LINKAGE usize _HM_BUFFER_SIZE(usize capacity) {
    usize total = 0;
    if (!checked_mul(capacity, sizeof(HASHMAP_SLOT_NAME), &total)) return 0;
    return total;
}

/* ============================================================================
 * hashmap_init — construct a hashmap over a caller-owned buffer
 * ========================================================================= */

/**
 * @brief Initializes a hashmap over a caller-provided flat buffer
 *
 * Partitions the buffer into a slot array and zeroes all slots.
 * No allocation is performed. The buffer must remain valid for the
 * entire lifetime of the hashmap.
 *
 * @param map      Pointer to uninitialized hashmap struct (must not be NULL)
 * @param buf      Caller-owned byte buffer backing the slot array
 * @param capacity Number of slots (must be power of 2)
 * @param ctx      Optional context forwarded to hash/eq functions (may be NULL)
 * @return         result_bool_Error: Ok(true) on success, Err on failure
 *
 * @retval ERR_INVALID_ARG       map is NULL, buf.ptr is NULL, or capacity is 0
 * @retval ERR_INVALID_ARG       capacity is not a power of 2
 * @retval ERR_BUFFER_TOO_SMALL  buf.len < hashmap_buffer_size(capacity)
 *
 * @pre bits_is_power_of_two(capacity)
 * @pre buf.len >= hashmap_buffer_size(capacity)
 *
 * Performance:
 * - Time:  O(capacity) — zeroes all slots
 * - Space: O(1) — no allocation
 */
HASHMAP_LINKAGE result_bool_Error _HM_INIT(
    borrowed(HASHMAP_TYPE_NAME*) map,
    bytes_t                      buf,
    usize                        capacity,
    void*                        ctx
) {
    if (!map)           return RESULT_ERR(bool, ERR_INVALID_ARG);
    if (!buf.ptr)       return RESULT_ERR(bool, ERR_INVALID_ARG);
    if (capacity == 0)  return RESULT_ERR(bool, ERR_INVALID_ARG);

    if (!bits_is_power_of_two((u64)capacity))
        return RESULT_ERR(bool, ERR_INVALID_ARG);

    usize required = _HM_BUFFER_SIZE(capacity);
    if (required == 0 || buf.len < required)
        return RESULT_ERR(bool, ERR_BUFFER_TOO_SMALL);

    map->slots    = (HASHMAP_SLOT_NAME*)buf.ptr;
    map->capacity = capacity;
    map->len      = 0;
    map->ctx      = ctx;

    memset(map->slots, 0, required);

    return RESULT_OK(bool, true);
}

/* ============================================================================
 * hashmap_clear — reset to empty without releasing the buffer
 * ========================================================================= */

/**
 * @brief Removes all entries from the hashmap, keeping the buffer intact
 *
 * After clear, len == 0 and all slots are zeroed. The backing buffer and
 * capacity are unchanged. O(capacity) — iterates all slots to zero them.
 *
 * @param map Pointer to initialized hashmap (must not be NULL)
 *
 * Performance:
 * - Time:  O(capacity)
 * - Space: O(1)
 */
HASHMAP_LINKAGE void _HM_CLEAR(borrowed(HASHMAP_TYPE_NAME*) map) {
    require_msg(map != NULL, "hashmap_clear: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_clear: map is uninitialized");
    memset(map->slots, 0, map->capacity * sizeof(HASHMAP_SLOT_NAME));
    map->len = 0;
}

/* ============================================================================
 * State queries
 * ========================================================================= */

/**
 * @brief Returns the number of key–value pairs in the hashmap
 *
 * Performance: O(1)
 */
HASHMAP_LINKAGE usize _HM_LEN(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_len: map cannot be NULL");
    return map->len;
}

/**
 * @brief Returns the slot capacity of the hashmap
 *
 * This is the total number of slots, not the maximum safe element count.
 * Maximum safe elements ≈ capacity * 3 / 4.
 *
 * Performance: O(1)
 */
HASHMAP_LINKAGE usize _HM_CAPACITY(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_capacity: map cannot be NULL");
    return map->capacity;
}

/**
 * @brief Returns true if the hashmap contains no entries
 *
 * Performance: O(1)
 */
HASHMAP_LINKAGE bool _HM_IS_EMPTY(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_is_empty: map cannot be NULL");
    return map->len == 0;
}

/**
 * @brief Returns the current load factor as a value in [0.0, 1.0]
 *
 * A load factor above 0.75 will cause inserts to fail with ERR_CAPACITY_EXCEEDED.
 *
 * Performance: O(1)
 */
HASHMAP_LINKAGE f64 _HM_LOAD_FACTOR(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_load_factor: map cannot be NULL");
    if (map->capacity == 0) return 0.0;
    return (f64)map->len / (f64)map->capacity;
}

/* ============================================================================
 * hashmap_insert — Robin Hood insertion
 * ========================================================================= */

/**
 * @brief Inserts a key–value pair into the hashmap (Robin Hood probing)
 *
 * If the key already exists, its value is overwritten and Ok(false) is returned.
 * If the key is new, it is inserted and Ok(true) is returned.
 * If the map is at 75% load, no insertion occurs and Err(ERR_CAPACITY_EXCEEDED)
 * is returned — the caller must use a larger buffer.
 *
 * Robin Hood invariant: during probing, if the incoming element has a higher PSL
 * than the element in the current slot, they swap and the probe continues with
 * the displaced element. This bounds worst-case probe lengths.
 *
 * @param map Pointer to initialized hashmap
 * @param key Pointer to key to insert (copied into slot — key need not outlive call)
 * @param val Pointer to value to insert (copied into slot)
 * @return    result_bool_Error — Ok(true) if new key, Ok(false) if key existed,
 *            Err(ERR_CAPACITY_EXCEEDED) if load factor >= 75%,
 *            Err(ERR_INVALID_ARG) if any pointer is NULL
 *
 * Performance:
 * - Time:  O(1) amortized (O(n) worst case, very rare with Robin Hood)
 * - Space: O(1) — no allocation
 */
HASHMAP_LINKAGE result_bool_Error _HM_INSERT(
    borrowed(HASHMAP_TYPE_NAME*)   map,
    const hm_key_t*                key,
    const hm_val_t*                val
) {
    if (!map)  return RESULT_ERR(bool, ERR_INVALID_ARG);
    if (!key)  return RESULT_ERR(bool, ERR_INVALID_ARG);
    if (!val)  return RESULT_ERR(bool, ERR_INVALID_ARG);
    require_msg(map->slots != NULL, "hashmap_insert: map is uninitialized");

    /* Enforce 75% load cap */
    if (map->len >= map->capacity - (map->capacity / 4))
        return RESULT_ERR(bool, ERR_CAPACITY_EXCEEDED);

    u64   h         = _hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize home      = _hm_home(h, map->capacity);
    usize probe_idx = home;
    u32   psl       = 0;

    /* Incoming element to insert (may be displaced by Robin Hood) */
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
            return RESULT_OK(bool, is_new_key);
        }

        /* Key already present — update value */
        if (slot->hash == incoming.hash &&
            HASHMAP_EQ_FN(&slot->key, &incoming.key, map->ctx))
        {
            slot->value = incoming.value;
            return RESULT_OK(bool, false);
        }

        /*
         * Robin Hood: steal from the rich (low PSL).
         *
         * If the resident slot has probed less than we have, swap it out.
         * After the swap, psl continues from its current value — it tracks
         * how far we have traveled in this probe sequence, not the displaced
         * element's old PSL. Resetting psl here would corrupt subsequent
         * Robin Hood comparisons and incoming.psl assignments.
         */
        if (slot->psl < psl) {
            HASHMAP_SLOT_NAME tmp = *slot;
            *slot    = incoming;
            incoming = tmp;
            /* psl intentionally NOT reset — continues from current probe distance */
        }

        psl++;
        incoming.psl = psl;
        probe_idx = _hm_wrap(probe_idx + 1, map->capacity);
    }

    /* Should never reach here if load factor is respected */
    return RESULT_ERR(bool, ERR_CAPACITY_EXCEEDED);
}

/* ============================================================================
 * hashmap_get — lookup by key, returns Option<VAL>
 * ========================================================================= */

/**
 * @brief Looks up a key and returns the associated value as Option
 *
 * Returns Some(value) if the key exists, None if not found.
 * The returned value is a copy — the map retains its own copy.
 *
 * @param map Pointer to initialized hashmap
 * @param key Pointer to key to look up
 * @return    option_hm_val_t — Some(value) or None
 *
 * Performance:
 * - Time:  O(1) expected — terminates as soon as PSL of a slot is less than
 *          the probe distance of the search (Robin Hood early termination)
 * - Space: O(1)
 */
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

        /*
         * Robin Hood early-exit invariant:
         * If the stored slot has a smaller PSL than our probe distance,
         * the key cannot exist — Robin Hood would have displaced it earlier.
         */
        if (slot->psl < psl) return option_hm_val_t_none();

        if (slot->hash == h && HASHMAP_EQ_FN(&slot->key, key, map->ctx))
            return option_hm_val_t_some(slot->value);

        psl++;
        probe_idx = _hm_wrap(probe_idx + 1, map->capacity);
    }

    return option_hm_val_t_none();
}

/* ============================================================================
 * hashmap_get_or_null — lookup returning a pointer into the map
 * ========================================================================= */

/**
 * @brief Looks up a key and returns a direct pointer to the stored value
 *
 * Returns a pointer into the hashmap's internal slot array. The pointer is
 * valid until the next insert or remove operation (which may displace slots).
 * Returns NULL if the key is not found.
 *
 * This avoids the copy cost of hashmap_get() for large value types.
 * Do NOT store this pointer beyond the next mutation of the map.
 *
 * @param map Pointer to initialized hashmap
 * @param key Pointer to key to look up
 * @return    borrowed pointer to stored value, or NULL if not found
 *
 * Performance:
 * - Time:  O(1) expected
 * - Space: O(1)
 */
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

/**
 * @brief Returns true if the hashmap contains the given key
 *
 * @param map Pointer to initialized hashmap
 * @param key Pointer to key to check
 * @return    true if key exists, false otherwise
 *
 * Performance: O(1) expected
 */
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

/**
 * @brief Removes a key from the hashmap and returns its associated value
 *
 * Uses backward shift deletion (no tombstones): after removing a slot,
 * subsequent slots with PSL > 0 are shifted back one position. This
 * maintains the Robin Hood invariant and avoids degraded probe chains.
 *
 * @param map Pointer to initialized hashmap
 * @param key Pointer to key to remove
 * @return    result_hm_val_t_Error — Ok(old_value) if removed,
 *            Err(ERR_NOT_FOUND) if key does not exist,
 *            Err(ERR_INVALID_ARG) if any pointer is NULL
 *
 * Performance:
 * - Time:  O(1) expected (Robin Hood keeps probe chains short)
 * - Space: O(1)
 */
HASHMAP_LINKAGE result_hm_val_t_Error _HM_REMOVE(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_key_t*              key
) {
    if (!map) return RESULT_ERR(hm_val_t, ERR_INVALID_ARG);
    if (!key) return RESULT_ERR(hm_val_t, ERR_INVALID_ARG);
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
        return RESULT_ERR(hm_val_t, ERR_NOT_FOUND);

    hm_val_t old_value = map->slots[found_idx].value;

    /* Phase 2: backward shift deletion */
    usize curr = found_idx;
    for (usize i = 0; i < map->capacity - 1; i++) {
        usize next = _hm_wrap(curr + 1, map->capacity);
        HASHMAP_SLOT_NAME* next_slot = &map->slots[next];

        /* Stop if next slot is empty or is in its home slot (PSL == 0) */
        if (!next_slot->occupied || next_slot->psl == 0) {
            map->slots[curr].occupied = false;
            map->slots[curr].hash     = 0;
            map->slots[curr].psl      = 0;
            break;
        }

        /* Shift next slot into current position, decrement its PSL */
        map->slots[curr]      = *next_slot;
        map->slots[curr].psl -= 1;
        curr = next;
    }

    map->len--;
    return RESULT_OK(hm_val_t, old_value);
}

/* ============================================================================
 * hashmap_iter_next — forward iteration over occupied slots
 * ========================================================================= */

/**
 * @brief Advances an iterator over all occupied slots
 *
 * Iteration state is just a usize index into the slot array. Initialize
 * to 0 before the first call. On each call, advances to the next occupied
 * slot and writes key/value pointers. Returns false when exhausted.
 *
 * The map must not be mutated during iteration. Mutations (insert/remove)
 * may move slots, invalidating the iterator position.
 *
 * Usage:
 * ```c
 * usize iter = 0;
 * const hm_key_t* k;
 * const hm_val_t* v;
 * while (hashmap_iter_next(&map, &iter, &k, &v)) {
 *     printf("key/value pair found\n");
 * }
 * ```
 *
 * @param map      Pointer to initialized hashmap
 * @param iter     Iterator state (usize*), initialize to 0
 * @param key_out  Set to pointer to key in current slot (borrowed)
 * @param val_out  Set to pointer to value in current slot (borrowed)
 * @return         true if a slot was found, false when iteration complete
 *
 * Performance:
 * - Time:  O(1) per call, O(capacity) total to exhaust
 * - Space: O(1)
 */
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
 * Clean up internal macros to avoid polluting downstream includes
 * ========================================================================= */
#undef hm_key_t
#undef hm_val_t
