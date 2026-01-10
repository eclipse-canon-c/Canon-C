// util/time.h
#ifndef CANON_UTIL_TIME_H
#define CANON_UTIL_TIME_H

#include <stdint.h>

/**
 * @file time.h
 * @brief Simple high-resolution monotonic stopwatch
 *
 * Provides a lightweight, platform-agnostic stopwatch for measuring time intervals.
 * Uses monotonic clocks to ensure reliable measurements (not affected by system time changes).
 *
 * Key properties:
 *   - No global state — each Stopwatch instance is independent
 *   - Explicit start/stop/reset control
 *   - Nanosecond precision where supported
 *   - Platform-specific implementations (Windows QueryPerformanceCounter, POSIX clock_gettime)
 *   - Safe: null pointers and unstarted watches return 0
 *
 * Usage pattern:
 *   Stopwatch sw;
 *   stopwatch_start(&sw);
 *   // ... do work ...
 *   uint64_t ns = stopwatch_elapsed_ns(&sw);
 *   double seconds = stopwatch_elapsed_sec(&sw);
 *
 * Notes on portability:
 *   - Windows: Uses QueryPerformanceCounter (high-resolution, monotonic)
 *   - POSIX/Unix: Uses CLOCK_MONOTONIC (guaranteed monotonic, nanosecond precision)
 *   - Other platforms: Currently unsupported — compilation will fail
 *   - Always include <time.h> / <windows.h> in your project when using this header
 *
 * Limitations:
 *   - Does not support pausing/resuming (only full reset via new start)
 *   - No overflow protection (uint64_t wraps after ~584 years on nanosecond scale)
 *   - Resolution depends on OS/hardware (usually 1–100 ns)
 */

/**
 * @brief Stopwatch instance (holds only the start timestamp)
 *
 * Simple struct with single uint64_t — zero-cost when not used.
 * Start time is in nanoseconds (monotonic clock).
 */
typedef struct {
    uint64_t start;  ///< Monotonic timestamp when stopwatch was started
} Stopwatch;

/**
 * @brief Starts (or restarts) the stopwatch by recording current time
 *
 * @param sw Valid Stopwatch pointer (if NULL, does nothing)
 *
 * Platform notes:
 *   - Windows: QueryPerformanceCounter + frequency scaling to ns
 *   - POSIX: clock_gettime(CLOCK_MONOTONIC)
 */
static inline void stopwatch_start(Stopwatch* sw) {
    if (!sw) return;

#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    sw->start = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    sw->start = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

/**
 * @brief Returns elapsed time since start in nanoseconds
 *
 * @param sw Valid Stopwatch (if NULL or not started, returns 0)
 * @return   Elapsed nanoseconds (monotonic)
 */
static inline uint64_t stopwatch_elapsed_ns(const Stopwatch* sw) {
    if (!sw) return 0;

#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&freq);
    uint64_t now = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif

    return now - sw->start;
}

/**
 * @brief Returns elapsed time since start in seconds (floating-point)
 *
 * @param sw Valid Stopwatch
 * @return   Elapsed seconds as double (precision depends on platform)
 */
static inline double stopwatch_elapsed_sec(const Stopwatch* sw) {
    return (double)stopwatch_elapsed_ns(sw) / 1e9;
}

#endif /* CANON_UTIL_TIME_H */
