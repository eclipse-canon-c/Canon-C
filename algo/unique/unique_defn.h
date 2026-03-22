/**
 * @file unique_defn.h
 * @brief Generates the unique function definition for separate compilation
 *
 * Include this in exactly ONE .c file to produce an external linkage
 * definition for algo_unique. All other translation units should include
 * unique_decl.h (for the declaration) or unique.h (for header-only
 * static-inline usage).
 *
 * Example (unique.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/unique/unique_defn.h"
 * ```
 *
 * That is all that is needed. No macros to set — this file handles linkage.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * unique_defn.h is included from .c files only. It pulls in unique_impl.h
 * with ALGO_UNIQUE_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * Consistent with any_all_defn.h, map_defn.h, reverse_defn.h,
 * search_defn.h, and sort_defn.h.
 *
 * @sa unique_decl.h  — pair this with decl.h in headers
 * @sa unique.h       — use this instead for header-only static-inline usage
 * @sa unique_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * unique_impl.h uses ALGO_UNIQUE_LINKAGE as the function specifier.
 */
#define ALGO_UNIQUE_LINKAGE /* external linkage */

#include "unique_impl.h"

/*
 * unique_impl.h checks for ALGO_UNIQUE_LINKAGE and errors if absent.
 * We set it here before including, so the check passes.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef ALGO_UNIQUE_LINKAGE
