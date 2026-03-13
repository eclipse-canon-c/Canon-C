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
 * - None        → value is absent (safe alternative to NULL/sentinels)
 *
 * Philosophy & goals:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fully explicit nullability (no accidental dereference of NULL)
 * - Zero-cost abstraction (bool + value, natural alignment/padding)
 * - Type-safe via macros (one concrete struct per type)
 * - Header-only by default, supports separate compilation
 * - Chainable & composable (map, and_then, or_else, filter...)
 * - Panic/unwrap only in debug builds (see "Panic behaviour" below)
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
 * Each Option instance is independent – no shared state.
 * The caller is responsible for synchronising access to a shared instance.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) — constant time for all operations except combinators
 * - Space complexity: O(1) — bool + value + alignment padding only
 * - No dynamic allocations or runtime overhead
 * - Zero-cost abstraction (compiles to simple struct operations)
 * - Inlined by default (static inline)
 *
 * Code-size note:
 * ────────────────────────────────────────────────────────────────────────────
 * Each CANON_OPTION(T) expands to ~1-2 KB of static inline functions *per
 * translation unit* that includes the instantiation.  If many TUs instantiate
 * the same type the linker cannot merge static copies.  To avoid bloat in
 * large projects either:
 *   (a) place CANON_OPTION(T) in exactly one .c file and expose only the
 *       struct typedef and extern declarations via a shared header, or
 *   (b) accept the per-TU duplication (safe but larger binary).
 *
 * Memory layout:
 * ────────────────────────────────────────────────────────────────────────────
 * - Size: sizeof(bool) + sizeof(T) + alignment padding
 * - Typical: 2–16 bytes depending on type and alignment
 * - Stack-allocated; no heap involvement
 *
 * Panic behaviour (platform-specific):
 * ────────────────────────────────────────────────────────────────────────────
 * Functions marked "or panic" (unwrap, expect) call CANON_PANIC() on failure.
 * The default implementation of CANON_PANIC() is:
 *
 *   #ifndef NDEBUG   →  assert(0 && message)   (debug builds)
 *   #ifdef  NDEBUG   →  abort()                (release builds)
 *
 * You can override this for your platform before including this header:
 *
 *   // Example: log then reset for embedded target
 *   #define CANON_PANIC(msg) do { log_fatal(msg); system_reset(); } while(0)
 *   #include "option.h"
 *
 * For safety-certified builds (DO-178C, ISO 26262, IEC 62304) you MUST
 * provide a CANON_PANIC() that satisfies your platform's failure-handling
 * requirements.  The default abort()/assert(0) is intentionally simple; do
 * not rely on it in production without review.
 *
 * Combinator function signatures (explicit):
 * ────────────────────────────────────────────────────────────────────────────
 * The following table shows exactly what signature `fn` must have for each
 * combinator on option_T:
 *
 *   option_T_map(o, fn)        fn : T  → T          returns option_T
 *   option_T_and_then(o, fn)   fn : T  → option_T   returns option_T
 *   option_T_or_else(o, fn)    fn : void → option_T returns option_T
 *   option_T_filter(o, pred)   pred: T  → bool      returns option_T
 *   option_T_zip(o1, o2, fn)   fn : T, T → T        returns option_T
 *
 * "Strict" mutation semantics (replace / take):
 * ────────────────────────────────────────────────────────────────────────────
 * option_T_replace and option_T_take are labelled "strict" meaning they
 * operate unconditionally and do NOT panic on None:
 *
 *   option_T_replace(&o, val)
 *     – Stores val into o regardless of o's current state.
 *     – Returns the *old* option (Some or None) to the caller.
 *     – After the call, o is always Some(val).
 *
 *   option_T_take(&o)
 *     – Copies o into a new option and resets o to None.
 *     – Returns the *old* option (Some or None) to the caller.
 *     – After the call, o is always None.
 *     – Does NOT panic if o was already None.
 *
 * Recommended patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * ✓ Always check is_some / is_none before unwrap
 * ✓ Use TRY_SOME for clean propagation (if using Result types)
 * ✓ Prefer unwrap_or / get / filter over raw unwrap in production code
 * ✓ Use expect() with descriptive messages for invariant violations
 * ✓ Combine with Result<T,E> for comprehensive error handling
 * ✓ Override CANON_PANIC() before including this header in certified builds
 *
 * Anti-patterns to avoid:
 * ────────────────────────────────────────────────────────────────────────────
 * ✗ Don't unwrap() without checking in production code
 * ✗ Don't use Option for error reporting (use Result<T,E> instead)
 * ✗ Don't use Option<bool> — Option<bool> has three logical states
 *   (None, Some(false), Some(true)) which is semantically ambiguous.
 *   Use an explicit three-value enum or Result<bool,E> instead, so that
 *   every state is self-documenting at the call site.
 *
 * Architecture:
 * ────────────────────────────────────────────────────────────────────────────
 * This header is the user-facing facade that provides a simple API.
 * Internally it uses a modular architecture:
 * - option_impl.h:   Pure implementation logic
 * - option_mangle.h: Name-mangling conventions (customisable)
 * - option_decl.h:   Declaration macros (for separate compilation)
 * - option_defn.h:   Definition macros (generates implementations)
 * - option.h:        This file (simple user API)
 *
 * Advanced users can include the individual files for customisation.
 * Most users should just use CANON_OPTION(type) from this file.
 *
 * @sa result.h, error.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   PANIC HOOK (OVERRIDE BEFORE INCLUDING THIS HEADER)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Failure handler invoked by unwrap() and expect() on None.
 *
 * Default behaviour:
 *   - Debug builds (NDEBUG not defined): assert(0 && msg)
 *   - Release builds (NDEBUG defined):   abort()
 *
 * Override before including this header to customise for your platform.
 * The macro receives a string literal message and must not return.
 *
 * Example (embedded reset):
 *   #define CANON_PANIC(msg) do { log_fatal(msg); system_reset(); } while(0)
 *   #include "option.h"
 */
#ifndef CANON_PANIC
#  ifndef NDEBUG
#    include <assert.h>
#    define CANON_PANIC(msg) assert(0 && (msg))
#  else
#    include <stdlib.h>
#    define CANON_PANIC(msg) abort()
#  endif
#endif

/* ════════════════════════════════════════════════════════════════════════════
   SIMPLE USER API – CEREMONY-FREE INTERFACE
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Defines a complete Option<T> type for the given value type.
 *
 * This is the main macro users should call.  It generates a complete Option
 * implementation for the specified type, including the struct definition and
 * all associated functions.
 *
 * Generated type name: option_T   (e.g. option_int, option_float, option_Point)
 *
 * ── Constructors ────────────────────────────────────────────────────────────
 *   option_T_some(value)
 *     Returns Some(value).  value is copied into the struct.
 *
 *   option_T_none()
 *     Returns None.
 *
 * ── Queries ─────────────────────────────────────────────────────────────────
 *   option_T_is_some(o)   → bool   true if o contains a value
 *   option_T_is_none(o)   → bool   true if o is empty
 *
 * ── Safe extraction ─────────────────────────────────────────────────────────
 *   option_T_get(o, &out) → bool
 *     Writes the value into *out if Some; leaves *out unchanged if None.
 *     out may be NULL (query-only mode).  Returns true iff Some.
 *
 *   option_T_unwrap_or(o, def) → T
 *     Returns the contained value if Some, otherwise returns def.
 *     def is evaluated regardless of o's state (eager).
 *
 * ── Unsafe extraction (check first or use expect) ───────────────────────────
 *   option_T_unwrap(o) → T
 *     Returns the contained value.
 *     Calls CANON_PANIC() if None.  Do not use in production without a prior
 *     is_some check or a proof that None cannot occur.
 *
 *   option_T_expect(o, msg) → T
 *     Like unwrap but calls CANON_PANIC(msg) with a descriptive message.
 *     Prefer this over unwrap for invariant assertions.
 *
 * ── Combinators ─────────────────────────────────────────────────────────────
 * All combinators treat None as a no-op and return None unchanged.
 *
 *   option_T_map(o, fn) → option_T
 *     fn signature: T → T
 *     Applies fn to the contained value and wraps the result in Some.
 *     If None, returns None without calling fn.
 *
 *   option_T_and_then(o, fn) → option_T
 *     fn signature: T → option_T
 *     Like map, but fn itself returns an Option (flatMap / monadic bind).
 *     Use when the transformation can also fail.
 *     If None, returns None without calling fn.
 *
 *   option_T_or_else(o, fn) → option_T
 *     fn signature: void → option_T
 *     If Some, returns o unchanged.
 *     If None, calls fn() and returns its result (lazy alternative).
 *
 *   option_T_filter(o, pred) → option_T
 *     pred signature: T → bool
 *     If Some and pred(value) is true, returns o unchanged.
 *     If Some and pred(value) is false, returns None.
 *     If None, returns None without calling pred.
 *
 *   option_T_zip(o1, o2, fn) → option_T
 *     fn signature: T, T → T
 *     If both o1 and o2 are Some, calls fn(val1, val2) and returns Some(result).
 *     If either is None, returns None without calling fn.
 *
 * ── Strict mutation ─────────────────────────────────────────────────────────
 * These operate unconditionally and never panic.  See "Strict mutation
 * semantics" in the file-level comment for precise before/after state.
 *
 *   option_T_replace(&o, val) → option_T
 *     Stores val into o; returns the old option_T (Some or None).
 *     After: o is always Some(val).
 *
 *   option_T_take(&o) → option_T
 *     Copies o; resets o to None; returns the old option_T (Some or None).
 *     After: o is always None.
 *
 * ── Comparison ──────────────────────────────────────────────────────────────
 *   option_T_eq(o1, o2, eq_fn) → bool
 *     eq_fn signature: T, T → bool
 *     Returns true iff both are None, or both are Some and eq_fn(v1,v2) is true.
 *
 * ── Performance ─────────────────────────────────────────────────────────────
 * All operations: O(1) except combinators which are O(f), where f is the
 * user-supplied function.
 * Memory per instance: sizeof(bool) + sizeof(T) + alignment padding.
 *
 * ── Usage example ───────────────────────────────────────────────────────────
 * @code
 *   CANON_OPTION(int)
 *   CANON_OPTION(float)
 *
 *   option_int x = option_int_some(42);
 *   if (option_int_is_some(x)) {
 *       int value = option_int_unwrap(x);
 *       printf("Value: %d\n", value);
 *   }
 * @endcode
 *
 * @param type The value type (e.g. int, float, MyStruct, void*).
 *
 * @note Must be used at file or global scope, not inside functions.
 *       Use once per type per translation unit.
 *       See the "Code-size note" in the file-level comment for multi-TU
 *       projects.
 */
#define CANON_OPTION(type) \
    DEFINE_OPTION_ALL(static inline, type)

/**
 * @brief Legacy alias for CANON_OPTION.
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
 * @brief Unwrap an Option or return from the enclosing function with err_code.
 *
 * Requires: GNU C statement expressions (__extension__ ({ ... })) or C23.
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Evaluates opt_expr exactly once.
 * If the result is Some, expands to the contained value (usable as an expression).
 * If the result is None, executes `return (err_code)` in the enclosing function.
 *
 * err_code is evaluated only when the option is None.
 *
 * Intended for error propagation in functions that return Result types.
 * Do not use in functions whose return type is incompatible with err_code.
 *
 * Performance: O(1), no allocations.
 *
 * @param type      Type name matching CANON_OPTION(type) (e.g. int, float).
 * @param opt_expr  Expression that evaluates to option_##type.  Evaluated once.
 * @param err_code  Expression returned from the enclosing function if None.
 *
 * @code
 *   result_int_error do_work(void) {
 *       option_int opt = get_optional_value();
 *       int value = TRY_SOME(int, opt, err(error, ERROR_NOT_FOUND));
 *       // value is valid here
 *   }
 * @endcode
 */
#define TRY_SOME(type, opt_expr, err_code) \
    ({ \
        option_##type _opt_try_ = (opt_expr); \
        if (option_##type##_is_none(_opt_try_)) return (err_code); \
        option_##type##_unwrap(_opt_try_); \
    })

/**
 * @brief Unwrap an Option or evaluate to a default value.
 *
 * Requires: GNU C statement expressions or C23.
 * Disable with: #define CANON_NO_GNU_EXTENSIONS
 *
 * Evaluates opt_expr exactly once.
 * Evaluates default_val only when opt_expr is None (lazy – no side-effect
 * if the option is Some).
 *
 * Equivalent to option_T_unwrap_or() but usable in compound expressions
 * without introducing a temporary variable at the call site.
 *
 * Performance: O(1), no allocations.
 *
 * @param type        Type name matching CANON_OPTION(type).
 * @param opt_expr    Expression that evaluates to option_##type. Evaluated once.
 * @param default_val Expression evaluated and returned if None.
 *
 * @code
 *   int x = UNWRAP_OR_DEFAULT(int, get_config(), 100);
 * @endcode
 */
#define UNWRAP_OR_DEFAULT(type, opt_expr, default_val) \
    ({ \
        option_##type _opt_uod_ = (opt_expr); \
        option_##type##_is_some(_opt_uod_) ? \
            option_##type##_unwrap(_opt_uod_) : (default_val); \
    })

#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   COMMON TYPE INSTANTIATIONS (COMMENTED OUT BY DEFAULT)
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * Uncomment the types you need, or define your own in your code.
 *
 * Code-size note: each CANON_OPTION() generates ~1-2 KB of static inline
 * code per translation unit.  Only instantiate types you actually use.
 * See the "Code-size note" in the file-level comment for strategies to
 * avoid per-TU duplication in large projects.
 */

// CANON_OPTION(int)
// CANON_OPTION(unsigned)
// CANON_OPTION(long)
// CANON_OPTION(size_t)
// CANON_OPTION(float)
// CANON_OPTION(double)
// CANON_OPTION(char)

/* For pointer types: */
// typedef void* void_ptr;
// CANON_OPTION(void_ptr)

/* For custom types: */
// typedef struct { int x, y; } Point;
// CANON_OPTION(Point)

/*
 * Note: Option<bool> is intentionally omitted from this list.
 * It has three observable states (None, Some(false), Some(true)) which
 * creates ambiguity at call sites.  Use an explicit three-value enum or
 * Result<bool,E> so every state is self-documenting.
 */

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

        return option_int_some((int)val);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 3: Safe usage patterns (recommended for production)
    // ────────────────────────────────────────────────────────────────────────
    void example_safe_usage(void) {
        option_int result = parse_int("123");

        // Pattern 1: explicit check then unwrap
        if (option_int_is_some(result)) {
            int value = option_int_unwrap(result);
            printf("Value: %d\n", value);
        } else {
            printf("No value\n");
        }

        // Pattern 2: safe extraction via pointer
        int extracted;
        if (option_int_get(result, &extracted)) {
            printf("Extracted: %d\n", extracted);
        }

        // Pattern 3: default value when absent
        int value_or_zero = option_int_unwrap_or(result, 0);
        printf("Value or zero: %d\n", value_or_zero);

        // Pattern 4: invariant assertion with descriptive message
        int must_exist = option_int_expect(result, "parse_int(\"123\") must succeed");
        (void)must_exist;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 4: Combinators (map, filter, and_then)
    //
    //   map       fn : T → T          (transform value, keep in Option)
    //   and_then  fn : T → option_T   (transform that can also be None)
    //   filter    pred: T → bool       (discard value if pred is false)
    // ────────────────────────────────────────────────────────────────────────
    int  double_it(int x) { return x * 2; }
    bool is_even(int x)   { return (x % 2) == 0; }

    void example_chaining(void) {
        option_int opt = parse_int("21");

        opt = option_int_map(opt, double_it);   // fn: int→int  → Some(42)
        opt = option_int_filter(opt, is_even);  // pred: int→bool → Some(42)

        int final = option_int_unwrap_or(opt, -1); // 42
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 5: and_then – chaining fallible operations
    //   fn signature: T → option_T
    // ────────────────────────────────────────────────────────────────────────
    option_int half_if_even(int x) {
        if (x % 2 == 0) return option_int_some(x / 2);
        return option_int_none();
    }

    void example_and_then(void) {
        option_int start  = option_int_some(42);
        option_int result = option_int_and_then(start, half_if_even);
        // fn: int → option_int  → Some(21)

        int value = option_int_unwrap_or(result, 0); // 21
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 6: zip – combining two Options
    //   fn signature: T, T → T
    //   Returns Some(fn(a,b)) iff both inputs are Some; else None.
    // ────────────────────────────────────────────────────────────────────────
    int add(int a, int b) { return a + b; }

    void example_zip(void) {
        option_int a = option_int_some(10);
        option_int b = option_int_some(32);
        option_int c = option_int_zip(a, b, add); // Some(42)

        option_int d = option_int_some(10);
        option_int e = option_int_none();
        option_int f = option_int_zip(d, e, add); // None – b is absent
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 7: Strict mutation (replace / take)
    //
    //   replace(&o, val) → always makes o = Some(val); returns old option
    //   take(&o)         → always makes o = None;       returns old option
    //   Neither panics. Neither requires o to be Some beforehand.
    // ────────────────────────────────────────────────────────────────────────
    void example_mutation(void) {
        option_int opt = option_int_some(10);

        option_int old = option_int_replace(&opt, 20);
        // opt → Some(20),  old → Some(10)

        option_int taken = option_int_take(&opt);
        // opt → None,  taken → Some(20)

        option_int taken2 = option_int_take(&opt);
        // opt → None,  taken2 → None  (no panic: take on None is valid)
    }

    // ────────────────────────────────────────────────────────────────────────
    // Example 8: Custom types
    // ────────────────────────────────────────────────────────────────────────
    typedef struct { int x; int y; } Point;
    CANON_OPTION(Point)

    option_Point find_point(int id) {
        if (id == 0) return option_Point_none();
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

    // ────────────────────────────────────────────────────────────────────────
    // Example 9: Custom panic hook for certified / embedded builds
    // ────────────────────────────────────────────────────────────────────────
    // Define CANON_PANIC *before* including option.h:
    //
    //   #define CANON_PANIC(msg) do { log_fatal(msg); system_reset(); } while(0)
    //   #include "option.h"
    //
    // The macro must not return.  It receives a string literal.
*/

#endif /* CANON_SEMANTICS_OPTION_H */
