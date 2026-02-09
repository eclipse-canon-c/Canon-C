// semantics/result/result_mangle.h
#ifndef CANON_RESULT_MANGLE_H
#define CANON_RESULT_MANGLE_H

/**
 * @file result_mangle.h
 * @brief Name mangling conventions for Result<T, E> type
 *
 * This file defines the naming scheme for all Result-related types and functions.
 * It serves as the single source of truth for names - all other files reference
 * these macros instead of hardcoding names.
 *
 * Philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Single source of truth: All names defined in one place
 * - Customizable: Users can override before including to change naming
 * - Namespace-safe: Default names use result_ prefix to avoid collisions
 * - Consistent: Uniform naming pattern across all functions
 * - Explicit: No hidden name generation
 * - Two type parameters: Handles both value type (T) and error type (E)
 *
 * Default naming scheme:
 * ────────────────────────────────────────────────────────────────────────────
 * Types T and E produce:
 * - Type name: result_T_E (e.g., result_int_error, result_float_constcharptr)
 * - Struct tag: result_T_E_s (for separate compilation if needed)
 * - Functions: result_T_E_<operation> (e.g., result_int_error_ok, result_int_error_unwrap)
 *
 * Customization example:
 * ────────────────────────────────────────────────────────────────────────────
 * // Use Either<T,E> naming instead of Result<T,E>
 * #define MANGLE_RESULT_TYPE(t, e)      Either_##t##_##e
 * #define MANGLE_RESULT_OK(t, e)        Either_##t##_##e##_Right
 * #define MANGLE_RESULT_ERR(t, e)       Either_##t##_##e##_Left
 * #include "result.h"
 * 
 * CANON_RESULT(int, error)
 * Either_int_error x = Either_int_error_Right(42);  // Custom naming!
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
 * @sa result_impl.h, result_decl.h, result_defn.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   TYPE NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Main Result type name
 *
 * Default: result_T_E (e.g., result_int_error, result_float_constcharptr)
 * Override to customize type naming across entire codebase
 */
#ifndef MANGLE_RESULT_TYPE
#  define MANGLE_RESULT_TYPE(_t, _e)          result_##_t##_##_e
#endif

/**
 * @brief Result struct tag name (for separate compilation)
 *
 * Default: result_T_E_s
 * Used when forward-declaring struct types
 */
#ifndef MANGLE_RESULT_STRUCT_TAG
#  define MANGLE_RESULT_STRUCT_TAG(_t, _e)    result_##_t##_##_e##_s
#endif

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ok(value) constructor function name
 *
 * Default: result_T_E_ok (e.g., result_int_error_ok(42))
 */
#ifndef MANGLE_RESULT_OK
#  define MANGLE_RESULT_OK(_t, _e)            result_##_t##_##_e##_ok
#endif

/**
 * @brief Err(error) constructor function name
 *
 * Default: result_T_E_err (e.g., result_int_error_err(ERR_NOT_FOUND))
 */
#ifndef MANGLE_RESULT_ERR
#  define MANGLE_RESULT_ERR(_t, _e)           result_##_t##_##_e##_err
#endif

/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief is_ok() query function name
 *
 * Default: result_T_E_is_ok (e.g., result_int_error_is_ok(res))
 */
#ifndef MANGLE_RESULT_IS_OK
#  define MANGLE_RESULT_IS_OK(_t, _e)         result_##_t##_##_e##_is_ok
#endif

/**
 * @brief is_err() query function name
 *
 * Default: result_T_E_is_err (e.g., result_int_error_is_err(res))
 */
#ifndef MANGLE_RESULT_IS_ERR
#  define MANGLE_RESULT_IS_ERR(_t, _e)        result_##_t##_##_e##_is_err
#endif

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief get_ok() safe extraction function name
 *
 * Default: result_T_E_get_ok (e.g., result_int_error_get_ok(res, &out))
 */
#ifndef MANGLE_RESULT_GET_OK
#  define MANGLE_RESULT_GET_OK(_t, _e)        result_##_t##_##_e##_get_ok
#endif

/**
 * @brief get_err() safe extraction function name
 *
 * Default: result_T_E_get_err (e.g., result_int_error_get_err(res, &out))
 */
#ifndef MANGLE_RESULT_GET_ERR
#  define MANGLE_RESULT_GET_ERR(_t, _e)       result_##_t##_##_e##_get_err
#endif

/**
 * @brief unwrap_or() with fallback function name
 *
 * Default: result_T_E_unwrap_or (e.g., result_int_error_unwrap_or(res, 0))
 */
#ifndef MANGLE_RESULT_UNWRAP_OR
#  define MANGLE_RESULT_UNWRAP_OR(_t, _e)     result_##_t##_##_e##_unwrap_or
#endif

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief unwrap() unsafe extraction function name
 *
 * Default: result_T_E_unwrap (e.g., result_int_error_unwrap(res))
 */
#ifndef MANGLE_RESULT_UNWRAP
#  define MANGLE_RESULT_UNWRAP(_t, _e)        result_##_t##_##_e##_unwrap
#endif

/**
 * @brief unwrap_err() error extraction function name
 *
 * Default: result_T_E_unwrap_err (e.g., result_int_error_unwrap_err(res))
 */
#ifndef MANGLE_RESULT_UNWRAP_ERR
#  define MANGLE_RESULT_UNWRAP_ERR(_t, _e)    result_##_t##_##_e##_unwrap_err
#endif

/**
 * @brief expect() with message function name
 *
 * Default: result_T_E_expect (e.g., result_int_error_expect(res, "msg"))
 */
#ifndef MANGLE_RESULT_EXPECT
#  define MANGLE_RESULT_EXPECT(_t, _e)        result_##_t##_##_e##_expect
#endif

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION NAMES (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief map() transformation function name
 *
 * Default: result_T_E_map (e.g., result_int_error_map(res, fn))
 */
#ifndef MANGLE_RESULT_MAP
#  define MANGLE_RESULT_MAP(_t, _e)           result_##_t##_##_e##_map
#endif

/**
 * @brief map_err() error transformation function name
 *
 * Default: result_T_E_map_err (e.g., result_int_error_map_err(res, fn))
 */
#ifndef MANGLE_RESULT_MAP_ERR
#  define MANGLE_RESULT_MAP_ERR(_t, _e)       result_##_t##_##_e##_map_err
#endif

/**
 * @brief and_then() chaining function name
 *
 * Default: result_T_E_and_then (e.g., result_int_error_and_then(res, fn))
 */
#ifndef MANGLE_RESULT_AND_THEN
#  define MANGLE_RESULT_AND_THEN(_t, _e)      result_##_t##_##_e##_and_then
#endif

/**
 * @brief or_else() alternative provider function name
 *
 * Default: result_T_E_or_else (e.g., result_int_error_or_else(res, fn))
 */
#ifndef MANGLE_RESULT_OR_ELSE
#  define MANGLE_RESULT_OR_ELSE(_t, _e)       result_##_t##_##_e##_or_else
#endif

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief eq() equality comparison function name
 *
 * Default: result_T_E_eq (e.g., result_int_error_eq(r1, r2, eq_ok, eq_err))
 */
#ifndef MANGLE_RESULT_EQ
#  define MANGLE_RESULT_EQ(_t, _e)            result_##_t##_##_e##_eq
#endif

/* ════════════════════════════════════════════════════════════════════════════
   USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
// ────────────────────────────────────────────────────────────────────────────
// Example 1: Default naming (no customization needed)
// ────────────────────────────────────────────────────────────────────────────
#include "result.h"

typedef enum { ERR_NONE, ERR_NOT_FOUND, ERR_INVALID } error;
CANON_RESULT(int, error)

result_int_error x = result_int_error_ok(42);      // Uses default names
bool success = result_int_error_is_ok(x);

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Custom naming (Haskell-style Either/Right/Left)
// ────────────────────────────────────────────────────────────────────────────
#define MANGLE_RESULT_TYPE(t, e)      Either_##t##_##e
#define MANGLE_RESULT_OK(t, e)        Either_##t##_##e##_Right
#define MANGLE_RESULT_ERR(t, e)       Either_##t##_##e##_Left
#define MANGLE_RESULT_IS_OK(t, e)     Either_##t##_##e##_isRight
#define MANGLE_RESULT_IS_ERR(t, e)    Either_##t##_##e##_isLeft

#include "result.h"
CANON_RESULT(int, error)

Either_int_error x = Either_int_error_Right(42);   // Custom names!
bool success = Either_int_error_isRight(x);

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Project-specific prefix (avoid name collisions)
// ────────────────────────────────────────────────────────────────────────────
#define MANGLE_RESULT_TYPE(t, e)      myproject_res_##t##_##e

#include "result.h"
CANON_RESULT(int, error)

myproject_res_int_error x = myproject_res_int_error_ok(42);  // Namespaced!

// ────────────────────────────────────────────────────────────────────────────
// Example 4: Handling pointer types with typedefs
// ────────────────────────────────────────────────────────────────────────────
typedef const char* constcharptr;
typedef void* voidptr;

CANON_RESULT(int, constcharptr)
CANON_RESULT(voidptr, int)

result_int_constcharptr parse_result = result_int_constcharptr_ok(123);
result_voidptr_int alloc_result = result_voidptr_int_ok(malloc(100));
*/

#endif /* CANON_RESULT_MANGLE_H */
