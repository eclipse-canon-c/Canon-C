// semantics/option.h
#ifndef CANON_C_SEMANTICS_OPTION_H
#define CANON_C_SEMANTICS_OPTION_H

#include <stdbool.h>
#include <assert.h>

/**
 * @file option.h
 * @brief Explicit presence or absence of a value (Some / None pattern)
 *
 * Provides a safe, explicit alternative to NULL pointers or sentinel values.
 * Each concrete type gets its own struct (e.g. option_int, option_string_view).
 *
 * Core idea:
 *   - Zero-cost abstraction: just bool + value (natural alignment)
 *   - No hidden states, no undefined behavior on misuse
 *   - Header-only, fully type-safe via per-type macros
 *
 * Basic usage example:
 *   option_int maybe_num = option_int_some(42);
 *   if (option_int_is_some(maybe_num)) {
 *       printf("Got: %d\n", option_int_unwrap(maybe_num));
 *   } else {
 *       printf("No value\n");
 *   }
 */

/**
 * @brief Defines a concrete Option type for a given value type
 *
 * Generates: struct option_##type with has_value flag and value field,
 * plus a full set of constructor/accessor/transform functions.
 *
 * @param type The value type (e.g. int, float*, const char*, custom struct)
 */
#define CANON_C_DEFINE_OPTION(type) \
typedef struct { \
    bool has_value; \
    type value; \
} option_##type; \
\
/** \
 * @brief Constructs Some(value) — Option containing a value \
 * @param v The value to wrap \
 * @return option_##type in Some state \
 */ \
static inline option_##type option_##type##_some(type v) { \
    option_##type o; \
    o.has_value = true; \
    o.value = v; \
    return o; \
} \
\
/** \
 * @brief Constructs None — empty Option (zero-initialized) \
 * @return option_##type in None state (safe for static storage) \
 */ \
static inline option_##type option_##type##_none(void) { \
    option_##type o = {0}; \
    return o; \
} \
\
/** \
 * @brief Checks if the Option contains a value \
 * @param o Option instance \
 * @return true if Some, false if None \
 */ \
static inline bool option_##type##_is_some(option_##type o) { \
    return o.has_value; \
} \
\
/** \
 * @brief Checks if the Option is empty \
 * @param o Option instance \
 * @return true if None, false if Some \
 */ \
static inline bool option_##type##_is_none(option_##type o) { \
    return !o.has_value; \
} \
\
/** \
 * @brief Safely extracts value if present \
 * @param o   Option instance \
 * @param out Pointer to receive the value (may be NULL) \
 * @return    true if value was written (Some), false otherwise (None) \
 */ \
static inline bool option_##type##_get(option_##type o, type *out) { \
    if (o.has_value && out) { \
        *out = o.value; \
        return true; \
    } \
    return false; \
} \
\
/** \
 * @brief Returns contained value or fallback if None \
 * @param o        Option instance \
 * @param fallback Value to return when None \
 * @return         Contained value or fallback \
 */ \
static inline type option_##type##_unwrap_or(option_##type o, type fallback) { \
    return o.has_value ? o.value : fallback; \
} \
\
/** \
 * @brief Extracts value or panics if None (debug only) \
 * @param o Option instance \
 * @return  Contained value (asserts if None) \
 */ \
static inline type option_##type##_unwrap(option_##type o) { \
    assert(o.has_value && "Called unwrap on None"); \
    return o.value; \
} \
\
/** \
 * @brief Extracts value or panics with custom message if None \
 * @param o   Option instance \
 * @param msg Custom panic message \
 * @return    Contained value \
 */ \
static inline type option_##type##_expect(option_##type o, const char *msg) { \
    assert(o.has_value && msg); \
    (void)msg; /* avoid unused warning in release builds */ \
    return o.value; \
} \
\
/** \
 * @brief Applies a transformation function if Some, returns None otherwise \
 * @param o  Option instance \
 * @param f  Function: type → type \
 * @return   New Option with transformed value (or None) \
 */ \
static inline option_##type option_##type##_map( \
    option_##type o, \
    type (*f)(type) \
) { \
    return o.has_value ? option_##type##_some(f(o.value)) : option_##type##_none(); \
} \
\
/** \
 * @brief Chains computations that themselves return Option (flat_map / and_then) \
 * @param o  Option instance \
 * @param f  Function: type → option_##type \
 * @return   Result of f if Some, None otherwise \
 */ \
static inline option_##type option_##type##_and_then( \
    option_##type o, \
    option_##type (*f)(type) \
) { \
    return o.has_value ? f(o.value) : option_##type##_none(); \
} \
\
/** \
 * @brief Provides alternative Option if current is None \
 * @param o       Option instance \
 * @param fallback Function returning option_##type (called only if None) \
 * @return        Current if Some, result of fallback() if None \
 */ \
static inline option_##type option_##type##_or_else( \
    option_##type o, \
    option_##type (*fallback)(void) \
) { \
    return o.has_value ? o : fallback(); \
}

#endif /* CANON_C_SEMANTICS_OPTION_H */
