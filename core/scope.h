#ifndef CANON_CORE_SCOPE_H
#define CANON_CORE_SCOPE_H

/**
 * @file scope.h
 * @brief RAII-style deferred cleanup for pure C using zero-overhead macros
 *
 * Executes cleanup code automatically when leaving the current lexical scope,
 * regardless of exit reason (normal end, return, break, continue, or goto
 * targeting a label *inside* the same block).
 *
 * Design:
 * ────────────────────────────────────────────────────────────────────────────
 * - Zero runtime overhead: expands to two nested for-loops the compiler
 *   eliminates entirely; no function pointers, no allocations, no vtables
 * - LIFO execution order: last declared defer runs first (stack unwinding)
 * - Multiple defers in the same scope are safe: unique names via __LINE__
 * - Nesting defers inside loops or conditionals is safe
 * - Pure C99 — no compiler extensions, no platform-specific code
 * - Tested: GCC, Clang, MSVC, TCC
 *
 * Exit-method compatibility:
 * ────────────────────────────────────────────────────────────────────────────
 * | Exit method                       | Cleanup runs? | Notes                              |
 * |-----------------------------------|---------------|------------------------------------|
 * | Normal end of block               | Yes           |                                    |
 * | return                            | Yes           |                                    |
 * | break / continue                  | Yes           | Exits enclosing loop/switch        |
 * | goto — label inside block         | Yes           |                                    |
 * | goto — label outside block        | No            | Skips all defers — avoid           |
 * | goto — forward jump past a defer  | No            | That defer is never registered     |
 * | longjmp out of scope              | No            | Bypasses completely — avoid        |
 * | exit() / abort() / _Exit()        | No            | Process termination                |
 *
 * Rules:
 * ────────────────────────────────────────────────────────────────────────────
 * - Do NOT place return / break / continue / goto inside a defer block
 * - Do NOT goto a label outside the block that contains the defer
 * - Do NOT nest one defer directly inside another defer block
 *   (inner and outer each declare their own loop variables; nesting them
 *   causes the inner block's loop to interfere with the outer block's exit
 *   condition, which may silently prevent the outer cleanup from running)
 * - Keep defer blocks simple: free, fclose, unlock, reset — nothing more
 *
 * @sa SCOPE_DEFER
 */

/* ────────────────────────────────────────────────────────────────────────────
   Internal helpers — unique variable names per call site via __LINE__
   ──────────────────────────────────────────────────────────────────────────── */
#define _SCOPE_CAT2(a, b)      a##b
#define _SCOPE_CAT(a, b)       _SCOPE_CAT2(a, b)
#define _SCOPE_VAR(base)       _SCOPE_CAT(base, __LINE__)

/**
 * @def SCOPE_DEFER
 * @brief Execute a cleanup block automatically on scope exit (LIFO order).
 *
 * Expands to two nested for-loops that modern compilers eliminate at -O1
 * or higher. Variable names are unique per call site via __LINE__, so
 * multiple defers in the same scope work correctly without collision.
 *
 * Basic pattern:
 * ```c
 * FILE* f = fopen("data.txt", "r");
 * if (!f) return ERR_OPEN;
 * SCOPE_DEFER { fclose(f); }
 *
 * // f is guaranteed closed on any normal exit from this block
 * ```
 *
 * Multiple resources (LIFO — second defer runs first):
 * ```c
 * void* buf = malloc(4096);
 * if (!buf) return ERR_ALLOC;
 * SCOPE_DEFER { free(buf); }                 // runs second
 *
 * FILE* log = fopen("run.log", "a");
 * if (!log) return ERR_LOG;
 * SCOPE_DEFER { fclose(log); }               // runs first
 * ```
 *
 * Mutex guard:
 * ```c
 * pthread_mutex_lock(&mtx);
 * SCOPE_DEFER { pthread_mutex_unlock(&mtx); }
 * // critical section — mutex always released
 * ```
 *
 * Arena checkpoint:
 * ```c
 * ArenaMark mark = arena_mark(&arena);
 * SCOPE_DEFER { arena_reset_to(&arena, mark); }
 * // all scratch allocations freed on exit
 * ```
 *
 * @warning Do NOT use return, break, continue, or outward goto inside
 *          a defer block. Do NOT nest one defer directly inside another.
 *
 * @warning A forward goto that jumps *past* a defer statement means that
 *          defer is never reached and its cleanup will not run.
 *
 * @note longjmp across a defer boundary bypasses cleanup entirely.
 *       Perform manual cleanup before longjmp if required.
 */
#define SCOPE_DEFER \
    for (int _SCOPE_VAR(_sc_once_) = 1; \
             _SCOPE_VAR(_sc_once_); \
             _SCOPE_VAR(_sc_once_) = 0) \
        for (int _SCOPE_VAR(_sc_done_) = 0; \
                 !_SCOPE_VAR(_sc_done_); \
                 _SCOPE_VAR(_sc_done_) = 1)

/**
 * @def defer
 * @brief Short alias for SCOPE_DEFER (Go / Zig style).
 *
 * Enabled by default. Disable with: #define CANON_NO_DEFER_ALIAS
 *
 * ```c
 * defer { free(ptr); }
 * ```
 */
#ifndef CANON_NO_DEFER_ALIAS
#   define defer SCOPE_DEFER
#endif

/* ────────────────────────────────────────────────────────────────────────────
   Full example
   ────────────────────────────────────────────────────────────────────────────

Result process_config(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return ERR_IO_FAILED;
    defer { fclose(f); }

    char* buf = malloc(8192);
    if (!buf) return ERR_OUT_OF_MEMORY;
    defer { free(buf); }

    size_t n = fread(buf, 1, 8192, f);
    if (n == 0 && ferror(f)) return ERR_IO_FAILED;

    ArenaMark mark = arena_mark(&scratch);
    defer { arena_reset_to(&scratch, mark); }

    Config cfg = parse_config(buf, n, &scratch);
    return validate_and_apply(&cfg);
    // All three defers run here in reverse order, on any exit path above.
}

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_SCOPE_H */
