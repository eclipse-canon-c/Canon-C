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
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * hashmap_mangle.h has no includes of its own. It is included by
 * hashmap_impl.h, hashmap_decl.h, and hashmap_defn.h before any logic.
 *
 * @sa hashmap_impl.h  — uses these names in all generated symbols
 * @sa hashmap_decl.h  — forward declarations using these names
 * @sa hashmap_defn.h  — definitions using these names
 */

#ifndef CANON_DATA_HASHMAP_MANGLE_H
#define CANON_DATA_HASHMAP_MANGLE_H

/* ============================================================================
 * Primary type name
 * ========================================================================= */

/**
 * @def HASHMAP_TYPE_NAME
 * @brief The struct typedef name for the hashmap (default: hashmap)
 *
 * Override before including hashmap_impl.h to rename the type:
 * ```c
 * #define HASHMAP_TYPE_NAME my_map
 * ```
 */
#ifndef HASHMAP_TYPE_NAME
    #define HASHMAP_TYPE_NAME hashmap
#endif

/**
 * @def HASHMAP_SLOT_NAME
 * @brief The internal slot struct typedef name (default: hashmap_slot)
 *
 * Each slot holds one key–value pair plus Robin Hood metadata.
 * Override if the default name conflicts with another definition.
 */
#ifndef HASHMAP_SLOT_NAME
    #define HASHMAP_SLOT_NAME hashmap_slot
#endif

/* ============================================================================
 * Function name prefix
 * ========================================================================= */

/**
 * @def HASHMAP_FN(name)
 * @brief Prefixes all public hashmap function names
 *
 * Default produces: hashmap_init, hashmap_insert, hashmap_get, etc.
 * Override to produce: mymap_init, mymap_insert, mymap_get, etc.
 *
 * ```c
 * #define HASHMAP_FN(name) mymap_##name
 * ```
 */
#ifndef HASHMAP_FN
    #define HASHMAP_FN(name) hashmap_##name
#endif

/* ============================================================================
 * Derived names (built from the macros above — do not override directly)
 * ========================================================================= */

/* Constructor / lifecycle */
#define _HM_INIT         HASHMAP_FN(init)
#define _HM_CLEAR        HASHMAP_FN(clear)

/* Capacity / state */
#define _HM_LEN          HASHMAP_FN(len)
#define _HM_CAPACITY     HASHMAP_FN(capacity)
#define _HM_IS_EMPTY     HASHMAP_FN(is_empty)
#define _HM_LOAD_FACTOR  HASHMAP_FN(load_factor)

/* Core operations */
#define _HM_INSERT       HASHMAP_FN(insert)
#define _HM_GET          HASHMAP_FN(get)
#define _HM_GET_OR_NULL  HASHMAP_FN(get_or_null)
#define _HM_CONTAINS_KEY HASHMAP_FN(contains_key)
#define _HM_REMOVE       HASHMAP_FN(remove)

/* Iteration */
#define _HM_ITER_NEXT    HASHMAP_FN(iter_next)

/* Utilities */
#define _HM_BUFFER_SIZE  HASHMAP_FN(buffer_size)

/* ============================================================================
 * Lifetime helper names (internal — used only when CANON_LIFETIME_DEBUG is set)
 * ============================================================================
 * Mangled via HASHMAP_FN so each hashmap instantiation gets its own type-
 * specific open/restamp helper. Without mangling, a translation unit that
 * instantiated two hashmap types (e.g. map_u64_int and map_str_u32 in the
 * same .c file) would redefine the helper with a different struct parameter
 * type on the second include — a compile error.
 *
 * The names always exist (the macros are unconditional). The function
 * BODIES are no-ops without CANON_LIFETIME_DEBUG, but the names remain
 * referenceable. Callers in hashmap_impl.h invoke them unconditionally.
 * ========================================================================= */

#define _HM_LIFETIME_OPEN    HASHMAP_FN(lifetime_open_)
#define _HM_LIFETIME_RESTAMP HASHMAP_FN(lifetime_restamp_)

#endif /* CANON_DATA_HASHMAP_MANGLE_H */
