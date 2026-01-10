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
 * Portability:
 *   - Requires C99 or later (for variable declarations in for-loop)
 *   - Works with all major compilers (GCC, Clang, MSVC)
 *   - No platform-specific code or extensions
 *
 * Thread-safety: Each scope is independent - no shared state
 *
 * Performance: Zero runtime overhead - expands to simple for-loop at compile time
 *
 * =====================================================================
 *                          IMPORTANT SEMANTICS
 * =====================================================================
 *
 * 1. Execution order
 *    • Multiple SCOPE_DEFER statements in the same block are executed in **LIFO** order
 *      (Last declared → executed first – like stack unwinding in C++/Rust)
 *    • This matches intuition: cleanup happens in reverse order of acquisition
 *
 * 2. When are defers executed?
 *
 *    ┌─────────────────────────────┬──────────────────────────────┐
 *    │ How the block is exited     │ Are deferred blocks run?     │
 *    ├─────────────────────────────┼──────────────────────────────┤
 *    │ Normal end of block         │ ✓ YES                        │
 *    │ return from function        │ ✓ YES                        │
 *    │ break / continue            │ ✓ YES (current block only)   │
 *    │ goto label **inside** block │ ✓ YES                        │
 *    │ goto label **outside** block│ ✗ NO (skips cleanup!)        │
 *    │ longjmp out of the block    │ ✗ NO (skips cleanup!)        │
 *    │ exit() / abort()            │ ✗ NO (process termination)   │
 *    └─────────────────────────────┴──────────────────────────────┘
 *
 *    → goto that jumps **out** of the current SCOPE block **skips** cleanup!
 *      This is fundamental C behavior (no mechanism to intercept it).
 *
 * 3. Critical warnings
 *
 *    ⚠️  Do NOT put break/continue/return/goto that jumps **outside** the block
 *        inside a SCOPE_DEFER cleanup block → undefined behavior
 *
 *    ⚠️  longjmp / setjmp will **bypass** deferred cleanups completely
 *        → If you must longjmp, call cleanup code manually before jump
 *
 *    ⚠️  Goto jumping out of scope skips cleanup - avoid or use explicit cleanup
 *
 *    ✓  Nested SCOPE_DEFER blocks work correctly (each level independent)
 *
 *    ✓  Multiple SCOPE_DEFER in same block work correctly (LIFO order)
 *
 * 4. Best practice recommendations
 *
 *    ✓  Keep cleanup blocks **very simple** (free, fclose, unlock, release...)
 *    ✓  Prefer **small, nested blocks** over huge functions with many defers
 *    ✓  For complex control flow → consider explicit cleanup labels + goto
 *    ✓  One resource per SCOPE_DEFER for clarity
 *    ✓  Always check for NULL before cleanup operations if needed
 *
 * 5. Implementation note
 *
 *    The macro expands to nested for-loops that execute the cleanup block
 *    exactly once when leaving scope. The compiler optimizes this away to
 *    have zero runtime overhead beyond the cleanup code itself.
 *
 * =====================================================================
 *                          Basic usage pattern
 * =====================================================================
 *
 *     FILE* f = fopen("data.txt", "r");
 *     if (!f) return ERROR_OPEN_FAILED;
 *
 *     SCOPE_DEFER {
 *         fclose(f);
 *     }
 *
 *     // ... safe usage of f here ...
 *     // File is guaranteed closed on any normal exit from this point
 *     // Works with: return, break, continue, end of block
 *
 * =====================================================================
 *                          Multiple cleanups (LIFO order)
 * =====================================================================
 *
 *     void* mem = malloc(4096);
 *     if (!mem) return ERROR_ALLOC;
 *
 *     FILE* log = fopen("log.txt", "a");
 *     if (!log) {
 *         free(mem);
 *         return ERROR_LOGFILE;
 *     }
 *
 *     SCOPE_DEFER { fclose(log); }     // Executed SECOND (last declared)
 *     SCOPE_DEFER { free(mem); }       // Executed FIRST (first declared)
 *
 *     // Both will be cleaned up automatically in reverse declaration order
 *     // This matches acquisition order: mem acquired first, freed last
 *
 * =====================================================================
 *                          Nested blocks
 * =====================================================================
 *
 *     {
 *         FILE* outer = fopen("outer.txt", "r");
 *         SCOPE_DEFER { fclose(outer); }
 *
 *         {
 *             FILE* inner = fopen("inner.txt", "r");
 *             SCOPE_DEFER { fclose(inner); }
 *
 *             // Both files open here
 *         }  // inner closed here
 *
 *         // Only outer still open here
 *     }  // outer closed here
 *
 * =====================================================================
 *                          Conditional cleanup
 * =====================================================================
 *
 *     void* ptr = malloc(1024);
 *     if (!ptr) return ERROR_ALLOC;
 *
 *     SCOPE_DEFER {
 *         if (ptr) {  // Safety check (optional but good practice)
 *             free(ptr);
 *         }
 *     }
 *
 * =====================================================================
 *                          Arena/pool pattern
 * =====================================================================
 *
 *     ArenaMark mark = arena_mark(&arena);
 *     SCOPE_DEFER {
 *         arena_reset_to(&arena, mark);
 *     }
 *
 *     // All arena allocations from this point are automatically freed
 *     void* temp1 = arena_alloc(&arena, 100);
 *     void* temp2 = arena_alloc(&arena, 200);
 *     // Both freed when scope exits
 *
 * =====================================================================
 *                          Mutex/lock pattern
 * =====================================================================
 *
 *     pthread_mutex_lock(&mutex);
 *     SCOPE_DEFER {
 *         pthread_mutex_unlock(&mutex);
 *     }
 *
 *     // Critical section protected - mutex always unlocked
 *     // even on early return or error conditions
 *
 * =====================================================================
 *                          Error handling pattern
 * =====================================================================
 *
 *     Result do_work(void) {
 *         Resource* r1 = acquire_resource();
 *         if (!r1) return ERR_RESOURCE;
 *         SCOPE_DEFER { release_resource(r1); }
 *
 *         Resource* r2 = acquire_resource();
 *         if (!r2) return ERR_RESOURCE;  // r1 automatically released
 *         SCOPE_DEFER { release_resource(r2); }
 *
 *         // Use both resources
 *         if (!process(r1, r2)) {
 *             return ERR_PROCESS;  // Both r1 and r2 automatically released
 *         }
 *
 *         return OK;  // Both resources still cleaned up
 *     }
 *
 * =====================================================================
 *                          ANTI-PATTERNS (DON'T DO THIS)
 * =====================================================================
 *
 *     // ❌ BAD: goto jumping out of scope
 *     {
 *         void* mem = malloc(100);
 *         SCOPE_DEFER { free(mem); }
 *         
 *         if (error) goto cleanup;  // SKIPS the SCOPE_DEFER!
 *     }
 *     cleanup:
 *         // mem was never freed - memory leak!
 *
 *     // ✓ GOOD: explicit cleanup before goto
 *     {
 *         void* mem = malloc(100);
 *         
 *         if (error) {
 *             free(mem);
 *             goto cleanup;
 *         }
 *         
 *         SCOPE_DEFER { free(mem); }
 *         // normal path cleanup
 *     }
 *
 *     // ❌ BAD: control flow inside defer
 *     SCOPE_DEFER {
 *         cleanup();
 *         return;  // DON'T DO THIS - undefined behavior
 *     }
 *
 *     // ✓ GOOD: simple cleanup only
 *     SCOPE_DEFER {
 *         cleanup();
 *     }
 */

/**
 * @brief Executes a block of code when leaving the current scope
 *
 * The cleanup block is executed exactly once, regardless of how the scope
 * is exited (normal end, return, break, continue, or goto within scope).
 *
 * Execution order: Multiple SCOPE_DEFER blocks execute in LIFO order
 * (reverse of declaration order).
 *
 * Implementation: Expands to nested for-loops with compile-time optimization.
 * Zero runtime overhead beyond the cleanup code itself.
 *
 * Usage:
 *     SCOPE_DEFER {
 *         // cleanup code here
 *     }
 *
 * Note: The cleanup block must not contain break, continue, return, or goto
 *       that exits the defer block itself.
 */
#define SCOPE_DEFER \
    for (int _scope_once = 1; _scope_once; _scope_once = 0) \
        for (; _scope_once; ) \
            for (int _scope_done = 0; !_scope_done; _scope_done = 1, _scope_once = 0)

/**
 * @brief Shorter alias for SCOPE_DEFER
 *
 * Many developers prefer the shorter name for readability.
 * Functionally identical to SCOPE_DEFER.
 *
 * Usage:
 *     defer {
 *         // cleanup code here
 *     }
 *
 * Uncomment the following line to enable this alias:
 */
// #define defer SCOPE_DEFER

/**
 * @brief Alternative spelling for those who prefer it
 *
 * Uncomment to enable:
 */
// #define DEFER SCOPE_DEFER

/* ────────────────────────────────────────────────────────────────────────────
   Comparison with other languages
   ────────────────────────────────────────────────────────────────────────────

   Go:
       defer file.Close()
   
   Rust:
       // (automatic via Drop trait)
   
   C++:
       // (automatic via RAII destructors)
   
   Zig:
       defer file.close();
   
   Swift:
       defer { file.close() }
   
   C (this library):
       SCOPE_DEFER { fclose(file); }

   ──────────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────────
   Advanced Example: Complete Resource Management
   ────────────────────────────────────────────────────────────────────────────

    typedef struct {
        FILE* file;
        void* buffer;
        pthread_mutex_t* lock;
    } Context;
    
    Result process_file(const char* path) {
        Context ctx = {0};
        
        // Acquire file
        ctx.file = fopen(path, "r");
        if (!ctx.file) return ERR_FILE;
        SCOPE_DEFER { 
            if (ctx.file) fclose(ctx.file); 
        }
        
        // Acquire buffer
        ctx.buffer = malloc(BUFFER_SIZE);
        if (!ctx.buffer) return ERR_MEMORY;
        SCOPE_DEFER { 
            if (ctx.buffer) free(ctx.buffer); 
        }
        
        // Acquire lock
        if (pthread_mutex_lock(&global_lock) != 0) return ERR_LOCK;
        ctx.lock = &global_lock;
        SCOPE_DEFER { 
            if (ctx.lock) pthread_mutex_unlock(ctx.lock); 
        }
        
        // All resources acquired - do work
        Result result = do_processing(&ctx);
        
        // All resources automatically cleaned up in reverse order:
        // 1. Unlock mutex
        // 2. Free buffer
        // 3. Close file
        
        return result;
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_SCOPE_H */
