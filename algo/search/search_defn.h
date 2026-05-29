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
 * @file search_defn.h
 * @brief Generates search function definitions for separate compilation
 *
 * Include this in exactly ONE .c file to produce external linkage
 * definitions for algo_lower_bound, algo_upper_bound, algo_find_sorted,
 * algo_binary_search, and algo_equal_range. All other translation units
 * should include search_decl.h (for declarations) or search.h
 * (for header-only static-inline usage).
 *
 * Example (search.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/search/search_defn.h"
 * ```
 *
 * That is all that is needed. No macros to set — this file handles linkage.
 *
 * Note: algo_lower_bound_impl is defined as static inline inside
 * search_impl.h regardless of ALGO_SEARCH_LINKAGE. It will be a
 * translation-unit-local function in the resulting .c file — correct
 * behavior, no ODR issues.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * search_defn.h is included from .c files only. It pulls in search_impl.h
 * with ALGO_SEARCH_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * Consistent with any_all_defn.h, map_defn.h, and reverse_defn.h.
 *
 * @sa search_decl.h  — pair this with decl.h in headers
 * @sa search.h       — use this instead for header-only static-inline usage
 * @sa search_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * search_impl.h uses ALGO_SEARCH_LINKAGE as the specifier for public functions.
 * algo_lower_bound_impl is always static inline regardless of this setting.
 */
#define ALGO_SEARCH_LINKAGE /* external linkage */

#include "search_impl.h"

/*
 * search_impl.h checks for ALGO_SEARCH_LINKAGE and errors if absent.
 * We set it here before including, so the check passes.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef ALGO_SEARCH_LINKAGE
