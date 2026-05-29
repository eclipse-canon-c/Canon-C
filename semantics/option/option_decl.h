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
 * - Explicit contracts: NULL pointer is always a caller bug, not a condition
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
 * - bool and stddef types provided by canon primitives/types.h (via option_impl.h)
 * - Works with GCC, Clang, MSVC (C99 mode or later)
 * - Linkage attributes are caller-specified via _linkage parameter
 *
 * Typical workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * // my_types.h - Header file
 * #include "option_decl.h"
 * DECLARE_OPTION_ALL(extern, int)
 *
 * // my_types.c - Source file
 * #include "my_types.h"
 * #include "option_defn.h"
 * DEFINE_OPTION_ALL(, int)          // empty linkage = define once, externally visible
 *
 * // main.c - Usage
 * #include "my_types.h"
 * option_int x = option_int_some(42);   // links to my_types.c definition
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
 * Allows using option_T as a type name without the struct keyword.
 *
 * Performance: O(1) compile time
 *
 * Example:
 *   DECLARE_OPTION_TYPEDEF(int)
 *   // Expands to: typedef struct option_int_s option_int;
 *
 * @param _t The value type
 */
#define DECLARE_OPTION_TYPEDEF(_t) \
    typedef struct MANGLE_OPTION_STRUCT_TAG(_t) MANGLE_OPTION_TYPE(_t);

/**
 * @brief Declare Option<T> struct definition
 *
 * Declares the actual struct layout for the Option type.
 * Uses the implementation macro for a consistent struct shape across
 * all compilation units.
 *
 * Performance: O(1) compile time
 * Memory layout: { bool has_value; _t value; } + alignment padding
 *
 * Note: The value field is always present in the struct regardless of
 * has_value. Its contents are unspecified when has_value is false.
 * Do not read value without checking has_value first.
 *
 * Example:
 *   DECLARE_OPTION_STRUCT(int)
 *   // Expands to: struct option_int_s { bool has_value; int value; };
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
 * Wraps a value in an Option. The resulting Option is guaranteed to
 * satisfy is_some().
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier (e.g., static inline, extern, or empty)
 * @param _t       The value type
 */
#define DECLARE_OPTION_SOME(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_SOME(_t)(_t v);

/**
 * @brief Declare None constructor function
 *
 * Constructs an empty Option. The resulting Option is guaranteed to
 * satisfy is_none().
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier (e.g., static inline, extern, or empty)
 * @param _t       The value type
 */
#define DECLARE_OPTION_NONE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_NONE(_t)(void);

/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare is_some() query function
 *
 * Returns true if the Option contains a value.
 *
 * Performance: O(1) runtime when defined (single boolean read)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_IS_SOME(_linkage, _t) \
    _linkage bool MANGLE_OPTION_IS_SOME(_t)(MANGLE_OPTION_TYPE(_t) o);

/**
 * @brief Declare is_none() query function
 *
 * Returns true if the Option is empty.
 *
 * Performance: O(1) runtime when defined (single boolean read)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_IS_NONE(_linkage, _t) \
    _linkage bool MANGLE_OPTION_IS_NONE(_t)(MANGLE_OPTION_TYPE(_t) o);

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare get() safe extraction function
 *
 * Writes the contained value into *out and returns true if the Option is
 * Some. Returns false and leaves *out unmodified if the Option is None.
 *
 * Contract: out must not be NULL. Passing NULL is a caller bug and will
 * trigger a contract violation. Use is_some() + unwrap() if you do not
 * need an out-parameter pattern, or unwrap_or() for a safe default.
 *
 * Performance: O(1) runtime when defined (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_GET(_linkage, _t) \
    _linkage bool MANGLE_OPTION_GET(_t)(MANGLE_OPTION_TYPE(_t) o, _t* out);

/**
 * @brief Declare unwrap_or() with fallback function
 *
 * Returns the contained value if Some, otherwise returns fallback.
 * Never panics — safe alternative to unwrap() in production paths.
 *
 * Performance: O(1) runtime when defined (conditional return)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_UNWRAP_OR(_linkage, _t) \
    _linkage _t MANGLE_OPTION_UNWRAP_OR(_t)(MANGLE_OPTION_TYPE(_t) o, _t fallback);

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare unwrap() unsafe extraction function
 *
 * Returns the contained value. Triggers a contract violation and aborts
 * if the Option is None.
 *
 * ⚠️  Only use when invariant analysis guarantees Some at the call site.
 *     Prefer get(), unwrap_or(), or expect() in all other cases.
 *
 * Performance: O(1) runtime when defined (assertion + field access)
 * Contract: Aborts with a diagnostic message if Option is None
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_UNWRAP(_linkage, _t) \
    _linkage _t MANGLE_OPTION_UNWRAP(_t)(MANGLE_OPTION_TYPE(_t) o);

/**
 * @brief Declare expect() with message function
 *
 * Returns the contained value. Triggers a contract violation and aborts
 * with the caller-supplied message if the Option is None.
 *
 * Prefer over unwrap() when the call site has meaningful context to
 * include in the abort message.
 *
 * Performance: O(1) runtime when defined (assertion + field access)
 * Contract: Aborts with msg if Option is None
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_EXPECT(_linkage, _t) \
    _linkage _t MANGLE_OPTION_EXPECT(_t)(MANGLE_OPTION_TYPE(_t) o, const char* msg);

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION DECLARATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare map() transformation function
 *
 * Applies f to the contained value and returns Some(f(value)) if Some.
 * Returns None without calling f if the Option is None.
 *
 * Performance: O(f) runtime where f is the transformation function
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_MAP(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_MAP(_t)( \
        MANGLE_OPTION_TYPE(_t) o, _t (*f)(_t));

/**
 * @brief Declare and_then() chaining function
 *
 * Applies f (which itself returns an Option) to the contained value if
 * Some. Returns None without calling f if the Option is None. Prevents
 * nested Option<Option<T>>.
 *
 * Performance: O(f) runtime where f is the chained function
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_AND_THEN(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_AND_THEN(_t)( \
        MANGLE_OPTION_TYPE(_t) o, MANGLE_OPTION_TYPE(_t) (*f)(_t));

/**
 * @brief Declare or_else() alternative provider function
 *
 * Returns o unchanged if Some. If None, calls fallback() and returns
 * its result. fallback is not called when o is Some.
 *
 * Performance: O(1) if Some, O(fallback) if None
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_OR_ELSE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_OR_ELSE(_t)( \
        MANGLE_OPTION_TYPE(_t) o, MANGLE_OPTION_TYPE(_t) (*fallback)(void));

/**
 * @brief Declare filter() conditional keeping function
 *
 * Returns o unchanged if Some and pred(value) is true.
 * Returns None if the Option is None or pred(value) is false.
 *
 * Performance: O(pred) runtime where pred is the predicate function
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_FILTER(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_FILTER(_t)( \
        MANGLE_OPTION_TYPE(_t) o, bool (*pred)(_t));

/**
 * @brief Declare combine_with() same-type combining function
 *
 * Returns Some(combine(a, b)) if both o1 and o2 are Some.
 * Returns None if either o1 or o2 is None. combine is not called
 * unless both Options are Some.
 *
 * Both operands and the result share the same type T.  For combining two
 * different Option types into a third, write a bespoke combinator.
 *
 * Performance: O(combine) runtime where combine is the combining function
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_COMBINE_WITH(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_COMBINE_WITH(_t)( \
        MANGLE_OPTION_TYPE(_t) o1, MANGLE_OPTION_TYPE(_t) o2, \
        _t (*combine)(_t, _t));

/* ════════════════════════════════════════════════════════════════════════════
   MUTATION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare replace() mutation function
 *
 * Sets *o to Some(new_value) and returns the previous Option value.
 * Useful for atomic swap-and-inspect patterns.
 *
 * Performance: O(1) runtime when defined (swap operation)
 * Contract: o must not be NULL — passing NULL is always a caller bug
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_REPLACE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) MANGLE_OPTION_REPLACE(_t)( \
        MANGLE_OPTION_TYPE(_t)* o, _t new_value);

/**
 * @brief Declare take() extraction function
 *
 * Returns the current value of *o and sets *o to None. Useful for
 * moving values out of Option containers.
 *
 * Performance: O(1) runtime when defined (swap operation)
 * Contract: o must not be NULL — passing NULL is always a caller bug
 * Postcondition: *o is None after this call regardless of prior state
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
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
 * Two Options are equal if:
 *   - Both are None, OR
 *   - Both are Some and eq(a, b) returns true
 * eq is not called when either Option is None.
 *
 * Performance: O(1) for None-None, O(eq) for Some-Some
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 */
#define DECLARE_OPTION_EQ(_linkage, _t) \
    _linkage bool MANGLE_OPTION_EQ(_t)( \
        MANGLE_OPTION_TYPE(_t) o1, MANGLE_OPTION_TYPE(_t) o2, \
        bool (*eq)(_t, _t));

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS (DECLARE MULTIPLE AT ONCE)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare all Option functions (no type or struct)
 *
 * Declares every function signature for Option<T> with the given
 * linkage specifier. Does not emit the typedef or struct — pair with
 * DECLARE_OPTION_TYPEDEF and DECLARE_OPTION_STRUCT when needed, or
 * use DECLARE_OPTION_ALL to get everything in one call.
 *
 * Performance: O(1) compile time
 *
 * @param _linkage Linkage specifier (e.g., extern, static inline, or empty)
 * @param _t       The value type
 */
#define DECLARE_OPTION_FUNCTIONS(_linkage, _t) \
    DECLARE_OPTION_SOME(_linkage, _t)          \
    DECLARE_OPTION_NONE(_linkage, _t)          \
    DECLARE_OPTION_IS_SOME(_linkage, _t)       \
    DECLARE_OPTION_IS_NONE(_linkage, _t)       \
    DECLARE_OPTION_GET(_linkage, _t)           \
    DECLARE_OPTION_UNWRAP_OR(_linkage, _t)     \
    DECLARE_OPTION_UNWRAP(_linkage, _t)        \
    DECLARE_OPTION_EXPECT(_linkage, _t)        \
    DECLARE_OPTION_MAP(_linkage, _t)           \
    DECLARE_OPTION_AND_THEN(_linkage, _t)      \
    DECLARE_OPTION_OR_ELSE(_linkage, _t)       \
    DECLARE_OPTION_FILTER(_linkage, _t)        \
    DECLARE_OPTION_COMBINE_WITH(_linkage, _t)  \
    DECLARE_OPTION_REPLACE(_linkage, _t)       \
    DECLARE_OPTION_TAKE(_linkage, _t)          \
    DECLARE_OPTION_EQ(_linkage, _t)

/**
 * @brief Declare complete Option<T> type and all functions
 *
 * One-shot macro to declare everything needed to use Option<T> from a
 * header:
 *   - typedef (option_T)
 *   - struct definition (struct option_T_s)
 *   - All function signatures
 *
 * Pair with DEFINE_OPTION_ALL in exactly one .c file when using extern
 * linkage. For header-only use, pass static inline as _linkage and
 * include option_defn.h in the same header.
 *
 * Performance: O(1) compile time
 *
 * @param _linkage Linkage specifier (extern for .h, static inline for
 *                 header-only, empty to define with external linkage
 *                 in a .c file)
 * @param _t       The value type
 */
#define DECLARE_OPTION_ALL(_linkage, _t) \
    DECLARE_OPTION_TYPEDEF(_t)           \
    DECLARE_OPTION_STRUCT(_t)            \
    DECLARE_OPTION_FUNCTIONS(_linkage, _t)

/* ════════════════════════════════════════════════════════════════════════════
   USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
// ────────────────────────────────────────────────────────────────────────────
// Example 1: Separate compilation (classic .h/.c model)
// ────────────────────────────────────────────────────────────────────────────

// my_options.h
#ifndef MY_OPTIONS_H
#define MY_OPTIONS_H

#include "option_decl.h"

DECLARE_OPTION_ALL(extern, int)
DECLARE_OPTION_ALL(extern, float)

#endif

// my_options.c
#include "my_options.h"
#include "option_defn.h"

DEFINE_OPTION_ALL(, int)    // empty linkage: externally visible, defined once
DEFINE_OPTION_ALL(, float)

// main.c
#include "my_options.h"

int main(void) {
    option_int x = option_int_some(42);
    int v;
    if (option_int_get(x, &v)) {  // &v must not be NULL
        // use v
    }
    return 0;
}

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Header-only (static inline, single translation unit)
// ────────────────────────────────────────────────────────────────────────────

// my_lib.h
#ifndef MY_LIB_H
#define MY_LIB_H

#include "option_decl.h"
#include "option_defn.h"

DECLARE_OPTION_ALL(static inline, int)
DEFINE_OPTION_ALL(static inline, int)

#endif

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Minimal API surface (selective declaration)
// ────────────────────────────────────────────────────────────────────────────

// minimal_api.h
#include "option_decl.h"

DECLARE_OPTION_TYPEDEF(int)
DECLARE_OPTION_STRUCT(int)
DECLARE_OPTION_SOME(extern, int)
DECLARE_OPTION_NONE(extern, int)
DECLARE_OPTION_IS_SOME(extern, int)
DECLARE_OPTION_GET(extern, int)
// combine_with, map, filter, etc. not exposed from this API surface.
*/

#endif /* CANON_OPTION_DECL_H */
