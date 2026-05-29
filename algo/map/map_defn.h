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
 * @file map_defn.h
 * @brief Generates map function definitions for separate compilation
 *
 * Include this in exactly ONE .c file to produce external linkage
 * definitions for algo_map and algo_map_inplace. All other translation
 * units should include map_decl.h (for declarations) or map.h
 * (for header-only static-inline usage).
 *
 * Example (map.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/map/map_defn.h"
 * ```
 *
 * That is all that is needed. No macros to set — this file handles linkage.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * map_defn.h is included from .c files only. It pulls in map_impl.h with
 * ALGO_MAP_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * It is consistent with any_all_defn.h — re-includable so that a single
 * .c file could in principle generate definitions under different linkage
 * configurations. In practice map_defn.h is included once per program.
 *
 * @sa map_decl.h  — pair this with decl.h in headers
 * @sa map.h       — use this instead for header-only static-inline usage
 * @sa map_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * map_impl.h uses ALGO_MAP_LINKAGE as the function specifier.
 */
#define ALGO_MAP_LINKAGE /* external linkage */

#include "map_impl.h"

/*
 * map_impl.h checks for ALGO_MAP_LINKAGE and errors if absent.
 * We set it here before including, so the check passes.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef ALGO_MAP_LINKAGE
