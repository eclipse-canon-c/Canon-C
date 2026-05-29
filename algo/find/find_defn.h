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
 * @file find_defn.h
 * @brief Generates find function definition for separate compilation
 *
 * Include this in exactly ONE .c file to produce an external linkage
 * definition for algo_find. All other translation units should include
 * find_decl.h (for the declaration) or find.h (for header-only usage).
 *
 * Example (find.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/find/find_defn.h"
 * ```
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * find_defn.h is included from .c files only. It pulls in find_impl.h
 * with ALGO_FIND_LINKAGE set to empty (external linkage, no static/inline).
 *
 * NOTE: This file intentionally has NO include guard.
 * Consistent with hashmap_defn.h, any_all_defn.h, and filter_defn.h.
 *
 * @sa find_decl.h  — pair this with decl.h in headers
 * @sa find.h       — use this instead for header-only static-inline usage
 * @sa find_impl.h  — the actual logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

#define ALGO_FIND_LINKAGE /* external linkage */
#include "find_impl.h"
#undef ALGO_FIND_LINKAGE
