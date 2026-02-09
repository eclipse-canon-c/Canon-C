// semantics/option/option_mangle.h
#ifndef CANON_OPTION_MANGLE_H
#define CANON_OPTION_MANGLE_H

/**
 * @file option_mangle.h
 * @brief Name mangling conventions for Option<T> type
 *
 * This file defines the naming scheme for all Option-related types and functions.
 * It serves as the single source of truth for names - all other files reference
 * these macros instead of hardcoding names.
 *
 * Philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Single source of truth: All names defined in one place
 * - Customizable: Users can override before including to change naming
 * - Namespace-safe: Default names use canon_ prefix to avoid collisions
 * - Consistent: Uniform naming pattern across all functions
 * - Explicit: No hidden name generation
 *
 * Default naming scheme:
 * ────────────────────────────────────────────────────────────────────────────
 * Type T produces:
 * - Type name: option_T (e.g., option_int, option_float)
 * - Struct tag: option_T_s (for separate compilation if needed)
 * - Functions: option_T_<operation> (e.g., option_int_some, option_int_unwrap)
 *
 * Customization example:
 * ────────────────────────────────────────────────────────────────────────────
 * // Use Maybe<T> naming instead of Option<T>
 * #define MANGLE_OPTION_TYPE(t)      Maybe##t
 * #define MANGLE_OPTION_SOME(t)      Maybe##t##_Just
 * #define MANGLE_OPTION_NONE(t)      Maybe##t##_Nothing
 * #include "option.h"
 * 
 * CANON_OPTION(int)
 * MaybeInt x = MaybeInt_Just(42);  // Custom naming!
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time: O(1) — pure compile-time token pasting
 * - Space: O(1) — no runtime cost, pure preprocessor
 * - Zero overhead: Names resolved at compile time
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * N/A — pure compile-time macros, no runtime behavior
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (token pasting with ##)
 * - Works with all C99+ compilers
 * - No platform-specific code
 *
 * @sa option_impl.h, option_decl.h, option_defn.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   TYPE NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Main Option type name
 *
 * Default: option_T (e.g., option_int, option_float, option_ptr)
 * Override to customize type naming across entire codebase
 */
#ifndef MANGLE_OPTION_TYPE
#  define MANGLE_OPTION_TYPE(_t)          option_##_t
#endif

/**
 * @brief Option struct tag name (for separate compilation)
 *
 * Default: option_T_s
 * Used when forward-declaring struct types
 */
#ifndef MANGLE_OPTION_STRUCT_TAG
#  define MANGLE_OPTION_STRUCT_TAG(_t)    option_##_t##_s
#endif

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Some(value) constructor function name
 *
 * Default: option_T_some (e.g., option_int_some(42))
 */
#ifndef MANGLE_OPTION_SOME
#  define MANGLE_OPTION_SOME(_t)          option_##_t##_some
#endif

/**
 * @brief None constructor function name
 *
 * Default: option_T_none (e.g., option_int_none())
 */
#ifndef MANGLE_OPTION_NONE
#  define MANGLE_OPTION_NONE(_t)          option_##_t##_none
#endif

/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief is_some() query function name
 *
 * Default: option_T_is_some (e.g., option_int_is_some(opt))
 */
#ifndef MANGLE_OPTION_IS_SOME
#  define MANGLE_OPTION_IS_SOME(_t)       option_##_t##_is_some
#endif

/**
 * @brief is_none() query function name
 *
 * Default: option_T_is_none (e.g., option_int_is_none(opt))
 */
#ifndef MANGLE_OPTION_IS_NONE
#  define MANGLE_OPTION_IS_NONE(_t)       option_##_t##_is_none
#endif

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief get() safe extraction function name
 *
 * Default: option_T_get (e.g., option_int_get(opt, &out))
 */
#ifndef MANGLE_OPTION_GET
#  define MANGLE_OPTION_GET(_t)           option_##_t##_get
#endif

/**
 * @brief unwrap_or() with fallback function name
 *
 * Default: option_T_unwrap_or (e.g., option_int_unwrap_or(opt, 0))
 */
#ifndef MANGLE_OPTION_UNWRAP_OR
#  define MANGLE_OPTION_UNWRAP_OR(_t)     option_##_t##_unwrap_or
#endif

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief unwrap() unsafe extraction function name
 *
 * Default: option_T_unwrap (e.g., option_int_unwrap(opt))
 */
#ifndef MANGLE_OPTION_UNWRAP
#  define MANGLE_OPTION_UNWRAP(_t)        option_##_t##_unwrap
#endif

/**
 * @brief expect() with message function name
 *
 * Default: option_T_expect (e.g., option_int_expect(opt, "msg"))
 */
#ifndef MANGLE_OPTION_EXPECT
#  define MANGLE_OPTION_EXPECT(_t)        option_##_t##_expect
#endif

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION NAMES (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief map() transformation function name
 *
 * Default: option_T_map (e.g., option_int_map(opt, fn))
 */
#ifndef MANGLE_OPTION_MAP
#  define MANGLE_OPTION_MAP(_t)           option_##_t##_map
#endif

/**
 * @brief and_then() chaining function name
 *
 * Default: option_T_and_then (e.g., option_int_and_then(opt, fn))
 */
#ifndef MANGLE_OPTION_AND_THEN
#  define MANGLE_OPTION_AND_THEN(_t)      option_##_t##_and_then
#endif

/**
 * @brief or_else() alternative provider function name
 *
 * Default: option_T_or_else (e.g., option_int_or_else(opt, fn))
 */
#ifndef MANGLE_OPTION_OR_ELSE
#  define MANGLE_OPTION_OR_ELSE(_t)       option_##_t##_or_else
#endif

/**
 * @brief filter() conditional keeping function name
 *
 * Default: option_T_filter (e.g., option_int_filter(opt, pred))
 */
#ifndef MANGLE_OPTION_FILTER
#  define MANGLE_OPTION_FILTER(_t)        option_##_t##_filter
#endif

/**
 * @brief zip() combining function name
 *
 * Default: option_T_zip (e.g., option_int_zip(opt1, opt2, combine))
 */
#ifndef MANGLE_OPTION_ZIP
#  define MANGLE_OPTION_ZIP(_t)           option_##_t##_zip
#endif

/* ════════════════════════════════════════════════════════════════════════════
   MUTATION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief replace() mutation function name
 *
 * Default: option_T_replace (e.g., option_int_replace(&opt, new_val))
 */
#ifndef MANGLE_OPTION_REPLACE
#  define MANGLE_OPTION_REPLACE(_t)       option_##_t##_replace
#endif

/**
 * @brief take() extraction function name
 *
 * Default: option_T_take (e.g., option_int_take(&opt))
 */
#ifndef MANGLE_OPTION_TAKE
#  define MANGLE_OPTION_TAKE(_t)          option_##_t##_take
#endif

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief eq() equality comparison function name
 *
 * Default: option_T_eq (e.g., option_int_eq(opt1, opt2, eq_fn))
 */
#ifndef MANGLE_OPTION_EQ
#  define MANGLE_OPTION_EQ(_t)            option_##_t##_eq
#endif

/* ════════════════════════════════════════════════════════════════════════════
   USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
// ────────────────────────────────────────────────────────────────────────────
// Example 1: Default naming (no customization needed)
// ────────────────────────────────────────────────────────────────────────────
#include "option.h"
CANON_OPTION(int)

option_int x = option_int_some(42);      // Uses default names
bool present = option_int_is_some(x);

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Custom naming (Rust-style Maybe/Just/Nothing)
// ────────────────────────────────────────────────────────────────────────────
#define MANGLE_OPTION_TYPE(t)      Maybe##t
#define MANGLE_OPTION_SOME(t)      Maybe##t##_Just
#define MANGLE_OPTION_NONE(t)      Maybe##t##_Nothing
#define MANGLE_OPTION_IS_SOME(t)   Maybe##t##_isJust
#define MANGLE_OPTION_IS_NONE(t)   Maybe##t##_isNothing

#include "option.h"
CANON_OPTION(int)

MaybeInt x = MaybeInt_Just(42);          // Custom names!
bool present = MaybeInt_isJust(x);

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Project-specific prefix (avoid name collisions)
// ────────────────────────────────────────────────────────────────────────────
#define MANGLE_OPTION_TYPE(t)      myproject_opt_##t

#include "option.h"
CANON_OPTION(int)

myproject_opt_int x = myproject_opt_int_some(42);  // Namespaced!
*/

#endif /* CANON_OPTION_MANGLE_H */
