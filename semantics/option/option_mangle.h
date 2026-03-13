/* semantics/option/option_mangle.h
 * ============================================================================
 * Name mangling conventions for Option<T>
 * ============================================================================
 *
 * Single source of truth for every identifier the Option<T> machinery emits.
 * All other Option headers consume these macros; none hard-code names.
 *
 * Philosophy (Canon-C)
 * ----------------------------------------------------------------------------
 *   - Everything is explicit.  No name is generated without a visible macro.
 *   - Everything is optional.  Every macro is guarded with #ifndef so the
 *     caller may override any — or all — names before inclusion.
 *   - No hidden behaviour.  If a name cannot be found by reading this file,
 *     it does not exist.
 *   - Abstractions clarify, not conceal.
 *
 * Default naming scheme
 * ----------------------------------------------------------------------------
 *   Given a type token T the defaults produce:
 *
 *     Type alias  : option_T          (e.g. option_int)
 *     Struct tag  : option_T_s        (e.g. option_int_s)
 *     Functions   : option_T_<verb>   (e.g. option_int_some,
 *                                           option_int_unwrap_or,
 *                                           option_int_combine_with)
 *
 *   The type token T must be a single preprocessor token that is also a
 *   valid C identifier fragment.  Avoid tokens that begin with an underscore
 *   or that contain two consecutive underscores; the resulting identifiers
 *   would be reserved by the C standard (C99 §7.1.3).
 *
 * Customisation
 * ----------------------------------------------------------------------------
 *   Define any subset of MANGLE_OPTION_* before including this header.
 *   Undefined macros receive their defaults; defined macros are left alone.
 *
 *   Example — Haskell-style Maybe/Just/Nothing naming:
 *
 *     #define MANGLE_OPTION_TYPE(T)      Maybe##T
 *     #define MANGLE_OPTION_STRUCT_TAG(T) Maybe##T##_s
 *     #define MANGLE_OPTION_SOME(T)      Maybe##T##_Just
 *     #define MANGLE_OPTION_NONE(T)      Maybe##T##_Nothing
 *     #define MANGLE_OPTION_IS_SOME(T)   Maybe##T##_isJust
 *     #define MANGLE_OPTION_IS_NONE(T)   Maybe##T##_isNothing
 *     #include "option.h"
 *
 *     CANON_OPTION(Int)               /* note: capital I, not 'int' */
 *
 *     MaybeInt x = MaybeInt_Just(42);
 *     bool present = MaybeInt_isJust(x);
 *
 *   IMPORTANT: The token you pass to CANON_OPTION must be the same token you
 *   embed in your MANGLE_* overrides.  If you write Maybe##Int then you must
 *   write CANON_OPTION(Int), not CANON_OPTION(int).
 *
 *   Example — project-scoped prefix to avoid link-time collisions:
 *
 *     #define MANGLE_OPTION_TYPE(T)  myproject_opt_##T
 *     #include "option.h"
 *
 *     CANON_OPTION(int)
 *
 *     myproject_opt_int x = myproject_opt_int_some(42);
 *
 * Token constraints
 * ----------------------------------------------------------------------------
 *   The type token T (argument to CANON_OPTION and all MANGLE_* macros) must:
 *     - Be a single preprocessor token.
 *     - Form a valid C identifier when pasted after "option_" (default) or
 *       after whatever prefix your override uses.
 *     - NOT begin with an underscore.
 *     - NOT contain two consecutive underscores.
 *
 *   Violating these constraints produces reserved identifiers (C99 §7.1.3)
 *   or ill-formed token pastes; behaviour is undefined.
 *
 * Performance
 * ----------------------------------------------------------------------------
 *   Time  : O(1) — pure compile-time token pasting.
 *   Space : O(1) — no runtime storage; all names resolved by the preprocessor.
 *
 * Thread safety
 * ----------------------------------------------------------------------------
 *   N/A — this file contains no runtime behaviour whatsoever.
 *
 * Portability
 * ----------------------------------------------------------------------------
 *   Requires C99 or later.  Token pasting (##) is defined in C99 §6.10.3.3.
 *   No compiler extensions are used or required.
 *   Tested with GCC, Clang, and MSVC in C99 mode.
 *
 * Consumed by
 * ----------------------------------------------------------------------------
 *   option_decl.h  — struct layout and function declarations
 *   option_defn.h  — inline / static function definitions
 *   option_impl.h  — non-inline implementation stubs (optional)
 *
 * ============================================================================
 * SPDX-License-Identifier: MPL-2.0
 * ============================================================================
 */

#ifndef CANON_OPTION_MANGLE_H
#define CANON_OPTION_MANGLE_H

/* ============================================================================
   TYPE NAMES
   Consumed by: option_decl.h (typedef, struct tag)
   ============================================================================ */

/**
 * MANGLE_OPTION_TYPE(T)
 *
 * The public typedef name for Option<T>.
 *
 * Default : option_T   (e.g. option_int, option_float, option_myptr)
 *
 * Override example:
 *   #define MANGLE_OPTION_TYPE(T)  Maybe##T
 *   → MaybeInt, MaybeFloat, …
 */
#ifndef MANGLE_OPTION_TYPE
#  define MANGLE_OPTION_TYPE(T)            option_##T
#endif

/**
 * MANGLE_OPTION_STRUCT_TAG(T)
 *
 * The struct tag (struct <tag>) used inside the typedef.  Required when a
 * forward declaration of the struct is needed in a separate translation unit.
 *
 * Default : option_T_s   (e.g. option_int_s)
 *
 * Consumed by: option_decl.h — the struct definition reads:
 *   typedef struct MANGLE_OPTION_STRUCT_TAG(T) { … } MANGLE_OPTION_TYPE(T);
 */
#ifndef MANGLE_OPTION_STRUCT_TAG
#  define MANGLE_OPTION_STRUCT_TAG(T)      option_##T##_s
#endif

/* ============================================================================
   CONSTRUCTOR FUNCTION NAMES
   Consumed by: option_decl.h (declarations), option_defn.h (definitions)
   ============================================================================ */

/**
 * MANGLE_OPTION_SOME(T)
 *
 * Constructor that wraps a value: Some(value).
 *
 * Default : option_T_some   (e.g. option_int_some(42))
 *
 * Signature (generated by option_defn.h):
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_SOME(T)(T value);
 */
#ifndef MANGLE_OPTION_SOME
#  define MANGLE_OPTION_SOME(T)            option_##T##_some
#endif

/**
 * MANGLE_OPTION_NONE(T)
 *
 * Constructor that represents the absent value: None.
 *
 * Default : option_T_none   (e.g. option_int_none())
 *
 * Signature:
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_NONE(T)(void);
 */
#ifndef MANGLE_OPTION_NONE
#  define MANGLE_OPTION_NONE(T)            option_##T##_none
#endif

/* ============================================================================
   QUERY FUNCTION NAMES
   Consumed by: option_decl.h, option_defn.h
   ============================================================================ */

/**
 * MANGLE_OPTION_IS_SOME(T)
 *
 * Returns non-zero when the option holds a value.
 *
 * Default : option_T_is_some   (e.g. option_int_is_some(opt))
 *
 * Signature:
 *   static inline int
 *   MANGLE_OPTION_IS_SOME(T)(MANGLE_OPTION_TYPE(T) opt);
 */
#ifndef MANGLE_OPTION_IS_SOME
#  define MANGLE_OPTION_IS_SOME(T)         option_##T##_is_some
#endif

/**
 * MANGLE_OPTION_IS_NONE(T)
 *
 * Returns non-zero when the option is absent.
 *
 * Default : option_T_is_none   (e.g. option_int_is_none(opt))
 *
 * Signature:
 *   static inline int
 *   MANGLE_OPTION_IS_NONE(T)(MANGLE_OPTION_TYPE(T) opt);
 */
#ifndef MANGLE_OPTION_IS_NONE
#  define MANGLE_OPTION_IS_NONE(T)         option_##T##_is_none
#endif

/* ============================================================================
   SAFE EXTRACTION FUNCTION NAMES
   Consumed by: option_decl.h, option_defn.h
   ============================================================================ */

/**
 * MANGLE_OPTION_GET(T)
 *
 * Writes the contained value into *out and returns non-zero on success.
 * Returns zero without modifying *out when the option is None.
 *
 * Contract: out must not be NULL — passing NULL is always a caller bug
 * and triggers a contract violation.
 *
 * Default : option_T_get   (e.g. option_int_get(opt, &out))
 *
 * Signature:
 *   static inline int
 *   MANGLE_OPTION_GET(T)(MANGLE_OPTION_TYPE(T) opt, T *out);
 */
#ifndef MANGLE_OPTION_GET
#  define MANGLE_OPTION_GET(T)             option_##T##_get
#endif

/**
 * MANGLE_OPTION_UNWRAP_OR(T)
 *
 * Returns the contained value, or fallback when the option is None.
 *
 * Default : option_T_unwrap_or   (e.g. option_int_unwrap_or(opt, 0))
 *
 * Signature:
 *   static inline T
 *   MANGLE_OPTION_UNWRAP_OR(T)(MANGLE_OPTION_TYPE(T) opt, T fallback);
 */
#ifndef MANGLE_OPTION_UNWRAP_OR
#  define MANGLE_OPTION_UNWRAP_OR(T)       option_##T##_unwrap_or
#endif

/* ============================================================================
   UNSAFE EXTRACTION FUNCTION NAMES
   Consumed by: option_decl.h, option_defn.h
   These functions abort / invoke a panic handler on None.
   ============================================================================ */

/**
 * MANGLE_OPTION_UNWRAP(T)
 *
 * Returns the contained value.  Panics (implementation-defined) on None.
 * Prefer MANGLE_OPTION_GET or MANGLE_OPTION_UNWRAP_OR in production code.
 *
 * Default : option_T_unwrap   (e.g. option_int_unwrap(opt))
 *
 * Signature:
 *   static inline T
 *   MANGLE_OPTION_UNWRAP(T)(MANGLE_OPTION_TYPE(T) opt);
 */
#ifndef MANGLE_OPTION_UNWRAP
#  define MANGLE_OPTION_UNWRAP(T)          option_##T##_unwrap
#endif

/**
 * MANGLE_OPTION_EXPECT(T)
 *
 * Returns the contained value.  Panics with msg on None.
 *
 * Default : option_T_expect   (e.g. option_int_expect(opt, "non-null"))
 *
 * Signature:
 *   static inline T
 *   MANGLE_OPTION_EXPECT(T)(MANGLE_OPTION_TYPE(T) opt, const char *msg);
 */
#ifndef MANGLE_OPTION_EXPECT
#  define MANGLE_OPTION_EXPECT(T)          option_##T##_expect
#endif

/* ============================================================================
   TRANSFORMATION FUNCTION NAMES (COMBINATORS)
   Consumed by: option_decl.h, option_defn.h
   ============================================================================ */

/**
 * MANGLE_OPTION_MAP(T)
 *
 * Applies f to the contained value and returns Some(f(value)), or None.
 * The return type is the same Option<T>; for cross-type mapping use a
 * manual combination of unwrap_or / some / none.
 *
 * Default : option_T_map   (e.g. option_int_map(opt, fn))
 *
 * Signature:
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_MAP(T)(MANGLE_OPTION_TYPE(T) opt, T (*f)(T));
 */
#ifndef MANGLE_OPTION_MAP
#  define MANGLE_OPTION_MAP(T)             option_##T##_map
#endif

/**
 * MANGLE_OPTION_AND_THEN(T)
 *
 * Monadic bind: applies f (which itself returns Option<T>) to the contained
 * value.  Returns None when the option is None.
 *
 * Default : option_T_and_then   (e.g. option_int_and_then(opt, fn))
 *
 * Signature:
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_AND_THEN(T)(MANGLE_OPTION_TYPE(T) opt,
 *                              MANGLE_OPTION_TYPE(T) (*f)(T));
 */
#ifndef MANGLE_OPTION_AND_THEN
#  define MANGLE_OPTION_AND_THEN(T)        option_##T##_and_then
#endif

/**
 * MANGLE_OPTION_OR_ELSE(T)
 *
 * Returns the option unchanged when Some; calls f() and returns its result
 * when None.
 *
 * Default : option_T_or_else   (e.g. option_int_or_else(opt, fn))
 *
 * Signature:
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_OR_ELSE(T)(MANGLE_OPTION_TYPE(T) opt,
 *                             MANGLE_OPTION_TYPE(T) (*f)(void));
 */
#ifndef MANGLE_OPTION_OR_ELSE
#  define MANGLE_OPTION_OR_ELSE(T)         option_##T##_or_else
#endif

/**
 * MANGLE_OPTION_FILTER(T)
 *
 * Returns Some(value) when pred(value) is non-zero, otherwise None.
 *
 * Default : option_T_filter   (e.g. option_int_filter(opt, pred))
 *
 * Signature:
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_FILTER(T)(MANGLE_OPTION_TYPE(T) opt, int (*pred)(T));
 */
#ifndef MANGLE_OPTION_FILTER
#  define MANGLE_OPTION_FILTER(T)          option_##T##_filter
#endif

/**
 * MANGLE_OPTION_COMBINE_WITH(T)
 *
 * Combines two Option<T> values using a caller-supplied (T, T) -> T function.
 * Returns Some(combine(a, b)) if both inputs are Some; None otherwise.
 * combine is not called unless both Options are Some.
 *
 * Both operands and the result share the same type T.  For combining two
 * different Option types into a third, write a bespoke combinator.
 *
 * Default : option_T_combine_with
 *   (e.g. option_int_combine_with(opt_a, opt_b, add))
 *
 * Signature (generated by option_defn.h):
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_COMBINE_WITH(T)(MANGLE_OPTION_TYPE(T) o1,
 *                                  MANGLE_OPTION_TYPE(T) o2,
 *                                  T (*combine)(T, T));
 */
#ifndef MANGLE_OPTION_COMBINE_WITH
#  define MANGLE_OPTION_COMBINE_WITH(T)    option_##T##_combine_with
#endif

/* ============================================================================
   MUTATION FUNCTION NAMES
   Consumed by: option_decl.h, option_defn.h
   ============================================================================ */

/**
 * MANGLE_OPTION_REPLACE(T)
 *
 * Stores new_val into *opt (making it Some), and returns the previous option.
 * The caller owns the returned value.
 *
 * Default : option_T_replace   (e.g. option_int_replace(&opt, 7))
 *
 * Signature:
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_REPLACE(T)(MANGLE_OPTION_TYPE(T) *opt, T new_val);
 */
#ifndef MANGLE_OPTION_REPLACE
#  define MANGLE_OPTION_REPLACE(T)         option_##T##_replace
#endif

/**
 * MANGLE_OPTION_TAKE(T)
 *
 * Moves the value out of *opt, leaving it as None, and returns the previous
 * option.  Equivalent to replace with None but without needing a sentinel.
 *
 * Default : option_T_take   (e.g. option_int_take(&opt))
 *
 * Signature:
 *   static inline MANGLE_OPTION_TYPE(T)
 *   MANGLE_OPTION_TAKE(T)(MANGLE_OPTION_TYPE(T) *opt);
 */
#ifndef MANGLE_OPTION_TAKE
#  define MANGLE_OPTION_TAKE(T)            option_##T##_take
#endif

/* ============================================================================
   COMPARISON FUNCTION NAMES
   Consumed by: option_decl.h, option_defn.h
   ============================================================================ */

/**
 * MANGLE_OPTION_EQ(T)
 *
 * Returns non-zero when both options are None, or both are Some and
 * eq_fn(a_value, b_value) returns non-zero.
 *
 * Default : option_T_eq   (e.g. option_int_eq(opt1, opt2, int_eq))
 *
 * Signature:
 *   static inline int
 *   MANGLE_OPTION_EQ(T)(MANGLE_OPTION_TYPE(T) a,
 *                        MANGLE_OPTION_TYPE(T) b,
 *                        int (*eq_fn)(T, T));
 */
#ifndef MANGLE_OPTION_EQ
#  define MANGLE_OPTION_EQ(T)              option_##T##_eq
#endif

#endif /* CANON_OPTION_MANGLE_H */
