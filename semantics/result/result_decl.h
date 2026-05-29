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

// semantics/result/result_decl.h
#ifndef CANON_SEMANTICS_RESULT_DECL_H
#define CANON_SEMANTICS_RESULT_DECL_H

/*
 * result_decl.h only needs name-mangling macros at declaration time.
 * result_impl.h (which pulls contract.h and types.h) is NOT included here:
 * it belongs in result_defn.h, which is the file that actually emits code.
 * Keeping this boundary means including result_decl.h in a header costs zero
 * transitive headers beyond result_mangle.h itself.
 *
 * stdbool.h IS included here because the struct definition emitted by
 * DECLARE_RESULT_STRUCT uses the bool type. Any translation unit that
 * includes result_decl.h must have bool available; including stdbool.h
 * here makes this self-contained and explicit.
 */
#include <stdbool.h>

#ifndef CANON_SEMANTICS_RESULT_MANGLE_H
#  include "result_mangle.h"
#endif

/**
 * @file result_decl.h
 * @brief Declaration macros for Result<T, E> type (separate compilation support)
 *
 * Provides macros for declaring Result types and functions without defining
 * their implementations. Use this in header files when separating declarations
 * from definitions (classic .h/.c compilation model).
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
 * Separate compilation — ODR and struct redefinition:
 * ────────────────────────────────────────────────────────────────────────────
 * DECLARE_RESULT_ALL emits the struct body in the header. The corresponding
 * .c file must call DEFINE_RESULT_FUNCTIONS (NOT DEFINE_RESULT_ALL) to emit
 * only the function bodies without re-emitting the struct. Calling
 * DEFINE_RESULT_ALL in a .c file that already included a header with
 * DECLARE_RESULT_ALL for the same type pair is a C error (struct
 * redefinition within the same translation unit).
 *
 * Correct separate-compilation pattern:
 *
 *   // my_types.h
 *   #include "result_decl.h"
 *   typedef enum { ERR_NONE, ERR_IO } error;
 *   DECLARE_RESULT_ALL(extern, int, error)   // typedef + struct + fn decls
 *
 *   // my_types.c
 *   #include "my_types.h"
 *   #include "result_defn.h"
 *   DEFINE_RESULT_FUNCTIONS(, int, error)    // fn bodies only — no struct
 *
 * C99 compliance note:
 * ────────────────────────────────────────────────────────────────────────────
 * The Result struct uses a NAMED union member (.val) by default to remain
 * strictly C99 compliant. Anonymous unions inside structs are a C11
 * extension (ISO/IEC 9899:2011 §6.7.2.1¶13) and are not portable to strict
 * C99 compilers (CompCert, MSVC /Za, Polyspace, LDRA toolchain).
 *
 * Default access pattern (C99):
 *   r.val.ok   r.val.err
 *
 * If your toolchain supports C11 or accepts anonymous unions as an extension,
 * define CANON_SEMANTICS_RESULT_ANON_UNION before including this header to opt in:
 *   r.ok   r.err
 *
 * CANON_SEMANTICS_RESULT_ANON_UNION — layout alignment requirement:
 * ────────────────────────────────────────────────────────────────────────────
 * result_decl.h (DECLARE_RESULT_STRUCT) and result_impl.h (IMPL_RESULT_STRUCT,
 * used by DEFINE_RESULT_STRUCT) both branch on CANON_SEMANTICS_RESULT_ANON_UNION to
 * select their union layout. Both files must be compiled with the same setting
 * of this flag in every translation unit that uses a given Result type pair,
 * or the struct layouts will differ between the declaration and the definition
 * — a silent ABI break.
 *
 * The safest approach is to set (or not set) CANON_SEMANTICS_RESULT_ANON_UNION in a
 * project-wide prefix header rather than per-file, so that all translation
 * units always agree.
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
 * - Runtime: Zero cost (declarations generate no code)
 * - Build time: Faster incremental builds with separate compilation
 * - Binary size: Avoid code duplication with extern linkage
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * N/A — pure compile-time declarations, no runtime behaviour
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline keyword, bool type, named union members)
 * - Anonymous union variant requires C11 or compiler extension (opt-in only)
 * - Works with GCC, Clang, MSVC, CompCert, Polyspace, LDRA
 * - Linkage attributes are compiler-specific (pass via _linkage parameter)
 *
 * Known limitations:
 * ────────────────────────────────────────────────────────────────────────────
 * - T and E must be single-token identifiers. Raw pointer types (int*, void*)
 *   and qualified types (const char*) cannot be used directly because the
 *   preprocessor ## operator does not accept tokens containing * or spaces.
 *   Workaround: typedef first.
 *
 *     typedef int*        intp;
 *     typedef const char* cstr;
 *     DECLARE_RESULT_ALL(extern, intp, cstr)  // OK
 *     DECLARE_RESULT_ALL(extern, int*, cstr)  // ERROR — * breaks ## pasting
 *
 * @sa result_defn.h, result_mangle.h, result_impl.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   STRUCT LAYOUT — C99 vs C11 ANONYMOUS UNION
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * CANON_SEMANTICS_RESULT_UNION_BODY_ — selects named vs anonymous union layout.
 *
 * This macro mirrors IMPL_RESULT_UNION_ in result_impl.h. Both must select
 * the same layout, which they do because both branch on the same flag
 * (CANON_SEMANTICS_RESULT_ANON_UNION). See the "layout alignment requirement" note in
 * the file-level documentation above.
 *
 * When CANON_SEMANTICS_RESULT_ANON_UNION is defined:
 *   union { _t ok; _e err; };         — anonymous, access as r.ok / r.err
 * When absent (default):
 *   union { _t ok; _e err; } val;     — named,     access as r.val.ok / r.val.err
 */
#ifdef CANON_SEMANTICS_RESULT_ANON_UNION
#  define CANON_SEMANTICS_RESULT_UNION_BODY_(_t, _e) \
       union { _t ok; _e err; };
#else
#  define CANON_SEMANTICS_RESULT_UNION_BODY_(_t, _e) \
       union { _t ok; _e err; } val;
#endif

/* ════════════════════════════════════════════════════════════════════════════
   TYPE DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Forward-declare the Result<T, E> struct tag only (opaque pointer use)
 *
 * Emits only "struct result_T_E_s;" with no layout. Use this when you need
 * to pass Result<T, E>* across a translation-unit boundary without exposing
 * the struct layout (opaque pointer / information-hiding pattern).
 *
 * Performance: O(1) compile time
 *
 * Example:
 *   DECLARE_RESULT_FORWARD(int, error)
 *   // Emits: struct result_int_error_s;
 *
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_FORWARD(_t, _e) \
    struct MANGLE_RESULT_STRUCT_TAG(_t, _e);

/**
 * @brief Declare Result<T, E> typedef (no struct body)
 *
 * Creates a typedef alias for the struct tag. The struct body must have been
 * declared (via DECLARE_RESULT_STRUCT) before the typedef is used in a
 * complete-type context (i.e., before you declare a value of this type or
 * pass one by value). For forward/opaque use (pointer only), the struct body
 * is not required.
 *
 * Performance: O(1) compile time
 *
 * Example:
 *   DECLARE_RESULT_TYPEDEF(int, error)
 *   // Emits: typedef struct result_int_error_s result_int_error;
 *
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_TYPEDEF(_t, _e) \
    typedef struct MANGLE_RESULT_STRUCT_TAG(_t, _e) MANGLE_RESULT_TYPE(_t, _e);

/**
 * @brief Declare the Result<T, E> struct layout
 *
 * Emits the complete struct definition. The union member is named (.val)
 * in strict C99 mode, or anonymous when CANON_SEMANTICS_RESULT_ANON_UNION is defined.
 *
 * Must not be emitted more than once per (T, E) pair in the same translation
 * unit. DECLARE_RESULT_ALL calls this; the corresponding .c file must use
 * DEFINE_RESULT_FUNCTIONS (not DEFINE_RESULT_ALL) to avoid re-emitting it.
 *
 * Memory layout: sizeof(bool) + padding + max(sizeof(_t), sizeof(_e))
 *
 * Performance: O(1) compile time
 *
 * Example (default, C99):
 *   DECLARE_RESULT_STRUCT(int, error)
 *   // Emits:
 *   //   struct result_int_error_s {
 *   //       bool is_ok;
 *   //       union { int ok; error err; } val;
 *   //   };
 *
 * Example (with CANON_SEMANTICS_RESULT_ANON_UNION, C11/extension):
 *   // Emits:
 *   //   struct result_int_error_s {
 *   //       bool is_ok;
 *   //       union { int ok; error err; };
 *   //   };
 *
 * @param _t The value type
 * @param _e The error type
 */
#define DECLARE_RESULT_STRUCT(_t, _e) \
    struct MANGLE_RESULT_STRUCT_TAG(_t, _e) { \
        bool is_ok; \
        CANON_SEMANTICS_RESULT_UNION_BODY_(_t, _e) \
    };

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare Ok(value) constructor function
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier (e.g., extern, static inline)
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_OK(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_OK(_t, _e)(_t v);

/**
 * @brief Declare Err(error) constructor function
 *
 * Performance: O(1) runtime when defined (stack allocation only)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
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
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_IS_OK(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_IS_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/**
 * @brief Declare is_err() query function
 *
 * Performance: O(1) runtime when defined (single boolean read, negated)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_IS_ERR(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_IS_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare get_ok() safe extraction function
 *
 * Extracts the success value into *out if the Result is Ok.
 * Contract: out must not be NULL — caught by require() in the definition.
 * Returns: true if Ok (and *out written), false if Err.
 *
 * Performance: O(1) runtime when defined (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_GET_OK(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_GET_OK(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t* out);

/**
 * @brief Declare get_err() safe extraction function
 *
 * Extracts the error value into *out if the Result is Err.
 * Contract: out must not be NULL — caught by require() in the definition.
 * Returns: true if Err (and *out written), false if Ok.
 *
 * Performance: O(1) runtime when defined (conditional assignment)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_GET_ERR(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_GET_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _e* out);

/**
 * @brief Declare unwrap_or() with fallback function
 *
 * Returns the contained value if Ok, otherwise returns fallback.
 * Never panics. Safe alternative to unwrap() for all call sites.
 *
 * Performance: O(1) runtime when defined (conditional return)
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_UNWRAP_OR(_linkage, _t, _e) \
    _linkage _t MANGLE_RESULT_UNWRAP_OR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, _t fallback);

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare unwrap() unsafe extraction function
 *
 * Extracts the success value. Crashes via require() if the Result is Err.
 * Only use when the caller has already verified is_ok().
 *
 * Performance: O(1) runtime when defined (assertion + union access)
 * Contract: require(r.is_ok) — aborts with message if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_UNWRAP(_linkage, _t, _e) \
    _linkage _t MANGLE_RESULT_UNWRAP(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/**
 * @brief Declare unwrap_err() error extraction function
 *
 * Extracts the error value. Crashes via require() if the Result is Ok.
 *
 * Performance: O(1) runtime when defined (assertion + union access)
 * Contract: require(!r.is_ok) — aborts with message if Ok
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_UNWRAP_ERR(_linkage, _t, _e) \
    _linkage _e MANGLE_RESULT_UNWRAP_ERR(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r);

/**
 * @brief Declare expect() with custom message function
 *
 * Like unwrap(), but crashes with a caller-supplied message.
 *
 * Performance: O(1) runtime when defined (assertion + union access)
 * Contract: require(r.is_ok, msg) — aborts with msg if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_EXPECT(_linkage, _t, _e) \
    _linkage _t MANGLE_RESULT_EXPECT(_t, _e)(MANGLE_RESULT_TYPE(_t, _e) r, const char* msg);

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATION FUNCTION DECLARATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare map() value transformation function
 *
 * Applies f to the contained value if Ok (T → T only).
 * If Err, returns Err unchanged without calling f.
 *
 * Constraint: f must be T → T. For T → U transforms use and_then().
 *
 * Performance: O(f) runtime
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_MAP(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_MAP(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, _t (*f)(_t));

/**
 * @brief Declare map_err() error transformation function
 *
 * Applies f to the contained error if Err (E → E only).
 * If Ok, returns Ok unchanged without calling f.
 *
 * Constraint: f must be E → E.
 *
 * Performance: O(f) runtime
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_MAP_ERR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_MAP_ERR(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, _e (*f)(_e));

/**
 * @brief Declare and_then() chaining function
 *
 * If Ok, calls f with the contained value and returns f's Result.
 * If Err, returns Err unchanged without calling f.
 * Prevents Result<Result<T,E>,E> nesting.
 *
 * Performance: O(f) runtime
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_AND_THEN(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_AND_THEN(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, MANGLE_RESULT_TYPE(_t, _e) (*f)(_t));

/**
 * @brief Declare or_else() alternative provider function
 *
 * If Ok, returns Ok unchanged without calling f.
 * If Err, calls f with the contained error and returns f's Result.
 *
 * Performance: O(1) if Ok, O(f) if Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_OR_ELSE(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_OR_ELSE(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, MANGLE_RESULT_TYPE(_t, _e) (*f)(_e));

/**
 * @brief Declare and() eager short-circuit AND function
 *
 * Returns other if Ok, otherwise returns Err unchanged.
 * other is always evaluated (eager). Use and_then() for lazy evaluation.
 *
 *   Ok(_)  -> other
 *   Err(e) -> Err(e)
 *
 * Performance: O(1) runtime
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_AND(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_AND(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, MANGLE_RESULT_TYPE(_t, _e) other);

/**
 * @brief Declare or() eager short-circuit OR function
 *
 * Returns r if Ok, otherwise returns other.
 * other is always evaluated (eager). Use or_else() for lazy evaluation.
 *
 *   Ok(v)  -> Ok(v)
 *   Err(_) -> other
 *
 * Performance: O(1) runtime
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_OR(_linkage, _t, _e) \
    _linkage MANGLE_RESULT_TYPE(_t, _e) MANGLE_RESULT_OR(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r, MANGLE_RESULT_TYPE(_t, _e) other);

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION DECLARATIONS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare eq() equality comparison function
 *
 * Two Results are equal when:
 *   - Both are Ok  and eq_ok(r1.val.ok,  r2.val.ok)  returns true, OR
 *   - Both are Err and eq_err(r1.val.err, r2.val.err) returns true.
 * An Ok and an Err are never equal.
 *
 * Contract: eq_ok and eq_err must not be NULL.
 *
 * Performance: O(1) for Ok-Err, O(eq_ok) for Ok-Ok, O(eq_err) for Err-Err
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
 */
#define DECLARE_RESULT_EQ(_linkage, _t, _e) \
    _linkage bool MANGLE_RESULT_EQ(_t, _e)( \
        MANGLE_RESULT_TYPE(_t, _e) r1, \
        MANGLE_RESULT_TYPE(_t, _e) r2, \
        bool (*eq_ok)(_t, _t), \
        bool (*eq_err)(_e, _e));

/* ════════════════════════════════════════════════════════════════════════════
   CONVENIENCE MACROS — DECLARE MULTIPLE AT ONCE
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Declare all Result<T, E> function signatures (no type or struct)
 *
 * Declares every function signature for a Result<T, E> instantiation.
 * Does not declare the type itself — pair with DECLARE_RESULT_TYPEDEF and
 * DECLARE_RESULT_STRUCT, or use DECLARE_RESULT_ALL for a single call.
 *
 * Performance: O(1) compile time
 *
 * @param _linkage Linkage specifier (e.g., extern, static inline)
 * @param _t       The value type
 * @param _e       The error type
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
    DECLARE_RESULT_AND(_linkage, _t, _e) \
    DECLARE_RESULT_OR(_linkage, _t, _e) \
    DECLARE_RESULT_EQ(_linkage, _t, _e)

/**
 * @brief Declare a complete Result<T, E> type and all its function signatures
 *
 * One-shot macro that emits, in order:
 *   1. typedef           (DECLARE_RESULT_TYPEDEF)
 *   2. struct body       (DECLARE_RESULT_STRUCT)
 *   3. function decls    (DECLARE_RESULT_FUNCTIONS)
 *
 * Use this in headers to expose a full Result type to consumers.
 * The corresponding .c file must call DEFINE_RESULT_FUNCTIONS (NOT
 * DEFINE_RESULT_ALL) to provide the function bodies without re-emitting
 * the struct. See the file-level "Separate compilation" note.
 *
 * Performance: O(1) compile time
 *
 * Example — separate compilation:
 * @code
 * // my_types.h
 * #include "result_decl.h"
 * typedef enum { ERR_NONE, ERR_IO } error;
 * DECLARE_RESULT_ALL(extern, int, error)
 *
 * // my_types.c
 * #include "my_types.h"
 * #include "result_defn.h"
 * DEFINE_RESULT_FUNCTIONS(, int, error)   // NOTE: not DEFINE_RESULT_ALL
 *
 * // main.c
 * #include "my_types.h"
 * result_int_error x = result_int_error_ok(42);
 * @endcode
 *
 * @param _linkage Linkage specifier
 * @param _t       The value type
 * @param _e       The error type
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
// Example 1: Separate compilation (.h / .c model)
// ────────────────────────────────────────────────────────────────────────────

// my_results.h
#ifndef MY_RESULTS_H
#define MY_RESULTS_H
#include "result_decl.h"
typedef enum { ERR_NONE, ERR_IO, ERR_PARSE } error;
DECLARE_RESULT_ALL(extern, int, error)
DECLARE_RESULT_ALL(extern, float, error)
#endif

// my_results.c
#include "my_results.h"
#include "result_defn.h"
DEFINE_RESULT_FUNCTIONS(, int, error)    // fn bodies only — struct already declared
DEFINE_RESULT_FUNCTIONS(, float, error)  // NOTE: NOT DEFINE_RESULT_ALL

// main.c
#include "my_results.h"
int main(void) {
    result_int_error x = result_int_error_ok(42);
    if (result_int_error_is_ok(x)) { ... }
    return 0;
}

// ────────────────────────────────────────────────────────────────────────────
// Example 2: Header-only library
// ────────────────────────────────────────────────────────────────────────────

// my_lib.h
#ifndef MY_LIB_H
#define MY_LIB_H
#include "result_defn.h"
typedef enum { ERR_NONE, ERR_FAIL } error;
DEFINE_RESULT_ALL(static inline, int, error)   // typedef + struct + fn bodies
#endif

// ────────────────────────────────────────────────────────────────────────────
// Example 3: Opaque pointer / minimal public API
// ────────────────────────────────────────────────────────────────────────────

// public_api.h — expose only what callers need; hide struct layout entirely
#include "result_decl.h"
typedef enum { ERR_NONE } error;
DECLARE_RESULT_FORWARD(int, error)   // struct result_int_error_s; — forward only
DECLARE_RESULT_TYPEDEF(int, error)   // typedef -> result_int_error
DECLARE_RESULT_OK(extern, int, error)
DECLARE_RESULT_ERR(extern, int, error)
DECLARE_RESULT_IS_OK(extern, int, error)
// Layout, unwrap, map, etc. remain internal — callers cannot access .val directly

// ────────────────────────────────────────────────────────────────────────────
// Example 4: Pointer types require a typedef
// ────────────────────────────────────────────────────────────────────────────

typedef const char* cstr;
typedef void*       voidptr;
DECLARE_RESULT_ALL(extern, int, cstr)
DECLARE_RESULT_ALL(extern, voidptr, int)

// ────────────────────────────────────────────────────────────────────────────
// Example 5: C11 / anonymous union opt-in
// ────────────────────────────────────────────────────────────────────────────

// Only if your compiler supports anonymous unions (C11 or documented extension).
// Set this flag in a project-wide prefix header so all TUs agree on the layout.
#define CANON_SEMANTICS_RESULT_ANON_UNION
#include "result_decl.h"
typedef enum { ERR_NONE } error;
DECLARE_RESULT_ALL(extern, int, error)
// Now r.ok and r.err work directly (no r.val.ok) — but ONLY if result_impl.h
// was also compiled with CANON_SEMANTICS_RESULT_ANON_UNION set in the same TU.
*/

#endif /* CANON_SEMANTICS_RESULT_DECL_H */
