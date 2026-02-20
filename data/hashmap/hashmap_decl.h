/**
 * @file hashmap_decl.h
 * @brief Forward declarations for Canon-C hashmap (separate compilation support)
 *
 * Include this in headers that reference hashmap types or functions without
 * needing full definitions. Pair with hashmap_defn.h in exactly one .c file
 * to generate the actual implementations.
 *
 * Typical separate-compilation workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * In map_u64_int.h (your type-specific header):
 * ```c
 * #define HASHMAP_KEY_TYPE u64
 * #define HASHMAP_VAL_TYPE int
 * #define HASHMAP_HASH_FN  hash_u64
 * #define HASHMAP_EQ_FN    eq_u64
 * #define HASHMAP_TYPE_NAME map_u64_int
 * #define HASHMAP_FN(name)  map_u64_int_##name
 * #include "data/hashmap/hashmap_decl.h"
 * ```
 *
 * In map_u64_int.c (your implementation unit):
 * ```c
 * #include "map_u64_int.h"
 * #define HASHMAP_LINKAGE  // (empty = external linkage)
 * #include "data/hashmap/hashmap_defn.h"
 * ```
 *
 * For header-only usage with static linkage, just use hashmap.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * hashmap_decl.h may be included from any layer that needs to forward-declare
 * the hashmap type. It generates extern declarations, not definitions.
 *
 * @sa hashmap.h       — header-only entry point (includes everything)
 * @sa hashmap_defn.h  — generates definitions (include in exactly one .c)
 * @sa hashmap_impl.h  — pure logic (included by defn)
 */

#ifndef CANON_DATA_HASHMAP_DECL_H
#define CANON_DATA_HASHMAP_DECL_H

#include "hashmap_mangle.h"

#include "core/primitives/types.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/option/option.h"
#include "semantics/result/result.h"
#include "semantics/error.h"

/* ============================================================================
 * Required user definitions must be set before including this file
 * ========================================================================= */
#ifndef HASHMAP_KEY_TYPE
    #error "hashmap_decl.h: define HASHMAP_KEY_TYPE before including"
#endif
#ifndef HASHMAP_VAL_TYPE
    #error "hashmap_decl.h: define HASHMAP_VAL_TYPE before including"
#endif

typedef HASHMAP_KEY_TYPE hm_decl_key_t;
typedef HASHMAP_VAL_TYPE hm_decl_val_t;

CANON_OPTION(hm_decl_val_t)
CANON_RESULT(bool, Error)
CANON_RESULT(hm_decl_val_t, Error)

/* ============================================================================
 * Slot forward declaration
 * ========================================================================= */

typedef struct {
    u64              hash;
    u32              psl;
    bool             occupied;
    hm_decl_key_t   key;
    hm_decl_val_t   value;
} HASHMAP_SLOT_NAME;

/* ============================================================================
 * Hashmap struct forward declaration
 * ========================================================================= */

typedef struct {
    HASHMAP_SLOT_NAME* slots;
    usize              capacity;
    usize              len;
    void*              ctx;
} HASHMAP_TYPE_NAME;

/* ============================================================================
 * External function declarations
 * ========================================================================= */

extern usize _HM_BUFFER_SIZE(usize capacity);

extern result_bool_Error _HM_INIT(
    borrowed(HASHMAP_TYPE_NAME*) map,
    bytes_t                      buf,
    usize                        capacity,
    void*                        ctx
);

extern void _HM_CLEAR(borrowed(HASHMAP_TYPE_NAME*) map);

extern usize _HM_LEN(const HASHMAP_TYPE_NAME* map);
extern usize _HM_CAPACITY(const HASHMAP_TYPE_NAME* map);
extern bool  _HM_IS_EMPTY(const HASHMAP_TYPE_NAME* map);
extern f64   _HM_LOAD_FACTOR(const HASHMAP_TYPE_NAME* map);

extern result_bool_Error _HM_INSERT(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_decl_key_t*         key,
    const hm_decl_val_t*         val
);

extern option_hm_decl_val_t _HM_GET(
    const HASHMAP_TYPE_NAME* map,
    const hm_decl_key_t*     key
);

extern borrowed(hm_decl_val_t*) _HM_GET_OR_NULL(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_decl_key_t*         key
);

extern bool _HM_CONTAINS_KEY(
    const HASHMAP_TYPE_NAME* map,
    const hm_decl_key_t*     key
);

extern result_hm_decl_val_t_Error _HM_REMOVE(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_decl_key_t*         key
);

extern bool _HM_ITER_NEXT(
    const HASHMAP_TYPE_NAME*  map,
    usize*                    iter,
    const hm_decl_key_t**    key_out,
    const hm_decl_val_t**    val_out
);

#undef hm_decl_key_t
#undef hm_decl_val_t

#endif /* CANON_DATA_HASHMAP_DECL_H */
