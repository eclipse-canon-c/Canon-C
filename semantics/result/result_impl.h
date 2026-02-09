// semantics/result/result_impl.h
#ifndef CANON_RESULT_IMPL_H
#define CANON_RESULT_IMPL_H

#include <stdbool.h>
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
 * Performance (all operations):
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) — all operations are constant time
 * - Space complexity: O(1) — no allocations, pure stack operations
 * - Memory layout: sizeof(bool) + max(sizeof(T), sizeof(E)) + padding (union saves space)
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
 * - Requires C99 or later (compound literals, designated initializers, unions)
 * - Uses Canon-C contract.h for assertions (require, ensure)
 * - No platform-specific code
 *
 * Customization example:
 * ────────────────────────────────────────────────────────────────────────────
 * // Specialize Result<ptr, error> to assert non-NULL on Ok
 * #define IMPL_RESULT_OK_ptr_error(t, e, tres, param) \
 *     { \
 *         require((param) != NULL, "result_ok: NULL pointer not allowed"); \
 *         return (tres){ .is_ok = true, .ok = (param) }; \
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
 * Memory layout: { bool is_ok; union { T ok; E err; }; }
 * 
 * Performance:
 * - Space: sizeof(bool) + max(sizeof(T), sizeof(E)) + alignment padding
 * - Union saves memory compared to storing both T and E separately
 * - Only one of ok/err is valid at any time (determined by is_ok)
 * - Accessing wrong union member is undefined behavior
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
        }; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTORS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Constructs Ok(value) - a successful result
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _tres The Result type name
 * @param _param The value to wrap
 */
#define IMPL_RESULT_OK(_t, _e, _tres, _param) \
    { return (_tres){ .is_ok = true, .ok = (_param) }; }

/**
 * @brief Constructs Err(error) - a failed result
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _tres The Result type name
 * @param _param The error to wrap
 */
#define IMPL_RESULT_ERR(_t, _e, _tres, _param) \
    { return (_tres){ .is_ok = false, .err = (_param) }; }

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
   SAFE EXTRACTION (PERMISSIVE - NULL ALLOWED)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Safely extracts success value if Ok
 *
 * Permissive: Allows NULL output pointer, returns false if NULL.
 * This is defensive programming - NULL check avoids crashes.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to extract from
 * @param _out Pointer to store value (NULL-safe)
 * @return true if value extracted, false if Err or out is NULL
 */
#define IMPL_RESULT_GET_OK(_t, _e, _r, _out) \
    { \
        if ((_r).is_ok && (_out)) { \
            *(_out) = (_r).ok; \
            return true; \
        } \
        return false; \
    }

/**
 * @brief Safely extracts error value if Err
 *
 * Permissive: Allows NULL output pointer, returns false if NULL.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to extract from
 * @param _out Pointer to store error (NULL-safe)
 * @return true if error extracted, false if Ok or out is NULL
 */
#define IMPL_RESULT_GET_ERR(_t, _e, _r, _out) \
    { \
        if (!(_r).is_ok && (_out)) { \
            *(_out) = (_r).err; \
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
 * Safe alternative to unwrap() - never panics.
 *
 * Performance: O(1) time, O(1) space (conditional return)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to extract from
 * @param _fallback Value to return if Err
 */
#define IMPL_RESULT_UNWRAP_OR(_t, _e, _r, _fallback) \
    { return (_r).is_ok ? (_r).ok : (_fallback); }

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION (REQUIRES OK/ERR)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extracts success value, panicking if Err
 *
 * ⚠️ WARNING: Only use when certain result is Ok!
 * Prefer: get_ok(), unwrap_or(), expect() over unwrap() in production.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Crashes with clear message if Result is Err
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to unwrap
 */
#define IMPL_RESULT_UNWRAP(_t, _e, _r) \
    { \
        require((_r).is_ok, "result_unwrap called on Err value"); \
        return (_r).ok; \
    }

/**
 * @brief Extracts error value, panicking if Ok
 *
 * The opposite of unwrap() - extracts error from Err.
 * Useful for testing or cases where you expect failure.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Crashes with clear message if Result is Ok
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to unwrap
 */
#define IMPL_RESULT_UNWRAP_ERR(_t, _e, _r) \
    { \
        require(!(_r).is_ok, "result_unwrap_err called on Ok value"); \
        return (_r).err; \
    }

/**
 * @brief Extracts value with custom panic message
 *
 * Like unwrap(), but with descriptive error message.
 * Use for invariant violations that should never happen.
 *
 * Performance: O(1) time, O(1) space (assertion + union access)
 * Contract: Crashes with custom message if Result is Err
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r The Result to extract from
 * @param _msg Error message for contract violation
 */
#define IMPL_RESULT_EXPECT(_t, _e, _r, _msg) \
    { \
        require((_r).is_ok, (_msg)); \
        return (_r).ok; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transforms the success value if Ok
 *
 * Applies function f to the success value, wrapping result in Result.
 * If Err, returns Err unchanged without calling f.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _tres The Result type name
 * @param _r The Result to transform
 * @param _f Transformation function: T -> T
 * @param _ok_fn Function to construct Ok (from mangle.h)
 */
#define IMPL_RESULT_MAP(_t, _e, _tres, _r, _f, _ok_fn) \
    { return (_r).is_ok ? (_ok_fn((_f)((_r).ok))) : (_r); }

/**
 * @brief Transforms the error value if Err
 *
 * Applies function f to the error value, wrapping result in Result.
 * If Ok, returns Ok unchanged without calling f.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _tres The Result type name
 * @param _r The Result to transform
 * @param _f Error transformation function: E -> E
 * @param _err_fn Function to construct Err (from mangle.h)
 */
#define IMPL_RESULT_MAP_ERR(_t, _e, _tres, _r, _f, _err_fn) \
    { return (_r).is_ok ? (_r) : (_err_fn((_f)((_r).err))); }

/**
 * @brief Chains Result-returning operations
 *
 * Like map(), but f returns a Result instead of plain value.
 * Prevents nested Result<Result<T,E>,E>.
 *
 * Performance: O(f) time where f is the chained function
 *              O(1) space (no nesting)
 *
 * @param _t The value type
 * @param _e The error type
 * @param _tres The Result type name
 * @param _r The Result to chain from
 * @param _f Function returning Result: T -> Result<T, E>
 */
#define IMPL_RESULT_AND_THEN(_t, _e, _tres, _r, _f) \
    { return (_r).is_ok ? (_f)((_r).ok) : (_r); }

/**
 * @brief Provides alternative if Err
 *
 * Returns r if Ok, otherwise calls fallback(error) to get alternative.
 *
 * Performance: O(1) if Ok, O(fallback) if Err
 *              O(1) space
 *
 * @param _t The value type
 * @param _e The error type
 * @param _tres The Result type name
 * @param _r The Result to check
 * @param _f Function providing alternative: E -> Result<T, E>
 */
#define IMPL_RESULT_OR_ELSE(_t, _e, _tres, _r, _f) \
    { return (_r).is_ok ? (_r) : (_f)((_r).err); }

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Checks equality of two Results
 *
 * Two Results are equal if:
 * - Both are Ok and their values compare equal via eq_ok, OR
 * - Both are Err and their errors compare equal via eq_err
 *
 * Performance: O(1) for Ok-Err or Err-Ok comparison
 *              O(eq_ok) for Ok-Ok comparison
 *              O(eq_err) for Err-Err comparison
 *              O(1) space
 *
 * @param _t The value type
 * @param _e The error type
 * @param _r1 First Result
 * @param _r2 Second Result
 * @param _eq_ok Equality comparison function for values: (T, T) -> bool
 * @param _eq_err Equality comparison function for errors: (E, E) -> bool
 */
#define IMPL_RESULT_EQ(_t, _e, _r1, _r2, _eq_ok, _eq_err) \
    { \
        if ((_r1).is_ok != (_r2).is_ok) return false; \
        if ((_r1).is_ok) return (_eq_ok)((_r1).ok, (_r2).ok); \
        return (_eq_err)((_r1).err, (_r2).err); \
    }

#endif /* CANON_RESULT_IMPL_H */
