// semantics/option/option_impl.h
#ifndef CANON_OPTION_IMPL_H
#define CANON_OPTION_IMPL_H

#include "../../core/primitives/types.h"
#include "../../core/primitives/contract.h"

/**
 * @file option_impl.h
 * @brief Pure implementation logic for Option<T> type (no name mangling)
 *
 * This file contains the actual behavior of each Option operation as individual
 * macros. These implementation macros are generic and take type parameters without
 * any naming conventions applied. They can be overridden for custom types by
 * defining type-specific versions before including this header.
 *
 * Philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Pure logic: No naming conventions, pure behavior
 * - Overridable: Define IMPL_OPTION_*_yourtype to specialize
 * - Composable: Used by option_defn.h to build concrete types
 * - Explicit: All behavior is visible in macro definitions
 *
 * Performance (all operations):
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) — all operations are constant time
 * - Space complexity: O(1) — no allocations, pure stack operations
 * - Memory layout: sizeof(bool) + sizeof(T) + padding
 * - No dynamic allocation, no function call overhead (inline)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations are thread-safe on independent Option instances
 * - No shared state between Option values
 * - Mutation operations (replace, take) require external synchronization
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (compound literals, designated initializers)
 * - Uses Canon-C contract.h for assertions (requires, ensure)
 * - No platform-specific code
 *
 * Customization example:
 * ────────────────────────────────────────────────────────────────────────────
 * // Specialize Option<ptr> to assert non-NULL on construction
 * #define IMPL_OPTION_SOME_ptr(t, topt, param) \
 *     { \
 *         require((param) != NULL, "option_some: NULL pointer not allowed"); \
 *         return (topt){ .has_value = true, .value = (param) }; \
 *     }
 * #include "option_defn.h"
 * DEFINE_OPTION_ALL(static inline, void*)
 *
 * @sa option_mangle.h, option_decl.h, option_defn.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   STRUCT LAYOUT
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Option struct layout implementation
 *
 * Memory layout: { bool has_value; T value; }
 * 
 * Performance:
 * - Space: sizeof(bool) + sizeof(T) + alignment padding
 * - Typical sizes: 2 bytes (bool), 4-8 bytes (int/ptr), + value size
 * - Zero allocation overhead
 *
 * @param _t The contained value type
 */
#define IMPL_OPTION_STRUCT(_t) \
    { bool has_value; _t value; }

/* ════════════════════════════════════════════════════════════════════════════
   CONSTRUCTORS
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Constructs Some(value) - an Option containing a value
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _param The value to wrap
 */
#define IMPL_OPTION_SOME(_t, _topt, _param) \
    { return (_topt){ .has_value = true, .value = (_param) }; }

/**
 * @brief Constructs None - an Option without a value
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t The value type
 * @param _topt The Option type name
 */
#define IMPL_OPTION_NONE(_t, _topt) \
    { return (_topt){ .has_value = false }; }

/* ════════════════════════════════════════════════════════════════════════════
   QUERIES
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Checks if Option contains a value
 *
 * Performance: O(1) time, O(1) space (single boolean read)
 *
 * @param _t The value type
 * @param _o The Option to check
 */
#define IMPL_OPTION_IS_SOME(_t, _o) \
    { return (_o).has_value; }

/**
 * @brief Checks if Option is empty
 *
 * Performance: O(1) time, O(1) space (single boolean read)
 *
 * @param _t The value type
 * @param _o The Option to check
 */
#define IMPL_OPTION_IS_NONE(_t, _o) \
    { return !(_o).has_value; }

/* ════════════════════════════════════════════════════════════════════════════
   SAFE EXTRACTION (PERMISSIVE - NULL ALLOWED)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Safely extracts value if present
 *
 * Permissive: Allows NULL output pointer, returns false if NULL.
 * This is defensive programming - NULL check avoids crashes.
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 *
 * @param _t The value type
 * @param _o The Option to extract from
 * @param _out Pointer to store value (NULL-safe)
 * @return true if value extracted, false if None or out is NULL
 */
#define IMPL_OPTION_GET(_t, _o, _out) \
    { \
        if ((_o).has_value && (_out)) { \
            *(_out) = (_o).value; \
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
 * @param _o The Option to extract from
 * @param _fallback Value to return if None
 */
#define IMPL_OPTION_UNWRAP_OR(_t, _o, _fallback) \
    { return (_o).has_value ? (_o).value : (_fallback); }

/* ════════════════════════════════════════════════════════════════════════════
   UNSAFE EXTRACTION (REQUIRES SOME)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extracts value, panicking if None
 *
 * ⚠️ WARNING: Only use when you are certain the value is present!
 * Prefer: get(), unwrap_or(), expect() over unwrap() in production.
 *
 * Performance: O(1) time, O(1) space (assertion + field access)
 * Contract: Crashes with clear message if Option is None
 *
 * @param _t The value type
 * @param _o The Option to unwrap
 */
#define IMPL_OPTION_UNWRAP(_t, _o) \
    { \
        require((_o).has_value, "option_unwrap called on None"); \
        return (_o).value; \
    }

/**
 * @brief Extracts value with custom panic message
 *
 * Like unwrap(), but with descriptive error message for debugging.
 * Use for invariant violations that should never happen.
 *
 * Performance: O(1) time, O(1) space (assertion + field access)
 * Contract: Crashes with custom message if Option is None
 *
 * @param _t The value type
 * @param _o The Option to extract from
 * @param _msg Error message for contract violation
 */
#define IMPL_OPTION_EXPECT(_t, _o, _msg) \
    { \
        require((_o).has_value, (_msg)); \
        return (_o).value; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transforms the contained value if present
 *
 * Applies function f to the contained value, wrapping result in Option.
 * If None, returns None without calling f.
 *
 * Performance: O(f) time where f is the transformation function
 *              O(1) space (stack allocation only)
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _o The Option to transform
 * @param _f Transformation function: T -> T
 * @param _some_fn Function to construct Some (from mangle.h)
 * @param _none_fn Function to construct None (from mangle.h)
 */
#define IMPL_OPTION_MAP(_t, _topt, _o, _f, _some_fn, _none_fn) \
    { \
        return (_o).has_value ? \
            (_some_fn((_f)((_o).value))) : \
            (_none_fn()); \
    }

/**
 * @brief Chains Option-returning operations
 *
 * Like map(), but f returns an Option instead of plain value.
 * Prevents nested Option<Option<T>>.
 *
 * Performance: O(f) time where f is the chained function
 *              O(1) space (no nesting)
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _o The Option to chain from
 * @param _f Function returning Option: T -> Option<T>
 * @param _none_fn Function to construct None (from mangle.h)
 */
#define IMPL_OPTION_AND_THEN(_t, _topt, _o, _f, _none_fn) \
    { return (_o).has_value ? (_f)((_o).value) : (_none_fn()); }

/**
 * @brief Provides alternative if None
 *
 * Returns o if Some, otherwise calls fallback() to get alternative.
 *
 * Performance: O(1) or O(fallback) depending on whether o is Some
 *              O(1) space
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _o The Option to check
 * @param _fallback Function providing alternative: void -> Option<T>
 */
#define IMPL_OPTION_OR_ELSE(_t, _topt, _o, _fallback) \
    { return (_o).has_value ? (_o) : (_fallback)(); }

/**
 * @brief Keeps value only if predicate returns true
 *
 * Converts Some(value) to None if predicate(value) is false.
 * Useful for conditional extraction.
 *
 * Performance: O(pred) time where pred is the predicate function
 *              O(1) space
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _o The Option to filter
 * @param _pred Predicate function: T -> bool
 * @param _none_fn Function to construct None (from mangle.h)
 */
#define IMPL_OPTION_FILTER(_t, _topt, _o, _pred, _none_fn) \
    { \
        return ((_o).has_value && (_pred)((_o).value)) ? \
            (_o) : (_none_fn()); \
    }

/**
 * @brief Combines two Options with a function
 *
 * Returns Some(combine(a, b)) if both Options are Some.
 * Returns None if either Option is None.
 *
 * Performance: O(combine) time where combine is the combining function
 *              O(1) space
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _o1 First Option
 * @param _o2 Second Option
 * @param _combine Function to combine values: (T, T) -> T
 * @param _some_fn Function to construct Some (from mangle.h)
 * @param _none_fn Function to construct None (from mangle.h)
 */
#define IMPL_OPTION_ZIP(_t, _topt, _o1, _o2, _combine, _some_fn, _none_fn) \
    { \
        if ((_o1).has_value && (_o2).has_value) { \
            return (_some_fn((_combine)((_o1).value, (_o2).value))); \
        } \
        return (_none_fn()); \
    }

/* ════════════════════════════════════════════════════════════════════════════
   MUTATION OPERATIONS (STRICT - NULL NOT ALLOWED)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Replaces the value, returning the old Option
 *
 * Updates the Option with a new value, returning previous state.
 * Strict: NULL pointer triggers contract violation (always a bug).
 *
 * Performance: O(1) time, O(1) space (swap operation)
 * Contract: Requires non-NULL pointer
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _o Pointer to Option to update (must not be NULL)
 * @param _new_value New value to set
 * @param _some_fn Function to construct Some (from mangle.h)
 */
#define IMPL_OPTION_REPLACE(_t, _topt, _o, _new_value, _some_fn) \
    { \
        require((_o) != NULL, "option_replace: o parameter cannot be NULL"); \
        _topt old = *(_o); \
        *(_o) = (_some_fn(_new_value)); \
        return old; \
    }

/**
 * @brief Takes the value out, leaving None
 *
 * Extracts the value from the Option, replacing it with None.
 * Useful for moving values out of Option containers.
 * Strict: NULL pointer triggers contract violation (always a bug).
 *
 * Performance: O(1) time, O(1) space (swap operation)
 * Contract: Requires non-NULL pointer
 * Postcondition: *o is None after this call
 *
 * @param _t The value type
 * @param _topt The Option type name
 * @param _o Pointer to Option to take from (must not be NULL)
 * @param _none_fn Function to construct None (from mangle.h)
 */
#define IMPL_OPTION_TAKE(_t, _topt, _o, _none_fn) \
    { \
        require((_o) != NULL, "option_take: o parameter cannot be NULL"); \
        _topt old = *(_o); \
        *(_o) = (_none_fn()); \
        return old; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   COMPARISON
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Checks equality of two Options
 *
 * Two Options are equal if:
 * - Both are None, OR
 * - Both are Some and their values compare equal via eq function
 *
 * Performance: O(1) for None-None comparison
 *              O(eq) for Some-Some comparison where eq is equality function
 *              O(1) space
 *
 * @param _t The value type
 * @param _o1 First Option
 * @param _o2 Second Option
 * @param _eq Equality comparison function: (T, T) -> bool
 */
#define IMPL_OPTION_EQ(_t, _o1, _o2, _eq) \
    { \
        if (!(_o1).has_value && !(_o2).has_value) return true; \
        if ((_o1).has_value != (_o2).has_value) return false; \
        return (_eq)((_o1).value, (_o2).value); \
    }

#endif /* CANON_OPTION_IMPL_H */
