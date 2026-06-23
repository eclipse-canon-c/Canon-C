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

// semantics/result/result.h
#ifndef CANON_SEMANTICS_RESULT_H
#define CANON_SEMANTICS_RESULT_H

#include "result_defn.h"

/**
 * @file result.h
 * @brief Explicit success/failure with value or error — Rust-style Result<T, E> for C
 *
 * Represents either:
 * - Ok(value)  → successful result with value of type T
 * - Err(error) → failure with error value of type E
 *
 * Philosophy & goals:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fully explicit error handling (no exceptions, no errno, no sentinels)
 * - Zero-cost abstraction (struct + bool flag + union)
 * - Type-safe via macros (one concrete struct per (T,E) pair)
 * - Header-only by default, supports separate compilation
 * - Chainable & composable (map, and_then, or_else, and, or, etc.)
 * - Panic/unwrap only in debug builds unless CANON_STRICT is defined
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (for inline functions, stdbool.h, compound literals)
 * - TRY and TRY_REMAP use do-while and work in strict C99
 * - UNWRAP_OR and TRY_UNWRAP use GNU C statement expressions ({ })
 *   and require GNU C or C23; guarded by #ifndef CANON_NO_GNU_EXTENSIONS
 * - Define CANON_NO_GNU_EXTENSIONS to disable the statement-expression macros
 * - All core functionality (CANON_RESULT, TRY, TRY_REMAP) works in strict C99
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Each Result instance is independent — no shared state
 * - All functions are thread-safe (no shared mutable state)
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) — constant time for all core operations
 * - Space complexity: O(1) — no allocations, stack-only
 * - Zero runtime overhead — compiles to simple struct + union operations
 * - Inline functions typically compile away completely
 * - Union layout saves memory: max(sizeof(T), sizeof(E)) + bool + padding
 *
 * Memory layout:
 * ────────────────────────────────────────────────────────────────────────────
 * - sizeof(Result<T,E>) = sizeof(bool) + max(sizeof(T), sizeof(E)) + padding
 * - Only one of ok/err is valid at any time (guarded by is_ok flag)
 * - Accessing the wrong union member is undefined behavior
 * - Natural alignment and padding follow standard struct rules
 *
 * Panic contract:
 * ────────────────────────────────────────────────────────────────────────────
 * unwrap(), unwrap_err(), and expect() call require() from contract.h on
 * violation. The exact behavior is controlled by build flags:
 *
 *   Default:           calls canon_contract_handler → abort()
 *   Custom handler:    contract_set_handler(my_fn) replaces abort()
 *   CANON_STRICT:      require() always-on even in release builds
 *   CANON_NO_REQUIRE:  require() compiled out (only when formally verified)
 *
 * Install a custom handler before using unwrap/expect in production:
 *
 *   contract_set_handler(my_embedded_panic_handler);
 *
 * @sa contract.h for full panic behavior documentation
 *
 * get_ok / get_err NULL contract:
 * ────────────────────────────────────────────────────────────────────────────
 * Passing NULL for the output pointer to get_ok() or get_err() is a
 * programming error. It is caught by require() in the implementation and
 * will abort (or invoke the configured panic handler). It does NOT return
 * false silently — false unambiguously means the Result holds the wrong
 * variant, never that the caller passed a bad pointer. Always pass a valid
 * non-NULL pointer.
 *
 * Recommended patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * ✓ Always check is_ok / is_err before unwrap
 * ✓ Use TRY / TRY_REMAP macros for clean error propagation (strict C99)
 * ✓ Use TRY_UNWRAP for inline value extraction with propagation (GNU C / C23)
 * ✓ Prefer get_ok / get_err or unwrap_or over raw unwrap in production
 * ✓ Use expect() with descriptive messages for invariant violations
 * ✓ Use and_then() for type-changing transforms (see map() note below)
 * ✓ Combine with Option<T> when distinguishing "no value" vs "error"
 *
 * Anti-patterns to avoid:
 * ────────────────────────────────────────────────────────────────────────────
 * ✗ Don't unwrap() without checking in production code
 * ✗ Don't ignore errors (always handle Result, even if just logging)
 * ✗ Don't use Result<bool, E> when Option<E> would be clearer
 * ✗ Don't pass NULL to get_ok() or get_err()
 *
 * map() same-type restriction:
 * ────────────────────────────────────────────────────────────────────────────
 * map() only supports same-type transforms: T → T.
 * For type-changing transforms, use and_then() with an explicit constructor:
 *
 *   result_float_error int_to_float_result(int x) {
 *       return result_float_error_ok((float)x);
 *   }
 *   result_float_error r = result_int_error_and_then(src, int_to_float_result);
 *   // Note: int_to_float_result returns a different Result type, so use
 *   // explicit extraction + construction for cross-type transforms.
 *
 * and() vs and_then(), or() vs or_else():
 * ────────────────────────────────────────────────────────────────────────────
 * and(r, other)      — eager: other is always evaluated before the call
 * and_then(r, fn)    — lazy:  fn is only called if r is Ok
 * or(r, other)       — eager: other is always evaluated before the call
 * or_else(r, fn)     — lazy:  fn is only called if r is Err
 *
 * Prefer and_then / or_else when constructing the second value is expensive
 * or has side effects. Use and / or only when the value is already computed.
 *
 * When to use Result vs Option:
 * ────────────────────────────────────────────────────────────────────────────
 * - Use Result<T,E> when you need to distinguish different error types
 * - Use Option<T> when "no value" is the only failure mode
 * - Combine them: Result<Option<T>, E> for "success with optional value" vs error
 *
 * Architecture:
 * ────────────────────────────────────────────────────────────────────────────
 * This header is the user-facing facade. Internally it uses:
 * - result_impl.h:   Pure implementation logic (IMPL_RESULT_* macros)
 * - result_mangle.h: Name mangling conventions (customizable)
 * - result_decl.h:   Declaration macros (for separate compilation)
 * - result_defn.h:   Definition macros (generates implementations)
 * - result.h:        This file (user API)
 *
 * Advanced users can include the individual files for customization.
 * Most users should just use CANON_RESULT(value_type, error_type).
 *
 * @sa option.h, error.h, contract.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   SIMPLE USER API — CEREMONY-FREE INTERFACE
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Define a complete Result<T, E> type for given value and error types
 *
 * This is the main macro users should call. It generates a complete
 * Result implementation for the specified type pair with static inline
 * linkage, safe for use in headers included by multiple translation units.
 *
 * Generated type: result_T_E (e.g., result_int_error, result_float_MyError)
 *
 * Generated functions (where T is value type, E is error type):
 * ────────────────────────────────────────────────────────────────────────────
 * Constructors:
 *   result_T_E_ok(value)              → Create Ok(value)
 *   result_T_E_err(error)             → Create Err(error)
 *
 * Queries:
 *   result_T_E_is_ok(r)               → true if contains success value
 *   result_T_E_is_err(r)              → true if contains error
 *
 * Safe extraction:
 *   result_T_E_get_ok(r, &out)        → Copy value to *out if Ok; returns bool
 *                                        out must not be NULL — aborts if NULL
 *   result_T_E_get_err(r, &out)       → Copy error to *out if Err; returns bool
 *                                        out must not be NULL — aborts if NULL
 *   result_T_E_unwrap_or(r, def)      → Extract value or return default
 *
 * Unsafe extraction (abort on wrong variant — see panic contract above):
 *   result_T_E_unwrap(r)              → Extract value or abort
 *   result_T_E_unwrap_err(r)          → Extract error or abort
 *   result_T_E_expect(r, msg)         → Extract value or abort with message
 *
 * Transformations (combinators):
 * ────────────────────────────────────────────────────────────────────────────
 *   result_T_E_map(r, fn)             T→T transform on Ok; passes Err through
 *   result_T_E_map_err(r, fn)         E→E transform on Err; passes Ok through
 *   result_T_E_and_then(r, fn)        flatMap over Ok (fn: T → Result<T,E>)
 *   result_T_E_or_else(r, fn)         flatMap over Err (fn: E → Result<T,E>)
 *   result_T_E_and(r, other)          return other if Ok, else Err (eager)
 *   result_T_E_or(r, other)           return r if Ok, else other (eager)
 *
 * Comparison:
 *   result_T_E_eq(r1, r2, eq_ok, eq_err)
 *
 * Performance: All O(1) except combinators which are O(f) where f is the
 *              supplied function. All operations are stack-only, no allocation.
 * Memory: sizeof(bool) + max(sizeof(T), sizeof(E)) + padding per instance.
 *
 * Usage example:
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * typedef enum { ERR_NONE, ERR_IO, ERR_PARSE } MyError;
 * CANON_RESULT(int, MyError)
 *
 * result_int_MyError parse(const char* s) {
 *     if (!s) return result_int_MyError_err(ERR_PARSE);
 *     return result_int_MyError_ok(42);
 * }
 *
 * void example(void) {
 *     result_int_MyError r = parse("42");
 *     if (result_int_MyError_is_ok(r)) {
 *         int v = result_int_MyError_unwrap(r);
 *         printf("value: %d\n", v);
 *     }
 * }
 * ```
 *
 * Type name convention:
 * ────────────────────────────────────────────────────────────────────────────
 * For pointer types, use a typedef first:
 *   typedef const char* constcharptr;
 *   CANON_RESULT(int, constcharptr)   // result_int_constcharptr
 *
 * Note: Must be used at file or global scope, not inside functions.
 *       Use once per (T,E) pair. Only instantiate type pairs you actually use.
 *
 * @param value_type The type for successful values (T)
 * @param error_type The type for error values (E)
 */
#define CANON_RESULT(value_type, error_type) \
    DEFINE_RESULT_ALL(static inline, value_type, error_type)

/* ════════════════════════════════════════════════════════════════════════════
   PROPAGATION MACROS — STRICT C99 (do-while, no statement expressions)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Early return on error — the most common propagation pattern
 *
 * Works in strict C99 (uses do-while, not statement expressions).
 * Always available regardless of CANON_NO_GNU_EXTENSIONS.
 *
 * If the Result is Err, returns immediately from the enclosing function
 * with the same Err value. If Ok, execution continues normally.
 * expr is evaluated exactly once.
 *
 * Performance: O(1) time, O(1) space
 *
 * Example:
 * ```c
 * result_int_error do_work(void) {
 *     result_int_error res1 = step1();
 *     TRY(int, error, res1);   // returns early if Err
 *
 *     result_int_error res2 = step2();
 *     TRY(int, error, res2);   // returns early if Err
 *
 *     return result_int_error_ok(42);
 * }
 * ```
 *
 * @param type_value Type name for the value (e.g., int)
 * @param type_error Type name for the error (e.g., error)
 * @param expr       Expression evaluating to Result (evaluated once)
 */
#define TRY(type_value, type_error, expr) \
    do { \
        result_##type_value##_##type_error _try_res_ = (expr); \
        if (result_##type_value##_##type_error##_is_err(_try_res_)) { \
            return _try_res_; \
        } \
    } while (0)

/**
 * @brief Early return with a remapped error
 *
 * Works in strict C99 (uses do-while, not statement expressions).
 * Always available regardless of CANON_NO_GNU_EXTENSIONS.
 *
 * If the Result is Err, returns immediately with new_error instead of the
 * original error. Useful for adding context or converting error types.
 * expr is evaluated exactly once.
 *
 * Performance: O(1) time, O(1) space
 *
 * Example:
 * ```c
 * result_int_error parse_file(const char* path) {
 *     result_int_ioerror io = read_file(path);
 *     TRY_REMAP(int, ioerror, io, ERR_FILE_READ);
 *     return result_int_error_ok(42);
 * }
 * ```
 *
 * @param type_value Type name for the value
 * @param type_error Type name for the error
 * @param expr       Expression evaluating to Result (evaluated once)
 * @param new_error  Error value to return instead of the original
 */
#define TRY_REMAP(type_value, type_error, expr, new_error) \
    do { \
        result_##type_value##_##type_error _try_res_ = (expr); \
        if (result_##type_value##_##type_error##_is_err(_try_res_)) { \
            return result_##type_value##_##type_error##_err(new_error); \
        } \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════════
   PROPAGATION MACROS — GNU C / C23 (statement expressions)
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Unwrap or evaluate to fallback value — usable inline in expressions
 *
 * Requires GNU C statement expressions ({ }) or C23.
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Similar to unwrap_or(), but usable inline in expressions.
 * expr is evaluated exactly once.
 *
 * Performance: O(1) time, O(1) space
 *
 * Example:
 * ```c
 * int x = UNWRAP_OR(int, error, parse_int("123"), 0);
 * ```
 *
 * @param type_value Type name for the value
 * @param type_error Type name for the error
 * @param expr       Expression evaluating to Result (evaluated once)
 * @param fallback   Value to use if Err
 */
#define UNWRAP_OR(type_value, type_error, expr, fallback) \
    ({ \
        result_##type_value##_##type_error _uor_res_ = (expr); \
        result_##type_value##_##type_error##_is_ok(_uor_res_) \
            ? result_##type_value##_##type_error##_unwrap(_uor_res_) \
            : (fallback); \
    })

/**
 * @brief Extract value from Result; early return with Err if error
 *
 * Requires GNU C statement expressions ({ }) or C23.
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Combines unwrapping with error propagation. If Ok, evaluates to the
 * unwrapped value (usable in expressions). If Err, returns immediately
 * from the enclosing function with the same Err.
 * expr is evaluated exactly once.
 *
 * Performance: O(1) time, O(1) space
 *
 * Example:
 * ```c
 * result_int_error do_work(void) {
 *     int x = TRY_UNWRAP(int, error, step1());
 *     int y = TRY_UNWRAP(int, error, step2());
 *     return result_int_error_ok(x + y);
 * }
 * ```
 *
 * @param type_value Type name for the value
 * @param type_error Type name for the error
 * @param expr       Expression evaluating to Result (evaluated once)
 *
 * Evaluates to: the unwrapped value (can be used in expressions)
 */
#define TRY_UNWRAP(type_value, type_error, expr) \
    ({ \
        result_##type_value##_##type_error _tu_res_ = (expr); \
        if (result_##type_value##_##type_error##_is_err(_tu_res_)) { \
            return _tu_res_; \
        } \
        result_##type_value##_##type_error##_unwrap(_tu_res_); \
    })

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   COMMON TYPE INSTANTIATIONS (COMMENTED OUT BY DEFAULT)
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * Uncomment the types you need, or define your own in your code.
 * Only instantiate type pairs you actually use.
 */

// typedef enum { ERR_NONE, ERR_INVALID, ERR_IO } error;
// CANON_RESULT(int, error)
// CANON_RESULT(long, error)
// CANON_RESULT(size_t, error)
// CANON_RESULT(float, error)
// CANON_RESULT(double, error)

/* For pointer types — typedef first: */
// typedef const char* constcharptr;
// typedef void* voidptr;
// CANON_RESULT(int, constcharptr)
// CANON_RESULT(voidptr, error)

/* For custom types: */
// typedef struct { int x, y; } Point;
// CANON_RESULT(Point, error)

/* ════════════════════════════════════════════════════════════════════════════
   COMPLETE USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
    // ────────────────────────────────────────────────────────────────────────
    // Example 1: Define needed Result types
    // ────────────────────────────────────────────────────────────────────────
    typedef enum {
        ERR_NONE,
        ERR_NOT_FOUND,
        ERR_INVALID_INPUT,
        ERR_IO
    } error;

    CANON_RESULT(int, error)
    CANON_RESULT(float, error)

    // ────────────────────────────────────────────────────────────────────────
    // Example 2: Function returning Result
    // ────────────────────────────────────────────────────────────────────────
    result_int_error parse_int(const char* str) {
        if (!str) return result_int_error_err(ERR_INVALID_INPUT);
        char* end;
        long val = strtol(str, &end, 10);
        if (end == str || *end != '\0') return result_int_error_err(ERR_INVALID_INPUT);
        if (val > INT32_MAX || val < INT32_MIN) return result_int_error_err(ERR_INVALID_INPUT);
        return result_int_error_ok((int)val);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 3: Safe usage patterns (recommended for production)
    // ────────────────────────────────────────────────────────────────────────
    void example_safe_usage(void) {
        result_int_error result = parse_int("123");

        // Pattern 1: explicit check + unwrap
        if (result_int_error_is_ok(result)) {
            int value = result_int_error_unwrap(result);
            printf("Value: %d\n", value);
        }

        // Pattern 2: safe extraction — out must not be NULL
        int value;
        if (result_int_error_get_ok(result, &value)) {
            printf("Extracted: %d\n", value);
        }

        // Pattern 3: default value when error
        int value_or_zero = result_int_error_unwrap_or(result, 0);

        // Pattern 4: expect with message (for invariants)
        int must_exist = result_int_error_expect(result, "parse must succeed");
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 4: Error propagation with TRY (strict C99)
    // ────────────────────────────────────────────────────────────────────────
    result_int_error add_parsed(const char* a, const char* b) {
        result_int_error res_a = parse_int(a);
        TRY(int, error, res_a);

        result_int_error res_b = parse_int(b);
        TRY(int, error, res_b);

        return result_int_error_ok(
            result_int_error_unwrap(res_a) +
            result_int_error_unwrap(res_b));
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 5: Cleaner with TRY_UNWRAP (GNU C / C23)
    // ────────────────────────────────────────────────────────────────────────
    result_int_error add_parsed_clean(const char* a, const char* b) {
        int val_a = TRY_UNWRAP(int, error, parse_int(a));
        int val_b = TRY_UNWRAP(int, error, parse_int(b));
        return result_int_error_ok(val_a + val_b);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 6: map() — same-type value decoration
    // ────────────────────────────────────────────────────────────────────────
    int double_it(int x) { return x * 2; }

    result_int_error compute(const char* str) {
        return result_int_error_map(parse_int(str), double_it);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 7: and_then() — chaining fallible operations
    // ────────────────────────────────────────────────────────────────────────
    result_int_error checked_double(int x) {
        if (x > INT32_MAX / 2) return result_int_error_err(ERR_IO);
        return result_int_error_ok(x * 2);
    }

    result_int_error safe_compute(const char* str) {
        return result_int_error_and_then(parse_int(str), checked_double);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 8: or_else() — providing an alternative on error
    // ────────────────────────────────────────────────────────────────────────
    result_int_error fallback_parse(error e) {
        (void)e;
        return result_int_error_ok(0);
    }

    result_int_error parse_with_fallback(const char* str) {
        return result_int_error_or_else(parse_int(str), fallback_parse);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 9: and() / or() — eager short-circuit combinators
    // ────────────────────────────────────────────────────────────────────────
    result_int_error second = result_int_error_ok(99);

    // and: if first is Ok, return second; else propagate first's Err
    result_int_error r = result_int_error_and(parse_int("42"), second);

    // or: if first is Ok, return it; else return second
    result_int_error s = result_int_error_or(parse_int("bad"), second);
*/

#endif /* CANON_SEMANTICS_RESULT_H */
