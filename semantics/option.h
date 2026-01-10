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
 * - None       → value is absent (safe alternative to NULL/sentinels)
 *
 * Philosophy & goals:
 *   - Fully explicit nullability (no accidental dereference of NULL)
 *   - Zero-cost abstraction (bool + value, natural alignment/padding)
 *   - Type-safe via macros (one concrete struct per type)
 *   - Header-only, no runtime dependencies
 *   - Chainable & composable (map, and_then, or_else, filter...)
 *   - Panic/unwrap only in debug builds (production code should check)
 *
 * Recommended patterns:
 *   Always check is_some / is_none before unwrap
 *   Use TRY_SOME for clean propagation
 *   Prefer unwrap_or / get / filter over raw unwrap in production code
 *
 * Classic example:
 *   option_int maybe_parse(const char* s) { ... }
 *
 *   int process() {
 *       TRY_SOME(int n, maybe_parse("42"), ERR_PARSE_FAILED);
 *       return n * 2;
 *   }
 */

/**
 * @brief Defines a concrete Option<T> type for given value type
 *
 * Generates:
 *   - struct option_##type
 *   - Constructors: some(...), none()
 *   - Queries: is_some, is_none
 *   - Accessors: get, unwrap, unwrap_or, expect
 *   - Transformations: map, and_then, or_else, filter, zip...
 */
#define CANON_C_DEFINE_OPTION(type) \
typedef struct { \
    bool has_value; \
    type value; \
} option_##type; \
\
static inline option_##type option_##type##_some(type v) { \
    return (option_##type){ .has_value = true, .value = v }; \
} \
\
static inline option_##type option_##type##_none(void) { \
    return (option_##type){ .has_value = false }; \
} \
\
static inline bool option_##type##_is_some(option_##type o) { \
    return o.has_value; \
} \
\
static inline bool option_##type##_is_none(option_##type o) { \
    return !o.has_value; \
} \
\
static inline bool option_##type##_get(option_##type o, type* out) { \
    if (o.has_value && out) { \
        *out = o.value; \
        return true; \
    } \
    return false; \
} \
\
static inline type option_##type##_unwrap_or( \
    option_##type o, type fallback) { \
    return o.has_value ? o.value : fallback; \
} \
\
static inline type option_##type##_unwrap(option_##type o) { \
    assert(o.has_value && "unwrap called on None"); \
    return o.value; \
} \
\
static inline type option_##type##_expect( \
    option_##type o, const char* msg) { \
    assert(o.has_value && msg); \
    (void)msg; \
    return o.value; \
} \
\
static inline option_##type option_##type##_map( \
    option_##type o, type (*f)(type)) { \
    return o.has_value ? option_##type##_some(f(o.value)) : option_##type##_none(); \
} \
\
static inline option_##type option_##type##_and_then( \
    option_##type o, option_##type (*f)(type)) { \
    return o.has_value ? f(o.value) : option_##type##_none(); \
} \
\
static inline option_##type option_##type##_or_else( \
    option_##type o, option_##type (*fallback)(void)) { \
    return o.has_value ? o : fallback(); \
} \
\
/* ────────────────────────────────────────────────────────────────────────────
   Additional popular combinators
   ──────────────────────────────────────────────────────────────────────────── */ \
\
/** \
 * @brief Keeps value only if predicate returns true, otherwise None \
 */ \
static inline option_##type option_##type##_filter( \
    option_##type o, bool (*pred)(type)) { \
    return (o.has_value && pred(o.value)) ? o : option_##type##_none(); \
} \
\
/** \
 * @brief Pairs two Options: Some(a,b) if both Some, None otherwise \
 */ \
static inline option_##type option_##type##_zip( \
    option_##type o1, option_##type o2, type (*combine)(type, type)) { \
    if (o1.has_value && o2.has_value) { \
        return option_##type##_some(combine(o1.value, o2.value)); \
    } \
    return option_##type##_none(); \
}

/* ────────────────────────────────────────────────────────────────────────────
   Popular propagation & convenience macros
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Early return with error if Option is None
 * @example:
 *     TRY_SOME(int n, maybe_value(), ERR_NO_VALUE);
 */
#define TRY_SOME(opt_expr, err_code) \
    ({ \
        auto _opt_ = (opt_expr); \
        if (option_is_none(_opt_)) return result_err(err_code); \
        option_unwrap(_opt_); \
    })

/**
 * @brief Unwrap or return default value
 * @example:
 *     int value = UNWRAP_OR_DEFAULT(maybe_num, 0);
 */
#define UNWRAP_OR_DEFAULT(opt_expr, default_val) \
    ({ \
        auto _opt_ = (opt_expr); \
        option_is_some(_opt_) ? option_unwrap(_opt_) : (default_val); \
    })

#endif /* CANON_C_SEMANTICS_OPTION_H */
