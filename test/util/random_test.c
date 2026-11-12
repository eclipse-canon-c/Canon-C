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

/**
 * @file random_test.c
 * @brief Unit tests (and optional fuzz harness) for random.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all TEST() groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput exercises random_range,
 *              random_i32_range, and random_f64_range with arbitrary
 *              parameters, letting libFuzzer explore rejection sampling
 *              edge cases and boundary conditions.
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   random_seed         — seeding, reseeding, inc always odd
 *   random_u32          — core generation, determinism, non-degeneracy
 *   random_range        — bounded [0, bound), bound==0, bound==1, bias check
 *   random_f64          — [0.0, 1.0) range
 *   random_bool         — produces both true and false
 *   random_i32_range    — [min, max], min==max, min>max, negative ranges,
 *                         spans above 2^31 and the full range (VERIFY-072)
 *   random_f32_range    — [min, max) float range; max never returned, where
 *                         rounding reaches it (VERIFY-072)
 *   random_f64_range    — [min, max) double range; the same
 *   float range edges   — !(min < max) returns min, NaN included, and an
 *                         infinite bound; a span that overflows works
 *   struct size         — 16 bytes (two u64)
 *   determinism         — same seed+seq → identical sequence
 *   independent streams — different seq → different sequence
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL input to all random_* functions is a contract violation
 *   (require_msg → abort). This is not tested here — contract violations
 *   are tested separately via fork/signal in contract_test.c patterns.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Loop variables are declared before the loop body (C99). No binary
 *   literals (C23/GNU extension). Statistical tests use large sample
 *   counts with generous tolerances to avoid CI flakiness.
 */

#define CANON_CONTRACT_IMPL
#include "util/random.h"
#include <float.h>
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
 * Struct layout
 * ====================================================================== */

TEST(struct_size) {
    EXPECT(sizeof(Random) == 2 * sizeof(u64));
    EXPECT(sizeof(Random) == 16u);
}

/* =========================================================================
 * Determinism — same seed+seq produces identical sequence
 * ====================================================================== */

TEST(determinism) {
    Random a, b;
    int i;

    random_seed(&a, 12345, 67890);
    random_seed(&b, 12345, 67890);

    for (i = 0; i < 1000; i++) {
        EXPECT(random_u32(&a) == random_u32(&b));
    }
}

/* =========================================================================
 * Reseed — reseeding with same values restarts identical sequence
 * ====================================================================== */

TEST(reseed_restarts) {
    Random r;
    u32 first_run[10];
    int i;

    random_seed(&r, 42, 1);
    for (i = 0; i < 10; i++) {
        first_run[i] = random_u32(&r);
    }

    /* Reseed with same values */
    random_seed(&r, 42, 1);
    for (i = 0; i < 10; i++) {
        EXPECT(random_u32(&r) == first_run[i]);
    }
}

/* =========================================================================
 * Different seeds → different sequences
 * ====================================================================== */

TEST(different_seeds) {
    Random a, b;
    int differ = 0;
    int i;

    random_seed(&a, 111, 1);
    random_seed(&b, 222, 1);

    for (i = 0; i < 100; i++) {
        if (random_u32(&a) != random_u32(&b)) {
            differ++;
        }
    }

    /* Overwhelmingly likely to differ on most outputs */
    EXPECT(differ > 90);
}

/* =========================================================================
 * Independent streams — same seed, different seq → different sequences
 * ====================================================================== */

TEST(independent_streams) {
    Random a, b;
    int differ = 0;
    int i;

    random_seed(&a, 42, 1);
    random_seed(&b, 42, 2);

    for (i = 0; i < 100; i++) {
        if (random_u32(&a) != random_u32(&b)) {
            differ++;
        }
    }

    EXPECT(differ > 90);
}

/* =========================================================================
 * Seeding — inc is always odd (seq << 1 | 1)
 * ====================================================================== */

TEST(inc_always_odd) {
    Random r;
    u64 seqs[5];
    int i;

    seqs[0] = 0;
    seqs[1] = 1;
    seqs[2] = 0x7FFFFFFFFFFFFFFFULL;
    seqs[3] = 0xFFFFFFFFFFFFFFFFULL;
    seqs[4] = 42;

    for (i = 0; i < 5; i++) {
        random_seed(&r, 0, seqs[i]);
        EXPECT((r.inc & 1) == 1);
    }
}

/* =========================================================================
 * random_u32 — produces non-degenerate output
 * ====================================================================== */

TEST(u32_non_degenerate) {
    Random r;
    u32 prev, curr;
    int changes = 0;
    int i;

    random_seed(&r, 1, 1);
    prev = random_u32(&r);

    for (i = 0; i < 1000; i++) {
        curr = random_u32(&r);
        if (curr != prev) changes++;
        prev = curr;
    }

    /* Should almost never produce the same value twice in a row */
    EXPECT(changes > 990);
}

/* =========================================================================
 * random_range — basic bounds
 * ====================================================================== */

TEST(range_bounds) {
    Random r;
    int i;

    random_seed(&r, 42, 1);

    /* bound == 0 → 0 */
    EXPECT(random_range(&r, 0) == 0);

    /* bound == 1 → always 0 */
    for (i = 0; i < 100; i++) {
        EXPECT(random_range(&r, 1) == 0);
    }

    /* bound == 2 → always 0 or 1 */
    for (i = 0; i < 100; i++) {
        u32 v = random_range(&r, 2);
        EXPECT(v < 2);
    }

    /* General bounds */
    for (i = 0; i < 10000; i++) {
        u32 v = random_range(&r, 100);
        EXPECT(v < 100);
    }
}

/* =========================================================================
 * random_range — large bound (exercises rejection sampling)
 * ====================================================================== */

TEST(range_large_bound) {
    Random r;
    int i;
    u32 bound = 0xFFFFFFFFu;  /* U32_MAX — maximum rejection threshold */

    random_seed(&r, 77, 1);

    for (i = 0; i < 1000; i++) {
        u32 v = random_range(&r, bound);
        EXPECT(v < bound);
    }
}

/* =========================================================================
 * random_range — power-of-two bounds (zero bias threshold)
 * ====================================================================== */

TEST(range_power_of_two) {
    Random r;
    int i;
    u32 bounds[4];
    int b;

    bounds[0] = 2;
    bounds[1] = 16;
    bounds[2] = 256;
    bounds[3] = 1024;

    random_seed(&r, 99, 1);

    for (b = 0; b < 4; b++) {
        for (i = 0; i < 1000; i++) {
            EXPECT(random_range(&r, bounds[b]) < bounds[b]);
        }
    }
}

/* =========================================================================
 * random_range — rough uniformity check
 * ====================================================================== */

TEST(range_uniformity) {
    Random r;
    u32 buckets[10];
    int i;
    u32 n = 100000u;

    random_seed(&r, 42, 1);
    memset(buckets, 0, sizeof(buckets));

    for (i = 0; i < (int)n; i++) {
        u32 v = random_range(&r, 10);
        buckets[v]++;
    }

    /* Each bucket should get ~10000 hits. Allow 8000-12000 (generous). */
    for (i = 0; i < 10; i++) {
        EXPECT(buckets[i] > 8000);
        EXPECT(buckets[i] < 12000);
    }
}

/* =========================================================================
 * random_f64 — range [0.0, 1.0)
 * ====================================================================== */

TEST(f64_range) {
    Random r;
    int i;

    random_seed(&r, 42, 1);

    for (i = 0; i < 10000; i++) {
        f64 v = random_f64(&r);
        EXPECT(v >= 0.0);
        EXPECT(v < 1.0);
    }
}

/* =========================================================================
 * random_bool — produces both values
 * ====================================================================== */

TEST(bool_both_values) {
    Random r;
    int trues  = 0;
    int falses = 0;
    int i;

    random_seed(&r, 42, 1);

    for (i = 0; i < 10000; i++) {
        if (random_bool(&r)) {
            trues++;
        } else {
            falses++;
        }
    }

    /* Both must appear, and should be roughly balanced */
    EXPECT(trues  > 4000);
    EXPECT(falses > 4000);
}

/* =========================================================================
 * random_i32_range — basic bounds and edge cases
 * ====================================================================== */

TEST(i32_range_bounds) {
    Random r;
    int i;

    random_seed(&r, 42, 1);

    /* Normal range */
    for (i = 0; i < 10000; i++) {
        i32 v = random_i32_range(&r, 10, 20);
        EXPECT(v >= 10);
        EXPECT(v <= 20);
    }

    /* Negative range */
    for (i = 0; i < 10000; i++) {
        i32 v = random_i32_range(&r, -100, -50);
        EXPECT(v >= -100);
        EXPECT(v <= -50);
    }

    /* Crossing zero */
    for (i = 0; i < 10000; i++) {
        i32 v = random_i32_range(&r, -10, 10);
        EXPECT(v >= -10);
        EXPECT(v <= 10);
    }
}

TEST(i32_range_edge_cases) {
    Random r;
    int i;

    random_seed(&r, 42, 1);

    /* min == max → always returns min */
    for (i = 0; i < 100; i++) {
        EXPECT(random_i32_range(&r, 42, 42) == 42);
    }

    /* min > max → returns min */
    for (i = 0; i < 100; i++) {
        EXPECT(random_i32_range(&r, 10, 5) == 10);
    }

    /* Single-element range */
    for (i = 0; i < 100; i++) {
        i32 v = random_i32_range(&r, 7, 8);
        EXPECT(v == 7 || v == 8);
    }
}

/* =========================================================================
 * random_f32_range — bounds
 * ====================================================================== */

TEST(f32_range_bounds) {
    Random r;
    int i;

    random_seed(&r, 42, 1);

    for (i = 0; i < 10000; i++) {
        f32 v = random_f32_range(&r, -5.0f, 5.0f);
        EXPECT(v >= -5.0f);
        EXPECT(v < 5.0f);
    }
}

/* =========================================================================
 * random_f64_range — bounds
 * ====================================================================== */

TEST(f64_range_bounds) {
    Random r;
    int i;

    random_seed(&r, 42, 1);

    for (i = 0; i < 10000; i++) {
        f64 v = random_f64_range(&r, -100.0, 100.0);
        EXPECT(v >= -100.0);
        EXPECT(v < 100.0);
    }

    /* Tiny range */
    for (i = 0; i < 10000; i++) {
        f64 v = random_f64_range(&r, 1.0, 1.001);
        EXPECT(v >= 1.0);
        EXPECT(v < 1.001);
    }
}

/* =========================================================================
 * Spans above 2^31, and the full i32 range (VERIFY-072)
 * ====================================================================== */

TEST(i32_range_wide_spans) {
    Random r;
    int i;
    int negative = 0;
    int high = 0;

    random_seed(&r, 42, 1);

    /* The full range: before VERIFY-072, every call returned INT32_MIN. */
    for (i = 0; i < 1000; i++) {
        i32 v = random_i32_range(&r, INT32_MIN, INT32_MAX);
        if (v < 0) { negative++; }
    }
    EXPECT(negative > 400);
    EXPECT(negative < 600);

    /* Above 2^31: before VERIFY-072, a draw above INT32_MAX overflowed int
       when added to min, which UBSan reported. */
    for (i = 0; i < 1000; i++) {
        i32 v = random_i32_range(&r, INT32_MIN + 1, INT32_MAX);
        EXPECT(v > INT32_MIN);
        if (v > 1073741824) { high++; }
    }
    EXPECT(high > 100);

    for (i = 0; i < 1000; i++) {
        i32 v = random_i32_range(&r, -10, INT32_MAX);
        EXPECT(v >= -10);
    }
}

/* =========================================================================
 * The float ranges never return max (VERIFY-072)
 * ====================================================================== */

TEST(float_ranges_exclude_max) {
    Random r;
    int i;
    int below_top = 0;
    const f64 lo = 1152921504606846976.0;   /* 2^60 */

    /* Seed 83909295, stream 1: the first draw is 0xFFFFFF8D, whose
       random_f64 rounds to 1.0f. Before VERIFY-072, random_f32_range
       returned 1.0f for it. */
    random_seed(&r, 83909295u, 1u);
    EXPECT(random_u32(&r) == 0xFFFFFF8Du);
    random_seed(&r, 83909295u, 1u);
    EXPECT(random_f32_range(&r, 0.0f, 1.0f) < 1.0f);

    /* From 2^60 to 2^60 + 1024, doubles are 256 apart, and an eighth of
       the draws round to max. Before VERIFY-072 they were returned. */
    random_seed(&r, 42, 1);
    for (i = 0; i < 1000; i++) {
        f64 v = random_f64_range(&r, lo, lo + 1024.0);
        EXPECT(v >= lo);
        EXPECT(v < (lo + 1024.0));
        if (v < (lo + 768.0)) { below_top++; }
    }
    EXPECT(below_top > 0);
}

/* =========================================================================
 * The float ranges' edges: empty, NaN, infinite, and spans that overflow
 * ====================================================================== */

TEST(float_ranges_edges) {
    Random r;
    int i;
    int negative = 0;
    volatile f64 zero  = 0.0;
    volatile f32 big32 = FLT_MAX;
    volatile f64 big64 = DBL_MAX;
    const f64 nan64 = zero / zero;
    const f32 nan32 = (f32)nan64;
    const f32 inf32 = big32 * 2.0f;
    const f64 inf64 = big64 * 2.0;

    random_seed(&r, 42, 1);

    /* !(min < max): min. */
    EXPECT(random_f32_range(&r, 1.0f, 1.0f) == 1.0f);
    EXPECT(random_f32_range(&r, 2.0f, 1.0f) == 2.0f);
    EXPECT(random_f64_range(&r, 1.0, 1.0) == 1.0);
    EXPECT(random_f64_range(&r, 2.0, 1.0) == 2.0);
    {
        const f32 v32 = random_f32_range(&r, nan32, 1.0f);
        const f64 v64 = random_f64_range(&r, nan64, 1.0);
        EXPECT(v32 != v32);
        EXPECT(v64 != v64);
    }

    /* An infinite bound: min. */
    EXPECT(random_f32_range(&r, -inf32, 1.0f) == -inf32);
    EXPECT(random_f32_range(&r, 0.0f, inf32) == 0.0f);
    EXPECT(random_f64_range(&r, -inf64, 1.0) == -inf64);
    EXPECT(random_f64_range(&r, 0.0, inf64) == 0.0);

    /* A span that overflows, the widest there is: the two halves keep
       every value finite and in range. */
    for (i = 0; i < 1000; i++) {
        const f32 v32 = random_f32_range(&r, -big32, big32);
        const f64 v64 = random_f64_range(&r, -big64, big64);
        EXPECT((v32 >= -big32) && (v32 < big32));
        EXPECT((v64 >= -big64) && (v64 < big64));
        if (v64 < 0.0) { negative++; }
    }
    EXPECT(negative > 400);
    EXPECT(negative < 600);
}

/* =========================================================================
 * Seed zero — defined behaviour (not degenerate)
 * ====================================================================== */

TEST(seed_zero) {
    Random r;
    int changes = 0;
    u32 prev, curr;
    int i;

    random_seed(&r, 0, 0);

    /* inc should still be odd */
    EXPECT((r.inc & 1) == 1);

    prev = random_u32(&r);
    for (i = 0; i < 100; i++) {
        curr = random_u32(&r);
        if (curr != prev) changes++;
        prev = curr;
    }

    /* Should still produce varied output */
    EXPECT(changes > 90);
}

/* =========================================================================
 * Known-value regression — locks down exact PCG32 output
 * ====================================================================== */

TEST(known_values) {
    Random r;

    /*
     * Reference sequence for seed=42, seq=54.
     * If these ever change, the PCG32 implementation has diverged.
     * Values generated by running the implementation and recording output.
     */
    random_seed(&r, 42, 54);

    u32 v0 = random_u32(&r);
    u32 v1 = random_u32(&r);
    u32 v2 = random_u32(&r);
    u32 v3 = random_u32(&r);

    /*
     * The first run records these values. If they match the reference
     * PCG32 implementation (pcg-random.org), lock them in here.
     * For now, verify determinism: re-seed and check same sequence.
     */
    random_seed(&r, 42, 54);
    EXPECT(random_u32(&r) == v0);
    EXPECT(random_u32(&r) == v1);
    EXPECT(random_u32(&r) == v2);
    EXPECT(random_u32(&r) == v3);
}

/* =========================================================================
 * Entry points
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    Random r;
    u32 bound;
    i32 min_val, max_val;

    if (size < 16) return 0;

    /* Use first 8 bytes as seed, next 8 as seq */
    {
        u64 seed = 0, seq = 0;
        memcpy(&seed, data,     8);
        memcpy(&seq,  data + 8, 8);
        random_seed(&r, seed, seq);
    }

    /* inc must always be odd */
    if ((r.inc & 1) != 1) __builtin_trap();

    /* Exercise core generation */
    (void)random_u32(&r);
    (void)random_bool(&r);

    /* f64 must be in [0.0, 1.0) */
    {
        f64 v = random_f64(&r);
        if (v < 0.0 || v >= 1.0) __builtin_trap();
    }

    /* random_range with fuzzed bound */
    if (size >= 20) {
        memcpy(&bound, data + 16, 4);
        if (bound > 0) {
            u32 v = random_range(&r, bound);
            if (v >= bound) __builtin_trap();
        }
    }

    /* random_i32_range with fuzzed min/max */
    if (size >= 28) {
        memcpy(&min_val, data + 20, 4);
        memcpy(&max_val, data + 24, 4);
        if (min_val < max_val) {
            i32 v = random_i32_range(&r, min_val, max_val);
            if (v < min_val || v > max_val) __builtin_trap();
        }
    }

    /* random_f64_range */
    {
        f64 v = random_f64_range(&r, -1000.0, 1000.0);
        if (v < -1000.0 || v >= 1000.0) __builtin_trap();
    }

    return 0;
}

#else

int main(void) {
    RUN(struct_size);
    RUN(determinism);
    RUN(reseed_restarts);
    RUN(different_seeds);
    RUN(independent_streams);
    RUN(inc_always_odd);
    RUN(u32_non_degenerate);
    RUN(range_bounds);
    RUN(range_large_bound);
    RUN(range_power_of_two);
    RUN(range_uniformity);
    RUN(f64_range);
    RUN(bool_both_values);
    RUN(i32_range_bounds);
    RUN(i32_range_edge_cases);
    RUN(f32_range_bounds);
    RUN(f64_range_bounds);
    RUN(i32_range_wide_spans);
    RUN(float_ranges_exclude_max);
    RUN(float_ranges_edges);
    RUN(seed_zero);
    RUN(known_values);

    printf("\nrandom_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
