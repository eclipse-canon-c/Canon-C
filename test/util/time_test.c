/**
 * @file time_test.c
 * @brief Unit tests for util/time.h
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   stopwatch_start        — start / restart behaviour
 *   stopwatch_elapsed_ns   — nanosecond query, unstarted
 *   stopwatch_elapsed_us   — microsecond query, unit consistency
 *   stopwatch_elapsed_ms   — millisecond query, unit consistency
 *   stopwatch_elapsed_sec  — fractional seconds query, unit consistency
 *   canon_stopwatch_now_ns_ — monotonicity of underlying clock
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL input to stopwatch_start and stopwatch_elapsed_* is a contract
 *   violation (require_msg → abort). This is not tested here — contract
 *   violations are tested separately via fork/signal in contract_test.c
 *   patterns.
 *
 * Timing strategy
 * ───────────────────────────────────────────────────────────────────────────
 *   CI runners have unpredictable scheduling. Tests use generous lower
 *   bounds (e.g. >= 10 ms for a 50 ms sleep) and avoid tight upper bounds.
 *   The goal is to verify correctness of the API — not benchmark precision.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   A portable sleep helper is provided: sleep_ms() uses Sleep() on
 *   Windows and nanosleep() on POSIX. Loop variables are declared before
 *   the loop body (C99).
 */

#define CANON_CONTRACT_IMPL
#include "util/time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Portability: unused-function suppression
 * ====================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_MAYBE_UNUSED __attribute__((unused))
#else
    #define CANON_MAYBE_UNUSED
#endif

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) static CANON_MAYBE_UNUSED void test_##name(void)
#define RUN(name)  do { test_##name(); } while (0)

#define EXPECT(expr)                                                \
    do {                                                            \
        if (expr) {                                                 \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr, "FAIL  %s:%d  %s\n",                   \
                    __FILE__, __LINE__, #expr);                     \
        }                                                           \
    } while (0)

/* =========================================================================
 * Portable sleep helper
 * ====================================================================== */

static void sleep_ms(u32 ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec  = (time_t)(ms / 1000u);
    req.tv_nsec = (long)((ms % 1000u) * 1000000uL);
    nanosleep(&req, NULL);
#endif
}

/* =========================================================================
 * Unstarted stopwatch — zero-initialised, never started
 * ====================================================================== */

TEST(unstarted_returns_zero) {
    Stopwatch sw;
    memset(&sw, 0, sizeof(sw));

    EXPECT(sw.start == 0);
    EXPECT(stopwatch_elapsed_ns(&sw)  == 0);
    EXPECT(stopwatch_elapsed_us(&sw)  == 0);
    EXPECT(stopwatch_elapsed_ms(&sw)  == 0);
    EXPECT(stopwatch_elapsed_sec(&sw) == 0.0);
}

/* =========================================================================
 * Basic start + elapsed — time passes after start
 * ====================================================================== */

TEST(elapsed_after_start) {
    Stopwatch sw;
    u64 ns;

    stopwatch_start(&sw);
    EXPECT(sw.start != 0);

    sleep_ms(50);

    ns = stopwatch_elapsed_ns(&sw);

    /* 50 ms sleep — generous lower bound of 10 ms to tolerate CI jitter */
    EXPECT(ns >= 10u * 1000000ULL);
}

/* =========================================================================
 * Unit conversion consistency
 * ====================================================================== */

TEST(unit_conversions) {
    Stopwatch sw;
    u64 ns, us, ms;
    f64 sec;

    stopwatch_start(&sw);
    sleep_ms(80);

    ns  = stopwatch_elapsed_ns(&sw);
    us  = stopwatch_elapsed_us(&sw);
    ms  = stopwatch_elapsed_ms(&sw);
    sec = stopwatch_elapsed_sec(&sw);

    /*
     * Queries are sequential, so each reads a slightly later time.
     * Verify all are non-zero and reflect >= 10 ms (generous lower
     * bound for 80 ms sleep).
     */
    EXPECT(ns  > 0);
    EXPECT(us  > 0);
    EXPECT(ms  > 0);
    EXPECT(sec > 0.0);

    EXPECT(ns  >= 10u * 1000000ULL);
    EXPECT(us  >= 10u * 1000ULL);
    EXPECT(ms  >= 10u);
    EXPECT(sec >= 0.01);

    /* Truncation: us == ns/1000 only if read at the same instant.
     * Since they are sequential reads, just verify us <= ns/1000 + small margin.
     * The margin accounts for the time between the two reads (< 1 ms typically). */
    EXPECT(us <= ns / 1000ULL + 1000ULL);
}

/* =========================================================================
 * Restart — calling start again resets the measurement
 * ====================================================================== */

TEST(restart_resets) {
    Stopwatch sw;
    u64 first_elapsed, second_elapsed;

    stopwatch_start(&sw);
    sleep_ms(60);
    first_elapsed = stopwatch_elapsed_ms(&sw);

    /* Restart — elapsed should drop back near zero */
    stopwatch_start(&sw);
    second_elapsed = stopwatch_elapsed_ms(&sw);

    EXPECT(first_elapsed >= 10);        /* original sleep was measured   */
    EXPECT(second_elapsed < first_elapsed); /* restart brought it down   */
    EXPECT(second_elapsed < 50);        /* should be near zero after restart */
}

/* =========================================================================
 * Monotonicity — successive reads never decrease
 * ====================================================================== */

TEST(monotonic_reads) {
    Stopwatch sw;
    u64 prev, curr;
    int i;

    stopwatch_start(&sw);
    prev = stopwatch_elapsed_ns(&sw);

    for (i = 0; i < 100; i++) {
        curr = stopwatch_elapsed_ns(&sw);
        EXPECT(curr >= prev);
        prev = curr;
    }
}

/* =========================================================================
 * Underlying clock monotonicity
 * ====================================================================== */

TEST(now_ns_monotonic) {
    u64 prev, curr;
    int i;

    prev = canon_stopwatch_now_ns_();
    for (i = 0; i < 100; i++) {
        curr = canon_stopwatch_now_ns_();
        EXPECT(curr >= prev);
        prev = curr;
    }
}

/* =========================================================================
 * Struct size — Stopwatch should be exactly 8 bytes (one u64)
 * ====================================================================== */

TEST(struct_size) {
    EXPECT(sizeof(Stopwatch) == sizeof(u64));
    EXPECT(sizeof(Stopwatch) == 8u);
}

/* =========================================================================
 * Multiple independent stopwatches
 * ====================================================================== */

TEST(independent_instances) {
    Stopwatch a, b;
    u64 a_elapsed, b_elapsed;

    stopwatch_start(&a);
    sleep_ms(60);

    stopwatch_start(&b);
    sleep_ms(60);

    a_elapsed = stopwatch_elapsed_ms(&a);
    b_elapsed = stopwatch_elapsed_ms(&b);

    /* a was running ~120 ms, b was running ~60 ms */
    EXPECT(a_elapsed >= 40);   /* generous lower bound for ~120 ms */
    EXPECT(b_elapsed >= 10);   /* generous lower bound for  ~60 ms */
    EXPECT(a_elapsed > b_elapsed);  /* a ran longer than b */
}

/* =========================================================================
 * Entry point
 * ====================================================================== */

int main(void) {
    RUN(unstarted_returns_zero);
    RUN(elapsed_after_start);
    RUN(unit_conversions);
    RUN(restart_resets);
    RUN(monotonic_reads);
    RUN(now_ns_monotonic);
    RUN(struct_size);
    RUN(independent_instances);

    printf("\ntime_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
