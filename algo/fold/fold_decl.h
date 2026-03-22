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
 * includes fold_decl.h. CANON_RESULT generates static inline functions
 * so re-instantiation is safe.
 */
CANON_RESULT(bool, Error)

#endif /* CANON_ALGO_FOLD_DECL_H */
