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
 * @file filter_defn.h
 * @brief Generates filter function definition for separate compilation
 *
 * Include this in exactly ONE .c file to produce an external linkage
 * definition for algo_filter. All other translation units should include
 * filter_decl.h (for the declaration) or filter.h (for header-only
 * static-inline usage).
 *
 * Example (filter.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/filter/filter_defn.h"
 * ```
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * filter_defn.h is included from .c files only. It pulls in filter_impl.h
 * with ALGO_FILTER_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * Consistent with hashmap_defn.h and any_all_defn.h — re-includable so that
 * a single .c file could generate definitions under different configurations.
 *
 * @sa filter_decl.h  — pair this with decl.h in headers
 * @sa filter.h       — use this instead for header-only static-inline usage
 * @sa filter_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * filter_impl.h uses ALGO_FILTER_LINKAGE as the function specifier.
 */
#define ALGO_FILTER_LINKAGE /* external linkage */
#include "filter_impl.h"
/*
 * filter_impl.h checks for ALGO_FILTER_LINKAGE and errors if absent.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef ALGO_FILTER_LINKAGE
