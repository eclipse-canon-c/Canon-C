// semantics/result/result_impl.h
#ifndef CANON_RESULT_IMPL_H
#define CANON_RESULT_IMPL_H

#include <stdbool.h>

#include "../../core/primitives/types.h"
#include "../../core/primitives/contract.h"

/**
 * @file result_impl.h
 * @brief Pure implementation logic for Result<T, E> type (no name mangling)
 *
 * This file contains the actual behaviour of each Result operation as
 * individual macros. These implementation macros are generic: they take type
 * parameters without any naming conventions applied and can be overridden for
 * custom types by redefining them before including result_defn.h.
 *
 * Philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Pure logic: No naming conventions, pure behaviour
 * - Overridable: Redefine any IMPL_RESULT_* before including result_defn.h
 * - Composable: Used by result_defn.h to build concrete types
 * - Explicit: All behaviour is visible in macro definitions
 * - Two type parameters: Handles both value type (T) and error type (E)
 *
 * Contract for every IMPL macro:
 * ────────────────────────────────────────────────────────────────────────────
 * Every macro in this file MUST expand to a brace-enclosed function body
 * of the form { ... } with NO trailing semicolon. result_defn.h relies on
 * this — it concatenates macro expansions directly after the function
 * signature with no intervening tokens.
 *
 * C99 compliance notes:
 * ────────────────────────────────────────────────────────────────────────────
 * - The default struct layout uses a NAMED union member (.val) to remain
 *   strictly C99 compliant. Anonymous unions inside structs are a C11
 *   extension (ISO/IEC 9899:2011 §6.7.2.1¶13). Define
 *   CANON_RESULT_ANON_UNION before including this header to opt in to
 *   anonymous union syntax; see IMPL_RESULT_STRUCT below.
 * - All macro parameters that appear more than once are captured into local
 *   variables at the top of each macro body to prevent double evaluation.
 * - <stdbool.h> is included explicitly so bool/true/false are always defined.
 *
 * CANON_RESULT_ANON_UNION — struct layout opt-in:
 * ────────────────────────────────────────────────────────────────────────────
 * When defined: union member is unnamed → access as r.ok / r.err
 * When absent:  union member is named   → access as r.val.ok / r.val.err  (default)
 *
 * IMPORTANT: result_decl.h exposes the same flag via CANON_RESULT_UNION_BODY_.
 * Both files must be included with the same setting of CANON_RESULT_ANON_UNION
 * in every translation unit that uses a given Result type, or the struct
 * layout will differ between the declaration and the definition — a silent
 * ABI break. The safest approach is to set the flag (or not) in a project-
 * wide prefix header rather than per-file.
 *
 * Performance (all operations):
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) — all operations are constant time
 * - Space complexity: O(1) — no allocations, pure stack operations
 * - Memory layout: sizeof(bool) + max(sizeof(T), sizeof(E)) + padding
 * - No dynamic allocation, no function call overhead (inline)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations are thread-safe on independent Result instances
 * - No shared state between Result values
 * - Union access requires is_ok check (accessing wrong member is UB)
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (compound literals, designated initializers)
 * - Named union member (.val) is used by default for C99 compatibility
 * - Uses Canon-C contract.h for assertions (require)
 * - No platform-specific code
 *
 * map / map_err type constraint:
 * ────────────────────────────────────────────────────────────────────────────
 * IMPL_RESULT_MAP and IMPL_RESULT_MAP_ERR constrain the transformation
 * function to T → T and E → E respectively. Cross-type mapping (T → U)
 * requires a distinct Result type and must be handled at the call site
 * using and_then / or_else with a function that constructs the target
 * Result type directly.
 *
 * Customization example:
 * ────────────────────────────────────────────────────────────────────────────
 * // Specialise Result<void_ptr, error> to reject NULL at construction time.
 * // Override before including result_defn.h:
 *
 * #undef  IMPL_RESULT_OK
 * #define IMPL_RESULT_OK(_t, _e, _tres, _param) \
 *     { \
 *         require((_param) != NULL, "result_ok: NULL pointer not allowed"); \
 *         return (_tres){ .is_ok = true, .val.ok = (_param) }; \
 *     }
 *
 * @sa result_mangle.h, result_decl.h, result_defn.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   STRUCT LAYOUT
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * IMPL_RESULT_UNION_ — internal helper that selects the union layout.
 *
 * Named form (default, strict C99):
 *   union { _t ok; _e err; } val;
 *   Access: r.val.ok, r.val.err
 *
 * Anonymous form (opt-in via CANON_RESULT_ANON_UNION, requires C11 or a
 * compiler that documents anonymous struct/union as a supported extension):
 *   union { _t ok; _e err; };
 *   Access: r.ok, r.err
 *
 * This macro mirrors the CANON_RESULT_UNION_BODY_ macro in result_decl.h so
 * that the struct layout produced by IMPL_RESULT_STRUCT (used by
 * DEFINE_RESULT_STRUCT in result_defn.h) is always identical to the layout
 * produced by DECLARE_RESULT_STRUCT (used in result_decl.h). Keeping both
 * files in sync on this flag is the caller's responsibility; see the
 * CANON_RESULT_ANON_UNION note in the file-level documentation above.
 */
#ifdef CANON_RESULT_ANON_UNION
#  define IMPL_RESULT_UNION_(_t, _e) \
       union { _t ok; _e err; };
#else
#  define IMPL_RESULT_UNION_(_t, _e) \
       union { _t ok; _e err; } val;
#endif

/**
 * @brief Result struct layout implementation
 *
 * Expands to the body of the Result struct (braces included, no trailing
 * semicolon — result_defn.h appends the semicolon).
 *
 * Memory layout: { bool is_ok; union { T ok; E err; } [val]; }
 *
 * Only one of val.ok / val.err (or ok / err with CANON_RESULT_ANON_UNION)
 * is valid at any time, determined by is_ok. Accessing the wrong member
 * is undefined behaviour.
 *
 * Performance:
 * - Space: sizeof(bool) + max(sizeof(T), sizeof(E)) + alignment padding
 * - Union saves memory compared to storing both T and E separately
 *
 * @param _t The success value type
 * @param _e The error value type
 */
#define IMPL_RESULT_STRUCT(_t, _e) \
    { \
        bool is_ok; \
        IMPL_RESULT_UNION_(_t, _e) \
    }

/* ════════════════════════════════════════════════════════════════════════════
   INTERNAL ACCESS HELPER
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * IMPL_RESULT_OK_FIELD_ / IMPL_RESULT_ERR_FIELD_
 *
 * Abstract over the named (.val.ok / .val.err) vs anonymous (.ok / .err)
 * union access so that all IMPL macros below remain correct regardless of
 * whether CANON_RESULT_ANON_UNION is set.
 *
 * All IMPL macros use these helpers instead of hard-coding .val.ok — this
 * is the single point of change for union access syntax.
 */
#ifdef CANON_RESULT_ANON_UNION
#  define IMPL_RESULT_OK_FIELD_(_r)   (_r).ok
#  define IMPL_RESULT_ERR_FIELD_(_r)  (_r).err
#else
#  define IMPL_RESULT_OK_FIELD_(_r)   (_r).val.ok
#  define IMPL_RESULT_ERR_FIELD_(_r)  (_r).val.err
#endif

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTORS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Constructs Ok(value) — a successful result
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t     The value type
 * @param _e     The error type
 * @param _tres  The Result type name
 * @param _param The value to wrap
 */
#ifdef CANON_RESULT_ANON_UNION
#  define IMPL_RESULT_OK(_t, _e, _tres, _param) \
     { return (_tres){ .is_ok = true, .ok = (_param) }; }
#else
#  define IMPL_RESULT_OK(_t, _e, _tres, _param) \
     { return (_tres){ .is_ok = true, .val = { .ok = (_param) } }; }
#endif

/**
 * @brief Constructs Err(error) — a failed result
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t     The value type
 * @param _e     The error type
 * @param _tres  The Result type name
 * @param _param The error to wrap
 */
#ifdef CANON_RESULT_ANON_UNION
#  define IMPL_RESULT_ERR(_t, _e, _tres, _param) \
     { return (_tres){ .is_ok = false, .err = (_param) }; }
#else
#  define IMPL_RESULT_ERR(_t, _e, _tres, _param) \
     { return (_tres){ .is_ok = false, .val = { .err = (_param) } }; }
#endif

/* ════════════════════════════════════════════════════════════════════════════
   QUERIES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Checks if Result is Ok
 *
 * Performance: O(1) time, O(1) space (single boolean read)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to check
 */
#define IMPL_RESULT_IS_OK(_t, _e, _r) \
    { return (_r).is_ok; }

/**
 * @brief Checks if Result is Err
 *
 * Performance: O(1) time, O(1) space (single boolean negation)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to check
 */
#define IMPL_RESULT_IS_ERR(_t, _e, _r) \
    { return !(_r).is_ok; }

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Safely extracts success value if Ok
 *
 * Contract: _out must not be NULL — a NULL pointer is a programmer error
 * caught by require(). The return value is unambiguous: false means the
 * Result is Err, never that the caller passed a bad pointer.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * @param _t   The value type
 * @param _e   The error type
 * @param _r   The Result to extract from
 * @param _out Non-NULL pointer to store the value
 */
#define IMPL_RESULT_GET_OK(_t, _e, _r, _out) \
    { \
        require((_out) != NULL, "result_get_ok: output pointer must not be NULL"); \
        if ((_r).is_ok) { \
            *(_out) = IMPL_RESULT_OK_FIELD_(_r); \
            return true; \
        } \
        return false; \
    }

/**
 * @brief Safely extracts error value if Err
 *
 * Contract: _out must not be NULL — a NULL pointer is a programmer error
 * caught by require(). The return value is unambiguous: false means the
 * Result is Ok, never that the caller passed a bad pointer.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * @param _t   The value type
 * @param _e   The error type
 * @param _r   The Result to extract from
 * @param _out Non-NULL pointer to store the error
 */
#define IMPL_RESULT_GET_ERR(_t, _e, _r, _out) \
    { \
        require((_out) != NULL, "result_get_err: output pointer must not be NULL"); \
        if (!(_r).is_ok) { \
            *(_out) = IMPL_RESULT_ERR_FIELD_(_r); \
            return true; \
        } \
        return false; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   EXTRACTION WITH DEFAULT
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extracts value or returns fallback
 *
 * Safe alternative to unwrap() — never panics.
 * _r is captured to prevent double evaluation.
 *
 * Performance: O(1) time, O(1) space (conditional return)
 *
 * @param _t        The value type
 * @param _e        The error type
 * @param _r        The Result to extract from
 * @param _fallback Value to return if Err
 */
#define IMPL_RESULT_UNWRAP_OR(_t, _e, _r, _fallback) \
    { \
        _t const _res_val_ = (_r).is_ok \
            ? IMPL_RESULT_OK_FIELD_(_r) \
            : (_fallback); \
        return _res_val_; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION (REQUIRES OK / ERR)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extracts success value, panicking if Err
 *
 * ⚠️  Only use when certain the Result is Ok.
 * Prefer get_ok(), unwrap_or(), or expect() in production.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: aborts with message if Result is Err
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to unwrap
 */
#define IMPL_RESULT_UNWRAP(_t, _e, _r) \
    { \
        require((_r).is_ok, "result_unwrap: called on Err value"); \
        return IMPL_RESULT_OK_FIELD_(_r); \
    }

/**
 * @brief Extracts error value, panicking if Ok
 *
 * The opposite of unwrap() — extracts the error from Err.
 * Useful in tests or when failure is the expected invariant.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: aborts with message if Result is Ok
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to unwrap
 */
#define IMPL_RESULT_UNWRAP_ERR(_t, _e, _r) \
    { \
        require(!(_r).is_ok, "result_unwrap_err: called on Ok value"); \
        return IMPL_RESULT_ERR_FIELD_(_r); \
    }

/**
 * @brief Extracts value with a custom panic message
 *
 * Like unwrap(), but the diagnostic message is provided by the caller.
 * Use to document invariant violations at the call site.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: aborts printing _msg if Result is Err
 *
 * @param _t   The value type
 * @param _e   The error type
 * @param _r   The Result to extract from
 * @param _msg Error message for the contract violation
 */
#define IMPL_RESULT_EXPECT(_t, _e, _r, _msg) \
    { \
        require((_r).is_ok, (_msg)); \
        return IMPL_RESULT_OK_FIELD_(_r); \
    }

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transforms the success value if Ok (T → T only)
 *
 * Applies _f to the success value and wraps the result back into the same
 * Result type. If Err, returns the original Result unchanged.
 *
 * Type constraint: _f must be T → T. For T → U use and_then() with a
 * function returning the target Result type.
 *
 * _r is captured into a local to prevent double evaluation.
 *
 * Performance: O(_f) time, O(1) space
 *
 * @param _t     The value type
 * @param _e     The error type
 * @param _tres  The Result type name
 * @param _r     The Result to transform
 * @param _f     Transformation function: T → T
 * @param _ok_fn Constructor function for Ok (from mangle.h)
 */
#define IMPL_RESULT_MAP(_t, _e, _tres, _r, _f, _ok_fn) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok \
            ? _ok_fn((_f)(IMPL_RESULT_OK_FIELD_(_res_))) \
            : _res_; \
    }

/**
 * @brief Transforms the error value if Err (E → E only)
 *
 * Applies _f to the error value and wraps the result back into the same
 * Result type. If Ok, returns the original Result unchanged.
 *
 * Type constraint: _f must be E → E. Same rationale as IMPL_RESULT_MAP.
 *
 * _r is captured into a local to prevent double evaluation.
 *
 * Performance: O(_f) time, O(1) space
 *
 * @param _t      The value type
 * @param _e      The error type
 * @param _tres   The Result type name
 * @param _r      The Result to transform
 * @param _f      Error transformation function: E → E
 * @param _err_fn Constructor function for Err (from mangle.h)
 */
#define IMPL_RESULT_MAP_ERR(_t, _e, _tres, _r, _f, _err_fn) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok \
            ? _res_ \
            : _err_fn((_f)(IMPL_RESULT_ERR_FIELD_(_res_))); \
    }

/**
 * @brief Chains Result-returning operations (flatMap / bind)
 *
 * If Ok, passes the value to _f and returns _f's Result.
 * If Err, returns the original Result unchanged without calling _f.
 * Prevents Result<Result<T,E>,E> nesting.
 *
 * _r is captured into a local to prevent double evaluation.
 *
 * Performance: O(_f) time, O(1) space
 *
 * @param _t    The value type
 * @param _e    The error type
 * @param _tres The Result type name
 * @param _r    The Result to chain from
 * @param _f    Function returning Result: T → Result<T, E>
 */
#define IMPL_RESULT_AND_THEN(_t, _e, _tres, _r, _f) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok \
            ? (_f)(IMPL_RESULT_OK_FIELD_(_res_)) \
            : _res_; \
    }

/**
 * @brief Provides an alternative Result if Err
 *
 * If Ok, returns the original Result unchanged.
 * If Err, calls _f with the error value and returns _f's Result.
 *
 * _r is captured into a local to prevent double evaluation.
 *
 * Performance: O(1) if Ok, O(_f) if Err, O(1) space
 *
 * @param _t    The value type
 * @param _e    The error type
 * @param _tres The Result type name
 * @param _r    The Result to check
 * @param _f    Recovery function: E → Result<T, E>
 */
#define IMPL_RESULT_OR_ELSE(_t, _e, _tres, _r, _f) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok \
            ? _res_ \
            : (_f)(IMPL_RESULT_ERR_FIELD_(_res_)); \
    }

/**
 * @brief Eager short-circuit AND — returns other if Ok, else propagates Err
 *
 *   Ok(_)  -> other
 *   Err(e) -> Err(e)
 *
 * Both _r and _other are evaluated before the call (eager). Use and_then()
 * when deferred / lazy construction of the second value is needed.
 *
 * _r is captured into a local to prevent double evaluation.
 *
 * Performance: O(1) time, O(1) space
 *
 * @param _t     The value type
 * @param _e     The error type
 * @param _tres  The Result type name
 * @param _r     The first Result
 * @param _other The second Result (always evaluated)
 */
#define IMPL_RESULT_AND(_t, _e, _tres, _r, _other) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok ? (_other) : _res_; \
    }

/**
 * @brief Eager short-circuit OR — returns r if Ok, else returns other
 *
 *   Ok(v)  -> Ok(v)
 *   Err(_) -> other
 *
 * Both _r and _other are evaluated before the call (eager). Use or_else()
 * when deferred / lazy construction of the fallback value is needed.
 *
 * _r is captured into a local to prevent double evaluation.
 *
 * Performance: O(1) time, O(1) space
 *
 * @param _t     The value type
 * @param _e     The error type
 * @param _tres  The Result type name
 * @param _r     The first Result
 * @param _other The fallback Result (always evaluated)
 */
#define IMPL_RESULT_OR(_t, _e, _tres, _r, _other) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok ? _res_ : (_other); \
    }

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Checks structural equality of two Results
 *
 * Two Results are equal if and only if:
 * - Both are Ok  and _eq_ok (_r1.ok,  _r2.ok)  returns true, OR
 * - Both are Err and _eq_err(_r1.err, _r2.err) returns true.
 * An Ok and an Err are never equal regardless of their contained values.
 *
 * Both _r1 and _r2 are captured into locals to prevent double evaluation
 * of potentially side-effecting expressions.
 *
 * Performance: O(1)        for Ok-vs-Err or Err-vs-Ok
 *              O(_eq_ok)   for Ok-vs-Ok
 *              O(_eq_err)  for Err-vs-Err
 *              O(1) space
 *
 * @param _t      The value type
 * @param _e      The error type
 * @param _r1     First Result
 * @param _r2     Second Result
 * @param _eq_ok  Equality function for values:  (T, T) → bool
 * @param _eq_err Equality function for errors:  (E, E) → bool
 */
#define IMPL_RESULT_EQ(_t, _e, _r1, _r2, _eq_ok, _eq_err) \
    { \
        bool const _r1_ok_ = (_r1).is_ok; \
        bool const _r2_ok_ = (_r2).is_ok; \
        if (_r1_ok_ != _r2_ok_) return false; \
        if (_r1_ok_) return (_eq_ok)( \
            IMPL_RESULT_OK_FIELD_(_r1), \
            IMPL_RESULT_OK_FIELD_(_r2)); \
        return (_eq_err)( \
            IMPL_RESULT_ERR_FIELD_(_r1), \
            IMPL_RESULT_ERR_FIELD_(_r2)); \
    }

#endif /* CANON_RESULT_IMPL_H */
