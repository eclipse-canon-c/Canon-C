/**
 * @file reverse_defn.h
 * @brief Generates reverse function definitions for separate compilation
 *
 * Include this in exactly ONE .c file to produce external linkage
 * definitions for algo_reverse and algo_is_palindrome. All other
 * translation units should include reverse_decl.h (for declarations)
 * or reverse.h (for header-only static-inline usage).
 *
 * Example (reverse.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/reverse/reverse_defn.h"
 * ```
 *
 * That is all that is needed. No macros to set — this file handles linkage.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * reverse_defn.h is included from .c files only. It pulls in reverse_impl.h
 * with ALGO_REVERSE_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * It is consistent with any_all_defn.h and map_defn.h — re-includable so
 * that a single .c file could in principle generate definitions under
 * different linkage configurations. In practice reverse_defn.h is included
 * once per program.
 *
 * @sa reverse_decl.h  — pair this with decl.h in headers
 * @sa reverse.h       — use this instead for header-only static-inline usage
 * @sa reverse_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * reverse_impl.h uses ALGO_REVERSE_LINKAGE as the function specifier.
 */
#define ALGO_REVERSE_LINKAGE /* external linkage */

#include "reverse_impl.h"

/*
 * reverse_impl.h checks for ALGO_REVERSE_LINKAGE and errors if absent.
 * We set it here before including, so the check passes.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef ALGO_REVERSE_LINKAGE
