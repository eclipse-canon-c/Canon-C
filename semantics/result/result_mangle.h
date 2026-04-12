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
 *            CANON_RESULT(int,   cstr_t)   // ok
 *            CANON_RESULT(vptr_t, int)     // ok
 *
 *          Passing raw pointer syntax will produce a hard compiler error or,
 *          worse, a silently malformed identifier with no diagnostic:
 *
 *            CANON_RESULT(int, const char *)   // WRONG — do not do this
 *
 * ────────────────────────────────────────────────────────────────────────────
 * Override / customization
 * ────────────────────────────────────────────────────────────────────────────
 * Define any subset of the macros below BEFORE including this header.
 * Overrides are per-translation-unit; different TUs may use different naming
 * schemes without conflict.
 *
 * Rule: if you override MANGLE_RESULT_TYPE you MUST also override
 *       MANGLE_RESULT_STRUCT_TAG so that the struct tag and typedef remain
 *       consistent.  Mismatching the two causes a struct-tag / typedef
 *       mismatch that the compiler will catch, but the diagnostic may be
 *       confusing.
 *
 * IMPORTANT — Why each macro expands t and e directly
 * ────────────────────────────────────────────────────────────────────────────
 * The C preprocessor's ## operator pastes preprocessing tokens, not the
 * results of macro expansions.  Constructs such as
 *
 *   MANGLE_RESULT_TYPE(t, e)##_suffix
 *
 * are ill-formed: ## sees the closing ')' of the macro call, not the
 * expanded identifier, and the compiler emits a hard error:
 *
 *   error: pasting ")" and "_suffix" does not give a valid preprocessing token
 *
 * Every macro in this file therefore expands t and e directly with ##,
 * never nesting another macro invocation inside a ## expression.
 * MANGLE_RESULT_STRUCT_TAG is NOT derived from MANGLE_RESULT_TYPE for this
 * reason — it expands t and e independently and appends _s itself.
 *
 * Customization example — Haskell-style Either / Right / Left:
 *
 *   #define MANGLE_RESULT_TYPE(t, e)       Either_##t##_##e
 *   #define MANGLE_RESULT_STRUCT_TAG(t, e) Either_##t##_##e##_s
 *   #define MANGLE_RESULT_IS_OK(t, e)      Either_##t##_##e##_isRight
 *   #define MANGLE_RESULT_IS_ERR(t, e)     Either_##t##_##e##_isLeft
 *   #define MANGLE_RESULT_OK(t, e)         Either_##t##_##e##_Right
 *   #define MANGLE_RESULT_ERR(t, e)        Either_##t##_##e##_Left
 *   #include "result.h"
 *
 *   CANON_RESULT(int, error_t)
 *   Either_int_error_t x = Either_int_error_t_Right(42);
 *
 * Customization example — project-scoped prefix:
 *
 *   #define MANGLE_RESULT_TYPE(t, e)       myproj_res_##t##_##e
 *   #define MANGLE_RESULT_STRUCT_TAG(t, e) myproj_res_##t##_##e##_s
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
 *     result_T_E_s            — underlying struct tag
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
   CONSISTENCY NOTE  (documentation only — no preprocessor logic)
   ────────────────────────────────────────────────────────────────────────────
   MANGLE_RESULT_TYPE and MANGLE_RESULT_STRUCT_TAG are paired: the typedef
   references the struct tag, so if you override one you MUST override the
   other. Mismatching them does not silently go wrong — the compiler will
   produce a struct-tag / typedef mismatch diagnostic — but that diagnostic
   is harder to read than this note.

   Earlier versions of this file attempted to enforce the pairing with an
   opt-in compile-time check gated on CANON_RESULT_MANGLE_CHECK:

       #ifdef CANON_RESULT_MANGLE_CHECK
       #  if defined(MANGLE_RESULT_TYPE) && !defined(MANGLE_RESULT_STRUCT_TAG)
       #    error "..."
       #  endif
       ...

   That approach was removed because static analyzers (cppcheck in
   particular) explore every #ifdef branch independently and will trigger
   the #error on configurations that cannot occur in real compilation,
   producing cascading false positives on every CANON_RESULT(...) call
   site. The compiler's native struct-tag diagnostic is good enough for
   the narrow case the check was guarding against, so no runtime or
   compile-time logic is needed here — only this documentation.

   If you override MANGLE_RESULT_TYPE, also override MANGLE_RESULT_STRUCT_TAG.
   ════════════════════════════════════════════════════════════════════════════ */


/* ════════════════════════════════════════════════════════════════════════════
   TYPE NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Main Result type name (typedef).
 *
 * Default:  result_##t##_##e
 * Example:  result_int_error
 *
 * Override to change the naming across your entire translation unit.
 * If you override this, you MUST also override MANGLE_RESULT_STRUCT_TAG
 * so the struct tag and typedef stay consistent.
 *
 * @warning t and e must each be a single C preprocessing token (no spaces,
 *          no punctuation).  See file-level note on token constraints.
 */
#ifndef MANGLE_RESULT_TYPE
#  define MANGLE_RESULT_TYPE(t, e)          result_##t##_##e
#endif

/**
 * @brief Underlying struct tag for the Result type.
 *
 * Default:  result_##t##_##e##_s
 * Example:  result_int_error_s
 *
 * This macro expands t and e directly — it does NOT derive from
 * MANGLE_RESULT_TYPE via ##.  Chaining ## onto a macro invocation is
 * ill-formed in C99 (the preprocessor sees ')' not the expanded token).
 * Both macros independently paste t and e, which guarantees the struct tag
 * is always result_##t##_##e##_s and the typedef is result_##t##_##e,
 * keeping them consistent by construction.
 *
 * Override this separately ONLY when you need explicit control over the
 * struct tag (e.g. forward declarations in another header).  Whenever
 * MANGLE_RESULT_TYPE is overridden, this macro MUST be overridden too.
 */
#ifndef MANGLE_RESULT_STRUCT_TAG
#  define MANGLE_RESULT_STRUCT_TAG(t, e)    result_##t##_##e##_s
#endif


/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTOR FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ok(value) constructor.
 *
 * Default:  result_##t##_##e##_ok
 * Example:  result_int_error_ok(42)
 */
#ifndef MANGLE_RESULT_OK
#  define MANGLE_RESULT_OK(t, e)            result_##t##_##e##_ok
#endif

/**
 * @brief Err(error) constructor.
 *
 * Default:  result_##t##_##e##_err
 * Example:  result_int_error_err(ERR_NOT_FOUND)
 */
#ifndef MANGLE_RESULT_ERR
#  define MANGLE_RESULT_ERR(t, e)           result_##t##_##e##_err
#endif


/* ════════════════════════════════════════════════════════════════════════════
   QUERY FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief is_ok() — returns true (non-zero) iff the result holds an Ok value.
 *
 * Default:  result_##t##_##e##_is_ok
 * Example:  result_int_error_is_ok(res)
 */
#ifndef MANGLE_RESULT_IS_OK
#  define MANGLE_RESULT_IS_OK(t, e)         result_##t##_##e##_is_ok
#endif

/**
 * @brief is_err() — returns true (non-zero) iff the result holds an Err value.
 *
 * Default:  result_##t##_##e##_is_err
 * Example:  result_int_error_is_err(res)
 */
#ifndef MANGLE_RESULT_IS_ERR
#  define MANGLE_RESULT_IS_ERR(t, e)        result_##t##_##e##_is_err
#endif


/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION FUNCTION NAMES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief get_ok() — copy Ok value into *out; return true on success.
 *
 * Safe: returns false when the result is Err.
 * Contract: out must not be NULL — caught by require() in the definition.
 *
 * Default:  result_##t##_##e##_get_ok
 * Example:  result_int_error_get_ok(res, &out_val)
 */
#ifndef MANGLE_RESULT_GET_OK
#  define MANGLE_RESULT_GET_OK(t, e)        result_##t##_##e##_get_ok
#endif

/**
 * @brief get_err() — copy Err value into *out; return true on success.
 *
 * Safe: returns false when the result is Ok.
 * Contract: out must not be NULL — caught by require() in the definition.
 *
 * Default:  result_##t##_##e##_get_err
 * Example:  result_int_error_get_err(res, &out_err)
 */
#ifndef MANGLE_RESULT_GET_ERR
#  define MANGLE_RESULT_GET_ERR(t, e)       result_##t##_##e##_get_err
#endif

/**
 * @brief unwrap_or() — return Ok value, or fallback if Err.
 *
 * The fallback expression is always evaluated (C has no lazy evaluation).
 * Use and_then / or_else for deferred computation.
 *
 * Default:  result_##t##_##e##_unwrap_or
 * Example:  result_int_error_unwrap_or(res, 0)
 */
#ifndef MANGLE_RESULT_UNWRAP_OR
#  define MANGLE_RESULT_UNWRAP_OR(t, e)     result_##t##_##e##_unwrap_or
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
 * Example:  result_int_error_unwrap(res)
 */
#ifndef MANGLE_RESULT_UNWRAP
#  define MANGLE_RESULT_UNWRAP(t, e)        result_##t##_##e##_unwrap
#endif

/**
 * @brief unwrap_err() — return Err value; abort / assert on Ok.
 *
 * Unsafe: only call when you have already verified is_err().
 *
 * Default:  result_##t##_##e##_unwrap_err
 * Example:  result_int_error_unwrap_err(res)
 */
#ifndef MANGLE_RESULT_UNWRAP_ERR
#  define MANGLE_RESULT_UNWRAP_ERR(t, e)    result_##t##_##e##_unwrap_err
#endif

/**
 * @brief expect() — return Ok value; abort with diagnostic message on Err.
 *
 * Unsafe: prefer over plain unwrap() when a failure message aids debugging.
 *
 * Default:  result_##t##_##e##_expect
 * Example:  result_int_error_expect(res, "parse_int must succeed here")
 */
#ifndef MANGLE_RESULT_EXPECT
#  define MANGLE_RESULT_EXPECT(t, e)        result_##t##_##e##_expect
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
 * Example:  result_int_error_map(res, double_it)
 */
#ifndef MANGLE_RESULT_MAP
#  define MANGLE_RESULT_MAP(t, e)           result_##t##_##e##_map
#endif

/**
 * @brief map_err() — apply fn to Err value; propagate Ok unchanged.
 *
 *   Ok(v)  -> Ok(v)
 *   Err(e) -> Err(fn(e))
 *
 * Default:  result_##t##_##e##_map_err
 * Example:  result_int_error_map_err(res, enrich_error)
 */
#ifndef MANGLE_RESULT_MAP_ERR
#  define MANGLE_RESULT_MAP_ERR(t, e)       result_##t##_##e##_map_err
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
 * Example:  result_int_error_and_then(res, next_step)
 */
#ifndef MANGLE_RESULT_AND_THEN
#  define MANGLE_RESULT_AND_THEN(t, e)      result_##t##_##e##_and_then
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
 * Example:  result_int_error_or_else(res, try_fallback)
 */
#ifndef MANGLE_RESULT_OR_ELSE
#  define MANGLE_RESULT_OR_ELSE(t, e)       result_##t##_##e##_or_else
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
 * Example:  result_int_error_and(res, second_result)
 */
#ifndef MANGLE_RESULT_AND
#  define MANGLE_RESULT_AND(t, e)           result_##t##_##e##_and
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
 * Example:  result_int_error_or(res, fallback_result)
 */
#ifndef MANGLE_RESULT_OR
#  define MANGLE_RESULT_OR(t, e)            result_##t##_##e##_or
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
 * Example:  result_int_error_eq(r1, r2, int_eq, error_eq)
 */
#ifndef MANGLE_RESULT_EQ
#  define MANGLE_RESULT_EQ(t, e)            result_##t##_##e##_eq
#endif


/* ════════════════════════════════════════════════════════════════════════════
   END OF MANGLING MACROS
   ════════════════════════════════════════════════════════════════════════════ */


#endif /* CANON_SEMANTICS_RESULT_RESULT_MANGLE_H */
