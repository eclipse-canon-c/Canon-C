/**
 * @file contract.h
 * @brief Explicit contracts, invariants, and assertions for Canon-C
 *
 * Provides explicit contract checking with better error messages and
 * customizable panic handling. Replaces standard assert() with semantic
 * clarity: require() for preconditions, ensure() for debug checks,
 * unreachable() for impossible code paths.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicitness: Contracts are visible and self-documenting in code
 * - Safety: Catch invariant violations early with clear diagnostics
 * - Flexibility: require() always checks, ensure() only in debug builds
 * - Clarity: Different macros signal different contract types
 * - Customizable: Single panic handler for all contract violations
 *
 * Build configuration flags:
 * ────────────────────────────────────────────────────────────────────────────
 * Three independent flags control enforcement levels:
 *
 * CANON_STRICT (define to enable)
 *   Promotes ensure() and ensure_msg() to always-on — identical to
 *   require() and require_msg(). Use for certified builds (DO-178C,
 *   ISO 26262) where debug-only checks must survive into production.
 *   Takes precedence over NDEBUG for ensure variants.
 *
 *   #define CANON_STRICT
 *   #include "core/primitives/contract.h"
 *
 * NDEBUG (standard — define to disable debug checks)
 *   Disables ensure() and ensure_msg() in release builds.
 *   Has no effect when CANON_STRICT is defined.
 *   Standard convention — same as assert.h.
 *
 * CANON_NO_REQUIRE (define to disable — use with extreme caution)
 *   Disables require() and require_msg() — the always-on precondition
 *   checks. Only for environments where the contract violations have
 *   been proved impossible by formal verification (Frama-C, SPARK)
 *   and the panic handler overhead is unacceptable (bare metal, ISR).
 *
 *   WARNING: Disabling require() removes Canon-C's last line of defense
 *   against null pointer dereference, overflow, and invariant violations.
 *   Do not use unless formal proof covers every call site.
 *
 *   #define CANON_NO_REQUIRE
 *   #include "core/primitives/contract.h"
 *
 * Enforcement matrix:
 * ────────────────────────────────────────────────────────────────────────────
 *
 *   Flag combination                  require()   ensure()
 *   ──────────────────────────────────────────────────────
 *   default (nothing defined)         ON          debug only
 *   NDEBUG                            ON          OFF
 *   CANON_STRICT                      ON          ON (always)
 *   CANON_STRICT + NDEBUG             ON          ON (STRICT wins)
 *   CANON_NO_REQUIRE                  OFF         debug only
 *   CANON_NO_REQUIRE + NDEBUG         OFF         OFF
 *   CANON_NO_REQUIRE + CANON_STRICT   OFF         ON
 *
 * Recommended build configurations:
 * ────────────────────────────────────────────────────────────────────────────
 *   Development:       default — require ON, ensure debug-only
 *   Release:           NDEBUG  — require ON, ensure OFF
 *   Certified build:   CANON_STRICT — require ON, ensure ON (always)
 *   Formal proof done: CANON_NO_REQUIRE — require OFF (proved impossible)
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All checks: O(1) — simple boolean expression evaluation
 * - require(): Always enabled by default, 1 branch + handler call on failure
 * - ensure(): Zero cost in release builds (NDEBUG defined, CANON_STRICT not)
 * - unreachable(): Zero cost hint in release, abort in debug
 * - Panic handler: User-defined, typically prints message + aborts
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Macros themselves are thread-safe (no shared state)
 * - Panic handler must be thread-safe if used in multithreaded context
 * - Default panic handler uses fprintf (not guaranteed thread-safe on all platforms)
 * - contract_set_handler() is NOT thread-safe — call once during init only
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (__FILE__, __LINE__, __func__)
 * - Uses standard abort() for termination
 * - No platform-specific code
 * - NDEBUG macro follows standard convention (same as assert.h)
 *
 * Handler storage:
 * ────────────────────────────────────────────────────────────────────────────
 * The handler is stored as a function-local static inside
 * canon_contract_handler_ptr(). This guarantees a single instance across
 * all translation units (unlike a static variable in a header, which would
 * produce one copy per TU). contract_set_handler() is therefore truly global
 * and affects all Canon-C contract checks program-wide.
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Precondition checks: require(ptr != NULL)
 * - Postcondition checks: ensure(result >= 0)
 * - Invariant checks: ensure(pool->used <= pool->capacity)
 * - Unreachable code: unreachable() in switch default cases
 * - Custom error messages: require_msg(len > 0, "empty buffer")
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Recoverable errors (use Result type instead)
 * - Input validation from untrusted sources (use explicit checks + errors)
 * - Performance-critical hot loops (check once outside loop)
 * - Expected failure conditions (contracts are for bugs, not expected errors)
 *
 * @sa types.h, result.h
 */

#ifndef CANON_CORE_PRIMITIVES_CONTRACT_H
#define CANON_CORE_PRIMITIVES_CONTRACT_H

#include "types.h"
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Contract Violation Handler
 * ========================================================================= */

/**
 * @brief Contract violation handler function type
 *
 * Called when a contract is violated. Should not return.
 *
 * @param file Source file where violation occurred
 * @param line Line number of violation
 * @param func Function name where violation occurred
 * @param expr Expression that failed (stringified condition)
 * @param msg  Optional custom message (may be NULL)
 */
typedef void (*contract_handler_fn)(
    const char* file,
    int         line,
    const char* func,
    const char* expr,
    const char* msg
);

/**
 * @brief Default contract violation handler
 *
 * Prints diagnostic information to stderr and calls abort().
 * This is the default handler used if no custom handler is installed.
 *
 * @remark Not thread-safe (uses fprintf)
 * @remark Never returns
 */
static inline void contract_default_handler(
    const char* file,
    int         line,
    const char* func,
    const char* expr,
    const char* msg
) {
    fprintf(stderr, "\n");
    fprintf(stderr, "════════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "CONTRACT VIOLATION\n");
    fprintf(stderr, "════════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "Location:   %s:%d\n", file, line);
    fprintf(stderr, "Function:   %s\n",    func);
    fprintf(stderr, "Condition:  %s\n",    expr);
    if (msg) {
        fprintf(stderr, "Message:    %s\n", msg);
    }
    fprintf(stderr, "════════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "\n");
    fflush(stderr);
    abort();
}

/* ============================================================================
 * Handler accessor — single instance across all translation units
 * ============================================================================
 *
 * A file-scope `static` variable in a header produces one independent copy
 * per translation unit. Calling contract_set_handler() in one .c file would
 * have no effect on any other .c file — the program-wide handler claim would
 * be silently broken.
 *
 * Storing the handler inside a function-local `static` variable fixes this:
 * the C standard guarantees exactly one instance of a function-local static
 * across the entire program, regardless of how many translation units include
 * this header. All reads and writes go through the same memory location.
 *
 * contract_set_handler() is NOT thread-safe. Call it once during program
 * initialization, before any concurrent code runs.
 * ========================================================================= */

/**
 * @brief Returns a pointer to the single global contract handler slot
 *
 * Internal use only. All contract macros and contract_set_handler() go
 * through this accessor to guarantee a single handler instance across
 * all translation units.
 *
 * @return Pointer to the handler function pointer (never NULL)
 */
static inline contract_handler_fn* canon_contract_handler_ptr(void) {
    static contract_handler_fn handler = contract_default_handler;
    return &handler;
}

/**
 * @brief Set a custom contract violation handler (program-wide)
 *
 * Replaces the default handler for all subsequent contract violations
 * across all translation units. Call once during program initialization,
 * before spawning threads.
 *
 * @param handler Function to call on contract violations (must not return).
 *                Passing NULL restores the default handler.
 *
 * Example:
 * ```c
 * void my_handler(const char* file, int line, const char* func,
 *                 const char* expr, const char* msg) {
 *     log_fatal("Contract failed: %s at %s:%d", expr, file, line);
 *     cleanup_and_exit(1);
 * }
 *
 * int main(void) {
 *     contract_set_handler(my_handler);
 *     // ...
 * }
 * ```
 */
static inline void contract_set_handler(contract_handler_fn handler) {
    *canon_contract_handler_ptr() = handler
        ? handler
        : contract_default_handler;
}

/* ============================================================================
 * Internal Helper Macros
 * ========================================================================= */

#ifdef __GNUC__
    #define CANON_UNREACHABLE_HINT() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define CANON_UNREACHABLE_HINT() __assume(0)
#else
    #define CANON_UNREACHABLE_HINT() ((void)0)
#endif

#ifndef __func__
    #if __STDC_VERSION__ >= 199901L
        /* C99 provides __func__ */
    #elif defined(__GNUC__)
        #define __func__ __FUNCTION__
    #else
        #define __func__ "<unknown>"
    #endif
#endif

/**
 * @brief Internal macro: invoke the current contract handler
 *
 * Goes through canon_contract_handler_ptr() to ensure the correct
 * program-wide handler is always called, regardless of TU.
 */
#define _CANON_INVOKE_HANDLER(expr_str, msg) \
    (*canon_contract_handler_ptr())(__FILE__, __LINE__, __func__, expr_str, msg)

/* ============================================================================
 * Core Contract Macros
 * ========================================================================= */

/**
 * @brief Always-on precondition check (disabled only with CANON_NO_REQUIRE)
 *
 * Use for critical invariants that must hold in production builds.
 * Never disabled by NDEBUG.
 *
 * Can be disabled by defining CANON_NO_REQUIRE — only appropriate when
 * formal verification (Frama-C, SPARK) has proved all call sites safe
 * and the panic handler overhead is unacceptable (bare metal, ISR context).
 *
 * @warning Disabling via CANON_NO_REQUIRE removes Canon-C's primary defense
 *          against null pointer dereference and invariant violations.
 *          Only use when formal proof covers every call site.
 *
 * @param cond Condition that must be true
 *
 * Example:
 * ```c
 * void* arena_alloc(Arena* arena, usize size) {
 *     require(arena != NULL);
 *     require(size > 0);
 *     // ... allocation logic
 * }
 * ```
 *
 * @sa ensure(), require_msg()
 */
#ifdef CANON_NO_REQUIRE
    #define require(cond) ((void)0)
#else
    #define require(cond) \
        do { \
            if (!(cond)) { \
                _CANON_INVOKE_HANDLER(#cond, NULL); \
            } \
        } while (0)
#endif

/**
 * @brief Always-on precondition check with custom message
 *
 * Like require(), but includes a custom error message.
 * Disabled by CANON_NO_REQUIRE — see require() for full warning.
 *
 * @param cond Condition that must be true
 * @param msg  Custom error message (string literal)
 *
 * Example:
 * ```c
 * require_msg(index < vec->len, "index out of bounds");
 * ```
 *
 * @sa require()
 */
#ifdef CANON_NO_REQUIRE
    #define require_msg(cond, msg) ((void)0)
#else
    #define require_msg(cond, msg) \
        do { \
            if (!(cond)) { \
                _CANON_INVOKE_HANDLER(#cond, msg); \
            } \
        } while (0)
#endif

/**
 * @brief Debug-only contract check — always-on with CANON_STRICT
 *
 * Default behavior:
 * - Debug builds (NDEBUG not defined): ON
 * - Release builds (NDEBUG defined):   OFF
 *
 * With CANON_STRICT defined:
 * - Always ON regardless of NDEBUG — identical to require()
 * - Use for certified builds (DO-178C, ISO 26262) where debug
 *   assertions must survive into production
 *
 * @param cond Condition that should be true
 *
 * Example:
 * ```c
 * void pool_release(Pool* pool, void* ptr) {
 *     ensure(pool != NULL);
 *     ensure(ptr != NULL);
 *     ensure(pool->used > 0);  // internal invariant
 * }
 * ```
 *
 * @sa require(), ensure_msg()
 */
#ifdef CANON_STRICT
    /* Certified build — ensure is always-on, identical to require */
    #define ensure(cond) \
        do { \
            if (!(cond)) { \
                _CANON_INVOKE_HANDLER(#cond, NULL); \
            } \
        } while (0)
#elif defined(NDEBUG)
    #define ensure(cond) ((void)0)
#else
    #define ensure(cond) require(cond)
#endif

/**
 * @brief Debug-only contract check with custom message — always-on with CANON_STRICT
 *
 * Like ensure(), but includes a custom error message.
 *
 * With CANON_STRICT: always-on regardless of NDEBUG.
 * Without CANON_STRICT: compiled out when NDEBUG is defined.
 *
 * @param cond Condition that should be true
 * @param msg  Custom error message (string literal)
 *
 * @sa ensure(), require_msg()
 */
#ifdef CANON_STRICT
    /* Certified build — ensure_msg is always-on, identical to require_msg */
    #define ensure_msg(cond, msg) \
        do { \
            if (!(cond)) { \
                _CANON_INVOKE_HANDLER(#cond, msg); \
            } \
        } while (0)
#elif defined(NDEBUG)
    #define ensure_msg(cond, msg) ((void)0)
#else
    #define ensure_msg(cond, msg) require_msg(cond, msg)
#endif

/**
 * @brief Mark code path as unreachable
 *
 * Use in code that should never execute (e.g., switch default cases for
 * exhaustive enums). In debug builds, triggers panic. In release builds,
 * provides optimization hint to compiler.
 *
 * With CANON_STRICT: always triggers panic (never just a hint).
 *
 * @remark Never returns
 *
 * Example:
 * ```c
 * switch (state) {
 *     case STATE_INIT:    return init_handler();
 *     case STATE_RUNNING: return run_handler();
 *     case STATE_STOPPED: return stop_handler();
 *     default: unreachable();
 * }
 * ```
 *
 * @sa unreachable_msg()
 */
#if defined(CANON_STRICT)
    /* Certified build — unreachable always panics, never silently optimized */
    #define unreachable() \
        do { \
            _CANON_INVOKE_HANDLER("unreachable code path", NULL); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#elif defined(NDEBUG)
    #define unreachable() CANON_UNREACHABLE_HINT()
#else
    #define unreachable() \
        do { \
            _CANON_INVOKE_HANDLER("unreachable code path", NULL); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#endif

/**
 * @brief Mark code path as unreachable with custom message
 *
 * Like unreachable(), but includes a custom error message.
 * With CANON_STRICT: always panics.
 *
 * @param msg Custom error message explaining why code is unreachable
 *
 * @sa unreachable()
 */
#if defined(CANON_STRICT)
    #define unreachable_msg(msg) \
        do { \
            _CANON_INVOKE_HANDLER("unreachable code path", msg); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#elif defined(NDEBUG)
    #define unreachable_msg(msg) CANON_UNREACHABLE_HINT()
#else
    #define unreachable_msg(msg) \
        do { \
            _CANON_INVOKE_HANDLER("unreachable code path", msg); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#endif

/**
 * @brief Unconditional panic with message
 *
 * Immediately triggers the contract violation handler with a custom message.
 * Use for fatal errors that cannot be recovered from.
 * Never disabled by any flag — panic is always unconditional.
 *
 * @param msg Error message (string literal)
 *
 * @remark Never returns
 *
 * Example:
 * ```c
 * if (config_parse_failed) {
 *     panic("failed to parse configuration file");
 * }
 * ```
 */
#define panic(msg) \
    do { \
        _CANON_INVOKE_HANDLER("panic", msg); \
    } while (0)

/* ============================================================================
 * Compile-Time Assertions
 * ========================================================================= */

/**
 * @brief Compile-time assertion (fails at compile time if condition is false)
 *
 * Use for checking compile-time constants like struct sizes, alignments,
 * or configuration values. Never disabled by any flag.
 *
 * @param cond Compile-time constant expression
 * @param msg  Identifier-safe message (no spaces, will appear in error)
 *
 * Example:
 * ```c
 * static_require(sizeof(Header) == 64, header_must_be_64_bytes);
 * static_require(BUFFER_SIZE >= 1024, buffer_too_small);
 * ```
 */
#if __STDC_VERSION__ >= 201112L
    /* C11 has _Static_assert */
    #define static_require(cond, msg) _Static_assert(cond, #msg)
#else
    /* C99 fallback using array typedef trick */
    #define static_require(cond, msg) \
        typedef char static_assert_##msg[(cond) ? 1 : -1]
#endif

/* ============================================================================
 * Build configuration sanity checks
 * ========================================================================= */

/*
 * Warn if CANON_NO_REQUIRE is combined with CANON_STRICT — this is almost
 * certainly a misconfiguration. CANON_STRICT promotes ensure to always-on
 * but CANON_NO_REQUIRE silences require entirely, leaving no enforcement
 * for preconditions. The combination is legal but likely unintentional.
 */
#if defined(CANON_NO_REQUIRE) && defined(CANON_STRICT)
    /* Use GCC/Clang pragma warning if available, otherwise rely on comment */
    #ifdef __GNUC__
        #pragma message \
            "WARNING: CANON_NO_REQUIRE + CANON_STRICT: require() is disabled " \
            "but ensure() is always-on. Preconditions are unprotected. " \
            "Only use this combination if formal proof covers all require() sites."
    #endif
#endif

/* ============================================================================
 * Usage Examples (documentation only)
 * ========================================================================= */

/*
// ─────────────────────────────────────────────────────────────────────────────
// Example 1: Arena allocator with contracts
// ─────────────────────────────────────────────────────────────────────────────
void* arena_alloc(Arena* arena, usize size) {
    require(arena != NULL);
    require(size > 0);

    usize new_offset;
    if (!checked_add(arena->offset, size, &new_offset)) {
        panic("arena allocation overflow");
    }

    require_msg(new_offset <= arena->capacity, "arena out of memory");

    void* ptr = arena->base + arena->offset;
    arena->offset = new_offset;

    ensure(is_aligned(ptr, 8));  // debug-only, always-on with CANON_STRICT
    return ptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Example 2: Pool with exhaustive enum check
// ─────────────────────────────────────────────────────────────────────────────
typedef enum { BLOCK_FREE, BLOCK_USED } BlockState;

void pool_process(Pool* pool, usize index) {
    BlockState state = pool->states[index];

    switch (state) {
        case BLOCK_FREE: handle_free_block(pool, index); break;
        case BLOCK_USED: handle_used_block(pool, index); break;
        default: unreachable_msg("invalid block state");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Example 3: Compile-time checks
// ─────────────────────────────────────────────────────────────────────────────
static_require(sizeof(Header) == 64, header_size_mismatch);
static_require(MAX_POOL_SIZE <= USIZE_MAX / sizeof(Block), pool_size_overflow);

// ─────────────────────────────────────────────────────────────────────────────
// Example 4: Custom panic handler — embedded systems
// ─────────────────────────────────────────────────────────────────────────────
void embedded_panic_handler(const char* file, int line, const char* func,
                            const char* expr, const char* msg) {
    uart_printf("PANIC: %s at %s:%d\n", expr, file, line);
    signal_fatal_error();
    while (1) { __WFI(); }
}

void embedded_init(void) {
    contract_set_handler(embedded_panic_handler);
}

// ─────────────────────────────────────────────────────────────────────────────
// Example 5: Build configurations
// ─────────────────────────────────────────────────────────────────────────────

// Development build (default):
//   require ON, ensure debug-only
//   gcc -o myapp main.c

// Release build:
//   require ON, ensure OFF
//   gcc -DNDEBUG -o myapp main.c

// Certified build (DO-178C / ISO 26262):
//   require ON, ensure always-on
//   gcc -DCANON_STRICT -o myapp main.c

// Formally verified build (Frama-C proved all sites):
//   require OFF, ensure OFF
//   gcc -DCANON_NO_REQUIRE -DNDEBUG -o myapp main.c

// ─────────────────────────────────────────────────────────────────────────────
// Example 6: Restore default handler
// ─────────────────────────────────────────────────────────────────────────────
contract_set_handler(NULL);  // NULL → restores contract_default_handler
*/

#endif /* CANON_CORE_PRIMITIVES_CONTRACT_H */
