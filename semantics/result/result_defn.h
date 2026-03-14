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
 * Contract with result_impl.h:
 * ────────────────────────────────────────────────────────────────────────────
 * Every IMPL_RESULT_* macro used here MUST expand to a brace-enclosed
 * function body of the form { ... } with NO trailing semicolon.
 *
 * Example of a conforming IMPL macro:
 *   #define IMPL_RESULT_OK(t, e, tres, param) \
 *       { return (tres){ .is_ok = true, .ok = (param) }; }
 *
 * Rationale: DEFINE_RESULT_FUNCTIONS concatenates multiple macro expansions
 * without inserting semicolons between them. If an IMPL macro ends with ';'
 * rather than '}', the resulting token stream is malformed C. This contract
 * is enforced by convention and should be verified by the author of any
 * custom IMPL override (see Example 3 in the usage section below).
 *
 * Use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Header-only: Define with static inline in .h files
 * 2. Separate compilation: Define once in .c files (no linkage keyword)
 * 3. Shared libraries: Define with __declspec or __attribute__ for export
 * 4. Custom implementations: Override specific IMPL_* macros before including
 *
 * Duplicate definition guard:
 * ────────────────────────────────────────────────────────────────────────────
 * DEFINE_RESULT_ALL (and DEFINE_RESULT_STRUCT) emit a struct definition.
 * If two translation units both include a header that calls DEFINE_RESULT_ALL
 * without static inline, you will get a duplicate struct definition error
 * at link time. To avoid this:
 *   - Use static inline (header-only, safe for multiple TUs), OR
 *   - Use DECLARE_RESULT_ALL in the .h and DEFINE_RESULT_ALL in exactly
 *     one .c file (separate compilation model).
 * DEFINE_RESULT_STRUCT itself is not idempotent; wrapping your header in a
 * per-type include guard is the caller's responsibility.
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
 * - Requires C99 or later (inline, compound literals, designated initializers,
 *   anonymous unions)
 * - Works with GCC, Clang, and MSVC in C99 mode
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
 * Must be defined exactly once per type pair per program
 * (or use static inline / separate-compilation guards — see file-level docs).
 *
 * NOTE: IMPL_RESULT_STRUCT must NOT include a trailing semicolon.
 *       This macro appends exactly one ';' after the expansion so that the
 *       struct declaration is well-formed regardless of whether IMPL expands
 *       to "{ ... }" or "{ ... };". If IMPL already has a trailing ';' the
 *       result is a harmless extra null-declaration in most compilers, but
 *       clang/gcc -Wpedantic may warn. The canonical IMPL must therefore
 *       expand to a body without a trailing semicolon.
 *
 * Performance: O(1) compile time
 * Memory layout: sizeof(bool) + max(sizeof(_t), sizeof(_e)) + alignment padding
 *                Union saves space compared to separate fields
 *
 * Example:
 *   DEFINE_RESULT_STRUCT(int, error)
 *   // Expands to:
 *   //   struct result_int_error_s { bool is_ok; union { int ok; error err; }; };
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
 * Expansion contract: IMPL_RESULT_OK must expand to a brace-enclosed function
 * body { ... } with no trailing semicolon. See file-level contract note.
 *
 * @param _linkage Linkage specifier (e.g., static inline, empty for extern)
 * @param _t       The value type
 * @param _e       The error type
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
 * Expansion contract: IMPL_RESULT_ERR must expand to a brace-enclosed function
 * body { ... } with no trailing semicolon. See file-level contract note.
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
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
 * @param _t       The value type
 * @param _e       The error type
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
 * @param _t       The value type
 * @param _e       The error type
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
 * Permissive: NULL pointer is allowed; returns false without dereferencing.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 * Returns: true if value extracted, false if Err or NULL pointer
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DEFINE_RESULT_GET_OK(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_GET_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t* out) \
        IMPL_RESULT_GET_OK(_t, _e, r, out)

/**
 * @brief Define get_err() safe extraction function
 *
 * Generates function that safely extracts error value via pointer.
 * Permissive: NULL pointer is allowed; returns false without dereferencing.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 * Returns: true if error extracted, false if Ok or NULL pointer
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DEFINE_RESULT_GET_ERR(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_GET_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _e* out) \
        IMPL_RESULT_GET_ERR(_t, _e, r, out)

/**
 * @brief Define unwrap_or() with fallback function
 *
 * Generates function that extracts value or returns fallback.
 * Safe alternative to unwrap() — never panics.
 *
 * Performance: O(1) time, O(1) space (conditional return)
 * Returns: Contained value if Ok, fallback if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
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
 * ⚠️  WARNING: Only use when certain result is Ok.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Terminates with a diagnostic message if Result is Err.
 *           The termination mechanism is defined by CANON_PANIC in result_impl.h.
 * Returns: Contained value
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DEFINE_RESULT_UNWRAP(_linkage, _t, _e) \
    _linkage _t \
    MANGLE_RESULT_UNWRAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_UNWRAP(_t, _e, r)

/**
 * @brief Define unwrap_err() error extraction function
 *
 * Generates function that extracts error, panicking if Ok.
 * The opposite of unwrap() — extracts error from Err.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Terminates with a diagnostic message if Result is Ok.
 *           The termination mechanism is defined by CANON_PANIC in result_impl.h.
 * Returns: Contained error
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DEFINE_RESULT_UNWRAP_ERR(_linkage, _t, _e) \
    _linkage _e \
    MANGLE_RESULT_UNWRAP_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_UNWRAP_ERR(_t, _e, r)

/**
 * @brief Define expect() with message function
 *
 * Generates function that extracts value with a caller-supplied panic message.
 * Like unwrap(), but the diagnostic message is provided by the caller.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Terminates printing msg if Result is Err.
 *           The termination mechanism is defined by CANON_PANIC in result_impl.h.
 * Returns: Contained value
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DEFINE_RESULT_EXPECT(_linkage, _t, _e) \
    _linkage _t \
    MANGLE_RESULT_EXPECT(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, const char* msg) \
        IMPL_RESULT_EXPECT(_t, _e, r, msg)

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION DEFINITIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define map() same-type transformation function
 *
 * Generates a function that applies f to the contained value if Ok,
 * leaving the type unchanged (T → T).
 * If Err, returns Err unchanged without calling f.
 *
 * Limitation — same-type only:
 *   The transformation function f must have the signature _t(*)(_t), meaning
 *   it cannot change the value type. This is intentional: C's macro system
 *   cannot express a generic Result<U, E> return type without a second type
 *   parameter. To map to a different value type, define a second Result type
 *   for the target type and write a plain conversion function instead.
 *   A two-type-parameter variant (DEFINE_RESULT_MAP_INTO) is not provided
 *   here to keep the API surface minimal and explicit.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 * Returns: Ok(f(value)) if Ok(value), Err unchanged if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type (both input and output of f)
 * @param _e       The error type
 */
#define DEFINE_RESULT_MAP(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_MAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t (*f)(_t)) \
        IMPL_RESULT_MAP(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f, \
                        MANGLE_RESULT_OK(_t, _e))

/**
 * @brief Define map_err() error transformation function
 *
 * Generates function that transforms the contained error if Err.
 * If Ok, returns Ok unchanged without calling f.
 *
 * Limitation — same-type only:
 *   f must have signature _e(*)(_e); the error type cannot change.
 *   See DEFINE_RESULT_MAP for the rationale.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 * Returns: Ok unchanged if Ok, Err(f(error)) if Err(error)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type (both input and output of f)
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
 * Like map(), but f itself returns a Result instead of a plain value,
 * preventing Result<Result<T,E>,E> nesting.
 *
 * Performance: O(f) time where f is the chained function
 *              O(1) space (no nesting)
 * Returns: f(value) if Ok(value), Err unchanged if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
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
 * Generates function that provides an alternative if Err.
 * Returns r unchanged if Ok; calls f(error) to produce an alternative if Err.
 *
 * Performance: O(1) if Ok, O(f) if Err
 *              O(1) space
 * Returns: r if Ok, f(error) if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
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
 * Two Results are equal if:
 *   - both are Ok   with values   that satisfy eq_ok(a, b), OR
 *   - both are Err  with errors   that satisfy eq_err(a, b).
 * An Ok and an Err are never equal regardless of their contents.
 *
 * Performance: O(1)       for Ok-Err or Err-Ok comparison (short-circuit)
 *              O(eq_ok)   for Ok-Ok   where eq_ok  is the equality function
 *              O(eq_err)  for Err-Err where eq_err is the equality function
 *              O(1) space
 * Returns: true if equal, false otherwise
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
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
 * @brief Define all Result functions (struct excluded)
 *
 * Generates implementations for all Result<T, E> functions without emitting
 * the struct definition. Use together with DEFINE_RESULT_STRUCT when you need
 * to separate the type definition from the function definitions, or when you
 * have already emitted the struct via DEFINE_RESULT_ALL in another TU.
 *
 * All functions O(1) except combinators which are O(f) where f is the
 * caller-supplied function pointer.
 *
 * @param _linkage Linkage specifier (e.g., static inline for header-only)
 * @param _t       The value type
 * @param _e       The error type
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
 *   - Struct definition  (via DEFINE_RESULT_STRUCT)
 *   - All function implementations (via DEFINE_RESULT_FUNCTIONS)
 *
 * ⚠️  Duplicate-definition hazard:
 *   This macro emits a struct definition. Calling it more than once for the
 *   same (_t, _e) pair within the same program — except with static inline —
 *   violates the One Definition Rule and produces a link-time error.
 *   Preferred patterns to avoid this:
 *     a) static inline (header-only): safe to include from multiple TUs.
 *     b) Separate compilation: use DECLARE_RESULT_ALL(extern, T, E) in the
 *        shared header, and DEFINE_RESULT_ALL(, T, E) in exactly one .c file.
 *
 * Performance: All operations O(1) except combinators which are O(f)
 * Space per instance: sizeof(bool) + max(sizeof(T), sizeof(E)) + padding
 *
 * Example (header-only):
 * @code
 *   #include "result_defn.h"
 *   typedef enum { ERR_NONE, ERR_IO } error;
 *   DEFINE_RESULT_ALL(static inline, int, error)
 *
 *   result_int_error x = result_int_error_ok(42);
 * @endcode
 *
 * Example (separate compilation):
 * @code
 *   // my_results.h
 *   #include "result_decl.h"
 *   typedef enum { ERR_NONE, ERR_IO } error;
 *   DECLARE_RESULT_ALL(extern, int, error)
 *
 *   // my_results.c
 *   #include "my_results.h"
 *   #include "result_defn.h"
 *   DEFINE_RESULT_ALL(, int, error)   /* empty linkage = extern by default */
 * @endcode
 *
 * @param _linkage Linkage specifier (static inline for header-only, empty for extern)
 * @param _t       The value type
 * @param _e       The error type
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

// static inline: each TU gets its own copy — no ODR violation.
DEFINE_RESULT_ALL(static inline, int, error)
DEFINE_RESULT_ALL(static inline, float, error)

#endif


// ────────────────────────────────────────────────────────────────────────────
// Example 2: Separate compilation for faster builds
// ────────────────────────────────────────────────────────────────────────────

// my_results.h — shared declarations only
#ifndef MY_RESULTS_H
#define MY_RESULTS_H

#include "result_decl.h"

typedef enum { ERR_NONE, ERR_IO } error;

DECLARE_RESULT_ALL(extern, int, error)
DECLARE_RESULT_ALL(extern, float, error)

#endif

// my_results.c — definitions in exactly one translation unit
#include "my_results.h"
#include "result_defn.h"

DEFINE_RESULT_ALL(, int, error)      // empty _linkage = extern
DEFINE_RESULT_ALL(, float, error)


// ────────────────────────────────────────────────────────────────────────────
// Example 3: Custom IMPL override for pointer types
// ────────────────────────────────────────────────────────────────────────────
//
// Prerequisites visible to this translation unit before including result_defn.h:
//   - #include <assert.h>   (or a Canon-C equivalent that provides assert/panic)
//   - The CANON_PANIC macro defined in result_impl.h (or the host project)
//     which is the mechanism used by unwrap/expect/unwrap_err to terminate.
//
// IMPL contract reminder: the replacement macro must expand to a
// brace-enclosed function body { ... } with NO trailing semicolon.

#include <assert.h>
#include "result_impl.h"    // pulls in CANON_PANIC and default IMPL_* macros

typedef enum { ERR_NONE, ERR_NULL } ptr_error;

// Override Ok() to reject NULL at construction time.
// The canonical CANON_PANIC(msg) is used so the panic mechanism is
// consistent with the rest of the Result API.
#undef IMPL_RESULT_OK
#define IMPL_RESULT_OK(_t, _e, _tres, _param) \
    { \
        if ((_param) == NULL) { CANON_PANIC("result_ok: NULL pointer"); } \
        return (_tres){ .is_ok = true, .ok = (_param) }; \
    }

#include "result_defn.h"

typedef void* void_ptr;
DEFINE_RESULT_ALL(static inline, void_ptr, ptr_error)

// result_void_ptr_ptr_error_ok(NULL) now panics via CANON_PANIC.
*/

#endif /* CANON_RESULT_DEFN_H */
