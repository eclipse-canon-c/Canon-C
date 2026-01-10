// core/scope.h
#ifndef CANON_CORE_SCOPE_H
#define CANON_CORE_SCOPE_H

/**
 * @file scope.h
 * @brief Deferred cleanup at scope exit (RAII-style pattern for C)
 *
 * Provides a simple, macro-based way to execute cleanup code automatically
 * when leaving the current scope — regardless of how the scope exits:
 *   - normal end of block
 *   - early return
 *   - break / continue
 *   - goto (as long as it doesn't jump over the SCOPE_DEFER)
 *
 * Key properties:
 *   - Zero runtime overhead (pure preprocessor expansion)
 *   - No hidden state or global variables
 *   - No dependencies on compiler extensions (works with standard C99+)
 *   - Very lightweight alternative to full RAII (no classes/structs needed)
 *
 * Limitations & warnings:
 *   - The cleanup block **must not** contain break/continue/goto that jumps
 *     outside the SCOPE_DEFER scope (undefined behavior in C)
 *   - Works best with simple cleanup (free, fclose, unlock, etc.)
 *   - Not suitable for complex control flow or exception-like semantics
 *   - Multiple SCOPE_DEFER in the same scope execute in reverse order
 *     (LIFO — like stack unwinding)
 *
 * Basic usage example:
 *   FILE* f = fopen("file.txt", "r");
 *   if (!f) return -1;
 *   SCOPE_DEFER {
 *       fclose(f);
 *   }
 *   // ... use f ...
 *   // file automatically closed on return/break/end of block
 */

/**
 * @brief Macro that defers execution of a block until scope exit
 *
 * The block following SCOPE_DEFER is executed exactly once when leaving
 * the current compound statement (block), in reverse order of declaration.
 *
 * Implementation uses a clever for-loop trick (no extra runtime cost):
 *   - Outer for: runs once
 *   - Inner for: controls execution of the cleanup block
 *
 * Recommended usage pattern:
 *   ResourceType* res = acquire_resource();
 *   if (!res) return error;
 *   SCOPE_DEFER {
 *       release_resource(res);
 *   }
 *   // safe usage of res here
 */
#define SCOPE_DEFER \
    for (int _scope_once = 1; _scope_once; _scope_once = 0) \
        for (; _scope_once; ) \
            for (int _scope_done = 0; !_scope_done; _scope_done = 1)

/* Alternative nicer-looking name (optional — you can choose either) */
// #define defer SCOPE_DEFER

#endif /* CANON_CORE_SCOPE_H */
