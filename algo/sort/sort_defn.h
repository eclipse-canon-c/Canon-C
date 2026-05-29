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
 * @file sort_defn.h
 * @brief Generates sort function definitions for separate compilation
 *
 * Include this in exactly ONE .c file to produce external linkage
 * definitions for algo_sort and algo_is_sorted. All other translation
 * units should include sort_decl.h (for declarations) or sort.h
 * (for header-only static-inline usage).
 *
 * Example (sort.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/sort/sort_defn.h"
 * ```
 *
 * That is all that is needed. No macros to set — this file handles linkage.
 *
 * Note: the four internal helpers (algo_sort_swap, algo_insertion_sort_range,
 * algo_merge, algo_merge_sort_range) are defined as static inline inside
 * sort_impl.h regardless of ALGO_SORT_LINKAGE. They will be translation-
 * unit-local functions in the resulting .c file — correct behavior, no ODR
 * issues.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * sort_defn.h is included from .c files only. It pulls in sort_impl.h with
 * ALGO_SORT_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * Consistent with any_all_defn.h, map_defn.h, reverse_defn.h, search_defn.h.
 *
 * @sa sort_decl.h  — pair this with decl.h in headers
 * @sa sort.h       — use this instead for header-only static-inline usage
 * @sa sort_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * sort_impl.h uses ALGO_SORT_LINKAGE as the specifier for algo_sort and
 * algo_is_sorted. The four internal helpers are always static inline
 * regardless of this setting.
 */
#define ALGO_SORT_LINKAGE /* external linkage */

#include "sort_impl.h"

/*
 * sort_impl.h checks for ALGO_SORT_LINKAGE and errors if absent.
 * We set it here before including, so the check passes.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef ALGO_SORT_LINKAGE
