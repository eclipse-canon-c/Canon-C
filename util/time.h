// util/time.h
#ifndef CANON_UTIL_TIME_H
#define CANON_UTIL_TIME_H

#include <stdint.h>

/**
 * @file time.h
 * @brief Lightweight, high-resolution monotonic stopwatch
 *
 * Provides a simple, portable, and explicit stopwatch for measuring time intervals  
 * using monotonic clocks (not affected by system time adjustments, NTP, sleep, etc.).
 *
 * Key properties:
 * ────────────────────────────────────────────────────────────────────────────
 * - **No global state** — each Stopwatch instance is fully independent
 * - **Explicit control** — start, measure, restart (no pause/resume)
 * - **Nanosecond precision** where supported by OS/hardware
 * - **Monotonic** — reliable even during system clock changes
 * - **Null-safe** — safe to pass NULL pointers (returns 0)
 * - **Very lightweight** — single uint64_t per instance
 * - **Cross-platform** — Windows (QueryPerformanceCounter), POSIX (CLOCK_MONOTONIC)
 *
 * Limitations & notes:
 * - No pause/resume support (use new start + subtract previous elapsed for manual pause)
 * - uint64_t overflow after ~584 years at nanosecond resolution (practically irrelevant)
 * - Actual resolution varies (typically 1–100 ns on modern systems)
 * - Requires proper inclusion of platform headers:
 *   - Windows: <windows.h>
 *   - POSIX/Unix: <time.h>
 * - Compilation fails on unsupported platforms (intentional — add support when needed)
 *
 * Typical usage patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * Basic timing:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * // ... expensive operation ...
 * uint64_t ns = stopwatch_elapsed_ns(&sw);
 * double sec = stopwatch_elapsed_sec(&sw);
 * ```
 *
 * Multiple measurements / restart:
 * ```c
 * Stopwatch sw;
 * stopwatch_start(&sw);
 * // phase 1
 * uint64_t phase1_ns = stopwatch_elapsed_ns(&sw);
 * stopwatch_start(&sw);  // restart for phase 2
 * // phase 2
 * uint64_t phase2_ns = stopwatch_elapsed_ns(&sw);
 * ```
 */

/**
 * @brief Stopwatch instance (minimal 128-bit state)
 *
 * Contains only the start timestamp in nanoseconds (monotonic).
 * Zero-initialized state means "not started" (elapsed = 0).
 */
typedef struct {
    uint64_t start;  ///< Monotonic nanoseconds when started
} Stopwatch;

/* ────────────────────────────────────────────────────────────────────────────
   Stopwatch control
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Starts (or restarts) the stopwatch
 *
 * @param sw Valid Stopwatch pointer (NULL-safe: does nothing)
 *
 * Records current monotonic time as start point.
 * Calling multiple times effectively resets the stopwatch.
 *
 * Platform implementations:
 * - Windows: QueryPerformanceCounter + frequency → nanoseconds
 * - POSIX: clock_gettime(CLOCK_MONOTONIC) → nanoseconds
 */
static inline void stopwatch_start(Stopwatch* sw) {
    if (!sw) {
        return;
    }

#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    sw->start = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else  // POSIX / Unix-like
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    sw->start = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* ────────────────────────────────────────────────────────────────────────────
   Elapsed time queries
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns elapsed time since start in nanoseconds
 *
 * @param sw Valid Stopwatch (NULL or not started → returns 0)
 * @return Elapsed nanoseconds (monotonic, uint64_t)
 *
 * Safe to call multiple times — measures from the last start call.
 */
static inline uint64_t stopwatch_elapsed_ns(const Stopwatch* sw) {
    if (!sw || sw->start == 0) {
        return 0;
    }

#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&freq);
    uint64_t now = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif

    return now - sw->start;
}

/**
 * @brief Returns elapsed time since start in seconds (floating-point)
 *
 * @param sw Valid Stopwatch
 * @return Elapsed time as double (seconds, fractional part)
 *
 * Convenience wrapper — divides nanoseconds by 1e9.
 * Precision depends on platform timer resolution.
 */
static inline double stopwatch_elapsed_sec(const Stopwatch* sw) {
    return (double)stopwatch_elapsed_ns(sw) / 1e9;
}

#endif /* CANON_UTIL_TIME_H */
