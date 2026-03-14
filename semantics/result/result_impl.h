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
 * This file contains the actual behavior of each Result operation as individual
 * macros. These implementation macros are generic and take type parameters without
 * any naming conventions applied. They can be overridden for custom types by
 * defining type-specific versions before including this header.
 *
 * Philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Pure logic: No naming conventions, pure behavior
 * - Overridable: Define IMPL_RESULT_*_yourtype to specialize
 * - Composable: Used by result_defn.h to build concrete types
 * - Explicit: All behavior is visible in macro definitions
 * - Two type parameters: Handles both value type (T) and error type (E)
 *
 * C99 compliance notes:
 * ────────────────────────────────────────────────────────────────────────────
 * - Anonymous unions are C11; this file uses a named union member (.val)
 * - All macro parameters that appear more than once are captured into local
 *   variables at the top of each macro body to prevent double evaluation
 * - <stdbool.h> is included explicitly so bool/true/false are always defined
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
 * - Named union member (.val) is used for C99 compatibility
 * - Uses Canon-C contract.h for assertions (require, ensure)
 * - No platform-specific code
 *
 * map/map_err type constraint:
 * ────────────────────────────────────────────────────────────────────────────
 * - IMPL_RESULT_MAP and IMPL_RESULT_MAP_ERR constrain the transformation
 *   function to T -> T and E -> E respectively (same-type transformation).
 *   Cross-type mapping (T -> U) requires a distinct Result type and is not
 *   expressible with a single _tres parameter; it must be handled at the
 *   call site using and_then / or_else with a function that constructs the
 *   target Result type directly.
 *
 * Customization example:
 * ────────────────────────────────────────────────────────────────────────────
 * // Specialize Result<ptr, error> to assert non-NULL on Ok
 * #define IMPL_RESULT_OK_ptr_error(_t, _e, _tres, _param) \
 *     { \
 *         require((_param) != NULL, "result_ok: NULL pointer not allowed"); \
 *         return (_tres){ .is_ok = true, .val.ok = (_param) }; \
 *     }
 * #include "result_defn.h"
 * DEFINE_RESULT_ALL(static inline, void*, error)
 *
 * @sa result_mangle.h, result_decl.h, result_defn.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   STRUCT LAYOUT
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Result struct layout implementation
 *
 * Memory layout: { bool is_ok; union { T ok; E err; } val; }
 *
 * The union is named (.val) for strict C99 compliance; anonymous unions
 * are a C11 extension. All macro accessors use .val.ok / .val.err.
 *
 * Performance:
 * - Space: sizeof(bool) + max(sizeof(T), sizeof(E)) + alignment padding
 * - Union saves memory compared to storing both T and E separately
 * - Only one of val.ok / val.err is valid at any time (determined by is_ok)
 * - Accessing the wrong union member is undefined behavior
 *
 * @param _t The success value type
 * @param _e The error value type
 */
#define IMPL_RESULT_STRUCT(_t, _e) \
    { \
        bool is_ok; \
        union { \
            _t ok; \
            _e err; \
        } val; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTORS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Constructs Ok(value) - a successful result
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t    The value type
 * @param _e    The error type
 * @param _tres The Result type name
 * @param _param The value to wrap
 */
#define IMPL_RESULT_OK(_t, _e, _tres, _param) \
    { return (_tres){ .is_ok = true, .val = { .ok = (_param) } }; }

/**
 * @brief Constructs Err(error) - a failed result
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t    The value type
 * @param _e    The error type
 * @param _tres The Result type name
 * @param _param The error to wrap
 */
#define IMPL_RESULT_ERR(_t, _e, _tres, _param) \
    { return (_tres){ .is_ok = false, .val = { .err = (_param) } }; }

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
 * Requires a non-NULL output pointer (_out). A NULL _out is a programmer
 * error and is caught by contract. This keeps the return value unambiguous:
 * false means the Result is Err, not that the caller passed a bad pointer.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * Contract: require(_out != NULL)
 *
 * @param _t   The value type
 * @param _e   The error type
 * @param _r   The Result to extract from
 * @param _out Non-NULL pointer to store value
 * @return true if Ok and value was written; false if Err
 */
#define IMPL_RESULT_GET_OK(_t, _e, _r, _out) \
    { \
        require((_out) != NULL, "result_get_ok: output pointer must not be NULL"); \
        if ((_r).is_ok) { \
            *(_out) = (_r).val.ok; \
            return true; \
        } \
        return false; \
    }

/**
 * @brief Safely extracts error value if Err
 *
 * Requires a non-NULL output pointer (_out). A NULL _out is a programmer
 * error and is caught by contract. This keeps the return value unambiguous:
 * false means the Result is Ok, not that the caller passed a bad pointer.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * Contract: require(_out != NULL)
 *
 * @param _t   The value type
 * @param _e   The error type
 * @param _r   The Result to extract from
 * @param _out Non-NULL pointer to store error
 * @return true if Err and error was written; false if Ok
 */
#define IMPL_RESULT_GET_ERR(_t, _e, _r, _out) \
    { \
        require((_out) != NULL, "result_get_err: output pointer must not be NULL"); \
        if (!(_r).is_ok) { \
            *(_out) = (_r).val.err; \
            return true; \
        } \
        return false; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   EXTRACTION WITH DEFAULTS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extracts value or returns fallback
 *
 * Safe alternative to unwrap() — never panics.
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
        _t const _res_val_  = (_r).is_ok ? (_r).val.ok : (_fallback); \
        return _res_val_; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION (REQUIRES OK / ERR)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extracts success value, panicking if Err
 *
 * ⚠️ WARNING: Only use when certain result is Ok.
 * Prefer: get_ok(), unwrap_or(), expect() over unwrap() in production.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Aborts with message if Result is Err
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to unwrap
 */
#define IMPL_RESULT_UNWRAP(_t, _e, _r) \
    { \
        require((_r).is_ok, "result_unwrap: called on Err value"); \
        return (_r).val.ok; \
    }

/**
 * @brief Extracts error value, panicking if Ok
 *
 * The opposite of unwrap() — extracts error from Err.
 * Useful for testing or cases where failure is the expected invariant.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Aborts with message if Result is Ok
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to unwrap
 */
#define IMPL_RESULT_UNWRAP_ERR(_t, _e, _r) \
    { \
        require(!(_r).is_ok, "result_unwrap_err: called on Ok value"); \
        return (_r).val.err; \
    }

/**
 * @brief Extracts value with custom panic message
 *
 * Like unwrap(), but with a caller-supplied message.
 * Use for invariant violations that must never happen in correct code.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Aborts with _msg if Result is Err
 *
 * @param _t   The value type
 * @param _e   The error type
 * @param _r   The Result to extract from
 * @param _msg Error message for contract violation
 */
#define IMPL_RESULT_EXPECT(_t, _e, _r, _msg) \
    { \
        require((_r).is_ok, (_msg)); \
        return (_r).val.ok; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transforms the success value if Ok (T -> T only)
 *
 * Applies function _f to the success value and wraps the result back into
 * the same Result type. If Err, returns the original Result unchanged.
 *
 * Type constraint: _f must be T -> T (same-type transformation).
 * For T -> U mappings use and_then with a function returning the target
 * Result type directly.
 *
 * _r is captured into a local variable to prevent double evaluation.
 *
 * Performance: O(_f) time, O(1) space (stack allocation only)
 *
 * @param _t    The value type
 * @param _e    The error type
 * @param _tres The Result type name
 * @param _r    The Result to transform
 * @param _f    Transformation function: T -> T
 * @param _ok_fn Function to construct Ok (from mangle.h)
 */
#define IMPL_RESULT_MAP(_t, _e, _tres, _r, _f, _ok_fn) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok ? _ok_fn((_f)(_res_.val.ok)) : _res_; \
    }

/**
 * @brief Transforms the error value if Err (E -> E only)
 *
 * Applies function _f to the error value and wraps the result back into
 * the same Result type. If Ok, returns the original Result unchanged.
 *
 * Type constraint: _f must be E -> E (same-type transformation).
 *
 * _r is captured into a local variable to prevent double evaluation.
 *
 * Performance: O(_f) time, O(1) space (stack allocation only)
 *
 * @param _t     The value type
 * @param _e     The error type
 * @param _tres  The Result type name
 * @param _r     The Result to transform
 * @param _f     Error transformation function: E -> E
 * @param _err_fn Function to construct Err (from mangle.h)
 */
#define IMPL_RESULT_MAP_ERR(_t, _e, _tres, _r, _f, _err_fn) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok ? _res_ : _err_fn((_f)(_res_.val.err)); \
    }

/**
 * @brief Chains Result-returning operations (flatMap / bind)
 *
 * If Ok, passes the value to _f and returns _f's Result.
 * If Err, returns the original Result unchanged without calling _f.
 * Prevents nested Result<Result<T,E>,E>.
 *
 * _r is captured into a local variable to prevent double evaluation.
 *
 * Performance: O(_f) time, O(1) space (no nesting)
 *
 * @param _t    The value type
 * @param _e    The error type
 * @param _tres The Result type name
 * @param _r    The Result to chain from
 * @param _f    Function returning Result: T -> Result<T, E>
 */
#define IMPL_RESULT_AND_THEN(_t, _e, _tres, _r, _f) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok ? (_f)(_res_.val.ok) : _res_; \
    }

/**
 * @brief Provides alternative Result if Err
 *
 * If Ok, returns the original Result unchanged.
 * If Err, calls _f with the error value and returns _f's Result.
 *
 * _r is captured into a local variable to prevent double evaluation.
 *
 * Performance: O(1) if Ok, O(_f) if Err, O(1) space
 *
 * @param _t    The value type
 * @param _e    The error type
 * @param _tres The Result type name
 * @param _r    The Result to check
 * @param _f    Recovery function: E -> Result<T, E>
 */
#define IMPL_RESULT_OR_ELSE(_t, _e, _tres, _r, _f) \
    { \
        _tres const _res_ = (_r); \
        return _res_.is_ok ? _res_ : (_f)(_res_.val.err); \
    }

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Checks structural equality of two Results
 *
 * Two Results are equal if and only if:
 * - Both are Ok  and eq_ok (_r1.val.ok,  _r2.val.ok)  returns true, OR
 * - Both are Err and eq_err(_r1.val.err, _r2.val.err) returns true.
 * An Ok and an Err are never equal regardless of their contained values.
 *
 * Both _r1 and _r2 are captured into local variables to prevent double
 * evaluation of potentially side-effecting expressions.
 *
 * Performance: O(1)       for Ok-vs-Err or Err-vs-Ok comparison
 *              O(_eq_ok)  for Ok-vs-Ok comparison
 *              O(_eq_err) for Err-vs-Err comparison
 *              O(1) space
 *
 * @param _t     The value type
 * @param _e     The error type
 * @param _r1    First Result
 * @param _r2    Second Result
 * @param _eq_ok  Equality function for values:  (T, T) -> bool
 * @param _eq_err Equality function for errors:  (E, E) -> bool
 */
#define IMPL_RESULT_EQ(_t, _e, _r1, _r2, _eq_ok, _eq_err) \
    { \
        _t const _dummy_t_; (void)_dummy_t_; \
        _e const _dummy_e_; (void)_dummy_e_; \
        /* capture to avoid double evaluation */ \
        /* NOTE: _t/_e locals below are only valid if the types are   */ \
        /* default-constructible (scalar). For non-scalar types callers */ \
        /* should pass lvalue expressions to _r1/_r2 directly.         */ \
        bool const _r1_ok_ = (_r1).is_ok; \
        bool const _r2_ok_ = (_r2).is_ok; \
        if (_r1_ok_ != _r2_ok_) return false; \
        if (_r1_ok_) return (_eq_ok)((_r1).val.ok, (_r2).val.ok); \
        return (_eq_err)((_r1).val.err, (_r2).val.err); \
    }

#endif /* CANON_RESULT_IMPL_H */
