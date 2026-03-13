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
 * - The underlying contract handler (canon_contract_handler) is shared global
 *   state. contract_set_handler() is NOT thread-safe — call once during
 *   program initialization, before spawning threads. See contract.h.
 *
 * Contract enforcement:
 * ────────────────────────────────────────────────────────────────────────────
 * - Precondition checks use require_msg() — always-on by default
 * - When CANON_NO_REQUIRE is defined, all require_msg() calls become no-ops.
 *   This removes NULL-pointer and invariant guards from unwrap(), expect(),
 *   replace(), and take(). Only use CANON_NO_REQUIRE when formal verification
 *   (Frama-C) has proved all call sites safe. See contract.h.
 * - expect() passes its message string directly to the contract handler via
 *   _CANON_INVOKE_HANDLER. This is the only macro that uses the internal
 *   helper directly, because require_msg() stringifies its message at
 *   compile time and cannot accept a runtime-variable string.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (compound literals, designated initializers)
 * - bool / true / false provided by types.h → <stdbool.h>
 * - Uses Canon-C contract.h for assertions (require_msg, _CANON_INVOKE_HANDLER)
 * - No platform-specific code
 *
 * Customization example:
 * ────────────────────────────────────────────────────────────────────────────
 * // Specialize Option<ptr> to assert non-NULL on construction
 * #define IMPL_OPTION_SOME_ptr(t, topt, param) \
 *     { \
 *         require_msg((param) != NULL, "option_some: NULL pointer not allowed"); \
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
 * @brief Constructs Some(value) — an Option containing a value
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t     The value type
 * @param _topt  The Option type name
 * @param _param The value to wrap
 */
#define IMPL_OPTION_SOME(_t, _topt, _param) \
    { return (_topt){ .has_value = true, .value = (_param) }; }

/**
 * @brief Constructs None — an Option without a value
 *
 * The .value field is zero-initialized ((_t){0}) to avoid indeterminate
 * memory, which would be flagged by Frama-C WP, ASan, and Valgrind even
 * when correctly guarded by has_value. Zero-initialization costs nothing
 * on all major targets.
 *
 * Performance: O(1) time, O(1) space (stack allocation only)
 *
 * @param _t    The value type
 * @param _topt The Option type name
 */
#define IMPL_OPTION_NONE(_t, _topt) \
    { return (_topt){ .has_value = false, .value = (_t){0} }; }

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
   SAFE EXTRACTION (STRICT — NULL NOT ALLOWED)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Safely extracts value if present
 *
 * Strict: a NULL output pointer is always a caller bug and triggers a
 * contract violation. Use unwrap_or() if you want a default instead of
 * an output parameter, or check is_some() before calling get().
 *
 * Performance: O(1) time, O(1) space (conditional assignment)
 * Contract:    Requires non-NULL out pointer
 *
 * @param _t   The value type
 * @param _o   The Option to extract from
 * @param _out Pointer to store value (must not be NULL)
 * @return true if Some and value was written, false if None
 */
#define IMPL_OPTION_GET(_t, _o, _out) \
    { \
        require_msg((_out) != NULL, "option_get: out parameter cannot be NULL"); \
        if ((_o).has_value) { \
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
 * Safe alternative to unwrap() — never panics.
 *
 * Performance: O(1) time, O(1) space (conditional return)
 *
 * @param _t        The value type
 * @param _o        The Option to extract from
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
 * ⚠️  WARNING: Only use when you are certain the value is present.
 * Prefer: get(), unwrap_or(), expect() over unwrap() in production.
 *
 * Performance: O(1) time, O(1) space (assertion + field access)
 * Contract:    Panics with clear message if Option is None
 *
 * @param _t The value type
 * @param _o The Option to unwrap
 */
#define IMPL_OPTION_UNWRAP(_t, _o) \
    { \
        require_msg((_o).has_value, "option_unwrap called on None"); \
        return (_o).value; \
    }

/**
 * @brief Extracts value with custom panic message
 *
 * Like unwrap(), but with a caller-supplied message for diagnostics.
 * Use for invariant violations that should never happen in correct code.
 *
 * Implementation note:
 *   This macro calls _CANON_INVOKE_HANDLER directly rather than
 *   require_msg(). require_msg() stringifies its message argument at
 *   compile time via the # operator, which discards any runtime variable
 *   passed as _msg. _CANON_INVOKE_HANDLER accepts the string value at
 *   runtime, which is the correct behavior for expect(). This is the
 *   only macro in this file that uses the internal helper; all other
 *   precondition checks use require_msg() as the public API.
 *
 *   When CANON_NO_REQUIRE is defined, require_msg() becomes a no-op but
 *   _CANON_INVOKE_HANDLER is not suppressed — expect() therefore always
 *   fires when the Option is None, regardless of CANON_NO_REQUIRE.
 *   This is intentional: a caller-supplied panic message signals a
 *   deliberate invariant assertion, not an optional precondition check.
 *
 * Performance: O(1) time, O(1) space (assertion + field access)
 * Contract:    Panics with _msg if Option is None
 *
 * @param _t   The value type
 * @param _o   The Option to extract from
 * @param _msg Error message string for the contract violation (char*)
 */
#define IMPL_OPTION_EXPECT(_t, _o, _msg) \
    { \
        if (!(_o).has_value) { \
            _CANON_INVOKE_HANDLER("option_expect called on None", (_msg)); \
        } \
        return (_o).value; \
    }

/* ════════════════════════════════════════════════════════════════════════════
   TRANSFORMATIONS (COMBINATORS)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transforms the contained value if present
 *
 * Applies function f to the contained value, wrapping result in Some.
 * If None, returns None without calling f.
 *
 * Calling convention for _some_fn / _none_fn:
 *   These parameters must be passed as bare function names (or function
 *   pointer variables) that match the signatures generated by option_mangle.h:
 *     _some_fn(_t value)  → _topt
 *     _none_fn(void)      → _topt
 *   They are called directly as _some_fn(...) and _none_fn() within the
 *   macro body. Passing a macro instead of a function name is not supported.
 *
 * Performance: O(f) time, O(1) space (stack allocation only)
 *
 * @param _t       The value type
 * @param _topt    The Option type name
 * @param _o       The Option to transform
 * @param _f       Transformation function: _t → _t
 * @param _some_fn Function to construct Some (from option_mangle.h)
 * @param _none_fn Function to construct None (from option_mangle.h)
 */
#define IMPL_OPTION_MAP(_t, _topt, _o, _f, _some_fn, _none_fn) \
    { \
        return (_o).has_value \
            ? _some_fn((_f)((_o).value)) \
            : _none_fn(); \
    }

/**
 * @brief Chains Option-returning operations
 *
 * Like map(), but _f returns an Option instead of a plain value.
 * Prevents nested Option<Option<T>>.
 *
 * Calling convention for _none_fn: see IMPL_OPTION_MAP.
 *
 * Performance: O(f) time, O(1) space (no nesting)
 *
 * @param _t       The value type
 * @param _topt    The Option type name
 * @param _o       The Option to chain from
 * @param _f       Function returning Option: _t → _topt
 * @param _none_fn Function to construct None (from option_mangle.h)
 */
#define IMPL_OPTION_AND_THEN(_t, _topt, _o, _f, _none_fn) \
    { return (_o).has_value ? (_f)((_o).value) : _none_fn(); }

/**
 * @brief Provides alternative if None
 *
 * Returns _o if Some, otherwise calls _fallback() to get alternative.
 * _fallback must be a zero-argument function returning _topt.
 *
 * Performance: O(1) or O(fallback) depending on whether _o is Some
 *              O(1) space
 *
 * @param _t       The value type
 * @param _topt    The Option type name
 * @param _o       The Option to check
 * @param _fallback Zero-argument function providing alternative: void → _topt
 */
#define IMPL_OPTION_OR_ELSE(_t, _topt, _o, _fallback) \
    { return (_o).has_value ? (_o) : (_fallback)(); }

/**
 * @brief Keeps value only if predicate returns true
 *
 * Converts Some(value) to None if _pred(value) is false.
 * Useful for conditional extraction without separate is_some() + get().
 *
 * Calling convention for _none_fn: see IMPL_OPTION_MAP.
 *
 * Performance: O(pred) time, O(1) space
 *
 * @param _t       The value type
 * @param _topt    The Option type name
 * @param _o       The Option to filter
 * @param _pred    Predicate function: _t → bool
 * @param _none_fn Function to construct None (from option_mangle.h)
 */
#define IMPL_OPTION_FILTER(_t, _topt, _o, _pred, _none_fn) \
    { \
        return ((_o).has_value && (_pred)((_o).value)) \
            ? (_o) \
            : _none_fn(); \
    }

/**
 * @brief Combines two Options of the same type with a function
 *
 * Returns Some(_combine(a, b)) if both Options are Some.
 * Returns None if either Option is None.
 *
 * Both operands and the result share the same type T.  For combining two
 * different Option types into a third, write a bespoke combinator.
 *
 * Naming rationale:
 *   combine_with() accurately describes what this function does: it combines
 *   two Option<T> values using a provided function, producing another Option<T>.
 *   The name zip() was not used because zip() in functional programming
 *   combines Option<A> and Option<B> into Option<(A,B)>, requiring two
 *   distinct types — a different operation to what is implemented here.
 *
 * Calling convention for _some_fn / _none_fn: see IMPL_OPTION_MAP.
 *
 * Performance: O(combine) time, O(1) space
 *
 * @param _t       The value type
 * @param _topt    The Option type name
 * @param _o1      First Option
 * @param _o2      Second Option
 * @param _combine Function to combine values: (_t, _t) → _t
 * @param _some_fn Function to construct Some (from option_mangle.h)
 * @param _none_fn Function to construct None (from option_mangle.h)
 */
#define IMPL_OPTION_COMBINE_WITH(_t, _topt, _o1, _o2, _combine, _some_fn, _none_fn) \
    { \
        if ((_o1).has_value && (_o2).has_value) { \
            return _some_fn((_combine)((_o1).value, (_o2).value)); \
        } \
        return _none_fn(); \
    }

/* ════════════════════════════════════════════════════════════════════════════
   MUTATION OPERATIONS (STRICT — NULL NOT ALLOWED)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Replaces the value, returning the old Option
 *
 * Updates *_o with Some(_new_value), returning the previous Option state.
 * Strict: a NULL pointer is always a caller bug.
 *
 * Calling convention for _some_fn: see IMPL_OPTION_MAP.
 *
 * Performance: O(1) time, O(1) space (swap operation)
 * Contract:    Requires non-NULL _o pointer
 *
 * @param _t        The value type
 * @param _topt     The Option type name
 * @param _o        Pointer to Option to update (must not be NULL)
 * @param _new_value New value to set
 * @param _some_fn  Function to construct Some (from option_mangle.h)
 */
#define IMPL_OPTION_REPLACE(_t, _topt, _o, _new_value, _some_fn) \
    { \
        require_msg((_o) != NULL, "option_replace: o parameter cannot be NULL"); \
        _topt old = *(_o); \
        *(_o) = _some_fn(_new_value); \
        return old; \
    }

/**
 * @brief Takes the value out, leaving None
 *
 * Extracts the Option from *_o, replacing it with None.
 * Useful for moving values out of Option containers.
 * Strict: a NULL pointer is always a caller bug.
 *
 * Calling convention for _none_fn: see IMPL_OPTION_MAP.
 *
 * Performance: O(1) time, O(1) space (swap operation)
 * Contract:    Requires non-NULL _o pointer
 * Postcondition: *_o is None after this call
 *
 * @param _t      The value type
 * @param _topt   The Option type name
 * @param _o      Pointer to Option to take from (must not be NULL)
 * @param _none_fn Function to construct None (from option_mangle.h)
 */
#define IMPL_OPTION_TAKE(_t, _topt, _o, _none_fn) \
    { \
        require_msg((_o) != NULL, "option_take: o parameter cannot be NULL"); \
        _topt old = *(_o); \
        *(_o) = _none_fn(); \
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
 * - Both are Some and their values compare equal via _eq
 *
 * Performance: O(1) for None-None comparison
 *              O(eq) for Some-Some comparison
 *              O(1) space
 *
 * @param _t  The value type
 * @param _o1 First Option
 * @param _o2 Second Option
 * @param _eq Equality comparison function: (_t, _t) → bool
 */
#define IMPL_OPTION_EQ(_t, _o1, _o2, _eq) \
    { \
        if (!(_o1).has_value && !(_o2).has_value) return true; \
        if ((_o1).has_value != (_o2).has_value)   return false; \
        return (_eq)((_o1).value, (_o2).value); \
    }

#endif /* CANON_OPTION_IMPL_H */
