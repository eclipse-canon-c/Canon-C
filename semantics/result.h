// semantics/result.h
#ifndef CANON_C_SEMANTICS_RESULT_H
#define CANON_C_SEMANTICS_RESULT_H

#include <stdbool.h>
#include <assert.h>

/**
 * @file result.h
 * @brief Explicit success or failure with value or error
 *
 * Result<T, E> represents either:
 *   - Ok(value)    : successful computation returning a value of type T
 *   - Err(error)   : failure with an error value of type E
 *
 * Goals:
 *   - Fully explicit error handling (no exceptions, no errno, no sentinel values)
 *   - Zero-cost abstraction (struct with bool + union)
 *   - Type-safe via per-type macros (one struct per value_type + error_type pair)
 *   - Header-only, no runtime dependencies
 *
 * Basic usage example:
 *   result_int_int errcode = result_int_int_ok(42);
 *   if (result_int_int_is_ok(errcode)) {
 *       printf("Success: %d\n", result_int_int_unwrap(errcode));
 *   } else {
 *       printf("Error: %d\n", result_int_int_unwrap_err(errcode));
 *   }
 */

/**
 * @brief Defines a concrete Result type for a given value_type and error_type
 *
 * Generates: struct result_##value_type##_##error_type with is_ok flag and union,
 * plus a full set of constructor/accessor/transformation functions.
 *
 * @param value_type Type of the success value (e.g. int, void*, custom struct)
 * @param error_type Type of the error value (e.g. int, enum ErrorCode, const char*)
 */
#define CANON_C_DEFINE_RESULT(value_type, error_type) \
typedef struct { \
    bool is_ok; \
    union { \
        value_type ok; \
        error_type err; \
    }; \
} result_##value_type##_##error_type; \
\
/** \
 * @brief Constructs successful result (Ok) \
 * @param v The success value \
 * @return result_##value_type##_##error_type in Ok state \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_ok(value_type v) \
{ \
    result_##value_type##_##error_type r = {0}; \
    r.is_ok = true; \
    r.ok = v; \
    return r; \
} \
\
/** \
 * @brief Constructs failed result (Err) \
 * @param e The error value \
 * @return result_##value_type##_##error_type in Err state \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_err(error_type e) \
{ \
    result_##value_type##_##error_type r = {0}; \
    r.is_ok = false; \
    r.err = e; \
    return r; \
} \
\
/** \
 * @brief Checks if the result is successful \
 * @param r Result instance \
 * @return true if Ok, false if Err \
 */ \
static inline bool \
result_##value_type##_##error_type##_is_ok(result_##value_type##_##error_type r) \
{ \
    return r.is_ok; \
} \
\
/** \
 * @brief Checks if the result contains an error \
 * @param r Result instance \
 * @return true if Err, false if Ok \
 */ \
static inline bool \
result_##value_type##_##error_type##_is_err(result_##value_type##_##error_type r) \
{ \
    return !r.is_ok; \
} \
\
/** \
 * @brief Safely extracts Ok value if present \
 * @param r   Result instance \
 * @param out Pointer to receive the value (may be NULL) \
 * @return    true if value was written (Ok), false otherwise (Err) \
 */ \
static inline bool \
result_##value_type##_##error_type##_get_ok( \
    result_##value_type##_##error_type r, \
    value_type *out \
) { \
    if (r.is_ok && out) { \
        *out = r.ok; \
        return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Safely extracts Err value if present \
 * @param r   Result instance \
 * @param out Pointer to receive the error (may be NULL) \
 * @return    true if error was written (Err), false otherwise (Ok) \
 */ \
static inline bool \
result_##value_type##_##error_type##_get_err( \
    result_##value_type##_##error_type r, \
    error_type *out \
) { \
    if (!r.is_ok && out) { \
        *out = r.err; \
        return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Returns Ok value or fallback if Err \
 * @param r        Result instance \
 * @param fallback Value to return when Err \
 * @return         Ok value or fallback \
 */ \
static inline value_type \
result_##value_type##_##error_type##_unwrap_or( \
    result_##value_type##_##error_type r, \
    value_type fallback \
) { \
    return r.is_ok ? r.ok : fallback; \
} \
\
/** \
 * @brief Extracts Ok value or panics if Err (debug only) \
 * @param r Result instance \
 * @return  Ok value (asserts if Err) \
 */ \
static inline value_type \
result_##value_type##_##error_type##_unwrap(result_##value_type##_##error_type r) \
{ \
    assert(r.is_ok && "Called unwrap on Err"); \
    return r.ok; \
} \
\
/** \
 * @brief Extracts Ok value or panics with custom message if Err \
 * @param r   Result instance \
 * @param msg Custom panic message \
 * @return    Ok value \
 */ \
static inline value_type \
result_##value_type##_##error_type##_expect( \
    result_##value_type##_##error_type r, \
    const char *msg \
) { \
    assert(r.is_ok && msg); \
    (void)msg; /* avoid unused warning in release builds */ \
    return r.ok; \
} \
\
/** \
 * @brief Applies transformation to Ok value; passes Err unchanged \
 * @param r  Result instance \
 * @param f  Function: value_type → value_type \
 * @return   New Result with mapped Ok or original Err \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_map( \
    result_##value_type##_##error_type r, \
    value_type (*f)(value_type) \
) { \
    return r.is_ok ? result_##value_type##_##error_type##_ok(f(r.ok)) : r; \
} \
\
/** \
 * @brief Applies transformation to Err value; passes Ok unchanged \
 * @param r  Result instance \
 * @param f  Function: error_type → error_type \
 * @return   New Result with mapped Err or original Ok \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_map_err( \
    result_##value_type##_##error_type r, \
    error_type (*f)(error_type) \
) { \
    return r.is_ok ? r : result_##value_type##_##error_type##_err(f(r.err)); \
} \
\
/** \
 * @brief Chains computations that return Result (flat_map / and_then) \
 * @param r  Result instance \
 * @param f  Function: value_type → result_##value_type##_##error_type \
 * @return   Result of f if Ok, original Err otherwise \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_and_then( \
    result_##value_type##_##error_type r, \
    result_##value_type##_##error_type (*f)(value_type) \
) { \
    return r.is_ok ? f(r.ok) : r; \
} \
\
/** \
 * @brief Provides alternative Result if current is Err \
 * @param r       Result instance \
 * @param fallback Function taking error_type and returning result_... \
 * @return        Current if Ok, result of fallback() if Err \
 */ \
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_or_else( \
    result_##value_type##_##error_type r, \
    result_##value_type##_##error_type (*fallback)(error_type) \
) { \
    return r.is_ok ? r : fallback(r.err); \
}

#endif /* CANON_C_SEMANTICS_RESULT_H */
