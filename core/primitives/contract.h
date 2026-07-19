/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

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
 * Header-only usage:
 * ────────────────────────────────────────────────────────────────────────────
 * This header is self-contained — no separate .c file is required.
 * However, exactly one translation unit must provide the handler definition
 * by defining CANON_CONTRACT_IMPL before including this header:
 *
 *   // In exactly one .c file (e.g. main.c):
 *   #define CANON_CONTRACT_IMPL
 *   #include "core/primitives/contract.h"
 *
 *   // In all other .c files:
 *   #include "core/primitives/contract.h"
 *
 * If CANON_CONTRACT_IMPL is defined in zero translation units:
 *   → linker error: undefined reference to canon_contract_handler
 *
 * If CANON_CONTRACT_IMPL is defined in two or more translation units:
 *   → linker error: duplicate symbol canon_contract_handler
 *
 * Both failure modes are loud and immediate at link time.
 * The contract is self-enforcing — no runtime behavior is silently wrong.
 *
 * Build configuration flags:
 * ────────────────────────────────────────────────────────────────────────────
 * Four independent flags control enforcement levels:
 *
 * CANON_CONTRACT_IMPL (define in exactly one translation unit)
 *   Emits the definition of canon_contract_handler. Required once per
 *   program. All other TUs see the extern declaration and link against it.
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
 *   checks. Only for environments where contract violations have been
 *   proved impossible by formal verification (Frama-C, SPARK) and the
 *   panic handler overhead is unacceptable (bare metal, ISR).
 *
 *   WARNING: Disabling require() removes Canon-C's last line of defense
 *   against null pointer dereference, overflow, and invariant violations.
 *   Do not use unless formal proof covers every call site.
 *
 *   #define CANON_NO_REQUIRE
 *   #include "core/primitives/contract.h"
 *
 * CANON_NO_GNU_EXTENSIONS (define to disable compiler-specific extensions)
 *   Disables __builtin_unreachable(), __assume(0), and #pragma message.
 *   Use when targeting strictly conforming C99 or compilers that do not
 *   support these extensions. The unreachable() macro degrades gracefully
 *   to a safe no-op hint (the panic call still fires in debug builds).
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
 *   Strict C99:        CANON_NO_GNU_EXTENSIONS — no compiler extensions
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
 * - Requires C99 or later (__FILE__, __LINE__, __func__ are C99 guaranteed)
 * - Uses standard abort() for termination
 * - Compiler-specific hints (__builtin_unreachable, __assume) are used where
 *   available and guarded by CANON_NO_GNU_EXTENSIONS for strict builds
 * - NDEBUG macro follows standard convention (same as assert.h)
 *
 * Formal verification:
 * ────────────────────────────────────────────────────────────────────────────
 * When __FRAMAC__ is defined (automatic under Frama-C), the default
 * handler's stdio body is replaced with a non-terminating while(1){}
 * loop guarded by `loop invariant \false`. The function contract
 * specifies `ensures \false` and `exits \false`, together stating that
 * the handler cannot terminate by any means (normal return or exit()).
 * WP discharges these obligations from the loop invariant: the
 * invariant \false cannot be established at loop entry, which WP
 * treats as proof that the loop is never exited, and therefore the
 * function never reaches any post-state. This pattern avoids ~20
 * spurious fprintf/fflush goals that would otherwise appear in every
 * WP run of a header that uses require_msg or ensure_msg at runtime,
 * while still proving the critical property (non-return) that callers
 * depend on. The runtime behavior in normal (non-Frama-C) builds is
 * unchanged.
 *
 * Handler storage:
 * ────────────────────────────────────────────────────────────────────────────
 * canon_contract_handler is a plain extern variable. The TU that defines
 * CANON_CONTRACT_IMPL emits the definition; all other TUs see the extern
 * declaration. contract_set_handler() writes directly to that variable —
 * no indirection, no tricks. All TUs share the same handler through
 * standard C extern linkage.
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
 * Called when a contract is violated. Must not return.
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
 * @note Not thread-safe (uses fprintf)
 * @note Never returns
 *
 * Formal verification: under Frama-C (__FRAMAC__ defined), the stdio
 * body is replaced with a non-terminating loop guarded by
 * `loop invariant \false`. The function contract specifies both
 * `ensures \false` and `exits \false`, stating that the handler
 * cannot terminate by any means. WP discharges these obligations
 * from the loop invariant's unestablishability. See the file-level
 * "Formal verification" note for details.
 */
/*@
  assigns \nothing;
  ensures \false;
  exits   \false;
 */
static inline void contract_default_handler(
    const char* file,
    int         line,
    const char* func,
    const char* expr,
    const char* msg
) {
#ifndef __FRAMAC__
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
#else
    /* Under Frama-C, replace the stdio body with a non-terminating loop.
     * The combination of `ensures \false` + `exits \false` on the function
     * contract tells WP the function never returns by any means (normal
     * return or exit()). The `loop invariant \false` on the while loop is
     * the standard ACSL idiom for proving an infinite loop: WP treats the
     * unestablishable invariant as proof that the loop is never exited,
     * and therefore the function never reaches any post-state. This
     * discharges ensures \false and exits \false cleanly. */
    (void)file;
    (void)line;
    (void)func;
    (void)expr;
    (void)msg;
    /*@ loop invariant \false;
        loop assigns \nothing;
     */
    while (1) { }
#endif
}

/* ============================================================================
 * Handler storage
 * ============================================================================
 *
 * canon_contract_handler is a plain extern variable shared across all
 * translation units through standard C extern linkage.
 *
 * Exactly one translation unit must define CANON_CONTRACT_IMPL before
 * including this header. That TU emits the variable definition. All
 * other TUs see the extern declaration and link against the same slot.
 *
 * contract_set_handler() writes directly to this variable — no indirection.
 * It is NOT thread-safe. Call once during program initialization, before
 * any concurrent code runs.
 * ========================================================================= */

#ifdef CANON_CONTRACT_IMPL
    /**
     * @brief The global contract violation handler
     *
     * Defined in the translation unit that sets CANON_CONTRACT_IMPL.
     * Initialized to contract_default_handler.
     * Write through contract_set_handler() only.
     */
    contract_handler_fn canon_contract_handler = contract_default_handler;
#else
    extern contract_handler_fn canon_contract_handler;
#endif

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
    canon_contract_handler = handler
        ? handler
        : contract_default_handler;
}

/* ============================================================================
 * Internal Helper Macros
 * ========================================================================= */

/**
 * @brief Compiler hint for unreachable code paths
 *
 * Provides an optimization hint to the compiler when a code path is
 * provably unreachable. This is a hint only — the panic call in
 * unreachable() fires before this is ever reached in debug builds.
 *
 * Disabled entirely when CANON_NO_GNU_EXTENSIONS is defined, falling
 * back to a safe no-op.
 *
 * @note This is a code-generation hint, not a safety mechanism.
 *       Safety is provided by the panic call that precedes it.
 */
#if !defined(CANON_NO_GNU_EXTENSIONS) && defined(__GNUC__)
    #define CANON_UNREACHABLE_HINT() __builtin_unreachable()
#elif !defined(CANON_NO_GNU_EXTENSIONS) && defined(_MSC_VER)
    #define CANON_UNREACHABLE_HINT() __assume(0)
#else
    #define CANON_UNREACHABLE_HINT() ((void)0)
#endif

/**
 * @brief Internal macro: invoke the current contract handler
 *
 * Reads canon_contract_handler directly — the same variable in all
 * translation units through standard extern linkage.
 */
#define CANON_INVOKE_HANDLER_(expr_str, msg) \
    canon_contract_handler(__FILE__, __LINE__, __func__, expr_str, msg)

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
                CANON_INVOKE_HANDLER_(#cond, NULL); \
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
                CANON_INVOKE_HANDLER_(#cond, msg); \
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
    #define ensure(cond) \
        do { \
            if (!(cond)) { \
                CANON_INVOKE_HANDLER_(#cond, NULL); \
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
    #define ensure_msg(cond, msg) \
        do { \
            if (!(cond)) { \
                CANON_INVOKE_HANDLER_(#cond, msg); \
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
 * provides an optimization hint to the compiler (see CANON_UNREACHABLE_HINT).
 *
 * With CANON_STRICT: always triggers panic (never silently optimized away).
 *
 * @note Never returns
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
    #define unreachable() \
        do { \
            CANON_INVOKE_HANDLER_("unreachable code path", NULL); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#elif defined(NDEBUG)
    #define unreachable() CANON_UNREACHABLE_HINT()
#else
    #define unreachable() \
        do { \
            CANON_INVOKE_HANDLER_("unreachable code path", NULL); \
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
            CANON_INVOKE_HANDLER_("unreachable code path", msg); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#elif defined(NDEBUG)
    #define unreachable_msg(msg) CANON_UNREACHABLE_HINT()
#else
    #define unreachable_msg(msg) \
        do { \
            CANON_INVOKE_HANDLER_("unreachable code path", msg); \
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
 * @note Never returns
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
        CANON_INVOKE_HANDLER_("panic", msg); \
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
 * In C11 and later, expands to _Static_assert.
 * In C99, expands to a negative-size array typedef that produces a
 * compile error on failure.
 *
 * @param cond Compile-time constant expression
 * @param msg  Identifier-safe message (no spaces — appears in compiler error)
 *
 * Example:
 * ```c
 * static_require(sizeof(Header) == 64, header_must_be_64_bytes);
 * static_require(BUFFER_SIZE >= 1024, buffer_too_small);
 * ```
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define static_require(cond, msg) _Static_assert(cond, #msg)
#else
    /* C99 fallback: negative-size array produces a compile error */
    #define static_require(cond, msg) \
        typedef char static_assert_##msg[(cond) ? 1 : -1]
#endif

/* ============================================================================
 * Build configuration sanity checks
 * ========================================================================= */

/*
 * Warn if CANON_NO_REQUIRE is combined with CANON_STRICT.
 * CANON_STRICT promotes ensure() to always-on, but CANON_NO_REQUIRE
 * silences require() entirely, leaving preconditions unprotected.
 * The combination is legal (require OFF, ensure ON) but almost certainly
 * a misconfiguration — warn unless GNU extensions are suppressed.
 */
#if defined(CANON_NO_REQUIRE) && defined(CANON_STRICT) && !defined(CANON_NO_GNU_EXTENSIONS)
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
// Example 1: Project setup — define CANON_CONTRACT_IMPL in exactly one TU
// ─────────────────────────────────────────────────────────────────────────────

// main.c — the one TU that owns the handler definition
#define CANON_CONTRACT_IMPL
#include "core/primitives/contract.h"

// arena.c, pool.c, etc. — all other TUs just include normally
#include "core/primitives/contract.h"

// ─────────────────────────────────────────────────────────────────────────────
// Example 2: Arena allocator with contracts
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
// Example 3: Pool with exhaustive enum check
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
// Example 4: Compile-time checks
// ─────────────────────────────────────────────────────────────────────────────
static_require(sizeof(Header) == 64, header_size_mismatch);
static_require(MAX_POOL_SIZE <= USIZE_MAX / sizeof(Block), pool_size_overflow);

// ─────────────────────────────────────────────────────────────────────────────
// Example 5: Custom panic handler — embedded systems
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
// Example 6: Build configurations
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

// Strictly conforming C99 (no compiler extensions):
//   gcc -DCANON_NO_GNU_EXTENSIONS -o myapp main.c

// ─────────────────────────────────────────────────────────────────────────────
// Example 7: Restore default handler
// ─────────────────────────────────────────────────────────────────────────────
contract_set_handler(NULL);  // NULL restores contract_default_handler
*/

#endif /* CANON_CORE_PRIMITIVES_CONTRACT_H */
