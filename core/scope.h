#ifndef CANON_CORE_SCOPE_H
#define CANON_CORE_SCOPE_H

/**
 * @file scope.h
 * @brief RAII-style deferred cleanup for pure C using zero-overhead macros
 *
 * Provides automatic execution of cleanup code when leaving the current lexical scope,
 * regardless of exit reason (normal end, return, break, continue, goto *within* scope).
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero runtime overhead: expands to nested for-loops optimized away by compiler
 * - LIFO execution order: last declared defer runs first (stack-like unwinding)
 * - Matches acquisition order: resources acquired first are released last
 * - Works with return, break, continue, normal block end, goto inside block
 * - No allocations, no function pointers, no vtables — pure macro expansion
 * - Simple, readable syntax inspired by Go, Zig, Swift defer
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Compile-time only: zero added instructions beyond the cleanup code itself
 * - Compiler usually inlines and eliminates the for-loop structure
 * - No indirection or dynamic dispatch
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Fully thread-safe — no shared state, each SCOPE_DEFER is local to its block
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (for loop variable declarations)
 * - Compatible with GCC, Clang, MSVC, ICC, TCC, etc.
 * - No compiler extensions required
 * - No platform-specific code
 *
 * Important limitations & semantics:
 * ──────────────────────────────────────────────────────────────────────────────
 * | Exit method                          | Cleanup executed? | Notes                                      |
 * |--------------------------------------|-------------------|--------------------------------------------|
 * | Normal end of block                  | Yes               |                                            |
 * | return from function                 | Yes               |                                            |
 * | break / continue                     | Yes               | Only current loop/block                    |
 * | goto label **inside** block          | Yes               |                                            |
 * | goto label **outside** block         | **No**            | Skips all defers — avoid!                  |
 * | longjmp / _longjmp out               | **No**            | Bypasses completely — avoid!               |
 * | exit(), abort(), _Exit()             | **No**            | Process termination                        |
 *
 * Critical rules:
 * - Never place break/continue/return/goto *inside* a SCOPE_DEFER block
 * - Avoid goto that jumps *out* of the SCOPE_DEFER scope
 * - Avoid longjmp across SCOPE_DEFER boundaries
 * - Keep cleanup blocks extremely simple (free, fclose, unlock, reset_to…)
 * - SCOPE_DEFER can be safely nested in loops/conditionals
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - File / socket / handle cleanup
 * - Memory allocation / arena checkpoint release
 * - Mutex / lock / semaphore unlocking
 * - Resource acquisition in error-prone code
 * - Temporary allocations in parsers / serializers
 * - Transaction-style scoped state changes
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Code that must survive longjmp or abnormal termination
 * - Cleanup that requires complex control flow inside defer block
 * - Situations where goto jumps frequently escape scopes
 * - Replacing proper exception / unwind mechanisms in very large codebases
 *
 * @sa SCOPE_DEFER
 */

/**
 * @def SCOPE_DEFER
 * @brief Executes cleanup code automatically on scope exit
 *
 * The cleanup block is guaranteed to run exactly once when control leaves
 * the enclosing block — whether by reaching the end, returning, breaking,
 * continuing, or jumping to a label inside the same block.
 *
 * Multiple SCOPE_DEFER statements in the same block execute in **reverse**
 * declaration order (LIFO — like stack unwinding).
 *
 * @remark Zero runtime cost — macro expands to nested for-loops that
 * modern compilers eliminate completely (especially with -O2 or higher).
 *
 * @remark The defer block must **not** contain:
 * - return
 * - break / continue that exits the defer itself
 * - goto that jumps out of the defer block
 *
 * Basic pattern:
 * ```c
 * FILE* f = fopen("data.txt", "r");
 * if (!f) return ERR_OPEN;
 *
 * SCOPE_DEFER {
 *     if (f) fclose(f);
 * }
 *
 * // use f safely — guaranteed closed on any normal exit
 * ```
 *
 * Multiple resources (LIFO order):
 * ```c
 * void* mem = malloc(4096);
 * if (!mem) return ERR_ALLOC;
 *
 * SCOPE_DEFER { free(mem); } // runs first
 *
 * FILE* log = fopen("log.txt", "a");
 * if (!log) return ERR_LOG;
 *
 * SCOPE_DEFER { if (log) fclose(log); } // runs second
 *
 * // both cleaned up automatically in reverse order
 * ```
 *
 * Arena checkpoint example:
 * ```c
 * ArenaMark mark = arena_mark(&arena);
 * SCOPE_DEFER {
 *     arena_reset_to(&arena, mark);
 * }
 *
 * // all allocations from here are auto-freed on scope exit
 * ```
 *
 * Lock guard pattern:
 * ```c
 * pthread_mutex_lock(&mtx);
 * SCOPE_DEFER { pthread_mutex_unlock(&mtx); }
 *
 * // critical section — mutex always released
 * ```
 *
 * @note Avoid combining with goto that jumps **outside** the current block —
 * such jumps bypass all SCOPE_DEFER statements in that block.
 *
 * @note longjmp / setjmp will also bypass deferred cleanups completely.
 * If longjmp is required, perform cleanup manually before jumping.
 *
 * @see defer (recommended short alias)
 */
#define SCOPE_DEFER \
    for (int _scope_once = 1; _scope_once; _scope_once = 0) \
        for (; _scope_once; ) \
            for (int _scope_done = 0; !_scope_done; _scope_done = 1, _scope_once = 0)

/**
 * @def defer
 * @brief Recommended short alias for SCOPE_DEFER
 *
 * Many developers prefer this shorter, Go/Zig-like name.
 * Enabled by default — disable with #define CANON_NO_DEFER_ALIAS
 *
 * Usage:
 * ```c
 * defer { free(ptr); }
 * ```
 */
#ifndef CANON_NO_DEFER_ALIAS
#define defer SCOPE_DEFER
#endif

/**
 * @def DEFER
 * @brief Alternative uppercase alias (uncomment to enable)
 */
// #define DEFER SCOPE_DEFER

/* ────────────────────────────────────────────────────────────────────────────
   Comparison with other languages
   ────────────────────────────────────────────────────────────────────────────
   Language | Syntax                       | Notes
   ---------|--------------------------------|--------------------------------------
   Go       | defer file.Close()            | Built-in, LIFO, simple
   Zig      | defer file.close();           | Compile-time, very similar
   Swift    | defer { file.close() }        | Very close syntax
   Rust     | (via Drop trait)              | Automatic, type-based
   C++      | (via RAII destructors)        | Most powerful, but requires classes
   C        | defer { fclose(file); }       | Macro-based, zero-cost, pure C
   ──────────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────────
   Complete realistic example
   ────────────────────────────────────────────────────────────────────────────
Result process_config(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return ERR_IO_FAILED;          // ← changed
    defer {
        if (f) fclose(f);
    }

    char* buffer = malloc(8192);
    if (!buffer) return ERR_OUT_OF_MEMORY; // ← changed
    defer {
        free(buffer);
    }

    // Read file into buffer safely
    size_t n = fread(buffer, 1, 8192, f);
    if (n == 0 && ferror(f)) return ERR_IO_FAILED; // ← changed

    ArenaMark mark = arena_mark(&scratch_arena);
    defer {
        arena_reset_to(&scratch_arena, mark);
    }

    // Parse using temporary arena allocations
    Config cfg = parse_config(buffer, n, &scratch_arena);
    return validate_and_apply(&cfg);
}
   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_SCOPE_H */
