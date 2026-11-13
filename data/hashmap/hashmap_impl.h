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
 * CANON_RESULT(_Bool, Error) generates result__Bool_Error (not result_bool_Error)
 * in every standard: _Bool is pasted as written. All function
 * signatures and call sites use the correct generated name result__Bool_Error.
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 * ────────────────────────────────────────────────────────────────────────────
 *   - Embeds a lifetime_t lt field on the hashmap struct.
 *   - hashmap_init opens a fresh lifetime. The ID is derived from a per-TU
 *     counter XOR'd with the struct's address (same pattern as vec/deque/PQ).
 *   - Hashmap has NO destructor. The buffer is caller-owned and the struct
 *     is caller-allocated; there is no hook on which to call close. The
 *     lifetime stays open for the life of the struct — use-after-out-of-
 *     scope is caller's responsibility, not the substrate's.
 *   - Restamp on EVERY successful mutating operation:
 *       * hashmap_clear  — all entries gone, prior borrows invalid
 *       * hashmap_insert — Robin Hood may shift adjacent slots even on
 *                          "new key into empty slot", and value updates
 *                          mutate slot contents in place. We restamp on
 *                          every success path conservatively rather than
 *                          try to distinguish "shift happened" from "shift
 *                          did not happen" — the latter requires reasoning
 *                          about the entire probe chain, brittle to express
 *                          and harder to verify. False positives (firing
 *                          when not strictly necessary) are a usability
 *                          cost; false negatives are a correctness bug.
 *       * hashmap_remove — backward shift deletion moves entries downstream;
 *                          prior pointers into the affected region are invalid.
 *     Error paths (NULL args, capacity exceeded, key not found) do NOT
 *     restamp — nothing changed.
 *   - Operations that do NOT restamp: hashmap_get, hashmap_get_or_null,
 *     hashmap_contains_key, hashmap_iter_next, and all queries (len,
 *     capacity, is_empty, load_factor). All read-only.
 *   - Iteration contract: hashmap_iter_next does not restamp, but the
 *     caller must not mutate the map during iteration (same caller-
 *     discipline rule the function already documents). The substrate
 *     does not catch mutation-during-iteration; HASHMAP_FOR_EACH users
 *     must observe the same contract.
 *   Zero cost in release builds — struct layout is identical without
 *   the flag.
 *
 * Multi-instantiation note (CANON_LIFETIME_DEBUG):
 * ────────────────────────────────────────────────────────────────────────────
 * When a single translation unit instantiates multiple hashmap types (via
 * repeated #include with different HASHMAP_TYPE_NAME), each instantiation
 * gets its own type-specific open/restamp helpers via HASHMAP_FN-mangled
 * names. They all share a single TU-wide counter helper, hm_lifetime_next_id_,
 * which is guarded against duplicate definition. Two hashmap types in the
 * same TU therefore produce distinct ids relative to each other — desirable.
 *
 * @sa hashmap.h         — user-facing entry point (include this)
 * @sa hashmap_mangle.h  — name customization
 * @sa hashmap_decl.h    — forward declarations for separate compilation
 * @sa hashmap_defn.h    — definitions for separate compilation
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
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

#ifdef CANON_LIFETIME_DEBUG
    #include <stdint.h>                       /* uintptr_t */
    #include "core/primitives/lifetime.h"     /* region_id_t, lifetime_t */
#endif

#include <stddef.h>  /* offsetof */
#include <stdint.h>  /* uintptr_t */

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
/* VERIFY-054: hm_key_t and hm_val_t are macros, not typedefs. As typedefs
   of a fixed name, a second map in the translation unit redefined them, and
   the option and result types named after hm_val_t, so two maps did not
   compile. The key and value types get per-map aliases, HASHMAP_FN(key_t)
   and HASHMAP_FN(val_t), and the option and result types are named after
   the value's (hashmap_mangle.h). */
typedef HASHMAP_KEY_TYPE HM_KEY_ALIAS_;
#define hm_key_t HM_KEY_ALIAS_
typedef HASHMAP_VAL_TYPE HM_VAL_ALIAS_;
#define hm_val_t HM_VAL_ALIAS_

/* ============================================================================
 * Result and Option instantiations needed by the API
 *
 * CANON_RESULT(_Bool, Error) generates result__Bool_Error (not result_bool_Error)
 * in every standard: _Bool is pasted as written.
 * ========================================================================= */

HM_CANON_OPTION_(HM_VAL_ALIAS_)
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    /* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
    CANON_RESULT(_Bool, Error)
#endif
/* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
HM_CANON_RESULT_(HM_VAL_ALIAS_, Error)

/* ============================================================================
 * Internal: lifetime field macro
 * ========================================================================= */

#ifdef CANON_LIFETIME_DEBUG
    #define HM_LIFETIME_FIELD_ \
        lifetime_t lt; /**< [debug] Lifetime token: id + open */
#else
    #define HM_LIFETIME_FIELD_  /* empty */
#endif

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

/* For the slot's alignment, read by hashmap_init (VERIFY-054): offsetof of a
   member that follows a char is the member type's alignment requirement. */
typedef struct {
    char              c_;
    HASHMAP_SLOT_NAME s_;
} HASHMAP_FN(slot_align_);

/* ============================================================================
 * Hashmap struct
 * ========================================================================= */

typedef struct {
    HASHMAP_SLOT_NAME* slots;    /**< Flat slot array (points into caller buffer) */
    usize              capacity; /**< Total slot count (power of 2) */
    usize              len;      /**< Number of occupied slots */
    void*              ctx;      /**< Optional caller context forwarded to hash/eq fns */
    HM_LIFETIME_FIELD_
} HASHMAP_TYPE_NAME;

/* VERIFY-055: the map's representation invariant, per map (the name is
   mangled, so two maps in one translation unit define two predicates).
   - capacity: at least 1, at most 2^30, so that insert's probe distance,
     a u32, cannot overflow (a contract limit: the code does not refuse a
     larger capacity; the verified unit does not admit one). Since
     VERIFY-056 the loop states the distance at most the step count, so
     below the capacity;
   - len: at most capacity;
   - slots: capacity of them, valid, apart from the map struct;
   - every slot's probe distance is below the capacity, and an occupied
     slot's cached hash is not 0 (insert normalises 0 to 1).
   Not stated, and not claimed: the Robin Hood order (that every key lies at
   its probe distance from its home slot), on which "get finds what insert
   stored" rests. */
/*@ predicate HASHMAP_FN(shape_)(HASHMAP_TYPE_NAME* m) =
      \valid(m) &&
      1 <= m->capacity <= 1073741824 &&
      m->len <= m->capacity &&
      \valid(m->slots + (0 .. m->capacity - 1)) &&
      \separated(m, m->slots + (0 .. m->capacity - 1));
    predicate HASHMAP_FN(dist_)(HASHMAP_TYPE_NAME* m) =
      \forall integer j; 0 <= j < m->capacity ==>
        m->slots[j].psl < m->capacity &&
        (m->slots[j].occupied ==> m->slots[j].hash != 0);
    predicate HASHMAP_FN(inv_)(HASHMAP_TYPE_NAME* m) =
      HASHMAP_FN(shape_)(m) && HASHMAP_FN(dist_)(m);

    // VERIFY-066: shape_ and inv_ for a map read through a pointer to
    // const. \valid(m) asks that the map be writable through m, which in
    // Frama-C's reading of ACSL a pointer to const never is: Eva found every
    // call of the read-only functions invalid. Those functions only read the
    // map, so they require it readable. The other conjuncts are shape_'s,
    // and must stay so.
    predicate HASHMAP_FN(shape_read_)(HASHMAP_TYPE_NAME* m) =
      \valid_read(m) &&
      1 <= m->capacity <= 1073741824 &&
      m->len <= m->capacity &&
      \valid(m->slots + (0 .. m->capacity - 1)) &&
      \separated(m, m->slots + (0 .. m->capacity - 1));
    predicate HASHMAP_FN(inv_read_)(HASHMAP_TYPE_NAME* m) =
      HASHMAP_FN(shape_read_)(m) && HASHMAP_FN(dist_)(m);
*/

/* ============================================================================
 * Internal: TU-wide lifetime counter helper
 * ============================================================================
 * Shared across all hashmap instantiations in the translation unit. Two
 * hashmap types in the same TU draw from the same counter, producing
 * distinct ids relative to each other.
 *
 * Guarded against duplicate definition because hashmap_impl.h may be
 * included multiple times per TU (once per instantiation). Same pattern
 * as CANON_RESULT_BOOL_ERROR_DEFINED in deque_impl.h.
 *
 * No thread-safety guarantee on concurrent construction or mutation —
 * the same constraint that applies to every other Canon-C container.
 *
 * REGION_ID_STATIC (0) is reserved; the counter starts at 1 and the
 * derivation is defensively guarded against producing 0.
 * ========================================================================= */

#ifdef CANON_LIFETIME_DEBUG
#ifndef CANON_HM_LIFETIME_NEXT_ID_DEFINED
#define CANON_HM_LIFETIME_NEXT_ID_DEFINED
    /* Delegates to the single shared generator in
     * core/primitives/lifetime.h — see docs/thread-safety.md for the
     * concurrency contract on token generation. */
    static inline region_id_t hm_lifetime_next_id_(void* mp) {
        return canon_lifetime_next_id_(mp);
    }
#endif
#endif

/* ============================================================================
 * Internal: per-instantiation lifetime open/restamp helpers
 * ============================================================================
 * Mangled via HASHMAP_FN so each hashmap type gets its own helpers (the
 * function parameter type differs per instantiation). The helpers are
 * always emitted; in release builds they are empty no-ops.
 * ========================================================================= */

#ifdef CANON_LIFETIME_DEBUG
    HASHMAP_LINKAGE void HM_LIFETIME_OPEN_(HASHMAP_TYPE_NAME* map) {
        map->lt.id   = hm_lifetime_next_id_(map);
        map->lt.open = true;
    }
    HASHMAP_LINKAGE void HM_LIFETIME_RESTAMP_(HASHMAP_TYPE_NAME* map) {
        map->lt.id = hm_lifetime_next_id_(map);
        /* lt.open stays true — restamp is not destruction */
    }
#else
    /*@ assigns \nothing; */
    HASHMAP_LINKAGE void HM_LIFETIME_OPEN_(HASHMAP_TYPE_NAME* map)    { (void)map; }
    /*@ assigns \nothing; */
    HASHMAP_LINKAGE void HM_LIFETIME_RESTAMP_(HASHMAP_TYPE_NAME* map) { (void)map; }
#endif

/* ============================================================================
 * Internal helpers (not part of public API)
 * ========================================================================= */

/* Defined once per translation unit (VERIFY-054): they do not depend on the
   map's types, and a second map redefined them. */
#ifndef CANON_HM_INTERNAL_HELPERS_DEFINED
#define CANON_HM_INTERNAL_HELPERS_DEFINED
/*@ requires capacity >= 1;
    assigns \nothing;
    ensures \result < capacity; */
static inline usize hm_home(u64 hash, usize capacity) {
    return (usize)(hash & (u64)(capacity - 1u));
}

/*@ requires capacity >= 1;
    assigns \nothing;
    ensures \result < capacity; */
static inline usize hm_wrap(usize index, usize capacity) {
    return index & (capacity - 1u);
}

/*@ assigns \nothing;
    ensures \result != 0;
    ensures h != 0 ==> \result == h; */
static inline u64 hm_normalize_hash(u64 h) {
    return (h == 0u) ? 1u : h;
}
#endif /* CANON_HM_INTERNAL_HELPERS_DEFINED */

/* ============================================================================
 * hashmap_buffer_size
 * ========================================================================= */

/* Empties every slot, a typed store per slot (VERIFY-055). It replaces a
   memset of the slot array: WP's typed model does not carry a byte fill to
   the slots' fields, so with memset no clause about the emptied slots could
   be proved. The fields read afterwards are the same: occupied false,
   hash 0, psl 0. Key and value are left as they were, as they are when a
   remove empties a slot; they are never read from an unoccupied slot. */
/*@ requires \valid(slots + (0 .. capacity - 1));
    assigns slots[0 .. capacity - 1];
    ensures \forall integer k; 0 <= k < capacity ==>
            !slots[k].occupied && slots[k].hash == 0 && slots[k].psl == 0; */
static inline void HASHMAP_FN(empty_slots_)(HASHMAP_SLOT_NAME* slots, usize capacity) {
    /*@ loop invariant 0 <= j <= capacity;
        loop invariant \forall integer k; 0 <= k < j ==>
            !slots[k].occupied && slots[k].hash == 0 && slots[k].psl == 0;
        loop assigns j, slots[0 .. capacity - 1];
        loop variant capacity - j; */
    for (usize j = 0; j < capacity; j++) {
        slots[j].occupied = false;
        slots[j].hash     = 0u;
        slots[j].psl      = 0u;
    }
}

/*@ assigns \nothing;
    ensures \result == 0 || \result == capacity * sizeof(HASHMAP_SLOT_NAME); */
HASHMAP_LINKAGE usize HM_BUFFER_SIZE_(usize capacity) {
    usize total = 0;
    if (!checked_mul(capacity, sizeof(HASHMAP_SLOT_NAME), &total)) { return 0; }
    return total;
}

/* ============================================================================
 * hashmap_init
 * ========================================================================= */

/*@ requires map == \null || \valid(map);
    requires buf.ptr == \null || buf.len == 0 || \valid(buf.ptr + (0 .. buf.len - 1));
    requires map == \null || buf.ptr == \null || buf.len == 0 ||
             \separated(map, buf.ptr + (0 .. buf.len - 1));
    requires capacity <= 1073741824;
    assigns *map, buf.ptr[0 .. buf.len - 1];
    ensures \result.is_ok ==> HASHMAP_FN(inv_)(map) && map->len == 0 &&
            map->capacity == capacity && map->ctx == ctx;
    ensures !\result.is_ok ==>
            (\result.val.err == ERR_INVALID_ARG || \result.val.err == ERR_BUFFER_TOO_SMALL); */
HASHMAP_LINKAGE result__Bool_Error HM_INIT_(
    borrowed(HASHMAP_TYPE_NAME*) map,
    bytes_t                      buf,
    usize                        capacity,
    void*                        ctx
) {
    if (!map)           { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (!buf.ptr)       { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (capacity == 0u)  { return result__Bool_Error_err(ERR_INVALID_ARG); }

    if (!bits_is_power_of_two((u64)capacity)) {
        return result__Bool_Error_err(ERR_INVALID_ARG);
    }

    usize required = HM_BUFFER_SIZE_(capacity);
    if ((required == 0u) || (buf.len < required)) {
        return result__Bool_Error_err(ERR_BUFFER_TOO_SMALL);
    }

    /* VERIFY-054 H1: the slots are accessed as HASHMAP_SLOT_NAME, so the
       buffer must be aligned for it. A byte buffer is not, in general; one
       accepted here made the first insert undefined. Tested by address
       value, which C defines for any pointer. */
    if (((uintptr_t)buf.ptr % (uintptr_t)offsetof(HASHMAP_FN(slot_align_), s_)) != 0u) {
        return result__Bool_Error_err(ERR_INVALID_ARG);
    }

    map->slots    = (HASHMAP_SLOT_NAME*)buf.ptr;
    map->capacity = capacity;
    map->len      = 0;
    map->ctx      = ctx;

    HASHMAP_FN(empty_slots_)(map->slots, capacity);

    HM_LIFETIME_OPEN_(map);

    return result__Bool_Error_ok(true);
}

/* ============================================================================
 * hashmap_clear
 * ========================================================================= */

/*@ requires HASHMAP_FN(inv_)(map);
    assigns map->len, map->slots[0 .. map->capacity - 1];
    ensures HASHMAP_FN(inv_)(map);
    ensures map->len == 0; */
HASHMAP_LINKAGE void HM_CLEAR_(borrowed(HASHMAP_TYPE_NAME*) map) {
    require_msg(map != NULL, "hashmap_clear: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_clear: map is uninitialized");
    HASHMAP_FN(empty_slots_)(map->slots, map->capacity);
    map->len = 0;
    HM_LIFETIME_RESTAMP_(map);
}

/* ============================================================================
 * State queries
 * ========================================================================= */

/*@ requires \valid_read(map);
    assigns \nothing;
    ensures \result == map->len; */
HASHMAP_LINKAGE usize HM_LEN_(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_len: map cannot be NULL");
    return map->len;
}

/*@ requires \valid_read(map);
    assigns \nothing;
    ensures \result == map->capacity; */
HASHMAP_LINKAGE usize HM_CAPACITY_(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_capacity: map cannot be NULL");
    return map->capacity;
}

/*@ requires \valid_read(map);
    assigns \nothing;
    ensures \result <==> map->len == 0; */
HASHMAP_LINKAGE bool HM_IS_EMPTY_(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_is_empty: map cannot be NULL");
    return map->len == 0u;
}

/*@ requires HASHMAP_FN(inv_read_)(map);
    assigns \nothing; */
HASHMAP_LINKAGE f64 HM_LOAD_FACTOR_(const HASHMAP_TYPE_NAME* map) {
    require_msg(map != NULL, "hashmap_load_factor: map cannot be NULL");
    if (map->capacity == 0u) { return 0.0; }
    return (f64)map->len / (f64)map->capacity;
}

/* ============================================================================
 * hashmap_insert — Robin Hood insertion
 * ========================================================================= */

/*@ requires map == \null || HASHMAP_FN(inv_)(map);
    requires key == \null || \valid_read(key);
    requires val == \null || \valid_read(val);
    assigns map->len, map->slots[0 .. map->capacity - 1];
    behavior invalid:
      assumes map == \null || key == \null || val == \null;
      ensures !\result.is_ok && \result.val.err == ERR_INVALID_ARG;
    behavior full:
      assumes map != \null && key != \null && val != \null &&
              map->len >= map->capacity - map->capacity / 4;
      ensures !\result.is_ok && \result.val.err == ERR_CAPACITY_EXCEEDED;
      ensures map->len == \old(map->len);
    behavior room:
      assumes map != \null && key != \null && val != \null &&
              map->len < map->capacity - map->capacity / 4;
      ensures HASHMAP_FN(inv_)(map);
      ensures \result.is_ok ==>
              (\result.val.ok ? map->len == \old(map->len) + 1 : map->len == \old(map->len));
      ensures !\result.is_ok ==> \result.val.err == ERR_CAPACITY_EXCEEDED;
    complete behaviors;
    disjoint behaviors; */
HASHMAP_LINKAGE result__Bool_Error HM_INSERT_(
    borrowed(HASHMAP_TYPE_NAME*)   map,
    const hm_key_t*                key,
    const hm_val_t*                val
) {
    if (!map)  { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (!key)  { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (!val)  { return result__Bool_Error_err(ERR_INVALID_ARG); }
    require_msg(map->slots != NULL, "hashmap_insert: map is uninitialized");

    /* Enforce 75% load cap */
    if (map->len >= (map->capacity - (map->capacity / 4u))) {
        return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED);
    }

    u64   h         = hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize home      = hm_home(h, map->capacity);
    usize probe_idx = home;
    u32   psl       = 0;

    HASHMAP_SLOT_NAME incoming;
    incoming.hash     = h;
    incoming.psl      = 0;
    incoming.occupied = true;
    incoming.key      = *key;
    incoming.value    = *val;

    bool is_new_key = true;

    /*@ loop invariant 0 <= i <= map->capacity;
    loop invariant probe_idx < map->capacity;
    loop invariant psl <= i;
    loop invariant incoming.psl == psl;
    loop invariant incoming.hash != 0;
    loop invariant HASHMAP_FN(shape_)(map);
        loop invariant HASHMAP_FN(dist_)(map);
    loop invariant map->len == \at(map->len, Pre);
    loop assigns i, probe_idx, psl, incoming, map->slots[0 .. map->capacity - 1];
    loop variant map->capacity - i; */
    for (usize i = 0; i < map->capacity; i++) {
        HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied) {
            /* Empty slot — place incoming here */
            *slot = incoming;
            if (is_new_key) { map->len++; }
            HM_LIFETIME_RESTAMP_(map);
            return result__Bool_Error_ok(is_new_key);
        }

        /* Key already present — update value */
        if (slot->hash == incoming.hash &&
            HASHMAP_EQ_FN(&slot->key, &incoming.key, map->ctx))
        {
            slot->value = incoming.value;
            HM_LIFETIME_RESTAMP_(map);
            return result__Bool_Error_ok(false);
        }

        /*
         * Robin Hood: steal from the rich (low PSL).
         *
         * After the swap, `incoming` is the displaced element whose original
         * PSL was tmp.psl. Reset `psl` to tmp.psl so the increment below
         * produces incoming.psl = tmp.psl + 1 — the correct PSL for the
         * displaced element one slot further along.
         *
         * Without this reset, the displaced element would be stored with
         * PSL = (original_incoming_psl + 1) instead of (tmp.psl + 1).
         * Since original_incoming_psl > tmp.psl (the steal condition),
         * the stored PSL would be too large, corrupting the early-exit
         * invariant (slot->psl < probe_dist → not found) and causing
         * lookups and duplicate checks to miss existing keys.
         */
        if (slot->psl < psl) {
            HASHMAP_SLOT_NAME tmp = *slot;
            *slot    = incoming;
            incoming = tmp;
            psl      = incoming.psl; /* reset to displaced element's PSL */
        }

        psl++;
        incoming.psl = psl;
        probe_idx = hm_wrap(probe_idx + 1u, map->capacity);
    }

    /* Should never reach here if load factor is respected */
    return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED);
}

/* ============================================================================
 * hashmap_get — lookup by key, returns Option<VAL>
 * ========================================================================= */

/*@ requires HASHMAP_FN(inv_read_)(map);
    requires \valid_read(key);
    assigns \nothing; */
HASHMAP_LINKAGE HM_OPT_T_ HM_GET_(
    const HASHMAP_TYPE_NAME* map,
    const hm_key_t*          key
) {
    require_msg(map != NULL,        "hashmap_get: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_get: map is uninitialized");
    require_msg(key != NULL,        "hashmap_get: key cannot be NULL");

    u64   h         = hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize probe_idx = hm_home(h, map->capacity);
    u32   psl       = 0;

    /*@ loop invariant 0 <= i <= map->capacity;
    loop invariant probe_idx < map->capacity;
    loop invariant psl == i;
    loop assigns i, probe_idx, psl;
    loop variant map->capacity - i; */
    for (usize i = 0; i < map->capacity; i++) {
        const HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied) { return HM_OPT_FN_(_none)(); }
        if (slot->psl < psl) { return HM_OPT_FN_(_none)(); }

        if (slot->hash == h && HASHMAP_EQ_FN(&slot->key, key, map->ctx)) {
            return HM_OPT_FN_(_some)(slot->value);
        }

        psl++;
        probe_idx = hm_wrap(probe_idx + 1u, map->capacity);
    }

    return HM_OPT_FN_(_none)();
}

/* ============================================================================
 * hashmap_get_or_null
 * ========================================================================= */

/*@ requires HASHMAP_FN(inv_)(map);
    requires \valid_read(key);
    assigns \nothing;
    ensures \result == \null ||
            (\exists integer j; 0 <= j < map->capacity && \result == &map->slots[j].value); */
HASHMAP_LINKAGE borrowed(hm_val_t*) HM_GET_OR_NULL_(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_key_t*              key
) {
    require_msg(map != NULL,        "hashmap_get_or_null: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_get_or_null: map is uninitialized");
    require_msg(key != NULL,        "hashmap_get_or_null: key cannot be NULL");

    u64   h         = hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize probe_idx = hm_home(h, map->capacity);
    u32   psl       = 0;

    /*@ loop invariant 0 <= i <= map->capacity;
    loop invariant probe_idx < map->capacity;
    loop invariant psl == i;
    loop assigns i, probe_idx, psl;
    loop variant map->capacity - i; */
    for (usize i = 0; i < map->capacity; i++) {
        HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied)      { return NULL; }
        if (slot->psl < psl)      { return NULL; }

        if (slot->hash == h && HASHMAP_EQ_FN(&slot->key, key, map->ctx)) {
            return &slot->value;
        }

        psl++;
        probe_idx = hm_wrap(probe_idx + 1u, map->capacity);
    }

    return NULL;
}

/* ============================================================================
 * hashmap_contains_key
 * ========================================================================= */

/*@ requires HASHMAP_FN(inv_read_)(map);
    requires \valid_read(key);
    assigns \nothing; */
HASHMAP_LINKAGE bool HM_CONTAINS_KEY_(
    const HASHMAP_TYPE_NAME* map,
    const hm_key_t*          key
) {
    require_msg(map != NULL, "hashmap_contains_key: map cannot be NULL");
    require_msg(key != NULL, "hashmap_contains_key: key cannot be NULL");
    return HM_OPT_FN_(_is_some)(HM_GET_(map, key));
}

/* ============================================================================
 * hashmap_remove — remove by key, backward shift deletion
 * ========================================================================= */

/*@ requires map == \null || HASHMAP_FN(inv_)(map);
    requires key == \null || \valid_read(key);
    assigns map->len, map->slots[0 .. map->capacity - 1];
    behavior invalid:
      assumes map == \null || key == \null;
      ensures !\result.is_ok && \result.val.err == ERR_INVALID_ARG;
    behavior valid:
      assumes map != \null && key != \null;
      ensures HASHMAP_FN(inv_)(map);
      ensures \result.is_ok ==>
              map->len == (\old(map->len) == 0 ? 0 : \old(map->len) - 1);
      ensures !\result.is_ok ==> \result.val.err == ERR_NOT_FOUND && map->len == \old(map->len);
    complete behaviors;
    disjoint behaviors; */
HASHMAP_LINKAGE HM_RES_T_ HM_REMOVE_(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_key_t*              key
) {
    if (!map) { return HM_RES_FN_(_err)(ERR_INVALID_ARG); }
    if (!key) { return HM_RES_FN_(_err)(ERR_INVALID_ARG); }
    require_msg(map->slots != NULL, "hashmap_remove: map is uninitialized");

    u64   h         = hm_normalize_hash(HASHMAP_HASH_FN(key, map->ctx));
    usize probe_idx = hm_home(h, map->capacity);
    u32   psl       = 0;
    usize found_idx = map->capacity; /* sentinel: not found */

    /* Phase 1: find the slot */
    /*@ loop invariant 0 <= i <= map->capacity;
    loop invariant probe_idx < map->capacity;
    loop invariant psl == i;
    loop assigns i, probe_idx, psl;
    loop variant map->capacity - i; */
    for (usize i = 0; i < map->capacity; i++) {
        HASHMAP_SLOT_NAME* slot = &map->slots[probe_idx];

        if (!slot->occupied)  { break; }
        if (slot->psl < psl)  { break; }

        if (slot->hash == h && HASHMAP_EQ_FN(&slot->key, key, map->ctx)) {
            found_idx = probe_idx;
            break;
        }

        psl++;
        probe_idx = hm_wrap(probe_idx + 1u, map->capacity);
    }

    if (found_idx == map->capacity) {
        return HM_RES_FN_(_err)(ERR_NOT_FOUND);
    }

    hm_val_t old_value = map->slots[found_idx].value;

    /* Phase 2: backward shift deletion */
    usize curr = found_idx;
    /*@ loop invariant 0 <= i <= map->capacity - 1;
    loop invariant curr < map->capacity;
    loop invariant HASHMAP_FN(shape_)(map);
        loop invariant HASHMAP_FN(dist_)(map);
    loop invariant map->len == \at(map->len, Pre);
    loop assigns i, curr, map->slots[0 .. map->capacity - 1];
    loop variant map->capacity - 1 - i; */
    for (usize i = 0u; i < (map->capacity - 1u); i++) {
        usize next = hm_wrap(curr + 1u, map->capacity);
        HASHMAP_SLOT_NAME* next_slot = &map->slots[next];

        if (!next_slot->occupied || (next_slot->psl == 0u)) {
            break;
        }

        map->slots[curr]      = *next_slot;
        map->slots[curr].psl -= 1u;
        curr = next;
    }

    /* VERIFY-056: empty the slot the shift ends at, whether the loop broke
       or ran out. It ran out when every slot was occupied (capacities 1 and
       2 allow that), and the emptying used to sit inside the break only: the
       last slot kept its element, so a removed key stayed in the map. */
    map->slots[curr].occupied = false;
    map->slots[curr].hash     = 0;
    map->slots[curr].psl      = 0;

    /* VERIFY-056: len is not decremented below 0. The invariant does not tie
       len to the occupied slots, so it admits len 0 with the key present;
       through the API, once the shift above is right, len is at least 1 here. */
    if (map->len > 0u) {
        map->len--;
    }
    HM_LIFETIME_RESTAMP_(map);
    return HM_RES_FN_(_ok)(old_value);
}

/* ============================================================================
 * hashmap_iter_next
 * ========================================================================= */

/*@ requires HASHMAP_FN(inv_read_)(map);
    requires \valid(iter);
    requires key_out == \null || \valid(key_out);
    requires val_out == \null || \valid(val_out);
    requires \separated(iter, key_out, val_out);
    requires \separated(map, iter) &&
             (key_out == \null || \separated(map, key_out)) &&
             (val_out == \null || \separated(map, val_out));
    assigns *iter, *key_out, *val_out;
    ensures \result ==> *iter <= map->capacity &&
            (key_out != \null ==> *key_out == &map->slots[*iter - 1].key) &&
            (val_out != \null ==> *val_out == &map->slots[*iter - 1].value);
    ensures !\result ==> *iter == \max(\old(*iter), map->capacity); */
HASHMAP_LINKAGE bool HM_ITER_NEXT_(
    const HASHMAP_TYPE_NAME* map,
    usize*                   iter,
    const hm_key_t**         key_out,
    const hm_val_t**         val_out
) {
    require_msg(map != NULL,        "hashmap_iter_next: map cannot be NULL");
    require_msg(map->slots != NULL, "hashmap_iter_next: map is uninitialized");
    require_msg(iter != NULL,       "hashmap_iter_next: iter cannot be NULL");

    /*@ loop invariant \at(*iter, Pre) <= *iter;
    loop invariant *iter <= map->capacity || *iter == \at(*iter, Pre);
    loop assigns *iter;
    loop variant map->capacity - *iter; */
    while (*iter < map->capacity) {
        const HASHMAP_SLOT_NAME* slot = &map->slots[*iter];
        (*iter)++;

        if (slot->occupied) {
            if (key_out) { *key_out = &slot->key; }
            if (val_out) { *val_out = &slot->value; }
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
#undef HM_LIFETIME_FIELD_
