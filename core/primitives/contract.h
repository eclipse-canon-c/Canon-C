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
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All checks: O(1) — simple boolean expression evaluation
 * - require(): Always enabled, 1 branch + function call on failure
 * - ensure(): Zero cost in release builds (NDEBUG defined)
 * - unreachable(): Zero cost hint in release, abort in debug
 * - Panic handler: User-defined, typically prints message + aborts
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Macros themselves are thread-safe (no shared state)
 * - Panic handler must be thread-safe if used in multithreaded context
 * - Default panic handler uses fprintf (not guaranteed thread-safe on all platforms)
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (__FILE__, __LINE__, __func__)
 * - Uses standard abort() for termination
 * - No platform-specific code
 * - NDEBUG macro follows standard convention (same as assert.h)
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
 * @param msg Optional custom message (may be NULL)
 */
typedef void (*contract_handler_fn)(
    const char* file,
    int line,
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
    int line,
    const char* func,
    const char* expr,
    const char* msg
) {
    fprintf(stderr, "\n");
    fprintf(stderr, "════════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "CONTRACT VIOLATION\n");
    fprintf(stderr, "════════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "Location:   %s:%d\n", file, line);
    fprintf(stderr, "Function:   %s\n", func);
    fprintf(stderr, "Condition:  %s\n", expr);
    if (msg) {
        fprintf(stderr, "Message:    %s\n", msg);
    }
    fprintf(stderr, "════════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "\n");
    fflush(stderr);
    abort();
}

/**
 * @brief Global contract violation handler (customizable)
 *
 * Users can override this by defining their own handler before including
 * contract.h, or by setting it at runtime.
 */
#ifndef CANON_CONTRACT_HANDLER
    static contract_handler_fn canon_contract_handler = contract_default_handler;
#endif

/**
 * @brief Set custom contract violation handler
 *
 * @param handler Function to call on contract violations (must not return)
 *
 * Example:
 * ```c
 * void my_handler(const char* file, int line, const char* func,
 *                 const char* expr, const char* msg) {
 *     log_fatal("Contract failed: %s at %s:%d", expr, file, line);
 *     cleanup_and_exit(1);
 * }
 * 
 * contract_set_handler(my_handler);
 * ```
 */
static inline void contract_set_handler(contract_handler_fn handler) {
    canon_contract_handler = handler;
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

/* ============================================================================
 * Core Contract Macros
 * ========================================================================= */

/**
 * @brief Always-on contract check (precondition/invariant)
 *
 * Use for critical invariants that must hold even in release builds.
 * Never disabled by NDEBUG.
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
#define require(cond) \
    do { \
        if (!(cond)) { \
            canon_contract_handler(__FILE__, __LINE__, __func__, #cond, NULL); \
        } \
    } while (0)

/**
 * @brief Always-on contract check with custom message
 *
 * Like require(), but includes a custom error message.
 *
 * @param cond Condition that must be true
 * @param msg Custom error message (string literal)
 *
 * Example:
 * ```c
 * require_msg(index < vec->len, "index out of bounds");
 * ```
 *
 * @sa require()
 */
#define require_msg(cond, msg) \
    do { \
        if (!(cond)) { \
            canon_contract_handler(__FILE__, __LINE__, __func__, #cond, msg); \
        } \
    } while (0)

/**
 * @brief Debug-only contract check (disabled in release builds)
 *
 * Use for checks that are helpful during development but too expensive
 * for production. Compiled out when NDEBUG is defined.
 *
 * @param cond Condition that should be true
 *
 * Example:
 * ```c
 * void pool_release(Pool* pool, void* ptr) {
 *     ensure(pool != NULL);
 *     ensure(ptr != NULL);
 *     ensure(pool->used > 0);  // internal invariant
 *     // ... release logic
 * }
 * ```
 *
 * @sa require()
 */
#ifdef NDEBUG
    #define ensure(cond) ((void)0)
#else
    #define ensure(cond) require(cond)
#endif

/**
 * @brief Debug-only contract check with custom message
 *
 * Like ensure(), but includes a custom error message.
 * Compiled out when NDEBUG is defined.
 *
 * @param cond Condition that should be true
 * @param msg Custom error message (string literal)
 *
 * @sa ensure(), require_msg()
 */
#ifdef NDEBUG
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
 * @remark Never returns
 *
 * Example:
 * ```c
 * switch (state) {
 *     case STATE_INIT: return init_handler();
 *     case STATE_RUNNING: return run_handler();
 *     case STATE_STOPPED: return stop_handler();
 *     default: unreachable();
 * }
 * ```
 *
 * @sa unreachable_msg()
 */
#ifdef NDEBUG
    #define unreachable() CANON_UNREACHABLE_HINT()
#else
    #define unreachable() \
        do { \
            canon_contract_handler(__FILE__, __LINE__, __func__, \
                                  "unreachable code path", NULL); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#endif

/**
 * @brief Mark code path as unreachable with custom message
 *
 * Like unreachable(), but includes a custom error message.
 *
 * @param msg Custom error message explaining why code is unreachable
 *
 * @sa unreachable()
 */
#ifdef NDEBUG
    #define unreachable_msg(msg) CANON_UNREACHABLE_HINT()
#else
    #define unreachable_msg(msg) \
        do { \
            canon_contract_handler(__FILE__, __LINE__, __func__, \
                                  "unreachable code path", msg); \
            CANON_UNREACHABLE_HINT(); \
        } while (0)
#endif

/**
 * @brief Unconditional panic with message
 *
 * Immediately triggers contract violation handler with custom message.
 * Use for fatal errors that cannot be recovered from.
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
        canon_contract_handler(__FILE__, __LINE__, __func__, "panic", msg); \
    } while (0)

/* ============================================================================
 * Compile-Time Assertions
 * ========================================================================= */

/**
 * @brief Compile-time assertion (fails at compile time if condition is false)
 *
 * Use for checking compile-time constants like struct sizes, alignments,
 * or configuration values.
 *
 * @param cond Compile-time constant expression
 * @param msg Identifier-safe message (no spaces, will appear in error)
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
 * Usage Examples (documentation only)
 * ========================================================================= */

/*
// Arena allocator with contracts
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
    
    ensure(is_aligned(ptr, 8));  // debug-only invariant check
    return ptr;
}

// Pool with exhaustive enum check
typedef enum { BLOCK_FREE, BLOCK_USED } BlockState;

void pool_process(Pool* pool, usize index) {
    BlockState state = pool->states[index];
    
    switch (state) {
        case BLOCK_FREE:
            handle_free_block(pool, index);
            break;
        case BLOCK_USED:
            handle_used_block(pool, index);
            break;
        default:
            unreachable_msg("invalid block state");
    }
}

// Compile-time checks
static_require(sizeof(Header) == 64, header_size_mismatch);
static_require(MAX_POOL_SIZE <= USIZE_MAX / sizeof(Block), pool_size_overflow);

// Custom panic handler for embedded systems
void embedded_panic_handler(const char* file, int line, const char* func,
                           const char* expr, const char* msg) {
    // Log to UART
    uart_printf("PANIC: %s at %s:%d\n", expr, file, line);
    
    // Flash LED pattern
    signal_fatal_error();
    
    // Halt system
    while (1) { __WFI(); }
}

void embedded_init(void) {
    contract_set_handler(embedded_panic_handler);
}
*/

#endif /* CANON_CORE_PRIMITIVES_CONTRACT_H */
