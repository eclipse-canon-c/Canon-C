/**
 * @file fold_impl.h
 * @brief Implementation support for Canon-C fold
 *
 * fold is architecturally different from any_all, filter, and find.
 * The core fold operation is macro-based at all three levels — there is
 * no single linkage-controlled function that covers all types. The typed
 * accumulator and element types are caller-determined at the macro call site.
 *
 * fold_impl.h therefore contains only the C99 fallback helper for
 * ALGO_FOLD_RESULT when CANON_NO_GNU_EXTENSIONS is defined. The main
 * macro interfaces (ALGO_FOLD, ALGO_FOLD_RESULT, DEFINE_ALGO_FOLD)
 * live entirely in fold.h.
 *
 * The ALGO_FOLD_RESULT C99 fallback here uses a do-while macro with an
 * explicit out-result parameter rather than a helper function with unsafe
 * function pointer casts. This avoids undefined behavior from casting
 * typed fold functions (e.g. result_bool_Error (*)(int*, const int*, void*))
 * to untyped void* function pointers.
 *
 * Do NOT include this file directly. Include fold.h (header-only) or
 * use fold_decl.h + fold_defn.h for separate compilation.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * fold_impl.h includes only:
 *   - fold_mangle.h
 *   - core/primitives/types.h
 *   - core/primitives/contract.h
 *   - semantics/error.h
 *   - semantics/result/result.h
 *   - core/ownership.h
 *
 * @sa fold.h       — user-facing entry point (include this)
 * @sa fold_mangle.h — name conventions
 * @sa fold_decl.h  — forward declarations for separate compilation
 * @sa fold_defn.h  — definitions for separate compilation
 */

/* ============================================================================
 * Guard: this file must be included with a linkage specifier set
 * ========================================================================= */
#ifndef ALGO_FOLD_LINKAGE
    #error "fold_impl.h must not be included directly. Include fold.h instead."
#endif

#include "fold_mangle.h"

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "semantics/error.h"
#include "semantics/result/result.h"
#include "core/ownership.h"

/*
 * result_bool_Error is instantiated once here so that fold_impl.h is
 * self-contained when included via fold_defn.h in a separate .c file.
 * CANON_RESULT generates static inline functions — re-instantiation
 * from fold.h (which also calls CANON_RESULT) is safe and harmless.
 */
CANON_RESULT(bool, Error)
