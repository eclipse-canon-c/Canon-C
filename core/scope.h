// core/scope.h
#ifndef CANON_CORE_SCOPE_H
#define CANON_CORE_SCOPE_H

/**
 * @file scope.h
 * @brief Deferred cleanup at scope exit – RAII-style pattern for pure C
 *
 * Provides macro-based automatic cleanup when leaving current block,
 * regardless of exit reason (return, break, continue, end of block, goto within scope).
 *
 * =====================================================================
 *                          IMPORTANT SEMANTICS
 * =====================================================================
 *
 * 1. Execution order
 *    • Multiple SCOPE_DEFER statements in the same block are executed in **LIFO** order
 *      (Last declared → executed first – like stack unwinding)
 *
 * 2. When are defers executed?
 *
 *    ┌─────────────────────────────┬──────────────────────────────┐
 *    │ How the block is exited     │ Are deferred blocks run?     │
 *    ├─────────────────────────────┼──────────────────────────────┤
 *    │ Normal end of block         │ YES                          │
 *    │ return from function        │ YES                          │
 *    │ break / continue            │ YES (current block only)     │
 *    │ goto label **inside** block │ YES                          │
 *    │ goto label **outside** block│ NO                           │
 *    │ longjmp out of the block    │ NO                           │
 *    └─────────────────────────────┴──────────────────────────────┘
 *
 *    → goto that jumps **out** of the current SCOPE block **skips** cleanup!
 *      This is standard C behavior (the for-loop trick cannot intercept it).
 *
 * 3. Critical warnings
 *
 *    • Do NOT put break/continue/goto that jumps **outside** the block
 *      inside a SCOPE_DEFER cleanup block → undefined behavior
 *
 *    • longjmp / setjmp will **bypass** deferred cleanups
 *      → If you must longjmp, call cleanup code manually before jump
 *
 *    • Nested SCOPE_DEFER blocks work correctly (each level independent)
 *
 * 4. Best practice recommendations
 *
 *    • Keep cleanup blocks **very simple** (free, fclose, unlock, release...)
 *    • Prefer **small, nested blocks** over huge functions with many defers
 *    • For complex control flow → consider explicit cleanup labels + goto
 *
 * =====================================================================
 *                          Basic usage pattern
 * =====================================================================
 *
 *     FILE *f = fopen("data.txt", "r");
 *     if (!f) return error_open_failed;
 *
 *     SCOPE_DEFER {
 *         fclose(f);
 *     }
 *
 *     // ... safe usage of f here ...
 *     // file is guaranteed closed on any normal exit from this point
 *
 * =====================================================================
 *                          Multiple cleanups (LIFO order)
 * =====================================================================
 *
 *     void *mem = malloc(4096);
 *     if (!mem) return error_alloc;
 *
 *     FILE *log = fopen("log.txt", "a");
 *     if (!log) {
 *         free(mem);
 *         return error_logfile;
 *     }
 *
 *     SCOPE_DEFER { fclose(log); }     // executed LAST
 *     SCOPE_DEFER { free(mem); }       // executed FIRST
 *
 *     // both will be cleaned up automatically in reverse order
 */

#define SCOPE_DEFER \
    for (int _scope_once = 1; _scope_once; _scope_once = 0) \
        for (; _scope_once; ) \
            for (int _scope_done = 0; !_scope_done; _scope_done = 1, _scope_once = 0)

// Optional: many people prefer the shorter name
// #define defer SCOPE_DEFER

#endif /* CANON_CORE_SCOPE_H */
