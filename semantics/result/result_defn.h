// semantics/result/result_defn.h
#ifndef CANON_RESULT_DEFN_H
#define CANON_RESULT_DEFN_H

#ifndef CANON_RESULT_MANGLE_H
#  include "result_mangle.h"
#endif

#ifndef CANON_RESULT_IMPL_H
#  include "result_impl.h"
#endif


/**
 * @file result_defn.h
 * @brief Definition macros for Result<T, E> type (implementation generation)
 *
 * This file provides macros for defining Result types and functions with actual
 * implementations. Use this in source files (.c) for separate compilation, or
 * in header files (.h) with static inline for header-only libraries.
 *
 * Philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Implementation generation: Combines naming (mangle.h) with logic (impl.h)
 * - Compilation model flexibility: Header-only OR separate compilation
 * - Linkage control: Choose static inline, extern, or other linkage
 * - One definition rule: Define once per translation unit (or static inline)
 * - Zero overhead: Inlined by default, no function call overhead
 * - Two type parameters: Handles both value type (T) and error type (E)
 *
 * Use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Header-only: Define with static inline in .h files
 * 2. Separate compilation: Define once in .c files (no linkage keyword)
 * 3. Shared libraries: Define with __declspec or __attribute__ for export
 * 4. Custom implementations: Override specific IMPL_* macros before including
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations: O(1) time complexity
 * - Space: sizeof(bool) + max(sizeof(T), sizeof(E)) + alignment padding (union)
 * - Inlining: static inline eliminates function call overhead
 * - No allocations: Pure stack-based operations
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Independent Result instances are thread-safe
 * - No shared mutable state
 * - Union access requires is_ok check (wrong member access is UB)
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline, compound literals, designated initializers, unions)
 * - Works with all C99+ compilers
 * - Linkage attributes are compiler-specific (use _linkage parameter)
 *
 * Typical workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * // Header-only style (traditional Canon-C)
 * #include "result_defn.h"
 * typedef enum { ERR_NONE, ERR_IO } error;
 * DEFINE_RESULT_ALL(static inline, int, error)
 * 
 * // Separate compilation style
 * // In .h: DECLARE_RESULT_ALL(extern, int, error)
 * // In .c: DEFINE_RESULT_ALL(, int, error)  // No linkage keyword for extern
 *
 * @sa result_decl.h, result_mangle.h, result_impl.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   TYPE DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define Result<T, E> struct
 *
 * Generates the actual struct definition with memory layout.
 * Must be defined exactly once per type pair (unless static inline).
 *
 * Performance: O(1) compile time
 * Memory layout: sizeof(bool) + max(sizeof(_t), sizeof(_e)) + alignment padding
 *                Union saves space compared to separate fields
 *
 * Example:
 * DEFINE_RESULT_STRUCT(int, error)
 * // Generates: struct result_int_error_s { bool is_ok; union { int ok; error err; }; };
 *
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_STRUCT(_t, _e) \
    struct MANGLE_RESULT_STRUCT_TAG(_t, _e) IMPL_RESULT_STRUCT(_t, _e);

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define Ok(value) constructor function
 *
 * Generates function that wraps a value in Result<T, E>.
 *
 * Performance: O(1) time, O(1) space (stack allocation via compound literal)
 * Returns: Result containing the success value
 *
 * @param _linkage Linkage specifier (e.g., static inline, empty for extern)
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_OK(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_OK(_t, _e)(_t v) \
        IMPL_RESULT_OK(_t, _e, MANGLE_RESULT_TYPE(_t, _e), v)

/**
 * @brief Define Err(error) constructor function
 *
 * Generates function that wraps an error in Result<T, E>.
 *
 * Performance: O(1) time, O(1) space (stack allocation via compound literal)
 * Returns: Result containing the error value
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_ERR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_ERR(_t, _e)(_e err) \
        IMPL_RESULT_ERR(_t, _e, MANGLE_RESULT_TYPE(_t, _e), err)

/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define is_ok() query function
 *
 * Generates function that checks if Result is Ok.
 *
 * Performance: O(1) time, O(1) space (single boolean read)
 * Returns: true if Ok(value), false if Err(error)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_IS_OK(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_IS_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_IS_OK(_t, _e, r)

/**
 * @brief Define is_err() query function
 *
 * Generates function that checks if Result is Err.
 *
 * Performance: O(1) time, O(1) space (single boolean negation)
 * Returns: true if Err(error), false if Ok(value)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_IS_ERR(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_IS_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_IS_ERR(_t, _e, r)

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define get_ok() safe extraction function
 *
 * Generates function that safely extracts success value via pointer.
 * Permissive: NULL pointer is allowed, returns false.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 * Returns: true if value extracted, false if Err or NULL pointer
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_GET_OK(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_GET_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t* out) \
        IMPL_RESULT_GET_OK(_t, _e, r, out)

/**
 * @brief Define get_err() safe extraction function
 *
 * Generates function that safely extracts error value via pointer.
 * Permissive: NULL pointer is allowed, returns false.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 * Returns: true if error extracted, false if Ok or NULL pointer
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_GET_ERR(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_GET_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _e* out) \
        IMPL_RESULT_GET_ERR(_t, _e, r, out)

/**
 * @brief Define unwrap_or() with fallback function
 *
 * Generates function that extracts value or returns fallback.
 * Safe alternative to unwrap() - never panics.
 *
 * Performance: O(1) time, O(1) space (conditional return)
 * Returns: Contained value if Ok, fallback if Err
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_UNWRAP_OR(_linkage, _t, _e) \
    _linkage _t \
    MANGLE_RESULT_UNWRAP_OR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t fallback) \
        IMPL_RESULT_UNWRAP_OR(_t, _e, r, fallback)

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define unwrap() unsafe extraction function
 *
 * Generates function that extracts value, panicking if Err.
 * ⚠️ WARNING: Only use when certain result is Ok!
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Crashes with clear message if Result is Err
 * Returns: Contained value
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_UNWRAP(_linkage, _t, _e) \
    _linkage _t \
    MANGLE_RESULT_UNWRAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_UNWRAP(_t, _e, r)

/**
 * @brief Define unwrap_err() error extraction function
 *
 * Generates function that extracts error, panicking if Ok.
 * The opposite of unwrap() - extracts error from Err.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Crashes with clear message if Result is Ok
 * Returns: Contained error
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_UNWRAP_ERR(_linkage, _t, _e) \
    _linkage _e \
    MANGLE_RESULT_UNWRAP_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_UNWRAP_ERR(_t, _e, r)

/**
 * @brief Define expect() with message function
 *
 * Generates function that extracts value with custom panic message.
 * Like unwrap(), but with descriptive error message.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Crashes with custom message if Err
 * Returns: Contained value
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_EXPECT(_linkage, _t, _e) \
    _linkage _t \
    MANGLE_RESULT_EXPECT(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, const char* msg) \
        IMPL_RESULT_EXPECT(_t, _e, r, msg)

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION DEFINITIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define map() transformation function
 *
 * Generates function that transforms contained value if Ok.
 * If Err, returns Err unchanged without calling transformation function.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 * Returns: Ok(f(value)) if Ok(value), Err unchanged if Err
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_MAP(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_MAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t (*f)(_t)) \
        IMPL_RESULT_MAP(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f, \
                        MANGLE_RESULT_OK(_t, _e))

/**
 * @brief Define map_err() error transformation function
 *
 * Generates function that transforms contained error if Err.
 * If Ok, returns Ok unchanged without calling transformation function.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 * Returns: Ok unchanged if Ok, Err(f(error)) if Err(error)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_MAP_ERR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_MAP_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _e (*f)(_e)) \
        IMPL_RESULT_MAP_ERR(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f, \
                            MANGLE_RESULT_ERR(_t, _e))

/**
 * @brief Define and_then() chaining function
 *
 * Generates function that chains Result-returning operations.
 * Like map(), but f returns Result instead of plain value.
 * Prevents nested Result<Result<T,E>,E>.
 *
 * Performance: O(f) time where f is the chained function
 *              O(1) space (no nesting)
 * Returns: f(value) if Ok(value), Err unchanged if Err
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_AND_THEN(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_AND_THEN(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, \
        MANGLE_RESULT_TYPE(_t, _e) (*f)(_t)) \
        IMPL_RESULT_AND_THEN(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f)

/**
 * @brief Define or_else() alternative provider function
 *
 * Generates function that provides alternative if Err.
 * Returns r if Ok, otherwise calls fallback(error) to get alternative.
 *
 * Performance: O(1) if Ok, O(fallback) if Err
 *              O(1) space
 * Returns: r if Ok, fallback(error) if Err
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_OR_ELSE(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_OR_ELSE(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, \
        MANGLE_RESULT_TYPE(_t, _e) (*f)(_e)) \
        IMPL_RESULT_OR_ELSE(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f)

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define eq() equality comparison function
 *
 * Generates function that checks equality of two Results.
 * Two Results are equal if both Ok with equal values, or both Err with equal errors.
 *
 * Performance: O(1) for Ok-Err or Err-Ok comparison
 *              O(eq_ok) for Ok-Ok where eq_ok is equality function
 *              O(eq_err) for Err-Err where eq_err is equality function
 *              O(1) space
 * Returns: true if equal, false otherwise
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_EQ(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_EQ(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r1, \
        MANGLE_RESULT_TYPE(_t, _e) r2, \
        bool (*eq_ok)(_t, _t), \
        bool (*eq_err)(_e, _e)) \
        IMPL_RESULT_EQ(_t, _e, r1, r2, eq_ok, eq_err)

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS (DEFINE MULTIPLE AT ONCE)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define all Result functions (no type/struct)
 *
 * Generates implementations for all Result<T, E> functions.
 * Does not define the struct itself - use with DEFINE_RESULT_STRUCT.
 *
 * Performance: All functions are O(1) except combinators which are O(f)
 *
 * @param _linkage Linkage specifier (e.g., static inline for header-only)
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_FUNCTIONS(_linkage, _t, _e) \
    DEFINE_RESULT_OK(_linkage, _t, _e) \
    DEFINE_RESULT_ERR(_linkage, _t, _e) \
    DEFINE_RESULT_IS_OK(_linkage, _t, _e) \
    DEFINE_RESULT_IS_ERR(_linkage, _t, _e) \
    DEFINE_RESULT_GET_OK(_linkage, _t, _e) \
    DEFINE_RESULT_GET_ERR(_linkage, _t, _e) \
    DEFINE_RESULT_UNWRAP_OR(_linkage, _t, _e) \
    DEFINE_RESULT_UNWRAP(_linkage, _t, _e) \
    DEFINE_RESULT_UNWRAP_ERR(_linkage, _t, _e) \
    DEFINE_RESULT_EXPECT(_linkage, _t, _e) \
    DEFINE_RESULT_MAP(_linkage, _t, _e) \
    DEFINE_RESULT_MAP_ERR(_linkage, _t, _e) \
    DEFINE_RESULT_AND_THEN(_linkage, _t, _e) \
    DEFINE_RESULT_OR_ELSE(_linkage, _t, _e) \
    DEFINE_RESULT_EQ(_linkage, _t, _e)

/**
 * @brief Define complete Result<T, E> type and all functions
 *
 * One-shot macro to define everything needed for Result<T, E>:
 * - Struct definition
 * - All function implementations
 *
 * Performance: All operations O(1) except combinators
 * Space: sizeof(bool) + max(sizeof(T), sizeof(E)) + padding per instance
 *
 * Example usage (header-only):
 * ```c
 * #include "result_defn.h"
 * typedef enum { ERR_NONE, ERR_IO } error;
 * DEFINE_RESULT_ALL(static inline, int, error)
 * 
 * result_int_error x = result_int_error_ok(42);
 * ```
 *
 * Example usage (separate compilation):
 * ```c
 * // In .c file only
 * #include "result_defn.h"
 * typedef enum { ERR_NONE, ERR_IO } error;
 * DEFINE_RESULT_ALL(, int, error)  // No linkage keyword for extern
 * ```
 *
 * @param _linkage Linkage specifier (static inline for header-only, empty for extern)
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_ALL(_linkage, _t, _e) \
    DEFINE_RESULT_STRUCT(_t, _e) \
    DEFINE_RESULT_FUNCTIONS(_linkage, _t, _e)

/* ════════════════════════════════════════════════════════════════════════════
   USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
// ────────────────────────────────────────────────────────────────────────────
// Example 1: Header-only library (traditional Canon-C style)
// ────────────────────────────────────────────────────────────────────────────

// my_lib.h
#ifndef MY_LIB_H
#define MY_LIB_H

#include "result_defn.h"

typedef enum { ERR_NONE, ERR_IO, ERR_PARSE } error;

// Define with static inline - each translation unit gets own copy
DEFINE_RESULT_ALL(static inline, int, error)
DEFINE_RESULT_ALL(static inline, float, error)

#endif

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Separate compilation for faster builds
// ────────────────────────────────────────────────────────────────────────────

// my_results.h - Header file
#ifndef MY_RESULTS_H
#define MY_RESULTS_H

#include "result_decl.h"

typedef enum { ERR_NONE, ERR_IO } error;

// Declare only (no definitions)
DECLARE_RESULT_ALL(extern, int, error)
DECLARE_RESULT_ALL(extern, float, error)

#endif

// my_results.c - Source file
#include "my_results.h"
#include "result_defn.h"

// Define once (empty linkage for extern)
DEFINE_RESULT_ALL(, int, error)
DEFINE_RESULT_ALL(, float, error)

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Custom implementation for pointer types
// ────────────────────────────────────────────────────────────────────────────

#include "result_impl.h"

typedef enum { ERR_NONE, ERR_NULL } error;

// Override Ok() to assert non-NULL for pointer types
#undef IMPL_RESULT_OK
#define IMPL_RESULT_OK(t, e, tres, param) \
    { \
        require((param) != NULL, "result_ok: NULL pointer not allowed"); \
        return (tres){ .is_ok = true, .ok = (param) }; \
    }

#include "result_defn.h"

typedef void* void_ptr;
DEFINE_RESULT_ALL(static inline, void_ptr, error)

// Now result_void_ptr_error_ok(NULL) will panic at runtime
*/

#endif /* CANON_RESULT_DEFN_H */
