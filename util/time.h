// util/time.h
#ifndef CANON_UTIL_TIME_H
#define CANON_UTIL_TIME_H

#include <stdint.h>

// Platform-specific headers for time measurement
#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

/**
 * @file time.h
 * @brief Lightweight, high-resolution monotonic stopwatch
 *
 * Provides simple time interval measurement using monotonic clocks that are
 * unaffected by system time adjustments. Implements a minimal stopwatch
 * abstraction with nanosecond precision on supported platforms.
 *
 * Portability:
 *   - Requires C99 or later (for uint64_t, inline functions)
 *   - Windows: Uses QueryPerformanceCounter API (Windows 2000+)
 *   - POSIX: Uses clock_gettime with CLOCK_MONOTONIC (POSIX.1-2001)
 *   - macOS/BSD: CLOCK_MONOTONIC supported since macOS 10.12
 *   - Linux: Fully supported (kernel 2.6.28+)
 *   - Compilation fails on unsupported platforms (intentional - add support)
 *
 * Thread-safety: Functions are reentrant. Each Stopwatch instance is independent.
 *                Safe to use different instances from different threads. Not
 *                safe if multiple threads share one Stopwatch without locking.
 *
 * Performance:
 *   - Time complexity: O(1) - constant time for all operations
 *   - Space complexity: O(1) - 8 bytes per Stopwatch instance
 *   - Start operation: Single syscall (clock query)
 *   - Elapsed operation: Single syscall + subtraction
 *   - Typical overhead: ~20-100 nanoseconds per measurement
 *   - No allocations, no locking, minimal overhead
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Monotonic time — unaffected by system clock changes, NTP, DST, sleep
 * - Simple model — just start and measure, no pause/resume complexity
 * - Explicit control — caller decides when to start/restart
 * - Zero global state — each instance fully independent
 * - Nanosecond precision — where hardware/OS support it
 * - Null-safe — graceful handling of NULL pointers
 * - Lightweight — single uint64_t per instance
 * - Cross-platform — consistent API across Windows/POSIX
 * - No hidden costs — what you see is what you get
 * - Overflow-safe for practical use (584+ years)
 *
 * Monotonic vs wall-clock time:
 * ────────────────────────────────────────────────────────────────────────────
 * This library uses MONOTONIC time, not wall-clock time. Understanding the
 * difference is important:
 *
 * WALL-CLOCK TIME (time(), gettimeofday()):
 * - Represents "real world" time (date/time)
 * - Can jump forward or backward (NTP sync, manual adjustment, DST)
 * - Can go backwards (negative elapsed time!)
 * - Affected by system sleep/hibernate
 * - Use for: timestamps, logging, date/time display
 * - DON'T use for: measuring intervals, timeouts, performance
 *
 * MONOTONIC TIME (this library):
 * - Represents time since arbitrary point (usually boot)
 * - Always increases (never jumps backward)
 * - Unaffected by system time changes
 * - Continues during sleep on most platforms
 * - Use for: measuring intervals, timeouts, benchmarking
 * - DON'T use for: timestamps, displaying time to users
 *
 * Example why monotonic matters:
 * ```c
 * // WRONG - using wall-clock time
 * time_t start = time(NULL);
 * expensive_operation();
 * time_t elapsed = time(NULL) - start;
 * // If NTP adjusted clock backward during operation,
 * // elapsed could be NEGATIVE!
 * 
 * // RIGHT - using monotonic time
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * expensive_operation();
 * uint64_t elapsed = stopwatch_elapsed_ns(&sw);
 * // Always correct, even if system time changes
 * ```
 *
 * Timer resolution and precision:
 * ────────────────────────────────────────────────────────────────────────────
 * While this API reports nanoseconds, actual resolution varies by platform:
 *
 * Windows (QueryPerformanceCounter):
 * - Resolution: ~100 nanoseconds typical (hardware-dependent)
 * - Frequency: Usually 10 MHz (some systems 3.5 MHz)
 * - Very stable on modern Windows (XP+)
 * - Per-processor consistency guaranteed (Windows 7+)
 *
 * Linux (CLOCK_MONOTONIC):
 * - Resolution: ~1-20 nanoseconds typical (depends on clocksource)
 * - Best clocksource: TSC (Time Stamp Counter) on modern x86
 * - Fallback: hpet, acpi_pm (lower resolution)
 * - Check: cat /sys/devices/system/clocksource/clocksource0/current_clocksource
 *
 * macOS (CLOCK_MONOTONIC):
 * - Resolution: ~1 nanosecond (Mach absolute time)
 * - Very high precision on modern Macs
 * - Based on CPU frequency
 *
 * Practical implications:
 * - Don't rely on exact nanosecond accuracy
 * - Good for microsecond-level timing and above
 * - For sub-microsecond, understand your platform
 * - Resolution != accuracy (systematic errors possible)
 *
 * Overflow considerations:
 * ────────────────────────────────────────────────────────────────────────────
 * Uses uint64_t for nanosecond storage:
 *
 * Maximum representable time:
 * - 2^64 nanoseconds = 18,446,744,073,709,551,615 ns
 * - = ~584.5 years
 * - = ~213,503 days
 *
 * Practical overflow scenarios:
 * - Single measurement overflow: Impossible (would need 584 year operation)
 * - Cumulative overflow: Only if you sum measurements for 584+ years
 * - Start timestamp overflow: Depends on platform epoch (usually system boot)
 *
 * Conclusion: Overflow is not a practical concern for any reasonable use case.
 *
 * Why no pause/resume?
 * ────────────────────────────────────────────────────────────────────────────
 * This library intentionally omits pause/resume functionality for simplicity.
 * You can implement it manually if needed:
 *
 * ```c
 * // Manual pause pattern
 * Stopwatch sw;
 * uint64_t accumulated = 0;
 * 
 * // First segment
 * stopwatch_start(&sw);
 * work_segment_1();
 * accumulated += stopwatch_elapsed_ns(&sw);
 * 
 * // Pause (do something else, not timed)
 * other_work();
 * 
 * // Resume
 * stopwatch_start(&sw);
 * work_segment_2();
 * accumulated += stopwatch_elapsed_ns(&sw);
 * 
 * printf("Total work time: %llu ns\n", accumulated);
 * ```
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Performance benchmarking
 * - Algorithm profiling
 * - Operation timeout implementation
 * - Request/response time measurement
 * - Frame time calculation (games, animation)
 * - Network latency measurement
 * - Cache timing analysis
 * - Workload characterization
 * - A/B performance testing
 * - Regression detection
 * - SLA monitoring
 * - Rate limiting
 *
 * NOT suitable for:
 * - Displaying time to users (use wall-clock time)
 * - Scheduling at specific times (use wall-clock + timer)
 * - Very short intervals (<100 ns) - precision limited
 * - Cross-system synchronization (use NTP/PTP)
 * - Persistent timing across reboots (epoch resets)
 *
 * Platform-specific notes:
 * ────────────────────────────────────────────────────────────────────────────
 * Windows:
 * - Requires Windows 2000 or later
 * - QueryPerformanceCounter is very reliable on modern Windows
 * - Old systems (pre-Vista) had per-core issues (mostly fixed)
 * - Frequency is cached per query (slight overhead)
 *
 * Linux:
 * - CLOCK_MONOTONIC doesn't include time suspended (use CLOCK_BOOTTIME if needed)
 * - Clocksource can be changed at runtime (/sys/devices/system/clocksource/)
 * - TSC-based timing is fastest but requires invariant TSC support
 * - vDSO optimization makes clock_gettime very fast (no syscall)
 *
 * macOS:
 * - CLOCK_MONOTONIC added in macOS 10.12 Sierra
 * - Earlier versions would need mach_absolute_time()
 * - Very high resolution on modern Macs
 *
 * Usage examples:
 *
 * Basic timing:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * expensive_operation();
 * 
 * uint64_t ns = stopwatch_elapsed_ns(&sw);
 * double sec = stopwatch_elapsed_sec(&sw);
 * 
 * printf("Operation took %llu ns (%.6f seconds)\n", ns, sec);
 * ```
 *
 * Measuring multiple phases:
 * ```c
 * Stopwatch sw;
 * 
 * stopwatch_start(&sw);
 * phase_1();
 * uint64_t phase1_time = stopwatch_elapsed_ns(&sw);
 * 
 * stopwatch_start(&sw);  // Restart for phase 2
 * phase_2();
 * uint64_t phase2_time = stopwatch_elapsed_ns(&sw);
 * 
 * printf("Phase 1: %llu ns\n", phase1_time);
 * printf("Phase 2: %llu ns\n", phase2_time);
 * ```
 */

/**
 * @brief Stopwatch instance (minimal state)
 *
 * Contains only the start timestamp in nanoseconds using monotonic time.
 * Zero-initialized state means "not started" (will report 0 elapsed time).
 *
 * Size: 8 bytes (single uint64_t)
 *
 * Fields:
 * - start: Monotonic nanoseconds when stopwatch_start() was called
 *          Value is platform-dependent (usually nanoseconds since boot)
 *          Zero means stopwatch hasn't been started
 *
 * Lifecycle:
 * 1. Declare: Stopwatch sw;
 * 2. Start: stopwatch_start(&sw);
 * 3. Measure: stopwatch_elapsed_ns(&sw) or stopwatch_elapsed_sec(&sw)
 * 4. Restart: stopwatch_start(&sw) again to reset
 * 5. No cleanup needed (no resources to free)
 *
 * Example:
 * ```c
 * Stopwatch sw = {0};  // Zero-init (optional, local vars may be uninitialized)
 * stopwatch_start(&sw);
 * // ... work ...
 * printf("Elapsed: %llu ns\n", stopwatch_elapsed_ns(&sw));
 * ```
 *
 * Thread-safety:
 * - Each Stopwatch instance is independent
 * - Safe to use different instances from different threads
 * - NOT safe to share one instance across threads without locking
 *
 * Memory:
 * - Can be stack-allocated, heap-allocated, or embedded in structs
 * - No internal pointers or resources
 * - Copyable (copies are independent stopwatches)
 */
typedef struct {
    uint64_t start;  ///< Monotonic nanoseconds at start (0 = not started)
} Stopwatch;

/* ────────────────────────────────────────────────────────────────────────────
   Stopwatch control
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Starts (or restarts) the stopwatch
 *
 * Records the current monotonic time as the start point. Calling this
 * multiple times effectively resets the stopwatch - elapsed time will be
 * measured from the most recent call to stopwatch_start().
 *
 * Algorithm:
 *   - Validate input (check for NULL)
 *   - Query platform monotonic clock
 *   - Convert to nanoseconds
 *   - Store in sw->start
 *
 * Platform implementations:
 *   - Windows: QueryPerformanceCounter + frequency conversion
 *   - POSIX: clock_gettime(CLOCK_MONOTONIC)
 *
 * @param sw Valid Stopwatch pointer (NULL-safe: does nothing)
 *
 * Preconditions:
 *   - sw should point to valid Stopwatch struct (or be NULL)
 *
 * Postconditions:
 *   - If sw is not NULL: sw->start contains current monotonic time in ns
 *   - If sw is NULL: no-op, function returns immediately
 *   - Previous start time is overwritten (stopwatch reset)
 *
 * Performance:
 *   - Time: O(1) - single syscall or CPU instruction
 *   - Space: O(1) - no allocations
 *   - Overhead: ~20-100 nanoseconds typical
 *
 * Thread-safety: Safe if sw is not shared across threads
 *
 * Example - Basic start:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * // Stopwatch is now running
 * ```
 *
 * Example - Restart:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * work_phase_1();
 * uint64_t phase1 = stopwatch_elapsed_ns(&sw);
 * 
 * stopwatch_start(&sw);  // Restart - phase1 time is saved above
 * work_phase_2();
 * uint64_t phase2 = stopwatch_elapsed_ns(&sw);
 * ```
 *
 * Example - NULL safety:
 * ```c
 * Stopwatch* sw = NULL;
 * stopwatch_start(sw);  // Safe - no crash
 * ```
 *
 * Example - Multiple instances:
 * ```c
 * Stopwatch sw1, sw2;
 * stopwatch_start(&sw1);
 * // ... work 1 ...
 * stopwatch_start(&sw2);
 * // ... work 2 ...
 * // Both stopwatches running independently
 * ```
 *
 * Common use cases:
 * - Starting a new timing measurement
 * - Resetting timing between phases
 * - Beginning a timeout countdown
 * - Synchronizing multiple timers
 */
static inline void stopwatch_start(Stopwatch* sw) {
    if (!sw) {
        return;
    }

#ifdef _WIN32
    // Windows: Use QueryPerformanceCounter
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    
    // Convert to nanoseconds
    sw->start = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    // POSIX: Use clock_gettime with CLOCK_MONOTONIC
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    // Convert to nanoseconds
    sw->start = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* ────────────────────────────────────────────────────────────────────────────
   Elapsed time queries
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns elapsed time since start in nanoseconds
 *
 * Measures the time elapsed since the most recent call to stopwatch_start().
 * Safe to call multiple times - each call re-measures from the same start point.
 *
 * Algorithm:
 *   - Validate input (check for NULL and started state)
 *   - Query current monotonic time
 *   - Convert to nanoseconds
 *   - Subtract start time
 *   - Return difference
 *
 * @param sw Valid Stopwatch pointer (NULL or not started → returns 0)
 *
 * @return   Elapsed nanoseconds since last stopwatch_start() call
 *           Returns 0 if sw is NULL or never started
 *
 * Preconditions:
 *   - For meaningful result: stopwatch_start() should have been called
 *
 * Postconditions:
 *   - Return value represents monotonic time interval
 *   - Stopwatch state unchanged (non-destructive read)
 *   - Safe to call multiple times
 *
 * Return value properties:
 *   - Always >= 0 (monotonic time never goes backward)
 *   - Increases on subsequent calls (unless stopwatch restarted)
 *   - Resolution: platform-dependent (typically 1-100 ns)
 *   - Accuracy: depends on platform timer quality
 *
 * Performance:
 *   - Time: O(1) - single syscall + subtraction
 *   - Space: O(1) - no allocations
 *   - Overhead: ~20-100 nanoseconds typical
 *
 * Thread-safety: Safe to call from any thread (read-only operation)
 *
 * Example - Basic measurement:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * expensive_operation();
 * 
 * uint64_t elapsed = stopwatch_elapsed_ns(&sw);
 * printf("Operation took %llu nanoseconds\n", elapsed);
 * ```
 *
 * Example - Multiple measurements:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * step1();
 * printf("After step1: %llu ns\n", stopwatch_elapsed_ns(&sw));
 * 
 * step2();
 * printf("After step1+step2: %llu ns\n", stopwatch_elapsed_ns(&sw));
 * 
 * step3();
 * printf("Total: %llu ns\n", stopwatch_elapsed_ns(&sw));
 * ```
 *
 * Example - Converting to other units:
 * ```c
 * uint64_t ns = stopwatch_elapsed_ns(&sw);
 * uint64_t us = ns / 1000;           // Microseconds
 * uint64_t ms = ns / 1000000;        // Milliseconds
 * double sec = ns / 1000000000.0;    // Seconds
 * ```
 *
 * Example - Timeout detection:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * const uint64_t TIMEOUT_NS = 5000000000ULL;  // 5 seconds
 * 
 * while (true) {
 *     if (stopwatch_elapsed_ns(&sw) > TIMEOUT_NS) {
 *         printf("Operation timed out\n");
 *         break;
 *     }
 *     
 *     if (try_operation()) {
 *         break;  // Success
 *     }
 * }
 * ```
 *
 * Example - Frame timing (games):
 * ```c
 * Stopwatch frame_timer;
 * 
 * while (running) {
 *     stopwatch_start(&frame_timer);
 *     
 *     update_game();
 *     render_game();
 *     
 *     uint64_t frame_ns = stopwatch_elapsed_ns(&frame_timer);
 *     uint64_t fps = 1000000000ULL / frame_ns;
 *     printf("FPS: %llu\n", fps);
 * }
 * ```
 *
 * Example - Benchmarking:
 * ```c
 * Stopwatch sw;
 * const int ITERATIONS = 1000;
 * 
 * stopwatch_start(&sw);
 * for (int i = 0; i < ITERATIONS; i++) {
 *     function_to_benchmark();
 * }
 * uint64_t total_ns = stopwatch_elapsed_ns(&sw);
 * uint64_t avg_ns = total_ns / ITERATIONS;
 * 
 * printf("Average time: %llu ns per call\n", avg_ns);
 * ```
 *
 * Example - NULL safety:
 * ```c
 * Stopwatch* sw = NULL;
 * uint64_t elapsed = stopwatch_elapsed_ns(sw);  // Returns 0, no crash
 * ```
 *
 * Common use cases:
 * - Performance measurement
 * - Timeout implementation
 * - Progress monitoring
 * - Rate limiting
 * - Profiling
 * - Latency tracking
 */
static inline uint64_t stopwatch_elapsed_ns(const Stopwatch* sw) {
    // Handle NULL or unstarted stopwatch
    if (!sw || sw->start == 0) {
        return 0;
    }

#ifdef _WIN32
    // Windows: Query current time
    LARGE_INTEGER freq, counter;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&freq);
    uint64_t now = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    // POSIX: Query current time
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif

    // Calculate elapsed time (always positive due to monotonic clock)
    return now - sw->start;
}

/**
 * @brief Returns elapsed time since start in seconds (floating-point)
 *
 * Convenience function that returns elapsed time as seconds with fractional
 * part. Internally calls stopwatch_elapsed_ns() and divides by 1 billion.
 *
 * This is useful when you want human-readable time values or when working
 * with APIs that expect seconds.
 *
 * Algorithm:
 *   - Call stopwatch_elapsed_ns()
 *   - Convert to double
 *   - Divide by 1,000,000,000
 *
 * @param sw Valid Stopwatch pointer (NULL or not started → returns 0.0)
 *
 * @return   Elapsed time in seconds as double
 *           Returns 0.0 if sw is NULL or never started
 *
 * Preconditions:
 *   - For meaningful result: stopwatch_start() should have been called
 *
 * Postconditions:
 *   - Return value represents time in seconds
 *   - Fractional part represents sub-second precision
 *   - Stopwatch state unchanged
 *
 * Precision notes:
 *   - double has ~15-17 decimal digits of precision
 *   - Can represent nanoseconds accurately up to ~104 days
 *   - Beyond that, sub-nanosecond precision is lost
 *   - Still accurate to microseconds for years
 *
 * Performance:
 *   - Same as stopwatch_elapsed_ns() plus one division
 *   - Division by constant is optimized by compiler
 *
 * Thread-safety: Safe to call from any thread (read-only operation)
 *
 * Example - Basic usage:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * expensive_operation();
 * 
 * double seconds = stopwatch_elapsed_sec(&sw);
 * printf("Operation took %.6f seconds\n", seconds);
 * ```
 *
 * Example - High precision display:
 * ```c
 * double sec = stopwatch_elapsed_sec(&sw);
 * printf("Time: %.9f seconds\n", sec);  // 9 decimal places (nanosecond)
 * printf("Time: %.6f seconds\n", sec);  // 6 decimal places (microsecond)
 * printf("Time: %.3f seconds\n", sec);  // 3 decimal places (millisecond)
 * ```
 *
 * Example - Comparing against threshold:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * operation();
 * 
 * if (stopwatch_elapsed_sec(&sw) > 1.0) {
 *     printf("Operation took longer than 1 second\n");
 * }
 * ```
 *
 * Example - Rate calculation:
 * ```c
 * const int ITEMS = 10000;
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * for (int i = 0; i < ITEMS; i++) {
 *     process_item(i);
 * }
 * 
 * double seconds = stopwatch_elapsed_sec(&sw);
 * double rate = ITEMS / seconds;
 * printf("Processing rate: %.2f items/sec\n", rate);
 * ```
 *
 * Example - Progress reporting:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * for (int i = 0; i < 100; i++) {
 *     process_chunk(i);
 *     
 *     if (i % 10 == 0) {
 *         double elapsed = stopwatch_elapsed_sec(&sw);
 *         printf("Progress: %d%% (%.2f seconds)\n", i, elapsed);
 *     }
 * }
 * ```
 *
 * Example - Logging with timestamps:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * 
 * log_event("Start");
 * step1();
 * log_event("Step 1 complete at %.3f sec", stopwatch_elapsed_sec(&sw));
 * step2();
 * log_event("Step 2 complete at %.3f sec", stopwatch_elapsed_sec(&sw));
 * ```
 *
 * Common use cases:
 * - Human-readable timing output
 * - Rate calculations (items/second)
 * - Timeout checking (in seconds)
 * - Performance reporting
 * - Progress estimation
 * - Logging with time offsets
 */
static inline double stopwatch_elapsed_sec(const Stopwatch* sw) {
    return (double)stopwatch_elapsed_ns(sw) / 1e9;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/time.h"
    #include <stdio.h>
    
    // Example 1: Basic timing
    void example_basic_timing(void) {
        Stopwatch sw;
        stopwatch_start(&sw);
        
        // Simulate work
        for (int i = 0; i < 1000000; i++) {
            // work...
        }
        
        uint64_t ns = stopwatch_elapsed_ns(&sw);
        double sec = stopwatch_elapsed_sec(&sw);
        
        printf("Elapsed: %llu ns (%.6f seconds)\n", ns, sec);
    }
    
    // Example 2: Phase timing
    void example_phase_timing(void) {
        Stopwatch sw;
        
        // Phase 1
        stopwatch_start(&sw);
        initialization();
        uint64_t init_time = stopwatch_elapsed_ns(&sw);
        
        // Phase 2
        stopwatch_start(&sw);
        processing();
        uint64_t proc_time = stopwatch_elapsed_ns(&sw);
        
        // Phase 3
        stopwatch_start(&sw);
        cleanup();
        uint64_t cleanup_time = stopwatch_elapsed_ns(&sw);
        
        printf("Initialization: %llu ns\n", init_time);
        printf("Processing: %llu ns\n", proc_time);
        printf("Cleanup: %llu ns\n", cleanup_time);
    }
    
    // Example 3: Benchmarking
    void example_benchmark(void) {
        const int ITERATIONS = 10000;
        Stopwatch sw;
        
        stopwatch_start(&sw);
        for (int i = 0; i < ITERATIONS; i++) {
            function_to_benchmark();
        }
        uint64_t total_ns = stopwatch_elapsed_ns(&sw);
        
        uint64_t avg_ns = total_ns / ITERATIONS;
        printf("Average: %llu ns per call\n", avg_ns);
        printf("Throughput: %.2f calls/sec\n", 
               1e9 / avg_ns);
    }
    
    // Example 4: Timeout implementation
    void example_timeout(void) {
        Stopwatch sw;
        stopwatch_start(&sw);
        
        const uint64_t TIMEOUT_NS = 5000000000ULL;  // 5 seconds
        
        while (true) {
            if (stopwatch_elapsed_ns(&sw) > TIMEOUT_NS) {
                printf("Operation timed out\n");
                break;
            }
            
            if (try_operation()) {
                printf("Operation succeeded\n");
                break;
            }
        }
    }
    
    // Example 5: Frame timing (game loop)
    void example_frame_timing(void) {
        Stopwatch frame_timer;
        int frame_count = 0;
        
        while (frame_count < 60) {
            stopwatch_start(&frame_timer);
            
            update_game_state();
            render_frame();
            
            uint64_t frame_ns = stopwatch_elapsed_ns(&frame_timer);
            double fps = 1.0 / stopwatch_elapsed_sec(&frame_timer);
            
            printf("Frame %d: %llu ns (%.2f FPS)\n", 
                   frame_count, frame_ns, fps);
            
            frame_count++;
        }
    }
    
    // Example 6: Progress reporting
    void example_progress(void) {
        const int TOTAL = 100;
        Stopwatch sw;
        stopwatch_start(&sw);
        
        for (int i = 0; i < TOTAL; i++) {
            process_item(i);
            
            if (i % 10 == 0) {
                double elapsed = stopwatch_elapsed_sec(&sw);
                double eta = (elapsed / (i + 1)) * (TOTAL - i - 1);
                printf("Progress: %d%% (%.2fs elapsed, %.2fs remaining)\n",
                       (i * 100) / TOTAL, elapsed, eta);
            }
        }
    }
    
    // Example 7: Multiple timers
    void example_multiple_timers(void) {
        Stopwatch cpu_timer, io_timer;
        
        stopwatch_start(&cpu_timer);
        stopwatch_start(&io_timer);
        
        // CPU-bound work
        compute();
        uint64_t cpu_time = stopwatch_elapsed_ns(&cpu_timer);
        
        // I/O-bound work
        stopwatch_start(&io_timer);
        read_file();
        uint64_t io_time = stopwatch_elapsed_ns(&io_timer);
        
        printf("CPU: %llu ns, I/O: %llu ns\n", cpu_time, io_time);
    }
    
    // Example 8: Cumulative timing (manual pause)
    void example_cumulative_timing(void) {
        Stopwatch sw;
        uint64_t total = 0;
        
        for (int i = 0; i < 5; i++) {
            stopwatch_start(&sw);
            work_segment(i);
            total += stopwatch_elapsed_ns(&sw);
            
            // Pause (other work, not timed)
            other_work();
        }
        
        printf("Total work time: %llu ns\n", total);
    }
    
    // Example 9: Rate limiting
    void example_rate_limiting(void) {
        Stopwatch sw;
        const uint64_t MIN_INTERVAL_NS = 100000000ULL;  // 100ms
        
        while (running) {
            stopwatch_start(&sw);
            
            process_request();
            
            uint64_t elapsed = stopwatch_elapsed_ns(&sw);
            if (elapsed < MIN_INTERVAL_NS) {
                uint64_t sleep_ns = MIN_INTERVAL_NS - elapsed;
                sleep_nanoseconds(sleep_ns);
            }
        }
    }
    
    // Example 10: Performance comparison
    void example_performance_comparison(void) {
        Stopwatch sw;
        
        // Test algorithm 1
        stopwatch_start(&sw);
        algorithm_1();
        uint64_t time1 = stopwatch_elapsed_ns(&sw);
        
        // Test algorithm 2
        stopwatch_start(&sw);
        algorithm_2();
        uint64_t time2 = stopwatch_elapsed_ns(&sw);
        
        printf("Algorithm 1: %llu ns\n", time1);
        printf("Algorithm 2: %llu ns\n", time2);
        printf("Speedup: %.2fx\n", (double)time1 / time2);
    }
    
    // Example 11: Latency tracking
    void example_latency_tracking(void) {
        Stopwatch sw;
        uint64_t latencies[100];
        
        for (int i = 0; i < 100; i++) {
            stopwatch_start(&sw);
            send_and_receive();
            latencies[i] = stopwatch_elapsed_ns(&sw);
        }
        
        // Calculate statistics
        uint64_t sum = 0, min = UINT64_MAX, max = 0;
        for (int i = 0; i < 100; i++) {
            sum += latencies[i];
            if (latencies[i] < min) min = latencies[i];
            if (latencies[i] > max) max = latencies[i];
        }
        
        printf("Min: %llu ns, Max: %llu ns, Avg: %llu ns\n",
               min, max, sum / 100);
    }
    
    // Example 12: Profiling sections
    void example_profiling(void) {
        Stopwatch sw;
        
        stopwatch_start(&sw);
        section_1();
        printf("Section 1: %.6f sec\n", stopwatch_elapsed_sec(&sw));
        
        stopwatch_start(&sw);
        section_2();
        printf("Section 2: %.6f sec\n", stopwatch_elapsed_sec(&sw));
        
        stopwatch_start(&sw);
        section_3();
        printf("Section 3: %.6f sec\n", stopwatch_elapsed_sec(&sw));
    }
    
    // Example 13: Incremental timing
    void example_incremental(void) {
        Stopwatch sw;
        stopwatch_start(&sw);
        
        step1();
        printf("After step 1: %.3f sec\n", stopwatch_elapsed_sec(&sw));
        
        step2();
        printf("After step 2: %.3f sec\n", stopwatch_elapsed_sec(&sw));
        
        step3();
        printf("After step 3: %.3f sec\n", stopwatch_elapsed_sec(&sw));
    }
    
    // Example 14: Adaptive timing
    void example_adaptive_timing(void) {
        Stopwatch sw;
        uint64_t target_ns = 16666666ULL;  // 60 FPS
        
        while (running) {
            stopwatch_start(&sw);
            
            update_and_render();
            
            uint64_t elapsed = stopwatch_elapsed_ns(&sw);
            if (elapsed > target_ns) {
                printf("Warning: frame took %llu ns (target: %llu)\n",
                       elapsed, target_ns);
                reduce_quality();
            }
        }
    }
    
    // Example 15: Statistical analysis
    void example_statistics(void) {
        const int SAMPLES = 1000;
        Stopwatch sw;
        uint64_t times[SAMPLES];
        
        // Collect samples
        for (int i = 0; i < SAMPLES; i++) {
            stopwatch_start(&sw);
            operation();
            times[i] = stopwatch_elapsed_ns(&sw);
        }
        
        // Sort for percentiles
        qsort(times, SAMPLES, sizeof(uint64_t), compare_uint64);
        
        printf("p50: %llu ns\n", times[SAMPLES/2]);
        printf("p95: %llu ns\n", times[(SAMPLES*95)/100]);
        printf("p99: %llu ns\n", times[(SAMPLES*99)/100]);
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_TIME_H */
