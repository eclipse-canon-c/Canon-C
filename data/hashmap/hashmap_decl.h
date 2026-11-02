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
 * In map_u64_int.c (your implementation unit), which does NOT include
 * map_u64_int.h: hashmap_defn.h generates the types too, and C99 forbids a
 * repeated typedef. (Before VERIFY-054 this example included the header,
 * and did not compile.) The hash and equality functions have external
 * linkage, declared in the header and defined here:
 * ```c
 * u64  hash_u64(const u64* k, void* ctx) { ... }
 * bool eq_u64(const u64* a, const u64* b, void* ctx) { ... }
 * #define HASHMAP_KEY_TYPE u64            // the same configuration
 * #define HASHMAP_VAL_TYPE int
 * #define HASHMAP_HASH_FN  hash_u64
 * #define HASHMAP_EQ_FN    eq_u64
 * #define HASHMAP_TYPE_NAME map_u64_int
 * #define HASHMAP_FN(name)  map_u64_int_##name
 * #include "data/hashmap/hashmap_defn.h"
 * ```
 * test/integration/hashmap_separate_test.c builds this recipe.
 *
 * For header-only usage with static linkage, just use hashmap.h directly.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * hashmap_decl.h may be included from any layer that needs to forward-declare
 * the hashmap type. It generates extern declarations, not definitions.
 *
 * NOTE: This file intentionally has NO include guard.
 * It is a typed-instantiation header — like hashmap_impl.h and hashmap.h —
 * and must be re-includable with different HASHMAP_KEY_TYPE / HASHMAP_VAL_TYPE
 * configurations to produce distinct type declarations in the same translation
 * unit. The internal hm_decl_key_t / hm_decl_val_t macros are #undef'd at
 * the bottom of this file, and the value alias, option and result names are
 * per map (hashmap_mangle.h), making re-inclusion safe.
 *
 * Note on result__Bool_Error:
 * ────────────────────────────────────────────────────────────────────────────
 * CANON_RESULT(_Bool, Error) generates result__Bool_Error (not result_bool_Error)
 * in every standard: _Bool is pasted as written. The extern
 * declarations for hashmap_init and hashmap_insert use result__Bool_Error to
 * match the definitions emitted by hashmap_impl.h exactly.
 *
 * Lifetime field (CANON_LIFETIME_DEBUG):
 * ────────────────────────────────────────────────────────────────────────────
 * When CANON_LIFETIME_DEBUG is set, the struct layout here includes an extra
 * lifetime_t lt field appended after ctx — matching the definition emitted
 * by hashmap_impl.h. The flag MUST be set consistently across all translation
 * units that share a hashmap type, otherwise the struct sizes will disagree
 * and break separate compilation. Build with CANON_LIFETIME_DEBUG for the
 * whole project or none of it — never mixed.
 *
 * @sa hashmap.h       — header-only entry point (includes everything)
 * @sa hashmap_defn.h  — generates definitions (include in exactly one .c)
 * @sa hashmap_impl.h  — pure logic (included by defn)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

#include "hashmap_mangle.h"

#include "core/primitives/types.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/option/option.h"
#include "semantics/result/result.h"
#include "semantics/error.h"

#ifdef CANON_LIFETIME_DEBUG
    #include "core/primitives/lifetime.h"  /* lifetime_t */
#endif

/* ============================================================================
 * Required user definitions must be set before including this file
 * ========================================================================= */
#ifndef HASHMAP_KEY_TYPE
    #error "hashmap_decl.h: define HASHMAP_KEY_TYPE before including"
#endif
#ifndef HASHMAP_VAL_TYPE
    #error "hashmap_decl.h: define HASHMAP_VAL_TYPE before including"
#endif

/* VERIFY-054: the same per-map names as hashmap_impl.h, so that the types
   declared here are the ones the definitions return. They were
   option_hm_decl_val_t here and option_hm_val_t there: two struct types of
   one layout, not one type. */
typedef HASHMAP_KEY_TYPE HM_KEY_ALIAS_;
#define hm_decl_key_t HM_KEY_ALIAS_
typedef HASHMAP_VAL_TYPE HM_VAL_ALIAS_;
#define hm_decl_val_t HM_VAL_ALIAS_

HM_CANON_OPTION_(HM_VAL_ALIAS_)
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
#define CANON_RESULT_BOOL_ERROR_DEFINED
/* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
CANON_RESULT(_Bool, Error)
#endif /* CANON_RESULT_BOOL_ERROR_DEFINED */
/* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
HM_CANON_RESULT_(HM_VAL_ALIAS_, Error)

/* ============================================================================
 * Lifetime field macro — must match hashmap_impl.h exactly
 * ========================================================================= */

#ifdef CANON_LIFETIME_DEBUG
    #define HM_DECL_LIFETIME_FIELD_ \
        lifetime_t lt; /**< [debug] Lifetime token: id + open */
#else
    #define HM_DECL_LIFETIME_FIELD_  /* empty */
#endif

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
 *
 * Layout MUST match hashmap_impl.h exactly under the same flag settings,
 * otherwise sizeof and field offsets diverge between translation units.
 * ========================================================================= */

typedef struct {
    HASHMAP_SLOT_NAME* slots;
    usize              capacity;
    usize              len;
    void*              ctx;
    HM_DECL_LIFETIME_FIELD_
} HASHMAP_TYPE_NAME;

/* ============================================================================
 * External function declarations
 *
 * Note: hashmap_init and hashmap_insert return result__Bool_Error, not
 * result_bool_Error. CANON_RESULT(_Bool, Error) token-pastes to
 * result__Bool_Error in every standard: _Bool is pasted as written.
 * ========================================================================= */

extern usize HM_BUFFER_SIZE_(usize capacity);

extern result__Bool_Error HM_INIT_(
    borrowed(HASHMAP_TYPE_NAME*) map,
    bytes_t                      buf,
    usize                        capacity,
    void*                        ctx
);

extern void HM_CLEAR_(borrowed(HASHMAP_TYPE_NAME*) map);

extern usize HM_LEN_(const HASHMAP_TYPE_NAME* map);
extern usize HM_CAPACITY_(const HASHMAP_TYPE_NAME* map);
extern bool  HM_IS_EMPTY_(const HASHMAP_TYPE_NAME* map);
extern f64   HM_LOAD_FACTOR_(const HASHMAP_TYPE_NAME* map);

extern result__Bool_Error HM_INSERT_(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_decl_key_t*         key,
    const hm_decl_val_t*         val
);

extern HM_OPT_T_ HM_GET_(
    const HASHMAP_TYPE_NAME* map,
    const hm_decl_key_t*     key
);

extern borrowed(hm_decl_val_t*) HM_GET_OR_NULL_(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_decl_key_t*         key
);

extern bool HM_CONTAINS_KEY_(
    const HASHMAP_TYPE_NAME* map,
    const hm_decl_key_t*     key
);

extern HM_RES_T_ HM_REMOVE_(
    borrowed(HASHMAP_TYPE_NAME*) map,
    const hm_decl_key_t*         key
);

extern bool HM_ITER_NEXT_(
    const HASHMAP_TYPE_NAME*  map,
    usize*                    iter,
    const hm_decl_key_t**    key_out,
    const hm_decl_val_t**    val_out
);

/* Lifetime helpers — extern declarations so other TUs can see them.
 * Their bodies are empty no-ops without CANON_LIFETIME_DEBUG, but the
 * names always exist (callers in hashmap_impl.h invoke them
 * unconditionally; consistent linkage across TUs is required). */
extern void HM_LIFETIME_OPEN_(HASHMAP_TYPE_NAME* map);
extern void HM_LIFETIME_RESTAMP_(HASHMAP_TYPE_NAME* map);

#undef hm_decl_key_t
#undef hm_decl_val_t
#undef HM_DECL_LIFETIME_FIELD_
