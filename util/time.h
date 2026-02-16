#ifndef CANON_UTIL_TIME_H
#define CANON_UTIL_TIME_H

#include "core/primitives/types.h"   // u64, usize, bool

/**
 * @file util/time.h
 * @brief Lightweight, high-resolution monotonic stopwatch
 *
 * Measures time intervals using monotonic clocks (unaffected by system time
 * changes, NTP, DST, sleep). Provides nanosecond precision where supported.
 *
 * Portability:
 * - Windows: QueryPerformanceCounter (Win2000+)
 * - POSIX/Linux/macOS: clock_gettime(CLOCK_MONOTONIC)
 * - Monotonic: never goes backward, continues during sleep (most platforms)
 * - Resolution: typically 1–100 ns (platform-dependent)
 * - Precision: nanoseconds reported, but actual resolution varies
 *
 * Thread-safety:
 * - Each Stopwatch instance is independent
 * - Safe to use different instances from multiple threads
 * - NOT safe to share one instance across threads without locking
 *
 * Performance:
 * - O(1) per operation
 * - Single syscall or CPU instruction per query
 * - Overhead: ~20–100 ns typical
 * - No allocations, no global state
 *
 * Core ideas:
 * - Monotonic time — safe for intervals, timeouts, benchmarking
 * - Simple API — start + measure, no pause/resume
 * - Explicit control — caller decides when to start/restart
 * - Null-safe — graceful handling of NULL pointers
 * - Overflow-safe — 584+ years before wraparound
 *
 * Monotonic vs wall-clock:
 * - Use this for intervals, performance, timeouts
 * - Use wall-clock (time(), gettimeofday()) for timestamps/logging
 *
 * Typical use cases:
 * - Benchmarking / profiling
 * - Frame timing (games)
 * - Timeout implementation
 * - Latency measurement
 * - Rate limiting
 * - Progress reporting
 *
 * NOT suitable for:
 * - Displaying current date/time
 * - Scheduling at specific wall-clock times
 * - Sub-100 ns intervals (platform limits)
 * - Cross-system synchronization
 */
/**
 * @brief Stopwatch instance (8 bytes)
 */
typedef struct {
    u64 start;  // Monotonic nanoseconds at start (0 = not started)
} Stopwatch;

/* ────────────────────────────────────────────────────────────────────────────
   Control
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Starts (or restarts) the stopwatch
 *
 * Records current monotonic time. Calling again resets measurement.
 *
 * @param sw Valid Stopwatch pointer (NULL-safe: no-op)
 */
static inline void stopwatch_start(Stopwatch* sw) {
    if (!sw) return;

#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    sw->start = (u64)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    sw->start = (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
#endif
}

/* ────────────────────────────────────────────────────────────────────────────
   Queries
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Elapsed time since last start (nanoseconds)
 *
 * @param sw Valid Stopwatch pointer (NULL or not started → 0)
 * @return Nanoseconds elapsed (always ≥ 0)
 */
static inline u64 stopwatch_elapsed_ns(const Stopwatch* sw) {
    if (!sw || sw->start == 0) return 0;

#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&freq);
    u64 now = (u64)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 now = (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
#endif

    return now - sw->start;
}

/**
 * @brief Elapsed time in seconds (floating-point)
 *
 * @param sw Valid Stopwatch pointer
 * @return Seconds elapsed (fractional part = sub-second precision)
 */
static inline double stopwatch_elapsed_sec(const Stopwatch* sw) {
    return (double)stopwatch_elapsed_ns(sw) / 1e9;
}

#endif /* CANON_UTIL_TIME_H */
