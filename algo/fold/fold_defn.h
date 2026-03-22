/**
 * @file fold_defn.h
 * @brief Generates fold support definitions for separate compilation
 *
 * Include this in .c files that use fold operations and need the
 * result_bool_Error type fully available in a separate compilation unit.
 *
 * Because fold is macro-based, this file is minimal — it sets the
 * ALGO_FOLD_LINKAGE guard and includes fold_impl.h which instantiates
 * CANON_RESULT(bool, Error) for the translation unit.
 *
 * Example (fold_support.c):
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "algo/fold/fold_defn.h"
 * ```
 *
 * For header-only usage, include fold.h directly instead.
 *
 * NOTE: This file intentionally has NO include guard.
 * Consistent with hashmap_defn.h, any_all_defn.h, filter_defn.h,
 * and find_defn.h.
 *
 * @sa fold_decl.h  — type declarations only
 * @sa fold.h       — full header-only entry point
 * @sa fold_impl.h  — the actual support logic (do not include directly)
 */

/*
 * NOTE: No include guard — intentionally re-includable.
 * See file-level comment above.
 */

#define ALGO_FOLD_LINKAGE /* external linkage */
#include "fold_impl.h"
#undef ALGO_FOLD_LINKAGE
