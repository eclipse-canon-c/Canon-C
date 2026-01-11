// semantics/result.h
#ifndef CANON_C_SEMANTICS_RESULT_H
#define CANON_C_SEMANTICS_RESULT_H

#include <stdbool.h>
#include <assert.h>

/**
 * @file result.h
 * @brief Explicit success/failure with value or error – Rust-style Result<T, E> for C
 *
 * Represents either:
 * - Ok(value)  → successful result with value of type T
 * - Err(error) → failure with error value of type E
 *
 * Philosophy & goals:
 *   - Fully explicit error handling (no exceptions, no errno, no sentinels)
 *   - Zero-cost abstraction (struct + bool flag + union)
 *   - Type-safe via macros (one concrete struct per (T,E) pair)
 *   - Header-only, no runtime dependencies
 *   - Chainable & composable (map, and_then, or_else, etc.)
 *   - Panic/unwrap only in debug builds (production code should check)
 *
 * Portability:
 *   - Requires C99 or later (for inline functions, stdbool.h, compound literals, unions)
 *   - Statement expressions require GNU C extension or C23
 *   - Define CANON_NO_GNU_EXTENSIONS to disable macros requiring extensions
 *   - All core functionality works in strict C99
 *
 * Thread-safety: Each Result instance is independent - no shared state
 *
 * Performance: Zero runtime overhead - compiles to simple struct + union operations
 *
 * Memory layout:
 *   - Uses union to save space: sizeof(Result<T,E>) ≈ sizeof(bool) + max(sizeof(T), sizeof(E)) + padding
 *   - Only one of ok/err is valid at any time (checked by is_ok flag)
 *   - Same ABI stability as regular structs
 *
 * Recommended patterns:
 *   ✓ Always check is_ok / is_err before unwrap
 *   ✓ Use TRY / TRY_REMAP macros for clean error propagation
 *   ✓ Prefer get_ok / get_err or unwrap_or over raw unwrap in production
 *   ✓ Use expect() with descriptive messages for invariant violations
 *   ✓ Combine with Option<T> when distinguishing "no value" vs "error"
 *
 * Anti-patterns to avoid:
 *   ✗ Don't unwrap() without checking in production code
 *   ✗ Don't ignore errors (always handle Result, even if just logging)
 *   ✗ Don't use Result<bool, E> when Option<E> would be clearer
 *   ✗ Don't use exceptions-style error handling (defeats the purpose)
 *
 * When to use Result vs Option:
 *   - Use Result<T,E> when you need to distinguish different error types
 *   - Use Option<T> when "no value" is the only failure mode
 *   - Combine them: Result<Option<T>, E> for "success with optional value" vs "error"
 *
 * Classic example:
 *   CANON_C_DEFINE_RESULT(int, const char*)
 *   
 *   result_int_constcharptr parse_number(const char* s) {
 *       if (!s) return result_int_constcharptr_err("null input");
 *       // ... parsing logic ...
 *       return result_int_constcharptr_ok(42);
 *   }
 *
 *   int process(void) {
 *       result_int_constcharptr res = parse_number("42");
 *       if (result_int_constcharptr_is_err(res)) {
 *           const char* err;
 *           result_int_constcharptr_get_err(res, &err);
 *           fprintf(stderr, "Error: %s\n", err);
 *           return -1;
 *       }
 *       int n = result_int_constcharptr_unwrap(res);
 *       return n * 2;
 *   }
 */

/**
 * @brief Defines a concrete Result<T, E> type for given value and error types
 *
 * Generates a complete Result implementation for the specified types,
 * including struct definition and all associated functions.
 *
 * Generated type: result_##value_type##_##error_type
 * Generated functions:
 *   - Constructors: result_##value_type##_##error_type##_ok(value)
 *                   result_##value_type##_##error_type##_err(error)
 *   - Queries: result_##value_type##_##error_type##_is_ok(r)
 *              result_##value_type##_##error_type##_is_err(r)
 *   - Safe accessors: result_##value_type##_##error_type##_get_ok(r, &out)
 *                     result_##value_type##_##error_type##_get_err(r, &out)
 *   - Unsafe accessors: result_##value_type##_##error_type##_unwrap(r)
 *                       result_##value_type##_##error_type##_unwrap_err(r)
 *                       result_##value_type##_##error_type##_expect(r, msg)
 *   - Defaults: result_##value_type##_##error_type##_unwrap_or(r, fallback)
 *   - Transformations: result_##value_type##_##error_type##_map(r, fn)
 *                      result_##value_type##_##error_type##_map_err(r, fn)
 *   - Chaining: result_##value_type##_##error_type##_and_then(r, fn)
 *               result_##value_type##_##error_type##_or_else(r, fn)
 *   - Conversion: result_##value_type##_##error_type##_ok_or(r, default_err)
 *
 * Type name convention:
 *   For pointer types, use a typedef first:
 *     typedef const char* constcharptr;
 *     CANON_C_DEFINE_RESULT(int, constcharptr)
 *   This produces: result_int_constcharptr
 *
 * Usage:
 *   CANON_C_DEFINE_RESULT(int, int)           // result_int_int
 *   CANON_C_DEFINE_RESULT(float, ErrorCode)   // result_float_ErrorCode
 *
 * Example:
 *   result_int_int divide(int a, int b) {
 *       if (b == 0) return result_int_int_err(-1);
 *       return result_int_int_ok(a / b);
 *   }
 *
 * Note: This must be used at file or global scope, not inside functions.
 *       Use once per (T,E) pair in a header or source file.
 */
#define CANON_C_DEFINE_RESULT(value_type, error_type) \
\
/** \
 * @brief Result type for (value_type, error_type) - represents success or failure \
 * \
 * Fields: \
 *   - is_ok: true if Ok(value), false if Err(error) \
 *   - union { ok, err }: only one is valid (determined by is_ok) \
 * \
 * Do not access fields directly - use the provided functions. \
 * Accessing the wrong union member is undefined behavior. \
 */ \
typedef struct { \
    bool is_ok; \
    union { \
        value_type ok; \
        error_type err; \
    }; \
} result_##value_type##_##error_type; \
\
/** \
 * @brief Constructs Ok(value) - a successful result \
 * \
 * @param v Value to wrap \
 * @return Result containing the success value \
 * \
 * Example: result_int_int r = result_int_int_ok(42); \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_ok(value_type v) { \
    return (result_##value_type##_##error_type){ .is_ok = true, .ok = v }; \
} \
\
/** \
 * @brief Constructs Err(error) - a failed result \
 * \
 * @param e Error value to wrap \
 * @return Result containing the error \
 * \
 * Example: result_int_int r = result_int_int_err(-1); \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_err(error_type e) { \
    return (result_##value_type##_##error_type){ .is_ok = false, .err = e }; \
} \
\
/** \
 * @brief Checks if Result is Ok \
 * \
 * @param r Result to check \
 * @return true if Ok(value), false if Err(error) \
 * \
 * Example: if (result_int_int_is_ok(r)) { ... } \
 */ \
static inline bool \
result_##value_type##_##error_type##_is_ok(result_##value_type##_##error_type r) { \
    return r.is_ok; \
} \
\
/** \
 * @brief Checks if Result is Err \
 * \
 * @param r Result to check \
 * @return true if Err(error), false if Ok(value) \
 * \
 * Example: if (result_int_int_is_err(r)) { handle_error(); } \
 */ \
static inline bool \
result_##value_type##_##error_type##_is_err(result_##value_type##_##error_type r) { \
    return !r.is_ok; \
} \
\
/** \
 * @brief Safely extracts success value if Ok \
 * \
 * This is the recommended way to extract values in production code. \
 * \
 * @param r   Result to extract from \
 * @param out Pointer to store the value (NULL-safe) \
 * @return    true if value was extracted, false if Err or out is NULL \
 * \
 * Example: \
 *   int value; \
 *   if (result_int_int_get_ok(r, &value)) { \
 *       printf("Success: %d\n", value); \
 *   } \
 */ \
static inline bool \
result_##value_type##_##error_type##_get_ok( \
    result_##value_type##_##error_type r, value_type* out) { \
    if (r.is_ok && out) { \
        *out = r.ok; \
        return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Safely extracts error value if Err \
 * \
 * @param r   Result to extract from \
 * @param out Pointer to store the error (NULL-safe) \
 * @return    true if error was extracted, false if Ok or out is NULL \
 * \
 * Example: \
 *   int err_code; \
 *   if (result_int_int_get_err(r, &err_code)) { \
 *       fprintf(stderr, "Error code: %d\n", err_code); \
 *   } \
 */ \
static inline bool \
result_##value_type##_##error_type##_get_err( \
    result_##value_type##_##error_type r, error_type* out) { \
    if (!r.is_ok && out) { \
        *out = r.err; \
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
 * @param r        Result to extract from \
 * @param fallback Value to return if Err \
 * @return         Success value if Ok, fallback if Err \
 * \
 * Example: int n = result_int_int_unwrap_or(r, 0); \
 */ \
static inline value_type \
result_##value_type##_##error_type##_unwrap_or( \
    result_##value_type##_##error_type r, value_type fallback) { \
    return r.is_ok ? r.ok : fallback; \
} \
\
/** \
 * @brief Extracts success value, panicking if Err \
 * \
 * ⚠️  WARNING: Only use when you are certain the result is Ok! \
 * In debug builds, this asserts. In release builds, undefined if Err. \
 * \
 * Prefer: get_ok(), unwrap_or(), expect() over unwrap() in production. \
 * \
 * @param r Result to unwrap \
 * @return  Success value \
 * \
 * Panics: If r is Err (assertion failure in debug builds) \
 * \
 * Example: int n = result_int_int_unwrap(r);  // Only if certain r is Ok \
 */ \
static inline value_type \
result_##value_type##_##error_type##_unwrap(result_##value_type##_##error_type r) { \
    assert(r.is_ok && "result_unwrap called on Err value"); \
    return r.ok; \
} \
\
/** \
 * @brief Extracts error value, panicking if Ok \
 * \
 * The opposite of unwrap() - extracts error from Err. \
 * Useful for testing or cases where you expect failure. \
 * \
 * @param r Result to unwrap \
 * @return  Error value \
 * \
 * Panics: If r is Ok (assertion failure in debug builds) \
 * \
 * Example: int err = result_int_int_unwrap_err(r);  // Only if certain r is Err \
 */ \
static inline error_type \
result_##value_type##_##error_type##_unwrap_err(result_##value_type##_##error_type r) { \
    assert(!r.is_ok && "result_unwrap_err called on Ok value"); \
    return r.err; \
} \
\
/** \
 * @brief Extracts value with custom panic message \
 * \
 * Like unwrap(), but with a descriptive error message for debugging. \
 * Use this for invariant violations that should never happen. \
 * \
 * @param r   Result to extract from \
 * @param msg Error message if Err (used in assertion) \
 * @return    Success value \
 * \
 * Panics: If r is Err (assertion failure in debug builds with msg) \
 * \
 * Example: \
 *   int n = result_int_int_expect(r, "division result must be valid"); \
 */ \
static inline value_type \
result_##value_type##_##error_type##_expect( \
    result_##value_type##_##error_type r, const char* msg) { \
    assert(r.is_ok && msg); \
    (void)msg; /* suppress unused warning in release builds */ \
    return r.ok; \
} \
\
/** \
 * @brief Transforms the success value if Ok \
 * \
 * Applies function f to the success value, wrapping result in Result. \
 * If Err, returns Err unchanged without calling f. \
 * \
 * @param r Result to transform \
 * @param f Transformation function \
 * @return  Ok(f(value)) if r is Ok(value), r unchanged if Err \
 * \
 * Example: \
 *   int double_it(int x) { return x * 2; } \
 *   result_int_int doubled = result_int_int_map(r, double_it); \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_map( \
    result_##value_type##_##error_type r, value_type (*f)(value_type)) { \
    return r.is_ok ? result_##value_type##_##error_type##_ok(f(r.ok)) : r; \
} \
\
/** \
 * @brief Transforms the error value if Err \
 * \
 * Applies function f to the error value, wrapping result in Result. \
 * If Ok, returns Ok unchanged without calling f. \
 * \
 * Useful for converting error types or adding context. \
 * \
 * @param r Result to transform \
 * @param f Error transformation function \
 * @return  r unchanged if Ok, Err(f(error)) if Err \
 * \
 * Example: \
 *   int add_context(int err) { return err + 1000; } \
 *   result_int_int contextualized = result_int_int_map_err(r, add_context); \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_map_err( \
    result_##value_type##_##error_type r, error_type (*f)(error_type)) { \
    return r.is_ok ? r : result_##value_type##_##error_type##_err(f(r.err)); \
} \
\
/** \
 * @brief Chains Result-returning operations \
 * \
 * Like map(), but f returns a Result instead of a plain value. \
 * Prevents nested Result<Result<T,E>,E>. \
 * \
 * @param r Result to chain from \
 * @param f Function returning Result \
 * @return  f(value) if r is Ok(value), r unchanged if Err \
 * \
 * Example: \
 *   result_int_int safe_divide(int a, int b) { \
 *       if (b == 0) return result_int_int_err(-1); \
 *       return result_int_int_ok(a / b); \
 *   } \
 *   result_int_int result = result_int_int_and_then(r, safe_divide); \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_and_then( \
    result_##value_type##_##error_type r, \
    result_##value_type##_##error_type (*f)(value_type)) { \
    return r.is_ok ? f(r.ok) : r; \
} \
\
/** \
 * @brief Provides alternative if Err \
 * \
 * Returns r if Ok, otherwise calls fallback(error) to get alternative. \
 * Useful for error recovery or fallback strategies. \
 * \
 * @param r        Result to check \
 * @param fallback Function providing alternative Result from error \
 * @return         r if Ok, fallback(error) if Err \
 * \
 * Example: \
 *   result_int_int retry(int err_code) { \
 *       return result_int_int_ok(0);  // Fallback to default \
 *   } \
 *   result_int_int result = result_int_int_or_else(r, retry); \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_or_else( \
    result_##value_type##_##error_type r, \
    result_##value_type##_##error_type (*f)(error_type)) { \
    return r.is_ok ? r : f(r.err); \
} \
\
/** \
 * @brief Checks equality of two Results \
 * \
 * Two Results are equal if: \
 * - Both are Ok and their values compare equal via eq_ok, OR \
 * - Both are Err and their errors compare equal via eq_err \
 * \
 * @param r1     First Result \
 * @param r2     Second Result \
 * @param eq_ok  Equality comparison function for success values \
 * @param eq_err Equality comparison function for error values \
 * @return       true if Results are equal, false otherwise \
 * \
 * Example: \
 *   bool int_eq(int a, int b) { return a == b; } \
 *   if (result_int_int_eq(r1, r2, int_eq, int_eq)) { ... } \
 */ \
static inline bool \
result_##value_type##_##error_type##_eq( \
    result_##value_type##_##error_type r1, \
    result_##value_type##_##error_type r2, \
    bool (*eq_ok)(value_type, value_type), \
    bool (*eq_err)(error_type, error_type)) { \
    if (r1.is_ok != r2.is_ok) return false; \
    if (r1.is_ok) return eq_ok(r1.ok, r2.ok); \
    return eq_err(r1.err, r2.err); \
}

/* ────────────────────────────────────────────────────────────────────────────
   Propagation & convenience macros (require GNU C extensions or C23)
   ──────────────────────────────────────────────────────────────────────────── */

#ifndef CANON_NO_GNU_EXTENSIONS

/**
 * @brief Early return on error – the most common pattern
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * If the Result is Err, returns immediately with the error.
 * If Ok, continues execution.
 *
 * @param type_value Type name for value (e.g., int)
 * @param type_error Type name for error (e.g., int)
 * @param expr       Expression evaluating to Result
 *
 * Example:
 *   result_int_int divide(int a, int b) {
 *       if (b == 0) return result_int_int_err(-1);
 *       return result_int_int_ok(a / b);
 *   }
 *
 *   result_int_int process(void) {
 *       TRY(int, int, divide(10, 2));
 *       // Continues only if division succeeded
 *       return result_int_int_ok(0);
 *   }
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
 * If the Result is Err, returns immediately with new_error instead.
 * Useful for error context or type conversion.
 *
 * @param type_value Type name for value
 * @param type_error Type name for error
 * @param expr       Expression evaluating to Result
 * @param new_error  Error value to return instead
 *
 * Example:
 *   TRY_REMAP(int, int, parse_config(), ERR_CONFIG_INVALID);
 */
#define TRY_REMAP(type_value, type_error, expr, new_error) \
    do { \
        result_##type_value##_##type_error _res_ = (expr); \
        if (result_##type_value##_##type_error##_is_err(_res_)) { \
            return result_##type_value##_##type_error##_err(new_error); \
        } \
    } while (0)

/**
 * @brief Unwrap or evaluate to fallback value
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * @param type_value Type name for value
 * @param type_error Type name for error
 * @param expr       Expression evaluating to Result
 * @param fallback   Value to use if Err
 *
 * Example:
 *   int value = UNWRAP_OR(int, int, parse_number(str), -1);
 */
#define UNWRAP_OR(type_value, type_error, expr, fallback) \
    ({ \
        result_##type_value##_##type_error _res_ = (expr); \
        result_##type_value##_##type_error##_is_ok(_res_) ? \
            result_##type_value##_##type_error##_unwrap(_res_) : (fallback); \
    })

/**
 * @brief Extract value from Result, early return with error if Err
 *
 * Requires: GNU C statement expressions or C23
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Combines unwrapping with error propagation. Useful for cleaner code.
 *
 * @param type_value Type name for value
 * @param type_error Type name for error
 * @param expr       Expression evaluating to Result
 *
 * Returns: The unwrapped value (can be used in expressions)
 *
 * Example:
 *   int doubled = TRY_UNWRAP(int, int, parse_number("42")) * 2;
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

/* ────────────────────────────────────────────────────────────────────────────
   Common type instantiations
   ────────────────────────────────────────────────────────────────────────────
   
   Uncomment the types you need, or define your own:
   
   // Integer results with error codes
   CANON_C_DEFINE_RESULT(int, int)
   CANON_C_DEFINE_RESULT(long, int)
   CANON_C_DEFINE_RESULT(size_t, int)
   
   // String results with error messages
   typedef const char* constcharptr;
   CANON_C_DEFINE_RESULT(int, constcharptr)
   CANON_C_DEFINE_RESULT(float, constcharptr)
   
   // Pointer results
   typedef void* voidptr;
   CANON_C_DEFINE_RESULT(voidptr, int)
   
   // Custom error types
   typedef enum { ERR_NONE, ERR_IO, ERR_PARSE } ErrorCode;
   CANON_C_DEFINE_RESULT(int, ErrorCode)
   
   ──────────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Example
   ────────────────────────────────────────────────────────────────────────────

    #include "result.h"
    #include <stdio.h>
    
    // Define error codes
    typedef enum {
        ERR_NONE = 0,
        ERR_INVALID_INPUT,
        ERR_DIVISION_BY_ZERO,
        ERR_OVERFLOW
    } ErrorCode;
    
    // Define Result types we need
    CANON_C_DEFINE_RESULT(int, ErrorCode)
    CANON_C_DEFINE_RESULT(float, ErrorCode)
    
    // Functions that can fail
    result_int_ErrorCode parse_int(const char* str) {
        if (!str) {
            return result_int_ErrorCode_err(ERR_INVALID_INPUT);
        }
        
        char* end;
        long val = strtol(str, &end, 10);
        
        if (end == str || *end != '\0') {
            return result_int_ErrorCode_err(ERR_INVALID_INPUT);
        }
        
        return result_int_ErrorCode_ok((int)val);
    }
    
    result_int_ErrorCode safe_divide(int a, int b) {
        if (b == 0) {
            return result_int_ErrorCode_err(ERR_DIVISION_BY_ZERO);
        }
        return result_int_ErrorCode_ok(a / b);
    }
    
    // Safe usage with explicit error handling
    void example_safe(void) {
        result_int_ErrorCode result = parse_int("42");
        
        // Method 1: Check and extract
        if (result_int_ErrorCode_is_ok(result)) {
            int value = result_int_ErrorCode_unwrap(result);
            printf("Parsed: %d\n", value);
        } else {
            ErrorCode err = result_int_ErrorCode_unwrap_err(result);
            fprintf(stderr, "Error code: %d\n", err);
        }
        
        // Method 2: Use get_ok/get_err
        int value;
        if (result_int_ErrorCode_get_ok(result, &value)) {
            printf("Parsed: %d\n", value);
        } else {
            ErrorCode err;
            result_int_ErrorCode_get_err(result, &err);
            fprintf(stderr, "Error: %d\n", err);
        }
        
        // Method 3: Provide default
        int value = result_int_ErrorCode_unwrap_or(result, 0);
    }
    
    // Error propagation with TRY macro
    result_int_ErrorCode compute(const char* a_str, const char* b_str) {
        TRY(int, ErrorCode, parse_int(a_str));
        int a = result_int_ErrorCode_unwrap(parse_int(a_str));
        
        TRY(int, ErrorCode, parse_int(b_str));
        int b = result_int_ErrorCode_unwrap(parse_int(b_str));
        
        return safe_divide(a, b);
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_C_SEMANTICS_RESULT_H */
