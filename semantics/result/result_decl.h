// semantics/result/result_decl.h
#ifndef CANON_RESULT_DECL_H
#define CANON_RESULT_DECL_H

#ifndef CANON_RESULT_MANGLE_H
#  include "result_mangle.h"
#endif

#ifndef CANON_RESULT_IMPL_H
#  include "result_impl.h"
#endif


/**
 * @file result_decl.h
 * @brief Declaration macros for Result<T, E> type (separate compilation support)
 *
 * This file provides macros for declaring Result types and functions without
 * defining their implementations. Use this in header files when you want to
 * separate declarations from definitions (classic .h/.c compilation model).
 *
 * Philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Separation of concerns: Declarations vs definitions
 * - Compilation model flexibility: Header-only OR separate compilation
 * - Linkage control: Choose static inline, extern, or other linkage
 * - Incremental compilation: Declare in headers, define in .c files
 * - Interface clarity: Headers show API without implementation details
 * - Two type parameters: Handles both value type (T) and error type (E)
 *
 * Use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Separate compilation: Declare in .h, define in .c (faster builds)
 * 2. Shared library APIs: Export Result types across library boundaries
 * 3. Forward declarations: Declare types before full definition
 * 4. Custom linkage: Apply extern, static, or __declspec attributes
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Declarations: O(1) compile time (no code generation)
 * - Runtime: Zero cost (declarations don't generate code)
 * - Build time: Faster incremental builds with separate compilation
 * - Binary size: Avoid code duplication with extern linkage
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * N/A — pure compile-time declarations, no runtime behavior
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline keyword, bool type, unions)
 * - Works with all C99+ compilers
 * - Linkage attributes are compiler-specific (use _linkage parameter)
 *
 * Typical workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * // my_types.h - Header file
 * #include "result_decl.h"
 * typedef enum { ERR_NONE, ERR_IO } error;
 * DECLARE_RESULT_ALL(extern, int, error)  // Declare for external linkage
 * 
 * // my_types.c - Source file
 * #include "result_defn.h"
 * DEFINE_RESULT_ALL(/* no linkage */, int, error)  // Define once
 * 
 * // main.c - Usage
 * #include "my_types.h"
 * result_int_error x = result_int_error_ok(42);  // Links to my_types.c
 *
 * @sa result_defn.h, result_mangle.h, result_impl.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   TYPE DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare Result<T, E> typedef
 *
 * Creates a typedef for the Result type using the mangled name.
 * This allows using result_T_E as a type name without struct keyword.
 *
 * Performance: O(1) compile time
 *
 * Example:
 * DECLARE_RESULT_TYPEDEF(int, error)
 * // Generates: typedef struct result_int_error_s result_int_error;
 *
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_TYPEDEF(_t, _e) \
    typedef struct MANGLE_RESULT_STRUCT_TAG(_t, _e) MANGLE_RESULT_TYPE(_t, _e);

/**
 * @brief Declare Result<T, E> struct definition
 *
 * Declares the actual struct layout for the Result type.
 * Uses implementation macro for consistent struct shape.
 *
 * Performance: O(1) compile time
 * Memory layout: sizeof(bool) + max(sizeof(_t), sizeof(_e)) + padding (union)
 *
 * Example:
 * DECLARE_RESULT_STRUCT(int, error)
 * // Generates: struct result_int_error_s { bool is_ok; union { int ok; error err; }; };
 *
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_STRUCT(_t, _e) \
    struct MANGLE_RESULT_STRUCT_TAG(_t, _e) IMPL_RESULT_STRUCT(_t, _e);

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare Ok(value) constructor function
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier (e.g., static inline, extern, empty)
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_OK(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_OK(_t, _e)(_t v);

/**
 * @brief Declare Err(error) constructor function
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_ERR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_ERR(_t, _e)(_e err);

/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare is_ok() query function
 *
 * Performance: O(1) runtime when defined (single boolean read)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_IS_OK(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_IS_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/**
 * @brief Declare is_err() query function
 *
 * Performance: O(1) runtime when defined (single boolean read)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_IS_ERR(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_IS_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare get_ok() safe extraction function
 *
 * Performance: O(1) runtime when defined (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_GET_OK(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_GET_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t* out);

/**
 * @brief Declare get_err() safe extraction function
 *
 * Performance: O(1) runtime when defined (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_GET_ERR(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_GET_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _e* out);

/**
 * @brief Declare unwrap_or() with fallback function
 *
 * Performance: O(1) runtime when defined (conditional return)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_UNWRAP_OR(_linkage, _t, _e) \
    _linkage _t MANGLE_RESULT_UNWRAP_OR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t fallback);

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare unwrap() unsafe extraction function
 *
 * Performance: O(1) runtime when defined (assertion + union access)
 * Contract: Crashes if Result is Err
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_UNWRAP(_linkage, _t, _e) \
    _linkage _t MANGLE_RESULT_UNWRAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/**
 * @brief Declare unwrap_err() error extraction function
 *
 * Performance: O(1) runtime when defined (assertion + union access)
 * Contract: Crashes if Result is Ok
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_UNWRAP_ERR(_linkage, _t, _e) \
    _linkage _e MANGLE_RESULT_UNWRAP_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/**
 * @brief Declare expect() with message function
 *
 * Performance: O(1) runtime when defined (assertion + union access)
 * Contract: Crashes with custom message if Err
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_EXPECT(_linkage, _t, _e) \
    _linkage _t MANGLE_RESULT_EXPECT(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, const char* msg);

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION DECLARATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare map() transformation function
 *
 * Performance: O(f) runtime where f is the transformation function
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_MAP(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_MAP(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, _t (*f)(_t));

/**
 * @brief Declare map_err() error transformation function
 *
 * Performance: O(f) runtime where f is the transformation function
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_MAP_ERR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_MAP_ERR(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, _e (*f)(_e));

/**
 * @brief Declare and_then() chaining function
 *
 * Performance: O(f) runtime where f is the chained function
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_AND_THEN(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_AND_THEN(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, MANGLE_RESULT_TYPE(_t, _e) (*f)(_t));

/**
 * @brief Declare or_else() alternative provider function
 *
 * Performance: O(1) or O(fallback) depending on whether r is Ok
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_OR_ELSE(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_OR_ELSE(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, MANGLE_RESULT_TYPE(_t, _e) (*f)(_e));

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare eq() equality comparison function
 *
 * Performance: O(1) for Ok-Err/Err-Ok, O(eq_ok) or O(eq_err) otherwise
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_EQ(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_EQ(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r1, MANGLE_RESULT_TYPE(_t, _e) r2, \
        bool (*eq_ok)(_t, _t), bool (*eq_err)(_e, _e));

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS (DECLARE MULTIPLE AT ONCE)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare all Result functions (no type/struct)
 *
 * Declares all function signatures for Result<T, E>.
 * Does not declare the type itself - use with DECLARE_RESULT_TYPEDEF/STRUCT.
 *
 * Performance: O(1) compile time
 *
 * @param _linkage Linkage specifier (e.g., extern, static inline)
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_FUNCTIONS(_linkage, _t, _e) \
    DECLARE_RESULT_OK(_linkage, _t, _e) \
    DECLARE_RESULT_ERR(_linkage, _t, _e) \
    DECLARE_RESULT_IS_OK(_linkage, _t, _e) \
    DECLARE_RESULT_IS_ERR(_linkage, _t, _e) \
    DECLARE_RESULT_GET_OK(_linkage, _t, _e) \
    DECLARE_RESULT_GET_ERR(_linkage, _t, _e) \
    DECLARE_RESULT_UNWRAP_OR(_linkage, _t, _e) \
    DECLARE_RESULT_UNWRAP(_linkage, _t, _e) \
    DECLARE_RESULT_UNWRAP_ERR(_linkage, _t, _e) \
    DECLARE_RESULT_EXPECT(_linkage, _t, _e) \
    DECLARE_RESULT_MAP(_linkage, _t, _e) \
    DECLARE_RESULT_MAP_ERR(_linkage, _t, _e) \
    DECLARE_RESULT_AND_THEN(_linkage, _t, _e) \
    DECLARE_RESULT_OR_ELSE(_linkage, _t, _e) \
    DECLARE_RESULT_EQ(_linkage, _t, _e)

/**
 * @brief Declare complete Result<T, E> type and all functions
 *
 * One-shot macro to declare everything needed for Result<T, E>:
 * - Type definition (typedef)
 * - Struct definition
 * - All function signatures
 *
 * Performance: O(1) compile time
 *
 * Example usage in header file:
 * ```c
 * // my_types.h
 * #include "result_decl.h"
 * typedef enum { ERR_NONE, ERR_IO } error;
 * DECLARE_RESULT_ALL(extern, int, error)
 * ```
 *
 * @param _linkage Linkage specifier (e.g., extern for .h, static inline for header-only)
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_ALL(_linkage, _t, _e) \
    DECLARE_RESULT_TYPEDEF(_t, _e) \
    DECLARE_RESULT_STRUCT(_t, _e) \
    DECLARE_RESULT_FUNCTIONS(_linkage, _t, _e)

/* ════════════════════════════════════════════════════════════════════════════
   USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
// ────────────────────────────────────────────────────────────────────────────
// Example 1: Separate compilation (classic .h/.c model)
// ────────────────────────────────────────────────────────────────────────────

// my_results.h - Header file
#ifndef MY_RESULTS_H
#define MY_RESULTS_H

#include "result_decl.h"

typedef enum { ERR_NONE, ERR_IO, ERR_PARSE } error;

// Declare for external linkage
DECLARE_RESULT_ALL(extern, int, error)
DECLARE_RESULT_ALL(extern, float, error)

#endif

// my_results.c - Source file
#include "my_results.h"
#include "result_defn.h"

// Define once in .c file (no linkage keyword for extern)
DEFINE_RESULT_ALL(, int, error)
DEFINE_RESULT_ALL(, float, error)

// main.c - Usage
#include "my_results.h"

int main(void) {
    result_int_error x = result_int_error_ok(42);  // Links to my_results.c
    return 0;
}

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Header-only library (traditional Canon-C style)
// ────────────────────────────────────────────────────────────────────────────

// my_lib.h - Header-only
#ifndef MY_LIB_H
#define MY_LIB_H

#include "result_decl.h"
#include "result_defn.h"

typedef enum { ERR_NONE, ERR_FAIL } error;

// Declare AND define with static inline
DECLARE_RESULT_ALL(static inline, int, error)
DEFINE_RESULT_ALL(static inline, int, error)

#endif

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Selective declaration (only declare what you need)
// ────────────────────────────────────────────────────────────────────────────

// minimal_api.h
#include "result_decl.h"

typedef enum { ERR_NONE } error;

// Only expose the type and basic operations
DECLARE_RESULT_TYPEDEF(int, error)
DECLARE_RESULT_STRUCT(int, error)
DECLARE_RESULT_OK(extern, int, error)
DECLARE_RESULT_ERR(extern, int, error)
DECLARE_RESULT_IS_OK(extern, int, error)
// (Don't expose unwrap, map, etc. - keep API minimal)
*/

#endif /* CANON_RESULT_DECL_H */
