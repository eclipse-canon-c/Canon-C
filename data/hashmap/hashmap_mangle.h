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
 * @file hashmap_mangle.h
 * @brief Name mangling conventions for Canon-C hashmap
 *
 * Controls how the hashmap type and all associated functions are named.
 * Each symbol can be overridden individually before including hashmap_impl.h,
 * allowing multiple hashmap instantiations in the same translation unit with
 * different names, key types, or value types.
 *
 * Override example (rename everything to "table_str_u32"):
 * ```c
 * #define HASHMAP_TYPE_NAME   table_str_u32
 * #define HASHMAP_SLOT_NAME   table_str_u32_slot
 * #define HASHMAP_FN(name)    table_str_u32_##name
 * #include "data/hashmap/hashmap_impl.h"
 * ```
 *
 * Defaults (VERIFY-054): the three names below default per instantiation,
 * not once per translation unit. This file is re-read at every include, so
 * a second hashmap configured after `#undef`-ing the first one's macros
 * gets its own defaults. HASHMAP_SLOT_NAME defaults to HASHMAP_FN(slot),
 * so a second map needs only HASHMAP_TYPE_NAME and HASHMAP_FN.
 *
 * Per-map value names (VERIFY-054): the key and value types are given
 * per-map aliases, HASHMAP_FN(key_t) and HASHMAP_FN(val_t) (hashmap_key_t
 * and hashmap_val_t by default), and the option and result
 * types the API returns are named after it:
 *   option_<alias>        e.g. option_hashmap_val_t   (hashmap_get)
 *   result_<alias>_Error  e.g. result_hashmap_val_t_Error (hashmap_remove)
 * Before VERIFY-054 both were named after a fixed internal typedef
 * (option_hm_val_t), which a second map in the same translation unit
 * redefined, so two maps did not compile.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * hashmap_mangle.h has no includes of its own. It is included by
 * hashmap_impl.h, hashmap_decl.h, and hashmap_defn.h before any logic.
 *
 * @sa hashmap_impl.h  — uses these names in all generated symbols
 * @sa hashmap_decl.h  — forward declarations using these names
 * @sa hashmap_defn.h  — definitions using these names
 */

/* ============================================================================
 * Defaults, re-evaluated at every include (no include guard here)
 * ========================================================================= */

#ifndef HASHMAP_TYPE_NAME
    #define HASHMAP_TYPE_NAME hashmap
#endif

#ifndef HASHMAP_FN
    #define HASHMAP_FN(name) hashmap_##name
#endif

#ifndef HASHMAP_SLOT_NAME
    #define HASHMAP_SLOT_NAME HASHMAP_FN(slot)
#endif

#ifndef CANON_DATA_HASHMAP_MANGLE_H
#define CANON_DATA_HASHMAP_MANGLE_H

/* ============================================================================
 * Token pasting after expansion, and the per-map value names
 * ============================================================================
 * The arguments are expanded before pasting (they are not operands of ##
 * in HM_PASTE_), so HM_PASTE_(option_, HASHMAP_FN(val_t)) is
 * option_hashmap_val_t. These macros read HASHMAP_FN when used, so they
 * name the map configured at that point.
 * ========================================================================= */

#define HM_PASTE2_(a, b)  a##b
#define HM_PASTE_(a, b)   HM_PASTE2_(a, b)

#define HM_KEY_ALIAS_     HASHMAP_FN(key_t)
#define HM_VAL_ALIAS_     HASHMAP_FN(val_t)
#define HM_OPT_T_         HM_PASTE_(option_, HM_VAL_ALIAS_)
#define HM_OPT_FN_(name)  HM_PASTE_(HM_OPT_T_, name)
#define HM_RES_T_         HM_PASTE_(HM_PASTE_(result_, HM_VAL_ALIAS_), _Error)
#define HM_RES_FN_(name)  HM_PASTE_(HM_RES_T_, name)

/* Expands its argument first, so CANON_OPTION / CANON_RESULT see the alias */
#define HM_CANON_OPTION_(t)        CANON_OPTION(t)
#define HM_CANON_RESULT_(t, e)     CANON_RESULT(t, e)

/* ============================================================================
 * Function names
 * ========================================================================= */

/* Constructor / lifecycle */
#define HM_INIT_         HASHMAP_FN(init)
#define HM_CLEAR_        HASHMAP_FN(clear)

/* Capacity / state */
#define HM_LEN_          HASHMAP_FN(len)
#define HM_CAPACITY_     HASHMAP_FN(capacity)
#define HM_IS_EMPTY_     HASHMAP_FN(is_empty)
#define HM_LOAD_FACTOR_  HASHMAP_FN(load_factor)

/* Core operations */
#define HM_INSERT_       HASHMAP_FN(insert)
#define HM_GET_          HASHMAP_FN(get)
#define HM_GET_OR_NULL_  HASHMAP_FN(get_or_null)
#define HM_CONTAINS_KEY_ HASHMAP_FN(contains_key)
#define HM_REMOVE_       HASHMAP_FN(remove)

/* Iteration */
#define HM_ITER_NEXT_    HASHMAP_FN(iter_next)

/* Utilities */
#define HM_BUFFER_SIZE_  HASHMAP_FN(buffer_size)

/* Lifetime helpers */
#define HM_LIFETIME_OPEN_    HASHMAP_FN(lifetime_open_)
#define HM_LIFETIME_RESTAMP_ HASHMAP_FN(lifetime_restamp_)

#endif /* CANON_DATA_HASHMAP_MANGLE_H */
