/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_UTIL_TIME_H
#define CANON_UTIL_TIME_H

/* clock_gettime and CLOCK_MONOTONIC require _POSIX_C_SOURCE >= 199309L.
 * Under strict -std=c99, glibc hides these symbols unless the feature-test
 * macro is defined. This must appear before any system header inclusion. */
#ifndef _WIN32
    #ifndef _POSIX_C_SOURCE
        /* cppcheck-suppress misra-c2012-21.1 ; MISRA-DEV-011 */
        #define _POSIX_C_SOURCE 199309L
    #elif _POSIX_C_SOURCE < 199309L
        /* cppcheck-suppress misra-c2012-21.1 ; MISRA-DEV-011 */
        #undef  _POSIX_C_SOURCE
        /* cppcheck-suppress misra-c2012-21.1 ; MISRA-DEV-011 */
        #define _POSIX_C_SOURCE 199309L
    #endif
#endif

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"

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
 * - Contracts — NULL pointer is a programming error (require_msg)
 * - Overflow-safe — u64 nanoseconds wraps after ~584 years
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * - sw (Stopwatch pointer) must not be NULL — violated → contract abort
 * - An unstarted stopwatch (start == 0) returns 0 from all query functions
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
static inline u64 stopwatch_now_ns_(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (u64)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((u64)ts.tv_sec * 1000000000ULL) + (u64)ts.tv_nsec;
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
 * @param sw Valid Stopwatch pointer (must not be NULL — contract)
 */
static inline void stopwatch_start(borrowed(Stopwatch*) sw) {
    require_msg(sw != NULL, "stopwatch_start: sw is NULL");
    sw->start = stopwatch_now_ns_();
}

/* ────────────────────────────────────────────────────────────────────────────
   Queries
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns elapsed time since last start in nanoseconds
 *
 * @param sw Valid Stopwatch pointer (must not be NULL — contract)
 * @return Nanoseconds elapsed since last stopwatch_start() call,
 *         or 0 if stopwatch has not been started (start == 0)
 */
static inline u64 stopwatch_elapsed_ns(borrowed(const Stopwatch*) sw) {
    require_msg(sw != NULL, "stopwatch_elapsed_ns: sw is NULL");
    if (sw->start == 0u) { return 0u; }
    return stopwatch_now_ns_() - sw->start;
}

/**
 * @brief Returns elapsed time since last start in microseconds
 *
 * @param sw Valid Stopwatch pointer (must not be NULL — contract)
 * @return Microseconds elapsed (truncated, not rounded)
 */
static inline u64 stopwatch_elapsed_us(borrowed(const Stopwatch*) sw) {
    return stopwatch_elapsed_ns(sw) / 1000ULL;
}

/**
 * @brief Returns elapsed time since last start in milliseconds
 *
 * @param sw Valid Stopwatch pointer (must not be NULL — contract)
 * @return Milliseconds elapsed (truncated, not rounded)
 */
static inline u64 stopwatch_elapsed_ms(borrowed(const Stopwatch*) sw) {
    return stopwatch_elapsed_ns(sw) / 1000000ULL;
}

/**
 * @brief Returns elapsed time since last start as fractional seconds
 *
 * @param sw Valid Stopwatch pointer (must not be NULL — contract)
 * @return Seconds elapsed with sub-second precision
 */
static inline f64 stopwatch_elapsed_sec(borrowed(const Stopwatch*) sw) {
    return (f64)stopwatch_elapsed_ns(sw) / 1e9;
}

#endif /* CANON_UTIL_TIME_H */
