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
 * @file fold_decl.h
 * @brief Forward declarations for Canon-C fold (separate compilation support)
 *
 * fold is architecturally macro-based — there is no single linkage-controlled
 * function at the generic level. This file exists for architectural consistency
 * with the 5-file pattern used across all algo/ modules.
 *
 * It provides the result_bool_Error type declaration needed by any translation
 * unit that references fold operations without including the full fold.h.
 *
 * For header-only usage, include fold.h directly.
 * For separate compilation of a .c file that uses fold macros, including
 * fold_decl.h provides the necessary type forward declarations.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * fold_decl.h may be included from any layer that uses fold types.
 *
 * @sa fold.h       — header-only entry point (include this for full API)
 * @sa fold_defn.h  — definitions for separate compilation
 * @sa fold_impl.h  — pure logic (included by defn)
 */
#ifndef CANON_ALGO_FOLD_DECL_H
#define CANON_ALGO_FOLD_DECL_H

#include "fold_mangle.h"

#include "core/primitives/types.h"
#include "semantics/error.h"
#include "semantics/result/result.h"
#include "core/ownership.h"

/*
 * Ensure result_bool_Error is available to any translation unit that
 * includes fold_decl.h. Guarded by CANON_RESULT_BOOL_ERROR_DEFINED so the
 * type is instantiated exactly once per translation unit no matter how
 * many canon headers a user combines — CANON_RESULT declares a struct
 * tag, and a second declaration in the same TU is a redefinition error.
 */
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
#define CANON_RESULT_BOOL_ERROR_DEFINED
/* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
CANON_RESULT(_Bool, Error)
#endif /* CANON_RESULT_BOOL_ERROR_DEFINED */

#endif /* CANON_ALGO_FOLD_DECL_H */
