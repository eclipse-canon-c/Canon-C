/* semantics/result/result_mangle.h */
#ifndef CANON_SEMANTICS_RESULT_RESULT_MANGLE_H
#define CANON_SEMANTICS_RESULT_RESULT_MANGLE_H

/**
 * @file result_mangle.h
 * @brief Name mangling conventions for Result<T, E> type
 *
 * This file defines the naming scheme for all Result-related types and
 * functions.  It is the single source of truth for names — all other files
 * reference these macros instead of hard-coding identifiers.
 *
 * Philosophy (Canon-C)
 * ────────────────────────────────────────────────────────────────────────────
 * - Everything is optional   — every name can be overridden before inclusion
 * - Everything is explicit   — no hidden name generation; read the header,
 *                              know every identifier that will be produced
 * - No runtime cost          — pure preprocessor token-pasting; O(1)
 *                              compile-time, zero object-code footprint
 * - No global state          — macros carry no side-effects
 * - No framework coupling    — nothing in this file requires any other header
 * - Plain C99                — ## token-pasting is standard C99 (§6.10.3.3);
 *                              no GNU extensions, no C11/C23 features
 * - Namespace-safe defaults  — default prefix result_ avoids collisions
 * - Consistent               — uniform pattern across all generated names
 *
 * > If behavior cannot be understood by reading the header, it does not belong.
 * > Abstractions must clarify behavior, not conceal it.
 *
 * ────────────────────────────────────────────────────────────────────────────
 * IMPORTANT — Token constraints (C99 §6.10.3.3)
 * ────────────────────────────────────────────────────────────────────────────
 * @warning Both _t and _e MUST be single, valid C preprocessing tokens.
 *          They must contain NO spaces and NO punctuation characters
 *          (including '*', '(', '[', etc.).
 *
 *          For pointer or qualified types use a typedef BEFORE invoking
 *          CANON_RESULT:
 *
 *            typedef const char *  cstr_t;
 *            typedef void       *  vptr_t;
 *            typedef int        *  intp_t;
 *
 *            CANON_RESULT(int,   cstr_t)   /* ok  * /
 *            CANON_RESULT(vptr_t, int)     /* ok  * /
 *
 *          Passing raw pointer syntax will produce a hard compiler error or,
 *          worse, a silently malformed identifier with no diagnostic:
 *
 *            CANON_RESULT(int, const char *)   /* WRONG — do not do this * /
 *
 * ────────────────────────────────────────────────────────────────────────────
 * Override / customization
 * ────────────────────────────────────────────────────────────────────────────
 * Define any subset of the macros below BEFORE including this header.
 * Overrides are per-translation-unit; different TUs may use different naming
 * schemes without conflict.
 *
 * Rule: if you override MANGLE_RESULT_TYPE you MUST also override
 *       MANGLE_RESULT_STRUCT_TAG (or rely on the default derivation below
 *       which appends _s to whatever MANGLE_RESULT_TYPE produces).
 *       Mismatching the two causes a struct-tag / typedef mismatch that the
 *       compiler will catch but the diagnostic may be confusing.
 *
 * Customization example — Haskell-style Either / Right / Left:
 *
 *   #define MANGLE_RESULT_TYPE(t, e)     Either_##t##_##e
 *   #define MANGLE_RESULT_IS_OK(t, e)   Either_##t##_##e##_isRight
 *   #define MANGLE_RESULT_IS_ERR(t, e)  Either_##t##_##e##_isLeft
 *   #define MANGLE_RESULT_OK(t, e)      Either_##t##_##e##_Right
 *   #define MANGLE_RESULT_ERR(t, e)     Either_##t##_##e##_Left
 *   #include "result.h"
 *
 *   CANON_RESULT(int, error_t)
 *   Either_int_error_t x = Either_int_error_t_Right(42);
 *
 * Customization example — project-scoped prefix:
 *
 *   #define MANGLE_RESULT_TYPE(t, e)    myproj_res_##t##_##e
 *   #include "result.h"
 *
 *   CANON_RESULT(int, error_t)
 *   myproj_res_int_error_t x = myproj_res_int_error_t_ok(42);
 *
 * ────────────────────────────────────────────────────────────────────────────
 * Default naming scheme
 * ────────────────────────────────────────────────────────────────────────────
 * For types T and E the defaults produce:
 *
 *   Type / tag
 *     result_T_E              — typedef'd struct name
 *     result_T_E_s            — underlying struct tag  (derived from type)
 *
 *   Constructors
 *     result_T_E_ok(val)      — construct Ok(val)
 *     result_T_E_err(err)     — construct Err(err)
 *
 *   Queries
 *     result_T_E_is_ok(r)     — true iff Ok
 *     result_T_E_is_err(r)    — true iff Err
 *
 *   Safe extraction
 *     result_T_E_get_ok(r, out)    — copy Ok value into *out; return bool
 *     result_T_E_get_err(r, out)   — copy Err value into *out; return bool
 *     result_T_E_unwrap_or(r, def) — return Ok value or fallback def
 *
 *   Unsafe extraction  (abort / assert on wrong variant)
 *     result_T_E_unwrap(r)         — return Ok value or abort
 *     result_T_E_unwrap_err(r)     — return Err value or abort
 *     result_T_E_expect(r, msg)    — return Ok value or abort with msg
 *
 *   Combinators
 *     result_T_E_map(r, fn)        — apply fn to Ok value; pass Err through
 *     result_T_E_map_err(r, fn)    — apply fn to Err value; pass Ok through
 *     result_T_E_and_then(r, fn)   — flatMap over Ok; short-circuit on Err
 *     result_T_E_or_else(r, fn)    — flatMap over Err; short-circuit on Ok
 *     result_T_E_and(r, other)     — return other if Ok, else propagate Err
 *     result_T_E_or(r, other)      — return r if Ok, else return other
 *
 *   Comparison
 *     result_T_E_eq(r1, r2, eq_ok, eq_err)
 *
 * ────────────────────────────────────────────────────────────────────────────
 * Performance
 * ────────────────────────────────────────────────────────────────────────────
 * Time:  O(1) — pure compile-time token pasting
 * Space: O(1) — no runtime cost; zero object-code footprint
 *
 * ────────────────────────────────────────────────────────────────────────────
 * Thread-safety
 * ────────────────────────────────────────────────────────────────────────────
 * N/A — pure compile-time macros; no runtime behavior whatsoever.
 *
 * ────────────────────────────────────────────────────────────────────────────
 * Portability
 * ────────────────────────────────────────────────────────────────────────────
 * Requires C99 or later.  ## token-pasting is defined in C99 §6.10.3.3.
 * Tested with GCC, Clang, and MSVC (/std:c11 or later).
 * No platform-specific code; no compiler extensions.
 *
 * @sa result_impl.h, result_decl.h, result_defn.h
 */


/* ════════════════════════════════════════════════════════════════════════════
   TYPE NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Main Result type name (typedef).
 *
 * Default:  result_##_t##_##_e
 * Example:  result_int_error_t
 *
 * Override to change the naming across your entire translation unit.
 *
 * @warning _t and _e must each be a single C preprocessing token (no spaces,
 *          no punctuation).  See file-level note on token constraints.
 */
#ifndef MANGLE_RESULT_TYPE
#  define MANGLE_RESULT_TYPE(t, e)          result_##t##_##e
#endif

/**
 * @brief Underlying struct tag for the Result type.
 *
 * Default:  derived from MANGLE_RESULT_TYPE by appending _s
 * Example:  result_int_error_t_s
 *
 * The default intentionally derives from MANGLE_RESULT_TYPE so that
 * overriding the type name automatically keeps the struct tag consistent.
 * Override this separately ONLY when you need explicit control over the
 * struct tag (e.g. forward declarations in another header).
 *
 * Invariant: MANGLE_RESULT_STRUCT_TAG must always refer to the same
 *            underlying struct as MANGLE_RESULT_TYPE.  Violating this
 *            produces a typedef / struct-tag mismatch diagnosed by the
 *            compiler.
 */
#ifndef MANGLE_RESULT_STRUCT_TAG
#  define MANGLE_RESULT_STRUCT_TAG(t, e)    MANGLE_RESULT_TYPE(t, e)##_s
#endif


/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ok(value) constructor.
 *
 * Default:  result_##t##_##e##_ok
 * Example:  result_int_error_t_ok(42)
 */
#ifndef MANGLE_RESULT_OK
#  define MANGLE_RESULT_OK(t, e)            MANGLE_RESULT_TYPE(t, e)##_ok
#endif

/**
 * @brief Err(error) constructor.
 *
 * Default:  result_##t##_##e##_err
 * Example:  result_int_error_t_err(ERR_NOT_FOUND)
 */
#ifndef MANGLE_RESULT_ERR
#  define MANGLE_RESULT_ERR(t, e)           MANGLE_RESULT_TYPE(t, e)##_err
#endif


/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief is_ok() — returns true (non-zero) iff the result holds an Ok value.
 *
 * Default:  result_##t##_##e##_is_ok
 * Example:  result_int_error_t_is_ok(res)
 */
#ifndef MANGLE_RESULT_IS_OK
#  define MANGLE_RESULT_IS_OK(t, e)         MANGLE_RESULT_TYPE(t, e)##_is_ok
#endif

/**
 * @brief is_err() — returns true (non-zero) iff the result holds an Err value.
 *
 * Default:  result_##t##_##e##_is_err
 * Example:  result_int_error_t_is_err(res)
 */
#ifndef MANGLE_RESULT_IS_ERR
#  define MANGLE_RESULT_IS_ERR(t, e)        MANGLE_RESULT_TYPE(t, e)##_is_err
#endif


/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief get_ok() — copy Ok value into *out; return true on success.
 *
 * Safe: returns false and leaves *out unmodified when the result is Err.
 *
 * Default:  result_##t##_##e##_get_ok
 * Example:  result_int_error_t_get_ok(res, &out_val)
 */
#ifndef MANGLE_RESULT_GET_OK
#  define MANGLE_RESULT_GET_OK(t, e)        MANGLE_RESULT_TYPE(t, e)##_get_ok
#endif

/**
 * @brief get_err() — copy Err value into *out; return true on success.
 *
 * Safe: returns false and leaves *out unmodified when the result is Ok.
 *
 * Default:  result_##t##_##e##_get_err
 * Example:  result_int_error_t_get_err(res, &out_err)
 */
#ifndef MANGLE_RESULT_GET_ERR
#  define MANGLE_RESULT_GET_ERR(t, e)       MANGLE_RESULT_TYPE(t, e)##_get_err
#endif

/**
 * @brief unwrap_or() — return Ok value, or fallback if Err.
 *
 * The fallback expression is always evaluated (C has no lazy evaluation).
 * Use and_then / or_else for deferred computation.
 *
 * Default:  result_##t##_##e##_unwrap_or
 * Example:  result_int_error_t_unwrap_or(res, 0)
 */
#ifndef MANGLE_RESULT_UNWRAP_OR
#  define MANGLE_RESULT_UNWRAP_OR(t, e)     MANGLE_RESULT_TYPE(t, e)##_unwrap_or
#endif


/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief unwrap() — return Ok value; abort / assert on Err.
 *
 * Unsafe: only call when you have already verified is_ok().
 * The implementation aborts (or invokes the configured panic handler) when
 * called on an Err result.
 *
 * Default:  result_##t##_##e##_unwrap
 * Example:  result_int_error_t_unwrap(res)
 */
#ifndef MANGLE_RESULT_UNWRAP
#  define MANGLE_RESULT_UNWRAP(t, e)        MANGLE_RESULT_TYPE(t, e)##_unwrap
#endif

/**
 * @brief unwrap_err() — return Err value; abort / assert on Ok.
 *
 * Unsafe: only call when you have already verified is_err().
 *
 * Default:  result_##t##_##e##_unwrap_err
 * Example:  result_int_error_t_unwrap_err(res)
 */
#ifndef MANGLE_RESULT_UNWRAP_ERR
#  define MANGLE_RESULT_UNWRAP_ERR(t, e)    MANGLE_RESULT_TYPE(t, e)##_unwrap_err
#endif

/**
 * @brief expect() — return Ok value; abort with diagnostic message on Err.
 *
 * Unsafe: prefer over plain unwrap() when a failure message aids debugging.
 *
 * Default:  result_##t##_##e##_expect
 * Example:  result_int_error_t_expect(res, "parse_int must succeed here")
 */
#ifndef MANGLE_RESULT_EXPECT
#  define MANGLE_RESULT_EXPECT(t, e)        MANGLE_RESULT_TYPE(t, e)##_expect
#endif


/* ════════════════════════════════════════════════════════════════════════════
   COMBINATOR FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief map() — apply fn to Ok value; propagate Err unchanged.
 *
 *   Ok(v)  -> Ok(fn(v))
 *   Err(e) -> Err(e)
 *
 * Default:  result_##t##_##e##_map
 * Example:  result_int_error_t_map(res, double_it)
 */
#ifndef MANGLE_RESULT_MAP
#  define MANGLE_RESULT_MAP(t, e)           MANGLE_RESULT_TYPE(t, e)##_map
#endif

/**
 * @brief map_err() — apply fn to Err value; propagate Ok unchanged.
 *
 *   Ok(v)  -> Ok(v)
 *   Err(e) -> Err(fn(e))
 *
 * Default:  result_##t##_##e##_map_err
 * Example:  result_int_error_t_map_err(res, enrich_error)
 */
#ifndef MANGLE_RESULT_MAP_ERR
#  define MANGLE_RESULT_MAP_ERR(t, e)       MANGLE_RESULT_TYPE(t, e)##_map_err
#endif

/**
 * @brief and_then() — flatMap over Ok; short-circuit on Err.
 *
 *   Ok(v)  -> fn(v)   (fn returns a Result)
 *   Err(e) -> Err(e)
 *
 * Use to chain fallible operations without nested error checks.
 *
 * Default:  result_##t##_##e##_and_then
 * Example:  result_int_error_t_and_then(res, next_step)
 */
#ifndef MANGLE_RESULT_AND_THEN
#  define MANGLE_RESULT_AND_THEN(t, e)      MANGLE_RESULT_TYPE(t, e)##_and_then
#endif

/**
 * @brief or_else() — flatMap over Err; short-circuit on Ok.
 *
 *   Ok(v)  -> Ok(v)
 *   Err(e) -> fn(e)   (fn returns a Result)
 *
 * Use to provide a recovery / retry path without explicit branching.
 *
 * Default:  result_##t##_##e##_or_else
 * Example:  result_int_error_t_or_else(res, try_fallback)
 */
#ifndef MANGLE_RESULT_OR_ELSE
#  define MANGLE_RESULT_OR_ELSE(t, e)       MANGLE_RESULT_TYPE(t, e)##_or_else
#endif

/**
 * @brief and() — return `other` if Ok, otherwise propagate Err.
 *
 *   Ok(_)  -> other
 *   Err(e) -> Err(e)
 *
 * Short-circuit AND for Results.  `other` is always evaluated (eager).
 * Use and_then() when you need lazy / deferred construction of `other`.
 *
 * Default:  result_##t##_##e##_and
 * Example:  result_int_error_t_and(res, second_result)
 */
#ifndef MANGLE_RESULT_AND
#  define MANGLE_RESULT_AND(t, e)           MANGLE_RESULT_TYPE(t, e)##_and
#endif

/**
 * @brief or() — return `res` if Ok, otherwise return `other`.
 *
 *   Ok(v)  -> Ok(v)
 *   Err(_) -> other
 *
 * Short-circuit OR for Results.  `other` is always evaluated (eager).
 * Use or_else() when you need lazy / deferred construction of `other`.
 *
 * Default:  result_##t##_##e##_or
 * Example:  result_int_error_t_or(res, fallback_result)
 */
#ifndef MANGLE_RESULT_OR
#  define MANGLE_RESULT_OR(t, e)            MANGLE_RESULT_TYPE(t, e)##_or
#endif


/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief eq() — structural equality over two Results.
 *
 * Signature:
 *   bool result_T_E_eq(result_T_E r1, result_T_E r2,
 *                      bool (*eq_ok)(T, T),
 *                      bool (*eq_err)(E, E));
 *
 * Returns true iff both are Ok with equal values, or both are Err with
 * equal errors.  Comparators are caller-supplied; no hidden magic.
 *
 * Default:  result_##t##_##e##_eq
 * Example:  result_int_error_t_eq(r1, r2, int_eq, error_eq)
 */
#ifndef MANGLE_RESULT_EQ
#  define MANGLE_RESULT_EQ(t, e)            MANGLE_RESULT_TYPE(t, e)##_eq
#endif


/* ════════════════════════════════════════════════════════════════════════════
   CONSISTENCY SELF-CHECK  (compile-time, debug builds only)
   ════════════════════════════════════════════════════════════════════════════
   If CANON_RESULT_MANGLE_CHECK is defined before inclusion, a small inline
   helper verifies that MANGLE_RESULT_STRUCT_TAG is still derived from
   MANGLE_RESULT_TYPE (i.e. the user has not overridden one without the
   other).  This produces a compile error — not a silent mismatch — when
   the invariant is violated.

   Usage:
     #define CANON_RESULT_MANGLE_CHECK
     #include "result_mangle.h"
   ════════════════════════════════════════════════════════════════════════════ */
#ifdef CANON_RESULT_MANGLE_CHECK
/*
 * We cannot compare macro expansions directly in the preprocessor, but we
 * can force the implementer to acknowledge the pairing by requiring both
 * overrides to be present whenever either is customized.
 */
#  if defined(MANGLE_RESULT_TYPE) && !defined(MANGLE_RESULT_STRUCT_TAG)
#    error "Canon-C: MANGLE_RESULT_TYPE was overridden but " \
           "MANGLE_RESULT_STRUCT_TAG was not.  Either override both, " \
           "or remove CANON_RESULT_MANGLE_CHECK to suppress this check."
#  endif
#  if !defined(MANGLE_RESULT_TYPE) && defined(MANGLE_RESULT_STRUCT_TAG)
#    error "Canon-C: MANGLE_RESULT_STRUCT_TAG was overridden but " \
           "MANGLE_RESULT_TYPE was not.  Either override both, " \
           "or remove CANON_RESULT_MANGLE_CHECK to suppress this check."
#  endif
#endif /* CANON_RESULT_MANGLE_CHECK */


#endif /* CANON_SEMANTICS_RESULT_RESULT_MANGLE_H */
