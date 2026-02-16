// semantics/option/option_decl.h
#ifndef CANON_OPTION_DECL_H
#define CANON_OPTION_DECL_H

#ifndef CANON_OPTION_MANGLE_H
#  include "option_mangle.h"
#endif

#ifndef CANON_OPTION_IMPL_H
#  include "option_impl.h"
#endif

/**
 * @file option_decl.h
 * @brief Declaration macros for Option<T> type (separate compilation support)
 *
 * This file provides macros for declaring Option types and functions without
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
 *
 * Use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Separate compilation: Declare in .h, define in .c (faster builds)
 * 2. Shared library APIs: Export Option types across library boundaries
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
 * - Requires C99 or later (inline keyword, bool type)
 * - Works with all C99+ compilers
 * - Linkage attributes are compiler-specific (use _linkage parameter)
 *
 * Typical workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * // my_types.h - Header file
 * #include "option_decl.h"
 * DECLARE_OPTION_ALL(extern, int)  // Declare for external linkage
 * 
 * // my_types.c - Source file
 * #include "option_defn.h"
 * DEFINE_OPTION_ALL(/* no linkage */, int)  // Define once
 * 
 * // main.c - Usage
 * #include "my_types.h"
 * option_int x = option_int_some(42);  // Links to my_types.c definition
 *
 * @sa option_defn.h, option_mangle.h, option_impl.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   TYPE DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare Option<T> typedef
 *
 * Creates a typedef for the Option type using the mangled name.
 * This allows using option_T as a type name without struct keyword.
 *
 * Performance: O(1) compile time
 *
 * Example:
 * DECLARE_OPTION_TYPEDEF(int)
 * // Generates: typedef struct option_int_s option_int;
 *
 * @param _t The value type
 */
#define DECLARE_OPTION_TYPEDEF(_t) \
    typedef struct MANGLE_OPTION_STRUCT_TAG(_t) MANGLE_OPTION_TYPE(_t);

/**
 * @brief Declare Option<T> struct definition
 *
 * Declares the actual struct layout for the Option type.
 * Uses implementation macro for consistent struct shape.
 *
 * Performance: O(1) compile time
 * Memory layout: sizeof(bool) + sizeof(_t) + alignment padding
 *
 * Example:
 * DECLARE_OPTION_STRUCT(int)
 * // Generates: struct option_int_s { bool has_value; int value; };
 *
 * @param _t The value type
 */
#define DECLARE_OPTION_STRUCT(_t) \
    struct MANGLE_OPTION_STRUCT_TAG(_t) IMPL_OPTION_STRUCT(_t);

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare Some(value) constructor function
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier (e.g., static inline, extern, empty)
 * @param _t The value type
 */
#define DECLARE_OPTION_SOME(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_SOME(_t)(_t v);

/**
 * @brief Declare None constructor function
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_NONE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_NONE(_t)(void);

/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare is_some() query function
 *
 * Performance: O(1) runtime when defined (single boolean read)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_IS_SOME(_linkage, _t) \
    _linkage bool MANGLE_OPTION_IS_SOME(_t)(MANGLE_OPTION_TYPE(_t) o);

/**
 * @brief Declare is_none() query function
 *
 * Performance: O(1) runtime when defined (single boolean read)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_IS_NONE(_linkage, _t) \
    _linkage bool MANGLE_OPTION_IS_NONE(_t)(MANGLE_OPTION_TYPE(_t) o);

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare get() safe extraction function
 *
 * Performance: O(1) runtime when defined (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_GET(_linkage, _t) \
    _linkage bool MANGLE_OPTION_GET(_t)(MANGLE_OPTION_TYPE(_t) o, _t* out);

/**
 * @brief Declare unwrap_or() with fallback function
 *
 * Performance: O(1) runtime when defined (conditional return)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_UNWRAP_OR(_linkage, _t) \
    _linkage _t MANGLE_OPTION_UNWRAP_OR(_t)(MANGLE_OPTION_TYPE(_t) o, _t fallback);

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare unwrap() unsafe extraction function
 *
 * Performance: O(1) runtime when defined (assertion + field access)
 * Contract: Crashes if Option is None
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_UNWRAP(_linkage, _t) \
    _linkage _t MANGLE_OPTION_UNWRAP(_t)(MANGLE_OPTION_TYPE(_t) o);

/**
 * @brief Declare expect() with message function
 *
 * Performance: O(1) runtime when defined (assertion + field access)
 * Contract: Crashes with custom message if None
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_EXPECT(_linkage, _t) \
    _linkage _t MANGLE_OPTION_EXPECT(_t)(MANGLE_OPTION_TYPE(_t) o, const char* msg);

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
 */
#define DECLARE_OPTION_MAP(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_MAP(_t)( \
        MANGLE_OPTION_TYPE(_t) o, _t (*f)(_t));

/**
 * @brief Declare and_then() chaining function
 *
 * Performance: O(f) runtime where f is the chained function
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_AND_THEN(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_AND_THEN(_t)( \
        MANGLE_OPTION_TYPE(_t) o, MANGLE_OPTION_TYPE(_t) (*f)(_t));

/**
 * @brief Declare or_else() alternative provider function
 *
 * Performance: O(1) or O(fallback) depending on whether o is Some
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_OR_ELSE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_OR_ELSE(_t)( \
        MANGLE_OPTION_TYPE(_t) o, MANGLE_OPTION_TYPE(_t) (*fallback)(void));

/**
 * @brief Declare filter() conditional keeping function
 *
 * Performance: O(pred) runtime where pred is the predicate function
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_FILTER(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_FILTER(_t)( \
        MANGLE_OPTION_TYPE(_t) o, bool (*pred)(_t));

/**
 * @brief Declare zip() combining function
 *
 * Performance: O(combine) runtime where combine is the combining function
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_ZIP(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_ZIP(_t)( \
        MANGLE_OPTION_TYPE(_t) o1, MANGLE_OPTION_TYPE(_t) o2, \
        _t (*combine)(_t, _t));

/* ════════════════════════════════════════════════════════════════════════════
   MUTATION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare replace() mutation function
 *
 * Performance: O(1) runtime when defined (swap operation)
 * Contract: Requires non-NULL pointer
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_REPLACE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_REPLACE(_t)( \
        MANGLE_OPTION_TYPE(_t)* o, _t new_value);

/**
 * @brief Declare take() extraction function
 *
 * Performance: O(1) runtime when defined (swap operation)
 * Contract: Requires non-NULL pointer
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_TAKE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_TAKE(_t)( \
        MANGLE_OPTION_TYPE(_t)* o);

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare eq() equality comparison function
 *
 * Performance: O(1) for None-None, O(eq) for Some-Some
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DECLARE_OPTION_EQ(_linkage, _t) \
    _linkage bool MANGLE_OPTION_EQ(_t)( \
        MANGLE_OPTION_TYPE(_t) o1, MANGLE_OPTION_TYPE(_t) o2, \
        bool (*eq)(_t, _t));

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS (DECLARE MULTIPLE AT ONCE)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare all Option functions (no type/struct)
 *
 * Declares all function signatures for Option<T>.
 * Does not declare the type itself - use with DECLARE_OPTION_TYPEDEF/STRUCT.
 *
 * Performance: O(1) compile time
 *
 * @param _linkage Linkage specifier (e.g., extern, static inline)
 * @param _t The value type
 */
#define DECLARE_OPTION_FUNCTIONS(_linkage, _t) \
    DECLARE_OPTION_SOME(_linkage, _t) \
    DECLARE_OPTION_NONE(_linkage, _t) \
    DECLARE_OPTION_IS_SOME(_linkage, _t) \
    DECLARE_OPTION_IS_NONE(_linkage, _t) \
    DECLARE_OPTION_GET(_linkage, _t) \
    DECLARE_OPTION_UNWRAP_OR(_linkage, _t) \
    DECLARE_OPTION_UNWRAP(_linkage, _t) \
    DECLARE_OPTION_EXPECT(_linkage, _t) \
    DECLARE_OPTION_MAP(_linkage, _t) \
    DECLARE_OPTION_AND_THEN(_linkage, _t) \
    DECLARE_OPTION_OR_ELSE(_linkage, _t) \
    DECLARE_OPTION_FILTER(_linkage, _t) \
    DECLARE_OPTION_ZIP(_linkage, _t) \
    DECLARE_OPTION_REPLACE(_linkage, _t) \
    DECLARE_OPTION_TAKE(_linkage, _t) \
    DECLARE_OPTION_EQ(_linkage, _t)

/**
 * @brief Declare complete Option<T> type and all functions
 *
 * One-shot macro to declare everything needed for Option<T>:
 * - Type definition (typedef)
 * - Struct definition
 * - All function signatures
 *
 * Performance: O(1) compile time
 *
 * Example usage in header file:
 * ```c
 * // my_types.h
 * #include "option_decl.h"
 * DECLARE_OPTION_ALL(extern, int)
 * ```
 *
 * @param _linkage Linkage specifier (e.g., extern for .h, static inline for header-only)
 * @param _t The value type
 */
#define DECLARE_OPTION_ALL(_linkage, _t) \
    DECLARE_OPTION_TYPEDEF(_t) \
    DECLARE_OPTION_STRUCT(_t) \
    DECLARE_OPTION_FUNCTIONS(_linkage, _t)

/* ════════════════════════════════════════════════════════════════════════════
   USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
// ────────────────────────────────────────────────────────────────────────────
// Example 1: Separate compilation (classic .h/.c model)
// ────────────────────────────────────────────────────────────────────────────

// my_options.h - Header file
#ifndef MY_OPTIONS_H
#define MY_OPTIONS_H

#include "option_decl.h"

// Declare for external linkage
DECLARE_OPTION_ALL(extern, int)
DECLARE_OPTION_ALL(extern, float)

#endif

// my_options.c - Source file
#include "my_options.h"
#include "option_defn.h"

// Define once in .c file (no linkage keyword for extern)
DEFINE_OPTION_ALL(, int)
DEFINE_OPTION_ALL(, float)

// main.c - Usage
#include "my_options.h"

int main(void) {
    option_int x = option_int_some(42);  // Links to my_options.c
    return 0;
}

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Header-only library (traditional Canon-C style)
// ────────────────────────────────────────────────────────────────────────────

// my_lib.h - Header-only
#ifndef MY_LIB_H
#define MY_LIB_H

#include "option_decl.h"
#include "option_defn.h"

// Declare AND define with static inline
DECLARE_OPTION_ALL(static inline, int)
DEFINE_OPTION_ALL(static inline, int)

#endif

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Selective declaration (only declare what you need)
// ────────────────────────────────────────────────────────────────────────────

// minimal_api.h
#include "option_decl.h"

// Only expose the type and basic operations
DECLARE_OPTION_TYPEDEF(int)
DECLARE_OPTION_STRUCT(int)
DECLARE_OPTION_SOME(extern, int)
DECLARE_OPTION_NONE(extern, int)
DECLARE_OPTION_IS_SOME(extern, int)
// (Don't expose unwrap, map, etc. - keep API minimal)
*/

#endif /* CANON_OPTION_DECL_H */
