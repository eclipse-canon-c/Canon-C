#ifndef CANON_UTIL_TIME_H
#define CANON_UTIL_TIME_H

/* clock_gettime and CLOCK_MONOTONIC require _POSIX_C_SOURCE >= 199309L.
 * Under strict -std=c99, glibc hides these symbols unless the feature-test
 * macro is defined. This must appear before any system header inclusion. */
#ifndef _WIN32
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 199309L
    #elif _POSIX_C_SOURCE < 199309L
        #undef  _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 199309L
    #endif
#endif

#include "core/primitives/types.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

/**
 * @file util/time.h
 * @brief Lightweight, high-resolution monotonic stopwatch
 *
 * Measures time intervals using monotonic clocks — unaffected by system time
 * changes, NTP adjustments, DST, or leap seconds. Provides nanosecond
 * precision where the platform supports it.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Windows:       QueryPerformanceCounter (Win2000+)
 * - POSIX/Linux/macOS: clock_gettime(CLOCK_MONOTONIC)
 * - Monotonic:     never goes backward, unaffected by wall-clock changes
 * - Resolution:    typically 1-100 ns (platform-dependent)
 * - Precision:     nanoseconds reported, actual resolution varies by platform
 *
 * Runtime dependency:
 * ────────────────────────────────────────────────────────────────────────────
 * - Windows:  <windows.h> — QueryPerformanceCounter, QueryPerformanceFrequency
 * - POSIX:    <time.h>    — clock_gettime, CLOCK_MONOTONIC, struct timespec
 * The correct header is selected automatically via _WIN32 preprocessor guard.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Each Stopwatch instance is fully independent
 * - Safe to use different instances from multiple threads simultaneously
 * - NOT safe to share one instance across threads without external locking
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - O(1) per operation
 * - Single syscall or hardware counter read per query
 * - Overhead: ~20-100 ns typical (platform-dependent)
 * - No allocations, no global state
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Monotonic time — safe for intervals, timeouts, and benchmarking
 * - Simple API — start + measure, no pause/resume complexity
 * - Explicit control — caller decides when to start or restart
 * - Null-safe — graceful handling of NULL pointers throughout
 * - Overflow-safe — u64 nanoseconds wraps after ~584 years
 *
 * Monotonic vs wall-clock:
 * ────────────────────────────────────────────────────────────────────────────
 * - Use this (monotonic) for: intervals, performance, timeouts, frame timing
 * - Use wall-clock (time(), gettimeofday()) for: timestamps, logging, scheduling
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Benchmarking / profiling
 * - Frame timing in games
 * - Timeout implementation
 * - Latency measurement
 * - Rate limiting
 * - Progress reporting
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Displaying current date/time (use wall-clock)
 * - Scheduling at specific wall-clock times
 * - Sub-100 ns intervals (platform resolution limits apply)
 * - Cross-system time synchronization
 */

/**
 * @brief Monotonic stopwatch instance (8 bytes)
 *
 * Initialize by calling stopwatch_start(). An unstarted stopwatch
 * (start == 0) returns 0 from all query functions.
 */
typedef struct {
    u64 start;  ///< Monotonic nanoseconds at last start (0 = not started)
} Stopwatch;

/* ────────────────────────────────────────────────────────────────────────────
   Internal: read current monotonic time in nanoseconds
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns current monotonic time in nanoseconds
 *
 * Abstracts platform differences between QueryPerformanceCounter (Windows)
 * and clock_gettime(CLOCK_MONOTONIC) (POSIX).
 *
 * @return Monotonic nanosecond timestamp
 *
 * @remark Internal — use stopwatch_start() and stopwatch_elapsed_ns() instead.
 */
static inline u64 _stopwatch_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (u64)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
#endif
}

/* ────────────────────────────────────────────────────────────────────────────
   Control
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Starts or restarts the stopwatch
 *
 * Records the current monotonic time as the reference point.
 * Calling again on a running stopwatch resets the measurement.
 *
 * @param sw Valid Stopwatch pointer (NULL-safe: no-op)
 */
static inline void stopwatch_start(Stopwatch* sw) {
    if (!sw) return;
    sw->start = _stopwatch_now_ns();
}

/* ────────────────────────────────────────────────────────────────────────────
   Queries
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns elapsed time since last start in nanoseconds
 *
 * @param sw Valid Stopwatch pointer (NULL or not started → 0)
 * @return Nanoseconds elapsed since last stopwatch_start() call
 */
static inline u64 stopwatch_elapsed_ns(const Stopwatch* sw) {
    if (!sw || sw->start == 0) return 0;
    return _stopwatch_now_ns() - sw->start;
}

/**
 * @brief Returns elapsed time since last start in microseconds
 *
 * @param sw Valid Stopwatch pointer (NULL or not started → 0)
 * @return Microseconds elapsed (truncated, not rounded)
 */
static inline u64 stopwatch_elapsed_us(const Stopwatch* sw) {
    return stopwatch_elapsed_ns(sw) / 1000ULL;
}

/**
 * @brief Returns elapsed time since last start in milliseconds
 *
 * @param sw Valid Stopwatch pointer (NULL or not started → 0)
 * @return Milliseconds elapsed (truncated, not rounded)
 */
static inline u64 stopwatch_elapsed_ms(const Stopwatch* sw) {
    return stopwatch_elapsed_ns(sw) / 1000000ULL;
}

/**
 * @brief Returns elapsed time since last start as fractional seconds
 *
 * @param sw Valid Stopwatch pointer (NULL or not started → 0.0)
 * @return Seconds elapsed with sub-second precision
 */
static inline f64 stopwatch_elapsed_sec(const Stopwatch* sw) {
    return (f64)stopwatch_elapsed_ns(sw) / 1e9;
}

#endif /* CANON_UTIL_TIME_H */
