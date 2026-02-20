/**
 * @file hashmap_defn.h
 * @brief Generates hashmap function definitions for separate compilation
 *
 * Include this in exactly ONE .c file to produce external linkage definitions
 * for all hashmap functions. All other translation units should include
 * hashmap_decl.h (for declarations) or hashmap.h (for header-only usage).
 *
 * Before including this file, you must:
 * 1. Set all HASHMAP_* configuration macros
 * 2. Include your type-specific header (which sets those macros)
 *
 * The generated functions use external linkage (no static, no inline).
 *
 * Example (map_u64_int.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * // Step 1: provide the hash and equality functions
 * static u64  hash_u64(const u64* k, void* ctx) {
 *     (void)ctx;
 *     // FNV-1a or similar
 *     u64 h = *k * 11400714819323198485ULL;
 *     return h == 0 ? 1 : h;
 * }
 * static bool eq_u64(const u64* a, const u64* b, void* ctx) {
 *     (void)ctx; return *a == *b;
 * }
 *
 * // Step 2: configure the hashmap
 * #define HASHMAP_KEY_TYPE  u64
 * #define HASHMAP_VAL_TYPE  int
 * #define HASHMAP_HASH_FN   hash_u64
 * #define HASHMAP_EQ_FN     eq_u64
 * #define HASHMAP_TYPE_NAME map_u64_int
 * #define HASHMAP_FN(name)  map_u64_int_##name
 *
 * // Step 3: generate definitions
 * #include "data/hashmap/hashmap_defn.h"
 * ```
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * hashmap_defn.h is included from .c files only. It pulls in hashmap_impl.h
 * with HASHMAP_LINKAGE set to empty (external linkage, no static/inline).
 *
 * @sa hashmap_decl.h  — pair this with decl.h in headers
 * @sa hashmap.h       — use this instead for header-only static-inline usage
 * @sa hashmap_impl.h  — the actual logic (do not include directly)
 */

#ifndef CANON_DATA_HASHMAP_DEFN_H
#define CANON_DATA_HASHMAP_DEFN_H

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * hashmap_impl.h uses HASHMAP_LINKAGE as the function specifier on all fns.
 */
#define HASHMAP_LINKAGE /* external linkage */

#include "hashmap_impl.h"

/*
 * hashmap_impl.h checks for HASHMAP_LINKAGE and errors if absent.
 * We set it here before including, so the check passes.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef HASHMAP_LINKAGE

#endif /* CANON_DATA_HASHMAP_DEFN_H */
