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

// semantics/result/result_defn.h
#ifndef CANON_SEMANTICS_RESULT_DEFN_H
#define CANON_SEMANTICS_RESULT_DEFN_H

#ifndef CANON_SEMANTICS_RESULT_MANGLE_H
#  include "result_mangle.h"
#endif

#ifndef CANON_SEMANTICS_RESULT_IMPL_H
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
 *       { return (tres){ .is_ok = true, .val.ok = (param) }; }
 *
 * Rationale: DEFINE_RESULT_FUNCTIONS concatenates multiple macro expansions
 * without inserting semicolons between them. If an IMPL macro ends with ';'
 * rather than '}', the resulting token stream is malformed C.
 *
 * Separate compilation — ODR and struct redefinition:
 * ────────────────────────────────────────────────────────────────────────────
 * The recommended separate-compilation pattern is:
 *
 *   // my_types.h — declare the type and function signatures
 *   #include "result_decl.h"
 *   DECLARE_RESULT_ALL(extern, int, error)   // emits typedef + struct + decls
 *
 *   // my_types.c — define the function bodies only (struct already declared)
 *   #include "my_types.h"
 *   #include "result_defn.h"
 *   DEFINE_RESULT_FUNCTIONS(, int, error)    // bodies only — NO struct re-emit
 *
 * Do NOT call DEFINE_RESULT_ALL in a .c file that already included a header
 * containing DECLARE_RESULT_ALL for the same type pair: DEFINE_RESULT_ALL
 * emits the struct body again and C does not permit struct redefinition within
 * the same translation unit, even with identical bodies.
 *
 * Use DEFINE_RESULT_FUNCTIONS (not DEFINE_RESULT_ALL) when the struct and
 * typedef have already been declared by DECLARE_RESULT_ALL.
 *
 * Header-only pattern (safe for multiple translation units):
 *
 *   #include "result_defn.h"
 *   typedef enum { ERR_NONE, ERR_IO } error;
 *   DEFINE_RESULT_ALL(static inline, int, error)
 *
 * static inline gives each translation unit its own copy — no ODR violation.
 *
 * Use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Header-only: Define with static inline in .h files
 * 2. Separate compilation: DECLARE_RESULT_ALL in .h, DEFINE_RESULT_FUNCTIONS
 *    (NOT DEFINE_RESULT_ALL) in the single .c file
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
 * - Requires C99 or later (inline, compound literals, designated initializers)
 * - Works with GCC, Clang, and MSVC in C99 mode
 * - Linkage attributes are compiler-specific (use _linkage parameter)
 *
 * @sa result_decl.h, result_mangle.h, result_impl.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   TYPE DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define the Result<T, E> typedef alias.
 *
 * Emits: typedef struct result_T_E_s result_T_E;
 *
 * Must appear before any use of the type name in declarations or definitions.
 * DEFINE_RESULT_ALL calls this automatically. When using DEFINE_RESULT_STRUCT
 * and DEFINE_RESULT_FUNCTIONS individually, call this first.
 *
 * Performance: O(1) compile time
 *
 * @param _t The value type
 * @param _e The error type
 */
#define DEFINE_RESULT_TYPEDEF(_t, _e) \
    typedef struct MANGLE_RESULT_STRUCT_TAG(_t, _e) MANGLE_RESULT_TYPE(_t, _e);

/**
 * @brief Define Result<T, E> struct body.
 *
 * Emits the struct definition. Must not be emitted more than once per
 * (T, E) pair in the same translation unit — C does not allow struct
 * redefinition. See file-level note on the separate-compilation pattern.
 *
 * Performance: O(1) compile time
 * Memory layout: sizeof(bool) + max(sizeof(_t), sizeof(_e)) + alignment padding
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
 * Performance: O(1) time, O(1) space (stack allocation via compound literal)
 * Returns: Result containing the success value
 *
 * @param _linkage Linkage specifier (e.g., static inline, empty for extern)
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_OK(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_OK(_t, _e)(_t v) \
        IMPL_RESULT_OK(_t, _e, MANGLE_RESULT_TYPE(_t, _e), v)

/**
 * @brief Define Err(error) constructor function
 *
 * Performance: O(1) time, O(1) space (stack allocation via compound literal)
 * Returns: Result containing the error value
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
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
 * Performance: O(1) time, O(1) space (single boolean read)
 * Returns: true if Ok(value), false if Err(error)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_IS_OK(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_IS_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_IS_OK(_t, _e, r)

/**
 * @brief Define is_err() query function
 *
 * Performance: O(1) time, O(1) space (single boolean negation)
 * Returns: true if Err(error), false if Ok(value)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
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
 * Extracts the success value into *out if the Result is Ok.
 * Contract: out must not be NULL — caught by require() in the implementation.
 * Returns: true if Ok (and *out written), false if Err.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_GET_OK(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_GET_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t* out) \
        IMPL_RESULT_GET_OK(_t, _e, r, out)

/**
 * @brief Define get_err() safe extraction function
 *
 * Extracts the error value into *out if the Result is Err.
 * Contract: out must not be NULL — caught by require() in the implementation.
 * Returns: true if Err (and *out written), false if Ok.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_GET_ERR(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_GET_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _e* out) \
        IMPL_RESULT_GET_ERR(_t, _e, r, out)

/**
 * @brief Define unwrap_or() with fallback function
 *
 * Returns the contained value if Ok, otherwise returns fallback.
 * Never panics. Safe alternative to unwrap() for all call sites.
 *
 * Performance: O(1) time, O(1) space (conditional return)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
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
 * Extracts the success value. Crashes via require() if the Result is Err.
 * Only use when the caller has already verified is_ok(), or when an Err
 * here is an unrecoverable invariant violation.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: require(r.is_ok) — aborts with message if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_UNWRAP(_linkage, _t, _e) \
    _linkage _t \
    MANGLE_RESULT_UNWRAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_UNWRAP(_t, _e, r)

/**
 * @brief Define unwrap_err() error extraction function
 *
 * Extracts the error value. Crashes via require() if the Result is Ok.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: require(!r.is_ok) — aborts with message if Ok
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_UNWRAP_ERR(_linkage, _t, _e) \
    _linkage _e \
    MANGLE_RESULT_UNWRAP_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r) \
        IMPL_RESULT_UNWRAP_ERR(_t, _e, r)

/**
 * @brief Define expect() with custom message function
 *
 * Like unwrap(), but crashes with a caller-supplied message.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: require_msg(r.is_ok, msg) — aborts with msg if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
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
 * Applies f to the contained value if Ok (T → T only).
 * If Err, returns Err unchanged without calling f.
 *
 * Limitation — same-type only: f must have signature _t(*)(_t).
 * For T → U transforms, use and_then() with a function returning the
 * target Result type.
 *
 * Performance: O(f) time, O(1) space
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type (both input and output of f)
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_MAP(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_MAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t (*f)(_t)) \
        IMPL_RESULT_MAP(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f, \
                        MANGLE_RESULT_OK(_t, _e))

/**
 * @brief Define map_err() error transformation function
 *
 * Applies f to the contained error if Err (E → E only).
 * If Ok, returns Ok unchanged without calling f.
 *
 * Performance: O(f) time, O(1) space
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type (both input and output of f)
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_MAP_ERR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_MAP_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _e (*f)(_e)) \
        IMPL_RESULT_MAP_ERR(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f, \
                            MANGLE_RESULT_ERR(_t, _e))

/**
 * @brief Define and_then() chaining function
 *
 * If Ok, calls f with the contained value and returns f's Result.
 * If Err, returns Err unchanged without calling f.
 * Prevents Result<Result<T,E>,E> nesting.
 *
 * Performance: O(f) time, O(1) space
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_AND_THEN(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_AND_THEN(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, \
        MANGLE_RESULT_TYPE(_t, _e) (*f)(_t)) \
        IMPL_RESULT_AND_THEN(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f)

/**
 * @brief Define or_else() alternative provider function
 *
 * If Ok, returns Ok unchanged without calling f.
 * If Err, calls f with the contained error and returns f's Result.
 *
 * Performance: O(1) if Ok, O(f) if Err, O(1) space
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_OR_ELSE(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_OR_ELSE(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, \
        MANGLE_RESULT_TYPE(_t, _e) (*f)(_e)) \
        IMPL_RESULT_OR_ELSE(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, f)

/**
 * @brief Define and() eager short-circuit AND function
 *
 * Returns other if Ok, otherwise returns Err unchanged.
 * other is always evaluated (eager). Use and_then() for lazy evaluation.
 *
 *   Ok(_)  -> other
 *   Err(e) -> Err(e)
 *
 * Performance: O(1) time, O(1) space
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_AND(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_AND(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, \
        MANGLE_RESULT_TYPE(_t, _e) other) \
        IMPL_RESULT_AND(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, other)

/**
 * @brief Define or() eager short-circuit OR function
 *
 * Returns r if Ok, otherwise returns other.
 * other is always evaluated (eager). Use or_else() for lazy evaluation.
 *
 *   Ok(v)  -> Ok(v)
 *   Err(_) -> other
 *
 * Performance: O(1) time, O(1) space
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_OR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) \
    MANGLE_RESULT_OR(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, \
        MANGLE_RESULT_TYPE(_t, _e) other) \
        IMPL_RESULT_OR(_t, _e, MANGLE_RESULT_TYPE(_t, _e), r, other)

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION DEFINITIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define eq() equality comparison function
 *
 * Two Results are equal when:
 *   - Both are Ok  and eq_ok(r1.val.ok,  r2.val.ok)  returns true, OR
 *   - Both are Err and eq_err(r1.val.err, r2.val.err) returns true.
 * An Ok and an Err are never equal.
 *
 * Performance: O(1) for Ok-Err, O(eq_ok) for Ok-Ok, O(eq_err) for Err-Err
 * Contract: eq_ok and eq_err must not be NULL.
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_RESULT_EQ(_linkage, _t, _e) \
    _linkage bool \
    MANGLE_RESULT_EQ(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r1, \
        MANGLE_RESULT_TYPE(_t, _e) r2, \
        bool (*eq_ok)(_t, _t), \
        bool (*eq_err)(_e, _e)) \
        IMPL_RESULT_EQ(_t, _e, r1, r2, eq_ok, eq_err)

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS — DEFINE MULTIPLE AT ONCE
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define all Result<T, E> function bodies (no typedef, no struct).
 *
 * Generates implementations for every Result<T, E> function without emitting
 * the typedef or struct definition. Use this in the separate-compilation
 * pattern when the struct and typedef have already been declared by
 * DECLARE_RESULT_ALL in an included header.
 *
 * Typical .c file pattern:
 * @code
 *   #include "my_types.h"     // contains DECLARE_RESULT_ALL(extern, int, error)
 *   #include "result_defn.h"
 *   DEFINE_RESULT_FUNCTIONS(, int, error)   // empty linkage = extern bodies
 * @endcode
 *
 * All functions O(1) except combinators which are O(f).
 *
 * @param _linkage Linkage specifier
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
    DEFINE_RESULT_AND(_linkage, _t, _e) \
    DEFINE_RESULT_OR(_linkage, _t, _e) \
    DEFINE_RESULT_EQ(_linkage, _t, _e)

/**
 * @brief Define a complete Result<T, E> type and all functions.
 *
 * One-shot macro that emits, in order:
 *   1. typedef  (DEFINE_RESULT_TYPEDEF)
 *   2. struct   (DEFINE_RESULT_STRUCT)
 *   3. all function bodies (DEFINE_RESULT_FUNCTIONS)
 *
 * Use this for header-only libraries (with static inline) or in a single
 * standalone .c file that does not include a header with DECLARE_RESULT_ALL
 * for the same type pair.
 *
 * ⚠️  Do NOT call DEFINE_RESULT_ALL in a .c file that already included a
 * header containing DECLARE_RESULT_ALL for the same (T, E) pair. Doing so
 * emits the struct body a second time in the same translation unit, which
 * is a C error. Use DEFINE_RESULT_FUNCTIONS instead (see above).
 *
 * Header-only example (safe for multiple TUs):
 * @code
 *   #include "result_defn.h"
 *   typedef enum { ERR_NONE, ERR_IO } error;
 *   DEFINE_RESULT_ALL(static inline, int, error)
 * @endcode
 *
 * Standalone .c example (no shared header for this type):
 * @code
 *   #include "result_defn.h"
 *   typedef enum { ERR_NONE, ERR_IO } error;
 *   DEFINE_RESULT_ALL(, int, error)   // empty linkage = extern
 * @endcode
 *
 * @param _linkage Linkage specifier (static inline for header-only, empty for extern)
 * @param _t       The value type
 * @param _e       The error type
 */
#define DEFINE_RESULT_ALL(_linkage, _t, _e) \
    DEFINE_RESULT_TYPEDEF(_t, _e) \
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

DECLARE_RESULT_ALL(extern, int, error)     // typedef + struct + fn decls
DECLARE_RESULT_ALL(extern, float, error)

#endif

// my_results.c — function bodies only (struct already declared by header)
#include "my_results.h"
#include "result_defn.h"

DEFINE_RESULT_FUNCTIONS(, int, error)     // bodies only — no struct re-emit
DEFINE_RESULT_FUNCTIONS(, float, error)   // NOTE: NOT DEFINE_RESULT_ALL


// ────────────────────────────────────────────────────────────────────────────
// Example 3: Custom IMPL override (NULL-rejecting Ok constructor)
// ────────────────────────────────────────────────────────────────────────────

// IMPL contract: replacement must expand to { ... } with NO trailing semicolon.

#include "result_impl.h"   // pulls in contract.h (require_msg) and default IMPL_* macros

typedef enum { ERR_NONE, ERR_NULL } ptr_error;
typedef void* void_ptr;

#undef IMPL_RESULT_OK
#define IMPL_RESULT_OK(_t, _e, _tres, _param) \
    { \
        require_msg((_param) != NULL, "result_ok: NULL pointer not allowed"); \
        return (_tres){ .is_ok = true, .val.ok = (_param) }; \
    }

#include "result_defn.h"
DEFINE_RESULT_ALL(static inline, void_ptr, ptr_error)
// result_void_ptr_ptr_error_ok(NULL) now panics via require_msg().
*/

#endif /* CANON_SEMANTICS_RESULT_DEFN_H */
