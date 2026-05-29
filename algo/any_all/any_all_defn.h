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
 * @file any_all_defn.h
 * @brief Generates any_all function definitions for separate compilation
 *
 * Include this in exactly ONE .c file to produce external linkage
 * definitions for algo_any and algo_all. All other translation units
 * should include any_all_decl.h (for declarations) or any_all.h
 * (for header-only static-inline usage).
 *
 * Example (any_all.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/any_all/any_all_defn.h"
 * ```
 *
 * That is all that is needed. No macros to set — this file handles linkage.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * any_all_defn.h is included from .c files only. It pulls in any_all_impl.h
 * with ALGO_ANY_ALL_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * It is consistent with hashmap_defn.h — re-includable so that a single
 * .c file could in principle generate definitions under different linkage
 * configurations. In practice any_all_defn.h is included once per program.
 *
 * @sa any_all_decl.h  — pair this with decl.h in headers
 * @sa any_all.h       — use this instead for header-only static-inline usage
 * @sa any_all_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

/*
 * Set linkage to external (no static, no inline) before impl generates code.
 * any_all_impl.h uses ALGO_ANY_ALL_LINKAGE as the function specifier.
 */
#define ALGO_ANY_ALL_LINKAGE /* external linkage */
#include "any_all_impl.h"
/*
 * any_all_impl.h checks for ALGO_ANY_ALL_LINKAGE and errors if absent.
 * We set it here before including, so the check passes.
 * Undefine after so re-includes from other sources are not confused.
 */
#undef ALGO_ANY_ALL_LINKAGE
