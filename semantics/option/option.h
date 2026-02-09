// semantics/option/option.h
#ifndef CANON_SEMANTICS_OPTION_H
#define CANON_SEMANTICS_OPTION_H

#include "option_defn.h"

/**
 * @file option.h
 * @brief Explicit presence/absence of a value – Rust-style Option<T> for C
 *
 * Represents either:
 * - Some(value) → value is present
 * - None → value is absent (safe alternative to NULL/sentinels)
 *
 * Philosophy & goals:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fully explicit nullability (no accidental dereference of NULL)
 * - Zero-cost abstraction (bool + value, natural alignment/padding)
 * - Type-safe via macros (one concrete struct per type)
 * - Header-only by default, supports separate compilation
 * - Chainable & composable (map, and_then, or_else, filter...)
 * - Panic/unwrap only in debug builds (production code should check)
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (for inline functions, stdbool.h, compound literals)
 * - Statement expressions require GNU C extension or C23
 * - Define CANON_NO_GNU_EXTENSIONS to disable macros requiring extensions
 * - All core functionality works in strict C99
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each Option instance is independent - no shared state
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) — constant time for all operations (except combinators)
 * - Space complexity: O(1) — bool + value + alignment padding only
 * - No dynamic allocations or runtime overhead
 * - Zero-cost abstraction (compiles to simple struct operations)
 * - Inlined by default (static inline)
 *
 * Memory layout:
 * ────────────────────────────────────────────────────────────────────────────
 * - Size: sizeof(bool) + sizeof(T) + alignment padding
 * - Typical: 2-16 bytes depending on type and alignment
 * - Stack allocated, no heap involvement
 *
 * Recommended patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * ✓ Always check is_some / is_none before unwrap
 * ✓ Use TRY_SOME for clean propagation (if using Result types)
 * ✓ Prefer unwrap_or / get / filter over raw unwrap in production code
 * ✓ Use expect() with descriptive messages for invariant violations
 * ✓ Combine with Result<T,E> for comprehensive error handling
 *
 * Anti-patterns to avoid:
 * ────────────────────────────────────────────────────────────────────────────
 * ✗ Don't unwrap() without checking in production code
 * ✗ Don't use Option for error reporting (use Result<T,E> instead)
 * ✗ Don't use Option<bool> (prefer explicit Result or enum)
 *
 * Architecture:
 * ────────────────────────────────────────────────────────────────────────────
 * This header is the user-facing facade that provides a simple API.
 * Internally, it uses a modular architecture:
 * - option_impl.h: Pure implementation logic
 * - option_mangle.h: Name mangling conventions (customizable)
 * - option_decl.h: Declaration macros (for separate compilation)
 * - option_defn.h: Definition macros (generates implementations)
 * - option.h: This file (simple user API)
 *
 * Advanced users can include the individual files for customization.
 * Most users should just use CANON_OPTION(type) from this file.
 *
 * @sa result.h, error.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   SIMPLE USER API - CEREMONY-FREE INTERFACE
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Defines a complete Option<T> type for given value type
 *
 * This is the main macro users should use. It generates a complete Option
 * implementation for the specified type, including struct definition and
 * all associated functions.
 *
 * Generated type: option_T (e.g., option_int, option_float, option_MyStruct)
 *
 * Generated functions (where T is your type):
 * ────────────────────────────────────────────────────────────────────────────
 * Constructors:
 *   - option_T_some(value)       → Create Some(value)
 *   - option_T_none()             → Create None
 *
 * Queries:
 *   - option_T_is_some(o)         → Check if contains value
 *   - option_T_is_none(o)         → Check if empty
 *
 * Safe extraction:
 *   - option_T_get(o, &out)       → Extract to pointer (safe, NULL-allowed)
 *   - option_T_unwrap_or(o, def)  → Extract or use default
 *
 * Unsafe extraction (use with caution):
 *   - option_T_unwrap(o)          → Extract or panic
 *   - option_T_expect(o, msg)     → Extract or panic with message
 *
 * Transformations (combinators):
 *   - option_T_map(o, fn)         → Transform value if present
 *   - option_T_and_then(o, fn)    → Chain Option-returning operations
 *   - option_T_or_else(o, fn)     → Provide alternative if None
 *   - option_T_filter(o, pred)    → Keep only if predicate matches
 *   - option_T_zip(o1, o2, fn)    → Combine two Options
 *
 * Mutation:
 *   - option_T_replace(&o, val)   → Replace value, return old (strict)
 *   - option_T_take(&o)           → Take value out, leave None (strict)
 *
 * Comparison:
 *   - option_T_eq(o1, o2, eq_fn)  → Check equality
 *
 * Performance: All O(1) except combinators which are O(f) where f is the function
 * Memory: sizeof(bool) + sizeof(T) + alignment padding per instance
 *
 * Usage examples:
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * // Define Option types you need
 * CANON_OPTION(int)
 * CANON_OPTION(float)
 * 
 * // Use them
 * option_int x = option_int_some(42);
 * if (option_int_is_some(x)) {
 *     int value = option_int_unwrap(x);
 *     printf("Value: %d\n", value);
 * }
 * ```
 *
 * @param type The value type (e.g., int, float, MyStruct, void*)
 *
 * Note: This must be used at file or global scope, not inside functions.
 * Use once per type in your code.
 */
#define CANON_OPTION(type) \
    DEFINE_OPTION_ALL(static inline, type)

/**
 * @brief Legacy alias for CANON_OPTION
 *
 * Kept for backward compatibility with existing Canon-C code.
 * New code should prefer CANON_OPTION() for consistency.
 */
#define CANON_C_DEFINE_OPTION(type) CANON_OPTION(type)

/* ════════════════════════════════════════════════════════════════════════════
   PROPAGATION & CONVENIENCE MACROS (GNU C EXTENSIONS OR C23)
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Unwraps Option or returns from function with error code
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Useful for error propagation in functions returning Result types.
 * If the Option is None, returns immediately with the error code.
 * If Some, extracts and returns the value.
 *
 * Performance: O(1) time, O(1) space
 *
 * Example:
 * ```c
 * result_int_error do_work(void) {
 *     option_int opt = get_optional_value();
 *     int value = TRY_SOME(int, opt, err(error, ERROR_NOT_FOUND));
 *     // Use value here...
 * }
 * ```
 *
 * @param type Type name (e.g., int, float)
 * @param opt_expr Expression evaluating to option_##type
 * @param err_code Value to return if None
 */
#define TRY_SOME(type, opt_expr, err_code) \
    ({ \
        option_##type _opt_ = (opt_expr); \
        if (option_##type##_is_none(_opt_)) return (err_code); \
        option_##type##_unwrap(_opt_); \
    })

/**
 * @brief Unwraps Option or evaluates to default value
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Similar to unwrap_or(), but as an expression macro.
 * Useful in compound expressions.
 *
 * Performance: O(1) time, O(1) space
 *
 * Example:
 * ```c
 * int x = UNWRAP_OR_DEFAULT(int, get_config(), 100);
 * ```
 *
 * @param type Type name
 * @param opt_expr Expression evaluating to option_##type
 * @param default_val Default value if None
 */
#define UNWRAP_OR_DEFAULT(type, opt_expr, default_val) \
    ({ \
        option_##type _opt_ = (opt_expr); \
        option_##type##_is_some(_opt_) ? \
            option_##type##_unwrap(_opt_) : (default_val); \
    })

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   COMMON TYPE INSTANTIATIONS (COMMENTED OUT BY DEFAULT)
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * Uncomment the types you need, or define your own in your code.
 * 
 * Performance note: Each CANON_OPTION() generates ~1-2KB of code per type.
 * Only instantiate types you actually use.
 */

// CANON_OPTION(int)
// CANON_OPTION(unsigned)
// CANON_OPTION(long)
// CANON_OPTION(size_t)
// CANON_OPTION(float)
// CANON_OPTION(double)
// CANON_OPTION(char)
// CANON_OPTION(bool)

/* For pointer types: */
// typedef void* void_ptr;
// CANON_OPTION(void_ptr)

/* For custom types: */
// typedef struct { int x, y; } Point;
// CANON_OPTION(Point)

/* ════════════════════════════════════════════════════════════════════════════
   COMPLETE USAGE EXAMPLES
   ════════════════════════════════════════════════════════════════════════════ */

/*
    // ────────────────────────────────────────────────────────────────────────
    // Example 1: Define needed Option types
    // ────────────────────────────────────────────────────────────────────────
    CANON_OPTION(int)
    CANON_OPTION(float)

    // ────────────────────────────────────────────────────────────────────────
    // Example 2: Function that might return a parsed integer or nothing
    // ────────────────────────────────────────────────────────────────────────
    option_int parse_int(const char* str) {
        if (!str) return option_int_none();

        char* end;
        long val = strtol(str, &end, 10);

        if (end == str || *end != '\0') {
            return option_int_none(); // invalid format
        }

        // Could add range checks here if needed
        return option_int_some((int)val);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 3: Safe usage patterns (recommended for production)
    // ────────────────────────────────────────────────────────────────────────
    void example_safe_usage(void) {
        option_int result = parse_int("123");

        // Pattern 1: explicit check + unwrap
        if (option_int_is_some(result)) {
            int value = option_int_unwrap(result);
            printf("Value: %d\n", value);
        } else {
            printf("No value\n");
        }

        // Pattern 2: safe extraction with pointer
        int extracted;
        if (option_int_get(result, &extracted)) {
            printf("Extracted: %d\n", extracted);
        }

        // Pattern 3: default value when absent
        int value_or_zero = option_int_unwrap_or(result, 0);
        printf("Value or zero: %d\n", value_or_zero);

        // Pattern 4: expect with message (for invariants)
        // int must_exist = option_int_expect(result, "parse_int should have succeeded");
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 4: Chaining operations (map, filter, and_then)
    // ────────────────────────────────────────────────────────────────────────
    int double_it(int x) { return x * 2; }
    bool is_even(int x)   { return (x % 2) == 0; }

    void example_chaining(void) {
        option_int opt = parse_int("21");

        // Apply transformation if present
        opt = option_int_map(opt, double_it);  // Some(42)

        // Keep only even values
        opt = option_int_filter(opt, is_even); // Some(42) stays

        // Use default if none
        int final = option_int_unwrap_or(opt, -1); // 42
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 5: and_then chaining (for operations that return Option)
    // ────────────────────────────────────────────────────────────────────────
    option_int half_if_even(int x) {
        if (x % 2 == 0) {
            return option_int_some(x / 2);
        }
        return option_int_none();
    }

    void example_and_then(void) {
        option_int start = option_int_some(42);
        option_int result = option_int_and_then(start, half_if_even);

        int value = option_int_unwrap_or(result, 0);
        // value == 21
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 6: Mutation operations
    // ────────────────────────────────────────────────────────────────────────
    void example_mutation(void) {
        option_int opt = option_int_some(10);

        // Replace value, get old one back
        option_int old = option_int_replace(&opt, 20);
        // opt is now Some(20), old is Some(10)

        // Take value out, leaving None
        option_int taken = option_int_take(&opt);
        // opt is now None, taken is Some(20)
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 7: Working with custom types
    // ────────────────────────────────────────────────────────────────────────
    typedef struct { int x; int y; } Point;
    CANON_OPTION(Point)

    option_Point find_point(int id) {
        if (id == 0) {
            return option_Point_none();
        }
        Point p = { .x = id * 10, .y = id * 20 };
        return option_Point_some(p);
    }

    void use_point(void) {
        option_Point opt = find_point(5);
        
        Point p;
        if (option_Point_get(opt, &p)) {
            printf("Point: (%d, %d)\n", p.x, p.y);
        }
    }
*/

#endif /* CANON_SEMANTICS_OPTION_H */
