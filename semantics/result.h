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
 * Recommended pattern:
 *   Always check is_ok / is_err before unwrap
 *   Use TRY / TRY_REMAP macros for clean propagation
 *   Prefer get_ok / get_err or unwrap_or over raw unwrap in production
 *
 * Classic example:
 *   result_int_constcharp parse_number(const char* s) { ... }
 *
 *   int process() {
 *       TRY(int n = parse_number("42"));           // early return on error
 *       return n * 2;
 *   }
 */

/**
 * @brief Defines a concrete Result<T, E> type for given value_type and error_type
 *
 * Generates:
 *   - struct result_##value_type##_##error_type
 *   - Constructors: ok(...), err(...)
 *   - Queries: is_ok, is_err
 *   - Accessors: get_ok, get_err, unwrap, unwrap_or, expect
 *   - Transformations: map, map_err, and_then, or_else
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
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_ok(value_type v) { \
    return (result_##value_type##_##error_type){ .is_ok = true, .ok = v }; \
} \
\
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_err(error_type e) { \
    return (result_##value_type##_##error_type){ .is_ok = false, .err = e }; \
} \
\
static inline bool \
result_##value_type##_##error_type##_is_ok(result_##value_type##_##error_type r) { \
    return r.is_ok; \
} \
\
static inline bool \
result_##value_type##_##error_type##_is_err(result_##value_type##_##error_type r) { \
    return !r.is_ok; \
} \
\
static inline bool \
result_##value_type##_##error_type##_get_ok( \
    result_##value_type##_##error_type r, value_type* out) { \
    if (r.is_ok && out) { *out = r.ok; return true; } \
    return false; \
} \
\
static inline bool \
result_##value_type##_##error_type##_get_err( \
    result_##value_type##_##error_type r, error_type* out) { \
    if (!r.is_ok && out) { *out = r.err; return true; } \
    return false; \
} \
\
static inline value_type \
result_##value_type##_##error_type##_unwrap_or( \
    result_##value_type##_##error_type r, value_type fallback) { \
    return r.is_ok ? r.ok : fallback; \
} \
\
static inline value_type \
result_##value_type##_##error_type##_unwrap(result_##value_type##_##error_type r) { \
    assert(r.is_ok && "unwrap called on Err value"); \
    return r.ok; \
} \
\
static inline value_type \
result_##value_type##_##error_type##_expect( \
    result_##value_type##_##error_type r, const char* msg) { \
    assert(r.is_ok && msg); \
    (void)msg; \
    return r.ok; \
} \
\
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_map( \
    result_##value_type##_##error_type r, value_type (*f)(value_type)) { \
    return r.is_ok ? result_##value_type##_##error_type##_ok(f(r.ok)) : r; \
} \
\
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_map_err( \
    result_##value_type##_##error_type r, error_type (*f)(error_type)) { \
    return r.is_ok ? r : result_##value_type##_##error_type##_err(f(r.err)); \
} \
\
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_and_then( \
    result_##value_type##_##error_type r, \
    result_##value_type##_##error_type (*f)(value_type)) { \
    return r.is_ok ? f(r.ok) : r; \
} \
\
static inline result_##value_type##_##error_type \
result_##value_type##_##error_type##_or_else( \
    result_##value_type##_##error_type r, \
    result_##value_type##_##error_type (*f)(error_type)) { \
    return r.is_ok ? r : f(r.err); \
}

/* ────────────────────────────────────────────────────────────────────────────
   Popular propagation & convenience macros
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Early return on error – the most common pattern
 * @example:
 *     TRY(open_file(path));
 *     // only executes on success
 */
#define TRY(expr) \
    do { \
        auto _res_ = (expr); \
        if (result_is_err(_res_)) return _res_; \
    } while (0)

/**
 * @brief Early return with custom/remapped error
 * @example:
 *     TRY_REMAP(parse_config(), ERR_CONFIG_INVALID);
 */
#define TRY_REMAP(expr, new_error) \
    do { \
        auto _res_ = (expr); \
        if (result_is_err(_res_)) return result_err(new_error); \
    } while (0)

/**
 * @brief Unwrap or return fallback value
 * @example:
 *     int value = UNWRAP_OR(parse_number(str), -1);
 */
#define UNWRAP_OR(expr, fallback) \
    ({ \
        auto _res_ = (expr); \
        result_is_ok(_res_) ? result_unwrap(_res_) : (fallback); \
    })

/**
 * @brief Optional-style: if none → early return with error
 * Useful when mixing Result + Option
 */
#define TRY_SOME(opt_expr, err_code) \
    ({ \
        auto _opt_ = (opt_expr); \
        if (option_is_none(_opt_)) return result_err(err_code); \
        option_unwrap(_opt_); \
    })

#endif /* CANON_C_SEMANTICS_RESULT_H */
