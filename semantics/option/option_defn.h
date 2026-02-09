// semantics/option/option_defn.h
#ifndef CANON_OPTION_DEFN_H
#define CANON_OPTION_DEFN_H

#ifndef CANON_OPTION_MANGLE_H
#  include "option_mangle.h"
#endif

#ifndef CANON_OPTION_IMPL_H
#  include "option_impl.h"
#endif

#include <stdbool.h>

/**
 * @file option_defn.h
 * @brief Definition macros for Option<T> type (implementation generation)
 *
 * This file provides macros for defining Option types and functions with actual
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
 * - Space: sizeof(bool) + sizeof(T) + alignment padding
 * - Inlining: static inline eliminates function call overhead
 * - No allocations: Pure stack-based operations
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Independent Option instances are thread-safe
 * - No shared mutable state
 * - Mutation operations (replace, take) require external synchronization
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline, compound literals, designated initializers)
 * - Works with all C99+ compilers
 * - Linkage attributes are compiler-specific (use _linkage parameter)
 *
 * Typical workflow:
 * ────────────────────────────────────────────────────────────────────────────
 * // Header-only style (traditional Canon-C)
 * #include "option_defn.h"
 * DEFINE_OPTION_ALL(static inline, int)
 * 
 * // Separate compilation style
 * // In .h: DECLARE_OPTION_ALL(extern, int)
 * // In .c: DEFINE_OPTION_ALL(, int)  // No linkage keyword for extern
 *
 * @sa option_decl.h, option_mangle.h, option_impl.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   TYPE DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define Option<T> struct
 *
 * Generates the actual struct definition with memory layout.
 * Must be defined exactly once per type (unless static inline).
 *
 * Performance: O(1) compile time
 * Memory layout: sizeof(bool) + sizeof(_t) + alignment padding
 *
 * Example:
 * DEFINE_OPTION_STRUCT(int)
 * // Generates: struct option_int_s { bool has_value; int value; };
 *
 * @param _t The value type
 */
#define DEFINE_OPTION_STRUCT(_t) \
    struct MANGLE_OPTION_STRUCT_TAG(_t) IMPL_OPTION_STRUCT(_t);

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define Some(value) constructor function
 *
 * Generates function that wraps a value in Option<T>.
 *
 * Performance: O(1) time, O(1) space (stack allocation via compound literal)
 * Returns: Option containing the value
 *
 * @param _linkage Linkage specifier (e.g., static inline, empty for extern)
 * @param _t The value type
 */
#define DEFINE_OPTION_SOME(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_SOME(_t)(_t v) \
        IMPL_OPTION_SOME(_t, MANGLE_OPTION_TYPE(_t), v)

/**
 * @brief Define None constructor function
 *
 * Generates function that creates an empty Option<T>.
 *
 * Performance: O(1) time, O(1) space (stack allocation via compound literal)
 * Returns: Empty Option
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_NONE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_NONE(_t)(void) \
        IMPL_OPTION_NONE(_t, MANGLE_OPTION_TYPE(_t))

/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define is_some() query function
 *
 * Generates function that checks if Option contains a value.
 *
 * Performance: O(1) time, O(1) space (single boolean read)
 * Returns: true if Some(value), false if None
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_IS_SOME(_linkage, _t) \
    _linkage bool \
    MANGLE_OPTION_IS_SOME(_t)(MANGLE_OPTION_TYPE(_t) o) \
        IMPL_OPTION_IS_SOME(_t, o)

/**
 * @brief Define is_none() query function
 *
 * Generates function that checks if Option is empty.
 *
 * Performance: O(1) time, O(1) space (single boolean negation)
 * Returns: true if None, false if Some(value)
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_IS_NONE(_linkage, _t) \
    _linkage bool \
    MANGLE_OPTION_IS_NONE(_t)(MANGLE_OPTION_TYPE(_t) o) \
        IMPL_OPTION_IS_NONE(_t, o)

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define get() safe extraction function
 *
 * Generates function that safely extracts value via pointer.
 * Permissive: NULL pointer is allowed, returns false.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 * Returns: true if value extracted, false if None or NULL pointer
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_GET(_linkage, _t) \
    _linkage bool \
    MANGLE_OPTION_GET(_t)(MANGLE_OPTION_TYPE(_t) o, _t* out) \
        IMPL_OPTION_GET(_t, o, out)

/**
 * @brief Define unwrap_or() with fallback function
 *
 * Generates function that extracts value or returns fallback.
 * Safe alternative to unwrap() - never panics.
 *
 * Performance: O(1) time, O(1) space (conditional return)
 * Returns: Contained value if Some, fallback if None
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_UNWRAP_OR(_linkage, _t) \
    _linkage _t \
    MANGLE_OPTION_UNWRAP_OR(_t)(MANGLE_OPTION_TYPE(_t) o, _t fallback) \
        IMPL_OPTION_UNWRAP_OR(_t, o, fallback)

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define unwrap() unsafe extraction function
 *
 * Generates function that extracts value, panicking if None.
 * ⚠️ WARNING: Only use when certain value is present!
 *
 * Performance: O(1) time, O(1) space (assertion + field access)
 * Contract: Crashes with clear message if Option is None
 * Returns: Contained value
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_UNWRAP(_linkage, _t) \
    _linkage _t \
    MANGLE_OPTION_UNWRAP(_t)(MANGLE_OPTION_TYPE(_t) o) \
        IMPL_OPTION_UNWRAP(_t, o)

/**
 * @brief Define expect() with message function
 *
 * Generates function that extracts value with custom panic message.
 * Like unwrap(), but with descriptive error message.
 *
 * Performance: O(1) time, O(1) space (assertion + field access)
 * Contract: Crashes with custom message if None
 * Returns: Contained value
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_EXPECT(_linkage, _t) \
    _linkage _t \
    MANGLE_OPTION_EXPECT(_t)(MANGLE_OPTION_TYPE(_t) o, const char* msg) \
        IMPL_OPTION_EXPECT(_t, o, msg)

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION DEFINITIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define map() transformation function
 *
 * Generates function that transforms contained value if present.
 * If None, returns None without calling transformation function.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 * Returns: Some(f(value)) if Some(value), None if None
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_MAP(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_MAP(_t)(MANGLE_OPTION_TYPE(_t) o, _t (*f)(_t)) \
        IMPL_OPTION_MAP(_t, MANGLE_OPTION_TYPE(_t), o, f, \
                        MANGLE_OPTION_SOME(_t), MANGLE_OPTION_NONE(_t))

/**
 * @brief Define and_then() chaining function
 *
 * Generates function that chains Option-returning operations.
 * Like map(), but f returns Option instead of plain value.
 * Prevents nested Option<Option<T>>.
 *
 * Performance: O(f) time where f is the chained function
 *              O(1) space (no nesting)
 * Returns: f(value) if Some(value), None if None
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_AND_THEN(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_AND_THEN(_t)( \
        MANGLE_OPTION_TYPE(_t) o, \
        MANGLE_OPTION_TYPE(_t) (*f)(_t)) \
        IMPL_OPTION_AND_THEN(_t, MANGLE_OPTION_TYPE(_t), o, f, \
                             MANGLE_OPTION_NONE(_t))

/**
 * @brief Define or_else() alternative provider function
 *
 * Generates function that provides alternative if None.
 * Returns o if Some, otherwise calls fallback() to get alternative.
 *
 * Performance: O(1) if Some, O(fallback) if None
 *              O(1) space
 * Returns: o if Some, fallback() if None
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_OR_ELSE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_OR_ELSE(_t)( \
        MANGLE_OPTION_TYPE(_t) o, \
        MANGLE_OPTION_TYPE(_t) (*fallback)(void)) \
        IMPL_OPTION_OR_ELSE(_t, MANGLE_OPTION_TYPE(_t), o, fallback)

/**
 * @brief Define filter() conditional keeping function
 *
 * Generates function that keeps value only if predicate returns true.
 * Converts Some(value) to None if predicate(value) is false.
 *
 * Performance: O(pred) time where pred is the predicate function
 *              O(1) space
 * Returns: o if Some(v) and pred(v), None otherwise
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_FILTER(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_FILTER(_t)( \
        MANGLE_OPTION_TYPE(_t) o, bool (*pred)(_t)) \
        IMPL_OPTION_FILTER(_t, MANGLE_OPTION_TYPE(_t), o, pred, \
                           MANGLE_OPTION_NONE(_t))

/**
 * @brief Define zip() combining function
 *
 * Generates function that combines two Options with a function.
 * Returns Some(combine(a, b)) if both are Some.
 * Returns None if either is None.
 *
 * Performance: O(combine) time where combine is the combining function
 *              O(1) space
 * Returns: Some(combine(v1, v2)) if both Some, None otherwise
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_ZIP(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_ZIP(_t)( \
        MANGLE_OPTION_TYPE(_t) o1, \
        MANGLE_OPTION_TYPE(_t) o2, \
        _t (*combine)(_t, _t)) \
        IMPL_OPTION_ZIP(_t, MANGLE_OPTION_TYPE(_t), o1, o2, combine, \
                        MANGLE_OPTION_SOME(_t), MANGLE_OPTION_NONE(_t))

/* ════════════════════════════════════════════════════════════════════════════
   MUTATION FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define replace() mutation function
 *
 * Generates function that replaces value, returning old Option.
 * Strict: NULL pointer triggers contract violation.
 *
 * Performance: O(1) time, O(1) space (swap operation)
 * Contract: Requires non-NULL pointer
 * Returns: Previous Option state
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_REPLACE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_REPLACE(_t)(MANGLE_OPTION_TYPE(_t)* o, _t new_value) \
        IMPL_OPTION_REPLACE(_t, MANGLE_OPTION_TYPE(_t), o, new_value, \
                            MANGLE_OPTION_SOME(_t))

/**
 * @brief Define take() extraction function
 *
 * Generates function that takes value out, leaving None.
 * Strict: NULL pointer triggers contract violation.
 *
 * Performance: O(1) time, O(1) space (swap operation)
 * Contract: Requires non-NULL pointer
 * Postcondition: *o is None after call
 * Returns: Previous Option state
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_TAKE(_linkage, _t) \
    _linkage MANGLE_OPTION_TYPE(_t) \
    MANGLE_OPTION_TAKE(_t)(MANGLE_OPTION_TYPE(_t)* o) \
        IMPL_OPTION_TAKE(_t, MANGLE_OPTION_TYPE(_t), o, \
                         MANGLE_OPTION_NONE(_t))

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define eq() equality comparison function
 *
 * Generates function that checks equality of two Options.
 * Two Options are equal if both None, or both Some with equal values.
 *
 * Performance: O(1) for None-None comparison
 *              O(eq) for Some-Some where eq is equality function
 *              O(1) space
 * Returns: true if equal, false otherwise
 *
 * @param _linkage Linkage specifier
 * @param _t The value type
 */
#define DEFINE_OPTION_EQ(_linkage, _t) \
    _linkage bool \
    MANGLE_OPTION_EQ(_t)( \
        MANGLE_OPTION_TYPE(_t) o1, \
        MANGLE_OPTION_TYPE(_t) o2, \
        bool (*eq)(_t, _t)) \
        IMPL_OPTION_EQ(_t, o1, o2, eq)

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS (DEFINE MULTIPLE AT ONCE)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define all Option functions (no type/struct)
 *
 * Generates implementations for all Option<T> functions.
 * Does not define the struct itself - use with DEFINE_OPTION_STRUCT.
 *
 * Performance: All functions are O(1) except combinators which are O(f)
 *
 * @param _linkage Linkage specifier (e.g., static inline for header-only)
 * @param _t The value type
 */
#define DEFINE_OPTION_FUNCTIONS(_linkage, _t) \
    DEFINE_OPTION_SOME(_linkage, _t) \
    DEFINE_OPTION_NONE(_linkage, _t) \
    DEFINE_OPTION_IS_SOME(_linkage, _t) \
    DEFINE_OPTION_IS_NONE(_linkage, _t) \
    DEFINE_OPTION_GET(_linkage, _t) \
    DEFINE_OPTION_UNWRAP_OR(_linkage, _t) \
    DEFINE_OPTION_UNWRAP(_linkage, _t) \
    DEFINE_OPTION_EXPECT(_linkage, _t) \
    DEFINE_OPTION_MAP(_linkage, _t) \
    DEFINE_OPTION_AND_THEN(_linkage, _t) \
    DEFINE_OPTION_OR_ELSE(_linkage, _t) \
    DEFINE_OPTION_FILTER(_linkage, _t) \
    DEFINE_OPTION_ZIP(_linkage, _t) \
    DEFINE_OPTION_REPLACE(_linkage, _t) \
    DEFINE_OPTION_TAKE(_linkage, _t) \
    DEFINE_OPTION_EQ(_linkage, _t)

/**
 * @brief Define complete Option<T> type and all functions
 *
 * One-shot macro to define everything needed for Option<T>:
 * - Struct definition
 * - All function implementations
 *
 * Performance: All operations O(1) except combinators
 * Space: sizeof(bool) + sizeof(T) + padding per instance
 *
 * Example usage (header-only):
 * ```c
 * #include "option_defn.h"
 * DEFINE_OPTION_ALL(static inline, int)
 * 
 * option_int x = option_int_some(42);
 * ```
 *
 * Example usage (separate compilation):
 * ```c
 * // In .c file only
 * #include "option_defn.h"
 * DEFINE_OPTION_ALL(, int)  // No linkage keyword for extern
 * ```
 *
 * @param _linkage Linkage specifier (static inline for header-only, empty for extern)
 * @param _t The value type
 */
#define DEFINE_OPTION_ALL(_linkage, _t) \
    DEFINE_OPTION_STRUCT(_t) \
    DEFINE_OPTION_FUNCTIONS(_linkage, _t)

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

#include "option_defn.h"

// Define with static inline - each translation unit gets own copy
DEFINE_OPTION_ALL(static inline, int)
DEFINE_OPTION_ALL(static inline, float)

#endif

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Separate compilation for faster builds
// ────────────────────────────────────────────────────────────────────────────

// my_options.h - Header file
#ifndef MY_OPTIONS_H
#define MY_OPTIONS_H

#include "option_decl.h"

// Declare only (no definitions)
DECLARE_OPTION_ALL(extern, int)
DECLARE_OPTION_ALL(extern, float)

#endif

// my_options.c - Source file
#include "my_options.h"
#include "option_defn.h"

// Define once (empty linkage for extern)
DEFINE_OPTION_ALL(, int)
DEFINE_OPTION_ALL(, float)

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Custom implementation for pointer types
// ────────────────────────────────────────────────────────────────────────────

#include "option_impl.h"

// Override Some() to assert non-NULL
#undef IMPL_OPTION_SOME
#define IMPL_OPTION_SOME(t, topt, param) \
    { \
        require((param) != NULL, "option_some: NULL pointer not allowed"); \
        return (topt){ .has_value = true, .value = (param) }; \
    }

#include "option_defn.h"

typedef void* void_ptr;
DEFINE_OPTION_ALL(static inline, void_ptr)

// Now option_void_ptr_some(NULL) will panic at runtime
*/

#endif /* CANON_OPTION_DEFN_H */
