// semantics/result/result.h
#ifndef CANON_SEMANTICS_RESULT_H
#define CANON_SEMANTICS_RESULT_H

#include "result_defn.h"

/**
 * @file result.h
 * @brief Explicit success/failure with value or error – Rust-style Result<T, E> for C
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
 * - Chainable & composable (map, and_then, or_else, etc.)
 * - Panic/unwrap only in debug builds unless CANON_STRICT is defined
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (for inline functions, stdbool.h, compound literals, unions)
 * - Statement expressions require GNU C extension or C23
 * - Define CANON_NO_GNU_EXTENSIONS to disable macros requiring extensions
 * - All core functionality works in strict C99
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
 * - Same ABI stability as regular structs
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
 * Recommended patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * ✓ Always check is_ok / is_err before unwrap
 * ✓ Use TRY / TRY_UNWRAP macros for clean error propagation
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
 * ✗ Don't use exceptions-style error handling (defeats the purpose)
 *
 * map() same-type restriction:
 * ────────────────────────────────────────────────────────────────────────────
 * map() only supports same-type transforms: T → T.
 * It cannot change the value type (e.g. Result<int,E> → Result<float,E>).
 *
 * This is intentional. Heterogeneous map would require a separate generated
 * type per (T, U, E) triple, causing combinatorial instantiation explosion
 * inconsistent with Canon-C's explicit, minimal design.
 *
 * For type-changing transforms, use and_then() with an explicit constructor:
 *
 *   // Instead of: result_float_error_map(r, int_to_float)
 *   // Do this:
 *   result_int_error_and_then(r, int_to_float_result)
 *
 *   result_float_error int_to_float_result(int x) {
 *       return result_float_error_ok((float)x);
 *   }
 *
 * and_then() is the correct composition primitive for structural transforms.
 * map() is for lightweight decoration of the value within the same type.
 *
 * When to use Result vs Option:
 * ────────────────────────────────────────────────────────────────────────────
 * - Use Result<T,E> when you need to distinguish different error types
 * - Use Option<T> when "no value" is the only failure mode
 * - Combine them: Result<Option<T>, E> for "success with optional value" vs "error"
 *
 * Architecture:
 * ────────────────────────────────────────────────────────────────────────────
 * This header is the user-facing facade that provides a simple API.
 * Internally, it uses a modular architecture:
 * - result_impl.h: Pure implementation logic
 * - result_mangle.h: Name mangling conventions (customizable)
 * - result_decl.h: Declaration macros (for separate compilation)
 * - result_defn.h: Definition macros (generates implementations)
 * - result.h: This file (user API)
 *
 * Advanced users can include the individual files for customization.
 * Most users should just use CANON_RESULT(value_type, error_type) from this file.
 *
 * @sa option.h, error.h, contract.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   SIMPLE USER API - CEREMONY-FREE INTERFACE
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Defines a complete Result<T, E> type for given value and error types
 *
 * This is the main macro users should use. It generates a complete Result
 * implementation for the specified type pair, including struct definition and
 * all associated functions.
 *
 * Generated type: result_T_E (e.g., result_int_error, result_float_constcharptr)
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
 *                                        Returns false (no diagnostic) if out is NULL
 *   result_T_E_get_err(r, &out)       → Copy error to *out if Err; returns bool
 *                                        Returns false (no diagnostic) if out is NULL
 *   result_T_E_unwrap_or(r, def)      → Extract value or return default
 *
 * Unsafe extraction (require Ok/Err — see panic contract above):
 *   result_T_E_unwrap(r)              → Extract value or panic
 *   result_T_E_unwrap_err(r)          → Extract error or panic
 *   result_T_E_expect(r, msg)         → Extract value or panic with message
 *
 * Transformations (combinators):
 * ────────────────────────────────────────────────────────────────────────────
 *   result_T_E_map(r, fn)
 *     Transform value if Ok. fn signature: T → T (same-type only).
 *     For type-changing transforms, use and_then() instead.
 *     If Err, returns Err unchanged without calling fn.
 *
 *   result_T_E_map_err(r, fn)
 *     Transform error if Err. fn signature: E → E (same-type only).
 *     If Ok, returns Ok unchanged without calling fn.
 *
 *   result_T_E_and_then(r, fn)
 *     Chain Result-returning operations. fn signature: T → result_T_E.
 *     Prevents nested Result<Result<T,E>,E>.
 *     Use this for type-changing transforms by instantiating a different
 *     result type and returning it from fn.
 *     If Err, returns Err unchanged without calling fn.
 *
 *   result_T_E_or_else(r, fn)
 *     Provide alternative Result if Err. fn signature: E → result_T_E.
 *     If Ok, returns Ok unchanged without calling fn.
 *
 * Comparison:
 *   result_T_E_eq(r1, r2, eq_ok, eq_err)
 *     Check equality. eq_ok signature: (T, T) → bool.
 *                     eq_err signature: (E, E) → bool.
 *
 * Performance: All O(1) except combinators which are O(f) where f is the
 *              supplied function. All operations are stack-only, no allocation.
 * Memory: sizeof(bool) + max(sizeof(T), sizeof(E)) + padding per instance
 *
 * Usage examples:
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * // Define Result types you need
 * typedef enum { ERR_NONE, ERR_IO, ERR_PARSE } error;
 * CANON_RESULT(int, error)
 * CANON_RESULT(float, error)
 *
 * // Use them
 * result_int_error x = result_int_error_ok(42);
 * if (result_int_error_is_ok(x)) {
 *     int value = result_int_error_unwrap(x);
 *     printf("Value: %d\n", value);
 * }
 * ```
 *
 * Type name convention:
 * ────────────────────────────────────────────────────────────────────────────
 * For pointer types, use a typedef first:
 *   typedef const char* constcharptr;
 *   CANON_RESULT(int, constcharptr)  // result_int_constcharptr
 *
 * @param value_type The type for successful values (T)
 * @param error_type The type for error values (E)
 *
 * Note: Must be used at file or global scope, not inside functions.
 *       Use once per (T,E) pair. Each instantiation generates ~2-3KB of
 *       code — only instantiate type pairs you actually use.
 */
#define CANON_RESULT(value_type, error_type) \
    DEFINE_RESULT_ALL(static inline, value_type, error_type)

/**
 * @brief Legacy alias for CANON_RESULT
 *
 * Kept for backward compatibility with existing Canon-C code.
 * New code should prefer CANON_RESULT() for consistency.
 */
#define CANON_C_DEFINE_RESULT(value_type, error_type) \
    CANON_RESULT(value_type, error_type)

/* ════════════════════════════════════════════════════════════════════════════
   PROPAGATION & CONVENIENCE MACROS (GNU C EXTENSIONS OR C23)
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Early return on error – the most common propagation pattern
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
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
 *     TRY(int, error, res1);  // Returns early if Err
 *
 *     result_int_error res2 = step2();
 *     TRY(int, error, res2);  // Returns early if Err
 *
 *     return result_int_error_ok(42);
 * }
 * ```
 *
 * @param type_value Type name for value (e.g., int)
 * @param type_error Type name for error (e.g., error)
 * @param expr       Expression evaluating to Result (evaluated once)
 */
#define TRY(type_value, type_error, expr) \
    do { \
        result_##type_value##_##type_error _res_ = (expr); \
        if (result_##type_value##_##type_error##_is_err(_res_)) { \
            return _res_; \
        } \
    } while (0)

/**
 * @brief Early return with custom/remapped error
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * If the Result is Err, returns immediately with new_error instead of the
 * original error. Useful for error context or error type conversion.
 * expr is evaluated exactly once.
 *
 * Performance: O(1) time, O(1) space
 *
 * Example:
 * ```c
 * result_int_error parse_file(const char* path) {
 *     result_int_ioerror io_res = read_file(path);
 *     TRY_REMAP(int, ioerror, io_res, ERR_FILE_READ);  // Convert error type
 *
 *     return result_int_error_ok(42);
 * }
 * ```
 *
 * @param type_value Type name for value
 * @param type_error Type name for error
 * @param expr       Expression evaluating to Result (evaluated once)
 * @param new_error  Error value to return instead of the original
 */
#define TRY_REMAP(type_value, type_error, expr, new_error) \
    do { \
        result_##type_value##_##type_error _res_ = (expr); \
        if (result_##type_value##_##type_error##_is_err(_res_)) { \
            return result_##type_value##_##type_error##_err(new_error); \
        } \
    } while (0)

/**
 * @brief Unwrap or evaluate to fallback value (expression form)
 *
 * Requires: GNU C statement expressions or C23
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
 * @param type_value Type name for value
 * @param type_error Type name for error
 * @param expr       Expression evaluating to Result (evaluated once)
 * @param fallback   Value to use if Err
 */
#define UNWRAP_OR(type_value, type_error, expr, fallback) \
    ({ \
        result_##type_value##_##type_error _res_ = (expr); \
        result_##type_value##_##type_error##_is_ok(_res_) ? \
            result_##type_value##_##type_error##_unwrap(_res_) : (fallback); \
    })

/**
 * @brief Extract value from Result, early return with Err if error
 *
 * Requires: GNU C statement expressions or C23
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
 *     int x = TRY_UNWRAP(int, error, step1());  // Extract or early return
 *     int y = TRY_UNWRAP(int, error, step2());  // Extract or early return
 *
 *     return result_int_error_ok(x + y);
 * }
 * ```
 *
 * @param type_value Type name for value
 * @param type_error Type name for error
 * @param expr       Expression evaluating to Result (evaluated once)
 *
 * Evaluates to: The unwrapped value (can be used in expressions)
 */
#define TRY_UNWRAP(type_value, type_error, expr) \
    ({ \
        result_##type_value##_##type_error _res_ = (expr); \
        if (result_##type_value##_##type_error##_is_err(_res_)) { \
            return _res_; \
        } \
        result_##type_value##_##type_error##_unwrap(_res_); \
    })

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   COMMON TYPE INSTANTIATIONS (COMMENTED OUT BY DEFAULT)
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * Uncomment the types you need, or define your own in your code.
 *
 * Performance note: Each CANON_RESULT() generates ~2-3KB of code per type pair.
 * Only instantiate type pairs you actually use.
 */

// typedef enum { ERR_NONE, ERR_INVALID, ERR_IO } error;
// CANON_RESULT(int, error)
// CANON_RESULT(long, error)
// CANON_RESULT(size_t, error)
// CANON_RESULT(float, error)
// CANON_RESULT(double, error)

/* For pointer types: */
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
        if (!str) {
            return result_int_error_err(ERR_INVALID_INPUT);
        }

        char* end;
        long val = strtol(str, &end, 10);

        if (end == str || *end != '\0') {
            return result_int_error_err(ERR_INVALID_INPUT);
        }

        if (val > INT_MAX || val < INT_MIN) {
            return result_int_error_err(ERR_INVALID_INPUT);
        }

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
        } else {
            error err = result_int_error_unwrap_err(result);
            printf("Error code: %d\n", err);
        }

        // Pattern 2: safe extraction with pointer
        int value;
        if (result_int_error_get_ok(result, &value)) {
            printf("Extracted: %d\n", value);
        }
        // Note: get_ok returns false silently if out pointer is NULL —
        // no diagnostic is emitted. Passing NULL is not a panic, just
        // a no-op. Always pass a valid pointer in production code.

        // Pattern 3: default value when error
        int value_or_zero = result_int_error_unwrap_or(result, 0);
        printf("Value or zero: %d\n", value_or_zero);

        // Pattern 4: expect with message (for invariants)
        // int must_exist = result_int_error_expect(result, "parse should succeed");
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 4: Error propagation with TRY macro
    // ────────────────────────────────────────────────────────────────────────
    result_int_error add_parsed(const char* a, const char* b) {
        result_int_error res_a = parse_int(a);
        TRY(int, error, res_a);  // Early return if error

        result_int_error res_b = parse_int(b);
        TRY(int, error, res_b);  // Early return if error

        int val_a = result_int_error_unwrap(res_a);
        int val_b = result_int_error_unwrap(res_b);

        return result_int_error_ok(val_a + val_b);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 5: Cleaner with TRY_UNWRAP
    // ────────────────────────────────────────────────────────────────────────
    result_int_error add_parsed_clean(const char* a, const char* b) {
        int val_a = TRY_UNWRAP(int, error, parse_int(a));
        int val_b = TRY_UNWRAP(int, error, parse_int(b));

        return result_int_error_ok(val_a + val_b);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 6: map() — same-type value decoration
    // ────────────────────────────────────────────────────────────────────────
    // map() transforms T → T only. fn must match the value type exactly.

    int double_it(int x) { return x * 2; }

    result_int_error compute(const char* str) {
        result_int_error res = parse_int(str);

        // Decorates the int value if Ok, passes Err through unchanged.
        res = result_int_error_map(res, double_it);

        return res;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 7: and_then() — chaining and type-changing transforms
    // ────────────────────────────────────────────────────────────────────────
    // and_then() is the correct primitive for:
    //   - Chaining Result-returning operations (prevents Result<Result<T,E>,E>)
    //   - Type-changing transforms (int → float, etc.) via a different result type
    //
    // fn signature: T → result_T_E  (returns a Result, not a plain value)

    // Same-type chain: int → int via Result
    result_int_error checked_double(int x) {
        if (x > INT_MAX / 2) return result_int_error_err(ERR_IO);
        return result_int_error_ok(x * 2);
    }

    result_int_error safe_compute(const char* str) {
        return result_int_error_and_then(parse_int(str), checked_double);
    }

    // Type-changing transform: Result<int,E> → Result<float,E>
    // Step 1: instantiate the target result type
    CANON_RESULT(float, error)

    // Step 2: write fn returning the target result type
    result_float_error int_to_float_result(int x) {
        return result_float_error_ok((float)x);
    }

    // Step 3: use and_then on the source result
    // (and_then on result_int_error accepts int → result_int_error,
    //  so wrap the call manually for cross-type chaining)
    result_float_error convert(const char* str) {
        result_int_error r = parse_int(str);
        if (result_int_error_is_err(r)) {
            return result_float_error_err(result_int_error_unwrap_err(r));
        }
        return int_to_float_result(result_int_error_unwrap(r));
    }
    // For type-changing transforms, explicit extraction + construction is
    // clearer than forcing and_then across types. This is intentional —
    // Canon-C favors explicitness over conciseness.

    // ────────────────────────────────────────────────────────────────────────
    // Example 8: or_else() — providing an alternative on error
    // ────────────────────────────────────────────────────────────────────────
    // fn signature: E → result_T_E

    result_int_error fallback_parse(error e) {
        (void)e;
        return result_int_error_ok(0);  // Default to 0 on any error
    }

    result_int_error parse_with_fallback(const char* str) {
        return result_int_error_or_else(parse_int(str), fallback_parse);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 9: Combining with Option
    // ────────────────────────────────────────────────────────────────────────
    #include "option.h"
    CANON_OPTION(int)
    CANON_RESULT(option_int, error)

    // Returns Ok(Some(value)), Ok(None), or Err(error)
    result_option_int_error find_value(int id) {
        if (id < 0) {
            return result_option_int_error_err(ERR_INVALID_INPUT);
        }

        if (id == 0) {
            return result_option_int_error_ok(option_int_none());
        }

        return result_option_int_error_ok(option_int_some(id * 10));
    }
*/

#endif /* CANON_SEMANTICS_RESULT_H */
