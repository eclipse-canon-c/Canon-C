// semantics/option.h
#ifndef CANON_C_SEMANTICS_OPTION_H
#define CANON_C_SEMANTICS_OPTION_H
#include <stdbool.h>
#include <assert.h>
/**
 * @file option.h
 * @brief Explicit presence/absence of a value – Rust-style Option<T> for C
 *
 * Represents either:
 * - Some(value) → value is present
 * - None → value is absent (safe alternative to NULL/sentinels)
 *
 * Philosophy & goals:
 * - Fully explicit nullability (no accidental dereference of NULL)
 * - Zero-cost abstraction (bool + value, natural alignment/padding)
 * - Type-safe via macros (one concrete struct per type)
 * - Header-only, no runtime dependencies
 * - Chainable & composable (map, and_then, or_else, filter...)
 * - Panic/unwrap only in debug builds (production code should check)
 *
 * Portability:
 * - Requires C99 or later (for inline functions, stdbool.h, compound literals)
 * - Statement expressions require GNU C extension or C23
 * - Define CANON_NO_GNU_EXTENSIONS to disable macros requiring extensions
 * - All core functionality works in strict C99
 *
 * Thread-safety: Each Option instance is independent - no shared state
 *
 * Performance:
 * - Time complexity: O(1) — constant time for all operations
 * - Space complexity: O(1) — bool + value + alignment padding only
 * - No dynamic allocations or runtime overhead
 * - Zero-cost abstraction (compiles to simple struct operations)
 *
 * Recommended patterns:
 * ✓ Always check is_some / is_none before unwrap
 * ✓ Use TRY_SOME for clean propagation (if using Result types)
 * ✓ Prefer unwrap_or / get / filter over raw unwrap in production code
 * ✓ Use expect() with descriptive messages for invariant violations
 * ✓ Combine with Result<T,E> for comprehensive error handling
 *
 * Anti-patterns to avoid:
 * ✗ Don't unwrap() without checking in production code
 * ✗ Don't use Option for error reporting (use Result<T,E> instead)
 * ✗ Don't use Option<bool> (prefer explicit Result or enum)
 *
 * Classic example:
 * CANON_C_DEFINE_OPTION(int)
 *
 * option_int maybe_parse(const char* s) {
 *     if (!s) return option_int_none();
 *     // ... parsing logic ...
 *     return option_int_some(42);
 * }
 *
 * int process(void) {
 *     option_int maybe = maybe_parse("42");
 *     if (option_int_is_none(maybe)) {
 *         return ERROR_PARSE_FAILED;
 *     }
 *     int n = option_int_unwrap(maybe);
 *     return n * 2;
 * }
 */

/**
 * @brief Defines a concrete Option<T> type for given value type
 *
 * Generates a complete Option implementation for the specified type,
 * including struct definition and all associated functions.
 *
 * Generated type: option_##type
 * Generated functions:
 * - Constructors: option_##type##_some(value), option_##type##_none()
 * - Queries: option_##type##_is_some(o), option_##type##_is_none(o)
 * - Safe accessors: option_##type##_get(o, &out)
 * - Unsafe accessors: option_##type##_unwrap(o), option_##type##_expect(o, msg)
 * - Defaults: option_##type##_unwrap_or(o, fallback)
 * - Transformations: option_##type##_map(o, fn)
 * - Chaining: option_##type##_and_then(o, fn), option_##type##_or_else(o, fn)
 * - Filtering: option_##type##_filter(o, predicate)
 * - Combining: option_##type##_zip(o1, o2, combine)
 * - Replacement: option_##type##_replace(o, new_value)
 * - Taking: option_##type##_take(&o)
 *
 * Usage:
 * CANON_C_DEFINE_OPTION(int) // Defines option_int
 * CANON_C_DEFINE_OPTION(float) // Defines option_float
 * CANON_C_DEFINE_OPTION(MyStruct) // Defines option_MyStruct
 *
 * Note: This must be used at file or global scope, not inside functions.
 * Use once per type in a header or source file.
 */
#define CANON_C_DEFINE_OPTION(type) \
\
/** \
 * @brief Option type for 'type' - represents optional value \
 * \
 * Fields: \
 * - has_value: true if value is present (Some), false if absent (None) \
 * - value: the contained value (only valid when has_value is true) \
 * \
 * Do not access fields directly - use the provided functions. \
 */ \
typedef struct { \
    bool has_value; \
    type value; \
} option_##type; \
\
/** \
 * @brief Constructs Some(value) - an Option containing a value \
 * \
 * @param v Value to wrap \
 * @return Option containing the value \
 */ \
static inline option_##type option_##type##_some(type v) { \
    return (option_##type){ .has_value = true, .value = v }; \
} \
\
/** \
 * @brief Constructs None - an Option without a value \
 * \
 * @return Empty Option \
 */ \
static inline option_##type option_##type##_none(void) { \
    return (option_##type){ .has_value = false }; \
} \
\
/** \
 * @brief Checks if Option contains a value \
 * \
 * @param o Option to check \
 * @return true if Some(value), false if None \
 */ \
static inline bool option_##type##_is_some(option_##type o) { \
    return o.has_value; \
} \
\
/** \
 * @brief Checks if Option is empty \
 * \
 * @param o Option to check \
 * @return true if None, false if Some(value) \
 */ \
static inline bool option_##type##_is_none(option_##type o) { \
    return !o.has_value; \
} \
\
/** \
 * @brief Safely extracts value if present \
 * \
 * This is the recommended way to extract values in production code. \
 * \
 * @param o Option to extract from \
 * @param out Pointer to store the value (NULL-safe) \
 * @return true if value was extracted, false if None or out is NULL \
 */ \
static inline bool option_##type##_get(option_##type o, type* out) { \
    if (o.has_value && out) { \
        *out = o.value; \
        return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Extracts value or returns fallback \
 * \
 * Safe alternative to unwrap() - never panics. \
 * \
 * @param o Option to extract from \
 * @param fallback Value to return if None \
 * @return Contained value if Some, fallback if None \
 */ \
static inline type option_##type##_unwrap_or( \
    option_##type o, type fallback) { \
    return o.has_value ? o.value : fallback; \
} \
\
/** \
 * @brief Extracts value, panicking if None \
 * \
 * ⚠️ WARNING: Only use when you are certain the value is present! \
 * In debug builds, this asserts. In release builds, undefined if None. \
 * \
 * Prefer: get(), unwrap_or(), expect() over unwrap() in production. \
 * \
 * @param o Option to unwrap \
 * @return Contained value \
 * \
 * Panics: If o is None (assertion failure in debug builds) \
 */ \
static inline type option_##type##_unwrap(option_##type o) { \
    assert(o.has_value && "option_unwrap called on None"); \
    return o.value; \
} \
\
/** \
 * @brief Extracts value with custom panic message \
 * \
 * Like unwrap(), but with a descriptive error message for debugging. \
 * Use this for invariant violations that should never happen. \
 * \
 * @param o Option to extract from \
 * @param msg Error message if None (used in assertion) \
 * @return Contained value \
 * \
 * Panics: If o is None (assertion failure in debug builds with msg) \
 */ \
static inline type option_##type##_expect( \
    option_##type o, const char* msg) { \
    assert(o.has_value && msg); \
    (void)msg; /* suppress unused warning in release builds */ \
    return o.value; \
} \
\
/** \
 * @brief Transforms the contained value if present \
 * \
 * Applies function f to the contained value, wrapping result in Option. \
 * If None, returns None without calling f. \
 * \
 * @param o Option to transform \
 * @param f Transformation function \
 * @return Some(f(value)) if o is Some(value), None if o is None \
 */ \
static inline option_##type option_##type##_map( \
    option_##type o, type (*f)(type)) { \
    return o.has_value ? option_##type##_some(f(o.value)) : option_##type##_none(); \
} \
\
/** \
 * @brief Chains Option-returning operations \
 * \
 * Like map(), but f returns an Option instead of a plain value. \
 * Prevents nested Option<Option<T>>. \
 * \
 * @param o Option to chain from \
 * @param f Function returning Option \
 * @return f(value) if o is Some(value), None if o is None \
 */ \
static inline option_##type option_##type##_and_then( \
    option_##type o, option_##type (*f)(type)) { \
    return o.has_value ? f(o.value) : option_##type##_none(); \
} \
\
/** \
 * @brief Provides alternative if None \
 * \
 * Returns o if Some, otherwise calls fallback() to get alternative. \
 * \
 * @param o Option to check \
 * @param fallback Function providing alternative Option \
 * @return o if Some, fallback() if None \
 */ \
static inline option_##type option_##type##_or_else( \
    option_##type o, option_##type (*fallback)(void)) { \
    return o.has_value ? o : fallback(); \
} \
\
/** \
 * @brief Keeps value only if predicate returns true \
 * \
 * Converts Some(value) to None if predicate(value) is false. \
 * Useful for conditional extraction. \
 * \
 * @param o Option to filter \
 * @param pred Predicate function \
 * @return o if Some(v) and pred(v), None otherwise \
 */ \
static inline option_##type option_##type##_filter( \
    option_##type o, bool (*pred)(type)) { \
    return (o.has_value && pred(o.value)) ? o : option_##type##_none(); \
} \
\
/** \
 * @brief Combines two Options with a function \
 * \
 * Returns Some(combine(a, b)) if both Options are Some. \
 * Returns None if either Option is None. \
 * \
 * @param o1 First Option \
 * @param o2 Second Option \
 * @param combine Function to combine values \
 * @return Some(combine(v1, v2)) if both Some, None otherwise \
 */ \
static inline option_##type option_##type##_zip( \
    option_##type o1, option_##type o2, type (*combine)(type, type)) { \
    if (o1.has_value && o2.has_value) { \
        return option_##type##_some(combine(o1.value, o2.value)); \
    } \
    return option_##type##_none(); \
} \
\
/** \
 * @brief Replaces the value, returning the old Option \
 * \
 * Updates the Option with a new value, returning the previous state. \
 * \
 * @param o Pointer to Option to update \
 * @param new_value New value to set \
 * @return Previous Option state \
 */ \
static inline option_##type option_##type##_replace( \
    option_##type* o, type new_value) { \
    assert(o != NULL && "option_replace: o parameter cannot be NULL"); \
    option_##type old = *o; \
    *o = option_##type##_some(new_value); \
    return old; \
} \
\
/** \
 * @brief Takes the value out, leaving None \
 * \
 * Extracts the value from the Option, replacing it with None. \
 * Useful for moving values out of Option containers. \
 * \
 * @param o Pointer to Option to take from \
 * @return The previous Option state \
 * \
 * Postcondition: *o is None after this call \
 */ \
static inline option_##type option_##type##_take(option_##type* o) { \
    assert(o != NULL && "option_take: o parameter cannot be NULL"); \
    option_##type old = *o; \
    *o = option_##type##_none(); \
    return old; \
} \
\
/** \
 * @brief Checks equality of two Options \
 * \
 * Two Options are equal if: \
 * - Both are None, OR \
 * - Both are Some and their values compare equal via eq function \
 * \
 * @param o1 First Option \
 * @param o2 Second Option \
 * @param eq Equality comparison function for values \
 * @return true if Options are equal, false otherwise \
 */ \
static inline bool option_##type##_eq( \
    option_##type o1, option_##type o2, bool (*eq)(type, type)) { \
    if (!o1.has_value && !o2.has_value) return true; \
    if (o1.has_value != o2.has_value) return false; \
    return eq(o1.value, o2.value); \
}

/* ────────────────────────────────────────────────────────────────────────────
   Propagation & convenience macros (require GNU C extensions or C23)
   ──────────────────────────────────────────────────────────────────────────── */
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
 */
#define UNWRAP_OR_DEFAULT(type, opt_expr, default_val) \
    ({ \
        option_##type _opt_ = (opt_expr); \
        option_##type##_is_some(_opt_) ? \
            option_##type##_unwrap(_opt_) : (default_val); \
    })
#endif /* CANON_NO_GNU_EXTENSIONS */

/* ────────────────────────────────────────────────────────────────────────────
   Common type instantiations
   ──────────────────────────────────────────────────────────────────────────── */

/* Uncomment the types you need, or define your own in your code: */
// CANON_C_DEFINE_OPTION(int)
// CANON_C_DEFINE_OPTION(unsigned)
// CANON_C_DEFINE_OPTION(long)
// CANON_C_DEFINE_OPTION(size_t)
// CANON_C_DEFINE_OPTION(float)
// CANON_C_DEFINE_OPTION(double)
// CANON_C_DEFINE_OPTION(char)
// CANON_C_DEFINE_OPTION(bool)

/* For pointer types: */
// typedef void* void_ptr;
// CANON_C_DEFINE_OPTION(void_ptr)

/* For custom types: */
// CANON_C_DEFINE_OPTION(MyStruct)

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ──────────────────────────────────────────────────────────────────────────── */

#include "option.h"

/* Define Option types we need */
CANON_C_DEFINE_OPTION(int)
CANON_C_DEFINE_OPTION(float)

/* Function that might fail to parse */
option_int parse_int(const char* str) {
    if (!str) return option_int_none();
    
    char* end;
    long val = strtol(str, &end, 10);
    
    if (end == str || *end != '\0') {
        return option_int_none(); // Parse failed
    }
    
    return option_int_some((int)val);
}

/* Safe usage with checking */
void example_safe(void) {
    option_int result = parse_int("42");
    
    // Method 1: Check before unwrap
    if (option_int_is_some(result)) {
        int value = option_int_unwrap(result);
        printf("Parsed: %d\n", value);
    } else {
        printf("Parse failed\n");
    }
    
    // Method 2: Use get()
    int value;
    if (option_int_get(result, &value)) {
        printf("Parsed: %d\n", value);
    }
    
    // Method 3: Provide default
    int value = option_int_unwrap_or(result, 0);
}

/* Chaining operations */
int double_value(int x) { return x * 2; }
bool is_positive(int x) { return x > 0; }

void example_chaining(void) {
    option_int result = parse_int("21");
    
    // Using functions explicitly
    result = option_int_map(result, double_value);
    result = option_int_filter(result, is_positive);
}

#endif /* CANON_C_SEMANTICS_OPTION_H */
